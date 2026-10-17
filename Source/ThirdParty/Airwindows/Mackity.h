/* ========================================
 *  Mackity - Mackity.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::MackityDSP
{



struct History
{



    double iirSampleAL;
    double iirSampleBL;
    double iirSampleAR;
    double iirSampleBR;
    double biquadA[15];
    double biquadB[15];

    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff

};
class Mackity : private History
{
public:
    static constexpr int controlCount = 2;
    Mackity() { reset(); }
    void reset() noexcept;
    void setSampleRate(double value) noexcept { sampleRate = value; }
    void setParameters(const std::array<float, controlCount>& values) noexcept { A = values[0]; B = values[1]; }
    void beginBlock(int frames) noexcept;
    void processFrame(float& left, float& right) noexcept;
    void process(float** inputs, float** outputs, int frames) noexcept;
private:
    struct Block
    {
    double overallscale {};
    double inTrim {};
    double outPad {};
    double iirAmountA {};
    double iirAmountB {};
    double K {};
    double norm {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.1f };
    float B { 1.0f };
    double sampleRate { 44100.0 };
};
}
