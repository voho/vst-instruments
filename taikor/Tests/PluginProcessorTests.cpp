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

// Renders a stroke and returns the frequency of its strongest low partial, by
// the Goertzel recurrence over a window that starts after the attack and stops
// before the head has emptied. Used to see where the head is actually tuned,
// which is the only way to observe the pitch wheel from outside the processor.
//
// Both bounds matter. The fundamental is the one mode of a head that moves
// enough air to radiate properly, so it is also the one that goes first, and
// past a third of a second the partials still sounding in this band are the
// ones that could not get a sound out - which is not where the drum is tuned.
// The caller is expected to have stilled the strike as well; see the wheel
// check below for why.
//
// The search band is deliberately narrow. Twenty-four to forty-eight hertz is
// where the default drum's lower axisymmetric branch sits, and it is the only
// thing in there whether the wheel is up or down - the branch above it is at
// sixty-one and does not reach forty-eight even bent two semitones. A wide band
// does not measure a pitch at all: it finds whichever partial is loudest, and
// which one that is changes with the tuning, because bending the head moves its
// modes out of the mounting loss while the shell's do not move at all. Sweeping
// thirty-five to three hundred and twenty put the unbent drum on one partial
// and the bent drum on another, and reported the wheel as having lowered it.
constexpr double lowestSounded = 24.0;
constexpr double highestSounded = 48.0;

double dominantLowPartial (TaikorAudioProcessor& processor, int midiNote,
                           const juce::MidiBuffer& before)
{
    juce::AudioBuffer<float> buffer { 2, blockSize };

    {
        auto controls = before;
        buffer.clear();
        processor.processBlock (buffer, controls);
    }

    // The wheel presses the head rather than transposing it, so the tension
    // glides to where it was sent. A stroke played in the same block as the
    // message is played during that glide and smears across it; these let it
    // arrive first, which is what "the pitch of a stroke played afterwards"
    // means.
    for (int block = 0; block < 8; ++block)
    {
        juce::MidiBuffer settling;
        buffer.clear();
        processor.processBlock (buffer, settling);
    }

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, 1.0f), 0);

    std::vector<float> samples;
    for (int block = 0; block < 90; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();
        for (int sample = 0; sample < blockSize; ++sample)
            samples.push_back (0.5f * (buffer.getSample (0, sample)
                                       + buffer.getSample (1, sample)));
    }

    // Past the attack, so the measurement is the ringing head rather than the
    // broadband contact, and stopping while the head is still sounding.
    const std::size_t first = std::min<std::size_t> (samples.size(), 2400);
    const std::size_t last = std::min<std::size_t> (samples.size(), 26400);

    double best = -1.0;
    double bestFrequency = 0.0;
    for (double frequency = lowestSounded; frequency < highestSounded;
         frequency += 0.05)
    {
        const double omega = 2.0 * 3.14159265358979 * frequency / sampleRate;
        const double coefficient = 2.0 * std::cos (omega);
        double s1 = 0.0;
        double s2 = 0.0;
        for (std::size_t index = first; index < last; ++index)
        {
            const double s0 = samples[index] + coefficient * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double magnitude = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
        if (magnitude > best)
        {
            best = magnitude;
            bestFrequency = frequency;
        }
    }
    return bestFrequency;
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
        { pids::headDiameter, 150.0f },  { pids::bodyDepth, 0.5f },
        { pids::tension, 0.62f },        { pids::headMaterial, 0.75f },
        { pids::shellMaterial, 0.8f },   { pids::resonantTension, 0.5f },
        { pids::cavityCoupling, 0.85f }, { pids::headDamping, 0.50f },
        { pids::shellResonance, 0.4f },  { pids::pitch, 0.0f },
        { pids::bachiHardness, 0.7f },   { pids::strikePosition, 0.0f },
        { pids::velocityDepth, 0.75f },  { pids::tensionModulation, 0.4f },
        { pids::strikeNoise, 0.35f },    { pids::humanise, 0.4f },
        { pids::octaveBody, 1.0f },      { pids::micDistance, 16.0f },
        { pids::micSpread, 0.55f },      { pids::stereoWidth, 0.5f },
        { pids::drive, 0.0f },           { pids::output, -22.5f },
    }};

    for (const auto& [id, expected] : expectedDefaults)
        expect (std::abs (parameterValue (processor, id) - expected) < 1.0e-4f,
                std::string ("unexpected default for ") + id);

    // The engine block the processor hands the DSP must reflect the parameters,
    // including the two that are presented in centimetres.
    const auto engineParameters = processor.snapshotEngineParameters();
    expect (std::abs (engineParameters.headDiameter - 1.50f) < 1.0e-4f,
            "head diameter must reach the engine in metres");
    expect (std::abs (engineParameters.micDistance - (16.0f - 3.0f) / (40.0f - 3.0f))
                < 1.0e-3f,
            "mic distance must reach the engine normalised");
    expect (std::abs (engineParameters.outputGain
                      - juce::Decibels::decibelsToGain (-22.5f)) < 1.0e-4f,
            "output must reach the engine as a linear gain");

    // The default drum must be the o-daiko the documentation describes. This
    // is the plug-in layer's half of the same statement the DSP suite makes:
    // the instrument opens on its heaviest voice rather than an octave above
    // it, because that is what a taiko is for. In the pitch the drum is heard
    // at, which on a drum this size is not its lowest mode - see soundingMode.
    const auto measurements = processor.measureDrum (0);
    expect (measurements.soundingHz > 40.0f && measurements.soundingHz < 65.0f,
            "the default drum is not in the o-daiko range");
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

    // Within an octave, every pitch class that carries a stroke is a different
    // stroke, and the ones past the last stroke carry nothing. The octave stays
    // twelve semitones because that is what chooses the drum and it has to line
    // up with the keyboard, so the vocabulary being four strokes long leaves the
    // top eight keys empty on purpose - and empty has to mean silent, not a
    // pitch class cast into an enum that does not have it.
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
    {
        const auto note = taikor::referenceNote + pitchClass;
        const bool carriesStroke =
            pitchClass < static_cast<int> (taikor::articulationCount);
        const auto peak = renderNote (processor, note, 0.9f, 6);
        if (carriesStroke)
            expect (peak > 1.0e-4f,
                    "note " + std::to_string (note) + " produced no audio");
        else
            expect (peak < 1.0e-6f,
                    "note " + std::to_string (note)
                        + " carries no stroke but sounded anyway");
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

// Higher octave, higher drum - and a different drum, not the same one rescaled.
// The instrument's central promise, checked through the plug-in rather than
// through the engine.
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

        // And a different drum, not the same one rescaled: the plug-in's half
        // of the DSP suite's clause. Each octave must resolve to the instrument
        // the engine's own table names, to within the tuning the keyboard asks
        // of it.
        const auto& drum = taikor::getDrumDescription (octave);
        expect (std::abs (2.0f * measurements.radiusMetres - drum.headDiameterMetres)
                    < drum.headDiameterMetres * 0.02f,
                "octave " + std::to_string (octave) + " is not the "
                    + std::string (drum.displayName.data(), drum.displayName.size())
                    + " it is labelled as");
    }

    // Every note that carries a stroke must sound, and every note that does not
    // must stay silent. The octave is twelve semitones because that is what
    // picks the drum, and there are four strokes in it, so the top eight keys of
    // each octave are deliberately empty - a gap is only a design if nothing
    // creeps into it, and casting a pitch class straight to the enum would have
    // let all eight play a Don.
    for (int note = taikor::lowestPlayableNote; note <= taikor::highestPlayableNote;
         ++note)
    {
        const auto mapped = taikor::articulationForMidiNote (note);
        processor.requestPanic();
        const auto peak = renderNote (processor, note, 0.85f, 6);
        if (mapped.has_value())
            expect (peak > 1.0e-4f,
                    "playable note " + std::to_string (note) + " produced no audio");
        else
            expect (peak < 1.0e-6f,
                    "note " + std::to_string (note)
                        + " carries no stroke but sounded anyway");
    }

    processor.releaseResources();
}

void testControllersAndPitchBend()
{
    TaikorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    // Every check here compares the energy of one stroke against the energy of
    // the same stroke played again with a controller moved, so the one thing
    // that must not vary between them is the stroke. Humanise exists to make
    // repeated strokes differ, and on a drum whose tail runs for seconds that
    // difference compounds across a four-hundred-millisecond measurement window
    // until it swamps the gesture being tested.
    setParameterValue (processor, taikor::parameters::humanise, 0.0f);

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

    // Reset All Controllers must release both performable gestures. A host that
    // resets its controller state and then plays would otherwise inherit a hand
    // still on the head and a wheel still off centre.
    {
        juce::MidiBuffer handDownThenReset;
        handDownThenReset.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
        handDownThenReset.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 1);
        processor.requestPanic();
        const auto releasedEnergy = renderTail (handDownThenReset);

        expect (releasedEnergy > openEnergy * 0.7,
                "CC121 must take the hand back off the head");

        // Non-vacuity: the same buffer without the reset must still damp, or
        // the check above would pass on a processor that ignored CC1 entirely.
        juce::MidiBuffer handDownOnly;
        handDownOnly.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
        processor.requestPanic();
        expect (renderTail (handDownOnly) < openEnergy * 0.7,
                "the CC121 check is vacuous unless CC1 still damps");

        juce::MidiBuffer release;
        release.addEvent (juce::MidiMessage::controllerEvent (1, 1, 0), 0);
        juce::AudioBuffer<float> scratch { 2, blockSize };
        scratch.clear();
        processor.processBlock (scratch, release);
    }

    // The same for the wheel, measured where it actually shows: the pitch of a
    // stroke played afterwards.
    {
        const auto note = taikor::midiNoteFor (taikor::Articulation::Don, 0);

        // Three readings taken from one processor, so all three have to be
        // readings of the same drum. Two of the shipping defaults stop them
        // being that.
        //
        // Humanising walks the strike around the head, and it walks it a
        // different way on every stroke - which is the point of it, and which
        // means consecutive readings differ by whatever that stroke happened to
        // wake. And a full Don lands a hand's width in from the middle, where
        // it drives the modes with a circumferential order; the first of those
        // sits within a hertz of the breathing branch, so the strongest partial
        // in this band becomes whichever member of that cluster won this time.
        // Between them the third reading came back thirty-nine hertz from the
        // first with nothing having changed but the stroke.
        //
        // Struck dead centre with the hand still, every mode of circumferential
        // order m has J_m(0) = 0 and the axisymmetric family is the whole of the
        // drum, so the strongest partial is a property of the tuning and the
        // wheel is the only thing that can move it.
        const auto restoreHumanise = parameterValue (processor, taikor::parameters::humanise);
        const auto restorePosition = parameterValue (processor, taikor::parameters::strikePosition);
        setParameterValue (processor, taikor::parameters::humanise, 0.0f);
        setParameterValue (processor, taikor::parameters::strikePosition, -1.0f);

        juce::MidiBuffer nothingAtAll;
        processor.requestPanic();
        const auto centred = dominantLowPartial (processor, note, nothingAtAll);

        juce::MidiBuffer wheelUp;
        wheelUp.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
        processor.requestPanic();
        const auto bent = dominantLowPartial (processor, note, wheelUp);

        juce::MidiBuffer wheelUpThenReset;
        wheelUpThenReset.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
        wheelUpThenReset.addEvent (juce::MidiMessage::controllerEvent (1, 121, 0), 1);
        processor.requestPanic();
        const auto reset = dominantLowPartial (processor, note, wheelUpThenReset);

        setParameterValue (processor, taikor::parameters::humanise, restoreHumanise);
        setParameterValue (processor, taikor::parameters::strikePosition, restorePosition);

        expect (bent > centred * 1.02,
                "the wheel must raise the pitch, or the reset check is vacuous");
        expect (std::abs (reset - centred) < 0.5,
                "CC121 must return the wheel to centre");

        juce::MidiBuffer recentre;
        recentre.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0);
        juce::AudioBuffer<float> scratch { 2, blockSize };
        scratch.clear();
        processor.processBlock (scratch, recentre);
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
    setParameterValue (processor, pids::tension, 0.62f);

    setParameterValue (processor, pids::headDiameter, 60.0f);
    expect (fundamentalNow() > reference * 1.6f,
            "the head diameter control must reach the engine");
    setParameterValue (processor, pids::headDiameter, 150.0f);

    setParameterValue (processor, pids::pitch, 12.0f);
    expect (std::abs (fundamentalNow() / reference - 2.0f) < 0.10f,
            "twelve semitones of pitch must roughly double the sounding pitch");
    setParameterValue (processor, pids::pitch, 0.0f);

    // Output must scale what the host receives. The gain is smoothed so that
    // automating it cannot click, which means a stroke struck in the same
    // block as the change still has its attack rendered at the previous
    // setting - the peak this measures happens inside the first millisecond.
    // Let the smoother arrive before striking, or the test compares two
    // strokes that were both rendered at whatever the gain was before.
    const auto peakAtOutput = [&processor] (float decibels)
    {
        setParameterValue (processor, taikor::parameters::output, decibels);
        processor.requestPanic();

        juce::AudioBuffer<float> settle { 2, blockSize };
        juce::MidiBuffer empty;
        for (int block = 0; block < 8; ++block)
        {
            settle.clear();
            processor.processBlock (settle, empty);
        }

        return renderNote (processor, taikor::referenceNote, 0.9f, 8);
    };

    // Both settings inside the control's linear region. A taiko has an enormous
    // crest factor and this one models all of it, so the top of the Output
    // range is deliberately far into the safety limiter - comparing a point up
    // there against one below it would be measuring the limiter. The upper
    // point moved down with the factory Output when the reference drum went to
    // five shaku: the same reference stroke that peaked clear of the limiter at
    // -12 dB on a three-shaku drum reaches 0.972 on this one, so this is the
    // same headroom measured on a louder instrument. At -14.5 it reads 0.729.
    const auto loud = peakAtOutput (-14.5f);
    const auto quiet = peakAtOutput (-24.0f);
    expect (loud < 0.95f, "the louder of the two output checks must not limit");
    expect (quiet < loud * 0.6f, "the output control must scale the rendered audio");
    expect (loud > 1.0e-4f, "the output check rendered no audio to compare");

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

// A pad's spoken description has to follow the octave strip. It used to be
// composed once, when the accessibility handler was built, so a reader was told
// whichever note the pad played at the moment it first asked - and after that
// the announcement and the sound disagreed for three of the four drums.
void testPadAnnouncesTheNoteItCurrentlyPlays()
{
    TaikorPad pad { taikor::Articulation::Don };

    pad.setOctaveOffset (0);
    const auto atReference = pad.accessibleHelpText();
    expect (atReference.contains (juce::String (
                taikor::midiNoteFor (taikor::Articulation::Don, 0))),
            "a pad must name the MIDI note it triggers");

    for (int octave = taikor::lowestOctaveOffset;
         octave <= taikor::highestOctaveOffset; ++octave)
    {
        pad.setOctaveOffset (octave);
        const auto spoken = pad.accessibleHelpText();
        const auto note = taikor::midiNoteFor (taikor::Articulation::Don, octave);

        expect (spoken.contains (juce::String (note)),
                "a pad must announce the note it plays now, not the one it was "
                "built with");
        expect (octave == 0 || spoken != atReference,
                "a pad's description must change when the octave strip moves it");
    }
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
    testPadAnnouncesTheNoteItCurrentlyPlays();
    testEditorRendering();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Taikor processor contract test(s) failed\n";
        return 1;
    }

    std::cout << "All Taikor processor contract tests passed\n";
    return 0;
}
