#include "PluginEditor.h"

#include "DSP/GhostarPresets.h"

namespace
{
namespace ids = ghostar::parameters;

// Ghostar's livery: a charcoal panel with grey knob caps, their markings
// in black, and the silkscreen in white — the monochrome scheme a machined
// front panel of the period actually wears. Nothing is coloured for its own
// sake: value is shown by a black arc against a pale track, and the only
// thing that lights is a state you need to see (a thrown rocker, the gate).
const juce::Colour panelCharcoal { 0xff3a3a3c };
const juce::Colour sectionCharcoal { 0xff333335 };
const juce::Colour accentCharcoal { 0xff3f3f42 };
const juce::Colour headerCharcoal { 0xff2b2b2d };
const juce::Colour silkscreen { 0xffffffff };
const juce::Colour silkscreenDim { 0xffbdbdbd };
const juce::Colour knobFace { 0xff9c9c9e };
const juce::Colour knobFaceTop { 0xffb0b0b2 };
const juce::Colour knobRim { 0xff77777a };
const juce::Colour markingBlack { 0xff141416 };
const juce::Colour controlBody { 0xff4a4a4d };
const juce::Colour controlRim { 0xff5c5c60 };
const juce::Colour trackPale { 0xff8b8b8e };
const juce::Colour hairline { 0xff4c4c50 };
const juce::Colour lampLit { 0xfff2f2f2 };
const juce::Colour lampDark { 0xff2a2a2c };

constexpr int editorWidth = 1280;
constexpr int editorHeight = 818;
constexpr int margin = 12;
constexpr int gap = 8;

// A control's caption shows its value for about this long after it stops
// moving, then returns to naming itself.
constexpr int readoutHoldTicks = 12;   // ~0.8 s at the editor's 15 Hz

// The rotary travel the panel's pointers sweep: the usual three-quarter turn.
constexpr float rotaryStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float rotaryEnd = juce::MathConstants<float>::pi * 2.75f;

juce::String travelText(double normalised)
{
    return juce::String(normalised * 10.0, 1);
}
} // namespace

GhostarLookAndFeel::GhostarLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, panelCharcoal);
    setColour(juce::Label::textColourId, silkscreen);
    setColour(juce::Slider::textBoxTextColourId, silkscreen);
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, controlBody);
    setColour(juce::ComboBox::textColourId, silkscreen);
    setColour(juce::ComboBox::outlineColourId, controlRim);
    setColour(juce::ComboBox::arrowColourId, silkscreen);
    setColour(juce::PopupMenu::backgroundColourId, sectionCharcoal);
    setColour(juce::PopupMenu::textColourId, silkscreen);
    setColour(juce::PopupMenu::headerTextColourId, silkscreenDim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              juce::Colour { 0xff5a5a5e });
    setColour(juce::PopupMenu::highlightedTextColourId, silkscreen);
    setColour(juce::TextButton::buttonColourId, controlBody);
    setColour(juce::TextButton::textColourOffId, silkscreen);
    setColour(juce::TextButton::textColourOnId, silkscreen);
    setColour(juce::TooltipWindow::backgroundColourId,
              juce::Colour { 0xff2b2b2d });
    setColour(juce::TooltipWindow::textColourId, silkscreen);
    setColour(juce::TooltipWindow::outlineColourId, controlRim);
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
              juce::Colour { 0xffe9e9e6 });
    setColour(juce::MidiKeyboardComponent::blackNoteColourId,
              juce::Colour { 0xff141416 });
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
              juce::Colour { 0xff8a8a88 });
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
              juce::Colour { 0x30000000 });
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
              juce::Colour { 0x88000000 });
    setColour(juce::MidiKeyboardComponent::textLabelColourId,
              juce::Colour { 0xff55555c });
}

void GhostarLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                        int width, int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle,
                                        juce::Slider& slider)
{
    const auto bounds =
        juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // The travel arc, outside the cap: a dim track for the whole sweep, lit
    // from the start of travel to where the pointer stands. A bipolar
    // control lights from its centre instead, because that is where its
    // zero is and the silkscreen circles it.
    const float arcRadius = radius * 0.94f;
    const float arcThickness = 2.6f;
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(trackPale);
    g.strokePath(track, juce::PathStrokeType(arcThickness,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    const bool bipolar = slider.getProperties()["bipolar"];
    const float originAngle = bipolar
        ? (rotaryStartAngle + rotaryEndAngle) * 0.5f
        : rotaryStartAngle;
    if (std::abs(angle - originAngle) > 0.001f)
    {
        juce::Path lit;
        lit.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                          juce::jmin(originAngle, angle),
                          juce::jmax(originAngle, angle), true);
        g.setColour(markingBlack);
        g.strokePath(lit, juce::PathStrokeType(arcThickness,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Conical black cap with a lighter top, as the hardware knobs wear.
    const float capOuter = radius * 0.80f;
    g.setColour(knobRim);
    g.fillEllipse(centre.x - capOuter, centre.y - capOuter, capOuter * 2.0f,
                  capOuter * 2.0f);
    const float capRadius = radius * 0.70f;
    g.setColour(knobFace);
    g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f,
                  capRadius * 2.0f);
    g.setColour(knobFaceTop);
    const float topRadius = radius * 0.36f;
    g.fillEllipse(centre.x - topRadius, centre.y - topRadius, topRadius * 2.0f,
                  topRadius * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle(-1.8f, -capRadius * 0.96f, 3.6f,
                                capRadius * 0.66f, 1.8f);
    g.setColour(markingBlack);
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
        static_cast<float>(x) + static_cast<float>(width) / 2.0f - 2.0f,
        static_cast<float>(y), 4.0f, static_cast<float>(height));
    g.setColour(trackPale);
    g.fillRoundedRectangle(track, 2.0f);
    g.setColour(knobRim);
    g.drawRoundedRectangle(track.reduced(0.5f), 2.0f, 1.0f);

    // The travelled part of the track lights, so a bank of faders reads as
    // a shape at a glance rather than as eight identical caps.
    auto lit = track;
    lit.setTop(sliderPos);
    g.setColour(markingBlack);
    g.fillRoundedRectangle(lit, 2.0f);

    // The black cap with its pale index stripe.
    const float capWidth = juce::jmin(24.0f, static_cast<float>(width));
    const juce::Rectangle<float> cap { track.getCentreX() - capWidth / 2.0f,
                                       sliderPos - 8.0f, capWidth, 16.0f };
    g.setColour(knobFace);
    g.fillRoundedRectangle(cap, 3.0f);
    g.setColour(knobRim);
    g.drawRoundedRectangle(cap.reduced(0.5f), 3.0f, 1.0f);
    g.setColour(markingBlack);
    g.fillRect(cap.getX() + 4.0f, cap.getCentreY() - 1.0f,
               cap.getWidth() - 8.0f, 2.0f);
}

void GhostarLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    const auto rocker =
        bounds.removeFromLeft(30.0f).withSizeKeepingCentre(24.0f, 15.0f);

    g.setColour(markingBlack);
    g.fillRoundedRectangle(rocker, 3.0f);
    g.setColour(shouldDrawButtonAsHighlighted ? silkscreen : controlRim);
    g.drawRoundedRectangle(rocker.reduced(0.5f), 3.0f, 1.0f);

    // The large grey rocker: the lit half shows which way it is thrown.
    auto lit = rocker.reduced(2.5f);
    lit = button.getToggleState() ? lit.removeFromTop(lit.getHeight() / 2.0f)
                                  : lit.removeFromBottom(lit.getHeight() / 2.0f);
    // The thrown half is pale, the resting half dark: which way the rocker
    // sits is legible without colour.
    g.setColour(button.getToggleState() ? knobFaceTop
                                        : juce::Colour { 0xff3d3d40 });
    g.fillRoundedRectangle(lit, 2.0f);

    g.setColour(button.getToggleState() ? silkscreen : silkscreenDim);
    g.setFont(juce::FontOptions { 11.5f });
    g.drawText(button.getButtonText(),
               button.getLocalBounds().withTrimmedLeft(33),
               juce::Justification::centredLeft);
}

void GhostarLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                    bool, int, int, int, int,
                                    juce::ComboBox& box)
{
    const auto bounds =
        juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    g.setColour(controlBody);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(box.isMouseOver() ? silkscreen : controlRim);
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    juce::Path arrow;
    const float cx = bounds.getRight() - 12.0f;
    const float cy = bounds.getCentreY();
    arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 3.0f);
    arrow.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(silkscreen);
    g.strokePath(arrow, juce::PathStrokeType(1.6f));
}

juce::Font GhostarLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font { juce::FontOptions { 12.5f } };
}

void GhostarLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button, const juce::Colour&,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(shouldDrawButtonAsDown ? knobRim : controlBody);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(shouldDrawButtonAsHighlighted ? silkscreen : controlRim);
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

void PanelKeyboard::drawBlackNote(int, juce::Graphics& g,
                                  juce::Rectangle<float> area, bool isDown,
                                  bool isOver, juce::Colour noteFillColour)
{
    auto colour = noteFillColour;
    if (isDown)
        colour = colour.overlaidWith(findColour(keyDownOverlayColourId));
    else if (isOver)
        colour = colour.overlaidWith(
            findColour(mouseOverKeyOverlayColourId));

    g.setColour(colour);
    g.fillRect(area);
    // A hairline down the sides only, so adjacent sharps stay separable
    // without the face lifting off black.
    g.setColour(juce::Colour { 0xff2e2e30 });
    g.drawRect(area, 1.0f);
}

GhostarAudioProcessorEditor::GhostarAudioProcessorEditor(
    GhostarAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);

    addKnob(tune, ids::tune, "TUNE",
            "Master tune: a minor third either side of centre.");
    tune.slider.getProperties().set("bipolar", true);
    addSelector(octave, ids::octave, "OCTAVE",
                "Master footage. At 8' the keyboard's second C sounds middle "
                "C.");
    addSelector(oscAWaveform, ids::oscAWaveform, "WAVEFORM",
                "Osc A's waveform. The rectangles are the panel's own duty "
                "cycles: 50, 30, 15 and 6 per cent.");
    addRocker(sync, ids::sync, "SYNC",
              "Hard sync: Osc A resets Osc B, so B follows A's pitch and its "
              "own tuning becomes timbre. Tune B above A to hear it.");
    addSelector(oscBWaveform, ids::oscBWaveform, "WAVEFORM",
                "Osc B's waveform. Its rectangles are narrower than A's: 40, "
                "20, 10 and 3 per cent.");
    addSelector(oscBRange, ids::oscBRange, "OCTAVE/RANGE",
                "Osc B's octave against A, or a drone range. BASS and WIDE "
                "disconnect B from the keyboard entirely and hand its pitch "
                "to INTERVAL.");
    addKnob(interval, ids::interval, "INTERVAL",
            "Osc B's detune: a perfect fifth either side of centre. Small "
            "offsets are the warmth the manual asks for. In BASS or WIDE it "
            "becomes B's drone pitch instead.");
    interval.slider.getProperties().set("bipolar", true);
    addSelector(trigger, ids::trigger, "TRIGGER",
                "MULTIPLE re-gates on every new key; SINGLE holds the gate "
                "while any key is down and re-gates only after all are "
                "released.");
    addRocker(gateKbd, ids::gateKbd, "KBD",
              "Gate the envelopes from the keyboard.");
    addRocker(gateX, ids::gateX, "X",
              "Gate the envelopes from the LFO square: auto-repeat at the "
              "MOD X rate, one gate per arpeggiator step.");
    addRocker(gateYExt, ids::gateYExt, "Y/EXT",
              "Gate the envelopes from the Shaper's own comparator. At least "
              "one gate source must be on or the envelopes never run.");
    addSelector(arpeggiator, ids::arpeggiator, "ARPEGGIATOR",
                "Scans held keys bottom to top on the LFO clock. RIPPLE is "
                "the plain sequence; ARPEGGIO repeats it up and down an "
                "octave; LEAP cycles the octave per note.");
    addSelector(modSource, ids::modSource, "MOD SOURCE",
                "What rides the MOD X bus. OSC B is the selected Osc B "
                "waveform itself — audio-rate modulation when B is in WIDE.");
    addKnob(lfoRate, ids::lfoRate, "LFO/S+H RATE",
            "The LFO rate, and with it the sample-and-hold clock and the "
            "arpeggiator's tempo. No effect on RED NOISE or OSC B.");
    addSelector(shaperMode, ids::shaperMode, "MODE",
                "FREE runs the Shaper as an LFO; KBD HOLD rises and holds "
                "while gated; RESET restarts on every key; RUN always "
                "finishes its rise before accepting a new gate.");
    addKnob(shaperShape, ids::shaperShape, "SHAPE",
            "How the Shaper's period splits between rise and fall, without "
            "changing the period itself. Fully left is a fast rise and slow "
            "fall; fully right the reverse.");
    addKnob(shaperRate, ids::shaperRate, "RATE",
            "The Shaper's total period. In the envelope modes it is the "
            "whole rise-plus-fall time.");
    addSelector(modXTo, ids::modXTo, "MOD X TO:",
                "Where the MOD X wheel's signal lands. RWM is pulse-width "
                "modulation of the rectangular waveforms.");
    addRocker(shapeXWithY, ids::shapeXWithY, "SHAPE X WITH Y",
              "The Shaper envelopes the MOD X signal — vibrato that fades in "
              "rather than arriving whole.");
    addSelector(shaperYTo, ids::shaperYTo, "SHAPER Y TO:",
                "Where the SHAPER Y wheel's signal lands. At LFO RATE the "
                "wheel sets the fastest rate and the panel knob the "
                "slowest.");
    addKnob(masterVolume, ids::masterVolume, "MASTER VOLUME",
            "Output level.");
    addKnob(brightness, ids::brightness, "BRIGHTNESS",
            "A 6 dB/octave lowpass on the Shaper path only. Fully up is "
            "effectively open.");
    addFader(shaperPathA, ids::shaperPathA, "A",
             "Osc A into the Shaper path.");
    addFader(shaperPathB, ids::shaperPathB, "B",
             "Osc B into the Shaper path.");
    addFader(shaperPathRing, ids::shaperPathRing, "RING",
             "The ring modulator into the Shaper path — the only path it "
             "reaches. It multiplies the two triangles, whatever the "
             "waveform switches say.");
    addFader(shaperPathNoise, ids::shaperPathNoise, "NOISE",
             "Noise into the Shaper path.");
    addFader(filterPathA, ids::filterPathA, "A",
             "Osc A into the filter path.");
    addFader(filterPathB, ids::filterPathB, "B",
             "Osc B into the filter path.");
    addFader(filterPathNoise, ids::filterPathNoise, "NOISE",
             "Noise into the filter path.");
    addKnob(cutoff, ids::cutoff, "MASTER",
            "Both filters' cutoff, always — the Lower section moves with it "
            "and LOWER ONLY sets the offset.");
    addKnob(lowerOnly, ids::lowerOnly, "LOWER ONLY",
            "The Lower filter's cutoff relative to the Upper. The two "
            "coincide at 8; below that the Lower sits underneath.");
    addSelector(upperResonance, ids::upperResonance, "RESONANCE",
                "LOW fixes the Upper filter's Q at 0.5; VARIABLE hands it to "
                "the resonance knob alongside the Lower.");
    addKnob(resonance, ids::resonance, "RESONANCE",
            "The Lower filter's Q always, and the Upper's in VARIABLE. Gentle "
            "through the middle of the travel, then steep — and "
            "self-oscillating at maximum.");
    addSelector(slope, ids::slope, "SLOPE",
                "The Upper filter's slope: one section or two cascaded.");
    addKnob(kbAmount, ids::kbAmount, "KB AMOUNT",
            "How far the cutoff follows the keyboard — slightly over 100 per "
            "cent at full travel.");
    addSelector(lowerMode, ids::lowerMode, "SLOPE/MODE",
                "The Lower section. BAND-PASS is a parametric boost, not a "
                "band-pass; OVERDRIVE adds the diode clipper between the "
                "filters; HIGH PASS gives the double-peak register.");
    addSelector(tracking, ids::tracking, "TRACKING",
                "FORMANT freezes the Lower filter's peak — no keyboard, no "
                "envelope, no wheels — as a fixed vocal formant. DYNAMIC "
                "lets it move.");
    addKnob(filterEnvAmount, ids::filterEnvAmount, "AMOUNT",
            "How far the filter envelope moves the cutoff, up to two and a "
            "half octaves either way. Below centre the envelope inverts.");
    filterEnvAmount.slider.getProperties().set("bipolar", true);
    addFader(filterAttack, ids::filterAttack, "A", "Filter envelope attack.");
    addFader(filterDecay, ids::filterDecay, "D", "Filter envelope decay.");
    addFader(filterSustain, ids::filterSustain, "S",
             "Filter envelope sustain level.");
    addFader(filterRelease, ids::filterRelease, "R",
             "Filter envelope release.");
    addRocker(vcaBypass, ids::vcaBypass, "VCA BYPASS",
              "Holds the filter path's VCA fully open, so the path drones "
              "without waiting for a key.");
    addFader(loudnessAttack, ids::loudnessAttack, "A",
             "Loudness envelope attack.");
    addFader(loudnessDecay, ids::loudnessDecay, "D",
             "Loudness envelope decay.");
    addFader(loudnessSustain, ids::loudnessSustain, "S",
             "Loudness envelope sustain level.");
    addFader(loudnessRelease, ids::loudnessRelease, "R",
             "Loudness envelope release.");
    addKnob(glide, ids::glide, "GLIDE",
            "Portamento time between notes.");
    addSelector(glideMode, ids::glideMode, "GLIDE MODE",
                "AUTO glides only while more than one key is held, so legato "
                "slides and separate notes do not.");
    addKnob(xWheel, ids::xWheel, "MOD X",
            "The MOD X performance wheel. It attenuates toward zero, so a "
            "bipolar source keeps its symmetry.");
    addKnob(yWheel, ids::yWheel, "SHAPER Y",
            "The SHAPER Y performance wheel.");
    addRocker(splitPaths, ids::splitPaths, "SPLIT",
              "Sends the filter path left and the Shaper path right, as the "
              "hardware's two rear jacks do, instead of mixing them.");

    // The spring-loaded bend wheel: vertical travel, snapping back to centre.
    pitchWheel.setSliderStyle(juce::Slider::LinearVertical);
    pitchWheel.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    pitchWheel.setRange(-1.0, 1.0, 0.0);
    pitchWheel.setValue(0.0, juce::dontSendNotification);
    pitchWheel.setDoubleClickReturnValue(true, 0.0);
    pitchWheel.setTooltip("Pitch bend, spring-loaded: it springs back to "
                          "centre when released. Full travel is eight "
                          "semitones.");
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
    pitchWheelLabel.setText("BEND", juce::dontSendNotification);
    pitchWheelLabel.setFont(juce::FontOptions { 10.0f });
    pitchWheelLabel.setJustificationType(juce::Justification::centred);
    pitchWheelLabel.setColour(juce::Label::textColourId, silkscreenDim);
    addAndMakeVisible(pitchWheelLabel);

    panicButton.setTooltip("Stops every sounding voice at once, including "
                           "drones held open by VCA BYPASS.");
    panicButton.onClick = [this] { processor.requestPanic(); };
    addAndMakeVisible(panicButton);

    const auto addCaption = [this](juce::Label& label,
                                   const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::FontOptions { 9.5f, juce::Font::bold });
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, silkscreenDim);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
    };
    addCaption(shaperPathCaption, "SHAPER Y PATH");
    addCaption(filterPathCaption, "FILTER PATH");

    wordmark.setText("ghostar", juce::dontSendNotification);
    wordmark.setFont(juce::FontOptions { 30.0f, juce::Font::bold });
    wordmark.setColour(juce::Label::textColourId, silkscreen);
    wordmark.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(wordmark);

    subtitle.setText("monophonic dual-filter synthesizer",
                     juce::dontSendNotification);
    subtitle.setFont(juce::FontOptions { 11.0f });
    subtitle.setColour(juce::Label::textColourId, silkscreenDim);
    subtitle.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitle);

    previousProgram.setTooltip("Previous program.");
    previousProgram.onClick = [this] { stepProgram(-1); };
    addAndMakeVisible(previousProgram);
    nextProgram.setTooltip("Next program.");
    nextProgram.onClick = [this] { stepProgram(1); };
    addAndMakeVisible(nextProgram);
    programName.setTooltip("The selected program. Click to browse the bank: "
                           "the manual's Sound Charts, then Ghostar's own "
                           "performance programs.");
    programName.onClick = [this] { showProgramMenu(); };
    addAndMakeVisible(programName);
    programBank.setFont(juce::FontOptions { 10.0f });
    programBank.setJustificationType(juce::Justification::centred);
    programBank.setColour(juce::Label::textColourId, silkscreenDim);
    addAndMakeVisible(programBank);

    keyboard.setAvailableRange(48, 84); // the hardware's 37 keys, C to C
    keyboard.setOctaveForMiddleC(4);
    addAndMakeVisible(keyboard);

    refreshProgramDisplay();
    // Every attachment above fired its slider's callback as it took the
    // parameter's standing value; from here a callback means a human.
    wiringUp = false;
    startTimerHz(15);
    setSize(editorWidth, editorHeight);
}

GhostarAudioProcessorEditor::~GhostarAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void GhostarAudioProcessorEditor::addKnob(Knob& knob, const char* parameterId,
                                        const juce::String& text,
                                        const juce::String& tooltip)
{
    knob.name = text;
    knob.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.slider.setRotaryParameters(rotaryStart, rotaryEnd, true);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    knob.slider.setTooltip(tooltip);
    knob.slider.onValueChange = [this, &knob] {
        if (wiringUp)
            return;
        knob.readoutTicks = readoutHoldTicks;
        knob.label.setText(travelText(knob.slider.getValue()),
                           juce::dontSendNotification);
        knob.label.setColour(juce::Label::textColourId, silkscreen);
    };
    addAndMakeVisible(knob.slider);
    knob.label.setText(text, juce::dontSendNotification);
    knob.label.setFont(juce::FontOptions { 10.5f });
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, silkscreen);
    knob.label.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(knob.label);
    knob.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, parameterId, knob.slider);
}

void GhostarAudioProcessorEditor::addFader(Fader& fader,
                                          const char* parameterId,
                                          const juce::String& text,
                                          const juce::String& tooltip)
{
    fader.name = text;
    fader.slider.setSliderStyle(juce::Slider::LinearVertical);
    fader.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    fader.slider.setTooltip(tooltip);
    fader.slider.onValueChange = [this, &fader] {
        if (wiringUp)
            return;
        fader.readoutTicks = readoutHoldTicks;
        fader.label.setText(travelText(fader.slider.getValue()),
                            juce::dontSendNotification);
        fader.label.setColour(juce::Label::textColourId, silkscreen);
    };
    addAndMakeVisible(fader.slider);
    fader.label.setText(text, juce::dontSendNotification);
    fader.label.setFont(juce::FontOptions { 10.0f });
    fader.label.setJustificationType(juce::Justification::centred);
    fader.label.setColour(juce::Label::textColourId, silkscreen);
    fader.label.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(fader.label);
    fader.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, parameterId, fader.slider);
}

void GhostarAudioProcessorEditor::addRocker(Rocker& rocker,
                                          const char* parameterId,
                                          const juce::String& text,
                                          const juce::String& tooltip)
{
    rocker.button.setButtonText(text);
    rocker.button.setTooltip(tooltip);
    addAndMakeVisible(rocker.button);
    rocker.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, parameterId, rocker.button);
}

void GhostarAudioProcessorEditor::addSelector(Selector& selector,
                                            const char* parameterId,
                                            const juce::String& text,
                                            const juce::String& tooltip)
{
    selector.box.setTooltip(tooltip);
    addAndMakeVisible(selector.box);
    selector.label.setText(text, juce::dontSendNotification);
    selector.label.setFont(juce::FontOptions { 10.5f });
    selector.label.setJustificationType(juce::Justification::centred);
    selector.label.setColour(juce::Label::textColourId, silkscreen);
    selector.label.setInterceptsMouseClicks(false, false);
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

void GhostarAudioProcessorEditor::timerCallback()
{
    // A caption that has been showing a value for long enough goes back to
    // naming its control.
    const auto revert = [](auto& control) {
        if (control.readoutTicks > 0 && --control.readoutTicks == 0)
        {
            control.label.setText(control.name, juce::dontSendNotification);
            control.label.setColour(juce::Label::textColourId, silkscreen);
        }
    };
    for (Knob* knob : { &tune, &interval, &lfoRate, &shaperShape, &shaperRate,
                        &masterVolume, &brightness, &cutoff, &lowerOnly,
                        &resonance, &kbAmount, &filterEnvAmount, &glide,
                        &xWheel, &yWheel })
        revert(*knob);
    for (Fader* fader : { &shaperPathA, &shaperPathB, &shaperPathRing,
                          &shaperPathNoise, &filterPathA, &filterPathB,
                          &filterPathNoise, &filterAttack, &filterDecay,
                          &filterSustain, &filterRelease, &loudnessAttack,
                          &loudnessDecay, &loudnessSustain, &loudnessRelease })
        revert(*fader);

    // A host can change the program behind the editor's back.
    if (processor.getCurrentProgram() != shownProgram)
        refreshProgramDisplay();

    if (processor.isGateOpenForDisplay() != gateLampLit)
    {
        gateLampLit = !gateLampLit;
        repaint(gateLampBounds.expanded(4));
    }
}

void GhostarAudioProcessorEditor::refreshProgramDisplay()
{
    shownProgram = processor.getCurrentProgram();
    programName.setButtonText(processor.getProgramName(shownProgram));
    const bool isProgram =
        ghostar::factoryPresetBank(shownProgram)
        == ghostar::PresetBank::Programs;
    programBank.setText(isProgram ? "GHOSTAR PROGRAM" : "SOUND CHART",
                        juce::dontSendNotification);
}

void GhostarAudioProcessorEditor::stepProgram(int delta)
{
    const int count = processor.getNumPrograms();
    if (count <= 0)
        return;
    const int next = (processor.getCurrentProgram() + delta + count) % count;
    processor.setCurrentProgram(next);
    refreshProgramDisplay();
}

void GhostarAudioProcessorEditor::showProgramMenu()
{
    juce::PopupMenu menu;
    juce::PopupMenu charts;
    juce::PopupMenu programs;
    const int current = processor.getCurrentProgram();

    for (int index = 0; index < processor.getNumPrograms(); ++index)
    {
        // The description rides in the item text, so the browser says what
        // each program does rather than only what it is called.
        const juce::String text =
            processor.getProgramName(index) + "   ·   "
            + juce::String(ghostar::factoryPresetDescription(index));
        auto& target =
            ghostar::factoryPresetBank(index) == ghostar::PresetBank::Programs
                ? programs
                : charts;
        target.addItem(index + 1, text, true, index == current);
    }

    menu.addSectionHeader("Sound Charts: the manual's lessons");
    menu.addSubMenu("Sound Charts", charts);
    menu.addSectionHeader("Ghostar Programs: the performance bank");
    menu.addSubMenu("Ghostar Programs", programs);

    // The menu outlives the editor if the window is closed while it is
    // open, and its completion callback still runs — so it holds a safe
    // pointer rather than a raw one and does nothing if the editor is gone.
    juce::Component::SafePointer<GhostarAudioProcessorEditor> safeThis { this };
    menu.showMenuAsync(
        juce::PopupMenu::Options {}
            .withTargetComponent(&programName)
            .withMinimumWidth(programName.getWidth()),
        [safeThis](int choice) {
            if (choice <= 0 || safeThis == nullptr)
                return;
            safeThis->processor.setCurrentProgram(choice - 1);
            safeThis->refreshProgramDisplay();
        });
}

void GhostarAudioProcessorEditor::layoutKnob(Knob& knob,
                                           juce::Rectangle<int> area)
{
    knob.label.setBounds(area.removeFromBottom(14));
    knob.slider.setBounds(area.reduced(2));
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
    g.fillAll(panelCharcoal);

    // The header plate, so the browser reads as chrome rather than as
    // another panel section.
    const auto header =
        getLocalBounds().withHeight(margin + 48).toFloat();
    g.setColour(headerCharcoal);
    g.fillRect(header);
    g.setColour(hairline);
    g.drawLine(0.0f, header.getBottom() - 0.5f,
               static_cast<float>(getWidth()), header.getBottom() - 0.5f, 1.0f);

    // The gate lamp: lit while a gate source is holding the envelopes open,
    // which is the difference between a silent patch and a silent host.
    {
        const auto lamp = gateLampBounds.toFloat();
        const bool open = processor.isGateOpenForDisplay();
        const auto dot = juce::Rectangle<float> { lamp.getX(),
                                                  lamp.getCentreY() - 4.0f,
                                                  8.0f, 8.0f };
        g.setColour(open ? lampLit : lampDark);
        g.fillEllipse(dot);
        g.setColour(open ? lampLit.withAlpha(0.28f) : hairline);
        g.drawEllipse(dot.expanded(2.5f), 1.0f);
        g.setColour(open ? silkscreen : silkscreenDim);
        g.setFont(juce::FontOptions { 10.0f });
        g.drawText("GATE", lamp.withTrimmedLeft(14.0f),
                   juce::Justification::centredLeft);
    }

    for (const auto& section : sections)
    {
        const auto frame = section.bounds.toFloat();
        g.setColour(section.accent ? accentCharcoal : sectionCharcoal);
        g.fillRoundedRectangle(frame, 5.0f);
        g.setColour(section.accent ? silkscreenDim : hairline);
        g.drawRoundedRectangle(frame.reduced(0.5f), 5.0f, 1.0f);

        // The silkscreened section name, with a hairline running out from it
        // across the top of the frame.
        g.setColour(section.accent ? silkscreen : silkscreenDim);
        g.setFont(juce::FontOptions { 10.5f, juce::Font::bold });
        const auto titleArea = section.bounds.withHeight(18).reduced(9, 3);
        g.drawText(section.title, titleArea, juce::Justification::centredLeft);
    }
}

void GhostarAudioProcessorEditor::resized()
{
    sections.clear();
    auto area = getLocalBounds().reduced(margin);

    // ---- Header: identity, the program browser, the panic key -----------
    {
        auto header = area.removeFromTop(48);
        auto identity = header.removeFromLeft(230);
        wordmark.setBounds(identity.removeFromTop(30));
        subtitle.setBounds(identity);

        panicButton.setBounds(
            header.removeFromRight(88).withSizeKeepingCentre(84, 26));
        header.removeFromRight(10);
        gateLampBounds = header.removeFromRight(64).withSizeKeepingCentre(64, 28);
        header.removeFromRight(10);

        // The browser sits centred in what is left, at a fixed width, so it
        // does not swim about when the window is resized.
        auto browser = header.withSizeKeepingCentre(
            juce::jmin(header.getWidth(), 460), 42);
        previousProgram.setBounds(
            browser.removeFromLeft(30).withSizeKeepingCentre(28, 26));
        nextProgram.setBounds(
            browser.removeFromRight(30).withSizeKeepingCentre(28, 26));
        browser.reduce(6, 0);
        programBank.setBounds(browser.removeFromTop(12));
        programName.setBounds(browser.withSizeKeepingCentre(
            browser.getWidth(), 26));
    }
    area.removeFromTop(10);

    // The keyboard is its own strip along the bottom, with the performance
    // wheels standing to its left where a player's hand reaches them — the
    // arrangement the instrument itself has. It is claimed before the
    // control rows so the controls own all the space above it.
    {
        auto keyboardStrip = area.removeFromBottom(190);
        area.removeFromBottom(10);
        sections.push_back({ "KEYBOARD  /  37 KEYS, C TO C", keyboardStrip,
                             false });
        auto keys = keyboardStrip.reduced(9, 5).withTrimmedTop(14);

        auto wheels = keys.removeFromLeft(184);
        keys.removeFromLeft(10);
        auto bend = wheels.removeFromLeft(58);
        pitchWheelLabel.setBounds(bend.removeFromBottom(14));
        pitchWheel.setBounds(bend.reduced(8, 6));
        auto xColumn = wheels.removeFromLeft(63);
        layoutKnob(xWheel, xColumn.withSizeKeepingCentre(63, 96));
        layoutKnob(yWheel, wheels.withSizeKeepingCentre(63, 96));

        keyboard.setKeyWidth(static_cast<float>(keys.getWidth()) / 22.0f);
        keyboard.setBounds(keys);
    }

    const auto addSection = [this](juce::Rectangle<int>& row, int width,
                                   const juce::String& title,
                                   bool accent = false) {
        auto bounds = row.removeFromLeft(width);
        row.removeFromLeft(gap);
        sections.push_back({ title, bounds, accent });
        return bounds.reduced(9, 5).withTrimmedTop(14);
    };

    // ---- Row 1: sources and modulation ----------------------------------
    auto row1 = area.removeFromTop(160);
    {
        auto master = addSection(row1, 130, "MASTER");
        layoutKnob(tune, master.removeFromTop(84));
        master.removeFromTop(4);
        layoutSelector(octave, master.removeFromTop(42));

        auto oscA = addSection(row1, 180, "OSCILLATOR A");
        layoutSelector(oscAWaveform, oscA.removeFromTop(42));
        oscA.removeFromTop(22);
        sync.button.setBounds(oscA.removeFromTop(28).withTrimmedLeft(4));

        auto oscB = addSection(row1, 300, "OSCILLATOR B");
        auto oscBTop = oscB.removeFromTop(42);
        layoutSelector(oscBWaveform, oscBTop.removeFromLeft(104));
        oscBTop.removeFromLeft(8);
        layoutSelector(oscBRange, oscBTop);
        layoutKnob(interval, oscB.removeFromTop(84).reduced(100, 0));

        auto modX = addSection(row1, 320, "MOD X");
        auto modXTop = modX.removeFromTop(42);
        layoutSelector(arpeggiator, modXTop.removeFromLeft(100));
        modXTop.removeFromLeft(8);
        layoutSelector(modSource, modXTop);
        layoutKnob(lfoRate, modX.removeFromTop(84).reduced(110, 0));

        auto shaper = addSection(row1, 294, "SHAPER Y");
        layoutSelector(shaperMode, shaper.removeFromTop(42).reduced(64, 0));
        auto shaperKnobs = shaper.removeFromTop(84);
        layoutKnob(shaperShape,
                   shaperKnobs.removeFromLeft(shaperKnobs.getWidth() / 2));
        layoutKnob(shaperRate, shaperKnobs);

    }
    area.removeFromTop(10);

    // ---- Row 2: mixer, the signature filters, envelopes -----------------
    auto row2 = area.removeFromTop(248);
    {
        auto mixer = addSection(row2, 384, "AUDIO MIXER");
        auto mixerKnobs = mixer.removeFromLeft(98);
        layoutKnob(masterVolume, mixerKnobs.removeFromTop(100));
        mixerKnobs.removeFromTop(10);
        layoutKnob(brightness, mixerKnobs.removeFromTop(100));
        mixer.removeFromLeft(6);
        // Which path a fader feeds is the thing a player most needs to see,
        // so the two groups are captioned as well as spaced apart.
        auto groupCaptions = mixer.removeFromTop(13);
        auto shaperGroup = mixer.removeFromLeft(mixer.getWidth() * 4 / 7);
        auto filterGroup = mixer.withTrimmedLeft(10);
        shaperPathCaption.setBounds(
            groupCaptions.removeFromLeft(shaperGroup.getWidth()));
        filterPathCaption.setBounds(groupCaptions.withTrimmedLeft(10));
        const int shaperWidth = shaperGroup.getWidth() / 4;
        layoutFader(shaperPathA, shaperGroup.removeFromLeft(shaperWidth));
        layoutFader(shaperPathB, shaperGroup.removeFromLeft(shaperWidth));
        layoutFader(shaperPathRing, shaperGroup.removeFromLeft(shaperWidth));
        layoutFader(shaperPathNoise, shaperGroup);
        const int filterWidth = filterGroup.getWidth() / 3;
        layoutFader(filterPathA, filterGroup.removeFromLeft(filterWidth));
        layoutFader(filterPathB, filterGroup.removeFromLeft(filterWidth));
        layoutFader(filterPathNoise, filterGroup);

        auto filters =
            addSection(row2, 396, "UPPER FILTER U  /  LOWER FILTER L", true);
        auto upperRow = filters.removeFromTop(114);
        const int filterCell = upperRow.getWidth() / 4;
        layoutKnob(cutoff, upperRow.removeFromLeft(filterCell));
        layoutSelector(upperResonance,
                       upperRow.removeFromLeft(filterCell).withTrimmedTop(28));
        layoutSelector(slope,
                       upperRow.removeFromLeft(filterCell).withTrimmedTop(28));
        layoutKnob(kbAmount, upperRow);
        auto lowerRow = filters;
        layoutKnob(lowerOnly, lowerRow.removeFromLeft(filterCell));
        layoutKnob(resonance, lowerRow.removeFromLeft(filterCell));
        layoutSelector(lowerMode,
                       lowerRow.removeFromLeft(filterCell).withTrimmedTop(28));
        layoutSelector(tracking, lowerRow.withTrimmedTop(28));

        auto filterEnv = addSection(row2, 226, "FILTER ENVELOPE");
        layoutKnob(filterEnvAmount,
                   filterEnv.removeFromLeft(84).withSizeKeepingCentre(84, 100));
        const int envFader = filterEnv.getWidth() / 4;
        layoutFader(filterAttack, filterEnv.removeFromLeft(envFader));
        layoutFader(filterDecay, filterEnv.removeFromLeft(envFader));
        layoutFader(filterSustain, filterEnv.removeFromLeft(envFader));
        layoutFader(filterRelease, filterEnv);

        auto loudness = addSection(row2, 226, "LOUDNESS ENVELOPE");
        auto vcaColumn = loudness.removeFromLeft(98);
        vcaBypass.button.setBounds(
            vcaColumn.withSizeKeepingCentre(98, 26).translated(0, -6));
        const int loudFader = loudness.getWidth() / 4;
        layoutFader(loudnessAttack, loudness.removeFromLeft(loudFader));
        layoutFader(loudnessDecay, loudness.removeFromLeft(loudFader));
        layoutFader(loudnessSustain, loudness.removeFromLeft(loudFader));
        layoutFader(loudnessRelease, loudness);
    }
    area.removeFromTop(10);

    // ---- Row 3: gating, the wheel routing, and the performance strip ----
    // Everything here is laid out across rather than down: the row is one
    // control deep, so the panel ends on a shallow band above the keys
    // instead of three tall boxes with air under them.
    auto row3 = area;
    {
        auto gating = addSection(row3, 400, "TRIGGER  /  GATE SELECT");
        layoutSelector(trigger, gating.removeFromLeft(160));
        gating.removeFromLeft(20);
        auto gateColumn = gating.removeFromLeft(110);
        gateKbd.button.setBounds(gateColumn.removeFromTop(26));
        gateX.button.setBounds(gateColumn.removeFromTop(26));
        gateYExt.button.setBounds(gateColumn.removeFromTop(26));

        // The destination switches sit beside the wheels they route, now
        // that the wheels stand by the keyboard.
        auto destinations = addSection(row3, 440, "WHEEL DESTINATIONS");
        auto selectorRow = destinations.removeFromTop(42);
        layoutSelector(modXTo, selectorRow.removeFromLeft(200));
        selectorRow.removeFromLeft(16);
        layoutSelector(shaperYTo, selectorRow.removeFromLeft(200));
        destinations.removeFromTop(6);
        shapeXWithY.button.setBounds(
            destinations.removeFromTop(26).withTrimmedLeft(110));

        auto performance = addSection(row3, row3.getWidth(), "PERFORMANCE");
        layoutKnob(glide, performance.removeFromLeft(88));
        performance.removeFromLeft(12);
        auto glideColumn = performance.removeFromLeft(150);
        layoutSelector(glideMode, glideColumn.removeFromTop(42));
        performance.removeFromLeft(20);
        splitPaths.button.setBounds(
            performance.removeFromTop(42).withTrimmedTop(16).withWidth(130));
    }
}
