#include "PluginEditor.h"
#include "EditorParameters.h"

namespace macro_osc_ii
{
using namespace editor;

namespace
{
constexpr float graphicHeight = 28.0f;
constexpr float rowLabelWidth = 58.0f;
constexpr float rowValueWidth = 52.0f;
constexpr float dragSensitivity = 180.0f;
constexpr float fineDragSensitivity = 1800.0f;
}

ParameterSlider::ParameterSlider (MacroOscIIAudioProcessor& processor, EditorFonts editorFonts)
    : audioProcessor (processor), fonts (std::move (editorFonts))
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    setWantsKeyboardFocus (true);
}

void ParameterSlider::configure (const juce::String& labelText,
                                 const char* parameterIdToUse,
                                 juce::Colour accent,
                                 const juce::String& unitText,
                                 int decimals,
                                 float displayMultiplier)
{
    label = labelText.toLowerCase();
    parameterId = parameterIdToUse;
    if (auto* value = parameter())
        setTitle (value->getName (80));
    setDescription ("Drag or use arrow keys to adjust; Shift for fine adjustment. Double-click or Backspace to reset.");
    setTooltip (getTitle() + ": drag or use arrow keys. Hold Shift for fine adjustment; double-click to reset.");
    accentColour = accent;
    unit = unitText;
    decimalPlaces = decimals;
    multiplier = displayMultiplier;
    repaint();
}

juce::RangedAudioParameter* ParameterSlider::parameter() const
{
    return parameterFor (audioProcessor, parameterId);
}

float ParameterSlider::parameterValue() const
{
    return parameterId != nullptr ? rawValue (audioProcessor, parameterId) : 0.0f;
}

juce::String ParameterSlider::displayValue() const
{
    if (formatter)
        return formatter (parameterValue());
    const float value = parameterValue() * multiplier;
    if (unit == "s")
        return value < 0.9995f ? juce::String (juce::roundToInt (value * 1000.0f)) + " ms"
                            : juce::String (value, 2) + " s";
    if (unit == "Hz")
        return value >= 1000.0f ? juce::String (value / 1000.0f, 2) + " kHz"
                                : juce::String (juce::roundToInt (value)) + " Hz";
    const auto number = decimalPlaces <= 0 ? juce::String (juce::roundToInt (value)) : juce::String (value, decimalPlaces);
    if (unit == "%")
        return number + "%";
    return unit.isEmpty() ? number : number + " " + unit;
}

juce::Rectangle<float> ParameterSlider::trackBounds() const
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f, 0.0f);
    if (layout == Layout::row)
    {
        const float labelWidth = label.isEmpty() ? 0.0f : rowLabelWidth;
        return bounds.withTrimmedLeft (labelWidth).withTrimmedRight (rowValueWidth + 12.0f)
                     .withSizeKeepingCentre (bounds.getWidth() - labelWidth - rowValueWidth - 12.0f, 6.0f);
    }
    const float top = (graphic != Graphic::none ? graphicHeight + 8.0f : 0.0f) + 18.0f + 8.0f;
    return bounds.withY (top).withHeight (6.0f);
}

void ParameterSlider::paint (juce::Graphics& g)
{
    const auto* value = parameter();
    const float normalised = value != nullptr ? value->getValue() : 0.0f;
    float origin = 0.0f;
    if (value != nullptr)
    {
        const auto& range = value->getNormalisableRange();
        if (range.start < 0.0f && range.end > 0.0f)
            origin = range.convertTo0to1 (0.0f);
    }
    const bool hot = dragging || isMouseOver();
    const auto text = onBand ? ui::stripInk : ui::ink;
    const auto subtle = onBand ? ui::stripMuted : ui::muted;
    // Keyboard focus lights the label rather than boxing the control.
    const bool focused = hasKeyboardFocus (true) && focusVisible;
    const auto labelColour = focused ? (onBand ? ui::turquoise : ui::turquoiseLine) : text;
    auto bounds = getLocalBounds().toFloat();

    if (layout == Layout::row)
    {
        g.setFont (fonts.medium.withPointHeight (14.0f));
        g.setColour (labelColour);
        g.drawText (label, bounds.removeFromLeft (label.isEmpty() ? 0.0f : rowLabelWidth), juce::Justification::centredLeft);
        g.setFont (fonts.mono.withPointHeight (12.0f));
        g.setColour (hot ? text : subtle);
        g.drawText (displayValue(), bounds.removeFromRight (rowValueWidth), juce::Justification::centredRight);
    }
    else
    {
        if (graphic != Graphic::none)
        {
            paintGraphic (g, bounds.removeFromTop (graphicHeight), normalised);
            bounds.removeFromTop (8.0f);
        }
        auto row = bounds.removeFromTop (18.0f);
        g.setFont (fonts.medium.withPointHeight (14.0f));
        g.setColour (labelColour);
        g.drawText (label, row, juce::Justification::bottomLeft);
        g.setFont (fonts.mono.withPointHeight (12.0f));
        g.setColour (hot ? text : subtle);
        g.drawText (graphic == Graphic::readout ? unit : displayValue(), row.translated (0.0f, -1.0f),
                    juce::Justification::bottomRight);
    }

    const auto track = trackBounds();
    const auto xFor = [&track] (float position) { return track.getX() + track.getWidth() * position; };
    auto trackColour = onBand ? ui::stripTrack : ui::faint;
    if (hot)
        trackColour = onBand ? trackColour.brighter (0.25f) : trackColour.darker (0.08f);
    g.setColour (trackColour);
    g.fillRect (track);
    const float from = xFor (juce::jmin (origin, normalised));
    const float to = xFor (juce::jmax (origin, normalised));
    if (to - from > 0.5f)
    {
        g.setColour (accentColour);
        g.fillRect (track.withX (from).withWidth (to - from));
    }
    g.setColour (text);
    g.fillRect (juce::Rectangle<float> (2.0f, 12.0f).withCentre ({ xFor (normalised), track.getCentreY() }));
}

// Small drawings of what the parameter does, in the manner of a printed panel.
// Small drawings of what the parameter does. Fills and the 2-unit lines share
// exact edges so every segment sits on the same baseline.
void ParameterSlider::paintGraphic (juce::Graphics& g, juce::Rectangle<float> area, float normalised) const
{
    const auto ink = onBand ? ui::stripGraphic : ui::ink;
    constexpr float line = 2.0f;
    const float x = area.getX();
    const float w = area.getWidth();
    const float top = std::round (area.getY());
    const float bottom = std::round (area.getBottom());
    const float sustainLevel = rawValue (audioProcessor, ParamIDs::sustain);
    const float sustainY = std::round (bottom - juce::jmax (line, sustainLevel * (bottom - top)));
    const float extent = juce::jmax (3.0f, std::round (normalised * w));
    juce::Path fill;
    g.setColour (ink);

    switch (graphic)
    {
        case Graphic::readout:
            g.fillRect (area);
            g.setColour (onBand ? ui::strip : ui::background);
            g.setFont (fonts.mono.withPointHeight (15.0f));
            g.drawText (displayValue(), area, juce::Justification::centred);
            return;
        case Graphic::glide:
        {
            // Glide is linear in pitch: a straight ramp whose length is the glide time.
            const float start = x + w * 0.12f;
            const float span = juce::jmax (line, normalised * w * 0.76f);
            const float low = bottom - line * 0.5f, high = top + line * 0.5f;
            juce::Path ramp;
            ramp.startNewSubPath (x, low);
            ramp.lineTo (start, low);
            ramp.lineTo (start + span, high);
            ramp.lineTo (x + w, high);
            g.strokePath (ramp, juce::PathStrokeType (line, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            return;
        }
        case Graphic::attack:
            g.fillRect (x, bottom - line, w, line);
            fill.addTriangle (x + w - extent, bottom, x + w, top, x + w, bottom);
            break;
        case Graphic::decay:
            fill.startNewSubPath (x, top);
            fill.lineTo (x + extent, sustainY);
            fill.lineTo (x + extent, bottom);
            fill.lineTo (x, bottom);
            fill.closeSubPath();
            g.fillRect (x + extent, sustainY, w - extent, line);
            break;
        case Graphic::sustain:
            g.fillRect (juce::Rectangle<float> (x, sustainY, w, bottom - sustainY));
            g.setColour (ink.withAlpha (0.35f));
            g.fillRect (x, top, w, line);
            return;
        case Graphic::release:
            g.fillRect (x, bottom - line, w, line);
            fill.addTriangle (x, top, x + extent, bottom, x, bottom);
            break;
        case Graphic::none:
            return;
    }
    g.fillPath (fill);
}

void ParameterSlider::setNormalised (float normalised)
{
    auto* value = parameter();
    if (value == nullptr)
        return;
    normalised = juce::jlimit (0.0f, 1.0f, normalised);
    if (snapper)
        normalised = value->convertTo0to1 (snapper (value->convertFrom0to1 (normalised)));
    value->setValueNotifyingHost (normalised);
}

void ParameterSlider::beginGesture()
{
    if (auto* value = parameter())
        value->beginChangeGesture();
}

void ParameterSlider::endGesture()
{
    if (auto* value = parameter())
        value->endChangeGesture();
}

void ParameterSlider::mouseDown (const juce::MouseEvent& event)
{
    if (auto* value = parameter())
    {
        dragStartNormalised = value->getValue();
        dragStartPosition = event.position;
        dragging = true;
        beginGesture();
        repaint();
    }
}

void ParameterSlider::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging)
        return;
    // Horizontal and vertical movement both count, so the track works like a fader
    // and the whole cell still works like a knob.
    const float pixels = (dragStartPosition.y - event.position.y) + (event.position.x - dragStartPosition.x);
    const float sensitivity = event.mods.isShiftDown() ? fineDragSensitivity : dragSensitivity;
    // Keep fractional movement before snapping, and rebase on every event so
    // pressing Shift never rescales earlier movement.
    dragStartNormalised = juce::jlimit (0.0f, 1.0f, dragStartNormalised + pixels / sensitivity);
    setNormalised (dragStartNormalised);
    dragStartPosition = event.position;
    repaint();
}

void ParameterSlider::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        endGesture();
    dragging = false;
    repaint();
}

void ParameterSlider::mouseDoubleClick (const juce::MouseEvent&)
{
    if (dragging)
        endGesture();
    dragging = false;
    resetToDefault (audioProcessor, parameterId);
    repaint();
}

bool ParameterSlider::keyPressed (const juce::KeyPress& key)
{
    auto* value = parameter();
    if (value == nullptr)
        return false;
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::backspaceKey || code == juce::KeyPress::deleteKey)
    {
        resetToDefault (audioProcessor, parameterId);
        repaint();
        return true;
    }
    const bool up = code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey;
    const bool down = code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey;
    if (! up && ! down && code != juce::KeyPress::homeKey && code != juce::KeyPress::endKey)
        return false;
    focusVisible = true;
    const auto& range = value->getNormalisableRange();
    const float adjacentValue = juce::jlimit (range.start, range.end,
        value->convertFrom0to1 (value->getValue()) + (up ? range.interval : -range.interval));
    const float minimumStep = std::abs (value->convertTo0to1 (adjacentValue) - value->getValue());
    float step = juce::jmax (key.getModifiers().isShiftDown() ? 0.001f : 0.01f, minimumStep);
    float target = code == juce::KeyPress::homeKey ? 0.0f
                 : code == juce::KeyPress::endKey ? 1.0f
                 : value->getValue() + (up ? step : -step);
    // Snapped values (synced rates) step to the next distinct value.
    if (snapper && (up || down))
    {
        const float current = snapper (value->convertFrom0to1 (value->getValue()));
        while (step < 1.0f && juce::approximatelyEqual (snapper (value->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, target))), current))
        {
            step *= 1.5f;
            target = value->getValue() + (up ? step : -step);
        }
    }
    beginGesture();
    setNormalised (target);
    endGesture();
    repaint();
    return true;
}

IconButton::IconButton (const juce::String& name, Icon iconToUse)
    : juce::Button (name), icon (iconToUse)
{
    setTitle (name);
    setTooltip (name);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    if (over || hasKeyboardFocus (true))
    {
        g.setColour (ui::faint.withAlpha (down ? 0.6f : 1.0f));
        g.fillRect (bounds);
    }
    const auto centre = bounds.getCentre();
    g.setColour (ui::ink.withAlpha (down ? 0.7f : 1.0f));
    if (icon == Icon::menu)
    {
        for (const float offset : { -5.0f, 0.0f, 5.0f })
            g.fillRect (juce::Rectangle<float> (14.0f, 2.0f).withCentre (centre.translated (0.0f, offset)));
        return;
    }
    if (icon == Icon::browse)
    {
        for (const auto offset : { juce::Point<float> (-3.5f, -3.5f), { 3.5f, -3.5f }, { -3.5f, 3.5f }, { 3.5f, 3.5f } })
            g.fillRect (juce::Rectangle<float> (5.0f, 5.0f).withCentre (centre + offset));
        return;
    }
    const float direction = icon == Icon::previous ? -1.0f : 1.0f;
    juce::Path chevron;
    chevron.startNewSubPath (centre.translated (-direction * 2.5f, -5.0f));
    chevron.lineTo (centre.translated (direction * 2.5f, 0.0f));
    chevron.lineTo (centre.translated (-direction * 2.5f, 5.0f));
    g.strokePath (chevron, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

namespace
{
// Five families: 24 cells and 19 gaps of 4 units, plus four 19-unit family gaps, fill the 560-unit row.
constexpr float modelCellWidth = 17.0f;
constexpr float modelCellPitch = 21.0f;
constexpr float modelFamilyGap = 19.0f;
}

ModelStrip::ModelStrip (MacroOscIIAudioProcessor& processor, EditorFonts editorFonts)
    : audioProcessor (processor), fonts (std::move (editorFonts))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Model map");
    setDescription ("All 24 models grouped by family. Click a cell to select that model.");
}

juce::Rectangle<float> ModelStrip::cellBounds (int target) const
{
    float x = 0.0f;
    int model = 0;
    for (const auto& family : modelFamilies)
    {
        for (int i = 0; i < family.count; ++i, ++model)
        {
            if (model == target)
                return { x, 0.0f, modelCellWidth, 14.0f };
            x += modelCellPitch;
        }
        x += modelFamilyGap - (modelCellPitch - modelCellWidth);
    }
    return {};
}

int ModelStrip::modelAt (juce::Point<float> position) const
{
    for (int model = 0; model < kModelCount; ++model)
        if (cellBounds (model).expanded (3.0f, 4.0f).contains (position))
            return model;
    return -1;
}

void ModelStrip::paint (juce::Graphics& g)
{
    const int current = juce::roundToInt (rawValue (audioProcessor, ParamIDs::model));
    const int focus = hovered >= 0 ? hovered : current;
    int model = 0;
    for (const auto& family : modelFamilies)
    {
        const auto first = cellBounds (model);
        const bool focusFamily = focus >= model && focus < model + family.count;
        for (int i = 0; i < family.count; ++i, ++model)
        {
            // On silver the current model is lit in ink, as the old band's fills were.
            g.setColour (model == current ? ui::ink : model == hovered ? ui::muted : ui::faint);
            g.fillRect (cellBounds (model));
        }
        // Every family is wide enough for its label; the focused one reads in ink.
        g.setFont (fonts.mono.withPointHeight (10.0f));
        g.setColour (focusFamily ? ui::ink : ui::muted);
        g.drawText (family.name, juce::Rectangle<float> (first.getX(), 20.0f, 120.0f, 14.0f), juce::Justification::centredLeft);
    }
}

void ModelStrip::mouseMove (const juce::MouseEvent& event)
{
    const int model = modelAt (event.position);
    if (model == hovered)
        return;
    hovered = model;
    setTooltip (model >= 0 ? juce::String (model + 1).paddedLeft ('0', 2) + "  " + plaitsModelNames()[model].toLowerCase() : juce::String());
    repaint();
}

void ModelStrip::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void ModelStrip::mouseDown (const juce::MouseEvent& event)
{
    const int model = modelAt (event.position);
    if (model >= 0)
        setAsGesture (audioProcessor, ParamIDs::model, static_cast<float> (model));
}

SegmentedControl::SegmentedControl (MacroOscIIAudioProcessor& processor, const char* parameterId,
                                    juce::StringArray choices, EditorFonts editorFonts)
    : options (std::move (choices)), fonts (std::move (editorFonts)),
      attachment (*processor.getState().getParameter (parameterId),
                  [this] (float value) { selected = juce::roundToInt (value); repaint(); })
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> SegmentedControl::optionBounds (int index) const
{
    const float optionWidth = (static_cast<float> (getWidth()) - 2.0f * static_cast<float> (options.size() - 1))
                              / static_cast<float> (options.size());
    return { static_cast<float> (index) * (optionWidth + 2.0f), 0.0f, optionWidth, static_cast<float> (getHeight()) };
}

void SegmentedControl::paint (juce::Graphics& g)
{
    g.setFont (fonts.mono.withPointHeight (12.0f));
    for (int i = 0; i < options.size(); ++i)
    {
        const auto area = optionBounds (i);
        if (i == selected)
        {
            g.setColour (ui::turquoise);
            g.fillRect (area.withTrimmedBottom (3.0f));
        }
        g.setColour (i == selected || i == hovered ? ui::ink : ui::muted);
        g.drawText (options[i], area.withTrimmedBottom (3.0f), juce::Justification::centred);
    }
    // Keyboard focus: a hairline under the whole switch.
    if (hasKeyboardFocus (true))
    {
        g.setColour (ui::turquoiseLine);
        g.fillRect (0.0f, static_cast<float> (getHeight()) - 1.0f, static_cast<float> (getWidth()), 1.0f);
    }
}

void SegmentedControl::mouseDown (const juce::MouseEvent& event)
{
    for (int i = 0; i < options.size(); ++i)
        if (optionBounds (i).contains (event.position))
            attachment.setValueAsCompleteGesture (static_cast<float> (i));
}

void SegmentedControl::mouseMove (const juce::MouseEvent& event)
{
    int next = -1;
    for (int i = 0; i < options.size(); ++i)
        if (optionBounds (i).contains (event.position))
            next = i;
    if (next != hovered)
    {
        hovered = next;
        repaint();
    }
}

void SegmentedControl::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

bool SegmentedControl::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const int delta = code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey ? 1
                    : code == juce::KeyPress::leftKey || code == juce::KeyPress::downKey ? -1 : 0;
    if (delta == 0)
        return false;
    attachment.setValueAsCompleteGesture (static_cast<float> (juce::jlimit (0, options.size() - 1, selected + delta)));
    return true;
}

PowerButton::PowerButton (MacroOscIIAudioProcessor& processor)
    : audioProcessor (processor)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Distortion on/off");
    setTooltip ("Switch distortion off, or back on to the last type used.");
}

void PowerButton::paint (juce::Graphics& g)
{
    const auto led = juce::Rectangle<float> (10.0f, 10.0f).withCentre (getLocalBounds().toFloat().getCentre());
    if (rawValue (audioProcessor, ParamIDs::distortionType) > 0.0f)
    {
        g.setColour (ui::magenta);
        g.fillRect (led);
        return;
    }
    g.setColour (isMouseOver() ? ui::ink : ui::muted);
    g.drawRect (led.reduced (0.75f), 1.5f);
}

void PowerButton::mouseDown (const juce::MouseEvent&)
{
    const int type = juce::roundToInt (rawValue (audioProcessor, ParamIDs::distortionType));
    if (type > 0)
        lastType = type;
    setAsGesture (audioProcessor, ParamIDs::distortionType, static_cast<float> (type > 0 ? 0 : lastType));
}

namespace
{
constexpr float voiceCellPitch = 11.0f;
constexpr float voiceCellWidth = 8.0f;
}

VoiceControl::VoiceControl (MacroOscIIAudioProcessor& processor, EditorFonts editorFonts)
    : audioProcessor (processor), fonts (std::move (editorFonts))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Voices");
    setTooltip ("Voices available in poly mode. Lit cells are sounding. Click or drag to set the count.");
    update();
}

void VoiceControl::update()
{
    const int nextActive = audioProcessor.getActiveVoiceCount();
    const int nextLimit = juce::roundToInt (rawValue (audioProcessor, ParamIDs::voices));
    const int nextMode = juce::roundToInt (rawValue (audioProcessor, ParamIDs::voiceMode));
    if (nextActive != active || nextLimit != limit || nextMode != mode)
    {
        active = nextActive;
        limit = nextLimit;
        mode = nextMode;
        repaint();
    }
}

void VoiceControl::paint (juce::Graphics& g)
{
    // Mono modes use a single voice whatever the count is set to.
    const int available = mode == 0 ? limit : 1;
    const float y = (static_cast<float> (getHeight()) - 14.0f) * 0.5f;
    for (int i = 0; i < MacroOscIIAudioProcessor::getMaxPolyphony(); ++i)
    {
        const auto cell = juce::Rectangle<float> (static_cast<float> (i) * voiceCellPitch, y, voiceCellWidth, 14.0f);
        if (i < active)
        {
            g.setColour (ui::turquoise);
            g.fillRect (cell);
        }
        else if (i < available)
        {
            g.setColour (ui::muted.withAlpha (0.55f));
            g.fillRect (cell);
        }
        else
        {
            g.setColour (ui::hairline);
            g.drawRect (cell, 1.0f);
        }
    }
    g.setFont (fonts.mono.withPointHeight (12.0f));
    g.setColour (ui::ink);
    g.drawText (juce::String (available), juce::Rectangle<float> (voiceCellPitch * 8.0f + 6.0f, 0.0f, 20.0f, static_cast<float> (getHeight())),
                juce::Justification::centredLeft);
}

void VoiceControl::setFromPosition (float x)
{
    const int count = juce::jlimit (1, MacroOscIIAudioProcessor::getMaxPolyphony(), static_cast<int> (x / voiceCellPitch) + 1);
    if (auto* parameter = parameterFor (audioProcessor, ParamIDs::voices))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (count)));
}

void VoiceControl::mouseDown (const juce::MouseEvent& event)
{
    if (auto* parameter = parameterFor (audioProcessor, ParamIDs::voices))
        parameter->beginChangeGesture();
    dragging = true;
    setFromPosition (event.position.x);
}

void VoiceControl::mouseDrag (const juce::MouseEvent& event)
{
    if (dragging)
        setFromPosition (event.position.x);
}

void VoiceControl::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        if (auto* parameter = parameterFor (audioProcessor, ParamIDs::voices))
            parameter->endChangeGesture();
    dragging = false;
}

MsegSlotSelector::MsegSlotSelector (MacroOscIIAudioProcessor& processor, EditorFonts editorFonts)
    : audioProcessor (processor), fonts (std::move (editorFonts))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (true);
    setTitle ("Modulation slot");
    setDescription ("Choose slot 1, 2 or 3. Arrow keys change the active slot.");
}

juce::Rectangle<float> MsegSlotSelector::slotBounds (int slot)
{
    return { static_cast<float> (slot) * 36.0f, 0.0f, 28.0f, 28.0f };
}

void MsegSlotSelector::setActiveSlot (int slot)
{
    activeSlot = juce::jlimit (0, kMsegSlotCount - 1, slot);
    repaint();
}

void MsegSlotSelector::paint (juce::Graphics& g)
{
    g.setFont (fonts.mono.withPointHeight (14.0f));
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        const auto area = slotBounds (slot);
        const bool active = slot == activeSlot;
        if (active)
        {
            g.setColour (ui::turquoise);
            g.fillRect (area);
        }
        else
        {
            g.setColour (ui::hairline);
            g.drawRect (area.reduced (0.75f), 1.5f);
        }
        g.setColour (active ? ui::ink : ui::muted);
        g.drawText (juce::String (slot + 1), area, juce::Justification::centred);
        // A dot marks slots that are routed somewhere.
        if (rawValue (audioProcessor, ParamIDs::msegDestination[static_cast<size_t> (slot)]) > 0.0f)
        {
            g.setColour (active ? ui::ink : ui::turquoiseLine);
            g.fillRect (area.getRight() - 7.0f, area.getY() + 3.0f, 4.0f, 4.0f);
        }
    }
    if (hasKeyboardFocus (true))
    {
        g.setColour (ui::ink);
        g.fillRect (slotBounds (activeSlot).withY (31.0f).withHeight (1.0f));
    }
}

void MsegSlotSelector::mouseDown (const juce::MouseEvent& event)
{
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        if (slotBounds (slot).contains (event.position) && onSlotSelected)
            onSlotSelected (slot);
}

bool MsegSlotSelector::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const int delta = code == juce::KeyPress::downKey || code == juce::KeyPress::rightKey ? 1
                    : code == juce::KeyPress::upKey || code == juce::KeyPress::leftKey ? -1 : 0;
    if (delta == 0)
        return false;
    if (onSlotSelected)
        onSlotSelected ((activeSlot + kMsegSlotCount + delta) % kMsegSlotCount);
    return true;
}
} // namespace macro_osc_ii
