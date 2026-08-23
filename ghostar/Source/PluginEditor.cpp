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
const juce::Colour trackSilver { 0xffd2d2d5 };
const juce::Colour trackPale { 0xff8b8b8e };
const juce::Colour hairline { 0xff4c4c50 };
const juce::Colour lampLit { 0xfff2f2f2 };
const juce::Colour lampDark { 0xff2a2a2c };

// The panel's design geometry. Everything is laid out and painted against
// these numbers; the window scales the result.
constexpr int editorWidth = 1460;
constexpr int editorHeight = 780;
// 60 % of the design size, where the smallest silkscreen is still about
// 7 points. Below that the panel would be legible only by tooltip.
constexpr int minimumWidth = 876;
constexpr int minimumHeight = 468;
// What a host or a desktop window puts around the editor: a frame either
// side, a title bar and a frame above and below. An allowance rather than a
// measurement — it only has to be generous enough that the panel is not the
// thing that overflows.
constexpr int windowChromeWidth = 32;
constexpr int windowChromeHeight = 64;
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
    // The untravelled slot is black; the travelled part below the cap is
    // silver, so a bank of faders reads as a shape at a glance.
    g.setColour(markingBlack);
    g.fillRoundedRectangle(track, 2.0f);
    g.setColour(knobRim);
    g.drawRoundedRectangle(track.reduced(0.5f), 2.0f, 1.0f);

    // The travelled part of the track lights, so a bank of faders reads as
    // a shape at a glance rather than as eight identical caps.
    auto lit = track;
    lit.setTop(sliderPos);
    g.setColour(trackSilver);
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
    const auto rocker =
        button.getLocalBounds().toFloat().withSizeKeepingCentre(38.0f, 24.0f);

    // The panel's paddle switches: a grey body in a black surround, with
    // the thrown half standing pale and the resting half sunk dark, so
    // which way it sits reads at a glance and without colour.
    g.setColour(markingBlack);
    g.fillRoundedRectangle(rocker.expanded(1.5f), 3.0f);
    auto body = rocker.reduced(1.0f);
    auto thrown = body;
    auto resting = body;
    if (button.getToggleState())
    {
        thrown = thrown.removeFromTop(body.getHeight() / 2.0f);
        resting = resting.withTrimmedTop(body.getHeight() / 2.0f);
    }
    else
    {
        thrown = thrown.removeFromBottom(body.getHeight() / 2.0f);
        resting = resting.withTrimmedBottom(body.getHeight() / 2.0f);
    }
    g.setColour(knobFaceTop);
    g.fillRoundedRectangle(thrown, 2.0f);
    g.setColour(juce::Colour { 0xff56565a });
    g.fillRoundedRectangle(resting, 2.0f);
    g.setColour(shouldDrawButtonAsHighlighted ? silkscreen : knobRim);
    g.drawRoundedRectangle(rocker.reduced(0.5f), 2.5f, 1.0f);

    g.setColour(button.getToggleState() ? silkscreen : silkscreenDim);
    g.setFont(juce::FontOptions { 13.5f });
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
    return juce::Font { juce::FontOptions { 13.5f } };
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
            "Dual 20 kΩ linear level control. The normalled main jack lets "
            "the two loaded wipers interact; SPLIT isolates them.");
    addKnob(brightness, ids::brightness, "BRIGHTNESS",
            "The passive post-VCA Shaper tone network. At zero it is a "
            "294.7 Hz lowpass; fully up retains a gentle -1.58 dB high "
            "shelf rather than bypassing.");
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
    addFader(xWheel, ids::xWheel, "MOD X",
            "The MOD X performance wheel. It attenuates toward zero, so a "
            "bipolar source keeps its symmetry.");
    addFader(yWheel, ids::yWheel, "SHAPER Y",
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
    canvas.addAndMakeVisible(pitchWheel);
    pitchWheelLabel.setText("BEND", juce::dontSendNotification);
    pitchWheelLabel.setFont(juce::FontOptions { 11.0f });
    pitchWheelLabel.setJustificationType(juce::Justification::centred);
    pitchWheelLabel.setColour(juce::Label::textColourId, silkscreenDim);
    canvas.addAndMakeVisible(pitchWheelLabel);

    panicButton.setTooltip("Stops every sounding voice at once, including "
                           "drones held open by VCA BYPASS.");
    panicButton.onClick = [this] { processor.requestPanic(); };
    canvas.addAndMakeVisible(panicButton);

    const auto addCaption = [this](juce::Label& label,
                                   const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::FontOptions { 11.0f, juce::Font::bold });
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, silkscreenDim);
        label.setInterceptsMouseClicks(false, false);
        canvas.addAndMakeVisible(label);
    };
    addCaption(shaperPathCaption, "SHAPER Y PATH");
    addCaption(filterPathCaption, "FILTER PATH");

    wordmark.setText("ghostar", juce::dontSendNotification);
    wordmark.setFont(juce::FontOptions { 30.0f, juce::Font::bold });
    wordmark.setColour(juce::Label::textColourId, silkscreen);
    wordmark.setJustificationType(juce::Justification::centredLeft);
    canvas.addAndMakeVisible(wordmark);

    subtitle.setText("monophonic dual-filter synthesizer",
                     juce::dontSendNotification);
    subtitle.setFont(juce::FontOptions { 11.0f });
    subtitle.setColour(juce::Label::textColourId, silkscreenDim);
    subtitle.setJustificationType(juce::Justification::centredLeft);
    canvas.addAndMakeVisible(subtitle);

    previousProgram.setTooltip("Previous program.");
    previousProgram.onClick = [this] { stepProgram(-1); };
    canvas.addAndMakeVisible(previousProgram);
    nextProgram.setTooltip("Next program.");
    nextProgram.onClick = [this] { stepProgram(1); };
    canvas.addAndMakeVisible(nextProgram);
    programName.onClick = [this] { showProgramMenu(); };
    canvas.addAndMakeVisible(programName);
    programBank.setFont(juce::FontOptions { 11.0f });
    programBank.setJustificationType(juce::Justification::centred);
    programBank.setColour(juce::Label::textColourId, silkscreenDim);
    canvas.addAndMakeVisible(programBank);

    keyboard.setAvailableRange(48, 84); // the hardware's 37 keys, C to C
    keyboard.setOctaveForMiddleC(4);
    canvas.addAndMakeVisible(keyboard);

    addAndMakeVisible(canvas);

    refreshProgramDisplay();
    // Every attachment above fired its slider's callback as it took the
    // parameter's standing value; from here a callback means a human.
    wiringUp = false;
    startTimerHz(15);

    // The window may be any size; the panel inside it is always the design
    // geometry, scaled. Limits stop the type shrinking past reading size at
    // one end and the panel going soft at the other.
    setResizable(true, false);
    // setResizeLimits is what installs the default constrainer, so it has to
    // come before the ratio is set on it.
    setResizeLimits(minimumWidth, minimumHeight, editorWidth * 2,
                    editorHeight * 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(static_cast<double>(editorWidth)
                                         / static_cast<double>(editorHeight));
    // Open at the largest whole panel the display can actually show.
    juce::Rectangle<int> workArea;
    if (auto* display =
            juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        workArea = display->userArea;
    const auto opening = panelSizeForWorkArea(workArea);
    setSize(opening.getWidth(), opening.getHeight());
}

juce::Rectangle<int> GhostarAudioProcessorEditor::panelSizeForWorkArea(
    juce::Rectangle<int> workArea)
{
    // Full size wherever it fits. Where it does not — a 1366x768 or 1280x800
    // laptop, or a 1080p panel at 150 % — the whole panel is shrunk to fit
    // rather than having its bottom edge cut off. Never below the size the
    // silkscreen stays readable at: on a screen smaller than that, a window
    // the user can move is a better failure than type nobody can read.
    double scale = 1.0;
    if (!workArea.isEmpty())
        scale = juce::jmin(1.0,
                           static_cast<double>(workArea.getWidth()
                                               - windowChromeWidth)
                               / editorWidth,
                           static_cast<double>(workArea.getHeight()
                                               - windowChromeHeight)
                               / editorHeight);
    scale = juce::jmax(scale, static_cast<double>(minimumWidth) / editorWidth);
    return { juce::roundToInt(editorWidth * scale),
             juce::roundToInt(editorHeight * scale) };
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
    canvas.addAndMakeVisible(knob.slider);
    knob.label.setText(text, juce::dontSendNotification);
    knob.label.setFont(juce::FontOptions { 12.0f });
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, silkscreen);
    knob.label.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(knob.label);
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
    canvas.addAndMakeVisible(fader.slider);
    fader.label.setText(text, juce::dontSendNotification);
    fader.label.setFont(juce::FontOptions { 11.0f });
    fader.label.setJustificationType(juce::Justification::centred);
    fader.label.setColour(juce::Label::textColourId, silkscreen);
    fader.label.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(fader.label);
    fader.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, parameterId, fader.slider);
}

void GhostarAudioProcessorEditor::addRocker(Rocker& rocker,
                                          const char* parameterId,
                                          const juce::String& text,
                                          const juce::String& tooltip)
{
    rocker.button.setTooltip(tooltip);
    canvas.addAndMakeVisible(rocker.button);
    rocker.label.setText(text, juce::dontSendNotification);
    rocker.label.setFont(juce::FontOptions { 12.0f });
    rocker.label.setJustificationType(juce::Justification::centred);
    rocker.label.setColour(juce::Label::textColourId, silkscreen);
    rocker.label.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(rocker.label);
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
    canvas.addAndMakeVisible(selector.box);
    selector.label.setText(text, juce::dontSendNotification);
    selector.label.setFont(juce::FontOptions { 12.0f });
    selector.label.setJustificationType(juce::Justification::centred);
    selector.label.setColour(juce::Label::textColourId, silkscreen);
    selector.label.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(selector.label);
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
                        &resonance, &kbAmount, &filterEnvAmount, &glide })
        revert(*knob);
    for (Fader* fader : { &shaperPathA, &shaperPathB, &shaperPathRing,
                          &shaperPathNoise, &filterPathA, &filterPathB,
                          &filterPathNoise, &filterAttack, &filterDecay,
                          &filterSustain, &filterRelease, &loudnessAttack,
                          &loudnessDecay, &loudnessSustain, &loudnessRelease,
                          &xWheel, &yWheel })
        revert(*fader);

    // A host can change the program behind the editor's back.
    if (processor.getCurrentProgram() != shownProgram)
        refreshProgramDisplay();

    if (processor.isGateOpenForDisplay() != gateLampLit)
    {
        gateLampLit = !gateLampLit;
        canvas.repaint(gateLampBounds.expanded(4));
    }
}

void GhostarAudioProcessorEditor::refreshProgramDisplay()
{
    shownProgram = processor.getCurrentProgram();
    programName.setButtonText(processor.getProgramName(shownProgram));
    programName.setTooltip(
        juce::String(ghostar::factoryPresetDescription(shownProgram))
        + " Click to browse the bank.");
    const bool isProgram =
        ghostar::factoryPresetBank(shownProgram)
        == ghostar::PresetBank::Programs;
    const bool isInit =
        shownProgram == ghostar::factoryPresetIndexByName("Init");
    const bool isPreparatoryPattern =
        shownProgram
        == ghostar::factoryPresetIndexByName("Preparatory Pattern");
    programBank.setText(isInit
                            ? "INIT"
                            : isPreparatoryPattern
                            ? "SOUND CHART - INTENTIONALLY SILENT"
                            : isProgram ? "GHOSTAR PROGRAM" : "SOUND CHART",
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
    const int init = ghostar::factoryPresetIndexByName("Init");

    for (int index = 0; index < processor.getNumPrograms(); ++index)
    {
        if (index == init)
            continue;

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

    menu.addSectionHeader("Default");
    menu.addItem(init + 1,
                 processor.getProgramName(init) + "   ·   "
                     + juce::String(ghostar::factoryPresetDescription(init)),
                 true, init == current);
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

// Every control is captioned the same way — the name directly above the
// thing it names — so a knob and a selector stacked in one column can never
// leave two captions adjacent in the middle with nothing to say which
// belongs to which.
void GhostarAudioProcessorEditor::layoutKnob(Knob& knob,
                                           juce::Rectangle<int> area)
{
    knob.label.setBounds(area.removeFromTop(16));
    knob.slider.setBounds(area.reduced(2));
}

void GhostarAudioProcessorEditor::layoutFader(Fader& fader,
                                            juce::Rectangle<int> area)
{
    fader.label.setBounds(area.removeFromTop(16));
    fader.slider.setBounds(area);
}

void GhostarAudioProcessorEditor::layoutSelector(Selector& selector,
                                               juce::Rectangle<int> area)
{
    selector.label.setBounds(area.removeFromTop(16));
    selector.box.setBounds(area.removeFromTop(26));
}

void GhostarAudioProcessorEditor::layoutRocker(Rocker& rocker,
                                              juce::Rectangle<int> area)
{
    rocker.label.setBounds(area.removeFromTop(16));
    rocker.button.setBounds(area.removeFromTop(26));
}

void GhostarAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Only ever seen if a host forces a size off the panel's aspect ratio;
    // the panel itself is painted by the canvas.
    g.fillAll(panelCharcoal);
}

void GhostarAudioProcessorEditor::PanelCanvas::paint(juce::Graphics& g)
{
    const auto& sections = owner.sections;

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
        const auto lamp = owner.gateLampBounds.toFloat();
        const bool open = owner.processor.isGateOpenForDisplay();
        const auto dot = juce::Rectangle<float> { lamp.getX(),
                                                  lamp.getCentreY() - 4.0f,
                                                  8.0f, 8.0f };
        g.setColour(open ? lampLit : lampDark);
        g.fillEllipse(dot);
        g.setColour(open ? lampLit.withAlpha(0.28f) : hairline);
        g.drawEllipse(dot.expanded(2.5f), 1.0f);
        g.setColour(open ? silkscreen : silkscreenDim);
        g.setFont(juce::FontOptions { 11.0f });
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
        g.setFont(juce::FontOptions { 12.0f, juce::Font::bold });
        const auto titleArea = section.bounds.withHeight(20).reduced(9, 4);
        g.drawText(section.title, titleArea, juce::Justification::centredLeft);
    }
}

void GhostarAudioProcessorEditor::resized()
{
    // The panel is laid out once, at its design size, and the window scales
    // it — so every window size shows the whole instrument, keys included,
    // with the proportions the silkscreen was drawn to.
    const double scale = juce::jmin(
        static_cast<double>(getWidth()) / static_cast<double>(editorWidth),
        static_cast<double>(getHeight()) / static_cast<double>(editorHeight));
    const auto scaled =
        juce::Rectangle<int> { juce::roundToInt(editorWidth * scale),
                               juce::roundToInt(editorHeight * scale) }
            .withCentre(getLocalBounds().getCentre());
    canvas.setTransform(
        juce::AffineTransform::scale(static_cast<float>(scale))
            .translated(static_cast<float>(scaled.getX()),
                        static_cast<float>(scaled.getY())));
    canvas.setBounds(0, 0, editorWidth, editorHeight);

    layoutPanel();
}

void GhostarAudioProcessorEditor::layoutPanel()
{
    sections.clear();
    auto area =
        juce::Rectangle<int> { editorWidth, editorHeight }.reduced(margin);

    // ---- Header: identity, the program browser, the product switches -----
    // PANIC and SPLIT live up here rather than on the panel: neither is a
    // control the modelled instrument has, and keeping them off the
    // silkscreen keeps the panel below honest.
    {
        auto header = area.removeFromTop(48);
        auto identity = header.removeFromLeft(250);
        wordmark.setBounds(identity.removeFromTop(30));
        subtitle.setBounds(identity);

        panicButton.setBounds(
            header.removeFromRight(92).withSizeKeepingCentre(88, 28));
        header.removeFromRight(14);
        auto splitCell = header.removeFromRight(70);
        splitPaths.label.setBounds(splitCell.removeFromTop(14));
        splitPaths.button.setBounds(splitCell.removeFromTop(24));
        header.removeFromRight(14);
        gateLampBounds =
            header.removeFromRight(66).withSizeKeepingCentre(66, 28);
        header.removeFromRight(14);

        auto browser = header.withSizeKeepingCentre(
            juce::jmin(header.getWidth(), 480), 44);
        previousProgram.setBounds(
            browser.removeFromLeft(32).withSizeKeepingCentre(30, 28));
        nextProgram.setBounds(
            browser.removeFromRight(32).withSizeKeepingCentre(30, 28));
        browser.reduce(6, 0);
        programBank.setBounds(browser.removeFromTop(14));
        programName.setBounds(
            browser.withSizeKeepingCentre(browser.getWidth(), 28));
    }
    area.removeFromTop(10);

    // A framed, titled block of panel. Returns the area inside it that the
    // controls may use.
    const auto frame = [this](juce::Rectangle<int> bounds,
                              const juce::String& title,
                              bool accent = false) {
        sections.push_back({ title, bounds, accent });
        return bounds.reduced(10, 6).withTrimmedTop(18);
    };
    const auto column = [&frame](juce::Rectangle<int>& row, int width,
                                 const juce::String& title,
                                 bool accent = false) {
        auto bounds = row.removeFromLeft(width);
        row.removeFromLeft(gap);
        return frame(bounds, title, accent);
    };
    // A knob and its caption want this much height; a captioned selector or
    // rocker wants far less, so the short ones are centred in their cell
    // rather than left hanging at the top of it.
    constexpr int knobCell = 106;
    constexpr int selectorCell = 42;
    const auto centred = [](juce::Rectangle<int> cell, int height) {
        return cell.withSizeKeepingCentre(cell.getWidth(), height);
    };

    // ---- The keyboard and the levers beside it --------------------------
    // The instrument puts GLIDE and the three levers on their own sub-panel
    // to the left of the keys; this follows it, and the keys take the rest.
    {
        auto bottom = area.removeFromBottom(206);
        area.removeFromBottom(10);

        auto levers = frame(bottom.removeFromLeft(330), "GLIDE  /  LEVERS");
        bottom.removeFromLeft(gap);
        auto glideRow = levers.removeFromLeft(150);
        layoutKnob(glide, centred(glideRow.removeFromTop(112), knobCell));
        layoutSelector(glideMode, centred(glideRow, selectorCell));
        auto leverRow = levers.withTrimmedLeft(8);
        const int leverWidth = leverRow.getWidth() / 3;
        pitchWheelLabel.setBounds(
            leverRow.removeFromTop(16).removeFromLeft(leverWidth));
        auto leverBodies = leverRow;
        xWheel.label.setBounds(
            juce::Rectangle<int> { leverBodies.getX() + leverWidth,
                                   leverBodies.getY() - 16, leverWidth, 16 });
        yWheel.label.setBounds(
            juce::Rectangle<int> { leverBodies.getX() + 2 * leverWidth,
                                   leverBodies.getY() - 16, leverWidth, 16 });
        pitchWheel.setBounds(leverBodies.removeFromLeft(leverWidth)
                                 .reduced(leverWidth / 2 - 14, 2));
        xWheel.slider.setBounds(leverBodies.removeFromLeft(leverWidth)
                                    .reduced(leverWidth / 2 - 14, 2));
        yWheel.slider.setBounds(
            leverBodies.reduced(leverBodies.getWidth() / 2 - 14, 2));

        auto keys = frame(bottom, "KEYBOARD  /  37 KEYS, C TO C");
        keyboard.setKeyWidth(static_cast<float>(keys.getWidth()) / 22.0f);
        keyboard.setBounds(keys);
    }

    // ---- MOD X and SHAPER Y: full-height columns on the right ----------
    // Both are tall columns on the instrument's own panel, spanning the
    // whole depth beside everything else, and they are here too.
    {
        auto right = area.removeFromRight(316);
        area.removeFromRight(gap);

        auto modX = frame(right.removeFromLeft(154), "MOD X");
        right.removeFromLeft(gap);
        const int modXSpacing =
            juce::jmax(12, (modX.getHeight() - 2 * selectorCell - knobCell)
                               / 3);
        layoutSelector(arpeggiator, modX.removeFromTop(selectorCell));
        modX.removeFromTop(modXSpacing);
        layoutSelector(modSource, modX.removeFromTop(selectorCell));
        modX.removeFromTop(modXSpacing);
        layoutKnob(lfoRate, modX.removeFromTop(knobCell));

        auto shaper = frame(right, "SHAPER Y");
        const int shaperSpacing =
            juce::jmax(10, (shaper.getHeight() - selectorCell - 2 * knobCell)
                               / 3);
        layoutSelector(shaperMode, shaper.removeFromTop(selectorCell));
        shaper.removeFromTop(shaperSpacing);
        layoutKnob(shaperShape, shaper.removeFromTop(knobCell));
        shaper.removeFromTop(shaperSpacing);
        layoutKnob(shaperRate, shaper.removeFromTop(knobCell));
    }

    // ---- Row 1: master, the oscillators, trigger and gating -------------
    {
        auto row1 = area.removeFromTop(142);
        auto master = column(row1, 190, "MASTER");
        layoutKnob(tune, centred(master.removeFromLeft(96), knobCell));
        master.removeFromLeft(10);
        layoutSelector(octave, centred(master, selectorCell));

        auto oscA = column(row1, 190, "OSCILLATOR A");
        layoutSelector(oscAWaveform,
                       centred(oscA.removeFromLeft(104), selectorCell));
        oscA.removeFromLeft(10);
        layoutRocker(sync, centred(oscA, selectorCell));

        auto oscB = column(row1, 350, "OSCILLATOR B");
        layoutSelector(oscBWaveform,
                       centred(oscB.removeFromLeft(112), selectorCell));
        oscB.removeFromLeft(8);
        layoutSelector(oscBRange,
                       centred(oscB.removeFromLeft(120), selectorCell));
        oscB.removeFromLeft(8);
        layoutKnob(interval, centred(oscB, knobCell));

        auto triggerSection = column(row1, 130, "TRIGGER");
        layoutSelector(trigger, centred(triggerSection, selectorCell));

        auto gates = column(row1, row1.getWidth(), "GATE SELECT");
        const int gateCell = gates.getWidth() / 3;
        layoutRocker(gateKbd,
                     centred(gates.removeFromLeft(gateCell), selectorCell));
        layoutRocker(gateX,
                     centred(gates.removeFromLeft(gateCell), selectorCell));
        layoutRocker(gateYExt, centred(gates, selectorCell));
    }
    area.removeFromTop(gap);

    // ---- Row 2: routing, the mixer, the filters, the envelopes ----------
    {
        auto row2 = area;

        auto destinations = column(row2, 186, "WHEEL DESTINATIONS");
        const int destinationSpacing =
            juce::jmax(14, (destinations.getHeight() - 3 * selectorCell) / 3);
        layoutSelector(modXTo, destinations.removeFromTop(selectorCell));
        destinations.removeFromTop(destinationSpacing);
        layoutRocker(shapeXWithY, destinations.removeFromTop(selectorCell));
        destinations.removeFromTop(destinationSpacing);
        layoutSelector(shaperYTo, destinations.removeFromTop(selectorCell));

        auto mixer = column(row2, 386, "AUDIO MIXER");
        auto mixerKnobs = mixer.removeFromLeft(96);
        layoutKnob(masterVolume, mixerKnobs.removeFromTop(knobCell + 8));
        mixerKnobs.removeFromTop(8);
        layoutKnob(brightness, mixerKnobs.removeFromTop(knobCell + 8));
        mixer.removeFromLeft(8);
        // The two path groups are captioned under their faders, as the
        // instrument's own silkscreen brackets them.
        auto pathCaptions = mixer.removeFromBottom(15);
        auto shaperGroup = mixer.removeFromLeft(mixer.getWidth() * 4 / 7);
        auto filterGroup = mixer.withTrimmedLeft(10);
        shaperPathCaption.setBounds(
            pathCaptions.removeFromLeft(shaperGroup.getWidth()));
        filterPathCaption.setBounds(pathCaptions.withTrimmedLeft(10));
        const int shaperWidth = shaperGroup.getWidth() / 4;
        layoutFader(shaperPathA, shaperGroup.removeFromLeft(shaperWidth));
        layoutFader(shaperPathB, shaperGroup.removeFromLeft(shaperWidth));
        layoutFader(shaperPathRing, shaperGroup.removeFromLeft(shaperWidth));
        layoutFader(shaperPathNoise, shaperGroup);
        const int filterWidth = filterGroup.getWidth() / 3;
        layoutFader(filterPathA, filterGroup.removeFromLeft(filterWidth));
        layoutFader(filterPathB, filterGroup.removeFromLeft(filterWidth));
        layoutFader(filterPathNoise, filterGroup);

        auto filters = column(row2, 330,
                              "UPPER FILTER U  /  LOWER FILTER L", true);
        auto upperRow = filters.removeFromTop(filters.getHeight() / 2);
        const int filterCell = upperRow.getWidth() / 4;
        layoutKnob(cutoff,
                   centred(upperRow.removeFromLeft(filterCell), knobCell));
        layoutSelector(upperResonance,
                       centred(upperRow.removeFromLeft(filterCell),
                               selectorCell));
        layoutSelector(slope, centred(upperRow.removeFromLeft(filterCell),
                                      selectorCell));
        layoutKnob(kbAmount, centred(upperRow, knobCell));
        auto lowerRow = filters;
        layoutKnob(lowerOnly,
                   centred(lowerRow.removeFromLeft(filterCell), knobCell));
        layoutKnob(resonance,
                   centred(lowerRow.removeFromLeft(filterCell), knobCell));
        layoutSelector(lowerMode,
                       centred(lowerRow.removeFromLeft(filterCell),
                               selectorCell));
        layoutSelector(tracking, centred(lowerRow, selectorCell));

        // The two envelopes stack in one column, as they do on the panel.
        auto envelopes = row2;
        auto filterEnv = frame(
            envelopes.removeFromTop(envelopes.getHeight() / 2),
            "FILTER ENVELOPE");
        auto loudness = frame(envelopes.withTrimmedTop(gap),
                              "LOUDNESS ENVELOPE");

        layoutKnob(filterEnvAmount,
                   centred(filterEnv.removeFromLeft(76), knobCell));
        const int envFader = filterEnv.getWidth() / 4;
        layoutFader(filterAttack, filterEnv.removeFromLeft(envFader));
        layoutFader(filterDecay, filterEnv.removeFromLeft(envFader));
        layoutFader(filterSustain, filterEnv.removeFromLeft(envFader));
        layoutFader(filterRelease, filterEnv);

        layoutRocker(vcaBypass,
                     centred(loudness.removeFromLeft(76), selectorCell));
        const int loudFader = loudness.getWidth() / 4;
        layoutFader(loudnessAttack, loudness.removeFromLeft(loudFader));
        layoutFader(loudnessDecay, loudness.removeFromLeft(loudFader));
        layoutFader(loudnessSustain, loudness.removeFromLeft(loudFader));
        layoutFader(loudnessRelease, loudness);
    }
}
