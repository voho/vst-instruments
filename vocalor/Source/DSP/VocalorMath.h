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

/** How a continuous dynamic level reaches the voice.

    A dynamic is not a fader. A singer at pianissimo is quieter, but is also
    duller (less vocal effort, so a laxer glottal pulse and a steeper source
    spectrum), proportionally breathier (the voiced source falls away faster
    than the leak noise), and vibrates less. These are the four scalars the
    engine applies; every one of them is exactly 1 at full dynamic, so the
    control is inert at its default.
*/
struct DynamicResponse
{
    float voicedGain { 1.0f };
    float airGain { 1.0f };
    float effortScale { 1.0f };
    float sourceTensionScale { 1.0f };
    float vibratoScale { 1.0f };
};

/** Resolves the dynamic response for a normalised dynamic level 0..1. */
[[nodiscard]] DynamicResponse dynamicResponse (float dynamics) noexcept;

/** Extent the Vibrato knob reaches at the top of its travel, in cents.
    Sundberg's definition of a sung vibrato is 5-7 Hz at an extent "of about
    +/-1 semitone". */
inline constexpr float kVibratoReachCents = 100.0f;

/** How wide an extent an ensemble singer settles at, in cents. The 2022
    systematic review records that "vibrato extent tended to be higher in solo
    singing compared to group singing", and twelve voices at solo extent smear
    into a chorus rather than reading as a section. */
inline constexpr float kSectionVibratoCents = 40.0f;

/** Vibrato extent in cents for a normalised knob position.

    @c sectionLimitCents is the extent the singer's own section tolerates, or 0
    for a soloist, whom nothing limits.
*/
[[nodiscard]] float vibratoExtentCents (float vibrato, float sectionLimitCents) noexcept;

/** First formant after the singer's own resonance strategy.

    A speech tract keeps F1 where the vowel puts it. A singer cannot: once the
    fundamental climbs past F1 the whole spectrum sits above the lowest
    resonance, the radiated power collapses and the timbre breaks. The measured
    response is to open the jaw and take F1 up with the pitch, which puts a
    resonance back on the fundamental at the cost of the vowel's identity --
    which is why sopranos are hard to understand at the top of the range.

    @c baseHz is the vowel's own F1, @c fundamentalHz the note being sung, and
    @c ceilingHz the highest F1 that jaw actually reaches. The strategy engages
    as the fundamental comes up on F1 and is complete a little above it; below
    that the vowel is returned unchanged.
*/
[[nodiscard]] float tunedFirstFormant (float baseHz, float fundamentalHz,
                                       float ceilingHz) noexcept;

/** Smallest F2/F1 ratio any real vowel presents. A tracked F1 that climbs into
    F2 would be a tract with two coincident lowest resonances, which is not a
    configuration a mouth has; F2 is pushed clear instead. */
inline constexpr float kMinimumFormantSpacing = 1.40f;

/** Cents by which the just interval departs from its equal-tempered neighbour.

    An a cappella ensemble does not sing equal temperament. It narrows the major
    third and widens the minor one until the overtones align, which is the
    "ring" of a locked chord and the thing a keyboard-tuned choir patch cannot
    produce. The table is five-limit: 5:4 for the major third (13.7 cents
    narrow), 6:5 for the minor (15.6 wide), 3:2 for the fifth (2.0 wide).

    @c semitonesAboveRoot may be any integer, positive or negative; only its
    pitch class matters, because an octave is just either way.
*/
[[nodiscard]] float justIntonationOffsetCents (int semitonesAboveRoot) noexcept;

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

/** Resolves the cascade-derived parallel gains and the two-pole coefficients
    from the same geometry in one pass. The coefficient outputs may be null.
    @c outPeakNormaliser is the numerator which makes each isolated resonator
    unity at its own centre frequency; multiply it by the signed formant gain
    to obtain b0. */
void parallelFormantCoefficients (const float* formantHz,
                                  const float* formantBandwidth,
                                  int count, float sampleRate, float floorGain,
                                  float* outGain, float* outPoleA1, float* outPoleA2,
                                  float* outPeakNormaliser) noexcept;

/** Magnitude of the summed two-pole formant bank at @c frequencyHz, in dB.
    This is the transfer function the engine actually realises, so the editor's
    curve is a measurement of the running tract rather than a decoration. */
[[nodiscard]] float formantResponseDb (float frequencyHz, const float* formantHz,
                                       const float* formantBandwidth,
                                       const float* formantGain, int count,
                                       float sampleRate) noexcept;

/** Precomputes the per-formant pole/gain terms formantResponseDb() resolves
    from @c formantHz, @c formantBandwidth, @c formantGain and @c sampleRate --
    everything that does not depend on the probe frequency. A response curve
    samples the same fixed formant bank at many frequencies (the editor's
    vocal-tract display walks it once per pixel), and those formant terms do
    not change between samples, so a caller that plots more than one point can
    resolve them here once and evaluate formantResponseDbFromCoefficients() at
    each frequency instead of paying formantResponseDb()'s trig/exp/sqrt work
    again per point. outA1/outA2/outScale must each hold at least @c count
    entries. */
void formantResponseCoefficients (const float* formantHz, const float* formantBandwidth,
                                  const float* formantGain, int count, float sampleRate,
                                  float* outA1, float* outA2, float* outScale) noexcept;

/** Magnitude in dB of the formant bank described by @c a1/@c a2/@c scale
    from formantResponseCoefficients(), evaluated at @c frequencyHz. Bit-
    identical to formantResponseDb() called with the same formant bank, for
    the frequency-dependent half of the same arithmetic. */
[[nodiscard]] float formantResponseDbFromCoefficients (float frequencyHz, const float* a1,
                                                        const float* a2, const float* scale,
                                                        int count, float sampleRate) noexcept;

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
