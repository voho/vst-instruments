# Ghost user guide

Ghost is a circuit-modelled monophonic dual-filter analog synthesizer. This
guide describes every panel control and the instrument's MIDI behaviour.
What each law models — and which constants remain voiced choices — is
recorded control by control in the
[circuit-modelling research contract](circuit-modelling-research.md).

![Ghost](screenshots/ghost-standalone.png)

## The voice in one paragraph

Two oscillators, a triangle-cross ring modulator and one noise source feed
**two parallel audio paths**. The Filter/ADSR path runs a mix of A, B and
noise through the **Lower Filter** and **Upper Filter in series** into a VCA
driven by the Loudness envelope. The Shaper path runs a mix of everything —
ring mod included — through the one-pole BRIGHTNESS tone control into a VCA
whose gain *is* the Shaper Y output. Both paths mix to the output, or split
left/right with the SPLIT switch. Every knob and slider is a host parameter
in 0..10 panel travel; every switch carries the modelled panel's own detent
labels.

## MASTER

- **TUNE** — the whole instrument, ± a minor third; centre is concert pitch
  (MIDI 69 = 440 Hz at 8').
- **OCTAVE** — 32' 16' 8' 4', transposing both oscillators in octaves.

## OSCILLATOR A / OSCILLATOR B

- **WAVEFORM** — triangle; four rectangles (A: 50/30/15/6 %, B: 40/20/10/3 %
  duty); sawtooth. The two oscillators deliberately carry different duty
  sets.
- **SYNC** — hard-syncs Oscillator B to A. Tune B above A (INTERVAL, or the
  +1/+2 ranges) and listen to B alone for the classic tearing sweep.
- **OCTAVE/RANGE** (B) — −1, UNISON, +1, +2 in octave steps; **BASS**
  (30–300 Hz) and **WIDE** (2 Hz–10 kHz) disconnect B from the keyboard,
  TUNE, OCTAVE and the bend wheel, turning INTERVAL into a drone-pitch
  control. X/Y modulation still reaches a droning B.
- **INTERVAL** — ± a perfect fifth around centre in the octave ranges;
  slight offsets from centre are the intended two-oscillator warmth.

## TRIGGER and GATE SELECT

- **TRIGGER** — MULTIPLE re-articulates the envelopes on every key press;
  SINGLE holds one gate while any key is down and re-articulates only after
  all keys are released (legato phrasing).
- **GATE SELECT** — the envelopes' gate sources, OR'ed: **KBD** (the
  keyboard), **X** (the LFO square — auto-repeat, one gate per clock),
  **Y/EXT** (the Shaper's own gate). *At least one must be on for the
  envelopes to run at all* — with none selected, keyed notes are silent
  unless VCA BYPASS drones the path open.

## MOD X

- **ARPEGGIATOR** — OFF · RIPPLE · ARPEGGIO · LEAP. All modes scan held keys
  bottom-to-top, wrapped, one note per LFO clock. RIPPLE plays the plain
  sequence; ARPEGGIO plays it at pitch, then an octave up, then an octave
  down; LEAP cycles unison/+1/−1 octave per successive note. Engaging the
  arpeggiator clock-slaves the Shaper's rate to the LFO (except in FREE).
- **MOD SOURCE** — LFO triangle; LFO square; S+H RANDOM (sampled red noise);
  S+H Y (the Shaper sampled — a regular, patterned staircase); RED NOISE
  (continuous slow wander); OSC B (its currently selected waveform, at audio
  rate).
- **LFO/S+H RATE** — under 1 Hz to about 50 Hz; also the sample-and-hold and
  arpeggiator clock. It does not affect RED NOISE or OSC B.

## SHAPER Y

A variable-symmetry envelope/LFO (the modelled instrument's second
modulation generator).

- **MODE** — **FREE**: a free-running LFO, symmetric about zero. **KBD
  HOLD**: rises while gated and holds at maximum. **RESET**: one rise-fall
  cycle from zero, restarted by *every* key press regardless of the TRIGGER
  switch. **RUN**: the rising segment always completes; new gates are
  ignored until it has.
- **SHAPE** — the rise/fall split of the period: 0 is fast-rise, 5
  symmetric, 10 slow-rise/quick-fall. Total time never changes.
- **RATE** — the total period: several cycles per minute up to about 20 Hz.

The Shaper's output also drives the Shaper path's VCA directly, so that path
pulses in FREE mode and articulates in the envelope modes.

## WHEEL DESTINATIONS

- **MOD X TO:** — OFF · OSC A+B · OSC A · OSC A RWM (rectangle width) ·
  FILT U+L · FILT U.
- **SHAPE X WITH Y** — the Y signal envelopes the X wheel's signal:
  automatically swelling vibrato.
- **SHAPER Y TO:** — OFF · OSC A+B · OSC B · OSC B RWM · LFO RATE (the Y
  wheel sets the fastest rate, the panel knob the slowest) · FILT L.

## AUDIO MIXER

- **MASTER VOLUME** — the final attenuator for both paths.
- **BRIGHTNESS** — the Shaper path's 6 dB/octave lowpass; fully open at
  maximum.
- **Shaper path sliders** — A, B, RING, NOISE. The ring modulator is always
  Osc A's triangle × Osc B's triangle, unaffected by the WAVEFORM switches.
- **Filter path sliders** — A, B, NOISE (no ring in this path).

## UPPER FILTER U / LOWER FILTER L

The signature series dual filter.

- **MASTER** — both filters' cutoff, always.
- **LOWER ONLY** — the Lower Filter's cutoff relative to the Upper; the two
  coincide at 8, below 8 the Lower sits below the Upper.
- **RESONANCE switch** — LOW fixes the Upper Filter at Q = 0.5; VARIABLE
  slaves it to the pot.
- **RESONANCE pot** — the Lower Filter always, the Upper in VARIABLE; both
  reach self-oscillation at maximum, playable across the keyboard with KB
  AMOUNT up.
- **SLOPE** — the Upper Filter at 12 or 24 dB/octave.
- **KB AMOUNT** — keyboard tracking, zero to slightly over 100 %.
- **Lower mode** — **OUT** (a plain 2- or 4-pole lowpass remains);
  **OVERDRIVE** (a soft clipper between the filters, re-filtered by the
  Upper — the fuzz register); **BAND-PASS** (a parametric boost: a peak
  without attenuation away from it — the vocal double-peak register);
  **HIGH PASS** (resonant highpass under the lowpass).
- **TRACKING** — **FORMANT** freezes the Lower Filter's peak (disconnecting
  keyboard, both wheels and the filter envelope from it) while the Upper
  articulates: the brass/woodwind configuration. **DYNAMIC** connects
  everything.

## FILTER ENVELOPE and LOUDNESS ENVELOPE

- **AMOUNT** (filter) — bipolar around its centre: no effect at 5, rising
  contours to the right, inverted to the left; full travel spans ±2.5
  octaves straddling the cutoff. Permanently wired to the Upper Filter, to
  the Lower only in DYNAMIC.
- **A D S R** — attack, decay and release each travel 5 ms to 10 s; sustain
  is a level. The attack genuinely takes its labelled time to peak.
- **VCA BYPASS** — holds the Filter path's VCA fully open: an
  un-articulated drone.

## PERFORMANCE

- **GLIDE / GLIDE MODE** — portamento amount; OFF, AUTO (only while more
  than one key is held — fingered portamento), ON.
- **Bend wheel** — spring-loaded, ± 8 semitones at full travel.
- **MOD X / SHAPER Y wheels** — attenuators for the two modulation signals,
  "toward zero volts": a bipolar source keeps its symmetry, a unipolar one
  scales toward silence.
- **SPLIT** — the two audio paths to left (Filter/ADSR) and right (Shaper),
  as the modelled hardware's two rear jacks.
- **PANIC** — the hard stop: resets the whole engine.

The keyboard plays with **last-note priority and held-note memory**:
releasing the newest key falls back to the newest key still held, at its
own pitch, without retriggering.

## MIDI

| Message | Effect |
|---|---|
| Note on/off | Keys; velocity is ignored (the modelled keyboard has none); velocity 0 releases |
| Pitch bend | ± 8 semitones at full range |
| CC 1 (mod wheel) | The MOD X wheel parameter |
| CC 2 (breath) | The SHAPER Y wheel parameter |
| CC 120 | All sound off: the hard stop |
| CC 123 | All notes off: releases held keys through the envelopes |

## Factory programs

The modelled instrument shipped no presets; its manual instead teaches
eleven **Sound Charts**, each a drawn panel setting with a lesson attached.
Ghost's factory program bank is those charts, behind an **Init** program
that is the default voice a fresh instance already carries:

| # | Program | The lesson |
|---|---|---|
| 1 | Init | Ghost's default voice — what a new instance sounds like. |
| 2 | Preparatory Pattern | The neutral starting point every chart is dialled from. All gates are off, so it is **silent by design**. |
| 3 | Sound Sources | VCA bypass drone: audition each mixer source raw. |
| 4 | Fat Filter | Keyboard gating, resonance and the filter envelope. |
| 5 | Mod Whistle | LFO vibrato through the MOD X wheel — raise X to hear it. |
| 6 | Sync | The sync siren: Shaper Y sweeps Osc B against locked A. |
| 7 | Shake Shape | SHAPE X WITH Y: the Y wheel patterns the vibrato. |
| 8 | Sample & Hold | Stepped random voltages to the filter and oscillators. |
| 9 | Parallel Rectangles | A fifth apart, independent PWM per oscillator. |
| 10 | Arpeggio | The arpeggiator clocking gates with Shaper-path noise. |
| 11 | Noise Scale | Resonance at maximum turns noise into a keyed pitch. |
| 12 | Inverted Guitar | The inverted filter envelope pluck. |

Selecting a program writes its chart onto every panel control (and pulls
the performance wheels fully back, as the charts instruct), so it is also a
readable lesson: open the editor and see how the sound is made.

## Hearing the instrument

The ten committed renders under [`Docs/audio`](audio/README.md) demonstrate
one signature mechanism each, rendered from the same engine the plug-in
runs.
