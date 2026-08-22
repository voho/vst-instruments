// Plug-in layer suite: the host-facing contract — parameter layout
// stability, MIDI reaching the engine, panic and all-notes-off semantics,
// state round-trips, and the editor rendering (which also produces the
// committed documentation screenshot when GHOSTAR_EDITOR_SNAPSHOT is set).

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

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
double renderBlocks(GhostarAudioProcessor& processor, int blocks,
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
    GhostarAudioProcessor processor;
    namespace ids = ghostar::parameters;
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
    GhostarAudioProcessor processor;
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
    GhostarAudioProcessor processor;
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
    namespace ids = ghostar::parameters;
    GhostarAudioProcessor saved;
    if (auto* parameter = saved.parameters.getParameter(ids::cutoff))
        parameter->setValueNotifyingHost(0.31f);
    if (auto* parameter = saved.parameters.getParameter(ids::lowerMode))
        parameter->setValueNotifyingHost(1.0f); // the last detent: High Pass
    juce::MemoryBlock state;
    saved.getStateInformation(state);

    GhostarAudioProcessor restored;
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

// Zero-crossing pitch estimate over rendered blocks, for checks that need
// to hear *which* note sounded rather than just that something did.
double renderedZeroCrossingHz(GhostarAudioProcessor& processor, int blocks)
{
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer empty;
    std::vector<float> collected;
    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer events;
        processor.processBlock(buffer, events);
        const auto* samples = buffer.getReadPointer(0);
        collected.insert(collected.end(), samples, samples + blockSize);
    }
    int crossings = 0;
    for (std::size_t index = 1; index < collected.size(); ++index)
        if ((collected[index - 1] < 0.0f) != (collected[index] < 0.0f))
            ++crossings;
    const double seconds =
        static_cast<double>(collected.size()) / sampleRate;
    return static_cast<double>(crossings) / (2.0 * seconds);
}

// MIDI CC120 (All Sound Off) must silence the instrument without resetting
// controllers: a pitch bend held across it still applies to the next note.
void testAllSoundOffKeepsTheBend()
{
    GhostarAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::MidiBuffer noteOn;
    noteOn.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
    renderBlocks(processor, 24, noteOn);
    const double unbentHz = renderedZeroCrossingHz(processor, 90);

    juce::MidiBuffer bendThenStop;
    bendThenStop.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0);
    bendThenStop.addEvent(juce::MidiMessage::controllerEvent(1, 120, 0), 8);
    renderBlocks(processor, 1, bendThenStop);
    juce::MidiBuffer quiet;
    const double stopped = renderBlocks(processor, 8, quiet);
    expect(stopped < 1.0e-4, "CC120 did not silence the sounding voice");

    renderBlocks(processor, 24, noteOn);
    const double bentHz = renderedZeroCrossingHz(processor, 90);
    expect(bentHz > unbentHz * 1.35,
           "the pitch bend did not survive CC120");
    processor.releaseResources();
}

// A mono host layout has no right channel to split onto; enabling SPLIT
// there must fold both paths into the mono output instead of silently
// discarding the whole Shaper path.
void testMonoLayoutKeepsTheShaperPath()
{
    namespace ids = ghostar::parameters;
    GhostarAudioProcessor processor;
    auto layout = processor.getBusesLayout();
    layout.outputBuses.getReference(0) = juce::AudioChannelSet::mono();
    expect(processor.setBusesLayout(layout),
           "the processor did not accept a mono output layout");

    const auto set = [&processor](const char* id, float value) {
        if (auto* parameter = processor.parameters.getParameter(id))
            parameter->setValueNotifyingHost(value);
    };
    set(ids::splitPaths, 1.0f);
    set(ids::filterPathA, 0.0f);
    set(ids::filterPathB, 0.0f);
    set(ids::filterPathNoise, 0.0f);
    set(ids::shaperPathA, 0.8f);
    set(ids::shaperMode, 1.0f / 3.0f); // KBD HOLD: contour rises while held

    processor.prepareToPlay(sampleRate, blockSize);
    juce::AudioBuffer<float> buffer(1, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
    double lastPeak = 0.0;
    double heard = 0.0;
    for (int block = 0; block < 48; ++block)
    {
        buffer.clear();
        juce::MidiBuffer events = block == 0 ? midi : juce::MidiBuffer {};
        processor.processBlock(buffer, events);
        expect(isFinite(buffer), "a mono block carried non-finite audio");
        lastPeak = peakOf(buffer);
        heard = std::max(heard, lastPeak);
    }
    expect(heard > 1.0e-4,
           "the Shaper path fell silent in a mono layout with SPLIT on");
    processor.releaseResources();
}

// A note queued from the on-screen keyboard before PANIC is clicked must
// not replay after the reset.
void testPanicDropsQueuedUiNotes()
{
    GhostarAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    processor.keyboardState.noteOn(1, 60, 1.0f);
    processor.requestPanic();
    juce::MidiBuffer quiet;
    const double after = renderBlocks(processor, 12, quiet);
    expect(after < 1.0e-4, "a note queued before panic replayed after it");
    expect(!processor.isGateOpenForDisplay(),
           "panic left the keying gate open");

    // Only what precedes the click dies: a key pressed after PANIC but
    // before the next audio callback is a fresh note and must sound.
    processor.keyboardState.noteOff(1, 60, 0.0f);
    processor.keyboardState.noteOn(1, 64, 1.0f);
    processor.requestPanic();
    processor.keyboardState.noteOn(1, 60, 1.0f);
    const double post = renderBlocks(processor, 24, quiet);
    expect(post > 1.0e-4,
           "a note pressed after panic was dropped with the stale queue");
    processor.releaseResources();
}

// The longest release decays at 3/10 per second down to the engine's 1e-5
// idle floor; the advertised tail must cover that, or hosts truncate it.
void testAdvertisedTailCoversTheLongestRelease()
{
    GhostarAudioProcessor processor;
    expect(processor.getTailLengthSeconds() >= std::log(1.0e5) / 0.3,
           "the advertised tail is shorter than the longest release");
}

// The factory bank is Init, the manual's eleven Sound Charts, and the
// seventeen Ghostar Programs. The host-facing contract: the count and names
// are published, selecting a program writes it into the host parameters,
// and a selected program actually plays.
void testFactoryProgramsAreTheSoundCharts()
{
    namespace ids = ghostar::parameters;
    GhostarAudioProcessor processor;

    expect(processor.getNumPrograms() == 29,
           "the program bank is not Init plus eleven charts plus seventeen "
           "programs");
    expect(processor.getProgramName(0) == "Init",
           "the bank does not open with the default voice");
    expect(processor.getProgramName(1) == "Preparatory Pattern",
           "the charts do not start with the Preparatory Pattern");
    expect(processor.getProgramName(3) == "Fat Filter",
           "program 3 is not the Fat Filter chart");
    expect(processor.getProgramName(12) == "Spirit Bass",
           "the performance bank does not begin where it should");

    processor.setCurrentProgram(3);
    expect(processor.getCurrentProgram() == 3,
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
    processor.setCurrentProgram(processor.getNumPrograms());
    expect(processor.getCurrentProgram() == 3,
           "an out-of-range program selection was not ignored");

    // A performance program must reach the host parameters exactly as a
    // chart does — the two banks travel the same lane.
    processor.setCurrentProgram(12);
    expect(processor.getCurrentProgram() == 12,
           "the performance program was not selected");
    auto* slope = processor.parameters.getRawParameterValue(ids::slope);
    expect(slope != nullptr && std::lround(slope->load()) == 1,
           "Spirit Bass did not select the 24 dB slope");
    processor.setCurrentProgram(3);

    // The selected program's name must survive a state round trip; the
    // values themselves already travel as parameters.
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    GhostarAudioProcessor restored;
    restored.setStateInformation(state.getData(),
                                 static_cast<int>(state.getSize()));
    expect(restored.getCurrentProgram() == 3,
           "the program index did not survive the state round trip");
    auto* restoredCutoff =
        restored.parameters.getRawParameterValue(ids::cutoff);
    expect(restoredCutoff != nullptr
               && std::abs(restoredCutoff->load() - 0.45f) < 0.002f,
           "the restored state lost the chart's cutoff travel");

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
    GhostarAudioProcessor processor;
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
        "GHOSTAR_EDITOR_SNAPSHOT", {});
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
    testAllSoundOffKeepsTheBend();
    testMonoLayoutKeepsTheShaperPath();
    testPanicDropsQueuedUiNotes();
    testAdvertisedTailCoversTheLongestRelease();
    testEditorRendering();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Ghostar plug-in check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Ghostar plug-in checks passed.\n";
    return EXIT_SUCCESS;
}
