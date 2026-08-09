# Comparative fidelity assessment — YouKnow106 in the JUNO-106 emulation market

**Assessed 2026-08-09.** This document places the project's fidelity and
sound-quality claims in the market of JUNO-106 (and sibling Juno-6/60)
software emulations, on criteria that can be checked from public
documentation and source code. It follows the same evidence discipline as
the [research contract](circuit-modelling-research.md): every comparative
cell cites what a vendor or repository actually documents, absence findings
are stated explicitly, and the final section records what this document
*cannot* establish — no perceptual-superiority claim is made anywhere in
it. Open-source projects were read at source level; commercial products can
be assessed only through what their vendors and users publish, which is a
weaker instrument, and that asymmetry is kept visible throughout.

## The claim under assessment

"The most realistic, most faithful recreation of the JUNO-106, with the
highest sound quality" decomposes into three different kinds of claim:

1. **Documented-fidelity coverage** — which hardware mechanisms are
   modelled, from which evidence, with what verification. Checkable today;
   this document's subject.
2. **Measured proximity to hardware** — null tests and calibrated captures
   against original units. Partially checkable (each project's own
   published measurements), fully checkable only with the instrument-level
   captures the [open-questions queue](open-questions.md) specifies.
3. **Perceptual superiority** — blind listening against hardware and
   competitors. Not checkable from any desk; recorded as external
   validation owed.

## Documented-fidelity comparison — open-source field

Source-level survey, 2026-08-07, of every substantive open Juno emulation
found on GitHub: `ultramaster_kr106` (KR-106, the serious 106 model),
`stevengoldberg/juno106` (Web Audio, 529★ but DSP-generic),
`jpcima/Hera` (Juno-60, alpha), `dzannotti/junox` (Juno-60, self-described
"really not great"), plus `pendragon-andyh/Juno60` (a measurement corpus,
not an emulation) and eight minor projects that are generic across every
criterion below. Cells state the mechanism as implemented, with the
strongest competitor named.

| Mechanism | YouKnow106 | Strongest open competitor |
|---|---|---|
| DCO pitch generation | 8 MHz reference divided by 16-bit timer integers — the hardware's own pitch quantisation (A4\@8' = 440.044 Hz), range switch as clock change, and a scanned restart whose exact forced state remains declared model policy under OQ-08 | None models timer quantisation. KR-106 uses continuous phase with the firmware's 8.8 portamento rate law; junox has an integer-period *artifact* |
| DCO numerical quality | The legacy full-engine HQ fence covers only the 8' saw at MIDI 60/84 and clears −70 dBc. An expanded common-host pre-VCF matrix uses the six octave-spaced notes MIDI 36/48/60/72/84/96, every range, saw/sub and 5/50/95% pulse; all 1×/2×/4× cells reject the same worst-single-bin gate, including 4× at −42.62/−41.45 dBc for 44.1/48 kHz. Analytic controls validate the spectrum mask, but the fold cause remains unattributed | No surveyed open project publishes an equivalent common-host, multi-waveform DCO reconstruction matrix |
| Scanned control system | The p. 8 chart's exact 23-write order on a fractional 4.2 ms scheduler; per-destination hold networks, incl. the derived PWM (R117/C62, R116/C63) and SUB (R11/C1) slews; exact intra-pass timestamps declared open (OQ-08) rather than invented | KR-106: 4.2335 ms canonical tick plus per-slot DAC phase offsets from firmware cycle counting — it *claims* the timestamps this project deliberately leaves open; its offsets corroborate our provisional ~125 µs figure |
| Playing latency | Exhaustive current-model characterization at 48 kHz: every one of the 1,008 host/scan boundary phases on all six cards, with Pitch write, ENV-mode VCA write, 687 µs hold milestone and a declared output-onset proxy reported separately from the fixed 24-sample host DSP report | KR-106 v2.5.12 says rebasing its DAC phase tables removed about 2 ms from tick-driven envelope/LFO updates. No like-for-like event-to-node/output distribution or original-JUNO capture is published in the surveyed open field |
| Firmware envelope | Hash-scoped B-2 recurrence: 14-bit state, exact Q(v,c) three-partial multiply with the dropped low×low term, `E>>2` DAC truncation, byte-exact fixtures in the suite | KR-106: the same law at instruction level (cleanroom, j106roms lineage), cross-checked against MAME — parity on the digital law; no automated fixtures |
| Firmware LFO/delay/portamento | Hash-scoped exact laws with regression vectors | KR-106: ROM-table reconstructions verified against four hardware captures — parity in kind |
| VCF | IR3109 cascade behind the anchored 68 kΩ/560 Ω divider; loop gain fitted only to the 4.8 Vpp service amplitude (rendered 4.8009 Vpp), with 247.90 Hz then predicted rather than fitted; 992 Hz WIDTH anchor (tracking exactly 1.00); AS3109 700 µA control-current knee; R-2R carry INL on the converter write | KR-106: IR3109 TPT cascade with BA662 67:1 feedback physics, self-osc calibrated to a named unit, and a **4096-point measured DAC→Hz table from a real card** — a data asset this project matches only by derivation (within 30 cents of that table's shape after the knee fix) |
| VCF numerical quality | Path-average antiderivative/divided-difference evaluation inside the Newton solve. An exhaustive 20 Hz–20 kHz hot-residual FFT clears −60 dBc at 2×/4× with an independent-oracle off-mask control below −93 dBc, but the independent common-host RK64/RK128 comparison rejects every 1×/2×/4× nominal-Character-0 cell on full-waveform NRMS for the production-compensated hot-saw fixture; 4× is still only −24.34/−25.81 dB at 44.1/48 kHz against −40. A one-step bounded-work candidate separately fails the engine-bound/standard-grid scanned-control matrix | KR-106: 2×/4× oversampling, no published equivalent common-host or fold-back fence. No other surveyed project publishes one |
| Chorus | Bucket-clocked two-phase 256-stage MN3009 model: full-period hold confirmed by the datasheet OUT1/OUT2 solve, per-shift transfer loss on the EC-row anchor, derived LFO rates 0.5533/0.8983 Hz from the 106's own timing network (ratio 1.6234799), measured 1.4–6.4 ms sweep, BGA/SGA separation via bounded polyBLEP, no-compander hiss modelled | KR-106: deliberately *not* bucket-clocked (Hermite delay line with a written rationale), surrounded by measured side-effects (CTE gain modulation, leakage noise, clock-reset clicks); Chorus II rate marked "inherited, not re-verified". Hera has the field's only other bucket-level BBD — attached to an admittedly inaccurate alpha |
| HPF incl. bass boost | The boost shelf **derived from the p. 15 branch** (+10.50 dB DC, +1.41 dB HF, 59.41 Hz; within 0.016 dB of the exact 2z/2p solve), cut corners from designators; asymptotic C14 coupling states | KR-106: the same 2p2z transfer from designators, verified 0.55 dB RMS against its (unpublished) hardware noise sweep — convergent result, one lineage of hardware corroboration |
| Common VCA LEVEL | Derived dB-linear law from p. 8/p. 15/NEC (−16.32 + 0.1656·b dB), C7 9.08 ms settling | Not modelled as a distinct stage anywhere else surveyed |
| Output boundary | JIS-B volume law with internal 29.3 kΩ wiper loading, three coupling boundaries, −18 dBFS RMS declared policy | Generic gain staging everywhere else |
| Patch compatibility | Exact 18-byte tone format, .syx load/save/drag-drop, factory bank in hardware order, PC 0–127 | KR-106 has patch banks; none document byte-exact SysEx round-tripping |
| Deterministic verification | Nine JUCE-free CTest contracts; anchored laws are fenced, while reviewed numerical failures are also locked as explicit rejections rather than silently counted as passes | **None in the entire open field.** KR-106 ships manual offline analysis tools only |
| Evidence discipline | Public research contract with provenance classes (anchored / ROM-resolved / derived / voiced / policy), a 21-item open-questions queue with capture protocols, recorded rejections | KR-106 embeds measurement claims in comments and ships some raw CSVs (a genuine strength — this project records fixtures and hashes instead, and its raw-capture asks are still open); no other project documents provenance at all |

Where the open field is genuinely ahead, stated plainly: KR-106 ships
in-tree measured artefacts this project does not have (the 4096-point DAC
table, a 56-point measured VCA sustain table, per-slot write offsets,
ADSR capture CSVs), and `pendragon-andyh/Juno60` remains the only public
repository of raw hardware WAV captures — for the sibling instrument.
This project's corresponding positions are derivations from primary
documents plus the queue's still-open capture protocols; where the two
approaches have met (the DAC curve's shape, the ~125 µs offset, the boost
shelf), they have converged.

## Documented-fidelity comparison — commercial field

Vendor pages, manuals (PDF text), KVR forum threads and press were
surveyed the same day. Closed-source products can only be assessed through
what is published — a documentation gap is not proof a mechanism is
unmodelled, and that asymmetry stands. The direct 106 products are Roland
Cloud JUNO-106 (ACB), Cherry Audio DCO-106 and **Softube Model 84** —
which, contrary to common listing, is a 106 emulation (its manual's 1984
unit, its I/II chorus exclusivity) and is the forum-consensus authenticity
leader among commercial products (KVR t=609080, t=564111, with Starsky
Carr's side-by-side spectra). TAL-U-NO-LX (60), Arturia Jun-6 V (6) and
u-he Diva (generic; Alpha-Juno-shaped DCO/envelope per its own manual)
border the market.

What the commercial field documents, against this project's coverage:

- **The scanned control system and firmware laws are documented by no
  commercial vendor at all.** Roland claims "ACB… circuit behaviors"
  without detail; Cherry claims "exhaustive detail" without method;
  Softube documents component-level modelling of "a fully-serviced and
  calibrated 1984 unit" and press independently reports its recreated
  cutoff stepping — the closest any commercial product comes to the 4.2 ms
  scan this project executes as the chart's 23-write order.
- **Chorus:** Roland documents modelled, adjustable chorus noise; Arturia
  documents "BBD character" plus a Chorus Noise control (on a Juno-6
  model); Softube's chorus noise and instability are press-reported; TAL
  and Cherry document generic choruses (Cherry's praised by a 106 owner
  precisely for *lacking* the noise). No vendor documents bucket-level BBD
  structure, clock-domain behaviour, or the 106's measured sweep — the
  mechanisms this project anchors to the MN3009 datasheet, the p. 15
  timing network and the measured 1.4–6.4 ms sweep.
- **HPF bass boost:** documented by Softube ("bass boost" EQ) and Diva
  (a BOOST position, hardware unnamed); absent from Roland's, Cherry's and
  TAL's documentation. No vendor publishes the transfer; this project
  derives it from the branch designators.
- **Per-voice calibration:** documented by Roland (Circuit Mod/Condition),
  TAL (Service Control), Arturia (three states) and Diva (Trimmers/
  Variance) — all as adjustable character features, none tied to a
  published measured dispersion. This project's Unit Character is likewise
  labelled compatibility, with the honest difference that its nominal
  model is zero-spread pending population data (OQ-10).
- **Patch compatibility:** Cherry Audio is the only commercial product
  documenting live SysEx interchange with a real 106 (single patches, plus
  control-surface use) — and simultaneously draws the field's harshest
  owner criticism on accuracy ("junk in terms of accuracy", KVR
  t=609080, against another owner's "spot on"). Roland sells recreated
  factory banks rather than documenting dump compatibility. This project
  round-trips the exact 18-byte tone format and ships the factory bank in
  hardware order.
- **Published hardware evidence:** TAL is the only vendor publishing its
  own hardware-versus-plugin audio comparisons (with a filter spectrum in
  the manual) — for the Juno-60. No 106 vendor publishes measurements,
  null tests or an evidence chain of any kind.
- **Authentic voice behaviour:** only Softube keeps the six-voice limit
  (and two poly modes); Roland extends to 2–8, Cherry to 16, Arturia
  to 36. This project defaults to the six cards with note-dropping
  assignment and labels more voices an extension.

The forum record worth keeping whole: hardware owners rank Softube first,
Roland's own plug-in mid-pack ("not super accurate to my originals"),
Arturia "grainier", and split irreconcilably on Cherry — a reminder that
undocumented listening consensus is unstable evidence, which is exactly
why this project's claims are tied to documents and tests instead.

## Measured proximity — the scoreboard that exists today

Dimension 2 is not empty while the queue's captures are awaited: several
of the model's laws can be, and now have been, checked against hardware
measurements that already exist. Each row names its hardware truth and its
verification; "fenced" means a deterministic test fails if the model
drifts off the value.

| Law | Hardware truth | Model | Status |
|---|---|---|---|
| VCF cutoff, full range | A real 106 voice card's measured DAC→Hz curve (93 measured codes, log-interpolated to 4096, gain-calibrated at the service anchor; third-party, GPL — measurements cited as facts, comparison re-derived) | Shipping law re-computed at every code (2026-08-07): **musical core 100 Hz–8 kHz within ±20 cents, RMS 10.3 cents** at nominal; audible-band RMS 19.7 cents; extremes carry the recorded deliberate base/slope trade pinned to Roland's own 248 Hz anchor (+0.4 cents there). Part of the residual is the card's raw slope, which its own WIDTH trim absorbs in service | Measured against one real card |
| VCF R-2R carry steps | The same card's bit-boundary steps: −0.50/+27.49/−0.33 cents locally at codes 1024/2048/3072 | −0.44/+27.49/−0.27 cents at Unit Character 1.0 — **the audible mid-sweep step class reproduced to under 0.1 cent** | Measured; carry constants sourced from this data |
| Self-oscillation endpoint | Service ADJUSTMENT: 4.8 Vpp sine at 248 Hz, every card | 4.8009 Vpp; 247.90 Hz predicted after fitting amplitude alone | Anchored, fenced |
| Key tracking | Service WIDTH anchor: C6 self-oscillates at 992 Hz = exactly two octaves over C4's 248 Hz | Exactly 4×, end-to-end through the converter path | Anchored, fenced |
| PWM duty windows | Service: 48–52% at PWM 50, 93–97% at PWM 10 | Inside both windows | Anchored, fenced |
| Chorus mode rates | A 106-chorus clone's scope readings 0.537/0.879 Hz; owner's manual "about 0.5/0.8" | Derived 0.5533/0.8983 Hz from the 106's own timing network (−3.0%/−2.2% vs the scope) | Derived, corroborated |
| Chorus sweep | 1.4–6.4 ms scoped on a designator-faithful build with genuine MN3009s, called identical against the measurer's real 106 | Shipped endpoints | Third-party measured |
| BBD transfer | MN3009 EC row: −3 dB at 12 kHz, 40 kHz clock (edition-verified 2026-08-07) | −3.000 dB at the anchor | Anchored, fenced with a cross-reading guard band |
| Digital laws | Hash-identified A-5/B-2 firmware behaviour | Byte-exact envelope/LFO/delay/portamento fixtures | ROM-resolved, fenced |
| Chorus noise delta | Same-chain real-106 true-peak captures report approximately 3.95 dB II−I with Panasonic and Xvive MN3009 populations (3.96/3.95 dB from the printed pairs) | The shipped default applies `10^(3.95/20) = 1.575796` to mode II while leaving mode I sample-bit-identical. The internal `useChorusRateNoiseHypothesis` comparison substitutes the circuit-derived 4.2089 dB rate-law prediction | Aggregate relative delta matched at moderate confidence; treating true peak as a broadband amplitude factor, plus absolute PSD, weighting, stereo correlation, spurs and physical cause, remains OQ-03 |

What this scoreboard is not: a full-instrument null test. That requires
the calibrated same-chain captures the queue specifies, and no product on
the market has published one either.

### Measured head-to-head against KR-106 (2026-08-07)

The one competitor that can be benchmarked symmetrically was: KR-106's
engine rendered the identical ten factory patches at the identical keys,
and its output went through the byte-identical adjudicated pipeline
against the same real-hardware demo segments. Read straight: **the two
models measure comparably close to the real unit on this material —
neither separates beyond the material's own noise.** YouKnow106 is closer
on harmonic-stack accuracy in 5 of 9 valid cases and clearly closer on
the two comparisons the adjudication rated cleanest (A15: 2.23 vs
5.39 dB RMS; A38: 2.95 vs 6.29); KR-106 reads closer on broadband band
envelopes in 9 of 10 (means 9.65 vs 10.64 dB). Two controls were run on
that band result. A hiss-muted re-render refuted the first conjecture
(that YouKnow106's chorus-hiss model was penalised against the
MP3-floored reference). A three-way band-region decomposition then
identified the real mechanism: **KR-106's modelled noise floor
coincides with the recording's own floor (±1.3 dB in every region)
while YouKnow106's sits ≈5 dB below it**, so the gap's largest
component — ≈5 dB of band deviation in the floor-dominated 4–8 kHz
region — is the metric rewarding agreement with the recording chain,
which an undocumented lossy chain cannot convert into a fidelity claim
about the instrument in either direction. The genuine residual is the
small (≈1–2 dB, mixed-direction) low/mid-band difference.

One further axis was benchmarked to close the set: **technical rendering
cleanliness**, measured on an identical bright-open-filter stress render
(16′ C5 saw, cutoff maximum, resonance 0.6, chorus off, 44.1 kHz host)
through both engines. Result: parity — worst inharmonic line −67.3 dBc
on both, total inharmonic energy −53.3 vs −52.5 dBc, the visible
near-carrier sidebands being each engine's own per-voice drift
mechanism, not aliasing. Recorded as measured rather than escalated:
hunting progressively adversarial corners until one engine loses would
be motivated measurement. The standing difference on this axis is
verification, not the number — YouKnow106 deterministically fences the
exhaustive 20 Hz–20 kHz hot-VCF residual spectrum and now records the expanded DCO, full-waveform VCF and
BBD matrix rejections; no competitor publishes equivalent gates. Pitch is
essentially exact on both. The axes this material cannot measure — scan
stepping, BBD clock behaviour, alias floors, and every deterministically
fenced anchor — remain where the two projects genuinely differ, per the
comparison above; and on the cutoff law KR-106 ships the measured table
as a lookup, so that axis matches by construction on their side and by
derivation on this one. Full tables, clips and caveats live on the
published comparison page.

## Playing latency — characterized, hardware timing still open

**Added 2026-08-09.** The engine fixture advances a live scheduler one host
sample at a time; it never writes a private scan phase. At 48 kHz the phase
increment is 5/1008 of a pass per sample, so 1,008 boundaries cover the complete
host/scan phase cycle over five 4.2 ms passes. Poly-1 note memory selects each
physical card, followed by a two-second exact-silence pre-roll. The measured
note is C4 through a saw-only, open-filter patch with ENV VCA, zero attack, full
sustain, HPF I, chorus/noise/Unit Character off. Offsets are 0-based host
samples from the timestamped Note On; each cell is min / median / max across
6,048 card/phase cases.

| Layer | HQ off | HQ on | Scope |
| --- | ---: | ---: | --- |
| Host-reported numerical DSP latency | 24 / 24 / 24 | 24 / 24 / 24 | 0.500 ms at 48 kHz; a nominal group-delay report, separate from scan/hold and external buffering |
| Pitch/envelope write | 0 / 100 / 201 | 0 / 100 / 201 | Current normalized 23-write schedule |
| VoiceVca target / first nonzero model gain | 70 / 192 / 315 | 70 / 192 / 315 | ENV mode; cannot precede that card's Pitch/envelope tick |
| Held control reaches 63.2% | 102 / 224 / 347 | 103 / 225 / 348 | Current continuous realization of the component-derived 687 µs hold |
| Raw output-onset proxy | 90 / 213 / 335 | 93 / 216 / 339 | First `max(abs(L),abs(R)) > 1e-4` (−80 dBFS amplitude); signal/patch dependent |
| Nominal host-compensation coordinate | 66 / 189 / 311 | 69 / 192 / 315 | Raw proxy index minus the 24-sample report; not a claim of an exact threshold delay |

HQ evaluates the converter queue on four internal substeps while HQ-off checks
once per host sample. At a few exact arrival boundaries one mode has already
passed a write that the other reaches after the MIDI event; paired cases can
therefore differ by one scan, with a measured worst raw-proxy difference of 205
samples, even though the aggregate distributions align. That is scan-grid
quantisation, not an extra 205 samples of output-path group delay.

The 4.2 ms pass, 23-write order and qualitative non-simultaneity are anchored.
The normalized sub-pass offsets, phase origin and continuously slewed hold are
compatibility/product behavior. The output threshold is a numerical regression
proxy, not psychoacoustic audibility, and this sweep is exhaustive only over
48 kHz converter phase for the declared patch and pre-roll. Exact hardware
offsets, acquisition behavior and audible thresholds remain OQ-07, OQ-08 and
OQ-12; the BA662 onset law remains OQ-19.

## Real-time cost — the axis this document was missing

**Added 2026-08-07 by the [best-in-class pass](best-in-class-plan.md).** The
comparison above weighs mechanisms and measured proximity and said nothing
about what the plug-in costs to run, although that is one of the axes this
market actually separates products on: Cherry Audio advertises lightness as a
feature of DCO-106 ("a lightweight yet powerful engine that manages 16 voices
of polyphony without overwhelming your system"), Roland Cloud's ACB JUNO-106
is repeatedly marked down for it — around 10% of a core with no note sounding,
and heavier than TAL's whole chain in direct comparison (KVR t=518758,
t=524111) — and TAL is the field's light-CPU reference point.

Measured on this project: one 2.8 GHz core, host rate 48 kHz, block 256, HQ on
(4× internal, 192 kHz), Unit Character 1.0, best of three three-second
renders. Provenance correction, 2026-08-09: the harness uses
`std::chrono::steady_clock`, so this is elapsed wall time rather than the
“process CPU time” previously printed here. Before and after the pass, same
harness, back to back:

| Scenario | Before | After |
|---|---|---|
| Idle — no key held, six cards running behind closed VCAs | 1.398× realtime | **0.852×** |
| Six voices, chorus off, resonance 0.10 | 1.105× | **0.699×** |
| Six voices, chorus II, saw+pulse+sub+noise, resonance 0.70 | 2.376× | **1.361×** |
| Six voices, chorus off, resonance 0.95 | 3.960× | **1.395×** |
| Six voices, chorus II, full mixer, HQ off | — | **0.449×** |
| Sixteen-voice extension, chorus II, full mixer | 7.7× | **4.10×** |

The figures are elapsed wall seconds per second of audio, so under 1.0
completed faster than realtime under that run's machine load. Read straight,
and the "before" column is the honest part: **this
engine did not run in real time on that machine in any configuration, and it
cost more with no key held than with six voices sounding.** That was the
largest measurable gap the project had against the commercial field, and none
of it was a hardware-evidence question — it was libm in the filter's implicit
solve, a convergence test single precision could not satisfy, and
loop-invariant work inside the per-sample loops.

Equally straight about the "after" column: **two of the four scenarios crossed
the realtime line and two did not.** Idle and ordinary six-voice playing now
run in real time on that machine; the chorus-engaged and near-oscillation
cases are still above it, at 1.36 and 1.40. The pass roughly halved the cost
without moving a calibrated constant; it did not make the instrument cheap.

Two caveats this table does not escape. Cross-product CPU comparison is not
possible from published material: no vendor states a measurement condition,
and the forum figures above are user reports, not benchmarks under a declared
patch, host, buffer and rate. And a figure from one machine is a figure from
one machine — what it supports is the before/after claim on this project's own
code, not a ranking against a competitor. The suite fences a same-run
wall-time *ratio* of resonant to plain render cost rather than an absolute
duration for the solver-specific check; a separate `testCpuBudget` retains a
deliberately coarse absolute runaway ceiling.

### Oversampling work attribution: measured, no split admitted

**Added 2026-08-09.** `YouKnow106OversamplingAudit` links the normal shipping
DSP and times only `engine.process` with the current thread's CPU clock. The
state is pre-rolled for two seconds, copied before the timer and rendered in
256-frame blocks; seven 4×/1× pairs alternate order and retain every raw run,
median, minimum, median absolute deviation and raw-float fingerprint. On an
Apple M1 Max under macOS 26.5.1, native arm64 Release, 48 kHz, the 32,768-frame
Unit-Character-1.0 windows measured:

| Current fixture | 4× CPU s / audio s | 1× CPU s / audio s | Paired 4× / 1× |
| --- | ---: | ---: | ---: |
| Idle, six physical cards behind closed VCAs | **0.533** | **0.145** | **3.684×** |
| Six voices, chorus off, cutoff 0.62, resonance 0.10 | **0.495** | **0.152** | **3.254×** |
| Six voices, chorus off, cutoff 0.62, resonance 0.95 | **0.659** | **0.191** | **3.452×** |
| Six voices, full mixer, chorus II/noise 1.0, resonance 0.70 | **0.766** | **0.292** | **2.589×** |

Every timing median absolute deviation is below 1%. These are JUCE-free
engine thread-CPU figures, not plug-in, host or device totals. The older table
above remains useful as a same-machine before/after history, but its clock was
`steady_clock`; it is not merged with this CPU-clock baseline.

The companion `YouKnow106DSPWorkAudit` recompiles only Engine and Chorus with
non-atomic semantic counters. That build is never timed and never linked into
the plug-in. CTest renders both libraries and requires matching raw-float
fingerprints with the counter sink active, then checks every structural identity. On the declared
six-voice resonant fixture, a 2,048-host-frame window at 48 kHz gives:

| Counted work | 4× | 1× | Scaling the fixture establishes |
| --- | ---: | ---: | --- |
| Internal frames / scan polls / chorus calls | 8,192 each | 2,048 each | internal grid, exactly `q·H` |
| Six-card audio updates / DCO frames / VCF steps | 49,152 each | 12,288 each | six powered cards on every internal frame |
| Sixteen-slot hold updates / PWM solves | 131,072 each | 32,768 each | all product slots on every internal frame |
| Two BBD-line support frames | 16,384 | 4,096 | two lines on every internal frame, even Chorus Off |
| Newton iterations, zero recoveries | 210,549 | 57,052 | 4.284 vs 4.643 iterations/VCF step; data/grid dependent |
| Cutoff memo misses | 1,421 | 1,385 | nearly wall-time driven; 2.89% vs 11.27% of card updates |
| Converter pass starts / writes | 10 / 234 | 10 / 233 | model's anchored nominal 4.2 ms pass, one numerical-window boundary write apart |
| DCO cycle wraps / BBD shifts | 61 / 3,162 | 61 / 3,162 | oscillator and asynchronous BBD-clock events track elapsed time |
| Past + future BLEP correction visits | 12,648 | 12,651 | edge/event driven, not four times larger at 4× |
| Half-band calls / stereo nonzero-tap MACs | 6,144 / 405,504 | 0 / 0 | three decimations and 198 stereo MACs per 4× host frame |

The same regression covers 96 kHz 2×/1× and 192 kHz 1×: one 2× decimator
call and 66 stereo nonzero-tap MACs per host frame, none at 1×. It also proves
four stage evaluations and two bidiagonal solves per Newton iteration, exact
memo and path-average partitions, no recovery in the declared fixture, and
the expected 23-write boundary tolerance.

This is work attribution, not a selective-rate admission. Counters with
different semantics are not cycle weights; the whole-engine 4×/1× ratios do
not say what a future split architecture will save. More importantly, DCO,
VCF/VCA, scan/holds, BBD/support processing and their reconstruction boundary
currently share one loop. The common-host qualification below now supplies the
isolated DCO, RK64/RK128 VCF, closed-form BBD and analytic scan comparisons. It
admits no split: the three rejecting audio fixtures block every tested factor,
and the
matrix does not include the inter-domain reconstruction, whole-engine or
latency proof a production architecture would require. No DSP equation, rate
selection or rendered sample changed in either measurement step.

### Common-host numerical quality: no split admitted

**Added 2026-08-09.** Each audited boundary or declared fixture was rendered at
44.1 and 48 kHz through candidate internal factors 1×, 2× and 4×. DCO harmonic
coefficients and analytic controls define its wanted mask and gain references;
the VCF and BBD oracles cross an independent 4,097-tap `q=16` Kaiser FIR with
explicit delay compensation. Four times is a candidate under test, never the
reference truth. Every audited fixture/factor cell is **REJECT**, so the current
global quality selector remains untouched.

The DCO grid uses the six octave-spaced notes MIDI 36/48/60/72/84/96, all three
ranges, saw and sub, and pulse at 5/50/95% duty. Its gate is ≤−70 dBc; the
reported value is the worst single
off-mask FFT bin, not integrated alias energy or a full-band floor. Independent
analytic multiline controls validate the mask. The normalized scan-ordinal
checks and declared DCO/PWM/SUB recurrences pass, but they neither identify the
fold mechanism — which remains **UNATTRIBUTED** — nor establish hardware scan
timing.

| Audited boundary/fixture and rejecting metric | Host | 1× | 2× | 4× | Gate |
| --- | ---: | ---: | ---: | ---: | ---: |
| DCO worst off-mask bin (dBc) | 44.1 kHz | −12.780565 | −36.596878 | −42.618000 | ≤−70 |
| DCO worst off-mask bin (dBc) | 48 kHz | −16.741087 | −36.344575 | −41.452375 | ≤−70 |
| VCF hot-saw NRMS (dB) | 44.1 kHz | −1.110 | −12.232 | −24.343 | ≤−40 |
| VCF hot-saw NRMS (dB) | 48 kHz | −1.062 | −13.751 | −25.810 | ≤−40 |

The VCF reference is an explicit RK128 solve whose convergence is cross-checked
against RK64. The
listed nominal-Character-0 fixture applies the production resonance input
compensation to the hot saw. The exhaustive 20 Hz–20 kHz residual FFT masks
only ±6 bins around each legitimate output harmonic; its 1×/2×/4× maxima are
−27.063/−67.588/−99.040 dBc at 44.1 kHz and
−19.658/−67.128/−99.618 dBc at 48 kHz against a <−60 dBc gate. The matching
oracle-only off-mask controls are −93.242/−93.163 dBc. Those 2×/4× passes do
not override the complete-waveform NRMS rejection or qualify Character 1.

The deterministic BBD oracle evaluates the documented component filters, the
128-edge transfer and complete zero-order-hold image phasors at a bounded
linearized drive. It deliberately uses the same model anchors, so it is an
independent numerical implementation, not a new hardware measurement or truth.
BGA is absolute error in wanted physical-image level; exactly one
non-fundamental wanted line clears the projection threshold per cell, so its
column is not an exhaustive image population. SGA is unwanted host-grid level
reported as the maximum over every unmasked 20 Hz–20 kHz Blackman–Harris FFT
bin. Only validated wanted physical lines whose source is ≤20 kHz may be masked.

| BBD metric | Host | 1× | 2× | 4× | Gate |
| --- | ---: | ---: | ---: | ---: | ---: |
| Analytic NRMS (dB) | 44.1 kHz | −3.099 | −14.910 | −27.043 | ≤−40 |
| Analytic NRMS (dB) | 48 kHz | −4.640 | −16.427 | −28.183 | ≤−40 |
| Qualifying-line BGA level error (dB) | 44.1 kHz | 34.389 | 4.090 | 0.869 | ≤0.75 |
| Qualifying-line BGA level error (dB) | 48 kHz | 22.893 | 3.257 | 0.708 | ≤0.75 |
| 20 Hz–20 kHz unmasked SGA max (dBc) | 44.1 kHz | −24.854 | −28.762 | −47.635 | <−60 |
| 20 Hz–20 kHz unmasked SGA max (dBc) | 48 kHz | −28.871 | −31.329 | −38.189 | <−60 |

Individual submetrics can pass without admitting a domain: for example, the
48 kHz/4× BGA error is inside 0.75 dB, while that cell still fails analytic
NRMS and SGA. Oracle controls put the post-FIR omitted-image tail at
−198.030/−202.098 dBc and exhaustive 20 Hz–20 kHz off-mask content at
−93.046/−135.607 dBc for 44.1/48 kHz. The same admission rule applies to
the exhaustive VCF residual spectrum and to the scan/hold controls. No production
selector, rate, audio path or preset changed. Inter-domain reconstruction,
whole-engine equivalence and latency qualification remain mandatory even after
a future isolated domain clears all of its gates.

### Bounded-work VCF candidate: matrix rejection

**Added 2026-08-09.** The cited DAFx-21 port-Hamiltonian construction is a
promising bounded-work method for its own Korg35 and Moog equations, not a generic
replacement or an automatic stability proof for this IR3109 model. A
research-only one-step quasi-Newton candidate therefore keeps the complete shipping
equations and is judged before any production switch exists. It uses exactly
one system evaluation and two bidiagonal solves per sample instead of up to
eight shipping iterations; its Early-effect derivative is frozen, so it is not
described as an exact tangent.

The candidate is excellent under static parameters: worst small-signal gain
error is 0.01368 dB, its worst hot waveform error against explicit 64× RK4 is
−46.03 dB RMS versus shipping's −44.60 dB (both at `k=4.4`), its normalized residual is 1.84e-5,
static stage-tolerance/headroom/Early-effect parity is −114.88 dB RMS, and the
existing hot fold-back probe reads −66.41 dBc. It also preserves the retime and
oscillation/boundedness classes. The decisive reachable-control fixture covers
all six VCF-card ordinals on the normalized 23-write pass at both engine bounds
and the 44.1/48/88.2/96/176.4/192 kHz standard internal grids. It applies the
production holds, flooring, compensation and mapping/caps with Unit Character
zero, keeping the decisive motion case nominal while a separate static fixture
covers character mechanisms. It fails the ≤−40 dB parity gate everywhere: the
worst error is +21.31 dB RMS at 8 kHz/card 1 (`g=6.31375`), 44.1 kHz is
+18.50 dB, and even 192 kHz reads +5.01 dB.
The separately retained +4.80 dB result comes from `g=30` jumps, instantaneous
resonance and audio-rate thermal-headroom changes the plug-in cannot generate;
it is an out-of-domain boundedness diagnostic, not automation evidence.

The isolated matrix therefore **rejects the candidate**. The superseded
−97.56 dB result used only a 192 kHz fixture with invented cutoff/resonance
phases rather than the production ordinals. Fixed evaluation/solve counts are
not invariant CPU time, and a fast solver that fails the reachable signal
contract has no production value. No production DSP changed; the market
comparison above continues to describe the shipping Newton solver. A later
bounded-work design must first clear the same engine-bound/standard-grid six-card circuit
matrix before engine integration or benchmarking.

## Market-presence note

Public attention in the open field is inverted relative to fidelity: the
dormant 2015 Web Audio project holds the most stars while the only
firmware-level open model is a 2026 newcomer. Star counts and marketing
reach measure presence, not fidelity; this document ranks neither product
nor project by them.

## External-validation register — what this document cannot establish

The superlative in the project goal is only fully decidable by evidence
that does not yet exist publicly for any product, this one included:

1. **Calibrated null/capture tests against original units** — the exact
   protocols are already written, per mechanism, in the
   [open-questions queue](open-questions.md) (OQ-01, -03, -05, -09, -15,
   -19 are the audible blockers). No vendor publishes such data either.
2. **Blind listening panels** against hardware and against the named
   competitors, with disclosed patches and levels.
3. **Third-party replication** of this project's derivations — the
   research contract and suite exist precisely so that a third party can
   check every constant without trusting this document.

Until those exist, the defensible market statement is narrower than the
goal's phrasing, and it is this: **no other JUNO-106 emulation, commercial
or open, documents a hardware-evidence chain of comparable depth, fences
its claims with deterministic tests, or models the combination of the
scanned control system, timer-quantised DCOs, firmware-exact modulation
laws, bucket-clocked BBD chorus and derived output network that this
project does — and where hardware measurements exist today, the model has
been run against them and the residuals published above, a scoreboard no
competitor publishes at all.** Fidelity coverage and measured proximity to
the evidence in hand are matters of record; perceptual superiority is not
claimed.
