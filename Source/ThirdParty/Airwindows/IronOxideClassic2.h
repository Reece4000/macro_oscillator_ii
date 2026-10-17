/* ========================================
 *  IronOxideClassic2 - IronOxideClassic2.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::IronOxideClassic2DSP
{



struct History
{


    double iirSampleAL;
    double iirSampleBL;
    double dL[264];
    double fastIIRAL;
    double fastIIRBL;
    double slowIIRAL;
    double slowIIRBL;
    double lastRefL[7];

    double iirSampleAR;
    double iirSampleBR;
    double dR[264];
    double fastIIRAR;
    double fastIIRBR;
    double slowIIRAR;
    double slowIIRBR;
    double lastRefR[7];

    double biquadA[15];
    double biquadB[15];

    int cycle;
    int gcount;
    bool flip;

    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff

};
class IronOxideClassic2 : private History
{
public:
    static constexpr int controlCount = 3;
    IronOxideClassic2() { reset(); }
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
    int cycleEnd {};
    double inputgain {};
    double outputgain {};
    double ips {};
    double iirAmount {};
    double fastTaper {};
    double slowTaper {};
    double K {};
    double norm {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.5f };
    float B { 0.562341325190349f };
    float C { 0.5f };
    double sampleRate { 44100.0 };
};
}
