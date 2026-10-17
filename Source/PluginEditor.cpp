#include "PluginEditor.h"
#include "EditorParameters.h"

#include "BinaryData.h"

#include <algorithm>
#include <array>
#include <tuple>

namespace macro_osc_ii
{
using namespace editor;

namespace
{
constexpr std::array<const char*, 43> uiParameterIds {
    ParamIDs::filterType, ParamIDs::filterFrequency, ParamIDs::filterQ,
    ParamIDs::distortionType, ParamIDs::distortionAmount, ParamIDs::distortionMix, ParamIDs::distortionOutput,
    ParamIDs::model, ParamIDs::harmonics, ParamIDs::timbre, ParamIDs::morph, ParamIDs::auxMix,
    ParamIDs::coarse, ParamIDs::fine, ParamIDs::portamento,
    ParamIDs::attack, ParamIDs::decay, ParamIDs::sustain, ParamIDs::release,
    ParamIDs::msegDestination[0], ParamIDs::msegDestination[1], ParamIDs::msegDestination[2],
    ParamIDs::msegAmount[0], ParamIDs::msegAmount[1], ParamIDs::msegAmount[2],
    ParamIDs::msegOffset[0], ParamIDs::msegOffset[1], ParamIDs::msegOffset[2],
    ParamIDs::msegRate[0], ParamIDs::msegRate[1], ParamIDs::msegRate[2],
    ParamIDs::msegLoop[0], ParamIDs::msegLoop[1], ParamIDs::msegLoop[2],
    ParamIDs::msegSync[0], ParamIDs::msegSync[1], ParamIDs::msegSync[2],
    ParamIDs::msegRandom[0], ParamIDs::msegRandom[1], ParamIDs::msegRandom[2],
    ParamIDs::level, ParamIDs::voiceMode, ParamIDs::voices
};

constexpr float minimumScale = 0.75f;
constexpr float maximumScale = 1.5f;

[[nodiscard]] juce::String signedText (float value, int decimals)
{
    const auto magnitude = decimals <= 0 ? juce::String (std::abs (juce::roundToInt (value))).paddedLeft ('0', 2)
                                         : juce::String (std::abs (value), decimals);
    return (value < -1.0e-4f ? "-" : "+") + magnitude;
}

[[nodiscard]] juce::String secondsText (float seconds)
{
    return seconds < 0.9995f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                          : juce::String (seconds, 2) + " s";
}

// Lays a row of equal cells across an area with a fixed gap.
template <typename Components>
void layoutRow (juce::Rectangle<int> area, int gap, const Components& components)
{
    const int count = static_cast<int> (components.size());
    const int cell = (area.getWidth() - gap * (count - 1)) / count;
    for (auto* component : components)
    {
        component->setBounds (area.removeFromLeft (cell));
        area.removeFromLeft (gap);
    }
}
} // namespace

EditorView::EditorView (MacroOscIIAudioProcessor& processor, EditorFonts editorFonts)
    : audioProcessor (processor),
      fonts (std::move (editorFonts)),
      volume (processor, fonts),
      voiceControl (processor, fonts),
      voiceMode (processor, ParamIDs::voiceMode, { "poly", "mono", "legato" }, fonts),
      modelStrip (processor, fonts),
      waveform (processor),
      harmonics (processor, fonts), timbre (processor, fonts), morph (processor, fonts), aux (processor, fonts),
      distortionPower (processor),
      distortionDisplay (processor, fonts.mono),
      amount (processor, fonts), mix (processor, fonts), output (processor, fonts),
      filterType (processor, ParamIDs::filterType, { "off", "lp", "bp", "hp" }, fonts),
      filterDisplay (processor, fonts.mono),
      frequency (processor, fonts), q (processor, fonts),
      pitch (processor, fonts), detune (processor, fonts), glide (processor, fonts),
      attack (processor, fonts), decay (processor, fonts), sustain (processor, fonts), release (processor, fonts),
      msegSlots (processor, fonts),
      msegEditor (processor),
      msegOffset (processor, fonts), msegScale (processor, fonts), msegRate (processor, fonts), msegRandom (processor, fonts)
{
    setComponentID ("editor-view");
    setOpaque (true);
    for (auto* component : std::initializer_list<juce::Component*> {
             &menuButton, &volume, &voiceControl, &voiceMode, &model, &previous, &next, &browse, &modelStrip, &waveform,
             &distortionType, &distortionPower, &distortionDisplay, &filterType, &filterDisplay,
             &msegSlots, &destination, &loop, &sync, &msegEditor })
        addAndMakeVisible (component);
    for (auto* slider : sliders())
        addAndMakeVisible (slider);

    const auto names = plaitsModelNames();
    for (int i = 0; i < names.size(); ++i)
        model.addItem (names[i], i + 1);
    model.setComponentID ("oscillator-model");
    model.setTitle ("Oscillator model");
    model.setTooltip ("Choose an oscillator model. The arrows step through all 24.");
    modelAttachment = std::make_unique<ComboBoxAttachment> (audioProcessor.getState(), ParamIDs::model, model);
    previous.onClick = [this] { stepModel (-1); };
    next.onClick = [this] { stepModel (1); };
    browse.onClick = [this] { model.showPopup(); };
    menuButton.onClick = [this] { showSettingsMenu(); };

    harmonics.configure ("Harmonics", ParamIDs::harmonics, ui::turquoise, {}, 0, 127.0f);
    timbre.configure ("Timbre", ParamIDs::timbre, ui::magenta, {}, 0, 127.0f);
    morph.configure ("Morph", ParamIDs::morph, ui::turquoise, {}, 0, 127.0f);
    aux.configure ("Aux", ParamIDs::auxMix, ui::muted, "%", 0, 100.0f);
    aux.setTooltip ("Blends from the main output to the model's auxiliary output, a variant such as a sub-oscillator, "
                    "a second voicing or a raw exciter.");

    for (const auto name : lodo::engine::fx::distortionNames)
        distortionType.addItem (juce::String (name.data(), name.size()), distortionType.getNumItems() + 1);
    distortionType.setTitle ("Distortion type");
    distortionType.setTooltip ("Distortion algorithm. Off bypasses distortion.");
    distortionType.setJustificationType (juce::Justification::centredLeft);
    distortionAttachment = std::make_unique<ComboBoxAttachment> (audioProcessor.getState(), ParamIDs::distortionType, distortionType);
    amount.configure ("Amount", ParamIDs::distortionAmount, ui::magenta, "%", 0, 100.0f);
    mix.configure ("Mix", ParamIDs::distortionMix, ui::magenta, "%", 0, 100.0f);
    output.configure ("Output", ParamIDs::distortionOutput, ui::magenta, "%", 0, 100.0f);
    output.setTooltip ("Wet output level. 50% is unity gain; the dry signal is unchanged.");

    filterType.setTitle ("Filter type");
    volume.configure ({}, ParamIDs::level, ui::muted, {}, 1);
    volume.setLayout (ParameterSlider::Layout::row);
    volume.setFormatter ([] (float value)
    {
        return value <= 0.0f ? juce::String ("-inf") : juce::String (juce::Decibels::gainToDecibels (value), 1) + "dB";
    });
    volume.setTooltip ("Output volume. Each voice keeps 9 dB of headroom so chords stay clean; raise this for single notes.");
    voiceMode.setTitle ("Voice mode");
    voiceMode.setTooltip ("Poly plays chords. Mono plays one note and retriggers the envelope on each key; legato glides between overlapping keys without retriggering.");
    frequency.configure ("Freq", ParamIDs::filterFrequency, ui::turquoise, "Hz", 0);
    q.configure ("Res", ParamIDs::filterQ, ui::turquoise, {}, 1);

    pitch.configure ("Pitch", ParamIDs::coarse, ui::stripGraphic, "st", 0);
    pitch.setFormatter ([] (float value) { return signedText (value, 0); });
    detune.configure ("Detune", ParamIDs::fine, ui::stripGraphic, "ct", 1, 100.0f);
    detune.setFormatter ([] (float value) { return signedText (value * 100.0f, 1); });
    glide.configure ("Glide", ParamIDs::portamento, ui::stripGraphic, "s", 3);
    attack.configure ("Attack", ParamIDs::attack, ui::stripGraphic, "s", 3);
    decay.configure ("Decay", ParamIDs::decay, ui::stripGraphic, "s", 3);
    sustain.configure ("Sustain", ParamIDs::sustain, ui::stripGraphic, "%", 0, 100.0f);
    release.configure ("Release", ParamIDs::release, ui::stripGraphic, "s", 3);
    const std::array<std::pair<ParameterSlider*, ParameterSlider::Graphic>, 7> bandControls { {
        { &pitch, ParameterSlider::Graphic::readout }, { &detune, ParameterSlider::Graphic::readout },
        { &glide, ParameterSlider::Graphic::glide }, { &attack, ParameterSlider::Graphic::attack },
        { &decay, ParameterSlider::Graphic::decay }, { &sustain, ParameterSlider::Graphic::sustain },
        { &release, ParameterSlider::Graphic::release } } };
    for (const auto& [slider, graphic] : bandControls)
    {
        slider->setOnBand (true);
        slider->setGraphic (graphic);
    }

    destination.addItemList (msegDestinationNames(), 1);
    destination.setTitle ("Modulation target");
    destination.setJustificationType (juce::Justification::centredLeft);
    msegSlots.onSlotSelected = [this] (int slot) { setActiveMsegSlot (slot); };
    setActiveMsegSlot (0);

    audioProcessor.addChangeListener (this);
    for (auto* parameterId : uiParameterIds)
        audioProcessor.getState().addParameterListener (parameterId, this);
    startTimerHz (30);
    setSize (ui::width, ui::height);
}

EditorView::~EditorView()
{
    stopTimer();
    for (auto* parameterId : uiParameterIds)
        audioProcessor.getState().removeParameterListener (parameterId, this);
    audioProcessor.removeChangeListener (this);
}

std::vector<ParameterSlider*> EditorView::sliders()
{
    return { &volume, &harmonics, &timbre, &morph, &aux, &amount, &mix, &output, &frequency, &q,
             &pitch, &detune, &glide, &attack, &decay, &sustain, &release,
             &msegOffset, &msegScale, &msegRate, &msegRandom };
}

void EditorView::paint (juce::Graphics& g)
{
    g.fillAll (ui::background);

    // Header: settings menu and title on the left, voices and voice mode on the right.
    g.setColour (ui::header);
    g.fillRect (0, 0, getWidth(), 40);
    ui::drawLogo (g, { 60.0f, 14.0f, 32.0f, 14.0f }, 2.2f, ui::header, { ui::magenta, ui::turquoiseLine, ui::ink });
    shownPresetName = audioProcessor.getPresetName();
    g.setFont (fonts.mono.withPointHeight (12.0f));
    g.setColour (ui::ink);
    g.drawText (shownPresetName, 106, 0, 200, 40, juce::Justification::centredLeft);
    g.setFont (fonts.mono.withPointHeight (11.0f));
    g.setColour (ui::muted);
    g.drawText ("volume", volume.getX() - 60, 0, 52, 40, juce::Justification::centredRight);
    g.drawText ("voices", voiceControl.getX() - 56, 0, 48, 40, juce::Justification::centredRight);
    g.drawText ("mode", voiceMode.getX() - 48, 0, 40, 40, juce::Justification::centredRight);

    g.setFont (fonts.medium.withPointHeight (22.0f));
    g.setColour (ui::muted);
    for (const auto& [title, x, y] : { std::tuple { "osc", 28, 64 }, std::tuple { "drive", 628, 64 },
                                       std::tuple { "filter", 628, 212 }, std::tuple { "mod", 28, 494 } })
        g.drawText (title, x, y, 200, 26, juce::Justification::centredLeft);

    // The dark strip behind pitch and envelope: Macro OSC's silver band, inverted.
    g.setColour (ui::strip);
    g.fillRect (0, 372, getWidth(), 104);
    g.setColour (ui::stripTrack);
    g.fillRect ((glide.getRight() + attack.getX()) / 2, 388, 1, 68);

    g.setFont (fonts.mono.withPointHeight (10.0f));
    g.setColour (ui::muted);
    g.drawText ("target", destination.getX(), destination.getY() - 16, 170, 14, juce::Justification::centredLeft);
}

void EditorView::resized()
{
    menuButton.setBounds (20, 4, 32, 32);
    volume.setBounds (376, 10, 140, 20);
    voiceMode.setBounds (764, 8, 168, 24);
    voiceControl.setBounds (592, 0, 116, 40);

    model.setBounds (28, 98, 448, 44);
    previous.setBounds (488, 104, 32, 32);
    next.setBounds (522, 104, 32, 32);
    browse.setBounds (556, 104, 32, 32);
    modelStrip.setBounds (28, 154, 560, 36);
    waveform.setBounds (28, 200, 560, 104);
    layoutRow ({ 28, 316, 560, 32 }, 24, std::array<juce::Component*, 4> { &harmonics, &timbre, &morph, &aux });

    distortionType.setBounds (764, 64, 140, 26);
    distortionPower.setBounds (918, 68, 18, 18);
    distortionDisplay.setBounds (628, 102, 304, 44);
    layoutRow ({ 628, 160, 304, 32 }, 20, std::array<juce::Component*, 3> { &amount, &mix, &output });

    filterType.setBounds (806, 213, 126, 24);
    filterDisplay.setBounds (628, 250, 304, 44);
    layoutRow ({ 628, 316, 304, 32 }, 20, std::array<juce::Component*, 2> { &frequency, &q });

    // Three pitch cells, a rule, then four envelope cells.
    const int cell = (904 - 20 * 7 - 1) / 7;
    auto band = juce::Rectangle<int> (28, 388, 904, 68);
    for (auto* slider : { &pitch, &detune, &glide })
    {
        slider->setBounds (band.removeFromLeft (cell));
        band.removeFromLeft (20);
    }
    band.removeFromLeft (21);
    for (auto* slider : { &attack, &decay, &sustain, &release })
    {
        slider->setBounds (band.removeFromLeft (cell));
        band.removeFromLeft (20);
    }

    // Modulation: title and slots share a line; target and the toggles below.
    msegSlots.setBounds (96, 493, 102, 32);
    loop.setBounds (28, 548, 64, 20);
    sync.setBounds (104, 548, 64, 20);
    destination.setBounds (28, 604, 170, 26);
    msegEditor.setBounds (222, 486, 716, 104);
    layoutRow ({ 228, 598, 704, 32 }, 24, std::array<juce::Component*, 4> { &msegOffset, &msegScale, &msegRate, &msegRandom });
}

void EditorView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! msegEditor.isEditing())
        msegEditor.setPoints (audioProcessor.getMsegPoints (activeMsegSlot));
}

void EditorView::parameterChanged (const juce::String&, float)
{
    parameterUiDirty.store (true, std::memory_order_relaxed);
}

void EditorView::timerCallback()
{
    voiceControl.update();
    if (audioProcessor.getPresetName() != shownPresetName)
        repaint (0, 0, getWidth(), 40);
    if (! parameterUiDirty.exchange (false, std::memory_order_relaxed))
        return;
    for (auto* slider : sliders())
        slider->repaint();
    waveform.refresh();
    distortionDisplay.refresh();
    for (auto* component : std::initializer_list<juce::Component*> {
             &model, &modelStrip, &voiceMode, &distortionPower, &filterDisplay, &msegSlots, &msegEditor, &destination, &loop, &sync })
        component->repaint();
}

void EditorView::stepModel (int delta)
{
    const int count = kModelCount;
    const int current = juce::roundToInt (rawValue (audioProcessor, ParamIDs::model));
    setAsGesture (audioProcessor, ParamIDs::model, static_cast<float> ((current + count + delta) % count));
}

void EditorView::showSettingsMenu()
{
    juce::PopupMenu presets;
    presets.addItem ("Save preset...", [this] { savePreset(); });
    presets.addItem ("Open preset...", [this] { openPreset(); });
    presets.addItem ("Show preset folder", []
    {
        const auto folder = MacroOscIIAudioProcessor::getPresetFolder();
        folder.createDirectory();
        folder.revealToUser();
    });
    presets.addSeparator();
    auto files = MacroOscIIAudioProcessor::getPresetFolder().findChildFiles (juce::File::findFiles, false, "*.mosc2");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
    });
    for (const auto& file : files)
        presets.addItem (file.getFileNameWithoutExtension(), true,
                         file.getFileNameWithoutExtension() == audioProcessor.getPresetName(),
                         [this, file] { loadPreset (file); });
    if (files.isEmpty())
        presets.addItem ("No saved presets", false, false, nullptr);
    presets.addSeparator();
    presets.addItem ("Initialise", [this]
    {
        audioProcessor.resetToDefaults();
        setActiveMsegSlot (activeMsegSlot);
    });

    const float scale = audioProcessor.getEditorScale();
    juce::PopupMenu sizes;
    for (const int percent : { 75, 100, 125, 150 })
        sizes.addItem (juce::String (percent) + "%", true, std::abs (scale * 100.0f - static_cast<float> (percent)) < 1.0f,
                       [this, percent] { if (onScaleChosen) onScaleChosen (static_cast<float> (percent) / 100.0f); });

    juce::PopupMenu menu;
    menu.addSubMenu ("Presets", presets);
    // Only the standalone app owns its audio device; in a DAW the host does.
    if (audioProcessor.showAudioSettings)
        menu.addItem ("Audio/MIDI settings...", [this] { if (audioProcessor.showAudioSettings) audioProcessor.showAudioSettings(); });
    menu.addSeparator();
    menu.addSubMenu ("Interface size", sizes);
    const bool tooltips = audioProcessor.getTooltipsEnabled();
    menu.addItem ("Tooltips", true, tooltips, [this, tooltips] { if (onTooltipsChanged) onTooltipsChanged (! tooltips); });
    menu.addSeparator();
    menu.addItem ("Macro OSC II " + juce::String (JucePlugin_VersionString), false, false, nullptr);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton).withParentComponent (this));
}

void EditorView::savePreset()
{
    const auto folder = MacroOscIIAudioProcessor::getPresetFolder();
    folder.createDirectory();
    presetChooser = std::make_unique<juce::FileChooser> ("Save preset", folder.getChildFile (audioProcessor.getPresetName() + ".mosc2"), "*.mosc2");
    presetChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
                                [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file == juce::File())
            return;
        if (! audioProcessor.savePreset (file.withFileExtension ("mosc2")))
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset not saved",
                                                         "Macro OSC II couldn't write " + file.getFullPathName() + ".");
        repaint (0, 0, getWidth(), 40);
    });
}

void EditorView::openPreset()
{
    const auto folder = MacroOscIIAudioProcessor::getPresetFolder();
    folder.createDirectory();
    presetChooser = std::make_unique<juce::FileChooser> ("Open preset", folder, "*.mosc2");
    presetChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                [this] (const juce::FileChooser& chooser)
    {
        if (const auto file = chooser.getResult(); file != juce::File())
            loadPreset (file);
    });
}

void EditorView::loadPreset (const juce::File& file)
{
    if (! audioProcessor.loadPreset (file))
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset not loaded",
                                                     file.getFileName() + " isn't a Macro OSC II preset.");
        return;
    }
    setActiveMsegSlot (activeMsegSlot);
    repaint (0, 0, getWidth(), 40);
}

void EditorView::setActiveMsegSlot (int slot)
{
    activeMsegSlot = juce::jlimit (0, kMsegSlotCount - 1, slot);
    const auto index = static_cast<size_t> (activeMsegSlot);
    msegSlots.setActiveSlot (activeMsegSlot);
    msegEditor.setActiveSlot (activeMsegSlot);

    destinationAttachment.reset();
    loopAttachment.reset();
    syncAttachment.reset();
    destinationAttachment = std::make_unique<ComboBoxAttachment> (audioProcessor.getState(), ParamIDs::msegDestination[index], destination);
    loopAttachment = std::make_unique<ButtonAttachment> (audioProcessor.getState(), ParamIDs::msegLoop[index], loop);
    syncAttachment = std::make_unique<ButtonAttachment> (audioProcessor.getState(), ParamIDs::msegSync[index], sync);

    msegOffset.configure ("Offset", ParamIDs::msegOffset[index], ui::turquoise, "%", 0);
    msegOffset.setFormatter ([] (float value) { return (value > 0.05f ? "+" : "") + juce::String (juce::roundToInt (value)) + "%"; });
    msegScale.configure ("Scale", ParamIDs::msegAmount[index], ui::turquoise, "%", 0);
    msegScale.setFormatter ([] (float value) { return (value > 0.05f ? "+" : "") + juce::String (juce::roundToInt (value)) + "%"; });
    // Rate reads in note values while synced and in time otherwise.
    const auto* syncId = ParamIDs::msegSync[index];
    msegRate.configure ("Rate", ParamIDs::msegRate[index], ui::turquoise, "s", 2);
    msegRate.setFormatter ([this, syncId] (float value)
    {
        return rawValue (audioProcessor, syncId) >= 0.5f ? msegSyncedDurationLabel (value) : secondsText (value);
    });
    msegRate.setSnapper ([this, syncId] (float value)
    {
        return rawValue (audioProcessor, syncId) >= 0.5f ? snapMsegSyncedBeats (value) : value;
    });
    msegRandom.configure ("Random", ParamIDs::msegRandom[index], ui::turquoise, "%", 0);
    msegRandom.setTooltip ("Moves each point by a random amount, up to this range: new for every note, and for every cycle when looping. "
                           "With a single point, every note gets its own fixed offset.");
}

MacroOscIIAudioProcessorEditor::MacroOscIIAudioProcessorEditor (MacroOscIIAudioProcessor& processorRef)
    : AudioProcessorEditor (&processorRef),
      audioProcessor (processorRef),
      fonts (loadFonts()),
      lookAndFeel (fonts),
      view (processorRef, fonts)
{
    // Read the saved size first: resized() runs during construction, while the
    // window is still at its minimum size, and would otherwise save that.
    const float savedScale = juce::jlimit (minimumScale, maximumScale, audioProcessor.getEditorScale());
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (ui::width * minimumScale), juce::roundToInt (ui::height * minimumScale),
                     juce::roundToInt (ui::width * maximumScale), juce::roundToInt (ui::height * maximumScale));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (static_cast<double> (ui::width) / ui::height);
    view.onScaleChosen = [this] (float scale)
    {
        setSize (juce::roundToInt (ui::width * scale), juce::roundToInt (ui::height * scale));
    };
    view.onTooltipsChanged = [this] (bool enabled) { setTooltipsEnabled (enabled); };
    setTooltipsEnabled (audioProcessor.getTooltipsEnabled());
    setSize (juce::roundToInt (ui::width * savedScale), juce::roundToInt (ui::height * savedScale));
    restoringSize = false;
    audioProcessor.setEditorScale (savedScale);
}

void MacroOscIIAudioProcessorEditor::setTooltipsEnabled (bool enabled)
{
    audioProcessor.setTooltipsEnabled (enabled);
    if (enabled && tooltipWindow == nullptr)
        tooltipWindow = std::make_unique<juce::TooltipWindow> (&view, 650);
    else if (! enabled)
        tooltipWindow.reset();
}

MacroOscIIAudioProcessorEditor::~MacroOscIIAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MacroOscIIAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::background);
}

void MacroOscIIAudioProcessorEditor::resized()
{
    // The view keeps its design size and is scaled as a whole.
    const float scale = static_cast<float> (getWidth()) / ui::width;
    view.setTransform (juce::AffineTransform::scale (scale));
    view.setBounds (0, 0, ui::width, ui::height);
    if (! restoringSize)
        audioProcessor.setEditorScale (scale);
}

EditorFonts MacroOscIIAudioProcessorEditor::loadFonts()
{
    const auto typeface = [] (const void* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (size)); };
    EditorFonts result;
    result.sans = juce::Font (juce::FontOptions (typeface (BinaryData::GeistRegular_ttf, BinaryData::GeistRegular_ttfSize)).withPointHeight (14.0f));
    result.medium = juce::Font (juce::FontOptions (typeface (BinaryData::GeistMedium_ttf, BinaryData::GeistMedium_ttfSize)).withPointHeight (14.0f));
    result.mono = juce::Font (juce::FontOptions (typeface (BinaryData::GeistMonoRegular_ttf, BinaryData::GeistMonoRegular_ttfSize)).withPointHeight (12.0f));
    result.segment = juce::Font (juce::FontOptions (typeface (BinaryData::DSEG14ModernRegular_ttf, BinaryData::DSEG14ModernRegular_ttfSize)).withPointHeight (28.0f));
    return result;
}
} // namespace macro_osc_ii
