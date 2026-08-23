#include "SeptumPresets.h"

#include <algorithm>
#include <initializer_list>
#include <string>

namespace septum
{
namespace
{
    struct DelayTemplate
    {
        const char* name;
        int time, feedback, hfDamp, modulationRate, modulationDepth;
    };

    struct ReverbTemplate
    {
        const char* name;
        int time, preDelay, size, highCut, density, diffusion;
        int lfDampFrequency, lfDampGain, hfDampFrequency, hfDampGain;
    };

    // Settled names; voiced values (OQ-12). Times are engine 0-127 values
    // through mapping::delaySeconds (1 ms - 1.3 s exponential): 64 = ~43 ms,
    // 96 = ~220 ms, 112 = ~500 ms, 120 = ~800 ms. The two chorus templates
    // are short modulated delays with no feedback, which is how the hardware
    // realizes chorus inside its modulation delay (settled).
    constexpr DelayTemplate delayTemplates[8] {
        { "Simple Delay", 100, 30, 17, 0, 0 },
        { "1 Shot Delay", 104, 0, 17, 0, 0 },
        { "Medium Delay", 108, 40, 15, 0, 0 },
        { "Long Delay", 118, 48, 14, 0, 0 },
        { "Analog Delay", 102, 52, 8, 0, 0 },
        { "Mod Delay", 106, 42, 13, 52, 36 },
        { "Chorus 1", 56, 0, 17, 44, 88 },
        { "Chorus 2", 62, 14, 16, 58, 110 },
    };

    constexpr ReverbTemplate reverbTemplates[8] {
        { "Room 1", 44, 8, 1, 15, 100, 90, 5, -3, 2, -9 },
        { "Room 2", 56, 12, 2, 14, 100, 96, 5, -2, 2, -6 },
        { "Studio 1", 50, 20, 3, 16, 110, 100, 6, -2, 3, -4 },
        { "Studio 2", 62, 25, 4, 16, 110, 104, 6, -2, 3, -3 },
        { "Hall 1", 84, 38, 6, 17, 118, 112, 7, -1, 3, -6 },
        { "Hall 2", 96, 50, 7, 17, 118, 116, 7, -1, 2, -8 },
        { "Plate 1", 74, 5, 4, 19, 127, 120, 9, -4, 4, -2 },
        { "Plate 2", 88, 10, 5, 19, 127, 124, 9, -4, 4, -1 },
    };
} // namespace

Patch initPatch()
{
    Patch patch;
    patch.name = "INIT PATCH";
    // Documented: after selecting INIT PATCH only OSC 1 is heard.
    patch.upper.balance = -63;
    patch.lower.balance = -63;
    clampToDocumentedRanges (patch);
    return patch;
}

void applyDelayTemplate (Patch& patch, int index)
{
    const auto& entry = delayTemplates[static_cast<std::size_t> (
        std::clamp (index, 0, 7))];
    patch.delay.time = entry.time;
    patch.delay.feedback = entry.feedback;
    patch.delay.hfDamp = entry.hfDamp;
    patch.delay.modulationRate = entry.modulationRate;
    patch.delay.modulationDepth = entry.modulationDepth;
}

void applyReverbTemplate (Patch& patch, int index)
{
    const auto& entry = reverbTemplates[static_cast<std::size_t> (
        std::clamp (index, 0, 7))];
    patch.reverb.time = entry.time;
    patch.reverb.preDelay = entry.preDelay;
    patch.reverb.size = entry.size;
    patch.reverb.highCut = entry.highCut;
    patch.reverb.density = entry.density;
    patch.reverb.diffusion = entry.diffusion;
    patch.reverb.lfDampFrequency = entry.lfDampFrequency;
    patch.reverb.lfDampGain = entry.lfDampGain;
    patch.reverb.hfDampFrequency = entry.hfDampFrequency;
    patch.reverb.hfDampGain = entry.hfDampGain;
}

const char* delayTemplateName (int index)
{
    return delayTemplates[static_cast<std::size_t> (std::clamp (index, 0, 7))].name;
}

const char* reverbTemplateName (int index)
{
    return reverbTemplates[static_cast<std::size_t> (std::clamp (index, 0, 7))].name;
}

namespace
{
    // =======================================================================
    // BANK A: LEADS & SOLO (A1 .. A8)
    // =======================================================================

    Patch presetA1_superSawLead()
    {
        Patch p = initPatch();
        p.name = "SuperLead201";
        p.upper.osc1.wave = Waveform::SuperSaw;
        p.upper.osc1.pulseWidth = 86;
        p.upper.osc2.wave = Waveform::SuperSaw;
        p.upper.osc2.pulseWidth = 64;
        p.upper.osc2.fine = 9;
        p.upper.balance = 22;
        p.upper.lowFreq = LowFreqMode::Boost;
        p.upper.filterType = FilterType::Lpf;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 104;
        p.upper.resonance = 18;
        p.upper.filterEnvDepth = 12;
        p.upper.filterEnvAttack = 0;
        p.upper.filterEnvDecay = 78;
        p.upper.filterEnvSustain = 92;
        p.upper.ampEnvAttack = 4;
        p.upper.ampEnvRelease = 40;
        p.upper.delayDepth = 46;
        p.upper.reverbDepth = 30;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 5);   // Mod Delay
        applyReverbTemplate (p, 4);  // Hall 1
        return p;
    }

    Patch presetA2_trancePluck()
    {
        Patch p = initPatch();
        p.name = "Trance Pluck";
        p.upper.osc1.wave = Waveform::SuperSaw;
        p.upper.osc1.pulseWidth = 72;
        p.upper.osc2.wave = Waveform::Saw;
        p.upper.osc2.coarse = -12;
        p.upper.balance = -20;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 34;
        p.upper.resonance = 24;
        p.upper.filterEnvDepth = 46;
        p.upper.filterEnvAttack = 0;
        p.upper.filterEnvDecay = 62;
        p.upper.filterEnvSustain = 0;
        p.upper.filterEnvRelease = 44;
        p.upper.ampEnvDecay = 88;
        p.upper.ampEnvSustain = 46;
        p.upper.ampEnvRelease = 34;
        p.upper.delayDepth = 58;
        p.upper.reverbDepth = 26;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 2);   // Medium Delay
        applyReverbTemplate (p, 6);  // Plate 1
        return p;
    }

    Patch presetA3_fbHowlLead()
    {
        Patch p = initPatch();
        p.name = "FB Howl Lead";
        p.upper.osc1.wave = Waveform::FbOsc;
        p.upper.osc1.pulseWidth = 92;
        p.upper.osc2.wave = Waveform::Saw;
        p.upper.osc2.coarse = -12;
        p.upper.balance = -34;
        p.upper.filterSlope = FilterSlope::Db12;
        p.upper.cutoff = 96;
        p.upper.resonance = 30;
        p.upper.mono = MonoMode::SoloLegato;
        p.upper.portamento = true;
        p.upper.portamentoTime = 48;
        p.upper.overdrive = true;
        p.upper.drive = 46;
        p.upper.ampEnvRelease = 30;
        p.upper.lfo2.rate = 84;
        p.upper.delayDepth = 40;
        p.delayOn = true;
        applyDelayTemplate (p, 4);   // Analog Delay
        return p;
    }

    Patch presetA4_syncSweeper()
    {
        Patch p = initPatch();
        p.name = "Sync Sweeper";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc1.coarse = 7;
        p.upper.osc1.pitchEnvDepth = 44;
        p.upper.osc2.wave = Waveform::Saw;
        p.upper.mixType = MixModType::Sync;
        p.upper.balance = -63;
        p.upper.pitchEnvAttack = 0;
        p.upper.pitchEnvDecay = 92;
        p.upper.filterSlope = FilterSlope::Db12;
        p.upper.cutoff = 112;
        p.upper.ampEnvRelease = 26;
        p.upper.overdrive = true;
        p.upper.drive = 30;
        p.upper.reverbDepth = 24;
        p.reverbOn = true;
        applyReverbTemplate (p, 2);  // Studio 1
        return p;
    }

    Patch presetA5_jupiterLead()
    {
        Patch p = initPatch();
        p.name = "Jupiter Lead";
        p.upper.osc1.wave = Waveform::PulseSquare;
        p.upper.osc1.pulseWidth = 50;
        p.upper.osc2.wave = Waveform::PulseSquare;
        p.upper.osc2.pulseWidth = 70;
        p.upper.osc2.fine = 7;
        p.upper.balance = 0;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 88;
        p.upper.resonance = 36;
        p.upper.filterEnvDepth = 20;
        p.upper.filterEnvDecay = 75;
        p.upper.filterEnvSustain = 70;
        p.upper.lfo1.rate = 60;
        p.upper.lfo1.destination1 = LfoDest1::Pw1;
        p.upper.lfo1.depth1 = 30;
        p.upper.delayDepth = 48;
        p.upper.reverbDepth = 32;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 6);   // Chorus 1
        applyReverbTemplate (p, 4);  // Hall 1
        return p;
    }

    Patch presetA6_sawReFi()
    {
        Patch p = initPatch();
        p.name = "Saw Re-Fi";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc2.wave = Waveform::Saw;
        p.upper.osc2.fine = -12;
        p.upper.balance = 0;
        p.upper.lowFreq = LowFreqMode::Boost;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 110;
        p.upper.resonance = 14;
        p.upper.ampEnvAttack = 0;
        p.upper.ampEnvDecay = 80;
        p.upper.ampEnvSustain = 110;
        p.upper.ampEnvRelease = 30;
        p.upper.overdrive = true;
        p.upper.drive = 38;
        p.upper.delayDepth = 35;
        p.delayOn = true;
        applyDelayTemplate (p, 0);   // Simple Delay
        return p;
    }

    Patch presetA7_fifthLead()
    {
        Patch p = initPatch();
        p.name = "5th Anthem";
        p.upper.osc1.wave = Waveform::SuperSaw;
        p.upper.osc1.pulseWidth = 78;
        p.upper.osc2.wave = Waveform::Saw;
        p.upper.osc2.coarse = 7; // 5th interval
        p.upper.osc2.fine = 5;
        p.upper.balance = -10;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 95;
        p.upper.resonance = 22;
        p.upper.filterEnvDepth = 18;
        p.upper.filterEnvDecay = 85;
        p.upper.filterEnvSustain = 90;
        p.upper.ampEnvRelease = 45;
        p.upper.delayDepth = 50;
        p.upper.reverbDepth = 40;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 5);   // Mod Delay
        applyReverbTemplate (p, 5);  // Hall 2
        return p;
    }

    Patch presetA8_soloHero()
    {
        Patch p = initPatch();
        p.name = "SoloScreamer";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc2.wave = Waveform::Square;
        p.upper.osc2.fine = 10;
        p.upper.balance = -15;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 85;
        p.upper.resonance = 48;
        p.upper.mono = MonoMode::SoloLegato;
        p.upper.portamento = true;
        p.upper.portamentoTime = 38;
        p.upper.overdrive = true;
        p.upper.drive = 65;
        p.upper.ampEnvRelease = 35;
        p.upper.delayDepth = 55;
        p.upper.reverbDepth = 45;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 2);   // Medium Delay
        applyReverbTemplate (p, 4);  // Hall 1
        return p;
    }

    // =======================================================================
    // BANK B: BASSES (B1 .. B8)
    // =======================================================================

    Patch presetB1_acid201()
    {
        Patch p = initPatch();
        p.name = "Acid 201";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc2.wave = Waveform::Square;
        p.upper.osc2.coarse = -12;
        p.upper.balance = -18;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 22;
        p.upper.resonance = 104;
        p.upper.filterEnvDepth = 40;
        p.upper.filterEnvDecay = 56;
        p.upper.filterEnvSustain = 6;
        p.upper.cutoffVelocitySens = 34;
        p.upper.ampEnvDecay = 80;
        p.upper.ampEnvSustain = 84;
        p.upper.ampEnvRelease = 8;
        p.upper.overdrive = true;
        p.upper.drive = 58;
        return p;
    }

    Patch presetB2_subBass201()
    {
        Patch p = initPatch();
        p.name = "Sub Bass 201";
        p.upper.osc1.wave = Waveform::Square;
        p.upper.osc2.wave = Waveform::Sine;
        p.upper.osc2.coarse = -12;
        p.upper.balance = 14;
        p.upper.lowFreq = LowFreqMode::Boost;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 52;
        p.upper.keyFollow = 100;
        p.upper.ampEnvDecay = 90;
        p.upper.ampEnvSustain = 100;
        p.upper.ampEnvRelease = 12;
        p.upper.levelVelocitySens = 24;
        return p;
    }

    Patch presetB3_pwmSlapBass()
    {
        Patch p = initPatch();
        p.name = "PWM SlapBass";
        p.upper.osc1.wave = Waveform::PulseSquare;
        p.upper.osc1.pulseWidth = 35;
        p.upper.osc2.wave = Waveform::Saw;
        p.upper.osc2.coarse = -12;
        p.upper.balance = -25;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 38;
        p.upper.resonance = 42;
        p.upper.filterEnvDepth = 48;
        p.upper.filterEnvAttack = 0;
        p.upper.filterEnvDecay = 50;
        p.upper.filterEnvSustain = 10;
        p.upper.ampEnvDecay = 65;
        p.upper.ampEnvSustain = 75;
        p.upper.ampEnvRelease = 15;
        p.upper.cutoffVelocitySens = 38;
        p.upper.overdrive = true;
        p.upper.drive = 25;
        return p;
    }

    Patch presetB4_overdriveBass()
    {
        Patch p = initPatch();
        p.name = "Grit Bass";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc2.wave = Waveform::Square;
        p.upper.osc2.fine = 6;
        p.upper.balance = 0;
        p.upper.lowFreq = LowFreqMode::Boost;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 44;
        p.upper.resonance = 30;
        p.upper.filterEnvDepth = 32;
        p.upper.filterEnvDecay = 60;
        p.upper.overdrive = true;
        p.upper.drive = 78;
        p.upper.ampEnvRelease = 18;
        return p;
    }

    Patch presetB5_solidPunch()
    {
        Patch p = initPatch();
        p.name = "Solid Punch";
        p.upper.osc1.wave = Waveform::Triangle;
        p.upper.osc1.pitchEnvDepth = 28;
        p.upper.osc2.wave = Waveform::Square;
        p.upper.osc2.coarse = -12;
        p.upper.balance = -10;
        p.upper.pitchEnvAttack = 0;
        p.upper.pitchEnvDecay = 24;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 48;
        p.upper.resonance = 20;
        p.upper.filterEnvDepth = 25;
        p.upper.filterEnvDecay = 45;
        p.upper.filterEnvSustain = 20;
        p.upper.ampEnvDecay = 70;
        p.upper.ampEnvRelease = 10;
        return p;
    }

    Patch presetB6_resoStepBass()
    {
        Patch p = initPatch();
        p.name = "Reso Squawk";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc2.wave = Waveform::PulseSquare;
        p.upper.osc2.pulseWidth = 60;
        p.upper.balance = -20;
        p.upper.filterSlope = FilterSlope::Db12;
        p.upper.cutoff = 28;
        p.upper.resonance = 85;
        p.upper.filterEnvDepth = 52;
        p.upper.filterEnvDecay = 48;
        p.upper.filterEnvSustain = 0;
        p.upper.cutoffVelocitySens = 45;
        p.upper.ampEnvDecay = 60;
        p.upper.ampEnvRelease = 12;
        return p;
    }

    Patch presetB7_dubbySub()
    {
        Patch p = initPatch();
        p.name = "Dub Sub 201";
        p.upper.osc1.wave = Waveform::Sine;
        p.upper.osc2.wave = Waveform::Triangle;
        p.upper.osc2.coarse = -12;
        p.upper.balance = 20;
        p.upper.lowFreq = LowFreqMode::Boost;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 30;
        p.upper.ampEnvAttack = 8;
        p.upper.ampEnvDecay = 95;
        p.upper.ampEnvSustain = 110;
        p.upper.ampEnvRelease = 25;
        p.upper.delayDepth = 38;
        p.delayOn = true;
        applyDelayTemplate (p, 4); // Analog Delay
        return p;
    }

    Patch presetB8_fbHeavyBass()
    {
        Patch p = initPatch();
        p.name = "FB GrowlBass";
        p.upper.osc1.wave = Waveform::FbOsc;
        p.upper.osc1.pulseWidth = 75;
        p.upper.osc2.wave = Waveform::Saw;
        p.upper.osc2.coarse = -12;
        p.upper.balance = -30;
        p.upper.lowFreq = LowFreqMode::Boost;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 50;
        p.upper.resonance = 55;
        p.upper.filterEnvDepth = 35;
        p.upper.filterEnvDecay = 55;
        p.upper.overdrive = true;
        p.upper.drive = 45;
        p.upper.ampEnvRelease = 16;
        return p;
    }

    // =======================================================================
    // BANK C: PADS, STRINGS & KEYS (C1 .. C8)
    // =======================================================================

    Patch presetC1_alaskaDual()
    {
        Patch p = initPatch();
        p.name = "Alaska Dual";
        p.keyboardMode = KeyboardMode::Dual;
        p.upper.osc1.wave = Waveform::SuperSaw;
        p.upper.osc1.pulseWidth = 48;
        p.upper.osc2.wave = Waveform::Saw;
        p.upper.osc2.fine = -11;
        p.upper.balance = -12;
        p.upper.cutoff = 66;
        p.upper.filterSlope = FilterSlope::Db12;
        p.upper.ampEnvAttack = 84;
        p.upper.ampEnvRelease = 96;
        p.upper.level = 84;
        p.upper.reverbDepth = 60;
        p.lower.osc1.wave = Waveform::PulseSquare;
        p.lower.osc1.pulseWidth = 30;
        p.lower.osc2.wave = Waveform::Sine;
        p.lower.osc2.coarse = -12;
        p.lower.balance = 8;
        p.lower.cutoff = 48;
        p.lower.filterSlope = FilterSlope::Db12;
        p.lower.ampEnvAttack = 96;
        p.lower.ampEnvRelease = 104;
        p.lower.level = 74;
        p.lower.reverbDepth = 64;
        p.lower.lfo1.rate = 40;
        p.lower.lfo1.destination1 = LfoDest1::Filter;
        p.lower.lfo1.depth1 = 10;
        p.reverbOn = true;
        applyReverbTemplate (p, 5);  // Hall 2
        return p;
    }

    Patch presetC2_pwmStrings()
    {
        Patch p = initPatch();
        p.name = "PWM Strings";
        p.upper.osc1.wave = Waveform::PulseSquare;
        p.upper.osc1.pulseWidth = 40;
        p.upper.osc2.wave = Waveform::PulseSquare;
        p.upper.osc2.pulseWidth = 58;
        p.upper.osc2.fine = -8;
        p.upper.balance = 0;
        p.upper.filterSlope = FilterSlope::Db12;
        p.upper.cutoff = 88;
        p.upper.keyFollow = 50;
        p.upper.lfo1.shape = LfoShape::Tri;
        p.upper.lfo1.rate = 52;
        p.upper.lfo1.destination1 = LfoDest1::Pw1;
        p.upper.lfo1.depth1 = 28;
        p.upper.lfo1.destination2 = LfoDest2::Pw2;
        p.upper.lfo1.depth2 = -24;
        p.upper.ampEnvAttack = 62;
        p.upper.ampEnvRelease = 70;
        p.upper.delayDepth = 64;
        p.upper.reverbDepth = 36;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 6);   // Chorus 1
        applyReverbTemplate (p, 4);  // Hall 1
        return p;
    }

    Patch presetC3_superSawPad()
    {
        Patch p = initPatch();
        p.name = "SuperSaw Pad";
        p.upper.osc1.wave = Waveform::SuperSaw;
        p.upper.osc1.pulseWidth = 65;
        p.upper.osc2.wave = Waveform::SuperSaw;
        p.upper.osc2.pulseWidth = 45;
        p.upper.osc2.fine = 8;
        p.upper.balance = 10;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 62;
        p.upper.resonance = 16;
        p.upper.filterEnvDepth = 22;
        p.upper.filterEnvAttack = 75;
        p.upper.filterEnvDecay = 90;
        p.upper.filterEnvSustain = 80;
        p.upper.ampEnvAttack = 70;
        p.upper.ampEnvRelease = 85;
        p.upper.delayDepth = 48;
        p.upper.reverbDepth = 65;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 7);   // Chorus 2
        applyReverbTemplate (p, 5);  // Hall 2
        return p;
    }

    Patch presetC4_spaceChoir()
    {
        Patch p = initPatch();
        p.name = "Space Choir";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc2.wave = Waveform::PulseSquare;
        p.upper.osc2.pulseWidth = 55;
        p.upper.osc2.fine = -6;
        p.upper.balance = 0;
        p.upper.filterType = FilterType::Bpf;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 58;
        p.upper.resonance = 60;
        p.upper.lfo1.shape = LfoShape::Sin;
        p.upper.lfo1.rate = 45;
        p.upper.lfo1.destination1 = LfoDest1::Filter;
        p.upper.lfo1.depth1 = 16;
        p.upper.ampEnvAttack = 80;
        p.upper.ampEnvRelease = 90;
        p.upper.reverbDepth = 75;
        p.reverbOn = true;
        applyReverbTemplate (p, 4);  // Hall 1
        return p;
    }

    Patch presetC5_ringBell()
    {
        Patch p = initPatch();
        p.name = "Glass Bell";
        p.upper.osc1.wave = Waveform::Sine;
        p.upper.osc1.pitchWide = true;
        p.upper.osc1.coarse = 24;
        p.upper.osc1.fine = 17;
        p.upper.osc2.wave = Waveform::Sine;
        p.upper.mixType = MixModType::Ring;
        p.upper.balance = -63;
        p.upper.filterType = FilterType::Bypass;
        p.upper.ampEnvAttack = 0;
        p.upper.ampEnvDecay = 96;
        p.upper.ampEnvSustain = 0;
        p.upper.ampEnvRelease = 96;
        p.upper.levelVelocitySens = 40;
        p.upper.delayDepth = 30;
        p.upper.reverbDepth = 52;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 0);   // Simple Delay
        applyReverbTemplate (p, 5);  // Hall 2
        return p;
    }

    Patch presetC6_airPad()
    {
        Patch p = initPatch();
        p.name = "Air Ethereal";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc2.wave = Waveform::Noise;
        p.upper.balance = -35;
        p.upper.filterType = FilterType::Hpf;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 40;
        p.upper.resonance = 35;
        p.upper.ampEnvAttack = 90;
        p.upper.ampEnvRelease = 100;
        p.upper.reverbDepth = 80;
        p.reverbOn = true;
        applyReverbTemplate (p, 7);  // Plate 2
        return p;
    }

    Patch presetC7_retroBrass()
    {
        Patch p = initPatch();
        p.name = "Retro Brass";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc2.wave = Waveform::Saw;
        p.upper.osc2.fine = 8;
        p.upper.balance = 0;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 55;
        p.upper.resonance = 25;
        p.upper.filterEnvDepth = 35;
        p.upper.filterEnvAttack = 30;
        p.upper.filterEnvDecay = 75;
        p.upper.filterEnvSustain = 65;
        p.upper.ampEnvAttack = 25;
        p.upper.ampEnvRelease = 40;
        p.upper.reverbDepth = 35;
        p.reverbOn = true;
        applyReverbTemplate (p, 1);  // Room 2
        return p;
    }

    Patch presetC8_sweepPad()
    {
        Patch p = initPatch();
        p.name = "Sweep Aurora";
        p.upper.osc1.wave = Waveform::SuperSaw;
        p.upper.osc1.pulseWidth = 55;
        p.upper.osc2.wave = Waveform::Triangle;
        p.upper.osc2.coarse = -12;
        p.upper.balance = -15;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 45;
        p.upper.resonance = 55;
        p.upper.lfo1.shape = LfoShape::Tri;
        p.upper.lfo1.rate = 22;
        p.upper.lfo1.destination1 = LfoDest1::Filter;
        p.upper.lfo1.depth1 = 32;
        p.upper.ampEnvAttack = 85;
        p.upper.ampEnvRelease = 95;
        p.upper.delayDepth = 45;
        p.upper.reverbDepth = 60;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 5);   // Mod Delay
        applyReverbTemplate (p, 4);  // Hall 1
        return p;
    }

    // =======================================================================
    // BANK D: SEQUENCES, ARPS & FX (D1 .. D8)
    // =======================================================================

    Patch presetD1_clubSplit()
    {
        Patch p = initPatch();
        p.name = "Club Split";
        p.keyboardMode = KeyboardMode::Split;
        p.splitPoint = 60;
        p.lower.osc1.wave = Waveform::Saw;
        p.lower.osc2.wave = Waveform::Square;
        p.lower.osc2.coarse = -12;
        p.lower.balance = -10;
        p.lower.lowFreq = LowFreqMode::Boost;
        p.lower.filterSlope = FilterSlope::Db24;
        p.lower.cutoff = 40;
        p.lower.filterEnvDepth = 28;
        p.lower.filterEnvDecay = 60;
        p.lower.filterEnvSustain = 10;
        p.lower.ampEnvSustain = 96;
        p.lower.ampEnvRelease = 10;
        p.upper.osc1.wave = Waveform::SuperSaw;
        p.upper.osc1.pulseWidth = 70;
        p.upper.osc2.wave = Waveform::SuperSaw;
        p.upper.osc2.pulseWidth = 52;
        p.upper.osc2.fine = 6;
        p.upper.balance = 10;
        p.upper.cutoff = 92;
        p.upper.ampEnvAttack = 10;
        p.upper.ampEnvRelease = 44;
        p.upper.delayDepth = 44;
        p.upper.reverbDepth = 28;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 2);   // Medium Delay
        applyReverbTemplate (p, 3);  // Studio 2
        return p;
    }

    Patch presetD2_sampleHoldRobot()
    {
        Patch p = initPatch();
        p.name = "S&H Robot";
        p.upper.osc1.wave = Waveform::Noise;
        p.upper.osc2.wave = Waveform::Square;
        p.upper.osc2.coarse = -5;
        p.upper.balance = -26;
        p.upper.filterType = FilterType::Bpf;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 74;
        p.upper.resonance = 96;
        p.upper.lfo1.shape = LfoShape::SampleHold;
        p.upper.lfo1.rate = 78;
        p.upper.lfo1.destination1 = LfoDest1::Filter;
        p.upper.lfo1.depth1 = 44;
        p.upper.ampEnvSustain = 127;
        p.upper.delayDepth = 40;
        p.delayOn = true;
        applyDelayTemplate (p, 1);   // 1 Shot Delay
        return p;
    }

    Patch presetD3_ionWind()
    {
        Patch p = initPatch();
        p.name = "Ion Wind";
        p.upper.osc1.wave = Waveform::Noise;
        p.upper.filterType = FilterType::Bpf;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 60;
        p.upper.resonance = 88;
        p.upper.lfo1.shape = LfoShape::Tri;
        p.upper.lfo1.rate = 18;
        p.upper.lfo1.destination1 = LfoDest1::Filter;
        p.upper.lfo1.depth1 = 40;
        p.upper.ampEnvAttack = 70;
        p.upper.ampEnvRelease = 100;
        p.upper.reverbDepth = 70;
        p.reverbOn = true;
        applyReverbTemplate (p, 7);  // Plate 2
        return p;
    }

    Patch presetD4_arpChime()
    {
        Patch p = initPatch();
        p.name = "Arp Chime";
        p.upper.osc1.wave = Waveform::Triangle;
        p.upper.osc2.wave = Waveform::Sine;
        p.upper.osc2.coarse = 12;
        p.upper.balance = 0;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 85;
        p.upper.resonance = 30;
        p.upper.ampEnvAttack = 0;
        p.upper.ampEnvDecay = 65;
        p.upper.ampEnvSustain = 20;
        p.upper.ampEnvRelease = 40;
        applyArpeggioStyle (p, 0);
        p.arpeggio.on = false;
        p.upper.delayDepth = 50;
        p.upper.reverbDepth = 40;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 0);   // Simple Delay
        applyReverbTemplate (p, 4);  // Hall 1
        return p;
    }

    Patch presetD5_technoPulse()
    {
        Patch p = initPatch();
        p.name = "Techno Pulse";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc2.wave = Waveform::PulseSquare;
        p.upper.osc2.pulseWidth = 80;
        p.upper.balance = -10;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 40;
        p.upper.resonance = 70;
        p.upper.filterEnvDepth = 45;
        p.upper.filterEnvDecay = 45;
        p.upper.filterEnvSustain = 0;
        applyArpeggioStyle (p, 4);
        p.arpeggio.on = false;
        p.upper.overdrive = true;
        p.upper.drive = 40;
        p.upper.delayDepth = 35;
        p.delayOn = true;
        applyDelayTemplate (p, 2);   // Medium Delay
        return p;
    }

    Patch presetD6_alienSignal()
    {
        Patch p = initPatch();
        p.name = "Alien Signal";
        p.upper.osc1.wave = Waveform::Sine;
        p.upper.osc1.coarse = 19;
        p.upper.osc2.wave = Waveform::Triangle;
        p.upper.mixType = MixModType::Ring;
        p.upper.balance = -63;
        p.upper.filterType = FilterType::Bpf;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 80;
        p.upper.resonance = 90;
        p.upper.lfo1.shape = LfoShape::SampleHold;
        p.upper.lfo1.rate = 95;
        p.upper.lfo1.destination1 = LfoDest1::Filter;
        p.upper.lfo1.depth1 = 50;
        p.upper.delayDepth = 60;
        p.upper.reverbDepth = 50;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 5);   // Mod Delay
        applyReverbTemplate (p, 5);  // Hall 2
        return p;
    }

    Patch presetD7_laserSweep()
    {
        Patch p = initPatch();
        p.name = "Laser Sweep";
        p.upper.osc1.wave = Waveform::Saw;
        p.upper.osc1.pitchEnvDepth = 63;
        p.upper.osc2.wave = Waveform::Noise;
        p.upper.balance = -30;
        p.upper.pitchEnvAttack = 0;
        p.upper.pitchEnvDecay = 40;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 75;
        p.upper.resonance = 65;
        p.upper.filterEnvDepth = 50;
        p.upper.filterEnvDecay = 40;
        p.upper.ampEnvDecay = 50;
        p.upper.ampEnvRelease = 30;
        p.upper.delayDepth = 45;
        p.delayOn = true;
        applyDelayTemplate (p, 3); // Long Delay
        return p;
    }

    Patch presetD8_spaceDrone()
    {
        Patch p = initPatch();
        p.name = "Space Drone";
        p.upper.osc1.wave = Waveform::FbOsc;
        p.upper.osc1.pulseWidth = 85;
        p.upper.osc2.wave = Waveform::SuperSaw;
        p.upper.osc2.pulseWidth = 60;
        p.upper.osc2.fine = -15;
        p.upper.balance = 0;
        p.upper.filterSlope = FilterSlope::Db24;
        p.upper.cutoff = 50;
        p.upper.resonance = 75;
        p.upper.lfo1.shape = LfoShape::Sin;
        p.upper.lfo1.rate = 15;
        p.upper.lfo1.destination1 = LfoDest1::Filter;
        p.upper.lfo1.depth1 = 25;
        p.upper.lfo2.shape = LfoShape::Tri;
        p.upper.lfo2.rate = 28;
        p.upper.lfo2.destination1 = LfoDest1::Pw1;
        p.upper.lfo2.depth1 = 30;
        p.upper.ampEnvAttack = 95;
        p.upper.ampEnvRelease = 110;
        p.upper.delayDepth = 65;
        p.upper.reverbDepth = 80;
        p.delayOn = true;
        p.reverbOn = true;
        applyDelayTemplate (p, 5);   // Mod Delay
        applyReverbTemplate (p, 5);  // Hall 2
        return p;
    }
} // namespace

const std::vector<NamedPatch>& factoryPatches()
{
    static const std::vector<NamedPatch> bank = []
    {
        std::vector<NamedPatch> patches;
        patches.reserve (64);

        // --- PRESET BANK A: Leads (A1..A8) ---
        patches.push_back ({ "A-1: SuperLead201", presetA1_superSawLead() });
        patches.push_back ({ "A-2: Trance Pluck", presetA2_trancePluck() });
        patches.push_back ({ "A-3: FB Howl Lead", presetA3_fbHowlLead() });
        patches.push_back ({ "A-4: Sync Sweeper", presetA4_syncSweeper() });
        patches.push_back ({ "A-5: Jupiter Lead", presetA5_jupiterLead() });
        patches.push_back ({ "A-6: Saw Re-Fi",    presetA6_sawReFi() });
        patches.push_back ({ "A-7: 5th Anthem",   presetA7_fifthLead() });
        patches.push_back ({ "A-8: Solo Screamer",presetA8_soloHero() });

        // --- PRESET BANK B: Basses (B1..B8) ---
        patches.push_back ({ "B-1: Acid 201",     presetB1_acid201() });
        patches.push_back ({ "B-2: Sub Bass 201", presetB2_subBass201() });
        patches.push_back ({ "B-3: PWM SlapBass", presetB3_pwmSlapBass() });
        patches.push_back ({ "B-4: Grit Bass",    presetB4_overdriveBass() });
        patches.push_back ({ "B-5: Solid Punch",  presetB5_solidPunch() });
        patches.push_back ({ "B-6: Reso Squawk",  presetB6_resoStepBass() });
        patches.push_back ({ "B-7: Dub Sub 201",  presetB7_dubbySub() });
        patches.push_back ({ "B-8: FB GrowlBass", presetB8_fbHeavyBass() });

        // --- PRESET BANK C: Pads & Strings (C1..C8) ---
        patches.push_back ({ "C-1: Alaska Dual",  presetC1_alaskaDual() });
        patches.push_back ({ "C-2: PWM Strings",  presetC2_pwmStrings() });
        patches.push_back ({ "C-3: SuperSaw Pad", presetC3_superSawPad() });
        patches.push_back ({ "C-4: Space Choir",  presetC4_spaceChoir() });
        patches.push_back ({ "C-5: Glass Bell",   presetC5_ringBell() });
        patches.push_back ({ "C-6: Air Ethereal", presetC6_airPad() });
        patches.push_back ({ "C-7: Retro Brass",  presetC7_retroBrass() });
        patches.push_back ({ "C-8: Sweep Aurora", presetC8_sweepPad() });

        // --- PRESET BANK D: Sequences & FX (D1..D8) ---
        patches.push_back ({ "D-1: Club Split",   presetD1_clubSplit() });
        patches.push_back ({ "D-2: S&H Robot",    presetD2_sampleHoldRobot() });
        patches.push_back ({ "D-3: Ion Wind",     presetD3_ionWind() });
        patches.push_back ({ "D-4: Arp Chime",    presetD4_arpChime() });
        patches.push_back ({ "D-5: Techno Pulse", presetD5_technoPulse() });
        patches.push_back ({ "D-6: Alien Signal", presetD6_alienSignal() });
        patches.push_back ({ "D-7: Laser Sweep",  presetD7_laserSweep() });
        patches.push_back ({ "D-8: Space Drone",  presetD8_spaceDrone() });

        // --- USER BANKS (User A1..D8) ---
        const char* const userBankNames[4] = { "A", "B", "C", "D" };
        for (int bankIdx = 0; bankIdx < 4; ++bankIdx)
        {
            for (int patchIdx = 1; patchIdx <= 8; ++patchIdx)
            {
                std::string fullName = "User " + std::string (userBankNames[bankIdx])
                                       + "-" + std::to_string (patchIdx);
                Patch userPatch = initPatch();
                userPatch.name = fullName;
                patches.push_back ({ fullName, userPatch });
            }
        }

        for (std::size_t i = 0; i < patches.size(); ++i)
        {
            clampToDocumentedRanges (patches[i].patch);
        }
        return patches;
    }();
    return bank;
}

// ---------------------------------------------------------------------------
// Arpeggio styles
// ---------------------------------------------------------------------------

namespace
{
    ArpeggioStyle makeStyle (std::initializer_list<const char*> rows)
    {
        ArpeggioStyle style;
        int longest = 1;
        int row = 0;
        for (const char* text : rows)
        {
            if (row >= arpeggioMaxRows)
                break;
            int step = 0;
            for (const char* c = text; *c != 0 && step < arpeggioMaxSteps; ++c, ++step)
            {
                signed char value = arpeggioRest;
                if (*c == '~')
                    value = arpeggioTie;
                else if (*c >= '1' && *c <= '9')
                    // Capped at the highest velocity a cell can carry through
                // SysEx: 127 is the tie's byte.
                value = static_cast<signed char> (
                    std::min ((*c - '0') * 127 / 9, arpeggioMaxCellVelocity));
                style.cells[static_cast<std::size_t> (step)]
                           [static_cast<std::size_t> (row)] = value;
            }
            longest = std::max (longest, step);
            ++row;
        }
        style.endStep = std::clamp (longest, 1, arpeggioMaxSteps);
        return style;
    }
} // namespace

const std::vector<NamedArpeggioStyle>& arpeggioStyles()
{
    static const std::vector<NamedArpeggioStyle> styles = []
    {
        std::vector<NamedArpeggioStyle> list;

        list.push_back ({ "Straight 4", makeStyle ({ "9---", "-7--", "--7-", "---7" }) });
        list.push_back ({ "Straight 8", makeStyle ({ "9-------", "-7------", "--7-----",
                                                     "---7----", "----7---", "-----7--",
                                                     "------7-", "-------7" }) });
        list.push_back ({ "Up Down 4", makeStyle ({ "9---", "-7-7", "--7-" }) });
        list.push_back ({ "Octave Run", makeStyle ({ "9-9-", "-7-7" }) });

        list.push_back ({ "Sixteenth Pulse",
                          makeStyle ({ "9-9-9-9-9-9-9-9-" }) });
        list.push_back ({ "Off Beat",
                          makeStyle ({ "--9---9-", "-7---7--" }) });
        list.push_back ({ "Long Short",
                          makeStyle ({ "9~~-", "---7" }) });
        list.push_back ({ "Gallop",
                          makeStyle ({ "9-99-9--", "-7--7-7-" }) });

        list.push_back ({ "Chord Stab",
                          makeStyle ({ "9---9---", "9---9---", "9---9---",
                                       "9---9---" }) });
        list.push_back ({ "Chord Roll",
                          makeStyle ({ "9~~~~~~~", "-8~~~~~~", "--7~~~~~",
                                       "---6~~~~" }) });
        list.push_back ({ "Pad Swell",
                          makeStyle ({ "5~~~~~~~~~~~~~~~", "5~~~~~~~~~~~~~~~",
                                       "5~~~~~~~~~~~~~~~", "5~~~~~~~~~~~~~~~" }) });
        list.push_back ({ "Bass And Top",
                          makeStyle ({ "9-------", "----7---", "--------",
                                       "------6-" }) });

        list.push_back ({ "Phrase Minor",
                          makeStyle ({ "9-------", "--------", "--------",
                                       "-7------", "--------", "--7-----",
                                       "--------", "---7----", "--------",
                                       "--------", "----6---" }) });
        list.push_back ({ "Phrase Fifths",
                          makeStyle ({ "9---9---", "--------", "--------",
                                       "--------", "--------", "--------",
                                       "--------", "--7---7-" }) });
        list.push_back ({ "Phrase Octave",
                          makeStyle ({ "9-9-----", "--------", "--------",
                                       "--------", "--------", "--------",
                                       "--------", "--------", "--------",
                                       "--------", "--------", "--------",
                                       "----7-7-" }) });
        list.push_back ({ "Phrase Riff",
                          makeStyle ({ "9---9---", "--------", "--------",
                                       "--7-----", "--------", "------7-",
                                       "--------", "----6---" }) });
        return list;
    }();
    return styles;
}

const char* arpeggioStyleName (int index)
{
    const auto& styles = arpeggioStyles();
    if (index < 0 || index >= (int) styles.size())
        return "";
    return styles[(std::size_t) index].name;
}

void applyArpeggioStyle (Patch& patch, int index)
{
    const auto& styles = arpeggioStyles();
    if (index < 0 || index >= (int) styles.size())
        return;
    patch.arpeggio.style = styles[(std::size_t) index].style;
    if (patch.arpeggio.endStep > 0)
        patch.arpeggio.style.endStep =
            std::min (patch.arpeggio.endStep, arpeggioMaxSteps);
}

} // namespace septum
