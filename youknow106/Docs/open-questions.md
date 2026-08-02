# YouKnow106 — open circuit questions

Every constant in the model is one of three things:

- **anchored** — read from service documentation, a datasheet or a measurement;
- **derived** — computed from anchored values;
- **voiced** — chosen inside a range the located sources bound but do not fix.

This file lists every constant still in the third category, one per section,
each written as a self-contained prompt. They are ordered by how much the answer
would change the sound.

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

**Why it matters:** this sets how bright the wet path is. The two located
accounts differ by roughly 20 dB at 15 kHz, which is the difference between a
chorus that shimmers and one that is soft.

> **Query**
>
> I need the filter topology and component values on the Roland Juno-106's
> chorus board — the board carrying two MN3009 bucket-brigade delay lines and an
> MN3101 clock driver.
>
> There is a filter before each delay line and another after it. Two published
> accounts contradict each other: a fifth-order model puts the input filter at
> 9.9 kHz and the output at 9.5 kHz, while a direct measurement of a sibling
> instrument's wet path fits a *single* pole at 14 kHz across the whole audio
> band. A fifth-order pair at 9.5 kHz would be about 20 dB darker at 15 kHz than
> the measurement permits, so these cannot both describe the same circuit.
>
> Find the actual schematic. I want: how many poles before the line, how many
> after, the topology of each (Sallen-Key, multiple-feedback, passive RC, …),
> and the resistor and capacitor values, with reference designators.
>
> Best sources: the Roland Juno-106 Service Notes chorus board schematic and its
> parts list; Juno-chorus clone or Eurorack-adaptation projects that publish a
> full bill of materials with values. Already tried without success: general
> searches for "Juno-106 chorus schematic filter values", MN3009 application
> notes, and clone build documentation that reproduces the board without stating
> component values.

**Required output**

```
Q1 CHORUS SUPPORT FILTERS — FOUND / NOT FOUND
Source:      <document, page, revision>
Pre-BBD:     <topology>, R___ = ___, R___ = ___, C___ = ___, C___ = ___
Post-BBD:    <topology>, R___ = ___, R___ = ___, C___ = ___, C___ = ___
Confidence:  high / medium / low — and why
Caveat:      <revision differences, illegible values, sibling instrument, …>
```

---

## 2. High-pass filter network

**Why it matters:** two self-consistent accounts disagree, and one is already
asserted by the test suite. Whichever is right, the other is wrong by ~10 Hz on
the middle corner and ~47 Hz on the top one.

> **Query**
>
> I need the resistor and capacitor values for all four positions of the Roland
> Juno-106's high-pass filter slide switch. It is a four-position switched RC
> network ahead of the voice filter, and it lives on the chorus board, so a
> chorus-board schematic may answer this and the chorus filter question at once.
>
> Position 0 is a bass boost, position 1 passes the signal untouched, positions
> 2 and 3 are single-pole high-passes.
>
> Sources agree the top corner is near 720 Hz but give both 225 Hz and 240 Hz
> for the middle one, and both "+3 dB at 70 Hz" and "+10 dB at 150 Hz" for the
> boost. I have two accounts that are each internally consistent but mutually
> exclusive: one implies an effective 44.9 kΩ working against 15 nF and 4.7 nF,
> the other implies 15 kΩ working against 47 nF and 15 nF. Note that both use
> values from the same E6 series, so this may be one schematic transcribed twice
> with the values shifted between positions.
>
> I want the raw component values per switch position, with reference
> designators, plus whatever sets the boost — a shelf usually needs a resistor
> in parallel with the capacitor, or a second RC leg.

**Required output**

```
Q2 HIGH-PASS NETWORK — FOUND / NOT FOUND
Source:       <document, page, revision>
Position 0:   R___ = ___, C___ = ___   (+ whatever else forms the shelf)
Position 1:   <what the switch connects>
Position 2:   R___ = ___, C___ = ___
Position 3:   R___ = ___, C___ = ___
Series/shunt: which element is in series with the signal
Confidence:   high / medium / low — and why
```

---

## 3. Chorus modulation oscillator, and the I+II rate

**Why it matters:** the model computes the I+II rate from an assumed switching
topology. The direction is certain — both buttons down is faster than either
alone — but the number is not.

> **Query**
>
> The Roland Juno-106's chorus has two buttons. Pressing both at once is a
> documented third setting, distinct from either alone. Modes I and II are
> documented at 0.513 Hz and 0.863 Hz with the same delay sweep of 1.66–5.35 ms.
>
> I want either of these:
>
> (a) the schematic of the modulation oscillator (the low-frequency triangle
>     generator that sweeps the delay lines) and specifically how each of the two
>     chorus switches changes its timing network — which resistor each switch
>     adds or removes, and its value; or
>
> (b) a measured rate for the both-buttons-down setting, in Hz. This can be read
>     off any recording of a Juno-106 with both chorus buttons engaged by
>     counting the sweep period; a YouTube demo is a perfectly good source if
>     you can state the timing.
>
> I currently assume each button switches its own resistor into the timing
> network, so closing both places them in parallel, the conductances add and the
> rate becomes 1.376 Hz. I want that confirmed or replaced.

**Required output**

```
Q3 MODULATION OSCILLATOR — FOUND / NOT FOUND
Source:        <document/page, or recording URL + timestamps>
Topology:      <how each switch changes the timing network, with values>
Rate mode I:   ___ Hz      Rate mode II: ___ Hz      Rate I+II: ___ Hz
Method:        read from schematic / counted from recording / stated by source
Confidence:    high / medium / low — and why
```

---

## 4. Chorus noise floor and wet gain

**Why it matters:** the noise floor is the effect's signature — the circuit has
no compander, which is exactly why it hisses. The model's figure sits inside a
10 dB band, and the band itself has no citation.

> **Query**
>
> Two figures for the Roland Juno-106's chorus:
>
> (a) The in-circuit signal-to-noise ratio of the chorus path. The circuit has
>     no compander (no NE570/571 or equivalent), which is why the effect is
>     known for hiss. Reported figures scatter across roughly 55–65 dB, against
>     the MN3009's own 88 dB datasheet number. I want a measured figure for the
>     Juno-106 specifically, with the measurement conditions: bandwidth,
>     weighting, chorus on or off, and what the reference level was.
>
> (b) The gain of the wet path relative to dry, in dB, as set by the summing
>     resistors into the final op-amp. I want the two resistor values.
>
> A measurement of your own is acceptable for (a) if you state the method. Note
> that a Juno-106 with failing voice chips or leaking capacitors will measure
> much worse than the design — I want the design's behaviour, not a fault's.

**Required output**

```
Q4 CHORUS NOISE AND WET GAIN — FOUND / NOT FOUND
S/N:          ___ dB,  bandwidth ___,  weighting ___,  reference level ___
Source:       <document/page or measurement description>
Wet:dry:      R_wet = ___, R_dry = ___  ->  ___ dB
Confidence:   high / medium / low — and why
```

---

## 5. This instrument's own chorus sweep and rate

**Why it matters:** the delay range and the rate decimals currently come from a
*sibling* instrument's calibrated capture. The Juno-106's own published figures
agree to the precision they are quoted at, but the decimals are borrowed.

> **Query**
>
> I want a measurement of a Roland Juno-106's own chorus — not a Juno-6,
> Juno-60, MKS-7, MKS-30, HS-60 or Alpha Juno.
>
> Per mode (I, II, and both buttons together):
>
> - the modulation rate in Hz;
> - the delay sweep in milliseconds, as a minimum and maximum.
>
> The figures I am using — 1.66 to 5.35 ms of sweep, 0.513 Hz and 0.863 Hz —
> come from a sibling's calibrated capture. The Juno-106's own published figures
> are quoted only as "about 0.5 Hz" and "about 0.8 Hz".
>
> Both quantities are measurable from a recording: the rate by counting the
> sweep period, the delay range by tracking the pitch deviation of a sustained
> tone, since a swept delay's pitch shift is the derivative of its delay.

**Required output**

```
Q5 JUNO-106 CHORUS MEASUREMENT — FOUND / NOT FOUND
Source:       <document/page, or recording URL + timestamps, or your own capture>
Mode I:       rate ___ Hz,  sweep ___ to ___ ms
Mode II:      rate ___ Hz,  sweep ___ to ___ ms
Mode I+II:    rate ___ Hz,  sweep ___ to ___ ms
Instrument:   confirmed Juno-106 / uncertain / a sibling (which)
Confidence:   high / medium / low — and why
```

---

## 6. Per-voice component tolerance

**Why it matters:** this is what makes six voices sound like six voices rather
than one voice played six times. One of the five mechanisms is anchored; the
other four are voiced.

> **Query**
>
> The Roland Juno-106 service documentation gives roughly ±5% cutoff variance
> per voice, and the service procedure provides per-voice filter trimmers but no
> per-voice oscillator trimmer.
>
> I want the spread of anything *other* than cutoff across the six voice cards:
> resonance or regeneration-amplifier gain, comparator offset, the ramp
> capacitor's charging current, output amplifier offset or gain, per-voice level
> control voltage, or the high-pass network's own part tolerances.
>
> Useful forms of answer, in descending order: a tolerance stated in the service
> notes or the parts list; the tolerance class of the components used (a 1%
> metal-film part and a 10% ceramic imply very different spreads); or a
> measurement across the six cards of an actual unit.
>
> Note that the envelopes are generated digitally by a single processor and are
> therefore identical across voices — I do not need envelope-rate spread, and an
> answer claiming one is describing something else.

**Required output**

```
Q6 VOICE-CARD TOLERANCE — FOUND / NOT FOUND
Source:        <document, page — or measurement method>
Mechanism:     <which parameter>
Spread:        ±___%  or  ±___ <unit>
Basis:         stated tolerance / component class / measured across ___ voices
Confidence:    high / medium / low — and why
(repeat the block per mechanism found)
```

---

## 7. Two smaller voiced constants

Lower value than the above, but cheap to settle from the same schematic.

> **Query**
>
> Two component-level questions about the Roland Juno-106, both answerable from
> the service notes schematics:
>
> (a) **Noise generator mix weight.** One shared noise source is mixed into
>     every voice through a per-voice level control voltage. The service
>     procedure has a module-board noise-level adjustment, but I have not found
>     what it is adjusted *to*. I want the target: an amplitude in volts
>     peak-to-peak, or the test-point voltage the procedure specifies.
>
> (b) **Chorus input divider.** The MN3009 is rated for 0.3% distortion at
>     0.78 Vrms, and the delay line is the first thing in the wet path to
>     overload. I want the resistor divider between the voice bus and the delay
>     line's input — the two resistor values — which is what sets how hard the
>     line is driven and therefore at what level the wet path starts to grit.

**Required output**

```
Q7a NOISE LEVEL — FOUND / NOT FOUND
Source:     <document, page>
Target:     ___ Vpp  (or: test point ___ = ___ V)

Q7b CHORUS INPUT DIVIDER — FOUND / NOT FOUND
Source:     <document, page>
Values:     R___ = ___, R___ = ___  ->  divider ratio ___
Confidence: high / medium / low — and why
```

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
- **Chorus I+II exists as a distinct setting.** The two buttons are independent
  latching switches. That the patch memory can only *store* three states is a
  limit of the memory, not of the panel.
- The cutoff control law, the resonance loop topology, the firmware envelope
  shape, and the output amplifier's quasi-linear response.
- The note timer, counter width, control scan period and hold slew, pulse duty
  anchors, sub-oscillator division, voice assignment and unison behaviour.

---

## Why component values, not corner frequencies

The last round of research returned a post-BBD filter as "15 kΩ, 15 kΩ, 1 nF,
2.2 nF, giving approximately 5000 Hz". Those parts in a Sallen-Key lowpass give
**7153 Hz**, not 5000. Substituting 4.7 nF for the 2.2 nF gives 4894 Hz, which
matches the stated corner — and 2.2 nF and 4.7 nF are adjacent E6 values, so
this looks like a single-digit transcription slip.

The finding may well be correct. But it cannot be adopted, because adopting it
means guessing which of the two numbers is the typo. Had the answer arrived as a
corner frequency alone, the error would have been invisible and would now be in
the model.

Send the parts. The arithmetic gets done here, and doing it here is the check.
