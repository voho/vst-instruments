#include "YouKnow106Presets.h"

#include <cstring>

namespace youknow106::presets
{
namespace
{
using sysex::Patch;

// A patch is written here the way it would be dialled in: the fields the entry
// does not name keep the struct's defaults, which are the panel's own rest
// positions. Only the sixteen continuous controls and the switches the patch
// memory holds appear -- volume, the bender depths, portamento and the assign
// mode are performance controls and are not part of a stored patch on this
// instrument, so they are not part of one here either.
constexpr std::array<Preset, presetCount> bank {{
    // --- Bank A, group 1: the sounds the architecture is known for ---------
    { "A11", "Hollow Strings", Patch {
        /* lfoRate */ 0.36f, /* lfoDelay */ 0.30f, /* dcoLfo */ 0.08f,
        /* pwm */ 0.62f, /* noise */ 0.0f, /* cutoff */ 0.44f,
        /* resonance */ 0.12f, /* vcfEnv */ 0.22f, /* vcfLfo */ 0.05f,
        /* keyFollow */ 0.55f, /* vcaLevel */ 0.78f, /* attack */ 0.30f,
        /* decay */ 0.55f, /* sustain */ 0.82f, /* release */ 0.45f,
        /* sub */ 0.18f,
        DcoRange::Eight, false, true, PwmSource::Lfo, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::One } },
    { "A12", "Wide Pad", Patch {
        0.22f, 0.42f, 0.06f, 0.70f, 0.0f, 0.36f, 0.18f, 0.26f, 0.10f,
        0.60f, 0.76f, 0.46f, 0.62f, 0.88f, 0.62f, 0.24f,
        DcoRange::Eight, true, true, PwmSource::Lfo, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::OneTwo } },
    { "A13", "Bright Brass", Patch {
        0.40f, 0.20f, 0.0f, 0.35f, 0.0f, 0.34f, 0.28f, 0.62f, 0.0f,
        0.45f, 0.82f, 0.10f, 0.40f, 0.62f, 0.22f, 0.0f,
        DcoRange::Eight, true, true, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "A14", "Sub Bass", Patch {
        0.30f, 0.0f, 0.0f, 0.28f, 0.0f, 0.24f, 0.20f, 0.40f, 0.0f,
        0.30f, 0.85f, 0.0f, 0.32f, 0.30f, 0.12f, 0.72f,
        DcoRange::Sixteen, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "A15", "Pluck Keys", Patch {
        0.42f, 0.0f, 0.0f, 0.30f, 0.0f, 0.40f, 0.34f, 0.48f, 0.0f,
        0.70f, 0.80f, 0.0f, 0.26f, 0.0f, 0.20f, 0.10f,
        DcoRange::Eight, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Two, ChorusMode::One } },
    { "A16", "Glass Bells", Patch {
        0.55f, 0.0f, 0.04f, 0.50f, 0.0f, 0.58f, 0.42f, 0.30f, 0.0f,
        0.85f, 0.72f, 0.0f, 0.44f, 0.0f, 0.48f, 0.0f,
        DcoRange::Four, false, true, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Two, ChorusMode::Two } },
    { "A17", "Reed Organ", Patch {
        0.34f, 0.24f, 0.05f, 0.48f, 0.0f, 0.52f, 0.08f, 0.0f, 0.0f,
        0.50f, 0.80f, 0.02f, 0.20f, 1.0f, 0.10f, 0.30f,
        DcoRange::Eight, true, true, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::One } },
    { "A18", "Noise Sweep", Patch {
        0.18f, 0.0f, 0.0f, 0.30f, 0.85f, 0.20f, 0.55f, 0.85f, 0.0f,
        0.0f, 0.70f, 0.55f, 0.70f, 0.30f, 0.60f, 0.0f,
        DcoRange::Eight, false, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Boost, ChorusMode::Two } },

    // --- Bank A, group 2: filter and modulation studies -------------------
    { "A21", "Resonant Swell", Patch {
        0.20f, 0.35f, 0.0f, 0.40f, 0.0f, 0.30f, 0.72f, 0.70f, 0.10f,
        0.40f, 0.74f, 0.62f, 0.60f, 0.70f, 0.58f, 0.0f,
        DcoRange::Eight, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::One } },
    { "A22", "Self Oscillation", Patch {
        0.30f, 0.0f, 0.0f, 0.30f, 0.0f, 0.46f, 0.96f, 0.20f, 0.0f,
        0.90f, 0.68f, 0.10f, 0.40f, 0.60f, 0.30f, 0.0f,
        DcoRange::Eight, false, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Three, ChorusMode::Off } },
    { "A23", "Inverted Env", Patch {
        0.32f, 0.0f, 0.0f, 0.34f, 0.0f, 0.70f, 0.38f, 0.55f, 0.0f,
        0.50f, 0.78f, 0.0f, 0.36f, 0.20f, 0.26f, 0.0f,
        DcoRange::Eight, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Inverted, HighPassMode::One, ChorusMode::Off } },
    { "A24", "Gate Stab", Patch {
        0.30f, 0.0f, 0.0f, 0.44f, 0.0f, 0.42f, 0.30f, 0.0f, 0.0f,
        0.50f, 0.76f, 0.0f, 0.30f, 1.0f, 0.10f, 0.20f,
        DcoRange::Eight, true, true, PwmSource::Manual, VcaMode::Gate,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "A25", "Slow Vibrato", Patch {
        0.14f, 0.55f, 0.28f, 0.40f, 0.0f, 0.48f, 0.16f, 0.20f, 0.0f,
        0.50f, 0.78f, 0.22f, 0.50f, 0.85f, 0.40f, 0.0f,
        DcoRange::Eight, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "A26", "PWM Motion", Patch {
        0.26f, 0.0f, 0.0f, 0.88f, 0.0f, 0.50f, 0.14f, 0.15f, 0.0f,
        0.50f, 0.78f, 0.28f, 0.55f, 0.86f, 0.42f, 0.0f,
        DcoRange::Eight, false, true, PwmSource::Lfo, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "A27", "Filter Wobble", Patch {
        0.62f, 0.0f, 0.0f, 0.35f, 0.0f, 0.34f, 0.52f, 0.10f, 0.72f,
        0.40f, 0.76f, 0.02f, 0.40f, 0.90f, 0.24f, 0.15f,
        DcoRange::Eight, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "A28", "Key Follow Test", Patch {
        0.30f, 0.0f, 0.0f, 0.30f, 0.0f, 0.18f, 0.35f, 0.10f, 0.0f,
        1.0f, 0.80f, 0.0f, 0.35f, 0.75f, 0.20f, 0.0f,
        DcoRange::Eight, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },

    // --- Bank B, group 1: ensemble and texture ----------------------------
    { "B11", "Choir Pad", Patch {
        0.20f, 0.48f, 0.05f, 0.76f, 0.02f, 0.32f, 0.22f, 0.24f, 0.08f,
        0.58f, 0.74f, 0.55f, 0.65f, 0.90f, 0.68f, 0.20f,
        DcoRange::Eight, false, true, PwmSource::Lfo, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::OneTwo } },
    { "B12", "Octave Strings", Patch {
        0.34f, 0.28f, 0.07f, 0.58f, 0.0f, 0.46f, 0.14f, 0.20f, 0.04f,
        0.62f, 0.78f, 0.26f, 0.58f, 0.84f, 0.50f, 0.55f,
        DcoRange::Eight, true, true, PwmSource::Lfo, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::One } },
    { "B13", "Soft Flute", Patch {
        0.28f, 0.36f, 0.06f, 0.30f, 0.06f, 0.38f, 0.10f, 0.16f, 0.0f,
        0.66f, 0.72f, 0.24f, 0.40f, 0.86f, 0.30f, 0.0f,
        DcoRange::Four, false, true, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Two, ChorusMode::One } },
    { "B14", "Deep Brass", Patch {
        0.36f, 0.16f, 0.0f, 0.40f, 0.0f, 0.28f, 0.34f, 0.66f, 0.0f,
        0.48f, 0.84f, 0.14f, 0.46f, 0.58f, 0.30f, 0.40f,
        DcoRange::Sixteen, true, true, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Two } },
    { "B15", "Clav Bite", Patch {
        0.40f, 0.0f, 0.0f, 0.22f, 0.0f, 0.46f, 0.44f, 0.42f, 0.0f,
        0.72f, 0.80f, 0.0f, 0.18f, 0.0f, 0.14f, 0.0f,
        DcoRange::Eight, false, true, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Three, ChorusMode::Off } },
    { "B16", "Marimba", Patch {
        0.30f, 0.0f, 0.0f, 0.30f, 0.0f, 0.54f, 0.24f, 0.36f, 0.0f,
        0.80f, 0.76f, 0.0f, 0.22f, 0.0f, 0.18f, 0.24f,
        DcoRange::Four, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Two, ChorusMode::Off } },
    { "B17", "Air Sweep", Patch {
        0.10f, 0.0f, 0.0f, 0.30f, 0.62f, 0.26f, 0.46f, 0.78f, 0.20f,
        0.0f, 0.68f, 0.70f, 0.78f, 0.40f, 0.72f, 0.0f,
        DcoRange::Eight, false, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Boost, ChorusMode::OneTwo } },
    { "B18", "Sync Lead", Patch {
        0.44f, 0.12f, 0.03f, 0.24f, 0.0f, 0.62f, 0.50f, 0.34f, 0.0f,
        0.52f, 0.86f, 0.02f, 0.38f, 0.72f, 0.24f, 0.0f,
        DcoRange::Eight, false, true, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Two, ChorusMode::Off } },

    // --- Bank B, group 2: bass and percussion -----------------------------
    { "B21", "Round Bass", Patch {
        0.30f, 0.0f, 0.0f, 0.30f, 0.0f, 0.22f, 0.26f, 0.44f, 0.0f,
        0.20f, 0.84f, 0.0f, 0.30f, 0.24f, 0.14f, 0.50f,
        DcoRange::Sixteen, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "B22", "Acid Bass", Patch {
        0.30f, 0.0f, 0.0f, 0.30f, 0.0f, 0.18f, 0.86f, 0.62f, 0.0f,
        0.30f, 0.82f, 0.0f, 0.24f, 0.10f, 0.12f, 0.0f,
        DcoRange::Sixteen, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "B23", "Pulse Bass", Patch {
        0.30f, 0.0f, 0.0f, 0.72f, 0.0f, 0.26f, 0.30f, 0.38f, 0.0f,
        0.25f, 0.84f, 0.0f, 0.34f, 0.30f, 0.16f, 0.30f,
        DcoRange::Sixteen, false, true, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "B24", "Tom", Patch {
        0.30f, 0.0f, 0.0f, 0.30f, 0.10f, 0.16f, 0.62f, 0.70f, 0.0f,
        0.0f, 0.80f, 0.0f, 0.26f, 0.0f, 0.22f, 0.0f,
        DcoRange::Sixteen, false, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::One, ChorusMode::Off } },
    { "B25", "Snare", Patch {
        0.30f, 0.0f, 0.0f, 0.30f, 1.0f, 0.44f, 0.40f, 0.30f, 0.0f,
        0.0f, 0.78f, 0.0f, 0.16f, 0.0f, 0.14f, 0.0f,
        DcoRange::Eight, false, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Three, ChorusMode::Off } },
    { "B26", "Wind", Patch {
        0.08f, 0.0f, 0.0f, 0.30f, 1.0f, 0.30f, 0.70f, 0.0f, 0.55f,
        0.0f, 0.66f, 0.80f, 0.80f, 0.70f, 0.80f, 0.0f,
        DcoRange::Eight, false, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Boost, ChorusMode::Two } },
    { "B27", "Sub Drone", Patch {
        0.06f, 0.0f, 0.02f, 0.40f, 0.0f, 0.20f, 0.44f, 0.0f, 0.12f,
        0.0f, 0.80f, 0.40f, 0.60f, 1.0f, 0.70f, 0.90f,
        DcoRange::Sixteen, true, false, PwmSource::Manual, VcaMode::Envelope,
        EnvPolarity::Normal, HighPassMode::Boost, ChorusMode::OneTwo } },
    { "B28", "Init Patch", Patch {} },
}};
} // namespace

const std::array<Preset, presetCount>& factoryBank() noexcept
{
    return bank;
}

const Preset* findByNumber(const char* number) noexcept
{
    if (number == nullptr)
        return nullptr;
    for (const auto& preset : bank)
        if (std::strcmp(preset.number, number) == 0)
            return &preset;
    return nullptr;
}

} // namespace youknow106::presets
