// The parameter surface of the modelled instrument, adopted verbatim from the
// Roland SH-201 MIDI Implementation v1.00 (2006-03-01) parameter address map.
// Raw ranges and enumeration orders follow that document exactly; signed
// displays use the document's `display = raw - 64` convention, stored here
// already decoded (so a depth field really holds -63..+63). Where a value's
// physical meaning is not settled by any source, the mapping lives in the
// engine and is recorded as a voiced choice in Docs/sh201-replica-research.md.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace septum
{

// SysEx enumeration order 0-8 (Patch Tone offset 00/06).
enum class Waveform
{
    Saw,
    Square,
    PulseSquare,
    Triangle,
    Sine,
    Noise,
    FbOsc,
    SuperSaw,
    ExtIn
};

// SysEx enumeration order 0-2 (Patch Tone offset 0E).
enum class MixModType { Mix, Sync, Ring };

// SysEx enumeration order 0-2 (Patch Tone offset 10): FLAT, BOOST, CUT.
// The panel button cycles CUT -> FLAT -> BOOST, but the stored value order is
// this one; conflating the two orders is a documented trap.
enum class LowFreqMode { Flat, Boost, Cut };

// SysEx enumeration order 0-3 (Patch Tone offset 11): BYPASS, LPF, HPF, BPF.
// The panel cycles LPF -> HPF -> BPF -> BYPASS.
enum class FilterType { Bypass, Lpf, Hpf, Bpf };

enum class FilterSlope { Db12, Db24 };

// The AUDIO FILTER on the external-input path is a separate circuit from the
// voice filter and has one type the voice filter does not: the panel cycles
// LPF -> HPF -> BPF -> NOTCH -> LPF (OM p. 50).
enum class AudioFilterType { Lpf, Hpf, Bpf, Notch };

// SysEx enumeration order 0-6 (Patch Tone offsets 27/31).
enum class LfoShape { Tri, Sin, Saw, Sqr, Trapezoid, SampleHold, Random };

// LFO destination 1 (0-3) and destination 2 (0-2), per the address map.
enum class LfoDest1 { Pitch1, Pw1, Filter, AudioFilter };
enum class LfoDest2 { Pitch2, Pw2, Amp };

// Patch Common offset 11.
enum class KeyboardMode { Single, Dual, Split };
enum class KeyboardPart { Upper, Lower };

// Patch Tone offset 3F: POLY, SOLO+LEGATO, SOLO.
enum class MonoMode { Poly, SoloLegato, Solo };

// ARPEGGIO parameters (OM p. 66), saved with each patch.
//
// GRID: the note division of one grid section, and how much shuffle
// syncopation is applied to it (none / light / heavy).
enum class ArpeggioGrid
{
    Quarter,        // 1/4  — one grid section = one beat
    Eighth,         // 1/8  — two grid sections = one beat
    EighthLight,    // 1/8L — two sections, light shuffle
    EighthHeavy,    // 1/8H — two sections, heavy shuffle
    Twelfth,        // 1/12 — eighth triplet, three sections = one beat
    Sixteenth,      // 1/16 — four sections = one beat
    SixteenthLight, // 1/16L
    SixteenthHeavy, // 1/16H
    TwentyFourth    // 1/24 — sixteenth triplet, six sections = one beat
};

// DURATION: how much of the grid the note occupies. FUL sustains until the
// next new sound is specified even without a tie.
enum class ArpeggioDuration
{
    P30, P40, P50, P60, P70, P80, P90, P100, P120, Full
};

// MOTIF: how the style's note rows are mapped onto the keys held down. The
// suffixes are the manual's: (L) sounds the lowest key every time, (L&H) the
// lowest and the highest every time, (-) neither.
enum class ArpeggioMotif
{
    UpL, UpLowHigh, Up,
    DownL, DownLowHigh, Down,
    UpDownL, UpDownLowHigh, UpDown,
    RandomL, Random,
    Phrase
};

// SPLIT ARPEGGIO: which tone(s) the arpeggiator drives in SPLIT mode.
enum class SplitArpeggio { Upper, Lower, Both };

// D BEAM ASSIGN (Patch Common offset 1F, 0-36), in the address map's own
// order.
//
// [settled range, no effect] The replica does not implement the D Beam: an
// infrared distance sensor is a control surface, and a plug-in has no hand
// above it to read. The four bytes the beam owns in the patch are still
// patch data, so they are stored and round-tripped and change nothing that
// sounds, exactly as PITCH WIDE does. This enumeration survives because it
// is what bounds offset 1F.
enum class DBeamAssign
{
    Osc1Pitch, Osc1Detune, Osc1Pw,
    Osc2Pitch, Osc2Detune, Osc2Pw,
    MixModBalance,
    FilterCutoff, FilterResonance, FilterCutoffKeyFollow, AmpLevel,
    AudioFilterCutoff, AudioFilterResonance,
    PitchEnvA, PitchEnvD, Osc1PitchEnvDepth, Osc2PitchEnvDepth,
    Lfo1Rate, Lfo1Depth1, Lfo1Depth2,
    Lfo2Rate, Lfo2Depth1, Lfo2Depth2,
    FilterEnvA, FilterEnvD, FilterEnvS, FilterEnvR, FilterEnvDepth,
    AmpEnvA, AmpEnvD, AmpEnvS, AmpEnvR,
    DelayTime, DelayDepth, ReverbTime, ReverbDepth,
    Bender
};
inline constexpr int dBeamAssignCount = 37;

// D BEAM POLARITY (Patch Common offset 20). "'+' and '-' will invert the
// direction of change" (OM p. 65). [settled range, no effect] — stored for
// the same reason DBeamAssign is.
enum class DBeamPolarity { Plus, Minus };

// CONTROLLER DESTINATION (Patch Common offsets 15, 16, 17, 18): which tone(s)
// each of the four physical controllers reaches. "Selects the tone(s) to be
// modulated by the modulation lever ... If this is 'BOTH,' modulation will be
// applied to both the UPPER tone and LOWER tone" (OM p. 65), and the same
// sentence for the D Beam, the pitch bend lever and the expression pedal.
enum class ToneDestination { Upper, Lower, Both };

[[nodiscard]] inline bool destinationReaches (ToneDestination destination,
                                              bool upper) noexcept
{
    return destination == ToneDestination::Both
           || (destination == ToneDestination::Upper) == upper;
}

inline constexpr int arpeggioMaxSteps = 32;
inline constexpr int arpeggioMaxRows = 16;
// Grid cell encodings: a rest, a tie holding the preceding note, or a note-on
// carrying the style's programmed velocity.
inline constexpr signed char arpeggioRest = 0;
inline constexpr signed char arpeggioTie = -1;
// How a cell travels through SysEx. [settled] Patch Arpeggio Pattern gives
// each step "Step<n> Data (0 - 128)" as a two-byte nibble (MIDI
// Implementation v1.00 p. 5), so the wire field holds 129 values -- exactly
// the number of states a cell has: a rest, 127 velocities and a tie.
// [voiced] Which end of that range is the tie is not written down; 128 is
// this project's choice, and because the counts match exactly nothing is
// clipped either way.
inline constexpr int arpeggioRestValue = 0;
inline constexpr int arpeggioTieValue = 128;

// "A series of data for basic arpeggio patterns and chord styles recorded in
// the form of a grid consisting of a maximum of 32 steps x 16 pitches"
// (OM p. 67). One style is saved per patch. Roland's own 32 templates are
// unpublished data and none of them ships here; the styles this project
// supplies are original patterns.
struct ArpeggioStyle
{
    int endStep { 4 };   // 1-32
    std::array<std::array<signed char, arpeggioMaxRows>, arpeggioMaxSteps> cells {};
    // [settled] Each of the sixteen rows carries an "Original Note (0 - 128)"
    // of its own -- Patch Arpeggio Pattern (Note 1..16) offset 00, MIDI
    // Implementation v1.00 p. 5. The document records the field but not what
    // reads it, so this engine only stores it: a patch dumped from hardware
    // survives a load and a re-save intact, and the PHRASE motif keeps the
    // voiced reading recorded under OQ-15. Nothing in the shipped styles sets
    // it.
    std::array<int, arpeggioMaxRows> originalNote {};

    [[nodiscard]] signed char cell (int step, int row) const noexcept
    {
        if (step < 0 || step >= arpeggioMaxSteps || row < 0
            || row >= arpeggioMaxRows)
            return arpeggioRest;
        return cells[static_cast<std::size_t> (step)][static_cast<std::size_t> (row)];
    }

    // The highest row the style uses: the width of the window it slides over
    // the keys held down.
    [[nodiscard]] int rowSpan() const noexcept
    {
        int span = 1;
        for (int step = 0; step < std::min (endStep, arpeggioMaxSteps); ++step)
            for (int row = 0; row < arpeggioMaxRows; ++row)
                if (cell (step, row) != arpeggioRest)
                    span = std::max (span, row + 1);
        return span;
    }
};

struct ArpeggioParams
{
    // Which of the supplied styles is loaded. The hardware stores the grid
    // itself in the patch and its panel only *selects* a template, so the
    // index is the panel's surface and `style` below stays the authority on
    // what actually plays.
    int styleIndex { 0 };
    // END STEP is its own front-panel control on the hardware, 1-32 and
    // independent of the template. Zero is the replica's own addition
    // (voiced): it means "however long the selected template is", so a patch
    // that never touches END STEP keeps whatever the style defines.
    int endStep { 0 };         // 0 = the template's own length, else 1-32
    bool on { false };
    bool hold { false };
    SplitArpeggio splitArpeggio { SplitArpeggio::Both };
    int octaveRange { 0 };     // -3..+3
    int accent { 100 };        // 0-100
    int velocity { 0 };        // 0 = REAL (the played velocity), else 1-127
    ArpeggioGrid grid { ArpeggioGrid::Sixteenth };
    ArpeggioDuration duration { ArpeggioDuration::P80 };
    ArpeggioMotif motif { ArpeggioMotif::Up };
    ArpeggioStyle style {};
};

// Patch Common offset 1E: what the modulation lever modulates.
enum class ModulationAssign
{
    Osc1AndOsc2,
    Osc1,
    Osc2,
    Pw1,
    Pw2,
    Filter,
    Amp,
    AudioFilter
};

struct OscParams
{
    Waveform wave { Waveform::Saw };
    bool pitchWide { false };   // expands the coarse range from +/-12 to +/-36
    int coarse { 0 };           // -36..+36 semitones (panel +/-12 without WIDE)
    int fine { 0 };             // -50..+50 cents
    int pulseWidth { 0 };       // 0-127: PW / FB amount / supersaw spread
    int pitchEnvDepth { 0 };    // -63..+63
};

struct LfoParams
{
    LfoShape shape { LfoShape::Tri };
    int rate { 64 };            // 0-127
    bool tempoSync { false };
    int tempoSyncNote { 11 };   // 0-19 into lfoTempoSyncWholeNotes (11 = 1/4)
    int fadeTime { 0 };         // 0-127
    bool keyTrigger { false };
    LfoDest1 destination1 { LfoDest1::Pitch1 };
    int depth1 { 0 };           // -63..+63 (negative inverts the waveform)
    LfoDest2 destination2 { LfoDest2::Pitch2 };
    int depth2 { 0 };           // -63..+63
};

// One complete synthesizer tone: the 0x40-byte Patch Tone block.
struct TonePatch
{
    OscParams osc1 {};
    OscParams osc2 {};
    int pitchEnvAttack { 0 };      // 0-127, shared by both oscillators
    int pitchEnvDecay { 40 };      // 0-127

    MixModType mixType { MixModType::Mix };
    int balance { -63 };           // -63 (OSC1 only)..+63 (OSC2 only)
    LowFreqMode lowFreq { LowFreqMode::Flat };

    FilterType filterType { FilterType::Lpf };
    FilterSlope filterSlope { FilterSlope::Db24 };
    int cutoff { 127 };            // 0-127
    int keyFollow { 0 };           // -200..+200 in steps of 10
    int cutoffVelocitySens { 0 };  // -63..+63
    int resonance { 0 };           // 0-127
    int filterEnvAttack { 0 };
    int filterEnvDecay { 60 };
    int filterEnvSustain { 127 };
    int filterEnvRelease { 20 };
    int filterEnvDepth { 0 };      // -63..+63

    bool overdrive { false };
    int drive { 64 };              // 0-127
    int level { 100 };             // 0-127
    int levelVelocitySens { 0 };   // -63..+63
    int pan { 0 };                 // -64 (L64)..+63 (63R)
    int ampEnvAttack { 0 };
    int ampEnvDecay { 60 };
    int ampEnvSustain { 127 };
    int ampEnvRelease { 10 };

    int delayDepth { 0 };          // 0-127 per-tone send
    int reverbDepth { 0 };         // 0-127 per-tone send

    LfoParams lfo1 {};
    LfoParams lfo2 { LfoShape::Tri, 80, false, 11, 0, false,
                     LfoDest1::Pitch1, 0, LfoDest2::Pitch2, 0 };

    int bendRange { 2 };           // 0-24 semitones
    int octaveShift { 0 };         // -3..+3
    bool portamento { false };
    int portamentoTime { 20 };     // 0-127
    MonoMode mono { MonoMode::Poly };
};

// Patch Delay block (modulation delay), 5 bytes.
struct DelayParams
{
    int time { 60 };       // 0-127
    int feedback { 30 };   // -98..+98 %, negative inverts the phase
    int hfDamp { 17 };     // 0-17 into delayHfDampHz (17 = BYPASS)
    int modulationRate { 0 };
    int modulationDepth { 0 };
};

// Patch Reverb block, 10 bytes.
struct ReverbParams
{
    int time { 64 };       // 0-127
    int preDelay { 0 };    // 0-125 -> 0.0-100.0 ms
    int size { 4 };        // 0-7 displayed 1-8
    int highCut { 20 };    // 0-20 into reverbHighCutHz (20 = BYPASS)
    int density { 96 };    // 0-127
    int diffusion { 96 };  // 0-127
    int lfDampFrequency { 7 };  // 0-19 into reverbLfDampHz
    int lfDampGain { 0 };  // -36..0 dB (0 = no damping)
    int hfDampFrequency { 3 };  // 0-5 into reverbHfDampHz
    int hfDampGain { -6 }; // -36..0 dB
};

// The external-input path: INPUT VOL, CENTER CANCEL and the AUDIO FILTER.
// The manual states three times over that none of it is stored in the patch
// (OM pp. 49-51), so it lives outside `Patch` exactly as it does on the
// instrument — a system setting the panel owns, not patch data.
struct ExternalInput
{
    int inputVolume { 100 };                     // 0-127, INPUT VOL knob
    bool centerCancel { false };                 // CENTER CANCEL ON button
    bool filterOn { false };                     // FILTER ON button
    AudioFilterType type { AudioFilterType::Lpf };
    FilterSlope slope { FilterSlope::Db12 };
    int cutoff { 127 };                          // 0-127, CC#2
    int resonance { 0 };                         // 0-127, CC#4
};

struct Patch
{
    std::string name { "INIT PATCH" };  // up to 12 ASCII characters
    int patchLevel { 100 };             // 0-127
    int toneBalance { 0 };              // -63 (LOWER)..+63 (UPPER)
    int tempo { 120 };                  // 5-300 BPM
    KeyboardMode keyboardMode { KeyboardMode::Single };
    KeyboardPart keyboardPart { KeyboardPart::Upper };
    int splitPoint { 60 };              // note number 21-108 (A0-C8)
    bool delayOn { false };
    bool reverbOn { false };
    ModulationAssign modulationAssign { ModulationAssign::Osc1AndOsc2 };
    // Settled: which tone(s) each controller reaches (OM p. 65).
    ToneDestination modulationDestination { ToneDestination::Both };
    ToneDestination pitchBendDestination { ToneDestination::Both };
    ToneDestination expressionDestination { ToneDestination::Both };
    // The four Patch Common bytes the D Beam owns: destination (offset 16),
    // ACTIVE EXPRESSION (19), assign (1F) and polarity (20). The replica does
    // not implement the beam — see DBeamAssign above — so all four are
    // [settled range, no effect]: stored, saved and round-tripped through
    // SysEx so a dump from a real unit survives the trip, and read by nothing
    // that sounds. ACTIVE EXPRESSION is a modifier of the beam's EXPRESS
    // button alone (OM p. 65) and no other controller on the instrument can
    // drive it, so it is inert with the rest rather than re-pointed at one.
    ToneDestination dBeamDestination { ToneDestination::Both };
    bool activeExpression { false };
    DBeamAssign dBeamAssign { DBeamAssign::FilterCutoff };
    DBeamPolarity dBeamPolarity { DBeamPolarity::Plus };
    ArpeggioParams arpeggio {};

    TonePatch upper {};
    TonePatch lower {};
    DelayParams delay {};
    ReverbParams reverb {};
};

// --------------------------------------------------------------------------
// Settled value tables, quoted from the MIDI Implementation — and, since the
// SH-201 Editor's `Resource.xml` was read, corroborated entry for entry by
// Roland's own display tables: `lfoTempoSyncNoteTable`, `freq200-BypassTable`,
// `hiCutTable`, `lfDampFreqTable`, `hfDampFreqTable` and `dampGainTable`. The
// enumeration orders agree too — `oscWaveFormTable` (SAW, SQR, PW-SQR, TRI,
// SIN, NOISE, FB-OSC, SUPER-SAW, EXT-IN), `lfoWaveFormTable` (TRI, SIN, SAW,
// SQR, TRP, S&H, RANDOM), `arpeggioGridTable` and `arpeggioDurationTable`.
// --------------------------------------------------------------------------

// LFO TEMPO SYNC NOTE (0-19), in whole notes.
inline constexpr std::array<double, 20> lfoTempoSyncWholeNotes {
    16.0, 12.0, 8.0, 4.0, 2.0, 1.0,
    3.0 / 4.0, 2.0 / 3.0, 1.0 / 2.0, 3.0 / 8.0, 1.0 / 3.0, 1.0 / 4.0,
    3.0 / 16.0, 1.0 / 6.0, 1.0 / 8.0, 3.0 / 32.0, 1.0 / 12.0, 1.0 / 16.0,
    1.0 / 24.0, 1.0 / 32.0
};

// Delay HF DAMP (0-17); the final entry is BYPASS, encoded as 0.
inline constexpr std::array<double, 18> delayHfDampHz {
    200.0, 250.0, 315.0, 400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0, 1600.0,
    2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 0.0
};

// Reverb HIGH CUT (0-20); the final entry is BYPASS, encoded as 0.
inline constexpr std::array<double, 21> reverbHighCutHz {
    160.0, 200.0, 250.0, 320.0, 400.0, 500.0, 640.0, 800.0, 1000.0, 1250.0,
    1600.0, 2000.0, 2500.0, 3200.0, 4000.0, 5000.0, 6400.0, 8000.0, 10000.0,
    12500.0, 0.0
};

// Reverb LF DAMP FREQUENCY (0-19).
inline constexpr std::array<double, 20> reverbLfDampHz {
    50.0, 64.0, 80.0, 100.0, 125.0, 160.0, 200.0, 250.0, 320.0, 400.0,
    500.0, 640.0, 800.0, 1000.0, 1250.0, 1600.0, 2000.0, 2500.0, 3200.0,
    4000.0
};

// Reverb HF DAMP FREQUENCY (0-5).
inline constexpr std::array<double, 6> reverbHfDampHz {
    4000.0, 5000.0, 6400.0, 8000.0, 10000.0, 12500.0
};

// --------------------------------------------------------------------------
// Clamping helpers: the engine trusts nothing outside the documented ranges.
// --------------------------------------------------------------------------

[[nodiscard]] inline int clampRaw (int value, int low, int high) noexcept
{
    return std::clamp (value, low, high);
}

inline void clampToDocumentedRanges (OscParams& osc) noexcept
{
    // [settled] Coarse Tune is raw 28-100, i.e. -36..+36 semitones, and the
    // address map does not narrow it when PITCH WIDE is off: the switch is
    // its own byte and the manual says what it gates — "This button expands
    // the range of the PITCH knob by a multiple of three" (OM p. 29). It is
    // the knob's travel, not the stored pitch, so a stored +24 with WIDE off
    // sounds +24 on the instrument and does here. Clamping the sounding pitch
    // by it made the panel and the host show a pitch the engine did not play.
    osc.coarse = clampRaw (osc.coarse, -36, 36);
    osc.fine = clampRaw (osc.fine, -50, 50);
    osc.pulseWidth = clampRaw (osc.pulseWidth, 0, 127);
    osc.pitchEnvDepth = clampRaw (osc.pitchEnvDepth, -63, 63);
}

inline void clampToDocumentedRanges (LfoParams& lfo) noexcept
{
    lfo.rate = clampRaw (lfo.rate, 0, 127);
    lfo.tempoSyncNote = clampRaw (lfo.tempoSyncNote, 0, 19);
    lfo.fadeTime = clampRaw (lfo.fadeTime, 0, 127);
    lfo.depth1 = clampRaw (lfo.depth1, -63, 63);
    lfo.depth2 = clampRaw (lfo.depth2, -63, 63);
}

inline void clampToDocumentedRanges (TonePatch& tone) noexcept
{
    clampToDocumentedRanges (tone.osc1);
    clampToDocumentedRanges (tone.osc2);
    clampToDocumentedRanges (tone.lfo1);
    clampToDocumentedRanges (tone.lfo2);
    tone.pitchEnvAttack = clampRaw (tone.pitchEnvAttack, 0, 127);
    tone.pitchEnvDecay = clampRaw (tone.pitchEnvDecay, 0, 127);
    tone.balance = clampRaw (tone.balance, -63, 63);
    tone.cutoff = clampRaw (tone.cutoff, 0, 127);
    // [settled] FILTER Cutoff Keyfollow is raw 44-84 displayed -200..+200,
    // so the instrument has 41 positions in steps of 10 and this parameter
    // must have the same ones.
    tone.keyFollow =
        10 * clampRaw (static_cast<int> (std::lround (tone.keyFollow / 10.0)),
                       -20, 20);
    tone.cutoffVelocitySens = clampRaw (tone.cutoffVelocitySens, -63, 63);
    tone.resonance = clampRaw (tone.resonance, 0, 127);
    tone.filterEnvAttack = clampRaw (tone.filterEnvAttack, 0, 127);
    tone.filterEnvDecay = clampRaw (tone.filterEnvDecay, 0, 127);
    tone.filterEnvSustain = clampRaw (tone.filterEnvSustain, 0, 127);
    tone.filterEnvRelease = clampRaw (tone.filterEnvRelease, 0, 127);
    tone.filterEnvDepth = clampRaw (tone.filterEnvDepth, -63, 63);
    tone.drive = clampRaw (tone.drive, 0, 127);
    tone.level = clampRaw (tone.level, 0, 127);
    tone.levelVelocitySens = clampRaw (tone.levelVelocitySens, -63, 63);
    tone.pan = clampRaw (tone.pan, -64, 63);
    tone.ampEnvAttack = clampRaw (tone.ampEnvAttack, 0, 127);
    tone.ampEnvDecay = clampRaw (tone.ampEnvDecay, 0, 127);
    tone.ampEnvSustain = clampRaw (tone.ampEnvSustain, 0, 127);
    tone.ampEnvRelease = clampRaw (tone.ampEnvRelease, 0, 127);
    tone.delayDepth = clampRaw (tone.delayDepth, 0, 127);
    tone.reverbDepth = clampRaw (tone.reverbDepth, 0, 127);
    tone.bendRange = clampRaw (tone.bendRange, 0, 24);
    tone.octaveShift = clampRaw (tone.octaveShift, -3, 3);
    tone.portamentoTime = clampRaw (tone.portamentoTime, 0, 127);
}

inline void clampToDocumentedRanges (ExternalInput& input) noexcept
{
    input.inputVolume = clampRaw (input.inputVolume, 0, 127);
    input.cutoff = clampRaw (input.cutoff, 0, 127);
    input.resonance = clampRaw (input.resonance, 0, 127);
}

inline void clampToDocumentedRanges (Patch& patch) noexcept
{
    clampToDocumentedRanges (patch.upper);
    clampToDocumentedRanges (patch.lower);
    patch.patchLevel = clampRaw (patch.patchLevel, 0, 127);
    patch.toneBalance = clampRaw (patch.toneBalance, -63, 63);
    patch.tempo = clampRaw (patch.tempo, 5, 300);
    patch.splitPoint = clampRaw (patch.splitPoint, 21, 108);
    patch.delay.time = clampRaw (patch.delay.time, 0, 127);
    // [settled] Feedback is raw 0-98 displayed -98..+98 %, so the display
    // moves in steps of two and raw 49 is 0 %. A host automating it finer
    // than the instrument can store it would not round-trip through SysEx.
    patch.delay.feedback =
        2 * clampRaw (static_cast<int> (std::lround (patch.delay.feedback / 2.0)),
                      -49, 49);
    patch.delay.hfDamp = clampRaw (patch.delay.hfDamp, 0, 17);
    patch.delay.modulationRate = clampRaw (patch.delay.modulationRate, 0, 127);
    patch.delay.modulationDepth = clampRaw (patch.delay.modulationDepth, 0, 127);
    patch.reverb.time = clampRaw (patch.reverb.time, 0, 127);
    patch.reverb.preDelay = clampRaw (patch.reverb.preDelay, 0, 125);
    patch.reverb.size = clampRaw (patch.reverb.size, 0, 7);
    patch.reverb.highCut = clampRaw (patch.reverb.highCut, 0, 20);
    patch.reverb.density = clampRaw (patch.reverb.density, 0, 127);
    patch.reverb.diffusion = clampRaw (patch.reverb.diffusion, 0, 127);
    patch.reverb.lfDampFrequency = clampRaw (patch.reverb.lfDampFrequency, 0, 19);
    patch.reverb.lfDampGain = clampRaw (patch.reverb.lfDampGain, -36, 0);
    patch.reverb.hfDampFrequency = clampRaw (patch.reverb.hfDampFrequency, 0, 5);
    patch.reverb.hfDampGain = clampRaw (patch.reverb.hfDampGain, -36, 0);
    patch.arpeggio.octaveRange = clampRaw (patch.arpeggio.octaveRange, -3, 3);
    patch.arpeggio.accent = clampRaw (patch.arpeggio.accent, 0, 100);
    patch.arpeggio.velocity = clampRaw (patch.arpeggio.velocity, 0, 127);
    patch.arpeggio.style.endStep =
        clampRaw (patch.arpeggio.style.endStep, 1, arpeggioMaxSteps);
    patch.arpeggio.styleIndex = std::max (0, patch.arpeggio.styleIndex);
    for (auto& note : patch.arpeggio.style.originalNote)
        note = clampRaw (note, 0, 128);
    patch.arpeggio.endStep = clampRaw (patch.arpeggio.endStep, 0, arpeggioMaxSteps);
}

} // namespace septum
