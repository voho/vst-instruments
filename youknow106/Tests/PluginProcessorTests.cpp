#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using namespace youknow106;

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

// The extension defaults preserve the base compatibility model: no velocity
// response, nominal zero Unit Character, six voices, and the delay lines' still
// voiced noise at its modelled level.
constexpr auto expectedParameters = std::to_array<ParameterExpectation> ({
    { parameters::volume,      0.80f,  1.0e-5f },
    { parameters::benderDco,   0.30f,  1.0e-5f },
    { parameters::benderVcf,   0.0f,   1.0e-5f },
    { parameters::benderLfo,   0.0f,   1.0e-5f },
    { parameters::portamento,  0.0f,   1.0e-5f },
    { parameters::legacyKeyMode, 0.0f, 1.0e-5f },
    { parameters::legacyChorus,  0.0f, 1.0e-5f },
    { parameters::poly1,       1.0f,   1.0e-5f },
    { parameters::poly2,       0.0f,   1.0e-5f },
    { parameters::lfoRate,     0.42f,  1.0e-5f },
    { parameters::lfoDelay,    0.0f,   1.0e-5f },
    { parameters::dcoLfo,      0.0f,   1.0e-5f },
    { parameters::pwm,         0.30f,  1.0e-5f },
    { parameters::pwmMode,     1.0f,   1.0e-5f },
    { parameters::range,       1.0f,   1.0e-5f },
    { parameters::saw,         1.0f,   1.0e-5f },
    { parameters::pulse,       0.0f,   1.0e-5f },
    { parameters::sub,         0.0f,   1.0e-5f },
    { parameters::noise,       0.0f,   1.0e-5f },
    { parameters::highPass,    1.0f,   1.0e-5f },
    { parameters::cutoff,      0.62f,  1.0e-5f },
    { parameters::resonance,   0.10f,  1.0e-5f },
    { parameters::envPolarity, 0.0f,   1.0e-5f },
    { parameters::vcfEnv,      0.35f,  1.0e-5f },
    { parameters::vcfLfo,      0.0f,   1.0e-5f },
    { parameters::keyFollow,   0.50f,  1.0e-5f },
    { parameters::vcaMode,     0.0f,   1.0e-5f },
    { parameters::vcaLevel,    0.80f,  1.0e-5f },
    { parameters::attack,      0.0f,   1.0e-5f },
    { parameters::decay,       0.45f,  1.0e-5f },
    { parameters::sustain,     0.70f,  1.0e-5f },
    { parameters::release,     0.30f,  1.0e-5f },
    { parameters::chorusI,     0.0f,   1.0e-5f },
    { parameters::chorusII,    0.0f,   1.0e-5f },
    { parameters::transpose,   0.0f,   1.0e-5f },
    { parameters::masterTune,  0.0f,   1.0e-5f },
    { parameters::velocity,    0.0f,   1.0e-5f },
    { parameters::calibration, 1.0f,   1.0e-5f },
    { parameters::chorusNoise, 1.0f,   1.0e-5f },
    { parameters::polyphony,   6.0f,   1.0e-5f },
    { parameters::hq,          1.0f,   1.0e-5f },
});

float parameterValue (const YouKnow106AudioProcessor& processor, const char* id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, std::string ("missing parameter ") + id);
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (YouKnow106AudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot set missing parameter ") + id);
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

bool bufferIsFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);
        for (int index = 0; index < buffer.getNumSamples(); ++index)
            if (! std::isfinite (samples[index]))
                return false;
    }
    return true;
}

float bufferPeak (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = std::max (peak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    return peak;
}

juce::Image renderEditorSnapshot (juce::AudioProcessorEditor& editor)
{
    juce::Image snapshot (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
    juce::Graphics graphics (snapshot);
    editor.paintEntireComponent (graphics, true);
    return snapshot;
}

juce::Component* findDescendantNamed (juce::Component& parent,
                                      const juce::String& name)
{
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        auto* child = parent.getChildComponent (index);
        if (child->getName() == name)
            return child;
        if (auto* match = findDescendantNamed (*child, name))
            return match;
    }
    return nullptr;
}

template <typename ComponentType>
void collectDescendantsOfType (juce::Component& parent,
                               std::vector<ComponentType*>& result)
{
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        auto* child = parent.getChildComponent (index);
        if (auto* wanted = dynamic_cast<ComponentType*> (child))
            result.push_back (wanted);
        collectDescendantsOfType (*child, result);
    }
}

bool containsTooltipWindow (juce::Component& parent)
{
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        auto* child = parent.getChildComponent (index);
        if (dynamic_cast<juce::TooltipWindow*> (child) != nullptr
            || containsTooltipWindow (*child))
            return true;
    }
    return false;
}

juce::MouseEvent mouseEventFor (juce::Component& component,
                                juce::Point<float> position,
                                juce::ModifierKeys modifiers)
{
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position, modifiers, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
             &component, &component, now, position, now, 1, false };
}

juce::Label* findDescendantLabelWithText (juce::Component& parent,
                                          const juce::String& text)
{
    const auto wanted = text.removeCharacters (" \t\r\n");
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        auto* child = parent.getChildComponent (index);
        if (auto* label = dynamic_cast<juce::Label*> (child);
            label != nullptr
                && label->getText().removeCharacters (" \t\r\n") == wanted)
            return label;
        if (auto* match = findDescendantLabelWithText (*child, text))
            return match;
    }
    return nullptr;
}

juce::Label* findSliderTextBox (juce::Slider& slider)
{
    for (int index = 0; index < slider.getNumChildComponents(); ++index)
        if (auto* label = dynamic_cast<juce::Label*> (
                slider.getChildComponent (index)))
            return label;
    return nullptr;
}

float realTextWidth (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, text);
}

bool labelTextFitsAtItsDeclaredSize (const juce::Label& label)
{
    const auto available = label.getLocalBounds().toFloat();
    const auto& font = label.getFont();
    if (available.getWidth() <= 0.0f || available.getHeight() < font.getHeight())
        return false;

    if (realTextWidth (font, label.getText()) <= available.getWidth())
        return true;

    // The editor's label renderer deliberately permits two lines. Long utility
    // captions therefore still count as full-size text when each word fits a
    // line and the component has room for both lines; relying on JUCE's final
    // horizontal squeeze would make the nominal font-size check meaningless.
    juce::StringArray words;
    words.addTokens (label.getText(), " \t\r\n", "");
    if (words.size() < 2 || available.getHeight() < font.getHeight() * 1.8f)
        return false;

    for (const auto& word : words)
        if (realTextWidth (font, word) > available.getWidth())
            return false;
    return true;
}

bool isRotaryStyle (juce::Slider::SliderStyle style) noexcept
{
    return style == juce::Slider::Rotary
        || style == juce::Slider::RotaryHorizontalVerticalDrag
        || style == juce::Slider::RotaryVerticalDrag;
}

juce::Button* findDescendantButtonWithText (juce::Component& parent,
                                            const juce::String& text)
{
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        auto* child = parent.getChildComponent (index);
        if (auto* button = dynamic_cast<juce::Button*> (child);
            button != nullptr && button->getButtonText() == text)
            return button;
        if (auto* match = findDescendantButtonWithText (*child, text))
            return match;
    }
    return nullptr;
}

int countDescendantButtonsWithText (juce::Component& parent,
                                    const juce::String& text)
{
    int count = 0;
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        auto* child = parent.getChildComponent (index);
        if (auto* button = dynamic_cast<juce::Button*> (child);
            button != nullptr && button->getButtonText() == text)
            ++count;
        count += countDescendantButtonsWithText (*child, text);
    }
    return count;
}

bool hasDescendantButtonWithTextPrefix (juce::Component& parent,
                                        const juce::String& prefix)
{
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        auto* child = parent.getChildComponent (index);
        if (auto* button = dynamic_cast<juce::Button*> (child);
            button != nullptr
                && button->getButtonText().startsWithIgnoreCase (prefix))
            return true;
        if (hasDescendantButtonWithTextPrefix (*child, prefix))
            return true;
    }
    return false;
}

juce::ComboBox* findDescendantComboBox (juce::Component& parent)
{
    for (int index = 0; index < parent.getNumChildComponents(); ++index)
    {
        auto* child = parent.getChildComponent (index);
        if (auto* combo = dynamic_cast<juce::ComboBox*> (child))
            return combo;
        if (auto* match = findDescendantComboBox (*child))
            return match;
    }
    return nullptr;
}

struct HostChangeRecorder final : juce::AudioProcessorListener
{
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}

    void audioProcessorChanged (
        juce::AudioProcessor*, const ChangeDetails& details) override
    {
        ++changeCount;
        lastDetails = details;
    }

    void clear()
    {
        changeCount = 0;
        lastDetails = {};
    }

    int changeCount { 0 };
    ChangeDetails lastDetails {};
};

float maximumBufferDifference (const juce::AudioBuffer<float>& first,
                               const juce::AudioBuffer<float>& second)
{
    if (first.getNumChannels() != second.getNumChannels()
        || first.getNumSamples() != second.getNumSamples())
        return std::numeric_limits<float>::infinity();

    float difference = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            difference = std::max (
                difference,
                std::abs (first.getSample (channel, sample)
                          - second.getSample (channel, sample)));
    return difference;
}

// A panel that rendered as one flat colour would still be "valid"; what we
// actually want to know is that it drew something.
bool snapshotHasDetail (const juce::Image& snapshot)
{
    if (! snapshot.isValid())
        return false;

    std::set<juce::uint32> distinct;
    for (int y = 0; y < snapshot.getHeight(); y += 4)
        for (int x = 0; x < snapshot.getWidth(); x += 4)
        {
            const auto pixel = snapshot.getPixelAt (x, y);
            if (pixel.getAlpha() < 250)
                return false;
            distinct.insert (pixel.getARGB());
            if (distinct.size() > 64)
                return true;
        }
    return distinct.size() > 8;
}

void renderBlocks (YouKnow106AudioProcessor& processor, juce::AudioBuffer<float>& buffer,
                   int blocks)
{
    juce::MidiBuffer midi;
    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
    }
}

// --------------------------------------------------------------------------

void testParameterContract()
{
    YouKnow106AudioProcessor processor;

    expect (processor.getParameters().size() == static_cast<int> (expectedParameters.size()),
            "parameter count changed without the contract being updated");

    std::set<juce::String> seen;
    for (const auto& expected : expectedParameters)
    {
        const auto* parameter = processor.parameters.getParameter (expected.id);
        expect (parameter != nullptr, std::string ("missing parameter ") + expected.id);
        if (parameter == nullptr)
            continue;

        expect (seen.insert (expected.id).second,
                std::string ("duplicate parameter ") + expected.id);
        expect (std::abs (parameterValue (processor, expected.id) - expected.defaultValue)
                    <= expected.tolerance,
                std::string ("unexpected default for ") + expected.id);
        expect (parameter->getName (64).isNotEmpty(),
                std::string ("parameter has no name: ") + expected.id);
    }

    // Every control the panel names has to exist, or the editor would attach to
    // nothing and the layout check would be testing a fiction.
    for (const auto& control : panel::controls())
        expect (processor.parameters.getParameter (control.parameterId) != nullptr,
                std::string ("panel names a parameter the processor does not have: ")
                    + control.parameterId);

    if (const auto* character = processor.parameters.getParameter (parameters::calibration))
    {
        expect (character->getName (64) == "Unit Character",
                "the compatibility id is not presented as Unit Character");
        constexpr int historicalCalibrationHostIndex = 33;
        expect (processor.getParameters().size() > historicalCalibrationHostIndex
                    && processor.getParameters()[historicalCalibrationHostIndex]
                           == character,
                "the calibration compatibility parameter moved from host index 33");
    }
}

void testParameterTextRoundTrips()
{
    YouKnow106AudioProcessor processor;

    // Panel positions that stand for a time or a frequency must display the
    // value the modelled circuit produces, not the slider's own travel.
    const auto textFor = [&processor] (const char* id, float value) {
        auto* parameter = processor.parameters.getParameter (id);
        return parameter != nullptr
                   ? parameter->getText (parameter->convertTo0to1 (value), 64)
                   : juce::String();
    };

    expect (textFor (parameters::attack, 0.0f) == "4 ms",
            "the shortest attack does not show its one-pass duration");
    expect (textFor (parameters::decay, 1.0f).contains ("s"),
            "the longest decay is not shown in seconds");
    expect (textFor (parameters::cutoff, 1.0f).contains ("kHz"),
            "a wide-open filter is not shown in kilohertz");
    expect (textFor (parameters::lfoRate, 0.0f).contains ("Hz"),
            "the modulation rate is not shown in hertz");
    expect (textFor (parameters::portamento, 0.0f) == "OFF",
            "portamento at rest is not shown as switched off");
    expect (textFor (parameters::portamento, 1.0f).contains ("/oct"),
            "portamento is not shown per octave");
    expect (textFor (parameters::masterTune, -50.0f) == "-50.0 ct"
                && textFor (parameters::masterTune, 12.3f) == "+12.3 ct",
            "Master Tune does not use the compact signed-cents display");
    if (auto* tune = processor.parameters.getParameter (parameters::masterTune))
    {
        const float parsed = tune->convertFrom0to1 (
            tune->getValueForText ("+12.3 ct"));
        expect (std::abs (parsed - 12.3f) < 0.051f,
                "Master Tune cannot parse the compact cents display");
    }
}

void testProcessingProducesSound()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    buffer.clear();
    processor.processBlock (buffer, midi);

    renderBlocks (processor, buffer, 40);
    expect (bufferIsFinite (buffer), "the processor emitted a non-finite sample");
    expect (bufferPeak (buffer) > 0.001f, "a held note produced silence");
    expect (processor.getActiveVoiceCount() == 1, "one held key is not one voice");
    expect (processor.isEngineReady(), "the engine did not report itself ready");

    // Note off, then long enough for the release and the delay lines to settle.
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    buffer.clear();
    processor.processBlock (buffer, off);
    renderBlocks (processor, buffer, 400);
    expect (processor.getActiveVoiceCount() == 0, "the voice never released");

    processor.releaseResources();
}

void testShortNoteInsideOneBlockIsHeard()
{
    // A note that opens and closes inside a single buffer must still sound.
    // Dispatching a block's MIDI at its boundary would apply both events before
    // any audio was rendered and lose the note entirely.
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::attack, 0.0f);
    setParameterValue (processor, parameters::sustain, 1.0f);
    setParameterValue (processor, parameters::release, 0.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), blockSize - 1);
    buffer.clear();
    processor.processBlock (buffer, midi);

    expect (bufferPeak (buffer) > 0.0f,
            "a note contained inside one block produced no audio at all");

    // The two events must also land where they were timed: an event at the end
    // of the block cannot affect the start of it.
    juce::AudioBuffer<float> late (2, blockSize);
    juce::MidiBuffer lateMidi;
    lateMidi.addEvent (juce::MidiMessage::noteOn (1, 72, 1.0f), blockSize - 2);
    late.clear();
    processor.releaseResources();
    processor.prepareToPlay (sampleRate, blockSize);
    processor.processBlock (late, lateMidi);

    float earlyPeak = 0.0f;
    for (int index = 0; index < blockSize / 2; ++index)
        earlyPeak = std::max (earlyPeak, std::abs (late.getSample (0, index)));
    expect (earlyPeak == 0.0f,
            "an event timed at the end of a block was applied at its start");

    processor.releaseResources();
}

void testUiKeyboardPressAndReleaseIsHeard()
{
    // The on-screen keyboard's events carry no sample position, so they are
    // applied at the block boundary. A press and its release arriving in the
    // same drain -- which is what happens after the audio thread has been held
    // up -- would then cancel before anything was rendered.
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::attack, 0.0f);
    setParameterValue (processor, parameters::sustain, 1.0f);
    setParameterValue (processor, parameters::release, 0.0f);

    processor.keyboardState.noteOn (1, 60, 1.0f);
    processor.keyboardState.noteOff (1, 60, 0.0f);

    float peak = 0.0f;
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < 4; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        peak = std::max (peak, bufferPeak (buffer));
    }

    expect (peak > 0.0f,
            "a keyboard press and release in one drain produced no audio at all");
    processor.releaseResources();
}

void testDeferredQualitySwitchIsNotAutomatable()
{
    // The engine holds a quality change until the output path is quiet, so an
    // automation point would not take effect where it was written.
    YouKnow106AudioProcessor processor;
    const auto* parameter = processor.parameters.getParameter (parameters::hq);
    expect (parameter != nullptr, "the quality switch is missing");
    if (parameter != nullptr)
        expect (! parameter->isAutomatable(),
                "the deferred quality switch is offered to the host as automatable");
}

void testAllNotesOffReleasesAndAllSoundOffCuts()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::release, 0.75f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    buffer.clear();
    processor.processBlock (buffer, midi);
    renderBlocks (processor, buffer, 16);

    juce::MidiBuffer notesOff;
    notesOff.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    buffer.clear();
    processor.processBlock (buffer, notesOff);
    renderBlocks (processor, buffer, 8);
    expect (bufferPeak (buffer) > 0.001f,
            "all-notes-off cut the release instead of letting it ring");

    juce::MidiBuffer soundOff;
    soundOff.addEvent (juce::MidiMessage::allSoundOff (1), 0);
    buffer.clear();
    processor.processBlock (buffer, soundOff);
    renderBlocks (processor, buffer, 4);
    expect (processor.getActiveVoiceCount() == 0,
            "all-sound-off left a voice running");

    processor.releaseResources();
}

void testTransportOfControllers()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::release, 0.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, 1.0f), 1);
    buffer.clear();
    processor.processBlock (buffer, midi);
    renderBlocks (processor, buffer, 8);

    juce::MidiBuffer release;
    release.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
    buffer.clear();
    processor.processBlock (buffer, release);
    renderBlocks (processor, buffer, 20);
    expect (processor.getActiveVoiceCount() == 1,
            "the hold controller did not hold the note");

    juce::MidiBuffer lift;
    lift.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    buffer.clear();
    processor.processBlock (buffer, lift);
    renderBlocks (processor, buffer, 200);
    expect (processor.getActiveVoiceCount() == 0,
            "the note did not release when hold was lifted");

    processor.releaseResources();
}

void testUiPerformanceLeverMatchesMidiAndCoalesces()
{
    YouKnow106AudioProcessor uiDriven;
    YouKnow106AudioProcessor midiDriven;
    YouKnow106AudioProcessor neutral;
    for (auto* processor : { &uiDriven, &midiDriven, &neutral })
    {
        setParameterValue (*processor, parameters::calibration, 0.0f);
        setParameterValue (*processor, parameters::chorusNoise, 0.0f);
        setParameterValue (*processor, parameters::chorusI, 0.0f);
        setParameterValue (*processor, parameters::chorusII, 0.0f);
        setParameterValue (*processor, parameters::benderDco, 0.8f);
        setParameterValue (*processor, parameters::benderVcf, 0.65f);
        setParameterValue (*processor, parameters::benderLfo, 1.0f);
        setParameterValue (*processor, parameters::cutoff, 0.36f);
        setParameterValue (*processor, parameters::dcoLfo, 0.0f);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    uiDriven.postUiLeverPosition (1.0f, 1.0f);
    juce::AudioBuffer<float> uiBuffer (2, blockSize);
    juce::AudioBuffer<float> midiBuffer (2, blockSize);
    juce::AudioBuffer<float> neutralBuffer (2, blockSize);
    float uiToMidiDifference = 0.0f;
    float drivenToNeutralDifference = 0.0f;

    for (int block = 0; block < 24; ++block)
    {
        juce::MidiBuffer uiMidi, midi, neutralMidi;
        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
            for (auto* events : { &uiMidi, &midi, &neutralMidi })
                events->addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 1);
        }

        uiBuffer.clear();
        midiBuffer.clear();
        neutralBuffer.clear();
        uiDriven.processBlock (uiBuffer, uiMidi);
        midiDriven.processBlock (midiBuffer, midi);
        neutral.processBlock (neutralBuffer, neutralMidi);
        uiToMidiDifference = std::max (
            uiToMidiDifference, maximumBufferDifference (uiBuffer, midiBuffer));
        drivenToNeutralDifference = std::max (
            drivenToNeutralDifference,
            maximumBufferDifference (uiBuffer, neutralBuffer));
    }

    expect (uiToMidiDifference < 1.0e-6f,
            "the UI pitch/mod lever does not match Pitch Wheel plus CC1");
    expect (drivenToNeutralDifference > 1.0e-3f,
            "the UI pitch/mod lever had no audible effect");

    // Thousands of mouse moves may arrive between two audio blocks. The
    // mailbox must coalesce them and retain the final exact spring return.
    for (int index = 0; index < 10000; ++index)
    {
        const float phase = static_cast<float> (index % 201) / 100.0f - 1.0f;
        uiDriven.postUiLeverPosition (phase,
                                      static_cast<float> (index % 128) / 127.0f);
    }
    uiDriven.postUiLeverPosition (0.0f, 0.0f);

    float returnDifference = 0.0f;
    for (int block = 0; block < 16; ++block)
    {
        juce::MidiBuffer uiMidi, midi;
        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 0), 0);
        }
        uiBuffer.clear();
        midiBuffer.clear();
        uiDriven.processBlock (uiBuffer, uiMidi);
        midiDriven.processBlock (midiBuffer, midi);
        returnDifference = std::max (
            returnDifference, maximumBufferDifference (uiBuffer, midiBuffer));
    }
    expect (returnDifference < 1.0e-6f,
            "dense UI lever traffic lost the final spring return or reasserted later");

    YouKnow106AudioProcessor stateProbe;
    juce::MemoryBlock before, after;
    stateProbe.getStateInformation (before);
    const bool wasEdited = stateProbe.currentProgramIsEdited();
    stateProbe.postUiLeverPosition (-0.72f, 0.63f);
    stateProbe.getStateInformation (after);
    const bool stateMatches = before.getSize() == after.getSize()
        && std::memcmp (before.getData(), after.getData(), before.getSize()) == 0;
    expect (stateMatches && stateProbe.currentProgramIsEdited() == wasEdited,
            "spring-loaded performance input leaked into preset/session state");

    uiDriven.releaseResources();
    midiDriven.releaseResources();
    neutral.releaseResources();
}

void testPerformanceLeverSpringsAndEditorCloseNeutralisesIt()
{
    YouKnow106PerformanceLever lever;
    lever.setSize (120, 122);
    float callbackBend = 0.0f;
    float callbackMod = 0.0f;
    int callbackCount = 0;
    lever.onPositionChanged = [&] (float bend, float modulation)
    {
        callbackBend = bend;
        callbackMod = modulation;
        ++callbackCount;
    };

    lever.mouseDown (mouseEventFor (
        lever, { 112.0f, 28.0f },
        juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
    expect (lever.getPitchBend() > 0.9f && lever.getModulation() > 0.9f,
            "the vector lever does not reach positive bend and full modulation");
    lever.mouseUp (mouseEventFor (lever, { 112.0f, 28.0f }, {}));
    expect (lever.getPitchBend() == 0.0f && lever.getModulation() == 0.0f
                && callbackBend == 0.0f && callbackMod == 0.0f
                && callbackCount >= 2,
            "the vector lever did not publish its exact spring return");

    lever.mouseDown (mouseEventFor (
        lever, { 8.0f, 110.0f },
        juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
    expect (lever.getPitchBend() < -0.9f && lever.getModulation() == 0.0f,
            "the vector lever does not reach negative bend or clamp downward mod");
    lever.mouseUp (mouseEventFor (lever, { 8.0f, 110.0f }, {}));

    // The editor owns the live gesture. Closing it while the mouse is held is
    // equivalent to letting go and must supersede the displaced mailbox value.
    YouKnow106AudioProcessor closedEditor;
    YouKnow106AudioProcessor reference;
    for (auto* processor : { &closedEditor, &reference })
    {
        setParameterValue (*processor, parameters::calibration, 0.0f);
        setParameterValue (*processor, parameters::chorusNoise, 0.0f);
        setParameterValue (*processor, parameters::benderDco, 1.0f);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (closedEditor.createEditor());
    auto* editorLever = editor != nullptr
        ? dynamic_cast<YouKnow106PerformanceLever*> (
              findDescendantNamed (*editor, "Pitch and modulation lever"))
        : nullptr;
    expect (editorLever != nullptr, "the editor has no performance lever to close");
    if (editorLever != nullptr)
    {
        const auto upperRight = juce::Point<float> {
            static_cast<float> (editorLever->getWidth() - 2), 2.0f };
        editorLever->mouseDown (mouseEventFor (
            *editorLever, upperRight,
            juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
    }
    editor.reset();

    juce::AudioBuffer<float> closedBuffer (2, blockSize);
    juce::AudioBuffer<float> referenceBuffer (2, blockSize);
    float difference = 0.0f;
    for (int block = 0; block < 12; ++block)
    {
        juce::MidiBuffer closedMidi, referenceMidi;
        if (block == 0)
        {
            closedMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 1);
            referenceMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 1);
        }
        closedBuffer.clear();
        referenceBuffer.clear();
        closedEditor.processBlock (closedBuffer, closedMidi);
        reference.processBlock (referenceBuffer, referenceMidi);
        difference = std::max (
            difference, maximumBufferDifference (closedBuffer, referenceBuffer));
    }
    expect (difference < 1.0e-6f,
            "closing the editor during a lever drag left pitch/mod latched");

    closedEditor.releaseResources();
    reference.releaseResources();

    // An untouched on-screen lever owns no controller state. Opening and
    // closing the editor must therefore leave an external controller's held
    // Pitch Wheel and CC1 positions alone; hosts are not required to resend
    // either value when a plug-in window disappears.
    YouKnow106AudioProcessor externalWithEditor;
    YouKnow106AudioProcessor externalReference;
    for (auto* processor : { &externalWithEditor, &externalReference })
    {
        setParameterValue (*processor, parameters::calibration, 0.0f);
        setParameterValue (*processor, parameters::chorusNoise, 0.0f);
        setParameterValue (*processor, parameters::chorusI, 0.0f);
        setParameterValue (*processor, parameters::chorusII, 0.0f);
        setParameterValue (*processor, parameters::benderDco, 0.8f);
        setParameterValue (*processor, parameters::benderLfo, 0.7f);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    juce::AudioBuffer<float> externalEditorBuffer (2, blockSize);
    juce::AudioBuffer<float> externalReferenceBuffer (2, blockSize);
    for (int block = 0; block < 8; ++block)
    {
        juce::MidiBuffer editorMidi, referenceMidi;
        if (block == 0)
        {
            for (auto* events : { &editorMidi, &referenceMidi })
            {
                events->addEvent (juce::MidiMessage::pitchWheel (1, 15100), 0);
                events->addEvent (juce::MidiMessage::controllerEvent (1, 1, 103), 0);
                events->addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 1);
            }
        }
        externalEditorBuffer.clear();
        externalReferenceBuffer.clear();
        externalWithEditor.processBlock (externalEditorBuffer, editorMidi);
        externalReference.processBlock (externalReferenceBuffer, referenceMidi);
    }

    {
        std::unique_ptr<juce::AudioProcessorEditor> untouchedEditor (
            externalWithEditor.createEditor());
        expect (untouchedEditor != nullptr,
                "the processor produced no editor for the external-controller check");
    }

    float externalDifference = 0.0f;
    for (int block = 0; block < 16; ++block)
    {
        juce::MidiBuffer editorMidi, referenceMidi;
        externalEditorBuffer.clear();
        externalReferenceBuffer.clear();
        externalWithEditor.processBlock (externalEditorBuffer, editorMidi);
        externalReference.processBlock (externalReferenceBuffer, referenceMidi);
        externalDifference = std::max (
            externalDifference,
            maximumBufferDifference (externalEditorBuffer,
                                     externalReferenceBuffer));
    }
    expect (externalDifference < 1.0e-6f,
            "closing an untouched editor cleared external pitch/mod state");

    externalWithEditor.releaseResources();
    externalReference.releaseResources();
}

void testPanicSilencesEverything()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    for (int note = 60; note < 66; ++note)
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);
    buffer.clear();
    processor.processBlock (buffer, midi);
    renderBlocks (processor, buffer, 8);
    expect (processor.getActiveVoiceCount() > 0, "no voices to silence");

    processor.requestPanic();
    renderBlocks (processor, buffer, 4);
    expect (processor.getActiveVoiceCount() == 0, "panic left voices sounding");

    processor.releaseResources();
}

void testStateRoundTripAndMigration()
{
    YouKnow106AudioProcessor source;
    setParameterValue (source, parameters::volume, 0.50f);
    setParameterValue (source, parameters::cutoff, 0.31f);
    setParameterValue (source, parameters::resonance, 0.77f);
    setParameterValue (source, parameters::chorusII, 1.0f);
    setParameterValue (source, parameters::range, 0.0f);
    setParameterValue (source, parameters::calibration, 0.17f);

    const auto removeCalibration = [] (juce::ValueTree& state)
    {
        for (int index = state.getNumChildren(); --index >= 0;)
            if (state.getChild (index).getProperty ("id").toString()
                    == parameters::calibration)
                state.removeChild (index, nullptr);
    };

    const auto serialise = [] (const juce::ValueTree& state,
                               juce::MemoryBlock& destination)
    {
        if (const auto xml = state.createXml())
        {
            juce::AudioProcessor::copyXmlToBinary (*xml, destination);
            return true;
        }
        return false;
    };

    juce::MemoryBlock state;
    source.getStateInformation (state);

    YouKnow106AudioProcessor destination;
    destination.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (std::abs (parameterValue (destination, parameters::cutoff) - 0.31f) < 1.0e-4f,
            "cutoff did not survive a state round trip");
    expect (std::abs (parameterValue (destination, parameters::resonance) - 0.77f) < 1.0e-4f,
            "resonance did not survive a state round trip");
    expect (std::abs (parameterValue (destination, parameters::chorusII) - 1.0f) < 1.0e-4f,
            "the effect switch did not survive a state round trip");
    expect (std::abs (parameterValue (destination, parameters::calibration) - 0.17f)
                < 1.0e-4f,
            "an explicit Unit Character value did not survive a state round trip");
    expect (std::abs (parameterValue (destination, parameters::volume) - 0.50f)
                < 1.0e-4f,
            "a current loaded-law Volume value was incorrectly migrated");

    // A schema-less state with an explicit historical Calibration value must
    // retain it. The schema migration is about choosing a fallback for a missing
    // child, never about replacing sound that the session actually stored.
    const auto legacyExplicit = source.parameters.copyState();
    juce::MemoryBlock legacyExplicitBytes;
    if (serialise (legacyExplicit, legacyExplicitBytes))
    {
        YouKnow106AudioProcessor migrated;
        migrated.setStateInformation (legacyExplicitBytes.getData(),
                                      static_cast<int> (legacyExplicitBytes.getSize()));
        expect (std::abs (parameterValue (migrated, parameters::calibration) - 0.17f)
                    < 1.0e-4f,
                "a schema-less explicit Calibration value was overwritten");
    }
    else
    {
        expect (false, "could not serialise a schema-less explicit state");
    }

    // A schema-less state written before Calibration existed keeps the legacy
    // 35% sound. This is deliberately not the new parameter's default.
    auto trimmed = legacyExplicit.createCopy();
    removeCalibration (trimmed);

    juce::MemoryBlock legacy;
    if (serialise (trimmed, legacy))
    {
        YouKnow106AudioProcessor migrated;
        migrated.setStateInformation (legacy.getData(),
                                      static_cast<int> (legacy.getSize()));
        expect (std::abs (parameterValue (migrated, parameters::calibration) - 0.35f)
                    < 1.0e-4f,
                "a legacy missing Calibration did not receive the historical 35%");
        expect (std::abs (parameterValue (migrated, parameters::cutoff) - 0.31f) < 1.0e-4f,
                "loading an older state discarded the parameters it did carry");
    }
    else
    {
        expect (false, "could not serialise a trimmed state");
    }

    // Current saves carry an explicit root schema. If a current-schema state is
    // partial or hand-edited and omits Unit Character, normal default filling
    // must use today's layout default rather than the legacy 35% fallback.
    const auto savedXml = juce::AudioProcessor::getXmlFromBinary (
        state.getData(), static_cast<int> (state.getSize()));
    expect (savedXml != nullptr, "could not decode a current saved state");
    if (savedXml != nullptr)
    {
        auto currentMissing = juce::ValueTree::fromXml (*savedXml);
        expect (static_cast<int> (currentMissing.getProperty (
                    "stateSchemaVersion", 0)) == 3,
                "a current saved state has no Unit Character schema marker");

        // Schema 1 used a squared Volume law after the old unloaded coupling
        // boundary. A saved 50% value must move to the new loaded-linear shaft
        // position that preserves that former static attenuation as closely as
        // the real full-travel load permits.
        auto legacyVolume = currentMissing.createCopy();
        legacyVolume.setProperty ("stateSchemaVersion", 1, nullptr);
        juce::MemoryBlock legacyVolumeBytes;
        if (serialise (legacyVolume, legacyVolumeBytes))
        {
            YouKnow106AudioProcessor migrated;
            migrated.setStateInformation (
                legacyVolumeBytes.getData(),
                static_cast<int> (legacyVolumeBytes.getSize()));
            const float restored = parameterValue (migrated, parameters::volume);
            const float formerGain =
                YouKnow106Engine::outputCouplingHighGain() * 0.25f;
            expect (std::abs (
                        YouKnow106Engine::outputCouplingHighGain (restored)
                        - formerGain) < 2.0e-5f,
                    "schema-1 Volume did not preserve its former static gain");
            expect (restored < 0.5f,
                    "schema-1 squared Volume was mistaken for a linear shaft value");
        }
        else
        {
            expect (false, "could not serialise a schema-1 Volume state");
        }

        removeCalibration (currentMissing);

        juce::MemoryBlock currentMissingBytes;
        if (serialise (currentMissing, currentMissingBytes))
        {
            YouKnow106AudioProcessor migrated;
            migrated.setStateInformation (
                currentMissingBytes.getData(),
                static_cast<int> (currentMissingBytes.getSize()));
            expect (std::abs (parameterValue (migrated, parameters::calibration)
                              - 1.0f) < 1.0e-4f,
                    "a current missing Unit Character did not receive the "
                    "layout default");
        }
        else
        {
            expect (false, "could not serialise a current partial state");
        }
    }
}

void testLegacySplitModeMigration()
{
    const auto setStored = [] (juce::ValueTree& state, const char* id, float value)
    {
        for (int index = 0; index < state.getNumChildren(); ++index)
        {
            auto child = state.getChild (index);
            if (child.getProperty ("id").toString() == id)
            {
                child.setProperty ("value", value, nullptr);
                return;
            }
        }
        expect (false, std::string ("legacy fixture is missing ") + id);
    };
    const auto removeStored = [] (juce::ValueTree& state, const char* id)
    {
        for (int index = state.getNumChildren(); --index >= 0;)
            if (state.getChild (index).getProperty ("id").toString() == id)
                state.removeChild (index, nullptr);
    };

    YouKnow106AudioProcessor source;
    for (int key = 0; key < 3; ++key)
        for (int chorus = 0; chorus < 3; ++chorus)
        {
            auto legacyState = source.parameters.copyState();
            removeStored (legacyState, parameters::poly1);
            removeStored (legacyState, parameters::poly2);
            removeStored (legacyState, parameters::chorusI);
            removeStored (legacyState, parameters::chorusII);
            setStored (legacyState, parameters::legacyKeyMode,
                       static_cast<float> (key));
            setStored (legacyState, parameters::legacyChorus,
                       static_cast<float> (chorus));

            juce::MemoryBlock bytes;
            if (const auto xml = legacyState.createXml())
                juce::AudioProcessor::copyXmlToBinary (*xml, bytes);
            else
            {
                expect (false, "could not serialise a genuine legacy mode state");
                continue;
            }

            YouKnow106AudioProcessor restored;
            restored.setStateInformation (bytes.getData(),
                                          static_cast<int> (bytes.getSize()));
            const auto expectedKey = static_cast<KeyMode> (key);
            const auto expectedChorus = static_cast<ChorusMode> (chorus);
            expect ((parameterValue (restored, parameters::poly1) > 0.5f)
                        == poly1Engaged (expectedKey),
                    "legacy key mode restored the wrong Poly 1 lamp");
            expect ((parameterValue (restored, parameters::poly2) > 0.5f)
                        == poly2Engaged (expectedKey),
                    "legacy key mode restored the wrong Poly 2 lamp");
            expect ((parameterValue (restored, parameters::chorusI) > 0.5f)
                        == chorusOneEngaged (expectedChorus),
                    "legacy chorus restored the wrong Chorus I lamp");
            expect ((parameterValue (restored, parameters::chorusII) > 0.5f)
                        == chorusTwoEngaged (expectedChorus),
                    "legacy chorus restored the wrong Chorus II lamp");
        }

    // A short-lived pair-based state could store neither POLY lamp even though
    // the firmware cannot. Saving and restoring it must expose the same Poly 1
    // fallback the audio engine uses.
    setParameterValue (source, parameters::poly1, 0.0f);
    setParameterValue (source, parameters::poly2, 0.0f);
    juce::MemoryBlock invalidPair;
    source.getStateInformation (invalidPair);
    YouKnow106AudioProcessor canonical;
    canonical.setStateInformation (invalidPair.getData(),
                                   static_cast<int> (invalidPair.getSize()));
    expect (parameterValue (canonical, parameters::poly1) > 0.5f
                && parameterValue (canonical, parameters::poly2) < 0.5f,
            "a both-off POLY state did not canonicalise to Poly 1");

    // A partially written pair must be reconstructed from the member that did
    // survive, not from Poly 1's global default. In particular, `poly2=true`
    // plus a missing Poly 1 child still means Poly 2 -- not Unison.
    const auto restorePartialPair = [&] (bool keepPoly1, float survivingValue)
    {
        auto partial = source.parameters.copyState();
        setStored (partial, parameters::poly1, keepPoly1 ? survivingValue : 0.0f);
        setStored (partial, parameters::poly2, keepPoly1 ? 0.0f : survivingValue);
        removeStored (partial, keepPoly1 ? parameters::poly2 : parameters::poly1);

        juce::MemoryBlock bytes;
        if (const auto xml = partial.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, bytes);
        else
        {
            expect (false, "could not serialise a partial POLY pair");
            return std::pair { false, false };
        }

        YouKnow106AudioProcessor restored;
        restored.setStateInformation (bytes.getData(), static_cast<int> (bytes.getSize()));
        return std::pair {
            parameterValue (restored, parameters::poly1) > 0.5f,
            parameterValue (restored, parameters::poly2) > 0.5f
        };
    };

    expect (restorePartialPair (false, 1.0f) == std::pair { false, true },
            "a poly2-only state restored as something other than Poly 2");
    expect (restorePartialPair (true, 0.0f) == std::pair { false, true },
            "a surviving Poly 1 off lamp did not reconstruct Poly 2");
    expect (restorePartialPair (true, 1.0f) == std::pair { true, false },
            "a poly1-only state restored as something other than Poly 1");
    expect (restorePartialPair (false, 0.0f) == std::pair { true, false },
            "a surviving Poly 2 off lamp did not reconstruct Poly 1");

    // Chorus differs from POLY in one important respect: both lamps off is a
    // legal state. A surviving on lamp is therefore conclusive, while a
    // surviving off lamp can use the legacy choice only when that choice names
    // the missing mode. Exercise both missing-member directions so migration
    // does not accidentally depend on Chorus I being the surviving child.
    const auto restorePartialChorus = [&] (bool keepChorusI,
                                           float survivingValue,
                                           int legacyMode)
    {
        auto partial = source.parameters.copyState();
        setStored (partial, parameters::legacyChorus,
                   static_cast<float> (legacyMode));
        setStored (partial, parameters::chorusI,
                   keepChorusI ? survivingValue : 0.0f);
        setStored (partial, parameters::chorusII,
                   keepChorusI ? 0.0f : survivingValue);
        removeStored (partial, keepChorusI ? parameters::chorusII
                                           : parameters::chorusI);

        juce::MemoryBlock bytes;
        if (const auto xml = partial.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, bytes);
        else
        {
            expect (false, "could not serialise a partial chorus pair");
            return std::pair { false, false };
        }

        YouKnow106AudioProcessor restored;
        restored.setStateInformation (bytes.getData(),
                                      static_cast<int> (bytes.getSize()));
        return std::pair {
            parameterValue (restored, parameters::chorusI) > 0.5f,
            parameterValue (restored, parameters::chorusII) > 0.5f
        };
    };

    expect (restorePartialChorus (true, 1.0f, 2)
                == std::pair { true, false },
            "a surviving Chorus I on lamp lost to stale legacy Chorus II");
    expect (restorePartialChorus (false, 1.0f, 1)
                == std::pair { false, true },
            "a surviving Chorus II on lamp lost to stale legacy Chorus I");
    expect (restorePartialChorus (true, 0.0f, 2)
                == std::pair { false, true },
            "a missing Chorus II lamp was not recovered from its legacy mode");
    expect (restorePartialChorus (false, 0.0f, 1)
                == std::pair { true, false },
            "a missing Chorus I lamp was not recovered from its legacy mode");
    expect (restorePartialChorus (true, 0.0f, 0)
                == std::pair { false, false },
            "a legal chorus-off state was changed while Chorus II was missing");
    expect (restorePartialChorus (false, 0.0f, 0)
                == std::pair { false, false },
            "a legal chorus-off state was changed while Chorus I was missing");
}

void testEveryStoredPatchFieldRecallsWithoutMovingPerformanceControls()
{
    YouKnow106AudioProcessor processor;

    // Give every non-patch control a conspicuous value. A recall must leave
    // all of these alone: they describe the current performance or the model,
    // not one of the JUNO's stored tones.
    constexpr auto performanceValues = std::to_array<std::pair<const char*, float>> ({
        { parameters::volume,       0.23f },
        { parameters::benderDco,    0.41f },
        { parameters::benderVcf,    0.37f },
        { parameters::benderLfo,    0.29f },
        { parameters::portamento,   0.31f },
        { parameters::legacyKeyMode, 1.0f },
        { parameters::poly1,        0.0f },
        { parameters::poly2,        1.0f },
        { parameters::legacyChorus, 1.0f },
        { parameters::transpose,   -7.0f },
        { parameters::masterTune,  13.0f },
        { parameters::velocity,     0.43f },
        { parameters::calibration,  0.17f },
        { parameters::chorusNoise,  0.27f },
        { parameters::polyphony,    4.0f },
        { parameters::hq,           0.0f },
    });
    for (const auto& [id, value] : performanceValues)
        setParameterValue (processor, id, value);

    // All sixteen continuous bytes deliberately differ, so swapping or
    // omitting any two fields cannot accidentally pass. The eight stored
    // switch fields likewise exercise the non-default side where possible.
    sysex::Patch expected {};
    expected.lfoRate = 0.03f;
    expected.lfoDelay = 0.09f;
    expected.dcoLfo = 0.15f;
    expected.pwm = 0.21f;
    expected.noise = 0.27f;
    expected.cutoff = 0.33f;
    expected.resonance = 0.39f;
    expected.vcfEnv = 0.45f;
    expected.vcfLfo = 0.51f;
    expected.keyFollow = 0.57f;
    expected.vcaLevel = 0.63f;
    expected.attack = 0.69f;
    expected.decay = 0.75f;
    expected.sustain = 0.81f;
    expected.release = 0.87f;
    expected.sub = 0.93f;
    expected.range = DcoRange::Four;
    expected.saw = false;
    expected.pulse = true;
    expected.pwmSource = PwmSource::Lfo;
    expected.vcaMode = VcaMode::Gate;
    expected.envPolarity = EnvPolarity::Inverted;
    expected.highPass = HighPassMode::Three;
    expected.chorus = ChorusMode::Two;

    processor.applyPatch (expected);
    const auto recalled = processor.currentPatch();
    const auto same = [] (float first, float second)
    {
        return std::abs (first - second) < 1.0e-6f;
    };

    expect (same (recalled.lfoRate, expected.lfoRate), "LFO rate recalled incorrectly");
    expect (same (recalled.lfoDelay, expected.lfoDelay), "LFO delay recalled incorrectly");
    expect (same (recalled.dcoLfo, expected.dcoLfo), "DCO LFO recalled incorrectly");
    expect (same (recalled.pwm, expected.pwm), "PWM recalled incorrectly");
    expect (same (recalled.noise, expected.noise), "noise recalled incorrectly");
    expect (same (recalled.cutoff, expected.cutoff), "cutoff recalled incorrectly");
    expect (same (recalled.resonance, expected.resonance), "resonance recalled incorrectly");
    expect (same (recalled.vcfEnv, expected.vcfEnv), "VCF envelope recalled incorrectly");
    expect (same (recalled.vcfLfo, expected.vcfLfo), "VCF LFO recalled incorrectly");
    expect (same (recalled.keyFollow, expected.keyFollow), "key follow recalled incorrectly");
    expect (same (recalled.vcaLevel, expected.vcaLevel), "VCA level recalled incorrectly");
    expect (same (recalled.attack, expected.attack), "attack recalled incorrectly");
    expect (same (recalled.decay, expected.decay), "decay recalled incorrectly");
    expect (same (recalled.sustain, expected.sustain), "sustain recalled incorrectly");
    expect (same (recalled.release, expected.release), "release recalled incorrectly");
    expect (same (recalled.sub, expected.sub), "sub level recalled incorrectly");
    expect (recalled.range == expected.range, "DCO range recalled incorrectly");
    expect (recalled.saw == expected.saw, "saw switch recalled incorrectly");
    expect (recalled.pulse == expected.pulse, "pulse switch recalled incorrectly");
    expect (recalled.pwmSource == expected.pwmSource, "PWM source recalled incorrectly");
    expect (recalled.vcaMode == expected.vcaMode, "VCA mode recalled incorrectly");
    expect (recalled.envPolarity == expected.envPolarity,
            "envelope polarity recalled incorrectly");
    expect (recalled.highPass == expected.highPass, "high-pass recalled incorrectly");
    expect (recalled.chorus == expected.chorus, "chorus mode recalled incorrectly");

    for (const auto& [id, value] : performanceValues)
        expect (same (parameterValue (processor, id), value),
                std::string ("patch recall moved non-patch control ") + id);
}

void testRandomizerPreservesQualityAndLevel()
{
    YouKnow106AudioProcessor processor;
    const float volume = parameterValue (processor, parameters::volume);
    const float voices = parameterValue (processor, parameters::polyphony);
    const float quality = parameterValue (processor, parameters::hq);

    processor.randomizeParameters (1.0f);

    expect (std::abs (parameterValue (processor, parameters::volume) - volume) < 1.0e-4f,
            "the randomiser moved the output level");
    expect (std::abs (parameterValue (processor, parameters::polyphony) - voices) < 1.0e-4f,
            "the randomiser moved the voice count");
    expect (std::abs (parameterValue (processor, parameters::hq) - quality) < 1.0e-4f,
            "the randomiser moved a quality setting");
}

void testBusLayoutsAndTail()
{
    YouKnow106AudioProcessor processor;
    expect (processor.getTailLengthSeconds() >= 25.55,
            "the reported tail is shorter than the longest release");
    expect (processor.acceptsMidi() && processor.producesMidi(),
            "the plug-in does not advertise itself as an instrument");
}

// The owner's Patch Selection table is zero based: 0..63 are A11..A88 and
// 64..127 are B11..B88. Exercise every address so no row/column transposition
// or compact-bank remnant can hide between boundary fixtures.
void testProgramChangeRecallsEveryHardwareSlot()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    constexpr float volume = 0.413f;
    constexpr float portamento = 0.271f;
    setParameterValue (processor, parameters::volume, volume);
    setParameterValue (processor, parameters::portamento, portamento);

    juce::AudioBuffer<float> buffer (2, blockSize);
    for (int midiProgram = 0; midiProgram < presets::presetCount; ++midiProgram)
    {
        const int hostProgram = midiProgram + 1;
        const auto& preset =
            presets::factoryBank()[static_cast<std::size_t> (midiProgram)];
        // Make the current panel different first, so matching the program index
        // alone cannot hide a recall which failed to move its controls.
        setParameterValue (processor, parameters::cutoff, 0.001f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::programChange (16, midiProgram), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        expect (midi.isEmpty(),
                std::string ("incoming Program Change was echoed for ")
                    + preset.number);

        // Parameter and host-program writes happen only on the message side.
        processor.flushPendingMidiEvents();
        expect (processor.getCurrentProgram() == hostProgram,
                std::string ("wrong host program for MIDI Program Change ")
                    + std::to_string (midiProgram));
        expect (processor.getProgramName (hostProgram)
                    .startsWith (preset.number),
                std::string ("wrong numbered patch for MIDI Program Change ")
                    + std::to_string (midiProgram));

        std::array<std::uint8_t, sysex::toneByteCount> recalled {}, expected {};
        sysex::toneBytesFromPatch (processor.currentPatch(), recalled.data());
        sysex::toneBytesFromPatch (processor.programPatch (hostProgram),
                                   expected.data());
        expect (recalled == expected,
                std::string ("Program Change did not recall the full patch ")
                    + preset.number);
        expect (std::abs (parameterValue (processor, parameters::volume) - volume)
                    < 1.0e-4f,
                "Program Change moved non-patch volume");
        expect (std::abs (parameterValue (processor, parameters::portamento)
                         - portamento) < 1.0e-4f,
                "Program Change moved non-patch portamento");
        expect (processor.currentProgramIsEdited(),
                "hardware Program Change hid retained product-control edits");
    }

    processor.releaseResources();
}

// Hosts may deliver a Program Change and a note in the same offline block
// without running the message loop between them. The tone must change at the
// event's sample, while the APVTS and host selector catch up later on the
// message thread. In particular, that later reflection must not pull an active
// voice back to the old panel snapshot for one block.
void testProgramChangeAffectsFollowingNoteWithoutTheMessageThread()
{
    constexpr int midiProgram = 2; // A13, host program index 3, chorus off.
    constexpr int hostProgram = 3;

    YouKnow106AudioProcessor midiDriven;
    YouKnow106AudioProcessor reference;
    reference.setCurrentProgram (hostProgram);

    for (auto* processor : { &midiDriven, &reference })
    {
        setParameterValue (*processor, parameters::calibration, 0.0f);
        setParameterValue (*processor, parameters::chorusNoise, 0.0f);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    juce::AudioBuffer<float> midiBuffer (2, blockSize);
    juce::AudioBuffer<float> referenceBuffer (2, blockSize);
    float difference = 0.0f;
    float peak = 0.0f;

    for (int block = 0; block < 16; ++block)
    {
        juce::MidiBuffer midi, referenceMidi;
        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::programChange (1, midiProgram), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 1);
            referenceMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 1);
        }

        midiBuffer.clear();
        referenceBuffer.clear();
        midiDriven.processBlock (midiBuffer, midi);
        reference.processBlock (referenceBuffer, referenceMidi);
        difference = std::max (
            difference, maximumBufferDifference (midiBuffer, referenceBuffer));
        peak = std::max (peak, bufferPeak (referenceBuffer));

        if (block == 5)
        {
            expect (midiDriven.getCurrentProgram() == 0,
                    "Program Change reached the host selector without a message tick");
            midiDriven.flushPendingMidiEvents();
            expect (midiDriven.getCurrentProgram() == hostProgram,
                    "the deferred host selector missed the Program Change");
        }
    }

    expect (peak > 0.001f,
            "same-block Program Change regression rendered silence");
    expect (difference < 1.0e-6f,
            "Program Change did not affect the following note at its sample, or its "
            "later host reflection reverted the DSP");

    midiDriven.releaseResources();
    reference.releaseResources();
}

// JUCE explicitly permits zero-frame callbacks, and their MIDI buffer still
// describes real state transitions. A stopped/offline host can use one to send
// a preset selection followed by a hardware control edit; neither may vanish
// merely because there are no samples to render.
void testZeroSampleBlockStillHandlesProgramAndSysEx()
{
    constexpr int hostProgram = 3; // MIDI program 2, A13.
    constexpr int cutoffByte = 19;
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    std::array<std::uint8_t, sysex::parameterMessageBytes> raw {};
    const auto written = sysex::writeParameterMessage (
        static_cast<int> (sysex::ToneParameter::VcfFreq), cutoffByte, 0,
        raw.data(), raw.size());
    expect (written == raw.size(),
            "could not build the zero-sample SysEx fixture");

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::programChange (1, 2), 0);
    // Deliberately outside the nonexistent audio range. processBlock must map
    // it to timestamp zero while retaining its order after Program Change.
    midi.addEvent (juce::MidiMessage::createSysExMessage (
                       raw.data() + 1, static_cast<int> (written) - 2),
                   27);
    juce::AudioBuffer<float> zeroSamples (2, 0);
    processor.processBlock (zeroSamples, midi);
    expect (midi.isEmpty(),
            "zero-sample callback echoed its input instead of consuming it");

    processor.flushPendingMidiEvents();
    expect (processor.getCurrentProgram() == hostProgram,
            "zero-sample Program Change was discarded");
    auto expected = processor.programPatch (hostProgram);
    expect (sysex::applyParameter (
                expected, static_cast<int> (sysex::ToneParameter::VcfFreq),
                cutoffByte),
            "could not build the zero-sample expected tone");
    std::array<std::uint8_t, sysex::toneByteCount> reflected {}, wanted {};
    sysex::toneBytesFromPatch (processor.currentPatch(), reflected.data());
    sysex::toneBytesFromPatch (expected, wanted.data());
    expect (reflected == wanted,
            "zero-sample SysEx did not apply after Program Change");

    processor.releaseResources();
}

// The JUCE-free suite checks legends fit using a deliberately conservative
// width model, because it has to run on a machine with no fonts. This is the
// check that makes relying on that model safe: it asks the real typeface, at
// the real size, whether each legend actually draws inside its box. If the
// model ever drifts optimistic, this fails on the macOS runner.
// A patch dump has to reach the parameters, and the parameters have to come
// back out as a message the hardware would accept. The DSP suite proves the
// byte layout; this proves the plug-in is actually wired to it.
void testSysExPatchRoundTripsThroughTheParameters()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    sysex::Patch sent {};
    sent.cutoff = 0.25f;
    sent.resonance = 0.75f;
    sent.attack = 0.5f;
    sent.range = DcoRange::Four;
    sent.saw = false;
    sent.pulse = true;
    sent.vcaMode = VcaMode::Gate;
    sent.highPass = HighPassMode::Boost;
    sent.chorus = ChorusMode::One;

    std::array<std::uint8_t, sysex::patchMessageBytes> raw {};
    const auto written = sysex::writePatchMessage (sent, 0, raw.data(), raw.size());
    expect (written > 0, "could not build a patch message");

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::createSysExMessage (
                       raw.data() + 1, static_cast<int> (written) - 2),
                   0);
    // Leave a newer-build compatibility id change pending while the full patch
    // arrives. The full SysEx is later and must own its stored chorus setting;
    // the assign-mode compatibility id is deliberately unrelated to a patch.
    setParameterValue (processor, parameters::legacyChorus, 2.0f);
    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();
    processor.processBlock (buffer, midi);

    // The patch is staged for the message thread; drain it here so the test is
    // deterministic rather than dependent on a message loop it does not run.
    processor.flushPendingMidiEvents();
    processor.forwardLegacyModeParametersForTest();

    expect (std::abs (parameterValue (processor, parameters::cutoff) - 0.25f) < 0.01f,
            "a patch dump did not reach the cutoff parameter");
    expect (std::abs (parameterValue (processor, parameters::resonance) - 0.75f) < 0.01f,
            "a patch dump did not reach the resonance parameter");
    expect (parameterValue (processor, parameters::saw) < 0.5f,
            "a patch dump did not clear the saw switch");
    expect (parameterValue (processor, parameters::pulse) > 0.5f,
            "a patch dump did not set the pulse switch");
    expect (parameterValue (processor, parameters::chorusI) > 0.5f,
            "a patch dump did not engage chorus I");
    expect (parameterValue (processor, parameters::chorusII) < 0.5f,
            "a patch dump engaged chorus II as well as I");

    // Straight back out again.
    const auto emitted = processor.currentPatchAsSysEx (0);
    expect (emitted.isSysEx(), "the current patch did not come back out as sysex");
    sysex::Patch returned {};
    int channel = -1;
    expect (sysex::readPatchMessage (emitted.getRawData(),
                                     static_cast<std::size_t> (emitted.getRawDataSize()),
                                     returned, channel),
            "the emitted message was not readable");
    expect (returned.range == DcoRange::Four, "the range did not survive the trip out");
    expect (returned.vcaMode == VcaMode::Gate, "the VCA mode did not survive");
    expect (returned.highPass == HighPassMode::Boost, "the high-pass did not survive");
    expect (returned.chorus == ChorusMode::One, "the chorus mode did not survive");

    processor.releaseResources();
}

// SysEx has the same timing obligation as notes and Program Change. A full
// dump followed by a one-parameter edit in the same block must build one
// ordered audio-side tone even when no message-thread callback is available.
// Reflecting the older full dump later must also leave the newer edit intact.
void testOrderedSysExAffectsAudioWithoutTheMessageThread()
{
    sysex::Patch sent {};
    sent.lfoRate = 0.17f;
    sent.dcoLfo = 0.11f;
    sent.cutoff = 0.29f;
    sent.resonance = 0.41f;
    sent.attack = 0.08f;
    sent.decay = 0.37f;
    sent.sustain = 0.72f;
    sent.release = 0.26f;
    sent.range = DcoRange::Four;
    sent.saw = false;
    sent.pulse = false;
    sent.chorus = ChorusMode::Off;

    // The hardware dump is seven-bit. Build the reference from exactly the
    // bytes which will arrive, then form a packed switch edit which enables
    // pulse without disturbing the full dump's other switches.
    std::array<std::uint8_t, sysex::toneByteCount> toneBytes {};
    sysex::toneBytesFromPatch (sent, toneBytes.data());
    auto expected = sysex::patchFromToneBytes (toneBytes.data());
    auto afterSwitch = expected;
    afterSwitch.pulse = true;
    const int switchParameter =
        static_cast<int> (sysex::ToneParameter::SwitchesOne);
    const int packedSwitches = sysex::parameterValue (afterSwitch, switchParameter);
    expect (sysex::applyParameter (expected, switchParameter, packedSwitches),
            "could not build the ordered SysEx reference patch");

    std::array<std::uint8_t, sysex::patchMessageBytes> patchMessage {};
    const auto patchWritten = sysex::writePatchMessage (
        sent, 0, patchMessage.data(), patchMessage.size());
    std::array<std::uint8_t, sysex::parameterMessageBytes> parameterMessage {};
    const auto parameterWritten = sysex::writeParameterMessage (
        switchParameter, packedSwitches, 0,
        parameterMessage.data(), parameterMessage.size());
    expect (patchWritten == patchMessage.size()
                && parameterWritten == parameterMessage.size(),
            "could not build the ordered SysEx messages");

    YouKnow106AudioProcessor midiDriven;
    YouKnow106AudioProcessor reference;
    // Give the MIDI-driven instance the dump's quantised continuous values but
    // deliberately leave its switches at another audible tone. This removes
    // parameter-smoother history from the comparison: any difference now is
    // specifically whether the two switch messages took effect in order.
    midiDriven.applyPatch (expected);
    setParameterValue (midiDriven, parameters::range,
                       static_cast<float> (DcoRange::Eight));
    setParameterValue (midiDriven, parameters::saw, 1.0f);
    setParameterValue (midiDriven, parameters::pulse, 0.0f);
    reference.applyPatch (expected);
    for (auto* processor : { &midiDriven, &reference })
    {
        setParameterValue (*processor, parameters::calibration, 0.0f);
        setParameterValue (*processor, parameters::chorusNoise, 0.0f);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    juce::AudioBuffer<float> midiBuffer (2, blockSize);
    juce::AudioBuffer<float> referenceBuffer (2, blockSize);
    float differenceBeforeReflection = 0.0f;
    float differenceAfterReflection = 0.0f;
    float peak = 0.0f;

    for (int block = 0; block < 16; ++block)
    {
        juce::MidiBuffer midi, referenceMidi;
        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::createSysExMessage (
                               patchMessage.data() + 1,
                               static_cast<int> (patchWritten) - 2),
                           0);
            midi.addEvent (juce::MidiMessage::createSysExMessage (
                               parameterMessage.data() + 1,
                               static_cast<int> (parameterWritten) - 2),
                           1);
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 2);
            referenceMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 2);
        }

        midiBuffer.clear();
        referenceBuffer.clear();
        midiDriven.processBlock (midiBuffer, midi);
        reference.processBlock (referenceBuffer, referenceMidi);
        const float blockDifference =
            maximumBufferDifference (midiBuffer, referenceBuffer);
        if (block <= 5)
            differenceBeforeReflection = std::max (differenceBeforeReflection,
                                                   blockDifference);
        else
            differenceAfterReflection = std::max (differenceAfterReflection,
                                                  blockDifference);
        peak = std::max (peak, bufferPeak (referenceBuffer));

        if (block == 5)
        {
            expect (parameterValue (midiDriven, parameters::saw) > 0.5f
                        && parameterValue (midiDriven, parameters::pulse) < 0.5f,
                    "SysEx wrote APVTS from the audio callback");
            midiDriven.flushPendingMidiEvents();
            expect (parameterValue (midiDriven, parameters::pulse) > 0.5f,
                    "the newer one-parameter SysEx was lost during reflection");
            std::array<std::uint8_t, sysex::toneByteCount> reflected {}, wanted {};
            sysex::toneBytesFromPatch (midiDriven.currentPatch(), reflected.data());
            sysex::toneBytesFromPatch (expected, wanted.data());
            expect (reflected == wanted,
                    "reflecting the older full SysEx overwrote the newer edit");
        }
    }

    expect (peak > 0.001f, "ordered SysEx regression rendered silence");
    // Re-sending the same parameter set at two MIDI sample boundaries can
    // move a smoothed coefficient by a few float ulps compared with preparing
    // it once up front. That bounded transient is separate from the large tone
    // error this catches; once APVTS has reflected both events the paths must
    // converge exactly again.
    expect (differenceBeforeReflection < 2.0e-5f,
            "ordered SysEx did not affect DSP at its sample "
                "(max difference "
                + std::to_string (differenceBeforeReflection) + ")");
    expect (differenceAfterReflection < 1.0e-6f,
            "later host reflection reverted the ordered SysEx tone "
                "(max difference "
                + std::to_string (differenceAfterReflection) + ")");

    midiDriven.releaseResources();
    reference.releaseResources();
}

// Reflection can finish in the few instructions after the audio thread has
// validated its APVTS generation but before it decides whether to retire the
// MIDI tone shadow. The acknowledgement must invalidate that already-gathered
// snapshot; otherwise one block briefly reinstalls the pre-SysEx patch.
void testReflectionAckCannotRetireShadowAgainstAStaleSnapshot()
{
    sysex::Patch sent {};
    sent.range = DcoRange::Four;
    sent.saw = false;
    sent.pulse = true;
    sent.chorus = ChorusMode::Off;
    std::array<std::uint8_t, sysex::toneByteCount> tone {};
    sysex::toneBytesFromPatch (sent, tone.data());
    const auto expected = sysex::patchFromToneBytes (tone.data());

    YouKnow106AudioProcessor midiDriven;
    YouKnow106AudioProcessor reference;
    midiDriven.applyPatch (expected);
    setParameterValue (midiDriven, parameters::range,
                       static_cast<float> (DcoRange::Eight));
    setParameterValue (midiDriven, parameters::saw, 1.0f);
    setParameterValue (midiDriven, parameters::pulse, 0.0f);
    reference.applyPatch (expected);
    for (auto* processor : { &midiDriven, &reference })
    {
        setParameterValue (*processor, parameters::calibration, 0.0f);
        setParameterValue (*processor, parameters::chorusNoise, 0.0f);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    std::array<std::uint8_t, sysex::patchMessageBytes> raw {};
    const auto written = sysex::writePatchMessage (
        expected, 0, raw.data(), raw.size());
    juce::MidiBuffer firstMidi, firstReferenceMidi;
    firstMidi.addEvent (juce::MidiMessage::createSysExMessage (
                            raw.data() + 1, static_cast<int> (written) - 2),
                        0);
    firstMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 1);
    firstReferenceMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 1);

    juce::AudioBuffer<float> firstBuffer (2, blockSize);
    juce::AudioBuffer<float> firstReferenceBuffer (2, blockSize);
    firstBuffer.clear();
    firstReferenceBuffer.clear();
    midiDriven.processBlock (firstBuffer, firstMidi);
    reference.processBlock (firstReferenceBuffer, firstReferenceMidi);
    float difference = maximumBufferDifference (firstBuffer,
                                                 firstReferenceBuffer);

    std::atomic<bool> snapshotCaptured { false };
    std::atomic<bool> resumeAudio { false };
    midiDriven.setMidiReflectionSnapshotBarrierForTest (&snapshotCaptured,
                                                        &resumeAudio);
    juce::AudioBuffer<float> racedBuffer (2, blockSize);
    racedBuffer.clear();
    std::thread audioThread ([&]
    {
        juce::MidiBuffer empty;
        midiDriven.processBlock (racedBuffer, empty);
    });

    const double deadline = juce::Time::getMillisecondCounterHiRes() + 2000.0;
    while (! snapshotCaptured.load (std::memory_order_acquire)
           && juce::Time::getMillisecondCounterHiRes() < deadline)
        std::this_thread::yield();
    const bool reachedBarrier =
        snapshotCaptured.load (std::memory_order_acquire);
    expect (reachedBarrier,
            "audio thread did not reach the MIDI reflection race barrier");
    if (reachedBarrier)
        midiDriven.flushPendingMidiEvents();
    resumeAudio.store (true, std::memory_order_release);
    audioThread.join();

    juce::AudioBuffer<float> racedReferenceBuffer (2, blockSize);
    juce::MidiBuffer emptyReference;
    racedReferenceBuffer.clear();
    reference.processBlock (racedReferenceBuffer, emptyReference);
    difference = std::max (
        difference,
        maximumBufferDifference (racedBuffer, racedReferenceBuffer));

    expect (parameterValue (midiDriven, parameters::saw) < 0.5f
                && parameterValue (midiDriven, parameters::pulse) > 0.5f,
            "race fixture did not reflect the SysEx patch");
    expect (bufferPeak (racedReferenceBuffer) > 0.001f,
            "MIDI reflection race regression rendered silence");
    expect (difference < 2.0e-5f,
            "reflection acknowledgement retired the shadow against stale APVTS "
                "(max difference " + std::to_string (difference) + ")");

    midiDriven.releaseResources();
    reference.releaseResources();
}

// A malformed or foreign message must be ignored rather than half-applied.
// A single-parameter message from the hardware moves that control and nothing
// else -- including after an edit made from somewhere else in between. An
// earlier version cached the panel on the audio thread and staged the whole
// snapshot, which quantised every untouched control to seven bits and reverted
// anything moved since the cache was taken.
void testSingleParameterSysExDoesNotDisturbAnythingElse()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto pump = [&] (juce::MidiBuffer& midi)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        processor.flushPendingMidiEvents();
    };

    // Deliberately off a 7-bit step, so any round trip through the tone format
    // shows up as a changed value.
    constexpr float awkward = 0.5001f;
    setParameterValue (processor, parameters::decay, awkward);
    setParameterValue (processor, parameters::chorusII, 1.0f);

    // One control moves on the hardware.
    std::array<std::uint8_t, sysex::parameterMessageBytes> first {};
    auto written = sysex::writeParameterMessage (
        static_cast<int> (sysex::ToneParameter::VcfFreq), 100, 0, first.data(),
        first.size());
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::createSysExMessage (
                       first.data() + 1, static_cast<int> (written) - 2), 0);
    pump (midi);

    expect (std::abs (parameterValue (processor, parameters::cutoff) - 100.0f / 127.0f)
                < 0.01f,
            "a single-parameter message did not reach its control");
    expect (std::abs (parameterValue (processor, parameters::decay) - awkward) < 1.0e-4f,
            "a single-parameter message quantised an untouched control");
    expect (parameterValue (processor, parameters::chorusI) < 0.5f
                && parameterValue (processor, parameters::chorusII) > 0.5f,
            "a single-parameter message disturbed chorus II");

    // Now edit something else from the UI side, then send another hardware
    // update. The second message must not resurrect the panel as it stood when
    // the first one arrived.
    setParameterValue (processor, parameters::release, 0.8123f);
    std::array<std::uint8_t, sysex::parameterMessageBytes> second {};
    written = sysex::writeParameterMessage (
        static_cast<int> (sysex::ToneParameter::VcfRes), 20, 0, second.data(),
        second.size());
    juce::MidiBuffer later;
    later.addEvent (juce::MidiMessage::createSysExMessage (
                        second.data() + 1, static_cast<int> (written) - 2), 0);
    pump (later);

    expect (std::abs (parameterValue (processor, parameters::resonance) - 20.0f / 127.0f)
                < 0.01f,
            "the second single-parameter message did not reach its control");
    expect (std::abs (parameterValue (processor, parameters::release) - 0.8123f) < 1.0e-4f,
            "a single-parameter message reverted an edit made after the previous one");
    expect (std::abs (parameterValue (processor, parameters::cutoff) - 100.0f / 127.0f)
                < 0.01f,
            "the second message undid the first");

    processor.releaseResources();
}

// A whole bank arriving in one buffer must land intact rather than overflowing
// the handoff queue.
void testAWholeBankTransferIsNotDropped()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::MidiBuffer midi;
    const auto& bank = presets::factoryBank();
    for (std::size_t index = 0; index < 64; ++index)
    {
        const auto& patch = bank[index % bank.size()].patch;
        std::array<std::uint8_t, sysex::patchMessageBytes> raw {};
        const auto written = sysex::writePatchMessage (patch, 0, raw.data(), raw.size());
        midi.addEvent (juce::MidiMessage::createSysExMessage (
                           raw.data() + 1, static_cast<int> (written) - 2),
                       static_cast<int> (index) % blockSize);
    }

    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();
    processor.processBlock (buffer, midi);
    processor.flushPendingMidiEvents();

    expect (processor.getMidiEventDroppedCount() == 0,
            "a whole bank delivered in one buffer overflowed the handoff queue");

    // And the panel has to end on the last dump, not on an intermediate one.
    const auto& last = bank[63 % bank.size()].patch;
    expect (std::abs (parameterValue (processor, parameters::cutoff) - last.cutoff)
                < 0.02f,
            "the panel did not end on the final dump of the transfer");

    processor.releaseResources();
}

// The fixed FIFO holds a complete hardware bank, but an offline host can put
// more than one bank's worth of changes in a block while never dispatching the
// message loop. Events beyond the FIFO still belong in sample time. Their host
// reflection may be coalesced, provided one exact final tone (and the latest
// Program Change selection) catches up once capacity returns.
void testOverflowedMidiReflectionCoalescesWithoutDroppingAudioEvents()
{
    constexpr int recalledProgram = 3; // MIDI program 2, A13.

    YouKnow106AudioProcessor midiDriven;
    YouKnow106AudioProcessor reference;
    auto finalPatch = midiDriven.programPatch (recalledProgram);
    finalPatch.range = DcoRange::Four;
    finalPatch.saw = false;
    finalPatch.pulse = true;

    auto baselinePatch = finalPatch;
    baselinePatch.range = DcoRange::Eight;
    baselinePatch.saw = true;
    baselinePatch.pulse = false;

    midiDriven.applyPatch (baselinePatch);
    reference.applyPatch (baselinePatch);
    for (auto* processor : { &midiDriven, &reference })
    {
        setParameterValue (*processor, parameters::calibration, 0.0f);
        setParameterValue (*processor, parameters::chorusNoise, 0.0f);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    const int switchParameter =
        static_cast<int> (sysex::ToneParameter::SwitchesOne);
    const int baselineSwitches =
        sysex::parameterValue (baselinePatch, switchParameter);
    const int finalSwitches = sysex::parameterValue (finalPatch, switchParameter);
    std::array<std::uint8_t, sysex::parameterMessageBytes> baselineMessage {},
                                                               finalMessage {};
    const auto baselineWritten = sysex::writeParameterMessage (
        switchParameter, baselineSwitches, 0,
        baselineMessage.data(), baselineMessage.size());
    const auto finalWritten = sysex::writeParameterMessage (
        switchParameter, finalSwitches, 0,
        finalMessage.data(), finalMessage.size());
    expect (baselineWritten == baselineMessage.size()
                && finalWritten == finalMessage.size(),
            "could not build the overflow-resync SysEx fixture");

    juce::MidiBuffer midi, referenceMidi;
    // Exactly fill the granular FIFO with no-op switch writes.
    for (int event = 0; event < 64; ++event)
        midi.addEvent (juce::MidiMessage::createSysExMessage (
                           baselineMessage.data() + 1,
                           static_cast<int> (baselineWritten) - 2),
                       event);

    // Event 65 must still recall the program in DSP. The following edits make
    // that selection intentionally edited, and are themselves also beyond the
    // FIFO. A resync which called setCurrentProgram would incorrectly erase
    // them, so this covers selector and tone coalescing together.
    midi.addEvent (juce::MidiMessage::programChange (1, 2), 64);
    referenceMidi.addEvent (juce::MidiMessage::programChange (1, 2), 64);
    for (int event = 65; event < 70; ++event)
    {
        midi.addEvent (juce::MidiMessage::createSysExMessage (
                           finalMessage.data() + 1,
                           static_cast<int> (finalWritten) - 2),
                       event);
        referenceMidi.addEvent (juce::MidiMessage::createSysExMessage (
                                    finalMessage.data() + 1,
                                    static_cast<int> (finalWritten) - 2),
                                event);
    }
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
    referenceMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);

    juce::AudioBuffer<float> midiBuffer (2, blockSize);
    juce::AudioBuffer<float> referenceBuffer (2, blockSize);
    midiBuffer.clear();
    referenceBuffer.clear();
    midiDriven.processBlock (midiBuffer, midi);
    reference.processBlock (referenceBuffer, referenceMidi);
    float difference = maximumBufferDifference (midiBuffer, referenceBuffer);
    float peak = bufferPeak (referenceBuffer);

    expect (midiDriven.getCurrentProgram() == 0,
            "overflowed Program Change wrote the selector on the audio thread");
    expect (parameterValue (midiDriven, parameters::saw) > 0.5f
                && parameterValue (midiDriven, parameters::pulse) < 0.5f,
            "overflowed SysEx wrote APVTS on the audio thread");

    // Reflect the 64 granular events. The fixed atomic fallback must make the
    // final snapshot available immediately after that prefix: a host may stop
    // calling processBlock as soon as this offline render finishes.
    midiDriven.flushPendingMidiEvents();
    expect (midiDriven.getCurrentProgram() == recalledProgram,
            "the overflow resync lost the latest Program Change selector");
    std::array<std::uint8_t, sysex::toneByteCount> reflected {}, expected {};
    sysex::toneBytesFromPatch (midiDriven.currentPatch(), reflected.data());
    sysex::toneBytesFromPatch (finalPatch, expected.data());
    expect (reflected == expected,
            "the overflow resync did not publish the final complete tone");
    expect (midiDriven.currentProgramIsEdited(),
            "overflow resync reloaded the program and erased later SysEx edits");
    expect (midiDriven.getMidiEventDroppedCount() == 0,
            "recoverable reflection overflow was reported as a hard event drop");

    // The next audio block observes the acknowledgement and retires its shadow;
    // doing so must not move the already-sounding voice back to the queued
    // prefix which preceded the resync.
    for (int block = 0; block < 8; ++block)
    {
        juce::MidiBuffer drivenMidi, wantedMidi;
        midiBuffer.clear();
        referenceBuffer.clear();
        midiDriven.processBlock (midiBuffer, drivenMidi);
        reference.processBlock (referenceBuffer, wantedMidi);
        difference = std::max (
            difference, maximumBufferDifference (midiBuffer, referenceBuffer));
        peak = std::max (peak, bufferPeak (referenceBuffer));
    }

    expect (peak > 0.001f, "overflow-resync regression rendered silence");
    // The event-driven instance changes a continuously running DCO shortly
    // before Note On, while the reference was prepared at the final pitch.
    // Bandlimited physical restarts correctly retain a minute pre-note phase/
    // residual difference; this bound remains far below an audible or stale-
    // patch mismatch while no longer demanding an artificial hard reset.
    expect (difference < 3.0e-5f,
            "event 65+ did not reach DSP in sample order, or resync reverted it "
                "(max difference " + std::to_string (difference) + ")");

    midiDriven.releaseResources();
    reference.releaseResources();
}

// Coalescing a gap made only of single-parameter messages must retain their
// normal "this control and nothing else" semantics. A whole-shadow writeback
// would overwrite unrelated automation which happened after the audio snapshot
// but before the message thread finally drained it.
void testSingleParameterOverflowResyncPreservesLaterUnrelatedEdit()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    constexpr int parameter = static_cast<int> (sysex::ToneParameter::VcfFreq);
    constexpr int queuedValue = 11;
    constexpr int overflowValue = 103;
    std::array<std::uint8_t, sysex::parameterMessageBytes> queued {}, overflow {};
    const auto queuedWritten = sysex::writeParameterMessage (
        parameter, queuedValue, 0, queued.data(), queued.size());
    const auto overflowWritten = sysex::writeParameterMessage (
        parameter, overflowValue, 0, overflow.data(), overflow.size());

    juce::MidiBuffer midi;
    for (int event = 0; event < 64; ++event)
        midi.addEvent (juce::MidiMessage::createSysExMessage (
                           queued.data() + 1,
                           static_cast<int> (queuedWritten) - 2),
                       event);
    for (int event = 64; event < 70; ++event)
        midi.addEvent (juce::MidiMessage::createSysExMessage (
                           overflow.data() + 1,
                           static_cast<int> (overflowWritten) - 2),
                       event);

    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();
    processor.processBlock (buffer, midi);

    constexpr float laterRelease = 0.8123f;
    setParameterValue (processor, parameters::release, laterRelease);
    processor.flushPendingMidiEvents();
    expect (std::abs (parameterValue (processor, parameters::cutoff)
                     - static_cast<float> (overflowValue) / 127.0f) < 1.0e-5f,
            "single-parameter overflow did not reflect its final touched value");
    expect (std::abs (parameterValue (processor, parameters::release)
                     - laterRelease) < 1.0e-5f,
            "single-parameter overflow snapshot overwrote unrelated later automation");
    expect (processor.getMidiEventDroppedCount() == 0,
            "single-parameter overflow was reported as a hard drop");

    processor.releaseResources();
}

// The write half of the compatibility claim. A dump that only a subclass
// method could produce is not reachable by a DAW, so it has to leave through
// the plug-in's MIDI output.
void testRequestedDumpLeavesThroughTheMidiOutput()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, parameters::cutoff, 0.3125f);
    setParameterValue (processor, parameters::chorusII, 1.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    // Nothing asked for, nothing sent: this is a synth, not a MIDI thru.
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    buffer.clear();
    processor.processBlock (buffer, midi);
    expect (midi.isEmpty(), "the processor echoed its input to the MIDI output");

    processor.requestSysExDump();
    midi.clear();
    buffer.clear();
    processor.processBlock (buffer, midi);

    juce::MidiMessage found;
    int count = 0;
    for (const auto metadata : midi)
        if (metadata.getMessage().isSysEx())
        {
            found = metadata.getMessage();
            ++count;
        }
    expect (count == 1, "a requested dump did not appear exactly once on the output");

    sysex::Patch sent {};
    int channel = -1;
    expect (sysex::readPatchMessage (found.getRawData(),
                                     static_cast<std::size_t> (found.getRawDataSize()),
                                     sent, channel),
            "the emitted dump is not a readable patch message");
    expect (std::abs (sent.cutoff - 0.3125f) < 0.01f,
            "the emitted dump does not carry the current panel");
    expect (sent.chorus == ChorusMode::Two, "the emitted dump lost the chorus mode");

    expect (channel == 0, "a dump with no device seen did not default to channel 1");

    // The request is one-shot: it must not keep sending every block.
    midi.clear();
    buffer.clear();
    processor.processBlock (buffer, midi);
    expect (midi.isEmpty(), "the dump request repeated on the following block");

    // Each request owns its bytes until the audio side has copied them. With one
    // shared array, the second request replaced the first before it could be
    // sent (and could rewrite that array while the callback was reading it).
    // Queue two deliberately different snapshots before either is consumed;
    // they must leave whole, once each, and in request order.
    processor.setCurrentProgram (1);
    const auto firstExpected = processor.programPatch (1);
    processor.requestSysExDump();
    processor.setCurrentProgram (2);
    const auto secondExpected = processor.programPatch (2);
    processor.requestSysExDump();

    const auto expectNextDump = [&] (const sysex::Patch& expected,
                                     const char* position)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock (buffer, midi);

        sysex::Patch actual {};
        int actualChannel = -1;
        int dumpCount = 0;
        for (const auto metadata : midi)
            if (metadata.numBytes > 0 && metadata.data[0] == 0xf0)
            {
                ++dumpCount;
                expect (sysex::readPatchMessage (
                            metadata.data,
                            static_cast<std::size_t> (metadata.numBytes),
                            actual, actualChannel),
                        std::string (position) + " queued dump was malformed");
            }

        expect (dumpCount == 1,
                std::string (position) + " queued dump was not emitted once");
        std::array<std::uint8_t, sysex::toneByteCount> actualBytes {},
                                                          expectedBytes {};
        sysex::toneBytesFromPatch (actual, actualBytes.data());
        sysex::toneBytesFromPatch (expected, expectedBytes.data());
        expect (actualBytes == expectedBytes,
                std::string (position) + " queued dump was overwritten or torn");
        expect (actualChannel == 0,
                std::string (position) + " queued dump changed MIDI channel");
    };

    expectNextDump (firstExpected, "first");
    expectNextDump (secondExpected, "second");
    expect (processor.getSysExDumpDroppedCount() == 0,
            "two queued patch-dump requests overflowed the fixed handoff");

    processor.releaseResources();
}

// A saved program index has to come back, or the host's selector and the sound
// disagree after a reload.
void testSelectedProgramSurvivesAStateRoundTrip()
{
    YouKnow106AudioProcessor source;
    source.setCurrentProgram (3);
    juce::MemoryBlock state;
    source.getStateInformation (state);

    YouKnow106AudioProcessor destination;
    destination.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (destination.getCurrentProgram() == 3,
            "the selected program did not survive a state round trip");

    // Schema 2's indices named the former 32 custom sounds. Reusing one as a
    // historical-bank identity would preserve the audio parameters but attach
    // a false name and selector. Migration keeps the sound and clears only that
    // obsolete identity.
    const auto schema3Xml = juce::AudioProcessor::getXmlFromBinary (
        state.getData(), static_cast<int> (state.getSize()));
    expect (schema3Xml != nullptr,
            "could not decode the current program-state fixture");
    if (schema3Xml != nullptr)
    {
        auto schema2State = juce::ValueTree::fromXml (*schema3Xml);
        schema2State.setProperty ("stateSchemaVersion", 2, nullptr);
        schema2State.setProperty ("program", 17, nullptr);
        juce::MemoryBlock schema2Bytes;
        if (const auto xml = schema2State.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, schema2Bytes);
        else
            expect (false, "could not serialise the schema-2 program fixture");

        YouKnow106AudioProcessor migrated;
        migrated.setStateInformation (
            schema2Bytes.getData(), static_cast<int> (schema2Bytes.getSize()));
        std::array<std::uint8_t, sysex::toneByteCount> before {}, after {};
        sysex::toneBytesFromPatch (source.currentPatch(), before.data());
        sysex::toneBytesFromPatch (migrated.currentPatch(), after.data());
        expect (before == after,
                "factory-bank migration changed the saved sound");
        expect (migrated.getCurrentProgram() == 0,
                "schema-2 custom program was mislabeled as a historical tone");
        expect (migrated.currentProgramIsEdited(),
                "migrated custom sound was not marked as an edited panel");
    }

    // Older chunks have no program property at all. Loading one into an
    // already-used processor must not leave the previous selector attached to
    // unrelated restored parameters.
    auto oldState = source.parameters.copyState();
    oldState.removeProperty ("program", nullptr);
    juce::MemoryBlock oldBytes;
    if (const auto xml = oldState.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, oldBytes);
    else
        expect (false, "could not serialise a pre-program-index state");

    destination.setCurrentProgram (7);
    destination.setStateInformation (oldBytes.getData(),
                                     static_cast<int> (oldBytes.getSize()));
    expect (destination.getCurrentProgram() == 0,
            "a state with no program property retained a stale selection");
}

// Hosts are allowed to request state from a thread other than the message
// thread. A save racing a multi-parameter recall must contain either complete
// program, never one program index paired with a half-written tone from the
// other recall.
void testConcurrentProgramRecallSavesACoherentState()
{
    YouKnow106AudioProcessor source;
    source.setCurrentProgram (1);

    std::atomic<bool> start { false };
    std::atomic<bool> finished { false };
    std::thread recalls ([&]
    {
        while (! start.load (std::memory_order_acquire))
            std::this_thread::yield();

        for (int iteration = 0; iteration < 1000; ++iteration)
        {
            source.setCurrentProgram ((iteration & 1) == 0 ? 2 : 1);
            std::this_thread::yield();
        }
        finished.store (true, std::memory_order_release);
    });

    start.store (true, std::memory_order_release);
    int snapshots = 0;
    int snapshotsRequestedWhileRecalling = 0;
    do
    {
        if (! finished.load (std::memory_order_acquire))
            ++snapshotsRequestedWhileRecalling;

        juce::MemoryBlock state;
        source.getStateInformation (state);
        expect (! state.isEmpty(), "a concurrent state request returned no state");
        if (state.isEmpty())
            continue;

        YouKnow106AudioProcessor restored;
        restored.setStateInformation (state.getData(),
                                      static_cast<int> (state.getSize()));
        const int program = restored.getCurrentProgram();
        expect (program == 1 || program == 2,
                "a concurrent save carried an impossible program index");

        std::array<std::uint8_t, sysex::toneByteCount> restoredBytes {},
                                                          programBytes {};
        sysex::toneBytesFromPatch (restored.currentPatch(), restoredBytes.data());
        sysex::toneBytesFromPatch (restored.programPatch (program),
                                   programBytes.data());
        expect (restoredBytes == programBytes,
                "a concurrent save paired a program index with a hybrid recall");
        expect (! restored.currentProgramIsEdited(),
                "a concurrent program save reopened as an edited factory patch");
        ++snapshots;
    }
    while (snapshots < 64 || ! finished.load (std::memory_order_acquire));

    recalls.join();
    expect (snapshotsRequestedWhileRecalling > 0,
            "the concurrent state regression did not overlap a recall request");
}

void testForeignSysExLeavesThePatchAlone()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    const auto before = parameterValue (processor, parameters::cutoff);

    const std::array<std::uint8_t, 6> body { 0x43, 0x30, 0x00, 0x01, 0x02, 0x03 };
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::createSysExMessage (body.data(),
                                                          static_cast<int> (body.size())),
                   0);
    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();
    processor.processBlock (buffer, midi);
    processor.flushPendingMidiEvents();

    expect (std::abs (parameterValue (processor, parameters::cutoff) - before) < 1.0e-6f,
            "a foreign sysex message moved the patch");
    processor.releaseResources();
}

// A lane automating one of the previous release's ids still has to control
// the sound. The bridge forwards a change to the pair that replaced it.
void testLegacyAutomationIdsStillReachTheSwitches()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    // Unison through the old three-way id.
    setParameterValue (processor, parameters::legacyKeyMode, 2.0f);
    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::poly1) > 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "the legacy key mode did not reach both POLY buttons");

    setParameterValue (processor, parameters::legacyChorus, 2.0f);
    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::chorusII) > 0.5f
                && parameterValue (processor, parameters::chorusI) < 0.5f,
            "the legacy chorus id did not reach the chorus buttons");

    // The bridge must not fight the panel: a direct change to a switch stands
    // until the legacy id itself moves again.
    setParameterValue (processor, parameters::chorusII, 0.0f);
    setParameterValue (processor, parameters::chorusI, 1.0f);
    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::chorusI) > 0.5f
                && parameterValue (processor, parameters::chorusII) < 0.5f,
            "the legacy bridge overwrote a switch the player had just moved");

    processor.releaseResources();
}

// Ordering is based on the last actual edit, not on which compatibility path
// the 20 ms timer happens to inspect first. A modern pair edit made after an
// old-id write must win even when that old write has not yet been forwarded.
void testLaterEditorPairEditSupersedesPendingLegacyAutomation()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "cannot test pair authority without an editor");
    if (editor == nullptr)
        return;

    auto* chorusI = dynamic_cast<juce::Button*> (
        findDescendantNamed (*editor, "I"));
    auto* poly2 = dynamic_cast<juce::Button*> (
        findDescendantNamed (*editor, "POLY 2"));
    expect (chorusI != nullptr && poly2 != nullptr,
            "the editor pair-authority controls were not found");
    if (chorusI == nullptr || poly2 == nullptr)
        return;

    const auto previousModifiers = juce::ModifierKeys::currentModifiers;
    juce::ModifierKeys::currentModifiers = {};

    // Old Chorus II automation is pending; a later physical I click wins.
    setParameterValue (processor, parameters::legacyChorus, 2.0f);
    chorusI->setToggleState (true, juce::sendNotificationSync);
    expect (parameterValue (processor, parameters::chorusI) > 0.5f
                && parameterValue (processor, parameters::chorusII) < 0.5f,
            "the later Chorus I click did not establish the modern pair");

    // Likewise, pending old Unison followed by an ordinary POLY 2 contact is
    // Poly 2 now, including in audio before the timer has run.
    setParameterValue (processor, parameters::legacyKeyMode, 2.0f);
    poly2->onClick();
    expect (parameterValue (processor, parameters::poly1) < 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "the later Poly 2 click did not establish the modern pair");

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    buffer.clear();
    processor.processBlock (buffer, midi);
    renderBlocks (processor, buffer, 3);
    expect (processor.getActiveVoiceCount() == 1,
            "audio rendered pending legacy Unison over the later Poly 2 click");

    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::chorusI) > 0.5f
                && parameterValue (processor, parameters::chorusII) < 0.5f,
            "the timer overwrote later Chorus I with pending legacy II");
    expect (parameterValue (processor, parameters::poly1) < 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "the timer overwrote later Poly 2 with pending legacy Unison");

    juce::ModifierKeys::currentModifiers = previousModifiers;
    processor.releaseResources();
}

// A selected hardware contact is still an event when its lamp does not move.
// Consequently a repeated Poly/Unison press is newer than pending automation
// on the obsolete three-way id, just like a pair-changing press is.
void testRepeatedPolyPressSupersedesPendingLegacyAutomation()
{
    const auto previousModifiers = juce::ModifierKeys::currentModifiers;

    {
        YouKnow106AudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto* poly1 = editor == nullptr ? nullptr : dynamic_cast<juce::Button*> (
            findDescendantNamed (*editor, "POLY 1"));
        expect (poly1 != nullptr,
                "cannot test repeated Poly 1 authority without its editor button");
        if (poly1 != nullptr)
        {
            juce::ModifierKeys::currentModifiers = {};
            setParameterValue (processor, parameters::legacyKeyMode, 2.0f);
            poly1->onClick(); // selected lamp remains on, but the contact repeats

            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            renderBlocks (processor, buffer, 3);
            expect (processor.getActiveVoiceCount() == 1,
                    "pending legacy Unison beat a later repeated Poly 1 press");

            processor.forwardLegacyModeParametersForTest();
            expect (parameterValue (processor, parameters::poly1) > 0.5f
                        && parameterValue (processor, parameters::poly2) < 0.5f,
                    "the timer overwrote a later repeated Poly 1 press");
        }
        processor.releaseResources();
    }

    {
        YouKnow106AudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto* poly1 = editor == nullptr ? nullptr : dynamic_cast<juce::Button*> (
            findDescendantNamed (*editor, "POLY 1"));
        expect (poly1 != nullptr,
                "cannot test repeated Unison authority without a POLY button");
        if (poly1 != nullptr)
        {
            juce::ModifierKeys::currentModifiers =
                juce::ModifierKeys (juce::ModifierKeys::shiftModifier);
            poly1->onClick(); // enter Unison
            setParameterValue (processor, parameters::legacyKeyMode, 1.0f);
            poly1->onClick(); // repeat the already-selected simultaneous press

            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            renderBlocks (processor, buffer, 3);
            expect (processor.getActiveVoiceCount() == processor.getVoiceLimitForDisplay(),
                    "pending legacy Poly 2 beat a later repeated Unison press");

            processor.forwardLegacyModeParametersForTest();
            expect (parameterValue (processor, parameters::poly1) > 0.5f
                        && parameterValue (processor, parameters::poly2) > 0.5f,
                    "the timer overwrote a later repeated Unison press");
        }
        processor.releaseResources();
    }

    juce::ModifierKeys::currentModifiers = previousModifiers;
}

// The bridge's other half. Forwarding to the host-visible pair is the message
// thread's job, but an offline render can finish without the message loop ever
// running, so the audio thread has to resolve the legacy id itself.
void testLegacyAutomationIsHeardWithoutTheMessageThread()
{
    YouKnow106AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);

    // How many voices one note lights. Unison stacks the whole card set on it;
    // any poly mode uses one.
    const auto voicesForOneNote = [&processor, &buffer]
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        renderBlocks (processor, buffer, 3);
        const int voices = processor.getActiveVoiceCount();

        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::allSoundOff (1), 0);
        buffer.clear();
        processor.processBlock (buffer, off);
        renderBlocks (processor, buffer, 3);
        return voices;
    };

    expect (voicesForOneNote() == 1, "a poly patch stacked more than one voice on a note");
    expect (processor.getActiveVoiceCount() == 0, "all sound off left a voice running");

    // Deliberately no forwardLegacyModeParametersForTest() here: the point is
    // that the sound changes without it.
    setParameterValue (processor, parameters::legacyKeyMode, 2.0f);
    expect (voicesForOneNote() == processor.getVoiceLimitForDisplay(),
            "a legacy unison never reached the audio without the message thread");

    // And control goes back to the pair once it has caught up, so a direct edit
    // is not held off by a legacy id that is no longer moving.
    processor.forwardLegacyModeParametersForTest();
    setParameterValue (processor, parameters::poly2, 0.0f);
    expect (voicesForOneNote() == 1,
            "the audio bridge kept holding unison after the switches moved off it");

    processor.releaseResources();
}

// The message-thread bridge can run before the audio thread has observed the
// legacy id. A later preset recall is newer than that forwarding and must win;
// otherwise the panel shows chorus I while the engine remains stuck on II.
void testForwardedLegacyChorusDoesNotOverrideALaterPresetRecall()
{
    YouKnow106AudioProcessor recalled;
    setParameterValue (recalled, parameters::legacyChorus, 2.0f);
    recalled.forwardLegacyModeParametersForTest();
    recalled.setCurrentProgram (1); // A11 stores chorus I.

    expect (parameterValue (recalled, parameters::chorusI) > 0.5f
                && parameterValue (recalled, parameters::chorusII) < 0.5f,
            "recalling a chorus-I preset did not replace forwarded chorus II");

    YouKnow106AudioProcessor reference;
    reference.setCurrentProgram (1);

    // A known-wrong comparison proves the audio equality below is capable of
    // distinguishing the two clock rates, rather than merely comparing silence.
    YouKnow106AudioProcessor chorusTwo;
    chorusTwo.setCurrentProgram (1);
    setParameterValue (chorusTwo, parameters::chorusI, 0.0f);
    setParameterValue (chorusTwo, parameters::chorusII, 1.0f);

    for (auto* processor : { &recalled, &reference, &chorusTwo })
    {
        setParameterValue (*processor, parameters::calibration, 0.0f);
        setParameterValue (*processor, parameters::chorusNoise, 0.0f);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    juce::AudioBuffer<float> recalledBuffer (2, blockSize);
    juce::AudioBuffer<float> referenceBuffer (2, blockSize);
    juce::AudioBuffer<float> chorusTwoBuffer (2, blockSize);
    float recalledDifference = 0.0f;
    float wrongModeDifference = 0.0f;
    float peak = 0.0f;

    for (int block = 0; block < 48; ++block)
    {
        juce::MidiBuffer recalledMidi, referenceMidi, chorusTwoMidi;
        if (block == 0)
        {
            const auto note = juce::MidiMessage::noteOn (1, 60, 0.8f);
            recalledMidi.addEvent (note, 0);
            referenceMidi.addEvent (note, 0);
            chorusTwoMidi.addEvent (note, 0);
        }

        recalledBuffer.clear();
        referenceBuffer.clear();
        chorusTwoBuffer.clear();
        recalled.processBlock (recalledBuffer, recalledMidi);
        reference.processBlock (referenceBuffer, referenceMidi);
        chorusTwo.processBlock (chorusTwoBuffer, chorusTwoMidi);
        recalledDifference = std::max (
            recalledDifference,
            maximumBufferDifference (recalledBuffer, referenceBuffer));
        wrongModeDifference = std::max (
            wrongModeDifference,
            maximumBufferDifference (chorusTwoBuffer, referenceBuffer));
        peak = std::max (peak, bufferPeak (referenceBuffer));
    }

    expect (peak > 0.001f, "legacy chorus regression rendered silence");
    expect (wrongModeDifference > 1.0e-5f,
            "the legacy chorus regression cannot distinguish chorus I from II");
    expect (recalledDifference < 1.0e-6f,
            "forwarded legacy chorus II overrode the later chorus-I recall in DSP");

    recalled.releaseResources();
    reference.releaseResources();
    chorusTwo.releaseResources();
}

// The inverse ordering is just as important: a host can write the old chorus
// id and then recall a patch before either the audio bridge or the 20 ms timer
// observes that write. The later patch must win both possible races.
void testPendingLegacyChorusDoesNotOverrideALaterPresetRecall()
{
    YouKnow106AudioProcessor audioFirst;
    setParameterValue (audioFirst, parameters::legacyChorus, 2.0f);
    audioFirst.setCurrentProgram (1); // A11 stores chorus I.

    YouKnow106AudioProcessor timerFirst;
    setParameterValue (timerFirst, parameters::legacyChorus, 2.0f);
    timerFirst.setCurrentProgram (1);
    timerFirst.forwardLegacyModeParametersForTest();
    expect (parameterValue (timerFirst, parameters::chorusI) > 0.5f
                && parameterValue (timerFirst, parameters::chorusII) < 0.5f,
            "a delayed legacy timer overwrote the later preset recall");

    YouKnow106AudioProcessor reference;
    reference.setCurrentProgram (1);

    for (auto* processor : { &audioFirst, &timerFirst, &reference })
    {
        setParameterValue (*processor, parameters::calibration, 0.0f);
        setParameterValue (*processor, parameters::chorusNoise, 0.0f);
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }

    juce::AudioBuffer<float> audioFirstBuffer (2, blockSize);
    juce::AudioBuffer<float> timerFirstBuffer (2, blockSize);
    juce::AudioBuffer<float> referenceBuffer (2, blockSize);
    float audioFirstDifference = 0.0f;
    float timerFirstDifference = 0.0f;
    float peak = 0.0f;

    for (int block = 0; block < 48; ++block)
    {
        juce::MidiBuffer audioFirstMidi, timerFirstMidi, referenceMidi;
        if (block == 0)
        {
            const auto note = juce::MidiMessage::noteOn (1, 60, 0.8f);
            audioFirstMidi.addEvent (note, 0);
            timerFirstMidi.addEvent (note, 0);
            referenceMidi.addEvent (note, 0);
        }

        audioFirstBuffer.clear();
        timerFirstBuffer.clear();
        referenceBuffer.clear();
        audioFirst.processBlock (audioFirstBuffer, audioFirstMidi);
        timerFirst.processBlock (timerFirstBuffer, timerFirstMidi);
        reference.processBlock (referenceBuffer, referenceMidi);
        audioFirstDifference = std::max (
            audioFirstDifference,
            maximumBufferDifference (audioFirstBuffer, referenceBuffer));
        timerFirstDifference = std::max (
            timerFirstDifference,
            maximumBufferDifference (timerFirstBuffer, referenceBuffer));
        peak = std::max (peak, bufferPeak (referenceBuffer));
    }

    expect (peak > 0.001f, "pending legacy chorus regression rendered silence");
    expect (audioFirstDifference < 1.0e-6f,
            "unseen legacy chorus II overrode the later recall in DSP");
    expect (timerFirstDifference < 1.0e-6f,
            "delayed legacy forwarding changed the recalled chorus in DSP");

    audioFirst.releaseResources();
    timerFirst.releaseResources();
    reference.releaseResources();
}

void testLegacyForwarderCanonicalisesBothPolyLampsOff()
{
    YouKnow106AudioProcessor processor;
    setParameterValue (processor, parameters::poly1, 0.0f);
    setParameterValue (processor, parameters::poly2, 0.0f);
    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::poly1) > 0.5f
                && parameterValue (processor, parameters::poly2) < 0.5f,
            "direct host automation left both POLY lamps dark");

    // The repair is specific to the invalid 00 state; valid solo and unison
    // latches must remain untouched.
    setParameterValue (processor, parameters::poly1, 0.0f);
    setParameterValue (processor, parameters::poly2, 1.0f);
    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::poly1) < 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "live POLY canonicalisation changed a valid Poly 2 latch");

    setParameterValue (processor, parameters::poly1, 1.0f);
    processor.forwardLegacyModeParametersForTest();
    expect (parameterValue (processor, parameters::poly1) > 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "live POLY canonicalisation changed a valid Unison latch");
}

// A restore rewrites the legacy ids and the pairs together. Reading the
// restored legacy value as a fresh edit would forward it over the pair the
// session actually saved -- and the old three-way ids cannot even say what the
// pairs can, so the settings it destroys are exactly the new ones.
void testRestoringASessionDoesNotOverwriteItsOwnModeSwitches()
{
    YouKnow106AudioProcessor source;
    // Reach Unison and Chorus II through the legacy ids, then present the
    // invalid both-buttons-on state that a short-lived older build could save.
    // Restore keeps Unison, but canonicalises that chorus state to II because
    // the JUNO-106 cannot engage both modes simultaneously.
    setParameterValue (source, parameters::legacyKeyMode, 2.0f);
    setParameterValue (source, parameters::legacyChorus, 2.0f);
    source.forwardLegacyModeParametersForTest();
    setParameterValue (source, parameters::chorusI, 1.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    YouKnow106AudioProcessor restored;
    restored.prepareToPlay (sampleRate, blockSize);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    restored.forwardLegacyModeParametersForTest();

    expect (parameterValue (restored, parameters::chorusI) < 0.5f
                && parameterValue (restored, parameters::chorusII) > 0.5f,
            "restoring both chorus buttons did not canonicalise the state to II");
    expect (parameterValue (restored, parameters::poly1) > 0.5f
                && parameterValue (restored, parameters::poly2) > 0.5f,
            "the first tick after a restore put the assign mode back to the legacy id");

    // The audio thread keeps its own baseline, so it needs reseeding too.
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    buffer.clear();
    restored.processBlock (buffer, midi);
    renderBlocks (restored, buffer, 3);
    expect (restored.getActiveVoiceCount() == restored.getVoiceLimitForDisplay(),
            "the restored session rendered the legacy assign mode, not its own");

    restored.releaseResources();
}

// A full state restore is authoritative for both split mode pairs. Legacy host
// writes queued immediately before it must not leak through later merely because
// neither the audio bridge nor the message-thread timer saw them first.
void testPendingLegacyModesDoNotOverrideALaterStateRestore()
{
    YouKnow106AudioProcessor source;
    setParameterValue (source, parameters::poly1, 0.0f);
    setParameterValue (source, parameters::poly2, 1.0f); // saved Poly 2
    setParameterValue (source, parameters::chorusI, 1.0f);
    setParameterValue (source, parameters::chorusII, 0.0f);
    setParameterValue (source, parameters::calibration, 0.0f);
    setParameterValue (source, parameters::chorusNoise, 0.0f);
    juce::MemoryBlock state;
    source.getStateInformation (state);

    YouKnow106AudioProcessor timerFirst;
    setParameterValue (timerFirst, parameters::legacyKeyMode, 2.0f);
    setParameterValue (timerFirst, parameters::legacyChorus, 2.0f);
    timerFirst.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    timerFirst.forwardLegacyModeParametersForTest();
    expect (parameterValue (timerFirst, parameters::poly1) < 0.5f
                && parameterValue (timerFirst, parameters::poly2) > 0.5f,
            "a delayed legacy key-mode timer overwrote the later state restore");
    expect (parameterValue (timerFirst, parameters::chorusI) > 0.5f
                && parameterValue (timerFirst, parameters::chorusII) < 0.5f,
            "a delayed legacy chorus timer overwrote the later state restore");

    YouKnow106AudioProcessor audioFirst;
    YouKnow106AudioProcessor reference;
    for (auto* processor : { &audioFirst, &reference })
    {
        processor->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        processor->prepareToPlay (sampleRate, blockSize);
    }
    setParameterValue (audioFirst, parameters::legacyKeyMode, 2.0f);
    setParameterValue (audioFirst, parameters::legacyChorus, 2.0f);
    audioFirst.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    reference.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    juce::AudioBuffer<float> audioFirstBuffer (2, blockSize);
    juce::AudioBuffer<float> referenceBuffer (2, blockSize);
    float difference = 0.0f;
    float peak = 0.0f;
    for (int block = 0; block < 48; ++block)
    {
        juce::MidiBuffer audioFirstMidi, referenceMidi;
        if (block == 0)
        {
            const auto note = juce::MidiMessage::noteOn (1, 60, 0.8f);
            audioFirstMidi.addEvent (note, 0);
            referenceMidi.addEvent (note, 0);
        }
        audioFirstBuffer.clear();
        referenceBuffer.clear();
        audioFirst.processBlock (audioFirstBuffer, audioFirstMidi);
        reference.processBlock (referenceBuffer, referenceMidi);
        difference = std::max (
            difference, maximumBufferDifference (audioFirstBuffer, referenceBuffer));
        peak = std::max (peak, bufferPeak (referenceBuffer));
    }

    expect (peak > 0.001f, "pending legacy state-restore regression rendered silence");
    expect (difference < 1.0e-6f,
            "unseen legacy mode writes changed the later restored state in DSP");
    expect (audioFirst.getActiveVoiceCount() == 1,
            "pending legacy Unison survived a later Poly 2 state restore");

    audioFirst.releaseResources();
    reference.releaseResources();
}

// A host may restore its state, write the first automation point and only then
// ask for the first audio block. Reseeding the compatibility bridge from the
// live parameter at block time would mistake that automation for the restored
// baseline and swallow it.
void testLegacyAutomationBetweenRestoreAndFirstBlockIsHonoured()
{
    YouKnow106AudioProcessor source;
    setParameterValue (source, parameters::legacyKeyMode, 0.0f);
    setParameterValue (source, parameters::poly1, 1.0f);
    setParameterValue (source, parameters::poly2, 0.0f);
    juce::MemoryBlock state;
    source.getStateInformation (state);

    YouKnow106AudioProcessor restored;
    restored.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    restored.prepareToPlay (sampleRate, blockSize);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    // Deliberately do not run the message-thread forwarding helper. This is an
    // automation write after restore and the audio thread must hear it itself.
    setParameterValue (restored, parameters::legacyKeyMode, 2.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    buffer.clear();
    restored.processBlock (buffer, midi);
    renderBlocks (restored, buffer, 3);

    expect (restored.getActiveVoiceCount() == restored.getVoiceLimitForDisplay(),
            "legacy automation between restore and first audio was swallowed");
    restored.releaseResources();
}

// The product program bar recalls a complete plug-in state. Its EDITED lamp
// therefore follows both the JUNO tone-memory bytes and the surrounding
// performance/model controls which a host program also restores.
void testEditedFlagFollowsTheCompleteProgram()
{
    YouKnow106AudioProcessor processor;
    processor.setCurrentProgram (3);
    expect (! processor.currentProgramIsEdited(),
            "a freshly recalled patch already claims to be edited");

    setParameterValue (processor, parameters::volume, 0.42f);
    expect (processor.currentProgramIsEdited(),
            "moving product-program volume did not count as an edit");

    processor.setCurrentProgram (3);
    expect (! processor.currentProgramIsEdited(),
            "reloading the program did not restore its volume");

    setParameterValue (processor, parameters::cutoff, 0.137f);
    expect (processor.currentProgramIsEdited(),
            "moving the cutoff away from the recalled patch went unnoticed");

    // Recalling the same program again is how the edits are thrown away.
    processor.setCurrentProgram (3);
    expect (! processor.currentProgramIsEdited(),
            "reloading the program did not restore it");

    // INIT is a program like any other, and every factory patch has to be
    // reachable without the bar reporting an edit that was never made.
    for (int index = 0; index < processor.getNumPrograms(); ++index)
    {
        processor.setCurrentProgram (index);
        expect (! processor.currentProgramIsEdited(),
                std::string ("program ") + std::to_string (index)
                    + " does not survive being recalled");
    }

    // Each visible parameter must independently light EDITED. Poisoning the
    // entire state at once would let one checked field conceal another that
    // had accidentally been omitted from the comparison.
    for (const auto& expected : expectedParameters)
    {
        if (std::strcmp (expected.id, parameters::legacyKeyMode) == 0
            || std::strcmp (expected.id, parameters::legacyChorus) == 0)
            continue;

        processor.setCurrentProgram (0);
        auto* parameter = processor.parameters.getParameter (expected.id);
        expect (parameter != nullptr,
                std::string ("cannot edit program parameter ") + expected.id);
        if (parameter == nullptr)
            continue;
        const float edit = parameter->getValue() < 0.5f ? 0.87f : 0.13f;
        parameter->setValueNotifyingHost (edit);
        expect (processor.currentProgramIsEdited(),
                std::string ("EDITED ignored parameter ") + expected.id);
    }
}

void testFactoryProgramsLoad()
{
    YouKnow106AudioProcessor processor;
    // Program 0 is the init patch, so the bank sits at 1..N.
    expect (processor.getNumPrograms() == presets::presetCount + 1,
            "the host does not see the whole factory bank");
    expect (processor.getProgramName (0) == "INIT",
            "program 0 is not the init patch");
    // A freshly constructed processor holds the layout defaults, so the program
    // it reports has to be the one that actually describes them.
    expect (processor.getCurrentProgram() == 0,
            "a new processor claims a factory program it has not loaded");
    expect (processor.programPatch (0).attack == 0.0f
                && parameterValue (processor, parameters::attack) == 0.0f,
            "INIT still hides a non-zero attack behind the bottom slider step");

    for (int index = 0; index < processor.getNumPrograms(); ++index)
        expect (processor.getProgramName (index).isNotEmpty(),
                "a factory program has no name");
    expect (processor.getProgramName (-1).isEmpty(),
            "a negative program index returned a name");
    expect (processor.getProgramName (processor.getNumPrograms()).isEmpty(),
            "a program index past the end returned a name");

    const auto& bank = presets::factoryBank();
    expect (bank[13].controls.transpose == 12
                && bank[16].controls.transpose == 12
                && bank[58].controls.transpose == -12,
            "archival octave-playing directions are missing");
    expect (bank[31].controls.keyMode == KeyMode::Unison
                && bank[43].controls.keyMode == KeyMode::Unison
                && bank[44].controls.keyMode == KeyMode::Unison
                && bank[45].controls.keyMode == KeyMode::Unison,
            "archival unison-playing directions are missing");

    processor.setCurrentProgram (32); // A48 Synth Bass I (unison)
    expect (parameterValue (processor, parameters::poly1) > 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "a product recall did not apply the factory sound's unison direction");
    processor.setCurrentProgram (14); // A26 Celeste, one octave up
    expect (std::abs (parameterValue (processor, parameters::transpose) - 12.0f)
                < 1.0e-4f,
            "a product recall did not apply the factory sound's octave direction");

    // The host/program-bar abstraction is a complete product preset, so it
    // restores the plug-in controls around the hardware-format tone too.
    setParameterValue (processor, parameters::volume, 0.42f);
    setParameterValue (processor, parameters::portamento, 0.33f);
    processor.setCurrentProgram (2);
    expect (processor.getCurrentProgram() == 2, "the selected program was not kept");

    const auto& expected = presets::factoryBank()[1].patch;
    expect (std::abs (parameterValue (processor, parameters::cutoff) - expected.cutoff)
                < 0.01f,
            "selecting a program did not load its cutoff");
    const auto& expectedControls = presets::factoryBank()[1].controls;
    expect (std::abs (parameterValue (processor, parameters::volume)
                     - expectedControls.volume) < 1.0e-4f,
            "loading a product program did not restore volume");
    expect (std::abs (parameterValue (processor, parameters::portamento)
                     - expectedControls.portamento) < 1.0e-4f,
            "loading a product program did not restore portamento");

    // INIT is the complete default product state, not only an empty hardware
    // tone. It must clear edits to extension and performance controls too.
    setParameterValue (processor, parameters::volume, 0.42f);
    setParameterValue (processor, parameters::transpose, 5.0f);
    setParameterValue (processor, parameters::attack, 0.75f);
    processor.setCurrentProgram (0);
    const presets::Preset::Controls initControls {};
    expect (std::abs (parameterValue (processor, parameters::volume)
                     - initControls.volume) < 1.0e-4f,
            "selecting INIT did not restore volume");
    expect (std::abs (parameterValue (processor, parameters::transpose)
                     - static_cast<float> (initControls.transpose)) < 1.0e-4f,
            "selecting INIT did not restore transpose");
    expect (std::abs (parameterValue (processor, parameters::cutoff) - 0.62f) < 0.01f,
            "selecting INIT did not restore the default patch");
    expect (parameterValue (processor, parameters::attack) == 0.0f,
            "selecting INIT did not restore the true minimum attack");
    processor.setCurrentProgram (2);

    processor.setCurrentProgram (-1);
    expect (processor.getCurrentProgram() == 2,
            "an out-of-range program index changed the selection");
    processor.setCurrentProgram (processor.getNumPrograms());
    expect (processor.getCurrentProgram() == 2,
            "a program index past the end changed the selection");

    // The 106 has only off, I and II, and every factory entry must therefore
    // survive its real patch-memory representation without an invented I+II
    // suffix or a change to its effective 7-bit state.
    for (int index = 1; index < processor.getNumPrograms(); ++index)
    {
        expect (presets::factoryBank()[static_cast<std::size_t> (index - 1)]
                    .exportsLosslessly(),
                "a factory preset does not survive patch memory");
        expect (! processor.getProgramName (index).contains ("I+II"),
                "a factory preset still advertises an impossible chorus mode");
    }
}

void testEveryProductProgramRestoresEveryParameter()
{
    YouKnow106AudioProcessor processor;

    for (int program = 0; program < processor.getNumPrograms(); ++program)
    {
        processor.setCurrentProgram (program);
        std::array<float, expectedParameters.size()> recalledValues {};
        for (std::size_t index = 0; index < expectedParameters.size(); ++index)
            recalledValues[index] = parameterValue (
                processor, expectedParameters[index].id);

        // Move every APVTS value to the opposite side of its normalised range.
        // This includes the hidden compatibility choices, so a recall cannot
        // leave stale state behind the two physical button pairs either.
        for (const auto& expected : expectedParameters)
        {
            auto* parameter = processor.parameters.getParameter (expected.id);
            expect (parameter != nullptr,
                    std::string ("cannot poison program parameter ") + expected.id);
            if (parameter == nullptr)
                continue;
            const float poison = parameter->getValue() < 0.5f ? 0.87f : 0.13f;
            parameter->setValueNotifyingHost (poison);
        }

        expect (processor.currentProgramIsEdited(),
                std::string ("program ") + std::to_string (program)
                    + " ignored edits to its complete state");
        processor.setCurrentProgram (program);

        for (std::size_t index = 0; index < expectedParameters.size(); ++index)
        {
            const auto& expected = expectedParameters[index];
            expect (std::abs (parameterValue (processor, expected.id)
                             - recalledValues[index]) <= expected.tolerance,
                    std::string ("program ") + std::to_string (program)
                        + " did not restore parameter " + expected.id);
        }
        expect (! processor.currentProgramIsEdited(),
                std::string ("program ") + std::to_string (program)
                    + " remains edited after complete recall");
    }
}

void testEveryPanelLegendFitsInTheRealFont()
{
    const auto boldFont = [] (float height) {
        auto font = juce::Font (juce::FontOptions (height, juce::Font::bold));
        font.setHorizontalScale (panel::typefaceHorizontalScale);
        return font;
    };
    // expect() takes a std::string; every message here is built as a
    // juce::String, so it is converted at the point of use.
    const auto say = [] (const juce::String& text) { return text.toStdString(); };

    for (const auto& section : panel::sections())
    {
        const float available = section.width - panel::sectionPadding;
        const float drawn = juce::GlyphArrangement::getStringWidth (
            boldFont (panel::headerPointSize), section.name);
        expect (drawn <= available,
                say (juce::String ("section header is truncated: ") + section.name
                     + " needs " + juce::String (drawn, 1) + " in "
                     + juce::String (available, 1)));
        // The approximation must also be an over-estimate, not merely close:
        // that is the property the JUCE-free check depends on.
        expect (panel::textWidth (section.name, panel::headerPointSize, true) >= drawn,
                say (juce::String ("the width model under-estimates ") + section.name));
    }

    for (const auto& control : panel::controls())
    {
        if (control.kind == panel::ControlKind::Slider
            || control.kind == panel::ControlKind::Steps)
        {
            const float drawn = juce::GlyphArrangement::getStringWidth (
                boldFont (panel::labelPointSize), control.label);
            expect (drawn <= control.labelWidth,
                    say (juce::String ("slider legend is truncated: ") + control.label
                         + " needs " + juce::String (drawn, 1) + " in "
                         + juce::String (control.labelWidth, 1)));
            expect (panel::textWidth (control.label, panel::labelPointSize, true)
                        >= drawn,
                    say (juce::String ("the width model under-estimates ")
                         + control.label));
        }
        else
        {
            const float size = panel::buttonPointSizeFor (control);
            expect (size >= panel::buttonPointSizeMin,
                    say (juce::String ("button legend is set too small to read: ")
                         + control.label));
            const float drawn =
                juce::GlyphArrangement::getStringWidth (boldFont (size), control.label);
            expect (drawn <= control.width,
                    say (juce::String ("button legend is truncated: ") + control.label
                         + " needs " + juce::String (drawn, 1) + " in "
                         + juce::String (control.width, 1)));
        }
    }

    // The nominal layout can fit while the smallest supported editor silently
    // turns ten-point legends into eight-point ones. Check the actual font and
    // the actual scaled boxes at the resize floor as a separate contract.
    const float minimumScale = juce::jmin (
        static_cast<float> (panel::minimumEditorWidth) / panel::panelWidth(),
        static_cast<float> (panel::minimumEditorHeight)
            / panel::editorHeight);

    for (const auto& section : panel::sections())
    {
        const float size = juce::jmax (10.0f,
                                       panel::headerPointSize * minimumScale);
        const float drawn = juce::GlyphArrangement::getStringWidth (
            boldFont (size), section.name);
        expect (drawn <= (section.width - panel::sectionPadding) * minimumScale,
                say (juce::String ("minimum-size header is truncated: ")
                     + section.name));
    }

    for (const auto& control : panel::controls())
    {
        if (control.kind == panel::ControlKind::Slider
            || control.kind == panel::ControlKind::Steps)
        {
            const float size = juce::jmax (10.0f,
                                           panel::labelPointSize * minimumScale);
            const float drawn = juce::GlyphArrangement::getStringWidth (
                boldFont (size), control.label);
            expect (drawn <= control.labelWidth * minimumScale,
                    say (juce::String ("minimum-size slider legend is truncated: ")
                         + control.label));
            continue;
        }

        // These are fixed vector marks at runtime; their accessible names do
        // not constrain the visible icon's point size.
        if (std::strcmp (control.label, "PULSE") == 0
            || std::strcmp (control.label, "SAW") == 0
            || std::strcmp (control.label, "16'") == 0
            || std::strcmp (control.label, "8'") == 0
            || std::strcmp (control.label, "4'") == 0)
            continue;

        const float size = panel::buttonPointSizeFor (
            control.label, control.width * minimumScale,
            control.height * minimumScale);
        expect (size >= panel::buttonPointSizeMin,
                say (juce::String ("minimum-size button legend is too small: ")
                     + control.label));
    }
}

void testEveryInteractiveEditorControlExplainsItself()
{
    YouKnow106AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "cannot audit contextual help without an editor");
    if (editor == nullptr)
        return;

    int interactiveCount = 0;
    const auto audit = [&interactiveCount] (auto&& self,
                                            juce::Component& parent) -> void
    {
        for (int index = 0; index < parent.getNumChildComponents(); ++index)
        {
            auto* component = parent.getChildComponent (index);
            const bool isInteractive =
                dynamic_cast<juce::Slider*> (component) != nullptr
                || dynamic_cast<juce::Button*> (component) != nullptr
                || dynamic_cast<juce::ComboBox*> (component) != nullptr
                || dynamic_cast<juce::MidiKeyboardComponent*> (component) != nullptr
                || dynamic_cast<YouKnow106PerformanceLever*> (component) != nullptr;
            if (isInteractive)
            {
                ++interactiveCount;
                const auto identity = component->getName().isNotEmpty()
                    ? component->getName().toStdString()
                    : std::string ("unnamed interactive control");
                auto* tooltipClient = dynamic_cast<juce::TooltipClient*> (component);
                expect (tooltipClient != nullptr,
                        identity + " cannot supply contextual help");
                if (tooltipClient != nullptr)
                {
                    const auto tooltip = tooltipClient->getTooltip().trim();
                    expect (tooltip.length() >= 24,
                            identity + " has no meaningful contextual help");
                    expect (tooltip != component->getName(),
                            identity + " help merely repeats the control name");
                }
            }
            else
            {
                // A public composite control owns its private JUCE children.
                // Recurse only through non-interactive containers, otherwise
                // hidden keyboard scroll buttons and ComboBox labels would be
                // mistaken for additional product controls.
                self (self, *component);
            }
        }
    };
    audit (audit, *editor);

    // Six extension knobs, six compact operation buttons, four patch-bar
    // controls, the keybed and the vector lever. Patch dumping remains a
    // processor/MIDI capability, but no longer takes scarce front-panel area.
    constexpr int expectedInteractiveCount =
        panel::controlCount + 6 + 6 + 4 + 1 + 1;
    expect (interactiveCount == expectedInteractiveCount,
            "the contextual-help audit did not cover every interactive control");
    expect (findDescendantButtonWithText (*editor, "SEND") == nullptr,
            "the removed SEND operation returned to the compact editor");
}

void testPersistentContextHelpAndValueBubbles()
{
    YouKnow106AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "cannot test the contextual-help strip");
    if (editor == nullptr)
        return;

    auto* help = dynamic_cast<YouKnow106ContextHelp*> (
        findDescendantNamed (*editor, "Context help"));
    expect (help != nullptr, "the editor has no fixed contextual-help strip");
    if (help == nullptr)
        return;

    const auto idleText = help->getHelpText();
    const auto stableBounds = help->getBounds();
    expect (help->getHelpTitle() == "HELP" && idleText.length() >= 24,
            "the help strip has no useful idle prompt");
    expect (! containsTooltipWindow (*editor),
            "descriptive help still creates a floating TooltipWindow");

    for (const auto* name : { "FREQ", "HQ", "Patch selector",
                              "Playable keyboard", "Status display",
                              "Pitch and modulation lever" })
    {
        auto* target = findDescendantNamed (*editor, name);
        expect (target != nullptr,
                std::string ("cannot route contextual help for ") + name);
        if (target == nullptr)
            continue;
        auto* client = dynamic_cast<juce::TooltipClient*> (target);
        expect (client != nullptr,
                std::string (name) + " has no TooltipClient source");
        if (client == nullptr)
            continue;

        help->showFor (target);
        expect (help->getHelpText() == client->getTooltip().trim(),
                std::string ("the help strip changed the explanation for ") + name);
        expect (help->getHelpTitle().isNotEmpty()
                    && help->getHelpTitle() != "HELP",
                std::string ("the help strip omitted the title for ") + name);
        expect (help->getBounds() == stableBounds,
                "the fixed help area moved when its content changed");
    }

    auto* patchBox = dynamic_cast<juce::ComboBox*> (
        findDescendantNamed (*editor, "Patch selector"));
    expect (patchBox != nullptr && patchBox->getNumChildComponents() > 0,
            "the patch selector has no nested child to exercise help routing");
    if (patchBox != nullptr && patchBox->getNumChildComponents() > 0)
    {
        help->showFor (patchBox->getChildComponent (0));
        expect (help->getHelpText() == patchBox->getTooltip().trim(),
                "context help did not climb from a ComboBox child");
    }

    help->showFor (editor.get());
    expect (help->getHelpTitle() == "HELP" && help->getHelpText() == idleText,
            "an unannotated area left stale contextual help behind");

    // Numeric readouts are JUCE Slider popups, independent of the removed
    // descriptive TooltipWindow. Exercise every no-text-box slider so the new
    // fixed help presentation cannot silently remove exact values.
    std::vector<juce::Slider*> sliders;
    collectDescendantsOfType (*editor, sliders);
    int expectedSliders = 6;
    for (const auto& control : panel::controls())
        if (control.kind == panel::ControlKind::Slider
            || control.kind == panel::ControlKind::Steps)
            ++expectedSliders;
    expect (static_cast<int> (sliders.size()) == expectedSliders,
            "the value-bubble audit missed a slider");

    for (auto* slider : sliders)
    {
        const double previous = slider->getValue();
        help->showFor (slider);
        const auto helpBeforeValuePopup = help->getHelpText();
        const auto centre = slider->getLocalBounds().getCentre().toFloat();
        slider->mouseDown (mouseEventFor (
            *slider, centre,
            juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
        expect (slider->getCurrentPopupDisplay() != nullptr,
                slider->getName().toStdString()
                    + " has no numeric value bubble while being adjusted");
        expect (help->getHelpText() == helpBeforeValuePopup,
                slider->getName().toStdString()
                    + " replaced its explanation with the numeric popup");
        slider->mouseUp (mouseEventFor (*slider, centre, {}));
        slider->setValue (previous, juce::sendNotificationSync);
    }
}

void testColdStartProgramAndEditorAreInSync()
{
    YouKnow106AudioProcessor processor;
    expect (processor.getCurrentProgram() == 0,
            "a cold processor does not select INIT");
    expect (! processor.currentProgramIsEdited(),
            "a cold processor's controls do not match INIT");

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "cannot inspect cold-start editor state");
    if (editor == nullptr)
        return;

    auto* patchBox = dynamic_cast<juce::ComboBox*> (
        findDescendantNamed (*editor, "Patch selector"));
    expect (patchBox != nullptr && patchBox->getSelectedId() == 1
                && patchBox->getText() == processor.getProgramName (0),
            "the cold preset selector does not show INIT");

    auto* edited = findDescendantLabelWithText (*editor, "EDITED");
    expect (edited != nullptr && ! edited->isVisible(),
            "the cold editor claims its untouched INIT is edited");

    struct SliderSync { const char* name; const char* parameter; };
    constexpr auto sliders = std::to_array<SliderSync> ({
        { "FREQ", parameters::cutoff },
        { "A", parameters::attack },
        { "Transpose", parameters::transpose },
        { "Unit Character", parameters::calibration },
    });
    for (const auto& expected : sliders)
    {
        auto* slider = dynamic_cast<juce::Slider*> (
            findDescendantNamed (*editor, expected.name));
        expect (slider != nullptr,
                std::string ("cold editor is missing ") + expected.name);
        if (slider != nullptr)
            expect (std::abs (static_cast<float> (slider->getValue())
                             - parameterValue (processor, expected.parameter))
                        < 1.0e-5f,
                    std::string (expected.name)
                        + " is out of sync with the selected INIT program");
    }

    auto* saw = dynamic_cast<juce::Button*> (
        findDescendantNamed (*editor, "SAW"));
    expect (saw != nullptr
                && saw->getToggleState()
                       == (parameterValue (processor, parameters::saw) > 0.5f),
            "the cold waveform switch is out of sync with INIT");
}

void testEditorReloadButtonDiscardsPatchEdits()
{
    YouKnow106AudioProcessor processor;
    processor.setCurrentProgram (3);
    const float storedCutoff = processor.programPatch (3).cutoff;
    const float storedVolume = presets::factoryBank()[2].controls.volume;
    setParameterValue (processor, parameters::volume, 0.42f);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "cannot test reload without an editor");
    if (editor == nullptr)
        return;

    HostChangeRecorder hostChanges;
    processor.addListener (&hostChanges);
    const auto expectOnlyProgramChanged = [&] (const char* action)
    {
        expect (hostChanges.changeCount == 1,
                std::string (action) + " did not notify the host exactly once");
        expect (hostChanges.lastDetails.programChanged,
                std::string (action) + " omitted programChanged");
        expect (! hostChanges.lastDetails.latencyChanged,
                std::string (action) + " falsely reported a latency change");
        expect (! hostChanges.lastDetails.parameterInfoChanged,
                std::string (action) + " falsely requested a parameter rescan");
        expect (! hostChanges.lastDetails.nonParameterStateChanged,
                std::string (action) + " falsely reported non-parameter state");
    };

    setParameterValue (processor, parameters::cutoff, 0.137f);
    expect (processor.currentProgramIsEdited(),
            "the reload regression did not create a patch edit");

    auto* reload = dynamic_cast<juce::Button*> (
        findDescendantNamed (*editor, "Reload patch"));
    expect (reload != nullptr, "the editor has no explicit Reload patch button");
    if (reload != nullptr)
    {
        expect (static_cast<bool> (reload->onClick),
                "the Reload patch button has no action");
        if (reload->onClick)
        {
            hostChanges.clear();
            reload->onClick();
            expectOnlyProgramChanged ("Reload patch");
        }
    }

    expect (! processor.currentProgramIsEdited(),
            "Reload patch did not discard the current edits");
    expect (std::abs (parameterValue (processor, parameters::cutoff) - storedCutoff)
                < 1.0e-6f,
            "Reload patch did not restore the selected program's cutoff");
    expect (std::abs (parameterValue (processor, parameters::volume) - storedVolume)
                < 1.0e-6f,
            "Reload patch did not restore the product program's volume");

    // The tone part of EDITED intentionally follows the hardware's 7-bit patch
    // memory. A sub-step host value can therefore sound/encode identically
    // while still differing from the exact factory float. RELOAD must restore
    // that exact source value even though the label quite correctly stays off.
    const float sameByteCutoff = storedCutoff + 0.001f;
    setParameterValue (processor, parameters::cutoff, sameByteCutoff);
    expect (! processor.currentProgramIsEdited(),
            "a cutoff move inside one hardware byte step marked the patch edited");
    expect (std::abs (parameterValue (processor, parameters::cutoff) - storedCutoff)
                > 1.0e-5f,
            "the reload sub-step fixture did not move the live control");
    if (reload != nullptr && reload->onClick)
    {
        hostChanges.clear();
        reload->onClick();
        expectOnlyProgramChanged ("second Reload patch");
    }
    expect (std::abs (parameterValue (processor, parameters::cutoff) - storedCutoff)
                < 1.0e-6f,
            "Reload patch skipped an unlabelled sub-byte control difference");

    auto* previous = findDescendantButtonWithText (*editor, "<");
    expect (previous != nullptr, "the editor has no previous-program button");
    if (previous != nullptr && previous->onClick)
    {
        hostChanges.clear();
        previous->onClick();
        expect (processor.getCurrentProgram() == 2,
                "the previous-program button did not step the selection");
        expectOnlyProgramChanged ("Previous patch");
    }

    processor.removeListener (&hostChanges);
}

void testEditorRandomizeStrengthsAndReset()
{
    YouKnow106AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "cannot test utility actions without an editor");
    if (editor == nullptr)
        return;

    struct RandomizeAction
    {
        const char* label;
        float maximumMovement;
    };
    constexpr auto actions = std::to_array<RandomizeAction> ({
        { "RND1%",  0.01f },
        { "RND10%", 0.10f },
        { "RND50%", 0.50f },
    });

    expect (! hasDescendantButtonWithTextPrefix (*editor, "RANDOMIZE"),
            "the editor still exposes an obsolete long RANDOMIZE legend");

    for (const auto& action : actions)
    {
        expect (countDescendantButtonsWithText (*editor, action.label) == 1,
                std::string ("the editor does not contain exactly one ")
                    + action.label + " button");
        auto* button = findDescendantButtonWithText (*editor, action.label);
        expect (button != nullptr,
                std::string ("the editor is missing ") + action.label);
        if (button == nullptr || ! button->onClick)
            continue;

        processor.setCurrentProgram (0);
        const int parameterCount = processor.getParameters().size();
        std::vector<float> before;
        before.reserve (static_cast<std::size_t> (parameterCount));
        for (const auto* parameter : processor.getParameters())
            before.push_back (parameter->getValue());

        button->onClick();
        bool movedSomething = false;
        for (int index = 0; index < parameterCount; ++index)
        {
            const float movement = std::abs (
                processor.getParameters()[index]->getValue()
                - before[static_cast<std::size_t> (index)]);
            movedSomething = movedSomething || movement > 1.0e-7f;
            expect (movement <= action.maximumMovement + 1.0e-6f,
                    std::string (action.label)
                        + " moved a parameter farther than its advertised strength");
        }
        expect (movedSomething,
                std::string (action.label) + " did not move any sound control");
    }

    processor.setCurrentProgram (0);
    const int parameterCount = processor.getParameters().size();
    std::vector<float> initValues;
    initValues.reserve (static_cast<std::size_t> (parameterCount));
    for (const auto* parameter : processor.getParameters())
        initValues.push_back (parameter->getValue());

    processor.setCurrentProgram (3);
    for (auto* parameter : processor.getParameters())
        parameter->setValueNotifyingHost (parameter->getValue() < 0.5f ? 0.87f : 0.13f);

    auto* reset = findDescendantButtonWithText (*editor, "RESET");
    expect (reset != nullptr, "the editor is missing RESET");
    expect (reset != nullptr && static_cast<bool> (reset->onClick),
            "RESET has no action");
    if (reset != nullptr && reset->onClick)
        reset->onClick();

    expect (processor.getCurrentProgram() == 0,
            "RESET did not select the complete INIT program");
    expect (! processor.currentProgramIsEdited(),
            "RESET left INIT marked as edited");
    for (int index = 0; index < parameterCount; ++index)
        expect (std::abs (processor.getParameters()[index]->getValue()
                         - initValues[static_cast<std::size_t> (index)]) < 1.0e-6f,
                "RESET did not restore every INIT control");

    if (auto* preset = findDescendantComboBox (*editor))
        expect (preset->getSelectedId() == 1,
                "RESET did not bring the patch display back to INIT");
    else
        expect (false, "RESET test could not find the patch display");
}

void testClickingTheSelectedRadioKeepsItsLampLit()
{
    YouKnow106AudioProcessor processor;
    setParameterValue (processor, parameters::range,
                       static_cast<float> (DcoRange::Eight));
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "cannot test radio buttons without an editor");
    if (editor == nullptr)
        return;

    auto* selected = dynamic_cast<juce::Button*> (
        findDescendantNamed (*editor, "8'"));
    expect (selected != nullptr, "the selected 8-foot range radio was not found");
    if (selected == nullptr)
        return;

    expect (! selected->getClickingTogglesState(),
            "a radio button still self-toggles before its parameter attachment");
    expect (selected->getToggleState(), "the selected range radio is not lit");
    expect (static_cast<bool> (selected->onClick),
            "the selected range radio has no click action");
    if (selected->onClick)
        selected->onClick();

    expect (selected->getToggleState(),
            "clicking the selected range radio extinguished its lamp");
    expect (std::abs (parameterValue (processor, parameters::range)
                     - static_cast<float> (DcoRange::Eight)) < 1.0e-6f,
            "clicking the selected range radio moved its parameter");
}

void testPolyButtonsKeepAValidFirmwareLatch()
{
    YouKnow106AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "cannot test POLY buttons without an editor");
    if (editor == nullptr)
        return;

    auto* poly1 = dynamic_cast<juce::Button*> (
        findDescendantNamed (*editor, "POLY 1"));
    auto* poly2 = dynamic_cast<juce::Button*> (
        findDescendantNamed (*editor, "POLY 2"));
    expect (poly1 != nullptr && poly2 != nullptr,
            "the editor's two POLY buttons were not found");
    if (poly1 == nullptr || poly2 == nullptr)
        return;

    const auto previousModifiers = juce::ModifierKeys::currentModifiers;
    juce::ModifierKeys::currentModifiers = {};

    expect (! poly1->getClickingTogglesState()
                && ! poly2->getClickingTogglesState(),
            "POLY contacts are still modelled as self-toggling latches");
    expect (poly1->getToggleState() && ! poly2->getToggleState(),
            "the POLY latch did not start in Poly 1");

    // Re-pressing the selected contact cannot extinguish its lamp.
    poly1->onClick();
    expect (parameterValue (processor, parameters::poly1) > 0.5f
                && parameterValue (processor, parameters::poly2) < 0.5f,
            "re-pressing Poly 1 created the impossible both-off state");

    // An ordinary press of the other physical contact selects that single
    // assign mode. It is not a shortcut for holding two contacts at once.
    poly2->onClick();
    expect (parameterValue (processor, parameters::poly1) < 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "an ordinary Poly 2 press did not select Poly 2 alone");

    // Shift-click is the editor's explicit way to hold both momentary panel
    // contacts simultaneously, which is the owner's-manual Solo Unison
    // gesture. The modifier is global JUCE input state, so restore it below.
    juce::ModifierKeys::currentModifiers =
        juce::ModifierKeys (juce::ModifierKeys::shiftModifier);
    poly1->onClick();
    expect (parameterValue (processor, parameters::poly1) > 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "Shift-clicking POLY did not select Solo Unison");

    const auto reassertSequence = processor.getKeyModeReassertSequenceForTest();
    poly1->onClick();
    expect (processor.getKeyModeReassertSequenceForTest() == reassertSequence + 1,
            "repeating the simultaneous POLY press did not reassert Unison");

    // From Unison, pressing one contact alone selects that single mode.
    juce::ModifierKeys::currentModifiers = {};
    poly2->onClick();
    expect (parameterValue (processor, parameters::poly1) < 0.5f
                && parameterValue (processor, parameters::poly2) > 0.5f,
            "pressing Poly 2 from Unison did not select Poly 2 alone");
    poly2->onClick();
    expect (parameterValue (processor, parameters::poly2) > 0.5f,
            "re-pressing Poly 2 extinguished the last assign lamp");

    juce::ModifierKeys::currentModifiers = previousModifiers;
}

void checkUtilityKnobLayout (juce::AudioProcessorEditor& editor,
                             bool atSupportedMinimum)
{
    struct UtilityExpectation
    {
        const char* sliderName;
        const char* caption;
    };
    constexpr auto expected = std::to_array<UtilityExpectation> ({
        { "Transpose",      "TRANSPOSE" },
        { "Master tune",    "TUNE" },
        { "Velocity",       "VELOCITY" },
        { "Unit Character", "UNIT CHARACTER" },
        { "Chorus noise",   "CHORUS NOISE" },
        { "Polyphony",      "VOICES" },
    });

    std::array<juce::Rectangle<int>, expected.size()> occupiedAreas {};
    const auto editorBounds = editor.getLocalBounds();

    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        const auto& item = expected[index];
        auto* slider = dynamic_cast<juce::Slider*> (
            findDescendantNamed (editor, item.sliderName));
        auto* caption = findDescendantLabelWithText (editor, item.caption);
        const auto context = std::string (atSupportedMinimum ? "minimum-size "
                                                             : "default-size ")
                           + item.sliderName;

        expect (slider != nullptr, context + " utility knob is missing");
        expect (caption != nullptr, context + " utility caption is missing");
        if (slider == nullptr || caption == nullptr)
            continue;

        expect (isRotaryStyle (slider->getSliderStyle()),
                context + " still uses a linear slider");

        const auto sliderArea = editor.getLocalArea (
            slider, slider->getLocalBounds());
        const auto captionArea = editor.getLocalArea (
            caption, caption->getLocalBounds());
        expect (! sliderArea.isEmpty(), context + " knob has empty bounds");
        expect (! captionArea.isEmpty(), context + " caption has empty bounds");
        expect (editorBounds.contains (sliderArea),
                context + " knob extends outside the editor");
        expect (editorBounds.contains (captionArea),
                context + " caption extends outside the editor");
        expect (! sliderArea.intersects (captionArea),
                context + " knob overlaps its caption");

        occupiedAreas[index] = sliderArea.getUnion (captionArea);
        for (std::size_t previous = 0; previous < index; ++previous)
            expect (! occupiedAreas[index].intersects (occupiedAreas[previous]),
                    context + " cell overlaps another utility control");

        const float minimumFontHeight = atSupportedMinimum ? 9.0f : 11.0f;
        expect (caption->getFont().getHeight() >= minimumFontHeight - 0.05f,
                context + " caption is below the readable font floor");
        expect (labelTextFitsAtItsDeclaredSize (*caption),
                context + " caption relies on ellipsis or compressed lettering");

        // Popup-only knobs have no persistent value label to truncate. When a
        // text box is present, exercise the longest signed Master Tune value
        // and prove the real font fits the real box instead of trusting a
        // screenshot or JUCE's silent fitted-text compression.
        if (std::strcmp (item.sliderName, "Master tune") == 0
            && slider->getTextBoxPosition() != juce::Slider::NoTextBox)
        {
            auto* valueLabel = findSliderTextBox (*slider);
            expect (valueLabel != nullptr,
                    context + " declares a value box but did not create one");
            if (valueLabel != nullptr)
            {
                const double previousValue = slider->getValue();
                slider->setValue (-50.0, juce::dontSendNotification);
                const auto expectedText = slider->getTextFromValue (-50.0);
                expect (valueLabel->getText() == expectedText,
                        context + " Master tune value display is incomplete");
                expect (realTextWidth (valueLabel->getFont(), expectedText)
                            <= static_cast<float> (valueLabel->getWidth()),
                        context + " Master tune value is wider than its text box");
                expect (valueLabel->getHeight()
                            >= juce::roundToInt (valueLabel->getFont().getHeight()),
                        context + " Master tune value is vertically clipped");
                slider->setValue (previousValue, juce::dontSendNotification);
            }
        }
    }
}

template <std::size_t Size>
double namedGroupFootprint (juce::AudioProcessorEditor& editor,
                            const std::array<const char*, Size>& componentNames,
                            const char* groupName)
{
    juce::Rectangle<int> footprint;
    bool foundAny = false;
    for (const auto* name : componentNames)
    {
        auto* component = findDescendantNamed (editor, name);
        expect (component != nullptr,
                std::string (groupName) + " is missing " + name);
        if (component == nullptr)
            continue;

        const auto area = editor.getLocalArea (component,
                                               component->getLocalBounds());
        expect (! area.isEmpty(),
                std::string (groupName) + " has empty geometry for " + name);
        if (area.isEmpty())
            continue;

        footprint = foundAny ? footprint.getUnion (area) : area;
        foundAny = true;
    }

    expect (foundAny, std::string (groupName) + " has no measurable footprint");
    return foundAny ? static_cast<double> (footprint.getWidth())
                            * static_cast<double> (footprint.getHeight())
                    : 0.0;
}

void checkSynthesisSectionsDominateUtilities (
    juce::AudioProcessorEditor& editor)
{
    // Measure the allocations, not individual knobs. A sparse utility card can
    // otherwise look artificially tiny while still taking half the console.
    // Character Lab and Operations are the two non-synthesis cards competing
    // for control-panel space. The masthead and live performance deck serve
    // navigation/playing rather than utility configuration, so folding those
    // into this comparison would measure a different design decision.
    constexpr auto characterLab = std::to_array<const char*> ({
        "Unit Character", "Unit Character label",
        "Chorus noise", "Chorus noise label", "HQ"
    });
    constexpr auto operations = std::to_array<const char*> ({
        "PANIC", "RND1%", "RND10%", "RND50%", "RESET"
    });
    const double utilityAndSystemArea =
        namedGroupFootprint (editor, characterLab, "Character Lab")
        + namedGroupFootprint (editor, operations, "Operations");

    const double scale = std::min (
        static_cast<double> (editor.getWidth())
            / static_cast<double> (panel::panelWidth()),
        static_cast<double> (editor.getHeight())
            / static_cast<double> (panel::editorHeight));
    double synthesisArea = 0.0;
    for (const auto& section : panel::sections())
        synthesisArea += static_cast<double> (section.width)
                       * static_cast<double> (section.height) * scale * scale;

    expect (utilityAndSystemArea > 0.0,
            "the utility/system footprint could not be measured");
    // The intended folded layout is about 4.45:1. Three-to-one is visibly
    // synthesis-first while retaining generous room for future label polish.
    constexpr double minimumDominance = 3.0;
    expect (synthesisArea > utilityAndSystemArea * minimumDominance,
            "synthesis sections no longer dominate the console area (ratio "
                + std::to_string (utilityAndSystemArea > 0.0
                                      ? synthesisArea / utilityAndSystemArea
                                      : 0.0)
                + ")");
}

void testEditorBuildsAndRenders()
{
    YouKnow106AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "the processor produced no editor");
    if (editor == nullptr)
        return;

    expect (findDescendantNamed (*editor, "Unit Character") != nullptr,
            "the editor still presents the compatibility profile as Calibration");

    auto* playableKeyboard = dynamic_cast<juce::MidiKeyboardComponent*> (
        findDescendantNamed (*editor, "Playable keyboard"));
    auto* performanceLever = dynamic_cast<YouKnow106PerformanceLever*> (
        findDescendantNamed (*editor, "Pitch and modulation lever"));
    auto* contextHelp = dynamic_cast<YouKnow106ContextHelp*> (
        findDescendantNamed (*editor, "Context help"));
    auto* transpose = dynamic_cast<juce::Slider*> (
        findDescendantNamed (*editor, "Transpose"));
    auto* character = dynamic_cast<juce::Slider*> (
        findDescendantNamed (*editor, "Unit Character"));
    expect (playableKeyboard != nullptr,
            "the editor has no playable keyboard to range-check");
    expect (performanceLever != nullptr,
            "the editor has no pitch/mod performance control");
    expect (contextHelp != nullptr,
            "the editor has no fixed help display");
    if (playableKeyboard != nullptr)
    {
        expect (playableKeyboard->getRangeStart()
                    == panel::keyboardLowestMidiNote
                    && playableKeyboard->getRangeEnd()
                    == panel::keyboardHighestMidiNote,
                "the on-screen keyboard is not the JUNO's 61-key C2-C7 span");
        expect (playableKeyboard->getLowestVisibleKey()
                    == panel::keyboardLowestMidiNote,
                "the on-screen keyboard does not begin on the physical low C");
    }

    // The default size follows the panel description, so widening a section to
    // fit a legend or adding a row moves it. Both dimensions are therefore
    // derived here rather than written down: the height was still a literal and
    // failed the moment the panel legitimately grew a patch bar, which is the
    // exact trap the width had already been rescued from.
    const int expectedWidth = juce::roundToInt (panel::panelWidth());
    const int expectedHeight = juce::roundToInt (panel::editorHeight);
    expect (editor->getWidth() == expectedWidth
                && editor->getHeight() == expectedHeight,
            "the editor did not open at its default size");
    const float defaultAspect = static_cast<float> (expectedWidth)
                              / static_cast<float> (expectedHeight);
    expect (defaultAspect >= 1.25f && defaultAspect <= 1.65f,
            "the folded console did not become materially more square");
    expect (editor->isOpaque(), "the editor does not advertise an opaque surface");
    checkUtilityKnobLayout (*editor, false);
    checkSynthesisSectionsDominateUtilities (*editor);

    // Exercise the layout at both extremes as well as at its default size. Read
    // the supported minimum from the constrainer so raising the readability
    // floor cannot leave this test checking an obsolete, unsupported window.
    auto supportedMinimum = juce::Point<int> { 900, 380 };
    if (auto* constrainer = editor->getConstrainer())
        supportedMinimum = { constrainer->getMinimumWidth(),
                             constrainer->getMinimumHeight() };
    else
        expect (false, "the resizable editor has no bounds constrainer");

    for (auto size : { supportedMinimum,
                       juce::Point<int> { panel::maximumEditorWidth,
                                          panel::maximumEditorHeight } })
    {
        editor->setSize (size.x, size.y);
        editor->resized();
        if (size == supportedMinimum)
            checkUtilityKnobLayout (*editor, true);
        checkSynthesisSectionsDominateUtilities (*editor);
        if (playableKeyboard != nullptr)
        {
            const auto keyboardArea = editor->getLocalArea (
                playableKeyboard, playableKeyboard->getLocalBounds());
            const float fittedKeyWidth =
                static_cast<float> (playableKeyboard->getWidth())
                / static_cast<float> (panel::keyboardWhiteKeyCount);
            expect (std::abs (playableKeyboard->getKeyWidth() - fittedKeyWidth)
                        < 1.0e-4f,
                    "the 61-key keyboard did not stay fitted after resize");
            expect (std::abs (
                        playableKeyboard->getRectangleForKey (
                            panel::keyboardLowestMidiNote).getX()) < 1.0f
                        && std::abs (
                            playableKeyboard->getRectangleForKey (
                                panel::keyboardHighestMidiNote).getRight()
                            - static_cast<float> (playableKeyboard->getWidth())) < 1.0f,
                    "the physical C2-C7 keybed left a blank or clipped edge");
            expect (fittedKeyWidth >= 24.0f,
                    "the squarer layout made the 61-key keybed impractically narrow");

            if (performanceLever != nullptr)
            {
                const auto leverArea = editor->getLocalArea (
                    performanceLever, performanceLever->getLocalBounds());
                expect (! leverArea.isEmpty() && editor->getLocalBounds().contains (leverArea),
                        "the pitch/mod lever escaped the editor");
                expect (leverArea.getBottom() <= keyboardArea.getY()
                            && ! leverArea.intersects (keyboardArea),
                        "the pitch/mod lever overlaps the keybed");
            }
            if (contextHelp != nullptr)
            {
                const auto helpArea = editor->getLocalArea (
                    contextHelp, contextHelp->getLocalBounds());
                expect (! helpArea.isEmpty() && editor->getLocalBounds().contains (helpArea),
                        "the fixed help display escaped the editor");
                expect (helpArea.getY() >= keyboardArea.getBottom()
                            && ! helpArea.intersects (keyboardArea),
                        "the fixed help display is not below the keys");
                expect (helpArea.getWidth()
                            >= editor->getWidth()
                                 - juce::roundToInt (30.0f
                                                    * static_cast<float> (size.x)
                                                    / panel::panelWidth()),
                        "the help display is no longer a full-width stable area");
            }
            if (transpose != nullptr)
            {
                const auto area = editor->getLocalArea (
                    transpose, transpose->getLocalBounds());
                expect (area.getY() < keyboardArea.getY()
                            && area.getBottom() <= keyboardArea.getY(),
                        "keyboard setup controls were not moved into the lower deck");
            }
            if (character != nullptr)
            {
                const auto area = editor->getLocalArea (
                    character, character->getLocalBounds());
                expect (area.getBottom() < keyboardArea.getY(),
                        "Character Lab overlaps the performance keybed");
            }
        }
        expect (snapshotHasDetail (renderEditorSnapshot (*editor)),
                "the editor did not render at "
                    + juce::String (size.x).toStdString() + " wide");
    }

    // Back to the default size, which is also the size the committed
    // documentation image is captured at below.
    editor->setSize (expectedWidth, expectedHeight);
    editor->resized();
    const auto snapshot = renderEditorSnapshot (*editor);
    expect (snapshotHasDetail (snapshot),
            "the editor rendered as a flat surface at its default size");

    // Committed documentation image, regenerated by the nightly build.
    const auto snapshotPath =
        juce::SystemStats::getEnvironmentVariable ("YOUKNOW106_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty() && snapshot.isValid())
    {
        // A clean checkout has no screenshots directory: git does not track an
        // empty one, and the first nightly run is what creates the image.
        const juce::File snapshotFile { snapshotPath };
        snapshotFile.getParentDirectory().createDirectory();
        juce::FileOutputStream output { snapshotFile };
        juce::PNGImageFormat png;
        const bool prepared = output.openedOk() && output.setPosition (0)
                           && output.truncate();
        const bool wrote = prepared && png.writeImageToStream (snapshot, output);
        output.flush();
        expect (wrote, "could not write the requested editor snapshot");
    }

    editor.reset();
    processor.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testParameterContract();
    testParameterTextRoundTrips();
    testProcessingProducesSound();
    testShortNoteInsideOneBlockIsHeard();
    testUiKeyboardPressAndReleaseIsHeard();
    testDeferredQualitySwitchIsNotAutomatable();
    testAllNotesOffReleasesAndAllSoundOffCuts();
    testTransportOfControllers();
    testUiPerformanceLeverMatchesMidiAndCoalesces();
    testPerformanceLeverSpringsAndEditorCloseNeutralisesIt();
    testPanicSilencesEverything();
    testStateRoundTripAndMigration();
    testLegacySplitModeMigration();
    testEveryStoredPatchFieldRecallsWithoutMovingPerformanceControls();
    testRandomizerPreservesQualityAndLevel();
    testBusLayoutsAndTail();
    testProgramChangeRecallsEveryHardwareSlot();
    testProgramChangeAffectsFollowingNoteWithoutTheMessageThread();
    testZeroSampleBlockStillHandlesProgramAndSysEx();
    testSysExPatchRoundTripsThroughTheParameters();
    testOrderedSysExAffectsAudioWithoutTheMessageThread();
    testReflectionAckCannotRetireShadowAgainstAStaleSnapshot();
    testSingleParameterSysExDoesNotDisturbAnythingElse();
    testAWholeBankTransferIsNotDropped();
    testOverflowedMidiReflectionCoalescesWithoutDroppingAudioEvents();
    testSingleParameterOverflowResyncPreservesLaterUnrelatedEdit();
    testRequestedDumpLeavesThroughTheMidiOutput();
    testSelectedProgramSurvivesAStateRoundTrip();
    testConcurrentProgramRecallSavesACoherentState();
    testForeignSysExLeavesThePatchAlone();
    testLegacyAutomationIdsStillReachTheSwitches();
    testLaterEditorPairEditSupersedesPendingLegacyAutomation();
    testRepeatedPolyPressSupersedesPendingLegacyAutomation();
    testLegacyAutomationIsHeardWithoutTheMessageThread();
    testForwardedLegacyChorusDoesNotOverrideALaterPresetRecall();
    testPendingLegacyChorusDoesNotOverrideALaterPresetRecall();
    testLegacyForwarderCanonicalisesBothPolyLampsOff();
    testRestoringASessionDoesNotOverwriteItsOwnModeSwitches();
    testPendingLegacyModesDoNotOverrideALaterStateRestore();
    testLegacyAutomationBetweenRestoreAndFirstBlockIsHonoured();
    testEditedFlagFollowsTheCompleteProgram();
    testColdStartProgramAndEditorAreInSync();
    testFactoryProgramsLoad();
    testEveryProductProgramRestoresEveryParameter();
    testEveryPanelLegendFitsInTheRealFont();
    testEveryInteractiveEditorControlExplainsItself();
    testPersistentContextHelpAndValueBubbles();
    testEditorReloadButtonDiscardsPatchEdits();
    testEditorRandomizeStrengthsAndReset();
    testClickingTheSelectedRadioKeepsItsLampLit();
    testPolyButtonsKeepAValidFirmwareLatch();
    testEditorBuildsAndRenders();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " YouKnow106 plug-in check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All YouKnow106 plug-in checks passed.\n";
    return EXIT_SUCCESS;
}
