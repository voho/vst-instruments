#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

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

constexpr std::array<ParameterExpectation, 22> expectedParameters {{
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
    { electry::parameters::artifacts,      0.18f, 1.0e-5f },
    { electry::parameters::outputMode,     0.0f,  1.0e-5f },
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
            "processor does not expose exactly 22 APVTS parameters");

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
    expectParameterText (processor, electry::parameters::scaleLength, 0.0f, "25.50\"");
    expectParameterText (processor, electry::parameters::scaleLength, 1.0f, "28.00\"");
    expectParameterText (processor, electry::parameters::output, -6.0f, "-6.0dB");
    expectParameterText (processor, electry::parameters::output, 3.0f, "+3.0dB");
    expectParameterText (processor, electry::parameters::tone, 0.8f, "80%");
    expectParameterText (processor, electry::parameters::artifacts, 0.18f, "18%");
    expectParameterText (processor, electry::parameters::outputMode, 0.0f, "Mono");
    expectParameterText (processor, electry::parameters::outputMode, 1.0f, "Stereo");
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
    setParameterValue (source, electry::parameters::artifacts, 0.72f);
    setParameterValue (source, electry::parameters::outputMode, 1.0f);
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
    expect (std::abs (parameterValue (restored, electry::parameters::artifacts) - 0.72f)
                < 1.0e-4f,
            "artifacts did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::outputMode) - 1.0f)
                < 1.0e-4f,
            "output mode did not survive a state round trip");
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
    expect (std::abs (processor.getTailLengthSeconds()
                          - ElectryAudioProcessor::maximumTailLengthSeconds) < 1.0e-9,
            "tail length does not match the documented contract");

    expect (processor.getBusCount (true) == 0,
            "instrument unexpectedly exposes an input bus");
    expect (processor.getBusCount (false) == 1,
            "instrument does not expose exactly one output bus");
    expect (processor.getTotalNumOutputChannels() == 2,
            "instrument main output is not stereo");

    // Test the actual processor topology rather than a hand-built layout, so
    // the bus count matches what checkBusesLayoutSupported would validate.
    const auto stereoLayout = processor.getBusesLayout();
    expect (processor.isBusesLayoutSupported (stereoLayout),
            "default no-input/stereo-output layout is unsupported");

    auto monoLayout = stereoLayout;
    monoLayout.outputBuses.set (0, juce::AudioChannelSet::mono());
    expect (! processor.isBusesLayoutSupported (monoLayout),
            "mono instrument output should be rejected");

    auto inputLayout = stereoLayout;
    inputLayout.inputBuses.add (juce::AudioChannelSet::stereo());
    expect (! processor.isBusesLayoutSupported (inputLayout),
            "an added instrument input bus should be rejected");
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
    // getMagnitude is non-negative, so <= 0 is exact silence.
    expect (before <= 0.0f, "audio appeared before the note-on sample position");
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

    const auto mutedIndex = static_cast<int> (electry::Articulation::Muted);
    const auto mutedNote = electry::ElectryEngine::firstKeyswitchNote + mutedIndex;

    // A keyswitch note alone changes the style and never sounds.
    midi.addEvent (juce::MidiMessage::noteOn (1, mutedNote, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    expect (audio.getMagnitude (0, blockSize) <= 0.0f,
            "keyswitch note produced audio");
    expect (processor.getActiveVoiceCount() == 0, "keyswitch note created a voice");
    expect (processor.getCurrentArticulationIndex() == mutedIndex,
            "palm-mute keyswitch did not latch the Muted style");

    // The latched style survives its own note-off and applies to played notes.
    midi.addEvent (juce::MidiMessage::noteOff (1, mutedNote), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::lowestPlayableNote, (juce::uint8) 96), 16);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentArticulationIndex() == mutedIndex,
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
    const auto slapIndex = static_cast<int> (electry::Articulation::Slap);
    processor.triggerArticulation (slapIndex);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentArticulationIndex() == slapIndex,
            "UI articulation trigger did not latch Slap");

    processor.triggerArticulation (99);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentArticulationIndex() == slapIndex,
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
        // Let the output-gain smoother settle before the note so the pluck is
        // measured at the requested level, not part-way through the
        // anti-zipper ramp from the default level.
        renderSeconds (processor, audio, 0.1);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        renderBlock (processor, audio, midi);
        const auto peak = renderSeconds (processor, audio, 0.4);
        processor.releaseResources();
        return peak;
    };

    const auto quiet = renderPeak (-24.0f);
    const auto loud = renderPeak (0.0f);
    // -24 dB to 0 dB is a 15.85x change; require most of it to survive the
    // pluck and output guard.
    expect (loud > quiet * 8.0f,
            "output level does not scale the rendered signal (quiet "
                + std::to_string (quiet) + ", loud " + std::to_string (loud) + ")");
}

void testOutputModeAudioField()
{
    struct ChannelResult
    {
        double leftRms = 0.0;
        double rightRms = 0.0;
        bool identical = true;
    };

    const auto render = [] (float mode, int midiNote)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::outputMode, mode);

        juce::AudioBuffer<float> audio;
        renderSeconds (processor, audio, 0.05); // settle the mode crossfade

        double leftEnergy = 0.0;
        double rightEnergy = 0.0;
        std::uint64_t sampleCount = 0;
        bool identical = true;
        for (int block = 0; block < 48; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (
                    1, midiNote, (juce::uint8) 102), 0);
            renderBlock (processor, audio, midi);
            if (block < 2)
                continue;
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const double left = audio.getSample (0, sample);
                const double right = audio.getSample (1, sample);
                const float leftSample = audio.getSample (0, sample);
                const float rightSample = audio.getSample (1, sample);
                leftEnergy += left * left;
                rightEnergy += right * right;
                identical = identical
                    && std::memcmp (&leftSample, &rightSample,
                                    sizeof (leftSample)) == 0;
                ++sampleCount;
            }
        }
        processor.releaseResources();
        const double divisor = static_cast<double> (std::max<std::uint64_t> (
            sampleCount, 1));
        return ChannelResult { std::sqrt (leftEnergy / divisor),
                               std::sqrt (rightEnergy / divisor), identical };
    };

    const auto mono = render (0.0f, electry::ElectryEngine::lowestPlayableNote);
    expect (mono.identical, "Mono output parameter did not produce exact dual mono");

    const auto stereoLow = render (
        1.0f, electry::ElectryEngine::lowestPlayableNote);
    expect (stereoLow.leftRms > stereoLow.rightRms * 1.08,
            "Stereo APVTS parameter did not spread the low string left");

    const auto stereoHigh = render (1.0f, 64);
    expect (stereoHigh.rightRms > stereoHigh.leftRms * 1.08,
            "Stereo APVTS parameter did not spread the high string right");
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
    processor.prepareToPlay (sampleRate, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "processor did not create an editor");
    if (editor == nullptr)
        return;

    expect (editor->getWidth() >= 900 && editor->getHeight() >= 500,
            "editor opened at an unexpectedly small size");

    std::vector<ElectryKnob*> knobs;
    for (auto* child : editor->getChildren())
    {
        if (auto* knob = dynamic_cast<ElectryKnob*> (child))
            knobs.push_back (knob);
        if (child->isVisible())
            expect (editor->getLocalBounds().contains (child->getBounds()),
                    "visible editor control escaped the editor bounds: "
                        + child->getName().toStdString());
    }
    expect (knobs.size() == 20u, "editor did not expose all 20 knob controls");

    for (std::size_t first = 0; first < knobs.size(); ++first)
    {
        expect (knobs[first]->getWidth() >= 68 && knobs[first]->getHeight() >= 110,
                "knob fell below the compact control size: "
                    + knobs[first]->getName().toStdString());
        for (auto* child : knobs[first]->getChildren())
            expect (knobs[first]->getLocalBounds().contains (child->getBounds()),
                    "knob child escaped its control bounds: "
                        + knobs[first]->getName().toStdString());

        for (std::size_t second = first + 1u; second < knobs.size(); ++second)
            expect (! knobs[first]->getBounds().intersects (knobs[second]->getBounds()),
                    "knob regions overlap: " + knobs[first]->getName().toStdString()
                        + " and " + knobs[second]->getName().toStdString());
    }

    const auto findControl = [&] (const char* componentId) -> juce::Component*
    {
        for (auto* child : editor->getChildren())
            if (child->getComponentID() == componentId)
                return child;
        return nullptr;
    };
    const auto effectiveDialSize = [&] (const char* componentId)
    {
        const auto* control = findControl (componentId);
        expect (control != nullptr,
                std::string ("missing editor component ID ") + componentId);
        return control == nullptr
            ? 0
            : juce::jmin (control->getWidth(), control->getHeight() - 30);
    };

    const auto* outputModeControl = findControl (electry::parameters::outputMode);
    const auto* outputControl = findControl (electry::parameters::output);
    expect (outputModeControl != nullptr && outputControl != nullptr,
            "Master panel is missing its Mono/Stereo field or Output control");
    if (outputModeControl != nullptr && outputControl != nullptr)
    {
        expect (! outputModeControl->getBounds().intersects (
                    outputControl->getBounds()),
                "Mono/Stereo field overlaps the Master Output knob");
        int modeButtons = 0;
        std::vector<juce::Rectangle<int>> modeButtonBounds;
        for (auto* child : outputModeControl->getChildren())
        {
            if (dynamic_cast<juce::TextButton*> (child) == nullptr)
                continue;
            ++modeButtons;
            modeButtonBounds.push_back (child->getBounds());
            expect (child->getWidth() >= 30 && child->getHeight() >= 22,
                    "Mono/Stereo button is too small to operate clearly");
        }
        expect (modeButtons == 2,
                "Output field did not expose exactly Mono and Stereo");
        if (modeButtonBounds.size() == 2u)
            expect (! modeButtonBounds[0].intersects (modeButtonBounds[1]),
                    "Mono and Stereo buttons overlap");
    }

    const std::array<const char*, 7> heroControls {
        electry::parameters::pickupType, electry::parameters::tone,
        electry::parameters::pickPosition, electry::parameters::pickHardness,
        electry::parameters::stringAge, electry::parameters::bodyResonance,
        electry::parameters::velocity
    };
    const std::array<const char*, 5> detailControls {
        electry::parameters::bendTime, electry::parameters::pickNoise,
        electry::parameters::fingerNoise, electry::parameters::releaseNoise,
        electry::parameters::artifacts
    };
    for (const auto* hero : heroControls)
        for (const auto* detail : detailControls)
            expect (effectiveDialSize (hero) * 100
                        >= effectiveDialSize (detail) * 135,
                    std::string (hero) + " is not visibly larger than " + detail);

    expect (effectiveDialSize (electry::parameters::bodyWood) * 100
                >= effectiveDialSize (electry::parameters::artifacts) * 120,
            "material controls are not visually above texture details");
    expect (effectiveDialSize (electry::parameters::muteDamping) * 100
                >= effectiveDialSize (electry::parameters::artifacts) * 110,
            "contextual Mute control is not visually above texture details");
    expect (effectiveDialSize (electry::parameters::output) * 100
                >= effectiveDialSize (electry::parameters::releaseNoise) * 110,
            "Master Output is not visually above release-noise detail");

    const auto* articulation = findControl ("articulationStrip");
    const auto* keyboard = findControl ("keyboard");
    const auto* keyboardHint = findControl ("keyboardHint");
    expect (articulation != nullptr && keyboard != nullptr && keyboardHint != nullptr,
            "editor hierarchy components are missing stable IDs");
    if (articulation != nullptr && keyboard != nullptr && keyboardHint != nullptr)
    {
        for (const auto* knob : knobs)
        {
            expect (articulation->getBottom() <= knob->getY(),
                    "sound control overlaps the articulation selector");
            expect (knob->getBottom() <= keyboard->getY(),
                    "sound control overlaps the keyboard");
        }
        expect (keyboard->getHeight() >= 90
                    && keyboard->getWidth() * 10 >= editor->getWidth() * 9,
                "keyboard lost its practical playing area");
        expect (keyboard->getBottom() <= keyboardHint->getY(),
                "keyboard hint is not below the keyboard");

        int articulationButtons = 0;
        std::vector<juce::Rectangle<int>> buttonBounds;
        for (auto* child : articulation->getChildren())
        {
            if (dynamic_cast<juce::TextButton*> (child) == nullptr)
                continue;
            ++articulationButtons;
            buttonBounds.push_back (child->getBounds());
            expect (child->getWidth() >= 60 && child->getHeight() >= 24,
                    "articulation button fell below its practical target size");
        }
        expect (articulationButtons == 16,
                "editor did not retain all 16 articulation buttons");
        for (std::size_t first = 0; first < buttonBounds.size(); ++first)
            for (std::size_t second = first + 1u; second < buttonBounds.size(); ++second)
                expect (! buttonBounds[first].intersects (buttonBounds[second]),
                        "articulation buttons overlap");
    }

    const auto snapshot = renderEditorSnapshot (*editor);
    int litPixels = 0;
    for (int y = 0; y < snapshot.getHeight(); y += 8)
        for (int x = 0; x < snapshot.getWidth(); x += 8)
            if (snapshot.getPixelAt (x, y).getPerceivedBrightness() > 0.05f)
                ++litPixels;
    expect (litPixels > 100, "editor snapshot appears blank");

    // The committed interface screenshot is this exact editor render. Setting
    // ELECTRY_EDITOR_SNAPSHOT to a path writes the PNG used in the READMEs, so
    // the documentation image is always the real regression-tested editor.
    const auto snapshotPath = juce::SystemStats::getEnvironmentVariable (
        "ELECTRY_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
    {
        juce::FileOutputStream output { juce::File (snapshotPath) };
        juce::PNGImageFormat png;
        const bool preparedOutput = output.openedOk()
            && output.setPosition (0)
            && output.truncate();
        const bool wroteSnapshot = preparedOutput
            && png.writeImageToStream (snapshot, output);
        output.flush();
        expect (wroteSnapshot, "could not write requested editor snapshot");
    }

    editor.reset();
    processor.releaseResources();
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
    testOutputModeAudioField();
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
