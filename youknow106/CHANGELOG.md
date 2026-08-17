# Changelog

Notable customer-facing changes to YouKnow106 are recorded here.

## 1.1.0 - Unreleased

- Added a universal macOS 11+ VST3, Audio Unit, and Standalone distribution.
- Added a three-level 1x/2x/4x QUALITY control with fixed reported latency;
  new instances default to the balanced 2x mode.
- Added the complete 128-program factory bank, host preset navigation, edited
  state indication, and exact reload behavior.
- Added hardware-format `.syx` patch import, drag-and-drop, and export.
- Added MIDI Program Change mapping, modulation, sustain, pitch bend, and
  host-automatable synthesis controls.
- Added model and performance extensions including Unit Character, velocity,
  variable polyphony, unison, master tune, transpose, and chorus noise.
- Added contextual control help, live output display, panic, reset, and patch
  randomization utilities.
- Clarified the factory/custom patch rail and widened the oscillator controls
  for cleaner alignment at every supported editor size.
- Hardened host operation across transport reset, standard bypass, variable
  block sizes, concurrent state saves, and synchronous state-save callbacks.
- Preserved historical Audio Unit parameter ordering while adding QUALITY, and
  moved quality-dependent chorus coefficient work out of the audio callback.
- Added a signed, notarized, stapled PKG release path with bundled licence and
  user documentation, a build manifest, and SHA-256 checksums.
