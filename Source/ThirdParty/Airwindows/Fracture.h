/* ========================================
 *  Fracture - Fracture.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::FractureDSP
{



struct History
{


    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff


};
class Fracture : private History
{
public:
    static constexpr int controlCount = 4;
    Fracture() { reset(); }
    void reset() noexcept;
    void setSampleRate(double value) noexcept { sampleRate = value; }
    void setParameters(const std::array<float, controlCount>& values) noexcept { A = values[0]; B = values[1]; C = values[2]; D = values[3]; }
    void beginBlock(int frames) noexcept;
    void processFrame(float& left, float& right) noexcept;
    void process(float** inputs, float** outputs, int frames) noexcept;
private:
    struct Block
    {
    double density {};
    double fracture {};
    double output {};
    double wet {};
    double dry {};
    double bridgerectifier {};
    double inputSampleL {};
    double inputSampleR {};
    double drySampleL {};
    double drySampleR {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.25f };
    float B { 0.5f };
    float C { 1.0f };
    float D { 1.0f };
    double sampleRate { 44100.0 };
};
}
