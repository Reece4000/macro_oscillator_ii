/* ========================================
 *  DrumSlam - DrumSlam.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
#include "DrumSlam.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace lodo::airwindows::DrumSlamDSP
{
void DrumSlam::reset() noexcept
{
    static_assert(std::is_trivially_copyable_v<History>);
    std::memset(static_cast<History*>(this), 0, sizeof(History));
    block = {};





    iirSampleAL = 0.0;
    iirSampleBL = 0.0;
    iirSampleCL = 0.0;
    iirSampleDL = 0.0;
    iirSampleEL = 0.0;
    iirSampleFL = 0.0;
    iirSampleGL = 0.0;
    iirSampleHL = 0.0;
    lastSampleL = 0.0;

    iirSampleAR = 0.0;
    iirSampleBR = 0.0;
    iirSampleCR = 0.0;
    iirSampleDR = 0.0;
    iirSampleER = 0.0;
    iirSampleFR = 0.0;
    iirSampleGR = 0.0;
    iirSampleHR = 0.0;
    lastSampleR = 0.0;
    fpFlip = true;
    fpdL = 0x6d2b79f5u;
    fpdR = 0xa341316cu;
    //this is reset: values being initialized only once. Startup values, whatever they are.


}
void DrumSlam::beginBlock(int frames) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    sampleFrames = frames;
    auto& overallscale = block.overallscale;
    auto& iirAmountL = block.iirAmountL;
    auto& iirAmountH = block.iirAmountH;
    auto& drive = block.drive;
    auto& out = block.out;
    auto& wet = block.wet;






    overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();
    iirAmountL = 0.0819;
    iirAmountL /= overallscale;
    iirAmountH = 0.377933067;
    iirAmountH /= overallscale;
    drive = (A*3.0)+1.0;
    out = B;
    wet = C;
    //removed extra dry variable


}
void DrumSlam::processFrame(float& left, float& right) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    --sampleFrames;
    auto& overallscale = block.overallscale;
    auto& iirAmountL = block.iirAmountL;
    auto& iirAmountH = block.iirAmountH;
    auto& drive = block.drive;
    auto& out = block.out;
    auto& wet = block.wet;

        double inputSampleL = left;
        double inputSampleR = right;
        if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
        double drySampleL = inputSampleL;
        double drySampleR = inputSampleR;
        double lowSampleL;
        double lowSampleR;
        double midSampleL;
        double midSampleR;
        double highSampleL;
        double highSampleR;


        inputSampleL *= drive;
        inputSampleR *= drive;

        if (fpFlip)
        {
            iirSampleAL = (iirSampleAL * (1 - iirAmountL)) + (inputSampleL * iirAmountL);
            iirSampleBL = (iirSampleBL * (1 - iirAmountL)) + (iirSampleAL * iirAmountL);
            lowSampleL = iirSampleBL;

            iirSampleAR = (iirSampleAR * (1 - iirAmountL)) + (inputSampleR * iirAmountL);
            iirSampleBR = (iirSampleBR * (1 - iirAmountL)) + (iirSampleAR * iirAmountL);
            lowSampleR = iirSampleBR;

            iirSampleEL = (iirSampleEL * (1 - iirAmountH)) + (inputSampleL * iirAmountH);
            iirSampleFL = (iirSampleFL * (1 - iirAmountH)) + (iirSampleEL * iirAmountH);
            midSampleL = iirSampleFL - iirSampleBL;

            iirSampleER = (iirSampleER * (1 - iirAmountH)) + (inputSampleR * iirAmountH);
            iirSampleFR = (iirSampleFR * (1 - iirAmountH)) + (iirSampleER * iirAmountH);
            midSampleR = iirSampleFR - iirSampleBR;

            highSampleL = inputSampleL - iirSampleFL;
            highSampleR = inputSampleR - iirSampleFR;
        }
        else
        {
            iirSampleCL = (iirSampleCL * (1 - iirAmountL)) + (inputSampleL * iirAmountL);
            iirSampleDL = (iirSampleDL * (1 - iirAmountL)) + (iirSampleCL * iirAmountL);
            lowSampleL = iirSampleDL;

            iirSampleCR = (iirSampleCR * (1 - iirAmountL)) + (inputSampleR * iirAmountL);
            iirSampleDR = (iirSampleDR * (1 - iirAmountL)) + (iirSampleCR * iirAmountL);
            lowSampleR = iirSampleDR;

            iirSampleGL = (iirSampleGL * (1 - iirAmountH)) + (inputSampleL * iirAmountH);
            iirSampleHL = (iirSampleHL * (1 - iirAmountH)) + (iirSampleGL * iirAmountH);
            midSampleL = iirSampleHL - iirSampleDL;

            iirSampleGR = (iirSampleGR * (1 - iirAmountH)) + (inputSampleR * iirAmountH);
            iirSampleHR = (iirSampleHR * (1 - iirAmountH)) + (iirSampleGR * iirAmountH);
            midSampleR = iirSampleHR - iirSampleDR;

            highSampleL = inputSampleL - iirSampleHL;
            highSampleR = inputSampleR - iirSampleHR;
        }
        fpFlip = !fpFlip;

        //generate the tone bands we're using
        if (lowSampleL > 1.0) {lowSampleL = 1.0;}
        if (lowSampleL < -1.0) {lowSampleL = -1.0;}
        if (lowSampleR > 1.0) {lowSampleR = 1.0;}
        if (lowSampleR < -1.0) {lowSampleR = -1.0;}
        lowSampleL -= (lowSampleL * (fabs(lowSampleL) * 0.448) * (fabs(lowSampleL) * 0.448) );
        lowSampleR -= (lowSampleR * (fabs(lowSampleR) * 0.448) * (fabs(lowSampleR) * 0.448) );
        lowSampleL *= drive;
        lowSampleR *= drive;

        if (highSampleL > 1.0) {highSampleL = 1.0;}
        if (highSampleL < -1.0) {highSampleL = -1.0;}
        if (highSampleR > 1.0) {highSampleR = 1.0;}
        if (highSampleR < -1.0) {highSampleR = -1.0;}
        highSampleL -= (highSampleL * (fabs(highSampleL) * 0.599) * (fabs(highSampleL) * 0.599) );
        highSampleR -= (highSampleR * (fabs(highSampleR) * 0.599) * (fabs(highSampleR) * 0.599) );
        highSampleL *= drive;
        highSampleR *= drive;

        midSampleL = midSampleL * drive;
        midSampleR = midSampleR * drive;

        double skew = (midSampleL - lastSampleL);
        lastSampleL = midSampleL;
        //skew will be direction/angle
        double bridgerectifier = fabs(skew);
        if (bridgerectifier > 3.1415926) bridgerectifier = 3.1415926;
        //for skew we want it to go to zero effect again, so we use full range of the sine
        bridgerectifier = sin(bridgerectifier);
        if (skew > 0) skew = bridgerectifier*3.1415926;
        else skew = -bridgerectifier*3.1415926;
        //skew is now sined and clamped and then re-amplified again
        skew *= midSampleL;
        //cools off sparkliness and crossover distortion
        skew *= 1.557079633;
        //crank up the gain on this so we can make it sing
        bridgerectifier = fabs(midSampleL);
        bridgerectifier += skew;
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
        bridgerectifier = sin(bridgerectifier);
        bridgerectifier *= drive;
        bridgerectifier += skew;
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
        bridgerectifier = sin(bridgerectifier);
        if (midSampleL > 0)
        {
            midSampleL = bridgerectifier;
        }
        else
        {
            midSampleL = -bridgerectifier;
        }
        //blend according to positive and negative controls, left

        skew = (midSampleR - lastSampleR);
        lastSampleR = midSampleR;
        //skew will be direction/angle
        bridgerectifier = fabs(skew);
        if (bridgerectifier > 3.1415926) bridgerectifier = 3.1415926;
        //for skew we want it to go to zero effect again, so we use full range of the sine
        bridgerectifier = sin(bridgerectifier);
        if (skew > 0) skew = bridgerectifier*3.1415926;
        else skew = -bridgerectifier*3.1415926;
        //skew is now sined and clamped and then re-amplified again
        skew *= midSampleR;
        //cools off sparkliness and crossover distortion
        skew *= 1.557079633;
        //crank up the gain on this so we can make it sing
        bridgerectifier = fabs(midSampleR);
        bridgerectifier += skew;
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
        bridgerectifier = sin(bridgerectifier);
        bridgerectifier *= drive;
        bridgerectifier += skew;
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
        bridgerectifier = sin(bridgerectifier);
        if (midSampleR > 0)
        {
            midSampleR = bridgerectifier;
        }
        else
        {
            midSampleR = -bridgerectifier;
        }
        //blend according to positive and negative controls, right

        inputSampleL = ((lowSampleL + midSampleL + highSampleL)/drive)*out;
        inputSampleR = ((lowSampleR + midSampleR + highSampleR)/drive)*out;

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
void DrumSlam::process(float** inputs, float** outputs, int frames) noexcept
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
