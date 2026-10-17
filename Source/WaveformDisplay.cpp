#include "PluginEditor.h"
#include "EditorParameters.h"

#include "DSP/PlaitsVoice.h"

namespace macro_osc_ii
{
namespace
{
// Plaits renders at about 47.9 kHz in 12-sample blocks; C3 gives a 366-sample period.
constexpr int previewNote = 48;
constexpr int warmUpSamples = 4800;
constexpr int strikeSamples = 14400;

[[nodiscard]] float periodSamples()
{
    const float frequency = 440.0f * std::pow (2.0f, (static_cast<float> (previewNote) - 69.0f) / 12.0f);
    return static_cast<float> (PlaitsEngine::nativeSampleRate()) / frequency;
}

}

WaveformDisplay::WaveformDisplay (MacroOscIIAudioProcessor& processor)
    : audioProcessor (processor)
{
    setInterceptsMouseClicks (true, false);
    setTitle ("Oscillator waveform");
    setTooltip ("The selected model at its base Harmonics, Timbre, Morph and Aux. Modulation and effects are not shown.");
    refresh();
}

void WaveformDisplay::refresh()
{
    const std::array<float, 5> parameters {
        editor::rawValue (audioProcessor, ParamIDs::model),
        editor::rawValue (audioProcessor, ParamIDs::harmonics),
        editor::rawValue (audioProcessor, ParamIDs::timbre),
        editor::rawValue (audioProcessor, ParamIDs::morph),
        editor::rawValue (audioProcessor, ParamIDs::auxMix)
    };
    if (parameters == lastParameters)
        return;
    lastParameters = parameters;

    const int model = juce::roundToInt (parameters[0]);
    // Struck models show their whole envelope.
    envelopeView = isStruckModel (model);
    // The engine's working memory is large; keep it off the message thread's stack.
    auto engine = std::make_unique<PlaitsEngine>();
    PlaitsEngine::Controls controls;
    controls.engine = kPlaitsModels[static_cast<size_t> (juce::jlimit (0, kModelCount - 1, model))].engine;
    controls.note = static_cast<float> (previewNote);
    controls.harmonics = parameters[1];
    controls.timbre = parameters[2];
    controls.morph = parameters[3];
    controls.gate = true;

    const float period = periodSamples();
    const int total = envelopeView ? strikeSamples : warmUpSamples + juce::roundToInt (period * 4.0f);
    std::vector<float> samples (static_cast<size_t> (total + PlaitsEngine::kBlockSize));
    std::array<float, PlaitsEngine::kBlockSize> out {}, aux {};
    for (int i = 0; i < total; i += PlaitsEngine::kBlockSize)
    {
        engine->render (controls, out.data(), aux.data());
        for (size_t j = 0; j < out.size(); ++j)
            samples[static_cast<size_t> (i) + j] = out[j] + (aux[j] - out[j]) * parameters[4];
    }

    // Steady models start on a rising zero crossing and show two and a half cycles.
    int start = 0, length = total;
    if (! envelopeView)
    {
        start = warmUpSamples;
        for (int i = warmUpSamples; i < warmUpSamples + juce::roundToInt (period); ++i)
            if (samples[static_cast<size_t> (i)] <= 0.0f && samples[static_cast<size_t> (i + 1)] > 0.0f)
            {
                start = i;
                break;
            }
        length = juce::roundToInt (period * 2.5f);
    }

    const int columns = juce::jmax (1, getWidth() > 0 ? getWidth() : 560);
    minimums.assign (static_cast<size_t> (columns), 0.0f);
    maximums.assign (static_cast<size_t> (columns), 0.0f);
    float peak = 0.05f;
    for (int i = start; i < start + length; ++i)
        peak = juce::jmax (peak, std::abs (samples[static_cast<size_t> (i)]));
    for (int column = 0; column < columns; ++column)
    {
        const int from = start + column * length / columns;
        const int to = juce::jmax (from + 1, start + (column + 1) * length / columns);
        float low = 1.0f, high = -1.0f;
        for (int i = from; i < to; ++i)
        {
            const float value = samples[static_cast<size_t> (i)] / peak;
            low = juce::jmin (low, value);
            high = juce::jmax (high, value);
        }
        // A steady wave reads as one line; a strike reads as its filled envelope.
        if (! envelopeView)
            low = high = samples[static_cast<size_t> (from)] / peak;
        minimums[static_cast<size_t> (column)] = low;
        maximums[static_cast<size_t> (column)] = high;
    }
    repaint();
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    ui::drawDotGrid (g, bounds);
    g.setColour (ui::faint);
    g.fillRect (bounds.withSizeKeepingCentre (bounds.getWidth(), 1.0f));
    if (minimums.empty())
        return;

    const float scale = bounds.getHeight() * 0.44f;
    const float step = bounds.getWidth() / static_cast<float> (minimums.size());
    const auto yFor = [&] (float value) { return bounds.getCentreY() - value * scale; };
    if (envelopeView)
    {
        juce::Path envelope;
        envelope.startNewSubPath (0.0f, yFor (maximums.front()));
        for (size_t i = 1; i < maximums.size(); ++i)
            envelope.lineTo (static_cast<float> (i) * step, yFor (maximums[i]));
        for (size_t i = minimums.size(); i-- > 0;)
            envelope.lineTo (static_cast<float> (i) * step, yFor (minimums[i]));
        envelope.closeSubPath();
        g.setColour (ui::turquoise.withAlpha (0.35f));
        g.fillPath (envelope);
        g.setColour (ui::turquoiseLine);
        g.strokePath (envelope, juce::PathStrokeType (1.2f));
        return;
    }
    juce::Path wave;
    wave.startNewSubPath (0.0f, yFor (maximums.front()));
    for (size_t i = 1; i < maximums.size(); ++i)
        wave.lineTo (static_cast<float> (i) * step, yFor (maximums[i]));
    g.setColour (ui::turquoiseLine);
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace macro_osc_ii
