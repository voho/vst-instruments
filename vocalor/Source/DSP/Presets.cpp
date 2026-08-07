#include "Presets.h"

#include <algorithm>
#include <array>

namespace vocalor
{
namespace
{
// Output gains are linear, because that is what the engine takes; the
// processor converts to the decibels its own parameter publishes. -6 dB is
// 0.501, -5 is 0.562, -7 is 0.447, -8 is 0.398, -4 is 0.631, -3 is 0.708.
constexpr std::array<FactoryPreset, 12> presets { {
    { "Init Soprano", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.30f, .resonance = 0.64f, .vibrato = 0.38f, .humanize = 0.52f,
        .spread = 0.62f, .tension = 0.36f, .room = 0.24f, .outputGain = 0.501f } },

    { "Intimate Alto", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Uuh, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.46f, .resonance = 0.55f, .vibrato = 0.26f, .humanize = 0.60f,
        .spread = 0.30f, .tension = 0.18f, .room = 0.16f, .outputGain = 0.631f,
        .formantShift = -3.0f, .glide = 0.22f, .legato = true, .roomSize = 0.30f,
        .dynamics = 0.55f } },

    { "Pressed Tenor", {
        .profile = VoiceProfile::Male, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.18f, .resonance = 0.78f, .vibrato = 0.44f, .humanize = 0.48f,
        .spread = 0.22f, .tension = 0.88f, .room = 0.18f, .outputGain = 0.501f,
        .glide = 0.15f, .legato = true, .roomSize = 0.40f } },

    { "Legato Soloist", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.28f, .resonance = 0.68f, .vibrato = 0.46f, .humanize = 0.58f,
        .spread = 0.24f, .tension = 0.48f, .room = 0.26f, .outputGain = 0.562f,
        .glide = 0.38f, .legato = true, .roomSize = 0.45f, .dynamics = 0.82f } },

    { "Breath And Air", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Solo,
        .vowel = Vowel::Uuh, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 1.00f, .resonance = 0.40f, .vibrato = 0.18f, .humanize = 0.60f,
        .spread = 0.40f, .tension = 0.08f, .room = 0.46f, .outputGain = 0.708f,
        .roomSize = 0.66f, .dynamics = 0.30f } },

    { "Warm Bass Choir", {
        .profile = VoiceProfile::Male, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Ooh, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.26f, .resonance = 0.58f, .vibrato = 0.30f, .humanize = 0.70f,
        .spread = 0.78f, .tension = 0.24f, .room = 0.32f, .outputGain = 0.447f,
        .formantShift = -4.0f, .roomSize = 0.58f, .intonation = 0.70f } },

    { "Cathedral Ensemble", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 12,
        .breath = 0.34f, .resonance = 0.70f, .vibrato = 0.42f, .humanize = 0.80f,
        .spread = 0.92f, .tension = 0.40f, .room = 0.74f, .outputGain = 0.398f,
        .roomSize = 0.95f, .intonation = 0.85f } },

    { "Closed Mouth Hum", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Ooh, .chordQuality = ChordQuality::Major, .choirSize = 6,
        .breath = 0.22f, .resonance = 0.60f, .vibrato = 0.28f, .humanize = 0.62f,
        .spread = 0.66f, .tension = 0.22f, .room = 0.34f, .outputGain = 0.631f,
        .roomSize = 0.55f, .dynamics = 0.70f, .nasal = 1.00f } },

    { "Small Voices", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 6,
        .breath = 0.30f, .resonance = 0.62f, .vibrato = 0.30f, .humanize = 0.66f,
        .spread = 0.74f, .tension = 0.30f, .room = 0.30f, .outputGain = 0.501f,
        .vowelX = 0.90f, .vowelY = 0.25f, .vowelMorph = 0.70f, .formantShift = 6.0f,
        .roomSize = 0.42f } },

    { "Vowel Morph Pad", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Choir,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 10,
        .breath = 0.36f, .resonance = 0.66f, .vibrato = 0.40f, .humanize = 0.72f,
        .spread = 0.84f, .tension = 0.34f, .room = 0.48f, .outputGain = 0.447f,
        .vowelX = 0.50f, .vowelY = 0.50f, .vowelMorph = 1.00f, .roomSize = 0.68f,
        .intonation = 0.50f } },

    { "Locked Major Chorale", {
        .profile = VoiceProfile::Male, .mode = PerformanceMode::Chord,
        .vowel = Vowel::Aah, .chordQuality = ChordQuality::Major, .choirSize = 8,
        .breath = 0.24f, .resonance = 0.66f, .vibrato = 0.32f, .humanize = 0.55f,
        .spread = 0.70f, .tension = 0.45f, .room = 0.40f, .outputGain = 0.447f,
        .roomSize = 0.60f, .intonation = 1.00f } },

    { "Airy Minor Pad", {
        .profile = VoiceProfile::Female, .mode = PerformanceMode::Chord,
        .vowel = Vowel::Uuh, .chordQuality = ChordQuality::Minor, .choirSize = 8,
        .breath = 0.62f, .resonance = 0.48f, .vibrato = 0.34f, .humanize = 0.74f,
        .spread = 0.88f, .tension = 0.12f, .room = 0.52f, .outputGain = 0.447f,
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
