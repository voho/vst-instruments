#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <string>

namespace
{
using namespace youknow106;

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

// The default patch is deliberately the hardware-faithful one: no velocity
// response, six voices, and the delay lines' own noise at its modelled level.
constexpr std::array<ParameterExpectation, 37> expectedParameters {{
    { parameters::volume,      0.80f,  1.0e-5f },
    { parameters::benderDco,   0.30f,  1.0e-5f },
    { parameters::benderVcf,   0.0f,   1.0e-5f },
    { parameters::benderLfo,   0.0f,   1.0e-5f },
    { parameters::portamento,  0.0f,   1.0e-5f },
    { parameters::keyMode,     0.0f,   1.0e-5f },
    { parameters::lfoRate,     0.42f,  1.0e-5f },
    { parameters::lfoDelay,    0.0f,   1.0e-5f },
    { parameters::dcoLfo,      0.0f,   1.0e-5f },
    { parameters::pwm,         0.30f,  1.0e-5f },
    { parameters::pwmMode,     1.0f,   1.0e-5f },
    { parameters::range,       1.0f,   1.0e-5f },
    { parameters::saw,         1.0f,   1.0e-5f },
    { parameters::pulse,       0.0f,   1.0e-5f },
    { parameters::sub,         0.0f,   1.0e-5f },
    { parameters::noise,       0.0f,   1.0e-5f },
    { parameters::highPass,    1.0f,   1.0e-5f },
    { parameters::cutoff,      0.62f,  1.0e-5f },
    { parameters::resonance,   0.10f,  1.0e-5f },
    { parameters::envPolarity, 0.0f,   1.0e-5f },
    { parameters::vcfEnv,      0.35f,  1.0e-5f },
    { parameters::vcfLfo,      0.0f,   1.0e-5f },
    { parameters::keyFollow,   0.50f,  1.0e-5f },
    { parameters::vcaMode,     0.0f,   1.0e-5f },
    { parameters::vcaLevel,    0.80f,  1.0e-5f },
    { parameters::attack,      0.04f,  1.0e-5f },
    { parameters::decay,       0.45f,  1.0e-5f },
    { parameters::sustain,     0.70f,  1.0e-5f },
    { parameters::release,     0.30f,  1.0e-5f },
    { parameters::chorus,      0.0f,   1.0e-5f },
    { parameters::transpose,   0.0f,   1.0e-5f },
    { parameters::masterTune,  0.0f,   1.0e-5f },
    { parameters::velocity,    0.0f,   1.0e-5f },
    { parameters::calibration, 0.35f,  1.0e-5f },
    { parameters::chorusNoise, 1.0f,   1.0e-5f },
    { parameters::polyphony,   6.0f,   1.0e-5f },
    { parameters::hq,          1.0f,   1.0e-5f },
}};

float parameterValue (const YouKnow106AudioProcessor& processor, const char* id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, std::string ("missing parameter ") + id);
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (YouKnow106AudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot set missing parameter ") + id);
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

bool bufferIsFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);
        for (int index = 0; index < buffer.getNumSamples(); ++index)
            if (! std::isfinite (samples[index]))
                return false;
    }
    return true;
}

float bufferPeak (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = std::max (peak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    return peak;
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

void renderBlocks (YouKnow106AudioProcessor& processor, juce::AudioBuffer<float>& buffer,
                   int blocks)
{
    juce::MidiBuffer midi;
    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
    }
}

// --------------------------------------------------------------------------

void testParameterContract()
{
    YouKnow106AudioProcessor processor;

    expect (processor.getParameters().size() == static_cast<int> (expectedParameters.size()),
            "parameter count changed without the contract being updated");

    std::set<juce::String> seen;
    for (const auto& expected : expectedParameters)
    {
        const auto* parameter = processor.parameters.getParameter (expected.id);
        expect (parameter != nullptr, std::string ("missing parameter ") + expected.id);
        if (parameter == nullptr)
            continue;

        expect (seen.insert (expected.id).second,
                std::string ("duplicate parameter ") + expected.id);
        expect (std::abs (parameterValue (processor, expected.id) - expected.defaultValue)
                    <= expected.tolerance,
                std::string ("unexpected default for ") + expected.id);
        expect (parameter->getName (64).isNotEmpty(),
                std::string ("parameter has no name: ") + expected.id);
    }

    // Every control the panel names has to exist, or the editor would attach to
    // nothing and the layout check would be testing a fiction.
    for (const auto& control : panel::controls())
        expect (processor.parameters.getParameter (control.parameterId) != nullptr,
                std::string ("panel names a parameter the processor does not have: ")
                    + control.parameterId);
}

void testParameterTextRoundTrips()
{
    YouKnow106AudioProcessor processor;

    // Panel positions that stand for a time or a frequency must display the
    // value the modelled circuit produces, not the slider's own travel.
    const auto textFor = [&processor] (const char* id, float value) {
        auto* parameter = processor.parameters.getParameter (id);
        return parameter != nullptr
                   ? parameter->getText (parameter->convertTo0to1 (value), 64)
                   : juce::String();
    };

    expect (textFor (parameters::attack, 0.0f).contains ("ms"),
            "the shortest attack is not shown in milliseconds");
    expect (textFor (parameters::decay, 1.0f).contains ("s"),
            "the longest decay is not shown in seconds");
    expect (textFor (parameters::cutoff, 1.0f).contains ("kHz"),
            "a wide-open filter is not shown in kilohertz");
    expect (textFor (parameters::lfoRate, 0.0f).contains ("Hz"),
            "the modulation rate is not shown in hertz");
    expect (textFor (parameters::portamento, 0.0f) == "OFF",
            "portamento at rest is not shown as switched off");
    expect (textFor (parameters::portamento, 1.0f).contains ("/oct"),
            "portamento is not shown per octave");
}

void testProcessingProducesSound()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    buffer.clear();
    processor.processBlock (buffer, midi);

    renderBlocks (processor, buffer, 40);
    expect (bufferIsFinite (buffer), "the processor emitted a non-finite sample");
    expect (bufferPeak (buffer) > 0.001f, "a held note produced silence");
    expect (processor.getActiveVoiceCount() == 1, "one held key is not one voice");
    expect (processor.isEngineReady(), "the engine did not report itself ready");

    // Note off, then long enough for the release and the delay lines to settle.
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    buffer.clear();
    processor.processBlock (buffer, off);
    renderBlocks (processor, buffer, 400);
    expect (processor.getActiveVoiceCount() == 0, "the voice never released");

    processor.releaseResources();
}

void testShortNoteInsideOneBlockIsHeard()
{
    // A note that opens and closes inside a single buffer must still sound.
    // Dispatching a block's MIDI at its boundary would apply both events before
    // any audio was rendered and lose the note entirely.
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::attack, 0.0f);
    setParameterValue (processor, parameters::sustain, 1.0f);
    setParameterValue (processor, parameters::release, 0.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), blockSize - 1);
    buffer.clear();
    processor.processBlock (buffer, midi);

    expect (bufferPeak (buffer) > 0.0f,
            "a note contained inside one block produced no audio at all");

    // The two events must also land where they were timed: an event at the end
    // of the block cannot affect the start of it.
    juce::AudioBuffer<float> late (2, blockSize);
    juce::MidiBuffer lateMidi;
    lateMidi.addEvent (juce::MidiMessage::noteOn (1, 72, 1.0f), blockSize - 2);
    late.clear();
    processor.releaseResources();
    processor.prepareToPlay (sampleRate, blockSize);
    processor.processBlock (late, lateMidi);

    float earlyPeak = 0.0f;
    for (int index = 0; index < blockSize / 2; ++index)
        earlyPeak = std::max (earlyPeak, std::abs (late.getSample (0, index)));
    expect (earlyPeak == 0.0f,
            "an event timed at the end of a block was applied at its start");

    processor.releaseResources();
}

void testAllNotesOffReleasesAndAllSoundOffCuts()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::release, 0.75f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    buffer.clear();
    processor.processBlock (buffer, midi);
    renderBlocks (processor, buffer, 16);

    juce::MidiBuffer notesOff;
    notesOff.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    buffer.clear();
    processor.processBlock (buffer, notesOff);
    renderBlocks (processor, buffer, 8);
    expect (bufferPeak (buffer) > 0.001f,
            "all-notes-off cut the release instead of letting it ring");

    juce::MidiBuffer soundOff;
    soundOff.addEvent (juce::MidiMessage::allSoundOff (1), 0);
    buffer.clear();
    processor.processBlock (buffer, soundOff);
    renderBlocks (processor, buffer, 4);
    expect (processor.getActiveVoiceCount() == 0,
            "all-sound-off left a voice running");

    processor.releaseResources();
}

void testTransportOfControllers()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::release, 0.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, 1.0f), 1);
    buffer.clear();
    processor.processBlock (buffer, midi);
    renderBlocks (processor, buffer, 8);

    juce::MidiBuffer release;
    release.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
    buffer.clear();
    processor.processBlock (buffer, release);
    renderBlocks (processor, buffer, 20);
    expect (processor.getActiveVoiceCount() == 1,
            "the hold controller did not hold the note");

    juce::MidiBuffer lift;
    lift.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    buffer.clear();
    processor.processBlock (buffer, lift);
    renderBlocks (processor, buffer, 200);
    expect (processor.getActiveVoiceCount() == 0,
            "the note did not release when hold was lifted");

    processor.releaseResources();
}

void testPanicSilencesEverything()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    for (int note = 60; note < 66; ++note)
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);
    buffer.clear();
    processor.processBlock (buffer, midi);
    renderBlocks (processor, buffer, 8);
    expect (processor.getActiveVoiceCount() > 0, "no voices to silence");

    processor.requestPanic();
    renderBlocks (processor, buffer, 4);
    expect (processor.getActiveVoiceCount() == 0, "panic left voices sounding");

    processor.releaseResources();
}

void testStateRoundTripAndMigration()
{
    YouKnow106AudioProcessor source;
    setParameterValue (source, parameters::cutoff, 0.31f);
    setParameterValue (source, parameters::resonance, 0.77f);
    setParameterValue (source, parameters::chorus, 2.0f);
    setParameterValue (source, parameters::range, 0.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    YouKnow106AudioProcessor destination;
    destination.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (std::abs (parameterValue (destination, parameters::cutoff) - 0.31f) < 1.0e-4f,
            "cutoff did not survive a state round trip");
    expect (std::abs (parameterValue (destination, parameters::resonance) - 0.77f) < 1.0e-4f,
            "resonance did not survive a state round trip");
    expect (std::abs (parameterValue (destination, parameters::chorus) - 2.0f) < 1.0e-4f,
            "the effect switch did not survive a state round trip");

    // A state written before a parameter existed must load, keeping what it
    // does carry and filling the rest with defaults.
    auto trimmed = source.parameters.copyState();
    for (int index = trimmed.getNumChildren(); --index >= 0;)
        if (trimmed.getChild (index).getProperty ("id").toString() == parameters::calibration)
            trimmed.removeChild (index, nullptr);

    juce::MemoryBlock legacy;
    if (const auto xml = trimmed.createXml())
    {
        juce::AudioProcessor::copyXmlToBinary (*xml, legacy);
        YouKnow106AudioProcessor migrated;
        migrated.setStateInformation (legacy.getData(),
                                      static_cast<int> (legacy.getSize()));
        expect (std::abs (parameterValue (migrated, parameters::calibration) - 0.35f)
                    < 1.0e-4f,
                "a missing parameter did not fall back to its default");
        expect (std::abs (parameterValue (migrated, parameters::cutoff) - 0.31f) < 1.0e-4f,
                "loading an older state discarded the parameters it did carry");
    }
    else
    {
        expect (false, "could not serialise a trimmed state");
    }
}

void testRandomizerPreservesQualityAndLevel()
{
    YouKnow106AudioProcessor processor;
    const float volume = parameterValue (processor, parameters::volume);
    const float voices = parameterValue (processor, parameters::polyphony);
    const float quality = parameterValue (processor, parameters::hq);

    processor.randomizeParameters (1.0f);

    expect (std::abs (parameterValue (processor, parameters::volume) - volume) < 1.0e-4f,
            "the randomiser moved the output level");
    expect (std::abs (parameterValue (processor, parameters::polyphony) - voices) < 1.0e-4f,
            "the randomiser moved the voice count");
    expect (std::abs (parameterValue (processor, parameters::hq) - quality) < 1.0e-4f,
            "the randomiser moved a quality setting");
}

void testBusLayoutsAndTail()
{
    YouKnow106AudioProcessor processor;
    expect (processor.getTailLengthSeconds() >= 12.0,
            "the reported tail is shorter than the longest release");
    expect (processor.acceptsMidi() && ! processor.producesMidi(),
            "the plug-in does not advertise itself as an instrument");
}

void testEditorBuildsAndRenders()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "the processor produced no editor");
    if (editor == nullptr)
        return;

    expect (editor->getWidth() == 1276 && editor->getHeight() == 470,
            "the editor did not open at its default size");
    expect (editor->isOpaque(), "the editor does not advertise an opaque surface");

    // Exercise the layout at both extremes as well as at its default size: a
    // divide-by-zero or a negative bound only shows up at one end.
    for (auto size : { juce::Point<int> { 900, 380 },
                       juce::Point<int> { 2200, 940 } })
    {
        editor->setSize (size.x, size.y);
        editor->resized();
        expect (snapshotHasDetail (renderEditorSnapshot (*editor)),
                "the editor did not render at "
                    + juce::String (size.x).toStdString() + " wide");
    }

    editor->setSize (1276, 470);
    editor->resized();
    const auto snapshot = renderEditorSnapshot (*editor);
    expect (snapshotHasDetail (snapshot),
            "the editor rendered as a flat surface at its default size");

    // Committed documentation image, regenerated by the nightly build.
    const auto snapshotPath =
        juce::SystemStats::getEnvironmentVariable ("YOUKNOW106_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty() && snapshot.isValid())
    {
        // A clean checkout has no screenshots directory: git does not track an
        // empty one, and the first nightly run is what creates the image.
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
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testParameterContract();
    testParameterTextRoundTrips();
    testProcessingProducesSound();
    testShortNoteInsideOneBlockIsHeard();
    testAllNotesOffReleasesAndAllSoundOffCuts();
    testTransportOfControllers();
    testPanicSilencesEverything();
    testStateRoundTripAndMigration();
    testRandomizerPreservesQualityAndLevel();
    testBusLayoutsAndTail();
    testEditorBuildsAndRenders();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " YouKnow106 plug-in check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All YouKnow106 plug-in checks passed.\n";
    return EXIT_SUCCESS;
}
