#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace youknow106
{

// Parameter identifiers. They live here rather than in the plug-in because the
// panel description below refers to them, and the panel description has to stay
// JUCE-free so the regression suite can check it on any toolchain.
namespace parameters
{
// Front-panel controls, in panel order.
inline constexpr auto volume       = "volume";
inline constexpr auto benderDco    = "benderDco";
inline constexpr auto benderVcf    = "benderVcf";
inline constexpr auto benderLfo    = "benderLfo";
inline constexpr auto portamento   = "portamento";
// Retained from the release before the paired switches were split. A host
// stores automation against a parameter *id*, so removing these would orphan
// any lane written for them -- the state migration only translates stored
// values. They forward to the pairs and are deliberately not on the panel.
inline constexpr auto legacyKeyMode = "keyMode";
inline constexpr auto legacyChorus  = "chorus";

// The two momentary assign-mode contacts, represented by their firmware-latched
// lamps. Both lit selects unison; neither lit is not a stable hardware state.
// There is no third button or single "key mode" parameter on the panel.
inline constexpr auto poly1        = "poly1";
inline constexpr auto poly2        = "poly2";
inline constexpr auto lfoRate      = "lfoRate";
inline constexpr auto lfoDelay     = "lfoDelay";
inline constexpr auto dcoLfo       = "dcoLfo";
inline constexpr auto pwm          = "pwm";
inline constexpr auto pwmMode      = "pwmMode";
inline constexpr auto range        = "range";
inline constexpr auto saw          = "saw";
inline constexpr auto pulse        = "pulse";
inline constexpr auto sub          = "sub";
inline constexpr auto noise        = "noise";
inline constexpr auto highPass     = "highPass";
inline constexpr auto cutoff       = "cutoff";
inline constexpr auto resonance    = "resonance";
inline constexpr auto envPolarity  = "envPolarity";
inline constexpr auto vcfEnv       = "vcfEnv";
inline constexpr auto vcfLfo       = "vcfLfo";
inline constexpr auto keyFollow    = "keyFollow";
inline constexpr auto vcaMode      = "vcaMode";
inline constexpr auto vcaLevel     = "vcaLevel";
inline constexpr auto attack       = "attack";
inline constexpr auto decay        = "decay";
inline constexpr auto sustain      = "sustain";
inline constexpr auto release      = "release";
// The two interlocked chorus buttons. Neither down is off; only one can latch.
inline constexpr auto chorusI      = "chorusI";
inline constexpr auto chorusII     = "chorusII";

// Controls the modelled instrument does not have. Character controls occupy an
// explicit extension card and keyboard-facing controls the live lower deck, so
// neither can be mistaken for stored hardware tone parameters. Their defaults
// preserve the base compatibility model; only explicitly documented structural
// defaults should be read as hardware claims.
inline constexpr auto transpose    = "transpose";
inline constexpr auto masterTune   = "masterTune";
inline constexpr auto velocity     = "velocity";
inline constexpr auto calibration  = "calibration";
inline constexpr auto chorusNoise  = "chorusNoise";
inline constexpr auto polyphony    = "polyphony";
// Internal-rate quality ladder: 1x, 2x, 4x. Replaces the earlier two-state
// `hq` switch, whose id is still recognised when an older session is opened.
inline constexpr auto quality      = "quality";
inline constexpr auto legacyHq     = "hq";
} // namespace parameters

namespace panel
{

// The front panel as data rather than as layout code.  The geometry follows the
// physical instrument: the sound strip is LFO, DCO, HPF, VCF, VCA, ENV and
// CHORUS; volume, portamento and the bender live in the controller cheek.

// Panel palette, 0xRRGGBB.
namespace colour
{
inline constexpr std::uint32_t faceplate     = 0x303236u; // charcoal painted metal
inline constexpr std::uint32_t faceplateHigh = 0x3b3e43u; // raised/moulded surface
inline constexpr std::uint32_t faceplateLow  = 0x202226u; // routed recess
inline constexpr std::uint32_t magenta       = 0xe0443eu; // hardware-style red/orange rail
inline constexpr std::uint32_t cyan          = 0x3c9fd1u; // programmer blue rail
inline constexpr std::uint32_t control       = 0xc7c5bdu; // warm grey switch/fader caps
inline constexpr std::uint32_t controlShadow = 0x6e7074u;
inline constexpr std::uint32_t led           = 0xff3f2fu; // red panel lamps
inline constexpr std::uint32_t ledDim        = 0x4c1715u;
inline constexpr std::uint32_t text          = 0xf0eee7u;
inline constexpr std::uint32_t textDim       = 0xa7a9acu;
inline constexpr std::uint32_t slot          = 0x111214u; // slider cut-out
inline constexpr std::uint32_t scope         = 0x090c0fu; // scope screen glass
inline constexpr std::uint32_t keyBlue       = 0x86b9cfu;
inline constexpr std::uint32_t keyAmber      = 0xf0a124u;
inline constexpr std::uint32_t keyIvory      = 0xd7d4c9u;
} // namespace colour

// Geometry, in abstract panel units. The editor scales the whole description to
// whatever size the window is, so nothing here depends on a pixel density.
inline constexpr float sectionPadding = 12.0f;
inline constexpr float sectionGap = 8.0f;
inline constexpr float panelMargin = 12.0f;
inline constexpr float headerHeight = 22.0f;
inline constexpr float controlLabelHeight = 18.0f;
// Expanded cards give each function more air without turning a single fader
// cap into a paddle. The cell owns the label and tick field; the mechanical
// fader itself stays within this convincingly physical width.
inline constexpr float maximumSliderWidth = 38.0f;

// The hardware surface occupies the top 522 units.  The sound strip begins to
// the right of the identity/controller cheek, the programmer tier sits below
// it, and the 61-key bed starts beside rather than underneath the bender.
inline constexpr float editorWidth = 1280.0f;
inline constexpr float displayReferenceHeight = 90.0f;
inline constexpr float soundRowTop = 12.0f;
inline constexpr float soundRowHeight = 218.0f;
inline constexpr float performanceDeckTop = 238.0f;
inline constexpr float performanceDeckHeight = 288.0f;
inline constexpr float programmerHeight = 92.0f;
// The host navigator is an add-on rail directly below the programmer: close
// enough to support the bank keys, but clearly outside the original controls.
inline constexpr float presetTop = 338.0f;
inline constexpr float presetHeight = 28.0f;
inline constexpr float panelHeight = 374.0f;
inline constexpr float keyboardHeight = 152.0f;

inline constexpr float controllerX = 12.0f;
inline constexpr float controllerWidth = 166.0f;
inline constexpr float instrumentLeft = 190.0f;
inline constexpr float instrumentRight = editorWidth - panelMargin;
inline constexpr float vectorPadX = 20.0f;
inline constexpr float vectorPadWidth = 150.0f;

// Everything without a front-panel hardware counterpart lives below the
// keybed in one unmistakable add-on bay.
inline constexpr float extensionDeckTop = panelHeight + keyboardHeight + 8.0f;
inline constexpr float extensionDeckHeight = 112.0f;
inline constexpr float characterCardX = 12.0f;
inline constexpr float characterCardWidth = 416.0f;
inline constexpr float keyboardCardX = 440.0f;
inline constexpr float keyboardCardWidth = 500.0f;
inline constexpr float displayCardX = 952.0f;
inline constexpr float displayCardWidth = 316.0f;
inline constexpr float operationsBarX = 780.0f;
inline constexpr float operationsBarWidth = 488.0f;

inline constexpr float helpStripGap = 8.0f;
inline constexpr float helpStripHeight = 40.0f;
inline constexpr float editorBottomMargin = 8.0f;
inline constexpr float editorHeight = extensionDeckTop + extensionDeckHeight
                                    + helpStripGap + helpStripHeight
                                    + editorBottomMargin;
// The physical keyboard is 61 notes, C2 through C7. With the display's
// middle-C convention those are MIDI notes 36..96; the instrument's Key
// Transpose function accounts for its wider transmitted range. External MIDI
// is a separate input path and must not be clamped to the visible keybed.
inline constexpr int keyboardLowestMidiNote = 36;
inline constexpr int keyboardHighestMidiNote = 96;
inline constexpr int keyboardWhiteKeyCount = 5 * 7 + 1;
// Both corners keep the panel's own aspect ratio, which the shorter one-row
// composition widens from 1.44 to about 1.84.
inline constexpr int minimumEditorWidth = 1130;
inline constexpr int minimumEditorHeight = 620;
inline constexpr int maximumEditorWidth = 1920;
inline constexpr int maximumEditorHeight = 1053;
inline constexpr float controlInset = 5.0f;
inline constexpr float stackGap = 6.0f;

enum class ControlKind
{
    Slider,  // vertical travel
    Knob,    // rotary travel on the controller cheek
    Toggle,  // a two-state button/lamp; pair interaction is defined by its section
    Radio,   // one button of a mutually exclusive group
    Steps    // a slider with named detents
};

struct Section
{
    const char* name;
    int slots;
    float x;
    float y;
    float width;
    float height;
};

struct Control
{
    const char* parameterId;
    const char* label;
    // Plain-language behavior shown by the plug-in editor. Keeping it in the
    // JUCE-free description makes tooltip coverage part of the panel contract
    // instead of an optional editor decoration.
    const char* tooltip;
    ControlKind kind;
    int section;
    float x;
    float y;
    float width;
    float height;
    // Where the legend is drawn. It follows the slot rather than the control,
    // because a slider's cut-out is deliberately narrower than the space its
    // name is allowed to use; taking the control's width instead is what
    // truncated "VOLUME" to "VOLU...".
    float labelX;
    float labelY;
    float labelWidth;
    float labelHeight;
    // Index of the mutually exclusive group this button belongs to, or -1, and
    // which value of the group's parameter it selects.
    int group;
    int groupValue;
};

inline constexpr int sectionCount = 9;
inline constexpr int controlCount = 38;

// Type sizes the editor draws with, in panel units. They live here rather than
// in the editor so the fit checks below and the drawing code cannot disagree.
inline constexpr float headerPointSize = 13.0f;
inline constexpr float labelPointSize = 11.0f;
inline constexpr float buttonPointSizeMax = 13.0f;
// The original compact selector keys carry unusually small legends. Keep a
// hard floor for them while allowing the proportions of those switches to
// remain authentic at the supported minimum editor size.
inline constexpr float buttonPointSizeMin = 7.5f;
// The reference panel uses a moderately condensed grotesque. Applying a small
// horizontal scale gives the platform font that character and, importantly,
// preserves full-size legends instead of relying on ellipses.
inline constexpr float typefaceHorizontalScale = 0.92f;
// A slider's legend is drawn in a box overhanging its control by this much on
// each side, because the legend belongs to the slot rather than to the narrow
// slider cut-out.
inline constexpr float labelOverhang = 7.0f;

[[nodiscard]] const std::array<Section, sectionCount>& sections() noexcept;
[[nodiscard]] const std::array<Control, controlCount>& controls() noexcept;
[[nodiscard]] float panelWidth() noexcept;

// Width of a string set in the panel typeface, in panel units.
//
// This is deliberately an *over*-estimate: the suites have to run on a machine
// with no JUCE and no fonts, so the check they perform cannot ask the real
// typeface how wide it draws. Every advance below is at or above what a bold
// grotesque of this size actually uses, so a legend this says fits is a legend
// that fits in the metal. The macOS suite repeats the check against the real
// font, which is what makes the approximation safe to rely on.
[[nodiscard]] float textWidth(const char* text, float pointSize,
                              bool bold) noexcept;

// The size the editor sets a button's legend in: the largest that fits the
// button both ways, never above `buttonPointSizeMax`. Returns a value below
// `buttonPointSizeMin` when even the floor does not fit, which is what
// `labelsFitTheirControls` refuses.
[[nodiscard]] float buttonPointSizeFor(const char* label, float width,
                                       float height) noexcept;
[[nodiscard]] float buttonPointSizeFor(const Control& control) noexcept;

// True when every control lies inside its own section, no two controls overlap,
// and every radio group is contiguous and complete. The suite asserts this
// rather than trusting the table by eye.
[[nodiscard]] bool layoutIsConsistent() noexcept;

// True when every legend on the panel is drawn in full: each section header
// inside its header bar, each slider legend inside its label box, and each
// button legend inside the button at a readable size. A layout that would
// ellipsize or clip any of them fails here rather than shipping.
[[nodiscard]] bool labelsFitTheirControls() noexcept;

// The first legend that does not fit, or nullptr when they all do. Only for
// the failure message -- the check above is the assertion.
[[nodiscard]] const char* firstOverflowingLabel() noexcept;

} // namespace panel
} // namespace youknow106
