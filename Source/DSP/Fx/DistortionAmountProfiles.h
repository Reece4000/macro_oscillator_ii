#pragma once
#include <array>

namespace lodo::engine::fx
{
// Control mappings: sample processing stays in the imported kernels, with the
// documented Core transformer-residual strength extension above 25% Amount.
// Native output trims compensate some drive gain; user Output is a separate
// wet-path trim and never changes the algorithm's internal feedback/voicing.
struct DistortionAmountProfile
{
    std::array<float, 9> native {};
    float engagement {};
    float inputGain {1.0f}, wetTrim {1.0f};
    float saturationStrength {1.0f};
};
[[nodiscard]] DistortionAmountProfile distortionAmountProfile(int type, float amount) noexcept;
}
