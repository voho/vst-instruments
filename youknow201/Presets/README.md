# YouKnow201 presets

The plug-in ships 13 programs built into the binary
(`Source/DSP/YouKnow201Presets.cpp`), INIT PATCH first. They are original
sounds programmed against the engine: the modelled instrument's 64 factory
patches are Roland's data, published nowhere as parameter values, and none of
that data ships in this repository. The INIT PATCH reproduces the documented
initialization behavior — only OSC 1 is heard, because the balance sits fully
left.

Host sessions store the full parameter state, so any edited sound saves with
the project. A loader for user-supplied SysEx dumps of real-hardware patches
is a natural follow-up recorded in the
[research contract](../Docs/sh201-replica-research.md).
