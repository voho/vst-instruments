// Original factory bank for Septum. The modelled instrument's 64 factory
// patches are Roland's data and do not ship here; these presets are original
// sounds programmed against the engine, loosely honouring the *kinds* of
// sound the hardware's patch list documents (supersaw leads, FB-OSC leads,
// sync leads, S&H effects, bass, pads). The effect templates implement the
// sixteen settled template names from the owner's manual and leaflet with
// this project's own voiced parameter values (OQ-12).

#pragma once

#include "SeptumPatch.h"

#include <vector>

namespace septum
{

struct NamedPatch
{
    const char* name;
    Patch patch;
};

// The INIT PATCH behavior is documented: only OSC 1 is heard, because the
// balance sits fully left.
[[nodiscard]] Patch initPatch();

// The original preset bank, INIT first.
[[nodiscard]] const std::vector<NamedPatch>& factoryPatches();

// Apply one of the eight delay templates (0-7: Simple Delay, 1 Shot Delay,
// Medium Delay, Long Delay, Analog Delay, Mod Delay, Chorus 1, Chorus 2).
void applyDelayTemplate (Patch& patch, int index);

// Apply one of the eight reverb templates (0-7: Room 1, Room 2, Studio 1,
// Studio 2, Hall 1, Hall 2, Plate 1, Plate 2).
void applyReverbTemplate (Patch& patch, int index);

[[nodiscard]] const char* delayTemplateName (int index);
[[nodiscard]] const char* reverbTemplateName (int index);

} // namespace septum
