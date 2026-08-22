#include "PluginEditor.h"

namespace
{
namespace ids = ghostar::parameters;

// Ghostar's livery: near-black panel, bone silkscreen, spectral cyan pointers.
const juce::Colour panelBlack { 0xff141416 };
const juce::Colour sectionBlack { 0xff1b1b1e };
const juce::Colour silkscreen { 0xffe8e4da };
const juce::Colour silkscreenDim { 0xff9a978e };
const juce::Colour spectralCyan { 0xff7fd8d8 };
const juce::Colour controlBody { 0xff2a2a2e };
const juce::Colour controlRim { 0xff3c3c42 };

constexpr int editorWidth = 1240;
constexpr int editorHeight = 700;
} // namespace

GhostarLookAndFeel::GhostarLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, panelBlack);
    setColour(juce::Label::textColourId, silkscreen);
    setColour(juce::Slider::textBoxTextColourId, silkscreen);
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, controlBody);
    setColour(juce::ComboBox::textColourId, silkscreen);
    setColour(juce::ComboBox::outlineColourId, controlRim);
    setColour(juce::ComboBox::arrowColourId, spectralCyan);
    setColour(juce::PopupMenu::backgroundColourId, sectionBlack);
    setColour(juce::PopupMenu::textColourId, silkscreen);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              spectralCyan.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, silkscreen);
    setColour(juce::TextButton::buttonColourId, controlBody);
    setColour(juce::TextButton::textColourOffId, silkscreen);
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
              juce::Colour { 0xffdedbd2 });
    setColour(juce::MidiKeyboardComponent::blackNoteColourId,
              juce::Colour { 0xff202024 });
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
              juce::Colour { 0xff3a3a3e });
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
              spectralCyan.withAlpha(0.25f));
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
              spectralCyan.withAlpha(0.55f));
}

void GhostarLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                        int width, int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider&)
{
    const auto bounds =
        juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Conical black cap with a light grey top, as the hardware knobs wear.
    g.setColour(controlRim);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f,
                  radius * 2.0f);
    g.setColour(controlBody);
    const float capRadius = radius * 0.82f;
    g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f,
                  capRadius * 2.0f);
    g.setColour(juce::Colour { 0xff4a4a50 });
    const float topRadius = radius * 0.45f;
    g.fillEllipse(centre.x - topRadius, centre.y - topRadius, topRadius * 2.0f,
                  topRadius * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle(-1.6f, -radius * 0.95f, 3.2f, radius * 0.6f,
                                1.4f);
    g.setColour(spectralCyan);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(
                            centre.x, centre.y));
}

void GhostarLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
                                        int width, int height, float sliderPos,
                                        float, float,
                                        juce::Slider::SliderStyle style,
                                        juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                         0.0f, 0.0f, style, slider);
        return;
    }

    const auto track = juce::Rectangle<float>(
        static_cast<float>(x) + static_cast<float>(width) / 2.0f - 1.5f,
        static_cast<float>(y), 3.0f, static_cast<float>(height));
    g.setColour(juce::Colour { 0xff0c0c0e });
    g.fillRoundedRectangle(track, 1.5f);
    g.setColour(controlRim);
    g.drawRoundedRectangle(track, 1.5f, 1.0f);

    // The black cap with its pale index stripe.
    const float capWidth = juce::jmin(26.0f, static_cast<float>(width));
    const juce::Rectangle<float> cap { track.getCentreX() - capWidth / 2.0f,
                                       sliderPos - 8.0f, capWidth, 16.0f };
    g.setColour(controlBody);
    g.fillRoundedRectangle(cap, 3.0f);
    g.setColour(controlRim);
    g.drawRoundedRectangle(cap, 3.0f, 1.0f);
    g.setColour(spectralCyan);
    g.fillRect(cap.reduced(4.0f, 6.5f));
}

void GhostarLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    const auto rocker =
        bounds.removeFromLeft(30.0f).withSizeKeepingCentre(24.0f, 15.0f);

    g.setColour(shouldDrawButtonAsHighlighted ? controlRim : controlBody);
    g.fillRoundedRectangle(rocker, 3.0f);
    g.setColour(controlRim);
    g.drawRoundedRectangle(rocker, 3.0f, 1.0f);

    // The large grey rocker: the lit half shows which way it is thrown.
    auto lit = rocker.reduced(2.5f);
    lit = button.getToggleState() ? lit.removeFromTop(lit.getHeight() / 2.0f)
                                  : lit.removeFromBottom(lit.getHeight() / 2.0f);
    g.setColour(button.getToggleState() ? spectralCyan
                                        : juce::Colour { 0xff55555c });
    g.fillRoundedRectangle(lit, 2.0f);

    g.setColour(silkscreen);
    g.setFont(juce::FontOptions { 12.0f });
    g.drawText(button.getButtonText(),
               button.getLocalBounds().withTrimmedLeft(32),
               juce::Justification::centredLeft);
}

GhostarAudioProcessorEditor::GhostarAudioProcessorEditor(GhostarAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);

    addKnob(tune, ids::tune, "TUNE");
    addSelector(octave, ids::octave, "OCTAVE");
    addSelector(oscAWaveform, ids::oscAWaveform, "WAVEFORM");
    addRocker(sync, ids::sync, "SYNC");
    addSelector(oscBWaveform, ids::oscBWaveform, "WAVEFORM");
    addSelector(oscBRange, ids::oscBRange, "OCTAVE/RANGE");
    addKnob(interval, ids::interval, "INTERVAL");
    addSelector(trigger, ids::trigger, "TRIGGER");
    addRocker(gateKbd, ids::gateKbd, "KBD");
    addRocker(gateX, ids::gateX, "X");
    addRocker(gateYExt, ids::gateYExt, "Y/EXT");
    addSelector(arpeggiator, ids::arpeggiator, "ARPEGGIATOR");
    addSelector(modSource, ids::modSource, "MOD SOURCE");
    addKnob(lfoRate, ids::lfoRate, "LFO/S+H RATE");
    addSelector(shaperMode, ids::shaperMode, "MODE");
    addKnob(shaperShape, ids::shaperShape, "SHAPE");
    addKnob(shaperRate, ids::shaperRate, "RATE");
    addSelector(modXTo, ids::modXTo, "MOD X TO:");
    addRocker(shapeXWithY, ids::shapeXWithY, "SHAPE X WITH Y");
    addSelector(shaperYTo, ids::shaperYTo, "SHAPER Y TO:");
    addKnob(masterVolume, ids::masterVolume, "MASTER VOLUME");
    addKnob(brightness, ids::brightness, "BRIGHTNESS");
    addFader(shaperPathA, ids::shaperPathA, "A");
    addFader(shaperPathB, ids::shaperPathB, "B");
    addFader(shaperPathRing, ids::shaperPathRing, "RING");
    addFader(shaperPathNoise, ids::shaperPathNoise, "NOISE");
    addFader(filterPathA, ids::filterPathA, "A");
    addFader(filterPathB, ids::filterPathB, "B");
    addFader(filterPathNoise, ids::filterPathNoise, "NOISE");
    addKnob(cutoff, ids::cutoff, "MASTER");
    addKnob(lowerOnly, ids::lowerOnly, "LOWER ONLY");
    addSelector(upperResonance, ids::upperResonance, "RESONANCE");
    addKnob(resonance, ids::resonance, "RESONANCE");
    addSelector(slope, ids::slope, "SLOPE");
    addKnob(kbAmount, ids::kbAmount, "KB AMOUNT");
    addSelector(lowerMode, ids::lowerMode, "SLOPE/MODE");
    addSelector(tracking, ids::tracking, "TRACKING");
    addKnob(filterEnvAmount, ids::filterEnvAmount, "AMOUNT");
    addFader(filterAttack, ids::filterAttack, "A");
    addFader(filterDecay, ids::filterDecay, "D");
    addFader(filterSustain, ids::filterSustain, "S");
    addFader(filterRelease, ids::filterRelease, "R");
    addRocker(vcaBypass, ids::vcaBypass, "VCA BYPASS");
    addFader(loudnessAttack, ids::loudnessAttack, "A");
    addFader(loudnessDecay, ids::loudnessDecay, "D");
    addFader(loudnessSustain, ids::loudnessSustain, "S");
    addFader(loudnessRelease, ids::loudnessRelease, "R");
    addKnob(glide, ids::glide, "GLIDE");
    addSelector(glideMode, ids::glideMode, "GLIDE MODE");
    addKnob(xWheel, ids::xWheel, "MOD X");
    addKnob(yWheel, ids::yWheel, "SHAPER Y");
    addRocker(splitPaths, ids::splitPaths, "SPLIT");

    // The spring-loaded bend wheel: vertical travel, snapping back to centre.
    pitchWheel.setSliderStyle(juce::Slider::LinearVertical);
    pitchWheel.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    pitchWheel.setRange(-1.0, 1.0, 0.0);
    pitchWheel.setValue(0.0, juce::dontSendNotification);
    pitchWheel.setDoubleClickReturnValue(true, 0.0);
    // A scroll is not a drag, so onDragEnd would never spring the wheel
    // back and the bend would latch; the spring only answers to dragging.
    pitchWheel.setScrollWheelEnabled(false);
    pitchWheel.onValueChange = [this] {
        processor.setUiPitchBend(static_cast<float>(pitchWheel.getValue()));
    };
    pitchWheel.onDragEnd = [this] {
        pitchWheel.setValue(0.0, juce::sendNotificationSync);
    };
    addAndMakeVisible(pitchWheel);

    panicButton.onClick = [this] { processor.requestPanic(); };
    addAndMakeVisible(panicButton);

    wordmark.setText("ghostar", juce::dontSendNotification);
    wordmark.setFont(juce::FontOptions { 34.0f, juce::Font::bold });
    wordmark.setColour(juce::Label::textColourId, spectralCyan);
    wordmark.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(wordmark);

    keyboard.setAvailableRange(48, 84); // the hardware's 37 keys, C to C
    keyboard.setOctaveForMiddleC(4);
    addAndMakeVisible(keyboard);

    setSize(editorWidth, editorHeight);
}

GhostarAudioProcessorEditor::~GhostarAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void GhostarAudioProcessorEditor::addKnob(Knob& knob, const char* parameterId,
                                        const juce::String& text)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(knob.slider);
    knob.label.setText(text, juce::dontSendNotification);
    knob.label.setFont(juce::FontOptions { 11.0f });
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, silkscreen);
    addAndMakeVisible(knob.label);
    knob.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, parameterId, knob.slider);
}

void GhostarAudioProcessorEditor::addFader(Fader& fader, const char* parameterId,
                                         const juce::String& text)
{
    fader.slider.setSliderStyle(juce::Slider::LinearVertical);
    fader.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(fader.slider);
    fader.label.setText(text, juce::dontSendNotification);
    fader.label.setFont(juce::FontOptions { 10.0f });
    fader.label.setJustificationType(juce::Justification::centred);
    fader.label.setColour(juce::Label::textColourId, silkscreen);
    addAndMakeVisible(fader.label);
    fader.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, parameterId, fader.slider);
}

void GhostarAudioProcessorEditor::addRocker(Rocker& rocker,
                                          const char* parameterId,
                                          const juce::String& text)
{
    rocker.button.setButtonText(text);
    addAndMakeVisible(rocker.button);
    rocker.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, parameterId, rocker.button);
}

void GhostarAudioProcessorEditor::addSelector(Selector& selector,
                                            const char* parameterId,
                                            const juce::String& text)
{
    addAndMakeVisible(selector.box);
    selector.label.setText(text, juce::dontSendNotification);
    selector.label.setFont(juce::FontOptions { 11.0f });
    selector.label.setJustificationType(juce::Justification::centred);
    selector.label.setColour(juce::Label::textColourId, silkscreen);
    addAndMakeVisible(selector.label);
    // The attachment populates the box from the parameter's choice labels.
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(
            processor.parameters.getParameter(parameterId)))
    {
        selector.box.addItemList(parameter->choices, 1);
        selector.box.setSelectedItemIndex(parameter->getIndex(),
                                          juce::dontSendNotification);
    }
    selector.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, parameterId, selector.box);
}

void GhostarAudioProcessorEditor::layoutKnob(Knob& knob,
                                           juce::Rectangle<int> area)
{
    knob.label.setBounds(area.removeFromBottom(14));
    knob.slider.setBounds(area);
}

void GhostarAudioProcessorEditor::layoutFader(Fader& fader,
                                            juce::Rectangle<int> area)
{
    fader.label.setBounds(area.removeFromBottom(14));
    fader.slider.setBounds(area);
}

void GhostarAudioProcessorEditor::layoutSelector(Selector& selector,
                                               juce::Rectangle<int> area)
{
    selector.label.setBounds(area.removeFromTop(14));
    selector.box.setBounds(area.removeFromTop(24));
}

void GhostarAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(panelBlack);

    for (const auto& section : sections)
    {
        const auto frame = section.bounds.toFloat();
        g.setColour(sectionBlack);
        g.fillRoundedRectangle(frame, 6.0f);
        g.setColour(controlRim);
        g.drawRoundedRectangle(frame.reduced(0.5f), 6.0f, 1.0f);

        g.setColour(silkscreenDim);
        g.setFont(juce::FontOptions { 12.0f, juce::Font::bold });
        g.drawText(section.title,
                   section.bounds.withHeight(18).reduced(8, 2),
                   juce::Justification::centredLeft);
    }
}

void GhostarAudioProcessorEditor::resized()
{
    sections.clear();
    auto area = getLocalBounds().reduced(10);

    const auto addSection = [this](juce::Rectangle<int>& row, int width,
                                   const juce::String& title) {
        auto bounds = row.removeFromLeft(width);
        row.removeFromLeft(6);
        sections.push_back({ title, bounds });
        return bounds.reduced(8, 4).withTrimmedTop(16);
    };

    // ---- Row 1: sources, timing and modulation --------------------------
    auto row1 = area.removeFromTop(190);
    {
        auto master = addSection(row1, 100, "MASTER");
        layoutKnob(tune, master.removeFromTop(78));
        layoutSelector(octave, master.removeFromTop(44));

        auto oscA = addSection(row1, 120, "OSCILLATOR A");
        layoutSelector(oscAWaveform, oscA.removeFromTop(44));
        sync.button.setBounds(oscA.removeFromTop(30));

        auto oscB = addSection(row1, 184, "OSCILLATOR B");
        auto oscBTop = oscB.removeFromTop(44);
        layoutSelector(oscBWaveform, oscBTop.removeFromLeft(84));
        layoutSelector(oscBRange, oscBTop);
        layoutKnob(interval, oscB.removeFromTop(78).reduced(38, 0));

        auto triggerSection = addSection(row1, 96, "TRIGGER");
        layoutSelector(trigger, triggerSection.removeFromTop(44));

        auto gates = addSection(row1, 96, "GATE SELECT");
        gateKbd.button.setBounds(gates.removeFromTop(26));
        gateX.button.setBounds(gates.removeFromTop(26));
        gateYExt.button.setBounds(gates.removeFromTop(26));

        auto modX = addSection(row1, 198, "MOD X");
        auto modXTop = modX.removeFromTop(44);
        layoutSelector(arpeggiator, modXTop.removeFromLeft(80));
        layoutSelector(modSource, modXTop);
        layoutKnob(lfoRate, modX.removeFromTop(78).reduced(48, 0));

        auto shaper = addSection(row1, 182, "SHAPER Y");
        layoutSelector(shaperMode, shaper.removeFromTop(44));
        auto shaperKnobs = shaper.removeFromTop(78);
        layoutKnob(shaperShape,
                   shaperKnobs.removeFromLeft(shaperKnobs.getWidth() / 2));
        layoutKnob(shaperRate, shaperKnobs);

        auto destinations = addSection(row1, 204, "WHEEL DESTINATIONS");
        layoutSelector(modXTo, destinations.removeFromTop(44));
        shapeXWithY.button.setBounds(destinations.removeFromTop(24));
        layoutSelector(shaperYTo, destinations.removeFromTop(44));
    }
    area.removeFromTop(8);

    // ---- Row 2: mixer, filters and envelopes ----------------------------
    auto row2 = area.removeFromTop(250);
    {
        auto mixer = addSection(row2, 352, "AUDIO MIXER");
        auto mixerKnobs = mixer.removeFromLeft(92);
        layoutKnob(masterVolume, mixerKnobs.removeFromTop(96));
        layoutKnob(brightness, mixerKnobs.removeFromTop(96));
        auto faderRow = mixer;
        const int faderWidth = faderRow.getWidth() / 7;
        layoutFader(shaperPathA, faderRow.removeFromLeft(faderWidth));
        layoutFader(shaperPathB, faderRow.removeFromLeft(faderWidth));
        layoutFader(shaperPathRing, faderRow.removeFromLeft(faderWidth));
        layoutFader(shaperPathNoise, faderRow.removeFromLeft(faderWidth));
        layoutFader(filterPathA, faderRow.removeFromLeft(faderWidth));
        layoutFader(filterPathB, faderRow.removeFromLeft(faderWidth));
        layoutFader(filterPathNoise, faderRow);

        auto filters = addSection(row2, 376, "UPPER FILTER U / LOWER FILTER L");
        auto upperRow = filters.removeFromTop(112);
        const int filterCell = upperRow.getWidth() / 4;
        layoutKnob(cutoff, upperRow.removeFromLeft(filterCell));
        layoutSelector(upperResonance,
                       upperRow.removeFromLeft(filterCell).withTrimmedTop(24));
        layoutSelector(slope,
                       upperRow.removeFromLeft(filterCell).withTrimmedTop(24));
        layoutKnob(kbAmount, upperRow);
        auto lowerRow = filters;
        layoutKnob(lowerOnly, lowerRow.removeFromLeft(filterCell));
        layoutKnob(resonance, lowerRow.removeFromLeft(filterCell));
        layoutSelector(lowerMode,
                       lowerRow.removeFromLeft(filterCell).withTrimmedTop(24));
        layoutSelector(tracking, lowerRow.withTrimmedTop(24));

        auto filterEnv = addSection(row2, 234, "FILTER ENVELOPE");
        layoutKnob(filterEnvAmount, filterEnv.removeFromLeft(84)
                                        .withSizeKeepingCentre(84, 96));
        const int envFader = filterEnv.getWidth() / 4;
        layoutFader(filterAttack, filterEnv.removeFromLeft(envFader));
        layoutFader(filterDecay, filterEnv.removeFromLeft(envFader));
        layoutFader(filterSustain, filterEnv.removeFromLeft(envFader));
        layoutFader(filterRelease, filterEnv);

        auto loudness = addSection(row2, 234, "LOUDNESS ENVELOPE");
        auto vcaColumn = loudness.removeFromLeft(100);
        vcaBypass.button.setBounds(vcaColumn.withSizeKeepingCentre(100, 26));
        const int loudFader = loudness.getWidth() / 4;
        layoutFader(loudnessAttack, loudness.removeFromLeft(loudFader));
        layoutFader(loudnessDecay, loudness.removeFromLeft(loudFader));
        layoutFader(loudnessSustain, loudness.removeFromLeft(loudFader));
        layoutFader(loudnessRelease, loudness);
    }
    area.removeFromTop(8);

    // ---- Row 3: the performance strip and keyboard ----------------------
    auto row3 = area;
    {
        auto performance = addSection(row3, 396, "PERFORMANCE");
        auto glideArea = performance.removeFromLeft(88);
        layoutKnob(glide, glideArea.removeFromTop(84));
        layoutSelector(glideMode, glideArea.removeFromTop(44));

        auto wheels = performance.removeFromLeft(210);
        auto bendArea = wheels.removeFromLeft(64);
        pitchWheel.setBounds(bendArea.reduced(18, 6).withTrimmedBottom(14));
        auto xArea = wheels.removeFromLeft(72);
        layoutKnob(xWheel, xArea.removeFromTop(84));
        layoutKnob(yWheel, wheels.removeFromTop(84));

        splitPaths.button.setBounds(performance.removeFromTop(26));
        panicButton.setBounds(
            performance.removeFromTop(34).reduced(0, 4).withWidth(84));

        keyboard.setKeyWidth(
            static_cast<float>(row3.getWidth() - 130) / 22.0f);
        keyboard.setBounds(row3.withTrimmedRight(130).reduced(0, 6));
        wordmark.setBounds(row3.removeFromRight(124).withTrimmedBottom(8));
    }
}
