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

The window is 1500 × 784, up from 1284 × 716: the arpeggiator and the
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

| Switch | thrown | crossed | steady |
|---|---|---|---|
| CENTER CANCEL | 0.211 | — | 0.042 |
| FILTER ON | 0.304 | — | 0.042 |
| SLOPE | 0.220 | — | 0.031 |
| TYPE | 0.292 | — | 0.051 |

Each of the four is under four times its own steady figure once crossed, where
thrown it was five to seven times it.

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
