#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>
#include <string>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
int failureCount = 0;

struct ParameterExpectation
{
    const char* id;
    float defaultValue;
    float tolerance;
};

constexpr std::array<ParameterExpectation, 40> expectedParameters {{
    { mars::parameters::osc1Wave,         0.0f,   1.0e-5f },
    { mars::parameters::osc1Octave,       0.0f,   1.0e-5f },
    { mars::parameters::osc2Wave,         1.0f,   1.0e-5f },
    { mars::parameters::osc2Octave,       0.0f,   1.0e-5f },
    { mars::parameters::osc2Tune,         0.0f,   1.0e-5f },
    { mars::parameters::osc2Fine,         0.0f,   1.0e-5f },
    { mars::parameters::oscMix,           0.46f,  1.0e-5f },
    { mars::parameters::pulseWidth,       0.50f,  1.0e-5f },
    { mars::parameters::subLevel,         0.16f,  1.0e-5f },
    { mars::parameters::noiseLevel,       0.025f, 1.0e-5f },
    { mars::parameters::crossMod,         0.06f,  1.0e-5f },
    { mars::parameters::filterModel,      0.0f,   1.0e-5f },
    { mars::parameters::cutoff,        4200.0f,   0.1f },
    { mars::parameters::resonance,        0.28f,  1.0e-5f },
    { mars::parameters::filterDrive,      0.22f,  1.0e-5f },
    { mars::parameters::filterShape,      0.35f,  1.0e-5f },
    { mars::parameters::filterEnvAmount,  0.46f,  1.0e-5f },
    { mars::parameters::keyTrack,         0.50f,  1.0e-5f },
    { mars::parameters::fAttack,          0.012f, 1.0e-5f },
    { mars::parameters::fDecay,           0.45f,  1.0e-5f },
    { mars::parameters::fSustain,         0.34f,  1.0e-5f },
    { mars::parameters::fRelease,         0.62f,  1.0e-5f },
    { mars::parameters::aAttack,          0.008f, 1.0e-5f },
    { mars::parameters::aDecay,           0.38f,  1.0e-5f },
    { mars::parameters::aSustain,         0.78f,  1.0e-5f },
    { mars::parameters::aRelease,         0.55f,  1.0e-5f },
    { mars::parameters::lfoWave,          0.0f,   1.0e-5f },
    { mars::parameters::lfoRate,          4.8f,   1.0e-5f },
    { mars::parameters::lfoPitch,         8.0f,   1.0e-5f },
    { mars::parameters::lfoFilter,        0.18f,  1.0e-5f },
    { mars::parameters::lfoPwm,           0.16f,  1.0e-5f },
    { mars::parameters::voiceMode,        0.0f,   1.0e-5f },
    { mars::parameters::unisonVoices,     4.0f,   1.0e-5f },
    { mars::parameters::drift,            0.28f,  1.0e-5f },
    { mars::parameters::spread,           0.58f,  1.0e-5f },
    { mars::parameters::glide,            0.0f,   1.0e-5f },
    { mars::parameters::velocity,         0.68f,  1.0e-5f },
    { mars::parameters::chorusMix,        0.30f,  1.0e-5f },
    { mars::parameters::chorusRate,       0.38f,  1.0e-5f },
    { mars::parameters::output,          -6.0f,   1.0e-5f },
}};

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool approximatelyEqual (float actual, float expected, float tolerance = 1.0e-5f)
{
    return std::abs (actual - expected) <= tolerance;
}

float parameterValue (const MarsAudioProcessor& processor, const char* id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, std::string ("missing parameter ") + id);
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (MarsAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot set missing parameter ") + id);
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void expectParameterText (MarsAudioProcessor& processor, const char* id, float value,
                          const char* expectedText)
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

float peakInRange (const juce::AudioBuffer<float>& buffer, int start, int end)
{
    const auto first = juce::jlimit (0, buffer.getNumSamples(), start);
    const auto last = juce::jlimit (first, buffer.getNumSamples(), end);
    float peak = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = first; sample < last; ++sample)
            peak = std::max (peak, std::abs (buffer.getSample (channel, sample)));

    return peak;
}

void renderBlock (MarsAudioProcessor& processor, juce::AudioBuffer<float>& audio,
                  juce::MidiBuffer& midi)
{
    audio.setSize (2, blockSize, false, false, true);
    audio.clear();
    processor.processBlock (audio, midi);
}

bool renderUntilVoicesStop (MarsAudioProcessor& processor,
                            juce::AudioBuffer<float>& audio, int maximumBlocks = 256)
{
    juce::MidiBuffer emptyMidi;
    for (int block = 0; block < maximumBlocks && processor.getActiveVoiceCount() > 0; ++block)
        renderBlock (processor, audio, emptyMidi);
    return processor.getActiveVoiceCount() == 0;
}

void useShortReleases (MarsAudioProcessor& processor)
{
    setParameterValue (processor, mars::parameters::fRelease, 0.010f);
    setParameterValue (processor, mars::parameters::aRelease, 0.010f);
}

void testParameterLayoutAndDefaults()
{
    MarsAudioProcessor processor;
    expect (expectedParameters.size() == 40u,
            "test parameter manifest does not contain exactly 40 entries");
    expect (processor.getParameters().size() == static_cast<int> (expectedParameters.size()),
            "processor does not expose exactly 40 APVTS parameters");

    std::set<std::string> uniqueIds;
    for (const auto& expected : expectedParameters)
    {
        expect (uniqueIds.insert (expected.id).second,
                std::string ("duplicate APVTS id ") + expected.id);

        const auto* parameter = processor.parameters.getParameter (expected.id);
        expect (parameter != nullptr,
                std::string ("APVTS does not contain ") + expected.id);
        if (parameter == nullptr)
            continue;

        const auto declaredDefault = parameter->convertFrom0to1 (parameter->getDefaultValue());
        expect (approximatelyEqual (declaredDefault, expected.defaultValue, expected.tolerance),
                std::string ("wrong declared default for ") + expected.id);
        expect (approximatelyEqual (parameterValue (processor, expected.id),
                                    expected.defaultValue, expected.tolerance),
                std::string ("wrong initial value for ") + expected.id);
    }

    expect (uniqueIds.size() == expectedParameters.size(),
            "the 40-entry APVTS manifest contains duplicate ids");
}

void testParameterTextFormatting()
{
    MarsAudioProcessor processor;
    expectParameterText (processor, mars::parameters::oscMix, 0.46f, "46%");
    expectParameterText (processor, mars::parameters::filterEnvAmount, 0.46f, "+46%");
    expectParameterText (processor, mars::parameters::osc1Octave, 1.0f, "+1oct");
    expectParameterText (processor, mars::parameters::osc2Tune, -7.0f, "-7st");
    expectParameterText (processor, mars::parameters::osc2Fine, 12.3f, "+12.3ct");
    expectParameterText (processor, mars::parameters::lfoFilter, 0.18f, "0.180oct");
    expectParameterText (processor, mars::parameters::output, -6.0f, "-6.0dB");
}

void testStateRoundTrip()
{
    MarsAudioProcessor source;
    setParameterValue (source, mars::parameters::osc2Wave, 2.0f);
    setParameterValue (source, mars::parameters::osc2Fine, -17.3f);
    setParameterValue (source, mars::parameters::cutoff, 777.0f);
    setParameterValue (source, mars::parameters::filterEnvAmount, -0.37f);
    setParameterValue (source, mars::parameters::voiceMode, 2.0f);
    setParameterValue (source, mars::parameters::output, 2.3f);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    expect (state.getSize() > 0, "processor produced empty state");

    MarsAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc2Wave), 2.0f),
            "VCO II waveform did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc2Fine),
                                -17.3f, 0.11f),
            "VCO II fine tune did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::cutoff),
                                777.0f, 0.5f),
            "filter cutoff did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::filterEnvAmount),
                                -0.37f, 0.002f),
            "bipolar filter envelope amount did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::voiceMode), 2.0f),
            "voice mode did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::output),
                                2.3f, 0.051f),
            "output level did not survive state round-trip");
}

void testBusAndPluginContract()
{
    MarsAudioProcessor processor;
    expect (processor.getBusCount (true) == 0, "instrument unexpectedly exposes an input bus");
    expect (processor.getBusCount (false) == 1, "instrument does not expose one output bus");
    expect (processor.getTotalNumInputChannels() == 0,
            "instrument unexpectedly exposes input channels");
    expect (processor.getTotalNumOutputChannels() == 2,
            "instrument main output is not stereo");
    expect (processor.acceptsMidi(), "instrument does not accept MIDI");
    expect (! processor.producesMidi(), "instrument unexpectedly produces MIDI");
    expect (! processor.isMidiEffect(), "instrument identifies as a MIDI effect");
    expect (processor.hasEditor(), "instrument does not advertise its editor");

    const auto stereoLayout = processor.getBusesLayout();
    expect (processor.isBusesLayoutSupported (stereoLayout),
            "default no-input/stereo-output layout is unsupported");

    auto monoLayout = stereoLayout;
    monoLayout.outputBuses.set (0, juce::AudioChannelSet::mono());
    expect (! processor.isBusesLayoutSupported (monoLayout),
            "unsupported mono instrument output was accepted");

    auto inputLayout = stereoLayout;
    inputLayout.inputBuses.add (juce::AudioChannelSet::stereo());
    expect (! processor.isBusesLayoutSupported (inputLayout),
            "unsupported instrument input bus was accepted");
}

void testSampleAccurateNoteOn()
{
    MarsAudioProcessor processor;
    useShortReleases (processor);
    processor.prepareToPlay (sampleRate, blockSize);

    constexpr int eventSample = 173;
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (112)),
                   eventSample);
    renderBlock (processor, audio, midi);

    expect (peakInRange (audio, 0, eventSample) == 0.0f,
            "MIDI note rendered before its sample offset");
    expect (peakInRange (audio, eventSample, blockSize) > 1.0e-7f,
            "MIDI note produced no audio after its sample offset");
    expect (processor.getActiveVoiceCount() > 0,
            "sample-accurate note-on did not create an active voice");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    renderBlock (processor, audio, midi);
    expect (renderUntilVoicesStop (processor, audio),
            "sample-accurate MIDI voice did not finish after note-off");
    processor.releaseResources();
}

void testMidiControllersAndVoiceLifecycle()
{
    MarsAudioProcessor processor;
    useShortReleases (processor);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (118)), 0);
    midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 64);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 96);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 128);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 160);
    renderBlock (processor, audio, midi);

    expect (peakInRange (audio, 0, blockSize) > 1.0e-7f,
            "pitch-wheel/CC1/CC64 sequence produced no audio");
    expect (processor.getActiveVoiceCount() > 0,
            "sustain pedal did not retain the released voice");

    midi.clear();
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() > 0,
            "sustained voice ended before pedal release");

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    midi.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 32);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 0), 64);
    renderBlock (processor, audio, midi);
    expect (renderUntilVoicesStop (processor, audio),
            "voice did not finish after sustain-pedal release");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, static_cast<juce::uint8> (110)), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 64);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() > 0,
            "all-notes-off setup did not create a voice");

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 128);
    renderBlock (processor, audio, midi);
    expect (peakInRange (audio, 0, 128) > 1.0e-7f,
            "all-notes-off block did not render the leading voice");
    expect (processor.getActiveVoiceCount() > 0,
            "MIDI CC123 ignored the active sustain pedal");

    midi.clear();
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() > 0,
            "CC123-held voice ended before sustain-pedal release");

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    renderBlock (processor, audio, midi);
    expect (renderUntilVoicesStop (processor, audio),
            "MIDI CC123 voices did not finish after sustain-pedal release");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, static_cast<juce::uint8> (110)), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() > 0,
            "all-sound-off setup did not create a voice");

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 120, 0), 128);
    renderBlock (processor, audio, midi);
    expect (peakInRange (audio, 0, 128) > 1.0e-7f,
            "all-sound-off block did not render the leading voice");
    expect (processor.getActiveVoiceCount() == 0,
            "MIDI CC120 did not silence all active voices immediately");

    processor.releaseResources();
}

void testOutputGainImpact()
{
    MarsAudioProcessor quiet;
    MarsAudioProcessor loud;
    setParameterValue (quiet, mars::parameters::noiseLevel, 0.0f);
    setParameterValue (loud, mars::parameters::noiseLevel, 0.0f);
    setParameterValue (quiet, mars::parameters::drift, 0.0f);
    setParameterValue (loud, mars::parameters::drift, 0.0f);
    setParameterValue (quiet, mars::parameters::chorusMix, 0.0f);
    setParameterValue (loud, mars::parameters::chorusMix, 0.0f);
    setParameterValue (quiet, mars::parameters::output, -24.0f);
    setParameterValue (loud, mars::parameters::output, 6.0f);
    quiet.prepareToPlay (sampleRate, blockSize);
    loud.prepareToPlay (sampleRate, blockSize);

    const auto note = juce::MidiMessage::noteOn (
        1, 60, static_cast<juce::uint8> (112));
    juce::AudioBuffer<float> quietAudio;
    juce::AudioBuffer<float> loudAudio;
    juce::MidiBuffer quietMidi;
    juce::MidiBuffer loudMidi;
    quietMidi.addEvent (note, 0);
    loudMidi.addEvent (note, 0);
    renderBlock (quiet, quietAudio, quietMidi);
    renderBlock (loud, loudAudio, loudMidi);

    const auto quietPeak = peakInRange (quietAudio, 0, blockSize);
    const auto loudPeak = peakInRange (loudAudio, 0, blockSize);
    expect (quietPeak > 0.0f, "quiet output-gain render produced no signal");
    expect (loudPeak > quietPeak * 10.0f,
            "output level did not materially affect the first rendered block");

    quiet.releaseResources();
    loud.releaseResources();
}

void testPrepareReleaseUiQueueAndPanic()
{
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    MarsAudioProcessor offline;
    offline.keyboardState.noteOn (1, 48, 0.9f);
    renderBlock (offline, audio, midi);
    expect (peakInRange (audio, 0, blockSize) == 0.0f,
            "unprepared processor emitted audio");
    expect (offline.getActiveVoiceCount() == 0,
            "unprepared processor accepted a UI note");

    offline.prepareToPlay (sampleRate, blockSize);
    expect (offline.isEngineReady(), "processor did not become ready after prepare");
    expect (std::abs (offline.getCurrentSampleRateForDisplay() - sampleRate) < 0.01,
            "prepare did not update the display sample rate");
    midi.clear();
    renderBlock (offline, audio, midi);
    expect (offline.getActiveVoiceCount() == 0,
            "UI note queued while offline leaked into the next prepare");
    expect (peakInRange (audio, 0, blockSize) == 0.0f,
            "offline UI queue emitted audio after prepare");
    offline.releaseResources();
    expect (! offline.isEngineReady(), "processor remained ready after releaseResources");
    expect (offline.getCurrentSampleRateForDisplay() == 0.0,
            "releaseResources did not clear the display sample rate");

    MarsAudioProcessor active;
    useShortReleases (active);
    active.prepareToPlay (sampleRate, blockSize);
    active.keyboardState.noteOn (1, 64, 0.9f);
    midi.clear();
    renderBlock (active, audio, midi);
    expect (active.getActiveVoiceCount() > 0,
            "prepared UI note was not dispatched at the next block boundary");
    expect (peakInRange (audio, 0, blockSize) > 1.0e-7f,
            "prepared UI note produced no audio");

    active.keyboardState.noteOn (1, 67, 0.9f);
    active.requestPanic();
    midi.clear();
    renderBlock (active, audio, midi);
    expect (active.getActiveVoiceCount() == 0,
            "panic did not stop voices and discard the stale UI queue");

    active.releaseResources();
    expect (! active.isEngineReady(), "UI processor remained ready after release");
    active.keyboardState.noteOn (1, 72, 1.0f);
    midi.clear();
    renderBlock (active, audio, midi);
    expect (active.getActiveVoiceCount() == 0,
            "UI note was accepted after releaseResources");
    expect (peakInRange (audio, 0, blockSize) == 0.0f,
            "released processor emitted audio");

    active.prepareToPlay (sampleRate, blockSize);
    midi.clear();
    renderBlock (active, audio, midi);
    expect (active.getActiveVoiceCount() == 0,
            "post-release UI queue leaked into a later prepare");
    active.releaseResources();
}

struct SnapshotStats
{
    std::size_t sampledColours = 0;
    int sampledPixels = 0;
    int opaquePixels = 0;
};

SnapshotStats inspectSnapshot (const juce::Image& snapshot)
{
    std::set<juce::uint32> colours;
    int sampledPixels = 0;
    int opaquePixels = 0;

    for (int y = 2; y < snapshot.getHeight(); y += 4)
    {
        for (int x = 2; x < snapshot.getWidth(); x += 4)
        {
            const auto pixel = snapshot.getPixelAt (x, y);
            colours.insert (pixel.getARGB());
            opaquePixels += pixel.getAlpha() == 255 ? 1 : 0;
            ++sampledPixels;
        }
    }

    return { colours.size(), sampledPixels, opaquePixels };
}

juce::Image renderEditorSnapshot (juce::AudioProcessorEditor& editor)
{
    juce::Image snapshot (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
    juce::Graphics graphics (snapshot);
    editor.paintEntireComponent (graphics, true);
    return snapshot;
}

void expectDetailedOpaqueSnapshot (const juce::Image& snapshot, const std::string& sizeName)
{
    expect (snapshot.isValid(), sizeName + " editor snapshot is invalid");
    if (! snapshot.isValid())
        return;

    const auto stats = inspectSnapshot (snapshot);
    expect (stats.sampledPixels > 0 && stats.opaquePixels == stats.sampledPixels,
            sizeName + " editor snapshot contains transparent sampled pixels");
    expect (stats.sampledColours > 512u,
            sizeName + " editor snapshot lacks visual detail ("
                + std::to_string (stats.sampledColours) + " sampled colours)");
}

void testEditorRenderingAtDefaultAndMinimumSize()
{
    MarsAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "processor did not create an editor");
    if (editor == nullptr)
        return;

    expect (editor->isOpaque(), "editor does not advertise an opaque surface");
    expect (editor->getWidth() == 1280 && editor->getHeight() == 840,
            "editor did not open at its 1280x840 default size");
    const auto defaultSnapshot = renderEditorSnapshot (*editor);
    expectDetailedOpaqueSnapshot (defaultSnapshot, "default-size");

    editor->setSize (1180, 820);
    editor->resized();
    expect (editor->getWidth() == 1180 && editor->getHeight() == 820,
            "editor did not honour its 1180x820 minimum size");
    const auto minimumSnapshot = renderEditorSnapshot (*editor);
    expectDetailedOpaqueSnapshot (minimumSnapshot, "minimum-size");

    const auto snapshotPath = juce::SystemStats::getEnvironmentVariable (
        "MARS_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
    {
        juce::FileOutputStream output { juce::File (snapshotPath) };
        juce::PNGImageFormat png;
        const bool preparedOutput = output.openedOk()
            && output.setPosition (0)
            && output.truncate();
        const bool wroteSnapshot = preparedOutput
            && png.writeImageToStream (minimumSnapshot, output);
        output.flush();
        expect (wroteSnapshot, "could not write requested editor snapshot");
    }

    editor.reset();
    processor.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    testParameterLayoutAndDefaults();
    testParameterTextFormatting();
    testStateRoundTrip();
    testBusAndPluginContract();
    testSampleAccurateNoteOn();
    testMidiControllersAndVoiceLifecycle();
    testOutputGainImpact();
    testPrepareReleaseUiQueueAndPanic();
    testEditorRenderingAtDefaultAndMinimumSize();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Mars processor contract test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Mars processor contract tests passed\n";
    return EXIT_SUCCESS;
}
