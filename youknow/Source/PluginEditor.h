#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "DSP/YouKnowPanel.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

// Controls and lettering are vectors over a lightly worn material scan.
class YouKnowLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    YouKnowLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool isHighlighted, bool isDown) override;
    void drawComboBox (juce::Graphics&, int width, int height,
                       bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawCornerResizer (juce::Graphics&, int width, int height,
                            bool isMouseOver, bool isMouseDragging) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    std::unique_ptr<juce::FocusOutline> createFocusOutlineForComponent (
        juce::Component&) override;

    // The editor's own description scale. The compact add-on keys are the one
    // thing drawn here whose type has to know how large the whole panel is,
    // not merely how large one key is; the editor hands it over from resized()
    // rather than this class guessing from a top-level component.
    void setEditorScale (float newScale) noexcept { editorScale = newScale; }

private:
    float editorScale { youknow::panel::defaultEditorScale };
};

// Voice indicators, modulation and envelope, drawn from the processor's
// display atomics.
class YouKnowDisplay final : public juce::Component,
                                public juce::SettableTooltipClient
{
public:
    void refresh (const YouKnowAudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    int voiceMask = 0;
    int voices = 0;
    int voiceLimit = 6;
    float envelope = 0.0f;
    float lfo = 0.0f;
    double sampleRate = 0.0;
    int oversampling = 1;
    bool ready = false;
    float temperature = 25.0f;
    float railDroop = 0.0f;
    std::array<float, 256> scopeBuffer {};
    // Scope vertical range: a slow-release peak follower and the power-of-two
    // gain it selects. Both are display state, so they live with the drawing
    // rather than with the audio the processor publishes.
    float scopePeak = 0.0f;
    float scopeGain = 1.0f;
};

// JUCE's MIDI keyboard is interactive but does not implement TooltipClient.
// Add that one small behavior here so it follows the same complete help
// contract as sliders, buttons and combo boxes.
class YouKnowKeyboard final : public juce::MidiKeyboardComponent,
                                public juce::SettableTooltipClient
{
public:
    explicit YouKnowKeyboard (juce::MidiKeyboardState& state)
        : juce::MidiKeyboardComponent (
              state, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
    }

    void drawWhiteNote (int midiNoteNumber, juce::Graphics&,
                        juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour,
                        juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics&,
                        juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
};

// A deliberately non-literal take on the reference lever: an illuminated
// vector puck keeps the familiar left/right pitch and forward modulation
// gesture without copying the original moulding. Both axes are spring-loaded
// during user gestures and follow their host automation lanes during playback.
class YouKnowPerformanceLever final : public juce::Component,
                                         public juce::SettableTooltipClient
{
public:
    YouKnowPerformanceLever();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    // Where the lever sits for a given set of held arrow keys. Both horizontal
    // keys held is a player pressing against themselves, so the lever rests;
    // Down is the key that releases modulation, so holding it keeps modulation
    // released however long Up is held with it. Free-standing because it is
    // the whole of what a key release has to decide, and a headless suite
    // cannot press real keys to reach it.
    struct Axes
    {
        float bend;
        float modulation;
    };
    [[nodiscard]] static constexpr Axes axesForHeldKeys (bool leftHeld,
                                                         bool rightHeld,
                                                         bool upHeld,
                                                         bool downHeld) noexcept
    {
        return { leftHeld == rightHeld ? 0.0f : (rightHeld ? 1.0f : -1.0f),
                 upHeld && ! downHeld ? 1.0f : 0.0f };
    }

    [[nodiscard]] float getPitchBend() const noexcept { return pitchBend; }
    [[nodiscard]] float getModulation() const noexcept { return modulation; }
    [[nodiscard]] juce::String getAccessibilityValueText() const;
    void setPositionFromHost (float bend, float mod) { setValues (bend, mod, false); }
    void endGesture();

    std::function<void (float, float)> onPositionChanged;
    std::function<void()> onGestureStarted, onGestureEnded;

private:
    [[nodiscard]] juce::Rectangle<float> controlArea() const noexcept;
    void updateFromPointer (juce::Point<float> position);
    void setValues (float bend, float mod, bool notify);
    void beginGesture();

    float pitchBend = 0.0f;
    float modulation = 0.0f;
    bool keyboardGestureActive = false;
    bool gestureActive = false;
};

// A stable home for the same explanatory strings exposed through
// TooltipClient. Floating descriptive tips cover the panel and move under the
// pointer; this strip leaves controls visible while JUCE's separate slider
// value bubbles continue to report exact values.
class YouKnowContextHelp final : public juce::Component
{
public:
    YouKnowContextHelp();

    // `value` is the hovered control's current setting, already formatted by
    // the parameter itself. It is optional because not every component the
    // strip explains is a parameter -- the keyboard and file actions are not.
    void showFor (juce::Component* component, juce::String value = {});
    void showIdle();
    // A transient status line -- a completed patch-file load or save -- that
    // holds the idle strip for a few seconds and then yields. Hover help
    // still wins while it is up.
    void showNotice (juce::String title, juce::String text);

    // Where the strip's three columns land and what they are set in. The body
    // size is chosen so the whole explanation is printed rather than
    // ellipsised, and that choice depends on the platform's own sans, so the
    // fit check has to ask for the same answer paint uses instead of
    // reproducing the decision. Public for that reason only.
    struct BodyLayout
    {
        juce::Rectangle<float> value;
        juce::Rectangle<float> title;
        juce::Rectangle<int> body;
        float headingPointSize { 13.0f };
        // How many lines the body may take. JUCE fits a body by stepping the
        // type down and the line count up until the text fits, so this is the
        // whole of what decides whether a long explanation is printed or cut.
        int maximumLines { 3 };
    };
    [[nodiscard]] BodyLayout bodyLayout() const;

    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    [[nodiscard]] const juce::String& getHelpTitle() const noexcept
    {
        return helpTitle;
    }

    [[nodiscard]] const juce::String& getHelpText() const noexcept
    {
        return helpText;
    }

    [[nodiscard]] const juce::String& getHelpValue() const noexcept
    {
        return helpValue;
    }

private:
    void setContent (juce::String title, juce::String text, juce::String value);

    juce::String helpTitle;
    juce::String helpText;
    juce::String helpValue;
    juce::String noticeTitle;
    juce::String noticeText;
    juce::uint32 noticeExpiresAt { 0 };
};

class YouKnowAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             public juce::FileDragAndDropTarget,
                                             private juce::KeyListener,
                                             private juce::Timer
{
public:
    explicit YouKnowAudioProcessorEditor (YouKnowAudioProcessor&);
    ~YouKnowAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // A .syx patch dump can be dropped anywhere on the instrument; the LOAD
    // key is the same operation through a chooser.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // What the help strip prints beside a hovered component: that component's
    // parameter, formatted by the parameter itself, or empty for a component
    // that is not a parameter control. Public because it is the whole of the
    // strip's value behaviour and the regression suite has to be able to ask
    // for it without a real pointer over a real window.
    [[nodiscard]] juce::String parameterValueTextFor (juce::Component*) const;

    // Keyboard traversal and pointer hover share the fixed help strip. A newly
    // focused control wins until the pointer really moves, at which point the
    // hover target takes over again. Public for the same headless regression
    // reason as parameterValueTextFor().
    void refreshContextHelp (juce::Component* hovered,
                             juce::Component* focused,
                             bool mouseMoved);

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using juce::AudioProcessorEditor::keyPressed;

    void timerCallback() override;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    void buildPanelControls();
    void buildUtilityStrip();
    void buildPresetBar();
    void buildHardwareProgrammer();
    // Loads a program and brings the bar's own display back in step with it.
    void selectProgram (int index);
    // Steps by one, stopping at the ends rather than wrapping: a bank has a
    // first and a last patch and arriving back at INIT from the end is not what
    // a nudge of the button means.
    void stepProgram (int delta);
    void selectHardwareProgram();
    void refreshPresetBar();
    // Patch-file transfer, LOAD/SAVE keys and drag-and-drop alike. Files
    // carry the hardware's program/manual patch dumps.
    void chooseAndImportPatchFile();
    void chooseAndExportPatchFile();
    void importPatchFile (const juce::File& file);
    void exportPatchFile (const juce::File& file);
    void attachSlider (juce::Slider&, const char* parameterId);
    void attachButton (juce::Button&, const char* parameterId);
    void attachPolyButton (juce::Button&, const char* parameterId,
                           const char* otherParameterId);
    // Mouse-friendly extension for the hardware's simultaneous POLY 1 + POLY 2
    // gesture. It owns no parameter; the two contacts remain authoritative.
    void attachUnisonButton (juce::Button&);
    void attachChorusOffButton (juce::Button&);
    // The same convenience for the chorus pair. The existing two boolean
    // contacts remain the complete four-state representation.
    void attachChorusBothButton (juce::Button&);
    void refreshChorusButtons();
    void attachKeyTransposeButton (juce::Button&);
    void attachExclusiveButton (juce::Button&, const char* parameterId,
                                const char* otherParameterId);
    void attachRadio (juce::Button&, const char* parameterId, int value);
    // Which parameter a hovered component belongs to, walking outward through
    // the private children a Slider or ComboBox may report as the mouse
    // target. Null for components that are not parameter controls.
    [[nodiscard]] const char* parameterIdFor (juce::Component*) const;
    [[nodiscard]] juce::Rectangle<float> scaled (float x, float y, float width,
                                                 float height) const;

    YouKnowAudioProcessor& audioProcessor;
    YouKnowLookAndFeel lookAndFeel;
    juce::Image surfaceTexture;

    // One entry per panel table row, in the same order.
    struct PanelControl
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::TextButton> button;
        std::unique_ptr<juce::Label> label;
    };
    std::array<PanelControl, youknow::panel::controlCount> panelControls {};

    juce::Label logoLabel;
    juce::Label editionLabel;
    YouKnowDisplay display;

    juce::TextButton panicButton { "PANIC" };
    // The quality ladder. A three-rung setting needs a selector, not a lamp:
    // the button this replaced could only say on or off.
    juce::ComboBox qualityBox;
    juce::Label qualityLabel;
    // The VCF solver ladder beside it. Same kind of control and the same kind
    // of setting -- processing cost, not sound design -- so it sits in the
    // same group rather than hiding in the host's parameter list.
    juce::ComboBox vcfSolverBox;
    juce::Label vcfSolverLabel;
    juce::TextButton randomize1Button { "DRIFT 1%" };
    juce::TextButton randomize10Button { "VARY 10%" };
    juce::TextButton randomize50Button { "MORPH 50%" };
    juce::TextButton resetButton { "INIT" };
    juce::TextButton unisonButton { "UNISON" };
    juce::TextButton chorusBothButton { "I+II" };
    // The tape section's own pairing: LOAD and SAVE move one patch between
    // the panel and a .syx file.
    juce::TextButton syxLoadButton { "LOAD" };
    juce::TextButton syxSaveButton { "SAVE" };
    std::unique_ptr<juce::FileChooser> sysExFileChooser;
    juce::TextButton keyTransposeButton { "TRANSPOSE" };
    juce::Slider transposeSlider;
    juce::Slider tuneSlider;
    juce::Slider velocitySlider;
    juce::Slider calibrationSlider;
    juce::Slider agingSlider;
    juce::Slider chorusNoiseSlider;
    juce::Slider polyphonySlider;
    // AGING is appended at index 6 so the historical index-based styling of
    // the HISS label (4) stays put.
    std::array<juce::Label, 7> utilityLabels {};
    int lastTransposeSemitones = 12;

    // The original programmer tier. The immutable factory bank maps directly
    // to GROUP A/B, BANK 1..8 and PATCH 1..8; unsupported write/verify
    // operations remain present and honestly disabled.
    std::array<juce::TextButton, 2> groupButtons {};
    std::array<juce::TextButton, 8> bankButtons {};
    std::array<juce::TextButton, 8> patchButtons {};
    juce::Label hardwarePatchDisplay;
    juce::TextButton manualButton { "MANUAL" };
    juce::TextButton writeButton { "WRITE" };
    juce::TextButton verifyButton { "VERIFY" };
    int selectedHardwareGroup = 0;
    int selectedHardwareBank = 0;
    int selectedHardwarePatch = 0;

    // The patch bar. The programs live on the processor -- the host addresses
    // them too -- so this only names them and asks it to switch.
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton presetPrevButton { "<" };
    juce::TextButton presetNextButton { ">" };
    juce::TextButton presetReloadButton { "RELOAD" };
    // Lit while the panel no longer matches the patch that was loaded, which is
    // the only way to tell a recalled patch from an edited one.
    juce::Label presetEditedLabel;
    juce::TextButton customPatchLoadButton { "LOAD .SYX" };
    juce::TextButton customPatchSaveButton { "SAVE .SYX" };
    int shownProgram = -1;
    bool shownEdited = false;

    YouKnowKeyboard keyboard;
    YouKnowPerformanceLever performanceLever;
    YouKnowContextHelp contextHelp;
    juce::Component::SafePointer<juce::Component> lastFocusedHelpComponent;
    juce::Point<float> lastMouseScreenPosition;
    bool contextHelpFollowsKeyboardFocus = false;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<ComboBoxAttachment>> comboBoxAttachments;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> parameterAttachments;

    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnowAudioProcessorEditor)
};
