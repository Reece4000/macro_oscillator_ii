#pragma once

#include "PlaitsVoice.h"

#include <vector>

namespace macro_osc_ii
{
enum class VoiceMode
{
    poly,
    mono,   // one voice, every note retriggers
    legato  // one voice, overlapping notes only move the pitch
};

class PlaitsSynthesiser final : public juce::Synthesiser
{
public:
    PlaitsSynthesiser();

    // Called before each render. Changing mode releases sounding notes; lowering
    // the voice limit releases voices above it.
    void configure (VoiceMode newMode, int newVoiceLimit);

    void noteOn (int channel, int note, float velocity) override;
    void noteOff (int channel, int note, float velocity, bool allowTailOff) override;
    void allNotesOff (int channel, bool allowTailOff) override;

protected:
    juce::SynthesiserVoice* findFreeVoice (juce::SynthesiserSound*, int channel,
                                          int note, bool stealIfNoneAvailable) const override;
    juce::SynthesiserVoice* findVoiceToSteal (juce::SynthesiserSound*, int channel, int note) const override;

private:
    struct HeldNote
    {
        int channel {};
        int note {};
        float velocity {};
    };

    mutable int nextVoice {};
    std::array<std::optional<int>, 16> lastPlayedNotes;
    VoiceMode mode { VoiceMode::poly };
    int voiceLimit { 128 };
    std::vector<HeldNote> heldNotes;

    void playMonoNote (const HeldNote& held, bool legato);
    void releaseAll();
};
}
