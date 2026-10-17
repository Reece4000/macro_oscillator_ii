#include "DistortionAmountProfiles.h"
#include <algorithm>
#include <cmath>

namespace lodo::engine::fx
{
DistortionAmountProfile distortionAmountProfile(int type, float amount) noexcept
{
    const float a = std::clamp(amount, 0.0f, 1.0f);
    const float square = a * a;
    // Preserve the established gentle range; extend drive above 25% Amount.
    const float upper = std::max(0.0f, (a - .25f) / .75f);
    const float push = upper * upper;
    DistortionAmountProfile result;
    // Imported processors can colour audio even at their least intense setting.
    // Ease into that character over the first tenth of Amount, with exact dry
    // output at zero. This is ordinary host dry/wet control, not new sample DSP.
    const float entry = std::min(1.0f, a * 10.0f);
    result.engagement = entry * entry * (3.0f - 2.0f * entry);
    switch (type)
    {
        case 5: // Drive: saturation with slight bass tightening and gain trim.
            result.native = {a, .04f * a, 1.0f / (1.0f + 2.0f * a), 1.0f};
            result.inputGain = 1.0f + 15.0f * push;
            result.wetTrim = 1.0f / (1.0f + 3.0f * upper); break;
        case 6: // Fracture: increase drive and the folding range together.
            result.native = {.25f + .75f * a, .75f * a, 1.0f / (1.0f + a), 1.0f}; break;
        case 7: // ToTape9: saturation and wear, without stereo/pitch movement.
            // Flutter is a performance FX concern. Keep both native controls
            // off. Return bias to neutral at full Amount so its slew limiting
            // does not soften away the strongly driven saturation.
            result.native = {.5f + .3f * a, .5f + .09f * a, .5f + .08f * a,
                0.0f, 0.0f, .5f + .18f * square * (1.0f - push),
                .18f * a, .5f - .12f * a, .5f - .12f * a};
            result.inputGain = 1.0f + 3.0f * square + 28.0f * push;
            result.wetTrim = 1.0f / std::sqrt(1.0f + 3.0f * square)
                / (1.0f + (2.0f / 3.0f) * upper); break;
        case 8: // IronOxide: hotter input, slower tape, compensating output.
            result.native = {.5f + .4f * a, .65f - .5f * a, .5f - .22f * a}; break;
        case 9: // Density3: its neutral density is .2, not the native zero.
            result.native = {.2f + .8f * a, .02f * a, 1.0f, 1.0f};
            result.inputGain = 1.0f + 15.0f * square;
            result.wetTrim = 1.0f / ((1.0f + 5.0f * a) * std::sqrt(result.inputGain) * (1.0f + 1.3f * a));
            // Density already saturates strongly; its old compensation hid it.
            result.wetTrim += push * (.125f - result.wetTrim); break;
        case 10: // Spiral2: drive plus presence and a small highpass.
            result.native = {.5f + .5f * a, .02f * a, .4f * a, 1.0f, 1.0f};
            result.inputGain = 1.0f + 7.0f * std::sqrt(a) + 8.0f * push;
            result.wetTrim = 1.0f / ((1.0f + 3.0f * a) * result.inputGain);
            // Folding reduces amplitude as input rises; inverse drive trim
            // made the upper range almost silent. Keep useful wet level.
            result.wetTrim += push * (.5f - result.wetTrim); break;
        case 11: // Tube2: the native input pad stays at unity.
            result.native = {1.0f, .9f * a};
            result.inputGain = 1.0f + 19.0f * square + 44.0f * push;
            result.wetTrim = 1.0f / (2.0f * std::sqrt(1.0f + 19.0f * square) * (1.0f + a));
            result.wetTrim += push * (.2f - result.wetTrim); break;
        case 12: // Mackity: square-law input gain, with output pad compensation.
            result.native = {.1f + .6f * a, 1.0f / (1.0f + 3.0f * a)}; break;
        case 13: // Coils2: transformer saturation and cheaper transformer voicing.
            // Increasing input alone eventually buries the bounded transformer
            // residual under the clean bandpass. Emphasize that native residual
            // in the upper range, while reducing the excessive input drive.
            result.native = {.15f + .35f * a, .95f, 1.0f};
            result.inputGain = 1.0f + 39.0f * a - 16.0f * push;
            result.saturationStrength = 1.0f + 11.0f * push;
            result.wetTrim = 1.0f / result.inputGain; break;
        case 14: // FireAmp: drive into a progressively darker amp/cab voicing.
            result.native = {.15f + .8f * a, .65f - .3f * a, .8f, 1.0f}; break;
        case 15: // DrumSlam: multiband drive with output compensation.
            result.native = {a, 1.0f / (1.0f + 1.5f * a), 1.0f};
            result.inputGain = 1.0f + 7.0f * push;
            result.wetTrim = 1.0f / (1.0f + (2.0f / 3.0f) * upper); break;
        case 16: // BitGlitter: glitter/undersampling, hotter quantizer input.
            result.native = {.5f + .2f * a, a, .5f - .2f * a, 1.0f}; break;
        case 17: // DeBez: smooth bit crushing, with native Bezier smoothing.
            // Keep DeRez at full rate: sweeping it toward zero averages away
            // almost the entire signal. Amount reduces 16-bit to 3.52-bit.
            result.native = {.5f, .5f - .39f * a, 1.0f}; break;
        default: break;
    }
    return result;
}
}
