#pragma once

#include "PluginProcessor.h"
#include "EffectsDisplays.h"
#include "UITheme.h"

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_extra/juce_gui_extra.h>

namespace macro_osc_ii
{
using EditorFonts = ui::Fonts;

// One parameter: a lowercase label, its value, and a thin pill track. Drag in
// any direction; Shift is fine; double-click, Backspace or Delete resets.
class ParameterSlider final : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Layout { stacked, row };
    enum class Graphic { none, readout, glide, attack, decay, sustain, release };

    ParameterSlider (MacroOscIIAudioProcessor& processor, EditorFonts fonts);

    void configure (const juce::String& labelText,
                    const char* parameter,
                    juce::Colour accent,
                    const juce::String& unitText,
                    int decimals,
                    float displayMultiplier = 1.0f);
    void setLayout (Layout newLayout) { layout = newLayout; repaint(); }
    void setGraphic (Graphic newGraphic) { graphic = newGraphic; repaint(); }
    void setOnBand (bool shouldUseBandColours) { onBand = shouldUseBandColours; repaint(); }
    void setFormatter (std::function<juce::String (float)> newFormatter) { formatter = std::move (newFormatter); }
    void setSnapper (std::function<float (float)> newSnapper) { snapper = std::move (newSnapper); }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    bool keyPressed (const juce::KeyPress& key) override;
    void focusGained (FocusChangeType cause) override
    {
        // Clicking takes focus too; only keyboard navigation shows the ring.
        focusVisible = cause != focusChangedByMouseClick;
        repaint();
    }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    MacroOscIIAudioProcessor& audioProcessor;
    EditorFonts fonts;
    juce::String label;
    juce::String unit;
    const char* parameterId {};
    juce::Colour accentColour { ui::turquoise };
    int decimalPlaces {};
    float multiplier { 1.0f };
    Layout layout { Layout::stacked };
    Graphic graphic { Graphic::none };
    bool onBand {};
    bool focusVisible {};
    bool dragging {};
    float dragStartNormalised {};
    juce::Point<float> dragStartPosition;
    std::function<juce::String (float)> formatter;
    std::function<float (float)> snapper;

    [[nodiscard]] juce::RangedAudioParameter* parameter() const;
    [[nodiscard]] float parameterValue() const;
    [[nodiscard]] juce::String displayValue() const;
    [[nodiscard]] juce::Rectangle<float> trackBounds() const;
    void setNormalised (float normalised);
    void beginGesture();
    void endGesture();
    void paintGraphic (juce::Graphics& g, juce::Rectangle<float> area, float normalised) const;
};

class IconButton final : public juce::Button
{
public:
    enum class Icon { previous, next, browse, menu };
    IconButton (const juce::String& name, Icon icon);
    void paintButton (juce::Graphics& g, bool over, bool down) override;

private:
    Icon icon;
};

// All 24 models as small cells grouped by family; click one to select it.
class ModelStrip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModelStrip (MacroOscIIAudioProcessor& processor, EditorFonts fonts);
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    MacroOscIIAudioProcessor& audioProcessor;
    EditorFonts fonts;
    int hovered { -1 };
    [[nodiscard]] juce::Rectangle<float> cellBounds (int model) const;
    [[nodiscard]] int modelAt (juce::Point<float> position) const;
};

// The selected model's waveform at its base Harmonics, Timbre and Morph, rendered offline.
class WaveformDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit WaveformDisplay (MacroOscIIAudioProcessor& processor);
    void refresh();
    void paint (juce::Graphics& g) override;

private:
    MacroOscIIAudioProcessor& audioProcessor;
    std::array<float, 5> lastParameters { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    std::vector<float> minimums, maximums;
    bool envelopeView {};
};

class SegmentedControl final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentedControl (MacroOscIIAudioProcessor& processor, const char* parameterId,
                      juce::StringArray options, EditorFonts fonts);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    juce::StringArray options;
    EditorFonts fonts;
    juce::ParameterAttachment attachment;
    int selected {};
    int hovered { -1 };
    [[nodiscard]] juce::Rectangle<float> optionBounds (int index) const;
};

// Switches distortion off, and back on to the last type used.
class PowerButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PowerButton (MacroOscIIAudioProcessor& processor);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    MacroOscIIAudioProcessor& audioProcessor;
    int lastType { 1 };
};

// Eight cells: how many voices are available, and how many are sounding.
// Click or drag across the cells to set the voice count.
class VoiceControl final : public juce::Component, public juce::SettableTooltipClient
{
public:
    VoiceControl (MacroOscIIAudioProcessor& processor, EditorFonts fonts);
    void update();
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    MacroOscIIAudioProcessor& audioProcessor;
    EditorFonts fonts;
    int active {}, limit {}, mode {};
    bool dragging {};
    void setFromPosition (float x);
};

class MsegSlotSelector final : public juce::Component
{
public:
    MsegSlotSelector (MacroOscIIAudioProcessor& processor, EditorFonts fonts);

    void setActiveSlot (int slot);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

    std::function<void (int)> onSlotSelected;

private:
    MacroOscIIAudioProcessor& audioProcessor;
    EditorFonts fonts;
    int activeSlot {};
    [[nodiscard]] static juce::Rectangle<float> slotBounds (int slot);
};

class MsegCurveEditor final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MsegCurveEditor (MacroOscIIAudioProcessor& processor);

    void setActiveSlot (int slot);
    void setPoints (std::vector<MsegPoint> newPoints);
    [[nodiscard]] bool isEditing() const noexcept { return editing; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;

private:
    MacroOscIIAudioProcessor& audioProcessor;
    std::vector<MsegPoint> points;
    int activeSlot {};
    int selectedIndex { -1 };
    int hoveredIndex { -1 };
    bool editing {};

    [[nodiscard]] juce::Rectangle<float> graphBounds() const;
    [[nodiscard]] juce::Point<float> pointToScreen (const MsegPoint& point) const;
    [[nodiscard]] MsegPoint screenToPoint (juce::Point<float> position) const;
    [[nodiscard]] MsegPoint appliedPoint (const MsegPoint& point) const;
    [[nodiscard]] int findNearestPoint (juce::Point<float> position, float radius) const;
    bool deletePointNear (juce::Point<float> position);
    void commit (bool notifyEditor);
};

// The whole instrument at its design size; the editor scales it to the window.
class EditorView final : public juce::Component,
                         private juce::ChangeListener,
                         private juce::AudioProcessorValueTreeState::Listener,
                         private juce::Timer
{
public:
    EditorView (MacroOscIIAudioProcessor& processor, EditorFonts fonts);
    ~EditorView() override;

    // Set by the editor: resize the window, and switch tooltips on or off.
    std::function<void (float)> onScaleChosen;
    std::function<void (bool)> onTooltipsChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    using ComboBoxAttachment = MacroOscIIAudioProcessor::APVTS::ComboBoxAttachment;
    using ButtonAttachment = MacroOscIIAudioProcessor::APVTS::ButtonAttachment;

    MacroOscIIAudioProcessor& audioProcessor;
    EditorFonts fonts;
    IconButton menuButton { "Settings", IconButton::Icon::menu };
    ParameterSlider volume;
    VoiceControl voiceControl;
    SegmentedControl voiceMode;
    juce::ComboBox model;
    IconButton previous { "Previous model", IconButton::Icon::previous };
    IconButton next { "Next model", IconButton::Icon::next };
    IconButton browse { "Browse models", IconButton::Icon::browse };
    ModelStrip modelStrip;
    WaveformDisplay waveform;
    ParameterSlider harmonics, timbre, morph, aux;
    juce::ComboBox distortionType;
    PowerButton distortionPower;
    DistortionDisplay distortionDisplay;
    ParameterSlider amount, mix, output;
    SegmentedControl filterType;
    FilterDisplay filterDisplay;
    ParameterSlider frequency, q;
    ParameterSlider pitch, detune, glide, attack, decay, sustain, release;
    MsegSlotSelector msegSlots;
    juce::ComboBox destination;
    juce::ToggleButton loop { "loop" }, sync { "sync" };
    MsegCurveEditor msegEditor;
    ParameterSlider msegOffset, msegScale, msegRate, msegRandom;
    std::unique_ptr<ComboBoxAttachment> modelAttachment, distortionAttachment, destinationAttachment;
    std::unique_ptr<ButtonAttachment> loopAttachment, syncAttachment;
    std::atomic_bool parameterUiDirty { false };
    int activeMsegSlot {};
    juce::String shownPresetName;
    std::unique_ptr<juce::FileChooser> presetChooser;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void stepModel (int delta);
    void showSettingsMenu();
    void savePreset();
    void openPreset();
    void loadPreset (const juce::File& file);
    void setActiveMsegSlot (int slot);
    [[nodiscard]] std::vector<ParameterSlider*> sliders();
};

class MacroOscIIAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MacroOscIIAudioProcessorEditor (MacroOscIIAudioProcessor&);
    ~MacroOscIIAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MacroOscIIAudioProcessor& audioProcessor;
    EditorFonts fonts;
    ui::LookAndFeel lookAndFeel;
    EditorView view;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    bool restoringSize { true };

    void setTooltipsEnabled (bool enabled);

    [[nodiscard]] static EditorFonts loadFonts();
};
} // namespace macro_osc_ii
