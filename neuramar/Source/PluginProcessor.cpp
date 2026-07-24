#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <string>

namespace
{
float valueOf (const std::atomic<float>* value, float fallback = 0.0f) noexcept
{
    return value != nullptr ? value->load (std::memory_order_relaxed) : fallback;
}

std::unique_ptr<juce::RangedAudioParameter> makePercentParameter (
    const char* id, const char* name, float defaultValue, float maximum = 1.0f)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> { 0.0f, maximum, 0.001f }, defaultValue,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([maximum] (float value, int)
            {
                return juce::String (juce::roundToInt (100.0f * value / maximum));
            })
            .withValueFromStringFunction ([maximum] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue()
                     * maximum / 100.0f;
            }));
}

juce::String learningErrorPrefix (const juce::String& sourceName)
{
    return sourceName.isNotEmpty() ? sourceName + ": " : juce::String {};
}

bool containsParameterState (
    const juce::ValueTree& state, const char* parameterId)
{
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };
    for (const auto& child : state)
    {
        if (child.hasType (parameterType)
            && child.getProperty (idProperty).toString() == parameterId)
            return true;
    }
    return false;
}

void addDefaultParameterStateIfMissing (
    juce::ValueTree& state,
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
            valueProperty,
            parameter->convertFrom0to1 (parameter->getDefaultValue()),
            nullptr);
        state.appendChild (parameterState, nullptr);
    }
}

void addLegacyTouchIfMissing (juce::ValueTree& state)
{
    if (containsParameterState (state, neuramar::parameters::touch))
        return;

    // Touch is the one appended control whose declared default is not its
    // identity value: the pre-1.1 synthesis path is Touch 0, which leaves the
    // harmonic tilt and Air gain untouched at every velocity, while new
    // instances open at 0.35. Restoring the declared default here would make a
    // legacy memory respond to velocity, which is what this migration exists
    // to prevent.
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };
    static const juce::Identifier valueProperty { "value" };
    juce::ValueTree parameterState { parameterType };
    parameterState.setProperty (idProperty, neuramar::parameters::touch, nullptr);
    parameterState.setProperty (valueProperty, 0.0f, nullptr);
    state.appendChild (parameterState, nullptr);
}

std::uint64_t mixRandomizationSeed (std::uint64_t value) noexcept
{
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebull;
    return value ^ (value >> 31u);
}

float randomizationStrength (
    NeuramarAudioProcessor::RandomizationAmount amount) noexcept
{
    switch (amount)
    {
        case NeuramarAudioProcessor::RandomizationAmount::Subtle1Percent:
            return 0.01f;
        case NeuramarAudioProcessor::RandomizationAmount::Evolve10Percent:
            return 0.10f;
        case NeuramarAudioProcessor::RandomizationAmount::Wild100Percent:
            return 1.0f;
    }

    return 0.10f;
}

juce::String randomizationLabel (
    NeuramarAudioProcessor::RandomizationAmount amount)
{
    switch (amount)
    {
        case NeuramarAudioProcessor::RandomizationAmount::Subtle1Percent:
            return "1%";
        case NeuramarAudioProcessor::RandomizationAmount::Evolve10Percent:
            return "10%";
        case NeuramarAudioProcessor::RandomizationAmount::Wild100Percent:
            return "100%";
    }

    return "10%";
}
} // namespace

NeuramarAudioProcessor::NeuramarAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "NEURAMAR_STATE", createParameterLayout())
{
    randomizationSeedBase = mixRandomizationSeed (
        static_cast<std::uint64_t> (juce::Time::getHighResolutionTicks())
        ^ (static_cast<std::uint64_t> (juce::Time::currentTimeMillis()) << 1u)
        ^ static_cast<std::uint64_t> (
            reinterpret_cast<std::uintptr_t> (this)));

    parameterPointers.imprint = parameters.getRawParameterValue (neuramar::parameters::imprint);
    parameterPointers.bodyLock = parameters.getRawParameterValue (neuramar::parameters::bodyLock);
    parameterPointers.air = parameters.getRawParameterValue (neuramar::parameters::air);
    parameterPointers.bone = parameters.getRawParameterValue (neuramar::parameters::bone);
    parameterPointers.brightness = parameters.getRawParameterValue (neuramar::parameters::brightness);
    parameterPointers.evolutionRate = parameters.getRawParameterValue (neuramar::parameters::evolutionRate);
    parameterPointers.orbit = parameters.getRawParameterValue (neuramar::parameters::orbit);
    parameterPointers.mutation = parameters.getRawParameterValue (neuramar::parameters::mutation);
    parameterPointers.attack = parameters.getRawParameterValue (neuramar::parameters::attack);
    parameterPointers.release = parameters.getRawParameterValue (neuramar::parameters::release);
    parameterPointers.spread = parameters.getRawParameterValue (neuramar::parameters::spread);
    parameterPointers.rootCorrection = parameters.getRawParameterValue (neuramar::parameters::rootCorrection);
    parameterPointers.output = parameters.getRawParameterValue (neuramar::parameters::output);
    parameterPointers.noise = parameters.getRawParameterValue (neuramar::parameters::noise);
    parameterPointers.stretch = parameters.getRawParameterValue (neuramar::parameters::stretch);
    parameterPointers.formant = parameters.getRawParameterValue (neuramar::parameters::formant);
    parameterPointers.touch = parameters.getRawParameterValue (neuramar::parameters::touch);

    neuramar::clearModelAnatomy (displayAnatomy.anatomy);
    keyboardState.addListener (this);
    displayState.message = "Drop a sound or press Randomize, then play MIDI.";
}

NeuramarAudioProcessor::~NeuramarAudioProcessor()
{
    keyboardState.removeListener (this);
    {
        const std::scoped_lock lock (learningThreadMutex);
        if (learningThread.joinable())
        {
            learningThread.request_stop();
            learningThread.join();
        }
    }
    engine.setModel (nullptr);
    audioModelHazard.store (nullptr, std::memory_order_seq_cst);
}

juce::AudioProcessorValueTreeState::ParameterLayout
NeuramarAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;
    result.reserve (17);

    result.push_back (makePercentParameter (
        neuramar::parameters::imprint, "Imprint", 0.90f));
    result.push_back (makePercentParameter (
        neuramar::parameters::bodyLock, "Body Lock", 0.52f));
    result.push_back (makePercentParameter (
        neuramar::parameters::air, "Air", 0.82f));
    result.push_back (makePercentParameter (
        neuramar::parameters::bone, "Bone", 0.78f));

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { neuramar::parameters::brightness, 1 }, "Gravity",
        juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f }, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f));
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue() / 100.0f;
            })));

    auto evolutionRange = juce::NormalisableRange<float> { 0.25f, 4.0f, 0.001f };
    evolutionRange.setSkewForCentre (1.0f);
    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { neuramar::parameters::evolutionRate, 1 }, "Memory Rate",
        evolutionRange, 1.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("x")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 2);
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue();
            })));

    result.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { neuramar::parameters::orbit, 1 }, "Orbit", true));
    result.push_back (makePercentParameter (
        neuramar::parameters::mutation, "Mutation", 0.12f));

    auto attackRange = juce::NormalisableRange<float> { 0.0f, 2.0f, 0.0001f };
    attackRange.setSkewForCentre (0.08f);
    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { neuramar::parameters::attack, 1 }, "Awaken",
        attackRange, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    auto releaseRange = juce::NormalisableRange<float> { 0.02f, 8.0f, 0.001f };
    releaseRange.setSkewForCentre (0.8f);
    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { neuramar::parameters::release, 1 }, "Dissolve",
        releaseRange, 0.65f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    result.push_back (makePercentParameter (
        neuramar::parameters::spread, "Horizon", 0.58f));

    result.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { neuramar::parameters::rootCorrection, 1 },
        "Root Correction", -12, 12, 0,
        juce::AudioParameterIntAttributes().withLabel ("st")));

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { neuramar::parameters::output, 1 }, "Output",
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Appended to preserve the established index and automation identity of
    // every parameter above. This drives neural-coordinate evolution; it does
    // not mix an additive noise generator into the audio output.
    result.push_back (makePercentParameter (
        neuramar::parameters::noise, "Noise", 0.0f));

    // Also appended, for the same reason. Stretch scales the model's fitted
    // stiff-string coefficient, so 100% is "exactly as learned" and a memory
    // without one is unaffected at any setting.
    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { neuramar::parameters::stretch, 1 }, "Stretch",
        juce::NormalisableRange<float> { 0.0f, 2.0f, 0.001f }, 1.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f));
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue()
                     / 100.0f;
            })));

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { neuramar::parameters::formant, 1 }, "Formant",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("st")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 2);
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue();
            })));

    result.push_back (makePercentParameter (
        neuramar::parameters::touch, "Touch", 0.35f));

    return { result.begin(), result.end() };
}

void NeuramarAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engineReady.store (false, std::memory_order_release);
    discardUiMidiEvents();
    engine.prepare (sampleRate, samplesPerBlock);
    installPublishedModelOnAudioThread();
    updateEngineParameters();
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    engineReady.store (true, std::memory_order_release);
}

void NeuramarAudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    discardUiMidiEvents();
    engine.allSoundOff();
    engine.reset();
    engine.setModel (nullptr);
    audioModelHazard.store (nullptr, std::memory_order_seq_cst);
    audioModelGeneration = publishedGeneration.load (std::memory_order_acquire);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    displaySampleRate.store (0.0, std::memory_order_relaxed);
}

bool NeuramarAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NeuramarAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! engineReady.load (std::memory_order_acquire))
        return;

    const auto generation = publishedGeneration.load (std::memory_order_acquire);
    if (generation != audioModelGeneration)
        installPublishedModelOnAudioThread();

    updateEngineParameters();
    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        discardUiMidiEvents();
        engine.allSoundOff();
    }

    dispatchUiMidiEvents();

    const auto numSamples = buffer.getNumSamples();
    auto renderedTo = 0;
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
}

void NeuramarAudioProcessor::dispatchMidiData (const juce::uint8* data,
                                               int numBytes) noexcept
{
    if (data == nullptr || numBytes < 1)
        return;

    const auto kind = static_cast<unsigned> (data[0]) & 0xf0u;
    if (kind == 0x90u && numBytes >= 3 && data[2] != 0)
    {
        engine.noteOn (static_cast<int> (data[1] & 0x7fu),
                       static_cast<float> (data[2] & 0x7fu) / 127.0f);
    }
    else if ((kind == 0x80u && numBytes >= 2)
             || (kind == 0x90u && numBytes >= 3 && data[2] == 0))
    {
        engine.noteOff (static_cast<int> (data[1] & 0x7fu));
    }
    else if (kind == 0xb0u && numBytes >= 3)
    {
        const auto controller = data[1] & 0x7fu;
        if (controller == 120u)
            engine.allSoundOff();
        else if (controller == 123u)
            engine.allNotesOff();
    }
}

void NeuramarAudioProcessor::updateEngineParameters() noexcept
{
    neuramar::EngineParameters next;
    next.imprint = valueOf (parameterPointers.imprint, 0.9f);
    next.bodyLock = valueOf (parameterPointers.bodyLock, 0.52f);
    next.air = valueOf (parameterPointers.air, 0.82f);
    next.bone = valueOf (parameterPointers.bone, 0.78f);
    next.brightness = 0.5f + 0.5f * valueOf (parameterPointers.brightness);
    next.evolutionRate = valueOf (parameterPointers.evolutionRate, 1.0f);
    next.orbit = valueOf (parameterPointers.orbit, 1.0f) >= 0.5f ? 1.0f : 0.0f;
    next.mutation = valueOf (parameterPointers.mutation, 0.12f);
    next.noise = valueOf (parameterPointers.noise, 0.0f);
    next.attackSeconds = valueOf (parameterPointers.attack, 0.0f);
    next.releaseSeconds = valueOf (parameterPointers.release, 0.65f);
    next.spread = valueOf (parameterPointers.spread, 0.58f);
    next.rootOffsetSemitones = static_cast<float> (
        juce::roundToInt (valueOf (parameterPointers.rootCorrection)));
    next.outputGain = juce::Decibels::decibelsToGain (
        valueOf (parameterPointers.output, -6.0f));
    next.stretch = valueOf (parameterPointers.stretch, 1.0f);
    next.formantShiftSemitones = valueOf (parameterPointers.formant, 0.0f);
    next.touch = valueOf (parameterPointers.touch, 0.35f);
    engine.setParameters (next);
}

void NeuramarAudioProcessor::installPublishedModelOnAudioThread() noexcept
{
    const neuramar::NeuralModel* candidate = nullptr;
    std::uint64_t generation = 0;
    do
    {
        generation = publishedGeneration.load (std::memory_order_acquire);
        candidate = publishedModel.load (std::memory_order_seq_cst);
        // Publish the hazard before validating the snapshot. If publication
        // races us, we retry without dereferencing the stale candidate; if the
        // snapshot validates, a publisher must observe this hazard before it
        // is allowed to reclaim the candidate.
        audioModelHazard.store (candidate, std::memory_order_seq_cst);
    }
    while (generation != publishedGeneration.load (std::memory_order_acquire)
           || candidate != publishedModel.load (std::memory_order_seq_cst));

    engine.setModel (candidate);
    audioModelGeneration = generation;
}

void NeuramarAudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int,
                                           int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, true);
}

void NeuramarAudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int,
                                            int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, false);
}

void NeuramarAudioProcessor::enqueueUiMidiEvent (int note, float velocity,
                                                  bool isNoteOn) noexcept
{
    const auto write = uiWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1u) % uiQueueCapacity;
    if (next == uiReadIndex.load (std::memory_order_acquire))
    {
        panicRequested.store (true, std::memory_order_release);
        return;
    }

    uiMidiQueue[write] = { juce::jlimit (0, 127, note),
                           juce::jlimit (0.0f, 1.0f, velocity), isNoteOn };
    uiWriteIndex.store (next, std::memory_order_release);
}

void NeuramarAudioProcessor::dispatchUiMidiEvents() noexcept
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

void NeuramarAudioProcessor::discardUiMidiEvents() noexcept
{
    uiReadIndex.store (uiWriteIndex.load (std::memory_order_acquire),
                       std::memory_order_release);
}

void NeuramarAudioProcessor::requestPanic() noexcept
{
    panicRequested.store (true, std::memory_order_release);
}

bool NeuramarAudioProcessor::isSupportedSampleFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".wave"
        || extension == ".aif" || extension == ".aiff"
        || extension == ".flac" || extension == ".ogg";
}

juce::String NeuramarAudioProcessor::noteName (int midiNote)
{
    static constexpr std::array<const char*, 12> names {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    if (midiNote < 0 || midiNote > 127)
        return "--";
    return juce::String (names[static_cast<std::size_t> (midiNote % 12)])
         + juce::String (midiNote / 12 - 1);
}

void NeuramarAudioProcessor::loadSampleFile (const juce::File& file)
{
    const std::scoped_lock operationLock (learningOperationMutex);
    cancelLearning();
    if (! isSupportedSampleFile (file))
    {
        setTerminalLearningStatus (
            LearningStage::Error,
            "Choose a readable WAV, AIFF, FLAC, or OGG file.");
        return;
    }

    {
        const juce::ScopedLock lock (displayLock);
        displayState.sourceName = file.getFileName();
        displayState.message = "Reading " + file.getFileName();
        displayState.stage = LearningStage::Reading;
        displayState.progress = 0.01f;
        displayState.generatedModel = false;
    }

    const std::scoped_lock threadLock (learningThreadMutex);
    const auto attempt = 2u * (
        learningAttemptCounter.fetch_add (1, std::memory_order_relaxed) + 1u);
    learningAttemptState.store (attempt, std::memory_order_release);
    learningThread = std::jthread ([this, file, attempt] (std::stop_token stopToken)
    {
        const auto finishError = [this, attempt] (juce::String message)
        {
            auto expected = attempt;
            if (learningAttemptState.compare_exchange_strong (
                    expected, attempt | 1u,
                    std::memory_order_acq_rel, std::memory_order_acquire))
                setTerminalLearningStatus (LearningStage::Error,
                                           std::move (message));
        };

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        auto reader = std::unique_ptr<juce::AudioFormatReader> (
            formatManager.createReaderFor (file));
        if (reader == nullptr || ! std::isfinite (reader->sampleRate)
            || reader->sampleRate < 8000.0 || reader->sampleRate > 768000.0
            || reader->numChannels == 0)
        {
            finishError (learningErrorPrefix (file.getFileName())
                         + "unsupported or damaged audio data");
            return;
        }

        constexpr double maximumReadSeconds = 12.0;
        const auto maximumSamples = static_cast<juce::int64> (
            std::ceil (reader->sampleRate * maximumReadSeconds));
        const auto samplesToRead = std::min (reader->lengthInSamples, maximumSamples);
        if (samplesToRead < static_cast<juce::int64> (reader->sampleRate * 0.06))
        {
            finishError ("The sound is too short; use at least 60 ms.");
            return;
        }

        constexpr int decodeChunkSize = 8192;
        const auto channelCount = static_cast<int> (
            std::min<juce::int64> (reader->numChannels, 2));
        juce::AudioBuffer<float> decoded (channelCount, decodeChunkSize);
        std::vector<float> mono (static_cast<std::size_t> (samplesToRead), 0.0f);
        const auto channelGain = 1.0f / static_cast<float> (channelCount);
        for (juce::int64 offset = 0; offset < samplesToRead;
             offset += decodeChunkSize)
        {
            if (stopToken.stop_requested())
                return;
            const auto count = static_cast<int> (
                std::min<juce::int64> (decodeChunkSize, samplesToRead - offset));
            decoded.clear();
            if (! reader->read (&decoded, 0, count, offset, true, true))
            {
                finishError (
                    "The audio decoder stopped before the sound was read.");
                return;
            }

            for (int channel = 0; channel < channelCount; ++channel)
            {
                const auto* source = decoded.getReadPointer (channel);
                for (int index = 0; index < count; ++index)
                    mono[static_cast<std::size_t> (offset + index)]
                        += source[index] * channelGain;
            }
        }
        if (stopToken.stop_requested())
            return;

        makeWaveformPreview (mono, attempt);
        if (stopToken.stop_requested())
            return;
        setLearningStatus (LearningStage::FindingRoot, 0.06f,
                           "Listening past the attack for a stable fundamental...",
                           attempt);
        runLearning (stopToken, std::move (mono), reader->sampleRate,
                     file.getFileName(), attempt);
    });
}

void NeuramarAudioProcessor::learnSampleData (std::vector<float> monoSamples,
                                               double sourceSampleRate,
                                               juce::String sourceName)
{
    const std::scoped_lock operationLock (learningOperationMutex);
    cancelLearning();
    makeWaveformPreview (monoSamples);
    {
        const juce::ScopedLock lock (displayLock);
        displayState.sourceName = sourceName;
        displayState.message = "Finding the root before the neural fit...";
        displayState.stage = LearningStage::FindingRoot;
        displayState.progress = 0.04f;
        displayState.generatedModel = false;
    }
    startLearningWorker (std::move (monoSamples), sourceSampleRate,
                         std::move (sourceName));
}

void NeuramarAudioProcessor::startLearningWorker (std::vector<float> monoSamples,
                                                   double sourceSampleRate,
                                                   juce::String sourceName)
{
    const std::scoped_lock threadLock (learningThreadMutex);
    const auto attempt = 2u * (
        learningAttemptCounter.fetch_add (1, std::memory_order_relaxed) + 1u);
    learningAttemptState.store (attempt, std::memory_order_release);
    learningThread = std::jthread (
        [this, samples = std::move (monoSamples), sourceSampleRate,
         name = std::move (sourceName), attempt] (std::stop_token stopToken) mutable
        {
            runLearning (stopToken, std::move (samples), sourceSampleRate,
                         std::move (name), attempt);
        });
}

void NeuramarAudioProcessor::runLearning (std::stop_token stopToken,
                                           std::vector<float> monoSamples,
                                           double sourceSampleRate,
                                           juce::String sourceName,
                                           std::uint64_t attemptState)
{
    const auto progressCallback = [this, attemptState] (
        const neuramar::SampleLearner::Progress& progress)
    {
        LearningStage mappedStage = LearningStage::Analysing;
        switch (progress.stage)
        {
            case neuramar::SampleLearner::Stage::Conditioning:
                mappedStage = LearningStage::Reading;
                break;
            case neuramar::SampleLearner::Stage::Pitch:
                mappedStage = LearningStage::FindingRoot;
                break;
            case neuramar::SampleLearner::Stage::Analysis:
                mappedStage = LearningStage::Analysing;
                break;
            case neuramar::SampleLearner::Stage::Training:
                mappedStage = LearningStage::Training;
                break;
        }

        const juce::ScopedLock lock (displayLock);
        if (learningAttemptState.load (std::memory_order_acquire) != attemptState)
            return;
        displayState.stage = mappedStage;
        displayState.progress = juce::jlimit (0.0f, 0.995f, progress.overallProgress);
        displayState.message = juce::String::fromUTF8 (progress.message.c_str());
        if (progress.rootMidiNote >= 0 && progress.rootFrequencyHz > 0.0f)
        {
            displayState.rootFrequencyHz = progress.rootFrequencyHz;
            displayState.rootMidiNote = progress.rootMidiNote;
            displayState.rootCents = progress.rootCents;
            displayState.rootConfidence = progress.pitchConfidence;
        }
    };

    auto result = neuramar::SampleLearner::learn (
        monoSamples, sourceSampleRate, progressCallback,
        [&stopToken] { return stopToken.stop_requested(); });

    if (stopToken.stop_requested() || result.cancelled)
        return;

    const auto claimCompletion = [this, attemptState]
    {
        auto expected = attemptState;
        return learningAttemptState.compare_exchange_strong (
            expected, attemptState | 1u,
            std::memory_order_acq_rel, std::memory_order_acquire);
    };
    if (result.model == nullptr)
    {
        if (claimCompletion())
            setTerminalLearningStatus (
                LearningStage::Error,
                juce::String::fromUTF8 (result.error.c_str()));
        return;
    }

    if (claimCompletion())
        publishModel (std::move (result.model), std::move (sourceName));
}

void NeuramarAudioProcessor::requestLearningCancellation()
{
    const std::scoped_lock operationLock (learningOperationMutex);
    auto cancellationWon = false;
    {
        const std::scoped_lock lock (learningThreadMutex);
        if (! learningThread.joinable())
            return;

        auto state = learningAttemptState.load (std::memory_order_acquire);
        while (state != 0u && (state & 1u) == 0u)
        {
            if (learningAttemptState.compare_exchange_weak (
                    state, state | 1u,
                    std::memory_order_acq_rel, std::memory_order_acquire))
            {
                cancellationWon = true;
                break;
            }
        }
        learningThread.request_stop();
    }
    if (cancellationWon)
        setTerminalLearningStatus (
            LearningStage::Cancelled,
            "Learning stopped; the previous memory is still playable.");
}

void NeuramarAudioProcessor::cancelLearning()
{
    const std::scoped_lock operationLock (learningOperationMutex);
    requestLearningCancellation();
    const std::scoped_lock lock (learningThreadMutex);
    if (learningThread.joinable())
    {
        learningThread.request_stop();
        learningThread.join();
    }
}

std::uint64_t NeuramarAudioProcessor::nextRandomizationSeed() noexcept
{
    constexpr std::uint64_t goldenRatio = 0x9e3779b97f4a7c15ull;
    const auto ordinal = randomizationCounter.fetch_add (
        1u, std::memory_order_relaxed) + 1u;
    return mixRandomizationSeed (
        randomizationSeedBase + goldenRatio * ordinal);
}

void NeuramarAudioProcessor::randomizeModel (RandomizationAmount amount)
{
    // This is intentionally a non-realtime operation. It may join the bounded
    // learner and allocates a fresh immutable model before publication.
    const std::scoped_lock operationLock (learningOperationMutex);
    cancelLearning();

    const auto seed = nextRandomizationSeed();
    const auto strength = randomizationStrength (amount);
    const auto amountText = randomizationLabel (amount);
    auto variedExistingModel = false;
    auto sourceName = juce::String {};
    std::unique_ptr<neuramar::NeuralModel> model;

    {
        // Holding the archive lock keeps the current immutable model alive
        // while its variation is constructed. The audio callback never takes
        // this lock and continues rendering the old generation throughout.
        const std::scoped_lock lock (modelArchiveMutex);
        if (const auto* current = publishedModel.load (std::memory_order_seq_cst))
        {
            model = current->createRandomizedVariation (seed, strength);
            sourceName = publishedSourceName;
            variedExistingModel = true;
        }
    }

    if (! variedExistingModel)
        model = neuramar::NeuralModel::createRandom (seed, strength);

    if (model == nullptr)
    {
        setTerminalLearningStatus (
            LearningStage::Error,
            "The neural randomizer could not create a valid memory.");
        return;
    }

    if (variedExistingModel)
    {
        static constexpr auto variationMarker = " / variation ";
        const auto priorVariation = sourceName.indexOf (variationMarker);
        if (priorVariation >= 0)
            sourceName = sourceName.substring (0, priorVariation);
        if (sourceName.isEmpty())
            sourceName = "Neural memory";
        sourceName = sourceName.substring (0, 896)
                   + variationMarker + amountText;
    }
    else
    {
        sourceName = "Random neural seed " + amountText;
    }

    publishModel (
        std::move (model), std::move (sourceName),
        variedExistingModel
            ? "Neural weights varied across " + amountText
                + " of their musical range - play any MIDI note."
            : "A playable neural memory was generated at " + amountText
                + " range - no source sample required.");
}

void NeuramarAudioProcessor::publishModel (
    std::unique_ptr<neuramar::NeuralModel> model, juce::String sourceName,
    juce::String readyMessage)
{
    if (model == nullptr)
        return;

    sourceName = sourceName.substring (0, 1024);
    const auto* modelPointer = model.get();
    std::uint64_t generation = 0;
    neuramar::NeuralModel::Metadata publishedMetadata;
    ModelAnatomySnapshot anatomy;
    {
        const std::scoped_lock lock (modelArchiveMutex);
        generation = publishedGeneration.load (std::memory_order_relaxed) + 1u;
        modelArchive.push_back ({ generation, std::move (model) });
        publishedSourceName = sourceName;
        publishedModel.store (modelPointer, std::memory_order_seq_cst);
        publishedGeneration.store (generation, std::memory_order_release);
        reclaimRetiredModelsLocked();

        // Everything the display needs is read while the archive lock still
        // guarantees this model is alive; a concurrent publication on another
        // thread can then reclaim it without racing these reads. Reducing the
        // model for the editor also happens here, on the thread that publishes
        // it, never on the audio thread and never in a paint call.
        publishedMetadata = modelPointer->metadata();
        anatomy.modelGeneration = generation;
        neuramar::buildModelAnatomy (*modelPointer, anatomy.anatomy);
    }

    const juce::ScopedLock lock (displayLock);
    displayAnatomy = std::move (anatomy);
    displayState.stage = LearningStage::Ready;
    displayState.progress = 1.0f;
    displayState.rootFrequencyHz = publishedMetadata.rootFrequencyHz;
    displayState.rootMidiNote = publishedMetadata.rootMidiNote;
    displayState.rootCents = publishedMetadata.rootCents;
    displayState.rootConfidence = publishedMetadata.pitchConfidence;
    displayState.generatedModel = publishedMetadata.trainingEpochs == 0;
    displayState.waveform = publishedMetadata.waveformPreview;
    displayState.modelGeneration = generation;
    displayState.sourceName = std::move (sourceName);
    displayState.message = readyMessage.isNotEmpty()
        ? std::move (readyMessage)
        : "Compact neural memory learned locally - play any MIDI note.";
}

void NeuramarAudioProcessor::publishRestoredModel (
    std::unique_ptr<neuramar::NeuralModel> model, juce::String sourceName)
{
    publishModel (std::move (model), std::move (sourceName));
    const juce::ScopedLock lock (displayLock);
    displayState.message = "The compact neural memory was restored with this session.";
}

void NeuramarAudioProcessor::clearPublishedModel (juce::String message)
{
    {
        const std::scoped_lock lock (modelArchiveMutex);
        publishedSourceName.clear();
        publishedModel.store (nullptr, std::memory_order_seq_cst);
        publishedGeneration.store (
            publishedGeneration.load (std::memory_order_relaxed) + 1u,
            std::memory_order_release);
        reclaimRetiredModelsLocked();
    }

    const juce::ScopedLock lock (displayLock);
    displayState = {};
    displayState.stage = LearningStage::Empty;
    displayState.message = std::move (message);
    displayAnatomy = {};
    neuramar::clearModelAnatomy (displayAnatomy.anatomy);
}

void NeuramarAudioProcessor::setLearningStatus (LearningStage stage, float progress,
                                                 juce::String message,
                                                 std::uint64_t attemptState)
{
    const juce::ScopedLock lock (displayLock);
    if (attemptState != 0u
        && learningAttemptState.load (std::memory_order_acquire) != attemptState)
        return;
    displayState.stage = stage;
    displayState.progress = juce::jlimit (0.0f, 1.0f, progress);
    if (message.isNotEmpty())
        displayState.message = std::move (message);
}

void NeuramarAudioProcessor::setTerminalLearningStatus (
    LearningStage stage, juce::String message)
{
    LearningSnapshot restored;
    const std::scoped_lock archiveLock (modelArchiveMutex);
    if (const auto* model = publishedModel.load (std::memory_order_seq_cst))
    {
        const auto& metadata = model->metadata();
        restored.rootFrequencyHz = metadata.rootFrequencyHz;
        restored.rootMidiNote = metadata.rootMidiNote;
        restored.rootCents = metadata.rootCents;
        restored.rootConfidence = metadata.pitchConfidence;
        restored.generatedModel = metadata.trainingEpochs == 0;
        restored.waveform = metadata.waveformPreview;
        restored.modelGeneration = publishedGeneration.load (
            std::memory_order_acquire);
        restored.sourceName = publishedSourceName;
    }
    restored.stage = stage;
    restored.progress = 0.0f;
    restored.message = std::move (message);

    const juce::ScopedLock lock (displayLock);
    displayState = std::move (restored);
}

void NeuramarAudioProcessor::reclaimRetiredModelsLocked()
{
    const auto* current = publishedModel.load (std::memory_order_seq_cst);
    const auto* hazard = audioModelHazard.load (std::memory_order_seq_cst);
    modelArchive.erase (
        std::remove_if (modelArchive.begin(), modelArchive.end(),
                        [current, hazard] (const ArchivedModel& archived)
                        {
                            const auto* pointer = archived.model.get();
                            return pointer != current && pointer != hazard;
                        }),
        modelArchive.end());
}

void NeuramarAudioProcessor::makeWaveformPreview (
    const std::vector<float>& samples, std::uint64_t attemptState)
{
    std::array<float, 256> preview {};
    if (! samples.empty())
    {
        auto maximum = 0.0f;
        for (std::size_t bin = 0; bin < preview.size(); ++bin)
        {
            const auto begin = bin * samples.size() / preview.size();
            const auto end = std::max (begin + 1,
                                       (bin + 1) * samples.size() / preview.size());
            auto chosen = 0.0f;
            for (auto index = begin; index < std::min (end, samples.size()); ++index)
                if (std::abs (samples[index]) > std::abs (chosen))
                    chosen = samples[index];
            preview[bin] = std::isfinite (chosen) ? chosen : 0.0f;
            maximum = std::max (maximum, std::abs (preview[bin]));
        }
        if (maximum > 1.0e-6f)
            for (auto& value : preview)
                value /= maximum;
    }

    const juce::ScopedLock lock (displayLock);
    if (attemptState != 0u
        && learningAttemptState.load (std::memory_order_acquire) != attemptState)
        return;
    displayState.waveform = preview;
}

NeuramarAudioProcessor::LearningSnapshot
NeuramarAudioProcessor::getLearningSnapshot() const
{
    const juce::ScopedLock lock (displayLock);
    return displayState;
}

NeuramarAudioProcessor::ModelAnatomySnapshot
NeuramarAudioProcessor::getModelAnatomySnapshot() const
{
    const juce::ScopedLock lock (displayLock);
    return displayAnatomy;
}

void NeuramarAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    juce::MemoryOutputStream stream (destinationData, false);
    stream.writeInt (static_cast<int> (stateMagic));
    stream.writeInt (static_cast<int> (stateVersion));

    auto xmlText = juce::String {};
    if (const auto xml = parameters.copyState().createXml())
        xmlText = xml->toString();
    const auto xmlBytes = xmlText.toRawUTF8();
    const auto xmlLength = static_cast<int> (std::strlen (xmlBytes));
    stream.writeInt (xmlLength);
    if (xmlLength > 0)
        stream.write (xmlBytes, static_cast<std::size_t> (xmlLength));

    std::vector<std::uint8_t> modelBytes;
    juce::String sourceName;
    {
        // The display may already show the next file being analysed while the
        // prior model is still playable. Snapshot the immutable model and its
        // own source label under the publication lock so host state cannot mix
        // two learning generations.
        const std::scoped_lock lock (modelArchiveMutex);
        if (const auto* model = publishedModel.load (std::memory_order_acquire))
            modelBytes = model->serialize();
        sourceName = publishedSourceName;
    }
    stream.writeInt (static_cast<int> (modelBytes.size()));
    if (! modelBytes.empty())
        stream.write (modelBytes.data(), modelBytes.size());

    const auto sourceUtf8 = sourceName.toRawUTF8();
    const auto sourceLength = static_cast<int> (std::strlen (sourceUtf8));
    stream.writeInt (sourceLength);
    if (sourceLength > 0)
        stream.write (sourceUtf8, static_cast<std::size_t> (sourceLength));
}

void NeuramarAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::scoped_lock operationLock (learningOperationMutex);
    if (data == nullptr || sizeInBytes <= 0)
        return;

    juce::MemoryInputStream stream (data, static_cast<std::size_t> (sizeInBytes), false);
    const auto magic = static_cast<std::uint32_t> (stream.readInt());
    if (magic != stateMagic)
    {
        // Early development builds used the sibling instruments' XML-only
        // state shape. Keep that harmless fallback for parameter migration.
        if (const auto xml = getXmlFromBinary (data, sizeInBytes);
            xml != nullptr && xml->hasTagName (parameters.state.getType()))
        {
            auto restoredState = juce::ValueTree::fromXml (*xml);
            addLegacyTouchIfMissing (restoredState);
            addDefaultParameterStateIfMissing (
                restoredState, parameters, neuramar::parameters::noise);
            addDefaultParameterStateIfMissing (
                restoredState, parameters, neuramar::parameters::stretch);
            addDefaultParameterStateIfMissing (
                restoredState, parameters, neuramar::parameters::formant);
            addDefaultParameterStateIfMissing (
                restoredState, parameters, neuramar::parameters::touch);
            parameters.replaceState (restoredState);
        }
        return;
    }

    const auto version = static_cast<std::uint32_t> (stream.readInt());
    if (version == 0 || version > stateVersion)
        return;

    const auto readBoundedBlock = [&stream] (int maximumBytes, std::vector<std::uint8_t>& output)
    {
        const auto length = stream.readInt();
        if (length < 0 || length > maximumBytes
            || static_cast<std::int64_t> (length) > stream.getNumBytesRemaining())
            return false;
        output.resize (static_cast<std::size_t> (length));
        return length == 0
            || stream.read (output.data(), static_cast<int> (output.size()))
                   == static_cast<int> (output.size());
    };

    std::vector<std::uint8_t> xmlBytes;
    if (! readBoundedBlock (1024 * 1024, xmlBytes))
        return;
    if (xmlBytes.empty())
        return;
    const auto xmlText = juce::String::fromUTF8 (
        reinterpret_cast<const char*> (xmlBytes.data()),
        static_cast<int> (xmlBytes.size()));
    const auto xml = juce::XmlDocument::parse (xmlText);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;
    auto restoredParameterState = juce::ValueTree::fromXml (*xml);
    if (! restoredParameterState.isValid())
        return;
    // Earlier sessions predate the appended Noise, Stretch, Formant, and
    // Touch controls. APVTS otherwise retains the live value for a missing
    // parameter child, which could make a legacy exact-recall preset
    // unexpectedly evolve, stretch, shift, or respond to velocity.
    addLegacyTouchIfMissing (restoredParameterState);
    addDefaultParameterStateIfMissing (
        restoredParameterState, parameters, neuramar::parameters::noise);
    addDefaultParameterStateIfMissing (
        restoredParameterState, parameters, neuramar::parameters::stretch);
    addDefaultParameterStateIfMissing (
        restoredParameterState, parameters, neuramar::parameters::formant);
    addDefaultParameterStateIfMissing (
        restoredParameterState, parameters, neuramar::parameters::touch);

    std::vector<std::uint8_t> modelBytes;
    if (! readBoundedBlock (maximumModelStateBytes, modelBytes))
        return;

    std::vector<std::uint8_t> sourceBytes;
    if (! readBoundedBlock (4096, sourceBytes))
        return;
    if (stream.getNumBytesRemaining() != 0)
        return;
    const auto sourceName = sourceBytes.empty()
        ? juce::String ("Restored memory")
        : juce::String::fromUTF8 (reinterpret_cast<const char*> (sourceBytes.data()),
                                  static_cast<int> (sourceBytes.size()));

    std::unique_ptr<neuramar::NeuralModel> restoredModel;
    if (! modelBytes.empty())
    {
        std::string error;
        restoredModel = neuramar::NeuralModel::deserialize (
            std::span<const std::uint8_t> (modelBytes.data(), modelBytes.size()), &error);
        if (restoredModel == nullptr)
        {
            // If the worker already claimed completion, request-only
            // cancellation cannot prevent its final Ready display write.
            // Settle that bounded worker before publishing the terminal error
            // so the invalid state is the last coherent operation observed.
            cancelLearning();
            setTerminalLearningStatus (
                LearningStage::Error,
                "Stored neural memory is invalid: "
                    + juce::String::fromUTF8 (error.c_str()));
            return;
        }
    }

    // Only commit after every bounded block, XML tree, and neural payload has
    // validated. A damaged state therefore cannot combine new parameters with
    // the previously playable model.
    cancelLearning();
    parameters.replaceState (restoredParameterState);
    if (restoredModel != nullptr)
        publishRestoredModel (std::move (restoredModel), sourceName);
    else
        clearPublishedModel ("This state does not contain a learned memory.");
}

juce::AudioProcessorEditor* NeuramarAudioProcessor::createEditor()
{
    return new NeuramarAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeuramarAudioProcessor();
}
