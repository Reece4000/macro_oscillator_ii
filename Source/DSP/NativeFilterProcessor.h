#pragma once

#include "SamplerSvfFilter.h"

#include <array>
#include <optional>

/** Shared SVF stage with click-free enable and mode changes. */
class NativeFilterProcessor
{
public:
    void prepare (double sampleRate, double parameterSmoothingSeconds) noexcept;
    void reset() noexcept;
    void setParameters (bool enabled,
                        int type,
                        float frequencyHz,
                        float q,
                        float shelfControl,
                        bool snapImmediately) noexcept;
    void processStereo (float& left, float& right) noexcept;
    [[nodiscard]] bool isActive() const noexcept;

private:
    struct Identity
    {
        bool enabled {};
        int type {};

        [[nodiscard]] bool operator== (const Identity& other) const noexcept
        {
            return enabled == other.enabled && type == other.type;
        }

        [[nodiscard]] bool operator!= (const Identity& other) const noexcept
        {
            return ! (*this == other);
        }
    };

    struct Request
    {
        Identity identity;
        float frequencyHz {};
        float q {};
        float shelfControl {};
    };

    static constexpr double kIdentityTransitionSeconds = 0.008;
    std::array<SamplerSvfFilter, 2> paths;
    std::array<Identity, 2> identities;
    int sourcePath {};
    int destinationPath {};
    double preparedSampleRate { 44100.0 };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> transition;
    bool transitionActive {};
    std::optional<Request> queuedRequest;
};
