/* ========================================
 *  FireAmp - FireAmp.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::FireAmpDSP
{



struct History
{



    double lastSampleL;
    double storeSampleL;
    double smoothAL;
    double smoothBL;
    double smoothCL;
    double smoothDL;
    double smoothEL;
    double smoothFL;
    double smoothGL;
    double smoothHL;
    double smoothIL;
    double smoothJL;
    double smoothKL;
    double smoothLL;
    double iirSampleAL;
    double iirSampleBL;
    double iirSampleCL;
    double iirSampleDL;
    double iirSampleEL;
    double iirSampleFL;
    double iirSampleGL;
    double iirSampleHL;
    double iirSampleIL;
    double iirSampleJL;
    double iirSampleKL;
    double iirSampleLL;
    double iirLowpassL;
    double iirSpkAL;
    double iirSpkBL;
    double iirSubL;
    double OddL[257];
    double EvenL[257];

    double lastSampleR;
    double storeSampleR;
    double smoothAR;
    double smoothBR;
    double smoothCR;
    double smoothDR;
    double smoothER;
    double smoothFR;
    double smoothGR;
    double smoothHR;
    double smoothIR;
    double smoothJR;
    double smoothKR;
    double smoothLR;
    double iirSampleAR;
    double iirSampleBR;
    double iirSampleCR;
    double iirSampleDR;
    double iirSampleER;
    double iirSampleFR;
    double iirSampleGR;
    double iirSampleHR;
    double iirSampleIR;
    double iirSampleJR;
    double iirSampleKR;
    double iirSampleLR;
    double iirLowpassR;
    double iirSpkAR;
    double iirSpkBR;
    double iirSubR;
    double OddR[257];
    double EvenR[257];

    bool flip;
    int count; //amp

    double bL[90];
    double lastCabSampleL;
    double smoothCabAL;
    double smoothCabBL; //cab

    double bR[90];
    double lastCabSampleR;
    double smoothCabAR;
    double smoothCabBR; //cab

    double lastRefL[10];
    double lastRefR[10];
    int cycle;  //undersampling

    enum {
        fix_freq,
        fix_reso,
        fix_a0,
        fix_a1,
        fix_a2,
        fix_b1,
        fix_b2,
        fix_sL1,
        fix_sL2,
        fix_sR1,
        fix_sR2,
        fix_total
    }; //fixed frequency biquad filter for ultrasonics, stereo
    double fixA[fix_total];
    double fixB[fix_total];
    double fixC[fix_total];
    double fixD[fix_total];
    double fixE[fix_total];
    double fixF[fix_total]; //filtering

    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff

};
class FireAmp : private History
{
public:
    static constexpr int controlCount = 4;
    FireAmp() { reset(); }
    void reset() noexcept;
    void setSampleRate(double value) noexcept { sampleRate = value; }
    void setParameters(const std::array<float, controlCount>& values) noexcept { A = values[0]; B = values[1]; C = values[2]; D = values[3]; }
    void beginBlock(int frames) noexcept;
    void processFrame(float& left, float& right) noexcept;
    void process(float** inputs, float** outputs, int frames) noexcept;
private:
    struct Block
    {
    double bassfill {};
    double outputlevel {};
    double wet {};
    double overallscale {};
    int cycleEnd {};
    double startlevel {};
    double samplerate {};
    double basstrim {};
    double toneEQ {};
    double EQ {};
    double bleed {};
    double bassfactor {};
    double BEQ {};
    int diagonal {};
    int side {};
    int down {};
    double cutoff {};
    double K {};
    double norm {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.5f };
    float B { 0.5f };
    float C { 0.8f };
    float D { 1.0f };
    double sampleRate { 44100.0 };
};
}
