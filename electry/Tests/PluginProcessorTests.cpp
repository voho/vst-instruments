#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
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

// These are the shipped defaults, and they are written out rather than read from
// EngineParameters on purpose: the layout now derives its defaults from that
// struct, so reading it here too would make the check tautological. This table is
// the independent statement of what the plug-in promises a new instance, and it is
// what would have caught the two lists silently drifting apart.
constexpr std::array<ParameterExpectation, 26> expectedParameters {{
    { electry::parameters::pickupSelector, 2.0f,  1.0e-5f },
    { electry::parameters::pickupType,     0.32f,  1.0e-5f },
    { electry::parameters::tone,           0.70f,  1.0e-5f },
    { electry::parameters::guitarBuild,    0.80f, 1.0e-5f },
    { electry::parameters::bodyResonance,  0.35f, 1.0e-5f },
    { electry::parameters::stringAge,      0.30f, 1.0e-5f },
    { electry::parameters::pickPosition,   0.18f, 1.0e-5f },
    { electry::parameters::pickHardness,   0.58f,  1.0e-5f },
    { electry::parameters::pickNoise,      0.5f,  1.0e-5f },
    { electry::parameters::fingerNoise,    0.4f,  1.0e-5f },
    { electry::parameters::releaseNoise,   0.4f,  1.0e-5f },
    { electry::parameters::muteDamping,    0.55f, 1.0e-5f },
    { electry::parameters::bendTime,       0.28f, 1.0e-4f },
    { electry::parameters::velocity,       0.85f, 1.0e-5f },
    { electry::parameters::output,        -6.0f,  1.0e-5f },
    { electry::parameters::artifacts,      0.18f, 1.0e-5f },
    { electry::parameters::outputMode,     0.0f,  1.0e-5f },
    { electry::parameters::distortion,     0.0f,  1.0e-5f },
    { electry::parameters::amp,            0.0f,  1.0e-5f },
    { electry::parameters::compressor,     0.0f,  1.0e-5f },
    { electry::parameters::delay,          0.0f,  1.0e-5f },
    { electry::parameters::room,           0.0f,  1.0e-5f },
    { electry::parameters::sympathetic,    0.20f, 1.0e-5f },
    { electry::parameters::palmMute,       0.0f,  1.0e-5f },
    { electry::parameters::strumSpread,    0.0f,  1.0e-4f },
    { electry::parameters::resonanceDepth,  35.0f,  1.0e-4f },
}};

float parameterValue (const ElectryAudioProcessor& processor, const char* id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, std::string ("missing parameter ") + id);
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (ElectryAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot set missing parameter ") + id);
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void expectParameterText (ElectryAudioProcessor& processor, const char* id,
                          float value, const char* expectedText)
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

void renderBlock (ElectryAudioProcessor& processor, juce::AudioBuffer<float>& audio,
                  juce::MidiBuffer& midi, int numSamples = blockSize)
{
    audio.setSize (2, numSamples, false, false, true);
    audio.clear();
    processor.processBlock (audio, midi);
    midi.clear();
}

float renderSeconds (ElectryAudioProcessor& processor, juce::AudioBuffer<float>& audio,
                     double seconds)
{
    juce::MidiBuffer emptyMidi;
    float peak = 0.0f;
    int remaining = static_cast<int> (seconds * sampleRate);
    while (remaining > 0)
    {
        const int samples = std::min (blockSize, remaining);
        renderBlock (processor, audio, emptyMidi, samples);
        peak = std::max (peak, audio.getMagnitude (0, audio.getNumSamples()));
        remaining -= samples;
    }
    return peak;
}

int capturedPlayStyle (const ElectryAudioProcessor& processor, int midiNote)
{
    for (int string = 0; string < electry::ElectryEngine::stringCount; ++string)
    {
        const auto state = processor.getStringVisualState (string);
        if (state.sounding && state.midiNote == midiNote)
            return static_cast<int> (state.playStyle);
    }
    return -1;
}

// Renders and keeps the left channel's raw samples, for the one check below
// that needs to measure a frequency rather than just a peak level.
std::vector<float> renderCapture (ElectryAudioProcessor& processor,
                                  juce::AudioBuffer<float>& audio, double seconds)
{
    juce::MidiBuffer emptyMidi;
    std::vector<float> captured;
    int remaining = static_cast<int> (seconds * sampleRate);
    captured.reserve (static_cast<std::size_t> (std::max (0, remaining)));
    while (remaining > 0)
    {
        const int samples = std::min (blockSize, remaining);
        renderBlock (processor, audio, emptyMidi, samples);
        const auto* channel = audio.getReadPointer (0);
        captured.insert (captured.end(), channel, channel + samples);
        remaining -= samples;
    }
    return captured;
}

// Hann-windowed DFT magnitude at an arbitrary frequency, evaluated with a
// phasor recurrence so the scan stays fast. Mirrors the DSP library's own
// dftMagnitude() in Tests/ElectryEngineTests.cpp; duplicated rather than
// shared because this test binary links the plug-in target, not ElectryDSP.
double dftMagnitude (const std::vector<float>& data, int start, int length,
                     double localSampleRate, double frequency)
{
    const int first = std::max (0, start);
    const int last = std::min<int> (first + length, static_cast<int> (data.size()));
    const int n = last - first;
    if (n < 16)
        return 0.0;

    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency
                        / localSampleRate;
    const double stepReal = std::cos (omega);
    const double stepImag = -std::sin (omega);
    double phasorReal = 1.0;
    double phasorImag = 0.0;
    double sumReal = 0.0;
    double sumImag = 0.0;
    const double windowStep = juce::MathConstants<double>::pi
                             / static_cast<double> (n - 1);

    for (int i = 0; i < n; ++i)
    {
        const double window = std::sin (windowStep * i);
        const double sample = window * window
            * static_cast<double> (data[static_cast<std::size_t> (first + i)]);
        sumReal += sample * phasorReal;
        sumImag += sample * phasorImag;
        const double nextReal = phasorReal * stepReal - phasorImag * stepImag;
        phasorImag = phasorReal * stepImag + phasorImag * stepReal;
        phasorReal = nextReal;
    }
    return std::sqrt (sumReal * sumReal + sumImag * sumImag);
}

// Locates the strongest spectral component near an expected fundamental by a
// coarse-to-fine scan, scoring a short harmonic series rather than assuming
// fundamental dominance: a magnetic pickup's output is an induced EMF, so its
// fundamental can be weaker than its first few partials.
double measureFundamentalHz (const std::vector<float>& data, int start, int length,
                             double localSampleRate, double expectedHz)
{
    const auto scan = [&] (double centre, double spanCents, double stepCents)
    {
        double bestFrequency = centre;
        double bestMagnitude = -1.0;
        for (double cents = -spanCents; cents <= spanCents; cents += stepCents)
        {
            const double frequency = centre * std::pow (2.0, cents / 1200.0);
            double magnitude = 0.0;
            for (int partial = 1; partial <= 5; ++partial)
            {
                const double partialFrequency = frequency * partial;
                if (partialFrequency >= 0.45 * localSampleRate)
                    break;
                magnitude += dftMagnitude (data, start, length, localSampleRate,
                                          partialFrequency)
                           / std::sqrt (static_cast<double> (partial));
            }
            if (magnitude > bestMagnitude)
            {
                bestMagnitude = magnitude;
                bestFrequency = frequency;
            }
        }
        return bestFrequency;
    };

    const double coarse = scan (expectedHz, 120.0, 6.0);
    return scan (coarse, 6.0, 0.5);
}

void testParameterLayoutAndDefaults()
{
    ElectryAudioProcessor processor;
    expect (processor.getParameters().size()
                == static_cast<int> (expectedParameters.size()),
            "processor does not expose exactly 26 APVTS parameters");

    std::set<std::string> uniqueIds;
    for (const auto& expected : expectedParameters)
    {
        expect (uniqueIds.insert (expected.id).second,
                std::string ("duplicate APVTS id ") + expected.id);
        const auto value = parameterValue (processor, expected.id);
        expect (std::abs (value - expected.defaultValue) <= expected.tolerance,
                std::string ("wrong default for ") + expected.id + ": got "
                    + std::to_string (value));
    }

    for (const char* removed : { "bodyWood", "bodySize", "bodyShape",
                                 "construction", "scaleLength", "stringGauge",
                                 "doubleMode", "vibratoDepth" })
        expect (processor.parameters.getParameter (removed) == nullptr,
                std::string ("obsolete host parameter still exists: ") + removed);
}

void testFactoryPrograms()
{
    class Listener final : public juce::AudioProcessorListener
    {
    public:
        void audioProcessorParameterChanged (juce::AudioProcessor*, int,
                                             float) override
        {
            ++parameterChanges;
        }

        void audioProcessorChanged (
            juce::AudioProcessor*, const ChangeDetails& details) override
        {
            if (details.programChanged)
                ++programChanges;
        }

        int parameterChanges = 0;
        int programChanges = 0;
    } listener;

    ElectryAudioProcessor processor;
    expect (processor.getNumPrograms() == 3,
            "processor does not expose exactly three factory rigs");
    expect (processor.getCurrentProgram() == 0,
            "new processor did not select Factory Default");

    const std::array<const char*, 3> expectedNames {
        "Factory Default", "Drop-E Metal", "Mute / Dead DI"
    };
    std::set<std::string> uniqueNames;
    for (int index = 0; index < processor.getNumPrograms(); ++index)
    {
        const auto name = processor.getProgramName (index).toStdString();
        expect (name == expectedNames[static_cast<std::size_t> (index)],
                "wrong factory-rig name at index " + std::to_string (index));
        expect (uniqueNames.insert (name).second,
                "factory-rig names are not unique");
    }
    expect (processor.getProgramName (-1).isEmpty()
                && processor.getProgramName (processor.getNumPrograms()).isEmpty(),
            "an invalid factory-rig index returned a name");

    const auto expectedValue = [] (int program,
                                   const ParameterExpectation& parameter)
    {
        float result = parameter.defaultValue;
        const auto overrideValue = [&] (const char* id, float value)
        {
            if (std::strcmp (parameter.id, id) == 0)
                result = value;
        };

        if (program == 1)
        {
            overrideValue (electry::parameters::tone, 1.00f);
            overrideValue (electry::parameters::stringAge, 0.10f);
            overrideValue (electry::parameters::pickHardness, 0.85f);
            overrideValue (electry::parameters::fingerNoise, 0.55f);
            overrideValue (electry::parameters::muteDamping, 0.85f);
            overrideValue (electry::parameters::velocity, 0.70f);
            overrideValue (electry::parameters::output, 6.00f);
            overrideValue (electry::parameters::artifacts, 0.15f);
            overrideValue (electry::parameters::distortion, 0.45f);
            overrideValue (electry::parameters::amp, 0.95f);
            overrideValue (electry::parameters::compressor, 0.60f);
            overrideValue (electry::parameters::sympathetic, 0.25f);
        }
        else if (program == 2)
        {
            overrideValue (electry::parameters::pickPosition, 0.20f);
            overrideValue (electry::parameters::pickHardness, 0.82f);
            overrideValue (electry::parameters::fingerNoise, 0.55f);
            overrideValue (electry::parameters::output, 6.00f);
            overrideValue (electry::parameters::artifacts, 0.15f);
            overrideValue (electry::parameters::sympathetic, 0.00f);
        }
        return result;
    };

    processor.addListener (&listener);
    for (int program = 0; program < processor.getNumPrograms(); ++program)
    {
        // Poison all controls before every load. A sparse implementation
        // that forgot to restore omitted controls would fail this full matrix.
        for (auto* parameter : processor.getParameters())
            parameter->setValueNotifyingHost (
                parameter->getDefaultValue() < 0.5f ? 1.0f : 0.0f);

        listener.parameterChanges = 0;
        listener.programChanges = 0;
        processor.setCurrentProgram (program);
        expect (processor.getCurrentProgram() == program,
                "factory-rig selection did not retain its index");
        expect (listener.parameterChanges > 0,
                "factory-rig selection did not notify changed parameters");
        expect (listener.programChanges == 1,
                "factory-rig selection did not notify one program change");

        for (const auto& parameter : expectedParameters)
            expect (std::abs (parameterValue (processor, parameter.id)
                                 - expectedValue (program, parameter))
                        <= juce::jmax (parameter.tolerance, 1.0e-4f),
                    std::string ("wrong value for ") + parameter.id
                        + " in factory rig " + std::to_string (program));
    }

    const int validProgram = processor.getCurrentProgram();
    const int notifications = listener.programChanges;
    processor.setCurrentProgram (-1);
    processor.setCurrentProgram (processor.getNumPrograms());
    expect (processor.getCurrentProgram() == validProgram
                && listener.programChanges == notifications,
            "invalid factory-rig selection changed processor state");
    processor.changeProgramName (1, "Renamed");
    expect (processor.getProgramName (1) == "Drop-E Metal",
            "immutable factory-rig name was changed");
    processor.removeListener (&listener);

    // Programs are rigs, not performances: changing one must not touch either
    // latched keyswitch bank.
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::firstKeyswitchNote + 2,
        (juce::uint8) 127), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::firstPlayStyleKeyswitchNote
               + static_cast<int> (electry::PlayStyle::Dead),
        (juce::uint8) 127), 0);
    renderBlock (processor, audio, midi);
    processor.setPlayStyleKeysHold (true);
    processor.setCurrentProgram (1);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentPickStyleIndex() == 2
                && processor.getCurrentPlayStyleIndex()
                       == static_cast<int> (electry::PlayStyle::Dead),
            "factory rig silently changed a performance articulation");
    expect (processor.getPlayStyleKeysHold(),
            "factory rig changed the play-style key mode");
    processor.releaseResources();
}

void testParameterTextFormatting()
{
    ElectryAudioProcessor processor;
    expectParameterText (processor, electry::parameters::guitarBuild, 0.0f,
                         "Slab fixed");
    expectParameterText (processor, electry::parameters::guitarBuild, 0.4f,
                         "Angular set");
    expectParameterText (processor, electry::parameters::guitarBuild, 0.5f, "50%");
    expectParameterText (processor, electry::parameters::guitarBuild, 0.8f,
                         "Dense extended");
    expectParameterText (processor, electry::parameters::guitarBuild, 1.0f,
                         "Neck-through");
    expectParameterText (processor, electry::parameters::output, -6.0f, "-6.0dB");
    expectParameterText (processor, electry::parameters::output, 3.0f, "+3.0dB");
    expectParameterText (processor, electry::parameters::tone, 0.8f, "80%");
    expectParameterText (processor, electry::parameters::artifacts, 0.18f, "18%");
    expectParameterText (processor, electry::parameters::outputMode, 0.0f, "Mono");
    expectParameterText (processor, electry::parameters::outputMode, 1.0f, "Stereo");
    expectParameterText (processor, electry::parameters::outputMode, 2.0f, "Double");
    expectParameterText (processor, electry::parameters::bendTime, 0.28f, "280 ms");
    expectParameterText (processor, electry::parameters::pickupType, 0.0f, "Humbucker");
    expectParameterText (processor, electry::parameters::pickupType, 1.0f, "Single coil");
    expectParameterText (processor, electry::parameters::sympathetic, 0.2f, "20%");
    expectParameterText (processor, electry::parameters::palmMute, 0.0f, "0%");
    expectParameterText (processor, electry::parameters::strumSpread, 0.0f,
                         "Block chord");
    expectParameterText (processor, electry::parameters::strumSpread, 18.0f,
                         "18.0 ms/string");
    expectParameterText (processor, electry::parameters::resonanceDepth, 35.0f,
                         "35%");

    const auto* selector = processor.parameters.getParameter (
        electry::parameters::pickupSelector);
    expect (selector != nullptr
                && selector->getText (selector->convertTo0to1 (2.0f), 64) == "Bridge",
            "pickup selector does not format its Bridge choice");
}

void expectParameterValueForText (ElectryAudioProcessor& processor, const char* id,
                                  const juce::String& text, float expectedValue,
                                  float tolerance, const std::string& label)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot parse missing parameter ") + id);
    if (parameter == nullptr)
        return;

    const auto actualValue =
        parameter->convertFrom0to1 (parameter->getValueForText (text));
    expect (std::abs (actualValue - expectedValue) < tolerance,
            label + ": expected " + std::to_string (expectedValue) + ", got "
                + std::to_string (actualValue));
}

// getText() (a parameter's plain value formatted into automation text) is
// covered above by testParameterTextFormatting(), but every parameter here
// also installs the opposite direction - getValueForText(), reached whenever
// a host or the generic parameter editor parses typed automation text back
// into a value - and nothing in the suite had ever called it: percentValue(),
// plainNumericValue() and timeValue() in PluginProcessor.cpp, plus
// Guitar Build's named anchors could all have silently mis-parsed without
// failing a single existing test.
void testParameterTextParsing()
{
    ElectryAudioProcessor processor;

    // percentValue(): a 0..1-ranged parameter's plain value equals its
    // normalised one, so the parsed percentage is the expected value exactly.
    expectParameterValueForText (processor, electry::parameters::tone, "80%", 0.80f,
                                 1.0e-4f, "tone \"80%\"");
    expectParameterValueForText (processor, electry::parameters::artifacts, "18%", 0.18f,
                                 1.0e-4f, "artifacts \"18%\"");

    // plainNumericValue(): output level's dB text keeps its sign through the
    // round trip in both directions.
    expectParameterValueForText (processor, electry::parameters::output, "-6.0dB", -6.0f,
                                 1.0e-3f, "output \"-6.0dB\"");
    expectParameterValueForText (processor, electry::parameters::output, "+3.0dB", 3.0f,
                                 1.0e-3f, "output \"+3.0dB\"");

    // resonanceDepth's percentText100()/plainNumericValue() pair is a
    // 0..100-ranged percent, unlike tone/artifacts' 0..1 one above.
    expectParameterValueForText (processor, electry::parameters::resonanceDepth, "35%",
                                 35.0f, 1.0e-3f, "resonanceDepth \"35%\"");

    // strumSpread reuses plainNumericValue() too, but its own display text
    // falls back to the word "Block chord" below 0.05 ms - a string with no
    // digits at all, so a naive re-parse of exactly what the control just
    // displayed must fold to 0.0 rather than leave String::getFloatValue()
    // to choke on an all-alphabetic string.
    expectParameterValueForText (processor, electry::parameters::strumSpread,
                                 "Block chord", 0.0f, 1.0e-3f,
                                 "strumSpread \"Block chord\"");
    expectParameterValueForText (processor, electry::parameters::strumSpread,
                                 "18.0 ms/string", 18.0f, 1.0e-3f,
                                 "strumSpread \"18.0 ms/string\"");

    // timeValue()'s branch on whether the typed text contains "ms": bendTime's
    // own display crosses from milliseconds to seconds at 1.0 s, so both
    // spellings of the same duration - one with the suffix, one without -
    // must parse back to the identical value rather than one of them silently
    // landing 1000x off.
    expectParameterValueForText (processor, electry::parameters::bendTime, "280 ms", 0.28f,
                                 1.0e-3f, "bendTime \"280 ms\"");
    expectParameterValueForText (processor, electry::parameters::bendTime, "1.50 s", 1.50f,
                                 1.0e-3f, "bendTime \"1.50 s\" (no \"ms\" suffix)");
    expectParameterValueForText (processor, electry::parameters::bendTime, "1500ms", 1.50f,
                                 1.0e-3f, "bendTime \"1500ms\" (\"ms\" suffix)");

    expectParameterValueForText (processor, electry::parameters::guitarBuild,
                                 "Angular set", 0.4f, 1.0e-3f,
                                 "guitarBuild named anchor");
    expectParameterValueForText (processor, electry::parameters::guitarBuild,
                                 "50%", 0.5f, 1.0e-3f,
                                 "guitarBuild percentage");
}

void testStateRoundTrip()
{
    ElectryAudioProcessor source;
    source.setCurrentProgram (2);
    source.setPlayStyleKeysHold (true);
    setParameterValue (source, electry::parameters::pickupType, 0.9f);
    setParameterValue (source, electry::parameters::guitarBuild, 0.15f);
    setParameterValue (source, electry::parameters::output, -12.0f);
    setParameterValue (source, electry::parameters::artifacts, 0.72f);
    setParameterValue (source, electry::parameters::outputMode, 2.0f);
    setParameterValue (source, electry::parameters::pickupSelector, 0.0f);
    setParameterValue (source, electry::parameters::sympathetic, 0.66f);
    setParameterValue (source, electry::parameters::palmMute, 0.44f);
    setParameterValue (source, electry::parameters::strumSpread, 22.0f);
    setParameterValue (source, electry::parameters::resonanceDepth, 80.0f);
    source.triggerArticulation (static_cast<int> (electry::PickStyle::Up));
    source.triggerArticulation (
        electry::ElectryEngine::pickStyleKeyswitchCount
            + static_cast<int> (electry::PlayStyle::Dead));

    juce::MemoryBlock state;
    source.getStateInformation (state);
    expect (state.getSize() > 0, "state serialisation produced no data");

    ElectryAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    expect (restored.getCurrentProgram() == 2,
            "factory-rig index did not survive a state round trip");

    expect (std::abs (parameterValue (restored, electry::parameters::pickupType) - 0.9f)
                < 1.0e-4f,
            "pickupType did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::guitarBuild) - 0.15f)
                < 1.0e-4f,
            "guitarBuild did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::output) + 12.0f)
                < 1.0e-3f,
            "output level did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::artifacts) - 0.72f)
                < 1.0e-4f,
            "artifacts did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::outputMode) - 2.0f)
                < 1.0e-4f,
            "output mode did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::pickupSelector))
                < 1.0e-4f,
            "pickup selector did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::sympathetic)
                          - 0.66f) < 1.0e-4f,
            "sympathetic ring did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::palmMute)
                          - 0.44f) < 1.0e-4f,
            "palm mute did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::strumSpread)
                          - 22.0f) < 1.0e-3f,
            "strum spread did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::resonanceDepth)
                          - 80.0f) < 1.0e-3f,
            "resonance depth did not survive a state round trip");
    expect (restored.getCurrentPickStyleIndex()
                == static_cast<int> (electry::PickStyle::Up)
                && restored.getCurrentPlayStyleIndex()
                       == static_cast<int> (electry::PlayStyle::Dead),
            "the two articulation latches did not survive a state round trip");
    expect (restored.getPlayStyleKeysHold(),
            "the play-style key mode did not survive a state round trip");

}

void testBusAndPluginContract()
{
    ElectryAudioProcessor processor;
    expect (processor.acceptsMidi(), "instrument does not accept MIDI");
    expect (! processor.producesMidi(), "instrument unexpectedly produces MIDI");
    expect (! processor.isMidiEffect(), "instrument reports being a MIDI effect");
    expect (processor.hasEditor(), "instrument does not advertise its editor");
    expect (std::abs (processor.getTailLengthSeconds()
                          - ElectryAudioProcessor::maximumTailLengthSeconds) < 1.0e-9,
            "tail length does not match the documented contract");

    expect (processor.getBusCount (true) == 0,
            "instrument unexpectedly exposes an input bus");
    expect (processor.getBusCount (false) == 1,
            "instrument does not expose exactly one output bus");
    expect (processor.getTotalNumOutputChannels() == 2,
            "instrument main output is not stereo");

    // Test the actual processor topology rather than a hand-built layout, so
    // the bus count matches what checkBusesLayoutSupported would validate.
    const auto stereoLayout = processor.getBusesLayout();
    expect (processor.isBusesLayoutSupported (stereoLayout),
            "default no-input/stereo-output layout is unsupported");

    auto monoLayout = stereoLayout;
    monoLayout.outputBuses.set (0, juce::AudioChannelSet::mono());
    expect (! processor.isBusesLayoutSupported (monoLayout),
            "mono instrument output should be rejected");

    auto inputLayout = stereoLayout;
    inputLayout.inputBuses.add (juce::AudioChannelSet::stereo());
    expect (! processor.isBusesLayoutSupported (inputLayout),
            "an added instrument input bus should be rejected");
}

void testSampleAccurateNoteAndSound()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.isEngineReady(), "engine is not ready after prepareToPlay");

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    // A note placed mid-block must not sound before its sample position.
    const int onset = 256;
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), onset);
    renderBlock (processor, audio, midi);

    const auto before = audio.getMagnitude (0, onset - 8);
    const auto after = audio.getMagnitude (onset, blockSize - onset);
    // getMagnitude is non-negative, so <= 0 is exact silence.
    expect (before <= 0.0f, "audio appeared before the note-on sample position");
    expect (after > 1.0e-5f, "note-on did not produce audio in its block");
    expect (processor.getActiveVoiceCount() == 1,
            "note-on did not report one active string");

    processor.releaseResources();
}

void testKeyswitchContract()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    expect (processor.getCurrentPickStyleIndex() == 0
                && processor.getCurrentPlayStyleIndex() == 0,
            "default styles are not a sustained downstroke");

    const auto mutedIndex = static_cast<int> (electry::PlayStyle::PalmMute);
    const auto mutedNote = electry::ElectryEngine::firstPlayStyleKeyswitchNote
                         + mutedIndex;
    const auto upNote = electry::ElectryEngine::firstKeyswitchNote
                      + static_cast<int> (electry::PickStyle::Up);

    // A keyswitch note alone changes the style and never sounds, and the two
    // banks latch independently.
    midi.addEvent (juce::MidiMessage::noteOn (1, mutedNote, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, upNote, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    expect (audio.getMagnitude (0, blockSize) <= 0.0f,
            "keyswitch note produced audio");
    expect (processor.getActiveVoiceCount() == 0, "keyswitch note created a voice");
    expect (processor.getCurrentPlayStyleIndex() == mutedIndex,
            "palm-mute keyswitch did not latch the Palm Mute style");
    expect (processor.getCurrentPickStyleIndex()
                == static_cast<int> (electry::PickStyle::Up),
            "upstroke keyswitch did not latch alongside the play style");

    // The latched styles survive their own note-offs and apply to played notes.
    midi.addEvent (juce::MidiMessage::noteOff (1, mutedNote), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, upNote), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::lowestPlayableNote, (juce::uint8) 96), 16);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentPlayStyleIndex() == mutedIndex,
            "keyswitch note-off cleared the latched style");
    expect (processor.getActiveVoiceCount() == 1,
            "played note after keyswitch did not sound");

    processor.releaseResources();
    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.getCurrentPlayStyleIndex() == mutedIndex
                && processor.getCurrentPickStyleIndex()
                       == static_cast<int> (electry::PickStyle::Up),
            "release/prepare cleared the two articulation latches");
    processor.releaseResources();
}

void testPlayStyleHoldContract()
{
    const int sustain = static_cast<int> (electry::PlayStyle::Sustain);
    const int palm = static_cast<int> (electry::PlayStyle::PalmMute);
    const int dead = static_cast<int> (electry::PlayStyle::Dead);
    const int palmKey = electry::ElectryEngine::firstPlayStyleKeyswitchNote + palm;
    const int deadKey = electry::ElectryEngine::firstPlayStyleKeyswitchNote + dead;

    // Note On and both spellings of Note Off condition a simultaneous attack
    // before it, independent of insertion order.
    const auto conditionedAttack = [=] (bool release, bool switchFirst,
                                        bool zeroVelocityRelease)
    {
        ElectryAudioProcessor processor;
        processor.setPlayStyleKeysHold (true);
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        if (release)
        {
            midi.addEvent (juce::MidiMessage::noteOn (
                1, palmKey, (juce::uint8) 127), 0);
            renderBlock (processor, audio, midi);
        }

        const auto styleEvent = release
            ? (zeroVelocityRelease
                   ? juce::MidiMessage::noteOn (
                         1, palmKey, (juce::uint8) 0)
                   : juce::MidiMessage::noteOff (1, palmKey))
            : juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127);
        const auto playable = juce::MidiMessage::noteOn (
            1, 40, (juce::uint8) 110);
        if (switchFirst)
            midi.addEvent (styleEvent, 0);
        midi.addEvent (playable, 0);
        if (! switchFirst)
            midi.addEvent (styleEvent, 0);
        renderBlock (processor, audio, midi);
        const int captured = capturedPlayStyle (processor, 40);
        processor.releaseResources();
        return captured;
    };

    expect (conditionedAttack (false, true, false) == palm
                && conditionedAttack (false, false, false) == palm,
            "HOLD Note On depended on same-sample insertion order");
    expect (conditionedAttack (true, true, false) == sustain
                && conditionedAttack (true, false, false) == sustain,
            "HOLD Note Off did not condition a same-sample attack");
    expect (conditionedAttack (true, true, true) == sustain
                && conditionedAttack (true, false, true) == sustain,
            "zero-velocity HOLD Note On did not behave as a release");

    const auto allNotesOffOrder = [=] (bool resetFirst)
    {
        ElectryAudioProcessor processor;
        processor.setPlayStyleKeysHold (true);
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        // Insert the playable note first to prove both state events are pulled
        // into the conditioning pass while retaining their mutual order.
        midi.addEvent (juce::MidiMessage::noteOn (
            1, 40, (juce::uint8) 110), 0);
        const auto reset = juce::MidiMessage::controllerEvent (1, 123, 0);
        const auto palmOn = juce::MidiMessage::noteOn (
            1, palmKey, (juce::uint8) 127);
        midi.addEvent (resetFirst ? reset : palmOn, 0);
        midi.addEvent (resetFirst ? palmOn : reset, 0);
        renderBlock (processor, audio, midi);
        const int captured = capturedPlayStyle (processor, 40);
        processor.releaseResources();
        return captured;
    };
    expect (allNotesOffOrder (true) == palm
                && allNotesOffOrder (false) == sustain,
            "CC123 and a HOLD Note On lost their same-sample source order");

    ElectryAudioProcessor processor;
    processor.setPlayStyleKeysHold (true);
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    const auto send = [&] (const juce::MidiMessage& message)
    {
        midi.addEvent (message, 0);
        renderBlock (processor, audio, midi);
    };

    expect (processor.getPlayStyleKeysHold()
                && processor.getCurrentPlayStyleIndex() == sustain
                && processor.getEffectivePlayStyleIndex() == sustain,
            "HOLD did not start from the saved Sustain base");

    // Last-down wins, duplicate downs require matching releases, and an older
    // still-held key is revealed before the saved base.
    send (juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127));
    send (juce::MidiMessage::noteOn (1, deadKey, (juce::uint8) 127));
    send (juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127));
    expect (processor.getCurrentPlayStyleIndex() == sustain
                && processor.getEffectivePlayStyleIndex() == palm,
            "a HOLD override overwrote its saved base or lost last-down order");
    send (juce::MidiMessage::noteOff (1, palmKey));
    expect (processor.getEffectivePlayStyleIndex() == palm,
            "one Note Off cleared a duplicated HOLD key");
    send (juce::MidiMessage::noteOff (1, palmKey));
    expect (processor.getEffectivePlayStyleIndex() == dead,
            "releasing the newest HOLD key did not reveal an older held key");
    send (juce::MidiMessage::noteOff (1, deadKey));
    send (juce::MidiMessage::noteOff (1, deadKey));
    expect (processor.getEffectivePlayStyleIndex() == sustain,
            "HOLD did not return to base or ignored a stray release");

    // The visible PLAY STYLE strip always selects the fallback. Changing it
    // cannot interrupt a physical key that is still held.
    send (juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127));
    processor.triggerArticulation (
        electry::ElectryEngine::pickStyleKeyswitchCount + dead);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentPlayStyleIndex() == dead
                && processor.getEffectivePlayStyleIndex() == palm,
            "a PLAY STYLE base change interrupted a held override");
    send (juce::MidiMessage::noteOff (1, palmKey));
    expect (processor.getEffectivePlayStyleIndex() == dead,
            "a held override did not return to the new PLAY STYLE base");

    // The on-screen keyboard follows the same temporary-key contract, while
    // Pick Stroke remains latched in either mode.
    processor.keyboardState.noteOn (1, palmKey, 1.0f);
    renderBlock (processor, audio, midi);
    expect (processor.getEffectivePlayStyleIndex() == palm,
            "on-screen HOLD key did not override the base");
    processor.keyboardState.noteOff (1, palmKey, 0.0f);
    renderBlock (processor, audio, midi);
    expect (processor.getEffectivePlayStyleIndex() == dead,
            "on-screen HOLD release did not return to base");
    const int upKey = electry::ElectryEngine::firstKeyswitchNote
                    + static_cast<int> (electry::PickStyle::Up);
    send (juce::MidiMessage::noteOn (1, upKey, (juce::uint8) 127));
    send (juce::MidiMessage::noteOff (1, upKey));
    expect (processor.getCurrentPickStyleIndex()
                == static_cast<int> (electry::PickStyle::Up),
            "HOLD changed the Pick Stroke bank from latching");

    // Controller resets are intentionally distinct: CC64/121 and CC120 keep
    // a physically held style, whereas All Notes Off releases that key state.
    send (juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127));
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getEffectivePlayStyleIndex() == palm,
            "CC64 or CC121 released a held play-style key");
    send (juce::MidiMessage::controllerEvent (1, 120, 0));
    expect (processor.getEffectivePlayStyleIndex() == palm,
            "All Sound Off discarded a still-held play-style key");
    send (juce::MidiMessage::noteOff (1, palmKey));
    send (juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127));
    send (juce::MidiMessage::controllerEvent (1, 123, 0));
    expect (processor.getEffectivePlayStyleIndex() == dead,
            "All Notes Off left a temporary play-style override held");

    // Panic, a mode change, and release/prepare are hard lifecycle boundaries:
    // transient key ownership is cleared, but the saved base and mode survive.
    send (juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127));
    processor.requestPanic();
    renderBlock (processor, audio, midi);
    expect (processor.getEffectivePlayStyleIndex() == dead,
            "Panic left a temporary play-style override held");
    send (juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127));
    processor.setPlayStyleKeysHold (false);
    renderBlock (processor, audio, midi);
    expect (! processor.getPlayStyleKeysHold()
                && processor.getEffectivePlayStyleIndex() == dead,
            "switching to LATCH promoted a held override into the base");
    processor.setPlayStyleKeysHold (true);
    renderBlock (processor, audio, midi);
    send (juce::MidiMessage::noteOff (1, palmKey));
    expect (processor.getEffectivePlayStyleIndex() == dead,
            "a stale Note Off survived a play-style key-mode change");
    send (juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127));
    processor.releaseResources();
    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.getPlayStyleKeysHold()
                && processor.getCurrentPlayStyleIndex() == dead
                && processor.getEffectivePlayStyleIndex() == dead,
            "release/prepare restored a transient override instead of the base");

    // A delayed stroke snapshots the held articulation at playable Note On;
    // releasing the style key only changes future attacks.
    setParameterValue (processor, electry::parameters::strumSpread, 20.0f);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::lowestPlayableNote, (juce::uint8) 110), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, palmKey, (juce::uint8) 127), 0);
    renderBlock (processor, audio, midi);
    send (juce::MidiMessage::noteOff (1, palmKey));
    expect (capturedPlayStyle (
                processor, electry::ElectryEngine::lowestPlayableNote) == palm,
            "a HOLD release rewrote a delayed Palm attack");
    send (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 110));
    expect (capturedPlayStyle (processor, 40) == dead,
            "the first attack after HOLD release did not use the base style");

    // Only mode and base are state. A temporary override must not reopen as a
    // stuck key in a restored session.
    send (juce::MidiMessage::noteOn (1, palmKey, (juce::uint8) 127));
    juce::MemoryBlock heldState;
    processor.getStateInformation (heldState);
    ElectryAudioProcessor restored;
    restored.setStateInformation (heldState.getData(),
                                  static_cast<int> (heldState.getSize()));
    expect (restored.getPlayStyleKeysHold()
                && restored.getCurrentPlayStyleIndex() == dead
                && restored.getEffectivePlayStyleIndex() == dead,
            "state restored a transient HOLD override");
    restored.prepareToPlay (sampleRate, blockSize);
    expect (restored.getEffectivePlayStyleIndex() == dead,
            "prepare restored a serialized temporary HOLD override");
    restored.releaseResources();
    processor.releaseResources();
}

void testSameSampleMuteKeyswitchOrder()
{
    struct Attack
    {
        std::vector<float> audio;
        int latchedPlayStyle = -1;
    };

    const auto renderAttack = [] (electry::PlayStyle playStyle,
                                  bool playableNoteFirst)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        const auto keyswitch = juce::MidiMessage::noteOn (
            1, electry::ElectryEngine::firstPlayStyleKeyswitchNote
                   + static_cast<int> (playStyle),
            (juce::uint8) 127);
        const auto playable = juce::MidiMessage::noteOn (
            1, electry::ElectryEngine::lowestPlayableNote,
            (juce::uint8) 120);

        if (playableNoteFirst)
            midi.addEvent (playable, 0);
        midi.addEvent (keyswitch, 0);
        if (! playableNoteFirst)
            midi.addEvent (playable, 0);
        renderBlock (processor, audio, midi);

        const auto* channel = audio.getReadPointer (0);
        Attack result { std::vector<float> (channel,
                                             channel + audio.getNumSamples()),
                        processor.getCurrentPlayStyleIndex() };
        processor.releaseResources();
        return result;
    };

    const auto renderUiAttack = [] (electry::PlayStyle playStyle,
                                    bool playableNoteFirst)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);

        const auto queuePlayable = [&processor]
        {
            processor.keyboardState.noteOn (
                1, electry::ElectryEngine::lowestPlayableNote,
                120.0f / 127.0f);
        };
        const auto articulation =
            electry::ElectryEngine::pickStyleKeyswitchCount
            + static_cast<int> (playStyle);
        if (playableNoteFirst)
            queuePlayable();
        processor.triggerArticulation (articulation);
        if (! playableNoteFirst)
            queuePlayable();

        const int immediateLatch = processor.getCurrentPlayStyleIndex();
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        renderBlock (processor, audio, midi);
        const auto* channel = audio.getReadPointer (0);
        Attack result { std::vector<float> (channel,
                                             channel + audio.getNumSamples()),
                        immediateLatch };
        processor.releaseResources();
        return result;
    };

    const auto renderHostSwitchWithUiNote = [] (electry::PlayStyle playStyle,
                                                int samplePosition = 0)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);
        processor.keyboardState.noteOn (
            1, electry::ElectryEngine::lowestPlayableNote,
            120.0f / 127.0f);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (
            1, electry::ElectryEngine::firstPlayStyleKeyswitchNote
                   + static_cast<int> (playStyle),
            (juce::uint8) 127), samplePosition);
        renderBlock (processor, audio, midi);
        const auto* channel = audio.getReadPointer (0);
        Attack result { std::vector<float> (channel,
                                             channel + audio.getNumSamples()),
                        processor.getCurrentPlayStyleIndex() };
        processor.releaseResources();
        return result;
    };

    const auto energy = [] (const std::vector<float>& signal)
    {
        double result = 0.0;
        for (const float sample : signal)
            result += static_cast<double> (sample) * sample;
        return result;
    };

    const auto palmSwitchFirst = renderAttack (electry::PlayStyle::PalmMute, false);
    const auto palmNoteFirst = renderAttack (electry::PlayStyle::PalmMute, true);
    const auto deadSwitchFirst = renderAttack (electry::PlayStyle::Dead, false);
    const auto deadNoteFirst = renderAttack (electry::PlayStyle::Dead, true);
    const auto palmUiSwitchFirst = renderUiAttack (
        electry::PlayStyle::PalmMute, false);
    const auto palmUiNoteFirst = renderUiAttack (
        electry::PlayStyle::PalmMute, true);
    const auto deadUiSwitchFirst = renderUiAttack (
        electry::PlayStyle::Dead, false);
    const auto deadUiNoteFirst = renderUiAttack (
        electry::PlayStyle::Dead, true);
    const auto palmHostSwitchUiNote = renderHostSwitchWithUiNote (
        electry::PlayStyle::PalmMute);
    const auto deadHostSwitchUiNote = renderHostSwitchWithUiNote (
        electry::PlayStyle::Dead);
    const auto sustainUi = renderUiAttack (electry::PlayStyle::Sustain, false);
    const auto palmNegativeHostTime = renderHostSwitchWithUiNote (
        electry::PlayStyle::PalmMute, -1);
    const auto palmPastBlockHostTime = renderHostSwitchWithUiNote (
        electry::PlayStyle::PalmMute, blockSize + 1);

    expect (energy (palmSwitchFirst.audio) > 1.0e-8
                && energy (deadSwitchFirst.audio) > 1.0e-8,
            "same-sample mute keyswitch test notes did not sound");
    expect (palmNoteFirst.audio == palmSwitchFirst.audio,
            "Palm keyswitch at the note-on sample depended on MIDI insertion order");
    expect (deadNoteFirst.audio == deadSwitchFirst.audio,
            "Dead keyswitch at the note-on sample depended on MIDI insertion order");
    expect (palmNoteFirst.latchedPlayStyle
                == static_cast<int> (electry::PlayStyle::PalmMute)
                && deadNoteFirst.latchedPlayStyle
                       == static_cast<int> (electry::PlayStyle::Dead),
            "same-sample reversed mute keyswitch left the wrong play-style latch");
    expect (palmSwitchFirst.audio != deadSwitchFirst.audio,
            "Palm and Dead same-sample keyswitch attacks rendered identically");
    expect (palmUiNoteFirst.audio == palmUiSwitchFirst.audio
                && palmUiNoteFirst.audio == palmSwitchFirst.audio,
            "on-screen Palm attack depended on UI queue order");
    expect (deadUiNoteFirst.audio == deadUiSwitchFirst.audio
                && deadUiNoteFirst.audio == deadSwitchFirst.audio,
            "on-screen Dead attack depended on UI queue order");
    expect (palmHostSwitchUiNote.audio == palmSwitchFirst.audio
                && deadHostSwitchUiNote.audio == deadSwitchFirst.audio,
            "sample-zero host mute keyswitch did not condition an on-screen note");
    expect (palmNegativeHostTime.audio == sustainUi.audio
                && palmPastBlockHostTime.audio == sustainUi.audio,
            "a nonzero raw host timestamp retroactively conditioned an "
            "on-screen attack at the block boundary");
    expect (palmUiNoteFirst.latchedPlayStyle
                == static_cast<int> (electry::PlayStyle::PalmMute)
                && deadUiNoteFirst.latchedPlayStyle
                       == static_cast<int> (electry::PlayStyle::Dead),
            "on-screen mute style did not latch immediately before rendering");

    // The remainder pass must preserve GUI note order as well as Note Ons.
    // Losing or pulling the Note Off ahead of the attack would leave a stuck
    // low string after this same sample-zero conditioning merge.
    ElectryAudioProcessor releasedUiNote;
    releasedUiNote.prepareToPlay (sampleRate, blockSize);
    releasedUiNote.keyboardState.noteOn (
        1, electry::ElectryEngine::lowestPlayableNote, 120.0f / 127.0f);
    releasedUiNote.keyboardState.noteOff (
        1, electry::ElectryEngine::lowestPlayableNote, 0.0f);
    juce::AudioBuffer<float> releaseAudio;
    juce::MidiBuffer releaseMidi;
    releaseMidi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::firstPlayStyleKeyswitchNote
               + static_cast<int> (electry::PlayStyle::PalmMute),
        (juce::uint8) 127), 0);
    renderBlock (releasedUiNote, releaseAudio, releaseMidi);
    renderSeconds (releasedUiNote, releaseAudio, 3.0);
    expect (releasedUiNote.getActiveVoiceCount() == 0,
            "sample-zero conditioning merge lost an on-screen Note Off");
    releasedUiNote.releaseResources();
}

void testMidiControllersAndVoiceLifecycle()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    // Sustain pedal holds a released string.
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() == 1,
            "sustain pedal did not hold the released string");

    // Releasing the pedal lets the string damp out.
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "string did not retire after the sustain pedal released");

    // All Sound Off mutes immediately without changing the selected hands.
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::firstPlayStyleKeyswitchNote
               + static_cast<int> (electry::PlayStyle::PalmMute),
        (juce::uint8) 127), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::firstKeyswitchNote
               + static_cast<int> (electry::PickStyle::Alternate),
        (juce::uint8) 127), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 120, 0), 0);
    renderBlock (processor, audio, midi);
    const auto afterSoundOff = renderSeconds (processor, audio, 0.05);
    expect (afterSoundOff < 1.0e-4f, "All Sound Off left audible output");
    expect (processor.getCurrentPlayStyleIndex()
                == static_cast<int> (electry::PlayStyle::PalmMute)
                && processor.getCurrentPickStyleIndex()
                       == static_cast<int> (electry::PickStyle::Alternate),
            "All Sound Off cleared the Palm/Alternate latches");

    // CC120 clears sounding voices, not the physical pedal controller. A note
    // played afterwards must therefore remain held until CC121 resets CC64.
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::firstPlayStyleKeyswitchNote
               + static_cast<int> (electry::PlayStyle::Sustain),
        (juce::uint8) 127), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 1,
            "All Sound Off lost the held CC64 state for later notes");
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "Reset All Controllers did not clear CC64 after All Sound Off");

    // All Notes Off releases held strings musically.
    midi.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "All Notes Off did not release the held string");

    processor.releaseResources();
}

void testSameSamplePalmMutePressureAttack()
{
    // CC2 is decoded by the processor, while the attack is configured by the
    // engine at note-on. The same timestamp must behave exactly like a hand
    // that was already down, not like an open pick followed by muted decay.
    enum class PressureTiming
    {
        Unmuted,
        Primed,
        ControllerFirst,
        NoteFirst,
        GuiNoteController,
        ResetThenController,
        ControllerThenReset
    };

    const auto renderAttack = [] (PressureTiming timing)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (
            1, electry::ElectryEngine::firstPlayStyleKeyswitchNote
                   + static_cast<int> (electry::PlayStyle::Sustain),
            (juce::uint8) 127), 0);
        if (timing == PressureTiming::Primed)
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, 127), 0);
        renderBlock (processor, audio, midi); // align clock and settle parameters

        if (timing == PressureTiming::ControllerFirst)
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, 127), 0);
        else if (timing == PressureTiming::GuiNoteController)
        {
            processor.keyboardState.noteOn (
                1, electry::ElectryEngine::lowestPlayableNote,
                120.0f / 127.0f);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, 127), 0);
        }
        else if (timing == PressureTiming::ResetThenController)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, 127), 0);
        }
        else if (timing == PressureTiming::ControllerThenReset)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, 127), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
        }
        if (timing != PressureTiming::GuiNoteController)
            midi.addEvent (juce::MidiMessage::noteOn (
                1, electry::ElectryEngine::lowestPlayableNote,
                (juce::uint8) 120), 0);
        if (timing == PressureTiming::NoteFirst)
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, 127), 0);
        renderBlock (processor, audio, midi);

        const auto* channel = audio.getReadPointer (0);
        std::vector<float> result (channel, channel + audio.getNumSamples());
        processor.releaseResources();
        return result;
    };

    const auto unmuted = renderAttack (PressureTiming::Unmuted);
    const auto primed = renderAttack (PressureTiming::Primed);
    const auto controllerFirst = renderAttack (PressureTiming::ControllerFirst);
    const auto noteFirst = renderAttack (PressureTiming::NoteFirst);
    const auto guiNoteController = renderAttack (
        PressureTiming::GuiNoteController);
    const auto resetThenController = renderAttack (
        PressureTiming::ResetThenController);
    const auto controllerThenReset = renderAttack (
        PressureTiming::ControllerThenReset);

    const auto energy = [] (const std::vector<float>& signal)
    {
        double result = 0.0;
        for (const float sample : signal)
            result += static_cast<double> (sample) * sample;
        return result;
    };
    const auto differenceEnergy = [] (const std::vector<float>& signal,
                                      const std::vector<float>& reference)
    {
        double result = 0.0;
        for (std::size_t i = 0; i < signal.size(); ++i)
        {
            const double difference = static_cast<double> (signal[i])
                                    - reference[i];
            result += difference * difference;
        }
        return result;
    };

    expect (energy (controllerFirst) > 1.0e-8 && energy (noteFirst) > 1.0e-8,
            "same-sample CC2 test note did not sound");
    expect (controllerFirst == primed,
            "CC2=127 at the Sustain note-on sample did not shape the attack "
            "exactly like pressure applied before the note");
    expect (noteFirst == primed,
            "CC2=127 after the Sustain note-on at the same sample depended on "
            "MIDI insertion order");
    expect (guiNoteController == primed,
            "sample-zero CC2 did not condition an on-screen Sustain attack");
    expect (resetThenController == primed
                && controllerThenReset == unmuted,
            "same-sample CC121 did not preserve its order relative to CC2 "
            "before the Sustain attack");
    expect (differenceEnergy (controllerFirst, unmuted)
                > energy (controllerFirst) * 0.01,
            "same-sample CC2=127 left the Sustain attack effectively unmuted");
}

// ElectryAudioProcessor::decodePitchBend14()'s 14-bit reconstruction is exact
// integer/float arithmetic, so it is asserted on precisely here rather than
// only inferred from rendered audio: the low-order MIDI data byte alone
// contributes at most a few cents to a full two-semitone bend (127 of the
// 8191/8192-count range), which testPitchWheelMidiDispatch()'s audio
// measurement below cannot reliably resolve. An implementation that dropped
// the low-order byte entirely - value14 = data2 << 7 - would still pass a
// full-down/centre/full-up rendered-audio check, since data1 happens to be 0
// or 127 at exactly those three positions either way; fixing data2 and
// sweeping data1 catches that directly.
void testPitchWheelByteReconstruction()
{
    using Processor = ElectryAudioProcessor;

    // The three positions testPitchWheelMidiDispatch() renders, checked here
    // to their exact decoded value instead of only through audio.
    expect (Processor::decodePitchBend14 (0, 0) == -1.0f,
            "full-down pitch wheel (0x0000) did not decode to exactly -1.0");
    expect (Processor::decodePitchBend14 (0, 64) == 0.0f,
            "centred pitch wheel (0x2000) did not decode to exactly 0.0");
    expect (Processor::decodePitchBend14 (127, 127) == 1.0f,
            "full-up pitch wheel (0x3fff) did not decode to exactly 1.0");

    // Fix the high-order byte at its centre-position value (64) and sweep
    // the low-order one across its full range. A decode that used only
    // data2 would return exactly 0.0 for all three, since they all share
    // data2 = 64.
    const float atLowByte0 = Processor::decodePitchBend14 (0, 64);
    const float atLowByte64 = Processor::decodePitchBend14 (64, 64);
    const float atLowByte127 = Processor::decodePitchBend14 (127, 64);
    expect (std::abs (atLowByte64 - 64.0f / 8191.0f) < 1.0e-6f,
            "data1=64, data2=64 (value14=8256) did not decode to its exact "
            "fractional bend");
    expect (std::abs (atLowByte127 - 127.0f / 8191.0f) < 1.0e-6f,
            "data1=127, data2=64 (value14=8319) did not decode to its exact "
            "fractional bend");
    expect (atLowByte0 != atLowByte64 && atLowByte64 != atLowByte127,
            "varying only the low-order MIDI data byte at or above centre "
            "left the decoded bend unchanged, meaning it was not read");

    // The same sweep just below centre, so both the value14 < 8192 and
    // value14 >= 8192 branches of the asymmetric divisor are exercised with
    // a fixed high-order byte.
    const float belowLow0 = Processor::decodePitchBend14 (0, 63);
    const float belowLow127 = Processor::decodePitchBend14 (127, 63);
    expect (std::abs (belowLow0 - (-128.0f / 8192.0f)) < 1.0e-6f,
            "data1=0, data2=63 (value14=8064) did not decode to its exact "
            "fractional bend");
    expect (std::abs (belowLow127 - (-1.0f / 8192.0f)) < 1.0e-6f,
            "data1=127, data2=63 (value14=8191) did not decode to its exact "
            "fractional bend");
    expect (belowLow0 != belowLow127,
            "varying only the low-order MIDI data byte below centre left "
            "the decoded bend unchanged, meaning it was not read");
}

// dispatchMidiData() reconstructs the pitch wheel's 14-bit position from its
// two 7-bit MIDI data bytes and then divides the excursion below centre by
// 8192 but above centre by 8191, matching the MIDI spec's asymmetric bend
// range - byte-level parsing that lives only in the shell and is untouched by
// any other test: ElectryEngineTests drives ElectryEngine::setPitchBend()
// directly with already-decoded floats, and this file's own controller test
// only exercises 7-bit CCs. A wrong byte order, a swapped divisor, a dropped
// clamp or a sign flip here would still pass every existing test while
// bending every host's pitch wheel by the wrong amount or the wrong
// direction, so this measures the actual rendered pitch raw pitch-wheel
// messages produce on both the full-range low string and a less-compliant open
// string, as an end-to-end check alongside
// testPitchWheelByteReconstruction()'s exact one.
void testPitchWheelMidiDispatch()
{
    constexpr double openLowStringHz = 41.2034; // E1, MIDI note 28
    constexpr int lessCompliantOpenNote = 50;   // D3
    constexpr double lessCompliantOpenHz = 146.83238;

    // measureFundamentalHz() only scans +/-120 cents around its seed, so each
    // case is seeded near the pitch its string should produce. E1 has bend
    // sensitivity 1.0; D3's search uses the halfway-wheel pitch because its
    // smaller full-up travel lies near one semitone.
    const auto nominalHz = [] (double openHz, int wheelPosition14) -> double
    {
        const double excursion = wheelPosition14 < 8192
            ? static_cast<double> (wheelPosition14 - 8192) / 8192.0
            : static_cast<double> (wheelPosition14 - 8192) / 8191.0;
        return openHz * std::pow (2.0, 2.0 * excursion / 12.0);
    };

    const auto measuredHz = [] (int midiNote, int wheelPosition14,
                                double expectedHz) -> double
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::pitchWheel (1, wheelPosition14), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            1, midiNote, (juce::uint8) 100), 0);
        renderBlock (processor, audio, midi);

        // Let the pick transient decay and the bend glide (bend time
        // defaults to 280 ms) fully settle before measuring, then capture a
        // separate steady window to analyse.
        renderSeconds (processor, audio, 0.5);
        const auto settled = renderCapture (processor, audio, 0.4);
        processor.releaseResources();
        return measureFundamentalHz (settled, 0, static_cast<int> (settled.size()),
                                     sampleRate, expectedHz);
    };

    const auto centre = measuredHz (
        electry::ElectryEngine::lowestPlayableNote, 8192,
        nominalHz (openLowStringHz, 8192));
    const auto bentUp = measuredHz (
        electry::ElectryEngine::lowestPlayableNote, 16383,
        nominalHz (openLowStringHz, 16383));
    const auto bentDown = measuredHz (
        electry::ElectryEngine::lowestPlayableNote, 0,
        nominalHz (openLowStringHz, 0));

    const auto centsUp = 1200.0 * std::log2 (bentUp / centre);
    const auto centsDown = 1200.0 * std::log2 (bentDown / centre);

    expect (centsUp > 170.0 && centsUp < 230.0,
            "a full-up pitch wheel (0x3fff) did not bend the open low string "
            "up by the documented two semitones (measured "
                + std::to_string (centsUp) + " cents)");
    expect (centsDown < -170.0 && centsDown > -230.0,
            "a full-down pitch wheel (0x0000) did not bend the open low "
            "string down by the documented two semitones (measured "
                + std::to_string (centsDown) + " cents)");

    // A centred message has to undo a real prior bend on the same ringing
    // voice. Both cases below receive the centre event at the same string age;
    // dropping the second event leaves the first case about 200 cents sharp.
    const auto afterCentreHz = [] (int initialWheelPosition14) -> double
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (
            juce::MidiMessage::pitchWheel (1, initialWheelPosition14), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            1, electry::ElectryEngine::lowestPlayableNote, (juce::uint8) 100), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 0.9);

        midi.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 0.5);
        const auto settled = renderCapture (processor, audio, 0.4);
        processor.releaseResources();
        const double midpointHz = openLowStringHz * std::pow (2.0, 1.0 / 12.0);
        return measureFundamentalHz (settled, 0, static_cast<int> (settled.size()),
                                     sampleRate, midpointHz);
    };

    const auto centredReference = afterCentreHz (8192);
    const auto recentered = afterCentreHz (16383);
    const auto recenterErrorCents = 1200.0
        * std::log2 (recentered / centredReference);
    // The audio estimator's final scan is quantized to 0.5 cents; allow one
    // bin while still separating this path decisively from a missed reset.
    expect (std::abs (recenterErrorCents) < 0.75,
            "a centred pitch wheel (0x2000) did not return the pre-bent open "
            "low string to its matched unbent pitch (measured "
                + std::to_string (recenterErrorCents) + " cents)");

    // The identical raw full-up message produces less travel on the stiffer
    // open D3 than on the maximally compliant E1. This keeps the processor
    // integration check coupled to the engine's per-string bar behaviour.
    const auto lessCompliantCentre = measuredHz (
        lessCompliantOpenNote, 8192,
        nominalHz (lessCompliantOpenHz, 8192));
    const auto lessCompliantBentUp = measuredHz (
        lessCompliantOpenNote, 16383,
        nominalHz (lessCompliantOpenHz, 12288));
    const auto lessCompliantTravelCents = 1200.0
        * std::log2 (lessCompliantBentUp / lessCompliantCentre);
    expect (lessCompliantTravelCents > 60.0
                && centsUp - lessCompliantTravelCents > 50.0,
            "the same full-up MIDI wheel message did not bend open D3 "
            "materially less than open E1 (measured "
                + std::to_string (lessCompliantTravelCents) + " versus "
                + std::to_string (centsUp) + " cents)");
}

// The CC1 resonance and the acoustic-return wiring live in the shell: the
// processor pushes each processed block back into the engine and derives the
// rig's loudness from its amplifier controls. This closes the actual plug-in
// loop, so deleting the pushAcousticReturn call, the CC1 dispatch or the
// return-level derivation makes it fail.
void testResonanceWheelFeedback()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    setParameterValue (processor, electry::parameters::amp, 0.9f);
    setParameterValue (processor, electry::parameters::distortion, 0.7f);
    setParameterValue (processor, electry::parameters::resonanceDepth, 100.0f);

    const auto playAndRelease = [&] (int wheelValue)
    {
        processor.requestPanic();
        renderBlock (processor, audio, midi);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, wheelValue), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 120), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 1.0);
        midi.addEvent (juce::MidiMessage::noteOff (1, 40), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 2.0);
        // The window well after the released string itself has damped: only
        // the regenerating loop can still be loud here.
        return renderSeconds (processor, audio, 0.5);
    };

    const auto fed = playAndRelease (127);
    const auto decayed = playAndRelease (0);
    expect (fed > 1.0e-3f,
            "the resonance wheel did not sustain a released distorted note "
            "through the acoustic return (late peak "
                + std::to_string (fed) + ")");
    expect (decayed < fed * 0.25f,
            "a released note with the wheel down failed to decay (late peak "
                + std::to_string (decayed) + " against fed "
                + std::to_string (fed) + ")");

    processor.releaseResources();
}

// Channel pressure (status nibble 0xd0, one data byte) and polyphonic
// aftertouch (status nibble 0xa0, note number then pressure) both dispatch to
// ElectryEngine::setVibrato() through raw MIDI byte reads in
// dispatchMidiData() - data[1] for channel pressure, but data[2] for
// polyphonic aftertouch, since its data[1] is the note number instead - a
// path with no coverage anywhere else: ElectryEngineTests drives
// setVibrato() directly with already-decoded floats, and this file's own
// testMidiControllersAndVoiceLifecycle() and testPerformanceControls() only
// ever send 7-bit CCs (status 0xb0). A status-nibble typo, a swapped data
// index, a dropped clamp or a missing "/ 127.0f" here would leave every
// host's pressure/aftertouch vibrato silently inert (or scaled wrong) while
// still passing every existing test. This measures the actual rendered pitch
// on a fretted (not open) string: the vibrato is documented to push a
// fingered string sharp and never flat (see ElectryEngine.h's setVibrato()
// and README.md's "Fretting-hand vibrato"), so once its 258 ms onset ramp has
// settled, a window spanning several of its ~5-6 Hz swing cycles reads
// measurably sharper than the same window with no pressure applied, even
// though the finger dwells at the fretted pitch between excursions.
void testChannelPressureAndAftertouchVibratoDispatch()
{
    // A2 open (45) + 2 frets, the same fixture ElectryEngineTests' own
    // testFrettingHandVibrato() uses: fretted, so the hand has a string to
    // rock. An open string is deliberately left alone by design and would
    // make this a no-op either way.
    constexpr int frettedNote = 47;
    const double frettedHz = 440.0 * std::pow (2.0, (frettedNote - 69) / 12.0);

    enum class Pressure
    {
        None,
        ChannelPressure,
        // Half-value, not full-value: both dispatch branches clamp their
        // decoded float into setVibrato(), so a full-value 127 alone cannot
        // tell a genuine "/ 127.0f" scale apart from a missing one - either
        // way the clamp lands on the same 1.0f. A half-value message only
        // reproduces the full-value bias if the byte was actually divided
        // down first.
        HalfChannelPressure,
        // Channel 9, not channel 1: dispatchMidiData masks the status byte
        // with "& 0xf0u" before comparing it, so the channel nibble should
        // never matter. Sending channel 1 only would let a regression that
        // compared the whole status byte (0xd0 exactly) instead of the
        // masked nibble pass unnoticed on every channel but the one tested.
        ChannelPressureOtherChannel,
        PolyAftertouch,
        // Same half-value reasoning as HalfChannelPressure, applied to the
        // independent data[2] scale in the aftertouch branch: a full-value
        // 127 alone cannot tell "data[2] / 127.0f" apart from a missing
        // divisor, since both clamp to the same 1.0f.
        HalfPolyAftertouch,
    };
    const auto measuredHz = [&] (Pressure kind) -> double
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (
            1, frettedNote, (juce::uint8) 100), 0);
        if (kind == Pressure::ChannelPressure)
            midi.addEvent (juce::MidiMessage::channelPressureChange (1, 127), 0);
        else if (kind == Pressure::HalfChannelPressure)
            midi.addEvent (juce::MidiMessage::channelPressureChange (1, 64), 0);
        else if (kind == Pressure::ChannelPressureOtherChannel)
            midi.addEvent (juce::MidiMessage::channelPressureChange (9, 127), 0);
        else if (kind == Pressure::PolyAftertouch)
            // The note byte deliberately does not name the fretted note:
            // dispatchMidiData never reads it (Electry has one fretting hand
            // for the whole engine, not one per key), so a swapped-index
            // regression that read this byte as the pressure instead of
            // data[2] would decode to 3 / 127 - far too small to sharpen the
            // note - rather than accidentally producing a passing result the
            // way reusing frettedNote (47) here would.
            midi.addEvent (
                juce::MidiMessage::aftertouchChange (1, 3, 127), 0);
        else if (kind == Pressure::HalfPolyAftertouch)
            midi.addEvent (
                juce::MidiMessage::aftertouchChange (1, 3, 64), 0);
        renderBlock (processor, audio, midi);

        // Let the attack settle and the vibrato's onset ramp (258 ms, see
        // ElectryEngine.h's vibratoOnsetSeconds) reach its steady swing
        // before measuring, then capture several full cycles so the window's
        // own frequency estimate reflects the vibrato's genuine time-average
        // rather than one arbitrary phase of it.
        renderSeconds (processor, audio, 0.5);
        const auto settled = renderCapture (processor, audio, 0.6);
        processor.releaseResources();
        return measureFundamentalHz (settled, 0, static_cast<int> (settled.size()),
                                     sampleRate, frettedHz);
    };

    const auto still = measuredHz (Pressure::None);
    const auto pressed = measuredHz (Pressure::ChannelPressure);
    const auto halfPressed = measuredHz (Pressure::HalfChannelPressure);
    const auto pressedOtherChannel =
        measuredHz (Pressure::ChannelPressureOtherChannel);
    const auto touched = measuredHz (Pressure::PolyAftertouch);
    const auto halfTouched = measuredHz (Pressure::HalfPolyAftertouch);

    const auto centsStill = 1200.0 * std::log2 (still / frettedHz);
    const auto centsPressed = 1200.0 * std::log2 (pressed / frettedHz);
    const auto centsHalfPressed = 1200.0 * std::log2 (halfPressed / frettedHz);
    const auto centsPressedOtherChannel =
        1200.0 * std::log2 (pressedOtherChannel / frettedHz);
    const auto centsTouched = 1200.0 * std::log2 (touched / frettedHz);
    const auto centsHalfTouched = 1200.0 * std::log2 (halfTouched / frettedHz);

    expect (std::abs (centsStill) < 10.0,
            "an unpressed fretted note drifted " + std::to_string (centsStill)
                + " cents off its fretted pitch");
    // Differential rather than an absolute cents target, for the same reason
    // ElectryEngineTests' own testFrettingHandVibrato() compares two engines
    // instead of asserting one fixed value: the vibrato's average bias over a
    // multi-cycle window (rock^2 time-averages to a fraction of the nominal
    // 40-cent excursion, not the excursion itself) is a function of the
    // depth/rate constants rather than a documented number, so the robust
    // assertion is that applying either message sharpens the note well beyond
    // the unpressed measurement's own noise floor. An upper bound catches a
    // runaway/nonsense measurement without pinning the exact bias.
    expect (centsPressed - centsStill > 5.0 && centsPressed < 100.0,
            "full-value channel pressure (status 0xd0) did not sharpen the "
            "fretted note by the vibrato's documented upward bias (measured "
                + std::to_string (centsPressed) + " cents against "
                + std::to_string (centsStill) + " unpressed)");
    // Half-value pressure must still clear the noise floor - it is a real
    // press, not a no-op - but land measurably below the full-value bias, the
    // signature of an actual "/ 127.0f" scale rather than a clamp that
    // saturates at 1.0f for any nonzero byte.
    expect (centsHalfPressed - centsStill > 2.0
                && centsHalfPressed < centsPressed - 2.0,
            "half-value channel pressure did not land strictly between "
            "unpressed and full-value pressure (measured "
                + std::to_string (centsHalfPressed) + " cents against "
                + std::to_string (centsStill) + " unpressed and "
                + std::to_string (centsPressed) + " full-value)");
    // Same message on channel 9 rather than channel 1: dispatchMidiData is
    // documented to mask the channel nibble out of the status byte before
    // comparing it, so this should sharpen the note exactly as the channel-1
    // case does.
    expect (centsPressedOtherChannel - centsStill > 5.0
                && centsPressedOtherChannel < 100.0,
            "full-value channel pressure on channel 9 did not sharpen the "
            "fretted note the same way channel 1 does (measured "
                + std::to_string (centsPressedOtherChannel)
                + " cents against " + std::to_string (centsStill)
                + " unpressed)");
    expect (centsTouched - centsStill > 5.0 && centsTouched < 100.0,
            "full-value polyphonic aftertouch (status 0xa0) did not sharpen "
            "the fretted note by the vibrato's documented upward bias "
            "(measured " + std::to_string (centsTouched) + " cents against "
                + std::to_string (centsStill) + " unpressed)");
    // Same half-value-versus-full-value reasoning as the channel-pressure
    // case, applied to the aftertouch branch's own independent data[2] scale.
    expect (centsHalfTouched - centsStill > 2.0
                && centsHalfTouched < centsTouched - 2.0,
            "half-value polyphonic aftertouch did not land strictly between "
            "unpressed and full-value aftertouch (measured "
                + std::to_string (centsHalfTouched) + " cents against "
                + std::to_string (centsStill) + " unpressed and "
                + std::to_string (centsTouched) + " full-value)");
}

// dispatchMidiData()'s handling of MIDI CC121 (Reset All Controllers) fans
// out into five separate engine calls - setPitchBend(0), setResonance(0),
// setPalmMutePressure(0), setVibrato(0) and setSustainPedal(false) - a path no
// existing test drives: testMidiControllersAndVoiceLifecycle() only ever
// sends CC64, CC120 and CC123, and every other performance control is
// exercised through its own dedicated controller number, never CC121. A
// dropped call, a wrong controller number, or a typo'd reset value here would
// leave a bent pitch wheel or a held sustain pedal stuck exactly when a host
// issues the routine Reset All Controllers message a DAW sends on transport
// stop or program change, while every existing test kept passing. This drives
// two of the five resets end to end through actual rendered audio - the
// pitch-bend glide, mirroring testPitchWheelMidiDispatch(), and the
// sustain-pedal release, mirroring testMidiControllersAndVoiceLifecycle() -
// since both have an audible, unambiguous signature a dropped call would
// miss.
void testResetAllControllersDispatch()
{
    constexpr double openLowStringHz = 41.2034; // E1, MIDI note 28

    // Pitch bend: bend the open low string fully up, let the glide settle,
    // then send Reset All Controllers and confirm the bend glides back to the
    // unbent pitch exactly as a centred pitch wheel would.
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            1, electry::ElectryEngine::lowestPlayableNote, (juce::uint8) 100), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 0.5);

        const auto bentSettled = renderCapture (processor, audio, 0.4);
        const auto bentHz = measureFundamentalHz (
            bentSettled, 0, static_cast<int> (bentSettled.size()), sampleRate,
            openLowStringHz * std::pow (2.0, 2.0 / 12.0));
        const auto centsBent = 1200.0 * std::log2 (bentHz / openLowStringHz);
        expect (centsBent > 170.0,
                "setup: a full-up pitch wheel did not bend the open low "
                "string before the reset (measured "
                    + std::to_string (centsBent) + " cents)");

        midi.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 0.5);

        const auto resetSettled = renderCapture (processor, audio, 0.4);
        const auto resetHz = measureFundamentalHz (
            resetSettled, 0, static_cast<int> (resetSettled.size()), sampleRate,
            openLowStringHz);
        const auto centsAfterReset = 1200.0 * std::log2 (resetHz / openLowStringHz);
        expect (std::abs (centsAfterReset) < 10.0,
                "Reset All Controllers (CC121) did not clear a pending pitch "
                "bend (measured " + std::to_string (centsAfterReset)
                    + " cents from the open string)");
        processor.releaseResources();
    }

    // Sustain pedal: hold the pedal down, release a note (which only flags
    // the voice sustained rather than stopping it - see setSustainPedal()'s
    // documented CC64 semantics), then send Reset All Controllers and confirm
    // the flagged voice is released exactly as an explicit pedal-up would.
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        renderBlock (processor, audio, midi);
        midi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
        renderBlock (processor, audio, midi);
        expect (processor.getActiveVoiceCount() == 1,
                "setup: the sustain pedal did not hold the released string");

        midi.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 3.0);
        expect (processor.getActiveVoiceCount() == 0,
                "Reset All Controllers (CC121) did not release a "
                "sustain-held string");
        processor.releaseResources();
    }
}

void testMutePressureDisplayFeedback()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.getMidiMutePressureForDisplay() == 0,
            "a prepared processor displayed stale CC2 mute pressure");

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    const auto sendController = [&] (int controller, int value)
    {
        midi.addEvent (juce::MidiMessage::controllerEvent (
            1, controller, value), 0);
        renderBlock (processor, audio, midi);
    };

    sendController (2, 127);
    expect (processor.getMidiMutePressureForDisplay() == 127,
            "CC2 mute pressure did not reach the editor-facing state");

    // All Sound Off silences strings but deliberately preserves physical
    // controller state, so its readout must agree with the next attack.
    sendController (120, 0);
    expect (processor.getMidiMutePressureForDisplay() == 127,
            "All Sound Off hid the CC2 pressure it preserves in the engine");

    sendController (121, 0);
    expect (processor.getMidiMutePressureForDisplay() == 0,
            "Reset All Controllers left stale CC2 pressure in the editor");

    ElectryStatusDisplay status;
    status.setStatus (1, 0, true, sampleRate, 127, false);
    expect (status.getStatusText().contains ("CC2 MUTE +100%")
                && status.getTitle() == status.getStatusText(),
            "the status display did not expose full live CC2 mute pressure");
    status.setStatus (1, 0, true, sampleRate, 0, false);
    expect (! status.getStatusText().contains ("CC2 MUTE")
                && status.getStatusText().contains ("48.0 kHz"),
            "the status display did not restore its normal readout at CC2 zero");

    sendController (2, 64);
    processor.releaseResources();
    expect (processor.getMidiMutePressureForDisplay() == 0,
            "releaseResources left stale CC2 mute pressure visible");
}

void testUiArticulationTriggerAndPanic()
{
    enum class ResetPath { None, Panic, AllSoundOff };
    const auto renderPalmAttack = [] (ResetPath resetPath)
    {
        ElectryAudioProcessor attackProcessor;
        attackProcessor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> attackAudio;
        juce::MidiBuffer attackMidi;
        const int palmArticulation =
            electry::ElectryEngine::pickStyleKeyswitchCount
            + static_cast<int> (electry::PlayStyle::PalmMute);
        if (resetPath == ResetPath::Panic)
        {
            attackProcessor.triggerArticulation (palmArticulation);
            attackProcessor.requestPanic();
        }
        else
        {
            attackMidi.addEvent (juce::MidiMessage::noteOn (
                1, electry::ElectryEngine::firstKeyswitchNote
                       + palmArticulation,
                (juce::uint8) 127), 0);
        }
        if (resetPath == ResetPath::AllSoundOff)
        {
            // Apply the host latch first, then require CC120's audio-thread
            // reset to preserve its sound, not only the display integer.
            renderBlock (attackProcessor, attackAudio, attackMidi);
            attackMidi.addEvent (
                juce::MidiMessage::controllerEvent (1, 120, 0), 0);
        }
        attackMidi.addEvent (juce::MidiMessage::noteOn (
            1, electry::ElectryEngine::lowestPlayableNote,
            (juce::uint8) 120), 0);
        renderBlock (attackProcessor, attackAudio, attackMidi);
        const auto* channel = attackAudio.getReadPointer (0);
        std::vector<float> result (channel,
                                   channel + attackAudio.getNumSamples());
        attackProcessor.releaseResources();
        return result;
    };

    const auto directPalm = renderPalmAttack (ResetPath::None);
    expect (renderPalmAttack (ResetPath::Panic) == directPalm,
            "panic preserved the visible Palm latch but reset its audible style");
    expect (renderPalmAttack (ResetPath::AllSoundOff) == directPalm,
            "CC120 preserved the visible Palm latch but reset its audible style");

    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    // The editor's keyswitch buttons route through the UI queue: index 0..2
    // is the picking bank, 3..6 the play-style bank.
    const auto harmonicsKeyswitch = electry::ElectryEngine::pickStyleKeyswitchCount
                                  + static_cast<int> (electry::PlayStyle::Harmonics);
    processor.triggerArticulation (harmonicsKeyswitch);
    processor.triggerArticulation (static_cast<int> (electry::PickStyle::Alternate));
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentPlayStyleIndex()
                == static_cast<int> (electry::PlayStyle::Harmonics),
            "UI keyswitch trigger did not latch Harmonics");
    expect (processor.getCurrentPickStyleIndex()
                == static_cast<int> (electry::PickStyle::Alternate),
            "UI keyswitch trigger did not latch Alternate");

    processor.triggerArticulation (99);
    renderBlock (processor, audio, midi);
    expect (processor.getCurrentPlayStyleIndex()
                    == static_cast<int> (electry::PlayStyle::Harmonics)
                && processor.getCurrentPickStyleIndex()
                       == static_cast<int> (electry::PickStyle::Alternate),
            "out-of-range keyswitch index was not ignored");

    // Panic silences a ringing string within one block.
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 110), 0);
    renderBlock (processor, audio, midi);
    processor.triggerArticulation (static_cast<int> (electry::PickStyle::Up));
    processor.triggerArticulation (
        electry::ElectryEngine::pickStyleKeyswitchCount
            + static_cast<int> (electry::PlayStyle::PalmMute));
    processor.requestPanic();
    renderBlock (processor, audio, midi);
    const auto peak = renderSeconds (processor, audio, 0.05);
    expect (peak < 1.0e-4f, "panic left audible output");
    expect (processor.getActiveVoiceCount() == 0, "panic left active strings");
    expect (processor.getCurrentPlayStyleIndex()
                == static_cast<int> (electry::PlayStyle::PalmMute)
                && processor.getCurrentPickStyleIndex()
                       == static_cast<int> (electry::PickStyle::Up),
            "panic discarded the newly requested articulation latches");

    processor.releaseResources();
}

void testOutputGainImpact()
{
    const auto renderPeak = [] (float outputDb)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::output, outputDb);

        juce::AudioBuffer<float> audio;
        // Let the output-gain smoother settle before the note so the pluck is
        // measured at the requested level, not part-way through the
        // anti-zipper ramp from the default level.
        renderSeconds (processor, audio, 0.1);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        renderBlock (processor, audio, midi);
        const auto peak = renderSeconds (processor, audio, 0.4);
        processor.releaseResources();
        return peak;
    };

    const auto quiet = renderPeak (-24.0f);
    const auto loud = renderPeak (0.0f);
    // -24 dB to 0 dB is a 15.85x change; require most of it to survive the
    // pluck and output guard.
    expect (loud > quiet * 8.0f,
            "output level does not scale the rendered signal (quiet "
                + std::to_string (quiet) + ", loud " + std::to_string (loud) + ")");
}

void testPerformanceControls()
{
    // Peak level of the window that starts `skipSeconds` after a note-off, so
    // the played string's own damping ramp is excluded and only what is still
    // ringing afterwards is measured.
    const auto tailAfterRelease = [] (float sympatheticPercent, float palmMutePercent,
                                      int palmMuteCc)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::sympathetic,
                           sympatheticPercent);
        setParameterValue (processor, electry::parameters::palmMute, palmMutePercent);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        if (palmMuteCc >= 0)
            midi.addEvent (juce::MidiMessage::controllerEvent (
                1, 2, (juce::uint8) palmMuteCc), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 0.05);

        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 110), 0);
        renderBlock (processor, audio, midi);
        const auto held = renderSeconds (processor, audio, 0.5);
        midi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 1.0);
        const auto tail = renderSeconds (processor, audio, 1.0);
        processor.releaseResources();
        return std::make_pair (held, tail);
    };

    const auto bypassed = tailAfterRelease (0.0f, 0.0f, -1);
    const auto coupled = tailAfterRelease (1.0f, 0.0f, -1);
    expect (bypassed.first > 1.0e-3f && coupled.first > 1.0e-3f,
            "the test note did not sound");
    expect (coupled.second > bypassed.second * 4.0f,
            "the sympathetic parameter did not ring the unplayed strings ("
                + std::to_string (bypassed.second) + " -> "
                + std::to_string (coupled.second) + ")");

    // Palm-mute pressure shortens the note itself, from the parameter and from
    // MIDI CC2 alike.
    const auto openHeld = tailAfterRelease (0.0f, 0.0f, -1).first;
    const auto mutedHeld = tailAfterRelease (0.0f, 1.0f, -1).first;
    const auto ccMutedHeld = tailAfterRelease (0.0f, 0.0f, 127).first;
    expect (mutedHeld < openHeld,
            "the palm-mute parameter did not damp the string");
    expect (ccMutedHeld < openHeld,
            "MIDI CC2 did not damp the string");

    // Spreading a chord lowers its stacked initial peak because the strings no
    // longer all start on the same sample.
    const auto chordPeak = [] (float spreadMs)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::strumSpread, spreadMs);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        renderSeconds (processor, audio, 0.05);
        for (const int note : { 28, 40, 45, 50, 55, 64 })
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 105), 0);
        renderBlock (processor, audio, midi);
        // Sequenced deliberately: renderSeconds overwrites the buffer, so the
        // first block's magnitude has to be taken before it runs.
        const auto firstBlock = audio.getMagnitude (0, blockSize);
        const auto rest = renderSeconds (processor, audio, 0.03);
        processor.releaseResources();
        return std::max (firstBlock, rest);
    };
    expect (chordPeak (40.0f) < chordPeak (0.0f),
            "strum spread did not stagger the chord's attack");
}

void testOutputModeAudioField()
{
    struct ChannelResult
    {
        double leftRms = 0.0;
        double rightRms = 0.0;
        double differenceRatio = 0.0;
        bool identical = true;
        std::uint64_t leftHash = 0;
        std::uint64_t rightHash = 0;
    };

    const auto render = [] (float mode, int midiNote)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::outputMode, mode);

        juce::AudioBuffer<float> audio;
        renderSeconds (processor, audio, 0.05); // settle the mode crossfade

        double leftEnergy = 0.0;
        double rightEnergy = 0.0;
        double differenceEnergy = 0.0;
        std::uint64_t sampleCount = 0;
        std::uint64_t leftHash = 1469598103934665603ull;
        std::uint64_t rightHash = 1469598103934665603ull;
        bool identical = true;
        for (int block = 0; block < 48; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (
                    1, midiNote, (juce::uint8) 102), 0);
            renderBlock (processor, audio, midi);
            if (block < 2)
                continue;
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const double left = audio.getSample (0, sample);
                const double right = audio.getSample (1, sample);
                const float leftSample = audio.getSample (0, sample);
                const float rightSample = audio.getSample (1, sample);
                leftEnergy += left * left;
                rightEnergy += right * right;
                differenceEnergy += (left - right) * (left - right);
                identical = identical
                    && std::memcmp (&leftSample, &rightSample,
                                    sizeof (leftSample)) == 0;
                std::uint32_t leftBits = 0;
                std::uint32_t rightBits = 0;
                std::memcpy (&leftBits, &leftSample, sizeof (leftBits));
                std::memcpy (&rightBits, &rightSample, sizeof (rightBits));
                leftHash = (leftHash ^ leftBits) * 1099511628211ull;
                rightHash = (rightHash ^ rightBits) * 1099511628211ull;
                ++sampleCount;
            }
        }
        processor.releaseResources();
        const double divisor = static_cast<double> (std::max<std::uint64_t> (
            sampleCount, 1));
        const double meanEnergy = 0.5 * (leftEnergy + rightEnergy);
        return ChannelResult {
            std::sqrt (leftEnergy / divisor),
            std::sqrt (rightEnergy / divisor),
            std::sqrt (differenceEnergy / std::max (meanEnergy, 1.0e-20)),
            identical, leftHash, rightHash
        };
    };

    const auto mono = render (0.0f, electry::ElectryEngine::lowestPlayableNote);
    expect (mono.identical, "Mono output parameter did not produce exact dual mono");

    const auto stereoLow = render (
        1.0f, electry::ElectryEngine::lowestPlayableNote);
    expect (stereoLow.leftRms > stereoLow.rightRms * 1.08,
            "Stereo APVTS parameter did not spread the low string left");

    const auto stereoHigh = render (1.0f, 64);
    expect (stereoHigh.rightRms > stereoHigh.leftRms * 1.08,
            "Stereo APVTS parameter did not spread the high string right");

    const auto doubled = render (
        2.0f, electry::ElectryEngine::lowestPlayableNote);
    const auto repeatedDouble = render (
        2.0f, electry::ElectryEngine::lowestPlayableNote);
    expect (! doubled.identical && doubled.differenceRatio > 0.02,
            "Double did not render two distinct Electry performances");
    expect (doubled.leftHash == mono.leftHash,
            "Double changed the established primary-engine performance");
    expect (doubled.leftRms > doubled.rightRms * 0.70
                && doubled.rightRms > doubled.leftRms * 0.70,
            "Double produced an excessive left/right level imbalance");
    expect (doubled.leftHash == repeatedDouble.leftHash
                && doubled.rightHash == repeatedDouble.rightHash,
            "Double output is not sample-deterministic");

    // Double is a new-note boundary, not a clone of an already-ringing voice.
    // Re-entering it clears the dormant player's old tail; the primary keeps
    // ringing on the left and the next physical attack starts both players.
    ElectryAudioProcessor toggled;
    toggled.prepareToPlay (sampleRate, blockSize);
    setParameterValue (toggled, electry::parameters::outputMode, 2.0f);
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    renderSeconds (toggled, audio, 0.05);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::lowestPlayableNote, (juce::uint8) 102), 0);
    renderBlock (toggled, audio, midi);
    renderSeconds (toggled, audio, 0.08);
    setParameterValue (toggled, electry::parameters::outputMode, 0.0f);
    renderBlock (toggled, audio, midi);
    setParameterValue (toggled, electry::parameters::outputMode, 2.0f);
    renderBlock (toggled, audio, midi);
    const auto reentryLeft = audio.getMagnitude (0, 0, blockSize);
    const auto reentryRight = audio.getMagnitude (1, 0, blockSize);
    expect (reentryLeft > 1.0e-5f && reentryRight < 1.0e-7f,
            "re-entering Double did not preserve only the primary tail (L "
                + std::to_string (reentryLeft) + ", R "
                + std::to_string (reentryRight) + ")");
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 102), 0);
    renderBlock (toggled, audio, midi);
    expect (audio.getMagnitude (0, 0, blockSize) > 1.0e-5f
                && audio.getMagnitude (1, 0, blockSize) > 1.0e-5f,
            "the first new note after entering Double did not start both engines");
    toggled.releaseResources();
}

juce::Image renderEditorSnapshot (juce::AudioProcessorEditor& editor)
{
    juce::Image snapshot (juce::Image::ARGB, editor.getWidth(),
                          editor.getHeight(), true);
    juce::Graphics graphics (snapshot);
    editor.paintEntireComponent (graphics, true);
    return snapshot;
}

void testEditorRendering()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "processor did not create an editor");
    if (editor == nullptr)
        return;

    expect (editor->getWidth() >= 900 && editor->getHeight() >= 500,
            "editor opened at an unexpectedly small size");

    std::vector<ElectryKnob*> knobs;
    for (auto* child : editor->getChildren())
    {
        if (auto* knob = dynamic_cast<ElectryKnob*> (child))
            knobs.push_back (knob);
        if (child->isVisible())
            expect (editor->getLocalBounds().contains (child->getBounds()),
                    "visible editor control escaped the editor bounds: "
                        + child->getName().toStdString());
    }
    // Nineteen instrument/master controls plus five FX controls.
    expect (knobs.size() == 24u, "editor did not expose all 24 knob controls");

    for (std::size_t first = 0; first < knobs.size(); ++first)
    {
        // The editor deliberately tiers its controls by audible impact, so a
        // texture detail is much smaller than a hero control. This is the
        // practical floor for the smallest tier; the relative-size assertions
        // below are what pin the intended hierarchy.
        expect (knobs[first]->getWidth() >= 44 && knobs[first]->getHeight() >= 110,
                "knob fell below the compact control size: "
                    + knobs[first]->getName().toStdString());
        for (auto* child : knobs[first]->getChildren())
            expect (knobs[first]->getLocalBounds().contains (child->getBounds()),
                    "knob child escaped its control bounds: "
                        + knobs[first]->getName().toStdString());

        for (std::size_t second = first + 1u; second < knobs.size(); ++second)
            expect (! knobs[first]->getBounds().intersects (knobs[second]->getBounds()),
                    "knob regions overlap: " + knobs[first]->getName().toStdString()
                        + " and " + knobs[second]->getName().toStdString());
    }

    const auto findControl = [&] (const char* componentId) -> juce::Component*
    {
        for (auto* child : editor->getChildren())
            if (child->getComponentID() == componentId)
                return child;
        return nullptr;
    };
    const auto effectiveDialSize = [&] (const char* componentId)
    {
        const auto* control = findControl (componentId);
        expect (control != nullptr,
                std::string ("missing editor component ID ") + componentId);
        return control == nullptr
            ? 0
            : juce::jmin (control->getWidth(), control->getHeight() - 30);
    };

    auto* buildControl = dynamic_cast<ElectryKnob*> (
        findControl (electry::parameters::guitarBuild));
    expect (buildControl != nullptr
                && buildControl->slider.getTooltip().contains ("material damping")
                && buildControl->slider.getTooltip().contains ("scale length")
                && buildControl->slider.getTooltip().contains ("remain independent"),
            "Guitar Build does not explain its coupled path and independent controls");
    for (const char* removed : { "bodyWood", "bodySize", "bodyShape",
                                 "construction", "scaleLength", "stringGauge" })
        expect (findControl (removed) == nullptr,
                std::string ("removed build control is still visible: ") + removed);

    auto* factoryProgramControl = dynamic_cast<juce::ComboBox*> (
        findControl ("factoryProgram"));
    expect (factoryProgramControl != nullptr,
            "editor is missing the factory-rig selector");
    if (factoryProgramControl != nullptr)
    {
        expect (factoryProgramControl->getNumItems() == processor.getNumPrograms()
                    && factoryProgramControl->getSelectedId() == 1,
                "factory-rig selector did not expose the processor programs");
        expect (factoryProgramControl->getWidth() >= 180
                    && factoryProgramControl->getHeight() >= 24,
                "factory-rig selector is too small to operate clearly");
        expect (factoryProgramControl->getTitle() == "Factory rig",
                "factory-rig selector has no accessible title");
        const auto rigTooltip = factoryProgramControl->getTooltip();
        expect (rigTooltip.contains ("Mute Tightness")
                    && rigTooltip.contains ("Mute Pressure")
                    && rigTooltip.contains ("PICK STROKE")
                    && rigTooltip.contains ("PLAY STYLE")
                    && rigTooltip.contains ("Mute")
                    && rigTooltip.contains ("Dead"),
                "factory-rig tooltip does not explain mute controls and latch independence");

        factoryProgramControl->setSelectedId (2, juce::sendNotificationSync);
        expect (processor.getCurrentProgram() == 1,
                "editor factory-rig selector did not change the processor program");
        factoryProgramControl->setSelectedId (1, juce::sendNotificationSync);
        expect (processor.getCurrentProgram() == 0,
                "editor factory-rig selector did not return to Factory Default");

        processor.setCurrentProgram (2);
        // The editor polls at 30 Hz. Drive due timers until the selector has
        // observed the host change, with a short bound for loaded CI runners;
        // one fixed sleep still flaked when the timer epoch landed just after
        // the single synchronous dispatch.
        for (int attempt = 0;
             attempt < 25 && factoryProgramControl->getSelectedId() != 3;
             ++attempt)
        {
            juce::Thread::sleep (10);
            juce::Timer::callPendingTimersSynchronously();
        }
        expect (factoryProgramControl->getSelectedId() == 3,
                "host-side program change did not reach the editor selector");
        factoryProgramControl->setSelectedId (1, juce::sendNotificationSync);
    }

    auto* outputModeControl = findControl (electry::parameters::outputMode);
    const auto* outputControl = findControl (electry::parameters::output);
    expect (outputModeControl != nullptr && outputControl != nullptr,
            "Master panel is missing its output-mode buttons or Output control");
    if (outputModeControl != nullptr && outputControl != nullptr)
    {
        expect (! outputModeControl->getBounds().intersects (
                    outputControl->getBounds()),
                "output-mode buttons overlap the Master Output knob");
        int modeButtons = 0;
        std::vector<juce::Rectangle<int>> modeButtonBounds;
        juce::TextButton* monoButton = nullptr;
        juce::TextButton* stereoButton = nullptr;
        juce::TextButton* doubleButton = nullptr;
        for (auto* child : outputModeControl->getChildren())
        {
            auto* button = dynamic_cast<juce::TextButton*> (child);
            if (button == nullptr)
                continue;
            ++modeButtons;
            modeButtonBounds.push_back (child->getBounds());
            expect (child->getWidth() >= 44 && child->getHeight() >= 22,
                    "output-mode button is too small to operate clearly");
            expect (child->getY() == 0
                        && child->getBottom() == outputModeControl->getHeight(),
                    "output-mode buttons still reserve space for a redundant label");
            if (button->getButtonText() == "MONO")
                monoButton = button;
            else if (button->getButtonText() == "STEREO")
                stereoButton = button;
            else if (button->getButtonText() == "DOUBLE")
                doubleButton = button;
        }
        expect (modeButtons == 3,
                "output controls did not expose Mono, Stereo and Double");
        for (std::size_t first = 0; first < modeButtonBounds.size(); ++first)
            for (std::size_t second = first + 1;
                 second < modeButtonBounds.size(); ++second)
                expect (! modeButtonBounds[first].intersects (
                            modeButtonBounds[second]),
                        "output-mode buttons overlap");
        expect (monoButton != nullptr && stereoButton != nullptr
                    && doubleButton != nullptr,
                "output-mode buttons lost a full readable label");
        if (monoButton != nullptr && stereoButton != nullptr
            && doubleButton != nullptr)
        {
            doubleButton->onClick();
            expect (parameterValue (processor, electry::parameters::outputMode)
                        > 1.5f,
                    "DOUBLE editor button did not enable the second engine");
            stereoButton->onClick();
            expect (std::abs (parameterValue (
                               processor, electry::parameters::outputMode) - 1.0f)
                        < 1.0e-5f,
                    "STEREO editor button did not select the stereo field");
            monoButton->onClick();
            expect (parameterValue (
                        processor, electry::parameters::outputMode) < 0.5f,
                    "MONO editor button did not restore the summed DI");
        }
    }

    const std::array<const char*, 7> heroControls {
        electry::parameters::pickupType, electry::parameters::tone,
        electry::parameters::pickPosition, electry::parameters::pickHardness,
        electry::parameters::stringAge, electry::parameters::bodyResonance,
        electry::parameters::velocity
    };
    const std::array<const char*, 5> detailControls {
        electry::parameters::bendTime, electry::parameters::pickNoise,
        electry::parameters::fingerNoise, electry::parameters::releaseNoise,
        electry::parameters::artifacts
    };
    for (const auto* hero : heroControls)
        for (const auto* detail : detailControls)
            expect (effectiveDialSize (hero) * 100
                        >= effectiveDialSize (detail) * 135,
                    std::string (hero) + " is not visibly larger than " + detail);

    expect (effectiveDialSize (electry::parameters::guitarBuild) * 100
                >= effectiveDialSize (electry::parameters::artifacts) * 120,
            "Guitar Build is not visually above texture details");
    expect (effectiveDialSize (electry::parameters::muteDamping) * 100
                >= effectiveDialSize (electry::parameters::artifacts) * 110,
            "contextual Mute control is not visually above texture details");
    expect (effectiveDialSize (electry::parameters::output) * 100
                >= effectiveDialSize (electry::parameters::releaseNoise) * 110,
            "Master Output is not visually above release-noise detail");

    // Performance controls sit beside the fretboard they change, and must not
    // be smaller than the texture details.
    for (const auto* performance : { electry::parameters::sympathetic,
                                     electry::parameters::palmMute,
                                     electry::parameters::strumSpread,
                                     electry::parameters::resonanceDepth })
        expect (effectiveDialSize (performance)
                    >= effectiveDialSize (electry::parameters::artifacts),
                std::string (performance)
                    + " is smaller than an artifact-texture control");

    const auto* fretboard = findControl ("fretboard");
    expect (fretboard != nullptr, "editor is missing the live fretboard display");
    if (fretboard != nullptr)
    {
        expect (fretboard->getWidth() >= 400 && fretboard->getHeight() >= 60,
                "the fretboard display is too small to read");
        for (const auto* knob : knobs)
            expect (! fretboard->getBounds().intersects (knob->getBounds()),
                    "the fretboard display overlaps a control: "
                        + knob->getName().toStdString());
    }

    const auto* pickStrip = findControl ("pickStyleStrip");
    const auto* styleStrip = findControl ("playStyleStrip");
    const auto* styleKeyMode = findControl ("playStyleKeyMode");
    const auto* keyboard = findControl ("keyboard");
    const auto* keyboardHint = findControl ("keyboardHint");
    expect (pickStrip != nullptr && styleStrip != nullptr
                && styleKeyMode != nullptr && keyboard != nullptr
                && keyboardHint != nullptr,
            "editor hierarchy components are missing stable IDs");
    if (pickStrip != nullptr && styleStrip != nullptr
        && styleKeyMode != nullptr && keyboard != nullptr
        && keyboardHint != nullptr)
    {
        if (factoryProgramControl != nullptr)
            expect (factoryProgramControl->getBottom() <= pickStrip->getY(),
                    "factory-rig selector overlaps the performance section");
        expect (! pickStrip->getBounds().intersects (styleKeyMode->getBounds())
                    && ! styleKeyMode->getBounds().intersects (
                           styleStrip->getBounds()),
                "the articulation strips overlap");
        for (const auto* knob : knobs)
        {
            expect (pickStrip->getBottom() <= knob->getY()
                        && styleStrip->getBottom() <= knob->getY()
                        && styleKeyMode->getBottom() <= knob->getY(),
                    "sound control overlaps a keyswitch strip");
            expect (knob->getBottom() <= keyboard->getY(),
                    "sound control overlaps the keyboard");
        }
        expect (keyboard->getHeight() >= 90
                    && keyboard->getWidth() * 10 >= editor->getWidth() * 9,
                "keyboard lost its practical playing area");
        expect (keyboard->getBottom() <= keyboardHint->getY(),
                "keyboard hint is not below the keyboard");

        std::vector<juce::Rectangle<int>> buttonBounds;
        const auto countButtons = [&buttonBounds] (const juce::Component* strip)
        {
            int count = 0;
            for (auto* child : strip->getChildren())
            {
                if (dynamic_cast<juce::TextButton*> (child) == nullptr)
                    continue;
                ++count;
                buttonBounds.push_back (
                    child->getBounds() + strip->getPosition());
                expect (child->getWidth() >= 60 && child->getHeight() >= 24,
                        "keyswitch button fell below its practical target size");
            }
            return count;
        };
        expect (countButtons (pickStrip) == 3,
                "editor did not retain the three picking-style buttons");
        expect (countButtons (styleStrip)
                    == electry::ElectryEngine::playStyleKeyswitchCount,
                "editor did not retain one button per play style");
        bool hasMuteButton = false;
        for (auto* child : styleStrip->getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*> (child))
                hasMuteButton = hasMuteButton
                             || button->getButtonText() == "MUTE";
        expect (hasMuteButton,
                "the bridge-hand articulation is not labelled MUTE");
        expect (countButtons (styleKeyMode) == 2,
                "editor did not expose both LATCH and HOLD modes");
        juce::TextButton* latchButton = nullptr;
        juce::TextButton* holdButton = nullptr;
        for (auto* child : styleKeyMode->getChildren())
        {
            if (auto* button = dynamic_cast<juce::TextButton*> (child))
            {
                if (button->getButtonText() == "LATCH")
                    latchButton = button;
                else if (button->getButtonText() == "HOLD")
                    holdButton = button;
            }
        }
        expect (latchButton != nullptr && holdButton != nullptr,
                "play-style key mode buttons lost their readable labels");
        if (latchButton != nullptr && holdButton != nullptr)
        {
            holdButton->onClick();
            expect (processor.getPlayStyleKeysHold(),
                    "HOLD editor button did not change processor state");
            latchButton->onClick();
            expect (! processor.getPlayStyleKeysHold(),
                    "LATCH editor button did not restore processor state");
        }
        for (std::size_t first = 0; first < buttonBounds.size(); ++first)
            for (std::size_t second = first + 1u; second < buttonBounds.size(); ++second)
                expect (! buttonBounds[first].intersects (buttonBounds[second]),
                        "keyswitch buttons overlap");
    }

    const auto snapshot = renderEditorSnapshot (*editor);
    int litPixels = 0;
    for (int y = 0; y < snapshot.getHeight(); y += 8)
        for (int x = 0; x < snapshot.getWidth(); x += 8)
            if (snapshot.getPixelAt (x, y).getPerceivedBrightness() > 0.05f)
                ++litPixels;
    expect (litPixels > 100, "editor snapshot appears blank");

    // The committed interface screenshot is this exact editor render. Setting
    // ELECTRY_EDITOR_SNAPSHOT to a path writes the PNG used in the READMEs, so
    // the documentation image is always the real regression-tested editor.
    const auto snapshotPath = juce::SystemStats::getEnvironmentVariable (
        "ELECTRY_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
    {
        const juce::File snapshotFile { snapshotPath };
        snapshotFile.getParentDirectory().createDirectory();
        juce::FileOutputStream output { snapshotFile };
        juce::PNGImageFormat png;
        const bool preparedOutput = output.openedOk()
            && output.setPosition (0)
            && output.truncate();
        const bool wroteSnapshot = preparedOutput
            && png.writeImageToStream (snapshot, output);
        output.flush();
        expect (wroteSnapshot, "could not write requested editor snapshot");
    }

    editor.reset();
    processor.releaseResources();
}

void testPrepareReleaseCycles()
{
    ElectryAudioProcessor processor;
    for (const double rate : { 44100.0, 96000.0, 192000.0 })
    {
        processor.prepareToPlay (rate, blockSize);
        expect (processor.isEngineReady(),
                "engine not ready at rate " + std::to_string (rate));
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 90), 0);
        renderBlock (processor, audio, midi);
        expect (audio.getMagnitude (0, blockSize) > 0.0f,
                "no audio after prepare at rate " + std::to_string (rate));
        processor.releaseResources();
        expect (! processor.isEngineReady(),
                "engine still ready after releaseResources");
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    testParameterLayoutAndDefaults();
    testFactoryPrograms();
    testParameterTextFormatting();
    testParameterTextParsing();
    testStateRoundTrip();
    testBusAndPluginContract();
    testSampleAccurateNoteAndSound();
    testKeyswitchContract();
    testPlayStyleHoldContract();
    testSameSampleMuteKeyswitchOrder();
    testMidiControllersAndVoiceLifecycle();
    testSameSamplePalmMutePressureAttack();
    testPitchWheelByteReconstruction();
    testPitchWheelMidiDispatch();
    testResonanceWheelFeedback();
    testChannelPressureAndAftertouchVibratoDispatch();
    testResetAllControllersDispatch();
    testMutePressureDisplayFeedback();
    testUiArticulationTriggerAndPanic();
    testOutputGainImpact();
    testPerformanceControls();
    testOutputModeAudioField();
    testEditorRendering();
    testPrepareReleaseCycles();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Electry processor contract test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Electry processor contract tests passed\n";
    return EXIT_SUCCESS;
}
