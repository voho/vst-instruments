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
inline constexpr auto keyMode      = "keyMode";
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
inline constexpr auto chorus       = "chorus";

// Controls the modelled instrument does not have. They sit in their own strip
// below the panel so the panel itself stays honest, and each defaults to the
// value that reproduces hardware behaviour.
inline constexpr auto transpose    = "transpose";
inline constexpr auto masterTune   = "masterTune";
inline constexpr auto velocity     = "velocity";
inline constexpr auto calibration  = "calibration";
inline constexpr auto chorusNoise  = "chorusNoise";
inline constexpr auto polyphony    = "polyphony";
inline constexpr auto hq           = "hq";
} // namespace parameters

namespace panel
{

// The front panel as data rather than as layout code.
//
// The section order, the controls inside each section, and the choice of slider
// against switch reproduce the modelled instrument's panel. What is deliberately
// not reproduced is its livery: the palette below is this project's own, so the
// panel reads as a relative rather than a copy.

// Panel palette, 0xRRGGBB.
namespace colour
{
inline constexpr std::uint32_t faceplate     = 0x22252au; // matte slate charcoal
inline constexpr std::uint32_t faceplateHigh = 0x2c3037u; // raised plastic
inline constexpr std::uint32_t faceplateLow  = 0x191b1fu; // recessed plastic
inline constexpr std::uint32_t magenta       = 0xff2a7au; // section highlight
inline constexpr std::uint32_t cyan          = 0x00e5ffu; // section highlight
inline constexpr std::uint32_t control       = 0xd0d5ddu; // cool silver-grey caps
inline constexpr std::uint32_t controlShadow = 0x8b929cu;
inline constexpr std::uint32_t led           = 0x39ff14u; // bright neon green
inline constexpr std::uint32_t ledDim        = 0x163d0cu;
inline constexpr std::uint32_t text          = 0xe8ecf1u;
inline constexpr std::uint32_t textDim       = 0x8b929cu;
inline constexpr std::uint32_t slot          = 0x121417u; // slider cut-out
} // namespace colour

// Geometry, in abstract panel units. The editor scales the whole description to
// whatever size the window is, so nothing here depends on a pixel density.
inline constexpr float slotWidth = 36.0f;
inline constexpr float sectionPadding = 14.0f;
inline constexpr float sectionGap = 8.0f;
inline constexpr float panelMargin = 10.0f;
inline constexpr float headerTop = 6.0f;
inline constexpr float headerHeight = 26.0f;
inline constexpr float controlTop = 62.0f;
inline constexpr float controlHeight = 200.0f;
inline constexpr float labelTop = 268.0f;
inline constexpr float labelHeight = 22.0f;
inline constexpr float sectionBottom = 300.0f;
inline constexpr float utilityTop = 310.0f;
inline constexpr float utilityHeight = 46.0f;
inline constexpr float panelHeight = 364.0f;
inline constexpr float keyboardHeight = 92.0f;
inline constexpr float controlInset = 5.0f;
inline constexpr float stackGap = 6.0f;

enum class ControlKind
{
    Slider,  // vertical travel
    Toggle,  // one independent latching button
    Radio,   // one button of a mutually exclusive group
    Steps    // a slider with named detents
};

// Which highlight a section carries. Alternating them is what makes the panel
// scannable at a glance in a dark room.
enum class Accent { Magenta, Cyan };

struct Section
{
    const char* name;
    Accent accent;
    int slots;
    float x;
    float width;
};

struct Control
{
    const char* parameterId;
    const char* label;
    ControlKind kind;
    int section;
    float x;
    float y;
    float width;
    float height;
    // Index of the mutually exclusive group this button belongs to, or -1, and
    // which value of the group's parameter it selects.
    int group;
    int groupValue;
};

inline constexpr int sectionCount = 10;
inline constexpr int controlCount = 39;

[[nodiscard]] const std::array<Section, sectionCount>& sections() noexcept;
[[nodiscard]] const std::array<Control, controlCount>& controls() noexcept;
[[nodiscard]] float panelWidth() noexcept;

// True when every control lies inside its own section, no two controls overlap,
// and every radio group is contiguous and complete. The suite asserts this
// rather than trusting the table by eye.
[[nodiscard]] bool layoutIsConsistent() noexcept;

} // namespace panel
} // namespace youknow106
