# Septum — best-in-class pass, 2026-08-22

**Work mode:** competitive evidence search plus measurement of the shipping
engine. **No hardware was measured.** This document refreshes the market
picture for an SH-201 recreation, states where this project is behind on axes
the [research contract](sh201-replica-research.md) does not cover, and lists
the numbered steps taken in response. It follows the contract's discipline:
every comparative cell cites what a vendor, reviewer or owner actually
publishes, absence findings are stated as absence, and no step below claims to
close an open question that needs a capture from a real unit.

## 1. The competitive landscape

The SH-201 is the rare case of a well-documented Roland instrument with **no
software recreation at all**. A search across Roland's own catalogue, the
commercial plug-in market and open-source repositories found none, and this
absence is the single most important fact about the project's position.

| Product | What it is | Position relative to an SH-201 recreation |
|---|---|---|
| **Roland Cloud** (SH-2, SYSTEM-100, JUNO-106, JUPITER-8, JX-3P, GALAXIAS) | Official, ACB component modelling | Roland models its *analog* back catalogue. Its published line-up contains no SH-201 and no SH-201 content, and the instrument was discontinued in 2010 ([Roland Cloud](https://www.rolandcloud.com/), [Wikipedia](https://en.wikipedia.org/wiki/Roland_SH-201)). **Absence finding:** no official recreation exists, so there is no vendor reference to be measured against |
| **Adam Szabo JP6K / Airwave** | Commercial, JP-8000-derived | The supersaw reference implementations, from the author of the measurements this project quotes. They model the *JP-8000* — the SH-201's ancestor — not the SH-201: one oscillator's worth of supersaw, no SH-201 voice architecture, no dual/split, no SH-201 effect block ([Blogosaur survey](https://blog.wavosaur.com/5-free-vst-emulations-of-roland-jp-8000-supersaw/)) |
| **Sunrizer, SuperWave P8, JE-8086, JP-4c** | Commercial and free | Repeatedly named by owners as the practical JP-8000 supersaw substitutes ([KVR t=492543](https://www.kvraudio.com/forum/viewtopic.php?t=492543)). Same gap: supersaw generators, not SH-201 models |
| **Generic modern VA (Vital, Serum, Diva, …)** | Commercial and free | Cover the *sound* of a detuned saw stack far beyond what an SH-201 can do. None reproduces the SH-201's documented parameter contract, its enumeration orders, its effect templates or its panel semantics — which is what a recreation is for |

Two consequences follow, and they shape every step below.

1. **There is no accuracy leader to catch up to.** No competitor publishes an
   evidence chain for the SH-201, because no competitor models it. The bar this
   project has to clear is not another vendor's fidelity; it is **the
   instrument's own documented behaviour**, and the gap that matters is
   whatever the hardware does that this engine does not.
2. **The market comparison that does bite is feature completeness.** A player
   reaching for an SH-201 recreation reaches for the whole instrument. The
   arpeggiator, the external-input path and the D-Beam are on the front panel
   and in every review; a recreation that omits them is not competing with
   another SH-201 plug-in, it is competing with the hardware.

Two reported properties of the real unit that the field's supersaw plug-ins
generally *remove* and this project deliberately keeps: the supersaw's
aliasing in the mid and upper octaves, reported by owners and framed by the
Roland Clan admin as part of what separates the instrument from "crystal-clear
synths" ([Roland Clan t=17268](https://forums.rolandclan.com/viewtopic.php?t=17268)),
and the non-band-limited saw stack that produces it (Szabo). Those stay.

## 2. Gap analysis

### 2.1 What the research contract already owns

The [research contract](sh201-replica-research.md) settles the architecture,
the parameter contract, the CC map and the analog output stage from primary
Roland documents, and its open-question queue owns every remaining *hardware*
unknown — OQ-01 (engine rate) through OQ-13 (voice-steal policy). **No step
below claims to close one of those.** Each needs a capture from an identified
unit, and re-fitting a constant to taste would be exactly what the queue
forbids.

### 2.2 Gap A — measured: the filter envelope does not have "bags of punch"

The contract records as *reported* that the instrument's "fast ADSR response
times ensure bags of punch" (Sound on Sound, April 2007). Measured on this
pass, at 44.1 kHz, on the shipping engine, driving NOISE through a −24 dB LPF
with the filter envelope at its fastest attack (A = 0, mapped to 1 ms) and
depth +63, RMS in 0.25 ms windows:

| Envelope, slider A = 0 | 90 % of final |
|---|---|
| AMP envelope | **1.25 ms** |
| FILTER envelope | **8.25 ms** (50 % at 4.00 ms) |

The two envelopes read the same slider through the same mapping, so the 6.6×
difference is not the envelope — it is the 2.5 ms one-pole slew that the voice
applies to the whole cutoff sum. That slew was added so stepped cutoff moves
(a patch edit, an S&H LFO) do not produce a sample-level discontinuity in the
filter coefficient. It cannot tell a stepped *parameter* from a fast
*modulator*, so it low-passes the filter envelope too, and the fastest
documented filter attack on the instrument is the exact case it damages most.
**Step 1 below.**

### 2.3 Gap B — measured: the overdrive's character depends on the host rate

A pure SINE through OVERDRIVE, at 44.1 kHz, with every partial that is not an
integer multiple of f₀ counted as alias energy:

| | drive 0 | 32 | 64 | 96 | 127 |
|---|---|---|---|---|---|
| note 45 (110 Hz) | −57.1 dB | −57.1 | −57.2 | −57.2 | −57.2 |
| note 69 (440 Hz) | −68.2 dB | −67.1 | −65.0 | −54.0 | **−33.2** |
| note 81 (880 Hz) | −62.5 dB | −61.8 | −58.9 | −34.0 | **−24.1** |
| note 93 (1760 Hz) | −57.4 dB | −56.9 | −33.6 | −21.6 | **−17.7** |

At the top of the DRIVE range the shaper folds inharmonic energy back at
−17.7 dB relative to the harmonics it is supposed to be making. The number
that matters is not its size — the hardware's own fixed-rate DSP folds
something too — but that **it is a function of the host's sample rate**. The
same patch rendered at 96 kHz folds far less. A model of a fixed-rate DSP
whose distortion character changes when the user switches their interface from
44.1 to 96 kHz is reproducing the port, not the instrument. Until OQ-01 fixes
the hardware's rate, the model should not manufacture a *different*,
host-dependent alias signature of its own. **Step 2 below.**

### 2.4 Gap C — measured: cost is not the constraint, so quality can be

One 2.1 GHz Xeon core, 48 kHz, block 256, five seconds of audio, elapsed wall
seconds per second of audio:

| Scenario | × realtime |
|---|---|
| Idle, no note held | 0.003 |
| 10 saw voices, no effects | 0.023 |
| 10 supersaw voices, resonance 90 | 0.050 |
| 10 supersaw voices + delay + reverb | 0.051 |

The engine runs the heaviest configuration it has at **5 % of one core**. In a
market where one JUNO-106 vendor advertises light CPU as a feature and another
is marked down for costing 10 % of a core at idle, this is a strong position —
and, more usefully, it is roughly 20× of headroom. Every quality step below is
affordable; none of them needs to be traded against real-time cost. This axis
is recorded here because the research contract has no row for it.

### 2.5 Gap D — the deferred features are the front panel

The contract's scope section defers the arpeggiator and step recorder, the
D-Beam, the external-input path (audio filter, centre cancel, EXT-IN
oscillators), SysEx DT1/RQ1 and sostenuto. Three of those are printed on the
instrument's fascia and appear in every review of it; two of them have
parameters in the engine right now that do nothing:

- **AUDIO-FILTER** is a settled LFO destination 1 and a settled MODULATION
  ASSIGN destination. Both are exposed on the panel and both are silent,
  because there is no audio filter to modulate.
- **EXT-IN** is waveform 8 of the settled enumeration and renders silence.
- **Sostenuto** is CC#66 in the settled part-controller list and is ignored.

**Steps 3, 4 and 5 below.** The D-Beam, the step recorder and SysEx DT1/RQ1
stay deferred this pass and keep their entries in the contract's scope section:
the first two need a design decision about what an infrared distance sensor and
a pattern recorder mean inside a plug-in, which is a question for the user, not
a gap a measurement closes.

### 2.6 Gap E — voiced constants that the contract does not list

The contract's own rule is that every voiced constant appears in the open
questions with the measurement that would close it. An audit of the engine
found constants that live inline in the render code and are named in no
question: the modulation lever's depth into each of its four destinations, the
fixed voice headroom, the delay modulation's rate and depth spans, the reverb's
line geometry and its density/diffusion laws, the FB-OSC loop's in-loop
damping and output trim, and the supersaw stack's summing normalisation.
**Step 6 below.**

### 2.7 Gap F — the resonance knob's first half is inaudible

Measured on this pass at 96 kHz, noise through the LPF, gain referred to the
filter's own passband, cutoff 64 (nominal 657.7 Hz):

| RESONANCE | −12 dB peak | −24 dB peak |
|---|---|---|
| 0 | +0.06 dB | +0.06 dB |
| 64 | **+1.38 dB** | +1.58 dB |
| 100 | +8.18 dB | +7.05 dB |
| 120 | +22.70 dB | +21.12 dB |

The mapping is linear in the state-variable damping `k`, which puts the entire
audible range of the control in the top fifth of its travel: at the exact
centre of the knob the filter peaks by less than 1.5 dB. This is a *voiced*
curve under OQ-08 and the endpoints it connects are the defensible ones
(Q = 0.5 at zero, self-oscillation at the far right); what is not settled is
the shape between them, and more than one shape is derivable. Per the
project's working conventions this is a listening-test question, not a
measurement question. **Step 7 below: render the candidates, do not re-pin the
constant unilaterally.**

### 2.8 Gap G — the panel

The shipping editor lays every section out through one generic
weight-and-rows algorithm. Measured against its own screenshot: knob diameters
range from 22 px in OSC 1 to 58 px in EFFECTS: REVERB for controls of equal
importance, the FILTER ENV depth label renders as `DE..`, MIX/MOD and PITCH
ENV carry large dead areas while OSC 2 crowds eight controls into the same
width, and no control shows its value without being touched. **Step 8 below.**

## 3. Steps

Each step is one commit, and is only listed here once it has landed; the entry
records what was measured before and after. The queue this pass opened, in the
order the gaps above put them:

1. Separate parameter smoothing from filter modulation (§2.2).
2. Stop the overdrive depending on the host rate (§2.3).
3. Sostenuto (§2.5).
4. The external-input path and the AUDIO FILTER (§2.5).
5. The arpeggiator (§2.5).
6. Register every voiced constant (§2.6).
7. Take the resonance curve to a listening test (§2.7) — a decision to hand
   over, not a change to make.
8. The panel (§2.8).

A second pass on 2026-08-23, from the user's own report rather than from a
competitive search, opened three more: the D Beam's removal, the panel's
UPPER/LOWER clarity, and an audit of the engine against this project's own
rules. Steps 27 onward are that pass. Its largest finding was not a gap in
what the instrument does but three inventions in what the replica claimed —
see Step 29.

### Landed

#### Step 1 — parameter smoothing and filter modulation are separated

The voice keeps the 2.5 ms slew where it was needed — on the parameter-derived
part of the cutoff (knob, key follow, velocity offset) and on the LFO, which is
the contribution that actually steps — and applies the filter envelope's level
directly, its depth knob being slewed with the rest of the panel. The filter
coefficients are then walked sample by sample across the control tick, so
taking the envelope out of the slew did not put a staircase back into them.

Measured after, same harness as §2.2:

| Envelope, slider A = 0 | before | after |
|---|---|---|
| AMP envelope, 90 % of final | 1.25 ms | 1.25 ms |
| FILTER envelope, 90 % of final | 8.25 ms | **1.25 ms** |
| FILTER envelope, 50 % of final | 4.00 ms | **1.00 ms** |

Nothing else moved: the filter's −3 dB points and resonant peaks (§2.7) are
unchanged to the resolution of the sweep, and the cost table (§2.4) is
unchanged. Two tests fence the result — the filter envelope may not open
slower than the amp envelope from the same slider value, and a fast S&H filter
LFO must still produce no sample-level discontinuity.

#### Step 2 — the overdrive stops depending on the host rate

The AMP overdrive is evaluated at the power-of-two multiple of the host rate
that lands closest to 176.4 kHz: 4× at 44.1/48 kHz, 2× at 88.2/96 kHz, none at
176.4/192 kHz, through two equiripple half-band polyphase stages, with `tanh`
under first-order antiderivative anti-aliasing inside the loop. A power-of-two
ladder cannot hit a fixed rate from an arbitrary host rate, so the guarantee is
a bound: ±1.0 octave of the target across 22.05–192 kHz, and exactly inside
176.4–192 kHz at the four common rates. Only 22.05 kHz sits a whole octave low;
every other rate in that span is within half an octave. The transfer curve
is untouched, so OQ-11 is exactly where it was.

Measured after, same harness as §2.3, alias energy inside 20 Hz – 20 kHz
relative to harmonic energy, at 44.1 kHz:

| | drive 64 | drive 96 | drive 127 |
|---|---|---|---|
| note 69, before | −65.0 dB | −56.9 dB | −34.5 dB |
| note 69, after | −65.0 dB | **−63.9 dB** | **−60.8 dB** |
| note 81, before | −60.1 dB | −35.6 dB | −24.8 dB |
| note 81, after | −60.9 dB | **−60.5 dB** | **−59.3 dB** |
| note 93, before | −37.6 dB | −23.2 dB | −18.7 dB |
| note 93, after | **−57.4 dB** | **−56.7 dB** | **−54.9 dB** |

The sharper reading is the one line that isolates the mechanism. The 25th
harmonic of 1760 Hz is 44 kHz, which folds to exactly 100 Hz at a 44.1 kHz
host rate and does not fold at all at any other rate. Relative to the
fundamental, that line measured **−29.3 dB before and −77.1 dB after** — a
47.8 dB drop — while at 48, 88.2, 96 and 176.4 kHz it stays below −134 dB in
both. Across those five rates the audible-band alias floor now spans 7–10 dB
instead of 27–41 dB, and never rises above −53.7 dB.

The chain's group delay is carried by every voice, shaping or not, so a clean
tone layered under an overdriven one stays in phase; the plug-in reports it
(19 samples at 44.1 kHz, 16 at 88.2/96 kHz, none above). Cost with ten voices,
both tones overdriven at DRIVE 127 and the effects running: 0.079 → 0.095 ×
realtime on the §2.4 machine. Patches with OVERDRIVE off are unchanged apart
from the shared delay.

Three tests fence it: the 100 Hz fold line must stay 60 dB below the
fundamental at 44.1, 48 and 88.2 kHz; the clean and overdriven paths must
cross-correlate to a peak at lag zero; and the reported latency must match the
chain at each rate.

#### Step 3 — sostenuto

CC#66 latches the notes whose keys are down at the moment it arrives and holds
only those; a key pressed afterwards plays and releases normally. It is
independent of the hold pedal — a note caught by both releases only when both
are up — and a stolen voice loses its latch, because the latch belonged to the
note the pedal caught rather than to the physical voice. The contract's scope
section no longer defers it.

#### Step 4 — the external-input path and the AUDIO FILTER

The plug-in declares a stereo input bus (off by default, so a host that gives
a synthesizer no input still loads it unchanged) and the engine implements the
path around it: INPUT VOL, CENTER CANCEL, and the AUDIO FILTER with its four
settled types — LPF, HPF, BPF and the NOTCH the voice filter does not have —
its −12/−24 dB slope, cutoff and resonance, answering on the settled CC#2 and
CC#4. Selecting EXT-IN as an oscillator waveform plays the input through the
voice in mono, and the direct monitor hands the input over while it does.

Two settled parameters that had nothing to move now move something: LFO
destination 1 = AUDIO-FILTER and MODULATION ASSIGN = AUDIO-FILTER modulate
that cutoff.

The manual's own recipe for "producing sound from the external device only
when you play the keyboard" — audio filter on, LPF, cutoff fully left — is
what settles the order of the two paths, and it is a test: the released state
measures more than 100× below the held one, and the played note is unaffected
by the audio filter's setting, which is only true if the oscillator taps the
input ahead of that filter. Nine checks cover the path in all.

#### Step 5 — the arpeggiator

The settled arpeggio block is implemented against the settled 32 × 16 grid:
GRID with its shuffled divisions, DURATION with tie chains and FUL, MOTIF,
OCTAVE RANGE, ACCENT, ARPEGGIO VELOCITY, END STEP, HOLD and SPLIT ARPEGGIO,
running at the patch tempo. Notes it generates go through the same voice
assigner the keyboard uses, so polyphony, solo/legato and the effect sends all
behave as they do for played notes.

The MOTIF mapping is not inferred. The manual works three examples of the
style `1-2-3-2` against the keys C-D-E-F-G, and the mapping — a pure function,
so it can be tested directly — reproduces all three exactly; **those examples
are the test**. Twelve more checks cover the grid divisions (each one adding
up to a beat, shuffled pairs included), the played-and-held behaviour, HOLD,
and OCTAVE RANGE moving the next cycle an octave.

Roland's 32 factory styles are unpublished data and none of them ships; 16
original patterns do, and the panel selects among them exactly as the
hardware's panel selects a template. A new committed demo,
`11-arpeggiator.wav`, plays one chord through UP(-), then UP&DOWN(L&H) with a
heavy shuffle, then OCTAVE RANGE +2 on HOLD.

#### Step 6 — every voiced constant is registered

The constants §2.6 lists moved out of the render code and into the engine's
`mapping` namespace, each tagged with its tier and the open question that owns
it: the balance crossfade law, the FB-OSC loop's four constants, the supersaw
stack's trim, the modulation lever's reach into each of its four settled
destinations, the delay's modulation range, the reverb's line geometry and its
density/diffusion/injection/return laws, the −24 dB path's second-stage
damping and the resonant stage's state limit, and the headroom, output-limiter
and pan-law constants. The research contract gained a table naming where each
one now lives, and says plainly which of them no measurement would ever settle
— headroom, safety and zipper are engineering choices, not claims about the
instrument.

There are now no bare numbers left in the render code. Twelve checks fence the
endpoints that *are* settled (BALANCE fully left is OSC1 alone; the pan law is
unity at the centre; every host rate reaches the overdrive's internal band),
and the move changed no audio at all: the committed demos re-render
bit-identically.

#### Step 8 — the panel

The editor was rebuilt around a fixed control geometry instead of a
proportional one. Every section is now sized to fit its contents rather than
its contents scaled to fit it, so the knob diameters that ranged from 22 px to
58 px are all 40 px, the sliders are all the same width, and the `DE..` that
FILTER ENV's DEPTH label used to render as is simply `DEPTH`.

What changed beyond the geometry:

- **The panel reads as a signal path.** Three bands: the voice chain
  (OSC 1 + OSC 2 → MIX/MOD → FILTER → AMP, with a plus where the oscillators
  meet and a chevron at each stage boundary, because that is what actually
  happens); the modulators (PITCH ENV, FILTER ENV, AMP ENV, LFO 1, LFO 2); and
  the two ends of the instrument (ARPEGGIO, EXT IN, DELAY, REVERB). One muted
  tint per band, appearing once as a short rule above each section title —
  enough to group the panel, not enough to turn it into a chart.
- **Every continuous control reads out its value**, in the units the manual
  prints, taken from the parameter's own text so the panel and the host can
  never disagree. Signed parameters print their sign and PAN prints
  `L64 … 0 … 63R`, which is also what the host's parameter list now shows.
- **The drag-time popup bubbles are gone**, since the value is always on the
  panel, and hovering any control names the parameter it edits.
- **The patch strip's knobs are the same knobs as everywhere else**, and its
  title no longer collides with its labels.
- The output meter and the voice count moved to the foot of the performance
  cluster — the one part of the panel that reports rather than edits.

The window is 1500 × 786, up from 1284 × 716: the arpeggiator and the
external-input path added two sections, and nothing is squeezed to fit.

#### Step 7 — three calibration questions went to a listening test

*Decided by ear, 2026-08-22.* Not committed — a set per decision would bloat the
repository, and the working conventions say to hand them over instead. Each
set was rendered through the shipping signal path with identical MIDI, seed,
sample rate, block size, length and controls; only the mechanism under test
differs. `A` is always the shipping engine. Each is level-matched on
whole-file RMS against `A`, with one further set-wide gain so the files are
comfortable to audition — the same factor on every letter, so the match is
untouched. Each carries a `listen.md` that is safe to read and a `key.md`
written at the same time and unread by design.

| Set | Letters | Question |
| --- | --- | --- |
| `2026-08-22-resonance-curve` | A, B, C | The resonance-to-Q shape between two settled endpoints (§2.7, OQ-08). 10.5 s: a slow RESONANCE sweep under one held note, then a bass line with the knob at dead centre |
| `2026-08-22-supersaw-hpf` | A, B | The tracked high-pass on the summed seven-saw stack: 1.0 × f₀ at Q = 0.707 against 2.5 × f₀ at Q = √2 (OQ-04) |
| `2026-08-22-supersaw-mix` | A, B, C | Where to evaluate Szabo's measured mix laws, given the SH-201 has no MIX knob (OQ-05) |

**The verdicts.** Each was made by ear, on level-matched takes, with the key
unread until after the choice. None of them closes an open question: the
captures the contract names for OQ-04, OQ-05 and OQ-08 are still what would.

| Set | Chosen | What that licensed |
| --- | --- | --- |
| resonance curve | **B** — the square-root taper, `k = 2 − 2.04·√(v/127)` | Re-pinned. Both settled endpoints are untouched; only the shape between them moved |
| supersaw HPF | **A**, the shipping engine, with the listener unsure and calling it "probably a bit better" | Nothing. 1.0 × f₀ at Q = 0.707 stays, and OQ-04 keeps its standing-candidate status: an unsure preference for the incumbent is not evidence about the hardware |
| supersaw mix | **A**, the shipping engine, "probably ok" | Nothing. m = 0.75 stays, OQ-05 unchanged |

Measured after re-pinning the resonance curve, same harness as §2.7, gain
referred to the filter's own passband at cutoff 64:

| RESONANCE | −12 dB peak, before | after | −24 dB peak, before | after |
|---|---|---|---|---|
| 0 | +0.06 dB | +0.06 dB | +0.06 dB | +0.06 dB |
| 64 | +1.38 dB | **+5.43 dB** | +1.58 dB | **+4.73 dB** |
| 100 | +8.18 dB | +14.39 dB | +7.05 dB | +12.80 dB |
| 120 | +22.70 dB | +33.32 dB | +21.12 dB | +31.73 dB |

The centre of the knob now does something audible, and the oscillation
threshold has not moved: the top of the travel still crosses into the bounded
self-oscillation the manual warns about, and the existing test that fences
that still passes.

#### Step 9 — three defects the review rounds found in the arpeggiator

The automated review's fourth round raised two, and testing the second of them
uncovered a third that was worse than either.

**The tie chain measured DURATION against the wrong step.** A note-on tied
across the grids that follow it holds for those grids plus DURATION of the
final one. The gate summed the tied steps and then took the fraction from the
step the chain *started* on, which is the same length on an even grid and a
different one on a shuffled pair. A chain beginning on a Heavy sixteenth and
tying into the following Light step lasted `short + ½·long` instead of
`long + ½·short` — about a fifth of a beat early, and equally late for a chain
starting on the Light step. The chain now carries its final step's length and
the fraction is taken from that.

**A shorter style fired a cell it does not have.** ARPEGGIO STYLE and END STEP
are both automatable. The step counter is reduced modulo the pattern length
*after* the step fires, so automating to a shorter pattern left it pointing
past the new end: the switch spent one grid on an unused cell and then resumed
from the wrong position. The counter is normalised before the step fires.

**A plain run on a single held key went silent at DURATION 100 %.** This one
was not reported; it turned up while building the test for the step counter,
and it is the most serious of the three. Every plain run in the shipped set —
`Straight 4`, `Straight 8` — puts each step on its own row, so one held key
resolves to the *same pitch* on every step. Voices are released by pitch, and
a row's gate is counted down in whole control ticks while the grid advances in
samples, so a gate that should end exactly on a grid boundary was still a few
samples alive when the next step fired. Its note-off then landed a tick later
— on the note that had just started, at the same pitch. Measured on `Straight
4` with one key held, whole-take RMS after the first grid:

| DURATION | before | after |
|---|---|---|
| 50 % | 0.0490 | 0.0490 |
| 100 % | **0.000119** | **0.0695** |
| 120 % | 0.0310 | 0.0695 |

At 100 % the pattern was inaudible; at 120 % every gate was cut back to its own
grid, which is why it measured *shorter* than 50 %. A note-on now takes over
any other claim on its pitch — another row's note, or a 120 % tail — since a
claim that nothing downstream can tell apart from the new note is not a claim
that can be kept. A repeated pitch retriggers, which is all a repeated pitch
can do.

Each of the three has a test that was checked by reverting its fix and watching
it fail. Two further guards written along the way were removed again: with the
pitch handover in place neither changed any measurement, and a fix that no test
can distinguish from its absence is not a fix worth carrying. The committed
demo audio is unchanged — `11-arpeggiator.wav` plays chords, so no row ever
repeats a pitch in it.

#### Step 10 — the fifth review round, after the pass had already merged

Four findings arrived on the merged pull request. All four were real, and the
first two are the uncomfortable kind: each was a claim the project had already
made in writing.

**The overdrive selected an oversampling factor the shaper cannot run.**
`overdriveOversampling` chose 8 at 22.05 and 24 kHz, but `OverdriveStage` has
an outer and an inner half-band stage and nothing beyond — the factor-4 and
factor-8 cases shared a branch. Those rates ran at 4× while the selector said
8×. The test that guarded the documented ±0.54-octave bound measured the
*selector*, so it agreed with the claim rather than with the audio, and the
bound had been defended in a review reply on that basis. A third stage would
cost 2p at 4× host — 1.5 host samples — a fractional group delay to report and
to align the bypass path against, bought for two unusual sample rates. The
ladder now stops where the shaper stops, the bound is restated at what it
delivers (**±1.0 octave across 22.05–192 kHz**, with only 22.05 kHz a full
octave low), and the test checks the factor is implementable before it measures
anything.

**END STEP was documented and unreachable.** The README and the contract both
listed it, the engine treated `style.endStep` as automatable, and there was no
parameter and no control: every snapshot reapplied the selected template's
hard-coded length. It is now a bound patch parameter with its own panel knob,
reading `STYLE` at zero — the replica's own addition, meaning "as long as the
template is", so no existing patch changes.

**Arpeggio routing changes stranded voices.** Four automatable controls decide
whether a part is arpeggiated; only the ARPEGGIO switch was watched. Holding an
Upper key and then selecting Upper left the plain voice unmigrated, and its
note-off found a part the arpeggiator now drives and skipped the release —
0.069 RMS still sounding a second and a half later. The transition is tracked
per part now, against the predicate that already folds in all four controls.
The other direction changed with it: deselecting a part hands its held key back
as a plain voice instead of silencing it, which is what the ARPEGGIO switch
already did.

**The re-arm could not see a same-sample chord change.** It lived only in
`advanceArpeggiator`, which needs audio rendered while the chord is empty.
Adjacent chords in a sequence share one sample position, so the gap was never
observed and the new chord continued mid-pattern — 604 Hz where step one owed
293.66 Hz. The last key leaving is noticed where it happens now.

Each has a test checked by reverting its fix. `11-arpeggiator.wav` re-rendered
1.2 dB lower at the same length: chords now begin at step one, which is the
point of the re-arm fix.

#### Step 11 — the sixth review round, on code already merged

Four more findings, all real, all in code that had already landed.

**Sostenuto caught pitches played after the pedal went down.** The latch is a
128-bit pitch mask, and the bit stayed set for the whole pedal hold: play C,
depress the pedal, release C, then play and release C again, and the second
one sustained too. Only the notes sounding at the transition are held. The mask
stays per pitch — per voice would lose the latch the moment a mono voice was
borrowed by another key, which is why it was written that way — so a genuinely
new press clears its own bit.

**The arpeggiator's octave cycle was clamped as though it were a MIDI note.**
Note 108 with OCTAVE RANGE +2 played 108, 120, 127 where the pattern owes 108,
120, 132: two cycles on one pitch, and the arpeggio stopped moving near the top
of the keyboard. Pitch is a number of semitones in this engine and already
leaves 0–127 through the octave shift and transpose; the oscillator increment
is capped at Nyquist and nothing indexes an array by the value. The clamp is
gone from the generated pitch and still applies to incoming MIDI.

**Every switch on the external-input path was thrown rather than crossed.**
Two were reported — CENTER CANCEL, and the filter's ON and SLOPE — and TYPE
turned out to be the same defect, so it is fixed with them. Each chooses
between signals whose instantaneous samples differ, so keeping both sides warm
(which an earlier round already did) stops a stale burst but not a step. Each
now crosses over 5 ms. Measured as the largest sample-to-sample jump the output
makes across the switch, against the largest jump the same signals make while
nothing is touched:

| Switch | thrown | steady |
|---|---|---|
| CENTER CANCEL | 0.211 | 0.042 |
| FILTER ON | 0.304 | 0.042 |
| SLOPE | 0.220 | 0.031 |
| TYPE | 0.292 | 0.051 |

Thrown, each of the four jumps five to seven times as far as the signal's own
steady sample-to-sample travel. What the test bounds after the fix is the same
ratio: the jump at the changeover must stay under four times the steady figure
the same take measures, and it does for all four. (An earlier version of this
table carried an empty third column headed "crossed"; the figures were never
printed, only asserted, and the column is gone rather than filled with numbers
this page cannot show.)

Building that test taught its own lesson twice. Comparing the new signal
against the *old* one's slope read a high-pass output's faster travel as a
click; and moving the cutoff at the same moment as the switch left the filtered
path still agreeing with the dry one just as the switch was thrown, so the step
had nothing to show. Both were errors in the measurement, not the fix, and both
were caught by insisting the test fail when the fix is reverted.

#### Step 12 — the panel dropped a control, and lit half its knobs wrong

Two defects an audit of the shipping panel found, both of them the panel
saying something untrue about its own controls.

**SPLIT ARPEGGIO was built, attached and never laid out.** `layoutSection`
walks a section's declared row counts and stops; ARPEGGIO declared `{ 5, 5 }`
and held eleven controls, so the eleventh — the selector that decides which
tone the arpeggiator drives in SPLIT mode — was left at (0, 0, 0, 0). It was
visible, it was bound to its parameter, and it was invisible to the player.
This is the same class of defect as END STEP in Step 10: documented in the
README and the contract, reachable from the host, absent from the instrument.
The rows are now `{ 5, 6 }` — the switches and the three selectors above, the
six controls that decide how the style is read below — and the section is
44 px wider for it.

The fix that matters more is the fence. A new plug-in check walks every
visible child of the editor and fails if any of them has empty bounds or is
laid out past the panel's edge. Reverting the row counts fails it:

```
FAIL: every visible control on the panel has bounds
```

**A bipolar knob lit its arc from the far left.** `drawRotarySlider` filled
the travel arc from `rotaryStartAngle` to the pointer whatever the control
was, so every parameter whose range straddles zero — BALANCE, PAN, PITCH,
DETUNE, KEY FOLLOW, both VELOCITY sensitivities, FILTER ENV DEPTH, both
P.ENV depths, ARPEGGIO OCT RANGE, TONE BAL and delay FEEDBACK — showed a
half-lit arc at its zero, which reads as half on. They now light from the top
of the travel, where their zero is. The flag is read straight off the
parameter's own `NormalisableRange` in `bindControls` rather than from a list
that could drift: a control is bipolar exactly when its range crosses zero,
so the arc and the sign the manual prints can never disagree.

#### Step 13 — the band-pass lost level where the manual says it oscillates

The voice filter took its band-pass tap as `damping * bp`. Scaling the
band-pass by the SVF damping is the ordinary way to hold its peak at unity —
but *this* damping is `k`, the quantity RESONANCE drives down: `k = 2 −
2.04·√(v/127)` reaches zero at RESONANCE ≈ 122 and −0.04 at 127. So the top
of the knob multiplied the band-pass by a factor shrinking to nothing and
then inverting.

Measured at 96 kHz, NOISE through a −12 dB filter at cutoff 64, whole-take
RMS after 0.5 s, relative to the same filter at RESONANCE 0:

| RESONANCE | BPF before | BPF after | LPF (unchanged) |
|---|---|---|---|
| 0 | 0.00 dB | 0.00 dB | 0.00 dB |
| 32 | −2.93 dB | **+3.30 dB** | +3.19 dB |
| 64 | −5.32 dB | **+5.87 dB** | +5.75 dB |
| 96 | −9.14 dB | **+9.78 dB** | +9.67 dB |
| 110 | −12.69 dB | **+13.20 dB** | +13.08 dB |
| 120 | −21.16 dB | **+20.24 dB** | +20.09 dB |
| 127 | −8.50 dB | **+25.48 dB** | +25.30 dB |

The −8.50 dB at the end of the "before" column is the give-away: level
*returns* past RESONANCE 122 because `k` has gone negative, so the band-pass
came back inverted. The manual's warning that resonance far right "may not
stop at all" is not written per filter type, and on BPF the shipping engine
could not oscillate at all — it went quiet instead.

The band-pass is now the raw integrator tap, as LPF and HPF already were. It
tracks the low-pass to within 0.2 dB across the whole knob and self-oscillates
at the top, and it costs 6 dB of level at RESONANCE 0, which is what `1/k`
at `k = 2` is worth. No constant was added or moved: the fix is the removal of
one factor.

The AUDIO FILTER's band-pass changed with it. Its damping is floored at 0.05
so it never collapsed, but the contract says its resonance curve *is* the
voice filter's, and a band-pass that behaved differently would have made that
false.

Two checks fence it, both watched to fail with the factor put back: RESONANCE
120 must lift the band-pass at least 15 dB above RESONANCE 0, and full
resonance on a −24 dB band-pass must sustain a bounded oscillation. The one
committed demo that uses the band-pass, `09-sample-hold-fx.wav`, re-rendered
11.2 dB louder before normalisation; the other ten are bit-identical.

#### Step 14 — the triangle was not band-limited at all

`renderClassicWave` corrects the triangle's two corners with polyBLAMP and
scaled the correction by `8.0 * inc`. That is the triangle's whole slope
change per sample: the wave runs at ±4 per unit phase, so a corner changes
its slope by 8·inc. The coefficient the residual wants is half of it, and the
reason is in the two residual functions themselves. `polyBlamp` is exactly the
antiderivative of `polyBlep` with respect to sample time, and `polyBlep` here
is the canonical form that already carries a step of **two** — it corrects the
saw's −2 wrap on its own, with no factor in front. So a slope change of 8·inc
needs `4.0 * inc`.

At 8·inc the correction overshoots by as much as it corrects. Measured at
44.1 kHz, TRI through the shipping voice with the filter bypassed, alias
energy relative to harmonic energy across a fine grid up to 20 kHz:

| note | no correction | shipping (8·inc) | corrected (4·inc) |
|---|---|---|---|
| 69 (440 Hz) | −104.0 dB | −104.0 dB | **−124.1 dB** |
| 81 (880 Hz) | −95.9 dB | −95.8 dB | **−139.6 dB** |
| 93 (1760 Hz) | −87.4 dB | −87.3 dB | **−153.4 dB** |
| 105 (3520 Hz) | −90.1 dB | −89.7 dB | **−162.1 dB** |

The middle column is the finding: to within a tenth of a decibel the shipping
triangle measured the same as the oscillator with its correction deleted. The
contract states that the classic waveforms are polyBLEP/polyBLAMP band-limited
at the host rate, and deliberate aliasing belongs to SUPER SAW and SYNC; for
TRI that claim was false. The corrected floor holds across host rates —
−124 to −162 dB at 44.1 kHz, −130 to −168 at 48 kHz, −102 to −156 at 96 kHz
(the last column starts higher only because the same grid reaches further up
in frequency).

Two checks fence it, both watched to fail with `8.0 * inc` put back. No
committed demo changed, because no shipped preset selects TRI — which is its
own gap, and is why this one survived a full pass.

#### Step 15 — two settled quantities were indexed by the wrong thing

**The modulation lever reached the AUDIO FILTER once per sounding tone.** In
`prepareExternalTick` the whole modulation block — the two per-tone LFO
destination-1 routings *and* the lever's MODULATION ASSIGN contribution — sat
inside the loop over UPPER and LOWER. Two LFOs routed at one target genuinely
sum; the lever does not. MODULATION ASSIGN is one patch-common setting, the
lever is one lever and the audio filter is one filter, so in DUAL and SPLIT
the cutoff moved twice as far as `audioFilterLeverOctaves` says it can.
Measured with the lever at full travel and both LFO2s square at the slowest
rate, on the 900 Hz side tone through a −12 dB audio-filter LPF at cutoff 30:

| Keyboard mode | 900 Hz through the filter |
|---|---|
| SINGLE | 0.0345 |
| DUAL, before | 0.1540 |
| DUAL, after | **0.0345** |

The lever now rides the keyboard part's LFO2 and is counted once, which is
exactly what SINGLE already did — so SINGLE is untouched and DUAL and SPLIT
join it.

**A shuffled grid took its long/short parity from the pattern step.**
`arpeggioStepSeconds` decides which half of a shuffled pair a section is by
`stepIndex & 1`, and it was being given `arpeggioStep_`, which wraps at END
STEP. END STEP is 1–32 and odd values are ordinary, so the parity repeated and
the pair stopped summing to its division: at END STEP 1 on 1/8L every section
was the long half, and the arpeggio ran 16 % slow and drifted for as long as
the key was held. The shuffle belongs to the beat, not to the pattern, so the
parity now comes from a monotonic count of grid sections that resets when the
pattern re-arms. Eight sections of a shuffled eighth at 120 BPM, from the
first onset to the ninth:

| END STEP | before | after | owed |
|---|---|---|---|
| 1 | 2.319 s | **2.000 s** | 2.000 s |
| 2 | 2.000 s | 2.000 s | 2.000 s |
| 3 | 2.079 s | **2.000 s** | 2.000 s |
| 4 | 2.000 s | 2.000 s | 2.000 s |

The even lengths were already right, which is why a full pass over the
arpeggiator missed this. The tie chain's look-ahead reads the same counter, so
a chain that ties across a pair still takes DURATION from the section it ends
on. Five checks fence the two fixes, all watched to fail when reverted; the
committed demos are unchanged, because `11-arpeggiator.wav` runs an even-length
style.

#### Step 16 — hard sync corrected a partial jump as though it were a whole one

The sync reset is documented as naive: "the modelled DSP's own sync aliases
audibly, and no source documents band-limiting there." What shipped was
neither naive nor band-limited. The reset writes OSC1's phase so that the
following increment inside `renderClassicWave` lands it at the right
sub-sample position — which means the increment carries the phase past 1.0,
`renderClassicWave` sees an ordinary wrap, and it applies the full polyBLEP
residual. That residual describes a *whole cycle's* discontinuity; a sync
reset jumps by whatever fraction of a cycle OSC1 happened to have reached.

Measured with a saw an octave below its slave clock, so every downward jump in
the take is a reset and the naive one owes about −1 × peak:

| | before | after |
|---|---|---|
| post-reset value, median | −0.681 × peak | **−0.735 × peak** |
| post-reset value, worst | **−0.089 × peak** | −0.628 × peak |
| resets landing above −0.5 × peak | **17 of 74** | 0 of 59 |
| downward jumps in 0.45 s | **74** | 59 |

Fifty-nine is the number OSC2's fundamental owes; the extra fifteen were the
spikes themselves. Against the same patch rendered without the residual, the
shipping output differed by up to **73.6 % of its own peak**, at −32.8 dB RMS.

`renderClassicWave` now takes a flag and skips the residual on the sample a
sync forced the phase on — for every classic wave, since a jump the reset
made is not a discontinuity any of their residuals describe. Two checks fence
it, both watched to fail with the flag removed: the take must contain one
downward jump per slave cycle and no more, and every one of them must land at
the bottom of the saw. `05-sync-sweeper.wav` re-rendered 0.1 dB louder.

#### Step 17 — a received CC notified the host from inside the render callback

`handleController` runs on the audio thread, and for every mapped panel CC it
called `beginChangeGesture`, `setValueNotifyingHost` and `endChangeGesture`.
All three take the processor's listener lock and walk its listener list, and
an APVTS attachment's listener wakes the message thread; a controller sweep
makes 128 of them a second. The comment above the loop claimed cached pointers
kept it "allocation-free on the audio thread", which was true of the string
building it had already removed and not of what remained.

The idiom this project already uses for a MIDI program change applies exactly:
the audio thread writes the parameter's raw value — which is what the engine
snapshots and what `getStateInformation` serialises, so audio and saved state
are correct immediately and without the message loop — and marks the entry in
a dirty mask. A coalescing `AsyncUpdater` then republishes those values with
host and UI notification on the message thread, so a 128-message sweep costs
one pass per frame instead of 128 passes inside the render callback.

The fence is a parameter listener attached in the suite: a CC delivered
through `processBlock` must produce no value change and no gesture callback,
and the reconciler must produce both. Reverting the three calls fails it with
`values 1, gestures 2`.

#### Step 18 — a short chord fell back on the wrong key under the DOWN motifs

The manual says what an arpeggio style does when it asks for more note rows
than the player is holding keys: "When the number of keys played is less than
the number of notes in the arpeggio style, the highest-pitched of the pressed
keys is played by default" (OM p. 66). The sentence is in the MOTIF row of the
parameter list and carries no direction qualifier.

`arpeggioKeyIndexForRow` clamped the *window position* into the chord and then
reversed it for a descending motif, so the fallback came out at the far end:
two keys held under any of the shipped four-row styles gave the highest key on
UP and the **lowest** on DOWN, DOWN(L), DOWN(L&H) and every UP&DOWN. The
position is now tested before the reversal, and a row the chord cannot fill
takes the highest key whichever way the window is walking.

Nine motifs × four cycles are checked against the rule, and the two cases that
prove the walk is otherwise untouched — a full chord still reads its window
from the top on DOWN and still reaches the bottom of it. Reverting the change
fails twelve of them. The manual's own three worked examples hold five keys
under a three-row style, so they never reached this branch and still pass
unchanged; the committed demos are unchanged for the same reason.

#### Step 19 — the panel printed four values the manual does not use, and one
pair of buttons set the wrong interval

**INTERVAL was absolute where the manual defines it as an interval.** OM p. 30
says all three outcomes against OSC 1: "-OCT ... lowers the OSC 2 pitch one
octave below that of OSC 1", "the OSC 2 pitch will be seven semitones (a
perfect fifth) higher than OSC 1", and "if you press the -OCT button and the
5th button simultaneously, the OSC 2 pitch will be the same as the OSC 1
pitch". Both handlers wrote −12 and +7 absolutely and returned to 0, so any
patch whose OSC 1 was transposed got the wrong interval — the one thing these
two buttons exist to get right. They read OSC 1's pitch now, and the second
press lands OSC 2 on it. Both also light while the interval they name is in
force: they were the only controls on the panel that wrote a parameter without
reflecting it, so a patch loaded at OSC 2 = OSC 1 − 12 showed two dark buttons.

**Four readouts printed the stored byte.** The manual prints SPLIT POINT as
`A0–C8`, SIZE as `1–8`, PRE DELAY as `0.0–100.0 (ms)` and ARPEGGIO VELOCITY as
`REAL, 1–127`; the panel printed 60, 4, 0 and 0. All four now print what the
manual prints, in the same `intAttributes` function that already did it for
PAN and END STEP, so the host's parameter list says the same thing. The
pre-delay conversion moved into `mapping::reverbPreDelayMs` so the readout and
the reverb cannot disagree about what a raw value means. The four frequency
tables carry their `Hz` too, which is how the manual's own parameter list
prints them.

**Every toggle says which way it is thrown.** Eleven controls on the panel are
switches and all of them read `ON` whatever their state — a button whose face
says ON while the thing is off is the commonest misreading a synthesizer panel
invites. The face is driven from the button's own state change, so it is right
the moment a patch loads rather than at the next frame, and the three labels
that read `ON` above a button that also read `ON` now read `SWITCH`.

**Two layout defects.** Combos and buttons were centred in a cell that had not
given up the value strip a knob gives up, so they sat 6.5 px below the knobs in
their own row — on nearly every row of the panel. Every style reserves the
strip now, whether or not it prints in it. And the second INTERVAL button
carried an empty label, the panel's only orphaned text; a control with no
caption of its own now shares the one to its left, so `INTERVAL` spans both
buttons.

#### Step 20 — three parameters did not have the instrument's own positions

The contract's rule is that the address map is adopted verbatim. Three
parameters were not.

**KEY FOLLOW had 401 positions where the instrument has 41.** The MIDI
Implementation stores `FILTER Cutoff Keyfollow (44 — 84)` displayed
`−200 — +200`, so the control moves in steps of 10 and raw 64 is zero. The
parameter was a plain integer over −200…+200, which meant a host automating it
could set a value the instrument cannot store and a patch edited by knob could
not round-trip through the documented SysEx. Both the engine's clamp and the
panel's readout quantise to the grid now.

**Delay FEEDBACK had 197 positions where the instrument has 99.** Same
document, `Patch Delay 00 01: Feedback (0 — 98)` displayed `−98 — +98 [%]`:
99 raw values across a 197-wide display, so the display advances in twos and
raw 49 is 0 %. Every other signed field in the file honours its raw range;
this was the one that did not.

**PITCH WIDE gated the sounding pitch, and the manual says it gates the
knob.** `clampToDocumentedRanges` narrowed the coarse tune to ±12 semitones
when the switch was off. The address map keeps `OSC1 Coarse Tune (28 — 100)`
— the full −36…+36 — in a byte of its own, with the wide switch in another,
and the manual says what the switch does: "This button expands the range of
the PITCH knob by a multiple of three. If you press the WIDE button so it's
lit, the PITCH knob will have a range of ±3 octaves" (OM p. 29). It is the
knob's travel. The consequence of clamping the stored value instead was that
the panel and the host printed a pitch the engine did not play: a patch at
+24 with WIDE off showed +24 and sounded +12. Measured on a sine at note 36
(65.41 Hz) with coarse +24 and the switch off: **130.86 Hz before, 261.63 Hz
after** — one octave out, and 261.63 Hz is what +24 semitones owes.

On a numeric parameter there is no knob travel to gate, so PITCH WIDE is now
patch data that stores and round-trips and does not change what sounds. That
is what the documents settle, and it is one invented behaviour fewer.

Nine checks fence the three, all watched to fail when the changes are
reverted. No shipped preset sat off either grid or outside ±12 with WIDE off,
so the committed demos are unchanged.

#### Step 21 — the controller destinations, which every controller had ignored

Patch Common carries four bytes the contract mentioned only as "controller
destinations" and the engine did not read: MODULATION, D BEAM, PITCH BEND and
EXPRESSION DESTINATION, each UPPER / LOWER / BOTH. The manual gives them one
sentence apiece — "Selects the tone(s) whose pitch will be changed by the pitch
bend lever ... If this is 'BOTH,' the pitch of both the UPPER tone and LOWER
tone will change" (OM p. 65) — so in DUAL and SPLIT they decide which half of
the patch a lever reaches. All three whose controller the replica has are
implemented; the D Beam's arrives with the D Beam.

Bending only UPPER, on a DUAL patch with the two tones an octave apart and a
full-octave bend range on each, the surviving partials say which tone moved:

| PITCH BEND DEST | 523 Hz (UPPER bent) | 131 Hz (LOWER unbent) | 262 Hz (LOWER bent) |
|---|---|---|---|
| UPPER | present | present | — |
| LOWER | — | — | present |
| BOTH | present | — | present |

EXPRESSION had to move for this: it multiplied the master chain, so it could
only ever reach both tones. It is a per-tone gain now, smoothed exactly as the
chain it left was, and with BOTH — the default — the product is unchanged. At
EXPRESSION 0 with the destination on one tone, that tone's partial drops more
than 34 dB while the other one is untouched.

Seven checks fence the three destinations, all watched to fail when
`destinationReaches` is made to return true unconditionally. No shipped preset
sets a destination away from BOTH, so the committed demos are unchanged.

#### Step 22 — two states behind the AMP stage, one cleared and one frozen

**A voice taken over lost the delay line it reads from.** `triggerVoice`
guards the filter and shelf states with `if (! wasActive)` — a stolen voice
keeps them deliberately, because its envelope carries on from where it was —
and cleared `voice.overdrive` outside that guard, on every non-legato trigger.
The overdrive stage carries the group delay *every* voice pays, shaping or
not, so emptying its line under a sounding voice reads silence for the whole
of it. Measured in SOLO, where one voice can be watched on its own: a run of
**18 near-silent samples** where the reported latency is 19. The `clear()`
moved inside the guard, beside the filter's.

**The chain stood still while OVERDRIVE was off.** Two half-band stages, up
and down, plus the ADAA's antiderivative reference, all carrying across
samples, all behind an automatable switch. Frozen while the switch was out,
they answered it coming back with whatever was playing when it was last in.
Measured against the same note with OVERDRIVE on throughout — identical voice
state, so the two takes must agree once the chain has the same recent history
— the first block after the switch differed by **0.078 against a peak of
0.039**: twice the signal, from a third of a second earlier.

The fix is not to shape unconditionally. Every state in the chain is linear or
depends only on the last few samples, and the bypass delay line has been
keeping that history all along for its own reasons, so the stage replays the
ring on the switch's rising edge and rebuilds itself exactly. The same
measurement after: **0.005**, which is the two takes' output-coupling
capacitors holding different offsets rather than anything the chain did.
Cost, ten supersaw voices with delay and reverb at 48 kHz: 0.066 → 0.068 ×
realtime with OVERDRIVE off, unchanged at 0.115 with it on. Shaping
unconditionally would have cost 0.113 with it off — correct, and 40 times the
price of being correct this way.

Two checks fence them, each watched to fail with its own change reverted. The
committed demos are unchanged: no demo steals a voice or automates the switch.

#### Step 23 — the panel scales to the window

The editor was a fixed 1500 × 786 with no resizing, no constrainer and no
transform, and hosts honour the size an editor asks for. A 1366 × 768 laptop's
work area is under 768 points tall once the taskbar and the host's window
frame are counted, so the keyboard, the patch strip and the whole bottom band
were pushed off the edge with no way to get them back. The same happens on
1280 × 800 and on a 1080p screen at 150 % scaling.

A hardware instrument's controls do not reflow, and this panel is deliberately
a fixed geometry, so the fix is not to make the layout responsive but to stop
the window being fixed. Every control and every rule the panel draws now lives
on one child component that is always exactly the design size and is laid out
against a constant rectangle rather than against the window; `resized()` gives
that child an `AffineTransform` that scales it to whatever the window is and
centres it. This is the pattern Ghostar already carries in this repository.
The editor opens at the largest whole panel the display can show, never below
60 % of the design size — under that the 10-point captions stop being
readable, and a window the player can move is a better failure than type
nobody can read.

Twelve checks fence it: the fit rule itself on screens no build machine has to
have (1366 × 768, 1280 × 800, 1440 × 900 all fit, all keep the panel's
proportions, an unknown or roomy display opens at the design size), and the
existing placement check re-run at 900, 1500 and 2100 points wide — every
control placed, nothing past the panel's edge, because the layout never looks
at the window. The committed screenshot is pinned to the design size in the
suite rather than taken from whatever the build machine's display happens to
be, so a small CI display cannot quietly shrink the documentation image.

#### Step 24 — the System Common settings the engine already honoured

`setMasterTuneHz`, `setMasterKeyShift`, `setKeyboardOctaveShift` and
`setTranspose` have been in the engine since it was written, each clamped to
its documented range and each folded into the pitch sum. Nothing called them.
The only line in the whole tree that reached one was a test. Documented,
settled, unreachable: the same class as END STEP two rounds ago and SPLIT
ARPEGGIO one round ago, and the third instance in a row is the argument for
the check that now walks the panel.

All four are published as plug-in parameters, outside the patch exactly as the
external-input block is, so a program change does not touch them: MASTER TUNE
as a float over 415.30–466.20 Hz, which is the frequency of A4 the manual
prints for the address map's 0.1-cent steps, and the three integers over
−24…+24, −3…+3 and −5…+6. They take the right of the header, which is now a
section of its own — settings that apply to the whole instrument, where the
whole instrument's name is.

The panel's OCT UP/DOWN buttons write the octave shift rather than a private
field, so they reach the engine, reach the host, and reach the documented ±3
instead of the ±2 they were limited to; the drawn keyboard still follows them.

Ten checks fence it: A4 renders at 440 by default and sharper at 466.16
(reverting the wiring measures 439.28 against 439.28 either way), the buttons
reach ±3, a program change leaves the block alone, and it survives a state
round trip.

#### Step 25 — the lever, the meter, and the space the panel was not using

**The lever latched modulation to wherever you clicked.** Its vertical axis
holds its position, which the hardware's does too, but the value was taken
absolutely from the click's y — so one tap near the top of the travel jumped
modulation to full and left it there, with no obvious way back. The hardware
lever cannot be *put* anywhere by tapping; it is pushed. The axis moves by the
drag now, from wherever it was grabbed, a double click puts both axes back,
and bend stays absolute and spring-loaded as it was. The caption also had its
own band taken off the component before the frame and the stick are drawn, so
`BEND / MOD` no longer sits on top of the border and the lever's foot.

**The output meter had the host's buffer size in its ballistics.** It fell by
a fixed factor once per render call, so the same patch released sixteen times
faster at a 1024-sample buffer than at a 64-sample one. The fall is a time
now — 0.30 s to 1/e, registered as a display choice rather than a claim about
the instrument — and a check renders identical audio at both block sizes and
requires the same fraction to survive a quarter-second of silence. Reverting
the factor measures 0.000 against 0.197.

The scale was linear amplitude over a 44-pixel bar, so a healthy −20 dBFS
filled four pixels and the meter sat near its floor for everything that was
not about to clip. It reads in decibels down to −48 now, carries a −6 dB mark,
and turns to the panel's accent colour at full scale. It also takes the height
the performance cluster had spare instead of a fixed 44 points: it is the one
thing on the panel that reads better the taller it is, and that space was the
largest dead area on the panel.

**Two layout defects.** A section's grid rows sat at the top of whatever
height the band gave it, so FILTER ENV's single DEPTH row left 70 points empty
beneath it beside four full-height sliders; the rows are centred in the
content now. And a section whose width was set by its *title* rather than its
contents — PITCH ENV is two 34-point cells under a nine-character name — hugged
the left edge; its strip is centred in what it was given.

**The reverb's four remaining settled bytes have controls.** LF DAMP, LF GAIN,
HF DAMP and HF GAIN are Patch Reverb parameters, are used by the engine's
per-line damping, and were automatable with nothing on the panel naming them
— while PRE DELAY, HIGH CUT, DENSITY and DIFFUSION, equally editor-only on the
instrument, were all there. REVERB is a 6 × 6 section now and carries its
whole documented parameter set. The panel is 1660 × 850 for it, up from
1500 × 786: the effects band needs 1474 points of content, and since Step 23
the window is no longer where the panel's size is decided.

#### Step 26 — the D Beam

The last of the three front-panel features the contract deferred, and the one
the plan's own §2.5 said needed "a design decision about what an infrared
distance sensor means inside a plug-in". It means a group of automatable
parameters, and the documents settle nearly all of them.

Three buttons under the beam choose what it does, each a toggle (OM pp. 20–21).
**PITCH** changes the pitch and answers on CC#69, which the control-change
list names "Part Pitch (D Beam Pitch Mode)". **EXPRESS** changes the volume,
or — with ACTIVE EXPRESSION on — combines the two tones: "Only the UPPER tone
will be heard when the volume is low, and the LOWER tone will be added as the
volume increases". **FILTER/ASSIGN** moves whichever of the 37 documented
destinations D BEAM ASSIGN names, and the manual settles the *law* as well as
the list: "the D Beam controller will have the same function as that knob… you
can also choose the direction in which the knob will be moved… the LFO speeds
up, just as if you had moved the LFO RATE knob toward the right." So the beam
takes the parameter from where the patch has it toward one end of its own
documented range, and POLARITY picks the end — for ASSIGN alone, since the
manual says plainly that it "will not change the direction of the change that
occurs when the PITCH button or EXPRESS button is lit".

The engine renders a patch that is the player's patch with the beam's one
assigned parameter moved, so no destination needed its own code path: the
37 entries are 37 fields, and everything downstream reads the rendered patch
it already read. D BEAM DESTINATION gates it per tone like the other three
controller destinations, and "Moving your hand outside this range will produce
no effect" is why the beam control is the hand's height with zero meaning the
hand is out.

**D BEAM SENS is settled in range and inert.** It compensates the infrared
sensor for "strong direct sunlight or strong artificial illumination". There
is no sensor here and no sunlight, so it is stored — a SysEx round trip has to
be lossless — and changes nothing that sounds, exactly as PITCH WIDE does.
Inventing a depth law for it would have been inventing the answer to a
question the manual settles the other way.

Fifteen checks fence it, watched to fail with the beam disabled: the beam
opens the filter it is assigned to and a beam at rest does not, polarity
inverts it, the destination gates it, PITCH mode carries note 48 to 261.63 Hz
over a 12-semitone bend range, EXPRESS carries the volume, ACTIVE EXPRESSION
holds LOWER back at a low beam and brings it in at a high one, the assign list
runs all 37 entries from OSC1-PITCH to BENDER, CC#69 moves the beam, and a
program change leaves the beam, the button and the sensitivity where the
player left them.

What is voiced is in OQ-16: that the three buttons are exclusive, PITCH mode's
reach and direction, the linear shape of the ASSIGN travel, the point ACTIVE
EXPRESSION starts adding LOWER, and whether the shared destinations follow
D BEAM DESTINATION. A recording of the beam's own MIDI output at a grid of
hand heights settles the first four.

The panel puts it on the bottom row beside the lever and the keys, which is
where the instrument keeps its performance controls; the row is a section tall
now and the panel is 1660 × 930.

#### Step 27 — the D Beam comes out

*A scope decision by the user, not a measurement.* An infrared distance
sensor reads how far a hand is above the panel; a plug-in has neither. Step 26
had built it as a group of automatable parameters, which is the only reading a
plug-in can give it, and the user's answer to that reading was that it is not
worth having.

What came out: the mode, the beam's own value and D BEAM SENS, the engine's
whole beam path (`applyDBeam` and its 37-case switch, `dBeamPitchSemitones`,
`dBeamGain`, the per-tone EXPRESS smoother), the panel section, and nineteen
checks across two test functions.

What stayed, and why. Four Patch Common bytes — 00 16 D BEAM DESTINATION,
00 19 ACTIVE EXPRESSION, 00 1F D BEAM ASSIGN, 00 20 D BEAM POLARITY — are
patch data, and the project's standing promise is that a SysEx round trip is
lossless. Deleting them would have made every dump Septum writes assert
D BEAM DESTINATION = UPPER and D BEAM ASSIGN = OSC1-PITCH to any real unit
that loaded it, because `encodePatchCommon` zeroes the block first and two of
the four defaults are not zero. They are stored, saved, program-changed with
the patch, and published as **non-automatable**: a host has no business
offering a lane that cannot change what the player hears. ACTIVE EXPRESSION is
stranded by this and stays stranded — the manual defines it only as a modifier
of the beam's EXPRESS button, and re-pointing it at the expression pedal would
be inventing a mechanism. CC#69 is accepted and ignored.

D BEAM SENS is gone outright, and the justification it used to carry was
wrong: it is System Common 00 1D, and this replica implements no System Common
SysEx block at all, so nothing ever encoded or decoded it. The round-trip
argument was true of PITCH WIDE and of the four patch bytes and was never true
of SENS.

The removal collapsed the engine's `rawPatch_`/`patch_` and
`rawExternal_`/`external_` pairs, which existed only so the beam could render
a patch one parameter away from the player's — three full-`Patch` copies per
block off the audio thread. Every beam entry point was an exact no-op at rest,
so the check is that **the committed demos re-render bit-identically**, and
they did. OQ-16 is withdrawn rather than deleted.

#### Step 28 — the panel says which tone it is editing

The user's report was that "the lower/upper thing is confusing". It was, and
an audit found ten distinct meanings of UPPER/LOWER on one panel. The edit
target was stated in exactly one place — two uncaptioned buttons in the patch
strip, below every one of the fifty-seven per-tone controls they govern,
drawn in the same lit-accent vocabulary as eleven parameter toggles — and
nowhere else. Four sections mixed per-tone and shared controls with no visual
difference between them.

- **Every section is now wholly per-tone or wholly shared**, the line the
  parameter contract already draws between the Patch Tone blocks and Patch
  Common. Three controls moved to make that true: DELAY DEPTH and REVERB DEPTH
  are Patch *Tone* bytes and are now DLY SEND and REV SEND in AMP, and
  PORTAMENTO, GLIDE TIME, POLY/SOLO, BEND and TONE OCT — five more Patch Tone
  bytes, previously scattered between the global performance cluster and the
  patch strip — became a TONE PLAY section on the keyboard row, in the space
  the D Beam vacated. An assertion fences it: a section with both kinds of
  control is a build-time failure.
- **Every per-tone section wears the tone's name on its title row**, in that
  tone's colour, and the well behind it carries six per cent of the same
  colour. Switching target repaints the whole per-tone half of the panel.
  A hollow grey chip means the tone is being edited and the keyboard mode is
  not letting it sound.
- **EDIT TONE moved into the header**, above everything it governs, with a
  caption, drawn as tabs rather than lamps, and a line beside it that states
  the consequence of the current keyboard mode in words. The target persists
  across closing the editor.
- **The keys say which tone they reach**: one band in SINGLE, two stripes in
  DUAL, and in SPLIT the two zones with the split point drawn where it falls
  and named.
- Controls the keyboard mode ignores — PART outside SINGLE, SPLIT POINT and
  SPLIT ARPEGGIO outside SPLIT — are dimmed. The three controller
  destinations read MOD/BEND/EXPR TO TONE. BALANCE, PAN and TONE BAL print the
  number the manual prints and name the two ends of their travel underneath.
  Switching target re-binds only the per-tone controls.

**One bug, found while reading.** The drawn keyboard applied SYSTEM COMMON
Octave Shift a second time: `applyKeyboardOctave` moved the drawn note range,
and a clicked key goes to `engine.noteOn` unmodified, where the engine applies
the shift itself. One press of OCT UP transposed the on-screen keys by two
octaves while their printed names claimed one. The keys keep their notes now
and the printed octave names move, which is what the shift does to the pitch
they sound.

The panel is 1660 × 862.

#### Step 29 — three inventions, removed

Commit e7d2967 landed three changes to the render code under the heading
"faithful … DSP voicing". None of them is faithful to anything: each invents a
constant or a mechanism, two of them assert a fact about the SH-201 that no
source in the contract states, and none was recorded in this document. They
are reverted, and the measurements are here so the record is not just an
assertion in the other direction.

**A "tube" stage in front of the overdrive.** `in * (1 + 0.08 * tanh(in))`,
commented "introduces gentle 2nd harmonic warmth", applied inside the shaper
chain. The instrument's overdrive is one algorithm on one Roland DSP; there is
no tube. The coefficient is invented, the constant was in the render code
rather than in `mapping`, and it broke the anti-aliasing: `shape()` divides by
the step in `logCosh`, the antiderivative of `tanh`, and the function actually
evaluated was `tanh ∘ tube`, whose antiderivative that is not. Measured on a
440 Hz sine at DRIVE 90, 96 kHz, harmonics relative to the fundamental:

| | h2 | h3 | h4 | h5 | h6 |
|---|---|---|---|---|---|
| with the tube stage | −50.6 dB | −9.75 | −51.5 | −14.5 | −52.8 |
| after | **−66.9** | −9.75 | **−72.1** | −14.5 | **−73.9** |

The odd harmonics — the ones a symmetric clipper makes — are unchanged to two
decimal places; the even ones drop 16 to 21 dB to the measurement's own
leakage floor. `testAsymmetricOverdriveHarmonics` went with it: it asserted
only a peak ceiling, measured no asymmetry and no harmonics, and passed
identically with the mechanism removed.

**NOISE as a Galois LFSR's state.** The comment claimed "Roland VA 23-bit
Galois LFSR with polynomial x^23 + x^18 + 1" — a statement about Roland's DSP
that nothing in the primary-source list settles. The *bit* sequence of a
Galois LFSR is white; its successive 23-bit *states* are not, because
`new = old>>1 ^ mask` makes `v(n+1) = ½v(n) + ½b(n)`, a one-pole filter. The
wave was a filtered bit stream whose colour depended on the host rate.
Band-averaged power through the shipping voice, filter bypassed:

| fs | 100–500 Hz | 1–4 kHz | 8–16 kHz | tilt |
|---|---|---|---|---|
| 44.1 kHz, before | −73.2 dB | −73.4 | −83.7 | **−10.9 dB** |
| 96 kHz, before | −77.5 | −77.5 | −82.8 | −6.5 |
| 192 kHz, before | −81.7 | −80.4 | −81.7 | −1.1 |
| 44.1 kHz, after | −79.0 | −77.2 | −79.1 | **−0.7** |
| 96 kHz, after | −81.5 | −81.2 | −81.9 | −1.3 |
| 192 kHz, after | −85.1 | −85.0 | −84.4 | −0.6 |

The contract's position is that NOISE is white until OQ-03 closes, and it is
white now, at every rate. The test that guarded it checked bounds, mean and
RMS and passed for any coloured source; it is replaced by one that measures
the spectrum at three rates.

**A coupled second stage in the −24 dB filter.** `max(0.12, 0.40·k₁ + 0.35)`
replaced the registered constant 1.2, under a comment reading "In the Roland
SH-201, resonance couples into the second stage" — while OQ-08 says in this
document that whether the hardware resonates on one stage or both is open, and
§2.7's own measurement table was left describing the old behaviour. Three
fitted numbers, no measurement, no listening test, no step. Reverted. Measured
on noise through the LPF at cutoff 64, resonant band referred to the filter's
own passband, 96 kHz:

| RESONANCE | −12 dB before | −24 dB before | −12 dB after | −24 dB after |
|---|---|---|---|---|
| 64 | +3.91 dB | +8.30 | +5.89 | +4.38 |
| 100 | +11.30 | +18.18 | +13.75 | +12.20 |
| 120 | +18.22 | +23.98 | +26.97 | +25.31 |

The ordering is what matters: the −12 dB path is the resonant one, so it must
peak at least as hard as the −24 dB path, and with the coupled stage it did
not. A test pins that ordering at three resonance settings, so the next repin
cannot be silent.

#### Step 30 — the filter's limiter was a full-time waveshaper

`filterStateLimit` exists to bound self-oscillation — "the manual's 'may not
stop at all' is a bounded oscillation on hardware". It was applied at every
resonance and in both stages. The TPT SVF's integrator states sit at roughly
the signal's own magnitude at low frequency, and two oscillators at unity put
more than ±1.5 into the filter, so an ordinary patch was soft-clipped in the
filter's feedback path with the resonance knob at zero.

Measured at 96 kHz, two sines at BALANCE 0 through the LPF at cutoff 127 and
RESONANCE 0, harmonics 2–12 against the fundamental: **THD −27.3 dB before,
−62.4 dB after**. The gate is `damping <= 0`, which is the stage's linear
stability boundary and not a new constant; `filterStateLimit` keeps its value
and its OQ-08 ownership, and the self-oscillation tests that fence the
divergent case still pass.

#### Step 31 — four control changes that stepped the output

Step 11 established the measure — the largest sample-to-sample jump a change
makes, against the same take's own steady travel — and the bound, four times
that. It was applied to the external-input path only. Four more controls fail
it, three of them louder. Measured at 44.1 kHz on a 65 Hz sine through a
−12 dB LPF at cutoff 30, resonance 110:

| Change | before | after |
|---|---|---|
| FILTER TYPE LPF → HPF | 116× | **0.9×** |
| FILTER TYPE LPF → BPF | 77× | **1.2×** |
| FILTER TYPE LPF → BYPASS | 31× | **0.8×** |
| FILTER SLOPE −12 → −24 | 84× | **1.0×** |
| S&H LFO → AMP | 206× | **3.7×** |
| AMP LEVEL 20 → 127 | 3840× (0.108) | **40× (0.0011)** |

The filter's TYPE and SLOPE are crossed with the same registered constant the
external switches use, and both stages now run unconditionally — BYPASS
included, because freezing the integrators while bypassed lets an old resonant
tail out of them when the switch comes back, which is the reasoning the audio
filter already carried. All four responses come off the same two integrators,
so a cross costs a select rather than a second filter.

The amp gain goes through the same two-stage treatment the cutoff has had
since Step 1: the *panel* side — LEVEL, the velocity offset, PAN and an LFO on
the AMP destination — is slewed by `controlSlewSeconds`, and the result is
walked across the control tick sample by sample. The amp envelope is
deliberately outside it, exactly as the filter envelope is, so the documented
"fast ADSR response" is untouched. AMP LEVEL's ratio stays large because the
reference is a quiet sine's own travel; the absolute jump falls from 0.108 to
0.0011.

LOW FREQ joins them: its shelf ran only in BOOST and CUT, so the state froze
in FLAT and a stale one came back when a position returned, and its
coefficients — `std::pow` included — were recomputed once per sample per
voice. It runs at every position now, its contribution crossed rather than
switched, and the constants are hoisted.

#### Step 32 — three defects in the envelopes and the LFOs

**A freed voice carried the previous note's filter-envelope level.** A voice
is freed on the amp envelope alone; the filter envelope simply stopped being
advanced and kept whatever its release had reached. Play the same key twice
through a patch whose filter release outlasts its amp release — the ordinary
plucked sound — and the second note skipped its whole sweep. Measured at
44.1 kHz with a 0.9 s filter attack, RMS over the first 20 ms: note one
0.0041, note two **0.0824**, twenty times as open. The envelope is re-armed
for a voice that was not already sounding, inside the guard that already
distinguishes that case; a *stolen* voice keeps its level, which is what this
document says it should.

**Raising SUSTAIN under a held note snapped the envelope.** The decay
segment's convergence test was one-sided, so a sustain raised above the
current level passed it on the first sample and the next one assigned the new
sustain outright. SUSTAIN is automatable and `setPatch` reconfigures every
sounding voice, so an ordinary control move or a program change put the whole
difference into one sample: measured 0.0246 → 0.1268 in under a millisecond, a
jump of 0.100 against the take's own steady 0.00034. The test is two-sided
now; no constant changed.

**All four LFOs drew the same random numbers.** One shared seed default, never
re-seeded, so a patch's UPPER LFO 1, UPPER LFO 2, LOWER LFO 1 and LOWER LFO 2
walked one sequence: two S&H modulators at the same rate produced
bit-identical output, and in DUAL the two tones stepped together. Seeded per
LFO from fixed constants, so a render stays reproducible.

#### Step 33 — the analog output stage and the settled damping tables

Two places where a published frequency was stored and then not delivered.

The output stage clamped both RC poles to 0.49·fs, which put **both of them on
one frequency** at every host rate at or below 48 kHz — 21.6 kHz twice at
44.1 kHz — and used the time-constant one-pole, whose −3 dB point is not the
frequency it is given. The stage measured up to 0.94 dB brighter at 20 kHz
than the network the service notes describe, with the error largest at the
commonest rates. The four settled damping tables went through the same
conversion: at 44.1 kHz the 8000 Hz HF-DAMP entry turned over at 9055 Hz, the
10000 Hz one at 12429 Hz, and the 12500 Hz HIGH CUT entry never reached −3 dB
at all.

`mapping::onePoleAtCorner` solves for the pole that puts the −3 dB point on
the corner it is given — `p = c − √(c²−1)` with `c = 2 − cos ω` — which is
exact at every rate, with no fitted number. A corner at or above Nyquist has
no −3 dB point to hit, so there the coefficient matches the analog magnitude
at Nyquist instead; both ends are then exact. Every table entry now turns over
on its own frequency at 44.1, 48 and 96 kHz, and the output stage's worst
in-band deviation from the component values falls from 0.94 dB to under 0.9 at
every rate and to 0.27 dB at 22.05 kHz, where it used to be 0.87 dB the wrong
way.

That change is audible in one existing test. `testOverdriveSwitchesBackInFromLiveState`
compares two takes that ran different signals for a third of a second, so what
it measures after the switch is partly the output stage's own memory — and a
more accurate stage is a slower one. The residual grew from 0.139 × peak to
0.247, against 2.0 for the defect the test exists to catch; the bound is
restated at 0.35 with the same margin it had before, and the reason is in the
test.

#### Step 34 — the reverb cancelled itself in mono

`wetReverbL` and `wetReverbR` were two overlapping alternating-sign windows
offset by one tap, so `L + R = taps[0] − taps[6]`: five of the seven lines
they used cancelled exactly in a mono sum, and the eighth line the geometry
pays for reached neither channel. Measured at 48 kHz on the tail from 0.8 s:

| Template | correlation before | mono − side before | correlation after | after |
|---|---|---|---|---|
| Hall 2 | +0.19 | +1.7 dB | +0.90 | +12.8 dB |
| Plate 1 | −0.81 | **−9.6 dB** | +0.51 | **+4.9 dB** |
| Plate 2 | −0.83 | **−10.1 dB** | +0.50 | **+4.8 dB** |

The two channels are disjoint halves of the network now — even lines left, odd
lines right — which uses all eight and keeps the whole tail in a fold-down. It
is not a defensible alternative formulation that was replaced; it was an
accident of picking two overlapping windows.

#### Step 35 — PAN's documented centre

PAN is `L64…63R`, so the centre it prints is 0. The law mapped the raw range
onto the pan angle as `(pan + 64) / 127`, whose midpoint is 0.504, so PAN 0
sat 0.107 dB left of centre — on every INIT patch and every preset that leaves
PAN alone. The received CC#10 pan in the same engine already put its own centre
where the map says it is. The two halves are now mapped around the documented
centre; both endpoints are unchanged.

#### Step 36 — FB OSC was a different sound at every host rate

`fbOscLoopDamping` was applied as a per-sample coefficient, so the one-pole
inside the feedback loop had its −3 dB point at 0.134 × fs: 5.9 kHz at
44.1 kHz, 25.8 kHz at 192 kHz. It sets the loop gain at every comb resonance,
so the same patch was 3 dB louder and a quarter of an octave brighter at
96 kHz. That is the character of the port rather than of the instrument — the
thing plan Step 2 ruled out for the overdrive in the same words — and it also
means a capture aimed at OQ-06 could not be matched against a model whose
partial structure is not a fixed function of the patch.

The constant is unchanged and is now quoted at a reference rate, so 44.1 kHz
is bit-identical and every other rate matches it. Spectral centroid at
FEEDBACK 127:

| | 44.1 kHz | 96 kHz | 192 kHz |
|---|---|---|---|
| note 60, before | 545.3 Hz | 542.9 | 540.8 |
| note 60, after | 545.3 | **545.9** | **545.8** |
| note 48, before | 498.7 | 494.2 | 486.2 |
| note 48, after | 498.7 | **501.9** | **503.0** |
| RMS at note 84, before | 0.0479 | 0.0682 (+3.1 dB) | 0.0767 (+4.1 dB) |
| RMS at note 84, after | 0.0479 | 0.0388 | 0.0355 |

The level still moves by 2.6 dB across that span, in the other direction: the
saw inside the loop is not band-limited, so how much alias energy folds back
into it is still a function of the rate. Band-limiting it would change what
FB OSC sounds like at 44.1 kHz too, on a mechanism OQ-06 owns, so it is an
A–Z candidate and not a unilateral edit. The fence is on the mechanism as well
as the audio: the loop filter's time constant in seconds must be the same at
every rate.

#### Step 37 — the SysEx codec, and three things it dropped

**The tie shared a byte with velocity 127.** A grid cell is a rest, a tie, or
a note-on carrying its velocity; the tie encoded as 0x7F and so did a velocity
of 127. Every one of the sixteen shipped styles opens on a cell the style
table built at exactly 127, so a dump and a reload turned the loudest step of
every pattern into a hold on nothing. A cell's velocity now stops at 126 —
clamped where every other documented range is clamped, so no value the engine
can hold is a value a dump cannot carry — and the tie keeps 0x7F to itself.
Nothing caught it: no factory patch loads a style, so every cell the existing
round-trip test saw was a rest.

**PATCH TEMPO above 255 did not round-trip.** Two nibbles carried eight bits
of a documented 5–300 range: 300 came back as 44, and 256–300 that were not
multiples of 16 silently kept whatever tempo was loaded. The arpeggiator and
both LFOs' tempo sync run off that one number. It is Roland's own two-byte
7-bit split now.

**END STEP had no byte at all.** It is the replica's own front-panel control
(0 meaning "as long as the template is"), separate from the length the
template defines, and the block encoded only the template's. It has a byte of
its own; the grid starts one later.

The round-trip test now runs every shipped style through all 512 of its cells,
checks END STEP, checks the tempo at 5/120/200/255/256/299/300, and checks a
byte-for-byte identity over the whole Patch Common block — which is also what
fences the four inert D Beam bytes Step 27 kept.

*Superseded in part by Step 40.* All three defects were real and the tests
that caught them still run, but the repairs were reasoned out where the layout
was written down: the tempo's byte split, the velocity cap and the byte chosen
for END STEP all went when the address map was read.

#### Step 38 — a note the arpeggiator could not let go of

A parameter change and a note-off can land at the same sample position, and
then no audio is rendered between them. The arpeggiator's routing transition
lived only in `advanceArpeggiator`, so in that case the note-off was consumed
against the *new* routing — removing nothing from the plain voice's held list
and releasing nothing — and the transition then copied the still-held key into
the chord. The arpeggiator went on playing a key the player had let go of, and
nothing short of a panic could end it: measured 0.043 RMS immediately, 0.039
one second later, **0.039 ten seconds later**. Four automatable controls reach
it. The transition is noticed in `noteOn` and `noteOff` as well now, which is
the same move Step 10 made for the chord re-arm.

Two more from the same reading. **Voice stealing took the oldest-triggered
released voice**, not the longest-released one this document and the code's own
comment both promise — the engine had no record of when a voice entered
release, so hold a bass note, play and release a melody note over it, then
release the bass, and the steal landed on the bass's fresh tail rather than the
melody's stale one. Voices carry a release stamp now. And **re-pressing a key
already in the arpeggio chord did not update the velocity the chord entry
plays at**: with ARPEGGIO VELOCITY = REAL the second, harder press went on
sounding at the first press's dynamics for as long as the chord was held.

#### Step 39 — three defects the documentation audit turned up

**Demo 06 had stopped rendering ring modulation.** `bankPatch` matched a
preset by name and fell back to `initPatch()` when it found none; the ring
preset was renamed from "Ring Bell" to "Glass Bell" in e7d2967, so the demo
rendered a plain saw under the title "ring modulation" — and passed every
guard the renderer has, because INIT is a perfectly good saw. The lookup
aborts on an unresolved name now. A fallback that renders a *different
instrument setting* is worse than a build failure, and the README's claim that
the demos "cannot drift from the code" was resting on it.

**Every User-bank program name was a dangling pointer.** `NamedPatch::name`
was a `const char*` and the 32 User slots built theirs from a `std::string`
local to the loop iteration. `getProgramName` handed those pointers to the
host. `NamedPatch::name` owns its storage now.

**A received SysEx dump notified the host from the render callback.** The
audio path called `loadPatch`, which builds a `juce::String` per parameter and
calls `setValueNotifyingHost` — an allocation and a host notification inside
`processBlock`, the exact defect Step 17 removed for control changes. It is
split the same way: the audio thread writes the raw values inside the seqlock,
and a queued message-thread pass republishes them.

#### Step 40 — the address map, read

Step 37 fixed three things the codec dropped and left two marks in the code
saying *this byte is the project's own, not Roland's* (`OQ-17`): the DELAY and
REVERB switches packed into one byte as a bitmask, and a tempo encoding chosen
because it could carry 5–300 rather than because a document said so. The way
to close that is not to reason harder about it. The **SH-201 MIDI
Implementation v1.00 (2006-03-01)** publishes the Parameter Address Map; it is
already listed as a primary source in the research contract. It was fetched
and read.

Both marked bytes were wrong, and so were four more.

| Offset | The map | The codec |
| --- | --- | --- |
| `#00 0E/0F/10` | Patch Tempo, **three** nibbles | a two-byte 7-bit split |
| `00 14` | Split Arpeggio (UPPER/LOWER/BOTH) | a DELAY+REVERB bitmask |
| `00 1A` | Arpeggio Switch | — |
| `00 1B` | Arpeggio Hold | — |
| `00 1C` | Delay Switch | Arpeggio Switch |
| `00 1D` | Reverb Switch | Arpeggio Hold |

Three structural divergences came with them.

**The block addresses are absolute.** Temporary Patch is at `10 00 00 00`, not
zero, and User Patch 001–**032** at `20 00 00 00`…`20 1F 00 00`, one step of
`00 01 00 00` apart. The bank writer had been computing
`20 00 00 00 | bank<<16 | (slot*8)<<8`, which collides with the block offsets
it then adds; a "bank file" it wrote addressed nothing on a real unit. It
writes 32 slots at the map's addresses now, and the parser splits patches on a
change of the two high address bytes rather than on seeing a particular block.

**The arpeggio grid is sixteen blocks, not one.** Patch Arpeggio Common is
eight bytes — grid, duration, motif, octave range, accent rate, velocity, and
END STEP as two nibbles at `#00 06/07`, so Step 37's byte for it was in the
right block by luck and the wrong place in it. The 32 × 16 grid lives in
sixteen **Patch Arpeggio Pattern (Note 1…16)** blocks at `00 06 00`…`00 15 00`,
`00 00 00 42` each: an Original Note, then Step1…Step32, every field a
two-byte nibble ranged 0–128. A patch is 22 DT1 blocks.

That range is also the end of Step 37's tie problem. 0–128 is 129 values and a
cell has exactly 129 states — a rest, 127 velocities, a tie — so nothing has
to be clamped away from anything: the velocity cap at 126 is gone and cells
carry 127 again. And each row carrying an **Original Note** of its own is the
first hard evidence about PHRASE, the one motif whose row-to-interval rule the
manual never works through. What reads the field is not documented, so the
engine stores it and PHRASE keeps its voiced reading (OQ-15) — but a style's
rows are recorded pitches, not just positions, which is worth knowing.

**Reverb LF/HF Damp Gain is not a ±64 field.** Raw 0–36 counts up from
−36 dB. Encoded as a biased signed value it put 0 dB at byte 64 and −36 dB at
byte 28, both outside the documented range, on a field whose two ends the
delay templates actually use.

One byte is deliberately left odd. The map prints LFO1/LFO2 Tempo Sync Switch
as `ON, OFF` where all 26 of its other switches read `OFF, ON`. Printed twice,
once per LFO — so either 0 really is ON, or Roland's document is wrong the same
way twice. The codec writes it as printed and `OQ-18` records the one
measurement that separates the two: a dump from a real unit.

What fences all of it: the document prints two finished SysEx messages of its
own, and both are now test vectors reproduced byte for byte — including the
RQ1 that requests `00 00 15 42` of Temporary Patch, which is exactly where this
codec's last block ends. Beyond those, a test asserts each relocated Patch
Common field from its own offset, a test asserts all 22 block addresses, and
the arpeggio round-trip runs per-row through the pattern blocks. 2799 engine
checks and 174 plug-in checks pass.

The README no longer says the codec's layout is the map's "where this document
can show it and the project's own where it cannot". It is the map's.

#### Step 41 — two defects a review found in Step 38's repairs

**A switch changed again mid-cross stepped the output.** FILTER TYPE and the
audio filter's TYPE each have four positions, and Step 31 crossed them with the
position they were coming from plus one fade scalar. That pair holds exactly
one outgoing signal, so a second change part way through the first cross either
re-aimed the destination while the fade was already non-zero, or — moving back
to where it started — collapsed the whole expression onto the source in a
single sample. Both step the output by however much had been mixed in, which
is the discontinuity the cross exists to prevent. Measured with three changes
inside one 5 ms cross: 0.045 against a steady 0.0012 on the voice filter and
0.078 against 0.0057 on the audio filter. Both switches carry a weight per
position now, no weight moves faster than the fade rate, and the mixture
survives a change. The bound is Step 11's, under four times the take's own
steady travel; the single-change cases are unchanged.

**All Notes Off rewrote the age of tails already decaying.** Step 38 gave each
voice a stamp for the moment it entered release, so the steal could take the
stalest tail. All Notes Off and a hold-pedal lift both sweep the whole voice
array, and both were stamping every active voice — including the ones already
in release, whose stamps are precisely the ordering being kept. After an All
Notes Off the ten tails were therefore ordered by their slot in the array, and
the steal took whichever voice sat first rather than whichever had been
decaying longest. Every release now goes through one function that stamps only
a voice not already in release. Fenced by a steal after an All Notes Off where
the stalest tail is deliberately *not* the one in the first slot.

#### Step 42 — the Receive Data section, read

Step 40 read the address map. The same document's **Receive Data** section
(pp. 1–2) is the authority on the messages that are not parameter edits, and
reading it turned up one defect and one gap.

**All Notes Off was a second All Sounds Off.** The document gives the two
different behaviour, and gives All Notes Off a proviso: "all notes on the
corresponding channel will be turned off. *However, if Hold 1 or Sostenuto is
ON, the sound will be continued until these are turned off.*" The replica
released every voice unconditionally and cleared the sostenuto latch with
them, so a sustain pedal that was still down had its notes taken away, and a
sostenuto latch set before the message was gone even though its own pedal had
not moved. All Notes Off is now every key coming up: the key lists and the
arpeggiator's physical presses are cleared one at a time — so ARPEGGIO HOLD
latches on the last one exactly as it does when the player lifts their hands —
and then only the voices no pedal is holding are released. All Sounds Off,
which is the panic, is untouched.

**Three Universal Realtime messages were ignored.** The document lists them as
received and names the SYSTEM COMMON parameter each one changes: Master Volume
(`04 01`) → MASTER LEVEL, Master Fine Tuning (`04 03`, ±100 cents) → MASTER
TUNE, Master Coarse Tuning (`04 04`, ±24 semitones) → MASTER KEY SHIFT. All
three parameters are already published here, so all three messages now land on
them, through the same audio-thread split every other received message uses:
raw atomics in the callback, host and UI notification from a queued pass.

Two things in that section stay unimplemented on purpose, and the contract now
says why: the Identity Request reply, because the plug-in transmits no SysEx at
all; and the Active Sensing 420 ms timeout, which is a cable-failure watchdog —
a plug-in has no cable, and a host that sends one `FE` and then goes quiet
during a pause would have the sound cut out from under it.

#### Step 43 — a lost controller value, and 25 KB of zeroing per note

**A received CC could be lost outright, not just delayed.** Step 17 split a
received control change into an audio-thread write and a message-thread
publish, and the publish read the value back from the parameter object's own
storage. That storage is the same atomic the audio thread writes and the
engine renders from, and `setValueNotifyingHost` writes it: a CC arriving
between the read and the publish had its value overwritten with the older one,
and the dirty bit that CC had set then made the next pass republish the stale
value it had just been clobbered with. The controller's move never reached
either the host or the engine. The audio thread now writes a shadow only it
writes, the publish reads that, and a value that arrives mid-publish is put
straight back rather than a frame later. The test stages the clobber the race
leaves behind and fails against the old read.

**Every note-on zeroed both oscillators' feedback lines.** `clearRuntime()`
filled the whole 70 ms comb for OSC 1 and OSC 2 on every non-legato trigger,
whatever waveform either was set to: 2 × 3095 floats at 44.1 kHz and 2 × 13448
at 192 kHz, per voice, inside the render callback — just over 1 MB for a
ten-note chord at 192 kHz. Writes walk the line from zero and wrap, so
whatever is in it is always a prefix; the prefix length is tracked and only
that much is cleared. A voice that has never run an FB OSC clears nothing.

Recorded rather than changed, from the same reading: the half-period delay
stops tracking pitch below about 7.14 Hz, where the 70 ms line runs out. That
is under the bottom of the keyboard (MIDI note 0 is 8.18 Hz), so it is only
reachable by pushing a low note further down with COARSE or the octave shifts,
and a line long enough for COARSE −36 at note 0 would be 7.5 MB across the
pool at 192 kHz for a fundamental below hearing. The bound is now stated in
the contract's FB OSC section under OQ-06 instead of being silent.

#### Step 44 — the two remaining Roland documents

The SH-201 Q&A (2009) and the TurboStart leaflet are the last two Roland
documents this project had not read. They confirm three things already
settled — the 32 + 32 bank layout, the 32 arpeggio templates, and that
TRANSPOSE composes with the octave buttons rather than replacing them ("press
OCT UP once … then, while holding CANCEL, press OCT DOWN three times", which
is what the engine's `octaveShift·12 + transpose` already does) — and turn up
one divergence.

**PATCH REMAIN.** It is a System Common switch (`00 04`) and Roland states
what it does: "you can change from one patch to another without cutting off
the notes of the first patch", so the default it implies cuts them. This
replica does neither. A program change sprays the new patch over the
parameters and the notes still sounding adopt it mid-flight — a third
behaviour, arrived at by structure rather than chosen. Doing either documented
one properly needs a per-voice snapshot of the patch a note was struck under,
which one live `Patch` shared by every voice cannot give. Recorded as `OQ-19`
and named in the README, not picked by taste.

#### Step 45 — three defects a second review round found, and the grid finally lands

**A patch dump could lose whole blocks.** Step 43 fixed this for control
changes; the patch republish had the same shape and was missed. It read each
value back from the parameter object's own storage — the atomic the audio
thread writes and the engine renders from — and `setValueNotifyingHost` writes
that storage, so a packet arriving while the republish ran had its value put
back to the older one. A dump is 22 packets and a whole block of it could go
that way. The audio thread now keeps shadows of its own for the three binding
tables, the republish reads those, and a value that lands mid-publish is
restored immediately. A `patchDirty` flag delimits the window, and every
message-thread writer (`loadPatch`, `applyProgram`, `reconcileProgram`) points
the shadows back at the parameters and clears it, so last writer wins rather
than a queued republish undoing a program change.

**One foreign SysEx message split a patch in two.** `parseSyxBankFile` read the
patch boundary — the two high address bytes — off the raw message before
`decodeSysExMessage` had accepted it, so anyone else's manufacturer ID, a
universal message, an RQ1, or a DT1 with a bad checksum ended the patch being
accumulated and `loadSysExData` then loaded the half read so far. The DT1
header parse is a function of its own now (`parseDt1Packet`), and the boundary
moves only on a message that has proved it is a DT1 for this model with a good
checksum and a block this codec owns. Three of the five intruders in the new
test split a patch against the old code.

**An imported arpeggio pattern was decoded and then thrown away.** Not new with
Step 40, as this section first claimed and the commit message with it — the
correction is worth stating plainly because provenance is what this document is
for. On `origin/main` the single-block `decodeArpeggioParams` already wrote the
whole grid into `arp.style.cells`, and `snapshotPatch()` already ended with an
unconditional `applyArpeggioStyle (patch, styleIndex)`, so the grid was rebuilt
from the selector and discarded there too. The README at `6bb2292` even said so
in as many words. What Step 40 changed was the shape of the data, not its fate.
Either way `snapshotPatch()` rebuilds the style on every block, so each decoded
row was discarded by the next packet, and a pattern imported from a real unit
neither played nor survived a re-export. Two halves:

- **END STEP** now maps onto the `arp_end_step` parameter. It is one control on
  the hardware, 1–32, and the replica's panel is the same control with a zero
  added *below* the documented range meaning "as long as the loaded style is" —
  so a documented value maps straight onto it, and that is the only way it
  survives the trip through the plug-in.
- **The grid** is kept beside the parameters, published under a seqlock because
  the audio thread both writes it (a dump is decoded in the render callback)
  and reads it, and the message thread reads it to save the session. It stands
  in for the selected template while the selector stays where it was when the
  dump arrived; moving the selector picks a template, which is what the
  hardware's panel does. It is saved in the plug-in state as the sixteen Patch
  Arpeggio Pattern blocks the address map defines, base64'd — the same bytes a
  real unit would send, so there is no second format to keep right.

The test imports a grid matching none of the shipped styles, with an Original
Note per row, and checks all 512 cells reach the engine, survive a re-export,
survive a session save and restore, and give way to a template when the
selector moves. Five of its checks fail against the previous code.

That closes the last of the three things the README said a SysEx load could not
carry. Only the patch *name* is left, and it genuinely has no parameter.

**And the grid had two more doors it did not fit through.** A factory program
carries its own style, named by its selector index, and the selector is only
the key the imported grid is filed under — so a program whose style index
happened to match the one a dump arrived under played the imported grid instead
of its own template. A program change drops the imported grid now, on both the
audio-thread and message-thread paths.

**And the grid had a second door it did not fit through.** A dump arriving on
the wire is decoded in the render callback; a `.syx` handed to the plug-in
through the API goes through `loadSysExData` and `loadPatch`, which writes the
parameter list — and the grid is not in the parameter list. The first entry
point kept it and the second dropped it. `loadPatch` publishes it too now;
both of its callers are SysEx loads, so there is nowhere else for it to leak.

**And the same race once more, in the device-control path Step 42 added.** The
three Universal Realtime messages take the same audio-thread split, and their
republish read the parameter's own storage exactly as the other two did — so a
second Master Volume arriving while the first was being published was lost.
Same shadow, same restore. Three of these were found one round after another,
which is what the shape deserves: every audio-thread-to-message-thread publish
in this processor now reads a value the message thread cannot clobber.

#### Step 46 — a DT1 addresses a byte, not a block

**The document's own worked example was applied to the wrong parameter.** A
DT1's address names a byte; `<Example1>` on p. 6 writes one byte to
`10 00 04 02`, which is REVERB SIZE two bytes into Patch Reverb, and every knob
a real unit transmits has that shape. The dispatch read only the block number
out of the address and handed the payload to the block's decoder as though it
always began at offset zero — so that example set REVERB **TIME** and left SIZE
alone. Rather than teach six decoders to index from an offset, the block is
reconstituted: encode what the patch holds now, lay the received bytes over it
at their address, decode the whole thing back. The encoders and decoders are
already each other's inverse, which the round-trip tests are what fence, so a
whole-block write is unchanged and a one-byte write moves one field. A write
addressed past the end of its own block is refused rather than wrapped.

**A System Common DT1 split a bank and was read as a patch.** `decodeSysExMessage`
already refused a base outside the Temporary Patch and the 32 User slots, but
the bank reader's gate checked only the *block* number — and System Common's is
`00`, the same as Patch Common's. So it passed the gate, moved the patch
boundary, flushed the half-read patch, and only then was refused. One predicate
now answers both questions, and the intruder list in the bank test grew a
System Common DT1 that splits the patch against the old gate.

**A state restore did not supersede a queued republish.** `applyProgram` and
`loadPatch` already re-pointed the shadows and dropped the pending flag;
`setStateInformation` did not, so a dump, a CC or a device-control message whose
republish had not run yet would land on top of the session that had just been
loaded — potentially replacing the whole restored patch. All three shadow sets
are re-seeded and all three dirty flags cleared on restore.

**And the imported grid's publication was a data race, not just a detected
one.** A seqlock over one buffer lets a reader's copy overlap a writer's: it
notices the tear and retries, but the overlapping access is itself undefined.
The grid is published into a ring of four slots now, so the slot being written
is never the slot being published — a reader would have to be overtaken by a
whole lap before it even shared memory with a writer, and the published counter
is still checked afterwards so an overtaken reader retries. Writers take a slot
by atomic increment, so two of them never pick the same one.

All four are fenced, each by a check watched to fail with its fix reverted, and
the 11 committed demos still re-render bit-identically.

#### Step 47 — four more, and one bound rather than a proof

**A discarded grid came back with the next session save.** The tree handed to
`getStateInformation` is a copy of the last state, so it can still carry an
earlier restore's `arpeggio_grid` properties. The writer returned early when no
grid was live and left them there — so restore a session with a grid, load a
factory program (which discards it), save, and the grid was still in the file,
ready to override that program's own style on the next restore. The early
return removes the three properties now.

**A device-control message was a whole buffer late.** SYSTEM COMMON is read
once before the MIDI loop, and `applyCurrentPatch()` refreshes only the patch
and the external input — so a Master Volume, Master Fine Tuning or Master
Coarse Tuning arriving mid-block did not take effect until the next one. In an
offline render with a large buffer that is arbitrarily late. The system
settings are refreshed on the same event boundary as the patch now; the test
sends MASTER LEVEL 0 a quarter of the way into a 4096-sample block and requires
the note to be gone by the end of it.

**Moving the style selector did not retire the imported grid.** It chose a
template while the grid stayed valid under its own index, so returning to that
index resurrected the import instead of loading the template — which
contradicts the panel model this project wrote down for it: the manual says
editing a style needs the SH-201 Editor, so the panel only ever *selects*, and
a selection replaces what the patch held. The first move away retires the grid
for good.

**And the publication ring can still be lapped — this is a bound, not a
proof.** With four slots, a 22-packet dump publishes 22 times and can lap a
reader preempted mid-copy, which is the data race the ring was meant to remove
rather than merely detect. The ring is 32 slots now, comfortably more than a
whole dump, so no single dump can lap a reader. That is honest engineering
headroom and not a guarantee: a proof needs publication that cannot reuse a
slot while any reader may hold it, which for a real-time writer that must never
block means a reader-registration scheme or an SPSC handoff to the message
thread. Recorded here as the residual it is, rather than described as solved.

#### Step 48 — the same five, one layer down

A sixth round on the same surface, and every one of the five was real. Four are
places where a fix from an earlier step was *nearly* right.

**The bank gate and the decode disagreed about one thing.** Step 46 gave both
`isForThisInstrument()`, but the decode also refused a packet addressed past
the end of its own block and the gate did not — so a checksum-valid DT1 like
`20 01 00 7F` moved the patch boundary and flushed a half-read patch before
being refused. The offset test now lives in the predicate, which makes the two
accept exactly the same messages by construction rather than by agreement.

**A restore with an unreadable grid kept the old one.** `readImportedArpeggioFromState`
returned early on a base64 failure or a wrong size without retiring what the
processor was holding — and that grid belongs to the session that has just been
replaced, so a restored patch whose selector happened to match played it
instead of its own style. Every path out of that function now settles the
question.

**The selector was published apart from its grid.** It sat in an atomic of its
own, so a reader could observe a new selector a moment ahead of its payload and
copy the previous slot as though it belonged to it — and two concurrent writers
could leave the pair inconsistent for good. The selector rides *in* the slot
now, so the single `published` store publishes both together.

**The grid was published outside the patch transaction.** It went in before
`writePatchToParameters` made the generation odd, so a state save could copy
the old parameter values, then see and serialise the new grid, and still pass
its generation check — writing a session that pairs one patch revision's
settings with another's grid. `writePatchToParameters` takes the grid now and
publishes it inside its own odd window.

**And a program's re-notification pass was clearing a newer dump's flag.**
`reconcileProgram` writes nothing — it re-notifies values the audio thread
already stored and skips any edited since — but Step 45 had it call
`syncPatchShadows()`, which dropped the pending flag of a dump that arrived
*after* the program change. The patch reconciler then returned silently and the
host and panel sat on the program's values while the engine rendered the
dump's. The call is gone; `writeProgramToParameters` already does that work
where the change actually lands.

All five are fenced, each by a check watched to fail with its fix reverted, and
the 11 committed demos still re-render bit-identically.

#### Step 49 — the resonance knob was not monotone

Step 30 found the filter's state limiter acting as a full-time waveshaper —
**−27.3 dB THD at RESONANCE 0** on an ordinary two-oscillator patch — and
gated it on `damping <= 0.0`, the stage's own stability boundary, on the
grounds that it was a boundary rather than a new constant. The diagnosis was
right and the repair was wrong.

`mapping::resonanceDamping (v) = 2 − 2.04·√(v/127)` crosses zero at
**RESONANCE 122.07**. So the gate did not limit "past the oscillation
threshold" — it left the entire high-Q shoulder *below* the threshold running
with no bound at all: k = 0.034 (Q 30) at RESONANCE 118, k = 0.0006 (Q 1783)
at 122. Measured on a two-saw LPF −24 dB patch at CUTOFF 60, note 48, whole
file:

| RESONANCE | 0 | 110 | 118 | 120 | 121 | 122 | **123** | 127 |
|---|---|---|---|---|---|---|---|---|
| RMS | 0.073 | 0.174 | 0.381 | 0.549 | 0.648 | 0.701 | **0.090** | 0.092 |
| peak | 0.143 | 0.429 | 0.742 | 0.992 | 1.043 | 1.050 | 0.182 | 0.183 |
| % past the output limiter's knee | 0 | 0 | 0 | 3.1 | 14.1 | 27.7 | 0 | 0 |

1.050 is exactly the output stage's own ceiling. So the last few degrees of
the knob were sustained clipping rather than resonance, and one more step —
122 to 123 — was a **17.8 dB fall**, on every filter type and both slopes
(worst: BPF −24 dB, 26.4 dB).

**What was actually wrong was the knee, not the fact that the limiter ran.**
`filterStateLimit` was 1.5, and an ordinary patch puts far more than that into
the filter. Measured at RESONANCE 0 with both oscillators at unity, LEVEL and
velocity at maximum and LOW FREQ BOOST, across all eight waveforms, three
filter types, both slopes and CUTOFF 0…127, the largest integrator state an
*unresonant* filter reaches is **6.15** (NOISE at CUTOFF 127; SQU/PW-SQU 5.02,
SINE 4.82, SAW 4.70, TRI 4.41, FB-OSC 2.83, SUPER-SAW 2.59). A knee under a
third of that could only be a waveshaper.

The gate is gone and the knee is re-pinned at **8.0** — above every measured
unresonant state, with headroom — so the limiter is a bound on runaway rather
than something in the signal path. It stays `[voiced, OQ-08]`, now with the
measurement that chose it recorded beside it. The sweep is monotone at every
filter type and slope, and never reaches the output limiter: LPF −24 dB now
runs 0.073 → 0.363 RMS from RESONANCE 0 to 127 with a peak of 0.641.

The RESONANCE-0 THD check that Step 30 added still passes, which is the point:
both defects had the same cause and one knee fixes both.

**This is the only change in this branch that alters shipped audio.**
`04-acid-filter-24db.wav` sweeps RESONANCE `96 + pass·10` — 96, 106, 116,
126 — straight through the unbounded shoulder and over the cliff, so it was
audibly hitting the defect. It is re-rendered; the other ten demos are still
bit-identical.

A new test sweeps RESONANCE across 0…127 on all six type × slope combinations
and requires the level never to fall by more than a tenth between steps and the
peak never to reach the output limiter's knee. It fails 12 of 12 against the
gated limiter.

#### Step 50 — three more from the same review round

**A SUSTAIN moved under a settled note stepped the whole difference in one
sample.** Step 43 made the decay's exit test two-sided so a SUSTAIN raised under
a *still-decaying* note glides, and stopped there: `Stage::Sustain` still
assigned the new value outright, and `setPatch` reconfigures every sounding
voice on every parameter change. So an ordinary automation move on a note that
had finished decaying stepped. Measured at 44.1 kHz on a sine with the filter
bypassed: AMP DECAY 40, SUSTAIN 20 → 110 after 0.8 s jumped **0.0364** against
that take's own steady travel of 0.000196 — 186 times it; DECAY 60, 100 → 20
jumped 0.0337; DECAY 0, 0 → 127 went from digital silence to −0.053 in one
sample. Even a one-unit move stepped 7 times the steady travel. The amp envelope
is deliberately outside the control slew, so nothing downstream smooths it.

`configure` hands a converged voice back to `Stage::Decay` when the sustain it
is given differs from the level the note is holding, and the two-sided branch
does the walking. Every one of those seams is now 0.00025 or less — at or below
the signal's own travel. The threshold the Decay branch tests against is a named
constant shared by both tests now, so they cannot disagree about whether a level
and its sustain are the same number. The fix is inert on the legato note-on
path, which reconfigures without triggering: in Sustain the level *is* the
sustain, so an unchanged SUSTAIN leaves the stage alone. Step 43's own test
passes unchanged — it holds for 50 ms at DECAY 100, a 1.9 s decay, so it only
ever exercised the Decay window.

**LOW FREQ crossed at a rate that was a bare number, and for a duration that
depended on the endpoints.** The shelf's contribution was walked at
`fadeStep * 2.0` — a factor of two with no derivation, in the render path, which
the contract forbids — under a comment saying the shelf is crossed "like the
filter's TYPE". It was not: TYPE and SLOPE cross a 0…1 weight, and the shelf
crossed the *depth*, which spans −0.602 to +1.512. Measured through the shipping
engine, one switch had three transition times — FLAT → CUT in **1.52 ms**,
FLAT → BOOST in **3.76 ms**, BOOST → CUT in **5.31 ms** — and none of them was
the registered `externalSwitchFadeSeconds`, which the filter's TYPE beside it
crosses in exactly 5.00 ms.

LOW FREQ now carries a `SwitchCrossfade<3>`, one weight per position walked at
the same `fadeStep`, and the three positions' depths are computed once per tick
instead of a `std::pow` per sample. All six transitions measure the registered
time. The bare number is gone and the comment is true.

**NOISE was 5.92 dB quieter at 192 kHz than at 44.1 kHz.** The generator drew
one full-scale value per *host* sample. A white sequence spreads its power over
the whole band it is white across, so its audible density falls as that band
widens: measured in-band (≤ 16 kHz) through the shipping engine, NOISE moved
−2.57 dB at 88.2 kHz, −2.96 at 96, −5.60 at 176.4 and −5.92 at 192, against a
SAW in the same patch that moved 0.04 dB. The balance between two legs of one
patch followed the user's interface setting — the same defect the OVERDRIVE
carries `overdriveInternalRateHz` to avoid and the FB OSC loop carries
`fbOscLoopDampingReferenceRateHz` to avoid, and for the same stated reason: it
is the character of the port rather than of the instrument.

The three properties cannot all hold at once. A sequence flat to the *host*
Nyquist whose audible level does not move needs a variance proportional to the
host rate, which is unbounded — so "white to Nyquist, level-invariant, bounded"
is not a thing any generator is. What a fixed-rate instrument does is the fourth
option: its noise is flat across the audio band and has no content above its own
Nyquist at all. So the source is drawn at the instrument's rate and interpolated
up through the same two half-band stages the OVERDRIVE already climbs, by the
largest power-of-two factor that keeps the internal rate at or above the lower
of the two rates OQ-01 brackets — none at 44.1/48 kHz, 2× at 88.2/96, 4× at
176.4/192. A floor rather than a target, so it decides a factor at every host
rate without deciding OQ-01.

This needs **no gain constant**: unity-gain half-band interpolation preserves
both the variance and the audio-band density, because it only changes where the
same continuous-time signal is sampled. Measured on the generator alone, the
audio-band density is now within 0.05 dB of flat and within 0.4 dB across
44.1…192 kHz; end to end, NOISE against SAW moves 0.41 dB across those rates
instead of 5.92. The residual 0.37 dB between the 44.1 and 48 kHz families is
OQ-01's own undecidedness showing, and is present in both builds. At 44.1 and
48 kHz the factor is one and the draw is returned untouched, so those rates
render exactly what they rendered before — all eleven demos are bit-identical.

The interpolators live on the oscillator rather than on the voice, because the
ladder is a rate: two oscillators both set to NOISE cannot share one chain
without driving it at twice the host rate. The draws still come off the voice's
single generator, so at a factor of one the two are the interleaved stream they
have always been. They are cleared on `reset` and not on a note-on: the chain
has no musical continuity to preserve, but starting it from zero would cost the
first few internal samples of level, which on a note-on is an attack transient
nothing asked for.

**One existing test was resolved rather than re-thresholded.**
`testNoiseIsWhiteAtEveryHostRate` read three bands at 24 log-spaced Goertzel
bins, an estimator carrying about a decibel of its own variance against a
two-decibel threshold — it read one 44.1 kHz take as −1.35 dB at 24 bins, −0.83
at 96 and −1.93 at 512, so it was a coin flip on every build and this change
happened to flip it. It reads 256 bins now, and it tests the claim OQ-03 makes:
that the shape does not follow the host rate. The absolute bound is stated at
what is measured — the voice path rolls off about 1.9 dB from 100 Hz to 16 kHz
at *every* rate and on both builds, so 2 dB was never a bound the whole chain
met. A second test fences the level claim directly: NOISE against a SAW in the
same patch, which fails at all four rates above 48 kHz on the previous
generator.

All three fixes are fenced by checks watched to fail with them reverted — 14 of
them. The 11 committed demos still re-render bit-identically.

#### What this pass did not do

Recorded here so the next reader knows they were considered and left:

- **Host-transport and MIDI-clock sync.** CLOCK SOURCE is a settled System
  Common parameter with an external setting, and a plug-in's external clock is
  its host — so following it would be a reading of a documented option rather
  than an invention. It is a feature addition rather than a fidelity fix, and
  it is the largest thing still missing for anyone using the arpeggiator in a
  session.
- **Whether SUPER SAW and FB OSC should respond to hard sync**, whether the
  AMP envelope belongs ahead of the overdrive or behind it, and what the top
  of the PW range should do when the narrow side of the pulse is shorter than
  a sample. All three are audible, all three have more than one defensible
  reading, and none is a measurement question — they are A–Z candidates, and
  A is the shipping engine in each.
- **The patch name through a SysEx load.** The block codec carries it; the
  plug-in does not, because its authoritative state is its parameter list and
  a twelve-character name has no parameter. Stated in the README rather than
  papered over. (The arpeggio grid was in this note until Step 45, which gave
  it somewhere to live.)
- **Transmitting SysEx, and answering RQ1.** The map is read now and the
  encoder writes every block at its documented address, so a plug-in that
  dumps itself to a real unit is a small step from here. It is a feature
  rather than a fidelity fix, and nothing in the plug-in currently has a
  reason to send.
