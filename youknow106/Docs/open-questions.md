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

## What is still open — 2026-08-05

The implementation and evidence boundary were reconciled again after the
2026-08-04 fidelity pass and the 2026-08-05 physical circuit modeling pass
(incorporating thermal warmup $V_t(T)$, VCA CV feedthrough thump, power supply
rail droop inter-voice coupling, Thévenin passive mixer node loading, and
BBD dynamic charge loss), and again after the 2026-08-05 transcription of
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
| P0 | 01 | Absolute JUNO-106 chorus rate scale, BBD clock/delay endpoints, and whether the clock is period- or frequency-linear in its CV | Two-line topology, mode controls, the integrator-plus-comparator LFO and its straight triangle, the schematic-derived 1.6234799 mode-rate ratio, and the provisional JUNO-60 scale and sweep |
| P0 | 02 | Roland stored-byte/DAC/hold-network to IC5 GC1 voltage, offset and in-circuit endpoints | Shared uPC1252H2 placement, `b<<5` DAC code, C12/R36 input coupling and the IC's −5.9 mV/dB typical GC1-to-gain law |
| P0 | 03 | Calibrated chorus noise PSD, SNR, spurs and stereo correlation | No-compander topology and the need for a wet-line noise model |
| P2 | 04 | Loaded post-BBD support transfer, including MN3009 and emitter-follower output impedance | Component topology and provisional ideal-source poles |
| P0 | 05 | TA75558S IC6 and High-output clipping swing versus frequency and load | IC6 identity, linear resistor gains and ±15 V rails |
| Dependency | 06 | Physical `Vref_rms` for a declared High-output/load condition | Final -18 dBFS RMS mapping, floating output and no-limiter policy |
| P1 | 07 | Acquisition, droop, loading and time constant of every converter hold | Hold ownership, 4.2 ms pass, VCF 522 µs and voice-VCA 687 µs anchors |
| P1 | 08 | Exact intra-pass timestamps/branches and the physical state forced by a changed-pitch write | Ordinal 23-write queue and normalized compatibility scheduler |
| P1 | 09 | Resonance DAC/control voltage to loop gain, compensation and oscillation correction | BA662/IR3109 topology, 4.8 Vpp service trim, shared hold and exact B-2 byte-to-DAC mapping |
| P3 | 10 | Post-calibration six-card and multi-unit residual distributions and thermal drift | Zero-spread nominal policy and optional deterministic Unit Character |
| P1 | 11 | Pulse-Off DC, bleed, loading and switching transient at the voice mixer | About -0.8 V pins the comparator; the final output capacitors are unrelated |
| P2 | 12 | Envelope wall-clock timing/jitter, analogue/audible thresholds and other firmware revisions | Exact hash-scoped B-2 recurrence and physical `E>>2` DAC truncation |
| P2 | 13 | LFO/delay wall-clock timing, analogue smoothing/output scale and revision differences | Exact hash-scoped B-2 rate, delay and fade algorithms |
| P2 | 14 | Portamento pot/ADC transfer, hysteresis, cadence and revision differences | Exact hash-scoped B-2 coefficient and 8.8-state law |
| P0 | 15 | Loaded oscillator/sub/noise mixer levels and their actual filter-drive budget | Node-specific 12 Vpp/4 Vpp anchors and the 68 kΩ/560 Ω core attenuator |
| P2 | 16 | Main-noise spectrum/distribution and physical filter-startup excitation | Shared generator topology and TP8 4.0 Vpp adjustment |
| P3 / dependency | 17 | Real VOLUME gang tracking plus selector, jack, headphone and external-load transfer | Nominal-linear `10KB×2` law and fixed 29.313 kΩ internal wiper load |
| P2 | 18 | Hardware cutoff-converter knee and upper saturation curve | Exponential audio-range law and transparent 50 kHz product cap |
| P1 | 19 | Voice-module BA662 control-to-gain curve, knee and possible deadband | BA662 identity, 6 Vpp service trim, ENV/GATE ownership and replaceable voiced compatibility profile |
| P2 | 20 | TR11/TR12 wet-mute switching envelope, leakage and click | Device identity, wet-only mute location and continuously running BBDs |
| P2 | 21 | Coupled C14/HPF transfer, switch-state memory and mode-change transient | Parts, placement, asymptotic C14 loads and established HPF endpoints |

The most consequential audible blockers are OQ-01, OQ-02, OQ-03, OQ-05,
OQ-09, OQ-15 and OQ-19. A well-instrumented original unit can also collect OQ-03,
OQ-05, OQ-17 and OQ-20 output data in the same session; keeping those captures
on one calibration/load chain would remove several cross-normalisation errors.

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
scope export, board photograph or other raw capture. Primary-source and
direct-teardown reconciliation therefore closes no task, but it narrows four
boundaries:

- NEC pp. 256–260 directly specify the µPC1252H2 GC1 voltage-to-gain constant
  as −5.9 mV/dB typical (5.8–6.1 mV/dB magnitude) over −30 to +30 dB. OQ-02
  now asks only for Roland's byte/DAC/hold network to GC1 voltage, offset and
  installed-circuit endpoints.
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
mode-rate ratio, and narrowed what remains. IC1 (µPC062) is an integrator plus
a Schmitt comparator: C3 sits across IC1b pins 6 and 7, IC1a returns its own
output to pin 3 through R6 47 kΩ against R15 1 kΩ to ground, and R7 33 kΩ
closes the loop from the integrator's output to IC1a pin 2. IC2a then inverts
once through R10/R9 33 kΩ, so TP4 carries the triangle and TP3 exactly its
negative. Two results follow with no measurement:

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

What is still open is the **absolute scale and the sweep endpoints**. The
schematic does not print C3, and `f = 1/(4·β·R_eff·C3)` needs it. The
implementation therefore keeps the measured **JUNO-60** sweep of 1.66–5.35 ms,
and takes the JUNO-60 rate pair's geometric mean as the scale, re-split by this
instrument's own ratio: **0.5222045 Hz for I and 0.8477886 Hz for II**. Both
round to the owner's manual's published about-0.5 and about-0.8.

The formerly claimed 1.54–5.15 ms JUNO-106 sweep had no valid measurement and
must not be reintroduced. The sibling's own rate ratio of 1.682 is likewise
superseded by this instrument's 1.6234799 and must not be reintroduced.

Note that `β = R15/(R15 + R6) = 1/48` is now known, so the absolute rates are
one number away: reading C3, **or** measuring one period at TP4, **or**
measuring the triangle's peak-to-peak amplitude at TP4 (which is `2·β·V_sat`
and therefore also yields C3), each closes the scale on its own.

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

  This is the one output that resolves the sweep law, and the model has now
  taken a position on it that a measurement still has to confirm. Service Notes
  p. 15 shows each MN3101 driven by a transistor voltage-to-current converter
  (Tr22 with R133 2.2 kΩ, R134 22 kΩ and R135 1.8 kΩ, against C53 150 pF),
  which is the shape of a *current-controlled* oscillator — **frequency**
  linear in the control voltage rather than period. `Chorus::process`
  accordingly bends the delay hyperbolically under
  `enableChorusHyperbolicSweep`. A period-linear, a frequency-linear and an
  exponential clock differ only in the trajectory between the endpoints, so no
  endpoint measurement can tell them apart; only a time series can. Confirming
  the MN3101's bias transfer from its datasheet would also settle it.

  **Endpoint defect fixed in code; the reason this stays P0 is the law
  itself.** An earlier implementation scaled the hyperbolic delay about the
  *centre*, which moved the endpoints: with the measured centre 3.505 ms and
  sweep 1.845 ms, at Unit Character 1.0 the rendered range became about
  2.30–7.40 ms instead of the measured 1.66–5.35 ms — a span 38% wider than
  the only measured figures this chorus has, and the deviation scaled with
  Unit Character even though whether the oscillator is current-controlled is
  a topology fact, not a component tolerance. `Chorus::process` now instead
  sweeps the *clock* linearly between `128/5.35 ms` and `128/1.66 ms` (derived
  from the same centre/sweep numbers, no new constant), which keeps both
  endpoints exact at any amount of Unit Character and changes only the path
  between them. This closes the endpoint-overshoot defect but not the
  question: the frequency-linear assumption itself is still unconfirmed
  against hardware, so resolving OQ-01 still requires the clock time-series
  capture described above.
- The peak-to-peak amplitude of the modulation triangle at TP4, and the
  saturated output swing of IC1a, in the same capture. With `β = 1/48` from
  R15/R6 those give C3, and C3 gives both absolute rates.
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

## OQ-02 — Stored VCA LEVEL byte-to-GC1/in-circuit gain law

**Priority:** P0

### Task definition

Recover the complete stored VCA LEVEL mapping for the common uPC1252H2 (IC5)
on the jack board. Its location after the six-voice sum and shared high-pass,
before the chorus, and its one-shared-hold ownership are settled. The IC's
device law is now also settled: NEC pp. 256–260 specify GC1 as the gain-control
voltage input and a −5.9 mV/dB typical control constant, with 5.8–6.1 mV/dB
magnitude over −30 to +30 dB under the stated ±12 V, 2 mA, 33 kΩ and 1 kHz test
conditions. Thus the nominal device boundary is
`gain_dB = GC1_volts / -0.0059`. The application circuit and input
specification also settle the populated C12 10 µF/R36 33 kΩ input coupling at
0.482288 Hz. None of this maps Roland's held control to GC1 voltage, offset or
installed-circuit endpoints.
Service Notes pp. 5, 8 and 13 show IC23 channel 6 feeding that hold. The
provisional firmware trace forms an aligned internal word `b<<7`; the DAC
routine drops its bottom two bits, producing physical 12-bit code `b<<5`. The
unknown is the code-to-hold-to-jack-board-network-to-GC1 law. The current model
is a provisional fit to reported points near −15 dB at −5, −12.5 dB at 0 and
+5 dB at +5 in the project's recentered coordinate. That notation is not the
original panel's byte scale and must be mapped explicitly. This task directly
affects preset loudness and chorus drive. The byte-exact 128-tone factory bank
now supplies the real stored-byte corpus for regression, but it is deliberately
not rebalanced and cannot substitute for the original unit's byte-to-gain
transfer. Audible levels may therefore need revisiting when this task is
resolved. The shipping-engine [factory gain audit](audio/factory-presets/README.md)
is the current product baseline: every tone remains finite, its stress-score
median is -26.65 dBFS gated RMS and 9 tones cross 0 dBFS. Those results expose
regressions and headroom pressure but do **not** close this question or establish
the hardware's relative levels.

### Needed output (for LLM)

- A 128-row CSV containing commanded byte, physical 12-bit DAC code,
  corresponding physical/nominal panel position, held control-node voltage,
  uPC1252H2 GC1 voltage, simultaneous settled VCA input/output amplitude,
  `Vout/Vin`, and relative dB.
- Exact input/output probe points, tone frequency and level, VOLUME setting,
  load, supply rails, calibration state and gain-reference setting.
- The byte-to-panel mapping and all analogue endpoint/saturation behaviour;
  verify the `b<<7` aligned-word and `b<<5` physical-DAC-code lead independently
  rather than returning it as a new gain law.
- A fitted byte-to-GC1 equation or lookup table with residuals and uncertainty,
  followed by the NEC device law and a measured residual check against the
  installed IC; compare the complete result explicitly with the current
  three-point fit.
- Separate regression fixtures for the anchored GC1 voltage-to-dB relation and
  the measured byte-to-GC1 endpoints/intermediate values/monotonicity. If a
  full sweep cannot be found or measured, return a protocol and identify which
  conclusions the three existing points cannot support.

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
specify the needed output impedance. Service Notes p. 15 identifies the two
branches as IC8 with R118/R119, R117 and C45, and IC10 with R111/R112, R110 and
C48; alternating active taps make the loaded circuit time-varying. The later
7.234/10.621 Hz C28/C25 coupling calculations likewise omit the driving
emitter followers' source impedance. TR11/TR12 on-resistance, leakage and
switching belong to OQ-20. Use one declared route: measured MN3009 and
emitter-follower output impedance followed by full modified-nodal analysis, or
a calibrated de-embedded wet-only sweep.

### Needed output (for LLM)

- The selected route and why its evidence is sufficient.
- For the circuit route: measured complex MN3009 and emitter-follower output
  impedance versus frequency, clock, bias and signal conditions, a
  component/designator table, complete small-signal netlist, transfer equation
  and uncertainty.
- For the sweep route: raw input/output data from 100 Hz to beyond the candidate
  −3 dB point where feasible; exact probes, level, loading and fixture response.
  Hold each BBD clock at a documented fixed rate or use a validated
  linear-periodically-time-varying method, because a normally sweeping chorus
  is not LTI. Include both output taps and stereo lines or state the narrower
  scope. Do not force a through-BBD sweep beyond the line's usable
  clock-dependent band where the paired-node ratio becomes noise-dominated;
  prefer local output-impedance injection there.
- Explicit de-embedding of the pre/post support filters, BBD zero-order hold,
  charge-transfer response, fixture and load. Removing only the dry path is not
  sufficient.
- Fitted pole(s), magnitude/phase residuals and a direct comparison with the
  current ideal-source 23.46 kHz tap pole and 7.234/10.621 Hz output-coupling
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
existing component-derived anchors. The common stored VCA LEVEL hold is a
different destination whose constant remains unmeasured; extending 522 µs to
it or to every other destination remains provisional.

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
   modelled.

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


Added 2026-08-06: the passive mixer node now loads unconditionally and counts
the permanently-wired SUB and NOISE legs, normalised against the three
usually-connected legs so a plain saw patch keeps its established level. What
remains open here is whether the SAW and PULSE panel switches **open their
100 kΩ resistors or merely mute their sources**. The two readings differ by a
constant gain that `filterInputAttenuation` and the output reference would
absorb, so the choice does not change the shape — every connected leg loads
every other — but it does set the absolute level and which configuration is the
right normalising reference. A continuity check on the switched legs, or a
level measurement at the IR3109 input with one waveform switched off, settles
it.

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
  OQ-11 and noise spectrum is OQ-16. The common stored-VCA transfer is OQ-02;
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
bounded uniform white xorshift noise for the shared source and an unexplained
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

- A circuit trace for the hardware noise generator and any shaping/filtering,
  plus raw TP8 captures with calibration setting, bandwidth and load.
- PSD, autocorrelation, amplitude distribution, crest factor, bandwidth and
  discrete spurs for the shared generator; state whether flat bounded white
  noise is adequate over the audible band.
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
modelled. This remains distinct from stored VCA LEVEL (OQ-02), IC6 clipping
(OQ-05) and dBFS policy (OQ-06).

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
control curve. Even the broadly quasi-linear shape remains unverified. The
current central law uses a knee at 0.12, a 260 dB-per-unit low-level slope and a
hard deadband at 0.005 as a voiced compatibility curve attributed to an
unavailable, only coarsely described sweep. No qualifying raw original-module
sweep establishes those constants, and a measurement noise floor can
masquerade as a hard deadband. A circuit reconstruction reports a roughly
150 mV no-current region, but it is useful only as a reason to sample densely
near onset—not as a stock-card constant. This is not the common stored VCA
LEVEL in OQ-02, and card-to-card residual spread belongs to OQ-10.

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
- A fitted law/table with residuals against the current voiced
  0.12/260/0.005 compatibility profile and deterministic boundary/interior
  fixtures. Keep this analogue transfer replaceable without changing OQ-12
  envelope states or patch bytes.
- Report the measured 6 Vpp service endpoint separately, without inventing a
  tolerance or using it to infer the knee/deadband law.

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

## Settled guardrails — do not reopen without contradictory primary evidence

- **Chorus modes:** the JUNO-106 has Off, I and II. Its owner's manual says I
  and II cannot be used simultaneously, and the board has one enable line plus
  one binary mode line. Obsolete both-buttons session states canonicalise to II.
- **Chorus balance:** dry enters IC6 through 39 kΩ, wet through 47 kΩ, with
  100 kΩ feedback. Thus dry gain is `100/39`, wet gain is `100/47`, and
  wet/dry is `39/47`, or −1.62 dB.
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
  switch's T-network — JFET Tr1 *shorts* R3 2.2 MΩ, giving effective timing
  resistances of 6.4352941 MΩ (mode I, slower) and 3.9638889 MΩ (mode II).
  Earlier notes describing R3 as a series resistor that mode I bypasses were
  wrong about the mechanism while right about the numbers. Do not reintroduce
  the JUNO-60's own 1.682 ratio. Only the absolute scale remains OQ-01.
- **Mains ripple:** not modelled, by derivation rather than for want of
  evidence. Service Notes p. 16 gives 3300 µF per rail behind a 0.25 A
  secondary and M5230L regulators, so what reaches a card is on the order of
  50 ppm of 15 V — about 0.03 cents of cutoff shift. Rail *droop* is modelled
  and is a different, much larger, DC mechanism. Neither may be routed to DCO
  pitch, which is an integer division of a crystal-derived clock.
- **Chorus support-chain boundaries:** the populated pre/post-BBD topology,
  wet-input 15.9 Hz coupling, nominal component-only 7.23/10.62 Hz wet-output
  coupling, datasheet-fitted nonlinearity, and split zero-order-hold/residual
  charge-transfer loss are settled or derived at their stated ideal-source
  boundaries. OQ-04 owns MN3009 and emitter-follower source loading, including
  the loaded tap-summing transfer; OQ-20 owns TR11/TR12 on-resistance, leakage
  and switching transients.
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
