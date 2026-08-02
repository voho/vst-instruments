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
constexpr auto expectedParameters = std::to_array<ParameterExpectation> ({
    { parameters::volume,      0.80f,  1.0e-5f },
    { parameters::benderDco,   0.30f,  1.0e-5f },
    { parameters::benderVcf,   0.0f,   1.0e-5f },
    { parameters::benderLfo,   0.0f,   1.0e-5f },
    { parameters::portamento,  0.0f,   1.0e-5f },
    { parameters::legacyKeyMode, 0.0f, 1.0e-5f },
    { parameters::legacyChorus,  0.0f, 1.0e-5f },
    { parameters::poly1,       1.0f,   1.0e-5f },
    { parameters::poly2,       0.0f,   1.0e-5f },
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
    { parameters::chorusI,     0.0f,   1.0e-5f },
    { parameters::chorusII,    0.0f,   1.0e-5f },
    { parameters::transpose,   0.0f,   1.0e-5f },
    { parameters::masterTune,  0.0f,   1.0e-5f },
    { parameters::velocity,    0.0f,   1.0e-5f },
    { parameters::calibration, 0.35f,  1.0e-5f },
    { parameters::chorusNoise, 1.0f,   1.0e-5f },
    { parameters::polyphony,   6.0f,   1.0e-5f },
    { parameters::hq,          1.0f,   1.0e-5f },
});

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

void testUiKeyboardPressAndReleaseIsHeard()
{
    // The on-screen keyboard's events carry no sample position, so they are
    // applied at the block boundary. A press and its release arriving in the
    // same drain -- which is what happens after the audio thread has been held
    // up -- would then cancel before anything was rendered.
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::attack, 0.0f);
    setParameterValue (processor, parameters::sustain, 1.0f);
    setParameterValue (processor, parameters::release, 0.0f);

    processor.keyboardState.noteOn (1, 60, 1.0f);
    processor.keyboardState.noteOff (1, 60, 0.0f);

    float peak = 0.0f;
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < 4; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        peak = std::max (peak, bufferPeak (buffer));
    }

    expect (peak > 0.0f,
            "a keyboard press and release in one drain produced no audio at all");
    processor.releaseResources();
}

void testDeferredQualitySwitchIsNotAutomatable()
{
    // The engine holds a quality change until the output path is quiet, so an
    // automation point would not take effect where it was written.
    YouKnow106AudioProcessor processor;
    const auto* parameter = processor.parameters.getParameter (parameters::hq);
    expect (parameter != nullptr, "the quality switch is missing");
    if (parameter != nullptr)
        expect (! parameter->isAutomatable(),
                "the deferred quality switch is offered to the host as automatable");
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
    setParameterValue (source, parameters::chorusII, 1.0f);
    setParameterValue (source, parameters::range, 0.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    YouKnow106AudioProcessor destination;
    destination.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (std::abs (parameterValue (destination, parameters::cutoff) - 0.31f) < 1.0e-4f,
            "cutoff did not survive a state round trip");
    expect (std::abs (parameterValue (destination, parameters::resonance) - 0.77f) < 1.0e-4f,
            "resonance did not survive a state round trip");
    expect (std::abs (parameterValue (destination, parameters::chorusII) - 1.0f) < 1.0e-4f,
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
    expect (processor.acceptsMidi() && processor.producesMidi(),
            "the plug-in does not advertise itself as an instrument");
}

// The JUCE-free suite checks legends fit using a deliberately conservative
// width model, because it has to run on a machine with no fonts. This is the
// check that makes relying on that model safe: it asks the real typeface, at
// the real size, whether each legend actually draws inside its box. If the
// model ever drifts optimistic, this fails on the macOS runner.
// A patch dump has to reach the parameters, and the parameters have to come
// back out as a message the hardware would accept. The DSP suite proves the
// byte layout; this proves the plug-in is actually wired to it.
void testSysExPatchRoundTripsThroughTheParameters()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    sysex::Patch sent {};
    sent.cutoff = 0.25f;
    sent.resonance = 0.75f;
    sent.attack = 0.5f;
    sent.range = DcoRange::Four;
    sent.saw = false;
    sent.pulse = true;
    sent.vcaMode = VcaMode::Gate;
    sent.highPass = HighPassMode::Boost;
    sent.chorus = ChorusMode::One;

    std::array<std::uint8_t, sysex::patchMessageBytes> raw {};
    const auto written = sysex::writePatchMessage (sent, 0, raw.data(), raw.size());
    expect (written > 0, "could not build a patch message");

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::createSysExMessage (
                       raw.data() + 1, static_cast<int> (written) - 2),
                   0);
    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();
    processor.processBlock (buffer, midi);

    // The patch is staged for the message thread; drain it here so the test is
    // deterministic rather than dependent on a message loop it does not run.
    processor.flushPendingSysEx();

    expect (std::abs (parameterValue (processor, parameters::cutoff) - 0.25f) < 0.01f,
            "a patch dump did not reach the cutoff parameter");
    expect (std::abs (parameterValue (processor, parameters::resonance) - 0.75f) < 0.01f,
            "a patch dump did not reach the resonance parameter");
    expect (parameterValue (processor, parameters::saw) < 0.5f,
            "a patch dump did not clear the saw switch");
    expect (parameterValue (processor, parameters::pulse) > 0.5f,
            "a patch dump did not set the pulse switch");
    expect (parameterValue (processor, parameters::chorusI) > 0.5f,
            "a patch dump did not engage chorus I");
    expect (parameterValue (processor, parameters::chorusII) < 0.5f,
            "a patch dump engaged chorus II as well as I");

    // Straight back out again.
    const auto emitted = processor.currentPatchAsSysEx (0);
    expect (emitted.isSysEx(), "the current patch did not come back out as sysex");
    sysex::Patch returned {};
    int channel = -1;
    expect (sysex::readPatchMessage (emitted.getRawData(),
                                     static_cast<std::size_t> (emitted.getRawDataSize()),
                                     returned, channel),
            "the emitted message was not readable");
    expect (returned.range == DcoRange::Four, "the range did not survive the trip out");
    expect (returned.vcaMode == VcaMode::Gate, "the VCA mode did not survive");
    expect (returned.highPass == HighPassMode::Boost, "the high-pass did not survive");
    expect (returned.chorus == ChorusMode::One, "the chorus mode did not survive");

    processor.releaseResources();
}

// A malformed or foreign message must be ignored rather than half-applied.
// A single-parameter message from the hardware moves that control and nothing
// else -- including after an edit made from somewhere else in between. An
// earlier version cached the panel on the audio thread and staged the whole
// snapshot, which quantised every untouched control to seven bits and reverted
// anything moved since the cache was taken.
void testSingleParameterSysExDoesNotDisturbAnythingElse()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto pump = [&] (juce::MidiBuffer& midi)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        processor.flushPendingSysEx();
    };

    // Deliberately off a 7-bit step, so any round trip through the tone format
    // shows up as a changed value.
    constexpr float awkward = 0.5001f;
    setParameterValue (processor, parameters::decay, awkward);
    setParameterValue (processor, parameters::chorusI, 1.0f);
    setParameterValue (processor, parameters::chorusII, 1.0f);

    // One control moves on the hardware.
    std::array<std::uint8_t, sysex::parameterMessageBytes> first {};
    auto written = sysex::writeParameterMessage (
        static_cast<int> (sysex::ToneParameter::VcfFreq), 100, 0, first.data(),
        first.size());
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::createSysExMessage (
                       first.data() + 1, static_cast<int> (written) - 2), 0);
    pump (midi);

    expect (std::abs (parameterValue (processor, parameters::cutoff) - 100.0f / 127.0f)
                < 0.01f,
            "a single-parameter message did not reach its control");
    expect (std::abs (parameterValue (processor, parameters::decay) - awkward) < 1.0e-4f,
            "a single-parameter message quantised an untouched control");
    expect (parameterValue (processor, parameters::chorusI) > 0.5f
                && parameterValue (processor, parameters::chorusII) > 0.5f,
            "a single-parameter message collapsed the I+II chorus setting");

    // Now edit something else from the UI side, then send another hardware
    // update. The second message must not resurrect the panel as it stood when
    // the first one arrived.
    setParameterValue (processor, parameters::release, 0.8123f);
    std::array<std::uint8_t, sysex::parameterMessageBytes> second {};
    written = sysex::writeParameterMessage (
        static_cast<int> (sysex::ToneParameter::VcfRes), 20, 0, second.data(),
        second.size());
    juce::MidiBuffer later;
    later.addEvent (juce::MidiMessage::createSysExMessage (
                        second.data() + 1, static_cast<int> (written) - 2), 0);
    pump (later);

    expect (std::abs (parameterValue (processor, parameters::resonance) - 20.0f / 127.0f)
                < 0.01f,
            "the second single-parameter message did not reach its control");
    expect (std::abs (parameterValue (processor, parameters::release) - 0.8123f) < 1.0e-4f,
            "a single-parameter message reverted an edit made after the previous one");
    expect (std::abs (parameterValue (processor, parameters::cutoff) - 100.0f / 127.0f)
                < 0.01f,
            "the second message undid the first");

    processor.releaseResources();
}

// A whole bank arriving in one buffer must land intact rather than overflowing
// the handoff queue.
void testAWholeBankTransferIsNotDropped()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::MidiBuffer midi;
    const auto& bank = presets::factoryBank();
    for (std::size_t index = 0; index < 64; ++index)
    {
        const auto& patch = bank[index % bank.size()].patch;
        std::array<std::uint8_t, sysex::patchMessageBytes> raw {};
        const auto written = sysex::writePatchMessage (patch, 0, raw.data(), raw.size());
        midi.addEvent (juce::MidiMessage::createSysExMessage (
                           raw.data() + 1, static_cast<int> (written) - 2),
                       static_cast<int> (index) % blockSize);
    }

    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();
    processor.processBlock (buffer, midi);
    processor.flushPendingSysEx();

    expect (processor.getSysExDroppedCount() == 0,
            "a whole bank delivered in one buffer overflowed the handoff queue");

    // And the panel has to end on the last dump, not on an intermediate one.
    const auto& last = bank[63 % bank.size()].patch;
    expect (std::abs (parameterValue (processor, parameters::cutoff) - last.cutoff)
                < 0.02f,
            "the panel did not end on the final dump of the transfer");

    processor.releaseResources();
}

// The write half of the compatibility claim. A dump that only a subclass
// method could produce is not reachable by a DAW, so it has to leave through
// the plug-in's MIDI output.
void testRequestedDumpLeavesThroughTheMidiOutput()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::cutoff, 0.3125f);
    setParameterValue (processor, parameters::chorusII, 1.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    // Nothing asked for, nothing sent: this is a synth, not a MIDI thru.
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    buffer.clear();
    processor.processBlock (buffer, midi);
    expect (midi.isEmpty(), "the processor echoed its input to the MIDI output");

    processor.requestSysExDump();
    midi.clear();
    buffer.clear();
    processor.processBlock (buffer, midi);

    juce::MidiMessage found;
    int count = 0;
    for (const auto metadata : midi)
        if (metadata.getMessage().isSysEx())
        {
            found = metadata.getMessage();
            ++count;
        }
    expect (count == 1, "a requested dump did not appear exactly once on the output");

    sysex::Patch sent {};
    int channel = -1;
    expect (sysex::readPatchMessage (found.getRawData(),
                                     static_cast<std::size_t> (found.getRawDataSize()),
                                     sent, channel),
            "the emitted dump is not a readable patch message");
    expect (std::abs (sent.cutoff - 0.3125f) < 0.01f,
            "the emitted dump does not carry the current panel");
    expect (sent.chorus == ChorusMode::Two, "the emitted dump lost the chorus mode");

    expect (channel == 0, "a dump with no device seen did not default to channel 1");

    // The request is one-shot: it must not keep sending every block.
    midi.clear();
    buffer.clear();
    processor.processBlock (buffer, midi);
    expect (midi.isEmpty(), "the dump request repeated on the following block");

    processor.releaseResources();
}

// A saved program index has to come back, or the host's selector and the sound
// disagree after a reload.
void testSelectedProgramSurvivesAStateRoundTrip()
{
    YouKnow106AudioProcessor source;
    source.setCurrentProgram (3);
    juce::MemoryBlock state;
    source.getStateInformation (state);

    YouKnow106AudioProcessor destination;
    destination.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (destination.getCurrentProgram() == 3,
            "the selected program did not survive a state round trip");
}

void testForeignSysExLeavesThePatchAlone()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    const auto before = parameterValue (processor, parameters::cutoff);

    const std::array<std::uint8_t, 6> body { 0x43, 0x30, 0x00, 0x01, 0x02, 0x03 };
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::createSysExMessage (body.data(),
                                                          static_cast<int> (body.size())),
                   0);
    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();
    processor.processBlock (buffer, midi);
    processor.flushPendingSysEx();

    expect (std::abs (parameterValue (processor, parameters::cutoff) - before) < 1.0e-6f,
            "a foreign sysex message moved the patch");
    processor.releaseResources();
}

// A lane automating one of the previous release's ids still has to control
// the sound. The bridge forwards a change to the pair that replaced it.
void testLegacyAutomationIdsStillReachTheSwitches()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    const auto pump = [&processor] {
        for (int attempt = 0; attempt < 40; ++attempt)
            juce::Thread::sleep (5);
    };
    juce::ignoreUnused (pump);

    // Unison through the old three-way id.
    setParameterValue (processor, parameters::legacyKeyMode, 2.0f);
    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::poly1) > 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "the legacy key mode did not reach both POLY buttons");

    setParameterValue (processor, parameters::legacyChorus, 2.0f);
    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::chorusII) > 0.5f
                && parameterValue (processor, parameters::chorusI) < 0.5f,
            "the legacy chorus id did not reach the chorus buttons");

    // The bridge must not fight the panel: a direct change to a switch stands
    // until the legacy id itself moves again.
    setParameterValue (processor, parameters::chorusI, 1.0f);
    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::chorusI) > 0.5f,
            "the legacy bridge overwrote a switch the player had just moved");

    processor.releaseResources();
}

void testFactoryProgramsLoad()
{
    YouKnow106AudioProcessor processor;
    // Program 0 is the init patch, so the bank sits at 1..N.
    expect (processor.getNumPrograms() == presets::presetCount + 1,
            "the host does not see the whole factory bank");
    expect (processor.getProgramName (0) == "INIT",
            "program 0 is not the init patch");
    // A freshly constructed processor holds the layout defaults, so the program
    // it reports has to be the one that actually describes them.
    expect (processor.getCurrentProgram() == 0,
            "a new processor claims a factory program it has not loaded");

    for (int index = 0; index < processor.getNumPrograms(); ++index)
        expect (processor.getProgramName (index).isNotEmpty(),
                "a factory program has no name");
    expect (processor.getProgramName (-1).isEmpty(),
            "a negative program index returned a name");
    expect (processor.getProgramName (processor.getNumPrograms()).isEmpty(),
            "a program index past the end returned a name");

    // Selecting a program has to move the patch controls and leave the
    // performance controls alone, because the instrument does not store them.
    setParameterValue (processor, parameters::volume, 0.42f);
    setParameterValue (processor, parameters::portamento, 0.33f);
    processor.setCurrentProgram (2);
    expect (processor.getCurrentProgram() == 2, "the selected program was not kept");

    const auto& expected = presets::factoryBank()[1].patch;
    expect (std::abs (parameterValue (processor, parameters::cutoff) - expected.cutoff)
                < 0.01f,
            "selecting a program did not load its cutoff");
    expect (std::abs (parameterValue (processor, parameters::volume) - 0.42f) < 1.0e-4f,
            "loading a patch moved the volume, which is not part of a patch");
    expect (std::abs (parameterValue (processor, parameters::portamento) - 0.33f)
                < 1.0e-4f,
            "loading a patch moved portamento, which is not part of a patch");

    // INIT restores the patch, not the instrument: the controls a patch does
    // not carry stay where the player put them, as they do for every other
    // program selection.
    setParameterValue (processor, parameters::volume, 0.42f);
    setParameterValue (processor, parameters::transpose, 5.0f);
    processor.setCurrentProgram (0);
    expect (std::abs (parameterValue (processor, parameters::volume) - 0.42f) < 1.0e-4f,
            "selecting INIT reset the volume, which is not part of a patch");
    expect (std::abs (parameterValue (processor, parameters::transpose) - 5.0f) < 1.0e-4f,
            "selecting INIT reset the transpose, which is not part of a patch");
    expect (std::abs (parameterValue (processor, parameters::cutoff) - 0.62f) < 0.01f,
            "selecting INIT did not restore the default patch");
    processor.setCurrentProgram (2);

    processor.setCurrentProgram (-1);
    expect (processor.getCurrentProgram() == 2,
            "an out-of-range program index changed the selection");
    processor.setCurrentProgram (processor.getNumPrograms());
    expect (processor.getCurrentProgram() == 2,
            "a program index past the end changed the selection");

    // An entry the patch memory cannot hold has to say so in its name, so a
    // bank about to be sent to hardware does not surprise anyone.
    bool sawMarked = false;
    for (int index = 1; index < processor.getNumPrograms(); ++index)
        if (! presets::factoryBank()[static_cast<std::size_t> (index - 1)]
                  .exportsLosslessly())
        {
            sawMarked = true;
            expect (processor.getProgramName (index).contains ("I+II"),
                    "a preset that cannot be exported is not marked as such");
        }
    expect (sawMarked, "no preset exercises the unstorable chorus setting");
}

void testEveryPanelLegendFitsInTheRealFont()
{
    const auto boldFont = [] (float height) {
        return juce::Font (juce::FontOptions (height, juce::Font::bold));
    };
    // expect() takes a std::string; every message here is built as a
    // juce::String, so it is converted at the point of use.
    const auto say = [] (const juce::String& text) { return text.toStdString(); };

    for (const auto& section : panel::sections())
    {
        const float available = section.width - panel::sectionPadding;
        const float drawn = juce::GlyphArrangement::getStringWidth (
            boldFont (panel::headerPointSize), section.name);
        expect (drawn <= available,
                say (juce::String ("section header is truncated: ") + section.name
                     + " needs " + juce::String (drawn, 1) + " in "
                     + juce::String (available, 1)));
        // The approximation must also be an over-estimate, not merely close:
        // that is the property the JUCE-free check depends on.
        expect (panel::textWidth (section.name, panel::headerPointSize, true) >= drawn,
                say (juce::String ("the width model under-estimates ") + section.name));
    }

    for (const auto& control : panel::controls())
    {
        if (control.kind == panel::ControlKind::Slider
            || control.kind == panel::ControlKind::Steps)
        {
            const float drawn = juce::GlyphArrangement::getStringWidth (
                boldFont (panel::labelPointSize), control.label);
            expect (drawn <= control.labelWidth,
                    say (juce::String ("slider legend is truncated: ") + control.label
                         + " needs " + juce::String (drawn, 1) + " in "
                         + juce::String (control.labelWidth, 1)));
            expect (panel::textWidth (control.label, panel::labelPointSize, true)
                        >= drawn,
                    say (juce::String ("the width model under-estimates ")
                         + control.label));
        }
        else
        {
            const float size = panel::buttonPointSizeFor (control);
            expect (size >= panel::buttonPointSizeMin,
                    say (juce::String ("button legend is set too small to read: ")
                         + control.label));
            const float drawn =
                juce::GlyphArrangement::getStringWidth (boldFont (size), control.label);
            expect (drawn <= control.width,
                    say (juce::String ("button legend is truncated: ") + control.label
                         + " needs " + juce::String (drawn, 1) + " in "
                         + juce::String (control.width, 1)));
        }
    }
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

    // The default size follows the panel description, so widening a section to
    // fit a legend moves it. Hard-coding the number here is what made this test
    // fail the moment the layout legitimately changed.
    const int expectedWidth = juce::roundToInt (panel::panelWidth());
    expect (editor->getWidth() == expectedWidth && editor->getHeight() == 470,
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

    editor->setSize (expectedWidth, 470);
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
    testUiKeyboardPressAndReleaseIsHeard();
    testDeferredQualitySwitchIsNotAutomatable();
    testAllNotesOffReleasesAndAllSoundOffCuts();
    testTransportOfControllers();
    testPanicSilencesEverything();
    testStateRoundTripAndMigration();
    testRandomizerPreservesQualityAndLevel();
    testBusLayoutsAndTail();
    testSysExPatchRoundTripsThroughTheParameters();
    testSingleParameterSysExDoesNotDisturbAnythingElse();
    testAWholeBankTransferIsNotDropped();
    testRequestedDumpLeavesThroughTheMidiOutput();
    testSelectedProgramSurvivesAStateRoundTrip();
    testForeignSysExLeavesThePatchAlone();
    testLegacyAutomationIdsStillReachTheSwitches();
    testFactoryProgramsLoad();
    testEveryPanelLegendFitsInTheRealFont();
    testEditorBuildsAndRenders();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " YouKnow106 plug-in check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All YouKnow106 plug-in checks passed.\n";
    return EXIT_SUCCESS;
}
