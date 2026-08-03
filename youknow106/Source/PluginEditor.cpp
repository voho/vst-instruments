#include "PluginEditor.h"

#include <cmath>
#include <cstring>

namespace
{
using namespace youknow106;

juce::Colour fromPalette (std::uint32_t rgb) noexcept
{
    return juce::Colour (static_cast<juce::uint8> ((rgb >> 16) & 0xffu),
                         static_cast<juce::uint8> ((rgb >> 8) & 0xffu),
                         static_cast<juce::uint8> (rgb & 0xffu));
}

juce::Colour accentColour (panel::Accent accent) noexcept
{
    return fromPalette (accent == panel::Accent::Magenta ? panel::colour::magenta
                                                         : panel::colour::cyan);
}

juce::Font panelFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold
                                                       : juce::Font::plain));
}
} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------

YouKnow106LookAndFeel::YouKnow106LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId,
               fromPalette (panel::colour::faceplate));
    setColour (juce::Label::textColourId, fromPalette (panel::colour::text));
    setColour (juce::TooltipWindow::backgroundColourId,
               fromPalette (panel::colour::faceplateLow));
    setColour (juce::TooltipWindow::textColourId, fromPalette (panel::colour::text));

    // The patch list. Without these the menu falls back to the base class's own
    // dark scheme, which is readable but is not this panel's palette -- and the
    // list is the main way to get at thirty-three patches, so it is not a corner
    // of the interface. The highlight is the section cyan, which is bright
    // enough that the text on it has to go dark rather than stay light.
    setColour (juce::PopupMenu::backgroundColourId,
               fromPalette (panel::colour::faceplateLow));
    setColour (juce::PopupMenu::textColourId, fromPalette (panel::colour::text));
    setColour (juce::PopupMenu::highlightedBackgroundColourId,
               fromPalette (panel::colour::cyan).withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId,
               fromPalette (panel::colour::faceplate));
    setColour (juce::PopupMenu::headerTextColourId,
               fromPalette (panel::colour::textDim));

    setColour (juce::MidiKeyboardComponent::whiteNoteColourId,
               fromPalette (panel::colour::control));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId,
               fromPalette (panel::colour::faceplateLow));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,
               fromPalette (panel::colour::faceplate));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
               fromPalette (panel::colour::magenta).withAlpha (0.85f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
               fromPalette (panel::colour::cyan).withAlpha (0.45f));
    setColour (juce::MidiKeyboardComponent::shadowColourId,
               juce::Colours::black.withAlpha (0.6f));
}

void YouKnow106LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y,
                                              int width, int height, float sliderPos,
                                              float, float,
                                              juce::Slider::SliderStyle style,
                                              juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                          0.0f, 0.0f, style, slider);
        return;
    }

    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height));

    // The slot: a routed channel through the faceplate, darker at its edges.
    const float slotWidth = juce::jmax (3.0f, bounds.getWidth() * 0.18f);
    const auto slot = bounds.withSizeKeepingCentre (slotWidth, bounds.getHeight())
                          .reduced (0.0f, 2.0f);
    g.setColour (fromPalette (panel::colour::slot));
    g.fillRoundedRectangle (slot, slotWidth * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (slot, slotWidth * 0.5f, 1.0f);

    // Travel ticks either side of the slot.
    g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.35f));
    for (int tick = 0; tick <= 10; ++tick)
    {
        const float t = bounds.getY() + 3.0f
                      + (bounds.getHeight() - 6.0f) * static_cast<float> (tick) / 10.0f;
        const float length = (tick % 5 == 0) ? 6.0f : 3.0f;
        g.fillRect (bounds.getX() + 1.0f, t, length, 1.0f);
        g.fillRect (bounds.getRight() - 1.0f - length, t, length, 1.0f);
    }

    // The cap.
    const float capHeight = juce::jmax (12.0f, bounds.getHeight() * 0.075f);
    const float capWidth = bounds.getWidth() * 0.92f;
    const auto cap = juce::Rectangle<float> (capWidth, capHeight)
                         .withCentre ({ bounds.getCentreX(),
                                        juce::jlimit (bounds.getY() + capHeight * 0.5f,
                                                      bounds.getBottom() - capHeight * 0.5f,
                                                      sliderPos) });

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 2.5f);

    juce::ColourGradient gradient (fromPalette (panel::colour::control),
                                   cap.getX(), cap.getY(),
                                   fromPalette (panel::colour::controlShadow),
                                   cap.getX(), cap.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (cap, 2.5f);

    // The witness line the eye actually reads the value from.
    g.setColour (fromPalette (panel::colour::faceplateLow));
    g.fillRect (cap.getX() + 1.5f, cap.getCentreY() - 0.75f, cap.getWidth() - 3.0f, 1.5f);
}

void YouKnow106LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour&,
                                                  bool isHighlighted, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.0f), 2.5f);

    juce::ColourGradient gradient (
        fromPalette (isDown ? panel::colour::faceplateLow : panel::colour::faceplateHigh),
        bounds.getX(), bounds.getY(),
        fromPalette (panel::colour::faceplateLow),
        bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, 2.5f);

    g.setColour (fromPalette (on ? panel::colour::led : panel::colour::textDim)
                     .withAlpha (isHighlighted ? 0.9f : 0.55f));
    g.drawRoundedRectangle (bounds, 2.5f, 1.0f);

    // The indicator. A lit button glows; an unlit one still shows its lens, so
    // the panel reads the same whether or not anything is on.
    const float lens = juce::jmin (7.0f, bounds.getHeight() * 0.34f);
    const auto led = juce::Rectangle<float> (lens, lens)
                         .withCentre ({ bounds.getCentreX(),
                                        bounds.getY() + bounds.getHeight() * 0.26f });
    if (on)
    {
        g.setColour (fromPalette (panel::colour::led).withAlpha (0.30f));
        g.fillEllipse (led.expanded (lens * 0.75f));
        g.setColour (fromPalette (panel::colour::led));
    }
    else
    {
        g.setColour (fromPalette (panel::colour::ledDim));
    }
    g.fillEllipse (led);
}

void YouKnow106LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                            bool, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    g.setColour (fromPalette (button.getToggleState() ? panel::colour::text
                                                      : panel::colour::textDim));
    // The size comes from the panel description rather than from a local
    // formula, so the legend drawn here is the legend the layout check measured.
    // Bounds are in pixels and the sizing is linear in them, so this yields the
    // panel-unit size already multiplied by the editor's scale.
    g.setFont (panelFont (panel::buttonPointSizeFor (
                              button.getButtonText().toRawUTF8(),
                              bounds.getWidth(), bounds.getHeight()), true));
    // One line, and no horizontal squashing: the size above already guarantees
    // the fit, so anything that did not fit should be visible as a layout bug
    // rather than quietly condensed into place.
    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds().withTrimmedTop (
                          juce::roundToInt (bounds.getHeight() * 0.40f)),
                      juce::Justification::centredTop, 1, 1.0f);
}

void YouKnow106LookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), label.getLocalBounds(),
                      label.getJustificationType(), 2, 1.0f);
}

juce::Label* YouKnow106LookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setColour (juce::Label::textColourId, fromPalette (panel::colour::text));
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setJustificationType (juce::Justification::centred);
    return label;
}

// ---------------------------------------------------------------------------
// Plastic texture
// ---------------------------------------------------------------------------

void PlasticTexture::ensureBuilt (int tileSize)
{
    if (tile.isValid() && tile.getWidth() == tileSize)
        return;

    tile = juce::Image (juce::Image::ARGB, tileSize, tileSize, true);
    juce::Image::BitmapData data (tile, juce::Image::BitmapData::writeOnly);

    // A fixed seed: the moulding is part of the instrument, so it must not
    // change between repaints or between runs.
    juce::Random random (0x106u);
    for (int y = 0; y < tileSize; ++y)
    {
        // A faint horizontal grain from the moulding direction, on top of the
        // speckle. Both are subtle enough to read as a surface rather than
        // as noise.
        const int grain = ((y % 3) == 0) ? 3 : 0;
        for (int x = 0; x < tileSize; ++x)
        {
            const int speckle = random.nextInt (13) - 6;
            const int level = juce::jlimit (0, 255, 128 + speckle + grain);
            const auto alpha = static_cast<juce::uint8> (26);
            data.setPixelColour (x, y, juce::Colour (static_cast<juce::uint8> (level),
                                                     static_cast<juce::uint8> (level),
                                                     static_cast<juce::uint8> (level),
                                                     alpha));
        }
    }
}

void PlasticTexture::fill (juce::Graphics& g, juce::Rectangle<int> area,
                           juce::Colour base) const
{
    g.setColour (base);
    g.fillRect (area);

    if (! tile.isValid())
        return;

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (area);
    for (int y = area.getY(); y < area.getBottom(); y += tile.getHeight())
        for (int x = area.getX(); x < area.getRight(); x += tile.getWidth())
            g.drawImageAt (tile, x, y);
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void YouKnow106Display::refresh (const YouKnow106AudioProcessor& source)
{
    const int mask = source.getVoiceMaskForDisplay();
    const int count = source.getActiveVoiceCount();
    const int limit = source.getVoiceLimitForDisplay();
    const float env = source.getEnvelopeForDisplay();
    const float modulation = source.getLfoForDisplay();
    const double rate = source.getCurrentSampleRateForDisplay();
    const int factor = source.getOversamplingFactorForDisplay();
    const bool isReady = source.isEngineReady();

    if (mask == voiceMask && count == voices && limit == voiceLimit
        && std::abs (env - envelope) < 0.004f
        && std::abs (modulation - lfo) < 0.004f
        && std::abs (rate - sampleRate) < 1.0e-6
        && factor == oversampling && isReady == ready)
        return;

    voiceMask = mask;
    voices = count;
    voiceLimit = limit;
    envelope = env;
    lfo = modulation;
    sampleRate = rate;
    oversampling = factor;
    ready = isReady;
    repaint();
}

void YouKnow106Display::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (fromPalette (panel::colour::faceplateLow));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (fromPalette (panel::colour::cyan).withAlpha (0.35f));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    auto area = bounds.reduced (8.0f, 6.0f);
    const float rowHeight = area.getHeight() / 2.0f;

    // Voice indicators: one lamp per available voice, so the row follows the
    // VOICES setting instead of always showing the default six.
    auto voiceRow = area.removeFromTop (rowHeight);
    const auto readout = voiceRow.removeFromRight (voiceRow.getWidth() * 0.44f);
    const int lamps =
        juce::jlimit (1, youknow106::YouKnow106Engine::maxVoices, voiceLimit);
    // Fixed pitch across the available width, so sixteen lamps shrink to fit
    // rather than running off the end of the display.
    const float pitch = voiceRow.getWidth() / static_cast<float> (lamps);
    const float lampSize = juce::jmin (9.0f, voiceRow.getHeight() * 0.7f, pitch * 0.62f);
    for (int voice = 0; voice < lamps; ++voice)
    {
        const auto lamp = juce::Rectangle<float> (lampSize, lampSize)
                              .withCentre ({ voiceRow.getX()
                                                 + (static_cast<float> (voice) + 0.5f) * pitch,
                                             voiceRow.getCentreY() });
        const bool lit = (voiceMask & (1 << voice)) != 0;
        if (lit)
        {
            g.setColour (fromPalette (panel::colour::led).withAlpha (0.28f));
            g.fillEllipse (lamp.expanded (lampSize * 0.6f));
        }
        g.setColour (fromPalette (lit ? panel::colour::led : panel::colour::ledDim));
        g.fillEllipse (lamp);
    }

    g.setColour (fromPalette (panel::colour::textDim));
    g.setFont (panelFont (10.0f));
    g.drawText (ready ? juce::String (voices) + " / " + juce::String (lamps) + " VOICES"
                      : juce::String ("STANDBY"),
                readout.toNearestInt(), juce::Justification::centredRight);

    // Modulation and envelope meters.
    const auto meter = [&g] (juce::Rectangle<float> row, float value, bool bipolar,
                             std::uint32_t tint, const char* caption)
    {
        auto labelArea = row.removeFromLeft (34.0f);
        g.setColour (fromPalette (panel::colour::textDim));
        g.setFont (panelFont (9.0f, true));
        g.drawText (caption, labelArea.toNearestInt(), juce::Justification::centredLeft);

        const auto track = row.reduced (0.0f, row.getHeight() * 0.33f);
        g.setColour (fromPalette (panel::colour::slot));
        g.fillRoundedRectangle (track, 1.5f);

        g.setColour (fromPalette (tint));
        if (bipolar)
        {
            const float centre = track.getCentreX();
            const float span = track.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, value);
            g.fillRoundedRectangle (juce::Rectangle<float> (
                                        juce::jmin (centre, centre + span), track.getY(),
                                        std::abs (span), track.getHeight()), 1.5f);
        }
        else
        {
            g.fillRoundedRectangle (juce::Rectangle<float> (
                                        track.getX(), track.getY(),
                                        track.getWidth() * juce::jlimit (0.0f, 1.0f, value),
                                        track.getHeight()), 1.5f);
        }
    };

    const float half = area.getHeight() / 2.0f;
    meter (area.removeFromTop (half), lfo, true, panel::colour::magenta, "LFO");
    meter (area, envelope, false, panel::colour::led, "ENV");
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

YouKnow106AudioProcessorEditor::YouKnow106AudioProcessorEditor (YouKnow106AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    texture.ensureBuilt (96);

    logoLabel.setText ("youknow106", juce::dontSendNotification);
    logoLabel.setFont (panelFont (22.0f, true));
    logoLabel.setColour (juce::Label::textColourId, fromPalette (panel::colour::cyan));
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("SIX-VOICE DCO POLYSYNTH", juce::dontSendNotification);
    editionLabel.setFont (panelFont (10.0f, true));
    editionLabel.setColour (juce::Label::textColourId,
                            fromPalette (panel::colour::textDim));
    editionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (editionLabel);

    addAndMakeVisible (display);

    buildPanelControls();
    buildUtilityStrip();
    buildPresetBar();

    keyboard.setAvailableRange (0, 127);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (22.0f);
    keyboard.setScrollButtonsVisible (true);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setKeyPressBaseOctave (3);
    keyboard.setName ("Playable keyboard");
    keyboard.setTitle ("Playable keyboard");
    keyboard.setDescription ("Play notes with the mouse or computer keyboard");
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (900, 380, 2200, 940);
    setSize (juce::roundToInt (panel::panelWidth()),
             juce::roundToInt (panel::panelHeight + panel::keyboardHeight + 14.0f));
    startTimerHz (24);
}

YouKnow106AudioProcessorEditor::~YouKnow106AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void YouKnow106AudioProcessorEditor::buildPanelControls()
{
    const auto& controls = panel::controls();

    for (std::size_t index = 0; index < controls.size(); ++index)
    {
        const auto& description = controls[index];
        auto& entry = panelControls[index];

        if (description.kind == panel::ControlKind::Slider
            || description.kind == panel::ControlKind::Steps)
        {
            entry.slider = std::make_unique<juce::Slider>();
            entry.slider->setSliderStyle (juce::Slider::LinearVertical);
            entry.slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            entry.slider->setName (description.label);
            entry.slider->setTitle (description.label);
            if (auto* parameter = audioProcessor.parameters.getParameter (description.parameterId))
                entry.slider->setTooltip (parameter->getName (64));
            addAndMakeVisible (*entry.slider);
            attachSlider (*entry.slider, description.parameterId);
        }
        else
        {
            entry.button = std::make_unique<juce::TextButton> (description.label);
            // Radio lamps are owned by their shared parameter attachment. If
            // they self-toggle, clicking the already-selected value can turn
            // its lamp off while the unchanged parameter suppresses a callback.
            const bool isPoly = std::strcmp (description.parameterId,
                                             parameters::poly1) == 0
                             || std::strcmp (description.parameterId,
                                             parameters::poly2) == 0;
            entry.button->setClickingTogglesState (
                description.kind != panel::ControlKind::Radio && ! isPoly);
            entry.button->setName (description.label);
            entry.button->setTitle (description.label);
            addAndMakeVisible (*entry.button);

            if (description.kind == panel::ControlKind::Toggle)
            {
                if (std::strcmp (description.parameterId, parameters::poly1) == 0)
                {
                    entry.button->setTooltip (
                        "Click to select POLY 1; Shift-click either POLY button for "
                        "Solo Unison; re-click the selected mode to reassign held keys");
                    attachPolyButton (*entry.button, parameters::poly1,
                                      parameters::poly2);
                }
                else if (std::strcmp (description.parameterId,
                                      parameters::poly2) == 0)
                {
                    entry.button->setTooltip (
                        "Click to select POLY 2; Shift-click either POLY button for "
                        "Solo Unison; re-click the selected mode to reassign held keys");
                    attachPolyButton (*entry.button, parameters::poly2,
                                      parameters::poly1);
                }
                else if (std::strcmp (description.parameterId,
                                      parameters::chorusI) == 0)
                {
                    entry.button->setTooltip (
                        "Chorus I — provisional JUNO-60 timing fallback; "
                        "absolute JUNO-106 timing is not yet measured");
                    attachExclusiveButton (*entry.button, parameters::chorusI,
                                           parameters::chorusII);
                }
                else if (std::strcmp (description.parameterId,
                                      parameters::chorusII) == 0)
                {
                    entry.button->setTooltip (
                        "Chorus II — provisional JUNO-60 timing fallback; "
                        "absolute JUNO-106 timing is not yet measured");
                    attachExclusiveButton (*entry.button, parameters::chorusII,
                                           parameters::chorusI);
                }
                else
                    attachButton (*entry.button, description.parameterId);
            }
            else
                attachRadio (*entry.button, description.parameterId,
                             description.groupValue);
        }

        entry.label = std::make_unique<juce::Label>();
        entry.label->setText (description.label, juce::dontSendNotification);
        entry.label->setFont (panelFont (panel::labelPointSize, true));
        entry.label->setColour (juce::Label::textColourId,
                                fromPalette (panel::colour::text));
        entry.label->setJustificationType (juce::Justification::centredTop);
        entry.label->setInterceptsMouseClicks (false, false);
        // A stacked button already carries its own legend; repeating it under
        // the stack would just be clutter.
        if (description.kind == panel::ControlKind::Slider
            || description.kind == panel::ControlKind::Steps)
            addAndMakeVisible (*entry.label);
    }
}

void YouKnow106AudioProcessorEditor::buildUtilityStrip()
{
    using namespace youknow106::parameters;

    const auto configure = [this] (juce::Slider& slider, const char* parameterId,
                                   const char* title)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 16);
        slider.setName (title);
        slider.setTitle (title);
        addAndMakeVisible (slider);
        attachSlider (slider, parameterId);
    };

    configure (transposeSlider, transpose, "Transpose");
    configure (tuneSlider, masterTune, "Master tune");
    configure (velocitySlider, velocity, "Velocity");
    configure (calibrationSlider, calibration, "Unit Character");
    configure (chorusNoiseSlider, chorusNoise, "Chorus noise");
    configure (polyphonySlider, polyphony, "Polyphony");

    const char* captions[] = { "TRANSPOSE", "TUNE", "VELOCITY",
                               "UNIT CHARACTER", "CHORUS NOISE", "VOICES" };
    for (std::size_t index = 0; index < utilityLabels.size(); ++index)
    {
        auto& label = utilityLabels[index];
        label.setText (captions[index], juce::dontSendNotification);
        label.setFont (panelFont (9.0f, true));
        label.setColour (juce::Label::textColourId, fromPalette (panel::colour::textDim));
        label.setJustificationType (juce::Justification::centredLeft);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
    }

    hqButton.setClickingTogglesState (true);
    hqButton.setTooltip ("Run the oscillators, filter and delay lines oversampled");
    addAndMakeVisible (hqButton);
    attachButton (hqButton, hq);

    panicButton.setTooltip ("Silence every voice immediately");
    panicButton.onClick = [this] { audioProcessor.requestPanic(); };
    addAndMakeVisible (panicButton);

    randomize10Button.setTooltip ("Nudge the patch");
    randomize10Button.onClick = [this] { audioProcessor.randomizeParameters (0.10f); };
    addAndMakeVisible (randomize10Button);

    sendSysExButton.setTooltip ("Send the current panel to hardware as a patch dump");
    sendSysExButton.onClick = [this] { audioProcessor.requestSysExDump(); };
    addAndMakeVisible (sendSysExButton);

    randomize100Button.setTooltip ("Draw a completely new patch");
    randomize100Button.onClick = [this] { audioProcessor.randomizeParameters (1.0f); };
    addAndMakeVisible (randomize100Button);
}

void YouKnow106AudioProcessorEditor::buildPresetBar()
{
    presetLabel.setText ("PATCH", juce::dontSendNotification);
    presetLabel.setFont (panelFont (9.0f, true));
    presetLabel.setColour (juce::Label::textColourId, fromPalette (panel::colour::textDim));
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    presetLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (presetLabel);

    // A ComboBox reserves id 0 for "nothing selected", so an item id is the
    // program index plus one. Only the three places that touch the box know
    // that; everything else in the bar works in program indices.
    for (int index = 0; index < audioProcessor.getNumPrograms(); ++index)
        presetBox.addItem (audioProcessor.getProgramName (index), index + 1);

    presetBox.setTooltip ("Recall a factory patch");
    presetBox.setColour (juce::ComboBox::backgroundColourId,
                         fromPalette (panel::colour::slot));
    presetBox.setColour (juce::ComboBox::textColourId, fromPalette (panel::colour::text));
    presetBox.setColour (juce::ComboBox::outlineColourId,
                         fromPalette (panel::colour::textDim).withAlpha (0.35f));
    presetBox.setColour (juce::ComboBox::arrowColourId, fromPalette (panel::colour::cyan));
    presetBox.onChange = [this] { selectProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    presetPrevButton.setTooltip ("Previous patch");
    presetPrevButton.onClick = [this] { stepProgram (-1); };
    addAndMakeVisible (presetPrevButton);

    presetNextButton.setTooltip ("Next patch");
    presetNextButton.onClick = [this] { stepProgram (1); };
    addAndMakeVisible (presetNextButton);

    // JUCE does not emit ComboBox::onChange when the user picks the already
    // selected item, so an explicit reload is the only reliable way to discard
    // edits without stepping to another sound and back.
    presetReloadButton.setName ("Reload patch");
    presetReloadButton.setTooltip ("Reload the selected patch and discard edits");
    presetReloadButton.onClick = [this]
    {
        selectProgram (audioProcessor.getCurrentProgram());
    };
    addAndMakeVisible (presetReloadButton);

    presetEditedLabel.setText ("EDITED", juce::dontSendNotification);
    presetEditedLabel.setFont (panelFont (9.0f, true));
    presetEditedLabel.setColour (juce::Label::textColourId,
                                 fromPalette (panel::colour::magenta));
    presetEditedLabel.setJustificationType (juce::Justification::centredLeft);
    presetEditedLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (presetEditedLabel);

    refreshPresetBar();
}

void YouKnow106AudioProcessorEditor::selectProgram (int index)
{
    if (index < 0 || index >= audioProcessor.getNumPrograms())
        return;

    // Deliberately apply even when this is already the selected program. The
    // RELOAD button promises an exact panel restore; a sub-7-bit slider move is
    // audibly equivalent and need not light EDITED, but its cap must still move
    // back to the stored position when the player asks to discard it.
    audioProcessor.setCurrentProgram (index);
    // The host owns the program index in its own UI too, so tell it the change
    // came from here rather than letting the two drift apart.
    audioProcessor.updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    refreshPresetBar();
}

void YouKnow106AudioProcessorEditor::stepProgram (int delta)
{
    const int wanted = juce::jlimit (0, audioProcessor.getNumPrograms() - 1,
                                     audioProcessor.getCurrentProgram() + delta);
    if (wanted == audioProcessor.getCurrentProgram())
        return;

    audioProcessor.setCurrentProgram (wanted);
    audioProcessor.updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    refreshPresetBar();
}

void YouKnow106AudioProcessorEditor::refreshPresetBar()
{
    const int program = audioProcessor.getCurrentProgram();
    const bool edited = audioProcessor.currentProgramIsEdited();
    if (program == shownProgram && edited == shownEdited)
        return;

    shownProgram = program;
    shownEdited = edited;
    presetBox.setSelectedId (program + 1, juce::dontSendNotification);
    presetEditedLabel.setVisible (edited);
    presetPrevButton.setEnabled (program > 0);
    presetNextButton.setEnabled (program < audioProcessor.getNumPrograms() - 1);
}

void YouKnow106AudioProcessorEditor::attachSlider (juce::Slider& slider,
                                                   const char* parameterId)
{
    if (const auto* parameter = audioProcessor.parameters.getParameter (parameterId))
        slider.setDoubleClickReturnValue (
            true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    else
        jassertfalse;

    sliderAttachments.push_back (std::make_unique<SliderAttachment> (
        audioProcessor.parameters, parameterId, slider));
}

void YouKnow106AudioProcessorEditor::attachButton (juce::Button& button,
                                                   const char* parameterId)
{
    jassert (audioProcessor.parameters.getParameter (parameterId) != nullptr);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (
        audioProcessor.parameters, parameterId, button));
}

void YouKnow106AudioProcessorEditor::attachExclusiveButton (
    juce::Button& button, const char* parameterId, const char* otherParameterId)
{
    attachButton (button, parameterId);
    button.onClick = [this, &button, otherParameterId]
    {
        if (! button.getToggleState())
            return; // Pressing the lit hardware button switches chorus off.

        if (auto* other = audioProcessor.parameters.getParameter (otherParameterId))
        {
            other->beginChangeGesture();
            other->setValueNotifyingHost (other->convertTo0to1 (0.0f));
            other->endChangeGesture();
        }
    };
}

void YouKnow106AudioProcessorEditor::attachPolyButton (
    juce::Button& button, const char* parameterId, const char* otherParameterId)
{
    // The contacts are momentary and the assigner firmware owns their lamps.
    // Use a ParameterAttachment with notification suppressed on the lamp: a
    // ButtonAttachment reports parameter-driven lamp changes as clicks, which
    // would recursively run the transition below.
    auto* attachedParameter = audioProcessor.parameters.getParameter (parameterId);
    jassert (attachedParameter != nullptr);
    if (attachedParameter == nullptr)
        return;
    auto attachment = std::make_unique<juce::ParameterAttachment> (
        *attachedParameter,
        [&button] (float current)
        {
            button.setToggleState (current > 0.5f, juce::dontSendNotification);
        },
        nullptr);

    button.onClick = [this, parameterId, otherParameterId]
    {
        // Both decisions come from the parameter atomics. The lamp update is
        // asynchronous, so using its presentation state here can take the
        // wrong branch when host automation and a mouse click coincide.
        const auto isOn = [this] (const char* id)
        {
            if (const auto* value = audioProcessor.parameters.getRawParameterValue (id))
                return value->load (std::memory_order_relaxed) > 0.5f;
            return false;
        };
        const bool ownWasOn = isOn (parameterId);
        const bool otherWasOn = isOn (otherParameterId);

        const auto set = [this] (const char* id, bool on)
        {
            if (auto* target = audioProcessor.parameters.getParameter (id))
            {
                const float wanted = on ? 1.0f : 0.0f;
                target->beginChangeGesture();
                target->setValueNotifyingHost (target->convertTo0to1 (wanted));
                target->endChangeGesture();
            }
        };

        // A mouse cannot hold both panel contacts at once. Shift-click is the
        // explicit virtual equivalent of the hardware's simultaneous press;
        // an ordinary click retains the real single-button meaning.
        if (juce::ModifierKeys::currentModifiers.isShiftDown())
        {
            if (ownWasOn && otherWasOn)
            {
                // Both lamps already show Solo Unison, but the contacts are
                // momentary. Holding them again still enters the firmware
                // handler and rebuilds every held assignment.
                audioProcessor.requestKeyModeReassert();
                return;
            }
            set (parameterId, true);
            set (otherParameterId, true);
            return;
        }

        if (ownWasOn && ! otherWasOn)
        {
            // The visible latch does not move, but the real assigner still
            // gates, clears and rescans held keys on this repeated press.
            audioProcessor.requestKeyModeReassert();
            return;
        }

        if (ownWasOn && otherWasOn)
        {
            // From Solo Unison, pressing one contact alone selects that mode.
            set (otherParameterId, false);
            return;
        }

        // A normal press selects this single mode. Clear the other parameter
        // first: the transient both-off pair canonicalises to the previous/
        // target Poly 1 behavior, whereas setting this one first would render
        // an unintended block of Unison during a Poly 1 -> Poly 2 change.
        set (otherParameterId, false);
        set (parameterId, true);
    };
    auto* pointer = attachment.get();
    parameterAttachments.push_back (std::move (attachment));
    pointer->sendInitialUpdate();
}

void YouKnow106AudioProcessorEditor::attachRadio (juce::Button& button,
                                                  const char* parameterId, int value)
{
    auto* parameter = audioProcessor.parameters.getParameter (parameterId);
    jassert (parameter != nullptr);
    if (parameter == nullptr)
        return;

    auto attachment = std::make_unique<juce::ParameterAttachment> (
        *parameter,
        [&button, value] (float current)
        {
            button.setToggleState (juce::roundToInt (current) == value,
                                   juce::dontSendNotification);
        },
        nullptr);

    auto* pointer = attachment.get();
    button.onClick = [pointer, &button, value]
    {
        pointer->setValueAsCompleteGesture (static_cast<float> (value));
        // A complete gesture to the already-selected value is intentionally a
        // no-op at the parameter. Keep the lamp canonical in that case too.
        button.setToggleState (true, juce::dontSendNotification);
    };
    parameterAttachments.push_back (std::move (attachment));
    pointer->sendInitialUpdate();
}

juce::Rectangle<float> YouKnow106AudioProcessorEditor::scaled (float x, float y,
                                                               float width,
                                                               float height) const
{
    return { x * scale, y * scale, width * scale, height * scale };
}

void YouKnow106AudioProcessorEditor::resized()
{
    const auto bounds = getLocalBounds();
    const float totalUnits = panel::panelHeight + panel::keyboardHeight;
    scale = juce::jmin (static_cast<float> (bounds.getWidth()) / panel::panelWidth(),
                        static_cast<float> (bounds.getHeight()) / totalUnits);

    const auto& controls = panel::controls();
    for (std::size_t index = 0; index < controls.size(); ++index)
    {
        const auto& description = controls[index];
        auto& entry = panelControls[index];
        const auto area = scaled (description.x, description.y,
                                  description.width, description.height);

        if (entry.slider != nullptr)
            entry.slider->setBounds (area.toNearestInt());
        else if (entry.button != nullptr)
            entry.button->setBounds (area.toNearestInt());

        if (entry.label != nullptr)
            entry.label->setBounds (
                scaled (description.labelX, panel::labelTop,
                        description.labelWidth, panel::labelHeight).toNearestInt());
    }

    logoLabel.setBounds (scaled (panel::panelMargin, panel::utilityTop + 4.0f,
                                 190.0f, 24.0f).toNearestInt());
    editionLabel.setBounds (scaled (panel::panelMargin, panel::utilityTop + 26.0f,
                                    190.0f, 14.0f).toNearestInt());

    display.setBounds (scaled (panel::panelWidth() - panel::panelMargin - 260.0f,
                               panel::utilityTop, 260.0f,
                               panel::utilityHeight).toNearestInt());

    // Utility strip: six compact horizontal controls and four buttons between
    // the logo and the display.
    const float stripLeft = panel::panelMargin + 206.0f;
    const float stripRight = panel::panelWidth() - panel::panelMargin - 274.0f;
    const float stripWidth = juce::jmax (10.0f, stripRight - stripLeft);
    const float cellWidth = stripWidth / 5.0f;

    juce::Slider* utilitySliders[] = { &transposeSlider, &tuneSlider, &velocitySlider,
                                       &calibrationSlider, &chorusNoiseSlider,
                                       &polyphonySlider };
    for (int index = 0; index < 6; ++index)
    {
        const float column = static_cast<float> (index % 3);
        const float row = static_cast<float> (index / 3);
        const float x = stripLeft + column * cellWidth;
        const float y = panel::utilityTop + 2.0f + row * 22.0f;
        utilityLabels[static_cast<std::size_t>(index)].setBounds (
            scaled (x, y, 74.0f, 18.0f).toNearestInt());
        utilitySliders[index]->setBounds (
            scaled (x + 76.0f, y, cellWidth - 82.0f, 18.0f).toNearestInt());
    }

    const float buttonX = stripLeft + 3.0f * cellWidth + 8.0f;
    // Three columns, not two: SEND joined the grid and a third column laid
    // out at the old width ran straight over the status display.
    const float buttonWidth =
        juce::jmax (30.0f, (stripWidth - 3.0f * cellWidth - 16.0f) / 3.0f - 5.0f);
    hqButton.setBounds (scaled (buttonX, panel::utilityTop + 2.0f,
                                buttonWidth, 18.0f).toNearestInt());
    panicButton.setBounds (scaled (buttonX + buttonWidth + 5.0f, panel::utilityTop + 2.0f,
                                   buttonWidth, 18.0f).toNearestInt());
    randomize10Button.setBounds (scaled (buttonX, panel::utilityTop + 24.0f,
                                         buttonWidth, 18.0f).toNearestInt());
    sendSysExButton.setBounds (scaled (buttonX + 2.0f * (buttonWidth + 5.0f),
                                      panel::utilityTop + 2.0f,
                                      buttonWidth, 18.0f).toNearestInt());
    randomize100Button.setBounds (scaled (buttonX + buttonWidth + 5.0f,
                                          panel::utilityTop + 24.0f,
                                          buttonWidth, 18.0f).toNearestInt());

    // Patch bar, its own row under the utility strip: caption, stepper, reload,
    // name box and the edited lamp, left to right.
    const float presetLeft = panel::panelMargin;
    const float stepWidth = 22.0f;
    presetLabel.setBounds (
        scaled (presetLeft, panel::presetTop + 3.0f, 44.0f, 18.0f).toNearestInt());
    presetPrevButton.setBounds (
        scaled (presetLeft + 46.0f, panel::presetTop + 2.0f, stepWidth,
                panel::presetHeight - 4.0f).toNearestInt());
    presetNextButton.setBounds (
        scaled (presetLeft + 46.0f + stepWidth + 3.0f, panel::presetTop + 2.0f,
                stepWidth, panel::presetHeight - 4.0f).toNearestInt());
    constexpr float reloadWidth = 58.0f;
    const float reloadLeft = presetLeft + 46.0f + 2.0f * (stepWidth + 3.0f);
    presetReloadButton.setBounds (
        scaled (reloadLeft, panel::presetTop + 2.0f, reloadWidth,
                panel::presetHeight - 4.0f).toNearestInt());
    const float boxLeft = reloadLeft + reloadWidth + 4.0f;
    presetBox.setBounds (
        scaled (boxLeft, panel::presetTop + 2.0f,
                300.0f, panel::presetHeight - 4.0f).toNearestInt());
    presetEditedLabel.setBounds (
        scaled (boxLeft + 310.0f,
                panel::presetTop + 3.0f, 60.0f, 18.0f).toNearestInt());

    keyboard.setBounds (scaled (0.0f, panel::panelHeight, panel::panelWidth(),
                                panel::keyboardHeight).toNearestInt());
}

void YouKnow106AudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    texture.fill (g, bounds, fromPalette (panel::colour::faceplate));

    const auto& sections = panel::sections();
    for (const auto& section : sections)
    {
        const auto box = scaled (section.x, panel::headerTop, section.width,
                                 panel::sectionBottom - panel::headerTop);

        // A shallow moulded recess for each section, then its coloured header.
        g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.55f));
        g.fillRoundedRectangle (box, 4.0f * scale);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawRoundedRectangle (box.reduced (0.5f), 4.0f * scale, 1.0f);

        const auto header = scaled (section.x, panel::headerTop, section.width,
                                    panel::headerHeight);
        const auto tint = accentColour (section.accent);
        g.setColour (tint.withAlpha (0.16f));
        g.fillRoundedRectangle (header, 3.0f * scale);
        g.setColour (tint);
        g.fillRect (header.getX() + 2.0f * scale,
                    header.getBottom() - 2.0f * scale,
                    header.getWidth() - 4.0f * scale,
                    2.0f * scale);

        g.setColour (tint);
        g.setFont (panelFont (juce::jmax (8.0f, panel::headerPointSize * scale), true));
        g.drawText (section.name, header.toNearestInt(),
                    juce::Justification::centred);
    }

    // A separating rule above the utility strip, so the controls the modelled
    // instrument does not have read as an addition rather than as panel.
    g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.25f));
    g.fillRect (scaled (panel::panelMargin, panel::utilityTop - 5.0f,
                        panel::panelWidth() - 2.0f * panel::panelMargin, 1.0f));
}

void YouKnow106AudioProcessorEditor::timerCallback()
{
    display.refresh (audioProcessor);
    // The program can also move from the host's own menu, and the panel from an
    // incoming patch dump or a randomise. Polling is what keeps the bar honest
    // about both without the processor having to know an editor exists.
    refreshPresetBar();
}
