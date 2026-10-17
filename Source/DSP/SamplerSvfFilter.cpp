#include "SamplerSvfFilter.h"

#include <cmath>

SamplerSvfFilter::SamplerSvfFilter() noexcept
{
    frequencyHzSmoothed.setCurrentAndTargetValue (16000.0f);
    qSmoothed.setCurrentAndTargetValue (1.0f);
    resetCoefficientSmoothers (coefficients);
}

void SamplerSvfFilter::prepare (double sampleRate, double newSmoothingSeconds) noexcept
{
    sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    smoothingSeconds = juce::jmax (0.0, newSmoothingSeconds);

    const auto currentFrequency = clampFrequency (frequencyHzSmoothed.getCurrentValue());
    const auto currentQ = resolveQ (qSmoothed.getCurrentValue());

    frequencyHzSmoothed.reset (sampleRateHz, smoothingSeconds);
    frequencyHzSmoothed.setCurrentAndTargetValue (currentFrequency);

    qSmoothed.reset (sampleRateHz, smoothingSeconds);
    qSmoothed.setCurrentAndTargetValue (currentQ);

    reset();
    processorFrequencyHz = -1.0f;
    processorQ = -1.0f;
    coefficients = makeCoefficients (currentFrequency, currentQ);
    resetCoefficientSmoothers (coefficients);
    processorFrequencyHz = currentFrequency;
    processorQ = currentQ;
}

void SamplerSvfFilter::reset() noexcept
{
    for (auto& channel : channels)
        channel.reset();
}

void SamplerSvfFilter::setParameters (bool enabled,
                                      int type,
                                      float frequencyHz,
                                      float q,
                                      float shelfControl,
                                      bool immediate) noexcept
{
    const auto newMode = resolveMode (enabled, type);
    const auto newFrequency = clampFrequency (frequencyHz);
    const auto newQ = resolveQ (q);
    juce::ignoreUnused (shelfControl);

    frequencyHzSmoothed.setCurrentAndTargetValue (newFrequency);
    qSmoothed.setCurrentAndTargetValue (newQ);

    setMode (newMode, immediate);

    updateCoefficients (newFrequency, newQ, immediate);
}

void SamplerSvfFilter::processStereo (float& left, float& right) noexcept
{
    if (coefficientsAreSmoothing())
        advanceSmoothedCoefficients();

    currentMode = targetMode;
    if (currentMode == Mode::bypass)
        return;

    if (processorFrequencyHz < 0.0f || processorQ < 0.0f)
        updateCoefficients (frequencyHzSmoothed.getCurrentValue(), qSmoothed.getCurrentValue(), false);

    left = processChannel (channels[0], left);
    right = processChannel (channels[1], right);
}

bool SamplerSvfFilter::isActive() const noexcept
{
    return currentMode != Mode::bypass || targetMode != Mode::bypass
        || coefficientsAreSmoothing();
}

float SamplerSvfFilter::clampFrequency (float hz) const noexcept
{
    const auto nyquist = static_cast<float> (sampleRateHz * 0.499);
    return juce::jlimit (20.0f, nyquist, hz);
}

float SamplerSvfFilter::resolveQ (float displayQ) noexcept
{
    return juce::jlimit (0.1f, 30.0f, displayQ);
}

SamplerSvfFilter::Mode SamplerSvfFilter::resolveMode (bool enabled, int type) noexcept
{
    if (! enabled)
        return Mode::bypass;

    switch (juce::jlimit (0, 2, type))
    {
        case 0: return Mode::lowPass;
        case 1: return Mode::bandPass;
        case 2: return Mode::highPass;
        default:
            return Mode::lowPass;
    }
}

SamplerSvfFilter::Coefficients SamplerSvfFilter::makeCoefficients (float frequencyHz, float q) const noexcept
{
    const auto g = std::tan (juce::MathConstants<float>::pi * frequencyHz / static_cast<float> (sampleRateHz));
    const auto k = juce::jlimit (1.0e-3f, 10.0f, 1.0f / q);
    const auto a1 = 1.0f / (1.0f + (g * (g + k)));
    const auto a2 = g * a1;

    return { k, a1, a2, g * a2 };
}

float SamplerSvfFilter::processChannel (ChannelState& state, float input) const noexcept
{
    const auto v3 = input - state.ic2eq;
    const auto band = (coefficients.a1 * state.ic1eq) + (coefficients.a2 * v3);
    const auto low = state.ic2eq + (coefficients.a2 * state.ic1eq) + (coefficients.a3 * v3);
    const auto high = input - (coefficients.k * band) - low;

    state.ic1eq = (2.0f * band) - state.ic1eq;
    state.ic2eq = (2.0f * low) - state.ic2eq;

    switch (currentMode)
    {
        case Mode::bandPass:
            return band;
        case Mode::highPass:
            return high;
        case Mode::lowPass:
            return low;
        default:
            return input;
    }
}

void SamplerSvfFilter::setMode (Mode mode, bool immediate) noexcept
{
    juce::ignoreUnused (immediate);

    const bool modeChanged = targetMode != mode;
    currentMode = mode;
    targetMode = mode;
    processorFrequencyHz = -1.0f;
    processorQ = -1.0f;

    if (modeChanged)
        reset();
}

void SamplerSvfFilter::updateCoefficients (float frequencyHz, float q, bool immediate) noexcept
{
    if (processorFrequencyHz == frequencyHz && processorQ == q)
    {
        if (immediate && coefficientsAreSmoothing())
            setCoefficientTargets (makeCoefficients (frequencyHz, q), true);
        return;
    }

    processorFrequencyHz = frequencyHz;
    processorQ = q;
    setCoefficientTargets (makeCoefficients (frequencyHz, q), immediate);
}

void SamplerSvfFilter::resetCoefficientSmoothers (const Coefficients& values) noexcept
{
    for (auto& smoother : coefficientSmoothers)
        smoother.reset (sampleRateHz, smoothingSeconds);

    coefficientSmoothers[0].setCurrentAndTargetValue (values.k);
    coefficientSmoothers[1].setCurrentAndTargetValue (values.a1);
    coefficientSmoothers[2].setCurrentAndTargetValue (values.a2);
    coefficientSmoothers[3].setCurrentAndTargetValue (values.a3);
}

void SamplerSvfFilter::setCoefficientTargets (const Coefficients& values, bool immediate) noexcept
{
    if (immediate)
    {
        coefficients = values;
        coefficientSmoothers[0].setCurrentAndTargetValue (values.k);
        coefficientSmoothers[1].setCurrentAndTargetValue (values.a1);
        coefficientSmoothers[2].setCurrentAndTargetValue (values.a2);
        coefficientSmoothers[3].setCurrentAndTargetValue (values.a3);
        return;
    }

    coefficientSmoothers[0].setTargetValue (values.k);
    coefficientSmoothers[1].setTargetValue (values.a1);
    coefficientSmoothers[2].setTargetValue (values.a2);
    coefficientSmoothers[3].setTargetValue (values.a3);
}

bool SamplerSvfFilter::coefficientsAreSmoothing() const noexcept
{
    for (const auto& smoother : coefficientSmoothers)
        if (smoother.isSmoothing())
            return true;

    return false;
}

void SamplerSvfFilter::advanceSmoothedCoefficients() noexcept
{
    coefficients.k = coefficientSmoothers[0].getNextValue();
    coefficients.a1 = coefficientSmoothers[1].getNextValue();
    coefficients.a2 = coefficientSmoothers[2].getNextValue();
    coefficients.a3 = coefficientSmoothers[3].getNextValue();
}
