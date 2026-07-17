#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
int failureCount = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct ParameterExpectation
{
    const char* id;
    float defaultValue;
    float tolerance;
};

constexpr std::array<ParameterExpectation, 20> expectedParameters {{
    { electry::parameters::pickupSelector, 2.0f,  1.0e-5f },
    { electry::parameters::pickupType,     0.5f,  1.0e-5f },
    { electry::parameters::tone,           0.8f,  1.0e-5f },
    { electry::parameters::bodyWood,       0.5f,  1.0e-5f },
    { electry::parameters::bodySize,       0.5f,  1.0e-5f },
    { electry::parameters::bodyShape,      0.5f,  1.0e-5f },
    { electry::parameters::construction,   0.5f,  1.0e-5f },
    { electry::parameters::scaleLength,    0.5f,  1.0e-5f },
    { electry::parameters::bodyResonance,  0.35f, 1.0e-5f },
    { electry::parameters::stringGauge,    0.5f,  1.0e-5f },
    { electry::parameters::stringAge,      0.15f, 1.0e-5f },
    { electry::parameters::pickPosition,   0.35f, 1.0e-5f },
    { electry::parameters::pickHardness,   0.6f,  1.0e-5f },
    { electry::parameters::pickNoise,      0.5f,  1.0e-5f },
    { electry::parameters::fingerNoise,    0.4f,  1.0e-5f },
    { electry::parameters::releaseNoise,   0.4f,  1.0e-5f },
    { electry::parameters::muteDamping,    0.55f, 1.0e-5f },
    { electry::parameters::bendTime,       0.28f, 1.0e-4f },
    { electry::parameters::velocity,       0.65f, 1.0e-5f },
    { electry::parameters::output,        -6.0f,  1.0e-5f },
}};

float parameterValue (const ElectryAudioProcessor& processor, const char* id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, std::string ("missing parameter ") + id);
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (ElectryAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot set missing parameter ") + id);
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void expectParameterText (ElectryAudioProcessor& processor, const char* id,
                          float value, const char* expectedText)
{
    const auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot format missing parameter ") + id);
    if (parameter == nullptr)
        return;

    const auto actualText = parameter->getText (parameter->convertTo0to1 (value), 64);
    expect (actualText == expectedText,
            std::string ("wrong formatted text for ") + id + ": expected "
                + expectedText + ", got " + actualText.toStdString());
}

void renderBlock (ElectryAudioProcessor& processor, juce::AudioBuffer<float>& audio,
                  juce::MidiBuffer& midi, int numSamples = blockSize)
{
    audio.setSize (2, numSamples, false, false, true);
    audio.clear();
    processor.processBlock (audio, midi);
    midi.clear();
}

float renderSeconds (ElectryAudioProcessor& processor, juce::AudioBuffer<float>& audio,
                     double seconds)
{
    juce::MidiBuffer emptyMidi;
    float peak = 0.0f;
    int remaining = static_cast<int> (seconds * sampleRate);
    while (remaining > 0)
    {
        const int samples = std::min (blockSize, remaining);
        renderBlock (processor, audio, emptyMidi, samples);
        peak = std::max (peak, audio.getMagnitude (0, audio.getNumSamples()));
        remaining -= samples;
    }
    return peak;
}

void testParameterLayoutAndDefaults()
{
    ElectryAudioProcessor processor;
    expect (processor.getParameters().size()
                == static_cast<int> (expectedParameters.size()),
            "processor does not expose exactly 20 APVTS parameters");

    std::set<std::string> uniqueIds;
    for (const auto& expected : expectedParameters)
    {
        expect (uniqueIds.insert (expected.id).second,
                std::string ("duplicate APVTS id ") + expected.id);
        const auto value = parameterValue (processor, expected.id);
        expect (std::abs (value - expected.defaultValue) <= expected.tolerance,
                std::string ("wrong default for ") + expected.id + ": got "
                    + std::to_string (value));
    }
}

void testParameterTextFormatting()
{
    ElectryAudioProcessor processor;
    expectParameterText (processor, electry::parameters::scaleLength, 0.0f, "24.75\"");
    expectParameterText (processor, electry::parameters::scaleLength, 1.0f, "25.50\"");
    expectParameterText (processor, electry::parameters::output, -6.0f, "-6.0dB");
    expectParameterText (processor, electry::parameters::output, 3.0f, "+3.0dB");
    expectParameterText (processor, electry::parameters::tone, 0.8f, "80%");
    expectParameterText (processor, electry::parameters::bendTime, 0.28f, "280 ms");
    expectParameterText (processor, electry::parameters::pickupType, 0.0f, "Humbucker");
    expectParameterText (processor, electry::parameters::pickupType, 1.0f, "Single coil");
    expectParameterText (processor, electry::parameters::bodyWood, 0.0f,
                         "Mahogany/maple");

    const auto* selector = processor.parameters.getParameter (
        electry::parameters::pickupSelector);
    expect (selector != nullptr
                && selector->getText (selector->convertTo0to1 (2.0f), 64) == "Bridge",
            "pickup selector does not format its Bridge choice");
}

void testStateRoundTrip()
{
    ElectryAudioProcessor source;
    setParameterValue (source, electry::parameters::pickupType, 0.9f);
    setParameterValue (source, electry::parameters::bodyWood, 0.15f);
    setParameterValue (source, electry::parameters::scaleLength, 1.0f);
    setParameterValue (source, electry::parameters::output, -12.0f);
    setParameterValue (source, electry::parameters::pickupSelector, 0.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    expect (state.getSize() > 0, "state serialisation produced no data");

    ElectryAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    expect (std::abs (parameterValue (restored, electry::parameters::pickupType) - 0.9f)
                < 1.0e-4f,
            "pickupType did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::bodyWood) - 0.15f)
                < 1.0e-4f,
            "bodyWood did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::scaleLength) - 1.0f)
                < 1.0e-4f,
            "scaleLength did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::output) + 12.0f)
                < 1.0e-3f,
            "output level did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::pickupSelector))
                < 1.0e-4f,
            "pickup selector did not survive a state round trip");
}

void testBusAndPluginContract()
{
    ElectryAudioProcessor processor;
    expect (processor.acceptsMidi(), "instrument does not accept MIDI");
    expect (! processor.producesMidi(), "instrument unexpectedly produces MIDI");
    expect (! processor.isMidiEffect(), "instrument reports being a MIDI effect");
    expect (processor.hasEditor(), "instrument does not advertise its editor");
    expect (processor.getTailLengthSeconds()
                == ElectryAudioProcessor::maximumTailLengthSeconds,
            "tail length does not match the documented contract");

    juce::AudioProcessor::BusesLayout stereoOut;
    stereoOut.inputBuses.add (juce::AudioChannelSet::disabled());
    stereoOut.outputBuses.add (juce::AudioChannelSet::stereo());
    expect (processor.checkBusesLayoutSupported (stereoOut),
            "stereo-out layout is not supported");

    juce::AudioProcessor::BusesLayout monoOut;
    monoOut.inputBuses.add (juce::AudioChannelSet::disabled());
    monoOut.outputBuses.add (juce::AudioChannelSet::mono());
    expect (! processor.checkBusesLayoutSupported (monoOut),
            "mono-only layout should be rejected");
}

void testSampleAccurateNoteAndSound()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.isEngineReady(), "engine is not ready after prepareToPlay");

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    // A note placed mid-block must not sound before its sample position.
    const int onset = 256;
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), onset);
    renderBlock (processor, audio, midi);

    const auto before = audio.getMagnitude (0, onset - 8);
    const auto after = audio.getMagnitude (onset, blockSize - onset);
    expect (before == 0.0f, "audio appeared before the note-on sample position");
    expect (after > 1.0e-5f, "note-on did not produce audio in its block");
    expect (processor.getActiveVoiceCount() == 1,
            "note-on did not report one active string");

    processor.releaseResources();
}

void testKeyswitchContract()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    expect (processor.getCurrentArticulationIndex() == 0,
            "default play style is not Downstroke");

    // A keyswitch note alone changes the style and never sounds.
    midi.addEvent (juce::MidiMessage::noteOn (1, 27, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    expect (audio.getMagnitude (0, blockSize) == 0.0f,
            "keyswitch note produced audio");
    expect (processor.getActiveVoiceCount() == 0, "keyswitch note created a voice");
    expect (processor.getCurrentArticulationIndex() == 3,
            "keyswitch 27 did not latch the Muted style");

    // The latched style survives its own note-off and applies to played notes.
    midi.addEvent (juce::MidiMessage::noteOff (1, 27), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 96), 16);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentArticulationIndex() == 3,
            "keyswitch note-off cleared the latched style");
    expect (processor.getActiveVoiceCount() == 1,
            "played note after keyswitch did not sound");

    processor.releaseResources();
}

void testMidiControllersAndVoiceLifecycle()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    // Sustain pedal holds a released string.
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() == 1,
            "sustain pedal did not hold the released string");

    // Releasing the pedal lets the string damp out.
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "string did not retire after the sustain pedal released");

    // All Sound Off mutes immediately.
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 120, 0), 0);
    renderBlock (processor, audio, midi);
    const auto afterSoundOff = renderSeconds (processor, audio, 0.05);
    expect (afterSoundOff < 1.0e-4f, "All Sound Off left audible output");

    // All Notes Off releases held strings musically.
    midi.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "All Notes Off did not release the held string");

    processor.releaseResources();
}

void testUiArticulationTriggerAndPanic()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    // The editor's play-style buttons route through the UI queue.
    processor.triggerArticulation (8);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentArticulationIndex() == 8,
            "UI articulation trigger did not latch Slap");

    processor.triggerArticulation (99);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentArticulationIndex() == 8,
            "out-of-range articulation index was not ignored");

    // Panic silences a ringing string within one block.
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 110), 0);
    renderBlock (processor, audio, midi);
    processor.requestPanic();
    renderBlock (processor, audio, midi);
    const auto peak = renderSeconds (processor, audio, 0.05);
    expect (peak < 1.0e-4f, "panic left audible output");
    expect (processor.getActiveVoiceCount() == 0, "panic left active strings");

    processor.releaseResources();
}

void testOutputGainImpact()
{
    const auto renderPeak = [] (float outputDb)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::output, outputDb);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        renderBlock (processor, audio, midi);
        const auto peak = renderSeconds (processor, audio, 0.4);
        processor.releaseResources();
        return peak;
    };

    const auto quiet = renderPeak (-24.0f);
    const auto loud = renderPeak (0.0f);
    expect (loud > quiet * 4.0f,
            "output level does not scale the rendered signal");
}

juce::Image renderEditorSnapshot (juce::AudioProcessorEditor& editor)
{
    juce::Image snapshot (juce::Image::ARGB, editor.getWidth(),
                          editor.getHeight(), true);
    juce::Graphics graphics (snapshot);
    editor.paintEntireComponent (graphics, true);
    return snapshot;
}

void testEditorRendering()
{
    ElectryAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "processor did not create an editor");
    if (editor == nullptr)
        return;

    expect (editor->getWidth() >= 900 && editor->getHeight() >= 500,
            "editor opened at an unexpectedly small size");

    const auto snapshot = renderEditorSnapshot (*editor);
    int litPixels = 0;
    for (int y = 0; y < snapshot.getHeight(); y += 8)
        for (int x = 0; x < snapshot.getWidth(); x += 8)
            if (snapshot.getPixelAt (x, y).getPerceivedBrightness() > 0.05f)
                ++litPixels;
    expect (litPixels > 100, "editor snapshot appears blank");
}

void testPrepareReleaseCycles()
{
    ElectryAudioProcessor processor;
    for (const double rate : { 44100.0, 96000.0, 192000.0 })
    {
        processor.prepareToPlay (rate, blockSize);
        expect (processor.isEngineReady(),
                "engine not ready at rate " + std::to_string (rate));
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 90), 0);
        renderBlock (processor, audio, midi);
        expect (audio.getMagnitude (0, blockSize) > 0.0f,
                "no audio after prepare at rate " + std::to_string (rate));
        processor.releaseResources();
        expect (! processor.isEngineReady(),
                "engine still ready after releaseResources");
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    testParameterLayoutAndDefaults();
    testParameterTextFormatting();
    testStateRoundTrip();
    testBusAndPluginContract();
    testSampleAccurateNoteAndSound();
    testKeyswitchContract();
    testMidiControllersAndVoiceLifecycle();
    testUiArticulationTriggerAndPanic();
    testOutputGainImpact();
    testEditorRendering();
    testPrepareReleaseCycles();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Electry processor contract test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Electry processor contract tests passed\n";
    return EXIT_SUCCESS;
}
