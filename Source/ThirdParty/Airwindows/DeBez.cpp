/* ========================================
 *  DeBez - DeBez.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) Airwindows, Airwindows uses the MIT license
 * ======================================== */
#include "DeBez.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace lodo::airwindows::DeBezDSP
{
void DeBez::reset() noexcept
{
    static_assert(std::is_trivially_copyable_v<History>);
    std::memset(static_cast<History*>(this), 0, sizeof(History));
    block = {};





    for (int x = 0; x < bez_total; x++) bezF[x] = 0.0;
    bezF[bez_cycle] = 1.0;

    rezA = 0.5; rezB = 0.5;
    bitA = 0.5; bitB = 0.5;
    wetA = 1.0; wetB = 1.0;

    fpdL = 0x6d2b79f5u;
    fpdR = 0xa341316cu;
    //this is reset: values being initialized only once. Startup values, whatever they are.


}
void DeBez::beginBlock(int frames) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    sampleFrames = frames;
    auto& inFramesToProcess = block.inFramesToProcess;
    auto& overallscale = block.overallscale;
    auto& steppedFreq = block.steppedFreq;
    auto& bezFreqFraction = block.bezFreqFraction;
    auto& bezFreqTrim = block.bezFreqTrim;






    inFramesToProcess = sampleFrames; //vst doesn't give us this as a separate variable so we'll make it
    overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    rezA = rezB; rezB = A*2.0;
    bitA = bitB; bitB = B*2.0;
    wetA = wetB; wetB = C*2.0;

    steppedFreq = true; // Revised Bezier Undersampling
    if (rezB > 1.0) {  // has full rez at center, stepped
        steppedFreq = false; // to left, continuous to right
        rezB = 1.0-(rezB-1.0);
    } //if it's set up like that it's the revised algorithm
    rezB = fmin(fmax(pow(rezB,3.0),0.0005),1.0); //note: no overallscale, already inside undersampling
    bezFreqFraction = (int)(1.0/rezB);
    bezFreqTrim = (double)bezFreqFraction/(bezFreqFraction+1.0);
    if (steppedFreq) { //this hard-locks derez to exact subdivisions of 1.0
        rezB = 1.0 / bezFreqFraction;
        bezFreqTrim = 1.0-(rezB*bezFreqTrim);
    } else { //this makes it match the 1.0 case using stepped
        bezFreqTrim = 1.0-pow(rezB*0.5,1.0/(rezB*0.5));
    } //the revision more accurately connects the bezier curves


}
void DeBez::processFrame(float& left, float& right) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    --sampleFrames;
    auto& inFramesToProcess = block.inFramesToProcess;
    auto& overallscale = block.overallscale;
    auto& steppedFreq = block.steppedFreq;
    auto& bezFreqFraction = block.bezFreqFraction;
    auto& bezFreqTrim = block.bezFreqTrim;

        double inputSampleL = left;
        double inputSampleR = right;
        if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
        double drySampleL = inputSampleL;
        double drySampleR = inputSampleR;
        double temp = (double)sampleFrames/inFramesToProcess;
        double rez = (rezA*temp)+(rezB*(1.0-temp));
        double bit = ((bitA*temp)+(bitB*(1.0-temp)))-1.0;
        double wet = ((wetA*temp)+(wetB*(1.0-temp)))-1.0;
        double dry = 1.0 - wet;
        if (wet > 1.0) wet = 1.0;
        if (wet < -1.0) wet = -1.0;
        if (dry > 1.0) dry = 1.0;
        if (dry < 0.0) dry = 0.0;
        //this bitcrush can be subtracted.

        bezF[bez_cycle] += rez;
        bezF[bez_SampL] += (inputSampleL * rez);
        bezF[bez_SampR] += (inputSampleR * rez);
        if (bezF[bez_cycle] > 1.0) {
            if (steppedFreq) bezF[bez_cycle] = 0.0;
            else bezF[bez_cycle] -= 1.0;

            inputSampleL = (bezF[bez_SampL]+bezF[bez_AvgInSampL])*0.5;
            bezF[bez_AvgInSampL] = bezF[bez_SampL];
            inputSampleR = (bezF[bez_SampR]+bezF[bez_AvgInSampR])*0.5;
            bezF[bez_AvgInSampR] = bezF[bez_SampR];

            bool crushGate = (bit < 0.0);
            bit = 1.0-fabs(bit);
            bit = fmin(fmax(bit*16.0,0.5),16.0);
            double bitFactor = pow(2.0,bit);
            inputSampleL *= bitFactor;
            inputSampleL = floor(inputSampleL+(crushGate?0.5/bitFactor:0.0));
            inputSampleL /= bitFactor;
            inputSampleR *= bitFactor;
            inputSampleR = floor(inputSampleR+(crushGate?0.5/bitFactor:0.0));
            inputSampleR /= bitFactor;
            //derez inside debez
            bezF[bez_CL] = bezF[bez_BL];
            bezF[bez_BL] = bezF[bez_AL];
            bezF[bez_AL] = inputSampleL;
            bezF[bez_SampL] = 0.0;
            bezF[bez_CR] = bezF[bez_BR];
            bezF[bez_BR] = bezF[bez_AR];
            bezF[bez_AR] = inputSampleR;
            bezF[bez_SampR] = 0.0;
        }
        double X = bezF[bez_cycle]*bezFreqTrim;
        double CBLfreq = (bezF[bez_CL]*(1.0-X))+(bezF[bez_BL]*X);
        double BALfreq = (bezF[bez_BL]*(1.0-X))+(bezF[bez_AL]*X);
        double CBALfreq = (bezF[bez_BL]+(CBLfreq*(1.0-X))+(BALfreq*X))*0.125;
        inputSampleL = CBALfreq+bezF[bez_AvgOutSampL]; bezF[bez_AvgOutSampL] = CBALfreq;

        double CBRfreq = (bezF[bez_CR]*(1.0-X))+(bezF[bez_BR]*X);
        double BARfreq = (bezF[bez_BR]*(1.0-X))+(bezF[bez_AR]*X);
        double CBARfreq = (bezF[bez_BR]+(CBRfreq*(1.0-X))+(BARfreq*X))*0.125;
        inputSampleR = CBARfreq+bezF[bez_AvgOutSampR]; bezF[bez_AvgOutSampR] = CBARfreq;

        inputSampleL = (wet*inputSampleL)+(dry*drySampleL);
        inputSampleR = (wet*inputSampleR)+(dry*drySampleR);

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
void DeBez::process(float** inputs, float** outputs, int frames) noexcept
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
