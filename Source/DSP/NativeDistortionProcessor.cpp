#include "NativeDistortionProcessor.h"
#include "Fx/DistortionKernelBank.h"

#include <cmath>

namespace
{
constexpr double kTypeTransitionSeconds = 0.008;

[[nodiscard]] float clamp01 (float value) noexcept
{
    return juce::jlimit (0.0f, 1.0f, value);
}

[[nodiscard]] float normalisedAtanSample (float value) noexcept
{
    return std::atan (value) * (2.0f / juce::MathConstants<float>::pi);
}

[[nodiscard]] float foldbackSample (float value, float threshold) noexcept
{
    if (threshold <= 0.0f)
        return 0.0f;

    if (value > threshold || value < -threshold)
    {
        value = std::abs (std::abs (std::fmod (value - threshold, threshold * 4.0f))
                                    - (threshold * 2.0f))
            - threshold;
        return juce::jlimit (-1.0f, 1.0f, value);
    }

    return value;
}
} // namespace

NativeDistortionProcessor::NativeDistortionProcessor() = default;
NativeDistortionProcessor::~NativeDistortionProcessor() = default;
NativeDistortionProcessor::NativeDistortionProcessor (const NativeDistortionProcessor& other)
{
    if (other.bank != nullptr) bank = std::make_unique<lodo::engine::fx::DistortionKernelBank>();
    *this = other;
}
NativeDistortionProcessor& NativeDistortionProcessor::operator= (const NativeDistortionProcessor& other)
{
    if (this == &other) return *this;
    // Retired voices are prepared before playback, so this audio-thread history
    // copy cannot allocate, destroy a kernel, or share mutable stereo histories.
    jassert (other.bank == nullptr || bank != nullptr);
    if (bank != nullptr && other.bank != nullptr) *bank = *other.bank;
    preparedSampleRate = other.preparedSampleRate;
    sourceType = other.sourceType; destinationType = other.destinationType;
    transition = other.transition; transitionActive = other.transitionActive;
    queuedType = other.queuedType; settings = other.settings;
    sourceSlot = other.sourceSlot; destinationSlot = other.destinationSlot;
    framesRemaining = other.framesRemaining;
    needsSetup = other.needsSetup; queuedSettings = other.queuedSettings;
    return *this;
}
void NativeDistortionProcessor::setParameters (const lodo::engine::fx::DistortionSettings& value) noexcept
{
    if (queuedType.has_value()) { queuedSettings = value; return; }
    auto& current = settings[static_cast<size_t>(destinationSlot)];
    if (current.parameters != value.parameters)
        needsSetup[static_cast<size_t>(destinationSlot)] = true;
    current = value;
}
void NativeDistortionProcessor::beginBlock (int frames) noexcept
{
    framesRemaining = juce::jmax (1, frames);
    if (bank == nullptr) return;
    bank->beginBlock (sourceSlot, framesRemaining, settings[static_cast<size_t> (sourceSlot)].parameters);
    needsSetup[static_cast<size_t> (sourceSlot)] = false;
    if (transitionActive)
    {
        bank->beginBlock (destinationSlot, framesRemaining, settings[static_cast<size_t> (destinationSlot)].parameters);
        needsSetup[static_cast<size_t> (destinationSlot)] = false;
    }
}
void NativeDistortionProcessor::processKind (int slot, int type, float& left, float& right) noexcept
{
    if (type == 0) return;
    const auto& value = settings[static_cast<size_t> (slot)];
    if (type >= 5)
    {
        if (bank != nullptr)
        {
            if (needsSetup[static_cast<size_t> (slot)])
            {
                bank->beginBlock (slot, juce::jmax (1, framesRemaining), value.parameters);
                needsSetup[static_cast<size_t> (slot)] = false;
            }
            bank->processFrame (slot, left, right);
        }
        return;
    }
    const float dryLeft = left, dryRight = right;
    left = processSample(left, type, value.parameters[0]);
    right = processSample(right, type, value.parameters[0]);
    const float mix = value.parameters[1], output = value.parameters[2] * 2.0f;
    left = dryLeft * (1.0f - mix) + left * mix * output;
    right = dryRight * (1.0f - mix) + right * mix * output;
}

void NativeDistortionProcessor::prepare (double sampleRate) noexcept
{
    preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    if (bank == nullptr) bank = std::make_unique<lodo::engine::fx::DistortionKernelBank>();
    sourceType = destinationType;
    sourceSlot = destinationSlot;
    bank->prepare (preparedSampleRate);
    bank->load (sourceSlot, sourceType);
    if (destinationSlot != sourceSlot) bank->load (destinationSlot, destinationType);
    transition.reset (preparedSampleRate, kTypeTransitionSeconds);
    transition.setCurrentAndTargetValue (1.0f);
    transitionActive = false;
    queuedType.reset();
    queuedSettings.reset();
}

void NativeDistortionProcessor::reset() noexcept
{
    if (queuedType.has_value())
    {
        destinationType = *queuedType;
        settings[static_cast<size_t> (destinationSlot)] = queuedSettings.value_or (
            settings[static_cast<size_t> (destinationSlot)]);
    }
    sourceType = destinationType;
    sourceSlot = destinationSlot;
    if (bank != nullptr) bank->load (sourceSlot, sourceType);
    needsSetup[static_cast<size_t> (sourceSlot)] = true;
    transition.setCurrentAndTargetValue (1.0f);
    transitionActive = false;
    queuedType.reset();
    queuedSettings.reset();
}

void NativeDistortionProcessor::setType (int type, bool snapImmediately) noexcept
{
    const int clampedType = lodo::engine::fx::clampDistortionType(type);
    const auto currentSettings = queuedSettings.value_or(settings[static_cast<size_t>(destinationSlot)]);
    if (clampedType == destinationType)
    {
        if (transitionActive)
            queuedType.reset();
        queuedSettings.reset();
        setParameters(currentSettings);
        return;
    }

    if (snapImmediately)
    {
        destinationSlot = sourceSlot;
        settings[static_cast<size_t>(destinationSlot)] = currentSettings;
        if (bank != nullptr) bank->load (sourceSlot, clampedType);
        needsSetup[static_cast<size_t> (sourceSlot)] = true;
        sourceType = clampedType;
        destinationType = clampedType;
        transition.setCurrentAndTargetValue (1.0f);
        transitionActive = false;
        queuedType.reset();
        queuedSettings.reset();
        return;
    }

    if (transitionActive)
    {
        queuedType = clampedType;
        return;
    }

    destinationSlot = 1 - sourceSlot;
    settings[static_cast<size_t>(destinationSlot)] = currentSettings;
    if (bank != nullptr)
    {
        bank->load (destinationSlot, clampedType);
        needsSetup[static_cast<size_t> (destinationSlot)] = true;
    }
    destinationType = clampedType;
    if (sourceType == destinationType)
    {
        transition.setCurrentAndTargetValue (1.0f);
        transitionActive = false;
        return;
    }

    transition.reset (preparedSampleRate, kTypeTransitionSeconds);
    transition.setCurrentAndTargetValue (0.0f);
    transition.setTargetValue (1.0f);
    transitionActive = true;
}

void NativeDistortionProcessor::processStereo (float& left,
                                               float& right) noexcept
{
    if (! transitionActive)
    {
        processKind (destinationSlot, destinationType, left, right);
        --framesRemaining;
        return;
    }

    const float mix = transition.getNextValue();
    float sourceLeft = left, sourceRight = right, destinationLeft = left, destinationRight = right;
    processKind (sourceSlot, sourceType, sourceLeft, sourceRight);
    processKind (destinationSlot, destinationType, destinationLeft, destinationRight);
    --framesRemaining;
    left = sourceLeft + ((destinationLeft - sourceLeft) * mix);
    right = sourceRight + ((destinationRight - sourceRight) * mix);

    if (! transition.isSmoothing())
    {
        sourceType = destinationType;
        sourceSlot = destinationSlot;
        transition.setCurrentAndTargetValue (1.0f);
        transitionActive = false;
        if (queuedType.has_value())
        {
            const int nextType = *queuedType;
            queuedType.reset();
            const auto nextSettings = queuedSettings;
            queuedSettings.reset();
            setType (nextType, false);
            if (nextSettings.has_value()) setParameters (*nextSettings);
        }
    }
}

bool NativeDistortionProcessor::isActive() const noexcept
{
    return transitionActive || destinationType != 0;
}

float NativeDistortionProcessor::processSample (float input,
                                                int distortionType,
                                                float driveAmount) noexcept
{
    const float amount = clamp01 (driveAmount);
    if (distortionType == 0 || amount <= 1.0e-5f)
        return input;

    const float wet = 0.5f + (amount * 0.5f);
    const float bias = amount * 0.12f;
    const float postTrim = 0.9f - (amount * 0.18f);
    float shaped = input;

    switch (distortionType)
    {
        case 1:
        {
            const float driveGain = 1.0f + (amount * 10.0f);
            const float dc = normalisedAtanSample (bias * driveGain);
            shaped = normalisedAtanSample ((input + bias) * driveGain) - dc;
            break;
        }
        case 2:
        {
            const float driveGain = 1.0f + (amount * 14.0f);
            const float threshold = juce::jmap (amount, 0.0f, 1.0f, 0.9f, 0.18f);
            const float dc = juce::jlimit (-threshold, threshold, bias * driveGain);
            shaped = (juce::jlimit (-threshold, threshold, (input + bias) * driveGain) - dc)
                / juce::jmax (0.18f, threshold);
            break;
        }
        case 3:
        {
            const float driveGain = 1.0f + (amount * 11.0f);
            const float threshold = juce::jmap (amount, 0.0f, 1.0f, 0.82f, 0.12f);
            const float dc = foldbackSample (bias * driveGain, threshold);
            shaped = (foldbackSample ((input + bias) * driveGain, threshold) - dc)
                / juce::jmax (0.15f, threshold);
            break;
        }
        case 4:
        {
            const float driveGain = 1.0f + (amount * 8.0f);
            const float wrapAmount = 1.0f + (amount * 2.5f);
            const float dc = std::sin (bias * driveGain * wrapAmount);
            shaped = std::sin ((input + bias) * driveGain * wrapAmount) - dc;
            break;
        }
        default:
            return input;
    }

    return juce::jlimit (-1.0f, 1.0f, juce::jmap (wet, input, shaped * postTrim));
}
