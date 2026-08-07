#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

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

juce::Image renderEditorSnapshot (juce::AudioProcessorEditor& editor)
{
    juce::Image snapshot (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
    juce::Graphics graphics (snapshot);
    editor.paintEntireComponent (graphics, true);
    return snapshot;
}

// A panel that rendered as one flat colour would still be "valid"; what we
// actually want to know is that it drew something.
bool snapshotHasDetail (const juce::Image& snapshot)
{
    if (! snapshot.isValid())
        return false;

    std::set<juce::uint32> distinct;
    for (int y = 0; y < snapshot.getHeight(); y += 4)
        for (int x = 0; x < snapshot.getWidth(); x += 4)
        {
            const auto pixel = snapshot.getPixelAt (x, y);
            if (pixel.getAlpha() < 250)
                return false;
            distinct.insert (pixel.getARGB());
            if (distinct.size() > 64)
                return true;
        }
    return distinct.size() > 8;
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

float denormalisedDefault (const juce::AudioProcessorValueTreeState& state, const char* id)
{
    if (auto* parameter = state.getParameter (id))
        return parameter->convertFrom0to1 (parameter->getDefaultValue());

    return std::numeric_limits<float>::quiet_NaN();
}

bool nearly (float value, float target, float tolerance = 0.0005f)
{
    return std::abs (value - target) <= tolerance;
}

void setNormalised (juce::AudioProcessorValueTreeState& state, const char* id, float normalised)
{
    if (auto* parameter = state.getParameter (id))
        parameter->setValueNotifyingHost (normalised);
    else
        expect (false, std::string ("parameter ") + id + " was unavailable");
}

void setDenormalised (juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
    if (auto* parameter = state.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    else
        expect (false, std::string ("parameter ") + id + " was unavailable");
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

void testParameterLayoutIsStable()
{
    VocalorAudioProcessor processor;
    auto& state = processor.parameters;
    using namespace vocalor::parameters;

    expect (processor.getParameters().size() == 21,
            "the published parameter count is no longer 21");

    // Version-1 identifiers and defaults are part of the saved session format.
    // Changing any of them would silently alter existing projects.
    struct Expected { const char* id; float value; };
    const Expected versionOne[] = {
        { profile, 0.0f }, { mode, 0.0f }, { vowel, 0.0f }, { chordQuality, 0.0f },
        { choirSize, 8.0f }, { breath, 0.30f }, { resonance, 0.64f }, { vibrato, 0.38f },
        { humanize, 0.52f }, { spread, 0.62f }, { tension, 0.36f }, { room, 0.24f },
        { output, -6.0f }
    };
    for (const auto& entry : versionOne)
        expect (nearly (denormalisedDefault (state, entry.id), entry.value, 0.002f),
                std::string ("version-1 default for ") + entry.id + " changed");

    // Version-1.1 additions all default to the neutral setting, so a recalled
    // session sounds exactly as it did before they existed.
    const Expected versionOnePointOne[] = {
        { legato, 0.0f }, { vowelX, 0.50f }, { vowelY, 0.50f }, { vowelMorph, 0.0f },
        { formantShift, 0.0f }, { glide, 0.0f }, { roomSize, 0.50f }
    };
    for (const auto& entry : versionOnePointOne)
        expect (nearly (denormalisedDefault (state, entry.id), entry.value, 0.002f),
                std::string ("default for the new parameter ") + entry.id + " is not neutral");

    // Version-1.2 additions carry the same obligation.
    const Expected versionOnePointTwo[] = { { dynamics, 1.0f } };
    for (const auto& entry : versionOnePointTwo)
        expect (nearly (denormalisedDefault (state, entry.id), entry.value, 0.002f),
                std::string ("default for the new parameter ") + entry.id + " is not neutral");

    if (auto* shift = state.getParameter (formantShift))
    {
        const auto range = shift->getNormalisableRange();
        expect (nearly (range.start, -12.0f) && nearly (range.end, 12.0f),
                "the formant shift range is not -12..+12 semitones");
    }
    else
    {
        expect (false, "the formant shift parameter is missing");
    }

    for (const char* id : { vowelX, vowelY, vowelMorph, glide, roomSize, dynamics })
    {
        if (auto* parameter = state.getParameter (id))
        {
            const auto range = parameter->getNormalisableRange();
            expect (nearly (range.start, 0.0f) && nearly (range.end, 1.0f),
                    std::string ("the range of ") + id + " is not 0..1");
        }
        else
        {
            expect (false, std::string ("parameter ") + id + " is missing");
        }
    }

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (legato)))
        expect (choice->choices.size() == 2, "legato is not a two-state choice");
    else
        expect (false, "legato is not an AudioParameterChoice");
}

/** The 1.2 MIDI performance messages have to reach the engine at all: the 1.1
    processor parsed note-on, note-off and CC 120/123 and silently discarded
    every other message, pitch bend included. */
void testPerformanceMidiReachesTheEngine()
{
    // Pitch bend moves the sound without changing the note count.
    {
        ProcessorHarness harness;
        harness.chargeVoiceAndRoom();
        const auto before = peakInRange (harness.buffer, 0, blockSize);

        juce::MidiBuffer bend;
        bend.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
        harness.process (std::move (bend));
        for (int block = 0; block < 8; ++block)
            harness.process();

        expect (harness.processor.getActiveVoiceCount() > 0,
                "a pitch bend silenced the sounding voice");
        expect (peakInRange (harness.buffer, 0, blockSize) > 1.0e-8 && before > 1.0e-8,
                "a pitch bend left no audio");
    }

    // The mod wheel is a dynamic, so an empty wheel has to be audibly quieter
    // than a full one while the note keeps sounding.
    {
        ProcessorHarness harness;
        harness.chargeVoiceAndRoom();
        const auto loud = peakInRange (harness.buffer, 0, blockSize);

        juce::MidiBuffer wheel;
        wheel.addEvent (juce::MidiMessage::controllerEvent (1, 1, 0), 0);
        harness.process (std::move (wheel));
        for (int block = 0; block < 120; ++block)
            harness.process();
        const auto soft = peakInRange (harness.buffer, 0, blockSize);

        expect (harness.processor.getActiveVoiceCount() > 0,
                "an empty mod wheel ended the note instead of softening it");
        expect (soft < 0.5 * loud && soft > 0.0,
                "the mod wheel did not lower the dynamic level");
    }

    // Channel pressure feeds the same dynamic.
    {
        ProcessorHarness harness;
        harness.chargeVoiceAndRoom();
        const auto loud = peakInRange (harness.buffer, 0, blockSize);

        juce::MidiBuffer pressure;
        pressure.addEvent (juce::MidiMessage::channelPressureChange (1, 0), 0);
        harness.process (std::move (pressure));
        for (int block = 0; block < 120; ++block)
            harness.process();

        expect (peakInRange (harness.buffer, 0, blockSize) < 0.5 * loud,
                "channel pressure did not reach the dynamic");
    }

    // The sustain pedal holds a note through its note-off and releases it on
    // pedal-up.
    {
        ProcessorHarness harness;
        harness.chargeVoiceAndRoom();

        juce::MidiBuffer down;
        down.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
        down.addEvent (juce::MidiMessage::noteOff (1, 60), 1);
        harness.process (std::move (down));
        for (int block = 0; block < 20; ++block)
            harness.process();
        const auto sustained = peakInRange (harness.buffer, 0, blockSize);
        expect (sustained > 1.0e-6,
                "the sustain pedal did not hold the note through its note-off");

        juce::MidiBuffer up;
        up.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        harness.process (std::move (up));
        for (int block = 0; block < 600; ++block)
            harness.process();
        expect (harness.processor.getActiveVoiceCount() == 0,
                "releasing the sustain pedal did not release the note it held");
    }
}

void testVowelPadIsInaudibleAtZeroMorph()
{
    // The pad has to be a no-op until the morph control is opened, otherwise a
    // recalled version-1 session would not sound the same.
    const auto renderNote = [] (float padX, float padY)
    {
        VocalorAudioProcessor processor;
        setNormalised (processor.parameters, vocalor::parameters::vowelX, padX);
        setNormalised (processor.parameters, vocalor::parameters::vowelY, padY);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> captured;
        juce::MidiBuffer noteOn;
        noteOn.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);

        for (int block = 0; block < 24; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi = block == 0 ? noteOn : juce::MidiBuffer {};
            processor.processBlock (buffer, midi);
            for (int sample = 0; sample < blockSize; ++sample)
                captured.push_back (buffer.getSample (0, sample));
        }

        processor.releaseResources();
        return captured;
    };

    const auto centred = renderNote (0.5f, 0.5f);
    const auto cornered = renderNote (1.0f, 0.0f);
    expect (centred.size() == cornered.size() && centred == cornered,
            "the vowel pad changed the sound while vowel morph was at its zero default");
}

void testNewParametersReachTheEngine()
{
    ProcessorHarness harness;
    auto& state = harness.processor.parameters;

    harness.chargeVoiceAndRoom();
    const auto neutral = harness.processor.getDisplayState();
    expect (neutral.formantHz[0] > 100.0f, "the display state exposed no tract");
    expect (neutral.levelLeft > 0.0f, "the display meters stayed silent while sounding");

    setNormalised (state, vocalor::parameters::vowelMorph, 1.0f);
    setNormalised (state, vocalor::parameters::vowelX, 1.0f);
    setNormalised (state, vocalor::parameters::vowelY, 0.0f);
    for (int block = 0; block < 40; ++block)
        harness.process();

    const auto morphed = harness.processor.getDisplayState();
    expect (morphed.formantHz[1] > neutral.formantHz[1] + 700.0f,
            "the vowel morph parameters did not reach the engine");

    setDenormalised (state, vocalor::parameters::formantShift, 12.0f);
    for (int block = 0; block < 40; ++block)
        harness.process();
    const auto shifted = harness.processor.getDisplayState();
    expect (shifted.formantHz[0] > morphed.formantHz[0] * 1.5f,
            "the formant shift parameter did not reach the engine");
    expect (isFinite (harness.buffer), "an extreme formant shift produced invalid audio");

    setDenormalised (state, vocalor::parameters::formantShift, 0.0f);
    setNormalised (state, vocalor::parameters::roomSize, 1.0f);
    setNormalised (state, vocalor::parameters::glide, 1.0f);
    for (int block = 0; block < 20; ++block)
        harness.process();
    expect (isFinite (harness.buffer), "the room-size and glide parameters destabilised the engine");
}

void testLegatoKeepsASingleVoice()
{
    ProcessorHarness harness;
    setNormalised (harness.processor.parameters, vocalor::parameters::legato, 1.0f);
    setNormalised (harness.processor.parameters, vocalor::parameters::glide, 0.4f);

    juce::MidiBuffer first;
    first.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (110)), 0);
    harness.process (std::move (first));
    for (int block = 0; block < 40; ++block)
        harness.process();

    const auto voicesBefore = harness.processor.getActiveVoiceCount();
    expect (voicesBefore == 1, "solo mode did not produce exactly one voice");

    juce::MidiBuffer second;
    second.addEvent (juce::MidiMessage::noteOn (1, 67, static_cast<juce::uint8> (110)), eventOffset);
    harness.process (std::move (second));
    for (int block = 0; block < 8; ++block)
        harness.process();

    expect (harness.processor.getActiveVoiceCount() == voicesBefore,
            "a legato overlap started an extra voice");
    expect (peakInRange (harness.buffer, 0, blockSize) > 1.0e-8,
            "a legato overlap silenced the instrument");

    juce::MidiBuffer release;
    release.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
    harness.process (std::move (release));
    for (int block = 0; block < 8; ++block)
        harness.process();
    expect (harness.processor.getActiveVoiceCount() == voicesBefore,
            "releasing the upper legato note stopped the phrase");

    juce::MidiBuffer last;
    last.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    harness.process (std::move (last));
    constexpr int releaseBlocks = static_cast<int> (sampleRate * 5.0 / blockSize);
    for (int block = 0; block < releaseBlocks; ++block)
        harness.process();
    expect (harness.processor.getActiveVoiceCount() == 0,
            "a legato phrase never released its final note");
}

void testStateRoundTrip()
{
    VocalorAudioProcessor source;
    using namespace vocalor::parameters;

    setNormalised (source.parameters, vowelMorph, 0.75f);
    setNormalised (source.parameters, vowelX, 0.25f);
    setNormalised (source.parameters, vowelY, 0.90f);
    setNormalised (source.parameters, glide, 0.40f);
    setNormalised (source.parameters, roomSize, 0.20f);
    setNormalised (source.parameters, legato, 1.0f);
    setDenormalised (source.parameters, formantShift, -5.5f);
    setNormalised (source.parameters, breath, 0.80f);

    juce::MemoryBlock stored;
    source.getStateInformation (stored);
    expect (stored.getSize() > 0, "the processor saved an empty state");

    VocalorAudioProcessor destination;
    destination.setStateInformation (stored.getData(), static_cast<int> (stored.getSize()));

    const auto valueOf = [] (const juce::AudioProcessorValueTreeState& state, const char* id)
    {
        auto* raw = state.getRawParameterValue (id);
        return raw != nullptr ? raw->load() : std::numeric_limits<float>::quiet_NaN();
    };

    for (const char* id : { vowelMorph, vowelX, vowelY, glide, roomSize, legato,
                            formantShift, breath })
        expect (nearly (valueOf (destination.parameters, id), valueOf (source.parameters, id), 0.002f),
                std::string ("parameter ") + id + " did not survive a state round trip");
}

void testVersionOneSessionsStillLoad()
{
    using namespace vocalor::parameters;
    VocalorAudioProcessor processor;

    // A saved version-1 session contains only the original thirteen parameters.
    juce::ValueTree legacy (processor.parameters.state.getType());
    const auto addParameter = [&legacy] (const char* id, float value)
    {
        juce::ValueTree entry ("PARAM");
        entry.setProperty ("id", id, nullptr);
        entry.setProperty ("value", value, nullptr);
        legacy.appendChild (entry, nullptr);
    };

    addParameter (profile, 1.0f);
    addParameter (mode, 1.0f);
    addParameter (vowel, 2.0f);
    addParameter (chordQuality, 0.0f);
    addParameter (choirSize, 12.0f);
    addParameter (breath, 0.71f);
    addParameter (resonance, 0.44f);
    addParameter (vibrato, 0.66f);
    addParameter (humanize, 0.33f);
    addParameter (spread, 0.88f);
    addParameter (tension, 0.55f);
    addParameter (room, 0.11f);
    addParameter (output, -3.5f);

    juce::MemoryBlock stored;
    if (const auto xml = legacy.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, stored);
    else
        expect (false, "the legacy state could not be serialised");

    processor.setStateInformation (stored.getData(), static_cast<int> (stored.getSize()));

    const auto valueOf = [&processor] (const char* id)
    {
        auto* raw = processor.parameters.getRawParameterValue (id);
        return raw != nullptr ? raw->load() : std::numeric_limits<float>::quiet_NaN();
    };

    expect (nearly (valueOf (breath), 0.71f, 0.002f)
                && nearly (valueOf (spread), 0.88f, 0.002f)
                && nearly (valueOf (output), -3.5f, 0.01f)
                && nearly (valueOf (choirSize), 12.0f, 0.01f),
            "a version-1 session did not restore its original parameter values");

    // The 1.1 additions have to stay in a range the engine accepts, whatever a
    // host does with parameters it has never seen before.
    for (const char* id : { vowelX, vowelY, vowelMorph, glide, roomSize })
    {
        const auto value = valueOf (id);
        expect (std::isfinite (value) && value >= 0.0f && value <= 1.0f,
                std::string ("parameter ") + id + " left its range after loading a legacy session");
    }
    const auto shift = valueOf (formantShift);
    expect (std::isfinite (shift) && shift >= -12.0f && shift <= 12.0f,
            "the formant shift left its range after loading a legacy session");

    // The same load into an instance that has already been sung through. APVTS
    // keeps a parameter's live value when the stored tree has no child for it,
    // so without the migration a version-1 session would inherit the player's
    // morph, glide and legato instead of the defaults it was saved with.
    VocalorAudioProcessor used;
    const auto setValue = [&used] (const char* id, float value)
    {
        auto* parameter = used.parameters.getParameter (id);
        expect (parameter != nullptr, std::string ("cannot set missing parameter ") + id);
        if (parameter != nullptr)
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    setValue (vowelX, 0.93f);
    setValue (vowelY, 0.07f);
    setValue (vowelMorph, 0.85f);
    setValue (formantShift, 7.0f);
    setValue (glide, 0.62f);
    setValue (legato, 1.0f);
    setValue (roomSize, 0.19f);

    used.setStateInformation (stored.getData(), static_cast<int> (stored.getSize()));

    VocalorAudioProcessor pristine;
    for (const char* id : { vowelX, vowelY, vowelMorph, formantShift, glide,
                            legato, roomSize })
    {
        auto* live = used.parameters.getRawParameterValue (id);
        auto* fresh = pristine.parameters.getRawParameterValue (id);
        expect (live != nullptr && fresh != nullptr,
                std::string ("missing parameter ") + id);
        if (live != nullptr && fresh != nullptr)
            expect (nearly (live->load(), fresh->load(), 0.002f),
                    std::string ("a version-1 session kept the live value of ") + id
                        + " instead of its default");
    }
    auto* usedBreath = used.parameters.getRawParameterValue (breath);
    expect (usedBreath != nullptr && nearly (usedBreath->load(), 0.71f, 0.002f),
            "reloading into a used instance lost a stored value");

    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 55, static_cast<juce::uint8> (96)), 0);
    for (int block = 0; block < 24; ++block)
    {
        buffer.clear();
        juce::MidiBuffer events = block == 0 ? midi : juce::MidiBuffer {};
        processor.processBlock (buffer, events);
        expect (isFinite (buffer), "a restored version-1 session produced invalid audio");
    }
    expect (peakInRange (buffer, 0, blockSize) > 1.0e-8,
            "a restored version-1 session produced no audio");
    processor.releaseResources();
}

void testEditorRendering()
{
    VocalorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "the processor produced no editor");
    if (editor == nullptr)
        return;

    editor->resized();
    const auto snapshot = renderEditorSnapshot (*editor);
    expect (snapshotHasDetail (snapshot),
            "the editor rendered as a flat surface at its default size");

    // Committed documentation image, regenerated by the nightly build.
    const auto snapshotPath =
        juce::SystemStats::getEnvironmentVariable ("VOCALOR_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty() && snapshot.isValid())
    {
        // A clean checkout has no screenshots directory: git does not track an
        // empty one, and the nightly run is what refreshes the image.
        const juce::File snapshotFile { snapshotPath };
        snapshotFile.getParentDirectory().createDirectory();
        juce::FileOutputStream output { snapshotFile };
        juce::PNGImageFormat png;
        const bool prepared = output.openedOk() && output.setPosition (0)
                           && output.truncate();
        const bool wrote = prepared && png.writeImageToStream (snapshot, output);
        output.flush();
        expect (wrote, "could not write the requested editor snapshot");
    }

    editor.reset();
    processor.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    testPanicHardStopsAtBlockBoundary();
    testMidiAllSoundOffIsSampleAccurate();
    testMidiAllNotesOffKeepsRelease();
    testParameterLayoutIsStable();
    testPerformanceMidiReachesTheEngine();
    testVowelPadIsInaudibleAtZeroMorph();
    testNewParametersReachTheEngine();
    testLegatoKeepsASingleVoice();
    testStateRoundTrip();
    testVersionOneSessionsStillLoad();
    testEditorRendering();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Vocalor processor check(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Vocalor processor checks passed.\n";
    return EXIT_SUCCESS;
}
