/* ========================================
 *  BitGlitter - BitGlitter.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::BitGlitterDSP
{



struct History
{


    uint32_t fpdL;
    uint32_t fpdR;

    double ataLastSampleL;
    double ataHalfwaySampleL;
    double ataDrySampleL;
    double lastSampleL;
    double heldSampleAL;
    double heldSampleBL;
    double positionAL;
    double positionBL;
    double lastOutputSampleL;

    double ataLastSampleR;
    double ataHalfwaySampleR;
    double ataDrySampleR;
    double lastSampleR;
    double heldSampleAR;
    double heldSampleBR;
    double positionAR;
    double positionBR;
    double lastOutputSampleR;
    //there is no noise shaping on this one, it uses all sorts of quantization to get its sound

};
class BitGlitter : private History
{
public:
    static constexpr int controlCount = 4;
    BitGlitter() { reset(); }
    void reset() noexcept;
    void setSampleRate(double value) noexcept { sampleRate = value; }
    void setParameters(const std::array<float, controlCount>& values) noexcept { A = values[0]; B = values[1]; C = values[2]; D = values[3]; }
    void beginBlock(int frames) noexcept;
    void processFrame(float& left, float& right) noexcept;
    void process(float** inputs, float** outputs, int frames) noexcept;
private:
    struct Block
    {
    double overallscale {};
    double factor {};
    int divvy {};
    double rateA {};
    double rezA {};
    double rateB {};
    double rezB {};
    double offset {};
    double ingain {};
    double outgain {};
    double wet {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.5f };
    float B { 0.0f };
    float C { 0.5f };
    float D { 1.0f };
    double sampleRate { 44100.0 };
};
}
