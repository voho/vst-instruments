// Plug-in layer suite: the host-facing contract — parameter layout
// stability, MIDI reaching the engine, panic and all-notes-off semantics,
// state round-trips, and the editor rendering (which also produces the
// committed documentation screenshot when GHOSTAR_EDITOR_SNAPSHOT is set).

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace ghostar
{
// Drive the real sparse kernels and ring schedule without involving an
// oscillator, envelope or gain calibration. GhostarEngine already grants
// this test seam access for component-level checks.
struct GhostarCircuitTestAccess
{
    struct ExternalPitchInput
    {
        bool jackInserted;
        double sourceVolts;
    };

    struct PedalInput
    {
        bool jackInserted;
        double resistanceKOhm;
    };

    static ExternalPitchInput externalPitchInput(
        const GhostarEngine& engine) noexcept
    {
        return { engine.externalPitchJackInserted_,
                 engine.externalPitchSourceVolts_ };
    }

    static PedalInput oscBPedalInput(const GhostarEngine& engine) noexcept
    {
        return { engine.oscBPedalJackInserted_,
                 engine.oscBPedalResistanceKOhm_ };
    }

    static PedalInput filterPedalInput(const GhostarEngine& engine) noexcept
    {
        return { engine.filterPedalJackInserted_,
                 engine.filterPedalResistanceKOhm_ };
    }

    static double decimatorImpulseCentroid() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);

        constexpr int responseSamples = 128;
        double sum = 0.0;
        double firstMoment = 0.0;
        bool impulse = true;
        for (int outputSample = 0; outputSample < responseSamples;
             ++outputSample)
        {
            for (int step = 0; step < 4; ++step)
            {
                engine.filterStageARing_[
                    static_cast<std::size_t>(engine.stageAIndex_)] =
                        impulse ? 1.0 : 0.0;
                impulse = false;
                engine.stageAIndex_ =
                    (engine.stageAIndex_ + 1) % GhostarEngine::stageATaps;

                if ((step & 1) == 1)
                {
                    engine.filterStageBRing_[
                        static_cast<std::size_t>(engine.stageBIndex_)] =
                            convolve(engine.stageAKernel_,
                                     engine.filterStageARing_,
                                     engine.stageAIndex_);
                    engine.stageBIndex_ =
                        (engine.stageBIndex_ + 1)
                        % GhostarEngine::stageBTaps;
                }
            }

            const double value = convolve(engine.stageBKernel_,
                                          engine.filterStageBRing_,
                                          engine.stageBIndex_);
            sum += value;
            firstMoment += static_cast<double>(outputSample) * value;
        }
        return firstMoment / sum;
    }

private:
    template <typename Kernel, std::size_t taps>
    static double convolve(const Kernel& kernel,
                           const std::array<double, taps>& ring,
                           int oldestIndex) noexcept
    {
        double result = 0.0;
        for (int index = 0; index < kernel.count; ++index)
        {
            int ringIndex = oldestIndex
                + kernel.offsets[static_cast<std::size_t>(index)];
            if (ringIndex >= static_cast<int>(taps))
                ringIndex -= static_cast<int>(taps);
            result += kernel.values[static_cast<std::size_t>(index)]
                    * ring[static_cast<std::size_t>(ringIndex)];
        }
        return result;
    }
};
} // namespace ghostar

struct GhostarAudioProcessorTestAccess
{
    static ghostar::GhostarCircuitTestAccess::ExternalPitchInput
    externalPitchInput(GhostarAudioProcessor& processor) noexcept
    {
        processor.updateEngineParameters();
        return ghostar::GhostarCircuitTestAccess::externalPitchInput(
            processor.engine);
    }

    static ghostar::GhostarCircuitTestAccess::PedalInput
    oscBPedalInput(GhostarAudioProcessor& processor) noexcept
    {
        processor.updateEngineParameters();
        return ghostar::GhostarCircuitTestAccess::oscBPedalInput(
            processor.engine);
    }

    static ghostar::GhostarCircuitTestAccess::PedalInput
    filterPedalInput(GhostarAudioProcessor& processor) noexcept
    {
        processor.updateEngineParameters();
        return ghostar::GhostarCircuitTestAccess::filterPedalInput(
            processor.engine);
    }
};

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
int failureCount = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool isFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (!std::isfinite(samples[sample]))
                return false;
    }
    return true;
}

double peakOf(const juce::AudioBuffer<float>& buffer)
{
    double peak = 0.0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = std::max(peak,
                            std::abs(static_cast<double>(samples[sample])));
    }
    return peak;
}

void setParameter(GhostarAudioProcessor& processor, const char* id,
                  float normalised)
{
    if (auto* parameter = processor.parameters.getParameter(id))
        parameter->setValueNotifyingHost(normalised);
    else
        expect(false, std::string("missing parameter: ") + id);
}

void setActualParameter(GhostarAudioProcessor& processor, const char* id,
                        float value)
{
    if (auto* parameter = processor.parameters.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    else
        expect(false, std::string("missing parameter: ") + id);
}

void configureExternalAudio(GhostarAudioProcessor& processor,
                            bool connected)
{
    namespace ids = ghostar::parameters;
    setParameter(processor, ids::externalAudioConnected,
                 connected ? 1.0f : 0.0f);
    setParameter(processor, ids::masterVolume, 1.0f);
    setParameter(processor, ids::brightness, 1.0f);
    setParameter(processor, ids::vcaBypass, 1.0f);
    setParameter(processor, ids::filterPathA, 0.0f);
    setParameter(processor, ids::filterPathB, 0.0f);
    setParameter(processor, ids::filterPathNoise, 1.0f);
    setParameter(processor, ids::shaperPathA, 0.0f);
    setParameter(processor, ids::shaperPathB, 0.0f);
    setParameter(processor, ids::shaperPathRing, 0.0f);
    setParameter(processor, ids::shaperPathNoise, 0.0f);
}

double maximumDifference(const juce::AudioBuffer<float>& a,
                         const juce::AudioBuffer<float>& b)
{
    double difference = 0.0;
    const int channels = std::min(a.getNumChannels(), b.getNumChannels());
    const int samples = std::min(a.getNumSamples(), b.getNumSamples());
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < samples; ++sample)
            difference = std::max(
                difference,
                std::abs(static_cast<double>(a.getSample(channel, sample))
                         - static_cast<double>(b.getSample(channel, sample))));
    return difference;
}

// Renders `blocks` blocks, feeding `midi` into the first one; returns the
// peak of the final block.
double renderBlocks(GhostarAudioProcessor& processor, int blocks,
                    const juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> buffer(2, blockSize);
    double lastPeak = 0.0;
    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer events = block == 0 ? midi : juce::MidiBuffer {};
        processor.processBlock(buffer, events);
        expect(isFinite(buffer), "a processed block carried non-finite audio");
        lastPeak = peakOf(buffer);
    }
    return lastPeak;
}

juce::Image renderEditorSnapshot(juce::AudioProcessorEditor& editor)
{
    juce::Image snapshot(juce::Image::ARGB, editor.getWidth(),
                         editor.getHeight(), true);
    juce::Graphics graphics(snapshot);
    editor.paintEntireComponent(graphics, true);
    return snapshot;
}

void saveRequestedSnapshot(const juce::Image& snapshot, const char* variable)
{
    const auto path = juce::SystemStats::getEnvironmentVariable(variable, {});
    if (path.isEmpty())
        return;
    const juce::File file { path };
    file.getParentDirectory().createDirectory();
    juce::FileOutputStream output { file };
    juce::PNGImageFormat png;
    expect(snapshot.isValid() && output.openedOk() && output.setPosition(0)
               && output.truncate() && png.writeImageToStream(snapshot, output),
           std::string("could not write requested snapshot: ") + variable);
}

// A panel that rendered as one flat colour would still be "valid"; what we
// actually want to know is that it drew something.
bool snapshotHasDetail(const juce::Image& snapshot)
{
    if (!snapshot.isValid())
        return false;

    std::set<juce::uint32> distinct;
    for (int y = 0; y < snapshot.getHeight(); y += 4)
        for (int x = 0; x < snapshot.getWidth(); x += 4)
        {
            const auto pixel = snapshot.getPixelAt(x, y);
            if (pixel.getAlpha() < 250)
                return false;
            distinct.insert(pixel.getARGB());
            if (distinct.size() > 64)
                return true;
        }
    return distinct.size() > 8;
}

// Check the actual host strings and component tree, including the tooltips
// and choices that are invisible in a screenshot. Source-reference URLs and
// circuit part numbers belong in engineering documentation, not the panel.
void checkPublicText(const juce::String& text)
{
    for (const auto* excluded : { "crumar", "spirit", "moog" })
        expect(!text.containsIgnoreCase(excluded),
               "original manufacturer/model name in public text: "
                   + text.toStdString());
}

void checkComponentText(juce::Component& component)
{
    checkPublicText(component.getName());
    checkPublicText(component.getTitle());
    checkPublicText(component.getDescription());
    if (auto* tooltip = dynamic_cast<juce::TooltipClient*>(&component))
        checkPublicText(tooltip->getTooltip());
    if (const auto* label = dynamic_cast<const juce::Label*>(&component))
        checkPublicText(label->getText());
    if (const auto* button = dynamic_cast<const juce::Button*>(&component))
        checkPublicText(button->getButtonText());
    if (const auto* box = dynamic_cast<const juce::ComboBox*>(&component))
        for (int item = 0; item < box->getNumItems(); ++item)
            checkPublicText(box->getItemText(item));
    for (auto* child : component.getChildren())
        checkComponentText(*child);
}

void testPublicIdentity()
{
    GhostarAudioProcessor processor;
    checkPublicText(processor.getName());
    for (const auto* parameter : processor.getParameters())
        checkPublicText(parameter->getName(256));
    for (int program = 0; program < processor.getNumPrograms(); ++program)
    {
        checkPublicText(processor.getProgramName(program));
        checkPublicText(ghostar::factoryPresetDescription(program));
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    expect(editor != nullptr, "the public identity check needs an editor");
    if (editor != nullptr)
        checkComponentText(*editor);
}

void collectPanelControls(juce::Component& parent,
                          std::vector<juce::Component*>& controls)
{
    for (auto* child : parent.getChildren())
    {
        if (!child->isVisible())
            continue;
        if (dynamic_cast<juce::Slider*>(child)
            || dynamic_cast<juce::ComboBox*>(child)
            || dynamic_cast<juce::Button*>(child))
            controls.push_back(child);
        else
            collectPanelControls(*child, controls);
    }
}

// Check actual click targets at both scales. A detailed-looking screenshot
// alone cannot catch a negative-width control or a drawer blocking its peers.
void checkPanelControlGeometry(juce::AudioProcessorEditor& editor)
{
    // An offscreen editor defaults to hidden; getComponentAt deliberately
    // returns null in that state even though paintEntireComponent draws it.
    const bool wasVisible = editor.isVisible();
    editor.setVisible(true);
    std::vector<juce::Component*> controls;
    collectPanelControls(editor, controls);
    expect(!controls.empty(), "the panel has no visible interactive controls");
    for (auto* control : controls)
    {
        const auto bounds = editor.getLocalArea(control,
                                                control->getLocalBounds());
        const auto name = control->getName().toStdString();
        expect(!bounds.isEmpty(), "empty panel control: " + name);
        expect(editor.getLocalBounds().expanded(1).contains(bounds),
               "panel control outside editor: " + name);
        auto* hit = editor.getComponentAt(bounds.getCentre());
        expect(hit == control || (hit != nullptr && control->isParentOf(hit)),
               "panel control cannot be clicked at its centre: " + name);
    }
    editor.setVisible(wasVisible);
}

// The automation contract: every published ID, exactly once. A removed or
// renamed ID breaks saved sessions, so the full list is pinned here.
void testParameterLayoutIsStable()
{
    GhostarAudioProcessor processor;
    namespace ids = ghostar::parameters;
    const char* expectedIds[] = {
        ids::tune, ids::octave, ids::oscAWaveform, ids::sync,
        ids::oscBWaveform, ids::oscBRange, ids::interval, ids::trigger,
        ids::gateKbd, ids::gateX, ids::gateYExt, ids::arpeggiator,
        ids::modSource, ids::lfoRate, ids::shaperMode, ids::shaperShape,
        ids::shaperRate, ids::modXTo, ids::shapeXWithY, ids::shaperYTo,
        ids::masterVolume, ids::brightness, ids::shaperPathA,
        ids::shaperPathB, ids::shaperPathRing, ids::shaperPathNoise,
        ids::filterPathA, ids::filterPathB, ids::filterPathNoise, ids::cutoff,
        ids::lowerOnly, ids::upperResonance, ids::resonance, ids::slope,
        ids::kbAmount, ids::lowerMode, ids::tracking, ids::filterEnvAmount,
        ids::filterAttack, ids::filterDecay, ids::filterSustain,
        ids::filterRelease, ids::vcaBypass, ids::loudnessAttack,
        ids::loudnessDecay, ids::loudnessSustain, ids::loudnessRelease,
        ids::glide, ids::glideMode, ids::xWheel, ids::yWheel,
        ids::splitPaths, ids::externalGate, ids::externalAudioConnected,
        ids::externalPitchConnected, ids::externalPitchVolts,
        ids::oscBPedalConnected, ids::oscBPedalResistance,
        ids::filterPedalConnected, ids::filterPedalResistance,
    };

    int found = 0;
    const auto& publishedParameters = processor.getParameters();
    for (std::size_t index = 0; index < std::size(expectedIds); ++index)
    {
        const auto* id = expectedIds[index];
        const auto* ordered = index < static_cast<std::size_t>(
                                          publishedParameters.size())
            ? dynamic_cast<const juce::AudioProcessorParameterWithID*>(
                  publishedParameters[static_cast<int>(index)])
            : nullptr;
        expect(ordered != nullptr && ordered->paramID == id,
               std::string("wrong parameter at published position: ") + id);
        if (const auto* parameter = processor.parameters.getParameter(id))
        {
            ++found;
            const bool rearInputAddedLater =
                index >= std::size(expectedIds) - 8;
            expect(parameter->getVersionHint()
                       == (rearInputAddedLater ? 2 : 1),
                   std::string("wrong Audio Unit version hint: ") + id);
        }
        else
            expect(false, std::string("missing parameter: ") + id);
    }
    expect(found == static_cast<int>(std::size(expectedIds)),
           "the published parameter set changed");
    expect(processor.getParameters().size()
               == static_cast<int>(std::size(expectedIds)),
           "the processor published an unexpected extra parameter");

    const auto* pitchVoltsParameter =
        processor.parameters.getParameter(ids::externalPitchVolts);
    const auto* pitchVolts =
        dynamic_cast<const juce::AudioParameterFloat*>(pitchVoltsParameter);
    expect(pitchVolts != nullptr,
           "External Pitch Voltage is not a float parameter");
    if (pitchVolts != nullptr)
    {
        const auto& range = pitchVolts->getNormalisableRange();
        expect(std::abs(range.start + 5.5f) < 1.0e-6f
                   && std::abs(range.end - 5.5f) < 1.0e-6f,
               "External Pitch Voltage range is not -5.5..5.5 V");
        expect(std::abs(range.interval) < 1.0e-6f
                   && !pitchVolts->isDiscrete(),
               "External Pitch Voltage is not continuously automatable");
        expect(std::abs(pitchVolts->convertFrom0to1(
                            pitchVoltsParameter->getDefaultValue()))
                   < 1.0e-6f,
               "External Pitch Voltage actual default is not zero volts");
        expect(pitchVolts->getLabel() == "V",
               "External Pitch Voltage host label is not V");
    }

    const auto* pitchConnectedParameter =
        processor.parameters.getParameter(ids::externalPitchConnected);
    const auto* pitchConnected =
        dynamic_cast<const juce::AudioParameterChoice*>(
            pitchConnectedParameter);
    expect(pitchConnected != nullptr,
           "External Pitch connection is not a choice parameter");
    if (pitchConnected != nullptr)
    {
        expect(pitchConnected->choices.size() == 2
                   && pitchConnected->choices[0] == "Unplugged"
                   && pitchConnected->choices[1] == "Plugged",
               "External Pitch connection choices or order changed");
        expect(std::abs(pitchConnected->convertFrom0to1(
                            pitchConnectedParameter->getDefaultValue()))
                       < 1.0e-6f
                   && pitchConnected->getIndex() == 0,
               "External Pitch connection no longer defaults to Unplugged");
    }

    for (const auto* id : { ids::oscBPedalConnected,
                            ids::filterPedalConnected })
    {
        const auto* parameter = processor.parameters.getParameter(id);
        const auto* connected =
            dynamic_cast<const juce::AudioParameterBool*>(parameter);
        expect(connected != nullptr,
               std::string("pedal connection is not a bool parameter: ") + id);
        if (connected != nullptr)
            expect(connected->convertFrom0to1(parameter->getDefaultValue())
                       < 0.5f,
                   std::string("pedal connection does not default off: ") + id);
    }

    for (const auto* id : { ids::oscBPedalResistance,
                            ids::filterPedalResistance })
    {
        const auto* parameter = processor.parameters.getParameter(id);
        const auto* resistance =
            dynamic_cast<const juce::AudioParameterFloat*>(parameter);
        expect(resistance != nullptr,
               std::string("pedal resistance is not a float parameter: ") + id);
        if (resistance == nullptr)
            continue;

        const auto& range = resistance->getNormalisableRange();
        expect(std::abs(range.start) < 1.0e-6f
                   && std::abs(range.end - 100.0f) < 1.0e-6f,
               std::string("pedal resistance range is not 0..100 kOhm: ")
                   + id);
        expect(std::abs(range.interval) < 1.0e-6f
                   && !resistance->isDiscrete()
                   && resistance->isAutomatable(),
               std::string("pedal resistance is not continuous: ") + id);
        expect(std::abs(resistance->convertFrom0to1(
                            parameter->getDefaultValue())
                        - 100.0f)
                   < 1.0e-6f,
               std::string("pedal resistance default is not 100 kOhm: ")
                   + id);
        expect(resistance->getLabel() == "kOhm",
               std::string("pedal resistance unit changed: ") + id);
        expect(parameter->getText(resistance->convertTo0to1(42.375f), 64)
                   == "42.4",
               std::string("pedal resistance host text is unclear: ") + id);
    }
}

void testMidiProducesAudio()
{
    GhostarAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::MidiBuffer midi;
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 7);
    const double sounding = renderBlocks(processor, 24, midi);
    expect(sounding > 1.0e-4, "a keyed note produced no audio");

    juce::MidiBuffer off;
    off.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
    renderBlocks(processor, 1, off);
    juce::MidiBuffer silentTail;
    const double released = renderBlocks(processor, 400, silentTail);
    expect(released < 1.0e-4, "a released note did not decay to silence");
    processor.releaseResources();
}

void testAllNotesOffReleasesEveryKey()
{
    GhostarAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::MidiBuffer midi;
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 55, static_cast<juce::uint8>(100)), 8);
    renderBlocks(processor, 8, midi);
    expect(processor.isGateOpenForDisplay(), "keyed notes opened the gate");

    juce::MidiBuffer allNotesOff;
    allNotesOff.addEvent(
        juce::MidiMessage::controllerEvent(1, 123, 0), 0);
    renderBlocks(processor, 1, allNotesOff);
    expect(!processor.isGateOpenForDisplay(),
           "CC123 did not release the held keys");

    juce::MidiBuffer quiet;
    const double tail = renderBlocks(processor, 400, quiet);
    expect(tail < 1.0e-4, "the all-notes-off release did not reach silence");
    processor.releaseResources();
}

void testStateRoundTrip()
{
    namespace ids = ghostar::parameters;
    GhostarAudioProcessor saved;
    if (auto* parameter = saved.parameters.getParameter(ids::cutoff))
        parameter->setValueNotifyingHost(0.31f);
    if (auto* parameter = saved.parameters.getParameter(ids::lowerMode))
        parameter->setValueNotifyingHost(1.0f); // the last detent: High Pass
    juce::MemoryBlock state;
    saved.getStateInformation(state);

    GhostarAudioProcessor restored;
    restored.setStateInformation(state.getData(),
                                 static_cast<int>(state.getSize()));
    auto* cutoff = restored.parameters.getRawParameterValue(ids::cutoff);
    expect(cutoff != nullptr && std::abs(cutoff->load() - 0.31f) < 0.002f,
           "a stored travel value did not survive the round trip");
    auto* lowerMode = restored.parameters.getRawParameterValue(ids::lowerMode);
    expect(lowerMode != nullptr
               && std::lround(lowerMode->load()) == 3,
           "a stored switch position did not survive the round trip");
}

void testExternalGateStatesRoundTripAndReachTheEngine()
{
    namespace ids = ghostar::parameters;

    for (int externalGate = 0; externalGate < 3; ++externalGate)
    {
        GhostarAudioProcessor saved;
        if (auto* parameter = saved.parameters.getParameter(ids::externalGate))
            parameter->setValueNotifyingHost(
                static_cast<float>(externalGate) / 2.0f);

        juce::MemoryBlock state;
        saved.getStateInformation(state);
        GhostarAudioProcessor restored;
        restored.setStateInformation(state.getData(),
                                     static_cast<int>(state.getSize()));

        const auto* raw =
            restored.parameters.getRawParameterValue(ids::externalGate);
        expect(raw != nullptr && std::lround(raw->load()) == externalGate,
               "an EXTERNAL GATE jack state did not survive state restore");

        if (auto* parameter = restored.parameters.getParameter(ids::gateKbd))
            parameter->setValueNotifyingHost(0.0f);
        if (auto* parameter = restored.parameters.getParameter(ids::gateX))
            parameter->setValueNotifyingHost(0.0f);
        if (auto* parameter = restored.parameters.getParameter(ids::gateYExt))
            parameter->setValueNotifyingHost(1.0f);
        if (auto* parameter = restored.parameters.getParameter(ids::shaperMode))
            parameter->setValueNotifyingHost(1.0f / 3.0f); // KBD HOLD: SG low

        restored.prepareToPlay(sampleRate, blockSize);
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        double heard = 0.0;
        for (int block = 0; block < 8; ++block)
        {
            buffer.clear();
            restored.processBlock(buffer, midi);
            heard = std::max(heard, peakOf(buffer));
        }
        // KBD HOLD has no way to raise its own normalled SG without a gate,
        // so UNPLUGGED and LOW must both stay shut; only mapped HIGH opens.
        const bool expectedOpen = externalGate == 2;
        expect(restored.isGateOpenForDisplay() == expectedOpen,
               "the restored EXTERNAL GATE state did not reach the engine");
        expect(expectedOpen ? heard > 1.0e-4 : heard < 1.0e-4,
               "the restored EXTERNAL GATE state did not articulate the "
               "Loudness path");

        // The complementary state signature checks the switching contact:
        // FREE raises internal SG, so only UNPLUGGED follows it. LOW still
        // replaces it with zero, while external HIGH remains high.
        if (auto* parameter =
                restored.parameters.getParameter(ids::shaperMode))
            parameter->setValueNotifyingHost(0.0f);
        restored.prepareToPlay(sampleRate, blockSize);
        buffer.clear();
        restored.processBlock(buffer, midi);
        const bool expectedWithFreeShaper = externalGate != 1;
        expect(restored.isGateOpenForDisplay() == expectedWithFreeShaper,
               "the EXTERNAL GATE state did not preserve the switched SG "
               "normal contact");
        restored.releaseResources();
    }
}

void testExternalAudioStateAndProgramPreservation()
{
    namespace ids = ghostar::parameters;

    for (const bool connected : { false, true })
    {
        GhostarAudioProcessor saved;
        setParameter(saved, ids::externalAudioConnected,
                     connected ? 1.0f : 0.0f);
        saved.setCurrentProgram(3);
        const auto* afterProgram =
            saved.parameters.getRawParameterValue(ids::externalAudioConnected);
        expect(afterProgram != nullptr
                   && (afterProgram->load() >= 0.5f) == connected,
               "a factory program changed the physical audio-cable state");

        juce::MemoryBlock state;
        saved.getStateInformation(state);
        GhostarAudioProcessor restored;
        restored.setStateInformation(state.getData(),
                                     static_cast<int>(state.getSize()));
        const auto* raw = restored.parameters.getRawParameterValue(
            ids::externalAudioConnected);
        expect(raw != nullptr && (raw->load() >= 0.5f) == connected,
               "the EXTERNAL AUDIO cable state did not survive restore");
        expect(restored.getCurrentProgram() == 3,
               "restoring EXTERNAL AUDIO lost the saved program label");

        restored.setCurrentProgram(12);
        expect(raw != nullptr && (raw->load() >= 0.5f) == connected,
               "changing programs overwrote the restored audio-cable state");
    }
}

void testExternalPitchStateProgramAndEngineForwarding()
{
    namespace ids = ghostar::parameters;
    constexpr float sourceVolts = 1.2345f;

    for (const bool jackInserted : { false, true })
    {
        GhostarAudioProcessor saved;
        setParameter(saved, ids::externalPitchConnected,
                     jackInserted ? 1.0f : 0.0f);
        setActualParameter(saved, ids::externalPitchVolts, sourceVolts);

        const auto forwarded =
            GhostarAudioProcessorTestAccess::externalPitchInput(saved);
        expect(forwarded.jackInserted == jackInserted
                   && std::abs(forwarded.sourceVolts - sourceVolts) < 1.0e-5,
               "the host External Pitch controls did not reach the engine");

        saved.setCurrentProgram(3);
        const auto* connected = saved.parameters.getRawParameterValue(
            ids::externalPitchConnected);
        const auto* volts = saved.parameters.getRawParameterValue(
            ids::externalPitchVolts);
        expect(connected != nullptr
                   && (connected->load() >= 0.5f) == jackInserted
                   && volts != nullptr
                   && std::abs(volts->load() - sourceVolts) < 1.0e-5f,
               "a factory program changed the physical pitch-source state");

        juce::MemoryBlock state;
        saved.getStateInformation(state);
        GhostarAudioProcessor restored;
        restored.setStateInformation(state.getData(),
                                     static_cast<int>(state.getSize()));
        connected = restored.parameters.getRawParameterValue(
            ids::externalPitchConnected);
        volts = restored.parameters.getRawParameterValue(
            ids::externalPitchVolts);
        expect(connected != nullptr
                   && (connected->load() >= 0.5f) == jackInserted
                   && volts != nullptr
                   && std::abs(volts->load() - sourceVolts) < 1.0e-5f,
               "a fractional External Pitch state did not survive restore");
        expect(restored.getCurrentProgram() == 3,
               "restoring External Pitch lost the saved program label");

        restored.setCurrentProgram(12);
        expect(connected != nullptr
                   && (connected->load() >= 0.5f) == jackInserted
                   && volts != nullptr
                   && std::abs(volts->load() - sourceVolts) < 1.0e-5f,
               "changing programs overwrote the restored pitch-source state");
    }
}

void testPedalStateProgramAndEngineForwarding()
{
    namespace ids = ghostar::parameters;

    for (const bool oscBConnected : { false, true })
    {
        const bool filterConnected = !oscBConnected;
        const float oscBResistance = oscBConnected ? 17.375f : 83.625f;
        const float filterResistance = oscBConnected ? 68.125f : 9.875f;
        GhostarAudioProcessor saved;
        setParameter(saved, ids::oscBPedalConnected,
                     oscBConnected ? 1.0f : 0.0f);
        setActualParameter(saved, ids::oscBPedalResistance, oscBResistance);
        setParameter(saved, ids::filterPedalConnected,
                     filterConnected ? 1.0f : 0.0f);
        setActualParameter(saved, ids::filterPedalResistance,
                           filterResistance);

        const auto expectHostState = [&](GhostarAudioProcessor& processor,
                                         const std::string& context) {
            const auto* oscBConnectedRaw =
                processor.parameters.getRawParameterValue(
                    ids::oscBPedalConnected);
            const auto* oscBResistanceRaw =
                processor.parameters.getRawParameterValue(
                    ids::oscBPedalResistance);
            const auto* filterConnectedRaw =
                processor.parameters.getRawParameterValue(
                    ids::filterPedalConnected);
            const auto* filterResistanceRaw =
                processor.parameters.getRawParameterValue(
                    ids::filterPedalResistance);
            expect(oscBConnectedRaw != nullptr
                       && (oscBConnectedRaw->load() >= 0.5f)
                              == oscBConnected
                       && oscBResistanceRaw != nullptr
                       && std::abs(oscBResistanceRaw->load()
                                   - oscBResistance)
                              < 1.0e-5f
                       && filterConnectedRaw != nullptr
                       && (filterConnectedRaw->load() >= 0.5f)
                              == filterConnected
                       && filterResistanceRaw != nullptr
                       && std::abs(filterResistanceRaw->load()
                                   - filterResistance)
                              < 1.0e-5f,
                   context);
        };

        const auto expectEngineState = [&](GhostarAudioProcessor& processor,
                                           const std::string& context) {
            const auto oscB =
                GhostarAudioProcessorTestAccess::oscBPedalInput(processor);
            const auto filter =
                GhostarAudioProcessorTestAccess::filterPedalInput(processor);
            expect(oscB.jackInserted == oscBConnected
                       && std::abs(oscB.resistanceKOhm - oscBResistance)
                              < 1.0e-5
                       && filter.jackInserted == filterConnected
                       && std::abs(filter.resistanceKOhm - filterResistance)
                              < 1.0e-5,
                   context);
        };

        expectEngineState(saved,
                          "the host pedal controls did not reach the engine");
        saved.setCurrentProgram(3);
        expectHostState(saved,
                        "a factory program changed the physical pedal state");

        juce::MemoryBlock state;
        saved.getStateInformation(state);
        GhostarAudioProcessor restored;
        restored.setStateInformation(state.getData(),
                                     static_cast<int>(state.getSize()));
        expectHostState(restored,
                        "the pedal state did not survive state restore");
        expectEngineState(restored,
                          "the restored pedal state did not reach the engine");
        expect(restored.getCurrentProgram() == 3,
               "restoring pedal state lost the saved program label");

        restored.setCurrentProgram(12);
        expectHostState(restored,
                        "changing programs overwrote the restored pedal state");
    }
}

void testLegacyStateDefaultsRearInputs()
{
    namespace ids = ghostar::parameters;
    constexpr const char* rearInputIds[] = {
        ids::externalGate,
        ids::externalAudioConnected,
        ids::externalPitchConnected,
        ids::externalPitchVolts,
        ids::oscBPedalConnected,
        ids::oscBPedalResistance,
        ids::filterPedalConnected,
        ids::filterPedalResistance,
    };

    GhostarAudioProcessor legacySource;
    setParameter(legacySource, ids::cutoff, 0.27f);
    auto legacyState = legacySource.parameters.copyState();
    for (const auto* id : rearInputIds)
        if (const auto child = legacyState.getChildWithProperty("id", id);
            child.isValid())
            legacyState.removeChild(child, nullptr);

    const auto xml = legacyState.createXml();
    expect(xml != nullptr, "could not encode the simulated legacy state");
    if (xml == nullptr)
        return;
    juce::MemoryBlock data;
    juce::AudioProcessor::copyXmlToBinary(*xml, data);

    GhostarAudioProcessor edited;
    for (const auto* id : rearInputIds)
        if (auto* parameter = edited.parameters.getParameter(id))
            parameter->setValueNotifyingHost(
                parameter->getDefaultValue() < 0.5f ? 1.0f : 0.0f);
    edited.setStateInformation(data.getData(), static_cast<int>(data.getSize()));

    const auto* cutoff = edited.parameters.getRawParameterValue(ids::cutoff);
    expect(cutoff != nullptr && std::abs(cutoff->load() - 0.27f) < 0.002f,
           "the simulated legacy state did not otherwise load");
    for (const auto* id : rearInputIds)
    {
        const auto* parameter = edited.parameters.getParameter(id);
        const auto* raw = edited.parameters.getRawParameterValue(id);
        const float expected = parameter != nullptr
            ? parameter->convertFrom0to1(parameter->getDefaultValue()) : 0.0f;
        expect(parameter != nullptr && raw != nullptr
                   && std::abs(raw->load() - expected) < 1.0e-6f,
               std::string("legacy state retained edited rear input: ") + id);
    }
}

// JUCE's Standalone holder persists the last panel as `filterState` and
// restores it before playback or the editor starts. Ghostar instead powers
// up at Init, but explicit Load State must still work with no audio device.
void testStandaloneStartsAtInitButStillLoadsStateExplicitly()
{
    namespace ids = ghostar::parameters;
    GhostarAudioProcessor saved;
    saved.setCurrentProgram(3); // Fat Filter: cutoff 0.45
    juce::MemoryBlock state;
    saved.getStateInformation(state);

    juce::AudioProcessor::setTypeOfNextNewPlugin(
        juce::AudioProcessor::wrapperType_Standalone);
    GhostarAudioProcessor standalone;
    juce::AudioProcessor::setTypeOfNextNewPlugin(
        juce::AudioProcessor::wrapperType_Undefined);
    expect(standalone.getMainBusNumInputChannels() == 1,
           "Standalone did not enable its External Audio capture bus");

    standalone.setStateInformation(state.getData(),
                                   static_cast<int>(state.getSize()));
    auto* cutoff = standalone.parameters.getRawParameterValue(ids::cutoff);
    expect(standalone.getCurrentProgram() == 0
               && cutoff != nullptr
               && std::abs(cutoff->load() - 0.62f) < 0.002f,
           "Standalone restored its previous panel instead of starting at Init");

    // StandaloneFilterWindow builds the editor even if device initialisation
    // failed. Its Options -> Load State action must therefore end the startup
    // phase independently of prepareToPlay().
    std::unique_ptr<juce::AudioProcessorEditor> editor(
        standalone.createEditorAndMakeActive());
    expect(editor != nullptr, "Standalone did not create its editor");
    standalone.setStateInformation(state.getData(),
                                   static_cast<int>(state.getSize()));
    expect(standalone.getCurrentProgram() == 3
               && cutoff != nullptr
               && std::abs(cutoff->load() - 0.45f) < 0.002f,
           "Standalone rejected an explicit state load without an audio device");
    standalone.editorBeingDeleted(editor.get());
    editor.reset();

    standalone.setCurrentProgram(0);
    standalone.prepareToPlay(sampleRate, blockSize);
    standalone.setStateInformation(state.getData(),
                                   static_cast<int>(state.getSize()));
    expect(standalone.getCurrentProgram() == 3
               && cutoff != nullptr
               && std::abs(cutoff->load() - 0.45f) < 0.002f,
           "Standalone rejected an explicit state load after starting");
    standalone.releaseResources();
}

// Zero-crossing pitch estimate over rendered blocks, for checks that need
// to hear *which* note sounded rather than just that something did.
double renderedZeroCrossingHz(GhostarAudioProcessor& processor, int blocks)
{
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer empty;
    std::vector<float> collected;
    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer events;
        processor.processBlock(buffer, events);
        const auto* samples = buffer.getReadPointer(0);
        collected.insert(collected.end(), samples, samples + blockSize);
    }
    int crossings = 0;
    for (std::size_t index = 1; index < collected.size(); ++index)
        if ((collected[index - 1] < 0.0f) != (collected[index] < 0.0f))
            ++crossings;
    const double seconds =
        static_cast<double>(collected.size()) / sampleRate;
    return static_cast<double>(crossings) / (2.0 * seconds);
}

// MIDI CC120 (All Sound Off) must silence the instrument without resetting
// controllers: a pitch bend held across it still applies to the next note.
void testAllSoundOffKeepsTheBend()
{
    GhostarAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::MidiBuffer noteOn;
    noteOn.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
    renderBlocks(processor, 24, noteOn);
    const double unbentHz = renderedZeroCrossingHz(processor, 90);

    juce::MidiBuffer bendThenStop;
    bendThenStop.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0);
    bendThenStop.addEvent(juce::MidiMessage::controllerEvent(1, 120, 0), 8);
    renderBlocks(processor, 1, bendThenStop);
    juce::MidiBuffer quiet;
    const double stopped = renderBlocks(processor, 8, quiet);
    expect(stopped < 1.0e-4, "CC120 did not silence the sounding voice");

    renderBlocks(processor, 24, noteOn);
    const double bentHz = renderedZeroCrossingHz(processor, 90);
    expect(bentHz > unbentHz * 1.35,
           "the pitch bend did not survive CC120");
    processor.releaseResources();
}

void testExternalAudioBusLayouts()
{
    GhostarAudioProcessor processor;
    const auto initial = processor.getBusesLayout();
    expect(initial.getMainInputChannelSet().isDisabled(),
           "the optional external-audio bus is enabled by default");
    expect(initial.getMainOutputChannelSet()
               == juce::AudioChannelSet::stereo(),
           "the default main output is no longer stereo");

    auto monoToStereo = initial;
    monoToStereo.inputBuses.getReference(0) =
        juce::AudioChannelSet::mono();
    expect(processor.isBusesLayoutSupported(monoToStereo),
           "a mono external input with stereo output is unsupported");

    auto monoToMono = monoToStereo;
    monoToMono.outputBuses.getReference(0) = juce::AudioChannelSet::mono();
    expect(processor.isBusesLayoutSupported(monoToMono),
           "a mono external input with mono output is unsupported");

    auto stereoInput = monoToStereo;
    stereoInput.inputBuses.getReference(0) =
        juce::AudioChannelSet::stereo();
    expect(!processor.isBusesLayoutSupported(stereoInput),
           "a stereo cable was accepted by the mono physical jack");
    expect(processor.setBusesLayout(monoToStereo),
           "the supported external-audio layout could not be activated");
}

// Cable presence is a panel state, not a synonym for whether the host has
// enabled its optional bus. A disabled input therefore supplies zero volts
// to an inserted jack; only the UNPLUGGED state restores IC4A pink.
void testExternalAudioDisabledBusIsStillAnInsertedSilentCable()
{
    constexpr int samples = 2048;

    GhostarAudioProcessor saved;
    configureExternalAudio(saved, true);
    juce::MemoryBlock state;
    saved.getStateInformation(state);

    GhostarAudioProcessor disabled;
    disabled.setStateInformation(state.getData(),
                                 static_cast<int>(state.getSize()));
    GhostarAudioProcessor active;
    configureExternalAudio(active, true);
    auto activeLayout = active.getBusesLayout();
    activeLayout.inputBuses.getReference(0) = juce::AudioChannelSet::mono();
    expect(active.setBusesLayout(activeLayout),
           "the mono external bus could not be enabled");
    GhostarAudioProcessor unplugged;
    configureExternalAudio(unplugged, false);

    disabled.prepareToPlay(sampleRate, samples);
    active.prepareToPlay(sampleRate, samples);
    unplugged.prepareToPlay(sampleRate, samples);
    juce::AudioBuffer<float> disabledOutput(2, samples);
    juce::AudioBuffer<float> activeZero(2, samples);
    juce::AudioBuffer<float> unpluggedOutput(2, samples);
    disabledOutput.clear();
    activeZero.clear();
    unpluggedOutput.clear();
    juce::MidiBuffer midi;
    disabled.processBlock(disabledOutput, midi);
    active.processBlock(activeZero, midi);
    unplugged.processBlock(unpluggedOutput, midi);

    expect(isFinite(disabledOutput) && isFinite(activeZero)
               && isFinite(unpluggedOutput),
           "an external-audio bus state produced non-finite output");
    expect(maximumDifference(disabledOutput, activeZero) == 0.0,
           "a disabled host bus changed the inserted jack from zero volts");
    expect(maximumDifference(disabledOutput, unpluggedOutput) > 1.0e-5,
           "UNPLUGGED did not restore the internal pink-noise normal");

    disabled.releaseResources();
    active.releaseResources();
    unplugged.releaseResources();
}

// With one input and two outputs, JUCE aliases the mono input with output
// channel zero. The processor must capture each input sample before writing
// that output, overwrite the output-only channel even if it arrives poisoned,
// and advance the input pointer correctly across MIDI-created segments.
void testExternalAudioInPlaceChannelsAndMidiSegmentation()
{
    constexpr int samples = 512;
    GhostarAudioProcessor whole;
    GhostarAudioProcessor segmented;
    GhostarAudioProcessor silentInput;
    for (auto* processor : { &whole, &segmented, &silentInput })
    {
        configureExternalAudio(*processor, true);
        auto layout = processor->getBusesLayout();
        layout.inputBuses.getReference(0) = juce::AudioChannelSet::mono();
        expect(processor->setBusesLayout(layout),
               "the mono external bus could not be enabled for processing");
        processor->prepareToPlay(sampleRate, samples);
    }

    juce::AudioBuffer<float> wholeBuffer(2, samples);
    juce::AudioBuffer<float> segmentedBuffer(2, samples);
    juce::AudioBuffer<float> zeroBuffer(2, samples);
    for (int sample = 0; sample < samples; ++sample)
    {
        const float input = static_cast<float>(
            0.31 * std::sin(0.037 * sample)
            + 0.19 * std::cos(0.013 * sample)
            + 0.07 * ((sample * 37) % 101 - 50) / 50.0);
        wholeBuffer.setSample(0, sample, input);
        segmentedBuffer.setSample(0, sample, input);
        zeroBuffer.setSample(0, sample, 0.0f);
        wholeBuffer.setSample(1, sample, 0.0f);
        segmentedBuffer.setSample(
            1, sample, std::numeric_limits<float>::quiet_NaN());
        zeroBuffer.setSample(1, sample, 0.0f);
    }

    juce::MidiBuffer noEvents;
    juce::MidiBuffer ignoredEvents;
    for (const int position : { 1, 17, 73, 255, 383, 511 })
        ignoredEvents.addEvent(
            juce::MidiMessage::controllerEvent(1, 99, position % 128),
            position);
    whole.processBlock(wholeBuffer, noEvents);
    segmented.processBlock(segmentedBuffer, ignoredEvents);
    silentInput.processBlock(zeroBuffer, noEvents);

    expect(isFinite(segmentedBuffer),
           "the output-only channel retained host garbage");
    expect(maximumDifference(wholeBuffer, segmentedBuffer) == 0.0,
           "MIDI segmentation changed external-audio phase or sample index");
    expect(maximumDifference(wholeBuffer, zeroBuffer) > 1.0e-5,
           "the aliased input was overwritten before reaching the engine");

    whole.releaseResources();
    segmented.releaseResources();
    silentInput.releaseResources();
}

// A mono host layout has no right channel to split onto; enabling SPLIT
// there must fold both paths into the mono output instead of silently
// discarding the whole Shaper path.
void testMonoLayoutKeepsTheShaperPath()
{
    namespace ids = ghostar::parameters;
    GhostarAudioProcessor processor;
    auto layout = processor.getBusesLayout();
    layout.outputBuses.getReference(0) = juce::AudioChannelSet::mono();
    expect(processor.setBusesLayout(layout),
           "the processor did not accept a mono output layout");

    const auto set = [&processor](const char* id, float value) {
        if (auto* parameter = processor.parameters.getParameter(id))
            parameter->setValueNotifyingHost(value);
    };
    set(ids::splitPaths, 1.0f);
    set(ids::filterPathA, 0.0f);
    set(ids::filterPathB, 0.0f);
    set(ids::filterPathNoise, 0.0f);
    set(ids::shaperPathA, 0.8f);
    set(ids::shaperMode, 1.0f / 3.0f); // KBD HOLD: contour rises while held

    processor.prepareToPlay(sampleRate, blockSize);
    juce::AudioBuffer<float> buffer(1, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
    double lastPeak = 0.0;
    double heard = 0.0;
    for (int block = 0; block < 48; ++block)
    {
        buffer.clear();
        juce::MidiBuffer events = block == 0 ? midi : juce::MidiBuffer {};
        processor.processBlock(buffer, events);
        expect(isFinite(buffer), "a mono block carried non-finite audio");
        lastPeak = peakOf(buffer);
        heard = std::max(heard, lastPeak);
    }
    expect(heard > 1.0e-4,
           "the Shaper path fell silent in a mono layout with SPLIT on");
    processor.releaseResources();
}

// A note queued from the on-screen keyboard before PANIC is clicked must
// not replay after the reset.
void testPanicDropsQueuedUiNotes()
{
    GhostarAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    processor.keyboardState.noteOn(1, 60, 1.0f);
    processor.requestPanic();
    juce::MidiBuffer quiet;
    const double after = renderBlocks(processor, 12, quiet);
    expect(after < 1.0e-4, "a note queued before panic replayed after it");
    expect(!processor.isGateOpenForDisplay(),
           "panic left the keying gate open");

    // Only what precedes the click dies: a key pressed after PANIC but
    // before the next audio callback is a fresh note and must sound.
    processor.keyboardState.noteOff(1, 60, 0.0f);
    processor.keyboardState.noteOn(1, 64, 1.0f);
    processor.requestPanic();
    processor.keyboardState.noteOn(1, 60, 1.0f);
    const double post = renderBlocks(processor, 24, quiet);
    expect(post > 1.0e-4,
           "a note pressed after panic was dropped with the stale queue");
    processor.releaseResources();
}

// The two real halfband kernels delay a single internal impulse by 15 + 2*63
// internal samples. The host output is taken after substep three, leaving
// (15 + 126 - 3) / 4 = 34.5 output samples. A signed impulse centroid checks
// that physical ring schedule without involving an oscillator or envelope.
void testDecimatorLatencyMatchesItsActualImpulseResponse()
{
    const double measured =
        ghostar::GhostarCircuitTestAccess::decimatorImpulseCentroid();
    expect(std::abs(measured - 34.5) < 1.0e-9,
           "the two decimator rings do not delay by 34.5 output samples");
}

// A host can only compensate for a delay the processor publishes. The DSP
// check above pins the engine's stated figure to the actual two-stage chain;
// this check pins the processor-facing rounded value to that figure.
void testAdvertisedLatencyMatchesTheMeasuredDelay()
{
    GhostarAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    const double derived = ghostar::GhostarEngine::outputLatencySamples();
    expect(ghostar::GhostarEngine::externalInputLatencyInternalSamples()
               == 141,
           "the reconstruction/frame delay is no longer 141 circuit ticks");
    expect(std::abs(ghostar::GhostarEngine::decimatorLatencySamples() - 34.5)
               < 1.0e-12,
           "the output decimator delay is no longer 34.5 host samples");
    expect(std::abs(derived - 69.75) < 1.0e-12,
           "the complete engine delay is no longer 69.75 host samples");
    expect(processor.getLatencySamples() == juce::roundToInt(derived),
           "the plug-in does not publish the latency the engine derives");
    expect(processor.getLatencySamples() == 70,
           "the plug-in does not round 69.75 samples to 70");
    processor.releaseResources();
}

void testAdvertisedTailCoversTheLongestRelease()
{
    GhostarAudioProcessor processor;
    const double longest = ghostar::GhostarEngine::longestReleaseTailSeconds();
    expect(longest > 1.0,
           "the engine reports an implausible longest release");
    expect(processor.getTailLengthSeconds() >= longest,
           "the advertised tail is shorter than the longest release");
}

// The factory bank is Init, the manual's eleven Sound Charts, and the
// seventeen Ghostar Programs. The host-facing contract: the count and names
// are published, selecting a program writes it into the host parameters,
// and a selected program actually plays.
void testFactoryProgramsAreTheSoundCharts()
{
    namespace ids = ghostar::parameters;
    GhostarAudioProcessor processor;

    expect(processor.getNumPrograms() == 29,
           "the program bank is not Init plus eleven charts plus seventeen "
           "programs");
    expect(processor.getProgramName(0) == "Init",
           "the bank does not open with the default voice");
    expect(processor.getProgramName(1) == "Preparatory Pattern",
           "the charts do not start with the Preparatory Pattern");
    expect(juce::String(ghostar::factoryPresetDescription(1))
               .startsWith("Intentionally silent"),
           "the Preparatory Pattern does not warn that it is silent");
    expect(processor.getProgramName(3) == "Fat Filter",
           "program 3 is not the Fat Filter chart");
    expect(processor.getProgramName(12) == "Ghost Bass",
           "the performance bank does not begin where it should");

    processor.setCurrentProgram(3);
    expect(processor.getCurrentProgram() == 3,
           "the selected program index was not retained");

    auto* cutoff = processor.parameters.getRawParameterValue(ids::cutoff);
    expect(cutoff != nullptr && std::abs(cutoff->load() - 0.45f) < 0.002f,
           "Fat Filter did not write its cutoff travel");
    auto* gateKbd = processor.parameters.getRawParameterValue(ids::gateKbd);
    expect(gateKbd != nullptr && gateKbd->load() > 0.5f,
           "Fat Filter did not switch the keyboard gate on");
    auto* octave = processor.parameters.getRawParameterValue(ids::octave);
    expect(octave != nullptr && std::lround(octave->load()) == 2,
           "Fat Filter did not select the 8' octave");
    auto* xWheel = processor.parameters.getRawParameterValue(ids::xWheel);
    expect(xWheel != nullptr && xWheel->load() < 0.002f,
           "selecting a chart did not pull the X wheel fully back");

    // An out-of-range selection must be ignored, not clamp or crash.
    processor.setCurrentProgram(processor.getNumPrograms());
    expect(processor.getCurrentProgram() == 3,
           "an out-of-range program selection was not ignored");

    // A performance program must reach the host parameters exactly as a
    // chart does — the two banks travel the same lane.
    processor.setCurrentProgram(12);
    expect(processor.getCurrentProgram() == 12,
           "the performance program was not selected");
    auto* slope = processor.parameters.getRawParameterValue(ids::slope);
    expect(slope != nullptr && std::lround(slope->load()) == 1,
           "Ghost Bass did not select the 24 dB slope");

    // Ghostar Programs can store a musically useful wheel stance, unlike the
    // historical charts whose drawings always begin with both wheels back.
    processor.setCurrentProgram(13); // Vowel Motion
    auto* yWheel = processor.parameters.getRawParameterValue(ids::yWheel);
    expect(yWheel != nullptr && std::abs(yWheel->load() - 0.45f) < 0.002f,
           "Vowel Motion did not load its performance-ready Y-wheel stance");
    processor.setCurrentProgram(3);
    expect(yWheel != nullptr && yWheel->load() < 0.002f,
           "returning to a Sound Chart did not pull the Y wheel back");

    // The selected program's name must survive a state round trip; the
    // values themselves already travel as parameters.
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    GhostarAudioProcessor restored;
    restored.setStateInformation(state.getData(),
                                 static_cast<int>(state.getSize()));
    expect(restored.getCurrentProgram() == 3,
           "the program index did not survive the state round trip");
    auto* restoredCutoff =
        restored.parameters.getRawParameterValue(ids::cutoff);
    expect(restoredCutoff != nullptr
               && std::abs(restoredCutoff->load() - 0.45f) < 0.002f,
           "the restored state lost the chart's cutoff travel");

    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent(
        juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
    const double sounding = renderBlocks(processor, 24, midi);
    expect(sounding > 1.0e-4, "the selected Sound Chart produced no audio");
    processor.releaseResources();
}

void testEditorRendering()
{
    GhostarAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    std::unique_ptr<juce::AudioProcessorEditor> editor(
        processor.createEditor());
    expect(editor != nullptr, "the processor produced no editor");
    if (editor == nullptr)
        return;

    // The editor opens at whatever fits the display it is on, so the
    // documentation image is pinned to the panel's design size here rather
    // than taken from whatever the build machine happens to have.
    editor->setSize(1460, 780);
    editor->resized();
    checkPanelControlGeometry(*editor);
    const auto snapshot = renderEditorSnapshot(*editor);
    expect(snapshotHasDetail(snapshot),
           "the editor rendered as a flat surface at its default size");
    expect(snapshot.getWidth() == 1460 && snapshot.getHeight() == 780,
           "the documentation screenshot is no longer the design size");

    // Committed documentation image, regenerated by the nightly build.
    saveRequestedSnapshot(snapshot, "GHOSTAR_EDITOR_SNAPSHOT");

    std::vector<juce::Component*> controls;
    collectPanelControls(*editor, controls);
    juce::Button* connections = nullptr;
    for (auto* control : controls)
        if (auto* button = dynamic_cast<juce::Button*>(control);
            button != nullptr && button->getButtonText() == "REAR CONNECTIONS")
            connections = button;
    expect(connections != nullptr, "rear connections cannot be opened");
    if (connections != nullptr)
    {
        connections->setToggleState(true, juce::dontSendNotification);
        connections->onClick();
        checkPanelControlGeometry(*editor);
        controls.clear();
        collectPanelControls(*editor, controls);
        juce::ComboBox* gate = nullptr;
        for (auto* control : controls)
            if (control->getName() == "EXT GATE")
                gate = dynamic_cast<juce::ComboBox*>(control);
        expect(gate != nullptr, "rear gate selector is inaccessible");
        if (gate != nullptr)
            gate->setSelectedItemIndex(2, juce::sendNotificationSync);
        expect(processor.parameters.getRawParameterValue(
                   ghostar::parameters::externalGate)->load() == 2.0f,
               "rear gate selector did not update its parameter");

        saveRequestedSnapshot(renderEditorSnapshot(*editor),
                              "GHOSTAR_EDITOR_REAR_SNAPSHOT");
        editor->setSize(876, 468);
        checkPanelControlGeometry(*editor);
        connections->setToggleState(false, juce::dontSendNotification);
        connections->onClick();
        checkPanelControlGeometry(*editor);
        saveRequestedSnapshot(renderEditorSnapshot(*editor),
                              "GHOSTAR_EDITOR_SMALL_SNAPSHOT");
        expect(processor.parameters.getRawParameterValue(
                   ghostar::parameters::externalGate)->load() == 2.0f,
               "closing rear connections changed the cable state");
    }

    editor.reset();
    processor.releaseResources();
}

// The panel is a fixed geometry, so the only question a small screen asks is
// whether it scales or gets cut off. A 1366x768 laptop's work area is under
// 768 points tall once the taskbar and the host's window frame are counted,
// and the design panel is 780.
void testEditorFitsASmallDisplay()
{
    // The fit rule itself, on screens no build machine has to have.
    using Editor = GhostarAudioProcessorEditor;
    const juce::Rectangle<int> design { 1460, 780 };
    expect(Editor::panelSizeForWorkArea({}) == design,
           "an unknown work area should open the panel at its design size");
    expect(Editor::panelSizeForWorkArea({ 1920, 1080 }) == design,
           "a display with room should open the panel at its design size");
    for (const auto work : { juce::Rectangle<int> { 1366, 768 },
                             juce::Rectangle<int> { 1280, 800 },
                             juce::Rectangle<int> { 1280, 720 } })
    {
        const auto fitted = Editor::panelSizeForWorkArea(work);
        expect(fitted.getWidth() <= work.getWidth()
                   && fitted.getHeight() <= work.getHeight(),
               "the panel does not fit the display it was fitted to");
        const auto ratio = static_cast<double>(fitted.getWidth())
                           / static_cast<double>(fitted.getHeight());
        expect(std::abs(ratio - 1460.0 / 780.0) < 0.02,
               "the fitted panel lost the design proportions");
    }
    // Smaller than the readable floor: a window the user can move beats type
    // nobody can read, so the rule clamps rather than shrinking further.
    expect(Editor::panelSizeForWorkArea({ 800, 600 }).getWidth() == 876,
           "the fit rule went below the readable minimum");

    GhostarAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(
        processor.createEditor());
    expect(editor != nullptr, "the processor produced no editor");
    if (editor == nullptr)
        return;

    expect(editor->isResizable(),
           "the editor cannot be resized, so a host cannot make it fit");

    auto* constrainer = editor->getConstrainer();
    expect(constrainer != nullptr, "the editor has no size constrainer");
    if (constrainer == nullptr)
        return;

    // Room for the window frame, the menu bar and the taskbar around it.
    constexpr int smallDisplayWidth = 1366;
    constexpr int smallDisplayHeight = 768;
    constexpr int chrome = 64;
    expect(constrainer->getMinimumWidth() <= smallDisplayWidth - chrome,
           "the editor cannot be narrowed onto a 1366-point display");
    expect(constrainer->getMinimumHeight() <= smallDisplayHeight - chrome,
           "the editor cannot be shortened onto a 768-point display");

    const auto designRatio = static_cast<double>(editor->getWidth())
                             / static_cast<double>(editor->getHeight());

    // A host resizing the window to that display goes through the
    // constrainer, which is what holds the panel's proportions.
    juce::Rectangle<int> asked { 0, 0, smallDisplayWidth - chrome,
                                 smallDisplayHeight - chrome };
    constrainer->checkBounds(asked, editor->getBounds(),
                             { 0, 0, 8192, 8192 }, false, false, false, true);
    const auto ratio = static_cast<double>(asked.getWidth())
                       / static_cast<double>(asked.getHeight());
    expect(std::abs(ratio - designRatio) < 0.02,
           "the panel lost its proportions when it was made to fit");
    expect(asked.getWidth() <= smallDisplayWidth - chrome
               && asked.getHeight() <= smallDisplayHeight - chrome,
           "the constrained size still does not fit the display");

    // And at that size the panel is still a panel: it draws, including the
    // keys, which are the part a clipped window loses first.
    editor->setSize(asked.getWidth(), asked.getHeight());
    checkPanelControlGeometry(*editor);
    const auto small = renderEditorSnapshot(*editor);
    expect(snapshotHasDetail(small),
           "the editor rendered as a flat surface at a small size");

    // The white keys are the brightest thing on a charcoal panel, and they
    // are the bottom of the layout: finding them in the bottom eighth of the
    // image is what says the panel scaled instead of being cut off.
    bool sawKeys = false;
    if (small.isValid())
    {
        const int floorStart = small.getHeight() * 7 / 8;
        for (int y = floorStart; y < small.getHeight() && !sawKeys; ++y)
            for (int x = 0; x < small.getWidth(); x += 2)
                if (small.getPixelAt(x, y).getBrightness() > 0.85f)
                {
                    sawKeys = true;
                    break;
                }
    }
    expect(sawKeys, "the keyboard is not on screen once the panel is fitted");

    editor.reset();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    testParameterLayoutIsStable();
    testPublicIdentity();
    testMidiProducesAudio();
    testAllNotesOffReleasesEveryKey();
    testStateRoundTrip();
    testExternalGateStatesRoundTripAndReachTheEngine();
    testExternalAudioStateAndProgramPreservation();
    testExternalPitchStateProgramAndEngineForwarding();
    testPedalStateProgramAndEngineForwarding();
    testLegacyStateDefaultsRearInputs();
    testStandaloneStartsAtInitButStillLoadsStateExplicitly();
    testFactoryProgramsAreTheSoundCharts();
    testAllSoundOffKeepsTheBend();
    testExternalAudioBusLayouts();
    testExternalAudioDisabledBusIsStillAnInsertedSilentCable();
    testExternalAudioInPlaceChannelsAndMidiSegmentation();
    testMonoLayoutKeepsTheShaperPath();
    testPanicDropsQueuedUiNotes();
    testAdvertisedTailCoversTheLongestRelease();
    testDecimatorLatencyMatchesItsActualImpulseResponse();
    testAdvertisedLatencyMatchesTheMeasuredDelay();
    testEditorRendering();
    testEditorFitsASmallDisplay();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Ghostar plug-in check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Ghostar plug-in checks passed.\n";
    return EXIT_SUCCESS;
}
