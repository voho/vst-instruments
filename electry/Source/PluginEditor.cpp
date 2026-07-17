#include "PluginEditor.h"

namespace
{
namespace colours
{
const juce::Colour background { 0xff221912 };
const juce::Colour panel { 0xff2f251a };
const juce::Colour panelOutline { 0xff4a3a27 };
const juce::Colour text { 0xffe9dcc4 };
const juce::Colour dimText { 0xffa99878 };
const juce::Colour accent { 0xffd9973b };
const juce::Colour accentDark { 0xff8a5f24 };
const juce::Colour knobFace { 0xff3a2d1e };
const juce::Colour knobRim { 0xff5c4930 };
} // namespace colours

constexpr int editorWidth = 1080;
constexpr int editorHeight = 660;
} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------

ElectryLookAndFeel::ElectryLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::panel);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId,
               juce::Colour (0xffe4d9c2));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId,
               juce::Colour (0xff1c150e));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,
               juce::Colour (0xff8a7a5d));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
               colours::accent.withAlpha (0.35f));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
               colours::accent.withAlpha (0.7f));
}

void ElectryLookAndFeel::drawRotarySlider (juce::Graphics& graphics, int x, int y,
                                           int width, int height, float sliderPos,
                                           float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Track arc.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    graphics.setColour (colours::knobRim.withAlpha (0.6f));
    graphics.strokePath (track, juce::PathStrokeType (2.4f));

    // Value arc.
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, angle, true);
    graphics.setColour (colours::accent);
    graphics.strokePath (value, juce::PathStrokeType (2.8f));

    // Knob body.
    const auto bodyRadius = radius * 0.78f;
    graphics.setColour (colours::knobFace);
    graphics.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius,
                          bodyRadius * 2.0f, bodyRadius * 2.0f);
    graphics.setColour (colours::knobRim);
    graphics.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius,
                          bodyRadius * 2.0f, bodyRadius * 2.0f, 1.4f);

    // Pointer.
    const auto pointerLength = bodyRadius * 0.72f;
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -pointerLength, 3.2f, pointerLength, 1.6f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                .translated (centre.x, centre.y));
    graphics.setColour (colours::text);
    graphics.fillPath (pointer);
}

void ElectryLookAndFeel::drawButtonBackground (juce::Graphics& graphics,
                                               juce::Button& button, const juce::Colour&,
                                               bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();

    auto fill = on ? colours::accentDark : colours::panel;
    if (isDown)
        fill = fill.brighter (0.25f);
    else if (isHighlighted)
        fill = fill.brighter (0.12f);

    graphics.setColour (fill);
    graphics.fillRoundedRectangle (bounds, 5.0f);
    graphics.setColour (on ? colours::accent : colours::panelOutline);
    graphics.drawRoundedRectangle (bounds, 5.0f, on ? 1.6f : 1.0f);
}

void ElectryLookAndFeel::drawButtonText (juce::Graphics& graphics, juce::TextButton& button,
                                         bool, bool)
{
    graphics.setFont (getTextButtonFont (button, button.getHeight()));
    graphics.setColour (button.getToggleState() ? colours::text
                                                : colours::text.withAlpha (0.82f));
    graphics.drawFittedText (button.getButtonText(),
                             button.getLocalBounds().reduced (2, 1),
                             juce::Justification::centred, 1);
}

void ElectryLookAndFeel::drawLabel (juce::Graphics& graphics, juce::Label& label)
{
    graphics.setColour (label.findColour (juce::Label::textColourId));
    graphics.setFont (label.getFont());
    graphics.drawFittedText (label.getText(), label.getLocalBounds(),
                             label.getJustificationType(), 2);
}

juce::Label* ElectryLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (12.0f));
    label->setColour (juce::Label::textColourId, colours::dimText);
    label->setJustificationType (juce::Justification::centred);
    return label;
}

juce::Font ElectryLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (
        juce::jmin (13.0f, static_cast<float> (buttonHeight) * 0.6f),
        juce::Font::bold));
}

// ---------------------------------------------------------------------------
// Choice strip
// ---------------------------------------------------------------------------

ElectryChoiceStrip::ElectryChoiceStrip (juce::String title, juce::StringArray choices)
    : titleText (std::move (title))
{
    for (int index = 0; index < choices.size(); ++index)
    {
        auto button = std::make_unique<juce::TextButton> (choices[index]);
        button->setClickingTogglesState (false);
        button->setRadioGroupId (0);
        button->onClick = [this, index]
        {
            setSelectedIndex (index);
            if (onChoice != nullptr)
                onChoice (index);
        };
        addAndMakeVisible (*button);
        buttons.push_back (std::move (button));
    }
    if (! buttons.empty())
        buttons.front()->setToggleState (true, juce::dontSendNotification);
}

void ElectryChoiceStrip::setSelectedIndex (int newIndex)
{
    selectedIndex = juce::jlimit (0, static_cast<int> (buttons.size()) - 1, newIndex);
    for (int index = 0; index < static_cast<int> (buttons.size()); ++index)
        buttons[static_cast<std::size_t> (index)]->setToggleState (
            index == selectedIndex, juce::dontSendNotification);
}

void ElectryChoiceStrip::paint (juce::Graphics& graphics)
{
    graphics.setColour (colours::dimText);
    graphics.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    graphics.drawText (titleText, getLocalBounds().removeFromTop (14),
                       juce::Justification::centredLeft);
}

void ElectryChoiceStrip::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (16);
    if (buttons.empty())
        return;
    const int gap = 4;
    const int buttonWidth = (area.getWidth() - gap * (static_cast<int> (buttons.size()) - 1))
                          / static_cast<int> (buttons.size());
    for (auto& button : buttons)
    {
        button->setBounds (area.removeFromLeft (buttonWidth));
        area.removeFromLeft (gap);
    }
}

// ---------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------

ElectryKnob::ElectryKnob (juce::String name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (label);
}

void ElectryKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (14));
    slider.setBounds (area);
}

// ---------------------------------------------------------------------------
// Status display
// ---------------------------------------------------------------------------

void ElectryStatusDisplay::setStatus (int activeVoices, bool ready, double sampleRate,
                                      bool scheduleRepaint)
{
    if (voices == activeVoices && isReady == ready
        && juce::approximatelyEqual (rate, sampleRate))
        return;
    voices = activeVoices;
    isReady = ready;
    rate = sampleRate;
    if (scheduleRepaint)
        repaint();
}

void ElectryStatusDisplay::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour (colours::background.darker (0.3f));
    graphics.fillRoundedRectangle (bounds, 5.0f);
    graphics.setColour (colours::panelOutline);
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    juce::String status;
    if (! isReady)
        status = "ENGINE STANDBY";
    else
    {
        status = juce::String (voices) + (voices == 1 ? " STRING" : " STRINGS");
        if (rate > 0.0)
            status += "  |  " + juce::String (rate / 1000.0, 1) + " kHz";
    }

    graphics.setColour (isReady ? colours::accent : colours::dimText);
    graphics.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    graphics.drawText (status, getLocalBounds().reduced (8, 0),
                       juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

ElectryAudioProcessorEditor::ElectryAudioProcessorEditor (ElectryAudioProcessor& processor)
    : AudioProcessorEditor (&processor),
      electryProcessor (processor),
      keyboard (processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    logoLabel.setText ("ELECTRY", juce::dontSendNotification);
    logoLabel.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    logoLabel.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("PHYSICALLY MODELED DRY ELECTRIC GUITAR",
                          juce::dontSendNotification);
    editionLabel.setFont (juce::FontOptions (11.5f));
    editionLabel.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (editionLabel);

    keyboardHintLabel.setText (
        "Keyswitches C1..G#1 select the play style. Playable range E2..D6.",
        juce::dontSendNotification);
    keyboardHintLabel.setFont (juce::FontOptions (11.0f));
    keyboardHintLabel.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (keyboardHintLabel);

    addAndMakeVisible (statusDisplay);

    panicButton.setTooltip ("Immediately silence all strings");
    panicButton.onClick = [this] { electryProcessor.requestPanic(); };
    addAndMakeVisible (panicButton);

    articulationStrip.onChoice = [this] (int index)
    {
        electryProcessor.triggerArticulation (index);
    };
    addAndMakeVisible (articulationStrip);

    // The pickup selector strip binds to the choice parameter.
    pickupStrip.onChoice = [this] (int index)
    {
        if (auto* parameter = electryProcessor.parameters.getParameter (
                electry::parameters::pickupSelector))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (static_cast<float> (index)));
            parameter->endChangeGesture();
        }
    };
    if (auto* parameter = electryProcessor.parameters.getParameter (
            electry::parameters::pickupSelector))
    {
        pickupAttachment = std::make_unique<juce::ParameterAttachment> (
            *parameter,
            [this] (float newValue)
            {
                pickupStrip.setSelectedIndex (juce::roundToInt (newValue));
            },
            nullptr);
        pickupAttachment->sendInitialUpdate();
    }
    addAndMakeVisible (pickupStrip);

    using namespace electry::parameters;
    const auto setup = [this] (ElectryKnob& knob, const char* parameterId,
                               const char* tooltip)
    {
        knob.slider.setTooltip (tooltip);
        addAndMakeVisible (knob);
        attachSlider (knob.slider, parameterId);
    };

    setup (bodyWoodKnob, bodyWood,
           "Body wood: mahogany/maple blank toward swamp ash");
    setup (bodySizeKnob, bodySize,
           "Body mass and thickness: thick heavy blank toward thin light slab");
    setup (bodyShapeKnob, bodyShape,
           "Body outline: carved single-cut toward flat slab");
    setup (constructionKnob, construction,
           "Neck joint and bridge: set neck + stopbar toward bolt-on + through-body");
    setup (scaleLengthKnob, scaleLength,
           "Scale length: 24.75 in toward 25.5 in");
    setup (bodyResonanceKnob, bodyResonance,
           "How much solid-body structural colour reaches the pickups");
    setup (pickupTypeKnob, pickupType,
           "Pickup construction: wide humbucker toward narrow single coil");
    setup (toneKnob, tone, "Passive tone control loading the pickup resonance");
    setup (stringGaugeKnob, stringGauge, "String set: light 9s toward medium 11s");
    setup (stringAgeKnob, stringAge, "String condition: fresh toward old and dead");
    setup (pickPositionKnob, pickPosition,
           "Picking spot: close to the bridge toward over the neck");
    setup (pickHardnessKnob, pickHardness,
           "Plectrum stiffness and edge: soft and round toward hard and sharp");
    setup (bendTimeKnob, bendTime, "Travel time of keyswitched finger bends");
    setup (muteDampingKnob, muteDamping,
           "Palm-mute strength for the Muted play style");
    setup (velocityKnob, velocity, "How strongly MIDI velocity drives the pluck");
    setup (pickNoiseKnob, pickNoise, "Plectrum contact and scrape level");
    setup (fingerNoiseKnob, fingerNoise, "Fretting-hand contact level");
    setup (releaseNoiseKnob, releaseNoise, "String damping noise at note end");
    setup (outputKnob, output, "Master output level");

    keyboard.setAvailableRange (electry::ElectryEngine::firstKeyswitchNote,
                                electry::ElectryEngine::highestPlayableNote);
    keyboard.setLowestVisibleKey (electry::ElectryEngine::firstKeyswitchNote);
    keyboard.setKeyWidth (17.0f);
    keyboard.setOctaveForMiddleC (4);
    addAndMakeVisible (keyboard);

    setSize (editorWidth, editorHeight);
    startTimerHz (12);
}

ElectryAudioProcessorEditor::~ElectryAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ElectryAudioProcessorEditor::attachSlider (juce::Slider& slider,
                                                const char* parameterId)
{
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (
        electryProcessor.parameters, parameterId, slider));
}

void ElectryAudioProcessorEditor::timerCallback()
{
    statusDisplay.setStatus (electryProcessor.getActiveVoiceCount(),
                             electryProcessor.isEngineReady(),
                             electryProcessor.getCurrentSampleRateForDisplay());
    articulationStrip.setSelectedIndex (
        electryProcessor.getCurrentArticulationIndex());
}

void ElectryAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (colours::background);

    const std::array<const char*, sectionCount> titles {
        "", "GUITAR", "PICKUPS", "STRINGS & PLAYING", "PLAY NOISE", "MASTER"
    };

    for (int section = 0; section < sectionCount; ++section)
    {
        const auto bounds = sectionBounds[static_cast<std::size_t> (section)];
        if (bounds.isEmpty())
            continue;
        graphics.setColour (colours::panel);
        graphics.fillRoundedRectangle (bounds.toFloat(), 8.0f);
        graphics.setColour (colours::panelOutline);
        graphics.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);

        if (titles[static_cast<std::size_t> (section)][0] != '\0')
        {
            graphics.setColour (colours::dimText);
            graphics.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            graphics.drawText (titles[static_cast<std::size_t> (section)],
                               bounds.reduced (12, 6).removeFromTop (14),
                               juce::Justification::centredLeft);
        }
    }
}

void ElectryAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Header.
    auto header = area.removeFromTop (44);
    logoLabel.setBounds (header.removeFromLeft (170));
    editionLabel.setBounds (header.removeFromLeft (330));
    panicButton.setBounds (header.removeFromRight (76).reduced (0, 8));
    header.removeFromRight (8);
    statusDisplay.setBounds (header.removeFromRight (210).reduced (0, 8));

    area.removeFromTop (6);

    // Keyboard and hint at the bottom.
    keyboardHintLabel.setBounds (area.removeFromBottom (18));
    keyboard.setBounds (area.removeFromBottom (86));
    area.removeFromBottom (8);

    // Articulation strip across the top.
    auto articulationArea = area.removeFromTop (66);
    sectionBounds[articulationSection] = articulationArea;
    articulationStrip.setBounds (articulationArea.reduced (12, 8));
    area.removeFromTop (8);

    const int knobHeight = 96;
    const int knobWidth = 92;

    const auto layoutKnobRow = [] (juce::Rectangle<int> rowArea,
                                   std::initializer_list<ElectryKnob*> knobs,
                                   int width, int height)
    {
        auto row = rowArea;
        for (auto* knob : knobs)
            knob->setBounds (row.removeFromLeft (width)
                                 .withSizeKeepingCentre (width, height));
    };

    // Left column: GUITAR. Right column stacks PICKUPS over MASTER.
    auto topRow = area.removeFromTop (knobHeight + 44);
    auto guitarArea = topRow.removeFromLeft (6 * knobWidth + 24);
    sectionBounds[guitarSection] = guitarArea;
    layoutKnobRow (guitarArea.reduced (12).withTrimmedTop (16),
                   { &bodyWoodKnob, &bodySizeKnob, &bodyShapeKnob,
                     &constructionKnob, &scaleLengthKnob, &bodyResonanceKnob },
                   knobWidth, knobHeight);

    topRow.removeFromLeft (8);
    auto pickupArea = topRow;
    sectionBounds[pickupSection] = pickupArea;
    {
        auto inner = pickupArea.reduced (12).withTrimmedTop (16);
        auto stripArea = inner.removeFromLeft (juce::jmax (150, inner.getWidth() - 2 * knobWidth));
        pickupStrip.setBounds (stripArea.withSizeKeepingCentre (
            stripArea.getWidth(), 58));
        layoutKnobRow (inner, { &pickupTypeKnob, &toneKnob }, knobWidth, knobHeight);
    }

    area.removeFromTop (8);

    // Second row: STRINGS & PLAYING plus PLAY NOISE and MASTER.
    auto bottomRow = area;
    auto playArea = bottomRow.removeFromLeft (7 * knobWidth + 24);
    sectionBounds[playSection] = playArea;
    layoutKnobRow (playArea.reduced (12).withTrimmedTop (16),
                   { &stringGaugeKnob, &stringAgeKnob, &pickPositionKnob,
                     &pickHardnessKnob, &bendTimeKnob, &muteDampingKnob,
                     &velocityKnob },
                   knobWidth, knobHeight);

    bottomRow.removeFromLeft (8);
    auto noiseArea = bottomRow.removeFromLeft (3 * knobWidth + 24);
    sectionBounds[noiseSection] = noiseArea;
    layoutKnobRow (noiseArea.reduced (12).withTrimmedTop (16),
                   { &pickNoiseKnob, &fingerNoiseKnob, &releaseNoiseKnob },
                   knobWidth, knobHeight);

    bottomRow.removeFromLeft (8);
    sectionBounds[masterSection] = bottomRow;
    layoutKnobRow (bottomRow.reduced (12).withTrimmedTop (16),
                   { &outputKnob }, knobWidth, knobHeight);
}
