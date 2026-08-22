// The factory programs, in two banks.
//
// The modelled instrument shipped with no presets at all: its owner's manual
// teaches eleven "Sound Charts" instead — panel drawings the player dials in
// by hand, from the silent Preparatory Pattern through the Inverted Guitar.
// Those charts are the first bank, voiced from the manual's own drawings and
// lesson text (sources in Docs/circuit-modelling-research.md), behind an
// opening Init program that is the engine's default voice — so a fresh
// instance, which hosts label with program 0, really is what program 0
// sounds like.
//
// The second bank is Ghostar's own: playable programs built on the modelled
// panel, for players who want to hear the instrument rather than be taught
// it. They make no historical claim — the hardware had no presets to copy —
// and each one is named for the mechanism it puts in the foreground.
//
// Both live in the JUCE-free core so the DSP suite can render every one.
#pragma once

#include "GhostarEngine.h"

namespace ghostar
{

// Which bank a program belongs to, for a browser that groups them. Hosts
// see one flat list; the index order is Sound Charts first, then Programs.
enum class PresetBank
{
    SoundCharts,
    Programs
};

[[nodiscard]] int factoryPresetCount() noexcept;
// nullptr outside [0, factoryPresetCount()).
[[nodiscard]] const char* factoryPresetName(int index) noexcept;
// The default voice (Init) for an out-of-range index.
[[nodiscard]] EngineParameters factoryPresetParameters(int index) noexcept;
// SoundCharts for an out-of-range index, since Init leads that bank.
[[nodiscard]] PresetBank factoryPresetBank(int index) noexcept;
// A one-line description of what the program is doing, for a browser's
// detail line and for the user guide's table. Never nullptr.
[[nodiscard]] const char* factoryPresetDescription(int index) noexcept;

} // namespace ghostar
