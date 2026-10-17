#include "PluginEditor.h"
#include "EditorParameters.h"

namespace macro_osc_ii
{
namespace
{
constexpr float pointHitRadius = 13.0f;
constexpr float pointSearchRadius = 24.0f;
}

MsegCurveEditor::MsegCurveEditor (MacroOscIIAudioProcessor& processor)
    : audioProcessor (processor), points (processor.getMsegPoints (0))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setTitle ("Modulation shape");
    setTooltip ("Click to add a point, drag to move it, right-click to remove it; remove down to one point for a fixed level per note. "
                "The dashed line is the shape after scale and offset; shaded boxes show how far random can move each point.");
}

void MsegCurveEditor::setActiveSlot (int slot)
{
    activeSlot = juce::jlimit (0, kMsegSlotCount - 1, slot);
    setPoints (audioProcessor.getMsegPoints (activeSlot));
}

void MsegCurveEditor::setPoints (std::vector<MsegPoint> newPoints)
{
    if (editing)
        return;
    points = MsegShape::sanitize (std::move (newPoints));
    selectedIndex = -1;
    repaint();
}

juce::Rectangle<float> MsegCurveEditor::graphBounds() const
{
    // Inset so handles on the first and last points are never clipped.
    return getLocalBounds().toFloat().reduced (6.0f);
}

juce::Point<float> MsegCurveEditor::pointToScreen (const MsegPoint& point) const
{
    const auto graph = graphBounds();
    return { graph.getX() + point.x * graph.getWidth(), graph.getBottom() - point.y * graph.getHeight() };
}

MsegPoint MsegCurveEditor::screenToPoint (juce::Point<float> position) const
{
    const auto graph = graphBounds();
    return { juce::jlimit (0.0f, 1.0f, (position.x - graph.getX()) / graph.getWidth()),
             juce::jlimit (0.0f, 1.0f, 1.0f - (position.y - graph.getY()) / graph.getHeight()) };
}

MsegPoint MsegCurveEditor::appliedPoint (const MsegPoint& point) const
{
    const float amount = editor::rawValue (audioProcessor, ParamIDs::msegAmount[static_cast<size_t> (activeSlot)]) * 0.01f;
    const float offset = editor::rawValue (audioProcessor, ParamIDs::msegOffset[static_cast<size_t> (activeSlot)]) * 0.01f;
    const float scaled = juce::jlimit (-1.0f, 1.0f, (point.y * 2.0f - 1.0f) * amount + offset);
    return { point.x, (scaled + 1.0f) * 0.5f };
}

int MsegCurveEditor::findNearestPoint (juce::Point<float> position, float radius) const
{
    int nearest = -1;
    for (size_t i = 0; i < points.size(); ++i)
    {
        const float distance = position.getDistanceFrom (pointToScreen (points[i]));
        if (distance < radius)
        {
            radius = distance;
            nearest = static_cast<int> (i);
        }
    }
    return nearest;
}

void MsegCurveEditor::paint (juce::Graphics& g)
{
    const auto graph = graphBounds();
    ui::drawDotGrid (g, graph);
    g.setColour (ui::faint);
    g.fillRect (graph.withSizeKeepingCentre (graph.getWidth(), 1.0f));
    if (points.empty())
        return;

    // How far random can move each point: half the height either way at 100%,
    // and for inner points half the gap to the nearer neighbour in time.
    const float random = editor::rawValue (audioProcessor, ParamIDs::msegRandom[static_cast<size_t> (activeSlot)]) * 0.01f;
    if (random > 0.0f)
    {
        g.setColour (ui::turquoise.withAlpha (0.1f));
        for (size_t i = 0; i < points.size(); ++i)
        {
            const bool inner = i > 0 && i + 1 < points.size();
            const float spanX = inner ? random * 0.5f * std::min (points[i].x - points[i - 1].x, points[i + 1].x - points[i].x)
                                              * graph.getWidth()
                                      : 0.0f;
            const float spanY = random * 0.5f * graph.getHeight();
            const auto centre = pointToScreen (points[i]);
            g.fillRect (juce::Rectangle<float> (centre.x - spanX - 2.0f, centre.y - spanY, spanX * 2.0f + 4.0f, spanY * 2.0f)
                            .getIntersection (graph));
        }
    }

    // A single point is a constant level across the whole cycle.
    juce::Path shape, applied;
    const bool single = points.size() == 1;
    shape.startNewSubPath (single ? juce::Point<float> (graph.getX(), pointToScreen (points.front()).y) : pointToScreen (points.front()));
    applied.startNewSubPath (single ? juce::Point<float> (graph.getX(), pointToScreen (appliedPoint (points.front())).y)
                                    : pointToScreen (appliedPoint (points.front())));
    for (size_t i = 1; i < points.size(); ++i)
    {
        shape.lineTo (pointToScreen (points[i]));
        applied.lineTo (pointToScreen (appliedPoint (points[i])));
    }
    if (single)
    {
        shape.lineTo (graph.getRight(), pointToScreen (points.front()).y);
        applied.lineTo (graph.getRight(), pointToScreen (appliedPoint (points.front())).y);
    }
    auto fill = shape;
    fill.lineTo (graph.getRight(), graph.getCentreY());
    fill.lineTo (graph.getX(), graph.getCentreY());
    fill.closeSubPath();
    g.setColour (ui::turquoise.withAlpha (0.16f));
    g.fillPath (fill);

    juce::Path dashed;
    const float dashes[] { 4.0f, 3.0f };
    juce::PathStrokeType (1.4f).createDashedStroke (dashed, applied, dashes, 2);
    g.setColour (ui::muted);
    g.fillPath (dashed);

    g.setColour (ui::turquoiseLine);
    g.strokePath (shape, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    for (size_t i = 0; i < points.size(); ++i)
    {
        const bool selected = static_cast<int> (i) == selectedIndex;
        const bool hovered = static_cast<int> (i) == hoveredIndex;
        const auto handle = juce::Rectangle<float> (8.0f, 8.0f).withCentre (pointToScreen (points[i]));
        if (hovered || selected)
        {
            g.setColour (ui::turquoise.withAlpha (0.22f));
            g.fillRect (handle.expanded (4.0f));
        }
        g.setColour (selected ? ui::turquoiseLine : ui::background);
        g.fillRect (handle);
        g.setColour (ui::turquoiseLine);
        g.drawRect (handle, 2.0f);
    }
}

void MsegCurveEditor::mouseDown (const juce::MouseEvent& event)
{
    const auto position = event.position;
    editing = true;
    if (event.mods.isRightButtonDown())
    {
        deletePointNear (position);
        repaint();
        return;
    }
    selectedIndex = findNearestPoint (position, pointSearchRadius);
    if (selectedIndex < 0)
    {
        points.push_back (screenToPoint (position));
        points = MsegShape::sanitize (std::move (points));
        selectedIndex = findNearestPoint (position, pointSearchRadius);
        commit (false);
    }
    repaint();
}

void MsegCurveEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        deletePointNear (event.position);
        repaint();
        return;
    }
    if (selectedIndex < 0 || selectedIndex >= static_cast<int> (points.size()))
        return;

    auto next = screenToPoint (event.position);
    const auto last = static_cast<int> (points.size()) - 1;
    if (points.size() == 1)
        next.x = 0.5f;
    else if (selectedIndex == 0)
        next.x = 0.0f;
    else if (selectedIndex == last)
        next.x = 1.0f;
    else
        next.x = juce::jlimit (points[static_cast<size_t> (selectedIndex - 1)].x,
                               points[static_cast<size_t> (selectedIndex + 1)].x, next.x);

    points[static_cast<size_t> (selectedIndex)] = next;
    audioProcessor.setMsegPoints (activeSlot, points, false);
    repaint();
}

void MsegCurveEditor::mouseUp (const juce::MouseEvent& event)
{
    editing = false;
    commit (true);
    hoveredIndex = findNearestPoint (event.position, pointHitRadius);
    repaint();
}

void MsegCurveEditor::mouseMove (const juce::MouseEvent& event)
{
    const int next = findNearestPoint (event.position, pointHitRadius);
    if (next == hoveredIndex)
        return;
    hoveredIndex = next;
    setMouseCursor (next >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}

void MsegCurveEditor::mouseExit (const juce::MouseEvent&)
{
    if (hoveredIndex != -1)
    {
        hoveredIndex = -1;
        repaint();
    }
}

bool MsegCurveEditor::deletePointNear (juce::Point<float> position)
{
    // Inner points go freely. The end points go only once they are all that is
    // left, leaving one point: a constant level with no movement.
    const int index = findNearestPoint (position, pointSearchRadius);
    const int count = static_cast<int> (points.size());
    const bool inner = index > 0 && index < count - 1;
    if (index < 0 || count <= 1 || (! inner && count > 2))
        return false;
    points.erase (points.begin() + index);
    if (points.size() == 1)
        points.front().x = 0.5f;
    selectedIndex = -1;
    audioProcessor.setMsegPoints (activeSlot, points, false);
    return true;
}

void MsegCurveEditor::commit (bool notifyEditor)
{
    if (notifyEditor)
        points = MsegShape::sanitize (std::move (points));
    audioProcessor.setMsegPoints (activeSlot, points, notifyEditor);
}
} // namespace macro_osc_ii
