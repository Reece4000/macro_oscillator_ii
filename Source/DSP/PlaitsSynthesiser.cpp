#include "PlaitsSynthesiser.h"

#include <algorithm>

namespace macro_osc_ii
{
namespace
{
template <typename Container, typename Predicate>
void eraseIf (Container& container, Predicate predicate)
{
    container.erase (std::remove_if (container.begin(), container.end(), predicate), container.end());
}
}

PlaitsSynthesiser::PlaitsSynthesiser()
{
    // JUCE's default 32-sample subdivision can deliver a close note-off before
    // its note-on has rendered anything. Cached Plaits blocks allow exact MIDI.
    setMinimumRenderingSubdivisionSize (1, true);
    heldNotes.reserve (128);
}

void PlaitsSynthesiser::configure (VoiceMode newMode, int newVoiceLimit)
{
    const juce::ScopedLock guard (lock);
    newVoiceLimit = juce::jlimit (1, juce::jmax (1, getNumVoices()), newVoiceLimit);
    if (newMode != mode)
    {
        mode = newMode;
        heldNotes.clear();
        releaseAll();
    }
    if (newVoiceLimit < voiceLimit)
        for (int i = newVoiceLimit; i < getNumVoices(); ++i)
            if (auto* voice = getVoice (i); voice->isVoiceActive())
            {
                voice->setKeyDown (false);
                stopVoice (voice, 0.0f, true);
            }
    voiceLimit = newVoiceLimit;
}

void PlaitsSynthesiser::releaseAll()
{
    for (auto* voice : voices)
        if (voice->isVoiceActive())
        {
            voice->setKeyDown (false);
            stopVoice (voice, 0.0f, true);
        }
}

void PlaitsSynthesiser::noteOn (int channel, int note, float velocity)
{
    const juce::ScopedLock guard (lock);
    if (channel < 1 || channel > 16) return;
    if (mode == VoiceMode::poly)
    {
        juce::Synthesiser::noteOn (channel, note, velocity);
    }
    else
    {
        // Last-note priority: the newest key sounds; earlier keys wait underneath.
        eraseIf (heldNotes, [&] (const HeldNote& held) { return held.channel == channel && held.note == note; });
        heldNotes.push_back ({ channel, note, velocity });
        playMonoNote (heldNotes.back(), mode == VoiceMode::legato && heldNotes.size() > 1);
    }
    lastPlayedNotes[static_cast<size_t> (channel - 1)] = note;
}

void PlaitsSynthesiser::noteOff (int channel, int note, float velocity, bool allowTailOff)
{
    const juce::ScopedLock guard (lock);
    if (mode == VoiceMode::poly || heldNotes.empty())
    {
        juce::Synthesiser::noteOff (channel, note, velocity, allowTailOff);
        return;
    }
    const bool wasSounding = heldNotes.back().channel == channel && heldNotes.back().note == note;
    eraseIf (heldNotes, [&] (const HeldNote& held) { return held.channel == channel && held.note == note; });
    if (! wasSounding)
        return;
    // Releasing the sounding key returns to the most recent key still held.
    if (! heldNotes.empty())
    {
        playMonoNote (heldNotes.back(), mode == VoiceMode::legato);
        return;
    }
    juce::Synthesiser::noteOff (channel, note, velocity, allowTailOff);
}

void PlaitsSynthesiser::playMonoNote (const HeldNote& held, bool legato)
{
    auto* voice = dynamic_cast<PlaitsVoice*> (getVoice (0));
    auto sound = getSound (0);
    if (voice == nullptr || sound == nullptr)
        return;
    const bool sounding = voice->isVoiceActive();
    // Glide continues from wherever the pitch is now, even mid-glide.
    voice->setGlideStartPitch (sounding ? voice->getCurrentPitch()
                                        : static_cast<float> (lastPlayedNotes[static_cast<size_t> (held.channel - 1)].value_or (held.note)));
    if (legato && sounding)
        voice->prepareLegato();
    else if (sounding)
        voice->captureStealTail();
    startVoice (voice, sound.get(), held.channel, held.note, held.velocity);
}

void PlaitsSynthesiser::allNotesOff (int channel, bool allowTailOff)
{
    const juce::ScopedLock guard (lock);
    juce::Synthesiser::allNotesOff (channel, allowTailOff);
    if (channel <= 0)
    {
        lastPlayedNotes.fill (std::nullopt);
        heldNotes.clear();
    }
    else if (channel <= 16)
    {
        lastPlayedNotes[static_cast<size_t> (channel - 1)].reset();
        eraseIf (heldNotes, [channel] (const HeldNote& held) { return held.channel == channel; });
    }
}

juce::SynthesiserVoice* PlaitsSynthesiser::findFreeVoice (juce::SynthesiserSound* sound,
                                                        int channel, int note,
                                                        bool stealIfNoneAvailable) const
{
    const auto prepareVoice = [&] (juce::SynthesiserVoice* voice)
    {
        if (auto* plaitsVoice = dynamic_cast<PlaitsVoice*> (voice))
            plaitsVoice->setGlideStartPitch (static_cast<float> (
                lastPlayedNotes[static_cast<size_t> (channel - 1)].value_or (note)));
        return voice;
    };
    const int count = juce::jmin (voiceLimit, getNumVoices());
    for (int offset = 0; offset < count; ++offset)
    {
        const int index = (nextVoice + offset) % count;
        auto* voice = getVoice (index);
        if (! voice->isVoiceActive() && voice->canPlaySound (sound))
        {
            nextVoice = (index + 1) % count;
            return prepareVoice (voice);
        }
    }
    if (! stealIfNoneAvailable || count == 0) return nullptr;
    auto* voice = findVoiceToSteal (sound, channel, note);
    if (auto* plaitsVoice = dynamic_cast<PlaitsVoice*> (voice))
        plaitsVoice->captureStealTail();
    return prepareVoice (voice);
}

juce::SynthesiserVoice* PlaitsSynthesiser::findVoiceToSteal (juce::SynthesiserSound* sound, int channel, int note) const
{
    if (voiceLimit >= getNumVoices())
        return juce::Synthesiser::findVoiceToSteal (sound, channel, note);

    // JUCE's heuristics, limited to the voices in use: reuse the oldest note
    // first, and protect the lowest and highest held notes.
    std::vector<juce::SynthesiserVoice*> usable;
    juce::SynthesiserVoice* low = nullptr;
    juce::SynthesiserVoice* top = nullptr;
    for (int i = 0; i < voiceLimit; ++i)
    {
        auto* voice = getVoice (i);
        if (! voice->canPlaySound (sound))
            continue;
        usable.push_back (voice);
        if (! voice->isPlayingButReleased())
        {
            const int playing = voice->getCurrentlyPlayingNote();
            if (low == nullptr || playing < low->getCurrentlyPlayingNote()) low = voice;
            if (top == nullptr || playing > top->getCurrentlyPlayingNote()) top = voice;
        }
    }
    std::sort (usable.begin(), usable.end(), [] (auto* a, auto* b) { return a->wasStartedBefore (*b); });
    if (top == low)
        top = nullptr;
    for (auto* voice : usable)
        if (voice->getCurrentlyPlayingNote() == note) return voice;
    for (auto* voice : usable)
        if (voice != low && voice != top && voice->isPlayingButReleased()) return voice;
    for (auto* voice : usable)
        if (voice != low && voice != top && ! voice->isKeyDown()) return voice;
    for (auto* voice : usable)
        if (voice != low && voice != top) return voice;
    return top != nullptr ? top : low;
}
}
