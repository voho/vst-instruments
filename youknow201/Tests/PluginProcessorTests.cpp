// Plug-in level tests: the APVTS layout mirrors the documented parameter
// contract, MIDI reaches the engine (including the documented CC map), the
// factory programs load, state round-trips, and the editor renders — writing
// the committed screenshot when the build asks for it.

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdio>

namespace
{
int failures = 0;
int checks = 0;

void expect (bool condition, const juce::String& message)
{
    ++checks;
    if (! condition)
    {
        ++failures;
        std::fprintf (stderr, "FAIL: %s\n", message.toRawUTF8());
    }
}

juce::MidiBuffer messageAt (const juce::MidiMessage& message, int sample = 0)
{
    juce::MidiBuffer buffer;
    buffer.addEvent (message, sample);
    return buffer;
}

double bufferRms (const juce::AudioBuffer<float>& buffer)
{
    double sum = 0.0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            sum += juce::square ((double) buffer.getSample (channel, i));
    return std::sqrt (sum / (buffer.getNumChannels() * buffer.getNumSamples()));
}

bool bufferFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (channel, i)))
                return false;
    return true;
}

void testParameterLayoutAndDefaults()
{
    YouKnow201AudioProcessor processor;

    const auto& all = processor.getParameters();
    expect (all.size() >= 150,
            "the layout carries the full documented parameter contract, got "
                + juce::String (all.size()));

    juce::StringArray ids;
    for (const auto* parameter : all)
        if (const auto* withId =
                dynamic_cast<const juce::AudioProcessorParameterWithID*> (parameter))
            ids.add (withId->paramID);
    juce::StringArray sorted = ids;
    sorted.removeDuplicates (false);
    expect (sorted.size() == ids.size(), "parameter IDs are unique");

    // Spot-check documented defaults: INIT PATCH is only OSC1 (balance -63).
    expect ((int) processor.parameters.getRawParameterValue ("up_balance")->load()
                == -63,
            "INIT PATCH balance sits fully left");
    expect ((int) processor.parameters.getRawParameterValue ("up_cutoff")->load()
                == 127,
            "INIT PATCH cutoff is open");
    for (const auto* id :
         { "up_osc1_wave", "lo_osc2_pw", "up_lfo2_depth2", "lo_mono_mode",
           "delay_time", "reverb_hf_damp_gain", "keyboard_mode", "split_point",
           "master_level" })
        expect (processor.parameters.getParameter (id) != nullptr,
                juce::String ("parameter exists: ") + id);
}

void testBusLayoutAndTail()
{
    YouKnow201AudioProcessor processor;
    expect (processor.getTotalNumOutputChannels() == 2,
            "the synth is stereo out");

    // INIT PATCH has both effects off and a short release: a small tail.
    expect (processor.getTailLengthSeconds() < 2.0,
            "a dry patch reports a short tail");

    // Maximum reverb: the report must cover its RT60.
    const auto set = [&processor] (const char* id, float natural)
    {
        auto* parameter = processor.parameters.getParameter (id);
        const auto& range = processor.parameters.getParameterRange (id);
        parameter->setValueNotifyingHost (range.convertTo0to1 (natural));
    };
    set ("reverb_on", 1.0f);
    set ("reverb_time", 127.0f);
    set ("reverb_size", 7.0f);
    expect (processor.getTailLengthSeconds() >= 15.0,
            "the reported tail covers the longest reverb");

    // The documented feedback extreme is capped rather than misreported.
    set ("delay_on", 1.0f);
    set ("delay_time", 127.0f);
    set ("delay_feedback", 98.0f);
    expect (processor.getTailLengthSeconds() >= 100.0,
            "extreme delay feedback lengthens the reported tail");
}

void testRenderingAndVoices()
{
    YouKnow201AudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);

    auto midi = messageAt (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100));
    processor.processBlock (buffer, midi);
    double rms = 0.0;
    for (int block = 0; block < 40; ++block)
    {
        juce::MidiBuffer empty;
        processor.processBlock (buffer, empty);
        rms = std::max (rms, bufferRms (buffer));
        expect (bufferFinite (buffer), "rendered audio is finite");
    }
    expect (rms > 1.0e-4, "a note produces audio");
    expect (processor.getActiveVoiceCount() == 1, "one key, one voice");

    auto off = messageAt (juce::MidiMessage::noteOff (1, 69));
    processor.processBlock (buffer, off);
    for (int block = 0; block < 200; ++block)
    {
        juce::MidiBuffer empty;
        processor.processBlock (buffer, empty);
    }
    expect (processor.getActiveVoiceCount() == 0,
            "the released voice frees after its envelope");
}

void testDocumentedControlChanges()
{
    YouKnow201AudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);

    // UPPER cutoff is CC#74, LOWER cutoff CC#102 (OM p.72).
    auto upper = messageAt (juce::MidiMessage::controllerEvent (1, 74, 40));
    processor.processBlock (buffer, upper);
    expect ((int) processor.parameters.getRawParameterValue ("up_cutoff")->load()
                == 40,
            "CC#74 edits the UPPER cutoff");

    auto lower = messageAt (juce::MidiMessage::controllerEvent (1, 102, 90));
    processor.processBlock (buffer, lower);
    expect ((int) processor.parameters.getRawParameterValue ("lo_cutoff")->load()
                == 90,
            "CC#102 edits the LOWER cutoff");

    // Signed display: CC#8 balance 64 = center 0.
    auto balance = messageAt (juce::MidiMessage::controllerEvent (1, 8, 64));
    processor.processBlock (buffer, balance);
    expect ((int) processor.parameters.getRawParameterValue ("up_balance")->load()
                == 0,
            "CC#8 at 64 centers the balance");

    // The CC#88 collision resolves to LOWER OSC2 pitch-env depth (OQ-02).
    auto collision = messageAt (juce::MidiMessage::controllerEvent (1, 88, 127));
    processor.processBlock (buffer, collision);
    expect ((int) processor.parameters
                    .getRawParameterValue ("lo_osc2_penv_depth")
                    ->load()
                == 63,
            "CC#88 edits LOWER OSC2 pitch-env depth");
    // ...and UPPER filter-env decay answers on CC#83 instead.
    auto decay = messageAt (juce::MidiMessage::controllerEvent (1, 83, 25));
    processor.processBlock (buffer, decay);
    expect ((int) processor.parameters.getRawParameterValue ("up_fenv_decay")->load()
                == 25,
            "CC#83 edits UPPER filter-env decay");
}

void testProgramsLoad()
{
    YouKnow201AudioProcessor processor;
    expect (processor.getNumPrograms() >= 12, "the factory bank is exposed");
    expect (processor.getProgramName (0) == "INIT PATCH",
            "program 0 is INIT PATCH");

    int superSawProgram = -1;
    for (int index = 0; index < processor.getNumPrograms(); ++index)
        if (processor.getProgramName (index) == "SuperLead201")
            superSawProgram = index;
    expect (superSawProgram > 0, "the supersaw lead ships in the bank");

    processor.setCurrentProgram (superSawProgram);
    expect (processor.getCurrentProgram() == superSawProgram,
            "the program index sticks");
    const int wave = (int) processor.parameters
                         .getRawParameterValue ("up_osc1_wave")
                         ->load();
    expect (wave == 7, "loading the lead selects SUPER-SAW (enum 7)");

    const auto patch = processor.snapshotPatch();
    expect (patch.upper.osc1.wave == youknow201::Waveform::SuperSaw,
            "the engine snapshot agrees with the loaded program");
}

void testStateRoundTrip()
{
    YouKnow201AudioProcessor processor;
    processor.parameters.getParameter ("up_cutoff")
        ->setValueNotifyingHost (
            processor.parameters.getParameterRange ("up_cutoff")
                .convertTo0to1 (55.0f));
    processor.parameters.getParameter ("lo_resonance")
        ->setValueNotifyingHost (
            processor.parameters.getParameterRange ("lo_resonance")
                .convertTo0to1 (111.0f));

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    YouKnow201AudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    expect ((int) restored.parameters.getRawParameterValue ("up_cutoff")->load()
                == 55,
            "cutoff survives the state round trip");
    expect ((int) restored.parameters.getRawParameterValue ("lo_resonance")->load()
                == 111,
            "resonance survives the state round trip");
}

void testEditorAndSnapshot()
{
    YouKnow201AudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "the processor provides an editor");
    if (editor == nullptr)
        return;

    editor->setSize (editor->getWidth(), editor->getHeight());
    expect (editor->getWidth() >= 1000 && editor->getHeight() >= 500,
            "the editor opens at its panel size");

    juce::Image rendered { juce::Image::ARGB, editor->getWidth(),
                           editor->getHeight(), true };
    {
        juce::Graphics graphics { rendered };
        editor->paintEntireComponent (graphics, true);
    }
    expect (rendered.getPixelAt (10, 10).getAlpha() == 255,
            "the editor paints an opaque panel");

    // The nightly workflow asks for the committed screenshot through this
    // variable, so the images in the documentation track the real editor.
    const auto snapshotPath =
        juce::SystemStats::getEnvironmentVariable ("YOUKNOW201_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
    {
        const juce::File snapshotFile { snapshotPath };
        snapshotFile.getParentDirectory().createDirectory();

        juce::FileOutputStream output { snapshotFile };
        juce::PNGImageFormat png;
        const bool preparedOutput =
            output.openedOk() && output.setPosition (0) && output.truncate().wasOk();
        const bool wroteSnapshot =
            preparedOutput && png.writeImageToStream (rendered, output);
        output.flush();
        expect (wroteSnapshot, "could not write the requested editor snapshot");
    }

    editor.reset();
    processor.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;

    testParameterLayoutAndDefaults();
    testBusLayoutAndTail();
    testRenderingAndVoices();
    testDocumentedControlChanges();
    testProgramsLoad();
    testStateRoundTrip();
    testEditorAndSnapshot();

    if (failures == 0)
    {
        std::printf ("YouKnow201 plug-in tests passed (%d checks).\n", checks);
        return 0;
    }
    std::fprintf (stderr, "%d of %d plug-in checks failed.\n", failures, checks);
    return 1;
}
