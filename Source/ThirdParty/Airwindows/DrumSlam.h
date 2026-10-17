/* ========================================
 *  DrumSlam - DrumSlam.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::DrumSlamDSP
{



struct History
{


    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff

    double iirSampleAL;
    double iirSampleBL;
    double iirSampleCL;
    double iirSampleDL;
    double iirSampleEL;
    double iirSampleFL;
    double iirSampleGL;
    double iirSampleHL;
    double lastSampleL;

    double iirSampleAR;
    double iirSampleBR;
    double iirSampleCR;
    double iirSampleDR;
    double iirSampleER;
    double iirSampleFR;
    double iirSampleGR;
    double iirSampleHR;
    double lastSampleR;
    bool fpFlip;


};
class DrumSlam : private History
{
public:
    static constexpr int controlCount = 3;
    DrumSlam() { reset(); }
    void reset() noexcept;
    void setSampleRate(double value) noexcept { sampleRate = value; }
    void setParameters(const std::array<float, controlCount>& values) noexcept { A = values[0]; B = values[1]; C = values[2]; }
    void beginBlock(int frames) noexcept;
    void processFrame(float& left, float& right) noexcept;
    void process(float** inputs, float** outputs, int frames) noexcept;
private:
    struct Block
    {
    double overallscale {};
    double iirAmountL {};
    double iirAmountH {};
    double drive {};
    double out {};
    double wet {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.0f };
    float B { 1.0f };
    float C { 1.0f };
    double sampleRate { 44100.0 };
};
}
