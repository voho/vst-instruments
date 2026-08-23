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
ring mod included — into a VCA whose current behavioral gain follows the
Shaper Y output, then the passive BRIGHTNESS shelf. The original two-BC173/
CEM3360 control transfer remains measurement-owned. The hardware's normalled
main jack passively joins the two Master wipers, so BRIGHTNESS and both paths
cross-load one another; SPLIT isolates them at full level left/right. Every
knob and slider is a host parameter in 0..10 panel travel; every switch
carries the modelled panel's own detent labels.

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

- **TRIGGER** — MULTIPLE routes every raw new-key KT pulse through the
  original's nominal 10 ms reset/release lane before KBD gate selection.
  Attack resumes only if a selected gate remains high; with KBD off, a key
  can therefore still re-articulate an envelope held by X or Y. SINGLE has
  no KT branch: it attacks only on a genuine selected-bus low-to-high rise,
  so legato presses and a keyboard rise hidden beneath held X/Y do nothing.
- **GATE SELECT** — the envelopes' gate sources, OR'ed: **KBD** (the
  keyboard), **X** (the LFO square — auto-repeat, one gate per clock),
  **Y/EXT** (the Shaper's own gate). In FREE that gate stays high for the
  whole rising leg and low for the whole falling leg. In KBD HOLD it rises only
  on a new cycle and falls at hold/release; in RESET and RUN it rises only with
  the active rising leg. Exact self-Y feedback edge acceptance remains open.
  *At least one must be on for the envelopes to run at all* — with none selected, the Filter/ADSR
  path is silent unless VCA BYPASS drones it open. The Shaper path is the
  exception: its VCA follows the Shaper contour itself, so raised Shaper-path
  sliders keep sounding (FREE mode cycles on its own, and RESET restarts on
  every key press) without any gate selected.
  X and Y/EXT have their own nominal 5 ms edge branches, so their rises still
  retrigger beneath a gate already held high by KBD or another source. They
  cannot shorten a coincident 10 ms keyboard/arpeggiator notch.

## MOD X

- **ARPEGGIATOR** — OFF · RIPPLE · ARPEGGIO · LEAP. All modes scan held keys
  bottom-to-top, wrapped, one note per LFO clock. RIPPLE plays the plain
  sequence; ARPEGGIO plays it at pitch, then an octave up, then an octave
  down; LEAP cycles unison/+1/−1 octave per successive note. A newly held
  group always starts a fresh scan at its lowest key, even when the no-key
  gap falls between clock edges. Engaging the arpeggiator clock-slaves the
  Shaper's rate to the LFO (except in FREE).
- **MOD SOURCE** — LFO triangle; LFO square; S+H RANDOM (sampled red noise);
  S+H Y (the Shaper sampled — a regular, patterned staircase); RED NOISE
  (continuous slow wander); OSC B (its currently selected waveform, at audio
  rate). Pitch destinations pass through each Spirit CEM3340 pin-14 network's
  derived nominal 1.82 µs memory, a tiny but deliberate phase softening that
  matters most under high-rate self-FM and sync.
- **LFO/S+H RATE** — nominally 0.3154787–50 Hz; also the sample-and-hold and
  arpeggiator clock. The original control is circuit-loaded, so half knob
  travel is 2.9974213 Hz rather than the 3.9716412 Hz midpoint of an unloaded
  exponential sweep. It does not affect RED NOISE or OSC B.

## SHAPER Y

A variable-symmetry envelope/LFO (the modelled instrument's second
modulation generator).

- **MODE** — **FREE**: a free-running LFO, symmetric about zero. **KBD
  HOLD**: rises while gated and holds at maximum. **RESET**: one rise-fall
  cycle from zero, restarted by *every* key press regardless of the TRIGGER
  switch. **RUN**: the rising segment always completes; new gates are
  ignored only until that rise has completed. With KBD selected, a MULTIPLE
  legato press can therefore start RUN again without the combined gate bus
  first going low; SINGLE still needs a new bus edge.
- **SHAPE** — the rise/fall split of the period: 0 is a 2.5617/97.4383
  fast-rise/slow-fall split, 5 is symmetric, and 10 reverses the split.
  Total time never changes.
- **RATE** — the total period: several cycles per minute up to about 20 Hz.

Functionally, the Shaper output controls the Shaper path's VCA, so that path
pulses in FREE mode and articulates in the envelope modes. Ghostar currently
uses normalized Y as its behavioral gain; the hardware's coupled transistor/
CEM3360 control law remains measurement-owned.

## WHEEL DESTINATIONS

- **MOD X TO:** — OFF · OSC A+B · OSC A · OSC A RWM (rectangle width) ·
  FILT U+L · FILT U.
- **SHAPE X WITH Y** — the Y signal envelopes the X wheel's signal:
  automatically swelling vibrato.
- **SHAPER Y TO:** — OFF · OSC A+B · OSC B · OSC B RWM · LFO RATE (the Y
  wheel sets the fastest rate, the panel knob the slowest) · FILT L.

The destination switch changes the electrical load, not just the label. As on
the original circuit, A+B is a little shallower per oscillator than a single
oscillator, FILT U is deeper than FILT U+L, and the X and Y wheels have
opposite-feeling curves: at half assumed-linear resistance travel X→A already
has 86.75% of its full depth, while Y→B has 33.56%. The wheel pot tapers and
absolute full-depth calibration remain hardware-measurement items; the
load-dependent differences are schematic-derived.

## AUDIO MIXER

- **MASTER VOLUME** — one linear 20 kΩ gang per path. With SPLIT off, the
  linked wipers interact; their DC limit is a half-sum, but their audio
  transfer also depends on BRIGHTNESS and the knob position.
- **BRIGHTNESS** — a passive shelf after the Shaper VCA. At 0 it is a
  294.7 Hz low-pass in SPLIT; at 10 it leaves a gentle −1.58 dB high shelf.
  The normalled jack shifts that response and subtly colours the Filter path
  too — a consequence of the original passive wiring, not stereo crosstalk.
- **Shaper path sliders** — A, B, RING, NOISE. The ring modulator is always
  Osc A's fixed internal triangle × Osc B's fixed internal triangle,
  unaffected by the WAVEFORM switches. Its internal P2 trim cancels carrier
  in the nominal circuit; real-device feedthrough is unit-dependent, not a
  fixed added bleed.
- **Filter path sliders** — A, B, NOISE (no ring in this path).

## UPPER FILTER U / LOWER FILTER L

The signature series dual filter.

- **MASTER** — both filters' cutoff, always. Its loaded analog pot covers
  nearly twelve octaves, so the end stops are deliberately very closed and
  effectively wide open rather than a polite digital cutoff range.
- **LOWER ONLY** — the Lower Filter's cutoff relative to the Upper; the two
  nominally coincide at 8, below 8 the Lower sits below the Upper. DYNAMIC
  keeps that coincidence throughout MASTER travel; FORMANT preserves the
  hardware's tiny moving mismatch instead of forcing mathematical identity.
- **RESONANCE switch** — LOW fixes the Upper Filter at Q = 0.5; VARIABLE
  slaves it to the pot.
- **RESONANCE pot** — the Lower Filter always, the Upper in VARIABLE; both
  reach self-oscillation at maximum, playable across the keyboard with KB
  AMOUNT up.
- **SLOPE** — the Upper Filter at 12 or 24 dB/octave. This is a physical
  switched network: the two positions have their original relative level,
  and moving it transfers the small timing capacitor's retained charge, so a
  state-dependent click or tiny pitch gesture is intentional.
- **KB AMOUNT** — keyboard tracking, zero to slightly over 100 %. Ghostar
  follows the original DAC/reference cancellation: its pivot is the
  keyboard's second C plus 0.6015 cent (MIDI 60.006015). Both the amount and
  that tiny offset are circuit-derived.
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
- **A D S R** — attack, decay and release each set a 5 ms to 10 s RC time
  constant. The original 100 Ω threshold/cap arm is retained in all three
  segments; at fastest Attack this makes the cap turn around slightly below
  the nominal 7.5 V threshold, one of the unit's small articulation quirks.
  The two sustain controls share the original diode-biased bottom
  rail; nominally, zero sustain lands at the Loudness VCA's 0.5 V shutoff and
  the remaining travel is affine above it. Release slows into the original
  series-diode knee (31.3 s from peak to audible silence at maximum), rather
  than remaining a pure exponential. At Ghostar's nominal 1.3× charging aim
  attack reaches threshold after about 1.47 time constants; real-part spread
  bounds that figure until a hardware envelope capture exists.
- **Loudness response** — the traced control network gives the 7.5 V envelope
  a 0.5 V dead zone, then Ghostar follows its nominal linear rise to full
  gain. An original CEM3360's exact top gain, saturation and feedthrough vary
  by device and remain unresolved.
- **VCA BYPASS** — holds the Filter path's VCA fully open: an
  un-articulated drone.

## PERFORMANCE

- **GLIDE / GLIDE MODE** — portamento amount; OFF, AUTO (only while more
  than one key is held — fingered portamento), ON. The traced 470 nF capacitor
  and 2 MΩ pot give a 0.94 s maximum RC time constant; the travel curve remains
  voiced.
- **Bend wheel** — Ghostar uses ±8 semitones at full travel. The vintage
  circuit's raw pot has ±15.88 semitones of electrical authority, but no source
  records how much of it the spring-loaded mechanism reaches.
- **MOD X / SHAPER Y wheels** — attenuators for the two modulation signals,
  "toward zero volts": a bipolar source keeps its symmetry, a unipolar one
  scales toward silence. X is a current-driven rheostat and feels immediate;
  Y is a voltage divider behind 15 kΩ and blooms later in its travel. Their
  destination-dependent loading is retained at control and audio rate.
- **SPLIT** — off sends the hardware-normalled, passively cross-loaded mix to
  both channels. On isolates the Filter/ADSR path at full level on the left and the
  Shaper path at full level on the right, like inserting a plug into the
  hardware's SHAPED rear jack. A mono plug-in bus keeps the normalled mix.
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
| 6 | Sync | The sync siren: raise the Y wheel and the Shaper sweeps Osc B against locked A. |
| 7 | Shake Shape | SHAPE X WITH Y: the Shaper envelopes the X wheel's vibrato. |
| 8 | Sample & Hold | Stepped random voltages to the filter and oscillators — raise the X wheel. |
| 9 | Parallel Rectangles | A fifth apart: the X wheel drives one PWM, the Y wheel the other. |
| 10 | Arpeggio | The arpeggiator clocking gates with Shaper-path noise. |
| 11 | Noise Scale | Resonance at maximum turns noise into a keyed pitch. |
| 12 | Inverted Guitar | The inverted filter envelope pluck. |

### Ghostar Programs — the performance bank

Programs 13–29 are Ghostar's own, for playing rather than for teaching.
They make no historical claim — the hardware had no presets to copy — and
each one puts a single mechanism in the foreground, so the bank doubles as
a tour of what the panel can do. Each is gain-checked under the gesture that
reveals it—held chord, legato overlap, free-running drone or release tail—and
the bank is clip-checked as a whole.

| # | Program | What it foregrounds | Initial X / Y |
|---|---|---|---:|
| 13 | Spirit Bass | Fast 24 dB punch over a detuned rectangular undertone. | 0 / 0 |
| 14 | Vowel Motion | Dynamic dual peaks; Y makes the lower vowel roam. | 0 / .45 |
| 15 | Fixed Reed | FORMANT fixes the lower peak while X moves only the upper. | .22 / 0 |
| 16 | Diode Growl | The lower-filter diode clipper feeding a dark 24 dB lowpass. | 0 / 0 |
| 17 | Sync Razor | Hard-synced Osc B, an automatic Shaper sweep and AUTO glide. | 0 / .62 |
| 18 | Crossmod Steel | KBD HOLD fades in genuine audio-rate Osc B cross-modulation. | .34 / 0 |
| 19 | PWM Choir | A fifth apart, with independent X and Y pulse-width motion. | .34 / .38 |
| 20 | Ring Temple | A fast RESET contour strikes the triangle-cross ring modulator. | 0 / 0 |
| 21 | Split Seance | A keyed dual-filter voice left and free ring/noise apparition right. | 0 / 0 |
| 22 | Motor Drone | VCA-bypass WIDE oscillator motor with red-noise filter drift. | .28 / 0 |
| 23 | Ripple Pluck | RIPPLE scans held notes through short resonant plucks. | 0 / 0 |
| 24 | Leap Machine | LEAP octave substitutions with ring-mod transients. | 0 / 0 |
| 25 | Stepped Formant | S+H samples Shaper Y into repeating steps over a fixed peak. | .42 / 0 |
| 26 | Run Chopper | X gates both ADSRs while RUN cuts the second audio path. | 0 / 0 |
| 27 | Noise Glass | Keyboard-tracked resonant noise ringing between two peaks. | 0 / 0 |
| 28 | Subharmonic Reed | A fixed BASS oscillator under a keyed rectangular formant voice. | 0 / 0 |
| 29 | Double Edge | Lower highpass against upper lowpass; Y moves the lower edge. | 0 / .38 |

Selecting a program writes every panel control, so each sound remains
readable in the editor. The historical Sound Charts also restore both wheels
fully back, exactly as drawn. Ghostar's own performance programs can instead
store the initial X/Y stance shown above, which makes their defining motion
audible immediately; CC1/CC2 or the on-screen wheels take over normally as
soon as you move them.

## Hearing the instrument

The twelve committed renders under [`Docs/audio`](audio/README.md) demonstrate
one signature mechanism each, rendered from the same engine the plug-in
runs — including a resonance sweep that lets you hear the derived resonance
law's shape, and a tour of six of the Ghostar Programs.
