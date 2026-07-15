#include "PluginProcessor.h"

#include <JuceHeader.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
int failures = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool approximatelyEqual (float actual, float expected, float tolerance = 1.0e-5f)
{
    return std::abs (actual - expected) <= tolerance;
}

float parameterValue (const NeuramarAudioProcessor& processor, const char* id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, std::string ("missing parameter ") + id);
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (NeuramarAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot set missing parameter ") + id);
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

bool corruptSerializedModel (juce::MemoryBlock& state)
{
    juce::MemoryInputStream stream (state.getData(), state.getSize(), false);
    constexpr auto expectedMagic = static_cast<std::uint32_t> (0x5453524eu);
    if (static_cast<std::uint32_t> (stream.readInt()) != expectedMagic
        || stream.readInt() != 1)
        return false;

    const auto xmlLength = stream.readInt();
    const auto xmlEnd = stream.getPosition() + static_cast<juce::int64> (xmlLength);
    if (xmlLength <= 0 || xmlEnd > stream.getTotalLength()
        || ! stream.setPosition (xmlEnd))
        return false;

    const auto modelLength = stream.readInt();
    const auto modelOffset = stream.getPosition();
    if (modelLength <= 0
        || static_cast<juce::int64> (modelLength) > stream.getNumBytesRemaining())
        return false;

    // The neural payload owns its checksum. Flipping its final checksum byte
    // keeps the outer processor-state framing valid while guaranteeing that
    // NeuralModel::deserialize rejects the embedded memory.
    auto* bytes = static_cast<std::uint8_t*> (state.getData());
    bytes[static_cast<std::size_t> (modelOffset + modelLength - 1)] ^= 0x80u;
    return true;
}

std::vector<float> makeLearningTone (double sampleRate, double seconds,
                                     double fundamentalHz)
{
    const auto count = static_cast<std::size_t> (sampleRate * seconds);
    std::vector<float> result (count, 0.0f);
    double phase = 0.0;
    std::uint32_t noise = 0x12345678u;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto time = static_cast<double> (i) / sampleRate;
        const auto attack = std::min (1.0, time / 0.018);
        const auto evolution = 0.72 + 0.28 * std::sin (time * 1.7);
        const auto envelope = attack * std::exp (-time * 0.28);
        phase += fundamentalHz / sampleRate;
        phase -= std::floor (phase);
        const auto angle = juce::MathConstants<double>::twoPi * phase;
        noise = noise * 1664525u + 1013904223u;
        const auto air = (static_cast<float> ((noise >> 9u) & 0x7fffffu)
                          / static_cast<float> (0x7fffffu) - 0.5f)
                       * static_cast<float> (std::exp (-time * 9.0));
        const auto tonal = 0.62 * std::sin (angle)
                         + 0.24 * evolution * std::sin (2.0 * angle + 0.3)
                         + 0.10 * std::sin (3.0 * angle - 0.5);
        result[i] = static_cast<float> (envelope * (tonal + 0.035 * air));
    }
    return result;
}

bool waitForLearning (NeuramarAudioProcessor& processor,
                      std::chrono::seconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto snapshot = processor.getLearningSnapshot();
        if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Ready)
            return true;
        if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Error
            || snapshot.stage == NeuramarAudioProcessor::LearningStage::Cancelled)
            return false;
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
    return false;
}

struct AudioMetrics
{
    double energy = 0.0;
    float peak = 0.0f;
    bool finite = true;
};

AudioMetrics measure (const juce::AudioBuffer<float>& buffer)
{
    AudioMetrics result;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto value = buffer.getSample (channel, i);
            result.finite = result.finite && std::isfinite (value);
            if (std::isfinite (value))
            {
                result.energy += static_cast<double> (value) * value;
                result.peak = std::max (result.peak, std::abs (value));
            }
        }
    return result;
}

AudioMetrics renderNote (NeuramarAudioProcessor& processor, int midiNote,
                         int blockCount)
{
    juce::AudioBuffer<float> buffer (2, 128);
    AudioMetrics result;
    for (int block = 0; block < blockCount; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, 0.82f), 0);
        processor.processBlock (buffer, midi);
        const auto blockMetrics = measure (buffer);
        result.energy += blockMetrics.energy;
        result.peak = std::max (result.peak, blockMetrics.peak);
        result.finite = result.finite && blockMetrics.finite;
    }
    return result;
}

void testContractsAndLearning()
{
    NeuramarAudioProcessor processor;
    expect (processor.getName() == "Neuramar", "processor name is not Neuramar");
    expect (processor.acceptsMidi() && ! processor.producesMidi(),
            "instrument MIDI contract is wrong");
    expect (! processor.isMidiEffect(), "instrument reports itself as a MIDI effect");
    expect (processor.getTailLengthSeconds() >= 8.0,
            "tail contract does not cover maximum release");
    expect (processor.getNumPrograms() == 1
                && processor.getProgramName (0).isNotEmpty(),
            "the VST3 factory program has no host-visible name");
    expect (processor.getParameters().size() == 13,
            "version-one host parameter count changed");

    const auto stereo = processor.getBusesLayout();
    expect (processor.isBusesLayoutSupported (stereo),
            "stereo instrument layout was rejected");

    processor.prepareToPlay (48000.0, 128);
    juce::AudioBuffer<float> silent (2, 128);
    juce::MidiBuffer noteBeforeModel;
    noteBeforeModel.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    processor.processBlock (silent, noteBeforeModel);
    expect (measure (silent).energy == 0.0,
            "processor rendered audio before a model was learned");

    constexpr auto analysisRate = 16000.0;
    processor.learnSampleData (makeLearningTone (analysisRate, 0.85, 261.625565),
                               analysisRate, "procedural C4");
    expect (waitForLearning (processor, std::chrono::seconds (25)),
            "background sample learning did not complete successfully");

    const auto learned = processor.getLearningSnapshot();
    expect (learned.stage == NeuramarAudioProcessor::LearningStage::Ready,
            "completed learner did not publish a ready model");
    expect (std::abs (learned.rootMidiNote - 60) <= 1,
            "root detector did not identify procedural C4");
    expect (learned.rootConfidence > 0.20f,
            "root detector reported implausibly low confidence for a harmonic tone");
    expect (learned.modelGeneration > 0,
            "ready model did not receive a generation");

    const auto rendered = renderNote (processor, 67, 80);
    expect (rendered.finite, "learned instrument rendered NaN or infinity");
    expect (rendered.energy > 1.0e-8, "learned instrument was silent");
    expect (rendered.peak < 8.0f, "learned instrument exceeded amplitude guardrail");
    expect (processor.getActiveVoiceCount() > 0,
            "engine did not report an active learned voice");

    juce::AudioBuffer<float> releaseBuffer (2, 128);
    juce::MidiBuffer releaseMidi;
    releaseMidi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
    processor.processBlock (releaseBuffer, releaseMidi);

    constexpr auto retainedImprint = 0.31f;
    constexpr auto rejectedImprint = 0.76f;
    setParameterValue (processor, neuramar::parameters::imprint, retainedImprint);
    expect (approximatelyEqual (
                parameterValue (processor, neuramar::parameters::imprint), retainedImprint),
            "could not establish the retained parameter-state fixture");

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    expect (state.getSize() > 512 && state.getSize() < 4 * 1024 * 1024,
            "state did not embed one compact, bounded neural model");

    setParameterValue (processor, neuramar::parameters::imprint, rejectedImprint);
    juce::MemoryBlock stateWithRejectedParameters;
    processor.getStateInformation (stateWithRejectedParameters);
    expect (stateWithRejectedParameters != state,
            "corrupt-state fixture did not contain different parameters");
    setParameterValue (processor, neuramar::parameters::imprint, retainedImprint);

    auto corruptedModelState = stateWithRejectedParameters;
    expect (corruptSerializedModel (corruptedModelState),
            "could not locate the serialized neural payload to corrupt");
    const auto beforeCorruptedRestore = processor.getLearningSnapshot();
    processor.setStateInformation (
        corruptedModelState.getData(), static_cast<int> (corruptedModelState.getSize()));
    const auto afterCorruptedRestore = processor.getLearningSnapshot();
    expect (afterCorruptedRestore.stage == NeuramarAudioProcessor::LearningStage::Error
                && afterCorruptedRestore.message.containsIgnoreCase ("invalid"),
            "corrupted neural payload was not explicitly rejected");
    expect (approximatelyEqual (
                parameterValue (processor, neuramar::parameters::imprint), retainedImprint),
            "corrupted neural payload partially committed its parameter state");
    expect (afterCorruptedRestore.modelGeneration == beforeCorruptedRestore.modelGeneration
                && afterCorruptedRestore.rootMidiNote == beforeCorruptedRestore.rootMidiNote
                && afterCorruptedRestore.sourceName == beforeCorruptedRestore.sourceName
                && afterCorruptedRestore.waveform == beforeCorruptedRestore.waveform,
            "corrupted neural payload changed the prior published memory");

    juce::MemoryBlock stateAfterRejectedRestore;
    processor.getStateInformation (stateAfterRejectedRestore);
    expect (stateAfterRejectedRestore == state,
            "corrupted neural payload changed the processor's serializable state");

    processor.requestPanic();
    juce::AudioBuffer<float> panicBuffer (2, 128);
    juce::MidiBuffer noMidi;
    processor.processBlock (panicBuffer, noMidi);
    const auto retainedRender = renderNote (processor, 55, 50);
    expect (retainedRender.finite && retainedRender.energy > 1.0e-8,
            "corrupted neural payload made the prior model unplayable");
    processor.requestPanic();
    processor.processBlock (panicBuffer, noMidi);

    processor.learnSampleData (makeLearningTone (analysisRate, 2.0, 391.995436),
                               analysisRate, "pending G4");
    const auto pending = processor.getLearningSnapshot();
    expect (pending.sourceName == "pending G4"
                && pending.modelGeneration == learned.modelGeneration,
            "second learning fixture did not retain the prior published generation");
    juce::MemoryBlock stateDuringSecondLearning;
    processor.getStateInformation (stateDuringSecondLearning);
    const auto cancelStart = std::chrono::steady_clock::now();
    processor.requestLearningCancellation();
    const auto cancelRequestSeconds = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - cancelStart).count();
    expect (cancelRequestSeconds < 0.25,
            "UI cancellation request blocked while joining the learner");
    const auto cancelledSnapshot = processor.getLearningSnapshot();
    expect (cancelledSnapshot.stage
                == NeuramarAudioProcessor::LearningStage::Cancelled
                && cancelledSnapshot.modelGeneration == learned.modelGeneration
                && cancelledSnapshot.rootMidiNote == learned.rootMidiNote
                && cancelledSnapshot.sourceName == learned.sourceName
                && cancelledSnapshot.waveform == learned.waveform,
            "cancellation did not restore the prior published model metadata");
    // Lifecycle/state callers can still request a definite worker join after
    // the non-blocking UI operation has closed the publication attempt.
    processor.cancelLearning();
    expect (stateDuringSecondLearning == state,
            "state saved during learning mixed the pending source with the prior model");

    // A rejected restore is a terminal state operation: once it returns, an
    // in-flight learner must not be able to publish Ready over the error. This
    // also exercises the path where the worker has started but has not yet
    // reached a cancellation checkpoint.
    processor.learnSampleData (makeLearningTone (analysisRate, 2.0, 349.228231),
                               analysisRate, "racing F4");
    expect (processor.getLearningSnapshot().sourceName == "racing F4",
            "invalid-state race fixture did not start its learner");
    processor.setStateInformation (
        corruptedModelState.getData(), static_cast<int> (corruptedModelState.getSize()));
    const auto rejectedWhileLearning = processor.getLearningSnapshot();
    expect (rejectedWhileLearning.stage
                == NeuramarAudioProcessor::LearningStage::Error
                && rejectedWhileLearning.message.containsIgnoreCase ("invalid"),
            "active learner overwrote the rejected-state error");
    expect (rejectedWhileLearning.modelGeneration == learned.modelGeneration
                && rejectedWhileLearning.rootMidiNote == learned.rootMidiNote
                && rejectedWhileLearning.sourceName == learned.sourceName
                && rejectedWhileLearning.waveform == learned.waveform,
            "rejected state during learning changed the published memory");
    expect (approximatelyEqual (
                parameterValue (processor, neuramar::parameters::imprint), retainedImprint),
            "rejected state during learning partially committed parameters");
    std::this_thread::sleep_for (std::chrono::milliseconds (40));
    expect (processor.getLearningSnapshot().stage
                == NeuramarAudioProcessor::LearningStage::Error,
            "learner published after rejected-state restoration returned");

    NeuramarAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    const auto restoredSnapshot = restored.getLearningSnapshot();
    expect (restoredSnapshot.stage == NeuramarAudioProcessor::LearningStage::Ready,
            "state round trip did not restore a playable model");
    expect (restoredSnapshot.rootMidiNote == learned.rootMidiNote,
            "state round trip changed detected root");
    expect (restoredSnapshot.sourceName == learned.sourceName
                && restoredSnapshot.waveform == learned.waveform,
            "state round trip broke saved source/model coherence");
    expect (approximatelyEqual (
                parameterValue (restored, neuramar::parameters::imprint), retainedImprint),
            "state round trip changed the saved parameter fixture");
    restored.prepareToPlay (48000.0, 128);
    const auto restoredRender = renderNote (restored, 55, 50);
    expect (restoredRender.finite && restoredRender.energy > 1.0e-8,
            "restored neural memory did not render valid audio");

    const std::array<std::uint8_t, 13> malformed {
        0x4e, 0x52, 0x53, 0x54, 1, 0, 0, 0, 0xff, 0xff, 0xff, 0x7f, 0
    };
    restored.setStateInformation (malformed.data(), static_cast<int> (malformed.size()));
    expect (restored.getLearningSnapshot().stage
                == NeuramarAudioProcessor::LearningStage::Ready,
            "malformed state destroyed the current learned memory");

    NeuramarAudioProcessor emptyStateProcessor;
    juce::MemoryBlock emptyState;
    emptyStateProcessor.getStateInformation (emptyState);
    restored.setStateInformation (emptyState.getData(),
                                  static_cast<int> (emptyState.getSize()));
    expect (restored.getLearningSnapshot().stage
                == NeuramarAudioProcessor::LearningStage::Empty,
            "a valid empty state did not clear the learned memory");
    juce::AudioBuffer<float> clearedAudio (2, 128);
    juce::MidiBuffer clearedMidi;
    clearedMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    restored.processBlock (clearedAudio, clearedMidi);
    expect (measure (clearedAudio).energy == 0.0,
            "processor rendered the previous model after restoring an empty state");

    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (processor.getLearningSnapshot().stage
                == NeuramarAudioProcessor::LearningStage::Ready,
            "could not restore a ready model for the editor render check");

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "processor did not create its custom editor");
    if (editor != nullptr)
    {
        expect (editor->getWidth() == 1180 && editor->getHeight() == 760,
                "editor default size changed unexpectedly");
        juce::Image snapshot (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        juce::Graphics graphics (snapshot);
        editor->paintEntireComponent (graphics, true);

        std::set<juce::uint32> sampledColours;
        auto opaque = true;
        for (int y = 4; y < snapshot.getHeight(); y += 11)
            for (int x = 4; x < snapshot.getWidth(); x += 11)
            {
                const auto pixel = snapshot.getPixelAt (x, y);
                sampledColours.insert (pixel.getARGB());
                opaque = opaque && pixel.getAlpha() == 255;
            }
        expect (opaque, "editor snapshot contains transparent holes");
        expect (sampledColours.size() > 80,
                "editor snapshot lacks the neural-pool visual detail");

        const auto snapshotPath = juce::SystemStats::getEnvironmentVariable (
            "NEURAMAR_EDITOR_SNAPSHOT", {});
        if (snapshotPath.isNotEmpty())
        {
            juce::FileOutputStream output { juce::File (snapshotPath) };
            juce::PNGImageFormat png;
            const auto prepared = output.openedOk()
                               && output.setPosition (0)
                               && output.truncate();
            const auto written = prepared && png.writeImageToStream (snapshot, output);
            output.flush();
            expect (written, "could not write requested editor snapshot");
        }
    }

    processor.releaseResources();
    restored.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    testContractsAndLearning();

    if (failures != 0)
    {
        std::cerr << failures << " Neuramar processor check(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Neuramar processor checks passed.\n";
    return EXIT_SUCCESS;
}
