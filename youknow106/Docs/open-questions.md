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
schematic pass established C14/HPF routing and parts. Step 15 now models the
selected-leg asymptotic load and qualifies the nominal fixed-position transfer;
switch-device parasitics, deselected-capacitor memory and directed mode-change
transients of the complete network remain open. Step 16 keeps the physical
main-noise corner at 4822.877063 Hz but bounds its TPT design corner to
`min(fc, 0.45 * internal_rate)`, closing an 8 kHz numerical-instability defect
without supplying the raw amplitude/PSD or mixer-drive evidence requested by
OQ-15/OQ-16; both questions remain open.

| Priority | OQ | Question still unanswered | Already settled / do not redo |
|---|---|---|---|
| P0 | 01 | Hardware confirmation of the derived 0.5533/0.8983 Hz rates (TP4 capture), a *calibrated* original-unit sweep capture, and whether the clock is period- or frequency-linear in its CV | Two-line topology, mode controls, the integrator-plus-comparator LFO and its straight triangle, the 1.6234799 mode-rate ratio, the summing-node β = 33/47 and C3 = 0.1 µF (netlist-corroborated), the derived rate scale, and the shipped 1.4–6.4 ms sweep — third-party-scoped on a designator-faithful p. 15 build with genuine MN3009s against a real 106 (2026-08-07), superseding the sibling JUNO-60 capture |
| P2 | 02 | Installed-unit common-VCA endpoint, component/rail/IC variation and residual error against the nominal law | The complete nominal path: `d=b<<5`, ideal R-2R `/4096`, p. 8's +4 to −6 V span, p. 15 R30/C7/R32/R31/R165 network, NEC's −5.9 mV/dB typical law, and C7's 9.08249 ms constant |
| P0 | 03 | Calibrated chorus noise PSD, SNR, spurs and stereo correlation (a capture with a reference tone; the −47.97/−44.01 dBFS floors have a declared but uncalibrated chain), plus the approximate 3.95 dB delta's physical cause | No-compander topology, the MN3009-derived mode-I line floor, the sourced heduhl true-peak floors and the reported approximately 3.95 dB I→II delta (3.96/3.95 dB from the printed pairs); that relative calibration ships as of 2026-08-09 |
| P2 | 04 | Loaded post-BBD support transfer (tap-pole candidates 11.9/15.1/22.2 kHz vs the shipped ideal-source 23.46 kHz), the Gi–fi fixture reading (tracked vs broadband — one tracked ~40 kHz sweep decides, and with it the typical-part residual coefficient inside its recorded [−4.355, −1.33] dB span at 40 k/12 k), and emitter-follower source loading | Component topology with the 106's own p. 15 capacitor codes read at designator level; the two-phase OUT1/OUT2 composite solved as a full-period hold, confirming the shipped shift/hold/polyBLEP structure (2026-08-07); the fixed per-shift residual coefficient anchored to the guaranteed-minimum 40 kHz/12 kHz row, inside every candidate reading's band; Rs ≈ 3.70 kΩ summed-output source impedance and ≈ +1.10 dB intrinsic gain derived from the Gi–RL panel; the digitised 10/40/100 kHz typical family with its self-contradiction between panels recorded |
| P0 | 05 | TA75558S IC6 and High-output clipping swing versus frequency and load | IC6 identity, linear resistor gains and ±15 V rails |
| Dependency | 06 | Physical `Vref_rms` for a declared High-output/load condition | Final -18 dBFS RMS mapping, floating output and no-limiter policy |
| P1 | 07 | Acquisition/tracking behaviour, droop and loading of every converter hold | Hold ownership, 4.2 ms pass, VCF 522 µs and voice-VCA 687 µs anchors, and the designator-complete inventory: all 23 holds are 0.01 µF (p. 13 ".01×7/.01×8"), with per-destination post-hold smoothing networks read in full. The PWM 4.7/2.632 ms cascade, SUB 10 ms pole and common-VCA 9.08249 ms pole ship as derived slews; all supported passive paths evaluate the existing normalized policy event fractionally |
| P1 | 08 | Exact intra-pass timestamps/branches and the physical state forced by a changed-pitch write | Ordinal 23-write queue and normalized compatibility scheduler; exact fractional realization for 16 passive destinations does not promote `ordinal/23` to hardware timing |
| P1 | 09 | Resonance DAC/control voltage to loop gain, compensation and oscillation correction | BA662/IR3109 topology, 4.8 Vpp service trim, shared hold, exact B-2 byte-to-DAC mapping, and the netlist-verified compensation mechanism (lineage divider values recorded, unpromoted) |
| P3 | 10 | Post-calibration six-card and multi-unit residual distributions and thermal drift | Zero-spread nominal policy and optional deterministic Unit Character |
| P1 | 11 | Pulse-Off DC, bleed, loading and switching transient at the voice mixer | About -0.8 V pins the comparator; the final output capacitors are unrelated |
| P2 | 12 | Envelope wall-clock timing/jitter, analogue/audible thresholds and other firmware revisions | Exact hash-scoped B-2 recurrence and physical `E>>2` DAC truncation |
| P2 | 13 | LFO/delay wall-clock timing, analogue smoothing/output scale and revision differences | Exact hash-scoped B-2 rate, delay and fade algorithms |
| P2 | 14 | Portamento pot/ADC transfer, hysteresis, cadence and revision differences | Exact hash-scoped B-2 coefficient and 8.8-state law |
| P0 | 15 | Loaded oscillator/sub/noise mixer levels and their actual filter-drive budget (chiefly the WAVE output's source impedance; the 39 kΩ noise leg and 33 kΩ sub-emitter path are traced readings from 2026-08-07 with one crossing below junction-dot certainty) | Node-specific 12 Vpp/4 Vpp anchors, the 68 kΩ/560 Ω core attenuator, and the mixer topology from the p. 13 read: one summed WAVE output per voice, sub via 27 kΩ + diode, C56/C50 coupling, sources muted at their generators — legs never switch (2026-08-07). Issue 16's one Borish-module unit offers VCA/oscillator capture leads only; it does not retune the nominal laws |
| P2 | 16 | Calibrated raw TP8 capture (PSD/distribution/amplitude against the shaped model), and physical filter-startup excitation | Shared generator topology, TP8 4.0 Vpp adjustment, and the 33.9 Hz/**4822.877063 Hz** band-shaping derived from the p. 13 designators (C42/4.7 kΩ and C41/R79, 2026-08-07). Step 16 numerically caps only the low-pass TPT design corner at `0.45 * internal_rate`, with 4,007-cell/state/mutation qualification; it makes no hardware-PSD claim |
| P3 / dependency | 17 | Real VOLUME gang tracking plus selector, jack, headphone and external-load transfer | Nominal-linear `10KB×2` law and fixed 29.313 kΩ internal wiper load |
| P2 | 18 | Hardware cutoff-converter knee and upper saturation curve | Exponential audio-range law and transparent 50 kHz product cap |
| P1 | 19 | Voice-module BA662 control-current-to-gain curve near cutoff and residual thump after the service null | BA662 pins and separate signal/control paths, ENV/GATE ownership, 6 Vpp endpoint, the per-card minimum-thump procedure, and the anchored +0.25…+0.27 V TP7 control-rail standoff the shipped 150 mV onset now sits relative to (2026-08-07) |
| P2 | 20 | TR11/TR12 wet-mute switching envelope, leakage and click | Device identity, wet-only mute location, continuously running BBDs, and the full gate-drive network designators from p. 15 |
| P2 | 21 | TC4052 parasitics, deselected-Cut charge memory and directed HPF mode-change transient | Parts, placement and control are settled: C14 feeds TC4052BP YCOM with R39 to ground, Tr3 drives INH rather than audio, Flat is R27 and Boost is R25. Step 15 includes the selected R21/R23 1 MΩ load: `R39 || 1 MΩ = 31,945.788964 Ω`, `τ = 319.457890 ms`, `fc = 0.498203201 Hz`. Independent nominal fixed-mode MNA bounds the scalar cascade at 0.011137 dB/0.056092° or below. The established 225.8/720.5 Hz selected Cut corners and 15.705/4.9209 ms deselected decays stand. The mode→tap map and Boost shelf are settled too: +10.50 dB/+1.41 dB/59.41 Hz, within 0.016 dB of the exact two-zero/two-pole solve. The withdrawn band-boost branch must not return |

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

A supplied five-page summary report dated 2026-08-07 is reconciled in
[Supplied-report reconciliation pass — 2026-08-07](#supplied-report-reconciliation-pass--2026-08-07-summary-compilation-checked-no-promotion-imported).
It restates this queue's own constants to their full recorded precision,
attaches no capture and cites no new source, so it corroborates nothing and
closes nothing; its promotions — the chorus sweep to anchored, the lineage
compensation slope to derived, the voice-VCA onset to derived and OQ-21 to
resolved — are declined there.

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

**2026-08-19 lead — the triangle ships symmetric, and its asymmetry rides on the
capture already requested.** `Chorus` derives the modulator as an IC1b
integrator charged at constant current for the whole of each half cycle against
IC1a's two-resistor Schmitt, and ships "a straight, symmetric triangle". That
symmetry assumes the comparator's two saturated output levels are equal in
magnitude. A real µPC062 does not saturate symmetrically, and in this topology
both the charging current *and* the trip threshold derive from that same
saturated output, so the imbalance appears twice:

    T_fall / T_rise = V_OL / V_OH ,

with the triangle spanning −β·V_OH to +β·V_OL rather than a centred ±β·V_sat. A
2% rail imbalance buys about 2% of half-period asymmetry and roughly −0.1 V of
offset on a ±9.6 V triangle — an up-swing and a down-swing of different
durations, which in the delay domain is the same "asymmetric dip and swoop"
character the frequency-linear sweep hypothesis reaches for by a different
route. The total period also stops being `4·β·R_eff·C3`, so the rate scale and
the asymmetry are not separable.

Nothing is implemented from this, and the magnitudes above use invented rails:
no loaded µPC062 saturation figure for this board is in tree, and a chosen pair
would set both the asymmetry and a correction to the derived rate scale at once
— the "draw a curve" failure this queue exists to prevent. What makes it worth
recording is that it costs no extra measurement. The raw multi-cycle TP4 capture
already first on the needed-output list below shows half-period inequality and
triangle offset directly, and the paired TP3 capture bounds IC2a's R9/R10
inverting gain — a 1–2% resistor pair is ±2% of line B's modulation depth, about
43 µs on a ±2.13 ms swing, against the 16 µs RMS residual of the only trajectory
series in existence. One capture settles the sweep law, the rate scale, β, the
up/down asymmetry and the two lines' depth match together.

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
not a substitute for measurement. The dated before/after corpus exercised
bytes 0, 32, 64, 96 and 127 plus rapid transitions, dry and through Chorus II,
with exact automation and shared listening gain; it measured the
implementation change, not an original unit. Those files were retired in the
2026-08-09 audio reset; the [current audio index](audio/README.md) owns
replacement renders.

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
No compander exists in this circuit, so a noise model is structurally required.
A noise voltage without a same-path reference tone, bandwidth and weighting does
not establish SNR.

*Implementation note, 2026-08-08; numerical transfer updated in Step 9 on
2026-08-09:* the per-line floor is **no longer voiced**. It is the MN3009's own
noise row — 0.2 mVrms max, A-weighted — from the same datasheet this model
already treats as anchored for bandwidth and distortion. After the exact
continuous output-support transition, its derived A-weighted transfer is
0.4026 = 1/√3 × 0.6973 and the injected amplitude is 1.9106577e-4 model
units. With the separate instrument-output factor divided out, the recovered
mode-I/mode-II wet lines are 0.200059/0.200078 mVrms at 176.4 kHz and
0.200006/0.200020 mVrms at 192 kHz, a cross-HQ spread below 0.004 dB. The
pre-calibration model measured 1.0488 mVrms, 14.39 dB hot. Three things
this does **not** settle, all of which the capture below still owns.

1. **The datasheet brackets rather than fixes the figure.** Its two noise rows
   disagree by **10.5 dB**: 0.2 mVrms *max* A-weighted against the ~59.7 µVrms
   implied by S/N 88 dB *typ* at the 1.5 Vrms maximum input. The guaranteed
   maximum ships, because it is the guaranteed figure and because anything near
   the other end is close to indistinguishable from the dry path's bit-exact
   zero — but that is a choice inside the bracket, not a derivation from it. A
   calibrated capture would say where in the 10.5 dB a real card sits.
2. **The node the row is landed on is a reading.** The figure is placed on the
   recovered wet line as it arrives at IC6, i.e. after the board's
   reconstruction sections; the injection node's own unweighted RMS is 3.12 dB
   above that. Which node a BBD datasheet's noise row denotes depends on its
   test circuit, which is not in tree.
3. **The base mechanism remains deliberately narrow.** It is still one
   edge-held uniform random per line, and the optional common/correlated, hum
   and clock-spur layers remain zero-amplitude hypotheses. The relative mode
   calibration below changes none of those spectra or correlations.

The Step-9 numerical check also keeps grid error visible. With HQ off, the
mode-I/mode-II recovered pairs are 0.208558/0.208917 mVrms at 44.1 kHz,
0.206982/0.207251 at 48 kHz, 0.201452/0.201763 at 88.2 kHz and
0.201218/0.201584 at 96 kHz: about +0.05…+0.38 dB above the datasheet row.
Five PSD-band changes against Step 8 remain within 0.147 dB and unweighted RMS
within 0.070 dB. Those are numerical/model measurements, not a calibrated
hardware spectrum, so they narrow no part of the capture request below.

*Implementation note, 2026-08-09:* the usable same-chain observation now ships
directly: mode I keeps the part-derived floor, and mode II's edge-held line
contribution is multiplied by `10^(3.95/20) = 1.575796`. The source used
iZotope RX true peak over unspecified noise-only selections; its printed pairs
yield 3.96 and 3.95 dB, so translating the reported approximately 3.95 dB into
a broadband amplitude/RMS factor is explicitly moderate-confidence policy. It
is an empirical **complete-instrument output** calibration, not a claim that a
standalone mode-II MN3009 exceeds its datasheet row; the exact physical
insertion point is unknown. The earlier rate-proportional candidate remains available internally
as `useChorusRateNoiseHypothesis`. It substitutes the instrument's 1.6234799
mode-rate ratio (4.2089 dB) instead of multiplying both profiles, keeping mode I
bit-identical so a third-rate capture can still falsify it. A useful measurement
caution remains: each line writes one noise sample per bucket edge, so its
instantaneous floor rides the swept clock, and a fixed window that is not a
whole number of modulation cycles reads mode II 0.69 dB hot even in a diagnostic
with both mode factors divided out. Any capture compared against this model must
average whole cycles.

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
documented rather than given an inaudible retune.

*Scoped 2026-08-07 (doc↔code audit pass): those two figures are the **analytic
law** — full-period hold times one pole per shift — and that is what the
circuit fixture asserts, by driving `transferLossStep` on a synthetic 40 kHz
grid against an analytic aperture. `processClockedCore` was never entered by
it. At that dated checkpoint the shipped 176.4 kHz path read about −3.15 dB
versus DC; the ≈0.13 dB residual came from the current/previous linear sampler
that placed the host-grid input on the clock edge. Its historical 12 kHz loss
averaged 0.11 dB on a 192 kHz grid, 0.45 dB on 96 kHz and 1.81 dB on 48 kHz.
Superseded 2026-08-09: each edge now evaluates a causal four-point Lagrange
polynomial through the current post-support sample and its three predecessors.
There is no future sample, lookahead or added latency, and this is not a
fractional BBD output read-tap or Thiran allpass. No constant moves:
`transferSmear` stays 0.8654743, the analytic raw-node anchor stays −3.000 dB
versus DC/−2.972 dB versus 1 kHz and the recorded [−4.355, −1.33] dB
cross-reading guard band remains. This numerical correction supplies no new
physical bandwidth evidence and does not close OQ-04.* The former additional factor
`1+(clock−26000)·1.5e−6` double-counted
that scaling: it gave −2.757 dB versus DC, or −2.732 dB versus 1 kHz, and swept
the normalized 0.3-cycle response from about
−3.04 to −2.14 dB over the modelled 23.9–77.1 kHz range. Removing it fixes an
implementation inconsistency and unsupported LFO-correlated brightness; it
does not quantitatively establish the real MN3009's transfer at other clocks.
The datasheet itself contains low-resolution typical `Gi-fi` curves at fCP 10,
40 and 100 kHz. *Superseded by the later 600-dpi pass:* extraction is complete
and exposes incompatible tracked and broadband readings; calibration and an
installed-unit tracked sweep remain, not a total absence of multi-clock evidence.

Those figures describe the raw held node before numerical output
reconstruction. The emitted deterministic step now receives a compact polyBLEP
after residual transfer loss and before the tap-summing pole, so the emitted
waveform is not a literal rectangle. This simulation-grid correction does not
resolve the physical MN3009 output impedance, normalized transfer, BGA or loaded
support network requested here; those remain OQ-04. Measurements must therefore
identify whether they address the raw held model, the reconstructed emitted
node, or an installed device, and de-embed the numerical reconstruction when
comparing the first two.

**2026-08-09 common-host numerical baseline — implementation check, not
closure.** `YouKnow106VcfBbdQualityAudit` compares one deterministic modeled
line at 44.1/48 kHz × 1×/2×/4× with a closed-form reference: the declared
continuous input/output component responses, exact 128-edge sequence delay,
fixed per-edge transfer-loss pole and full-period zero-order-hold image phasors,
followed by an independently designed fixed-16×, 4,097-tap host-boundary FIR.
The low coherent drive `A=0.02` keeps the fitted BBD saturation within a fenced
`1e-6` linearization bound; cases cover 20, 50 and 91.429 kHz clocks at a low
tone plus 50 kHz clock/12 kHz tone. This oracle independently evaluates the
same declared component and fitted-transfer anchors. It is not a second
hardware fit, an installed-unit measurement or evidence for selecting among
the unresolved loaded poles in this question.

The SGA measure is the exhaustive 20 Hz–20 kHz unmasked Blackman–Harris FFT
maximum across all four cases after masking only the fundamental and
analytically validated wanted physical-image lines at or below 20 kHz. Sources
above 20 kHz do not receive an unmeasured BGA exemption. Exactly one
non-fundamental wanted image clears the projection threshold in each cell, so
the physical-image column is that qualifying line's error rather than an
exhaustive image population. The reviewed matrix is:

| Host | Analytic NRMS, 1× / 2× / 4× | Qualifying-line physical-image gain error, 1× / 2× / 4× | 20 Hz–20 kHz unmasked SGA, 1× / 2× / 4× | Result |
| ---: | ---: | ---: | ---: | --- |
| 44.1 kHz | −3.099 / −14.910 / −27.045 dB | 34.389 / 4.088 / 0.867 dB | −24.854 / −28.762 / −47.635 dBc | **all REJECT** |
| 48 kHz | −4.640 / −16.426 / −28.181 dB | 22.893 / 3.257 / 0.708 dB | −28.871 / −31.329 / −38.189 dBc | **all REJECT** |

The gates are ≤−40 dB analytic NRMS, ≤0.75 dB physical-image gain error
and <−60 dBc SGA. The oracle's exhaustive 20 Hz–20 kHz off-mask controls are
−93.046/−135.607 dBc and its post-FIR image-tail bounds are
−198.030/−202.098 dBc at 44.1/48 kHz; clock-phase, exact edge-state, projection,
finite-value and independent-filter controls also pass. Four-times remains a
candidate, not truth, and every BBD rate is unadmitted.

**2026-08-09 Step-8 causal input-edge rerun — SGA submetric passes, domain
still rejects.** The table above is retained as the Step-7 before-state. The
only production signal-path change is current-plus-three-past four-point
Lagrange sampling
of the post-support input at each existing edge in place of the two-point
linear sampler. It is causal and has no future sample, lookahead, added
latency, support-filter change, physical-constant change or noise-path change.
The deterministic output polyBLEP also remains unchanged. Using the same
oracle, cases and gates, the before → current matrix is:

| Host | Factor | Analytic NRMS, Step 7 → Step 8 | BGA error, Step 7 → Step 8 | SGA, Step 7 → Step 8 | Result |
| ---: | ---: | ---: | ---: | ---: | --- |
| 44.1 kHz | 1× | −3.099 → −3.602 dB | 34.389 → 34.362 dB | −24.854 → −26.765 dBc | **REJECT** |
| 44.1 kHz | 2× | −14.910 → −18.159 dB | 4.088 → 4.080 dB | −28.762 → −41.304 dBc | **REJECT** |
| 44.1 kHz | 4× | −27.045 → −30.394 dB | 0.867 → 0.865 dB | −47.635 → **−72.041 dBc** | **REJECT** |
| 48 kHz | 1× | −4.640 → −5.768 dB | 22.893 → 22.866 dB | −28.871 → −30.364 dBc | **REJECT** |
| 48 kHz | 2× | −16.426 → −19.696 dB | 3.257 → 3.249 dB | −31.329 → −45.866 dBc | **REJECT** |
| 48 kHz | 4× | −28.181 → −31.847 dB | 0.708 → **0.706 dB** | −38.189 → **−65.597 dBc** | **REJECT** |

At q4 the <−60 dBc SGA gate now passes at both hosts, and the 48 kHz BGA
error remains inside its ≤0.75 dB gate. Analytic NRMS still misses ≤−40 dB in
every cell, while 44.1 kHz/q4 BGA remains outside at 0.865 dB; all six rows
therefore remain overall **REJECT**. With the component support chain held
fixed, the remaining q4 discrepancy is support/grid limited rather than
permission to retune an MN3009 constant. OQ-01 (clock/delay), OQ-03 (noise),
OQ-04 (loaded physical transfer/support) and OQ-20 (wet switching) all remain
open.

**2026-08-09 Step-9 continuous support rerun — the bounded low-drive fixture
passes on every shipping HQ path; the physical question stays open.** The input
and output component networks are
now each one six-state continuous system. A prepare-only 10×10 augmented
matrix exponential gives the exact transition under the declared causal cubic
through the current and three past samples. Output uses it at every rate; input
uses it at internal rates ≥176.4 kHz and retains Step 8's TPT path below,
where the exact cubic input drive worsened SGA. It adds no future sample,
lookahead or latency and changes no component, BBD, clock or selector constant.

| Host | Factor | Analytic NRMS | BGA error | SGA | Absolute result |
| ---: | ---: | ---: | ---: | ---: | --- |
| 44.1 kHz | 1× | −3.511 dB | 4.764 dB | −26.934 dBc | **REJECT** |
| 44.1 kHz | 2× | −18.390 dB | 0.070 dB | −41.304 dBc | **REJECT** |
| 44.1 kHz | 4× | −53.442 dB | 0.011 dB | −71.831 dBc | **PASS** |
| 48 kHz | 1× | −5.263 dB | 3.406 dB | −30.746 dBc | **REJECT** |
| 48 kHz | 2× | −20.051 dB | 0.016 dB | −46.044 dBc | **REJECT** |
| 48 kHz | 4× | −56.101 dB | 0.008 dB | −65.381 dBc | **PASS** |

The shipping-policy extension passes that same four-case low-drive fixture on
all six actual HQ paths:
44.1 kHz/4× −53.442/0.011/−71.831,
48 kHz/4× −56.101/0.008/−65.381,
88.2 kHz/2× −50.700/0.011/−71.832,
96 kHz/2× −51.863/0.008/−65.382,
176.4 kHz/1× −53.481/0.011/−71.832 and
192 kHz/1× −56.079/0.008/−65.381 (NRMS dB/BGA dB/SGA dBc).
HQ-off at 44.1/48/88.2/96 kHz remains an absolute failure but passes only the
frozen Step-8 nonregression allowances. Exact integration therefore resolves
the warping measured by this bounded HQ fixture; it does not measure the real
loaded transfer, select among the unresolved source impedances, prove the adopted
component boundary or close OQ-04.

This matrix characterizes only the current deterministic model boundary; it
cannot close the real MN3009 normalized transfer, BGA, source impedance or
loaded time-varying support chain. OQ-04 therefore remains open at P2.

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

**2026-08-09 numerical-grid baseline — implementation check, not closure.**
`YouKnow106.DcoScanQualityContract` advances the current DCO, two-pole PWM and SUB
recurrences at 44.1/48 kHz × 1×/2×/4× and compares them with their closed-form
continuous exponentials. All six grid cells clear the declared absolute-error
gates. At the Step-6 checkpoint, this was only the hold-law subcheck: the DCO
spectrum in all six of the same cells rejected its separate −70 dBc quality
gate. Step 7 subsequently corrected interpolation across the numerical step
residual's event-side jump, and all six spectral cells now pass; the scan/hold
results did not move. Neither result supplies evidence for acquisition windows,
droop, charge injection, continuous-versus-track-and-hold behaviour, or the
voiced DCO/RESONANCE/NOISE constants. This question remains open at P1.

**2026-08-09 Step-11 numerical-policy update — still not closure.** The engine
now evaluates the existing 522 µs per-card VCF and shared RESONANCE holds at
their fractional `NormalizedServiceChart` events. It purely peeks an event in
`(phase, phase + delta]`, latches that policy-time payload, uses exact
piecewise-exponential endpoints and seven Merson-node values, then lets the
ordinary scheduler commit the latch once at its next poll. This removes
host-grid event snapping from those two modeled paths. It does **not** establish
whether 522 µs is an acquisition or settling constant, whether a physical hold
tracks continuously, what it does while disconnected, or the size of droop,
loading and charge injection. The DCO/NOISE 522 µs values and every other hold
retain their declared paths. OQ-07 remains open at P1.

**2026-08-09 Step-12 numerical-policy update — still not closure.** The same
pure peek/latch/once-only-commit mechanism now classifies 16 passive
destinations per pass: RESONANCE, common VCA, SUB, PWM, six VCF and six
VoiceVca. Step 11's VCF/resonance path is unchanged. The six 687 µs
VoiceVca states, derived 9.08249 ms common-VCA pole and 10 ms SUB pole advance
by exact old/new-target segments; the 4.7/2.632 ms PWM cascade uses its exact
continuous affine two-pole transition. Their physical states use `double`
without adding a state. Pitch/DCO remains sample-grid because the timer write's
physical consequences belong to OQ-08, and NOISE remains sample-grid because
its hold/source law is unresolved here and under OQ-15/OQ-16. This is exact
evaluation of an existing model schedule, not acquisition, droop, injection or
disconnected-hold evidence. OQ-07 remains open at P1.

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

**2026-08-09 model baseline — characterization, not closure.** At 48 kHz,
`testNoteOnPlayingLatencyAcrossConverterPhases` now advances all 1,008 distinct
host/scan boundaries and selects each of the six physical cards through Poly-1
note memory. For the declared two-second-pre-rolled C4 saw/ENV fixture,
event-to-Pitch-write is 0/100/201 samples (min/median/max), event-to-VoiceVca
target and first nonzero model gain is 70/192/315, and 63.2% of the held target
is reached at 102/224/347 HQ-off or 103/225/348 HQ-on. The raw first stereo
sample above `1e-4` is 90/213/335 or 93/216/339. The plug-in's fixed 24-sample
host latency report is numerical group-delay bookkeeping and is not folded into
those scan/hold figures. HQ evaluates four substeps per host sample while HQ-off
evaluates one; at exact write boundaries the same host-indexed event can catch
different passes, so the worst paired raw proxy differs by 205 samples despite
closely aligned aggregate distributions. This baseline characterizes
`NormalizedServiceChart`; it supplies no physical offset, acquisition, phase
origin or audible-threshold evidence. OQ-07, this question, OQ-12 and OQ-19 all
remain open.

That paragraph is the Step-3 pre-reconstruction baseline. Step 7 leaves its
Pitch, VoiceVca and 63.2% hold distributions unchanged, but H=24 reconstruction
and the 95-tap half-band move the signal-dependent raw output proxy to
87/210/335 HQ-off and 105/228/351 HQ-on. The fixed numerical report is now 41
samples; the corresponding nominal compensation coordinates are 46/169/294 and
64/187/310. Raw 1×/2×/4× centres plus integer pads are 24+17=41,
35.5+6=41.5 and 41.25+0=41.25, all within 0.5 sample of the report. The −80
dBFS crossing is signal dependent and responds to factor-dependent symmetric
pre-ringing, so neither the raw movement nor subtracting 41 supplies hardware
timing.

**2026-08-09 rate-grid baseline — numerical policy, not closure.** A separate
`YouKnow106.DcoScanQualityContract` subcheck samples the normalized
`ordinal/23` schedule at 44.1/48 kHz × 1×/2×/4× on
each internal grid. It observes exactly 23 ordered model writes per pass over
50 complete 44.1 kHz passes and five complete 48 kHz passes, with zero frame
mismatch and less than one internal-sample interval of quantisation. Its
DCO/PWM/SUB hold checks also match the current recurrence laws. Those
references encode `NormalizedServiceChart` and the compatibility constants
already under test; they provide no original-unit sub-pass timestamp,
acquisition, jitter or pitch-restart evidence. The same audit's DCO spectral
cells all reject their separate absolute quality gate, so these passing
schedule/hold subchecks are not DCO-domain admission. OQ-07 and this question
remain open at P1.

The final two sentences above preserve the dated Step-6 state. Step 7 changes
only its numerical DCO classification: all six spectral cells now pass after
the step-response/event-side repair, while the 23-write, quantisation and hold
measurements remain the same. That numerical pass is still not an original-unit
timestamp, acquisition or restart capture. OQ-07 and this question remain open
at P1.

**2026-08-09 Step-11 fractional-event update — compatibility policy, not
timing evidence.** The normalized `ordinal/23` offsets and exact 23-write order
are unchanged. For VCF and shared RESONANCE only, production now sees an event
inside `(phase, phase + delta]`, including resonance across a pass wrap, before
the sample-grid poll. A pure peek preserves the official cursor and visible
target, latches the policy-time converter payload against later automation, and
the next normal poll commits it exactly once. This makes the continuous hold
evaluation agree with the timestamps the compatibility profile already
declared; it neither measures nor infers the actual JUNO-106 offsets. Every
absolute timestamp, jitter/data dependency and changed-pitch restart behavior
requested below remains unknown. OQ-08 remains open at P1.

**2026-08-09 Step-12 fractional-event update — compatibility policy, not
timing evidence.** Production now applies the same fractional mechanism to all
16 supported passive writes: RESONANCE/common VCA/SUB/PWM and six VCF/six
VoiceVca. The six Pitch/DCO writes remain sample-grid specifically because this
question has not established what an 8253 write forces in the ramp,
comparator or sub-divider, and NOISE remains outside the fractional set under
OQ-07/OQ-15/OQ-16. At 48 kHz the official VoiceVca target commit remains
70/192/315 samples, while HQ-off can realize its fractional physical write and
first gain at 69/191/314; the fixed numerical host report remains 41 samples.
Neither coordinate identifies an original-unit timestamp or audible latency.
OQ-08 remains open at P1.

**2026-08-19 lead — what an 8253 write forces, and the unison comb it implies.**
This question already asks what the 8253 write and surrounding DCO circuitry
actually force, and at which edge. A mechanism answer is available from the part
rather than from the instrument, and it points away from the model's current
choice.

An Intel 8253/8254 counter in either periodic mode does **not** restart on the
write. In Mode 2 and Mode 3 a count written while the counter is running leaves
the counting sequence in progress alone; the new value is taken from the buffer
register at the end of that cycle — the counter's own terminal count — and only
the following period has the new length. Mode 0 is the contrasting case, where
the write does disturb the count in progress. If the 106 programs its note
timers in a periodic mode, then a changed-pitch write cannot force a ramp reset
*at the write*, and the reset edge is each channel's own next terminal count:
six edges scattered by whatever the six counters had left to run, not six edges
laid out on the converter's write grid.

`restartDcoBandlimited` currently does the opposite. It zeroes `dco.phase` and
forces the comparator and sub-divider states at the instant of that voice's
Pitch write, and `converterEventPhases` places those writes on a uniform
`ordinal/23` grid at consecutive ordinals 3–8. A six-voice Solo Unison restart
therefore lays six identical ramps down at 4.2 ms / 23 = 182.6 µs spacing, and
because all six divide the same reference by the same integer those offsets
never decay: the stack sums as a fixed six-tap comb whose first null sits at
1 / (6 × 182.6 µs) = 913 Hz and stands for as long as the note is held. The
restart is only reached on a pitch change to a card that is not already running
(`initialiseVoice`), so held repeats and legato already keep the free-running
behaviour the claims boundary describes; it is the changed-pitch case that is
at issue.

Why nothing moves yet. The reload-at-terminal-count behaviour is reported
consistently by secondary restatements of the Intel datasheet, but the datasheet
itself could not be retrieved in the environment this note was written in, so it
is recorded as a **lead** rather than as the component-datasheet anchor the
taxonomy would otherwise allow. More decisively, **which mode the 106 programs
is not established** — neither the service notes nor the A-5/B-2 images in hand
have been read for the timer control word — and the mechanism only bites in the
periodic modes. Nor is it settled that the 8253 output edge resets the ramp
integrator rather than merely clocking it. Acting now would replace one
unmeasured restart law with another, so the uniform grid stands.

What would settle it: the control word the firmware writes to the timer, which
a read of the initialization path in the supplied B-2 image would give; and a
capture of two or more cards' ramp outputs across a changed-pitch write, showing
whether the resets land on the converter's write spacing or on the counters'
residual counts. The second also answers whether the 913 Hz comb above exists on
hardware, which is the audible half of this question.

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
2.0 V sub (5.0 until 2026-08-19) and 7.4161 V noise, then a `0.40` filter-input
scale. The noise figure
looks out of family only because it is the one coordinate applied *before* a
shaping stage: it delivers +/-2 V at the shaped rail, which is where the TP8
adjustment measures, having lost 11.383 dB to the C41/R79 pole on the way (see
OQ-16, level settled 2026-08-17).

What that settlement does **not** decide is placement. The paired TP8
adjustments constrain the *product* of the noise coordinate and this `0.40`
filter-input scale, and the self-oscillation they are referred to is generated
inside the filter, downstream of both. Attributing the whole 13 dB to the noise
leg rests on the deficit coinciding with a noise-only mechanism -- the shaping
loss -- to within the measurement band, plus the fact that moving the shared
`0.40` would drive every tonal source that much further into the OTA
non-linearity and re-voice the instrument. That is reasoning, not evidence, and
it is exactly what a measured source-to-VCF-input budget would replace. The sub amplitude
and complete node-to-node transfer are not explicitly anchored. A centered
`+/-6 V` source is compatible with a 12 Vpp reading only at the same stated
node; it does not prove the later numerical coordinate. TP8 is downstream, so
its 4.0 Vpp noise adjustment cannot directly establish a `+/-2 V` pre-filter
noise amplitude or distribution. Treat the sub coordinate, `+/-2 V` noise and `0.40`
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
four-switchable-100 kΩ-legs Thévenin model is superseded.

*Narrowed again 2026-08-07 by the complete-scan pass below, which resolves
the 33 kΩ/39 kΩ chain this paragraph used to list as one unknown: the
"chain toward the saw on/off rail" reading is **dead** — the MC5534's own
pin 17 carries the saw gate — and the pair separates into R99/R102 33 kΩ,
a DC bridge from the sub switch emitter to the WAVE line in parallel with
D5/D6, and R98/R103 39 kΩ, the noise leg, traced to the NOISE SIG rail at
the noise circuit's output corner (one crossing below junction-dot
certainty at 300 dpi). The unverified per-voice 100 kΩ assumption is
retired with it.*

Still open here: the WAVE output's source impedance, the exact termination
of the summed node, and the loaded level budget those would close — the
33 kΩ bridge and the 39 kΩ leg cannot be converted into the model's level
budget until that impedance is measured. **C56/C50 are no longer among the
unimplemented parts:** the per-voice module-input coupling ships as of
2026-08-07 (see the implementation note under the complete-scan pass), with
its capacitance read and the resistance it works against still voiced.
Provenance note: the
KR-106 "measured sub/pulse ratio 1.51" recorded by the 2026-08-06 mining
pass could not be re-located in that project's current tree (its own engine
mixes sub at 0.67 against 0.5 waves, ratio 1.34, as engine constants, not
measurements), so that lead is downgraded until raw provenance surfaces.

**Quantified contradiction, 2026-08-18 (external recording, not promotable):**
the [external sound validation](external-sound-validation.md) compared all
128 factory renders against synthmania.com's per-patch hardware recordings
(undocumented unit and chain). On the noise-forward drum programs the
noise-versus-sub balance inverts: A64 Snare Drum (saw/pulse off, key follow
0, noise 91, sub 64) is noise-dominated on the hardware recording (centroid
~2.7 kHz, broadband to 8 kHz) while the model renders the noise leg
9.7–10.8 dB **below** the sub leg — a ~13–16 dB inversion that survives
register matching and cannot be a chain tilt, because both legs share one
take and the corpus-wide chain tilt is only −8.2 dB median. A65 shows the
same signature, A66 a milder one behind its more closed filter, and the
sub-only A83 passes clean, which points the residual at the unanchored
`subMixVolts = 5.0` rather than the ±~4 dB-anchored noise leg. The direction
is solid; the magnitude inherits the recording chain's uncertainty.

**2026-08-19 — `subMixVolts` re-voiced 5.0 -> 2.0 on the owner's decision to act
on real-unit factory-preset discrepancies. OQ-15 stays open.**

The pass above recorded the contradiction and retuned nothing. That was a
judgement call rather than something this contract compelled: `subMixVolts` has
always been labelled **voiced** -- "chosen inside a range the sources bound but
do not fix" -- so changing it never required an exemption. What the contract
forbids is *promoting* such a number to anchored or measured, and nothing here
does that. It remains voiced, and this task's measured budget is still what
would settle it.

What the change rests on, measured on the shipping engine rather than inferred:

- With both legs at full slider, no saw or pulse, a flat envelope and the filter
  wide open, the noise leg rendered **13.3 dB below** the sub leg. The figure
  moves less than 0.1 dB across MIDI 36/48/60 and under 1 dB from cutoff 1.00 to
  0.85, so it is the mix constants and not a patch or filter artefact. This is
  the "off by more than triple that band" figure above, now measured directly.
- At A64's own slider values the model rendered noise **10.10 dB** under sub,
  reproducing the 9.7-10.8 dB this task already recorded.

The correction goes on the sub leg because it is the coordinate with no
end-to-end anchor, while the noise leg is tied to the 4 Vpp TP8 adjustment
within about 4 dB *and* is derived (`2.0 / sqrt(0.0727330)`) rather than chosen.
Closing the gap from the noise side would break an anchor to fix an unanchored
number.

2.0 takes 7.96 dB of the 10.10, leaving A64 at **-2.26 dB** -- balanced within
the corpus's own 8.2 dB median chain tilt, and deliberately short of the full
inversion the recording implies, because the magnitude inherits an undocumented
MP3 chain's uncertainty even though the direction does not.

Bank cost, measured rather than assumed, as the level change on each program:

| program | sub byte | change |
| --- | --- | --- |
| A83 Drum Booms (1 oct. down) | 46 | **-7.80 dB** |
| A12 Brass Swell | 70 | -1.40 dB |
| A21 Organ I | 23 | -0.16 dB |
| A11 Brass Set 1, B11 Strings | 0 | none |

Most of the bank is untouched: a program either has `sub = 0` or has its sub
masked by saw and pulse. A83 is a sub-feature patch and takes the whole cost.

**What this does not fix, and why it is not this constant's to fix.** A65 Tom
Toms and A66 Timpani remain sub-dominant afterwards at -9.19 and -7.07 dB. No
single scalar here reconciles the family: A64 needs 0.30x, A66 0.18x and A65
0.10x. Both run much more closed filters than A64, which attenuates the
broadband noise leg far more than the low sub -- the same reading this queue
already gave A66. Their residual is a filter-and-level interaction and stays
open here. A67 Shaker and A86 Hand Claps carry `sub = 0` and cannot constrain
this coordinate at all, so the "re-check as a family" instruction above is
narrower than it looked: only A64/A65/A66 have both legs.

**2026-08-19, same pass — a supplied circuit study does not account for the
gap, and that is the useful part of it.** A grounded research note was reviewed
against this queue. Its topology agrees with the 2026-08-07 designator read and
adds nothing this task disputes: sub through R101/R97 27 kOhm behind D5/D6 with
the R99/R102 33 kOhm bridge, noise through R98/R103 39 kOhm, both into the
module's 68 kOhm input, C56/C50 coupling, the 33.86 Hz and 4822.88 Hz noise
corners, and the 4 Vpp TP8 adjustment. Its conclusion, that modelling those legs
plus `subMixVolts = 3.5` "perfectly restores the balance on A64, A65, A66 and
A67", does not follow from its own numbers:

| mechanism it proposes | effect on the noise-versus-sub balance |
| --- | --- |
| per-leg dividers, sub 68/(68+27) vs noise 68/(68+39) | **+1.03 dB the wrong way** |
| the same with its 27k\|\|33k conducting-half reading | **+2.22 dB the wrong way** |
| `subMixVolts` 5.0 -> 3.5 | -3.10 dB |
| unipolar diode law at A64's sub byte 64 | -1.10 dB |
| **net** | **about -3.2 dB, leaving A64 near -7 dB** |

The sub leg has the *lower* series resistance, so its own per-leg dividers make
the sub relatively louder and widen the discrepancy they were offered to close.

Two things follow. First, the per-leg dividers are **not** implemented, and not
only because they point the wrong way: computing them needs the WAVE output's
source impedance, which is precisely what this task lists as unresolved. The
study assumes the node is terminated by the 68 kOhm module input alone; a WAVE
output with low source impedance loads that node and moves every divider. This
is the error class the tree already committed once, when a revision modelled
four switchable 100 kOhm legs against the module's 68 kOhm and took a phantom
1.76 dB from every patch with both waveforms on.

Second, and more usefully: **no circuit mechanism yet identified accounts for
the measured 13.3 dB.** Everything on the table above reaches about 3 dB. That
bears directly on the `subMixVolts = 2.0` shipped above, which is a fit to the
recording and not a derived value -- roughly 10 dB of the gap has no circuit
explanation at all. Either the magnitude the undocumented MP3 chain implies is
overstated, or the cause is somewhere this task has not looked.

**Corrected the same day:** this note first named OQ-16's noise amplitude
distribution as the strongest remaining candidate. It is not, and the arithmetic
rules it out rather than supporting it. The shipped source is bounded-uniform
and delivers 1.155 V RMS at full slider. If the hardware generator is Gaussian,
as an avalanche source is, and the 4.0 Vpp TP8 figure is a visual read of that
trace, its RMS is 1.000 V at +/-2 sigma, 0.667 V at +/-3 sigma and 0.500 V at
+/-4 sigma -- the model is 1.25 to 7.27 dB *louder* than hardware, not quieter.
Correcting the distribution would therefore push A64 further sub-dominant.

That leaves every mechanism examined so far pointing the wrong way or too small:
per-leg dividers +1.03 dB wrong way, Gaussian crest +1.25 to +7.27 dB wrong way,
the diode law -1.10 dB and `subMixVolts` 3.5 -3.10 dB right way. The remaining
possibilities are narrower than they looked: either the sub leg's real amplitude
at the mixer node is far below the modelled coordinate -- which is exactly the
measurement this task asks for and nothing else substitutes for -- or the A64
reference recording is not the stock patch it is being read as. Until one of
those closes, the shipped coordinate stays voiced and this question stays open.

**2026-08-19, second research submission — adjudicated, nothing promoted.**
A findings document was supplied answering the five questions this pass raised.
It is mixed: two answers corroborate existing derivations, one is contradicted
by a hashed artefact in this tree, one is internally inconsistent, and one
conflicts with three settled items. None of it moves a constant. Recorded so the
same claims are not re-litigated.

- **"Factory A64 has SUB = 0" — contradicted here.** The repo's decoded bank
  gives A64 as `52 27 0 0 91 94 11 17 0 0 101 0 25 0 30 64 34 17`: noise byte
  91, sub byte **64**. That payload is 2,304 bytes, SHA-256 recorded in
  README.md, checksum-verified and mechanically decoded with zero mismatches.
  The submission also quotes A64 parameters on a 0-255 scale ("Resonance 220",
  "Noise 255") which the hardware's 7-bit tone memory cannot represent, so it is
  unlikely to be reading the tone bytes at all. The sub-slider-at-zero claim is
  **rejected**; the bank stands.
- **Outboard processing on the reference archive — the one claim that would
  change the answer, and it is not settled here.** The same submission states
  that synthmania applies external EQ and a TC Electronic M300 across its
  archive. If true it does not need the SUB-zero claim to matter: a 13 dB
  judgement about the balance of two sources inside those files would not be
  safe, and the re-voicing above rests on them. This is the live question
  against `subMixVolts`, ahead of anything circuit-side. It is recorded, not
  acted on, because the submission cites no page for it.

  **Attempted 2026-08-19, not settled.** The source could not be reached from
  the environment this was written in (outbound access limited to package
  registries), so synthmania's own pages were not read. A search pass found no
  corroboration for the M300 or outboard-EQ claim anywhere. It did surface two
  things pointing the other way, both at snippet strength and neither anchored:
  that the site's demo methodology presents material dry and then wet, where
  wet appears to mean the instrument's *own* chorus rather than outboard
  processing; and that its factory-preset material uses only factory patches,
  which weighs directly against the companion claim that a performer moved
  A64's SUB slider before capture. Taken with the hashed bank giving A64 sub
  byte 64, the balance of what is checkable currently supports leaving the
  coordinate where it is. What would still settle it is a direct read of
  synthmania's own description of its recording chain, specifically for the
  Juno-106 files.
- **"Sub is 1.0 Vpp at pin 1 (+/-0.5 V) at full" — internally inconsistent.**
  Against the anchored 68 k/560 attenuator the pair's linear span referred to
  that node is +/-6.37 V, so +/-0.5 V is 7.9% of it and cannot produce the
  "third-harmonic clipping" the same paragraph says it is sized to prevent. It
  would also place the sub 21.6 dB under the service-anchored saw at full, which
  is not what the instrument does. No saw or noise figure at the same node is
  given, so it does not close this task's budget either way.
- **8253 Mode 3, control word 0x36 — corroborates the OQ-08 lead.** 0x36 decodes
  exactly as stated (SC 00, RL 11, M 011, BCD 0) and matches the reload-at-
  terminal-count semantics filed under OQ-08 on this date from the datasheet
  side. Still a lead: no image offset or capture is cited, and this project's
  own disassembly source self-describes as partial and unofficial.
- **TP4 at 20 Vpp — corroborates OQ-01's derivation.** The tree derives the
  triangle as `beta * V_sat` with `beta = 33/47`, which puts it at 18.3-19.7 Vpp
  for a 13-14 V saturated swing. An independent 20 Vpp reading lands inside that
  within a volt. The reported 48/52 rise/fall split is the same asymmetry OQ-01
  gained as a lead on this date, at four times the size that a 2% rail imbalance
  predicts.
- **Three claims in the same section conflict with settled items** and are
  rejected: TP3 described as "analog ground reference" (this tree has TP3
  carrying the *inverted* triangle for the second line), a chorus mode-rate
  ratio of 2.00 (anchored here at 1.6234799 from the T-network), and Chorus I
  and II summing when engaged together (a settled guardrail: the board has one
  enable and one binary mode line, and both-buttons canonicalises to II). Their
  presence is why nothing else in the document was promoted on its own word.

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

**Level: settled, 2026-08-17.** The 4.0 Vpp TP8 adjustment had been written
onto the source *ahead* of its own C41/R79 shaping, which keeps only 7.2733% of
a white source's power, so the audible rail sat 11.383 dB under the figure it
was named after. Measured through one identical path, the model put noise
23.35 dB below its own calibrated 4.8 Vpp self-oscillation where the two
procedurally chained TP8 adjustments -- 6 Vpp sine at BANK 3, 4 Vpp noise at
BANK 6, same VCA state so its gain cancels -- put it between 8.5 and 12.6 dB
below, the spread being the crest convention a scope trace of noise is read
with. `noiseMixVolts` is now 7.4161, which restores exactly what the shaping
discards and therefore assumes no crest convention; it measures -11.96 dB.
Raising it required digital full scale to be referred to the output summer's
rail first, because one shared generator sums coherently across held voices at
20*log10(N) and a six-note NOISE-10 chord otherwise peaked at +1.97 dBFS.
What remains open below is spectrum, distribution and absolute PSD -- the level
is now anchored, the *shape* is still a parts-value calculation never checked
against a real rail.

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
distribution and the absolute pre-filter coordinate remain voiced.

Step 16 resolves only a numerical endpoint defect in that declared low-pass.
The physical C41/R79 helper remains
`1/(2π·100 pF·330 kΩ) = 4822.877063391 Hz`, but the TPT coefficient now uses
`min(4822.877063391 Hz, 0.45 * internal_rate)`. Former direct evaluation at
8 kHz q1 gave `g = -2.986132794`, state pole `-2.006982013`, and a hidden
private state near `7.87e294` after 0.25 s even when downstream finite recovery
made final audio look safe. The bound is active below
`10717.504585313 Hz`; the old instability seam is
`9645.754126782 Hz`. At 8 kHz q1 it gives
`g = 6.313755512`, pole `-0.726542677`, while 8 kHz q4 uses its 32 kHz
internal grid and retains the physical component corner. It adds no state,
storage, latency or per-sample work and supplies no amplitude, distribution or
PSD evidence.

The model also uses an unexplained
20 µV per-card white excitation at the filter input, after the open 0.40 source
coordinate scale. Each of the six physical card filters now runs continuously
behind its closed VCA, preserving filter history and its deterministic per-card
noise stream across note assignment and retirement; only a full engine reset
reconstructs those states. Dormant plug-in extension slots have no physical
card and may stop processing. Both discrete sources are normalized by
`sqrt(internal_rate / 192 kHz)` to preserve wall-clock spectral density across
host rates and HQ modes; that is a numerical policy and does not settle their
unknown hardware amplitudes or spectra. It normalises *density*, deliberately
not total power: the shaped rail's total RMS is about 0.9 dB lower at 1x than
at 4x because a shallower grid carries less bandwidth, and nearly all of that
shortfall sits above 8 kHz while the 20 Hz-2 kHz band holds within 0.5 dB.
Normalising the shaped total instead was implemented and reverted on
2026-08-17: it recovers the inaudible top octave by pushing the audible band
0.65 dB the wrong way and breaks
`testMainNoiseDensityIsProcessingRateInvariant` at 44.1 and 48 kHz with the
quality ladder at 1x. This task is separate from BBD chorus
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

KR-106 issue 16 records 96 kHz/24-bit calibration work on one JUNO-106
(serial 439522) fitted with Borish replacement voice chips and recalibrated in
2022; surviving archive provenance is incomplete. It remains a capture lead,
not closure. The common source/mixer may
remain original, but voice VCF/VCA and output filtering traverse replacement
modules. Candidate VCA slope/endpoint and oscillator ratios belong to OQ-15's
future protocol and do not retune the nominal law; this mixed path cannot
replace the raw TP8 capture, bandwidth/load declaration and population scope
required here.

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

**2026-08-19 — the 64 kHz asymptote does not follow from 700 µA on 240 pF.**
The paragraph above, the shipping comment and the step record all state the knee
as "700 µA … a pole near 64 kHz on this circuit's own 240 pF / 68 kΩ". That
arithmetic does not close. In the coordinate the cascade is actually solved in —
`wc = Ig · stageAttenuation / (2 Vt C)`, equivalently `Ig / (C H)` with
`H = 2 Vt / stageAttenuation = 6.3663 V` — 700 µA on 240 pF is **72.9 kHz**. The
same 700 µA is quoted as 8.9 MHz ahead of the 560/68560 divider, and
8.9 MHz × 0.00816803 = 72.9 kHz; the two figures were carried side by side in a
ratio of 139 where that divider is 122.43.

64 kHz is instead what the same equation returns on **270 pF** (64.8 kHz) — the
Open80017a integrator value this queue records as a contradiction against the
service circuit's 240 pF — or what **614 µA** returns on 240 pF. Which of those
produced the number is not recorded anywhere in tree.

Two consequences. First, `vcfControlSaturationHz` is reclassified from derived
to **voiced**, bracketed by 64.8 and 72.9 kHz, and the shipping comment now says
so. It is **not** retuned: the exponent was fitted to the measured
code-to-frequency curve with this ceiling already standing, so the pair moves
together or not at all, and the 248 Hz self-oscillation anchor pins absolute
cutoff either way. Second, the 240-versus-270 pF item in the best-in-class queue
said the 270 pF reading "moves the derived 64 kHz upper knee to roughly 57 kHz";
that scaled a figure which was already the 270 pF answer, double-counting the
capacitance. Corrected there.

This sharpens the 40 kHz disagreement rather than softening it: the project's own
derivation, done correctly on its own shipped capacitance, sits at 72.9 kHz —
further from KR-106's 40 kHz ceiling, not nearer. Nothing here promotes either
figure. A Roland-published or independently reproduced high-code curve remains
what this task wants.

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

**C59 itself is modelled as of 2026-08-08**, as a per-voice first-order coupling
between the cascade output and the VCA multiply — until then the envelope
multiplied whatever DC the filter core made, which on a wide-duty pulse patch
was tens of millivolts and arrived as a duty-dependent sub-audio thump. It adds
one new voiced quantity, and this entry owns it: the capacitance is the anchored
read, but the pin-9 load is not, so its 33 kΩ is **voiced and bracketed** at
33–100 kΩ (4.82–1.59 Hz) in the same way `moduleCouplingResistanceOhms` is for
C56/C50. Reading R108 and VR27's installed setting off pp. 18–19, or measuring
the pin-9 termination directly, would settle it, and is a small addition to the
capture below. Nothing audible turns on the choice inside that bracket — the
content is the DC block, not the corner — and an independent implementation
(Ultramaster KR-106 v2.5.13) places its own post-VCF, pre-VCA blocker at
1.59 Hz, the other end of the same bracket.

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
parts are settled. The implementation gives C14 one continuous state whose
sub-hertz pole uses the selected leg's asymptotic load, then runs an independent
Boost/Flat/Cut-II/Cut-III filter. In either Cut position that load now includes
the mux-side R21/R23 1 MΩ in parallel with R39. This captures a nominal
fixed-position response without pretending C14 is absent, but it is not the
full switched network: deselected capacitors plus CMOS-switch parasitics may
retain charge across mode changes.

A 2026-08-09 read-only schematic correction established that C14 feeds
TC4052BP YCOM pin 3 with R39 from that node to ground; Tr3 controls INH pin 6
and is not an audio buffer. Flat uses R27 and Boost uses R25. The deselected
C10/C11 Cut states have derived 15.705/4.9209 ms decays. Step 15 on 2026-08-10
then independently stamped the ideal fixed-mode nominal network and bounded
the production scalar residual. OQ-21 remains open because measured TC4052
parasitics, the complete switched state-space and directed mode-change
transients are still unqualified.

### Needed output (for LLM)

- A designator-complete netlist from IC1a through C14, R39, IC3 and every HPF
  leg, including source/load impedance, CMOS-switch on-resistance, off leakage
  and capacitance, component tolerances and all selected/deselected states.
- Extend the Step-15 ideal fixed-position MNA baseline to a complete switched
  state-space with measured or explicitly bounded device parasitics, including
  magnitude, phase and group delay from below the C14 pole through the audio
  band. Keep exact nominal-component results separate from measured or assumed
  switch terms.
- Re-run the fixed-mode comparison at minimum at
  DC/asymptotic gain, 0.1/0.5/1/10/59.4/225.8/720.5 Hz, 1/10/20 kHz and each
  mode's maximum magnitude/phase error after adding those parasitics; the
  published Step-15 nominal maxima are the non-parasitic baseline, not work to
  repeat unchanged.
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
HIGH, +25 dB into a Fireface 800, original Panasonic MN3009s) reports RX
true-peak noise of **−47.97 dBFS for Chorus I and −44.01 dBFS for Chorus II**.

The absolute figures are not usable — the chain gain is stated but the reference is
the converter's dBFS, not dBu. The **difference is usable**, because the chain gain
cancels: **Chorus II is reported about 3.95 dB noisier than Chorus I** (3.96 dB
for the printed Panasonic pair and 3.95 dB for Xvive). The shipped model now
reproduces that relative delta directly while leaving mode I on the MN3009-derived
baseline. Its chosen insertion point preserves the existing spectrum and
correlation; it does not identify the hardware cause.

Status: **partially resolved.** Confidence moderate for the implemented relative
delta and its true-peak-to-broadband extrapolation, low for the absolute level
and cause. Remaining gap: calibrated PSD, declared bandwidth/weighting, stereo
correlation and spurs.

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
part's normalized response versus clock. *Superseded by the later 600-dpi
pass:* the extraction exists; its tracked/broadband ambiguity still requires
the multi-clock de-embedded installed-unit sweep specified above.

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
clock scaling is removed and the broader contradiction is quantified. The
later 600-dpi digitization of all three curves is complete; its incompatible
tracked/broadband readings now make the next step a multi-clock de-embedded
MN3009/installed-unit tracked sweep,
plus a schematic re-read of the capacitor codes behind
`YouKnow106Chorus.cpp:73-83` — a single 10× code misread moves a corner by a decade.

### Deterministic BBD host-grid aliasing — shipping HQ passes; lower grids reject

Gabrielli, D'Angelo and Squartini distinguish wanted BBD-generated aliasing
(BGA) at `k·Fclock ± f` from the simulation-generated aliasing (SGA) introduced
when asynchronous held-output steps meet the fixed sample grid. The engine now
uses a paper-motivated, deterministic-only polyBLEP after transfer loss and
before the tap-summing pole. Its fixed scheduler has 54 slots and uses 50 in the
tested 200 kHz-clock/8 kHz-grid worst case; the correction handles multiple
edges per internal sample. That output correction alone leaves buckets, index,
BBD phase, transfer and held-noise state, and RNG sequence unchanged. The
grid-specific input-interpolation history and output-correction slots also clear
on an internal-rate change.
Noise remains uncorrected.

The isolated core reduces SGA by **36.2873 dB at 50 kHz** and **42.6752 dB in
the 90 kHz multi-edge case**; at 10 kHz the improvement is **25.0819 dB** after
excluding physical-image bins. Full-line BGA deltas at 44.1 kHz LQ are
**−0.3405, −0.6039, −4.5851 and −5.9408 dB** at 9.216, 10.784, 19.216 and
20.784 kHz; at the default 176.4 kHz HQ rate they are **−0.0016, −0.0030,
−0.0281 and −0.0382 dB**. This qualifies strong SGA suppression and BGA
preservation that is near-transparent at HQ, not exact image preservation at LQ.

The dated strict BBD host-grid comparison used a 2.093 kHz probe at minimum
clock. The wanted image moves **−0.0383 dB in
HQ**; its **−5.2986 dB LQ** movement starts from **−100.47 dBc**. The two false
LQ second-image folds improve from **−26.87/−27.42 to −55.23/−53.61 dBc**,
while the roughly **−116 dBc** HQ folds move to about **−171/−170 dBc**. The
whole comparison's signed difference is **−15.95 dBc peak and −27.66 dBc RMS**
at one fixed gain. These are deterministic software measurements, not a
subjective test or a hardware measurement. The old files were retired in the
2026-08-09 audio reset; replacement renders belong to the
[current audio index](audio/README.md).

The paper's experiment uses an ideal, linear, noiseless 4096-stage MN3005 at
44.1 kHz; this engine models a nonlinear, noisy 256-stage MN3009 at several
internal rates. Its published SNR numbers are therefore not reused as product
claims. The peer-reviewed result supports the BGA/SGA classification and method
family; the exact bounded scheduler and the figures above are engine validation.
The reconstruction mechanism is implemented and the relative-improvement
fixture above remains useful. At the dated Step-7 checkpoint, the common-host
absolute audit rejected every factor and 4× reached only −47.635/−38.189 dBc
for exhaustive 20 Hz–20 kHz unmasked SGA at 44.1/48 kHz, against <−60.
Step 8's causal edge sampler moved that to −72.041/−65.597 dBc but still left
the complete q4 cells rejected by support-grid error.

Step 9 removes that remaining HQ numerical warping in the audited boundary by advancing each complete
six-state support side with an exact continuous transition under the same
causal cubic drive. Common-host q4 now passes every absolute gate:
NRMS **−53.442/−56.101 dB**, BGA error **0.011/0.008 dB** and SGA
**−71.831/−65.381 dBc**. The same four-case low-drive fixture, with one
qualifying BGA line per cell, passes on all six actual HQ selector paths through
192 kHz. It is not a nonlinear whole-line oracle. Lower common-host factors
remain absolute REJECT; the four HQ-off
shipping paths pass only predeclared Step-8 nonregression gates. Noise-on
verification now spans both modes at 176.4/192 kHz and all four HQ-off rates:
the recovered cross-HQ spread is below 0.004 dB, five PSD-band changes versus
Step 8 stay within 0.147 dB and unweighted RMS within 0.070 dB. This is model
validation, not a measured hardware PSD. It does **not** close OQ-01's
clock/delay law, OQ-03's stochastic model, OQ-04's physical loaded
transfer/BGA response or OQ-20's wet switching.

Step 14 leaves that dated low-drive fixture and its classifications unchanged.
Its separate public-path contract now supplies the nonlinear, modulated,
stereo and stochastic software evidence that the Step-9 fixture explicitly did
not claim: six actual HQ selectors pass and four actual HQ-off q1 selectors
reject. The complete current scope and unchanged OQ boundaries are recorded in
the dynamic-BBD section near the end of this file.

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

*Full-curve comparison, 2026-08-07:* the shipping law was re-computed at
every one of the table's 4096 codes (measurements cited as facts, the
comparison re-derived independently). Nominal model: musical core
100 Hz–8 kHz within ±20 cents at 10.3 cents RMS, audible band 19.7 cents
RMS, +0.4 cents at the 248 Hz anchor; the extremes carry the deliberate
base/slope trade above, and part of the residual is the card's raw slope,
which its own WIDTH trim absorbs in service. At Unit Character 1.0 the
local bit-boundary steps render −0.44/+27.49/−0.27 cents against the
card's measured −0.50/+27.49/−0.33 — the mid-sweep step class to under
0.1 cent. Recorded in the
[comparative assessment](comparative-assessment.md) scoreboard.

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

- **In-band alias-floor projection, superseded by implementation** (≤20 kHz,
  re the loudest harmonic, C6 saw): the exploratory calculation reported the
  oscillator alone at **−111.5 dB**, the VCF wide open at resonance 0 at
  **−85 dB**, and cutoff 16 kHz/resonance ≈0.85 at **−55.5 dB**, projecting
  **−87.7 dB** for a doubled filter grid. The doubled-grid build below did not
  reproduce that projection: it measured **−48.5 → −54.4 dB**, limited by the
  inter-domain interpolator. The dominant in-band artefact remains the VCF's
  `tanh` set rather than the oscillator. `testAliasFloor` cannot observe this:
  it runs at resonance 0, calibration 0, and stops sweeping at 20 kHz.
- **Historical pre-Step-7 half-band at a 44.1 kHz host**: −0.85 dB at 20 kHz,
  −2.5 dB at 21 kHz, with fold-back rejection of only −31.7 dB for content
  landing at 19.1 kHz. Both stages then shared the 63-tap kernel; the first
  stage's transition band was wide enough that only the last stage mattered.
  The later H=24/95-tap DCO reconstruction supersedes this as the current
  boundary; these figures remain the dated Blackman-Harris before-state.
- **Measured as *not* audible in that narrow C6/VCF experiment**, recorded so
  the work is not repeated under the same conditions: widening the BLEP kernel
  or interpolating its table more finely (then already −111 dB); `double`
  state in the former implicit `OtaCascade` (bit-indistinguishable from `float`
  at 40 Hz / 200 Hz / 1 kHz cutoff, resonance 3.9—this narrow negative did not
  test Step 10's high-order explicit path, which now retains double state);
  more Newton iterations (the 8-iteration cap is
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
700 µA**. That is the physical origin of the upper knee OQ-18 asks about, and it
is consistent with Roland's published 50 kHz top. This paragraph originally put
the corresponding pole "near **64 kHz**"; corrected 2026-08-19 above — 700 µA is
72.9 kHz on the shipped 240 pF and 64.8 kHz on the reconstruction's 270 pF, and
the shipped 64 kHz ceiling is voiced between them. The knee is OTA current saturation, not an arbitrary cap. A replacement
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
| **OQ-04 — read complete** | chorus support chain | the capacitor codes behind `YouKnow106Chorus.cpp`'s support-filter block, **read separately for the input and output sides** | wet-path bandwidth; the chain is −12 dB at 10 kHz where the MN3009 alone is ~−3 dB. *Closed 2026-08-07:* the original-page pass read the 106's own p. 15 at designator level on both sides — pre-BBD C33 820 pF/C31 680 pF then C34 1.8 nF/C32 270 pF, and per output line C37/C35, C38/C36, C42/C40, C43/C41 — so the reconstruction sections no longer *assume* the input sections' values; the corner set is **106-anchored** and OQ-04 keeps only the loaded/time-varying transfer |

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

  *The wart is closed, 2026-08-08, and it did not need the measured family.*
  `frequencyTrimAmount` is deleted. The correction is now derived from the
  cascade's own harmonic balance — the sinusoidal-input describing function of
  `tanh` on the four stage pairs at their own `2Vt/stageAttenuation` headroom
  and on the resonance return at its own `2Vt·(100/1.5)` = 3.4667 V, solved for the limit
  cycle each loop gain sustains — so it is identically **1** below the
  oscillation threshold, which the same balance places at a loop gain of
  exactly 4, the profile's own `nominalOscillationFeedback`. That removes the
  hidden cutoff lift the wart named: +8.76 / +32.24 / +80.17 / +116.25 cents
  at resonance panel 0.30 / 0.50 / 0.70 / 0.80 became +0.00 at all four.

  With the correction derived, the two anchors are no longer a joint fit.
  `maximumFeedback` was re-solved against the amplitude anchor alone and moved
  4.51 → **4.504**, landing **4.80 Vp-p** where the joint solve had landed
  4.83. The 248 Hz anchor is then a *prediction*: the rendered oscillation
  reads **247.90 Hz**, 0.67 cents under it, and stays inside ±0.81 cents at
  every loop gain in the oscillating range, so it constrains no constant any
  more. Two independent readings of the same 248 Hz now agree — the service
  self-oscillation trim and, at resonance 0, OQ-18's measured
  code-to-frequency table — which is what a cancelled droop is supposed to
  deliver. What OQ-09's measured family still owns is the *shape* of
  `loopGain()` and `inputCompensation()` between the ends.

  *One finding this raised, filed here and not acted on.* Those two readings
  of 248 Hz agreeing is equivalent to saying the real card's self-oscillation
  does **not** sit flat of its own small-signal corner, while this cascade's
  sits 203 cents flat of it. A four-pole loop oscillating at its own corner
  carries √2 more amplitude at every step back towards the input, so the model
  drives its first stage to `a = 1.01` on a 6.37 V headroom while its output
  is only at 2.40 V peak. Either the hardware's limit cycle really is that
  undrooped — in which case something upstream is too compressed, the stage
  headroom or the internal amplitudes it implies — or the two 248 Hz readings
  are not the same measurement, the service trim being read on an oscillating
  card and the table on a swept one. Nothing in tree settles it. The
  correction is right either way, because it is exactly what reconciles the
  model with both anchors at once; what is open is whether the droop it
  cancels should have been that large. The measured
  response-versus-resonance family this task asks for would settle it, since
  it fixes the corner at each resonance setting independently of either
  endpoint.

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
  (stated then as a 64 kHz pole on this circuit's 240 pF / 68 kΩ; that
  attribution is corrected under OQ-18 above, 2026-08-19 — 700 µA on 240 pF is
  72.9 kHz, and 64 kHz is the 270 pF answer) and only the exponent fitted
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
     are not the limit in this hot VCF fold-back experiment. It is not the later
     all-waveform pre-VCF common-host audit, whose Step-7 diagnosis is recorded
     below.
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
  is not available on every toolchain this project builds with. This is the
  dated 63-tap window comparison; Step 7 later selects 95 taps against the
  expanded DCO matrix.

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

**Which coordinate that 0.275 lives in matters**, because a different — also
correct — reading of the same netlist yields a number 14.5× larger and must
not be swapped in. Solving the stage-1 node with ideal elements at DC gives
`V₄ ∝ (68k/4.7k)·(1 + 0.2752·K)·V_in/(1 + K)` with `K = 68k·gm_r/67.7` the
loop gain. *Output-referred*, the compensation term is `K·(67.7/17)·V_in ≈
3.982·K·V_in` — in the ratio of the two OTA terms the shared injection
transfer cancels entirely. But the engine's coefficient is defined as a
multiplier on the **direct input path** (`filterInput = mixed ·
filterInputAttenuation · inputCompensation`), and that path — V_in through
the 4.7 kΩ into the same node — carries gain 68 k/4.7 k with no
transconductance in it, so the boost relative to it is
`(67.7/17)·(4.7/68) = 0.27525` per unit loop gain. A future OQ-09 refit of
`inputCompensationPerFeedback` therefore compares against **0.27525**;
**3.98235** is the output-referred added-term ratio, a different quantity,
and the two differ by exactly 68/4.7 = 14.47. Both OTA terms and the direct
path share stage 1's dynamics, so the ratio — and the static multiplier form
the engine uses — is frequency-independent for ideal elements.

Also read from the same netlist: the loop's limiting mechanism is OTA
saturation alone (no clipping diodes anywhere in the reconstruction), and
the first stage's gain structure (−68 k/4.7 k rather than a passive
divider) is a new lead against OQ-15's open input coordinate.

This is **corroboration, not an anchor**: Open80017a descends from the same
dksynth thread as the ÷17.0/÷67.7 figures already on file, so the agreement
is depth within one lineage — now netlist-explicit and build-validated by
ear — not a second independent source. The constant is unchanged on that
provenance ground alone. A future refit is also *cheap*: the multiplier acts
only on the mixed source signal entering the cascade, and the 4.8 Vp-p/248 Hz
service endpoint calibration is evaluated with every source silenced (the
current amplitude-only result is 4.8009 Vp-p / 247.90 Hz), so the
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

The corrected full-resolution read has C14/R39 feeding TC4052BP YCOM pin 3
directly. Tr3 controls INH pin 6; it is not an audio buffer. Taps
Y0/Y1/Y2/Y3 are pins 1/5/2/4; "47K×4" summing resistors R28/R26/R27/R25
feed IC4a's virtual ground with R29 47 kΩ feedback; series caps C11 4.7 nF
(Y0 leg) and C10 15 nF (Y1 leg) form the Cut paths. **R23/R21 "1M×2" bias
the mux side of the capacitor legs**: deselected C11/R23 decays in 4.9209 ms
and C10/R21 in 15.705 ms. With the mode table
(p. 13: MODE 0/1/2/3 → HPF B ① = 1,1,0,0; HPF A ② = 1,0,1,0, and the
4052's (B,A) decoding) the map is: mode 0 = Y3 (boost: direct R25 plus the
IC4b branch), mode 1 = Y2 (flat, direct R27), mode 2 = Y1 (C10 15 nF),
mode 3 = Y0 (C11 4.7 nF). On the selected leg the 1 MΩ does not redefine the
C10/C11 pole against its 47 kΩ virtual-ground leg, so **225.8/720.5 Hz stand,
and KR-106's 236/754 Hz (1 MΩ paralleling the 47 kΩ) put the bias on the
wrong side of the capacitor.** It does, however, directly load the C14/YCOM
node; Step 15 supersedes the old R39-only C14 value with 0.498203201 Hz from
`R39 || 1 MΩ`. The deselected-leg charge-memory question remains OQ-21's
transient ask.

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
*Corrected 2026-08-07 (doc↔code audit pass): 23.9 kHz was the **JUNO-60**
sweep's minimum (128/5.35 ms), and this containment claim was never recomputed
when the 106's own 1.4–6.4 ms sweep was promoted the same day. The shipped
minimum clock is **20.0 kHz** — this task's own status row already says so —
against which ν = 0.45 falls at 9 kHz, inside the band the pre-BBD chain still
passes (its poles are 7.23, 9.69 and 10.38 kHz). The model therefore does enter
the quarantined normalised region at the slow end of the sweep. This does not
move `transferSmear`, which is a fixed per-shift coefficient rather than a fit
to these curves; what it removes is the claim that the datasheet's typical
family could validate the model everywhere the sweep goes. It cannot, at the
bottom of the range.*

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
variants), so the then-current 4.83 Vpp/248 Hz joint endpoint solve was
untouched. The later amplitude-only solve publishes 4.8009 Vpp / 247.90 Hz. A live
grid change also simplifies: the carried states are now a physical
capacitor voltage and a dimensionless drive, both rate-invariant, so
`retime` re-expresses nothing. `testCascadeDeniesTheFoldback` fences the
hot case below −60 dBc — a bound the endpoint evaluation fails —
and the retime/reference-solve/oscillation tests pin what must not have
changed. This is a numerical product mechanism in the same class as the
BBD host-grid polyBLEP: it removes simulation-grid artefacts the analogue
instrument never had, and it neither resolves nor claims any OQ.

### OQ-15 — the module-input coupling ships (2026-08-07, doc↔code audit pass)

**Work mode:** analysis of supplied evidence plus measurement of the shipping
algorithms. **No hardware was measured.** A subsystem-by-subsystem audit of the
engine against this queue found the p. 13 mixer read only half-consumed: the
renderer's own comment named `C56/C50 10 µF NP` as the coupling between the
summed WAVE node and pin 1 VCF IN, and the signal path did not have it. The
summed node went to `filterInputAttenuation` unblocked, and the transconductor
cascade carries no DC-blocking term, so mixer DC reached the voice VCA and was
multiplied by the envelope there.

The pulse carries the most of it. The comparator's output is a duty-asymmetric
square, so its mean walks with PWM: at the 95 % duty the hold's 0.6 V endpoint
reaches, mean = 6 V·(2d − 1) = 5.4 V at the node, or 2.16 V after the 0.40
source coordinate. Measured on the shipping engine — pulse only, MIDI 48,
manual PWM, envelope VCA, 50 ms boxcar peak after note-on against the settled
sustain peak — the note-on thump **rose** with PWM depth:

| PWM depth | before | after (coupling settled) |
|---|---|---|
| 0.0 | −22.6 dB | −22.5 dB |
| 0.3 | −17.2 dB | −25.6 dB |
| 0.6 | −14.5 dB | −30.6 dB |
| 1.0 | −12.7 dB | −37.0 dB |

A step that grows 10 dB as PWM deepens is not a step this instrument makes.
With the capacitor in place the trend reverses and the steady AC level is
essentially unchanged, so this is a thump removal, not a tone change.

**What is settled and what is not.** The capacitor is a designator-level read
and its position is settled; the resistance it works against is **not**. The
same complete-scan pass re-roles R99/R102 33 kΩ as the sub-emitter DC bridge,
so it is not this pole's load, and the node's termination is exactly the
measurement OQ-15 still wants. The shipped 33 kΩ is therefore a **voiced**
stand-in taken by analogy with the two settled 10 µF NP / 33 kΩ couplings
downstream (C14/R39, C12/R36), and the choice barely matters: every plausible
10–100 kΩ termination lands the corner between 0.16 and 1.6 Hz, far below the
lowest note. The audible content is the DC block itself, not the corner.

Two consequences recorded rather than smoothed over. A 10 µF/33 kΩ coupling
has a 330 ms time constant, so a **cold engine spends about 1 s charging each
card's capacitor to the standing duty offset** — a power-on transient the real
instrument also has, and the reason the fixture below warms up before it
measures. And the fixture that guarded the C17/C20 tail across an HQ rebuild
had been leaning on the very DC this capacitor removes: a single voice's
residual now reads 0.0025 against its 0.005 guard, so it moves to a six-card
chord at full VCA LEVEL, which leaves 0.026 by a mechanism the circuit does
have — each card's own 0.48 Hz coupling passing the note-on duty step, six
summed. `testModuleInputCouplingKeepsMixerDcOutOfTheVoiceVca` pins the corner
and asserts the thump falls monotonically as PWM deepens.

### OQ-03 — a causal hypothesis behind the reported ~3.95 dB mode delta

Recorded as a lead, not a mechanism. The reported II−I true-peak delta —
3.96 dB from the printed Panasonic pair and 3.95 dB from Xvive, summarized as
3.95 dB for both in the source post — sits
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
rate-proportional candidate the requested calibrated capture could confirm or
kill. The shipped default now follows the observation directly; the rate law
survives only as the internal `useChorusRateNoiseHypothesis` comparison. A
same-chain capture at a third, artificial rate would separate rate-proportional
noise from mode-switch-network noise directly.

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

## Factory-demo A/B pass — 2026-08-07 (real-unit audio compared, adjudicated)

**Work mode:** analysis of supplied evidence. A complete per-preset demo set
of a real JUNO-106 playing its factory patches (synthmania.com, MP3 128k,
unit and chain undocumented) was compared against the engine rendering the
same stored patch bytes at matched keys. Twelve comparisons were built and
put through two rounds of independent refute-first adjudication; the first
round rejected every naive pairing (the demos are chordal almost
throughout, and sub-equipped patches set octave traps for pitch trackers),
and ten adjudicator-prescribed rescues (correct keys, clean segments,
chord-vs-chord renders through the engine's full common path) produced ten
presentable comparisons, all with caveats, none excluded. Median absolute
pitch error across the measurable set: **4.0 cents**. Where the material
could measure at all, harmonic-stack agreement reached 2.2–2.9 dB RMS
(A15 chord composite, A38) and 4.9 dB (A34). The MP3/undocumented-chain
bound is stated on every card; this pass does not substitute for the
queue's calibrated captures.

Genuine model observations that survived adjudication, recorded as leads
(each hedged by the demo chain and none promoted):

- **Upper-mid darkness** recurs across independent verifiers: harmonic-peak
  deficits of ~3.6 dB (1–2 kHz) growing to ~10 dB (2–3 kHz) on A15;
  ~6–8 dB less 2–3.5 kHz presence on A34's matched note; a ~3 dB
  attack-brightness deficit at 320–600 Hz on A48; B33's harmonics above
  ~3 kHz fading faster through the pluck decay. Bears on OQ-15's open
  drive budget and OQ-18's knee via patch-byte-dependent cutoff.
- **Sub-oscillator balance**: A15's chord render carries a ~14 dB hotter
  41 Hz sub line than the hardware relative to the reference band, while
  A34's single note locks sub weight within ~1 dB — an inconsistency that
  points at the mixer's loaded level budget (OQ-15, the 27 kΩ+diode leg)
  rather than a single scalar.
- **Hiss floor**: the model's broadband floor reads ~9 dB above the
  hardware recording's floor at 3–9 kHz on A17 after discounting known
  confounds — "plausible but unproven" per the verifier, because the
  hardware floor may be the MP3's own; feeds OQ-03's calibrated-capture
  ask.
- **Chorus behaviour corroborated where measurable**: A17's 0.45 Hz swirl
  and ±6 Hz vibrato sidebands match; A63 reproduces the square-plus-sub
  voicing "to within a few dB everywhere the MP3 can measure".

The same pipeline was then run symmetrically against **KR-106's engine**
rendering the identical patches at the identical keys — the field's one
benchmarkable competitor. The two models measure comparably close to the
real unit on this material; YouKnow106 is closer on harmonic-stack
accuracy in 5 of 9 valid cases and clearly closer on the two cleanest
(A15 2.23 vs 5.39, A38 2.95 vs 6.29 dB RMS), KR-106 closer on broadband
band envelopes in 9 of 10. **Two controls were run on that band gap.**
The first conjecture — that YouKnow106's chorus-hiss model was being
punished against the MP3-floored reference — was tested with a
hiss-muted re-render and refuted (recorded so it is not retried as
framed). The second control decomposed the gap by band region with a
three-way inter-harmonic floor measurement and identified the actual
mechanism: **KR-106's modelled floor sits on the recording's own floor
(within ±1.3 dB in every region), while YouKnow106's total floor sits
≈5 dB below it** — consistent with KR's dry/wet noise floors having been
calibrated from similar chain-bearing recordings. The gap's largest
component (≈5 dB of band deviation at 4–8 kHz) is therefore the metric
rewarding coincidence with the *recording chain's* floor, which an
undocumented lossy chain cannot convert into a claim about the
*instrument's* floor in either direction. The residual low/mid-band gap
(≈1–2 dB, mixed directions) remains a genuine small tonal difference and
keeps the upper-mid darkness lead above alive for the calibrated
captures.

The published comparison page carries every clip, spectrogram pair, band
overlay, metric, the head-to-head table and every caveat. The CNCD
sample-CD corpus (327 uncompressed real-106 WAVs) was also retrieved;
its 70-file bass set was inspected file-by-file and is **not** the
stepped-cutoff characterisation series its archive description suggests —
fundamentals scatter across 29–99 Hz and spectral centroids across
48–1425 Hz with no monotonic march, so it is a heterogeneous patch
collection with undocumented settings. It remains the only lossless
real-106 audio in hand (usable for noise-floor and qualitative checks),
but a controlled filter characterisation still requires the queue's
calibrated captures.

## Supplied-report reconciliation pass — 2026-08-07 (summary compilation checked; no promotion imported)

**Work mode:** analysis of supplied evidence. The supplied artifact is a
five-page typeset report, "High-Fidelity Parametric Emulation and Circuit
Analysis of the Roland JUNO-106 Synthesizer", written in reaction to this
queue: a system-wide parameter table, a restated open-queue status table and
prose derivations, using this project's own five-class evidence hierarchy.
It was delivered as a rendered document only — no raw capture, CSV, scope
export, netlist or citation this project has not already reconciled travels
with it, and no file is retained in the repository to hash. Every checkable
number was still verified against the current model state.

### It is a restatement, not corroboration

The report reproduces this project's own computed constants to their full
recorded precision — 1.6234799, 0.8654743, 908.249 Ω,
`−16.31966 + 0.165581·b`, Rs ≈ 3.70 kΩ, 0.5533/0.8983 Hz,
+10.50 dB/+1.41 dB/59.41 Hz — including values that exist nowhere outside
this repository at that precision. That level of agreement demonstrates
shared origin, not independent derivation: a future pass must not count
this document as a second source for any of them. Where its arithmetic
could be re-run it is correct and current — the R_eff pair and mode-rate
ratio, the β = 33/47 rate scale, the 47/39 wet/dry direction and +1.62 dB,
the 560/68560 attenuator in the correct shunt-in-denominator form, the
±19.6 mV/13.9 mV rms drive figures, the Vgc affine constants and the
Q(v,c) recurrence — and its chorus mode-switch prose matches the
relabelled T-network read (Tr1 grounding the R4 junction), not the
superseded R3-shorting description. One internal slip: the quoted
compensation factor 0.2749 does not reproduce from its own equation
`(67.67/17.00)·(4.7/68)`, which evaluates to 0.2751.

### Promotions and misstatements declined

- **Chorus sweep "Anchored".** The 1.4–6.4 ms sweep stays third-party-scoped
  exactly as OQ-01 records it. No third-party measurement is promoted to an
  anchor, and a calibrated original-unit capture still owns the final word.
- **Resonance compensation 0.2749 "Derived".** The resistor-only conversion
  restated here is the same dksynth-lineage divider set already recorded
  under OQ-09 — depth within one reconstruction lineage, not a second
  source. The shipping coefficient remains the voiced 0.2296; OQ-09's
  measured family owns the number.
- **OQ-21 "Resolved".** Only the Boost-shelf sub-question is resolved, and
  the report's own status row cites nothing beyond those shelf constants.
  The coupled C14/HPF transfer, switch-state memory and mode-change
  transient remain open; OQ-21 keeps its P2 row.
- **Voice-VCA onset "Derived".** The +0.25…+0.27 V TP7 standoff is the
  anchored part; the 150 mV onset remains a reconstruction-lead-placed
  compatibility value, and OQ-19's sweep owns it.
- **"IR3109 teardown" as the 700 µA source.** The recorded source of the
  control-current saturation is the AS3109 teardown; this project records
  no IR3109-sourced figure. The clone-versus-original provenance
  distinction stands.
- **ADAA "below −66 dBc".** The fenced, measured bound is −60 dBc in the
  hot resonant case (`testCascadeDeniesTheFoldback`); the tighter figure
  arrives without a measurement and the fence is unchanged.
- **OQ-05 "Linear model; ±12 V swing limit into R_L ≥ 2 kΩ".** Both halves
  misdescribe the current state: the shipped IC6 stage is not linear — it is
  the rail-bound algebraic clip (13.5 V, exponent 8), evaluated per sample on
  the internal oversampled grid with no antialiasing, because antiderivative
  antialiasing was built at this node and **measured as not audible** (no
  measurable aliasing at the rail, +3 dB or +6 dB in; recorded above under the
  2026-08-06 evidence search). At this dated checkpoint the engine's only
  antiderivative machinery was the VCF cascade's Newton solve; Step 10 removes
  that discretization. And no ±12 V bound is recorded anywhere in this project.
  The pair reads like a 4558-family guaranteed-minimum output-swing row,
  but it arrives uncited; if a sourced TA75558S specification row is later
  produced it belongs in OQ-05's protocol as a datasheet bound, never as
  the loaded board measurement that task requires.

The report also lists the designator-level wet/dry read as merely
"Derived" where the project's 2026-08-07 read is anchored, and states the
nominal 4.20 ms pass without the recorded ~4.27 ms measured contradiction
— classification drift in the lenient direction as well as the strict one,
so its class column was compiled without auditing the evidence hierarchy
and is not imported in either direction.

### Net effect

No model change, no class change, no queue change. The queue's 20 open
questions plus the OQ-06 dependency stand exactly as written above.

## Best-in-class pass — 2026-08-07 (real-time cost measured; no OQ touched)

**Work mode:** competitive evidence search plus measurement of the shipping
algorithms. **No hardware was measured, and no task below is advanced or
closed.** It is recorded here because the queue is where this project keeps its
measurements, and because three of these are model-internal facts a future
reader would otherwise rediscover. The full write-up is
[the plan](best-in-class-plan.md).

A refreshed market sweep found the fidelity picture unchanged and one axis
missing from the comparative assessment entirely: **real-time cost**, which
this market does separate products on. Measured at 48 kHz/HQ on one 2.8 GHz
core, the engine did not run in real time in any configuration — 1.398×
realtime with no key held, 2.376× on a six-voice chorus patch, 3.960× at
resonance 0.95 — and cost more idle than sounding.

### Model-internal measurements from that pass

- **The four-stage implicit solve is 65% of the whole engine's cost**, and
  almost all of that is libm. At resonance 0.95 it evaluated roughly 69
  `tanhf`, 33 `expf` and 33 `log1pf` per filter step per voice, 1.15 M steps a
  second.
- **The Newton loop's convergence test was unsatisfiable wherever the filter
  was working.** It compared an absolute `1.0e-7` against a step on capacitor
  voltages of several volts, which single precision resolves to about 6e-7, so
  the loop ran its eight-iteration cap: instrumented, 7.99 iterations of 8 at
  resonance 0.95, 6.22 with chorus engaged, 2.86 on a quiet patch. Raising the
  cap to 32 raised the average to 27.0, confirming the test — not the problem —
  was what the loop could not satisfy. The step remaining at the cap measured a
  mean of **5.1e-5 V, worst 9.5e-4 V, on states averaging 1.7 V**: the
  iteration was already at its own round-off floor several iterations before it
  stopped. This corrects the earlier "more Newton iterations" entry under
  *Model-internal measurements* above, which recorded the cap as reached
  "10–22% of the time under hot resonant input" — on the settings that define
  this instrument it was reached essentially always.
- **A candidate for OQ-03's 3.95 dB mode delta was implemented and measured.**
  See the OQ-03 implementation note. One measurement caution came with it: each
  line writes one noise sample per bucket edge, so its instantaneous floor
  rides the swept clock, and a window that is not a whole number of modulation
  cycles reads mode II 0.69 dB hot with no mechanism present at all.

### Acted on, without touching a calibrated constant

`tanh` and `ln cosh` now come from one shared exponential and the path-start
antiderivative left the Newton loop; the convergence test is scaled to the
volts it measures; four pieces of loop-invariant per-card work left the
per-sample loops. Cost, same harness back to back: idle 1.398 → **0.852**, six
voices 1.105 → **0.699**, six voices with chorus II and the full mixer 2.376 →
**1.361**, resonance 0.95 3.960 → **1.395**. The difference from the pre-pass
engine is −102 dB RMS relative to signal on a plain six-voice patch and −95 dB
on a full chorus patch; on a self-oscillating patch it is −20 dB, a limit
cycle's phase rather than its amplitude or frequency. The current amplitude-only
calibration publishes 4.8009 Vpp / 247.90 Hz against the 4.8 Vpp / 248 Hz
service anchors.

None of this is hardware evidence and none of it is a claim about the
instrument. The queue's 20 open questions plus the OQ-06 dependency stand
exactly as written above.

## DCO numerical-reconstruction pass — 2026-08-09 (quality defect fixed; no OQ closed)

**Work mode:** numerical mechanism diagnosis and production qualification
against the existing analytic/common-host contracts. **No original unit was
measured.** This pass changes the fidelity with which the engine realizes its
declared oscillator equations; it does not add physical evidence to those
equations.

Step 6 exposed a broad DCO defect that the legacy two-note/full-engine fence
could not see. The step correction table stored `bandlimited step − ideal
step`, a residual with a unit jump at `t=0`, and then linearly interpolated it.
The final lookup interval before zero therefore blended the two event sides and
emitted a premature fractional edge. The corrected path stores/interpolates the
continuous bandlimited step response and subtracts the exact Heaviside value
only after lookup. The slope residual is continuous at zero and stays direct.
A circular H=24 delay supplies the symmetric support; its immutable 64× tables
are shared rather than rebuilt per engine. The selected global decimator is a
95-tap Kaiser (β=7.857).

The before/after classification uses the same six 90-take cells and unchanged
−70 dBc gate:

| DCO worst off-mask bin | 1×, Step 6 → current | 2×, Step 6 → current | 4×, Step 6 → current | Current result |
| --- | ---: | ---: | ---: | --- |
| 44.1 kHz | −12.780565 → **−83.476933 dBc** | −36.596878 → **−82.436627 dBc** | −42.618000 → **−82.432588 dBc** | **PASS / PASS / PASS** |
| 48 kHz | −16.741087 → **−84.879008 dBc** | −36.344575 → **−92.976529 dBc** | −41.452375 → **−92.978397 dBc** | **PASS / PASS / PASS** |

Analytic controls, strict/top harmonic gain, scan and DCO/PWM/SUB holds pass in
every cell. Candidate selection kept margin rather than merely clearing the
line: H=20/95 taps failed at −65.940893 dBc, H=24/79 failed at −68.0828,
H=24/87 first passed at −77.8416, and the chosen H=24/95 combination has a
−82.432588 dBc worst cell, 12.43 dB below the gate. H=32 added no useful
gain. The longer half-band is necessary at 44.1 kHz because the shorter
boundary leaked a legitimate 25.1 kHz sixth pulse harmonic back near 19.0 kHz.

Because that half-band is global, the independent VCF/BBD contract was rerun.
The VCF's decisive hot full-waveform row remains:

| VCF hot NRMS vs RK128 | 1× | 2× | 4× | Result |
| --- | ---: | ---: | ---: | --- |
| 44.1 kHz | −1.110 dB | −12.233 dB | −24.348 dB | **REJECT / REJECT / REJECT** |
| 48 kHz | −1.062 dB | −13.752 dB | −25.810 dB | **REJECT / REJECT / REJECT** |

Its exhaustive hot-residual off-mask values are
−27.063/−67.589/−99.040 dBc at 44.1 kHz and
−19.658/−67.128/−99.620 dBc at 48 kHz. The BBD rerun is:

| Host | Factor | Analytic NRMS | Qualifying-line BGA error | Unmasked SGA | Result |
| ---: | ---: | ---: | ---: | ---: | --- |
| 44.1 kHz | 1× | −3.099 dB | 34.389 dB | −24.854 dBc | **REJECT** |
| 44.1 kHz | 2× | −14.910 dB | 4.088 dB | −28.762 dBc | **REJECT** |
| 44.1 kHz | 4× | −27.045 dB | 0.867 dB | −47.635 dBc | **REJECT** |
| 48 kHz | 1× | −4.640 dB | 22.893 dB | −28.871 dBc | **REJECT** |
| 48 kHz | 2× | −16.426 dB | 3.257 dB | −31.329 dBc | **REJECT** |
| 48 kHz | 4× | −28.181 dB | 0.708 dB | −38.189 dBc | **REJECT** |

At this DCO checkpoint, every one of those VCF/BBD cells still rejected its
absolute domain gate. The 4× candidate was not truth, and a DCO pass could not
admit a split-rate engine. Step 9 subsequently admitted the bounded BBD HQ
fixture; the Step-10 section below supersedes this dated VCF row.

The reconstruction/filter support also moves numerical latency. Raw
1×/2×/4× centres and pads are 24+17=41, 35.5+6=41.5 and
41.25+0=41.25 host samples. The engine/processor report one fixed 41-sample
coordinate: 0.930 ms at 44.1 kHz, 0.854 ms at 48 kHz, 0.427 ms at 96 kHz and
0.214 ms at 192 kHz. The exhaustive playing proxy becomes 87/210/335 HQ-off
and 105/228/351 HQ-on, or 46/169/294 and 64/187/310 after subtracting the
report. Pitch-write, VoiceVca, 63.2% hold and scan results are unchanged. The
nominal numerical centres align within 0.5 sample; the −80 dBFS onset proxy is
signal dependent and sees different amounts of symmetric pre-ringing.

In that checkpoint's work audit, 6,144 decimator calls visited 301,056 nonzero
taps and performed 602,112 stereo MACs—49 visits and 98 MACs per call. The
former VCF iteration counts were 205,008/56,604 at 48 kHz 4×/1×,
104,146/53,679 at 96 kHz 2×/1× and 51,508 at 192 kHz 1×, with zero recovery;
Step 10 retires those Newton counts. The informational
M1 Max/arm64 Release timing rerun gives 4×/1× CPU/audio ratios of 3.543
idle, 3.177 six-voice plain, 3.471 six-voice resonant and 2.655 full-mixer
Chorus II. These measurements move no performance gate.

The physical/model laws were deliberately held fixed: 8 MHz integer
divider/range clocks, straight 12 V ramp, 2.2 µs reset, comparator geometry,
sub divider, converter schedule/holds and changed-pitch restart policy. Thus
OQ-07 remains open on hold acquisition/droop, OQ-08 on exact scan/restart
timing and forced state, OQ-11 on the pinned comparator leg's loaded behavior,
and OQ-15 on oscillator/mixer levels. Passing a numerical equation-fidelity
gate is not new hardware evidence.

## VCF numerical-integration pass — 2026-08-09 (standard HQ admitted; no OQ closed)

**Work mode:** direct numerical realization and independent qualification of
the already declared continuous VCF equations. **No original unit was measured
and no physical constant or evidence class moved.** The old float
path-average/capped-Newton discretization and the rejected Step-4 one-step
quasi-Newton candidate are implementation history, not current truth.

Production now retains only four double-precision capacitor voltages as
physical VCF state. Every internal interval uses two fixed half-step,
five-evaluation Merson RK4 advances: 2 substeps, 10 right-hand-side and
resonance-return evaluations, 40 stage evaluations, 40 full Early-effect
evaluations when enabled, and 7 causal input phases. The current-plus-three-past
polynomial ramps linear to quadratic to cubic at startup. Cutoff, resonance and
thermal headroom interpolate between known endpoints at the same nodes. A
quality-rate change preserves physical charge and the common endpoint, maps it
through the cap-aware rate ratio, and collapses older old-grid history under the
existing zero-gain transition. No future sample, lookahead or latency is added;
the report stays fixed at 41 host samples. A final post-thermal product-grid
bound limits `omega*dt` to `0.9π` (0.45 cycles/internal sample); that is a
numerical policy, not JUNO-106 bandwidth evidence.

The focused full-mechanism comparison with independent RK96 reads
**−162.551 dB / 4.21471e-8 V**; an alternating-control trajectory reads
**−95.2005 dB**. Every cold/warm product-cap tail is exactly zero across the
six actual cards at Unit Character/calibration 2. The unchanged common-host
classification is:

| Host | 1× hot / driven / off-mask | 2× hot / driven / off-mask | 4× hot / driven / off-mask | Verdict |
| --- | ---: | ---: | ---: | --- |
| 44.1 kHz | −12.538 / −145.593 / −44.602 dB(c) | −30.414 / −113.526 / −85.968 dB(c) | **−50.351 / −112.144 / −133.278 dB(c)** | **REJECT / REJECT / PASS** |
| 48 kHz | −14.269 / −144.364 / −48.081 dB(c) | −33.028 / −114.710 / −88.898 dB(c) | **−50.064 / −113.339 / −140.552 dB(c)** | **REJECT / REJECT / PASS** |

Here “hot” and “driven” are RK NRMS; “off-mask” is the hot residual in dBc.
Self-oscillation stays within 0.001 cent and 0.010 dB of the independent oracle.
The 4× pass is a declared-model result, not hardware truth.

The dynamic oracle evaluates the exact 23-write order, 522 µs trajectories and
all actual cards at its RK nodes. Nineteen physical takes per rate family cover
24 logical profiles because exact Character-0 card collapses are shared. The
six actual standard HQ selector cells pass at **−48.585, −48.724, −48.557,
−48.514, −48.324 and −48.293 dB** for 44.1/4×, 48/4×, 88.2/2×, 96/2×,
176.4/1× and 192/1×. RK64/RK128 convergence is at or below −150.9 dB, with
zero recovery, schedule or count mismatch. The 768 kHz/1× endpoint also passes
at **−61.360 dB**.

The 8 kHz/4× endpoint is deliberately an **expected REJECT** at **−33.245 dB**
despite −139.820 dB convergence and finite/exact counts. Its maximum
converter-event snap is 30.978 µs; the named steady,
continuous-cutoff/resonance/headroom and all-exact diagnostic controls read
−136.916, −60.546 and −100.713 dB. That diagnosis assigns the next atomic step
to fractional, event-aware hold evaluation at 8 kHz—not a relaxed −40 dB gate.

Merson is not labelled uniformly superior: fixed three-substep RK4 is about
0.55 dB better in the rejected 44.1 kHz/1× hot high-mu cell. Merson wins the
primary HQ trajectory, damping/Hopf/onset and product-cap stability bake-off
while avoiding a runtime method selector.

The 48 kHz, six-card resonant 2,048-frame HQ/HQ-off windows count
49,152/12,288 VCF steps, 98,304/24,576 Merson substeps,
491,520/122,880 RHS and feedback evaluations, 1,966,080/491,520 stage and
full-Early evaluations, 344,064/86,016 input phases and zero recovery. Three
alternating seven-repetition M1 Max audits put current 4×/1× CPU/audio at
0.677/0.170 idle, 0.690/0.178 plain, 0.850/0.228 resonant and 0.736/0.191 full
mixer Chorus II; the corresponding Step-9 baselines were 0.546/0.152,
0.509/0.159, 0.670/0.192 and 0.786/0.296. The numbers are informational and
patch-dependent; the hard `<5×` Release runaway gate is unchanged. The build
registers 11 JUCE-free contracts.

The pass changes no resonance byte-to-loop-gain or input-compensation law
(OQ-09), card dispersion/drift evidence (OQ-10), oscillator/filter drive
calibration (OQ-15), startup-noise mechanism (OQ-16), physical upper-cutoff law
(OQ-18), or BA662 VCA law (OQ-19). Each remains open exactly because numerical
agreement with the declared ODE cannot validate the hardware that ODE is meant
to represent.

## Fractional VCF/resonance event pass — 2026-08-09 (grid defect fixed; no OQ closed)

**Work mode:** event-timing realization and independent numerical
qualification under the existing compatibility profile. **No original unit was
measured.** Step 10's −33.245 dB 8 kHz rejection is retained above as the dated
baseline; it is not the current engine result.

The converter's 4.2 ms pass, exact 23-write order and normalized `ordinal/23`
offsets are unchanged. For VCF and shared RESONANCE events only, production
purely peeks the next policy event in `(phase, phase + delta]`, including a
shared-resonance event across the pass wrap. The peek latches the converter
payload computed at that event without moving the official cursor or visible
target. The affected 522 µs hold then advances by its exact segmented
exponential at the endpoint and all seven unique Merson nodes. The next normal
scheduler poll commits the latched payload once and clears it, so intervening
host automation cannot recompute an event that has already happened in the
declared timeline.

Only event-containing intervals receive those exact trajectories. Each
affected voice interval adds six nonlinear cutoff/feedback mappings for the
seven node values. The integrator remains two fixed half-step Merson advances,
ten RHS evaluations and no solver split, retry, runtime selector, audio-sample
lookahead or latency change. The other converter destinations retain their prior behavior.
Engine contracts exercise all six card-VCF events plus next-pass resonance,
pure peek state, pass wrap, payload retention under automation and once-only
commit. A separate production-path probe replays `renderVoice` bit-exactly when
the event trajectory is connected; a deliberate `nullptr` mutation must and
does diverge.

The independent dynamic matrix still renders 19 physical takes for 24 logical
card/Character/thermal profiles and keeps the −40 dB gate unchanged. The
current engine-bound results are:

| Coverage | NRMS vs independent oracle | Result |
| --- | ---: | --- |
| 8 kHz / 4× | **−84.881 dB** | **PASS** |
| Six standard HQ selector paths | **worst −112.406 dB; best −116.317 dB** | **PASS** |
| 768 kHz / 1× | **−119.340 dB** | **PASS** |

Late `ceil` snapping is an expected **REJECT at −33.245 dB** and early
`floor` snapping an expected **REJECT at −32.007 dB**. Both controls retain
finite state and exact structural counts. This distinguishes fractional event
handling from an accidental gate relaxation, missing event or changed oracle.
The scheduler, write/count, hold and real-wiring/null-mutation contracts pass.

In the 48 kHz, six-card resonant 2,048-frame work window, counters record
**70 fractional peeks, 70 eventual commits, 120 affected voice intervals, 840
exact control nodes and 720 additional nonlinear maps**. Seven nodes and six
maps per affected interval account for the latter two figures; the fixed
Merson RHS/stage work and zero-recovery contract do not change.

Three fresh alternating Step-10/current pairs, each with seven repetitions at
48 kHz/block 256, put 4× CPU/audio at 0.653→0.666 idle (+2.056%), 0.670→0.682 plain
(+1.856%), 0.823→0.832 resonant (+1.096%) and 0.706→0.719 full-mixer Chorus
II (+1.755%). The 1× pairs are 0.164→0.169 (+3.072%), 0.172→0.176
(+2.483%), 0.221→0.225 (+1.807%) and 0.184→0.188 (+2.182%). The worst
current row is 0.832×; every case remains below realtime and the hard Engine
CPU gate passes. These paired meta-medians are machine/patch specific and
should not be mixed with Step 10's earlier standalone timing cohort.

Under the user-authorized from-scratch reset, four frozen-binary render passes
all exit 0. Two demo runs are byte-identical, as are two complete factory runs.
The canonical 23-file tree has manifest SHA-256
`764f2770d21a138163c756025551dc8ead7925f4cf003eb98e960234afc098ea`.
Its 20 WAVs are finite stereo PCM16, with demos at 44.1 kHz and factory previews
at 48 kHz; maximum absolute DC is 0.000000576 FS and the worst edge is
−46.96 dBFS. The factory result contains 128 finite, unique rows/tone blobs,
median gated RMS −21.48 dBFS, 31 rows containing samples above 0 dBFS, zero
near-silent rows and nine rows outside ±18 dB of the corpus median. This
qualifies the new canonical artifact set without claiming a delta against the
retired legacy renders or advancing any hardware question.

This pass changes no component value, hold constant, resonance/cutoff law,
converter ordering, oversampling selector, domain split or fixed 41-sample
latency. In particular, a model event at exact normalized `ordinal/23` is not
evidence that a JUNO-106 wrote there. **OQ-07 remains open** on acquisition,
droop, loading, injection and the meaning of 522 µs. **OQ-08 remains open** on
physical intra-pass offsets, jitter/data dependence and DCO restart effects.
The other VCF evidence questions named in Step 10 remain open unchanged.

## Fractional passive-hold pass — 2026-08-09 (supported scalar paths; no OQ closed)

**Work mode:** exact event-time realization and independent numerical
qualification under the existing compatibility profile. **No original unit
was measured.** The 4.2 ms pass, 23-write order and normalized `ordinal/23`
offsets are unchanged.

The Step-11 pure peek/latch/once-only-commit path is now generic across the 16
passive destinations in each pass: shared RESONANCE, common VCA, SUB and PWM,
plus six VCF and six VoiceVca writes. VCF and resonance retain their exact
522 µs endpoint and Merson-node behavior. Step 12 adds exact segmented
evaluation for the six component-derived 687 µs VoiceVca holds, the derived
9.08249 ms common-VCA pole and 10 ms SUB pole, and the exact continuous affine
4.7/2.632 ms PWM cascade. The six VoiceVca states, common VCA, SUB and two PWM
states move from `float` to `double`; state dimension does not change.

Pitch/DCO is excluded because a converter write's physical timer/ramp/
comparator/sub state remains OQ-08's question. NOISE is excluded because its
held-control and source/level law remains open under OQ-07/OQ-15/OQ-16. Both
destinations keep their sample-grid behavior. This boundary is intentional,
not missing coverage. There is no future sample, lookahead, new domain split
or latency.

The new independent long-double contract runs **1,105** actual
`Engine::process` state cases and **17** block-wrap cases, including a passive
event in a later q4 internal substep. Maximum process error is
**4.440892e-16**; state-to-float and consumer seams are **0 ULP**. Scheduler
coverage observes **23** ordinals, classifies exactly **16** passive, and
records zero Pitch/NOISE peeks, duplicates, payload failures, cursor/order
failures or pass-wrap failures. The timing/disconnection mutations are:

| Destination | Late | Early | Disconnected |
| --- | ---: | ---: | ---: |
| Common VCA | 0.009838067 | 0.009838067 | 0.009838067 |
| SUB | 0.008684780 | 0.008684780 | 0.008684780 |
| PWM | 0.02180667 | 0.02180667 | 0.02180667 |
| VoiceVca | 0.1480581 | 0.1480581 | 0.1480581 |

The separate sequential-PWM mutation is **0.000525998**. Common-VCA consumer
wiring spans 495 samples, agrees within **8.961428e-7** relative and differs by
**0.7034001** disconnected; SUB agrees exactly at printed precision and differs
by **0.6666667** disconnected. Thus a correct helper without real consumer
wiring cannot pass.

The 48 kHz, six-card resonant 2,048-frame work contract is:

| Semantic work | HQ 4× | HQ-off 1× |
| --- | ---: | ---: |
| Internal frames | 8,192 | 2,048 |
| Passive peeks / commits | 160 / 160 | 160 / 160 |
| VCF/resonance peeks / commits | 70 / 70 | 70 / 70 |
| Exact VCF intervals / nodes / maps | 120 / 840 / 720 | 120 / 840 / 720 |
| VCF steps / Merson halfsteps | 49,152 / 98,304 | 12,288 / 24,576 |
| VCF RHS / feedback | 491,520 / 491,520 | 122,880 / 122,880 |
| BBD line frames / shifts | 16,384 / 3,162 | 4,096 / 3,162 |

Passive events/commits and the VCF subset are equal-wall-time invariant across
the 44.1/48/88.2/96 kHz factor pairs. VCF and BBD work remains unchanged: the
common-host quality matrix is still REJECT/REJECT/PASS at 1×/2×/4× for both
44.1 and 48 kHz, and the Step-11 dynamic VCF results and fixed Merson counts do
not move.

The exhaustive 48 kHz latency fixture retains Pitch 0/100/201 and official
VoiceVca commit 70/192/315. HQ-off physical write and first nonzero gain now
land at **69/191/314**; HQ-on remains 70/192/315. Fixed host latency remains
**41 samples**. These are compatibility-profile coordinates, not original-unit
latency evidence.

Three alternating seven-repetition Step-11/current CPU pairs give exact 4×/1×
meta-medians of 0.677068→0.682068 / 0.171473→0.172614 idle,
0.697359→0.696475 / 0.179268→0.180543 plain,
0.847179→0.853898 / 0.228095→0.231158 resonant, and
0.731646→0.737013 / 0.191505→0.192318 full-mixer Chorus II. Their changes
are +0.738406/+0.665476%, −0.126874/+0.711718%,
+0.793131/+1.342855% and +0.733578/+0.424526%. Worst current load is
**0.853898×** and worst regression **+1.342855%**; the `<1×` and `+5%`
gates pass. These results are machine/patch specific.

A fresh warning-clean native Release/plugin-off build registers **12 JUCE-free
CTest contracts** and passes **12/12 in 323.07 s**, including the new passive-
hold contract. Five focused ASan+UBSan gates pass with halt-on-error and no
diagnostics: Engine passive-hold-only, independent passive hold, full DCO
quality, oversampling normal/work parity and dynamic VCF. The universal
Release/plugin-on build passes **13/13 in 344.05 s**. VST3, AU and Standalone
each contain `x86_64 arm64` and pass strict/deep ad-hoc signature verification
after packaging; all target macOS 11.0. A genuinely
translated Rosetta `x86_64` passive-hold run passes in **0.55 s**. The only
universal warnings are two inherited Step-11 `-Wfloat-equal` sites and nested-
Make's jobserver notice; no Step-12 warning class is introduced.

The user-authorized Step-12 audio regeneration is complete. Twin demo/factory
trees are byte-identical with manifests
`6e953be720d71a4947d41f4aa848dd228078b919520f7fccf006f27a19136667`
and
`dec0d91c6f2012519d713743e7c897c37d3c5cace2cec5db9e4648039791d57e`;
the canonical 23-file manifest is
`f9a6b274e7efb857a712ecaed1061e5251bd554e22462adce986e5e4d8158cbd`.
All 20 WAVs are finite stereo PCM16, with maximum absolute DC
`0.000000592814 FS` and worst edge `−46.962652 dBFS`. The 128-row factory
audit has exact median `−21.480711305 dBFS`, 31 overloads, zero silent rows,
nine `±18 dB` outliers and common gain `0.543091`. Relative to dated Step 11,
the median moves `+0.000034651 dB`, common gain moves
`0.543089 → 0.543091`, and the largest sample-peak movement is B77's displayed peak,
`+1.022040722 → +0.806945831 dBFS`; eight displayed rows change and every WAV
byte changes. These deterministic differences make no audibility claim and
close no hardware-evidence question.

This pass changes no component, state dimension, converter order, VCF/BBD law,
selector, split or fixed latency. It makes the normalized policy internally
exact; it does not recover physical timestamps. **OQ-07 and OQ-08 remain open**
with the same hardware-evidence requirements.

## HQ-off VCF qualification — 2026-08-09 (audit truth table; no OQ closed)

**Work mode:** numerical audit of the current declared model. **No original
unit was measured, no production DSP was changed and no hardware evidence was
created.** The Step 10 numerical-integrator, Step 11 fractional VCF/resonance
and Step 12 passive-hold sections above remain dated history rather than being
rewritten as if they had always included this coverage.

Step 13 extends only `AuditVcfDynamicQuality`. The first profile repeats the
four exact stored-byte cutoff/resonance snapshots over 12 converter passes and
uses the exact 23-write/fractional-control schedule, independent RK64/RK128 and
all 19 physical takes representing 24 logical card/Character/thermal profiles.
Actual HQ-off scheduler and production-`renderVoice` wiring probes cover every
8/44.1/48/88.2/96 kHz q1 selector row, including a rejected disconnected-
trajectory mutation. Finite/reset state, zero recovery, exact writes/cursor/
control intervals, real selector rate and within-run raw identity are required.
The unchanged −40 dB waveform and −80 dB convergence gates give:

| HQ-off q1 moving-control coverage | NRMS | RK64/RK128 | Frames | Result |
| --- | ---: | ---: | ---: | --- |
| 8 kHz supported endpoint | −53.279 dB | −110.051 dB | 147 | **PASS, moving only** |
| 44.1 kHz standard | −84.738 dB | −142.698 dB | 1,198 | **PASS** |
| 48 kHz standard | −86.568 dB | −144.403 dB | 1,395 | **PASS** |
| 88.2 kHz standard | −97.893 dB | −154.666 dB | 3,421 | **PASS** |
| 96 kHz standard | −99.618 dB | −157.689 dB | 3,814 | **PASS** |

The 8 kHz q1 late/ceil and early/floor timing mutations reject at
**−27.259/−26.860 dB** while remaining structurally exact. Moving NRMS and
mutation metrics are fenced by ±0.05 dB scalar goldens. Moving RK64/RK128
convergence uses a distinct ±0.15 dB cross-architecture fingerprint band at
its roughly −110…−158 dB numerical floor; the absolute −80 dB gate is
unchanged. Raw hashes remain within-run identities and are not frozen across
architectures.

The second profile prevents that smooth result from becoming a false
admission. It is static nominal Character 0—not a claimed hot × 19/24 schedule
cross-product—with an analytic 19-harmonic, 20 kHz-band-limited 1,046.502 Hz
saw, production-compensated 2.4 V drive, 16 kHz cutoff and `k=3.8`. Independent
RK64/RK128, a checked 4,097-tap boundary FIR and 32,768-frame exhaustive
residual/oracle spectra retain the −40 dB NRMS, <−60 dBc residual and ≤−85 dBc
oracle gates:

| Standard HQ-off q1 hot profile | NRMS | RK64/RK128 | Residual / oracle off-mask | Exact bins | Combined result |
| --- | ---: | ---: | ---: | ---: | --- |
| 44.1 kHz | −12.538 dB | −135.643 dB | −44.602 / −93.242 dBc | 14,618 | **REJECT** |
| 48 kHz | −14.269 dB | −138.574 dB | −48.081 / −93.163 dBc | 13,412 | **REJECT** |
| 88.2 kHz | −30.417 dB | −159.637 dB | −85.765 / −97.212 dBc | 7,195 | **REJECT** |
| 96 kHz | −33.080 dB | −162.578 dB | −88.712 / −97.141 dBc | 6,592 | **REJECT** |

Every hot row passes structural, oracle, filter, selector, finite and zero-
recovery gates but fails waveform NRMS; 44.1/48 kHz also fail residual
off-mask. Hot scalar metrics have ±0.05 dB goldens and the unmasked-bin counts
are exact. The frozen standard rule is `moving structural && moving quality &&
hot structural && hot quality`, hence all four standard HQ-off rows are
**REJECT**. The 16 kHz hot fixture cannot exist at an 8 kHz host, so 8 kHz is
explicitly a moving-control selector endpoint only and never a fifth hot row.

This changes the **current numerical diagnosis**, not an OQ answer. The moving
control law is accurate at q1, but the hot result exposes insufficient lower-
grid input density and nonlinear pre-grid foldback: once a sparse q1 input/
control sequence has driven the nonlinear cascade, later filtering cannot
reconstruct the lost trajectory. The improvement with host rate supports that
software mechanism and says nothing about the physical JUNO-106.

A future local VCF boundary—q4 at 44.1/48 kHz and q2 at 88.2/96 kHz—is allowed
only as a new, atomic candidate. It must retain the same-host smooth and hot
RK/spectral gates, exact 23-write
fractional schedule, all-card coverage, production wiring/null mutation, the
existing ±0.05 dB signal/mutation and ±0.15 dB moving-convergence fingerprint
bands, finite state and zero recovery. It must preserve four capacitor
states plus input/control history across quality, rate and block boundaries;
prove q4/q2 decimator impulse/fractional-delay alignment with no lookahead and
no change to the fixed **41-host-sample** report; qualify DCO, sub and shaped-
noise input transfer including wall-clock PSD and RNG-state invariance; and
publish paired local-VCF and whole-engine CPU measurements that pass `<1×`
realtime and `+5%` regression fences while demonstrating an actual saving.
Step 13 implements none of this production boundary.

The same research pass produced one **deferred BBD lead**, not an
implementation. The current q1 low-drive deterministic NRMS/BGA/SGA rows at
44.1/48/88.2/96 kHz are −3.511/4.764/−26.934,
−5.263/3.406/−30.746, −18.390/0.071/−41.304 and
−20.051/0.016/−46.044 dB. A scratch exact analog-input-at-edge plus exact
fractional-output-event prototype moves them to −15.859/0.00123/−54.044,
−18.344/0.00103/−53.747, −37.660/0.00071/−97.307 and
−40.408/0.00065/−96.646 dB. It clears BGA everywhere and SGA at 88.2/96 kHz,
but only 96 kHz passes the complete scratch gates. Stochastic/noise state is
unqualified. The candidate is not shipping and does not reclassify BBD quality
or close OQ-01/OQ-03/OQ-04/OQ-20.

Final Step-13 portability qualification is green: a warning-clean native
Release/plugin-off tree passes **12/12** contracts in **375.88 s** while the
translated audit shares the machine; the registered dynamic contract passes
alone in **38.90 s**, and its focused ASan+UBSan self-test passes with
`halt_on_error=1`, `detect_leaks=0` and no diagnostic. A universal
`x86_64 arm64` audit executable passes natively on
arm64 and under genuine Rosetta translation in **963.10 s**. The translated
run exposed a 0.103 dB difference only at the moving convergence fingerprint's
roughly −158 dB floor. Its explicit ±0.15 dB portability band does not move the
−80 dB admission gate or any signal/hot/mutation band.

No audio, latency or CPU measurement was produced. The untouched Step-12
canonical audio manifest remains
`f9a6b274e7efb857a712ecaed1061e5251bd554e22462adce986e5e4d8158cbd`.
OQ-07/OQ-08/OQ-09/OQ-10/OQ-15/OQ-16/OQ-18/OQ-19 remain open with exactly the
hardware-evidence boundaries already stated.

## Dynamic BBD qualification — 2026-08-09 (broader audit; no OQ closed)

**Work mode:** numerical audit of the current declared model. **No original
unit was measured, no production DSP was changed and no hardware evidence was
created.** Step 14 adds the separate JUCE-free
`YouKnow106.BbdDynamicQualityContract`; it changes no `Source`/`Tests` file,
selector, state, latency, CPU path, preset or audio. The dated Step 8/9 BBD and
Step 13 VCF sections remain history rather than being rewritten as if they had
always covered this scope.

The candidate is public `Chorus::process`, including both production BBD lines.
The audit obtains the real q4/q2/q1 factor from the Engine and crosses the real
shipping `downsamplePair` implementation and q4 cascade. It does **not** run the
surrounding full `Engine::process` clip/slew/output call site and therefore
does not qualify that seam or its fixed 41-host-sample latency.

An independently written reference integrates the triangle LFO, solves every
affine clock edge continuously, advances two antiphase 128-cell lines through
the nonlinear transfer and per-edge loss pole, and integrates the six-state
output support systems across clock and mute-load changes. RK4×4/RK4×8,
checked 4,097-tap q16 decimation, declared boundary delay and no lag search
separate it from the shipping sample grid. Its rounded
**7,234.0/9,688.0/10,377.0/23,461.38 Hz** support constants are frozen audit
policy for the already declared model boundary. They are not promoted to
measured JUNO-106 values and do not answer OQ-04.

The 0.72 s public schedule is I `[0,.30)`, Off `[.30,.36)`, II `[.36,.60)`
and Off `[.60,.72)`, with the oscillator, BBDs and RNG continuing through Off.
Its 997/5,213 Hz analytic card reaches **1.500000010 Vrms** at the model
input-support boundary against a 1.500000000 Vrms target. The unchanged gates
are whole L/R/M/S NRMS ≤−40 dB, I and II ≤−50 dB, Off ≤−60 dB, aligned-II
residual <−60 dBc and RK convergence ≤−80 dB. Whole/I/Off/II compare
`[.12,.64)`, `[.15,.30)`, `[.30,.36)` and `[.40,.60)`; the aligned-II BH92
residual examines every 20 Hz–20 kHz bin without masking:

| Actual selector | Worst L/R/M/S | I / Off / II | Residual | Convergence | Result |
| --- | ---: | --- | ---: | ---: | --- |
| HQ 44.1 q4 | −60.761 dB | −68.498 / −66.911 / −57.884 dB | −75.664 dBc | −204.699 dB | **PASS** |
| HQ 48 q4 | −60.497 dB | −62.899 / −70.748 / −57.590 dB | −76.378 dBc | −205.088 dB | **PASS** |
| HQ 88.2 q2 | −58.580 dB | −74.118 / −73.000 / −55.643 dB | −75.549 dBc | −223.287 dB | **PASS** |
| HQ 96 q2 | −59.249 dB | −74.753 / −74.063 / −56.313 dB | −76.229 dBc | −226.078 dB | **PASS** |
| HQ 176.4 q1 | −58.574 dB | −74.177 / −73.204 / −55.639 dB | −75.454 dBc | −245.943 dB | **PASS** |
| HQ 192 q1 | −59.246 dB | −74.809 / −74.023 / −56.311 dB | −76.123 dBc | −251.768 dB | **PASS** |
| HQ-off 44.1 q1 | −24.133 dB | −24.077 / −23.733 / −24.056 dB | −24.841 dBc | −204.699 dB | **REJECT** |
| HQ-off 48 q1 | −25.640 dB | −25.594 / −25.236 / −25.561 dB | −26.346 dBc | −205.088 dB | **REJECT** |
| HQ-off 88.2 q1 | −36.300 dB | −36.337 / −35.999 / −36.204 dB | −36.993 dBc | −223.287 dB | **REJECT** |
| HQ-off 96 q1 | −37.768 dB | −37.822 / −37.488 / −37.669 dB | −38.458 dBc | −226.078 dB | **REJECT** |

The noise-only waveform NRMS is informational because differing fractional
edge times decorrelate samples. The normative stochastic gates instead require
plain-RMS level error ≤0.10 dB, four-band Welch-power error ≤0.75 dB,
correlation error ≤0.02, absolute correlation ≤0.05 and II/I-delta error
≤0.05 dB, together with exact edge/RNG/index/phase/LFO/mode/wet-gain ledgers,
finite transfer state and a bit-exact held-noise relation to that production
transfer. HQ maxima are **0.072/0.561/0.003/0.012** for
level/band/correlation-error/mode-delta-error and pass. HQ-off level errors are
**0.696/0.524/0.184/0.171 dB**, so all four reject; 44.1/48 kHz also exceed
the band gate at **1.432/1.106 dB**. Four-band analysis uses 4,096-sample BH92
windows, 2,048-sample hop and averaged unnormalized power over I `[.15,.30)`
and II `[.40,.60)`.

All ten rows retain PASS infrastructure—exact selector and internal rate,
finite values and the structural schedule ledger—so an expected HQ-off quality
failure cannot hide broken state. Complete mode-I and mode-II cycles pass at
all six unique internal grids, including both triangle corners and wrap.
Same-family raw internal renders match before decimation. The registered
aggregate mutates captured output for disconnected/collapsed/inverted stereo
and correlated noise, and fences frozen source-local controls for common
clocks, linear transfer, snapped edge input, always-connected Off loading and
doubled noise. Separate source-local review rejects disabled output correction.

The result closes the prior **software-audit scope gap**: the declared BBD now
has nonlinear, modulated, stereo and stochastic evidence in addition to the
dated low-drive line fixture. It deliberately closes none of these hardware
tasks:

- **OQ-01 remains open:** exact model-LFO/clock integration does not measure a
  JUNO-106 clock or decide the physical timing law.
- **OQ-03 remains open:** exact RNG and statistical agreement validate the
  declared product noise, not its absolute hardware PSD, correlation, spurs or
  physical cause.
- **OQ-04 remains open:** the audit-policy support corners do not resolve real
  MN3009/source/loading transfer.
- **OQ-20 remains open:** testing the declared connected/muted load and wet
  gain does not measure TR11/TR12 on-resistance, leakage or switching transient.

Production HQ-off repair remains deferred. A candidate needs a causal
bandlimited local BBD boundary that clears the same ten-row gates; preserves
both lines' bucket, phase, RNG, held-transfer and support state through quality,
rate and block changes; proves the local decimator and surrounding full-engine
alignment without lookahead or a change to the fixed 41-sample report; and
passes paired BBD-only plus whole-engine CPU gates before any selector change.

The Step-14 inventory is **13 plugin-off / 14 plugin-on** contracts. The
targeted metric run passes in **44.12 s** (audit elapsed **43.89 s**). A fresh
warning-clean native arm64 Release/plugin-off build registers **13
JUCE-free contracts and passes 13/13 in 381.25 s**; the BBD dynamic, VCF dynamic
and passive-hold tests take **43.46/37.30/0.62 s**. A fresh warning-clean
ASan+UBSan build passes the existing static VCF/BBD seam and new dynamic BBD
contract **2/2 in 126.85 s** (40.80/86.05 s), under halt-on-error with leak
detection disabled and zero diagnostics. A fresh universal `arm64;x86_64`
Release/plugin-on all-target build passes in **114.17 s**, targeting macOS
11.0. Only nested-Make's inherited jobserver notice and the pre-existing
`YouKnow106Engine.h:431/787` `-Wfloat-equal` warnings remain; the new audit is
warning-clean. The universal serial matrix passes **14/14 in 400.62 s**; its
BBD dynamic, VCF dynamic and PluginProcessor tests take
**44.86/38.11/11.93 s**. The explicit universal full audit passes on arm64 in
**43.69 s**; its binary contains `x86_64 arm64` and both slices target macOS
11.0. A genuine translated full-oracle launch printed `uname -m=x86_64` and
`sysctl.proc_translated=1` but was intentionally stopped at **2666.66 s**,
still in the first hot RK4×4 solve: x86's 80-bit `long double` reference
arithmetic is software-emulated on arm64 and projects to a multi-hour run. That
incomplete launch is neither a PASS nor a quality failure; no full x86 audit is
claimed or used as a portability admission gate. At frozen audit-source
SHA-256 `33a0818c00560a502fa774223030409a4310ffe0053df3e23ae5bc5aad348228`, a
warning-clean universal target rebuild passes in **3.15 s**. The bounded
shipping-only `--shipping-self-test` bypasses all audit alignment, reference
and audit-FIR work while retaining raw internal and actual shipping-decimator
boundaries. It passes on arm64 in **0.90 s** self-reported (**1.37 s** external
wall) and in a genuine translated Rosetta process in **3.75 s** self-reported
(**3.96 s** external wall), where it prints `x86_64` and
`sysctl.proc_translated=1`. All ten public `Chorus::process` selector rows pass
the input-support card, raw-boundary and decimator-boundary finite checks,
hot/noise schedule ledgers, within-run same-family identity and both full mode
cycles at all six grids. No audit reference, audit FIR, RK, continuous-oracle,
quality-classification or mutation path runs or prints in this mode. This is
shipping/ledger portability evidence, not a continuous-reference x86
pass. Prescribed isolated packaging passes in **3.41 s**: VST3, AU and
Standalone each contain both slices with minimum macOS 11.0 and pass strict and
deep ad-hoc verification; their respective CDHash prefixes are `7a102a35…`,
`21b94c10…` and `fb7f0da6…`. No audio was rendered; the untouched Step-12
canonical manifest remains
`f9a6b274e7efb857a712ecaed1061e5251bd554e22462adce986e5e4d8158cbd`.

## Selected-Cut C14 load correction — 2026-08-10 (nominal fixed modes qualified; OQ-21 remains open)

**Work mode:** narrow production correction plus independent nominal-circuit
audit. **No original unit was measured, no switch click was inferred and no
full switched-network state-space was claimed.**

Service Notes p. 15 places C14 10 µF NP ahead of TC4052BP YCOM and R39
33 kΩ from that common node to ground. In either selected Cut position,
R21/R23 1 MΩ remains a direct mux-side shunt even though C10/C11 opens the
far-side 47 kΩ path at the sub-hertz asymptote. Production now uses

`Rload = R39 || 1 MΩ = 31,945.788964 Ω`,
`τ = C14 · Rload = 319.457890 ms`, and
`fc = 1/(2πτ) = 0.498203201 Hz`.

The previous R39-only 0.482287706 Hz selected-Cut result is superseded. Boost
and Flat remain 0.820915 Hz from `33 kΩ || 47 kΩ`; C10/C11 retain their
225.8/720.5 Hz selected Cut poles, and the Boost shelf does not move. The
single C14 state survives mode, numerical-rate and preserving-clear changes.
Hard output-path clears (including public panic) and engine reset discharge it.
Whole versus 37-sample block partitions are
sample-identical in the circuit fixture.

The new JUCE-free `YouKnow106.HighPassNetworkContract` independently stamps
the nominal p. 15 fixed-position components into long-double complex MNA. Its
converged 240,001-point 0.001 Hz–20 kHz sweep reports maximum absolute
production magnitude/phase residuals of:

| Fixed mode | Magnitude | Phase |
| --- | ---: | ---: |
| Boost | 0.008363013 dB | 0.056091136° |
| Flat | 0.000000391 dB | 0.000001289° |
| Cut II | 0.011136100 dB | 0.042871357° |
| Cut III | 0.003887336 dB | 0.013452200° |

The old R39-only load, a 1 MΩ incorrectly placed across the 47 kΩ leg and
swapped C10/C11 mutations reject. A reset-on-mode-change mutation is observable.
Thirty-six production-updater probes bind all four modes to the nine declared
endpoint/common/oversampled policy grids. A separate helper-derived scalar TPT
prediction evaluates **147,492 finite frequency responses** over the explicit
8/32/44.1/48/88.2/96/176.4/192/768 kHz grids. The seven common grids remain
inside 0.02 dB/1.65°. At 8 kHz, Cut II/III are explicitly
**ENDPOINT_LIMITED** at 0.024664910 dB/3.147789295° and
0.236403340 dB/9.902784181° but pass the separate 0.25 dB/10° fence. At the
32 kHz endpoint-HQ grid, Cut III alone is **ENDPOINT_HQ_LIMITED** at
0.014694508 dB/2.506236943° and passes the 0.03 dB/3° fence.

Those are numerical endpoint classifications, not TC4052 evidence. The ideal
fixed-mode solve does not determine on resistance, leakage, off-capacitance,
charge injection, break-before-make timing or the initial-charge projection of
deselected C10/C11. Consequently **OQ-21 stays P2 and open** for the complete
switched state-space and every directed mode-change transient.

CMake now registers **14 plugin-off / 15 plugin-on** contracts. A fresh
warning-clean native Release/plugin-off tree passes **14/14 in 367.27 s**;
Engine, Circuit and HPF take **175.77/3.88/0.77 s**. Focused ASan+UBSan
Circuit/HPF passes **2/2 in 8.41 s** (7.50/0.91 s), under halt-on-error with
leak detection disabled and no diagnostic. A fresh universal
`x86_64;arm64` Release/plugin-on build completes in **102.30 s**, registers 15
contracts and passes **15/15 in 382.36 s**; HPF and PluginProcessor take
**0.98/11.49 s**.

VST3, AU and Standalone each contain `x86_64 arm64`, target minimum macOS 11.0
and pass strict/deep ad-hoc signature verification. Their CDHash prefixes are
`965c40c0`, `9290dacb` and `26f74b2a`. The explicit HPF audit passes on arm64
in **0.42 s** and under genuine Rosetta `x86_64` translation in **73.91 s**.
All printed metrics agree to displayed precision; only three harmless
equal-valued frequency locations differ for Flat's near-zero magnitude maxima
in the analog, 8 kHz endpoint and 32 kHz endpoint-HQ rows. No result moves.

Three alternating base/current CPU pairs retain exact raw-float fingerprint
identity. Worst current load is **0.837× realtime**, largest positive
meta-median change **+0.1128%** and worst individual paired change
**+2.1991%**; every result remains below 5%. The scalar change adds no
per-sample work, state, storage or latency and does not promote the model into
a hardware-switch claim.

The verified non-audio source SHA-256 set is:

| Source | SHA-256 |
| --- | --- |
| `CMakeLists.txt` | `33b31ca661c1538d19dcafac12add1838e576ff074399069eb2a7744d60ba524` |
| `Source/DSP/YouKnow106Engine.cpp` | `ed8fef679a94b0667569e1b0281f4381a46aa942c490be9b4765b445e1963182` |
| `Source/DSP/YouKnow106Engine.h` | `9ae15f16b795bf752693eb146c137a63f486d1ee29148dce0b38c58fec453b52` |
| `Tests/YouKnow106CircuitTests.cpp` | `a3f6168c3602cee5345e21e1e2b564b67e7a3981082ca0604dca74be3d59d998` |
| `Tools/AuditHighPassNetwork.cpp` | `341030ab93d8506547176dd30c27ea65684bd96a0a92d0ee681da23b953866eb` |

The Step-15 audio handoff is complete. Two independent demo renders and two
independent full factory renders are pairwise byte-identical, with manifests
`b42e87351748d79ad91cfbfb29ca85fce99a08b0c2a090754c4cba7bf69a9434`
and
`0783040d94af15527450f8062813ac03ae6c6def0184574c037a5cf4106767e8`.
The renderer-owned 22-file and installed canonical 23-file manifests are
`bc1564713b46151a77fbbc3c5403f8bd829955cd9ff9dbcb5b2bd6cc1e13c614`
and
`0280ae697c209f513283b0c1cac3ad451528f5e6909046ba26d592dce459a430`,
superseding Step 12's
`f9a6b274e7efb857a712ecaed1061e5251bd554e22462adce986e5e4d8158cbd`.

All 20 WAVs are non-silent finite stereo PCM16, with maximum absolute DC
**0.000000592814 FS** and worst edge **−46.962652 dBFS**. The CSV contains
128 finite unique slots and tone states, median **−21.480711305 dBFS**,
31 overload, zero near-silent and nine median-outlier rows. Nine demos remain
byte-identical to Step 12. Only demo 09 and the ten common-gain previews
change, each by at most two PCM16 LSB: demo 09 reads **−85.129 dB NRMS**.
A86 is worst by L2 NRMS at **−54.771 dB**; A17 reads **−83.872 dB** and
uniquely reaches a two-LSB peak. Twenty-nine CSV rows move only at fine
precision: A17's overload-sample count **7000 → 6999**, common gain
**0.543091 → 0.543092**, and B51's displayed crest **23.36 → 23.35 dB**.
Exactly 14 tracked `Docs/audio` files change—one demo, ten previews, the audio
index, generated factory README and metrics CSV.

These render differences are bounded provenance, not an audibility result,
switch-click measurement or full switched-network validation. OQ-21 remains
open. **Step 15 is complete. DOCS FROZEN.**

### Step 16 — low-rate main-noise TPT safety, not OQ-15/OQ-16 closure

**Dated 2026-08-10.** C41 100 pF and R79 330 kΩ independently derive the
physical main-noise low-pass at **4822.877063391 Hz**. The former direct
`tan(pi*fc/internal_rate)` updater became unstable at a supported endpoint:
8 kHz q1 gave `g = -2.986132794`, pole `-2.006982013`. Its private low-pass
state grew to approximately `7.87e294` in 0.25 s; downstream VCF/output finite
recovery could hide the state poison, so finite final audio was a false pass.

Production now designs at
`min(4822.877063391 Hz, 0.45 * internal_rate)`. The old `2fc` instability seam
is **9645.754126782 Hz** and the cap releases at
**10717.504585313 Hz**. The fixed 8 kHz q1 coefficient/pole are
`6.313755512/-0.726542677`; 8 kHz q4 correctly uses its 32 kHz internal grid
and keeps the physical corner. State/reset/rate-transition semantics and the
physical-corner helper remain unchanged. This block/rate-time coefficient
choice adds no state, storage, latency or per-sample work.

The JUCE-free `YouKnow106.NoiseSourceQualityContract` uses an independent
long-double component oracle. It exercises 21 explicit rate rows, 4,007 dense
8–12 kHz and adjacent-float seam cells, a 4,096-frame TPT impulse, and the real
public Engine seeded trajectory from idle/no-note processing into driven noise
without reset, plus block partitions, reset and q1/q4 transitions. It requires
finite positive `g` and `|pole| < 1`; worst cap-active error against the
physical analogue RC is **1.697765947 dB**. Twelve cap-inactive
current-versus-legacy families remain exact within each architecture/run. All
nine named controls reject: no cap, host-rate cap, 0.44/0.46/0.49/0.55 caps,
`abs(tan)`, post-tan clamp and sanitize-only.

The current inventory is **15 plugin-off / 16 plugin-on**. Fresh native arm64
Release/plugin-off configure/build takes **1.21/18.57 s**, warning-free; the
serial matrix passes **15/15 in 381.56 s** (Engine/Circuit/Noise
**181.42/4.12/1.81 s**). Fresh ASan+UBSan configure/build takes
**1.31/15.04 s**, warning-free; Circuit/Noise passes **2/2 in 10.29 s**
(7.64/2.65 s) with halt-on-error, leak detection disabled and zero
diagnostics. Fresh universal `arm64;x86_64` Release/plugin-on configure/build
takes **33.9/121.7 s**; its exact serial matrix passes **16/16 in 401.47 s**.
The new audit is warning-clean; universal translation units repeat only two
pre-existing Engine-header `-Wfloat-equal` sites.

Prescribed packaging takes **3.92 s**. VST3, AU and Standalone contain both
slices, target minimum macOS 11.0 and pass strict/deep ad-hoc codesign, with
CDHashes `340ce9f3a80aeb589582911db16d66b37b49cab5`,
`39d3767acba6afca02d0a0402fd641d8d44c5293` and
`afc0333071a2b1ebdd3f7414d8bcc1402eed361c`. Explicit audit runs pass on
native arm64 in **1.769 s** and under genuine Rosetta `x86_64` in
**56.952 s**, printing `sysctl.proc_translated=1`. Scalar metrics agree; raw
identity is claimed only within each architecture, not across libm/FP paths.

Three alternating seven-repetition CPU pairs retain exact normal/work/base/
current fingerprints and show no counter leakage. Eight 4×/1× meta-medians
span −0.471% to **+0.334%**, global ratio is **1.000678**, worst pair median is
+3.068%, and worst current raw load is **0.972737× realtime**. One isolated
+12.766% raw timing outlier is preserved in the record but does not change the
robust paired classification.

Issue 16 records 96 kHz/24-bit calibration work on one serial-439522 JUNO-106
using Borish replacement voice chips, recalibrated in 2022; surviving archive
provenance is incomplete. Its candidate VCA slope/endpoint and oscillator
ratios may guide
OQ-15 captures but do not retune the nominal laws. The mixed original-board/
replacement-module path cannot establish OQ-16's raw TP8 main-noise PSD,
amplitude or distribution. This step is numerical repair and qualification,
not a hardware or audibility claim; **OQ-15 and OQ-16 remain open**.

Final audio qualification is exact identity, not an audibility result.
Sequential demo A/B renders take **96.24/94.13 s** and full factory A/B takes
**440.98/461.15 s**. Both pairs are byte-identical and exactly match Step 15.
Demo/factory manifests remain
`b42e87351748d79ad91cfbfb29ca85fce99a08b0c2a090754c4cba7bf69a9434`
and
`0783040d94af15527450f8062813ac03ae6c6def0184574c037a5cf4106767e8`;
the renderer-owned 22-file manifest remains
`bc1564713b46151a77fbbc3c5403f8bd829955cd9ff9dbcb5b2bd6cc1e13c614`
and the current canonical 23-file manifest is
`8346a817bd215808112510dc3d37b5a8fac3a5f401aa93d117b2b9f0912ba8dd`.
Only `Docs/audio/README.md` changes for Step-16 provenance, at SHA-256
`8e4333223c3d58406be7919d7959327029094a7559e57d1733c9c5c943dd2483`.
Candidate demo/factory binary hashes are
`0ae8dec6e0ddec230aab5fbb8b8efbd63a4875900721ee60aff5371468fd9cd3`
and
`e74569b26d5bc8437a0c88b325d55db4bba7730e8fa40a391b2273b18aa08498`.

All 20 WAVs are finite non-silent stereo PCM16 (ten 44.1 kHz, ten 48 kHz),
with maximum absolute DC **0.000000592814 FS** and worst edge
**0.004486083984 FS / −46.962652 dBFS**. The CSV has 128 finite unique
slots/tones, median **−21.480711305 dBFS**, 31 overload, zero near-silent and
nine outliers, range A86 **−61.956882039 dBFS** to A48
**−8.749547764 dBFS**, and common gain **0.543092**. No WAV, metric CSV,
preview or renderer-generated factory-text output changes. **Step 16 is
complete. DOCS FROZEN.**

### Step 17 — lifecycle correction, no OQ closure

**Dated 2026-08-10.** A code-path audit verified that the documented
"initial idle host-snapshot priming" exception could recur. After the output
path had remained quiet for its 40 ms drain interval, each ordinary host-block
`setParameters()` call directly replaced the shared RESONANCE, common VCA,
PWM, SUB and NOISE target/held states and cancelled a pending passive-hold
event. It bypassed the 23-write cursor and therefore the existing 522 µs
resonance/noise, 9.08249 ms common-VCA, 4.7/2.632 ms PWM and 10 ms SUB
trajectories.

The Step-17 correction permits direct priming only before the first valid
positive-length process call in a prepared lifecycle. Once processing starts,
silence cannot re-arm it; shared controls wait for their normal scan writes
and hold responses until the next hard reset or `prepare()`. This preserves a
practical saved-state startup while removing an unphysical post-startup
shortcut.

This result settles only a software lifecycle contradiction. It supplies no
new hardware capture and does **not** answer OQ-07's sample/hold capacitance,
mux, acquisition, droop, jitter or physical scan-timing questions; it does not
answer OQ-08's exact event offsets or changed-pitch electrical state; and it
does not change any other OQ. The startup behavior remains product policy.
No state, storage, latency or per-sample work was added.

Focused lifecycle qualification passes: the expanded startup test and new
ordered-idle-edit test cover repeated restore snapshots, invalid/zero/
unprepared calls, more than 40 ms silence, immediate Note On, the exact
half-interval common-VCA event and next-poll commit, 257-frame partition
identity, panic, reset and unchanged 41-sample latency. The old recurring-idle
mutation rejects with six assertions.

Final qualification is green within its declared chronology. Native Release
passes **15/15 in 473.02 s** after an **8.23 s** warning-clean build;
ASan+UBSan focused lifecycle coverage passes in **0.64 s** without a
diagnostic. Universal compilation passes in **127.43 s** with only the two
inherited Engine-header warnings. Its initial serial run retained tests 1–15;
a stale PluginProcessor reference chronology then failed. The test-only
correction keeps the full dump at sample 0 only on the MIDI-driven path,
preloads the reference from the same quantized dump before prepare, and gives
both paths the edit/note at samples 1/2. This yields equivalent pre-first-render
converter/hold chronology, relaxes no threshold and passes the registered
suite in **11.40 s**, so all 16 contracts are green without misstating them as
one uninterrupted 16/16 log. Native/genuine-Rosetta focused
startup runs pass in **0.04/0.24 s**. Packaging and CPU gates pass; worst CPU
meta-median/pair-median/aggregate are **+1.630160%/+2.939967%/+0.306995%**,
with worst candidate median **0.868× realtime** and exact fingerprints.

Two demo and two full-factory renders are pairwise and Step-16 byte-identical.
The unchanged renderer-owned manifest is
`bc1564713b46151a77fbbc3c5403f8bd829955cd9ff9dbcb5b2bd6cc1e13c614`;
the final audio-index/canonical-23 hashes are
`a6bb49018b312bab2a8e82dcabb9bc105ccd19e076bf39ec0e580631108ed3aa` /
`19053f2cb7b57eef5fccb7bfa9f7f5e14ab2e1e932af1672b5138565430d196c`.
No renderer-owned payload changes. **Step 17 is complete. DOCS FROZEN.**

## Proposed-mechanism adjudication pass — 2026-08-19 (two arithmetic corrections; no OQ closed)

Two batches of proposed "realism win" mechanisms were checked against this
queue, the claims boundary and the shipping code. No DSP constant moved. The
value of the pass is in what it corrected about the tree's own claims, and in
separating mechanisms that are genuinely unrecorded from ones already shipped or
already owned by an open question.

**Batch 1 — oscillator/filter/chorus/output constants.** Four proposals were
already adjudicated and declined in tree, with reasons the proposals did not
engage: the 40 kHz expo ceiling and 270 pF integrator (best-in-class queue, item
1–2 of the recorded-contradictions list), the 0.275 input compensation (its full
Open80017a derivation is written into the shipping header, declined as one
reconstruction lineage against OQ-09's measured family), the hyperbolic chorus
sweep (contradicted by the only trajectory measurement in existence — see OQ-01
above), and the chorus clock bleed (amplitude is an unvalidated placeholder
pending OQ-03). One was live and correctly identified: the noise-versus-sub
balance on the drum programs, already recorded under OQ-15 above, where the
direction points at the unanchored `subMixVolts` rather than the ±4 dB-anchored
noise leg — a proposal to raise noise instead would break the anchor established
on 2026-08-17. One produced the OQ-18 correction recorded above. One — an
always-on summing-bus noise floor — restated gap 7 with a −105…−112 dBFS target
that the derivable Johnson figure (≈1.32 µVrms, ~124 dB below a nominal patch)
does not support; the audible version still needs the µPC1252H2 excess-noise
figure and OQ-16's capture.

**Batch 2 — eight further mechanisms.** Outcomes:

- **Even-harmonic self-oscillation from stage DC asymmetry — already shipped.**
  `Voice::filter.offsetVoltage[4]` carries per-stage offsets seeded per card and
  referred into node coordinates through `stageAttenuation`, alongside per-stage
  pole scatter, under `enableVcfStageOffsets` (default on). The code comment
  states the same purpose the proposal does: four identical poles give a
  self-oscillation "more symmetric and purer than any real four-section filter
  produces". The narrower claim — that the BA662 resonance VCA specifically
  injects DC into stage 1 — is not modelled, and the header already declines to
  reuse VR30's trim as a BA662 signal-input offset. Unanchored magnitude; no
  change.
- **Control-current (I_abc) slew feeding through to a differential transient —
  not modelled, unanchored.** The mechanism is real in principle and distinct
  from what the cascade carries, but its size is set by an unmeasured V_be
  mismatch, and the 522 µs hold law already governs how fast the control moves.
  Recorded here only; belongs with OQ-09/OQ-18 if a measurement ever bounds it.
  The proposal's acoustic account misroutes it through C59, which is the
  VCF→VCA coupling the engine already carries.
- **8253 reload on terminal count — filed under OQ-08** (see above).
- **PWM comparator propagation delay — magnitude does not survive checking.**
  A constant t_pd shifts both edges alike and so moves the pulse in time rather
  than changing its width; only the *difference* in t_pd between the two
  crossings tilts duty, which is second order in overdrive. Even taking the
  whole delay as tilt, 1200 ns against the top of the 8' range (2093 Hz) is
  0.25%, and 300 ns at 1.5 kHz is 0.045% — not the 0.5–1% claimed. The
  comparator part identity is also not established in tree. No change.
- **Chorus modulator triangle up/down asymmetry — new, and recorded under
  OQ-01** above. The only batch-2 item that adds something the queue did not
  have.
- **Anti-phase inverter gain tolerance — same capture, recorded with it.** A
  1–2% R9/R10 pair is ±2% of line B's modulation depth; folded into the OQ-01
  note rather than given its own entry, because the TP3 capture that bounds it
  is already on OQ-01's needed-output list.
- **TC4052 deselected-capacitor charge memory and mode-change transient —
  already OQ-21**, at P2, in more detail than the proposal carries: parts,
  placement and control are settled, and the deselected decays are established
  at 15.705/4.9209 ms against the proposal's estimated ~22 ms. Nothing new.
- **Headphone driver current limiting — already OQ-17, and misdirected.** IC7's
  101 kΩ input is modelled as a load on the VOLUME wiper; driven headphone
  output is explicitly OQ-17's. The proposal's premise concerns how third-party
  *reference recordings* may have been captured, which is a caveat on comparison
  material, not a property of the product's line output; modelling headphone
  current limiting on the main output would model the wrong path.

Net: OQ-08 and OQ-01 gain one recorded lead each, OQ-18 and the best-in-class
queue gain one arithmetic correction each, and `vcfControlSaturationHz` is
reclassified from derived to voiced without moving. No question closed.

**Batch 3 — tooling, performance, product features and QA.** One change shipped;
the rest were already done, outside the contract, or product decisions this pass
does not get to make on its own.

- **Half-band decimator tap compaction — implemented.** The proposal asked for
  hand-written NEON/AVX2 in `downsamplePair`, claiming ~15–20% of engine CPU and
  no change to floating-point output. Both halves are wrong: vectorising a MAC
  reduction splits it into per-lane partial sums, which reorders the additions
  and is *not* bit-identical, and the decimator is nowhere near that share of the
  engine. What the loop did carry was real waste — it walked all 95 taps and
  skipped the 46 analytically-zero ones with a per-tap branch, paying 95 loads
  and 95 compares to perform 49 stereo multiply-accumulates. Those taps are now
  compacted once, where the kernel is built, in their original ascending order,
  so the accumulation sequence is unchanged and the result is bit-identical by
  construction. Measured: the loop in isolation runs 10.4% faster; whole-engine
  render time moves within run-to-run noise, which is the honest answer to the
  15–20% claim.
- **Denormal flush on the high-pass state nodes — already handled.**
  `juce::ScopedNoDenormals` wraps the whole audio callback and sets FTZ/DAZ for
  every subnormal the DSP can produce. Per-node manual flushing would be
  redundant.
- **Thermal and rail-droop readout — already shipped.** The engine exposes
  `getDisplayTemperatureC()` and `getDisplayRailDroopVolts()`, the processor
  publishes both as atomics, and the display paints them on one telemetry line.
- **Hardware EDIT dot — already shipped in the form this UI supports.** The
  plug-in has no 2-digit patch LED to put a decimal point on; patch selection
  lives on the add-on host-navigator rail, where an `EDITED` lamp driven by
  `currentProgramIsEdited()` already serves the same purpose.
- **SysEx fuzz harness — low incremental value.** The parser reads a
  fixed-length binary format and already has targeted coverage for truncation,
  malformed content, null pointers and buffer bounds.
- **OQ-15 mixer sensitivity sweep tool — declined.** A tool that sweeps
  `subMixVolts` and reports where bank-wide spectral error minimises would take
  that minimum from the same undocumented MP3 chain the research contract
  refuses to retune against, and would dress a fit as preparation for a
  measurement. OQ-15's direction is already recorded with numbers; what is
  missing is the hardware capture, not a sweep.
- **MR-STFT distance in the comparison tool** is a reasonable idea, but nothing
  here shows a gap the existing dBc/NRMS/BGA/SGA gates miss.
- **MTS-ESP/Scala microtuning, voice pan spread and MPE pressure** are product
  decisions rather than model questions. The instrument has no aftertouch and no
  pan spread, so each would ship as a labelled product extension alongside the
  existing voice-count and velocity ones. Not taken unilaterally here.

**Batch 4 — circuit acoustics.** Nothing implemented: three of the eight are
already in the model, and the rest are unanchored or do not survive their
arithmetic.

- **Filter slew-rate limiting at low cutoff — already modelled, exactly.** The
  cascade integrates `dV/dt = ω · gScale · early · H · tanh(…)`, and `|tanh| ≤ 1`
  puts a hard ceiling of `ω · H` on every stage. Since `ω = Ig / (C H)`, that
  ceiling *is* `Ig / C` — precisely the bound the proposal derives. Solving the
  real nonlinear ODE rather than a linear filter gives this for free, and it is
  one of the things that formulation buys.
- **Sallen-Key reconstruction peak — already modelled, at the same number.**
  `Chorus::sallenKeyQ` computes `Q = 0.5·√(C_feedback / C_shunt)`; on the
  proposal's own 1.8 nF / 270 pF that is 1.291, the figure it quotes. The
  passband peak is a consequence of a Q the engine already carries.
- **Inter-voice rail coupling — already modelled and already measured.** Rail
  droop under polyphonic load is in the engine and reported to the display; the
  best-in-class pass measured it at 0.104 cents across the entire one-to-six
  voice load change and records it as modelled-and-inert.
- **HPF Position-0 boost driving the output stages — already emergent.** The
  +10.50 dB / 59.41 Hz Boost shelf and the summer's algebraic soft clip are both
  modelled, and the boost sits ahead of the common VCA and line drivers, so the
  interaction the proposal wants is already what the chain does.
- **Open-collector pulse rise/fall asymmetry — does not survive its own
  numbers.** A 200–400 ns rise is a bandwidth of roughly `0.35 / t_r` ≈ 1 MHz,
  two orders above the 12 kHz content the proposal says it tames; at the 192 kHz
  internal rate that edge is 0.06 of a sample, below anything the grid
  represents. The comparator part identity is also not established in tree.
- **Sub-oscillator diode knee — topology already carried, magnitude
  unanchored.** D5/D6 and the 27 kΩ legs are in the mixer model; a forward-knee
  softening of the divider's square is not, and the tree already reverted one
  0.3% sub-level asymmetry as mis-attributed to an edge-timing effect.
- **OTA differential-pair bulk-resistance compression — real, unmodelled,
  unanchored.** Emitter bulk resistance would soften the pair's transfer below
  the ideal `tanh`, which the cascade does not carry. Its size needs a device
  measurement; recorded here as a lead beside the Early-effect coefficient,
  where a previous revision using sixteen times the supportable value is the
  standing warning about guessing it.
- **Anti-phase BBD clock crossing — speculative.** Stray inter-trace coupling is
  unquantified, and the audible claim sits next to `enableChorusClockBleed`,
  already off pending OQ-03's calibrated capture.

**Classification sweep, same date.** The OQ-18 correction exposed a claim the
best-in-class queue had already flagged as owed: the 240 pF integrator was being
carried as an unqualified anchor while a reconstruction lineage reads 270 pF in
the same position. Three places now say otherwise — the research contract's
filter-core row (component values anchored *except* the integrator, voiced
pending OQ-18), the README fidelity ledger (no longer among the hardware-fixed
figures, and the knee's 64 kHz ceiling stated as voiced between 64.8 and
72.9 kHz), and the comparative assessment's knee entry. The three older
paragraphs that state the knee as "a pole near 64 kHz on this circuit's own
240 pF" are left in place per this document's dated-record convention, each now
carrying a pointer to the correction. No constant moved and the capacitance
question stays open.

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
- **C14/HPF established boundary:** placement, populated parts and control,
  direct C14/R39→YCOM routing, Tr3→INH, Flat R27, Boost R25, the Boost/Flat
  endpoints, 225.8/720.5 Hz cut anchors and 15.705/4.9209 ms deselected Cut
  decays are settled. The selected-Cut C14 load is now
  `R39 || 1 MΩ = 31,945.788964 Ω`, or 0.498203201 Hz / 319.457890 ms, and
  the nominal fixed-position cascade is MNA-qualified to 0.011137 dB/0.056092°
  or below. OQ-21 owns switch parasitics, deselected-capacitor charge memory and
  mode-change transients; it is not permission to discard C14 or refit the
  established endpoints.
- **Converter ownership and ordinal order:** the 23 used holds are 18 per-card
  DCO/VCF/ENV-VCA destinations plus shared SUB, VCA LEVEL, PWM, RESONANCE and
  NOISE. The service timing chart orders shared RES/VCA/SUB, DCO 1–6, PWM,
  interleaved VCF/VCA 1–6, then NOISE. The current compatibility profile places
  ordinal `n` at `n/23`; RESONANCE/common VCA/SUB/PWM and six VCF/six VoiceVca
  holds now evaluate that policy event fractionally and commit their latched
  payload on the next ordinary poll. Pitch/DCO and NOISE remain sample-grid for
  the explicit OQ-08 and OQ-07/OQ-15/OQ-16 evidence boundaries.
  Direct host-snapshot priming is allowed only until the first valid prepared
  audio interval; later silence and panic do not re-arm it, while hard reset or
  `prepare()` begins a new startup window. That exception is product policy,
  not hardware timing evidence.
  Hold constants and physical event offsets remain OQ-07 and OQ-08 respectively.
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
  host-grid reconstruction is an implemented numerical product mechanism: it acts
  after transfer loss and before the tap pole, leaves physical BBD/RNG state and
  noise unchanged, and clears its grid-specific slots at a rate change. Step 9
  additionally advances each output-side support network as one exact
  continuous six-state transition at every rate and does the same on the input
  at internal rates ≥176.4 kHz; lower input grids retain the reviewed TPT path.
  A guarded rate change resets the support coordinates under zero gain. They
  are physical voltages, not timestep-embedded carries, but preservation or
  reseeding is not yet qualified.
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
