/* ========================================
 *  Coils2 - Coils2.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace lodo::airwindows::Coils2DSP
{



struct History
{


    double biquadA[15];
    double biquadB[15];
    double hysteresisL;
    double hysteresisR;
    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff

};
class Coils2 : private History
{
public:
    static constexpr int controlCount = 3;
    Coils2() { reset(); }
    void reset() noexcept;
    void setSampleRate(double value) noexcept { sampleRate = value; }
    void setParameters(const std::array<float, controlCount>& values) noexcept { A = values[0]; B = values[1]; C = values[2]; }
    // Lodo's driven Core profile emphasizes the native transformer residual.
    // Unity retains the original algorithm for standalone/native use.
    void setSaturationStrength(float value) noexcept { saturationStrength = std::clamp(value, 1.0f, 12.0f); }
    void beginBlock(int frames) noexcept;
    void processFrame(float& left, float& right) noexcept;
    void process(float** inputs, float** outputs, int frames) noexcept;
private:
    struct Block
    {
    double overallscale {};
    double distScaling {};
    double iirAmount {};
    double K {};
    double norm {};
    double wet {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.0f };
    float B { 0.0f };
    float C { 1.0f };
    float saturationStrength { 1.0f };
    double sampleRate { 44100.0 };
};
}
