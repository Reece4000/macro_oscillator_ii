#pragma once

#include "DSP/PlaitsVoice.h"
#include "DSP/PlaitsSynthesiser.h"
#include "DSP/Mseg.h"
#include "DSP/OutputLimiter.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

namespace macro_osc_ii
{
namespace ParamIDs
{
static constexpr const char* filterType = "filterType";
static constexpr const char* filterFrequency = "filterFrequency";
static constexpr const char* filterQ = "filterQ";
static constexpr const char* distortionType = "distortionType";
static constexpr const char* distortionAmount = "distortionAmount";
static constexpr const char* distortionMix = "distortionMix";
static constexpr const char* distortionOutput = "distortionOutput";
static constexpr const char* model = "model";
static constexpr const char* harmonics = "harmonics";
static constexpr const char* timbre = "timbre";
static constexpr const char* morph = "morph";
static constexpr const char* auxMix = "auxMix";
static constexpr const char* level = "level";
static constexpr const char* coarse = "coarse";
static constexpr const char* fine = "fine";
static constexpr const char* portamento = "portamento";
static constexpr const char* voiceMode = "voiceMode";
static constexpr const char* voices = "voices";
static constexpr const char* attack = "attack";
static constexpr const char* decay = "decay";
static constexpr const char* sustain = "sustain";
static constexpr const char* release = "release";
static constexpr std::array<const char*, kMsegSlotCount> msegDestination {
    "mseg1Destination", "mseg2Destination", "mseg3Destination"
};
static constexpr std::array<const char*, kMsegSlotCount> msegAmount {
    "mseg1Amount", "mseg2Amount", "mseg3Amount"
};
static constexpr std::array<const char*, kMsegSlotCount> msegOffset {
    "mseg1Offset", "mseg2Offset", "mseg3Offset"
};
static constexpr std::array<const char*, kMsegSlotCount> msegRate {
    "mseg1Rate", "mseg2Rate", "mseg3Rate"
};
static constexpr std::array<const char*, kMsegSlotCount> msegRandom {
    "mseg1Random", "mseg2Random", "mseg3Random"
};
static constexpr std::array<const char*, kMsegSlotCount> msegLoop {
    "mseg1Loop", "mseg2Loop", "mseg3Loop"
};
static constexpr std::array<const char*, kMsegSlotCount> msegSync {
    "mseg1Sync", "mseg2Sync", "mseg3Sync"
};
} // namespace ParamIDs

[[nodiscard]] juce::StringArray plaitsModelNames();

class MacroOscIIAudioProcessor final : public juce::AudioProcessor,
                                     public juce::ChangeBroadcaster
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    MacroOscIIAudioProcessor();
    ~MacroOscIIAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Macro OSC II"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    [[nodiscard]] APVTS& getState() noexcept { return parameters; }
    [[nodiscard]] const APVTS& getState() const noexcept { return parameters; }

    [[nodiscard]] std::vector<MsegPoint> getMsegPoints (int slotIndex) const;
    [[nodiscard]] std::shared_ptr<const MsegShape> getMsegShape (int slotIndex) const;
    void setMsegPoints (int slotIndex, std::vector<MsegPoint> points, bool notifyEditor = true);
    [[nodiscard]] int getActiveVoiceCount() const noexcept { return activeVoiceCount.load (std::memory_order_relaxed); }
    [[nodiscard]] static constexpr int getMaxPolyphony() noexcept { return maxPolyphony; }

    // Every parameter to its default and every modulation shape to the default curve.
    void resetToDefaults();
    // Editor preferences, saved with the plug-in state.
    [[nodiscard]] float getEditorScale() const;
    void setEditorScale (float scale);
    [[nodiscard]] bool getTooltipsEnabled() const;
    void setTooltipsEnabled (bool enabled);

    // Presets are the plug-in state as XML, without the editor preferences.
    [[nodiscard]] static juce::File getPresetFolder();
    [[nodiscard]] juce::String getPresetName() const;
    bool savePreset (const juce::File& file);
    bool loadPreset (const juce::File& file);

    // Set by the standalone app only; plug-in hosts own the audio and MIDI setup.
    std::function<void()> showAudioSettings;

    static APVTS::ParameterLayout createParameterLayout();

private:
    static constexpr int maxPolyphony = 8;

    struct RawParameterPointers
    {
        std::atomic<float>* filterType {};
        std::atomic<float>* filterFrequency {};
        std::atomic<float>* filterQ {};
        std::atomic<float>* distortionType {};
        std::atomic<float>* distortionAmount {};
        std::atomic<float>* distortionMix {};
        std::atomic<float>* distortionOutput {};
        std::atomic<float>* model {};
        std::atomic<float>* harmonics {};
        std::atomic<float>* timbre {};
        std::atomic<float>* morph {};
        std::atomic<float>* auxMix {};
        std::atomic<float>* level {};
        std::atomic<float>* coarse {};
        std::atomic<float>* fine {};
        std::atomic<float>* portamento {};
        std::atomic<float>* voiceMode {};
        std::atomic<float>* voices {};
        std::atomic<float>* attack {};
        std::atomic<float>* decay {};
        std::atomic<float>* sustain {};
        std::atomic<float>* release {};
        std::array<std::atomic<float>*, kMsegSlotCount> msegDestination {};
        std::array<std::atomic<float>*, kMsegSlotCount> msegAmount {};
        std::array<std::atomic<float>*, kMsegSlotCount> msegOffset {};
        std::array<std::atomic<float>*, kMsegSlotCount> msegRate {};
        std::array<std::atomic<float>*, kMsegSlotCount> msegRandom {};
        std::array<std::atomic<float>*, kMsegSlotCount> msegLoop {};
        std::array<std::atomic<float>*, kMsegSlotCount> msegSync {};
    };

    PlaitsSynthesiser synth;
    APVTS parameters;
    RawParameterPointers rawParameters;
    std::array<PlaitsVoice*, maxPolyphony> plaitsVoices {};
    std::array<std::shared_ptr<const MsegShape>, kMsegSlotCount> currentMsegShapes;
    std::array<std::atomic<const MsegShape*>, kMsegSlotCount> currentAudioMsegShapes {};
    std::vector<std::shared_ptr<const MsegShape>> retainedMsegShapes;
    std::atomic<float> currentTempoBpm { 120.0f };
    std::atomic<int> activeVoiceCount {};
    std::array<float, 2> dcBlockerPreviousInput {};
    std::array<float, 2> dcBlockerPreviousOutput {};
    float dcBlockerCoefficient { 0.995f };
    OutputLimiter outputLimiter;

    void cacheRawParameterPointers();
    [[nodiscard]] juce::ValueTree createStateTree();
    void updateHostTempo() noexcept;
    void resetOutputDcBlocker() noexcept;
    void applyOutputDcBlocker (juce::AudioBuffer<float>& buffer) noexcept;
    [[nodiscard]] const MsegShape* getAudioMsegShape (int slotIndex) const noexcept;
    [[nodiscard]] PlaitsVoiceParameters buildVoiceParameters() const;
    [[nodiscard]] static float getFloatParameter (const std::atomic<float>* value) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroOscIIAudioProcessor)
};
} // namespace macro_osc_ii
