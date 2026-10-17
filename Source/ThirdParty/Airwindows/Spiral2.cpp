/* ========================================
 *  Spiral2 - Spiral2.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
#include "Spiral2.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace lodo::airwindows::Spiral2DSP
{
void Spiral2::reset() noexcept
{
    static_assert(std::is_trivially_copyable_v<History>);
    std::memset(static_cast<History*>(this), 0, sizeof(History));
    block = {};






    iirSampleAL = 0.0;
    iirSampleBL = 0.0;
    prevSampleL = 0.0;
    fpdL = 0x6d2b79f5u;
    fpdR = 0xa341316cu;

    iirSampleAR = 0.0;
    iirSampleBR = 0.0;
    prevSampleR = 0.0;
    flip = true;
    //this is reset: values being initialized only once. Startup values, whatever they are.


}
void Spiral2::beginBlock(int frames) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    sampleFrames = frames;
    auto& overallscale = block.overallscale;
    auto& gain = block.gain;
    auto& iirAmount = block.iirAmount;
    auto& presence = block.presence;
    auto& output = block.output;
    auto& wet = block.wet;






    overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    gain = pow(A*2.0,2.0);
    iirAmount = pow(B,3.0)/overallscale;
    presence = C;
    output = D;
    wet = E;


}
void Spiral2::processFrame(float& left, float& right) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    --sampleFrames;
    auto& overallscale = block.overallscale;
    auto& gain = block.gain;
    auto& iirAmount = block.iirAmount;
    auto& presence = block.presence;
    auto& output = block.output;
    auto& wet = block.wet;

        double inputSampleL = left;
        double inputSampleR = right;

        if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;

        double drySampleL = inputSampleL;
        double drySampleR = inputSampleR;

        if (gain != 1.0) {
            inputSampleL *= gain;
            inputSampleR *= gain;
            prevSampleL *= gain;
            prevSampleR *= gain;
        }

        if (flip)
        {
            iirSampleAL = (iirSampleAL * (1 - iirAmount)) + (inputSampleL * iirAmount);
            iirSampleAR = (iirSampleAR * (1 - iirAmount)) + (inputSampleR * iirAmount);
            inputSampleL -= iirSampleAL;
            inputSampleR -= iirSampleAR;
        }
        else
        {
            iirSampleBL = (iirSampleBL * (1 - iirAmount)) + (inputSampleL * iirAmount);
            iirSampleBR = (iirSampleBR * (1 - iirAmount)) + (inputSampleR * iirAmount);
            inputSampleL -= iirSampleBL;
            inputSampleR -= iirSampleBR;
        }
        //highpass section

        double presenceSampleL = sin(inputSampleL * fabs(prevSampleL)) / ((prevSampleL == 0.0) ?1:fabs(prevSampleL));
        double presenceSampleR = sin(inputSampleR * fabs(prevSampleR)) / ((prevSampleR == 0.0) ?1:fabs(prevSampleR));
        //change from first Spiral: delay of one sample on the scaling factor.
        inputSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((fabs(inputSampleL) == 0.0) ?1:fabs(inputSampleL));
        inputSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((fabs(inputSampleR) == 0.0) ?1:fabs(inputSampleR));

        if (output < 1.0) {
            inputSampleL *= output;
            inputSampleR *= output;
            presenceSampleL *= output;
            presenceSampleR *= output;
        }
        if (presence > 0.0) {
            inputSampleL = (inputSampleL * (1.0-presence)) + (presenceSampleL * presence);
            inputSampleR = (inputSampleR * (1.0-presence)) + (presenceSampleR * presence);
        }
        if (wet < 1.0) {
            inputSampleL = (drySampleL * (1.0-wet)) + (inputSampleL * wet);
            inputSampleR = (drySampleR * (1.0-wet)) + (inputSampleR * wet);
        }
        //nice little output stage template: if we have another scale of floating point
        //number, we really don't want to meaninglessly multiply that by 1.0.

        prevSampleL = drySampleL;
        prevSampleR = drySampleR;
        flip = !flip;

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
void Spiral2::process(float** inputs, float** outputs, int frames) noexcept
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
