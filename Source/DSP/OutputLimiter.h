#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace macro_osc_ii
{
// Output safety: a stereo-linked peak limiter without lookahead, then a soft
// clipper that catches whatever the limiter's attack lets through. Below the
// threshold both stages pass audio unchanged, so normal levels are untouched.
class OutputLimiter
{
public:
    static constexpr float threshold = 0.89f; // -1 dBFS: the limiter starts here
    static constexpr float knee = 0.9f;       // the clipper is linear below this
    static constexpr float ceiling = 1.0f;    // and never exceeds this

    void prepare (double sampleRate)
    {
        attackCoefficient = static_cast<float> (std::exp (-1.0 / (0.001 * sampleRate)));
        releaseCoefficient = static_cast<float> (std::exp (-1.0 / (0.15 * sampleRate)));
        reset();
    }

    void reset() noexcept
    {
        envelope = 0.0f;
        gain = 1.0f;
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        if (channels == 0)
            return;
        auto* left = buffer.getWritePointer (0);
        auto* right = channels > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float peak = juce::jmax (std::abs (left[i]), right != nullptr ? std::abs (right[i]) : 0.0f);
            envelope = juce::jmax (peak, envelope * releaseCoefficient);
            const float target = envelope > threshold ? threshold / envelope : 1.0f;
            // Fast to pull down, slow to recover (the envelope sets the release).
            gain = target < gain ? target + (gain - target) * attackCoefficient : target;
            left[i] = softClip (left[i] * gain);
            if (right != nullptr)
                right[i] = softClip (right[i] * gain);
        }
    }

    [[nodiscard]] static float softClip (float x) noexcept
    {
        const float magnitude = std::abs (x);
        if (magnitude <= knee)
            return x;
        const float headroom = ceiling - knee;
        return std::copysign (knee + headroom * std::tanh ((magnitude - knee) / headroom), x);
    }

private:
    float attackCoefficient { 0.97f };
    float releaseCoefficient { 0.9999f };
    float envelope {};
    float gain { 1.0f };
};
} // namespace macro_osc_ii
