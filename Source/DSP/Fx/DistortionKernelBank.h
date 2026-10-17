#pragma once
#include "DistortionCatalog.h"
#include "../NativeReductionProcessor.h"
#include <variant>
#include <array>
#include "../../ThirdParty/Airwindows/Drive.h"
#include "../../ThirdParty/Airwindows/Fracture.h"
#include "../../ThirdParty/Airwindows/ToTape9.h"
#include "../../ThirdParty/Airwindows/IronOxideClassic2.h"
#include "../../ThirdParty/Airwindows/Density3.h"
#include "../../ThirdParty/Airwindows/Spiral2.h"
#include "../../ThirdParty/Airwindows/Tube2.h"
#include "../../ThirdParty/Airwindows/Mackity.h"
#include "../../ThirdParty/Airwindows/Coils2.h"
#include "../../ThirdParty/Airwindows/FireAmp.h"
#include "../../ThirdParty/Airwindows/DrumSlam.h"
#include "../../ThirdParty/Airwindows/BitGlitter.h"
#include "../../ThirdParty/Airwindows/DeBez.h"

namespace lodo::engine::fx
{
// Two maximal, preallocated histories; no per-kind heap objects in a voice.
// Cold templates are initialized by prepare(), before audio starts. Loading a
// type copies its initialized POD state, not its constructor/table setup.
class DistortionKernelBank
{
public:
    void prepare(double sampleRate);
    void load(int slot, int type) noexcept;
    void beginBlock(int slot, int frames, const std::array<float, 3>& values) noexcept;
    void processFrame(int slot, float& left, float& right) noexcept;
private:
    using Kernel = std::variant<std::monostate,
        lodo::airwindows::DriveDSP::Drive,
        lodo::airwindows::FractureDSP::Fracture,
        lodo::airwindows::ToTape9DSP::ToTape9,
        lodo::airwindows::IronOxideClassic2DSP::IronOxideClassic2,
        lodo::airwindows::Density3DSP::Density3,
        lodo::airwindows::Spiral2DSP::Spiral2,
        lodo::airwindows::Tube2DSP::Tube2,
        lodo::airwindows::MackityDSP::Mackity,
        lodo::airwindows::Coils2DSP::Coils2,
        lodo::airwindows::FireAmpDSP::FireAmp,
        lodo::airwindows::DrumSlamDSP::DrumSlam,
        lodo::airwindows::BitGlitterDSP::BitGlitter,
        lodo::airwindows::DeBezDSP::DeBez,
        NativeReductionProcessor>;
    static const std::array<Kernel, 15>& coldTemplates();
    struct Slot
    {
        Kernel kernel;
        int type {};
        float mix { 1.0f }, output { 1.0f }, reduction { 1.0f };
        std::array<float, 3> parameters {};
        int framesRemaining {};
        float inputGain {1.0f}, wetTrim {1.0f}, engagement {1.0f};
    };
    std::array<Slot, 2> slots;
    double sampleRate { 44100.0 };
};
}
