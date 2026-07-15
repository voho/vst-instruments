#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
constexpr int eventOffset = 73;
int failureCount = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

double peakInRange (const juce::AudioBuffer<float>& buffer, int start, int end)
{
    start = juce::jlimit (0, buffer.getNumSamples(), start);
    end = juce::jlimit (start, buffer.getNumSamples(), end);
    double peak = 0.0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = start; sample < end; ++sample)
            peak = std::max (peak, std::abs (static_cast<double> (
                buffer.getSample (channel, sample))));

    return peak;
}

bool isFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;

    return true;
}

class ProcessorHarness
{
public:
    ProcessorHarness()
        : buffer (2, blockSize)
    {
        if (auto* room = processor.parameters.getParameter (vocalor::parameters::room))
            room->setValueNotifyingHost (1.0f);
        else
            expect (false, "room parameter was unavailable");

        processor.prepareToPlay (sampleRate, blockSize);
    }

    ~ProcessorHarness()
    {
        processor.releaseResources();
    }

    void process (juce::MidiBuffer midi = {})
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        expect (isFinite (buffer), "processor produced a NaN or infinity");
    }

    void chargeVoiceAndRoom()
    {
        juce::MidiBuffer noteOn;
        noteOn.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (114)), 0);
        process (std::move (noteOn));

        constexpr int chargeBlocks = static_cast<int> (sampleRate * 0.6 / blockSize);
        for (int block = 0; block < chargeBlocks; ++block)
            process();

        expect (processor.getActiveVoiceCount() > 0,
                "charged processor did not retain an active voice");
        expect (peakInRange (buffer, 0, blockSize) > 1.0e-8,
                "charged processor did not produce audible output");
    }

    VocalorAudioProcessor processor;
    juce::AudioBuffer<float> buffer;
};

void testPanicHardStopsAtBlockBoundary()
{
    ProcessorHarness harness;
    harness.chargeVoiceAndRoom();

    // A queued GUI note must not be able to undo Panic in the same block.
    harness.processor.keyboardState.noteOn (1, 67, 0.9f);
    harness.processor.requestPanic();
    harness.process();

    expect (harness.processor.getActiveVoiceCount() == 0,
            "requestPanic left processor voices active");
    expect (peakInRange (harness.buffer, 0, blockSize) == 0.0,
            "requestPanic left audible voice or room-tail samples");
}

void testMidiAllSoundOffIsSampleAccurate()
{
    ProcessorHarness harness;
    harness.chargeVoiceAndRoom();

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 120, 0), eventOffset);
    harness.process (std::move (midi));

    expect (peakInRange (harness.buffer, 0, eventOffset) > 1.0e-8,
            "CC120 test had no audio before its nonzero sample offset");
    expect (peakInRange (harness.buffer, eventOffset, blockSize) == 0.0,
            "CC120 left audible voice or room-tail samples after its sample offset");
    expect (harness.processor.getActiveVoiceCount() == 0,
            "CC120 left processor voices active");
}

void testMidiAllNotesOffKeepsRelease()
{
    ProcessorHarness harness;
    harness.chargeVoiceAndRoom();

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), eventOffset);
    harness.process (std::move (midi));

    expect (peakInRange (harness.buffer, 0, eventOffset) > 1.0e-8,
            "CC123 test had no audio before its nonzero sample offset");
    expect (peakInRange (harness.buffer, eventOffset, blockSize) > 1.0e-8,
            "CC123 incorrectly hard-stopped the normal release path");
    expect (harness.processor.getActiveVoiceCount() > 0,
            "CC123 incorrectly removed releasing voices immediately");
}
} // namespace

int main()
{
    testPanicHardStopsAtBlockBoundary();
    testMidiAllSoundOffIsSampleAccurate();
    testMidiAllNotesOffKeepsRelease();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Vocalor processor check(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Vocalor processor checks passed.\n";
    return EXIT_SUCCESS;
}
