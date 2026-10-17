#include "DSP/PlaitsVoice.h"
#include "PluginEditor.h"
#include "EditorParameters.h"
#include "stmlib/utils/random.h"

#include <iostream>
#include <stdexcept>
#include <vector>

using namespace macro_osc_ii;
namespace macro_osc_ii { bool runPolyphonyTests(); }

namespace
{
void require (bool condition, const std::string& message)
{
    if (! condition)
        throw std::runtime_error (message);
}

juce::AudioBuffer<float> renderVoice (const PlaitsVoiceParameters& parameters,
                                    double rate, int block, bool refreshParameters = false)
{
    stmlib::Random::Seed (1);
    juce::Synthesiser synth;
    auto* voice = new PlaitsVoice();
    synth.addVoice (voice);
    synth.addSound (new PlaitsSound());
    synth.setCurrentPlaybackSampleRate (rate);
    synth.setMinimumRenderingSubdivisionSize (1, true);
    voice->prepare (rate);
    voice->setParameters (parameters);
    synth.noteOn (1, 60, 0.8f);
    juce::AudioBuffer<float> output (2, 4800);
    output.clear();
    for (int at = 0; at < output.getNumSamples(); at += block)
    {
        if (refreshParameters)
        {
            voice->setParameters (parameters);
            synth.handlePitchWheel (1, 8192);
        }
        synth.renderNextBlock (output, {}, at, std::min (block, output.getNumSamples() - at));
    }
    return output;
}

float peakDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float peak = 0.0f;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
        {
            require (std::isfinite (a.getSample (ch, i)) && std::isfinite (b.getSample (ch, i)),
                     "non-finite audio");
            peak = std::max (peak, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
        }
    return peak;
}

std::vector<float> renderEngine (PlaitsEngine& engine, int plaitsEngine, int blocks)
{
    PlaitsEngine::Controls controls;
    controls.engine = plaitsEngine;
    controls.note = 60.0f;
    controls.harmonics = 0.4f;
    controls.timbre = 0.6f;
    controls.morph = 0.3f;
    controls.gate = true;
    std::vector<float> result;
    std::array<float, PlaitsEngine::kBlockSize> out {}, aux {};
    stmlib::Random::Seed (1);
    for (int block = 0; block < blocks; ++block)
    {
        engine.render (controls, out.data(), aux.data());
        result.insert (result.end(), out.begin(), out.end());
        result.insert (result.end(), aux.begin(), aux.end());
    }
    return result;
}

// Our model order is a regrouping: every Plaits engine appears exactly once.
void verifyModelTable()
{
    std::array<int, kModelCount> uses {};
    for (const auto& model : kPlaitsModels)
    {
        require (model.engine >= 0 && model.engine < kModelCount, std::string ("bad engine for ") + model.name);
        ++uses[static_cast<size_t> (model.engine)];
    }
    for (int engine = 0; engine < kModelCount; ++engine)
        require (uses[static_cast<size_t> (engine)] == 1, "engine " + std::to_string (engine) + " not mapped exactly once");
    int total = 0;
    for (const auto& family : editor::modelFamilies)
        total += family.count;
    require (total == kModelCount, "model families do not cover every model");
}

// Every note starts from a reset engine, so a model must sound the same after
// another model has used the shared working memory as it does from power-up.
void verifyInitialisation()
{
    for (int model = 0; model < kModelCount; ++model)
    {
        PlaitsEngine fresh, used;
        renderEngine (used, (model + 11) % kModelCount, 400);
        used.reset();
        require (renderEngine (fresh, model, 400) == renderEngine (used, model, 400),
                 "engine state survived a reset, model=" + std::to_string (model));
    }
}

void verifyContinuity()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (int model = 0; model < kModelCount; ++model)
        {
            PlaitsVoiceParameters parameters;
            parameters.model = model;
            const auto reference = renderVoice (parameters, rate, 24);
            require (reference.getMagnitude (0, reference.getNumSamples()) > 1.0e-3f,
                     "silent model=" + std::to_string (model));
            for (int block : { 1, 57, 64, 256 })
                require (peakDifference (reference, renderVoice (parameters, rate, block, true)) < 1.0e-6f,
                         "sample continuity, model=" + std::to_string (model)
                             + " block=" + std::to_string (block));
        }

    PlaitsVoiceParameters parameters;
    parameters.auxMix = 0.4f;
    parameters.harmonics = 0.7f;
    const auto reference = renderVoice (parameters, 44100.0, 24);
    require (peakDifference (reference, renderVoice (parameters, 44100.0, 57)) < 1.0e-6f,
             "aux mix changes with host block size");
}

// Plaits renders at its own rate; resampling must keep A4 at 440 Hz at any host rate.
void verifyPitch()
{
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        juce::Synthesiser synth;
        auto* voice = new PlaitsVoice();
        synth.addVoice (voice);
        synth.addSound (new PlaitsSound());
        synth.setCurrentPlaybackSampleRate (rate);
        voice->prepare (rate);
        PlaitsVoiceParameters parameters;
        parameters.model = 10; // two-operator FM; no modulation leaves a sine
        parameters.timbre = 0.0f;
        parameters.sustainLevel = 1.0f;
        voice->setParameters (parameters);
        synth.noteOn (1, 69, 0.8f);
        const int frames = static_cast<int> (rate);
        juce::AudioBuffer<float> output (2, frames);
        output.clear();
        synth.renderNextBlock (output, {}, 0, frames);
        // Time between the first and last rising zero crossings after a settling tenth of a second.
        double first = -1.0, last = -1.0;
        int cycles = -1;
        for (int i = frames / 10; i < frames; ++i)
        {
            const float a = output.getSample (0, i - 1), b = output.getSample (0, i);
            if (a <= 0.0f && b > 0.0f)
            {
                const double at = (i - 1) + a / (a - b);
                if (first < 0.0) first = at;
                last = at;
                ++cycles;
            }
        }
        const double frequency = cycles * rate / (last - first);
        require (std::abs (frequency - 440.0) < 0.5,
                 "A4 measured " + std::to_string (frequency) + " Hz at " + std::to_string (rate));
    }
}

void verifyEffects()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (int type = 0; type <= lodo::engine::fx::maxDistortionType; ++type)
        {
            PlaitsVoiceParameters parameters;
            parameters.model = 0; // virtual analog
            parameters.effects.distortionType = type;
            parameters.effects.distortionAmount = 0.7f;
            parameters.effects.filterType = 1 + type % 3;
            parameters.effects.filterFrequency = 2200.0f;
            parameters.effects.filterQ = 2.0f;
            require (peakDifference (renderVoice (parameters, rate, 24),
                                     renderVoice (parameters, rate, 57)) < 1.0e-6f,
                     "effect continuity, type=" + std::to_string (type));
        }

    // A low-pass must substantially reject an above-cutoff sine; bypass must
    // preserve the source exactly. This exercises actual audio, not parameters.
    VoiceEffects effects;
    effects.prepare (48000.0);
    VoiceEffectsParameters parameters;
    effects.beginBlock (parameters, 24);
    float left = 0.25f, right = -0.25f;
    effects.process (left, right);
    require (std::abs (left - 0.25f) + std::abs (right + 0.25f) == 0.0f, "insert bypass changed audio");
    effects.reset();
    parameters.filterType = 1;
    parameters.filterFrequency = 100.0f;
    double energy = 0.0;
    for (int i = 0; i < 4800; ++i)
    {
        if (i % 24 == 0) effects.beginBlock (parameters, 24);
        left = right = 0.2f * std::sin (juce::MathConstants<float>::twoPi * 4000.0f * i / 48000.0f);
        effects.process (left, right);
        if (i >= 2400) energy += left * left;
    }
    require (energy / 2400.0 < 1.0e-6, "low-pass did not reject high frequencies");

    // The dry path survives zero wet Output; full-wet zero Output is silent.
    for (int type = 1; type <= lodo::engine::fx::maxDistortionType; ++type)
    {
        parameters = {};
        parameters.distortionType = type;
        parameters.distortionMix = 0.25f;
        parameters.distortionOutput = 0.0f;
        effects.reset();
        effects.beginBlock (parameters, 24);
        left = right = 0.2f;
        effects.process (left, right);
        require (std::abs (left - 0.15f) < 1.0e-6f, "wet Output attenuated dry audio");
        effects.reset();
        parameters.distortionMix = 1.0f;
        effects.beginBlock (parameters, 24);
        left = right = 0.2f;
        effects.process (left, right);
        require (left == 0.0f && right == 0.0f, "zero wet Output is not silent");
    }

    // Exercise live type switches and endpoint automation through both stages.
    effects.reset();
    for (int block = 0; block < 600; ++block)
    {
        parameters.distortionType = block % 20;
        parameters.distortionAmount = block % 2 == 0 ? 0.0f : 1.0f;
        parameters.distortionOutput = 1.0f;
        parameters.filterType = block % 4;
        parameters.filterFrequency = block % 2 == 0 ? 20.0f : 20000.0f;
        parameters.filterQ = 30.0f;
        effects.beginBlock (parameters, 24);
        for (int i = 0; i < 24; ++i)
        {
            left = 0.8f * std::sin (0.3f * (block * 24 + i));
            right = -left;
            effects.process (left, right);
            require (std::isfinite (left) && std::isfinite (right)
                         && std::abs (left) <= 4.0f && std::abs (right) <= 4.0f,
                     "unstable effect automation");
        }
    }
}

void verifyEffectModulation()
{
    MsegShape shape { { { 0.0f, 1.0f }, { 1.0f, 1.0f } } };
    for (auto target : { MsegDestination::filterFrequency, MsegDestination::filterQ,
                         MsegDestination::distortionAmount, MsegDestination::distortionMix,
                         MsegDestination::distortionOutput, MsegDestination::filterType,
                         MsegDestination::distortionType })
    {
        PlaitsVoiceParameters parameters;
        parameters.model = 0; // virtual analog
        parameters.effects.filterType = 1;
        parameters.effects.filterFrequency = 1500.0f;
        parameters.effects.distortionType = 1;
        parameters.effects.distortionAmount = 0.2f;
        parameters.effects.distortionMix = 0.3f;
        const auto dry = renderVoice (parameters, 48000.0, 57);
        parameters.msegSlots[0].destination = static_cast<int> (target);
        parameters.msegSlots[0].amountPercent = 100.0f;
        parameters.msegSlots[0].shape = &shape;
        const auto modulated = renderVoice (parameters, 48000.0, 57);
        require (peakDifference (dry, modulated) > 0.001f, "inaudible effects modulation target");
        require (peakDifference (modulated, renderVoice (parameters, 48000.0, 1)) < 1.0e-6f,
                 "effect modulation depends on host partition");
    }
}

void setParameter (MacroOscIIAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getState().getParameter (id);
    require (parameter != nullptr, std::string ("missing parameter: ") + id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void verifyState()
{
    MacroOscIIAudioProcessor processor;
    const std::array<std::pair<const char*, float>, 8> values {{
        { ParamIDs::filterType, 2.0f }, { ParamIDs::filterFrequency, 830.0f },
        { ParamIDs::filterQ, 7.2f }, { ParamIDs::distortionType, 7.0f },
        { ParamIDs::distortionAmount, 0.72f }, { ParamIDs::distortionMix, 0.61f },
        { ParamIDs::distortionOutput, 0.37f },
        { ParamIDs::msegDestination[0], static_cast<float> (MsegDestination::filterFrequency) }
    }};
    for (const auto& value : values) setParameter (processor, value.first, value.second);
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    MacroOscIIAudioProcessor restored;
    restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    for (const auto& value : values)
        require (std::abs (restored.getState().getRawParameterValue (value.first)->load() - value.second) < 0.001f,
                 std::string ("preset round trip: ") + value.first);
}

void verifyOutputSafety()
{
    // Quiet audio passes untouched; hot audio never exceeds full scale.
    OutputLimiter limiter;
    limiter.prepare (48000.0);
    juce::AudioBuffer<float> quiet (2, 4800), hot (2, 4800);
    for (int i = 0; i < 4800; ++i)
        for (int channel = 0; channel < 2; ++channel)
        {
            const float sine = std::sin (juce::MathConstants<float>::twoPi * 220.0f * static_cast<float> (i) / 48000.0f);
            quiet.setSample (channel, i, 0.5f * sine);
            hot.setSample (channel, i, 4.0f * sine);
        }
    auto untouched = quiet;
    limiter.process (quiet);
    for (int i = 0; i < 4800; ++i)
        require (juce::exactlyEqual (quiet.getSample (0, i), untouched.getSample (0, i)), "limiter changed audio below its threshold");
    limiter.reset();
    limiter.process (hot);
    require (hot.getMagnitude (0, 4800) <= 1.0f, "limiter let the output exceed full scale");
    require (hot.getMagnitude (2400, 2400) > 0.8f, "limiter pulled a hot signal down too far");

    // A full chord through loud distortion stays within full scale at the output.
    MacroOscIIAudioProcessor processor;
    setParameter (processor, ParamIDs::distortionType, 5.0f);
    setParameter (processor, ParamIDs::distortionAmount, 1.0f);
    setParameter (processor, ParamIDs::distortionOutput, 1.0f);
    setParameter (processor, ParamIDs::sustain, 1.0f);
    processor.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int note : { 36, 43, 48, 55, 60, 64, 67, 72 })
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);
    float peak = 0.0f;
    for (int block = 0; block < 40; ++block)
    {
        processor.processBlock (buffer, midi);
        midi.clear();
        peak = juce::jmax (peak, buffer.getMagnitude (0, 512));
    }
    require (peak <= 1.0f, "an eight-note chord through distortion exceeded full scale");
}

void verifyPresets()
{
    MacroOscIIAudioProcessor processor;
    setParameter (processor, ParamIDs::timbre, 0.25f);
    setParameter (processor, ParamIDs::voiceMode, 2.0f);
    processor.setMsegPoints (1, { { 0.0f, 0.1f }, { 0.4f, 0.9f }, { 1.0f, 0.3f } });
    processor.setEditorScale (1.25f);
    const auto file = juce::File::createTempFile ("mosc2");
    require (processor.savePreset (file), "preset did not save");
    require (processor.getPresetName() == file.getFileNameWithoutExtension(), "saving did not name the preset");

    processor.resetToDefaults();
    processor.setEditorScale (0.75f);
    require (processor.getPresetName() == "init", "initialise did not reset the preset name");
    require (processor.loadPreset (file), "preset did not load");
    require (std::abs (processor.getState().getRawParameterValue (ParamIDs::timbre)->load() - 0.25f) < 1.0e-4f
                 && juce::roundToInt (processor.getState().getRawParameterValue (ParamIDs::voiceMode)->load()) == 2,
             "preset did not restore parameters");
    require (processor.getMsegPoints (1).size() == 3, "preset did not restore a modulation shape");
    require (juce::approximatelyEqual (processor.getEditorScale(), 0.75f), "preset overrode the interface size");
    file.deleteFile();
}

void verifyEditorSize()
{
    MacroOscIIAudioProcessor processor;
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        require (editor->getWidth() == 960, "editor did not open at 100%");
    }
    require (juce::approximatelyEqual (processor.getEditorScale(), 1.0f), "opening the editor saved the wrong size");
    processor.setEditorScale (1.25f);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    require (editor->getWidth() == 1200, "editor did not reopen at its saved size");
}

// Struck models sound from the gate's rising edge, which reaches Plaits
// through its trigger input; a held note must ring rather than stay silent.
void verifyStrikeModels()
{
    for (const int model : { 18, 19, 21, 22, 23 }) // string, modal, bass drum, snare drum, hi-hat
    {
        MacroOscIIAudioProcessor processor;
        setParameter (processor, ParamIDs::model, static_cast<float> (model));
        setParameter (processor, ParamIDs::sustain, 1.0f);
        setParameter (processor, ParamIDs::msegDestination[0], 0.0f);
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> block (2, 512);
        float peak = 0.0f;
        for (int index = 0; index < 8; ++index)
        {
            juce::MidiBuffer midi;
            if (index == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);
            processor.processBlock (block, midi);
            peak = juce::jmax (peak, block.getMagnitude (0, 0, 512));
        }
        require (peak > 0.01f, "struck model " + std::to_string (model) + " did not sound");
    }
}

// The gate follows the key: a six-operator patch releases with its own
// envelopes when the key is let go, even while the amp envelope holds.
void verifyGate()
{
    PlaitsVoiceParameters parameters;
    parameters.model = 12; // 6-op FM 1
    parameters.sustainLevel = 1.0f;
    parameters.releaseSeconds = 4.0f;
    juce::Synthesiser synth;
    auto* voice = new PlaitsVoice();
    synth.addVoice (voice);
    synth.addSound (new PlaitsSound());
    synth.setCurrentPlaybackSampleRate (48000.0);
    voice->prepare (48000.0);
    voice->setParameters (parameters);
    synth.noteOn (1, 60, 1.0f);
    juce::AudioBuffer<float> held (2, 24000), released (2, 48000);
    held.clear();
    released.clear();
    synth.renderNextBlock (held, {}, 0, held.getNumSamples());
    synth.noteOff (1, 60, 0.0f, true);
    synth.renderNextBlock (released, {}, 0, released.getNumSamples());
    require (held.getRMSLevel (0, 12000, 12000) > 0.01f, "six-operator patch silent while held");
    require (released.getRMSLevel (0, 36000, 12000) < 0.5f * held.getRMSLevel (0, 12000, 12000),
             "six-operator patch ignored the released gate");
}

void captureEditor (const juce::File& file, bool useDefaults = false, int width = 0, int model = -1)
{
    MacroOscIIAudioProcessor processor;
    if (model >= 0)
        setParameter (processor, ParamIDs::model, static_cast<float> (model));
    else if (! useDefaults)
    {
        setParameter (processor, ParamIDs::distortionType, 5.0f);
        setParameter (processor, ParamIDs::distortionAmount, 0.65f);
        setParameter (processor, ParamIDs::filterType, 1.0f);
        setParameter (processor, ParamIDs::filterFrequency, 1800.0f);
        setParameter (processor, ParamIDs::filterQ, 3.0f);
        setParameter (processor, ParamIDs::msegDestination[0], static_cast<float> (MsegDestination::filterFrequency));
        setParameter (processor, ParamIDs::model, 1.0f);
        setParameter (processor, ParamIDs::harmonics, 40.0f / 127.0f);
        setParameter (processor, ParamIDs::timbre, 90.0f / 127.0f);
        setParameter (processor, ParamIDs::morph, 30.0f / 127.0f);
        setParameter (processor, ParamIDs::attack, 0.3f);
        setParameter (processor, ParamIDs::sustain, 0.4f);
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (width > 0)
        editor->setSize (width, width * editor->getHeight() / editor->getWidth());
    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    auto stream = file.createOutputStream();
    require (stream != nullptr, "could not open editor capture");
    stream->setPosition (0);
    stream->truncate();
    require (juce::PNGImageFormat().writeImageToStream (image, *stream), "editor capture failed");
}

void verifyKnobGestures()
{
    MacroOscIIAudioProcessor processor;
    ParameterSlider knob (processor, {});
    knob.configure ("Pitch", ParamIDs::coarse, juce::Colours::white, "st", 0);
    const auto event = [&] (float y, bool shift)
    {
        const auto time = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 20.0f, y },
                                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                     | (shift ? juce::ModifierKeys::shiftModifier : 0)),
                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &knob, &knob, time,
                                 { 20.0f, 100.0f }, time, 1, true);
    };
    knob.mouseDown (event (100.0f, false));
    for (int i = 1; i <= 10; ++i) knob.mouseDrag (event (100.0f - i, false));
    knob.mouseUp (event (90.0f, false));
    require (std::abs (processor.getState().getRawParameterValue (ParamIDs::coarse)->load() - 4.0f) < 0.01f,
             "slow stepped knob drag lost fractional movement");
    require (knob.keyPressed (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0)),
             "knob did not handle fine keyboard adjustment");
    require (std::abs (processor.getState().getRawParameterValue (ParamIDs::coarse)->load() - 5.0f) < 0.01f,
             "fine arrow key cannot move stepped parameter");

    knob.configure ("Amount", ParamIDs::distortionAmount, juce::Colours::white, "%", 1, 100.0f);
    knob.mouseDown (event (100.0f, false));
    knob.mouseDrag (event (10.0f, false));
    const float beforeShift = processor.getState().getRawParameterValue (ParamIDs::distortionAmount)->load();
    knob.mouseDrag (event (10.0f, true));
    const float afterShift = processor.getState().getRawParameterValue (ParamIDs::distortionAmount)->load();
    knob.mouseUp (event (10.0f, true));
    require (std::abs (beforeShift - afterShift) < 1.0e-6f, "Shift changed the value without pointer movement");
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        if (argc == 2 && juce::String (argv[1]) == "--polyphony")
            return runPolyphonyTests() ? 0 : 1;
        if ((argc == 3 || argc == 4) && (juce::String (argv[1]) == "--snapshot" || juce::String (argv[1]) == "--snapshot-default"))
        {
            captureEditor (juce::File (argv[2]), juce::String (argv[1]) == "--snapshot-default",
                           argc == 4 ? juce::String (argv[3]).getIntValue() : 0);
            return 0;
        }
        if (argc == 4 && juce::String (argv[1]) == "--snapshot-model")
        {
            captureEditor (juce::File (argv[2]), true, 0, juce::String (argv[3]).getIntValue());
            return 0;
        }
        verifyModelTable();
        verifyInitialisation();
        verifyContinuity();
        verifyPitch();
        std::cout << "PASS: all 24 Plaits models reset deterministically, keep sample continuity and track pitch\n";
        verifyEffects();
        verifyEffectModulation();
        verifyState();
        verifyKnobGestures();
        verifyOutputSafety();
        verifyPresets();
        verifyEditorSize();
        verifyStrikeModels();
        verifyGate();
        require (runPolyphonyTests(), "keyboard polyphony regression");
        std::cout << "PASS: 19 distortion modes, filter, effects modulation, automation, presets and gate\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
