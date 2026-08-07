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
project does.** Fidelity coverage is checkable and is a matter of record
above; perceptual superiority is not claimed.
