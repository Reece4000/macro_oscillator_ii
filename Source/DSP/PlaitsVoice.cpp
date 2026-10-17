#include "DSP/PlaitsVoice.h"

#include "plaits/dsp/dsp.h"
#include "plaits/dsp/voice.h"
#include "stmlib/utils/buffer_allocator.h"

#include <cmath>
#include <cstdlib>
#include <new>
#include <utility>

#include <juce_core/juce_core.h>

namespace macro_osc_ii
{
namespace
{
constexpr float kVoiceSilenceThreshold = 1.0e-5f;
constexpr double kCustomParameterSmoothTimeSeconds = 0.01;
constexpr float kMinimumSmoothedEnvelopeSeconds = 0.001f;
// Each voice sits about 9 dB below full scale after its effects, so a full
// chord stays clear of the output limiter instead of driving it constantly.
constexpr float kVoiceHeadroom = 0.355f;
constexpr int kLastModel = kModelCount - 1;
static_assert (kModelCount == plaits::kMaxEngines);

static_assert (PlaitsEngine::kBlockSize == static_cast<int> (plaits::kBlockSize));

// Four-point, third-order Hermite: a smooth curve from x0 to x1 at t in 0..1.
[[nodiscard]] float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b = w + a;
    return (((a * t) - b) * t + c) * t + x0;
}
} // namespace

PlaitsEngine::PlaitsEngine()
{
    // Zeroed like the module's RAM at power-up: a few engine members are never
    // set by Init() and rely on it.
    auto* memory = std::calloc (1, sizeof (plaits::Voice));
    if (memory == nullptr)
        throw std::bad_alloc();
    voice.reset (new (memory) plaits::Voice());
    reset();
}

PlaitsEngine::~PlaitsEngine() = default;

void PlaitsEngine::VoiceDeleter::operator() (plaits::Voice* voiceToDelete) const noexcept
{
    voiceToDelete->~Voice();
    std::free (voiceToDelete);
}

void PlaitsEngine::reset()
{
    // Every engine shares the same working memory, as on the module.
    stmlib::BufferAllocator allocator (ram.data(), ram.size());
    voice->Init (&allocator);
}

void PlaitsEngine::render (const Controls& controls, float* out, float* aux)
{
    plaits::Patch patch {};
    patch.note = controls.note;
    patch.harmonics = juce::jlimit (0.0f, 1.0f, controls.harmonics);
    patch.timbre = juce::jlimit (0.0f, 1.0f, controls.timbre);
    patch.morph = juce::jlimit (0.0f, 1.0f, controls.morph);
    patch.engine = juce::jlimit (0, plaits::kMaxEngines - 1, controls.engine);
    patch.decay = 0.5f;
    // A fully open, full-colour gate passes the engine output unchanged.
    patch.lpg_colour = 1.0f;

    plaits::Modulations modulations {};
    // Plaits averages modulations.note with its previous block, so pitch goes
    // through patch.note: a fresh voice would otherwise start an octave low.
    modulations.trigger = controls.gate ? 1.0f : 0.0f;
    modulations.level = 1.0f;
    modulations.trigger_patched = true;
    modulations.level_patched = true;

    std::array<plaits::Voice::Frame, plaits::kBlockSize> frames {};
    voice->Render (patch, modulations, frames.data(), frames.size());
    for (size_t i = 0; i < frames.size(); ++i)
    {
        out[i] = static_cast<float> (frames[i].out) / 32768.0f;
        aux[i] = static_cast<float> (frames[i].aux) / 32768.0f;
    }
}

double PlaitsEngine::nativeSampleRate() noexcept
{
    return static_cast<double> (plaits::kCorrectedSampleRate);
}

PlaitsVoice::PlaitsVoice()
{
    msegRandom.setSeedRandomly();
    resetParameterSmoothers();
}

void PlaitsVoice::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    nativeStep = PlaitsEngine::nativeSampleRate() / sampleRate;
    hostSamplesPerBlock = juce::jmax (1, juce::roundToInt (kRenderBlockSize / nativeStep));
    resetParameterSmoothers();
    effects.prepare (sampleRate);
    engine.reset();
    resetResampler();
}

void PlaitsVoice::setParameters (const PlaitsVoiceParameters& newParameters)
{
    parameters = newParameters;
    if (isVoiceActive())
        setParameterSmootherTargets();
}

void PlaitsVoice::resetParameterSmoothers()
{
    const double controlRate = PlaitsEngine::nativeSampleRate();
    auto reset = [] (LinearSmoother& smoother, double rate, float value)
    {
        smoother.reset (rate, kCustomParameterSmoothTimeSeconds);
        smoother.setCurrentAndTargetValue (value);
    };

    reset (parameterSmoothers.auxMix, controlRate, parameters.auxMix);
    reset (parameterSmoothers.level, sampleRate, parameters.level);
    reset (parameterSmoothers.coarseSemitones, controlRate, parameters.coarseSemitones);
    reset (parameterSmoothers.fineSemitones, controlRate, parameters.fineSemitones);
    reset (parameterSmoothers.portamentoSeconds, controlRate, parameters.portamentoSeconds);
    reset (parameterSmoothers.attackSeconds, controlRate, parameters.attackSeconds);
    reset (parameterSmoothers.decaySeconds, controlRate, parameters.decaySeconds);
    reset (parameterSmoothers.sustainLevel, controlRate, parameters.sustainLevel);
    reset (parameterSmoothers.releaseSeconds, controlRate, parameters.releaseSeconds);

    smoothedParameters.auxMix = parameters.auxMix;
    smoothedParameters.level = parameters.level;
    smoothedParameters.coarseSemitones = parameters.coarseSemitones;
    smoothedParameters.fineSemitones = parameters.fineSemitones;
    smoothedParameters.portamentoSeconds = parameters.portamentoSeconds;
    smoothedParameters.attackSeconds = parameters.attackSeconds;
    smoothedParameters.decaySeconds = parameters.decaySeconds;
    smoothedParameters.sustainLevel = parameters.sustainLevel;
    smoothedParameters.releaseSeconds = parameters.releaseSeconds;

    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        const auto& slot = parameters.msegSlots[static_cast<size_t> (i)];
        auto& smoothers = parameterSmoothers.msegSlots[static_cast<size_t> (i)];
        auto& smoothed = smoothedParameters.msegSlots[static_cast<size_t> (i)];

        reset (smoothers.amountPercent, controlRate, slot.amountPercent);
        reset (smoothers.offsetPercent, controlRate, slot.offsetPercent);
        reset (smoothers.rateSeconds, controlRate, slot.rateSeconds);

        smoothed.amountPercent = slot.amountPercent;
        smoothed.offsetPercent = slot.offsetPercent;
        smoothed.rateSeconds = slot.rateSeconds;
    }
}

void PlaitsVoice::setParameterSmootherTargets() noexcept
{
    parameterSmoothers.auxMix.setTargetValue (parameters.auxMix);
    parameterSmoothers.level.setTargetValue (parameters.level);
    parameterSmoothers.coarseSemitones.setTargetValue (parameters.coarseSemitones);
    parameterSmoothers.fineSemitones.setTargetValue (parameters.fineSemitones);
    parameterSmoothers.portamentoSeconds.setTargetValue (parameters.portamentoSeconds);
    parameterSmoothers.attackSeconds.setTargetValue (parameters.attackSeconds);
    parameterSmoothers.decaySeconds.setTargetValue (parameters.decaySeconds);
    parameterSmoothers.sustainLevel.setTargetValue (parameters.sustainLevel);
    parameterSmoothers.releaseSeconds.setTargetValue (parameters.releaseSeconds);

    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        const auto& slot = parameters.msegSlots[static_cast<size_t> (i)];
        auto& smoothers = parameterSmoothers.msegSlots[static_cast<size_t> (i)];

        smoothers.amountPercent.setTargetValue (slot.amountPercent);
        smoothers.offsetPercent.setTargetValue (slot.offsetPercent);
        smoothers.rateSeconds.setTargetValue (slot.rateSeconds);
    }
}

void PlaitsVoice::advanceSmoothedParameters() noexcept
{
    smoothedParameters.auxMix = parameterSmoothers.auxMix.skip (kRenderBlockSize);
    smoothedParameters.level = parameterSmoothers.level.getCurrentValue();
    smoothedParameters.coarseSemitones = parameterSmoothers.coarseSemitones.skip (kRenderBlockSize);
    smoothedParameters.fineSemitones = parameterSmoothers.fineSemitones.skip (kRenderBlockSize);
    smoothedParameters.portamentoSeconds = parameterSmoothers.portamentoSeconds.skip (kRenderBlockSize);
    smoothedParameters.attackSeconds = parameterSmoothers.attackSeconds.skip (kRenderBlockSize);
    smoothedParameters.decaySeconds = parameterSmoothers.decaySeconds.skip (kRenderBlockSize);
    smoothedParameters.sustainLevel = parameterSmoothers.sustainLevel.skip (kRenderBlockSize);
    smoothedParameters.releaseSeconds = parameterSmoothers.releaseSeconds.skip (kRenderBlockSize);

    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        auto& smoothers = parameterSmoothers.msegSlots[static_cast<size_t> (i)];
        auto& smoothed = smoothedParameters.msegSlots[static_cast<size_t> (i)];

        smoothed.amountPercent = smoothers.amountPercent.skip (kRenderBlockSize);
        smoothed.offsetPercent = smoothers.offsetPercent.skip (kRenderBlockSize);
        smoothed.rateSeconds = smoothers.rateSeconds.skip (kRenderBlockSize);
    }
}

bool PlaitsVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<PlaitsSound*> (sound) != nullptr;
}

void PlaitsVoice::drawMsegOffsets (int slotIndex) noexcept
{
    for (auto& offset : msegOffsets[static_cast<size_t> (slotIndex)])
        offset = { msegRandom.nextFloat() * 2.0f - 1.0f, msegRandom.nextFloat() * 2.0f - 1.0f };
}

void PlaitsVoice::beginGlide (float targetPitch)
{
    // Glide takes the set time whatever the interval, from wherever the pitch is.
    targetPitchSemitones = targetPitch;
    const float portamento = smoothedParameters.portamentoSeconds;
    if (portamento <= 1.0e-5f)
    {
        currentPitchSemitones = targetPitchSemitones;
        glideRateSemitonesPerSecond = 0.0f;
        return;
    }
    glideRateSemitonesPerSecond = std::abs (targetPitchSemitones - currentPitchSemitones) / portamento;
}

void PlaitsVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheelPosition)
{
    if (std::exchange (legatoPending, false) && envelopeStage != EnvelopeStage::idle)
    {
        // Legato: keep the envelope, the gate and the engine running.
        currentMidiNote = juce::jlimit (0, 127, midiNoteNumber);
        if (nextGlideStartPitch)
            currentPitchSemitones = *nextGlideStartPitch;
        nextGlideStartPitch.reset();
        beginGlide (static_cast<float> (currentMidiNote));
        pitchWheelMoved (pitchWheelPosition);
        return;
    }
    if (! keepStealTailOnStart)
        stealTailLength = stealTailPosition = 0;
    keepStealTailOnStart = false;
    resetParameterSmoothers();
    currentMidiNote = juce::jlimit (0, 127, midiNoteNumber);
    currentPitchSemitones = nextGlideStartPitch.value_or (static_cast<float> (currentMidiNote));
    nextGlideStartPitch.reset();
    beginGlide (static_cast<float> (currentMidiNote));
    currentVelocity = juce::jlimit (0.0f, 1.0f, velocity);
    msegPhases.fill (0.0);
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        drawMsegOffsets (slot);
    pitchWheelMoved (pitchWheelPosition);
    // A fresh engine for every note: struck models ring from the same state,
    // and the gate rises from low so Plaits sees a new trigger.
    engine.reset();
    gateOpen = true;
    resetResampler();
    effects.reset();
    envelopeStage = EnvelopeStage::attack;
    envelopeValue = smoothedParameters.attackSeconds <= 1.0e-5f ? 1.0f : 0.0f;
    releaseStartValue = 0.0f;
    if (envelopeValue >= 1.0f)
        envelopeStage = EnvelopeStage::decay;
}

void PlaitsVoice::stopNote (float, bool allowTailOff)
{
    // A legato start replaces the note in place; JUCE's preceding hard stop is skipped.
    if (legatoPending && envelopeStage != EnvelopeStage::idle)
        return;

    // The gate follows the key: models with their own envelopes release too.
    gateOpen = false;

    // JUCE can stop the same voice again on pedal-up, another note-off, or a
    // repeated note-on. A release is a one-way transition: restarting its slope
    // would keep old notes sounding well beyond the original key release.
    if (allowTailOff && envelopeStage == EnvelopeStage::release)
        return;

    const float release = juce::jlimit (0.0f, 10.0f, smoothedParameters.releaseSeconds);
    if (allowTailOff && release > 1.0e-5f && envelopeValue > kVoiceSilenceThreshold)
    {
        releaseStartValue = envelopeValue;
        envelopeStage = EnvelopeStage::release;
        return;
    }

    envelopeStage = EnvelopeStage::idle;
    envelopeValue = 0.0f;
    releaseStartValue = 0.0f;
    clearCurrentNote();
}

void PlaitsVoice::pitchWheelMoved (int newPitchWheelValue)
{
    const float normalised = (static_cast<float> (juce::jlimit (0, 16383, newPitchWheelValue)) - 8192.0f) / 8192.0f;
    pitchBendSemitones = juce::jlimit (-1.0f, 1.0f, normalised) * kPitchBendRangeSemitones;
}

void PlaitsVoice::controllerMoved (int, int)
{
}

void PlaitsVoice::captureStealTail()
{
    // Preserve five milliseconds of the outgoing oscillator and effect history.
    // Fixed stack storage also handles another steal during an existing fade.
    const int frames = juce::jlimit (1, kStealFadeCapacity, juce::roundToInt (sampleRate * 0.005));
    std::array<std::array<float, kStealFadeCapacity>, 2> captured {};
    float* channels[] { captured[0].data(), captured[1].data() };
    juce::AudioBuffer<float> buffer (channels, 2, frames);
    renderNextBlock (buffer, 0, frames);
    stealTail = captured;
    stealTailLength = frames;
    stealTailPosition = 0;
    keepStealTailOnStart = true;
}

void PlaitsVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const int numChannels = outputBuffer.getNumChannels();
    auto* left = numChannels > 0 ? outputBuffer.getWritePointer (0, startSample) : nullptr;
    auto* right = numChannels > 1 ? outputBuffer.getWritePointer (1, startSample) : nullptr;
    for (int i = 0; i < numSamples; ++i)
    {
        // Engine blocks follow the native stream, not host splits, so control
        // and modulation clocks are the same at every host block size.
        float sample = nextOscillatorSample();
        const float env = nextEnvelopeSample();
        const float level = juce::jlimit (0.0f, 1.5f, parameterSmoothers.level.getNextValue() + modulatedLevelOffset);
        sample *= env * currentVelocity * level;
        sample = juce::jlimit (-1.0f, 1.0f, sample);

        float sampleLeft = sample, sampleRight = sample;
        effects.process (sampleLeft, sampleRight);
        sampleLeft *= kVoiceHeadroom;
        sampleRight *= kVoiceHeadroom;
        if (stealTailPosition < stealTailLength)
        {
            const float fade = static_cast<float> (stealTailPosition) / static_cast<float> (stealTailLength);
            const auto tailIndex = static_cast<size_t> (stealTailPosition++);
            sampleLeft = stealTail[0][tailIndex] * (1.0f - fade) + sampleLeft * fade;
            sampleRight = stealTail[1][tailIndex] * (1.0f - fade) + sampleRight * fade;
        }
        if (left != nullptr)
            left[i] += numChannels == 1 ? 0.5f * (sampleLeft + sampleRight) : sampleLeft;
        if (right != nullptr)
            right[i] += sampleRight;
        for (int channel = 2; channel < numChannels; ++channel)
            outputBuffer.addSample (channel, startSample + i, 0.5f * (sampleLeft + sampleRight));

        if (! envelopeActive() && stealTailPosition >= stealTailLength)
        {
            envelopeStage = EnvelopeStage::idle;
            envelopeValue = 0.0f;
            gateOpen = false;
            clearCurrentNote();
            break;
        }
    }
}

float PlaitsVoice::nextEnvelopeSample() noexcept
{
    const float sustain = juce::jlimit (0.0f, 1.0f, smoothedParameters.sustainLevel);

    switch (envelopeStage)
    {
        case EnvelopeStage::idle:
            envelopeValue = 0.0f;
            return 0.0f;

        case EnvelopeStage::attack:
        {
            const float attack = juce::jlimit (kMinimumSmoothedEnvelopeSeconds, 10.0f, smoothedParameters.attackSeconds);
            envelopeValue += 1.0f / static_cast<float> (attack * sampleRate);
            if (envelopeValue >= 1.0f)
            {
                envelopeValue = 1.0f;
                envelopeStage = EnvelopeStage::decay;
            }
            return envelopeValue;
        }

        case EnvelopeStage::decay:
        {
            const float decay = juce::jlimit (kMinimumSmoothedEnvelopeSeconds, 10.0f, smoothedParameters.decaySeconds);
            envelopeValue -= (1.0f - sustain) / static_cast<float> (decay * sampleRate);
            if (envelopeValue <= sustain)
            {
                envelopeValue = sustain;
                envelopeStage = EnvelopeStage::sustain;
            }
            return envelopeValue;
        }

        case EnvelopeStage::sustain:
            envelopeValue = sustain;
            return envelopeValue;

        case EnvelopeStage::release:
        {
            const float release = juce::jlimit (kMinimumSmoothedEnvelopeSeconds, 10.0f, smoothedParameters.releaseSeconds);
            envelopeValue -= releaseStartValue / static_cast<float> (release * sampleRate);
            if (envelopeValue <= kVoiceSilenceThreshold)
            {
                envelopeValue = 0.0f;
                envelopeStage = EnvelopeStage::idle;
            }
            return envelopeValue;
        }
    }

    return 0.0f;
}

bool PlaitsVoice::envelopeActive() const noexcept
{
    return envelopeStage != EnvelopeStage::idle;
}

void PlaitsVoice::resetResampler() noexcept
{
    renderReadIndex = kRenderBlockSize;
    history.fill (0.0f);
    // Start one step along so the first host sample renders a block, which
    // also gives the effects their settings before they process anything.
    resamplePosition = 1.0;
}

float PlaitsVoice::nextOscillatorSample()
{
    // Pull native samples until the host sample falls between history[1] and [2].
    while (resamplePosition >= 1.0)
    {
        if (renderReadIndex == kRenderBlockSize)
            renderOscillatorBlock();
        history = { history[1], history[2], history[3], renderBuffer[static_cast<size_t> (renderReadIndex++)] };
        resamplePosition -= 1.0;
    }
    const float sample = hermite (history[0], history[1], history[2], history[3], static_cast<float> (resamplePosition));
    resamplePosition += nativeStep;
    return sample;
}

void PlaitsVoice::renderOscillatorBlock()
{
    advanceSmoothedParameters();
    const auto values = currentModulatedValues();
    effects.beginBlock (values.effects, hostSamplesPerBlock);
    modulatedLevelOffset = values.level - smoothedParameters.level;
    advanceGlide();

    PlaitsEngine::Controls controls;
    controls.engine = kPlaitsModels[static_cast<size_t> (values.model)].engine;
    controls.note = currentPitchSemitones
        + smoothedParameters.coarseSemitones
        + smoothedParameters.fineSemitones
        + pitchBendSemitones
        + values.pitchOffsetSemitones;
    controls.harmonics = values.harmonics;
    controls.timbre = values.timbre;
    controls.morph = values.morph;
    controls.gate = gateOpen;

    std::array<float, kRenderBlockSize> out {}, aux {};
    engine.render (controls, out.data(), aux.data());
    for (size_t i = 0; i < out.size(); ++i)
        renderBuffer[i] = out[i] + (aux[i] - out[i]) * values.auxMix;
    renderReadIndex = 0;
    advanceModulators (static_cast<double> (kRenderBlockSize) / PlaitsEngine::nativeSampleRate());
}

void PlaitsVoice::advanceGlide() noexcept
{
    if (glideRateSemitonesPerSecond <= 0.0f || smoothedParameters.portamentoSeconds <= 1.0e-5f)
    {
        currentPitchSemitones = targetPitchSemitones;
        return;
    }
    // Linear in pitch at the rate set when the glide began; one engine block per step.
    const float step = static_cast<float> (glideRateSemitonesPerSecond * kRenderBlockSize / PlaitsEngine::nativeSampleRate());
    currentPitchSemitones += juce::jlimit (-step, step, targetPitchSemitones - currentPitchSemitones);
}

void PlaitsVoice::advanceModulators (double seconds) noexcept
{
    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        const auto& slot = parameters.msegSlots[static_cast<size_t> (i)];
        if (slot.destination == static_cast<int> (MsegDestination::off) || slot.shape == nullptr)
            continue;

        const float rate = juce::jlimit (0.01f, 64.0f, smoothedParameters.msegSlots[static_cast<size_t> (i)].rateSeconds);
        auto& phase = msegPhases[static_cast<size_t> (i)];
        phase += seconds / static_cast<double> (rate);

        if (slot.loop)
        {
            // Each new cycle gets new random offsets, so a loop never repeats exactly.
            if (phase >= 1.0)
                drawMsegOffsets (i);
            phase -= std::floor (phase);
        }
        else
            phase = juce::jlimit (0.0, 1.0, phase);
    }
}

PlaitsModulatedValues PlaitsVoice::currentModulatedValues() const noexcept
{
    PlaitsModulatedValues values;
    values.effects = parameters.effects;
    float model = static_cast<float> (juce::jlimit (0, kLastModel, parameters.model));
    values.harmonics = juce::jlimit (0.0f, 1.0f, parameters.harmonics);
    values.timbre = juce::jlimit (0.0f, 1.0f, parameters.timbre);
    values.morph = juce::jlimit (0.0f, 1.0f, parameters.morph);
    values.auxMix = juce::jlimit (0.0f, 1.0f, smoothedParameters.auxMix);
    values.level = juce::jlimit (0.0f, 1.5f, smoothedParameters.level);
    values.pitchOffsetSemitones = 0.0f;

    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        const auto& slot = parameters.msegSlots[static_cast<size_t> (i)];
        const float wave = currentMsegWave (i);

        switch (static_cast<MsegDestination> (slot.destination))
        {
            case MsegDestination::pitch:
                values.pitchOffsetSemitones += targetDelta (0.0f, -36.0f, 36.0f, wave);
                break;
            case MsegDestination::model:
                model += targetDelta (model, 0.0f, static_cast<float> (kLastModel), wave);
                break;
            case MsegDestination::harmonics:
                values.harmonics += targetDelta (values.harmonics, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::timbre:
                values.timbre += targetDelta (values.timbre, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::morph:
                values.morph += targetDelta (values.morph, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::auxMix:
                values.auxMix += targetDelta (values.auxMix, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::level:
                values.level += targetDelta (values.level, 0.0f, 1.5f, wave);
                break;
            case MsegDestination::filterFrequency:
            {
                const float logFrequency = std::log2 (juce::jlimit (20.0f, 20000.0f, values.effects.filterFrequency));
                values.effects.filterFrequency = std::exp2 (logFrequency
                    + targetDelta (logFrequency, std::log2 (20.0f), std::log2 (20000.0f), wave));
                break;
            }
            case MsegDestination::filterQ:
                values.effects.filterQ += targetDelta (values.effects.filterQ, 1.0f, 30.0f, wave);
                break;
            case MsegDestination::distortionAmount:
                values.effects.distortionAmount += targetDelta (values.effects.distortionAmount, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::distortionMix:
                values.effects.distortionMix += targetDelta (values.effects.distortionMix, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::distortionOutput:
                values.effects.distortionOutput += targetDelta (values.effects.distortionOutput, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::filterType:
                values.effects.filterType = juce::jlimit (0, 3, juce::roundToInt (
                    static_cast<float> (values.effects.filterType)
                    + targetDelta (static_cast<float> (values.effects.filterType), 0.0f, 3.0f, wave)));
                break;
            case MsegDestination::distortionType:
                values.effects.distortionType = lodo::engine::fx::clampDistortionType (juce::roundToInt (
                    static_cast<float> (values.effects.distortionType)
                    + targetDelta (static_cast<float> (values.effects.distortionType), 0.0f,
                                   static_cast<float> (lodo::engine::fx::maxDistortionType), wave)));
                break;
            case MsegDestination::off:
                break;
        }
    }

    values.model = juce::jlimit (0, kLastModel, juce::roundToInt (model));
    values.harmonics = juce::jlimit (0.0f, 1.0f, values.harmonics);
    values.timbre = juce::jlimit (0.0f, 1.0f, values.timbre);
    values.morph = juce::jlimit (0.0f, 1.0f, values.morph);
    values.auxMix = juce::jlimit (0.0f, 1.0f, values.auxMix);
    values.level = juce::jlimit (0.0f, 1.5f, values.level);
    values.pitchOffsetSemitones = juce::jlimit (-36.0f, 36.0f, values.pitchOffsetSemitones);
    return values;
}

float PlaitsVoice::currentMsegWave (int slotIndex) const noexcept
{
    if (slotIndex < 0 || slotIndex >= kMsegSlotCount)
        return 0.0f;

    const auto& slot = parameters.msegSlots[static_cast<size_t> (slotIndex)];
    if (slot.destination == static_cast<int> (MsegDestination::off) || slot.shape == nullptr)
        return 0.0f;

    const float rawWave = slot.shape->evaluateUnipolar (static_cast<float> (msegPhases[static_cast<size_t> (slotIndex)]),
                                                        msegOffsets[static_cast<size_t> (slotIndex)],
                                                        slot.randomPercent * 0.01f) * 2.0f - 1.0f;
    const auto& smoothedSlot = smoothedParameters.msegSlots[static_cast<size_t> (slotIndex)];
    const float scaled = (rawWave * (smoothedSlot.amountPercent * 0.01f)) + (smoothedSlot.offsetPercent * 0.01f);
    return juce::jlimit (-1.0f, 1.0f, scaled);
}

float PlaitsVoice::targetDelta (float base, float minimum, float maximum, float wave) noexcept
{
    const float clampedBase = juce::jlimit (minimum, maximum, base);
    if (wave >= 0.0f)
        return wave * (maximum - clampedBase);

    return wave * (clampedBase - minimum);
}
} // namespace macro_osc_ii
