#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <vector>

namespace
{
using youknow201::Patch;
using youknow201::TonePatch;

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
    using youknow201::FilterSlope;
    using youknow201::FilterType;
    using youknow201::LfoDest1;
    using youknow201::LfoDest2;
    using youknow201::LfoShape;
    using youknow201::LowFreqMode;
    using youknow201::MixModType;
    using youknow201::MonoMode;
    using youknow201::Waveform;

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
    using youknow201::KeyboardMode;
    using youknow201::KeyboardPart;
    using youknow201::ModulationAssign;

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

YouKnow201AudioProcessor::YouKnow201AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput (
          "Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "YouKnow201", createParameterLayout())
{
    cacheParameterPointers();
}

void YouKnow201AudioProcessor::cacheParameterPointers()
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

    for (const auto& binding : ccBindings)
    {
        const juce::String id =
            juce::String (binding.upper ? "up_" : "lo_") + binding.suffix;
        if (auto* parameter = parameters.getParameter (id))
            ccCache.push_back ({ binding.controller, parameter,
                                 binding.signedValue,
                                 juce::String (binding.suffix) == "key_follow" });
    }
    if (auto* parameter = parameters.getParameter ("delay_time"))
        ccCache.push_back ({ 12, parameter, false, false });
    if (auto* parameter = parameters.getParameter ("reverb_time"))
        ccCache.push_back ({ 13, parameter, false, false });
}

juce::AudioProcessorValueTreeState::ParameterLayout
YouKnow201AudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const Patch defaults = youknow201::initPatch();

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
                        (int) std::lround (defaultValue)));
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
                    binding.low, binding.high, (int) std::lround (defaultValue)));
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

youknow201::Patch YouKnow201AudioProcessor::snapshotPatch() const
{
    // Runs on the audio thread every block: only cached atomic loads, no
    // string building or lookups.
    Patch patch = youknow201::initPatch();

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

    youknow201::clampToDocumentedRanges (patch);
    return patch;
}

void YouKnow201AudioProcessor::prepareToPlay (double sampleRate,
                                              int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setMasterLevel ((int) std::lround (masterValue->load()));
    engine.setPatch (snapshotPatch());
    engine.reset();
    monoScratch.assign ((std::size_t) juce::jmax (samplesPerBlock, 16), 0.0f);
}

void YouKnow201AudioProcessor::releaseResources() {}

bool YouKnow201AudioProcessor::isBusesLayoutSupported (
    const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
           && layouts.getMainInputChannelSet().isDisabled();
}

double YouKnow201AudioProcessor::getTailLengthSeconds() const
{
    // Honest per-patch tail: the longest amp release, plus the delay's
    // repeats down to -60 dB, plus the reverb's RT60 (the delay feeds the
    // reverb in series). The documented +/-98 % feedback extreme decays only
    // 2 % per repeat — minutes of audible tail — so the report is capped at
    // a bound offline renderers can live with.
    const Patch patch = snapshotPatch();
    using namespace youknow201::mapping;

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

void YouKnow201AudioProcessor::triggerFromUi (int note, int velocity) noexcept
{
    const auto write = uiWrite.load (std::memory_order_relaxed);
    const auto read = uiRead.load (std::memory_order_acquire);
    if (write - read >= uiQueueCapacity)
        return;
    uiQueue[write % uiQueueCapacity] = { note, juce::jlimit (1, 127, velocity) };
    uiWrite.store (write + 1, std::memory_order_release);
}

void YouKnow201AudioProcessor::releaseFromUi (int note) noexcept
{
    const auto write = uiWrite.load (std::memory_order_relaxed);
    const auto read = uiRead.load (std::memory_order_acquire);
    if (write - read >= uiQueueCapacity)
        return;
    uiQueue[write % uiQueueCapacity] = { note, 0 };
    uiWrite.store (write + 1, std::memory_order_release);
}

void YouKnow201AudioProcessor::handleController (int controller, int value)
{
    switch (controller)
    {
        case 0:
        case 32:
            // Bank select is accepted (settled CCs) but there is only the one
            // built-in program bank to select.
            return;
        case 1:  engine.setModulation (value / 127.0); return;
        case 7:  engine.setPartLevel (value / 127.0); return;
        case 10: engine.setPartPan ((value - 64) / 63.0); return;
        case 11: engine.setExpression (value / 127.0); return;
        case 64: engine.setHold (value >= 64); return;
        case 84: engine.setPortamentoControl (value); return;
        case 120: engine.allSoundOff(); return;
        case 121:
            engine.setPitchBend (0.0);
            engine.setModulation (0.0);
            engine.setExpression (1.0);
            engine.setHold (false);
            return;
        case 123:
        case 124:
        case 125:
            engine.allNotesOff();
            return;
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
        return;
    }
}

void YouKnow201AudioProcessor::handleMidiMessage (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        engine.noteOn (message.getNoteNumber(), message.getVelocity());
    else if (message.isNoteOff())
        engine.noteOff (message.getNoteNumber());
    else if (message.isPitchWheel())
        engine.setPitchBend ((message.getPitchWheelValue() - 8192) / 8192.0);
    else if (message.isController())
        handleController (message.getControllerNumber(),
                          message.getControllerValue());
    else if (message.isProgramChange())
    {
        const int program = message.getProgramChangeNumber();
        if (program >= 0 && program < getNumPrograms())
        {
            currentProgram.store (program, std::memory_order_relaxed);
            applyProgramAsync (program);
        }
    }
    else if (message.isAllNotesOff())
        engine.allNotesOff();
    else if (message.isAllSoundOff())
        engine.allSoundOff();
}

void YouKnow201AudioProcessor::applyProgramAsync (int program)
{
    // Program changes arrive on the audio thread; loading a program touches
    // every parameter, which belongs on the message thread.
    juce::MessageManager::callAsync (
        [weakThis = juce::WeakReference<YouKnow201AudioProcessor> (this), program]
        {
            if (auto* self = weakThis.get())
                self->applyProgram (program);
        });
}

void YouKnow201AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
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

    engine.setMasterLevel ((int) std::lround (
        masterValue->load (std::memory_order_relaxed)));
    engine.setPatch (snapshotPatch());

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
                            eventPosition - position);
            position = eventPosition;
        }
        handleMidiMessage (metadata.getMessage());
    }
    if (position < buffer.getNumSamples())
        engine.process (left + position, right + position,
                        buffer.getNumSamples() - position);

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            left[i] = 0.5f * (left[i] + monoScratch[(std::size_t) i]);

    activeVoices.store (engine.activeVoiceCount(), std::memory_order_relaxed);
}

int YouKnow201AudioProcessor::getNumPrograms()
{
    return (int) youknow201::factoryPatches().size();
}

int YouKnow201AudioProcessor::getCurrentProgram()
{
    return currentProgram.load (std::memory_order_relaxed);
}

void YouKnow201AudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram.store (index, std::memory_order_relaxed);
    applyProgram (index);
}

void YouKnow201AudioProcessor::applyProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    const Patch& patch =
        youknow201::factoryPatches()[(std::size_t) index].patch;

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
}

const juce::String YouKnow201AudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};
    return youknow201::factoryPatches()[(std::size_t) index].name;
}

void YouKnow201AudioProcessor::getStateInformation (
    juce::MemoryBlock& destinationData)
{
    if (auto state = parameters.copyState(); state.isValid())
    {
        state.setProperty ("program",
                           currentProgram.load (std::memory_order_relaxed),
                           nullptr);
        if (const auto xml = state.createXml())
            copyXmlToBinary (*xml, destinationData);
    }
}

void YouKnow201AudioProcessor::setStateInformation (const void* data,
                                                    int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            currentProgram.store (state.getProperty ("program", 0),
                                  std::memory_order_relaxed);
            parameters.replaceState (state);
        }
    }
}

juce::AudioProcessorEditor* YouKnow201AudioProcessor::createEditor()
{
    return new YouKnow201AudioProcessorEditor (*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YouKnow201AudioProcessor();
}
