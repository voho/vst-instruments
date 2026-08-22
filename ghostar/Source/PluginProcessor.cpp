#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
namespace ids = ghostar::parameters;

// Panel travel as a host parameter: 0..1, shown as 0..10 like the silkscreen.
std::unique_ptr<juce::AudioParameterFloat> travel(const char* id,
                                                  const char* name,
                                                  float defaultValue)
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, defaultValue,
        juce::AudioParameterFloatAttributes {}.withStringFromValueFunction(
            [](float value, int) {
                return juce::String(value * 10.0f, 1);
            }));
}

std::unique_ptr<juce::AudioParameterChoice>
detents(const char* id, const char* name, const juce::StringArray& labels,
        int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id, 1 }, name, labels, defaultIndex);
}

std::unique_ptr<juce::AudioParameterBool> rocker(const char* id,
                                                 const char* name,
                                                 bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id, 1 }, name, defaultValue);
}

const juce::StringArray waveformLabels(bool oscA)
{
    return oscA ? juce::StringArray { "Triangle", "Rect 50%", "Rect 30%",
                                      "Rect 15%", "Rect 6%", "Sawtooth" }
                : juce::StringArray { "Triangle", "Rect 40%", "Rect 20%",
                                      "Rect 10%", "Rect 3%", "Sawtooth" };
}

float loadTravel(const juce::AudioProcessorValueTreeState& state,
                 const char* id) noexcept
{
    if (auto* raw = state.getRawParameterValue(id))
        return raw->load();
    return 0.0f;
}

int loadIndex(const juce::AudioProcessorValueTreeState& state,
              const char* id) noexcept
{
    if (auto* raw = state.getRawParameterValue(id))
        return static_cast<int>(std::lround(raw->load()));
    return 0;
}

bool loadRocker(const juce::AudioProcessorValueTreeState& state,
                const char* id) noexcept
{
    if (auto* raw = state.getRawParameterValue(id))
        return raw->load() >= 0.5f;
    return false;
}
} // namespace

GhostarAudioProcessor::GhostarAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "GhostarParameters", createParameterLayout())
{
    keyboardState.addListener(this);
}

GhostarAudioProcessor::~GhostarAudioProcessor()
{
    keyboardState.removeListener(this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GhostarAudioProcessor::createParameterLayout()
{
    const ghostar::EngineParameters defaults {};
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(
        // MASTER
        travel(ids::tune, "Tune", defaults.tune),
        detents(ids::octave, "Octave", { "32'", "16'", "8'", "4'" },
                static_cast<int>(defaults.octave)),
        // OSCILLATOR A
        detents(ids::oscAWaveform, "Osc A Waveform", waveformLabels(true),
                static_cast<int>(defaults.oscAWaveform)),
        rocker(ids::sync, "Sync", defaults.sync),
        // OSCILLATOR B
        detents(ids::oscBWaveform, "Osc B Waveform", waveformLabels(false),
                static_cast<int>(defaults.oscBWaveform)),
        detents(ids::oscBRange, "Osc B Octave/Range",
                { "-1", "Unison", "+1", "+2", "Bass", "Wide" },
                static_cast<int>(defaults.oscBRange)),
        travel(ids::interval, "Interval", defaults.interval),
        // TRIGGER / GATE SELECT
        detents(ids::trigger, "Trigger", { "Single", "Multiple" },
                static_cast<int>(defaults.trigger)),
        rocker(ids::gateKbd, "Gate Kbd", defaults.gateKbd),
        rocker(ids::gateX, "Gate X", defaults.gateX),
        rocker(ids::gateYExt, "Gate Y/Ext", defaults.gateYExt),
        // MOD X
        detents(ids::arpeggiator, "Arpeggiator",
                { "Off", "Ripple", "Arpeggio", "Leap" },
                static_cast<int>(defaults.arpeggiator)),
        detents(ids::modSource, "Mod Source",
                { "LFO Triangle", "LFO Square", "S+H Random", "S+H Y",
                  "Red Noise", "Osc B" },
                static_cast<int>(defaults.modSource)),
        travel(ids::lfoRate, "LFO/S+H Rate", defaults.lfoRate),
        // SHAPER Y
        detents(ids::shaperMode, "Shaper Mode",
                { "Free", "KBD Hold", "Reset", "Run" },
                static_cast<int>(defaults.shaperMode)),
        travel(ids::shaperShape, "Shaper Shape", defaults.shaperShape),
        travel(ids::shaperRate, "Shaper Rate", defaults.shaperRate));

    layout.add(
        // WHEEL DESTINATIONS
        detents(ids::modXTo, "Mod X To",
                { "Off", "Osc A+B", "Osc A", "Osc A RWM", "Filt U+L",
                  "Filt U" },
                static_cast<int>(defaults.modXTo)),
        rocker(ids::shapeXWithY, "Shape X With Y", defaults.shapeXWithY),
        detents(ids::shaperYTo, "Shaper Y To",
                { "Off", "Osc A+B", "Osc B", "Osc B RWM", "LFO Rate",
                  "Filt L" },
                static_cast<int>(defaults.shaperYTo)),
        // AUDIO MIXER
        travel(ids::masterVolume, "Master Volume", defaults.masterVolume),
        travel(ids::brightness, "Brightness", defaults.brightness),
        travel(ids::shaperPathA, "Shaper Path A", defaults.shaperPathA),
        travel(ids::shaperPathB, "Shaper Path B", defaults.shaperPathB),
        travel(ids::shaperPathRing, "Shaper Path Ring",
               defaults.shaperPathRing),
        travel(ids::shaperPathNoise, "Shaper Path Noise",
               defaults.shaperPathNoise),
        travel(ids::filterPathA, "Filter Path A", defaults.filterPathA),
        travel(ids::filterPathB, "Filter Path B", defaults.filterPathB),
        travel(ids::filterPathNoise, "Filter Path Noise",
               defaults.filterPathNoise));

    layout.add(
        // FILTERS
        travel(ids::cutoff, "Master Cutoff", defaults.cutoff),
        travel(ids::lowerOnly, "Lower Only Cutoff", defaults.lowerOnly),
        detents(ids::upperResonance, "Upper Resonance", { "Low", "Variable" },
                static_cast<int>(defaults.upperResonance)),
        travel(ids::resonance, "Resonance", defaults.resonance),
        detents(ids::slope, "Slope", { "12 dB", "24 dB" },
                static_cast<int>(defaults.slope)),
        travel(ids::kbAmount, "KB Amount", defaults.kbAmount),
        detents(ids::lowerMode, "Lower Filter Mode",
                { "Out", "Overdrive", "Band-Pass", "High Pass" },
                static_cast<int>(defaults.lowerMode)),
        detents(ids::tracking, "Tracking", { "Formant", "Dynamic" },
                static_cast<int>(defaults.tracking)),
        // FILTER ENVELOPE
        travel(ids::filterEnvAmount, "Filter Env Amount",
               defaults.filterEnvAmount),
        travel(ids::filterAttack, "Filter Attack", defaults.filterAttack),
        travel(ids::filterDecay, "Filter Decay", defaults.filterDecay),
        travel(ids::filterSustain, "Filter Sustain", defaults.filterSustain),
        travel(ids::filterRelease, "Filter Release", defaults.filterRelease),
        // LOUDNESS ENVELOPE
        rocker(ids::vcaBypass, "VCA Bypass", defaults.vcaBypass),
        travel(ids::loudnessAttack, "Loudness Attack",
               defaults.loudnessAttack),
        travel(ids::loudnessDecay, "Loudness Decay", defaults.loudnessDecay),
        travel(ids::loudnessSustain, "Loudness Sustain",
               defaults.loudnessSustain),
        travel(ids::loudnessRelease, "Loudness Release",
               defaults.loudnessRelease),
        // Performance
        travel(ids::glide, "Glide", defaults.glide),
        detents(ids::glideMode, "Glide Mode", { "Off", "Auto", "On" },
                static_cast<int>(defaults.glideMode)),
        travel(ids::xWheel, "Mod X Wheel", 0.0f),
        travel(ids::yWheel, "Shaper Y Wheel", 0.0f),
        rocker(ids::splitPaths, "Split Paths", defaults.splitPaths));

    return layout;
}

void GhostarAudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= ghostar::factoryPresetCount())
        return;
    currentProgram = index;

    const auto preset = ghostar::factoryPresetParameters(index);
    namespace ids = ghostar::parameters;

    const auto setTravel = [this](const char* id, float value) {
        if (auto* parameter = parameters.getParameter(id))
            parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, value));
    };
    const auto setDetent = [this](const char* id, int position, int count) {
        if (auto* parameter = parameters.getParameter(id))
            parameter->setValueNotifyingHost(
                count > 1 ? static_cast<float>(position)
                                / static_cast<float>(count - 1)
                          : 0.0f);
    };
    const auto setRocker = [this](const char* id, bool on) {
        if (auto* parameter = parameters.getParameter(id))
            parameter->setValueNotifyingHost(on ? 1.0f : 0.0f);
    };

    setTravel(ids::tune, preset.tune);
    setDetent(ids::octave, static_cast<int>(preset.octave), 4);
    setDetent(ids::oscAWaveform, static_cast<int>(preset.oscAWaveform), 6);
    setRocker(ids::sync, preset.sync);
    setDetent(ids::oscBWaveform, static_cast<int>(preset.oscBWaveform), 6);
    setDetent(ids::oscBRange, static_cast<int>(preset.oscBRange), 6);
    setTravel(ids::interval, preset.interval);
    setDetent(ids::trigger, static_cast<int>(preset.trigger), 2);
    setRocker(ids::gateKbd, preset.gateKbd);
    setRocker(ids::gateX, preset.gateX);
    setRocker(ids::gateYExt, preset.gateYExt);
    setDetent(ids::arpeggiator, static_cast<int>(preset.arpeggiator), 4);
    setDetent(ids::modSource, static_cast<int>(preset.modSource), 6);
    setTravel(ids::lfoRate, preset.lfoRate);
    setDetent(ids::shaperMode, static_cast<int>(preset.shaperMode), 4);
    setTravel(ids::shaperShape, preset.shaperShape);
    setTravel(ids::shaperRate, preset.shaperRate);
    setDetent(ids::modXTo, static_cast<int>(preset.modXTo), 6);
    setRocker(ids::shapeXWithY, preset.shapeXWithY);
    setDetent(ids::shaperYTo, static_cast<int>(preset.shaperYTo), 6);
    setTravel(ids::masterVolume, preset.masterVolume);
    setTravel(ids::brightness, preset.brightness);
    setTravel(ids::shaperPathA, preset.shaperPathA);
    setTravel(ids::shaperPathB, preset.shaperPathB);
    setTravel(ids::shaperPathRing, preset.shaperPathRing);
    setTravel(ids::shaperPathNoise, preset.shaperPathNoise);
    setTravel(ids::filterPathA, preset.filterPathA);
    setTravel(ids::filterPathB, preset.filterPathB);
    setTravel(ids::filterPathNoise, preset.filterPathNoise);
    setTravel(ids::cutoff, preset.cutoff);
    setTravel(ids::lowerOnly, preset.lowerOnly);
    setDetent(ids::upperResonance, static_cast<int>(preset.upperResonance), 2);
    setTravel(ids::resonance, preset.resonance);
    setDetent(ids::slope, static_cast<int>(preset.slope), 2);
    setTravel(ids::kbAmount, preset.kbAmount);
    setDetent(ids::lowerMode, static_cast<int>(preset.lowerMode), 4);
    setDetent(ids::tracking, static_cast<int>(preset.tracking), 2);
    setTravel(ids::filterEnvAmount, preset.filterEnvAmount);
    setTravel(ids::filterAttack, preset.filterAttack);
    setTravel(ids::filterDecay, preset.filterDecay);
    setTravel(ids::filterSustain, preset.filterSustain);
    setTravel(ids::filterRelease, preset.filterRelease);
    setRocker(ids::vcaBypass, preset.vcaBypass);
    setTravel(ids::loudnessAttack, preset.loudnessAttack);
    setTravel(ids::loudnessDecay, preset.loudnessDecay);
    setTravel(ids::loudnessSustain, preset.loudnessSustain);
    setTravel(ids::loudnessRelease, preset.loudnessRelease);
    setTravel(ids::glide, preset.glide);
    setDetent(ids::glideMode, static_cast<int>(preset.glideMode), 3);
    setRocker(ids::splitPaths, preset.splitPaths);
    // Every chart begins with the performance wheels fully back.
    setTravel(ids::xWheel, 0.0f);
    setTravel(ids::yWheel, 0.0f);
}

void GhostarAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
    // The voice runs at 4x and comes back through two linear-phase halfband
    // stages, which delay it by a fixed 34.5 samples at any host rate. Told
    // nothing, a host assumes zero and lands Ghostar a third of a
    // millisecond behind everything it is layered with. The delay is not a
    // whole number of samples, so the nearest one is what can be published;
    // half a sample is what the host cannot compensate.
    setLatencySamples(juce::roundToInt(
        ghostar::GhostarEngine::outputLatencySamples()));
    updateEngineParameters();
}

void GhostarAudioProcessor::releaseResources() {}

bool GhostarAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void GhostarAudioProcessor::updateEngineParameters() noexcept
{
    ghostar::EngineParameters engineParameters;
    engineParameters.tune = loadTravel(parameters, ids::tune);
    engineParameters.octave =
        static_cast<ghostar::MasterOctave>(loadIndex(parameters, ids::octave));
    engineParameters.oscAWaveform =
        static_cast<ghostar::Waveform>(loadIndex(parameters, ids::oscAWaveform));
    engineParameters.sync = loadRocker(parameters, ids::sync);
    engineParameters.oscBWaveform =
        static_cast<ghostar::Waveform>(loadIndex(parameters, ids::oscBWaveform));
    engineParameters.oscBRange =
        static_cast<ghostar::OscBRange>(loadIndex(parameters, ids::oscBRange));
    engineParameters.interval = loadTravel(parameters, ids::interval);
    engineParameters.trigger =
        static_cast<ghostar::TriggerMode>(loadIndex(parameters, ids::trigger));
    engineParameters.gateKbd = loadRocker(parameters, ids::gateKbd);
    engineParameters.gateX = loadRocker(parameters, ids::gateX);
    engineParameters.gateYExt = loadRocker(parameters, ids::gateYExt);
    engineParameters.arpeggiator = static_cast<ghostar::ArpeggiatorMode>(
        loadIndex(parameters, ids::arpeggiator));
    engineParameters.modSource =
        static_cast<ghostar::ModSource>(loadIndex(parameters, ids::modSource));
    engineParameters.lfoRate = loadTravel(parameters, ids::lfoRate);
    engineParameters.shaperMode =
        static_cast<ghostar::ShaperMode>(loadIndex(parameters, ids::shaperMode));
    engineParameters.shaperShape = loadTravel(parameters, ids::shaperShape);
    engineParameters.shaperRate = loadTravel(parameters, ids::shaperRate);
    engineParameters.modXTo =
        static_cast<ghostar::ModXDestination>(loadIndex(parameters, ids::modXTo));
    engineParameters.shapeXWithY = loadRocker(parameters, ids::shapeXWithY);
    engineParameters.shaperYTo = static_cast<ghostar::ShaperYDestination>(
        loadIndex(parameters, ids::shaperYTo));
    engineParameters.masterVolume = loadTravel(parameters, ids::masterVolume);
    engineParameters.brightness = loadTravel(parameters, ids::brightness);
    engineParameters.shaperPathA = loadTravel(parameters, ids::shaperPathA);
    engineParameters.shaperPathB = loadTravel(parameters, ids::shaperPathB);
    engineParameters.shaperPathRing =
        loadTravel(parameters, ids::shaperPathRing);
    engineParameters.shaperPathNoise =
        loadTravel(parameters, ids::shaperPathNoise);
    engineParameters.filterPathA = loadTravel(parameters, ids::filterPathA);
    engineParameters.filterPathB = loadTravel(parameters, ids::filterPathB);
    engineParameters.filterPathNoise =
        loadTravel(parameters, ids::filterPathNoise);
    engineParameters.cutoff = loadTravel(parameters, ids::cutoff);
    engineParameters.lowerOnly = loadTravel(parameters, ids::lowerOnly);
    engineParameters.upperResonance = static_cast<ghostar::UpperResonanceMode>(
        loadIndex(parameters, ids::upperResonance));
    engineParameters.resonance = loadTravel(parameters, ids::resonance);
    engineParameters.slope =
        static_cast<ghostar::UpperSlope>(loadIndex(parameters, ids::slope));
    engineParameters.kbAmount = loadTravel(parameters, ids::kbAmount);
    engineParameters.lowerMode = static_cast<ghostar::LowerFilterMode>(
        loadIndex(parameters, ids::lowerMode));
    engineParameters.tracking =
        static_cast<ghostar::TrackingMode>(loadIndex(parameters, ids::tracking));
    engineParameters.filterEnvAmount =
        loadTravel(parameters, ids::filterEnvAmount);
    engineParameters.filterAttack = loadTravel(parameters, ids::filterAttack);
    engineParameters.filterDecay = loadTravel(parameters, ids::filterDecay);
    engineParameters.filterSustain =
        loadTravel(parameters, ids::filterSustain);
    engineParameters.filterRelease =
        loadTravel(parameters, ids::filterRelease);
    engineParameters.vcaBypass = loadRocker(parameters, ids::vcaBypass);
    engineParameters.loudnessAttack =
        loadTravel(parameters, ids::loudnessAttack);
    engineParameters.loudnessDecay =
        loadTravel(parameters, ids::loudnessDecay);
    engineParameters.loudnessSustain =
        loadTravel(parameters, ids::loudnessSustain);
    engineParameters.loudnessRelease =
        loadTravel(parameters, ids::loudnessRelease);
    engineParameters.glide = loadTravel(parameters, ids::glide);
    engineParameters.glideMode =
        static_cast<ghostar::GlideMode>(loadIndex(parameters, ids::glideMode));
    // A mono bus has no right jack to split onto, and splitting there
    // would silently discard the whole Shaper path (the engine's right
    // output is dropped): the two paths stay summed instead.
    engineParameters.splitPaths = loadRocker(parameters, ids::splitPaths)
                               && getMainBusNumOutputChannels() >= 2;
    engine.setParameters(engineParameters);

    // The wheels are attenuators the player rides; hosts automate them as
    // parameters and MIDI CC1/CC2 write them back through the same lane.
    engine.setModWheel(loadTravel(parameters, ids::xWheel));
    engine.setShaperWheel(loadTravel(parameters, ids::yWheel));
}

void GhostarAudioProcessor::handleMidiMessage(
    const juce::MidiMessage& message) noexcept
{
    if (message.isNoteOn())
    {
        engine.noteOn(message.getNoteNumber(), message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        engine.noteOff(message.getNoteNumber());
    }
    else if (message.isPitchWheel())
    {
        const auto raw = message.getPitchWheelValue(); // 0..16383
        engine.setPitchBend(
            static_cast<float>(raw - 8192) / 8192.0f);
    }
    else if (message.isController())
    {
        const int controller = message.getControllerNumber();
        const float normalised =
            static_cast<float>(message.getControllerValue()) / 127.0f;
        // CC1 rides the X wheel and CC2 the Y wheel, through the same host
        // parameters the editor's wheels write — and straight into the
        // engine as well, because the parameters were already latched for
        // this block and the wheel must move at the event's own sample.
        if (controller == 1)
        {
            if (auto* parameter = parameters.getParameter(ids::xWheel))
                parameter->setValueNotifyingHost(normalised);
            engine.setModWheel(normalised);
        }
        else if (controller == 2)
        {
            if (auto* parameter = parameters.getParameter(ids::yWheel))
                parameter->setValueNotifyingHost(normalised);
            engine.setShaperWheel(normalised);
        }
        else if (controller == 120) // all sound off: stop, keep controllers
        {
            engine.stopAllSound();
        }
        else if (controller == 123) // all notes off: release through envelopes
        {
            engine.releaseAllKeys();
        }
    }
}

void GhostarAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (panicRequested.exchange(false, std::memory_order_acq_rel))
    {
        // Notes queued from the on-screen keyboard before the panic click
        // must not replay after the reset. Only up to the click's own
        // snapshot: a key pressed after the click is a fresh note and
        // stays queued. Forward only — if the click raced the previous
        // block's dispatch, the queue may already stand past the snapshot,
        // and rewinding would replay consumed (possibly overwritten) slots.
        const unsigned dropBefore =
            panicDropBefore.load(std::memory_order_relaxed);
        const unsigned readNow = uiReadIndex.load(std::memory_order_relaxed);
        if (static_cast<int>(dropBefore - readNow) > 0)
            uiReadIndex.store(dropBefore, std::memory_order_release);
        engine.reset();
        // The reset centred the engine's bend, so the tracker must agree —
        // a wheel still held off-centre then reapplies itself next block.
        lastAppliedUiBend = 0.0f;
    }

    dispatchUiMidiEvents();
    updateEngineParameters();

    // The editor's wheel only writes when it moves, so a held MIDI bend from
    // the host is not stomped by a resting on-screen wheel.
    const float uiBend = uiPitchBend.load(std::memory_order_relaxed);
    if (uiBend != lastAppliedUiBend)
    {
        lastAppliedUiBend = uiBend;
        engine.setPitchBend(uiBend);
    }

    const int numSamples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1)
                                              : nullptr;

    // Segment-accurate event handling: render up to each event's position,
    // apply it, continue — so a gate change lands on its own sample.
    int renderedUpTo = 0;
    std::array<float, 2048> scratch {};
    const auto renderSegment = [&](int upTo) {
        while (renderedUpTo < upTo)
        {
            const int count = std::min(upTo - renderedUpTo,
                                       static_cast<int>(scratch.size()));
            float* rightTarget =
                right != nullptr ? right + renderedUpTo : scratch.data();
            engine.process(left + renderedUpTo, rightTarget, count);
            renderedUpTo += count;
        }
    };

    for (const auto metadata : midiMessages)
    {
        renderSegment(std::clamp(metadata.samplePosition, 0, numSamples));
        handleMidiMessage(metadata.getMessage());
    }
    renderSegment(numSamples);

    for (int channel = 2; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    // The lamp means "the envelopes are being held open", which is the
    // OR'ed gate bus and not the keyboard: an X- or Y-gated patch
    // articulates with no key down, and a key with KBD deselected
    // articulates nothing.
    gateOpenForDisplay.store(engine.isEnvelopeGateOpen(),
                             std::memory_order_relaxed);
}

void GhostarAudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int,
                                       int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent(midiNoteNumber, velocity, true);
}

void GhostarAudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int,
                                        int midiNoteNumber, float)
{
    enqueueUiMidiEvent(midiNoteNumber, 0.0f, false);
}

void GhostarAudioProcessor::enqueueUiMidiEvent(int note, float velocity,
                                             bool isNoteOn) noexcept
{
    const auto write = uiWriteIndex.load(std::memory_order_relaxed);
    const auto read = uiReadIndex.load(std::memory_order_acquire);
    if (write - read >= uiQueueCapacity)
        return; // a full queue drops the event rather than blocking the UI
    uiMidiQueue[write % uiQueueCapacity] =
        UiMidiEvent { note, velocity, isNoteOn };
    uiWriteIndex.store(write + 1, std::memory_order_release);
}

void GhostarAudioProcessor::dispatchUiMidiEvents() noexcept
{
    auto read = uiReadIndex.load(std::memory_order_relaxed);
    const auto write = uiWriteIndex.load(std::memory_order_acquire);
    while (read != write)
    {
        const auto& event = uiMidiQueue[read % uiQueueCapacity];
        if (event.noteOn)
            engine.noteOn(event.note, event.velocity);
        else
            engine.noteOff(event.note);
        ++read;
    }
    uiReadIndex.store(read, std::memory_order_release);
}

void GhostarAudioProcessor::getStateInformation(
    juce::MemoryBlock& destinationData)
{
    if (auto state = parameters.copyState(); state.isValid())
    {
        // The program index rides along so a restored session keeps its
        // program name; the parameters themselves are the sound.
        state.setProperty("program", currentProgram, nullptr);
        if (const auto xml = state.createXml())
            copyXmlToBinary(*xml, destinationData);
    }
}

void GhostarAudioProcessor::setStateInformation(const void* data,
                                              int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
        {
            const auto restored = juce::ValueTree::fromXml(*xml);
            // Restore the label only, not the preset's values: the saved
            // parameters may have been edited after the program was picked.
            const int program = restored.getProperty("program", 0);
            if (program >= 0 && program < ghostar::factoryPresetCount())
                currentProgram = program;
            parameters.replaceState(restored);
        }
}

juce::AudioProcessorEditor* GhostarAudioProcessor::createEditor()
{
    return new GhostarAudioProcessorEditor(*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GhostarAudioProcessor();
}
