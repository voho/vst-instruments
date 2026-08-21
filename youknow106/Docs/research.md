# YouKnow106 — research notes and open questions

One document holds the research side of this instrument: the evidence rules
the model is built under, the current state of every open question, the
guardrails that are settled, the decision log, and the sources. It replaced
the former `circuit-modelling-research.md`, `open-questions.md`,
`best-in-class-plan.md`, `comparative-assessment.md` and
`external-sound-validation.md` on 2026-08-21; their complete histories, with
every dated pass and measurement, remain in git. What is simulated and what
is not is summarised for readers in the [project README](../README.md).

## Evidence rules

Every constant and mechanism in the engine carries one of five classes:

- **anchored** — supported by JUNO-106 service documentation, a component
  datasheet, provenance-safe firmware analysis, or a calibrated measurement.
- **ROM-resolved** — exact for one explicitly hash-identified firmware image
  (voice/main B-2, assigner A-5), without implying other revisions match.
- **derived** — calculated from anchored values by a stated equation.
- **product policy** — an explicit plug-in decision, not a hardware claim.
- **voiced** — provisional: chosen inside a range the sources bound but do
  not fix. Changing a voiced value never needs an exemption; *promoting* one
  to a stronger class does.

The working rules, unchanged:

- JUNO-60 evidence is comparison-only, always labelled, and can never
  resolve a JUNO-106 question.
- Cite exact pages, designators, test points, datasheet conditions or
  firmware locations; separate direct evidence from calculation and
  inference; preserve raw evidence with units, bandwidth and loading.
- Report contradictions instead of silently reconciling them. **Not found**
  and **measurement required** are valid results; an unsupported estimate
  is not.
- A measurement from one instrument establishes that unit under those
  conditions, not a population value.
- Never reproduce or embed ROM images or proprietary table dumps; report
  observable behaviour, equations and boundary semantics with
  provenance-safe test vectors.
- Resolving a question updates this file, the source comments/constants and
  deterministic tests in the same change.
- Audible decisions between formulations that evidence cannot separate go
  to an A–Z listening test under the repository-level rules in `CLAUDE.md`;
  a direction chosen by ear is recorded here as chosen by ear.

## Open questions

Twenty-one standing questions. Each entry: current state, what closes it.
Priorities (P0 loudest) describe likely audible impact, not permission to
weaken the evidence standard.

### OQ-01 — Absolute JUNO-106 chorus timing (P0)
Topology, waveform and scale are derived from the 106's own schematic:
µPC062 integrator + Schmitt comparator gives a straight symmetric triangle
at 0.5532934 / 0.8982608 Hz (ratio 1.6234799 from the mode switch's
T-network). The shipped 1.4–6.4 ms sweep endpoints are a third-party scope
measurement of a designator-faithful build compared against a real 106. The
sweep *law* is contested: the V-to-I converter implies frequency-linear,
but KR-106's ~50-point click-timing series fits delay-linear at 16 µs RMS —
delay-linear ships, the hyperbolic path waits behind
`enableChorusHyperbolicSweep`. Clone-kit photos corroborate the derived
triangle amplitude bracket; the Service Notes print no TP3/TP4 figure.
**Closes it:** one raw multi-cycle TP3+TP4 capture in both modes on a
calibrated 106 with a simultaneous instantaneous-clock series.

### OQ-02 — Installed common-VCA tolerance (P2)
The nominal law is fully derived (`d = b<<5`, the p. 15 network, NEC's
−5.9 mV/dB → `gain_dB = −16.3196647 + 0.165581014·b`, τ = 9.08249 ms).
KR-106's identified-unit endpoints sit within 0.8 dB of the derived law.
Open: installed resistor/C7/rail/DAC/IC spread. **Closes it:** a 128-row
installed sweep plus a C7 step-response capture on a calibrated unit.

### OQ-03 — Chorus noise and SNR under calibrated conditions (P0)
Mode I's per-line floor is the MN3009's own 0.2 mVrms max A-weighted row,
referred through the model's measured 0.4026 A-weighted transfer. Mode II
ships the reported ~3.95 dB complete-output delta as a relative factor
(rate-proportional alternative behind `useChorusRateNoiseHypothesis`).
Open: the datasheet's two noise rows disagree by 10.5 dB, so where a real
card sits in that bracket is unmeasured, as are absolute PSD, weighting,
stereo correlation, spurs and the delta's physical cause. **Closes it:**
calibrated same-chain tone+noise captures for Off/I/II on both channels
with declared bandwidth/weighting/detector.

### OQ-04 — Loaded post-BBD support-chain transfer (P2)
Component topology is 106-anchored at designator level on both sides of the
BBD, the two-phase output is confirmed a full-period hold, and the
charge-transfer coefficient is anchored to the datasheet's 40 kHz/12 kHz
row. Open: the loaded tap-summing pole (ideal-source 23.46 kHz vs loaded
candidates 11.9/15.1/22.2 kHz) and the digitised Gi–fi curves'
self-contradictory tracked-vs-broadband reading — one tracked ~40 kHz
installed sweep decides both. **Closes it:** measured MN3009+follower
output impedance into full MNA, or a calibrated de-embedded wet-only sweep
at several held clock rates.

### OQ-05 — Loaded TA75558S IC6 and High-output clipping swing (P0)
Device identity, resistor gains and ±15 V rails are settled; the Toshiba
datasheet column is on file (V_OM ±12 min/±14 typ at 10 kΩ — the row that
applies, since the wiper network computes to 8.96–11.5 kΩ). The modelled
13.5 V rail bound sits between guaranteed minimum and typical — population
figures, not the installed measurement. **Closes it:** simultaneous
IC6-in/IC6-out/High-jack captures under stepped low-distortion drive with a
declared clipping criterion.

### OQ-06 — Absolute output-reference calibration (dependent)
The product convention is settled and not to be reopened: −18 dBFS RMS at
the final boundary, floating samples beyond ±1, no limiter. Roland's
L −30 / M −15 / H 0 dBm selector spec fixes the intended steps but not the
reference impedance. The standing candidate — Roland's era convention
"0 dBm = 0.775 V RMS", printed on the RS-505's identical-style selector
spec — is recorded and **not adopted** (a user decision, pending the OQ-05
and OQ-17 measurements it would be checked against).

### OQ-07 — Converter hold topology and time constants (P1)
Ownership and inventory are closed: 23 used 0.01 µF holds over a 4.2 ms
pass, per-destination smoothing designator-complete (PWM 4.7 + 2.632 ms,
SUB 10 ms, common VCA 9.08249 ms; DCO/RES/NOISE keep labelled 522 µs
compatibility defaults). With HD14051B on-resistance rows, in-window
acquisition computes to 0.8–10.5 µs and inter-pass droop to ≈0.05 cents —
so the physical hold is a per-pass staircase into the smoothing networks,
and the engine's exponential-toward-target realization is equivalent at
nominal precision. Charge injection computes to 0.25–0.38 cents,
trim-nulled and unimplemented. **Closes it:** installed step-response
captures at the actual hold nodes with a per-destination table.

### OQ-08 — Exact intra-pass timing and DCO pitch-write restart (P1)
The 23-write ordinal order is settled and shipped on a fractional
scheduler; the normalized `ordinal/23` offsets are compatibility policy,
not timestamps. The p. 8 chart's drawn geometry has been pixel-measured
(three slot widths on a 10:7:5 drafting grid, NOISE-first origin, up to
+371 µs deviation from `ordinal/23`) and ships as the selectable
`MeasuredChartGeometry` profile with drawn-artwork provenance; the default
is unchanged. The restart question is firmware-determinable: in the
8253/82C54 family a count-only write loads at the end of the current
period, while a control-word write forces a restart — so the modelled
phase-zero restart corresponds to the control-word branch only.
**Closes it:** reading the pitch-write path in the hash-identified B-2
image (no image is in the repository by policy; none has been supplied),
or a logic capture across a changed-pitch write.

### OQ-09 — Resonance byte-to-loop-gain law (P1)
Topology and mechanism are settled — including, from the p. 9 module
drawing, Roland-printed input-side compensation (the drawing prints no
component values, so the 0.2296 coefficient stays voiced). The traced RES
CV path (200–348 µA at +10 V, converging with an independent measurement)
supports a linear-above-threshold shape prior, shipped as
`CircuitDerivedResonanceProfile` behind `useCircuitDerivedResonanceShape`
(default off) — **an A–Z listening test is delivered and the direction is
pending the user's ear**. The shipping curve remains voiced; the 4.8 Vpp
self-oscillation endpoint is anchored and the 248 Hz service point is
predicted within 1 cent. **Closes it:** a 128-value de-embedded loop-gain
sweep on all six cards.

### OQ-10 — Post-calibration voice dispersion and thermal wander (P3)
The calibrated nominal model is settled policy: zero inter-voice spread,
zero drift; all seeded variation lives in Unit Character and is voiced
sound design. The VCF trim residual is now bounded by Roland's own printed
±10-cent acceptance at the two check points (anchored acceptance bounds).
The separate `aging` extension — since 2026-08-21 the Aging host
parameter — carries one documented four-year recalibration lead (flatward
per-card drift up to a quarter tone, +3.5 dB noise-trim drift), voiced,
single-unit lineage. **Closes it:** repeated post-calibration measurements
of all six cards through warm-up, on multiple instruments.

### OQ-11 — Pulse-off pinned-leg mixer behaviour (P1)
Settled: about −0.8 V holds the comparator output high, and the model
represents that state with a provisional hard-zero audio gate. Untraced:
what the coupling network does — DC shift, residual bleed, loading, the
switching transient. **Closes it:** raw mixer/filter-input captures in
both states and both switching directions.

### OQ-12 — Envelope physical timing and firmware-revision scope (P2)
The digital law is ROM-resolved for B-2. The printed spec endpoints
reconcile with the model under stated threshold conventions (the
reconcilable endpoints imply a 3.84–4.57 ms pass, all within ±9% of the
anchored 4.2 ms; regression fixtures pin the reconciliation). Exactly one
released-final OS pair (A-5/B-2) is known; no other image or changelog has
surfaced. **Closes it:** raw envelope-DAC captures with measured pass
period/jitter, plus a second hash-identified image.

### OQ-13 — LFO/delay physical timing and analogue transfer (P2)
The digital law is ROM-resolved for B-2, and the printed 30 Hz top inverts
to the same pass period (−0.8%). One standing contradiction: the printed
0.1 Hz floor is unreconcilable with rate byte 0 at any pass period. The
slave ADC's per-channel conversion time is printed; its software sampling
cadence is not. **Closes it:** raw multi-cycle rate/delay captures with
measured pass period and jitter.

### OQ-14 — Portamento pot/ADC transfer and firmware-revision scope (P2)
The digital law is ROM-resolved for B-2, and the physical transfer is now
designator-complete from p. 16: 50KB linear pot loaded by R16 47 kΩ
(≈0.395 of full scale at midpoint), the off switch pinning the ADC at the
ROM's raw-0 immediate code. The loaded transfer ships at the knob boundary
with an exact inverse. Open: tolerance, hysteresis, sampling cadence,
revision scope. **Closes it:** measured pot/ADC transfer on hardware plus
raw glide traces.

### OQ-15 — Oscillator-mixer levels and filter-drive calibration (P0)
Node anchors are settled (saw/pulse ≈12 Vpp, noise 4.0 Vpp at TP8, the
68 kΩ/560 Ω core attenuator), and the mixer topology is
designator-complete: sources mute, legs never switch; the sub injects
through 60 kΩ as a diode-gated half-cycle current; the WAVE output's
source impedance is chip-internal and unprinted — measurement-only, and
all per-leg divider arithmetic depends on it. The A64 Snare Drum
contradiction stands recorded, not acted on: the isolated noise-vs-sub leg
ratio measures 13.3 dB with no circuit mechanism found for more than
~3 dB; a candidate re-voicing was reverted the same day when the reference
recording proved processed. `subMixVolts`, the noise coordinate and the
0.40 filter-input scale remain voiced. **Closes it:** a calibrated
designator-level source-to-VCF-input level budget across a dense byte
sweep.

### OQ-16 — Main noise spectrum and self-oscillation startup (P2)
Level is settled: the 4.0 Vpp TP8 anchor is applied *after* accounting for
the C41/R79 shaping it is measured behind. The shape class is settled from
designators (33.9 Hz high-pass, 4822.877 Hz pole), independently
corroborated by a restorer's description. Open: amplitude distribution
(bounded-uniform, voiced — a Gaussian correction would make the model
quieter, not louder), absolute PSD, spurs, and the per-card startup
excitation. **Closes it:** a raw calibrated TP8 capture with PSD,
distribution and crest, plus six-card no-input startup captures.

### OQ-17 — Main VOLUME tracking and output-selector transfer (P3)
The nominal law is settled: VR1 `10KB×2` maps to Panasonic's nominal-linear
`1B` group; the pre-wiper transfer and the fixed internal 29.313 kΩ wiper
load are derived, and the ladder's ideal taps land within 1.2 dB of the
published −15/−30 dBm steps. Panasonic publishes no numeric gang-tracking
tolerance, so none is invented. **Closes it:** a calibrated
pre-volume-to-jack sweep for High/Mid/Low in one- and two-plug
configurations, plus a PHONES table and real tracking data.

### OQ-18 — Upper cutoff-converter saturation law (P2)
The exponential audio-range law is confirmed by measurement (3.46–3.49
oct/1000 codes vs the model's 3.500; 248 Hz anchor within 3 cents), the
50 kHz cap is declared product policy, and the R-2R major-carry INL ships
as a persistent converter-write offset. The knee is the live question:
`vcfControlSaturationHz` is voiced, bracketed 64.8–72.9 kHz, and the
240 pF-vs-270 pF integrator contradiction is asymmetric (neither reading
is a JUNO-106 hardware measurement of the hybrid's own part; the printed
values are the JUNO-60 sibling's). KR-106's 52.2 kHz measured table vs
the shipped 64 kHz is the narrowed disagreement. **Closes it:** a dense
high-code sweep (code → CV → frequency) on multiple cards.

### OQ-19 — Voice BA662 gain, knee and deadband (P1)
Topology is settled: current-controlled BA662 with no intentional
volts-per-decade converter drawn, supporting the shipped quasi-linear
compatibility law; the audio chain and control chain are
designator-complete, the 687 µs hold constant is corroborated by the
R106/R105 junction read, and the thump mechanism is anchored as
existence-plus-null-procedure with no residual added until measured.
**Closes it:** a dense simultaneous CV/control-current/in/out sweep on a
calibrated card, plus DC-coupled thump captures around the VR30 null.

### OQ-20 — Chorus wet-mute switching transient and leakage (P2)
Settled: Off mutes wet only, and the wet-return devices are TR11/TR12
(2SK30A). Datasheet brackets are on file: static wet-level error at most
−0.184 dB worst-case (below audibility, stays unmodelled), gate feedthrough
13–39 pC, muted bleed −67 dB at 1 kHz rising to −41 dB at 20 kHz. The
shipped 5 ms wet glide is declared anti-click policy. **Closes it:**
simultaneous control/wet-return/output captures for both switching
directions at several levels and BBD phases.

### OQ-21 — Coupled C14 and switched high-pass transfer (P2)
Parts, placement and control are settled; the selected-Cut load includes
the mux-side 1 MΩ bleed (0.498203 Hz), and the nominal fixed-position
network is qualified against independent long-double MNA to
0.011 dB/0.056°. TC4052B on-resistance moves the corner by parts-per-
million; the sheet publishes no charge-injection spec. Open: the complete
switched state-space, mode-change transients and charge memory.
**Closes it:** mode-change captures (or a validated transient simulation)
for all directed switch pairs, plus the YCOM node's parasitic capacitance.

## Settled guardrails

Not to be reopened without contradictory primary evidence:

- Chorus modes are Off/I/II only, mutually exclusive; obsolete both-buttons
  states canonicalise to II. Off mutes the wet return only — oscillator and
  BBDs keep running. Normal output is dry plus wet.
- Chorus balance: dry 100/47, wet 100/39 — wet is the hotter leg (+1.62 dB).
- The chorus modulator is a straight symmetric triangle; line 2 is its
  exact negative. Mode-rate ratio 1.6234799, this instrument's own; the
  JUNO-60's 1.682 must not return.
- The two-phase BBD output is one sample per clock period (full-period
  composite hold), never a 2·fcp output stage.
- Voice summer: 0.1 per voice (33 kΩ into 3.3 kΩ feedback); then C14 and
  the shared HPF, C12/R36 into the common VCA, chorus, IC6, VOLUME.
- C14/HPF boundary: placement, parts, control routing, Boost/Flat
  endpoints, 225.8/720.5 Hz cut anchors, deselected decays and the
  MNA-qualified selected-Cut load are settled — OQ-21 is not permission to
  discard C14 or refit endpoints.
- Converter ownership and ordinal order (23 writes, RES/VCA/SUB, DCO 1–6,
  PWM, VCF/VCA 1–6, NOISE) are settled; the `ordinal/23` placement is the
  declared compatibility profile.
- Startup host-snapshot priming ends at the first valid prepared audio
  interval; later silence and panic do not re-arm it.
- Pulse-off: about −0.8 V pins the comparator output high.
- POLY 1 + POLY 2 is Solo Unison: six equal-frequency free-running voices,
  unnormalised, no programmed detune. The POLY switches are momentary
  firmware inputs; both-off is not stable.
- Mains ripple is not modelled by derivation (≈0.03 cents of cutoff), and
  neither ripple nor rail droop may ever be routed to DCO pitch — pitch is
  integer division of a crystal-derived clock.
- Chorus support-chain boundaries and the split ZOH/residual
  charge-transfer loss are settled at their datasheet anchor; the removed
  affine clock multiplier double-counted clock scaling.
- Main VOLUME is the nominal-linear `10KB×2` law with the fixed internal
  loads — not a generic squared taper.
- The noise calibration point (4.0 Vpp at TP8 via VR32) exists; OQ-16 asks
  about spectrum and context.
- Published selector levels L −30 / M −15 / H 0 dBm stand; OQ-17 owns their
  reference and loading.
- The output-reference convention (−18 dBFS RMS, floating samples, no
  limiter) is settled; OQ-06 owns only the physical reference value.

## Decisions

### Pending

- **Resonance shape by ear (OQ-09).** A/B delivered 2026-08-20,
  re-verified byte-identical on the current engine 2026-08-21. A = shipping
  voiced curve, B = `CircuitDerivedResonanceProfile`. Selecting B makes the
  circuit-derived shape the default; either way the verdict is recorded
  here as chosen by ear.
- **Vref = 0.775 V (OQ-06).** Roland's era convention, recorded as the
  standing candidate; adoption is a product decision the user has not
  taken.
- **B-2 pitch-write semantics (OQ-08).** Blocked: needs the hash-identified
  firmware image, which the repository does not carry by policy and no
  session has been supplied.
- **Chart-geometry follow-on (OQ-08).** If `MeasuredChartGeometry` and the
  normalized profile prove indistinguishable by ear, OQ-08's audible-impact
  priority drops; no verdict yet.
- Several load-bearing external reads remain blocked or unread (vendor
  pages behind an egress-blocked host class; one paywalled shootout; TAL's
  second-hand resonance-rolloff admission unread at source).

### Log

- 2026-08-07 — Shipped the solver-cost pass (no calibrated constant moved)
  and derived the chorus modulator scale from the 106's own circuit.
- 2026-08-08 — Adversarial review struck two steps: the terminal-count
  pitch-write guess (OQ-08 keeps the declared restart policy) and the
  four-leg HPF state (routed to OQ-21).
- 2026-08-08/09 — Gated PWM with LFO DELAY (internal consistency); replaced
  the fitted self-oscillation trim with a derived harmonic-balance solve
  (248 Hz becomes a prediction, not a fit); inserted C59; set BBD hiss from
  the MN3009 noise row; made warm-up a wall-clock accumulator; routed
  velocity through the envelope path into the VCF.
- 2026-08-09 — Adopted the reported 3.95 dB II−I chorus-noise delta as the
  shipped relative calibration; rejected the bounded-work quasi-Newton VCF
  candidate on reachable-control parity; admitted the fixed Merson VCF
  realization, causal BBD edge sampling and exact support integration on
  all standard HQ paths; declared HQ-off VCF/BBD rates numerically
  rejected rather than silently passing; user-authorized a new canonical
  audio corpus.
- 2026-08-10 — Corrected the selected-Cut C14 load (Step 15); capped the
  noise design corner as numerical policy (Step 16); confined startup
  priming to before the first prepared audio interval (Step 17); declined
  to promote KR-106's incomplete-provenance calibration material.
- 2026-08-17..19 — Anchored the noise level through its own shaping;
  investigated and rejected a quality-dependent noise-level defect;
  re-voiced `subMixVolts` against a factory-bank comparison and reverted it
  the same day when the reference chain proved processed (recorded under
  OQ-15).
- 2026-08-18 — External sanity validation of all 128 factory presets
  against public hardware recordings (see below).
- 2026-08-20 — Evidence pass: p. 8 chart geometry measured, p. 9 module
  drawing read (topology printed, no values), datasheet closes (TA75558,
  8253-family, HD14051B, 2SK30A, TC4052B), provenance corrections (the
  "AS3109 teardown" figures are forum-sourced; printed filter values are
  the JUNO-60 sibling's), spec-page endpoint reconciliation.
- 2026-08-20 — Implementation on request: `MeasuredChartGeometry` profile
  (default unchanged), spec-endpoint fixtures, the loaded portamento pot
  law, printed ±10-cent VCF trim windows, the aged-unit extension, and the
  circuit-derived resonance candidate behind its flag. No mixer or noise
  constant moved; no timing profile promoted; Vref not adopted.
- 2026-08-21 — Exposed the aged-unit extension as the Aging host parameter
  beside Unit Character (AU version hint 4; excluded from randomisation and
  program recall). Consolidated the research docs into this file.

## External validation and market position

A 2026-08-18 release-gate validation rendered all 128 factory presets
through the exact product path and compared each against a public hardware
recording of the same patch. Corpus-level agreement is strong (median
spectral-shape correlation 0.897; 94/128 at or above 0.80); every flagged
preset but one adjudicated as the model rendering exactly what the stored
tone bytes dictate, and two adjudications independently corroborate the
cutoff/resonance calibration at absolute frequencies. The one engine-side
finding — a noise-versus-sub balance inversion on the noise-forward drum
programs — is recorded as a quantified contradiction under OQ-15, not
re-voiced from an undocumented recording chain.

A 2026-08-10 comparative assessment of the JUNO-106 emulation market
concluded that no other emulation, commercial or open, documents a
hardware-evidence chain of comparable depth or fences its claims with
deterministic tests — while stating plainly what it cannot establish:
perceptual superiority, which would need calibrated captures, blind
level-matched listening and third-party replication that exist publicly
for no product, this one included. Where the open field is ahead (KR-106's
in-tree measured tables; raw sibling captures), that is recorded as capture
leads under the owning questions.

## Sources

Primary documentation and parts:

- Roland JUNO-106 Service Notes, First Edition (JUL. 31 1984) — two
  byte-identical circulating scans, read at page level; pixel-measured
  reads of p. 8 (timing chart) and p. 9 (module drawing).
- Roland JUNO-106 Owner's Manual; two Roland Service Information bulletins
  (100222, 100229).
- Hash-identified firmware images (voice/main B-2, assigner A-5) analysed
  behaviourally; published clean-room assigner reverse engineering; the
  unofficial partial IC29 disassembly (provenance-pending leads only).
- Datasheets: Panasonic MN3009 and MN3101; NEC µPC1252H2; Toshiba TA75558,
  TC4051/4052/4053B, 2SK30A; Hitachi HD14051B; Intel 82C54/8254; NEC
  µPD7810/7811; TI TL072/TL082; Rohm BA6110; Alfa AS3109/AS662.
- A1QH80017A VCF/VCA module teardown photographs; the Open80017a/dksynth
  module reconstruction (one lineage); published DCO charge-circuit
  analysis.

Measurements and archives:

- KR-106 (click-timing chorus series, measured code-to-frequency and
  sustain tables, archival tone transcription); ModWiggler and Gearspace
  measurement threads read at post level; Alpes Machines One-O-Six kit
  guide; atosynth reconstruction; Cornutt workbench pages; Analogue
  Renaissance service notes.
- Factory tone corpus cross-checked with zero mismatches across the Hinzen
  tape/PAT archive, the Jarvik7 librarian library and the KR-106
  transcription; Roland's Original 128 announcement.
- Sibling JUNO-60 Service Notes and chorus measurements — labelled
  comparison only.

Literature:

- Zavalishin (TPT, `1/(4−k)`); Stilson & Smith; Huovilainen; D'Angelo &
  Välimäki; Välimäki, Pekonen & Nam (BLEP residuals); Holters & Parker
  (BBD modelling, DAFx-18); Gabrielli, D'Angelo & Squartini (DAFx-25,
  BGA/SGA and BBD polyBLEP); Danish, Bilbao & Ducceschi (DAFx-21, basis of
  a rejected solver candidate).
