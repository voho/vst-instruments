// Original factory bank for Septum. The modelled instrument's 64 factory
// patches are Roland's data and do not ship here; these presets are original
// sounds programmed against the engine, loosely honouring the *kinds* of
// sound the hardware's patch list documents (supersaw leads, FB-OSC leads,
// sync leads, S&H effects, bass, pads). The effect templates implement the
// sixteen settled template names from the owner's manual and leaflet with
// this project's own voiced parameter values (OQ-12).

#pragma once

#include "SeptumPatch.h"

#include <string>
#include <vector>

namespace septum
{

struct NamedPatch
{
    // Owns its storage. It used to be a `const char*`, and the 32 User slots
    // built theirs from a local std::string that died at the end of the loop
    // iteration, so every one of those names was a dangling pointer by the
    // time a host asked for it.
    std::string name;
    Patch patch;
};

// The INIT PATCH behavior is documented: only OSC 1 is heard, because the
// balance sits fully left.
[[nodiscard]] Patch initPatch();

// The shipped bank: 32 original programs in the PRESET A-1..D-8 positions
// followed by 32 initialised User slots, mirroring the instrument's own bank
// layout. None of Roland's factory patch data ships here.
[[nodiscard]] const std::vector<NamedPatch>& factoryPatches();

// Apply one of the eight delay templates (0-7: Simple Delay, 1 Shot Delay,
// Medium Delay, Long Delay, Analog Delay, Mod Delay, Chorus 1, Chorus 2).
void applyDelayTemplate (Patch& patch, int index);

// Apply one of the eight reverb templates (0-7: Room 1, Room 2, Studio 1,
// Studio 2, Hall 1, Hall 2, Plate 1, Plate 2).
void applyReverbTemplate (Patch& patch, int index);

[[nodiscard]] const char* delayTemplateName (int index);
[[nodiscard]] const char* reverbTemplateName (int index);

// Arpeggio styles. The modelled instrument ships 32 templates in four banks;
// their grids are Roland's data, unpublished and not shipped here, so these
// are original patterns written against the same settled 32 x 16 grid. The
// hardware's panel also only *selects* a template — the manual says editing a
// style needs the SH-201 Editor — so a selector is the faithful surface.
struct NamedArpeggioStyle
{
    const char* name;
    ArpeggioStyle style;
};

[[nodiscard]] const std::vector<NamedArpeggioStyle>& arpeggioStyles();
[[nodiscard]] const char* arpeggioStyleName (int index);
void applyArpeggioStyle (Patch& patch, int index);

} // namespace septum
