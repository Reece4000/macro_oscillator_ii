/* ========================================
 *  Mackity - Mackity.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
#include "Mackity.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace lodo::airwindows::MackityDSP
{
void Mackity::reset() noexcept
{
    static_assert(std::is_trivially_copyable_v<History>);
    std::memset(static_cast<History*>(this), 0, sizeof(History));
    block = {};




    iirSampleAL = 0.0;
    iirSampleBL = 0.0;
    iirSampleAR = 0.0;
    iirSampleBR = 0.0;
    for (int x = 0; x < 15; x++) {biquadA[x] = 0.0; biquadB[x] = 0.0;}

    fpdL = 0x6d2b79f5u;
    fpdR = 0xa341316cu;
    //this is reset: values being initialized only once. Startup values, whatever they are.


}
void Mackity::beginBlock(int frames) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    sampleFrames = frames;
    auto& overallscale = block.overallscale;
    auto& inTrim = block.inTrim;
    auto& outPad = block.outPad;
    auto& iirAmountA = block.iirAmountA;
    auto& iirAmountB = block.iirAmountB;
    auto& K = block.K;
    auto& norm = block.norm;






    overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    inTrim = A*10.0;
    outPad = B;
    inTrim *= inTrim;

    iirAmountA = 0.001860867/overallscale;
    iirAmountB = 0.000287496/overallscale;

    biquadB[0] = biquadA[0] = std::min(19160.0 / getSampleRate(), 0.49); // Lodo: sub-Nyquist at low rates
    biquadA[1] = 0.431684981684982;
    biquadB[1] = 1.1582298;

    K = tan(M_PI * biquadA[0]); //lowpass
    norm = 1.0 / (1.0 + K / biquadA[1] + K * K);
    biquadA[2] = K * K * norm;
    biquadA[3] = 2.0 * biquadA[2];
    biquadA[4] = biquadA[2];
    biquadA[5] = 2.0 * (K * K - 1.0) * norm;
    biquadA[6] = (1.0 - K / biquadA[1] + K * K) * norm;

    K = tan(M_PI * biquadB[0]);
    norm = 1.0 / (1.0 + K / biquadB[1] + K * K);
    biquadB[2] = K * K * norm;
    biquadB[3] = 2.0 * biquadB[2];
    biquadB[4] = biquadB[2];
    biquadB[5] = 2.0 * (K * K - 1.0) * norm;
    biquadB[6] = (1.0 - K / biquadB[1] + K * K) * norm;


}
void Mackity::processFrame(float& left, float& right) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    --sampleFrames;
    auto& overallscale = block.overallscale;
    auto& inTrim = block.inTrim;
    auto& outPad = block.outPad;
    auto& iirAmountA = block.iirAmountA;
    auto& iirAmountB = block.iirAmountB;
    auto& K = block.K;
    auto& norm = block.norm;

        double inputSampleL = left;
        double inputSampleR = right;
        if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;

        if (fabs(iirSampleAL)<1.18e-37) iirSampleAL = 0.0;
        iirSampleAL = (iirSampleAL * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
        inputSampleL -= iirSampleAL;
        if (fabs(iirSampleAR)<1.18e-37) iirSampleAR = 0.0;
        iirSampleAR = (iirSampleAR * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
        inputSampleR -= iirSampleAR;

        if (inTrim != 1.0) {inputSampleL *= inTrim; inputSampleR *= inTrim;}

        double outSampleL = biquadA[2]*inputSampleL+biquadA[3]*biquadA[7]+biquadA[4]*biquadA[8]-biquadA[5]*biquadA[9]-biquadA[6]*biquadA[10];
        biquadA[8] = biquadA[7]; biquadA[7] = inputSampleL; inputSampleL = outSampleL; biquadA[10] = biquadA[9]; biquadA[9] = inputSampleL; //DF1 left

        double outSampleR = biquadA[2]*inputSampleR+biquadA[3]*biquadA[11]+biquadA[4]*biquadA[12]-biquadA[5]*biquadA[13]-biquadA[6]*biquadA[14];
        biquadA[12] = biquadA[11]; biquadA[11] = inputSampleR; inputSampleR = outSampleR; biquadA[14] = biquadA[13]; biquadA[13] = inputSampleR; //DF1 right

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        inputSampleL -= pow(inputSampleL,5)*0.1768;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        inputSampleR -= pow(inputSampleR,5)*0.1768;

        outSampleL = biquadB[2]*inputSampleL+biquadB[3]*biquadB[7]+biquadB[4]*biquadB[8]-biquadB[5]*biquadB[9]-biquadB[6]*biquadB[10];
        biquadB[8] = biquadB[7]; biquadB[7] = inputSampleL; inputSampleL = outSampleL; biquadB[10] = biquadB[9]; biquadB[9] = inputSampleL; //DF1 left

        outSampleR = biquadB[2]*inputSampleR+biquadB[3]*biquadB[11]+biquadB[4]*biquadB[12]-biquadB[5]*biquadB[13]-biquadB[6]*biquadB[14];
        biquadB[12] = biquadB[11]; biquadB[11] = inputSampleR; inputSampleR = outSampleR; biquadB[14] = biquadB[13]; biquadB[13] = inputSampleR; //DF1 right

        if (fabs(iirSampleBL)<1.18e-37) iirSampleBL = 0.0;
        iirSampleBL = (iirSampleBL * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
        inputSampleL -= iirSampleBL;
        if (fabs(iirSampleBR)<1.18e-37) iirSampleBR = 0.0;
        iirSampleBR = (iirSampleBR * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
        inputSampleR -= iirSampleBR;

        if (outPad != 1.0) {inputSampleL *= outPad; inputSampleR *= outPad;}

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
void Mackity::process(float** inputs, float** outputs, int frames) noexcept
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
