#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <set>
#include <span>
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
constexpr std::array<ParameterExpectation, 28> expectedParameters {{
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
    { electry::parameters::tremoloRate,    12.0f, 1.0e-4f },
    { electry::parameters::ampModel,        2.0f, 1.0e-5f },
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
                             double localSampleRate, double searchCentreHz,
                             double searchSpanCents = 120.0)
{
    const int first = std::max (0, start);
    const int last = std::min<int> (first + length,
                                    static_cast<int> (data.size()));
    float peak = 0.0f;
    for (int index = first; index < last; ++index)
        peak = std::max (peak,
                         std::abs (data[static_cast<std::size_t> (index)]));
    expect (peak > 1.0e-7f,
            "pitch estimator received a silent or negligible capture");

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

    const double coarse = scan (searchCentreHz, searchSpanCents, 6.0);
    return scan (coarse, 6.0, 0.5);
}

void testParameterLayoutAndDefaults()
{
    ElectryAudioProcessor processor;
    expect (processor.getParameters().size()
                == static_cast<int> (expectedParameters.size()),
            "processor does not expose exactly 28 APVTS parameters");

    std::set<std::string> uniqueIds;
    for (std::size_t index = 0; index < expectedParameters.size(); ++index)
    {
        const auto& expected = expectedParameters[index];
        expect (uniqueIds.insert (expected.id).second,
                std::string ("duplicate APVTS id ") + expected.id);
        const auto* indexed = dynamic_cast<juce::AudioProcessorParameterWithID*> (
            processor.getParameters()[static_cast<int> (index)]);
        expect (indexed != nullptr && indexed->paramID == expected.id,
                std::string ("host parameter index drifted at ")
                    + std::to_string (index));
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

    const auto* tremoloRate = dynamic_cast<const juce::AudioParameterFloat*> (
        processor.parameters.getParameter (electry::parameters::tremoloRate));
    expect (tremoloRate != nullptr
                && std::abs (tremoloRate->range.start - 4.0f) < 1.0e-5f
                && std::abs (tremoloRate->range.end - 20.0f) < 1.0e-5f
                && std::abs (tremoloRate->range.interval - 0.1f) < 1.0e-5f,
            "Tremolo Rate did not expose its exact 4..20 strokes/s host range");
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
    expect (processor.getNumPrograms() == 4,
            "processor does not expose exactly four factory rigs");
    expect (processor.getCurrentProgram() == 0,
            "new processor did not select Factory Default");

    const std::array<const char*, 4> expectedNames {
        "Factory Default", "Drop-E Metal", "Mute / Dead DI", "Blues Rock Lead"
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
        else if (program == 3)
        {
            overrideValue (electry::parameters::pickupSelector, 0.00f);
            overrideValue (electry::parameters::pickupType, 0.56f);
            overrideValue (electry::parameters::tone, 0.78f);
            overrideValue (electry::parameters::bodyResonance, 0.62f);
            overrideValue (electry::parameters::stringAge, 0.20f);
            overrideValue (electry::parameters::pickPosition, 0.38f);
            overrideValue (electry::parameters::pickHardness, 0.52f);
            overrideValue (electry::parameters::fingerNoise, 0.48f);
            overrideValue (electry::parameters::bendTime, 0.15f);
            overrideValue (electry::parameters::velocity, 0.90f);
            overrideValue (electry::parameters::output, 3.20f);
            overrideValue (electry::parameters::outputMode, 1.00f);
            overrideValue (electry::parameters::distortion, 0.14f);
            overrideValue (electry::parameters::amp, 0.62f);
            overrideValue (electry::parameters::compressor, 0.28f);
            overrideValue (electry::parameters::delay, 0.18f);
            overrideValue (electry::parameters::room, 0.28f);
            overrideValue (electry::parameters::sympathetic, 0.35f);
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
#if ELECTRY_MEASURED_BODY_RESPONSE
    expectParameterText (processor, electry::parameters::guitarBuild, 0.0f,
                         "Walnut short/heavy");
    expectParameterText (processor, electry::parameters::guitarBuild, 0.4f,
                         "Medium balanced");
    expectParameterText (processor, electry::parameters::guitarBuild, 0.5f, "50%");
    expectParameterText (processor, electry::parameters::guitarBuild, 0.8f,
                         "Extended heavy");
    expectParameterText (processor, electry::parameters::guitarBuild, 1.0f,
                         "Ash extended/light");
#else
    expectParameterText (processor, electry::parameters::guitarBuild, 0.0f,
                         "Slab fixed");
    expectParameterText (processor, electry::parameters::guitarBuild, 0.4f,
                         "Angular set");
    expectParameterText (processor, electry::parameters::guitarBuild, 0.5f, "50%");
    expectParameterText (processor, electry::parameters::guitarBuild, 0.8f,
                         "Dense extended");
    expectParameterText (processor, electry::parameters::guitarBuild, 1.0f,
                         "Neck-through");
#endif
    expectParameterText (processor, electry::parameters::output, -6.0f, "-6.0dB");
    expectParameterText (processor, electry::parameters::output, 3.0f, "+3.0dB");
    expectParameterText (processor, electry::parameters::tone, 0.8f, "80%");
    expectParameterText (processor, electry::parameters::artifacts, 0.18f, "18%");
    expectParameterText (processor, electry::parameters::outputMode, 0.0f, "Mono");
    expectParameterText (processor, electry::parameters::outputMode, 1.0f, "Stereo");
    expectParameterText (processor, electry::parameters::outputMode, 2.0f, "Double");
    expectParameterText (processor, electry::parameters::ampModel, 0.0f,
                         "American Clean");
    expectParameterText (processor, electry::parameters::ampModel, 1.0f,
                         "British Crunch");
    expectParameterText (processor, electry::parameters::ampModel, 2.0f,
                         "Modern High-Gain");
    expectParameterText (processor, electry::parameters::bendTime, 0.28f, "280 ms");
    expectParameterText (processor, electry::parameters::pickupType, 0.0f, "Humbucker");
    expectParameterText (processor, electry::parameters::pickupType, 1.0f, "Single coil");
    expectParameterText (processor, electry::parameters::sympathetic, 0.2f, "20%");
    expectParameterText (processor, electry::parameters::palmMute, 0.0f, "0%");
    expectParameterText (processor, electry::parameters::strumSpread, 0.0f,
                         "Block chord");
    expectParameterText (processor, electry::parameters::strumSpread, 18.0f,
                         "18.0 ms/string");
    expectParameterText (processor, electry::parameters::tremoloRate, 12.0f,
                         "12.0 strokes/s");
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
    expectParameterValueForText (processor, electry::parameters::tremoloRate,
                                 "12.0 strokes/s", 12.0f, 1.0e-3f,
                                 "tremoloRate \"12.0 strokes/s\"");

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

#if ELECTRY_MEASURED_BODY_RESPONSE
    expectParameterValueForText (processor, electry::parameters::guitarBuild,
                                 "Medium balanced", 0.4f, 1.0e-3f,
                                 "measured guitarBuild named anchor");
#else
    expectParameterValueForText (processor, electry::parameters::guitarBuild,
                                 "Angular set", 0.4f, 1.0e-3f,
                                 "guitarBuild named anchor");
#endif
    expectParameterValueForText (processor, electry::parameters::guitarBuild,
                                 "50%", 0.5f, 1.0e-3f,
                                 "guitarBuild percentage");
}

void testStateRoundTrip()
{
    ElectryAudioProcessor source;
    source.setCurrentProgram (3);
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
    setParameterValue (source, electry::parameters::tremoloRate, 16.0f);
    setParameterValue (source, electry::parameters::resonanceDepth, 80.0f);
    setParameterValue (source, electry::parameters::ampModel, 1.0f);
    source.triggerArticulation (static_cast<int> (electry::PickStyle::Up));
    source.triggerArticulation (
        electry::ElectryEngine::pickStyleKeyswitchCount
            + static_cast<int> (electry::PlayStyle::Dead));

    juce::MemoryBlock state;
    source.getStateInformation (state);
    expect (state.getSize() > 0, "state serialisation produced no data");

    ElectryAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    expect (restored.getCurrentProgram() == 3,
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
    expect (std::abs (parameterValue (restored, electry::parameters::tremoloRate)
                          - 16.0f) < 1.0e-3f,
            "tremolo rate did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::resonanceDepth)
                          - 80.0f) < 1.0e-3f,
            "resonance depth did not survive a state round trip");
    expect (std::abs (parameterValue (restored, electry::parameters::ampModel)
                          - 1.0f) < 1.0e-4f,
            "amp model did not survive a state round trip");
    expect (restored.getCurrentPickStyleIndex()
                == static_cast<int> (electry::PickStyle::Up)
                && restored.getCurrentPlayStyleIndex()
                       == static_cast<int> (electry::PlayStyle::Dead),
            "the two articulation latches did not survive a state round trip");
    expect (restored.getPlayStyleKeysHold(),
            "the play-style key mode did not survive a state round trip");

    // Sessions written before ampModel existed have no matching PARAM child.
    // They used today's Modern circuit, so loading one must select Modern even
    // when the receiving instance happened to have another model selected.
    const auto savedXml = juce::AudioProcessor::getXmlFromBinary (
        state.getData(), static_cast<int> (state.getSize()));
    expect (savedXml != nullptr, "could not decode state for legacy migration check");
    if (savedXml != nullptr)
    {
        auto legacyState = juce::ValueTree::fromXml (*savedXml);
        const auto ampModelState = legacyState.getChildWithProperty (
            "id", electry::parameters::ampModel);
        expect (ampModelState.isValid(),
                "saved state omitted the new amp-model parameter");
        legacyState.removeChild (ampModelState, nullptr);

        juce::MemoryBlock legacyData;
        if (const auto legacyXml = legacyState.createXml())
            juce::AudioProcessor::copyXmlToBinary (*legacyXml, legacyData);

        ElectryAudioProcessor legacyRestored;
        setParameterValue (legacyRestored, electry::parameters::ampModel, 0.0f);
        legacyRestored.setStateInformation (
            legacyData.getData(), static_cast<int> (legacyData.getSize()));
        expect (std::abs (parameterValue (
                            legacyRestored, electry::parameters::ampModel) - 2.0f)
                    < 1.0e-4f,
                "legacy state did not migrate to the established Modern amp");
    }

}

void testBusAndPluginContract()
{
    ElectryAudioProcessor processor;
    expect (processor.acceptsMidi(), "instrument does not accept MIDI");
    expect (! processor.producesMidi(), "instrument unexpectedly produces MIDI");
    expect (! processor.isMidiEffect(), "instrument reports being a MIDI effect");
    expect (processor.supportsMPE(), "instrument does not advertise MPE support");
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

void testSameSampleChordAllocationIsCanonical()
{
    struct Render
    {
        std::array<electry::StringVisualState,
                   electry::ElectryEngine::stringCount> strings {};
        std::vector<float> left;
        std::vector<float> right;
        bool heldAfterFirstProbeOff = false;
        bool heldAfterSecondProbeOff = false;
    };

    const auto render = []<std::size_t N> (
        const std::array<int, N>& hostNotes, std::span<const int> uiNotes,
        float spreadMilliseconds, int onset, int ownershipProbe = -1)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);
        setParameterValue (processor, electry::parameters::strumSpread,
                           spreadMilliseconds);

        constexpr auto velocityByte = static_cast<juce::uint8> (110);
        constexpr float velocity = 110.0f / 127.0f;
        for (const int note : uiNotes)
            processor.keyboardState.noteOn (1, note, velocity);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        for (const int note : hostNotes)
            if (note >= 0)
                midi.addEvent (juce::MidiMessage::noteOn (
                                   1, note, velocityByte), onset);

        Render result;
        for (int block = 0; block < 4; ++block)
        {
            renderBlock (processor, audio, midi);
            const auto* left = audio.getReadPointer (0);
            const auto* right = audio.getReadPointer (1);
            result.left.insert (result.left.end(), left,
                                left + audio.getNumSamples());
            result.right.insert (result.right.end(), right,
                                 right + audio.getNumSamples());
        }
        for (int string = 0; string < electry::ElectryEngine::stringCount; ++string)
            result.strings[static_cast<std::size_t> (string)] =
                processor.getStringVisualState (string);

        if (ownershipProbe >= 0)
        {
            const auto probeIsHeld = [&]
            {
                for (int string = 0;
                     string < electry::ElectryEngine::stringCount; ++string)
                {
                    const auto state = processor.getStringVisualState (string);
                    if (state.midiNote == ownershipProbe && state.sounding
                        && ! state.releasing)
                        return true;
                }
                return false;
            };
            midi.addEvent (juce::MidiMessage::noteOff (1, ownershipProbe), 0);
            renderBlock (processor, audio, midi);
            result.heldAfterFirstProbeOff = probeIsHeld();
            midi.addEvent (juce::MidiMessage::noteOff (1, ownershipProbe), 0);
            renderBlock (processor, audio, midi);
            result.heldAfterSecondProbeOff = probeIsHeld();
        }
        processor.releaseResources();
        return result;
    };

    const auto sameMapping = [] (const Render& first, const Render& second)
    {
        for (int string = 0; string < electry::ElectryEngine::stringCount; ++string)
        {
            const auto index = static_cast<std::size_t> (string);
            if (first.strings[index].sounding != second.strings[index].sounding
                || first.strings[index].midiNote != second.strings[index].midiNote
                || first.strings[index].fret != second.strings[index].fret)
                return false;
        }
        return true;
    };

    const std::array<int, 3> canonical { 33, 40, 45 };
    for (const float spread : { 0.0f, 12.0f })
    {
        const auto reference = render (canonical, {}, spread, 137);
        expect (reference.strings[0].midiNote == 33
                    && reference.strings[0].fret == 5
                    && reference.strings[1].midiNote == 40
                    && reference.strings[1].fret == 5
                    && reference.strings[2].midiNote == 45
                    && reference.strings[2].fret == 5,
                "same-sample A power chord did not use its compact fifth-fret shape");
        expect (std::all_of (reference.left.begin(),
                            reference.left.begin() + 137,
                            [] (float sample) { return sample == 0.0f; }),
                "canonical chord sounded before its host sample position");

        auto permutation = canonical;
        do
        {
            const auto candidate = render (permutation, {}, spread, 137);
            expect (sameMapping (candidate, reference),
                    "same-sample host chord fingering depended on event order");
            expect (candidate.left == reference.left
                        && candidate.right == reference.right,
                    "same-sample host chord audio depended on event order");
        }
        while (std::next_permutation (permutation.begin(), permutation.end()));
    }

    const auto firstAudibleSample = [] (const std::vector<float>& audio)
    {
        const auto found = std::find_if (audio.begin(), audio.end(), [] (float sample)
        {
            return sample != 0.0f;
        });
        return found == audio.end()
            ? -1 : static_cast<int> (std::distance (audio.begin(), found));
    };
    const auto flatChord = render (canonical, {}, 0.0f, 137);
    const auto spreadChord = render (canonical, {}, 12.0f, 137);
    expect (firstAudibleSample (flatChord.left) >= 137
                && firstAudibleSample (spreadChord.left)
                       == firstAudibleSample (flatChord.left),
            "a complete host chord retained Strum Spread's re-anchor latency");

    // The processor knows that a one-note timestamp is complete too. With no
    // strings to cross, enabling Strum Spread must be a sample-exact no-op;
    // otherwise a monophonic riff pays the old fixed 20 ms lookahead.
    const std::array<int, 1> singleNote { 45 };
    const auto flatSingle = render (singleNote, {}, 0.0f, 137);
    const auto spreadSingle = render (singleNote, {}, 12.0f, 137);
    expect (flatSingle.left == spreadSingle.left
                && flatSingle.right == spreadSingle.right,
            "Strum Spread delayed or recoloured a complete host single note");

    // A#0 is continuous performance state, not a fourth chord note. It must
    // condition the complete simultaneous attack wherever the host inserted
    // it, without flushing the bounded chord solve into separate fingerings.
    const std::array<int, 4> vibratoFirst { 22, 33, 40, 45 };
    const std::array<int, 4> vibratoMiddle { 33, 40, 22, 45 };
    const std::array<int, 4> vibratoLast { 33, 40, 45, 22 };
    const auto conditionedReference = render (
        vibratoFirst, {}, 0.0f, 137);
    for (const auto& candidate : {
             render (vibratoMiddle, {}, 0.0f, 137),
             render (vibratoLast, {}, 0.0f, 137) })
    {
        expect (sameMapping (candidate, conditionedReference),
                "same-sample A#0 vibrato split the physical chord solve");
        expect (candidate.left == conditionedReference.left
                    && candidate.right == conditionedReference.right,
                "same-sample A#0 vibrato depended on host insertion order");
    }

    std::array duplicateChord { 33, 40, 40, 45 };
    const auto duplicateReference = render (
        duplicateChord, {}, 0.0f, 137, 40);
    expect (duplicateReference.strings[0].midiNote == 33
                && duplicateReference.strings[0].fret == 5
                && duplicateReference.strings[1].midiNote == 40
                && duplicateReference.strings[1].fret == 5
                && duplicateReference.strings[2].midiNote == 45
                && duplicateReference.strings[2].fret == 5,
            "duplicate chord did not retain its compact physical shape");
    do
    {
        const auto candidate = render (duplicateChord, {}, 0.0f, 137, 40);
        expect (sameMapping (candidate, duplicateReference),
                "mixed duplicate chord fingering depended on event order");
        expect (candidate.left == duplicateReference.left
                    && candidate.right == duplicateReference.right,
                "mixed duplicate chord audio depended on event order");
        expect (candidate.heldAfterFirstProbeOff
                    && ! candidate.heldAfterSecondProbeOff,
                "mixed duplicate chord lost its two MIDI-note owners");
    }
    while (std::next_permutation (duplicateChord.begin(),
                                  duplicateChord.end()));

    const std::array nineNotes { 28, 35, 40, 45, 50, 55, 59, 64, 69 };
    const auto nineReference = render (nineNotes, {}, 0.0f, 137);
    for (int string = 0; string < electry::ElectryEngine::stringCount; ++string)
    {
        const auto& state = nineReference.strings[static_cast<std::size_t> (string)];
        expect (state.midiNote == nineNotes[static_cast<std::size_t> (string)]
                    && state.fret == 0,
                "nine-note policy did not retain the lowest eight open strings");
    }
    for (std::size_t shift = 0; shift < nineNotes.size(); ++shift)
    {
        auto rotated = nineNotes;
        std::rotate (rotated.begin(), rotated.begin() + shift, rotated.end());
        const auto candidate = render (rotated, {}, 0.0f, 137);
        expect (sameMapping (candidate, nineReference),
                "nine-note chord fingering depended on event order");
        expect (candidate.left == nineReference.left
                    && candidate.right == nineReference.right,
                "nine-note chord audio depended on event order");
    }
    auto reversedNine = nineNotes;
    std::reverse (reversedNine.begin(), reversedNine.end());
    const auto reversedNineRender = render (reversedNine, {}, 0.0f, 137);
    expect (sameMapping (reversedNineRender, nineReference)
                && reversedNineRender.left == nineReference.left
                && reversedNineRender.right == nineReference.right,
            "reversed nine-note chord bypassed the engine's lowest-eight policy");

    // At sample zero the GUI queue and the host describe the same physical
    // attack. Splitting the chord between them must not change its fingering
    // or the deterministic player draws.
    const std::array<int, 3> noHost { -1, -1, -1 };
    const auto hostReference = render (canonical, {}, 0.0f, 0);
    const std::array<int, 2> uiLowHigh { 33, 45 };
    const std::array<int, 2> uiHighLow { 45, 33 };
    const std::array<int, 1> uiMiddle { 40 };
    const auto lowHighSplit = render (
        std::array { 40, -1, -1 }, uiLowHigh, 0.0f, 0);
    const auto highLowSplit = render (
        std::array { 40, -1, -1 }, uiHighLow, 0.0f, 0);
    const auto reverseHostSplit = render (
        std::array { 45, 33, -1 }, uiMiddle, 0.0f, 0);
    const auto uiOnly = render (noHost, canonical, 0.0f, 0);
    for (const auto* candidate : { &lowHighSplit, &highLowSplit,
                                  &reverseHostSplit, &uiOnly })
    {
        expect (sameMapping (*candidate, hostReference),
                "sample-zero GUI/host chord split changed its fingering");
        expect (candidate->left == hostReference.left
                    && candidate->right == hostReference.right,
                "sample-zero GUI/host chord split changed its audio");
    }

    // Lifecycle messages are ordering barriers, not chord members. A release
    // before an attack and a release after it must therefore remain different.
    const auto noteThenRelease = [] (bool releaseFirst)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        const auto noteOn = juce::MidiMessage::noteOn (
            1, 45, static_cast<juce::uint8> (110));
        const auto noteOff = juce::MidiMessage::noteOff (1, 45);
        if (releaseFirst)
            midi.addEvent (noteOff, 0);
        midi.addEvent (noteOn, 0);
        if (! releaseFirst)
            midi.addEvent (noteOff, 0);
        renderBlock (processor, audio, midi);
        electry::StringVisualState result;
        for (int string = 0; string < electry::ElectryEngine::stringCount; ++string)
        {
            const auto state = processor.getStringVisualState (string);
            if (state.midiNote == 45)
                result = state;
        }
        processor.releaseResources();
        return result;
    };
    const auto released = noteThenRelease (false);
    const auto held = noteThenRelease (true);
    expect ((! released.sounding || released.releasing)
                && held.sounding && ! held.releasing,
            "same-sample Note On/Off lifecycle order was canonicalised away");

    ElectryAudioProcessor overlaps;
    overlaps.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> overlapAudio;
    juce::MidiBuffer overlapMidi;
    overlapMidi.addEvent (juce::MidiMessage::noteOn (
                              1, 45, static_cast<juce::uint8> (110)), 0);
    overlapMidi.addEvent (juce::MidiMessage::noteOn (
                              1, 45, static_cast<juce::uint8> (110)), 0);
    renderBlock (overlaps, overlapAudio, overlapMidi);
    overlapMidi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
    renderBlock (overlaps, overlapAudio, overlapMidi);
    bool heldAfterOneRelease = false;
    for (int string = 0; string < electry::ElectryEngine::stringCount; ++string)
    {
        const auto state = overlaps.getStringVisualState (string);
        heldAfterOneRelease = heldAfterOneRelease
                           || (state.midiNote == 45 && state.sounding
                               && ! state.releasing);
    }
    overlapMidi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
    renderBlock (overlaps, overlapAudio, overlapMidi);
    bool secondOwnerStillHeld = false;
    for (int string = 0; string < electry::ElectryEngine::stringCount; ++string)
    {
        const auto state = overlaps.getStringVisualState (string);
        secondOwnerStillHeld = secondOwnerStillHeld
                            || (state.midiNote == 45 && state.sounding
                                && ! state.releasing);
    }
    expect (heldAfterOneRelease && ! secondOwnerStillHeld,
            "duplicate same-sample Note Ons lost overlap ownership");
    overlaps.releaseResources();
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
    setParameterValue (processor, electry::parameters::bendTime, 0.04f);
    setParameterValue (processor, electry::parameters::bodyResonance, 0.0f);
    setParameterValue (processor, electry::parameters::artifacts, 0.0f);
    setParameterValue (processor, electry::parameters::sympathetic, 0.0f);
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

void testRepickMidiContract()
{
    constexpr int heldNote = electry::ElectryEngine::lowestPlayableNote;
    constexpr int repickNote = electry::ElectryEngine::firstRepickNote;
    const int muteKeyswitch = electry::ElectryEngine::firstPlayStyleKeyswitchNote
                            + static_cast<int> (electry::PlayStyle::PalmMute);

    const auto differenceEnergy = [] (const std::vector<float>& first,
                                      const std::vector<float>& second)
    {
        double result = 0.0;
        for (std::size_t sample = 0; sample < first.size(); ++sample)
        {
            const double difference = static_cast<double> (first[sample])
                                    - second[sample];
            result += difference * difference;
        }
        return result;
    };

    struct ConditionedRepick
    {
        std::vector<float> audio;
        int playStyle = -1;
        int pressure = -1;
        bool ownedAfterNoteOff = false;
        bool ownedAfterVelocityZero = false;
        bool releasedByOriginalNoteOff = false;
    };
    const auto renderConditionedRepick = [] (bool repickFirst, int cc2Value)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (
            1, heldNote, (juce::uint8) 110), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 0.04);

        const auto repick = juce::MidiMessage::noteOn (
            1, repickNote, (juce::uint8) 120);
        if (repickFirst)
            midi.addEvent (repick, 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            1, muteKeyswitch, (juce::uint8) 127), 0);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, cc2Value), 0);
        if (! repickFirst)
            midi.addEvent (repick, 0);
        renderBlock (processor, audio, midi);

        ConditionedRepick result;
        const auto* channel = audio.getReadPointer (0);
        result.audio.assign (channel, channel + audio.getNumSamples());
        result.playStyle = capturedPlayStyle (processor, heldNote);
        result.pressure = processor.getMidiMutePressureForDisplay();
        const auto stillOwned = [&processor]
        {
            const auto state = processor.getStringVisualState (0);
            return state.sounding && state.midiNote == heldNote && ! state.releasing;
        };

        midi.addEvent (juce::MidiMessage::noteOff (1, repickNote), 0);
        renderBlock (processor, audio, midi);
        result.ownedAfterNoteOff = stillOwned();
        midi.addEvent (juce::MidiMessage::noteOn (
            1, repickNote, (juce::uint8) 0), 0);
        renderBlock (processor, audio, midi);
        result.ownedAfterVelocityZero = stillOwned();
        midi.addEvent (juce::MidiMessage::noteOff (1, heldNote), 0);
        renderBlock (processor, audio, midi);
        const auto released = processor.getStringVisualState (0);
        result.releasedByOriginalNoteOff = ! released.sounding || released.releasing;

        processor.releaseResources();
        return result;
    };

    const auto repickFirst = renderConditionedRepick (true, 127);
    const auto conditionsFirst = renderConditionedRepick (false, 127);
    const auto noCc2 = renderConditionedRepick (true, 0);
    expect (repickFirst.audio == conditionsFirst.audio,
            "repick before same-sample Mute/CC2 missed attack conditioning");
    expect (repickFirst.playStyle
                == static_cast<int> (electry::PlayStyle::PalmMute)
                && repickFirst.pressure == 127,
            "same-sample repick did not capture Mute and full CC2 pressure");
    expect (differenceEnergy (repickFirst.audio, noCc2.audio) > 1.0e-8,
            "same-sample CC2 did not audibly condition the Mute repick");
    expect (repickFirst.ownedAfterNoteOff
                && repickFirst.ownedAfterVelocityZero,
            "repick Note Off or velocity-zero Note On removed the held string owner");
    expect (repickFirst.releasedByOriginalNoteOff,
            "repick added an owner that survived the original playable Note Off");

    struct AlternateRepick
    {
        std::vector<float> audio;
        bool beganDown = false;
        bool repickedUp = false;
        bool soundingBeforeRepick = false;
        bool unchangedByVelocityZero = false;
    };
    enum class RepickInput
    {
        host,
        keyboard,
        fretboard
    };
    const auto renderAlternateRepick = [] (RepickInput input, bool sustainOnly,
                                            bool shouldRepick)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        if (sustainOnly)
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            1, electry::ElectryEngine::firstKeyswitchNote
                   + static_cast<int> (electry::PickStyle::Alternate),
            (juce::uint8) 127), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            1, heldNote, (juce::uint8) 110), 0);
        renderBlock (processor, audio, midi);
        const bool beganDown = ! processor.getStringVisualState (0).strokeUp;
        if (sustainOnly)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, heldNote), 0);
            renderBlock (processor, audio, midi);
        }
        const bool soundingBeforeRepick =
            processor.getStringVisualState (0).sounding;
        renderSeconds (processor, audio, 0.04);

        if (shouldRepick && input == RepickInput::keyboard)
            processor.keyboardState.noteOn (
                1, repickNote, 110.0f / 127.0f);
        else if (shouldRepick && input == RepickInput::fretboard)
            processor.triggerStringRepick (0, 110.0f / 127.0f);
        else if (shouldRepick)
            midi.addEvent (juce::MidiMessage::noteOn (
                1, repickNote, (juce::uint8) 110), 0);
        renderBlock (processor, audio, midi);
        const auto* channel = audio.getReadPointer (0);
        AlternateRepick result;
        result.audio.assign (channel, channel + audio.getNumSamples());
        result.beganDown = beganDown;
        result.repickedUp = processor.getStringVisualState (0).strokeUp;
        result.soundingBeforeRepick = soundingBeforeRepick;
        if (shouldRepick && ! sustainOnly)
        {
            midi.addEvent (juce::MidiMessage::noteOn (
                1, repickNote, (juce::uint8) 0), 0);
            renderBlock (processor, audio, midi);
            result.unchangedByVelocityZero =
                processor.getStringVisualState (0).strokeUp == result.repickedUp;
        }
        processor.releaseResources();
        return result;
    };

    const auto hostAlternate = renderAlternateRepick (
        RepickInput::host, false, true);
    const auto uiAlternate = renderAlternateRepick (
        RepickInput::keyboard, false, true);
    const auto fretboardAlternate = renderAlternateRepick (
        RepickInput::fretboard, false, true);
    expect (hostAlternate.beganDown && hostAlternate.repickedUp
                && uiAlternate.beganDown && uiAlternate.repickedUp
                && fretboardAlternate.beganDown
                && fretboardAlternate.repickedUp,
            "host, keyboard or fretboard repick did not advance Alternate "
            "from Down to Up");
    expect (uiAlternate.audio == hostAlternate.audio
                && fretboardAlternate.audio == hostAlternate.audio,
            "keyboard or fretboard repick did not match host MIDI");
    expect (hostAlternate.unchangedByVelocityZero,
            "velocity-zero repick created an attack and advanced Alternate");

    const auto sustainTail = renderAlternateRepick (
        RepickInput::host, true, false);
    const auto sustainedRepick = renderAlternateRepick (
        RepickInput::host, true, true);
    expect (sustainedRepick.soundingBeforeRepick
                && sustainedRepick.audio == sustainTail.audio
                && ! sustainedRepick.repickedUp,
            "repick accepted a sustain-pedal voice without a physically held key");

    struct StereoRepick
    {
        std::array<std::vector<float>, 2> channels;
    };
    const auto renderDoubleRepick = [] (bool shouldRepick)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);
        setParameterValue (processor, electry::parameters::outputMode, 2.0f);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        renderSeconds (processor, audio, 0.05);
        midi.addEvent (juce::MidiMessage::noteOn (
            1, heldNote, (juce::uint8) 110), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 0.05);
        if (shouldRepick)
            midi.addEvent (juce::MidiMessage::noteOn (
                1, repickNote, (juce::uint8) 120), 0);
        renderBlock (processor, audio, midi);

        StereoRepick result;
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto* samples = audio.getReadPointer (channel);
            result.channels[static_cast<std::size_t> (channel)].assign (
                samples, samples + audio.getNumSamples());
        }
        processor.releaseResources();
        return result;
    };

    const auto doubleTail = renderDoubleRepick (false);
    const auto doubleRepick = renderDoubleRepick (true);
    expect (differenceEnergy (doubleRepick.channels[0], doubleTail.channels[0])
                > 1.0e-8
                && differenceEnergy (doubleRepick.channels[1], doubleTail.channels[1])
                       > 1.0e-8,
            "Double repick did not produce a fresh attack in both engines");
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
// messages produce on two different strings, as an end-to-end check alongside
// testPitchWheelByteReconstruction()'s exact one.
void testPitchWheelMidiDispatch()
{
    constexpr double openLowStringHz = 41.2034; // E1, MIDI note 28
    constexpr int openD3Note = 50;
    constexpr double openD3Hz = 146.83238;

    const auto measuredHz = [] (int midiNote, int wheelPosition14,
                                double openHz) -> double
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
        // One fixed +/-260-cent search covers centre and both +/-2-semitone
        // endpoints, so moving the expected seed cannot manufacture a pass.
        return measureFundamentalHz (settled, 0, static_cast<int> (settled.size()),
                                     sampleRate, openHz, 260.0);
    };

    const auto centre = measuredHz (
        electry::ElectryEngine::lowestPlayableNote, 8192,
        openLowStringHz);
    const auto bentUp = measuredHz (
        electry::ElectryEngine::lowestPlayableNote, 16383,
        openLowStringHz);
    const auto bentDown = measuredHz (
        electry::ElectryEngine::lowestPlayableNote, 0,
        openLowStringHz);

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
                                     sampleRate, midpointHz, 160.0);
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

    // A standard pitch-wheel interval is uniform across strings: D3 must
    // travel the same two semitones as E1, preserving chord tuning.
    const auto d3Centre = measuredHz (
        openD3Note, 8192, openD3Hz);
    const auto d3BentUp = measuredHz (
        openD3Note, 16383, openD3Hz);
    const auto d3TravelCents = 1200.0 * std::log2 (d3BentUp / d3Centre);
    expect (d3TravelCents > 170.0 && d3TravelCents < 230.0
                && std::abs (d3TravelCents - centsUp) < 15.0,
            "the same full-up MIDI wheel message did not move open E1 and "
            "D3 by the same two semitones (measured "
                + std::to_string (centsUp) + " and "
                + std::to_string (d3TravelCents) + " cents)");
}

void testMpeRouting()
{
    // Until a valid zone RPN arrives, MIDI channels remain the historical
    // channel-agnostic path, including the global +/-2-semitone wheel.
    const auto renderLegacy = [] (bool spreadAcrossChannels)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::pitchWheel (
            spreadAcrossChannels ? 9 : 1, 12288), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            spreadAcrossChannels ? 2 : 1, 28, (juce::uint8) 105), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            spreadAcrossChannels ? 15 : 1, 52, (juce::uint8) 99), 0);
        renderBlock (processor, audio, midi);
        std::vector<float> rendered (audio.getReadPointer (0),
                                     audio.getReadPointer (0) + blockSize);
        auto tail = renderCapture (processor, audio, 0.35);
        rendered.insert (rendered.end(), tail.begin(), tail.end());
        midi.addEvent (juce::MidiMessage::noteOff (
            spreadAcrossChannels ? 2 : 1, 28), 0);
        midi.addEvent (juce::MidiMessage::noteOff (
            spreadAcrossChannels ? 15 : 1, 52), 0);
        renderBlock (processor, audio, midi);
        tail = renderCapture (processor, audio, 0.35);
        rendered.insert (rendered.end(), tail.begin(), tail.end());
        processor.releaseResources();
        return rendered;
    };
    expect (renderLegacy (true) == renderLegacy (false),
            "MIDI channels changed legacy audio before an MPE zone was enabled");

    // Pulling RPN state ahead of same-sample attacks must not erase the batch
    // boundary that any ignored controller had before MPE support. This RPN is
    // complete but unrelated to zone layout or pitch-bend sensitivity.
    const auto renderLegacyInterruptedChord = [] (int rpnParameter)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (
            4, 40, (juce::uint8) 105), 0);
        if (rpnParameter >= 0)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 101, 0), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (
                1, 100, rpnParameter), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (
                1, 6, rpnParameter == 6 ? 0 : 12), 0);
        }
        else
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (4, 7, 12), 0);
        }
        midi.addEvent (juce::MidiMessage::noteOn (
            4, 52, (juce::uint8) 99), 0);
        renderBlock (processor, audio, midi);
        std::vector<float> rendered (audio.getReadPointer (0),
                                     audio.getReadPointer (0) + blockSize);
        const auto tail = renderCapture (processor, audio, 0.25);
        rendered.insert (rendered.end(), tail.begin(), tail.end());
        processor.releaseResources();
        return rendered;
    };
    const auto interruptedReference = renderLegacyInterruptedChord (-1);
    expect (renderLegacyInterruptedChord (1) == interruptedReference,
            "an unrelated RPN changed legacy same-sample note batching");
    expect (renderLegacyInterruptedChord (0) == interruptedReference,
            "an inactive-zone pitch-range RPN changed legacy note batching");
    expect (renderLegacyInterruptedChord (6) == interruptedReference,
            "a redundant clear-zone RPN changed legacy note batching");

    // JUCE's zone layout parser accepts the same numeric parameter from an
    // NRPN unless the caller filters it. Parameter 6 sent as an NRPN must stay
    // an ignored legacy controller sequence, not silently enable a zone.
    const auto renderAfterZoneLikeNrpn = [] (bool useNrpn)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        if (useNrpn)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 98, 6), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 99, 0), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 6, 2), 0);
        }
        else
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 7, 6), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 8, 0), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 9, 2), 0);
        }
        midi.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            2, 28, (juce::uint8) 105), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            3, 52, (juce::uint8) 99), 0);
        renderBlock (processor, audio, midi);
        std::vector<float> rendered (audio.getReadPointer (0),
                                     audio.getReadPointer (0) + blockSize);
        const auto tail = renderCapture (processor, audio, 0.3);
        rendered.insert (rendered.end(), tail.begin(), tail.end());
        processor.releaseResources();
        return rendered;
    };
    expect (renderAfterZoneLikeNrpn (true)
                == renderAfterZoneLikeNrpn (false),
            "NRPN 6 was incorrectly accepted as an MPE zone RPN");

    const auto renderConfiguredChord = [] (int setupPosition,
                                            bool interleaveTimbre)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        const auto addSetup = [&]
        {
            const auto setup = juce::MPEMessages::setLowerZone (2, 5, 1);
            midi.addEvents (setup, 0, -1, 0);
        };
        if (setupPosition == 0)
            addSetup();
        midi.addEvent (juce::MidiMessage::noteOn (
            2, 40, (juce::uint8) 105), 0);
        if (setupPosition == 1)
            addSetup();
        if (interleaveTimbre)
            midi.addEvent (juce::MidiMessage::controllerEvent (2, 74, 96), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            3, 50, (juce::uint8) 99), 0);
        if (setupPosition == 2)
            addSetup();
        renderBlock (processor, audio, midi);
        std::vector<float> rendered (audio.getReadPointer (0),
                                     audio.getReadPointer (0) + blockSize);
        const auto tail = renderCapture (processor, audio, 0.25);
        rendered.insert (rendered.end(), tail.begin(), tail.end());
        processor.releaseResources();
        return rendered;
    };
    const auto setupFirst = renderConfiguredChord (0, false);
    expect (renderConfiguredChord (1, false) == setupFirst
                && renderConfiguredChord (2, false) == setupFirst,
            "same-sample MPE setup order changed chord allocation or audio");
    expect (renderConfiguredChord (0, true) == setupFirst,
            "unassigned MPE CC74 split a same-sample member chord");

    // With only channel 3 sounding, a channel-2 member wheel must be
    // bit-identical to no event. This proves selective routing without asking
    // a spectral estimator to separate overlapping guitar harmonics.
    const auto renderForeignMemberBend = [] (bool sendForeignBend)
    {
        ElectryAudioProcessor memberProcessor;
        setParameterValue (memberProcessor,
                           electry::parameters::bodyResonance, 0.0f);
        setParameterValue (memberProcessor, electry::parameters::artifacts,
                           0.0f);
        setParameterValue (memberProcessor, electry::parameters::sympathetic,
                           0.0f);
        memberProcessor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> memberAudio;
        juce::MidiBuffer memberMidi;
        const auto memberSetup = juce::MPEMessages::setLowerZone (2, 5, 1);
        memberMidi.addEvents (memberSetup, 0, -1, 0);
        memberMidi.addEvent (juce::MidiMessage::noteOn (
            3, 50, (juce::uint8) 105), 0);
        renderBlock (memberProcessor, memberAudio, memberMidi);
        if (sendForeignBend)
            memberMidi.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
        renderBlock (memberProcessor, memberAudio, memberMidi);
        std::vector<float> result (memberAudio.getReadPointer (0),
                                   memberAudio.getReadPointer (0) + blockSize);
        const auto tail = renderCapture (memberProcessor, memberAudio, 0.3);
        result.insert (result.end(), tail.begin(), tail.end());
        memberProcessor.releaseResources();
        return result;
    };
    expect (renderForeignMemberBend (true)
                == renderForeignMemberBend (false),
            "one MPE member wheel changed another member's sounding string");

    constexpr double lowOpenHz = 41.2034;
    ElectryAudioProcessor processor;
    setParameterValue (processor, electry::parameters::bodyResonance, 0.0f);
    setParameterValue (processor, electry::parameters::artifacts, 0.0f);
    setParameterValue (processor, electry::parameters::sympathetic, 0.0f);
    setParameterValue (processor, electry::parameters::bendTime, 0.04f);
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    // Insert notes before the complete same-sample RPN sequence. The
    // processor's conditioning pass must still activate the zone first.
    midi.addEvent (juce::MidiMessage::noteOn (2, 28, (juce::uint8) 110), 0);
    auto setup = juce::MPEMessages::setLowerZone (2, 5, 1);
    midi.addEvents (setup, 0, -1, 0);
    renderBlock (processor, audio, midi);

    const auto measureLow = [&]
    {
        renderSeconds (processor, audio, 0.55);
        const auto capture = renderCapture (processor, audio, 0.35);
        // One fixed window covers every state below: -5..+6 semitones.
        return measureFundamentalHz (
            capture, 0, static_cast<int> (capture.size()), sampleRate,
            lowOpenHz * std::pow (2.0, 0.5 / 12.0), 580.0);
    };
    const auto cents = [] (double value, double reference)
    {
        return 1200.0 * std::log2 (value / reference);
    };

    const auto baseline = measureLow();
    midi.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
    renderBlock (processor, audio, midi);
    const auto memberBent = measureLow();
    const auto memberCents = cents (memberBent, baseline);
    expect (memberCents > 470.0 && memberCents < 530.0,
            "an MPE member wheel did not use its declared five-semitone "
            "range (measured " + std::to_string (memberCents) + " cents)");

    midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
    renderBlock (processor, audio, midi);
    const auto masterBent = measureLow();
    const auto masterCents = cents (masterBent, baseline);
    expect (masterCents > 570.0 && masterCents < 630.0,
            "the MPE master wheel did not add its declared semitone to the "
            "member interval (measured " + std::to_string (masterCents)
                + " cents)");

    // Reset All Controllers centres every expression wheel without deleting
    // the zone: a subsequent channel-2 wheel must remain per-note.
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
    renderBlock (processor, audio, midi);
    const auto reset = measureLow();
    expect (std::abs (cents (reset, baseline)) < 20.0,
            "CC121 did not centre all MPE expression bends");
    midi.addEvent (juce::MidiMessage::pitchWheel (2, 0), 0);
    renderBlock (processor, audio, midi);
    const auto afterReset = measureLow();
    expect (cents (afterReset, baseline) < -470.0,
            "CC121 discarded the MPE zone instead of retaining its routing");

    // Reusing an idle member channel starts from centre unless a new member
    // wheel arrives. The zone-master bend remains live across that handoff.
    midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::noteOff (2, 28), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::noteOn (
        2, 28, (juce::uint8) 110), 0);
    renderBlock (processor, audio, midi);
    const auto reusedMember = measureLow();
    const auto reusedCents = cents (reusedMember, baseline);
    expect (reusedCents > 70.0 && reusedCents < 130.0,
            "an idle MPE member retained its old bend or lost the master "
            "bend (measured " + std::to_string (reusedCents) + " cents)");
    processor.releaseResources();

    // The upper zone has a different master channel and reverse member range.
    // Exercise those indices and the mirrored Double engine independently of
    // the lower-zone path above.
    ElectryAudioProcessor upperProcessor;
    setParameterValue (upperProcessor, electry::parameters::outputMode, 2.0f);
    setParameterValue (upperProcessor, electry::parameters::bodyResonance, 0.0f);
    setParameterValue (upperProcessor, electry::parameters::artifacts, 0.0f);
    setParameterValue (upperProcessor, electry::parameters::sympathetic, 0.0f);
    setParameterValue (upperProcessor, electry::parameters::bendTime, 0.04f);
    upperProcessor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> upperAudio;
    juce::MidiBuffer upperMidi;
    auto upperSetup = juce::MPEMessages::setUpperZone (1, 3, 1);
    upperMidi.addEvents (upperSetup, 0, -1, 0);
    upperMidi.addEvent (juce::MidiMessage::noteOn (
        15, 45, (juce::uint8) 108), 0);
    renderBlock (upperProcessor, upperAudio, upperMidi);
    renderSeconds (upperProcessor, upperAudio, 0.55);
    const auto captureUpperStereo = [&]
    {
        juce::MidiBuffer emptyMidi;
        std::pair<std::vector<float>, std::vector<float>> captured;
        int remaining = static_cast<int> (0.35 * sampleRate);
        captured.first.reserve (static_cast<std::size_t> (remaining));
        captured.second.reserve (static_cast<std::size_t> (remaining));
        while (remaining > 0)
        {
            const int samples = std::min (blockSize, remaining);
            renderBlock (upperProcessor, upperAudio, emptyMidi, samples);
            const auto* left = upperAudio.getReadPointer (0);
            const auto* right = upperAudio.getReadPointer (1);
            captured.first.insert (captured.first.end(), left, left + samples);
            captured.second.insert (captured.second.end(), right,
                                    right + samples);
            remaining -= samples;
        }
        return captured;
    };
    const auto upperBaselineCapture = captureUpperStereo();
    const double upperSearchCentre = 110.0 * std::pow (2.0, 2.0 / 12.0);
    const double upperBaselineLeft = measureFundamentalHz (
        upperBaselineCapture.first, 0,
        static_cast<int> (upperBaselineCapture.first.size()), sampleRate,
        upperSearchCentre, 260.0);
    const double upperBaselineRight = measureFundamentalHz (
        upperBaselineCapture.second, 0,
        static_cast<int> (upperBaselineCapture.second.size()), sampleRate,
        upperSearchCentre, 260.0);
    upperMidi.addEvent (juce::MidiMessage::pitchWheel (15, 16383), 0);
    renderBlock (upperProcessor, upperAudio, upperMidi);
    renderSeconds (upperProcessor, upperAudio, 0.55);
    const auto upperMemberCapture = captureUpperStereo();
    const double upperMemberLeft = measureFundamentalHz (
        upperMemberCapture.first, 0,
        static_cast<int> (upperMemberCapture.first.size()), sampleRate,
        upperSearchCentre, 260.0);
    const double upperMemberRight = measureFundamentalHz (
        upperMemberCapture.second, 0,
        static_cast<int> (upperMemberCapture.second.size()), sampleRate,
        upperSearchCentre, 260.0);
    const double upperMemberLeftCents = cents (
        upperMemberLeft, upperBaselineLeft);
    const double upperMemberRightCents = cents (
        upperMemberRight, upperBaselineRight);
    expect (upperMemberLeftCents > 270.0 && upperMemberLeftCents < 330.0
                && upperMemberRightCents > 270.0
                && upperMemberRightCents < 330.0,
            "upper-zone member pitch did not reach both Double engines "
            "selectively (measured "
                + std::to_string (upperMemberLeftCents) + " and "
                + std::to_string (upperMemberRightCents) + " cents)");

    upperMidi.addEvent (juce::MidiMessage::pitchWheel (16, 16383), 0);
    renderBlock (upperProcessor, upperAudio, upperMidi);
    renderSeconds (upperProcessor, upperAudio, 0.55);
    const auto upperBentCapture = captureUpperStereo();
    const double upperBentLeft = measureFundamentalHz (
        upperBentCapture.first, 0,
        static_cast<int> (upperBentCapture.first.size()), sampleRate,
        upperSearchCentre, 260.0);
    const double upperBentRight = measureFundamentalHz (
        upperBentCapture.second, 0,
        static_cast<int> (upperBentCapture.second.size()), sampleRate,
        upperSearchCentre, 260.0);
    const double upperLeftCents = cents (upperBentLeft, upperBaselineLeft);
    const double upperRightCents = cents (upperBentRight, upperBaselineRight);
    expect (upperLeftCents > 370.0 && upperLeftCents < 430.0
                && upperRightCents > 370.0 && upperRightCents < 430.0,
            "upper-zone member/master pitch did not reach both Double "
            "engines (measured " + std::to_string (upperLeftCents) + " and "
                + std::to_string (upperRightCents) + " cents)");
    upperProcessor.releaseResources();
}

void testMpeIdlePitchPreservesReleaseTail()
{
    constexpr int note = 50;
    constexpr double openHz = 146.83238;
    ElectryAudioProcessor reference;
    ElectryAudioProcessor idleBent;
    for (auto* processor : { &reference, &idleBent })
    {
        setParameterValue (*processor, electry::parameters::bodyResonance, 0.0f);
        setParameterValue (*processor, electry::parameters::artifacts, 0.0f);
        setParameterValue (*processor, electry::parameters::sympathetic, 0.0f);
        setParameterValue (*processor, electry::parameters::bendTime, 0.04f);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    juce::AudioBuffer<float> referenceAudio;
    juce::AudioBuffer<float> idleBentAudio;
    juce::MidiBuffer referenceMidi;
    juce::MidiBuffer idleBentMidi;
    const auto startBentNote = [] (juce::MidiBuffer& midi)
    {
        const auto setup = juce::MPEMessages::setLowerZone (2, 5, 1);
        midi.addEvents (setup, 0, -1, 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            2, note, (juce::uint8) 108), 0);
        midi.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
    };
    startBentNote (referenceMidi);
    startBentNote (idleBentMidi);
    renderBlock (reference, referenceAudio, referenceMidi);
    renderBlock (idleBent, idleBentAudio, idleBentMidi);
    renderSeconds (reference, referenceAudio, 0.55);
    renderSeconds (idleBent, idleBentAudio, 0.55);

    referenceMidi.addEvent (juce::MidiMessage::noteOff (2, note), 0);
    idleBentMidi.addEvent (juce::MidiMessage::noteOff (2, note), 0);
    renderBlock (reference, referenceAudio, referenceMidi);
    renderBlock (idleBent, idleBentAudio, idleBentMidi);

    // A wheel message received while the channel is idle belongs to its next
    // MPE finger. It must be cached without retuning the still-audible string
    // tail left by the preceding finger.
    idleBentMidi.addEvent (juce::MidiMessage::pitchWheel (2, 0), 0);
    renderBlock (reference, referenceAudio, referenceMidi);
    renderBlock (idleBent, idleBentAudio, idleBentMidi);
    expect (referenceAudio.getMagnitude (0, blockSize) > 1.0e-7f,
            "MPE release-tail fixture decayed to silence before its check");
    expect (std::equal (referenceAudio.getReadPointer (0),
                        referenceAudio.getReadPointer (0) + blockSize,
                        idleBentAudio.getReadPointer (0))
                && std::equal (referenceAudio.getReadPointer (1),
                               referenceAudio.getReadPointer (1) + blockSize,
                               idleBentAudio.getReadPointer (1)),
            "idle member pitch-wheel traffic retuned a released string tail");

    referenceMidi.addEvent (juce::MidiMessage::noteOn (
        2, note, (juce::uint8) 108), 0);
    idleBentMidi.addEvent (juce::MidiMessage::noteOn (
        2, note, (juce::uint8) 108), 0);
    renderBlock (reference, referenceAudio, referenceMidi);
    renderBlock (idleBent, idleBentAudio, idleBentMidi);
    renderSeconds (reference, referenceAudio, 0.55);
    renderSeconds (idleBent, idleBentAudio, 0.55);
    const auto referenceCapture = renderCapture (reference, referenceAudio, 0.35);
    const auto idleBentCapture = renderCapture (idleBent, idleBentAudio, 0.35);
    const double searchCentre = openHz * std::pow (2.0, -2.5 / 12.0);
    const double referenceHz = measureFundamentalHz (
        referenceCapture, 0, static_cast<int> (referenceCapture.size()),
        sampleRate, searchCentre, 280.0);
    const double idleBentHz = measureFundamentalHz (
        idleBentCapture, 0, static_cast<int> (idleBentCapture.size()),
        sampleRate, searchCentre, 280.0);
    const double cachedBendCents =
        1200.0 * std::log2 (idleBentHz / referenceHz);
    expect (cachedBendCents < -470.0 && cachedBendCents > -530.0,
            "a pre-note MPE member wheel was not applied to the next finger "
            "(measured " + std::to_string (cachedBendCents) + " cents)");

    reference.releaseResources();
    idleBent.releaseResources();
}

void testMpeFractionalRangeAndLiveMasterTail()
{
    constexpr int note = 50;
    constexpr double openHz = 146.83238;
    ElectryAudioProcessor processor;
    setParameterValue (processor, electry::parameters::bodyResonance, 0.0f);
    setParameterValue (processor, electry::parameters::artifacts, 0.0f);
    setParameterValue (processor, electry::parameters::sympathetic, 0.0f);
    setParameterValue (processor, electry::parameters::bendTime, 0.04f);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    const auto setup = juce::MPEMessages::setLowerZone (2, 2, 1);
    midi.addEvents (setup, 0, -1, 0);
    // Raw RPN 0 Data Entry MSB/LSB: two semitones plus 50 cents. JUCE's
    // MPEZoneLayout retains the integer two for routing, while Electry must
    // perform the full 2.50-semitone interval.
    const auto exactMemberRange = juce::MidiRPNGenerator::generate (
        2, 0, 2 * 128 + 50, false, true);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    midi.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        2, note, (juce::uint8) 108), 0);
    renderBlock (processor, audio, midi);

    const auto measureAt = [&] (double cents)
    {
        renderSeconds (processor, audio, 0.5);
        const auto capture = renderCapture (processor, audio, 0.35);
        return measureFundamentalHz (
            capture, 0, static_cast<int> (capture.size()), sampleRate,
            openHz * std::pow (2.0, cents / 1200.0), 90.0);
    };
    const auto centsFromOpen = [] (double frequency)
    {
        return 1200.0 * std::log2 (frequency / openHz);
    };

    // Refine the range while the already-bent note is sounding. Its whole
    // semitone value remains two, so only the CC38 cents update can trigger
    // the retune; JUCE's integer MPEZoneLayout is unchanged.
    midi.addEvents (exactMemberRange, 0, -1, 0);
    renderBlock (processor, audio, midi);
    const double memberCents = centsFromOpen (measureAt (250.0));
    expect (memberCents > 232.0 && memberCents < 268.0,
            "RPN 0 lost its 50-cent Data Entry LSB (measured "
                + std::to_string (memberCents) + " cents)");

    midi.addEvent (juce::MidiMessage::noteOff (2, note), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() == 1,
            "CC64 did not retain the fractional-range MPE note tail");

    // Per-note control ends at Note Off, but Zone Master pitch remains live
    // for the still-sounding pedal tail. The member's full-down wheel must be
    // ignored by that tail, then the one-semitone master must add to its
    // frozen +2.50-semitone performed interval.
    midi.addEvent (juce::MidiMessage::pitchWheel (2, 0), 0);
    midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
    renderBlock (processor, audio, midi);
    const double tailCents = centsFromOpen (measureAt (350.0));
    expect (tailCents > 332.0 && tailCents < 368.0,
            "MPE master did not remain live independently of the frozen "
            "member bend on a CC64-held tail (measured "
                + std::to_string (tailCents) + " cents)");

    // Reset All Controllers centres the wheels and releases the pedal. It
    // nulls the RPN/NRPN selection transaction, so a later bare Data Entry LSB
    // cannot refine the old RPN 0, but it must retain the already-declared
    // exact range and active zone for the next member finger.
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::controllerEvent (2, 38, 99), 0);
    midi.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        2, note, (juce::uint8) 108), 0);
    renderBlock (processor, audio, midi);
    const double retainedCents = centsFromOpen (measureAt (250.0));
    expect (retainedCents > 232.0 && retainedCents < 268.0,
            "CC121 did not null Data Entry or preserve the exact MPE range "
            "(measured "
                + std::to_string (retainedCents) + " cents)");

    // A host re-prepare is another parser boundary, not a new MPE
    // configuration. Keep the exact range and zone, but discard this selected
    // RPN 0 so the first bare Data Entry in the new run cannot alter it.
    midi.addEvent (juce::MidiMessage::controllerEvent (2, 101, 0), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (2, 100, 0), 0);
    renderBlock (processor, audio, midi);
    processor.prepareToPlay (sampleRate, blockSize);
    midi.addEvent (juce::MidiMessage::controllerEvent (2, 6, 3), 0);
    midi.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        2, note, (juce::uint8) 108), 0);
    renderBlock (processor, audio, midi);
    const double preparedCents = centsFromOpen (measureAt (250.0));
    expect (preparedCents > 232.0 && preparedCents < 268.0,
            "prepare retained an RPN selector or discarded the exact MPE "
            "range (measured " + std::to_string (preparedCents) + " cents)");

    processor.releaseResources();
}

void testMpeSamePitchOwnershipAcrossLayoutChange()
{
    ElectryAudioProcessor processor;
    processor.setPlayStyleKeysHold (true);
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    auto setup = juce::MPEMessages::setLowerZone (2, 2, 2);
    midi.addEvents (setup, 0, -1, 0);
    midi.addEvent (juce::MidiMessage::noteOn (2, 52, (juce::uint8) 110), 0);
    midi.addEvent (juce::MidiMessage::noteOn (3, 52, (juce::uint8) 105), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() == 2,
            "same-pitch MPE owners did not allocate two independent strings");

    // Articulation and momentary-gesture notes remain global controls when a
    // controller happens to send them on a member channel.
    constexpr int palm = static_cast<int> (electry::PlayStyle::PalmMute);
    constexpr int palmKey =
        electry::ElectryEngine::firstPlayStyleKeyswitchNote + palm;
    midi.addEvent (juce::MidiMessage::noteOn (
        2, palmKey, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getEffectivePlayStyleIndex() == palm,
            "an MPE member-channel HOLD key did not engage globally");
    midi.addEvent (juce::MidiMessage::noteOff (2, palmKey), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getEffectivePlayStyleIndex()
                == static_cast<int> (electry::PlayStyle::Sustain),
            "an MPE member-channel HOLD release remained latched");

    midi.addEvent (juce::MidiMessage::noteOn (
        2, electry::ElectryEngine::vibratoGestureNote,
        (juce::uint8) 80), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getVibratoGestureForDisplay() == 80,
            "an MPE member-channel vibrato gesture did not engage globally");
    midi.addEvent (juce::MidiMessage::noteOff (
        2, electry::ElectryEngine::vibratoGestureNote), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getVibratoGestureForDisplay() == 0,
            "an MPE member-channel vibrato release remained latched");

    // The layout change is deliberately inserted after the release at the
    // same timestamp. RPN conditioning runs first, while the fixed ownership
    // table must still route the old channel-2 release to expression ID 2.
    midi.addEvent (juce::MidiMessage::noteOff (2, 52), 0);
    auto clear = juce::MPEMessages::clearLowerZone();
    midi.addEvents (clear, 0, -1, 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 1,
            "a zone change lost same-pitch channel ownership on Note Off");

    midi.addEvent (juce::MidiMessage::noteOff (3, 52), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "the second same-pitch MPE owner did not release independently");

    // Check the opposite transition too: an ID-0 note that began before zone
    // activation must not be released as a newly assigned member-channel ID.
    midi.addEvent (juce::MidiMessage::noteOn (
        2, 52, (juce::uint8) 105), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() == 1,
            "legacy-to-MPE ownership fixture did not start its legacy note");
    midi.addEvent (juce::MidiMessage::noteOff (2, 52), 0);
    setup = juce::MPEMessages::setLowerZone (2, 5, 1);
    midi.addEvents (setup, 0, -1, 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "zone activation misrouted a preceding legacy Note Off");

    // Overlap both identities on one channel/note across a layout change.
    // Note Off is FIFO for that MIDI identity: the older legacy owner must go
    // first, leaving the newer member owner available for per-note bend.
    clear = juce::MPEMessages::clearLowerZone();
    midi.addEvents (clear, 0, -1, 0);
    renderBlock (processor, audio, midi);
    midi.addEvent (juce::MidiMessage::noteOn (
        2, 52, (juce::uint8) 105), 0);
    renderBlock (processor, audio, midi);
    setup = juce::MPEMessages::setLowerZone (2, 5, 1);
    midi.addEvents (setup, 0, -1, 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        2, 52, (juce::uint8) 108), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() == 2,
            "mixed legacy/MPE overlap did not retain both string owners");
    midi.addEvent (juce::MidiMessage::noteOff (2, 52), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 1,
            "the first mixed-identity Note Off did not leave one owner");
    const auto mixedBaselineCapture = renderCapture (processor, audio, 0.35);
    const double mixedSearchCentre =
        164.81378 * std::pow (2.0, 2.5 / 12.0);
    const double mixedBaseline = measureFundamentalHz (
        mixedBaselineCapture, 0,
        static_cast<int> (mixedBaselineCapture.size()), sampleRate,
        mixedSearchCentre, 280.0);
    midi.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 0.55);
    const auto mixedBentCapture = renderCapture (processor, audio, 0.35);
    const double mixedBent = measureFundamentalHz (
        mixedBentCapture, 0, static_cast<int> (mixedBentCapture.size()),
        sampleRate, mixedSearchCentre, 280.0);
    const double mixedCents = 1200.0 * std::log2 (mixedBent / mixedBaseline);
    expect (mixedCents > 470.0 && mixedCents < 530.0,
            "an older legacy Note Off released the newer MPE owner (measured "
                + std::to_string (mixedCents) + " cents)");
    midi.addEvent (juce::MidiMessage::noteOff (2, 52), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "the second mixed-identity Note Off did not release its MPE owner");

    // Mirror the overlap: older member owner, then a newer legacy owner after
    // the zone is cleared. FIFO must release the member first even though the
    // current layout says channel 2 is ordinary MIDI.
    midi.addEvent (juce::MidiMessage::noteOn (
        2, 52, (juce::uint8) 108), 0);
    renderBlock (processor, audio, midi);
    clear = juce::MPEMessages::clearLowerZone();
    midi.addEvents (clear, 0, -1, 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        2, 52, (juce::uint8) 105), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() == 2,
            "mixed MPE/legacy overlap did not retain both string owners");
    midi.addEvent (juce::MidiMessage::noteOff (2, 52), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 1,
            "reverse mixed-identity Note Off did not leave one owner");
    const auto reverseBaselineCapture = renderCapture (processor, audio, 0.35);
    const double reverseSearchCentre =
        164.81378 * std::pow (2.0, 1.0 / 12.0);
    const double reverseBaseline = measureFundamentalHz (
        reverseBaselineCapture, 0,
        static_cast<int> (reverseBaselineCapture.size()), sampleRate,
        reverseSearchCentre, 160.0);
    midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 0.55);
    const auto reverseBentCapture = renderCapture (processor, audio, 0.35);
    const double reverseBent = measureFundamentalHz (
        reverseBentCapture, 0, static_cast<int> (reverseBentCapture.size()),
        sampleRate, reverseSearchCentre, 160.0);
    const double reverseCents =
        1200.0 * std::log2 (reverseBent / reverseBaseline);
    expect (reverseCents > 170.0 && reverseCents < 230.0,
            "an older MPE Note Off released the newer legacy owner (measured "
                + std::to_string (reverseCents) + " cents)");
    setup = juce::MPEMessages::setLowerZone (2, 5, 1);
    midi.addEvents (setup, 0, -1, 0);
    midi.addEvent (juce::MidiMessage::noteOff (2, 52), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "the second reverse mixed-identity Note Off did not release its "
            "legacy owner");
    processor.releaseResources();
}

void testMpeOwnershipCapacityIsClosed()
{
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    auto setup = juce::MPEMessages::setLowerZone (2, 5, 1);
    midi.addEvents (setup, 0, -1, 0);
    for (int owner = 0; owner < 128; ++owner)
        midi.addEvent (juce::MidiMessage::noteOn (
            2, 52, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);

    // The fixed per-pitch FIFO is full. This next attack arrives in another
    // callback and must be rejected rather than sounded without a releasable
    // ownership record.
    midi.addEvent (juce::MidiMessage::noteOn (
        2, 52, (juce::uint8) 100), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() == 1,
            "MPE ownership-capacity fixture did not retain its repeated note");

    auto clear = juce::MPEMessages::clearLowerZone();
    midi.addEvents (clear, 0, -1, 0);
    for (int owner = 0; owner < 129; ++owner)
        midi.addEvent (juce::MidiMessage::noteOff (2, 52), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 3.0);
    expect (processor.getActiveVoiceCount() == 0,
            "a Note On beyond the fixed MPE ownership capacity left an "
            "unreleasable string owner");
    processor.releaseResources();
}

void testMpeOwnershipLifecycleBoundaries()
{
    enum class Boundary { Panic, AllSoundOff, AllNotesOff, Prepare };
    const auto nameOf = [] (Boundary boundary)
    {
        switch (boundary)
        {
            case Boundary::Panic: return "Panic";
            case Boundary::AllSoundOff: return "CC120";
            case Boundary::AllNotesOff: return "CC123";
            case Boundary::Prepare: return "prepareToPlay";
        }
        return "unknown";
    };

    for (const auto boundary : { Boundary::Panic, Boundary::AllSoundOff,
                                 Boundary::AllNotesOff, Boundary::Prepare })
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        auto setup = juce::MPEMessages::setLowerZone (2, 5, 1);
        midi.addEvents (setup, 0, -1, 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            2, 52, (juce::uint8) 105), 0);
        renderBlock (processor, audio, midi);

        if (boundary == Boundary::Panic)
        {
            processor.requestPanic();
            renderBlock (processor, audio, midi);
        }
        else if (boundary == Boundary::Prepare)
        {
            processor.prepareToPlay (sampleRate, blockSize);
        }
        else
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (
                1, boundary == Boundary::AllSoundOff ? 120 : 123, 0), 0);
            renderBlock (processor, audio, midi);
        }

        // Start the same channel/note as legacy MIDI, then reactivate MPE
        // before Note Off. Any stale pre-boundary ID-2 FIFO entry would be
        // consumed first and leave the new ID-0 string held forever.
        auto clear = juce::MPEMessages::clearLowerZone();
        midi.addEvents (clear, 0, -1, 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            2, 52, (juce::uint8) 105), 0);
        renderBlock (processor, audio, midi);
        setup = juce::MPEMessages::setLowerZone (2, 5, 1);
        midi.addEvents (setup, 0, -1, 0);
        midi.addEvent (juce::MidiMessage::noteOff (2, 52), 0);
        renderBlock (processor, audio, midi);
        renderSeconds (processor, audio, 1.0);
        expect (processor.getActiveVoiceCount() == 0,
                std::string (nameOf (boundary))
                    + " left stale MPE ownership across its lifecycle boundary");
        processor.releaseResources();
    }
}

// The CC1 resonance and the acoustic-return wiring live in the shell: the
// processor returns fixed-delay causal chunks to the engine and derives the
// rig's loudness from its amplifier controls. This closes the actual plug-in
// loop, so deleting the return, CC1 dispatch or loudness derivation fails.
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

// A DAW callback is only a transport partition; it cannot be the voiced
// acoustic delay. The same feedback performance must therefore produce
// the same samples at common 64, 256 and 1024-sample host block sizes. Before
// the fixed acoustic FIFO and causal sub-block scheduler, these three renders
// locked onto different howl modes even though every MIDI and parameter value
// was identical.
void testResonanceFeedbackIsBlockSizeInvariant()
{
    struct StereoTrace
    {
        std::vector<float> left;
        std::vector<float> right;
    };

    constexpr int totalSamples = 8192;
    constexpr int noteOnSample = 137;
    constexpr int noteOffSample = 4099;
    const auto render = [] (int hostBlockSize)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, hostBlockSize);
        setParameterValue (processor, electry::parameters::amp, 0.9f);
        setParameterValue (processor, electry::parameters::distortion, 0.7f);
        setParameterValue (processor, electry::parameters::compressor, 0.35f);
        setParameterValue (processor, electry::parameters::delay, 0.30f);
        setParameterValue (processor, electry::parameters::room, 0.35f);
        setParameterValue (processor, electry::parameters::resonanceDepth,
                           100.0f);
        setParameterValue (processor, electry::parameters::sympathetic, 0.0f);
        setParameterValue (processor, electry::parameters::artifacts, 0.0f);
        setParameterValue (processor, electry::parameters::pickNoise, 0.0f);
        setParameterValue (processor, electry::parameters::fingerNoise, 0.0f);
        setParameterValue (processor, electry::parameters::releaseNoise, 0.0f);

        StereoTrace result;
        result.left.reserve (totalSamples);
        result.right.reserve (totalSamples);
        juce::AudioBuffer<float> audio;
        int rendered = 0;
        while (rendered < totalSamples)
        {
            const int samples = std::min (hostBlockSize,
                                          totalSamples - rendered);
            juce::MidiBuffer midi;
            const auto addAtAbsoluteSample = [&] (const juce::MidiMessage& event,
                                                  int absoluteSample)
            {
                if (absoluteSample >= rendered
                    && absoluteSample < rendered + samples)
                {
                    midi.addEvent (event, absoluteSample - rendered);
                }
            };
            addAtAbsoluteSample (
                juce::MidiMessage::controllerEvent (1, 1, 127), noteOnSample);
            addAtAbsoluteSample (
                juce::MidiMessage::noteOn (1, 40, (juce::uint8) 120),
                noteOnSample);
            addAtAbsoluteSample (juce::MidiMessage::noteOff (1, 40),
                                 noteOffSample);
            renderBlock (processor, audio, midi, samples);
            const auto* left = audio.getReadPointer (0);
            const auto* right = audio.getReadPointer (1);
            result.left.insert (result.left.end(), left, left + samples);
            result.right.insert (result.right.end(), right, right + samples);
            rendered += samples;
        }
        processor.releaseResources();
        return result;
    };

    const auto at64 = render (64);
    const auto at256 = render (256);
    const auto at1024 = render (1024);
    const auto finite = [] (const StereoTrace& trace)
    {
        return std::all_of (trace.left.begin(), trace.left.end(),
                            [] (float sample) { return std::isfinite (sample); })
            && std::all_of (trace.right.begin(), trace.right.end(),
                            [] (float sample) { return std::isfinite (sample); });
    };
    const auto peak = [] (const StereoTrace& trace)
    {
        float result = 0.0f;
        for (const float sample : trace.left)
            result = std::max (result, std::abs (sample));
        for (const float sample : trace.right)
            result = std::max (result, std::abs (sample));
        return result;
    };
    expect (finite (at64) && finite (at256) && finite (at1024),
            "a block-partition feedback trace contained non-finite audio");
    expect (peak (at256) > 1.0e-5f,
            "the block-partition feedback fixture rendered silence");
    expect (at64.left == at256.left && at256.left == at1024.left
                && at64.right == at256.right && at256.right == at1024.right,
            "the resonance-feedback performance changed with the host block "
            "size");
}

// MIDI pressure is deliberately unassigned. Inserting channel pressure or
// unrelated poly aftertouch between simultaneous notes must neither retune the
// guitar nor split the notes out of their canonical chord allocation batch.
void testMidiPressureLeavesChordUnchanged()
{
    enum class Pressure { None, Channel, Poly };

    const auto renderChord = [] (Pressure pressure)
    {
        ElectryAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (
            1, 40, (juce::uint8) 100), 0);
        if (pressure == Pressure::Channel)
            midi.addEvent (juce::MidiMessage::channelPressureChange (1, 127), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            1, 43, (juce::uint8) 100), 0);
        if (pressure == Pressure::Poly)
            midi.addEvent (juce::MidiMessage::aftertouchChange (1, 64, 127), 0);
        midi.addEvent (juce::MidiMessage::noteOn (
            1, 47, (juce::uint8) 100), 0);
        renderBlock (processor, audio, midi);

        const auto* channel = audio.getReadPointer (0);
        std::vector<float> rendered (channel, channel + audio.getNumSamples());
        auto tail = renderCapture (processor, audio, 0.6);
        rendered.insert (rendered.end(), tail.begin(), tail.end());
        processor.releaseResources();
        return rendered;
    };

    const auto reference = renderChord (Pressure::None);
    expect (renderChord (Pressure::Channel) == reference,
            "channel pressure changed a simultaneous chord's pitch, audio or "
            "canonical string allocation");
    expect (renderChord (Pressure::Poly) == reference,
            "unrelated poly aftertouch changed a simultaneous chord's pitch, "
            "audio or canonical string allocation");
}

// dispatchMidiData()'s handling of MIDI CC121 (Reset All Controllers) fans
// out into pitch-bend, resonance, mute-pressure and sustain resets - a path no
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
        const double resetSearchCentre =
            openLowStringHz * std::pow (2.0, 1.0 / 12.0);
        const auto bentHz = measureFundamentalHz (
            bentSettled, 0, static_cast<int> (bentSettled.size()), sampleRate,
            resetSearchCentre, 160.0);
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
            resetSearchCentre, 160.0);
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
    status.setStatus (1, 0, true, sampleRate, 127, 0, 0, false);
    expect (status.getStatusText().contains ("CC2 MUTE +100%")
                && status.getTitle() == status.getStatusText(),
            "the status display did not expose full live CC2 mute pressure");
    status.setStatus (1, 0, true, sampleRate, 0, 0, 0, false);
    expect (! status.getStatusText().contains ("CC2 MUTE")
                && status.getStatusText().contains ("48.0 kHz"),
            "the status display did not restore its normal readout at CC2 zero");

    sendController (2, 64);
    processor.releaseResources();
    expect (processor.getMidiMutePressureForDisplay() == 0,
            "releaseResources left stale CC2 mute pressure visible");
}

void testVibratoGestureMidiAndLifecycle()
{
    constexpr int gestureNote = electry::ElectryEngine::vibratoGestureNote;
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.getVibratoGestureForDisplay() == 0,
            "a prepared processor displayed stale A#0 vibrato");

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    const auto noteOn = [&] (int velocity)
    {
        midi.addEvent (juce::MidiMessage::noteOn (
            1, gestureNote, static_cast<juce::uint8> (velocity)), 0);
        renderBlock (processor, audio, midi);
    };
    const auto noteOff = [&]
    {
        midi.addEvent (juce::MidiMessage::noteOff (1, gestureNote), 0);
        renderBlock (processor, audio, midi);
    };

    noteOn (64);
    expect (processor.getVibratoGestureForDisplay() == 64
                && processor.getActiveVoiceCount() == 0,
            "A#0 did not become a silent velocity-shaped vibrato gesture");
    noteOn (127);
    expect (processor.getVibratoGestureForDisplay() == 127,
            "a newer A#0 owner did not update the vibrato width");
    noteOff();
    expect (processor.getVibratoGestureForDisplay() == 127,
            "one A#0 Note Off cancelled another held owner");
    noteOff();
    noteOff();
    expect (processor.getVibratoGestureForDisplay() == 0,
            "balanced or stray A#0 Note Off left vibrato held");

    noteOn (64);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, gestureNote, static_cast<juce::uint8> (0)), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getVibratoGestureForDisplay() == 0,
            "velocity-zero A#0 did not act as Note Off");

    processor.keyboardState.noteOn (
        1, gestureNote, 64.0f / 127.0f);
    renderBlock (processor, audio, midi);
    expect (processor.getVibratoGestureForDisplay() == 64,
            "the on-screen A#0 key did not reach the gesture path");
    processor.keyboardState.noteOff (1, gestureNote, 0.0f);
    renderBlock (processor, audio, midi);
    expect (processor.getVibratoGestureForDisplay() == 0,
            "releasing the on-screen A#0 key left vibrato held");

    noteOn (96);
    for (const int controller : { 121, 120 })
    {
        midi.addEvent (juce::MidiMessage::controllerEvent (
            1, controller, 0), 0);
        renderBlock (processor, audio, midi);
        expect (processor.getVibratoGestureForDisplay() == 96,
                std::string (controller == 121 ? "CC121" : "CC120")
                    + " released the physically held A#0 gesture");
    }
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getVibratoGestureForDisplay() == 0,
            "All Notes Off did not release A#0 vibrato");

    noteOn (127);
    processor.requestPanic();
    renderBlock (processor, audio, midi);
    expect (processor.getVibratoGestureForDisplay() == 0,
            "Panic left A#0 vibrato armed");

    const auto renderGestureTail = [] (int note, bool gesture)
    {
        struct Tail
        {
            std::vector<float> left;
            int fret = -1;
        } result;

        ElectryAudioProcessor p;
        p.prepareToPlay (sampleRate, blockSize);
        setParameterValue (p, electry::parameters::sympathetic, 0.0f);
        setParameterValue (p, electry::parameters::artifacts, 0.0f);
        juce::AudioBuffer<float> block;
        juce::MidiBuffer events;
        events.addEvent (juce::MidiMessage::noteOn (
            1, note, static_cast<juce::uint8> (105)), 0);
        renderBlock (p, block, events);
        renderSeconds (p, block, 0.2);
        if (gesture)
            events.addEvent (juce::MidiMessage::noteOn (
                1, electry::ElectryEngine::vibratoGestureNote,
                static_cast<juce::uint8> (127)), 0);
        renderBlock (p, block, events);
        result.left = renderCapture (p, block, 0.8);
        for (int string = 0; string < electry::ElectryEngine::stringCount; ++string)
        {
            const auto state = p.getStringVisualState (string);
            if (state.sounding && state.midiNote == note)
                result.fret = state.fret;
        }
        p.releaseResources();
        return result;
    };

    const auto stillStopped = renderGestureTail (47, false);
    const auto movingStopped = renderGestureTail (47, true);
    float stoppedDifference = 0.0f;
    for (std::size_t i = 0; i < stillStopped.left.size(); ++i)
        stoppedDifference = std::max (
            stoppedDifference,
            std::abs (stillStopped.left[i] - movingStopped.left[i]));
    expect (stillStopped.fret > 0 && movingStopped.fret > 0
                && stoppedDifference > 1.0e-6f,
            "A#0 did not audibly move a stopped string");

    const auto stillOpen = renderGestureTail (45, false);
    const auto movingOpen = renderGestureTail (45, true);
    expect (stillOpen.fret == 0 && movingOpen.fret == 0
                && stillOpen.left == movingOpen.left,
            "A#0 moved an open string that no finger can rock");

    const auto renderDormantDouble = [] (bool gesture)
    {
        std::vector<float> right;
        ElectryAudioProcessor p;
        p.prepareToPlay (sampleRate, blockSize);
        setParameterValue (p, electry::parameters::sympathetic, 0.0f);
        juce::AudioBuffer<float> block;
        juce::MidiBuffer events;
        if (gesture)
            events.addEvent (juce::MidiMessage::noteOn (
                1, electry::ElectryEngine::vibratoGestureNote,
                static_cast<juce::uint8> (127)), 0);
        renderBlock (p, block, events); // Double is still dormant here.
        setParameterValue (p, electry::parameters::outputMode, 2.0f);
        events.addEvent (juce::MidiMessage::noteOn (
            1, 47, static_cast<juce::uint8> (105)), 0);
        for (int remaining = static_cast<int> (0.8 * sampleRate);
             remaining > 0;)
        {
            const int samples = std::min (blockSize, remaining);
            renderBlock (p, block, events, samples);
            const auto* channel = block.getReadPointer (1);
            right.insert (right.end(), channel, channel + samples);
            remaining -= samples;
        }
        p.releaseResources();
        return right;
    };
    const auto stillDouble = renderDormantDouble (false);
    const auto movingDouble = renderDormantDouble (true);
    float doubleDifference = 0.0f;
    for (std::size_t i = 0; i < stillDouble.size(); ++i)
        doubleDifference = std::max (
            doubleDifference, std::abs (stillDouble[i] - movingDouble[i]));
    expect (doubleDifference > 1.0e-6f,
            "A#0 pressed in Mono did not reach the dormant Double player");

    ElectryStatusDisplay status;
    status.setStatus (1, 0, true, sampleRate, 0, 64, 0, false);
    expect (status.getStatusText().contains ("VIB 50%")
                && status.getTitle() == status.getStatusText(),
            "the status display did not expose live A#0 width");

    noteOn (80);
    processor.releaseResources();
    expect (processor.getVibratoGestureForDisplay() == 0,
            "releaseResources left A#0 vibrato visible");
    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.getVibratoGestureForDisplay() == 0,
            "a later prepare restored stale A#0 vibrato");
    processor.releaseResources();
}

void testTremoloPickingMidiAndLifecycle()
{
    constexpr int gestureNote = electry::ElectryEngine::tremoloGestureNote;
    constexpr int heldNote = electry::ElectryEngine::lowestPlayableNote;
    ElectryAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, electry::parameters::sympathetic, 0.0f);
    expect (gestureNote == 23
                && processor.getTremoloGestureForDisplay() == 0,
            "a prepared processor lost B0 or displayed stale tremolo picking");

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::firstKeyswitchNote
               + static_cast<int> (electry::PickStyle::Alternate),
        static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, heldNote, static_cast<juce::uint8> (105)), 0);
    renderBlock (processor, audio, midi);
    renderSeconds (processor, audio, 0.04);
    expect (! processor.getStringVisualState (0).strokeUp,
            "the B0 Alternate fixture did not begin down");

    const auto gestureOn = [&] (int velocity)
    {
        midi.addEvent (juce::MidiMessage::noteOn (
            1, gestureNote, static_cast<juce::uint8> (velocity)), 0);
        renderBlock (processor, audio, midi);
    };
    const auto gestureOff = [&]
    {
        midi.addEvent (juce::MidiMessage::noteOff (1, gestureNote), 0);
        renderBlock (processor, audio, midi);
    };

    gestureOn (96);
    expect (processor.getTremoloGestureForDisplay() == 96
                && processor.getStringVisualState (0).strokeUp,
            "B0 did not immediately repick an already-held string");
    gestureOn (127);
    expect (processor.getTremoloGestureForDisplay() == 127,
            "a repeated B0 Note On did not capture the latest pick force");
    gestureOff();
    expect (processor.getTremoloGestureForDisplay() == 127,
            "one B0 Note Off cancelled another held owner");
    gestureOff();
    gestureOff();
    expect (processor.getTremoloGestureForDisplay() == 0
                && processor.getStringVisualState (0).sounding,
            "balanced/stray B0 Off stuck the wrist or released the fretting key");

    gestureOn (80);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, gestureNote, static_cast<juce::uint8> (0)), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getTremoloGestureForDisplay() == 0,
            "velocity-zero B0 did not act as Note Off");

    processor.keyboardState.noteOn (1, gestureNote, 64.0f / 127.0f);
    renderBlock (processor, audio, midi);
    expect (processor.getTremoloGestureForDisplay() == 64,
            "the on-screen B0 TRM key did not reach the gesture path");
    processor.keyboardState.noteOff (1, gestureNote, 0.0f);
    renderBlock (processor, audio, midi);
    expect (processor.getTremoloGestureForDisplay() == 0,
            "releasing the on-screen B0 TRM key left picking armed");

    gestureOn (90);
    for (const int controller : { 121, 120 })
    {
        midi.addEvent (juce::MidiMessage::controllerEvent (
            1, controller, 0), 0);
        renderBlock (processor, audio, midi);
        expect (processor.getTremoloGestureForDisplay() == 90,
                std::string (controller == 121 ? "CC121" : "CC120")
                    + " released the physically held B0 gesture");
    }
    // CC120 cleared the old fretting state but retained the wrist. A new note
    // is its own first contact and then resumes at the configured 12/s.
    midi.addEvent (juce::MidiMessage::noteOn (
        1, heldNote, static_cast<juce::uint8> (105)), 0);
    renderBlock (processor, audio, midi);
    const bool restartedDown = ! processor.getStringVisualState (0).strokeUp;
    renderSeconds (processor, audio, 0.09);
    expect (restartedDown && processor.getStringVisualState (0).strokeUp,
            "CC120 preserved B0's display but not its running wrist");

    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getTremoloGestureForDisplay() == 0,
            "All Notes Off did not release B0 tremolo picking");

    gestureOn (110);
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    ElectryAudioProcessor restored;
    restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    expect (restored.getTremoloGestureForDisplay() == 0
                && std::abs (parameterValue (
                    restored, electry::parameters::tremoloRate) - 12.0f)
                       < 1.0e-4f,
            "state restore persisted the momentary B0 gesture or lost its rate");

    processor.requestPanic();
    renderBlock (processor, audio, midi);
    expect (processor.getTremoloGestureForDisplay() == 0,
            "Panic left B0 tremolo picking armed");

    // B0 is attack-conditioning state. Its insertion position must not split
    // or double a same-sample chord.
    const auto renderSameBoundary = [] (int gesturePosition)
    {
        ElectryAudioProcessor p;
        p.prepareToPlay (sampleRate, blockSize);
        setParameterValue (p, electry::parameters::sympathetic, 0.0f);
        juce::AudioBuffer<float> block;
        juce::MidiBuffer events;
        const auto addGesture = [&]
        {
            events.addEvent (juce::MidiMessage::noteOn (
                1, electry::ElectryEngine::tremoloGestureNote,
                static_cast<juce::uint8> (100)), 0);
        };
        if (gesturePosition == 0)
            addGesture();
        events.addEvent (juce::MidiMessage::noteOn (
            1, 28, static_cast<juce::uint8> (105)), 0);
        if (gesturePosition == 1)
            addGesture();
        events.addEvent (juce::MidiMessage::noteOn (
            1, 35, static_cast<juce::uint8> (105)), 0);
        if (gesturePosition == 2)
            addGesture();
        renderBlock (p, block, events);
        std::vector<float> result (
            block.getReadPointer (0), block.getReadPointer (0) + blockSize);
        p.releaseResources();
        return result;
    };
    const auto gestureFirst = renderSameBoundary (0);
    const auto gestureMiddle = renderSameBoundary (1);
    const auto gestureLast = renderSameBoundary (2);
    expect (gestureFirst == gestureMiddle && gestureMiddle == gestureLast,
            "same-sample B0 placement changed or doubled a chord attack");

    // This crosses the APVTS boundary rather than setting EngineParameters
    // directly: 20 strokes/s repeats after exactly 2,400 host samples at
    // 48 kHz, then a live 4 -> 20 change keeps the existing wrist phase.
    ElectryAudioProcessor hostRate;
    hostRate.prepareToPlay (sampleRate, blockSize);
    setParameterValue (hostRate, electry::parameters::sympathetic, 0.0f);
    setParameterValue (hostRate, electry::parameters::tremoloRate, 20.0f);
    juce::AudioBuffer<float> rateAudio;
    juce::MidiBuffer rateMidi;
    rateMidi.addEvent (juce::MidiMessage::noteOn (
        1, electry::ElectryEngine::firstKeyswitchNote
               + static_cast<int> (electry::PickStyle::Alternate),
        static_cast<juce::uint8> (127)), 0);
    rateMidi.addEvent (juce::MidiMessage::noteOn (
        1, heldNote, static_cast<juce::uint8> (105)), 0);
    rateMidi.addEvent (juce::MidiMessage::noteOn (
        1, gestureNote, static_cast<juce::uint8> (100)), 0);
    renderBlock (hostRate, rateAudio, rateMidi, 1);
    const auto renderRateSamples = [&] (int samples)
    {
        while (samples > 0)
        {
            const int count = std::min (blockSize, samples);
            renderBlock (hostRate, rateAudio, rateMidi, count);
            samples -= count;
        }
    };
    expect (! hostRate.getStringVisualState (0).strokeUp,
            "same-boundary B0 duplicated the host-rate fixture's first contact");
    renderRateSamples (2399);
    expect (! hostRate.getStringVisualState (0).strokeUp,
            "20 strokes/s arrived before its exact 2,400-sample host interval");
    renderRateSamples (1);
    expect (hostRate.getStringVisualState (0).strokeUp,
            "the APVTS 20 strokes/s value did not reach the picking scheduler");
    setParameterValue (hostRate, electry::parameters::tremoloRate, 4.0f);
    renderRateSamples (9600);
    expect (hostRate.getStringVisualState (0).strokeUp,
            "a live 4 strokes/s value advanced the held wrist too quickly");
    setParameterValue (hostRate, electry::parameters::tremoloRate, 20.0f);
    renderRateSamples (479);
    expect (hostRate.getStringVisualState (0).strokeUp,
            "a live Tremolo Rate change discarded the existing wrist phase");
    renderRateSamples (1);
    expect (! hostRate.getStringVisualState (0).strokeUp,
            "a live Tremolo Rate change did not reach the held B0 gesture");
    hostRate.releaseResources();

    const auto renderReenteredDouble = [] (bool gesture)
    {
        ElectryAudioProcessor p;
        p.prepareToPlay (sampleRate, blockSize);
        setParameterValue (p, electry::parameters::sympathetic, 0.0f);
        juce::AudioBuffer<float> block;
        juce::MidiBuffer events;
        if (gesture)
            events.addEvent (juce::MidiMessage::noteOn (
                1, electry::ElectryEngine::tremoloGestureNote,
                static_cast<juce::uint8> (100)), 0);
        renderBlock (p, block, events); // The second player is dormant.
        setParameterValue (p, electry::parameters::outputMode, 2.0f);
        renderBlock (p, block, events);
        setParameterValue (p, electry::parameters::outputMode, 0.0f);
        renderBlock (p, block, events);
        setParameterValue (p, electry::parameters::outputMode, 2.0f);
        events.addEvent (juce::MidiMessage::noteOn (
            1, electry::ElectryEngine::lowestPlayableNote,
            static_cast<juce::uint8> (105)), 0);
        std::vector<float> right;
        for (int remaining = static_cast<int> (0.11 * sampleRate);
             remaining > 0;)
        {
            const int samples = std::min (blockSize, remaining);
            renderBlock (p, block, events, samples);
            const auto* channel = block.getReadPointer (1);
            right.insert (right.end(), channel, channel + samples);
            remaining -= samples;
        }
        p.releaseResources();
        return right;
    };
    const auto plainReentry = renderReenteredDouble (false);
    const auto tremoloReentry = renderReenteredDouble (true);
    float reentryDifference = 0.0f;
    for (std::size_t i = 0; i < plainReentry.size(); ++i)
        reentryDifference = std::max (
            reentryDifference,
            std::abs (plainReentry[i] - tremoloReentry[i]));
    expect (reentryDifference > 1.0e-6f,
            "B0 held through Mono/Double re-entry did not reach player two");

    ElectryStatusDisplay status;
    status.setStatus (1, 0, true, sampleRate, 0, 0, 64, false);
    expect (status.getStatusText().contains ("TRM 50%")
                && status.getTitle() == status.getStatusText(),
            "the status display did not expose live B0 pick force");
    status.setStatus (1, 0, true, sampleRate, 0, 64, 96, false);
    expect (status.getStatusText().contains ("VIB 50%")
                && status.getStatusText().contains ("TRM 76%")
                && ! status.getStatusText().contains ("kHz"),
            "simultaneous A#0 vibrato and B0 picking hid one live gesture");

    gestureOn (80);
    processor.releaseResources();
    expect (processor.getTremoloGestureForDisplay() == 0,
            "releaseResources left B0 tremolo picking visible");
    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.getTremoloGestureForDisplay() == 0,
            "a later prepare restored stale B0 tremolo picking");
    processor.releaseResources();
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
        int leftOnset = -1;
        int rightOnset = -1;
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
        int leftOnset = -1;
        int rightOnset = -1;
        for (int block = 0; block < 48; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (
                    1, midiNote, (juce::uint8) 102), 0);
            renderBlock (processor, audio, midi);
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const int absoluteSample = block * audio.getNumSamples() + sample;
                if (leftOnset < 0
                    && std::abs (audio.getSample (0, sample)) > 1.0e-9f)
                    leftOnset = absoluteSample;
                if (rightOnset < 0
                    && std::abs (audio.getSample (1, sample)) > 1.0e-9f)
                    rightOnset = absoluteSample;
            }
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
            identical, leftHash, rightHash, leftOnset, rightOnset
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
    expect (doubled.leftOnset == mono.leftOnset
                && doubled.rightOnset > doubled.leftOnset
                && doubled.rightOnset - doubled.leftOnset
                       <= static_cast<int> (std::ceil (0.006 * sampleRate)) + 2,
            "Double did not preserve the primary onset and bound the second "
            "player to 0-6 ms (L " + std::to_string (doubled.leftOnset)
                + ", R " + std::to_string (doubled.rightOnset) + ")");
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
    // Twenty instrument/master knobs plus five FX amount knobs; Amp Voice is a
    // choice strip and therefore deliberately absent from this knob count.
    expect (knobs.size() == 25u, "editor did not expose all 25 knob controls");

    for (std::size_t first = 0; first < knobs.size(); ++first)
    {
        // The editor deliberately tiers its controls by audible impact, so a
        // texture detail is much smaller than a hero control. This is the
        // practical floor for the smallest tier; the relative-size assertions
        // below are what pin the intended hierarchy.
        expect (knobs[first]->getWidth() >= 44 && knobs[first]->getHeight() >= 110,
                "knob fell below the compact control size: "
                    + knobs[first]->getName().toStdString());
        expect (knobs[first]->slider.getTitle()
                    == knobs[first]->slider.getName()
                    && knobs[first]->slider.getTitle().isNotEmpty()
                    && knobs[first]->slider.getWantsKeyboardFocus()
                    && knobs[first]->slider.hasFocusOutline(),
                "knob is not named and visibly keyboard-focusable: "
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
    const auto expectAccessibleChoiceStrip = [&] (const char* componentId)
    {
        auto* strip = findControl (componentId);
        expect (strip != nullptr,
                std::string ("missing choice strip ") + componentId);
        if (strip == nullptr)
            return;

        expect (strip->getTitle().isNotEmpty(),
                std::string (componentId) + " has no accessibility context");
        int buttonCount = 0;
        int selectedCount = 0;
        for (auto* child : strip->getChildren())
        {
            auto* button = dynamic_cast<juce::TextButton*> (child);
            if (button == nullptr)
                continue;

            ++buttonCount;
            selectedCount += button->getToggleState() ? 1 : 0;
            const auto expectedTitle = strip->getTitle() + ": "
                                     + button->getButtonText();
            expect (button->isToggleable()
                        && button->getClickingTogglesState()
                        && button->getRadioGroupId() != 0,
                    std::string (componentId)
                        + " choice is not an exclusive toggle button");
            expect (button->getWantsKeyboardFocus()
                        && button->hasFocusOutline(),
                    std::string (componentId)
                        + " choice is not visibly keyboard-focusable");
            expect (button->getTitle() == expectedTitle,
                    std::string (componentId)
                        + " choice has no contextual accessibility title");
        }
        expect (buttonCount > 0 && selectedCount == 1,
                std::string (componentId)
                    + " does not expose exactly one selected choice");
    };
    for (const auto* componentId : {
             "pickStyleStrip", "playStyleStrip", "playStyleKeyMode",
             electry::parameters::pickupSelector,
             electry::parameters::outputMode,
             electry::parameters::ampModel })
        expectAccessibleChoiceStrip (componentId);

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
#if ELECTRY_MEASURED_BODY_RESPONSE
                && buildControl->slider.getTooltip().contains ("matched walnut/ash")
                && buildControl->slider.getTooltip().contains ("pole")
                && buildControl->slider.getTooltip().contains ("remain neutral")
#else
                && buildControl->slider.getTooltip().contains ("material damping")
#endif
                && buildControl->slider.getTooltip().contains ("scale length")
                && buildControl->slider.getTooltip().contains ("remain independent"),
            "Guitar Build does not explain its coupled path and independent controls: "
                + (buildControl == nullptr
                       ? std::string ("<missing>")
                       : buildControl->slider.getTooltip().toStdString()));
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
        expect (factoryProgramControl->getWantsKeyboardFocus()
                    && factoryProgramControl->hasFocusOutline(),
                "factory-rig selector is not visibly keyboard-focusable");
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

    auto* panicControl = dynamic_cast<juce::TextButton*> (
        findControl ("panic"));
    expect (panicControl != nullptr,
            "editor is missing the PANIC action");
    if (panicControl != nullptr)
    {
        expect (panicControl->getWantsKeyboardFocus()
                    && panicControl->hasFocusOutline(),
                "PANIC is not visibly keyboard-focusable");
        expect (panicControl->getButtonText() == "PANIC"
                    && panicControl->getTooltip()
                           == "Immediately silence all strings",
                "PANIC lost its accessible label or help text");
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
        juce::TextButton* twoXButton = nullptr;
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
            else if (button->getButtonText() == "2X")
                twoXButton = button;
        }
        expect (modeButtons == 3,
                "output controls did not expose Mono, Stereo and 2X");
        for (std::size_t first = 0; first < modeButtonBounds.size(); ++first)
            for (std::size_t second = first + 1;
                 second < modeButtonBounds.size(); ++second)
                expect (! modeButtonBounds[first].intersects (
                            modeButtonBounds[second]),
                        "output-mode buttons overlap");
        expect (monoButton != nullptr && stereoButton != nullptr
                    && twoXButton != nullptr,
                "output-mode buttons lost a full readable label");
        if (monoButton != nullptr && stereoButton != nullptr
            && twoXButton != nullptr)
        {
            setParameterValue (processor, electry::parameters::outputMode, 1.0f);
            expect (! monoButton->getToggleState()
                        && stereoButton->getToggleState()
                        && ! twoXButton->getToggleState(),
                    "host output-mode change did not update its radio choice");
            twoXButton->onClick();
            expect (parameterValue (processor, electry::parameters::outputMode)
                        > 1.5f
                        && ! monoButton->getToggleState()
                        && ! stereoButton->getToggleState()
                        && twoXButton->getToggleState(),
                    "2X editor button did not enable the second engine");
            stereoButton->onClick();
            expect (std::abs (parameterValue (
                               processor, electry::parameters::outputMode) - 1.0f)
                        < 1.0e-5f
                        && ! monoButton->getToggleState()
                        && stereoButton->getToggleState()
                        && ! twoXButton->getToggleState(),
                    "STEREO editor button did not select the stereo field");
            monoButton->onClick();
            expect (parameterValue (
                        processor, electry::parameters::outputMode) < 0.5f
                        && monoButton->getToggleState()
                        && ! stereoButton->getToggleState()
                        && ! twoXButton->getToggleState(),
                    "MONO editor button did not restore the summed DI");
        }
    }

    auto* ampModelControl = findControl (electry::parameters::ampModel);
    const auto* ampControl = findControl (electry::parameters::amp);
    expect (ampModelControl != nullptr && ampControl != nullptr,
            "FX panel is missing its amp-model buttons or Amp control");
    if (ampModelControl != nullptr && ampControl != nullptr)
    {
        expect (! ampModelControl->getBounds().intersects (ampControl->getBounds()),
                "amp-model buttons overlap the Amp knob");
        expect (ampModelControl->getBottom() <= ampControl->getY(),
                "amp-model selector is not placed above the Amp control");

        const std::array<juce::String, 3> expectedLabels {
            "AMERICAN CLEAN", "BRITISH CRUNCH", "MODERN HIGH-GAIN"
        };
        std::array<juce::TextButton*, 3> modelButtons {};
        int modelButtonCount = 0;
        for (auto* child : ampModelControl->getChildren())
        {
            auto* button = dynamic_cast<juce::TextButton*> (child);
            if (button == nullptr)
                continue;
            if (modelButtonCount < static_cast<int> (modelButtons.size()))
                modelButtons[static_cast<std::size_t> (modelButtonCount)] = button;
            expect (modelButtonCount >= static_cast<int> (expectedLabels.size())
                        || button->getButtonText()
                               == expectedLabels[static_cast<std::size_t> (
                                   modelButtonCount)],
                    "amp-model button lost its complete readable label");
            expect (button->getWidth() >= 100 && button->getHeight() >= 22,
                    "amp-model button is too small to operate clearly");
            expect (button->getTooltip().contains ("amplifier")
                        && button->getTooltip().contains ("cabinet"),
                    "amp-model button does not explain the complete rig choice");
            ++modelButtonCount;
        }
        expect (modelButtonCount == 3,
                "amp-model selector did not expose all three voices");
        if (modelButtons[0] != nullptr && modelButtons[1] != nullptr
            && modelButtons[2] != nullptr)
        {
            modelButtons[0]->onClick();
            expect (parameterValue (processor, electry::parameters::ampModel) < 0.5f,
                    "American Clean button did not select its amp model");
            modelButtons[1]->onClick();
            expect (std::abs (parameterValue (
                               processor, electry::parameters::ampModel) - 1.0f)
                        < 1.0e-5f,
                    "British Crunch button did not select its amp model");
            modelButtons[2]->onClick();
            expect (parameterValue (processor, electry::parameters::ampModel) > 1.5f,
                    "Modern High-Gain button did not select its amp model");
        }
    }

    auto* pickupControl = findControl (electry::parameters::pickupSelector);
    expect (pickupControl != nullptr,
            "Core panel is missing the pickup selector");
    if (pickupControl != nullptr)
    {
        int pickupButtons = 0;
        int previousBottom = -1;
        for (auto* child : pickupControl->getChildren())
        {
            if (dynamic_cast<juce::TextButton*> (child) == nullptr)
                continue;
            ++pickupButtons;
            expect (child->getX() == 0
                        && child->getWidth() == pickupControl->getWidth()
                        && child->getHeight() >= 40
                        && child->getY() > previousBottom,
                    "pickup buttons are not a clear vertical stack");
            previousBottom = child->getBottom();
        }
        expect (pickupButtons == 3,
                "pickup selector lost Neck, Both or Bridge");
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
                                     electry::parameters::tremoloRate,
                                     electry::parameters::resonanceDepth })
        expect (effectiveDialSize (performance)
                    >= effectiveDialSize (electry::parameters::artifacts),
                std::string (performance)
                    + " is smaller than an artifact-texture control");

    auto* tremoloRateControl = dynamic_cast<ElectryKnob*> (
        findControl (electry::parameters::tremoloRate));
    expect (tremoloRateControl != nullptr
                && tremoloRateControl->slider.getTooltip().contains ("B0")
                && tremoloRateControl->slider.getTooltip().contains ("8, 12 and 16")
                && tremoloRateControl->slider.getTooltip().containsIgnoreCase (
                    "not transport synced")
                && tremoloRateControl->slider.getTooltip().contains ("180 BPM"),
            "TRM Rate does not explain its gesture and physical rate anchors");

    auto* fretboard = dynamic_cast<ElectryFretboardDisplay*> (
        findControl ("fretboard"));
    expect (fretboard != nullptr, "editor is missing the live fretboard display");
    if (fretboard != nullptr)
    {
        expect (fretboard->getWidth() >= 400 && fretboard->getHeight() >= 60,
                "the fretboard display is too small to read");
        bool interceptsSelf = false;
        bool interceptsChildren = true;
        fretboard->getInterceptsMouseClicks (interceptsSelf, interceptsChildren);
        expect (interceptsSelf && ! interceptsChildren
                    && fretboard->getWantsKeyboardFocus()
                    && fretboard->getMouseClickGrabsKeyboardFocus()
                    && fretboard->hasFocusOutline()
                    && fretboard->onRepick != nullptr,
                "the live fretboard is not mouse- and keyboard-operable");
        expect (fretboard->getTitle().contains ("physical string 8")
                    && fretboard->getTitle().contains ("Up and Down")
                    && fretboard->getTitle().contains ("1 through 8")
                    && fretboard->getTitle().contains ("Space or Return")
                    && fretboard->getTitle().containsIgnoreCase ("repick")
                    && fretboard->getHelpText().contains ("Up and Down")
                    && fretboard->getHelpText().contains ("1 through 8")
                    && fretboard->getHelpText().contains ("Space")
                    && fretboard->getHelpText().contains ("Return")
                    && fretboard->getTooltip().containsIgnoreCase ("click")
                    && fretboard->getTooltip().contains ("E6")
                    && fretboard->getTooltip().contains ("B6")
                    && fretboard->getTooltip().containsIgnoreCase ("velocity"),
                "the live fretboard does not explain its selection and repick controls");

        const auto processorRepick = std::move (fretboard->onRepick);
        int clickedString = -1;
        fretboard->onRepick = [&clickedString] (int stringIndex)
        {
            clickedString = stringIndex;
        };
        const auto clickString = [fretboard] (int stringIndex,
                                              juce::ModifierKeys modifiers)
        {
            const auto position = juce::Point<float> (
                static_cast<float> (fretboard->getWidth()) * 0.5f,
                static_cast<float> (fretboard->getHeight())
                    * electry::visuals::stringRowFraction (
                          stringIndex, electry::ElectryEngine::stringCount,
                          0.085f));
            const auto now = juce::Time::getCurrentTime();
            fretboard->mouseDown (juce::MouseEvent (
                juce::Desktop::getInstance().getMainMouseSource(), position,
                modifiers, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                fretboard, fretboard, now, position, now, 1, false));
        };
        clickString (0, juce::ModifierKeys::leftButtonModifier);
        expect (clickedString == 0
                    && fretboard->getTitle().contains ("physical string 8"),
                "the fretboard's low E1 row did not select physical string 8");
        clickString (electry::ElectryEngine::stringCount - 1,
                     juce::ModifierKeys::leftButtonModifier);
        expect (clickedString == electry::ElectryEngine::stringCount - 1
                    && fretboard->getTitle().contains ("physical string 1"),
                "the fretboard's high E4 row did not select physical string 1");
        clickedString = -1;
        clickString (3, juce::ModifierKeys::rightButtonModifier);
        expect (clickedString < 0,
                "a non-primary fretboard click triggered a string repick");

        expect (fretboard->keyPressed (
                    juce::KeyPress { juce::KeyPress::upKey })
                    && fretboard->keyPressed (
                           juce::KeyPress { juce::KeyPress::returnKey })
                    && clickedString == 6
                    && fretboard->getTitle().contains ("physical string 2"),
                "Up and Return did not select and repick the row above");
        expect (fretboard->keyPressed (
                    juce::KeyPress { juce::KeyPress::downKey })
                    && fretboard->keyPressed (
                           juce::KeyPress { juce::KeyPress::spaceKey })
                    && clickedString == 7
                    && fretboard->getTitle().contains ("physical string 1"),
                "Down and Space did not select and repick the row below");

        clickedString = -1;
        expect (fretboard->keyPressed (juce::KeyPress { '8' })
                    && clickedString < 0
                    && fretboard->getTitle().contains ("physical string 8")
                    && fretboard->keyPressed (
                           juce::KeyPress { juce::KeyPress::spaceKey })
                    && clickedString == 0,
                "number key 8 did not select and repick the lowest string");
        clickedString = -1;
        expect (fretboard->keyPressed (juce::KeyPress { '1' })
                    && clickedString < 0
                    && fretboard->getTitle().contains ("physical string 1")
                    && fretboard->keyPressed (
                           juce::KeyPress { juce::KeyPress::returnKey })
                    && clickedString == 7,
                "number key 1 did not select and repick the highest string");
        clickedString = -1;
        expect (! fretboard->keyPressed (juce::KeyPress { '9' })
                    && clickedString < 0,
                "an unrelated key changed or repicked the fretboard selection");
        expect (fretboard->keyPressed (juce::KeyPress { '8' })
                    && fretboard->getTitle().contains ("physical string 8"),
                "the fretboard did not restore its default selected row");
        fretboard->onRepick = processorRepick;

        for (const auto* knob : knobs)
            expect (! fretboard->getBounds().intersects (knob->getBounds()),
                    "the fretboard display overlaps a control: "
                        + knob->getName().toStdString());
    }

    const auto* pickStrip = findControl ("pickStyleStrip");
    const auto* styleStrip = findControl ("playStyleStrip");
    const auto* styleKeyMode = findControl ("playStyleKeyMode");
    auto* keyboard = findControl ("keyboard");
    const auto* keyboardHint = findControl ("keyboardHint");
    expect (pickStrip != nullptr && styleStrip != nullptr
                && styleKeyMode != nullptr && keyboard != nullptr
                && keyboardHint != nullptr,
            "editor hierarchy components are missing stable IDs");
    if (pickStrip != nullptr && styleStrip != nullptr
        && styleKeyMode != nullptr && keyboard != nullptr
        && keyboardHint != nullptr)
    {
        auto* midiKeyboard =
            dynamic_cast<ElectryKeyboardComponent*> (keyboard);
        const auto* hintLabel = dynamic_cast<const juce::Label*> (keyboardHint);
        expect (midiKeyboard != nullptr
                    && midiKeyboard->getRangeStart()
                        == electry::ElectryEngine::firstKeyswitchNote
                    && midiKeyboard->getRangeEnd()
                        == electry::ElectryEngine::highestPlayableNote,
                "keyboard does not stop at the highest pitched note D6");
        expect (midiKeyboard != nullptr
                    && midiKeyboard->getWhiteNoteText (24).isEmpty(),
                "keyboard labels the silent C1 dead zone as playable");
        expect (keyboard->getWantsKeyboardFocus()
                    && keyboard->hasFocusOutline(),
                "keyboard is not visibly keyboard-focusable");
        expect (keyboard->getTitle().contains ("D6")
                    && keyboard->getTitle().contains ("A#0")
                    && keyboard->getTitle().contains ("B0")
                    && keyboard->getTitle().containsIgnoreCase ("tremolo")
                    && ! keyboard->getTitle().contains ("E6")
                    && ! keyboard->getTitle().contains ("B6"),
                "keyboard title presents out-of-range E6..B6 keys as pitched notes");
        expect (hintLabel != nullptr
                    && hintLabel->getText().contains ("E1..D6")
                    && hintLabel->getText().contains ("A#0")
                    && hintLabel->getText().containsIgnoreCase ("vibrato")
                    && hintLabel->getText().contains ("B0")
                    && hintLabel->getText().containsIgnoreCase ("tremolo")
                    && ! hintLabel->getText().contains ("E6")
                    && ! hintLabel->getText().contains ("B6"),
                "keyboard hint presents out-of-range E6..B6 keys as pitched notes");
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
        expect (keyboard->getHeight() >= 100
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
        bool hasAltButton = false;
        bool hasLongAlternateButton = false;
        for (auto* child : pickStrip->getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*> (child))
            {
                hasAltButton = hasAltButton || button->getButtonText() == "ALT";
                hasLongAlternateButton = hasLongAlternateButton
                                      || button->getButtonText() == "ALTERNATE";
            }
        expect (hasAltButton && ! hasLongAlternateButton,
                "pick stroke did not reclaim space with the ALT label");
        expect (countButtons (styleStrip)
                    == electry::ElectryEngine::playStyleKeyswitchCount,
                "editor did not retain one button per play style");
        bool hasMuteButton = false;
        for (auto* child : styleStrip->getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*> (child))
            {
                expect (button->getWidth() >= 80,
                        "play-style button lost its added horizontal padding");
                hasMuteButton = hasMuteButton
                             || button->getButtonText() == "MUTE";
            }
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

void runRealtimeDeadlineBenchmarkIfRequested()
{
    const auto* requested = std::getenv ("ELECTRY_BENCHMARK_REALTIME");
    if (requested == nullptr || std::strcmp (requested, "1") != 0)
        return;

    struct Scenario
    {
        const char* name;
        float pickupSelector;
        float outputMode;
        electry::PlayStyle playStyle;
        bool allEffectsMaximum;
        bool switchAmplifiers;
    };

    constexpr std::array scenarios {
        Scenario { "default-bridge-mono", 2.0f, 0.0f,
                   electry::PlayStyle::Sustain, false, false },
        Scenario { "both-stereo", 1.0f, 1.0f,
                   electry::PlayStyle::Sustain, false, false },
        Scenario { "double", 2.0f, 2.0f,
                   electry::PlayStyle::Sustain, false, false },
        Scenario { "eight-palm", 1.0f, 1.0f,
                   electry::PlayStyle::PalmMute, false, false },
        Scenario { "modern-all-max", 2.0f, 0.0f,
                   electry::PlayStyle::Sustain, true, false },
        Scenario { "amp-ab-switch", 2.0f, 0.0f,
                   electry::PlayStyle::Sustain, false, true },
    };
    constexpr std::array rates { 48000.0, 96000.0, 384000.0 };
    constexpr std::array frameCounts { 64, 512 };
    constexpr std::array chord { 28, 35, 40, 45, 50, 55, 59, 64 };

    const auto percentile = [] (const std::vector<double>& sorted,
                                double probability)
    {
        const auto rank = static_cast<std::size_t> (
            std::ceil (probability * static_cast<double> (sorted.size())));
        return sorted[std::min (sorted.size() - 1,
                                std::max<std::size_t> (1, rank) - 1)];
    };
    const auto finite = [] (const juce::AudioBuffer<float>& audio)
    {
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        {
            const auto* samples = audio.getReadPointer (channel);
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                if (! std::isfinite (samples[sample]))
                    return false;
        }
        return true;
    };

    std::cout << "BENCH realtime-deadline begin (nearest-rank percentiles; "
                 "host parameter writes excluded)\n";
    const auto benchmarkStart = std::chrono::steady_clock::now();
    for (const double rate : rates)
    {
        for (const int frames : frameCounts)
        {
            ElectryAudioProcessor processor;
            for (const auto& scenario : scenarios)
            {
                if (processor.isEngineReady())
                    processor.releaseResources();
                for (const auto& parameter : expectedParameters)
                    setParameterValue (processor, parameter.id,
                                       parameter.defaultValue);
                setParameterValue (processor, electry::parameters::pickupSelector,
                                   scenario.pickupSelector);
                setParameterValue (processor, electry::parameters::outputMode,
                                   scenario.outputMode);
                if (scenario.allEffectsMaximum)
                {
                    for (const auto* id : { electry::parameters::distortion,
                                            electry::parameters::amp,
                                            electry::parameters::compressor,
                                            electry::parameters::delay,
                                            electry::parameters::room })
                        setParameterValue (processor, id, 1.0f);
                    setParameterValue (processor, electry::parameters::ampModel,
                                       2.0f);
                }
                if (scenario.switchAmplifiers)
                {
                    setParameterValue (processor, electry::parameters::amp, 1.0f);
                    setParameterValue (processor, electry::parameters::ampModel,
                                       0.0f);
                }

                processor.prepareToPlay (rate, frames);
                juce::AudioBuffer<float> audio (2, frames);
                juce::MidiBuffer midi;
                const int warmupBlocks = std::max (
                    32, static_cast<int> (std::ceil (0.1 * rate / frames)));
                const int measuredBlocks = std::max (
                    256, static_cast<int> (std::ceil (0.5 * rate / frames)));
                const int repickBlocks = std::max (
                    1, static_cast<int> (std::lround (0.1 * rate / frames)));
                const auto fillEvents = [&] (int block, bool includeStyle)
                {
                    midi.clear();
                    if (includeStyle)
                    {
                        midi.addEvent (juce::MidiMessage::noteOn (
                            1, electry::ElectryEngine::firstPlayStyleKeyswitchNote
                                   + static_cast<int> (scenario.playStyle),
                            (juce::uint8) 127), 0);
                    }
                    if (block % repickBlocks == 0)
                        for (const int note : chord)
                            midi.addEvent (juce::MidiMessage::noteOn (
                                1, note, (juce::uint8) 115), 0);
                };

                bool outputFinite = true;
                float peak = 0.0f;
                for (int block = 0; block < warmupBlocks; ++block)
                {
                    if (scenario.switchAmplifiers)
                        setParameterValue (processor,
                                           electry::parameters::ampModel,
                                           static_cast<float> (block & 1));
                    fillEvents (block, block == 0);
                    processor.processBlock (audio, midi);
                    outputFinite = outputFinite && finite (audio);
                    peak = std::max (peak, audio.getMagnitude (0, frames));
                }

                std::vector<double> timings;
                timings.reserve (static_cast<std::size_t> (measuredBlocks));
                const double deadline = static_cast<double> (frames) / rate;
                int misses = 0;
                for (int block = 0; block < measuredBlocks; ++block)
                {
                    if (scenario.switchAmplifiers)
                        setParameterValue (processor,
                                           electry::parameters::ampModel,
                                           static_cast<float> (block & 1));
                    fillEvents (block, false);
                    const auto begin = std::chrono::steady_clock::now();
                    processor.processBlock (audio, midi);
                    const double elapsed = std::chrono::duration<double> (
                        std::chrono::steady_clock::now() - begin).count();
                    timings.push_back (elapsed);
                    misses += elapsed > deadline ? 1 : 0;
                    outputFinite = outputFinite && std::isfinite (elapsed)
                                 && finite (audio);
                    peak = std::max (peak, audio.getMagnitude (0, frames));
                }

                std::sort (timings.begin(), timings.end());
                const double maximum = timings.back();
                expect (outputFinite,
                        std::string ("realtime benchmark produced non-finite ")
                            + scenario.name + " output");
                expect (peak > 1.0e-7f,
                        std::string ("realtime benchmark rendered silence for ")
                            + scenario.name);
                expect (maximum < 1.0,
                        std::string ("realtime benchmark exceeded its loose ")
                            + "one-second runaway guard for " + scenario.name);

                const auto milliseconds = [] (double seconds)
                {
                    return 1000.0 * seconds;
                };
                std::cout << std::fixed << std::setprecision (4)
                          << "BENCH rate=" << static_cast<int> (rate)
                          << " frames=" << frames
                          << " scenario=" << scenario.name
                          << " blocks=" << measuredBlocks
                          << " deadline_ms=" << milliseconds (deadline)
                          << " p50_ms=" << milliseconds (
                                 percentile (timings, 0.50))
                          << " p95_ms=" << milliseconds (
                                 percentile (timings, 0.95))
                          << " p99_ms=" << milliseconds (
                                 percentile (timings, 0.99))
                          << " max_ms=" << milliseconds (maximum)
                          << " misses=" << misses << '/' << measuredBlocks
                          << '\n';
            }
            processor.releaseResources();
        }
    }
    std::cout << std::fixed << std::setprecision (3)
              << "BENCH realtime-deadline end wall_s="
              << std::chrono::duration<double> (
                     std::chrono::steady_clock::now() - benchmarkStart).count()
              << '\n';
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
    testSameSampleChordAllocationIsCanonical();
    testKeyswitchContract();
    testPlayStyleHoldContract();
    testSameSampleMuteKeyswitchOrder();
    testMidiControllersAndVoiceLifecycle();
    testSameSamplePalmMutePressureAttack();
    testRepickMidiContract();
    testPitchWheelByteReconstruction();
    testPitchWheelMidiDispatch();
    testMpeRouting();
    testMpeIdlePitchPreservesReleaseTail();
    testMpeFractionalRangeAndLiveMasterTail();
    testMpeSamePitchOwnershipAcrossLayoutChange();
    testMpeOwnershipCapacityIsClosed();
    testMpeOwnershipLifecycleBoundaries();
    testResonanceWheelFeedback();
    testResonanceFeedbackIsBlockSizeInvariant();
    testMidiPressureLeavesChordUnchanged();
    testResetAllControllersDispatch();
    testMutePressureDisplayFeedback();
    testVibratoGestureMidiAndLifecycle();
    testTremoloPickingMidiAndLifecycle();
    testUiArticulationTriggerAndPanic();
    testOutputGainImpact();
    testPerformanceControls();
    testOutputModeAudioField();
    testEditorRendering();
    testPrepareReleaseCycles();
    runRealtimeDeadlineBenchmarkIfRequested();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Electry processor contract test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Electry processor contract tests passed\n";
    return EXIT_SUCCESS;
}
