# YouKnow106 — open questions as LLM tasks

This is the canonical queue for every material hardware, firmware or
model-calibration fact that the current implementation does not yet fix. Each
item is written as a standalone task
that can be handed to an LLM with browsing access, a researcher with a
JUNO-106, or an LLM analysing supplied measurements.

Every current model value is classified as:

- **anchored** — supported by JUNO-106 service documentation, a component
  datasheet, provenance-safe firmware analysis, or a calibrated measurement;
- **ROM-resolved** — exact for one explicitly hash-identified supplied firmware
  image, without implying that every firmware revision is identical;
- **derived** — calculated from anchored values by a stated equation;
- **product policy** — an explicit plug-in decision rather than a claim about
  the analogue instrument; or
- **voiced** — provisional because the available evidence does not fix it.

JUNO-60 evidence is useful for comparison only. It must always remain labelled
JUNO-60 and cannot resolve a JUNO-106 task.

## LLM execution contract

For every task below:

1. Declare the work mode: **evidence search**, **measurement-protocol design**,
   or **analysis of supplied evidence**. Do not imply that a hardware
   measurement was performed when no raw capture was supplied.
2. Prefer an identified, healthy, warmed-up and service-calibrated JUNO-106.
   Record serial number, board/revision, mains variant, repairs or substitutions,
   calibration state, warm-up time, test equipment and loading where known.
3. Cite exact manual pages, schematic designators, test points, datasheet
   conditions, firmware-analysis locations, or capture filenames. Separate
   direct evidence from calculations and inference.
4. Preserve raw evidence: CSV for sweeps, WAV or FLAC for audio, scope exports or
   photographs for waveforms, and a netlist plus component values for circuit
   analysis. State units, bandwidth, weighting, probe/loading, sample rate,
   uncertainty and calculation steps.
5. Report contradictions instead of silently reconciling them. **Not found** or
   **measurement required** is a valid result; an unsupported estimate is not.
6. End with: status (**resolved**, **partially resolved**, **not found**, or
   **protocol only**), confidence, remaining gaps, the current assumption being
   tested, the proposed replacement if justified, and deterministic regression
   fixtures. Do not change source code unless separately requested.
7. Do not reproduce or embed ROM images or proprietary table dumps. Report
   observable 0–127 behaviour, equations, rounding and boundary semantics, with
   provenance-safe test vectors.

A measurement from one instrument establishes that unit under the stated
conditions, not an exact population value. Promote it to a nominal model
constant only with an explicit unit/population scope and tolerance rationale.

The priorities describe likely audible impact, not permission to weaken the
evidence standard.

## What is still open — 2026-08-06

The implementation and evidence boundary were reconciled again after the
2026-08-04 fidelity pass and the 2026-08-05 physical circuit modeling pass
(incorporating thermal warmup $V_t(T)$, a provisional VCA-feedthrough heuristic
later removed after the pin-9/pin-11 topology audit, power supply
rail droop inter-voice coupling, Thévenin passive mixer node loading, and
BBD residual charge-transfer loss), and again after the 2026-08-05 transcription of
Service Notes pp. 15–16, which closed the chorus modulation oscillator's
topology, waveform and mode-rate ratio and showed by derivation that mains
ripple on the regulated rails is inaudible. The table below is the
current research queue: every row names only the part that is still unknown. “Already settled” is a guardrail,
not work to repeat. None of these gaps can be closed by choosing a nicer-sounding
constant or by another source-code review; they require raw hardware evidence,
a defensible circuit analysis with missing device parameters, or comparison
against another identified firmware revision. The queue contains 20 independent
research questions plus the dependent output-reference calibration in OQ-06.

OQ-06 is a dependent calibration task rather than an open product-design
decision: the -18 dBFS RMS convention is settled, while its physical
`Vref_rms` value must come from OQ-05/OQ-17 evidence. OQ-12 through OQ-14 are
also deliberately narrow—their B-2 digital algorithms are resolved, leaving
only physical timing/transfer and revision scope. OQ-21 is new: the latest
schematic pass established C14's asymptotic loads but exposed the still-
unmodelled interaction and switching memory of the complete HPF network.

| Priority | OQ | Question still unanswered | Already settled / do not redo |
|---|---|---|---|
| P0 | 01 | Hardware confirmation of the derived 0.5533/0.8983 Hz rates (TP4 capture), a *calibrated* original-unit sweep capture, and whether the clock is period- or frequency-linear in its CV | Two-line topology, mode controls, the integrator-plus-comparator LFO and its straight triangle, the 1.6234799 mode-rate ratio, the summing-node β = 33/47 and C3 = 0.1 µF (netlist-corroborated), the derived rate scale, and the shipped 1.4–6.4 ms sweep — third-party-scoped on a designator-faithful p. 15 build with genuine MN3009s against a real 106 (2026-08-07), superseding the sibling JUNO-60 capture |
| P2 | 02 | Installed-unit common-VCA endpoint, component/rail/IC variation and residual error against the nominal law | The complete nominal path: `d=b<<5`, ideal R-2R `/4096`, p. 8's +4 to −6 V span, p. 15 R30/C7/R32/R31/R165 network, NEC's −5.9 mV/dB typical law, and C7's 9.08249 ms constant |
| P0 | 03 | Calibrated chorus noise PSD, SNR, spurs and stereo correlation (a capture with a reference tone; the −47.97/−44.01 dBFS floors have a declared but uncalibrated chain) | No-compander topology, the need for a wet-line noise model, the sourced heduhl floors and the structural 3.95 dB I→II delta on both chip populations (provenance corrected 2026-08-07) |
| P2 | 04 | Loaded post-BBD support transfer (tap-pole candidates 11.9/15.1/22.2 kHz vs the shipped ideal-source 23.46 kHz), the Gi–fi fixture reading (tracked vs broadband — one tracked ~40 kHz sweep decides, and with it the typical-part residual coefficient inside its recorded [−4.355, −1.33] dB span at 40 k/12 k), and emitter-follower source loading | Component topology with the 106's own p. 15 capacitor codes read at designator level; the two-phase OUT1/OUT2 composite solved as a full-period hold, confirming the shipped shift/hold/polyBLEP structure (2026-08-07); the fixed per-shift residual coefficient anchored to the guaranteed-minimum 40 kHz/12 kHz row, inside every candidate reading's band; Rs ≈ 3.70 kΩ summed-output source impedance and ≈ +1.10 dB intrinsic gain derived from the Gi–RL panel; the digitised 10/40/100 kHz typical family with its self-contradiction between panels recorded |
| P0 | 05 | TA75558S IC6 and High-output clipping swing versus frequency and load | IC6 identity, linear resistor gains and ±15 V rails |
| Dependency | 06 | Physical `Vref_rms` for a declared High-output/load condition | Final -18 dBFS RMS mapping, floating output and no-limiter policy |
| P1 | 07 | Acquisition/tracking behaviour, droop and loading of every converter hold | Hold ownership, 4.2 ms pass, VCF 522 µs and voice-VCA 687 µs anchors, and the designator-complete inventory: all 23 holds are 0.01 µF (p. 13 ".01×7/.01×8"), with per-destination post-hold smoothing networks read in full (2026-08-07); the PWM (R117/C62 then R116/C63) and SUB (R11/C1) networks ship as derived slews |
| P1 | 08 | Exact intra-pass timestamps/branches and the physical state forced by a changed-pitch write | Ordinal 23-write queue and normalized compatibility scheduler |
| P1 | 09 | Resonance DAC/control voltage to loop gain, compensation and oscillation correction | BA662/IR3109 topology, 4.8 Vpp service trim, shared hold, exact B-2 byte-to-DAC mapping, and the netlist-verified compensation mechanism (lineage divider values recorded, unpromoted) |
| P3 | 10 | Post-calibration six-card and multi-unit residual distributions and thermal drift | Zero-spread nominal policy and optional deterministic Unit Character |
| P1 | 11 | Pulse-Off DC, bleed, loading and switching transient at the voice mixer | About -0.8 V pins the comparator; the final output capacitors are unrelated |
| P2 | 12 | Envelope wall-clock timing/jitter, analogue/audible thresholds and other firmware revisions | Exact hash-scoped B-2 recurrence and physical `E>>2` DAC truncation |
| P2 | 13 | LFO/delay wall-clock timing, analogue smoothing/output scale and revision differences | Exact hash-scoped B-2 rate, delay and fade algorithms |
| P2 | 14 | Portamento pot/ADC transfer, hysteresis, cadence and revision differences | Exact hash-scoped B-2 coefficient and 8.8-state law |
| P0 | 15 | Loaded oscillator/sub/noise mixer levels and their actual filter-drive budget (chiefly the WAVE output's source impedance; the 39 kΩ noise leg and 33 kΩ sub-emitter path are traced readings from 2026-08-07 with one crossing below junction-dot certainty) | Node-specific 12 Vpp/4 Vpp anchors, the 68 kΩ/560 Ω core attenuator, and the mixer topology from the p. 13 read: one summed WAVE output per voice, sub via 27 kΩ + diode, C56/C50 coupling, sources muted at their generators — legs never switch (2026-08-07) |
| P2 | 16 | Calibrated TP8 capture (PSD/distribution against the shaped model), and physical filter-startup excitation | Shared generator topology, TP8 4.0 Vpp adjustment, and the 33.9 Hz/4.82 kHz band-shaping derived from the p. 13 designators (C42/4.7 kΩ and C41/R79, 2026-08-07) |
| P3 / dependency | 17 | Real VOLUME gang tracking plus selector, jack, headphone and external-load transfer | Nominal-linear `10KB×2` law and fixed 29.313 kΩ internal wiper load |
| P2 | 18 | Hardware cutoff-converter knee and upper saturation curve | Exponential audio-range law and transparent 50 kHz product cap |
| P1 | 19 | Voice-module BA662 control-current-to-gain curve near cutoff and residual thump after the service null | BA662 pins and separate signal/control paths, ENV/GATE ownership, 6 Vpp endpoint, the per-card minimum-thump procedure, and the anchored +0.25…+0.27 V TP7 control-rail standoff the shipped 150 mV onset now sits relative to (2026-08-07) |
| P2 | 20 | TR11/TR12 wet-mute switching envelope, leakage and click | Device identity, wet-only mute location, continuously running BBDs, and the full gate-drive network designators from p. 15 |
| P2 | 21 | Coupled C14/HPF transfer, switch-state memory and mode-change transient | Parts, placement, asymptotic C14 loads, the established HPF endpoints, the adjudicated 225.8/720.5 Hz cut corners (the 1 MΩ pair biases the mux side and cannot move them — 2026-08-07), the mode→tap map from the p. 13 truth table, and the Boost shelf — its +10.50 dB/+1.41 dB/59.41 Hz constants are derived from the corrected 300 dpi p. 15 branch read, within 0.016 dB of the exact two-zero/two-pole solve (2026-08-07); the withdrawn band-boost branch must not return |

The most consequential audible blockers are OQ-01, OQ-03, OQ-05, OQ-09,
OQ-15 and OQ-19. A well-instrumented original unit can also collect OQ-02, OQ-03,
OQ-05, OQ-17 and OQ-20 output data in the same session; keeping those captures
on one calibration/load chain would remove several cross-normalisation errors.

A later public-source pass is recorded in
[Evidence search — 2026-08-06](#evidence-search--2026-08-06). It moved OQ-01,
OQ-03, OQ-04, OQ-06, OQ-07, OQ-10 and OQ-18 to *partially resolved* or added
quantified contradictions, and records five refinements measured as inaudible
so they are not attempted again. Its first OQ-02 assessment is retained as the
history of the former cubic; the subsequent primary-schematic read derives the
nominal law. No third-party measurement is promoted to an anchor.

### Evidence baseline

The supplied evidence-search reports and final compilation have been reconciled
with the project, the primary schematics and hash-identified firmware. They were
not imported verbatim: several page, test-point, DAC-width and mute-transistor
identifications were incorrect. The primary hardware sources for this pass are
the [Roland JUNO-106 Service
Notes](https://www.vintagesynthparts.com/wp-content/uploads/2017/03/JUNO-106_SERVICE_NOTES.pdf),
the [Roland owner's
manual](https://cdn.roland.com/assets/media/pdf/JUNO-106_OM.pdf), and the
[Panasonic MN3009
datasheet](https://www.experimentalistsanonymous.com/diy/Datasheets/MN3009.pdf).
The 2026-08-04 output-path follow-up also uses Panasonic's
[JIS/EIAJ potentiometer taper definitions](https://mediap.industry.panasonic.eu/assets/imported/industrial.panasonic.com/ac/cdn/e/control/encoders-potentiometers/potentiometers/catalog/sw_vr_eng_common.pdf)
and NEC's 1983
[µPC1252H2 data and application circuit](https://bitsavers.org/components/nec/_dataBooks/1983_NEC_Integrated_Circuits_for_Consumer_Use.pdf).
The supplied 2026-08-05 evidence-search artifact has SHA-256
`e6b6f500a3968191efd239aca9a786bd7a2643d97fd51bfc030b265ae8ab8ab5`.
It contained a report and measurement protocols, but no JUNO-106 CSV, audio,
scope export, board photograph or other raw capture. At that stage the
primary-source and direct-teardown reconciliation closed no task, but narrowed
four boundaries. The later direct read of Service Notes pp. 8 and 15 closes the
nominal part of OQ-02:

- NEC pp. 256–260 directly specify the µPC1252H2 GC1 voltage-to-gain constant
  as −5.9 mV/dB typical (5.8–6.1 mV/dB magnitude) over −30 to +30 dB.
  Service Notes p. 8 supplies the +4 to −6 V converter range, while p. 15
  supplies R30 2.2 kΩ, C7 10 µF NP, R32 1.5 kΩ, R31 47 Ω and R165 15 kΩ.
  Together with `d=b<<5`, these derive the nominal byte-to-GC1 and settling laws;
  OQ-02 now asks only for installed-circuit variation and endpoint validation.
- A [photographed A1QH80017A
  teardown](https://obsoletetechnology.wordpress.com/projects/80017a-vcfvca-teardown/)
  identifies one IR3109 and two BA662 devices, assigning one BA662 to resonance
  and the other to the voice VCA. This settles device topology for OQ-09/OQ-19,
  not either control curve.
- Service Notes p. 15 identifies final summer IC6 as TA75558S. That removes
  device identity from OQ-05, but a part number and ±15 V rails do not determine
  its loaded in-circuit clipping swing.
- Service Notes p. 19 sets resonance to 4.8 Vpp and voice-VCA gain to 6 Vpp
  during calibration. Those are endpoint procedures without published
  tolerances; neither de-embeds loop gain, knee or deadband.

The report's proposed 26 kHz chorus fixture remains JUNO-60-only. Its roughly
150 mV no-current region comes from a
[simulated/bench circuit reconstruction](https://atosynth.blogspot.com/2019/01/juno-filter-vca-and-resonance-cv.html),
not a stock calibrated-card sweep, so it is a probe-density lead rather than a
JUNO-106 fixture. Likewise, the service setpoints do not justify invented ±5%
tolerances. Finally, the exact 68 kΩ/560 Ω divider attenuation is
`560/(68000+560)`, or 1/122.43; `68000/560 = 121.43` omits the shunt resistance
from the divider denominator.

The supplied B-2 image's effective first 4 KiB has SHA-256
`b75d27d181dee58a7e969aa5119e6ac96f624066a8d84b22eb7f2523988e4527`;
the supplied A-5 assigner image's effective first 4 KiB has SHA-256
`72132b8803bd02d2640612aa0055a05fb2f478b03103a6031ddae642fd96a8f5`.
The second half of each 8 KiB container is `0xff` padding. The supplied 2 KiB
`Juno-6.bin` has SHA-256
`9373f259aa20e4e1ef146d2c2bef1fd16aa7ef78f4cbd2bf82849416a4dafe34`
and remains comparison evidence only; it cannot establish a JUNO-106 value.
OQ-12 through OQ-14 record only equations, hashes and behavioural vectors
verified against the B-2 image, not ROM contents. Addresses taken only from the
[unofficial, explicitly partial and inaccurate IC29
disassembly](https://github.com/ErroneousBosh/j106roms) remain
provenance-pending unless a hash-identified supplied image, the official timing
chart or a hardware observation independently corroborates them.

A negative search result does not downgrade stronger evidence already recorded
by this project. Detailed tasks below retain their settled boundaries so a
future researcher does not spend a pass rediscovering them.

## OQ-01 — Absolute JUNO-106 chorus timing

**Priority:** P0

### Task definition

Measure the absolute modulation rates and delay-sweep endpoints of both chorus
modes on identified, healthy, calibrated JUNO-106 unit(s), and estimate nominal
values only to the extent the sample and uncertainty support.

A 2026-08-05 transcription of Service Notes p. 15 closed the topology and the
mode-rate ratio; the 2026-08-06 netlist pass corrected its comparator wiring.
IC1 (µPC062) is an integrator plus a Schmitt comparator: C3 sits across IC1b
pins 6 and 7, and R6 47 kΩ from the comparator's own output meets R7 33 kΩ
from the integrator's output at the non-inverting input, the inverting input
grounded. (The 2026-08-05 transcription placed R7 on pin 2 with R15 1 kΩ
dividing pin 3 down; that reading put both rates 34× high and the sister
board's netlist has no divider resistor on the node — do not reuse it.) IC2a
then inverts once through R10/R9 33 kΩ, so TP4 carries the triangle and TP3
exactly its negative. Two results follow with no measurement:

- The modulation waveform is a **straight, symmetric triangle**, because the
  integrator is fed a constant current for the whole of each half cycle. It is
  not the exponential-segment waveform an RC relaxation oscillator makes.
- The **mode-rate ratio is 1.6234799**, this instrument's own. The CHORUS I/II
  line drives JFET Tr1, which *shorts R3 2.2 MΩ* — it does not insert it in
  series, as an earlier note in this file wrongly described. With the
  integrator input at virtual ground the shunt leg and R8 both return to 0 V,
  so the charging current sees `R_eff = R8·R5/(R_sh ∥ R8) + R8`, giving
  6.4352941 MΩ with Tr1 conducting (mode I, the slower leg) and 3.9638889 MΩ
  without it (mode II). R5 = 1 MΩ, R8 = 2.2 MΩ, R4 = 680 kΩ, R3 = 2.2 MΩ.

The **absolute scale is now derived** (2026-08-06, netlist-corroboration pass):
`f = 1/(4·β·R_eff·C3)` with β = R7/R6 = 33/47 (the summing-node comparator
reading, netlist-verified on the sister board's clone) and C3 = 0.1 µF
(reported as ".1" on p. 15; 100 nF in exactly the integrator's position in
that same netlist) gives **0.5532934 Hz for I and 0.8982608 Hz for II** — both
within 3% of the 106-chorus-clone scope readings, both truncating to the
owner's manual's about-0.5 and about-0.8. What is still open here is
**hardware confirmation** (one TP4 capture closes it) and a **calibrated
sweep capture of an original unit**. As of 2026-08-07 the implementation
ships the 106's own third-party-measured sweep of **1.4–6.4 ms**: the
2026-08-07 ModWiggler read established its provenance in full — it was
scoped on a designator-faithful build of the p. 15 board (independently
assessed as 1:1 with the service manual) carrying genuine 256-stage
MN3009s, published as scope plots, and compared directly against the
measurer's own real JUNO-106 with the two sweeps called identical. Its
±2.5 ms excursion also matches the one independent depth report on record.
The **JUNO-60's** calibrated 1.66–5.35 ms capture is thereby superseded as
a sibling-instrument value and stays in the suite as a comparison figure
(the two boards share the Tr22 clock driver, but a sibling measurement is
never primary 106 evidence). Remaining contradictions stay recorded: that
clone kit's build guide expects 28–38 kHz clocks (≈3.4–4.6 ms), and one
suspected-faulty build read 28–60 kHz; the mod-lore inference near
3.2–8.5 ms is uncalibrated. A calibrated original-unit capture still owns
the final word.

The formerly claimed 1.54–5.15 ms JUNO-106 sweep had no valid measurement and
must not be reintroduced. The sibling's own rate ratio of 1.682 is likewise
superseded by this instrument's 1.6234799 and must not be reintroduced.

With β = 33/47 and C3 = 0.1 µF the scale is derived, so what a hardware
capture adds is confirmation against Roland's own board: one TP4 period
checks the rates directly, and the triangle's peak-to-peak amplitude against
IC1a's saturated swing measures β itself (their ratio is `2·β·V_sat`
against `2·V_sat`). The falsified `1/48` divider figure must not be used in
any of that arithmetic.

### Needed output (for LLM)

- Raw, multi-cycle TP3 and TP4 low-frequency modulation captures in Chorus I
  and Chorus II, with both lines recorded even if they appear identical. TP3
  and TP4 are not BBD phase-clock nodes: do not derive `128/f` delay from their
  edge intervals.
- Minimum and maximum clock frequency at each MN3101/MN3009 clock connection
  over a complete modulation cycle, plus mode periods and frequencies.
- The clock frequency as a **time series across at least one complete
  modulation cycle** — a demodulated instantaneous-frequency trace, or
  timestamped period measurements — not merely its extrema, captured
  simultaneously with the TP3/TP4 modulation waveform so the clock trajectory
  can be referred to the voltage that produced it.

  This is the one output that resolves the sweep law, and the model's position
  on it reversed on 2026-08-06 when the first such series surfaced. Service
  Notes p. 15 shows each MN3101 driven by a transistor voltage-to-current
  converter (Tr22 with R133 2.2 kΩ, R134 22 kΩ and R135 1.8 kΩ, against C53
  150 pF), which is the shape of a *current-controlled* oscillator —
  **frequency** linear in the control voltage rather than period, which would
  bend the delay hyperbolically. But KR-106's ~50-point click-timing series
  across a real 106's modulation cycle fits the delay **linear in time** with
  16 µs RMS residual and "no detectable exponential curvature", and its
  changelog records shipping clock-domain modulation and reverting on that
  measurement. The linear-in-delay trajectory therefore ships as the default,
  and the frequency-linear reading waits behind
  `enableChorusHyperbolicSweep`. The KR-106 series is below this project's
  anchoring bar (raw clicks unpublished), so the question stays open: a
  calibrated time series — or the MN3101's bias transfer from its datasheet —
  still settles it.

  **The retained hyperbolic path had its endpoint defect fixed before it was
  demoted to a hypothesis.** An earlier implementation scaled the hyperbolic
  delay about the *centre*, which moved the endpoints: with the measured
  centre 3.505 ms and sweep 1.845 ms, at Unit Character 1.0 the rendered
  range became about 2.30–7.40 ms instead of the measured 1.66–5.35 ms — a
  span 38% wider than the only measured figures this chorus has, and the
  deviation scaled with Unit Character even though whether the oscillator is
  current-controlled is a topology fact, not a component tolerance. When
  enabled, `Chorus::process` sweeps the *clock* linearly between the clocks
  of the shipped delay endpoints — now `128/6.4 ms` and `128/1.4 ms`,
  derived from the same centre/sweep numbers with no new constant —
  which keeps both endpoints exact at any amount
  of Unit Character, so the two laws differ only in the path between the
  endpoints — which is exactly what the requested time series measures.
- The peak-to-peak amplitude of the modulation triangle at TP4, and the
  saturated output swing of IC1a, in the same capture. Their ratio measures
  `β` directly — confirming (or falsifying) the netlist-derived 33/47 against
  Roland's own board — and with β and one TP4 period the capture also
  independently re-derives C3 against the corroborated 0.1 µF.
- A table containing mode, line, clock minimum/maximum, derived delay
  maximum/minimum, modulation rate and uncertainty. Use
  `delay_seconds = 128 / clock_hz` for the two-phase 256-stage line, where
  `clock_hz` is the repetition rate of either individual MN3009 CP1/CP2
  phase—not the upstream oscillator frequency or a count of both phases'
  transitions.
- Instrument and measurement metadata required by the common execution
  contract, and an explicit comparison with the provisional JUNO-60 values.
- A complete proposed per-mode set of rate, minimum/maximum delay (or
  centre/sweep) and tolerances only if the evidence genuinely supports a
  JUNO-106 nominal.

## OQ-02 — Installed common-VCA tolerance and endpoint validation

**Priority:** P2

### Task definition

Measure how a healthy installed common uPC1252H2 (IC5) departs from the now-
derived nominal VCA LEVEL law. Its position after the six-voice sum and shared
high-pass and before the chorus, and its ownership by one shared hold, are
settled. So is the nominal path:

- Hash-identified B-2 behavior forms aligned word `b<<7`; the converter routine
  drops the bottom two bits, giving physical 12-bit code `d=b<<5`.
- Service Notes p. 8 gives VCA LEVEL's converter span as +4 to −6 V. With an
  ideal 12-bit R-2R convention, `Vhold=4−10d/4096`. Division by 4096 is an
  explicit ideal-DAC assumption, not a measured full-scale endpoint.
- Service Notes p. 15 shows R30 2.2 kΩ from the converter to the C7 10 µF NP
  hold node, then R32 1.5 kΩ to GC1, with R31 47 Ω to ground and R165 15 kΩ
  to +15 V. R32 was the least-legible of these values in the circulating
  scan; the 2026-08-07 complete-scan pass reads it unambiguously as 1.5 kΩ,
  closing that caveat.
- Solving that loaded network gives
  `Vgc=0.01250467817·Vhold+0.04626730922`. NEC pp. 256–260 specify
  `gain_dB=Vgc/−0.0059` typical, hence
  `gain_dB=−16.3196647+0.165581014·b` for bytes 0…127.
- C7 sees
  `R30||(R32+(R31||R165))=908.249 Ω`, deriving `τ=9.08249 ms` and
  `fc=17.523 Hz`. C12 10 µF NP/R36 33 kΩ independently gives the settled
  0.482288 Hz signal-input coupling pole.

That nominal law replaces the former voiced cubic and no longer blocks the
implementation. The remaining question is real-unit variation: resistor and C7
tolerances, +15 V rail error, DAC integral/end-point error, R32 scan
transcription, and the NEC part's 5.8–6.1 mV/dB specified spread can move the
curve and settling. The byte-exact factory bank remains a regression corpus,
not a substitute for measurement. The targeted
[before/after comparison](audio/realism-comparisons/common-vca-level/README.md)
exercises bytes 0, 32, 64, 96 and 127 plus rapid transitions, dry and through
Chorus II, with exact automation and shared listening gain; it measures the
implementation change, not an original unit.

### Needed output (for LLM)

- A 128-row CSV containing commanded byte, physical 12-bit DAC code, held-node
  voltage, GC1 voltage, simultaneous settled VCA input/output amplitude,
  `Vout/Vin`, relative dB, and residual against the nominal equations above.
- A step-response capture at C7 and GC1 sufficient to fit the installed time
  constant and distinguish acquisition behavior from subsequent hold/droop.
- Exact probe points, tone frequency and level, VOLUME setting, load, supply
  rails, board/revision, R30/R32/R31/R165/C7 measured values, calibration state,
  warm-up and gain-reference setting.
- A fitted residual curve with uncertainty, explicitly separating ideal-DAC,
  passive-network and uPC1252 contributions. (R32's value is no longer owed:
  the 2026-08-07 complete-scan pass reads it unambiguously as 1.5 kΩ.)
- Regression fixtures for nominal byte-to-GC1/dB values, monotonicity and the
  9.08249 ms nominal step response, plus a separately labelled measured-unit
  profile only if raw evidence justifies one.

## OQ-03 — Chorus noise and SNR under calibrated conditions

**Priority:** P0

### Task definition

Characterise the in-circuit noise of a healthy JUNO-106 chorus in Off, I and II.
No compander exists in this circuit, so a noise model is structurally required,
but the current per-line floor is voiced. A noise voltage without a same-path
reference tone, bandwidth and weighting does not establish SNR.

### Needed output (for LLM)

- Raw tone and noise captures from the same stated probe point for Off, I and
  II, on both output channels, without moving gain controls between captures.
- Input termination, patch/control settings, dry-path inclusion, VOLUME,
  output selector, load, bandwidth, weighting, detector type and RMS
  integration method; also state reference-tone frequency, injection point,
  source distortion and the exact tone-removal/excluded-bin method, including
  treatment of chorus-modulation sidebands.
- Tone level, broadband noise level, SNR, spectrum and uncertainty for each
  state/channel; identify hum and discrete clock spurs separately from random
  noise. Use analogue/acquisition bandwidth sufficient for the stated clock-
  feedthrough scope; a 96 kHz file alone cannot characterise spurs extending
  toward the BBD clock range.
- If possible, a wet-only or de-embedded estimate for each BBD line without
  confusing a fault-induced wet-only output with normal operation.
- A simultaneous stereo capture plus channel cross-spectrum, coherence and
  correlation if the evidence will be used to choose independent versus
  correlated per-line noise sources.
- A proposed stochastic model and level relative to the modelled BBD input
  only when the captures support that conversion.

## OQ-04 — Loaded post-BBD support-chain transfer

**Priority:** P2

### Task definition

Resolve the effective loaded transfer after each BBD. The chief uncertainty is
the tap-summing pole: the present nominal
first-order model uses `(3.3 kΩ || 47 kΩ) × 2.2 nF`, or 23.46 kHz, while
treating the active MN3009 output as ideal. The MN3009 datasheet does not
specify the needed output impedance directly — the 2026-08-07 solve derives
Rs ≈ 3.70 kΩ for the summed pair from the Gi–RL panel, leaving the per-leg
topology ambiguous (loaded-pole candidates 11.9/15.1/22.2 kHz; see the
session record). Service Notes p. 15 identifies the two
branches as IC8 with R118/R119, R117 and C45, and IC10 with R111/R112, R110 and
C48; alternating active taps make the loaded circuit time-varying. The later
7.234/11.315 Hz C28/C25 coupling calculations likewise omit the driving
emitter followers' source impedance. TR11/TR12 on-resistance, leakage and
switching belong to OQ-20. Use one declared route: measured MN3009 and
emitter-follower output impedance followed by full modified-nodal analysis, or
a calibrated de-embedded wet-only sweep.

The 2026-08-06 module netlist pass corroborated the *part identity* of the
output sections against the input sections on the sibling clone board: three
complete Sallen-Key chains, one pre-BBD and one per output line, every
section 22 kΩ/22 kΩ with 820 pF/680 pF then 1.8 nF/270 pF, plus a per-BBD
10 kΩ/2.2 nF input pole and both 3.3 kΩ taps into 47 kΩ/2.2 nF. The corner
values themselves, Roland's own p. 15 capacitor codes, and the loaded
time-varying transfer remain exactly this task.

The renderer's residual charge-transfer state is no longer an open numerical
law. It advances once per modeled BBD shift (one fCP period), so fixed coefficient 0.8654743 already makes
the absolute pole track the clock while preserving response versus normalized
`f/Fclock`. At the raw node before numerical reconstruction, together with the
explicit held output it gives −3.000 dB versus DC at 40 kHz clock/12 kHz signal,
or −2.972 dB against the datasheet's 1 kHz reference. The 0.028 dB residual is
documented rather than given an inaudible retune. The former additional factor
`1+(clock−26000)·1.5e−6` double-counted
that scaling: it gave −2.757 dB versus DC, or −2.732 dB versus 1 kHz, and swept
the normalized 0.3-cycle response from about
−3.04 to −2.14 dB over the modelled 23.9–77.1 kHz range. Removing it fixes an
implementation inconsistency and unsupported LFO-correlated brightness; it does
does not quantitatively establish the real MN3009's transfer at other clocks.
The datasheet itself contains low-resolution typical `Gi-fi` curves at fCP 10,
40 and 100 kHz, so the remaining gap is extraction/calibration and installed-unit
confirmation, not a total absence of multi-clock evidence.

Those figures describe the raw held node before numerical output
reconstruction. The emitted deterministic step now receives a compact polyBLEP
after residual transfer loss and before the tap-summing pole, so the emitted
waveform is not a literal rectangle. This simulation-grid correction does not
resolve the physical MN3009 output impedance, normalized transfer, BGA or loaded
support network requested here; those remain OQ-04. Measurements must therefore
identify whether they address the raw held model, the reconstructed emitted
node, or an installed device, and de-embed the numerical reconstruction when
comparing the first two.

### Needed output (for LLM)

- The selected route and why its evidence is sufficient.
- For the circuit route: measured complex MN3009 and emitter-follower output
  impedance versus frequency, clock, bias and signal conditions, a
  component/designator table, complete small-signal netlist, transfer equation
  and uncertainty.
- For the sweep route: raw input/output data from 100 Hz to beyond the candidate
  −3 dB point where feasible; exact probes, level, loading and fixture response.
  Hold each BBD clock at several documented fixed rates spanning the usable
  range, including 40 kHz, or use a validated
  linear-periodically-time-varying method, because a normally sweeping chorus
  is not LTI. Include both output taps and stereo lines or state the narrower
  scope. Do not force a through-BBD sweep beyond the line's usable
  clock-dependent band where the paired-node ratio becomes noise-dominated;
  prefer local output-impedance injection there.
- Explicit de-embedding of the pre/post support filters, BBD zero-order hold,
  charge-transfer response, numerical output-step reconstruction, fixture and
  load. Removing only the dry path is not sufficient.
- Plot residual MN3009 magnitude and phase against both absolute frequency and
  normalized `f/Fclock`. Compare with the fixed-coefficient baseline rather than
  assuming either clock invariance or the removed affine clock multiplier.
- Fitted pole(s), magnitude/phase residuals and a direct comparison with the
  current ideal-source 23.46 kHz tap pole and 7.234/11.315 Hz output-coupling
  corners.
- Replacement coefficients and response fixtures at supported sample/clock
  rates, or a precise statement of why the result remains unresolved. A sweep
  ending at 20 kHz cannot resolve a candidate pole above its measured band.

## OQ-05 — Loaded TA75558S IC6 and High-output clipping swing

**Priority:** P0

### Task definition

Measure the actual loaded clipping swing of final summer IC6 and the High
output. Service Notes p. 15 identifies IC6 as a TA75558S; the schematic also
fixes the linear resistor gains and ±15 V system rails. Device identity and
rail labels can bound a protocol, but they do not fix the real in-circuit swing
under the board's supply network and output load. This is a hardware question;
do not mix it with the separate plug-in dBFS policy in OQ-06.
The current factory stress audit identifies reproducible hot fixtures—most
notably B33 Lute, B44 Contact Wah and A63 Frontier Organ—but their floating
dBFS peaks are not evidence of the IC6 voltage at which real hardware clips.

### Needed output (for LLM)

- Simultaneous IC6-input, IC6-output and High-jack waveforms on both channels
  while a low-distortion source injected at a documented point is raised in
  known steps. An internal programmed tone is acceptable only if the captures
  prove every IC6 input remains linear.
- Chorus state, VCA LEVEL, VOLUME, output selector, load impedance, tone
  frequency, rails, probe attenuation and instrument calibration state.
- A declared objective clipping criterion, such as a specified THD threshold
  or measured departure from linear gain, plus the first-clip Vpk, Vpp and
  Vrms. Keep service-standard nominal level separate from clipping level.
- Positive/negative asymmetry and a declared frequency/load matrix. If only one
  condition is measured, scope the conclusion to that condition rather than
  calling it the general loaded swing.
- The fitted TA75558S device and any substitution/repair history, together
  with a comparison against datasheet output-swing conditions that does not
  substitute an unloaded datasheet typical for the board measurement.
- An analogue headroom table and uncertainty suitable for validating the
  output-stage model. Do not invent a rail fraction when no measurement exists.

## OQ-06 — Absolute output-reference calibration

**Priority:** Dependent calibration; the product convention is settled and the
remaining value depends on OQ-05/OQ-17

### Task definition

The product convention is settled: for a declared reference sine of
`Vref_rms`, apply

`digital_sample = analogue_output_volts * 10^(-18/20) / Vref_rms`

once at the final output boundary, after the modeled VOLUME/output path. This
places that reference sine at -18 dBFS RMS without changing drive inside the
VCF, BBD or IC6 model. Floating output beyond +/-1 is allowed and this mapping
must not add a clipper or limiter. Preserve the previous effective output gain
as the migration default for `Vref_rms`; its absolute voltage meaning remains
calibration metadata until OQ-05/OQ-17 establish the corresponding hardware
condition. The engine's 2.6 V internal coordinate is not that calibration.

### Needed output (for LLM)

- Establish the physical `Vref_rms` condition from the output-stage and
  selector measurements in OQ-05/OQ-17, including load, selector, VOLUME,
  frequency, calibration state and uncertainty. Do not change the adopted
  -18 dBFS RMS boundary convention merely because that value is still open.
- Supply the raw reference-sine capture and simultaneous IC6/High-jack levels,
  the published-level interpretation, meter calibration and uncertainty needed
  to relate physical volts to the selected reference condition.
- Report whether High, Mid and Low require distinct physical reference rows
  under their stated loads. Return the evidence-backed `Vref_rms` value(s), or
  state precisely why OQ-05/OQ-17 still prevent calibration. Product mapping,
  session migration and no-limiter behavior are settled implementation
  guardrails, not evidence-search deliverables.

## OQ-07 — Converter hold topology and time constants

**Priority:** P1

### Task definition

Recover the acquisition, droop, loading and destination-specific time constants
of the converter holds. Ownership is no longer open: Service Notes pp. 5, 8 and
13 show three 8-way muxes containing 18 per-card holds—DCO, VCF and ENV/GATE
VCA for six cards—plus five shared holds—SUB, stored VCA LEVEL, PWM, RESONANCE
and NOISE—and one unused channel. The pass is 4.2 ms. Approximately 522 µs for
the VCF hold family and 687 µs for the per-voice ENV/GATE VCA hold family are
existing component-derived anchors. The common stored VCA LEVEL path has a
separately derived post-S/H pole: p. 15's R30 2.2 kΩ, C7 10 µF NP, R32
1.5 kΩ, R31
47 Ω and R165 15 kΩ derive `Rth=908.249 Ω` and `τ=9.08249 ms`. The p. 13
read (2026-08-07) made the per-destination post-hold smoothing inventory
designator-complete, and its two outright-fixed networks now ship as
derived constants: the PWM hold reaches the comparators through R117
100 kΩ/C62 47 nF and then R116 560 kΩ/C63 4.7 nF around IC17a — two
cascaded poles, 4.7 ms and 2.632 ms — and the stored SUB level crosses
R11 1 kΩ into C1 10 µF, one 10 ms pole, ahead of the R9/R10 inverter.
DCO, RESONANCE and NOISE retain their labelled 522 µs compatibility
defaults.

Two model questions now depend on this task, both added 2026-08-06:

1. **What do the 522/687 µs figures denote?** The engine charges each hold
   toward its target for the whole 4.2 ms pass. Physically the multiplexer
   connects a given hold for roughly 1/23 of that, then disconnects and the
   capacitor holds and droops. If 522 µs were the on-window RC, a hold would
   reach only about 30% per pass and take some six passes to settle, which
   would visibly slow every envelope — so the figures most likely already
   describe settling. Establishing which they are decides whether the control
   voltages are continuous ramps, as now, or the stepped staircase a
   track-and-hold produces. That staircase is what the instrument's filter
   sweeps are usually described as sounding like, so this is an audible
   question, not a bookkeeping one.
2. **What is the hold capacitance, and the multiplexer's on-resistance?**
   A CD4051's channel switches inject ΔQ = C_gd·ΔV into the hold capacitor as
   the scan steps. Two mechanisms modelling that injection were removed on
   2026-08-06 because they wrote into a target the same converter write
   recomputed, and so were measured at −360 dBc: bit-identical output. A
   physical injection lands on the *slewed hold state*, and its size is
   ΔQ/C_hold — neither term established. Until they are, no injection is
   modelled. The former release-tail re-strike shortcut is removed for the
   same reason: it wrote up to 0.8 V straight into the VCA hold at host Note On,
   before any converter slot or mux transition could physically occur.

### Needed output (for LLM)

- A schematic/measurement table for every destination: confirmed converter
  channel, switch/multiplexer path, node/designators, source impedance, R, C,
  loading, calculated time constant, droop and expected 10–90% settling.
- Step-response captures at the actual nodes where practical, including raw
  waveforms and the fitted exponential/multi-pole response.
- A reconciliation of component-derived and measured constants with tolerances
  and loading included.
- An implementation-delta table and scan/settling regression fixtures, with
  unresolved nodes explicitly marked and separately parameterized even where a
  compatibility default currently happens to equal 522 µs.

## OQ-08 — Exact intra-pass timing and DCO pitch-write restart

**Priority:** P1

### Task definition

Recover the exact physical timing of one complete converter scan. Ordinal order
is settled by the Service Notes p. 8 timing chart: RESONANCE, common VCA LEVEL,
SUB, DCO 1–6, PWM, then VCF1/VCA1 through VCF6/VCA6, then NOISE over the 4.2 ms
pass. The model now executes that exact 23-write logical queue with a fractional
4.2 ms scheduler. Its default normalized compatibility profile spreads the
ordered events monotonically across the pass, preserving the chart's qualitative
non-simultaneity without claiming exact timestamps; a phase-zero diagnostic
profile is retained for comparison. The reported
approximately 125 µs VCF-before-VCA offset and every other time distance remain
provisional; do not invent them by distributing ordinal events evenly.

The model also treats a changed-pitch write to an idle/releasing physical card
as a timer restart: phase returns to zero, comparator and sub-divider logic are
forced to declared states, and the resulting value/slope discontinuities are
bandlimited numerically. That is a model of plausible reload behavior, not a
fact established by the service timing chart. This task must determine what the
8253 write and surrounding DCO circuitry actually force, and at which edge.

### Needed output (for LLM)

- A timestamped scan timeline covering a complete pass: LFO/envelope
  computation, converter selection, sample instant, destination write, hold
  acquisition, divider programming, DCO-reset consumption, gate/assignment
  events, common resonance/VCA writes and voice number.
- Primary firmware/schematic evidence and, if available, simultaneous logic or
  analogue captures from converter selects and two or more hold nodes.
- For a deliberate changed-pitch write, simultaneous timer-output, ramp,
  comparator/pulse and sub-divider captures establishing reload timing, ramp
  discharge/phase, and whether either logic state is reset, toggled or left
  running. Include same-pitch/legato and released-card controls.
- Exact nominal offsets and jitter; determine whether timing is invariant or
  data-dependent, and enumerate every conditional path involving key,
  envelope, bender or panel activity.
- A comparison against both current normalized and phase-zero profiles, including
  the maximum control error and an audible/null-test assessment.
- A sample-accurate replacement schedule and deterministic tests, or a protocol
  if the approximately 125 µs ordering cannot yet be independently verified.

## OQ-09 — Resonance byte-to-loop-gain law

**Priority:** P1

### Task definition

Recover the complete analogue resonance law. The current compatibility profile
uses a quadratic through loop gain about 0.91 at 30% travel and a nominal
self-oscillation threshold near 90%, followed by a linear segment to a fitted
maximum near 4.19. Those are comparison landmarks chosen by the model, not
measured hardware anchors. A photographed A1QH80017A teardown identifies the
IR3109 filter and the BA662 used in its resonance-feedback path. Service Notes
p. 19 sets every card to a 4.8 Vpp self-oscillating sine during calibration,
but publishes no amplitude tolerance and does not de-embed loop gain. Service
Notes pp. 5, 8 and 13 settle one shared IC26-channel-6 hold; hash-scoped
firmware analysis gives aligned word `b<<7` and physical DAC code `b<<5`.
Hold settling belongs to OQ-07; this task owns every analogue step from DAC
voltage/current to loop gain, compensation, oscillation onset and
feedback-dependent pitch correction. A reconstruction of the control circuit
reports roughly 10 V full control and an approximately 150 mV no-current
region. Because that result is not a stock calibrated-card sweep, use it only
to place extra samples around a possible conduction onset—not as a target or
regression tolerance.

Added 2026-08-06 (module netlist pass): the dksynth-lineage reconstruction —
netlist-explicit in `ThomHPL/Open80017a` and validated by ear as a build in a
real JUNO-106 — wires the resonance OTA's non-inverting input from VCF IN
through 24 kΩ against a 1.5 kΩ shunt (÷17.0), its inverting input from
VCF OUT through 100 kΩ against 1.5 kΩ (÷67.7 loaded), and injects its output
current at the first filter stage's 4.7 kΩ/560 Ω/68 kΩ summing node. Stage 1
is a feedback stage (−68 k/4.7 k, the inter-stage sections −68 k/68 k
unity), so converting those dividers into the model's loop-gain coordinate
is resistor-only — the OTA's transconductance cancels:
`(67.7/17.0)·(4.7/68) = 0.275` input boost per unit loop gain, against the
shipping voiced `0.2296`, same linear form. This is depth within one
reconstruction lineage, not a second independent source, so the value stays
unpromoted; the sweep this task asks for still owns it, and can now also
discriminate the reconstruction's first-stage gain structure, which bears on
OQ-15's open input coordinate.

### Needed output (for LLM)

- Preferably a 128-value sweep, on all six voice cards, containing byte/panel
  position, control voltage, BA662 control current if safely accessible,
  explicitly open-loop or de-embedded loop gain, self-oscillation
  state/frequency and steady oscillation amplitude. Output amplitude alone
  cannot identify loop gain because compensation, saturation and oscillation
  trim also affect it.
- Exact probe/injection method, filter cutoff, input level, calibration,
  temperature, threshold criterion and any hysteresis.
- A table or equation for the central law with residuals, confidence intervals
  and monotonicity, compared with the present piecewise quadratic/linear law.
- Separate common control-law behaviour from card-specific residual spread,
  and identify whether each residual acts as a CV offset, amplifier-gain scale
  or another domain. OQ-10 owns the distribution of those residuals.
- Replacement fixtures for 30% travel, oscillation onset, maximum and
  representative intermediate bytes, explicitly labelling the former values
  as compatibility comparisons rather than evidence targets.
- Report the measured 4.8 Vpp service endpoint separately from the loop-gain
  law, and state whether the observed low-control onset corroborates or rejects
  the reconstruction lead without treating that lead as prior truth.

## OQ-10 — Post-calibration voice dispersion and thermal wander

**Priority:** P3

### Task definition

The calibrated nominal product model is settled as zero inter-voice spread and
zero temporal drift because no qualifying post-calibration population data
supports nonzero distributions. Existing deterministic seeded variation,
including the 375 Hz/0.9992/0.004/up-to-40-cutoff-count drift process, belongs
only to an optional `Unit Character` compatibility/sound-design profile. It is
not a measured JUNO-106 nominal law. Quantify the actual residual differences
among six calibrated cards and their warm-up/time behavior so that evidence can
eventually replace the zero nominal or parameterize the optional profile.
Untrimmed component tolerance must not be treated as residual tolerance after
trimming, and analogue card variation must never be added to the one shared
digital envelope generator.

### Needed output (for LLM)

- A mechanism matrix with circuit designators, raw component tolerance,
  trimmer/calibration status, expected post-calibration residual and current
  model assumption, explicitly comparing the zero nominal and optional
  `Unit Character` profile.
- Repeated measurements for all six cards after calibration, at stable
  temperature and through warm-up, with enough repetitions to separate
  measurement noise, static card identity and time variation.
- Per-mechanism within-unit distributions, correlations, outliers, temperature
  dependence, drift bandwidth and variance. Determine which mechanisms
  genuinely wander and separate common-mode from card-specific drift. Do not
  add envelope-rate dispersion: one digital generator serves all voices.
- A proposed deterministic six-card fixture and separate temporal process only
  for variation actually supported by the captures. Measurements of all six
  cards in one instrument establish within-unit spread only; require multiple
  instruments before fitting a population model.
- Clear **not found** entries and a six-card lab protocol for every mechanism
  that cannot be sourced.

## OQ-11 — Pulse-off pinned-leg mixer behaviour

**Priority:** P1

### Task definition

Determine what the oscillator mixer actually receives when pulse is switched
off. Service Notes p. 9 directly states that about −0.8 V holds the MC5534A
comparator output high; the model now represents that control/comparator state
while retaining a provisional hard-zero audio-path gate. Trace whether the
pinned output, coupling network and mixer impedances introduce DC shift,
residual pulse bleed, loading or a switching transient before replacing that
hard gate. Keep this live-switch transient distinct from an oscillator bug:
the renderer now solves crossings of the free-running ramp and moving PWM
threshold inside each audio sample, so deep PWM cannot miss an edge and emit a
spurious full-cycle blip. A click coincident with changing Pulse on a held note
is still possible under the provisional instantaneous gate and cannot be
removed, or called authentic, until the transition below is measured.

### Needed output (for LLM)

- A designator-level trace from the −0.8 V control through the comparator,
  coupling components and oscillator/filter mixer.
- A small-signal/large-signal equation or simulation netlist for pulse enabled,
  pulse disabled and the switching transition.
- Raw measurements of the mixer/filter input in both states, including DC,
  transient waveform and spectrum under otherwise identical settings. Capture
  both disable and re-enable transitions at multiple oscillator phases,
  frequencies and PWM duties, including the coupling-capacitor initial state.
- Companion live Saw-switch captures at the MC5534A composite WAVE output and
  filter input. The service schematic exposes no separate external Saw gate or
  transition RC, so this must be measured rather than inferred from the Pulse
  control path.
- Quantified bleed/loading/transient level relative to a normal pulse and an
  assessment of audibility across cutoff/resonance settings.
- A recommended model topology and regression specification comparing hard
  gating with the measured pinned-leg behaviour. If a plug-in-only anti-click
  envelope is retained or proposed, label it as product policy and quantify it
  separately from the measured hardware transition.

## OQ-12 — Envelope physical timing and firmware-revision scope

**Priority:** P2

### Task definition

The exact digital law is resolved for the hash-identified B-2 image. Sustain is
`S = 128*b` (`0..0x3F80`). Attack uses a 16-bit selected increment and
`E' = min(0x3FFF, E + A[b])`; key-on selects attack without clearing `E`.
The physical DAC receives `E >> 2`, while the 14-bit recurrence retains the low
two bits. Define `v_hi=v>>8`, `v_lo=v&255`, `c_hi=c>>8` and `c_lo=c&255`:

`Q(v,c) = c_hi*v_hi + floor(c_lo*v_hi/256) + floor(c_hi*v_lo/256)`

The low-byte-times-low-byte term is intentionally omitted. Decay is
`S + Q(E-S,c)` when `E>S`, otherwise `S`; release is `Q(E,c)`. Decay and
release share the same coefficient region. The attack region
`0x0B60-0x0C5F` hashes to
`faef5ad5666a501bfe373f0af4cb345cae8ec6c569821873bb15f69f71ec3eea`;
the decay/release region `0x0D60-0x0E5F` hashes to
`0de73bedf11904538056eec3622b09470461f13ad016103ab9992be73e467754`.
What remains open is physical pass duration/jitter, downstream hold behavior,
audible timing thresholds and whether other firmware revisions differ.

### Needed output (for LLM)

- Independently generated behavioral confirmation of the equations, widths,
  saturation, truncation, shared decay/release selection, retrigger and
  mid-note sustain semantics, stating the exact firmware revision/hash scope.
  Do not reproduce coefficient-table or ROM contents.
- Raw envelope-DAC and audible-output captures with a measured pass period,
  jitter, node, load and timing thresholds. De-embed the OQ-07/OQ-08 hold/write
  schedule and distinguish digital zero from an audible/noise-floor endpoint.
- Compare other identified firmware revisions and report every behavioral or
  region-hash difference rather than generalizing from B-2.
- Legal regression vectors including sustain bytes `0/64/127 ->
  0/0x2000/0x3F80`; attack increments `16384/127/21` and peak pass counts
  `1/129/781`; and release coefficients `4096/65276/65524` with digital-zero
  pass counts `4/984/6083`. Seconds may be reported only with the measured or
  explicitly nominal pass duration.

## OQ-13 — LFO/delay physical timing and analogue transfer

**Priority:** P2

### Task definition

The exact digital law is resolved for the hash-identified B-2 image. Its LFO
magnitude accumulator spans `0..0x1FFF`; each pass adds/subtracts selected
coefficient `c`, clamps at an endpoint and advances direction/polarity state.
One signed cycle contains four magnitude ramps, so
`N_ramp=ceil(8192/c)` and `f=1/(4*N_ramp*T)`. The rate region
`0x0C60-0x0D5F` hashes to
`4e3d87f7f12202e846d4010b08799dabd4d70d3cb5cffa0566933587538ff1d0`.

Delay has a silent hold of `ceil(16384/A[b])` passes using the exact OQ-12
attack increment, followed by a fade of `ceil(65536/F[b>>4])` passes. This
creates eight bins (`0-15` through `112-127`); the fade region
`0x0B30-0x0B3F` hashes to
`e145e0e5de512ef77ae0ffb91cefea40263b8200e78ed2a9a81befc13cf8ac99`.
The fade output uses the accumulator's high byte, so its audible depth is
8-bit quantized. Byte 0 is not delay bypass: it has one attack-derived hold
pass plus a two-pass fade, about 12.6 ms total at nominal `T=4.2 ms`. Physical
pass timing/jitter, output polarity/scale, analogue smoothing and firmware
revision scope remain open.

### Needed output (for LLM)

- Independently generated behavioral confirmation of accumulator width,
  update/clamp ordering, direction/polarity state, delay phases and high-byte
  fade output, with exact firmware revision/hash scope and no ROM/table dump.
- Raw multi-cycle rate, delay-depth and relevant control-node captures with the
  measured pass period/jitter and the method used to separate LFO phase from
  delay-envelope amplitude and analogue smoothing.
- Revision comparison plus regression vectors: rate codes `0/64/127` use
  `c=5/666/4096`, take `1639/13/2` passes per ramp and yield
  `0.036317/4.578755/29.761905 Hz` at nominal 4.2 ms; every bin boundary
  `15/16` through `111/112` is explicit; byte 0 is a three-pass total delay.
- Confirm or replace the nominal eight fade durations
  `8.4/264.6/529.2/789.6/1075.2/1075.2/1075.2/1075.2 ms` using measured `T`,
  without treating those conditional times as independent hardware evidence.

## OQ-14 — Portamento pot/ADC transfer and firmware-revision scope

**Priority:** P2

### Task definition

The exact digital law is resolved for the hash-identified B-2 image. It reads
an 8-bit raw ADC value: raw zero stores coefficient zero directly; every
nonzero value selects an 8-bit coefficient with `index=raw>>1`. Index zero is
also zero, so raw 0 and 1 are both immediate/no glide and codes `2n`/`2n+1`
share a coefficient. The 128-byte region `0x0A00-0x0A7F` hashes to
`06d1c862622b5aaa2b7e42d561dbdf2cd424620a8e46cfa0c2c9deb5c484984e`.
Each of six voice slots keeps an 8.8-semitone state; every pass adds or
subtracts constant `c` toward its target and clamps on crossing, including for
currently inactive slots. Thus `octave_passes=ceil(12*256/c)` and
`octave_seconds=octave_passes*T`. The physical potentiometer/ADC voltage law,
sampling cadence, hysteresis and firmware revision scope remain open.

### Needed output (for LLM)

- Independently confirm the ADC width, raw-zero path, `raw>>1` pairing,
  coefficient application, 8.8 state, inactive-slot advance and endpoint clamp
  for the stated firmware hash without reproducing its coefficient table.
- Measure the physical pot voltage, ADC transfer, noise/hysteresis and sampling
  cadence, including the Off/raw-0/raw-1 transition and endpoint behavior.
- Raw glide traces for several interval sizes and both directions, proving
  constant rate and exposing rounding, asymmetry, endpoint and off-threshold
  behaviour.
- Compare identified firmware revisions and provide legal behavioral fixtures:
  raw `0 -> immediate`, `1 -> immediate`, `2/3 -> c=255 -> 13 passes/octave`,
  `127/128 -> c=13 -> 237 passes/octave`, and
  `254/255 -> c=1 -> 3072 passes/octave`. At nominal 4.2 ms these are
  `54.6 ms`, `995.4 ms` and `12.9024 s` per octave respectively.
- Treatment of reassigned and previously idle voice slots, clearly separating
  the ROM-resolved state semantics from the open physical control mapping.

## OQ-15 — Oscillator-mixer levels and filter-drive calibration

**Priority:** P0

### Task definition

Build a traceable signal-level budget from each oscillator source into the
filter core. Service Notes p. 9 documents saw and pulse near 12 Vpp and the sub
collector-supply amplitude mechanism; p. 19 adjusts shared noise to 4.0 Vpp at
TP8. These are node-specific Vpp anchors, not RMS values or an end-to-end gain
budget. The implementation currently uses peaks of 6.0 V saw, 6.0 V pulse,
5.0 V sub and 2.0 V noise, then a `0.40` filter-input scale. The sub amplitude
and complete node-to-node transfer are not explicitly anchored. A centered
`+/-6 V` source is compatible with a 12 Vpp reading only at the same stated
node; it does not prove the later numerical coordinate. TP8 is downstream, so
its 4.0 Vpp noise adjustment cannot directly establish a `+/-2 V` pre-filter
noise amplitude or distribution. Treat `+/-5 V` sub, `+/-2 V` noise and `0.40`
as voiced compatibility values. This task must not compare voltages from
different nodes as though they were interchangeable.


Added 2026-08-06, resolved 2026-08-07: the sources-or-legs question is
answered by the p. 13/p. 9 read — **sources mute, legs never switch**. Saw
and pulse leave the waveshaper already summed on one per-voice WAVE output
(IC12/IC8/IC4 pin 14/16); the sub joins that line through R101/R97 27 kΩ
behind D6/D5 from its own switch transistor whose collector supply is the
SUB LEVEL rail; the shared noise rail arrives on its own leg; C56/C50
10 µF NP couple the node into the module input. SAW is gated by a control
rail at the generator ("0: saw ON" at Tr24/R148 47 kΩ), PULSE by the −0.8 V
comparator hold, SUB by its collector supply, NOISE by the level OTA. The
node's loading is therefore one configuration-independent constant absorbed
by `filterInputAttenuation` and the output reference; the earlier
four-switchable-100 kΩ-legs Thévenin model is superseded. Still open here:
the WAVE output's source impedance, the exact termination and role of the
33 kΩ/39 kΩ (R102/R103, R99/R98) chain toward the saw on/off rail, the
noise leg's value (the assumed per-voice 100 kΩ was NOT verified in the
scan), and the loaded level budget those would close. Provenance note: the
KR-106 "measured sub/pulse ratio 1.51" recorded by the 2026-08-06 mining
pass could not be re-located in that project's current tree (its own engine
mixes sub at 0.67 against 0.5 waves, ratio 1.34, as engine constants, not
measurements), so that lead is downgraded until raw provenance surfaces.

### Needed output (for LLM)

- A designator-level signal path and impedance/gain budget for saw, pulse, sub
  and noise from their source test points through level control and mixer to
  the filter core.
- Calibrated Vpp, Vrms, DC offset and source impedance at each common node, at
  every byte or a sufficiently dense sweep to validate the sub/noise level
  laws. Zero, midpoint and maximum alone are insufficient.
- Saw and pulse amplitude across representative pitch/range settings and during
  compensation-voltage transients.
- Single-source and representative multi-source captures, including loading
  and the actual filter-input differential voltage.
- A table mapping those measurements to `sawMixVolts`, `pulseMixVolts`,
  `subMixVolts`, `noiseMixVolts` and `filterInputAttenuation`, identifying
  which are physical gains and which are numerical normalisation.
- A replacement level budget, clipping/drive predictions and regression
  fixtures. If the existing 0.40 scale is a derived coordinate conversion,
  show the derivation explicitly.
- Preserve the anchored 68 kΩ/560 Ω filter-stage divider:
  `560/(68000+560) = 0.00816803`, or approximately 122.43:1 attenuation. The
  open 0.40 value is the preceding source/node coordinate mapping, not
  permission to refit that circuit attenuation.
- Keep scope to normal enabled-pulse and static noise transfer. Pulse-off is
  OQ-11 and noise spectrum is OQ-16. Installed common-VCA tolerance is OQ-02;
  downstream clipping, output reference and physical loading are
  OQ-05/OQ-06/OQ-17.

## OQ-16 — Main noise spectrum and filter self-oscillation startup

**Priority:** P2

### Task definition

Characterise two distinct noise mechanisms that must not be conflated:
(a) the one shared audible noise generator mixed into all voices and calibrated
to 4.0 Vpp at TP8 (Service Notes pp. 5, 13 and 19), and (b) the much smaller
voice-module/input-referred noise
that lets a real resonant filter start oscillating from silence. The model uses
bounded uniform white xorshift noise for the shared source — since 2026-08-07
band-shaped by the source's own support circuit as read at designator level
from p. 13: C42 1 µF into the BA662 level OTA's 4.7 kΩ input bias makes a
33.9 Hz high-pass, and C41 100 pF against R79 330 kΩ loads the OTA output
with a 4.82 kHz pole; the level control sits between the two poles and is a
scalar, so the shared source is shaped once with unity passband, keeping the
established in-band density. This settles the *shape* class (the earlier
flat-white placeholder, and KR-106's uncorroborated 34 Hz–5.3 kHz trace, are
superseded by the direct read); the generator's bounded uniform amplitude
distribution and the absolute pre-filter coordinate remain voiced. The model
also uses an unexplained
20 µV per-card white excitation at the filter input, after the open 0.40 source
coordinate scale. Each of the six physical card filters now runs continuously
behind its closed VCA, preserving filter history and its deterministic per-card
noise stream across note assignment and retirement; only a full engine reset
reconstructs those states. Dormant plug-in extension slots have no physical
card and may stop processing. Both discrete sources are normalized by
`sqrt(internal_rate / 192 kHz)` to preserve wall-clock spectral density across
host rates and HQ modes; that is a numerical policy and does not settle their
unknown hardware amplitudes or spectra. This task is separate from BBD chorus
hiss in OQ-03.

### Needed output (for LLM)

- Raw TP8 captures with calibration setting, bandwidth and load. (The circuit
  trace itself was closed by the 2026-08-07 p. 13 designator read; what a
  capture now adds is validation of the implemented 33.9 Hz/4.82 kHz shaped
  model against the real rail.)
- PSD, autocorrelation, amplitude distribution, crest factor, bandwidth and
  discrete spurs for the shared generator; state whether shaped bounded
  uniform noise is adequate over the audible band or whether the source's
  amplitude distribution is audibly non-uniform.
- An input-referred noise estimate for the voice filter/OTA path from component
  data or measurement, with bandwidth, temperature and the physical injection
  location; reconcile that node with where the model injects its nominal
  20 µV.
- Repeated no-input self-oscillation startup captures for all six cards at
  several resonance margins, yielding onset-time distributions and steady
  noise/oscillation levels.
- Evidence on whether self-oscillation persists behind a closed VCA and retains
  inter-note phase/history, compared with the model's continuously running
  six-card policy.
- Separate proposed models and regression tests for shared audible noise and
  microscopic startup excitation, with uncertainty or **protocol only** where
  hardware evidence is unavailable.

## OQ-17 — Main VOLUME tracking/loading and output-selector transfer

**Priority:** P3 overall; the High-reference/load slice is elevated only when
required by OQ-06

### Task definition

Recover the loaded static transfer and gang tracking of the one physical dual
VOLUME potentiometer, the
dual-gang High/Mid/Low attenuator network feeding the Mono/Stereo output-jack
paths, their jack normaling/summing behaviour, and the separate IC7 PHONES
path. Service Notes p. 1 publishes nominal output levels L -30 dBm, M -15 dBm
and H 0 dBm;
the schematic and parts list identify VR1 as a dual `10KB×2` VOLUME
potentiometer. Panasonic's later JIS/EIAJ table maps plain `B` to the nominal-
linear `1B` group: its stated midpoint window is 40–60% at 50% rotation, while
the table's separate S-shaped volume law is `3BM`. This
settles the nominal shaft-to-wiper law and replaces the former squared audio-
taper approximation. The marking does not establish real gang tracking,
tolerance, downstream loading, output impedance or the selector/jack transfer.
The schematic does establish one narrower transfer before the wipers: IC6 pin
1 crosses C17 10 µF then R54 1.5 kΩ into one 10 kΩ track, and IC6 pin 7 crosses
C20/R57 into the other identical track. With a high-impedance downstream load,
each channel is therefore an independent high-pass with a 115 ms time constant,
1.383956 Hz corner and `10/11.5 = 0.869565` (−1.213957 dB) high-frequency
gain at the unloaded/full-track boundary. The internal selector ladder is
`33k+6.8k+1.5k = 41.3 kΩ`; the headphone amplifier input is
`1k+100k = 101 kΩ`, giving a fixed per-wiper parallel load of 29.313 kΩ. For
shaft position `x`, the implemented internal transfer is
`Z=(10kx)||29.313k`, `Vw/VIC6=Z/[1.5k+10k(1−x)+Z]`, with the same resistance
setting the C17/C20 pole. It gives 0.39655 at half travel and 0.83252 at full,
or a normalized midpoint of 0.4763. Selected-tap loading, R64/R65 2.2 kΩ,
C21/C22 1 nF, jack normaling and external loads can move that result and remain
open.
The implementation applies this loaded nominal-linear track law, adds a 5 ms
anti-zipper glide, and does not expose the hardware selector. Its
declared fixed product-policy position is the pre-jack High-tap equivalent;
the physical selector attenuation, output impedance and loading are not
modelled. This remains distinct from installed stored-VCA tolerance (OQ-02),
IC6 clipping (OQ-05) and dBFS policy (OQ-06).

### Needed output (for LLM)

- A schematic/designator account that verifies the dual 10KB marking and the
  published H/M/L dBm levels, and establishes their reference/load conditions,
  attenuator values and source impedance. Treat the nominal JIS B law as
  settled; measure real tracking/tolerance rather than refitting an unsupported
  generic audio taper. Begin from the now-established C17/C20, R54/R57 and VR1
  network; do not rediscover or remove that stage.
- Extend the settled internal 29.313 kΩ per-wiper load analysis through the
  Mono/Stereo jack normaling and declared external loads. Report how those
  configurations move the implemented position-dependent corner and gain.
- A calibrated sweep from IC6/pre-volume level to the loaded Mono/Stereo output
  jacks at fine shaft/scale intervals for High, Mid and Low, with load, tone
  and frequency stated. Measure both one-plug and two-plug configurations and
  capture PHONES separately with its own declared load.
- A CSV of physical position, measured linear gain and dB for each selector
  state and output configuration, plus a separate headphone-path table, fitted
  law, residuals, channel tracking and loading dependence.
  Report a true zero endpoint as zero linear gain and −infinity dB, or censor it
  at a stated measurement noise floor; do not invent a finite dB value.
- A comparison of measured loaded gain with the settled nominal-linear/internal-
  load calculation, separating real track tolerance, gang mismatch, selector
  loading and external-load effects. The former squared curve, 5 ms automation
  glide and selector-exposure/session policy are implementation history or
  product guardrails, not hardware evidence targets.

## OQ-18 — Upper cutoff-converter saturation law

**Priority:** P2

### Task definition

Recover the upper-range relationship between converter code, control voltage
and filter cutoff. The exponential count law is anchored through the audio
range, and Service Notes p. 1 publishes an approximate 5 Hz-50 kHz range. A
range endpoint is not an exact hard limit or a saturation curve. The fallback
project describes a 4096-code table made from 93 points on one card plus
log-domain interpolation, but the complete raw capture, metadata and
multi-card scope are unavailable. Neither source establishes the former
24 kHz knee, tanh shape or 52.2 kHz asymptote. The adopted default product
policy is the validated exponential law followed by a transparent numerical
`min(..., 50000 Hz)` safety cap; this is not claimed as analogue saturation.

The former 24 kHz/tanh/52.2 kHz curve may exist only as a named legacy
compatibility profile. The hardware high-code law remains the research target.

**Updated 2026-08-06.** The knee is no longer a product cap alone. An AS3109
teardown reports the control current saturating internally at 700 µA, which on
this circuit's own 240 pF / 68 kΩ is a pole near 64 kHz, and the shipping law
now bends toward that asymptote with a single fitted exponent. The 50 kHz cap
remains, and now binds only at and above the top of the slider. What this task
still wants is a Roland-published or independently reproduced curve to replace
the one third-party table that exponent was fitted to.

### Needed output (for LLM)

- A dense sweep of top-range converter codes, converter/control voltage and
  resulting cutoff or self-oscillation frequency, with exact probe/method,
  temperature, card, trim and uncertainty.
- A method capable of identifying above-audio cutoff without mistaking
  analyser bandwidth, aliasing or the self-oscillation frequency trim for
  converter saturation.
- Multiple cards/units where feasible, keeping nominal central law separate
  from calibrated card residuals.
- Candidate knee/ceiling equations or a lookup table with residuals, compared
  separately with the transparent 50 kHz product cap, the uncapped exponential
  law and any named legacy 24 kHz/tanh/52.2 kHz profile.
- Explicit **not found** gaps, supported replacement constants and
  deterministic high-code fixtures.

## OQ-19 — Central voice-module BA662 gain, knee and deadband

**Priority:** P1

### Task definition

Characterise the static ENV/GATE-controlled BA662 VCA inside a voice module. A
photographed A1QH80017A teardown settles that it is the second BA662 beside the
IR3109/resonance devices, while the service schematic settles placement and
ENV/GATE ownership. Service Notes p. 19 adjusts each card to 6 Vpp during the
VCA GAIN procedure, but publishes no tolerance and does not identify the
control curve.

**Updated 2026-08-06.** The voiced curve this task was written against — a knee
at 0.12, a 260 dB-per-unit low-level slope and a hard deadband at 0.005 — is
gone. Roland's drawing narrows the topology but does not settle the exact law.
The BA662 is current-controlled; held VCA CV reaches pin 11 through
R106/C58/R105/grounded-base Tr20, and no intentional volts-per-decade converter
is drawn. That supports a quasi-linear compatibility approximation above
conduction. Tr20's installed onset, the resistor-plus-$V_{BE}(I)$ relation and
the BA662's own low-current $g_m(I)$ still require measurement; a 150 mV onset
from a reconstruction and an ideal-BJT thermal softplus are not hardware data.

The same pages settle a separate thump mechanism. VCF OUT pin 3 is AC-coupled
by C59 into VCA IN pin 9; VR30 reaches that signal node through R112 2.2 MΩ,
and Roland adjusts the six corresponding trimmers at TP8–TP13 for minimum
thumps. That anchors existence and a null procedure, not the post-calibration
residual. The removed renderer term reused a control-hold spread as this
signal-input trim, added a common +0.8 mV bias and then multiplied by control
and VCA gain, producing unsupported control². The nominal model now adds no
residual until the measurement below exists. This is not the downstream shared
stored-VCA validation in OQ-02; population spread remains OQ-10.

### Needed output (for LLM)

- A dense simultaneous sweep of control-node voltage, BA662 control current if
  safely accessible, VCA input and VCA output on a calibrated voice card, with
  a low-distortion tone, fixed load, settled holds and stated noise floor.
- Linear gain and dB versus physical control voltage and corresponding digital
  envelope/GATE value, with special density around conduction onset and the
  lower 15% of travel.
- Objective definitions and confidence bounds for deadband, knee location,
  knee slope, central linearity and maximum gain; distinguish true cutoff from
  the measurement noise floor or a defective/leaky module.
- Measurements on additional cards sufficient to separate a nominal curve from
  OQ-10 dispersion.
- A fitted law/table with residuals against the current compatibility profile
  — provisional 150 mV onset, ideal-BJT knee and quasi-linear upper region —
  plus alternatives justified by the measured data and deterministic
  boundary/interior fixtures. Keep this analogue transfer replaceable without
  changing OQ-12 envelope states or patch bytes.
- Report the measured 6 Vpp service endpoint separately, without inventing a
  tolerance or using it to infer the knee/deadband law.
- Recreate the Bank-1 offset procedure with all intentional sources silent.
  Capture each TP8–TP13 output DC-coupled before null, at the best VR30 null and
  after a documented small trim offset. Record trimmer position, load, bandwidth,
  noise floor, temperature and calibration state.
- Apply gate/control steps 0→1→0, 0→0.5→0 and 0→0.1→0, plus several attack
  slopes. Report signed peak, pulse area, 20–200 Hz energy, polarity and decay.
  Separate a constant pin-9 residual multiplied once by gain from any derivative-
  like control coupling; measure multiple cards and retain raw captures.
- Capture both TP8–TP13 before C14 and the final output after C14/selected HPF.
  Do not infer the source residual from the final pulse alone, and do not assign
  per-voice ENV/GATE thump to the later shared uPC1252H2.

## OQ-20 — Chorus wet-mute switching transient and leakage

**Priority:** P2

### Task definition

Recover the analogue enable/disable behaviour of the chorus wet-return mute
transistors. It is settled that Off mutes only wet and leaves the oscillator and
BBDs running. Service Notes p. 15 identifies the wet-return devices as
TR11/TR12 (2SK30A/K381) immediately before R71/R73, not the full-output shunts
TR7/TR8. The implementation's 5 ms value is an exponential time constant
(`10–90%` about 10.99 ms), an undocumented anti-click choice rather than a
hardware transition time. Measure the real switching envelope, leakage and
click while preserving continuous BBD phase/state. Any account assigning the
wet-return mute to TR7/TR8 conflicts with the printed p. 15 schematic and must
be rejected unless it identifies a documented board revision: TR7/TR8 are the
later full-output shunts.

### Needed output (for LLM)

- A designator-level TR11/TR12, D4/D5 and control-network path with
  measured/static and dynamic JFET on-resistance, component-derived
  turn-on/turn-off constants, logic levels and expected leakage. Quantify how
  on-resistance moves the nominal C28/C25 connected corner; explicitly
  distinguish the later TR7/TR8 output mutes.
- Simultaneous control-node, wet-return and final-output captures for Off→I/II
  and I/II→Off at several signal levels, BBD phases and waveform polarities on
  both channels.
- Separate turn-on and turn-off gain envelopes, delay, asymmetry, feedthrough,
  residual wet attenuation and click spectrum with uncertainty.
- A comparison with an instantaneous switch and the current 5 ms glide,
  including audibility and worst-case transient tests.
- Either measured replacement laws/tests or an explicit decision to retain
  5 ms as a labelled plug-in anti-click policy rather than a hardware claim.

## OQ-21 — Coupled C14 and switched high-pass transfer

**Priority:** P2

### Task definition

Resolve the complete linear and switching behavior from IC1a's voice-summer
output through C14/R39 and the IC3-selected HPF network. Placement and populated
parts are settled. The implementation currently gives C14 one state whose
sub-hertz pole uses the selected leg's asymptotic load, then runs an independent
Boost/Flat/Cut-I/Cut-II filter. This captures the correct endpoints without
pretending C14 is absent, but it is not the full coupled network: the Cut
capacitors begin loading C14 as frequency rises, the Boost leg is multi-pole,
and deselected capacitors plus CMOS-switch parasitics may retain charge across
mode changes.

### Needed output (for LLM)

- A designator-complete netlist from IC1a through C14, R39, IC3 and every HPF
  leg, including source/load impedance, CMOS-switch on-resistance, off leakage
  and capacitance, component tolerances and all selected/deselected states.
- A symbolic or numerical modified-nodal/state-space solution for every switch
  position, with magnitude, phase and group delay from below the C14 pole
  through the audio band. Separate exact component results from measured or
  assumed device parasitics.
- A direct comparison with the implemented cascaded approximation at minimum:
  DC/asymptotic gain, 0.1/0.5/1/10/59.4/225.8/720.5 Hz, 1/10/20 kHz, and each
  mode's maximum magnitude/phase error.
- Mode-change captures or a validated transient simulation for all directed
  switch pairs under silence, a centred sine and asymmetric PWM. Report charge
  memory, click amplitude/spectrum, settling and dependence on initial state.
- A minimal replacement topology with explicit state-migration/reset semantics
  and deterministic steady-state/transient fixtures, or a quantified argument
  that the present approximation is below the declared audibility/error bound.

## Evidence search — 2026-08-06

**Work mode:** evidence search (public sources) plus analysis of the shipping
implementation. **No hardware was measured for this pass.** Every dB figure
attributed to the model below was produced by an offline replica of the shipping
algorithms — the exact residual tables, the exact `OtaCascade::process`, the exact
half-band kernel — and is reproducible from the source alone. Nothing here is
promoted to **anchored** unless it comes from Roland service documentation or a
component datasheet.

### New primary-source material

The Roland JUNO-106 Service Notes **specification page** and **ADJUSTMENT section**
were read in full. Both are primary Roland documentation and therefore **anchored**.

The specification page corroborates a large block of already-implemented constants,
and no change follows from it — it is recorded so a later pass does not re-derive
them: `VCF CUTOFF FREQ. 5Hz to 50kHz`; `VCF ENV MOD. ±14 octaves` (model 16255/1143
= 14.22); `VCF LFO MOD. ±3.5 octaves` (4047/1143 = 3.54); `VCF BENDER ±3.5 octaves`
(4064/1143 = 3.56); `DCO LFO MOD. ±400 cents`; `DCO BENDER ±1200 cents`;
`LFO RATE 0.1Hz to 30Hz`.

Two specification entries are **new evidence against open tasks**:

- `AUDIO OUTPUT   L: −30dBm;  M: −15dBm;  H: 0dBm`. This is a Roland-published
  nominal output level for each selector position and bears directly on **OQ-06**,
  which currently treats the absolute output reference as product policy. Roland
  gives no reference impedance, so it does not by itself fix `Vref_rms`; it does
  fix the selector's intended 15 dB steps, which is a testable constraint on the
  41.3 kΩ ladder model. Status: **partially resolved**, confidence moderate.
- `ENV DECAY 1.5ms to 12s` / `RELEASE 1.5ms to 12s` against the model's
  firmware-derived 16.8 ms – 25.55 s. The top differs by roughly 2×. The model
  measures to digital zero while Roland's figure is almost certainly to an
  unstated threshold, so this is probably not a contradiction — but per contract
  rule 5 it is recorded as an unreconciled discrepancy rather than explained away.
  Relevant to **OQ-12**. Status: **not resolved**.

The ADJUSTMENT section yields a complete calibration corpus. Most is already
recorded; two items are usable and currently unasserted by the suite:

| Step | Test point | Target | Note |
|---|---|---|---|
| VCF FREQUENCY | BANK 3, hold C4 | 248 Hz (B3) | already the model's anchor |
| **VCF WIDTH** | **BANK 3, hold C6** | **992 Hz (B5)** | **a second cutoff anchor two octaves up — independently constrains the counts-per-octave slope** |
| **VCF RESONANCE** | TP19…TP14, BANK 3, hold C4 | **4.8 Vp-p sine** | recorded in the project, but self-oscillation *amplitude* is asserted nowhere; the suite checks frequency only |
| VCA GAIN | TP8…TP13, BANK 3, hold C4 | 6 Vp-p | already recorded |
| NOISE LEVEL | TP8, BANK 6 | 4 Vp-p | already recorded, and correctly noted as downstream of the pre-filter coordinate OQ-15 asks about |
| PWM | BANK 5, hold C4 | 50 %, tol. 48–52 %; at PWM 10, 93–97 % | already recorded |
| VCA BIAS | TP7 | +0.25 … +0.27 V | not currently used |
| DCO CV OFFSET | TP3 | 0 V | not currently used |

The D/A & S/H timing chart also gives converter output ranges: **DCO CV / SUB
LEVEL 0 to −10 V; VCF CV / VCA LEVEL / PWM CV +4 to −6 V; VCA CV / RESO CV /
NOISE LEVEL 0 to +10 V**, refresh 4.2 ms. The VCA LEVEL range is now consumed by
the nominal OQ-02 derivation; the remaining abstract destinations belong to
**OQ-07**.

### OQ-01 — the missing integrator capacitor is printed

`YouKnow106Chorus.h:141-147` states that every term of
`f = 1/(4·β·R_eff·C3)` is known except C3, "which the schematic does not print".

A 300 dpi render of Service Notes p. 15 (JACK BOARD) is reported to show
**C3 = 0.1 µF**, printed in Roland's usual bare ".1" form beside C4's "220P".
**This reading needs independent confirmation against the project's own copy of
p. 15 before use** — it was not verified by the author of this section directly.

Independently of that reading, an arithmetic check falsifies a second term.
Evaluating `f = 1/(4·β·R_eff·C3)` with the model's own `lfoTimingOhms()`:

| β | Mode I | Mode II | vs the third-party measured 0.537 / 0.879 Hz |
|---|---|---|---|
| `1/48` — `lfoThresholdRatio` as implemented | 18.65 Hz | 30.27 Hz | +3372 % / +3344 % |
| `R7/R6 = 33/47 = 0.702` | 0.5533 Hz | 0.8983 Hz | +3.0 % / +2.2 % |

So `lfoThresholdRatio = 1.0/48.0` is inconsistent with the circuit by roughly 34×.
It causes no audible defect today because the absolute rate is supplied by the
JUNO-60 fallback rather than computed from β — but it is the reason the derivation
was believed to be blocked. Reaching the measured rates with C3 = 0.1 µF requires
β ≈ 0.72, which is either `R7/R6` or an R15 nearer 120 kΩ than the reported 1 kΩ.
**Which of the two is correct is a schematic-reading task, not a measurement.**

The mode ratio is unaffected and comes out at **1.6235** either way — identical to
the project's own `modeRateRatio()` of 1.6234799, and within **0.82 %** of the
third-party measured ratio. The ratio was always right; only the scale was borrowed.

Status: **partially resolved.** Confidence: the β inconsistency is high (pure
arithmetic against the project's own constants); the C3 value is moderate pending
confirmation. Remaining gap: confirm C3 and the β network on p. 15; then the
absolute scale is **derived**, not borrowed.

### OQ-01 — third-party JUNO-106 measurements (not anchored)

These are uncalibrated third-party measurements. They are **not** promoted to
anchored and must not be treated as fixtures without confirmation.

| Quantity | Model today (JUNO-60 fallback) | Reported for a JUNO-106 |
|---|---|---|
| LFO rate I / II | 0.5222 / 0.8478 Hz | 0.537 / 0.879 Hz (scope, on a *clone* of the 106 chorus) |
| Delay sweep | 1.66–5.35 ms | 1.4–6.4 ms |
| Centre delay | 3.505 ms | 3.9 ms |
| Excursion | ±1.845 ms | ±2.5 ms — **the model is at 74 % of it** |
| BBD clock | 23.9–77.1 kHz | 20.0–91.4 kHz |

The delay-sweep figure comes from an owner comparing a real 106 against a clone
after discovering the clone's "MN3009s" were re-badged MN3007s; it is bracketed by
three independent clock readings at BBD pins on 106-chorus clones (25–65, 35–85 and
28–60 kHz). Provenance is forum posts with scope screenshots, several now dead —
below this project's anchoring bar, but mutually consistent and consistent with the
schematic derivation above to within a few percent.

Measurement trap worth recording: the MN3101 datasheet states *"Clock signal
frequency is 1/2 of oscillation frequency"*. A probe on the OSC pins reads 2× the
BBD clock; only CP1/CP2 read it directly. Several published readings are ambiguous
on this point.

Provenance of the current fallback, for the record: the 23.9–77.1 kHz / 1.66–5.35 ms
constants match `pendragon-andyh/Juno60`, whose README gives 0.513/0.863 Hz and
0.00166/0.00535 s — measured, but on a **JUNO-60**, by inspection in Sonic
Visualiser.

Status: **not resolved** (evidence below the anchoring bar). Confidence: moderate
that the model's sweep depth is materially shallow; the ±26 % excursion shortfall is
the largest single audible number found in this pass.

### OQ-03 — a robust chorus-noise delta

A third-party measurement of a real JUNO-106 (48 kHz/24-bit, VOLUME 10, OUTPUT
HIGH, +25 dB into a Fireface 800, original Panasonic MN3009s) reports peak noise of
**−47.97 dBFS for Chorus I and −44.01 dBFS for Chorus II**.

The absolute figures are not usable — the chain gain is stated but the reference is
the converter's dBFS, not dBu. The **difference is usable**, because the chain gain
cancels: **Chorus II is 3.95 dB noisier than Chorus I**. The model uses one noise
amplitude for both modes and does not reproduce this. It is physically plausible —
mode II's sweep spends more time at low clock rates, where BBD noise is worse.

Status: **partially resolved.** Confidence moderate for the delta, low for the
absolute level. Remaining gap: calibrated PSD, stereo correlation and spurs.

The BBD host-grid polyBLEP does not change that status. It reconstructs only the
deterministic held-output step: held noise and the RNG sequence remain unchanged,
and stochastic BBD noise is not predicted or corrected. The 2025 BLEP paper uses
an ideal noiseless BBD and supplies no evidence for extending its method to this
noise model. Any remaining simulation-grid content from noise belongs to OQ-03's
future calibrated stochastic model, not to an invented look-ahead sequence.

An offline counterfactual did copy the RNG state and polyBLEP its future noise
jumps without mutating the production sequence. With Chorus Noise 1 and silent
input, 20 Hz–20 kHz RMS changed by only **−0.05/−0.06 dB in HQ** at 48/44.1 kHz
hosts, with aligned differences around −82/−81 dBFS. LQ changed **−0.83/−1.04
dB**. Its 15–20 kHz noise fell 3.29/4.07 dB, but was already below −96 dBFS.
That is too little default-path benefit to assign every provisional hiss source
to clock-held charge noise without measurement. Preserve this rejected result;
do not repeat it as a generic “cleaner” improvement before the capture above
separates held and continuous noise components.

### OQ-04 — the MN3009's own bandwidth bounds the support chain

Panasonic MN3009 datasheet, **anchored**: input signal frequency is `−3 dB at
12 kHz` minimum (0 dB reference at 1 kHz) at a 40 kHz clock; this is the sole
numeric transfer anchor adopted by the model. Its typical `Gi-fi` plot explicitly
shows distinct fCP 10, 40 and 100 kHz curves. By visual inspection their
normalized corner does not move upward at faster clocks, so they do not
qualitatively support the removed faster-clock brightening. The scan is too
coarse to promote eyeballed points to calibration data without digitization and
uncertainty, but it is genuine qualitative multi-clock evidence. Insertion loss is `0 dB typ.`
(−4 / +4 dB); clock
range 10–200 kHz; `S/N 88 dB typ.`; noise `0.2 mVrms max` A-weighted at 100 kHz
clock. The THD anchors the model already fits are confirmed verbatim: `0.3 %
typ. at V_i = 0.78 Vrms`, `2.5 %` at the 1.5 Vrms maximum input.

A numerical audit found that `transferLossStep` advances once per modeled BBD
shift (one fCP period).
Fixed alpha 0.8654743 therefore already scales the absolute pole with clock and,
with the explicit zero-order hold, produces −3.000 dB versus DC and −2.972 dB
versus the datasheet's 1 kHz reference at 40 kHz/12 kHz at the raw node upstream
of numerical reconstruction. The extra affine clock multiplier instead produced
−2.757 dB versus DC and −2.732 dB versus 1 kHz there
and changed the normalized 0.3-cycle response from about −3.04 to −2.14 dB
over the modelled 23.9–77.1 kHz sweep. It is removed as double scaling. This
settles the renderer's internally consistent one-anchor law, **not** the real
part's normalized response versus clock; that still requires quantitative
extraction of the typical curves and preferably the multi-clock de-embedded
installed-unit sweep specified above.

The upstream raw-hold/support-chain calculation evaluates to **−12.0 dB at
10 kHz and −38.5 dB at 15 kHz** relative to 2 kHz (input Sallen-Key pair +
7.2 kHz passive + tap pole + output Sallen-Key pair + ZOH aperture at the
3.505 ms centre clock). It describes the physical-model baseline before the
polyBLEP output reconstruction. The part alone is roughly 3 dB down over that
range, so the modelled darkening is dominated by the support chain, not the BBD.

Removing the duplicated output Sallen-Key pair does not reconcile it either
(−9.0 dB at 10 kHz), so the corner *values* are implicated, not only the pole count.
Note `YouKnow106Chorus.cpp:334-339` re-derives `reconstructionFirst/Second` from the
same constants as `antiAliasFirst/Second` rather than reading the reconstruction
side from the schematic separately. (Narrowed 2026-08-06: the sibling clone
netlist reads the output sections at the same part values as the input
sections — see the module netlist-corroboration pass below — so the shared
constants are now family-corroborated; the 106's own p. 15 codes stay
unread and the corner values stay implicated.)

This is coupled to OQ-01: the present chain is a reasonable anti-alias design for
the modelled 23.9 kHz minimum clock (Nyquist 12 kHz, chain −21.8 dB there) and
over-engineered for a ~43 kHz clock. **The wet-path bandwidth is a measurable proxy
that constrains the BBD clock, so OQ-01 and OQ-04 should be resolved together.**

Status: **physical loaded transfer not resolved**, but the renderer's duplicate
clock scaling is removed and the broader contradiction is quantified.
Recommended next step is digitizing the datasheet's three curves with uncertainty,
then checking them with a multi-clock de-embedded MN3009/installed-unit sweep,
plus a schematic re-read of the capacitor codes behind
`YouKnow106Chorus.cpp:73-83` — a single 10× code misread moves a corner by a decade.

### Deterministic BBD host-grid aliasing — implementation resolved

Gabrielli, D'Angelo and Squartini distinguish wanted BBD-generated aliasing
(BGA) at `k·Fclock ± f` from the simulation-generated aliasing (SGA) introduced
when asynchronous held-output steps meet the fixed sample grid. The engine now
uses a paper-motivated, deterministic-only polyBLEP after transfer loss and
before the tap-summing pole. Its fixed scheduler has 54 slots and uses 50 in the
tested 200 kHz-clock/8 kHz-grid worst case; the correction handles multiple
edges per internal sample. Buckets, index, BBD phase, transfer and held-noise
state, and RNG sequence remain unchanged. Only the grid-specific correction
slots clear on an internal-rate change. Noise remains uncorrected.

The isolated core reduces SGA by **36.2873 dB at 50 kHz** and **42.6752 dB in
the 90 kHz multi-edge case**; at 10 kHz the improvement is **25.0819 dB** after
excluding physical-image bins. Full-line BGA deltas at 44.1 kHz LQ are
**−0.3405, −0.6039, −4.5851 and −5.9408 dB** at 9.216, 10.784, 19.216 and
20.784 kHz; at the default 176.4 kHz HQ rate they are **−0.0016, −0.0030,
−0.0281 and −0.0382 dB**. This qualifies strong SGA suppression and BGA
preservation that is near-transparent at HQ, not exact image preservation at LQ.

The strict [BBD host-grid alias comparison](audio/realism-comparisons/bbd-host-grid-alias/README.md)
uses a 2.093 kHz probe at minimum clock. The wanted image moves **−0.0383 dB in
HQ**; its **−5.2986 dB LQ** movement starts from **−100.47 dBc**. The two false
LQ second-image folds improve from **−26.87/−27.42 to −55.23/−53.61 dBc**,
while the roughly **−116 dBc** HQ folds move to about **−171/−170 dBc**. The
whole comparison's signed difference is **−15.95 dBc peak and −27.66 dBc RMS**
at one fixed gain. These are deterministic software measurements and listening
files, not a subjective test or a hardware measurement.

The paper's experiment uses an ideal, linear, noiseless 4096-stage MN3005 at
44.1 kHz; this engine models a nonlinear, noisy 256-stage MN3009 at several
internal rates. Its published SNR numbers are therefore not reused as product
claims. The peer-reviewed result supports the BGA/SGA classification and method
family; the exact bounded scheduler and the figures above are engine validation.
This deterministic numerical issue is resolved, but it does **not** close
OQ-03's stochastic model or OQ-04's physical loaded transfer and BGA response.
A noise-on fixed/swept HQ/LQ PSD test and audition section are still required
before extending the same reconstruction claim to the default hiss path.

### OQ-02 — the nominal common-VCA law is circuit-derived

The first pass correctly identified that NEC specifies **−5.9 mV/dB typical**
(5.8–6.1 mV/dB magnitude) and that the former `patchLevelGain=−15+20·p³`
could not follow from a linear passive divider. Its claim that the divider was
still unread is now superseded by a direct Service Notes p. 15 read.

The complete nominal chain is:

1. Physical code `d=b<<5` from the hash-identified B-2 firmware.
2. Page 8's +4 to −6 V range, interpreted with an explicitly assumed ideal
   12-bit R-2R transfer: `Vhold=4−10d/4096`.
3. Page 15's R30 2.2 kΩ into the C7 10 µF NP hold node, R32 1.5 kΩ onward
   to GC1, R31 47 Ω to ground and R165 15 kΩ to +15 V.
4. The loaded solve
   `Vgc=0.01250467817·Vhold+0.04626730922`, followed by NEC's
   `gain_dB=Vgc/−0.0059`.

Therefore `gain_dB=−16.3196647+0.165581014·b`, linear in dB across stored
bytes 0…127. C7 sees
`R30||(R32+(R31||R165))=908.249 Ω`, giving `τ=9.08249 ms` and
`fc=17.523 Hz`. This replaces both the former cubic curve and its borrowed hold
constant. R32 is the least-legible value in the scan, `/4096` is an ideal-DAC
assumption, and physical component, rail and IC spread remain for an installed
sweep under the narrowed OQ-02.

Status: **nominal law resolved by primary schematic/datasheet derivation;
installed tolerance not measured**. The strict comparison fixture records the
audible implementation delta separately from any hardware claim.

### OQ-18 — a measured code-to-frequency curve exists

A third-party open-source project publishes a measured cutoff-vs-DAC-code table for
a real JUNO-106 voice card, gain-calibrated so DAC 1568 reads 248.000 Hz — the same
service anchor this project uses (6272 counts = 1568 DAC codes). Provenance
indicators are strong: the anchor matches; the companion source is a clean-room
reimplementation of the **D7811G** firmware's cutoff routine, and the µPD7811 is the
106's CPU; and the documented bit-boundary steps fall exactly where R-2R
major-carry non-linearity physically belongs. It is nonetheless **third-party and
not independently verified**, and the project is **GPL-3.0** while this one is MIT —
measurements are facts and may be cited like a datasheet, but no code may be copied
and any fit must be re-derived.

Comparison against the shipping path at Unit Character 1.0, resonance 0:

| DAC | Measured | Model | Error |
|---|---|---|---|
| 1024 | 67.2 Hz | 66.3 Hz | −25 ¢ |
| 1568 | 248.0 Hz | 247.6 Hz | −3 ¢ |
| 2560 | 2 725 Hz | 2 690 Hz | −23 ¢ |
| 2816 | 5 048 Hz | 4 911 Hz | −48 ¢ |
| 3072 | 9 297 Hz | 8 827 Hz | −90 ¢ |
| **3328** | **16 779 Hz** | **15 447 Hz** | **−143 ¢** |
| 3584 | 27 876 Hz | 25 876 Hz | −129 ¢ |
| 4064 | 50 792 Hz | 50 000 Hz | −27 ¢ |

Findings:

- The **exponential law is confirmed**: measured slope 3.46–3.49 octaves per 1000
  DAC codes against the model's constant 3.500, so `vcfCountsPerOctave = 1143`
  is sound and the 248 Hz anchor agrees to 3 cents.
- The **50 kHz endpoint is confirmed** (measured 50.8 kHz at the top of the slider),
  and independently by the Roland spec page's `5Hz to 50kHz`.
- What is wrong is the **knee shape between them**. The single-pole `R_e`
  compression `rawHz/(1 + calibration·rawHz/120000)` over-corrects, leaving the
  model up to **143 cents flat around a 16 kHz cutoff** and 48–90 cents flat from
  5–9 kHz, which is inside the musical range. At Unit Character 0 the uncompressed
  law is instead **+292 ¢ sharp** at DAC 3584. A single pole cannot describe the
  measured knee.

Status: **partially resolved.** A defensible replacement is a fit to the measured
curve with the base and exponent re-fitted together; note the model's
`vcfBaseFrequencyHz = 5.53` sits 63 cents below the measured 5.73 Hz at code 0.

*Acted on the same day; see the implementation pass below.* The base and slope
were deliberately **not** refitted — they are pinned by Roland's own 248 Hz
calibration anchor, and the 63-cent base gap and the measured 3.46–3.49 against
the model's 3.50 octaves per 1000 codes are the same statement seen from two
ends. Only the knee moved.

### OQ-07 / OQ-18 — the R-2R carry non-linearity is real and was removed

The same measured table documents excess steps at the bit boundaries of
**−4.64 ¢ at code 1024, +23.31 ¢ at code 2048, −4.48 ¢ at code 3072**.

This project added an R-2R major-carry model and then removed it, correctly: the
implementation wrote a transient impulse into `voice.cutoffCountsTarget`, a field
the same converter write immediately reassigned, so it measured −360 dBc and was
bit-identical. **The mechanism is real; only its placement was wrong.** It belongs
in the static code-to-frequency map as an INL offset, not as an impulse — at which
point a slow cutoff sweep steps by roughly 23 cents crossing mid-scale, which is
audible on a slow resonant sweep. Folding it into the OQ-18 fit costs nothing extra,
since the measured curve already contains it.

Status: **partially resolved**, confidence moderate. *Implemented the same day,
as a persistent offset applied by the converter write rather than folded into
the static map: the map is a function of counts and never sees the code
boundary, and putting the offset on the write is what lets the hold capacitor
slew it as the hardware's would. See the implementation pass below.* *Implemented the same day
as a persistent offset applied by the converter write, not folded into the
static map: the map does not see the code, and putting it on the write is what
lets the hold capacitor slew it as the hardware's would. See the implementation
pass below.*

### OQ-10 — a comparison point for per-card dispersion

The same third-party project ships **±10 DAC counts (≈ ±10.5 cents)** as its
per-voice VCF cutoff spread and ±2.4 % for VCA gain. That is a chosen default rather
than a published measurement, but it was chosen by an author holding six-card data,
which makes it a meaningful prior.

For comparison, this model's `thermalCutoffSpread` alone contributes
`1 + 0.04·calibration·(cardIndex − 2.5)`, i.e. **±165 cents** at the shipping
default, applied *in addition to* `cutoffOffsetError` (±84 cents) and
`cutoffScaleError` (±5 %). Three internal observations, independent of the external
comparison:

1. It is roughly 10× what the model's own `dynamicThermalVoltage` computation
   supports — a 4 °C card-to-card gradient moves `V_t` by about 1.3 %.
2. Its shape is linear in `cardIndex` while the temperature profile computed
   alongside it is `exp(−cardIndex/2.5)`; the two disagree.
3. It is absent from the README's Unit Character table, which lists only
   "VCF cutoff scale trim up to ±5%".

Status: **not resolved** (OQ-10 still needs real population data), but the present
value is internally inconsistent and is recorded here as such.

### Model-internal measurements — no hardware evidence required

These characterise the implementation, not the instrument, and are fully
reproducible from source. They are recorded because two of them bear on audibility
claims the suite cannot currently see.

- **In-band alias floor** (≤20 kHz, re the loudest harmonic, C6 saw): oscillator
  alone **−111.5 dB**; with the VCF wide open at resonance 0 **−85 dB**; with cutoff
  16 kHz and resonance ≈0.85 **−55.5 dB**; the same case with the filter run at 2×
  the internal rate **−87.7 dB**. The dominant in-band artefact is the VCF's `tanh`
  set, not the oscillator. `testAliasFloor` cannot observe this: it runs at
  resonance 0, calibration 0, and stops sweeping at 20 kHz.
- **Final half-band decimator at a 44.1 kHz host**: −0.85 dB at 20 kHz, −2.5 dB at
  21 kHz, with fold-back rejection of only −31.7 dB for content landing at 19.1 kHz.
  Both stages share the 63-tap kernel; the first stage's transition band is wide
  enough that only the last stage matters.
- **Measured as *not* audible**, recorded so the work is not repeated: widening the
  BLEP kernel or interpolating its table more finely (already −111 dB); `double`
  state in `OtaCascade` (bit-indistinguishable from `float` at 40 Hz / 200 Hz /
  1 kHz cutoff, resonance 3.9); more Newton iterations (the 8-iteration cap is
  reached 10–22 % of the time under hot resonant input, but the resulting error is
  −103 dBc); antiderivative antialiasing on `outputSummerClip` (no measurable
  aliasing at the rail, +3 dB or +6 dB in); and an exponential rather than linear
  DCO ramp reset (2.2 µs against a 2273 µs period at A440 — the two shapes differ
  only above ~72 kHz).

### OQ-19 — the voice VCA topology narrows the law; measurement still chooses it

The BA662 is current-controlled, and Roland draws no intentional volts-per-decade
converter in the VCA path. The external control-current branch is:

```
VCA CV (0…+10 V, from the S/H) → R106 10 kΩ → C58 0.1 µF node
   → R105 22 kΩ → Tr20 emitter
   [Tr20 base grounded; collector = pin 11 VCA CONT]
```

The independent signal/null path is:

```
module pin 3 VCF OUT → C59 1 µF/50 V NP → VR27/R108 network
   → pin 9 VCA IN
VR30 100 kΩ wiper → R112 2.2 MΩ → the same pin-9 input node
pin 10 VCA OUT → TP8 (TP9…TP13 on cards 2…6) → 33 kΩ voice summer
```

The control path supports quasi-linear gain above conduction, but
`I=(Vcv−VBE)/32 kΩ` is only an idealized approximation because $V_{BE}$ depends
on current; the actual inverse is not an exact softplus, and the BA662's own
gain near cutoff is not specified here. A reconstruction's 150 mV onset and the
model's ideal-BJT knee are compatibility values. Separately, Service Notes p. 18
adjusts VR30/25/20/15/10/5 for minimum thumps at TP8–TP13. This anchors the real
null topology while leaving its calibrated residual unmeasured.

The shipping replacement is a narrower, smooth compatibility profile. Its
topology is better motivated than the former wide curve, but its onset, knee and
deadband are not promoted to measurements. Unit Character's deterministic
control-hold offset is also explicitly distinct from the VR30 signal-input null.

Contradiction recorded rather than reconciled: a published teardown asserts *"the
envelope generators are linear and generated by the CPU, so the VCA response must be
exponential."* That is an inference, and the schematic contradicts it. Whether the
firmware pre-shapes the envelope DAC data is a separate, unresolved question.

Status: **partially resolved**, confidence high on the drawn topology and null
procedure, low on the unmeasured numerical transfer and post-null residual.

### OQ-09 — resonance compensation confirmed from Roland's own drawing

The resonance BA662's **two signal inputs are fed from two separate dividers — one
from VCF IN, one from VCF OUT** — with its output current injected into the input
chain. Raising resonance therefore raises the feedback *and* the input drive
together. Independent circuit commentary on the Juno-6, which is the same circuit
un-potted, names these on Roland's schematic as **"Resonance"** and **"Resonance
compensation"**, and describes it as *"the classic way of increasing the input level
to the filter to compensate"*, noting the 106 uses the same scheme inside the module.

**This settles the direction of `inputCompensation()` in the model's favour.** The
mechanism is real, and any future pass proposing to remove it should be refused. What
remains open is only its magnitude. A reverse-engineered clone puts the compensation
divider at ÷17.0 against the feedback divider at ÷67.7, i.e. compensation about **4×
stronger per unit of resonance CV**; those specific values are one person's
reconstruction, not Roland's, and are not promoted.

The limiting mechanism is **OTA saturation itself**, not clipping diodes — the SH-101
uses a phase-splitter with diodes to ground and must not be ported to a JUNO model.

Status: **partially resolved.** Still not found: any measured frequency-response
family versus resonance, and any self-oscillation-amplitude-versus-cutoff data.

### OQ-15 — a derivable filter-drive budget

The signal path reaches the module with **no series attenuator**: the mixer node
(`R102 33 kΩ` load) couples through `C56 10 µF NP` straight to pin 1 VCF IN.

Working back from the calibration table gives the first defensible drive figure. The
VCA GAIN trim sets 4.8 Vp-p at the VCF output against 6 Vp-p at the VCA output, a
gain of 1.25; the noise step then calibrates 4 Vp-p at TP8 with the filter wide open,
so **noise at VCF IN ≈ 4/6 × 4.8 ≈ 3.2 Vp-p**.

The consequent OTA drive, using the confirmed `560/(68000+560)` internal divider:
4.8 Vp-p at the output is ±2.4 V, so each differential pair sees **±19.6 mV, i.e.
13.9 mV rms**. The AS662D distortion reference is 0.25 % THD at 5 mV rms, so the
JUNO runs its pairs at about 2.8× that level — roughly **1–3 % THD per stage at
self-oscillation**. The pairs sit right at the edge of their linear region rather
than deep into saturation, which is the instrument's distortion character.

Status: **partially resolved.** Still not found: any measured DCO, sub or pulse
amplitude at pin 1 on a real unit — which is the remaining half of OQ-15.

### OQ-18 — the upper knee has a physical cause

The AS3109 datasheet (Alfa's IR3109 clone, whose own test condition is
`C = 240 pF, R = 68 kΩ` — this circuit) gives a **250 Hz pole at V_C = 0** and an
expo scale of **−19 mV/octave typ** (−17.5 … −20.5); a measured teardown of a real
IR3109 reports −290 dB/V, i.e. **20.8 mV/octave**, bracketing it.

Critically, the same teardown reports the control current **saturates internally at
700 µA**. That corresponds to a pole near **64 kHz** — which is the physical origin
of the upper knee OQ-18 asks about, and it is consistent with Roland's published
50 kHz top. The knee is OTA current saturation, not an arbitrary cap. A replacement
for the single-pole `R_e` compression should be shaped by that saturation.

Also confirmed: **the 560 Ω is internal to the IR3109**, so the model's
`stageAttenuation = 560/(68000+560)` is the right form. And the C4→248 Hz / C6→992 Hz
calibration pair is exactly two octaves of cutoff for two octaves of keyboard, i.e.
the WIDTH trim sets key tracking to exactly 1.00.

### OQ-10 — the cutoff CV path is temperature-compensated by design

The module board's VCF CV chain is
`R113 10 kΩ → VR28 5 kΩ (WIDTH) → R110 8.2 kΩ → pin 6`, with **R111 a 560 Ω
positor** — a PTC thermistor, listed as such in the module-board parts legend —
returning the node to ground, and VR29 100 kΩ (FREQ) injecting an offset through
R109 680 kΩ. The AS3109's stated tempco is 0.33 %/°C, and the positor exists
specifically to cancel it.

That is a second, independent argument against the size of the model's
`thermalCutoffSpread`: the hardware carries a dedicated compensating component in
exactly this path, on top of the per-card FREQ and WIDTH trimmers. The divider also
checks out: 560/(10 k + 0…5 k + 8.2 k + 560) ≈ 1/38, turning a 10 V DAC span into
≈266 mV at pin 6, which at 19 mV/octave is ≈14 octaves — matching the published
13.3-octave range.

### Explicit negatives — searched for, does not exist publicly

Recorded so a later pass does not repeat the search: no published THD figure, SNR,
dry noise floor, output level in Vrms/dBu, output impedance, headphone
specification, frequency-response plot, filter-sweep spectra of a real 80017A,
envelope captures at the VCA output, or null test against any emulation for a real
JUNO-106. No DAFx or AES paper uses the JUNO-106 as a measured reference. No
cents-scale chorus pitch-deviation measurement for a healthy unit exists. Roland's
ACB material contains no numbers.

Two leads remain unread: `analoguerenaissance.com/JUNOTEST/` (a JUNO-106 VCF/VCA and
wave-generator test procedure, typically carrying scope photographs — its TLS
certificate has expired, so a browser that accepts the warning is needed), and the
Gearspace chorus-noise thread's attached spectra and WAV files.

### Where the blocking reads actually live

This section originally identified OQ-01, OQ-02 and OQ-04 as three unresolved
document reads. The OQ-02 read is now complete; its row remains to record exactly
which evidence closed the nominal law. OQ-01 and OQ-04 remain document work,
while OQ-02's remaining installed-tolerance task is a measurement.

**The Service Notes are freely readable on the Internet Archive**, item
`synthmanual-roland-juno-106-service-notes`, which carries both page images and a
full OCR text stream. A second copy is hosted at
`analoguerenaissance.com/D80017A/juno-serv.pdf`, on the same site as the unread
JUNOTEST procedure above. That single document contains all three relevant
regions:

| Task | The page | What to read off it | What it converts |
|---|---|---|---|
| **OQ-01** | p. 15, JACK BOARD | *Now confirmation rather than discovery:* the 2026-08-06 netlist pass closed β (summing node, 33/47) and C3 (0.1 µF) from the sister board's clone, and the rates ship as **derived**. The page read still independently confirms both against Roland's own print — and gains one line from the same pass: **which IC6 input resistor carries the wet return** (R71/R73 side), because the clone wires wet through 39 kΩ and dry through 47 kΩ, the mirror of this project's anchored reading, a 3.24 dB question if the transcription swapped them | original-page confirmation of an already-derived scale; a wet/dry balance check worth 3.24 dB if wrong |
| **OQ-02 — read complete** | p. 8 converter chart and p. 15 jack board | `d=b<<5`, +4/−6 V, R30 2.2 kΩ, C7 10 µF NP, R32 1.5 kΩ, R31 47 Ω and R165 15 kΩ | Nominal VCA LEVEL from **voiced** to **derived**: `−16.3196647+0.165581014·b` dB and `τ=9.08249 ms`; only installed variation remains. This replaces the shipping cubic, which was 6.9 dB out at mid-slider across all 128 factory patches |
| **OQ-04** | chorus support chain | the capacitor codes behind `YouKnow106Chorus.cpp:73-83`, **read separately for the input and output sides** | wet-path bandwidth; the chain is −12 dB at 10 kHz where the MN3009 alone is ~−3 dB, and the reconstruction sections currently *assume* the input sections' part values rather than reading their own |

For OQ-01 the schematic read is a single yes/no, and the arithmetic has already
decided which answer is consistent:

- if the integrator output reaches IC1a's **inverting** input, with R6/R15 setting
  hysteresis, then `β = R15/(R15+R6) = 1/48` — the value the code carries, which
  the project's own timing arithmetic puts **34× off** the reported rates, and
  which would make the modulation triangle only about ±0.3 V;
- if the integrator output and the comparator output meet at a **summing node on
  the non-inverting input**, then `β = R7/R6 = 33/47 = 0.702`, which lands the
  derived rates within **3 %** of the reported ones and gives a ±10 V triangle —
  a sensible amplitude to drive Tr22's control current.

Two independent lines already favour the second reading. Neither is a substitute
for looking.

Further third-party leads, recorded as pointers rather than as evidence — none has
been read, and nothing in them may be promoted without the reading:
a Gearspace thread titled *"Detailed values of the Juno-106 chorus"*
(`gearspace.com/threads/detailed-values-of-the-juno-106-chorus.1367045/`), a long
JUNO-106-chorus-clone build thread on ModWiggler (`viewtopic.php?t=111159`), the
**One-O-Six** chorus clone kit's published bill of materials
(`alpesmachines.net`), and a 106 chorus rack build at
`hkadesign.org.uk/106chorus.html`.

**Environment note.** This attempt was made from a session whose egress policy
allows GitHub and nothing else, so every host above returned a proxy 403 and none
of them was actually read. That is a property of the session, not of the sources.
A later pass should check whether it can reach them before spending the time.
A 2026-08-06 session re-checked: github.com and raw.githubusercontent.com
answer, while archive.org, vintagesynthparts.com, cdn.roland.com,
manualslib.com, seriescircuits.com, usermanual.wiki and synthxl.com all still
refuse at the proxy, and a search for a GitHub-hosted scan of the JUNO-106
Service Notes (including the joeynotjoe/Schematics-Manuals collection, which
carries no Juno item) found none — so the p. 15/p. 9 original-page reads
remain blocked from this environment class and need either a widened egress
policy or a human with the document.

### Two anchored ADJUSTMENT values still unused

Both are already recorded in the evidence table and neither has ever been carried
into the model or a test.

- **`VCA BIAS`, TP7, +0.25 … +0.27 V.** This is adjusted globally by VR34 in the
  DAC-conditioning/control path before the per-card sample-and-holds. It is not
  the per-card VR30/R112 pin-9 signal-input null and cannot validate a 150 mV
  VCA onset. Its downstream scaling into pin-11 current still needs a complete
  solve or simultaneous TP7/control-current measurement.
- **`DCO CV OFFSET`, TP3, 0 V.** Unused, and worth a look: the model's DCO
  compensation voltage is kept in the frequency it stands for rather than in
  volts, so what a 0 V trim at TP3 constrains is not currently obvious.

## Implementation pass — 2026-08-06 (later same day)

**Work mode:** analysis of supplied evidence plus measurement of the shipping
algorithms. **No hardware was measured.** Every dB and cent figure below was
produced against the built engine and is reproducible from this repository.

This pass acted on the evidence search above. Four items moved from *voiced* to
*derived*, and three proposals were measured and **rejected** — those are
recorded here in as much detail as the accepted ones, because a rejected idea
that is not written down is an idea that gets tried again.

### Acted on — the two anchors nobody had checked

The ADJUSTMENT table's own numbers, taken seriously for the first time.

- **OQ-09 — the self-oscillation endpoint was 4.1 dB out.** The table trims
  every card, at BANK 3 with C4 held, to a **4.8 Vp-p self-oscillating sine at
  248 Hz**. The suite had only ever checked the frequency. Measured against the
  amplitude, the model produced **2.99 Vp-p**.

  The two anchors are coupled and neither can be satisfied alone: the limit
  cycle grows with loop gain, and the stage `tanh`'s compression at the larger
  amplitude pulls the oscillation flat, so raising loop gain alone lands 4.8
  Vp-p at 233 Hz — 108 cents under the frequency anchor. `maximumFeedback` and
  `frequencyTrimAmount` were solved *together*, moving from a voiced 4.19 /
  0.045 to 4.51 / 0.098, and land on **4.83 Vp-p at 248.0 Hz**. The loop
  divider was held fixed rather than fitted, because an independent
  reverse-engineering of the same module reports 67.7 against the model's
  66.67 — it is the one constant in the profile with outside corroboration.

  Status: **endpoint resolved, shape still voiced.** `loopGain()` is unchanged
  below travel 0.9, so the change is confined to the top tenth of the slider
  plus a resonance-scaled cutoff lift. The shape between the endpoints remains
  voiced, and it carries one known wart: the trim is a function of loop gain
  while the droop it corrects is a function of amplitude, so it lifts cutoff
  slightly below the oscillation threshold where there is no droop to correct.
  Closing that needs the measured frequency-response-versus-resonance family
  this task already asks for.

- **The VCF WIDTH anchor is asserted.** `BANK 3, hold C6 → 992 Hz` against
  `hold C4 → 248 Hz` is exactly two octaves of cutoff for two octaves of
  keyboard. The new test drives it end to end through the real converter path,
  so full key tracking is held at 1.00 by measurement rather than by reading
  the coefficient.

- **OQ-15 — the filter-drive budget is asserted, and was already right.**
  Working back from the same table gives ±2.4 V at the module input and
  therefore **±19.6 mV at each differential pair** through the IR3109's own
  560 Ω divider. The shipping `filterInputAttenuation = 0.40` — a *voiced*
  constant — puts a full-level source at exactly that figure. Recorded as
  corroboration, not a change, and now fenced by a test.

### Acted on

- **OQ-18 — the upper cutoff knee.** The single-pole `R_e` compression is
  replaced by the transconductor's own control-current saturation, using the
  generalized algebraic clip already used twice elsewhere in the engine, with
  the asymptote taken from the AS3109 teardown's 700 µA internal saturation
  (a 64 kHz pole on this circuit's 240 pF / 68 kΩ) and only the exponent fitted
  to the measured card. The base and slope are **not** refitted: they are pinned
  by Roland's own 248 Hz calibration anchor, and the search's own conclusion was
  that the exponential law and 1143 counts/octave are sound.

  Result: worst error against the measured table falls from **−143 cents to
  under 30**, the correction is under **5 cents anywhere below 2.7 kHz**, and
  the Unit Character 0 case is no longer 292 cents sharp — the saturation is a
  property of the part, so it applies at every setting, like the output
  summer's rails. Status: **partially resolved → derived, with one fitted
  exponent.** The remaining gap is a Roland-published or independently
  reproduced curve.

- **OQ-07 / OQ-18 — the R-2R carry.** Reinstated as a persistent offset applied
  by the converter write to the code it just produced, which is where the search
  said it belongs. A slow sweep crossing mid-scale now steps by about 23 cents.
  Scaled by Unit Character: an ideal ladder has no carry error, so its size is
  resistor matching. Status: **implemented**; the underlying hold topology
  remains OQ-07.

- **OQ-19 — the voice VCA.** Replaced the old wide curve with a narrower,
  schematic-informed quasi-linear compatibility law. The drawing anchors the
  separate pin-11 control-current and pin-9/VR30 thump-null paths, but not an
  exact softplus, onset, knee or residual. The unsupported control² feedthrough
  term is now removed; the focused raw fixture falls from −68.24 to −148.42 dBFS
  peak. Status: **topology narrowed, numerical transfer and calibrated residual
  still open**.

- **OQ-10 — the cutoff thermal spread.** Reduced from ±165 cents to about
  ±10, derived from the same exponential card profile as the temperature through
  the AS3109's 0.33 %/°C tempco and taken about the six-card mean. Still an
  upper bound rather than a residual, because R111's positor exists to cancel
  exactly this. Status: **internally consistent now**; OQ-10 itself still needs
  population data.

### Rejected, with the measurement that rejected it

- **Running the VCF on a doubled grid does not buy −87.7 dB.** The search's
  model-internal note predicted the bright-resonant in-band floor would go from
  −55.5 dB to −87.7 dB with the filter run at 2× the internal rate. It was
  built — half-step coefficients, linear input interpolation, a symmetric
  three-tap decimator whose null sits on the folding frequency — and measured at
  **−48.5 dB → −54.4 dB**, for about **40 % of the whole engine's cost**. It is
  not in the shipping code.

  What the intervening experiments established, which is the reusable part:

  1. The artefacts are not broadband. They are discrete lines at
     `192 kHz − n·f0` for `n` around 169–183: harmonics the tanh set creates
     above the internal Nyquist, appearing directly in band because that grid
     cannot hold them.
  2. They are **not** the converter scan (doubling the scan rate moved nothing),
     **not** the card noise (zeroing it moved nothing), **not** the output
     summer or its slew limiter (disabling both moved nothing), and **not** the
     decimation chain (48, 96 and 192 kHz hosts all reach the same 192 kHz
     internal rate and measure identically).
  3. Widening the oscillator's residual kernel from 4 to 8 half-widths moved the
     same case by **0.7 dB**, confirming the earlier note that the BLEP tables
     are not the limit here.
  4. The doubled grid is limited by the *interpolation*, not the decimation.
     Zero-order hold into the doubled grid measured −48.8 dB — no better than
     not doubling at all — while linear interpolation with no decimation filter
     measured −55.5 dB. Getting the predicted figure would need a real
     interpolating upsampler per voice, which costs more than the filter it
     feeds.

- **The chorus-mode noise delta cannot be reproduced by sweep depth.** The
  search records a usable third-party delta — Chorus II is 3.95 dB noisier than
  Chorus I — with the suggested mechanism "mode II's sweep spends more time at
  low clock rates". That contradicts this project's own settled finding, which
  the schematic fixes: **the mode line changes a timing resistance only**, so I
  and II have identical sweep depth and identical clock ranges, and the
  time-average of the clock is the same for both.

  The model also already carries the physics the explanation appeals to: BBD
  noise is injected once per modeled BBD shift at fixed per-sample amplitude, so its
  in-band density already rises as the clock falls, automatically and without a
  coefficient. Reproducing the 3.95 dB by any depth or level difference between
  the modes would be inventing a mechanism the circuit does not have. **Not
  implemented.** The delta stands as unexplained evidence against OQ-03, and
  wants a different mechanism — most plausibly something in the mode switch's
  own network rather than in the sweep.

- **OQ-02 (VCA LEVEL) was not changed during this evidence-search pass.** At
  that point the linear-in-dB argument was conditional on an unread divider, so
  retaining the cubic was the evidence-safe decision. A subsequent direct read
  of p. 15 identified R30/C7/R32/R31/R165; combined with p. 8, `d=b<<5` and the
  NEC constant, it now derives the nominal linear-in-dB law and 9.08249 ms
  settling. This bullet is retained as decision history, not current status.

### Also measured, and acted on

- **The decimation window was costing the top of the band.** Both decimation
  stages shared a 63-tap Blackman-Harris half-band, whose main lobe is eight
  bins wide. At a 44.1 kHz host that put the final stage's transition at roughly
  18–30 kHz: **0.85 dB down at 20 kHz, with content folding onto 19.1 kHz
  rejected by only 31.7 dB.** Replacing the window with a Kaiser at the standard
  80 dB design value, at the same tap count and therefore the same cost, moves
  the transition to about 20–28 kHz: **−0.43 dB at 20 kHz and −46.2 dB of fold
  rejection at 44.1 kHz**, and **−0.00 dB / −80.5 dB at 48 kHz** against the old
  −0.06 / −63.7. `Tests/YouKnow106CircuitTests.cpp::testDecimatorProtectsTheTopOfTheBand`
  measures the built kernel rather than trusting the design equations. The
  Bessel function is written out in the engine because the standard
  special-function header — the original reason a Kaiser window was avoided —
  is not available on every toolchain this project builds with.

## Netlist-corroboration pass — 2026-08-06 (chorus board and third-source sweep)

**Work mode:** analysis of supplied/public source material. **No hardware was
measured.** Three sources were read in full: the KiCad netlist of
`gligli/juno-chorus-clone` (a Juno-60 chorus-board clone — sibling evidence,
labelled as such throughout), the `ultramaster_kr106` source tree (GPL-3.0;
facts and measurements cited, no code taken), and a search-snippet sweep of the
public web under an egress policy that still blocks every relevant host except
GitHub.

### OQ-01 — the scale went from borrowed to derived

The clone's netlist settles the two readings the queue was blocked on, for the
board family if not yet for Roland's own print:

- **The comparator is the two-resistor summing-node Schmitt.** 47 kΩ returns
  the comparator's own output to its non-inverting input, 33 kΩ brings the
  triangle to the same node, the inverting input is grounded, and **no divider
  resistor exists on that node**. β = 33/47, and the falsified `1/48` reading
  is closed (the arithmetic had already put it 34× off).
- **The integrator capacitor in that position is 100 nF**, matching the
  reported ".1" print beside C4's "220P" on the 106's own p. 15.
- **The clock driver is component-identical on the two boards**: the V-to-I
  values (2.2 kΩ/22 kΩ/1.8 kΩ, 150 pF) and the per-line 8.2 kΩ all match the
  106's p. 15 transcription. This upgrades the retained JUNO-60 sweep from "a
  sibling's numbers" to "a calibrated measurement of the circuit both boards
  share".

The engine now ships the derived rates `f = 1/(4·β·R_eff·C3)` =
**0.5532934 / 0.8982608 Hz** (ratio exactly the schematic's 1.6234799). Every
measured rate now on record sits below the derived nominal, which is the
expected side for ±5% timing parts: JUNO-60 0.513/0.863 (calibrated capture),
a 106-chorus clone's scope 0.537/0.879 (−3.0%/−2.2% vs derived), and KR-106's
real-106 Chorus I **0.514 Hz** (−7.1%; two-carrier cross-verified, raw capture
not published; its Chorus II value is Juno-6-inherited and unusable for the
ratio).

### OQ-01 — the sweep *trajectory* has now been measured once, against us

KR-106 reports a ~50-point click-timing **time series** of the 106's wet delay
across one modulation cycle — exactly the measurement this queue said was the
only discriminator between period-, frequency- and exponential-linear clocks.
Result: **delay linear in time, 16 µs RMS residual, "no exponential
curvature"**, centre 3.30 ms, swing ±2.13 ms (1.17–5.43 ms), and their
changelog records shipping clock-domain modulation and *reverting to
delay-linear on measurement*. This contradicts the frequency-linear reading of
Tr22's current source that `enableChorusHyperbolicSweep` encodes; a
delay-linear triangle also renders the classic fixed-detune character rather
than a mid-flank pitch slide. Raw click data is not in their repository, so
this stays below the anchoring bar — but it is a direct measurement against an
explicit assumption, and the assumption side of that pairing loses the
default. The sweep-geometry scatter (their 1.17–5.43 ms vs the 60-measured
1.66–5.35 vs the owner-report 1.4–6.4) remains mutually inconsistent and
unpromoted.

### IC6 wet/dry assignment — RESOLVED 2026-08-07 by the p. 15 read

Two independent sibling-board transcriptions — the gligli netlist (wet enters
the final summers through **39 kΩ** from the 2SK30 mute sources, dry through
**47 kΩ**) and KR-106's own schematic reading (same assignment) — both carried
the mirror of this project's earlier reading (dry 39 kΩ, wet 47 kΩ,
wet/dry −1.62 dB). The 2026-08-07 designator-level read of the Service Notes
p. 15 scan settled it in the siblings' favour: R71 = 47 kΩ and R73 = 47 kΩ
both hang off the shared vertical dry bus from IC2b pin 7, while R72 = 39 kΩ
and R74 = 39 kΩ arrive from the wet returns behind Tr11/Tr12, into 100 kΩ
feedback R70/R67. Dry gain `100/47`, wet gain `100/39`, wet/dry +1.62 dB.
Both legs enter the same inverting input per channel and the two channel
mixers are drawn identically — no polarity inversion between the wet returns,
confirming the shipped same-polarity topology. The implementation, suite and
guardrail were corrected in the same change.

### KR-106 mining — contradictions and corroborations recorded

Nothing in the KR-106 tree meets this project's anchoring bar (no raw captures
ship in the repository, and its comments drift against its constants), but the
following are recorded per contract rule 5. Corroborations: its firmware
tables and semantics match every OQ-12/13/14 fixture of ours exactly
(independent j106roms-lineage consistency check); its cycle-count scan
timeline lands on our provisional ~125 µs VCF→VCA offset (adding a voice-6
exception near 402 µs and a ~2.2 ms compute/write split); its designator-level
hold RCs reproduce our 522/687 µs anchors and add a WIDTH-trim dependence; its
measured 56-point 106 envelope→gain curve is near-linear with a soft toe,
siding with our OQ-19 law; its measured DAC anchors two-anchor-confirm
1143 counts/octave. Contradictions: a measured pass period near **4.27 ms
±3%** against the timing chart's anchored 4.2 ms (anchor kept); a measured
**sub/pulse ratio of 1.51** against our voiced 0.833 (OQ-15 — the largest new
audible lead of this pass); "dB-linear ±10 dB confirmed from hardware" against
our OQ-02 three-point cubic; a service-manual noise-circuit trace implying a
34 Hz–5.3 kHz band-shaped source against our flat-white OQ-16 placeholder;
C14 cut corners computed with the 1 MΩ bias resistor participating
(236/754 Hz vs our 225.8/720.5 — OQ-21); measured INL steps at the shared
DAC's MSB flips (−4.6/+23.3/−4.5 cents at codes 1024/2048/3072 — OQ-18); and a
+18 dB wet-over-dry 120 Hz component in its noise calibration against our
rail-ripple-inaudibility derivation (single unit, health unknown — OQ-03).
Provenance warning recorded: our OQ-09 landmarks (0.91 at 30%, onset 0.9,
max 4.19) turn out to be KR-106's own fits, with the 0.91 point measured on a
*Juno-6* — they were already labelled comparison landmarks, and must never be
promoted.

### Web sweep — one usable absolute, the rest scatter

The chorus-noise thread supplies absolute floors under a declared chain
(Chorus I −47.97 dBFS / II −44.01, NOS MN3009s; the II−I delta is **3.95 dB
on both chip populations**, confirming it as structural — OQ-03), and the
mod-lore around the One-O-Six clone yields only an ambiguous stock-clock
inference (~15–40 kHz) recorded with the other unpromoted sweep reports.
Selector spec H/M/L = 0/−15/−30 dBm re-surfaced (OQ-06); everything else was
dead ends, logged in the session record so the queries are not repeated.

## Module netlist-corroboration pass — 2026-08-06 (voice-module reconstruction and support-chain identity)

**Work mode:** analysis of supplied/public source material. **No hardware was
measured.** Two source trees were read at netlist level:
`ThomHPL/Open80017a` (CERN-OHL-S; a KiCad 8 reconstruction of the potted
80017A voice module whose v0.1 build was installed and played in a real
JUNO-106 — "no perceivable difference (to my ears)", a listening report, not
a calibrated sweep — and whose rev 0.2 changes footprints only; it credits
the dksynth/guest ModWiggler thread, the same lineage as the reconstruction
OQ-09 already cites, and the same 80017A teardown this project cites for
device identity), and a re-read of `gligli/juno-chorus-clone` (GPL-3.0).
From both, component values and connectivity are cited as facts; no code or
design files are taken. Connectivity was extracted from the LTspice and
KiCad sources by geometry, not read off renders, and the KR-106
`docs/analysis` tree was checked directly: it ships analyzer scripts and
summary reports whose source WAV/click captures are not in the repository,
confirming the earlier "raw captures unpublished" standing.

### OQ-04 — the output sections are the input sections, read rather than assumed

The chorus-clone netlist carries exactly three complete Sallen-Key chains —
one before the BBDs, one per output line — every section built from the same
four values: 22 kΩ/22 kΩ with 820 pF feedback and 680 pF shunt (9.69 kHz,
Q 0.549), then 22 kΩ/22 kΩ with 1.8 nF and 270 pF (10.38 kHz, Q 1.291).
Each BBD input has its own 10 kΩ/2.2 nF passive pole; each BBD's two outputs
reach the summing node through 3.3 kΩ each against 47 kΩ to ground and
2.2 nF; each branch couples through 100 nF against 100 kΩ (15.9 Hz). The
engine's `reconstructionFirst/Second` corners therefore no longer merely
*assume* the input sections' part values: the identity is corroborated at
designator level for the board family, by the same netlist whose clock
driver and LFO components matched Roland's p. 15 transcription. Not closed:
the 106's own p. 15 capacitor codes (a single 10× misread on Roland's print
would still move a corner a decade), the MN3009/emitter-follower output
impedance, and the loaded time-varying transfer — all still OQ-04. The
clone's own front end (10 µF into 33 kΩ and an op-amp input stage) is
clone-specific and is not evidence against the 106's C44/R120 wet coupling.
One inference is recorded, not promoted: a five-pole ~10 kHz chain on *both*
sides of the BBD is a coherent design for a clock whose minimum approaches
24 kHz (Nyquist 12 kHz) and needlessly dark for a ~43 kHz minimum, which
sides with the retained 23.9–77.1 kHz range against the higher third-party
minima — a design-intent argument, not a measurement.

Status: **partially resolved** (part identity family-corroborated;
corner values, Roland print read and loaded transfer still open).

### OQ-09 — the compensation mechanism is netlist-verified; its converted slope brackets the shipping constant

Extracted connectivity, identical in the LTspice full-module sim and the
rev 0.2 KiCad values (24 kΩ on the board, 25 kΩ in the sim draft): the
resonance OTA's non-inverting input is fed from **VCF IN through
24 kΩ/1.5 kΩ (÷17.0)**, its inverting input from **VCF OUT through
100 kΩ/1.5 kΩ (÷67.7 loaded — the same network the model's
`loopDividerRatio` reads unloaded as 66.67 and the earlier
reverse-engineering reported as 67.7)**, and its **output current injects
into the first filter stage's summing node** — the junction of the 4.7 kΩ
input resistor, 560 Ω shunt and the stage's own 68 kΩ feedback. That is the
mechanism of `inputCompensation()`, now verified at netlist level in a
design that ran inside a real JUNO-106: raising resonance raises feedback
and input drive together through one transconductance.

Because stage 1 is a feedback stage (DC gain −68 k/4.7 k = −14.5; the
inter-stage sections are −68 k/68 k unity), converting the dividers into the
model's loop-gain coordinate needs no device parameter — the resonance
OTA's gm cancels: `slope = (67.7/17.0)·(4.7/68) = 0.275` input boost per
unit loop gain, against the shipping voiced
`inputCompensationPerFeedback = 0.2296` — about 20% above it (equivalently,
the shipping value sits 17% below), same linear-in-k form.
Also read from the same netlist: the loop's limiting mechanism is OTA
saturation alone (no clipping diodes anywhere in the reconstruction), and
the first stage's gain structure (−68 k/4.7 k rather than a passive
divider) is a new lead against OQ-15's open input coordinate.

This is **corroboration, not an anchor**: Open80017a descends from the same
dksynth thread as the ÷17.0/÷67.7 figures already on file, so the agreement
is depth within one lineage — now netlist-explicit and build-validated by
ear — not a second independent source. The constant is unchanged on that
provenance ground alone. A future refit is also *cheap*: the multiplier acts
only on the mixed source signal entering the cascade, and the
4.83 Vp-p/248 Hz endpoint pair is solved with every source silenced, so the
two calibrations are independent and re-fitting the compensation would not
disturb the endpoint solve. Status: **mechanism resolved, magnitude still
open** — the 128-point family OQ-09 asks for owns it.

### OQ-19 — corroboration only

The reconstruction's VCA block drives the OTA bias input through a plain
resistor-defined current path with 47 pF/47 kΩ compensation — consistent
with the quasi-linear reading Roland's drawing supports. The clone
substitutes LM13700 halves for the BA662s, so this establishes topology
only, never the BA662's own low-current curve; the OQ-19 measurement stands.

### Negative results, recorded so they are not repeated

No GitHub-hosted scan of the JUNO-106 Service Notes exists in any repository
found by search (the joeynotjoe/Schematics-Manuals collection carries no
Juno item), so the p. 15/p. 9 original-page reads remain blocked from
GitHub-only egress. The KR-106 analysis tree publishes no raw captures — its
`chorus_report.txt` summarizes an out-of-repository Juno-6 WAV. Neither
Open80017a nor the chorus clone contains any Roland page image.

## Original-page evidence pass — 2026-08-07 (Service Notes obtained and read)

**Work mode:** evidence search plus analysis of primary source material. **No
hardware was measured.** This session ran with the widened egress the earlier
passes requested, and the blocked reads landed: the **Roland JUNO-106 Service
Notes, First Edition (every content page dated JUL. 31 1984)** were obtained
as two independent scans — `synthfool.com/docs/Roland/Juno_Series/
Roland_Juno_106/roland_juno106_service_manual.pdf` and
`polynominal.com/roland-juno106/Roland-juno106-service-manual.pdf`, both
2,773,470 bytes (byte-identical circulating copy, PDF metadata "Edy Hinzen",
Sep 2001 scan), carrying printed pages 1, 5, 8, 9, 10, 11, 12, 13, 15, 16,
18, 19 at ~315 dpi (pp. 2–4, 6–7, 14, 17, 20+ absent — the parts list is not
in this copy). All reads below were made from full-resolution crops of the
embedded 1-bit scans; page/designator corrections against earlier
assumptions: the MODULE BOARD schematic is printed **p. 13** (p. 9 is the
WAVE GENERATOR text page), and the C14/R39 network is the **jack board
input** (p. 15), not the module summing bus.

Also read in the same session: the Gearspace Juno-106 chorus threads
(migrated to XenForo; guest view), ModWiggler threads t=111159 and t=158257
in full, and the Panasonic **MN3009/MN3101 datasheets**
(experimentalistsanonymous.com scans, digitised at 600 dpi).

### Acted on in this change-set (each with its own commit)

- **IC6 wet/dry (guardrail corrected):** dry = R71/R73 47 kΩ off the IC2b
  bus, wet = R72/R74 39 kΩ from Tr11/Tr12, feedback R70/R67 100 kΩ; same
  inverting input per channel, channels identical — no wet polarity
  inversion. See the corrected guardrail below.
- **Chorus sweep promotion (OQ-01):** the ModWiggler read grounded the
  1.4–6.4 ms figure's provenance (kvitekp, 2016-11-13, FEEDBACK kit built
  1:1 from p. 15, genuine 256-stage MN3009s, scope plots at
  midisizer.files.wordpress.com, compared directly against his own JUNO-106
  and called identical). Shipped; JUNO-60 pair kept as suite comparison.
- **Noise band-shaping (OQ-16):** p. 13 noise-circuit designators read in
  full — Tr21 2SC945 "selected", R104 470 kΩ, C42 1 µF into IC14 BA662 with
  4.7 kΩ input bias (R80/R81), C41 100 pF against R79 330 kΩ on the OTA
  output, Tr14 follower to the NOISE rail. Implemented as 33.9 Hz/4.82 kHz
  shaping of the shared source.
- **Mixer topology (OQ-15 narrowed):** saw and pulse leave the waveshaper
  summed on one WAVE output per voice; sub joins via R101/R97 27 kΩ behind
  D6/D5 (collector supply = SUB LEVEL rail, itself the S/H'd 0..−10 V
  converter output re-inverted to 0..+10 V through R9/R10 100 kΩ at IC1b);
  C56/C50 10 µF NP couple into the module. Sources mute; legs never switch.
  Implemented as constant node loading.

### OQ-21 — the 1 MΩ pair adjudicated; the cut corners stand

The HPF ladder reads: Tr3 buffer → IC3 4052 Ycom pin 3; taps Y0/Y1/Y2/Y3 =
pins 1/5/2/4; "47K×4" summing resistors R28/R26/R27/R25 into IC4a's virtual
ground with R29 47 kΩ feedback; series caps C11 4.7 nF (Y0 leg) and C10
15 nF (Y1 leg); **R23/R21 "1M×2" bias the mux side of the capacitor legs**,
tying otherwise-floating deselected lines to ground. With the mode table
(p. 13: MODE 0/1/2/3 → HPF B ① = 1,1,0,0; HPF A ② = 1,0,1,0, and the
4052's (B,A) decoding) the map is: mode 0 = Y3 (boost: direct R25 plus the
IC4b branch), mode 1 = Y2 (flat, direct R27), mode 2 = Y1 (C10 15 nF),
mode 3 = Y0 (C11 4.7 nF). On the selected leg the 1 MΩ parallels the
buffer's low output impedance and cannot move the pole; the corner remains
the capacitor against its 47 kΩ virtual-ground leg: **225.8/720.5 Hz stand,
and KR-106's 236/754 Hz (1 MΩ paralleling the 47 kΩ) put the bias on the
wrong side of the capacitor.** The deselected-leg charge-memory question the
1 MΩs actually govern remains OQ-21's transient ask.

### OQ-21 — the Boost branch is designator-complete, and contradicts the fitted shelf's shape

The boost branch reads: selected Y3 line → C9 47 nF → node with R22 47 kΩ
to ground → R20 47 kΩ → IC4b's non-inverting input with C8 10 nF to ground;
IC4b gain 1 + R18 100 kΩ/R19 10 kΩ = 11; output → C6 22 nF → R24 220 kΩ →
IC4a's summing node (R29 47 kΩ feedback), in parallel with the dry R25 leg.
Solved exactly, the branch is two zeros (C9, C6) and three poles — the
coupled C9/R22/R20/C8 section gives poles at **57.3 Hz and 425.4 Hz**
(peak transmission 47/67 = 0.702 near 156 Hz), and C6/R24 adds a 32.9 Hz
high-pass — for a **band boost peaking ≈ +8.5 dB near 150 Hz, returning
toward unity at DC and settling ≈ +0.7…+1.4 dB in the high band** (branch
plateau 11 × 47/220 = 2.35 over dry). The shipped one-pole shelf
(+10.5 dB DC / +1.41 dB HF / 59.4 Hz pole, from a third-party hardware
noise sweep) agrees with the branch in the plateau region within ~2 dB but
**contradicts it in shape below ~60 Hz** (the branch is capacitor-coupled
and cannot boost DC). Recorded per contract rule 5, not reconciled: the
sweep's raw data is unavailable, the branch read carries residual
connectivity risk from a 1-bit scan (R22's lower node), and a one-pole
shelf fitted to a band-boost would infer exactly the too-high DC asymptote
observed. One clean low-frequency response measurement of position 0 — or
one designator-level photo of the C9/R22/R20 region — settles whether the
implementation should move to the derived two-zero/three-pole branch.
Sequence15's commenter analysis ("two poles and two zeros … two pole bass
boost") sides with the branch. *Superseded 2026-08-07 by the complete-scan
pass below: the 300 dpi grayscale read corrects the connectivity this
solve was built on — C9 is in parallel with R22, C8 shunts their junction,
C6 bypasses R18 inside the feedback, and R24 couples DC — and the
corrected branch derives exactly the shipped +10.50 dB/+1.41 dB/59.41 Hz
shelf, within 0.016 dB of one pole. This two-zero/three-pole solve must
not be reintroduced.*

### OQ-19 — TP7 VCA BIAS consumed as the control-path operating point

Pages 18–19 (read in full, both scans agreeing): VCA BIAS adjusts **VR34
(10KB) for +0.25…+0.27 V at TP7** with the D/A forced to 0 V, *before* the
per-card VCA OFFSET thump nulls. Page 13 places VR34's injection through
R126 18 kΩ/R127 470 kΩ into distribution amplifier IC27b (R135/R136 10 kΩ),
whose output is TP7 and feeds the VCA-group demux IC26 — so the per-voice
VCA control rail carries a **+0.26 V nominal standoff at envelope zero**.
Read with the reconstruction's ~150 mV no-current region (measured from the
CV input on a calibrated unit, i.e. *on top of* the trimmed standoff), the
shipping `VoiceVcaControlLaw::turnOn = 0.015` — 150 mV of envelope travel
above the bias point — is exactly the surviving free parameter, now
expressed relative to an anchored operating point rather than floating.
No constant changes; the BA662's own low-current curve remains OQ-19's
measurement. DCO CV OFFSET (TP3 = 0 V via VR33, D/A forced to zero) is
likewise consumed: the model's zero-offset pitch law *is* the adjusted
state, and no static DCO CV offset term may be added to the nominal model.

### OQ-07 — every converter hold is 0.01 µF, read at designator level

Page 13 marks the hold capacitors "**.01×7**"/"**.01×8**": IC24 (DCO group)
C73–C79, IC23 (VCF group) C65–C72, IC26 (VCA group) C80–C87, all 0.01 µF,
with the per-destination post-hold smoothing drawn separately (VCA CONT:
R105/R95 22 kΩ with C58/C49 0.1 µF at the module pin; VCF: C61/C48 0.1 µF
in the trim network; PWM: R117 100 kΩ/C62 47 nF then R116 560 kΩ/C63
4.7 nF around IC17a; SUB LEVEL: R11 1 kΩ + C1 10 µF then the R9/R10
inverter; VCA LEVEL: the jack-board R30/R32/C7/R165 network already
modelled). The mux parts are 4051s with IC26 on ±15 V behind IC25 (7407)
level shifting, "(except TC4051)" noted as a parts restriction. This gives
OQ-07 its designator-complete acquisition-network inventory; the in-window
tracking behaviour and droop measurements remain open.

The two networks the read fixed outright are consumed (2026-08-07): the
engine now slews the PWM hold through the R117/C62 then R116/C63 cascade
(4.7 ms and 2.632 ms) and the SUB hold through R11/C1's 10 ms pole,
replacing their 522 µs voiced compatibility defaults. Both networks settle
to the held value, so the calibrated DC laws are untouched; what changed
is the lag the PWM LFO and the level staircase cross on their way to the
cards. DCO, RESONANCE and NOISE keep their labelled 522 µs defaults.

### OQ-03/OQ-01 — Gearspace provenance corrected and the noise floors sourced

The thread "Detailed values of the Juno-106 chorus" (gearspace, 1367045,
Bonsaipanda, 2021-12-01) is a **zero-reply question post with no
attachments** — the earlier citation of it as carrying spectra and WAV
measurements is a misattribution, corrected here. The measured floors
recorded against OQ-03 originate in thread 916126 ("Juno-106 chorus
noise."), post-13930822 by **heduhl** (2019-04-17): stereo 48 kHz/24-bit,
Volume 10, Output High, +25 dB into a Fireface 800, iZotope RX true-peak
over noise-only selections — Panasonic MN3009s: Chorus I −47.97 dBFS TP,
Chorus II −44.01; Xvive MN3009s: −52.19/−48.24; "both chip populations show
the 3.95 dB I→II boost" (structural), corroborated by autoy on a second
parts set. No reference tone accompanies the chain, so the absolute floors
still cannot calibrate the model's hiss; the RX spectrum screenshots are
not retrievable from guest view. Mode-depth lore recorded as assertions:
martin@arturia (Juno-6, explicitly not measured on a 106): I+II is a
mono-vibrato-like mode with 0° LFO offset, ~7–8 Hz, sine, low depth; two
Juno-60 accounts of I+II contradict each other; no source states what I+II
does on a real 106, whose owner's manual forbids it.

### OQ-04 — MN3009 typical curves digitised, and they exceed the model's framework

The Gi–fi family (VDD −15 V, VGG −14 V, Vi 0 dB) was digitised at 600 dpi
with axis calibration (frequency ±4–5%, level ±0.1 dB): flat-band +0.78 dB
for all three curves; −3 dB-relative points at **fi/fcp = 0.522 (fcp
10 kHz), 0.403 (40 kHz), 0.355 (100 kHz)** — the normalized bandwidth
*shrinks* as the clock rises, so the family does not scale with fcp. Two
consequences, recorded rather than acted on: (a) the EC table's "fi min
12 kHz at −3 dB, fcp 40 kHz" row the model anchors to is the
guaranteed-minimum part, while the typical curve reads 16.1 kHz — the
shipped anchor is conservative by ~0.4 of an octave of wet bandwidth; and
(b) the typical family is **not reproducible inside the model's
hold-plus-single-pole framework at any coefficient** — at fcp = 10 kHz the
−3 dB point sits at fi/fcp = 0.522, where the model's own full-period hold
alone is already −4.3 dB, so matching the typical family needs a different
output-stage reading (the two-phase OUT1/OUT2 summing the 3.3 kΩ tap pair
implements, whose composite response must be solved against these curves
before anything ships). *Both consequences are superseded in part by the
same-day output-stage solve recorded below: (a)'s direction turns out to
depend on an unresolved fixture reading, and (b) dissolves — the two-phase
composite is itself a full-period hold, the shipped framework is
confirmed, and the >Nyquist points belong to the measurement plane, not to
a different sampling structure.* Also extracted: the full THD–Vi curve (minimum
0.42% near 0.8 Vrms, 2.5% at ≈1.97 Vrms — the EC table's "2.5% at
1.5 Vrms" is a minimum-swing guarantee, and its "0.3% typ" disagrees with
the same sheet's typical curve reading 0.43%, recorded as the datasheet's
internal contradiction); Gi/THD versus RL (Gi 0 dB crossing near 27–28 kΩ,
THD minimum at 70–100 kΩ); and THD versus fcp across 24–200 kHz. From
MN3101: **clock = half the oscillation frequency is stated three times**
(the recorded measurement trap is now primary-confirmed); CP1/CP2 are
complementary at duty 1/2 with no specified non-overlap; the oscillator has
**no voltage-to-frequency input** — external control is only by driving
OX1 — which bears on OQ-01's period-versus-frequency-linear question: the
106's Tr22 network drives the MN3101's oscillator node directly, and the
datasheet supplies no transfer for that condition.

### Chorus LFO designators — prose corrected, derivation unchanged

Page 15 reads: R5 1 MΩ (IC1a square out → junction), R8 2.2 MΩ (junction →
IC1b integrator), C3 0.1 µF, **R4 680 kΩ from the junction to Tr1's drain**
(the mode shunt), **R3 2.2 MΩ as Tr1's gate-source bleed**, D1/Tr2/R2 47 kΩ
as the drive from the CHORUS I/II line, R11 150 kΩ gate pull to −15 V;
Schmitt R6 47 kΩ/R7 33 kΩ; IC2a inverter R10/R9 33 kΩ with C4 220 pF. The
earlier description "Tr1 shorts R3 2.2 MΩ" mislabelled the shunt leg — Tr1
grounds the junction *through R4* — while the derived R_eff values and the
0.5533/0.8983 Hz rates (independently corroborated by the clone scope
measurements 0.537/0.879 Hz) are unchanged. The p. 13 mode table (③ I/II:
OFF = •, I = 0, II = 1; ④ on/off: OFF = 1, I = 0, II = 0) does not label
which logic level conducts Tr1; the rate assignment rests on the derived
R_eff pair matching the measured rates, mode I the slower leg.

### Corroborations recorded without change

- Page 8's printed scan order — NOISE, RES, VCA LEVEL, SUB, DCO CH1–6,
  PWM, then interleaved VCF/VCA per channel — is the shipped 23-write cycle
  (rotated: the model begins its pass at RES; the cycle is identical).
- Page 1 spec verbatim: `VCF ENV MOD ±14 octaves` (verified at high zoom),
  `KEY FOLLOW +3/−2 octaves`, `ATTACK 1.5 ms–3 s`, `DECAY/RELEASE
  1.5 ms–12 s`, `LFO 0.1–30 Hz`, `AUDIO OUTPUT L −30/M −15/H 0 dBm`.
- The chorus support chain's capacitor codes are now read from the 106's own
  p. 15 at designator level — pre-BBD C33 820 pF/C31 680 pF and C34
  1.8 nF/C32 270 pF, per-line post-BBD C37/C35 and C38/C36, line 2 C42/C40
  and C43/C41, all with 22 kΩ pairs; BBD input poles R122/R115 10 kΩ with
  C52/C56 2.2 nF; taps R118/R119 and R111/R112 3.3 kΩ into R117/R110
  47 kΩ with C45/C48 2.2 nF; wet couplings C44/C47 0.1 µF with R120/R114
  100 kΩ biases. The family-corroborated corner set is thereby
  **106-anchored**; OQ-04 keeps only the loaded/time-varying transfer.
  (A partial contradiction from the ModWiggler kit's Mouser BOM — no
  820 pF/1.8 nF positions — is outweighed by the direct page read; the kit
  cart list was possibly incomplete, and its 270 pF×3 positions match the
  page's three 270 pF shunts.)
- The chorus-bias service step (feed 10 Vp-p at module TP2, trim jack VR1/
  VR2 for symmetric clipping) confirms the BBD bias trims target
  symmetric clipping — consistent with the fitted symmetric `bbdTransfer`.
- Cornutt's 2008 repair/calibration log (sequence15) supplies real-unit
  colour consistent with the model: DCO CV offset "dead on", PWM range
  50–95% "on spec", one unit's noise measured 6 Vpp against the 4 Vpp spec
  (out of calibration), and scope-visible ringing/tilt on pulse tops.
  His "Intel 8350" timer identification contradicts the p. 8/p. 13
  8253/82C53 print; the page wins.

### Still blocked / unread

Hosts still refusing: analoguerenaissance, hkadesign, alpesmachines,
kvraudio search (login-gated; individual thread URLs render), imgur/dropbox
attachment mirrors, midisizer.files.wordpress.com (scope plot images —
quoted figures only). Gearspace attachments are not rendered for guests
(heduhl's RX screenshots, ccaudio's schematic, DJMaytag's photos).
Pages 2–4, 6–7, 14, 17 and 20+ were absent from both copies then in hand;
the 2026-08-07 complete-scan pass closed that gap — synfo.nl's directory
403s but its `/pages/servicemanuals.html` index links every file, and its
First Edition 5th-printing scan carries all printed pages 1–21 (it also
hosts JUNO-1/-2/-6/-60 service notes). Of the three component ambiguities
a complete scan was expected to close, R22's node closed with the boost
adjudication, the noise-mixer leg advanced to a strong 39 kΩ candidate,
and the 33 kΩ/39 kΩ chain resolved into a 33 kΩ sub-emitter path plus the
39 kΩ candidate leg — the parts list itemises no discrete resistors, so
junction-level schematic reads remain the instrument for both.

## OQ-04 output-stage solve — 2026-08-07 (two-phase composite derived; no retune ships)

**Work mode:** analysis of supplied evidence plus datasheet evidence
search. **No hardware was measured.** The MN3009 datasheet was retrieved
from two mirrors and its Gi–fi page independently re-digitised at 600 dpi
(13 points across all three curves, including the drawn tails the earlier
record does not contain). Three independent derivations — sampled-data
first-principles, circuit-first, and fixture-first — were fitted
numerically and then adjudicated by re-running all three composite models
against the full trace and against the datasheet's other panels.

**The device mechanism is closed, unanimously.** The input gate admits one
packet per full clock period — a bucket can only pour into an empty
neighbour, so a two-phase line physically cannot sample at 2·fcp — and the
datasheet's own circuit diagram places OUT1/OUT2 as source followers on
the adjacent stages 257/258, which present the *same* sample on
complementary half-cycles. The equal 3.3 kΩ tap pair therefore
reconstructs a full-period zero-order hold; the half-period stagger
cancels the complementary clock pedestals and buys no bandwidth. The
shipped structure — one shift per clock period, one held value, polyBLEP
reconstruction, a single per-shift residual pole — is thereby positively
confirmed, and the digitisation pass's consequence (b) is withdrawn as an
indictment of the framework: the 10 kHz curve's smooth continuation past
fi/fcp = 0.5 is legitimate in the measurement plane, where the staircase's
image lines genuinely exist, and says nothing about the sampling
structure. Alternative readings (an effective 2·fcp staircase, half-period
return-to-zero pairs, unequal tap weights, tilted or drooping hold
windows, clock-scaled inefficiency) were each constructed and numerically
falsified against the 13-point trace.

**The typical-part parameter extraction does not close, because the scan
contradicts itself.** A tracked/coherent reading of the Gi–fi family —
sinc × essentially-zero charge-recycling dispersion × one absolute
~65 kHz bench pole — reproduces the drawn in-band shape (rms 0.295 dB
over the usable ν ≤ 0.45 band) but is falsified by the Gi–fcp panel,
which is drawn flat down to fcp = 1 kHz at fi = 1 kHz, where a tracked
reading demands an undrawn sinc plunge. A broadband-meter reading — the
staircase's image power restores the hold's sinc by Parseval, leaving the
charge-transfer law to set the −3 dB points — hits the three −3 dB
anchors but misses the drawn −1 dB points by 0.4–1.2 dB (the small-ε
charge-transfer shape forces −1 dB at ν = 0.186 where the curve draws
0.280) and is symmetric about ν = 0.5 where the drawn tails fall
monotonically to −4.6 dB. Each reading is falsified by a different panel
of the same low-resolution scan; all three curves terminate exactly on
the −4 dB gridline, so drafting idealisation is a live possibility.
Consequences recorded, not resolved: the typical-part raw held node at
40 kHz clock / 12 kHz signal spans −1.33 dB (tracked) to ≈−3.45 dB
(broadband); consequence (a)'s "conservative by ~0.4 octave" holds only
under the tracked reading and *reverses* under the broadband one (shipped
≈0.4 dB brighter than typical). `transferSmear = 0.8654743` — the
guaranteed-minimum anchor, −3.000 dB at 40 kHz/12 kHz — lies inside every
reading's band and ships unchanged; the suites now also fence the
cross-reading guard band [−4.355, −1.33] dB at that point so any future
retune must confront this record.

**New anchored numbers, recorded against the task.** The Gi–RL panel's
0 dB crossing at 27.5 kΩ, joined with the +0.78 dB flat band at
RL = 100 kΩ, solves to a summed-output source impedance Rs ≈ 3.70 kΩ
(3.60–3.79 across the 27–28 kΩ reading) and an intrinsic follower gain of
≈ +1.10 dB — the first concrete answer to this task's "the MN3009
datasheet does not specify the needed output impedance". Both are
datasheet-fixture figures: the +0.78 dB flat gain and every absolute
bench pole must never ship. De-embedding Rs into the 106's own
3.3 kΩ/47 kΩ/2.2 nF node is topology-ambiguous — is 3.70 kΩ the pair or
one follower, and do both legs load the node continuously? — giving
loaded tap-pole candidates 11.88 kHz (single active leg, Rs + 3.3 kΩ),
15.07 kHz (pair-valued Rs, both legs), and 22.21 kHz (per-follower Rs,
both legs, within 0.08 dB of shipped across the wet band) against the
shipped ideal-source 23.46 kHz. Recorded for the task's declared
MNA/wet-sweep route; no silent retune.

**Also recorded:** an EC-row edition discrepancy — the retrieved scan
prints "fi 14 kHz max, 3 dB down" where the repo's record reads "fi min
12 kHz"; the anchoring edition's row identity must be re-verified before
any future retune. *Resolved 2026-08-07 by the three-edition datasheet
sweep recorded in the complete-scan pass below: the anchoring copy prints
fi min 12 kHz, the "14 max" reading conflated its Features bullet, and the
genuine 14 kHz row belongs to an older edition at Vi = 1.8 Vrms. The
anchor stands.* The 10 kHz curve's knee and tail (ν > 0.45) exceed
every candidate composite by 0.7–1.6 dB and are quarantined; the 106's
23.9 kHz minimum clock keeps ν ≤ 0.42, outside the region.

**One decisive falsifier is named for hardware:** a single tracked
through-BBD sweep at a fixed ~40 kHz clock out to ~18 kHz. A raw-node
reading near −1.3 dB at ν = 0.3 confirms the tracked reading and a
typical part with almost no recycling dispersion; ≈−3.4 dB confirms the
broadband one. Either result collapses the typical `transferSmear` span
and settles consequence (a)'s direction.

## Complete-scan evidence pass — 2026-08-07 (First Edition 5th printing obtained; OQ-21 Boost adjudicated)

**Work mode:** evidence search plus analysis of primary source material. **No
hardware was measured.** A complete scan of the Service Notes surfaced on a
host this environment reaches: `https://www.synfo.nl/servicemanuals/Roland/
ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf` (file index at
`/pages/servicemanuals.html`; the directory itself 403s), 23,611,766 bytes,
SHA-256
`995edcfc594dbd3774f110f1cde0570da7b6236814e3c58c1639962081c3001c` — a
2018 assembly of 300 dpi grayscale A4 scans, First Edition JUL. 31 1984,
**5th printing NOV. '88** by its p. 1 footer. It carries every printed page
1–21 plus two appended Roland Service Information bulletins (No. 100222,
memory-backup battery countermeasure; No. 100229, A1Q-80017 fault lots
41C/42B). Against the circulating 12-page copy this adds pp. 2–4 (PARTS
CHANGE NOTES and PARTS LIST), 6–7 (circuit descriptions), 14 (MIDI/jack
board layouts), 17 (IC DATA), 20–21 (MIDI and SysEx tables) — and re-scans
every previously read page in grayscale at materially higher legibility.
The same session also ran the MN3009 datasheet edition sweep recorded
below. The KVR/sequence15/KR-106 corroboration titles cited here were
collected by a parallel public-source sweep the same day.

### OQ-21 — the Boost branch adjudication REVERSES: the shipped shelf is the circuit

The 300 dpi grayscale read of the p. 15 boost region corrects the 1-bit
scan's connectivity in three places, each verified at junction-dot level
and corroborated by the p. 14 layout's part placement:

- **C9 47 nF and R22 47 kΩ are in parallel** — R22's lower node is not
  ground; the ground the 1-bit read put there belongs to **C8 10 nF**,
  which shunts the C9∥R22 → R20 junction. R20 47 kΩ then feeds IC4b's
  non-inverting input and drops nothing at small signal (no input
  current), so stage one is exactly
  `H1 = (1 + sR22C9)/(1 + sR22(C9+C8))`.
- **C6 22 nF is feedback, in parallel with R18 100 kΩ** around IC4b — not
  a series output coupling. The stage's gain is 11 at DC falling to unity
  above `R18·C6` = 72.34 Hz.
- **R24 220 kΩ couples IC4b's output to the R25–R28/R29 summing bus
  directly** — the branch is DC-coupled end to end, so the "a
  capacitor-coupled branch cannot boost DC" contradiction recorded against
  the fitted shelf dissolves.

Solving the corrected network: DC gain `1 + (47/220)·11 = 3.35`
(**+10.50 dB**), high-band plateau `1 + (47/220)·(47/57) = 1.17616`
(**+1.41 dB**), dominant pole `R22(C9+C8)` = **59.41 Hz** — exactly the
three constants the shipped one-pole shelf carried as a fit to a
third-party hardware noise sweep. The first stage's 72.05 Hz zero cancels
the feedback's 72.34 Hz pole to 0.6%, which is why one pole describes a
two-stage branch: the exact two-zero/two-pole response stays within
**0.016 dB** of the shipped shelf across the band (worst near 111 Hz).
The formerly *fitted* constants are therefore now **derived**, the noise
sweep moves from source to corroboration, and the earlier
"two-zero/three-pole band-boost peaking +8.5 dB near 150 Hz" derivation is
withdrawn as the 1-bit misread the record already suspected ("residual
connectivity risk from a 1-bit scan").

Independent corroboration collected the same day: the polynominal.com
grayscale copy reads identically at every junction; sequence15's 2008 post
embeds a clean HPF crop, and its commenter DustySchematics (2009-01-22)
analyses the branch as "an active version of Stage 1 … two poles and two
zeros", C6 inside the active stage; aciddose's 2011 KVR analysis lands on
"about 10 db … if you calculate it ideally"; and KR-106's HPF header
implements the identical two-pole/two-zero response with a claimed
0.55 dB-RMS hardware noise-sweep validation — that sweep is unpublished
and is very plausibly the same one the shipped constants were fitted to,
so it counts once, not twice. The 4052's on-resistance (a few hundred
ohms against 47 kΩ legs) moves every corner under 1% and stays
unmodelled.

Implementation: the three constants are now written as their closed forms
(59.4083 Hz, 3.35, 1.17616), the one-pole realisation is retained, and
`testPulseWidthAndHighPassLaws` solves the complete branch in complex
arithmetic and holds the shipped shelf within 0.02 dB of it. OQ-21 keeps
only what it always owned beyond this: the coupled C14 interaction, CMOS
switch parasitics, deselected-leg charge memory and mode-change
transients.

### OQ-02 — R32 confirmed

The VCA LEVEL network's least-legible value reads unambiguously as
**R32 1.5K** in the grayscale scan. The nominal law's one transcription
caveat closes; nothing changes numerically.

### OQ-04 — the MN3009 EC-row edition question closes

Three distinct printed editions were retrieved and read at 300–400 dpi
(experimentalistsanonymous scan; a bilingual "MOS IC, LSI" databook scan
via datasheetarchive; the older two-colour Matsushita "BBD SERIES"
databook pp. 36–39 via datasheet4u). The experimentalistsanonymous copy —
the anchoring one — prints **fi min 12 kHz** (fCP 40 kHz, Vi 1.5 Vrms,
3 dB down, 0 dB at 1 kHz); the "fi 14 kHz max, 3 dB down" reading recorded
against it on 2026-08-07 was a conflation with its Features bullet
("fi ≤ 14KHz"). A genuine 14 kHz max row exists only in the older BBD
Series edition, at the hotter Vi = 1.8 Vrms condition; the bilingual
edition prints the same 12 kHz claim in its max column. Both English
editions' typical curves read ≈16.3–16.8 kHz. The shipped
`transferSmear = 0.8654743` anchor (−3.000 dB at 40 kHz/12 kHz) rests on
the correctly read current-edition row and ships unchanged; the
[−4.355, −1.33] dB guard band stands.

### OQ-15 — the p. 13 leg inventory advances; the noise leg gains a strong candidate

- The noise support chain is now designator-complete out to the rail:
  IC14's output crosses the C41 100 pF/R79 330 kΩ load into the internal
  buffer (pins 7→8), then **Tr14's follower (R65 10 kΩ emitter load)
  drives the NOISE SIG rail through C40 10 µF NP against R64 47 kΩ to
  ground** — a ≈0.34 Hz rail coupling, recorded for completeness; the
  33.9 Hz/4.82 kHz band-shaping stands unchanged.
- Per-channel WAVE-line legs (CH2 drawn in full; CH1 mirrors): the sub
  switch emitter reaches the line through R97/R101 27 kΩ behind D5/D6 as
  recorded — and **R99/R102 33 kΩ bridges the same emitter node to the
  line in parallel with the diode**, a resistive DC path the previous read
  did not place. **R98/R103 39 kΩ leaves the WAVE line toward a shared
  bus** — and the follow-up trace closed it: the joined legs descend to
  the ×3 replication tie hooks, and a bus run along the sheet's bottom
  (beside, and distinct from, the labelled RESO CV riser) joins the
  **NOISE SIG rail exactly at the noise circuit's output corner**. Three
  independent facts corroborate the reading: the MC5534's own on/off
  pin 17 carries the saw gate, so a resistor chain has no gating role
  left; every other leg on the mixer node is accounted for; and p. 12's
  designation tables group the 33 kΩ/39 kΩ pair with the wave-line block
  in every channel (R102/R103, R99/R98, R68/R69, R63/R62, R32/R33,
  R29/R28). The earlier "saw-rail chain" interpretation is dead. **The
  noise leg is 39 kΩ**, not the unverified 100 kΩ assumption — a traced
  reading with one caveat: one crossing on the long bottom run sits
  below junction-dot certainty at 300 dpi. Converting the leg into the
  model's level budget still waits on the WAVE output's source
  impedance, which is OQ-15's remaining measurement.
- IC1a's summer is read exactly: R2 3.3 kΩ feedback, six 33 kΩ legs, and
  **R1 3.3 kΩ is TP2's injection resistor** — noise does not enter at the
  summer.

### Acted on the same day — the VCF's fold-back removed at its source

The 2026-08-06 model-internal measurements identified the tanh set of the
VCF cascade as the dominant in-band numerical artefact — discrete lines at
`192 kHz − n·f0` reaching **−55.5 dBc** in the hot bright-resonant case —
and rejected a doubled filter grid that bought 6 dB for 40% of the whole
engine's cost. This pass replaced the mechanism instead of the grid: the
trapezoidal step approximates the integral of each stage's tanh across the
step by its endpoint average, and the implicit solve now takes that
integral **exactly along the straight drive path** — the divided
difference of ln cosh between the previous and current converged drives
(first-order antiderivative anti-aliasing, applied inside the Newton
iteration; the resonance return stays an endpoint evaluation because the
fourth pole has already band-limited it). Offline A/B of the exact
shipping algorithm against the replacement, saw drive at 1046.5 Hz/2.4 V,
16 kHz cutoff, 192 kHz grid: worst folded line **−54.96 → −66.49 dBc**
(k = 3.8) and **−58.14 → −69.40 dBc** (k = 3.4), alias RMS floor down
~11 dB, for +11% cost on the filter alone. Two properties made this safe
to ship without touching any calibrated constant, and both are measured,
not argued: the divided difference degenerates to the endpoint average in
the linear region, so the **small-signal response is identical to six
decimal places**, and the **self-oscillation limit cycle is identical in
amplitude and frequency** (4.7578 Vpp at 221.56 Hz in the fixture, both
variants), so the 4.83 Vpp/248 Hz endpoint solve is untouched. A live
grid change also simplifies: the carried states are now a physical
capacitor voltage and a dimensionless drive, both rate-invariant, so
`retime` re-expresses nothing. `testCascadeDeniesTheFoldback` fences the
hot case below −60 dBc — a bound the endpoint evaluation fails —
and the retime/reference-solve/oscillation tests pin what must not have
changed. This is a numerical product mechanism in the same class as the
BBD host-grid polyBLEP: it removes simulation-grid artefacts the analogue
instrument never had, and it neither resolves nor claims any OQ.

### OQ-03 — a numerical lead on the unexplained 3.95 dB mode delta

Recorded as a lead, not a mechanism. The structural II−I noise delta —
3.95 dB on Panasonic parts and 3.95 dB again on Xvive parts — sits
0.26 dB from the mode-rate ratio expressed in amplitude:
`20·log10(1.6234799) = 4.21 dB`. The settled topology gives the two modes
identical sweep depth and clock range; the *only* thing the mode line
changes is the modulation rate. A noise mechanism proportional to
modulation rate (equivalently to the sweep's slope `d(delay)/dt`, which
mode II raises by exactly 1.6235) would therefore reproduce the measured
delta to within the measurement's plausible uncertainty and would be
chip-population-independent — matching the delta's strongest recorded
property. This does not conflict with the earlier rejection of a
sweep-depth or level difference between the modes; it names a
rate-proportional candidate the requested calibrated capture could
confirm or kill (a same-chain capture at a third, artificial rate would
separate rate-proportional noise from mode-switch-network noise
directly).

### Pages read for the record, without model consequence

The PARTS LIST (pp. 3–4) itemises no discrete resistors — only arrays
(RM-8 103J 10K×8, RM-8 223J 22K×8, RGSD 22K×4, and the **RKM14L503F
R-2R ladder**) and potentiometers/trimmers — so the remaining
component-value asks stay schematic reads, not parts-list lookups. It
confirms "2SC945 Selected For Noise Generator" for Tr21. Page 2's PARTS
CHANGE NOTES record the µPD7810/7811G CPU variants, MC5534 wave-generator
versions and the A1QH800170 → A1QH80017A module change. Page 17's IC DATA
is pinouts and truth tables only — the µPC1252H2 entry is a block diagram
with no electrical curves, and no 4051 on-resistance is given — so it
advances neither OQ-02 nor OQ-07 numerically.

## Settled guardrails — do not reopen without contradictory primary evidence

- **Chorus modes:** the JUNO-106 has Off, I and II. Its owner's manual says I
  and II cannot be used simultaneously, and the board has one enable line plus
  one binary mode line. Obsolete both-buttons session states canonicalise to II.
- **Chorus balance:** dry enters IC6 through 47 kΩ (R71/R73, off the shared
  IC2b bus), wet through 39 kΩ (R72/R74, from the Tr11/Tr12 mute sources),
  with 100 kΩ feedback (R70/R67). Thus dry gain is `100/47`, wet gain is
  `100/39`, and wet/dry is `47/39`, or +1.62 dB — the wet leg is the hotter
  one. Resolved 2026-08-07 by a designator-level read of the Service Notes
  p. 15 scan (synthfool.com copy, First Edition, JUL. 31 1984), agreeing with
  both sibling-board transcriptions; the earlier project reading carried the
  mirror.
- **Voice-summer gain and signal order:** each voice contributes `3.3/33 = 0.1`
  before C14/R39 and the shared high-pass. C12/R36 couples that result into the
  common VCA LEVEL; chorus and IC6 follow, then main VOLUME.
- **C14/HPF established boundary:** placement, populated parts, asymptotic C14
  loads, Boost/Flat endpoints and the 225.8/720.5 Hz cut anchors are settled.
  OQ-21 owns only their full coupled transfer, switch parasitics and mode-change
  memory; it is not permission to discard C14 or refit the established endpoints.
- **Converter ownership and ordinal order:** the 23 used holds are 18 per-card
  DCO/VCF/ENV-VCA destinations plus shared SUB, VCA LEVEL, PWM, RESONANCE and
  NOISE. The service timing chart orders shared RES/VCA/SUB, DCO 1–6, PWM,
  interleaved VCF/VCA 1–6, then NOISE. Hold constants and exact event offsets
  remain OQ-07 and OQ-08 respectively.
  OQ-08 also owns the physical ramp/comparator/sub state forced by a changed-
  pitch timer write; the current bandlimited restart is a declared model.
- **Pulse-off control state:** about -0.8 V pins the comparator output high;
  OQ-11 concerns only what the local DCO-to-voice-mixer coupling does with that
  state, not the now-modelled final C17/C20 output capacitors.
- **POLY 1 + POLY 2:** this is Solo Unison: all six equal-frequency,
  unnormalised, free-running voices. There is no programmed detune or
  normalisation. Highest remaining held key wins the rescan.
- **POLY switches:** they are momentary firmware inputs whose lamps show the
  latched state. Neither-off is not stable, and re-pressing a selected mode
  still clears and rebuilds held-key allocation.
- **Normal chorus output:** dry plus wet. Wet-only output is a documented
  mute-transistor fault.
- **Chorus modulation waveform:** a straight, symmetric triangle. Service Notes
  p. 15 shows IC1b integrating through C3 inside a loop closed by the IC1a
  Schmitt comparator, so the capacitor charges from a constant current for the
  whole of each half cycle. Do not replace it with an RC-relaxation or
  otherwise exponential-segment shape; this circuit is not a relaxation
  oscillator. IC2a inverts it once for the second line, so the antiphase pair
  is one waveform and its negative, not two oscillators.
- **Chorus mode-rate ratio:** 1.6234799, this instrument's own, from the mode
  switch's T-network — R4 680 kΩ runs from the R5/R8 junction to JFET Tr1's
  drain and R3 2.2 MΩ is Tr1's gate-source bleed, so the junction reaches
  ground through R4 alone while Tr1 conducts (mode I) and through R4 + R3
  while it does not — giving effective timing resistances of 6.4352941 MΩ
  (mode I, slower) and 3.9638889 MΩ (mode II). Earlier notes had Tr1
  shorting R3 itself; the p. 15 original-page read relabelled the legs
  without moving a number. Do not reintroduce the JUNO-60's own 1.682
  ratio. The absolute scale is likewise derived from this circuit; OQ-01
  keeps only its hardware confirmation.
- **Mains ripple:** not modelled, by derivation rather than for want of
  evidence. Service Notes p. 16 gives 3300 µF per rail behind a 0.25 A
  secondary and M5230L regulators, so what reaches a card is on the order of
  50 ppm of 15 V — about 0.03 cents of cutoff shift. Rail *droop* is modelled
  and is a different, much larger, DC mechanism. Neither may be routed to DCO
  pitch, which is an integer division of a crystal-derived clock.
- **Chorus support-chain boundaries:** the populated pre/post-BBD topology,
  wet-input 15.9 Hz coupling, nominal component-only 7.23/11.31 Hz wet-output
  coupling, datasheet-fitted nonlinearity, and split zero-order-hold/residual
  charge-transfer loss are settled or derived at the adopted 40 kHz/12 kHz
  ideal-source anchor. That anchor is the raw held node upstream of the
  deterministic polyBLEP; the emitted waveform is no longer a literal
  rectangle. Its residual coefficient is fixed per BBD shift; the removed
  affine clock multiplier double-counted clock scaling. The paper-motivated
  host-grid reconstruction is a resolved numerical product mechanism: it acts
  after transfer loss and before the tap pole, leaves physical BBD/RNG state and
  noise unchanged, and clears only its grid-specific slots at a rate change.
  The two-phase OUT1/OUT2 pair presents one sample per clock period — the
  2026-08-07 solve confirmed the composite is a full-period hold — so it must
  not be modelled as a 2·fcp output stage.
  OQ-04 still owns real MN3009 normalized response and BGA across multiple
  clocks and emitter-follower source loading, including the loaded tap-summing
  transfer; OQ-03 owns stochastic noise and OQ-20 owns TR11/TR12 on-resistance,
  leakage and switching transients.
- **Nominal main VOLUME law:** VR1 is `10KB×2`; Panasonic's later JIS/EIAJ
  table maps plain B to the nominal-linear 1B group. The fixed internal 41.3 kΩ
  selector and 101 kΩ headphone-input loads are modeled. OQ-17 owns real gang
  tracking, selected-tap/output-jack transfer and tolerance, not a return to an
  unsupported generic squared law.
- **Chorus bypass:** Off mutes the wet return only; the oscillator and BBDs
  continue running.
- **Noise amplitude calibration:** the service procedure specifies 4.0 Vpp at
  TP8 via VR32. OQ-16 asks about spectrum and context, not whether that
  calibration point exists.
- **Published nominal output-selector levels:** L -30 dBm, M -15 dBm and H
  0 dBm. OQ-17 owns their reference/load, physical jack normaling/summing,
  one-versus-two-plug transfer and separate headphone characterization—not the
  existence or ordering of these service specifications.

Resolving a task requires updating this file, the matching claim in
`Docs/circuit-modelling-research.md`, source comments/constants and deterministic
tests in the same change.
