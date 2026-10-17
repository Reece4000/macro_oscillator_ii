/* ========================================
 *  Fracture - Fracture.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */
#include "Fracture.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace lodo::airwindows::FractureDSP
{
void Fracture::reset() noexcept
{
    static_assert(std::is_trivially_copyable_v<History>);
    std::memset(static_cast<History*>(this), 0, sizeof(History));
    block = {};

     //1 from 0 to 4: A*4
     //2 from 1 to 3: (B*2.999)+1


    fpdL = 0x6d2b79f5u;
    fpdR = 0xa341316cu;
    //this is reset: values being initialized only once. Startup values, whatever they are.


}
void Fracture::beginBlock(int frames) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    sampleFrames = frames;
    auto& density = block.density;
    auto& fracture = block.fracture;
    auto& output = block.output;
    auto& wet = block.wet;
    auto& dry = block.dry;
    auto& bridgerectifier = block.bridgerectifier;
    auto& inputSampleL = block.inputSampleL;
    auto& inputSampleR = block.inputSampleR;
    auto& drySampleL = block.drySampleL;
    auto& drySampleR = block.drySampleR;







    density = A*4;
    fracture = (((B*2.999)+1)*3.14159265358979);
    output = C;
    wet = D;
    dry = 1.0-wet;

    density = density * fabs(density);







}
void Fracture::processFrame(float& left, float& right) noexcept
{
    auto& sampleFrames = block.sampleFrames;
    --sampleFrames;
    auto& density = block.density;
    auto& fracture = block.fracture;
    auto& output = block.output;
    auto& wet = block.wet;
    auto& dry = block.dry;
    auto& bridgerectifier = block.bridgerectifier;
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

        inputSampleL *= density;
        inputSampleR *= density;

        bridgerectifier = fabs(inputSampleL)*fracture;
        if (bridgerectifier > fracture) bridgerectifier = fracture;
        //max value for sine function
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleL > 0) inputSampleL = bridgerectifier;
        else inputSampleL = -bridgerectifier;
        //blend according to density control

        bridgerectifier = fabs(inputSampleR)*fracture;
        if (bridgerectifier > fracture) bridgerectifier = fracture;
        //max value for sine function
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleR > 0) inputSampleR = bridgerectifier;
        else inputSampleR = -bridgerectifier;
        //blend according to density control

        inputSampleL *= output;
        inputSampleR *= output;

        inputSampleL = (drySampleL * dry)+(inputSampleL * wet);
        inputSampleR = (drySampleR * dry)+(inputSampleR * wet);

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
void Fracture::process(float** inputs, float** outputs, int frames) noexcept
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
