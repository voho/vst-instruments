#pragma once

#include "YouKnow106Chorus.h"
#include "YouKnow106Engine.h"

#include <cstddef>
#include <cstdint>

namespace youknow106::sysex
{

// System-exclusive compatibility with the modelled instrument.
//
// The reference synth stores a patch as eighteen bytes: sixteen continuous
// controls at 0..127, then two bytes of packed switches. That is the whole
// patch -- volume, the bender depths, portamento and the assign mode are
// performance controls and are deliberately not in it, exactly as on the
// hardware, which is why loading a patch does not move them here either.
//
// Everything in this file is JUCE-free so the regression suite can drive the
// byte layout directly rather than through a plug-in.

inline constexpr std::uint8_t manufacturerId = 0x41;   // Roland
inline constexpr std::uint8_t patchDataOpcode = 0x30;  // all parameters
inline constexpr std::uint8_t parameterOpcode = 0x32;  // one parameter

inline constexpr int toneByteCount = 18;
inline constexpr int patchMessageBytes = 5 + toneByteCount;  // F0 41 30 0n .. F7
inline constexpr int parameterMessageBytes = 7;              // F0 41 32 0n p v F7

// The continuous controls, in the order the message carries them. The order is
// the instrument's, not this project's, and the suite asserts each index.
enum class ToneParameter
{
    LfoRate = 0, LfoDelay, DcoLfo, DcoPwm, DcoNoise,
    VcfFreq, VcfRes, VcfEnv, VcfLfo, VcfKybd,
    VcaLevel, EnvAttack, EnvDecay, EnvSustain, EnvRelease,
    DcoSub,
    SwitchesOne,   // byte 16: range, waveforms, chorus
    SwitchesTwo    // byte 17: PWM source, VCA mode, VCF polarity, HPF
};

// One stored patch, in this engine's own units: every continuous control as
// panel travel in 0..1, every switch as its panel position.
struct Patch
{
    float lfoRate { 0.42f };
    float lfoDelay { 0.0f };
    float dcoLfo { 0.0f };
    float pwm { 0.30f };
    float noise { 0.0f };
    float cutoff { 0.62f };
    float resonance { 0.10f };
    float vcfEnv { 0.35f };
    float vcfLfo { 0.0f };
    float keyFollow { 0.50f };
    float vcaLevel { 0.80f };
    float attack { 0.0f };
    float decay { 0.45f };
    float sustain { 0.70f };
    float release { 0.30f };
    float sub { 0.0f };

    DcoRange range { DcoRange::Eight };
    bool saw { true };
    bool pulse { false };
    PwmSource pwmSource { PwmSource::Manual };
    VcaMode vcaMode { VcaMode::Envelope };
    EnvPolarity envPolarity { EnvPolarity::Normal };
    HighPassMode highPass { HighPassMode::One };
    ChorusMode chorus { ChorusMode::Off };
};

// --- Tone bytes ----------------------------------------------------------

// Reads the eighteen tone bytes into a patch. Every byte is accepted: the
// instrument's own encoding leaves combinations the panel cannot produce, and
// refusing them would mean refusing dumps real hardware emits.
[[nodiscard]] Patch patchFromToneBytes(const std::uint8_t* bytes) noexcept;

// Writes a patch back out as eighteen tone bytes.
void toneBytesFromPatch(const Patch& patch, std::uint8_t* bytes) noexcept;

// Whether the patch has an exact state in the hardware's eighteen tone bytes.
//
// Continuous panel travel is effective at the hardware's own 7-bit resolution:
// for example 0.36 and 46/127 name the same stored control step, so that normal
// quantisation is not a loss. The current panel exposes exactly the memory's
// Off/I/II chorus states, but one obsolete compatibility value does not have an
// effective equivalent: early YouKnow106 sessions could store both chorus
// buttons on. `toneBytesFromPatch` canonicalises that value to II and this
// predicate reports the categorical conversion.
[[nodiscard]] bool survivesPatchMemory(const Patch& patch) noexcept;

// --- Messages ------------------------------------------------------------

// Recognises `F0 41 30 0n <18 bytes> F7`. Returns false and leaves both
// outputs untouched for anything else, including messages from other makers,
// other opcodes and short or overlong bodies.
[[nodiscard]] bool readPatchMessage(const std::uint8_t* message, std::size_t length,
                                    Patch& patch, int& channel) noexcept;

// Writes `F0 41 30 0n <18 bytes> F7` into `out`, which must have room for
// `patchMessageBytes`. Returns the number of bytes written, or 0.
[[nodiscard]] std::size_t writePatchMessage(const Patch& patch, int channel,
                                            std::uint8_t* out,
                                            std::size_t capacity) noexcept;

// Recognises `F0 41 32 0n <parameter> <value> F7`, the message the hardware
// sends when a single control is moved.
[[nodiscard]] bool readParameterMessage(const std::uint8_t* message,
                                        std::size_t length, int& parameter,
                                        int& value, int& channel) noexcept;

[[nodiscard]] std::size_t writeParameterMessage(int parameter, int value,
                                                int channel, std::uint8_t* out,
                                                std::size_t capacity) noexcept;

// Applies one parameter-change message to a patch. Returns false when the
// parameter number is outside the instrument's range.
[[nodiscard]] bool applyParameter(Patch& patch, int parameter, int value) noexcept;

// The 0..127 value a parameter currently holds in a patch, or -1 when the
// number is out of range.
[[nodiscard]] int parameterValue(const Patch& patch, int parameter) noexcept;

} // namespace youknow106::sysex
