#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace drumalor
{

enum class Instrument : std::uint8_t
{
    Kick,
    Snare,
    Clap,
    ClosedHat,
    OpenHat,
    Ride,
    Crash,
    LowTom,
    MidTom,
    HighTom,
    Shaker,
    Perc1,
    Perc2,
    Count
};

// How the stick met the drum. A drummer's snare is three instruments, and the
// difference between them is entirely where the stick landed and how long it
// stayed: on the head, on the head and the rim together, or laid flat across a
// hand-damped head with its shaft dropped onto the rim. Every voice accepts the
// value; only the Snare currently does anything but ignore it.
enum class Articulation : std::uint8_t
{
    Head,
    Rimshot,
    CrossStick,
    // The one stroke on a kit played without a stick. The plates are already
    // clamped when it happens and what strikes them is the other plate, so it
    // is neither a closed hat nor a quiet one: no stick noise, a contact patch
    // a hundred times the area of a wooden tip, and the shortest thing the
    // pair can ring.
    FootChick,
    // Three voicings of the two cymbal channels. Neither channel has a modal
    // bank, so none of these is a strike position: they are the machine's own
    // controls - band gains, sample clock, contact time, decay - set to what
    // each cymbal is. That is voicing and not geometry, and it is what the
    // General MIDI notes for these instruments can honestly be given here.
    Bell,
    China,
    Splash
};

struct MidiTrigger
{
    Instrument instrument {};
    Articulation articulation { Articulation::Head };
};

inline constexpr std::size_t instrumentCount = static_cast<std::size_t> (Instrument::Count);
inline constexpr double maximumTailSeconds = 8.0;
// Choke/mute groups A, B and C. Group 0 means the voice never chokes anything.
inline constexpr int chokeGroupCount = 3;
inline constexpr float minimumVoiceLevelDecibels = -24.0f;
inline constexpr float maximumVoiceLevelDecibels = 6.0f;

struct InstrumentParameters
{
    float characterA { 0.5f };
    float characterB { 0.5f };
    float pitch { 0.0f };
    float decay { 0.5f };
    // Per-voice mixer. Level is in decibels so its unity default is exact;
    // pan is the usual -1 (left) to +1 (right) constant-power position.
    float level { 0.0f };
    float pan { 0.0f };
    // 0 = no group, 1..chokeGroupCount = mute group A..C.
    int chokeGroup { 0 };
};

// Kit-wide controls that shape every voice and the shared output bus.
struct KitParameters
{
    // 0 makes the kit machine-tight, 0.5 is the calibrated unit and 1.0
    // doubles the per-hit variation.
    float humanise { 0.5f };
    // Both bus stages are fully bypassed at 0.
    float busDrive { 0.0f };
    float busCompression { 0.0f };
    // How much of the kit each drum's neighbours hear. Fully bypassed at 0,
    // which is the default, so an existing session is unchanged.
    float bleed { 0.0f };
};

struct InstrumentMetadata
{
    Instrument instrument {};
    std::string_view displayName;
    std::string_view slug;
    int standardMidiNote { 36 };
    std::string_view characterALabel;
    std::string_view characterBLabel;
    InstrumentParameters defaultParameters {};
};

[[nodiscard]] const InstrumentMetadata& getInstrumentMetadata (Instrument instrument) noexcept;
[[nodiscard]] std::string_view getInstrumentDisplayName (Instrument instrument) noexcept;
[[nodiscard]] std::string_view getInstrumentSlug (Instrument instrument) noexcept;
[[nodiscard]] int getStandardMidiNote (Instrument instrument) noexcept;
[[nodiscard]] std::string_view getCharacterALabel (Instrument instrument) noexcept;
[[nodiscard]] std::string_view getCharacterBLabel (Instrument instrument) noexcept;
[[nodiscard]] std::optional<Instrument> instrumentForMidiNote (int midiNote) noexcept;
[[nodiscard]] std::optional<MidiTrigger> midiTriggerForNote (int midiNote) noexcept;

// What a note-on's velocity byte is worth, with MIDI 1.0's High Resolution
// Velocity Prefix taken into account when the controller sent one. CC 88
// immediately ahead of a note-on carries the low seven bits of a fourteen-bit
// velocity; every electronic kit that resolves finer than 127 steps over MIDI
// 1.0 sends it this way.
//
// The two paths are deliberately not the same arithmetic. Without a prefix the
// byte is divided by 127, exactly as it always was, so an ordinary note-on is
// bit-identical to what the engine produced before this existed; folding it
// into the fourteen-bit scaling instead would read a full-velocity note as
// 16256/16383, which is 0.07 dB low and would move every existing session.
[[nodiscard]] float velocityFromMidi (
    int velocityByte, std::optional<int> highResolutionLsb = std::nullopt) noexcept;

// The pending CC 88 prefixes, one per MIDI channel.
//
// CC 88 is a channel message and a prefix for the *next* note event on its own
// channel, so it has to be held per channel and taken by that channel's note.
// One pending value for the whole port handed channel 2 the fine bits a
// controller sent for channel 1, and cleared them before channel 1's note
// arrived - which on a kit split across channels is every hit landing on some
// other pad's low velocity bits.
//
// It is state that belongs to a stream of MIDI rather than to the engine, so
// every boundary that ends such a stream has to discard it: a prefix that
// survived a stop, a re-prepare or a panic would give the first note of the
// next stream fine bits drawn from the last one.
class HighResolutionVelocityPrefix
{
public:
    static constexpr int channelCount = 16;

    // Hold what a CC 88 on this channel carries, for that channel's next note.
    void set (int channel, int lowBits) noexcept;

    // Take this channel's prefix, if it has one, and clear it. A prefix belongs
    // to one note event and to no other, so it is taken by a note-off as well
    // as by a note-on: a note-on of velocity zero is a note-off, and leaving the
    // prefix queued would hand it to a later, unrelated stroke.
    [[nodiscard]] std::optional<int> take (int channel) noexcept;

    // One channel's prefix, discarded: what CC 120 and CC 123 do, both of which
    // are channel messages.
    void clear (int channel) noexcept;

    // Every channel's, for the boundaries that are not channel messages at all.
    void clearAll() noexcept;

private:
    [[nodiscard]] static bool valid (int channel) noexcept
    {
        return channel >= 0 && channel < channelCount;
    }

    std::array<std::optional<int>, channelCount> pending_ {};
};

class DrumEngine
{
public:
    DrumEngine() noexcept;

    void prepare (double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void setInstrumentParameters (Instrument instrument,
                                  const InstrumentParameters& values) noexcept;
    void setKitParameters (const KitParameters& values) noexcept;
    void setOutputGain (float linearGain) noexcept;
    // Continuous hi-hat pedal position: 0 is fully open, 1 is tightly closed.
    // Until this is called the two hats behave exactly as they always did, on
    // their own notes; from the first call onwards the pedal is what decides
    // how open the pair is and the notes only choose which channel strip plays.
    // Call it from the same thread that calls trigger(): the plug-in routes
    // MIDI CC 4 to it at the controller event's own sample offset.
    void setHiHatPedal (float position) noexcept;
    [[nodiscard]] float getHiHatPedal() const noexcept;
    void trigger (Instrument instrument, float velocity,
                  Articulation articulation = Articulation::Head) noexcept;
    [[nodiscard]] bool triggerMidi (int midiNote, float velocity) noexcept;
    // A hand closing on ringing bronze, as aftertouch. Pressure 0 does nothing
    // - a controller sends it when the hand comes off, and nothing on this
    // engine can put a cymbal back - and every value above it damps harder,
    // over a time constant rather than as a switch, so a grab that tightens
    // over a few controller messages is a grab that tightens. The channel form
    // takes every ringing cymbal; the note form takes only the cymbal that
    // note plays, which is what polyphonic aftertouch from a pad means.
    void applyAftertouch (float pressure) noexcept;
    bool applyAftertouch (int midiNote, float pressure) noexcept;
    void allSoundsOff() noexcept;
    void process (float* left, float* right, int numSamples) noexcept;
    // Includes short fade-only tails retained to make voice stealing click-free.
    [[nodiscard]] int getActiveVoiceCount() const noexcept;

    // Metering, published once per processed chunk for the editor. All three
    // are already ballistically smoothed inside the engine so a UI that polls
    // slower than the block rate cannot miss a transient.
    [[nodiscard]] float getOutputLevel (int channel) const noexcept;
    [[nodiscard]] float getInstrumentLevel (Instrument instrument) const noexcept;
    // Linear gain currently applied by the bus compressor; 1.0 means no
    // reduction and the stage may be bypassed entirely.
    [[nodiscard]] float getBusGain() const noexcept;
    // What the drawn oscillator of the most recently triggered voice was
    // running at, in hertz, over the last processed sample; zero when the
    // newest voice does not have one. Published with the meters above and read
    // the same way. It exists because the pitch sweep on a membrane collapses
    // inside one period of the settled note - the Kick's envelope is at 1/e in
    // 7.2 ms against a settled period of 20.4 ms - so no estimator of the
    // rendered output can see the sweep, and the only honest way to assert what
    // the sweep does is to ask the oscillator.
    [[nodiscard]] float getNewestVoicePitchHz() const noexcept;

private:
    static constexpr int maxVoices = 64;
    static constexpr int retiringVoiceCount = maxVoices;
    static constexpr int oscillatorCount = 8;
    // Twelve was the size of the mode table. Eighteen is the size of the table
    // with every m > 0 mode allowed to be the pair it physically is: an ideal
    // circular head's m > 0 modes are doubly degenerate, and buildHeadBank now
    // splits the loudest of them into their two members. Six extra slots is
    // what the table's tail plus that splitting needs. The cost is time, and it
    // is small but not nothing: measured against a reverted build, the suite's
    // dense thirteen-voice stress render goes from 0.86 s to 0.95 s, a 9 %
    // increase against a 20 s guardrail.
    static constexpr int resonatorCount = 18;
    static constexpr int metallicBankCount = 5;
    static constexpr int metallicOscillatorCount = 6;
    static constexpr int maximumMetallicDecimatorTaps = 401;
    static constexpr int sineTableSize = 2048;
    static constexpr int sineTableMask = sineTableSize - 1;

    // The 909's cymbal ROMs. On the machine these are mask ROMs holding a
    // recorded cymbal, and every hit reads the same data - so they are engine
    // state built once, not per-voice state. Drumalor generates the contents
    // rather than embedding a recording, but what the counter walks is a fixed
    // table either way.
    //
    // The table is walked cyclically and its contents are built to loop
    // seamlessly, which is the one place this departs from the hardware: a
    // real ROM is finite and the counter stops at its end. Here the address
    // envelope is what ends the sound, so the source has to keep going until
    // it does.
    static constexpr int cymbalRomSize = 32768;
    static constexpr int cymbalRomMask = cymbalRomSize - 1;

    struct CymbalRoms
    {
        std::array<float, cymbalRomSize> ride {};
        std::array<float, cymbalRomSize> crash {};
    };

    struct AtomicInstrumentParameters
    {
        std::atomic<float> characterA { 0.5f };
        std::atomic<float> characterB { 0.5f };
        std::atomic<float> pitch { 0.0f };
        std::atomic<float> decay { 0.5f };
        std::atomic<float> level { 0.0f };
        std::atomic<float> pan { 0.0f };
        std::atomic<int> chokeGroup { 0 };
    };

    struct Biquad
    {
        float b0 { 1.0f };
        float b1 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float z1 { 0.0f };
        float z2 { 0.0f };

        [[nodiscard]] float tick (float input) noexcept;
        void clear() noexcept;
    };

    struct Resonator
    {
        float inputGain { 0.0f };
        // sin(omega)/radius: what a unit strike has to put into the state for
        // the mode to answer with an amplitude of exactly one.
        float strikeGain { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float y1 { 0.0f };
        float y2 { 0.0f };
        // What a1 is when the head is at rest, how far it moves per unit of
        // relative frequency change, and the 2r it can never exceed.
        //
        // a1 = 2 r cos(omega (1 + delta)) is, to first order in delta,
        // nominalA1 - 2 r omega sin(omega) delta. A drum head's tension change
        // is a few per cent, where that linearisation is good to well under a
        // cent, and it costs one multiply-add instead of a cosine. Only a1
        // moves: a2 is -r^2 and is left alone, so a mode's decay time cannot
        // drift with its amplitude the way it would if the pole were retuned.
        float nominalA1 { 0.0f };
        float tensionSlope { 0.0f };
        float poleDiameter { 0.0f };

        [[nodiscard]] float tick (float input) noexcept;
        void setTension (float relativeFrequencyChange) noexcept;
        // Set a mode in motion rather than pushing a sample through it. A
        // struck mode starts at rest and answers A r^n sin(n omega): zero at the
        // instant of contact, rising to A a quarter period later. Driving the
        // same impulse through inputGain instead lands A times the input gain on
        // the very first sample, and that gain is proportional to sin(omega) -
        // six times larger at 8 kHz than at 48 for the same mode, which a bank
        // of twelve modes struck together turns into an audible difference.
        void strike (float amplitude) noexcept;
        void clear() noexcept;
    };

    // One cymbal channel, laid out the way the two machines that made this
    // sound actually lay out: a TR-808 analogue channel and a TR-909 digital
    // one, summed at a single buffer amplifier.
    //
    // The 808 half is the classic block chain - six Schmitt-trigger inverter
    // oscillators summed at a virtual earth, two active band-passes on that
    // node, a trigger pulse through an attack smoother into two envelope
    // generators, three swing-type VCAs, a Sallen-Key high-pass on each band,
    // and a tone-control mixer.
    //
    // The 909 half is a counter walking a ROM into a companded 6-bit DAC, a
    // second DAC on the address lines through an anti-log converter to make
    // the envelope, the VCA that restores it, and the low-pass that removes
    // the sample clock. Drumalor generates the ROM contents rather than
    // embedding any recording.
    struct CymbalChannel
    {
        // 808: the two band-passes hanging off the oscillator summing node.
        Biquad bandLow {};
        Biquad bandHigh {};
        // 808: one Sallen-Key high-pass per band, ahead of the tone mixer.
        Biquad highpassLow {};
        Biquad highpassMid {};
        Biquad highpassHigh {};
        // 909: the reconstruction filter after the DAC and its VCA.
        Biquad reconstruction {};
        // The stick's own contact time, as a first-order tilt on each
        // machine's carrier. A tip touches a plate for a finite time and
        // cannot deliver force above about one cycle per contact, so this is
        // a property of the excitation and sits ahead of both channels'
        // envelopes rather than across the voice's output, where it would
        // blur the onsets the smoothers below are there to shape.
        Biquad contactAnalogue {};
        Biquad contactDigital {};

        // 808 trigger path. The trigger pulse is not a step: it charges the
        // envelope capacitors through the attack smoother, so every band opens
        // over a short ramp rather than switching on.
        float gate { 0.0f };
        float gateCoefficient { 1.0f };
        float peak { 1.0f };
        // 808 swing VCAs: the control-voltage knee and the control leakage
        // that a steered pair puts through even with no signal.
        float vcaKnee { 0.22f };
        float feedthrough { 0.0f };
        // 808 tone-control mixer weights for the three high-passed bands.
        float lowGain { 0.0f };
        float midGain { 0.0f };
        float highGain { 0.0f };
        // Ages past which the mid and high VCAs are shut so far that their
        // bands cannot reach even -150 dB, so the sections feeding them are
        // skipped. Derived from the voice's own decay, so the boundary lands
        // at the same instant at every sample rate and under every host block
        // partitioning. The low band carries the DECAY control and runs for as
        // long as the voice does.
        std::uint64_t midActiveSamples { 0 };
        std::uint64_t highActiveSamples { 0 };

        // 909 trigger path. The data path has no smoother of its own - the
        // counter is reset by the trigger and the first address is read on the
        // next clock - so without this the channel opens in three samples.
        // What it opens through is the same trigger RC the analogue channel
        // uses, and it is the only place on this leg where how fast the stick
        // was travelling can reach the sound at all: the ROM is a recording and
        // the address envelope is walked by the counter, so neither can move.
        float digitalGate { 0.0f };
        float digitalGateCoefficient { 1.0f };

        // 909 sample clock, as a phase increment per engine sample.
        float clockPhase { 1.0f };
        float clockIncrement { 0.625f };
        // The DAC's held output: the current ROM word through the companded
        // quantizer, kept until the next sample clock.
        float hold { 0.0f };
        // The address-line DAC's anti-log envelope and its per-clock ratio.
        // Because it steps with the counter rather than with time, retuning
        // the clock moves pitch and tail length together, exactly as the
        // machine does.
        float romEnvelope { 1.0f };
        float romDecay { 1.0f };
        // An OTA's transconductance sets its bandwidth as well as its gain, so
        // a channel closing down loses its top before it loses its level. This
        // is the pole that moves with the control current; it is retuned once
        // per sample clock rather than once per sample.
        float vcaBandwidthState { 0.0f };
        float vcaBandwidthCoefficient { 1.0f };
        float vcaBandwidthOpen { 1.0f };
        // How far the OTA's pole is allowed to close as its control current
        // falls. The transconductance sets bandwidth as well as gain, so a
        // channel fading out loses its top before its level - but how much top
        // it loses is a bias choice, and the two cymbal channels are not
        // biased alike. A ride's tail is meant to darken as it goes; a crash's
        // is meant to stay a splash for its whole length, which on a tail this
        // long is most of what "bright" means.
        float vcaBandwidthFloor { 0.26f };
        // What the counter is reading, and where it has got to. The ROM is
        // engine-owned and shared by every voice; the address is per-voice
        // because two crashes overlapping are two counters walking the same
        // mask at their own rates, which is exactly what the machine does.
        const float* rom { nullptr };
        float romPhase { 0.0f };
        // The tone mixer is the last stage before the buffer amplifier, so the
        // digital channel passes through it too. It has no three analogue legs
        // of its own, so it is split by a single first-order crossover and
        // weighted by the same control.
        float pcmSplitState { 0.0f };
        float pcmSplitCoefficient { 0.3f };
        float pcmLowGain { 0.0f };
        float pcmHighGain { 0.0f };
    };

    struct HitVariation
    {
        float pitchCents { 0.0f };
        float decayScale { 1.0f };
        float characterAOffset { 0.0f };
        float characterBOffset { 0.0f };
        float transientScale { 1.0f };
        float circuitDriveOffset { 0.0f };
        float circuitBias { 0.0f };
        float phaseOffset { 0.0f };
        // Where around the head this particular stroke landed, in degrees away
        // from the nominal aim. This is the first deviation Humanise has ever
        // made to the strike itself rather than to a control, and it is the
        // only field here with no component-drift term in it: a supply rail and
        // an ambient temperature do not decide where a stick lands, and two
        // drums struck a moment apart have no reason to be hit in the same
        // place. It is per-hit and nothing else.
        float strikeAzimuthDegrees { 0.0f };
    };

    struct Voice
    {
        bool active { false };
        bool choking { false };
        bool bandLimitedNoiseReady { false };
        Instrument instrument { Instrument::Kick };
        Articulation articulation { Articulation::Head };
        int chokeGroup { 0 };
        std::uint64_t generation { 0 };
        std::uint64_t ageSamples { 0 };
        std::uint64_t maximumSamples { 0 };
        std::uint64_t minimumSilenceSamples { 0 };
        // Age after which the modal bank has rung down past -150 dB and can be
        // skipped. Derived from the voice's own decay, so it stays identical at
        // every sample rate and under every host block partitioning.
        std::uint64_t modalActiveSamples { 0 };
        std::uint32_t noiseState { 1u };
        std::uint32_t quietSamples { 0u };
        float velocity { 0.0f };
        float characterA { 0.5f };
        float characterB { 0.5f };
        float pitchRatio { 1.0f };
        float decaySeconds { 0.5f };
        float envelope { 1.0f };
        float envelopeMultiplier { 0.999f };
        float auxiliaryEnvelope { 1.0f };
        float auxiliaryMultiplier { 0.999f };
        float transientEnvelope { 1.0f };
        float transientMultiplier { 0.99f };
        float pitchEnvelope { 1.0f };
        float pitchEnvelopeMultiplier { 0.99f };
        float transientScale { 1.0f };
        float excitationScale { 1.0f };
        // Softer strikes excite fewer high partials on a real drum. This scales
        // the struck-timbre filters and stick content, not just the VCA gain.
        float velocityTimbre { 1.0f };
        float levelGain { 1.0f };
        float circuitDrive { 1.2f };
        float circuitBias { 0.0f };
        // Transfer curve of the voice's output stage. Both curvatures, the
        // quiescent operating point and the makeup gain follow only from the
        // instrument and characterB, so they are resolved once at note-on
        // instead of being rebuilt for every sample of the voice.
        float analogPositiveCurvature { 0.205f };
        float analogNegativeCurvature { 0.165f };
        float analogMakeup { 1.0f };
        float analogZero { 0.0f };
        float analogPreviousInput { 0.0f };
        // Antiderivative of the transfer curve at analogPreviousInput. Carrying
        // it forward halves the transcendental count of the ADAA stage.
        float analogPreviousPrimitive { 0.0f };
        float supplySag { 0.0f };
        float kickStateX { 0.0f };
        float kickStateY { 0.0f };
        float kickCharge { 0.0f };
        float kickBaseRadius { 0.0f };
        // How long the beater or the stick stays on the head. Hertzian: it
        // shortens as the strike gets harder, and everything the strike can
        // reach follows from it. The phase runs from 0 to 1 across the contact
        // and then stops; the increment is one sample's worth of it.
        float contactSeconds { 0.001f };
        float contactPhase { 1.0f };
        float contactIncrement { 1.0f };
        float chokeGain { 1.0f };
        float chokeMultiplier { 1.0f };
        float recentPeak { 0.0f };
        float bandLimitedNoiseCurrent { 0.0f };
        float bandLimitedNoiseNext { 0.0f };
        float bandLimitedNoisePhase { 0.0f };
        // Tension modulation of the head bank. A displaced membrane is a
        // stretched one, so every mode sharpens while the strike energy is
        // still in the head and settles as it rings out. Zero depth on every
        // voice whose resonators are not a membrane.
        float modalEnergy { 0.0f };
        float modalTension { 0.0f };
        float tensionDepth { 0.0f };
        float tensionSmoothing { 1.0f };
        // What the articulation did to the mix. A rimshot drives the wires
        // harder and rings the rim; a cross-stick has the player's hand on the
        // head, so the membrane and the wires are both mostly gone and what is
        // left is the shell.
        float bodyScale { 1.0f };
        float wireScale { 1.0f };
        float rimLevel { 0.0f };
        // How much of the strike is the stick. A wooden tip landing on bronze
        // puts a burst of broadband noise in before the plate answers at all;
        // a plate landing on a plate does not, because there is no tip.
        float strikeNoise { 1.0f };
        // How open the pair was when this hat was struck, and what its decay
        // law was derived at. A pedal that moves re-derives that law at the new
        // aperture: closing adds the friction between two faces on top, and
        // opening again takes the friction away and leaves whatever is still
        // ringing to decay at the open plate's own rate. Opening does not bring
        // the note back - the energy that friction took has gone - which is
        // exactly what a foot splash is.
        float hatAperture { 1.0f };
        // This hit's Humanise draw on the decay, kept so the aperture can be
        // re-derived later without the tolerance drifting each time.
        float decayVariation { 1.0f };
        // The per-sample multiplier the pedal's friction is currently taking
        // out of this voice, or zero if the pedal is not damping it. Lifting
        // the foot releases the choke only while this is still the tightest
        // thing acting on the voice: a mute group or a panic that arrived
        // afterwards is not friction and a pedal must not undo it.
        float pedalFrictionMultiplier { 0.0f };
        float baseFrequency { 100.0f };
        // Where the strike landed around the head, in radians. The head
        // geometry says how far out from the middle the stick was, which is
        // what decides which modes it can reach at all; this says where around
        // the hoop, which is what decides the balance between the two members
        // of every split pair. Read only by buildHeadBank.
        float strikeAzimuth { 0.0f };
        float sweepAmount { 0.0f };
        // How much of the drawn sweep this strike is allowed to use. Latched at
        // note-on from the energy the strike put into the drum, because a head
        // is stiff only because it is stretched, so a ghost stroke bends the
        // pitch hardly at all where an accent bends it a fourth. Unity for
        // everything at or above the velocity where it saturates, which leaves
        // every accent exactly where it was.
        float strikeDepth { 1.0f };
        // What the drawn oscillator ran at over the last rendered sample, in
        // hertz. Written by the voices that have one and read only by the
        // metering pass; nothing in the audio path consumes it.
        float oscillatorFrequency { 0.0f };
        float panLeft { 0.70710678f };
        float panRight { 0.70710678f };
        std::array<float, oscillatorCount> phases {};
        std::array<float, oscillatorCount> phaseIncrements {};
        std::array<float, oscillatorCount> oscillatorAsymmetries {};
        // How many of the bank's slots this voice actually filled. The render
        // loops used to carry the count as a literal, which is how the toms
        // ended up ringing five modes out of twelve.
        int modeCount { 0 };
        std::array<float, resonatorCount> modeGains {};
        std::array<std::uint64_t, 4> burstStarts {};
        std::array<Resonator, resonatorCount> resonators {};
        Biquad filterA {};
        Biquad filterB {};
        Biquad filterC {};
        // Only the Ride and Crash use this; every other voice leaves it at its
        // reset state and never ticks it.
        CymbalChannel cymbal {};
    };

    // A drum that is not being struck is still a drum. The snare's resonant head
    // carries a set of wires resting on it and answers everything the kit puts
    // into the air and the floor; a tom's head answers whatever lands near its
    // own note. Neither is a voice - they exist whether or not their instrument
    // has been played - so they live here rather than in the voice pool.
    static constexpr int sympatheticBedCount = 4;
    static constexpr int sympatheticModeCount = 3;

    struct SympatheticBed
    {
        Instrument instrument { Instrument::Snare };
        bool hasWires { false };
        int modeCount { 0 };
        float lastPitch { 0.0f };
        float lastDecay { 0.5f };
        float panLeft { 0.70710678f };
        float panRight { 0.70710678f };
        std::array<Resonator, sympatheticModeCount> resonators {};
        Biquad drive {};
        Biquad wires {};
        std::uint32_t noiseState { 1u };
    };

    struct RelaxationOscillatorBank
    {
        Instrument instrument { Instrument::ClosedHat };
        int activeOscillators { metallicOscillatorCount };
        // False for the ride and crash banks, whose mix reads only the Schmitt
        // pulses, so their RC integrators can be skipped entirely.
        bool usesCapacitors { true };
        float characterA { 0.5f };
        float output { 0.0f };
        float lastParameterPitch { 0.0f };
        float lastParameterCharacterA { 0.5f };
        std::array<float, metallicOscillatorCount> phases {};
        std::array<float, metallicOscillatorCount> currentIncrements {};
        std::array<float, metallicOscillatorCount> targetIncrements {};
        std::array<float, metallicOscillatorCount> dutyCycles {};
        std::array<float, metallicOscillatorCount> thresholds {};
        std::array<float, metallicOscillatorCount> capacitorStates {};
        std::array<float, metallicOscillatorCount> riseCoefficients {};
        std::array<float, metallicOscillatorCount> fallCoefficients {};
        std::array<float, metallicOscillatorCount> fixedTolerances {};
        std::array<float, maximumMetallicDecimatorTaps> decimatorHistory {};
        int decimatorWriteIndex { 0 };
        // Samples this bank's circuit has been skipped because no voice could
        // observe it. Restored analytically by wakeMetallicOscillatorBank().
        std::uint64_t frozenSamples { 0 };
    };

    // How a struck object's damping rises with frequency. Every real material
    // loses more per cycle as the mode goes up, but by laws that differ enough
    // to be the difference between materials: a stretched film loses to
    // hysteresis in the plastic and to the air it pushes, so its damping climbs
    // steeply; cast bronze has almost no internal loss at all, which is why a
    // cymbal rings for seconds where a drumhead is gone in one.
    //
    // The three shares are normalised to sum to one at the fundamental, so
    // asking for a decay time always gets exactly that decay time and only the
    // modes above it are shortened.
    struct ModalLoss
    {
        float fixed { 1.0f };       // frequency-independent
        float hysteretic { 0.0f };  // rises as omega: a constant loss angle
        float viscous { 0.0f };     // rises as omega squared: rate-of-strain
        // Sound that actually leaves. Only the head bank uses it, because only
        // there does the engine know each mode's multipole order - and it is
        // the term that decides which modes are loud and which ones last, since
        // for a drum those are opposite questions.
        float radiation { 0.0f };
    };

    [[nodiscard]] static bool validInstrument (Instrument instrument) noexcept;
    [[nodiscard]] static std::size_t indexFor (Instrument instrument) noexcept;
    [[nodiscard]] InstrumentParameters snapshotParameters (Instrument instrument) const noexcept;
    [[nodiscard]] float decaySecondsFor (Instrument instrument, float normalizedDecay) const noexcept;
    [[nodiscard]] int findVoiceSlot() const noexcept;
    void initialiseVoice (Voice& voice, Instrument instrument, float velocity,
                          const InstrumentParameters& values, std::uint32_t seed,
                          const HitVariation& variation,
                          Articulation articulation) noexcept;
    void initialiseModalVoice (Voice& voice, const float* ratios, int modeCount,
                               float baseFrequency, float decaySeconds,
                               float spread, float brightness,
                               ModalLoss loss,
                               const float* excitation = nullptr) noexcept;

    // Everything a two-headed drum is, above its fundamental. Geometry that
    // belongs to the instrument rather than to the hit.
    struct HeadGeometry
    {
        // How much of the air the head drags reaches it. The load itself
        // follows from the drum: a wide head against a light film carries far
        // more air than a small one, and it falls off for the finer modes,
        // which move less air per unit area. That is what pushes a real head's
        // overtones above the ideal Bessel ratios rather than below them.
        float airLoadScale { 1.0f };
        float radius { 0.20f };       // m, the head itself
        float strikeRadius { 0.22f }; // where the stick lands, as a fraction of the head radius
        float headDensity { 0.35f };  // kg/m^2 of the film
        float shellDepth { 0.40f };   // m between the two heads
        float contactSeconds { 0.001f }; // how long the strike is on the head
    };

    int buildHeadBank (Voice& voice, float fundamental, const HeadGeometry& head,
                       float decaySeconds, float brightness, ModalLoss loss) noexcept;
    void chokeGroup (int group) noexcept;
    [[nodiscard]] static bool isStruckMembrane (Instrument instrument) noexcept;
    [[nodiscard]] static bool isHiHat (Instrument instrument) noexcept;
    [[nodiscard]] static bool isCymbal (Instrument instrument) noexcept;
    [[nodiscard]] float hiHatAperture (Instrument instrument) const noexcept;
    // How long the pair rings at a given aperture, and the loss law that goes
    // with it. Both are read at note-on and again whenever the pedal moves on
    // a hat that is still sounding, so one place decides what an aperture is.
    [[nodiscard]] float hatDecaySecondsFor (float aperture,
                                            float decayVariation) const noexcept;
    void applyHatAperture (Voice& voice, float aperture) noexcept;
    // Move a ringing mode's pole radius without touching its state, given the
    // angle it is already ringing at as cosine/angle (the caller recovers
    // these from the resonator's own a1/a2 for its own purposes, so this does
    // not re-derive them). The frequency is therefore preserved exactly and
    // only how fast it dies changes. configureResonator cannot be used for
    // this: it clears the resonator, which on a ringing plate is the note
    // stopping.
    void retuneResonatorDecay (Resonator& resonator, float cosine, float angle,
                               float decaySeconds) const noexcept;
    void dampRingingMembrane (Instrument instrument, float velocity) noexcept;
    void beginChoke (Voice& voice, float seconds) noexcept;
    void beginFadeToSilence (Voice& voice, float multiplier) noexcept;
    void retireVoice (const Voice& source) noexcept;
    void silenceVoice (Voice& voice) noexcept;
    // A voice is sounding in one of two pools - the live one and the retiring
    // one voice-stealing keeps around for a click-free fade - and reset(),
    // dampRingingMembrane(), chokeGroup(), allSoundsOff(),
    // updateActiveVoiceCount() and process() each used to walk both with their
    // own identical pair of range-for loops around whatever they actually
    // wanted to do per voice. One helper walking both pools in the same order
    // - voices_ then retiringVoices_ - and calling the functor on every voice
    // it finds replaces every one of those pairs with no change to which
    // voice is visited or when.
    template <typename Fn>
    void forEachVoice (Fn&& fn) noexcept
    {
        for (auto& voice : voices_)
            fn (voice);
        for (auto& voice : retiringVoices_)
            fn (voice);
    }

    template <typename Fn>
    void forEachVoice (Fn&& fn) const noexcept
    {
        for (const auto& voice : voices_)
            fn (voice);
        for (const auto& voice : retiringVoices_)
            fn (voice);
    }
    void addBankReference (Instrument instrument) noexcept;
    void releaseBankReference (Instrument instrument) noexcept;
    void updateActiveVoiceCount() noexcept;
    void applyBusStage (float& left, float& right, float driveAmount,
                        float compressionAmount) noexcept;
    void configureSympatheticBeds() noexcept;
    // Rebuilds one bed's resonators and drive filter(s) from its instrument's
    // current pitch/decay. configureResonator() ends by clearing the
    // resonator's y1/y2, so this is also the unit of "what may stop ringing":
    // only the bed actually being retuned should lose whatever it was
    // sounding, not its three siblings.
    void configureSympatheticBed (std::size_t index) noexcept;
    void updateSympatheticBeds() noexcept;
    void clearSympatheticBeds() noexcept;
    void renderSympatheticBeds (float excitation, float amount,
                                float& left, float& right) noexcept;
    void resetBusStage() noexcept;

    [[nodiscard]] float advanceContact (Voice& voice) noexcept;
    void advanceModalTension (Voice& voice, float bankOutput) noexcept;
    // Strike a voice's modal bank once at note-on and tick it forward every
    // sample it is active - the shape shared by the kick's head, the snare's
    // and tom's membrane, and the hat's plate. applyTension is false for banks
    // with no tensionDepth of their own (the hat's plate is not a membrane),
    // which skips the call rather than relying on advanceModalTension's own
    // early-out, since that still costs a branch on every sample of every
    // voice of that instrument.
    [[nodiscard]] float renderModalBank (Voice& voice, float impulse,
                                         bool applyTension) noexcept;
    [[nodiscard]] float renderVoice (Voice& voice) noexcept;
    [[nodiscard]] float renderKick (Voice& voice) noexcept;
    [[nodiscard]] float renderSnare (Voice& voice) noexcept;
    [[nodiscard]] float renderClap (Voice& voice) noexcept;
    [[nodiscard]] float renderHat (Voice& voice) noexcept;
    [[nodiscard]] float renderRide (Voice& voice) noexcept;
    [[nodiscard]] float renderCrash (Voice& voice) noexcept;
    // Ride and Crash run the identical two-machine signal path - the analogue
    // oscillator bank through its band-passes and the digital ROM channel,
    // summed at one buffer amplifier - and differ only in that amplifier's own
    // gain, which is voiced per machine rather than shared.
    [[nodiscard]] float renderCymbalVoice (Voice& voice, float outputGain) noexcept;
    [[nodiscard]] float renderTom (Voice& voice) noexcept;
    [[nodiscard]] float renderShaker (Voice& voice) noexcept;
    [[nodiscard]] float renderPerc1 (Voice& voice) noexcept;
    [[nodiscard]] float renderPerc2 (Voice& voice) noexcept;

    [[nodiscard]] float oscillator (Voice& voice, int oscillatorIndex) const noexcept;
    void resetMetallicOscillatorBanks() noexcept;
    void wakeMetallicOscillatorBank (RelaxationOscillatorBank& bank) noexcept;
    void wakeMetallicOscillatorBankFor (Instrument instrument) noexcept;
    void configureMetallicDecimator() noexcept;
    void updateMetallicBankParameterTargets() noexcept;
    void configureMetallicOscillatorBank (Instrument instrument, float pitchRatio,
                                          float characterA, bool snap) noexcept;
    void renderMetallicOscillatorBanks (std::uint32_t activeBankMask) noexcept;
    [[nodiscard]] float renderMetallicBankSubstep (
        RelaxationOscillatorBank& bank) noexcept;
    [[nodiscard]] float decimateMetallicBank (
        const RelaxationOscillatorBank& bank) const noexcept;
    [[nodiscard]] float metallicSourceFor (Instrument instrument) const noexcept;
    [[nodiscard]] static int metallicBankIndexFor (Instrument instrument) noexcept;

    // The TR-808 analogue cymbal channel and the TR-909 digital one. Both
    // cymbal voices run both; only the balance and the tuning differ, and all
    // of that is resolved into the voice at note-on.
    void configureCymbalChannel (Voice& voice, Instrument instrument,
                                 float velocity, float machineSelect,
                                 Articulation articulation) noexcept;
    [[nodiscard]] static float swingVcaGain (float control, float knee) noexcept;
    struct CymbalBands
    {
        float low { 0.0f };
        float mid { 0.0f };
        float high { 0.0f };
    };
    [[nodiscard]] CymbalBands renderCymbalBands (Voice& voice,
                                                 float source) noexcept;
    [[nodiscard]] float boardDriftAt (std::uint64_t sampleIndex) const noexcept;
    [[nodiscard]] static float companding6BitDac (float value) noexcept;
    [[nodiscard]] float nextCymbalPcm (Voice& voice) const noexcept;
    // One mask for the whole process. See the definition for why this is not
    // per-engine state.
    [[nodiscard]] static const CymbalRoms& cymbalRoms() noexcept;
    void sineAndCosineLookup (float phase, float& sine, float& cosine) const noexcept;
    // The xorshift32 core nextNoise() runs on a voice's own noiseState. The
    // sympathetic beds carry the identical generator on their own state field
    // rather than a voice's, so this takes the state by reference instead of
    // taking a Voice, and both callers share the one implementation.
    [[nodiscard]] static float advanceXorshiftNoise (std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextNoise (Voice& voice) noexcept;
    [[nodiscard]] float nextBandLimitedNoise (Voice& voice) const noexcept;
    [[nodiscard]] static std::uint32_t hash32 (std::uint32_t value) noexcept;
    [[nodiscard]] static float signedUnitFromHash (std::uint32_t value) noexcept;
    [[nodiscard]] float applyAnalogOutputStage (Voice& voice, float input) const noexcept;
    void configureHighpass (Biquad& filter, float frequency, float q) const noexcept;
    void configureBandpass (Biquad& filter, float frequency, float q) const noexcept;
    void configureLowpass (Biquad& filter, float frequency, float q) const noexcept;
    // One pole, bilinear, in the same Biquad the two-pole sections use. A
    // contact time is a first-order tilt rather than a corner, and it has to
    // land on the same hertz at every host rate.
    void configureOnePoleLowpass (Biquad& filter, float frequency) const noexcept;
    void configureResonator (Resonator& resonator, float frequency,
                             float decaySeconds) const noexcept;

    // Level and Pan are channel-strip controls rather than per-hit properties,
    // so their current values are republished each block and applied to voices
    // that are already ringing.
    struct MixerTarget
    {
        float levelGain { 1.0f };
        float panLeft { 0.7071f };
        float panRight { 0.7071f };
    };
    std::array<MixerTarget, instrumentCount> mixerTargets_ {};
    std::array<AtomicInstrumentParameters, instrumentCount> parameters_ {};
    std::array<std::uint64_t, instrumentCount> triggerCounters_ {};
    std::array<float, instrumentCount> componentDrift_ {};
    // Engine time, in samples since the last reset. Advanced by whole blocks,
    // so it does not depend on how the host divides them.
    std::uint64_t engineSamples_ { 0 };
    // How many times process()'s own numSamples <= 0 guard has returned
    // early. Exists so tests can pin that guard directly, since a
    // non-positive block is otherwise observationally identical to simply
    // not calling process() at all.
    std::uint64_t nonPositiveProcessCallCount_ { 0 };

    friend struct DrumEngineTestAccess;
    std::array<Voice, maxVoices> voices_ {};
    std::array<Voice, retiringVoiceCount> retiringVoices_ {};
    std::array<SympatheticBed, sympatheticBedCount> sympatheticBeds_ {};
    std::array<RelaxationOscillatorBank, metallicBankCount> metallicBanks_ {};
    std::array<int, metallicBankCount> metallicBankVoiceCounts_ {};
    std::array<float, maximumMetallicDecimatorTaps> metallicDecimatorCoefficients_ {};
    std::array<float, sineTableSize> sineTable_ {};
    std::array<std::atomic<float>, instrumentCount> instrumentLevels_ {};

    // Pedal position and whether a controller has ever set it. Both are only
    // touched from the trigger/audio path and from reset().
    float hiHatPedal_ { 0.0f };
    bool hiHatPedalActive_ { false };

    std::atomic<float> outputGain_ { 0.82f };
    std::atomic<float> humanise_ { 0.5f };
    std::atomic<float> busDrive_ { 0.0f };
    std::atomic<float> busCompression_ { 0.0f };
    std::atomic<float> bleed_ { 0.0f };
    std::atomic<float> outputLevelLeft_ { 0.0f };
    std::atomic<float> outputLevelRight_ { 0.0f };
    std::atomic<float> busGainMeter_ { 1.0f };
    std::atomic<float> newestVoicePitch_ { 0.0f };
    std::atomic<int> activeVoiceCount_ { 0 };
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    int maxBlockSize_ { 512 };
    bool prepared_ { false };
    bool anyVoiceActive_ { false };
    std::uint32_t metallicBankMask_ { 0u };
    std::uint64_t generation_ { 0 };
    std::uint64_t maximumVoiceSamples_ { 384000 };
    std::uint64_t forcedFadeStartSamples_ { 383760 };
    std::uint32_t naturalQuietHoldSamples_ { 2160u };
    float peakReleaseMultiplier_ { 0.999f };
    float retirementFadeMultiplier_ { 0.999f };
    float forcedFadeMultiplier_ { 0.999f };
    float sagAttackCoefficient_ { 0.01f };
    float sagReleaseCoefficient_ { 0.001f };
    float gainSmoothingCoefficient_ { 0.001f };
    float dcBlockerCoefficient_ { 0.9984f };
    float modalNoiseScale_ { 1.0f };
    float bandLimitedNoiseIncrement_ { 1.0f };
    float metallicInternalSampleRate_ { 192000.0f };
    float metallicInverseSampleRate_ { 1.0f / 192000.0f };
    float metallicIncrementSmoothing_ { 0.01f };
    int metallicOversampleFactor_ { 4 };
    int metallicDecimatorTapCount_ { 257 };
    float smoothedOutputGain_ { 0.82f };
    // Both bus controls are ramped at the master gain's 20 ms constant instead
    // of stepping once per block, which used to click on automation.
    float smoothedBusDrive_ { 0.0f };
    float smoothedBusCompression_ { 0.0f };
    float smoothedBleed_ { 0.0f };
    // The previous sample's dry mix, before anything the beds added to it. One
    // sample of delay taken from the mix the beds cannot see makes the whole
    // path strictly feed-forward, so there is no loop to be stable about.
    float bleedExcitation_ { 0.0f };
    float dcInputLeft_ { 0.0f };
    float dcInputRight_ { 0.0f };
    float dcOutputLeft_ { 0.0f };
    float dcOutputRight_ { 0.0f };
    float masterAdaaPreviousLeft_ { 0.0f };
    float masterAdaaPreviousRight_ { 0.0f };
    float masterAdaaPrimitiveLeft_ { 0.0f };
    float masterAdaaPrimitiveRight_ { 0.0f };
    float busEnvelope_ { 0.0f };
    float busGain_ { 1.0f };
    float busDriveAdaaLeft_ { 0.0f };
    float busDriveAdaaRight_ { 0.0f };
    float busAttackCoefficient_ { 0.05f };
    float busReleaseCoefficient_ { 0.002f };
    float meterPeakLeft_ { 0.0f };
    float meterPeakRight_ { 0.0f };
};

} // namespace drumalor
