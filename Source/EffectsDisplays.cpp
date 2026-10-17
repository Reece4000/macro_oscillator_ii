#include "EffectsDisplays.h"
#include "UITheme.h"

namespace macro_osc_ii
{
namespace
{
float value (MacroOscIIAudioProcessor& processor, const char* id)
{
    return processor.getState().getRawParameterValue (id)->load (std::memory_order_relaxed);
}

void drawBypassed (juce::Graphics& g, const juce::Font& font, juce::Rectangle<float> bounds)
{
    g.setFont (font.withPointHeight (11.0f));
    g.setColour (ui::muted);
    g.drawText ("bypassed", bounds, juce::Justification::centred);
}
}

DistortionDisplay::DistortionDisplay (MacroOscIIAudioProcessor& owner, juce::Font labelFont)
    : processor (owner), font (std::move (labelFont))
{
    setInterceptsMouseClicks (true, false);
    setTitle ("Distortion response to a 100 Hz reference tone");
    setTooltip ("A 100 Hz reference through the selected distortion at its base settings. This is not a live audio meter.");
    preview.prepare (48000.0);
    refresh();
}

void DistortionDisplay::refresh()
{
    const std::array<float, 4> parameters {
        value (processor, ParamIDs::distortionType), value (processor, ParamIDs::distortionAmount),
        value (processor, ParamIDs::distortionMix), value (processor, ParamIDs::distortionOutput)
    };
    if (parameters == lastParameters) return;
    lastParameters = parameters;
    preview.reset();
    preview.setType (juce::roundToInt (parameters[0]), true);
    preview.setParameters ({ { parameters[1], parameters[2], parameters[3] } });
    peak = 0.0f;
    // Render a real reference through the selected kernel, including its history,
    // Mix and wet Output. This runs only on the message thread when controls move.
    for (int i = 0; i < 4800 + static_cast<int> (waveform.size()); ++i)
    {
        if (i % 24 == 0) preview.beginBlock (24);
        float left = 0.65f * std::sin (juce::MathConstants<float>::twoPi * static_cast<float> (i % 480) / 480.0f);
        float right = left;
        preview.processStereo (left, right);
        if (i >= 4800)
        {
            const float sample = std::isfinite (left) ? juce::jlimit (-4.0f, 4.0f, left) : 0.0f;
            waveform[static_cast<size_t> (i - 4800)] = sample;
            peak = juce::jmax (peak, std::abs (sample));
        }
    }
    repaint();
}

void DistortionDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    ui::drawDotGrid (g, bounds);
    if (value (processor, ParamIDs::distortionType) == 0.0f)
    {
        drawBypassed (g, font, bounds);
        return;
    }
    const auto plot = bounds.reduced (0.0f, 3.0f);
    const float scale = plot.getHeight() * 0.48f / juce::jmax (1.0f, peak);
    juce::Path dry, wet;
    for (size_t i = 0; i < waveform.size(); ++i)
    {
        const float x = plot.getX() + plot.getWidth() * static_cast<float> (i) / static_cast<float> (waveform.size() - 1);
        const float input = 0.65f * std::sin (juce::MathConstants<float>::twoPi * static_cast<float> (i % 480) / 480.0f);
        const float y = plot.getCentreY() - scale * waveform[i];
        if (i == 0) { dry.startNewSubPath (x, plot.getCentreY() - input * scale); wet.startNewSubPath (x, y); }
        else { dry.lineTo (x, plot.getCentreY() - input * scale); wet.lineTo (x, y); }
    }
    g.setColour (ui::muted.withAlpha (0.5f));
    g.strokePath (dry, juce::PathStrokeType (1.0f));
    g.setColour (ui::magenta);
    g.strokePath (wet, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

FilterDisplay::FilterDisplay (MacroOscIIAudioProcessor& owner, juce::Font labelFont)
    : processor (owner), font (std::move (labelFont))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setTitle ("Filter response");
    setDescription ("Drag horizontally for frequency and vertically for Q. The Frequency and Q knobs also support keyboard adjustment.");
    setTooltip ("Base filter response. Drag horizontally for frequency and vertically for Q. MSEG modulation is applied per voice.");
}

FilterDisplay::~FilterDisplay() { endGesture(); }

juce::Rectangle<float> FilterDisplay::plotBounds() const
{
    return getLocalBounds().toFloat().reduced (0.0f, 2.0f);
}

void FilterDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    ui::drawDotGrid (g, bounds);
    const auto plot = plotBounds();
    const int type = juce::roundToInt (value (processor, ParamIDs::filterType));
    if (type == 0)
    {
        drawBypassed (g, font, bounds);
        return;
    }
    const float cutoff = value (processor, ParamIDs::filterFrequency);
    const float q = value (processor, ParamIDs::filterQ);
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const float frequencyLimit = static_cast<float> (sampleRate * 0.499);
    const auto xFor = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); };
    const auto yFor = [&] (float db) { return plot.getBottom() - (juce::jlimit (-36.0f, 30.0f, db) + 36.0f) * plot.getHeight() / 66.0f; };
    juce::Path response;
    for (int i = 0; i <= 240; ++i)
    {
        const float hz = 20.0f * std::pow (1000.0f, i / 240.0f);
        // Match the audio stage's bilinear SVF and Nyquist clamp.
        const float w = static_cast<float> (
            std::tan (juce::MathConstants<double>::pi * juce::jmin (hz, frequencyLimit) / sampleRate)
            / std::tan (juce::MathConstants<double>::pi * juce::jmin (cutoff, frequencyLimit) / sampleRate));
        const float denominator = std::sqrt (std::pow (1.0f - w * w, 2.0f) + (w * w) / (q * q));
        const float gain = (type == 2 ? w : type == 3 ? w * w : 1.0f) / juce::jmax (1.0e-8f, denominator);
        const float x = plot.getX() + plot.getWidth() * i / 240.0f;
        const float y = yFor (20.0f * std::log10 (juce::jmax (1.0e-6f, gain)));
        if (i == 0) response.startNewSubPath (x, y); else response.lineTo (x, y);
    }
    auto fill = response;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setColour (ui::turquoise.withAlpha (0.22f));
    g.fillPath (fill);
    g.setColour (ui::turquoiseLine);
    g.strokePath (response, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const juce::Point<float> handle { xFor (cutoff), yFor (20.0f * std::log10 (q)) };
    const auto square = juce::Rectangle<float> (8.0f, 8.0f).withCentre (handle);
    if (dragging || isMouseOver())
    {
        g.setColour (ui::turquoise.withAlpha (0.22f));
        g.fillRect (square.expanded (4.0f));
    }
    g.setColour (ui::background);
    g.fillRect (square);
    g.setColour (ui::turquoiseLine);
    g.drawRect (square, 2.0f);
}

void FilterDisplay::setFromPosition (juce::Point<float> point)
{
    const auto plot = plotBounds();
    const float x = juce::jlimit (0.0f, 1.0f, (point.x - plot.getX()) / plot.getWidth());
    const float db = juce::jlimit (0.0f, 20.0f * std::log10 (30.0f),
                                  (plot.getBottom() - point.y) * 66.0f / plot.getHeight() - 36.0f);
    processor.getState().getParameter (ParamIDs::filterFrequency)->setValueNotifyingHost (x);
    auto* qParameter = processor.getState().getParameter (ParamIDs::filterQ);
    qParameter->setValueNotifyingHost (qParameter->convertTo0to1 (std::pow (10.0f, db / 20.0f)));
    repaint();
}

void FilterDisplay::mouseDown (const juce::MouseEvent& event)
{
    for (const auto* id : { ParamIDs::filterFrequency, ParamIDs::filterQ })
        processor.getState().getParameter (id)->beginChangeGesture();
    dragging = true;
    setFromPosition (event.position);
}

void FilterDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (dragging) setFromPosition (event.position);
}

void FilterDisplay::endGesture()
{
    if (! dragging) return;
    for (const auto* id : { ParamIDs::filterFrequency, ParamIDs::filterQ })
        processor.getState().getParameter (id)->endChangeGesture();
    dragging = false;
}

void FilterDisplay::mouseUp (const juce::MouseEvent&) { endGesture(); repaint(); }
}
