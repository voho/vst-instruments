#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
constexpr auto accent = 0xffffb34d;
constexpr auto accentHot = 0xffff5c78;
constexpr auto accentCool = 0xff64d9ff;
constexpr auto panel = 0xff11161e;
constexpr auto panelRaised = 0xff171e28;
constexpr auto panelEdge = 0xff303947;
constexpr auto textBright = 0xfff4f4ef;
constexpr auto textDim = 0xff8996a4;

juce::Colour colour (juce::uint32 argb)
{
    return juce::Colour (argb);
}

template <typename Text>
juce::String toJuceString (const Text& text)
{
    if constexpr (std::is_pointer_v<std::decay_t<Text>>)
        return text != nullptr ? juce::String::fromUTF8 (text) : juce::String {};
    else
    {
        const std::string_view view { text };
        return juce::String::fromUTF8 (view.data(), static_cast<int> (view.size()));
    }
}

constexpr bool isValidInstrument (drumalor::Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument) < drumalor::instrumentCount;
}

constexpr std::size_t instrumentIndex (drumalor::Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument);
}

void styleHeaderLabel (juce::Label& label, float size, juce::Colour labelColour)
{
    label.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, labelColour);
    label.setInterceptsMouseClicks (false, false);
}

void drawPanel (juce::Graphics& graphics, juce::Rectangle<int> bounds)
{
    const auto area = bounds.toFloat();
    graphics.setColour (colour (panel));
    graphics.fillRoundedRectangle (area, 11.0f);
    graphics.setColour (colour (panelEdge));
    graphics.drawRoundedRectangle (area.reduced (0.5f), 11.0f, 1.0f);
}

class DrumalorPadAccessibilityHandler final : public juce::AccessibilityHandler
{
public:
    explicit DrumalorPadAccessibilityHandler (DrumalorPad& padToWrap)
        : AccessibilityHandler (padToWrap, juce::AccessibilityRole::radioButton,
                                makeActions (padToWrap)),
          pad (padToWrap)
    {
    }

    juce::AccessibleState getCurrentState() const override
    {
        auto state = AccessibilityHandler::getCurrentState().withCheckable();
        return pad.getToggleState() ? state.withChecked() : state;
    }

private:
    static juce::AccessibilityActions makeActions (DrumalorPad& target)
    {
        const auto selectAndTrigger = [&target]
        {
            if (target.getToggleState())
                target.triggerClick();
            else
                target.setToggleState (true, juce::sendNotification);
        };

        return juce::AccessibilityActions()
            .addAction (juce::AccessibilityActionType::press, selectAndTrigger)
            .addAction (juce::AccessibilityActionType::toggle, selectAndTrigger);
    }

    DrumalorPad& pad;
};
} // namespace

DrumalorLookAndFeel::DrumalorLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colour (textBright));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, colour (accent));
    setColour (juce::Slider::rotarySliderOutlineColourId, colour (panelEdge));
    setColour (juce::TextButton::textColourOffId, colour (textDim));
    setColour (juce::TextButton::textColourOnId, colour (textBright));
}

void DrumalorLookAndFeel::drawRotarySlider (juce::Graphics& graphics,
                                            int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
{
    const auto diameter = static_cast<float> (juce::jmin (width, height)) - 15.0f;
    const auto radius = diameter * 0.5f;
    const auto centre = juce::Point<float> (
        static_cast<float> (x) + static_cast<float> (width) * 0.5f,
        static_cast<float> (y) + static_cast<float> (height) * 0.5f - 1.0f);
    const auto bounds = juce::Rectangle<float> (diameter, diameter).withCentre (centre);
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    graphics.setColour (colour (0x2affb34d));
    graphics.fillEllipse (bounds.expanded (3.0f));

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    graphics.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    graphics.strokePath (track, juce::PathStrokeType (4.0f,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f,
                            rotaryStartAngle, angle, true);
    juce::ColourGradient arcGradient (colour (accentHot), bounds.getBottomLeft(),
                                      colour (accent), bounds.getTopRight(), false);
    graphics.setGradientFill (arcGradient);
    graphics.strokePath (valueArc, juce::PathStrokeType (4.2f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

    graphics.setColour (colour (0xff090d12));
    graphics.fillEllipse (bounds.reduced (9.0f));
    graphics.setColour (colour (0xff3b4654));
    graphics.drawEllipse (bounds.reduced (9.0f), 1.2f);

    juce::Path pointer;
    const auto pointerLength = radius * 0.43f;
    pointer.addRoundedRectangle (-1.5f, -pointerLength, 3.0f,
                                 pointerLength * 0.58f, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                .translated (centre.x, centre.y));
    graphics.setColour (colour (textBright));
    graphics.fillPath (pointer);

    if (slider.hasKeyboardFocus (true))
    {
        graphics.setColour (colour (accentCool));
        graphics.drawEllipse (bounds.expanded (2.0f), 1.6f);
    }
}

void DrumalorLookAndFeel::drawButtonBackground (juce::Graphics& graphics,
                                                juce::Button& button,
                                                const juce::Colour&,
                                                bool isHighlighted,
                                                bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    auto fill = colour (0xff18151b);
    if (isHighlighted)
        fill = fill.brighter (0.08f);
    if (isDown)
        fill = fill.darker (0.12f);

    graphics.setColour (fill);
    graphics.fillRoundedRectangle (bounds, 6.0f);
    graphics.setColour (colour (accentHot).withAlpha (isHighlighted ? 0.9f : 0.55f));
    graphics.drawRoundedRectangle (bounds, 6.0f, 1.2f);
    if (button.hasKeyboardFocus (true))
    {
        graphics.setColour (colour (accentCool));
        graphics.drawRoundedRectangle (bounds.reduced (2.5f), 4.5f, 1.5f);
    }
}

juce::Font DrumalorLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (
                                          13.0f, static_cast<float> (buttonHeight) * 0.38f),
                                          juce::Font::bold));
}

DrumalorPad::DrumalorPad (drumalor::Instrument instrument,
                          juce::String displayName,
                          int midiNote)
    : Button (displayName), drum (instrument), nameText (std::move (displayName)),
      noteText ("GM " + juce::String (midiNote))
{
    setName (nameText + " pad");
    setTitle (nameText + " drum pad");
    setDescription ("Select and trigger " + nameText
                    + " on MIDI note " + juce::String (midiNote));
    setWantsKeyboardFocus (true);
    setClickingTogglesState (true);
    setRadioGroupId (1, juce::dontSendNotification);
}

void DrumalorPad::setSelected (bool shouldBeSelected)
{
    if (selected == shouldBeSelected)
        return;
    selected = shouldBeSelected;
    setToggleState (selected, juce::dontSendNotification);
    repaint();
}

void DrumalorPad::triggerFlash()
{
    flashLevel = 1.0f;
    repaint();
}

void DrumalorPad::advanceFlash()
{
    if (flashLevel <= 0.001f)
        return;

    flashLevel *= 0.76f;
    if (flashLevel < 0.015f)
        flashLevel = 0.0f;
    repaint();
}

std::unique_ptr<juce::AccessibilityHandler>
DrumalorPad::createAccessibilityHandler()
{
    return std::make_unique<DrumalorPadAccessibilityHandler> (*this);
}

void DrumalorPad::paintButton (juce::Graphics& graphics,
                               bool isMouseOverButton,
                               bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    auto top = selected ? colour (0xff2b2420) : colour (panelRaised);
    auto bottom = selected ? colour (0xff17171d) : colour (0xff10151c);

    if (isMouseOverButton)
    {
        top = top.brighter (0.08f);
        bottom = bottom.brighter (0.05f);
    }
    if (isButtonDown)
    {
        top = top.darker (0.12f);
        bottom = bottom.darker (0.12f);
        bounds = bounds.reduced (1.5f);
    }

    juce::ColourGradient fill (top, bounds.getTopLeft(), bottom,
                               bounds.getBottomLeft(), false);
    graphics.setGradientFill (fill);
    graphics.fillRoundedRectangle (bounds, 8.0f);

    if (flashLevel > 0.0f)
    {
        graphics.setColour (colour (accent).withAlpha (0.10f + 0.50f * flashLevel));
        graphics.fillRoundedRectangle (bounds.reduced (1.0f), 7.5f);
    }

    graphics.setColour (selected ? colour (accent) : colour (panelEdge));
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f,
                                   selected ? 1.8f : 1.0f);
    if (hasKeyboardFocus (true))
    {
        graphics.setColour (colour (accentCool));
        graphics.drawRoundedRectangle (bounds.reduced (2.5f), 6.5f, 1.5f);
    }

    const auto strip = bounds.withHeight (4.0f).reduced (8.0f, 0.0f);
    graphics.setColour ((selected ? colour (accent) : colour (accentHot))
                            .withAlpha (selected ? 0.95f : 0.36f + 0.35f * flashLevel));
    graphics.fillRoundedRectangle (strip, 2.0f);

    auto textArea = getLocalBounds().reduced (9, 9);
    textArea.removeFromTop (4);
    auto noteArea = textArea.removeFromBottom (20);
    graphics.setColour (colour (textBright));
    graphics.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    graphics.drawFittedText (nameText.toUpperCase(), textArea,
                             juce::Justification::centred, 2, 0.82f);

    graphics.setColour (selected ? colour (accent) : colour (textDim));
    graphics.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    graphics.drawText (noteText, noteArea, juce::Justification::centred);
}

DrumalorKnob::DrumalorKnob (juce::String name, ValueStyle style)
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, colour (textDim));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 21);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);

    if (style == ValueStyle::Percent)
    {
        slider.setNumDecimalPlacesToDisplay (0);
        slider.textFromValueFunction = [] (double value)
        {
            return juce::String (juce::roundToInt (value * 100.0)) + "%";
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.retainCharacters ("0123456789.-").getDoubleValue() / 100.0;
        };
        slider.setDoubleClickReturnValue (true, 0.5);
    }
    else if (style == ValueStyle::Semitones)
    {
        slider.setNumDecimalPlacesToDisplay (1);
        slider.textFromValueFunction = [] (double value)
        {
            return juce::String (value, 1) + " st";
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.retainCharacters ("0123456789.-").getDoubleValue();
        };
        slider.setDoubleClickReturnValue (true, 0.0);
    }
    else
    {
        slider.setNumDecimalPlacesToDisplay (1);
        slider.textFromValueFunction = [] (double value)
        {
            return juce::String (value, 1) + " dB";
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.retainCharacters ("0123456789.-").getDoubleValue();
        };
        slider.setDoubleClickReturnValue (true, -6.0);
    }

    addAndMakeVisible (slider);
    setLabelText (name, "Adjust " + name.toLowerCase());
}

void DrumalorKnob::setLabelText (const juce::String& text,
                                 const juce::String& description)
{
    label.setText (text, juce::dontSendNotification);
    slider.setName (text);
    slider.setTitle (text);
    slider.setDescription (description);
}

void DrumalorKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (19));
    slider.setBounds (area);
}

DrumalorStatusDisplay::DrumalorStatusDisplay()
{
    setTitle ("Drumalor engine offline");
    setDescription ("Current Drumalor audio engine status");
    setAccessible (true);
}

std::unique_ptr<juce::AccessibilityHandler>
DrumalorStatusDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::staticText);
}

void DrumalorStatusDisplay::setStatus (int activeVoices, bool ready, double sampleRate)
{
    if (voices == activeVoices && isReady == ready && std::abs (rate - sampleRate) < 1.0)
        return;

    voices = activeVoices;
    isReady = ready;
    rate = sampleRate;
    const auto statusText = ready
        ? juce::String (juce::jmax (0, voices)) + " active drum tails at "
            + juce::String (juce::roundToInt (rate / 1000.0)) + " kilohertz"
        : juce::String { "No active drum tails; audio engine is offline" };
    setTitle (statusText);
    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::titleChanged);
    repaint();
}

void DrumalorStatusDisplay::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    graphics.setColour (colour (panel));
    graphics.fillRoundedRectangle (bounds, 6.0f);
    graphics.setColour (colour (panelEdge));
    graphics.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    const auto light = juce::Rectangle<float> (bounds.getX() + 11.0f,
                                               bounds.getCentreY() - 4.0f, 8.0f, 8.0f);
    graphics.setColour (isReady ? colour (accent) : colour (0xff59616a));
    graphics.fillEllipse (light);
    if (isReady)
    {
        graphics.setColour (colour (0x36ffb34d));
        graphics.fillEllipse (light.expanded (5.0f));
    }

    graphics.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    graphics.setColour (colour (textBright));
    graphics.drawText (juce::String (juce::jmax (0, voices)).paddedLeft ('0', 2) + " TAILS",
                       28, 0, 78, getHeight(), juce::Justification::centredLeft);

    graphics.setColour (colour (textDim));
    const auto rateText = rate > 0.0
        ? juce::String (juce::roundToInt (rate / 1000.0)) + " kHz"
        : juce::String { "OFFLINE" };
    graphics.drawText (rateText, 105, 0, getWidth() - 114, getHeight(),
                       juce::Justification::centredRight);
}

DrumalorAudioProcessorEditor::DrumalorAudioProcessorEditor (DrumalorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    logoLabel.setText ("DRUMALOR", juce::dontSendNotification);
    styleHeaderLabel (logoLabel, 25.0f, colour (textBright));
    logoLabel.setAccessible (true);
    logoLabel.setTitle ("Drumalor drum synthesizer");
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("13-VOICE SYNTHETIC DRUM MACHINE  ·  NO SAMPLES",
                          juce::dontSendNotification);
    styleHeaderLabel (editionLabel, 10.5f, colour (textDim));
    addAndMakeVisible (editionLabel);

    addAndMakeVisible (statusDisplay);

    panicButton.setColour (juce::TextButton::textColourOffId, colour (0xffffaab7));
    panicButton.setName ("Panic — stop all drum tails");
    panicButton.setTitle ("Panic");
    panicButton.setDescription ("Immediately stop every sounding drum voice");
    panicButton.onClick = [this]
    {
        audioProcessor.requestPanic();
    };
    addAndMakeVisible (panicButton);

    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        pads[index] = std::make_unique<DrumalorPad> (
            instrument,
            toJuceString (drumalor::getInstrumentDisplayName (instrument)),
            drumalor::getStandardMidiNote (instrument));
        pads[index]->onClick = [this, instrument]
        {
            // JUCE notifies the old radio button when a new one turns it off.
            // Only the button that is now selected should audition a voice.
            if (! pads[instrumentIndex (instrument)]->getToggleState())
                return;
            selectInstrument (instrument);
            pads[instrumentIndex (instrument)]->triggerFlash();
            audioProcessor.triggerFromUi (instrument);
        };
        observedTriggerCounters[index] = audioProcessor.getTriggerCounter (instrument);
        addAndMakeVisible (*pads[index]);
    }

    selectedInstrumentLabel.setFont (
        juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    selectedInstrumentLabel.setColour (juce::Label::textColourId, colour (textBright));
    selectedInstrumentLabel.setJustificationType (juce::Justification::centredLeft);
    selectedInstrumentLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (selectedInstrumentLabel);

    for (auto* knob : { &characterAKnob, &characterBKnob, &pitchKnob,
                        &decayKnob, &outputKnob })
        addAndMakeVisible (*knob);

    outputAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters, drumalor::parameters::output, outputKnob.slider);
    outputKnob.slider.setTextValueSuffix (" dB");

    selectInstrument (selectedInstrument);

    setResizable (true, true);
    setResizeLimits (900, 640, 1600, 1000);
    setSize (1180, 780);
    startTimerHz (30);
}

DrumalorAudioProcessorEditor::~DrumalorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DrumalorAudioProcessorEditor::selectInstrument (drumalor::Instrument instrument)
{
    if (! isValidInstrument (instrument))
        return;

    const auto needsAttachmentRefresh = instrument != selectedInstrument
                                     || characterAAttachment == nullptr;
    selectedInstrument = instrument;

    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        pads[index]->setSelected (index == instrumentIndex (selectedInstrument));

    const auto name = toJuceString (
        drumalor::getInstrumentDisplayName (selectedInstrument));
    const auto characterA = toJuceString (
        drumalor::getCharacterALabel (selectedInstrument));
    const auto characterB = toJuceString (
        drumalor::getCharacterBLabel (selectedInstrument));
    const auto midiNote = drumalor::getStandardMidiNote (selectedInstrument);

    selectedInstrumentLabel.setText (
        "EDITING  " + name.toUpperCase() + "   ·   GM " + juce::String (midiNote),
        juce::dontSendNotification);
    characterAKnob.setLabelText (characterA.toUpperCase(),
                                 "Adjust " + name + " " + characterA.toLowerCase());
    characterBKnob.setLabelText (characterB.toUpperCase(),
                                 "Adjust " + name + " " + characterB.toLowerCase());
    pitchKnob.setLabelText ("PITCH", "Transpose " + name + " by semitones");
    decayKnob.setLabelText ("DECAY", "Adjust " + name + " decay time");

    if (needsAttachmentRefresh)
        rebuildSelectedAttachments();
}

void DrumalorAudioProcessorEditor::rebuildSelectedAttachments()
{
    // Detach every slider before any new attachment pushes an initial value.
    // Otherwise an attachment for the previously selected drum can observe a
    // slider update and write the new drum's value back into the old parameter.
    characterAAttachment.reset();
    characterBAttachment.reset();
    pitchAttachment.reset();
    decayAttachment.reset();

    characterAAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::characterA),
        characterAKnob.slider);
    characterBAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::characterB),
        characterBKnob.slider);
    pitchAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::pitch),
        pitchKnob.slider);
    decayAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::decay),
        decayKnob.slider);

    // JUCE attachments replace text conversion callbacks, but retain suffixes.
    characterAKnob.slider.setTextValueSuffix ("%");
    characterBKnob.slider.setTextValueSuffix ("%");
    pitchKnob.slider.setTextValueSuffix (" st");
    decayKnob.slider.setTextValueSuffix ("%");
}

void DrumalorAudioProcessorEditor::timerCallback()
{
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto counter = audioProcessor.getTriggerCounter (instrument);
        if (counter != observedTriggerCounters[index])
        {
            observedTriggerCounters[index] = counter;
            pads[index]->triggerFlash();
        }
        pads[index]->advanceFlash();
    }

    statusDisplay.setStatus (audioProcessor.getActiveVoiceCount(), audioProcessor.isEngineReady(),
                             audioProcessor.getCurrentSampleRateForDisplay());
}

DrumalorAudioProcessorEditor::LayoutAreas
DrumalorAudioProcessorEditor::calculateLayout() const
{
    auto content = getLocalBounds().reduced (18);
    LayoutAreas layout;
    layout.header = content.removeFromTop (52);
    content.removeFromTop (12);

    const auto padHeight = juce::jlimit (230, 345, content.getHeight() * 48 / 100);
    layout.pads = content.removeFromTop (padHeight);
    content.removeFromTop (12);
    layout.controls = content;
    return layout;
}

void DrumalorAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    juce::ColourGradient background (colour (0xff0e1218), 0.0f, 0.0f,
                                     colour (0xff080b10), 0.0f,
                                     static_cast<float> (getHeight()), false);
    background.addColour (0.46, colour (0xff15141a));
    graphics.setGradientFill (background);
    graphics.fillAll();

    graphics.setColour (colour (0x137b8c9f));
    const auto spacing = juce::jmax (24, getWidth() / 45);
    for (int x = 20; x < getWidth(); x += spacing)
        graphics.drawVerticalLine (x, 68.0f, static_cast<float> (getHeight() - 18));

    const auto layout = calculateLayout();
    drawPanel (graphics, layout.pads);
    drawPanel (graphics, layout.controls);

    graphics.setColour (colour (accent));
    graphics.fillRect (layout.header.getX(), layout.header.getBottom() - 2,
                       juce::jmax (110, getWidth() / 7), 2);
    graphics.setColour (colour (accentHot));
    graphics.fillRect (layout.header.getX() + juce::jmax (110, getWidth() / 7),
                       layout.header.getBottom() - 2, 52, 2);

    graphics.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    graphics.setColour (colour (textDim));
    graphics.drawText ("13-VOICE KIT   ·   CLICK A PAD TO SELECT + AUDITION",
                       layout.pads.reduced (14).removeFromTop (22),
                       juce::Justification::centredLeft);

    auto controlContent = layout.controls.reduced (15);
    controlContent.removeFromTop (39);
    const auto outputSeparatorX = controlContent.getX()
                                + controlContent.getWidth() * 4 / 5;
    graphics.setColour (colour (panelEdge));
    graphics.drawVerticalLine (outputSeparatorX,
                               static_cast<float> (controlContent.getY() + 8),
                               static_cast<float> (controlContent.getBottom() - 8));
    graphics.setColour (colour (accentCool).withAlpha (0.58f));
    graphics.fillEllipse (static_cast<float> (outputSeparatorX + 10),
                          static_cast<float> (controlContent.getY() + 11), 5.0f, 5.0f);
}

void DrumalorAudioProcessorEditor::resized()
{
    const auto layout = calculateLayout();

    auto header = layout.header.reduced (6, 3);
    logoLabel.setBounds (header.removeFromLeft (178));
    statusDisplay.setBounds (header.removeFromRight (210).reduced (0, 5));
    header.removeFromRight (10);
    panicButton.setBounds (header.removeFromRight (78).reduced (0, 5));
    header.removeFromRight (12);
    editionLabel.setBounds (header);

    auto padContent = layout.pads.reduced (14);
    padContent.removeFromTop (27);
    constexpr int rowGap = 8;
    auto firstRow = padContent.removeFromTop ((padContent.getHeight() - rowGap) / 2);
    padContent.removeFromTop (rowGap);
    auto secondRow = padContent;

    const auto layoutPadRow = [this] (juce::Rectangle<int> row,
                                      std::size_t firstIndex,
                                      std::size_t count)
    {
        constexpr int gap = 7;
        const auto usableWidth = row.getWidth() - gap * (static_cast<int> (count) - 1);
        const auto padWidth = usableWidth / static_cast<int> (count);
        for (std::size_t offset = 0; offset < count; ++offset)
        {
            const auto targetWidth = offset + 1 == count
                ? row.getWidth()
                : juce::jmax (1, padWidth);
            pads[firstIndex + offset]->setBounds (
                row.removeFromLeft (targetWidth));
            if (offset + 1 < count)
                row.removeFromLeft (gap);
        }
    };

    layoutPadRow (firstRow, 0, 7);
    layoutPadRow (secondRow, 7, drumalor::instrumentCount - 7);

    auto controls = layout.controls.reduced (15);
    selectedInstrumentLabel.setBounds (controls.removeFromTop (31));
    controls.removeFromTop (8);

    constexpr int knobCount = 5;
    const auto knobWidth = controls.getWidth() / knobCount;
    DrumalorKnob* knobs[] = { &characterAKnob, &characterBKnob, &pitchKnob,
                              &decayKnob, &outputKnob };
    for (int index = 0; index < knobCount; ++index)
    {
        auto cell = controls.removeFromLeft (
            index == knobCount - 1 ? controls.getWidth() : knobWidth);
        const auto leftInset = index == knobCount - 1 ? 14 : 4;
        knobs[index]->setBounds (cell.reduced (leftInset, 1));
    }
}
