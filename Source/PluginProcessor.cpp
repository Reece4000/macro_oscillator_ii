#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace macro_osc_ii
{
namespace
{
constexpr double kDcBlockerCutoffHz = 20.0;

juce::NormalisableRange<float> skewedRange (float start, float end, float interval, float skew)
{
    return { start, end, interval, skew };
}

[[nodiscard]] juce::String msegPointsPropertyName (int slotIndex)
{
    return "msegPoints" + juce::String (juce::jlimit (0, kMsegSlotCount - 1, slotIndex) + 1);
}
} // namespace

juce::StringArray plaitsModelNames()
{
    juce::StringArray names;
    for (const auto& model : kPlaitsModels)
        names.add (model.name);
    return names;
}

MacroOscIIAudioProcessor::MacroOscIIAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MacroOscIIState", createParameterLayout())
{
    retainedMsegShapes.reserve (128);
    cacheRawParameterPointers();

    synth.addSound (new PlaitsSound());
    for (auto& voice : plaitsVoices)
    {
        voice = new PlaitsVoice();
        synth.addVoice (voice);
    }
    synth.setNoteStealingEnabled (true);

    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        setMsegPoints (slot, MsegShape::defaultPoints(), false);
}

void MacroOscIIAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.allNotesOff (0, false);
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (auto* voice : plaitsVoices)
        voice->prepare (sampleRate);

    if (sampleRate > 0.0)
        dcBlockerCoefficient = static_cast<float> (std::exp (-2.0 * juce::MathConstants<double>::pi * kDcBlockerCutoffHz / sampleRate));
    else
        dcBlockerCoefficient = 0.995f;

    resetOutputDcBlocker();
    outputLimiter.prepare (sampleRate > 0.0 ? sampleRate : 44100.0);
}

void MacroOscIIAudioProcessor::releaseResources()
{
    synth.allNotesOff (0, false);
    resetOutputDcBlocker();
    outputLimiter.reset();
}

bool MacroOscIIAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void MacroOscIIAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    updateHostTempo();
    const auto voiceParameters = buildVoiceParameters();
    for (auto* voice : plaitsVoices)
        voice->setParameters (voiceParameters);
    synth.configure (static_cast<VoiceMode> (juce::jlimit (0, 2, juce::roundToInt (getFloatParameter (rawParameters.voiceMode)))),
                     juce::roundToInt (getFloatParameter (rawParameters.voices)));

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    applyOutputDcBlocker (buffer);
    // Chords and hot distortion settings can sum past full scale; keep the output clean.
    outputLimiter.process (buffer);
    activeVoiceCount.store (static_cast<int> (std::count_if (plaitsVoices.begin(), plaitsVoices.end(),
        [] (const PlaitsVoice* voice) { return voice->isVoiceActive(); })), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* MacroOscIIAudioProcessor::createEditor()
{
    return new MacroOscIIAudioProcessorEditor (*this);
}

void MacroOscIIAudioProcessor::resetToDefaults()
{
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
            ranged->endChangeGesture();
        }
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        setMsegPoints (slot, MsegShape::defaultPoints(), true);
    parameters.state.setProperty ("presetName", "init", nullptr);
}

float MacroOscIIAudioProcessor::getEditorScale() const
{
    return static_cast<float> (parameters.state.getProperty ("editorScale", 1.0f));
}

void MacroOscIIAudioProcessor::setEditorScale (float scale)
{
    parameters.state.setProperty ("editorScale", scale, nullptr);
}

bool MacroOscIIAudioProcessor::getTooltipsEnabled() const
{
    return static_cast<bool> (parameters.state.getProperty ("tooltips", true));
}

void MacroOscIIAudioProcessor::setTooltipsEnabled (bool enabled)
{
    parameters.state.setProperty ("tooltips", enabled, nullptr);
}

juce::ValueTree MacroOscIIAudioProcessor::createStateTree()
{
    auto state = parameters.copyState();
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        state.setProperty (msegPointsPropertyName (slot), MsegShape::serialise (getMsegPoints (slot)), nullptr);
    return state;
}

void MacroOscIIAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

juce::File MacroOscIIAudioProcessor::getPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Macro OSC II").getChildFile ("Presets");
}

juce::String MacroOscIIAudioProcessor::getPresetName() const
{
    return parameters.state.getProperty ("presetName", "init").toString();
}

bool MacroOscIIAudioProcessor::savePreset (const juce::File& file)
{
    auto state = createStateTree();
    state.removeProperty ("editorScale", nullptr);
    state.removeProperty ("tooltips", nullptr);
    state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    const auto xml = state.createXml();
    if (xml == nullptr || ! file.getParentDirectory().createDirectory() || ! xml->writeTo (file))
        return false;
    parameters.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    return true;
}

bool MacroOscIIAudioProcessor::loadPreset (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != parameters.state.getType())
        return false;
    // Keep this session's editor preferences; take everything else from the file.
    state.setProperty ("editorScale", getEditorScale(), nullptr);
    state.setProperty ("tooltips", getTooltipsEnabled(), nullptr);
    state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    juce::MemoryBlock block;
    if (const auto merged = state.createXml())
        copyXmlToBinary (*merged, block);
    setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    return true;
}

void MacroOscIIAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != parameters.state.getType())
        return;

    std::array<juce::String, kMsegSlotCount> encodedPoints;
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        encodedPoints[static_cast<size_t> (slot)] = state.getProperty (msegPointsPropertyName (slot)).toString();

    parameters.replaceState (state);
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        setMsegPoints (slot, MsegShape::parse (encodedPoints[static_cast<size_t> (slot)]), false);

    sendChangeMessage();
}

std::vector<MsegPoint> MacroOscIIAudioProcessor::getMsegPoints (int slotIndex) const
{
    if (auto shape = getMsegShape (slotIndex))
        return shape->points;

    return MsegShape::defaultPoints();
}

std::shared_ptr<const MsegShape> MacroOscIIAudioProcessor::getMsegShape (int slotIndex) const
{
    const int clampedSlot = juce::jlimit (0, kMsegSlotCount - 1, slotIndex);
    return std::atomic_load_explicit (&currentMsegShapes[static_cast<size_t> (clampedSlot)], std::memory_order_acquire);
}

const MsegShape* MacroOscIIAudioProcessor::getAudioMsegShape (int slotIndex) const noexcept
{
    const int clampedSlot = juce::jlimit (0, kMsegSlotCount - 1, slotIndex);
    return currentAudioMsegShapes[static_cast<size_t> (clampedSlot)].load (std::memory_order_acquire);
}

void MacroOscIIAudioProcessor::cacheRawParameterPointers()
{
    rawParameters.filterType = parameters.getRawParameterValue (ParamIDs::filterType);
    rawParameters.filterFrequency = parameters.getRawParameterValue (ParamIDs::filterFrequency);
    rawParameters.filterQ = parameters.getRawParameterValue (ParamIDs::filterQ);
    rawParameters.distortionType = parameters.getRawParameterValue (ParamIDs::distortionType);
    rawParameters.distortionAmount = parameters.getRawParameterValue (ParamIDs::distortionAmount);
    rawParameters.distortionMix = parameters.getRawParameterValue (ParamIDs::distortionMix);
    rawParameters.distortionOutput = parameters.getRawParameterValue (ParamIDs::distortionOutput);
    rawParameters.model = parameters.getRawParameterValue (ParamIDs::model);
    rawParameters.harmonics = parameters.getRawParameterValue (ParamIDs::harmonics);
    rawParameters.timbre = parameters.getRawParameterValue (ParamIDs::timbre);
    rawParameters.morph = parameters.getRawParameterValue (ParamIDs::morph);
    rawParameters.auxMix = parameters.getRawParameterValue (ParamIDs::auxMix);
    rawParameters.level = parameters.getRawParameterValue (ParamIDs::level);
    rawParameters.coarse = parameters.getRawParameterValue (ParamIDs::coarse);
    rawParameters.fine = parameters.getRawParameterValue (ParamIDs::fine);
    rawParameters.portamento = parameters.getRawParameterValue (ParamIDs::portamento);
    rawParameters.voiceMode = parameters.getRawParameterValue (ParamIDs::voiceMode);
    rawParameters.voices = parameters.getRawParameterValue (ParamIDs::voices);
    rawParameters.attack = parameters.getRawParameterValue (ParamIDs::attack);
    rawParameters.decay = parameters.getRawParameterValue (ParamIDs::decay);
    rawParameters.sustain = parameters.getRawParameterValue (ParamIDs::sustain);
    rawParameters.release = parameters.getRawParameterValue (ParamIDs::release);

    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        const auto index = static_cast<size_t> (slot);
        rawParameters.msegDestination[index] = parameters.getRawParameterValue (ParamIDs::msegDestination[index]);
        rawParameters.msegAmount[index] = parameters.getRawParameterValue (ParamIDs::msegAmount[index]);
        rawParameters.msegOffset[index] = parameters.getRawParameterValue (ParamIDs::msegOffset[index]);
        rawParameters.msegRate[index] = parameters.getRawParameterValue (ParamIDs::msegRate[index]);
        rawParameters.msegLoop[index] = parameters.getRawParameterValue (ParamIDs::msegLoop[index]);
        rawParameters.msegRandom[index] = parameters.getRawParameterValue (ParamIDs::msegRandom[index]);
        rawParameters.msegSync[index] = parameters.getRawParameterValue (ParamIDs::msegSync[index]);
    }
}

void MacroOscIIAudioProcessor::resetOutputDcBlocker() noexcept
{
    dcBlockerPreviousInput.fill (0.0f);
    dcBlockerPreviousOutput.fill (0.0f);
}

void MacroOscIIAudioProcessor::applyOutputDcBlocker (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = juce::jmin (buffer.getNumChannels(), static_cast<int> (dcBlockerPreviousInput.size()));
    const auto samples = buffer.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        auto previousInput = dcBlockerPreviousInput[static_cast<size_t> (channel)];
        auto previousOutput = dcBlockerPreviousOutput[static_cast<size_t> (channel)];

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto input = data[sample];
            const auto output = input - previousInput + dcBlockerCoefficient * previousOutput;
            data[sample] = output;
            previousInput = input;
            previousOutput = output;
        }

        dcBlockerPreviousInput[static_cast<size_t> (channel)] = previousInput;
        dcBlockerPreviousOutput[static_cast<size_t> (channel)] = previousOutput;
    }
}

void MacroOscIIAudioProcessor::updateHostTempo() noexcept
{
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
            {
                const float tempo = static_cast<float> (*bpm);
                if (std::isfinite (tempo) && tempo > 0.0f)
                    currentTempoBpm.store (juce::jlimit (20.0f, 300.0f, tempo), std::memory_order_relaxed);
            }
        }
    }
}

void MacroOscIIAudioProcessor::setMsegPoints (int slotIndex, std::vector<MsegPoint> points, bool notifyEditor)
{
    const int clampedSlot = juce::jlimit (0, kMsegSlotCount - 1, slotIndex);
    auto shape = std::make_shared<MsegShape>();
    shape->points = MsegShape::sanitize (std::move (points));
    if (notifyEditor)
        parameters.state.setProperty (msegPointsPropertyName (clampedSlot), MsegShape::serialise (shape->points), nullptr);

    auto publishedShape = std::static_pointer_cast<const MsegShape> (shape);
    retainedMsegShapes.push_back (publishedShape);
    std::atomic_store_explicit (&currentMsegShapes[static_cast<size_t> (clampedSlot)],
                                publishedShape,
                                std::memory_order_release);
    currentAudioMsegShapes[static_cast<size_t> (clampedSlot)].store (publishedShape.get(), std::memory_order_release);
    if (notifyEditor)
        sendChangeMessage();
}

MacroOscIIAudioProcessor::APVTS::ParameterLayout MacroOscIIAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    // Oscillator: Plaits' model and its three macros, plus the out/aux balance.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::model, 1 }, "Model", plaitsModelNames(), 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::harmonics, 1 }, "Harmonics", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::timbre, 1 }, "Timbre", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::morph, 1 }, "Morph", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::auxMix, 1 }, "Aux Mix", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f));

    // Voice: level, pitch, glide, envelope and allocation.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::level, 1 }, "Level", juce::NormalisableRange<float> { 0.0f, 1.5f, 0.001f }, 0.8f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::coarse, 1 }, "Pitch", juce::NormalisableRange<float> { -36.0f, 36.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::fine, 1 }, "Detune", juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::portamento, 1 }, "Portamento", skewedRange (0.0f, 2.0f, 0.001f, 0.35f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::attack, 1 }, "Attack", skewedRange (0.0f, 10.0f, 0.001f, 0.35f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::decay, 1 }, "Decay", skewedRange (0.0f, 10.0f, 0.001f, 0.35f), 1.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::sustain, 1 }, "Sustain", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::release, 1 }, "Release", skewedRange (0.0f, 10.0f, 0.001f, 0.35f), 0.35f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::voiceMode, 1 }, "Voice Mode", juce::StringArray { "Poly", "Mono", "Legato" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIDs::voices, 1 }, "Voices", 1, maxPolyphony, maxPolyphony));

    // Per-voice distortion and filter.
    juce::StringArray distortionNames;
    for (const auto name : lodo::engine::fx::distortionNames)
        distortionNames.add (juce::String (name.data(), name.size()));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::distortionType, 1 }, "Distortion Type", distortionNames, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::distortionAmount, 1 }, "Distortion Amount",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.25f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::distortionMix, 1 }, "Distortion Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::distortionOutput, 1 }, "Distortion Output",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::filterType, 1 }, "Filter Type",
        juce::StringArray { "Off", "LP", "BP", "HP" }, 0));
    auto frequencyRange = juce::NormalisableRange<float> {
        20.0f, 20000.0f,
        [] (float lo, float hi, float x) { return lo * std::pow (hi / lo, x); },
        [] (float lo, float hi, float x) { return std::log (x / lo) / std::log (hi / lo); }
    };
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::filterFrequency, 1 }, "Filter Frequency", frequencyRange, 16000.0f));
    auto qRange = juce::NormalisableRange<float> {
        1.0f, 30.0f,
        [] (float lo, float hi, float x) { return lo * std::pow (hi / lo, x); },
        [] (float lo, float hi, float x) { return std::log (x / lo) / std::log (hi / lo); }
    };
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::filterQ, 1 }, "Filter Q", qRange, 1.0f));

    // Modulation: three MSEG slots.
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        const auto index = static_cast<size_t> (slot);
        const auto prefix = "MSEG " + juce::String (slot + 1) + " ";
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::msegDestination[index], 1 }, prefix + "Destination", msegDestinationNames(),
            slot == 0 ? static_cast<int> (MsegDestination::timbre) : static_cast<int> (MsegDestination::off)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::msegAmount[index], 1 }, prefix + "Amount",
            juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f }, 50.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::msegOffset[index], 1 }, prefix + "Offset",
            juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f }, 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::msegRate[index], 1 }, prefix + "Rate",
            skewedRange (0.05f, 16.0f, 0.001f, 0.35f), 1.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::msegRandom[index], 1 }, prefix + "Random",
            juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::msegLoop[index], 1 }, prefix + "Loop", true));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::msegSync[index], 1 }, prefix + "Sync", false));
    }

    return layout;
}

PlaitsVoiceParameters MacroOscIIAudioProcessor::buildVoiceParameters() const
{
    PlaitsVoiceParameters result;
    result.effects.filterType = juce::roundToInt (getFloatParameter (rawParameters.filterType));
    result.effects.filterFrequency = getFloatParameter (rawParameters.filterFrequency);
    result.effects.filterQ = getFloatParameter (rawParameters.filterQ);
    result.effects.distortionType = juce::roundToInt (getFloatParameter (rawParameters.distortionType));
    result.effects.distortionAmount = getFloatParameter (rawParameters.distortionAmount);
    result.effects.distortionMix = getFloatParameter (rawParameters.distortionMix);
    result.effects.distortionOutput = getFloatParameter (rawParameters.distortionOutput);
    result.model = juce::roundToInt (getFloatParameter (rawParameters.model));
    result.harmonics = getFloatParameter (rawParameters.harmonics);
    result.timbre = getFloatParameter (rawParameters.timbre);
    result.morph = getFloatParameter (rawParameters.morph);
    result.auxMix = getFloatParameter (rawParameters.auxMix);
    result.level = getFloatParameter (rawParameters.level);
    result.coarseSemitones = getFloatParameter (rawParameters.coarse);
    result.fineSemitones = getFloatParameter (rawParameters.fine);
    result.portamentoSeconds = getFloatParameter (rawParameters.portamento);
    result.attackSeconds = getFloatParameter (rawParameters.attack);
    result.decaySeconds = getFloatParameter (rawParameters.decay);
    result.sustainLevel = getFloatParameter (rawParameters.sustain);
    result.releaseSeconds = getFloatParameter (rawParameters.release);

    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        const auto index = static_cast<size_t> (slot);
        auto& target = result.msegSlots[static_cast<size_t> (slot)];
        target.destination = juce::roundToInt (getFloatParameter (rawParameters.msegDestination[index]));
        target.amountPercent = getFloatParameter (rawParameters.msegAmount[index]);
        target.offsetPercent = getFloatParameter (rawParameters.msegOffset[index]);
        const float rateValue = getFloatParameter (rawParameters.msegRate[index]);
        const bool syncDuration = getFloatParameter (rawParameters.msegSync[index]) >= 0.5f;
        target.rateSeconds = syncDuration
            ? msegSyncedBeatsToSeconds (rateValue, currentTempoBpm.load (std::memory_order_relaxed))
            : rateValue;
        target.loop = getFloatParameter (rawParameters.msegLoop[index]) >= 0.5f;
        target.randomPercent = getFloatParameter (rawParameters.msegRandom[index]);
        target.shape = getAudioMsegShape (slot);
    }

    return result;
}

float MacroOscIIAudioProcessor::getFloatParameter (const std::atomic<float>* value) noexcept
{
    if (value != nullptr)
        return value->load (std::memory_order_relaxed);

    return 0.0f;
}
} // namespace macro_osc_ii

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new macro_osc_ii::MacroOscIIAudioProcessor();
}
