# Mars sound-design recipes

Mars state is stored by the host. These original recipes are starting points
for the version-1.4 parameter set; percentages are approximate and are meant to
be adjusted by ear. Parameters not listed in a recipe remain at their
version-1.4 defaults; see the [complete parameter contract](../README.md#exact-47-parameter-contract).
Both oscillator mixer switches and HQ oversampling are On unless a recipe says
otherwise. Oscillators use the Moog-like VCO model, Mono is Off, and polyphony
is limited to 16 render voices unless a recipe says otherwise. HQ is a
persisted quality setting, not part of preset randomization, and changes take
effect after the engine becomes idle.

These recipes assume the Mars 1.4 signal path. HQ runs the complete nonlinear
voice, VCA, and dual variable-clock BBD ensemble at 4x in 44.1/48 kHz sessions,
2x at 88.2/96 kHz, and natively at 176.4 kHz and above. The Moog-like VCO has
the measured saw contour and full seeded pitch wander; the Juno-like DCO has
clocked cycle timing, a divider-locked sub, prewarped reconstruction filtering,
and much tighter fundamental drift. All oscillator discontinuities, including
the triangle driver and DCO sub, receive the four-sample integrated B-spline
correction. Because these are distinct running models rather than labels, model
choice is an intentional part of every recipe below.

The `1%`, `10%`, and `100%` randomizer buttons draw independent legal targets
in normalized parameter space and move the current sound toward each target by
that amount. `1%` and `10%` are bounded variations of the current recipe;
`100%` is a full-range draw. Output level, oscillator On/Off state, HQ, Mono,
and the physical polyphony limit are preserved. Positive frequency and time
controls are smoothed multiplicatively, so even a large randomizer move follows
their perceptual range without a raw parameter step.

## Red Planet Brass

- Oscillator I `Moog-like VCO` / `Saw`, octave 0; oscillator II
  `Moog-like VCO` / `Saw`, octave 0, tune 0, fine +7 ct;
  oscillator balance 48%, noise 2%, cross modulation 3%.
- `Ladder`; cutoff 1.4 kHz, resonance 24%, drive 32%, filter envelope amount
  +55%, key tracking 55%.
- Filter ADSR 8 ms / 480 ms / 18% / 260 ms; amplifier ADSR 5 ms / 520 ms / 72%
  / 320 ms.
- `Poly`; drift 22%, spread 34%, velocity response 72%; ensemble mix 12%, rate
  0.45 Hz.

## Phobos Strings

- Oscillator I `Juno-like DCO` / `Pulse`; oscillator II `Juno-like DCO` /
  `Saw`, fine -8 ct; pulse width 42%, oscillator
  balance 44%, noise 3%.
- `SEM`; shape 18% (mostly low-pass), cutoff 2.6 kHz, resonance 32%, drive
  16%, filter envelope amount +24%, key tracking 48%.
- Filter ADSR 420 ms / 1.8 s / 55% / 2.2 s; amplifier ADSR 680 ms / 1.5 s / 82%
  / 3.0 s.
- Sine LFO at 0.24 Hz, PWM depth 38%, filter depth 0.08 oct; `Poly`, drift 35%,
  spread 78%; ensemble mix 52%, rate 0.31 Hz.

## Dust Engine Bass

- Oscillator I `Moog-like VCO` / `Saw`, octave -1; oscillator II
  `Moog-like VCO` / `Pulse`, octave -1, fine +4 ct; oscillator
  balance 38%, sub level 62%, noise 2%, pulse width 47%.
- `Ladder`; cutoff 620 Hz, resonance 20%, drive 68%, filter envelope amount
  +62%, key tracking 32%.
- Filter ADSR 2 ms / 190 ms / 8% / 120 ms; amplifier ADSR 2 ms / 260 ms / 68%
  / 150 ms.
- `Mono` override On; drift 14%, spread 12%, glide 45 ms, velocity response 62%; ensemble
  mix 0%, output to taste.

## Polar Dawn Pad

- Oscillator I `Moog-like VCO` / `Triangle`; oscillator II `Juno-like DCO` /
  `Pulse`, octave +1, fine -11 ct; oscillator balance
  58%, pulse width 56%, noise 5%.
- `SEM`; shape 58% (notch-to-high-pass side of centre), cutoff 4.2 kHz,
  resonance 44%, drive 18%, filter envelope amount -18%, key tracking 40%.
- Filter ADSR 1.4 s / 3.1 s / 66% / 4.5 s; amplifier ADSR 1.8 s / 2.4 s / 86%
  / 5.0 s.
- Triangle LFO at 0.11 Hz, pitch depth 3 ct, filter depth 0.12 oct, PWM depth
  25%; `Unison`, 4 voices, drift 42%, spread 88%; ensemble mix 58%, rate
  0.21 Hz.

## Colony Fifths

- `Fifth` voice mode; oscillator I `Juno-like DCO` / `Saw`, oscillator II
  `Moog-like VCO` / `Triangle`, fine +5 ct; oscillator
  balance 43%, cross modulation 8%, sub level 8%.
- `SEM`; shape 10%, cutoff 3.2 kHz, resonance 26%, drive 24%, filter envelope
  amount +38%, key tracking 65%.
- Filter ADSR 18 ms / 720 ms / 28% / 900 ms; amplifier ADSR 10 ms / 620 ms /
  74% / 1.1 s.
- Drift 26%, spread 70%, velocity response 76%; ensemble mix 27%, rate 0.52 Hz.

## Copper Keys

- Oscillator I `Juno-like DCO` / `Pulse`; oscillator II `Juno-like DCO` /
  `Saw`, fine +3 ct; pulse width 50%, oscillator
  balance 35%, sub level 10%.
- `Ladder`; cutoff 1.8 kHz, resonance 31%, drive 38%, filter envelope amount
  +48%, key tracking 58%.
- Filter ADSR 3 ms / 310 ms / 12% / 230 ms; amplifier ADSR 2 ms / 640 ms / 42%
  / 420 ms.
- Triangle LFO at 4.6 Hz, pitch depth 2 ct, PWM depth 8%; `Poly`, drift 18%,
  spread 42%, velocity response 84%; ensemble mix 16%.

## Crater Motion

- Oscillator I `Moog-like VCO` / `Pulse`; oscillator II `Juno-like DCO` /
  `Pulse`, tune +7, fine -6 ct; pulse width 36%,
  oscillator balance 52%, cross modulation 16%.
- `SEM`; shape 48% (near notch), cutoff 2.1 kHz, resonance 52%, drive
  28%, filter envelope amount +22%, key tracking 35%.
- Sample & hold LFO at 2.7 Hz, filter depth 0.42 oct, PWM depth 20%, pitch depth
  4 ct.
- `Poly`; drift 31%, spread 64%, glide 70 ms, velocity response 66%; ensemble
  mix 33%, rate 0.66 Hz.

## Glass Horizon

- Oscillator I `Juno-like DCO` / `Triangle`, octave 0; oscillator II
  `Juno-like DCO` / `Triangle`, octave 0, tune +12, fine +2
  ct; oscillator balance 50%, noise 1%, cross modulation 2%.
- `SEM`; shape 82% (toward high-pass), cutoff 5.8 kHz, resonance 18%, drive
  8%, filter envelope amount +14%, key tracking 72%.
- Filter ADSR 16 ms / 1.2 s / 48% / 1.6 s; amplifier ADSR 12 ms / 1.1 s / 72%
  / 2.2 s.
- Sine LFO at 5.2 Hz, pitch depth 5 ct; `Poly`, drift 12%, spread 58%; ensemble
  mix 24%, rate 0.39 Hz.

Only original parameter settings and redistributable assets belong in this
directory. Exported factory presets should record the Mars version they target
so later parameter migrations remain explicit.
