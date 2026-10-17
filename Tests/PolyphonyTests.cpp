#include "DSP/PlaitsVoice.h"
#include "PluginProcessor.h"
#include "stmlib/utils/random.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

namespace macro_osc_ii
{
bool runPolyphonyTests();
namespace
{
struct Keyboard
{
    PlaitsSynthesiser synth;
    PlaitsVoiceParameters parameters;
    double rate { 48000.0 };

    Keyboard()
    {
        parameters.model = 10; // two-operator FM; no modulation leaves a sine
        parameters.timbre = 0.0f;
        parameters.attackSeconds = 0.0f;
        parameters.sustainLevel = 1.0f;
        parameters.releaseSeconds = 0.02f;
        synth.addSound (new PlaitsSound());
        for (int i = 0; i < 8; ++i)
            synth.addVoice (new PlaitsVoice());
        synth.setCurrentPlaybackSampleRate (rate);
        synth.setMinimumRenderingSubdivisionSize (1, true);
        update();
        for (int i = 0; i < synth.getNumVoices(); ++i)
            static_cast<PlaitsVoice*> (synth.getVoice (i))->prepare (rate);
    }

    void update()
    {
        for (int i = 0; i < synth.getNumVoices(); ++i)
            static_cast<PlaitsVoice*> (synth.getVoice (i))->setParameters (parameters);
    }

    juce::AudioBuffer<float> render (int frames)
    {
        juce::AudioBuffer<float> result (2, frames);
        result.clear();
        synth.renderNextBlock (result, {}, 0, frames);
        return result;
    }

    int active() const
    {
        int count = 0;
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (synth.getVoice (i)->isVoiceActive()) ++count;
        return count;
    }
};

void check (bool condition, const char* message)
{
    if (! condition) throw std::runtime_error (message);
}

void slowAttack()
{
    Keyboard keyboard;
    keyboard.parameters.attackSeconds = 10.0f;
    keyboard.update();
    keyboard.synth.noteOn (1, 60, 0.8f);
    keyboard.render (1);
    check (keyboard.active() == 1, "a short callback killed a note before its slow attack became audible");
}

void repeatedNotes()
{
    Keyboard keyboard;
    keyboard.synth.noteOn (1, 60, 0.8f);
    const auto reference = keyboard.render (1024);
    for (int repeat = 0; repeat < 12; ++repeat)
    {
        keyboard.synth.noteOff (1, 60, 0.0f, true);
        keyboard.render (2000);
        check (keyboard.active() == 0, "released note remains allocated");
        keyboard.synth.noteOn (1, 60, 0.8f);
        const auto candidate = keyboard.render (1024);
        float peak = 0.0f;
        for (int i = 0; i < 1024; ++i)
            peak = std::max (peak, std::abs (reference.getSample (0, i) - candidate.getSample (0, i)));
        check (peak < 1.0e-6f, "the same note changes sound when an old voice is reused");
    }
}

void releasedNotesStayReleased()
{
    // Fill and retire every voice at different pitches, then compare the next
    // note with an instrument that has never played those old notes.
    for (int model = 0; model < kModelCount; ++model)
    {
        Keyboard used, fresh;
        used.parameters.model = fresh.parameters.model = model;
        used.update();
        fresh.update();
        for (int note = 48; note < 56; ++note)
        {
            used.synth.noteOn (1, note, 0.8f);
            used.render (1024);
            used.synth.noteOff (1, note, 0.0f, true);
            used.render (2000);
        }
        check (used.active() == 0, "released notes remained active before another key was played");
        check (used.render (256).getMagnitude (0, 256) == 0.0f, "released notes replayed without new MIDI");
        stmlib::Random::Seed (1);
        used.synth.noteOn (1, 84, 0.8f);
        const auto actual = used.render (4096);
        stmlib::Random::Seed (1);
        fresh.synth.noteOn (1, 84, 0.8f);
        const auto expected = fresh.render (4096);
        float difference = 0.0f;
        for (int i = 0; i < actual.getNumSamples(); ++i)
            difference = std::max (difference, std::abs (actual.getSample (0, i) - expected.getSample (0, i)));
        if (difference > 1.0e-6f)
            throw std::runtime_error ("old note history replayed on voice reuse, model=" + std::to_string (model)
                                      + " peak difference=" + std::to_string (difference));
    }
}

void stealing()
{
    Keyboard reference, keyboard;
    for (int note = 60; note < 76; note += 2)
    {
        reference.synth.noteOn (1, note, 0.8f);
        keyboard.synth.noteOn (1, note, 0.8f);
    }
    reference.render (4096);
    keyboard.render (4096);
    keyboard.synth.noteOn (1, 84, 0.8f);
    const auto expected = reference.render (1);
    const auto stolen = keyboard.render (1);
    const float discontinuity = std::abs (expected.getSample (0, 0) - stolen.getSample (0, 0));
    check (keyboard.active() == 8, "voice limit changed after stealing");
    check (discontinuity < 0.01f, "stealing a held voice abruptly cuts its waveform");
}

void overlappingKeyboardNotes()
{
    // Exercise real, timestamped MIDI, including repeats, overlapping release
    // tails, velocity-zero note-offs, channels, and more keys than voices.
    for (const int blockSize : { 1, 7, 23, 24, 64, 128, 511 })
    {
        Keyboard keyboard;
        std::mt19937 random (0x4b4559);
        std::array<std::array<bool, 128>, 4> held {};
        std::array<std::array<int, 128>, 4> releasedAt {};
        int clock = 0;
        for (int block = 0; block < 1500; ++block)
        {
            juce::MidiBuffer midi;
            const int channel = static_cast<int> (random() % 4);
            const int note = 48 + static_cast<int> (random() % 36);
            const int offset = static_cast<int> (random() % static_cast<unsigned int> (blockSize));
            auto& down = held[static_cast<size_t> (channel)][static_cast<size_t> (note)];
            if (! down && random() % 3 != 0)
            {
                midi.addEvent (juce::MidiMessage::noteOn (channel + 1, note, 0.8f), offset);
                down = true;
            }
            else
            {
                midi.addEvent (random() % 2 == 0 ? juce::MidiMessage::noteOff (channel + 1, note)
                                               : juce::MidiMessage::noteOn (channel + 1, note, 0.0f), offset);
                down = false;
                releasedAt[static_cast<size_t> (channel)][static_cast<size_t> (note)] = clock + offset;
            }
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            keyboard.synth.renderNextBlock (audio, midi, 0, blockSize);
            clock += blockSize;
            for (int v = 0; v < keyboard.synth.getNumVoices(); ++v)
            {
                const auto* voice = keyboard.synth.getVoice (v);
                if (! voice->isVoiceActive()) continue;
                const auto pitch = static_cast<size_t> (voice->getCurrentlyPlayingNote());
                for (size_t ch = 0; ch < held.size(); ++ch)
                    if (voice->isPlayingChannel (static_cast<int> (ch) + 1) && ! held[ch][pitch])
                        check (clock - releasedAt[ch][pitch] <= 1008,
                               "a released keyboard note survived its release during voice rotation");
            }
        }
        for (size_t ch = 0; ch < held.size(); ++ch)
            for (size_t note = 0; note < held[ch].size(); ++note)
                if (held[ch][note]) keyboard.synth.noteOff (static_cast<int> (ch) + 1, static_cast<int> (note), 0.0f, true);
        keyboard.render (2000);
        check (keyboard.active() == 0, "random keyboard sequence left released notes allocated");
        check (keyboard.render (512).getMagnitude (0, 512) == 0.0f, "random keyboard sequence replayed after key-up");
    }
}

void pedalAfterRelease()
{
    for (int releaseEvent = 0; releaseEvent < 3; ++releaseEvent)
    {
        Keyboard keyboard;
        keyboard.synth.noteOn (1, 60, 0.8f);
        keyboard.render (1024);
        keyboard.synth.noteOff (1, 60, 0.0f, true);
        keyboard.render (720); // Three quarters of the 20 ms release has elapsed.
        auto* voice = keyboard.synth.getVoice (0);
        check (! voice->isKeyDown() && ! voice->isSustainPedalDown() && ! voice->isSostenutoPedalDown(),
               "released note retained key or pedal ownership");
        if (releaseEvent == 0) keyboard.synth.handleSustainPedal (1, false);
        if (releaseEvent == 1) keyboard.synth.noteOff (1, 60, 0.0f, true);
        if (releaseEvent == 2) keyboard.synth.allNotesOff (1, true);
        keyboard.render (300);
        check (keyboard.active() == 0, "another release message restarted an already released note's timer");
    }
}

void releasedEffectHistory()
{
    for (int type = 0; type <= lodo::engine::fx::maxDistortionType; ++type)
    {
        Keyboard used, fresh;
        used.parameters.effects.distortionType = type;
        used.parameters.effects.distortionAmount = 0.8f;
        used.parameters.effects.filterType = 1 + type % 3;
        used.parameters.effects.filterFrequency = 800.0f;
        used.parameters.effects.filterQ = 5.0f;
        fresh.parameters = used.parameters;
        used.update();
        fresh.update();
        for (int note = 48; note < 64; ++note)
        {
            used.synth.noteOn (1, note, 0.8f);
            used.render (237);
        }
        for (int note = 48; note < 64; ++note) used.synth.noteOff (1, note, 0.0f, true);
        used.render (2000);
        check (used.active() == 0, "effect voice survived release");
        used.synth.noteOn (1, 84, 0.8f);
        const auto actual = used.render (4096);
        fresh.synth.noteOn (1, 84, 0.8f);
        const auto expected = fresh.render (4096);
        float difference = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < actual.getNumSamples(); ++i)
                difference = std::max (difference, std::abs (actual.getSample (ch, i) - expected.getSample (ch, i)));
        if (difference > 1.0e-5f)
            throw std::runtime_error ("released effect history replayed, distortion=" + std::to_string (type)
                                      + " difference=" + std::to_string (difference));
    }
}

void pedalsAndRelease()
{
    Keyboard keyboard;
    for (int note = 60; note < 68; ++note) keyboard.synth.noteOn (1, note, 0.8f);
    keyboard.render (1000);
    keyboard.synth.handleSustainPedal (1, true);
    for (int note = 60; note < 68; ++note) keyboard.synth.noteOff (1, note, 0.0f, true);
    keyboard.render (2000);
    check (keyboard.active() == 8, "sustain failed to hold released keys");
    keyboard.synth.handleSustainPedal (1, false);
    keyboard.render (2000);
    check (keyboard.active() == 0, "pedal-up left hanging voices");
    for (int note = 60; note < 68; ++note) keyboard.synth.noteOn (1, note, 0.8f);
    keyboard.render (1000);
    keyboard.synth.allNotesOff (0, false);
    check (keyboard.active() == 0, "all-sound-off left hanging voices");
    check (keyboard.render (256).getMagnitude (0, 256) == 0.0f, "panic left audible tails");
}

void midiTiming()
{
    MacroOscIIAudioProcessor processor;
    for (const auto& parameter : std::array<std::pair<const char*, float>, 5> {{
             { ParamIDs::model, 10.0f }, { ParamIDs::attack, 0.0f },
             { ParamIDs::release, 0.0f }, { ParamIDs::sustain, 1.0f },
             { ParamIDs::msegDestination[0], 0.0f } }})
    {
        auto* value = processor.getState().getParameter (parameter.first);
        value->setValueNotifyingHost (value->convertTo0to1 (parameter.second));
    }
    processor.prepareToPlay (48000.0, 64);
    juce::AudioBuffer<float> buffer (2, 64);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 72, 0.8f), 10);
    midi.addEvent (juce::MidiMessage::noteOff (1, 72), 20);
    processor.processBlock (buffer, midi);
    check (buffer.getMagnitude (10, 10) > 0.001f, "nearby MIDI events swallowed a short note");
    check (buffer.getMagnitude (0, 10) == 0.0f, "MIDI note started before its timestamp");
}

void playbackRestart()
{
    MacroOscIIAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    processor.processBlock (buffer, midi);
    processor.releaseResources();
    processor.prepareToPlay (48000.0, 256);
    midi.clear();
    processor.processBlock (buffer, midi);
    check (buffer.getMagnitude (0, 256) == 0.0f, "playback restart resurrected an old held note");
}

void glideAcrossVoices()
{
    Keyboard keyboard;
    keyboard.parameters.portamentoSeconds = 0.2f;
    keyboard.update();
    keyboard.synth.noteOn (1, 48, 0.8f);
    keyboard.render (1000);
    keyboard.synth.noteOn (1, 72, 0.8f);
    juce::SynthesiserVoice* upper = nullptr;
    for (int i = 0; i < keyboard.synth.getNumVoices(); ++i)
        if (keyboard.synth.getVoice (i)->getCurrentlyPlayingNote() == 72)
            upper = keyboard.synth.getVoice (i);
    check (upper != nullptr, "second glide note missing");
    const auto crossings = [] (const juce::AudioBuffer<float>& audio, int start, int frames)
    {
        int count = 0;
        for (int i = start + 1; i < start + frames; ++i)
            if (audio.getSample (0, i - 1) <= 0.0f && audio.getSample (0, i) > 0.0f) ++count;
        return count;
    };
    juce::AudioBuffer<float> audio (2, 14400);
    audio.clear();
    upper->renderNextBlock (audio, 0, audio.getNumSamples());
    check (crossings (audio, 0, 960) < 7, "a new voice skipped glide from the previous played note");
    check (crossings (audio, 9600, 4800) >= 50, "glide ran at block rate instead of elapsed sample time");
}
}

namespace
{
void voiceLimit()
{
    Keyboard keyboard;
    keyboard.synth.configure (VoiceMode::poly, 2);
    for (int note : { 60, 64, 67, 71 })
    {
        keyboard.synth.noteOn (1, note, 0.8f);
        keyboard.render (64);
    }
    check (keyboard.active() <= 2, "poly playing exceeded the voice limit");
    keyboard.synth.configure (VoiceMode::poly, 8);
    for (int note : { 48, 50, 52 })
        keyboard.synth.noteOn (1, note, 0.8f);
    keyboard.render (64);
    check (keyboard.active() >= 4, "raising the voice limit did not free more voices");
}

void monoNotePriority()
{
    Keyboard keyboard;
    keyboard.synth.configure (VoiceMode::mono, 8);
    keyboard.synth.noteOn (1, 60, 0.8f);
    keyboard.render (256);
    keyboard.synth.noteOn (1, 64, 0.8f);
    keyboard.render (256);
    check (keyboard.active() == 1, "mono mode sounded more than one voice");
    check (keyboard.synth.getVoice (0)->getCurrentlyPlayingNote() == 64, "mono mode did not play the newest key");
    keyboard.synth.noteOff (1, 64, 0.0f, true);
    keyboard.render (256);
    check (keyboard.synth.getVoice (0)->getCurrentlyPlayingNote() == 60, "releasing the top key did not return to the held key");
    check (keyboard.synth.getVoice (0)->isKeyDown(), "the returned-to key was not held");
    keyboard.synth.noteOff (1, 60, 0.0f, true);
    keyboard.render (4800);
    check (keyboard.active() == 0, "releasing every key left the mono voice sounding");
}

// Legato keeps the envelope where it is; mono restarts it from silence.
void legatoKeepsEnvelope()
{
    const auto levelAfterSecondNote = [] (VoiceMode mode)
    {
        Keyboard keyboard;
        keyboard.parameters.attackSeconds = 0.5f;
        keyboard.update();
        keyboard.synth.configure (mode, 8);
        keyboard.synth.noteOn (1, 60, 0.8f);
        keyboard.render (12000);
        keyboard.synth.noteOn (1, 67, 0.8f);
        // Skip the 5 ms crossfade that mono uses to avoid a click.
        return keyboard.render (960).getMagnitude (0, 480, 480);
    };
    const float legato = levelAfterSecondNote (VoiceMode::legato);
    const float retrig = levelAfterSecondNote (VoiceMode::mono);
    check (legato > retrig * 3.0f, "legato retriggered the envelope, or mono did not");
}

void msegSinglePointAndRandom()
{
    const auto single = MsegShape::sanitize ({ { 0.2f, 0.7f } });
    check (single.size() == 1, "a one-point shape gained points");
    MsegShape flat { single };
    check (std::abs (flat.evaluateUnipolar (0.1f) - 0.7f) < 1.0e-6f && std::abs (flat.evaluateUnipolar (0.9f) - 0.7f) < 1.0e-6f,
           "a one-point shape was not constant");

    MsegShape shape { MsegShape::defaultPoints() };
    std::array<MsegPoint, kMsegMaxPoints> offsets {};
    for (auto& offset : offsets)
        offset = { 1.0f, 1.0f };
    check (std::abs (shape.evaluateUnipolar (0.3f, offsets, 0.0f) - shape.evaluateUnipolar (0.3f)) < 1.0e-6f,
           "zero random changed the shape");
    for (float phase = 0.0f; phase <= 1.0f; phase += 0.01f)
        check (std::abs (shape.evaluateUnipolar (phase, offsets, 0.4f) - shape.evaluateUnipolar (phase)) <= 0.2f + 0.15f,
               "random moved the shape beyond its range");
}

// Random gives each note, and each loop cycle, its own values.
void msegRandomPerNoteAndCycle()
{
    MsegShape flat { MsegShape::sanitize ({ { 0.5f, 0.5f } }) };
    const auto levelOfNote = [&] (float randomPercent, int seedNotes)
    {
        Keyboard keyboard;
        auto& slot = keyboard.parameters.msegSlots[0];
        slot.destination = static_cast<int> (MsegDestination::level);
        slot.amountPercent = 100.0f;
        slot.randomPercent = randomPercent;
        slot.loop = false;
        slot.shape = &flat;
        keyboard.update();
        std::vector<float> levels;
        for (int n = 0; n < seedNotes; ++n)
        {
            keyboard.synth.noteOn (1, 60, 0.8f);
            const auto audio = keyboard.render (4800);
            levels.push_back (audio.getRMSLevel (0, 2400, 2400));
            keyboard.synth.noteOff (1, 60, 0.0f, false);
            keyboard.render (64);
        }
        return levels;
    };
    const auto steady = levelOfNote (0.0f, 4);
    for (float level : steady)
        check (std::abs (level - steady.front()) < 1.0e-4f, "notes differed with random at zero");
    const auto random = levelOfNote (100.0f, 4);
    const auto [low, high] = std::minmax_element (random.begin(), random.end());
    check (*high - *low > 0.01f, "random did not give notes their own levels");

    Keyboard keyboard;
    MsegShape steadyShape { MsegShape::sanitize ({ { 0.0f, 0.5f }, { 1.0f, 0.5f } }) };
    auto& slot = keyboard.parameters.msegSlots[0];
    slot.destination = static_cast<int> (MsegDestination::level);
    slot.amountPercent = 100.0f;
    slot.randomPercent = 100.0f;
    slot.rateSeconds = 0.05f;
    slot.loop = true;
    slot.shape = &steadyShape;
    keyboard.update();
    keyboard.synth.noteOn (1, 60, 0.8f);
    const auto audio = keyboard.render (2400 * 6);
    std::vector<float> cycles;
    for (int cycle = 1; cycle < 6; ++cycle)
        cycles.push_back (audio.getRMSLevel (0, cycle * 2400 + 600, 1200));
    const auto [quietest, loudest] = std::minmax_element (cycles.begin(), cycles.end());
    check (*loudest - *quietest > 0.01f, "a looping shape repeated the same random values");
}

// Glide covers the set time whatever the interval, then holds the target.
void glideLandsOnTime()
{
    Keyboard keyboard;
    keyboard.parameters.portamentoSeconds = 0.1f;
    keyboard.update();
    keyboard.synth.configure (VoiceMode::legato, 8);
    keyboard.synth.noteOn (1, 48, 0.8f);
    keyboard.render (4800);
    keyboard.synth.noteOn (1, 72, 0.8f);
    auto* voice = static_cast<PlaitsVoice*> (keyboard.synth.getVoice (0));
    keyboard.render (2400);
    const float halfway = voice->getCurrentPitch();
    check (halfway > 58.0f && halfway < 62.0f, "glide was not linear over its set time");
    keyboard.render (2600);
    check (std::abs (voice->getCurrentPitch() - 72.0f) < 0.01f, "glide did not reach the target in its set time");
}
}

bool runPolyphonyTests()
{
    bool passed = true;
    for (const auto& test : { std::pair<const char*, void (*)()> { "slow attack", slowAttack },
                              std::pair<const char*, void (*)()> { "voice reuse", repeatedNotes },
                              std::pair<const char*, void (*)()> { "released note replay", releasedNotesStayReleased },
                              std::pair<const char*, void (*)()> { "voice stealing", stealing },
                              std::pair<const char*, void (*)()> { "overlapping keyboard notes", overlappingKeyboardNotes },
                              std::pair<const char*, void (*)()> { "pedal after release", pedalAfterRelease },
                              std::pair<const char*, void (*)()> { "released effect history", releasedEffectHistory },
                              std::pair<const char*, void (*)()> { "sustain and panic", pedalsAndRelease },
                              std::pair<const char*, void (*)()> { "MIDI timing", midiTiming },
                              std::pair<const char*, void (*)()> { "playback restart", playbackRestart },
                              std::pair<const char*, void (*)()> { "polyphonic glide", glideAcrossVoices },
                              std::pair<const char*, void (*)()> { "voice limit", voiceLimit },
                              std::pair<const char*, void (*)()> { "mono note priority", monoNotePriority },
                              std::pair<const char*, void (*)()> { "legato envelope", legatoKeepsEnvelope },
                              std::pair<const char*, void (*)()> { "glide timing", glideLandsOnTime },
                              std::pair<const char*, void (*)()> { "single-point and random shapes", msegSinglePointAndRandom },
                              std::pair<const char*, void (*)()> { "random per note and cycle", msegRandomPerNoteAndCycle } })
    {
        try { test.second(); std::cout << "PASS: " << test.first << '\n'; }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL: " << test.first << ": " << error.what() << '\n';
            passed = false;
        }
    }
    return passed;
}
}
