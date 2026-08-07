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

- [ ] **4. Raise the body representation: 16 Air bands and 12 Bone modes.**
  Air moves from 8 to 16 log-spaced bands (1.03 to 0.51 octaves each) and Bone
  from 6 to 12 persistence-scored candidates, in one model format bump to
  version 5. Versions 2–4 keep an exact legacy read path: their 8 bands and 6
  modes load into the low slots with the remaining slots inactive, so a saved
  session renders as it did. *Closes gaps 3 and 4.* *Verified by:* residual
  ERB-power MAE falls on step 1's noisy fixture; a new modal fixture with ten
  known inharmonic modes reports active-mode recall and frequency error; the
  legacy-compatibility serialization tests are extended to cover the migration.

- [ ] **5. Raise polyphony to sixteen voices.**
  `NeuramarEngine::maximumVoices` from 8 to 16, with the voice-steal, pan
  refresh, and fade-tail paths re-measured under the higher count. *Closes gap
  6.* *Verified by:* the existing voice-ceiling, steal-tail-bound, and
  hand-off-continuity tests run at the new ceiling; the benchmark's cost table
  gains a sixteen-voice row.

- [ ] **6. Measure and fix automatic root detection across a corpus.**
  Add a deterministic root-detection corpus to the test suite covering the
  analytic ground-truth classes the benchmark names — exponential roll-off,
  odd/even with a missing fundamental, formant-dominant, stiff-string,
  harmonics-plus-noise, vibrato, low and high registers — and report
  correct-semitone rate and octave-error rate. Fix what it exposes. *Closes gap
  7.* *Verified by:* the rate is printed and guarded; any detector change is
  justified by a case that fails without it.

- [ ] **7. Reconcile the documentation with the measurements.**
  `README.md`, `neural-synthesis-research.md`, and
  `resynthesis-quality-benchmark.md` updated so every behavioural claim matches
  a measured number or is explicitly marked as a target. Gap 8 (velocity) is
  written down as a stated limit rather than left implied.

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
