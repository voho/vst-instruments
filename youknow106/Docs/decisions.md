# YouKnow106 — decision log

Directions chosen by ear, recorded per the A–Z listening-test convention in
the repository's `CLAUDE.md`. A choice made by ear is recorded as made by ear,
never written up as though a measurement had settled it, and none of these
closes an open question — the captures named under
[known gaps](../README.md#known-gaps) are still what would.

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

- **Resonance shape (OQ-09).** An A/B was delivered 2026-08-20 and
  re-verified byte-identical on the current engine 2026-08-21: A is the
  shipping voiced curve, B the `CircuitDerivedResonanceProfile`. Selecting B
  would make the circuit-derived shape the default. No verdict yet; either
  way it is recorded here as chosen by ear.
- **Vref = 0.775 V (OQ-06).** Roland's era convention, recorded as the
  standing candidate. Adoption is a product decision, not a listening
  question.
- **Chart-geometry follow-on (OQ-08).** If `MeasuredChartGeometry` and the
  normalised profile prove indistinguishable by ear, OQ-08's audible-impact
  priority drops. No verdict yet.
