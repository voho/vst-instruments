# YouKnow106 — decision log

Model directions and listening verdicts. A choice made by ear is recorded as
made by ear, never written up as though a measurement had settled it, and none
of these closes an open question — the captures named under
[known gaps](../README.md#known-gaps) are still what would.

## 2026-08-29 — C14 voltage coefficient withdrawn from the default

The optional C14 capacitance modulation remains available to the isolated
comparison renderer but no longer ships enabled. Its `0.15` coefficient had no
installed-part measurement, and the implementation drove it from the complete
bus voltage rather than the voltage across C14. More importantly, current
[aluminum-electrolytic manufacturer guidance](https://www.chemi-con.co.jp/en/faq/detail.php?id=alBiasVoltageChara)
states that voltage bias does not change this capacitor class's capacitance.
That general guidance is not a measurement of Roland's 1980s 10 uF non-polar
part under AC, so it does not prove the real distortion is zero; it does make
removing the guessed law the evidence-conservative default. OQ-21 remains open
for a level-swept transfer/THD or direct voltage-across-C14 capacitance
measurement, plus a switching capture. The internal comparison flag is not
serialized, so this correction also changes restored sessions at nonzero Unit
Character; preserving an unsupported default would be the less faithful
compatibility choice.

## 2026-08-28 — Fidelity-first quality default

New instances now request the deepest 4× QUALITY rung. The DCO passes the
project's numerical gates at every rung, but the BBD and VCF domains pass their
absolute gates only at 4× for common host rates; keeping 1× as the default was
therefore a CPU-first product choice at odds with the fidelity goal. Existing
sessions retain their stored choice, 1× and 2× remain available, and the
request is still capped against the host rate, so sufficiently high-rate hosts
do no redundant work.

## 2026-08-28 — Resonance control law

The circuit-derived linear-above-junction profile is now the default. The
module drawing traces the resonance CV through a grounded-base stage directly
into the BA662 control input, whose transconductance is linear in control
current; that is stronger physical evidence than the legacy quadratic-then-
linear compatibility voicing. Both retain the same service-calibrated maximum,
input compensation and self-oscillation correction, so this changes only the
intermediate slider response and adds no DSP work.

This is an evidence-priority decision under the realism/CPU goal, not a
listening verdict and not a closure of OQ-09: the 0.6 V junction onset and
0.2296 compensation coefficient still await a measured response family. The
legacy voiced curve remains available behind the internal comparison switch.

## 2026-08-28 — MN3009 transfer and default floor

The chorus write transfer now distinguishes the MN3009's guaranteed input-swing
limit from its typical distortion curve. It retains the existing 2.924 V rail,
fits 0.3% THD at 0.78 Vrms and approximately 2% at 2.0 Vrms, and remains below
the 2.5% guarantee at 1.5 Vrms. The prepared curve and slope changed, but the
realtime path is still the same 512-interval Hermite lookup with the same cost.

The datasheet specifies 0.2 mVrms A-weighted noise as a maximum and 88 dB S/N
as typical, not a typical noise voltage. HISS 100% therefore remains the maximum
endpoint, while new instances and factory controls default to 29.858%, the
59.716 uVrms inference from 1.5 Vrms / 88 dB. This makes the uncertainty visible
without changing saved values or tying noise to Unit Character. OQ-03 remains
open for an identified, calibrated instrument's absolute PSD.

A native Release benchmark against the pre-change tree measured the worst 1x
scenario at +0.4% CPU (the others ranged from -0.04% to +0.2%), comfortably
inside the goal's +20% ceiling.

## 2026-08-23 — VCF solver ladder

`VCF Solver` descends Max / High / Normal — Merson ×2, RK4 ×2, RK4 ×1, so
10/8/4 right-hand-side evaluations — every rung on the same seven control
nodes, so no rung moves a control node or a hold trajectory. The default
rung's error against an independent 96-substep reference is −162.551 dB
(4.2e-8 V peak).

**Verdict, by ear:** a blind four-letter set returned no audible difference
between rungs, so instances ship on **Normal**, roughly half the filter's CPU.
`EngineParameters` stays on Merson as the reference kernel that every frozen
fingerprint tests.

## 2026-08-24 — CPU defaults

The `Poly` tanh kernel and `Cubic` Fast Early form, plus the freewheel wake
that lets silent voice cards and a settled, switched-off chorus skip work.
Numerical evidence: a six-voice chord nulls at −89 dB against `Exact`;
self-oscillation anchors are identical to six decimals in amplitude and
0.003 cents in pitch. Sample nulls on resonant material decorrelate by phase
drift and are judged by anchor, as the earlier solver pass established.

The default flip and the freewheel wake are an audible-impact question the
measurements cannot close, so a four-take A/B set was rendered — A the
shipping configuration, B the new defaults; retrigger-after-silence, resonant
lead, self-oscillation, chorus engage; RMS-matched within 0.001 dB, no trims —
with its `key.md` unread by design.

**Verdict, by ear:** the player could not tell A from B on any take, so the
new defaults stand. Chosen by ear, not settled by a measurement.
`VCF Tanh = Exact` remains the one-menu revert.

## Pending

- **Vref = 0.775 V (OQ-06).** Roland's era convention, recorded as the
  standing candidate. Adoption is a product decision, not a listening
  question.
- **Chart-geometry follow-on (OQ-08).** If `MeasuredChartGeometry` and the
  normalised profile prove indistinguishable by ear, OQ-08's audible-impact
  priority drops. No verdict yet.
