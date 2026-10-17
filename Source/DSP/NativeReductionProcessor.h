#pragma once

/** Shared bit-depth and sample-rate reduction for built-in instruments. */
class NativeReductionProcessor
{
public:
    void reset() noexcept;
    void prime (float left, float right) noexcept;
    void processBitReduction (float& left, float& right, float amount01) const noexcept;
    void processSampleRateReduction (float& left, float& right, float rateScale) noexcept;

private:
    float decimationPhase {};
    float heldLeft {};
    float heldRight {};
};
