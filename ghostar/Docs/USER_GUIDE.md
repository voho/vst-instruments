# Ghostar user guide

Ghostar is a circuit-modelled monophonic dual-filter analog synthesizer. This
guide describes every panel control and the instrument's MIDI behaviour.
What each law models — and which constants remain voiced choices — is
recorded control by control in the
[circuit-modelling research contract](circuit-modelling-research.md).

![Ghostar](screenshots/ghostar-standalone.png)

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

- **TRIGGER** — with KBD selected, MULTIPLE re-articulates the envelopes
  on every key press; SINGLE holds one gate while any key is down and
  re-articulates only after all keys are released (legato phrasing). With
  KBD off, key presses do not re-articulate at all: X and Y/EXT patches
  articulate on their own gate edges, as the hardware's selected-bus
  trigger derivation does.
- **GATE SELECT** — the envelopes' gate sources, OR'ed: **KBD** (the
  keyboard), **X** (the LFO square — auto-repeat, one gate per clock),
  **Y/EXT** (the Shaper's own gate). *At least one must be on for the
  envelopes to run at all* — with none selected, the Filter/ADSR path is
  silent unless VCA BYPASS drones it open. The Shaper path is the
  exception: its VCA follows the Shaper contour itself, so raised
  Shaper-path sliders keep sounding (FREE mode cycles on its own, and
  RESET restarts on every key press) without any gate selected.

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
  as the modelled hardware's two rear jacks. On a mono output bus the two
  paths stay summed — there is no right jack to split onto.
- **PANIC** — the hard stop: kills every sounding voice and resets the
  engine's voice state. Panel settings are parameters and survive, so a
  drone dialled in with VCA BYPASS or a raised Shaper path in a
  self-running mode starts again immediately — pull those controls down
  to stop it. The X/Y wheels likewise keep their positions, and a bend
  still held on a wheel reapplies itself.

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
| CC 120 | All sound off: kills every sounding voice, keeping wheel and bend positions (a drone dialled in with VCA BYPASS or raised Shaper-path sliders starts again immediately — pull those controls down to stop it) |
| CC 123 | All notes off: releases held keys through the envelopes |

## Factory programs

The bank has two halves. Hosts see one flat list; the editor's browser
groups them.

### Sound Charts — the manual's own lessons

The modelled instrument shipped no presets: its manual instead teaches
eleven **Sound Charts**, each a drawn panel setting with a lesson attached.
Those are programs 2–12, behind an **Init** program that is the default
voice a fresh instance already carries.

| # | Program | The lesson |
|---|---|---|
| 1 | Init | Ghostar's default voice — what a new instance sounds like. |
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

### Ghostar Programs — the performance bank

Programs 13–29 are Ghostar's own, for playing rather than for teaching.
They make no historical claim — the hardware had no presets to copy — and
each one puts a single mechanism in the foreground, so the bank doubles as
a tour of what the panel can do. They are level-matched to each other, so
you can step through them without reaching for the volume.

| # | Program | What it foregrounds |
|---|---|---|
| 13 | Spirit Bass | The 24 dB lowpass under a fast filter envelope. |
| 14 | Vocal Pair | The signature dual filter: a boost peak sliding under the lowpass. |
| 15 | Formant Reed | FORMANT freezes the lower peak while the upper articulates. |
| 16 | Growl Bass | The inter-filter clipper, re-filtered by the upper lowpass. |
| 17 | Sync Lead | Hard sync torn open by the Shaper — ride the Y wheel. |
| 18 | Ring Bell | The triangle-cross ring modulator, struck and left to fall. |
| 19 | Two-Path Drift | An enveloped line left, a free-running ring drone right (SPLIT is on). |
| 20 | Bypass Pad | VCA BYPASS holds the path open; red noise wanders the cutoff. Held keys drone. |
| 21 | Leap Sequence | LEAP cycles each note through unison, up and down an octave. Hold a chord. |
| 22 | Patterned Steps | S+H sampling the Shaper: a repeating figure, not a wander. |
| 23 | Glide Lead | AUTO glide — legato only when two keys overlap. X wheel adds vibrato. |
| 24 | Hollow Fifth | A fifth apart with independent PWM from the X and Y buses. |
| 25 | Noise Flute | Noise rung at the cutoff, with the cutoff tracking the keys. |
| 26 | Sub and Lead | Osc B parked in BASS as a fixed sub beneath a played lead. |
| 27 | Shaper Pulse | RUN chops the second path into a rhythm keys cannot interrupt. |
| 28 | Thunder | WIDE below the keyboard, noise through the overdrive stage. |
| 29 | Hollow Ghost | The resonant highpass against the lowpass: the double peak. |

Selecting any program writes it onto every panel control (and pulls the
performance wheels fully back, as the charts instruct), so every one is
also readable: open the editor and see how the sound is made.

## Hearing the instrument

The twelve committed renders under [`Docs/audio`](audio/README.md) demonstrate
one signature mechanism each, rendered from the same engine the plug-in
runs — including a resonance sweep that lets you hear the derived resonance
law's shape, and a tour of six of the Ghostar Programs.
