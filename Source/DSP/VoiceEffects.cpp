#include "VoiceEffects.h"

#include <cmath>

namespace macro_osc_ii
{
void VoiceEffects::prepare (double sampleRate)
{
    filter.prepare (sampleRate, 0.01);
    distortion.prepare (sampleRate);
    for (auto& control : distortionControls)
        control.reset (sampleRate, 0.01);
    resonanceCompensation.reset (sampleRate, 0.01);
    reset();
}

void VoiceEffects::reset() noexcept
{
    filter.reset();
    distortion.reset();
    snapParameters = true;
}

void VoiceEffects::beginBlock (const VoiceEffectsParameters& parameters, int frames) noexcept
{
    const int filterType = juce::jlimit (0, 3, parameters.filterType);
    const float q = juce::jlimit (1.0f, 30.0f, parameters.filterQ);
    filter.setParameters (filterType != 0, juce::jmax (0, filterType - 1),
                          juce::jlimit (20.0f, 20000.0f, parameters.filterFrequency),
                          q, 0.5f, snapParameters);
    // About -1.8 dB at Q 3 and -8.6 dB at Q 26: resonance still sings, but a
    // harmonic landing on the cutoff no longer jumps 20 dB or more.
    const float compensation = filterType != 0 ? 1.0f / std::sqrt (1.0f + 0.25f * (q - 1.0f)) : 1.0f;
    if (snapParameters)
        resonanceCompensation.setCurrentAndTargetValue (compensation);
    else
        resonanceCompensation.setTargetValue (compensation);
    distortion.setType (parameters.distortionType, snapParameters);
    lodo::engine::fx::DistortionSettings settings {
        { parameters.distortionAmount, parameters.distortionMix, parameters.distortionOutput }
    };
    for (size_t i = 0; i < distortionControls.size(); ++i)
    {
        const float target = juce::jlimit (0.0f, 1.0f, settings.parameters[i]);
        if (snapParameters)
            distortionControls[i].setCurrentAndTargetValue (target);
        else
            distortionControls[i].setTargetValue (target);
        settings.parameters[i] = distortionControls[i].skip (frames);
    }
    distortion.setParameters (settings);
    distortion.beginBlock (frames);
    snapParameters = false;
}

void VoiceEffects::process (float& left, float& right) noexcept
{
    distortion.processStereo (left, right);
    filter.processStereo (left, right);
    const float compensation = resonanceCompensation.getNextValue();
    left *= compensation;
    right *= compensation;
    if (! std::isfinite (left) || ! std::isfinite (right))
    {
        left = right = 0.0f;
        reset();
    }
    // Match Lodo's native instrument safety headroom.
    left = juce::jlimit (-4.0f, 4.0f, left);
    right = juce::jlimit (-4.0f, 4.0f, right);
}
}
