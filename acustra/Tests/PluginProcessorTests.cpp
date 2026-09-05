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
constexpr int editorHeight = 800;
constexpr int editorMinimumWidth = 896;
constexpr int editorMinimumHeight = 640;
constexpr int editorMaximumWidth = 1456;
constexpr int editorMaximumHeight = 1040;

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
        ids::stereoWidth, ids::output, ids::capture, ids::picking, ids::bridgeModel,
        ids::upperMic, ids::piezoLoading
    };
    constexpr std::array<float, ids::parameterCount> expectedDefaults {
        2.0f, 0.0f, 1.0f, 0.0f, 15.0f, 28.0f, 58.0f, 82.0f, 62.0f, -7.5f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f
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
    const auto* capture = dynamic_cast<const juce::AudioParameterChoice*> (
        processor.parameters.getParameter (ids::capture));
    const auto* picking = dynamic_cast<const juce::AudioParameterChoice*> (
        processor.parameters.getParameter (ids::picking));
    const auto* upperMic = dynamic_cast<const juce::AudioParameterBool*> (
        processor.parameters.getParameter (ids::upperMic));
    const auto* piezoLoading = dynamic_cast<const juce::AudioParameterBool*> (
        processor.parameters.getParameter (ids::piezoLoading));
    expect (shape != nullptr && shape->choices.size() == 4,
            "Shape does not expose four bodies");
    expect (wood != nullptr && wood->choices.size() == 4,
            "Body Material does not expose four woods");
    expect (strings != nullptr && strings->choices
                == juce::StringArray { "Nylon", "Steel" },
            "String Material no longer exposes Nylon and Steel");
    expect (tuning != nullptr && tuning->choices.size() == 5,
            "Tuning does not expose the five supported tunings");

    expect (capture != nullptr && capture->choices == juce::StringArray {
                "Stereo mics", "Treble mic", "Bass mic", "Saddle piezo",
                "Magnetic (steel)" },
            "the five legacy capture choices changed their automation contract");
    expect (upperMic != nullptr && upperMic->getVersionHint() == 4
                && upperMic->getParameterIndex() == 13,
            "Upper mic is not an appended boolean with AU version hint 4");
    expect (piezoLoading != nullptr && piezoLoading->getVersionHint() == 5
                && piezoLoading->getParameterIndex() == 14,
            "Piezo loading changed the legacy parameter order or AU version hint");
    expect (picking != nullptr && picking->choices
                == juce::StringArray { "Finger", "Pick", "Thumb" },
            "Picking does not expose finger, pick and thumb");

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
    setValue (processor, ids::capture, 3.0f);
    setValue (processor, ids::picking, 2.0f);
    setValue (processor, ids::bridgeModel, 1.0f);
    const auto engine = processor.snapshotEngineParameters();
    expect (engine.shape == acustra::BodyShape::Jumbo
                && engine.bodyMaterial == acustra::BodyMaterial::Mahogany
                && engine.stringMaterial == acustra::StringMaterial::Nylon
                && engine.tuning == acustra::Tuning::Dadgad
                && engine.capture == acustra::CaptureType::SaddlePiezo
                && engine.picking == acustra::PickingTechnique::Thumb
                && engine.bridgeModel == acustra::BridgeModel::FyldeSteel,
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

    // A sixth item in the old choice would reinterpret existing normalized
    // automation. Upper mic instead overrides that unchanged five-item value.
    auto* legacyCapture = processor.parameters.getParameter (ids::capture);
    if (legacyCapture != nullptr)
        for (int choice = 0; choice < 5; ++choice)
        {
            legacyCapture->setValueNotifyingHost (choice * 0.25f);
            expect (valueOf (processor, ids::capture) == static_cast<float> (choice)
                        && processor.snapshotEngineParameters().capture
                            == static_cast<acustra::CaptureType> (choice),
                    "legacy normalized capture automation changed meaning");
            setValue (processor, ids::upperMic, 1.0f);
            expect (processor.snapshotEngineParameters().capture
                        == acustra::CaptureType::UpperMic
                        && valueOf (processor, ids::capture) == static_cast<float> (choice),
                    "Upper mic did not override capture while retaining its saved value");
            setValue (processor, ids::upperMic, 0.0f);
            expect (processor.snapshotEngineParameters().capture
                        == static_cast<acustra::CaptureType> (choice),
                    "disabling Upper mic did not restore the legacy capture");
            setValue (processor, ids::piezoLoading, 1.0f);
            const auto loaded = choice == 3 ? acustra::CaptureType::LoadedPiezo
                : static_cast<acustra::CaptureType> (choice);
            expect (processor.snapshotEngineParameters().capture == loaded
                        && valueOf (processor, ids::capture) == static_cast<float> (choice),
                    "piezo loading altered a different capture or its legacy value");
            setValue (processor, ids::upperMic, 1.0f);
            expect (processor.snapshotEngineParameters().capture == acustra::CaptureType::UpperMic,
                    "piezo loading overrode the independent Upper mic switch");
            setValue (processor, ids::upperMic, 0.0f);
            expect (processor.snapshotEngineParameters().capture == loaded,
                    "Upper mic did not restore the underlying pickup loading");
            setValue (processor, ids::piezoLoading, 0.0f);
        }
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

double upperRegisterRise (const std::vector<float>& mono)
{
    // Five-millisecond heterodyne windows locate the rise around C6,
    // relative to first sound, to one millisecond. Use a clip-relative
    // level so the measured per-string dynamics do not set the threshold.
    std::size_t first = 0;
    while (first < mono.size() && std::abs (mono[first]) < 1.0e-5f)
        ++first;
    const double frequency = 440.0 * std::exp2 ((84.0 - 69.0) / 12.0);
    const auto window = static_cast<std::size_t> (0.005 * sampleRate);
    const auto hop = static_cast<std::size_t> (0.001 * sampleRate);
    const auto extent = static_cast<std::size_t> (0.08 * sampleRate);
    std::vector<double> level;
    for (std::size_t start = first; start + window <= mono.size()
                                    && start - first < extent; start += hop)
    {
        double real = 0.0;
        double imaginary = 0.0;
        for (std::size_t index = 0; index < window; ++index)
        {
            const double angle = 2.0 * juce::MathConstants<double>::pi
                * frequency * static_cast<double> (index) / sampleRate;
            real += mono[start + index] * std::cos (angle);
            imaginary += mono[start + index] * std::sin (angle);
        }
        level.push_back (std::hypot (real, imaginary));
    }
    expect (! level.empty(), "the strum direction probe rendered silence");
    if (level.empty())
        return 1.0;
    const double top = *std::max_element (level.begin(), level.end());
    for (std::size_t index = 0; index < level.size(); ++index)
        if (level[index] > 0.2 * top)
            return static_cast<double> (index) * 0.001;
    return 1.0;
}

void testSameSampleChordsAreStrummedAndAlternate()
{
    // Listen to upper-register energy in the rendered stereo output. C6 on
    // the top string separates down/up attacks; the old F#4 probe mistook
    // strong 349 Hz partials from the bass strings for its 370 Hz fundamental.
    // No engine scheduling state or private wrapper flags are inspected.
    AcustraAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio { 2, blockSize };
    const auto upperRegisterDelay = [&] (bool legato, double restSeconds)
    {
        juce::MidiBuffer chord;
        chord.addEvent (juce::MidiMessage::controllerEvent (1, 68, legato ? 127 : 0), 0);
        for (const int note : { 41, 46, 51, 56, 61, 84 })
            chord.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
        std::vector<float> mono;
        const int blocks = static_cast<int> (0.10 * sampleRate / blockSize);
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer empty;
            processor.processBlock (audio, block == 0 ? chord : empty);
            for (int sample = 0; sample < blockSize; ++sample)
                mono.push_back (0.5f * (audio.getSample (0, sample)
                                        + audio.getSample (1, sample)));
        }
        juce::MidiBuffer off;
        for (const int note : { 41, 46, 51, 56, 61, 84 })
            off.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        off.addEvent (juce::MidiMessage::controllerEvent (1, 68, 0), 0);
        const int restBlocks = static_cast<int> (restSeconds * sampleRate / blockSize);
        for (int block = 0; block < restBlocks; ++block)
        {
            juce::MidiBuffer empty;
            processor.processBlock (audio, block == 0 ? off : empty);
        }
        return upperRegisterRise (mono);
    };
    const auto median = [] (std::vector<double> values)
    {
        std::sort (values.begin(), values.end());
        return values[values.size() / 2];
    };
    std::vector<double> downs, ups, downAgains, hammereds;
    for (int stroke = 0; stroke < 8; ++stroke)
    {
        // The odd-length group ends on a downstroke: the next stroke must
        // restart down after the long rest, overriding its pending upstroke.
        downs.push_back (upperRegisterDelay (false, 0.5));
        ups.push_back (upperRegisterDelay (false, 0.5));
        downAgains.push_back (upperRegisterDelay (false, 2.5));
    }
    for (int group = 0; group < 4; ++group)
        hammereds.push_back (upperRegisterDelay (true, 2.5));
    const double down = median (downs);
    const double up = median (ups);
    const double downAgain = median (downAgains);
    const double restarted = median ({ downs.begin() + 1, downs.end() });
    const double hammered = median (hammereds);
    // Medians allow the existing measured stroke-speed and string-level
    // variation. At the shipping defaults they are 13/0/12/13/0 ms; the
    // direction margins are several analysis hops, not threshold rounding.
    // Off-tree wrapper mutations disabling alternation or the rest reset
    // must fail these audible checks; no golden audio is required.
    expect (downs.front() > 0.006,
            "the first same-sample chord did not sweep low to high");
    expect (up < 0.5 * down,
            "the return strum did not sweep high to low");
    expect (downAgain > 0.006 && downAgain > 2.0 * up,
            "the third strum did not alternate back to low to high");
    expect (restarted > 0.006 && restarted > 2.0 * up,
            "a long rest did not restart with a low-to-high strum");
    expect (hammered < 0.5 * down,
            "a legato group was swept like a strum");
    std::cout << "Acustra strum sweep medians: down " << down * 1000.0
              << " ms, up " << up * 1000.0 << " ms, down again "
              << downAgain * 1000.0 << " ms, restart " << restarted * 1000.0
              << " ms, legato " << hammered * 1000.0 << " ms\n";
}

void testRepeatedHeldChordsKeepTheirAudibleSweep()
{
    // Subtract a processor with identical preceding strokes but no new MIDI.
    // This removes the still-ringing chord from the onset measurement without
    // releasing any key or inspecting the engine's scheduling state.
    std::vector<double> downs, ups;
    for (int previousStrokes = 1; previousStrokes <= 6; ++previousStrokes)
    {
        AcustraAudioProcessor repeated, continuation;
        repeated.prepareToPlay (sampleRate, blockSize);
        continuation.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::AudioBuffer<float> held { 2, blockSize };
        const auto chord = []
        {
            juce::MidiBuffer midi;
            for (int note : { 41, 46, 51, 56, 61, 84 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
            return midi;
        };
        for (int stroke = 0; stroke < previousStrokes; ++stroke)
            for (int block = 0; block < 60; ++block)
            {
                auto midi = block == 0 ? chord() : juce::MidiBuffer {};
                auto matched = midi;
                repeated.processBlock (audio, midi);
                continuation.processBlock (held, matched);
            }
        expect (flattened (audio) == flattened (held),
                "held-chord continuation did not match before the repeated stroke");
        expect (repeated.getActiveVoiceCount() == 6,
                "the repeated-chord probe lost a held string");
        std::vector<float> difference;
        for (int block = 0; block < 18; ++block)
        {
            auto midi = block == 0 ? chord() : juce::MidiBuffer {};
            juce::MidiBuffer empty;
            repeated.processBlock (audio, midi);
            continuation.processBlock (held, empty);
            for (int sample = 0; sample < blockSize; ++sample)
                difference.push_back (0.5f * (
                    audio.getSample (0, sample) - held.getSample (0, sample)
                    + audio.getSample (1, sample) - held.getSample (1, sample)));
        }
        (previousStrokes % 2 == 0 ? downs : ups)
            .push_back (upperRegisterRise (difference));
    }
    std::sort (downs.begin(), downs.end());
    std::sort (ups.begin(), ups.end());
    const double down = downs[downs.size() / 2];
    const double up = ups[ups.size() / 2];
    expect (down > 0.006 && down > 2.0 * up,
            "repeated held chords struck together instead of sweeping in alternate directions");
    std::cout << "Acustra held-chord sweep medians: down " << down * 1000.0
              << " ms, up " << up * 1000.0 << " ms\n";
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

void testTheModulationWheelReachesTheEngineAsVibrato()
{
    // CC1 is MIDI's Modulation Wheel, which this instrument reads as the
    // fretting hand's vibrato (see AcustraEngine's vibratoSemitones for what
    // it is bounded by). Zero is the wheel untouched, down to the sample.
    const auto phrase = [] (int wheel, bool send)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        std::vector<float> mono;
        juce::MidiBuffer start;
        if (send)
            start.addEvent (juce::MidiMessage::controllerEvent (1, 1, wheel),
                            0);
        start.addEvent (juce::MidiMessage::noteOn (1, 52, 0.85f), 1);
        const int blocks = static_cast<int> (2.0 * sampleRate / blockSize);
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer empty;
            processor.processBlock (audio, block == 0 ? start : empty);
            for (int sample = 0; sample < blockSize; ++sample)
                mono.push_back (audio.getSample (0, sample));
        }
        return mono;
    };
    const auto untouched = phrase (0, false);
    const auto zeroed = phrase (0, true);
    const auto deep = phrase (127, true);
    expect (untouched == zeroed,
            "CC1 at zero changed the sound through the wrapper");
    expect (untouched.size() == deep.size() && untouched != deep,
            "CC1 did not reach the engine as vibrato");
    double largest = 0.0;
    for (std::size_t index = 0; index < untouched.size(); ++index)
        largest = std::max (largest, std::abs (
            static_cast<double> (deep[index] - untouched[index])));
    std::cout << "Acustra vibrato wrapper: largest sample difference "
              << largest << "\n";
}

void testLegatoControllerReachesTheEngine()
{
    // CC68 is MIDI's Legato Footswitch. With it down, a second note a
    // sounding string can reach is hammered on rather than replucked: the
    // string it was on is the string it stays on, so one voice sounds where
    // a repluck would have taken a second string.
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
        return std::pair { after / std::max (before, 1.0e-9),
                           processor.getActiveVoiceCount() };
    };
    const auto [plucked, pluckedVoices] = arrivalRise (false);
    const auto [hammered, hammeredVoices] = arrivalRise (true);
    expect (plucked > 2.0, "the replucked reference arrival did not rise");
    expect (hammered > 1.5, "the hammered arrival did not rise");
    std::cout << "Acustra legato wrapper rise: plucked=" << plucked
              << " hammered=" << hammered << "\n";
    // The engine suite measures the mechanism itself. This one only has to
    // prove the controller arrives: hammered on, the second note stays on
    // the first note's string instead of taking another.
    expect (pluckedVoices == 2 && hammeredVoices == 1,
            "CC68 did not reach the engine as legato");
}

void testReleaseVelocityReachesTheEngineAsAFingerLift()
{
    // Note-off velocity is how fast the fretting finger leaves the string.
    // MIDI's default when unsensed is 64, so 64 and a plain Note On at
    // velocity zero are exactly the release every host sent before; 127 lifts
    // the finger clear and the open string rings.
    const auto phrase = [] (const juce::MidiMessage& off)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        std::vector<float> mono;
        const auto sweep = [&] (double seconds, juce::MidiBuffer& first)
        {
            const int blocks = std::max (1,
                static_cast<int> (seconds * sampleRate / blockSize));
            for (int block = 0; block < blocks; ++block)
            {
                juce::MidiBuffer empty;
                processor.processBlock (audio, block == 0 ? first : empty);
                for (int sample = 0; sample < blockSize; ++sample)
                    mono.push_back (0.5f * (audio.getSample (0, sample)
                                            + audio.getSample (1, sample)));
            }
        };
        juce::MidiBuffer start;
        start.addEvent (juce::MidiMessage::noteOn (1, 43, 0.8f), 0);
        sweep (0.8, start);
        juce::MidiBuffer release;
        release.addEvent (off, 0);
        const auto releasedAt = mono.size();
        sweep (1.0, release);
        return std::pair { mono, releasedAt };
    };
    const auto tailEnergy = [] (const std::vector<float>& mono,
                                std::size_t from)
    {
        double energy = 0.0;
        for (std::size_t index = from + static_cast<std::size_t> (0.3 * sampleRate);
             index < mono.size(); ++index)
            energy += static_cast<double> (mono[index]) * mono[index];
        return energy;
    };
    const auto [plain, plainAt] = phrase (juce::MidiMessage::noteOff (1, 43));
    const auto [sixtyFour, sixtyFourAt]
        = phrase (juce::MidiMessage::noteOff (1, 43, static_cast<juce::uint8> (64)));
    const auto [zeroOn, zeroOnAt]
        = phrase (juce::MidiMessage::noteOn (1, 43, static_cast<juce::uint8> (0)));
    const auto [lifted, liftedAt]
        = phrase (juce::MidiMessage::noteOff (1, 43, static_cast<juce::uint8> (127)));
    expect (plain == sixtyFour && plain == zeroOn,
            "a release velocity of 64 or an unsensed release changed the note-off");
    // A plain note-off no longer leaves near silence: the two-way junction
    // lets the strings the note drove ring on, so the lifted string's tail
    // exceeds the damped one's by a few times rather than the ten it did.
    expect (tailEnergy (lifted, liftedAt) > 2.0 * tailEnergy (plain, plainAt),
            "a release velocity of 127 did not lift the finger");
    // What rings afterwards is the open string, not the fretted note.
    const auto bandAt = [&] (const std::vector<float>& mono, std::size_t from,
                             double frequency)
    {
        double real = 0.0;
        double imaginary = 0.0;
        const auto begin = from + static_cast<std::size_t> (0.3 * sampleRate);
        const auto end = std::min (mono.size(),
                                   from + static_cast<std::size_t> (0.9 * sampleRate));
        for (std::size_t index = begin; index < end; ++index)
        {
            const double angle = 2.0 * juce::MathConstants<double>::pi
                * frequency * static_cast<double> (index) / sampleRate;
            real += mono[index] * std::cos (angle);
            imaginary += mono[index] * std::sin (angle);
        }
        return std::hypot (real, imaginary);
    };
    const double openHz = 440.0 * std::exp2 ((40.0 - 69.0) / 12.0);
    const double frettedHz = 440.0 * std::exp2 ((43.0 - 69.0) / 12.0);
    const double openBand = bandAt (lifted, liftedAt, openHz);
    const double frettedBand = bandAt (lifted, liftedAt, frettedHz);
    std::cout << "Acustra lift open/fretted band ratio: "
              << (openBand / std::max (frettedBand, 1.0e-12)) << std::endl;
    // 2.2 rather than 3.0 since the 2026-09-04 refit: the open string still
    // dominates what the lift leaves behind, but by 2.67 rather than the 3.4
    // the previous calibration gave, because the refit lowers the open
    // fundamental's share of the first 0.9 s. The clause still says what it
    // set out to say - a lifted string rings at its open pitch, not the
    // fretted one - with a third of the former margin.
    expect (openBand > 2.2 * frettedBand,
            "the lifted string did not ring at its open pitch");
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
    setValue (source, ids::capture, 4.0f);
    setValue (source, ids::picking, 1.0f);
    setValue (source, ids::bridgeModel, 1.0f);
    setValue (source, ids::upperMic, 1.0f);
    setValue (source, ids::piezoLoading, 1.0f);

    juce::MemoryBlock stored;
    source.getStateInformation (stored);
    expect (stored.getSize() > 0, "getStateInformation returned no state");

    AcustraAudioProcessor restored;
    restored.setStateInformation (stored.getData(),
                                  static_cast<int> (stored.getSize()));
    for (const char* id : { ids::shape, ids::bodyMaterial, ids::stringMaterial,
                            ids::tuning, ids::stringAge, ids::pluckPosition,
                            ids::output, ids::capture, ids::picking, ids::bridgeModel,
                            ids::upperMic, ids::piezoLoading })
        expect (std::abs (valueOf (restored, id) - valueOf (source, id)) < 0.011f,
                std::string { "state round trip lost " } + id);

    // A complete version-3 state has a saved capture value but no upper-mic
    // override. Loading it into a live upper-mic session must restore that
    // actual capture, not leave the new override latched.
    auto previousState = source.parameters.copyState();
    for (int child = previousState.getNumChildren(); --child >= 0;)
        if (previousState.getChild (child).getProperty ("id").toString()
                == ids::upperMic
            || previousState.getChild (child).getProperty ("id").toString()
                == ids::piezoLoading)
            previousState.removeChild (child, nullptr);
    juce::MemoryBlock previousBytes;
    if (const auto xml = previousState.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, previousBytes);
    restored.setStateInformation (previousBytes.getData(),
                                  static_cast<int> (previousBytes.getSize()));
    expect (valueOf (restored, ids::upperMic) == 0.0f
                && valueOf (restored, ids::piezoLoading) == 0.0f
                && valueOf (restored, ids::capture) == 4.0f
                && restored.snapshotEngineParameters().capture
                    == acustra::CaptureType::Magnetic,
            "a legacy state retained the new upper-mic override or lost its capture");

    // A loaded-piezo session round-trips; a version-4 state without the new
    // modifier must clear it when restored into that same live processor.
    setValue (source, ids::upperMic, 0.0f);
    setValue (source, ids::capture, 3.0f);
    source.getStateInformation (stored);
    restored.setStateInformation (stored.getData(), static_cast<int> (stored.getSize()));
    expect (restored.snapshotEngineParameters().capture == acustra::CaptureType::LoadedPiezo,
            "a saved loaded-piezo capture did not round-trip");
    auto unloadedState = source.parameters.copyState();
    for (int child = unloadedState.getNumChildren(); --child >= 0;)
        if (unloadedState.getChild (child).getProperty ("id").toString() == ids::piezoLoading)
            unloadedState.removeChild (child, nullptr);
    juce::MemoryBlock unloadedBytes;
    if (const auto xml = unloadedState.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, unloadedBytes);
    restored.setStateInformation (unloadedBytes.getData(), static_cast<int> (unloadedBytes.getSize()));
    expect (valueOf (restored, ids::piezoLoading) == 0.0f
                && restored.snapshotEngineParameters().capture == acustra::CaptureType::SaddlePiezo,
            "a legacy saddle-piezo state retained the newer electrical loading");

    // A session saved before later controls existed must receive their factory
    // defaults, not whatever values happen to be live in the destination.
    setValue (restored, ids::stringAge, 99.0f);
    setValue (restored, ids::output, 5.0f);
    setValue (restored, ids::capture, 3.0f);
    setValue (restored, ids::picking, 2.0f);
    setValue (restored, ids::bridgeModel, 1.0f);
    setValue (restored, ids::upperMic, 1.0f);
    setValue (restored, ids::piezoLoading, 1.0f);
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
                && std::abs (valueOf (restored, ids::output) + 7.5f) < 0.011f
                && valueOf (restored, ids::capture) == 0.0f
                && valueOf (restored, ids::picking) == 0.0f
                && valueOf (restored, ids::bridgeModel) == 0.0f
                && valueOf (restored, ids::upperMic) == 0.0f
                && valueOf (restored, ids::piezoLoading) == 0.0f,
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
    juce::Label* engineStatus = nullptr;
    std::vector<juce::ComboBox*> setupMenus;
    while (! pending.empty())
    {
        auto* parent = pending.back();
        pending.pop_back();
        for (auto* child : parent->getChildren())
        {
            pending.push_back (child);
            if (auto* menu = dynamic_cast<juce::ComboBox*> (child))
                setupMenus.push_back (menu);
            if (auto* label = dynamic_cast<juce::Label*> (child);
                label != nullptr && label->getName() == "Engine status")
                engineStatus = label;
            if (auto* button = dynamic_cast<juce::TextButton*> (child);
                button != nullptr && button->getRadioGroupId() != 0)
                choiceButtons.push_back (button);
            if (auto* candidate = dynamic_cast<juce::MidiKeyboardComponent*> (child))
                midiKeyboard = candidate;
        }
    }

    expect (setupMenus.size() == 3,
            "the guitar, picking and capture setup menus are missing");
    const auto refreshDisplayTimer = [&]
    {
        expect (engineStatus != nullptr, "the visible engine status is missing");
        if (engineStatus == nullptr)
            return;
        engineStatus->setText ("timer pending", juce::dontSendNotification);
        // TimerThread may already be waiting for an older queued callback.
        // Keep ComboBox notifications queued until an observed display refresh,
        // rather than assuming one sleep made the editor's timer due.
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            juce::Thread::sleep (20);
            juce::Timer::callPendingTimersSynchronously();
            if (engineStatus->getText() != "timer pending")
                return;
        }
        expect (false, "the editor display timer did not run");
    };
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

    for (auto* menu : setupMenus)
    {
        namespace ids = acustra::parameters;
        expect (menu->getWantsKeyboardFocus() && menu->getDescription().isNotEmpty(),
                "a setup menu lacks keyboard focus or an accessible description");
        if (menu->getName() == "GUITAR")
        {
            // Real ComboBox selections notify asynchronously. A due status
            // timer must preserve the pending preset until its callback runs.
            menu->setSelectedId (5, juce::sendNotificationAsync);
            refreshDisplayTimer();
            expect (menu->getSelectedId() == 5,
                    "a display timer discarded the pending guitar preset");
            // Drain the pending notification through a different synchronous
            // selection before testing the resulting construction below.
            menu->setSelectedId (2, juce::sendNotificationSync);
            setValue (processor, ids::tuning, 2.0f);
            setValue (processor, ids::output, -4.0f);
            setValue (processor, ids::capture, 4.0f);
            menu->setSelectedId (5, juce::sendNotificationSync);
            expect (engineStatus != nullptr
                        && engineStatus->getText() == "Magnetic needs steel",
                    "the silent magnetic/nylon combination has no visible explanation");
            auto state = processor.snapshotEngineParameters();
            expect (state.shape == acustra::BodyShape::Auditorium
                        && state.bodyMaterial == acustra::BodyMaterial::Cedar
                        && state.stringMaterial == acustra::StringMaterial::Nylon,
                    "the classical preset did not set the guitar construction");
            for (auto* captureMenu : setupMenus)
                if (captureMenu->getName() == "CAPTURE")
                {
                    expect (! captureMenu->isItemEnabled (5),
                            "nylon still offers a magnetic pickup in the menu");
                    setValue (processor, ids::capture, 4.0f);
                    expect (captureMenu->getSelectedId() == 5
                                && valueOf (processor, ids::capture) == 4.0f,
                            "the UI silently replaced host-automated magnetic capture");
                    setValue (processor, ids::upperMic, 1.0f);
                    juce::Timer::callPendingTimersSynchronously();
                    expect (captureMenu->isItemEnabled (6)
                                && captureMenu->getSelectedId() == 6
                                && engineStatus != nullptr
                                && engineStatus->getText().contains ("kHz"),
                            "the upper microphone retained a false magnetic/nylon warning");
                    setValue (processor, ids::upperMic, 0.0f);
                    setValue (processor, ids::capture, 0.0f);
                }
            menu->setSelectedId (2, juce::sendNotificationSync);
            expect (engineStatus != nullptr
                        && engineStatus->getText().contains ("kHz"),
                    "the compatible guitar preset did not restore normal status");
            state = processor.snapshotEngineParameters();
            expect (state.shape == acustra::BodyShape::Dreadnought
                        && state.bodyMaterial == acustra::BodyMaterial::Spruce
                        && state.stringMaterial == acustra::StringMaterial::Steel,
                    "the dreadnought preset did not restore steel construction");
            menu->setSelectedId (6, juce::sendNotificationSync);
            state = processor.snapshotEngineParameters();
            expect (state.bridgeModel == acustra::BridgeModel::FyldeSteel
                        && state.stringMaterial == acustra::StringMaterial::Steel,
                    "the Fylde preset did not select the measured steel bridge");
            juce::Timer::callPendingTimersSynchronously();
            expect (menu->getSelectedId() == 6,
                    "the measured bridge preset caption was lost");
            menu->setSelectedId (2, juce::sendNotificationSync);
            expect (processor.snapshotEngineParameters().bridgeModel
                        == acustra::BridgeModel::Original,
                    "the original preset retained the alternative bridge");
            expect (valueOf (processor, ids::tuning) == 2.0f
                        && std::abs (valueOf (processor, ids::output) + 4.0f) < 0.011f,
                    "a guitar construction preset changed tuning or output");
            setValue (processor, ids::tuning, 0.0f);
            setValue (processor, ids::output, -7.5f);
        }
        else
        {
            const bool captureMenu = menu->getName() == "CAPTURE";
            const auto* id = captureMenu ? ids::capture : ids::picking;
            menu->setSelectedItemIndex (captureMenu ? 3 : 2,
                                        juce::sendNotificationSync);
            expect (std::abs (valueOf (processor, id)
                              - (captureMenu ? 3.0f : 2.0f)) < 0.011f,
                    "a setup menu did not update its host parameter");
            if (captureMenu)
            {
                expect (menu->getNumItems() == 7,
                        "the Capture menu does not expose all microphone/pickup choices");
                // ComboBox user notifications are asynchronous. A display
                // timer firing before that notification must not replace the
                // user's pending selection with the old parameter value.
                menu->setSelectedId (6, juce::sendNotificationAsync);
                refreshDisplayTimer();
                expect (menu->getSelectedId() == 6,
                        "a display timer discarded the pending upper-mic selection");
                // Deliver a synchronous selection to drain the pending
                // notification and return to the same starting capture.
                menu->setSelectedId (4, juce::sendNotificationSync);
                menu->setSelectedId (6, juce::sendNotificationSync);
                expect (valueOf (processor, ids::upperMic) == 1.0f
                            && valueOf (processor, ids::capture) == 3.0f
                            && processor.snapshotEngineParameters().capture
                                == acustra::CaptureType::UpperMic,
                        "the upper microphone menu item changed the legacy choice");
                setValue (processor, ids::capture, 1.0f);
                juce::Timer::callPendingTimersSynchronously();
                expect (menu->getSelectedId() == 6,
                        "legacy capture automation hid an active upper-mic override");
                menu->setSelectedId (3, juce::sendNotificationSync);
                expect (valueOf (processor, ids::upperMic) == 0.0f
                            && valueOf (processor, ids::capture) == 2.0f,
                        "selecting a legacy microphone left the upper override enabled");
                setValue (processor, ids::upperMic, 1.0f);
                juce::Timer::callPendingTimersSynchronously();
                expect (menu->getSelectedId() == 6,
                        "upper microphone host automation did not update the menu");
                setValue (processor, ids::upperMic, 0.0f);
                menu->setSelectedId (7, juce::sendNotificationAsync);
                refreshDisplayTimer();
                expect (menu->getSelectedId() == 7,
                        "a display timer discarded the pending loaded-piezo selection");
                menu->setSelectedId (4, juce::sendNotificationSync);
                menu->setSelectedId (7, juce::sendNotificationSync);
                expect (valueOf (processor, ids::capture) == 3.0f
                            && valueOf (processor, ids::piezoLoading) == 1.0f
                            && processor.snapshotEngineParameters().capture
                                == acustra::CaptureType::LoadedPiezo,
                        "Loaded piezo did not select its legacy sensor and loading");
                menu->setSelectedId (6, juce::sendNotificationSync);
                expect (valueOf (processor, ids::piezoLoading) == 1.0f,
                        "Upper mic discarded the underlying piezo loading");
                setValue (processor, ids::upperMic, 0.0f);
                expect (menu->getSelectedId() == 7,
                        "disabling Upper mic did not restore Loaded piezo in the menu");
                menu->setSelectedId (4, juce::sendNotificationSync);
                expect (valueOf (processor, ids::piezoLoading) == 0.0f
                            && processor.snapshotEngineParameters().capture
                                == acustra::CaptureType::SaddlePiezo,
                        "the ideal piezo menu selection retained electrical loading");
                setValue (processor, ids::piezoLoading, 1.0f);
                expect (menu->getSelectedId() == 7,
                        "host piezo-loading automation did not update its menu");
                setValue (processor, ids::piezoLoading, 0.0f);
            }
            setValue (processor, id, 0.0f);
            expect (menu->getSelectedItemIndex() == 0,
                    "host automation did not update the setup menu");
        }
    }

    const auto renderAt = [&] (int width, int height)
    {
        editor->setSize (width, height);
        for (auto* menu : setupMenus)
            expect (menu->getWidth() >= 175 && menu->getHeight() >= 32
                        && editor->getLocalBounds().contains (menu->getBounds()),
                    "a setup menu is clipped or too small at a supported size");
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

    const auto path = juce::SystemStats::getEnvironmentVariable (
        "ACUSTRA_EDITOR_SNAPSHOT", {});
    const auto saveImage = [&] (const juce::Image& image, const juce::String& suffix)
    {
        if (path.isEmpty())
            return;
        const juce::File requested { path };
        const auto destination = requested.getSiblingFile (
            requested.getFileNameWithoutExtension() + suffix + requested.getFileExtension());
        destination.getParentDirectory().createDirectory();
        juce::FileOutputStream output { destination };
        juce::PNGImageFormat png;
        const bool ready = output.openedOk() && output.setPosition (0)
            && output.truncate();
        const bool written = ready && png.writeImageToStream (image, output);
        output.flush();
        expect (written, "the requested editor screenshot could not be written");
    };

    const auto findMenu = [&] (const char* name) -> juce::ComboBox*
    {
        for (auto* menu : setupMenus)
            if (menu->getName() == name)
                return menu;
        return nullptr;
    };
    auto* guitarMenu = findMenu ("GUITAR");
    auto* pickingMenu = findMenu ("PICKING");
    auto* captureMenu = findMenu ("CAPTURE");
    if (guitarMenu != nullptr && pickingMenu != nullptr && captureMenu != nullptr)
    {
        // Reload an actual serialized state into an already-open editor. The
        // composite capture menu, preset caption and radio groups must all agree
        // with the restored construction, not retain the intervening controls.
        juce::MemoryBlock initialState, nylonState;
        processor.getStateInformation (initialState);
        guitarMenu->setSelectedId (5, juce::sendNotificationSync);
        pickingMenu->setSelectedId (3, juce::sendNotificationSync);
        captureMenu->setSelectedId (6, juce::sendNotificationSync);
        processor.getStateInformation (nylonState);
        guitarMenu->setSelectedId (2, juce::sendNotificationSync);
        pickingMenu->setSelectedId (1, juce::sendNotificationSync);
        captureMenu->setSelectedId (1, juce::sendNotificationSync);
        processor.setStateInformation (nylonState.getData(),
                                      static_cast<int> (nylonState.getSize()));
        refreshDisplayTimer();
        const auto nylon = std::find_if (choiceButtons.begin(), choiceButtons.end(),
            [] (const auto* button) { return button->getName() == "STRINGS: Nylon"; });
        expect (guitarMenu->getSelectedId() == 5 && pickingMenu->getSelectedId() == 3
                    && captureMenu->getSelectedId() == 6 && ! captureMenu->isItemEnabled (5)
                    && nylon != choiceButtons.end() && (*nylon)->getToggleState()
                    && processor.snapshotEngineParameters().capture == acustra::CaptureType::UpperMic,
                "live state reload left the editor showing a different guitar or capture");
        saveImage (renderAt (editorWidth, editorHeight), "-restored-nylon-upper");

        setValue (processor, acustra::parameters::capture, 4.0f);
        setValue (processor, acustra::parameters::upperMic, 0.0f);
        refreshDisplayTimer();
        saveImage (renderAt (editorMinimumWidth, editorMinimumHeight), "-nylon-magnetic");
        captureMenu->setSelectedId (7, juce::sendNotificationSync);
        juce::MemoryBlock loadedState;
        processor.getStateInformation (loadedState);
        captureMenu->setSelectedId (1, juce::sendNotificationSync);
        processor.setStateInformation (loadedState.getData(),
                                      static_cast<int> (loadedState.getSize()));
        refreshDisplayTimer();
        expect (captureMenu->getSelectedId() == 7 && captureMenu->isItemEnabled (7)
                    && processor.snapshotEngineParameters().capture == acustra::CaptureType::LoadedPiezo,
                "live loaded-piezo state reload left an incorrect capture in the editor");
        saveImage (renderAt (editorMinimumWidth, editorMinimumHeight), "-loaded-piezo");
        processor.setStateInformation (initialState.getData(),
                                      static_cast<int> (initialState.getSize()));
        refreshDisplayTimer();
    }

    saveImage (renderAt (editorMinimumWidth, editorMinimumHeight), "-minimum");
    saveImage (renderAt (editorMaximumWidth, editorMaximumHeight), "-maximum");
    saveImage (renderAt (editorWidth, editorHeight), "");

    editor.reset();
    processor.releaseResources();
}
} // namespace

void testMpeTimbreReachesTheEngineOnMemberChannelOnly()
{
    // CC74 is MPE Timbre. On a lower-zone member channel it places this one
    // note's own pluck point (see AcustraEngine::initialisePluck); off a
    // member channel, or with no lower zone at all, it must not move a
    // single sample.
    const auto phrase = [] (int timbre, int channel, bool memberZone)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::MidiBuffer start;
        if (memberZone)
            addLowerZone (start, 2);
        start.addEvent (juce::MidiMessage::controllerEvent (channel, 74, timbre), 0);
        start.addEvent (juce::MidiMessage::noteOn (channel, 52, 0.8f), 1);
        std::vector<float> mono;
        const int blocks = static_cast<int> (0.5 * sampleRate / blockSize);
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer empty;
            processor.processBlock (audio, block == 0 ? start : empty);
            for (int sample = 0; sample < blockSize; ++sample)
                mono.push_back (audio.getSample (0, sample));
        }
        return mono;
    };
    const auto conventionalLow = phrase (10, 1, false);
    const auto conventionalHigh = phrase (110, 1, false);
    expect (conventionalLow == conventionalHigh,
            "CC74 moved the sound on a conventional, non-member channel");

    const auto memberLow = phrase (10, 2, true);
    const auto memberHigh = phrase (110, 2, true);
    expect (memberLow.size() == memberHigh.size() && memberLow != memberHigh,
            "CC74 did not reach the engine as a member channel's pluck point");
}

void testMpePressureReachesTheEngineOnMemberChannelOnly()
{
    // MPE channel pressure, status 0xD0, biases this note's own vibrato
    // depth (see AcustraEngine::mpePressureFor) and must be inert off a
    // member channel or with no lower zone. It does not reach a pull-off:
    // the phrase below therefore holds CC1 up and frets the note, so the
    // difference it asserts on comes through the vibrato path alone.
    const auto phrase = [] (int pressure, int channel, bool memberZone)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::MidiBuffer start;
        if (memberZone)
            addLowerZone (start, 2);
        start.addEvent (juce::MidiMessage::channelPressureChange (channel, pressure), 0);
        start.addEvent (juce::MidiMessage::controllerEvent (channel, 1, 127), 1);
        start.addEvent (juce::MidiMessage::noteOn (channel, 52, 0.8f), 2);
        std::vector<float> mono;
        const int blocks = static_cast<int> (1.0 * sampleRate / blockSize);
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer empty;
            processor.processBlock (audio, block == 0 ? start : empty);
            for (int sample = 0; sample < blockSize; ++sample)
                mono.push_back (audio.getSample (0, sample));
        }
        return mono;
    };
    const auto conventionalLight = phrase (0, 1, false);
    const auto conventionalFirm = phrase (127, 1, false);
    expect (conventionalLight == conventionalFirm,
            "channel pressure moved the sound on a conventional, non-member "
            "channel");

    const auto memberLight = phrase (0, 2, true);
    const auto memberFirm = phrase (127, 2, true);
    expect (memberLight.size() == memberFirm.size() && memberLight != memberFirm,
            "channel pressure did not reach the engine on a member channel");
}

void testStringPerChannelModeViaMonoModeOn()
{
    // MIDI 1.0's own Mono Mode On (CC126, value = channel count) is the
    // toggle: M=6 is the standard spelling of the one-string-per-channel
    // layout Roland's GK and Fishman's TriplePlay each produce, though
    // neither is documented to transmit this specific message itself.
    // Poly Mode On (CC127) restores the fret-distance allocator. Never
    // sending either leaves the allocator exactly as it was.
    const auto activeVoices = [] (bool sendMonoOn, bool sendPolyOnAfter,
                                  int channel, int midiNote)
    {
        AcustraAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio { 2, blockSize };
        juce::MidiBuffer start;
        if (sendMonoOn)
            start.addEvent (juce::MidiMessage::controllerEvent (1, 126, 6), 0);
        if (sendPolyOnAfter)
            start.addEvent (juce::MidiMessage::controllerEvent (1, 127, 0), 1);
        start.addEvent (juce::MidiMessage::noteOn (channel, midiNote, 0.8f), 2);
        processor.processBlock (audio, start);
        return processor.getActiveVoiceCount();
    };
    // Channel 6 is string index 5, the highest string; MIDI note 40 is far
    // below what any fret on it can reach, so a controller enforcing the
    // channel-to-string mapping cannot play it at all.
    expect (activeVoices (false, false, 6, 40) == 1,
            "the allocator, left alone, could not reach a low note on a high "
            "channel it is free to reassign");
    expect (activeVoices (true, false, 6, 40) == 0,
            "Mono Mode On did not force the channel's own string");
    expect (activeVoices (true, true, 6, 40) == 1,
            "Poly Mode On did not restore the fret-distance allocator");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    testParameterContract();
    testProcessorContractAndSampleAccurateMidi();
    testSameSampleChordOrderIsCanonical();
    testSameSampleNoteOnOffDoesNotStick();
    testSameSampleChordsAreStrummedAndAlternate();
    testRepeatedHeldChordsKeepTheirAudibleSweep();
    testBridgeHandControllerReachesTheEngine();
    testTheModulationWheelReachesTheEngineAsVibrato();
    testMpeTimbreReachesTheEngineOnMemberChannelOnly();
    testMpePressureReachesTheEngineOnMemberChannelOnly();
    testStringPerChannelModeViaMonoModeOn();
    testLegatoControllerReachesTheEngine();
    testReleaseVelocityReachesTheEngineAsAFingerLift();
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
