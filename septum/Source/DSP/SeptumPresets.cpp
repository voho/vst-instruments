#include "SeptumPresets.h"

#include <algorithm>
#include <initializer_list>

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
    Patch superSawLead()
    {
        Patch patch = initPatch();
        patch.name = "SuperLead201";
        patch.upper.osc1.wave = Waveform::SuperSaw;
        patch.upper.osc1.pulseWidth = 86;
        patch.upper.osc2.wave = Waveform::SuperSaw;
        patch.upper.osc2.pulseWidth = 64;
        patch.upper.osc2.fine = 9;
        patch.upper.balance = 22;
        patch.upper.lowFreq = LowFreqMode::Boost;
        patch.upper.filterType = FilterType::Lpf;
        patch.upper.filterSlope = FilterSlope::Db24;
        patch.upper.cutoff = 104;
        patch.upper.resonance = 18;
        patch.upper.filterEnvDepth = 12;
        patch.upper.filterEnvAttack = 0;
        patch.upper.filterEnvDecay = 78;
        patch.upper.filterEnvSustain = 92;
        patch.upper.ampEnvAttack = 4;
        patch.upper.ampEnvRelease = 40;
        patch.upper.delayDepth = 46;
        patch.upper.reverbDepth = 30;
        patch.delayOn = true;
        patch.reverbOn = true;
        applyDelayTemplate (patch, 5);   // Mod Delay
        applyReverbTemplate (patch, 4);  // Hall 1
        return patch;
    }

    Patch trancePluck()
    {
        Patch patch = initPatch();
        patch.name = "Trance Pluck";
        patch.upper.osc1.wave = Waveform::SuperSaw;
        patch.upper.osc1.pulseWidth = 72;
        patch.upper.osc2.wave = Waveform::Saw;
        patch.upper.osc2.coarse = -12;
        patch.upper.balance = -20;
        patch.upper.filterSlope = FilterSlope::Db24;
        patch.upper.cutoff = 34;
        patch.upper.resonance = 24;
        patch.upper.filterEnvDepth = 46;
        patch.upper.filterEnvAttack = 0;
        patch.upper.filterEnvDecay = 62;
        patch.upper.filterEnvSustain = 0;
        patch.upper.filterEnvRelease = 44;
        patch.upper.ampEnvDecay = 88;
        patch.upper.ampEnvSustain = 46;
        patch.upper.ampEnvRelease = 34;
        patch.upper.delayDepth = 58;
        patch.upper.reverbDepth = 26;
        patch.delayOn = true;
        patch.reverbOn = true;
        applyDelayTemplate (patch, 2);   // Medium Delay
        applyReverbTemplate (patch, 6);  // Plate 1
        return patch;
    }

    Patch fbLead()
    {
        Patch patch = initPatch();
        patch.name = "FB Howl Lead";
        patch.upper.osc1.wave = Waveform::FbOsc;
        patch.upper.osc1.pulseWidth = 92;
        patch.upper.osc2.wave = Waveform::Saw;
        patch.upper.osc2.coarse = -12;
        patch.upper.balance = -34;
        patch.upper.filterSlope = FilterSlope::Db12;
        patch.upper.cutoff = 96;
        patch.upper.resonance = 30;
        patch.upper.mono = MonoMode::SoloLegato;
        patch.upper.portamento = true;
        patch.upper.portamentoTime = 48;
        patch.upper.overdrive = true;
        patch.upper.drive = 46;
        patch.upper.ampEnvRelease = 30;
        patch.upper.lfo2.rate = 84;
        patch.upper.delayDepth = 40;
        patch.delayOn = true;
        applyDelayTemplate (patch, 4);   // Analog Delay
        return patch;
    }

    Patch acidBass()
    {
        Patch patch = initPatch();
        patch.name = "Acid 201";
        patch.upper.osc1.wave = Waveform::Saw;
        patch.upper.osc2.wave = Waveform::Square;
        patch.upper.osc2.coarse = -12;
        patch.upper.balance = -18;
        patch.upper.filterSlope = FilterSlope::Db24;
        patch.upper.cutoff = 22;
        patch.upper.resonance = 104;
        patch.upper.filterEnvDepth = 40;
        patch.upper.filterEnvDecay = 56;
        patch.upper.filterEnvSustain = 6;
        patch.upper.cutoffVelocitySens = 34;
        patch.upper.ampEnvDecay = 80;
        patch.upper.ampEnvSustain = 84;
        patch.upper.ampEnvRelease = 8;
        patch.upper.overdrive = true;
        patch.upper.drive = 58;
        return patch;
    }

    Patch syncLead()
    {
        Patch patch = initPatch();
        patch.name = "Sync Sweeper";
        patch.upper.osc1.wave = Waveform::Saw;
        patch.upper.osc1.coarse = 7;
        patch.upper.osc1.pitchEnvDepth = 44;
        patch.upper.osc2.wave = Waveform::Saw;
        patch.upper.mixType = MixModType::Sync;
        patch.upper.balance = -63;   // sync output lives on the OSC1 leg
        patch.upper.pitchEnvAttack = 0;
        patch.upper.pitchEnvDecay = 92;
        patch.upper.filterSlope = FilterSlope::Db12;
        patch.upper.cutoff = 112;
        patch.upper.ampEnvRelease = 26;
        patch.upper.overdrive = true;
        patch.upper.drive = 30;
        patch.upper.reverbDepth = 24;
        patch.reverbOn = true;
        applyReverbTemplate (patch, 2);  // Studio 1
        return patch;
    }

    Patch ringBell()
    {
        Patch patch = initPatch();
        patch.name = "Ring Bell";
        patch.upper.osc1.wave = Waveform::Sine;
        patch.upper.osc1.pitchWide = true;
        patch.upper.osc1.coarse = 24;
        patch.upper.osc1.fine = 17;
        patch.upper.osc2.wave = Waveform::Sine;
        patch.upper.mixType = MixModType::Ring;
        patch.upper.balance = -63;   // the ring product alone
        patch.upper.filterType = FilterType::Bypass;
        patch.upper.ampEnvAttack = 0;
        patch.upper.ampEnvDecay = 96;
        patch.upper.ampEnvSustain = 0;
        patch.upper.ampEnvRelease = 96;
        patch.upper.levelVelocitySens = 40;
        patch.upper.delayDepth = 30;
        patch.upper.reverbDepth = 52;
        patch.delayOn = true;
        patch.reverbOn = true;
        applyDelayTemplate (patch, 0);   // Simple Delay
        applyReverbTemplate (patch, 5);  // Hall 2
        return patch;
    }

    Patch pwmStrings()
    {
        Patch patch = initPatch();
        patch.name = "PWM Strings";
        patch.upper.osc1.wave = Waveform::PulseSquare;
        patch.upper.osc1.pulseWidth = 40;
        patch.upper.osc2.wave = Waveform::PulseSquare;
        patch.upper.osc2.pulseWidth = 58;
        patch.upper.osc2.fine = -8;
        patch.upper.balance = 0;
        patch.upper.filterSlope = FilterSlope::Db12;
        patch.upper.cutoff = 88;
        patch.upper.keyFollow = 50;
        patch.upper.lfo1.shape = LfoShape::Tri;
        patch.upper.lfo1.rate = 52;
        patch.upper.lfo1.destination1 = LfoDest1::Pw1;
        patch.upper.lfo1.depth1 = 28;
        patch.upper.lfo1.destination2 = LfoDest2::Pw2;
        patch.upper.lfo1.depth2 = -24;
        patch.upper.ampEnvAttack = 62;
        patch.upper.ampEnvRelease = 70;
        patch.upper.delayDepth = 64;
        patch.upper.reverbDepth = 36;
        patch.delayOn = true;
        patch.reverbOn = true;
        applyDelayTemplate (patch, 6);   // Chorus 1
        applyReverbTemplate (patch, 4);  // Hall 1
        return patch;
    }

    Patch subBass()
    {
        Patch patch = initPatch();
        patch.name = "Sub Bass 201";
        patch.upper.osc1.wave = Waveform::Square;
        patch.upper.osc2.wave = Waveform::Sine;
        patch.upper.osc2.coarse = -12;
        patch.upper.balance = 14;
        patch.upper.lowFreq = LowFreqMode::Boost;
        patch.upper.filterSlope = FilterSlope::Db24;
        patch.upper.cutoff = 52;
        patch.upper.keyFollow = 100;
        patch.upper.ampEnvDecay = 90;
        patch.upper.ampEnvSustain = 100;
        patch.upper.ampEnvRelease = 12;
        patch.upper.levelVelocitySens = 24;
        return patch;
    }

    Patch sampleHoldFx()
    {
        Patch patch = initPatch();
        patch.name = "S&H Robot";
        patch.upper.osc1.wave = Waveform::Noise;
        patch.upper.osc2.wave = Waveform::Square;
        patch.upper.osc2.coarse = -5;
        patch.upper.balance = -26;
        patch.upper.filterType = FilterType::Bpf;
        patch.upper.filterSlope = FilterSlope::Db24;
        patch.upper.cutoff = 74;
        patch.upper.resonance = 96;
        patch.upper.lfo1.shape = LfoShape::SampleHold;
        patch.upper.lfo1.rate = 78;
        patch.upper.lfo1.destination1 = LfoDest1::Filter;
        patch.upper.lfo1.depth1 = 44;
        patch.upper.ampEnvSustain = 127;
        patch.upper.delayDepth = 40;
        patch.delayOn = true;
        applyDelayTemplate (patch, 1);   // 1 Shot Delay
        return patch;
    }

    Patch softPad()
    {
        Patch patch = initPatch();
        patch.name = "Alaska Dual";
        patch.keyboardMode = KeyboardMode::Dual;
        patch.upper.osc1.wave = Waveform::SuperSaw;
        patch.upper.osc1.pulseWidth = 48;
        patch.upper.osc2.wave = Waveform::Saw;
        patch.upper.osc2.fine = -11;
        patch.upper.balance = -12;
        patch.upper.cutoff = 66;
        patch.upper.filterSlope = FilterSlope::Db12;
        patch.upper.ampEnvAttack = 84;
        patch.upper.ampEnvRelease = 96;
        patch.upper.level = 84;
        patch.upper.reverbDepth = 60;
        patch.lower.osc1.wave = Waveform::PulseSquare;
        patch.lower.osc1.pulseWidth = 30;
        patch.lower.osc2.wave = Waveform::Sine;
        patch.lower.osc2.coarse = -12;
        patch.lower.balance = 8;
        patch.lower.cutoff = 48;
        patch.lower.filterSlope = FilterSlope::Db12;
        patch.lower.ampEnvAttack = 96;
        patch.lower.ampEnvRelease = 104;
        patch.lower.level = 74;
        patch.lower.reverbDepth = 64;
        patch.lower.lfo1.rate = 40;
        patch.lower.lfo1.destination1 = LfoDest1::Filter;
        patch.lower.lfo1.depth1 = 10;
        patch.reverbOn = true;
        applyReverbTemplate (patch, 5);  // Hall 2
        return patch;
    }

    Patch noiseSweep()
    {
        Patch patch = initPatch();
        patch.name = "Ion Wind";
        patch.upper.osc1.wave = Waveform::Noise;
        patch.upper.filterType = FilterType::Bpf;
        patch.upper.filterSlope = FilterSlope::Db24;
        patch.upper.cutoff = 60;
        patch.upper.resonance = 88;
        patch.upper.lfo1.shape = LfoShape::Tri;
        patch.upper.lfo1.rate = 18;
        patch.upper.lfo1.destination1 = LfoDest1::Filter;
        patch.upper.lfo1.depth1 = 40;
        patch.upper.ampEnvAttack = 70;
        patch.upper.ampEnvRelease = 100;
        patch.upper.reverbDepth = 70;
        patch.reverbOn = true;
        applyReverbTemplate (patch, 7);  // Plate 2
        return patch;
    }

    Patch splitStack()
    {
        Patch patch = initPatch();
        patch.name = "Club Split";
        patch.keyboardMode = KeyboardMode::Split;
        patch.splitPoint = 60;
        patch.lower.osc1.wave = Waveform::Saw;
        patch.lower.osc2.wave = Waveform::Square;
        patch.lower.osc2.coarse = -12;
        patch.lower.balance = -10;
        patch.lower.lowFreq = LowFreqMode::Boost;
        patch.lower.filterSlope = FilterSlope::Db24;
        patch.lower.cutoff = 40;
        patch.lower.filterEnvDepth = 28;
        patch.lower.filterEnvDecay = 60;
        patch.lower.filterEnvSustain = 10;
        patch.lower.ampEnvSustain = 96;
        patch.lower.ampEnvRelease = 10;
        patch.upper.osc1.wave = Waveform::SuperSaw;
        patch.upper.osc1.pulseWidth = 70;
        patch.upper.osc2.wave = Waveform::SuperSaw;
        patch.upper.osc2.pulseWidth = 52;
        patch.upper.osc2.fine = 6;
        patch.upper.balance = 10;
        patch.upper.cutoff = 92;
        patch.upper.ampEnvAttack = 10;
        patch.upper.ampEnvRelease = 44;
        patch.upper.delayDepth = 44;
        patch.upper.reverbDepth = 28;
        patch.delayOn = true;
        patch.reverbOn = true;
        applyDelayTemplate (patch, 2);   // Medium Delay
        applyReverbTemplate (patch, 3);  // Studio 2
        return patch;
    }
} // namespace

const std::vector<NamedPatch>& factoryPatches()
{
    static const std::vector<NamedPatch> bank = []
    {
        std::vector<NamedPatch> patches;
        patches.push_back ({ "INIT PATCH", initPatch() });
        patches.push_back ({ "SuperLead201", superSawLead() });
        patches.push_back ({ "Trance Pluck", trancePluck() });
        patches.push_back ({ "FB Howl Lead", fbLead() });
        patches.push_back ({ "Acid 201", acidBass() });
        patches.push_back ({ "Sync Sweeper", syncLead() });
        patches.push_back ({ "Ring Bell", ringBell() });
        patches.push_back ({ "PWM Strings", pwmStrings() });
        patches.push_back ({ "Sub Bass 201", subBass() });
        patches.push_back ({ "S&H Robot", sampleHoldFx() });
        patches.push_back ({ "Alaska Dual", softPad() });
        patches.push_back ({ "Ion Wind", noiseSweep() });
        patches.push_back ({ "Club Split", splitStack() });
        for (auto& entry : patches)
        {
            entry.patch.name = entry.name;
            clampToDocumentedRanges (entry.patch);
        }
        return patches;
    }();
    return bank;
}

// ---------------------------------------------------------------------------
// Arpeggio styles
//
// Written against the settled 32 x 16 grid: `cells[step][row]` is a rest, a
// tie, or a note-on carrying the style's programmed velocity. Roland's own 32
// templates are unpublished data and none of them ships here; these are this
// project's own patterns, laid out so the MOTIF, OCTAVE RANGE, ACCENT,
// DURATION and GRID controls all have something to act on.
// ---------------------------------------------------------------------------

namespace
{
    // A compact way to write a style: one string per note row, one character
    // per step. '-' rest, '~' tie, and a digit 1-9 a note-on at that tenth of
    // full velocity. Row 1 is the first string.
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
                    value = static_cast<signed char> ((*c - '0') * 127 / 9);
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

        // 1-4: the plain runs. One row per step, so the MOTIF decides
        // everything about which key each step lands on.
        list.push_back ({ "Straight 4", makeStyle ({ "9---", "-7--", "--7-", "---7" }) });
        list.push_back ({ "Straight 8", makeStyle ({ "9-------", "-7------", "--7-----",
                                                     "---7----", "----7---", "-----7--",
                                                     "------7-", "-------7" }) });
        // The manual's own worked example, "1-2-3-2".
        list.push_back ({ "Up Down 4", makeStyle ({ "9---", "-7-7", "--7-" }) });
        list.push_back ({ "Octave Run", makeStyle ({ "9-9-", "-7-7" }) });

        // 5-8: rhythmic patterns with rests and ties, where DURATION and the
        // shuffle grids show.
        list.push_back ({ "Sixteenth Pulse",
                          makeStyle ({ "9-9-9-9-9-9-9-9-" }) });
        list.push_back ({ "Off Beat",
                          makeStyle ({ "--9---9-", "-7---7--" }) });
        list.push_back ({ "Long Short",
                          makeStyle ({ "9~~-", "---7" }) });
        list.push_back ({ "Gallop",
                          makeStyle ({ "9-99-9--", "-7--7-7-" }) });

        // 9-12: chord styles — several rows sounding on the same step.
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

        // 13-16: phrases, meant for the PHRASE motif, where the rows read as
        // semitone steps above the key you play.
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
    // The template carries a length, but END STEP is a control of its own and
    // outranks it when the player has set one.
    if (patch.arpeggio.endStep > 0)
        patch.arpeggio.style.endStep =
            std::min (patch.arpeggio.endStep, arpeggioMaxSteps);
}

} // namespace septum
