#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace
{
constexpr std::array<const char*, 4> factoryProgramNames {
    "Factory Default", "Drop-E Metal", "Mute / Dead DI", "Blues Rock Lead"
};

struct FactoryParameterValue
{
    const char* id;
    float plainValue;
};

// These are the two measured/demonstrated deviations from the existing
// instrument defaults. Keeping them sparse makes the defaults the single
// source of truth, while setCurrentProgram() still derives every target from
// a default plus these overrides so a rig never inherits the previous patch.
constexpr std::array<FactoryParameterValue, 12> dropEMetalValues {{
    { electry::parameters::tone,          1.00f },
    { electry::parameters::stringAge,     0.10f },
    { electry::parameters::pickHardness,  0.85f },
    { electry::parameters::fingerNoise,   0.55f },
    { electry::parameters::muteDamping,   0.85f },
    { electry::parameters::velocity,      0.70f },
    { electry::parameters::output,        6.00f },
    { electry::parameters::artifacts,     0.15f },
    { electry::parameters::distortion,    0.45f },
    { electry::parameters::amp,           0.95f },
    { electry::parameters::compressor,    0.60f },
    { electry::parameters::sympathetic,   0.25f },
}};

constexpr std::array<FactoryParameterValue, 6> muteDeadDiValues {{
    { electry::parameters::pickPosition,  0.20f },
    { electry::parameters::pickHardness,  0.82f },
    { electry::parameters::fingerNoise,   0.55f },
    { electry::parameters::output,        6.00f },
    { electry::parameters::artifacts,     0.15f },
    { electry::parameters::sympathetic,   0.00f },
}};

// The host-representable version of 21-blues-rock-lead-study.wav's voicing.
// The renderer writes gain directly, while the host control is quantised to
// 0.1 dB; +3.2 dB is the nearest representable value to its 1.45 linear gain.
constexpr std::array<FactoryParameterValue, 18> bluesRockLeadValues {{
    { electry::parameters::pickupSelector, 0.00f },
    { electry::parameters::pickupType,     0.56f },
    { electry::parameters::tone,           0.78f },
    { electry::parameters::bodyResonance,  0.62f },
    { electry::parameters::stringAge,      0.20f },
    { electry::parameters::pickPosition,   0.38f },
    { electry::parameters::pickHardness,   0.52f },
    { electry::parameters::fingerNoise,    0.48f },
    { electry::parameters::bendTime,       0.15f },
    { electry::parameters::velocity,       0.90f },
    { electry::parameters::output,         3.20f },
    { electry::parameters::outputMode,     1.00f },
    { electry::parameters::distortion,     0.14f },
    { electry::parameters::amp,            0.62f },
    { electry::parameters::compressor,     0.28f },
    { electry::parameters::delay,          0.18f },
    { electry::parameters::room,           0.28f },
    { electry::parameters::sympathetic,    0.35f },
}};

std::span<const FactoryParameterValue> valuesForProgram (int index) noexcept
{
    if (index == 1)
        return dropEMetalValues;
    if (index == 2)
        return muteDeadDiValues;
    if (index == 3)
        return bluesRockLeadValues;
    return {};
}

constexpr auto factoryProgramProperty = "factoryProgram";
constexpr auto pickStyleProperty = "pickStyle";
constexpr auto playStyleProperty = "playStyle";
constexpr auto playStyleKeysHoldProperty = "playStyleKeysHold";

bool isAttackConditioningNote (int note) noexcept
{
    return electry::ElectryEngine::isKeyswitchNote (note)
        || electry::ElectryEngine::isVibratoGestureNote (note)
        || electry::ElectryEngine::isTremoloGestureNote (note)
        || electry::ElectryEngine::isMidiSoloStringNote (note)
        || electry::ElectryEngine::isMidiSoloClearNote (note);
}

bool isRpnStateMidiEvent (const juce::MidiMessageMetadata& metadata) noexcept
{
    if (metadata.data == nullptr || metadata.numBytes < 3
        || (metadata.data[0] & 0xf0u) != 0xb0u)
        return false;

    const auto controller = metadata.data[1] & 0x7fu;
    return controller == 6u || controller == 38u || controller == 98u
        || controller == 99u || controller == 100u || controller == 101u;
}

bool isAttackConditioningMidiEvent (
    const juce::MidiMessageMetadata& metadata) noexcept
{
    if (metadata.data == nullptr || metadata.numBytes < 1)
        return false;

    const auto kind = static_cast<unsigned> (metadata.data[0]) & 0xf0u;
    if (kind == 0x90u && metadata.numBytes >= 3)
    {
        const int note = static_cast<int> (metadata.data[1] & 0x7fu);
        return isAttackConditioningNote (note);
    }

    if (kind == 0x80u && metadata.numBytes >= 3)
    {
        const int note = static_cast<int> (metadata.data[1] & 0x7fu);
        return isAttackConditioningNote (note);
    }

    if (kind != 0xb0u || metadata.numBytes < 3)
        return false;

    const auto controller = metadata.data[1] & 0x7fu;
    // CC121 is the matching reset for CC2, and CC123 releases temporary HOLD
    // keys. RPN/NRPN selectors and data entry must also retain source order
    // while completing before same-sample attacks, so MPE setup cannot race a
    // member-channel Note On.
    return controller == 2u || controller == 121u || controller == 123u
        || isRpnStateMidiEvent (metadata);
}

float valueOf (const std::atomic<float>* value) noexcept
{
    return value->load (std::memory_order_relaxed);
}

juce::String percentText (float value, int)
{
    return juce::String (juce::roundToInt (value * 100.0f)) + "%";
}

float percentValue (const juce::String& text)
{
    return text.retainCharacters ("0123456789.-").getFloatValue() / 100.0f;
}

juce::String decibelsText (float value, int)
{
    return (value > 0.0f ? "+" : "") + juce::String (value, 1) + "dB";
}

float plainNumericValue (const juce::String& text)
{
    return text.retainCharacters ("0123456789.-").getFloatValue();
}

juce::String timeText (float value, int)
{
    if (value < 1.0f)
        return juce::String (juce::roundToInt (value * 1000.0f)) + " ms";
    return juce::String (value, 2) + " s";
}

// For parameters whose plain value already spans 0..100.
juce::String percentText100 (float value, int)
{
    return juce::String (juce::roundToInt (value)) + "%";
}

float timeValue (const juce::String& text)
{
    const auto value = text.retainCharacters ("0123456789.-").getFloatValue();
    return text.containsIgnoreCase ("ms") ? value / 1000.0f : value;
}

juce::AudioParameterFloatAttributes percentAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("%")
        .withStringFromValueFunction (percentText)
        .withValueFromStringFunction (percentValue);
}

juce::AudioParameterFloatAttributes morphAttributes (const char* lowName,
                                                     const char* highName)
{
    const juce::String low (lowName);
    const juce::String high (highName);
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction (
            [low, high] (float value, int)
            {
                if (value <= 0.02f)
                    return low;
                if (value >= 0.98f)
                    return high;
                return juce::String (juce::roundToInt (value * 100.0f)) + "% "
                     + high;
            })
        .withValueFromStringFunction (percentValue);
}

juce::AudioParameterFloatAttributes guitarBuildAttributes()
{
#if ELECTRY_MEASURED_BODY_RESPONSE
    static constexpr std::array<const char*, 6> names {
        "Walnut short/heavy", "Short medium-heavy", "Medium balanced",
        "Long light", "Extended heavy", "Ash extended/light"
    };
#else
    static constexpr std::array<const char*, 6> names {
        "Slab fixed", "Contoured", "Angular set", "Modern bolt",
        "Dense extended", "Neck-through"
    };
#endif

    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction (
            [] (float value, int)
            {
                const int nearest = juce::jlimit (
                    0, static_cast<int> (names.size()) - 1,
                    juce::roundToInt (value * static_cast<float> (names.size() - 1)));
                const float anchor = static_cast<float> (nearest)
                                   / static_cast<float> (names.size() - 1);
                if (std::abs(value - anchor) < 0.002f)
                    return juce::String (names[static_cast<std::size_t> (nearest)]);
                return juce::String (juce::roundToInt (value * 100.0f)) + "%";
            })
        .withValueFromStringFunction (
            [] (const juce::String& text)
            {
                for (std::size_t index = 0; index < names.size(); ++index)
                    if (text.containsIgnoreCase (names[index]))
                        return static_cast<float> (index)
                             / static_cast<float> (names.size() - 1);
                return percentValue(text);
            });
}

} // namespace

ElectryAudioProcessor::ElectryAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ELECTRY_STATE", createParameterLayout())
{
    using namespace electry::parameters;

    // Fixed, distinct player stream: repeatable contact and performance
    // timing, but not a phase-identical copy of the primary engine in DOUBLE.
    doubleEngine->setVariationSeed (0x9e3779b9u);

    parameterPointers.pickupSelector = parameters.getRawParameterValue (pickupSelector);
    parameterPointers.pickupType     = parameters.getRawParameterValue (pickupType);
    parameterPointers.tone           = parameters.getRawParameterValue (tone);
    parameterPointers.guitarBuild    = parameters.getRawParameterValue (guitarBuild);
    parameterPointers.bodyResonance  = parameters.getRawParameterValue (bodyResonance);
    parameterPointers.stringAge      = parameters.getRawParameterValue (stringAge);
    parameterPointers.pickPosition   = parameters.getRawParameterValue (pickPosition);
    parameterPointers.pickHardness   = parameters.getRawParameterValue (pickHardness);
    parameterPointers.pickNoise      = parameters.getRawParameterValue (pickNoise);
    parameterPointers.fingerNoise    = parameters.getRawParameterValue (fingerNoise);
    parameterPointers.releaseNoise   = parameters.getRawParameterValue (releaseNoise);
    parameterPointers.muteDamping    = parameters.getRawParameterValue (muteDamping);
    parameterPointers.bendTime       = parameters.getRawParameterValue (bendTime);
    parameterPointers.velocity       = parameters.getRawParameterValue (velocity);
    parameterPointers.output         = parameters.getRawParameterValue (output);
    parameterPointers.artifacts      = parameters.getRawParameterValue (artifacts);
    parameterPointers.outputMode     = parameters.getRawParameterValue (outputMode);
    parameterPointers.distortion     = parameters.getRawParameterValue (distortion);
    parameterPointers.amp            = parameters.getRawParameterValue (amp);
    parameterPointers.compressor     = parameters.getRawParameterValue (compressor);
    parameterPointers.delay          = parameters.getRawParameterValue (delay);
    parameterPointers.room           = parameters.getRawParameterValue (room);
    parameterPointers.sympathetic    = parameters.getRawParameterValue (sympathetic);
    parameterPointers.palmMute       = parameters.getRawParameterValue (palmMute);
    parameterPointers.strumSpread    = parameters.getRawParameterValue (strumSpread);
    parameterPointers.tremoloRate    = parameters.getRawParameterValue (tremoloRate);
    parameterPointers.resonanceDepth = parameters.getRawParameterValue (resonanceDepth);
    parameterPointers.ampModel       = parameters.getRawParameterValue (ampModel);

    jassert (parameterPointers.pickupSelector != nullptr
             && parameterPointers.pickupType != nullptr
             && parameterPointers.tone != nullptr
             && parameterPointers.guitarBuild != nullptr
             && parameterPointers.bendTime != nullptr
             && parameterPointers.output != nullptr
             && parameterPointers.artifacts != nullptr
             && parameterPointers.outputMode != nullptr
             && parameterPointers.sympathetic != nullptr
             && parameterPointers.palmMute != nullptr
             && parameterPointers.strumSpread != nullptr
             && parameterPointers.tremoloRate != nullptr
             && parameterPointers.resonanceDepth != nullptr
             && parameterPointers.ampModel != nullptr);
    keyboardState.addListener (this);
}

ElectryAudioProcessor::~ElectryAudioProcessor()
{
    keyboardState.removeListener (this);
}

int ElectryAudioProcessor::getNumPrograms()
{
    return static_cast<int> (factoryProgramNames.size());
}

int ElectryAudioProcessor::getCurrentProgram()
{
    return currentProgram.load (std::memory_order_relaxed);
}

void ElectryAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;

    currentProgram.store (index, std::memory_order_relaxed);
    const auto overrides = valuesForProgram (index);

    for (auto* hostParameter : getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (hostParameter);
        jassert (parameter != nullptr);
        if (parameter == nullptr)
            continue;

        auto target = parameter->getDefaultValue();
        for (const auto& value : overrides)
            if (parameter->paramID == value.id)
            {
                target = parameter->convertTo0to1 (value.plainValue);
                break;
            }

        if (! juce::approximatelyEqual (parameter->getValue(), target))
            parameter->setValueNotifyingHost (target);
    }

    // Rigs may initialize Mute Tightness and Mute Pressure, but keep both
    // keyswitch latches player-controlled. The host learns every changed
    // parameter above, then refreshes its program view.
    updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
}

const juce::String ElectryAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};
    return factoryProgramNames[static_cast<std::size_t> (index)];
}

juce::AudioProcessorValueTreeState::ParameterLayout
ElectryAudioProcessor::createParameterLayout()
{
    using namespace electry::parameters;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;
    result.reserve (28);

    // Every default below is read from the engine's own struct rather than
    // written out again here. These two lists had drifted apart: the engine's
    // defaults became a specific instrument - thick blank, heaviest set, tone
    // back, pick near the bridge - while this layout still created every new
    // plug-in instance at the old midpoints, so the shipping product and its
    // reset-to-default kept a voicing the README and the demos no longer
    // described. Deriving them makes that divergence impossible rather than
    // merely fixed.
    const electry::EngineParameters engineDefaults {};

    const auto addFloat = [&result] (const char* id, const char* name,
                                     juce::NormalisableRange<float> range, float defaultValue,
                                     juce::AudioParameterFloatAttributes attributes = {})
    {
        result.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue, std::move (attributes)));
    };
    const auto addPercent = [&addFloat] (const char* id, const char* name,
                                         float defaultValue)
    {
        addFloat (id, name, { 0.0f, 1.0f, 0.001f }, defaultValue, percentAttributes());
    };
    const auto addMorph = [&addFloat] (const char* id, const char* name,
                                       float defaultValue, const char* lowName,
                                       const char* highName)
    {
        addFloat (id, name, { 0.0f, 1.0f, 0.001f }, defaultValue,
                  morphAttributes (lowName, highName));
    };

    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { pickupSelector, 1 }, "Pickup selector",
        juce::StringArray { "Neck", "Both", "Bridge" },
        static_cast<int> (engineDefaults.pickupSelector)));
    addMorph (pickupType, "Pickup type", engineDefaults.pickupType,
              "Humbucker", "Single coil");
    addPercent (tone, "Tone", engineDefaults.toneKnob);
    addFloat (guitarBuild, "Guitar build", { 0.0f, 1.0f, 0.001f },
              electry::defaultGuitarBuild, guitarBuildAttributes());
    addPercent (bodyResonance, "Body resonance", engineDefaults.bodyResonance);
    addPercent (stringAge, "String age", engineDefaults.stringAge);

    addPercent (pickPosition, "Pick position", engineDefaults.pickPosition);
    addPercent (pickHardness, "Pick hardness", engineDefaults.pickHardness);
    addPercent (pickNoise, "Pick noise", engineDefaults.pickNoise);
    addPercent (fingerNoise, "Finger noise", engineDefaults.fingerNoise);
    addPercent (releaseNoise, "Release noise", engineDefaults.releaseNoise);
    addPercent (muteDamping, "Mute tightness", engineDefaults.muteDamping);

    auto bendRange = juce::NormalisableRange<float> { 0.04f, 2.0f, 0.0f };
    bendRange.setSkewForCentre (0.30f);
    addFloat (bendTime, "Bend time", bendRange, engineDefaults.bendTimeSeconds,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("s")
                  .withStringFromValueFunction (timeText)
                  .withValueFromStringFunction (timeValue));
    addPercent (velocity, "Velocity response", engineDefaults.velocityAmount);

    addFloat (output, "Output level", { -24.0f, 6.0f, 0.1f }, -6.0f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("dB")
                  .withStringFromValueFunction (decibelsText)
                  .withValueFromStringFunction (plainNumericValue));

    addPercent (artifacts, "Artifacts", engineDefaults.artifactAmount);
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { outputMode, 1 }, "Output mode",
        juce::StringArray { "Mono", "Stereo", "Double" },
        static_cast<int> (engineDefaults.outputMode)));

    addPercent (distortion, "Distortion", 0.0f);
    addPercent (amp, "Amp simulation", 0.0f);
    addPercent (compressor, "Compressor", 0.0f);
    addPercent (delay, "Delay", 0.0f);
    addPercent (room, "Room", 0.0f);

    addPercent (sympathetic, "Sympathetic ring", engineDefaults.sympatheticAmount);
    addPercent (palmMute, "Mute pressure", engineDefaults.palmMute);
    addFloat (strumSpread, "Strum spread", { 0.0f, 40.0f, 0.1f },
              1000.0f * engineDefaults.strumSpreadSeconds,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("ms")
                  .withStringFromValueFunction (
                      [] (float value, int)
                      {
                          if (value < 0.05f)
                              return juce::String ("Block chord");
                          return juce::String (value, 1) + " ms/string";
                      })
                  .withValueFromStringFunction (plainNumericValue));
    addFloat (resonanceDepth, "Resonance depth", { 0.0f, 100.0f, 1.0f },
              100.0f * engineDefaults.resonanceDepth,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("%")
                  .withStringFromValueFunction (percentText100)
                  .withValueFromStringFunction (plainNumericValue));
    // New parameters append to the published list so existing host automation
    // indices remain stable; editor order is independent of this layout.
    addFloat (tremoloRate, "Tremolo picking rate", { 4.0f, 20.0f, 0.1f },
              engineDefaults.tremoloRateHz,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("strokes/s")
                  .withStringFromValueFunction (
                      [] (float value, int)
                      {
                          return juce::String (value, 1) + " strokes/s";
                      })
                  .withValueFromStringFunction (plainNumericValue));
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ampModel, 1 }, "Amp voice",
        juce::StringArray { "American Clean", "British Crunch",
                            "Modern High-Gain" },
        static_cast<int> (electry::FxParameters {}.ampModel)));

    return { result.begin(), result.end() };
}

void ElectryAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // This is the prepare boundary: forget only an older request. Any Panic
    // click from here onward either reaches the still-running old block or
    // survives the preparation work for the first new block.
    panicRequested.store (false, std::memory_order_release);
    engineReady.store (false, std::memory_order_release);
    engine.prepare (sampleRate, samplesPerBlock);
    doubleEngine->prepare (sampleRate, samplesPerBlock);
    updateEngineParameters();
    sustainPedalDown = false;
    // Drop stale on-screen note events before reading the producer-side
    // latches. A style click published after this boundary remains queued for
    // the first block, so it can never be discarded after changing the UI.
    discardUiMidiEvents();
    clearHeldPlayStyles();
    clearHeldSoloStrings();
    applySoloStringMask (0);
    appliedPlayStyleKeysHold = playStyleKeysHold.load (std::memory_order_relaxed);
    appliedBasePlayStyleIndex = juce::jlimit (
        0, electry::ElectryEngine::playStyleKeyswitchCount - 1,
        playStyleIndex.load (std::memory_order_relaxed));
    resetEngineWithArticulations (
        pickStyleIndex.load (std::memory_order_relaxed),
        appliedBasePlayStyleIndex);
    mpeRpnDetector.reset();
    clearMpeNoteOwnership();
    clearVibratoGesture();
    clearTremoloGesture();
    resetPitchBends();
    engine.setResonance (0.0f);
    doubleEngine->setResonance (0.0f);
    engine.setPalmMutePressure (0.0f);
    doubleEngine->setPalmMutePressure (0.0f);
    midiMutePressureForDisplay.store (0, std::memory_order_relaxed);
    effects.prepare (sampleRate);
    updateEffectParameters();
    effects.reset();
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    sympatheticStringCount.store (0, std::memory_order_relaxed);
    publishStringVisualState();
    engineReady.store (true, std::memory_order_release);
}

void ElectryAudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    mpeRpnDetector.reset();
    resetPitchBends();
    clearMpeNoteOwnership();
    sustainPedalDown = false;
    engine.setSustainPedal (false);
    doubleEngine->setSustainPedal (false);
    engine.setResonance (0.0f);
    doubleEngine->setResonance (0.0f);
    engine.setPalmMutePressure (0.0f);
    doubleEngine->setPalmMutePressure (0.0f);
    midiMutePressureForDisplay.store (0, std::memory_order_relaxed);
    clearVibratoGesture();
    clearTremoloGesture();
    engine.allNotesOff();
    doubleEngine->allNotesOff();
    engine.reset();
    doubleEngine->reset();
    effects.reset();
    discardUiMidiEvents();
    clearHeldPlayStyles();
    clearHeldSoloStrings();
    applySoloStringMask (0);
    appliedBasePlayStyleIndex = juce::jlimit (
        0, electry::ElectryEngine::playStyleKeyswitchCount - 1,
        playStyleIndex.load (std::memory_order_relaxed));
    effectivePlayStyleIndex.store (appliedBasePlayStyleIndex,
                                   std::memory_order_relaxed);
    panicRequested.store (false, std::memory_order_release);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    sympatheticStringCount.store (0, std::memory_order_relaxed);
    displaySampleRate.store (0.0, std::memory_order_relaxed);
    publishStringVisualState();
}

bool ElectryAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ElectryAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! engineReady.load (std::memory_order_acquire))
        return;

    updateEngineParameters();
    updateEffectParameters();
    synchronisePlayStyleKeyMode();

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        sustainPedalDown = false;
        // Preserve an already-clicked style while discarding queued playable
        // notes. Discard first: a later click remains queued, instead of being
        // lost after its immediate display latch has changed.
        discardUiMidiEvents();
        clearHeldPlayStyles();
        clearHeldSoloStrings();
        applySoloStringMask (0);
        clearVibratoGesture();
        clearTremoloGesture();
        appliedBasePlayStyleIndex = juce::jlimit (
            0, electry::ElectryEngine::playStyleKeyswitchCount - 1,
            playStyleIndex.load (std::memory_order_relaxed));
        resetEngineWithArticulations (
            pickStyleIndex.load (std::memory_order_relaxed),
            appliedBasePlayStyleIndex);
        clearMpeNoteOwnership();
        effects.reset();
    }

    // GUI notes and articulation clicks enter through a bounded lock-free
    // queue and start at the next block boundary. Snapshot the producer once:
    // anything arriving during this callback belongs to the next block.
    const auto uiBegin = uiReadIndex.load (std::memory_order_relaxed);
    const auto uiEnd = uiWriteIndex.load (std::memory_order_acquire);
    NoteOnBatch uiBatch;
    dispatchUiMidiEventPass (uiBegin, uiEnd, true, uiBatch);

    // A normal sample-zero host group and the GUI snapshot describe one attack
    // boundary. Let both sources condition it before either source's playable
    // notes. If a host supplies out-of-range negative positions, retain the
    // old source order instead of pulling a later raw-zero event ahead of them.
    const auto firstHostEvent = midiMessages.begin();
    bool uiRemainderPending = firstHostEvent != midiMessages.end()
                           && (*firstHostEvent).samplePosition == 0;
    if (! uiRemainderPending)
    {
        dispatchUiMidiEventPass (uiBegin, uiEnd, false, uiBatch);
        flushNoteOnBatch (uiBatch);
        uiReadIndex.store (uiEnd, std::memory_order_release);
    }

    const auto numSamples = buffer.getNumSamples();
    int renderedTo = 0;

    // The nominal acoustic delay is fixed in time by the engine, not by the
    // host's callback size. Render, amplify and return audio in causal chunks
    // no longer than that delay; event boundaries may make a chunk shorter,
    // but the engine's FIFO keeps the acoustic latency unchanged.
    const int feedbackChunkSize = engine.getAcousticReturnDelaySamples();
    const auto renderFeedbackPath = [&] (int startSample, int sampleCount)
    {
        while (sampleCount > 0)
        {
            const int chunk = std::min(feedbackChunkSize, sampleCount);
            auto* left = buffer.getWritePointer (0, startSample);
            auto* right = buffer.getWritePointer (1, startSample);
            renderEngines (left, right, chunk);
            effects.process (left, right, chunk);

            if (doubleModeActive)
            {
                engine.pushAcousticReturn (left, nullptr, chunk);
                doubleEngine->pushAcousticReturn (right, nullptr, chunk);
            }
            else
            {
                engine.pushAcousticReturn (left, right, chunk);
            }

            startSample += chunk;
            sampleCount -= chunk;
        }
    };

    for (auto event = midiMessages.begin(); event != midiMessages.end();)
    {
        const auto eventSample = juce::jlimit (0, numSamples,
                                               (*event).samplePosition);

        if (eventSample > renderedTo)
        {
            renderFeedbackPath (renderedTo, eventSample - renderedTo);
            renderedTo = eventSample;
        }

        auto groupEnd = event;
        while (groupEnd != midiMessages.end()
               && (*groupEnd).samplePosition == (*event).samplePosition)
            ++groupEnd;

        NoteOnBatch groupBatch;
        bool acceptedMpeRpn = false;

        // Hosts are free to store simultaneous MIDI in either insertion order.
        // A keyswitch or CC2 therefore conditions every attack at its timestamp,
        // rather than only attacks that happened to follow it in the buffer.
        // The two iterator walks allocate nothing and preserve input order within
        // both the conditioning events and everything else.
        for (auto current = event; current != groupEnd; ++current)
        {
            const auto metadata = *current;
            if (isAttackConditioningMidiEvent (metadata))
            {
                if (isRpnStateMidiEvent (metadata))
                    acceptedMpeRpn = processMpeController (
                                          metadata.data, metadata.numBytes)
                                      || acceptedMpeRpn;
                else
                    dispatchMidiData (metadata.data, metadata.numBytes);
            }
        }

        // Stable partition across event sources at the block boundary:
        // GUI conditioning, host conditioning, GUI remainder, host remainder.
        // Do not release the queue slots until both GUI passes have read them.
        if (uiRemainderPending && (*event).samplePosition == 0)
        {
            dispatchUiMidiEventPass (uiBegin, uiEnd, false, groupBatch);
            uiReadIndex.store (uiEnd, std::memory_order_release);
            uiRemainderPending = false;
        }

        for (auto current = event; current != groupEnd; ++current)
        {
            const auto metadata = *current;
            if (isAttackConditioningMidiEvent (metadata))
            {
                // RPN state has already been consumed by the conditioning
                // pass, but an ignored controller historically split the two
                // surrounding note batches. Retain that boundary whenever no
                // zone (or an unrelated RPN) leaves ordinary MIDI in charge.
                if (isRpnStateMidiEvent (metadata)
                    && ! (acceptedMpeRpn && mpeZoneLayout.isActive()))
                    flushNoteOnBatch (groupBatch);
                continue;
            }

            const auto* data = metadata.data;
            const bool ordinaryPositiveNoteOn =
                data != nullptr && metadata.numBytes >= 3
                && (data[0] & 0xf0u) == 0x90u
                && data[2] != 0u;
            if (ordinaryPositiveNoteOn)
            {
                const int midiChannel = static_cast<int> (data[0] & 0x0fu) + 1;
                batchOrDispatchNoteOn (
                    static_cast<int> (data[1] & 0x7fu),
                    static_cast<float> (data[2] & 0x7fu) / 127.0f,
                    expressionIdForNoteOn (midiChannel), midiChannel, false,
                    groupBatch);
                continue;
            }

            // Pressure is deliberately unassigned. Ignore it without splitting
            // a simultaneous chord into separate allocation batches.
            const auto kind = data != nullptr && metadata.numBytes >= 1
                ? static_cast<unsigned> (data[0]) & 0xf0u : 0u;
            if (kind == 0xa0u || kind == 0xd0u)
                continue;
            if (kind == 0xb0u && metadata.numBytes >= 3
                && (data[1] & 0x7fu) == 74u)
            {
                const int midiChannel =
                    static_cast<int> (data[0] & 0x0fu) + 1;
                if (mpeZoneLayout.getLowerZone().isUsing (midiChannel)
                    || mpeZoneLayout.getUpperZone().isUsing (midiChannel))
                    continue;
            }

            // Anything that can change ownership or performance state splits
            // the chord. Dispatch it in source order between the two batches.
            flushNoteOnBatch (groupBatch);
            dispatchMidiData (data, metadata.numBytes);
        }
        flushNoteOnBatch (groupBatch);

        event = groupEnd;
    }

    if (renderedTo < numSamples)
        renderFeedbackPath (renderedTo, numSamples - renderedTo);

    activeVoiceCount.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);
    sympatheticStringCount.store (engine.getSympatheticStringCount(),
                                  std::memory_order_relaxed);
    publishStringVisualState();
}

void ElectryAudioProcessor::dispatchMidiData (const juce::uint8* data, int numBytes) noexcept
{
    if (data == nullptr || numBytes < 1)
        return;

    const auto status = static_cast<unsigned> (data[0]);
    const auto kind = status & 0xf0u;
    const int midiChannel = static_cast<int> (status & 0x0fu) + 1;

    if (kind == 0x90u && numBytes >= 3)
    {
        if (data[2] != 0)
            dispatchNoteOn (static_cast<int> (data[1] & 0x7fu),
                            static_cast<float> (data[2] & 0x7fu) / 127.0f,
                            false);
        else
            dispatchHostNoteOff (static_cast<int> (data[1] & 0x7fu),
                                 midiChannel);
    }
    else if (kind == 0x80u && numBytes >= 3)
    {
        dispatchHostNoteOff (static_cast<int> (data[1] & 0x7fu), midiChannel);
    }
    else if (kind == 0xb0u && numBytes >= 3)
    {
        static_cast<void> (processMpeController (data, numBytes));
        const auto controller = data[1] & 0x7fu;
        const auto controllerValue = data[2] & 0x7fu;
        if (controller == 64u)
        {
            sustainPedalDown = controllerValue >= 64u;
            engine.setSustainPedal (sustainPedalDown);
            doubleEngine->setSustainPedal (sustainPedalDown);
        }
        else if (controller == 1u)
        {
            // The modulation wheel is the performance resonance: it lifts the
            // sympathetic coupling toward total and opens the acoustic
            // feedback path, so a distorted tone can be played into a howl.
            engine.setResonance (static_cast<float> (controllerValue) / 127.0f);
            doubleEngine->setResonance (
                static_cast<float> (controllerValue) / 127.0f);
        }
        else if (controller == 2u)
        {
            // Breath/CC2 is the performable side of the Mute Pressure parameter:
            // it adds bridge-hand pressure without needing automation.
            midiMutePressureForDisplay.store (
                static_cast<int> (controllerValue), std::memory_order_relaxed);
            engine.setPalmMutePressure (
                static_cast<float> (controllerValue) / 127.0f);
            doubleEngine->setPalmMutePressure (
                static_cast<float> (controllerValue) / 127.0f);
        }
        else if (controller == 121u)
        {
            sustainPedalDown = false;
            mpeRpnDetector.reset();
            resetPitchBends();
            engine.setResonance (0.0f);
            doubleEngine->setResonance (0.0f);
            engine.setPalmMutePressure (0.0f);
            doubleEngine->setPalmMutePressure (0.0f);
            midiMutePressureForDisplay.store (0, std::memory_order_relaxed);
            engine.setSustainPedal (false);
            doubleEngine->setSustainPedal (false);
        }
        else if (controller == 120u)
        {
            // MIDI All Sound Off is an immediate audio mute. Unlike the
            // front-panel Panic, it preserves controller/key state: parameter
            // targets, selected latches and a physically held play-style key
            // survive; Alternate deliberately restarts on Down.
            // Producer/display latches can already describe a GUI event beyond
            // this callback's queue snapshot. Preserve only styles that the
            // audio thread has actually applied at the CC120 boundary.
            const int appliedPickStyle = static_cast<int> (
                engine.getCurrentPickStyle());
            const int appliedPlayStyle = static_cast<int> (
                engine.getCurrentPlayStyle());
            resetEngineWithArticulations (appliedPickStyle, appliedPlayStyle);
            clearMpeNoteOwnership();
            refreshMpePitchBends();
            engine.setSustainPedal (sustainPedalDown);
            doubleEngine->setSustainPedal (sustainPedalDown);
            effects.reset();
        }
        else if (controller == 123u)
        {
            // All Notes Off releases the held strings so the natural damped
            // ring-out remains musical. In HOLD mode the play-style
            // keyswitches are notes too, so they return to the saved base.
            clearHeldPlayStyles();
            if (appliedPlayStyleKeysHold)
            {
                clearHeldSoloStrings();
                applySoloStringMask (0);
            }
            clearVibratoGesture();
            clearTremoloGesture();
            applyPlayStyle (appliedBasePlayStyleIndex);
            engine.allNotesOff();
            doubleEngine->allNotesOff();
            clearMpeNoteOwnership();
        }
    }
    // Pressure is deliberately unassigned: mapping either channel pressure or
    // one note's poly pressure to global pitch pulls otherwise tuned chords
    // apart without a visible control.
    else if (kind == 0xe0u && numBytes >= 3)
    {
        const auto bend = decodePitchBend14 (data[1], data[2]);
        dispatchPitchWheel (midiChannel, bend);
    }
}

electry::ElectryEngine::ExpressionId
ElectryAudioProcessor::expressionIdForNoteOn (int midiChannel) const noexcept
{
    const auto lower = mpeZoneLayout.getLowerZone();
    const auto upper = mpeZoneLayout.getUpperZone();
    if ((lower.isActive() && lower.isUsingChannelAsMemberChannel (midiChannel))
        || (upper.isActive()
            && upper.isUsingChannelAsMemberChannel (midiChannel)))
        return static_cast<electry::ElectryEngine::ExpressionId> (midiChannel);
    return electry::ElectryEngine::legacyExpressionId;
}

void ElectryAudioProcessor::dispatchHostNoteOff (int note,
                                                  int midiChannel) noexcept
{
    // Keyswitches, gestures and repick commands are global performance
    // controls, even when a controller sends them on an MPE member channel.
    // Only playable notes participate in per-channel string ownership.
    if (! electry::ElectryEngine::isMidiPlayableNote (note))
    {
        dispatchNoteOff (note);
        return;
    }

    const auto currentExpressionId = expressionIdForNoteOn (midiChannel);
    const int recordedOwner = takeNoteOwnership (
        note, midiChannel,
        currentExpressionId == electry::ElectryEngine::legacyExpressionId);
    const auto expressionId = recordedOwner >= 0
        ? static_cast<electry::ElectryEngine::ExpressionId> (recordedOwner)
        : currentExpressionId;
    const int engineNote = note - electry::ElectryEngine::octaveShiftSemitones;
    if (expressionId != electry::ElectryEngine::legacyExpressionId)
    {
        engine.noteOff (engineNote, expressionId);
        doubleEngine->noteOff (engineNote, expressionId);
        if (expressionId <= electry::ElectryEngine::maximumExpressionId)
        {
            auto& total = outstandingMpeChannelNotes[
                static_cast<std::size_t> (expressionId - 1)];
            if (total != 0 && --total == 0)
                midiPitchBends[static_cast<std::size_t> (expressionId - 1)] =
                    0.0f;
        }
        return;
    }
    engine.noteOff (engineNote, electry::ElectryEngine::legacyExpressionId);
    doubleEngine->noteOff (engineNote, electry::ElectryEngine::legacyExpressionId);
}

bool ElectryAudioProcessor::processMpeController (const juce::uint8* data,
                                                  int numBytes) noexcept
{
    if (data == nullptr || numBytes < 3)
        return false;

    const auto oldLower = mpeZoneLayout.getLowerZone();
    const auto oldUpper = mpeZoneLayout.getUpperZone();
    const int midiChannel = static_cast<int> (data[0] & 0x0fu) + 1;
    const int controller = static_cast<int> (data[1] & 0x7fu);
    const int value = static_cast<int> (data[2] & 0x7fu);
    const auto parsed = mpeRpnDetector.tryParse (midiChannel, controller, value);
    bool accepted = false;
    bool exactRangeChanged = false;
    if (parsed.has_value() && ! parsed->isNRPN)
    {
        if (parsed->parameterNumber
            == juce::MPEMessages::zoneLayoutMessagesRpnNumber)
        {
            if (parsed->value < 16)
            {
                if (parsed->channel == 1)
                {
                    mpeZoneLayout.setLowerZone (parsed->value);
                    exactRangeChanged = ! juce::approximatelyEqual (
                                            lowerPerNotePitchBendRange, 48.0f)
                                     || ! juce::approximatelyEqual (
                                            lowerMasterPitchBendRange, 2.0f);
                    lowerPerNotePitchBendRange = 48.0f;
                    lowerMasterPitchBendRange = 2.0f;
                    accepted = true;
                }
                else if (parsed->channel == 16)
                {
                    mpeZoneLayout.setUpperZone (parsed->value);
                    exactRangeChanged = ! juce::approximatelyEqual (
                                            upperPerNotePitchBendRange, 48.0f)
                                     || ! juce::approximatelyEqual (
                                            upperMasterPitchBendRange, 2.0f);
                    upperPerNotePitchBendRange = 48.0f;
                    upperMasterPitchBendRange = 2.0f;
                    accepted = true;
                }
            }
        }
        else if (parsed->parameterNumber == 0)
        {
            // JUCE's public zone model stores only the whole-semitone MSB.
            // Keep that integer for routing and retain RPN 0's cents LSB in
            // the performed range used below.
            const int range = juce::jlimit (
                0, 96, parsed->is14BitValue ? parsed->value / 128
                                            : parsed->value);
            const float exactRange = juce::jlimit (
                0.0f, 96.0f,
                static_cast<float> (range)
                    + (parsed->is14BitValue
                           ? 0.01f * static_cast<float> (parsed->value % 128)
                           : 0.0f));
            if (parsed->channel == 1)
            {
                const auto zone = mpeZoneLayout.getLowerZone();
                mpeZoneLayout.setLowerZone (zone.numMemberChannels,
                                            zone.perNotePitchbendRange, range);
                exactRangeChanged = ! juce::approximatelyEqual (
                    lowerMasterPitchBendRange, exactRange);
                lowerMasterPitchBendRange = exactRange;
                accepted = true;
            }
            else if (parsed->channel == 16)
            {
                const auto zone = mpeZoneLayout.getUpperZone();
                mpeZoneLayout.setUpperZone (zone.numMemberChannels,
                                            zone.perNotePitchbendRange, range);
                exactRangeChanged = ! juce::approximatelyEqual (
                    upperMasterPitchBendRange, exactRange);
                upperMasterPitchBendRange = exactRange;
                accepted = true;
            }
            else
            {
                const auto lower = mpeZoneLayout.getLowerZone();
                const auto upper = mpeZoneLayout.getUpperZone();
                if (lower.isUsingChannelAsMemberChannel (parsed->channel))
                {
                    mpeZoneLayout.setLowerZone (lower.numMemberChannels, range,
                                                lower.masterPitchbendRange);
                    exactRangeChanged = ! juce::approximatelyEqual (
                        lowerPerNotePitchBendRange, exactRange);
                    lowerPerNotePitchBendRange = exactRange;
                    accepted = true;
                }
                else if (upper.isUsingChannelAsMemberChannel (parsed->channel))
                {
                    mpeZoneLayout.setUpperZone (upper.numMemberChannels, range,
                                                upper.masterPitchbendRange);
                    exactRangeChanged = ! juce::approximatelyEqual (
                        upperPerNotePitchBendRange, exactRange);
                    upperPerNotePitchBendRange = exactRange;
                    accepted = true;
                }
            }
        }
    }
    if (oldLower != mpeZoneLayout.getLowerZone()
        || oldUpper != mpeZoneLayout.getUpperZone() || exactRangeChanged)
        refreshMpePitchBends();
    return accepted;
}

void ElectryAudioProcessor::dispatchPitchWheel (int midiChannel,
                                                 float bend) noexcept
{
    if (midiChannel < 1 || midiChannel > 16)
        return;

    midiPitchBends[static_cast<std::size_t> (midiChannel - 1)] = bend;
    const std::array zones { mpeZoneLayout.getLowerZone(),
                             mpeZoneLayout.getUpperZone() };
    for (const auto& zone : zones)
    {
        if (! zone.isActive())
            continue;

        if (zone.isUsingChannelAsMemberChannel (midiChannel))
        {
            refreshMpePitchBend (midiChannel);
            return;
        }

        if (midiChannel == zone.getMasterChannel())
        {
            refreshMpePitchBends();
            return;
        }
    }

    engine.setPitchBend (bend);
    doubleEngine->setPitchBend (bend);
}

void ElectryAudioProcessor::refreshMpePitchBend (int midiChannel) noexcept
{
    if (midiChannel < 1 || midiChannel > 16)
        return;

    float memberTarget = 0.0f;
    float masterTarget = 0.0f;
    const std::array zones { mpeZoneLayout.getLowerZone(),
                             mpeZoneLayout.getUpperZone() };
    for (const auto& zone : zones)
    {
        if (! zone.isActive()
            || ! zone.isUsingChannelAsMemberChannel (midiChannel))
            continue;
        const bool lower = zone.getMasterChannel() == 1;
        memberTarget = midiPitchBends[static_cast<std::size_t> (
                           midiChannel - 1)]
                     * (lower ? lowerPerNotePitchBendRange
                              : upperPerNotePitchBendRange);
        masterTarget = midiPitchBends[static_cast<std::size_t> (
                           zone.getMasterChannel() - 1)]
                     * (lower ? lowerMasterPitchBendRange
                              : upperMasterPitchBendRange);
        break;
    }
    const auto expressionId =
        static_cast<electry::ElectryEngine::ExpressionId> (midiChannel);
    engine.setExpressionPitchBend (expressionId, memberTarget);
    engine.setExpressionMasterPitchBend (expressionId, masterTarget);
    doubleEngine->setExpressionPitchBend (expressionId, memberTarget);
    doubleEngine->setExpressionMasterPitchBend (expressionId, masterTarget);
}

void ElectryAudioProcessor::refreshMpePitchBends() noexcept
{
    for (int midiChannel = 1; midiChannel <= 16; ++midiChannel)
        refreshMpePitchBend (midiChannel);
}

void ElectryAudioProcessor::clearMpeNoteOwnership() noexcept
{
    for (std::size_t channel = 0; channel < outstandingMpeChannelNotes.size();
         ++channel)
    {
        if (outstandingMpeChannelNotes[channel] == 0)
            continue;
        midiPitchBends[channel] = 0.0f;
    }
    outstandingMpeChannelNotes.fill (0);
    for (auto& note : outstandingNoteOwners)
        note.size = 0;
}

bool ElectryAudioProcessor::recordNoteOwnership (
    int note, int midiChannel,
    electry::ElectryEngine::ExpressionId expressionId) noexcept
{
    if (note < 0 || note >= 128 || midiChannel < 0 || midiChannel > 16)
        return false;

    auto& queue = outstandingNoteOwners[static_cast<std::size_t> (note)];
    if (queue.size == queue.owners.size())
        return false;
    queue.owners[queue.size++] = {
        static_cast<std::uint8_t> (midiChannel), expressionId
    };
    if (expressionId != electry::ElectryEngine::legacyExpressionId
        && midiChannel >= 1)
    {
        auto& total = outstandingMpeChannelNotes[
            static_cast<std::size_t> (midiChannel - 1)];
        if (total == 0)
        {
            // MPE member dimensions return to their defaults after the last
            // owner. Keep a released tail at its performed pitch, then snap
            // the shared channel state only when a new finger claims it.
            refreshMpePitchBend (midiChannel);
            engine.snapExpressionPitchBendToTarget (expressionId);
            doubleEngine->snapExpressionPitchBendToTarget (expressionId);
        }
        if (total < std::numeric_limits<std::uint16_t>::max())
            ++total;
    }
    return true;
}

int ElectryAudioProcessor::takeNoteOwnership (int note, int midiChannel,
                                               bool allowAnyLegacy) noexcept
{
    if (note < 0 || note >= 128)
        return -1;
    auto& queue = outstandingNoteOwners[static_cast<std::size_t> (note)];
    int selected = -1;
    for (int index = 0; index < static_cast<int> (queue.size); ++index)
    {
        const auto& owner = queue.owners[static_cast<std::size_t> (index)];
        if (owner.midiChannel == midiChannel)
        {
            selected = index;
            break;
        }
    }
    if (selected < 0 && allowAnyLegacy)
    {
        for (int index = 0; index < static_cast<int> (queue.size); ++index)
        {
            if (queue.owners[static_cast<std::size_t> (index)].expressionId
                == electry::ElectryEngine::legacyExpressionId)
            {
                selected = index;
                break;
            }
        }
    }
    if (selected < 0)
        return -1;

    const int expressionId = queue.owners[static_cast<std::size_t> (selected)]
                                 .expressionId;
    for (int index = selected + 1; index < static_cast<int> (queue.size);
         ++index)
        queue.owners[static_cast<std::size_t> (index - 1)] =
            queue.owners[static_cast<std::size_t> (index)];
    --queue.size;
    return expressionId;
}

void ElectryAudioProcessor::resetPitchBends() noexcept
{
    midiPitchBends.fill (0.0f);
    engine.setPitchBend (0.0f);
    doubleEngine->setPitchBend (0.0f);
    refreshMpePitchBends();
}

void ElectryAudioProcessor::dispatchNoteOn (
    int note, float velocity, bool selectsBaseArticulation) noexcept
{
    if (electry::ElectryEngine::isVibratoGestureNote (note))
    {
        if (velocity <= 0.0f)
        {
            dispatchNoteOff (note);
            return;
        }
        if (vibratoGestureOwners < std::numeric_limits<std::uint16_t>::max())
            ++vibratoGestureOwners;
        applyVibratoGesture (velocity);
        return;
    }

    if (electry::ElectryEngine::isTremoloGestureNote (note))
    {
        if (velocity <= 0.0f)
        {
            dispatchNoteOff (note);
            return;
        }
        if (tremoloGestureOwners < std::numeric_limits<std::uint16_t>::max())
            ++tremoloGestureOwners;
        applyTremoloGesture (velocity);
        return;
    }

    if (electry::ElectryEngine::isMidiSoloClearNote (note))
    {
        clearHeldSoloStrings();
        applySoloStringMask (0);
        return;
    }

    if (electry::ElectryEngine::isMidiSoloStringNote (note))
    {
        const int stringIndex = note - electry::ElectryEngine::firstMidiSoloStringNote;
        if (appliedPlayStyleKeysHold)
        {
            auto& count = heldSoloStringCounts[static_cast<std::size_t> (stringIndex)];
            if (count < std::numeric_limits<std::uint16_t>::max())
                ++count;
            applySoloStringMask (computeHeldSoloStringMask());
        }
        else
        {
            const auto toggleBit = static_cast<std::uint8_t> (1u << stringIndex);
            const std::uint8_t nextMask = latchedSoloStringMask.load (std::memory_order_relaxed) ^ toggleBit;
            latchedSoloStringMask.store (nextMask, std::memory_order_relaxed);
            applySoloStringMask (nextMask);
        }
        return;
    }

    const int articulation = note - electry::ElectryEngine::firstKeyswitchNote;
    if (articulation < 0
        || articulation >= electry::ElectryEngine::keyswitchCount)
    {
        if (electry::ElectryEngine::isMidiPlayableNote (note)
            || electry::ElectryEngine::isMidiRepickNote (note))
        {
            const int engineNote = note - electry::ElectryEngine::octaveShiftSemitones;
            engine.noteOn (engineNote, velocity);
            if (doubleModeActive)
                doubleEngine->noteOn (engineNote, velocity);
        }
        return;
    }

    if (articulation < electry::ElectryEngine::pickStyleKeyswitchCount)
    {
        if (! selectsBaseArticulation)
            latchArticulation (articulation);
        engine.noteOn (note, velocity);
        doubleEngine->noteOn (note, velocity);
        return;
    }

    const int style = articulation
                    - electry::ElectryEngine::pickStyleKeyswitchCount;
    if (selectsBaseArticulation)
    {
        appliedBasePlayStyleIndex = style;
        if (! appliedPlayStyleKeysHold || latestHeldPlayStyle() < 0)
            applyPlayStyle (style);
        return;
    }

    if (! appliedPlayStyleKeysHold)
    {
        playStyleIndex.store (style, std::memory_order_relaxed);
        appliedBasePlayStyleIndex = style;
        applyPlayStyle (style);
        return;
    }

    auto& count = heldPlayStyleCounts[static_cast<std::size_t> (style)];
    if (count < std::numeric_limits<std::uint16_t>::max())
        ++count;
    heldPlayStyleOrder[static_cast<std::size_t> (style)] = ++heldPlayStyleSequence;
    applyPlayStyle (style);
}

void ElectryAudioProcessor::dispatchNoteOff (int note) noexcept
{
    if (electry::ElectryEngine::isVibratoGestureNote (note))
    {
        if (vibratoGestureOwners == 0)
            return;
        if (--vibratoGestureOwners == 0)
            applyVibratoGesture (0.0f);
        return;
    }

    if (electry::ElectryEngine::isTremoloGestureNote (note))
    {
        if (tremoloGestureOwners == 0)
            return;
        if (--tremoloGestureOwners == 0)
            applyTremoloGesture (0.0f);
        return;
    }

    if (electry::ElectryEngine::isMidiSoloClearNote (note))
        return;

    if (electry::ElectryEngine::isMidiSoloStringNote (note))
    {
        if (appliedPlayStyleKeysHold)
        {
            const int stringIndex = note - electry::ElectryEngine::firstMidiSoloStringNote;
            auto& count = heldSoloStringCounts[static_cast<std::size_t> (stringIndex)];
            if (count > 0)
            {
                --count;
                applySoloStringMask (computeHeldSoloStringMask());
            }
        }
        return;
    }

    if (electry::ElectryEngine::isMidiPlayableNote (note))
        static_cast<void> (takeNoteOwnership (note, 0, true));

    const int style = note - electry::ElectryEngine::firstPlayStyleKeyswitchNote;
    if (! appliedPlayStyleKeysHold || style < 0
        || style >= electry::ElectryEngine::playStyleKeyswitchCount)
    {
        const int engineNote = electry::ElectryEngine::isMidiPlayableNote (note)
            ? note - electry::ElectryEngine::octaveShiftSemitones
            : note;
        engine.noteOff (engineNote);
        doubleEngine->noteOff (engineNote);
        return;
    }

    auto& count = heldPlayStyleCounts[static_cast<std::size_t> (style)];
    if (count == 0)
        return;
    --count;
    if (count == 0)
        heldPlayStyleOrder[static_cast<std::size_t> (style)] = 0;

    const int held = latestHeldPlayStyle();
    applyPlayStyle (held >= 0 ? held : appliedBasePlayStyleIndex);
}

void ElectryAudioProcessor::applyPlayStyle (int styleIndex) noexcept
{
    styleIndex = juce::jlimit (
        0, electry::ElectryEngine::playStyleKeyswitchCount - 1, styleIndex);
    engine.noteOn (electry::ElectryEngine::firstPlayStyleKeyswitchNote
                       + styleIndex, 1.0f);
    doubleEngine->noteOn (electry::ElectryEngine::firstPlayStyleKeyswitchNote
                              + styleIndex, 1.0f);
    effectivePlayStyleIndex.store (styleIndex, std::memory_order_relaxed);
}

int ElectryAudioProcessor::latestHeldPlayStyle() const noexcept
{
    int latest = -1;
    std::uint64_t latestOrder = 0;
    for (int style = 0;
         style < electry::ElectryEngine::playStyleKeyswitchCount; ++style)
    {
        const auto index = static_cast<std::size_t> (style);
        if (heldPlayStyleCounts[index] != 0
            && heldPlayStyleOrder[index] > latestOrder)
        {
            latest = style;
            latestOrder = heldPlayStyleOrder[index];
        }
    }
    return latest;
}

void ElectryAudioProcessor::clearHeldPlayStyles() noexcept
{
    heldPlayStyleCounts.fill (0);
    heldPlayStyleOrder.fill (0);
    heldPlayStyleSequence = 0;
}

void ElectryAudioProcessor::applySoloStringMask (std::uint8_t mask) noexcept
{
    engine.setSoloStringMask (mask);
    if (doubleModeActive)
        doubleEngine->setSoloStringMask (mask);
    soloStringMaskForDisplay.store (mask, std::memory_order_relaxed);
}

std::uint8_t ElectryAudioProcessor::computeHeldSoloStringMask() const noexcept
{
    std::uint8_t mask = 0;
    for (int s = 0; s < electry::ElectryEngine::stringCount; ++s)
    {
        if (heldSoloStringCounts[static_cast<std::size_t> (s)] > 0)
            mask |= static_cast<std::uint8_t> (1u << s);
    }
    return mask;
}

void ElectryAudioProcessor::clearHeldSoloStrings() noexcept
{
    heldSoloStringCounts.fill (0);
    latchedSoloStringMask.store (0, std::memory_order_relaxed);
}

void ElectryAudioProcessor::applyVibratoGesture (float amount) noexcept
{
    amount = juce::jlimit (0.0f, 1.0f, std::isfinite (amount) ? amount : 0.0f);
    engine.setVibrato (amount);
    doubleEngine->setVibrato (amount);
    vibratoGestureForDisplay.store (
        juce::roundToInt (127.0f * amount), std::memory_order_relaxed);
}

void ElectryAudioProcessor::clearVibratoGesture() noexcept
{
    vibratoGestureOwners = 0;
    applyVibratoGesture (0.0f);
}

void ElectryAudioProcessor::applyTremoloGesture (float velocity) noexcept
{
    velocity = juce::jlimit (0.0f, 1.0f,
                             std::isfinite (velocity) ? velocity : 0.0f);
    if (velocity > 0.0f)
    {
        engine.beginTremoloPicking (velocity);
        doubleEngine->beginTremoloPicking (velocity);
    }
    else
    {
        engine.endTremoloPicking();
        doubleEngine->endTremoloPicking();
    }
    tremoloGestureForDisplay.store (
        juce::roundToInt (127.0f * velocity), std::memory_order_relaxed);
}

void ElectryAudioProcessor::clearTremoloGesture() noexcept
{
    tremoloGestureOwners = 0;
    applyTremoloGesture (0.0f);
}

void ElectryAudioProcessor::synchronisePlayStyleKeyMode() noexcept
{
    const bool requested = playStyleKeysHold.load (std::memory_order_relaxed);
    if (requested == appliedPlayStyleKeysHold)
        return;

    appliedPlayStyleKeysHold = requested;
    clearHeldPlayStyles();
    clearHeldSoloStrings();
    applySoloStringMask (0);
    applyPlayStyle (appliedBasePlayStyleIndex);
}

void ElectryAudioProcessor::resetEngineWithArticulations (int pickStyle,
                                                          int playStyle) noexcept
{
    engine.reset();
    doubleEngine->reset();
    engine.noteOn (electry::ElectryEngine::firstKeyswitchNote
                       + pickStyle, 1.0f);
    doubleEngine->noteOn (electry::ElectryEngine::firstKeyswitchNote
                              + pickStyle, 1.0f);
    engine.noteOn (electry::ElectryEngine::firstPlayStyleKeyswitchNote
                       + playStyle, 1.0f);
    doubleEngine->noteOn (electry::ElectryEngine::firstPlayStyleKeyswitchNote
                              + playStyle, 1.0f);
    effectivePlayStyleIndex.store (playStyle, std::memory_order_relaxed);
    const auto mask = soloStringMaskForDisplay.load (std::memory_order_relaxed);
    engine.setSoloStringMask (mask);
    doubleEngine->setSoloStringMask (mask);
}

float ElectryAudioProcessor::decodePitchBend14 (juce::uint8 data1, juce::uint8 data2) noexcept
{
    const auto value14 = static_cast<int> (data1 & 0x7fu)
                       | (static_cast<int> (data2 & 0x7fu) << 7);
    const auto bend = value14 < 8192
        ? static_cast<float> (value14 - 8192) / 8192.0f
        : static_cast<float> (value14 - 8192) / 8191.0f;
    return juce::jlimit (-1.0f, 1.0f, bend);
}

void ElectryAudioProcessor::renderEngines (float* left, float* right,
                                           int numSamples) noexcept
{
    engine.process (left, right, numSamples);
    if (doubleModeActive)
        doubleEngine->process (right, right, numSamples);
}

void ElectryAudioProcessor::updateEngineParameters() noexcept
{
    electry::EngineParameters next;
    const auto selector = juce::jlimit (0, 2,
        juce::roundToInt (valueOf (parameterPointers.pickupSelector)));
    next.pickupSelector = static_cast<electry::PickupSelector> (selector);
    next.pickupType = valueOf (parameterPointers.pickupType);
    next.toneKnob = valueOf (parameterPointers.tone);
    electry::applyGuitarBuild(next, valueOf (parameterPointers.guitarBuild));
    next.bodyResonance = valueOf (parameterPointers.bodyResonance);
    next.stringAge = valueOf (parameterPointers.stringAge);
    next.pickPosition = valueOf (parameterPointers.pickPosition);
    next.pickHardness = valueOf (parameterPointers.pickHardness);
    next.pickNoise = valueOf (parameterPointers.pickNoise);
    next.fingerNoise = valueOf (parameterPointers.fingerNoise);
    next.releaseNoise = valueOf (parameterPointers.releaseNoise);
    next.muteDamping = valueOf (parameterPointers.muteDamping);
    next.bendTimeSeconds = valueOf (parameterPointers.bendTime);
    next.velocityAmount = valueOf (parameterPointers.velocity);
    next.outputGain = juce::Decibels::decibelsToGain (valueOf (parameterPointers.output));
    next.artifactAmount = valueOf (parameterPointers.artifacts);
    next.sympatheticAmount = valueOf (parameterPointers.sympathetic);
    next.palmMute = valueOf (parameterPointers.palmMute);
    next.strumSpreadSeconds = 0.001f * valueOf (parameterPointers.strumSpread);
    next.tremoloRateHz = valueOf (parameterPointers.tremoloRate);
    next.resonanceDepth = 0.01f * valueOf (parameterPointers.resonanceDepth);
    const auto mode = juce::jlimit (0, 2,
        juce::roundToInt (valueOf (parameterPointers.outputMode)));
    const bool requestedDouble = mode == 2;
    next.outputMode = mode == 1 ? electry::OutputMode::Stereo
                                : electry::OutputMode::Mono;
    engine.setParameters (next);

    // A Double lane is a complete mono guitar, not the divided-pickup field.
    // Refresh it when Double is entered: dormant strings cannot resume and
    // parameter targets changed while it slept become the new engine's exact
    // starting state before the next note. Controller targets keep mirroring
    // while dormant; reset() deliberately preserves them, except for sustain,
    // which is restored explicitly below.
    next.outputMode = electry::OutputMode::Mono;
    doubleEngine->setParameters (next);
    if (! doubleModeActive && requestedDouble)
    {
        const int pickStyle = static_cast<int> (engine.getCurrentPickStyle());
        const int playStyle = static_cast<int> (engine.getCurrentPlayStyle());
        doubleEngine->reset();
        doubleEngine->noteOn (electry::ElectryEngine::firstKeyswitchNote
                                  + pickStyle, 1.0f);
        doubleEngine->noteOn (electry::ElectryEngine::firstPlayStyleKeyswitchNote
                                  + playStyle, 1.0f);
        doubleEngine->setSustainPedal (sustainPedalDown);
        doubleEngine->setSoloStringMask (engine.getSoloStringMask());
    }
    doubleModeActive = requestedDouble;
}

void ElectryAudioProcessor::updateEffectParameters() noexcept
{
    electry::FxParameters next;
    next.distortion = valueOf (parameterPointers.distortion);
    next.amp = valueOf (parameterPointers.amp);
    next.ampModel = static_cast<electry::AmpModel> (juce::jlimit (
        0, 2, juce::roundToInt (valueOf (parameterPointers.ampModel))));
    next.compressor = valueOf (parameterPointers.compressor);
    next.delay = valueOf (parameterPointers.delay);
    next.room = valueOf (parameterPointers.room);
    effects.setParameters (next);

    // How loud the rig actually is in the room. The chain manages its own
    // listening level, but acoustically a cranked amplifier is deafening
    // while a clean DI is not in the room at all - and that level is what
    // decides whether the resonance wheel can push the strings into feedback.
    engine.setAcousticReturnLevel (
        juce::jmin (1.0f, next.amp + 0.6f * next.distortion));
    doubleEngine->setAcousticReturnLevel (
        juce::jmin (1.0f, next.amp + 0.6f * next.distortion));
}

void ElectryAudioProcessor::publishStringVisualState() noexcept
{
    engine.getStringVisualState (visualScratch);
    for (int stringIndex = 0;
         stringIndex < electry::ElectryEngine::stringCount; ++stringIndex)
    {
        const auto index = static_cast<std::size_t> (stringIndex);
        if (visualScratch[index].sounding && visualScratch[index].midiNote >= 0)
            visualScratch[index].midiNote += electry::ElectryEngine::octaveShiftSemitones;
        stringVisuals[index].store (
            electry::visuals::packStringVisual (visualScratch[index]),
            std::memory_order_relaxed);
    }
}

void ElectryAudioProcessor::triggerArticulation (int articulationIndex)
{
    if (articulationIndex < 0
        || articulationIndex >= electry::ElectryEngine::keyswitchCount)
        return;
    // A strip click chooses the persistent base even when physical MIDI
    // play-style keys are configured as momentary overrides.
    latchArticulation (articulationIndex);
    enqueueUiMidiEvent (
        electry::ElectryEngine::firstKeyswitchNote + articulationIndex,
        1.0f, true, true);
}

void ElectryAudioProcessor::triggerStringRepick (int stringIndex,
                                                 float velocity)
{
    if (stringIndex < 0
        || stringIndex >= electry::ElectryEngine::stringCount
        || velocity <= 0.0f)
        return;

    enqueueUiMidiEvent (
        electry::ElectryEngine::firstMidiRepickNote + stringIndex,
        velocity, true);
}

void ElectryAudioProcessor::setPlayStyleKeysHold (bool shouldHold)
{
    if (playStyleKeysHold.exchange (shouldHold, std::memory_order_relaxed)
        == shouldHold)
        return;
    updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails()
            .withNonParameterStateChanged (true));
}

void ElectryAudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int,
                                          int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, true);
}

void ElectryAudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int,
                                           int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, false);
}

void ElectryAudioProcessor::latchArticulation (int articulationIndex) noexcept
{
    if (articulationIndex >= 0
        && articulationIndex < electry::ElectryEngine::pickStyleKeyswitchCount)
        pickStyleIndex.store (articulationIndex, std::memory_order_relaxed);
    else if (articulationIndex >= electry::ElectryEngine::pickStyleKeyswitchCount
             && articulationIndex < electry::ElectryEngine::keyswitchCount)
        playStyleIndex.store (
            articulationIndex - electry::ElectryEngine::pickStyleKeyswitchCount,
            std::memory_order_relaxed);
}

void ElectryAudioProcessor::enqueueUiMidiEvent (
    int note, float velocity, bool isNoteOn,
    bool selectsBaseArticulation) noexcept
{
    const auto write = uiWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1u) % uiQueueCapacity;

    // Dropping an event is preferable to ever waiting on the message or audio thread.
    if (next == uiReadIndex.load (std::memory_order_acquire))
    {
        panicRequested.store (true, std::memory_order_release);
        return;
    }

    uiMidiQueue[write] = { juce::jlimit (0, 127, note),
                           juce::jlimit (0.0f, 1.0f, velocity), isNoteOn,
                           selectsBaseArticulation };
    uiWriteIndex.store (next, std::memory_order_release);
}

void ElectryAudioProcessor::dispatchUiMidiEventPass (unsigned begin,
                                                      unsigned end,
                                                      bool conditioning,
                                                      NoteOnBatch& batch) noexcept
{
    auto read = begin;
    while (read != end)
    {
        const auto event = uiMidiQueue[read];
        const bool eventConditionsAttack =
            isAttackConditioningNote (event.note);
        if (eventConditionsAttack == conditioning)
        {
            if (conditioning)
            {
                if (event.noteOn)
                    dispatchNoteOn (event.note, event.velocity,
                                    event.selectsBaseArticulation);
                else
                    dispatchNoteOff (event.note);
            }
            else if (event.noteOn)
            {
                batchOrDispatchNoteOn (event.note, event.velocity,
                                       electry::ElectryEngine::legacyExpressionId,
                                       0,
                                       event.selectsBaseArticulation, batch);
            }
            else
            {
                flushNoteOnBatch (batch);
                dispatchNoteOff (event.note);
            }
        }
        read = (read + 1u) % uiQueueCapacity;
    }
}

void ElectryAudioProcessor::batchOrDispatchNoteOn (
    int note, float velocity,
    electry::ElectryEngine::ExpressionId expressionId,
    int midiChannel,
    bool selectsBaseArticulation,
    NoteOnBatch& batch) noexcept
{
    if (selectsBaseArticulation || velocity <= 0.0f
        || ! electry::ElectryEngine::isMidiPlayableNote (note))
    {
        flushNoteOnBatch (batch);
        dispatchNoteOn (note, velocity, selectsBaseArticulation);
        return;
    }

    // Keep hostile event floods bounded without splitting any ordinary MIDI
    // chord or overlap that fits in one complete 128-note MIDI key range.
    if (batch.size == batch.events.size())
        flushNoteOnBatch (batch);
    // A sounded Note On must always have a matching fixed ownership entry.
    // Once the per-pitch FIFO is full, drop the hostile excess attack rather
    // than create an owner that no later Note Off can address.
    if (! recordNoteOwnership (note, midiChannel, expressionId))
        return;
    const int engineNote = note - electry::ElectryEngine::octaveShiftSemitones;
    batch.events[batch.size++] = { engineNote, velocity, expressionId };
}

void ElectryAudioProcessor::flushNoteOnBatch (NoteOnBatch& batch) noexcept
{
    if (batch.size != 0)
    {
        const std::span<const electry::ElectryEngine::NoteOnEvent> notes (
            batch.events.data(), batch.size);
        engine.noteOnChord (notes);
        if (doubleModeActive)
            doubleEngine->noteOnChord (notes);
    }
    batch.size = 0;
}

void ElectryAudioProcessor::discardUiMidiEvents() noexcept
{
    uiReadIndex.store (uiWriteIndex.load (std::memory_order_acquire),
                       std::memory_order_release);
}

void ElectryAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    auto state = parameters.copyState();
    state.setProperty (factoryProgramProperty,
                       currentProgram.load (std::memory_order_relaxed), nullptr);
    state.setProperty (pickStyleProperty,
                       pickStyleIndex.load (std::memory_order_relaxed), nullptr);
    state.setProperty (playStyleProperty,
                       playStyleIndex.load (std::memory_order_relaxed), nullptr);
    state.setProperty (playStyleKeysHoldProperty,
                       playStyleKeysHold.load (std::memory_order_relaxed), nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destinationData);
}

void ElectryAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        auto restoredState = juce::ValueTree::fromXml (*xml);
        if (! restoredState.getChildWithProperty ("id", electry::parameters::ampModel)
                  .isValid())
        {
            juce::ValueTree legacyAmpModel { "PARAM" };
            legacyAmpModel.setProperty ("id", electry::parameters::ampModel, nullptr);
            legacyAmpModel.setProperty (
                "value", static_cast<int> (electry::AmpModel::ModernHighGain), nullptr);
            restoredState.appendChild (legacyAmpModel, nullptr);
        }
        const int restoredProgram = static_cast<int> (
            restoredState.getProperty (factoryProgramProperty, 0));
        pickStyleIndex.store (juce::jlimit (
            0, electry::ElectryEngine::pickStyleKeyswitchCount - 1,
            static_cast<int> (restoredState.getProperty (pickStyleProperty, 0))),
            std::memory_order_relaxed);
        const int restoredPlayStyle = juce::jlimit (
            0, electry::ElectryEngine::playStyleKeyswitchCount - 1,
            static_cast<int> (restoredState.getProperty (playStyleProperty, 0)));
        playStyleIndex.store (restoredPlayStyle, std::memory_order_relaxed);
        effectivePlayStyleIndex.store (restoredPlayStyle,
                                       std::memory_order_relaxed);
        playStyleKeysHold.store (
            static_cast<bool> (
                restoredState.getProperty (playStyleKeysHoldProperty, false)),
            std::memory_order_relaxed);
        parameters.replaceState (restoredState);
        currentProgram.store (
            restoredProgram >= 0 && restoredProgram < getNumPrograms()
                ? restoredProgram : 0,
            std::memory_order_relaxed);
        requestPanic();
        updateHostDisplay (
            juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    }
}

juce::AudioProcessorEditor* ElectryAudioProcessor::createEditor()
{
    return new ElectryAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ElectryAudioProcessor();
}
