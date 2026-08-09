# YouKnow106 — best-in-class pass, 2026-08-07

**Work mode:** competitive evidence search plus measurement of the shipping
algorithms. **No hardware was measured.** This document refreshes the market
picture, states where this project is behind on axes its own
[comparative assessment](comparative-assessment.md) does not cover, and lists
the numbered steps taken in response. It follows the same discipline as the
[research contract](circuit-modelling-research.md): every comparative cell
cites what a vendor, reviewer or repository actually publishes, absence
findings are stated as absence, and nothing here promotes a hardware claim.

## 1. Refreshed competitive landscape

The 2026-08-07 comparative assessment surveyed documented *fidelity* coverage
and measured proximity to hardware. This pass re-ran the search against the
axes reviewers and owners actually separate products on — filter and
resonance character, chorus authenticity and noise floor, voice-to-voice
drift, envelope timing, assigner behaviour, **real-time cost**, and patch/MIDI
workflow — and looked specifically for axes the earlier document leaves out.

| Product | What it is | Published/reported position on the reviewer axes |
|---|---|---|
| **Softube Model 84** | Commercial, component-modelled | The forum-consensus authenticity leader. Modelled from a serviced, calibrated 1984 unit; press independently reports its recreated **cutoff stepping**, "exaggerated in self-oscillation" ([MusicRadar](https://www.musicradar.com/reviews/softube-model-84), [MusicTech](https://musictech.com/reviews/software-instruments/softube-model-84-review/)). Keeps the hardware's six voices and both POLY modes; adds velocity, aftertouch and an extended unison, plus a modular breakout of DCO/LPF/ENV/LFO/NOISE/VCA/chorus ([Synth Anatomy](https://synthanatomy.com/2021/04/softube-model-84-roland-juno-106-synthesizer-emulation-in-plugin-modular-form.html)) |
| **Roland Cloud JUNO-106** | Commercial, ACB | Official, documents modelled and adjustable chorus noise plus Circuit Mod/Condition per-voice variation; extends to 2–8 voices. Owners rank it mid-pack for accuracy. **Consistently reported as the CPU-heavy option** — around 10% of a core with no note sounding, and heavier than TAL's chain in direct comparison ([KVR t=518758](https://www.kvraudio.com/forum/viewtopic.php?t=518758&start=15), [KVR t=524111](https://www.kvraudio.com/forum/viewtopic.php?t=524111)) |
| **Cherry Audio DCO-106** | Commercial, low cost | Claims "exhaustive detail" without method. The only commercial product documenting live SysEx interchange with a real 106. Adds MPE, arpeggiator, chord memory, 16 voices, delay and reverb. **Advertises light CPU as a feature** — "a lightweight yet powerful engine that manages 16 voices of polyphony without overwhelming your system" ([Cherry Audio](https://cherryaudio.com/products/dco-106), [Synth Anatomy](https://synthanatomy.com/2020/10/cherry-audio-dco-106-plugin-new-product-juno-106-emulation-with-mpe-support.html)). Its chorus is repeatedly described as authentic **but without the hardware's noise** ([SOUND7](https://sound7.com/blogs/synth-talk/cherry-audio-dco-106-a-review)); owner opinion on overall accuracy is split irreconcilably |
| **TAL-U-NO-LX** | Commercial | A Juno-**60** model, not a 106. The only vendor publishing its own hardware-versus-plug-in audio comparisons. Reported as the light-CPU reference point against Roland's ACB |
| **Arturia Jun-6 V** | Commercial | A Juno-**6** model. Documents "BBD character" and a Chorus Noise control, three per-voice condition states, 36 voices. Reported "grainier" by hardware owners |
| **Ultramaster KR-106** | Open source, 2026 | The one benchmarkable competitor. Six voices, IR3109 TPT cascade at 2× oversampling, CD4013 sub model, 2SC945 avalanche noise model, a 1984 mode calibrated from firmware analysis and a 1982 mode from CV-path circuit analysis and hardware measurements ([Synth Anatomy](https://synthanatomy.com/2026/03/ultramaster-kr-106-an-open-source-roland-juno-106-synthesizer-emulation.html), [Bedroom Producers Blog](https://bedroomproducersblog.com/2026/03/19/ultramaster-kr-106/)). Ships measured in-tree artefacts this project derives instead |

Nothing in this sweep contradicts the comparative assessment's fidelity
conclusions, and no vendor has begun publishing an evidence chain. Two things
it *does* change:

1. **Real-time cost is a first-class buying axis in this market and the
   comparative assessment has no row for it.** Cherry advertises it; Roland is
   marked down for it; TAL is the light-CPU reference. The assessment compares
   mechanisms and measured proximity and is silent on what the plug-in costs to
   run.
2. **Chorus noise is a live differentiator in both directions.** Roland and
   Arturia expose a noise control; Cherry is praised by a 106 owner precisely
   for leaving the hiss out. This project models the hiss and can defeat it,
   which is the right side of that argument — but the one reported structural
   property of the hardware's chorus noise, the approximately 3.95 dB II−I
   level difference, was an unexplained, unmodelled lead at this plan's initial
   snapshot. Continuous-fidelity step 2 below supersedes that state with a
   direct relative calibration.

## 2. Gap analysis

### 2.1 What the existing documents already settle

The [comparative assessment](comparative-assessment.md) establishes that no
other 106 emulation, commercial or open, documents the scanned control system,
timer-quantised DCOs, firmware-exact modulation laws, bucket-clocked BBD chorus
and derived output network together, that none fences its claims with
deterministic tests, and that no 106 vendor publishes hardware evidence at all.
It also states plainly where the field is ahead: KR-106 ships measured
artefacts (a 4096-point DAC→Hz table, per-slot write offsets, ADSR CSVs) that
this project matches only by derivation. **None of that is redone here.**

The [open-questions queue](open-questions.md) owns every remaining *hardware*
unknown. Its P0 rows — OQ-01, OQ-03, OQ-05, OQ-09, OQ-15, OQ-19 — need raw
captures from an identified unit and cannot be closed from a desk. **No step
below claims to close one.** The recurring "upper-mid darkness" lead from the
factory-demo A/B pass is likewise left where the queue puts it: it bears on
OQ-15's open drive budget and OQ-18's knee, and refitting tone to close it
would be exactly the "choose a nicer-sounding constant" the queue forbids.

### 2.2 The gap neither document covers: what the engine costs to run

Measured on this pass, on one 2.8 GHz Xeon core, host rate 48 kHz, block 256,
HQ on (4× internal, 192 kHz), Unit Character 1.0, best of three three-second
renders. Historical provenance correction, 2026-08-09: the harness used
`std::chrono::steady_clock`, so these are elapsed wall seconds per second of
audio, not the “process CPU time” the original text called them:

| Scenario | Cost, × realtime |
|---|---|
| Idle — no key held, six cards running behind closed VCAs | **1.40** |
| Six voices, chorus off, resonance 0.10 | **1.11** |
| Six voices, chorus II, saw+pulse+sub+noise, resonance 0.70 | **2.38** |
| Six voices, chorus off, resonance 0.95 | **3.96** |

Read straight: **the engine did not run in real time on that machine in any
configuration, and it cost more with no key held than with six sounding.**
Against a field in which one competitor advertises 16 voices as lightweight and
another is marked down for costing 10% of a core at idle, this is the largest
measurable gap the project has, and it is entirely a product-engineering
problem — not one line of it is a hardware-evidence question, which is why the
open-questions queue correctly says nothing about it.

Where the cost was in that 2026-08-07 profile, measured rather than assumed:

- **`OtaCascade::process` is 65% of all engine time.** Its cost is almost
  entirely libm: at resonance 0.95 the solve evaluates roughly 69 `tanhf`,
  33 `expf` and 33 `log1pf` per filter step per voice, 1.15 M steps a second.
- **The solver runs its full 8-iteration cap in the settings that define the
  instrument.** Instrumented: 7.99 iterations per call at resonance 0.95 and
  6.22 with chorus II engaged, against 2.86 on a plain patch. The cause is not
  a hard problem — it is that the convergence test compares an absolute
  `1.0e-7` against a step on states that reach several volts, which single
  precision cannot resolve, so the loop can never satisfy it and always runs to
  the cap. Measured final step at the cap: mean 5.1e-5 V, worst 9.5e-4 V, on
  states averaging 1.7 V — i.e. the iteration is *already* at its round-off
  floor several iterations before it stops.
- **Loop-invariant work sits inside the hot loops.** The path-start
  `ln cosh` is recomputed on every Newton iteration although its argument is
  fixed for the whole call; the per-card chassis gradient (`exp`) and the shared
  warm-up fraction (`exp`) are recomputed per voice per internal sample from
  values that change slowly or not at all; the cutoff chain
  (`exp2` + two double `pow` + `tan`) is recomputed per voice per internal
  sample even when its inputs have settled to bit-identical values; and the
  comparator solver value-initialises a 2 KB event buffer on every voice on
  every internal sample although only its first few entries are ever read.

### 2.3 Historical second gap: one reported chorus property was not modelled

At this plan's initial snapshot, OQ-03 recorded an approximately 3.95 dB II−I
true-peak difference reported with two chip populations, and the 2026-08-07
pass named a candidate mechanism — noise proportional to modulation rate,
which mode II raises by exactly the instrument's own 1.6234799 ratio,
predicting 4.21 dB. The then-current per-line floor was mode-independent, so
the model had **no** delta at all. The calibrated capture that would confirm or
kill the mechanism is still owed, so the causal hypothesis could not ship on
by default; leaving it unimplemented also left the one reported property of
this circuit's noise unrepresented and untestable.

*Superseded 2026-08-09 by continuous-fidelity step 2:* the default now carries
the reported relative delta directly, while the rate law remains a separate
comparison profile. Absolute calibration, the true-peak-to-broadband
extrapolation and physical causality remain OQ-03.

## 3. Steps

Each step states what changes, which gap it closes, and how it is verified.
Steps are landed in order, one commit each, with the suite green at every
commit.

- [x] **1. Plan.** This document.

- [x] **2. Share one exponential between the cascade's `tanh` and its
  `ln cosh`, and hoist the path start out of the Newton loop.**
  *Closes:* the libm share of §2.2. `tanh(x)` and `ln cosh(x)` are both
  functions of `e = exp(-2|x|)`; computing `e` once serves both and removes a
  `tanhf` and a `log1pf` from every long-path evaluation, and the path-start
  `ln cosh` — constant across the whole call — is computed once per stage
  instead of once per iteration.
  *Verified by:* a new circuit-suite fixture asserting the two kernels agree
  with `std::tanh` and `std::log1p` to within one float ULP across the whole
  range the cascade drives them over, plus the service-anchor result now
  published as 4.8009 Vpp with 247.90 Hz predicted, the Runge-Kutta reference
  solve and the fold-back fence.

- [x] **3. Scale the cascade's convergence test to the volts it measures.**
  *Closes:* the wasted-iteration finding of §2.2. The absolute `1.0e-7` step
  test becomes `1.0e-6 * (1 + max|V|)`, which is where single precision's own
  round-off floor sits, so the loop stops when it has converged instead of
  when it runs out of iterations. The 8-iteration cap is unchanged, so the
  worst-case residual cannot get larger.
  *Verified by:* a new engine-suite fence on the same-run *ratio* of
  high-resonance to plain-patch wall time — less machine/load-dependent than an
  absolute duration, not machine-independent — plus a direct assertion that
  the converged step is inside the new bound.

- [x] **4. Stop recomputing settled per-card constants every internal sample.**
  *Closes:* the loop-invariant work of §2.2. The chassis gradient becomes a
  per-card table, the warm-up fraction is advanced once per internal sample
  beside the timer it derives from, the cutoff chain is recomputed only when its
  inputs actually change, and the comparator event buffer is no longer
  value-initialised.
  *Verified by:* output that is bit-identical to the previous commit — an FNV
  lock over four rendered scenarios matches exactly — plus
  `testQualityChangeRefreshesTheFilterCoefficient`, which holds the one
  invariant the memo introduces: a card whose holds have settled must not keep
  integrating on the old grid's pole after a quality change. **Recorded
  honestly:** that fixture passes on the pre-change engine too, because until
  the memo existed there was nothing for it to catch. A bit-identical change
  has no behaviour to fail on; the fence's job is to keep the invariant from
  here on, and the cost reduction is a measurement, not an assertion.

- [x] **5. Model the rate-proportional chorus-noise mechanism behind its own
  switch.** *Closes:* §2.3. A named, off-by-default
  `enableChorusRateNoise` (the identifier and default policy in this pass)
  scales each line's noise with its own modulation rate, so mode II sits
  `20·log10(1.6234799) = 4.21 dB` above mode I — the candidate the queue
  records against the reported ~3.95 dB.
  *Verified by:* an engine-suite test measuring the rendered II−I floor
  difference with the switch on, and asserting bit-identical output with it off.
  *Superseded 2026-08-09:* continuous-fidelity step 2 promotes the reported
  ~3.95 dB relative delta directly and retains the rate law only as the renamed
  `useChorusRateNoiseHypothesis` comparison.

- [x] **6. Publish the throughput baseline, and true the documents up.**
  *Closes:* the assessment's missing axis. The comparative assessment gains a
  real-time-cost row with the before/after figures and the competitors'
  published positions; the research contract records the two numerical changes
  as product mechanisms with no hardware claim; the open-questions queue records
  the solver measurements as model-internal evidence and attaches the chorus
  switch to OQ-03; the README states the new cost and the new switch.

## 4. Result

Same machine, same harness, the four scenarios of §2.2 rendered back to back
by the pre-pass and post-pass engines:

| Scenario | Before | After | Change |
|---|---|---|---|
| Idle, six cards behind closed VCAs | 1.398 | **0.852** | −39% |
| Six voices, chorus off, resonance 0.10 | 1.105 | **0.699** | −37% |
| Six voices, chorus II, full mixer, resonance 0.70 | 2.376 | **1.361** | −43% |
| Six voices, chorus off, resonance 0.95 | 3.960 | **1.395** | −65% |

Stated exactly after the provenance correction above: the figures are elapsed
wall seconds per second of audio, so under 1.0 completed faster than realtime
under that run's machine load. Two of the four scenarios crossed that line and two
did not. **Idle and ordinary six-voice playing now run in real time on this
machine and did not before; the chorus-engaged and near-oscillation cases are
still above it, at 1.36 and 1.40 against 2.38 and 3.96.** With the 4×
oversampling switched off the six-voice chorus patch costs 0.45. The
sixteen-voice extension — nearly three times the polyphony the hardware has —
costs 4.10, down from 7.7.

Two things that figure is not. It is not a claim about any other machine: this
is one contended 2.8 GHz core, and the honest use of it is the before/after
ratio on identical hardware, not an absolute product spec. And it is not a
comparison with a competitor, because no vendor publishes a measurement
condition — the CPU reports in §1 are user impressions, not benchmarks. The
solver-specific fence is correspondingly a same-run *ratio*; a separate
`testCpuBudget` keeps only a deliberately coarse absolute runaway ceiling.

Steps 2 and 3 change the last bits of the rendered samples; steps 4 and 5's
default position do not. The measured difference from the pre-pass engine is
−102 dB RMS relative to signal on a plain six-voice patch and −95 dB on a full
chorus patch. On a self-oscillating patch it is −20 dB, which is what a limit
cycle does when its phase is perturbed at all: its amplitude and frequency
stay within the 4.8 Vpp / 248 Hz service fences. The later amplitude-only solve
publishes the current result as 4.8009 Vpp / 247.90 Hz.

## 5. What this pass deliberately does not do

- **It closes no open question.** Every P0 row still needs the calibrated
  captures the queue specifies.
- **It does not touch a calibrated constant.** No level, corner, coefficient or
  law moves. Steps 2 and 3 change only how the same equations are solved, and
  their audible effect is measured, not asserted: on a plain six-voice patch and
  on a full chorus patch the difference from the pre-change engine is
  −102 dB and −95 dB RMS relative to signal. On a self-oscillating patch it is
  −20 dB, because a limit cycle's phase is not determined by its equations —
  amplitude and frequency stay on their service anchors, which is what the
  suite fences.
- **It does not chase the upper-mid darkness lead.** That is OQ-15/OQ-18 work
  and needs measurement, not a tone control.
- **It adds no demo take.** The renderer still writes ten.

---

## YouKnow106 — best-in-class pass, 2026-08-07 (second pass)

**Work mode:** competitive evidence search under a restricted egress, plus
direct measurement of the shipping DSP. **No hardware was measured.** The
previous pass above was a *cost* pass: it halved what the engine costs to run
and touched no calibrated constant. This one is the opposite. It is a *sound*
pass. Measurement of the shipping algorithms found two mechanisms that actively
mis-shape the output, three level or routing errors, and a set of structural
defects including one that makes a quality setting change the modelled physics.
Everything below is a product-engineering defect in this engine's own terms —
not one step claims to close an open question, and every step is verified
against a number the engine either already asserts or is measured to produce
today.

**Adversarial review, 2026-08-08.** The whole section was re-measured
independently against the same shipping code, with its own harnesses. All six
CTest suites were green before and after (169 s, 6/6). Six of the eight steps
survive; two were struck for reasons recorded in section 9, both because their
mechanism was contradicted by this project's own open-questions adjudications
rather than because their gap was unreal. Numbers that did not reproduce were
replaced with what was measured, and three verifications that could not have
failed before their own change were rewritten. Corrected figures are marked
**[re-measured 2026-08-08]** wherever they appear, and the surviving steps have
been renumbered 1–6; nothing was ticked at review time, because implementation
was the next phase. All six have since landed — section 11 records what they
achieved and where they departed from this text.

### 6. What changed in the field since the pass above

**Backfilled 2026-08-08 by a search-only pass.** The original text of this
subsection recorded that no competitive research had been possible: the
WebSearch budget was exhausted before that pass began, and the section-1 table
was carried forward explicitly unverified. Budget has since refreshed and the
sweep was run — 28 distinct queries across products, reviewers, forums,
shootouts and literature. Everything in this subsection down to "What this
sweep could not establish" is new material from that sweep, except the
Ultramaster/GitHub findings, the three recorded contradictions and the
literature absence, which are the earlier pass's own work and stand unchanged.

**Read every finding below as second-hand.** The egress policy still refuses
publisher domains: a direct read of `kvraudio.com` was attempted once this
session and returned `EGRESS_BLOCKED`, and the policy is an organisation denial
covering publisher hosts generally, not a transient failure. **No vendor page,
review, forum thread or changelog cited below was opened.** What is cited is
what a search engine quoted from those pages. That is weaker evidence than a
page read in three specific ways, all of which bite here: a search summary can
attribute a quote to the wrong page, can silently merge two pages, and carries
no publication date of its own. Where a date, version or price appears below it
is because the search result stated it; where the result stated two
incompatible values, both are printed and the item is listed as unresolved.
Section 1's table is therefore **corroborated in its substance and still
unverified at source** — a later pass with open egress owes the primary reads.

**All six carried-forward products still exist, and none of the section-1
positions was contradicted.** Nothing found retires a product,
reverses a reviewer verdict, or overturns a mechanism claim:

- **Softube Model 84** — actively sold, not discontinued. Its authenticity-leader
  reputation is intact and the *specific* section-1 claim reproduces: reviews
  still describe the recreated cutoff stepping as "exaggerated in
  self-oscillation" and the hardware's unison retrigger quirk as reproduced,
  and characterise the product as "a perfect facsimile of the original hardware
  with all the same quirks and linearities"
  ([MusicRadar](https://www.musicradar.com/reviews/softube-model-84),
  [MusicTech](https://musictech.com/reviews/software-instruments/softube-model-84-review/)).
  It has **no arpeggiator**, which reviewers split on — "a missed opportunity"
  against fidelity discipline. No engine change was found since launch; the only
  2025 Softube release note that names Model 84 concerns its *chorus* as an Amp
  Room / Flows module, not the synthesiser. Pricing is unresolved: price-history
  trackers report a $159/€159 regular with a $99/€99 introductory, a most-recent
  sale at $49 and a floor of $39, while a separate summary called $49 the
  standing price ([PluginDealz](https://plugindealz.com/price-history/Softube-Model-84-Polyphonic-Synth),
  [Softube](https://www.softube.com/us/plug-ins/model-84-polyphonic-synthesizer)).
- **Roland Cloud JUNO-106** — the CPU row is now backed by the vendor rather
  than by forum impression. Roland's own support article explains the Legendary
  series' cost in terms: ACB "does component-level modeling of original analog
  instruments" and "is very demanding"
  ([Roland support](https://support.roland.com/hc/en-us/articles/4412191951771-Roland-Cloud-Why-are-the-Legendary-series-synthesizers-so-intensive-on-my-CPU)).
  Roland *also* publishes a how-to article on removing the plug-in's "constant
  hissing sound", pointing users at the FX Tone control
  ([Roland support](https://support.roland.com/hc/en-us/articles/4407795403803-Juno-106-virtual-instrument-plug-in-how-to-remove-constant-hissing-sound))
  — first-party confirmation that chorus noise is a live differentiator in both
  directions, which is exactly what section 1 argued. **Correction to a trap
  this sweep nearly fell into:** the high-DPI UI, universal patch browsing and
  Circuit Mod that current pages advertise are the **v2.0 update of April
  2023**, not a recent one
  ([Synth Anatomy](https://synthanatomy.com/2023/04/roland-jupiter-4-jupiter-8-and-juno-106-v2-plugins-new-circuit-mod-ui-and-more.html)).
  No 2025–2026 engine change was established. Lifetime Key pricing is
  unresolved — $199, a $99 promotion said to end 2026-06-30, and a separate
  $290 regular / $149 bundle figure all appeared
  ([Roland](https://lp.roland.com/juno-106-lifetime-key)).
- **Cherry Audio DCO-106** — actively sold and still cheap (store $29, retailers
  reported $25–$39). Latest release found is **1.0.21**, which adds preset
  favourites and, materially, "fixed issues with high-frequency aliasing
  artifacts that could potentially show up when using DCO-106 at 44.1 kHz"
  ([Cherry Audio](https://cherryaudio.com/news/dco-106-1-0-21-and-ca2600-1-0-20-released),
  [version history](https://cherryaudio.com/products/dco-106/version-history)).
  A vendor shipping an aliasing fix is the field conceding aliasing as a defect
  class, not a flavour.
- **TAL-U-NO-LX** — alive but on maintenance. Reported releases: **5.1.1**
  (2025-09-17, more MPE options, Osmose compatibility), **5.1.2** (2025-11-03,
  MPE pitch on note release), **5.1.5** (2026-04-14, framework update and small
  UI changes) ([TAL](https://tal-software.com/products/tal-u-no-lx)). No engine
  change was established. One substantive fidelity note surfaced, attributed to
  TAL's own documentation: plug-in resonance "is often slightly higher than in
  the hardware, as the drop in resonance at higher frequencies hasn't been fully
  emulated". That is a *vendor* admitting a resonance-versus-cutoff dependence
  it does not model. It bears on this engine's cascade and on OQ-09/OQ-18, and
  it is exactly the kind of statement that must be read at source before it is
  used for anything.
- **Arturia Jun-6 V** — reported updated to build **1.6.6.6366** on 2025-10-09,
  and now also sold inside a new lower-priced "V Collection Intro" tier
  (reported €199/$199, ten instruments, Jun-6 V among them)
  ([Arturia](https://www.arturia.com/products/software-instruments/jun-6-v/resources)).
  Nothing establishes an audible change in that build.
- **Ultramaster KR-106** — press and search indexes still describe **v2.1.9,
  2026-03-19** as current ([kayrock.org](https://kayrock.org/kr106/)). The
  earlier pass read the project's own in-tree changelog and found **v2.5.13**.
  The repository is the better source and its finding stands; the discrepancy is
  a reminder that the search index lags this competitor by several months, so
  *absence of news about a competitor is not evidence that it has not moved.*

**Four entrants exist that section 1 has no row for. One of them matters.**

1. **AudioThing JUNE, renamed JULY at v1.2 — the one that matters.** A Juno-**60**
   model, released October 2025 and updated/renamed in 2026, explicitly
   **circuit-modelled and sample-free**: press describes modelling of the DCO,
   sub-oscillator and filter sections, and — the load-bearing part — that "the
   chorus circuit has been modelled in detail, including the MN3009 BBD chips
   with 256 stages each, the surrounding filters, and the LFOs that drive modes
   I, II, and I+II"
   ([KVR](https://www.kvraudio.com/news/audiothing-releases-june---juno-60-emulation-plugin-65002),
   [Synth Anatomy](https://synthanatomy.com/2026/03/audiothing-june-a-roland-juno-60-synthesizer.html)).
   That is a competitor **publishing a structural number about the BBD** — stage
   count, per chip, with the mode LFOs named. It is the closest thing found to
   another vendor putting a mechanism on the record, and it lands squarely on
   the axis step 4 works on. Two cautions: it is a 60, not a 106, so its bucket
   count and clocking are not automatically this instrument's; and "256 stages"
   is a datasheet property of the part, not a measurement of a unit, so it
   corroborates nothing this project has not already anchored.
2. **MNTRA QUASAR-106** (May 2026) — not a synth but a **Juno-106-derived filter
   + chorus + drive effect** (VST3/AU/AAX), intro $19 to 2026-06-16, regular $49
   ([KVR](https://www.kvraudio.com/news/mntra-releases-quasar-106---juno-inspired-filter-chorus-and-drive-plugin-67067),
   [rekkerd](https://rekkerd.org/mntra-quasar-106-analog-color-filter-effect-plugin/),
   [MNTRA](https://www.mntra.io/product/quasar-106/)). It claims "true analog
   modelling — we model how physical components interact, warm up, and push back
   when driven", ships the three chorus modes plus a 12 dB slope "the original
   never had", and puts "vintage aging in one knob". Method claimed, **no
   evidence published**. Notable because it unbundles precisely the two things
   2025–2026 buyers say separate 106 emulations — filter and chorus — and sells
   them alone.
3. **ORU Audio HOLO-106** — a Juno-106 chorus plug-in offering a choice between
   a clean digital chorus and "the subtly characterful analog sound of a modeled
   MN3009 BBD chip", described as "the result of months studying recordings from
   a genuine 1984 Roland Juno-106" ([ORU Audio](https://oruaudio.gumroad.com/l/holo106)).
   No release date, version or price was established. Same pattern as QUASAR-106:
   the chorus sold on its own, method asserted, evidence not shown.
4. **Fingerlab JYNTH** (announced end of May 2026, released 2026-06-04, a "2.0"
   update in June) — press calls it a Juno-106 emulation, iOS/macOS only,
   standalone plus AUv3, free download with a reported $6.99/€7.99 unlock
   ([Synth Anatomy](https://synthanatomy.com/2026/05/fingerlab-jynth-a-physics-controlled-juno-106-synth-for-ios-and-macos.html),
   [Synthtopia](https://www.synthtopia.com/content/2026/06/20/fingerlabs-jynth-brings-analog-style-synthesis-physics-engine-control-to-ios-macos/)).
   Its selling point is a **physics engine** driving up to eight modulators, not
   fidelity. **No modelling method is published at all** — nothing found says
   whether it is modelled, sampled or neither. It is a competitor for attention,
   not on this project's axes.

   *Not new, listed so a later pass does not re-discover them:* TubeOhm's
   106-Emulation (Windows VST, 2016, €44.95) is the other product besides Cherry
   that does SysEx interchange with real hardware; TAL-Pha (Feb 2024) is an
   Alpha Juno II and not in this field.

**What 2025–2026 reviewers and owners say separates the best — and it is not
what section 1's axes list leads with.** The sweep found the ranking argument
being fought almost entirely on **the chorus first and the filter second**, with
the split section 1 recorded still unresolved and still irreconcilable:

- Pro-Softube/Roland: an owner with hardware reports Roland's JUNO-106 and
  Model 84 "can sound exactly the same and sound just like the hardware", while
  "no amount of tweaking will get DCO-106 to sound even remotely the same" —
  "the chorus being wrong". Another puts Arturia's chorus as lacking "a touch of
  width/depth" and DCO-106 as "the least convincing of the bunch"
  ([KVR t=564111](https://www.kvraudio.com/forum/viewtopic.php?t=564111)).
- Against: a poster prefers KR-106 "because it has character", calling Roland
  "rather bland and typical digital VA"; TAL draws "it has a weird stereo thing
  going on, almost sounding like an artificial stereo widening plugin was
  attached to the output" and a filter that "sounds sterile"
  ([KVR t=628710](https://www.kvraudio.com/forum/viewtopic.php?t=628710)).
- Editorial roundups do not converge either: one names Roland's ACB "the only
  plugin that is indistinguishable from a real Juno 106"; buyer's guides put
  Model 84 at the top ([KVR roundup](https://www.kvraudio.com/best-juno-106-emulation),
  [Gearspace t=1400358](https://gearspace.com/threads/best-juno-106-emulation.1400358/)).

  **Conclusion for this project: there is no consensus leader to displace, and
  the axis buyers argue on is the chorus.** That is a strategically useful
  finding and it changes no step.

**The four things they complain about**, in the order the sweep found them:
(i) **chorus wrongness** — the dominant complaint, and the only one that decides
rankings; (ii) **CPU** — Model 84 is repeatedly recommended over Roland Cloud
specifically for efficiency, and Roland concedes the cause on its own support
site; (iii) **aliasing** — a March-2026 KVR poster reports "ridiculous aliasing"
in KR-106, and Cherry shipped a 44.1 kHz fix; (iv) **envelope timing and control
law** — the same thread reports that "both the Cherry Audio and Ultramaster's
attack and release times were WAY too long", and that Roland's was "the only one
that paid attention to making sure the presets were correct", with sliders
scaled differently between products ("halfway on one synth is 2/3rds on
another").

**No published listening test, measurement or shootout with a stated result was
found — and that absence was tested hard.** What exists is: a Sonic Academy
course, "Juno Shootout with Kirk Degiorgio", with per-plug-in tutorials for
DCO-106, Roland Cloud JUNO-106 and TAL-U-NO-LX, whose verdict sits behind the
paywall and whose page teases "Do you agree with Kirk's choice?"
([Sonic Academy](https://www.sonicacademy.com/courses/juno-shootout)); a
"Which JUNO Synth Sounds the Best?" video hosted by **Cherry Audio itself**,
covering TAL, DCO-106, two Roland Cloud instruments, Zenology and Model 84
([Cherry Audio](https://cherryaudio.com/videos/which-juno-synth-sounds-best));
and several YouTube A/B comparisons. **None is blind, none is level-matched,
none publishes data, and the one with the broadest coverage is published by an
interested party.** No DAFx or AES paper on Juno or BBD chorus from 2025–2026
surfaced; the standing BBD reference remains Holters & Parker, DAFx-18, which
the research contract already cites. **The field still publishes no
measurements.** Section 1's central absence finding therefore survives a fresh,
adversarial sweep — and this project's deterministic tests and in-tree derived
artefacts remain the only evidence chain in the category.

---

**The remainder of this subsection is the 2026-08-07 pass's own work and is
unchanged.** It was gathered under the same egress denial but from hosts that
*were* reachable — `github.com`, `api.github.com`, `raw.githubusercontent.com` —
so unlike everything above it rests on primary sources actually read. The one
benchmarkable competitor develops in the open there, and it had moved a long way
since section 1 read it. Nothing in the search sweep above contradicts any of it.

**Ultramaster KR-106 is not a fixed March-2026 snapshot.** Its in-tree
changelog shows it shipping through at least v2.5.13, and several entries land
on axes this instrument leaves voiced, open, or has no row for
([changelog](https://raw.githubusercontent.com/kayrockscreenprinting/ultramaster_kr106/main/docs/website/changelog.html),
verified this session):

- v2.5.13: "Per-voice DC blocker chain: post-DCO and post-VCF stages (1.59 Hz)
  before the VCA, plus master pre-HPF stage (0.35 Hz) — mirrors hardware
  AC-coupling cascade, eliminates duty-dependent DC excursions on extreme-PWM
  patches". YouKnow106 has no post-VCF, pre-VCA high-pass at all. Gap 6 below
  measures what that costs here. *[Closed 2026-08-08 by step 3: C59 now sits
  between the filter output and the VCA input at 4.82 Hz, against this
  competitor's 1.59 Hz for the same stage. The post-DCO stage it also names is
  this engine's own C56/C50 at 0.48 Hz, already modelled.]*
- v2.5.13: "Chorus: per-channel ClickRing low-frequency resonance modeled
  (Chamberlin SVF, ~30 Hz / Q=18) excited by every click event". That is
  OQ-20's territory, still open here; a competitor has now put a shape and a
  number on it, which gives OQ-20's capture protocol something to confirm or
  refute.
- v2.5.13: "floor noise upgraded to pink + 500 Hz shelf to match measured
  spectrum", on top of v2.5.0's separate "broadband, mains ripple
  (120/240/360 Hz), and BBD clock bleed" sources with independent controls.
  YouKnow106 models no always-on electronics floor (gap 7) and rejects mains
  ripple through a derivation that covers only the *cutoff-modulation* path,
  not the additive one.
- v2.5.7: "Variable VCF oversample (Off/2x/4x)"; v2.4.2: "Oscillator runs at
  base sample rate; VCF handles its own oversampling internally". YouKnow106's
  HQ is a binary switch that oversamples the whole engine, chorus and scanned
  control system included, at 4x.
- v2.5.12: "DAC phase tables rebased so slot 0 lands at tick rollover; cuts
  ~2 ms of latency off every tick-driven envelope/LFO update". Playing latency
  was an unrepresented axis when this comparison was written; continuous-work
  step 3 now gives it a deterministic 48 kHz model baseline and the comparative
  assessment its own row, without inferring the competitor's phase table or a
  physical JUNO timing profile.
  (Its "VCF write lands ~125 us before VCA per slot" independently corroborates
  this project's own provisional ~125 µs intra-pass figure, which is already
  recorded, so only the latency-origin question is new.)

**The competitor's real-time-cost statement is now much harder than a forum
impression.** KR-106's site lists "Raspberry Pi OS / arm64" among its system
requirements and "WebAudio" among its formats
([index](https://raw.githubusercontent.com/kayrockscreenprinting/ultramaster_kr106/main/docs/website/index.html)),
and a third party has ported it to the Ableton Move
([artofcircuitry/schwung-kr106](https://github.com/artofcircuitry/schwung-kr106)).
Section 4's after-column leaves the chorus-engaged and near-oscillation cases at
1.361x and 1.395x realtime on a 2.8 GHz core. The bar is no longer "users say
Roland's is heavy"; it is a circuit model of the same instrument running on a
Pi. **No step below addresses this** — see "considered and not planned".

**Three recorded contradictions, none of which moves a constant today.** All
three belong in the open-questions queue with their lineage stated, which is
where this pass will file them:

1. **240 pF versus 270 pF integrator capacitance.** The Open80017a
   dksynth-lineage reconstruction this project already cites for the resonance
   divider carries C4/C5/C6/C7 = 270 pF across all four filter stages
   ([netlist](https://raw.githubusercontent.com/ThomHPL/Open80017a/main/Simulations/dksynth_80017A.asc)),
   alongside the same 560 Ω and 68 kΩ values the model does use. The engine
   hard-codes `poleCapacitorFarads = 240.0e-12f` from the service circuit.
   That is a 12.5% first-order disagreement — about two semitones of cutoff
   scale — and it moves the derived "240 pF / 68 kΩ" 64 kHz upper knee to
   roughly 57 kHz. The 248 Hz self-oscillation anchor pins absolute cutoff
   either way, so nothing audible has to move; what the project's discipline
   requires is that the 240 pF figure stop being carried as unqualified
   "anchored" (OQ-18).
2. **40 kHz versus 64 kHz expo-converter ceiling.** KR-106 v2.3.1 places the
   IR3109 exponential converter's tanh saturation at a 40 kHz ceiling; this
   project places the top of the cutoff law at ~64 kHz. The two disagree by
   most of an octave on where the sweep stops opening, in the same direction as
   the 270 pF finding and in the same region as the unresolved upper-mid
   darkness lead (OQ-18).
3. **KR-106 zeroed its VCF input compensation.** v2.5.12: "VCF input
   compression disabled (kInputCompressAmount = 0)", reversing its own v2.4.12
   "VCF InputComp and output gain recalibrated from hardware recordings". The
   *direction* of this mechanism is not in question here — it is anchored on
   Roland's own drawing and is not reopened — but the only competitor that
   recalibrated it against hardware recordings subsequently zeroed it, which is
   evidence that the magnitude is contested by someone who looked at hardware.
   That belongs in OQ-09's row beside the shipping `0.2296` (OQ-09).

**Third-party measured captures now exist for two P0 rows.** KR-106 publishes
in-tree analyses of real filter-sweep and oscillator recordings — per-file
resonance-peak heights of 9.5 / 10.5 / 17.9 dB at three panel settings, and
per-waveform saw/square/sub/mixer peak and RMS levels
([analysis report](https://raw.githubusercontent.com/kayrockscreenprinting/ultramaster_kr106/main/docs/analysis/juno6/analysis/analysis_report.txt))
— which is exactly the quantity OQ-09 and OQ-15 leave voiced. Two cautions
before anything is promoted: the directory is named `juno6` while the report
header reads "Juno-106 Recording Analysis", and the quoted rolloff figures
(−7.5 dB/oct at resonance 0 for a four-pole) are clearly estimator-dependent.
The right action is a targeted read of the analysis scripts to establish unit
and estimator, then record the result against OQ-09/OQ-15 as third-party
measured. **No step below fits a constant to these numbers.**

**Historical literature absence (2026-08-07; resolved by continuous step 4).**
At the time of this sweep the research contract cited Zavalishin, Stilson &
Smith, Huovilainen, D'Angelo & Välimäki, Välimäki/Pekonen/Nam, Holters & Parker
and Gabrielli/D'Angelo/Squartini. It does not cite the *stateful* ADAA line —
Holters, "Antiderivative Antialiasing for Stateful Systems", DAFx-19
([paper](https://www.dafx.de/paper-archive/2019/DAFx2019_paper_4.pdf), not
re-fetchable this session; author's code at
[ADAA_Examples.jl](https://github.com/martinholters/ADAA_Examples.jl)) — nor
the non-iterative port-Hamiltonian VCF solvers of Danish, Bilbao & Ducceschi,
DAFx-21 ([DOI 10.23919/dafx51585.2021.9768301](https://doi.org/10.23919/dafx51585.2021.9768301)).
This engine applies the *memoryless* first-order ADAA formula (the divided
difference of `ln cosh`) to four nonlinearities embedded in a stateful,
feedback-coupled solve, and it caps a damped Newton iteration at 8 steps with
no stability proof. Both are recorded as citation gaps in the research
contract; neither becomes a step here, for the reasons under "considered and
not planned".

---

#### What this sweep could not establish

This list is the point of the subsection, not an apology at the end of it.
**Every item above that came from search is an unopened page**, and these
specific things stayed out of reach even so. None of them may be treated as
settled, quoted as if read, or used to move a constant.

- **No primary source was read.** `WebFetch` on `kvraudio.com` returned
  `EGRESS_BLOCKED`, and the policy covers publisher domains as a class. Every
  vendor page, review, changelog, price and forum quote above is a search
  engine's rendering of a page nobody in this session opened. Quotation marks
  above mark *what the search result attributed to the page*, not a verified
  transcription.
- **Prices are unresolved for two of the six products.** Model 84 returned
  $159 regular / $99 intro / $49 recent sale / $39 floor from a tracker, and
  $49 flat from a summary. Roland's JUNO-106 Lifetime Key returned $199, a $99
  promotion, and a $290 regular / $149 bundle. These are not reconcilable from
  search results and no price above should be quoted.
- **No 2025–2026 engine change was established for any commercial product.**
  Roland's Circuit Mod and UI work dates to April 2023. TAL's 5.1.x releases are
  MPE and framework work. Arturia's 1.6.6.6366 has no published changelog here.
  Softube's Model 84 synthesiser has no update found at all. **Absence of a
  found changelog is not absence of a change** — the KR-106 v2.1.9-versus-v2.5.13
  discrepancy above proves the index lags reality by months.
- **The two shootouts that exist have unread verdicts.** Kirk Degiorgio's choice
  is paywalled; the broadest video comparison is published by Cherry Audio, a
  participant. Neither result may be cited in either direction.
- **TAL's resonance-rolloff admission is second-hand and load-bearing.** The
  statement that plug-in resonance runs high because "the drop in resonance at
  higher frequencies hasn't been fully emulated" is the single most technically
  useful sentence the sweep found, and it is exactly the kind of claim that must
  be read at source before it touches OQ-09 or OQ-18. It is recorded as a lead,
  not as evidence.
- **Fingerlab JYNTH's method is unknown.** Nothing establishes whether it models,
  samples, or does neither. It is listed as an entrant, not as a competitor on
  fidelity.
- **QUASAR-106 and HOLO-106 publish method claims with no evidence.** "True
  analog modelling" and "a modeled MN3009 BBD chip" are marketing sentences.
  Neither vendor publishes a measurement, a schematic reference or a test. They
  do not change the finding that no 106 vendor publishes an evidence chain.
- **No listening test, null test or measurement suite comparing 106 emulations
  was found to exist.** This is stated as an absence after a deliberate search
  for one, in the same discipline the research contract requires — not as a
  claim that none exists anywhere.

#### What this research changed in the step list: nothing

The section-8 steps were measured adversarially on 2026-08-08 and two were
struck. **This research reopens none of them, and none of the six was
added, reordered or removed as a result of it.** Each pressure the field
evidence could plausibly have applied was checked against the bar and failed it:

- **Is any step chasing a property the category has abandoned?** No. The
  category has moved the other way. MNTRA sells "we model how physical
  components interact, warm up, and push back when driven"; AudioThing publishes
  BBD stage counts and names the mode LFOs; KR-106 publishes analysis reports.
  All six steps are mechanism-fidelity steps and every one sits on an axis the
  field is currently competing on.
- **Is there a capability every competitor has and reviewers treat as table
  stakes?** Checked explicitly, and no. MPE is in DCO-106 and TAL-U-NO-LX but
  not established for Model 84 — the product reviewers rank first on fidelity —
  and Model 84 has no arpeggiator either, which reviewers note and then rank it
  first anyway. Neither is table stakes for the position this instrument
  occupies.
- **Is any priority wrong given what buyers complain about?** The complaint
  order found is chorus, CPU, aliasing, envelope timing. **Step 4 is a chorus
  step and it is the only chorus step**, so the field evidence *raises*
  confidence in its placement rather than disturbing it; AudioThing publishing a
  BBD structural number is the field arriving at the same axis from the other
  side. CPU is already the subject of section 10's explicit statement that this
  pass does not address it and of section 9's deferred solver and oversampling
  work — the research corroborates that ordering and adds nothing that would
  make cost a *sound* step. Aliasing is already measured clean here (worst
  inharmonic line −67.3 dBc), which is why the ADAA item sits in section 9 as a
  2x/1x concern rather than a defect. Envelope timing is a live axis in the
  field and no defect in it was measured here.
- **One honest tension, recorded and not acted on.** Only one of the six steps
  touches the field's top complaint. That is by construction — section 10 says
  in terms that this pass fits nothing to a competitor and lists this engine's
  own measured defects — but a later pass that wants market response rather than
  defect repair should start from the chorus, and should start by reading the
  TAL resonance statement and the AudioThing BBD description at source.
- **Nothing here promotes a hardware claim or moves a constant.** The three
  recorded contradictions above stay recorded. The new entrants' method claims
  are unevidenced and stay unevidenced.

### 7. Where the engine actually stands

Measured this pass on the shipping code, with scratch programs under
`/tmp/.../scratchpad/youknow106-*.cpp`. All six CTest suites were green before
and after measurement; no repository file was modified during it. The gaps are
numbered and each names the file, the constant and the number.

> **Adversarial review, 2026-08-08 — every number below was re-measured
> independently.** The re-measurement used its own harnesses
> (`youknow106-probe.cpp`, `-probe2.cpp`, `-probe3.cpp`, `-probe4.cpp`,
> `-therm.cpp`, `-hpf.cpp`, `-bench.cpp` in the same scratchpad), linked
> against the same `libYouKnow106DSP.a`, and no repository file was modified
> by it either. Gaps 1, 2, 5 and 8 reproduce, several of them to the digit.
> Gap 6's *magnitude* does not: it was measured on the wrong signal and is
> some 30 dB smaller than stated, though the mechanism is real and shows up
> elsewhere. Gap 9's premise is contradicted by this project's own OQ-21
> adjudication, and its settling times are an order of magnitude out. Gap 3
> carries an internal contradiction with gap 7. Gap 8's per-rate table is
> wrong at 96 kHz. Every correction is inline below, marked
> **[re-measured 2026-08-08]**, and it is the corrected number that the
> steps in section 8 are now fenced against. Where a step's *verification*
> could not fail today, or could not isolate the effect it names, the step
> was rewritten or struck; two were struck outright and moved to section 9.

**Gap 1 — RESONANCE is a second, hidden CUTOFF slider.**
`Source/DSP/YouKnow106Engine.h:309`
(`VoicedResonanceCompatibilityProfile::frequencyTrimAmount = 0.098f`),
implemented at `Source/DSP/YouKnow106Engine.cpp:234-243` and applied
unconditionally in `vcfEffectiveCutoffHz`
(`Source/DSP/YouKnow106Engine.cpp:264-265`). The trim multiplies cutoff by
`1 + 0.098·min(k/4, 1.2)²`, a function of **loop gain**. The droop it exists to
cancel is a function of **limit-cycle amplitude** — the tanh compression of a
large self-oscillation — which is zero below the oscillation threshold. So
below threshold the entire correction is error. Evaluating the shipping law
directly: +8.8 cents at panel 0.30, **+32.2 at 0.50, +80.2 at 0.70, +116.2 at
0.80**, +156.8 at 0.89 and +203.3 at panel 1.00.

**[re-measured 2026-08-08]** The law reproduces to the digit — calling
`VoicedResonanceCompatibilityProfile::loopGain` and `frequencyTrim` directly
gives +0.00 / +0.70 / +3.32 / **+8.76 / +32.24 / +80.17 / +116.25** / +137.80 /
**+156.84** / +182.06 / **+203.27** cents at panel 0.00 / 0.10 / 0.20 / 0.30 /
0.50 / 0.70 / 0.80 / 0.85 / 0.89 / 0.95 / 1.00. One correction: the ordinary
0.2→0.8 gesture moves the corner **+112.93 cents**, not +107.6 — the trim at
panel 0.2 is +3.32 cents, not zero. The skirt extraction is *not* independent
corroboration and is dropped from the evidence: in a pure −24 dB/oct region the
level is proportional to `inputCompensation(k) · g⁴`, so recovering `g` from it
requires dividing out `1 + 0.2296 k` exactly — and `inputCompensationPerFeedback
= 0.2296` is itself voiced, with a reconstruction-derived 0.275 (20% higher) on
record in the header's own note at `Source/DSP/YouKnow106Engine.h:291-303`. A
20% error in that divisor moves the apparent pole by 79 cents, the same order as
the +108.9 cents being measured. The law evaluation above is exact and needs no
divisor, so it is what gap 1 now rests on. A real IR3109 four-pole with a
resonance return keeps its pole where the CV puts it. Audibility: obvious.

*[Closed 2026-08-08 by step 2. The correction is now the reciprocal of the
droop the cascade's own limit cycle imposes on itself, and is identically 1
below the oscillation threshold, so every cent figure in this gap now reads
0.00 — checked as a pure function across all 128 panel bytes and at the render
seam at three converter codes.]*

**Gap 2 — Solo Unison is a fixed comb filter, not a stack.**
`Source/DSP/YouKnow106Engine.cpp:626-642` (`converterEventPhases`,
`phases[ordinal] = ordinal/23`) with the six consecutive `Pitch` entries at
`:604-609` and the phase-zero restart in `restartDcoBandlimited` (`:1425`). All
six cards divide the same crystal, so in Unison they run at bit-identical
frequencies forever; their only separation is the instant each `Pitch` write
resets the ramp to zero, and the default `NormalizedServiceChart` profile
spaces the 23 writes **evenly** — 4.2 ms / 23 = **182.6 µs** apart. Six
equal-amplitude, equally-delayed copies is the textbook maximum-depth uniform
comb, with perfect nulls at multiples of 1/(6 × 182.6 µs) = 912.7 Hz at
absolute frequencies that do not move with the note and never drift, because
the offsets are digital. Measured at MIDI 48: **+15.19 dB** over one voice at
h1 but **−26.98 dB at h7**, **−19.27 dB at h14** and **−34.02 dB at h21** —
42 and 50 dB below the +15.56 dB coherent ideal. Broadband unison gain falls
from +14.89 dB at MIDI 36 to **+10.88 dB at MIDI 67** as more of the spectrum
enters the combed region.

**[re-measured 2026-08-08]** The comb and its depths reproduce essentially to
the digit (65536-point Blackman-Harris after 0.6 s, Unison/6 against Poly1/1,
CUTOFF wide open, Unit Character 1.0, 48 kHz HQ): h1 **+15.19 dB**, h7
**−26.98**, h14 **−19.26**, h21 **−34.02** at MIDI 48; broadband **+14.87 dB**
at MIDI 36 falling to **+10.88 dB** at MIDI 67. The mechanism is confirmed
directly on engine state: after one second of a Unison note the six cards read
`divider = 15289` and `periodSamples = 1467.744000` **identically**, with
`dco.phase` at 0.740783 / 0.716937 / 0.693091 / 0.669245 / 0.645399 / 0.621553 —
uniform steps of 0.0238460 of a period, which at 192 kHz is 35.0 samples, i.e.
**182.3 µs**, the ordinal grid exactly.

One claim does **not** reproduce and is withdrawn: the deepest null does *not*
sit at 2744–2747 Hz at every pitch. Sampling the ratio on the harmonic grid, the
deepest point lands at 915.69 Hz (MIDI 36), 1763.98 Hz (MIDI 43), 2747.07 Hz
(MIDI 48), 2743.97 Hz (MIDI 55) and 8231.90 Hz (MIDI 67). What is pitch-invariant
is the *comb*, whose nulls sit at multiples of 1/(6 × 182.3 µs) = 914 Hz
regardless of the note; which of those nulls a harmonic happens to land in is
what moves. That distinction matters, because the step's original criterion (b)
— "the deepest null's frequency moves by more than an octave across the five
pitches" — is satisfied *today* (915.69 Hz to 8231.90 Hz) and therefore could
never have failed before the change.

OQ-08 states in so many words "do not invent them by distributing ordinal events
evenly" and asks for "an audible/null-test assessment" of this profile. This is
that assessment, and it fails. Audibility: obvious. What OQ-08 also says, and
what struck step 3 below, is that the same task owns "what the 8253 write and
surrounding DCO circuitry actually force, and at which edge".

**Gap 3 — Chorus BBD hiss sits 14.4 dB above the MN3009's own
guaranteed-maximum noise spec.** `Source/DSP/YouKnow106Chorus.cpp:63`
(`independentLineRandomAmplitude = 1.0e-3f`), injected per clock edge at
`:566-569`. The same datasheet this model already treats as anchored for
bandwidth (−3 dB at 12 kHz) and distortion (0.3% at 0.78 Vrms, 2.5% at
1.5 Vrms) also specifies **noise 0.2 mVrms max** A-weighted and S/N 88 dB typ.
Driving `Chorus` directly at 192 kHz with silence and recovering the wet line
through the known gain chain gives **1.061 mVrms full band, 1.047 mVrms
A-weighted** at the BBD node in the model's own 2.6 V coordinate
[re-measured on implementation, 2026-08-08: **1.0611 mVrms full band** to the
digit, and **1.0488 mVrms A-weighted** — the A-weighted figure refines by
0.015 dB when both channels and both modes are averaged over a window long
enough to converge] — **+14.4 dB**
against the datasheet maximum and +24.9 dB against the ~59.7 µVrms implied by
S/N 88 dB at the 1.5 Vrms maximum input. Chorus is the instrument's signature
and this is the tell that survives every other fidelity gain. Audibility: clear.

**[re-measured 2026-08-08]** Three corrections.

1. The idle plug-in output floor is **−63.55 dBFS with Chorus I and −63.56 dBFS
   with Chorus II** — a never-played engine at 48 kHz HQ, VOLUME 0.80, VCA LEVEL
   0.80, CHORUS NOISE 1.0, RMS over 2–4 s — not −61.8 and −61.1. The document's
   own gap 7 gives a third figure (−60.4 dBFS, Chorus II) for the same quantity;
   that contradiction is resolved in favour of the measurement above, and both
   originals are withdrawn. The number is patch-dependent (it scales with VOLUME
   and VCA LEVEL), so any fixture asserting it must pin all three controls.
2. **Chorus I and II have the same floor, and structurally must.**
   `Chorus::settingsFor` (`Source/DSP/YouKnow106Chorus.cpp:208-229`) returns the
   same `lineGain` wet gain and the same sweep for both modes; only `rateHz`
   differs, and the one rate-proportional noise mechanism
   (`enableChorusRateNoise`) is off by default. The 0.016 dB I-vs-II difference
   measured is round-off. A verification asserting two different mode targets
   0.7 dB apart asserts something the code cannot produce.
   *Superseded 2026-08-09 by continuous-fidelity step 2:* this describes the
   pre-change model, not the real-unit evidence. The default output floors now
   differ by the usable same-chain 3.95 dB observation while the raw part
   calibration remains one baseline target.
3. The datasheet's own two noise figures disagree by 10.5 dB — 0.2 mVrms
   **max** A-weighted against the ~59.7 µVrms implied by S/N 88 dB **typ** at
   1.5 Vrms. Landing the model exactly on a guaranteed worst case is a choice
   inside that bracket, not a derivation from it.

*[Closed 2026-08-08 by step 4, inside that bracket and at its
guaranteed-maximum end. Those step-4 figures are retained as dated evidence.
Step 9 remeasures the changed exact output chain at 0.200059/0.200078 mVrms
(I/II) at 176.4 kHz and 0.200006/0.200020 at 192 kHz, after dividing out the
separate mode factor; the cross-HQ spread is below 0.004 dB.]*

**Gap 4 — LFO DELAY gates the DCO and VCF modulation but not PWM.**
`Source/DSP/YouKnow106Engine.cpp:3214-3226` (`performConverterWrite`,
`case ConverterDestination::Pwm` reads the raw `lfoValue_`) against `:3201-3237`
(`Pitch` and `Vcf` receive `lfoGated`); the same asymmetry in `updateSharedScan`
at `:3166-3169`. There is one firmware LFO and one delay envelope, so every
destination should see the same gated value. Held note, PWM SOURCE = LFO,
depth 1.0, RATE 0.75, DELAY 1.0 (a 4.36 s delay): at t = 0.05 s the delay
envelope reads `lfoDelayLevel_ = 0.0000` while `voice.pulseDuty` already spans
**0.5141..0.8637 (span 0.3496)**, against a span of 0.4129 at full release —
**85% of full modulation depth before the delay has released any at all**, and
full depth by t = 0.5 s. Over the same first second the vibrato and the filter
sweep are correctly held flat. This is an internal inconsistency in the
engine's own routing, not an evidence question. Audibility: clear.

**[re-measured 2026-08-08]** Confirmed: `lfoDelayLevel_ = 0.000000` at
t = 0.05 s and `voice.pulseDuty` spanning **0.5141..0.8639 (0.3498)** over the
same 83 ms window. One caution about the *instrument*, which the step below now
carries: LFO RATE 0.75 is **7.4405 Hz**, a 134.4 ms period, so an 83 ms window
is shorter than one LFO cycle and the span it reads is alignment-dependent. The
same 83 ms probe reads 0.4128 at t = 0.50 s, **0.2783 at t = 1.00 s** and 0.4078
at t = 6.00 s — all at full delay release. The "0.4129 at full release" figure is
therefore a lucky alignment, not a property, and any reference assertion must use
a window of at least one full LFO period. Note also that `updateSharedScan` is
called with the raw `lfoValue_` from two places
(`Source/DSP/YouKnow106Engine.cpp:2189` and `:2330`), both of which have to
change with it.

*[Closed 2026-08-08 by step 1; all four sites now carry the gated product, so
the duty span 50 ms into the note is 0.0000 at a fixed 0.7250 and the idle
prime lands on +3.300000 V. The 83 ms span this gap records, 0.3496, is
superseded by the step's own 0.3575 on the same window — a probing offset in a
figure that only ever argued for measuring over a full LFO period.]*

**Gap 5 — the velocity extension moves level only; the spectrum is
bit-identical across the whole dynamic range.**
`Source/DSP/YouKnow106Engine.cpp:3137-3143` (`updateVoiceVcaTarget`,
`velocityGain = 1 − velocityDepth·(1 − velocity)` multiplying the VCA control
and nothing else). The modelled hardware ignores velocity, so `velocityDepth =
0` is the faithful default and this is an extension — but it is the only
dynamics control the instrument has and it is a pure gain. At
`velocityDepth = 1.0`, CUTOFF 0.50, ENV depth 0.40, RESONANCE 0.30: RMS
**−34.05 / −25.64 / −19.48 dBFS** at velocity 0.2 / 0.5 / 1.0 — a 14.6 dB level
span — while the energy-weighted spectral centroid is identical at all three and
identical to the `velocityDepth = 0` renders.

**[re-measured 2026-08-08]** Reproduces, with the absolute numbers restated
against a *stated* estimator, because a spectral centroid has no meaning without
one. Estimator: 32768-point Blackman-Harris starting at t = 0.3 s, energy-weighted
over a 10 Hz grid from 20 Hz to 8 kHz. On that estimator the centroid reads
**240.75 Hz at velocity 0.2, 0.5 and 1.0, and at both velocityDepth 0.0 and
1.0 — identical to 0.01 Hz** (the original's 470.1 Hz is a different band or
grid; the invariance, not the value, is the finding). The level span at
velocityDepth 1.0 is **14.65 dB**: −41.58 / −33.11 / −26.93 dBFS at velocity
0.2 / 0.5 / 1.0. Audibility: clear (when the extension is used).

*[Closed 2026-08-08 by step 6; velocity now scales the ENV amount into the VCF
as well as the amplifier control, so the rendered corner moves 4062 cents
across the dynamic range. Re-measured once more on the pre-step-6 engine while
the revert proof was standing: on the same estimator the centroid reads
**238.2694 / 238.2690 / 238.2693 Hz** and the level span **14.53 dB**. The
invariance is unchanged — 0.0004 Hz across velocity and across velocityDepth —
but the absolute centroid has moved 2.5 Hz since this row was written, because
steps 2 and 3 landed between. The three levels read −38.83 / −30.46 /
−24.30 dBFS, 2.6-2.7 dB above the row's, on an RMS taken over the same
32768-sample window as the spectrum; the row does not state its own RMS
window.]*

**Gap 6 — there is no AC coupling between the filter output and the VCA input,
so the model manufactures DC and then multiplies it by the envelope.**
`Source/DSP/YouKnow106Engine.cpp` (`renderVoice`:
`output = filtered * voice.vca * voltsToSample`, with no high-pass between
them). *[Closed 2026-08-08 by step 3; the multiply now reads a `vcaInput` that
has been through C59.]* Roland draws VCF OUT pin 3 through **C59 1 µF/50 V NP** and the
VR27/R108 network to VCA IN pin 9 — already stated in this project's own README
ledger and research contract as hardware topology — and the service procedure
trims VR30/25/20/15/10/5 through 2.2 MΩ for **minimum thump**, which is
evidence Roland cared about this exact path. The model's per-voice coupling
sits only at the *module input* (`moduleCoupling`, C56/C50); every remaining
coupling stage is on the shared bus (`voiceBusCoupling_`,
`commonVcaInputCoupling_`, `outputCouplingLeft_/Right_`), all downstream of the
multiply. The topology claim is solid: C59 is recorded as **anchored** in the
research contract's voice-module VCA row and in `Docs/open-questions.md:1799-1809`,
and the VR30/R112 2.2 MΩ null exists precisely to hold that node at zero.

**[re-measured 2026-08-08] — the mechanism is real, the stated magnitude is
not.** The original figures were taken on the wrong signal. Reading
`voice.filter.voltage[3]` (which *is* the value `renderVoice` multiplies by
`voice.vca`) over the settled sustain window 1.5–3.0 s of a held MIDI 48 pulse
patch, ATTACK 0.45, CUTOFF 0.30, RESONANCE 0.75, 48 kHz HQ:

| Unit Character | panel PWM (duty) | DC mean | AC rms | DC/AC |
|---|---|---|---|---|
| 1.0 | 1.00 (0.95) | **+0.0308 V** | 0.596 V | **−25.8 dB** |
| 0.0 | 1.00 (0.95) | −0.0114 V | 0.676 V | −35.5 dB |
| 1.0 | 0.00 (0.50) | +0.0412 V | 1.108 V | −28.6 dB |
| 0.0 | 0.00 (0.50) | +0.00003 V | 1.113 V | −92.0 dB |
| 1.0 | 1.00 (0.95), RES 0.10 | +0.1292 V | 0.865 V | −16.5 dB |

The largest filter-output DC anywhere in the sweep is **+0.129 V**, and it is
never above the AC it accompanies — it is 16.5 to 92 dB *below* it. The claimed
"+187.7 mV against 60.4 mVrms, 9.9 dB above the signal" does not reproduce at
this patch and is withdrawn; a 60 mVrms AC reading at CUTOFF 0.30 / RESONANCE
0.75 can only come from a window dominated by the 0.479 s attack, where the
envelope-swept filter is still closed, not from the sustain the DC actually
rides on. Note in passing that the panel value 0.95 gives duty **0.9275**, not
0.95; duty 0.95 is panel 1.00.

The 2 Hz DFT bin does not reproduce either, and is the wrong instrument. Over a
6 s render with note-off at 3.0 s it reads **−73.6 dB** relative to broadband
RMS at duty 0.95 / Character 1.0 and −85.1 dB at duty 0.50 — not −29.2 and
−44.5 dB. A thump is a transient; a 2 Hz bin taken over six seconds averages it
away.

**What is real, and is what step 4 is now fenced against.** Low-passing the
rendered output at 20 Hz (four cascaded one-poles) over an 8 s note-on/note-off
cycle and taking the peak excursion relative to broadband RMS:

| Unit Character | duty | sub-20 Hz peak |
|---|---|---|
| 1.0 | 0.95 | **−18.06 dB** |
| 0.0 | 0.95 | −19.36 dB |
| 1.0 | 0.50 | **−43.33 dB** |
| 1.0 | 0.95, RES 0.10 | −14.30 dB |

**[re-measured again 2026-08-08, at implementation.]** The table above is
pre-step-2 and no longer reproduces: at RESONANCE 0.75 the loop gain is 2.9413,
where the deleted `frequencyTrimAmount` was raising the corner 5.30 %, so
removing it moved every reading taken through the filter. On today's tree the
same estimator over the same 8 s cycle reads **−42.49 dB at duty 0.5043** and
**−17.55 dB at duty 0.9436** at Character 1.0, and −58.23 / −19.14 dB at
Character 0.0. The rise is **24.9 dB**, not 25.3, and the conclusions below are
unchanged: the mechanism is duty-driven and it survives at Character 0.0. Step 3
carries the full before/after table.

That is a **25.3 dB rise in sub-audio excursion driven by PWM duty alone**, at
otherwise identical settings — the duty-dependent DC excursion KR-106 v2.5.13
names, arriving through the envelope as a thump rather than as a steady 2 Hz
tone. It is nearly unchanged between Unit Character 1.0 and 0.0, so it is a
property of the **nominal calibrated model** (the cascade rectifying a strongly
asymmetric pulse), not a Unit Character artefact — which strengthens the case
for the step, since the fix cannot be waved away as a tolerance mechanism.
Audibility: clear on extreme-PWM patches.

**Gap 7 — the instrument has no output noise floor at all; the entire residual
sits inside the per-voice VCA and is gated by the envelope.**
`Source/DSP/YouKnow106Engine.cpp:108` (`filterNoiseVolts = 2.0e-5f`, the only
dry-path noise source, injected per voice at `:3667-3691`) and `:3729-3730`
(`renderVoice` returns exactly `0.0f` for an inactive voice). The shared
summing amplifier IC1a, the switched HPF, the common µPC1252H2 and the
TA75558S output stage are all always powered and always in circuit on the
hardware and contribute nothing here. Measured with all oscillators off:
**−77.2 dBFS** with the key held, −65.6 dBFS 1 s after release, −83.8 dBFS at
2 s, −139.5 dBFS at 5 s, **−247.0 dBFS at 10 s**, and an engine that has never
played a note outputs **bit-exact 0.0** (peak 0.000e+00 over a full second).
With Chorus II the floor is constant — i.e. the only continuous noise the
instrument has is the over-loud chorus wet path of gap 3, and fixing gap 3 will
make this one *more* exposed. Audibility: subtle.

**[re-measured 2026-08-08]** The bit-exact zero is confirmed: chorus Off, never
played, one second of output at peak **0.000e+00**. The chorus-engaged floor is
**−63.55 dBFS (I) / −63.56 dBFS (II)**, correcting this gap's own −60.4 dBFS as
well as gap 3's −61.1 dBFS; see gap 3.

**Gap 8 — the modelled 900-second warm-up permanently stalls after 128 s, and
how far it gets depends on the quality setting.**
The former `thermalWarmupSeconds_ += static_cast<float>(inverseOversampledRate_)`
in the internal-sample loop (now `advanceThermalWarmup()`,
`Source/DSP/YouKnow106Engine.cpp:3639-3651`), consumed by
`dynamicOtaHeadroomVolts` (`:3653-3664`) and reported by
`getDisplayTemperatureC` (`Source/DSP/YouKnow106Engine.h:204-213`). At 192 kHz
the increment is 5.208e-6 s; once the float accumulator passes 128.0 its ULP is
1.526e-5, the increment rounds away entirely and the accumulator freezes
forever.

**[re-measured 2026-08-08]** The mechanism is confirmed exactly, by simulating
the float accumulation itself (`youknow106-therm.cpp`) rather than by rendering
hours of audio at every rate. The accumulator always freezes on a power-of-two
boundary, and *which* boundary depends on the internal rate:

| host rate / HQ | internal rate | increment | freezes at | after (audio) | temperature | OTA headroom |
|---|---|---|---|---|---|---|
| 48 kHz, HQ on | 192 kHz | 5.208e-6 | 128.0 | 118.64 s | **26.9886 C** (13.3%) | 6.4087 V |
| 48 kHz, HQ off | 48 kHz | 2.083e-5 | 512.0 | 474.54 s | **31.5077 C** (43.4%) | 6.5052 V |
| 44.1 kHz, HQ on | 176.4 kHz | 5.669e-6 | 128.0 | 126.82 s | 26.9886 C | 6.4087 V |
| ~~96 kHz, HQ on~~ | ~~384 kHz~~ | ~~2.604e-6~~ | ~~64.0~~ | ~~59.32 s~~ | ~~26.0296 C~~ | ~~6.3883 V~~ |
| ~~192 kHz, HQ on~~ | ~~768 kHz~~ | ~~1.302e-6~~ | ~~32.0~~ | ~~29.66 s~~ | ~~25.5240 C~~ | ~~6.3775 V~~ |
| **96 kHz, HQ on** | 192 kHz | 5.208e-6 | 128.0 | 118.64 s | **26.9886 C** (13.3%) | 6.4087 V |
| **192 kHz, HQ on** | 192 kHz | 5.208e-6 | 128.0 | 118.64 s | **26.9886 C** (13.3%) | 6.4087 V |

**[corrected 2026-08-08, while implementing step 5]** The two struck rows
assumed HQ multiplies the host rate by four. It does not:
`updateProcessingRate` (`Source/DSP/YouKnow106Engine.cpp:2180-2190`) picks the
factor that *reaches* `minimumHqProcessingRate`, so 96 kHz HQ on oversamples by
2 and 192 kHz HQ on by 1, and all three HQ-on rates run the engine at 192 kHz
internal. Measured by rendering silence on the unchanged engine, they freeze at
the identical 128.0000 s and read the identical 26.988573 C and 6.408747 V.
The 384 kHz and 768 kHz internal rates, and every number derived from them, are
withdrawn.

The document's "44.1 kHz and 96 kHz freeze at the same temperature" is
therefore **right**, and it is the *quality switch* that moves the modelled
physics, not the host rate: the spread across supported configurations is
**26.99 C to 31.51 C** where the law `25 + 15(1 - e^-t/900)` wants 34.4818 C at
900 s. `Source/DSP/YouKnow106Engine.h:812-818`, the comment on
`voiceEnergyFollowerSeconds`, explicitly forbids exactly this ("a quality
setting is not allowed to change what the supply does"). The OTA headroom the
accumulator drives stops between 6.4087 V and 6.5052 V instead of
**6.5687 V** -- a 2.5% error with HQ on and 1.0% with it off -- and the panel
thermometer reports the frozen value as fact. One implementation detail every
fixture must respect: `getDisplayTemperatureC()`
(`Source/DSP/YouKnow106Engine.h:204-213`) multiplies the 15 C rise by
`activeParameters_.calibration`, so the temperatures above are the Unit
Character 1.0 readings and a fixture at any other Character is asserting a
different law. Audibility: inaudible-but-structural, plus a 1.0-2.5% headroom
error that is real on hot patches.

*[Closed 2026-08-08 by step 5. The accumulator is a `double`, so the law runs
to completion: 26.988573 / 29.252031 / 34.481808 °C at 128 / 300 / 900 s and
6.568748 V of headroom, identically at 44.1, 48, 96 and 192 kHz and in both
quality settings, with the cross-configuration spread inside 0.01 °C.]*

**Gap 9 -- the four-position HPF keeps one shared first-order state and
reinterprets it at the switch.**
`Source/DSP/YouKnow106Engine.h:917-927` (`struct HighPass`, one `double state`)
driven by `Source/DSP/YouKnow106Engine.cpp:2291-2302` and `:4061-4063`. The
model has one state and swaps its coefficient and gains, so a state that was
tracking a 59.4 Hz pole is then integrated as if it were a 720.5 Hz pole's
charge.

**[re-measured 2026-08-08] -- the defect exists but is a tenth the size claimed,
and the original description of the hardware is contradicted by this project's
own adjudication.** Three findings, which together strike step 6.

1. **The hardware does not carry four continuously-driven, already-settled
   legs.** A later full-resolution schematic read corrects the earlier Tr3
   description: the C14/R39 audio node feeds TC4052BP YCOM pin 3 directly,
   while Tr3 controls the switch's INH pin 6; it is not an audio buffer. The
   Y0/Y1/Y2/Y3 taps feed the four 47 kOhm summing legs into IC4a's virtual
   ground, with Flat on R27 and Boost on R25. The multiplexer connects the
   common audio node to exactly one leg, and R23/R21 "1M x 2" discharge the
   deselected capacitor legs. A deselected C10/C11 is not tracking the input at
   225.8/720.5 Hz; its corrected Cut-state decay is 15.705/4.9209 ms.
   "Advance all four legs from the same input every sample" is therefore a
   *different* wrong model, not the circuit -- and the same OQ-21 entry names
   "the deselected-leg charge-memory question the 1 MOhms actually govern" as
   its own open transient ask.
2. **Position One and position Boost share a pole in the model, so 1 -> Boost
   has no shared-state defect at all.** `highPassCornerHz` returns
   **59.4083 Hz for both**, `voiceBusCouplingCornerHz(mode)` returns 0.8209 Hz
   for both, and `HighPass::process`'s state update depends only on the input
   and `g`, never on `shelfGain`/`highGain`. Driving two `HighPass` instances
   with the same signal and switching one at t = 1 s, the 1 -> Boost difference
   measures **-599.78 dB at 1 ms, 10 ms, 50 ms and 200 ms** -- exactly zero to
   float round-off. The original's -38.40 / -36.44 / -40.44 dB for 1 -> Boost
   was measuring downstream coupling history in the reference render, not the
   HPF state.
3. **The real residual settles in under a millisecond, not "several tens".**
   Same isolated harness, 192 kHz internal rate, a held 40 + 130.81 + 392.4 Hz
   tone: 1 -> 3 gives **-45.81 dB at 1 ms and -580 dB (converged) by 10 ms**;
   1 -> 2 gives **-20.15 dB at 1 ms, -131.14 dB at 10 ms, exactly zero by
   50 ms**. It could not be otherwise: the destination leg's own pole is
   225.8 Hz or 720.5 Hz, time constants of 0.70 ms and 0.22 ms. The engine-level
   numbers in the original (-25.87 / -12.78 / -29.78 dB out to 50 ms, and my own
   engine-level reruns at -31.7 / -55.5 / -49.6 dB, still -41.8 dB at 200 ms)
   are dominated by `voiceBusCoupling_`, whose corner **also** changes with the
   HPF mode (0.8209 Hz for Boost/One against 0.4823 Hz for Two/Three) and whose
   330 ms time constant no change to `struct HighPass` touches.

The switch step itself is large and physical: isolated, it measures 0.290622
(1 -> 2), 0.291814 (1 -> 3) and 0.710782 (1 -> Boost) against a steady-tone
maximum step of 0.005447, i.e. +34.5 dB and +42.3 dB. That click is what a
listener hears, and the proposed step preserves it by design. Audibility of the
part the step would actually remove -- a sub-millisecond residual at -20 to
-46 dB: **none**.

**Gap 10 — two circuit mechanisms that are enabled by default are numerically
inert.** `Source/DSP/YouKnow106Engine.cpp:3974-3980`
(`powerSupplyDroop_ = totalVoiceEnergy * 0.0015f`, consumed at `:3276-3277`
through `railToCutoffCountsPerVolt = 35.0f`) and `:4100-4110`
(`enableOpAmpSlewLimiting`, `maxStep = 653846.15 / oversampledRate_`). The
polyphonic rail-sag path measures, **[re-measured 2026-08-08]** on a held
6-voice chord at CUTOFF 0.62 after 2 s, **0.000522 V at one voice → 0.003356 V
at six**, i.e. **−0.0192 → −0.1233 cents of cutoff**: the entire
one-to-six-voice load change is **0.104 cents** (the original's 0.000470 →
0.002514 V and 0.075 cents were measured at some other patch or settling time
and are superseded). Either way it is three orders of magnitude below
audibility, so a six-note chord and a single note have the same filter. The
TA75558S slew
limit is worse: at 192 kHz the per-sample cap is 3.405 engine units while a
band-limited signal at this node cannot exceed about 1.6, so the clamp
**mathematically cannot engage** — switching it off changes a full chorus
render by **−171.48 dB relative to signal**, at or below single-precision
round-off. For scale, the same A/B for the VCF stage offsets is −45.93 dB, the
thermal gradient −64.16 dB, the VCF Early effect −71.71 dB and the whole Unit
Character 1 → 0 change −18.30 dB. Neither is a *wrong* model of the part; both
are advertised as circuit modelling and neither does anything. Audibility:
inaudible-but-structural.

#### Strengths this pass must not regress

These were measured and are at or above the commercial top tier. No step below
may trade them away.

- **Oscillator band-limiting.** Non-harmonic energy for a saw is −86.7 dB
  relative to harmonic energy at MIDI 60 and −76.1 dB at MIDI 108, worst single
  spur −98 dB; a window-free periodicity null (`x[i] − x[i+4587]`, exactly 25
  periods at MIDI 60) puts the whole non-periodic residual at −78.9 dB at Unit
  Character 1 and −100.0 dB at Character 0. Do not trade this for CPU.
- **No machine-gun problem.** Six successive identical note-ons through the
  same slot give |correlation| 0.26 / 0.36 / 0.86 / 0.26 / 0.39 against the
  first take. The struck DCO-restart step would have changed the rule this
  rests on; nothing in the surviving list touches it.
- **Earlier envelope-tick timing probe.** The four-press fixture showed that
  notes land on different parts of the 4.2 ms scan grid (t(90%) 8.62 / 10.90 /
  5.52 / 7.79 ms, with 1.83 ms between voices in one chord). It was not an
  end-to-end latency distribution. Continuous-work step 3 now separates the
  Pitch/envelope write, VoiceVca write, hold milestone, raw output threshold
  and host-reported DSP delay over the complete 48 kHz phase cycle.
- **Integer-recurrence envelopes and LFO.** Attack 4.2 ms – 3.28 s, decay(−20
  dB) 4.2 ms – 21.6 s, release 16.8 ms – 25.5 s, LFO 0.0363 – 29.76 Hz.
- **The chorus delay engine.** Impulse-timed wet peak sweeps 1.67–6.44 ms
  (mode I) and 1.53–6.47 ms (mode II) against the designed 1.4–6.4 ms. Only the
  noise amplitude is wrong; step 4 touches nothing else.
- **Stable self-oscillation.** 124.5 / 993.2 / 7152.8 Hz at CUTOFF 0.30 / 0.50
  / 0.70, clean threshold, no blow-up at extremes, resonant alias floor
  −64.9 dB A/H at saw + resonance 0.85. Step 2 rewrites the trim that sits
  under this and must keep the 4.8 Vpp / 248 Hz service anchors — noting
  that the fixture's published tolerance is ±0.48 Vpp and ±4 Hz (±28 cents),
  which is a fence and not a pin, so "keeps the anchors" is a necessary
  condition and not a sufficient one.

### 8. Steps

Ordered cheapest-and-most-audible first; nothing here depends on a later step.
Each is landed as one commit with the suite green.

**Hardened by adversarial review, 2026-08-08.** Six of the eight survive; two
(the DCO pitch-write restart and the four-legged HPF) were struck and moved to
section 9 with the measurements that killed them. Every verification below has
been checked for the one property that matters — **it must fail today** — and
three of the originals did not: step 3's null-frequency criterion already
passed, step 4's 2 Hz bin already passed by 34 dB, and step 8 cited a render
lock the suites do not contain. Those are replaced, not relaxed.

**Verification contracts re-audited in preflight, 2026-08-07, before any code
was written.** An external automated review of this pull request filed a finding
against step 2's rendered A/B; it was checked against the source, reproduced by
measurement, and **upheld**. Every other step was then put to the same question —
*if this step were implemented wrongly, or not at all, would the stated test
still pass?* — and **all six needed work**. Two contracts could not fail: step
1's routing change touched two call sites no assertion reached, and step 2's
centroid was confounded by two other resonance-dependent mechanisms. Two would
have failed a **correct** implementation: step 6 promised a centroid span the
mechanism cannot produce at the fixture it named, and step 3's duty-0.50 clause
was two-sided where the fix must move the number. Two rested on figures that do
not reproduce: step 5's host-rate claim, and step 3's DC and sub-audio levels.
Two asked for "a reference render" of an engine that will not exist after the
step lands. Each step below carries a *Contract corrected in preflight* note
recording what was wrong. Corrected figures are marked **[re-measured
2026-08-07 preflight]** or **[measured 2026-08-07 preflight]**. No engine change
was made in preflight; nothing was ticked then.

- [x] **1. Gate the PWM converter write with the LFO delay envelope.** The
  firmware computes one LFO value and one delay level, and the delay scales the
  value *before* the CPU distributes it — there is one attenuator in the
  instrument, not one per destination. The engine already holds that gated value
  (`converterPassLfoGated_ = lfoValue_ * lfoDelayLevel_`,
  `Source/DSP/YouKnow106Engine.cpp:3889`) and already hands it to `Pitch` and
  `Vcf`; only `ConverterDestination::Pwm` reads the raw `lfoValue_`. That case
  and the matching branch in `updateSharedScan` read `lfoGated` instead, and the
  two `updateSharedScan` call sites that pass the raw value
  (`Source/DSP/YouKnow106Engine.cpp:2189` and `:2330`) pass the gated product
  with them. No new constant, no new law, one value substituted for another the
  same function already holds. Closes gap 4. *Evidence status*: this is an
  internal-consistency correction, not an anchored hardware fact — no
  service-note or firmware source in tree states whether DELAY reaches PWM. What
  it does settle is that the engine currently contradicts *itself*, including
  its own panel LFO display, which is already `lfoValue_ * lfoDelayLevel_`.
  *Verified by*: a new engine-suite fixture holding a note with the saw off, the
  pulse on, PWM SOURCE = LFO, PWM depth 1.0, LFO RATE 0.75, LFO DELAY 1.0,
  CUTOFF 1.0 and Unit Character 0.0, probing `voice.pulseDuty` over a **200 ms**
  window (LFO RATE 0.75 is 7.4405 Hz, a 134.4 ms period, so anything shorter
  than one full cycle reads an alignment-dependent span — the 83 ms window reads
  0.3574 at t = 0.05 s but 0.4130 at t = 6.00 s at identical full depth). It
  asserts

  1. the duty span at t = 0.05 s is **below 0.005** — **0.4166 today**
     [re-measured 2026-08-07 preflight] — and that `lfoDelayLevel_` reads
     0.000000 at that sample point;
  2. that `lfoDelayLevel_` reads **exactly 1.0f** at t = 6.00 s and the span
     over the 200 ms window there is **0.4166 ± 1%**, unchanged by the routing
     because at delay level 1 the gated and raw values are the same float;
  3. that the **idle-priming path** carries the gated value too: with every
     voice released, the LFO advanced 30 ms so `lfoValue_` reads +1.000000 while
     `lfoDelayLevel_` is still 0.000000, a silent `setParameters` must prime
     `pwmVolts_` to the lfoGated = 0 value of **+3.300000 V**, where it primes
     **+0.600000 V today**.

  Reverting the routing restores the 0.4166 span at t = 0.05 s and fails (1).

  *Contract corrected in preflight, 2026-08-07.* Three faults. The 0.3498 span
  quoted against the 200 ms window was the 83 ms figure; measured over the
  window the step actually names it is 0.4166. The t = 6.00 s assertion asked
  for a comparison "against a reference render" of an engine that will not exist
  once the step lands; measurement shows `lfoDelayLevel_` is bit-exactly 1.0
  there, so the guard needs no reference at all and is stated against a recorded
  constant instead. Most seriously, the step's own text changes the two
  `updateSharedScan` call sites, and **nothing in the original contract reached
  them**: a note-holding fixture exercises `performConverterWrite` only, so an
  implementation that fixed the `ConverterDestination::Pwm` case and left both
  `updateSharedScan` sites raw passed every assertion. The measured gap on that
  path is the full PWM travel — +0.600000 V against +3.300000 V — which is the
  first attack after a silent patch change landing at the wrong duty. Assertion
  (3) is new and closes it.

  *What actually shipped, 2026-08-08.* As written, with no departure from the
  mechanism and no constant that came out anywhere else. Every figure the
  contract asserts reproduced to the digit on the unchanged engine: the 200 ms
  span at t = 0.05 s is **0.4166** with `lfoDelayLevel_` reading 0.000000; the
  same window at t = 6.00 s is 0.4166 with `lfoDelayLevel_` bit-exactly 1.0;
  30 ms of idle running puts `lfoValue_` at exactly +1.000000 with the delay
  still shut, and the silent `setParameters` there primes `pwmVolts_` to
  **+0.600000 V**.

  Four edits: the `ConverterDestination::Pwm` case and the `updateSharedScan`
  body read the gated value, and the two call sites
  (`Source/DSP/YouKnow106Engine.cpp:2189` and `:2330`) pass
  `lfoValue_ * lfoDelayLevel_`, which is the product `displayLfo_` already is.
  The `updateSharedScan` parameter is renamed `lfoRaw` to `lfoGated` so the
  signature states what it now receives. After the change the t = 0.05 s span
  is **0.0000** at a fixed duty of 0.7250, the t = 6.00 s span is unchanged at
  0.4166, and the idle prime lands on **+3.300000 V**, which is
  `pwmControlVolts(0.5)` — the middle of the comparator's travel rather than
  its far end.

  *Test:* `testModulationDelayGatesPulseWidthToo` in
  `Tests/YouKnow106EngineTests.cpp`, with two new `YouKnow106TestAccess` probes
  for `lfoValue_` and `lfoDelayLevel_`. *Proved to bite:* with all four edits
  reverted it fails (1) — `the pulse width swept 0.416600 while the delay
  envelope was still shut` — and (3) — `got 0.6, expected 3.3 +/- 1e-06`. (2)
  passes either way, as the contract predicts. The preflight's warning was
  worth the ink: reverting **only** the two `updateSharedScan` call sites, with
  the converter-write case left correct, still fails (3) at 0.6 V against
  3.3 V while (1) and (2) pass, so the original contract really would have
  signed that implementation off. Full suite green afterwards, 6/6.

  The one figure that moved is a parenthetical and is immaterial: the 83 ms
  illustration reads 0.3575 at t = 0.05 s and 0.4129 at t = 6.00 s here against
  the 0.3574 and 0.4130 recorded above — a one-sample probing offset, which
  argues for the 200 ms window either way.

- [x] **2. Drive the resonance frequency trim from the cascade's own
  limit-cycle amplitude instead of from loop gain.** The droop this trim
  cancels is the describing-function gain of the stage `tanh` at large drive:
  for a sinusoid of amplitude `A` into `tanh(v/H)`, the first-harmonic gain is
  `N(a) = (2/(π a)) ∫₀^π tanh(a sin θ) sin θ dθ` with `a = A/H`, which is the
  classical sinusoidal-input describing function and is `1 − a²/4 + O(a⁴)` for
  small `a` (the expansion checks out: `∫sin² = π/2`, `∫sin⁴ = 3π/8`). Each
  integrator's pole scales with the stage's effective transconductance, so the
  realised pole is `f_CV · N(a)` and the correction that restores it is `1/N(a)`
  — **identically 1 when `a → 0`**, which is every setting below the oscillation
  threshold. `frequencyTrim(feedback)` is replaced by `1/N(a)` evaluated on a
  running amplitude estimate through a small tabulated `N`.
  `frequencyTrimAmount = 0.098f` is deleted, not re-tuned. Closes gap 1 — the
  single most audible defect in the list, which is why the step survives review
  despite carrying the three obligations below. **Risk: high**, and three of the
  original step's claims did not survive:

  1. **`maximumFeedback` must be re-solved with it.**
     `Source/DSP/YouKnow106Engine.h:270-283` states in terms that
     `maximumFeedback = 4.51f` and `frequencyTrimAmount = 0.098f` were "solved
     together" against the coupled 4.83 Vpp / 248.0 Hz anchors. Deleting one and
     leaving the other is not a smaller change, it is an unsolved one. The step
     owns re-solving the pair and republishing both.
  2. **The physically-honest evaluation under-delivers by a factor of three, and
     the step must say which pairing it used.** At the service anchor the limit
     cycle is 4.83 Vpp, i.e. 2.415 V peak at `voltage[3]`, against the stage
     headroom `2·V_t/stageAttenuation = 6.366 V`. That gives `a = 0.379`,
     `N = 1 − a²/4 = 0.964`, and `1/N` supplies only **+64 cents** where the
     shipping trim supplies **+203**. The pairing that does land near the anchor
     is `voltage[3]` referred to the *resonance return's* headroom
     (`2 × 0.026 × 67.7 = 3.520 V`, giving `a = 0.686` and +217 cents) — but that
     is the compression of a **different** nonlinearity from the one the
     derivation is about, and choosing it would be a fudge factor doing the real
     work. Note also that the four stage drives are wildly unequal at
     oscillation: with `k = 4.51` and `tanh(0.686) = 0.5955`, stage 1 alone sees
     a ~9.5 V feedback term against a 6.37 V headroom, so a single-node,
     single-amplitude description is a strong approximation of a
     four-different-drive system. The step must **report the node/headroom
     pairing it shipped** and, if it is a mismatched referral or if no pairing
     reproduces the anchor, file that as a finding against OQ-09 rather than
     absorb it into a scale factor.

     **[re-measured 2026-08-08, on implementation.]** Both halves of this are
     right about a *single-node* account and wrong about the mechanism. The
     shortfall is not a missing scale factor, it is the other three stages.
     A four-pole loop oscillating at its own corner carries `√2` more amplitude
     at every step back towards the input, so the four drives measured on the
     engine at the anchor are `a = 1.014 / 0.715 / 0.514 / 0.377` on the *stage*
     headroom — the fourth is the one this note evaluated, and it is the least
     driven of the four. Carrying all four, with the resonance return on its own
     headroom setting the amplitude, the derivation supplies **+202.4 cents**
     against the shipping fit's +203.3, with no mismatched referral anywhere.
     Two smaller corrections: `loopHeadroomVolts` is `2 × 0.026 × (100/1.5)` =
     **3.4667 V**, not 3.520, because the header keeps the unloaded divider form,
     so the fourth stage's return-referred drive is `a = 0.696`; and the ~9.5 V
     feedback term reproduces at **9.40 V**.
  3. **"Pinned by the anchor" overstates the anchor.**
     `testSelfOscillationMatchesTheServiceTrim`
     (`Tests/YouKnow106EngineTests.cpp:4125-4210`) fences 248 Hz to **±4 Hz**
     (±28 cents) and 4.8 Vpp to ±0.48. That is a coarse fence, not a pin; several
     pairings will pass it. The step may not claim the anchor selected the
     implementation.

  **Cost — the step's real-time impact is roughly twenty times what section 10
  claims.** Making the trim a function of a continuously moving amplitude
  follower defeats the exact-equality `filterG` memo at
  `Source/DSP/YouKnow106Engine.cpp:3290-3298`, whose own comment says the chain
  it guards "costs an exp2, two double pow and a tan, per card, per internal
  sample". Measured on this box: a six-voice chorus-off render costs 14.767 s
  for 20 s of audio (1.354x realtime), and re-running that chain at every
  per-voice internal sample in the same run costs an additional **2.716 s, i.e.
  +18.4%** — before the follower and the table lookup. The step must therefore
  keep the memo alive (quantise the amplitude estimate and recompute the chain
  only when it crosses a step, or fold `1/N` in after the memo) and republish
  `testCpuBudget`'s measured ratio. The `resonant/plain < 1.7` fence section 10
  names is the **wrong** fence to watch — both patches rise together and the
  ratio barely moves; `testCpuBudget`'s absolute ceiling is the one at risk.

  **[re-measured 2026-08-08, on implementation.]** The cost did not arrive,
  because the amplitude never became a running estimate. At a limit cycle the
  amplitude is not free: harmonic balance fixes it from the loop gain that
  sustains it, so the correction stays a pure function of `voice.feedback`,
  the memo's existing key, and nothing in the audio path changed. Measured on
  the same box with the `testCpuBudget` patch: **1.2455x realtime before,
  1.2543x after** (best of three, two warm passes), and the
  resonant-over-plain ratio 2.2345 before, 2.2376 after — both differences
  inside the run-to-run spread of this shared box. The one new cost is the
  2.4 ms solve that fills the table, which happens once and is warmed in
  `prepare()` so no audio callback pays for it; a warm call is 9 ns.

  *Verified by*: **(a)** a new circuit-suite pure-function assertion that
  `vcfEffectiveCutoffHz(counts, k)` returns the same frequency for
  `k = loopGain(0.00 / 0.30 / 0.50 / 0.70 / 0.80)` to within **±10 cents** at
  three cutoff codes. This replaces the original skirt extraction, which could
  not have resolved the effect: in a pure −24 dB/oct region the level is
  proportional to `inputCompensation(k) · g⁴`, so the extraction turns entirely
  on dividing out `1 + 0.2296 k` — and `0.2296` is voiced, with a 20%-higher
  reconstruction value recorded in the header's own note at
  `Source/DSP/YouKnow106Engine.h:291-303`; a 20% error there moves the apparent
  pole by 79 cents, the same order as the +116 cents being measured. The
  pure-function form needs no divisor and fails today by exactly +0.0 / +8.76 /
  +32.24 / +80.17 / +116.25 cents. **(b)** the existing self-oscillation fixture
  still asserting 4.83 Vpp at 248.0 Hz inside its published ±0.48 V / ±4 Hz.
  [The fixture's own targets are 4.8 Vpp and 248.0 Hz; 4.83 was what the
  superseded joint solve rendered. It now renders 4.80 — see below.]
  **(c)** an engine-suite assertion at the **render seam**, so that a law
  corrected in the pure function but never wired into audio still fails: with a
  note held, every source off, Unit Character 0.0 and CUTOFF fixed, the corner
  the render actually uses — `atan(voice.filterG) · internalRate / π`, the
  `filterG` memo at `Source/DSP/YouKnow106Engine.cpp:3290-3298` — must agree
  across RESONANCE **0.00 / 0.30 / 0.50 / 0.70 / 0.80** to within **±10 cents**,
  at CUTOFF codes 3840, 6272 and 11520. Today it spreads **+0.00 / +8.70 /
  +32.91 / +80.42 / +117.53 cents** at code 6272 and +0.00 / +8.55 / +32.33 /
  +78.96 / +115.36 at code 11520 [measured 2026-08-07 preflight]. With no signal
  in the cascade there is no droop to correct, so `1/N(a)` is 1 at every one of
  these settings and the five must coincide; this is the wart
  `Source/DSP/YouKnow106Engine.h:274-280` already states in words.
  **(d)** `testCpuBudget` re-measured and republished, not relaxed.

  *Contract corrected in preflight, 2026-08-07.* The external review's finding
  against the original (c) — a rendered A/B at RESONANCE 0.20 and 0.80 asserting
  the spectral centroid moves by less than 15% — holds, and measurement makes it
  worse than the finding claimed. The 112.93 cents of corner is a 6.74%
  frequency change, so the 15% bound was already slack; but the centroid of that
  A/B is not a reading of the corner at all, because RESONANCE also moves Q and
  `inputCompensation`. Measured on the shipping engine, the centroid between
  RESONANCE 0.20 and 0.80 moves **−0.12% at CUTOFF 0.30, +4.97% at CUTOFF 0.50
  and +113.96% at CUTOFF 0.70** — and the step did not state a CUTOFF. At the
  first two the assertion cannot fail; at the third it fails today *and* after a
  correct fix, because the 6.74% the trim contributes is buried under a 114%
  swing the fix does not touch. The replacement measures the corner itself at
  the seam the render consumes, where at fixed CUTOFF and Unit Character 0.0
  nothing but `frequencyTrim` couples RESONANCE to `filterG`.

  *What actually shipped, 2026-08-08.* The mechanism as written; the
  *implementation* of it is a closed-form solve rather than a running
  follower, for the reason under the cost note above, and that is the one
  departure from the step's letter. Every baseline figure the contract asserts
  reproduced to the digit on the unchanged engine before anything was touched:
  the pure law spreads +0.00 / +8.76 / +32.24 / +80.17 / +116.25 cents across
  panel 0.00 / 0.30 / 0.50 / 0.70 / 0.80, and the render seam spreads
  +0.00 / +8.70 / +32.91 / +80.42 / +117.53 at code 6272 and
  +0.00 / +8.55 / +32.33 / +78.96 / +115.36 at code 11520.

  **The pairing shipped, which point 2 obliges the step to report.** Both, on
  their own headrooms, with no referral across them. The four stage
  differential pairs compress on `otaHeadroomVolts = 2·V_t/stageAttenuation =
  6.3663 V` and set the *frequency*; the resonance return compresses on
  `loopHeadroomVolts = 2·V_t·(100/1.5) = 3.4667 V` and sets the *amplitude*.
  The four stage drives are solved rather than assumed, from the loop's own
  structure: at the oscillation point each pole contributes −45° and 1/√2, so
  the node amplitudes rise by √2 per stage walking back towards the input and
  the drive at stage *n* is `|V_n|·(D/N_n)/H`. That is a three-line harmonic
  balance —
  `Σ atan(D/N_n) = π` for the frequency, `k·N_fb·Π(1+(D/N_n)²)^(−1/2) = 1` for
  the amplitude — parameterised by amplitude so there is no outer root-find,
  and resampled onto a uniform grid in loop gain. `N` itself is tabulated: 512
  entries of `(2/πa)∫₀^π tanh(a sin t) sin t dt` by Simpson over the
  integrand's own quarter period, continued above `a = 8` by its own `4/(πa)`
  asymptote.

  **It reproduces the engine's own limit cycle, which is the evidence that the
  derivation is the right one.** At `k = 4.51` the balance predicts a droop of
  **0.88968** and a 4.849 Vpp limit cycle; the rendered cascade measures
  **0.88915** and 4.828 Vpp — 1.04 cents and 0.4% apart, with no fitted
  quantity anywhere in the prediction. The threshold falls out of the same
  balance at a loop gain of **exactly 4**, which is the profile's own
  `nominalOscillationFeedback`, so the correction is identically 1 up to panel
  byte 114 and first departs from it at byte 115 by 13.2 cents.

  **`maximumFeedback` was re-solved, as point 1 requires, and it moved:
  4.51 → 4.504.** With the correction derived there is one free constant and
  it answers to the amplitude anchor alone, so the 4.83 Vpp the joint solve
  had settled for is no longer a necessary compromise: the model now renders
  **4.8009 Vp-p**. The 248 Hz anchor is then a *prediction*, and it lands at
  **247.90 Hz** — 0.67 cents under, against a fence of ±4 Hz. Point 3 is
  respected in the strongest possible way: the anchor did not select the
  implementation, because it no longer selects anything. Swept across the
  whole oscillating range — the panel byte grid above travel 0.9, which is
  linear in `k` — the rendered frequency reads 248.05 / 248.01 / 248.01 /
  247.95 / 247.88 Hz at `k` = 4.309 / 4.349 / 4.390 / 4.470 / 4.510, inside
  ±0.81 cents everywhere, where before the change only the one fitted point
  was on the anchor. That sweep is what identifies `maximumFeedback` from the
  amplitude alone: 4.6417 Vpp at `k` = 4.470 and 4.8283 at 4.510 put 4.80 Vpp
  at **4.504**, which renders 4.8009 Vp-p at 247.90 Hz. The VCF WIDTH anchor
  is unmoved at 1.99749 octaves.

  One constant left the header — `frequencyTrimAmount = 0.098f`, deleted and
  not re-tuned — and `maximumFeedback` is republished at 4.504f. No constant
  was added — the derivation's inputs are `otaHeadroomVolts`,
  `loopHeadroomVolts` and `nominalOscillationFeedback`, all of which the
  profile already carried. `Docs/circuit-modelling-research.md`'s oscillation
  frequency-correction row and OQ-09's status in `Docs/open-questions.md` are
  updated: the wart OQ-09 recorded is closed, and it did not need the measured
  family OQ-09 asks for.

  **One finding, recorded and not acted on.** The correction is now exactly
  what makes the model's *oscillating* frequency agree with its own
  *small-signal* control law, and the evidence says both should read 248 Hz at
  code 6272 — the service self-oscillation trim, and OQ-18's measured
  code-to-frequency table taken at resonance 0. Those two agreeing is
  equivalent to the real card's oscillation not drooping where this cascade
  droops by 203 cents. Either the hardware's limit cycle really is that
  undrooped, in which case something upstream of the correction is too
  compressed — the 6.37 V stage headroom, or the `√2`-per-stage internal
  amplitudes it implies — or the two 248 Hz readings are not the same
  measurement. Nothing in tree settles it, no step here touches it, and the
  correction is right either way, because it is what reconciles the model with
  both anchors at once. It belongs in OQ-09's queue beside the shipping
  `inputCompensationPerFeedback`, and it is filed there.

  *Tests:* `testResonanceLeavesTheCornerAloneBelowOscillation` in
  `Tests/YouKnow106CircuitTests.cpp` for (a), and
  `testResonanceDoesNotMoveTheRenderedCorner` in
  `Tests/YouKnow106EngineTests.cpp` for (c). (b) is
  `testSelfOscillationMatchesTheServiceTrim`, unchanged and still inside its
  published ±0.48 V / ±4 Hz. The circuit test also sweeps all 128 panel bytes
  below the threshold rather than the contract's five points, and fences the
  correction's continuity across the threshold, since a table has a join a
  closed form does not. *Proved to bite:* with the fitted quadratic and
  `maximumFeedback = 4.51f` put back, the circuit suite fails 12 checks —
  `resonance panel 0.800000 moves the cutoff law at 6272.000000 counts (got
  116.24, expected 0 +/- 10)`, `the frequency correction is not identically
  one below the oscillation threshold: panel byte 114 lifts the corner by
  160.659227 cents`, and `the frequency correction steps discontinuously as
  the cascade starts to oscillate (got 161.931, expected 0 +/- 1)` — and the
  engine suite fails 9, `resonance 0.800000 moves the rendered corner at
  converter code 6272 (got 117.526, expected 0 +/- 10)` among them, which is
  the preflight's recorded +117.53 to the digit. Nothing else in either suite
  fails on the reverted build, which is what makes those 21 the new tests'
  own. Full CTest run after restoring the change: **6/6 passed, 214.79 s**.

- [x] **3. Put C59 between the filter output and the VCA input.** Roland draws
  module pin 3 VCF OUT → **C59 1 µF/50 V NP** → the VR27/R108 network → pin 9
  VCA IN, and adjusts VR30/25/20/15/10/5 through 2.2 MΩ for minimum thump
  (Service Notes pp. 18–19; recorded as **anchored** in the research contract's
  voice-module VCA row and at `Docs/open-questions.md:1799-1809`). `renderVoice`
  gains a per-voice first-order high-pass between `filtered` and the
  `* voice.vca` multiply, using the same `HighPass` the module input already
  uses. The corner is `1/(2π·C59·R_pin9)`; R108 and VR27 are not in-tree, so the
  load is declared **voiced and bracketed** exactly as
  `moduleCouplingResistanceOhms` already is for C56/C50 — 33 kΩ gives 4.82 Hz
  and 100 kΩ gives 1.59 Hz, both far below the band, so the audible consequence
  is insensitive to the choice inside the bracket, and that is what the test
  fences rather than the corner. Closes gap 6.

  **The step survives review, but for a different reason than the original
  gave.** The original's headline — "+187.7 mV of DC against 60.4 mVrms of AC,
  9.9 dB above the signal" — does not reproduce and is withdrawn; measured on
  the settled sustain, the filter output's DC is a few tens of millivolts and is
  always well *below* the AC it rides on. What is real is that this DC is
  multiplied by the envelope and arrives as a **duty-dependent sub-audio
  thump**: the peak of the output through a four-pole 20 Hz low-pass, over a
  note-on/note-off cycle, rises from **−42.49 dB relative to broadband RMS at
  duty 0.5043 to −17.55 dB at duty 0.9436** — **24.9 dB from PWM duty alone**
  [re-measured 2026-08-08 at implementation; the preflight's −41.65/−10.27 dB
  pair was taken before step 2 of this same pass landed, which is why it no
  longer reproduces — see the correction note below]. At Unit Character 0.0 the
  same measure reads −58.23 / −24.97 / −19.14 dB across the same three panels,
  so the mechanism is a property of the **nominal calibrated model** — the
  cascade rectifying an asymmetric pulse — and not a tolerance artefact. At
  Character 0.0 and panel 0.00 the duty is exactly 0.5000 and the pin 9 mean is
  **+0.000085 V**: a symmetric pulse makes no offset, which is the mechanism
  stating itself.

  *Verified by*: a new engine-suite fixture holding MIDI 48 on a pulse patch at
  CUTOFF 0.30, RESONANCE 0.75, PWM SOURCE = MANUAL, Unit Character 1.0,
  ATTACK 0.45, DECAY 1.0, SUSTAIN 1.0, RELEASE 0.30, released at 4.0 s of an 8 s
  render, at PWM panel 1.00 (duty **0.9436**), panel 0.50 (duty 0.7224) and
  panel 0.00 (duty 0.5043). It asserts **(a)** the **Hann-weighted** mean of the
  sounding voice's filter-to-VCA input over the settled 1.5–3.0 s window is
  **below 1.0e-3 V** — **+0.029799 V today** at panel 1.00, **+0.042769 V** at
  panel 0.00, which is where the sweep's maximum actually sits, and
  **+0.024291 V** at panel 0.50 [re-measured 2026-08-08 at implementation];
  and **(b)** the peak of the output through a **four-pole 20 Hz low-pass** (a
  cascade of four one-pole sections, taken over the whole 8 s render including
  the note-on and note-off transients) is **at least 25 dB below broadband RMS**
  at duty 0.9436, where it is **−17.55 dB today**, while the same measure at
  duty 0.5043 — **−42.49 dB today** — **does not rise**, allowing 3 dB of slack.
  **The original assertion (b) — a 2 Hz DFT bin at least 40 dB below RMS — stays
  deleted because it passes today by 34 dB** (measured −73.56 dB): a six-second
  bin averages a transient away, and a test that cannot fail proves nothing.
  Removing the coupling restores the −17.55 dB figure and fails the new (b).

  *Contract corrected in preflight, 2026-08-07.* Three faults, none fatal to the
  step. First, **(b)'s measurand was under-specified to the point of being
  unreproducible**: "the peak of the output low-passed at 20 Hz" reads −9.87 dB
  through one pole and −41.65 dB through four, on the same render at the same
  duty. A 32 dB estimator dependence makes any threshold arbitrary, so the
  order, the topology and the window are now stated. Second, the DC figures did
  not reproduce — the settled filter output carries +0.0791 V at the fixture's
  own top duty, not +0.0308 V, and the sweep maximum is +0.2015 V at a *middle*
  duty rather than +0.129 V at an extreme. Assertion (a) bites either way, but
  the numbers a later reader would check against are now the measured ones.
  Third, the duty-0.50 clause was **two-sided — "within 3 dB" — and a correct
  implementation would have failed it**: the coupling removes DC *and* the
  sub-5 Hz part of the note gate, so that figure must fall, and by design. It is
  now a one-sided ceiling. It cannot fail today and is not meant to; (a) and
  (b)-at-top-duty are the assertions that bite. *(The preflight's own DC
  figures are themselves superseded — see immediately below. Its three
  structural corrections all stand; only its arithmetic moved.)*

  *Contract corrected again at implementation, 2026-08-08.* Three more faults,
  again none fatal. First, **none of the preflight's numbers reproduce on this
  tree, and the reason is step 2 of this same pass.** The fixture sits at
  RESONANCE 0.75, where the loop gain is **k = 2.9413**; the deleted
  `frequencyTrimAmount` used to multiply cutoff by `1 + 0.098·(k/4)²` = **1.0530**
  there, and the derived trim that replaced it is identically 1.000000 below a
  loop gain of 4. The fixture's corner therefore fell by **5.30 %** when step 2
  landed, and every figure taken through it moved with it. Measured on today's tree with the
  fixture fully pinned, the pin 9 mean is **+0.042769 / +0.024291 / +0.029799 V**
  at panel 0.00 / 0.50 / 1.00 — the sweep maximum is at panel **0.00**, not at a
  middle duty, and the assertion bites by a factor of 43 rather than 51. Second,
  **(a)'s estimator was under-specified in the same way (b)'s was.** A plain
  rectangular mean over 1.5–3.0 s is not an integer number of periods of a
  130.8 Hz note, and the leakage it leaves — up to **2.7e-3 V**, larger than the
  1.0e-3 V bound and entirely independent of the coupling — would fail the
  assertion after a correct fix. On the same render the Hann-weighted mean of
  that node reads **1.7e-4 V**, and at the quietest panel the two estimators
  differ by a factor of nineteen (−6.1e-4 V plain against −3.2e-5 V weighted);
  the window is now part of the measurand. Third, the
  **35 dB fence in (b) is unreachable and is lowered to 25 dB.** With C59 in
  place the same measure reads **−30.25 dB** at duty 0.9436. What remains is not
  DC: it is the attack's own amplitude ramp, which at roughly 2 Hz is *below*
  the coupling's 4.82 Hz corner and so is only about 8 dB attenuated by it, and
  which is a real property of a note that starts. 25 dB leaves the fence failing
  by 7.5 dB before the change and clearing by 5.3 dB after it.

  *What actually shipped, 2026-08-08.* The mechanism as written, with the
  bracketed load as written. Two constants in
  `Source/DSP/YouKnow106Engine.cpp` — `vcaInputCouplingCapacitanceF = 1 µF`
  (C59, anchored) and `vcaInputCouplingResistanceOhms = 33 kΩ` (voiced, OQ-19)
  — give `vcaInputCouplingCornerHz()` = **4.822877 Hz**, and `renderVoice` runs
  the filter output through one more `HighPass` before the `* voice.vca`
  multiply. The capacitor is advanced ahead of the inactive-voice early return,
  because it is a physical node on a card that stays powered. No new law and no
  fitted quantity: the corner is `1/(2π·C·R)` and the stage is the same
  topology-preserving one-pole the module input already uses. It costs one more
  one-pole per voice per internal sample against a four-stage OTA solve:
  `testCpuBudget`'s patch measures **1.2351x realtime before and 1.2442x after**
  (best of five), which is 0.7 % and inside the run-to-run spread.

  Independent corroboration of the bracket, from a source section 6 already
  reads: Ultramaster KR-106 v2.5.13's changelog gives its own post-VCF,
  pre-VCA DC blocker as **1.59 Hz** — the 100 kΩ end of this step's declared
  33–100 kΩ bracket. Two implementations that never saw each other's code land
  inside the same octave-and-a-half, which is what "insensitive to the choice
  inside the bracket" is supposed to mean.

  Measured effect, on the fixture the test uses:

  | PWM panel | duty | pin 9 mean before | after | sub-20 Hz peak / RMS before | after |
  |---|---|---|---|---|---|
  | 0.00 | 0.5043 | +0.042769 V | −0.000032 V | −42.49 dB | −54.59 dB |
  | 0.50 | 0.7224 | +0.024291 V | −0.000167 V | −25.10 dB | −28.53 dB |
  | 1.00 | 0.9436 | +0.029799 V | −0.000120 V | −17.55 dB | −30.25 dB |

  Those six pin 9 figures were taken before step 5 landed, and step 5 moved
  them in their last digit: the warm-up accumulator feeds the OTA headroom the
  cascade is solved with, so making it a `double` changes the eight rendered
  seconds of this fixture by a few parts in 10⁵. On the shipped tree the same
  fixture reads **+0.042768 / +0.024289 / +0.029798 V** before the coupling and
  **−0.000033 / −0.000166 / −0.000118 V** after it [verified 2026-08-08 by
  reverting each step in turn]. Nothing else in the row moves — the four
  sub-20 Hz figures are unchanged to the digit — and no assertion is anywhere
  near either version of these numbers.

  The DC falls by a factor of 145 to 1340, and the duty-driven spread in the
  audible measure falls from 24.9 dB to 26.1 dB — that is, it does *not* fall,
  because the residual at wide duty is now the attack ramp rather than the
  offset, and the ramp has its own duty dependence. What the step promised, and
  what the change delivers, is the offset: at every duty the amplifier now
  multiplies a node whose mean is under 0.2 mV.

  *Test:* `testFilterToVcaCouplingRemovesTheDutyDependentThump` in
  `Tests/YouKnow106EngineTests.cpp`, with two new `YouKnow106TestAccess` probes.
  One of them reads a new `Voice::vcaInputVolts`, a per-internal-sample hold of
  the pin 9 node: the value cannot be inferred from the mix, because
  `voiceBusCoupling_`, `commonVcaInputCoupling_` and `outputCouplingLeft_/Right_`
  all sit on the far side of the multiply and remove any DC that survives it.
  *Proved to bite:* with only the `renderVoice` line reverted to
  `const float vcaInput = filtered;`, the suite fails four assertions —
  `the voice amplifier is multiplying filter DC again at duty 0.504347
  (pin 9 mean 0.042769 V)`, the same at duty 0.722353 (0.024291 V) and duty
  0.943640 (0.029799 V), and `the sub-20 Hz thump at duty 0.9436 is back above
  25 dB under RMS (got -17.550660 dB)`, and nothing else in the suite moves.
Restored, the full run is green: 6/6, 224.8 s.

  **One existing test needed updating, and the reason is the change itself.**
  `testQualityChangePreservesOutputCouplingTail` guards that an HQ rebuild does
  not clear C17/C20's charge, and it needs a real tail to guard. Its comment
  already records losing one DC path when C56/C50 were modelled; it has now lost
  the second. What used to charge the final capacitor was each card's own
  0.48 Hz module coupling passing the note-on duty step as a slow transient, six
  of them summing to 0.026 — and C59 removes exactly that before the VCA. The
  tail is now **0.0037**, the gate closure's own step, so the fixture guard moves
  from `> 0.005` to `> 0.002`. The assertion that matters is untouched: the
  quality change must displace the preserved tail by less than 5.0e-4, which is
  still seven times under the tail it would move if C17/C20 were reset. The test
  now reads only a step a real gate leaves, not DC the model manufactures, which
  is the stronger position.

- [x] **4. Set the BBD line noise from the MN3009's own noise specification.**
  `independentLineRandomAmplitude` is the last voiced quantity in an otherwise
  anchored part model: the same datasheet already fixes the −3 dB bandwidth at
  12 kHz and the distortion at 0.3% / 2.5%. Stated plainly, this is *not* a fit
  to a recording — the head-to-head recorded the model's floor ~9 dB above one
  lossy archive recording and the comparative assessment ~5 dB below another,
  and agreement with an undocumented chain is not a fidelity claim in either
  direction. Closes gap 3.

  **The target is a bracket, not a point, and the step must say so.** The
  datasheet's own two figures disagree by 10.5 dB: **0.2 mVrms max, A-weighted**
  against the ~59.7 µVrms implied by **S/N 88 dB typ** at the 1.5 Vrms maximum
  input. Landing the model exactly on a guaranteed worst case is a choice inside
  that bracket, not a derivation from it. The step therefore moves
  `independentLineRandomAmplitude` to the value that puts the recovered
  A-weighted wet-line noise at **0.200 mVrms** — the conservative end, chosen
  because it is the *guaranteed* figure and because any lower value is
  indistinguishable from silence against gap 7's bit-exact zero — and records in
  the research contract that the datasheet also supports a figure 10.5 dB lower,
  so a later pass has the bracket rather than a rediscovered contradiction.

  *Verified by*: a new circuit-suite check beside `testChorusNoiseComponents`
  asserting the recovered wet-line A-weighted noise is **at or below 0.200 mVrms
  (allowing 0.05 dB for the estimator's own repeatability, see below), and
  within 1 dB of it** (the "max" is an upper bound, so the assertion is
  one-sided by construction with a floor to catch an over-correction), plus a
  new engine-suite assertion that the idle plug-in output floor with **VOLUME
  0.80, VCA LEVEL 0.80, CHORUS NOISE 1.0 and CHORUS MODE I pinned**, measured as
  RMS over 4 s after a 2 s settle, lands at **−77.85 dBFS ± 0.5 dB**, against
  **−63.44 dBFS today** [re-measured 2026-08-07 preflight; the shipped fixture
  reproduces it at **−63.4413 dBFS**, to the digit] — the same 14.4 dB the
  wet-line assertion imposes. The two deltas must agree to within 0.5 dB: with
  the chorus off the same fixture's output is **bit-exact zero** (measured,
  peak 0.000e+00 over 2 s), so the BBD line noise is the *only* contributor to
  this floor and any
  output-floor movement that does not match the wet-line movement means
  something other than `independentLineRandomAmplitude` moved.
  **One target, not two:** the original's separate −76.2 dBFS (I) and −75.5 dBFS
  (II) assert a 0.7 dB mode difference the code cannot produce —
  `Chorus::settingsFor` gives modes I and II the same `lineGain` and the same
  sweep, differing only in rate, and `enableChorusRateNoise` is off by default;
  measured at the pinned controls above, the two floors are **−63.4413 and
  −63.4504 dBFS, 0.0091 dB apart** [reproduced on implementation: the shipped
  fixture reads −63.4413 dBFS for mode I exactly, and a separate harness reads
  −63.4409 / −63.4508, 0.0099 dB apart — the fourth decimal moves with the
  measurement window and nothing else does]. The existing MN3009 bandwidth and
  THD anchors must pass unchanged.

  *Superseded 2026-08-09 by continuous-fidelity step 2:* “one target” applies
  to the MN3009 part row after separating the instrument-level factor, not to
  the finished instrument's two mode floors. The shipped default now carries
  the reported ~3.95 dB II−I output delta directly.

  **The measurand needs a rate and a window, and the upper bound needs a
  tolerance** [corrected on implementation, 2026-08-08]. Two things the contract
  above left unstated turn out to decide whether it can be met at all.

  *It is rate dependent (dated Step-4 measurement).* A coarser numerical grid folds more of the held noise
  sequence back into the reconstruction band, so the recovered figure is not one
  number: the same measure reads **0.40338 at 192 kHz, 0.40795 at 96 kHz,
  0.42238 at 48 kHz and 0.42486 at 44.1 kHz** in transfer terms — so with HQ
  switched off the recovered figure reads **0.40 dB high at a 48 kHz host,
  0.45 dB high at 44.1 kHz and 0.10 dB high at 96 kHz**. Gap 3's 192 kHz is
  therefore part of the measurand and not an incidental fixture choice. Across
  HQ the choice does not bite: `minimumHqProcessingRate` is 176400, so HQ
  targets 176.4 kHz from the 44.1 kHz host-rate family and 192 kHz from the
  48 kHz family, and the two read 0.005 dB apart. Step 9 supersedes these
  numerical-transfer values after replacing the output support integration:
  HQ now spans only 0.200006–0.200078 mVrms (<0.004 dB), while HQ-off reads
  +0.36…+0.38 dB at 44.1 kHz, +0.30…+0.31 at 48 kHz,
  +0.06…+0.08 at 88.2 kHz and +0.05…+0.07 at 96 kHz.

  *A bound placed exactly on 0.200 mVrms is decided by the estimator, not by the
  constant.* This is a finite-window estimate of a random process's power and
  does not converge better than about ±0.1%: over windows of 4 s to 256 s in
  both modes the pre-change estimate spans **1.04657–1.04968 mVrms** about a
  **1.04878 mVrms** long run, and a 1 s window reaches 1.05429. Scaled onto the
  0.200 mVrms target that is a ±0.02 dB coin flip against a strictly one-sided
  bound. The upper bound therefore carries **0.05 dB** — about four times the
  estimator's measured 0.026 dB peak-to-peak spread over that window range, and
  negligible against the datasheet's own 10.5 dB bracket. The lower
  fence stays at the step's 1 dB, because a maximum is a maximum and 1 dB only
  has to catch a gross over-correction.

  *Contract corrected in preflight, 2026-08-07.* The floor assertion was written
  as "a delta against a reference render", which is not buildable: once the
  constant moves there is no pre-change engine left to render, so the reference
  can only ever be a number recorded now. The reason given for avoiding a hard
  dBFS figure — that it scales with two panel controls — does not apply once
  those controls are pinned, which the same sentence already does. Measuring the
  chorus-off floor settles it: it is bit-exact zero, so the pinned dBFS figure
  is fully determined by the line-noise constant and is stated directly. The
  requirement that the two deltas agree is new, and is what stops the second
  assertion from being satisfied by moving a gain somewhere else.

  *What actually shipped in Step 4.* The step, with the measurand pinned down as above.
  `independentLineRandomAmplitude` is no longer a literal: it is
  `mn3009OutputNoiseAWeightedVrms / (nodeVoltsPerUnit ·
  lineNoiseAWeightedTransfer)` = **1.90687e-4**, down 14.39 dB from the 1.0e-3
  compatibility level, and all three terms are published on `Chorus` so the
  suite can re-solve the equation rather than re-assert the answer. The middle
  term is new and is the part the step's text does not name: the datasheet row
  bounds a noise *voltage at the part's output*, so referring it to the
  amplitude the line writes at each clock edge needs the whole chain from the
  injection point — hold, tap-summing pole, both reconstruction sections, wet
  output coupling — under the datasheet's own A weighting. That transfer is
  **0.4034**, which was 1/√3 (a uniform sequence's own RMS) times 0.6987, and the
  second factor is dominated by the hold: at the sweep's 20.0–91.4 kHz clock the
  held sequence's sinc-shaped density puts roughly half its power inside the
  10 kHz reconstruction band. It is a measured property of this model's own
  linear filters, not a fit to any recording, and the assertion re-measures it
  from the render, so a change to the reconstruction sections that left it stale
  would fail. Step 9 exercises that fence: the current exact output transition
  remeasures the factor as **0.4026 = 1/√3 × 0.6973** and therefore derives
  `independentLineRandomAmplitude = 1.9106577e-4`; the part's 0.2 mVrms row,
  node coordinate and stochastic law do not change.

  Both gap-3 figures reproduce: **1.0611 mVrms full band** to the digit, and
  **1.0488 mVrms A-weighted** against the gap's 1.047 — a 0.015 dB refinement
  from averaging both channels and both modes over a long window rather than
  one channel over a short one. The gap's text is corrected accordingly.

  Everything the step predicted landed. Recovered A-weighted wet line after the
  change: **0.19978 mVrms (I) and 0.20016 mVrms (II)** at the suite's 16 s
  window, 0.19997 / 0.20001 in the long run. Idle output floor at the pinned
  controls: **−77.8342 dBFS (I)** on the shipped fixture, and −77.8345 /
  −77.8444 dBFS (I / II) on the separate harness, against the step's predicted
  −77.85 ± 0.5 dB — 0.016 dB from the prediction. The two deltas agree: the
  constant moved **−14.3929 dB** exactly, the idle floor moved −14.3929 dB and
  the recovered wet line −14.3922 dB, which is what a path that is linear from
  the injection point onwards has to do. The MN3009 bandwidth and THD anchors
  pass unchanged, as does everything else in the suite.

  One existing check needed a one-line change and is stronger for it.
  `testBbdOutputPolyBlepSeparatesPhysicalAndNumericalAliases` re-derived the
  expected held sample with a literal `1.0e-3f`. It is a bit-exactness check on
  the BLEP path, not a level check, so it now reads
  `Chorus::independentLineRandomAmplitude` and no longer has an opinion about
  what that value is.

  Two small notes. The 2.6 V node coordinate had to be named inside `Chorus` to
  refer the datasheet's volts to model units; the engine `static_assert`s it
  equal to `internalVoltsPerUnit` so the two files cannot drift apart. And gap 7
  predicted this: with the chorus wet path 14.4 dB quieter, the dry path's own
  bit-exact silence between notes is now that much more exposed.

- [x] **5. Make the warm-up clock a wall-clock accumulator neither a quality
  setting nor a host rate can move.** `thermalWarmupSeconds_` becomes a `double`
  (or an integer internal-sample count divided at the point of use), so the
  increment stops falling below half an ULP and the modelled law
  `T(t) = 25 + 15(1 − e^{−t/900})` runs to completion. No constant, curve or
  time-scale changes; only the accumulator's precision does. Closes gap 8. The
  invariant restored is the one `Source/DSP/YouKnow106Engine.h:812-818` states in
  words. The freeze point is a power-of-two boundary set by the **internal**
  rate: the accumulator stops when `inverseOversampledRate_` falls below half an
  ULP of the running total, which at a 192 kHz internal rate is exactly
  **128.0000 s** and at a 48 kHz internal rate exactly **512.0000 s**.

  *Verified by*: a new engine-suite fixture running silence at **Unit Character
  1.0** (`getDisplayTemperatureC()` scales the rise by `calibration`, so the
  targets below are meaningless at any other setting) with the **spatial thermal
  gradient disabled** (it adds up to 4 °C on card 0 and would put the headroom
  target out of reach), polling `getDisplayTemperatureC()` at t = 128 s, 300 s
  and 900 s of audio. It asserts **26.9886 / 29.2520 / 34.4818 °C ± 0.05 °C**
  (frozen at 26.9886 °C today at 48 kHz HQ), that the three readings **agree
  within 0.01 °C across 48 kHz HQ on, 48 kHz HQ off, 96 kHz HQ on and 192 kHz HQ
  on** — today **26.9886 / 31.5077 / 26.9886 / 26.9886** at their respective
  stalls — and that the modelled OTA headroom at 900 s is **6.5687 V ± 0.001 V**
  (**6.4087 V today at every HQ-on rate**, 6.5052 V at 48 kHz HQ off). Reverting
  the accumulator to `float` fails all three.

  Because a 900 s render at 192 kHz internal is expensive, the fixture may
  advance the accumulator through the test friend rather than rendering — but
  only by adding **the same `inverseOversampledRate_` increment the render loop
  adds, once per internal sample, at the rate `prepare()` actually selected for
  that configuration**, so the ULP behaviour is bit-for-bit what a render would
  produce. It may skip the surrounding render work and nothing else. One short
  rendered case must then be checked to agree with its driven counterpart to the
  last bit over the same sample count, proving the accumulator is the one the
  render loop advances.

  *Contract corrected in preflight, 2026-08-07.* Two faults. **The host-rate
  claim is withdrawn: it does not reproduce.** HQ does not multiply the host
  rate by four, it targets an internal rate — measured, 48 kHz HQ, 96 kHz HQ and
  192 kHz HQ all run the engine at **192000 Hz internal**, so all three freeze
  at the identical 128.0000 s and read the identical 26.9886 °C. The stated
  26.0296 °C and 25.5240 °C, and the 6.3775 V derived from the latter, are not
  values this engine produces. The four-way comparison is kept because it still
  fails today — 48 kHz HQ off stalls at 512 s and 31.5077 °C, which reproduces
  exactly — but its justification is now the quality axis and the internal rate
  it selects, not the host rate. Second, the licence to "drive the accumulator
  directly" would have made the four-way comparison **vacuous**: a friend that
  simply assigns the accumulator is rate-independent by construction, so the one
  assertion specifically about rate dependence could be passed without ever
  exercising it. The direct-drive path is now pinned to the render loop's own
  increment. The 6.5687 V figure required the spatial gradient off, which the
  fixture never said; with it on and card 0 the headroom is 6.6542 V and the
  ±0.001 V tolerance is unreachable.

  *What actually shipped, 2026-08-08.* The step as written, and every figure in
  it — including all four the preflight corrected — reproduced to the digit
  before anything was touched. `thermalWarmupSeconds_` is a `double` (the
  step's first option; no integer sample count was needed), and that one
  declaration is the whole defect. Rendering silence on the unchanged engine at
  48 kHz HQ on put the accumulator at exactly **128.000000 s** at t = 128 s of
  audio and left it there through t = 140 s, with the thermometer frozen at
  **26.988573 °C** and the OTA headroom at **6.408747 V**; 96 kHz HQ on and
  192 kHz HQ on read the same three numbers, and 48 kHz HQ off was still
  climbing (130.19 s at t = 128 s, because a float total in [64, 128) rounds
  its 2.083e-5 s increment *up* and the clock runs 1.7% fast before it stops).
  A separate simulation of the accumulation alone puts the freezes at 128.0 s
  after 118.6355 s of audio at 192 kHz internal, 128.0 s after 126.8222 s at
  176.4 kHz, and 512.0 s after 474.5420 s at 48 kHz — the gap's 118.64 /
  126.82 / 474.54 s, exactly.

  After the change the fixture reads **26.988573 / 29.252031 / 34.481808 °C**
  and **6.568748 V**, identically at all four configurations. Reverting the one
  declaration to `float` and rerunning the fixture fails **15 assertions**:
  every 300 s and 900 s temperature at all four configurations, every headroom
  (6.40875 V at the three HQ-on rates, 6.50524 V at 48 kHz HQ off), and all
  three spreads — 0.031551 °C at t = 128 s, 1.728998 °C at t = 300 s and
  4.519115 °C at t = 900 s, against the 0.01 °C the invariant allows.

  Four notes on what the change is made of, none of them a departure from the
  step's substance.

  1. **The increment stays the float `inverseOversampledRate_`**, because the
     step says only the accumulator's precision changes. `float(1/192000)` is
     1.071e-8 low, so 900 s of accumulation lands at 899.9999904 s and
     34.4818083 °C against the law's 34.4818084 — 5.9e-8 °C, six orders under
     the fixture's tolerance. The exponential's argument is narrowed back to
     `float` at the point of use, so the audio path still pays one `expf` per
     internal sample rather than a `double` `exp`; a float ULP at 900 s is
     6.1e-5 s, worth 3.7e-7 °C.
  2. **Two pure extractions**, both so the fixture reads the engine rather than
     a restatement of it. `advanceThermalWarmup()` is the render loop's two
     lines moved into a member the loop calls, so the permitted direct drive is
     literally the render loop's own code reading the rate `prepare()`
     selected. `dynamicOtaHeadroomVolts(parameters, cardIndex)` is the
     headroom's four lines moved out of `renderVoice`, which now calls it, so
     the 6.5687 V assertion reads the number the cascade is solved with. No
     arithmetic and no ordering changed in either.
  3. **The bit-exactness cross-check the step requires is 2 s of silence at
     48 kHz HQ on** against 384000 driven internal samples. Both
     `thermalWarmupSeconds_` (1.9999999785795808 s) and the derived
     `thermalWarmupFraction_` compare equal bit for bit.
  4. **No measurable cost.** The `testCpuBudget` patch, best of five: 1.2478 /
     1.2567 / 1.2828x realtime before, 1.2373 / 1.2425 / 1.2428x after. The
     difference is smaller than the run-to-run spread, which is what one double
     add in place of one float add per internal sample should look like.

  No existing test needed changing; the whole suite is green (6/6, 243 s).

- [x] **6. Route the velocity extension through the envelope's own path to the
  filter.** Velocity stays an extension — the hardware has none, `velocityDepth`
  stays at 0 by default, and the faithful default render stays bit-identical —
  but when a player turns it up it must do what a dynamics control does. Rather
  than adding a new curve, velocity scales the **ENV amount into the VCF**, the
  one path the instrument already has from the envelope to cutoff: the same
  `envDepth` the panel drives, multiplied by the same `velocityGain =
  1 − velocityDepth·(1 − velocity)` the VCA already applies. A quieter note is
  then a note whose filter envelope opened less far, which is what happens on
  every velocity-sensitive analogue polysynth. Closes gap 5. *Note*: the
  original justified the choice by what Softube Model 84 and Roland Cloud
  JUNO-106 do; section 6 records that all five commercial rows are **unrefreshed
  and unverifiable this session**, so that justification is withdrawn. The
  argument that stands is the internal one — this is the only envelope-to-cutoff
  path the instrument has, so using it adds no law.

  *Verified by*: a new engine-suite fixture rendering MIDI 48 at velocity
  0.2 / 0.5 / 1.0 with `velocityDepth = 1.0`, **CUTOFF 0.30, ENV depth 0.30,
  RESONANCE 0.30, ATTACK 0.0, DECAY 1.00, SUSTAIN 1.00, RELEASE 0.0, Unit
  Character 0.0** — the envelope must be stated, because the measurement is
  taken at t = 0.3 s and a decaying envelope would put the three velocities at
  three different points of the decay rather than three different depths. It
  asserts

  1. **the audible one.** From a 32768-point Blackman-Harris spectrum at
     t = 0.3 s on a stated estimator — a 10 Hz grid from 20 Hz to 8 kHz — the
     fraction of energy **at or above 1 kHz** rises monotonically with velocity
     and spans **at least 30 dB** across the three. Today it is identical at all
     three velocities, a span of 0.0001 dB; with the routing in place the
     measured fraction is **−83.62 / −54.31 / −16.33 dB**, a span of
     **67.29 dB** [measured 2026-08-08 on the shipped engine; the preflight's
     −82.55 / −54.15 / −16.28 dB came from driving `envDepth` to
     `0.30 · velocity`, which quantises the *scaled* value to a stored panel
     byte where the shipped mechanism scales the byte the panel already
     stored].
  2. **the seam one.** The corner the render actually uses,
     `atan(voice.filterG) · internalRate / π`, read at t = 0.3 s, rises
     monotonically and spans at least 3000 cents: **189.97 / 458.19 /
     1985.03 Hz**, a span of 4062 cents, against **1985.03 Hz at all three
     today** [measured 2026-08-08 on the shipped engine; the preflight's
     196.57 / 460.50 / 1995.00 Hz was taken before step 2 removed the fitted
     `frequencyTrim`, which sat +8.7 cents high at RESONANCE 0.30 — between the
     +3.32 cents step 2 records for panel 0.20 and the +116.25 cents it records
     for panel 0.80 — and which shifts all three corners alike].
  3. **the faithfulness one.** At `velocityDepth = 0.0` the render is
     bit-identical to a reference. That reference has to be **built**: there is
     no FNV render lock in the CTest suites — the `fnv1a64` hashes the original
     cited live only in `Tools/RenderRealismComparison.cpp`, which is a report
     renderer, not a test. It is captured as a hash constant from the pre-change
     engine and hard-coded, since after the step lands there is no pre-change
     engine to render against.

  Reverting the routing collapses both spans to zero and fails (1) and (2).

  *Contract corrected in preflight, 2026-08-07.* **The original assertion would
  have failed a correct implementation.** At the fixture it named — CUTOFF 0.50,
  ENV depth 0.40 — the realised corner at velocity 1.0 is **32.8 kHz**, far
  outside the 20 Hz–8 kHz band the centroid is taken over, so the filter is
  already fully open at velocity 0.5 and the audible spectrum saturates. The
  measured centroid there is 169.51 / 230.67 / 238.13 Hz: monotone, but a span
  of **68.62 Hz against the promised 100 Hz**. Push ENV depth up to reach 100 Hz
  and the centroid stops being monotone — at CUTOFF 0.30 / ENV 1.00 it reads
  151.73 / **247.62** / 236.59 Hz, velocity 0.5 above velocity 1.0, because once
  the corner passes the strong harmonics the centroid is pulled back toward the
  130.8 Hz fundamental. The centroid is the wrong measurand for this quantity
  and is dropped. The replacement fixture keeps the corner inside the audio band
  at every velocity, and the high-band energy fraction it measures is monotone
  by construction of the mechanism with a 66 dB margin instead of a 69 Hz one.
  The envelope and Unit Character, previously unstated, are now pinned.

  *What actually shipped, 2026-08-08.* The step as written, in one term. The
  ENV-into-VCF sum in `updateVoiceVcfTarget`
  (`Source/DSP/YouKnow106Engine.cpp:3381`) is now
  `envelopeSign · byte7(envDepth) · vcfEnvelopeCounts · envelope ·
  velocityGain(parameters, voice)`, reading the same
  `velocityGain = 1 − velocityDepth·(1 − velocity)` the amplifier reads four
  lines further down. No new constant, no new curve, no new call: the
  extension multiplies what the stored panel byte asks for, exactly as it
  already multiplies the amplifier's own control.

  Three notes on what the change is made of.

  1. **The multiply sits after the panel quantisation, not before.** The
     preflight simulated the mechanism by driving `envDepth` to
     `0.30 · velocity`, which sends the *scaled* value through
     `storedControlByte`; the shipped term scales the byte the firmware
     already stored. The two differ wherever the scaled value rounds to a
     different byte — at velocity 0.2, `byte7(0.06) = 8/127` against
     `byte7(0.30) · 0.2 = 0.0598`, a 5.3 % deeper envelope — which is the whole
     of the 196.57 Hz / 189.97 Hz difference in assertion (2) beyond the
     +8.7 cents step 2 removed. Scaling the stored byte is the right one: the
     panel byte is what the firmware wrote, and the extension is downstream of
     the firmware.
  2. **Assertion (1)'s monotonicity clause does not bite on its own.** On the
     reverted engine the three high-band fractions are ordered — by
     0.000018 dB of float noise. The span clause is what fails, and it fails by
     the full 30 dB. Recorded because a future edit that keeps only the
     monotonicity clause would keep a test that cannot fail.
  3. **The faithfulness assertion is proved rather than hashed.** The step
     asked for a hard-coded FNV render lock captured from the pre-change
     engine. `velocityGain` has two exact identities — it is exactly `1.0f`
     when `velocityDepth` is 0 whatever the velocity, and exactly `1.0f` at
     velocity 1.0 whatever the depth — so the fixture renders the two
     equalities directly on a patch that runs saw, pulse, sub, noise, key
     follow, the filter, the amplifier and chorus I, and asserts a maximum
     difference of exactly 0.0. That proves the same property the hash would
     have, without freezing a constant about this machine's libm.

  Measured on the shipped engine, at the fixture the contract names
  (max difference 0.0 on all three bit-exactness renders, peak 0.7676):

  | | high-band fraction at velocity 0.2 / 0.5 / 1.0 | span | corner at velocity 0.2 / 0.5 / 1.0 | span |
  |---|---|---|---|---|
  | routed | −83.62 / −54.31 / −16.33 dB | 67.29 dB | 189.97 / 458.19 / 1985.03 Hz | 4062 cents |
  | reverted | −16.3311 / −16.3311 / −16.3310 dB | 0.0001 dB | 1985.03 Hz at all three | 0 cents |

  Gap 5's own numbers, re-measured on the reverted engine: on its fixture
  (CUTOFF 0.50, ENV 0.40, RESONANCE 0.30) the centroid reads
  **238.2694 / 238.2690 / 238.2693 Hz** at velocity 0.2 / 0.5 / 1.0 and
  **238.2694 Hz** at every velocity at `velocityDepth = 0.0` — invariant to
  0.0004 Hz, which is the gap's finding, reproduced. The absolute value is
  238.27 Hz rather than the 240.75 Hz the gap records, because steps 2 and 3
  have since moved the corner and the coupling under it; the level span is
  **14.53 dB** against the recorded 14.65 dB, on an RMS taken over the same
  32768-sample window as the spectrum, a window the gap text does not state for
  its own figures.

  No existing test needed changing; the whole suite is green (6/6, 241.97 s).

### 9. Considered and not planned

**Struck by the 2026-08-08 adversarial review.** Both were section-8 steps; both
are recorded here with the measurement that removed them, because the gaps they
name are real and a later pass will want the reasoning rather than the idea.

- **Making a pitch write take effect at the DCO's own next terminal count
  (was step 3; gap 2 stands, the mechanism does not).** The 8253/82C53 identity
  *is* anchored — `Docs/open-questions.md:2607` adjudicates the p. 8 / p. 13
  print against a contradicting forum identification and the page wins — and
  gap 2's comb reproduces to the digit. Three things kill the step as written.
  **(i) It produces no scatter from the state the test would measure it in.**
  Probing the six idle cards after `reset()` and half a second of silence, they
  read `periodSamples = 733.920000` identically with `dco.phase` at
  0.008829300 / 0.961140179 / 0.913451057 / 0.865761936 / 0.818072814 /
  0.770383693 — uniform steps of 0.0476893 of a period, which at 192 kHz is
  again **35.0 samples, 182.3 µs**. The cards keep running behind closed VCAs
  (`Source/DSP/YouKnow106Engine.cpp:4006-4010`), but the power-on pass restarted
  them on the same ordinal grid, so the "pre-existing free-running phase" the
  step would inherit **is the ordinal grid**. From a freshly-reset engine — which
  is what every fixture and every one of the proposed five renders uses — the
  comb would be unchanged in structure, merely re-expressed as a fixed fraction
  of the period instead of a fixed time, which moves its nulls onto fixed
  *harmonic numbers* at every pitch. That is a different artefact, not an
  absence of one, and which of the two you get depends on unspecified play
  history. **(ii) Criterion (b) already passes.** "The deepest null's frequency
  moves by more than an octave across the five pitches" measures 915.69 Hz at
  MIDI 36 against 8231.90 Hz at MIDI 67 **today**; see gap 2. A test that cannot
  fail before the change cannot verify it. **(iii) It is OQ-08's question, not a
  free mechanism correction.** OQ-08 says in terms that the model's
  changed-pitch restart "is a model of plausible reload behavior, not a fact
  established by the service timing chart", and that the task "must determine
  what the 8253 write and surrounding DCO circuitry actually force, **and at
  which edge**". Deferring the phase-zero restart to the next terminal count is
  operationally the same as deleting the pitch-write restart altogether — at a
  terminal count the ramp wraps anyway — so it answers OQ-08's question with a
  guess, in a pass whose own section 10 says it closes no open question. It
  would also change every note-on transient in the instrument, not only Unison,
  putting the "derived note-on timing scatter" and "no machine-gun" strengths
  and the retrigger-decorrelation fixture in play for a benefit that is
  undetermined. The honest next move is OQ-08's capture, plus a separate
  question the gap raises and this pass never asked: six cards dividing one
  crystal with one integer count *are* frequency-identical on the hardware too,
  so some comb is architectural, and only the *offsets* — the uniform ordinal
  grid `converterEventPhases` invents — are the compatibility profile OQ-08
  forbids trusting.
- **Giving the four high-pass legs four states (was step 6).** Struck on
  three counts, all measured under gap 9. The premise contradicts this
  project's own OQ-21 adjudication: C14/R39 feeds TC4052BP YCOM pin 3
  directly, Tr3 controls INH rather than buffering audio, and the switch
  connects that common node to only one leg. The deselected Cut legs are not
  continuously driven; R23/R21 discharge them with 15.705/4.9209 ms decays,
  which is the charge-memory question OQ-21 still owns.
  Position One and position Boost share a pole in the model (59.4083 Hz both),
  and `HighPass::process`'s state update ignores the shelf and high gains, so
  1 → Boost has **no** shared-state defect: isolated, the error is −599.78 dB at
  every horizon. And the residual that does exist settles in **under a
  millisecond** — 1 → 3 measures −45.81 dB at 1 ms and −580 dB by 10 ms, 1 → 2
  −20.15 dB at 1 ms and −131.14 dB at 10 ms — because the destination leg's own
  pole is 225.8 Hz or 720.5 Hz. The tens-of-milliseconds residual the original
  measured at the engine output is `voiceBusCoupling_`, whose corner also
  changes with the HPF mode (0.8209 Hz against 0.4823 Hz) and whose 330 ms time
  constant the proposed change does not touch — so the step could not have met
  its own −60 dB assertion. What a listener actually hears at the switch is the
  instantaneous gain step (+34.5 to +42.3 dB above the steady-tone step), which
  the step preserves by design. Cost: three extra first-order sections on the
  shared bus per internal sample, to remove something inaudible. The right home
  for this is OQ-21's transient ask, with the 1 MΩ bleed in the model rather
  than four always-driven legs.

- **An always-on electronics noise floor (gap 7).** The only part of it this
  project can derive today is Johnson noise of the anchored summing network —
  six 33 kΩ inputs into 3.3 kΩ feedback, `e = √(4kT·R_f·B)` on the feedback
  resistor plus `√6·√(4kT·B/R_i)·R_f` from the inputs, which at 300 K over a
  20 kHz band is about **1.32 µVrms** at the summer output, roughly 124 dB below
  a nominal patch. That is honest and it is inaudible: it would remove the
  bit-exact zero without changing what anyone hears, so it fails this section's
  own rule. Raising it to a level that *is* audible requires the µPC1252H2
  excess-noise figure and the calibrated TP8 capture **OQ-16 already owns**, and
  guessing it would be exactly the "choose a nicer-sounding constant" the queue
  forbids. Note that step 4 makes this gap more exposed, not less, and that is
  the correct order: remove the wrong noise first, then measure the right one.
- **Making the rail droop and the op-amp slew limiter do something (gap 10).**
  Both are correct models of their parts that happen to be inert. The slew
  limiter genuinely cannot engage — a TA75558S at this node is not slew-limited
  by a band-limited signal 1.6 units tall, and −171.48 dB says so — and forcing
  it to would be modelling a part the instrument does not contain. The droop
  coefficient is only derivable from the M5230L's output impedance and the
  reservoir's ESR at the six-voice load current, neither of which is in-tree;
  choosing a coefficient that makes a chord audibly darker is drawing a curve.
  Both are documentation work: state plainly in the README ledger that they are
  modelled and measured inert, with the numbers above.
- **The 270 pF integrator capacitance, the 40 kHz expo ceiling, and KR-106's
  zeroed input compensation.** All three are recorded contradictions, not
  established corrections. Acting on any of them would move a first-order
  constant on the strength of one reconstruction lineage or one competitor's
  changelog. They belong in OQ-18 and OQ-09 with their lineage stated, which is
  where this pass will file them, and the 248 Hz self-oscillation anchor pins
  absolute cutoff either way so nothing audible is blocked by leaving them open.
- **Stateful-formulation ADAA (Holters, DAFx-19) in the cascade.** The
  engine's memoryless divided-difference of `ln cosh` applied to four
  nonlinearities inside a stateful feedback solve is the wrong formulation, and
  the citation gap is real. But the measured payoff at the shipping 4x rate is
  small — worst inharmonic line −67.3 dBc on both engines in the head-to-head,
  −64.9 dB A/H on the hot resonant saw — so the honest reason to do it is to
  keep that cleanliness at 2x or 1x, which only pays off inside a wider
  oversampling rework. It is a large change with no audible delta at the default
  quality, so it fails the "must change what the instrument sounds like" rule as
  a standalone step. Record the citation gap in the research contract now; carry
  the work with the oversampling item below.
- **Replacing the capped Newton solve with a bounded-work construction inspired
  by Danish, Bilbao & Ducceschi, DAFx-21 — first feasibility candidate rejected
  on 2026-08-09.** The paper gives a
  one-linear-solve construction for
  its specifically derived Korg35 and Moog systems; it is not a generic solver
  substitution. Its Moog proof is zero-input, assumes a static resonance in
  the paper's admitted range, and explicitly leaves time-varying resonance for
  future work. This Juno cascade instead averages `tanh` over a carried drive
  path and adds a nonlinear return, stage-specific pole scales and offsets,
  dynamic headroom, Early effect and live cutoff/resonance changes. None of
  those inherits the paper's Hamiltonian, state transform or Lyapunov proof,
  and the first-order construction can change sound and aliasing. Continuous
  step 4 therefore bakes off a research-only one-step
  quasi-Newton candidate before touching the load-bearing shipping routine. It
  passes the static, RK64, residual, oscillation, retime and fold-back gates but
  fails reachable scanned-control parity at both engine bounds and every
  standard 44.1–192 kHz host/HQ internal grid. The worst error is +21.31 dB RMS
  at 8 kHz/card 1, with the production cap reaching `g=6.31375`; 44.1 kHz is
  +18.50 dB and even 192 kHz reaches +5.01 dB. The separate +4.80 dB
  `g=30` torture result remains only an out-of-domain boundedness diagnostic.
  The candidate is rejected, and `OtaCascade::process` remained unchanged at
  this checkpoint. Continuous Step 10 later supersedes that dated state with a
  different construction—direct fixed Merson integration of the continuous
  ODE, not a port-Hamiltonian transplant or the rejected one-step update.
- **Narrowing the oversampled domain, or offering an intermediate 2x.** KR-106
  exposes Off/2x/4x and confines oversampling to the nonlinear stages; this
  engine oversamples the DCO, the BBD and the scanned control system too. That
  is the untried architectural lever on the cost axis and the only one the plan
  above never considered. It is not a free win — this DCO is a genuine
  ramp-and-comparator solve rather than a wavetable and the BBD is genuinely
  bucket-clocked, so both may legitimately need the higher rate. Continuous
  step 5 now establishes exact scaling for selected semantic events by domain
  and uninstrumented whole-engine thread-CPU baselines without changing the
  signal path. It does **not** publish which domain needs 4×. Continuous step
  6 below supplies the common-host 1×/2×/4× isolated-domain matrix against
  analytic DCO/scan, RK4 VCF and closed-form BBD references. Its dated DCO
  result blocked every tested factor; Step 7 subsequently clears those DCO
  cells without changing the global oversampling-factor selector. Step 8 then
  replaces only the BBD's linear input-edge sampler with a causal current-plus-three-past
  four-point Lagrange reconstruction. Step 9 then advances each complete
  six-state support side as one exact continuous transition under that same
  causal drive, with exact output at every rate and exact input at internal
  rates ≥176.4 kHz. The common-host 4× cells and all six actual HQ selector
  paths now pass every gate in the four-case low-drive deterministic-line
  fixture; lower common-host factors remain
  absolute REJECT and HQ-off passes only frozen Step-8 nonregression limits.
  At the Step-9 checkpoint every VCF cell still rejected. Step 10 now passes
  the common-host VCF q4 cells and all six standard dynamic HQ selector cells;
  q1/q2 remain REJECT and the 8 kHz/4× endpoint remains an event-timing
  REJECT. These isolated admissions still cannot qualify a split before
  inter-domain reconstruction, whole-engine behavior and latency parity are
  measured.
- **Note-on-to-first-sample latency — completed 2026-08-09.** Continuous-work
  step 3 measures and publishes the complete declared 48 kHz host/scan-phase
  distribution before any scheduler change. It became a step because making
  the result durable required a 12,096-case regression, exact processor-latency
  fences and an evidence-bound
  documentation update. It establishes the build baseline, not whether the
  normalized phase origin matches or should be changed to match hardware; that
  remains OQ-08.
- **Additive mains hum, and the chorus click resonance.** The settled guardrail
  that rejects mains ripple derives only the *cutoff-modulation* path — 3300 µF
  behind a 0.25 A secondary, ~60 dB of M5230L rejection, ~0.03 cents of cutoff
  shift. It says nothing about ripple arriving additively through the audio
  stages' PSRR or the six-voice summing bus's ground returns. Extending that
  derivation to the additive path, or adding the measurement to OQ-16, is owed —
  but the extension may well also come out inaudible, and modelling a hum on the
  strength of a competitor's changelog is not evidence. The chorus click
  resonance is squarely OQ-20 and needs the capture the queue already specifies;
  KR-106's ~30 Hz / Q = 18 gives that protocol something concrete to confirm.

### 10. What this pass deliberately does not do

- **It closes no open question.** Every P0 row still needs the calibrated
  captures the queue specifies. Steps 2 and 3 touch mechanisms OQ-09 and OQ-19
  own, and each is fenced by an anchor or a null test rather than by a claimed
  hardware number. The two steps that would have pre-empted OQ-08 and OQ-21
  were struck in review and are recorded in section 9.
- **It fits nothing to a recording, and nothing to a competitor.** Step 4's
  target is a datasheet figure from a part the model already anchors twice over,
  not the head-to-head's ~9 dB or the assessment's ~5 dB against undocumented
  archive material -- and the review made the step carry the datasheet's own
  10.5 dB bracket (0.2 mVrms max against 59.7 uVrms from S/N 88 dB typ) instead
  of presenting one end of it as a derivation. Step 6 no longer leans on what
  Softube or Roland Cloud are said to do. The 2026-08-08 search backfill has
  since corroborated those rows, but corroboration by search summary is not a
  source read, so the step stays independent of them.
- **It does not chase the upper-mid darkness lead.** That is still OQ-15/OQ-18
  work. The 270 pF and 40 kHz contradictions above point at the same region and
  are recorded, not acted on.
- **It does not address real-time cost.** Section 4's after-column stands; a
  six-voice chorus-off render re-measured on this box costs 1.354x realtime,
  consistent with it. None of the six steps makes that better. Step 3 adds one
  first-order section per voice, which is small.

  **[corrected 2026-08-08, on implementation.]** The rest of this bullet
  predicted that step 2 would be expensive, and it was written against an
  implementation that did not ship. It is kept because the measurement behind
  it is sound and the fence it names is the right one; what did not happen is
  the cost. The shipped correction is a pure function of loop gain, tabulated
  once in `prepare()`, so the memo below survives untouched and `testCpuBudget`
  moves 1.2455x → 1.2543x realtime, inside the run-to-run spread. Step 3's
  own measurement is 1.2351x → 1.2442x, and step 5's one double add is
  likewise inside the spread. The pass costs, in total, less than this box's
  own noise. The prediction as written:

  **Step 2 is not small**, and the original's "very slightly worse" is
  withdrawn: making the frequency trim a function of a running amplitude
  estimate defeats the exact-equality `filterG` memo at
  `Source/DSP/YouKnow106Engine.cpp:3290-3298`, and re-running the chain that
  memo guards -- an exp2, two double `pow` and a `tan` per card per internal
  sample, by the memo's own comment -- costs a measured **+18.4%** of the whole
  render before the follower and the table are counted. Step 2 must keep the
  memo alive and republish `testCpuBudget`'s measured ratio. Note also that the
  fence the original named is the wrong one: the
  high-resonance-to-plain-patch ratio (`< 1.7`) barely moves when both patches
  rise together, and it is `testCpuBudget`'s absolute ceiling that is at risk.
  Striking the four-legged HPF step removed three extra first-order sections
  from the shared bus that would have bought nothing audible.
- **It added no demo take at this checkpoint.** The renderer still wrote ten
  and the then-current per-fix previews stayed frozen. That corpus was later
  retired in the 2026-08-09 audio reset; the
  [current audio index](audio/README.md) owns replacement renders.

### 11. Result

All six surviving steps landed, each with a fixture that was proved to fail by
a real revert and a real rebuild rather than by argument, and the suite is
green at the close of the pass: **6/6, 245.38 s** (Engine 229.92, Circuit 6.96,
SysEx 0.01, RenderDemos 0.56, AuditFactoryPresets 7.93,
RealismComparisonContract 0.01). The pass set out to remove mechanisms that
mis-shape the output rather than to add any, and that is what it did. One
constant left the engine — the fitted `frequencyTrimAmount` — and the only
quantities that arrived are C59's two component values, one of them a
designator read and the other voiced inside a stated bracket, and the three
published terms of the chorus-noise derivation: a datasheet row, the model's
own node coordinate, and a transfer measured from the model's own filters.
Step 2's derivation added nothing at all; its inputs were already in the
profile. Steps 1, 5 and 6 changed no constant of any kind.

| Step | Measurand | Before | After |
|---|---|---|---|
| 1. LFO DELAY gates PWM | duty span over 200 ms at t = 0.05 s, DELAY 1.0 | 0.4166 | **0.0000**, duty pinned at 0.7250 |
| 2. Derived frequency trim | corner lift at resonance panel 0.50 / 0.80 | +32.24 / +116.25 cents | **0.00 / 0.00 cents** |
| 2. Derived frequency trim | rendered limit cycle | 4.83 Vp-p at 248.0 Hz (fitted) | **4.8009 Vp-p at 247.90 Hz** (predicted) |
| 3. C59 into the VCA input | pin 9 mean at duty 0.9436 | +0.029799 V | **−0.000120 V** |
| 3. C59 into the VCA input | sub-20 Hz peak against RMS, duty 0.9436 | −17.55 dB | **−30.25 dB** |
| 4. MN3009 noise row | wet line, A-weighted | 1.0488 mVrms | **0.19978 / 0.20016 mVrms** (I / II) |
| 4. MN3009 noise row | idle output floor at the pinned controls | −63.44 dBFS | **−77.83 dBFS** |
| 5. Warm-up clock | modelled chassis at 900 s, Character 1.0 | frozen at 26.99 °C (HQ on), 31.51 °C (HQ off) | **34.481808 °C at every rate and quality** |
| 6. Velocity into the VCF | rendered corner at velocity 0.2 / 0.5 / 1.0 | 1985.03 Hz at all three | **189.97 / 458.19 / 1985.03 Hz** |

The Step-4 noise row is the dated result of that pass. Step 9's exact output
transition subsequently re-derived the injection transfer and current HQ
values; its checked record is appended at the end of this plan.

Stated as behaviour rather than as numbers: RESONANCE is no longer a second
CUTOFF slider; the voice amplifier no longer multiplies the filter's own DC, so
a deep-PWM patch no longer thumps as it opens; the chorus hiss sits on its
part's datasheet row instead of 14.4 dB above it; the quality switch no longer
decides how warm the modelled chassis gets; LFO DELAY holds the pulse width
with the vibrato and the filter sweep; and the velocity extension is a dynamics
control rather than a second output trim.

**Where reality differed from the plan.**

1. **Step 2 shipped as a closed-form solve, not a running amplitude follower.**
   At a limit cycle the amplitude is not an independent variable — harmonic
   balance fixes it from the loop gain that sustains it — so the correction is a
   pure function of `voice.feedback`, tabulated once. That is the one departure
   from a step's letter in the whole pass, and it took the pass's only predicted
   cost with it: section 10's +18.4 % did not arrive, because the `filterG` memo
   was never defeated. It also declines to cancel *signal*-driven droop, which
   no anchor asks for and which is genuine modelled nonlinearity.
2. **Step 2's own point 2 was wrong and is corrected in place.** It claimed a
   physically honest evaluation under-delivers by 3x and that only a mismatched
   referral reaches the anchor. That is true of a single node and false of the
   mechanism: the node it evaluated is the least driven of four, and carrying
   all four on the stage headroom with the return on its own supplies
   +202.4 cents with no referral. Two smaller figures went with it —
   `loopHeadroomVolts` is 3.4667 V, not 3.520, and the feedback term is 9.40 V,
   not ~9.5 V.
3. **`maximumFeedback` moved, 4.51 → 4.504, and the rendered limit cycle moved
   with it.** The step required the pair be re-solved. With one anchor derived
   the other is alone, so the 4.83 Vp-p the joint fit had settled for was no
   longer a necessary compromise. Every published figure that quoted 4.83 Vp-p
   at 248.0 Hz now reads 4.80 Vp-p at 247.9 Hz.
4. **Step 3's audible measure did not fall the way the gap implied, and the
   shipped note says so.** The duty-driven spread in sub-20 Hz excursion goes
   from 24.9 dB to 26.1 dB — it does not fall, because at wide duty the residual
   becomes the attack ramp, which has a duty dependence of its own. What the
   capacitor delivers, and all it was asked to deliver, is the offset: under
   0.2 mV at every duty, down by a factor of 145 to 1340.
5. **Step 6's two headline figures moved from the preflight's**, for two
   identified reasons rather than one: the shipped term scales the panel byte
   the firmware already stored where the preflight simulated it by scaling the
   value before quantisation (5.3 % at velocity 0.2), and step 2 had by then
   removed the +8.7 cents the fitted trim added at RESONANCE 0.30. Its
   assertion (3) is also proved rather than hashed — `velocityGain` has two
   exact identities, which is stronger than freezing a constant about one
   machine's libm.
6. **Gap 8's per-rate table was wrong at 96 kHz and 192 kHz**, and the two rows
   are struck in place: HQ picks the factor that *reaches* the minimum HQ rate,
   so 48, 96 and 192 kHz with HQ on all run the engine at 192 kHz internal and
   freeze identically. The axis the defect ran along is the quality switch, not
   the host rate.
7. **Two parentheticals moved by less than they are quoted to.** Step 1's 83 ms
   illustration reads 0.3575 / 0.4129 under the fixture's own harness against
   the 0.3574 / 0.4130 recorded in preflight, a one-sample probing offset; gap
   4's older 0.3496 for the same window predates both and is superseded by the
   step's own re-measurement. Step 5's frozen headroom is 6.408747673 V, which
   prints as either 6.408747 or 6.408748 depending on the format. Neither was
   churned through the document.
8. **Four of the six steps were found already implemented in the tree by the
   agent assigned to them.** Rather than re-author existing work or claim
   authorship of it, each did the thing that cannot be faked — a real revert, a
   real rebuild, and the captured failures, plus an independent measurement
   harness sharing no code with the fixture. The revert proofs in steps 3, 4, 5
   and 6 are therefore verification *of* the change rather than by its author,
   which is the stronger reading of them, and step 6's agent additionally found
   the plan work missing and did it.

**What was struck, and what was not attempted.** Nothing was struck during
implementation; the two struck steps were struck in the 2026-08-08 adversarial
review and are recorded in section 9 with the measurements that removed them —
the DCO pitch-write restart, because it answers OQ-08's question with a guess
and because its own criterion already passed, and the four-legged high-pass,
because this project's own OQ-21 adjudication puts the multiplexer on the
source side and the residual it would remove settles in under a millisecond at
−20 to −46 dB. Section 9's other entries stayed where they were. One of them
carried a documentation debt this pass now pays: the polyphonic rail sag
(0.104 cents across the whole one-to-six-voice load change) and the TA75558S
slew limiter (−171.48 dB when switched off) are modelled, enabled and
measurably inert, and the README ledger says so plainly instead of leaving two
advertised mechanisms doing nothing.

**Honest bounds on what can now be claimed.**

- **No open question is closed.** OQ-09 still owns the panel-to-loop-gain shape
  and the input compensation; OQ-19 still owns the BA662 transfer and the pin-9
  load; OQ-03 still owns the calibrated stereo capture; OQ-08, OQ-16 and OQ-21
  are untouched. What step 2 closed is the *wart* OQ-09 had recorded against
  the fitted trim, which is a different and smaller thing.
- **Two of the six rest on a voiced quantity inside a declared bracket.** C59
  works against a voiced 33 kΩ, bracketed 33–100 kΩ (4.82–1.59 Hz), and the
  chorus noise level sits at the guaranteed-maximum end of the datasheet's own
  10.5 dB bracket. Both brackets are published in the header, in the research
  contract and in the README; neither was narrowed by this pass.
- **Step 1 is internal consistency, not an anchored routing.** No source in
  tree states whether the hardware's DELAY reaches PWM. What is settled is that
  the engine used to contradict itself, including its own LFO display.
- **The derived frequency correction is a property of this cascade, not a
  measurement of an IR3109.** It reconciles the model with both service anchors
  at once and predicts the 248 Hz endpoint to 0.67 cents, which is evidence
  that the derivation is the right one for this model. The finding it exposes —
  that a real card apparently does not droop where this cascade droops
  203 cents — is filed to OQ-09 and not acted on.
- **Nothing was fitted to a recording or to a competitor.** KR-106's 1.59 Hz
  post-VCF DC blocker corroborates step 3's bracket after the fact; it did not
  select the constant. Step 4's target is a datasheet row for a part the model
  already anchors twice over.
- **The cost table above this section stands.** The three steps that could have
  cost anything per sample measure 1.2351 → 1.2442, 1.2455 → 1.2543 and no
  measurable change on `testCpuBudget`'s patch, each inside this box's
  run-to-run spread. The fixture's 5.0x ceiling — a runaway guard, not a
  performance target — was not moved, and the measured ratios are published in
  the steps rather than folded into it. The one-off cost step 2 does add, the
  2.4 ms table solve, is warmed in `prepare()` so no audio callback pays it.
- **No hardware was measured in this pass.** No demo take was added and the
  then-current per-fix previews were not re-rendered. That corpus was later
  retired in the 2026-08-09 audio reset; replacements belong to the
  [current audio index](audio/README.md).

**Documents trued up with the code.** The README's sound-engine sections carry
the six changes and the numbers that shipped; `Docs/circuit-modelling-research.md`
rows 64 (LFO to PWM), 73 (oscillation frequency correction), 75 (voice-module
VCA and C59), 92 (chorus noise) and 99 (velocity) state the new mechanisms with
their evidence class; `Docs/open-questions.md` records the closures inside
OQ-03, OQ-09 and OQ-19 without claiming the questions themselves. Gaps 1, 3, 4,
5, 6 and 8 in section 7 carry closure notes naming the step that closed them.
Gap 2 does not, and must not: its step was struck, so Solo Unison is still the
uniform comb this section measured, and gaps 7, 9 and 10 stand as recorded.

---

## YouKnow106 — continuous fidelity work, 2026-08-09

Every item in this continuation lands as one scoped commit with its regression
and documentation in that same commit. Open hardware questions remain governed
by `Docs/open-questions.md`; a consistency fix must not be promoted into new
evidence about a physical JUNO-106.

- [x] **1. Make the panel help agree with the modulation path.** The DSP, its
  engine fixture, the README and the research contract all agree that the one
  delay-gated LFO value reaches DCO pitch, PWM and VCF cutoff. Two surviving
  tooltips and the PWM-strings renderer comment still described the superseded
  implementation: DELAY said “It does not delay PWM,” and the PWM LFO selector
  said “LFO DELAY does not apply.” They now describe the three-destination
  route the instrument actually runs.
  `testPanelHelpMatchesTheModulationRouting` locates both controls through the
  JUCE-free panel description, requires the DELAY help to name DCO/PWM/VCF,
  requires the PWM help to call its source delay-gated, and rejects both stale
  exclusions. This changes no DSP sample and makes no new hardware claim: as
  section 10 records, PWM delay gating remains an internal-consistency choice
  pending direct evidence.
- [x] **2. Make the shipped Chorus II floor carry the reported I→II lift.** A
  same-chain true-peak capture of a real JUNO-106 reports approximately 3.95 dB
  II−I with Panasonic and Xvive MN3009 populations; the printed pairs give 3.96
  and 3.95 dB. The old default left
  both modes within 0.10 dB and kept the 4.2089 dB rate-law approximation behind
  an off-by-default switch. Mode I now remains on the MN3009-derived baseline;
  mode II applies the observed factor directly,
  `10^(3.95/20) = 1.575796`. The rate-law profile survives under the explicit
  internal name `useChorusRateNoiseHypothesis` and substitutes for, rather than
  compounds with, the empirical factor. Engine regressions measure both modes
  over whole modulation cycles, require the default delta within 0.10 dB of
  3.95 dB, require the alternative within 0.10 dB of the circuit's 4.2089 dB
  prediction, prove mode I is sample-bit-identical between profiles, and prove Chorus
  Noise zero defeats both. The circuit suite still recovers the 0.2 mVrms
  A-weighted part row on each clock programme after separating the output-level
  factor, and directly proves that bypass preserves the last-selected hidden
  II clock/noise profile while legacy `OneTwo` canonicalises to II. A canonical
  Linux before/after render leaves eight documentation
  takes byte-identical and refreshes only `02-pwm-strings.wav` and
  `06-chorus-modes.wav`, the two takes that use mode II. This is a
  moderate-confidence relative calibration only: treating the source's
  true-peak difference as a broadband amplitude factor, absolute PSD,
  bandwidth/weighting, stereo correlation, spurs and the physical cause or
  insertion point all remain OQ-03.
- [x] **3. Characterize note-on playing latency without changing the
  scheduler.** At 48 kHz the scan advances exactly 5/1008 of a pass per host
  sample, so the new engine fixture advances a silent live timeline through
  all 1,008 host-boundary phases, copies it for each Note On, and repeats that
  for all six physical cards with HQ off and on: 12,096 cases. Poly-1 note
  memory selects the card; a two-second exact-silence pre-roll settles the
  powered card state; transpose holds the measured board pitch at C4. The patch
  is saw only, open VCF, ENV VCA, zero attack, full sustain, HPF I, with
  chorus/noise/Unit Character off. Offsets are 0-based host samples from the
  Note On:

  | Model layer (min / median / max) | HQ off | HQ on |
  | --- | ---: | ---: |
  | Pitch/envelope write | 0 / 100 / 201 | 0 / 100 / 201 |
  | VoiceVca target / first nonzero model gain | 70 / 192 / 315 | 70 / 192 / 315 |
  | Held control reaches 63.2% | 102 / 224 / 347 | 103 / 225 / 348 |
  | Raw output-onset proxy, `max(abs(L),abs(R)) > 1e-4` | 90 / 213 / 335 | 93 / 216 / 339 |
  | Nominal host-compensation coordinate (raw minus 24) | 66 / 189 / 311 | 69 / 192 / 315 |

  At this Step-3 checkpoint the engine and plug-in separately reported exactly
  24 host samples for 4×, 2× and 1× numerical paths: 0.500 ms only at 48 kHz,
  0.250 ms at 96 kHz and 0.125 ms at 192 kHz. It was nominal
  oscillator-reconstruction/decimation group delay, not a physical
  decomposition of the signal-dependent output threshold. Step 7 below
  supersedes the reconstruction, report and proxy values while preserving this
  measurement as the before-state.
  The processor fixture also keeps sample-positioned MIDI ahead of rendering
  and exact silence before a late event.

  The regression proves the measured fresh ENV Note On leaves its zero VCA
  target, hold and gain closed until the scheduled path advances, requires
  VoiceVca to follow that card's Pitch/envelope tick, fences
  the per-card 8/23…13/23 ordinal gaps, and verifies the 687 µs hold reaches
  63.2% at a 32-index offset in HQ-off—33 one-pole updates including the write
  sample—and a 32–33-index offset with HQ substeps. HQ
  checks the queue at four internal substeps and HQ-off once per host sample;
  on a few exact boundaries one has passed a write the other still catches, so
  paired cases can differ by one pass (worst raw proxy 205 samples) while their
  aggregate distributions remain aligned. This is scan-grid quantisation, not
  205 samples of extra output-path delay.

  The `1e-4` crossing is a −80 dBFS numerical proxy, not psychoacoustic
  audibility, and the exhaustive claim covers converter phase at 48 kHz for
  this patch/pre-roll—not every oscillator phase, patch, buffer or unit. The
  4.2 ms pass, 23-write order and qualitative non-simultaneity are anchored;
  normalized offsets, phase origin and continuous hold realization are policy.
  Exact acquisition/timing and audible-output captures remain OQ-07/OQ-08/OQ-12,
  and the BA662 onset law remains OQ-19. No production DSP, scheduler, preset or
  audio demonstration changes in this step.
- [x] **4. Derive and bake off a fixed-solve-count VCF candidate before changing the
  shipping cascade.** The primary DAFx-21 paper is now read rather than cited
  by title. It transforms a circuit-specific separable Hamiltonian to a
  quadratic energy and advances the transformed Moog or Korg35 state with one
  state-dependent linear solve. Its supported claim is narrower than the old
  plan: zero-input stability for those derived systems and admitted static
  parameters. It does not derive an IR3109/BA662 cascade, prove BIBO behavior,
  treat aliasing, or extend the Moog proof to time-varying resonance; the paper
  names that last item as future work. Its first-order update is therefore not
  automatically a sound-transparent or Lyapunov-stable replacement here.

  At the Step-4 checkpoint, the deliberately non-shipping feasibility source
  now recoverable from commit `93b3c25` retained the then-engine's discrete
  equations—carried path-average `tanh`, nonlinear feedback return, per-stage
  pole scales and offsets, dynamic headroom and Early effect—but performed
  exactly one
  residual/frozen-modulation Jacobian evaluation and the two bidiagonal solves
  needed for the rank-one feedback correction. There was no tolerance, retry or
  convergence loop. The Early-effect multiplier was reevaluated from the
  then-current state but its derivative was frozen, as in each then-shipping
  quasi-Newton iteration. This was an engineering one-step quasi-Newton
  adaptation, not a transcription of the paper and not a claim that the
  paper's proof transferred.

  The then-registered circuit fixture reused the 12-cell 192 kHz small-signal
  matrix and added four explicit 64× RK4 hot cells (1.5 kHz cutoff,
  3 V/220 Hz drive,
  `k = 0/2/3.6/4.4`), the independent path-equation residual, stage scales and
  offsets, static hot/cold headroom, Early effect, bidirectional/identity
  retiming, the oscillation-threshold and boundedness fixtures, and the existing
  hot C6/fc16k/k3.8 fold-back probe. A reachable-control fixture covered all six
  physical VCF-card slots on the normalized 23-write schedule at the 8 kHz and
  768 kHz engine bounds plus the 44.1, 48, 88.2, 96, 176.4 and 192 kHz standard
  internal grids. After four low-setting settling passes it alternated one
  coherent panel snapshot; the shared resonance and each selected-card VCF
  hold acquired it only at their own ordinals. The fixture applied the
  production 522 µs holds, 14-to-12-bit flooring, count-to-frequency law,
  resonance input compensation and `min(50 kHz, 0.45·Fs)` cap. Its decisive
  motion case used Unit Character zero, so stage scales were unity, offsets and
  ladder carry were zero, headroom was nominal and Early effect was inert; the
  separate static fixture covered those mechanisms at nonzero settings. The
  declared drive was a 2.4 V sine before compensation: the 6 V mixer coordinate
  through the model's 0.4 input attenuation. The former
  direct `g=30`/instantaneous
  resonance/audio-rate-headroom sequence remained solely an out-of-domain
  robustness diagnostic. Per-sample counters proved one system evaluation and
  two bidiagonal solves; elementary-function branches make this bounded work,
  not literally invariant CPU time.

  | Admission result | Shipping Newton | One-step quasi-Newton candidate | Gate |
  | --- | ---: | ---: | --- |
  | Worst small-signal gain error vs 16× RK4 | already ≤0.6 dB | **0.01368 dB** | ≤0.6 dB |
  | Worst hot waveform RMS error vs 64× RK4 | −44.60 dB at `k=4.4` | **−46.03 dB at `k=4.4`** | candidate ≤1.05× shipping RMS error in every cell |
  | Worst static-hot normalized discrete-equation residual | ≤2e-4 fenced separately | **1.84e-5** | ≤2e-4 |
  | Static scales/offsets/headroom/Early parity vs shipping | reference | **−114.88 dB RMS** | ≤−40 dB |
  | Reachable scanned cutoff/resonance parity vs shipping | reference | **+21.31 dB RMS**, 8 kHz/card 1 | **fails** ≤−40 dB |
  | Out-of-domain direct-solver torture parity | diagnostic only | **+4.80 dB RMS** | no parity gate; finite/no recovery |
  | Tail peak at `k=3.6 / 4.3 / 8.0` | pass | **3.08e-7 / 1.269 / 6.369 V** | decay / sustain / finite <40 V |
  | Worst hot folded line | <−60 dBc | **−66.41 dBc** | <−60 dBc |
  | Solve structure per sample | 1–8 evaluations, two solves each | **1 evaluation + 2 solves** | fixed counts |

  The engine-bound/standard-grid failure was not hidden by one aggregate number:

  | Internal grid | Worst card | Relative RMS error | Maximum reachable `g` |
  | ---: | ---: | ---: | ---: |
  | 8 kHz (engine lower bound) | 1 | +21.31 dB | 6.31375 |
  | 44.1 kHz | 0 | +18.50 dB | 6.31375 |
  | 48 kHz | 0 | +18.55 dB | 6.31375 |
  | 88.2 kHz | 3 | +18.47 dB | 6.31375 |
  | 96 kHz | 3 | +18.00 dB | 6.31375 |
  | 176.4 kHz | 0 | +6.76 dB | 1.23580 |
  | 192 kHz | 0 | +5.01 dB | 1.06769 |
  | 768 kHz (engine upper bound) | 0 | −9.10 dB | 0.207431 |

  **Verdict: reject this candidate.** It was accurate for the static bake-off,
  but one frozen-modulation step did not track the shipping solution under the
  actual engine-bound/standard-grid scanned sequence. The earlier −97.56 dB
  figure came from an incomplete 192 kHz fixture that placed cutoff at phase
  zero and resonance at phase 0.5 instead of using the production ordinals; it is
  superseded by the table above. No production code, constant, preset or
  rendered sample changes here; OQ-09 remains open and the shipping capped
  Newton solver was untouched at this checkpoint. Step 10 below later clears a
  new independent standard-grid dynamic matrix with direct fixed Merson
  integration; it does not rehabilitate this rejected candidate, and it keeps
  the 8 kHz event-timing rejection visible.
- [x] **5. Attribute oversampling work before proposing a split-rate engine.**
  The old cost tables were first corrected at their source: the historical
  helper uses `std::chrono::steady_clock`, so those numbers are elapsed wall
  seconds per audio second, not the “process CPU time” the documents called
  them. They remain useful as back-to-back history but are not mixed with the
  new baseline.

  `Tools/AuditOversamplingDomains.cpp` is built twice. The normal executable
  links the shipping `YouKnow106DSP` and times only `engine.process` with the
  current thread's CPU clock. Each fixture is pre-rolled for two seconds, then
  copied outside the timer; buffers are allocated before timing, seven 4×/1×
  pairs alternate order, and every raw run, median, minimum, median absolute
  deviation and raw-float fingerprint is printed. The counter executable links
  a separate Engine/Chorus library compiled with `YOUKNOW106_WORK_AUDIT`; that
  macro and every increment are absent from the shipping translation units,
  and the counter build is never timed or linked into the plug-in.

  On Apple M1 Max, macOS 26.5.1, native arm64 Release, 48 kHz/block 256,
  Unit Character 1.0, the uninstrumented 32,768-frame windows read. The six
  notes are MIDI 36/48/55/60/64/67; the full-mixer row also keeps Chorus Noise
  at its shipped 1.0 setting:

  | Step-5 fixture | 4× CPU/audio | 1× CPU/audio | Paired 4×/1× |
  | --- | ---: | ---: | ---: |
  | Idle, six powered cards closed | 0.533 | 0.145 | 3.684× |
  | Six voices, cutoff .62/resonance .10, chorus off | 0.495 | 0.152 | 3.254× |
  | Six voices, cutoff .62/resonance .95, chorus off | 0.659 | 0.191 | 3.452× |
  | Six voices, full mixer/resonance .70, Chorus II | 0.766 | 0.292 | 2.589× |

  Every timing MAD is below 1%. These are JUCE-free engine thread-CPU
  measurements on one machine, not plug-in/host totals or competitor data.
  They establish the global switch's present cost but cannot predict the
  saving from moving only one part of a coupled loop.

  The Step-5 deterministic work window is the six-voice resonant fixture after
  the same pre-roll, 2,048 host frames. At 48 kHz 4×/1× it counts 8,192/2,048
  internal frames; 49,152/12,288 six-card audio/DCO/VCF steps;
  131,072/32,768 sixteen-slot hold and PWM calls; 16,384/4,096 BBD-line support
  frames; and 6,144/0 decimator calls, or 405,504/0 stereo nonzero-tap MACs.
  Elapsed-time-driven model events stay put: both grids see 10 converter pass
  starts, 61 DCO wraps and 3,162 BBD shifts, with converter writes differing
  by one at the window boundary (234/233). Newton work is not a simple factor:
  210,549/57,052 iterations, or 4.284/4.643 per step, with zero recovery.
  Cutoff misses are likewise nearly event-driven at 1,421/1,385 rather than
  4:1. The BBD correction loops visit 12,648/12,651 past-plus-future edge
  events, even though their audio-rate support frames scale exactly 4:1.

  The event-dependent values above are published observations, not goldens.
  CTest at this checkpoint covers the structural identities plus 96 kHz 2×/1×
  and 192 kHz 1×, requires 33 nonzero half-band coefficients and two stereo
  MACs per visit,
  partitions cutoff memo and VCF path-average work exactly, requires zero
  recovery, and proves four stage evaluations/two bidiagonal solves per
  Newton iteration. It separately
  renders normal and instrumented executables and requires matching raw-float
  fingerprints while the counter sink is active.

  **Verdict: measurement baseline complete; no rate split admitted.** One DCO
  frame, Newton iteration, BBD edge visit and FIR MAC are not equal-cost units,
  and this step measures no cross-boundary error. Production equations,
  oversampling selection, presets and audio files remain unchanged. Step 6
  below performs the same-host isolated-domain qualification, uses independent
  references rather than treating 4× as truth, and rejects rate admission; a
  later production split would still need inter-domain, latency and whole-engine
  parity before it could ship.
- [x] **6. Qualify isolated numerical domains at one common host boundary before
  proposing any rate split.** `Tools/AuditDcoScanQuality.cpp` and
  `Tools/AuditVcfBbdQuality.cpp` link the untouched shipping DSP and render every
  44.1/48 kHz × 1×/2×/4× cell to the same host coordinate. Their registered
  contracts are `YouKnow106.DcoScanQualityContract` and
  `YouKnow106.VcfBbdQualityContract`. Shared
  `Tools/OversamplingQualitySupport.h` supplies VCF/BBD with a factor-independent
  fixed-16× reference path: a 4,097-tap Kaiser FIR, zero-phase host-boundary
  decimation and explicit 0/15.5/23.25-frame alignment. Canonical filter,
  stop-path, phase and fractional-delay self-checks guard that support. The
  then-shipping 4× render remains one candidate under test, never the reference.

  The isolated pre-VCF DCO matrix contains 90 takes per cell: the six
  octave-spaced notes MIDI 36/48/60/72/84/96 in 16'/8'/4', saw and sub, plus
  pulse at 5/50/95% duty. Its absolute metric is
  the worst single FFT bin outside the analytic harmonic mask, relative to the
  fundamental; it is not integrated alias energy or a claim about the complete
  audible floor.

  | DCO worst off-mask bin | 1× | 2× | 4× | −70 dBc gate |
  | --- | ---: | ---: | ---: | --- |
  | 44.1 kHz | −12.780565 dBc | −36.596878 dBc | −42.618000 dBc | **REJECT / REJECT / REJECT** |
  | 48 kHz | −16.741087 dBc | −36.344575 dBc | −41.452375 dBc | **REJECT / REJECT / REJECT** |

  Every per-take analytic multiline control passes its spur and gain gates.
  The separately fenced normalized 23-write scan and the declared DCO,
  two-pole PWM and SUB hold recurrences pass in every cell. Frequency-coincident
  boundary-stopband and pre-grid fold families are diagnostics only: they did
  not establish the source of the DCO result, whose cause remained
  **unattributed at this checkpoint**. Step 7 below supplies the subsequent
  mechanism diagnosis and passing rerun.

  The VCF reference solves the declared continuous four-stage equations at the
  fixed 16× grid with RK4 at four and eight substeps—effective 64×/128×—then
  crosses the independent FIR. The domain is deliberately nominal Character 0:
  input to fourth pole, calibration/offsets zero, `gScale=1`, nominal fixed
  headroom and Early off. Its decisive hot fixture is a 1,046.502 Hz saw,
  16 kHz cutoff and `k=3.8`; the normal 2.4 V mixer coordinate becomes
  4.493952 V after the shipping resonance input compensation.

  | VCF hot waveform NRMS vs RK128 | 1× | 2× | 4× | −40 dB gate |
  | --- | ---: | ---: | ---: | --- |
  | 44.1 kHz | −1.110 dB | −12.232 dB | −24.343 dB | **REJECT / REJECT / REJECT** |
  | 48 kHz | −1.062 dB | −13.751 dB | −25.810 dB | **REJECT / REJECT / REJECT** |

  The exhaustive 20 Hz–20 kHz hot-residual FFT uses 32,768 frames and masks
  only ±6 bins around each legitimate output harmonic, leaving 14,618/13,412
  bins at 44.1/48 kHz under direct review. Its 1×/2×/4× maxima are
  −27.063/−67.588/−99.040 dBc at 44.1 kHz and
  −19.658/−67.128/−99.618 dBc at 48 kHz against a <−60 dBc gate; the matching
  oracle-only off-mask controls are −93.242/−93.163 dBc. The narrower
  3 V/220 Hz driven-sine RK fixtures and 500 Hz-cutoff self-oscillation
  pitch/level fixtures also pass their own gates. None overrides the
  production-compensated full-waveform rejection or admits the VCF domain.
  Character 1 remains untested by this matrix.

  The deterministic one-line BBD reference is closed-form rather than a higher
  shipping grid: continuous input/output component responses, the exact
  128-edge sequence delay and loss pole, and full-period zero-order-hold image
  phasors cross the same independent FIR. Four low-drive cases cover 20 kHz,
  50 kHz and 91.429 kHz clocks plus 50 kHz/12 kHz; the declared amplitude
  `A=0.02` keeps the fitted saturation inside its independently fenced
  linearization bound.

  The SGA metric is the maximum remaining 20 Hz–20 kHz Blackman–Harris FFT bin
  across all four cases after masking only the fundamental and analytically
  validated wanted physical-image lines at or below 20 kHz. Physical sources
  above that boundary are not silently protected. Exactly one non-fundamental
  wanted image line clears the projection threshold in each cell, so the BGA
  column below is that qualifying line's error rather than an exhaustive image
  population.

  | Host | Factor | Analytic NRMS (gate ≤ −40 dB) | Qualifying-line BGA gain error (gate ≤ 0.75 dB) | 20 Hz–20 kHz unmasked SGA (gate < −60 dBc) | Result |
  | ---: | ---: | ---: | ---: | ---: | --- |
  | 44.1 kHz | 1× | −3.099 dB | 34.389 dB | −24.854 dBc | **REJECT** |
  | 44.1 kHz | 2× | −14.910 dB | 4.090 dB | −28.762 dBc | **REJECT** |
  | 44.1 kHz | 4× | −27.043 dB | 0.869 dB | −47.635 dBc | **REJECT** |
  | 48 kHz | 1× | −4.640 dB | 22.893 dB | −28.871 dBc | **REJECT** |
  | 48 kHz | 2× | −16.427 dB | 3.257 dB | −31.329 dBc | **REJECT** |
  | 48 kHz | 4× | −28.183 dB | 0.708 dB | −38.189 dBc | **REJECT** |

  This BBD oracle independently evaluates the same declared component values
  and fitted transfer pole; it is not a second hardware fit or a measurement of
  an original JUNO-106. Edge-state, phase and projection controls pass; the
  exhaustive 20 Hz–20 kHz oracle off-mask controls are
  −93.046/−135.607 dBc and the
  independently bounded post-FIR image tails are −198.030/−202.098 dBc at
  44.1/48 kHz.

  **Verdict: the common-host baseline is complete and admits no production rate
  change.** At this checkpoint all six DCO, nominal-VCF and BBD cells reject
  their absolute domain gates, including the then-current 4× path. No source
  equation, quality selection,
  preset or audio file changes in this step. Inter-domain reconstruction,
  Character 1, whole-engine output, latency parity and live transition behavior
  remain unqualified; none may be inferred from an isolated sub-fixture pass.
- [x] **7. Correct the DCO reconstruction defect exposed by the common-host
  matrix.** Step 6 is retained above as the dated before-state: its six DCO
  cells all rejected −70 dBc. The defect was numerical, not a newly discovered
  oscillator law. The old step table stored `bandlimited step − ideal step`,
  which contains a unit jump at `t=0`, then linearly interpolated that
  discontinuous residual. A query in the last interval before zero therefore
  blended the two event sides and emitted a premature fractional edge.

  The engine now stores and interpolates the *continuous bandlimited step
  response*, then subtracts the exact Heaviside value at the query time. The
  slope residual is continuous at zero and remains stored/interpolated
  directly. A circular H=24 delay supplies the non-causal half of the
  symmetric correction without shifting an array on every sample; immutable
  64× construction tables are shared across engine instances. No oscillator
  equation or source coordinate moved: the 8 MHz integer divider and range
  clocks, 12 V straight ramp, 2.2 µs reset, comparator/duty geometry,
  divide-by-two sub, pitch-write restart policy, scan schedule and held-control
  laws are unchanged.

  The same unchanged 90-take-per-cell audit now reads:

  | DCO worst off-mask bin | 1×, Step 6 → Step 7 | 2×, Step 6 → Step 7 | 4×, Step 6 → Step 7 | −70 dBc gate |
  | --- | ---: | ---: | ---: | --- |
  | 44.1 kHz | −12.780565 → **−83.476933 dBc** | −36.596878 → **−82.436627 dBc** | −42.618000 → **−82.432588 dBc** | **PASS / PASS / PASS** |
  | 48 kHz | −16.741087 → **−84.879008 dBc** | −36.344575 → **−92.976529 dBc** | −41.452375 → **−92.978397 dBc** | **PASS / PASS / PASS** |

  Every cell still contains 90/90 finite candidate takes and 90 valid analytic
  controls; spur, strict/top harmonic gain, normalized scan and DCO/PWM/SUB
  hold gates all pass. Frequency-coincident fold-family labels remain
  diagnostics rather than causal proof.

  Support and decimation were selected together instead of stopping at the
  first passing pair:

  | Correction / half-band candidate | Worst DCO cell | Result |
  | --- | ---: | --- |
  | H=20 / 95 taps | −65.940893 dBc | **REJECT** |
  | H=24 / 79 taps | −68.0828 dBc | **REJECT** |
  | H=24 / 87 taps | −77.8416 dBc | PASS |
  | **H=24 / 95 taps** | **−82.432588 dBc** | **selected; 12.43 dB margin** |

  Extending the selected design to H=32 produced no useful additional
  rejection. The 95-tap Kaiser (β=7.857) boundary is retained because the
  shorter decimator leaked the legitimate 25.1 kHz sixth pulse harmonic back
  near 19.0 kHz at a 44.1 kHz host. This global half-band change was therefore
  rerun through the independent VCF/BBD audit. The VCF's decisive
  production-compensated hot row remains:

  | VCF hot waveform NRMS vs RK128 | 1× | 2× | 4× | −40 dB gate |
  | --- | ---: | ---: | ---: | --- |
  | 44.1 kHz | −1.110 dB | −12.233 dB | −24.348 dB | **REJECT / REJECT / REJECT** |
  | 48 kHz | −1.062 dB | −13.752 dB | −25.810 dB | **REJECT / REJECT / REJECT** |

  Its exhaustive hot-residual off-mask values are
  −27.063/−67.589/−99.040 dBc at 44.1 kHz and
  −19.658/−67.128/−99.620 dBc at 48 kHz; these passing 2×/4×
  spectral subchecks do not override the full-waveform rejection. The complete
  deterministic BBD rows are:

  | Host | Factor | Analytic NRMS | Qualifying-line BGA error | Unmasked SGA | Result |
  | ---: | ---: | ---: | ---: | ---: | --- |
  | 44.1 kHz | 1× | −3.099 dB | 34.389 dB | −24.854 dBc | **REJECT** |
  | 44.1 kHz | 2× | −14.910 dB | 4.088 dB | −28.762 dBc | **REJECT** |
  | 44.1 kHz | 4× | −27.045 dB | 0.867 dB | −47.635 dBc | **REJECT** |
  | 48 kHz | 1× | −4.640 dB | 22.893 dB | −28.871 dBc | **REJECT** |
  | 48 kHz | 2× | −16.426 dB | 3.257 dB | −31.329 dBc | **REJECT** |
  | 48 kHz | 4× | −28.181 dB | 0.708 dB | −38.189 dBc | **REJECT** |

  The factor-independent oracle/support self-checks still pass, including the
  updated 0/23.5/35.25-frame 1×/2×/4× candidate advances. These are
  numerical-equation tests, the 4× render remains a candidate rather than
  truth, and the surviving VCF/BBD rejections admit no split-rate engine.

  H=24 and the 95-tap half-band change the numerical centres. The raw 1×/2×/4×
  centres are 24/35.5/41.25 host samples; pads of 17/6/0 make them
  41/41.5/41.25. The engine and processor therefore report one fixed 41-sample
  latency, with every nominal centre within 0.5 sample: 0.930 ms at 44.1 kHz,
  0.854 ms at 48 kHz, 0.427 ms at 96 kHz and 0.214 ms at 192 kHz.
  The exhaustive 48 kHz playing-latency rerun leaves Pitch, VoiceVca and held
  63.2% distributions unchanged. Its signal-dependent −80 dBFS output proxy
  is now 87/210/335 HQ-off and 105/228/351 HQ-on; subtracting the fixed report
  gives 46/169/294 and 64/187/310. That threshold sees factor-dependent
  symmetric pre-ringing and is not an exact group-delay or audibility measure.

  The updated 2,048-frame work audit counts 301,056 nonzero half-band visits
  and 602,112 stereo MACs across 6,144 decimator calls at 48 kHz/4×—49 and 98
  per call. VCF iterations are 205,008/56,604 at 48 kHz 4×/1×,
  104,146/53,679 at 96 kHz 2×/1×, and 51,508 at 192 kHz 1×, all with zero
  recovery. An informational rerun on the same M1 Max/arm64 Release protocol
  gives 4×/1× CPU/audio ratios 3.543 idle, 3.177 six-voice plain, 3.471
  six-voice resonant and 2.655 full-mixer Chorus II. These are observations,
  not performance gates or evidence for a rate split.

  **Verdict: the isolated DCO numerical defect is fixed, with no hardware
  question silently answered.** OQ-07 (hold acquisition), OQ-08 (physical
  write/restart timing), OQ-11 (pinned comparator leg) and OQ-15 (loaded
  oscillator/mixer levels) remain open. Numerical agreement with the declared
  model cannot substitute for an original-unit capture.
- [x] **8. Replace linear BBD input-edge interpolation with a causal four-point
  reconstruction.** Step 7's six BBD rows above remain the dated before-state.
  At each already-scheduled BBD clock edge, the line now evaluates the unique
  four-point Lagrange polynomial through the current input-support sample and
  its three predecessors instead of linearly interpolating the current and
  previous samples. This is a numerical input sampler, not a physical BBD law:
  it uses no future sample or lookahead and adds no output delay. The edge
  timestamp and phase, 128-stage bucket count/index progression,
  `transferLossStep` law and cadence, output-step polyBLEP, component support
  filters, physical constants and RNG sequence are unchanged. Corrected signal
  values written into the buckets and propagated to the held output
  intentionally differ. It is also distinct
  from a fractional BBD read-tap or Thiran allpass; neither is added here.

  Gabrielli, D'Angelo and Squartini's official DAFx-25
  [paper](https://dafx.de/paper-archive/2025/DAFx25_paper_29.pdf) and
  [companion](https://dangelo.audio/dafx25-bbd) continue to support the BGA/SGA
  distinction and numerical antialiasing method family. They do not establish
  this MN3009 model's physical response or these implementation-specific
  figures. The unchanged common-host audit reads:

  | Host | Factor | Analytic NRMS, Step 7 → Step 8 | BGA error, Step 7 → Step 8 | SGA, Step 7 → Step 8 | Result |
  | ---: | ---: | ---: | ---: | ---: | --- |
  | 44.1 kHz | 1× | −3.099 → −3.602 dB | 34.389 → 34.362 dB | −24.854 → −26.765 dBc | **REJECT** |
  | 44.1 kHz | 2× | −14.910 → −18.159 dB | 4.088 → 4.080 dB | −28.762 → −41.304 dBc | **REJECT** |
  | 44.1 kHz | 4× | −27.045 → −30.394 dB | 0.867 → 0.865 dB | −47.635 → **−72.041 dBc** | **REJECT** |
  | 48 kHz | 1× | −4.640 → −5.768 dB | 22.893 → 22.866 dB | −28.871 → −30.364 dBc | **REJECT** |
  | 48 kHz | 2× | −16.426 → −19.696 dB | 3.257 → 3.249 dB | −31.329 → −45.866 dBc | **REJECT** |
  | 48 kHz | 4× | −28.181 → −31.847 dB | 0.708 → **0.706 dB** | −38.189 → **−65.597 dBc** | **REJECT** |

  The absolute gates remain analytic NRMS ≤−40 dB, qualifying-line BGA error
  ≤0.75 dB and unmasked SGA <−60 dBc. The 4× SGA column now passes at both
  hosts, and the 48 kHz/4× BGA column passes, but NRMS rejects every cell and
  44.1 kHz/4× still misses BGA by 0.115 dB. The near-static BGA column also
  verifies that this correction is not a hidden retune of wanted physical
  images. With no support-filter change in this step, the remaining 4× error
  stays assigned to the sampled support path rather than to an invented MN3009
  constant.

  **Verdict: the causal input-edge sampler fixes the decisive 4× SGA failure,
  but no BBD rate is qualified and no selector changes.** OQ-01 (clock and
  delay law), OQ-03 (noise mechanism), OQ-04 (physical loaded transfer/support)
  and OQ-20 (wet switching) explicitly remain open. Character 1,
  inter-domain reconstruction and whole-engine output remain outside this
  isolated numerical result.

  A sequential same-machine thread-CPU rerun measured the full-mixer Chorus-II
  fixture at 0.758×/0.284× realtime for the detached Step-7 baseline and
  0.753×/0.284× for this step at 4×/1×. That difference is inside run
  variability; the unchanged coarse real-time budget remains the gate.

  The documentation render was regenerated as one ten-file set; only wet demos
  01, 02, 06 and 07 changed. The complete 128-tone factory audit and all ten
  common-gain previews were regenerated; corpus flags stayed unchanged and the
  shared non-boosting gain moved from 0.542977 to 0.542974. The older strict
  host-grid listening comparison remains frozen: its 925,348-frame baseline
  predates the current derived clock schedule, which renders 891,964 frames,
  and the tool correctly refuses to align them. The current common-host oracle,
  not that archived null, controls this step's numerical claim.
- [x] **9. Replace the remaining BBD support-chain warping with a combined
  continuous-state transition, without moving a physical constant.** Steps 1–8
  above remain the dated history. The exact-transition formulation represents
  each input or output side as one six-state physical network rather than a
  sequence of separately warped TPT sections; production uses it for every
  output grid and for input only at internal rates ≥176.4 kHz.
  A prepare-only Higham Pade-13 scaling-and-squaring exponential builds a
  10×10 augmented transition for the sample interval and for the declared
  causal cubic through the current and three preceding samples. The audio loop
  performs only the fixed state/drive multiply. It reads no future sample,
  adds no lookahead and leaves the fixed 41-sample latency unchanged.

  Output support uses the exact transition on every accepted grid. Input
  support uses it at internal rates ≥176.4 kHz and retains Step 8's reviewed
  TPT input path below; a full-exact low-grid candidate was rejected because it
  worsened SGA. Muted and connected output loads are two prepared transitions
  in the same physical coordinate system, so Off/on wet-mute switching changes
  the matrix, not the meaning of state; I↔II remains connected. Dead output TPT
  coefficients/carries are removed.
  A guarded quality-rate rebuild still clears support and cubic histories under
  zero gain while preserving BBD buckets, clocks and RNG. Exact support state is
  a vector of physical voltages rather than timestep-embedded carries, but
  preservation/reseeding has not been qualified and is not claimed.

  The matrix construction agrees with an independent SciPy exponential within
  1.78e-15 at the selected rates and 2.22e-15 across a 601-rate scan; DC
  identities are within 2.22e-16. The maximum Padé denominator condition number
  is 15.03, at most three squarings are required and the worst transition pole
  radius is 0.999940816. Circuit coverage spans 8, 44.1, 48, 176.4, 192 and
  768 kHz, the adjacent-float 176.4 kHz selector fence, exact DC/RK4 agreement,
  muted/connected switching, hostile-input recovery and the deliberate reset.

  This oracle covers four deterministic low-drive cases and exactly one
  qualifying BGA line per cell; it is not nonlinear whole-line coverage. The
  unchanged common-host absolute gates are analytic NRMS ≤−40 dB,
  qualifying BGA error ≤0.75 dB and unmasked SGA <−60 dBc:

  | Host | Factor | Analytic NRMS | BGA error | SGA | Result |
  | ---: | ---: | ---: | ---: | ---: | --- |
  | 44.1 kHz | 1× | −3.511 dB | 4.764 dB | −26.934 dBc | **REJECT** |
  | 44.1 kHz | 2× | −18.390 dB | 0.070 dB | −41.304 dBc | **REJECT** |
  | 44.1 kHz | 4× | **−53.442 dB** | **0.011 dB** | **−71.831 dBc** | **PASS** |
  | 48 kHz | 1× | −5.263 dB | 3.406 dB | −30.746 dBc | **REJECT** |
  | 48 kHz | 2× | −20.051 dB | 0.016 dB | −46.044 dBc | **REJECT** |
  | 48 kHz | 4× | **−56.101 dB** | **0.008 dB** | **−65.381 dBc** | **PASS** |

  The common-host q4 cells now pass in full; lower factors remain absolute
  rejections. A second matrix follows the actual engine selector instead of
  treating the two common hosts as the whole product:

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

  The HQ-off limits were declared before inspection: at most +0.75 dB NRMS,
  +0.25 dB BGA error and +1.5 dB SGA relative to frozen Step 8. Those four
  results are compatibility passes, not absolute numerical admission. Thus the
  bounded low-drive fixture passes on all six actual HQ paths and no lower rate
  is relabelled or promoted. It is not a nonlinear whole-line qualification.

  The exact output path changes its derived noise transfer, so the old 0.4034
  factor is not carried forward. The current value is
  **0.4026 = 1/√3 × 0.6973**, deriving
  `independentLineRandomAmplitude = 1.9106577e-4` while keeping the MN3009
  0.2 mVrms row, 2.6 V coordinate, RNG and mode-II factor unchanged. After
  dividing out that separate factor, recovered I/II values are
  0.200059/0.200078 mVrms at 176.4 kHz and
  0.200006/0.200020 at 192 kHz, under 0.004 dB across HQ. HQ-off pairs are
  0.208558/0.208917, 0.206982/0.207251, 0.201452/0.201763 and
  0.201218/0.201584 mVrms at 44.1/48/88.2/96 kHz. Five PSD-band changes
  versus Step 8 are within 0.147 dB and unweighted RMS within 0.070 dB.

  The compile-time audit names the added work. Over 2,048 host frames and two
  lines, 48 kHz HQ counts 16,384 exact input plus 16,384 exact output advances,
  196,608 coordinate updates and 1,966,080 MACs; HQ-off counts 4,096 legacy
  input frames plus 4,096 exact output advances, 24,576 updates and 245,760
  MACs. The 2× exact path counts 8,192 advances on each side, 98,304 updates and
  983,040 MACs; a high-rate exact 1× path counts 4,096 each, 49,152 and
  491,520. Every exact advance is six coordinates/60 MACs, and normal/counter
  builds remain raw-float identical.

  A seven-run current M1 Max observation remains below realtime at 4×/1×:
  0.578/0.167 idle, 0.547/0.178 plain, 0.719/0.205 resonant and
  0.830/0.315 full-mixer Chorus II. The final 0.682667-second window's median
  CPU times are 566.779/215.019 ms (MAD 1.735/3.640 ms). Alternating
  Step-8/current runs were load/thermal sensitive, so no paired speedup or
  regression is inferred; the hard CPU suite owns the gate.

  Documentation artifacts were regenerated from the final DSP. Exactly wet
  demos 01/02/06/07 changed; the other six remained byte-identical, and only
  demo 01's displayed peak moved (−4.2/+1.2 to −4.3/+1.3 dB). All ten factory
  previews changed under one common gain, 0.542974 → 0.543119 (−5.30 dB
  rounded). All 128 factory rows remain finite; the summary is unchanged at
  32 over-zero, zero silent and nine balance outliers, median −21.48 dBFS.
  The largest peak delta is 0.027693 dB (B86), the largest gated-RMS delta
  0.003771 dB (B27), and no classification changes.

  **Verdict: the BBD support integration makes the four-case low-drive fixture
  pass on all six shipping HQ paths, without pretending its independent
  declared-model oracle is nonlinear whole-line or hardware truth.** OQ-01's
  clock/delay trajectory, OQ-03's physical
  noise mechanism, OQ-04's loaded MN3009/support response and OQ-20's wet-mute
  switching remain explicitly open. No hardware constant, global oversampling-factor selector,
  split-domain architecture or latency changes.

- [x] **10. Advance the declared continuous VCF equations directly with fixed,
  bounded work.** Steps 1–9 remain the dated history above. The former
  float, path-averaged, capped Newton discretization is replaced by two fixed
  half-interval, five-evaluation Merson RK4 advances over the same continuous
  four-stage OTA equations. The four capacitor voltages are the complete
  physical state and are stored in double precision. Each internal VCF step
  has exactly 2 integration substeps, 10 right-hand-side/feedback evaluations,
  40 stage evaluations, 40 full Early-effect evaluations when enabled, and 7
  input-reconstruction phases. There is no tolerance loop, solver selector or
  data-dependent retry.

  The drive at those seven unique abscissae is reconstructed by the causal
  polynomial through the current endpoint and three predecessors; startup
  ramps linear to quadratic to cubic as real history becomes available.
  Cutoff, resonance and thermal headroom move linearly between their previous
  and current endpoints at the same abscissae. No future sample is read, no
  lookahead is added and the fixed 41-host-sample latency report is unchanged.
  A quality-rate change preserves the four physical capacitor voltages and the
  shared input endpoint, discards old-grid spacing history under the existing
  zero-gain transition, and refills it in order. The card thermal scale is
  applied before the product-grid bound, whose final interval cap is
  `omega·dt = 0.9π` (0.45 cycles per internal sample).

  The existing common-host VCF fixture keeps its independent fixed-q16
  RK64/RK128 oracle, filters every candidate to the same host boundary and
  keeps all gates unchanged. Its current nominal Character-0 matrix is:

  | Host | Factor | Hot RK NRMS | Driven RK NRMS | Hot residual off-mask | Verdict |
  | ---: | ---: | ---: | ---: | ---: | --- |
  | 44.1 kHz | 1× | −12.538 dB | −145.593 dB | −44.602 dBc | **REJECT** |
  | 44.1 kHz | 2× | −30.414 dB | −113.526 dB | −85.968 dBc | **REJECT** |
  | 44.1 kHz | 4× | **−50.351 dB** | **−112.144 dB** | **−133.278 dBc** | **PASS** |
  | 48 kHz | 1× | −14.269 dB | −144.364 dB | −48.081 dBc | **REJECT** |
  | 48 kHz | 2× | −33.028 dB | −114.710 dB | −88.898 dBc | **REJECT** |
  | 48 kHz | 4× | **−50.064 dB** | **−113.339 dB** | **−140.552 dBc** | **PASS** |

  Thus the classification is REJECT/REJECT/PASS at each common host. The
  self-oscillation pitch error is 0.000/0.000/0.000 cents at 44.1 kHz and
  0.001/0.001/0.001 cents at 48 kHz; its level error is at most 0.010 dB.
  Passing q4 is a numerical admission of this declared-model fixture, not a
  claim that 4× is truth or that the declared circuit matches hardware.

  The focused integrator contract independently evaluates the full Early
  effect and endpoint trajectories with RK96. Its primary dynamic comparison
  is **−162.551 dB / 4.21471e-8 V**, an alternating-control trajectory reads
  **−95.2005 dB**, and every cold/warm product-cap tail is exactly zero for all
  six actual cards at Unit Character/calibration 2. The shipping-schedule
  audit then drives the exact 23-write order, 522 µs holds and all actual card
  profiles. Nineteen physical takes per rate family cover 24 logical profiles
  because exact Character-0 collapses are reused:

  | Actual HQ selector cell | Candidate vs independent oracle | Admission |
  | --- | ---: | --- |
  | 44.1 kHz / 4× | −48.585 dB | **PASS** |
  | 48 kHz / 4× | −48.724 dB | **PASS** |
  | 88.2 kHz / 2× | −48.557 dB | **PASS** |
  | 96 kHz / 2× | −48.514 dB | **PASS** |
  | 176.4 kHz / 1× | −48.324 dB | **PASS** |
  | 192 kHz / 1× | −48.293 dB | **PASS** |

  RK64/RK128 convergence is at or below −150.9 dB in that matrix, with zero
  recovery, schedule or count mismatch. This is the primary admission: all six
  standard HQ paths clear the predeclared −40 dB dynamic gate with the actual
  converter schedule and card mechanisms active.

  The engine-bound extension is deliberately not hidden. A 768 kHz/1× cell
  passes at **−61.360 dB**; the 8 kHz/4× endpoint is an **expected REJECT** at
  **−33.245 dB**, even though its oracle convergence is −139.820 dB and every
  sample/count remains finite and exact. Diagnostic variants localise that
  result: the maximum converter-event snap is 30.978 µs, while the named
  steady, continuous-omega/resonance/headroom and all-exact controls read
  −136.916, −60.546 and −100.713 dB respectively. The next atomic step is
  therefore fractional, event-aware hold evaluation at the lower endpoint,
  not relaxing the −40 dB gate.

  Merson was chosen by bake-off, not declared uniformly superior. A fixed
  three-substep classical RK4 candidate is about 0.55 dB better in the
  44.1 kHz/1× hot high-mu transient, but that cell still rejects. The Merson
  path wins the primary HQ dynamic matrix and the reviewed damping,
  Hopf/onset and product-cap stability checks while removing the runtime
  selector entirely.

  Compile-time work attribution now describes the shipping integrator instead
  of the historical Newton/path-average solve. In the six-card resonant
  2,048-host-frame window, the exact VCF counts are:

  | 48 kHz path | VCF steps | Merson substeps | RHS / feedback | Stage / full-Early | Input phases | Recovery |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: |
  | HQ, 4× | 49,152 | 98,304 | 491,520 / 491,520 | 1,966,080 / 1,966,080 | 344,064 | 0 |
  | HQ off, 1× | 12,288 | 24,576 | 122,880 / 122,880 | 491,520 / 491,520 | 86,016 | 0 |

  Three alternating seven-repetition thread-CPU audits at 48 kHz/block 256
  give these median-of-run-medians. They are informational engine measurements
  on one M1 Max, not performance gates or competitor data:

  | Scenario | Step 9 → Step 10, 4× | Change | Step 9 → Step 10, 1× | Change |
  | --- | ---: | ---: | ---: | ---: |
  | Idle | 0.546 → **0.677×** | +24.0% | 0.152 → **0.170×** | +11.8% |
  | Six-voice plain | 0.509 → **0.690×** | +35.6% | 0.159 → **0.178×** | +11.9% |
  | Six-voice resonant | 0.670 → **0.850×** | +26.9% | 0.192 → **0.228×** | +18.8% |
  | Full mixer, Chorus II | 0.786 → **0.736×** | −6.4% | 0.296 → **0.191×** | −35.5% |

  Per-run MADs are small and every unrounded current row is below 0.85×
  realtime (the resonant row prints 0.850 at three decimals). The coarse
  `<5×` Release CPU runaway gate is unchanged. Heterogeneous
  patch results do not support a blanket speed claim. The build now registers
  **11 JUCE-free CTest contracts**, including separate focused-integrator and
  dynamic-VCF contracts.

  **Verdict: admit the fixed Merson realization on all standard HQ paths, and
  carry the 8 kHz event-timing miss into Step 11.** This step changes only the
  numerical realization of the already declared ODE. It supplies no hardware
  validation and changes no resonance law, input-drive calibration, cutoff
  law, six-card evidence class or physical hold timing. OQ-09, OQ-10, OQ-15,
  OQ-16, OQ-18 and OQ-19 remain open; the 0.45·Fs bound is product/numerical
  policy, not a property of a JUNO-106.

- [x] **11. Evaluate VCF and shared-resonance holds at their fractional
  compatibility-policy event times.** Steps 1–10 remain the dated history
  above, including Step 10's diagnostic 8 kHz rejection before this change.
  The converter still runs the exact 23-write queue and the
  `NormalizedServiceChart` still places write ordinal `n` at `n/23` of the
  4.2 ms pass. This step does not claim that those normalized offsets are the
  hardware timestamps. It changes only how the existing 522 µs VCF and shared
  RESONANCE holds are evaluated when one of those policy events falls inside
  an audio interval; DCO, VCA, PWM, SUB, NOISE and converter cursor/order/count
  behavior are unchanged.

  Before rendering an interval, the engine purely peeks for the next relevant
  event in `(phase, phase + delta]`, including the shared-resonance event across
  a pass wrap. The peek does not consume the official firmware scheduler or
  change its target. It latches the converter payload at that event time,
  advances the affected RC endpoint and its seven unique Merson control nodes
  by the exact segmented exponential, and lets the next normal scheduler poll
  commit that latched payload exactly once. Thus automation arriving between
  the fractional event and the later poll cannot rewrite history. Dedicated
  card-VCF and next-pass shared-resonance tests fence pure-peek cursor/target
  state, pass wrap, event-time payload retention and single retirement.

  Exact node trajectories are supplied only for intervals containing a VCF or
  shared-resonance event. The fixed integrator remains two half-interval
  Merson steps with ten right-hand-side evaluations and no split, retry or
  selector. Each affected voice interval evaluates the same seven unique
  Merson nodes and adds six nonlinear control maps; ordinary intervals retain
  Step 10's endpoint-linear path. A production `renderVoice` replay contract
  is bit-exact when that trajectory is connected, while an explicit
  `nullptr` wiring mutation must differ, so a local helper that is never wired
  into the shipping voice cannot pass unnoticed.

  The dynamic audit retains its independent RK64/RK128 oracle, the exact
  23-write order, all six cards and cold/warm Character profiles. Nineteen
  physical takes per rate family still cover 24 logical profiles. The
  predeclared quality gate is unchanged at −40 dB:

  | Engine-bound selector coverage | Candidate vs independent oracle | Admission |
  | --- | ---: | --- |
  | 8 kHz / 4× | **−84.881 dB** | **PASS** |
  | Six standard HQ paths | **worst −112.406 dB; best −116.317 dB** | **PASS** |
  | 768 kHz / 1× | **−119.340 dB** | **PASS** |

  Both deliberately broken timing controls remain finite and structurally
  exact but fail the unchanged gate: snapping events late with `ceil` reads
  **−33.245 dB**, and snapping them early with `floor` reads **−32.007 dB**.
  Scheduler peek/latch/commit, pass-wrap, write/count and control-contract
  checks all pass, as do the real-voice wiring and rejected-null-mutation
  checks. In the 48 kHz, six-card resonant 2,048-frame work window the new
  semantic counters record **70 peeks, 70 commits, 120 exact-control voice
  intervals, 840 exact nodes and 720 extra nonlinear maps**. These counts add
  no right-hand-side evaluation and change no recovery behavior.

  Three fresh Step-10/current alternating pairs, each with seven repetitions
  at 48 kHz/block 256, give these thread-CPU meta-medians. The percentage is
  computed only inside this paired cohort; its Step-10 values are not the
  earlier standalone medians printed in Step 10:

  | Scenario | Step 10 → Step 11, 4× | Change | Step 10 → Step 11, 1× | Change |
  | --- | ---: | ---: | ---: | ---: |
  | Idle | 0.653 → **0.666×** | +2.056% | 0.164 → **0.169×** | +3.072% |
  | Six-voice plain | 0.670 → **0.682×** | +1.856% | 0.172 → **0.176×** | +2.483% |
  | Six-voice resonant | 0.823 → **0.832×** | +1.096% | 0.221 → **0.225×** | +1.807% |
  | Full mixer, Chorus II | 0.706 → **0.719×** | +1.755% | 0.184 → **0.188×** | +2.182% |

  The worst current observation is 0.832× realtime; every case stays below
  1× and the hard Engine CPU gate passes. These measurements are
  informational and machine/patch specific, not a universal performance claim.

  The user-authorized audio reset treats this as a new canonical corpus rather
  than comparing against legacy renders. Four frozen-binary passes all exit 0;
  the two demo passes are byte-identical to each other, as are the two complete
  factory passes. The canonical tree contains exactly 23 files and has manifest
  SHA-256
  `764f2770d21a138163c756025551dc8ead7925f4cf003eb98e960234afc098ea`.
  Its 20 WAVs are finite stereo PCM16: demos at 44.1 kHz and factory previews at
  48 kHz. Maximum absolute DC is 0.000000576 FS and the worst file edge is
  −46.96 dBFS. The factory report contains 128 finite, unique rows/tone blobs,
  with median gated RMS −21.48 dBFS, 31 rows containing samples above 0 dBFS,
  zero near-silent rows and nine rows outside ±18 dB of the corpus median. This
  is a deterministic from-scratch qualification; no legacy audio delta is
  claimed.

  **Verdict: admit fractional event-aware VCF/shared-resonance hold evaluation
  across the complete engine-bound rate matrix without relaxing the numerical
  gate.** This is a more faithful realization of the existing normalized
  compatibility policy, not new hardware timing or acquisition evidence. It
  changes no physical constant, converter pass/order, oversampling selector,
  split-domain boundary or fixed 41-sample latency. Exact intra-pass offsets
  remain policy; OQ-07 remains open on acquisition, droop and true hold laws,
  and OQ-08 remains open on physical timestamps, jitter and restart behavior.

- [x] **12. Resolve fractional timing for the remaining evidence-backed
  passive holds.** Steps 1–11 remain the dated history above. The converter
  still executes the same 23 writes over the same nominal 4.2 ms pass, and
  `NormalizedServiceChart` still places ordinal `n` at policy offset `n/23`.
  This step generalizes Step 11's pure peek/latch/once-only-commit mechanism to
  16 passive destinations per pass: shared RESONANCE, VCA LEVEL, SUB and PWM,
  plus six VCF and six VoiceVca writes. Step 11's VCF/shared-resonance 522 µs
  behavior is retained; no recovered hardware timestamp is claimed.

  The new exact paths are deliberately bounded by evidence. Six per-card
  VoiceVca holds use the component-derived **687 µs** constant. The common
  VCA LEVEL uses its designator-derived **9.08249 ms** pole, SUB its derived
  **10 ms** pole, and PWM the exact continuous affine solution of its
  **4.7/2.632 ms** two-pole cascade. Each state advances for the old-target
  fraction and new-target fraction around an event, while the official
  scheduler commits the latched payload at its next ordinary poll. The PWM
  cascade is solved continuously rather than as two sequential discrete
  one-pole endpoint updates.

  Six Pitch/DCO writes remain on their sample-grid path because the physical
  timer write is coupled to the unresolved ramp, comparator and sub-divider
  restart state in OQ-08. NOISE also remains sample-grid because its held-law
  and source/level coordinates are still owned by OQ-07/OQ-15/OQ-16. The six
  VoiceVca states, common-VCA state, SUB state and two PWM states are promoted
  from `float` to `double` so high-rate exponential tails cannot stall. This
  changes representation only: physical state dimension, converter order,
  selector, split-domain boundary, future-sample policy and latency are all
  unchanged.

  The independent passive-hold audit uses long-double piecewise one-pole and
  exact affine two-pole oracles. It covers **1,105** actual
  `Engine::process` cases plus **17** block-wrap cases, including an event in a
  later q4 internal substep. Maximum process error is **4.440892e-16**;
  state-to-float and
  consumer comparisons are **0 ULP**. It observes all **23** ordinals,
  classifies exactly **16** passive destinations, and records zero Pitch/NOISE
  peeks, duplicate peeks, payload failures, cursor/order failures or pass-wrap
  failures.

  Per-destination timing and wiring mutations cannot hide behind a global
  maximum:

  | Destination | Late/ceil | Early/floor | Disconnected |
  | --- | ---: | ---: | ---: |
  | Common VCA | 0.009838067 | 0.009838067 | 0.009838067 |
  | SUB | 0.008684780 | 0.008684780 | 0.008684780 |
  | PWM | 0.02180667 | 0.02180667 | 0.02180667 |
  | VoiceVca | 0.1480581 | 0.1480581 | 0.1480581 |

  A separate sequential-PWM mutation differs by **0.000525998**. The common-
  VCA consumer spans 495 samples, agrees within **8.961428e-7** relative and
  moves by **0.7034001** when disconnected; SUB is exact at printed precision
  and its disconnected consumer moves by **0.6666667**. These contracts fence
  both the physical states and their real production consumers.

  The 48 kHz, six-card resonant 2,048-frame work window now reads:

  | Semantic work | HQ 4× | HQ-off 1× | Contract |
  | --- | ---: | ---: | --- |
  | Internal frames | 8,192 | 2,048 | factor-scaled |
  | Passive peeks / commits | 160 / 160 | 160 / 160 | 16-per-pass invariant |
  | VCF/resonance peeks / commits | 70 / 70 | 70 / 70 | Step-11 subset unchanged |
  | Exact VCF intervals / nodes / maps | 120 / 840 / 720 | 120 / 840 / 720 | invariant |
  | VCF steps / Merson halfsteps | 49,152 / 98,304 | 12,288 / 24,576 | fixed factor scaling |
  | VCF RHS / feedback | 491,520 / 491,520 | 122,880 / 122,880 | fixed solve unchanged |
  | BBD line frames / shifts | 16,384 / 3,162 | 4,096 / 3,162 | frame-scaled / physical invariant |

  Equal-wall-time passive and VCF-subset invariance also pass for the
  44.1/48/88.2/96 kHz factor pairs. The dated common-host VCF/BBD matrix stays
  REJECT/REJECT/PASS at 1×/2×/4× for both 44.1 and 48 kHz, and Step 11's
  dynamic VCF results and fixed Merson work remain unchanged. This is a scalar
  passive-state correction, not a VCF or BBD quality reclassification.

  The exhaustive 48 kHz playing-latency characterization now distinguishes a
  physical fractional VoiceVca event from its later official target commit.
  Pitch remains 0/100/201 samples and the commit remains 70/192/315 in both
  modes. HQ-off physical write and first gain occur at **69/191/314**, one host
  frame earlier; HQ-on still observes both at 70/192/315. The fixed host report
  remains **41 samples**. These are model coordinates for one declared fixture,
  not a JUNO-106 event-to-output measurement.

  Three alternating seven-repetition Step-11/current pairs at 48 kHz/block 256
  give these thread-CPU meta-medians:

  | Scenario | Step 11 → Step 12, 4× | Change | Step 11 → Step 12, 1× | Change |
  | --- | ---: | ---: | ---: | ---: |
  | Idle | 0.677068 → **0.682068×** | +0.738406% | 0.171473 → **0.172614×** | +0.665476% |
  | Six-voice plain | 0.697359 → **0.696475×** | −0.126874% | 0.179268 → **0.180543×** | +0.711718% |
  | Six-voice resonant | 0.847179 → **0.853898×** | +0.793131% | 0.228095 → **0.231158×** | +1.342855% |
  | Full mixer, Chorus II | 0.731646 → **0.737013×** | +0.733578% | 0.191505 → **0.192318×** | +0.424526% |

  Worst current load is **0.853898×** realtime and worst paired regression
  is **+1.342855%**; the predeclared `<1×` and `+5%` gates pass. The figures
  are informational machine/patch-specific measurements.

  A fresh warning-clean native Release/plugin-off build registers **12
  JUCE-free CTest contracts** and passes **12/12 in 323.07 s**, including the
  new passive-hold contract. Five focused ASan+UBSan gates pass with
  halt-on-error and no diagnostics: Engine passive-hold-only, independent
  passive hold, full DCO quality, oversampling normal/work parity and dynamic
  VCF, in 0.55/0.48/68.31/45.36/17.88 s respectively. The universal
  Release/plugin-on build passes **13/13 in 344.05 s**.
  VST3, AU and Standalone each contain `x86_64 arm64`, pass strict/deep ad-hoc
  signature verification after packaging and target macOS 11.0. A genuinely
  translated Rosetta `x86_64` passive-hold run
  passes in **0.55 s**. The only universal warnings are two inherited Step-11
  `-Wfloat-equal` sites and nested-Make's jobserver notice; Step 12 adds none.

  The user-authorized Step-12 audio regeneration is complete. The twin demo and
  factory manifests are
  `6e953be720d71a4947d41f4aa848dd228078b919520f7fccf006f27a19136667`
  and
  `dec0d91c6f2012519d713743e7c897c37d3c5cace2cec5db9e4648039791d57e`;
  the canonical 23-file manifest is
  `f9a6b274e7efb857a712ecaed1061e5251bd554e22462adce986e5e4d8158cbd`.
  Its 20 finite stereo PCM16 WAVs have maximum absolute DC
  `0.000000592814 FS` and worst edge `−46.962652 dBFS`. The factory audit's
  exact median is `−21.480711305 dBFS`, with 31 overload rows, zero silent
  rows, nine `±18 dB` outliers and common gain `0.543091`. Relative to dated
  Step 11 the median moves `+0.000034651 dB`, the common gain moves
  `0.543089 → 0.543091`, and the largest sample-peak movement is B77's displayed peak,
  `+1.022040722 → +0.806945831 dBFS`; eight displayed rows and all WAV bytes
  change.
  No audibility inference is made from those byte/metric deltas.

  **Numerical verdict: admit exact fractional evaluation for the supported
  passive scalar laws.** This is a more faithful realization of the existing
  normalized compatibility policy, not hardware timing evidence. OQ-07 remains
  open on acquisition, droop, loading and hold interpretation; OQ-08 remains
  open on physical timestamps, jitter and DCO restart behavior.

- [x] **13. Qualify the actual HQ-off VCF boundary without changing it.**
  Steps 10–12 remain the dated implementation history above. This step changes
  only `Tools/AuditVcfDynamicQuality.cpp`: no production equation, selector,
  state, delay, preset, CMake target or audio sample changes. It asks two
  deliberately separate questions so a smooth signal cannot admit a nonlinear
  lower grid.

  The moving-control side repeats the existing four stored-byte snapshots over
  **12 converter passes**. At each rate it exercises the exact 23-write order,
  fractional 522 µs VCF/resonance trajectories, 19 physical takes representing
  24 logical card/Character/thermal profiles, independent RK64/RK128 references
  and the actual HQ-off selector. The real scheduler and `renderVoice`
  trajectory seam are probed at 8/44.1/48/88.2/96 kHz; a disconnected
  trajectory must diverge. Every row has zero recovery, write/count or control-
  contract mismatch. The −40 dB candidate-NRMS and −80 dB reference-
  convergence gates are unchanged:

  | HQ-off q1 moving-control row | Worst NRMS | RK64/RK128 | Minimum compared frames | Result |
  | --- | ---: | ---: | ---: | --- |
  | 8 kHz supported selector endpoint | −53.279 dB | −110.051 dB | 147 | **PASS — moving only** |
  | 44.1 kHz standard host | −84.738 dB | −142.698 dB | 1,198 | **PASS** |
  | 48 kHz standard host | −86.568 dB | −144.403 dB | 1,395 | **PASS** |
  | 88.2 kHz standard host | −97.893 dB | −154.666 dB | 3,421 | **PASS** |
  | 96 kHz standard host | −99.618 dB | −157.689 dB | 3,814 | **PASS** |

  The 8 kHz row is not silently promoted into the standard set. Its late/ceil
  and early/floor event-snap mutations remain structurally exact but reject at
  **−27.259 and −26.860 dB**. Moving NRMS and both mutation metrics are frozen
  with ±0.05 dB bands. Moving RK64/RK128 convergence uses a separate ±0.15 dB
  cross-architecture fingerprint band at its roughly −110…−158 dB numerical
  floor; the unchanged −80 dB convergence admission gate is not relaxed. Raw
  hashes are compared only within one run, not frozen across architectures.

  The second side reuses the nominal Character-0 nonlinear fixture: an analytic
  19-harmonic, 20 kHz-band-limited 1,046.502 Hz saw at the production-
  compensated 2.4 V coordinate, 16 kHz cutoff and `k=3.8`. It is **static
  nominal coverage, not a hot × 19/24 scheduled cross-product**. Independent
  RK64/RK128, a 4,097-tap oracle boundary, a 32,768-frame exhaustive spectrum,
  exact unmasked-bin counts, finite/reset/recovery/selector checks and filter-
  response convergence are structural gates. Quality requires waveform NRMS
  ≤−40 dB and residual off-mask <−60 dBc; the oracle control must remain
  ≤−85 dBc. The response grid keeps constant physical-Hz density at the doubled
  88.2/96 kHz family rates without weakening any response threshold.

  | Standard HQ-off q1 hot row | NRMS | RK64/RK128 | Residual / oracle off-mask | Unmasked bins | Combined result |
  | --- | ---: | ---: | ---: | ---: | --- |
  | 44.1 kHz | −12.538 dB | −135.643 dB | −44.602 / −93.242 dBc | 14,618 | **REJECT** |
  | 48 kHz | −14.269 dB | −138.574 dB | −48.081 / −93.163 dBc | 13,412 | **REJECT** |
  | 88.2 kHz | −30.417 dB | −159.637 dB | −85.765 / −97.212 dBc | 7,195 | **REJECT** |
  | 96 kHz | −33.080 dB | −162.578 dB | −88.712 / −97.141 dBc | 6,592 | **REJECT** |

  All four hot rows are structurally valid and all four fail waveform quality;
  44.1/48 kHz also fail the residual off-mask gate. Each standard classification
  is computed as `moving structural && moving quality && hot structural && hot
  quality`, so every standard HQ-off row is frozen **REJECT** even though its
  smooth moving-control side passes. Hot NRMS, convergence and both off-mask
  values use ±0.05 dB goldens. The 16 kHz hot fixture is undefined at an
  8 kHz host and is therefore not run or compared there.

  The result identifies a numerical limitation, not a JUNO-106 property:
  HQ-off supplies too few input/control samples before the nonlinear VCF grid,
  so interpolation cannot reconstruct nonlinear pre-grid foldback already
  created at q1. Increasing the host rate improves but does not clear the hot
  waveform gate. A future production candidate may keep only the VCF at q4 for
  44.1/48 kHz and q2 for 88.2/96 kHz while other domains remain q1, but it is
  not admitted by this audit. That candidate must, in one atomic qualification:

  - retain the same-host moving and hot gates, exact 23-write/fractional-
    trajectory coverage, the existing ±0.05 dB signal/mutation and ±0.15 dB
    moving-convergence fingerprint bands, actual production wiring, finite
    state and zero recovery at every 44.1/48/88.2/96 kHz boundary;
  - preserve the four capacitor voltages plus input/control history across
    quality and local-rate changes, with block-boundary and mutation tests;
  - account for the local decimator's fractional delay without changing the
    fixed **41-host-sample** report or adding lookahead, and prove impulse/block
    alignment at q4/q2→host boundaries;
  - qualify DCO, sub and shaped-noise transfer into the denser VCF grid,
    including wall-clock PSD/RNG-state invariance and the existing hot
    residual/oracle masks, rather than repeating q1 samples by assumption; and
  - publish paired whole-engine and VCF-only work/CPU results, keep the hard
    `<1×` realtime and `+5%` regression fences, and demonstrate a useful saving
    rather than inferring one from solve counts.

  A separate **non-shipping, low-drive deterministic BBD scratch** remains only
  a deferred research lead. Replacing its current q1 boundary with exact analog
  input-at-edge plus exact fractional output events moves NRMS/BGA/SGA from
  `−3.511/4.764/−26.934`, `−5.263/3.406/−30.746`,
  `−18.390/0.071/−41.304`, `−20.051/0.016/−46.044` dB at
  44.1/48/88.2/96 kHz to `−15.859/0.00123/−54.044`,
  `−18.344/0.00103/−53.747`, `−37.660/0.00071/−97.307` and
  `−40.408/0.00065/−96.646` dB. Only 96 kHz clears that scratch's complete
  gates; stochastic/noise state is unqualified. Nothing from it ships in Step
  13, and it does not reclassify the BBD.

  Final Step-13 portability qualification is green. A warning-clean native
  Release/plugin-off tree passes **12/12** contracts in **375.88 s** while the
  translated audit shares the machine; the registered dynamic contract also
  passes alone in **38.90 s**, and its focused ASan+UBSan self-test passes with
  `halt_on_error=1`, `detect_leaks=0` and no diagnostics. The universal
  `x86_64 arm64` audit binary
  passes natively on arm64 and under genuine Rosetta translation in
  **963.10 s**. The translated run exposed a 0.103 dB difference only at the
  moving convergence fingerprint's roughly −158 dB floor, motivating its
  explicit ±0.15 dB portability band without moving the −80 dB gate or any
  signal/hot/mutation band.

  **Verdict: HQ-off VCF remains rejected at every standard host under the
  complete smooth-plus-hot rule.** Step 13 is an audit/truth-table improvement,
  not DSP, latency, CPU, audio or hardware evidence. The Step-12 audio tree is
  untouched; its canonical manifest remains
  `f9a6b274e7efb857a712ecaed1061e5251bd554e22462adce986e5e4d8158cbd`.
