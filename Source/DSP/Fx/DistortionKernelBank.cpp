#include "DistortionKernelBank.h"
#include "DistortionAmountProfiles.h"
#include <algorithm>
#include <cmath>
#include <type_traits>

namespace lodo::engine::fx
{
const std::array<DistortionKernelBank::Kernel, 15>& DistortionKernelBank::coldTemplates()
{
    static const std::array<Kernel, 15> templates {{
        lodo::airwindows::DriveDSP::Drive {},
        lodo::airwindows::FractureDSP::Fracture {},
        [] {
            lodo::airwindows::ToTape9DSP::ToTape9 tape;
            // The insert has no stereo motion: link native clipping noise and
            // dither choices so identical channels remain identical when hot.
            tape.linkStereoRandomness();
            return tape;
        }(),
        lodo::airwindows::IronOxideClassic2DSP::IronOxideClassic2 {},
        lodo::airwindows::Density3DSP::Density3 {},
        lodo::airwindows::Spiral2DSP::Spiral2 {},
        lodo::airwindows::Tube2DSP::Tube2 {},
        lodo::airwindows::MackityDSP::Mackity {},
        lodo::airwindows::Coils2DSP::Coils2 {},
        lodo::airwindows::FireAmpDSP::FireAmp {},
        lodo::airwindows::DrumSlamDSP::DrumSlam {},
        lodo::airwindows::BitGlitterDSP::BitGlitter {},
        lodo::airwindows::DeBezDSP::DeBez {},
        NativeReductionProcessor {}, NativeReductionProcessor {}
    }};
    return templates;
}
void DistortionKernelBank::prepare(double rate)
{
    sampleRate = rate;
    static_assert(std::is_trivially_copyable_v<Kernel>);
    static_cast<void>(coldTemplates());
    slots = {};
}
void DistortionKernelBank::load(int index, int type) noexcept
{
    auto& slot = slots[static_cast<size_t>(index)];
    slot.type = type;
    slot.kernel = type >= 5 ? coldTemplates()[static_cast<size_t>(type - 5)] : Kernel {};
    std::visit([this](auto& kernel)
    {
        using K = std::decay_t<decltype(kernel)>;
        if constexpr (!std::is_same_v<K, std::monostate> && !std::is_same_v<K, NativeReductionProcessor>)
            kernel.setSampleRate(sampleRate);
    }, slot.kernel);
}
void DistortionKernelBank::beginBlock(int index, int frames, const std::array<float, 3>& p) noexcept
{
    auto& slot = slots[static_cast<size_t>(index)];
    slot.parameters = p;
    slot.framesRemaining = frames;
    const auto profile = distortionAmountProfile(slot.type, p[0]);
    const auto& n = profile.native;
    switch (slot.type)
    {
        case 5: std::get<lodo::airwindows::DriveDSP::Drive>(slot.kernel).setParameters({n[0], n[1], n[2], n[3]}); break;
        case 6: std::get<lodo::airwindows::FractureDSP::Fracture>(slot.kernel).setParameters({n[0], n[1], n[2], n[3]}); break;
        case 7: std::get<lodo::airwindows::ToTape9DSP::ToTape9>(slot.kernel).setParameters(n); break;
        case 8: std::get<lodo::airwindows::IronOxideClassic2DSP::IronOxideClassic2>(slot.kernel).setParameters({n[0], n[1], n[2]}); break;
        case 9: std::get<lodo::airwindows::Density3DSP::Density3>(slot.kernel).setParameters({n[0], n[1], n[2], n[3]}); break;
        case 10: std::get<lodo::airwindows::Spiral2DSP::Spiral2>(slot.kernel).setParameters({n[0], n[1], n[2], n[3], n[4]}); break;
        case 11: std::get<lodo::airwindows::Tube2DSP::Tube2>(slot.kernel).setParameters({n[0], n[1]}); break;
        case 12: std::get<lodo::airwindows::MackityDSP::Mackity>(slot.kernel).setParameters({n[0], n[1]}); break;
        case 13:
        {
            auto& core = std::get<lodo::airwindows::Coils2DSP::Coils2>(slot.kernel);
            core.setParameters({n[0], n[1], n[2]});
            core.setSaturationStrength(profile.saturationStrength);
            break;
        }
        case 14: std::get<lodo::airwindows::FireAmpDSP::FireAmp>(slot.kernel).setParameters({n[0], n[1], n[2], n[3]}); break;
        case 15: std::get<lodo::airwindows::DrumSlamDSP::DrumSlam>(slot.kernel).setParameters({n[0], n[1], n[2]}); break;
        case 16: std::get<lodo::airwindows::BitGlitterDSP::BitGlitter>(slot.kernel).setParameters({n[0], n[1], n[2], n[3]}); break;
        case 17: std::get<lodo::airwindows::DeBezDSP::DeBez>(slot.kernel).setParameters({n[0], n[1], n[2]}); break;
        default: break;
    }
    slot.mix = p[1];
    slot.output = p[2] * 2.0f;
    slot.inputGain = profile.inputGain;
    slot.wetTrim = profile.wetTrim;
    slot.engagement = slot.type >= 18 ? 1.0f : profile.engagement;
    if (slot.type == 18) slot.reduction = std::pow(.01f, p[0]);
    if (slot.type == 19) slot.reduction = p[0];
    std::visit([frames](auto& kernel)
    {
        using K = std::decay_t<decltype(kernel)>;
        if constexpr (!std::is_same_v<K, std::monostate> && !std::is_same_v<K, NativeReductionProcessor>)
            kernel.beginBlock(frames);
    }, slot.kernel);
}
void DistortionKernelBank::processFrame(int index, float& left, float& right) noexcept
{
    auto& slot = slots[static_cast<size_t>(index)];
    --slot.framesRemaining;
    const float dryLeft = left, dryRight = right;
    left *= slot.inputGain;
    right *= slot.inputGain;
    std::visit([&](auto& kernel)
    {
        using K = std::decay_t<decltype(kernel)>;
        if constexpr (std::is_same_v<K, NativeReductionProcessor>)
        {
            if (slot.type == 18) kernel.processSampleRateReduction(left, right, slot.reduction);
            else kernel.processBitReduction(left, right, slot.reduction);
        }
        else if constexpr (!std::is_same_v<K, std::monostate>)
            kernel.processFrame(left, right);
    }, slot.kernel);
    // Native amp filters can overflow under abrupt endpoint automation with hot
    // input. Restore a prepared cold history rather than let NaN poison the voice
    // or mixer. This guard leaves ordinary native output and its dither intact.
    if (!std::isfinite(left) || !std::isfinite(right) || (slot.type == 14 && (std::abs(left) > 16.0f || std::abs(right) > 16.0f)))
    {
        const auto parameters = slot.parameters;
        const int remaining = std::max(1, slot.framesRemaining);
        load(index, slot.type);
        beginBlock(index, remaining, parameters);
        left = dryLeft * (1.0f - slot.mix);
        right = dryRight * (1.0f - slot.mix);
        return;
    }
    // Amount engagement colours the wet path. Output trims only the wet
    // contribution; a zero Output leaves the dry fraction untouched.
    const float wetLeft = slot.engagement == 1.0f ? left * slot.wetTrim
        : dryLeft + (left * slot.wetTrim - dryLeft) * slot.engagement;
    const float wetRight = slot.engagement == 1.0f ? right * slot.wetTrim
        : dryRight + (right * slot.wetTrim - dryRight) * slot.engagement;
    left = dryLeft * (1.0f - slot.mix) + wetLeft * slot.mix * slot.output;
    right = dryRight * (1.0f - slot.mix) + wetRight * slot.mix * slot.output;
}
}
