#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <optional>
#include <memory>
#include "Fx/DistortionCatalog.h"

namespace lodo::engine::fx { class DistortionKernelBank; }

/**
 * Shared native distortion stage for all built-in instruments.
 *
 * Named Amount/Output/Mix controls are supplied by the owning instrument.
 * Discrete algorithm changes are rendered in parallel and
 * crossfaded so changing a state cannot splice two unrelated waveforms at one
 * sample.
 */
class NativeDistortionProcessor
{
public:
    NativeDistortionProcessor();
    ~NativeDistortionProcessor();
    NativeDistortionProcessor (const NativeDistortionProcessor&);
    NativeDistortionProcessor& operator= (const NativeDistortionProcessor&);
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setType (int type, bool snapImmediately) noexcept;
    void setParameters (const lodo::engine::fx::DistortionSettings&) noexcept;
    void beginBlock (int frames) noexcept;
    void processStereo (float& left, float& right) noexcept;

    [[nodiscard]] bool isTransitioning() const noexcept { return transitionActive; }
    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] int currentType() const noexcept { return sourceType; }
    [[nodiscard]] int targetType() const noexcept { return destinationType; }

private:
    void processKind (int slot, int type, float& left, float& right) noexcept;
    [[nodiscard]] static float processSample (float input,
                                              int distortionType,
                                              float driveAmount) noexcept;

    double preparedSampleRate { 44100.0 };
    int sourceType {};
    int destinationType {};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> transition;
    bool transitionActive {};
    std::optional<int> queuedType;
    std::unique_ptr<lodo::engine::fx::DistortionKernelBank> bank;
    std::array<lodo::engine::fx::DistortionSettings, 2> settings;
    int sourceSlot {}, destinationSlot {}, framesRemaining {};
    std::array<bool, 2> needsSetup { true, true };
    std::optional<lodo::engine::fx::DistortionSettings> queuedSettings;
};
