// The factory programs. The modelled instrument shipped with no presets:
// its owner's manual instead teaches eleven "Sound Charts" — panel drawings
// the player dials in by hand, from the silent Preparatory Pattern through
// the Inverted Guitar. Ghost's factory bank is exactly those eleven charts,
// voiced from the manual's own drawings and lesson text (see
// Docs/circuit-modelling-research.md for the sources). They live in the
// JUCE-free core so the DSP suite can render every one of them.
#pragma once

#include "GhostEngine.h"

namespace ghost
{

[[nodiscard]] int factoryPresetCount() noexcept;
// nullptr outside [0, factoryPresetCount()).
[[nodiscard]] const char* factoryPresetName(int index) noexcept;
// The Preparatory Pattern for an out-of-range index: the manual's own
// "easily remembered starting point".
[[nodiscard]] EngineParameters factoryPresetParameters(int index) noexcept;

} // namespace ghost
