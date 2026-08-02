# YouKnow106 — open circuit questions

Every constant in the model is one of three things:

- **anchored** — read from service documentation, a datasheet or a measurement;
- **derived** — computed from anchored values;
- **voiced** — chosen inside a range the located sources bound but do not fix.

This file lists every constant still in the third category, one per section,
each written as a self-contained prompt.

**One question remains.** Two research rounds have settled the rest; what they
answered is listed at the end so nobody spends time re-finding it. The one left
is the one both rounds got wrong, and it is also the most valuable.

## How to use this file

Each section has a **Query** — paste it whole into a research model; it carries
its own context and needs nothing from this repository — and a **Required
output**, which is the shape an answer has to arrive in to be usable.

Three rules apply to every question here:

1. **A source, or nothing.** Every number needs its provenance: a service manual
   page, a schematic reference designator, a datasheet, a named measurement, a
   specific forum post. "Typical for this kind of circuit" answers nothing.
2. **"Not found" is a real answer** and is better than an estimate. A voiced
   constant is at least labelled as uncertain; a wrong one presented as anchored
   is not. Several of these have survived multiple search passes, and confirming
   a dead end saves the next attempt.
3. **Component values beat corner frequencies.** Send `R47 = 15 kΩ, C12 = 1 nF`
   rather than `about 10 kHz`. The arithmetic is done here, and doing it here is
   how transcription errors get caught — see the note at the end, which is not
   hypothetical.

The Juno-6, Juno-60, MKS-7, MKS-30, HS-60 and the Alpha Junos share parts and
whole circuit blocks with the Juno-106 but are not the same instrument. A figure
from a sibling is still useful; it just has to say which sibling. And much of
the online material is repair discussion: a degraded voice chip or a leaking
capacitor is not the instrument's design.

---

## 1. Chorus support filters — highest value

**Why it matters:** this sets how bright the wet path is, and it is the last
voiced constant in the chorus.

**Two rounds have now answered this wrongly, in the same way.** Both returned
component values that do not produce the corners they were returned with:

| Round | Stated parts | Stated corner | What the parts give |
| --- | --- | --- | --- |
| 1 | post-BBD 15k/15k/1n/2.2n | ~5,000 Hz | 7,153 Hz |
| 2 | pre-BBD 33k/12k/2.2n/1.0n | 10,220 Hz | 5,392 Hz |
| 2 | post-BBD 39k/12k/47n/82n | 9,480 Hz | **118 Hz** |

The second round also gave `R115 = 33 kΩ` and `R113 = 12 kΩ` two incompatible
roles in the same document — as the two series resistors of a Sallen-Key stage,
and as a series/shunt divider to ground. Only the divider reading computes, and
that part *was* adopted.

So the corner is now better supported than it was — two independent accounts put
it near 9.5–10 kHz, and the second explains the conflicting 14 kHz measurement
as test-gear loading — but no source has yet produced values that compute.

> **Query**
>
> I need the filter topology and component values on the Roland Juno-106's
> chorus board — the board carrying two MN3009 bucket-brigade delay lines and an
> MN3101 clock driver. There is a filter before each delay line and another
> after it.
>
> **Please verify your own arithmetic before answering.** For a Sallen-Key
> low-pass, f = 1 / (2π·√(R1·R2·C1·C2)); for a single RC pole, f = 1 / (2π·R·C).
> Compute the corner from the values you are about to send, and if it does not
> match the corner your source states, say so explicitly rather than sending
> both. Two previous answers failed exactly this check — one by a factor of 80 —
> and were therefore unusable. An answer of "I found values but they do not
> produce the stated corner, here are both" is genuinely useful. An answer that
> silently reconciles them is not.
>
> I want: how many poles before the line, how many after, the topology of each
> (Sallen-Key, multiple-feedback, passive RC, …), and the resistor and capacitor
> values with reference designators.
>
> Context for cross-checking: the surrounding op-amps are M5218 dual low-noise
> parts; the input divider ahead of the line is 33 kΩ series against 12 kΩ to
> ground; the published corner estimates are 9.9 kHz in and 9.5 kHz out.
> Reference designators near R113–R121 and C50–C69 appear to be in the right
> region of the board.
>
> Best sources: the Roland Juno-106 Service Notes chorus board schematic and its
> parts list; Juno-chorus clone or Eurorack-adaptation projects publishing a full
> bill of materials with values. Already tried without success: general searches
> for "Juno-106 chorus schematic filter values", MN3009 application notes, and
> clone build documentation that reproduces the board without stating values.

**Required output**

```
Q1 CHORUS SUPPORT FILTERS — FOUND / NOT FOUND
Source:        <document, page, revision>
Pre-BBD:       <topology>, R___ = ___, R___ = ___, C___ = ___, C___ = ___
  computed fc: ___ Hz          source states: ___ Hz     agree? YES / NO
Post-BBD:      <topology>, R___ = ___, R___ = ___, C___ = ___, C___ = ___
  computed fc: ___ Hz          source states: ___ Hz     agree? YES / NO
Confidence:    high / medium / low — and why
Caveat:        <revision differences, illegible values, sibling instrument, …>
```

**Failing that**, this can be settled without a schematic at all: a swept
frequency measurement of one unit's wet path with the chorus engaged, dry muted
if possible. Magnitude in dB from 100 Hz to 20 kHz is enough to fit both the
corner and the order.

---

## Already settled — please do not spend time here

- **Whether the chorus carries a dry path.** It does. Dry reaches the final
  op-amp through a series resistor and is always present; losing it is a
  documented *fault* of the mute transistors.
- **Bucket-brigade charge transfer.** Settled from the MN3009 datasheet: −3 dB
  at 12 kHz on a 40 kHz clock, which puts the half-power point at 0.3 of the
  clock rate.
- **Ramp generator curvature.** The ramp is straight — a constant-current
  integrator charge, not a resistive one. This is also the only shape consistent
  with the comparator's 6 V / 50% duty anchor.
- **Chorus I+II does not exist on the instrument.** The manual states that I and
  II cannot be used together, and the board carries one enable bit plus one
  binary I/II bit. The plug-in keeps the mode as a deliberate addition, clearly
  labelled as one; there is nothing further to research here.
- The cutoff control law, the resonance loop topology, the firmware envelope
  shape, and the output amplifier's quasi-linear response.
- The note timer, counter width, control scan period and hold slew, pulse duty
  anchors, sub-oscillator division, voice assignment and unison behaviour.

Settled by the second research round, all of it arithmetically verified here
before adoption:

- **High-pass network.** Each cutting leg's own series capacitor — 47 nF and
  15 nF — against one shared 15 kΩ shunt, switched by a CMOS multiplexer, giving
  225.8 Hz and 707.4 Hz. This corrected an earlier 44.9 kΩ / 15 nF / 4.7 nF
  reading: the same three E6 capacitors, read one switch position out of step.
- **Wet/dry balance.** 47 kΩ and 39 kΩ into a shared 100 kΩ feedback, so the wet
  path sits 1.62 dB above the dry. The feedback value cancels; only the
  imbalance reaches the model.
- **Chorus delay sweep.** 1.54 ms to 5.15 ms, the same in every mode. Both ends
  imply an MN3009 clock (83.1 kHz and 24.9 kHz) inside the part's rated
  10–200 kHz window, which is the check that the capture describes this circuit.
- **Chorus noise floor.** 60 dB unweighted, 20 Hz–20 kHz, referred to 0 dBu.
- **Chorus rate ratio.** 1.623, from the timing network: mode II leaves a
  2.2 MΩ resistor in series that mode I bypasses. The absolute rates are *not*
  anchored — the timing capacitor is illegible in the schematic — so the model
  still stands in a Juno-60's measured 0.513/0.863 Hz, whose ratio of 1.682
  agrees to 3.6%.
- **Per-voice tolerances.** Resonance ±5%, ramp integrator capacitor ±5% (a 5%
  polyester film part with no per-voice trimmer), comparator ±2 duty points from
  the 48–52% alignment window.
- **Noise generator level.** 4.0 Vpp at TP8 via VR32 — which confirmed the value
  that had been voiced.

---

## Why component values, not corner frequencies

Every number in this project is recomputed from its parts before it is adopted.
That check has now caught three bad answers across two rounds, one of them off
by a factor of eighty — and it also *confirmed* six good ones, which is why the
rejected findings did not poison the rest.

Had any of those answers arrived as a corner frequency alone, the error would
have been invisible and would now be in the model. Had they arrived as parts
with no stated corner, they would have been adopted silently and wrongly.

Send both: the parts, and the corner the source claims for them. Disagreement
between the two is information, not a problem to tidy up before answering.
