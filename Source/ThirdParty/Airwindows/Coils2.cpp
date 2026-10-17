/* ========================================
 *  Coils2 - Coils2.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
#include "Coils2.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace lodo::airwindows::Coils2DSP
{
void Coils2::reset() noexcept
{
    static_assert(std::is_trivially_copyable_v<History>);
    std::memset(static_cast<History*>(this), 0, sizeof(History));
    block = {};
    saturationStrength = 1.0f;




    for (int x = 0; x < 15; x++) {biquadA[x] = 0.0; biquadB[x] = 0.0;}
    hysteresisL = hysteresisR = 0.0;
    fpdL = 0x6d2b79f5u;
    fpdR = 0xa341316cu;
    //this is reset: values being initialized only once. Startup values, whatever they are.


}
void Coils2::beginBlock(int frames) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    sampleFrames = frames;
    auto& overallscale = block.overallscale;
    auto& distScaling = block.distScaling;
    auto& iirAmount = block.iirAmount;
    auto& K = block.K;
    auto& norm = block.norm;
    auto& wet = block.wet;






    overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    distScaling = pow(1.0-A,2);
    if (distScaling < 0.0001) distScaling = 0.0001;
    biquadA[0] = 600.0/getSampleRate();
    biquadA[1] = 0.01+(pow(B,2)*0.5);
    iirAmount = biquadA[1]/overallscale;
    K = tan(M_PI * biquadA[0]);
    norm = 1.0 / (1.0 + K / biquadA[1] + K * K);
    biquadA[2] = K / biquadA[1] * norm;
    biquadA[4] = -biquadA[2];
    biquadA[5] = 2.0 * (K * K - 1.0) * norm;
    biquadA[6] = (1.0 - K / biquadA[1] + K * K) * norm;
    biquadB[0] = (21890.0-(biquadA[1]*890.0))/getSampleRate();
    biquadB[1] = 0.89;
    K = tan(M_PI * biquadB[0]);
    norm = 1.0 / (1.0 + K / biquadB[1] + K * K);
    biquadB[2] = K * K * norm;
    biquadB[3] = 2.0 * biquadB[2];
    biquadB[4] = biquadB[2];
    biquadB[5] = 2.0 * (K * K - 1.0) * norm;
    biquadB[6] = (1.0 - K / biquadB[1] + K * K) * norm;
    wet = C;


}
void Coils2::processFrame(float& left, float& right) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    --sampleFrames;
    auto& overallscale = block.overallscale;
    auto& distScaling = block.distScaling;
    auto& iirAmount = block.iirAmount;
    auto& K = block.K;
    auto& norm = block.norm;
    auto& wet = block.wet;

        double inputSampleL = left;
        double inputSampleR = right;
        if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
        double drySampleL = inputSampleL;
        double drySampleR = inputSampleR;

        if (biquadA[0] < 0.49999) {
            double tempSample = (inputSampleL * biquadA[2]) + biquadA[7];
            biquadA[7] = -(tempSample * biquadA[5]) + biquadA[8];
            biquadA[8] = (inputSampleL * biquadA[4]) - (tempSample * biquadA[6]);
            inputSampleL = tempSample;
            tempSample = (inputSampleR * biquadA[2]) + biquadA[9];
            biquadA[9] = -(tempSample * biquadA[5]) + biquadA[10];
            biquadA[10] = (inputSampleR * biquadA[4]) - (tempSample * biquadA[6]);
            inputSampleR = tempSample; //create bandpass of clean tone
        }
        double diffSampleL = (drySampleL-inputSampleL)/distScaling;
        double diffSampleR = (drySampleR-inputSampleR)/distScaling; //mids notched out
        if (biquadB[0] < 0.49999) {
            double tempSample = (diffSampleL * biquadB[2]) + biquadB[7];
            biquadB[7] = (diffSampleL * biquadB[3]) - (tempSample * biquadB[5]) + biquadB[8];
            biquadB[8] = (diffSampleL * biquadB[4]) - (tempSample * biquadB[6]);
            diffSampleL = tempSample;
            tempSample = (diffSampleR * biquadB[2]) + biquadB[9];
            biquadB[9] = (diffSampleR * biquadB[3]) - (tempSample * biquadB[5]) + biquadB[10];
            biquadB[10] = (diffSampleR * biquadB[4]) - (tempSample * biquadB[6]);
            diffSampleR = tempSample;
        }
        hysteresisL = (hysteresisL * (1.0-iirAmount)) + (diffSampleL * iirAmount);
        if (fabs(hysteresisL)<1.18e-37) hysteresisL = 0.0; else diffSampleL -= hysteresisL;
        if (diffSampleL > 1.571) diffSampleL = 1.571; else if (diffSampleL < -1.571) diffSampleL = -1.571;
        if (hysteresisL > 1.571) hysteresisL = 1.571; else if (hysteresisL < -1.571) hysteresisL = -1.571;
        hysteresisR = (hysteresisR * (1.0-iirAmount)) + (diffSampleR * iirAmount);
        if (fabs(hysteresisR)<1.18e-37) hysteresisR = 0.0; else diffSampleR -= hysteresisR;
        if (diffSampleR > 1.571) diffSampleR = 1.571; else if (diffSampleR < -1.571) diffSampleR = -1.571;
        if (hysteresisR > 1.571) hysteresisR = 1.571; else if (hysteresisR < -1.571) hysteresisR = -1.571;
        inputSampleL += (sin(diffSampleL)-sin(hysteresisL))*distScaling*saturationStrength;
        inputSampleR += (sin(diffSampleR)-sin(hysteresisR))*distScaling*saturationStrength; //apply transformer distortions

        if (wet !=1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
            inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
        }

        //begin 32 bit stereo floating point dither
        int expon; frexpf((float)inputSampleL, &expon);
        fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
        inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
        frexpf((float)inputSampleR, &expon);
        fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
        inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
        //end 32 bit stereo floating point dither

        left = inputSampleL;
        right = inputSampleR;






}
void Coils2::process(float** inputs, float** outputs, int frames) noexcept
{
    beginBlock(frames);
    for (int i = 0; i < frames; ++i)
    {
        float left = inputs[0][i], right = inputs[1][i];
        processFrame(left, right);
        outputs[0][i] = left;
        outputs[1][i] = right;
    }
}
}
