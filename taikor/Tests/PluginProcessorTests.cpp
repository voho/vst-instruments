#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
// Must stay in step with PluginEditor.cpp's design constants.
constexpr int editorDesignWidth = 1280;
constexpr int editorDesignHeight = 880;
constexpr int editorMinimumWidth = 1024;
constexpr int editorMinimumHeight = 704;
constexpr int editorMaximumWidth = 1472;
constexpr int editorMaximumHeight = 1012;
constexpr double editorAspectRatio =
    static_cast<double> (editorDesignWidth) / static_cast<double> (editorDesignHeight);

int failureCount = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

float parameterValue (const TaikorAudioProcessor& processor, const juce::String& id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, "missing parameter " + id.toStdString());
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (TaikorAudioProcessor& processor, const juce::String& id,
                        float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, "cannot set missing parameter " + id.toStdString());
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

float peakOf (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = std::max (peak, std::abs (buffer.getSample (channel, sample)));
    return peak;
}

bool bufferIsFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;
    return true;
}

// Renders one block containing a single note-on at the start.
float renderNote (TaikorAudioProcessor& processor, int midiNote, float velocity,
                  int blocks = 1)
{
    juce::AudioBuffer<float> buffer { 2, blockSize };
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, velocity), 0);

    float peak = 0.0f;
    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        expect (bufferIsFinite (buffer), "processBlock produced non-finite audio");
        peak = std::max (peak, peakOf (buffer));
        midi.clear();
    }
    return peak;
}

// ---------------------------------------------------------------------------

void testParameterLayoutAndDefaults()
{
    TaikorAudioProcessor processor;

    const auto& hostParameters = processor.getParameters();
    expect (hostParameters.size() == taikor::parameters::parameterCount,
            "the host parameter count does not match the declared layout");

    std::set<std::string> ids;
    for (auto* hostParameter : hostParameters)
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (hostParameter);
        expect (ranged != nullptr, "every parameter must be a ranged parameter");
        if (ranged == nullptr)
            continue;

        ids.insert (ranged->paramID.toStdString());
        expect (ranged->getName (64).isNotEmpty(),
                "parameter " + ranged->paramID.toStdString() + " has no name");

        // Every parameter must round-trip through its own text conversion, or
        // a host that types a value into an automation lane gets a different
        // one back.
        const auto value = ranged->convertFrom0to1 (ranged->getDefaultValue());
        const auto text = ranged->getText (ranged->getDefaultValue(), 64);
        expect (text.isNotEmpty(),
                "parameter " + ranged->paramID.toStdString() + " renders no text");
        juce::ignoreUnused (value);
    }

    expect (ids.size() == static_cast<std::size_t> (taikor::parameters::parameterCount),
            "parameter IDs are not unique");

    namespace pids = taikor::parameters;
    const std::array<std::pair<const char*, float>, 22> expectedDefaults {{
        { pids::headDiameter, 55.0f },   { pids::bodyDepth, 0.5f },
        { pids::tension, 0.55f },        { pids::headMaterial, 0.75f },
        { pids::shellMaterial, 0.8f },   { pids::resonantTension, 0.5f },
        { pids::cavityCoupling, 0.85f }, { pids::headDamping, 0.35f },
        { pids::shellResonance, 0.4f },  { pids::pitch, 0.0f },
        { pids::bachiHardness, 0.7f },   { pids::strikePosition, 0.0f },
        { pids::velocityDepth, 0.75f },  { pids::tensionModulation, 0.4f },
        { pids::strikeNoise, 0.35f },    { pids::humanise, 0.4f },
        { pids::octaveBody, 0.7f },      { pids::micDistance, 16.0f },
        { pids::micSpread, 0.55f },      { pids::stereoWidth, 0.6f },
        { pids::drive, 0.0f },           { pids::output, -10.0f },
    }};

    for (const auto& [id, expected] : expectedDefaults)
        expect (std::abs (parameterValue (processor, id) - expected) < 1.0e-4f,
                std::string ("unexpected default for ") + id);

    // The engine block the processor hands the DSP must reflect the parameters,
    // including the two that are presented in centimetres.
    const auto engineParameters = processor.snapshotEngineParameters();
    expect (std::abs (engineParameters.headDiameter - 0.55f) < 1.0e-4f,
            "head diameter must reach the engine in metres");
    expect (std::abs (engineParameters.micDistance - (16.0f - 3.0f) / (40.0f - 3.0f))
                < 1.0e-3f,
            "mic distance must reach the engine normalised");
    expect (std::abs (engineParameters.outputGain
                      - juce::Decibels::decibelsToGain (-10.0f)) < 1.0e-4f,
            "output must reach the engine as a linear gain");

    // The default drum must be the nagado-daiko the documentation describes.
    const auto measurements = processor.measureDrum (0);
    expect (measurements.loadedFundamentalHz > 70.0f
                && measurements.loadedFundamentalHz < 120.0f,
            "the default drum is not in the nagado-daiko range");
    expect (measurements.breathingModeHz > measurements.loadedFundamentalHz,
            "the cavity must lift the breathing mode above the fundamental");
}

void testBusLayoutAndTail()
{
    TaikorAudioProcessor processor;

    expect (processor.getTailLengthSeconds() >= taikor::maximumTailSeconds - 1.0e-6,
            "the reported tail is shorter than the engine's longest");
    expect (processor.acceptsMidi() && ! processor.producesMidi()
                && ! processor.isMidiEffect(),
            "the plug-in must present itself as a MIDI instrument");
    expect (processor.hasEditor(), "the plug-in must offer an editor");

    using Layout = juce::AudioProcessor::BusesLayout;
    Layout stereo;
    stereo.outputBuses.add (juce::AudioChannelSet::stereo());
    expect (processor.checkBusesLayoutSupported (stereo),
            "a stereo output must be supported");

    Layout mono;
    mono.outputBuses.add (juce::AudioChannelSet::mono());
    expect (! processor.checkBusesLayoutSupported (mono),
            "a mono output must be refused: the two channels are two microphones");
}

void testNoteMappingAndRendering()
{
    TaikorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    // Within an octave, every note is a different stroke.
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
    {
        const auto note = taikor::referenceNote + pitchClass;
        const auto peak = renderNote (processor, note, 0.9f, 6);
        expect (peak > 1.0e-4f,
                "note " + std::to_string (note) + " produced no audio");
        processor.requestPanic();
        renderNote (processor, taikor::referenceNote, 0.0f, 1);
    }

    // Notes outside the playable range stay silent.
    for (const int note : { 0, taikor::lowestPlayableNote - 1,
                            taikor::highestPlayableNote + 1, 127 })
    {
        processor.requestPanic();
        juce::AudioBuffer<float> flush { 2, blockSize };
        juce::MidiBuffer empty;
        flush.clear();
        processor.processBlock (flush, empty);

        const auto peak = renderNote (processor, note, 1.0f, 4);
        expect (peak < 1.0e-6f,
                "note " + std::to_string (note)
                    + " is outside the playable range and must be silent");
    }

    // Every trigger counter must move only for its own stroke.
    processor.requestPanic();
    const auto before = processor.getTriggerCounter (taikor::Articulation::Ka);
    renderNote (processor, taikor::midiNoteFor (taikor::Articulation::Ka, 0), 0.8f, 2);
    expect (processor.getTriggerCounter (taikor::Articulation::Ka) > before,
            "the trigger counter must move when its stroke sounds");

    processor.releaseResources();
}

// Higher octave, higher drum: the instrument's central promise, checked through
// the plug-in rather than through the engine.
void testOctavesRaisePitchThroughThePlugin()
{
    TaikorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    float previous = 0.0f;
    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto measurements = processor.measureDrum (octave);
        expect (measurements.loadedFundamentalHz > previous,
                "octave " + std::to_string (octave) + " did not raise the pitch");
        previous = measurements.loadedFundamentalHz;
    }

    // And every playable note must actually sound.
    for (int note = taikor::lowestPlayableNote; note <= taikor::highestPlayableNote;
         note += 7)
    {
        processor.requestPanic();
        const auto peak = renderNote (processor, note, 0.85f, 6);
        expect (peak > 1.0e-4f,
                "playable note " + std::to_string (note) + " produced no audio");
    }

    processor.releaseResources();
}

void testControllersAndPitchBend()
{
    TaikorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    const auto renderTail = [&processor] (const juce::MidiBuffer& controls)
    {
        juce::AudioBuffer<float> buffer { 2, blockSize };
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (
                           1, taikor::midiNoteFor (taikor::Articulation::Don, -1), 1.0f),
                       0);
        buffer.clear();
        processor.processBlock (buffer, midi);

        // Let the stroke establish itself.
        for (int block = 0; block < 6; ++block)
        {
            juce::MidiBuffer empty;
            buffer.clear();
            processor.processBlock (buffer, empty);
        }

        auto applied = controls;
        buffer.clear();
        processor.processBlock (buffer, applied);

        double energy = 0.0;
        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer empty;
            buffer.clear();
            processor.processBlock (buffer, empty);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < blockSize; ++sample)
                {
                    const auto value = buffer.getSample (channel, sample);
                    energy += static_cast<double> (value) * value;
                }
        }
        return energy;
    };

    juce::MidiBuffer nothing;
    processor.requestPanic();
    const auto openEnergy = renderTail (nothing);

    juce::MidiBuffer handDown;
    handDown.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
    processor.requestPanic();
    const auto dampedEnergy = renderTail (handDown);

    expect (dampedEnergy < openEnergy * 0.7,
            "CC1 must damp the ringing head");

    // Release the hand again so it cannot leak into later checks.
    juce::MidiBuffer handUp;
    handUp.addEvent (juce::MidiMessage::controllerEvent (1, 1, 0), 0);
    juce::AudioBuffer<float> buffer { 2, blockSize };
    buffer.clear();
    processor.processBlock (buffer, handUp);

    // All-sounds-off and all-notes-off must both silence the drum.
    for (const int controller : { 120, 123 })
    {
        processor.requestPanic();
        renderNote (processor, taikor::referenceNote, 1.0f, 4);

        juce::MidiBuffer stop;
        stop.addEvent (juce::MidiMessage::controllerEvent (1, controller, 0), 0);
        buffer.clear();
        processor.processBlock (buffer, stop);

        juce::MidiBuffer empty;
        buffer.clear();
        processor.processBlock (buffer, empty);
        expect (peakOf (buffer) < 1.0e-6f,
                "controller " + std::to_string (controller) + " must silence the drum");
    }

    // The wheel presses the head, so it must raise the pitch.
    processor.requestPanic();
    juce::MidiBuffer bend;
    bend.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
    buffer.clear();
    processor.processBlock (buffer, bend);
    for (int block = 0; block < 40; ++block)
    {
        juce::MidiBuffer empty;
        buffer.clear();
        processor.processBlock (buffer, empty);
    }
    expect (bufferIsFinite (buffer), "a pitch bend produced non-finite audio");

    processor.releaseResources();
}

void testParametersReachTheEngine()
{
    TaikorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    namespace pids = taikor::parameters;

    const auto fundamentalNow = [&processor]
    {
        return processor.measureDrum (0).loadedFundamentalHz;
    };

    const auto reference = fundamentalNow();

    setParameterValue (processor, pids::tension, 0.95f);
    expect (fundamentalNow() > reference * 1.15f,
            "the tension control must reach the engine");
    setParameterValue (processor, pids::tension, 0.55f);

    setParameterValue (processor, pids::headDiameter, 100.0f);
    expect (fundamentalNow() < reference * 0.75f,
            "the head diameter control must reach the engine");
    setParameterValue (processor, pids::headDiameter, 55.0f);

    setParameterValue (processor, pids::pitch, 12.0f);
    expect (std::abs (fundamentalNow() / reference - 2.0f) < 0.10f,
            "twelve semitones of pitch must roughly double the sounding pitch");
    setParameterValue (processor, pids::pitch, 0.0f);

    // Output must scale what the host receives.
    setParameterValue (processor, pids::output, 0.0f);
    processor.requestPanic();
    const auto loud = renderNote (processor, taikor::referenceNote, 0.9f, 8);
    setParameterValue (processor, pids::output, -12.0f);
    processor.requestPanic();
    const auto quiet = renderNote (processor, taikor::referenceNote, 0.9f, 8);
    expect (quiet < loud * 0.6f, "the output control must scale the rendered audio");

    processor.releaseResources();
}

void testStateRoundTrip()
{
    TaikorAudioProcessor processor;
    namespace pids = taikor::parameters;

    setParameterValue (processor, pids::tension, 0.91f);
    setParameterValue (processor, pids::headDiameter, 88.5f);
    setParameterValue (processor, pids::micSpread, 0.13f);
    setParameterValue (processor, pids::output, -3.5f);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    TaikorAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    expect (std::abs (parameterValue (restored, pids::tension) - 0.91f) < 1.0e-3f,
            "tension did not survive a state round trip");
    expect (std::abs (parameterValue (restored, pids::headDiameter) - 88.5f) < 1.0e-2f,
            "head diameter did not survive a state round trip");
    expect (std::abs (parameterValue (restored, pids::micSpread) - 0.13f) < 1.0e-3f,
            "mic spread did not survive a state round trip");
    expect (std::abs (parameterValue (restored, pids::output) - (-3.5f)) < 1.0e-2f,
            "output did not survive a state round trip");

    // A stored tree that predates a control must restore that control to its
    // default rather than to whatever the instance happened to be holding.
    TaikorAudioProcessor partial;
    setParameterValue (partial, pids::humanise, 0.99f);

    juce::ValueTree trimmed { partial.parameters.state.getType() };
    juce::ValueTree keep { "PARAM" };
    keep.setProperty ("id", pids::tension, nullptr);
    keep.setProperty ("value", 0.22f, nullptr);
    trimmed.appendChild (keep, nullptr);

    juce::MemoryBlock trimmedState;
    if (const auto xml = trimmed.createXml())
        partial.copyXmlToBinary (*xml, trimmedState);

    partial.setStateInformation (trimmedState.getData(),
                                 static_cast<int> (trimmedState.getSize()));
    expect (std::abs (parameterValue (partial, pids::tension) - 0.22f) < 1.0e-3f,
            "a stored parameter must be restored");
    expect (std::abs (parameterValue (partial, pids::humanise) - 0.4f) < 1.0e-3f,
            "a parameter missing from the stored tree must return to its default");

    // Garbage must be refused rather than crash.
    const char rubbish[] = "not a Taikor session";
    processor.setStateInformation (rubbish, static_cast<int> (sizeof rubbish));
    expect (std::abs (parameterValue (processor, pids::tension) - 0.91f) < 1.0e-3f,
            "invalid state must be ignored rather than applied");
}

void testUiQueueAndLifecycle()
{
    TaikorAudioProcessor processor;

    // Auditioning before prepareToPlay must be dropped rather than queued.
    processor.triggerFromUi (taikor::Articulation::Don, 0, 0.9f);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer { 2, blockSize };
    juce::MidiBuffer empty;
    buffer.clear();
    processor.processBlock (buffer, empty);
    expect (peakOf (buffer) < 1.0e-6f,
            "an audition queued before prepareToPlay must not sound afterwards");

    processor.triggerFromUi (taikor::Articulation::Don, 0, 0.9f);
    float peak = 0.0f;
    for (int block = 0; block < 6; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, empty);
        peak = std::max (peak, peakOf (buffer));
    }
    expect (peak > 1.0e-4f, "an audition from the editor must sound");

    // Overrunning the queue must drop events rather than block or corrupt.
    for (int index = 0; index < 4096; ++index)
        processor.triggerFromUi (
            static_cast<taikor::Articulation> (index % taikor::articulationCount),
            (index % 6) - 2, 0.7f);

    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, empty);
        expect (bufferIsFinite (buffer), "an overrun UI queue produced unsafe audio");
    }

    expect (processor.getActiveVoiceCount() <= 16,
            "the plug-in must not exceed the engine's voice pool");

    processor.releaseResources();
    expect (processor.getActiveVoiceCount() == 0,
            "releaseResources must free every voice");
}

void testEditorRendering()
{
    TaikorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    std::unique_ptr<juce::AudioProcessorEditor> editor { processor.createEditor() };
    expect (editor != nullptr, "the plug-in must create an editor");
    if (editor == nullptr)
        return;

    expect (editor->getWidth() == editorDesignWidth
                && editor->getHeight() == editorDesignHeight,
            "the editor did not open at its design size");

    if (auto* constrainer = editor->getConstrainer())
    {
        expect (constrainer->getMinimumWidth() == editorMinimumWidth
                    && constrainer->getMinimumHeight() == editorMinimumHeight,
                "the editor resize minimum does not protect the control layout");
        expect (constrainer->getMaximumWidth() == editorMaximumWidth
                    && constrainer->getMaximumHeight() == editorMaximumHeight,
                "the editor resize maximum does not match the layout");
        expect (std::abs (constrainer->getFixedAspectRatio() - editorAspectRatio)
                    < 1.0e-9,
                "the editor no longer preserves its design aspect ratio");

        const auto limits = juce::Rectangle<int> (-10000, -10000, 20000, 20000);
        for (const auto requested : std::array { juce::Point<int> { 420, 260 },
                                                 juce::Point<int> { 1900, 1200 } })
        {
            auto constrained = juce::Rectangle<int> (0, 0, requested.x, requested.y);
            constrainer->checkBounds (constrained, editor->getBounds(), limits, false,
                                      false, true, true);
            expect (constrained.getWidth() >= editorMinimumWidth
                        && constrained.getWidth() <= editorMaximumWidth
                        && constrained.getHeight() >= editorMinimumHeight
                        && constrained.getHeight() <= editorMaximumHeight,
                    "a user resize escaped the configured editor limits");
            expect (std::abs (static_cast<double> (constrained.getWidth())
                                  / static_cast<double> (constrained.getHeight())
                              - editorAspectRatio) < 0.002,
                    "a user resize did not preserve the editor aspect ratio");
        }
    }

    // Drive the editor's timer path so the head display and meters are painted
    // with live values rather than their initial ones.
    processor.triggerFromUi (taikor::Articulation::Ka, 1, 0.95f);
    juce::AudioBuffer<float> buffer { 2, blockSize };
    juce::MidiBuffer empty;
    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, empty);
    }

    juce::Image snapshot { juce::Image::ARGB, editor->getWidth(), editor->getHeight(),
                           true };
    {
        juce::Graphics graphics { snapshot };
        editor->paintEntireComponent (graphics, true);
    }

    std::set<juce::uint32> sampledColours;
    int opaqueSamples = 0;
    int sampledPixels = 0;
    for (int y = 4; y < snapshot.getHeight(); y += 8)
    {
        for (int x = 4; x < snapshot.getWidth(); x += 8)
        {
            const auto pixel = snapshot.getPixelAt (x, y);
            sampledColours.insert (pixel.getARGB());
            opaqueSamples += pixel.getAlpha() >= 250 ? 1 : 0;
            ++sampledPixels;
        }
    }

    expect (sampledPixels > 0 && opaqueSamples == sampledPixels,
            "the opaque editor left transparent pixels in its rendered surface");
    expect (sampledColours.size() > 256u,
            "the editor snapshot lacks the panel, head display and control detail");

    // The layout must survive both extremes of the resize range.
    for (const auto size : std::array {
             juce::Point<int> { editorMinimumWidth, editorMinimumHeight },
             juce::Point<int> { editorMaximumWidth, editorMaximumHeight } })
    {
        editor->setSize (size.x, size.y);
        juce::Image resized { juce::Image::ARGB, editor->getWidth(),
                              editor->getHeight(), true };
        juce::Graphics resizedGraphics { resized };
        editor->paintEntireComponent (resizedGraphics, true);

        std::set<juce::uint32> resizeColours;
        bool resizeOpaque = true;
        for (int y = 5; y < resized.getHeight(); y += 13)
        {
            for (int x = 5; x < resized.getWidth(); x += 13)
            {
                const auto pixel = resized.getPixelAt (x, y);
                resizeColours.insert (pixel.getARGB());
                resizeOpaque = resizeOpaque && pixel.getAlpha() >= 250;
            }
        }
        expect (resizeOpaque, "a resized editor left transparent pixels");
        expect (resizeColours.size() > 64u, "a resized editor lost its visual detail");
    }

    editor->setSize (editorDesignWidth, editorDesignHeight);

    // The nightly workflow asks for the committed screenshot through this
    // variable, so the images in the documentation track the real editor.
    const auto snapshotPath =
        juce::SystemStats::getEnvironmentVariable ("TAIKOR_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
    {
        juce::Image committed { juce::Image::ARGB, editor->getWidth(),
                                editor->getHeight(), true };
        {
            juce::Graphics graphics { committed };
            editor->paintEntireComponent (graphics, true);
        }

        // The directory is tracked in the repository, but a local build may
        // point this anywhere, so make sure the parent exists first.
        const juce::File snapshotFile { snapshotPath };
        snapshotFile.getParentDirectory().createDirectory();

        juce::FileOutputStream output { snapshotFile };
        juce::PNGImageFormat png;
        const bool preparedOutput =
            output.openedOk() && output.setPosition (0) && output.truncate();
        const bool wroteSnapshot =
            preparedOutput && png.writeImageToStream (committed, output);
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
    testNoteMappingAndRendering();
    testOctavesRaisePitchThroughThePlugin();
    testControllersAndPitchBend();
    testParametersReachTheEngine();
    testStateRoundTrip();
    testUiQueueAndLifecycle();
    testEditorRendering();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Taikor processor contract test(s) failed\n";
        return 1;
    }

    std::cout << "All Taikor processor contract tests passed\n";
    return 0;
}
