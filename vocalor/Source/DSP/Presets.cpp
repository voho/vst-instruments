#include "Presets.h"

#include <algorithm>
#include <array>

namespace vocalor
{
namespace
{
// Output gains are linear, because that is what the engine takes; the
// processor converts to the decibels its own parameter publishes.
//
// Every gain here was re-trimmed when the dynamic control grew from 18.1 dB to
// the 30.0 dB a singer covers and the source slope started following the note's
// own loudness. Both changes lower a preset that is not sung at full velocity
// and full dynamic, so the trims restore each preset to the level it shipped at
// before them, measured as 1 s of stereo RMS from t = 0.6 s on notes 57 and 64
// at velocity 0.85. Breath And Air is the one preset the trim alone cannot
// reach -- it wanted 2.23 against a published maximum of 2.0 -- so it takes the
// whole of that 2.0 and the rest from its own Dynamics, which moves from 0.30
// to 0.34, and still renders 0.64 dB under the level it was voiced at. It has
// no headroom left: anything that lowers a breathy patch again has nowhere to
// put the difference.
constexpr std::array<FactoryPreset, 12> presets { {
    { "Init Soprano", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.30f, .resonance = 0.64f, .vibrato = 0.38f, .humanize = 0.52f,
        .spread = 0.62f, .tension = 0.36f, .room = 0.24f, .outputGain = 0.660f } },

    { "Intimate Alto", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Uuh, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.46f, .resonance = 0.55f, .vibrato = 0.26f, .humanize = 0.60f,
        .spread = 0.30f, .tension = 0.18f, .room = 0.16f, .outputGain = 1.502f,
        .formantShift = -3.0f, .glide = 0.22f, .legato = true, .roomSize = 0.30f,
        .dynamics = 0.55f } },

    { "Pressed Tenor", {
        .profile = VoiceProfile::Male, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.18f, .resonance = 0.78f, .vibrato = 0.44f, .humanize = 0.48f,
        .spread = 0.22f, .tension = 0.88f, .room = 0.18f, .outputGain = 0.615f,
        .glide = 0.15f, .legato = true, .roomSize = 0.40f } },

    { "Legato Soloist", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.28f, .resonance = 0.68f, .vibrato = 0.46f, .humanize = 0.58f,
        // Re-trimmed by 1.93 dB when the presence shelf stopped applying the
        // note's broadband gain twice. This is the one preset in the bank the
        // correction moved past a decibel: a soloist on an open vowel with
        // little breath is almost all voiced source, and at its Dynamics 0.82
        // the shelf plateau rose from -14.1 dB to -7.1.
        .spread = 0.24f, .tension = 0.48f, .room = 0.26f, .outputGain = 1.178f,
        .glide = 0.38f, .legato = true, .roomSize = 0.45f, .dynamics = 0.82f } },

    { "Breath And Air", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Uuh, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 1.00f, .resonance = 0.40f, .vibrato = 0.18f, .humanize = 0.60f,
        .spread = 0.40f, .tension = 0.08f, .room = 0.46f, .outputGain = 2.000f,
        .roomSize = 0.66f, .dynamics = 0.34f } },

    { "Warm Bass Choir", {
        .profile = VoiceProfile::Male, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Ooh, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.26f, .resonance = 0.58f, .vibrato = 0.30f, .humanize = 0.70f,
        .spread = 0.78f, .tension = 0.24f, .room = 0.32f, .outputGain = 0.460f,
        .formantShift = -4.0f, .roomSize = 0.58f, .intonation = 0.70f } },

    { "Cathedral Ensemble", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 12,
        .breath = 0.34f, .resonance = 0.70f, .vibrato = 0.42f, .humanize = 0.80f,
        // Twelve singers on a wide vibrato in a large room sum less coherently
        // than twelve on a narrow one, so the extent this preset asks for now
        // costs it 1.19 dB of ensemble buildup. Re-trimmed by exactly that, so
        // the preset still plays at the level it was voiced at.
        .spread = 0.92f, .tension = 0.40f, .room = 0.74f, .outputGain = 0.563f,
        .roomSize = 0.95f, .intonation = 0.85f } },

    { "Closed Mouth Hum", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Ooh, .chordQuality = ChordQuality::Major, .choirSize = 6,
        .breath = 0.22f, .resonance = 0.60f, .vibrato = 0.28f, .humanize = 0.62f,
        .spread = 0.66f, .tension = 0.22f, .room = 0.34f, .outputGain = 1.120f,
        .roomSize = 0.55f, .dynamics = 0.70f, .nasal = 1.00f } },

    { "Small Voices", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 6,
        .breath = 0.30f, .resonance = 0.62f, .vibrato = 0.30f, .humanize = 0.66f,
        .spread = 0.74f, .tension = 0.30f, .room = 0.30f, .outputGain = 0.672f,
        .vowelX = 0.90f, .vowelY = 0.25f, .vowelMorph = 0.70f, .formantShift = 6.0f,
        .roomSize = 0.42f } },

    { "Vowel Morph Pad", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 10,
        .breath = 0.36f, .resonance = 0.66f, .vibrato = 0.40f, .humanize = 0.72f,
        .spread = 0.84f, .tension = 0.34f, .room = 0.48f, .outputGain = 0.432f,
        .vowelX = 0.50f, .vowelY = 0.50f, .vowelMorph = 1.00f, .roomSize = 0.68f,
        .intonation = 0.50f } },

    { "Locked Major Chorale", {
        .profile = VoiceProfile::Male, .mode = PerformanceMode::Chord,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.24f, .resonance = 0.66f, .vibrato = 0.32f, .humanize = 0.55f,
        .spread = 0.70f, .tension = 0.45f, .room = 0.40f, .outputGain = 0.550f,
        .roomSize = 0.60f, .intonation = 1.00f } },

    { "Airy Minor Pad", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Chord,
        .vowel = Vowel::Uuh, .chordQuality = ChordQuality::Minor, .choirSize = 8,
        .breath = 0.62f, .resonance = 0.48f, .vibrato = 0.34f, .humanize = 0.74f,
        .spread = 0.88f, .tension = 0.12f, .room = 0.52f, .outputGain = 1.461f,
        .vowelX = 0.18f, .vowelY = 0.40f, .vowelMorph = 0.55f, .roomSize = 0.72f,
        .dynamics = 0.48f, .intonation = 1.00f } }
} };
} // namespace

int factoryPresetCount() noexcept
{
    return static_cast<int> (presets.size());
}

const FactoryPreset& factoryPreset (int index) noexcept
{
    return presets[static_cast<std::size_t> (
        std::clamp (index, 0, static_cast<int> (presets.size()) - 1))];
}

const char* factoryPresetName (int index) noexcept
{
    return factoryPreset (index).name;
}

} // namespace vocalor
