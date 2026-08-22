# Changelog

Notable customer-facing changes to Ghostar are recorded here.

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
  Charts instead of shipping presets, and those charts are Ghostar's program
  bank — from the silent Preparatory Pattern the lessons all start from,
  through Fat Filter, Sync and Sample & Hold, to the Inverted Guitar —
  behind an Init program that is the default voice itself. The selected
  program survives session save and restore.
- Travel smoothing: every continuous panel control and both performance
  wheels glide to new values over ~25 ms, so host automation at any block
  size and 7-bit MIDI CCs never step the audio; switches stay immediate,
  and a silent instrument adopts restored settings exactly. Justified and
  measured by the zipper audit recorded in the best-in-class plan.
- Twelve committed demonstration renders under `Docs/audio`, regenerated
  nightly from the shipping engine.
- **Derived, not voiced.** The character-defining laws that were first-pass
  choices are now computed from primary documents, and several of them
  changed the instrument audibly:
  - **Resonance** follows the filter chip's own exponential Q scale through
    the panel's actual pot network, anchored by the manual's "LOW fixes
    Q = 0.5". Resonance is now gentle through the middle of the travel and
    steep at the top — Q at half travel is 1.5 where the old law gave 5.7 —
    and the two filters have genuinely different curves, as their different
    bias networks require.
  - **Envelope times** read the panel's 5 ms–10 s as the RC time constant it
    is, so long decays and releases last about 2.8 times longer than before;
    the attack aims where the timer's output pin actually charges to, making
    it longer and flatter-topped.
  - **Both filter nonlinearities** are the BA130 diode pair's own law rather
    than a tanh stand-in. The overdrive stage is solved as its circuit, so
    past the knee it keeps climbing with drive instead of flattening.
  - **Keyboard tracking** is 108 %, computed from the CV ladder's resistors,
    which reproduces the manual's "slightly over 100 %" independently.
- **Audio quality.** Every waveform discontinuity is bandlimited as a
  sub-sample event — including the hard-sync reset, which was uncorrected,
  and the triangle's corners, which had no correction at all — and the voice
  core runs at 4x with a two-stage decimation chain. Measured against a
  16x ground truth, the worst-case aliasing strokes improved by 40 to 90 dB
  and every stroke whose reference converges now sits at or below −80 dB.
  The filter's nonlinearity is a term of the continuous system rather than a
  per-sample map, so a patch sounds the same at every host sample rate —
  which it previously did not.
- Seventeen **Ghostar Programs** join the eleven Sound Charts: playable
  voicings, each foregrounding one mechanism, level-matched to each other.
- The editor is rebuilt around the modelled instrument's own panel: MOD X
  and SHAPER Y as full-height columns on the right, WHEEL DESTINATIONS
  under MASTER, the two filters as one block and the two envelopes as
  another, and GLIDE with the three levers on their own sub-panel beside
  the keys — which now have the bottom of the window to themselves. The
  livery is the panel's: charcoal, grey knob caps with black pointers and
  travel arcs, white silkscreen, paddle switches whose thrown half stands
  pale. Every control is captioned directly above itself and carries a
  tooltip; knobs and faders show the silkscreen's 0–10 while they move.
  A program browser reaches both banks with each program's description, and
  a gate lamp shows when a gate source is holding the envelopes open.
