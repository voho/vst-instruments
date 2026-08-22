// Plug-in level tests: the APVTS layout mirrors the documented parameter
// contract, MIDI reaches the engine (including the documented CC map), the
// factory programs load, state round-trips, and the editor renders — writing
// the committed screenshot when the build asks for it.

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
        if (processor.getProgramName (index) == "SuperLead201")
            superSawProgram = index;
    expect (superSawProgram > 0, "the supersaw program exists");

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
        if (processor.getProgramName (index) == "SuperLead201")
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
        if (processor.getProgramName (index) == "SuperLead201")
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
        if (processor.getProgramName (index) == "SuperLead201")
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

    auto* minusOctave = findButton (*editor, "-OCT");
    auto* fifth = findButton (*editor, "5TH");
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
    expect (findButton (*editor, "OFF") != nullptr,
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
    expect (findButton (*editor, "ON") != nullptr,
            "a switch that is on says so on its face");
}

void testEditorAndSnapshot()
{
    SeptumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "the processor provides an editor");
    if (editor == nullptr)
        return;

    editor->setSize (editor->getWidth(), editor->getHeight());
    expect (editor->getWidth() >= 1000 && editor->getHeight() >= 500,
            "the editor opens at its panel size");

    // Every control the panel puts on screen has to be given bounds. A
    // section whose declared row counts do not cover its contents lays out
    // all but the last control and leaves that one at (0, 0, 0, 0): visible,
    // attached to its parameter, and invisible to the player. SPLIT ARPEGGIO
    // was exactly that until the ARPEGGIO section's rows were corrected.
    int placed = 0, unplaced = 0, escaped = 0;
    for (int i = 0; i < editor->getNumChildComponents(); ++i)
    {
        auto* child = editor->getChildComponent (i);
        if (child == nullptr || ! child->isVisible())
            continue;
        if (child->getBounds().isEmpty())
        {
            ++unplaced;
            continue;
        }
        ++placed;
        if (! editor->getLocalBounds().contains (child->getBounds()))
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
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;

    testParameterLayoutAndDefaults();
    testBusLayoutAndTail();
    testRenderingAndVoices();
    testDocumentedControlChanges();
    testControlChangesDoNotNotifyFromTheAudioThread();
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
    testTogglesShowTheirState();
    testEditorAndSnapshot();

    if (failures == 0)
    {
        std::printf ("Septum plug-in tests passed (%d checks).\n", checks);
        return 0;
    }
    std::fprintf (stderr, "%d of %d plug-in checks failed.\n", failures, checks);
    return 1;
}
