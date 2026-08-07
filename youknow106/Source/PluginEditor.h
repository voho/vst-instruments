#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "DSP/YouKnow106Panel.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

// Panel look. Controls and legends stay resolution-independent JUCE vectors;
// the only bitmap is a bundled material scan composited into the faceplate.
class YouKnow106LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    YouKnow106LookAndFeel();

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
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// The moulded plastic of the faceplate: a maintained-but-used ABS material scan
// with a deterministic procedural fallback. It is decoded once and tiled, so
// repainting cannot make the wear crawl under the controls.
class PlasticTexture
{
public:
    void ensureBuilt (int tileSize);
    void fill (juce::Graphics&, juce::Rectangle<int> area, juce::Colour base) const;

private:
    juce::Image tile;
};

// Voice indicators, modulation and envelope, drawn from the processor's
// display atomics.
class YouKnow106Display final : public juce::Component,
                                public juce::SettableTooltipClient
{
public:
    void refresh (const YouKnow106AudioProcessor&);
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
class YouKnow106Keyboard final : public juce::MidiKeyboardComponent,
                                public juce::SettableTooltipClient
{
public:
    explicit YouKnow106Keyboard (juce::MidiKeyboardState& state)
        : juce::MidiKeyboardComponent (
              state, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
    }
};

// A deliberately non-literal take on the reference lever: an illuminated
// vector puck keeps the familiar left/right pitch and forward modulation
// gesture without copying the original moulding. Both axes are spring-loaded
// performance input and are never stored in a patch or session.
class YouKnow106PerformanceLever final : public juce::Component,
                                         public juce::SettableTooltipClient
{
public:
    YouKnow106PerformanceLever();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    [[nodiscard]] float getPitchBend() const noexcept { return pitchBend; }
    [[nodiscard]] float getModulation() const noexcept { return modulation; }

    std::function<void (float, float)> onPositionChanged;

private:
    [[nodiscard]] juce::Rectangle<float> controlArea() const noexcept;
    void updateFromPointer (juce::Point<float> position);
    void setValues (float bend, float mod, bool notify);

    float pitchBend = 0.0f;
    float modulation = 0.0f;
};

// A stable home for the same explanatory strings exposed through
// TooltipClient. Floating descriptive tips cover the panel and move under the
// pointer; this strip leaves controls visible while JUCE's separate slider
// value bubbles continue to report exact values.
class YouKnow106ContextHelp final : public juce::Component
{
public:
    YouKnow106ContextHelp();

    // `value` is the hovered control's current setting, already formatted by
    // the parameter itself. It is optional because not every component the
    // strip explains is a parameter -- the keyboard and the lever are not.
    void showFor (juce::Component* component, juce::String value = {});
    void showIdle();
    // A transient status line -- a completed patch-file load or save -- that
    // holds the idle strip for a few seconds and then yields. Hover help
    // still wins while it is up.
    void showNotice (juce::String title, juce::String text);
    void paint (juce::Graphics&) override;

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

class YouKnow106AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             public juce::FileDragAndDropTarget,
                                             private juce::Timer
{
public:
    explicit YouKnow106AudioProcessorEditor (YouKnow106AudioProcessor&);
    ~YouKnow106AudioProcessorEditor() override;

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

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void buildPanelControls();
    void buildUtilityStrip();
    void buildPresetBar();
    // Loads a program and brings the bar's own display back in step with it.
    void selectProgram (int index);
    // Steps by one, stopping at the ends rather than wrapping: a bank has a
    // first and a last patch and arriving back at INIT from the end is not what
    // a nudge of the button means.
    void stepProgram (int delta);
    void refreshPresetBar();
    // Patch-file transfer, LOAD/SAVE keys and drag-and-drop alike. Files
    // carry the hardware's own F0 41 30 patch dumps and nothing else.
    void chooseAndImportPatchFile();
    void chooseAndExportPatchFile();
    void importPatchFile (const juce::File& file);
    void exportPatchFile (const juce::File& file);
    void attachSlider (juce::Slider&, const char* parameterId);
    void attachButton (juce::Button&, const char* parameterId);
    void attachPolyButton (juce::Button&, const char* parameterId,
                           const char* otherParameterId);
    // The third MODE latch. It owns no parameter of its own: it closes both
    // momentary POLY contacts, which is how the hardware selects Solo Unison.
    void attachUnisonButton (juce::Button&);
    void attachExclusiveButton (juce::Button&, const char* parameterId,
                                const char* otherParameterId);
    void attachRadio (juce::Button&, const char* parameterId, int value);
    // Which parameter a hovered component belongs to, walking outward through
    // the private children a Slider or ComboBox may report as the mouse
    // target. Null for components that are not parameter controls.
    [[nodiscard]] const char* parameterIdFor (juce::Component*) const;
    [[nodiscard]] juce::Rectangle<float> scaled (float x, float y, float width,
                                                 float height) const;

    YouKnow106AudioProcessor& audioProcessor;
    YouKnow106LookAndFeel lookAndFeel;
    PlasticTexture texture;

    // One entry per panel table row, in the same order.
    struct PanelControl
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::TextButton> button;
        std::unique_ptr<juce::Label> label;
    };
    std::array<PanelControl, youknow106::panel::controlCount> panelControls {};

    juce::Label logoLabel;
    juce::Label editionLabel;
    YouKnow106Display display;

    juce::TextButton panicButton { "PANIC" };
    juce::TextButton hqButton { "HQ" };
    juce::TextButton randomize1Button { "RND1%" };
    juce::TextButton randomize10Button { "RND10%" };
    juce::TextButton randomize50Button { "RND50%" };
    juce::TextButton resetButton { "RESET" };
    // The tape section's own pairing: LOAD and SAVE move one patch between
    // the panel and a .syx file.
    juce::TextButton syxLoadButton { "LOAD" };
    juce::TextButton syxSaveButton { "SAVE" };
    std::unique_ptr<juce::FileChooser> sysExFileChooser;
    juce::Slider transposeSlider;
    juce::Slider tuneSlider;
    juce::Slider velocitySlider;
    juce::Slider calibrationSlider;
    juce::Slider chorusNoiseSlider;
    juce::Slider polyphonySlider;
    std::array<juce::Label, 6> utilityLabels {};

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
    int shownProgram = -1;
    bool shownEdited = false;

    YouKnow106Keyboard keyboard;
    YouKnow106PerformanceLever performanceLever;
    YouKnow106ContextHelp contextHelp;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> parameterAttachments;

    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnow106AudioProcessorEditor)
};
