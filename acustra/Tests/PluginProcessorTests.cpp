#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
constexpr int editorWidth = 1120;
constexpr int editorHeight = 720;
constexpr int editorMinimumWidth = 896;
constexpr int editorMinimumHeight = 576;
constexpr int editorMaximumWidth = 1456;
constexpr int editorMaximumHeight = 936;

int failureCount = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

float valueOf (const AcustraAudioProcessor& processor, const char* id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, std::string { "missing parameter " } + id);
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setValue (AcustraAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr,
            std::string { "cannot set missing parameter " } + id);
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

bool finite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;
    return true;
}

float peak (const juce::AudioBuffer<float>& buffer, int begin = 0,
            int end = blockSize)
{
    float result = 0.0f;
    begin = std::clamp (begin, 0, buffer.getNumSamples());
    end = std::clamp (end, begin, buffer.getNumSamples());
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = begin; sample < end; ++sample)
            result = std::max (result,
                               std::abs (buffer.getSample (channel, sample)));
    return result;
}

void addRpn (juce::MidiBuffer& midi, int channel, int parameter,
             int valueMsb, int valueLsb = -1, int sample = 0)
{
    midi.addEvent (juce::MidiMessage::controllerEvent (
        channel, 101, (parameter >> 7) & 0x7f), sample);
    midi.addEvent (juce::MidiMessage::controllerEvent (
        channel, 100, parameter & 0x7f), sample);
    midi.addEvent (juce::MidiMessage::controllerEvent (
        channel, 6, std::clamp (valueMsb, 0, 127)), sample);
    if (valueLsb >= 0)
        midi.addEvent (juce::MidiMessage::controllerEvent (
            channel, 38, std::clamp (valueLsb, 0, 127)), sample);
}

void addLowerZone (juce::MidiBuffer& midi, int memberCount, int sample = 0)
{
    addRpn (midi, 1, juce::MPEMessages::zoneLayoutMessagesRpnNumber,
            memberCount, -1, sample);
}

std::vector<float> flattened (const juce::AudioBuffer<float>& audio)
{
    std::vector<float> result;
    result.reserve (static_cast<std::size_t> (
        audio.getNumChannels() * audio.getNumSamples()));
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        result.insert (result.end(), audio.getReadPointer (channel),
                       audio.getReadPointer (channel) + audio.getNumSamples());
    return result;
}

void testParameterContract()
{
    AcustraAudioProcessor processor;
    namespace ids = acustra::parameters;

    constexpr std::array<const char*, ids::parameterCount> expectedIds {
        ids::shape, ids::bodyMaterial, ids::stringMaterial, ids::tuning,
        ids::stringAge, ids::pluckPosition, ids::touch, ids::bodyAmount,
        ids::stereoWidth, ids::output
    };
    constexpr std::array<float, ids::parameterCount> expectedDefaults {
        2.0f, 0.0f, 1.0f, 0.0f, 15.0f, 28.0f, 58.0f, 82.0f, 62.0f, -7.5f
    };

    const auto& hostParameters = processor.getParameters();
    expect (hostParameters.size() == ids::parameterCount,
            "the public parameter count changed");
    for (int index = 0;
         index < std::min (hostParameters.size(), ids::parameterCount); ++index)
    {
        const auto* ranged =
            dynamic_cast<const juce::RangedAudioParameter*> (hostParameters[index]);
        expect (ranged != nullptr, "a public parameter is not ranged");
        if (ranged == nullptr)
            continue;
        expect (ranged->paramID == expectedIds[static_cast<std::size_t> (index)],
                "the stable parameter order or ID changed at slot "
                    + std::to_string (index));
        const float factory = ranged->convertFrom0to1 (ranged->getDefaultValue());
        expect (std::abs (factory
                          - expectedDefaults[static_cast<std::size_t> (index)])
                    < 0.011f,
                "the factory default changed for " + ranged->paramID.toStdString());
    }

    const auto* shape = dynamic_cast<const juce::AudioParameterChoice*> (
        processor.parameters.getParameter (ids::shape));
    const auto* wood = dynamic_cast<const juce::AudioParameterChoice*> (
        processor.parameters.getParameter (ids::bodyMaterial));
    const auto* strings = dynamic_cast<const juce::AudioParameterChoice*> (
        processor.parameters.getParameter (ids::stringMaterial));
    const auto* tuning = dynamic_cast<const juce::AudioParameterChoice*> (
        processor.parameters.getParameter (ids::tuning));
    expect (shape != nullptr && shape->choices.size() == 4,
            "Shape does not expose four bodies");
    expect (wood != nullptr && wood->choices.size() == 4,
            "Body Material does not expose four woods");
    expect (strings != nullptr && strings->choices
                == juce::StringArray { "Nylon", "Steel" },
            "String Material no longer exposes Nylon and Steel");
    expect (tuning != nullptr && tuning->choices.size() == 5,
            "Tuning does not expose the five supported tunings");

    setValue (processor, ids::shape, 3.0f);
    setValue (processor, ids::bodyMaterial, 2.0f);
    setValue (processor, ids::stringMaterial, 0.0f);
    setValue (processor, ids::tuning, 2.0f);
    setValue (processor, ids::stringAge, 73.0f);
    setValue (processor, ids::pluckPosition, 41.0f);
    setValue (processor, ids::touch, 19.0f);
    setValue (processor, ids::bodyAmount, 66.0f);
    setValue (processor, ids::stereoWidth, 35.0f);
    setValue (processor, ids::output, -3.0f);
    const auto engine = processor.snapshotEngineParameters();
    expect (engine.shape == acustra::BodyShape::Jumbo
                && engine.bodyMaterial == acustra::BodyMaterial::Mahogany
                && engine.stringMaterial == acustra::StringMaterial::Nylon
                && engine.tuning == acustra::Tuning::Dadgad,
            "choice parameters did not reach the engine snapshot");
    expect (std::abs (engine.stringAge - 0.73f) < 0.002f
                && std::abs (engine.pluckPosition - 0.41f) < 0.002f
                && std::abs (engine.touch - 0.19f) < 0.002f
                && std::abs (engine.bodyAmount - 0.66f) < 0.002f
                && std::abs (engine.stereoWidth - 0.35f) < 0.002f,
            "continuous parameters did not reach the engine snapshot");
    expect (std::abs (engine.outputGain
                      - juce::Decibels::decibelsToGain (-3.0f)) < 0.001f,
            "Output was not converted from dB to linear gain");
}

void testProcessorContractAndSampleAccurateMidi()
{
    AcustraAudioProcessor processor;
    expect (processor.acceptsMidi() && ! processor.producesMidi()
                && ! processor.isMidiEffect() && processor.hasEditor()
                && processor.supportsMPE(),
            "processor MIDI/editor capabilities are wrong");
    expect (std::abs (processor.getTailLengthSeconds() - 30.0) < 1.0e-9,
            "the host tail declaration changed");
    expect (processor.getTotalNumInputChannels() == 0
                && processor.getTotalNumOutputChannels() == 2,
            "Acustra must remain a no-input stereo instrument");

    auto mono = processor.getBusesLayout();
    mono.outputBuses.set (0, juce::AudioChannelSet::mono());
    expect (! processor.isBusesLayoutSupported (mono),
            "a mono main output was accepted");

    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.isEngineReady()
                && std::abs (processor.getCurrentSampleRateForDisplay()
                             - sampleRate) < 0.1,
            "prepareToPlay did not publish a ready 48 kHz engine");

    constexpr int onset = 91;
    juce::AudioBuffer<float> audio { 2, blockSize };
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, 0.9f), onset);
    processor.processBlock (audio, midi);
    expect (finite (audio), "processBlock produced NaN or infinity");
    expect (peak (audio, 0, onset) == 0.0f,
            "a note sounded before its MIDI sample offset");
    float onsetPeak = peak (audio, onset, blockSize);
    juce::MidiBuffer empty;
    audio.clear();
    processor.processBlock (audio, empty);
    constexpr int remainingFiveMillisecondWindow = 240 - (blockSize - onset);
    onsetPeak = std::max (onsetPeak,
                          peak (audio, 0, remainingFiveMillisecondWindow));
    expect (onsetPeak > 0.001f,
            "a playable E2 note-on did not render audible audio");
    expect (processor.getActiveVoiceCount() == 1,
            "a playable note did not own one physical string");

    juce::MidiBuffer panic;
    panic.addEvent (juce::MidiMessage::controllerEvent (1, 120, 0), 0);
    audio.clear();
    processor.processBlock (audio, panic);
    expect (processor.getActiveVoiceCount() == 0 && peak (audio) == 0.0f,
            "MIDI All Sound Off did not stop and clear the model");

    juce::MidiBuffer tooLow;
    tooLow.addEvent (juce::MidiMessage::noteOn (1, 20, 1.0f), 0);
    audio.clear();
    processor.processBlock (audio, tooLow);
    expect (processor.getActiveVoiceCount() == 0 && peak (audio) == 0.0f,
            "a note outside the physical guitar range was accepted");

    processor.releaseResources();
    expect (! processor.isEngineReady()
                && processor.getCurrentSampleRateForDisplay() == 0.0,
            "releaseResources left the engine advertised as ready");
}

std::vector<float> renderChord (const std::array<int, 6>& notes,
                                int& activeVoiceCount)
{
    AcustraAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio { 2, blockSize };
    juce::MidiBuffer midi;
    for (const int note : notes)
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 37);
    processor.processBlock (audio, midi);

    std::vector<float> result;
    result.reserve (static_cast<std::size_t> (2 * blockSize));
    for (int channel = 0; channel < 2; ++channel)
        result.insert (result.end(), audio.getReadPointer (channel),
                       audio.getReadPointer (channel) + blockSize);
    activeVoiceCount = processor.getActiveVoiceCount();
    processor.releaseResources();
    return result;
}

void testSameSampleChordOrderIsCanonical()
{
    int ascendingVoices = 0;
    int descendingVoices = 0;
    const auto ascending = renderChord ({ 45, 50, 55, 60, 64, 69 },
                                        ascendingVoices);
    const auto descending = renderChord ({ 69, 64, 60, 55, 50, 45 },
                                         descendingVoices);
    expect (ascending == descending,
            "same-sample chord sound depended on host insertion order");
    expect (ascendingVoices == 6 && descendingVoices == 6,
            "a playable six-string voicing lost a chord member");
    expect (std::any_of (ascending.begin(), ascending.end(), [] (float value)
            { return std::abs (value) > 0.001f; }),
            "the chord determinism check rendered silence");
}

void testSameSampleNoteOnOffDoesNotStick()
{
    const auto renderOneShot = [] (bool noteOnFirst)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::MidiBuffer midi;
        const auto on = juce::MidiMessage::noteOn (1, 52, 0.8f);
        const auto off = juce::MidiMessage::noteOff (1, 52);
        midi.addEvent (noteOnFirst ? on : off, 0);
        midi.addEvent (noteOnFirst ? off : on, 0);
        processor.processBlock (audio, midi);
        for (int block = 0; block < 100; ++block)
        {
            juce::MidiBuffer empty;
            processor.processBlock (audio, empty);
        }
        return processor.getActiveVoiceCount();
    };

    expect (renderOneShot (true) == 0 && renderOneShot (false) == 0,
            "same-sample Note On/Off left a string held in one insertion order");
}

void testBridgeHandControllerReachesTheEngine()
{
    // CC2 is the bridge-hand pressure. It must shorten the note it is applied
    // to and leave the panel's parameters alone.
    const auto tailRms = [] (bool muted)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::MidiBuffer start;
        if (muted)
            start.addEvent (juce::MidiMessage::controllerEvent (1, 2, 127), 0);
        processor.processBlock (audio, start);
        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer empty;
            processor.processBlock (audio, empty);
        }
        juce::MidiBuffer note;
        note.addEvent (juce::MidiMessage::noteOn (1, 52, 0.85f), 0);
        processor.processBlock (audio, note);
        const int blocks = static_cast<int> (1.5 * sampleRate / blockSize);
        double energy = 0.0;
        int counted = 0;
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer empty;
            processor.processBlock (audio, empty);
            if (block < blocks / 2)
                continue;
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const double mono = 0.5 * (audio.getSample (0, sample)
                                         + audio.getSample (1, sample));
                energy += mono * mono;
                ++counted;
            }
        }
        return std::sqrt (energy / std::max (1, counted));
    };
    const double open = tailRms (false);
    const double muted = tailRms (true);
    expect (open > 1.0e-6, "the unmuted reference note was silent");
    expect (muted < 0.2 * open,
            "CC2 did not reach the engine as bridge-hand pressure");
}

void testLegatoControllerReachesTheEngine()
{
    // CC68 is MIDI's Legato Footswitch. With it down, a second note a
    // sounding string can reach must be hammered on rather than replucked,
    // so it arrives without a pluck's attack.
    const auto arrivalRise = [] (bool legato)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        const auto sweep = [&] (double seconds, juce::MidiBuffer& first)
        {
            double peak = 0.0;
            const int blocks = std::max (1,
                static_cast<int> (seconds * sampleRate / blockSize));
            for (int block = 0; block < blocks; ++block)
            {
                juce::MidiBuffer empty;
                processor.processBlock (audio, block == 0 ? first : empty);
                for (int sample = 0; sample < blockSize; ++sample)
                    peak = std::max (peak, static_cast<double> (std::max (
                        std::abs (audio.getSample (0, sample)),
                        std::abs (audio.getSample (1, sample)))));
            }
            return peak;
        };
        juce::MidiBuffer start;
        if (legato)
            start.addEvent (juce::MidiMessage::controllerEvent (1, 68, 127), 0);
        start.addEvent (juce::MidiMessage::noteOn (1, 52, 0.85f), 1);
        sweep (0.5, start);
        juce::MidiBuffer none;
        const double before = sweep (0.1, none);
        juce::MidiBuffer second;
        second.addEvent (juce::MidiMessage::noteOn (1, 57, 0.85f), 0);
        const double after = sweep (0.2, second);
        return after / std::max (before, 1.0e-9);
    };
    const double plucked = arrivalRise (false);
    const double hammered = arrivalRise (true);
    expect (plucked > 2.0, "the replucked reference arrival did not rise");
    std::cout << "Acustra legato wrapper rise: plucked=" << plucked
              << " hammered=" << hammered << "\n";
    // The engine suite measures the mechanism itself, with the idle strings
    // muted and below the safety limiter. This one only has to prove the
    // controller arrives, through a default output gain that compresses both
    // arrivals and over a first note that is still ringing under them.
    expect (hammered < 0.7 * plucked,
            "CC68 did not reach the engine as legato");
}

void testResetAllControllersReleasesSustain()
{
    AcustraAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio { 2, blockSize };

    juce::MidiBuffer held;
    held.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);
    held.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 1);
    held.addEvent (juce::MidiMessage::noteOff (1, 52), 2);
    processor.processBlock (audio, held);
    expect (processor.getActiveVoiceCount() == 1,
            "sustain setup did not retain the released string");

    juce::MidiBuffer reset;
    reset.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
    processor.processBlock (audio, reset);
    for (int block = 0; block < 100; ++block)
    {
        juce::MidiBuffer empty;
        processor.processBlock (audio, empty);
    }
    expect (processor.getActiveVoiceCount() == 0,
            "Reset All Controllers left sustain latched");
}

void testMemberChannelOwnershipAndControllers()
{
    AcustraAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio { 2, blockSize };
    const auto render = [&] (juce::MidiBuffer& midi)
    {
        audio.clear();
        processor.processBlock (audio, midi);
    };
    const auto renderTail = [&] (int blocks)
    {
        juce::MidiBuffer empty;
        for (int block = 0; block < blocks; ++block)
            render (empty);
    };
    const auto soundOff = [&] (int channel)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 120, 0), 0);
        render (midi);
    };

    juce::MidiBuffer samePitch;
    samePitch.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    samePitch.addEvent (juce::MidiMessage::noteOn (3, 52, 0.8f), 0);
    render (samePitch);
    expect (processor.getActiveVoiceCount() == 2,
            "same-pitch member notes did not receive separate string ownership");
    soundOff (2);
    expect (processor.getActiveVoiceCount() == 1,
            "member All Sound Off affected another channel");
    soundOff (3);
    expect (processor.getActiveVoiceCount() == 0,
            "member All Sound Off missed its owned string");
    soundOff (1);

    // A channel-scoped boundary controller cancels only pending notes on its
    // own channel, regardless of same-sample host insertion order.
    juce::MidiBuffer boundary;
    boundary.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    boundary.addEvent (juce::MidiMessage::noteOn (3, 52, 0.8f), 0);
    boundary.addEvent (juce::MidiMessage::controllerEvent (2, 123, 0), 0);
    render (boundary);
    expect (processor.getActiveVoiceCount() == 1,
            "member All Notes Off affected another pending channel (active="
                + std::to_string (processor.getActiveVoiceCount()) + ")");
    soundOff (3);

    juce::MidiBuffer held;
    held.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    held.addEvent (juce::MidiMessage::controllerEvent (2, 64, 127), 1);
    held.addEvent (juce::MidiMessage::noteOff (2, 52), 2);
    render (held);
    renderTail (400);
    expect (processor.getActiveVoiceCount() == 1,
            "member sustain did not retain its released owner");
    juce::MidiBuffer unrelatedReset;
    unrelatedReset.addEvent (juce::MidiMessage::controllerEvent (3, 121, 0), 0);
    render (unrelatedReset);
    expect (processor.getActiveVoiceCount() == 1,
            "member Reset All Controllers affected another channel (active="
                + std::to_string (processor.getActiveVoiceCount()) + ")");
    juce::MidiBuffer ownerReset;
    ownerReset.addEvent (juce::MidiMessage::controllerEvent (2, 121, 0), 0);
    render (ownerReset);
    renderTail (400);
    expect (processor.getActiveVoiceCount() == 0,
            "member Reset All Controllers did not release its sustain");
    soundOff (1);

    juce::MidiBuffer memberIsolation;
    memberIsolation.addEvent (
        juce::MidiMessage::controllerEvent (2, 64, 127), 0);
    memberIsolation.addEvent (juce::MidiMessage::noteOn (3, 52, 0.8f), 1);
    memberIsolation.addEvent (juce::MidiMessage::noteOff (3, 52), 2);
    render (memberIsolation);
    renderTail (400);
    expect (processor.getActiveVoiceCount() == 0,
            "member sustain leaked to another channel");
    juce::MidiBuffer memberPedalUp;
    memberPedalUp.addEvent (
        juce::MidiMessage::controllerEvent (2, 64, 0), 0);
    render (memberPedalUp);

    // Before an RPN 6 MCM actually creates a zone, channel 1 is ordinary
    // MIDI and must not sustain or control channel 3.
    juce::MidiBuffer conventionalMaster;
    conventionalMaster.addEvent (
        juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    conventionalMaster.addEvent (juce::MidiMessage::noteOn (3, 52, 0.8f), 1);
    conventionalMaster.addEvent (juce::MidiMessage::noteOff (3, 52), 2);
    render (conventionalMaster);
    renderTail (400);
    expect (processor.getActiveVoiceCount() == 0,
            "conventional channel 1 acted as an MPE master before RPN 6");
    juce::MidiBuffer conventionalPedalUp;
    conventionalPedalUp.addEvent (
        juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    render (conventionalPedalUp);

    juce::MidiBuffer zoneSetup;
    addLowerZone (zoneSetup, 2);
    render (zoneSetup);

    juce::MidiBuffer masterHeld;
    masterHeld.addEvent (juce::MidiMessage::noteOn (3, 52, 0.8f), 0);
    masterHeld.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 1);
    masterHeld.addEvent (juce::MidiMessage::noteOff (3, 52), 2);
    render (masterHeld);
    renderTail (400);
    expect (processor.getActiveVoiceCount() == 1,
            "channel 1 sustain did not act as the MPE master (active="
                + std::to_string (processor.getActiveVoiceCount()) + ")");

    juce::MidiBuffer masterReset;
    masterReset.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
    render (masterReset);
    renderTail (400);
    expect (processor.getActiveVoiceCount() == 0,
            "channel 1 Reset All Controllers did not release master sustain");
}

void testMemberPitchBendDoesNotLeakChannels()
{
    const auto render = [] (int bendChannel, int wheel, bool mpe)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::MidiBuffer midi;
        if (mpe)
            addLowerZone (midi, 2);
        midi.addEvent (juce::MidiMessage::pitchWheel (bendChannel, wheel), 0);
        midi.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
        processor.processBlock (audio, midi);

        std::vector<float> result;
        result.reserve (2 * blockSize);
        for (int channel = 0; channel < 2; ++channel)
            result.insert (result.end(), audio.getReadPointer (channel),
                           audio.getReadPointer (channel) + blockSize);
        return result;
    };

    const auto baseline = render (2, 8192, false);
    const auto unrelated = render (3, 10240, false);
    const auto conventionalMaster = render (1, 10240, false);
    const auto owned = render (2, 10240, false);
    const auto mpeMaster = render (1, 10240, true);
    float unrelatedDifference = 0.0f;
    float conventionalMasterDifference = 0.0f;
    float ownedDifference = 0.0f;
    float mpeMasterDifference = 0.0f;
    for (std::size_t sample = 0; sample < baseline.size(); ++sample)
    {
        unrelatedDifference = std::max (
            unrelatedDifference,
            std::abs (unrelated[sample] - baseline[sample]));
        conventionalMasterDifference = std::max (
            conventionalMasterDifference,
            std::abs (conventionalMaster[sample] - baseline[sample]));
        ownedDifference = std::max (
            ownedDifference,
            std::abs (owned[sample] - baseline[sample]));
        mpeMasterDifference = std::max (
            mpeMasterDifference,
            std::abs (mpeMaster[sample] - baseline[sample]));
    }
    expect (unrelatedDifference == 0.0f
                && conventionalMasterDifference == 0.0f
                && ownedDifference > 1.0e-6f
                && mpeMasterDifference > 1.0e-6f,
            "channel bend isolation/master routing did not follow zone state");
}

void testRpnPitchRangesAndSelectionState()
{
    const auto renderConventional = [] (int variant)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::MidiBuffer midi;
        addRpn (midi, 2, 0, variant == 1 ? 2 : variant == 5 ? 96 : 2,
                variant == 1 || variant == 5 ? -1 : 50);
        if (variant == 2)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (2, 101, 127), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (2, 100, 127), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (2, 38, 99), 0);
        }
        else if (variant == 3)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (2, 99, 0), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (2, 98, 0), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (2, 6, 12), 0);
        }
        else if (variant == 4)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (2, 121, 0), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (2, 38, 99), 0);
        }
        midi.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
        midi.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
        processor.processBlock (audio, midi);
        return std::pair { flattened (audio), finite (audio) };
    };

    const auto exact = renderConventional (0);
    const auto whole = renderConventional (1);
    const auto rpnNull = renderConventional (2);
    const auto nrpn = renderConventional (3);
    const auto reset = renderConventional (4);
    const auto maximum = renderConventional (5);
    expect (exact.first != whole.first,
            "RPN 0 Data Entry LSB did not refine pitch sensitivity");
    expect (exact.first == rpnNull.first && exact.first == nrpn.first
                && exact.first == reset.first,
            "RPN Null/NRPN/CC121 did not preserve the configured exact range");
    expect (maximum.second && maximum.first != whole.first,
            "the legal 96-semitone RPN 0 range was ignored or became non-finite");

    const auto renderSharedMember = [] (int rangeChannel, bool configureRange,
                                        bool resetMaster)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::MidiBuffer midi;
        addLowerZone (midi, 2);
        if (configureRange)
            addRpn (midi, rangeChannel, 0, 5, 25);
        if (resetMaster)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (
                rangeChannel, 38, 99), 0);
        }
        midi.addEvent (juce::MidiMessage::pitchWheel (3, 16383), 0);
        midi.addEvent (juce::MidiMessage::noteOn (3, 52, 0.8f), 0);
        processor.processBlock (audio, midi);
        return flattened (audio);
    };

    const auto memberFromTwo = renderSharedMember (2, true, false);
    const auto memberFromThree = renderSharedMember (3, true, false);
    const auto memberAfterReset = renderSharedMember (2, true, true);
    const auto defaultMember = renderSharedMember (2, false, false);
    expect (memberFromTwo == memberFromThree
                && memberFromTwo == memberAfterReset
                && memberFromTwo != defaultMember,
            "MPE member RPN 0 was not shared or survived CC121 incorrectly");

    const auto renderConventionalCrossChannel = [] (int rangeChannel)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::MidiBuffer midi;
        if (rangeChannel != 0)
            addRpn (midi, rangeChannel, 0, 5, 0);
        midi.addEvent (juce::MidiMessage::pitchWheel (3, 16383), 0);
        midi.addEvent (juce::MidiMessage::noteOn (3, 52, 0.8f), 0);
        processor.processBlock (audio, midi);
        return flattened (audio);
    };
    const auto conventionalDefault = renderConventionalCrossChannel (0);
    expect (renderConventionalCrossChannel (2) == conventionalDefault
                && renderConventionalCrossChannel (3) != conventionalDefault,
            "conventional RPN 0 range was not channel-scoped");
}

void testLowerZoneLifecycleAndControllerBoundaries()
{
    AcustraAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio { 2, blockSize };
    const auto render = [&] (juce::MidiBuffer& midi)
    {
        audio.clear();
        processor.processBlock (audio, midi);
    };

    juce::MidiBuffer notes;
    notes.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    notes.addEvent (juce::MidiMessage::noteOn (8, 55, 0.8f), 0);
    render (notes);
    juce::MidiBuffer enable;
    addLowerZone (enable, 2);
    render (enable);
    expect (processor.getActiveVoiceCount() == 1,
            "lower-zone enable did not stop exactly the newly affected channels");
    juce::MidiBuffer resize;
    addLowerZone (resize, 7);
    render (resize);
    expect (processor.getActiveVoiceCount() == 0,
            "lower-zone resize did not stop the expanded affected union");
    juce::MidiBuffer member;
    member.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    render (member);
    juce::MidiBuffer disable;
    addLowerZone (disable, 0);
    render (disable);
    expect (processor.getActiveVoiceCount() == 0,
            "lower-zone deactivation left an old member voice sounding");

    AcustraAudioProcessor nrpnProcessor;
    nrpnProcessor.prepareToPlay (sampleRate, blockSize);
    juce::MidiBuffer nrpnNotes;
    nrpnNotes.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    nrpnNotes.addEvent (juce::MidiMessage::noteOn (8, 55, 0.8f), 0);
    nrpnProcessor.processBlock (audio, nrpnNotes);
    juce::MidiBuffer nrpnLayout;
    nrpnLayout.addEvent (juce::MidiMessage::controllerEvent (1, 99, 0), 0);
    nrpnLayout.addEvent (juce::MidiMessage::controllerEvent (1, 98, 6), 0);
    nrpnLayout.addEvent (juce::MidiMessage::controllerEvent (1, 6, 2), 0);
    nrpnProcessor.processBlock (audio, nrpnLayout);
    expect (nrpnProcessor.getActiveVoiceCount() == 2,
            "NRPN 6 was incorrectly accepted as an MPE layout RPN");

    for (const int controller : { 120, 123 })
        for (const bool controllerFirst : { false, true })
        {
            AcustraAudioProcessor boundary;
            boundary.prepareToPlay (sampleRate, blockSize);
            juce::MidiBuffer setup;
            addLowerZone (setup, 2);
            boundary.processBlock (audio, setup);
            juce::MidiBuffer events;
            const auto control = juce::MidiMessage::controllerEvent (
                1, controller, 0);
            const auto affected = juce::MidiMessage::noteOn (2, 52, 0.8f);
            const auto outside = juce::MidiMessage::noteOn (8, 55, 0.8f);
            events.addEvent (controllerFirst ? control : affected, 0);
            events.addEvent (outside, 0);
            events.addEvent (controllerFirst ? affected : control, 0);
            boundary.processBlock (audio, events);
            expect (boundary.getActiveVoiceCount() == 1,
                    "same-sample CC" + std::to_string (controller)
                        + " failed to cancel zone Note Ons in one insertion order");
        }

    for (const int controller : { 120, 123 })
        for (const bool controllerFirst : { false, true })
        {
            AcustraAudioProcessor boundary;
            boundary.prepareToPlay (sampleRate, blockSize);
            juce::MidiBuffer events;
            const auto control = juce::MidiMessage::controllerEvent (
                2, controller, 0);
            const auto affected = juce::MidiMessage::noteOn (2, 52, 0.8f);
            const auto outside = juce::MidiMessage::noteOn (3, 55, 0.8f);
            events.addEvent (controllerFirst ? control : affected, 0);
            events.addEvent (outside, 0);
            events.addEvent (controllerFirst ? affected : control, 0);
            boundary.processBlock (audio, events);
            expect (boundary.getActiveVoiceCount() == 1,
                    "same-sample conventional CC" + std::to_string (controller)
                        + " cancellation crossed channels or depended on order");
        }
}

void testControllerResetSoundOffAndUiPanic()
{
    const auto renderTail = [] (AcustraAudioProcessor& processor,
                                juce::AudioBuffer<float>& audio, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer empty;
            processor.processBlock (audio, empty);
        }
    };

    AcustraAudioProcessor preserved;
    preserved.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio { 2, blockSize };
    juce::MidiBuffer setup;
    setup.addEvent (juce::MidiMessage::controllerEvent (2, 64, 127), 0);
    setup.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
    setup.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    preserved.processBlock (audio, setup);
    juce::MidiBuffer soundOff;
    soundOff.addEvent (juce::MidiMessage::controllerEvent (2, 120, 0), 0);
    preserved.processBlock (audio, soundOff);
    expect (preserved.getActiveVoiceCount() == 0,
            "CC120 did not silence its conventional channel");
    juce::MidiBuffer afterSoundOff;
    afterSoundOff.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    afterSoundOff.addEvent (juce::MidiMessage::noteOff (2, 52), 1);
    preserved.processBlock (audio, afterSoundOff);
    renderTail (preserved, audio, 400);
    expect (preserved.getActiveVoiceCount() == 1,
            "CC120 reset sustain instead of preserving controller state");
    juce::MidiBuffer pedalUp;
    pedalUp.addEvent (juce::MidiMessage::controllerEvent (2, 64, 0), 0);
    preserved.processBlock (audio, pedalUp);
    renderTail (preserved, audio, 400);
    expect (preserved.getActiveVoiceCount() == 0,
            "preserved CC120 sustain did not release on pedal-up");

    AcustraAudioProcessor panic;
    panic.prepareToPlay (sampleRate, blockSize);
    juce::MidiBuffer pedalDown;
    pedalDown.addEvent (juce::MidiMessage::controllerEvent (2, 64, 127), 0);
    panic.processBlock (audio, pedalDown);
    panic.requestPanic();
    juce::MidiBuffer empty;
    panic.processBlock (audio, empty);
    juce::MidiBuffer afterPanic;
    afterPanic.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    afterPanic.addEvent (juce::MidiMessage::noteOff (2, 52), 1);
    panic.processBlock (audio, afterPanic);
    renderTail (panic, audio, 400);
    expect (panic.getActiveVoiceCount() == 0,
            "front-panel Panic failed to reset controller state fully");

    AcustraAudioProcessor masterReset;
    masterReset.prepareToPlay (sampleRate, blockSize);
    juce::MidiBuffer mpe;
    addLowerZone (mpe, 2);
    mpe.addEvent (juce::MidiMessage::noteOn (2, 52, 0.8f), 0);
    mpe.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 1);
    mpe.addEvent (juce::MidiMessage::noteOn (3, 56, 0.8f), 2);
    mpe.addEvent (juce::MidiMessage::noteOff (3, 56), 3);
    masterReset.processBlock (audio, mpe);
    const int beforeResetCount = masterReset.getActiveVoiceCount();
    juce::MidiBuffer reset;
    reset.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
    masterReset.processBlock (audio, reset);
    const int afterResetCount = masterReset.getActiveVoiceCount();
    juce::MidiBuffer removeTail;
    removeTail.addEvent (juce::MidiMessage::controllerEvent (3, 120, 0), 0);
    masterReset.processBlock (audio, removeTail);
    expect (beforeResetCount == 2 && afterResetCount == 2
                && masterReset.getActiveVoiceCount() == 1,
            "MPE master CC121 released a held key or reset the wrong zone state "
                "(before=" + std::to_string (beforeResetCount)
                + ", after=" + std::to_string (afterResetCount)
                + ", after member CC120="
                + std::to_string (masterReset.getActiveVoiceCount()) + ")");
}

void testStateRoundTripAndMigration()
{
    namespace ids = acustra::parameters;
    AcustraAudioProcessor source;
    setValue (source, ids::shape, 0.0f);
    setValue (source, ids::bodyMaterial, 3.0f);
    setValue (source, ids::stringMaterial, 0.0f);
    setValue (source, ids::tuning, 4.0f);
    setValue (source, ids::stringAge, 87.0f);
    setValue (source, ids::pluckPosition, 64.0f);
    setValue (source, ids::output, -2.4f);

    juce::MemoryBlock stored;
    source.getStateInformation (stored);
    expect (stored.getSize() > 0, "getStateInformation returned no state");

    AcustraAudioProcessor restored;
    restored.setStateInformation (stored.getData(),
                                  static_cast<int> (stored.getSize()));
    for (const char* id : { ids::shape, ids::bodyMaterial, ids::stringMaterial,
                            ids::tuning, ids::stringAge, ids::pluckPosition,
                            ids::output })
        expect (std::abs (valueOf (restored, id) - valueOf (source, id)) < 0.011f,
                std::string { "state round trip lost " } + id);

    // A session saved before later controls existed must receive their factory
    // defaults, not whatever values happen to be live in the destination.
    setValue (restored, ids::stringAge, 99.0f);
    setValue (restored, ids::output, 5.0f);
    juce::ValueTree oldState { restored.parameters.state.getType() };
    juce::ValueTree shape { "PARAM" };
    shape.setProperty ("id", ids::shape, nullptr);
    shape.setProperty ("value", 1.0f, nullptr);
    oldState.appendChild (shape, nullptr);
    juce::MemoryBlock oldBytes;
    if (const auto xml = oldState.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, oldBytes);
    restored.setStateInformation (oldBytes.getData(),
                                  static_cast<int> (oldBytes.getSize()));
    expect (valueOf (restored, ids::shape) == 1.0f,
            "a retained parameter was not restored from an old state");
    expect (std::abs (valueOf (restored, ids::stringAge) - 15.0f) < 0.011f
                && std::abs (valueOf (restored, ids::output) + 7.5f) < 0.011f,
            "parameters absent from an old state did not receive defaults");

    const char garbage[] = "not an Acustra state";
    restored.setStateInformation (garbage, static_cast<int> (sizeof garbage));
    expect (valueOf (restored, ids::shape) == 1.0f,
            "invalid host state was applied instead of ignored");
}

void testEditorRendering()
{
    AcustraAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> editor { processor.createEditor() };
    expect (editor != nullptr, "createEditor returned null");
    if (editor == nullptr)
        return;

    expect (editor->getWidth() == editorWidth && editor->getHeight() == editorHeight,
            "the editor did not open at its documented design size");
    expect (editor->isResizable(), "the editor is not host-resizable");
    expect (editor->getNumChildComponents() >= 18,
            "the editor is missing controls or the keyboard");

    std::vector<juce::Component*> pending { editor.get() };
    std::vector<juce::TextButton*> choiceButtons;
    juce::MidiKeyboardComponent* midiKeyboard = nullptr;
    int comboBoxCount = 0;
    while (! pending.empty())
    {
        auto* parent = pending.back();
        pending.pop_back();
        for (auto* child : parent->getChildren())
        {
            pending.push_back (child);
            if (dynamic_cast<juce::ComboBox*> (child) != nullptr)
                ++comboBoxCount;
            if (auto* button = dynamic_cast<juce::TextButton*> (child);
                button != nullptr && button->getRadioGroupId() != 0)
                choiceButtons.push_back (button);
            if (auto* candidate = dynamic_cast<juce::MidiKeyboardComponent*> (child))
                midiKeyboard = candidate;
        }
    }

    expect (comboBoxCount == 0,
            "a compact choice is still hidden in a combo box");
    expect (choiceButtons.size() == 15,
            "the four compact choices do not expose all 15 options");
    expect (std::all_of (choiceButtons.begin(), choiceButtons.end(),
                        [] (const auto* button)
                        {
                            return button->getWantsKeyboardFocus()
                                && button->isToggleable();
                        }),
            "a choice button is not keyboard-focusable and toggle-accessible");

    std::set<juce::Component*> choiceGroups;
    for (auto* button : choiceButtons)
        choiceGroups.insert (button->getParentComponent());
    expect (choiceGroups.size() == 4,
            "choice buttons are not split into four radio groups");
    for (auto* group : choiceGroups)
    {
        const auto selected = std::count_if (
            choiceButtons.begin(), choiceButtons.end(), [group] (const auto* button)
            {
                return button->getParentComponent() == group
                    && button->getToggleState();
            });
        expect (selected == 1,
                "a choice radio group does not have exactly one selection");
    }

    const auto dreadnought = std::find_if (
        choiceButtons.begin(), choiceButtons.end(), [] (const auto* button)
        {
            return button->getName() == "BODY SHAPE: Dreadnought";
        });
    expect (dreadnought != choiceButtons.end(),
            "the Dreadnought body-shape switch is missing");
    if (dreadnought != choiceButtons.end())
    {
        setValue (processor, acustra::parameters::shape, 1.0f);
        (*dreadnought)->setToggleState (true, juce::sendNotification);
        expect (std::abs (valueOf (processor, acustra::parameters::shape) - 2.0f)
                    < 0.011f,
                "a choice button did not update its host parameter");
        setValue (processor, acustra::parameters::shape, 0.0f);
        const auto parlor = std::find_if (
            choiceButtons.begin(), choiceButtons.end(), [] (const auto* button)
            {
                return button->getName() == "BODY SHAPE: Parlor";
            });
        expect (parlor != choiceButtons.end() && (*parlor)->getToggleState(),
                "host automation did not update the visible choice selection");
    }

    const auto renderAt = [&] (int width, int height)
    {
        editor->setSize (width, height);
        expect (midiKeyboard != nullptr && midiKeyboard->getBottom() == height,
                "the MIDI keyboard is not anchored to the editor bottom edge");
        juce::Image image { juce::Image::ARGB, width, height, true };
        juce::Graphics graphics { image };
        editor->paintEntireComponent (graphics, true);

        std::set<juce::uint32> colours;
        bool opaque = true;
        for (int y = 4; y < height; y += 11)
            for (int x = 4; x < width; x += 11)
            {
                const auto pixel = image.getPixelAt (x, y);
                colours.insert (pixel.getARGB());
                opaque = opaque && pixel.getAlpha() >= 250;
            }
        expect (opaque, "the editor rendered transparent holes");
        expect (colours.size() > 96u,
                "the editor lost its visual panels or control detail");
        return image;
    };

    renderAt (editorMinimumWidth, editorMinimumHeight);
    renderAt (editorMaximumWidth, editorMaximumHeight);
    auto image = renderAt (editorWidth, editorHeight);

    const auto path = juce::SystemStats::getEnvironmentVariable (
        "ACUSTRA_EDITOR_SNAPSHOT", {});
    if (path.isNotEmpty())
    {
        const juce::File destination { path };
        destination.getParentDirectory().createDirectory();
        juce::FileOutputStream output { destination };
        juce::PNGImageFormat png;
        const bool ready = output.openedOk() && output.setPosition (0)
            && output.truncate();
        const bool written = ready && png.writeImageToStream (image, output);
        output.flush();
        expect (written, "the requested editor screenshot could not be written");
    }

    editor.reset();
    processor.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    testParameterContract();
    testProcessorContractAndSampleAccurateMidi();
    testSameSampleChordOrderIsCanonical();
    testSameSampleNoteOnOffDoesNotStick();
    testBridgeHandControllerReachesTheEngine();
    testLegatoControllerReachesTheEngine();
    testResetAllControllersReleasesSustain();
    testMemberChannelOwnershipAndControllers();
    testMemberPitchBendDoesNotLeakChannels();
    testRpnPitchRangesAndSelectionState();
    testLowerZoneLifecycleAndControllerBoundaries();
    testControllerResetSoundOffAndUiPanic();
    testStateRoundTripAndMigration();
    testEditorRendering();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Acustra processor contract test(s) failed\n";
        return 1;
    }

    std::cout << "All Acustra processor contract tests passed\n";
    return 0;
}
