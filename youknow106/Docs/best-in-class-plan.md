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
