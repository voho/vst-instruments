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
   which is the right side of that argument — but the one *measured* structural
   property of the hardware's chorus noise, the 3.95 dB II−I level difference,
   is recorded in the queue as an unexplained lead and is not modelled.

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
renders, process CPU time:

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

Where the cost is, measured by profile rather than assumed:

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

### 2.3 The second gap: one measured chorus property is not modelled

OQ-03 records a structural 3.95 dB II−I noise-level difference measured on two
independent chip populations, and the 2026-08-07 pass named a candidate
mechanism — noise proportional to modulation rate, which mode II raises by
exactly the instrument's own 1.6234799 ratio, predicting 4.21 dB. The
implementation's per-line floor is mode-independent, so the model has **no**
delta at all. The calibrated capture that would confirm or kill the mechanism is
still owed, so it cannot ship on by default; but leaving it unimplemented also
leaves the one measured property of this circuit's noise unrepresented and
untestable.

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
  range the cascade drives them over, plus the unchanged service anchors
  (4.83 Vpp at 248.0 Hz), the Runge-Kutta reference solve and the fold-back
  fence.

- [x] **3. Scale the cascade's convergence test to the volts it measures.**
  *Closes:* the wasted-iteration finding of §2.2. The absolute `1.0e-7` step
  test becomes `1.0e-6 * (1 + max|V|)`, which is where single precision's own
  round-off floor sits, so the loop stops when it has converged instead of
  when it runs out of iterations. The 8-iteration cap is unchanged, so the
  worst-case residual cannot get larger.
  *Verified by:* a new engine-suite fence on the *ratio* of high-resonance to
  plain-patch render cost — a machine-independent quantity — plus a direct
  assertion that the converged step is inside the new bound.

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
  switch.** *Closes:* §2.3. A named, off-by-default `enableChorusRateNoise`
  scales each line's noise with its own modulation rate, so mode II sits
  `20·log10(1.6234799) = 4.21 dB` above mode I — the candidate the queue
  records against the measured 3.95 dB.
  *Verified by:* an engine-suite test measuring the rendered II−I floor
  difference with the switch on, and asserting bit-identical output with it off.

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

Stated exactly: the figures are CPU seconds per second of audio, so under 1.0
is faster than realtime. Two of the four scenarios crossed that line and two
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
condition — the CPU reports in §1 are user impressions, not benchmarks. What
the suite fences is correspondingly a *ratio*, not a time.

Steps 2 and 3 change the last bits of the rendered samples; steps 4 and 5's
default position do not. The measured difference from the pre-pass engine is
−102 dB RMS relative to signal on a plain six-voice patch and −95 dB on a full
chorus patch. On a self-oscillating patch it is −20 dB, which is what a limit
cycle does when its phase is perturbed at all: its amplitude and frequency
stay on the 4.83 Vpp / 248.0 Hz service anchors, which is what the suite
fences and what the instrument is calibrated against.

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
been renumbered 1–6; nothing is ticked, because implementation is the next
phase.

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
  measures what that costs here.
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
  is an axis neither this plan nor the comparative assessment has a row for.
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

**A literature absence.** The research contract cites Zavalishin, Stilson &
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
A-weighted** at the BBD node in the model's own 2.6 V coordinate — **+14.4 dB**
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
3. The datasheet's own two noise figures disagree by 10.5 dB — 0.2 mVrms
   **max** A-weighted against the ~59.7 µVrms implied by S/N 88 dB **typ** at
   1.5 Vrms. Landing the model exactly on a guaranteed worst case is a choice
   inside that bracket, not a derivation from it.

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

**Gap 6 — there is no AC coupling between the filter output and the VCA input,
so the model manufactures DC and then multiplies it by the envelope.**
`Source/DSP/YouKnow106Engine.cpp:3725-3741` (`renderVoice`:
`output = filtered * voice.vca * voltsToSample`, with no high-pass between
them). Roland draws VCF OUT pin 3 through **C59 1 µF/50 V NP** and the
VR27/R108 network to VCA IN pin 9 — already stated in this project's own README
ledger and research contract as hardware topology — and the service procedure
trims VR30/25/20/15/10/5 through 2.2 MΩ for **minimum thump**, which is
evidence Roland cared about this exact path. The model's per-voice coupling
sits only at the *module input* (`moduleCoupling`, C56/C50); every remaining
coupling stage is on the shared bus (`voiceBusCoupling_`,
`commonVcaInputCoupling_`, `outputCouplingLeft_/Right_`), all downstream of the
multiply. The topology claim is solid: C59 is recorded as **anchored** in the
research contract's voice-module VCA row and in `Docs/open-questions.md:1785-1795`,
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
`Source/DSP/YouKnow106Engine.cpp:3927-3929`
(`thermalWarmupSeconds_ += static_cast<float>(inverseOversampledRate_)`),
consumed at `:3702-3705` and reported by `getDisplayTemperatureC`
(`Source/DSP/YouKnow106Engine.h:204-212`). At 192 kHz the increment is
5.208e-6 s; once the float accumulator passes 128.0 its ULP is 1.526e-5, the
increment rounds away entirely and the accumulator freezes forever.

**[re-measured 2026-08-08]** The mechanism is confirmed exactly, by simulating
the float accumulation itself (`youknow106-therm.cpp`) rather than by rendering
hours of audio at every rate. The accumulator always freezes on a power-of-two
boundary, and *which* boundary depends on the internal rate:

| host rate / HQ | internal rate | increment | freezes at | after (audio) | temperature | OTA headroom |
|---|---|---|---|---|---|---|
| 48 kHz, HQ on | 192 kHz | 5.208e-6 | 128.0 | 118.64 s | **26.9886 C** (13.3%) | 6.4087 V |
| 48 kHz, HQ off | 48 kHz | 2.083e-5 | 512.0 | 474.54 s | **31.5077 C** (43.4%) | 6.5052 V |
| 44.1 kHz, HQ on | 176.4 kHz | 5.669e-6 | 128.0 | 126.82 s | 26.9886 C | 6.4087 V |
| **96 kHz, HQ on** | 384 kHz | 2.604e-6 | **64.0** | 59.32 s | **26.0296 C** (6.9%) | 6.3883 V |
| **192 kHz, HQ on** | 768 kHz | 1.302e-6 | **32.0** | 29.66 s | **25.5240 C** (3.5%) | 6.3775 V |

The document's "44.1 kHz and 96 kHz freeze at the same temperature" is **wrong**
and is withdrawn: 44.1 kHz does, 96 kHz does not, and 192 kHz is worse again.
The defect is therefore *larger* than stated -- the **host** sample rate moves
the modelled physics as well as the quality switch, and the spread across
supported rates is **25.52 C to 31.51 C** where the law `25 + 15(1 - e^-t/900)`
wants 34.4818 C at 900 s. `Source/DSP/YouKnow106Engine.h:786-790` explicitly
forbids exactly this ("a quality setting is not allowed to change what the
supply does"). The OTA headroom the accumulator drives stops between 6.3775 V
and 6.5052 V instead of **6.5687 V** -- a 2.5% error at 48 kHz HQ and 2.9% at
192 kHz -- and the panel thermometer reports the frozen value as fact. One
implementation detail every fixture must respect: `getDisplayTemperatureC()`
(`Source/DSP/YouKnow106Engine.h:204-212`) multiplies the 15 C rise by
`activeParameters_.calibration`, so the temperatures above are the Unit
Character 1.0 readings and a fixture at any other Character is asserting a
different law. Audibility: inaudible-but-structural, plus a 2.5-2.9% headroom
error that is real on hot patches.

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
   legs.** `Docs/open-questions.md:2420-2435` adjudicates this ladder at
   designator level: `Tr3 buffer -> IC3 4052 **Ycom** pin 3`, with the taps
   Y0/Y1/Y2/Y3 feeding the four 47 kOhm summing resistors into IC4a's virtual
   ground. The multiplexer is on the **source** side: it connects the buffer to
   exactly one leg, and R23/R21 "1M x 2" bias the mux side of the deselected
   capacitor legs to ground. A deselected C10/C11 is not tracking the input at
   225.8/720.5 Hz; it is bleeding to ground through 1 MOhm (10.6 ms / 4.7 ms).
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
- **Derived note-on timing scatter.** Notes land on the 4.2 ms scan grid,
  giving 0–4.2 ms of attack spread (measured t(90%) 8.62 / 10.90 / 5.52 /
  7.79 ms across four presses) and 1.83 ms between voices in a chord.
- **Integer-recurrence envelopes and LFO.** Attack 4.2 ms – 3.28 s, decay(−20
  dB) 4.2 ms – 21.6 s, release 16.8 ms – 25.5 s, LFO 0.0363 – 29.76 Hz.
- **The chorus delay engine.** Impulse-timed wet peak sweeps 1.67–6.44 ms
  (mode I) and 1.53–6.47 ms (mode II) against the designed 1.4–6.4 ms. Only the
  noise amplitude is wrong; step 4 touches nothing else.
- **Stable self-oscillation.** 124.5 / 993.2 / 7152.8 Hz at CUTOFF 0.30 / 0.50
  / 0.70, clean threshold, no blow-up at extremes, resonant alias floor
  −64.9 dB A/H at saw + resonance 0.85. Step 2 rewrites the trim that sits
  under this and must keep the 4.83 Vpp / 248.0 Hz service anchors — noting
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

- [ ] **1. Gate the PWM converter write with the LFO delay envelope.** The
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
  *Verified by*: a new engine-suite fixture holding a note with PWM SOURCE =
  LFO, PWM depth 1.0, LFO RATE 0.75 and LFO DELAY 1.0, probing `voice.pulseDuty`
  over a **200 ms** window (LFO RATE 0.75 is 7.4405 Hz, a 134.4 ms period, so
  anything shorter than one full cycle reads an alignment-dependent span — the
  original 83 ms window reads 0.4128 at t = 0.50 s but **0.2783 at t = 1.00 s**
  at identical full depth). It asserts the duty span at t = 0.05 s is **below
  0.005** (0.3498 today), that `lfoDelayLevel_` reads 0.0000 at that sample
  point, and that the span over the 200 ms window at t = 6.00 s is **unchanged
  from the pre-change engine to within 1%**, expressed as a comparison against a
  reference render rather than against a hard-coded 0.4129. Reverting the
  routing restores the 0.3498 span and fails the first assertion.

- [ ] **2. Drive the resonance frequency trim from the cascade's own
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
  **(c)** a rendered A/B at RESONANCE 0.20 and 0.80, same CUTOFF, asserting the
  spectral centroid moves by less than 15% where it moves 112.93 cents of corner
  today. **(d)** `testCpuBudget` re-measured and republished, not relaxed.

- [ ] **3. Put C59 between the filter output and the VCA input.** Roland draws
  module pin 3 VCF OUT → **C59 1 µF/50 V NP** → the VR27/R108 network → pin 9
  VCA IN, and adjusts VR30/25/20/15/10/5 through 2.2 MΩ for minimum thump
  (Service Notes pp. 18–19; recorded as **anchored** in the research contract's
  voice-module VCA row and at `Docs/open-questions.md:1785-1795`). `renderVoice`
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
  the settled sustain, the filter output's DC is **+0.0308 V against 0.596 Vrms
  (−25.8 dB)**, and the largest anywhere in the sweep is +0.129 V, always well
  *below* the AC. What is real is that this DC is multiplied by the envelope and
  arrives as a **duty-dependent sub-audio thump**: the peak of the output
  low-passed at 20 Hz over a note-on/note-off cycle rises from **−43.33 dB
  relative to broadband RMS at duty 0.50 to −18.06 dB at duty 0.95** — 25.3 dB
  from PWM duty alone, and essentially unchanged at Unit Character 0.0
  (−19.36 dB), so it is a property of the nominal calibrated model rather than a
  tolerance mechanism.

  *Verified by*: a new engine-suite fixture holding MIDI 48 on a pulse patch at
  CUTOFF 0.30, RESONANCE 0.75, PWM panel 1.00 (duty **0.95**; panel 0.95 is duty
  0.9275), Unit Character 1.0, ATTACK 0.45, released at 4.0 s of an 8 s render.
  It asserts **(a)** the mean of each active voice's filter-to-VCA input over the
  settled 1.5–3.0 s window is **below 1.0e-3 V** (+0.0308 V today, and up to
  +0.1292 V across the sweep); and **(b)** the peak of the output low-passed at
  20 Hz is **at least 35 dB below broadband RMS** at duty 0.95 (−18.06 dB
  today), while the same measure at duty 0.50 stays within 3 dB of its present
  −43.33 dB. **The original assertion (b) — a 2 Hz DFT bin at least 40 dB below
  RMS — is deleted because it passes today by 34 dB** (measured −73.56 dB): a
  six-second bin averages a transient away, and a test that cannot fail proves
  nothing. Removing the coupling restores the −18.06 dB figure and fails the new
  (b).

- [ ] **4. Set the BBD line noise from the MN3009's own noise specification.**
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

  *Verified by*: an extension of `testChorusNoiseComponents` asserting the
  recovered wet-line A-weighted noise is **at or below 0.200 mVrms, and within
  1 dB of it** (the "max" is an upper bound, so the assertion is one-sided by
  construction with a floor to catch an over-correction), plus a new
  engine-suite assertion that the idle plug-in output floor with **VOLUME 0.80,
  VCA LEVEL 0.80 and CHORUS NOISE 1.0 pinned** drops by **14.4 dB ± 0.5 dB**
  from its present value, expressed as a delta against a reference render rather
  than as a hard dBFS number, because the dBFS figure scales with two panel
  controls. **One target, not two:** the original's separate −76.2 dBFS (I) and
  −75.5 dBFS (II) assert a 0.7 dB mode difference the code cannot produce —
  `Chorus::settingsFor` gives modes I and II the same `lineGain` and the same
  sweep, differing only in rate, and `enableChorusRateNoise` is off by default;
  measured, the two floors are **−63.55 and −63.56 dBFS**, 0.016 dB apart. The
  existing MN3009 bandwidth and THD anchors must pass unchanged.

- [ ] **5. Make the warm-up clock a wall-clock accumulator neither a quality
  setting nor a host rate can move.** `thermalWarmupSeconds_` becomes a `double`
  (or an integer internal-sample count divided at the point of use), so the
  increment stops falling below half an ULP and the modelled law
  `T(t) = 25 + 15(1 − e^{−t/900})` runs to completion. No constant, curve or
  time-scale changes; only the accumulator's precision does. Closes gap 8. The
  invariant restored is the one `Source/DSP/YouKnow106Engine.h:786-790` states in
  words, and it is broken **more widely than the original said**: the freeze
  point is a power-of-two boundary that depends on the *internal* rate, so
  96 kHz HQ stalls at 26.0296 °C and 192 kHz HQ at 25.5240 °C — the host sample
  rate moves the modelled physics too, not only the HQ switch.

  *Verified by*: a new engine-suite fixture running silence at **Unit Character
  1.0** (`getDisplayTemperatureC()` scales the rise by `calibration`, so the
  targets below are meaningless at any other setting) and polling
  `getDisplayTemperatureC()` at t = 128 s, 300 s and 900 s of audio. It asserts
  **26.99 / 29.25 / 34.48 °C ± 0.05 °C** (frozen at 26.9886 °C today at 48 kHz
  HQ), that the three readings **agree within 0.01 °C across 48 kHz HQ on,
  48 kHz HQ off, 96 kHz HQ on and 192 kHz HQ on** (today 26.9886 / 31.5077 /
  26.0296 / 25.5240 at their respective stalls — the four-way comparison is what
  makes the assertion fail on the host-rate axis as well as the quality axis),
  and that the modelled OTA headroom at 900 s is **6.5687 V ± 0.001 V**
  (6.4087 V today at 48 kHz HQ, 6.3775 V at 192 kHz). Reverting the accumulator
  to `float` fails all three. Because a 900 s render at 768 kHz internal is
  expensive, the fixture may drive the accumulator directly through the test
  friend rather than rendering, provided it also renders one short case to prove
  the accumulator is the one the render loop advances.

- [ ] **6. Route the velocity extension through the envelope's own path to the
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

  *Verified by*: a new engine-suite fixture rendering one note at velocity
  0.2 / 0.5 / 1.0 with `velocityDepth = 1.0`, CUTOFF 0.50, ENV depth 0.40,
  RESONANCE 0.30, taking a 32768-point Blackman-Harris spectrum at t = 0.3 s and
  computing the energy-weighted centroid **on a stated estimator: a 10 Hz grid
  from 20 Hz to 8 kHz** (a centroid without a stated band and grid is not a
  number — the same render reads 240.75 Hz on this estimator against the 470.1 Hz
  originally printed). It asserts the centroid **rises monotonically with
  velocity and spans at least 100 Hz across the three** (today 240.75 Hz at all
  three, a span of 0.01 Hz), and that at `velocityDepth = 0.0` the render is
  **bit-identical to a reference captured before the change**. That reference
  has to be **built**: there is no FNV render lock in the CTest suites — the
  `fnv1a64` hashes the original cited live only in
  `Tools/RenderRealismComparison.cpp`, which is a report renderer, not a test.
  Reverting the routing collapses the centroid span to 0.01 Hz and fails the
  first assertion.

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
  project's own OQ-21 adjudication at `Docs/open-questions.md:2420-2435`: IC3's
  4052 sits on the **source** side (`Tr3 buffer → Ycom pin 3`), so the
  deselected legs are not continuously driven at all — they are bled to ground
  through R23/R21 1 MΩ, which is the charge-memory question OQ-21 still owns.
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
- **Replacing the capped Newton solve with a non-iterative discrete-gradient
  scheme (Danish, Bilbao & Ducceschi, DAFx-21).** This is the literature answer
  to the one gap the pass above measured and only half-closed — 1.395x realtime
  at resonance 0.95 against 0.699x at resonance 0.10 — and it would come with a
  Lyapunov stability guarantee the capped iteration does not have. It is also
  a cost step, not a sound step, and it rewrites the single most load-bearing
  routine in the engine (65% of all engine time, and the thing the 4.83 Vpp /
  248.0 Hz anchors are calibrated against). It is the right next pass, not a
  step in this one.
- **Narrowing the oversampled domain, or offering an intermediate 2x.** KR-106
  exposes Off/2x/4x and confines oversampling to the nonlinear stages; this
  engine oversamples the DCO, the BBD and the scanned control system too. That
  is the untried architectural lever on the cost axis and the only one the plan
  above never considered. It is not a free win — this DCO is a genuine
  ramp-and-comparator solve rather than a wavetable and the BBD is genuinely
  bucket-clocked, so both may legitimately need the higher rate — and the honest
  first move is to measure each domain's cost separately and publish which ones
  actually need 4x. That measurement belongs with the solver rework above.
- **Note-on-to-first-sample latency.** Neither this plan nor the comparative
  assessment has a row for playing latency, and KR-106 v2.5.12 deliberately
  removed ~2 ms from its tick-driven update path. Whether this engine's scan
  phase origin adds avoidable latency on top of the host buffer has never been
  measured here. It should be measured and published before anything is changed;
  measuring it is not a step because it changes nothing.
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
- **It does not address real-time cost, and step 2 makes it materially worse.**
  Section 4's after-column stands; a six-voice chorus-off render re-measured on
  this box costs 1.354x realtime, consistent with it. None of the six steps
  makes that better. Step 3 adds one first-order section per voice, which is
  small. **Step 2 is not small**, and the original's "very slightly worse" is
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
- **It adds no demo take.** The renderer still writes ten, and the frozen
  per-fix previews under `Docs/audio/` stay frozen.
