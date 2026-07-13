#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
template <typename Enum>
Enum enumFromParameter (const std::atomic<float>* value, int maximum) noexcept
{
    return static_cast<Enum> (juce::jlimit (0, maximum,
                                           juce::roundToInt (value->load (std::memory_order_relaxed))));
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

juce::String bipolarPercentText (float value, int)
{
    return (value > 0.0f ? "+" : "")
         + juce::String (juce::roundToInt (value * 100.0f)) + "%";
}

juce::String centsText (float value, int)
{
    return (value > 0.0f ? "+" : "") + juce::String (value, 1) + "ct";
}

juce::String octaveDepthText (float value, int)
{
    return juce::String (value, 3) + "oct";
}

juce::String decibelsText (float value, int)
{
    return (value > 0.0f ? "+" : "") + juce::String (value, 1) + "dB";
}

float plainNumericValue (const juce::String& text)
{
    return text.retainCharacters ("0123456789.-").getFloatValue();
}

juce::String frequencyText (float value, int)
{
    if (value >= 1000.0f)
        return juce::String (value / 1000.0f, value >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (value, value < 10.0f ? 2 : 1) + " Hz";
}

float frequencyValue (const juce::String& text)
{
    const auto value = text.retainCharacters ("0123456789.-").getFloatValue();
    return text.containsIgnoreCase ("k") ? value * 1000.0f : value;
}

juce::String timeText (float value, int)
{
    if (value < 1.0f)
        return juce::String (juce::roundToInt (value * 1000.0f)) + " ms";
    return juce::String (value, value < 10.0f ? 2 : 1) + " s";
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

juce::AudioParameterFloatAttributes bipolarPercentAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("%")
        .withStringFromValueFunction (bipolarPercentText)
        .withValueFromStringFunction (percentValue);
}

juce::AudioParameterFloatAttributes timeAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("s")
        .withStringFromValueFunction (timeText)
        .withValueFromStringFunction (timeValue);
}

bool containsParameterState (const juce::ValueTree& state, const char* parameterId)
{
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };

    for (const auto& child : state)
        if (child.hasType (parameterType)
            && child.getProperty (idProperty).toString() == parameterId)
            return true;

    return false;
}

void addDefaultParameterStateIfMissing (
    juce::ValueTree& state, juce::AudioProcessorValueTreeState& parameters,
    const char* parameterId)
{
    if (containsParameterState (state, parameterId))
        return;

    if (const auto* parameter = parameters.getParameter (parameterId))
    {
        static const juce::Identifier parameterType { "PARAM" };
        static const juce::Identifier idProperty { "id" };
        static const juce::Identifier valueProperty { "value" };
        juce::ValueTree parameterState { parameterType };
        parameterState.setProperty (idProperty, parameterId, nullptr);
        parameterState.setProperty (
            valueProperty, parameter->convertFrom0to1 (parameter->getDefaultValue()), nullptr);
        state.appendChild (parameterState, nullptr);
    }
}
} // namespace

MarsAudioProcessor::MarsAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MARS_STATE", createParameterLayout())
{
    using namespace mars::parameters;

    parameterPointers.osc1Wave        = parameters.getRawParameterValue (osc1Wave);
    parameterPointers.osc1Octave      = parameters.getRawParameterValue (osc1Octave);
    parameterPointers.osc2Wave        = parameters.getRawParameterValue (osc2Wave);
    parameterPointers.osc2Octave      = parameters.getRawParameterValue (osc2Octave);
    parameterPointers.osc2Tune        = parameters.getRawParameterValue (osc2Tune);
    parameterPointers.osc2Fine        = parameters.getRawParameterValue (osc2Fine);
    parameterPointers.oscMix          = parameters.getRawParameterValue (oscMix);
    parameterPointers.pulseWidth      = parameters.getRawParameterValue (pulseWidth);
    parameterPointers.subLevel        = parameters.getRawParameterValue (subLevel);
    parameterPointers.noiseLevel      = parameters.getRawParameterValue (noiseLevel);
    parameterPointers.crossMod        = parameters.getRawParameterValue (crossMod);
    parameterPointers.filterModel     = parameters.getRawParameterValue (filterModel);
    parameterPointers.cutoff          = parameters.getRawParameterValue (cutoff);
    parameterPointers.resonance       = parameters.getRawParameterValue (resonance);
    parameterPointers.filterDrive     = parameters.getRawParameterValue (filterDrive);
    parameterPointers.filterShape     = parameters.getRawParameterValue (filterShape);
    parameterPointers.filterEnvAmount = parameters.getRawParameterValue (filterEnvAmount);
    parameterPointers.keyTrack        = parameters.getRawParameterValue (keyTrack);
    parameterPointers.fAttack         = parameters.getRawParameterValue (fAttack);
    parameterPointers.fDecay          = parameters.getRawParameterValue (fDecay);
    parameterPointers.fSustain        = parameters.getRawParameterValue (fSustain);
    parameterPointers.fRelease        = parameters.getRawParameterValue (fRelease);
    parameterPointers.aAttack         = parameters.getRawParameterValue (aAttack);
    parameterPointers.aDecay          = parameters.getRawParameterValue (aDecay);
    parameterPointers.aSustain        = parameters.getRawParameterValue (aSustain);
    parameterPointers.aRelease        = parameters.getRawParameterValue (aRelease);
    parameterPointers.lfoWave         = parameters.getRawParameterValue (lfoWave);
    parameterPointers.lfoRate         = parameters.getRawParameterValue (lfoRate);
    parameterPointers.lfoPitch        = parameters.getRawParameterValue (lfoPitch);
    parameterPointers.lfoFilter       = parameters.getRawParameterValue (lfoFilter);
    parameterPointers.lfoPwm          = parameters.getRawParameterValue (lfoPwm);
    parameterPointers.voiceMode       = parameters.getRawParameterValue (voiceMode);
    parameterPointers.unisonVoices    = parameters.getRawParameterValue (unisonVoices);
    parameterPointers.drift           = parameters.getRawParameterValue (drift);
    parameterPointers.spread          = parameters.getRawParameterValue (spread);
    parameterPointers.glide           = parameters.getRawParameterValue (glide);
    parameterPointers.velocity        = parameters.getRawParameterValue (velocity);
    parameterPointers.chorusMix       = parameters.getRawParameterValue (chorusMix);
    parameterPointers.chorusRate      = parameters.getRawParameterValue (chorusRate);
    parameterPointers.output          = parameters.getRawParameterValue (output);
    parameterPointers.osc1Enabled     = parameters.getRawParameterValue (osc1Enabled);
    parameterPointers.osc2Enabled     = parameters.getRawParameterValue (osc2Enabled);
    parameterPointers.hqOversampling  = parameters.getRawParameterValue (hqOversampling);

    jassert (parameterPointers.osc1Wave != nullptr
             && parameterPointers.osc1Enabled != nullptr
             && parameterPointers.osc2Enabled != nullptr
             && parameterPointers.hqOversampling != nullptr);
    keyboardState.addListener (this);
}

MarsAudioProcessor::~MarsAudioProcessor()
{
    keyboardState.removeListener (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout MarsAudioProcessor::createParameterLayout()
{
    using namespace mars::parameters;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;
    result.reserve (43);

    const auto addChoice = [&result] (const char* id, const char* name,
                                      juce::StringArray choices, int defaultIndex)
    {
        result.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, 1 }, name, std::move (choices), defaultIndex));
    };
    const auto addInt = [&result] (
        const char* id, const char* name, int minimum, int maximum, int defaultValue,
        juce::AudioParameterIntAttributes attributes = {})
    {
        result.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id, 1 }, name, minimum, maximum, defaultValue,
            std::move (attributes)));
    };
    const auto addFloat = [&result] (const char* id, const char* name,
                                     juce::NormalisableRange<float> range, float defaultValue,
                                     juce::AudioParameterFloatAttributes attributes = {})
    {
        result.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue, std::move (attributes)));
    };
    const auto addPercent = [&addFloat] (const char* id, const char* name,
                                         float defaultValue, float minimum = 0.0f,
                                         float maximum = 1.0f)
    {
        addFloat (id, name, { minimum, maximum, 0.001f }, defaultValue,
                  minimum < 0.0f ? bipolarPercentAttributes() : percentAttributes());
    };

    const auto signedIntegerAttributes = [] (juce::String suffix)
    {
        return juce::AudioParameterIntAttributes()
            .withLabel (suffix)
            .withStringFromValueFunction (
                [suffix] (int value, int)
                {
                    return (value > 0 ? "+" : "") + juce::String (value) + suffix;
                })
            .withValueFromStringFunction (
                [] (const juce::String& text)
                {
                    return text.retainCharacters ("0123456789.-").getIntValue();
                });
    };

    addChoice (osc1Wave, "VCO I waveform", { "Saw", "Pulse", "Triangle" }, 0);
    addInt (osc1Octave, "VCO I octave", -2, 2, 0, signedIntegerAttributes ("oct"));
    addChoice (osc2Wave, "VCO II waveform", { "Saw", "Pulse", "Triangle" }, 1);
    addInt (osc2Octave, "VCO II octave", -2, 2, 0, signedIntegerAttributes ("oct"));
    addInt (osc2Tune, "VCO II tune", -12, 12, 0, signedIntegerAttributes ("st"));
    addFloat (osc2Fine, "VCO II fine tune", { -50.0f, 50.0f, 0.1f }, 0.0f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("ct")
                  .withStringFromValueFunction (centsText)
                  .withValueFromStringFunction (plainNumericValue));
    addPercent (oscMix, "Oscillator balance", 0.46f);
    addPercent (pulseWidth, "Pulse width", 0.50f, 0.05f, 0.95f);
    addPercent (subLevel, "Sub oscillator level", 0.16f);
    addPercent (noiseLevel, "Noise level", 0.025f);
    addPercent (crossMod, "Cross modulation", 0.06f);

    addChoice (filterModel, "Filter model", { "Ladder", "SEM" }, 0);

    auto cutoffRange = juce::NormalisableRange<float> { 20.0f, 20000.0f, 0.0f };
    cutoffRange.setSkewForCentre (1000.0f);
    addFloat (cutoff, "Filter cutoff", cutoffRange, 4200.0f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("Hz")
                  .withStringFromValueFunction (frequencyText)
                  .withValueFromStringFunction (frequencyValue));
    addPercent (resonance, "Filter resonance", 0.28f);
    addPercent (filterDrive, "Filter drive", 0.22f);
    addPercent (filterShape, "Filter shape", 0.35f);
    addPercent (filterEnvAmount, "Filter envelope amount", 0.46f, -1.0f, 1.0f);
    addPercent (keyTrack, "Filter key tracking", 0.50f);

    auto attackRange = juce::NormalisableRange<float> { 0.001f, 8.0f, 0.0f };
    attackRange.setSkewForCentre (0.18f);
    auto decayRange = juce::NormalisableRange<float> { 0.010f, 8.0f, 0.0f };
    decayRange.setSkewForCentre (0.55f);
    auto releaseRange = juce::NormalisableRange<float> { 0.010f, 12.0f, 0.0f };
    releaseRange.setSkewForCentre (0.75f);

    addFloat (fAttack, "Filter envelope attack", attackRange, 0.012f, timeAttributes());
    addFloat (fDecay, "Filter envelope decay", decayRange, 0.45f, timeAttributes());
    addPercent (fSustain, "Filter envelope sustain", 0.34f);
    addFloat (fRelease, "Filter envelope release", releaseRange, 0.62f, timeAttributes());
    addFloat (aAttack, "Amp envelope attack", attackRange, 0.008f, timeAttributes());
    addFloat (aDecay, "Amp envelope decay", decayRange, 0.38f, timeAttributes());
    addPercent (aSustain, "Amp envelope sustain", 0.78f);
    addFloat (aRelease, "Amp envelope release", releaseRange, 0.55f, timeAttributes());

    auto lfoRateRange = juce::NormalisableRange<float> { 0.05f, 30.0f, 0.0f };
    lfoRateRange.setSkewForCentre (3.0f);
    addChoice (lfoWave, "LFO waveform", { "Triangle", "Sine", "Sample & hold" }, 0);
    addFloat (lfoRate, "LFO rate", lfoRateRange, 4.8f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("Hz")
                  .withStringFromValueFunction (frequencyText)
                  .withValueFromStringFunction (frequencyValue));
    addFloat (lfoPitch, "LFO pitch depth", { 0.0f, 100.0f, 0.1f }, 8.0f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("ct")
                  .withStringFromValueFunction (centsText)
                  .withValueFromStringFunction (plainNumericValue));
    addFloat (lfoFilter, "LFO filter depth", { 0.0f, 4.0f, 0.001f }, 0.18f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("oct")
                  .withStringFromValueFunction (octaveDepthText)
                  .withValueFromStringFunction (plainNumericValue));
    addPercent (lfoPwm, "LFO PWM depth", 0.16f);

    addChoice (voiceMode, "Voice mode", { "Poly", "Unison", "Fifth" }, 0);
    addInt (unisonVoices, "Unison voices", 2, 8, 4);
    addPercent (drift, "Voice-card drift", 0.28f);
    addPercent (spread, "Stereo spread", 0.58f);
    auto glideRange = juce::NormalisableRange<float> { 0.0f, 2.0f, 0.0f };
    glideRange.setSkewForCentre (0.10f);
    addFloat (glide, "Glide time", glideRange, 0.0f, timeAttributes());
    addPercent (velocity, "Velocity response", 0.68f);

    addPercent (chorusMix, "Ensemble mix", 0.30f);
    auto chorusRateRange = juce::NormalisableRange<float> { 0.05f, 2.0f, 0.0f };
    chorusRateRange.setSkewForCentre (0.42f);
    addFloat (chorusRate, "Ensemble rate", chorusRateRange, 0.38f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("Hz")
                  .withStringFromValueFunction (frequencyText)
                  .withValueFromStringFunction (frequencyValue));
    addFloat (output, "Output level", { -24.0f, 6.0f, 0.1f }, -6.0f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("dB")
                  .withStringFromValueFunction (decibelsText)
                  .withValueFromStringFunction (plainNumericValue));

    // Version-2 parameters are appended so every version-1 host parameter
    // retains both its identifier and its position in the processor list.
    // The higher hint also keeps AUv2 automation ordering backward compatible.
    result.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { osc1Enabled, 2 }, "VCO I enabled", true));
    result.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { osc2Enabled, 2 }, "VCO II enabled", true));

    // This quality switch is persisted with plug-in state, but is deliberately
    // not automatable: changing the internal nonlinear timebase is deferred
    // until the engine has been idle long enough to remain click-free.
    result.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { hqOversampling, 3 }, "HQ oversampling", true,
        juce::AudioParameterBoolAttributes()
            .withAutomatable (false)
            .withStringFromValueFunction (
                [] (bool enabled, int) { return enabled ? "On" : "Off"; })
            .withValueFromStringFunction (
                [] (const juce::String& text)
                {
                    return text.containsIgnoreCase ("2")
                        || text.containsIgnoreCase ("on")
                        || text.containsIgnoreCase ("hq");
                })));

    return { result.begin(), result.end() };
}

void MarsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engineReady.store (false, std::memory_order_release);
    const bool hqEnabled = valueOf (parameterPointers.hqOversampling) >= 0.5f;
    engine.prepare (sampleRate, samplesPerBlock, hqEnabled);
    updateEngineParameters();
    engine.reset();
    engine.setPitchBend (0.0f);
    engine.setModWheel (0.0f);
    engine.setSustainPedal (false);
    discardUiMidiEvents();
    panicRequested.store (false, std::memory_order_release);
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    displayOversamplingFactor.store (engine.getOversamplingFactor(),
                                     std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    engineReady.store (true, std::memory_order_release);
}

void MarsAudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    engine.setPitchBend (0.0f);
    engine.setModWheel (0.0f);
    engine.setSustainPedal (false);
    engine.allNotesOff();
    engine.reset();
    discardUiMidiEvents();
    panicRequested.store (false, std::memory_order_release);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    displaySampleRate.store (0.0, std::memory_order_relaxed);
    displayOversamplingFactor.store (1, std::memory_order_relaxed);
}

bool MarsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MarsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! engineReady.load (std::memory_order_acquire))
        return;

    updateEngineParameters();

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        engine.reset();
        discardUiMidiEvents();
    }

    // GUI notes enter through a bounded lock-free queue and start at the next
    // block boundary. This avoids allocating or locking in the audio callback.
    dispatchUiMidiEvents();

    const auto numSamples = buffer.getNumSamples();
    int renderedTo = 0;

    for (const auto metadata : midiMessages)
    {
        const auto eventSample = juce::jlimit (0, numSamples, metadata.samplePosition);

        if (eventSample > renderedTo)
        {
            engine.process (buffer.getWritePointer (0, renderedTo),
                            buffer.getWritePointer (1, renderedTo),
                            eventSample - renderedTo);
            renderedTo = eventSample;
        }

        dispatchMidiData (metadata.data, metadata.numBytes);
    }

    if (renderedTo < numSamples)
        engine.process (buffer.getWritePointer (0, renderedTo),
                        buffer.getWritePointer (1, renderedTo),
                        numSamples - renderedTo);

    activeVoiceCount.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);
    displayOversamplingFactor.store (engine.getOversamplingFactor(),
                                     std::memory_order_relaxed);
}

void MarsAudioProcessor::dispatchMidiData (const juce::uint8* data, int numBytes) noexcept
{
    if (data == nullptr || numBytes < 1)
        return;

    const auto status = static_cast<unsigned> (data[0]);
    const auto kind = status & 0xf0u;

    if (kind == 0x90u && numBytes >= 3)
    {
        if (data[2] != 0)
            engine.noteOn (static_cast<int> (data[1] & 0x7fu),
                           static_cast<float> (data[2] & 0x7fu) / 127.0f);
        else
            engine.noteOff (static_cast<int> (data[1] & 0x7fu));
    }
    else if (kind == 0x80u && numBytes >= 3)
    {
        engine.noteOff (static_cast<int> (data[1] & 0x7fu));
    }
    else if (kind == 0xb0u && numBytes >= 3)
    {
        const auto controller = data[1] & 0x7fu;
        const auto controllerValue = data[2] & 0x7fu;
        if (controller == 1u)
            engine.setModWheel (static_cast<float> (controllerValue) / 127.0f);
        else if (controller == 64u)
            engine.setSustainPedal (controllerValue >= 64u);
        else if (controller == 121u)
        {
            engine.setPitchBend (0.0f);
            engine.setModWheel (0.0f);
            engine.setSustainPedal (false);
        }
        else if (controller == 120u)
        {
            // MIDI All Sound Off is an immediate mute, like the front-panel
            // Panic control. The parameter targets survive the engine reset.
            engine.reset();
        }
        else if (controller == 123u)
        {
            // All Notes Off behaves like releasing the held keys, so the amp
            // envelope and ensemble tail remain musical. Hold 1 remains in
            // force until its own controller is released.
            engine.allNotesOff();
        }
    }
    else if (kind == 0xe0u && numBytes >= 3)
    {
        const auto value14 = static_cast<int> (data[1] & 0x7fu)
                           | (static_cast<int> (data[2] & 0x7fu) << 7);
        const auto bend = value14 < 8192
            ? static_cast<float> (value14 - 8192) / 8192.0f
            : static_cast<float> (value14 - 8192) / 8191.0f;
        engine.setPitchBend (juce::jlimit (-1.0f, 1.0f, bend));
    }
}

void MarsAudioProcessor::updateEngineParameters() noexcept
{
    mars::EngineParameters next;
    next.osc1Wave = enumFromParameter<mars::OscillatorWave> (parameterPointers.osc1Wave, 2);
    next.osc1Octave = juce::jlimit (-2, 2, juce::roundToInt (valueOf (parameterPointers.osc1Octave)));
    next.osc2Wave = enumFromParameter<mars::OscillatorWave> (parameterPointers.osc2Wave, 2);
    next.osc2Octave = juce::jlimit (-2, 2, juce::roundToInt (valueOf (parameterPointers.osc2Octave)));
    next.osc2Semitones = juce::jlimit (-12, 12, juce::roundToInt (valueOf (parameterPointers.osc2Tune)));
    next.osc2FineCents = valueOf (parameterPointers.osc2Fine);
    next.oscMix = valueOf (parameterPointers.oscMix);
    next.pulseWidth = valueOf (parameterPointers.pulseWidth);
    next.subLevel = valueOf (parameterPointers.subLevel);
    next.noiseLevel = valueOf (parameterPointers.noiseLevel);
    next.crossMod = valueOf (parameterPointers.crossMod);
    next.filterModel = enumFromParameter<mars::FilterModel> (parameterPointers.filterModel, 1);
    next.cutoffHz = valueOf (parameterPointers.cutoff);
    next.resonance = valueOf (parameterPointers.resonance);
    next.filterDrive = valueOf (parameterPointers.filterDrive);
    next.filterShape = valueOf (parameterPointers.filterShape);
    next.filterEnvAmount = valueOf (parameterPointers.filterEnvAmount);
    next.filterKeyTrack = valueOf (parameterPointers.keyTrack);
    next.filterAttack = valueOf (parameterPointers.fAttack);
    next.filterDecay = valueOf (parameterPointers.fDecay);
    next.filterSustain = valueOf (parameterPointers.fSustain);
    next.filterRelease = valueOf (parameterPointers.fRelease);
    next.ampAttack = valueOf (parameterPointers.aAttack);
    next.ampDecay = valueOf (parameterPointers.aDecay);
    next.ampSustain = valueOf (parameterPointers.aSustain);
    next.ampRelease = valueOf (parameterPointers.aRelease);
    next.lfoWave = enumFromParameter<mars::LfoWaveform> (parameterPointers.lfoWave, 2);
    next.lfoRateHz = valueOf (parameterPointers.lfoRate);
    next.lfoPitchCents = valueOf (parameterPointers.lfoPitch);
    next.lfoFilterOctaves = valueOf (parameterPointers.lfoFilter);
    next.lfoPwm = valueOf (parameterPointers.lfoPwm);
    next.voiceMode = enumFromParameter<mars::VoiceMode> (parameterPointers.voiceMode, 2);
    next.unisonVoices = juce::jlimit (2, 8, juce::roundToInt (valueOf (parameterPointers.unisonVoices)));
    next.drift = valueOf (parameterPointers.drift);
    next.spread = valueOf (parameterPointers.spread);
    next.glideSeconds = valueOf (parameterPointers.glide);
    next.velocityAmount = valueOf (parameterPointers.velocity);
    next.chorusMix = valueOf (parameterPointers.chorusMix);
    next.chorusRateHz = valueOf (parameterPointers.chorusRate);
    next.outputGain = juce::Decibels::decibelsToGain (valueOf (parameterPointers.output));
    next.osc1Enabled = valueOf (parameterPointers.osc1Enabled) >= 0.5f;
    next.osc2Enabled = valueOf (parameterPointers.osc2Enabled) >= 0.5f;
    engine.setOversamplingEnabled (
        valueOf (parameterPointers.hqOversampling) >= 0.5f);
    engine.setParameters (next);
}

void MarsAudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int,
                                       int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, true);
}

void MarsAudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int,
                                        int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, false);
}

void MarsAudioProcessor::enqueueUiMidiEvent (int note, float velocity, bool isNoteOn) noexcept
{
    const auto write = uiWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1u) % uiQueueCapacity;

    // Dropping an event is preferable to ever waiting on the message or audio thread.
    if (next == uiReadIndex.load (std::memory_order_acquire))
    {
        panicRequested.store (true, std::memory_order_release);
        return;
    }

    uiMidiQueue[write] = { juce::jlimit (0, 127, note), juce::jlimit (0.0f, 1.0f, velocity), isNoteOn };
    uiWriteIndex.store (next, std::memory_order_release);
}

void MarsAudioProcessor::dispatchUiMidiEvents() noexcept
{
    auto read = uiReadIndex.load (std::memory_order_relaxed);
    const auto write = uiWriteIndex.load (std::memory_order_acquire);

    while (read != write)
    {
        const auto event = uiMidiQueue[read];
        if (event.noteOn)
            engine.noteOn (event.note, event.velocity);
        else
            engine.noteOff (event.note);
        read = (read + 1u) % uiQueueCapacity;
    }

    uiReadIndex.store (read, std::memory_order_release);
}

void MarsAudioProcessor::discardUiMidiEvents() noexcept
{
    uiReadIndex.store (uiWriteIndex.load (std::memory_order_acquire),
                       std::memory_order_release);
}

void MarsAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destinationData);
}

void MarsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        auto restoredState = juce::ValueTree::fromXml (*xml);

        // APVTS preserves the current raw value when a parameter child is
        // absent from a replacement tree. Explicitly restore the declared
        // defaults for legacy states, otherwise loading an old preset after a
        // newer one could leave later switches at the newer state's values.
        for (const auto* parameterId : { mars::parameters::osc1Enabled,
                                         mars::parameters::osc2Enabled,
                                         mars::parameters::hqOversampling })
            addDefaultParameterStateIfMissing (restoredState, parameters, parameterId);

        parameters.replaceState (restoredState);
        requestPanic();
    }
}

juce::AudioProcessorEditor* MarsAudioProcessor::createEditor()
{
    return new MarsAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MarsAudioProcessor();
}
