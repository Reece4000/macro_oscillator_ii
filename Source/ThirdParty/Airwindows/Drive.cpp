/* ========================================
 *  Drive - Drive.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
#include "Drive.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace lodo::airwindows::DriveDSP
{
void Drive::reset() noexcept
{
    static_assert(std::is_trivially_copyable_v<History>);
    std::memset(static_cast<History*>(this), 0, sizeof(History));
    block = {};





    iirSampleAL = 0.0;
    iirSampleBL = 0.0;
    iirSampleAR = 0.0;
    iirSampleBR = 0.0;
    fpFlip = true;
    fpdL = 0x6d2b79f5u;
    fpdR = 0xa341316cu;
    //this is reset: values being initialized only once. Startup values, whatever they are.


}
void Drive::beginBlock(int frames) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    sampleFrames = frames;
    auto& overallscale = block.overallscale;
    auto& driveone = block.driveone;
    auto& iirAmount = block.iirAmount;
    auto& output = block.output;
    auto& wet = block.wet;
    auto& dry = block.dry;
    auto& glitch = block.glitch;
    auto& out = block.out;
    auto& inputSampleL = block.inputSampleL;
    auto& inputSampleR = block.inputSampleR;
    auto& drySampleL = block.drySampleL;
    auto& drySampleR = block.drySampleR;






    overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    driveone = pow(A*2.0,2);
    iirAmount = pow(B,3)/overallscale;
    output = C;
    wet = D;
    dry = 1.0-wet;
    glitch = 0.60;









}
void Drive::processFrame(float& left, float& right) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    --sampleFrames;
    auto& overallscale = block.overallscale;
    auto& driveone = block.driveone;
    auto& iirAmount = block.iirAmount;
    auto& output = block.output;
    auto& wet = block.wet;
    auto& dry = block.dry;
    auto& glitch = block.glitch;
    auto& out = block.out;
    auto& inputSampleL = block.inputSampleL;
    auto& inputSampleR = block.inputSampleR;
    auto& drySampleL = block.drySampleL;
    auto& drySampleR = block.drySampleR;

        inputSampleL = left;
        inputSampleR = right;
        if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
        drySampleL = inputSampleL;
        drySampleR = inputSampleR;

        if (fpFlip)
        {
            iirSampleAL = (iirSampleAL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
            inputSampleL -= iirSampleAL;
            iirSampleAR = (iirSampleAR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
            inputSampleR -= iirSampleAR;
        }
        else
        {
            iirSampleBL = (iirSampleBL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
            inputSampleL -= iirSampleBL;
            iirSampleBR = (iirSampleBR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
            inputSampleR -= iirSampleBR;
        }
        //highpass section
        fpFlip = !fpFlip;


        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;

        out = driveone;
        while (out > glitch)
        {
            out -= glitch;
            inputSampleL -= (inputSampleL * (fabs(inputSampleL) * glitch) * (fabs(inputSampleL) * glitch) );
            inputSampleL *= (1.0+glitch);
            inputSampleR -= (inputSampleR * (fabs(inputSampleR) * glitch) * (fabs(inputSampleR) * glitch) );
            inputSampleR *= (1.0+glitch);
        }
        //that's taken care of the really high gain stuff

        inputSampleL -= (inputSampleL * (fabs(inputSampleL) * out) * (fabs(inputSampleL) * out) );
        inputSampleL *= (1.0+out);
        inputSampleR -= (inputSampleR * (fabs(inputSampleR) * out) * (fabs(inputSampleR) * out) );
        inputSampleR *= (1.0+out);

        if (output < 1.0) {
            inputSampleL *= output;
            inputSampleR *= output;
        }
        if (wet < 1.0) {
            inputSampleL = (drySampleL * dry)+(inputSampleL * wet);
            inputSampleR = (drySampleR * dry)+(inputSampleR * wet);
        }
        //nice little output stage template: if we have another scale of floating point
        //number, we really don't want to meaninglessly multiply that by 1.0.

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
void Drive::process(float** inputs, float** outputs, int frames) noexcept
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
