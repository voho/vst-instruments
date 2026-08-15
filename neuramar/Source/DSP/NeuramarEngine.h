#pragma once

#include "NeuralModel.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace neuramar
{

struct EngineParameters
{
    float imprint { 1.0f };             // learned harmonic core, 0..1
    float bodyLock { 0.65f };            // fixed formants vs pitch-follow, 0..1
    float air { 0.35f };                 // residual/noise branch, 0..1
    float bone { 0.30f };                // inharmonic modal branch, 0..1
    float brightness { 0.50f };          // spectral tilt, 0..1
    float evolutionRate { 1.0f };        // learned-time rate, 0.125..4
    float orbit { 0.15f };               // one-shot to learned-loop blend, 0..1
    float mutation { 0.10f };            // deterministic per-voice variation, 0..1
    float noise { 0.0f };                // voice-local model-space drift, 0..1
    // How long the performance fade-in takes, end to end, not an exponential
    // time constant. Zero renders the model's own onset untouched.
    float attackSeconds { 0.0f };         // 0..10 seconds
    float releaseSeconds { 0.35f };       // 0.005..20 seconds
    float spread { 0.35f };               // stereo voice spread, 0..1
    float rootOffsetSemitones { 0.0f };   // manual detected-root correction, -12..12
    float outputGain { 0.72f };           // linear gain, 0..2
    // Scales the model's fitted stiff-string coefficient. 1 renders the
    // learned partial stretch, 0 forces an ideal harmonic series, and 2
    // exaggerates it. Legacy memories carry no coefficient, so this control
    // is inaudible for them regardless of its value.
    float stretch { 1.0f };               // 0..2
    // Moves the resonant body in frequency without moving the played pitch.
    float formantShiftSemitones { 0.0f }; // -24..24
    // Velocity-to-timbre depth. Zero preserves the pure amplitude response
    // that every earlier Neuramar build had.
    float touch { 0.0f };                 // 0..1
    // Key tracking. Positive darkens notes played above the learned root and
    // opens up notes played below it, the way a real instrument's spectrum
    // changes across its compass. It changes tone, not level.
    float registerTilt { 0.0f };          // -1..1
};

class NeuramarEngine final
{
public:
    // Sixteen voices. The per-voice cost is unchanged, so an eight-note
    // performance renders for exactly what it always did; what the higher
    // ceiling buys is that a four-note chord under a sustain pedal, or a pad
    // played over its own release tails, stops stealing.
    static constexpr int maximumVoices = 16;

    NeuramarEngine() noexcept;

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // The engine never owns or destroys the model. The caller must keep the
    // pointed-to immutable model alive until all process calls that could have
    // observed it have completed.
    void setModel(const NeuralModel* immutableModel) noexcept;
    void setParameters(const EngineParameters& parameters) noexcept;

    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void allNotesOff() noexcept;
    void allSoundOff() noexcept;
    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] int getActiveVoiceCount() const noexcept;

    // The exponent p in tau(f) = tau_1 (f/f_1)^-p, fitted by setModel() from
    // the published model's own per-partial amplitude trajectories and used to
    // damp the release by frequency. Zero means the source decayed at the same
    // rate at every frequency, or that its trajectories carried no usable
    // decay evidence at all, and the release then behaves exactly as it did
    // before this was fitted.
    [[nodiscard]] float dampingExponent() const noexcept
    {
        return dampingExponent_;
    }

    // The loop region's own log-amplitude trend, in nepers per second of model
    // time, which Orbit divides out so a wrap is level-continuous. It is the
    // trend of the mix the renderer is *producing*, so it follows the Air and
    // Bone controls: on a source whose layers decay at different rates, the
    // trend of the Core alone and the trend of all three together are different
    // numbers, and correcting a wrap by the wrong one puts the level step back.
    // Published so the suite can read it directly, because the step it prevents
    // is a fraction of a decibel against a residual several times that.
    [[nodiscard]] float loopLevelSlopePerSecond() const noexcept
    {
        return loopLevelSlopePerSecond_;
    }

private:
    // The neural controller remains a compact 64-partial model. A larger,
    // engine-only bank lets Body Lock resample that observed spectrum onto the
    // denser harmonic grid required by lower notes without changing presets.
    static constexpr std::size_t renderedHarmonicCount = 256;
    static constexpr std::size_t airOutputOffset = renderedHarmonicCount;
    static constexpr std::size_t boneOutputOffset = airOutputOffset
        + NeuralModel::airBandCount;
    static constexpr std::size_t renderAmplitudeCount = boneOutputOffset
        + NeuralModel::boneModeCount;
    // The level a releasing voice retires at, and the level the Dissolve time
    // is the duration to. The frequency-dependent release below is normalised
    // against it, so both have to name the same number.
    static constexpr float retirementLevel = 1.0e-5f;

    struct AtomicParameters
    {
        std::atomic<float> imprint { 1.0f };
        std::atomic<float> bodyLock { 0.65f };
        std::atomic<float> air { 0.35f };
        std::atomic<float> bone { 0.30f };
        std::atomic<float> brightness { 0.50f };
        std::atomic<float> evolutionRate { 1.0f };
        std::atomic<float> orbit { 0.15f };
        std::atomic<float> mutation { 0.10f };
        std::atomic<float> noise { 0.0f };
        std::atomic<float> attackSeconds { 0.0f };
        std::atomic<float> releaseSeconds { 0.35f };
        std::atomic<float> spread { 0.35f };
        std::atomic<float> rootOffsetSemitones { 0.0f };
        std::atomic<float> outputGain { 0.72f };
        std::atomic<float> stretch { 1.0f };
        std::atomic<float> formantShiftSemitones { 0.0f };
        std::atomic<float> touch { 0.0f };
        std::atomic<float> registerTilt { 0.0f };
    };

    // One coefficient set with two independent state pairs. The Air layer needs
    // a decorrelated second realisation for stereo width, and the side copy
    // must track exactly the same ramped response as the centred one.
    struct Bandpass
    {
        float z1 { 0.0f };
        float z2 { 0.0f };
        float sideZ1 { 0.0f };
        float sideZ2 { 0.0f };
        float b0 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float outputScale { 1.0f };
        float b0Step { 0.0f };
        float b2Step { 0.0f };
        float a1Step { 0.0f };
        float a2Step { 0.0f };
        float outputScaleStep { 0.0f };
        float configuredCentreHz { -1.0f };
        float configuredBandwidthOctaves { -1.0f };
        int rampRemaining { 0 };

        void clear() noexcept { z1 = z2 = sideZ1 = sideZ2 = 0.0f; }
        void set(float centreHz, float bandwidthOctaves,
                 float sampleRate, int rampSamples) noexcept;
        // Retires a coefficient ramp without running the filter, leaving it
        // where tick() would have left it. A band whose layer is silent for a
        // whole control period is never ticked, so without this its ramp would
        // stay open forever and defeat the early-out in set().
        void finishRamp() noexcept;
        [[nodiscard]] float tick(float input) noexcept;
        // Uses the coefficients that tick() has already advanced this sample,
        // so it must be called after it and only once per sample.
        [[nodiscard]] float tickSide(float input) noexcept;
    };

    struct Voice
    {
        bool active { false };
        bool releasing { false };
        int midiNote { -1 };
        int controlCountdown { 0 };
        std::uint64_t ageStamp { 0 };
        std::array<std::uint32_t, NeuralModel::airBandCount> airNoiseStates {};
        std::array<std::uint32_t, NeuralModel::airBandCount> airSideNoiseStates {};
        float velocity { 0.0f };
        float velocityGain { 0.0f };
        float envelope { 0.0f };
        // Position along the performance fade-in, 0 at note-on and 1 once the
        // whole Awaken time has elapsed. Kept separate from the envelope so
        // release still decays whatever level the fade-in had reached.
        float attackPosition { 0.0f };
        double modelTimeSeconds { 0.0 };
        std::uint64_t renderedSampleCount { 0 };
        float mutationOffset { 0.0f };
        float latentPhaseA { 0.0f };
        float latentPhaseB { 0.0f };
        float latentRateAHertz { 0.0f };
        float latentRateBHertz { 0.0f };
        float cachedBrightnessTilt { -1000.0f };
        float cachedReferenceTilt { -1000.0f };
        // Covers the whole rendered bank so the control-rate target loop can
        // factor pow(index * scale, tilt) into one cached table lookup and one
        // scalar instead of a pow() per rendered harmonic.
        std::array<float, renderedHarmonicCount> brightnessTiltTable {};
        std::array<float, NeuralModel::harmonicCount> referenceTiltTable {};
        std::array<float, NeuralModel::harmonicCount> harmonicVariationSin {};
        std::array<float, NeuralModel::airBandCount> airVariationSin {};
        float pan { 0.0f };
        float panLeft { 0.70710678f };
        float panRight { 0.70710678f };
        float panLeftStep { 0.0f };
        float panRightStep { 0.0f };
        float airSideGain { 0.0f };
        float airSideGainStep { 0.0f };
        float transpositionRatio { 1.0f };
        // r^p, the rate at which this key replays the learned trajectory, and
        // the per-sample envelope factor that key-tracks Dissolve by the same
        // law. Both are exactly the untracked value at the root note and on
        // any source that fits p = 0.
        float clockRateScale { 1.0f };
        float releaseMultiplier { 1.0f };
        float pitchRatio { 1.0f };
        float pitchRatioStep { 0.0f };
        float lastLeft { 0.0f };
        float lastRight { 0.0f };
        std::size_t activeHarmonicCount { NeuralModel::harmonicCount };
        std::size_t targetHarmonicCount { NeuralModel::harmonicCount };
        // Harmonics at or above this index are silent now and stay silent for
        // the whole coming control period, so the audio loop skips them
        // entirely instead of advancing a phase nothing reads.
        std::size_t soundingHarmonicCount { 0 };
        // The same idea for the two body layers: a layer that is silent now
        // and stays silent for the whole coming control period contributes
        // exactly zero, so the audio loop runs neither its filters nor its
        // oscillators.
        bool airSounding { false };
        bool boneSounding { false };
        std::array<float, renderedHarmonicCount> harmonicPhases {};
        std::array<float, NeuralModel::boneModeCount> bonePhases {};
        std::array<float, NeuralModel::boneModeCount> boneFrequenciesHz {};
        std::array<float, NeuralModel::boneModeCount> boneFrequencySteps {};
        std::array<float, renderAmplitudeCount> amplitudes {};
        std::array<float, renderAmplitudeCount> amplitudeSteps {};
        // The release damps by frequency, so each rendered slot carries the
        // *excess* attenuation it owes on top of the one release scalar the
        // audio loop applies to the summed voice. Partial 1 owes none of it,
        // which is what keeps the Dissolve time and the retirement decision
        // exactly where the panel says they are. The pair is a running gain
        // and the factor it is multiplied by once per control period; the
        // pow() that builds the factors is paid once, at note-off.
        std::array<float, renderAmplitudeCount> releaseSlotGain {};
        std::array<float, renderAmplitudeCount> releaseSlotDecay {};
        // The Dissolve time the factors above were built for. Negative until
        // the voice releases, so a held voice costs nothing at all.
        float releaseShapeSeconds { -1.0f };
        std::array<Bandpass, NeuralModel::airBandCount> airFilters {};

        void clear() noexcept;
    };

    // A stolen voice leaves its last sample behind, so the tail is a step to be
    // removed rather than a signal to be faded. A raised cosine has zero slope
    // at both ends, which keeps that removal from adding the click it exists to
    // prevent; a linear ramp corners at both ends instead.
    struct FadeTail
    {
        float left { 0.0f };
        float right { 0.0f };
        float position { 0.0f };
        float positionStep { 0.0f };
        int remaining { 0 };

        void clear() noexcept { *this = FadeTail {}; }
    };

    [[nodiscard]] EngineParameters loadParameters() const noexcept;
    [[nodiscard]] float initialPhaseAt(const NeuralModel& model,
                                       float oneBasedIndex) const noexcept;
    void refreshHarmonicStretch(float inharmonicity) noexcept;
    void updateVoiceControl(Voice& voice, const NeuralModel& model,
                            const EngineParameters& parameters) noexcept;
    // Samples the three layers' power across the loop region into the arrays
    // below, once per model. Costs the decoder evaluations the single fit it
    // replaces already cost.
    void sampleLoopLevelTrajectory(const NeuralModel& model) noexcept;
    // Least-squares slope of the mix those samples make at the given layer
    // gains. No decoder work, so it is cheap enough to redo whenever Air or
    // Bone moves.
    void refreshLoopLevelSlope(float air, float bone) noexcept;
    [[nodiscard]] static float fitDampingExponent(
        const NeuralModel& model) noexcept;
    void buildReleaseShape(
        Voice& voice, float releaseSeconds, float renderedFundamentalHz,
        const std::array<float, NeuralModel::airBandCount>& airCentresHz,
        const std::array<float, NeuralModel::boneModeCount>& boneCentresHz)
        const noexcept;
    [[nodiscard]] static float nextNoise(std::uint32_t& state) noexcept;
    void beginFadeTail(std::size_t voiceIndex) noexcept;

    std::atomic<const NeuralModel*> model_ { nullptr };
    AtomicParameters parameters_ {};
    std::array<Voice, maximumVoices> voices_ {};
    std::array<FadeTail, maximumVoices> fadeTails_ {};
    std::array<float, NeuralModel::harmonicCount> inverseHarmonicRolloff_ {};
    // Unit vectors for the published model's learned onset phases, refreshed
    // once per model rather than per note-on.
    std::array<float, NeuralModel::harmonicCount> initialPhaseCos_ {};
    std::array<float, NeuralModel::harmonicCount> initialPhaseSin_ {};
    // Rendered partial frequency divided by the fundamental. Identity for an
    // ideal harmonic series; rebuilt only when the effective stiff-string
    // coefficient actually changes, never per voice and never per sample.
    std::array<float, renderedHarmonicCount> harmonicStretchRatio_ {};
    float cachedInharmonicity_ { -1.0f };
    // ln(retirementLevel), resolved once by the constructor. retirementLevel
    // is a compile-time constant, so this removes a repeated std::log() call
    // from the two places that turn the Dissolve time into the key-tracked
    // release law: buildReleaseShape() (once per voice per release-shape
    // rebuild) and updateVoiceControl() (every control frame of every
    // active voice, the hotter of the two).
    float logRetirementLevel_ { 0.0f };
    // Mean log-amplitude slope of the published model across its own loop
    // region, in nepers per second of model time, fitted once per model by
    // setModel(). Orbit divides it out so that a wrap from loopEnd back to
    // loopStart is level-continuous. Only the trend is removed: whatever
    // tremolo, breath pulsing or beating the region carries is a residual
    // about this line and survives untouched.
    float loopLevelSlopePerSecond_ { 0.0f };
    // The three layers' own power trajectories across the loop region, sampled
    // once per model by setModel() at the points the fit above uses.
    //
    // The slope has to be the slope of the mix that is *rendered*, and the
    // renderer does not render the three layers at full strength: Air and Bone
    // carry their own controls, and Bone drops every mode the analysis could
    // not vouch for. On a source whose layers decay at different rates a fit
    // taken over all three at unity is a fit of a signal nobody is listening
    // to, so moving either control left the correction based on one trend and
    // the wrap on another. Keeping the samples lets the fit be redone from them
    // whenever those controls move, at no decoder cost.
    static constexpr int loopFitPoints = 32;
    std::array<float, loopFitPoints> loopFitSeconds_ {};
    std::array<float, loopFitPoints> loopFitCorePower_ {};
    std::array<float, loopFitPoints> loopFitAirPower_ {};
    std::array<float, loopFitPoints> loopFitBonePower_ {};
    float loopFitLengthSeconds_ { 0.0f };
    // The layer gains the stored slope was last fitted at, so a block that did
    // not move them does no work.
    float loopFitAir_ { -1.0f };
    float loopFitBone_ { -1.0f };
    // The published model's own damping exponent p, fitted once per model by
    // setModel() and bounded to [0, 1.5]. It is the shape the release borrows:
    // a source whose partials all decayed at the same rate fits zero and keeps
    // the frequency-independent release every earlier build had.
    float dampingExponent_ { 0.0f };
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    // Core anti-alias fade constants for coreNyquistGain(), precomputed so the
    // per-sample harmonic loop needs no division.
    float coreNyquistLimitHz_ { 0.49f * 48000.0f };
    float coreNyquistFadeScale_ { 1.0f / (0.06f * 48000.0f) };
    // The same ceiling as a plain float, so the per-sample Bone frequency
    // ramp needs neither a double-to-float conversion nor a multiply.
    float boneCeilingHz_ { 0.49f * 48000.0f };
    // Where the Air and Bone layers fade out. Both are anchored to a fixed
    // audio-band ceiling as well as to the host Nyquist, so a memory keeps the
    // same audible top octave at every supported rate instead of losing it at
    // 44.1 kHz and rendering it ultrasonically at 96 kHz.
    float airEdgeLimitHz_ { 20000.0f };
    float airEdgeFadeHz_ { 2000.0f };
    float boneEdgeLimitHz_ { 20000.0f };
    float boneEdgeFadeHz_ { 1400.0f };
    int controlPeriod_ { 192 };
    // Length of a fresh voice-steal fade tail, in samples: 3 ms, floored at 16
    // samples so an absurdly low host rate cannot collapse it to nothing.
    // Fixed by the sample rate alone, so prepare() resolves it once instead of
    // beginFadeTail() re-deriving it (an std::lround plus a multiply) on every
    // voice steal and every model swap.
    int fadeTailSamples_ { 144 };
    // Output level is the one control applied straight to the summed signal
    // rather than through a control-rate target, so it carries its own
    // smoother. The sentinel means "not yet primed": the first block after
    // prepare() adopts the host's value instead of sliding up to it.
    float smoothedOutputGain_ { -1.0f };
    float outputGainCoefficient_ { 1.0f };
    std::uint64_t ageCounter_ { 0 };
};

} // namespace neuramar
