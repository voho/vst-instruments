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

The AMP overdrive is evaluated inside a fixed 176.4–192 kHz band whatever the
host runs at: 4× oversampling at 44.1/48 kHz, 2× at 88.2/96 kHz, none above,
through two equiripple half-band polyphase stages, with `tanh` under
first-order antiderivative anti-aliasing inside the loop. The transfer curve
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
