// Plug-in layer suite: the host-facing contract — parameter layout
// stability, MIDI reaching the engine, panic and all-notes-off semantics,
// state round-trips, and the editor rendering (which also produces the
// committed documentation screenshot when GHOST_EDITOR_SNAPSHOT is set).

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
int failureCount = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool isFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (!std::isfinite(samples[sample]))
                return false;
    }
    return true;
}

double peakOf(const juce::AudioBuffer<float>& buffer)
{
    double peak = 0.0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = std::max(peak,
                            std::abs(static_cast<double>(samples[sample])));
    }
    return peak;
}

// Renders `blocks` blocks, feeding `midi` into the first one; returns the
// peak of the final block.
double renderBlocks(GhostAudioProcessor& processor, int blocks,
                    const juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> buffer(2, blockSize);
    double lastPeak = 0.0;
    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer events = block == 0 ? midi : juce::MidiBuffer {};
        processor.processBlock(buffer, events);
        expect(isFinite(buffer), "a processed block carried non-finite audio");
        lastPeak = peakOf(buffer);
    }
    return lastPeak;
}

juce::Image renderEditorSnapshot(juce::AudioProcessorEditor& editor)
{
    juce::Image snapshot(juce::Image::ARGB, editor.getWidth(),
                         editor.getHeight(), true);
    juce::Graphics graphics(snapshot);
    editor.paintEntireComponent(graphics, true);
    return snapshot;
}

// A panel that rendered as one flat colour would still be "valid"; what we
// actually want to know is that it drew something.
bool snapshotHasDetail(const juce::Image& snapshot)
{
    if (!snapshot.isValid())
        return false;

    std::set<juce::uint32> distinct;
    for (int y = 0; y < snapshot.getHeight(); y += 4)
        for (int x = 0; x < snapshot.getWidth(); x += 4)
        {
            const auto pixel = snapshot.getPixelAt(x, y);
            if (pixel.getAlpha() < 250)
                return false;
            distinct.insert(pixel.getARGB());
            if (distinct.size() > 64)
                return true;
        }
    return distinct.size() > 8;
}

// The automation contract: every published ID, exactly once. A removed or
// renamed ID breaks saved sessions, so the full list is pinned here.
void testParameterLayoutIsStable()
{
    GhostAudioProcessor processor;
    namespace ids = ghost::parameters;
    const char* expectedIds[] = {
        ids::tune, ids::octave, ids::oscAWaveform, ids::sync,
        ids::oscBWaveform, ids::oscBRange, ids::interval, ids::trigger,
        ids::gateKbd, ids::gateX, ids::gateYExt, ids::arpeggiator,
        ids::modSource, ids::lfoRate, ids::shaperMode, ids::shaperShape,
        ids::shaperRate, ids::modXTo, ids::shapeXWithY, ids::shaperYTo,
        ids::masterVolume, ids::brightness, ids::shaperPathA,
        ids::shaperPathB, ids::shaperPathRing, ids::shaperPathNoise,
        ids::filterPathA, ids::filterPathB, ids::filterPathNoise, ids::cutoff,
        ids::lowerOnly, ids::upperResonance, ids::resonance, ids::slope,
        ids::kbAmount, ids::lowerMode, ids::tracking, ids::filterEnvAmount,
        ids::filterAttack, ids::filterDecay, ids::filterSustain,
        ids::filterRelease, ids::vcaBypass, ids::loudnessAttack,
        ids::loudnessDecay, ids::loudnessSustain, ids::loudnessRelease,
        ids::glide, ids::glideMode, ids::xWheel, ids::yWheel,
        ids::splitPaths,
    };

    int found = 0;
    for (const auto* id : expectedIds)
    {
        if (processor.parameters.getParameter(id) != nullptr)
            ++found;
        else
            expect(false, std::string("missing parameter: ") + id);
    }
    expect(found == static_cast<int>(std::size(expectedIds)),
           "the published parameter set changed");
}

void testMidiProducesAudio()
{
    GhostAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::MidiBuffer midi;
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 7);
    const double sounding = renderBlocks(processor, 24, midi);
    expect(sounding > 1.0e-4, "a keyed note produced no audio");

    juce::MidiBuffer off;
    off.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
    renderBlocks(processor, 1, off);
    juce::MidiBuffer silentTail;
    const double released = renderBlocks(processor, 400, silentTail);
    expect(released < 1.0e-4, "a released note did not decay to silence");
    processor.releaseResources();
}

void testAllNotesOffReleasesEveryKey()
{
    GhostAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::MidiBuffer midi;
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 55, static_cast<juce::uint8>(100)), 8);
    renderBlocks(processor, 8, midi);
    expect(processor.isGateOpenForDisplay(), "keyed notes opened the gate");

    juce::MidiBuffer allNotesOff;
    allNotesOff.addEvent(
        juce::MidiMessage::controllerEvent(1, 123, 0), 0);
    renderBlocks(processor, 1, allNotesOff);
    expect(!processor.isGateOpenForDisplay(),
           "CC123 did not release the held keys");

    juce::MidiBuffer quiet;
    const double tail = renderBlocks(processor, 400, quiet);
    expect(tail < 1.0e-4, "the all-notes-off release did not reach silence");
    processor.releaseResources();
}

void testStateRoundTrip()
{
    namespace ids = ghost::parameters;
    GhostAudioProcessor saved;
    if (auto* parameter = saved.parameters.getParameter(ids::cutoff))
        parameter->setValueNotifyingHost(0.31f);
    if (auto* parameter = saved.parameters.getParameter(ids::lowerMode))
        parameter->setValueNotifyingHost(1.0f); // the last detent: High Pass
    juce::MemoryBlock state;
    saved.getStateInformation(state);

    GhostAudioProcessor restored;
    restored.setStateInformation(state.getData(),
                                 static_cast<int>(state.getSize()));
    auto* cutoff = restored.parameters.getRawParameterValue(ids::cutoff);
    expect(cutoff != nullptr && std::abs(cutoff->load() - 0.31f) < 0.002f,
           "a stored travel value did not survive the round trip");
    auto* lowerMode = restored.parameters.getRawParameterValue(ids::lowerMode);
    expect(lowerMode != nullptr
               && std::lround(lowerMode->load()) == 3,
           "a stored switch position did not survive the round trip");
}

// The factory bank is the manual's eleven Sound Charts. The host-facing
// contract: the count and names are published, selecting a program writes
// its chart into the host parameters, and a selected chart actually plays.
void testFactoryProgramsAreTheSoundCharts()
{
    namespace ids = ghost::parameters;
    GhostAudioProcessor processor;

    expect(processor.getNumPrograms() == 11,
           "the program bank is not the manual's eleven Sound Charts");
    expect(processor.getProgramName(0) == "Preparatory Pattern",
           "the bank does not open with the Preparatory Pattern");
    expect(processor.getProgramName(2) == "Fat Filter",
           "program 2 is not the Fat Filter chart");

    processor.setCurrentProgram(2);
    expect(processor.getCurrentProgram() == 2,
           "the selected program index was not retained");

    auto* cutoff = processor.parameters.getRawParameterValue(ids::cutoff);
    expect(cutoff != nullptr && std::abs(cutoff->load() - 0.45f) < 0.002f,
           "Fat Filter did not write its cutoff travel");
    auto* gateKbd = processor.parameters.getRawParameterValue(ids::gateKbd);
    expect(gateKbd != nullptr && gateKbd->load() > 0.5f,
           "Fat Filter did not switch the keyboard gate on");
    auto* octave = processor.parameters.getRawParameterValue(ids::octave);
    expect(octave != nullptr && std::lround(octave->load()) == 2,
           "Fat Filter did not select the 8' octave");
    auto* xWheel = processor.parameters.getRawParameterValue(ids::xWheel);
    expect(xWheel != nullptr && xWheel->load() < 0.002f,
           "selecting a chart did not pull the X wheel fully back");

    // An out-of-range selection must be ignored, not clamp or crash.
    processor.setCurrentProgram(11);
    expect(processor.getCurrentProgram() == 2,
           "an out-of-range program selection was not ignored");

    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
    const double sounding = renderBlocks(processor, 24, midi);
    expect(sounding > 1.0e-4, "the selected Sound Chart produced no audio");
    processor.releaseResources();
}

void testEditorRendering()
{
    GhostAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    std::unique_ptr<juce::AudioProcessorEditor> editor(
        processor.createEditor());
    expect(editor != nullptr, "the processor produced no editor");
    if (editor == nullptr)
        return;

    editor->resized();
    const auto snapshot = renderEditorSnapshot(*editor);
    expect(snapshotHasDetail(snapshot),
           "the editor rendered as a flat surface at its default size");

    // Committed documentation image, regenerated by the nightly build.
    const auto snapshotPath = juce::SystemStats::getEnvironmentVariable(
        "GHOST_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty() && snapshot.isValid())
    {
        const juce::File snapshotFile { snapshotPath };
        snapshotFile.getParentDirectory().createDirectory();
        juce::FileOutputStream output { snapshotFile };
        juce::PNGImageFormat png;
        const bool prepared = output.openedOk() && output.setPosition(0)
                           && output.truncate();
        const bool wrote =
            prepared && png.writeImageToStream(snapshot, output);
        output.flush();
        expect(wrote, "could not write the requested editor snapshot");
    }

    editor.reset();
    processor.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    testParameterLayoutIsStable();
    testMidiProducesAudio();
    testAllNotesOffReleasesEveryKey();
    testStateRoundTrip();
    testFactoryProgramsAreTheSoundCharts();
    testEditorRendering();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Ghost plug-in check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Ghost plug-in checks passed.\n";
    return EXIT_SUCCESS;
}
