#pragma once

#include <array>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace macro_osc_ii::ui
{
// Macro OSC inverted. Both take their colour from the Braids hardware:
// turquoise and magenta caps, the yellow-green readout and the silver
// faceplate. Macro OSC is dark with a silver band; Macro OSC II is silver
// with a dark band. The readout stays a dark window, as on the module.
inline const juce::Colour background { 0xffcfd1d6 };
inline const juce::Colour header { 0xffc2c4ca };
inline const juce::Colour ink { 0xff111216 };
inline const juce::Colour muted { 0xff4f535b };
// Tracks, unlit cells and centre lines.
inline const juce::Colour faint { 0xffb0b3ba };
// Outlines around fields and buttons.
inline const juce::Colour hairline { 0xffa4a7af };
inline const juce::Colour dot { 0xffbcbfc5 };
// Turquoise fills as on the knob caps; thin lines take the deeper shade to read on silver.
inline const juce::Colour turquoise { 0xff34c6cc };
inline const juce::Colour turquoiseLine { 0xff1f9fa5 };
inline const juce::Colour magenta { 0xffe8437a };
inline const juce::Colour display { 0xffe8fa42 };
// The readout window.
inline const juce::Colour chip { 0xff121418 };
inline const juce::Colour chipLine { 0xff1d2026 };
inline const juce::Colour popup { 0xffd8dadf };
// The dark strip behind pitch and envelope, and its printing.
inline const juce::Colour strip { 0xff15171b };
inline const juce::Colour stripInk { 0xffeceef2 };
inline const juce::Colour stripMuted { 0xff8d929d };
inline const juce::Colour stripTrack { 0xff2c2f36 };
inline const juce::Colour stripGraphic { 0xffc6c8ce };

inline constexpr int width = 960;
inline constexpr int height = 640;

struct Fonts
{
    juce::Font sans { juce::FontOptions (14.0f) };
    juce::Font medium { juce::FontOptions (14.0f) };
    juce::Font mono { juce::FontOptions (12.0f) };
    juce::Font segment { juce::FontOptions (28.0f) };
};

// Faint dots on a 12-unit grid: the backdrop for every plot.
inline void drawDotGrid (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour = dot)
{
    g.setColour (colour);
    for (float y = area.getY() + 6.0f; y < area.getBottom(); y += 12.0f)
        for (float x = area.getX() + 6.0f; x < area.getRight(); x += 12.0f)
            g.fillRect (x - 1.0f, y - 1.0f, 2.0f, 2.0f);
}

// A soft halo behind the lit readout.
inline void drawGlow (juce::Graphics& g, const juce::Path& shape, juce::Colour colour, int radius = 8)
{
    juce::DropShadow (colour.withAlpha (0.5f), radius, {}).drawForPath (g, shape);
}

inline void drawChevron (juce::Graphics& g, juce::Point<float> centre, juce::Colour colour)
{
    juce::Path chevron;
    chevron.startNewSubPath (centre.translated (-3.0f, -1.5f));
    chevron.lineTo (centre.translated (0.0f, 1.5f));
    chevron.lineTo (centre.translated (3.0f, -1.5f));
    g.setColour (colour);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// DSEG14 lights every segment for '~' and leaves a full-width blank for '!', so
// the unlit cells sit exactly behind the lit characters.
inline void drawSegmentText (juce::Graphics& g, const juce::Font& font, const juce::String& value,
                             juce::Rectangle<float> area, int cells)
{
    const float baseline = area.getCentreY() + (font.getAscent() - font.getDescent()) * 0.5f;
    juce::GlyphArrangement ghost, lit;
    ghost.addLineOfText (font, juce::String::repeatedString ("~", cells), area.getX(), baseline);
    lit.addLineOfText (font, value.toUpperCase().replaceCharacter (' ', '!'), area.getX(), baseline);
    g.setColour (display.withAlpha (0.08f));
    ghost.draw (g);
    juce::Path glyphs;
    lit.createPath (glyphs);
    juce::DropShadow (display.withAlpha (0.35f), 10, {}).drawForPath (g, glyphs);
    g.setColour (display);
    g.fillPath (glyphs);
}

// The mark: a plait of three strands, where Macro OSC's is a braid of two rings.
// The strands are cosines a third of a cycle apart across the area's width. At
// each crossing the strand coming in from the outside passes over the middle
// one, as in a real plait; a gap cut into the strand beneath shows the weave.
inline void drawLogo (juce::Graphics& g, juce::Rectangle<float> area, float stroke, juce::Colour behind,
                      const std::array<juce::Colour, 3>& colours)
{
    const float x0 = area.getX(), w = area.getWidth();
    const float cy = area.getCentreY(), amplitude = area.getHeight() * 0.5f;
    const auto y = [&] (int strand, float x)
    {
        return cy + amplitude * std::cos (juce::MathConstants<float>::twoPi * x / w + juce::MathConstants<float>::pi / 6.0f
                                          + juce::MathConstants<float>::twoPi * static_cast<float> (strand) / 3.0f);
    };
    const auto segment = [&] (int strand, float from, float to)
    {
        juce::Path path;
        from = juce::jmax (0.0f, from);
        to = juce::jmin (w, to);
        const float step = w / 160.0f;
        path.startNewSubPath (x0 + from, y (strand, from));
        for (float x = from + step; x < to; x += step)
            path.lineTo (x0 + x, y (strand, x));
        path.lineTo (x0 + to, y (strand, to));
        return path;
    };
    const auto strokeWith = [&] (const juce::Path& path, juce::Colour colour, float thickness)
    {
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };

    struct Crossing
    {
        float at;
        int over, under;
    };
    std::vector<Crossing> crossings;
    const float scan = w / 2000.0f;
    for (int i = 0; i < 3; ++i)
        for (int j = i + 1; j < 3; ++j)
            for (float x = 0.0f; x < w - scan; x += scan)
            {
                const float a = y (i, x) - y (j, x), b = y (i, x + scan) - y (j, x + scan);
                if (a * b >= 0.0f && a != 0.0f)
                    continue;
                const float at = x + scan * std::abs (a) / (std::abs (a) + std::abs (b));
                const int third = 3 - i - j;
                // Screen y grows downwards: a third strand below means this is the upper pair.
                const bool upperPair = y (third, at) > y (i, at);
                const bool iFalling = y (i, at + 0.01f) > y (i, at - 0.01f);
                const bool iOver = upperPair == iFalling;
                crossings.push_back ({ at, iOver ? i : j, iOver ? j : i });
            }

    // A strand passing under a crossing right at the edge would leave a stub beyond
    // its gap; it starts (or ends) at the crossing instead.
    const float gap = w * 0.08f;
    std::array<juce::Range<float>, 3> extents;
    extents.fill ({ 0.0f, w });
    for (const auto& crossing : crossings)
    {
        auto& extent = extents[static_cast<size_t> (crossing.under)];
        if (crossing.at < gap * 1.5f)
            extent.setStart (crossing.at);
        else if (crossing.at > w - gap * 1.5f)
            extent.setEnd (crossing.at);
    }
    for (int strand = 0; strand < 3; ++strand)
        strokeWith (segment (strand, extents[static_cast<size_t> (strand)].getStart(), extents[static_cast<size_t> (strand)].getEnd()),
                    colours[static_cast<size_t> (strand)], stroke);
    for (const auto& crossing : crossings)
    {
        strokeWith (segment (crossing.over, crossing.at - gap, crossing.at + gap), behind, stroke + 2.4f);
        // The strand runs a little past its gap so no seam shows at the ends.
        strokeWith (segment (crossing.over, crossing.at - gap * 1.4f, crossing.at + gap * 1.4f),
                    colours[static_cast<size_t> (crossing.over)], stroke);
    }
}

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static constexpr float modelSeam = 74.0f;

    explicit LookAndFeel (Fonts editorFonts) : fonts (std::move (editorFonts))
    {
        setColour (juce::PopupMenu::backgroundColourId, popup);
        setColour (juce::PopupMenu::textColourId, ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, turquoise);
        setColour (juce::PopupMenu::highlightedTextColourId, ink);
        setColour (juce::TooltipWindow::backgroundColourId, popup);
        setColour (juce::TooltipWindow::textColourId, ink);
        setColour (juce::TooltipWindow::outlineColourId, hairline);
        setColour (juce::ComboBox::textColourId, ink);
        setColour (juce::ComboBox::arrowColourId, muted);
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return isModelSelector (&box) ? fonts.segment : fonts.medium.withPointHeight (15.0f);
    }

    juce::Font getPopupMenuFont() override { return fonts.mono.withPointHeight (12.0f); }

    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox& box, juce::Label&) override
    {
        const bool models = isModelSelector (&box);
        // No item is pinned to the box: every list fits in full, and pinning
        // makes JUCE scroll the menu instead of opening it clear of the box.
        auto options = juce::PopupMenu::Options().withTargetComponent (&box)
            .withInitiallySelectedItem (box.getSelectedId())
            .withMinimumWidth (models ? 450 : juce::jmax (140, box.getWidth()))
            .withMinimumNumColumns (models ? 3 : 1)
            .withMaximumNumColumns (models ? 3 : 1)
            .withStandardItemHeight (24);
        // Menus open inside the scaled editor view, so they follow the window size
        // and stay within the plug-in window in every host. Selectors in the lower
        // half open upwards so the whole list fits without scrolling.
        for (auto* parent = box.getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
            if (parent->getComponentID() == "editor-view")
            {
                const bool lower = parent->getLocalArea (&box, box.getLocalBounds()).getCentreY() > parent->getHeight() / 2;
                return options.withParentComponent (parent)
                              .withPreferredPopupDirection (lower ? juce::PopupMenu::Options::PopupDirection::upwards
                                                                  : juce::PopupMenu::Options::PopupDirection::downwards);
            }
        return options;
    }

    void getIdealPopupMenuItemSizeWithOptions (const juce::String& itemText, bool separator, int,
                                              int& itemWidth, int& itemHeight, const juce::PopupMenu::Options& options) override
    {
        itemWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (getPopupMenuFont(), itemText))
                    + (isModelSelector (options.getTargetComponent()) ? 64 : 40);
        itemHeight = separator ? 9 : 24;
    }

    void drawPopupMenuBackground (juce::Graphics& g, int menuWidth, int menuHeight) override
    {
        g.fillAll (popup);
        g.setColour (hairline);
        g.drawRect (0, 0, menuWidth, menuHeight);
    }

    void drawPopupMenuItemWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area,
                                      bool highlighted, const juce::PopupMenu::Item& item,
                                      const juce::PopupMenu::Options& options) override
    {
        if (item.isSeparator)
        {
            g.setColour (hairline);
            g.fillRect (area.withSizeKeepingCentre (area.getWidth() - 20, 1));
            return;
        }

        const bool hover = highlighted && item.isEnabled;
        const bool models = isModelSelector (options.getTargetComponent());
        if (hover)
        {
            g.setColour (turquoise);
            g.fillRect (area.reduced (4, 1));
        }
        if (item.isTicked)
        {
            g.setColour (hover ? ink : turquoiseLine);
            g.fillRect (static_cast<float> (area.getX() + 10), area.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
        auto label = area.withTrimmedLeft (24).withTrimmedRight (10);
        if (models)
        {
            g.setFont (fonts.mono.withPointHeight (11.0f));
            g.setColour (hover ? ink.withAlpha (0.7f) : muted);
            g.drawText (juce::String (item.itemID).paddedLeft ('0', 2), label.removeFromLeft (26),
                        juce::Justification::centredLeft);
        }
        g.setFont (getPopupMenuFont());
        g.setColour ((hover ? ink : item.isTicked ? turquoiseLine : ink).withMultipliedAlpha (item.isEnabled ? 1.0f : 0.4f));
        g.drawText (item.text.toLowerCase(), label, juce::Justification::centredLeft);
        if (item.subMenu != nullptr)
        {
            const juce::Point<float> tip (static_cast<float> (area.getRight() - 12), static_cast<float> (area.getCentreY()));
            juce::Path chevron;
            chevron.startNewSubPath (tip.translated (-1.5f, -3.0f));
            chevron.lineTo (tip.translated (1.5f, 0.0f));
            chevron.lineTo (tip.translated (-1.5f, 3.0f));
            g.setColour (hover ? ink : muted);
            g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawComboBox (juce::Graphics& g, int boxWidth, int boxHeight, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float> (0, 0, static_cast<float> (boxWidth), static_cast<float> (boxHeight));
        const bool focused = box.hasKeyboardFocus (true);
        const bool over = box.isMouseOver (true);
        if (isModelSelector (&box))
        {
            // The module's dark readout window: two index cells, a fine seam, then the name.
            g.setColour (chip);
            g.fillRect (bounds);
            g.setColour (focused ? turquoise : over ? stripTrack : chipLine);
            g.drawRect (bounds, 1.0f);
            drawSegmentText (g, fonts.segment, juce::String (box.getSelectedId()).paddedLeft ('0', 2),
                             bounds.withTrimmedLeft (16.0f).withWidth (48.0f), 2);
            g.setColour (stripTrack);
            g.fillRect (juce::Rectangle<float> (1.0f, 24.0f).withCentre ({ modelSeam, bounds.getCentreY() }));
            drawChevron (g, { bounds.getRight() - 18.0f, bounds.getCentreY() }, over ? stripInk : stripMuted);
            return;
        }
        // Other selectors are a quiet field: a hairline outline, text and a chevron.
        g.setColour (focused ? turquoiseLine : over ? muted.withAlpha (0.6f) : hairline);
        g.drawRect (bounds, 1.0f);
        drawChevron (g, { bounds.getRight() - 12.0f, bounds.getCentreY() + 1.0f }, over ? ink : muted);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        if (isModelSelector (&box))
            label.setBounds (juce::roundToInt (modelSeam) + 16, 0,
                             box.getWidth() - juce::roundToInt (modelSeam) - 50, box.getHeight());
        else
            label.setBounds (10, 0, box.getWidth() - 30, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setBorderSize ({});
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        auto* box = dynamic_cast<juce::ComboBox*> (label.getParentComponent());
        if (box == nullptr)
        {
            LookAndFeel_V4::drawLabel (g, label);
            return;
        }
        const auto area = label.getLocalBounds().toFloat();
        if (isModelSelector (box))
        {
            const float cell = juce::GlyphArrangement::getStringWidth (fonts.segment, "~");
            drawSegmentText (g, fonts.segment, label.getText(), area, static_cast<int> (area.getWidth() / cell));
            return;
        }
        g.setFont (label.getFont());
        g.setColour (ink);
        g.drawText (label.getText().toLowerCase(), area, box->getJustificationType(), true);
    }

    // Loop and Sync: a small square LED that lights when on.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool over, bool) override
    {
        const bool on = button.getToggleState();
        const auto led = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ 5.0f, static_cast<float> (button.getHeight()) * 0.5f });
        if (on)
        {
            g.setColour (turquoise);
            g.fillRect (led);
        }
        else
        {
            g.setColour (over ? ink : muted);
            g.drawRect (led.reduced (0.75f), 1.5f);
        }
        g.setFont (fonts.medium.withPointHeight (14.0f));
        g.setColour (on || over ? ink : muted);
        g.drawText (button.getButtonText().toLowerCase(), button.getLocalBounds().withTrimmedLeft (17),
                    juce::Justification::centredLeft);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (popup);
        g.setColour (hairline);
        g.drawRect (0, 0, w, h);
        g.setColour (ink);
        g.setFont (fonts.sans.withPointHeight (12.0f));
        g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> position, juce::Rectangle<int> parentArea) override
    {
        const auto font = fonts.sans.withPointHeight (12.0f);
        juce::AttributedString attributed;
        attributed.append (text, font);
        juce::TextLayout layout;
        layout.createLayout (attributed, 300.0f);
        const int w = juce::roundToInt (layout.getWidth()) + 18;
        const int h = juce::roundToInt (layout.getHeight()) + 10;
        return juce::Rectangle<int> (position.x > parentArea.getCentreX() ? position.x - (w + 12) : position.x + 12,
                                     position.y > parentArea.getCentreY() ? position.y - (h + 8) : position.y + 18,
                                     w, h).constrainedWithin (parentArea);
    }

    void drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging) override
    {
        g.setColour ((over || dragging) ? muted : hairline);
        for (float inset : { 4.0f, 8.0f })
            g.drawLine (static_cast<float> (w) - inset, static_cast<float> (h) - 2.0f,
                        static_cast<float> (w) - 2.0f, static_cast<float> (h) - inset, 1.2f);
    }

private:
    static bool isModelSelector (const juce::Component* component)
    {
        return component != nullptr && component->getComponentID() == "oscillator-model";
    }
    Fonts fonts;
};
}
