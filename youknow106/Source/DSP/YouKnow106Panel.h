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

// Product extensions remain distinct from stored hardware tone parameters even
// when the editor places one beside its closest original control. Their defaults
// preserve the base compatibility model; only explicitly documented structural
// defaults should be read as hardware claims.
inline constexpr auto transpose    = "transpose";
inline constexpr auto masterTune   = "masterTune";
inline constexpr auto velocity     = "velocity";
inline constexpr auto calibration  = "calibration";
// Time since the modelled unit's last service, beside Unit Character in the
// MODEL zone: Character is how far this unit sits from nominal build,
// Aging how far its trims have drifted since calibration.
inline constexpr auto aging        = "aging";
inline constexpr auto chorusNoise  = "chorusNoise";
inline constexpr auto polyphony    = "polyphony";
// Internal-rate quality ladder: 1x, 2x, 4x. The earlier two-state `hq` id must
// remain registered so Audio Unit parameter indices and old sessions survive.
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
inline constexpr std::uint32_t faceplate     = 0x2d2d2bu; // warm charcoal painted metal
inline constexpr std::uint32_t faceplateHigh = 0x3b3a36u; // raised/moulded surface
inline constexpr std::uint32_t faceplateLow  = 0x1c1d1cu; // routed recess
inline constexpr std::uint32_t magenta       = 0xa8443cu; // oxblood enamel signal rail
inline constexpr std::uint32_t cyan          = 0x4d929du; // desaturated programmer teal
inline constexpr std::uint32_t control       = 0xd0c9bcu; // warm nickel/ivory control caps
inline constexpr std::uint32_t controlShadow = 0x6f6b63u;
inline constexpr std::uint32_t led           = 0xff573bu; // warm red panel lamps
inline constexpr std::uint32_t ledDim        = 0x491b16u;
inline constexpr std::uint32_t text          = 0xf2ede4u;
inline constexpr std::uint32_t textDim       = 0xaaa79fu;
inline constexpr std::uint32_t slot          = 0x101110u; // slider cut-out
inline constexpr std::uint32_t scope         = slot; // shared recess/scope glass

// Semantic material roles reuse the two accent families and neutral metals;
// they must not introduce extra decorative hues.
inline constexpr std::uint32_t brass         = textDim;
inline constexpr std::uint32_t brassHigh     = control;
} // namespace colour

// Geometry, in abstract panel units. The editor scales the whole description to
// whatever size the window is, so nothing here depends on a pixel density.
inline constexpr float sectionPadding = 14.0f;
inline constexpr float panelMargin = 14.0f;
inline constexpr float headerHeight = 26.0f;
inline constexpr float controlLabelHeight = 20.0f;
// Expanded cells give each function more air without turning a single fader
// cap into a paddle. The cell owns the label and tick field; the mechanical
// fader itself stays within this convincingly physical width.
inline constexpr float maximumSliderWidth = 38.0f;

// The hardware surface and keybed occupy the top 596 units. The sound strip begins to
// the right of the identity/controller cheek, the programmer tier sits below
// it, and the 61-key bed starts beside rather than underneath the bender.
inline constexpr float editorWidth = 1520.0f;
inline constexpr float displayReferenceHeight = 106.0f;
inline constexpr float soundRowTop = 14.0f;
inline constexpr float soundRowHeight = 244.0f;
inline constexpr float performanceDeckTop = 268.0f;
inline constexpr float performanceDeckHeight = 328.0f;
inline constexpr float programmerHeight = 104.0f;
// The host navigator is an add-on rail directly below the programmer: close
// enough to support the bank keys, but clearly outside the original controls.
inline constexpr float presetTop = 382.0f;
inline constexpr float presetHeight = 32.0f;
inline constexpr float panelHeight = 424.0f;
inline constexpr float keyboardHeight = 172.0f;

inline constexpr float controllerX = 14.0f;
inline constexpr float controllerWidth = 190.0f;
inline constexpr float instrumentLeft = 218.0f;
inline constexpr float instrumentRight = editorWidth - panelMargin;
inline constexpr float vectorPadX = 24.0f;
inline constexpr float vectorPadWidth = 170.0f;

// Plug-in-only controls without a useful hardware neighbour live below the
// keybed in one unmistakable add-on bay. Its quiet zones follow the hardware
// above: global model controls under the cheek, continuous voice controls
// below VOICE MODE, and pitch controls below the DCO.
inline constexpr float extensionDeckTop = panelHeight + keyboardHeight + 10.0f;
inline constexpr float extensionDeckHeight = 128.0f;
// MODEL holds three controls (Character, Aging, Quality); VOICE lends it the
// width its two knobs never used.
inline constexpr float modelZoneX = 14.0f;
inline constexpr float modelZoneWidth = 220.0f;
inline constexpr float voiceZoneX = 246.0f;
inline constexpr float voiceZoneWidth = 168.0f;
inline constexpr float pitchZoneX = 420.0f;
inline constexpr float pitchZoneWidth = 232.0f;
inline constexpr float monitorZoneX = 664.0f;
inline constexpr float monitorZoneWidth = 390.0f;
inline constexpr float operationsBarX = 1066.0f;
inline constexpr float operationsBarWidth = 440.0f;
inline constexpr float extensionControlPadding = 8.0f;
inline constexpr float operationsGroupSplitX = 1250.0f;

inline constexpr float helpStripGap = 10.0f;
inline constexpr float helpStripHeight = 48.0f;
inline constexpr float editorBottomMargin = 10.0f;
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
// composition widens from 1.44 to about 1.90.
inline constexpr int defaultEditorWidth = 1360;
inline constexpr int defaultEditorHeight = 718;
inline constexpr int minimumEditorWidth = 1200;
inline constexpr int minimumEditorHeight = 633;
inline constexpr int maximumEditorWidth = 2280;
inline constexpr int maximumEditorHeight = 1203;
inline constexpr float controlInset = 5.0f;
inline constexpr float stackGap = 8.0f;

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
    const char* displayTitle;
    const char* displayCode;
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
inline constexpr float headerPointSize = 21.0f;
inline constexpr float labelPointSize = 13.5f;
inline constexpr float buttonPointSizeMax = 14.0f;
// The original compact selector keys carry unusually small legends. Keep a
// hard floor for them while allowing the proportions of those switches to
// remain authentic at the supported minimum editor size.
inline constexpr float buttonPointSizeMin = 9.5f;
// The reference panel uses a moderately condensed grotesque. Applying a small
// horizontal scale gives the platform font that character and, importantly,
// preserves full-size legends instead of relying on ellipses.
inline constexpr float typefaceHorizontalScale = 0.92f;
// A slider's legend is drawn in a box overhanging its control by this much on
// each side, because the legend belongs to the slot rather than to the narrow
// slider cut-out.
inline constexpr float labelOverhang = 4.0f;

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

// The add-on keys below the keybed -- PANIC, INIT, the variation strengths and
// the patch-file rail -- are drawn from a fixed pixel budget rather than from
// the panel table, because they carry an icon beside the legend. This is that
// budget, in the same pixels the editor draws in.
inline constexpr float compactLegendPointSizeMax = 13.0f;
inline constexpr float compactLegendPointSizeMin = 10.5f;
inline constexpr float compactLegendHeightRatio = 0.55f;
// The key face the legend is drawn on, and the icon that shares it.
inline constexpr float compactLegendPaddingX = 5.0f;
inline constexpr float compactLegendPaddingY = 4.0f;
inline constexpr float compactLegendIconSize = 15.0f;
inline constexpr float compactLegendIconGap = 3.0f;

// The editor scales the whole description by the smaller of the two window
// ratios, so this is the factor every pixel geometry above is expressed in.
[[nodiscard]] constexpr float editorScaleFor(int width, int height) noexcept
{
    const float horizontal = static_cast<float>(width) / editorWidth;
    const float vertical = static_cast<float>(height) / editorHeight;
    return horizontal < vertical ? horizontal : vertical;
}
inline constexpr float defaultEditorScale =
    editorScaleFor(defaultEditorWidth, defaultEditorHeight);

// The legend size for a compact key of this pixel height, at this editor
// scale. The editor draws with it and the fit checks measure with it, so the
// two cannot disagree about what is on screen.
//
// The height sets the size and the scale caps it. Without the cap the tall
// action keys sat at the ceiling over the whole supported size range, so a
// key that the minimum editor width had already made 11% narrower still held
// full-size type -- which is what pushed "PANIC" past its own key face on the
// wider Linux and Windows system fonts. Type never grows past its design size
// and never stays larger than the panel around it.
[[nodiscard]] float compactLegendPointSize(float buttonHeight,
                                           float editorScale) noexcept;

// What is left for the legend on a compact key once its padding and, for a key
// that has one, its icon and the gap beside it are taken out.
[[nodiscard]] float compactLegendWidth(float buttonWidth, float buttonHeight,
                                       bool hasIcon) noexcept;

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
