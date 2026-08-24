# Changelog

Notable customer-facing changes to YouKnow106 are recorded here.

## 1.1.0 - Unreleased

- Added a universal macOS 11+ VST3, Audio Unit, and Standalone distribution.
- Added a three-level 1x/2x/4x QUALITY control with fixed reported latency;
  new instances default to the cheapest 1x mode, with 2x and 4x one menu away.
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
- Added a VCF SOLVER control beside QUALITY, choosing how much arithmetic the
  filter's solver spends per internal sample. New instances use Normal, which
  roughly halves whole-engine CPU; High and Max cost more, and Max is what
  every earlier release ran. All three keep the same filter, resonance
  calibration and self-oscillation, a blind listening test found no audible
  difference between them, and the setting applies immediately without waiting
  for an idle instrument or adding latency.
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
- Fixed sustain (CC 64) to latch on any non-zero value instead of only at 64
  and above, matching the original's MIDI implementation chart, which prints
  hold off at zero and hold on for 1–127. A half-pressed pedal now holds.
- Fixed the channel-mode messages CC 124–127 so they release the keyboard like
  all-notes-off, which the same chart says the instrument does with every mode
  message from 123 up. They were previously ignored, leaving notes hanging in
  hosts that end playback with an omni or poly message rather than CC 123.
- Kept the modelled instrument warm across a transport stop, so the chassis
  warm-up runs as intended instead of restarting whenever playback stops.
- Retired the voice lamps, meters and oscilloscope whenever the host bypasses
  or deactivates the instrument, so the panel stops reporting audio nobody
  receives.
- Reduced per-sample work in the chorus and the voice loop with no change to
  the audio: the demonstration renders are bit-identical.
