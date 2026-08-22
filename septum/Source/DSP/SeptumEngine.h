// The Septum engine: a behavioral model of the Roland SH-201's virtual
// analog voice, grounded in the documents catalogued in
// Docs/sh201-replica-research.md. The hardware computes its whole voice on one
// custom DSP behind a documented analog output stage; this engine reproduces
// the documented architecture exactly — two tones of
// OSC1+OSC2 -> MIX/MOD -> FILTER -> AMP with three envelopes and two LFOs per
// tone, a shared modulation-delay -> reverb effects block, 10 voices halved in
// DUAL — and keeps every mapping from a 7-bit panel value to a physical
// quantity in one place (the `mapping` namespace) so each voiced constant can
// be replaced by a measurement without touching the render code.

#pragma once

#include "SeptumPatch.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace septum
{

inline constexpr int maxPolyphony = 10;     // settled: 10 voices
inline constexpr int dualPolyphony = 5;     // settled: DUAL halves polyphony
inline constexpr int controlInterval = 8;   // samples per control tick (voiced)

// --------------------------------------------------------------------------
// Panel-value-to-physics mappings. Tiers per the research contract:
// functions marked [settled] implement a documented law; [reported] follow a
// published measurement of related hardware; [voiced] are this project's
// choices, each owned by an open question.
// --------------------------------------------------------------------------
namespace mapping
{
    // [voiced, OQ-08] CUTOFF 0-127 -> Hz, exponential over ten octaves.
    [[nodiscard]] inline double cutoffHz (double value) noexcept
    {
        return 20.0 * std::exp2 (value * (10.0 / 127.0));
    }

    // [voiced, OQ-08] RESONANCE 0-127 -> state-variable damping k. k = 2 is
    // Q = 0.5; zero is the oscillation threshold; the top of the knob goes
    // slightly negative so self-oscillation grows until the stage limiter
    // holds it, matching the manual's "may not stop at all" warning.
    [[nodiscard]] inline double resonanceDamping (double value) noexcept
    {
        return 2.0 - 2.04 * (value / 127.0);
    }

    // [voiced, OQ-09] Envelope attack 0-127 -> seconds, 1 ms to 5 s.
    [[nodiscard]] inline double attackSeconds (int value) noexcept
    {
        return 0.001 * std::pow (5000.0, value / 127.0);
    }

    // [voiced, OQ-09] Envelope decay/release 0-127 -> seconds to -60 dB,
    // 2 ms to 12 s.
    [[nodiscard]] inline double decaySeconds (int value) noexcept
    {
        return 0.002 * std::pow (6000.0, value / 127.0);
    }

    // [voiced, OQ-10] LFO RATE 0-127 -> Hz, 0.03 to 30 Hz.
    [[nodiscard]] inline double lfoRateHz (double value) noexcept
    {
        return 0.03 * std::pow (1000.0, value / 127.0);
    }

    // [settled] Tempo-synced LFO frequency from the documented note table.
    [[nodiscard]] inline double lfoSyncHz (int tempoBpm, int noteIndex) noexcept
    {
        const double wholeNotes = lfoTempoSyncWholeNotes[static_cast<std::size_t> (
            std::clamp (noteIndex, 0, 19))];
        return (tempoBpm / 60.0) / (4.0 * wholeNotes);
    }

    // [voiced, OQ-10] LFO fade time 0-127 -> seconds.
    [[nodiscard]] inline double lfoFadeSeconds (int value) noexcept
    {
        const double n = value / 127.0;
        return n * n * 10.0;
    }

    // [voiced, OQ-03] Pulse width 0-127 -> duty cycle. Settled endpoints only
    // in words: left approaches a square (50 %), right broadens the pulse.
    [[nodiscard]] inline double pulseDuty (double value) noexcept
    {
        return 0.5 + 0.45 * (value / 127.0);
    }

    // [reported] Szabo's fitted 11th-degree polynomial mapping the detune
    // knob (0-1) to the supersaw offset scaler, quoted verbatim.
    [[nodiscard]] inline double superSawDetuneAmount (double knob) noexcept
    {
        const double x = std::clamp (knob, 0.0, 1.0);
        return ((((((((((10028.7312891634 * x - 50818.8652045924) * x
                        + 111363.4808729368) * x - 138150.6761080548) * x
                      + 106649.6679158292) * x - 53046.9642751875) * x
                    + 17019.9518580080) * x - 3425.0836591318) * x
                  + 404.2703938388) * x - 24.1878824391) * x
                + 0.6717417634) * x + 0.0030115596;
    }

    // [reported] Szabo's measured per-oscillator frequency offsets at full
    // detune, relative to the center oscillator.
    inline constexpr std::array<double, 7> superSawOffsets {
        -0.11002313, -0.06288439, -0.01952356, 0.0,
        0.01991221, 0.06216538, 0.10745242
    };

    // [reported + voiced, OQ-05] Szabo's JP-8000 mix laws evaluated at the
    // fixed mix m = 0.75 the SH-201's missing MIX control is voiced to.
    inline constexpr double superSawFixedMix = 0.75;
    [[nodiscard]] inline double superSawCenterGain() noexcept
    {
        return -0.55366 * superSawFixedMix + 0.99785;
    }
    [[nodiscard]] inline double superSawSideGain() noexcept
    {
        return (-0.73764 * superSawFixedMix + 1.2841) * superSawFixedMix
               + 0.044372;
    }

    // [voiced, OQ-06] FB OSC feedback 0-127 -> loop gain; past unity the
    // in-loop soft clip bounds it.
    [[nodiscard]] inline double fbOscGain (double value) noexcept
    {
        return 1.15 * (value / 127.0);
    }

    // [voiced, OQ-09] Pitch-envelope depth -63..+63 -> semitones.
    [[nodiscard]] inline double pitchEnvSemitones (int depth) noexcept
    {
        return depth * (24.0 / 63.0);
    }

    // [voiced, OQ-08] Filter-envelope depth -63..+63 -> octaves of cutoff.
    [[nodiscard]] inline double filterEnvOctaves (int depth) noexcept
    {
        return depth * (10.0 / 63.0);
    }

    // [voiced, OQ-08] Cutoff velocity sensitivity: offset in octaves at the
    // velocity extremes.
    [[nodiscard]] inline double cutoffVelocityOctaves (int sens,
                                                      double velocity) noexcept
    {
        return (sens / 63.0) * (velocity - 0.5) * 2.0 * 4.0;
    }

    // [voiced, OQ-10] LFO pitch depth -63..+63 -> cents, squared taper to a
    // one-octave extreme.
    [[nodiscard]] inline double lfoPitchCents (int depth) noexcept
    {
        const double n = depth / 63.0;
        return (n >= 0.0 ? 1.0 : -1.0) * n * n * 1200.0;
    }

    // [voiced, OQ-10] LFO filter depth -63..+63 -> octaves.
    [[nodiscard]] inline double lfoFilterOctaves (int depth) noexcept
    {
        return depth * (5.0 / 63.0);
    }

    // [voiced, OQ-11] DRIVE 0-127 -> pre-gain, up to +32 dB.
    [[nodiscard]] inline double overdrivePreGain (int drive) noexcept
    {
        return std::pow (10.0, (drive / 127.0) * (32.0 / 20.0));
    }

    // [voiced, OQ-12] Delay TIME 0-127 -> seconds, 1 ms to 1.3 s.
    [[nodiscard]] inline double delaySeconds (double value) noexcept
    {
        return 0.001 * std::pow (1300.0, value / 127.0);
    }

    // [voiced, OQ-12] Reverb TIME 0-127 with SIZE 0-7 -> RT60 seconds.
    [[nodiscard]] inline double reverbSeconds (int time, int size) noexcept
    {
        const double sizeScale = 0.5 + size * (1.0 / 7.0);
        return 0.15 * std::pow (10.0 / 0.15, time / 127.0) * sizeScale;
    }

    // [voiced, OQ-13] Portamento time 0-127 -> seconds of glide.
    [[nodiscard]] inline double portamentoSeconds (int value) noexcept
    {
        const double n = value / 127.0;
        return n * n * 5.0;
    }

    // [settled] KEY FOLLOW -200..+200: octaves of cutoff per octave of key
    // distance from C4 (+100 tracks 1:1 per the manual's p.36 diagram).
    [[nodiscard]] inline double keyFollowOctavesPerOctave (int keyFollow) noexcept
    {
        return keyFollow / 100.0;
    }

    // [voiced, OQ-07] MIX/MOD LOW FREQ shelf: corner and gain.
    inline constexpr double lowShelfHz = 200.0;
    inline constexpr double lowShelfGainDb = 8.0;
}

// --------------------------------------------------------------------------
// Engine
// --------------------------------------------------------------------------

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // Full-patch update; safe between process calls. Continuous values are
    // smoothed inside the engine, so per-block updates do not zipper.
    void setPatch (const Patch& patch);
    [[nodiscard]] const Patch& currentPatch() const noexcept { return patch_; }

    // System-common controls (documented ranges).
    void setMasterLevel (int level0to127) noexcept;
    void setMasterTuneHz (double a4Hz) noexcept;         // 415.30-466.20
    void setMasterKeyShift (int semitones) noexcept;     // -24..+24
    void setKeyboardOctaveShift (int octaves) noexcept;  // -3..+3
    void setTranspose (int semitones) noexcept;          // -5..+6

    // Performance inputs.
    void noteOn (int note, int velocity1to127);
    void noteOff (int note);
    void setHold (bool down);
    void setPitchBend (double normalised);   // -1..+1 over the per-tone range
    void setModulation (double amount0to1);  // mod lever / CC#1
    void setExpression (double amount0to1);  // CC#11
    void setPartLevel (double amount0to1);   // CC#7
    void setPartPan (double pan);            // CC#10, -1 (left)..+1 (right)
    void setPortamentoControl (int note);    // CC#84: glide source for the next key
    void allNotesOff();
    void allSoundOff();

    void process (float* left, float* right, int numSamples);

    [[nodiscard]] int activeVoiceCount() const noexcept;
    [[nodiscard]] float getOutputLevel (int channel) const noexcept
    {
        return outputLevel_[channel == 0 ? 0 : 1].load (std::memory_order_relaxed);
    }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    // The tail a host should allow for: covers the longest reverb (RT60 15 s
    // at TIME 127 / SIZE 8) with margin. Extreme settings — delay feedback at
    // the documented ±98 % — ring longer on the hardware too; a finite report
    // is the practical bound, not a claim that ±98 % ever fully dies.
    [[nodiscard]] static double maximumTailSeconds() noexcept { return 30.0; }

private:
    enum class Part { Upper, Lower };
    static constexpr int partCount = 2;

    struct Envelope
    {
        enum class Stage { Idle, Attack, Decay, Sustain, Release };
        Stage stage { Stage::Idle };
        double level { 0.0 };
        double attackRate { 0.0 };   // level per sample while attacking
        double decayCoeff { 0.0 };
        double releaseCoeff { 0.0 };
        double sustain { 1.0 };

        void configure (double sr, int a, int d, int s, int r) noexcept;
        void trigger() noexcept { stage = Stage::Attack; }
        void release() noexcept
        {
            if (stage != Stage::Idle)
                stage = Stage::Release;
        }
        void kill() noexcept { stage = Stage::Idle; level = 0.0; }
        double advance (int samples) noexcept;
        [[nodiscard]] bool idle() const noexcept { return stage == Stage::Idle; }
    };

    // Two-stage pitch envelope: linear ramp up over A, exponential fall over D.
    struct PitchEnvelope
    {
        double level { 0.0 };
        bool attacking { false };
        bool active { false };
        double attackRate { 0.0 };
        double decayCoeff { 0.0 };

        void configure (double sr, int a, int d) noexcept;
        void trigger() noexcept { active = true; attacking = true; level = 0.0; }
        double advance (int samples) noexcept;
    };

    struct Lfo
    {
        double phase { 0.0 };
        double heldValue { 0.0 };      // S&H current value
        double randomFrom { 0.0 };     // RND segment endpoints
        double randomTo { 0.0 };
        double fadeLevel { 1.0 };
        bool primed { false };
        std::uint32_t rng { 0x9e3779b9u };

        void restart (bool resetFade) noexcept;
        double nextRandomValue() noexcept;
        double advance (const LfoParams& params, double hz, double fadePerTick,
                        int samples, double sr) noexcept;
    };

    struct SvfStage
    {
        double ic1eq { 0.0 };
        double ic2eq { 0.0 };
        void clear() noexcept { ic1eq = ic2eq = 0.0; }
    };

    struct OscState
    {
        double phase { 0.0 };
        std::array<double, 7> superPhases {};
        std::vector<float> comb;      // FB-OSC feedback line
        int combWrite { 0 };
        double combState { 0.0 };     // in-loop damping memory
        double hpfX1 { 0.0 }, hpfX2 { 0.0 }, hpfY1 { 0.0 }, hpfY2 { 0.0 };

        void clearRuntime() noexcept
        {
            combWrite = 0;
            combState = 0.0;
            hpfX1 = hpfX2 = hpfY1 = hpfY2 = 0.0;
            std::fill (comb.begin(), comb.end(), 0.0f);
        }
    };

    struct BiquadCoeffs
    {
        double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
    };

    struct Voice
    {
        bool active { false };
        Part part { Part::Upper };
        int note { -1 };
        double velocity { 0.0 };
        bool held { false };          // key (or pedal) still down
        std::uint32_t age { 0 };

        OscState osc1 {}, osc2 {};
        PitchEnvelope pitchEnv {};
        Envelope filterEnv {}, ampEnv {};
        SvfStage filter1 {}, filter2 {};
        double shelfState { 0.0 };

        double glidePitch { 0.0 };    // current portamento pitch in note units
        double targetPitch { 0.0 };

        // Control-tick scalars.
        double inc1 { 0.0 }, inc2 { 0.0 };
        double duty1 { 0.5 }, duty2 { 0.5 };
        double superAmount1 { 0.0 }, superAmount2 { 0.0 };
        double fbGain1 { 0.0 }, fbGain2 { 0.0 };
        double filterG { 0.1 }, filterK { 2.0 };
        double ampGainL { 0.0 }, ampGainR { 0.0 };
        BiquadCoeffs superHpf1 {}, superHpf2 {};
        std::uint32_t noiseRng { 0x1234567u };
        // Control slew (voiced ~2.5 ms): stepped cutoff moves (S&H, patch
        // edits) stay audibly steppy without sample-level discontinuities.
        double cutoffOctSlewed { 8.0 };
        double resonanceSlewed { 0.0 };
        bool controlsPrimed { false };
    };

    struct ToneRuntime
    {
        Lfo lfo1 {}, lfo2 {};
        double lfo1Value { 0.0 }, lfo2Value { 0.0 };
        // Solo-mode held-note stack, most recent last.
        std::array<int, 16> heldNotes {};
        std::array<int, 16> heldVelocities {};
        int heldCount { 0 };
        double lastPitch { 60.0 };
        bool anyKeyDown { false };
    };

    struct DelayLine
    {
        std::vector<float> buffer;
        int write { 0 };
        double dampState { 0.0 };
        // Samples written since the last panic. A read reaching further back
        // than this lands on pre-panic material and must return silence, so
        // a panic never has to clear the whole buffer on the audio thread.
        int fresh { 1 << 30 };
    };

    struct Reverb
    {
        static constexpr int lineCount = 8;
        std::array<std::vector<float>, lineCount> lines {};
        std::array<int, lineCount> writes {};
        std::array<int, lineCount> lengths {};
        std::array<double, lineCount> lowStates {};
        std::array<double, lineCount> highStates {};
        std::array<std::vector<float>, 4> diffusers {};
        std::array<int, 4> diffuserWrites {};
        std::vector<float> preDelay;
        int preDelayWrite { 0 };
        double highCutStateL { 0.0 }, highCutStateR { 0.0 };
        // Same panic bookkeeping as DelayLine::fresh, shared by every buffer
        // in the network: their write heads advance in lockstep.
        int fresh { 1 << 30 };
        void clear();
    };

    // -- helpers -----------------------------------------------------------
    const TonePatch& tonePatch (Part part) const noexcept
    {
        return part == Part::Upper ? patch_.upper : patch_.lower;
    }
    ToneRuntime& toneRuntime (Part part) noexcept
    {
        return tones_[part == Part::Upper ? 0 : 1];
    }
    [[nodiscard]] bool partSounds (Part part) const noexcept;
    [[nodiscard]] int partVoiceLimit() const noexcept;
    void startNoteForPart (Part part, int note, int velocity);
    void releaseNoteForPart (Part part, int note);
    Voice* allocateVoice (Part part);
    void triggerVoice (Voice& voice, Part part, int note, double velocity,
                       bool legato);
    void updateVoiceControls (Voice& voice, int tickSamples);
    void renderVoiceTick (Voice& voice, float* mono, int samples);
    void advanceToneLfos (int samples);
    void processEffects (const float* dryL, const float* dryR,
                         const float* delaySendL, const float* delaySendR,
                         const float* reverbSendL, const float* reverbSendR,
                         float* outL, float* outR, int samples);
    void updateEffectCoefficients();
    [[nodiscard]] double noteToHz (double note) const noexcept;
    [[nodiscard]] std::uint32_t nextRandom() noexcept;

    // -- state -------------------------------------------------------------
    Patch patch_ {};
    double sampleRate_ { 44100.0 };
    int maxBlock_ { 512 };

    int masterLevel_ { 127 };
    double masterTuneHz_ { 440.0 };
    int masterKeyShift_ { 0 };
    int octaveShift_ { 0 };
    int transpose_ { 0 };

    double pitchBend_ { 0.0 };
    double modulation_ { 0.0 };
    double expression_ { 1.0 };
    double partLevel_ { 1.0 };
    double partPan_ { 0.0 };
    bool hold_ { false };

    std::array<Voice, maxPolyphony> voices_ {};
    std::array<ToneRuntime, partCount> tones_ {};
    std::uint32_t voiceClock_ { 0 };
    std::uint32_t rng_ { 0x2545f491u };

    // Effects.
    DelayLine delayL_ {}, delayR_ {};
    double delayModPhase_ { 0.0 };
    Reverb reverb_ {};
    double delayTimeSmoothed_ { 0.0 };
    double reverbFeedback_ { 0.0 };

    // Analog output stage state (documented component values).
    double dcX1_[2] {}, dcY1_[2] {};
    double rcState1_[2] {}, rcState2_[2] {};
    double dcCoeff_ { 0.999 };
    double rcCoeff1_ { 1.0 }, rcCoeff2_ { 1.0 };

    // Smoothed globals.
    double smoothedMaster_ { 0.0 };

    // Scratch buffers sized in prepare().
    std::vector<float> scratchMono_, dryL_, dryR_, sendDelayL_, sendDelayR_,
        sendReverbL_, sendReverbR_;

    std::array<std::atomic<float>, 2> outputLevel_ { 0.0f, 0.0f };
};

} // namespace septum
