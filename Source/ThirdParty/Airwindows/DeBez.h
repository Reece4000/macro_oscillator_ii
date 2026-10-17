/* ========================================
 *  DeBez - DeBez.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) Airwindows, Airwindows uses the MIT license
 * ======================================== */
// Lodo SDK-free adaptation. See README.md and the retained original sources.
#pragma once
#include <array>
#include <cstdint>
namespace lodo::airwindows::DeBezDSP
{



struct History
{


    enum {
        bez_AL,
        bez_BL,
        bez_CL,
        bez_InL,
        bez_UnInL,
        bez_SampL,
        bez_AR,
        bez_BR,
        bez_CR,
        bez_InR,
        bez_UnInR,
        bez_SampR,
        bez_AvgInSampL,
        bez_AvgInSampR,
        bez_AvgOutSampL,
        bez_AvgOutSampR,
        bez_cycle,
        bez_total
    }; //the new undersampling. bez signifies the bezier curve reconstruction
    double bezF[bez_total];

    double rezA;
    double rezB;
    double bitA;
    double bitB;
    double wetA;
    double wetB;

    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff

};
class DeBez : private History
{
public:
    static constexpr int controlCount = 3;
    DeBez() { reset(); }
    void reset() noexcept;
    void setSampleRate(double value) noexcept { sampleRate = value; }
    void setParameters(const std::array<float, controlCount>& values) noexcept { A = values[0]; B = values[1]; C = values[2]; }
    void beginBlock(int frames) noexcept;
    void processFrame(float& left, float& right) noexcept;
    void process(float** inputs, float** outputs, int frames) noexcept;
private:
    struct Block
    {
    int inFramesToProcess {};
    double overallscale {};
    bool steppedFreq {};
    int bezFreqFraction {};
    double bezFreqTrim {};
        int sampleFrames {};
    } block;
    double getSampleRate() const noexcept { return sampleRate; }
    float A { 0.5f };
    float B { 0.5f };
    float C { 1.0f };
    double sampleRate { 44100.0 };
};
}
