#pragma once

namespace vocalor
{

/** Number of formants modelled by the vocal tract. */
inline constexpr int kFormantCount = 5;

/** Number of cardinal vowels that anchor the continuous vowel space. */
inline constexpr int kCardinalVowelCount = 5;

/** A position in the normalised vowel space.

    @c x runs from 0 (back vowels such as /u/) to 1 (front vowels such as /i/),
    @c y from 0 (close) to 1 (open). The layout mirrors the usual vowel chart so
    the editor can draw the pad and the engine can resolve formants from the very
    same model.
*/
struct VowelPoint
{
    float x { 0.5f };
    float y { 0.5f };
};

/** Pad position of cardinal vowel @c index (0..kCardinalVowelCount-1). */
[[nodiscard]] VowelPoint cardinalVowelPosition (int index) noexcept;

/** Short display name of cardinal vowel @c index, e.g. "EE". */
[[nodiscard]] const char* cardinalVowelName (int index) noexcept;

/** Pad position that best matches one of the three preset vowels
    (0 = AAH, 1 = OOH, 2 = UUH), used to draw the morph anchor. */
[[nodiscard]] VowelPoint presetVowelPosition (int vowelIndex) noexcept;

/** Writes kFormantCount formant frequencies (Hz) for a continuous vowel-space
    position. Interpolation is inverse-distance weighted over the cardinal
    vowels, so every point in the pad is a smooth blend of real vowel targets. */
void formantsForVowelPoint (bool male, float x, float y, float* outHz) noexcept;

/** Writes kFormantCount formant frequencies (Hz) for a preset vowel index. */
void formantsForPresetVowel (bool male, int vowelIndex, float* outHz) noexcept;

/** Frequency multiplier for a formant shift expressed in semitones. */
[[nodiscard]] float formantShiftRatio (float semitones) noexcept;

/** Portamento time in seconds for the normalised glide parameter. */
[[nodiscard]] float glideTimeSeconds (float glide) noexcept;

/** Delay-length multiplier for the normalised room-size parameter.
    0.5 returns exactly 1.0 so the historical room geometry is the default. */
[[nodiscard]] float roomSizeScale (float size) noexcept;

/** Feed-forward gain that normalises a two-pole formant resonator to unity gain
    at its own centre frequency.

    @c poleRadius is exp(-pi * bandwidth / sampleRate) and @c sinPoleAngle the
    sine of 2*pi * centreHz / sampleRate. Without this normalisation a
    resonator's peak height is proportional to the sample rate and inversely
    proportional to its centre frequency, so both the sample rate and the vowel
    would change the loudness of the instrument.
*/
[[nodiscard]] float formantResonatorGain (float poleRadius, float sinPoleAngle) noexcept;

/** Polarity of formant @c index in a parallel formant bank.

    Adjacent formants have to alternate in sign. Summed with a common sign they
    cancel in the valley between two peaks and dig a notch tens of dB deeper than
    anything an all-pole vocal tract produces.
*/
[[nodiscard]] constexpr float formantPolarity (int index) noexcept
{
    return (index & 1) != 0 ? -1.0f : 1.0f;
}

/** Writes @c count relative peak amplitudes for a parallel formant bank whose
    spectral envelope follows the all-pole cascade the same poles would realise.

    A real vocal tract does not give its formants independent amplitudes: they
    follow from the formant frequencies and bandwidths. Half of the cascade's
    absolute gain is compensated so that moving the vowel or the formant shift
    changes the timbre far more than it changes the loudness, and the weakest
    formants are floored at @c floorGain of the strongest.
*/
void parallelFormantAmplitudes (const float* formantHz, const float* formantBandwidth,
                                int count, float sampleRate, float floorGain,
                                float* outGain) noexcept;

/** Magnitude of the summed two-pole formant bank at @c frequencyHz, in dB.
    This is the transfer function the engine actually realises, so the editor's
    curve is a measurement of the running tract rather than a decoration. */
[[nodiscard]] float formantResponseDb (float frequencyHz, const float* formantHz,
                                       const float* formantBandwidth,
                                       const float* formantGain, int count,
                                       float sampleRate) noexcept;

/** Maps a frequency onto 0..1 across a logarithmic axis. */
[[nodiscard]] float normalisedLogFrequency (float hz, float minHz, float maxHz) noexcept;

/** Inverse of normalisedLogFrequency(). */
[[nodiscard]] float logFrequencyForNormalised (float position, float minHz, float maxHz) noexcept;

/** Converts a linear amplitude to dB with a hard floor. */
[[nodiscard]] float linearToDecibels (float linear, float floorDb) noexcept;

/** Maps a decibel value onto 0..1 between a floor and a ceiling. */
[[nodiscard]] float decibelsToMeterPosition (float decibels, float floorDb, float ceilingDb) noexcept;

/** One-pole coefficient for a time constant sampled at a given interval. */
[[nodiscard]] float smoothingCoefficient (float timeConstantSeconds,
                                          float updateIntervalSeconds) noexcept;

/** Meter ballistics: instant-ish attack, slow release, no allocation. */
[[nodiscard]] float meterFollow (float current, float target, float attack, float release) noexcept;

} // namespace vocalor
