#pragma once

#include "DSP/Mseg.h"
#include "DSP/VoiceEffects.h"

#include <array>
#include <memory>
#include <optional>

#include <juce_audio_basics/juce_audio_basics.h>

namespace plaits
{
class Voice;
}

namespace macro_osc_ii
{
// The 24 models in our order, grouped by kind of sound rather than by Plaits'
// firmware banks, and the Plaits engine each one plays.
struct PlaitsModel
{
    const char* name;
    int engine;
};

inline constexpr int kModelCount = 24;
inline constexpr std::array<PlaitsModel, kModelCount> kPlaitsModels { {
    // synth
    { "Virtual Analog", 8 }, { "VA VCF", 0 }, { "Waveshaping", 9 }, { "Wavetable", 13 }, { "Wave Terrain", 5 },
    { "Additive", 12 }, { "Grain", 11 }, { "Swarm", 16 }, { "Noise", 17 }, { "Speech", 15 },
    // fm
    { "FM", 10 }, { "Phase Dist", 1 }, { "6-Op FM 1", 2 }, { "6-Op FM 2", 3 }, { "6-Op FM 3", 4 },
    // paraphonic
    { "Chord", 14 }, { "String Machine", 6 }, { "Chiptune", 7 },
    // physical
    { "String", 19 }, { "Modal", 20 }, { "Particle", 18 },
    // percussive
    { "Bass Drum", 21 }, { "Snare Drum", 22 }, { "Hi-Hat", 23 }
} };

// Struck models sound from the gate's rising edge and then decay.
[[nodiscard]] inline bool isStruckModel (int model) noexcept
{
    const int engine = kPlaitsModels[static_cast<size_t> (juce::jlimit (0, kModelCount - 1, model))].engine;
    return engine >= 19; // string, modal and the three drums
}

struct PlaitsVoiceParameters
{
    VoiceEffectsParameters effects;
    int model {};
    float harmonics { 0.5f };
    float timbre { 0.5f };
    float morph { 0.5f };
    float auxMix {};
    float level { 0.8f };
    float coarseSemitones {};
    float fineSemitones {};
    float portamentoSeconds {};
    float attackSeconds { 0.005f };
    float decaySeconds { 0.25f };
    float sustainLevel { 0.8f };
    float releaseSeconds { 0.25f };

    struct MsegSlot
    {
        int destination {};
        float amountPercent { 50.0f };
        float offsetPercent {};
        float rateSeconds { 1.0f };
        float randomPercent {};
        bool loop { true };
        const MsegShape* shape {};
    };

    std::array<MsegSlot, kMsegSlotCount> msegSlots;
};

struct PlaitsModulatedValues
{
    VoiceEffectsParameters effects;
    int model {};
    float harmonics {};
    float timbre {};
    float morph {};
    float auxMix {};
    float level {};
    float pitchOffsetSemitones {};
};

class PlaitsSound final : public juce::SynthesiserSound
{
public:
    [[nodiscard]] bool appliesToNote (int) override { return true; }
    [[nodiscard]] bool appliesToChannel (int) override { return true; }
};

// One Plaits voice and its own working memory. Plaits renders at its native
// rate (about 47.9 kHz) in 12-sample blocks; the output is resampled to the
// host rate, so its envelopes, decays and speech keep their hardware timing.
class PlaitsEngine
{
public:
    static constexpr int kBlockSize = 12;
    static constexpr int kRamBytes = 16 * 1024;

    struct Controls
    {
        int engine {}; // a Plaits engine index, not our model order
        float note { 60.0f };
        float harmonics { 0.5f };
        float timbre { 0.5f };
        float morph { 0.5f };
        bool gate {};
    };

    PlaitsEngine();
    ~PlaitsEngine();

    // Clears all engine state, as when the module powers up.
    void reset();
    // Renders one native block of the out and aux channels, scaled to -1..1.
    void render (const Controls& controls, float* out, float* aux);
    [[nodiscard]] static double nativeSampleRate() noexcept;

private:
    struct VoiceDeleter
    {
        void operator() (plaits::Voice* voice) const noexcept;
    };

    std::unique_ptr<plaits::Voice, VoiceDeleter> voice;
    alignas (16) std::array<char, kRamBytes> ram {};

    JUCE_DECLARE_NON_COPYABLE (PlaitsEngine)
};

class PlaitsVoice final : public juce::SynthesiserVoice
{
public:
    PlaitsVoice();

    void prepare (double newSampleRate);
    void setParameters (const PlaitsVoiceParameters& newParameters);
    void captureStealTail();
    void setGlideStartPitch (float pitch) noexcept { nextGlideStartPitch = pitch; }
    // The next startNote moves this voice to the new note without retriggering.
    void prepareLegato() noexcept { legatoPending = true; }
    [[nodiscard]] float getCurrentPitch() const noexcept { return currentPitchSemitones; }

    [[nodiscard]] bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int controllerNumber, int newControllerValue) override;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    static constexpr int kRenderBlockSize = PlaitsEngine::kBlockSize;
    static constexpr float kPitchBendRangeSemitones = 2.0f;
    static constexpr int kStealFadeCapacity = 2048;
    using LinearSmoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    enum class EnvelopeStage : std::uint8_t
    {
        idle,
        attack,
        decay,
        sustain,
        release
    };

    struct SmoothedParameterSnapshot
    {
        float auxMix {};
        float level { 0.8f };
        float coarseSemitones {};
        float fineSemitones {};
        float portamentoSeconds {};
        float attackSeconds { 0.005f };
        float decaySeconds { 0.25f };
        float sustainLevel { 0.8f };
        float releaseSeconds { 0.25f };

        struct MsegSlot
        {
            float amountPercent { 50.0f };
            float offsetPercent {};
            float rateSeconds { 1.0f };
        };

        std::array<MsegSlot, kMsegSlotCount> msegSlots;
    };

    // Control smoothers step once per native block; level steps per host sample.
    struct ParameterSmoothers
    {
        LinearSmoother auxMix;
        LinearSmoother level;
        LinearSmoother coarseSemitones;
        LinearSmoother fineSemitones;
        LinearSmoother portamentoSeconds;
        LinearSmoother attackSeconds;
        LinearSmoother decaySeconds;
        LinearSmoother sustainLevel;
        LinearSmoother releaseSeconds;

        struct MsegSlot
        {
            LinearSmoother amountPercent;
            LinearSmoother offsetPercent;
            LinearSmoother rateSeconds;
        };

        std::array<MsegSlot, kMsegSlotCount> msegSlots;
    };

    PlaitsEngine engine;
    VoiceEffects effects;
    PlaitsVoiceParameters parameters;
    ParameterSmoothers parameterSmoothers;
    SmoothedParameterSnapshot smoothedParameters;
    EnvelopeStage envelopeStage { EnvelopeStage::idle };
    float envelopeValue {};
    float releaseStartValue {};
    double sampleRate { 44100.0 };
    // Native samples per host sample.
    double nativeStep { 1.0 };
    int hostSamplesPerBlock { kRenderBlockSize };
    float currentVelocity {};
    int currentMidiNote { 60 };
    float currentPitchSemitones { 60.0f };
    float targetPitchSemitones { 60.0f };
    std::optional<float> nextGlideStartPitch;
    float glideRateSemitonesPerSecond {};
    bool legatoPending {};
    bool gateOpen {};
    float pitchBendSemitones {};
    std::array<double, kMsegSlotCount> msegPhases {};
    // This voice's random point offsets per slot: drawn at each note and each loop.
    std::array<std::array<MsegPoint, kMsegMaxPoints>, kMsegSlotCount> msegOffsets {};
    juce::Random msegRandom;
    std::array<float, kRenderBlockSize> renderBuffer {};
    int renderReadIndex { kRenderBlockSize };
    // The last four native samples, for cubic interpolation at the host rate.
    std::array<float, 4> history {};
    double resamplePosition {};
    float modulatedLevelOffset {};
    std::array<std::array<float, kStealFadeCapacity>, 2> stealTail {};
    int stealTailLength {};
    int stealTailPosition {};
    bool keepStealTailOnStart {};

    void resetParameterSmoothers();
    void setParameterSmootherTargets() noexcept;
    void advanceSmoothedParameters() noexcept;
    [[nodiscard]] float nextEnvelopeSample() noexcept;
    [[nodiscard]] bool envelopeActive() const noexcept;
    [[nodiscard]] float nextOscillatorSample();
    void renderOscillatorBlock();
    void resetResampler() noexcept;
    void beginGlide (float targetPitch);
    void drawMsegOffsets (int slotIndex) noexcept;
    void advanceGlide() noexcept;
    void advanceModulators (double seconds) noexcept;
    [[nodiscard]] PlaitsModulatedValues currentModulatedValues() const noexcept;
    [[nodiscard]] float currentMsegWave (int slotIndex) const noexcept;
    [[nodiscard]] static float targetDelta (float base, float minimum, float maximum, float wave) noexcept;
};
} // namespace macro_osc_ii
