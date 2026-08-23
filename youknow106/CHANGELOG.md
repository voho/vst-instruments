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
- Fixed panel text that could truncate (monitor voice readout, PANIC key,
  voice-lamp numbers above nine voices), dimmed disabled keys, and kept knob
  tick marks inside their controls at large editor sizes.
- Hardened host operation across transport reset, standard bypass, variable
  block sizes, concurrent state saves, and synchronous state-save callbacks.
- Added a VCF SOLVER control beside QUALITY, selecting which Runge-Kutta
  tableau advances the filter. The default Merson x2 rung is unchanged; RK4 x2
  is the same fourth-order accuracy for a fifth less work, and RK4 x1 roughly
  halves whole-engine CPU. All three keep the same filter, resonance
  calibration and self-oscillation, and the setting applies immediately without
  waiting for an idle instrument or adding latency.
- Preserved historical Audio Unit parameter ordering while adding QUALITY, and
  moved quality-dependent chorus coefficient work out of the audio callback.
- Added a signed, notarized, stapled PKG release path with bundled licence and
  user documentation, a build manifest, and SHA-256 checksums.
- Finalized the Protocodus bundle and package identifiers for v1 and documented
  migration from the earlier public Pluto Audio nightly builds.
- Fixed help text that could be cut short in the help strip, key legends that
  could be squeezed at the smallest window size, and three VOICE MODE keys that
  sat a few pixels above the programmer keys beside them.
- Fixed Chorus I and II so host automation of one no longer writes the other,
  which previously made the rendered chorus mode depend on whether the plug-in
  window was open.
- Fixed the pitch-bend lever so holding one axis with the keyboard survives
  releasing the other.
- Kept the modelled instrument warm across a transport stop, so the chassis
  warm-up runs as intended instead of restarting whenever playback stops.
- Retired the voice lamps, meters and oscilloscope whenever the host bypasses
  or deactivates the instrument, so the panel stops reporting audio nobody
  receives.
- Reduced per-sample work in the chorus and the voice loop with no change to
  the audio: the demonstration renders are bit-identical.
