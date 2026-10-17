#pragma once

#include "NativeDistortionProcessor.h"
#include "NativeFilterProcessor.h"

namespace macro_osc_ii
{
struct VoiceEffectsParameters
{
    int filterType {}; // Off, low-pass, band-pass, high-pass.
    float filterFrequency { 16000.0f };
    float filterQ { 1.0f };
    int distortionType {};
    float distortionAmount { 0.25f };
    float distortionMix { 1.0f };
    float distortionOutput { 0.5f }; // Lodo's unity wet output is 50%.
};

// Per-voice Lodo inserts, in their original distortion -> filter order.
// All kernel allocation and cold initialization happens in prepare().
class VoiceEffects
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void beginBlock (const VoiceEffectsParameters&, int frames) noexcept;
    void process (float& left, float& right) noexcept;

private:
    NativeFilterProcessor filter;
    NativeDistortionProcessor distortion;
    std::array<juce::SmoothedValue<float>, 3> distortionControls;
    // Resonance raises the level at the cutoff by up to Q; this trims it back.
    juce::SmoothedValue<float> resonanceCompensation { 1.0f };
    bool snapParameters { true };
};
}
