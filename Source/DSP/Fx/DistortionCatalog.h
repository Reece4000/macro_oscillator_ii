#pragma once
#include <array>
#include <string_view>

namespace lodo::engine::fx
{
struct DistortionDescriptor
{
    std::string_view id, name;
    std::array<std::string_view, 3> labels;
    std::array<float, 3> defaults;
};
inline constexpr std::array<std::string_view, 20> distortionIds {{
    "off", "lodo.soft-clip.v2", "lodo.hard-clip.v2", "lodo.foldback.v2", "lodo.sine.v2",
    "airwindows.drive.amount.v2", "airwindows.fracture.amount.v2", "airwindows.totape9.degradation.v2",
    "airwindows.ironoxideclassic2.amount.v2", "airwindows.density3.amount.v2", "airwindows.spiral2.amount.v2",
    "airwindows.tube2.amount.v2", "airwindows.mackity.amount.v2", "airwindows.coils2.amount.v2",
    "airwindows.fireamp.amount.v2", "airwindows.drumslam.amount.v2", "airwindows.bitglitter.amount.v2",
    "airwindows.debez.amount.v2", "lodo.sample-rate.amount.v2", "lodo.bitcrush.amount.v2"
}};

inline constexpr std::array<std::string_view, 20> distortionNames {{
    "Off", "Soft", "Hard", "Fold", "Sine", "Heat", "Shred", "Tape", "Oxide", "Thick",
    "Twist", "Valve", "Crunch", "Core", "Stack", "Punch", "Grit", "Crush", "Rate", "Bitcrush"
}};
inline constexpr auto distortionCatalog = []
{
    std::array<DistortionDescriptor, 20> result {};
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = {distortionIds[i], distortionNames[i],
            i == 0 ? std::array<std::string_view, 3>{"—", "—", "—"}
                   : std::array<std::string_view, 3>{"Amount", "Mix", "Output"},
            {i == 0 || i >= 18 ? 0.0f : 0.25f, 1.0f, 0.5f}};
    return result;
}();
// Bypass, clipping, analog saturation, tape, amp/drum, folding, digital.
inline constexpr std::array<int, 20> distortionSelectionOrder {{
    0, 1, 2, 11, 13, 9, 10, 5, 12, 7, 8, 15, 14, 3, 4, 6, 18, 19, 17, 16
}};
inline constexpr std::array<std::string_view, 3> distortionControlNames {{
    "distortion_amount", "distortion_mix", "distortion_output"
}};
inline constexpr int distortionParameterIndex(std::string_view name) noexcept
{
    for (int i = 0; i < 3; ++i)
        if (distortionControlNames[static_cast<size_t>(i)] == name) return i;
    return -1;
}
inline constexpr int maxDistortionType = static_cast<int>(distortionCatalog.size()) - 1;
inline constexpr int clampDistortionType(int type) noexcept
{
    return type < 0 ? 0 : (type > maxDistortionType ? maxDistortionType : type);
}
inline constexpr int distortionTypeForId(std::string_view id) noexcept
{
    for (int i = 0; i <= maxDistortionType; ++i)
        if (distortionCatalog[static_cast<size_t>(i)].id == id) return i;
    return 0;
}
struct DistortionSettings
{
    std::array<float, 3> parameters {0.0f, 1.0f, 0.5f};
};
}
