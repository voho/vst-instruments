#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
using namespace youknow106;

juce::String percentText (float value, int)
{
    return juce::String (juce::roundToInt (value * 100.0f)) + "%";
}

float percentValue (const juce::String& text)
{
    return text.retainCharacters ("0123456789.-").getFloatValue() / 100.0f;
}

juce::AudioParameterFloatAttributes percentAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("%")
        .withStringFromValueFunction (percentText)
        .withValueFromStringFunction (percentValue);
}

juce::String secondsText (float value, int)
{
    if (value < 1.0f)
        return juce::String (juce::roundToInt (value * 1000.0f)) + " ms";
    return juce::String (value, value < 10.0f ? 2 : 1) + " s";
}

// A control whose *display* is a time or a frequency needs a way back from the
// text a host may let someone type. Without it, "1.00 kHz" would be read as a
// panel position of 1.0 and drive the control to its top.
float secondsFromText (const juce::String& text)
{
    const auto number = text.retainCharacters ("0123456789.-").getFloatValue();
    return text.containsIgnoreCase ("ms") ? number / 1000.0f : number;
}

float hertzFromText (const juce::String& text)
{
    const auto number = text.retainCharacters ("0123456789.-").getFloatValue();
    return text.containsIgnoreCase ("k") ? number * 1000.0f : number;
}

// A panel position that stands for a time is shown as the time the modelled
// circuit produces, not as a percentage of travel.
juce::AudioParameterFloatAttributes attackAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            return secondsText (YouKnow106Engine::envelopeAttackSeconds (value), 0);
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForAttack (secondsFromText (text));
        });
}

juce::AudioParameterFloatAttributes decayAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            return secondsText (YouKnow106Engine::envelopeDecaySeconds (value), 0);
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForDecay (secondsFromText (text));
        });
}

juce::AudioParameterFloatAttributes lfoRateAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            const auto hz = YouKnow106Engine::lfoRateHz (value);
            return juce::String (hz, hz < 10.0f ? 2 : 1) + " Hz";
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForLfoRate (hertzFromText (text));
        });
}

juce::AudioParameterFloatAttributes lfoDelayAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            return secondsText (YouKnow106Engine::lfoDelaySeconds (value), 0);
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForLfoDelay (secondsFromText (text));
        });
}

juce::AudioParameterFloatAttributes cutoffAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            const auto hz = YouKnow106Engine::vcfCutoffHz (
                YouKnow106Engine::vcfPanelCounts (value));
            if (hz >= 1000.0f)
                return juce::String (hz / 1000.0f, 2) + " kHz";
            return juce::String (hz, hz < 100.0f ? 1 : 0) + " Hz";
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForCutoff (hertzFromText (text));
        });
}

juce::AudioParameterFloatAttributes portamentoAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            const auto seconds = YouKnow106Engine::portamentoSeconds (value);
            if (seconds <= 0.0f)
                return juce::String ("OFF");
            return secondsText (seconds, 0) + "/oct";
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            if (text.trim().equalsIgnoreCase ("off"))
                return 0.0f;
            return YouKnow106Engine::panelPositionForPortamento (
                secondsFromText (text));
        });
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

void addDefaultParameterStateIfMissing (juce::ValueTree& state,
                                        juce::AudioProcessorValueTreeState& parameters,
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
            valueProperty, parameter->convertFrom0to1 (parameter->getDefaultValue()),
            nullptr);
        state.appendChild (parameterState, nullptr);
    }
}

// Reads a parameter's stored value out of a saved state, or returns `fallback`.
float storedParameterValue (const juce::ValueTree& state, const char* parameterId,
                            float fallback)
{
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };
    static const juce::Identifier valueProperty { "value" };

    for (const auto& child : state)
        if (child.hasType (parameterType)
            && child.getProperty (idProperty).toString() == parameterId)
            return static_cast<float> (child.getProperty (valueProperty, fallback));
    return fallback;
}

void setStoredParameterValue (juce::ValueTree& state, const char* parameterId,
                              float value)
{
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };
    static const juce::Identifier valueProperty { "value" };

    for (auto child : state)
        if (child.hasType (parameterType)
            && child.getProperty (idProperty).toString() == parameterId)
        {
            child.setProperty (valueProperty, value, nullptr);
            return;
        }

    juce::ValueTree added { parameterType };
    added.setProperty (idProperty, parameterId, nullptr);
    added.setProperty (valueProperty, value, nullptr);
    state.appendChild (added, nullptr);
}
} // namespace

// Sessions saved before the paired switches were split carry a three-way
// `keyMode` (0 Poly 1, 1 Poly 2, 2 Unison) and a three-way `chorus` (0 off,
// 1 mode I, 2 mode II). Translate them into the button pairs that replaced
// them, but only when the new parameters are absent -- a state already written
// by this build must not be overwritten by a stale legacy entry.
void YouKnow106AudioProcessor::migrateSplitModeParameters (juce::ValueTree& state)
{
    using namespace youknow106::parameters;

    if (containsParameterState (state, "keyMode")
        && ! containsParameterState (state, poly1)
        && ! containsParameterState (state, poly2))
    {
        const auto legacy = juce::roundToInt (storedParameterValue (state, "keyMode", 0.0f));
        const bool unison = legacy == 2;
        setStoredParameterValue (state, poly1, (legacy == 0 || unison) ? 1.0f : 0.0f);
        setStoredParameterValue (state, poly2, (legacy == 1 || unison) ? 1.0f : 0.0f);
    }

    if (containsParameterState (state, "chorus")
        && ! containsParameterState (state, chorusI)
        && ! containsParameterState (state, chorusII))
    {
        const auto legacy = juce::roundToInt (storedParameterValue (state, "chorus", 0.0f));
        setStoredParameterValue (state, chorusI, legacy == 1 ? 1.0f : 0.0f);
        setStoredParameterValue (state, chorusII, legacy == 2 ? 1.0f : 0.0f);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
YouKnow106AudioProcessor::createParameterLayout()
{
    using namespace youknow106::parameters;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto travel = [] (const char* id, const char* name, float defaultValue,
                            juce::AudioParameterFloatAttributes attributes)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, defaultValue,
            std::move (attributes));
    };

    // --- Front panel, in panel order --------------------------------------
    layout.add (travel (volume, "Volume", 0.80f, percentAttributes()));
    layout.add (travel (benderDco, "Bender DCO", 0.30f, percentAttributes()));
    layout.add (travel (benderVcf, "Bender VCF", 0.0f, percentAttributes()));
    layout.add (travel (benderLfo, "Bender LFO", 0.0f, percentAttributes()));
    layout.add (travel (portamento, "Portamento", 0.0f, portamentoAttributes()));
    // Two independent latching buttons rather than one three-way choice: the
    // panel has no unison button, and holding both POLY buttons down is how the
    // instrument is put into unison.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { poly1, 1 }, "Poly 1", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { poly2, 1 }, "Poly 2", false));

    layout.add (travel (lfoRate, "LFO Rate", 0.42f, lfoRateAttributes()));
    layout.add (travel (lfoDelay, "LFO Delay", 0.0f, lfoDelayAttributes()));

    layout.add (travel (dcoLfo, "DCO LFO", 0.0f, percentAttributes()));
    layout.add (travel (pwm, "PWM", 0.30f, percentAttributes()));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { pwmMode, 1 }, "PWM Mode",
        juce::StringArray { "LFO", "Manual" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { range, 1 }, "Range",
        juce::StringArray { "16'", "8'", "4'" }, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { saw, 1 }, "Saw", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pulse, 1 }, "Pulse", false));
    layout.add (travel (sub, "Sub", 0.0f, percentAttributes()));
    layout.add (travel (noise, "Noise", 0.0f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { highPass, 1 }, "HPF",
        juce::StringArray { "0", "1", "2", "3" }, 1));

    layout.add (travel (cutoff, "VCF Freq", 0.62f, cutoffAttributes()));
    layout.add (travel (resonance, "VCF Res", 0.10f, percentAttributes()));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { envPolarity, 1 }, "VCF Env Polarity",
        juce::StringArray { "Normal", "Inverted" }, 0));
    layout.add (travel (vcfEnv, "VCF Env", 0.35f, percentAttributes()));
    layout.add (travel (vcfLfo, "VCF LFO", 0.0f, percentAttributes()));
    layout.add (travel (keyFollow, "VCF Kybd", 0.50f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { vcaMode, 1 }, "VCA Mode",
        juce::StringArray { "Env", "Gate" }, 0));
    layout.add (travel (vcaLevel, "VCA Level", 0.80f, percentAttributes()));

    layout.add (travel (attack, "Attack", 0.04f, attackAttributes()));
    layout.add (travel (decay, "Decay", 0.45f, decayAttributes()));
    layout.add (travel (sustain, "Sustain", 0.70f, percentAttributes()));
    layout.add (travel (release, "Release", 0.30f, decayAttributes()));

    // Likewise two independent buttons. Neither down is off; both down is the
    // faster I+II setting, which the hardware reaches the same way.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { chorusI, 1 }, "Chorus I", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { chorusII, 1 }, "Chorus II", false));

    // --- Controls the modelled instrument does not have -------------------
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { transpose, 1 }, "Transpose", -12, 12, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { masterTune, 1 }, "Master Tune",
        juce::NormalisableRange<float> { -50.0f, 50.0f, 0.0f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ct")));
    layout.add (travel (velocity, "Velocity", 0.0f, percentAttributes()));
    layout.add (travel (calibration, "Calibration", 0.35f, percentAttributes()));
    layout.add (travel (chorusNoise, "Chorus Noise", 1.0f, percentAttributes()));
    // Taken from the engine rather than written out, so the host can never be
    // offered a voice count the engine would clamp away. The default is the
    // hardware's own six.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { polyphony, 1 }, "Polyphony", 1,
        youknow106::YouKnow106Engine::maxVoices,
        youknow106::YouKnow106Engine::hardwareVoices));
    // Persisted with the patch, but deliberately not automatable: changing the
    // internal rate empties the whole output path, so the engine holds the
    // change until it has been quiet long enough for that to be inaudible. An
    // automation point would therefore not take effect where it was written --
    // or at all, if it were automated back before the engine went idle.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { hq, 1 }, "HQ", true,
        juce::AudioParameterBoolAttributes()
            .withAutomatable (false)
            .withStringFromValueFunction (
                [] (bool enabled, int) { return enabled ? "On" : "Off"; })
            .withValueFromStringFunction (
                [] (const juce::String& text)
                {
                    return text.containsIgnoreCase ("on")
                        || text.containsIgnoreCase ("hq")
                        || text.containsIgnoreCase ("2")
                        || text.containsIgnoreCase ("4");
                })));

    return layout;
}

YouKnow106AudioProcessor::YouKnow106AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "YOUKNOW106_STATE", createParameterLayout())
{
    using namespace youknow106::parameters;
    const std::array<const char*, 37> ids {
        volume, benderDco, benderVcf, benderLfo, portamento, poly1, poly2,
        lfoRate, lfoDelay, dcoLfo, pwm, pwmMode, range, saw, pulse, sub, noise,
        highPass, cutoff, resonance, envPolarity, vcfEnv, vcfLfo, keyFollow,
        vcaMode, vcaLevel, attack, decay, sustain, release, chorusI, chorusII,
        transpose, masterTune, velocity, calibration, chorusNoise, polyphony, hq
    };

    for (std::size_t index = 0; index < ids.size(); ++index)
        parameterPointers[index] = { ids[index],
                                     parameters.getRawParameterValue (ids[index]) };

    keyboardState.addListener (this);
}

YouKnow106AudioProcessor::~YouKnow106AudioProcessor()
{
    keyboardState.removeListener (this);
}

float YouKnow106AudioProcessor::valueOf (const char* parameterId) const noexcept
{
    for (const auto& pointer : parameterPointers)
        if (pointer.id != nullptr && std::strcmp (pointer.id, parameterId) == 0
            && pointer.value != nullptr)
            return pointer.value->load (std::memory_order_relaxed);
    return 0.0f;
}

int YouKnow106AudioProcessor::choiceOf (const char* parameterId, int maximum) const noexcept
{
    return juce::jlimit (0, maximum, juce::roundToInt (valueOf (parameterId)));
}

void YouKnow106AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engineReady.store (false, std::memory_order_release);
    engine.prepare (sampleRate, samplesPerBlock,
                    valueOf (youknow106::parameters::hq) > 0.5f);
    updateEngineParameters();
    discardUiMidiEvents();
    keyboardState.reset();

    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    displayOversamplingFactor.store (engine.getOversamplingFactor(),
                                     std::memory_order_relaxed);
    setLatencySamples (engine.getProcessingLatencySamples());
    engineReady.store (true, std::memory_order_release);
}

void YouKnow106AudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    engine.allNotesOff();
    engine.reset();
    discardUiMidiEvents();
    keyboardState.reset();
}

bool YouKnow106AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannels() != 0)
        return false;
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::stereo();
}

void YouKnow106AudioProcessor::updateEngineParameters() noexcept
{
    using namespace youknow106;
    using namespace youknow106::parameters;

    EngineParameters engineParameters;
    engineParameters.volume = valueOf (volume);
    engineParameters.benderDcoDepth = valueOf (benderDco);
    engineParameters.benderVcfDepth = valueOf (benderVcf);
    engineParameters.benderLfoDepth = valueOf (benderLfo);
    engineParameters.portamento = valueOf (portamento);
    engineParameters.keyMode = keyModeFor (valueOf (poly1) > 0.5f, valueOf (poly2) > 0.5f);

    engineParameters.lfoRate = valueOf (lfoRate);
    engineParameters.lfoDelay = valueOf (lfoDelay);

    engineParameters.dcoLfoDepth = valueOf (dcoLfo);
    engineParameters.pwmDepth = valueOf (pwm);
    engineParameters.pwmSource = static_cast<PwmSource> (choiceOf (pwmMode, 1));
    engineParameters.range = static_cast<DcoRange> (choiceOf (range, 2));
    engineParameters.sawEnabled = valueOf (saw) > 0.5f;
    engineParameters.pulseEnabled = valueOf (pulse) > 0.5f;
    engineParameters.subLevel = valueOf (sub);
    engineParameters.noiseLevel = valueOf (noise);

    engineParameters.highPass = static_cast<HighPassMode> (choiceOf (highPass, 3));

    engineParameters.cutoff = valueOf (cutoff);
    engineParameters.resonance = valueOf (resonance);
    engineParameters.envPolarity = static_cast<EnvPolarity> (choiceOf (envPolarity, 1));
    engineParameters.envDepth = valueOf (vcfEnv);
    engineParameters.vcfLfoDepth = valueOf (vcfLfo);
    engineParameters.keyFollow = valueOf (keyFollow);

    engineParameters.vcaMode = static_cast<VcaMode> (choiceOf (vcaMode, 1));
    engineParameters.vcaLevel = valueOf (vcaLevel);

    engineParameters.attack = valueOf (attack);
    engineParameters.decay = valueOf (decay);
    engineParameters.sustain = valueOf (sustain);
    engineParameters.release = valueOf (release);

    engineParameters.chorus = chorusModeFor (valueOf (chorusI) > 0.5f, valueOf (chorusII) > 0.5f);

    engineParameters.keyTranspose = juce::roundToInt (valueOf (transpose));
    engineParameters.masterTuneCents = valueOf (masterTune);
    engineParameters.velocityDepth = valueOf (velocity);
    engineParameters.calibration = valueOf (calibration);
    engineParameters.chorusNoise = valueOf (chorusNoise);
    engineParameters.polyphony = juce::roundToInt (valueOf (polyphony));

    engine.setParameters (engineParameters);

    // The editor draws one lamp per available voice, so it needs the count the
    // engine actually settled on rather than the raw parameter.
    displayVoiceLimit.store (
        juce::jlimit (1, youknow106::YouKnow106Engine::maxVoices,
                      engineParameters.polyphony),
        std::memory_order_relaxed);
}

void YouKnow106AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int channel = getTotalNumInputChannels(); channel < buffer.getNumChannels();
         ++channel)
        buffer.clear (channel, 0, numSamples);

    if (! engineReady.load (std::memory_order_acquire) || numSamples <= 0)
    {
        buffer.clear();
        midiMessages.clear();
        return;
    }

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        engine.allNotesOff();
        discardUiMidiEvents();
    }

    // The engine reports one latency for every configuration and pads the
    // shallower ones out to it, so the quality setting can change here without
    // the host having to be told anything. Calling setLatencySamples from the
    // audio callback would be a notification into host code that is free to
    // lock or allocate.
    engine.setOversamplingEnabled (valueOf (youknow106::parameters::hq) > 0.5f);
    displayOversamplingFactor.store (engine.getOversamplingFactor(),
                                     std::memory_order_relaxed);

    updateEngineParameters();
    dispatchUiMidiEvents();

    // Render up to each event before applying it. Dispatching the whole buffer's
    // MIDI at the block boundary would collapse a short note-on/note-off pair
    // onto the same instant and lose the note entirely.
    int renderedTo = 0;
    for (const auto metadata : midiMessages)
    {
        const int eventSample = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (eventSample > renderedTo)
        {
            engine.process (buffer.getWritePointer (0, renderedTo),
                            buffer.getWritePointer (1, renderedTo),
                            eventSample - renderedTo);
            renderedTo = eventSample;
        }

        const auto message = metadata.getMessage();
        if (message.isNoteOn())
            engine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff())
            engine.noteOff (message.getNoteNumber());
        else if (message.isAllSoundOff())
            engine.allNotesOff();
        else if (message.isAllNotesOff())
            // All notes off means release the keys, not cut the sound: the
            // envelope's release is up to twelve seconds and truncating it
            // would be an all-sound-off.
            engine.releaseAllNotes();
        else if (message.isPitchWheel())
            engine.setPitchBend ((static_cast<float> (message.getPitchWheelValue())
                                  - 8192.0f) / 8192.0f);
        else if (message.isSysEx())
        {
            // A patch dump from the hardware, or from a librarian speaking to
            // it. The parameters cannot be written from here, so the tone bytes
            // are staged and applied on the message thread. Only the newest
            // dump in a block survives, which is what a bank transfer wants
            // anyway: the last one is the one that was asked for.
            const auto* raw = message.getRawData();
            const auto length = static_cast<std::size_t> (message.getRawDataSize());

            sysex::Patch incoming {};
            int channel = 0;
            int parameter = 0;
            int value = 0;

            if (sysex::readPatchMessage (raw, length, incoming, channel))
            {
                // A whole patch. The tone bytes are the message's own
                // representation, so they are handed over as they arrived.
                SysExEvent event;
                event.kind = SysExEventKind::FullPatch;
                sysex::toneBytesFromPatch (incoming, event.bytes.data());
                stageSysExEvent (event);
            }
            else if (sysex::readParameterMessage (raw, length, parameter, value,
                                                  channel))
            {
                // One control moved on the hardware. Only its number and value
                // cross over: the panel it applies to is read on the message
                // thread, so nothing else is quantised and no stale mirror of
                // the panel can revert an edit made in between.
                SysExEvent event;
                event.kind = SysExEventKind::SingleParameter;
                event.parameter = parameter;
                event.value = value;
                stageSysExEvent (event);
            }
        }
        else if (message.isController())
        {
            // The modelled instrument answers to modulation and hold, and to
            // nothing else: it has no continuous controllers for its panel.
            if (message.getControllerNumber() == 1)
                engine.setModWheel (static_cast<float> (message.getControllerValue())
                                    / 127.0f);
            else if (message.getControllerNumber() == 64)
                engine.setSustainPedal (message.getControllerValue() >= 64);
            else if (message.getControllerNumber() == 121)
            {
                // Reset All Controllers. Lifting the pedal matters most: with
                // it down and keys already released, ignoring this message
                // would leave those voices held until a panic.
                engine.setSustainPedal (false);
                engine.setModWheel (0.0f);
                engine.setPitchBend (0.0f);
            }
        }
    }

    if (renderedTo < numSamples)
        engine.process (buffer.getWritePointer (0, renderedTo),
                        buffer.getWritePointer (1, renderedTo),
                        numSamples - renderedTo);

    midiMessages.clear();

    activeVoiceCount.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);
    displayVoiceMask.store (engine.getDisplayVoiceMask(), std::memory_order_relaxed);
    displayEnvelope.store (engine.getDisplayEnvelope(), std::memory_order_relaxed);
    displayLfo.store (engine.getDisplayLfo(), std::memory_order_relaxed);
}

void YouKnow106AudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int,
                                             int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, true);
}

void YouKnow106AudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int,
                                              int midiNoteNumber, float)
{
    enqueueUiMidiEvent (midiNoteNumber, 0.0f, false);
}

void YouKnow106AudioProcessor::enqueueUiMidiEvent (int note, float velocity,
                                                   bool isNoteOn) noexcept
{
    const auto write = uiWriteIndex.load (std::memory_order_relaxed);
    const auto read = uiReadIndex.load (std::memory_order_acquire);
    if (write - read >= uiQueueCapacity)
    {
        // The queue only fills if processing has stalled for a long time. A
        // press dropped here is a note that never sounds, which is a shrug; a
        // release dropped here is a note held down for good, which is not. So
        // releases are never dropped -- they are remembered as a pending key
        // lift and applied once the backlog has been worked through.
        if (! isNoteOn)
        {
            const auto index = static_cast<unsigned> (juce::jlimit (0, 127, note));
            uiPendingNoteOff[index >> 6].fetch_or (1ull << (index & 63u),
                                                   std::memory_order_release);
        }
        return;
    }

    uiMidiQueue[write % uiQueueCapacity] = { note, velocity, isNoteOn };
    uiWriteIndex.store (write + 1, std::memory_order_release);
}

void YouKnow106AudioProcessor::discardUiMidiEvents() noexcept
{
    uiReadIndex.store (uiWriteIndex.load (std::memory_order_acquire),
                       std::memory_order_release);
    // Nothing is held any more, so there is no release left to honour.
    for (auto& pending : uiPendingNoteOff)
        pending.store (0, std::memory_order_release);
}

void YouKnow106AudioProcessor::dispatchUiMidiEvents() noexcept
{
    auto read = uiReadIndex.load (std::memory_order_relaxed);
    const auto write = uiWriteIndex.load (std::memory_order_acquire);

    // The on-screen keyboard has no sample clock, so its events arrive at the
    // block boundary with nothing to place them within the block. Applying a
    // whole backlog at that one instant would let a press and its release land
    // together and cancel out -- which is what happens when the audio thread
    // was stalled or the host buffer is long. So the drain stops at the second
    // event for any one key and leaves the rest queued: every press then gets
    // at least a block of its own, at the cost of a block of latency in the
    // case that would otherwise have lost the note entirely.
    std::array<std::uint64_t, 2> touched { 0u, 0u };

    while (read != write)
    {
        const auto event = uiMidiQueue[read % uiQueueCapacity];
        const auto note = static_cast<unsigned> (juce::jlimit (0, 127, event.note));
        const auto word = static_cast<std::size_t> (note >> 6);
        const std::uint64_t bit = 1ull << (note & 63u);
        if ((touched[word] & bit) != 0)
            break;
        touched[word] |= bit;

        if (event.noteOn)
            engine.noteOn (event.note, event.velocity);
        else
            engine.noteOff (event.note);
        ++read;
    }

    uiReadIndex.store (read, std::memory_order_release);

    // Keys whose release was dropped by a full queue. Two kinds of key have to
    // wait: one already touched in this drain, whose press would otherwise be
    // cancelled before anything is rendered, and one whose press is still
    // sitting further down the queue, which the release must not overtake.
    std::array<std::uint64_t, 2> deferred = touched;
    for (auto scan = read; scan != write; ++scan)
    {
        const auto queued = uiMidiQueue[scan % uiQueueCapacity];
        const auto note = static_cast<unsigned> (juce::jlimit (0, 127, queued.note));
        deferred[static_cast<std::size_t> (note >> 6)] |= 1ull << (note & 63u);
    }

    for (std::size_t word = 0; word < uiPendingNoteOff.size(); ++word)
    {
        const auto pending = uiPendingNoteOff[word].load (std::memory_order_acquire)
                           & ~deferred[word];
        if (pending == 0)
            continue;

        for (unsigned bit = 0; bit < 64; ++bit)
        {
            const std::uint64_t mask = 1ull << bit;
            if ((pending & mask) == 0)
                continue;
            uiPendingNoteOff[word].fetch_and (~mask, std::memory_order_acq_rel);
            engine.noteOff (static_cast<int> (word * 64u + bit));
        }
    }
}

void YouKnow106AudioProcessor::randomizeParameters (float amount)
{
    if (const auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
        messageManager != nullptr && ! messageManager->isThisTheMessageThread())
    {
        // Host gesture callbacks belong to the message thread. Refuse an
        // accidental call from anywhere else rather than racing the host's own
        // listeners.
        jassertfalse;
        return;
    }

    if (! std::isfinite (amount))
        return;
    amount = juce::jlimit (0.0f, 1.0f, amount);
    if (amount <= 0.0f)
        return;

    using namespace youknow106::parameters;
    // Deliberately sound-design controls only. Output level, voice count,
    // oversampling, and the two controls that describe the *instrument* rather
    // than the patch — Calibration and Chorus Noise — are excluded, so a
    // randomisation cannot mute the patch, jump its gain, change its running
    // cost, or quietly re-specify the hardware being modelled.
    static constexpr std::array<const char*, 29> soundParameterIds {{
        benderDco, benderVcf, benderLfo, portamento, poly1, poly2,
        lfoRate, lfoDelay,
        dcoLfo, pwm, pwmMode, range, saw, pulse, sub, noise,
        highPass,
        cutoff, resonance, envPolarity, vcfEnv, vcfLfo, keyFollow,
        vcaMode, vcaLevel,
        attack, decay, sustain, release,
        chorusI, chorusII
    }};

    juce::Random random;
    for (const auto* parameterId : soundParameterIds)
    {
        auto* parameter = parameters.getParameter (parameterId);
        jassert (parameter != nullptr);
        if (parameter == nullptr)
            continue;

        const float current = parameter->getValue();
        const float destination = random.nextFloat();
        const float requested = juce::jlimit (
            0.0f, 1.0f, current + amount * (destination - current));
        // Round through the parameter's own range before notifying the host, so
        // a switch cannot report a value between two of its positions.
        const float legal = parameter->convertTo0to1 (
            parameter->convertFrom0to1 (requested));
        // A switch's step can be wider than a subtle strength. Where it is,
        // leaving the control alone is more honest than letting quantisation
        // move it further than the button advertises.
        const float movement = std::abs (legal - current);
        if (movement <= 1.0e-7f || movement > amount + 1.0e-7f)
            continue;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (legal);
        parameter->endChangeGesture();
    }
}

void YouKnow106AudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (const auto xml = state.createXml())
            copyXmlToBinary (*xml, destinationData);
}

void YouKnow106AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid())
        return;

    // Sessions written before the paired switches were split still carry the
    // old three-way `keyMode` and `chorus` choices. Filling in defaults alone
    // would silently reopen a saved Unison as Poly 1 and a saved chorus as off,
    // so the old values are translated first.
    migrateSplitModeParameters (state);

    // A state written by an earlier build will not carry parameters added
    // since. Filling them with their defaults keeps the rest of the patch
    // rather than discarding the whole thing.
    for (const auto& pointer : parameterPointers)
        if (pointer.id != nullptr)
            addDefaultParameterStateIfMissing (state, parameters, pointer.id);

    // Deliberately no engine update here. This runs on the message thread while
    // the audio thread may be inside process(), and the engine's parameter
    // structs are plain values it reads without synchronisation. The next
    // processBlock picks the restored patch up on the thread that owns them.
    parameters.replaceState (state);
}

juce::AudioProcessorEditor* YouKnow106AudioProcessor::createEditor()
{
    return new YouKnow106AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YouKnow106AudioProcessor();
}

// ---------------------------------------------------------------------------
// Patches and the factory bank
// ---------------------------------------------------------------------------

// Program 0 is the init patch -- the layout's own defaults, which is what a
// freshly constructed processor actually holds. Without it, a new instance
// would report "A11 Hollow Strings" while sounding like nothing of the sort.
// The factory bank follows from index 1.
int YouKnow106AudioProcessor::getNumPrograms()
{
    return youknow106::presets::presetCount + 1;
}

int YouKnow106AudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void YouKnow106AudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    if (index == 0)
    {
        // Back to the init patch: every parameter to its own default.
        for (const auto& pointer : parameterPointers)
            if (pointer.id != nullptr)
                if (auto* parameter = parameters.getParameter (pointer.id))
                    parameter->setValueNotifyingHost (parameter->getDefaultValue());
        return;
    }
    const auto& bank = youknow106::presets::factoryBank();
    applyPatch (bank[static_cast<std::size_t> (index - 1)].patch);
}

const juce::String YouKnow106AudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};
    if (index == 0)
        return "INIT";
    const auto& preset =
        youknow106::presets::factoryBank()[static_cast<std::size_t> (index - 1)];
    juce::String name = juce::String (preset.number) + " " + preset.name;
    // A patch the hardware cannot store is still perfectly playable here, but
    // someone about to send a bank should be able to see which ones will not
    // survive the trip.
    if (! preset.exportsLosslessly())
        name += " (I+II)";
    return name;
}

void YouKnow106AudioProcessor::applyPatch (const youknow106::sysex::Patch& patch)
{
    using namespace youknow106::parameters;

    const auto set = [this] (const char* id, float value)
    {
        if (auto* parameter = parameters.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set (lfoRate, patch.lfoRate);
    set (lfoDelay, patch.lfoDelay);
    set (dcoLfo, patch.dcoLfo);
    set (pwm, patch.pwm);
    set (noise, patch.noise);
    set (cutoff, patch.cutoff);
    set (resonance, patch.resonance);
    set (vcfEnv, patch.vcfEnv);
    set (vcfLfo, patch.vcfLfo);
    set (keyFollow, patch.keyFollow);
    set (vcaLevel, patch.vcaLevel);
    set (attack, patch.attack);
    set (decay, patch.decay);
    set (sustain, patch.sustain);
    set (release, patch.release);
    set (sub, patch.sub);

    set (range, static_cast<float> (patch.range));
    set (saw, patch.saw ? 1.0f : 0.0f);
    set (pulse, patch.pulse ? 1.0f : 0.0f);
    set (pwmMode, static_cast<float> (patch.pwmSource));
    set (vcaMode, static_cast<float> (patch.vcaMode));
    set (envPolarity, static_cast<float> (patch.envPolarity));
    set (highPass, static_cast<float> (patch.highPass));
    set (chorusI, youknow106::chorusOneEngaged (patch.chorus) ? 1.0f : 0.0f);
    set (chorusII, youknow106::chorusTwoEngaged (patch.chorus) ? 1.0f : 0.0f);

    // Volume, the bender depths, portamento and the assign mode are performance
    // controls. The instrument does not store them in a patch, so loading one
    // deliberately leaves them where the player set them.
}

youknow106::sysex::Patch YouKnow106AudioProcessor::currentPatch() const
{
    using namespace youknow106::parameters;

    youknow106::sysex::Patch patch {};
    patch.lfoRate = valueOf (lfoRate);
    patch.lfoDelay = valueOf (lfoDelay);
    patch.dcoLfo = valueOf (dcoLfo);
    patch.pwm = valueOf (pwm);
    patch.noise = valueOf (noise);
    patch.cutoff = valueOf (cutoff);
    patch.resonance = valueOf (resonance);
    patch.vcfEnv = valueOf (vcfEnv);
    patch.vcfLfo = valueOf (vcfLfo);
    patch.keyFollow = valueOf (keyFollow);
    patch.vcaLevel = valueOf (vcaLevel);
    patch.attack = valueOf (attack);
    patch.decay = valueOf (decay);
    patch.sustain = valueOf (sustain);
    patch.release = valueOf (release);
    patch.sub = valueOf (sub);

    patch.range = static_cast<youknow106::DcoRange> (choiceOf (range, 2));
    patch.saw = valueOf (saw) > 0.5f;
    patch.pulse = valueOf (pulse) > 0.5f;
    patch.pwmSource = static_cast<youknow106::PwmSource> (choiceOf (pwmMode, 1));
    patch.vcaMode = static_cast<youknow106::VcaMode> (choiceOf (vcaMode, 1));
    patch.envPolarity =
        static_cast<youknow106::EnvPolarity> (choiceOf (envPolarity, 1));
    patch.highPass = static_cast<youknow106::HighPassMode> (choiceOf (highPass, 3));
    patch.chorus = youknow106::chorusModeFor (valueOf (chorusI) > 0.5f,
                                              valueOf (chorusII) > 0.5f);
    return patch;
}

juce::MidiMessage YouKnow106AudioProcessor::currentPatchAsSysEx (int channel) const
{
    std::array<std::uint8_t, youknow106::sysex::patchMessageBytes> raw {};
    const auto written = youknow106::sysex::writePatchMessage (
        currentPatch(), channel, raw.data(), raw.size());
    if (written == 0)
        return {};
    // JUCE wants the body without the leading F0 and trailing F7.
    return juce::MidiMessage::createSysExMessage (raw.data() + 1,
                                                  static_cast<int> (written) - 2);
}

// Audio thread. Claims the next slot, fills it, then publishes it. A slot is
// only written while its `ready` flag is clear, so the message thread is never
// reading the event this is writing.
void YouKnow106AudioProcessor::stageSysExEvent (const SysExEvent& event) noexcept
{
    const int index = sysExWriteIndex.load (std::memory_order_relaxed);
    auto& slot = sysExQueue[static_cast<std::size_t> (index)];
    if (slot.ready.load (std::memory_order_acquire))
    {
        // The message thread has not caught up. Dropping is the only safe move
        // -- overwriting a published slot would race a reader that may be
        // partway through it. Reaching here needs the message thread stalled
        // for most of a second at MIDI's own rate, and it is counted rather
        // than silent so a test can assert a whole bank transfer never does.
        sysExDropped.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    slot.event = event;
    slot.ready.store (true, std::memory_order_release);
    sysExWriteIndex.store ((index + 1) % sysExQueueSlots, std::memory_order_relaxed);
    triggerAsyncUpdate();
}

void YouKnow106AudioProcessor::handleAsyncUpdate()
{
    // Drain everything published so far. Single-parameter edits are cumulative,
    // so applying only the newest would lose the rest.
    for (int drained = 0; drained < sysExQueueSlots; ++drained)
    {
        auto& slot = sysExQueue[static_cast<std::size_t> (sysExReadIndex)];
        if (! slot.ready.load (std::memory_order_acquire))
            return;

        const auto event = slot.event;
        // Release the slot only after the event has been copied out of it.
        slot.ready.store (false, std::memory_order_release);
        sysExReadIndex = (sysExReadIndex + 1) % sysExQueueSlots;

        if (event.kind == SysExEventKind::FullPatch)
        {
            applyPatch (youknow106::sysex::patchFromToneBytes (event.bytes.data()));
        }
        else
        {
            // The base is read here rather than carried across, so it reflects
            // every panel, preset, state and automation change made since --
            // and no control the message did not name is touched.
            auto patch = currentPatch();
            if (youknow106::sysex::applyParameter (patch, event.parameter, event.value))
                applyPatch (patch);
        }
    }
}
