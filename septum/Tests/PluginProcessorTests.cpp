// Plug-in level tests: the APVTS layout mirrors the documented parameter
// contract, MIDI reaches the engine (including the documented CC map), the
// factory programs load, state round-trips, and the editor renders — writing
// the committed screenshot when the build asks for it.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/SeptumSysEx.h"

#include <cmath>
#include <cstdio>
#include <string>

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
    SeptumAudioProcessor processor;

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
    SeptumAudioProcessor processor;
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
    SeptumAudioProcessor processor;
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
    SeptumAudioProcessor processor;
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

// A received panel CC edits the parameter's raw value on the audio thread and
// notifies nobody from there: a gesture or a host notification inside the
// render callback takes the processor's listener lock and can wake the
// message thread, and a controller sweep makes 128 of them a second. The
// message-thread reconciler is what catches the parameter object and the host
// up.
struct CountingParameterListener final
    : public juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged (int, float) override { ++values; }
    void parameterGestureChanged (int, bool) override { ++gestures; }
    int values { 0 };
    int gestures { 0 };
};

void testControlChangesDoNotNotifyFromTheAudioThread()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);

    auto* parameter = processor.parameters.getParameter ("up_cutoff");
    expect (parameter != nullptr, "the cutoff parameter exists");
    if (parameter == nullptr)
        return;

    CountingParameterListener listener;
    parameter->addListener (&listener);

    auto cc = messageAt (juce::MidiMessage::controllerEvent (1, 74, 40));
    processor.processBlock (buffer, cc);

    expect (listener.values == 0 && listener.gestures == 0,
            "a received CC notifies nothing from the render callback (values "
                + std::to_string (listener.values) + ", gestures "
                + std::to_string (listener.gestures) + ")");
    expect ((int) processor.parameters.getRawParameterValue ("up_cutoff")->load()
                == 40,
            "the CC still lands in the value the engine renders from");

    // The message-thread half. It runs from an AsyncUpdater in the plug-in;
    // here it is called directly, as the harness stands in for the loop.
    processor.reconcileControlChanges();
    const auto& range = processor.parameters.getParameterRange ("up_cutoff");
    expect (std::abs (parameter->getValue() - range.convertTo0to1 (40.0f))
                < 1.0e-6,
            "the reconciler catches the parameter object up");
    expect (listener.values > 0 && listener.gestures > 0,
            "the reconciler is what notifies the host");

    parameter->removeListener (&listener);
}

// The reconciler publishes a CC's value to the host and the UI, and
// setValueNotifyingHost writes the parameter object's own storage — the very
// atomic the audio thread stores a received CC into and the engine renders
// from. Reading that storage to decide what to publish therefore raced with
// the audio thread: a CC arriving after the read put its value there, the
// publish wrote the older value back over it, and the newer CC's dirty bit
// then made the next pass republish the stale value it had just been
// overwritten with. The controller's move was lost outright.
//
// The reconciler reads a shadow only the audio thread writes now. Here the
// clobber is staged directly: the parameter's storage is set to a stale value
// behind the reconciler's back, which is exactly the state the race leaves.
void testTheCcReconcilerCannotPublishAStaleValue()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);

    auto* parameter = processor.parameters.getParameter ("up_cutoff");
    auto* raw = processor.parameters.getRawParameterValue ("up_cutoff");
    expect (parameter != nullptr && raw != nullptr, "the cutoff parameter exists");
    if (parameter == nullptr || raw == nullptr)
        return;

    auto cc = messageAt (juce::MidiMessage::controllerEvent (1, 74, 90));
    processor.processBlock (buffer, cc);
    expect ((int) raw->load() == 90, "the CC lands in the rendered value");

    raw->store (11.0f);              // what the race leaves behind
    processor.reconcileControlChanges();

    const auto& range = processor.parameters.getParameterRange ("up_cutoff");
    expect (std::abs (parameter->getValue() - range.convertTo0to1 (90.0f)) < 1.0e-6,
            "the reconciler publishes the CC's own value, not the parameter's"
            " storage (published "
                + juce::String (range.convertFrom0to1 (parameter->getValue())).toStdString()
                + ")");
    expect ((int) raw->load() == 90,
            "and the value the engine renders from is the CC's too (got "
                + juce::String (raw->load()).toStdString() + ")");
}

double goertzel (const juce::AudioBuffer<float>& buffer, double hz,
                 double sampleRate)
{
    const double w = 2.0 * 3.14159265358979323846 * hz / sampleRate;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    const auto* x = buffer.getReadPointer (0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const double s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return std::sqrt (std::max (0.0, power)) / buffer.getNumSamples();
}

void testPanelCcAppliesWithinTheBlock()
{
    // A mapped panel CC must reach the engine before the next rendered
    // segment: a note in the same block as CC#74 = 0 plays through a fully
    // closed filter from its first sample.
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);

    juce::MidiBuffer midi;
    // Cutoff 30 leaves the fundamental clearly audible (~2 octaves above
    // the corner) while the eighth harmonic sits ~5 octaves above it.
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 30), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 120), 0);

    juce::AudioBuffer<float> capture (2, 44100);
    capture.clear();
    juce::AudioBuffer<float> block (2, 512);
    int written = 0;
    for (int i = 0; i < 86; ++i)
    {
        juce::MidiBuffer events = i == 0 ? midi : juce::MidiBuffer();
        processor.processBlock (block, events);
        const int count = juce::jmin (512, capture.getNumSamples() - written);
        for (int channel = 0; channel < 2; ++channel)
            capture.copyFrom (channel, written, block, channel, 0, count);
        written += count;
        if (written >= capture.getNumSamples())
            break;
    }

    // With the corner near 100 Hz the eighth harmonic must sit far below
    // the fundamental; an open filter leaves them comparable on a saw.
    const double fundamental = goertzel (capture, 440.0, 44100.0);
    const double eighth = goertzel (capture, 3520.0, 44100.0);
    expect (fundamental > 1.0e-7, "the darkened note still sounds");
    expect (eighth < fundamental * 0.05,
            "the same-block CC closed the filter before the note rendered");
}

void testProgramChangeStagesOnTheAudioPath()
{
    // A MIDI program change followed by a note in the same block must play
    // the new program even when the message loop never runs (offline hosts).
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);

    int superSawProgram = -1;
    for (int index = 0; index < processor.getNumPrograms(); ++index)
        if (processor.getProgramName (index).contains ("SuperLead201"))
            superSawProgram = index;
    expect (superSawProgram >= 0, "the supersaw program exists");

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::programChange (1, superSawProgram), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 120), 0);

    juce::AudioBuffer<float> capture (2, 44100);
    capture.clear();
    juce::AudioBuffer<float> block (2, 512);
    int written = 0;
    for (int i = 0; written < capture.getNumSamples(); ++i)
    {
        juce::MidiBuffer events = i == 0 ? midi : juce::MidiBuffer();
        processor.processBlock (block, events);
        const int count = juce::jmin (512, capture.getNumSamples() - written);
        for (int channel = 0; channel < 2; ++channel)
            capture.copyFrom (channel, written, block, channel, 0, count);
        written += count;
    }

    // SuperLead201's OSC1 spread puts a detuned partial well below 440 Hz
    // that the INIT saw does not have; its presence proves the program
    // reached the audio path without the message thread's help.
    const double centre = goertzel (capture, 440.0, 44100.0);
    const double detuned = goertzel (capture, 431.0, 44100.0);
    expect (centre > 1.0e-6, "the note sounds");
    expect (detuned > centre * 0.1,
            "the staged program's supersaw spread is audible in-block");
}

void testUiQueueOverflowStillReleases()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    // Fill the UI queue completely without processing, then release a note
    // whose event no longer fits: the release must still reach the engine.
    processor.triggerFromUi (60, 100);
    for (int i = 0; i < 70; ++i)
        processor.triggerFromUi (61, 100);
    processor.releaseFromUi (60);  // queue is full here
    processor.releaseFromUi (61);  // also latched

    juce::AudioBuffer<float> block (2, 256);
    juce::MidiBuffer empty;
    processor.processBlock (block, empty);
    for (int i = 0; i < 400; ++i)
    {
        juce::MidiBuffer none;
        processor.processBlock (block, none);
    }
    expect (processor.getActiveVoiceCount() == 0,
            "overflowed note-offs still release every voice");
}

void testRetriggerAfterOverflowSurvives()
{
    // Re-pressing a key whose release was latched during a queue overflow
    // supersedes that release: the stale latch must not cut the new note
    // short once the queue drains.
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    processor.triggerFromUi (60, 100);
    juce::AudioBuffer<float> block (2, 256);
    juce::MidiBuffer first;
    processor.processBlock (block, first);  // note 60 is sounding and held

    // Fill the queue with note-offs for a silent key, overflow the release
    // of 60 so it latches, then re-press 60 while the queue is still full.
    for (int i = 0; i < 64; ++i)
        processor.releaseFromUi (61);
    processor.releaseFromUi (60);       // queue full: latched instead
    processor.triggerFromUi (60, 100);  // the press supersedes the latch

    for (int i = 0; i < 400; ++i)
    {
        juce::MidiBuffer none;
        processor.processBlock (block, none);
    }
    expect (processor.getActiveVoiceCount() >= 1,
            "a re-pressed key outlives its overflowed release");
}

void testProgramChangeLandsWithoutMessagePump()
{
    // A MIDI program change writes the program into the raw parameter
    // values on the audio path itself; the queued message-thread spray only
    // repeats them with host/UI notification. So even in a host that never
    // pumps the message loop, the program must land and later parameter
    // edits must compose on top of it instead of being eaten.
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);

    int superSawProgram = -1;
    for (int index = 0; index < processor.getNumPrograms(); ++index)
        if (processor.getProgramName (index).contains ("SuperLead201"))
            superSawProgram = index;

    juce::AudioBuffer<float> block (2, 512);
    juce::MidiBuffer programChange =
        messageAt (juce::MidiMessage::programChange (1, superSawProgram));
    processor.processBlock (block, programChange);
    juce::MidiBuffer drain;
    processor.processBlock (block, drain);

    const int wave = (int) processor.parameters
                         .getRawParameterValue ("up_osc1_wave")
                         ->load();
    expect (wave == 7, "the program's parameters landed without a message pump");

    // With the staging consumed, parameter edits reach the render again:
    // muting the tone must actually mute the note.
    auto* level = processor.parameters.getParameter ("up_level");
    level->setValueNotifyingHost (
        processor.parameters.getParameterRange ("up_level").convertTo0to1 (0.0f));
    juce::MidiBuffer note =
        messageAt (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 120));
    juce::AudioBuffer<float> capture (2, 44100);
    capture.clear();
    int written = 0;
    for (int i = 0; written < capture.getNumSamples(); ++i)
    {
        juce::MidiBuffer events = i == 0 ? note : juce::MidiBuffer();
        processor.processBlock (block, events);
        const int count = juce::jmin (512, capture.getNumSamples() - written);
        for (int channel = 0; channel < 2; ++channel)
            capture.copyFrom (channel, written, block, channel, 0, count);
        written += count;
    }
    expect (bufferRms (capture) < 1.0e-5,
            "edits after the program change reach the audio path");
}

void testReconcileKeepsEditsAfterProgramChange()
{
    // An edit that lands after a program change — even in the same MIDI
    // block — must survive the message thread's queued reconciliation:
    // only values untouched since the change are repeated with
    // notification, so the edit can never snap back.
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);

    int superSawProgram = -1;
    for (int index = 0; index < processor.getNumPrograms(); ++index)
        if (processor.getProgramName (index).contains ("SuperLead201"))
            superSawProgram = index;
    const septum::Patch& factory =
        septum::factoryPatches()[(std::size_t) superSawProgram].patch;
    expect (factory.upper.cutoff != 30, "the edit differs from the factory value");

    juce::MidiBuffer events;
    events.addEvent (juce::MidiMessage::programChange (1, superSawProgram), 0);
    events.addEvent (juce::MidiMessage::controllerEvent (1, 74, 30), 0);
    juce::AudioBuffer<float> block (2, 512);
    processor.processBlock (block, events);

    // Stand in for the queued message-loop callback.
    processor.reconcileProgram (superSawProgram);

    expect ((int) processor.parameters.getRawParameterValue ("up_cutoff")->load()
                == 30,
            "the CC edit survives the reconciliation");
    auto* wave = processor.parameters.getParameter ("up_osc1_wave");
    const auto& range = processor.parameters.getParameterRange ("up_osc1_wave");
    expect ((int) std::lround (range.convertFrom0to1 (wave->getValue())) == 7,
            "untouched parameters reconcile through the parameter objects");
}

void testStateSurvivesUnpumpedProgramChange()
{
    // With the message loop never pumped, saved state must still carry what
    // is audible — the raw values the program change wrote — not the value
    // tree's stale pre-change copy.
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);

    int superSawProgram = -1;
    for (int index = 0; index < processor.getNumPrograms(); ++index)
        if (processor.getProgramName (index).contains ("SuperLead201"))
            superSawProgram = index;

    juce::MidiBuffer events =
        messageAt (juce::MidiMessage::programChange (1, superSawProgram));
    juce::AudioBuffer<float> block (2, 512);
    processor.processBlock (block, events);

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);

    SeptumAudioProcessor restored;
    restored.setStateInformation (saved.getData(), (int) saved.getSize());
    expect ((int) restored.parameters.getRawParameterValue ("up_osc1_wave")
                ->load()
                == 7,
            "saved state carries the audible program without a message pump");
    expect (restored.getCurrentProgram() == superSawProgram,
            "the program index round-trips with it");
}

void testProgramsLoad()
{
    SeptumAudioProcessor processor;
    // 64 slots, laid out the way the instrument lays its own bank out. None of
    // them is Roland's: the SH-201's factory patch data is published nowhere.
    expect (processor.getNumPrograms() == 64,
            "the bank exposes 64 program slots");
    expect (processor.getProgramName (0).contains ("SuperLead201"),
            "program 0 is SuperLead201");

    int superSawProgram = -1;
    for (int index = 0; index < processor.getNumPrograms(); ++index)
        if (processor.getProgramName (index).contains ("SuperLead201"))
            superSawProgram = index;
    expect (superSawProgram >= 0, "the supersaw lead ships in the bank");

    processor.setCurrentProgram (superSawProgram);
    expect (processor.getCurrentProgram() == superSawProgram,
            "the program index sticks");
    const int wave = (int) processor.parameters
                         .getRawParameterValue ("up_osc1_wave")
                         ->load();
    expect (wave == 7, "loading the lead selects SUPER-SAW (enum 7)");

    const auto patch = processor.snapshotPatch();
    expect (patch.upper.osc1.wave == septum::Waveform::SuperSaw,
            "the engine snapshot agrees with the loaded program");
}

void testStateRoundTrip()
{
    SeptumAudioProcessor processor;
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

    SeptumAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    expect ((int) restored.parameters.getRawParameterValue ("up_cutoff")->load()
                == 55,
            "cutoff survives the state round trip");
    expect ((int) restored.parameters.getRawParameterValue ("lo_resonance")->load()
                == 111,
            "resonance survives the state round trip");
}

// The panel lives on a canvas child of the editor, so the suite reaches it
// through the editor's own accessor rather than walking children blind.
juce::Component& panelOf (juce::AudioProcessorEditor& editor)
{
    auto* septum = dynamic_cast<SeptumAudioProcessorEditor*> (&editor);
    return septum != nullptr ? septum->getPanel() : editor;
}

// Walks a component tree looking for a button whose face reads `text`.
juce::Button* findButton (juce::Component& root, const juce::String& text)
{
    for (int i = 0; i < root.getNumChildComponents(); ++i)
    {
        auto* child = root.getChildComponent (i);
        if (child == nullptr)
            continue;
        if (auto* button = dynamic_cast<juce::Button*> (child))
            if (button->getButtonText() == text)
                return button;
        if (auto* found = findButton (*child, text))
            return found;
    }
    return nullptr;
}

// Settled (OM p. 30): "-OCT ... lowers the OSC 2 pitch one octave below that
// of OSC 1"; "the OSC 2 pitch will be seven semitones (a perfect fifth)
// higher than OSC 1"; and both together "the OSC 2 pitch will be the same as
// the OSC 1 pitch". All three are intervals, so a transposed OSC 1 moves them.
void testIntervalButtonsAreRelativeToOscOne()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "the processor provides an editor");
    if (editor == nullptr)
        return;

    const auto set = [&processor] (const char* id, float natural)
    {
        auto* parameter = processor.parameters.getParameter (id);
        const auto& range = processor.parameters.getParameterRange (id);
        parameter->setValueNotifyingHost (
            range.convertTo0to1 (range.snapToLegalValue (natural)));
    };
    const auto get = [&processor] (const char* id)
    {
        return (int) processor.parameters.getRawParameterValue (id)->load();
    };

    auto* minusOctave = findButton (panelOf (*editor), "-OCT");
    auto* fifth = findButton (panelOf (*editor), "5TH");
    expect (minusOctave != nullptr && fifth != nullptr,
            "the panel carries both INTERVAL buttons");
    if (minusOctave == nullptr || fifth == nullptr)
        return;

    set ("up_osc1_wide", 1.0f);     // room for +/-36, so nothing clamps
    set ("up_osc2_wide", 1.0f);
    set ("up_osc1_pitch", 5.0f);
    set ("up_osc2_pitch", 0.0f);

    minusOctave->onClick();
    expect (get ("up_osc2_pitch") == -7,
            "-OCT puts OSC 2 an octave below OSC 1, not at -12 (got "
                + std::to_string (get ("up_osc2_pitch")) + ")");
    minusOctave->onClick();
    expect (get ("up_osc2_pitch") == 5,
            "pressing -OCT again returns OSC 2 to OSC 1's pitch (got "
                + std::to_string (get ("up_osc2_pitch")) + ")");

    fifth->onClick();
    expect (get ("up_osc2_pitch") == 12,
            "5TH puts OSC 2 a fifth above OSC 1 (got "
                + std::to_string (get ("up_osc2_pitch")) + ")");

    // Near the ends of the pitch range the interval the button aims at does
    // not exist, so the write snaps — and comparing against the unsnapped
    // target made the button a one-way trap: it landed on +36, read "not there
    // yet", and the second press, documented as the way back to unison, did
    // nothing at all.
    set ("up_osc1_pitch", 30.0f);
    set ("up_osc2_pitch", 0.0f);
    fifth->onClick();
    expect (get ("up_osc2_pitch") == 36,
            "5TH from OSC 1 at +30 lands on the top of the range (got "
                + std::to_string (get ("up_osc2_pitch")) + ")");
    fifth->onClick();
    expect (get ("up_osc2_pitch") == 30,
            "and pressing it again still returns OSC 2 to OSC 1's pitch (got "
                + std::to_string (get ("up_osc2_pitch")) + ")");

    set ("up_osc1_pitch", -30.0f);
    set ("up_osc2_pitch", 0.0f);
    minusOctave->onClick();
    expect (get ("up_osc2_pitch") == -36,
            "-OCT from OSC 1 at -30 lands on the bottom of the range (got "
                + std::to_string (get ("up_osc2_pitch")) + ")");
    minusOctave->onClick();
    expect (get ("up_osc2_pitch") == -30,
            "and pressing it again returns OSC 2 to OSC 1's pitch (got "
                + std::to_string (get ("up_osc2_pitch")) + ")");
}

// The three invariants the panel is built on were `jassert`s, and NDEBUG
// removes those from every build this project produces — the plug-in, both
// test binaries and CI's Release — so what the change log called "a build-time
// failure" was present in no build and checked by nothing. They are state the
// editor publishes now, and this is what enforces them.
void testThePanelsInvariantsAreCheckedBySomethingThatRuns()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditor());
    auto* editor = dynamic_cast<SeptumAudioProcessorEditor*> (base.get());
    expect (editor != nullptr, "the processor provides its own editor");
    if (editor == nullptr)
        return;

    expect (editor->getUnresolvedParameterIds().isEmpty(),
            "every control on the panel names a parameter that exists (unresolved: "
                + editor->getUnresolvedParameterIds().joinIntoString (", ").toStdString()
                + ")");
    expect (editor->getMixedScopeSections().isEmpty(),
            "no section mixes per-tone and shared controls (mixed: "
                + editor->getMixedScopeSections().joinIntoString (", ").toStdString()
                + ")");

    // A section is sized to its contents, so one handed less room than it asked
    // for does not shrink — it overflows. TONE PLAY was handed 94 points for the
    // 102 its single control row declares, because the keyboard row was reduced
    // vertically before the section was cut out of it, and its GLIDE TIME, BEND
    // and TONE OCT read-outs sat on the well's bottom border.
    const auto design = SeptumAudioProcessorEditor::panelSizeForWorkArea ({});
    editor->setSize (design.getWidth(), design.getHeight());
    editor->resized();
    expect (editor->getSectionsOverflowingTheirWell().isEmpty(),
            "every section's contents fit inside its own well (overflowing: "
                + editor->getSectionsOverflowingTheirWell()
                      .joinIntoString (", ")
                      .toStdString()
                + ")");

    // The titles `resized()` addresses by index. Inserting a section shifts
    // every list below it, and the three layoutBand calls would then lay out
    // the wrong sections into the wrong bands.
    const auto titles = editor->getSectionTitles();
    const std::pair<int, const char*> addressed[] {
        { 1, "OSC 1" }, { 5, "AMP" },      { 6, "PITCH ENV" },
        { 10, "LFO 2" }, { 11, "ARPEGGIO" }, { 14, "REVERB" }
    };
    for (const auto& entry : addressed)
        expect (titles.size() > entry.first
                    && titles[entry.first] == juce::String (entry.second),
                std::string ("section ") + std::to_string (entry.first) + " is "
                    + entry.second + " (found "
                    + (titles.size() > entry.first ? titles[entry.first].toStdString()
                                                   : std::string ("nothing"))
                    + ")");
}

// A switch on the panel says which way it is thrown.
void testTogglesShowTheirState()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
        return;

    const auto set = [&processor] (const char* id, bool on)
    {
        auto* parameter = processor.parameters.getParameter (id);
        parameter->setValueNotifyingHost (on ? 1.0f : 0.0f);
    };

    set ("delay_on", false);
    expect (findButton (panelOf (*editor), "OFF") != nullptr,
            "a switch that is off says so on its face");
    set ("delay_on", true);
    set ("reverb_on", true);
    set ("arp_on", true);
    set ("up_overdrive", true);
    set ("up_portamento", true);
    set ("up_osc1_wide", true);
    set ("up_osc2_wide", true);
    set ("up_lfo1_sync", true);
    set ("up_lfo2_sync", true);
    set ("up_lfo1_key_trig", true);
    set ("up_lfo2_key_trig", true);
    set ("arp_hold", true);
    set ("ext_center_cancel", true);
    set ("audio_filter_on", true);
    expect (findButton (panelOf (*editor), "ON") != nullptr,
            "a switch that is on says so on its face");
}

// The panel is a fixed geometry, so the only question a small screen asks is
// whether it scales or gets cut off. A 1366x768 laptop's work area is under
// 768 points tall once the taskbar and the host's window frame are counted,
// and the design panel is 786.
// [settled] SYSTEM COMMON: MASTER TUNE (the frequency of A4, 415.30-466.20
// Hz), MASTER KEY SHIFT -24..+24, keyboard OCTAVE SHIFT -3..+3 and TRANSPOSE
// -5..+6. The engine has honoured all four since it was written; nothing
// reached them.
// The bend/mod lever's modulation axis holds its position, so a click that
// set it absolutely could latch full vibrato from one tap on the top of the
// travel — something the hardware lever cannot do. It moves by the drag now,
// and a double click puts it back.
// The D Beam is not implemented — an infrared distance sensor is a control
// surface, and a plug-in has no hand above it. What survives is the four
// Patch Common bytes the beam owns, stored so a SysEx round trip stays
// lossless: published, saved, program-changed with the patch, and
// non-automatable, because a host should not offer a lane that cannot change
// what the player hears. CC#69, the beam's own controller, is accepted and
// ignored.
void testDBeamBytesAreStoredAndInert()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    for (const char* id : { "dbeam_mode", "dbeam_value", "dbeam_sens" })
        expect (processor.parameters.getParameter (id) == nullptr,
                juce::String ("the plug-in no longer publishes ") + id);

    for (const char* id : { "dbeam_dest", "dbeam_assign", "dbeam_polarity",
                            "active_expression" })
    {
        auto* parameter = processor.parameters.getParameter (id);
        expect (parameter != nullptr,
                juce::String ("the stored D Beam byte ") + id
                    + " is still published");
        if (parameter != nullptr)
            expect (! parameter->isAutomatable(),
                    juce::String (id) + " is published as non-automatable");
    }

    // One policy, not two. PITCH WIDE holds the same [settled range, no
    // effect] position: the manual gives it as expanding the COARSE knob's
    // *travel*, a numeric parameter that already reaches +/-36 has none to
    // expand, and nothing in the engine reads the byte — so it must not be
    // offered as an automation lane either. Four parameters, two per tone.
    for (const char* id : { "up_osc1_wide", "up_osc2_wide", "lo_osc1_wide",
                            "lo_osc2_wide" })
    {
        auto* parameter = processor.parameters.getParameter (id);
        expect (parameter != nullptr,
                juce::String ("the stored PITCH WIDE byte ") + id
                    + " is still published");
        if (parameter != nullptr)
            expect (! parameter->isAutomatable(),
                    juce::String (id) + " is published as non-automatable");
    }

    // The settled 37-entry assign list, in the address map's order: it bounds
    // Patch Common 1F, so it has to stay complete even with no beam to move.
    if (auto* assign = dynamic_cast<juce::AudioParameterChoice*> (
            processor.parameters.getParameter ("dbeam_assign")))
    {
        expect (assign->choices.size() == 37,
                "D BEAM ASSIGN carries all 37 documented destinations (got "
                    + std::to_string (assign->choices.size()) + ")");
        expect (assign->choices[0] == "OSC1-PITCH"
                    && assign->choices[36] == "BENDER",
                "the assign list runs from OSC1-PITCH to BENDER");
    }

    // Settled (OM p. 72): CC#69 is "Part Pitch (D Beam Pitch Mode)". With no
    // beam it must move nothing at all.
    std::vector<float> before;
    for (auto* parameter : processor.getParameters())
        before.push_back (parameter->getValue());
    juce::AudioBuffer<float> buffer (2, 256);
    auto beamCc = messageAt (juce::MidiMessage::controllerEvent (1, 69, 96));
    processor.processBlock (buffer, beamCc);
    bool moved = false;
    for (int i = 0; i < processor.getParameters().size(); ++i)
        moved = moved
                || processor.getParameters()[i]->getValue() != before[(std::size_t) i];
    expect (! moved, "CC#69 is accepted and moves no parameter");

    // They are patch data, so a program change carries them.
    const auto get = [&processor] (const char* id)
    {
        return (int) processor.parameters.getRawParameterValue (id)->load();
    };
    auto* assign = processor.parameters.getParameter ("dbeam_assign");
    const auto& range = processor.parameters.getParameterRange ("dbeam_assign");
    assign->setValueNotifyingHost (range.convertTo0to1 (36.0f));  // BENDER
    expect (get ("dbeam_assign") == 36, "the stored assign byte takes a value");
    processor.setCurrentProgram (2);
    expect (get ("dbeam_assign")
                == (int) septum::factoryPatches()[2].patch.dBeamAssign,
            "a program change carries the stored D Beam bytes with the patch");
}

void testLeverModulationMovesByTheDrag()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* septum = dynamic_cast<SeptumAudioProcessorEditor*> (editor.get());
    expect (septum != nullptr, "the processor provides the Septum editor");
    if (septum == nullptr)
        return;
    editor->setSize (editor->getWidth(), editor->getHeight());

    auto& lever = septum->getLever();
    const auto press = [&lever] (juce::Point<float> at)
    {
        const juce::MouseEvent event (
            juce::Desktop::getInstance().getMainMouseSource(), at,
            juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            &lever, &lever, juce::Time::getCurrentTime(), at,
            juce::Time::getCurrentTime(), 1, false);
        return event;
    };

    const auto top = juce::Point<float> ((float) lever.getWidth() * 0.5f, 4.0f);
    const auto bottom =
        juce::Point<float> ((float) lever.getWidth() * 0.5f,
                            (float) lever.getHeight() - 16.0f);

    lever.mouseDown (press (top));
    lever.mouseUp (press (top));
    expect (lever.getModulation() == 0.0f,
            "a tap at the top of the travel does not latch modulation (value "
                + std::to_string (lever.getModulation()) + ")");

    // A drag from the bottom to the top does.
    lever.mouseDown (press (bottom));
    lever.mouseDrag (press (top));
    expect (lever.getModulation() > 0.5f,
            "a drag up the travel raises modulation (value "
                + std::to_string (lever.getModulation()) + ")");
    lever.mouseUp (press (top));

    lever.mouseDoubleClick (press (top));
    expect (lever.getModulation() == 0.0f,
            "a double click puts the modulation back");
}

void testSystemCommonSettings()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    for (const char* id : { "system_master_tune", "system_key_shift",
                            "system_octave", "system_transpose" })
        expect (processor.parameters.getParameter (id) != nullptr,
                juce::String ("the plug-in publishes ") + id);

    const auto set = [&processor] (const char* id, float natural)
    {
        auto* parameter = processor.parameters.getParameter (id);
        const auto& range = processor.parameters.getParameterRange (id);
        parameter->setValueNotifyingHost (
            range.convertTo0to1 (range.snapToLegalValue (natural)));
    };

    // A held A4 against MASTER TUNE. A fresh instance each time, because a
    // note left sounding at the old tuning would be in the take as well.
    const auto pitchOf = [] (float tuneHz)
    {
        SeptumAudioProcessor instance;
        instance.prepareToPlay (44100.0, 512);
        const auto write = [&instance] (const char* id, float natural)
        {
            auto* parameter = instance.parameters.getParameter (id);
            const auto& range = instance.parameters.getParameterRange (id);
            parameter->setValueNotifyingHost (
                range.convertTo0to1 (range.snapToLegalValue (natural)));
        };
        write ("system_master_tune", tuneHz);
        write ("delay_on", 0.0f);
        write ("reverb_on", 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
        instance.processBlock (buffer, midi);
        std::vector<float> take;
        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer none;
            instance.processBlock (buffer, none);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                take.push_back (buffer.getSample (0, i));
        }
        // A saw crosses zero upward once per cycle, so the crossings over a
        // known span are the frequency.
        int crossings = 0;
        for (std::size_t i = 1; i < take.size(); ++i)
            if (take[i - 1] <= 0.0f && take[i] > 0.0f)
                ++crossings;
        return crossings * 44100.0 / (double) take.size();
    };

    const double atA440 = pitchOf (440.0f);
    const double atA466 = pitchOf (466.16f);
    expect (std::abs (atA440 - 440.0) < 12.0,
            "A4 renders at A440 by default (measured "
                + std::to_string (atA440) + ")");
    expect (atA466 > atA440 * 1.03,
            "MASTER TUNE moves the whole instrument (measured "
                + std::to_string (atA466) + " against "
                + std::to_string (atA440) + ")");

    // The panel's OCT buttons write the settled -3..+3 parameter.
    set ("system_master_tune", 440.0f);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
        return;
    auto* down = findButton (panelOf (*editor), "DOWN");
    auto* up = findButton (panelOf (*editor), "UP");
    expect (down != nullptr && up != nullptr,
            "the panel carries the octave buttons");
    if (down == nullptr || up == nullptr)
        return;
    for (int i = 0; i < 5; ++i)
        up->onClick();
    expect ((int) processor.parameters.getRawParameterValue ("system_octave")
                    ->load()
                == 3,
            "the octave buttons reach the documented +3");
    for (int i = 0; i < 10; ++i)
        down->onClick();
    expect ((int) processor.parameters.getRawParameterValue ("system_octave")
                    ->load()
                == -3,
            "the octave buttons reach the documented -3");

    // A program change is patch data; the system block is not.
    set ("system_key_shift", 7.0f);
    set ("system_transpose", -4.0f);
    processor.setCurrentProgram (1);
    expect ((int) processor.parameters.getRawParameterValue ("system_key_shift")
                    ->load()
                == 7
                && (int) processor.parameters
                           .getRawParameterValue ("system_transpose")
                           ->load()
                       == -4,
            "a program change leaves the system settings alone");

    // ...and they survive the state round trip.
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    SeptumAudioProcessor restored;
    restored.prepareToPlay (44100.0, 256);
    restored.setStateInformation (state.getData(), (int) state.getSize());
    expect ((int) restored.parameters.getRawParameterValue ("system_key_shift")
                    ->load()
                == 7,
            "the system settings survive a state round trip");
}

void testEditorFitsASmallDisplay()
{
    using Editor = SeptumAudioProcessorEditor;
    // An unknown work area is what the design size means here.
    const auto design = Editor::panelSizeForWorkArea ({});
    expect (design.getWidth() >= 1000 && design.getHeight() >= 500,
            "the panel has a design size");
    expect (Editor::panelSizeForWorkArea ({ 3840, 2160 }) == design,
            "a display with room opens the panel at its design size");
    const double aspect =
        (double) design.getWidth() / (double) design.getHeight();
    for (const auto work : { juce::Rectangle<int> { 1366, 768 },
                             juce::Rectangle<int> { 1280, 800 },
                             juce::Rectangle<int> { 1440, 900 } })
    {
        const auto size = Editor::panelSizeForWorkArea (work);
        expect (size.getWidth() <= work.getWidth()
                    && size.getHeight() <= work.getHeight(),
                "the panel fits a " + std::to_string (work.getWidth()) + "x"
                    + std::to_string (work.getHeight()) + " work area");
        expect (std::abs ((double) size.getWidth() / size.getHeight() - aspect)
                    < 0.01,
                "the panel keeps its proportions when it shrinks");
    }

    // And the controls still land inside it at the smallest size the rule
    // will produce, because the layout never depends on the window.
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
        return;
    for (const auto size : { design / 2, design, design * 3 / 2 })
    {
        editor->setSize (size.getWidth(), size.getHeight());
        auto& panel = panelOf (*editor);
        int unplaced = 0, escaped = 0;
        for (int i = 0; i < panel.getNumChildComponents(); ++i)
        {
            auto* child = panel.getChildComponent (i);
            if (child == nullptr || ! child->isVisible())
                continue;
            if (child->getBounds().isEmpty())
                ++unplaced;
            else if (! panel.getLocalBounds().contains (child->getBounds()))
                ++escaped;
        }
        expect (unplaced == 0 && escaped == 0,
                "every control is placed at window width "
                    + std::to_string (size.getWidth()));
    }
}

void testEditorAndSnapshot()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "the processor provides an editor");
    if (editor == nullptr)
        return;

    // The editor opens at whatever fits the display it is on, so the
    // documentation image is pinned to the panel's design size here rather
    // than taken from whatever the build machine happens to have.
    const auto design = SeptumAudioProcessorEditor::panelSizeForWorkArea ({});
    editor->setSize (design.getWidth(), design.getHeight());
    editor->resized();
    expect (editor->getWidth() == design.getWidth()
                && editor->getHeight() == design.getHeight(),
            "the documentation screenshot is the panel's design size");

    // Every control the panel puts on screen has to be given bounds. A
    // section whose declared row counts do not cover its contents lays out
    // all but the last control and leaves that one at (0, 0, 0, 0): visible,
    // attached to its parameter, and invisible to the player. SPLIT ARPEGGIO
    // was exactly that until the ARPEGGIO section's rows were corrected.
    int placed = 0, unplaced = 0, escaped = 0;
    auto& panel = panelOf (*editor);
    for (int i = 0; i < panel.getNumChildComponents(); ++i)
    {
        auto* child = panel.getChildComponent (i);
        if (child == nullptr || ! child->isVisible())
            continue;
        if (child->getBounds().isEmpty())
        {
            ++unplaced;
            continue;
        }
        ++placed;
        if (! panel.getLocalBounds().contains (child->getBounds()))
            ++escaped;
    }
    expect (placed > 100, "the panel places its controls");
    expect (unplaced == 0, "every visible control on the panel has bounds");
    expect (escaped == 0, "no control is laid out past the panel's edge");

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
        juce::SystemStats::getEnvironmentVariable ("SEPTUM_EDITOR_SNAPSHOT", {});
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

// [settled] "Of the System Exclusive messages received by this device, the
// Universal Non-realtime messages and the Universal Realtime messages and the
// Data Request (RQ1) messages and the Data Set (DT1) messages will be set
// automatically." Three Universal Realtime device-control messages name a
// SYSTEM COMMON parameter apiece, and this replica publishes all three.
void testUniversalRealtimeDeviceControl()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    juce::AudioBuffer<float> block (2, 256);

    const auto send = [&] (std::uint8_t sub, int lsb, int msb)
    {
        const std::uint8_t body[] { 0x7F, 0x7F, 0x04, sub,
                                    (std::uint8_t) (lsb & 0x7F),
                                    (std::uint8_t) (msb & 0x7F) };
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::createSysExMessage (body, (int) sizeof (body)), 0);
        processor.processBlock (block, midi);
    };
    const auto valueOf = [&] (const char* id)
    {
        return processor.parameters.getRawParameterValue (id)->load();
    };

    // Master Volume: "The lower byte (llH) ... will be handled as 00H".
    const float levelBefore = valueOf ("master_level");
    send (0x01, 0x7F, 40);
    expect (std::abs (valueOf ("master_level") - 40.0f) < 0.5f
                && std::abs (levelBefore - 40.0f) > 0.5f,
            "Master Volume lands on MASTER LEVEL (got "
                + juce::String (valueOf ("master_level")).toStdString() + ")");

    // Master Fine Tuning: 40 00H is +/-0, so A4 stays at 440 Hz; 00 00H is
    // -100 cents, which is the documented bottom of MASTER TUNE, 415.30 Hz.
    send (0x03, 0x00, 0x40);
    expect (std::abs (valueOf ("system_master_tune") - 440.0f) < 0.05f,
            "Master Fine Tuning centre is A440 (got "
                + juce::String (valueOf ("system_master_tune")).toStdString() + ")");
    send (0x03, 0x00, 0x00);
    expect (std::abs (valueOf ("system_master_tune") - 415.30f) < 0.05f,
            "Master Fine Tuning at -100 cents is the range's own bottom (got "
                + juce::String (valueOf ("system_master_tune")).toStdString() + ")");

    // Master Coarse Tuning: "llH: ignored", mmH 28H - 40H - 58H = -24 - +24.
    send (0x04, 0x7F, 0x28);
    expect (std::abs (valueOf ("system_key_shift") + 24.0f) < 0.5f,
            "Master Coarse Tuning 28H is -24 semitones (got "
                + juce::String (valueOf ("system_key_shift")).toStdString() + ")");
    send (0x04, 0x00, 0x58);
    expect (std::abs (valueOf ("system_key_shift") - 24.0f) < 0.5f,
            "Master Coarse Tuning 58H is +24 semitones (got "
                + juce::String (valueOf ("system_key_shift")).toStdString() + ")");

    // A universal message the instrument does not list is not swallowed as
    // one it does.
    const float before = valueOf ("master_level");
    send (0x02, 0x00, 0x00);
    expect (std::abs (valueOf ("master_level") - before) < 0.001f,
            "an unlisted device-control sub-ID changes nothing");

    // A device-control message has to take effect where it sits in the block,
    // not at the next one. SYSTEM COMMON was read once before the MIDI loop,
    // so a Master Volume arriving mid-block was a whole buffer late — in an
    // offline render with a large buffer, arbitrarily late.
    {
        SeptumAudioProcessor timed;
        timed.prepareToPlay (44100.0, 4096);
        juce::AudioBuffer<float> block (2, 4096);

        // A held note, then MASTER LEVEL to zero a quarter of the way in.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
        const std::uint8_t body[] { 0x7F, 0x7F, 0x04, 0x01, 0x00, 0x00 };
        midi.addEvent (juce::MidiMessage::createSysExMessage (body, (int) sizeof (body)),
                       1024);
        timed.processBlock (block, midi);

        double before = 0.0, after = 0.0;
        for (int i = 512; i < 1024; ++i)
            before = std::max (before, (double) std::abs (block.getSample (0, i)));
        for (int i = 3072; i < 4096; ++i)
            after = std::max (after, (double) std::abs (block.getSample (0, i)));
        expect (before > 1.0e-3,
                "the note is sounding before the message (peak "
                    + juce::String (before).toStdString() + ")");
        expect (after < 0.2 * before,
                "MASTER LEVEL 0 takes effect inside the same block (peak after "
                    + juce::String (after).toStdString() + " against "
                    + juce::String (before).toStdString() + ")");
    }

    // A dump that arrives after a program change but before the program's
    // queued re-notification must still reach the host and the UI. That pass
    // writes nothing new, so clearing the dump's pending flag left the host
    // and the panel on the program's values while the engine rendered the
    // dump's.
    {
        SeptumAudioProcessor host;
        host.prepareToPlay (44100.0, 256);
        juce::AudioBuffer<float> b (2, 256);

        juce::MidiBuffer program;
        program.addEvent (juce::MidiMessage::programChange (1, 6), 0);
        host.processBlock (b, program);

        septum::Patch dumped = septum::initPatch();
        dumped.upper.cutoff = 19;
        for (const auto& pkt : septum::sysex::encodePatchToSysExPackets (dumped))
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
            host.processBlock (b, midi);
        }

        // The program's queued pass runs after the dump has landed.
        host.reconcileProgram (6);
        host.republishPatchParameters();

        auto* parameter = host.parameters.getParameter ("up_cutoff");
        const auto& range = host.parameters.getParameterRange ("up_cutoff");
        expect (parameter != nullptr
                    && std::abs (parameter->getValue() - range.convertTo0to1 (19.0f))
                           < 1.0e-6,
                "a dump landing after a program change still reaches the host"
                " (host has "
                    + juce::String (parameter != nullptr
                                        ? range.convertFrom0to1 (parameter->getValue())
                                        : -1.0f).toStdString()
                    + ")");
    }

    // The republish must not put an older value back over a message that
    // arrived while it was running. setValueNotifyingHost writes the same
    // atomic the audio thread stores into, so reading that atomic to decide
    // what to publish lost the newer message outright. Staged here the way the
    // race leaves it: the parameter's storage is set to a stale value behind
    // the republish's back.
    send (0x01, 0x00, 55);
    processor.parameters.getRawParameterValue ("master_level")->store (9.0f);
    processor.republishSystemParameters();
    expect (std::abs (valueOf ("master_level") - 55.0f) < 0.5f,
            "the system republish publishes the message's own value, not the"
            " parameter's storage (got "
                + juce::String (valueOf ("master_level")).toStdString() + ")");
}

// A patch dump is 22 DT1 packets and they arrive on the audio thread, one or
// more per block. The message-thread republish used to read each value back
// from the parameter object's own storage — the very atomic the audio thread
// writes — and setValueNotifyingHost writes it, so a packet landing while the
// republish ran had its value overwritten with the older one. A whole block of
// the dump could be lost that way.
//
// The republish reads a shadow only the audio thread writes now. Here the
// clobber is staged directly: the parameters are set to stale values behind
// the republish's back, which is exactly the state the race leaves.
void testThePatchReconcilerCannotPublishAStaleDump()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    juce::AudioBuffer<float> block (2, 256);

    septum::Patch dump = septum::initPatch();
    dump.upper.cutoff = 31;
    dump.upper.resonance = 96;
    dump.lower.cutoff = 118;
    dump.tempo = 205;

    for (const auto& pkt : septum::sysex::encodePatchToSysExPackets (dump))
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
        processor.processBlock (block, midi);
    }

    auto* upperCutoff = processor.parameters.getRawParameterValue ("up_cutoff");
    auto* lowerCutoff = processor.parameters.getRawParameterValue ("lo_cutoff");
    expect (upperCutoff != nullptr && lowerCutoff != nullptr, "the cutoffs exist");
    if (upperCutoff == nullptr || lowerCutoff == nullptr)
        return;
    expect ((int) upperCutoff->load() == 31 && (int) lowerCutoff->load() == 118,
            "the dump lands in the values the engine renders from");

    // What a republish overtaken by a later packet leaves behind.
    upperCutoff->store (7.0f);
    lowerCutoff->store (7.0f);
    processor.republishPatchParameters();

    expect ((int) upperCutoff->load() == 31 && (int) lowerCutoff->load() == 118,
            "the republish restores the dump's own values, not the parameter"
            " storage (upper " + juce::String (upperCutoff->load()).toStdString()
                + ", lower " + juce::String (lowerCutoff->load()).toStdString() + ")");

    auto* parameter = processor.parameters.getParameter ("up_cutoff");
    const auto& range = processor.parameters.getParameterRange ("up_cutoff");
    expect (parameter != nullptr
                && std::abs (parameter->getValue() - range.convertTo0to1 (31.0f)) < 1.0e-6,
            "and the host is told the dump's value");

    // A program change that lands after the dump but before its republish is
    // the later writer, so the republish must not undo it.
    {
        SeptumAudioProcessor second;
        second.prepareToPlay (44100.0, 256);
        juce::AudioBuffer<float> b (2, 256);
        for (const auto& pkt : septum::sysex::encodePatchToSysExPackets (dump))
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
            second.processBlock (b, midi);
        }
        second.setCurrentProgram (5);
        const float afterProgram =
            second.parameters.getRawParameterValue ("up_cutoff")->load();
        second.republishPatchParameters();
        expect (std::abs (second.parameters.getRawParameterValue ("up_cutoff")->load()
                          - afterProgram) < 1.0e-6,
                "a program change after the dump survives the dump's republish"
                " (was " + juce::String (afterProgram).toStdString() + ", now "
                    + juce::String (second.parameters.getRawParameterValue ("up_cutoff")->load()).toStdString()
                    + ")");
    }
}

// The arpeggio grid is patch data on the hardware — sixteen SysEx blocks of
// it — and it is the one documented field with no plug-in parameter to hold
// it. `snapshotPatch()` rebuilds the style from the selector every block, so
// every decoded row used to be thrown away by the next call: a pattern
// imported from a real unit neither played nor survived a re-export.
void testAnImportedArpeggioPatternSurvivesAndPlays()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    juce::AudioBuffer<float> block (2, 256);

    // A grid that matches none of the shipped styles, with an Original Note
    // per row and an END STEP the templates do not use.
    septum::Patch dump = septum::initPatch();
    dump.arpeggio.styleIndex = 0;
    dump.arpeggio.endStep = 0;              // "as long as the style is"
    dump.arpeggio.style = septum::ArpeggioStyle {};
    dump.arpeggio.style.endStep = 13;
    for (int row = 0; row < septum::arpeggioMaxRows; ++row)
    {
        dump.arpeggio.style.originalNote[(std::size_t) row] = 48 + row;
        for (int step = 0; step < 13; ++step)
            dump.arpeggio.style.cells[(std::size_t) step][(std::size_t) row] =
                (signed char) (((step * 7 + row * 5) % 3 == 0)
                                   ? septum::arpeggioTie
                                   : (signed char) (1 + ((step * 11 + row) % 127)));
    }

    const auto packets = septum::sysex::encodePatchToSysExPackets (dump);
    for (const auto& pkt : packets)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
        processor.processBlock (block, midi);
    }

    const auto loaded = processor.snapshotPatch();
    bool cellsMatch = true;
    for (int step = 0; step < septum::arpeggioMaxSteps && cellsMatch; ++step)
        for (int row = 0; row < septum::arpeggioMaxRows; ++row)
            if (loaded.arpeggio.style.cell (step, row)
                != dump.arpeggio.style.cell (step, row))
            {
                cellsMatch = false;
                break;
            }
    expect (cellsMatch, "every one of the imported grid's 512 cells reaches the engine");

    bool notesMatch = true;
    for (int row = 0; row < septum::arpeggioMaxRows; ++row)
        if (loaded.arpeggio.style.originalNote[(std::size_t) row] != 48 + row)
            notesMatch = false;
    expect (notesMatch, "each row's Original Note reaches the engine");

    expect (loaded.arpeggio.style.endStep == 13,
            "the imported END STEP reaches the engine (got "
                + juce::String (loaded.arpeggio.style.endStep).toStdString() + ")");
    expect ((int) processor.parameters.getRawParameterValue ("arp_end_step")->load() == 13,
            "and lands on the END STEP parameter, which is the same control");

    // It survives a re-export, which is what makes the plug-in a usable
    // waypoint between two real units.
    {
        const auto exported = processor.createSysExDataForCurrentPatch();
        std::vector<septum::NamedPatch> parsed;
        expect (septum::sysex::parseSyxBankFile (exported.data(), exported.size(), parsed)
                    && parsed.size() == 1,
                "the re-export parses back to one patch");
        if (parsed.size() == 1)
        {
            bool same = true;
            for (int step = 0; step < septum::arpeggioMaxSteps && same; ++step)
                for (int row = 0; row < septum::arpeggioMaxRows; ++row)
                    if (parsed[0].patch.arpeggio.style.cell (step, row)
                        != dump.arpeggio.style.cell (step, row))
                    {
                        same = false;
                        break;
                    }
            expect (same, "the imported grid survives a re-export unchanged");
        }
    }

    // The same grid handed to the plug-in as a .syx buffer through the API,
    // which is a different entry point from a dump arriving on the wire:
    // `loadSysExData` parses the file and goes through `loadPatch`, which
    // writes the parameter list — and the grid is not in the parameter list.
    {
        const auto exported = processor.createSysExDataForCurrentPatch();
        SeptumAudioProcessor recipient;
        recipient.prepareToPlay (44100.0, 256);
        recipient.loadSysExData (exported.data(), exported.size());
        const auto after = recipient.snapshotPatch();
        bool same = true;
        for (int step = 0; step < septum::arpeggioMaxSteps && same; ++step)
            for (int row = 0; row < septum::arpeggioMaxRows; ++row)
                if (after.arpeggio.style.cell (step, row)
                    != dump.arpeggio.style.cell (step, row))
                {
                    same = false;
                    break;
                }
        expect (same, "the grid survives a .syx loaded through loadSysExData");
        expect (after.arpeggio.style.originalNote[7] == 55,
                "and so do its Original Notes");
        expect (after.arpeggio.style.endStep == 13,
                "and its END STEP (got "
                    + juce::String (after.arpeggio.style.endStep).toStdString() + ")");
    }

    // And a session save/restore.
    {
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        SeptumAudioProcessor restored;
        restored.prepareToPlay (44100.0, 256);
        restored.setStateInformation (state.getData(), (int) state.getSize());
        const auto after = restored.snapshotPatch();
        bool same = true;
        for (int step = 0; step < septum::arpeggioMaxSteps && same; ++step)
            for (int row = 0; row < septum::arpeggioMaxRows; ++row)
                if (after.arpeggio.style.cell (step, row)
                    != dump.arpeggio.style.cell (step, row))
                {
                    same = false;
                    break;
                }
        expect (same, "the imported grid survives a session save and restore");
        expect (after.arpeggio.style.originalNote[3] == 51,
                "and so do the Original Notes");
    }

    // A state restore is the newest writer. A dump whose republish is still
    // queued must not run afterwards and put its own values back over the
    // session that was just loaded.
    {
        SeptumAudioProcessor host;
        host.prepareToPlay (44100.0, 256);
        juce::AudioBuffer<float> b (2, 256);

        // A session with a distinctive cutoff, saved.
        if (auto* cutoff = host.parameters.getParameter ("up_cutoff"))
            cutoff->setValueNotifyingHost (
                host.parameters.getParameterRange ("up_cutoff").convertTo0to1 (23.0f));
        juce::MemoryBlock session;
        host.getStateInformation (session);

        // A dump lands on the audio path, leaving a republish queued.
        septum::Patch other = septum::initPatch();
        other.upper.cutoff = 101;
        for (const auto& pkt : septum::sysex::encodePatchToSysExPackets (other))
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
            host.processBlock (b, midi);
        }
        expect ((int) host.parameters.getRawParameterValue ("up_cutoff")->load() == 101,
                "the dump landed before the restore");

        // The host restores the session before the message loop gets there.
        host.setStateInformation (session.getData(), (int) session.getSize());
        const float restored =
            host.parameters.getRawParameterValue ("up_cutoff")->load();
        host.republishPatchParameters();
        expect (std::abs (host.parameters.getRawParameterValue ("up_cutoff")->load()
                          - restored) < 0.001f,
                "a queued dump republish does not overwrite a restored session"
                " (restored " + juce::String (restored).toStdString() + ", now "
                    + juce::String (host.parameters.getRawParameterValue ("up_cutoff")->load()).toStdString()
                    + ")");
    }

    // A restore whose grid property is present but unreadable must retire the
    // grid this processor was holding: it belongs to the session that has
    // just been replaced. Leaving it valid meant a restored patch whose
    // selector happened to match played the stale grid instead of its style.
    {
        SeptumAudioProcessor host;
        host.prepareToPlay (44100.0, 256);
        juce::AudioBuffer<float> b (2, 256);
        septum::Patch atZero = dump;
        atZero.arpeggio.styleIndex = 0;
        for (const auto& pkt : septum::sysex::encodePatchToSysExPackets (atZero))
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
            host.processBlock (b, midi);
        }
        expect (host.snapshotPatch().arpeggio.style.cell (0, 0)
                    == dump.arpeggio.style.cell (0, 0),
                "the grid is held before the malformed restore");

        // A session of its own, with a grid property that is not decodable.
        juce::MemoryBlock clean;
        SeptumAudioProcessor donor;
        donor.prepareToPlay (44100.0, 256);
        donor.getStateInformation (clean);
        if (const auto xml = juce::AudioProcessor::getXmlFromBinary (
                clean.getData(), (int) clean.getSize()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            tree.setProperty ("arpeggio_grid", "not base64 at all!!", nullptr);
            tree.setProperty ("arpeggio_grid_selector", 0, nullptr);
            juce::MemoryBlock broken;
            if (const auto out = tree.createXml())
                juce::AudioProcessor::copyXmlToBinary (*out, broken);
            host.setStateInformation (broken.getData(), (int) broken.getSize());
        }

        septum::Patch reference = septum::initPatch();
        reference.arpeggio.endStep = 0;
        septum::applyArpeggioStyle (reference, 0);
        const auto after = host.snapshotPatch();
        bool matchesTemplate = true;
        for (int step = 0; step < septum::arpeggioMaxSteps && matchesTemplate; ++step)
            for (int row = 0; row < septum::arpeggioMaxRows; ++row)
                if (after.arpeggio.style.cell (step, row)
                    != reference.arpeggio.style.cell (step, row))
                {
                    matchesTemplate = false;
                    break;
                }
        expect (matchesTemplate,
                "a restore with an unreadable grid retires the one being held");
    }

    // A factory program carries its own style. The selector is only the key
    // the imported grid is filed under, so a program whose style index
    // happens to match the one a dump arrived under must still play its own
    // template, not the imported grid.
    {
        SeptumAudioProcessor host;
        host.prepareToPlay (44100.0, 256);
        juce::AudioBuffer<float> b (2, 256);

        // Find a factory program and file the imported grid under its own
        // style index, which is the collision that has to be safe.
        const int program = 12;
        const int programStyle =
            septum::factoryPatches()[(std::size_t) program].patch.arpeggio.styleIndex;
        septum::Patch collide = dump;
        collide.arpeggio.styleIndex = programStyle;
        if (auto* selector = host.parameters.getParameter ("arp_style"))
            selector->setValueNotifyingHost (
                host.parameters.getParameterRange ("arp_style")
                    .convertTo0to1 ((float) programStyle));
        for (const auto& pkt : septum::sysex::encodePatchToSysExPackets (collide))
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
            host.processBlock (b, midi);
        }
        expect (host.snapshotPatch().arpeggio.style.cell (0, 0)
                    == dump.arpeggio.style.cell (0, 0),
                "the imported grid is in place before the program change");

        host.setCurrentProgram (program);
        septum::Patch reference =
            septum::factoryPatches()[(std::size_t) program].patch;
        septum::applyArpeggioStyle (reference, programStyle);
        const auto after = host.snapshotPatch();
        bool matchesProgram = true;
        for (int step = 0; step < septum::arpeggioMaxSteps && matchesProgram; ++step)
            for (int row = 0; row < septum::arpeggioMaxRows; ++row)
                if (after.arpeggio.style.cell (step, row)
                    != reference.arpeggio.style.cell (step, row))
                {
                    matchesProgram = false;
                    break;
                }
        expect (matchesProgram,
                "a program change drops the imported grid and plays the"
                " program's own style");
    }

    // A grid discarded before a save must not come back with the session.
    // The tree handed to getStateInformation is a copy of the last state, so
    // it can still carry an earlier restore's grid properties.
    {
        SeptumAudioProcessor first;
        first.prepareToPlay (44100.0, 256);
        juce::AudioBuffer<float> b (2, 256);
        for (const auto& pkt : septum::sysex::encodePatchToSysExPackets (dump))
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
            first.processBlock (b, midi);
        }
        juce::MemoryBlock withGrid;
        first.getStateInformation (withGrid);

        // A session restored with the grid, then a factory program, then
        // saved again: the second save must not still carry the grid.
        SeptumAudioProcessor second;
        second.prepareToPlay (44100.0, 256);
        second.setStateInformation (withGrid.getData(), (int) withGrid.getSize());
        second.setCurrentProgram (12);
        juce::MemoryBlock afterProgram;
        second.getStateInformation (afterProgram);

        SeptumAudioProcessor third;
        third.prepareToPlay (44100.0, 256);
        third.setStateInformation (afterProgram.getData(), (int) afterProgram.getSize());
        const auto after = third.snapshotPatch();
        septum::Patch reference = septum::factoryPatches()[12].patch;
        septum::applyArpeggioStyle (reference, reference.arpeggio.styleIndex);
        bool matchesProgram = true;
        for (int step = 0; step < septum::arpeggioMaxSteps && matchesProgram; ++step)
            for (int row = 0; row < septum::arpeggioMaxRows; ++row)
                if (after.arpeggio.style.cell (step, row)
                    != reference.arpeggio.style.cell (step, row))
                {
                    matchesProgram = false;
                    break;
                }
        expect (matchesProgram,
                "a grid discarded by a program change does not come back with"
                " the next session save");
    }

    // Moving the style selector retires the grid for good: coming back to the
    // same index has to load that template, not resurrect the import.
    {
        SeptumAudioProcessor host;
        host.prepareToPlay (44100.0, 256);
        juce::AudioBuffer<float> b (2, 256);
        septum::Patch atZero = dump;
        atZero.arpeggio.styleIndex = 0;
        for (const auto& pkt : septum::sysex::encodePatchToSysExPackets (atZero))
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
            host.processBlock (b, midi);
        }
        auto* selector = host.parameters.getParameter ("arp_style");
        const auto& range = host.parameters.getParameterRange ("arp_style");
        expect (selector != nullptr, "the style selector exists");
        if (selector != nullptr)
        {
            selector->setValueNotifyingHost (range.convertTo0to1 (4.0f));
            host.snapshotPatch();                       // the move is noticed here
            selector->setValueNotifyingHost (range.convertTo0to1 (0.0f));
            const auto back = host.snapshotPatch();
            septum::Patch reference = septum::initPatch();
            reference.arpeggio.endStep = 0;
            septum::applyArpeggioStyle (reference, 0);
            bool matchesTemplate = true;
            for (int step = 0; step < septum::arpeggioMaxSteps && matchesTemplate; ++step)
                for (int row = 0; row < septum::arpeggioMaxRows; ++row)
                    if (back.arpeggio.style.cell (step, row)
                        != reference.arpeggio.style.cell (step, row))
                    {
                        matchesTemplate = false;
                        break;
                    }
            expect (matchesTemplate,
                    "returning to the imported grid's own selector index loads"
                    " the template, not the retired grid");
        }
    }

    // Moving the style selector picks a template again, which is what the
    // hardware's panel does — the grid is not sticky across a selection.
    {
        auto* selector = processor.parameters.getParameter ("arp_style");
        expect (selector != nullptr, "the style selector exists");
        if (selector != nullptr)
        {
            const auto& range = processor.parameters.getParameterRange ("arp_style");
            selector->setValueNotifyingHost (range.convertTo0to1 (3.0f));
            const auto templated = processor.snapshotPatch();
            septum::Patch reference = septum::initPatch();
            reference.arpeggio.endStep = 0;
            septum::applyArpeggioStyle (reference, 3);
            bool matchesTemplate = true;
            for (int step = 0; step < septum::arpeggioMaxSteps && matchesTemplate; ++step)
                for (int row = 0; row < septum::arpeggioMaxRows; ++row)
                    if (templated.arpeggio.style.cell (step, row)
                        != reference.arpeggio.style.cell (step, row))
                    {
                        matchesTemplate = false;
                        break;
                    }
            expect (matchesTemplate,
                    "moving the selector selects a template over the imported grid");
        }
    }
}

void testSysExBlockProcessing()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    septum::Patch custom = septum::initPatch();
    custom.name = "SysExTest";
    custom.upper.osc1.wave = septum::Waveform::Square;
    custom.upper.cutoff = 42;
    custom.upper.resonance = 88;
    custom.delayOn = true;

    const auto packets = septum::sysex::encodePatchToSysExPackets (custom);
    juce::AudioBuffer<float> block (2, 256);

    for (const auto& pkt : packets)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage (pkt.data(), (int) pkt.size()), 0);
        processor.processBlock (block, midi);
    }

    const auto snapshot = processor.snapshotPatch();
    expect (snapshot.upper.osc1.wave == septum::Waveform::Square, "SysEx set OSC1 wave to Square");
    expect (snapshot.upper.cutoff == 42, "SysEx set cutoff to 42");
    expect (snapshot.upper.resonance == 88, "SysEx set resonance to 88");
    expect (snapshot.delayOn == true, "SysEx enabled delay");

    const auto exportedSyx = processor.createSysExDataForCurrentPatch();
    expect (exportedSyx.size() > 100, "createSysExDataForCurrentPatch generates valid SysEx buffer");

    SeptumAudioProcessor recipient;
    recipient.prepareToPlay (44100.0, 256);
    recipient.loadSysExData (exportedSyx.data(), exportedSyx.size());
    const auto recipientSnap = recipient.snapshotPatch();
    expect (recipientSnap.upper.cutoff == 42, "loadSysExData restored cutoff");
    expect (recipientSnap.upper.resonance == 88, "loadSysExData restored resonance");
}

// A received SysEx patch dump lands on the audio thread, so it may not notify
// the host from there — the split Step 17 established for control changes.
void testSysExDoesNotNotifyFromTheAudioThread()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    septum::Patch custom = septum::initPatch();
    custom.upper.cutoff = 42;
    custom.upper.resonance = 88;
    const auto packets = septum::sysex::encodePatchToSysExPackets (custom);

    auto* parameter = processor.parameters.getParameter ("up_cutoff");
    expect (parameter != nullptr, "the cutoff parameter exists");
    if (parameter == nullptr)
        return;
    CountingParameterListener listener;
    parameter->addListener (&listener);

    juce::AudioBuffer<float> block (2, 256);
    for (const auto& packet : packets)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage (packet.data(), (int) packet.size()), 0);
        processor.processBlock (block, midi);
    }
    expect (listener.values == 0 && listener.gestures == 0,
            "a received SysEx dump notifies nothing from the render callback"
            " (values " + std::to_string (listener.values) + ", gestures "
                + std::to_string (listener.gestures) + ")");
    expect ((int) processor.parameters.getRawParameterValue ("up_cutoff")->load() == 42,
            "the dump still lands in the value the engine renders from");

    // The message-thread half, which the harness stands in for.
    processor.republishPatchParameters();
    const auto& range = processor.parameters.getParameterRange ("up_cutoff");
    expect (std::abs (parameter->getValue() - range.convertTo0to1 (42.0f)) < 1.0e-6,
            "the reconciler catches the parameter object up");
    // Values, not gestures: a whole patch arriving over SysEx is not a
    // player dragging a knob, and that is what loadPatch has always reported.
    expect (listener.values > 0,
            "the reconciler is what notifies the host");
    parameter->removeListener (&listener);
}

// SYSTEM COMMON Octave Shift is applied once, by the engine. The drawn
// keyboard shifts the octave *names* its keys are printed with, not the notes
// they send: a click goes straight to engine.noteOn, which applies the shift
// itself, so moving the drawn range as well applied it twice — one press of
// OCT UP transposed the on-screen keys by two octaves while their printed
// names claimed one.
juce::MidiKeyboardComponent* findKeyboard (juce::Component& root)
{
    for (int i = 0; i < root.getNumChildComponents(); ++i)
    {
        auto* child = root.getChildComponent (i);
        if (auto* keys = dynamic_cast<juce::MidiKeyboardComponent*> (child))
            return keys;
        if (child != nullptr)
            if (auto* found = findKeyboard (*child))
                return found;
    }
    return nullptr;
}

void testKeyboardOctaveIsAppliedOnce()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* septumEditor = dynamic_cast<SeptumAudioProcessorEditor*> (editor.get());
    expect (septumEditor != nullptr, "the processor provides the Septum editor");
    if (septumEditor == nullptr)
        return;
    editor->setSize (editor->getWidth(), editor->getHeight());
    auto* keys = findKeyboard (septumEditor->getPanel());
    expect (keys != nullptr, "the panel draws a keyboard");
    if (keys == nullptr)
        return;

    auto* parameter = processor.parameters.getParameter ("system_octave");
    const auto& range = processor.parameters.getParameterRange ("system_octave");
    for (int shift : { 0, 1, -2, 3 })
    {
        parameter->setValueNotifyingHost (range.convertTo0to1 ((float) shift));
        septumEditor->resized();
        expect (keys->getRangeStart() == 36 && keys->getRangeEnd() == 96,
                "the drawn keys keep their note numbers at shift "
                    + std::to_string (shift) + " (range "
                    + std::to_string (keys->getRangeStart()) + ".."
                    + std::to_string (keys->getRangeEnd()) + ")");
        expect (keys->getOctaveForMiddleC() == 4 + shift,
                "the printed octave names follow the shift at "
                    + std::to_string (shift) + " (middle C octave "
                    + std::to_string (keys->getOctaveForMiddleC()) + ")");
    }
}

// The key-zone band's split-point caption has to stay on the panel and has to
// name the key the way the keyboard under it names it.
void testTheSplitPointCaptionStaysOnThePanelAndAgreesWithTheKeys()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditor());
    auto* editor = dynamic_cast<SeptumAudioProcessorEditor*> (base.get());
    if (editor == nullptr)
        return;
    const auto design = SeptumAudioProcessorEditor::panelSizeForWorkArea ({});
    editor->setSize (design.getWidth(), design.getHeight());
    editor->resized();

    const auto set = [&processor] (const char* id, float natural)
    {
        auto* parameter = processor.parameters.getParameter (id);
        const auto& range = processor.parameters.getParameterRange (id);
        parameter->setValueNotifyingHost (
            range.convertTo0to1 (range.snapToLegalValue (natural)));
    };
    set ("keyboard_mode", 2.0f);   // SPLIT

    // SPLIT POINT reaches C8 (108); the drawn keyboard stops at C7 (96), so the
    // top twelve settings put the boundary on the band's right edge. The name
    // used to be drawn unconditionally to its right, off the panel.
    for (int note : { 21, 36, 60, 84, 96, 97, 98, 103, 108 })
    {
        set ("split_point", (float) note);
        editor->resized();
        const auto caption = editor->getSplitPointCaption();
        expect (editor->getPanel().getLocalBounds().contains (caption.bounds),
                "the split-point caption for note " + std::to_string (note)
                    + " is drawn on the panel (x "
                    + std::to_string (caption.bounds.getX()) + ".."
                    + std::to_string (caption.bounds.getRight()) + " of "
                    + std::to_string (editor->getPanel().getWidth()) + ")");
    }

    // And it follows the octave shift, because the drawn keys' printed names do.
    auto* keys = findKeyboard (editor->getPanel());
    expect (keys != nullptr, "the panel draws a keyboard");
    if (keys == nullptr)
        return;
    set ("split_point", 60.0f);
    for (int shift : { 0, 1, -2 })
    {
        set ("system_octave", (float) shift);
        editor->resized();
        const auto expected = juce::MidiMessage::getMidiNoteName (
            60, true, true, keys->getOctaveForMiddleC());
        expect (editor->getSplitPointCaption().text == expected,
                "the split point is named like the key under it at shift "
                    + std::to_string (shift) + " (caption "
                    + editor->getSplitPointCaption().text.toStdString()
                    + ", key " + expected.toStdString() + ")");
    }
}

// Nothing on the panel may repaint on a frame where nothing moved. JUCE's
// setOctaveForMiddleC repaints unconditionally and the frame timer called it
// every tick, so an idle editor invalidated the whole 1204x73 keyboard 24 times
// a second — and through the scaled canvas re-ran the panel paint over that
// strip with it.
void testAnIdleLayoutDoesNotRepaintTheKeyboard()
{
    struct CountingImage final : juce::CachedComponentImage
    {
        int invalidations = 0;
        void paint (juce::Graphics&) override {}
        bool invalidate (const juce::Rectangle<int>&) override
        {
            ++invalidations;
            return true;
        }
        bool invalidateAll() override
        {
            ++invalidations;
            return true;
        }
        void releaseResources() override {}
    };

    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditor());
    auto* editor = dynamic_cast<SeptumAudioProcessorEditor*> (base.get());
    if (editor == nullptr)
        return;
    const auto design = SeptumAudioProcessorEditor::panelSizeForWorkArea ({});
    editor->setSize (design.getWidth(), design.getHeight());
    editor->resized();

    auto* keys = findKeyboard (editor->getPanel());
    expect (keys != nullptr, "the panel draws a keyboard");
    if (keys == nullptr)
        return;

    auto counter = std::make_unique<CountingImage>();
    auto* counting = counter.get();
    keys->setCachedComponentImage (counter.release());
    // One warm-up layout: attaching the image invalidates it once by itself.
    editor->resized();
    counting->invalidations = 0;
    editor->resized();
    expect (counting->invalidations == 0,
            "a layout that changes nothing does not invalidate the keyboard ("
                + std::to_string (counting->invalidations) + " invalidations)");

    // And it does still follow a shift that actually moves.
    auto* parameter = processor.parameters.getParameter ("system_octave");
    const auto& range = processor.parameters.getParameterRange ("system_octave");
    parameter->setValueNotifyingHost (range.convertTo0to1 (2.0f));
    editor->resized();
    expect (keys->getOctaveForMiddleC() == 6,
            "and a shift that does move still renames the keys (middle C octave "
                + std::to_string (keys->getOctaveForMiddleC()) + ")");
    keys->setCachedComponentImage (nullptr);
}

// Every section on the panel is wholly per-tone or wholly shared, and every
// per-tone section says which tone it is showing.
// The edit target is not a parameter — it changes nothing that sounds, so a
// host has no business automating it — and it rides in the state tree instead.
// setStateInformation replaces that tree wholesale, so an editor left open
// across a session load has to be told, or it keeps showing UPPER while the
// restored state says LOWER and the next save writes back the wrong one.
void testTheEditTargetFollowsARestoredState()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditor());
    auto* editor = dynamic_cast<SeptumAudioProcessorEditor*> (base.get());
    if (editor == nullptr)
        return;
    const auto design = SeptumAudioProcessorEditor::panelSizeForWorkArea ({});
    editor->setSize (design.getWidth(), design.getHeight());
    editor->resized();

    auto& panel = editor->getPanel();
    auto* upperTab = findButton (panel, "UPPER");
    auto* lowerTab = findButton (panel, "LOWER");
    if (upperTab == nullptr || lowerTab == nullptr)
        return;

    // A session saved while LOWER was the target.
    lowerTab->onClick();
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    expect (lowerTab->getToggleState(), "the panel is on LOWER before saving");

    // Back to UPPER, then that session is loaded under the open editor.
    upperTab->onClick();
    expect (upperTab->getToggleState(), "the panel is on UPPER before the load");
    processor.setStateInformation (saved.getData(), (int) saved.getSize());
    editor->resized();

    expect (lowerTab->getToggleState() && ! upperTab->getToggleState(),
            "the panel follows the restored state back to LOWER");
    expect ((bool) processor.parameters.state.getProperty ("editingUpperTone", true)
                == false,
            "and the stored property still says LOWER rather than being "
            "overwritten by the panel");
}

void testThePanelSaysWhichToneItIsEditing()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* septumEditor = dynamic_cast<SeptumAudioProcessorEditor*> (editor.get());
    if (septumEditor == nullptr)
        return;
    const auto design = SeptumAudioProcessorEditor::panelSizeForWorkArea ({});
    editor->setSize (design.getWidth(), design.getHeight());
    editor->resized();

    auto& panel = septumEditor->getPanel();
    auto* upper = findButton (panel, "UPPER");
    auto* lower = findButton (panel, "LOWER");
    expect (upper != nullptr && lower != nullptr,
            "the panel carries an edit-target pair");
    if (upper == nullptr || lower == nullptr)
        return;
    expect (upper->getToggleState() && ! lower->getToggleState(),
            "the panel opens on UPPER");

    // Switching the target re-points every per-tone control and leaves the
    // shared ones where they were.
    const auto cutoffOf = [&processor] (const char* id)
    {
        return (int) processor.parameters.getRawParameterValue (id)->load();
    };
    auto* upperCutoff = processor.parameters.getParameter ("up_cutoff");
    auto* lowerCutoff = processor.parameters.getParameter ("lo_cutoff");
    const auto& range = processor.parameters.getParameterRange ("up_cutoff");
    upperCutoff->setValueNotifyingHost (range.convertTo0to1 (30.0f));
    lowerCutoff->setValueNotifyingHost (range.convertTo0to1 (90.0f));

    if (lower->onClick)
        lower->onClick();
    expect (! upper->getToggleState() && lower->getToggleState(),
            "the target moves to LOWER");
    // The panel is showing LOWER now, so nudging a per-tone control has to
    // move the LOWER parameter and leave UPPER alone.
    expect (cutoffOf ("up_cutoff") == 30 && cutoffOf ("lo_cutoff") == 90,
            "switching the target edits neither tone by itself");

    // The target survives closing and reopening the editor.
    editor.reset();
    std::unique_ptr<juce::AudioProcessorEditor> reopened (processor.createEditor());
    auto* second = dynamic_cast<SeptumAudioProcessorEditor*> (reopened.get());
    if (second == nullptr)
        return;
    reopened->setSize (design.getWidth(), design.getHeight());
    auto* reopenedLower = findButton (second->getPanel(), "LOWER");
    expect (reopenedLower != nullptr && reopenedLower->getToggleState(),
            "reopening the editor keeps the tone the player was editing");
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;

    testParameterLayoutAndDefaults();
    testBusLayoutAndTail();
    testRenderingAndVoices();
    testDocumentedControlChanges();
    testControlChangesDoNotNotifyFromTheAudioThread();
    testTheCcReconcilerCannotPublishAStaleValue();
    testPanelCcAppliesWithinTheBlock();
    testProgramChangeStagesOnTheAudioPath();
    testUiQueueOverflowStillReleases();
    testRetriggerAfterOverflowSurvives();
    testProgramChangeLandsWithoutMessagePump();
    testReconcileKeepsEditsAfterProgramChange();
    testStateSurvivesUnpumpedProgramChange();
    testProgramsLoad();
    testStateRoundTrip();
    testIntervalButtonsAreRelativeToOscOne();
    testThePanelsInvariantsAreCheckedBySomethingThatRuns();
    testTogglesShowTheirState();
    testDBeamBytesAreStoredAndInert();
    testLeverModulationMovesByTheDrag();
    testSystemCommonSettings();
    testEditorFitsASmallDisplay();
    testEditorAndSnapshot();
    testUniversalRealtimeDeviceControl();
    testThePatchReconcilerCannotPublishAStaleDump();
    testAnImportedArpeggioPatternSurvivesAndPlays();
    testSysExBlockProcessing();
    testSysExDoesNotNotifyFromTheAudioThread();
    testKeyboardOctaveIsAppliedOnce();
    testTheSplitPointCaptionStaysOnThePanelAndAgreesWithTheKeys();
    testAnIdleLayoutDoesNotRepaintTheKeyboard();
    testThePanelSaysWhichToneItIsEditing();
    testTheEditTargetFollowsARestoredState();

    if (failures == 0)
    {
        std::printf ("Septum plug-in tests passed (%d checks).\n", checks);
        return 0;
    }
    std::fprintf (stderr, "%d of %d plug-in checks failed.\n", failures, checks);
    return 1;
}
