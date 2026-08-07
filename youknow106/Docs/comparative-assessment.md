# Comparative fidelity assessment — YouKnow106 in the JUNO-106 emulation market

**Assessed 2026-08-07.** This document places the project's fidelity and
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
| DCO pitch generation | 8 MHz reference divided by 16-bit timer integers — the hardware's own pitch quantisation (A4\@8' = 440.044 Hz), range switch as clock change, scanned restart semantics | None models timer quantisation. KR-106 uses continuous phase with the firmware's 8.8 portamento rate law; junox has an integer-period *artifact* |
| Scanned control system | The p. 8 chart's exact 23-write order on a fractional 4.2 ms scheduler; per-destination hold networks, incl. the derived PWM (R117/C62, R116/C63) and SUB (R11/C1) slews; exact intra-pass timestamps declared open (OQ-08) rather than invented | KR-106: 4.2335 ms canonical tick plus per-slot DAC phase offsets from firmware cycle counting — it *claims* the timestamps this project deliberately leaves open; its offsets corroborate our provisional ~125 µs figure |
| Firmware envelope | Hash-scoped B-2 recurrence: 14-bit state, exact Q(v,c) three-partial multiply with the dropped low×low term, `E>>2` DAC truncation, byte-exact fixtures in the suite | KR-106: the same law at instruction level (cleanroom, j106roms lineage), cross-checked against MAME — parity on the digital law; no automated fixtures |
| Firmware LFO/delay/portamento | Hash-scoped exact laws with regression vectors | KR-106: ROM-table reconstructions verified against four hardware captures — parity in kind |
| VCF | IR3109 cascade behind the anchored 68 kΩ/560 Ω divider; jointly solved 4.83 Vpp/248.0 Hz self-oscillation endpoints; 992 Hz WIDTH anchor (tracking exactly 1.00); AS3109 700 µA control-current knee; R-2R carry INL on the converter write | KR-106: IR3109 TPT cascade with BA662 67:1 feedback physics, self-osc calibrated to a named unit, and a **4096-point measured DAC→Hz table from a real card** — a data asset this project matches only by derivation (within 30 cents of that table's shape after the knee fix) |
| VCF numerical quality | Path-average ADAA inside the Newton solve: hot-case folded lines below −60 dBc, fenced by `testCascadeDeniesTheFoldback`; linear response and limit cycle measured identical | KR-106: 2×/4× oversampling, no ADAA; no fence. No other project has either |
| Chorus | Bucket-clocked two-phase 256-stage MN3009 model: full-period hold confirmed by the datasheet OUT1/OUT2 solve, per-shift transfer loss on the EC-row anchor, derived LFO rates 0.5533/0.8983 Hz from the 106's own timing network (ratio 1.6234799), measured 1.4–6.4 ms sweep, BGA/SGA separation via bounded polyBLEP, no-compander hiss modelled | KR-106: deliberately *not* bucket-clocked (Hermite delay line with a written rationale), surrounded by measured side-effects (CTE gain modulation, leakage noise, clock-reset clicks); Chorus II rate marked "inherited, not re-verified". Hera has the field's only other bucket-level BBD — attached to an admittedly inaccurate alpha |
| HPF incl. bass boost | The boost shelf **derived from the p. 15 branch** (+10.50 dB DC, +1.41 dB HF, 59.41 Hz; within 0.016 dB of the exact 2z/2p solve), cut corners from designators; asymptotic C14 coupling states | KR-106: the same 2p2z transfer from designators, verified 0.55 dB RMS against its (unpublished) hardware noise sweep — convergent result, one lineage of hardware corroboration |
| Common VCA LEVEL | Derived dB-linear law from p. 8/p. 15/NEC (−16.32 + 0.1656·b dB), C7 9.08 ms settling | Not modelled as a distinct stage anywhere else surveyed |
| Output boundary | JIS-B volume law with internal 29.3 kΩ wiper loading, three coupling boundaries, −18 dBFS RMS declared policy | Generic gain staging everywhere else |
| Patch compatibility | Exact 18-byte tone format, .syx load/save/drag-drop, factory bank in hardware order, PC 0–127 | KR-106 has patch banks; none document byte-exact SysEx round-tripping |
| Deterministic verification | Six suites; every anchored claim above is fenced by a test that fails if the constant or law drifts | **None in the entire open field.** KR-106 ships manual offline analysis tools only |
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
| Self-oscillation endpoint | Service ADJUSTMENT: 4.8 Vpp sine at 248 Hz, every card | 4.83 Vpp at 248.0 Hz | Anchored, fenced |
| Key tracking | Service WIDTH anchor: C6 self-oscillates at 992 Hz = exactly two octaves over C4's 248 Hz | Exactly 4×, end-to-end through the converter path | Anchored, fenced |
| PWM duty windows | Service: 48–52% at PWM 50, 93–97% at PWM 10 | Inside both windows | Anchored, fenced |
| Chorus mode rates | A 106-chorus clone's scope readings 0.537/0.879 Hz; owner's manual "about 0.5/0.8" | Derived 0.5533/0.8983 Hz from the 106's own timing network (−3.0%/−2.2% vs the scope) | Derived, corroborated |
| Chorus sweep | 1.4–6.4 ms scoped on a designator-faithful build with genuine MN3009s, called identical against the measurer's real 106 | Shipped endpoints | Third-party measured |
| BBD transfer | MN3009 EC row: −3 dB at 12 kHz, 40 kHz clock (edition-verified 2026-08-07) | −3.000 dB at the anchor | Anchored, fenced with a cross-reading guard band |
| Digital laws | Hash-identified A-5/B-2 firmware behaviour | Byte-exact envelope/LFO/delay/portamento fixtures | ROM-resolved, fenced |
| Chorus noise delta | Measured 3.95 dB II−I on two chip populations | The rate-proportional candidate now ships behind `enableChorusRateNoise`, off by default: engaged it raises mode II by a measured 4.2225 dB against its 4.2089 dB prediction and leaves mode I's floor bit-identical | Open; the candidate is implemented and testable, not asserted |

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
verification, not the number — YouKnow106's worst-case fold-back is
deterministically fenced in its suite; no competitor fences theirs. Pitch is
essentially exact on both. The axes this material cannot measure — scan
stepping, BBD clock behaviour, alias floors, and every deterministically
fenced anchor — remain where the two projects genuinely differ, per the
comparison above; and on the cutoff law KR-106 ships the measured table
as a lookup, so that axis matches by construction on their side and by
derivation on this one. Full tables, clips and caveats live on the
published comparison page.

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
renders, process CPU time. Before and after the pass, same harness, back to
back:

| Scenario | Before | After |
|---|---|---|
| Idle — no key held, six cards running behind closed VCAs | 1.398× realtime | **0.852×** |
| Six voices, chorus off, resonance 0.10 | 1.105× | **0.699×** |
| Six voices, chorus II, saw+pulse+sub+noise, resonance 0.70 | 2.376× | **1.361×** |
| Six voices, chorus off, resonance 0.95 | 3.960× | **1.395×** |
| Six voices, chorus II, full mixer, HQ off | — | **0.449×** |
| Sixteen-voice extension, chorus II, full mixer | 7.7× | **4.10×** |

The figures are CPU seconds per second of audio, so under 1.0 is faster than
realtime. Read straight, and the "before" column is the honest part: **this
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
code, not a ranking against a competitor. The suite fences the *ratio* of
resonant to plain render cost rather than any wall-clock time, for that reason.

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
