/* ========================================
 *  ToTape9 - ToTape9.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) Airwindows, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::ToTape9DSP
{



struct History
{


    double iirEncL;
    double iirDecL;
    double compEncL;
    double compDecL;
    double avgEncL;
    double avgDecL;
    double iirEncR;
    double iirDecR;
    double compEncR;
    double compDecR;
    double avgEncR;
    double avgDecR;

    double dL[1002];
    double dR[1002];
    double sweepL;
    double sweepR;
    double nextmaxL;
    double nextmaxR;
    int gcount;

    enum {
        prevSampL1,
        prevSampR1,
        threshold1,
        prevSampL2,
        prevSampR2,
        threshold2,
        prevSampL3,
        prevSampR3,
        threshold3,
        prevSampL4,
        prevSampR4,
        threshold4,
        prevSampL5,
        prevSampR5,
        threshold5,
        prevSampL6,
        prevSampR6,
        threshold6,
        prevSampL7,
        prevSampR7,
        threshold7,
        prevSampL8,
        prevSampR8,
        threshold8,
        prevSampL9,
        prevSampR9,
        threshold9,
        gslew_total
    }; //fixed frequency pear filter for ultrasonics, stereo
    double gslew[gslew_total]; //end bias

    double hysteresisL;
    double hysteresisR;

    double headBumpL;
    double headBumpR;
    enum {
        hdb_freq,
        hdb_reso,
        hdb_a0,
        hdb_a1,
        hdb_a2,
        hdb_b1,
        hdb_b2,
        hdb_sL1,
        hdb_sL2,
        hdb_sR1,
        hdb_sR2,
        hdb_total
    }; //fixed frequency biquad filter for ultrasonics, stereo
    double hdbA[hdb_total];
    double hdbB[hdb_total];

    double avg32L[33];
    double avg32R[33];
    double avg16L[17];
    double avg16R[17];
    double avg8L[9];
    double avg8R[9];
    double avg4L[5];
    double avg4R[5];
    double avg2L[3];
    double avg2R[3];
    double post32L[33];
    double post32R[33];
    double post16L[17];
    double post16R[17];
    double post8L[9];
    double post8R[9];
    double post4L[5];
    double post4R[5];
    double post2L[3];
    double post2R[3];
    double lastDarkL;
    double lastDarkR;
    int avgPos;
    //preTapeHack

    double lastSampleL;
    double intermediateL[18];
    double slewL[34];
    bool wasPosClipL;
    bool wasNegClipL;
    double lastSampleR;
    double intermediateR[18];
    double slewR[34];
    bool wasPosClipR;
    bool wasNegClipR; //Stereo ClipOnly3

    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff

};
class ToTape9 : private History
{
public:
    static constexpr int controlCount = 9;
    ToTape9() { reset(); }
    void reset() noexcept;
    void linkStereoRandomness() noexcept { fpdR = fpdL; }
    void setSampleRate(double value) noexcept { sampleRate = value; }
    void setParameters(const std::array<float, controlCount>& values) noexcept { A = values[0]; B = values[1]; C = values[2]; D = values[3]; E = values[4]; F = values[5]; G = values[6]; H = values[7]; I = values[8]; }
    void beginBlock(int frames) noexcept;
    void processFrame(float& left, float& right) noexcept;
    void process(float** inputs, float** outputs, int frames) noexcept;
private:
    struct Block
    {
    double overallscale {};
    int spacing {};
    int slewsing {};
    double inputGain {};
    double dublyAmount {};
    double outlyAmount {};
    double iirEncFreq {};
    double iirDecFreq {};
    double flutDepth {};
    double flutFrequency {};
    double bias {};
    double underBias {};
    double overBias {};
    double headBumpDrive {};
    double headBumpMix {};
    double K {};
    double norm {};
    double outputGain {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.5f };
    float B { 0.5f };
    float C { 0.5f };
    float D { 0.5f };
    float E { 0.5f };
    float F { 0.5f };
    float G { 0.5f };
    float H { 0.5f };
    float I { 0.5f };
    double sampleRate { 44100.0 };
};
}
