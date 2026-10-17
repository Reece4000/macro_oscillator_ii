#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <iostream>
#include <random>
#include <stdexcept>

namespace
{
void require (bool condition, const char* message)
{
    if (! condition) throw std::runtime_error (message);
}

void setParameter (juce::AudioPluginInstance& plugin, const juce::String& name, float normalised)
{
    for (auto* parameter : plugin.getParameters())
        if (parameter->getName (128) == name)
        {
            parameter->setValueNotifyingHost (normalised);
            return;
        }
    throw std::runtime_error ("missing VST3 parameter: " + name.toStdString());
}

void runKeyboardSequence (juce::AudioPluginInstance& plugin, double rate, int blockSize)
{
    plugin.releaseResources();
    plugin.setRateAndBufferSizeDetails (rate, blockSize);
    plugin.prepareToPlay (rate, blockSize);
    setParameter (plugin, "Model", 12.0f / 46.0f);
    setParameter (plugin, "Attack", 0.0f);
    setParameter (plugin, "Sustain", 1.0f);
    setParameter (plugin, "Release", 0.0f);
    setParameter (plugin, "Portamento", 0.0f);
    setParameter (plugin, "MSEG 1 Destination", 0.0f);

    juce::AudioBuffer<float> audio (2, blockSize);
    std::mt19937 random (0x504f4c59);
    for (int pass = 0; pass < 12; ++pass)
    {
        // More than eight overlapping notes, followed by all the corresponding
        // key-ups on the original channels, then callbacks without any MIDI.
        std::array<std::pair<int, int>, 24> notes;
        for (auto& note : notes)
        {
            note = {1 + static_cast<int> (random() % 4), 36 + static_cast<int> (random() % 60)};
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (note.first, note.second, 0.8f),
                           static_cast<int> (random() % static_cast<unsigned int> (blockSize)));
            plugin.processBlock (audio, midi);
        }
        std::shuffle (notes.begin(), notes.end(), random);
        for (const auto& note : notes)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOff (note.first, note.second),
                           static_cast<int> (random() % static_cast<unsigned int> (blockSize)));
            plugin.processBlock (audio, midi);
        }
        for (int frames = 0; frames < static_cast<int> (rate / 2); frames += blockSize)
        {
            juce::MidiBuffer midi;
            plugin.processBlock (audio, midi);
        }
        require (audio.getMagnitude (0, blockSize) < 1.0e-6f, "VST3 replayed a released note with all keys up");

        // Starting a new pitch must not bring any old pitch back: compare it
        // with the same note after a complete processor playback reset.
        auto renderNewNote = [&]
        {
            juce::AudioBuffer<float> result (2, blockSize * 16);
            result.clear();
            for (int block = 0; block < 16; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 90, 0.8f), 0);
                plugin.processBlock (audio, midi);
                for (int ch = 0; ch < 2; ++ch)
                    result.copyFrom (ch, block * blockSize, audio, ch, 0, blockSize);
            }
            return result;
        };
        const auto reused = renderNewNote();
        plugin.releaseResources();
        plugin.prepareToPlay (rate, blockSize);
        const auto clean = renderNewNote();
        float difference = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < clean.getNumSamples(); ++i)
                difference = std::max (difference, std::abs (reused.getSample (ch, i) - clean.getSample (ch, i)));
        require (clean.getMagnitude (0, clean.getNumSamples()) > 0.001f, "VST3 keyboard test produced no audio");
        require (difference < 1.0e-6f, "VST3 replayed old note history when a new key started");
        plugin.releaseResources();
        plugin.prepareToPlay (rate, blockSize);
    }
    plugin.releaseResources();
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        require (argc == 2, "usage: MacroOscIIVst3Tests /path/to/Macro OSC II.vst3");
        juce::VST3PluginFormatHeadless format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (descriptions, juce::File (argv[1]).getFullPathName());
        require (descriptions.size() == 1, "could not scan Macro OSC II VST3");
        juce::String error;
        auto plugin = format.createInstanceFromDescription (*descriptions[0], 48000.0, 512, error);
        if (! plugin) throw std::runtime_error (error.toStdString());
        for (double rate : { 44100.0, 48000.0, 96000.0 })
            for (int block : { 7, 64, 128, 256, 511 })
            {
                runKeyboardSequence (*plugin, rate, block);
                std::cout << "PASS: hosted VST3 keyboard at " << rate << " Hz / " << block << " frames\n";
            }
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
