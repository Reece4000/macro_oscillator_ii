/* ========================================
 *  Spiral2 - Spiral2.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::Spiral2DSP
{



struct History
{


    double iirSampleAL;
    double iirSampleBL;
    double prevSampleL;
    uint32_t fpdL;
    uint32_t fpdR;

    double iirSampleAR;
    double iirSampleBR;
    double prevSampleR;
    bool flip;
    //default stuff


};
class Spiral2 : private History
{
public:
    static constexpr int controlCount = 5;
    Spiral2() { reset(); }
    void reset() noexcept;
    void setSampleRate(double value) noexcept { sampleRate = value; }
    void setParameters(const std::array<float, controlCount>& values) noexcept { A = values[0]; B = values[1]; C = values[2]; D = values[3]; E = values[4]; }
    void beginBlock(int frames) noexcept;
    void processFrame(float& left, float& right) noexcept;
    void process(float** inputs, float** outputs, int frames) noexcept;
private:
    struct Block
    {
    double overallscale {};
    double gain {};
    double iirAmount {};
    double presence {};
    double output {};
    double wet {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.5f };
    float B { 0.0f };
    float C { 0.5f };
    float D { 1.0f };
    float E { 1.0f };
    double sampleRate { 44100.0 };
};
}
