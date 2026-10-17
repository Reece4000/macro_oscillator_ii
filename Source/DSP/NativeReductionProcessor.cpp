#include "NativeReductionProcessor.h"

#include <algorithm>

#include <cmath>

void NativeReductionProcessor::reset() noexcept
{
    decimationPhase = 0.0f;
    heldLeft = 0.0f;
    heldRight = 0.0f;
}

void NativeReductionProcessor::prime (float left, float right) noexcept
{
    decimationPhase = 0.0f;
    heldLeft = left;
    heldRight = right;
}

void NativeReductionProcessor::processBitReduction (float& left,
                                                     float& right,
                                                     float amount01) const noexcept
{
    const float reduction = std::clamp (amount01, 0.0f, 1.0f);
    if (reduction <= 1.0e-5f)
        return;

    const float bitDepth = 24.0f - (reduction * 23.0f);
    const float levels = std::max (1.0f, std::exp2 (bitDepth) - 1.0f);
    left = std::round (left * levels) / levels;
    right = std::round (right * levels) / levels;
}

void NativeReductionProcessor::processSampleRateReduction (float& left,
                                                            float& right,
                                                            float rateScale) noexcept
{
    const float scale = std::clamp (rateScale, 0.01f, 1.0f);
    if (scale >= 0.9999f)
    {
        heldLeft = left;
        heldRight = right;
        decimationPhase = 0.0f;
        return;
    }

    if (left == 0.0f && right == 0.0f)
    {
        reset();
        return;
    }

    decimationPhase += scale;
    if (decimationPhase >= 1.0f)
    {
        decimationPhase -= 1.0f;
        heldLeft = left;
        heldRight = right;
    }

    left = heldLeft;
    right = heldRight;
}
