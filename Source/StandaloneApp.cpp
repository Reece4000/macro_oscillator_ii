#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "PluginProcessor.h"

namespace macro_osc_ii
{
namespace
{
void hideBluetoothMidiControls (juce::Component& component)
{
    for (int i = 0; i < component.getNumChildComponents(); ++i)
    {
        if (auto* child = component.getChildComponent (i))
        {
            if (auto* button = dynamic_cast<juce::Button*> (child))
            {
                if (button->getButtonText().containsIgnoreCase ("Bluetooth MIDI"))
                {
                    button->setVisible (false);
                    button->setEnabled (false);
                    button->setBounds ({});
                }
            }

            hideBluetoothMidiControls (*child);
        }
    }
}

class SafeAudioSettingsComponent final : public juce::Component,
                                         private juce::Timer
{
public:
    explicit SafeAudioSettingsComponent (juce::AudioDeviceManager& deviceManager)
        : selector (deviceManager, 0, 0, 0, 2, true, false, true, false)
    {
        setOpaque (true);
        addAndMakeVisible (selector);
        setSize (500, 520);
        startTimerHz (4);
        hideBluetoothMidiControls (selector);
    }

    ~SafeAudioSettingsComponent() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        selector.setBounds (getLocalBounds().reduced (10));
        hideBluetoothMidiControls (selector);
    }

    void childBoundsChanged (juce::Component* child) override
    {
        if (child == &selector)
            setSize (getWidth(), selector.getHeight() + 20);
    }

private:
    void timerCallback() override
    {
        hideBluetoothMidiControls (selector);
    }

    juce::AudioDeviceSelectorComponent selector;
};
} // namespace

class NativeTitlebarStandaloneWindow final : public juce::StandaloneFilterWindow
{
public:
    NativeTitlebarStandaloneWindow (const juce::String& title,
                                    juce::Colour backgroundColour,
                                    std::unique_ptr<juce::StandalonePluginHolder> pluginHolderIn)
        : juce::StandaloneFilterWindow (title, backgroundColour, std::move (pluginHolderIn))
    {
        setUsingNativeTitleBar (true);
        // Changing title-bar styles must not add the old title bar's height to
        // the fixed editor, leaving an empty strip below the controls.
        if (auto* editor = getAudioProcessor()->getActiveEditor())
            setContentComponentSize (editor->getWidth(), editor->getHeight());
        hideCustomTitlebarOptionsButton();
        if (auto* processor = dynamic_cast<macro_osc_ii::MacroOscIIAudioProcessor*> (getAudioProcessor()))
            processor->showAudioSettings = [this] { showSafeAudioSettingsDialog(); };
    }

    ~NativeTitlebarStandaloneWindow() override
    {
        if (auto* processor = dynamic_cast<macro_osc_ii::MacroOscIIAudioProcessor*> (getAudioProcessor()))
            processor->showAudioSettings = nullptr;
    }

    void resized() override
    {
        juce::StandaloneFilterWindow::resized();
        hideCustomTitlebarOptionsButton();
    }

    void handleMenuResult (int result)
    {
        if (result == 1)
        {
            showSafeAudioSettingsDialog();
            return;
        }

        juce::StandaloneFilterWindow::handleMenuResult (result);
    }

private:
    void showSafeAudioSettingsDialog()
    {
        juce::DialogWindow::LaunchOptions options;
        auto content = std::make_unique<SafeAudioSettingsComponent> (getDeviceManager());

        options.content.setOwned (content.release());
        options.dialogTitle = "Audio/MIDI Settings";
        options.dialogBackgroundColour = findColour (juce::ResizableWindow::backgroundColourId);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }

    void hideCustomTitlebarOptionsButton()
    {
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (auto* child = getChildComponent (i); child != nullptr && child->getName() == "Options")
                child->setVisible (false);
    }
};

class StandaloneMenuModel final : public juce::MenuBarModel
{
public:
    void setWindow (NativeTitlebarStandaloneWindow* windowToUse) noexcept
    {
        window = windowToUse;
    }

    juce::StringArray getMenuBarNames() override
    {
        return { "Options" };
    }

    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override
    {
        juce::ignoreUnused (topLevelMenuIndex, menuName);

        juce::PopupMenu menu;
        menu.addItem (1, "Audio/MIDI Settings...");
        menu.addSeparator();
        menu.addItem (2, "Save current state...");
        menu.addItem (3, "Load a saved state...");
        menu.addSeparator();
        menu.addItem (4, "Reset to default state");
        return menu;
    }

    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override
    {
        juce::ignoreUnused (topLevelMenuIndex);

        if (window != nullptr)
            window->handleMenuResult (menuItemID);
    }

private:
    NativeTitlebarStandaloneWindow* window {};
};

class NativeTitlebarStandaloneApp final : public juce::JUCEApplication
{
public:
    NativeTitlebarStandaloneApp()
    {
        juce::PropertiesFile::Options options;

        options.applicationName = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #else
        options.folderName = "";
       #endif

        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    NativeTitlebarStandaloneWindow* createWindow()
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }

        return new NativeTitlebarStandaloneWindow (
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
            createPluginHolder());
    }

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        constexpr auto autoOpenMidiDevices =
       #if (JUCE_ANDROID || JUCE_IOS) && ! JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
            true;
       #else
            true;
       #endif

       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig (channels,
                                                                                     juce::numElementsInArray (channels));
       #else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif

        juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
        preferredSetup.bufferSize = 128;

        return std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(),
                                                               false,
                                                               juce::String {},
                                                               &preferredSetup,
                                                               channelConfig,
                                                               autoOpenMidiDevices);
    }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (createWindow());

        if (mainWindow != nullptr)
        {
            menuModel.setWindow (mainWindow.get());
           #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu (&menuModel);
           #else
            mainWindow->setMenuBar (&menuModel);
           #endif

           #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            juce::Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
           #endif

            mainWindow->setVisible (true);
        }
        else
        {
            pluginHolder = createPluginHolder();
        }
    }

    void shutdown() override
    {
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (nullptr);
       #else
        if (mainWindow != nullptr)
            mainWindow->setMenuBar (nullptr);
       #endif

        menuModel.setWindow (nullptr);
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            if (auto* holder = mainWindow->getPluginHolder())
                holder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []()
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    juce::ApplicationProperties appProperties;
    StandaloneMenuModel menuModel;
    std::unique_ptr<NativeTitlebarStandaloneWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};
} // namespace macro_osc_ii

START_JUCE_APPLICATION (macro_osc_ii::NativeTitlebarStandaloneApp)
