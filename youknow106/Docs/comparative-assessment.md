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
| DCO numerical quality | The legacy full-engine HQ fence covers only the 8' saw at MIDI 60/84. The expanded common-host pre-VCF matrix uses MIDI 36/48/60/72/84/96, every range, saw/sub and 5/50/95% pulse. Its dated Step 6 baseline rejected all six cells, including 4× at −42.62/−41.45 dBc for 44.1/48 kHz. Step 7 fixes the numerical reconstruction without changing the divider/ramp/comparator/sub/scan laws: 1×/2×/4× now measure −83.48/−82.44/−82.43 and −84.88/−92.98/−92.98 dBc, and every alias, gain, analysis, scan and hold gate passes | No surveyed open project publishes an equivalent common-host, multi-waveform DCO reconstruction matrix |
| Scanned control system | The p. 8 chart's exact 23-write order on a fractional 4.2 ms scheduler; per-destination hold networks, incl. the derived PWM (R117/C62, R116/C63) and SUB (R11/C1) slews; exact intra-pass timestamps declared open (OQ-08) rather than invented | KR-106: 4.2335 ms canonical tick plus per-slot DAC phase offsets from firmware cycle counting — it *claims* the timestamps this project deliberately leaves open; its offsets corroborate our provisional ~125 µs figure |
| Playing latency | Exhaustive current-model characterization at 48 kHz: every one of the 1,008 host/scan boundary phases on all six cards, with Pitch write, ENV-mode VCA write, 687 µs hold milestone and a declared output-onset proxy reported separately from the fixed 41-sample host DSP report | KR-106 v2.5.12 says rebasing its DAC phase tables removed about 2 ms from tick-driven envelope/LFO updates. No like-for-like event-to-node/output distribution or original-JUNO capture is published in the surveyed open field |
| Firmware envelope | Hash-scoped B-2 recurrence: 14-bit state, exact Q(v,c) three-partial multiply with the dropped low×low term, `E>>2` DAC truncation, byte-exact fixtures in the suite | KR-106: the same law at instruction level (cleanroom, j106roms lineage), cross-checked against MAME — parity on the digital law; no automated fixtures |
| Firmware LFO/delay/portamento | Hash-scoped exact laws with regression vectors | KR-106: ROM-table reconstructions verified against four hardware captures — parity in kind |
| VCF | IR3109 cascade behind the anchored 68 kΩ/560 Ω divider; loop gain fitted only to the 4.8 Vpp service amplitude (rendered 4.8009 Vpp), with 247.90 Hz then predicted rather than fitted; 992 Hz WIDTH anchor (tracking exactly 1.00); AS3109 700 µA control-current knee; R-2R carry INL on the converter write | KR-106: IR3109 TPT cascade with BA662 67:1 feedback physics, self-osc calibrated to a named unit, and a **4096-point measured DAC→Hz table from a real card** — a data asset this project matches only by derivation (within 30 cents of that table's shape after the knee fix) |
| VCF numerical quality | Step 10 advances the continuous four-stage equations with two fixed five-stage Merson RK4 halfsteps, causal cubic drive and endpoint-linear ordinary controls over four double capacitor states. Step 11 preserves its ten RHS evaluations but, only in intervals containing a cutoff or shared-resonance write, evaluates the exact segmented 522 µs hold at all seven Merson nodes. A pure scheduler peek latches the fractional event payload without consuming the official cursor/target; the normal poll commits it once. All six standard HQ cells plus 8/768 kHz engine bounds pass at −84.881…−119.340 dB against independent RK64/RK128 references and the unchanged −40 dB gate. Late/ceil and early/floor snap mutations reject at −33.245/−32.007 dB | KR-106: 2×/4× oversampling, no published equivalent common-host, converter-schedule dynamic or fold-back fence. No other surveyed project publishes one |
| Chorus | Bucket-clocked two-phase 256-stage MN3009 model: full-period hold confirmed by the datasheet OUT1/OUT2 solve, per-shift transfer loss on the EC-row anchor, derived LFO rates 0.5533/0.8983 Hz from the 106's own timing network (ratio 1.6234799), measured 1.4–6.4 ms sweep, causal four-point Lagrange edge-input interpolation, bounded-polyBLEP BGA/SGA separation and combined exact continuous six-state support integration on every shipping HQ path, with no-compander hiss modelled | KR-106: deliberately *not* bucket-clocked (Hermite delay line with a written rationale), surrounded by measured side-effects (CTE gain modulation, leakage noise, clock-reset clicks); Chorus II rate marked "inherited, not re-verified". Hera has the field's only other bucket-level BBD — attached to an admittedly inaccurate alpha |
| HPF incl. bass boost | The boost shelf **derived from the p. 15 branch** (+10.50 dB DC, +1.41 dB HF, 59.41 Hz; within 0.016 dB of the exact 2z/2p solve), cut corners from designators; asymptotic C14 coupling states | KR-106: the same 2p2z transfer from designators, verified 0.55 dB RMS against its (unpublished) hardware noise sweep — convergent result, one lineage of hardware corroboration |
| Common VCA LEVEL | Derived dB-linear law from p. 8/p. 15/NEC (−16.32 + 0.1656·b dB), C7 9.08 ms settling | Not modelled as a distinct stage anywhere else surveyed |
| Output boundary | JIS-B volume law with internal 29.3 kΩ wiper loading, three coupling boundaries, −18 dBFS RMS declared policy | Generic gain staging everywhere else |
| Patch compatibility | Exact 18-byte tone format, .syx load/save/drag-drop, factory bank in hardware order, PC 0–127 | KR-106 has patch banks; none document byte-exact SysEx round-tripping |
| Deterministic verification | 11 JUCE-free CTest contracts; anchored laws are fenced, while reviewed numerical failures are also locked as explicit rejections rather than silently counted as passes | **None in the entire open field.** KR-106 ships manual offline analysis tools only |
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

### Dated audio comparisons retired from the current corpus

The 2026-08-07 head-to-head and the later before/after listening corpora remain
recoverable from the Step 9 commit, preserving that dated history. They were
removed when the maintained [audio corpus](audio/README.md) was regenerated
from scratch, so neither their
clips nor their deltas are presented as evidence for the current engine. The
maintained corpus now contains only ten fresh demonstrations and the fresh
[128-row factory report](audio/factory-presets/README.md) with ten common-gain
previews. For Step 11, two independent demo renders and two independent full
factory renders from the frozen engine produced byte-identical trees. The
canonical 23-file manifest is
`764f2770d21a138163c756025551dc8ead7925f4cf003eb98e960234afc098ea`;
all 20 WAVs are finite stereo PCM16, with maximum absolute DC
`0.000000576 FS` and worst edge `−46.96 dBFS`. Current comparative claims
rest on reproducible numerical contracts and primary-source mechanism coverage,
not on an undocumented recording-chain comparison or a legacy listening delta.

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
| Host-reported numerical DSP latency | 41 / 41 / 41 | 41 / 41 / 41 | 0.854 ms at 48 kHz; a nominal group-delay report, separate from scan/hold and external buffering |
| Pitch/envelope write | 0 / 100 / 201 | 0 / 100 / 201 | Current normalized 23-write schedule |
| VoiceVca target / first nonzero model gain | 70 / 192 / 315 | 70 / 192 / 315 | ENV mode; cannot precede that card's Pitch/envelope tick |
| Held control reaches 63.2% | 102 / 224 / 347 | 103 / 225 / 348 | Current continuous realization of the component-derived 687 µs hold |
| Raw output-onset proxy | 87 / 210 / 335 | 105 / 228 / 351 | First `max(abs(L),abs(R)) > 1e-4` (−80 dBFS amplitude); signal/patch dependent |
| Nominal host-compensation coordinate | 46 / 169 / 294 | 64 / 187 / 310 | Raw proxy index minus the 41-sample report; not a claim of an exact threshold delay |

The fixed report corresponds to raw reconstruction centers of 24 host samples
plus 17 samples of padding at 1×, 35.5 plus 6 at 2×, and 41.25 at 4×: the
nominal centers align within 0.5 sample. The −80 dBFS proxy can cross the
symmetric correction's pre-ringing at a different point for each factor, so
its HQ-off/on shift is not a measurement of that center. Forty-one samples are
0.930 ms at 44.1 kHz, 0.854 ms at 48 kHz, 0.427 ms at 96 kHz and 0.214 ms at
192 kHz.

HQ evaluates the converter queue on four internal substeps while HQ-off checks
once per host sample. At a few exact arrival boundaries one mode has already
passed a write that the other reaches after the MIDI event; paired cases can
therefore differ by one scan, with a measured worst raw-proxy difference of 223
samples, even though the aggregate distributions align. That is scan-grid
quantisation, not an extra 223 samples of output-path group delay.

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
256-frame blocks; repeated 4×/1× runs retain every raw run, median, minimum,
median absolute deviation and raw-float fingerprint. On an Apple M1 Max under
macOS 26.5.1, native arm64 Release, 48 kHz, the 32,768-frame
Unit-Character-1.0 windows measured the following dated Step-8 baseline:

| Step-8 fixture | 4× CPU s / audio s | 1× CPU s / audio s | Paired 4× / 1× |
| --- | ---: | ---: | ---: |
| Idle, six physical cards behind closed VCAs | **0.520** | **0.147** | **3.545×** |
| Six voices, chorus off, cutoff 0.62, resonance 0.10 | **0.493** | **0.154** | **3.175×** |
| Six voices, chorus off, cutoff 0.62, resonance 0.95 | **0.636** | **0.183** | **3.466×** |
| Six voices, full mixer, chorus II/noise 1.0, resonance 0.70 | **0.753** | **0.284** | **2.655×** |

Every Step-8 timing median absolute deviation is below 1.2%. Step 9 later
recorded a separate seven-run observation: 4×/1× was 0.578/0.167 idle,
0.547/0.178 plain, 0.719/0.205 resonant and 0.830/0.315 full-mixer Chorus II.
Its final 0.682667-second window used median thread-CPU times
566.779/215.019 ms (MAD 1.735/3.640 ms). Those values remain dated history but
were not collected as a valid pair with Step 10. The completed comparison instead
uses three alternating seven-repetition audits on the same declared platform.
The values below are authoritative median-of-run-medians:

| Scenario | Step 9 → Step 10, 4× | Change | Step 9 → Step 10, 1× | Change |
| --- | ---: | ---: | ---: | ---: |
| Idle, six physical cards behind closed VCAs | 0.546 → **0.677×** | +24.0% | 0.152 → **0.170×** | +11.8% |
| Six voices, chorus off, cutoff 0.62, resonance 0.10 | 0.509 → **0.690×** | +35.6% | 0.159 → **0.178×** | +11.9% |
| Six voices, chorus off, cutoff 0.62, resonance 0.95 | 0.670 → **0.850×** | +26.9% | 0.192 → **0.228×** | +18.8% |
| Six voices, full mixer, chorus II/noise 1.0, resonance 0.70 | 0.786 → **0.736×** | −6.4% | 0.296 → **0.191×** | −35.5% |

Per-run median absolute deviations are small. All values are JUCE-free engine
thread-CPU observations, not plug-in, host or device totals. Every Step 10 row
is at or below 0.850× realtime, and the coarse `<5×` Release CPU runaway gate
is unchanged. The mixed direction across patches does not support a blanket
speedup or regression claim. The older wall-time table remains useful as dated
history, but its `steady_clock` values are not merged with this thread-CPU
series.

The current Step 10 → Step 11 comparison uses three alternating pairs, each
with seven repetitions, on the same declared 48 kHz/block-256 platform. The
values below are authoritative meta-medians of the three run medians; their
Step 10 baselines are contemporaneous pair controls, so deltas are evaluated
within this series rather than by mixing it with the dated table above:

| Scenario | Step 10 → Step 11, 4× | Change | Step 10 → Step 11, 1× | Change |
| --- | ---: | ---: | ---: | ---: |
| Idle, six physical cards behind closed VCAs | 0.653 → **0.666×** | +2.056% | 0.164 → **0.169×** | +3.072% |
| Six voices, chorus off, cutoff 0.62, resonance 0.10 | 0.670 → **0.682×** | +1.856% | 0.172 → **0.176×** | +2.483% |
| Six voices, chorus off, cutoff 0.62, resonance 0.95 | 0.823 → **0.832×** | +1.096% | 0.221 → **0.225×** | +1.807% |
| Six voices, full mixer, chorus II/noise 1.0, resonance 0.70 | 0.706 → **0.719×** | +1.755% | 0.184 → **0.188×** | +2.182% |

All eight Step 11 meta-medians remain below realtime; the worst is 0.832×,
and the hard Engine CPU gate passes. These are uninstrumented JUCE-free engine
thread-CPU observations on one machine, not plug-in/host totals or competitor
data. The consistent +1.096…+3.072% movement describes these fixtures and does
not authorize a rate split. The event-counter results below remain semantic
work attribution, not a substitute for these timings.

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
| Input support | 16,384 exact advances | 4,096 legacy TPT frames | exact input selected only at internal rates ≥176.4 kHz |
| Exact output support advances | 16,384 | 4,096 | output support is exact at every accepted rate |
| Exact support coordinate updates / MACs | 196,608 / 1,966,080 | 24,576 / 245,760 | six physical coordinates and 60 transition MACs per exact advance |
| Merson halfsteps | 98,304 | 24,576 | exactly two fixed halfsteps per VCF step |
| VCF RHS / feedback evaluations | 491,520 / 491,520 | 122,880 / 122,880 | exactly ten of each per VCF step |
| VCF stage / full-Early evaluations | 1,966,080 / 1,966,080 | 491,520 / 491,520 | exactly 40 of each per VCF step for the enabled-Character fixture |
| Causal-cubic input phases / recoveries | 344,064 / 0 | 86,016 / 0 | exactly seven phases and no normal-path recovery per VCF step |
| Fractional VCF peeks / latched-target commits | 70 / 70 | 70 / 70 | declared-schedule RES/VCF events; a peek neither consumes the cursor nor changes the official target |
| Exact event-aware card intervals / control nodes / extra maps | 120 / 840 / 720 | 120 / 840 / 720 | seven Merson nodes and six additional maps per affected card interval; ten RHS evaluations per VCF step are unchanged |
| Cutoff memo misses | 1,367 | 1,505 | nearly wall-time driven; 2.78% vs 12.25% of card updates |
| Converter pass starts / writes | 10 / 234 | 10 / 233 | model's anchored nominal 4.2 ms pass, one numerical-window boundary write apart |
| DCO cycle wraps / BBD shifts | 61 / 3,162 | 61 / 3,162 | oscillator and asynchronous BBD-clock events track elapsed time |
| Past + future BLEP correction visits | 12,648 | 12,651 | edge/event driven, not four times larger at 4× |
| Half-band calls / stereo nonzero-tap MACs | 6,144 / 602,112 | 0 / 0 | three decimations and 294 stereo MACs per 4× host frame |

The support counter algebra is also fenced across every shipping 4×/2×/1×
case. A two-line 2× window has 8,192 exact input and 8,192 exact output
advances, 98,304 coordinate updates and 983,040 MACs; a high-rate 1× window
has 4,096 of each advance, 49,152 coordinate updates and 491,520 MACs. A
low-rate 1× window instead has 4,096 legacy input frames and 4,096 exact output
advances, 24,576 coordinate updates and 245,760 MACs. These are semantic
operation counts, not cycle weights.

The 95-tap half-band has 49 exact nonzero coefficients, so each decimator call
performs 49 coefficient visits and 98 stereo MACs. The same regression covers
96 kHz 2×/1× and 192 kHz 1×: one 2× decimator call and 98 stereo nonzero-tap
MACs per host frame, none at 1×. It also proves the fixed Merson algebra above,
the exact fractional-event counts across every shipping selector case, the
expected 23-write boundary tolerance and raw-float identity between normal and
active-counter renders. Shipping-target preprocessing plus symbol/string scans
contain no audit instrumentation.

This is work attribution, not a selective-rate admission. Counters with
different semantics are not cycle weights; the whole-engine 4×/1× ratios do
not say what a future split architecture will save. More importantly, DCO,
VCF/VCA, scan/holds, BBD/support processing and their reconstruction boundary
currently share one loop. The common-host qualification below now supplies the
isolated DCO, RK64/RK128 VCF, closed-form BBD and analytic scan comparisons. The
DCO clears its isolated numerical boundary after Step 7; Step 8 improves the
BBD edge-input reconstruction, and Step 9 makes the declared four-case
low-drive BBD fixture pass at the two common-host 4× cells plus every actual HQ
selector path. Step 10 makes the common-host VCF pass at 4× and all six
standard actual-HQ VCF trajectories pass with the converter schedule and card
mechanisms active. Step 11 retains the common-host classifications while making
the event-aware dynamic VCF matrix pass all six standard HQ paths and both
8/768 kHz engine bounds. Lower common-host VCF/BBD factors still reject their
absolute gates. The matrix does not include the inter-domain reconstruction,
whole-engine or
latency proof a split architecture would require. Steps 8–11 add no future
audio-sample lookahead or latency. No domain split or rate-selector change is
admitted.

### Common-host numerical quality: no split admitted

**Step 6 baseline, 2026-08-09.** Each audited boundary or declared fixture was
rendered at 44.1 and 48 kHz through candidate internal factors 1×, 2× and 4×.
DCO harmonic coefficients and analytic controls define its wanted mask and gain
references; the VCF and BBD oracles cross an independent 4,097-tap `q=16`
Kaiser FIR with explicit delay compensation. Four times is a candidate under
test, never the reference truth. That first matrix rejected every cell and
changed no production DSP. Its DCO rows remain below as historical evidence.

**Step 7, 2026-08-09.** The DCO failure was numerical, not permission to move a
hardware/model law. The production reconstruction now stores the continuous
bandlimited step response, interpolates it, and subtracts the exact ideal step
at the query time; this avoids interpolating across the residual's unit jump.
The continuous slope residual remains stored directly. A circular naive delay
and symmetric correction both use `H=24`, followed at oversampled boundaries
by a 95-tap Kaiser half-band with `beta=7.857`. The 8 MHz timer division, range
clocking, 2.2 µs ramp reset, PWM geometry, sub divider, scan timing and restart
policy are unchanged.

**Step 8, 2026-08-09.** The production BBD now evaluates the signal presented
at each fractional clock edge with a causal four-point Lagrange interpolation
over the current and three preceding support-filter outputs, replacing the
linear edge-input estimate. It adds no lookahead or delay. Exact edge times and
clock phase, bucket count and index progression, transfer-law cadence, RNG
sequence, hardware and model constants, global rate selection and the reported
41-sample latency are unchanged. Corrected values written into and propagated
through the buckets intentionally differ. This is a numerical reconstruction
change, not a new MN3009 or JUNO-106 claim.

**Step 9, 2026-08-09.** Each input or output support side is now represented as
one six-state continuous physical network rather than a cascade of separately
warped TPT sections. A prepare-only 10×10 augmented matrix exponential builds
the discrete state and drive maps; processing advances the six physical
coordinates under the causal cubic determined by the current and three past
samples. There is no future sample, lookahead or added delay. Output support is
exact at every accepted rate. Input support uses the same exact combined
transition at internal rates ≥176.4 kHz and retains the reviewed Step-8 TPT
path below that boundary because exact cubic input drive there worsened SGA.
The muted and connected output loads select two prepared transitions over one
shared physical state. Clock phase, bucket count/index progression, transfer
law/cadence, RNG sequence, deterministic BBD and physical component anchors,
the global oversampling-factor selector and 41-sample latency do not move. A
guarded quality change deliberately resets support and grid histories under
zero gain; the
exact coordinates are physical voltages, but their preservation/reseeding has
not been qualified.

**Step 10, 2026-08-09.** The former float, path-averaged capped Newton
discretization is replaced by two fixed half-interval, five-stage Merson RK4
advances over the same continuous four-stage OTA equations. The four capacitor
voltages are the complete physical state and are stored in double precision.
The drive at seven unique abscissae is reconstructed causally from the current
endpoint and three predecessors; startup ramps linear → quadratic → cubic as
history becomes available. Cutoff, resonance and thermal headroom move linearly
between previous and current endpoints at those same abscissae. There is no
tolerance loop, runtime method selector, data-dependent retry, future sample,
lookahead or latency change. A quality-rate change preserves the capacitor
voltages, maps the shared control endpoint through the cap-aware grid ratio,
retains the most recent input endpoint and collapses older old-grid history
under the existing zero-gain transition. The card thermal scale is applied
before the product-grid cap, `omega*dt = 0.9*pi`.

This changes only the numerical realization of the already declared ODE. It
does not validate that ODE against hardware or change the resonance law,
input-drive calibration, cutoff law, physical hold timing or six-card evidence
class. OQ-09, OQ-10, OQ-15, OQ-16, OQ-18 and OQ-19 remain open; the 0.45-Fs
cap is product/numerical policy, not a JUNO-106 property.

The method was selected by measured bake-off, not claimed universally better.
A fixed three-substep classical RK4 candidate is about 0.55 dB better in the
44.1 kHz/1× hot high-`mu` transient, but that cell still rejects. Merson wins
the primary HQ dynamic matrix and the reviewed damping, Hopf/onset and
product-cap stability checks while removing a runtime selector.

**Step 11, 2026-08-09.** The dated Step 10 8 kHz rejection isolated a
converter-event quantisation error, so this step changes only how the existing
522 µs cutoff and shared-resonance holds are evaluated when one of their writes
falls in `(phase, phase + delta]`. Before that interval is solved,
a pure scheduler lookahead checks the next relevant event in
`(phase, phase + delta]`, including the resonance write at a pass wrap. It
latches the declared fractional position and the target computed at that event
without consuming the official 23-write cursor, changing the official target,
or advancing shared control state. At the next ordinary due-event poll, the
normal write path commits that latched target exactly once. Host automation
that arrives between peek and poll therefore cannot retroactively change the
event-time payload.

For the one card owning a cutoff event, or all six cards under a shared
resonance event, the engine evaluates the exact piecewise exponential RC hold
at the seven unique Merson nodes. Ordinary intervals keep Step 10's
endpoint-linear control path, and every other converter destination is
unchanged. The two fixed Merson halfsteps still perform exactly ten RHS and ten
feedback evaluations per VCF step; there is no solver split, extra
solver/capacitor state, lookahead audio sample, latency change, rate-selector
change, physical constant change or new hardware claim. The 23 logical
destinations still use the
normalized `ordinal/23` compatibility profile. Exact acquisition timing and
jitter remain OQ-07/OQ-08 rather than being inferred from this numerical fix.

The DCO grid uses the six octave-spaced notes MIDI 36/48/60/72/84/96, all three
ranges, saw and sub, and pulse at 5/50/95% duty. Its gate is ≤−70 dBc; the
reported value is the worst single off-mask FFT bin, not integrated alias
energy or a full-band floor. Independent analytic multiline controls validate
the mask. Every current cell passes alias, strict/top-band gain, finite/analysis,
normalized scan and DCO/PWM/SUB hold gates. Candidate fold families remain
informational, and the remaining worst-bin fold-family attribution is
**UNATTRIBUTED**; neither the pass nor the unchanged scan recurrences establish
hardware timing or close an open question.

| Audited boundary/fixture and metric | Host | 1× | 2× | 4× | Gate |
| --- | ---: | ---: | ---: | ---: | ---: |
| DCO Step 6 baseline, worst off-mask bin (dBc) | 44.1 kHz | −12.780565 | −36.596878 | −42.618000 | ≤−70 |
| DCO Step 6 baseline, worst off-mask bin (dBc) | 48 kHz | −16.741087 | −36.344575 | −41.452375 | ≤−70 |
| DCO Step 7 current, worst off-mask bin (dBc) | 44.1 kHz | **−83.476933** | **−82.436627** | **−82.432588** | ≤−70 |
| DCO Step 7 current, worst off-mask bin (dBc) | 48 kHz | **−84.879008** | **−92.976529** | **−92.978397** | ≤−70 |
| VCF dated Step 9 hot-saw NRMS (dB) | 44.1 kHz | −1.110 | −12.233 | −24.348 | ≤−40 |
| VCF dated Step 9 hot-saw NRMS (dB) | 48 kHz | −1.062 | −13.752 | −25.810 | ≤−40 |

The DCO result qualifies the numerical reconstruction of the declared model;
it is not a measurement of an original JUNO-106 or proof of the model laws.
The dated Step 9 rows preserve the rejection that motivated Step 10. The VCF
reference remains an explicit fixed-`q=16` RK128 solve whose convergence is
cross-checked against RK64; the nominal-Character-0 fixture applies production
resonance input compensation to the hot saw. With all gates unchanged, the
current matrix is:

| Host | Factor | Hot RK NRMS | Driven RK NRMS | Hot residual off-mask | Verdict |
| ---: | ---: | ---: | ---: | ---: | --- |
| 44.1 kHz | 1× | −12.538 dB | −145.593 dB | −44.602 dBc | **REJECT** |
| 44.1 kHz | 2× | −30.414 dB | −113.526 dB | −85.968 dBc | **REJECT** |
| 44.1 kHz | 4× | **−50.351 dB** | **−112.144 dB** | **−133.278 dBc** | **PASS** |
| 48 kHz | 1× | −14.269 dB | −144.364 dB | −48.081 dBc | **REJECT** |
| 48 kHz | 2× | −33.028 dB | −114.710 dB | −88.898 dBc | **REJECT** |
| 48 kHz | 4× | **−50.064 dB** | **−113.339 dB** | **−140.552 dBc** | **PASS** |

Thus both common hosts classify REJECT/REJECT/PASS. Self-oscillation pitch
error is 0.000/0.000/0.000 cents at 44.1 kHz and 0.001/0.001/0.001 cents at
48 kHz; level error is at most 0.010 dB. Passing 4× qualifies this numerical
fixture for the declared model. It does not make 4× reference truth, qualify a
lower factor or establish the equations against an original unit.

The deterministic BBD oracle evaluates the documented component filters, the
128-edge transfer and complete zero-order-hold image phasors across four cases
at a bounded linearized drive. It deliberately uses the same model anchors, so it is an
independent numerical implementation, not a new hardware measurement or truth.
BBD production history, exact edge/clock/bucket/transfer state and the causal
four-point result are fenced separately by the suite.
BGA is absolute error in wanted physical-image level; exactly one
non-fundamental wanted line clears the projection threshold per cell, so its
column is not an exhaustive image population. SGA is unwanted host-grid level
reported as the maximum over every unmasked 20 Hz–20 kHz Blackman–Harris FFT
bin. Only validated wanted physical lines whose source is ≤20 kHz may be masked.

The left side of each matrix cell preserves the dated Step 7 shipping result;
the bold right side is the dated Step 8 result.

| BBD metric, Step 7 before → dated Step 8 | Host | 1× | 2× | 4× | Gate |
| --- | ---: | ---: | ---: | ---: | ---: |
| Analytic NRMS (dB) | 44.1 kHz | −3.099 → **−3.602** | −14.910 → **−18.159** | −27.045 → **−30.394** | ≤−40 |
| Analytic NRMS (dB) | 48 kHz | −4.640 → **−5.768** | −16.426 → **−19.696** | −28.181 → **−31.847** | ≤−40 |
| Qualifying-line BGA absolute level error (dB) | 44.1 kHz | 34.389 → **34.362** | 4.088 → **4.080** | 0.867 → **0.865** | ≤0.75 |
| Qualifying-line BGA absolute level error (dB) | 48 kHz | 22.893 → **22.866** | 3.257 → **3.249** | 0.708 → **0.706** | ≤0.75 |
| 20 Hz–20 kHz unmasked SGA max (dBc) | 44.1 kHz | −24.854 → **−26.765** | −28.762 → **−41.304** | −47.635 → **−72.041** | <−60 |
| 20 Hz–20 kHz unmasked SGA max (dBc) | 48 kHz | −28.871 → **−30.364** | −31.329 → **−45.866** | −38.189 → **−65.597** | <−60 |

Step 8 still rejected every BBD cell. Step 9's current common-host matrix is:

| BBD metric, Step 9 current | Host | 1× | 2× | 4× | Gate |
| --- | ---: | ---: | ---: | ---: | ---: |
| Analytic NRMS (dB) | 44.1 kHz | −3.511 | −18.390 | **−53.442** | ≤−40 |
| Analytic NRMS (dB) | 48 kHz | −5.263 | −20.051 | **−56.101** | ≤−40 |
| Qualifying-line BGA absolute level error (dB) | 44.1 kHz | 4.764 | **0.070** | **0.011** | ≤0.75 |
| Qualifying-line BGA absolute level error (dB) | 48 kHz | 3.406 | **0.016** | **0.008** | ≤0.75 |
| 20 Hz–20 kHz unmasked SGA max (dBc) | 44.1 kHz | −26.934 | −41.304 | **−71.831** | <−60 |
| 20 Hz–20 kHz unmasked SGA max (dBc) | 48 kHz | −30.746 | −46.044 | **−65.381** | <−60 |

Both 4× cells now pass every absolute gate. The four lower-factor cells remain
overall **REJECT**: their exact output support materially improves NRMS/BGA,
but NRMS fails at 1×/2×, SGA fails there too, and 1× BGA remains outside its
gate. The independent oracle controls and state fences still pass.

The production-selector extension prevents the common-host grid from standing
in for shipping policy. It evaluates ten unique configurations using the
engine's actual factor selection and a constant physical case family:

| Shipping path | Analytic NRMS | BGA error | SGA | Admission |
| --- | ---: | ---: | ---: | --- |
| 44.1 kHz HQ, 4× | −53.442 dB | 0.011 dB | −71.831 dBc | **absolute PASS** |
| 48 kHz HQ, 4× | −56.101 dB | 0.008 dB | −65.381 dBc | **absolute PASS** |
| 88.2 kHz HQ, 2× | −50.700 dB | 0.011 dB | −71.832 dBc | **absolute PASS** |
| 96 kHz HQ, 2× | −51.863 dB | 0.008 dB | −65.382 dBc | **absolute PASS** |
| 176.4 kHz HQ, 1× | −53.481 dB | 0.011 dB | −71.832 dBc | **absolute PASS** |
| 192 kHz HQ, 1× | −56.079 dB | 0.008 dB | −65.381 dBc | **absolute PASS** |
| 44.1 kHz HQ-off, 1× | −3.511 dB | 4.764 dB | −26.934 dBc | Step-8 nonregression PASS |
| 48 kHz HQ-off, 1× | −5.263 dB | 3.406 dB | −30.746 dBc | Step-8 nonregression PASS |
| 88.2 kHz HQ-off, 1× | −18.390 dB | 0.071 dB | −41.304 dBc | Step-8 nonregression PASS |
| 96 kHz HQ-off, 1× | −20.051 dB | 0.016 dB | −46.044 dBc | Step-8 nonregression PASS |

The HQ-off allowances were declared before inspection: no more than +0.75 dB
NRMS, +0.25 dB BGA error or +1.5 dB SGA relative to the frozen Step-8 path.
They are compatibility/nonregression gates, not a relabeling of absolute
failures as numerical fidelity. Thus all six actual HQ paths pass the declared
four-case low-drive deterministic-line fixture and all four HQ-off paths avoid
regression, but
no physical BBD law, component value or open hardware question closes. Step 7
changes the production DCO reconstruction and fixed latency; Steps 8 and 9
change only causal BBD numerical realization and do not change latency. No
split follows: inter-domain reconstruction, whole-engine equivalence and
latency qualification remain mandatory.

The dated focused Step 10 integrator contract evaluates the full Early effect and
endpoint trajectories against RK96. Its primary comparison is −162.551 dB /
4.21471e-8 V; an alternating-control trajectory reads −95.2005 dB; and every
cold/warm product-cap tail is exactly zero for all six actual cards at Unit
Character/calibration 2. The Step 10 dynamic VCF audit then uses the engine's
exact 23-write order, analytic 522 µs holds and every actual card profile.
Nineteen physical takes per rate
family cover 24 logical profiles because exact Character-0 collapses are
reused. Independent RK64/RK128 convergence is at or below −150.9 dB, with no
recovery, schedule or count mismatch:

| Actual HQ selector cell | Production vs independent oracle | Admission |
| --- | ---: | --- |
| 44.1 kHz / 4× | −48.585 dB | **PASS** |
| 48 kHz / 4× | −48.724 dB | **PASS** |
| 88.2 kHz / 2× | −48.557 dB | **PASS** |
| 96 kHz / 2× | −48.514 dB | **PASS** |
| 176.4 kHz / 1× | −48.324 dB | **PASS** |
| 192 kHz / 1× | −48.293 dB | **PASS** |

All six standard HQ paths clear the predeclared −40 dB gate with the declared
converter schedule and card mechanisms active. The engine-bound extension is
not hidden: 768 kHz/1× passes at −61.360 dB, while 8 kHz/4× is an expected
**REJECT** at −33.245 dB despite −139.820 dB oracle convergence. Its maximum
converter-event snap is 30.978 µs; steady and continuously moving
cutoff/feedback/headroom diagnostics read −136.916 and −60.546 dB. The result
became the dated motivation for Step 11's fractional event-aware hold
evaluation, not a gate relaxation.

The current Step 11 audit keeps the same 19 physical takes per rate family and
24 logical card/Character/thermal profiles, the exact logical write count and
order, finite-state and zero-recovery checks, and independent RK64/RK128
references. Its direct production-scheduler contract observes six card-cutoff
writes plus the pass-wrapped shared-resonance write: seven pure peeks and seven
single normal-path commits, with cursor/order intact and the event payload
unchanged by later automation. A separate shipping-boundary probe replays four
strongly curved intervals bit-exactly with the connected trajectory; replacing
that trajectory with `nullptr` changes every probed state and is deliberately
rejected. This makes the audit sensitive to wiring the feature into
`renderVoice`, not merely to a local cascade helper.

| Step 11 dynamic selector/bound cell | Production vs independent oracle | Admission |
| --- | ---: | --- |
| 8 kHz / 4× engine bound | −84.881 dB | **PASS** |
| 44.1 kHz / 4× | −114.226 dB | **PASS** |
| 48 kHz / 4× | −116.317 dB | **PASS** |
| 88.2 kHz / 2× | −112.717 dB | **PASS** |
| 96 kHz / 2× | −115.823 dB | **PASS** |
| 176.4 kHz / 1× | −112.406 dB | **PASS** |
| 192 kHz / 1× | −115.445 dB | **PASS** |
| 768 kHz / 1× engine bound | −119.340 dB | **PASS** |

Every cell clears the same −40 dB NRMS gate. A late/ceil snap of the declared
events reproduces the dated −33.245 dB rejection; an early/floor snap rejects
at −32.007 dB. Both remain finite and structurally valid, so their failures
show that the admission depends on fractional event timing rather than a
relaxed threshold. No physical VCF law, control constant, global factor,
domain boundary, VCF ODE/capacitor-state dimension or 41-sample latency changes,
and no hardware timing is established. OQ-07 and OQ-08 remain open alongside the VCF
measurement debts listed above.

Step 11 regenerated the documentation audio from scratch. The maintained tree
contains ten fresh demos, a fresh 128-row factory report/CSV and ten common-gain
previews. Two demo runs and two full factory runs produced byte-identical
23-file trees; the factory report reconciles 128 finite unique rows at a
`−21.48 dBFS` median gated RMS, with 31 overload flags, zero near-silent
presets and nine outside `±18 dB` of the corpus median.
Historical comparison/fidelity/realism/state-of-the-art trees remain
recoverable from Step 9 and are intentionally not used for before/after claims
about the current engine.

### Dated Step 4 bounded-work VCF candidate: matrix rejection

**Added 2026-08-09.** The cited DAFx-21 port-Hamiltonian construction is a
promising bounded-work method for its own Korg35 and Moog equations, not a generic
replacement or an automatic stability proof for this IR3109 model. A
research-only one-step quasi-Newton candidate therefore kept the complete
then-shipping equations and was judged before any production switch existed.
It used exactly one system evaluation and two bidiagonal solves per sample
instead of up to eight then-shipping iterations; its Early-effect derivative
was frozen, so it was not described as an exact tangent.

The candidate was excellent under static parameters: worst small-signal gain
error was 0.01368 dB, its worst hot waveform error against explicit 64× RK4 was
−46.03 dB RMS versus the then-shipping path's −44.60 dB (both at `k=4.4`),
its normalized residual was 1.84e-5,
static stage-tolerance/headroom/Early-effect parity was −114.88 dB RMS, and the
existing hot fold-back probe read −66.41 dBc. It also preserved the retime and
oscillation/boundedness classes. The decisive reachable-control fixture covered
all six VCF-card ordinals on the normalized 23-write pass at both engine bounds
and the 44.1/48/88.2/96/176.4/192 kHz standard internal grids. It applied the
production holds, flooring, compensation and mapping/caps with Unit Character
zero, keeping the decisive motion case nominal while a separate static fixture
covered character mechanisms. It failed the ≤−40 dB parity gate everywhere:
the worst error was +21.31 dB RMS at 8 kHz/card 1 (`g=6.31375`), 44.1 kHz was
+18.50 dB, and even 192 kHz read +5.01 dB.
The separately retained +4.80 dB result came from `g=30` jumps, instantaneous
resonance and audio-rate thermal-headroom changes the plug-in could not
generate; it was an out-of-domain boundedness diagnostic, not automation
evidence.

The isolated matrix therefore **rejected that candidate**. The superseded
−97.56 dB result used only a 192 kHz fixture with invented cutoff/resonance
phases rather than the production ordinals. Fixed evaluation/solve counts are
not invariant CPU time, and a fast solver that fails the reachable signal
contract has no production value. No production DSP changed in that dated
step. Step 10's distinct Merson design later cleared the standard-HQ dynamic
matrix and replaced the Newton realization; this historical rejection remains
evidence against the one-step quasi-Newton shortcut, not against all
bounded-work methods.

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
   [open-questions queue](open-questions.md) (including OQ-01, -03, -04 and
   -20 for the chorus; OQ-05, -09, -15 and -19 remain other audible blockers).
   No vendor publishes such data either.
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
