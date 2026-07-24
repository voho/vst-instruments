# Drumalor presets

The current plug-in stores its
[13-voice, 95-parameter kit state](../README.md#voices-midi-notes-and-controls)
in the host session and exposes the version-1 defaults on first launch. That is
seven controls per voice - Character A, Character B, Pitch, Decay, Level, Pan
and Choke Group - plus the four kit controls: Humanise, Bus Drive, Bus
Compression and Output. Factory preset files are intentionally not baked into
the project yet: keeping them out avoids silently changing sound design while
the synthesis parameters are still evolving.

A session saved by version 1.0 restores correctly. Its 53 stored parameters keep
their values and host indices, and the controls added in 1.1 come back at
defaults that reproduce the 1.0 sound.

When presets are added, use descriptive names such as `Dusty Machine`, `Hard
Electro`, or `Soft Circuits`. Only include original settings. Drumalor contains
no samples, and presets must not introduce uncleared sample or impulse-response
assets.
