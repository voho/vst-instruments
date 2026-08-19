# Changelog

Notable customer-facing changes to YouKnow106 are recorded here.

## 1.1.0 - Unreleased

- Rebalanced the sub oscillator against the noise source in the voice mixer.
  Noise-forward programs — snare, toms, timpani and other percussive patches —
  now let the noise through with the bite the hardware reference recordings
  show, instead of sitting behind the sub octave. Patches that feature the sub
  heavily are correspondingly quieter and may want their SUB slider raised.
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
- Fixed panel text that could truncate (monitor voice readout, PANIC key,
  voice-lamp numbers above nine voices), dimmed disabled keys, and kept knob
  tick marks inside their controls at large editor sizes.
- Hardened host operation across transport reset, standard bypass, variable
  block sizes, concurrent state saves, and synchronous state-save callbacks.
- Preserved historical Audio Unit parameter ordering while adding QUALITY, and
  moved quality-dependent chorus coefficient work out of the audio callback.
- Added a signed, notarized, stapled PKG release path with bundled licence and
  user documentation, a build manifest, and SHA-256 checksums.
- Finalized the Protocodus bundle and package identifiers for v1 and documented
  migration from the earlier public Pluto Audio nightly builds.
