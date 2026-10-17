#pragma once

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

class SamplerSvfFilter
{
public:
    SamplerSvfFilter() noexcept;

    void prepare (double sampleRate, double smoothingSeconds) noexcept;
    void reset() noexcept;
    void setParameters (bool enabled, int type, float frequencyHz, float q, float shelfControl, bool immediate) noexcept;
    void processStereo (float& left, float& right) noexcept;
    [[nodiscard]] bool isActive() const noexcept;

private:
    struct ChannelState
    {
        float ic1eq = 0.0f;
        float ic2eq = 0.0f;

        void reset() noexcept
        {
            ic1eq = 0.0f;
            ic2eq = 0.0f;
        }
    };

    struct Coefficients
    {
        float k = 1.41421356f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float a3 = 0.0f;
    };

    static constexpr size_t kCoefficientCount = 4;
    using CoefficientSmootherArray = std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, kCoefficientCount>;

    enum class Mode : int
    {
        bypass = -1,
        lowPass = 0,
        bandPass = 1,
        highPass = 2
    };

    [[nodiscard]] float clampFrequency (float hz) const noexcept;
    [[nodiscard]] static float resolveQ (float displayQ) noexcept;
    [[nodiscard]] static Mode resolveMode (bool enabled, int type) noexcept;
    [[nodiscard]] Coefficients makeCoefficients (float frequencyHz, float q) const noexcept;
    [[nodiscard]] float processChannel (ChannelState& state, float input) const noexcept;
    void setMode (Mode mode, bool immediate) noexcept;
    void updateCoefficients (float frequencyHz, float q, bool immediate) noexcept;
    void resetCoefficientSmoothers (const Coefficients& values) noexcept;
    void setCoefficientTargets (const Coefficients& values, bool immediate) noexcept;
    [[nodiscard]] bool coefficientsAreSmoothing() const noexcept;
    void advanceSmoothedCoefficients() noexcept;

    double sampleRateHz = 44100.0;
    double smoothingSeconds = 0.01;
    std::array<ChannelState, 2> channels {};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> frequencyHzSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> qSmoothed;
    Mode currentMode = Mode::bypass;
    Mode targetMode = Mode::bypass;
    Coefficients coefficients {};
    CoefficientSmootherArray coefficientSmoothers {};
    float processorFrequencyHz = -1.0f;
    float processorQ = -1.0f;
};
