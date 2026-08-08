# Neuramar best-in-class plan

## What this document is

A competitive assessment of Neuramar against the commercial instruments it
actually competes with, an unflattering account of where it stands today, and a
numbered work plan. Each step states what changes, which gap it closes, and how
it is verified. Steps are marked off as they land.

The yardstick is
[`resynthesis-quality-benchmark.md`](resynthesis-quality-benchmark.md). That
document already specifies the metrics; what it lacks is measurements. Closing
that gap is step 1, because without it every later claim here is an opinion.

## Competitive landscape

Neuramar's category is *source-independent resynthesis*: import one sound, fit a
synthesis model, discard the recording, play the model across the keyboard. Four
products define the field, and two more set ceilings that Neuramar deliberately
does not compete for.

### Direct peers

**Sonic Charge Synplant 2 (Genopatch).** The closest philosophical match: a
genetic search fits Synplant's own FM/subtractive engine to a dropped sample,
runs entirely on the local CPU with no servers, and produces a patch that does
not reference the source audio. Its published limits are informative. The engine
is bounded to 48 genes, and reviewers converge on the same finding: it is good
on simple, synthetic, and percussive material and weak on complex acoustic
sources. Sound On Sound's assessment is that it is "unrealistic to load a
two-second sample from one note (for example, from a guitar) and expect Synplant
2 to then generate a fully playable, totally realistic patch that both spans the
full range of MIDI keys and offers the dynamic response of the original
instrument". Genopatch is CPU-only and reviewers report it saturating a core
while it converges.
[Product](https://soniccharge.com/synplant),
[Sound On Sound review](https://www.soundonsound.com/reviews/sonic-charge-synplant-2),
[MusicRadar review](https://www.musicradar.com/reviews/sonic-charge-synplant-2),
[MusicTech review](https://musictech.com/reviews/plug-ins/sonic-charges-synplant-2-review/).

**Dawesome MYTH 1.5 (Re-synthesis V2).** Machine-learning-assisted resynthesis
into a modular engine. The V2 algorithm shipped in November 2024 for one stated
reason, and it is the single most useful data point in this survey: the previous
algorithm lost the sample's transients, and V2 exists to keep them. V1.5 also
splits behaviour into four source-class playback modes — Wavetable for synth
sources, General for acoustic and voice, Groove for rhythmic material, Pitch
Flat for inharmonic or unpitched sources — which is an admission that one
analysis prior does not serve every source.
[Product](https://www.tracktion.com/products/myth),
[Synth Anatomy on 1.5](https://synthanatomy.com/2024/11/dawesome-myth-new-synthesizer-takes-you-on-an-ai-supported-resynthesis-journey.html),
[rekkerd on 1.5](https://rekkerd.org/dawesome-updates-myth-re-synthesis-instrument-to-v1-5/).

**Madrona Labs Sumu 1.1 + Vutu.** Bandwidth-enhanced additive: up to 64 partials
per voice, each with its own frequency, amplitude, and *noisiness* trajectory,
rendered by 128 sine+noise oscillators arranged as 64 FM pairs, each pair
individually placed in a reverberant space. Analysis is a separate offline
application (Vutu) with manual parameter choices, so it is an expert-assisted
ceiling rather than a one-click peer. 1.1 added full MPE.
[Product](https://www.madronalabs.com/products/sumu),
[Synth Anatomy on 1.1](https://synthanatomy.com/2025/08/madrona-labs-sumu-novel-synth-plugin-explores-spatialized-additive-resynthesis-with-fm.html).

**Xfer Serum 2 Spectral oscillator.** Real-time harmonic-level resynthesis of an
imported sample, with transient-detection processing described in the same terms
as advanced time-stretching, plus spectral warps that spread partials and twist
phases. It is not a source-independent instrument in the Synplant sense, but it
puts harmonic resynthesis into the highest-volume synth on the market and
therefore sets the baseline a reviewer will compare against.
[Product](https://xferrecords.com/products/serum-2),
[CDM overview](https://cdm.link/serum-2/).

### Sample-linked and analysed ceilings

**Image-Line Harmor.** Up to 516 partials per note *per unison voice*, with
image and audio resynthesis. Its highest-fidelity mode continues to reference
the original sample, so it is not a source-independent peer, but it is the
partial-count ceiling.
[Manual](https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/plugins/Harmor.htm).

**Apple Alchemy.** Additive resynthesis with a Num Partials control that goes to
600, and — the informative detail — an `Add+Spec` import mode that turns on the
*spectral* engine alongside the additive one specifically to recreate the noisy
aspects of a sound, "because this isn't a strength of additive resynthesis".
Alchemy's documentation also states that additive import quality depends on
correctly identifying the root note, from the filename or from waveform
analysis.
[Additive element controls](https://support.apple.com/guide/logicpro/additive-element-controls-lgsi55ccfb06/mac),
[Import browser](https://support.apple.com/guide/logicpro/import-browser-lgsi440c233a/10.7/mac/11.0),
[Alchemy overview](https://support.apple.com/guide/logicpro/alchemy-overview-lgsi2618652a/mac).

### Neural-audio state of the art

**Simionato and Fasciani, *Sines, transient, noise neural modeling of piano
notes* (Frontiers in Signal Processing, 2024).** The closest research analogue
to Neuramar's architecture, and it has three branches where Neuramar has three
different ones. Its quasi-harmonic module is physics-guided and predicts the
inharmonicity factor plus a per-partial amplitude envelope, and it explicitly
models phantom partials, beating, and double decay. Its *transient* module
generates a sine-based waveform transformed through an inverse DCT to produce
the impulsive component. Noise is the decomposition residual.
[Frontiers](https://www.frontiersin.org/journals/signal-processing/articles/10.3389/frsip.2024.1494864/full),
[preprint](https://arxiv.org/abs/2409.06513).

**Barahona-Ríos and Collins, NoiseBandNet (IEEE/ACM TASLP, 2024).** Already
cited in Neuramar's research document as the motivation for the Air branch, but
the numbers matter here: NoiseBandNet uses a **2048**-filter filterbank with a
32-sample synthesis window, and the paper's stated reason is that time-varying
FIR filters trade time against frequency, so good frequency resolution needs
many taps and many taps smear transients. A filterbank sidesteps the trade.
[Paper](https://arxiv.org/abs/2307.08007),
[project page](https://www.adrianbarahonarios.com/noisebandnet/).

**Least-squares sinusoidal estimation.** The standard result is that
least-squares parameter estimation is more accurate than FFT-peak and
analysis-by-synthesis methods, and — the property that matters here — FFT and
ABS methods cannot handle overlapping frequency responses or short analysis
windows, while a joint least-squares solve can.
[Smith, *Spectral Audio Signal Processing*](https://ccrma.stanford.edu/~jos/sasp/Least_Squares_Sinusoidal_Parameter.html).

### What the field says separates the top tier

Reading across the reviews and the product changes, four properties decide
whether one of these instruments is taken seriously:

1. **Transient survival.** MYTH's flagship update exists for it; Serum 2
   advertises transient detection; the piano paper gives transients their own
   branch. It is the property users notice first and complain about first.
2. **Noise realism.** Alchemy runs a second engine for it. NoiseBandNet exists
   for it. An additive-only reconstruction sounds synthetic on breath, bow,
   scrape, and hammer material.
3. **Playability away from the analysed pitch.** This is where Synplant is
   openly criticised, and it is the property a single recording can least
   support.
4. **Resolution.** 516 and 600 partials are the commercial numbers. Sumu's 64
   partials are defensible only because each carries a bandwidth/noisiness
   trajectory, not a bare sine.

## Where Neuramar actually stands

Read against that list, honestly:

**1. There is no measurement of resynthesis fidelity against the source.**
This is the worst finding in this document. Neuramar's entire premise is that
the fitted model reproduces the sound the user dropped in, and there is not one
number in the repository that says how well it does. The benchmark document
specifies multi-resolution spectral convergence, log-magnitude MAE, residual ERB
band power, early cumulative energy, and attack time — and then reports none of
them. What *is* measured is held-out *register* shape (2.790 dB MAE) and
stiff-string partial placement (0.094 cents), which are extrapolation and
placement metrics, not reconstruction metrics. The instrument's central claim is
unverified.

**2. The harmonic solve is sequential, and that biases every partial.**
`analyseHarmonicResidual()` estimates partial 1, subtracts it from the waveform,
estimates partial 2 from what is left, and so on. The analysis aperture targets
four root periods, which makes the Hann main lobe exactly one partial spacing
wide — so adjacent partials' main lobes touch and every subtraction leaks into
its neighbours. The damage is worst exactly where it is most audible: a quiet
partial next to a loud one absorbs the loud one's leakage. The repository's own
numbers show the symptom. Excitation parity error, which measures how well the
odd/even contrast of a 25 dB alternating spectrum survives, got *worse* across
releases: 0.609 dB in 1.0, 0.898 dB in 1.1, 1.043 dB in 1.2. The literature is
unambiguous that a joint least-squares solve is the fix and that it is the only
one of the standard methods that tolerates overlapping responses and short
windows.

**3. Eight Air bands is roughly one octave per band.**
The bands are log-spaced from 80 Hz to 16 kHz, so each is 1.03 octaves wide. The
benchmark proposes a residual ERB-power MAE gate of 2 dB at the root. Eight
octave-wide filters cannot represent an ERB-resolved noise floor to 2 dB on any
source whose residual has structure — a breath formant, a bow scrape peak, and a
hiss shelf inside one band are one number. Alchemy's answer to the same problem
is a second engine; NoiseBandNet's is 2048 filters. Neuramar's is eight.

**4. Six Bone modes cannot hold a struck body.**
The private hard-case corpus lists "bell or struck-metal impact" and
"electric-piano tine". A bell has tens of audible partials with independent
decay. Six persistence-scored candidates is a gesture at modal synthesis, not a
representation of one, and there is no measurement of modal precision, recall,
frequency error, or T60 error anywhere in the suite.

**5. There is no transient branch and the aperture is 10.7–85 ms.**
`transientAnalysisWindowSize()` bounds the Core/Air aperture to 512–4096 samples
at 48 kHz. At 220 Hz that is 1024 samples, 21 ms. The analysis grid puts 48 of
its 128 frames in the first 120 ms, so frames are 2.5 ms apart — but each is
measured through a 21 ms window, so the effective onset resolution is the
window, not the grid. The four-period rule exists to resolve adjacent partials,
which is precisely the constraint a joint solve removes.

**6. Eight voices.**
Harmor gives 516 partials per unison voice. Alchemy gives 600 partials. Sumu
gives 128 oscillators per voice. Neuramar renders up to 256 oscillators per
voice, which is respectable, and then caps polyphony at eight. A four-note chord
with sustain pedal steals voices. The engine's own cost figures say the ceiling
is conservative: the worst measured eight-voice case runs at 5.9x real time.

**7. Root detection accuracy is unmeasured.**
The benchmark's one-click protocol treats automatic root error as a headline
result and proposes a 98%-correct gate. The suite has four hand-picked root
cases. Alchemy's documentation is explicit that additive import quality depends
on getting the root right; a resynthesis instrument that mis-octaves a source
produces a model that is wrong in a way no later control can fix.

**8. Velocity response is a fixed heuristic.**
`Touch` tilts brightness and Air around a mezzo-forte reference. It is a musical
control, but it carries no evidence from the source, and reviewers of this class
of instrument name dynamic response as a specific weakness. One recording
genuinely contains no cross-velocity evidence, so this is a limit rather than a
defect — but it should be stated as one and not quietly claimed as modelling.

Two things are genuinely strong and should not be traded away. The aliasing
floor (-108.2 dB worst spectral line below the played fundamental, against a
-85 dB guard) is better than this class of instrument normally achieves, and the
fit time (0.557 s for a 1.6 s source) is far better than Genopatch's converging
strands. Neither should regress.

## Plan

Each step is a single commit, green before it lands.

- [x] **1. Measure resynthesis fidelity against the source.** *Landed.* The
  first measurement found three things worth recording: log-magnitude MAE of
  10.75 dB on the source/filter fixture, a residual ERB MAE that is *worse*
  (5.00 dB) on the noise-free fixture than on the noisy one (3.54 dB) because
  the render fits an Air layer to a source that has none, and 12.46 dB too much
  energy in the first millisecond of the attack. See the benchmark document's
  *Root-note reconstruction* section.
  Extend `Tests/ResynthesisQualityTests.cpp` with a root-note reconstruction
  harness: render the learned model at the source pitch under the frozen
  `Match` state, align the onset, apply one RMS scalar, and report
  multi-resolution spectral convergence and log-magnitude MAE at
  `(256, 64)`, `(1024, 256)` and `(4096, 1024)`, ERB-band residual power MAE
  with the harmonic bins excluded, cumulative-energy error at 1/5/10/20/50 ms,
  and T10–T90 attack-time error. Two fixtures: the existing source/filter note,
  and a new one combining harmonics, time-varying shaped noise, and a short
  broadband transient (gold-corpus items 5 and 6). *Closes gap 1.* *Verified
  by:* the numbers are printed by the test binary and pinned by regression
  guards; the benchmark document gains a measured-results table.

- [x] **2. Make the harmonic solve joint.** *Landed.* Parity error 1.043 →
  0.923 dB, held-out shape 2.790 → 2.714 dB, root convergence 0.0412 → 0.0392,
  root residual ERB 5.00 → 4.73 dB, `learn()` 0.609 → 0.765 s. Refinement is
  applied only to apertures shorter than eight fundamental periods; applying it
  to the long modal aperture as well cost 2.2x on `learn()` and made the noisy
  fixture *worse*, which is what the orthogonality argument predicts. Early
  cumulative energy moved 0.28 dB the wrong way against a 12.5 dB aperture
  error the solve did not create — that is step 3's target.
  Replace the single sequential pass in `analyseHarmonicResidual()` with
  Gauss-Seidel refinement over the partials: after the first pass, add each
  partial's contribution back into the residual, re-solve its 2x2 normal
  equations against the other partials' current residual, and subtract the new
  estimate. This converges to the joint least-squares solution without forming
  or factorising a 128x128 system, and costs one extra residual pass per sweep.
  *Closes gap 2.* *Verified by:* excitation parity error and log-magnitude MAE
  in step 1's harness both fall; `learn()` cost is re-measured and reported.

- [x] **3. Shorten the analysis aperture now that the solve is joint.**
  *Landed.* Frames inside the first 40 ms ask for two root periods instead of
  four. Source/filter cumulative energy at 1 ms 13.31 → 5.66 dB, its
  cumulative-energy MAE 3.45 → 1.32 dB, its spectral convergence 0.0392 →
  0.0365, its T10-T90 error +1 → 0 ms; the noise+transient fixture's
  cumulative-energy MAE 1.09 → 0.34 dB. The trade is real and recorded: that
  fixture's spectral convergence rises 0.0594 → 0.0757, all of it at the
  `(256, 64)` resolution whose 5.3 ms frames are measuring a broadband noise
  burst the renderer draws its own realisation of, and its T10-T90 error moves
  to -5 ms because more of the burst is attributed to the phase-aligned
  harmonic branch.
  The four-period rule exists only to keep adjacent partial main lobes apart,
  which a joint solve no longer requires. Reduce the aperture for frames inside
  the dense onset region, keeping the long aperture for sustain, and keep the
  parallel 4096-sample modal residual untouched. *Closes gap 5.* *Verified by:*
  cumulative-energy error at 1/5/10 ms and T10–T90 attack error fall in step 1's
  harness while shape MAE and parity do not regress. **Dropped if** the joint
  solve does not hold up at the shorter aperture; that finding is recorded here
  either way.

- [x] **4. Raise the body representation: 16 Air bands and 12 Bone modes.**
  *Landed as model format version 5.* Residual ERB-band power MAE on the fixture
  that actually contains noise 3.55 → 2.35 dB; log-magnitude MAE 5.40 → 5.22 dB
  there and 10.76 → 10.29 dB on the source/filter fixture; a new struck-body
  fixture with ten known inharmonic modes reports 0.90 recall, 0.90 precision,
  and 8.09 cents mean frequency error where six slots capped recall at 0.60.
  The noise-free fixture's residual ERB MAE moves 0.31 dB the wrong way, for the
  same reason it was already the worst of the two: the render fits an Air layer
  to a source that has none, and a finer filterbank follows that leakage more
  closely. Cost is 11–18% on the body layers per voice; the muted-layer and
  note-on benchmark rows do not move.
  Air moves from 8 to 16 log-spaced bands (1.03 to 0.51 octaves each) and Bone
  from 6 to 12 persistence-scored candidates, in one model format bump to
  version 5. Versions 2–4 keep an exact legacy read path: their 8 bands and 6
  modes load into the low slots with the remaining slots inactive, so a saved
  session renders as it did. *Closes gaps 3 and 4.* *Verified by:* residual
  ERB-power MAE falls on step 1's noisy fixture; a new modal fixture with ten
  known inharmonic modes reports active-mode recall and frequency error; the
  legacy-compatibility serialization tests are extended to cover the migration.

- [x] **5. Raise polyphony to sixteen voices.** *Landed.* The eight-voice
  benchmark rows are unchanged, so this costs nothing to an eight-note
  performance; the new sixteen-voice row costs 1.7x the eight-voice figure at
  the same root. A twelve-note held cluster now sounds twelve voices instead of
  stealing four, and that is asserted rather than assumed.
  `NeuramarEngine::maximumVoices` from 8 to 16, with the voice-steal, pan
  refresh, and fade-tail paths re-measured under the higher count. *Closes gap
  6.* *Verified by:* the existing voice-ceiling, steal-tail-bound, and
  hand-off-continuity tests run at the new ceiling; the benchmark's cost table
  gains a sixteen-voice row.

- [x] **6. Measure and fix automatic root detection across a corpus.**
  *Landed as a measurement; no fix was needed.* Twelve analytic classes —
  roll-off at three registers, missing fundamental, fixed formants, stiff
  string, broadband noise, vibrato, moving formant, delayed onsets, a
  two-octave resonant saw sweep, and a 3:1 phase-modulation tone — are all
  identified to the correct semitone with no octave errors. The step is
  reported as what it is: an untested claim turned into a measured one with a
  regression gate behind it, not a defect found and repaired.
  Add a deterministic root-detection corpus to the test suite covering the
  analytic ground-truth classes the benchmark names — exponential roll-off,
  odd/even with a missing fundamental, formant-dominant, stiff-string,
  harmonics-plus-noise, vibrato, low and high registers — and report
  correct-semitone rate and octave-error rate. Fix what it exposes. *Closes gap
  7.* *Verified by:* the rate is printed and guarded; any detector change is
  justified by a case that fails without it.

- [x] **7. Reconcile the documentation with the measurements.** *Landed.*
  `README.md`, `neural-synthesis-research.md`, and
  `resynthesis-quality-benchmark.md` updated so every behavioural claim matches
  a measured number or is explicitly marked as a target. Gap 8 (velocity) is
  written down as a stated limit rather than left implied, and the missing
  transient branch is named in the research document as the clearest remaining
  structural gap, with the two measurements that expose it.

## What this closed, and what it did not

Measured deltas across the seven steps, all from
[`resynthesis-quality-benchmark.md`](resynthesis-quality-benchmark.md):

| Measurement | Before | After |
| --- | --- | --- |
| Root-note reconstruction | not measured at all | six metrics on two fixtures, guarded |
| Residual ERB-band power MAE, noisy fixture | 3.54 dB | **2.35 dB** |
| Log-magnitude MAE, source/filter | 10.75 dB | **10.29 dB** |
| Cumulative energy at 1 ms, source/filter | -12.46 dB | **-5.84 dB** |
| Cumulative-energy MAE, noisy fixture | 0.87 dB | **0.46 dB** |
| Excitation parity error | 1.043 dB | **0.923 dB** |
| Held-out Body-Locked shape MAE | 2.790 dB | **2.713 dB** |
| Modal recall on ten known modes | 0.60 (capacity bound) | **0.90** |
| Automatic root, twelve analytic classes | not measured | 12/12, no octave errors |
| Voice ceiling | 8 | **16** |
| `learn()` on a 1.6 s / 44.1 kHz source | 0.609 s | 0.768 s |

Three things got worse and are not hidden. The noise+transient fixture's
spectral convergence rose from 0.0571 to 0.0754, entirely at the `(256, 64)`
resolution that measures the transient itself; its T10-T90 attack error moved
from 0 to -5 ms; and the noise-free source/filter fixture's residual ERB MAE
rose from 5.00 to a matching 5.00 by way of 4.69, because a finer filterbank
follows invented Air more closely than a coarse one does. All three have the
same root cause and the same answer, which this work did not attempt: there is
no transient branch, so an impulsive onset is reproduced by phase-aligned
harmonics. That is written up in
[`neural-synthesis-research.md`](neural-synthesis-research.md) as the next
structural step.

`learn()` costs 26% more. It is offline, bounded, and still far inside the
benchmark's proposed ten-second import gate.

## Deliberately not attempted

- **A pretrained pitch or timbre model.** The repository ships no weights and
  that is a licensing claim in its README, not a convenience.
- **Velocity-layer modelling.** One recording contains no cross-velocity
  evidence. `Touch` stays a musical control and is documented as one.
- **An end-to-end differentiable renderer.** It would change the fit from
  bounded seconds to unbounded minutes and is the reason this architecture was
  chosen in the first place.
- **Matching Harmor's 516 partials.** Neuramar already renders up to 256
  oscillators per voice from a 64-partial model; the binding constraint is
  evidence in one recording, not oscillator count.

---

## Second pass — the render side, 2026-08-07

The first pass fixed the analysis: it measured reconstruction for the first
time, made the harmonic solve joint, shortened the onset aperture, widened the
body representation, raised polyphony, and measured root detection. Every one of
those steps improved how faithfully the fitted model reproduces the source *at
the source pitch*. This pass is about everything that happens after the model is
fitted — the parts of the render that stand in for a physical mechanism instead
of replaying learned evidence. A fresh engine audit measured thirteen defects on
the shipping code at the plug-in's own default parameter values, and they share
a shape: where Neuramar reproduces evidence it is at or above the commercial top
tier, and where it has to invent behaviour the source did not record — note-off,
transposition, repetition, sustain past the sample, stereo placement — it does
something with no physical counterpart. Nine of the thirteen are audible on the
first note. This pass closes six of them; a seventh and an eighth were planned,
re-measured, and struck, and the measurements that struck them are kept under
*Considered and not planned* so the next pass does not re-derive them.

### What changed in the field

A provenance caveat first, because it bears on how much weight the prices and
dates below can carry. Direct page fetches were blocked by this session's egress
proxy for every vendor and forum host tried, so the product claims here rest on
search-result summaries plus the URLs those results returned, not on the pages
themselves. The engine-side and acoustics claims are not affected — those were
read from the repository and from the cited papers' abstracts and records.

**A direct one-click competitor now exists, and it ships the branch Neuramar
lacks.** Samplab Resynthesizer (announced May 2025, released June 2025) takes a
one-shot sample, analyses it on-device, splits it into harmonic and percussive
components, and plays the result chromatically with independent harmonic count,
drive and smoothing on each layer. That is Neuramar's premise plus the
harmonic/percussive factorisation the first pass named as its clearest remaining
structural gap. Reported at EUR 77.99 perpetual.
[Synthtopia](https://www.synthtopia.com/content/2025/05/27/samplab-resynthesizer-combines-simplicity-of-sampling-with-the-control-of-a-synthesizer/),
[gearnews](https://www.gearnews.com/samplab-resynthesizer-synth/).

**Owners judge that competitor on partial count, harshly and immediately.**
Review and forum commentary calls its resynthesis "very poor" with "too few
harmonics for full fidelity", and quotes a user saying "23 harmonics is way too
low to get a proper resynthesis". The previous pass's *Deliberately not
attempted* section dismisses partial count as not the binding constraint, which
is true of Neuramar's 64-partial model driving 256 oscillators, and is also the
first number an owner quotes. It should be on the tin.
[KVR product reviews](https://www.kvraudio.com/product/resynthesizer-by-samplab/reviews).

**The partial-count ceiling this document cites was reset.** Sound Radix
Radical1 (January 2026) claims "tens of thousands of oscillators in real time"
with "no aliasing and no oversampling", plus spectral sample resynthesis. Harmor's
516 and Alchemy's 600 are no longer the top of the scale. The more useful
consequence is that Radical1 now makes the same aliasing claim Neuramar's
strongest measured result makes, without publishing a number or a method —
Neuramar's -108.2 dB (this audit re-measured -111.5 dB at root+43 semitones) is a
differentiator only if it is published with its method.
[Sound Radix](https://www.soundradix.com/products/radical1/),
[Sound On Sound news](https://www.soundonsound.com/news/new-radical1-soft-synth-sound-radix).

**The direct peer gained a local, weight-free way to aim its generator.** Sonic
Charge PhenoType (June 2026) generates fully editable Synplant 2 patches from
typed descriptions; the parser "runs locally in Synplant with no internet
connection required" and "is not a large language model", working from a few
hundred internal tags plus synonyms and misspellings. It is free to Synplant 2
owners. Neuramar's sample-free Randomize is an unguided roll of its own neural
field with 1%/10%/100% breadth and nothing steering it, and PhenoType shows that
steering is achievable inside exactly the constraints Neuramar set itself: no
bundled weights, no network, no provenance obligations.
[KVR news](https://www.kvraudio.com/news/sonic-charge-releases-phenotype---free-text-to-patch-generator-for-synplant-2-67348),
[Synth Anatomy](https://synthanatomy.com/2026/06/sonic-charge-phenotype-prompt-fully-tweakable-sounds-in-synplant-2-first-look-review.html).

**Every named peer ships MPE. Neuramar handles no per-note expression at all.**
Serum 2 advertises full MPE; MYTH advertises "full MPE Support" and ships 300+
MPE presets; Sumu 1.1 added it; HALion 7.5 (free, August 2026) added it.
`Source/PluginProcessor.cpp` handles CC 120 and CC 123 and nothing else — no
pitch bend, no channel pressure, no per-note anything, and
`NeuramarEngine::noteOn(midiNote, velocity)` has no continuous per-note input to
receive it. This is the most visible spec-sheet omission in the instrument.
[MYTH](https://www.tracktion.com/products/myth),
[Serum 2](https://xferrecords.com/products/serum-2),
[HALion 7.5](https://synthanatomy.com/2026/08/steinberg-halion-7-flagship-sampler-plugin-gets-an-fm-and-spectral-makeover.html).

**The systems owners actually name for accuracy are missing from the comparison
set.** On KVR, users state that "HALion 7 and Icarus 2 have probably the most
accurate results" and that "Icarus, HALion 7 and Myth give you a very accurate
reproduction of the sound you put in"; another says Icarus 2 "is far easier and
gets better results than Synplant 2 or Serum". The benchmark's ceiling row is
Harmor and Alchemy, chosen on partial count. It should carry at least Icarus,
chosen on the property owners are actually ranking.
[KVR thread](https://www.kvraudio.com/forum/viewtopic.php?t=615611).

**Sumu's binding weakness is confirmed to be its workflow, not its resolution.**
Sumu 1.3.0 (5 March 2026) shipped "partials import feedback": importing a Vutu
file with more than 64 partials now fails with a log file instead of producing an
unplayable map. Madrona's own forums record that analysing a sound with Vutu "is
a complicated process", that "it can be hard to find the combination of control
settings that reproduces a sound in 64 partials or fewer", and that importing is
"super clunky". Neuramar's one-click, no-parameters, 0.77 s fit is a competitive
claim and is currently stated nowhere as one.
[Madrona news](https://www.madronalabs.com/news/192),
[Vutu thread](https://madronalabs.com/topics/8984-vutu-sound-analysis-for-sumu).

**MYTH owners name an artifact class Neuramar cannot measure.** Users report "a
certain sound ingredient typical for resynthesis" and "the fizziness of the
resynthesis". That is the perceptual name for high-band noise that is
uncorrelated with the tone. Neuramar's benchmark measures residual ERB-band power
MAE, which is a power match and blind to it by construction: two renders with
identical band powers can differ entirely in whether the noise fuses with the
tone. No coherence or noise-modulation measurement exists in the suite.
[KVR MYTH thread](https://www.kvraudio.com/forum/viewtopic.php?t=604436).

**Neuramar's headline fidelity metric has published evidence against it.**
Horner, Beauchamp and So, *A Search for Best Error Metrics to Predict
Discrimination of Original and Spectrally Altered Musical Instrument Sounds*
(JAES, 2006) report that the best correspondence to listener discrimination was
"a spectral error metric based on linear harmonic amplitude differences
normalized by rms amplitude and raised to a power a, achieving an optimum
correspondence of 91% for a 0.64", and that critical-band grouping gave no
improvement over it. Turian and Henry, *I'm Sorry for Your Loss:
Spectrally-Based Audio Distances Are Bad at Pitch* (arXiv:2012.04572), find that
multi-scale spectral distances "still contain many very fine-grained local
minima". Vahidi et al. (JAES 71(9), 2023; arXiv:2301.10183) add that
spectrogram-based distances are insensitive to intermediate-timescale structure.
Neuramar's gates are multi-resolution spectral convergence and log-magnitude MAE.
Horner's metric is a per-partial quantity and Neuramar's whole representation is
per-partial, so it is directly computable from what the model already holds.
[JAES](https://www.aes.org/e-lib/browse.cfm?elib=13671),
[arXiv:2012.04572](https://arxiv.org/abs/2012.04572),
[arXiv:2301.10183](https://arxiv.org/abs/2301.10183).

**Absence: nobody in this category has published a number.** A targeted search
for MUSHRA or listening-test comparisons of Synplant, Harmor, Alchemy, MYTH or
Sumu against each other returned nothing — only unrelated MUSHRA work on speech
synthesis, amplifier emulation and ambisonics. All reviewer coverage of these
products is descriptive and anecdotal. The field has opinions, not standards, so
executing the benchmark protocol against the locked competitor set would be a
first rather than a catch-up.

**Absence: no acoustics result was found that supports pitch-invariant decay,
pitch-invariant inharmonicity, or stationary residual noise.** Every source
consulted says the opposite, and two of them give quantitative audibility bounds
that Neuramar currently breaks by a wide margin. Those bounds are used as the
verification numbers in the plan below: Järveläinen and Tolonen, *Perceptual
Tolerances for Decay Parameters in Plucked String Synthesis* (JAES 49(11), 2001)
measure a tolerance of roughly 75%-140% of the reference decay time constant, and
Järveläinen, Välimäki and Karjalainen, *Audibility of the timbral effects of
inharmonicity in stringed instrument tones* (ARLO 2(3), 2001) fit an audibility
threshold `ln B = 2.57 ln f0 - 26.5`, with the threshold at C#6 "over 1,000 times
higher than for A1".
[JAES 10173](https://www.aes.org/e-lib/browse.cfm?elib=10173),
[ARLO](https://pubs.aip.org/asa/arlo/article/2/3/79/123666/Audibility-of-the-timbral-effects-of-inharmonicity).

### Where the engine actually stands

Every number below was measured on the shipping code by scratch programs linking
`libNeuramarDSP.a` and driving the engine at the plug-in's own default parameter
values. No repository file was modified to obtain them. The suite is green: 3/3
ctest suites in 40.5 s.

**Re-measurement note.** This section was re-measured a second time, from
scratch, by an adversarial pass, and where the two passes disagree the number
below is the second one. Two structural findings from that pass are folded in
where they belong: the register compensation cannot change the noise/tone
balance the way gap 2 originally claimed, and each Bone mode already carries its
own learned amplitude trajectory, so gap 5 was attributing a Core defect to the
Bone layer. Both are shown with the measurement that settles them. Every
absolute level below is fixture-dependent — the register, Orbit, repetition and
modal numbers move by a factor of several between a sustained, a breathy, a
decaying, a percussive and a struck fixture — so each claim now names the
fixture it was taken on, and the plan's gates are written as spreads and ratios
rather than as absolute levels wherever that is possible. The five scratch
fixtures are: *sustained* (20 harmonics, `h^-1.4`, 5 Hz vibrato, 1.6 s),
*breathy* (48 harmonics `h^-1` plus a strong high-passed noise bed, 1.6 s),
*decay* (32 partials with `tau_h = tau_1 h^-0.75`, `tau_1` 0.9 s or 0.20 s),
*percussive* (12 partials, `tau_h = 0.30 h^-0.8`, plus a 4 ms noise burst), and
*struck* (eight independent inharmonic modes at 261.6-4155 Hz with T60 2.20 down
to 0.07 s).

**Gap 1. Note-off is a gain fade, not damping.**
`NeuramarEngine.cpp:1199-1201` builds one `releaseMultiplier` per block;
`:1270-1273` multiplies `voice.envelope` by it; `:1370-1372` applies that single
scalar to the already-summed `coreSample + airSample + boneSample`. All 256
partials, 16 Air bands and 12 Bone modes are therefore attenuated by exactly the
same factor. Measured on the *sustained* fixture over 150 ms after a note-off at
Dissolve 0.65 s, subtracting the held note's own drop from the released note's at
each partial by least-squares sinusoid amplitude: -23.077 dB at partial 1
(220 Hz), -23.083 dB at partial 8, -23.033 dB at partial 16, -23.200 dB at
partial 32 (7040 Hz). That is 0.12 dB of frequency dependence across five
octaves, and it reproduces exactly. The released tail's spectral centroid 150 ms
after note-off is 1079.3 Hz against the held note's 1079.1 Hz — **0.02%**, not
the 0.34-0.59% first recorded; the smaller number is the one the plan's gate has
to beat. No physical damper works this
way: the tone should darken as it dies. The rate is register-independent too —
-19.19 dB/100 ms at MIDI 36 against -16.98 dB/100 ms at MIDI 96, and all 2.2 dB
of that difference is the model's own decay riding along.

**Gap 2. Applying the register compensation to Air and Bone is what swings the
noise/tone balance across the keyboard — by 17-22 dB.**
`NeuramarEngine.cpp:991-1012` computes `registerGain = sqrt(referencePower /
renderedPower)`, clamped to [0.25, 4.0], and `:1013-1015`, `:1037-1039` and
`:1078-1079` multiply the Core, Air and Bone targets by it. Measured with an
Air-muted render subtracted from a full one — the two differ only in
`parameters.air`, so the difference is exactly the Air layer — on the *breathy*
fixture at shipping defaults with Mutation 0, Air's share of total energy runs
-21.58 dB at MIDI 12, -9.52 dB at MIDI 60 and -4.37 dB at MIDI 108: a **17.2 dB**
swing, 19.2 dB measured against Core alone. With Body Lock 1 and Register 0
freezing the band centres it is **21.6 dB** against Core (-19.77 dB at MIDI 12 to
+1.78 dB at MIDI 108). On the *sustained* fixture the same spread is 15.6 dB and
17.1 dB. That is the defect, and it is real, audible and on by default.

The mechanism first recorded here was wrong, and the correction changes what has
to be done about it. `registerGain` scales all three layers by the *same* factor,
so it cannot by itself move the balance between them — but it does not scale the
Core, it *normalises* it: after the multiply, Core power is `referencePower`,
which barely depends on the played note, while Air power is `A * registerGain`.
The ratio is therefore `A / sqrt(renderedPower)`, and `renderedPower` is the
pre-gain Core power, which collapses on a high note as partials fall past the
anti-alias limit and past the Body-Lock envelope's range. Multiplying Air and
Bone by the same gain is exactly what converts a Core-only rendering artefact
into a keyboard-wide balance error. Two measurements settle it. First, a scratch
build with `referenceFundamental` changed to the *played* fundamental —
the like-for-like band limit — produces Air-share and Air/Core columns that are
**identical to the printed precision** at every note on all four fixtures: that
change cannot move this metric at all, and it does not stop the clamp
(`registerGain` still reaches 4.000 at MIDI 96-108 on every fixture, and the
`h^-1` formant fixture's 21.2 dB level fade across MIDI 12-108 is 21.2 dB after
it too). Second, a scratch build that simply stops applying `registerGain` to Air
and Bone collapses the Body-Lock-1 Air/Core spread from **21.55 dB to 0.10 dB**
and the shipping-default spread from 19.20 dB to 6.77 dB, with the Core's own
level spread unchanged at 0.07 dB. `README.md:250-255` states the inverted
version of this reasoning — "Core, Air, and Bone all take the same compensation,
so the balance between the three layers is the source's at every point on the
keyboard" — and is false by 17-22 dB for that exact reason.

**Gap 3. Orbit, on by default, makes a held note exactly periodic with
time-reversed legs.**
`NeuramarEngine.cpp:723-734` folds the model clock into a triangle over
`[loopStart, loopEnd]`; `PluginProcessor.cpp:244` declares Orbit as a bool
defaulting to `true`, and `:452` maps that to `orbit = 1.0`. On the *decay*
fixture the learned loop is [1.0082, 1.2859] s, so the ping-pong period is
0.5553 s (1.80 Hz). The 20 ms RMS envelope's normalised autocorrelation over
t in [2, 6] s, with Air and Bone muted so the noise floor does not blur it, is
**0.979 at one period and 0.992 at two**, with a matching trough of -0.989 at
half a period; the level oscillates 2.99 dB peak-to-peak on that fixture and
**4.68 dB** on the *percussive* one at full shipping defaults, indefinitely, and
is still doing it at t = 5.9 s. (The originally recorded 0.99977 and 4.8 dB are
the right order; 0.979/0.992 and 2.99-4.68 dB are what re-measurement gives.)
Half of every cycle plays backwards, so a decaying trajectory becomes a rising
one; nothing in a passive resonator re-energises itself. The loop also holds the
note above its own decay at t = 6 s by **5.3 dB** on the *decay* fixture, 7.7 dB
on the *percussive* one and 22.6 dB on the *sustained* one — the 9.4 dB first
recorded is inside that range, not a constant.

**Gap 4. Repeated identical notes are near-clones, and the only variation
mechanism throws attack away.**
`NeuramarEngine.cpp:731-734` adds `parameters.mutation * voice.mutationOffset *
0.018 * duration` to the model clock and clamps the result to `[0, duration]`;
`:703-705` gives +/-0.0012 of detune per unit mutation; `:884` sets
`variationDepth = mutation * 0.045`; `:504-516` re-seeds the Air PRNG. At the
shipping Mutation of 0.12 the entire per-note budget is +/-0.25 cents, +/-0.54%
of harmonic amplitude, a fresh noise stream and +/-2.6 ms of start-time offset.
Twelve identical `noteOn(57, 0.8)` calls — each allowed to retire before the next
so `ageStamp` advances, which is what selects `mutationOffset` — spread
**0.823 dB** in peak on the *percussive* fixture and **0.101 dB** on the
*sustained* one at Mutation 0.12, and 2.265 dB / 0.311 dB at Mutation 0.50. (An
earlier measurement of 1.41 dB and 0.195 dB is the same finding on a different
fixture.) A hand-struck acoustic instrument varies 1.5-3 dB in peak between
nominally identical strikes.

The clamp does make the offset one-sided, but narrowly, and one consequence
recorded here does not hold. `effectiveTime` is clamped to `[0, duration]` and at
the first control frame `oneShotTime` is 0, so a negative `mutationOffset` clamps
to 0 and only a positive one skips forward — but that asymmetry lasts only
`mutation * |offset| * 0.018 * duration`, at most 2.6 ms at Mutation 0.12 on a
1.2 s source, after which a negative offset simply reads slightly earlier and is
not clamped at all. The takes that clamp are **not** bit-identical to each other:
detune, the harmonic variation phase and the Air seed all still differ with
`mutationOffset`, and the worst take-against-take difference at Mutation 0.12 is
-50.5 dB, not silence. Nor is the peak distribution detectably bimodal:
`|mean - median|` across twelve takes is **0.015 dB** at Mutation 0.12 and
0.119 dB at Mutation 0.50. What is true is that the mechanism is the wrong one —
first-10 ms energy relative to each take's own peak spreads 0.624 dB at Mutation
0.12 and 1.231 dB at 0.50, so variation is being bought by deleting transient
rather than by varying how hard the thing was hit.

**Gap 5. On a struck inharmonic body the render invents a harmonic series and
holds the modal decays far too long.**
`NeuramarEngine.cpp:889-972` lays 64-256 partials on integer multiples of the
detected root, and `:1069` collapses `boneModeReliabilities_` to a binary 1.0/0.0
gate. On the *struck* fixture (261.6 / 396.4 / 705.2 / 1043.0 / 1519.7 / 2231.5 /
3068.0 / 4155.0 Hz, T60 2.20 down to 0.07 s) the learner detects a root of
131.17 Hz — an octave below the lowest mode — and the render's least-squares
log-amplitude T60 at each *source* mode frequency, played at the detected root,
is 118.4% of ground truth at mode 1, 118.6% at mode 2, 164.1% at mode 3, 271.6%
at mode 4, 421.1% at mode 5, 1497.6% at mode 6, 1601.1% at mode 7 and **2911.8%**
at mode 8. A struck body's modes are held far too long, by up to a factor of 29,
and that reproduces the earlier finding in kind.

Two claims first recorded under this gap do not survive re-reading the source,
and they matter because the fix depended on them. First, the Bone modes do
**not** share one trajectory: `SynthesisFrame::boneAmplitudes` is a 12-element
per-mode network output (`NeuralModel.h:42`, `outputSize` at `:76-78`) and
`:1078` reads `frame.boneAmplitudes[mode]`, so every mode already carries its own
learned amplitude envelope. Measured on the Bone layer in isolation — a full
render minus a Bone-muted one — the rendered T60 at each populated mode's centre
is 1.3985 s where the learner's own fitted value implies 1.4055 s, 0.8493 s
against 0.8517 s, and 2.7712 s against 2.585 s for the three modes with
reliability above 0.99. The engine does not read `boneDecaySeconds_` because that
field is redundant with the trajectory, not because a feature was forgotten.
Second, `SampleLearner.cpp`'s `estimateDecay` returns `-1/slope` of the
*natural-log* amplitude — a time constant `tau`, not a T60; the three matches
above only line up after multiplying by `ln(1000)`. Third, and decisively for
where the defect lives: the Bone layer renders **11.5 dB below** the full render
on this fixture (-42.91 dB against -31.43 dB), so the 118-2912% modal T60 errors
above are a property of the harmonic Core laid on the detected root, not of the
Bone branch. Six of twelve Bone slots were populated, two with reliability below
0.57 (0.535 and 0.219), all rendered at identical weight; on the Bone layer alone
those two are the modes whose rendered decay departs furthest from the learner's
own estimate (159% and 167%).

**Gap 6. A sounding note jumps 5.75 dB across the stereo image when another key
is struck.**
`NeuramarEngine.cpp:656-685` assigns `pan = -1 + 2*rank/(count-1)` by `ageStamp`
rank among currently sounding voices and forces `controlCountdown = 0`, so every
note-on and every retirement re-places every other voice. Holding MIDI 57 with
Air and Bone muted, the held note's own 220 Hz fundamental measures L-R = 0.000 dB
alone and L-R = **5.736 dB** in the first control period after MIDI 76 is struck,
5.769 dB thereafter (pan -0.580), and never returns while any other voice is
alive. That reproduces exactly. Because ranking is by note order rather than
pitch, a chord's layout depends on the order the notes were played: the same
{57, 64, 71} in the six possible orders gives buffers that differ by **-6.0 to
-13.5 dB** relative to the signal, which is not a rounding effect but a
completely different stereo image. Two smaller facts bound the fix. Voice
summation is over fixed voice slots, so even with every pan equalised the six
orders still differ by -146.7 dB relative and only 68% of samples match bit for
bit — float addition is not associative and note order changes which slot a note
lands in. And `:1142-1143` adds `0.08 * mutation * mutationOffset` to the pan
*outside* the Spread multiply, so at the shipping Mutation of 0.12 a single voice
is placed up to +/-0.0096 off centre even at Spread 0: the instrument is not
exactly mono at Spread 0 today.

**Gap 7. Timbre barely changes with dynamics.**
`NeuramarEngine.cpp:519` sets `velocityGain = velocity * (0.72 + 0.28*sqrt(velocity))`;
`:770-774` sets `touchTilt = touch * 0.55 * (velocity - 0.72)` and
`touchAirGain = 1 + touch * 1.35 * (velocity - 0.72)`. At Touch 0, velocity 0.05
to 1.00 moves level +28.15 dB and the spectral centroid by exactly 0.0%. At the
shipping Touch of 0.35 the same span moves level +28.46 dB and the centroid
1047.7 to 1322.2 Hz, which is **+26.2%** on the *sustained* fixture, not the
+5.5% first recorded; at Touch 1.0 it is +96.5%. This gap is therefore much more
fixture-dependent than it was written as, and on a bright fixture Touch is not
obviously undersized. It carries no step in this pass and the number is corrected
here only so that a later pass does not plan against +5.5%.
A piano's energy above 2 kHz rises 15-25 dB from pp to ff. The
level-matched onset waveform of v=0.10 correlates at 0.99404 with v=1.00 over the
first 20 ms; v=0.85 against v=1.00 is 0.99981.

**Gap 8. Every learned trajectory replays in absolute seconds at every key.**
`NeuramarEngine.cpp:1379` advances `voice.modelTimeSeconds` by
`evolutionRate / sampleRate_` with no term involving `voice.transpositionRatio`.
A model learned from a plucked string or a tine therefore rings for very nearly
as long two octaves up as it did at the source pitch. On a real string, damping is
frequency-dependent (Desvages, Bilbao, Ducceschi and Chabassier, POMA 28, 035005,
2017; Cheng, Dixon and Mauch, ICASSP 2015), and decay time falls steeply across
the compass. Measured on the fast *decay* fixture with Orbit 0, Air and Bone
muted, as T20 — the time for the played fundamental to fall 20 dB below its 80 ms
level, which is the only decay estimator that stays inside the un-frozen part of
the trajectory — T20 is 0.4850 s at MIDI 33, 0.4600 s at MIDI 57 and 0.4500 s at
MIDI 81. `T20(81)/T20(57)` is **0.978** and `T20(33)/T20(57)` is 1.054 where a
source whose per-partial decay follows `tau(f) = tau_1 (f/f_1)^-0.75` predicts
0.354 and 2.83. Against Järveläinen and Tolonen's 75%-140% tolerance window that
is 276% of correct going up and 37% going down. Two qualifications the original
statement lacked. It is not exactly 1.00 — the register path leaves a residual
5% tilt of its own. And **at the shipping defaults this gap does not manifest at
all**: Orbit is on, so the model clock ping-pongs inside the loop and the note
never decays past the loop region at any key. Gap 8 is a defect of Orbit-off
playing, and any fix for it lands on top of whatever gap 3's fix leaves behind.
The uniform per-voice release of gap 1 compounds it.

**Gap 9. Every note on the keyboard is made exactly equally loud.**
The same `registerGain` block at `:991-1012` normalises rendered power to a
reference that does not depend on the played note. Measured across four learned
models: the stiff-string model spans 0.09 dB of RMS over MIDI 24-96, an `h^-1.6`
source 0.06 dB over MIDI 36-96, an `h^-1.0` source 0.06 dB, an `h^-0.6` source
0.08 dB. Re-measured at the same settings the existing
`testKeyboardLevelFlatness` uses — Register 0, Touch 0, Air and Bone off,
Orbit 0 — the *sustained* fixture spans 0.05 dB and the *decay* fixture 0.17 dB
over MIDI 12-108 at Body Lock 1. A piano at constant hammer velocity varies over
10 dB across its compass. Above MIDI 96 the flatness breaks the other way as the
gain saturates: 3.1 dB on the *sustained* fixture at shipping defaults, 4.1 dB on
the *breathy* one, and **21.2 dB** across MIDI 12-108 on a 96-partial formant
source (11.3 dB at Body Lock 1), where the first measurement recorded 13.4 dB.
`registerGain` reaches its 4.000 ceiling at MIDI 96-108 on every fixture tried.
`README.md` documents the flatness as intentional, so this is a design choice
measuring wrong against physics rather than an oversight — but the saturation
fade at the top is not a design choice, and it is the only part a step could
honestly claim.

**Gap 10. Below about MIDI 30 the instrument gets brighter as you play lower.**
`NeuramarEngine.cpp:783-787` computes
`registerContribution = -registerTilt * 0.30 * log2(transpositionRatio)`;
`:816-819` applies the tilt as `pow(index, tilt)` over all 256 rendered slots;
`:846-853` grows `desiredHarmonicCount` to 256 as the note falls. The tilt is a
power law in harmonic *index*, so the same setting brightens a low note far
harder because it has four times as many indices to climb. Measured at shipping
defaults, model A: centroid 407.2 Hz at MIDI 30, 525.7 Hz at MIDI 24, 963.5 Hz at
MIDI 20 — a note ten semitones lower is 7.5 dB brighter. Model B inverts
independently: 300.8 Hz at MIDI 40 rising to 857.3 Hz at MIDI 20. At Body Lock 0
the centroid/f0 ratio is a flat 3.36 at every note and at Body Lock 1 the
absolute centroid is a flat ~700 Hz; only the intermediate blend inverts.

**Gap 11. The fitted stiff-string coefficient does not key-track.**
`NeuramarEngine.cpp:1185` calls `refreshHarmonicStretch(model->inharmonicity() *
parameters.stretch)` once per block, `:284-297` builds one global ratio table,
and `:1306` advances every voice's phase through it. Fitted B = 0.000602 against
a source B of 0.000600, so the fit itself is excellent — and partial stretch is
32.7 cents at partial 8, 124.1 cents at partial 16 and 257.8 cents at partial 24,
identically to the printed precision at MIDI 36 (65.4 Hz) and at MIDI 93
(1760 Hz). Zero variation across 4.75 octaves where a real string family varies
by roughly a factor of 100 in B. Against the ARLO threshold law
`ln B = 2.57 ln f0 - 26.5`, the audibility threshold falls about three orders of
magnitude from a treble source to a bass rendering of it, so a stretch that was
subliminal at the source pitch is grossly above threshold two octaves down.

**Gap 12. Every simultaneously sounding voice shares one pitch and amplitude
trajectory.**
`NeuramarEngine.cpp:712-763` gives voices an `evaluationModelTime` that differs
only by the mutation start offset, and `:1133-1139` and `:1266-1267` ramp
`frame.pitchRatio` identically per voice. On a source carrying 5 Hz / 45-cent
vibrato, two note-ons of the same key give pitch-trajectory correlation 1.0000 at
Mutation 0.00, 0.9951 at the shipping 0.12, and 0.9262 (best lag +23 ms) at
Mutation 1.00. Depth is accurate (+/-40.6 cents rendered against +/-45 in the
source), so the defect is the lock, not the depth. Maximum available
decorrelation at the default is +/-3.46 ms on a 1.6 s memory: 6.2 degrees of a
5 Hz cycle. There is also no coupling of any kind between voices — the output is
a linear sum of independent oscillator stacks, so a triad is three synthesizers
rather than one instrument. Simionato and Fasciani model exactly this coupling
with a separate FiLM-conditioned chord module (arXiv:2409.06513).

**Gap 13. Gravity is a level control as well as a tone control, and can approach
the output guard.**
`NeuramarEngine.cpp:781` folds the brightness term into `referenceTilt`, so
`registerGain` divides it out rather than levelling it. Sweeping the panel's
Gravity -1.00 to +1.00 at Output unity and velocity 1.0 on the *sustained*
fixture moves RMS from -7.52 to -4.31 dB (**3.21 dB**) and peak from -2.34 to
+6.79 dBFS at MIDI 57, and from -0.06 to **+17.55 dBFS** at MIDI 24 (17.6 dB of
peak swing). Nothing pinned the pathological-state guard at `:55-64` and
`:1409-1414` on this fixture — the guard sits at 7.95 linear, `+18.01 dBFS`, and
the worst case reached +17.55 dBFS — so the first measurement's 12.5 dB RMS,
23.07 dB peak and 0.350% pinned samples are a harder fixture's numbers, not a
constant. The structural point stands: Gravity is not level-neutral, the margin
at Output 0 dB is under half a dB on a loud fixture, and at the shipping Output
of -6 dB it does not come close.

**Two more things the audit found that are not defects but bound this plan.**
The trajectory grid puts 48 of its 128 frames in the first 120 ms and warps the
remaining 80 as `onsetEnd + (1 - onsetEnd) * p^1.24`, so the final gap is about
1.55% of the post-onset span. Against a 5.9 Hz vibrato, whose half-period is
85 ms, that is exactly two samples per cycle at a 5.6 s source and unrepresentable
at 10 s. `README.md` line 170 claims the contour retains slow vibrato; that is
true at two seconds and false at six, and the boundary is stated nowhere.
(Mellody and Wakefield, JASA 107(1):598-611, 2000, measure violin vibrato at a
mean 5.9 Hz and +/-15.2 cents, and find realism lives "almost entirely" in the
per-partial amplitude fluctuations.) Separately, the Air branch draws independent
white noise per band and filters it (`:1334`), stationary within a control
period; real breath, bow and jet noise is amplitude-modulated at the fundamental
period, and US5508473A states the failure mode plainly: "There is no perceptual
fusion of the noise and periodic sounds, and the listener hears two sources."

**Strengths this pass must not regress.**

- Alias floor: worst spectral line below the played fundamental is -111.5 dB at
  MIDI 100 (root+43 semitones) at Stretch 1.0 and -112.7 dB at Stretch 2.0,
  better than the -108.2 dB this document already records. No non-finite output
  at Formant +/-24 st, Stretch 2.0, Gravity 0 or 1, MIDI 12 or 108, Output +6 dB.
- Per-partial decay reproduction: on a source whose per-partial T60 spans a
  factor of 13, the render's drop from 0.08 s to 0.60 s is within 0.00 dB at
  partials 1-7, 0.01 dB at 8-12, 0.00 dB at 16 and 0.02 dB at 20. This is the
  part of the instrument that is genuinely best-in-class. Confirmed
  independently: on the *decay* fixture (`tau_1` 0.9 s, `p` 0.75) the rendered
  fundamental's T60 at the root is 6.217 s against a ground truth of
  `0.9 * ln(1000)` = 6.217 s, and the model's own per-partial time constants
  recover the source's to within a few percent out to partial 24.
- Per-mode Bone decay reproduction: each Bone mode carries its own network
  amplitude trajectory, and on the *struck* fixture the Bone layer in isolation
  renders the three high-reliability modes at 1.3985, 2.7712 and 0.8493 s against
  the learner's own fitted 1.4055, 2.585 and 0.8517 s. This is also a strength,
  and it is the reason step 3 of the first draft of this plan was struck.
- Beating survives: a source partial beating at 4.3 Hz with 36.59 dB of
  peak-to-trough depth renders with 29.67 dB, while a non-beating neighbour
  reproduces 5.68 dB against the source's 5.69 dB. Beats are neither smeared away
  nor fabricated.
- Stiff-string estimation: fitted B = 0.000602 against 0.000600.
- Exact determinism when asked for: at Mutation 0 and Noise 0, two identical
  note-ons differ by -318 dB. Any replacement variation mechanism must keep an
  exact-zero setting.
- No clicks at state boundaries: the voice-steal tail is a raised cosine with
  bounded carry, pan changes ramp over a control period, Output has a 6 ms
  smoother, and the Air filterbank is normalised to unit expected RMS per band so
  level is independent of centre frequency and sample rate.

### Plan

Six steps, cheapest and most audible first. Each is a single commit, green
before it lands. Steps 4 and 5 share one fitted damping exponent, step 5 depends
on step 4, and step 5 also has to land after step 3 because at the shipping
defaults Orbit is what decides whether a note decays at all. Two steps from the
first draft of this section — band-limiting the register reference, and giving
each Bone mode its own fitted decay — were struck on re-measurement and are
recorded, with the measurements that struck them, under *Considered and not
planned*.

- [x] **1. Stop applying the register compensation to Air and Bone.**
  `registerGain` at `NeuramarEngine.cpp:991-1012` does not scale the Core, it
  *normalises* it: after `:1013-1015` the Core's power is `referencePower`, which
  barely depends on the played note. `:1037-1039` and `:1078-1079` then
  *multiply* Air and Bone by that same factor, so the noise-to-tone ratio comes
  out as `A / sqrt(renderedPower)` and rides every Core-only rendering artefact —
  the anti-alias taper, the Body-Lock envelope running out of range — straight
  into the layer balance. Delete the `* registerGain` from the Air and Bone target
  expressions and leave the Core normalisation alone. The physical statement is
  that a compensation for partials the *Core* could not render is not evidence
  about how much breath noise or body ring the source had; the two are measured
  independently and should stay independent. This is the fix the block's own
  comment and `README.md:250-255` argue against, and the argument is inverted:
  applying one gain to a normalised quantity and an un-normalised one is exactly
  what breaks the balance it claims to preserve. Closes gap 2. *Verified by*: new
  `testRegisterLayerBalance` in `Tests/NeuramarEngineTests.cpp` renders MIDI 12,
  24, 36, 48, 60, 72, 84, 96 and 108 at Mutation 0 on a fixture with a strong
  noise bed, and reports the Air layer by subtracting an Air-muted render from a
  full one — the
  two renders differ in `parameters.air` and nothing else, so the difference is
  exactly the Air layer and the control moves one thing. With Body Lock 1 and
  Register 0, which freezes the Air band centres so `registerGain` is the only
  remaining variable, it asserts the Air-to-Core ratio spread across those nine
  notes is **below 1.0 dB**, where a scratch build of this change measures
  0.10 dB and the shipping engine measures 21.55 dB. (Both of those numbers were
  written before the gate was; the shipped form of this assertion measures the
  isolated Air level instead, for the reason in the note below.) At shipping
  defaults, where
  the Air centres do move with pitch and are edge-limited near Nyquist, it
  asserts the same spread is **below 9.0 dB**, against 6.77 dB for the change and
  19.20 dB today. (Struck: measured over the full keyboard this spread *rises*
  with the change, 8.89 to 11.41 dB, and the 6.77 dB figure is unreachable. See
  the note below.) A third assertion covers **Bone**, isolated the same way by
  subtracting a Bone-muted render from a full one: at Body Lock 1 and Register 0
  the isolated Bone layer's own level spread across those nine notes must be
  **below 1.0 dB**. At that setting the Bone target depends on the played note
  through `registerGain` and through nothing else — the Bone centres are frozen,
  `frame.boneAmplitudes` does not depend on the played note, and Mutation is 0 —
  so this is the sharpest form the gate can take. The test asserts first that at
  least one Bone mode carries non-zero reliability, since `:1069` gates a
  zero-reliability mode to silence and a silent layer would pass by default. A
  fourth assertion guards the Core: `testKeyboardLevelFlatness` must stay green
  unchanged, and the Core-only level spread across MIDI 12-108 at Body Lock 1
  must move by less than 0.2 dB (measured: 0.14 dB against 0.07 dB). (Shipped as
  an absolute bound instead, because a test cannot see the pre-change build; on
  the fixture it builds the Core spread is 2.98 dB and does not move at all.)
  *Contract corrected in preflight.* Every assertion first written here measured
  Air or Core, so an implementation that deleted `* registerGain` from the Air
  expression and left the Bone one alone satisfied the whole contract while
  leaving half the defect in place. Preflight measured the missing half on a
  scratch build of this change against a noise-bed fixture rooted at 220 Hz: the
  isolated Bone layer's level spread across MIDI 12-108 at Body Lock 1 goes from
  22.2 dB to 0.000 dB, and its ratio to Core from 24.40 dB to 2.16 dB, which is
  that fixture's whole Core spread. The 9.0 dB bound on the shipping-default Air
  case is fixture-sensitive in the other direction — the same scratch build
  measures 13.5 dB after the change on a fixture rooted at 220 Hz, because the
  moving Air centres run into the edge limit near Nyquist at the top of the
  keyboard — so that bound must be re-measured on the fixture the test actually
  builds before it is written down.
  *What actually shipped*: the engine change is exactly the one specified —
  `* registerGain` is gone from both the Air target at `:1037-1039` and the Bone
  target at `:1078-1079`, the Core normalisation is untouched, and the comment
  that argued for the old behaviour is replaced by the normalised-versus-scaled
  reasoning. `testRegisterLayerBalance` in `Tests/NeuramarEngineTests.cpp`
  builds its own fixture, `makeRegisterBalanceSample`: 40 harmonics at 220 Hz
  falling as `h^-0.82`, six steady inharmonic modes at 1.35-6.35x the
  fundamental, and the same high-passed noise bed the Air separation fixture
  uses. The learner detects a root of 219.93 Hz (MIDI 57) and populates all
  twelve Bone slots, six at reliability 1.000, so the guard that at least one
  mode is non-zero passes on evidence rather than by luck. Three of the four
  contracted assertions changed, each for a measured reason.
  **The Body Lock 1 gate moved from the Air-to-Core ratio to the isolated Air
  level.** On this fixture the Core's own level is not flat at Body Lock 1: it
  spreads 2.98 dB across MIDI 12-108 because `registerGain` saturates at its
  4.0 ceiling on MIDI 96 and 108, which is gap 9 and not something this step
  touches. The ratio therefore carries the Core's saturation fade and no correct
  implementation could hold it under 1.0 dB. The isolated Air level is the
  quantity `registerGain` actually moves, and it is the same form preflight
  already chose for Bone: it goes from **23.36 dB to 0.36 dB**, and the isolated
  Bone level from **23.26 dB to 0.000 dB**. The 0.36 dB of Air residual is not a
  register effect — at Mutation 0 the sixteen Air noise seeds are still drawn
  from `mixHash(midiNote)` (`:494-506`), so every note gets an independent noise
  realisation and its windowed RMS differs by that much.
  **The Core guard is an absolute bound, not a before-and-after comparison**,
  since a test cannot see the pre-change build: the Core level spread at Body
  Lock 1 must stay under 4.0 dB, against 2.98 dB measured. What it defends is real — a
  scratch build that also dropped `registerGain` from the Core loop measures
  **26.28 dB** on the same fixture. `testKeyboardLevelFlatness` is green and
  unchanged.
  **The 9.0 dB shipping-defaults bound was struck, and the direction of that
  claim was wrong.** Across the full MIDI 12-108 range at the shipping defaults
  this change makes the Air-to-Core spread *worse*, 8.89 dB to **11.41 dB**,
  because two independent errors were partially cancelling: at Body Lock 0.65
  the Air centres scale as `ratio^0.35`, so above about MIDI 72 the top of the
  sixteen-band set crosses the filterbank's 18 kHz edge fade and is legitimately
  attenuated, and `registerGain` climbing to its ceiling at the same notes was
  masking that. The mechanism is settled by pulling every centre back into range
  with Formant -24 st, where the same full-keyboard spread after the change is
  **0.354 dB**. The edge taper is a Body Lock property this step does not touch,
  so the shipping-defaults gate is stated over MIDI 12-72, the register in which
  every Air band is still in range: **7.82 dB before, 1.51 dB after**, bound at
  3.0 dB.
  Reverting both deletions and rerunning fails all three balance assertions at
  23.364 dB (Air), 23.256 dB (Bone) and 7.820 dB (shipping-defaults Air-to-Core)
  against bounds of 1.0, 1.0 and 3.0 dB. Suite green, 3/3 ctest suites in
  41.9 s.
  `README.md:250-255` still states the inverted claim this step disproves and
  needs the correction; no engine step owns it.

- [x] **2. Pan by pitch, fixed at note-on, instead of by chord rank.**
  `refreshVoicePans()` at `NeuramarEngine.cpp:656-685` recomputes every sounding
  voice's pan from its rank among the currently sounding set, so striking a
  second key drags the first note 5.736 dB across the image in one control period
  and the same three notes played in six different orders give six different
  stereo images, differing by -6.0 to -13.5 dB relative. Replace it with a pan
  assigned once at note-on and never revised:
  `pan = clamp(spread * (midiNote - correctedRootMidi) / 24, -1, +1)`. On a
  piano, harp, marimba or guitar the sounding elements are laid out monotonically
  in pitch across the width of the radiating body, and a string that is already
  vibrating does not move when another is struck; both properties fall out of
  making pan a function of the note rather than of the chord. Order-dependence
  goes with it, and `refreshVoicePans()` leaves the note-on and voice-retire paths
  entirely, taking its forced `controlCountdown = 0` with it. Two things this step
  must handle that the first draft did not. It **changes** the documented
  behaviour that a single note is always centred: at Spread 0.58 a note two
  octaves from the root now sits at +/-0.58, which is the point of the change but
  is not what the engine promises today, so the comment at `:544-546` and the
  existing `"a single note was not centred at maximum stereo spread"` assertion
  have to be rewritten rather than merely left passing — that assertion currently
  survives only by accident, because it plays the root note. And the pan jitter at
  `:1142-1143` is added *outside* the Spread multiply, so Spread 0 is not exactly
  mono today; fold it inside the multiply in the same commit so that the claim
  "Spread 0 is exactly mono" becomes true rather than merely repeated. Closes
  gap 6. *Verified by*: `testHeldNoteDoesNotMoveInImage` holds MIDI 57 with Air
  and Bone muted, measures the least-squares L and R amplitude of its own 220 Hz
  fundamental over 0.30-0.45 s, strikes MIDI 76 at 0.50 s, and asserts the held
  note's L-R difference over 0.51-0.60 s and again over 0.70-0.90 s changes by
  less than 0.05 dB, where it changes by 5.736 dB and 5.769 dB today. A second
  assertion renders {57, 64, 71} in the six possible note orders **at Mutation
  0** and requires the six buffers to agree to **better than -120 dB** relative
  RMS — not bit for bit.
  Bit-identity is unreachable and the test must not ask for it: voices sum in
  voice-slot order, note order decides which slot a note lands in, and float
  addition is not associative, so with every pan already equal the six orders
  still differ by -146.7 dB and only 68% of samples match exactly. Today the same
  six differ by -6.0 to -13.5 dB, so -120 dB separates the two cases by 130 dB.
  A third assertion renders a single note at Spread 0 and Mutation 0.12 —
  deliberately at the shipping Mutation, because this one exists to catch the pan
  jitter — and requires L and R identical to within 1e-7, which fails today.
  *Contract corrected in preflight.* The six-order assertion did not say what
  Mutation it ran at, and at any non-zero setting it cannot be met by a correct
  implementation. `mutationOffset` is drawn at note-on from a hash of `midiNote`
  and the order-dependent `ageStamp` (`NeuramarEngine.cpp:494-518`) and then
  fixes the detune at `:703-705`, the harmonic and Air variation phases at
  `:523-529`, the sixteen Air noise seeds at `:505-516` and the model-clock start
  offset at `:731-734`, so permuting the note-ons changes the rendered audio for
  reasons that have nothing to do with panning. Preflight measured the six orders
  with the rank pan already removed (Spread 0): -145.9 dB at Mutation 0, -4.1 dB
  at 0.10 and -2.9 dB at 0.12.
  *What actually shipped*: the engine change is the one specified.
  `refreshVoicePans()` is deleted outright, along with its declaration in
  `NeuramarEngine.h`, its call at the end of `noteOn()` and its call on voice
  retirement in the render loop; the forced `controlCountdown = 0` went with it,
  so the only remaining early control update is the fresh voice's own at
  note-on, where `firstControlFrame` is true anyway. In its place `noteOn()`
  sets `voice.pan = (midiNote - correctedRootMidi) / 24` once, using the
  `correctedRootMidi` that was already being computed two lines further down for
  the phase ratio, and the pan jitter at the target site is folded inside the
  Spread multiply. Two comments elsewhere in the file cited a voice-pan refresh
  as the thing that can re-assert a control frame mid-ramp — one in
  `Bandpass::set()` and one guarding the harmonic-count contraction — and were
  corrected to drop a mechanism that no longer exists.
  **Spread stays live rather than being frozen into the pan.** The step writes
  the placement as `clamp(spread * (midiNote - correctedRootMidi) / 24, -1, +1)`
  assigned at note-on. What shipped stores only the note-dependent factor on the
  voice and leaves the `* parameters.spread` and the outer clamp where they
  already were, in `updateVoiceControl()`, so the rendered pan is that same
  expression evaluated every control period. It is identical arithmetic — the
  clamp is still outside the Spread multiply, which is what makes Spread 0
  collapse the jitter with it — and it keeps a host automating Spread audible on
  notes that are already sounding, which freezing the product at note-on would
  have silently removed.
  `testHeldNoteDoesNotMoveInImage` in `Tests/NeuramarEngineTests.cpp` runs on
  the model `learnFixture()` already builds, whose root is 219.96 Hz at MIDI 57,
  so it costs no extra learn. The second key it strikes is root+19, chosen so
  none of its partials lands on the held note's fundamental, and the chord is
  root, root+7, root+14. Measured on that fixture, before and after: the held
  note's L-R goes from **0.000 dB alone to 5.7546 dB** in the first control
  period after the second note-on and 5.7541 dB once settled, against
  **0.00016 dB and -0.0000083 dB** after; the worst of the six note orders goes
  from **-6.14 dB to -146.88 dB** relative, which lands on preflight's predicted
  -146.7 dB float-summation floor; and Spread 0 at Mutation 0.12 goes from a
  worst channel difference of **1.745e-3 to exactly 0**. The step's recorded
  5.736 / 5.769 dB and -6.0 / -13.5 dB are the same finding on the audit's own
  fixture; on this one the six orders span -6.14 to -12.04 dB.
  **The existing "single note centred" assertion was rewritten, not left
  passing.** It played the root note, which this change still centres, so it
  survived the change by accident. `testStereoPanAndOverlappingNoteOff` now
  asserts both halves: the root note is still mono to 1e-7 at Spread 1, and a
  note an octave above the root sits at -4.771 dB left-against-right, which is
  what the constant-power law gives for `pan = +0.5`. Measured -4.771 dB after
  the change and 0.000 dB before, so the pair can no longer pass on an engine
  that centres everything.
  Reverting all four parts of the change and rerunning fails five assertions:
  the octave-placement assertion at 0.000 dB against -4.771 dB, both held-note
  assertions at 5.7546 dB and 5.7541 dB against a 0.05 dB bound, the six-order
  assertion at -6.138 dB against -120 dB, and the mono assertion at 1.745e-3
  against 1e-7. Reverting only the jitter fold and keeping the pitch pan fails
  the mono assertion alone, at the same 1.745e-3, which is what pins that
  assertion to that one line. Suite green, 3/3 ctest suites in 41.0 s.
  `README.md:264` still claims that "adding or removing a chord note glides the
  remaining voices into their new positions instead of stepping them"; after
  this change the remaining voices do not move at all, so the sentence needs
  replacing rather than softening. `README.md:134` describes Horizon as widening
  voices across the stereo field, which is still true but no longer says what
  decides where a voice lands. No engine step owns either line.

- [x] **3. Make Orbit a forward, level-continuous, non-repeating sustain.**
  The triangle fold at `NeuramarEngine.cpp:723-734` gives a held note an envelope
  autocorrelation of 0.979 at one ping-pong period and 0.992 at two, 2.99 dB of
  level pumping on a decaying fixture and 4.68 dB on a percussive one that never
  stops, and a time-reversed second leg. Reversing a decaying trajectory is a
  negative-damping segment, which is the one thing a passive resonator cannot
  produce, and no acoustic sustain repeats exactly. Replace the fold with three
  changes. Traverse the loop forward only, wrapping `loopEnd` back to `loopStart`
  with an **equal-gain** crossfade of one control period — the two sides blended
  linearly, weights summing to 1 — so no leg runs backwards. The crossfade acts
  on the control-rate amplitude targets, not on two independent audio streams:
  `voice.harmonicPhases` advance from `phaseStep` alone (`:1296-1308`) and never
  consult the model clock, so both sides of the wrap drive the *same* oscillators
  at the *same* phases and are perfectly correlated. Blending two equal
  amplitudes with equal-power weights of `1/sqrt(2)` would therefore raise the
  level 3.01 dB at the midpoint of every wrap, which is a wrap-rate pumping
  artefact of exactly the kind this step exists to remove. Equal-gain is also
  what the existing target ramp already does for free: `:1126-1128` steps each
  amplitude linearly from its old value to its new one over one control period.
  Advance the wrap point each pass by `(phi - 1) = 0.6180339887` of the loop
  length, keeping the read segment inside `[loopStart, loopEnd]` and wrapping the
  offset modulo the loop length, so successive passes read a different part of the
  trajectory and the result is quasi-periodic rather than periodic — the golden
  ratio is the choice that maximises the smallest gap between successive wrap
  offsets, so no short lag ever lines up. And remove the loop region's own **level
  trend** across each pass — fit and divide out the mean log-level slope between
  `loopStart` and `loopEnd`, so the wrap is level-continuous — rather than
  replacing the level with the region's mean. This is a deliberate narrowing of
  the first draft, which proposed holding the level flat at the loop mean: a
  sustained note is quasi-stationary in *trend*, not in fine structure, and
  flattening the level outright would delete any tremolo, breath pulsing or
  beating the loop region carries, which the strengths list above records as
  something this pass must not regress. It would also not fit the parameter: Orbit
  is a *time* blend, `oneShotTime + orbit * (orbitTime - oneShotTime)`, and a
  level hold is not expressible inside it. Detrending is. Closes gap 3.
  *Verified by*: `testOrbitSustainIsNotPeriodic` holds a note for 6 s at shipping
  defaults on the *decay* fixture with Air and Bone muted — muted because the Air
  noise stream is what blurs the envelope and the test must measure the
  trajectory, not the noise — takes a short-time RMS envelope over frames of a
  **whole number of played-fundamental periods** (11 periods, 50.0 ms at the
  220 Hz fixture), and asserts over t in [2, 6] s that the envelope's
  **peak-to-peak spread is below 1.5 dB**, against 2.49 dB today at that frame
  length (2.56 dB in preflight, on a fixture whose partial rolloff differed) and
  4.68 dB on the percussive fixture. That bound, not
  an autocorrelation, is the primary gate, because it is the one that stays
  well-posed after the fix: a normalised autocorrelation of a mean-removed
  near-constant envelope is 0/0, and the shipping engine already scores above 0.98
  at *every* lag on the sustained fixture, whose envelope only moves 0.56 dB — the
  metric proposed in the first draft cannot tell a pumping sustain from a steady
  one. Three supporting assertions. The largest rise between consecutive frames
  after t = 2 s must be below 0.25 dB, which is a direct test that no leg runs
  backwards on a monotonically decaying trajectory: today it is 0.51 dB, and a
  frozen model clock — the stationary control — gives exactly 0.00 dB. The
  sustain must still be **moving**: the per-frame spectral centroid over the same
  span must have a peak-to-peak spread of at least 1% of its own mean, which is
  8.26% for the traversing clock today with the magnitude-weighted centroid the
  test uses (3.35% in preflight, on a differently defined centroid) and 0.0001%
  for a frozen one. And the
  sustain must not **repeat**: the normalised autocorrelation of the mean-removed
  per-frame centroid must be below 0.60 both at the new forward period
  `loopLength` and at the old ping-pong period `2 * loopLength`, against 0.88 and
  0.86 today — as **magnitudes**, see the shipping note below.
  *Contract corrected in preflight.* Three things were wrong, all measured on the
  shipping engine by a scratch program against a decay fixture (32 partials,
  `tau_1` 0.9 s, `p` 0.75, root 220.01 Hz at MIDI 57, learned loop
  [1.0082, 1.2859] s, so `loopLength` is 0.2777 s). First, the 20 ms
  envelope frame first specified here is 4.4 periods of a 220 Hz fundamental, so
  its RMS ripples by 0.335 dB peak to peak and 0.333 dB frame to frame on a
  *perfectly stationary* render — the 0.25 dB rise bound could not be met by any
  correct implementation. At a whole number of periods the same stationary render
  measures 0.0000 dB and the bound is well-posed. Second, both remaining
  assertions were stated on the level envelope, which this step's own detrending
  is designed to flatten: an implementation that simply froze the clock at
  `loopEnd`, and one that wrapped forward from a *fixed* point and so repeated
  exactly at `loopLength`, both passed the spread and rise gates and both had the
  autocorrelation gate skipped by its "spread exceeds 0.5 dB" guard. Moving the
  non-repetition test onto the per-frame spectral centroid fixes both: detrending
  does not touch it, a frozen clock collapses it to 0.002% of its mean, and a
  fixed wrap point is caught at lag `loopLength`. Third, the autocorrelation gate
  was stated only at `2 * loopLength`, which the golden-ratio advance is not the
  only way to defeat; testing `loopLength` as well is what makes the advance
  load-bearing.
  *What actually shipped*: both halves of the mechanism, as specified, plus one
  correction to the contract and one to the metric.
  **The wrap is stateless and the crossfade is the one that was already there.**
  `NeuramarEngine.cpp:784-796` replaces the triangle fold with
  `loopOffset = fmod(fmod(elapsed, L) + fmod(floor(elapsed / L) * 0.618034, 1) * L, L)`,
  which reads the region forward only and starts each pass 0.618 of the loop
  length further in. No crossfade code was written, because the step's own
  reading of `:1126-1128` is right: every wrap is a step in the control-rate
  amplitude *targets*, and the existing ramp already walks each amplitude
  linearly from its old value to its new one over one control period, which is
  an equal-gain crossfade of one control period. Nothing was added that could
  make it equal-power.
  **The detrend is fitted once per model, on the whole decoded frame.**
  `setModel()` calls a new `fitLoopLevelSlope()` (`:370-433`) that least-squares
  fits the slope of `log` total frame amplitude — harmonics, Air and Bone
  together — over 32 points across the loop region, and
  `updateVoiceControl()` multiplies the decoded frame by
  `exp(-slope * orbit * loopOffset)`. Applying it to the *frame* rather than to
  the finished targets is what keeps it out of the register compensation:
  `registerGain` is a ratio of two powers taken from the same frame, so both
  sides move together and it is left exactly where it was. Scaling by
  `parameters.orbit` is what makes Orbit 0 bit-identical to the old engine. The
  fitted slope is -1.137 nepers/s on the decay fixture, which is **-2.74 dB
  across the loop region** — that trend is the pumping, and it is the whole of
  it. The slope is clamped so the correction across one pass cannot exceed
  12 dB; the five fixtures measured need 0.04 to 9.5 dB.
  **The autocorrelation gate is on the magnitude, not the signed value.** At lag
  `loopLength` the shipping engine measures **-0.881**, not +0.88: `loopLength`
  is half the ping-pong period, so it is the trough, and the preflight figure was
  its magnitude. Stated signed, the gate at that lag would have passed the very
  engine it exists to fail. `testOrbitSustainIsNotPeriodic` takes `|r|` at both
  lags.
  **The centroid is magnitude-weighted, not power-weighted.** Squaring hands
  almost the whole weight to partial 1 on a source with this rolloff: the
  power-weighted centroid sits at 228.6 Hz and moves only 1.53% of its own mean
  where the magnitude-weighted one sits at 291.2 Hz and moves 7.30%, against a
  1% floor. The power-weighted form would have left that assertion 0.5% from its
  bound for no reason. It is also measured over exactly the analysis frame
  rather than through the file's `windowedSinusoidAmplitude()`, whose fixed
  40 ms half-window would smear four frames of this length together.
  The test builds its own fixture, `makeDecayingPartialSample`: 32 partials at
  220.01 Hz falling as `h^-1.2` with `tau(h) = 0.9 h^-0.75` s, 1.6 s long. The
  learner reproduces the numbers this step was planned against exactly — root
  220.021 Hz at MIDI 57, loop [1.0082, 1.2859] s, `loopLength` 0.2777 s. Over
  t in [2, 6] s at 11-period (50.0 ms) frames, the four gates go
  **2.491 -> 0.024 dB** of level spread (bound 1.5), **0.505 -> 0.021 dB** of
  largest frame-to-frame rise (bound 0.25), **8.26% -> 7.30%** of centroid
  spread (floor 1.0), and **0.882 -> 0.324** and **0.862 -> 0.014** of centroid
  autocorrelation at `loopLength` and `2 * loopLength` (bound 0.60). A
  percussive fixture — 12 partials, `tau(h) = 0.30 h^-0.8`, plus a 4 ms noise
  burst — measures 0.033 dB of level spread and 0.027 dB of largest rise after
  the change, against the 4.68 dB recorded for the fold under gap 3.
  Reverting both halves and rerunning fails four of the five assertions at
  2.491 dB, 0.505 dB, 0.882 and 0.862; the "still moving" assertion correctly
  passes, since the old engine does move. Each half is load-bearing on its own.
  Restoring the forward wrap but leaving the detrend out fails only the two
  level assertions, at 2.169 dB and 2.151 dB — the 2.15 dB rise is the wrap step
  itself, larger than anything the fold produced. Restoring the detrend but
  leaving the triangle fold in fails only the two autocorrelation assertions, at
  0.882 and 0.862, with the level flat at 0.019 dB. And a forward wrap from a
  *fixed* point — the golden advance set to zero — fails both autocorrelation
  assertions at 0.693 and 0.837, which is what makes the advance itself
  load-bearing rather than decorative. The stationary control was run too: a
  clock frozen at `loopEnd` measures 0.00014 dB of level spread and 3.7e-6 dB of
  largest rise at the 11-period frame — which is the direct confirmation that
  the whole-period frame length makes the 0.25 dB rise bound well-posed — and is
  caught by the "still moving" gate at 0.0001% of centroid spread and by the
  `loopLength` autocorrelation at 0.746. Suite green, 3/3 ctest suites in
  46.0 s.
  Gap 8's qualification still holds after this change. It was recorded as "a
  defect of Orbit-off playing" because the fold kept a held note inside the loop
  region forever, and a forward wrap keeps it there just the same, so nothing
  about that gap has moved. What changed for step 5 is only that the clock it
  key-tracks now traverses rather than ping-pongs.

- [x] **4. Damp the release by frequency, using the source's own damping law as
  the shape.** Release is one scalar on the summed output
  (`NeuramarEngine.cpp:1199-1201`, `:1270-1273`, `:1370-1372`) and measures
  0.12 dB of frequency dependence across five octaves, with the released tail's
  spectral centroid 0.02% away from the held note's at 150 ms after note-off.
  (Both numbers are the *sustained* fixture's. On the `tau_h = tau_1 h^-0.75`
  fixture the test builds, the same measurements are 0.05 dB across partials 1
  to 32 and a centroid **0.167% the wrong way** — the released tail is very
  slightly *brighter* than the held note, not darker.)
  Real damping is frequency-dependent — air viscosity dominates at low frequency
  and internal friction at high, so decay time falls with frequency (Desvages,
  Bilbao, Ducceschi and Chabassier, POMA 28, 035005, 2017) — and a damped note
  darkens as it dies. At `setModel()`, least-squares fit the exponent `p` in
  `tau(f) = tau_1 (f/f_1)^-p` over the model's own per-partial amplitude
  trajectories, clamped to `p` in [0, 1.5]. The release then gives output slot `k`
  a time constant `tau_rel(k) = releaseSeconds * (f_k/f_1)^-p`, normalised so
  partial 1 still reaches the retirement level in exactly the Dissolve time and
  the panel's number keeps its meaning, and with the *ratio* `tau_rel(1) /
  tau_rel(k)` additionally clamped to at most 12 so that a badly-conditioned fit
  cannot turn Dissolve into a brickwall on the top of the spectrum. Air bands and
  Bone modes take the same law at their own centre frequencies, and the law is
  clamped on the slow side as well: `tau_rel(k)` is never allowed to exceed
  `tau_rel(1)`. That second clamp is not cosmetic. `f_1` is the *played*
  fundamental while the Air centres are the model's own 94 Hz to 13.6 kHz grid
  and the Bone centres are `rootFrequencyHz * ratio`, so slots below the played
  fundamental are routine rather than exotic: at the root note itself, three of
  sixteen Air bands sit below `f_1` and the lowest of them would take a time
  constant 1.89 times partial 1's, and at MIDI 108 at Body Lock 1 twelve of
  sixteen do, the lowest at 17.2 times. Without the clamp those slots outlive the
  slot the voice retires on and are cut mid-release.
  Be honest about what the exponent is: `p` is fitted from the source's *free*
  decay and applied to a *damper*, which is an analogy and not a derivation — the
  claim this step can defend is that the release should be frequency-dependent,
  that the direction and rough magnitude should come from the sound the user
  dropped in rather than from a drawn curve, and that a source with
  frequency-independent decay must get `p = 0` and keep today's behaviour exactly.
  Cost is not free and should be planned for: the per-slot release factor folds
  into the control-rate amplitude targets rather than the per-sample loop, which
  is one multiply per slot per 192 samples, but it needs a 284-float per-voice
  release-gain array (about 73 KB across 16 voices) and 284 `pow()` calls at
  note-off. (Shipped as *two* 284-float arrays — the running gain and the
  per-control-period factor — which is 36 KB across 16 voices, not 73 KB: one
  284-float array is 1.1 KB per voice and 18 KB across the engine.) Voice retirement stays on partial 1, which the slow-side clamp above
  makes the slowest slot again, so no note is cut short. Closes gap 1 and
  supplies the exponent step 5 needs.
  *Verified by*: `testReleaseDarkensTail` learns the fixture whose per-partial
  decay is `tau_h = tau_1 h^-0.75`, renders it twice at Dissolve 0.65 s — held,
  and released at t = 0.30 s — and measures each partial's least-squares sinusoid
  amplitude in both, subtracting the held drop from the released drop so that the
  model's own decay is cancelled and only the release is left. It asserts partial
  1 loses `100 dB * 0.150/0.65 = 23.08 dB` over 0.32 s to 0.47 s to within 0.5 dB
  (unchanged calibration; measured -23.077 dB today, and -22.966 dB both before
  and after on the fixture the test builds). The frequency-dependence
  assertion is taken over a **40 ms** release interval, 0.32 s to 0.36 s, with an
  analysis half-window of 10 ms: partial 8 must lose between **15 dB and 30 dB**
  more than partial 1 — `6.15 * (8^0.75 - 1)` predicts 23.1 dB — and the excess
  must increase monotonically over partials 1, 2, 4 and 8. The window is
  two-sided because an unbounded assertion would accept a release that simply
  deletes the top of the spectrum. A third assertion is that the released tail's
  spectral centroid at 150 ms after note-off is at least 20% below the held
  note's, against **0.02%** today (-0.167% on the fixture the test builds:
  today's released tail is fractionally brighter than the held note, so the
  gate has the right sign as well as the right size). The load-bearing assertion is the
  fourth: the same test learns a **control fixture with frequency-independent
  decay** (`tau_h = tau_1`, `p = 0`) and requires its partial-8 excess to stay
  below 1.0 dB and its fitted `p` below 0.10. Without that control, a hard-coded
  exponent passes every other assertion in this step. Measured on scratch fits of
  the model's own trajectories: `p` recovers as 0.734 on the `p = 0.75` fixture,
  **-0.0009** on the `p = 0` fixture and 1.467 on a `p = 1.5` fixture, with the
  root-mean-square residual in `log tau` at 0.006, 0.001 and 0.013 respectively
  against 0.332 on a sustained source and 0.551 on a struck body — so the residual
  is also the fallback signal, and a fit above about 0.15 should fall back to
  `p = 0` rather than key-track on noise. (The shipped fit recovers 0.747,
  0.0000187 and 1.447 on those three fixtures at residuals of 0.002, 0.0004 and
  0.061; the sustained source is turned away by a different gate, see the note
  below.) A fifth assertion covers retirement,
  which no assertion in the first draft touched: at Body Lock 1, at root+24 and
  root+51 semitones, the isolated Air layer and the isolated Bone layer must each
  sit at least 60 dB below their own level at note-off over the last 5 ms before
  `getActiveVoiceCount()` reaches 0. That one passes today — the single release
  scalar takes every layer down together — and exists to catch the specific way
  this step can break retirement: without the slow-side clamp, the slowest Air
  band at root+24 has fallen only 30.9 dB and at root+51 only 5.8 dB when the
  voice built on partial 1 retires underneath it. (Those are the slowest single
  band. The assertion measures the whole isolated layer, which is a sum over
  sixteen bands, so the shipped numbers without the clamp are 42.5 dB at
  root+24 and 22.7 dB at root+51 for Air and 59.4 dB and 29.0 dB for Bone.)
  *Contract corrected in preflight.* Two things were wrong. The excess assertion
  was stated on partial 32 over the 150 ms interval, where the arithmetic puts it
  out of reach of any measurement: with the ratio clamped at 12, partial 32's
  release drop over 150 ms is 254 dB, which on this fixture lands it near
  -290 dBFS. A release that deletes the top of the spectrum outright measures the
  same floor-limited number as the correct one, so the 260 dB upper bound could
  never bind. Partial 8 over 40 ms keeps both ends of the window on real signal:
  preflight measured the released partial at -71.8 dBFS there. The analysis
  window matters too — the existing `windowedSinusoidAmplitude` uses a fixed
  40 ms half-window, which smears a 40 ms interval and reports partial 1 losing
  6.042 dB where the true figure is 6.154 dB; at a 10 ms half-window it reports
  6.153 dB. And retirement had no assertion at all while the step asserted in
  prose that partial 1 is the slowest slot, which the sub-fundamental Air and
  Bone slots make false; the slow-side clamp and the fifth assertion above are
  what make that sentence true.
  *What actually shipped*: both halves of the mechanism as specified, plus one
  addition to the fit that the step did not name and one fixture finding that
  bounds what this step can do.
  **The release is expressed as the excess each slot owes over partial 1, not
  as a per-slot release.** `process()` keeps the single `releaseMultiplier` on
  `voice.envelope` exactly as it was, and `updateVoiceControl()` multiplies the
  control-rate targets by a per-slot running gain
  `exp(-((f_k/f_1)^p - 1) * t / tau_rel(1))` (`NeuramarEngine.cpp:1398-1436`).
  Partial 1's factor is identically 1, so the Dissolve time, the retirement
  decision and `testReleaseDurationSemantics` are untouched by construction
  rather than by a normalisation that has to be got right: retirement is
  0.6507 s after note-off at Dissolve 0.65 s before and after the change, at
  every key measured. The running gain is advanced by one multiply per slot per
  control period and the 284 `pow()` calls build the per-period factors once,
  in `buildReleaseShape()` (`:593-629`), at note-off — and again only if a host
  moves Dissolve mid-release. A held voice costs nothing at all: the whole
  block is behind `voice.releasing`, and behind `dampingExponent_ > 0`, so a
  source that fits `p = 0` renders bit-identically to the previous build.
  **The exponent is fitted on a warped scan, which the step did not call for
  and the short-decay fixture needs.** `fitDampingExponent()` (`:456-571`)
  evaluates the model at 64 points, fits each partial's own time constant in
  log amplitude between its peak and the point where it has lost 40 dB, and
  regresses `log(1/tau)` on `log h`. The scan points are placed at
  `(point/63)^1.24` of the duration — the same 1.24 the trajectory grid itself
  uses past the onset — because a uniform scan under-samples exactly the early
  span where the fast partials live and have already finished. On the
  `tau_1 = 0.20 s` fixture step 5 uses, a uniform scan recovers **p = 0.703 at
  a residual of 0.056** where the warped one recovers **0.735 at 0.004**; on
  the `tau_1 = 0.9 s` fixture the two agree to 0.003. Two gates decide whether
  the answer is evidence: a partial that loses less than 6 dB across its own
  fit window is dropped, and if fewer than six survive the fit is abandoned.
  **The sustained fixture is turned away by the six-partial gate, not by the
  residual gate** the step predicted — only four of its partials decay at all —
  and both paths return exactly zero, so the distinction costs nothing.
  **A source with a sustained noise bed cannot be fitted at all, and that is
  the honest answer rather than a defect.** Adding the *breathy* fixture's
  high-passed noise bed to the `p = 0.75` partials floors every fast partial's
  trajectory on the residual instead of on its own decay: the recovered
  exponent is **-0.27 at a residual of 0.51**, so the residual gate declines and
  the release keeps today's behaviour. The scatter is where it should be — the
  fit is clean out to about partial 17 and then the time constants climb back
  from 0.11 s to 0.56 s as the partials meet the noise floor. A breathy or
  bowed source therefore gets nothing from this step, which is the same
  conclusion the step's own `p = 0` control reaches by a different route. It is
  also why the test's retirement fixture carries a resonant body and no noise
  bed.
  `testReleaseDarkensTail` in `Tests/NeuramarEngineTests.cpp` learns three
  fixtures. `makeDecayingPartialSample` gained a decay-exponent parameter, so
  the frequency-dependent fixture and the `p = 0` control are the same
  generator at `0.75` and `0.0`; step 3's `testOrbitSustainIsNotPeriodic` passes
  `0.75` and is otherwise unchanged. The third,
  `makeDampedBodyNoteSample`, is those same partials plus six long-ringing
  inharmonic modes at 1.37 to 8.55 times the fundamental, deliberately placed
  clear of every integer multiple: the plain partial fixture's Bone layer
  measures **-114.6 dBFS** at note-off against this one's **-47.1 dBFS**, and a
  layer that quiet cannot show whether a body slot outlived its voice. `windowedSinusoidAmplitude` was split so the analysis half-window
  is a parameter, because preflight is right that the existing fixed 40 ms
  window smears a 40 ms interval — it reports the fundamental losing 6.042 dB
  where the true figure is 6.154 dB, and the test measures 6.156 dB at a 10 ms
  half-window.
  Measured before and after: the fitted exponent is **0.747** on the
  `h^-0.75` fixture and **0.0000187** on the control; the fundamental loses
  **22.966 dB** over 150 ms either way; partials 1, 2, 4 and 8 lose
  **6.153, 6.152, 6.154, 6.154 dB** over 40 ms before and
  **6.156, 10.262, 17.267, 29.044 dB** after, so partial 8's excess goes
  **0.0005 -> 22.888 dB** against a window of [15, 30] and a prediction of
  23.1; the centroid 150 ms after note-off goes **-0.167% -> 46.93%** below the
  held note's; and the control fixture's partial-8 excess is **0.002 dB**.
  Reverting the release block and rerunning fails five assertions: the
  partial-8 window at 0.0005 dB and all three monotonicity assertions at
  6.153/6.152/6.154/6.154 dB, and the centroid at -0.167%. The calibration and
  control assertions correctly still pass on the reverted engine, which is what
  they are for. Dropping only the slow-side clamp — leaving the rest of the
  change in place — fails all four retirement assertions and nothing else, at
  **42.5 dB** and **22.7 dB** for the isolated Air layer at root+24 and root+51
  and **59.4 dB** and **29.0 dB** for the isolated Bone layer, against a bound
  of 60 dB and 107 to 112 dB with the clamp. Hard-coding `p = 0.75` instead of
  fitting it fails the two control assertions and nothing else, at an exponent
  of 0.75 and a partial-8 excess of **23.13 dB** on a source with no
  frequency-dependent decay at all — which is what the step predicted that
  control was for.
  Cost: `setModel()` goes from **133.5 us to 421.4 us** per swap here, all of
  it the fit's 64 decoder evaluations. It is a one-off on a model swap, which
  already fades every sounding voice out, and the render path is unchanged for
  held notes — the engine benchmark's six scenarios are all held chords and
  move within run-to-run noise. Suite green, 3/3 ctest suites in 49.0 s; the
  NeuralEngine suite grows 27.0 s to 30.2 s, which is the three added learns.

- [ ] **5. Key-track the model clock by the same fitted damping exponent.**
  `NeuramarEngine.cpp:1379` advances the model clock at `evolutionRate /
  sampleRate_` with no transposition term, so with Orbit off a plucked or struck
  memory rings for very nearly the source's wall-clock duration at every key:
  `T20(81)/T20(57)` is 0.978 and `T20(33)/T20(57)` is 1.054. Step 4 fits `p` from
  the source's own partials in `tau(f) = tau_1 (f/f_1)^-p`; that is a statement
  about frequency rather than about harmonic number, so a note played at
  transposition ratio `r` has fundamental `f_1 r` and a consistent decay time
  `tau_1 r^-p`, which means the model clock should advance at
  `evolutionRate * r^p`, clamped to [0.25, 4] so a badly-fitted exponent cannot
  make a note vanish. At the root `r = 1` and the render is bit-identical.
  **Divide** step 4's release time constants by the same `r^p` — equivalently,
  multiply their decay rates by it — so the top of the keyboard also damps faster
  than the bottom. The direction matters and is easy to get backwards: `r^p` is a
  clock *rate*, so it multiplies the model clock, and a time constant is the
  reciprocal of a rate, so it is divided. `tau_rel(1) = releaseSeconds * r^-p` is
  the same `tau_1 r^-p` the derivation above starts from. This is the mechanism, not a taste
  control: the exponent comes from the sound the user dropped in, so a source with
  frequency-independent damping keeps `p = 0` and keeps a pitch-invariant decay.
  Three costs the first draft did not name, all of which have to be decided before
  this lands. The model clock is the *only* time variable, so key-tracking it also
  key-tracks the learned onset and any learned vibrato: a 5 Hz source vibrato
  becomes 14 Hz two octaves up at `p = 0.75`, which is not what a player does, and
  the step needs either a separate un-tracked clock for the pitch trajectory or an
  explicit decision to accept it. At the shipping defaults **this step changes no
  decay at all** — Orbit is on, the clock ping-pongs inside the loop, and the only
  audible effect is that the gap-3 pumping runs 2.83 times faster two octaves up —
  so it must land after step 3 and its value is scoped to Orbit-off playing. And
  the trajectory freezes at `t = duration`, so a high note reaches the frozen
  final frame `r^p` times sooner and then holds it indefinitely; the step should
  say what happens there. Closes gap 8; depends on steps 3 and 4. *Verified by*:
  `testDecayKeyTracking` learns the `tau_h = tau_1 h^-0.75` fixture with
  `tau_1 = 0.20 s`, asserts the fitted exponent is 0.75 +/- 0.10 (measured 0.734),
  then renders MIDI 33, 57 and 81 at **Orbit 0** with Air and Bone muted and
  measures each played fundamental's **T20** — the time for the fundamental to
  fall 20 dB below its 80 ms level. T20 is the estimator this test must use, and
  the reason is measured, not stylistic: a least-squares log-amplitude T60 fit
  over 0.08-1.5 s gives `T60(81)/T60(57)` = **1.268** on a scratch build that
  already key-tracks the clock correctly, because the trajectory freezes at
  `t = duration` — 0.565 s of real time at MIDI 81 — and the frozen tail flattens
  the fit. T20 stays inside the un-frozen span. It asserts `T20(81)/T20(57)` lies
  in [0.28, 0.45] against a predicted `4^-0.75 = 0.354` (a scratch build measures
  0.359; today 0.978) and `T20(33)/T20(57)` in [2.2, 3.6] against a predicted
  2.83 (scratch build 3.076; today 1.054). A second fixture with
  frequency-independent decay must keep both ratios within [0.90, 1.10], which
  fails on any implementation that key-tracks by a constant rather than by the
  fitted exponent.
  The release side needs its own assertion, because every T20 above is measured
  on a held note and none of them involves a note-off at all. The same test
  renders MIDI 45, 57 and 81 with a note-off at t = 0.30 s at Dissolve 0.65 s,
  Orbit 0, Air and Bone muted, and measures the played fundamental's
  **release-only** drop over 0.32 s to 0.36 s the way step 4 does — released
  minus held, 10 ms analysis half-window. The drop must scale as `r^p`: it
  asserts `drop(81)/drop(57)` lies in [2.4, 3.3] against a predicted
  `4^0.75 = 2.83`, and `drop(45)/drop(57)` in [0.50, 0.70] against a predicted
  `0.5^0.75 = 0.595`. Preflight measured the shipping engine at -6.177 dB at
  MIDI 45, -6.153 dB at 57 and -6.154 dB at 81, ratios 1.000 and 1.004, and a
  retirement
  0.6480 s after note-off at every key, so the assertion bites hard on a
  missing implementation. It also bites on a reversed one: multiplying the time
  constant by `r^p` instead of dividing puts the ratios at 0.354 and 1.68, on the
  wrong side of both windows. The `p = 0` control fixture must hold both release
  ratios within [0.95, 1.05].
  *Contract corrected in preflight.* This step said "apply the same `r^p` to the
  release time constant", which for `p > 0` and a note above the root
  *lengthens* the release — the opposite of the sentence it justifies and of the
  `tau_1 r^-p` the paragraph derives two lines earlier. The T20 assertions could
  not catch it, because they exercise the model trajectory of a held note and
  never release one. The mechanism and the missing release assertion are both
  fixed above.

- [ ] **6. Vary excitation strength between notes instead of skipping into the
  attack.** Delete the start-time offset at `NeuramarEngine.cpp:731-734`, whose
  clamp to `[0, duration]` makes it one-sided over the first
  `mutation * |offset| * 0.018 * duration` seconds — at most 2.6 ms at Mutation
  0.12 — so the mechanism buys its variation by deleting transient: first-10 ms
  energy relative to each take's own peak spreads 0.624 dB at Mutation 0.12 and
  1.231 dB at Mutation 0.50. Route the per-note variation through excitation
  strength instead: add `dv = 0.55 * mutation * mutationOffset` to
  `voice.velocity` at note-on, before `velocityGain` is computed at `:519` and
  therefore before the Touch terms at `:770-774` read it, clamped to keep velocity
  in [0.05, 1.0]. What differs between two nominally identical hand strikes is the
  energy delivered, and on a real instrument level and brightness co-vary because
  a harder strike shortens the contact time and pushes the excitation spectrum's
  corner up. Neuramar already has that coupling in the Touch path, so a velocity
  jitter reproduces the correlated level, tilt and Air variation for free and
  nothing new is drawn. The +/-0.25 cents of detune at `:703-705`, the +/-0.54%
  harmonic variation at `:884` and the Air re-seed all stay. Note that the size of
  the coefficient is load-bearing: with `velocityGain = v(0.72 + 0.28 sqrt(v))`,
  `dv = +/-0.066` at Mutation 0.12 spans 1.62 dB end to end, and twelve uniform
  draws cover about 85% of a range, so the *expected* observed spread is around
  1.4 dB — under the 1.5 dB floor an acoustic instrument shows. Either raise the
  coefficient to about 0.75 or state the gate as a standard deviation; do not
  ship a coefficient whose own arithmetic lands below its acceptance test. Closes
  gap 4. *Verified by*: `testRepeatedNotesVaryInStrength` fires twelve identical
  `noteOn(57, 0.8)` calls on the *percussive* fixture at Mutation 0.12, letting
  each voice retire between takes so that `ageStamp` — which is what selects
  `mutationOffset` — actually advances, and asserts that the standard deviation of
  the twelve peak levels lies in [0.45, 1.10] dB, which corresponds to a 1.5-3.5 dB
  expected range for a uniform draw and is the robust form of that gate; today the
  standard deviation is 0.26 dB and the range 0.823 dB. It asserts that every
  take's first-10 ms energy relative to its own peak is within 0.5 dB of the
  Mutation-0 render's, so no take loses transient, against 0.624 dB spread today.
  And it asserts that at Mutation 0 twelve takes are bit-identical, which they are
  today and which any replacement mechanism must preserve. A fourth assertion is
  what makes the step's physical claim testable rather than decorative: the same
  twelve takes are re-rendered at **Touch 0.35**, the shipping value, and the
  takes' peak levels and spectral centroids must rank-correlate at **0.9 or
  better** (Spearman), with the loudest take's centroid at least **1%** above the
  quietest take's. That is the "level and brightness co-vary" claim stated as a
  number. Routing the jitter through `voice.velocity` produces it by
  construction, because `velocityGain` at `:519` and `touchTilt` at `:770-774`
  are both monotone in the same velocity; a per-take output gain, which passes
  all three of the assertions above, produces exactly zero of it. The 1% floor is
  conservative: scaling gap 7's measured +26.2% centroid movement for a velocity
  span of 0.95 down to the +/-0.066 this jitter draws at Mutation 0.12 predicts
  about 3.6%, or about 5% at a coefficient of 0.75, and the exact figure is
  fixture-dependent and has to be re-measured on the *percussive* fixture the
  test builds. The `|mean - median|`
  assertion proposed in the first draft is dropped: it is 0.015 dB today at
  Mutation 0.12 and 0.119 dB at Mutation 0.50, so it passes before the change and
  proves nothing. The peak distribution is not measurably bimodal.
  *Contract corrected in preflight.* The three assertions first written here
  measure peak-level spread, transient retention and exact determinism at
  Mutation 0, and a per-note random *output gain* satisfies all three — a pure
  gain cancels out of "first-10 ms energy relative to its own peak", so it scores
  0.0 dB there. The step's whole argument is that the variation must travel
  through the excitation path so level and timbre move together, and nothing
  tested that. The rank-correlation assertion does.

### Considered and not planned

- **Band-limiting the register reference at the played pitch (was step 1).**
  The first draft of this plan proposed changing `referenceFundamental` at
  `NeuramarEngine.cpp:991` from `rootFrequencyHz * frame.pitchRatio` to
  `rootFrequencyHz * voice.transpositionRatio * frame.pitchRatio`, so that
  `referencePower` and `renderedPower` see the same anti-alias taper, and claimed
  that this would stop `registerGain` saturating and close gap 2's noise/tone
  swing. It is a genuine like-for-like tidy-up and it is also very nearly inert,
  which a scratch build settles three ways. The Air-share and Air/Core columns
  across MIDI 12-108 come out **identical to the printed precision** before and
  after on all four fixtures — `registerGain` multiplies Core, Air and Bone alike,
  so it cannot move the ratio between them, and gap 2's real mechanism is that
  the Core is *normalised* while Air and Bone are *scaled*. The clamp still
  saturates: `registerGain` reaches 4.000 at MIDI 96-108 on every fixture after
  the change. And the top-octave level fade it was supposed to remove does not
  move — the formant fixture spans 21.21 dB across MIDI 12-108 both before and
  after, the *breathy* fixture's level spread moves 4.10 to 3.52 dB and the
  *sustained* fixture's 0.05 to 0.14 dB. The reason it does so little is that
  every fixture's spectrum rolls off at least as fast as `1/h`, so the partials
  the taper removes at a high note carry a small fraction of the reference power;
  what actually drives `registerGain` up is the Body-Lock envelope running out of
  observed range, which is exactly what the block was written to correct. The
  change is worth making some day for the sake of the comment being true, but it
  buys at most a few tenths of a dB and its stated verification would score the
  same number before and after.
- **Giving each Bone mode its own fitted decay (was step 3).** The first draft
  proposed multiplying each Bone mode's trajectory amplitude by a differential
  envelope built from `boneDecaySeconds_`, on the premise that the twelve modes
  share one trajectory and that the engine ignores a fitted per-mode decay. Four
  measurements strike it. **The premise is false**: `SynthesisFrame::boneAmplitudes`
  is a twelve-element per-mode network output (`NeuralModel.h:42`, `:76-78`) and
  `NeuramarEngine.cpp:1078` reads `frame.boneAmplitudes[mode]`, so each mode
  already carries its own learned envelope — and it works, with the Bone layer in
  isolation rendering the three high-reliability modes of the *struck* fixture at
  1.3985, 2.7712 and 0.8493 s against the learner's own 1.4055, 2.585 and
  0.8517 s. The engine does not read `boneDecaySeconds_` because it is redundant
  with the trajectory. **The formula carries a unit error**:
  `SampleLearner.cpp`'s `estimateDecay` returns `-1/slope` of the natural-log
  amplitude, a time constant `tau`, not a T60, so the draft's
  `tau_m = T60_m / 6.9078` conversion would have decayed every Bone mode 6.9 times
  too fast. **It targets the wrong layer**: the Bone branch renders 11.5 dB below
  the full render on a struck body, so the 118-2912% modal T60 errors that
  motivate gap 5 belong to the harmonic Core laid on the detected root, and no
  Bone-side change can move them. **And the named verification cannot see the
  effect**: `makeStruckBodyNote` in `Tests/ResynthesisQualityTests.cpp` gives its
  ten modes decay rates `0.55 + 0.045 * ratio`, so their time constants span
  1.61 s down to 0.87 s — a factor of 1.85, not the factor of 31 the step assumed
  — and per-mode T60 assertions on that fixture would neither fail before the
  change nor pass more convincingly after it. What survives is small: replacing
  the binary reliability gate at `:1069` with the stored reliability as a linear
  weight is principled and cheap, and on the *struck* fixture the two modes below
  0.57 reliability are the ones whose rendered decay departs furthest from the
  learner's own estimate (159% and 167%) — but they sit inside a layer that is
  11.5 dB down, so it does not earn a step under this pass's audibility rule. Gap
  5 remains open and its real address is the Core's harmonic grid on an
  inharmonic source, which is a structural change of the kind the missing
  transient branch already is.
- **Key-tracking the stiff-string coefficient (gap 11).** The physics is clean —
  for a constant-diameter, constant-tension string family
  `B = pi^3 E d^4 / (64 T L^2)` and `f0 ∝ 1/L`, so `B ∝ f0^2` — and the ARLO
  threshold law gives an exact verification number. It is cut on cost, not on
  merit: `harmonicStretchRatio_` is one global 256-entry table shared by every
  voice (`:284-297`, `:1306`), and key-tracking B means a per-voice table
  rebuilt at note-on, 16 KB of new voice state and 256 square roots per note.
  The audit rates the audibility subtle where all six planned steps rate clear
  or obvious. Recorded here so a later pass can pick it up with the exponent and
  the threshold law already stated.
- **Period-synchronous Air modulation.** This is the mechanism behind the
  "fizziness" MYTH owners describe and behind the failure of sines-plus-noise to
  fuse into one source (US5508473A; Mehta and Quatieri, 2006). The render side is
  cheap — each voice already knows its instantaneous fundamental phase — but the
  analysis side needs a new measurement, the depth of period-synchronous
  modulation in the residual, and the benchmark needs a coherence metric before
  the result could be verified at all. Two new measurements plus a model format
  field is a pass of its own.
- **Non-stationary sinusoid bases in the learner.** Adding a damping and a linear
  chirp column to each partial's design matrix (Betser 2009; Marchand and Depalle,
  DAFx-08) would attack the residual-ERB regression this document already records
  as unexplained across steps 1, 2 and 4, and the vibrato halo. It is the highest-
  value analysis-side change available and it is an analysis pass, not a render
  pass; this one is scoped to the render.
- **Raising the trajectory grid past 128 frames.** The 5.9 Hz vibrato arithmetic
  above shows the grid runs out at about a 5.6 s source. The fix is a model format
  bump and a re-measurement of every fidelity number in the benchmark, which is
  too much to carry alongside six render changes. The correct interim action is
  documentation: `README.md` line 170's claim about retained slow vibrato needs
  the duration boundary stated.
- **MPE and pitch bend.** The largest spec-sheet omission in the instrument and
  genuinely cheap to wire, since the engine already carries per-voice
  `pitchRatio`, per-voice Air gain and a velocity-driven Touch tilt. It is
  excluded here because every step in this pass had to be verifiable by a
  deterministic DSP test, and MPE's value is in `Source/Plugin*.cpp` note
  routing. It should be the first item of the next pass, not of this one.
- **Decorrelating vibrato phase across voices (gap 12) and voice coupling.** The
  audit rates the lock subtle. The honest version of coupling — a shared,
  level-following excitation of the Bone modes across sounding voices — was
  deferred behind per-mode Bone decays; that dependency is gone, because the Bone
  modes turn out to carry their own learned decays already, but the coupling
  itself still needs a shared excitation bus that no version of the engine has,
  and the audit rates the defect it addresses subtle.
- **A keyboard loudness contour (gap 9).** Perfect flatness is wrong against
  physics, but the right contour is not derivable from one recording: the source
  carries no cross-register evidence, and any curve chosen here would be drawn,
  not derived. The saturation fade at the top of the keyboard — up to 21.2 dB
  across MIDI 12-108 on a formant-rich fixture, with `registerGain` pinned at its
  4.000 ceiling from MIDI 96 up — is the part that is a defect rather than a
  design choice, and no step in this pass removes it: the change that was
  supposed to, band-limiting the register reference, measures the same 21.2 dB
  after as before. It is the strongest remaining candidate for the next pass.
- **Decoupling Gravity from level (gap 13).** Real, and the audit rates it
  inaudible-but-structural: it makes A/B tone comparison require a compensating
  Output move, and at Gravity 1.0 / MIDI 24 / velocity 1.0 / Output 0 dB it pins
  0.350% of samples at the hard-clip guard. It is a control-scaling fix rather
  than a change to how the instrument sounds when used at its defaults, so it did
  not earn a step under this pass's rule.
