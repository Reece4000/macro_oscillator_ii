/* ========================================
 *  Tube2 - Tube2.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
#include "Tube2.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace lodo::airwindows::Tube2DSP
{
void Tube2::reset() noexcept
{
    static_assert(std::is_trivially_copyable_v<History>);
    std::memset(static_cast<History*>(this), 0, sizeof(History));
    block = {};



    previousSampleA = 0.0;
    previousSampleB = 0.0;
    previousSampleC = 0.0;
    previousSampleD = 0.0;
    previousSampleE = 0.0;
    previousSampleF = 0.0;
    fpdL = 0x6d2b79f5u;
    fpdR = 0xa341316cu;
    //this is reset: values being initialized only once. Startup values, whatever they are.


}
void Tube2::beginBlock(int frames) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    sampleFrames = frames;
    auto& overallscale = block.overallscale;
    auto& inputPad = block.inputPad;
    auto& iterations = block.iterations;
    auto& powerfactor = block.powerfactor;
    auto& asymPad = block.asymPad;
    auto& gainscaling = block.gainscaling;
    auto& outputscaling = block.outputscaling;






    overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    inputPad = A;
    iterations = 1.0-B;
    powerfactor = (9.0*iterations)+1;
    asymPad = (double)powerfactor;
    gainscaling = 1.0/(double)(powerfactor+1);
    outputscaling = 1.0 + (1.0/(double)(powerfactor));


}
void Tube2::processFrame(float& left, float& right) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    --sampleFrames;
    auto& overallscale = block.overallscale;
    auto& inputPad = block.inputPad;
    auto& iterations = block.iterations;
    auto& powerfactor = block.powerfactor;
    auto& asymPad = block.asymPad;
    auto& gainscaling = block.gainscaling;
    auto& outputscaling = block.outputscaling;

        double inputSampleL = left;
        double inputSampleR = right;
        if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;

        if (inputPad < 1.0) {
            inputSampleL *= inputPad;
            inputSampleR *= inputPad;
        }

        if (overallscale > 1.9) {
            double stored = inputSampleL;
            inputSampleL += previousSampleA; previousSampleA = stored; inputSampleL *= 0.5;
            stored = inputSampleR;
            inputSampleR += previousSampleB; previousSampleB = stored; inputSampleR *= 0.5;
        } //for high sample rates on this plugin we are going to do a simple average

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;

        //flatten bottom, point top of sine waveshaper L
        inputSampleL /= asymPad;
        double sharpen = -inputSampleL;
        if (sharpen > 0.0) sharpen = 1.0+sqrt(sharpen);
        else sharpen = 1.0-sqrt(-sharpen);
        inputSampleL -= inputSampleL*fabs(inputSampleL)*sharpen*0.25;
        //this will take input from exactly -1.0 to 1.0 max
        inputSampleL *= asymPad;
        //flatten bottom, point top of sine waveshaper R
        inputSampleR /= asymPad;
        sharpen = -inputSampleR;
        if (sharpen > 0.0) sharpen = 1.0+sqrt(sharpen);
        else sharpen = 1.0-sqrt(-sharpen);
        inputSampleR -= inputSampleR*fabs(inputSampleR)*sharpen*0.25;
        //this will take input from exactly -1.0 to 1.0 max
        inputSampleR *= asymPad;
        //end first asym section: later boosting can mitigate the extreme
        //softclipping of one side of the wave
        //and we are asym clipping more when Tube is cranked, to compensate

        //original Tube algorithm: powerfactor widens the more linear region of the wave
        double factor = inputSampleL; //Left channel
        for (int x = 0; x < powerfactor; x++) factor *= inputSampleL;
        if ((powerfactor % 2 == 1) && (inputSampleL != 0.0)) factor = (factor/inputSampleL)*fabs(inputSampleL);
        factor *= gainscaling;
        inputSampleL -= factor;
        inputSampleL *= outputscaling;
        factor = inputSampleR; //Right channel
        for (int x = 0; x < powerfactor; x++) factor *= inputSampleR;
        if ((powerfactor % 2 == 1) && (inputSampleR != 0.0)) factor = (factor/inputSampleR)*fabs(inputSampleR);
        factor *= gainscaling;
        inputSampleR -= factor;
        inputSampleR *= outputscaling;

        if (overallscale > 1.9) {
            double stored = inputSampleL;
            inputSampleL += previousSampleC; previousSampleC = stored; inputSampleL *= 0.5;
            stored = inputSampleR;
            inputSampleR += previousSampleD; previousSampleD = stored; inputSampleR *= 0.5;
        } //for high sample rates on this plugin we are going to do a simple average
        //end original Tube. Now we have a boosted fat sound peaking at 0dB exactly

        //hysteresis and spiky fuzz L
        double slew = previousSampleE - inputSampleL;
        if (overallscale > 1.9) {
            double stored = inputSampleL;
            inputSampleL += previousSampleE; previousSampleE = stored; inputSampleL *= 0.5;
        } else previousSampleE = inputSampleL; //for this, need previousSampleC always
        if (slew > 0.0) slew = 1.0+(sqrt(slew)*0.5);
        else slew = 1.0-(sqrt(-slew)*0.5);
        inputSampleL -= inputSampleL*fabs(inputSampleL)*slew*gainscaling;
        //reusing gainscaling that's part of another algorithm
        if (inputSampleL > 0.52) inputSampleL = 0.52;
        if (inputSampleL < -0.52) inputSampleL = -0.52;
        inputSampleL *= 1.923076923076923;
        //hysteresis and spiky fuzz R
        slew = previousSampleF - inputSampleR;
        if (overallscale > 1.9) {
            double stored = inputSampleR;
            inputSampleR += previousSampleF; previousSampleF = stored; inputSampleR *= 0.5;
        } else previousSampleF = inputSampleR; //for this, need previousSampleC always
        if (slew > 0.0) slew = 1.0+(sqrt(slew)*0.5);
        else slew = 1.0-(sqrt(-slew)*0.5);
        inputSampleR -= inputSampleR*fabs(inputSampleR)*slew*gainscaling;
        //reusing gainscaling that's part of another algorithm
        if (inputSampleR > 0.52) inputSampleR = 0.52;
        if (inputSampleR < -0.52) inputSampleR = -0.52;
        inputSampleR *= 1.923076923076923;
        //end hysteresis and spiky fuzz section

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
void Tube2::process(float** inputs, float** outputs, int frames) noexcept
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
