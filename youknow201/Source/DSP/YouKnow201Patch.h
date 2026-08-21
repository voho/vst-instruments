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
#include <string>

namespace youknow201
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

    TonePatch upper {};
    TonePatch lower {};
    DelayParams delay {};
    ReverbParams reverb {};
};

// --------------------------------------------------------------------------
// Settled value tables, quoted from the MIDI Implementation.
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
    const int coarseLimit = osc.pitchWide ? 36 : 12;
    osc.coarse = clampRaw (osc.coarse, -coarseLimit, coarseLimit);
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
    tone.keyFollow = clampRaw (tone.keyFollow, -200, 200);
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

inline void clampToDocumentedRanges (Patch& patch) noexcept
{
    clampToDocumentedRanges (patch.upper);
    clampToDocumentedRanges (patch.lower);
    patch.patchLevel = clampRaw (patch.patchLevel, 0, 127);
    patch.toneBalance = clampRaw (patch.toneBalance, -63, 63);
    patch.tempo = clampRaw (patch.tempo, 5, 300);
    patch.splitPoint = clampRaw (patch.splitPoint, 21, 108);
    patch.delay.time = clampRaw (patch.delay.time, 0, 127);
    patch.delay.feedback = clampRaw (patch.delay.feedback, -98, 98);
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
}

} // namespace youknow201
