/* ========================================
 *  Tube2 - Tube2.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::Tube2DSP
{



struct History
{


    double previousSampleA;
    double previousSampleB;
    double previousSampleC;
    double previousSampleD;
    double previousSampleE;
    double previousSampleF;

    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff

};
class Tube2 : private History
{
public:
    static constexpr int controlCount = 2;
    Tube2() { reset(); }
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
    double inputPad {};
    double iterations {};
    int powerfactor {};
    double asymPad {};
    double gainscaling {};
    double outputscaling {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.5f };
    float B { 0.5f };
    double sampleRate { 44100.0 };
};
}
