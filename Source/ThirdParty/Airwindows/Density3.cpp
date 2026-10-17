/* ========================================
 *  Density3 - Density3.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) Airwindows, Airwindows uses the MIT license
 * ======================================== */
#include "Density3.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace lodo::airwindows::Density3DSP
{
void Density3::reset() noexcept
{
    static_assert(std::is_trivially_copyable_v<History>);
    std::memset(static_cast<History*>(this), 0, sizeof(History));
    block = {};






    iirSampleL = 0.0;
    iirSampleR = 0.0;

    fpdL = 0x6d2b79f5u;
    fpdR = 0xa341316cu;
    //this is reset: values being initialized only once. Startup values, whatever they are.


}
void Density3::beginBlock(int frames) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    sampleFrames = frames;
    auto& overallscale = block.overallscale;
    auto& density = block.density;
    auto& iirAmount = block.iirAmount;
    auto& output = block.output;
    auto& wet = block.wet;






    overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();
    density = A*5.0;
    iirAmount = pow(B,3)/overallscale;
    if (iirAmount == 0.0) {iirSampleL = 0.0; iirSampleR = 0.0;}
    output = C;
    wet = D;


}
void Density3::processFrame(float& left, float& right) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    --sampleFrames;
    auto& overallscale = block.overallscale;
    auto& density = block.density;
    auto& iirAmount = block.iirAmount;
    auto& output = block.output;
    auto& wet = block.wet;

        double inputSampleL = left;
        double inputSampleR = right;
        if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
        double drySampleL = inputSampleL;
        double drySampleR = inputSampleR;

        iirSampleL = (iirSampleL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
        inputSampleL -= iirSampleL;
        iirSampleR = (iirSampleR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
        inputSampleR -= iirSampleR;

        double altered = inputSampleL;
        if (density > 1.0) {
            altered = fmax(fmin(inputSampleL*density*M_PI_2,M_PI_2),-M_PI_2);
            double X = altered*altered;
            double temp = altered*X;
            altered -= (temp / 6.0); temp *= X;
            altered += (temp / 120.0); temp *= X;
            altered -= (temp / 5040.0); temp *= X;
            altered += (temp / 362880.0); temp *= X;
            altered -= (temp / 39916800.0);
        }
        if (density < 1.0) {
            altered = fmax(fmin(inputSampleL,1.0),-1.0);
            double polarity = altered;
            double X = inputSampleL * altered;
            double temp = X;
            altered = (temp / 2.0); temp *= X;
            altered -= (temp / 24.0); temp *= X;
            altered += (temp / 720.0); temp *= X;
            altered -= (temp / 40320.0); temp *= X;
            altered += (temp / 3628800.0);
            altered *= ((polarity<0.0)?-1.0:1.0);
        }
        if (density > 2.0) inputSampleL = altered;
        else inputSampleL = (inputSampleL*(1.0-fabs(density-1.0)))+(altered*fabs(density-1.0));

        altered = inputSampleR;
        if (density > 1.0) {
            altered = fmax(fmin(inputSampleR*density*M_PI_2,M_PI_2),-M_PI_2);
            double X = altered*altered;
            double temp = altered*X;
            altered -= (temp / 6.0); temp *= X;
            altered += (temp / 120.0); temp *= X;
            altered -= (temp / 5040.0); temp *= X;
            altered += (temp / 362880.0); temp *= X;
            altered -= (temp / 39916800.0);
        }
        if (density < 1.0) {
            altered = fmax(fmin(inputSampleR,1.0),-1.0);
            double polarity = altered;
            double X = inputSampleR * altered;
            double temp = X;
            altered = (temp / 2.0); temp *= X;
            altered -= (temp / 24.0); temp *= X;
            altered += (temp / 720.0); temp *= X;
            altered -= (temp / 40320.0); temp *= X;
            altered += (temp / 3628800.0);
            altered *= ((polarity<0.0)?-1.0:1.0);
        }
        if (density > 2.0) inputSampleR = altered;
        else inputSampleR = (inputSampleR*(1.0-fabs(density-1.0)))+(altered*fabs(density-1.0));

        inputSampleL = (drySampleL*(1.0-wet))+(inputSampleL*output*wet);
        inputSampleR = (drySampleR*(1.0-wet))+(inputSampleR*output*wet);

        //begin 32 bit stereo floating point dither
        int expon; frexpf((float)inputSampleL, &expon);
        fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
        inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 3.553e-44l * pow(2,expon+62));
        frexpf((float)inputSampleR, &expon);
        fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
        if (fpdL-fpdR < 1073741824 || fpdR-fpdL < 1073741824) {
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;}
        inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 3.553e-44l * pow(2,expon+62));
        //end 32 bit stereo floating point dither

        left = inputSampleL;
        right = inputSampleR;






}
void Density3::process(float** inputs, float** outputs, int frames) noexcept
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
