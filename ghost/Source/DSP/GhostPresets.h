// The factory programs. The modelled instrument shipped with no presets:
// its owner's manual instead teaches eleven "Sound Charts" — panel drawings
// the player dials in by hand, from the silent Preparatory Pattern through
// the Inverted Guitar. Ghost's factory bank is those eleven charts, voiced
// from the manual's own drawings and lesson text (see
// Docs/circuit-modelling-research.md for the sources), behind an opening
// Init program that is the engine's default voice — so a fresh instance,
// which hosts label with program 0, really is what program 0 sounds like.
// They live in the JUCE-free core so the DSP suite can render every one.
#pragma once

#include "GhostEngine.h"

namespace ghost
{

[[nodiscard]] int factoryPresetCount() noexcept;
// nullptr outside [0, factoryPresetCount()).
[[nodiscard]] const char* factoryPresetName(int index) noexcept;
// The default voice (Init) for an out-of-range index.
[[nodiscard]] EngineParameters factoryPresetParameters(int index) noexcept;

} // namespace ghost
