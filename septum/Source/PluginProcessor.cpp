#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
using septum::Patch;
using septum::TonePatch;

// ---------------------------------------------------------------------------
// One binding per parameter: the same list drives layout creation, patch
// snapshots and program loading, so the three can never disagree.
// ---------------------------------------------------------------------------

enum class Kind { Int, Bool, Choice };

struct ToneBinding
{
    const char* suffix;
    const char* label;
    Kind kind;
    int low, high;           // Int range, or choice count in `high`
    const juce::StringArray* choices;
    float (*get) (const TonePatch&);
    void (*set) (TonePatch&, float);
};

const juce::StringArray waveChoices {
    "SAW", "SQU", "PW-SQU", "TRI", "SINE", "NOISE", "FB-OSC", "SUPER-SAW",
    "EXT-IN"
};
const juce::StringArray mixTypeChoices { "MIX", "SYNC", "RING" };
const juce::StringArray lowFreqChoices { "FLAT", "BOOST", "CUT" };
const juce::StringArray filterTypeChoices { "BYPASS", "LPF", "HPF", "BPF" };
const juce::StringArray slopeChoices { "-12 dB", "-24 dB" };
const juce::StringArray lfoShapeChoices { "TRI", "SIN", "SAW", "SQR", "TRP",
                                          "S&H", "RND" };
const juce::StringArray lfoDest1Choices { "PITCH1", "PW1", "FILTER", "AUDIO-F" };
const juce::StringArray lfoDest2Choices { "PITCH2", "PW2", "AMP" };
const juce::StringArray monoChoices { "POLY", "SOLO+LEGATO", "SOLO" };
const juce::StringArray syncNoteChoices {
    "16", "12", "8", "4", "2", "1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4",
    "3/16", "1/6", "1/8", "3/32", "1/12", "1/16", "1/24", "1/32"
};
const juce::StringArray arpGridChoices { "1/4", "1/8", "1/8L", "1/8H", "1/12",
                                         "1/16", "1/16L", "1/16H", "1/24" };
const juce::StringArray arpDurationChoices { "30%", "40%", "50%", "60%", "70%",
                                             "80%", "90%", "100%", "120%", "FUL" };
const juce::StringArray arpMotifChoices {
    "UP(L)", "UP(L&H)", "UP(-)", "DOWN(L)", "DOWN(L&H)", "DOWN(-)",
    "UP&DN(L)", "UP&DN(L&H)", "UP&DN(-)", "RAND(L)", "RAND(-)", "PHRASE"
};
const juce::StringArray arpSplitChoices { "UPPER", "LOWER", "BOTH" };

const juce::StringArray& arpStyleChoices()
{
    static const juce::StringArray names = []
    {
        juce::StringArray list;
        for (const auto& entry : septum::arpeggioStyles())
            list.add (entry.name);
        return list;
    }();
    return names;
}

const juce::StringArray keyboardModeChoices { "SINGLE", "DUAL", "SPLIT" };
const juce::StringArray keyboardPartChoices { "UPPER", "LOWER" };
const juce::StringArray modAssignChoices { "OSC1&OSC2", "OSC1", "OSC2", "PW1",
                                           "PW2", "FILTER", "AMP", "AUDIO-FIL" };
const juce::StringArray delayHfDampChoices {
    "200", "250", "315", "400", "500", "630", "800", "1000", "1250", "1600",
    "2000", "2500", "3150", "4000", "5000", "6300", "8000", "BYPASS"
};
const juce::StringArray reverbHighCutChoices {
    "160", "200", "250", "320", "400", "500", "640", "800", "1000", "1250",
    "1600", "2000", "2500", "3200", "4000", "5000", "6400", "8000", "10000",
    "12500", "BYPASS"
};
const juce::StringArray reverbLfDampChoices {
    "50", "64", "80", "100", "125", "160", "200", "250", "320", "400", "500",
    "640", "800", "1000", "1250", "1600", "2000", "2500", "3200", "4000"
};
const juce::StringArray reverbHfDampChoices { "4000", "5000", "6400", "8000",
                                              "10000", "12500" };

// Shorthand for the field accessors.
#define TONE_INT(field) \
    [] (const TonePatch& t) { return (float) t.field; }, \
    [] (TonePatch& t, float v) { t.field = (int) std::lround (v); }
#define TONE_BOOL(field) \
    [] (const TonePatch& t) { return t.field ? 1.0f : 0.0f; }, \
    [] (TonePatch& t, float v) { t.field = v >= 0.5f; }
#define TONE_ENUM(field, type) \
    [] (const TonePatch& t) { return (float) (int) t.field; }, \
    [] (TonePatch& t, float v) { t.field = (type) (int) std::lround (v); }

const std::vector<ToneBinding>& toneBindings()
{
    using septum::FilterSlope;
    using septum::FilterType;
    using septum::LfoDest1;
    using septum::LfoDest2;
    using septum::LfoShape;
    using septum::LowFreqMode;
    using septum::MixModType;
    using septum::MonoMode;
    using septum::Waveform;

    static const std::vector<ToneBinding> bindings {
        { "osc1_wave", "OSC1 Wave", Kind::Choice, 0, 9, &waveChoices,
          TONE_ENUM (osc1.wave, Waveform) },
        { "osc1_wide", "OSC1 Pitch Wide", Kind::Bool, 0, 1, nullptr,
          TONE_BOOL (osc1.pitchWide) },
        { "osc1_pitch", "OSC1 Pitch", Kind::Int, -36, 36, nullptr,
          TONE_INT (osc1.coarse) },
        { "osc1_detune", "OSC1 Detune", Kind::Int, -50, 50, nullptr,
          TONE_INT (osc1.fine) },
        { "osc1_pw", "OSC1 PW/Feedback", Kind::Int, 0, 127, nullptr,
          TONE_INT (osc1.pulseWidth) },
        { "osc1_penv_depth", "OSC1 Pitch Env Depth", Kind::Int, -63, 63, nullptr,
          TONE_INT (osc1.pitchEnvDepth) },
        { "osc2_wave", "OSC2 Wave", Kind::Choice, 0, 9, &waveChoices,
          TONE_ENUM (osc2.wave, Waveform) },
        { "osc2_wide", "OSC2 Pitch Wide", Kind::Bool, 0, 1, nullptr,
          TONE_BOOL (osc2.pitchWide) },
        { "osc2_pitch", "OSC2 Pitch", Kind::Int, -36, 36, nullptr,
          TONE_INT (osc2.coarse) },
        { "osc2_detune", "OSC2 Detune", Kind::Int, -50, 50, nullptr,
          TONE_INT (osc2.fine) },
        { "osc2_pw", "OSC2 PW/Feedback", Kind::Int, 0, 127, nullptr,
          TONE_INT (osc2.pulseWidth) },
        { "osc2_penv_depth", "OSC2 Pitch Env Depth", Kind::Int, -63, 63, nullptr,
          TONE_INT (osc2.pitchEnvDepth) },
        { "penv_attack", "Pitch Env A", Kind::Int, 0, 127, nullptr,
          TONE_INT (pitchEnvAttack) },
        { "penv_decay", "Pitch Env D", Kind::Int, 0, 127, nullptr,
          TONE_INT (pitchEnvDecay) },
        { "mix_type", "Mix/Mod Type", Kind::Choice, 0, 3, &mixTypeChoices,
          TONE_ENUM (mixType, MixModType) },
        { "balance", "Balance", Kind::Int, -63, 63, nullptr, TONE_INT (balance) },
        { "low_freq", "Low Freq", Kind::Choice, 0, 3, &lowFreqChoices,
          TONE_ENUM (lowFreq, LowFreqMode) },
        { "filter_type", "Filter Type", Kind::Choice, 0, 4, &filterTypeChoices,
          TONE_ENUM (filterType, FilterType) },
        { "filter_slope", "Filter Slope", Kind::Choice, 0, 2, &slopeChoices,
          TONE_ENUM (filterSlope, FilterSlope) },
        { "cutoff", "Cutoff", Kind::Int, 0, 127, nullptr, TONE_INT (cutoff) },
        { "key_follow", "Key Follow", Kind::Int, -200, 200, nullptr,
          TONE_INT (keyFollow) },
        { "cutoff_vel", "Cutoff Velocity Sens", Kind::Int, -63, 63, nullptr,
          TONE_INT (cutoffVelocitySens) },
        { "resonance", "Resonance", Kind::Int, 0, 127, nullptr,
          TONE_INT (resonance) },
        { "fenv_attack", "Filter Env A", Kind::Int, 0, 127, nullptr,
          TONE_INT (filterEnvAttack) },
        { "fenv_decay", "Filter Env D", Kind::Int, 0, 127, nullptr,
          TONE_INT (filterEnvDecay) },
        { "fenv_sustain", "Filter Env S", Kind::Int, 0, 127, nullptr,
          TONE_INT (filterEnvSustain) },
        { "fenv_release", "Filter Env R", Kind::Int, 0, 127, nullptr,
          TONE_INT (filterEnvRelease) },
        { "fenv_depth", "Filter Env Depth", Kind::Int, -63, 63, nullptr,
          TONE_INT (filterEnvDepth) },
        { "overdrive", "Overdrive", Kind::Bool, 0, 1, nullptr,
          TONE_BOOL (overdrive) },
        { "drive", "Drive", Kind::Int, 0, 127, nullptr, TONE_INT (drive) },
        { "level", "Level", Kind::Int, 0, 127, nullptr, TONE_INT (level) },
        { "level_vel", "Level Velocity Sens", Kind::Int, -63, 63, nullptr,
          TONE_INT (levelVelocitySens) },
        { "pan", "Pan", Kind::Int, -64, 63, nullptr, TONE_INT (pan) },
        { "aenv_attack", "Amp Env A", Kind::Int, 0, 127, nullptr,
          TONE_INT (ampEnvAttack) },
        { "aenv_decay", "Amp Env D", Kind::Int, 0, 127, nullptr,
          TONE_INT (ampEnvDecay) },
        { "aenv_sustain", "Amp Env S", Kind::Int, 0, 127, nullptr,
          TONE_INT (ampEnvSustain) },
        { "aenv_release", "Amp Env R", Kind::Int, 0, 127, nullptr,
          TONE_INT (ampEnvRelease) },
        { "delay_depth", "Delay Depth", Kind::Int, 0, 127, nullptr,
          TONE_INT (delayDepth) },
        { "reverb_depth", "Reverb Depth", Kind::Int, 0, 127, nullptr,
          TONE_INT (reverbDepth) },
        { "lfo1_shape", "LFO1 Shape", Kind::Choice, 0, 7, &lfoShapeChoices,
          TONE_ENUM (lfo1.shape, LfoShape) },
        { "lfo1_rate", "LFO1 Rate", Kind::Int, 0, 127, nullptr,
          TONE_INT (lfo1.rate) },
        { "lfo1_sync", "LFO1 Tempo Sync", Kind::Bool, 0, 1, nullptr,
          TONE_BOOL (lfo1.tempoSync) },
        { "lfo1_sync_note", "LFO1 Sync Note", Kind::Choice, 0, 20,
          &syncNoteChoices, TONE_INT (lfo1.tempoSyncNote) },
        { "lfo1_fade", "LFO1 Fade Time", Kind::Int, 0, 127, nullptr,
          TONE_INT (lfo1.fadeTime) },
        { "lfo1_key_trig", "LFO1 Key Trigger", Kind::Bool, 0, 1, nullptr,
          TONE_BOOL (lfo1.keyTrigger) },
        { "lfo1_dest1", "LFO1 Destination 1", Kind::Choice, 0, 4,
          &lfoDest1Choices, TONE_ENUM (lfo1.destination1, LfoDest1) },
        { "lfo1_depth1", "LFO1 Depth 1", Kind::Int, -63, 63, nullptr,
          TONE_INT (lfo1.depth1) },
        { "lfo1_dest2", "LFO1 Destination 2", Kind::Choice, 0, 3,
          &lfoDest2Choices, TONE_ENUM (lfo1.destination2, LfoDest2) },
        { "lfo1_depth2", "LFO1 Depth 2", Kind::Int, -63, 63, nullptr,
          TONE_INT (lfo1.depth2) },
        { "lfo2_shape", "LFO2 Shape", Kind::Choice, 0, 7, &lfoShapeChoices,
          TONE_ENUM (lfo2.shape, LfoShape) },
        { "lfo2_rate", "LFO2 Rate", Kind::Int, 0, 127, nullptr,
          TONE_INT (lfo2.rate) },
        { "lfo2_sync", "LFO2 Tempo Sync", Kind::Bool, 0, 1, nullptr,
          TONE_BOOL (lfo2.tempoSync) },
        { "lfo2_sync_note", "LFO2 Sync Note", Kind::Choice, 0, 20,
          &syncNoteChoices, TONE_INT (lfo2.tempoSyncNote) },
        { "lfo2_fade", "LFO2 Fade Time", Kind::Int, 0, 127, nullptr,
          TONE_INT (lfo2.fadeTime) },
        { "lfo2_key_trig", "LFO2 Key Trigger", Kind::Bool, 0, 1, nullptr,
          TONE_BOOL (lfo2.keyTrigger) },
        { "lfo2_dest1", "LFO2 Destination 1", Kind::Choice, 0, 4,
          &lfoDest1Choices, TONE_ENUM (lfo2.destination1, LfoDest1) },
        { "lfo2_depth1", "LFO2 Depth 1", Kind::Int, -63, 63, nullptr,
          TONE_INT (lfo2.depth1) },
        { "lfo2_dest2", "LFO2 Destination 2", Kind::Choice, 0, 3,
          &lfoDest2Choices, TONE_ENUM (lfo2.destination2, LfoDest2) },
        { "lfo2_depth2", "LFO2 Depth 2", Kind::Int, -63, 63, nullptr,
          TONE_INT (lfo2.depth2) },
        { "bend_range", "Pitch Bend Range", Kind::Int, 0, 24, nullptr,
          TONE_INT (bendRange) },
        { "octave_shift", "Octave Shift", Kind::Int, -3, 3, nullptr,
          TONE_INT (octaveShift) },
        { "portamento", "Portamento", Kind::Bool, 0, 1, nullptr,
          TONE_BOOL (portamento) },
        { "porta_time", "Portamento Time", Kind::Int, 0, 127, nullptr,
          TONE_INT (portamentoTime) },
        { "mono_mode", "Poly/Solo", Kind::Choice, 0, 3, &monoChoices,
          TONE_ENUM (mono, MonoMode) },
    };
    return bindings;
}

struct PatchBinding
{
    const char* id;
    const char* label;
    Kind kind;
    int low, high;
    const juce::StringArray* choices;
    float (*get) (const Patch&);
    void (*set) (Patch&, float);
};

// The external-input path is a system setting, not patch data (OM pp. 49-51),
// so it gets its own binding table: these parameters live in the plug-in's
// state and are automatable, but a program change must not touch them.
struct ExternalBinding
{
    const char* id;
    const char* label;
    Kind kind;
    int low, high;
    const juce::StringArray* choices;
    float (*get) (const septum::ExternalInput&);
    void (*set) (septum::ExternalInput&, float);
};

const juce::StringArray audioFilterTypeChoices { "LPF", "HPF", "BPF", "NOTCH" };

#define EXT_INT(field) \
    [] (const septum::ExternalInput& e) { return (float) e.field; }, \
    [] (septum::ExternalInput& e, float v) { e.field = (int) std::lround (v); }
#define EXT_BOOL(field) \
    [] (const septum::ExternalInput& e) { return e.field ? 1.0f : 0.0f; }, \
    [] (septum::ExternalInput& e, float v) { e.field = v >= 0.5f; }
#define EXT_ENUM(field, type) \
    [] (const septum::ExternalInput& e) { return (float) (int) e.field; }, \
    [] (septum::ExternalInput& e, float v) { e.field = (type) (int) std::lround (v); }

const std::vector<ExternalBinding>& externalBindings()
{
    using septum::AudioFilterType;
    using septum::FilterSlope;
    static const std::vector<ExternalBinding> bindings {
        { "ext_input_vol", "External Input Volume", Kind::Int, 0, 127, nullptr,
          EXT_INT (inputVolume) },
        { "ext_center_cancel", "Center Cancel", Kind::Bool, 0, 1, nullptr,
          EXT_BOOL (centerCancel) },
        { "audio_filter_on", "Audio Filter Switch", Kind::Bool, 0, 1, nullptr,
          EXT_BOOL (filterOn) },
        { "audio_filter_type", "Audio Filter Type", Kind::Choice, 0, 4,
          &audioFilterTypeChoices, EXT_ENUM (type, AudioFilterType) },
        { "audio_filter_slope", "Audio Filter Slope", Kind::Choice, 0, 2,
          &slopeChoices, EXT_ENUM (slope, FilterSlope) },
        { "audio_filter_cutoff", "Audio Filter Cutoff", Kind::Int, 0, 127,
          nullptr, EXT_INT (cutoff) },
        { "audio_filter_reso", "Audio Filter Resonance", Kind::Int, 0, 127,
          nullptr, EXT_INT (resonance) },
    };
    return bindings;
}

#define PATCH_INT(field) \
    [] (const Patch& p) { return (float) p.field; }, \
    [] (Patch& p, float v) { p.field = (int) std::lround (v); }
#define PATCH_BOOL(field) \
    [] (const Patch& p) { return p.field ? 1.0f : 0.0f; }, \
    [] (Patch& p, float v) { p.field = v >= 0.5f; }
#define PATCH_ENUM(field, type) \
    [] (const Patch& p) { return (float) (int) p.field; }, \
    [] (Patch& p, float v) { p.field = (type) (int) std::lround (v); }

const std::vector<PatchBinding>& patchBindings()
{
    using septum::KeyboardMode;
    using septum::KeyboardPart;
    using septum::ModulationAssign;

    static const std::vector<PatchBinding> bindings {
        { "patch_level", "Patch Level", Kind::Int, 0, 127, nullptr,
          PATCH_INT (patchLevel) },
        { "tone_balance", "Tone Balance", Kind::Int, -63, 63, nullptr,
          PATCH_INT (toneBalance) },
        { "patch_tempo", "Patch Tempo", Kind::Int, 5, 300, nullptr,
          PATCH_INT (tempo) },
        { "keyboard_mode", "Keyboard Mode", Kind::Choice, 0, 3,
          &keyboardModeChoices, PATCH_ENUM (keyboardMode, KeyboardMode) },
        { "keyboard_part", "Keyboard Part", Kind::Choice, 0, 2,
          &keyboardPartChoices, PATCH_ENUM (keyboardPart, KeyboardPart) },
        { "split_point", "Split Point", Kind::Int, 21, 108, nullptr,
          PATCH_INT (splitPoint) },
        { "delay_on", "Delay Switch", Kind::Bool, 0, 1, nullptr,
          PATCH_BOOL (delayOn) },
        { "reverb_on", "Reverb Switch", Kind::Bool, 0, 1, nullptr,
          PATCH_BOOL (reverbOn) },
        { "mod_assign", "Modulation Assign", Kind::Choice, 0, 8,
          &modAssignChoices, PATCH_ENUM (modulationAssign, ModulationAssign) },
        { "arp_on", "Arpeggio Switch", Kind::Bool, 0, 1, nullptr,
          PATCH_BOOL (arpeggio.on) },
        { "arp_hold", "Arpeggio Hold", Kind::Bool, 0, 1, nullptr,
          PATCH_BOOL (arpeggio.hold) },
        { "arp_style", "Arpeggio Style", Kind::Choice, 0,
          (int) septum::arpeggioStyles().size(), &arpStyleChoices(),
          PATCH_INT (arpeggio.styleIndex) },
        { "arp_grid", "Arpeggio Grid", Kind::Choice, 0, 9, &arpGridChoices,
          PATCH_ENUM (arpeggio.grid, septum::ArpeggioGrid) },
        { "arp_duration", "Arpeggio Duration", Kind::Choice, 0, 10,
          &arpDurationChoices,
          PATCH_ENUM (arpeggio.duration, septum::ArpeggioDuration) },
        { "arp_motif", "Arpeggio Motif", Kind::Choice, 0, 12, &arpMotifChoices,
          PATCH_ENUM (arpeggio.motif, septum::ArpeggioMotif) },
        { "arp_octave", "Arpeggio Octave Range", Kind::Int, -3, 3, nullptr,
          PATCH_INT (arpeggio.octaveRange) },
        { "arp_accent", "Arpeggio Accent", Kind::Int, 0, 100, nullptr,
          PATCH_INT (arpeggio.accent) },
        { "arp_velocity", "Arpeggio Velocity", Kind::Int, 0, 127, nullptr,
          PATCH_INT (arpeggio.velocity) },
        { "arp_split", "Split Arpeggio", Kind::Choice, 0, 3, &arpSplitChoices,
          PATCH_ENUM (arpeggio.splitArpeggio, septum::SplitArpeggio) },
        { "delay_time", "Delay Time", Kind::Int, 0, 127, nullptr,
          PATCH_INT (delay.time) },
        { "delay_feedback", "Delay Feedback", Kind::Int, -98, 98, nullptr,
          PATCH_INT (delay.feedback) },
        { "delay_hf_damp", "Delay HF Damp", Kind::Choice, 0, 18,
          &delayHfDampChoices, PATCH_INT (delay.hfDamp) },
        { "delay_mod_rate", "Delay Mod Rate", Kind::Int, 0, 127, nullptr,
          PATCH_INT (delay.modulationRate) },
        { "delay_mod_depth", "Delay Mod Depth", Kind::Int, 0, 127, nullptr,
          PATCH_INT (delay.modulationDepth) },
        { "reverb_time", "Reverb Time", Kind::Int, 0, 127, nullptr,
          PATCH_INT (reverb.time) },
        { "reverb_pre_delay", "Reverb Pre Delay", Kind::Int, 0, 125, nullptr,
          PATCH_INT (reverb.preDelay) },
        { "reverb_size", "Reverb Size", Kind::Int, 0, 7, nullptr,
          PATCH_INT (reverb.size) },
        { "reverb_high_cut", "Reverb High Cut", Kind::Choice, 0, 21,
          &reverbHighCutChoices, PATCH_INT (reverb.highCut) },
        { "reverb_density", "Reverb Density", Kind::Int, 0, 127, nullptr,
          PATCH_INT (reverb.density) },
        { "reverb_diffusion", "Reverb Diffusion", Kind::Int, 0, 127, nullptr,
          PATCH_INT (reverb.diffusion) },
        { "reverb_lf_damp_freq", "Reverb LF Damp Freq", Kind::Choice, 0, 20,
          &reverbLfDampChoices, PATCH_INT (reverb.lfDampFrequency) },
        { "reverb_lf_damp_gain", "Reverb LF Damp Gain", Kind::Int, -36, 0,
          nullptr, PATCH_INT (reverb.lfDampGain) },
        { "reverb_hf_damp_freq", "Reverb HF Damp Freq", Kind::Choice, 0, 6,
          &reverbHfDampChoices, PATCH_INT (reverb.hfDampFrequency) },
        { "reverb_hf_damp_gain", "Reverb HF Damp Gain", Kind::Int, -36, 0,
          nullptr, PATCH_INT (reverb.hfDampGain) },
        { "master_level", "Master Level", Kind::Int, 0, 127, nullptr,
          [] (const Patch&) { return 127.0f; }, [] (Patch&, float) {} },
    };
    return bindings;
}

// The documented control-change map (Owner's Manual p.72). CC#83 stands in
// for the printed CC#88 collision on UPPER filter-env decay (see the research
// contract's OQ-02).
struct CcBinding
{
    int controller;
    bool upper;
    const char* suffix;
    bool signedValue;  // CC 0-127 arrives as value-64 for -63..+63 displays
};

constexpr CcBinding ccBindings[] {
    { 20, true, "osc1_pitch", true },   { 76, true, "osc1_detune", true },
    { 3, true, "osc1_pw", false },      { 24, true, "osc1_penv_depth", true },
    { 21, true, "osc2_pitch", true },   { 77, true, "osc2_detune", true },
    { 95, true, "osc2_pw", false },     { 25, true, "osc2_penv_depth", true },
    { 26, true, "penv_attack", false }, { 27, true, "penv_decay", false },
    { 8, true, "balance", true },       { 74, true, "cutoff", false },
    { 30, true, "key_follow", true },   { 71, true, "resonance", false },
    { 82, true, "fenv_attack", false }, { 83, true, "fenv_decay", false },
    { 28, true, "fenv_sustain", false },{ 29, true, "fenv_release", false },
    { 81, true, "fenv_depth", true },   { 14, true, "level", false },
    { 73, true, "aenv_attack", false }, { 75, true, "aenv_decay", false },
    { 31, true, "aenv_sustain", false },{ 72, true, "aenv_release", false },
    { 93, true, "delay_depth", false }, { 91, true, "reverb_depth", false },
    { 16, true, "lfo1_rate", false },   { 18, true, "lfo1_depth1", true },
    { 19, true, "lfo1_depth2", true },  { 17, true, "lfo2_rate", false },
    { 22, true, "lfo2_depth1", true },  { 23, true, "lfo2_depth2", true },
    { 78, false, "osc1_pitch", true },  { 79, false, "osc1_detune", true },
    { 80, false, "osc1_pw", false },    { 70, false, "osc1_penv_depth", true },
    { 85, false, "osc2_pitch", true },  { 86, false, "osc2_detune", true },
    { 87, false, "osc2_pw", false },    { 88, false, "osc2_penv_depth", true },
    { 89, false, "penv_attack", false },{ 90, false, "penv_decay", false },
    { 9, false, "balance", true },      { 102, false, "cutoff", false },
    { 103, false, "key_follow", true }, { 104, false, "resonance", false },
    { 105, false, "fenv_attack", false }, { 106, false, "fenv_decay", false },
    { 107, false, "fenv_sustain", false }, { 108, false, "fenv_release", false },
    { 109, false, "fenv_depth", true }, { 15, false, "level", false },
    { 110, false, "aenv_attack", false }, { 111, false, "aenv_decay", false },
    { 112, false, "aenv_sustain", false }, { 113, false, "aenv_release", false },
    { 94, false, "delay_depth", false }, { 92, false, "reverb_depth", false },
    { 114, false, "lfo1_rate", false }, { 115, false, "lfo1_depth1", true },
    { 116, false, "lfo1_depth2", true },{ 117, false, "lfo2_rate", false },
    { 118, false, "lfo2_depth1", true },{ 119, false, "lfo2_depth2", true },
};
} // namespace

// ---------------------------------------------------------------------------

SeptumAudioProcessor::SeptumAudioProcessor()
    // The modelled instrument has stereo INPUT jacks feeding the AUDIO FILTER
    // and the EXT-IN oscillator waveform, so the plug-in declares a stereo
    // input bus. It is off by default: a host that gives a synthesizer no
    // input still loads exactly as before, and the engine then behaves like
    // the hardware with nothing plugged in.
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(),
                                       true)
                          .withInput ("External In",
                                      juce::AudioChannelSet::stereo(), false)),
      parameters (*this, nullptr, "Septum", createParameterLayout())
{
    cacheParameterPointers();
}

void SeptumAudioProcessor::cacheParameterPointers()
{
    for (const bool upper : { true, false })
    {
        auto& values = upper ? upperValues : lowerValues;
        const juce::String prefix = upper ? "up_" : "lo_";
        for (const auto& binding : toneBindings())
        {
            auto* value = parameters.getRawParameterValue (prefix + binding.suffix);
            jassert (value != nullptr);
            values.push_back (value);
        }
    }
    for (const auto& binding : patchBindings())
    {
        auto* value = parameters.getRawParameterValue (binding.id);
        jassert (value != nullptr);
        patchValues.push_back (value);
    }
    masterValue = parameters.getRawParameterValue ("master_level");
    for (const auto& binding : externalBindings())
    {
        auto* value = parameters.getRawParameterValue (binding.id);
        jassert (value != nullptr);
        externalValues.push_back (value);
    }

    for (const auto& binding : ccBindings)
    {
        const juce::String id =
            juce::String (binding.upper ? "up_" : "lo_") + binding.suffix;
        if (auto* parameter = parameters.getParameter (id))
            ccCache.push_back ({ binding.controller, parameter,
                                 binding.signedValue,
                                 juce::String (binding.suffix) == "key_follow" });
    }
    // Settled (OM p. 72): the audio filter answers on CC#2 and CC#4.
    if (auto* parameter = parameters.getParameter ("audio_filter_cutoff"))
        ccCache.push_back ({ 2, parameter, false, false });
    if (auto* parameter = parameters.getParameter ("audio_filter_reso"))
        ccCache.push_back ({ 4, parameter, false, false });
    if (auto* parameter = parameters.getParameter ("delay_time"))
        ccCache.push_back ({ 12, parameter, false, false });
    if (auto* parameter = parameters.getParameter ("reverb_time"))
        ccCache.push_back ({ 13, parameter, false, false });
}

namespace
{
// How a value is printed, on the panel and in the host's own parameter list.
// The manual prints signed parameters with their sign and PAN as L64...63R,
// so the plug-in does too.
const std::vector<juce::String>& signedParameterSuffixes()
{
    static const std::vector<juce::String> suffixes {
        "osc1_pitch", "osc1_detune", "osc1_penv_depth", "osc2_pitch",
        "osc2_detune", "osc2_penv_depth", "balance", "key_follow",
        "cutoff_vel", "fenv_depth", "level_vel",
        "octave_shift", "tone_balance", "arp_octave", "delay_feedback",
        "reverb_lf_damp_gain", "reverb_hf_damp_gain"
    };
    return suffixes;
}

[[nodiscard]] bool isSignedDisplay (const juce::String& id)
{
    for (const auto& suffix : signedParameterSuffixes())
        if (id == suffix || id.endsWith ("_" + suffix)
            || (id.startsWith ("up_") && id.substring (3) == suffix)
            || (id.startsWith ("lo_") && id.substring (3) == suffix))
            return true;
    return false;
}

[[nodiscard]] juce::AudioParameterIntAttributes intAttributes (const juce::String& id)
{
    if (id == "pan" || id == "up_pan" || id == "lo_pan")
        return juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int value, int)
            {
                // L64 ... 0 ... 63R, the display the manual prints.
                if (value < 0)
                    return "L" + juce::String (-value);
                if (value > 0)
                    return juce::String (value) + "R";
                return juce::String ("0");
            });
    if (isSignedDisplay (id))
        return juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int value, int)
            {
                return value > 0 ? "+" + juce::String (value)
                                 : juce::String (value);
            });
    return {};
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
SeptumAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const Patch defaults = septum::initPatch();

    const auto addTone = [&layout, &defaults] (bool upper)
    {
        const TonePatch& tone = upper ? defaults.upper : defaults.lower;
        const String prefix = upper ? "up_" : "lo_";
        const String partName = upper ? "Upper " : "Lower ";
        for (const auto& binding : toneBindings())
        {
            const String id = prefix + binding.suffix;
            const String name = partName + binding.label;
            const auto defaultValue = binding.get (tone);
            switch (binding.kind)
            {
                case Kind::Int:
                    layout.add (std::make_unique<AudioParameterInt> (
                        ParameterID { id, 1 }, name, binding.low, binding.high,
                        (int) std::lround (defaultValue), intAttributes (id)));
                    break;
                case Kind::Bool:
                    layout.add (std::make_unique<AudioParameterBool> (
                        ParameterID { id, 1 }, name, defaultValue >= 0.5f));
                    break;
                case Kind::Choice:
                    layout.add (std::make_unique<AudioParameterChoice> (
                        ParameterID { id, 1 }, name, *binding.choices,
                        (int) std::lround (defaultValue)));
                    break;
            }
        }
    };
    addTone (true);
    addTone (false);

    const septum::ExternalInput externalDefaults {};
    for (const auto& binding : externalBindings())
    {
        const auto defaultValue = binding.get (externalDefaults);
        switch (binding.kind)
        {
            case Kind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (
                    juce::ParameterID { binding.id, 1 }, binding.label,
                    binding.low, binding.high, (int) std::lround (defaultValue),
                    intAttributes (binding.id)));
                break;
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { binding.id, 1 }, binding.label,
                    defaultValue >= 0.5f));
                break;
            case Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { binding.id, 1 }, binding.label,
                    *binding.choices, (int) std::lround (defaultValue)));
                break;
        }
    }

    for (const auto& binding : patchBindings())
    {
        const auto defaultValue =
            juce::String (binding.id) == "master_level"
                ? 100.0f
                : binding.get (defaults);
        switch (binding.kind)
        {
            case Kind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (
                    juce::ParameterID { binding.id, 1 }, binding.label,
                    binding.low, binding.high, (int) std::lround (defaultValue),
                    intAttributes (binding.id)));
                break;
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { binding.id, 1 }, binding.label,
                    defaultValue >= 0.5f));
                break;
            case Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { binding.id, 1 }, binding.label,
                    *binding.choices, (int) std::lround (defaultValue)));
                break;
        }
    }

    return layout;
}

septum::ExternalInput SeptumAudioProcessor::snapshotExternalInput() const
{
    septum::ExternalInput settings {};
    const auto& bindings = externalBindings();
    for (std::size_t i = 0; i < bindings.size(); ++i)
        bindings[i].set (settings,
                         externalValues[i]->load (std::memory_order_relaxed));
    septum::clampToDocumentedRanges (settings);
    return settings;
}

septum::Patch SeptumAudioProcessor::snapshotPatch() const
{
    // Runs on the audio thread every block: only cached atomic loads, no
    // string building or lookups.
    Patch patch = septum::initPatch();

    const auto& bindings = toneBindings();
    for (std::size_t index = 0; index < bindings.size(); ++index)
    {
        bindings[index].set (patch.upper,
                             upperValues[index]->load (std::memory_order_relaxed));
        bindings[index].set (patch.lower,
                             lowerValues[index]->load (std::memory_order_relaxed));
    }
    const auto& shared = patchBindings();
    for (std::size_t index = 0; index < shared.size(); ++index)
        shared[index].set (patch,
                           patchValues[index]->load (std::memory_order_relaxed));

    // The style index is the panel's selector; the grid it names is what the
    // engine actually plays.
    septum::applyArpeggioStyle (patch, patch.arpeggio.styleIndex);
    septum::clampToDocumentedRanges (patch);
    return patch;
}

void SeptumAudioProcessor::prepareToPlay (double sampleRate,
                                              int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setMasterLevel ((int) std::lround (masterValue->load()));
    engine.setPatch (snapshotPatch());
    engine.setExternalInput (snapshotExternalInput());
    engine.reset();
    monoScratch.assign ((std::size_t) juce::jmax (samplesPerBlock, 16), 0.0f);
    externalInputL.assign ((std::size_t) juce::jmax (samplesPerBlock, 16), 0.0f);
    externalInputR.assign ((std::size_t) juce::jmax (samplesPerBlock, 16), 0.0f);
    // The AMP overdrive's oversampling chain has a fixed group delay, and
    // every voice carries it whether it is shaping or not so layered tones
    // stay in phase. Report it so the host can line the track back up.
    setLatencySamples (engine.latencySamples());
}

void SeptumAudioProcessor::releaseResources() {}

bool SeptumAudioProcessor::isBusesLayoutSupported (
    const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto input = layouts.getMainInputChannelSet();
    return input.isDisabled() || input == juce::AudioChannelSet::mono()
           || input == juce::AudioChannelSet::stereo();
}

double SeptumAudioProcessor::getTailLengthSeconds() const
{
    // Honest per-patch tail: the longest amp release, plus the delay's
    // repeats down to -60 dB, plus the reverb's RT60 (the delay feeds the
    // reverb in series). The documented +/-98 % feedback extreme decays only
    // 2 % per repeat — minutes of audible tail — so the report is capped at
    // a bound offline renderers can live with.
    const Patch patch = snapshotPatch();
    using namespace septum::mapping;

    double tail = std::max (decaySeconds (patch.upper.ampEnvRelease),
                            decaySeconds (patch.lower.ampEnvRelease));
    if (patch.delayOn)
    {
        const double feedback =
            std::clamp (std::abs (patch.delay.feedback) / 100.0, 0.0, 0.99);
        const double repeats =
            feedback <= 0.0 ? 1.0
                            : std::max (1.0, std::log (1.0e-3) / std::log (feedback));
        tail += delaySeconds (patch.delay.time) * repeats;
    }
    if (patch.reverbOn)
        tail += reverbSeconds (patch.reverb.time, patch.reverb.size);
    return std::clamp (tail, 0.1, 120.0);
}

void SeptumAudioProcessor::triggerFromUi (int note, int velocity) noexcept
{
    // A release latched during an earlier overflow is now stale: this press
    // supersedes it, and if it survived, the audio thread could apply it
    // right after the new note-on and cut the note short. Clearing it before
    // publishing the note-on makes that impossible — the audio thread drains
    // the queue before the latches, so once it can see this note-on, the
    // stale bit is already gone.
    note = juce::jlimit (0, 127, note);
    forcedRelease[(std::size_t) (note >> 6)].fetch_and (
        ~(1ull << (note & 63)), std::memory_order_acq_rel);
    const auto write = uiWrite.load (std::memory_order_relaxed);
    const auto read = uiRead.load (std::memory_order_acquire);
    if (write - read >= uiQueueCapacity)
        return;
    uiQueue[write % uiQueueCapacity] = { note, juce::jlimit (1, 127, velocity) };
    uiWrite.store (write + 1, std::memory_order_release);
}

void SeptumAudioProcessor::releaseFromUi (int note) noexcept
{
    const auto write = uiWrite.load (std::memory_order_relaxed);
    const auto read = uiRead.load (std::memory_order_acquire);
    if (write - read >= uiQueueCapacity)
    {
        // A dropped note-off would leave the note stuck once processing
        // resumes; latch the release instead of losing it.
        note = juce::jlimit (0, 127, note);
        forcedRelease[(std::size_t) (note >> 6)].fetch_or (
            1ull << (note & 63), std::memory_order_release);
        return;
    }
    uiQueue[write % uiQueueCapacity] = { note, 0 };
    uiWrite.store (write + 1, std::memory_order_release);
}

bool SeptumAudioProcessor::handleController (int controller, int value)
{
    switch (controller)
    {
        case 0:
        case 32:
            // Bank select is accepted (settled CCs) but there is only the one
            // built-in program bank to select.
            return false;
        case 1:  engine.setModulation (value / 127.0); return false;
        case 7:  engine.setPartLevel (value / 127.0); return false;
        case 10: engine.setPartPan ((value - 64) / 63.0); return false;
        case 11: engine.setExpression (value / 127.0); return false;
        case 64: engine.setHold (value >= 64); return false;
        case 66: engine.setSostenuto (value >= 64); return false;
        case 84: engine.setPortamentoControl (value); return false;
        case 120: engine.allSoundOff(); return false;
        case 121:
            engine.setPitchBend (0.0);
            engine.setModulation (0.0);
            engine.setExpression (1.0);
            engine.setHold (false);
            engine.setSostenuto (false);
            return false;
        case 123:
        case 124:
        case 125:
            engine.allNotesOff();
            return false;
        default: break;
    }

    // Panel parameters per the documented CC map: received CCs edit the
    // corresponding patch parameter, exactly as the hardware does. Cached
    // pointers keep this allocation-free on the audio thread.
    for (const auto& cached : ccCache)
    {
        if (cached.controller != controller)
            continue;
        float natural = cached.signedValue ? (float) (value - 64) : (float) value;
        if (cached.keyFollow)
            natural = juce::jlimit (-200.0f, 200.0f, (float) ((value - 64) * 10));
        const auto& range = cached.parameter->getNormalisableRange();
        cached.parameter->beginChangeGesture();
        cached.parameter->setValueNotifyingHost (
            range.convertTo0to1 (range.snapToLegalValue (natural)));
        cached.parameter->endChangeGesture();
        return true;
    }
    return false;
}

bool SeptumAudioProcessor::handleMidiMessage (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        engine.noteOn (message.getNoteNumber(), message.getVelocity());
    else if (message.isNoteOff())
        engine.noteOff (message.getNoteNumber());
    else if (message.isPitchWheel())
        engine.setPitchBend ((message.getPitchWheelValue() - 8192) / 8192.0);
    else if (message.isController())
        return handleController (message.getControllerNumber(),
                                 message.getControllerValue());
    else if (message.isProgramChange())
    {
        const int program = message.getProgramChangeNumber();
        if (program >= 0 && program < getNumPrograms())
        {
            // Land the program in the raw parameter values right here: notes
            // later in this same block must already play it, later edits must
            // compose on top of it, and none of that may depend on a message
            // loop that an offline host might never pump. The queued spray
            // only repeats these values with host/UI notification.
            writeProgramToParameters (program);
            applyProgramAsync (program);
            return true;
        }
    }
    else if (message.isAllNotesOff())
        engine.allNotesOff();
    else if (message.isAllSoundOff())
        engine.allSoundOff();
    return false;
}

void SeptumAudioProcessor::writeProgramToParameters (int index) noexcept
{
    if (index < 0 || index >= getNumPrograms())
        return;
    const Patch& patch =
        septum::factoryPatches()[(std::size_t) index].patch;

    // Hold the generation odd across the burst, program index included, so a
    // concurrent state save — which seqlocks its raw-value copy against the
    // generation — can never serialize a half-written program or pair the
    // new index with the old values.
    patchGeneration.fetch_add (1, std::memory_order_acq_rel);
    currentProgram.store (index, std::memory_order_relaxed);

    const auto& bindings = toneBindings();
    for (std::size_t i = 0; i < bindings.size(); ++i)
    {
        upperValues[i]->store (bindings[i].get (patch.upper),
                               std::memory_order_relaxed);
        lowerValues[i]->store (bindings[i].get (patch.lower),
                               std::memory_order_relaxed);
    }
    const auto& shared = patchBindings();
    for (std::size_t i = 0; i < shared.size(); ++i)
        if (std::strcmp (shared[i].id, "master_level") != 0)
            patchValues[i]->store (shared[i].get (patch),
                                   std::memory_order_relaxed);

    patchGeneration.fetch_add (1, std::memory_order_acq_rel);
}

void SeptumAudioProcessor::reconcileProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    const Patch& patch =
        septum::factoryPatches()[(std::size_t) index].patch;

    const auto apply = [this] (const juce::String& id, float natural)
    {
        auto* parameter = parameters.getParameter (id);
        auto* raw = parameters.getRawParameterValue (id);
        if (parameter == nullptr || raw == nullptr)
            return;
        const auto& range = parameters.getParameterRange (id);
        const float target = range.snapToLegalValue (natural);
        // The audio path already wrote this exact value when the program
        // change arrived. If it has moved since, the user edited it after
        // the program change and the edit wins — replaying the factory
        // value here would snap their edit back.
        if (raw->load() != target)
            return;
        parameter->setValueNotifyingHost (range.convertTo0to1 (target));
    };

    for (const bool upper : { true, false })
    {
        const TonePatch& tone = upper ? patch.upper : patch.lower;
        const juce::String prefix = upper ? "up_" : "lo_";
        for (const auto& binding : toneBindings())
            apply (prefix + binding.suffix, binding.get (tone));
    }
    for (const auto& binding : patchBindings())
        if (juce::String (binding.id) != "master_level")
            apply (binding.id, binding.get (patch));
}

void SeptumAudioProcessor::applyProgramAsync (int program)
{
    // A MIDI program change already landed in the raw values on the audio
    // path; the message thread only repeats those values with host and UI
    // notification, skipping anything edited since. The writes are
    // value-identical to the current raw values, so no staging or
    // generation guard is needed around them.
    juce::MessageManager::callAsync (
        [weakThis = juce::WeakReference<SeptumAudioProcessor> (this), program]
        {
            if (auto* self = weakThis.get())
                self->reconcileProgram (program);
        });
}

namespace
{
[[nodiscard]] const float* externalPointer (int position, bool present,
                                            const std::vector<float>& buffer)
{
    return present ? buffer.data() + position : nullptr;
}
} // namespace

void SeptumAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // The input bus shares this buffer with the output, so the external audio
    // has to be copied out before the buffer is cleared. With the bus
    // disabled — the default, and what a synth normally gets — there is
    // nothing to copy and the engine sees the hardware's empty INPUT jacks.
    const auto samples = buffer.getNumSamples();
    if ((int) externalInputL.size() < samples)
    {
        externalInputL.resize ((std::size_t) juce::jmax (samples, 16));
        externalInputR.resize ((std::size_t) juce::jmax (samples, 16));
    }
    const auto inputBus = getBusBuffer (buffer, true, 0);
    const int inputChannels = inputBus.getNumChannels();
    const bool haveExternalInput = inputChannels > 0 && samples > 0;
    if (haveExternalInput)
    {
        const float* sourceL = inputBus.getReadPointer (0);
        const float* sourceR = inputBus.getReadPointer (inputChannels > 1 ? 1 : 0);
        std::copy (sourceL, sourceL + samples, externalInputL.begin());
        std::copy (sourceR, sourceR + samples, externalInputR.begin());
    }
    buffer.clear();

    // UI keyboard events.
    auto read = uiRead.load (std::memory_order_relaxed);
    const auto write = uiWrite.load (std::memory_order_acquire);
    while (read != write)
    {
        const auto& event = uiQueue[read % uiQueueCapacity];
        if (event.velocity > 0)
            engine.noteOn (event.note, event.velocity);
        else
            engine.noteOff (event.note);
        ++read;
    }
    uiRead.store (read, std::memory_order_release);

    // Releases latched when the UI queue overflowed: apply them now so no
    // on-screen key can stay stuck.
    for (std::size_t word = 0; word < forcedRelease.size(); ++word)
    {
        auto bits = forcedRelease[word].exchange (0u, std::memory_order_acquire);
        while (bits != 0u)
        {
            const int note = (int) (word * 64) + std::countr_zero (bits);
            engine.noteOff (note);
            bits &= bits - 1u;
        }
    }

    if (uiLeverDirty.exchange (false, std::memory_order_acquire))
    {
        engine.setPitchBend (uiBend.load (std::memory_order_relaxed));
        engine.setModulation (uiMod.load (std::memory_order_relaxed));
    }

    engine.setMasterLevel ((int) std::lround (
        masterValue->load (std::memory_order_relaxed)));

    // The engine's patch never depends on the message loop: MIDI program
    // changes write the raw values directly, and the snapshot is validated
    // against the message thread's write bursts (program sprays, state
    // restores) so a half-written mix is never rendered — the staged factory
    // patch or the previous block's patch covers the gap instead.
    const auto applyCurrentPatch = [this]
    {
        const int staged = stagedProgram.load (std::memory_order_acquire);
        if (staged >= 0 && staged < (int) septum::factoryPatches().size())
        {
            engine.setPatch (
                septum::factoryPatches()[(std::size_t) staged].patch);
            return;
        }
        // The external-input block is not patch data, but CC#2 and CC#4 edit
        // it and arrive mid-block like any other mapped panel CC, so it is
        // refreshed on the same segment boundary rather than a block late.
        engine.setExternalInput (snapshotExternalInput());
        const auto generation = patchGeneration.load (std::memory_order_acquire);
        const septum::Patch snapshot = snapshotPatch();
        if ((generation & 1u) == 0u
            && patchGeneration.load (std::memory_order_acquire) == generation)
            engine.setPatch (snapshot);
        // Otherwise a burst was in flight while the snapshot was read: keep
        // the previous patch for this segment and pick up the completed
        // values on the next one.
    };
    applyCurrentPatch();

    auto* left = buffer.getWritePointer (0);
    // The declared bus is stereo-only, but a defensive mono path must not
    // alias the channels: the engine writes L and R independently and the
    // output stage carries per-channel state.
    if ((int) monoScratch.size() < buffer.getNumSamples())
        monoScratch.resize ((std::size_t) juce::jmax (buffer.getNumSamples(), 16));
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1)
                                              : monoScratch.data();

    // Sample-accurate segmentation around MIDI events.
    int position = 0;
    for (const auto metadata : midiMessages)
    {
        const int eventPosition =
            juce::jlimit (0, buffer.getNumSamples(), metadata.samplePosition);
        if (eventPosition > position)
        {
            engine.process (left + position, right + position,
                            eventPosition - position,
                            externalPointer (position, haveExternalInput,
                                             externalInputL),
                            externalPointer (position, haveExternalInput,
                                             externalInputR));
            position = eventPosition;
        }
        if (handleMidiMessage (metadata.getMessage()))
            applyCurrentPatch();  // panel CC or program: next segment uses it
    }
    if (position < samples)
        engine.process (left + position, right + position, samples - position,
                        externalPointer (position, haveExternalInput,
                                         externalInputL),
                        externalPointer (position, haveExternalInput,
                                         externalInputR));

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            left[i] = 0.5f * (left[i] + monoScratch[(std::size_t) i]);

    activeVoices.store (engine.activeVoiceCount(), std::memory_order_relaxed);
}

int SeptumAudioProcessor::getNumPrograms()
{
    return (int) septum::factoryPatches().size();
}

int SeptumAudioProcessor::getCurrentProgram()
{
    return currentProgram.load (std::memory_order_relaxed);
}

void SeptumAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram.store (index, std::memory_order_relaxed);
    applyProgram (index);
}

void SeptumAudioProcessor::applyProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    const Patch& patch =
        septum::factoryPatches()[(std::size_t) index].patch;

    // The audio path renders this factory patch directly until every
    // parameter below has been written, so a block can never snapshot a
    // half-loaded program. The generation goes odd behind it: a snapshot
    // that overlapped this spray in any way is discarded.
    stagedProgram.store (index, std::memory_order_release);
    patchGeneration.fetch_add (1, std::memory_order_acq_rel);

    const auto apply = [this] (const juce::String& id, float natural)
    {
        if (auto* parameter = parameters.getParameter (id))
        {
            const auto& range = parameters.getParameterRange (id);
            parameter->setValueNotifyingHost (
                range.convertTo0to1 (range.snapToLegalValue (natural)));
        }
    };

    for (const bool upper : { true, false })
    {
        const TonePatch& tone = upper ? patch.upper : patch.lower;
        const juce::String prefix = upper ? "up_" : "lo_";
        for (const auto& binding : toneBindings())
            apply (prefix + binding.suffix, binding.get (tone));
    }
    for (const auto& binding : patchBindings())
        if (juce::String (binding.id) != "master_level")
            apply (binding.id, binding.get (patch));

    // Every parameter now matches the program; the audio path can go back to
    // snapshotting the APVTS.
    patchGeneration.fetch_add (1, std::memory_order_acq_rel);
    stagedProgram.store (-1, std::memory_order_release);
}

const juce::String SeptumAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};
    return septum::factoryPatches()[(std::size_t) index].name;
}

void SeptumAudioProcessor::getStateInformation (
    juce::MemoryBlock& destinationData)
{
    if (auto state = parameters.copyState(); state.isValid())
    {
        // The value tree lags an audio-path program write until the message
        // thread reconciles it — which a headless host may never do.
        // Serializing the raw values instead makes saved state always match
        // what is audible. The tree copy is ours alone, so this is safe on
        // any thread; the copy seqlocks against the generation so it can
        // never interleave a program write's burst, pairing the program
        // index with values it does not describe. The final attempt copies
        // unconditionally as a best effort.
        for (int attempt = 0; attempt < 64; ++attempt)
        {
            const auto generation =
                patchGeneration.load (std::memory_order_acquire);
            if ((generation & 1u) != 0u && attempt < 63)
                continue;
            for (int i = 0; i < state.getNumChildren(); ++i)
            {
                auto child = state.getChild (i);
                if (auto* raw = parameters.getRawParameterValue (
                        child.getProperty ("id").toString()))
                    child.setProperty ("value",
                                       raw->load (std::memory_order_relaxed),
                                       nullptr);
            }
            state.setProperty ("program",
                               currentProgram.load (std::memory_order_relaxed),
                               nullptr);
            if ((generation & 1u) == 0u
                && patchGeneration.load (std::memory_order_acquire) == generation)
                break;
        }
        if (const auto xml = state.createXml())
            copyXmlToBinary (*xml, destinationData);
    }
}

void SeptumAudioProcessor::setStateInformation (const void* data,
                                                    int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            currentProgram.store (state.getProperty ("program", 0),
                                  std::memory_order_relaxed);
            // A state restore is a multi-parameter write burst like a
            // program spray: keep the generation odd across it so the audio
            // thread discards any snapshot that overlapped it.
            patchGeneration.fetch_add (1, std::memory_order_acq_rel);
            parameters.replaceState (state);
            patchGeneration.fetch_add (1, std::memory_order_acq_rel);
        }
    }
}

juce::AudioProcessorEditor* SeptumAudioProcessor::createEditor()
{
    return new SeptumAudioProcessorEditor (*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeptumAudioProcessor();
}
