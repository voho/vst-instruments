# Changelog

Notable customer-facing changes to Ghost are recorded here.

## 0.9.0 - Unreleased

- First complete instrument: VST3, Audio Unit and Standalone builds for
  macOS (universal), Linux and Windows, wrapping the circuit-modelled engine.
- Modelled voice: two bandlimited oscillators with one-directional hard sync
  and the panel's exact duty-cycle sets, ± a-perfect-fifth interval detune
  with BASS/WIDE drone ranges, triangle-cross ring modulator,
  white-plus-pink noise, and two parallel audio paths with independent VCAs.
- The signature series dual filter: a lower multimode section (parametric
  boost, inter-filter overdrive, resonant highpass, or out) sliding against
  a 12/24 dB upper lowpass, one resonance control with the LOW/VARIABLE
  switch, ~110 % keyboard tracking, diode-bounded self-oscillation, the
  frozen-formant tracking mode, and a bipolar ±2.5-octave filter envelope.
- Modulation: MOD X with six sources (LFO triangle/square, random and
  Y-patterned sample-and-hold, red-noise drift, Osc B) and the
  RIPPLE/ARPEGGIO/LEAP arpeggiator; the Shaper Y variable-rate integrator
  with FREE/KBD HOLD/RESET/RUN modes and rise/fall symmetry; both
  wheel-destination buses including SHAPE X WITH Y and Y-to-LFO-rate.
- Performance layer: last-note keying with held-note fallback that never
  retriggers, SINGLE/MULTIPLE triggering behind OR'ed gate sources
  (keyboard, LFO auto-repeat, the Shaper's own gate), AUTO glide, VCA
  bypass droning, a spring-loaded ±8-semitone bend wheel and assignable
  X/Y performance wheels.
- Every modelled panel control is a host-automatable parameter with the
  silkscreen's own detent labels (the spring-loaded bend wheel is the one
  momentary exception: it rides MIDI pitch bend, not a parameter); MIDI
  CC1/CC2 ride the X and Y wheels, CC120 stops all sound while keeping
  controller positions, and CC123 releases held keys through the
  envelopes.
- Factory programs: the modelled instrument's manual teaches eleven Sound
  Charts instead of shipping presets, and those charts are Ghost's program
  bank — from the silent Preparatory Pattern the lessons all start from,
  through Fat Filter, Sync and Sample & Hold, to the Inverted Guitar —
  behind an Init program that is the default voice itself. The selected
  program survives session save and restore.
- Travel smoothing: every continuous panel control and both performance
  wheels glide to new values over ~25 ms, so host automation at any block
  size and 7-bit MIDI CCs never step the audio; switches stay immediate,
  and a silent instrument adopts restored settings exactly. Justified and
  measured by the zipper audit recorded in the best-in-class plan.
- Ten committed demonstration renders under `Docs/audio`, regenerated
  nightly from the shipping engine.
