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
} // namespace

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
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { keyMode, 1 }, "Key Mode",
        juce::StringArray { "Poly 1", "Poly 2", "Unison" }, 0));

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

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { chorus, 1 }, "Chorus",
        juce::StringArray { "Off", "I", "II" }, 0));

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
        volume, benderDco, benderVcf, benderLfo, portamento, keyMode,
        lfoRate, lfoDelay, dcoLfo, pwm, pwmMode, range, saw, pulse, sub, noise,
        highPass, cutoff, resonance, envPolarity, vcfEnv, vcfLfo, keyFollow,
        vcaMode, vcaLevel, attack, decay, sustain, release, chorus,
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
    engineParameters.keyMode = static_cast<KeyMode> (choiceOf (keyMode, 2));

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

    engineParameters.chorus = static_cast<ChorusMode> (choiceOf (chorus, 2));

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
        else if (message.isController())
        {
            // The hardware's MIDI receives hold and nothing else among the
            // control changes -- not even modulation. Mapping CC 1 onto the
            // bender lever's push-away axis is a plug-in extension, inert at
            // its default depth; the panel has no continuous controllers at
            // all.
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
        benderDco, benderVcf, benderLfo, portamento, keyMode,
        lfoRate, lfoDelay,
        dcoLfo, pwm, pwmMode, range, saw, pulse, sub, noise,
        highPass,
        cutoff, resonance, envPolarity, vcfEnv, vcfLfo, keyFollow,
        vcaMode, vcaLevel,
        attack, decay, sustain, release,
        chorus
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
