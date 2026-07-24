# Neuramar resynthesis-quality benchmark

## Status and claim boundary

This document specifies a future evaluation protocol against real competitors
and human listeners. That protocol has **not** been run. Every numerical
threshold in the *Proposed target gates* table below is a **proposed target**,
not evidence that the current Neuramar build satisfies it.

The one exception is *Measured analytic-fixture results* immediately below. That
section reports numbers actually produced by the repository's own regression
binaries on deterministic analytic fixtures. It is a self-comparison across
Neuramar versions on synthetic material, not a competitor comparison, not a
listening test, and not evidence about real acoustic instruments.

"Industry-leading" must not mean universally better than every instrument. A
defensible claim is narrower: in a preregistered, version-pinned comparison,
Neuramar achieved the highest reference-similarity result among the named
systems on the stated corpus, while meeting the pitch, aliasing, and real-time
gates below. The tested versions, presets, stimuli, raw anonymized ratings, and
analysis code must accompany any public claim.

A single source note contains no evidence about unrecorded registers,
velocities, or articulations. Root-note reconstruction and cross-register
instrument resemblance are therefore reported separately.

## Measured analytic-fixture results

Source: `Tests/ResynthesisQualityTests.cpp`, run through
`ctest --test-dir build-dsp`. Every figure below is printed by that binary and
is reproducible from a clean checkout. The fixtures are analytic, the targets
are generated independently at every rendered pitch, and the machine was an
x86-64 Linux container; absolute timings will differ elsewhere, ratios much
less so.

### Held-out source/filter family

A 220 Hz note with three fixed formants, an alternating odd/even excitation, and
a boosted fundamental is learned once and then rendered at ten register offsets
from -24 to +24 semitones. `shape MAE` is the mean absolute deviation, in dB,
between each rendered partial and the independently generated target after
removing one median level offset; `parity error` is the error in the mean
odd-minus-even partial level.

| Metric (mean over -24 … +24 st) | 1.0 | 1.1 |
| --- | --- | --- |
| Body-Locked shape MAE | 3.495 dB | **2.898 dB** |
| Pitch-following shape MAE (reference) | 6.453 dB | 6.453 dB |
| Excitation parity error | 0.609 dB | 0.898 dB |

Every one of the ten offsets improved. The largest gains are in the middle of
the matrix (-7 st: 3.38 → 2.64 dB; +7 st: 2.49 → 1.96 dB; +12 st: 2.06 → 1.69 dB)
and at the extremes (-24 st: 6.54 → 5.80 dB; +24 st: 4.46 → 3.86 dB). The cause
is the narrower parity-balanced source/filter kernel described in
[`neural-synthesis-research.md`](neural-synthesis-research.md): the previous
five-tap kernel blurred a narrow formant across roughly four source harmonics.
Parity is the deliberate trade: a narrower envelope tracks the excitation a
little more closely, so slightly less of the odd/even contrast survives into the
excitation residual. The worst single offset is +24 st at 2.23 dB, well inside
the 3.5 dB regression guard.

### Stiff-string partial placement

A struck-string fixture with a known coefficient `B = 4.0e-4` is learned once and
rendered at -12, 0, and +12 semitones. Partial placement error is the mean
absolute deviation, in cents, between rendered partials 6, 10, and 14 and the
independently generated stiff-string targets, located by an analytic sweep with
parabolic refinement.

| Metric | 1.0 (no stiff-string model) | 1.1 |
| --- | --- | --- |
| Fitted coefficient | not modelled | 4.0145e-4 (true 4.0e-4) |
| Detected root | — | 219.94 Hz (true 220 Hz) |
| Partial placement, mean over -12/0/+12 st | 37.44 cents | **0.095 cents** |

The 1.0 column is measured by rendering the same 1.1 model with `Stretch` at 0%,
which reproduces the ideal harmonic bank every earlier release used. The
engine-side test in `Tests/NeuramarEngineTests.cpp` repeats the experiment with
a different coefficient (`B = 3.0e-4`) and a different fixture and reports
0.20 cents fitted against 27.81 cents ideal-harmonic.

This measures placement of the partial series only. It says nothing about
amplitude, decay, or perceived similarity to a real piano.

### Cost

Measured with the same binaries, best of three runs, on the same container.
The 1.0 column was produced by reverting only the changed hot paths in a
scratch copy of the tree, so the workload is identical.

| Case | 1.0 | 1.1 |
| --- | --- | --- |
| Eight voices, two octaves above the root, 1 s at 48 kHz | 0.156 s (6.40x realtime) | **0.067 s (15.0x realtime)** |
| Eight voices, two octaves below the root, 0.5 s at 48 kHz | 0.354 s (1.41x realtime) | **0.211 s (2.37x realtime)** |
| `learn()` on a 1.6 s / 44.1 kHz source | 0.715 s | **0.557 s** |
| Tabulated vs direct windowed-sinc resampling of 5 s at 48 → 12 kHz | 0.060 s | **0.0033 s** |

The resampler row is an in-test A/B: the same regression binary runs a literal
transcription of the pre-1.1 direct-evaluation kernel and the shipped polyphase
table over identical input and reports both the speedup and the difference
between their outputs, which is below -150 dB. The 1.1 `learn()` figure includes
the new stiff-string estimation pass, which 1.0 did not perform at all.

None of these are the acceptance gates below. In particular, the render-speed
gates are specified for Apple silicon and Rosetta and have not been measured.

## Locked comparison systems

No benchmark may use an unqualified "latest" release. Before capture, record
the semantic version, host version, architecture, binary SHA-256, preset/state
SHA-256, sample rate, and block size in the benchmark lock file.

- **Direct — Sonic Charge Synplant 2.0.2, Genopatch.** Enable Correct
  Tuning, run all four strands to completion, render the four terminal
  solutions, and select by the preregistered root-only metric. Do not edit the
  winner. The official guide describes the converging strands and 48-gene
  synthesis limit; the developer states that no source audio is used in the
  result. [Product](https://soniccharge.com/synplant),
  [guide](https://cdn.soniccharge.com/public/Synplant%20User%20Guide.pdf?inline=),
  [developer explanation](https://beta.soniccharge.com/forum/topic/2512-does-genopatch-use-the-original-sample-at-all).
- **Direct — Dawesome MYTH 1.5, new Re-synthesis algorithm.** Use the
  exact installed binary recorded in the lock. Begin with a neutral
  state and disable effects, randomization, stereo processing, and creative
  Transformers. The manual says version 1.5 introduced the new algorithm
  optimized to preserve source characteristics; the product page identifies
  Re-synthesis V2 as machine-learning powered.
  [Manual](https://assets.tracktion.com/pdf/2024/myth-user-manual-2.pdf),
  [product](https://www.tracktion.com/products/myth).
- **Expert-assisted direct — Madrona Labs Sumu 1.3.0 with Vutu 0.9.9 on
  macOS or 0.9.10 on Windows.** Vutu analysis choices are manual, so report
  Sumu as an expert-assisted ceiling rather than a one-click workflow peer.
  Sumu represents sounds with at most 64 bandwidth-enhanced partials whose
  frequency, amplitude, and noisiness evolve over time.
  [Product and downloads](https://www.madronalabs.com/products/sumu).
- **Sample-linked ceiling — Image-Line Harmor, exact plug-in and FL Studio
  build recorded at capture.** Use audio-resynthesis mode with all effects
  off. Harmor supports 516 additive partials, but its highest-fidelity mode
  continues to reference the original sample. It is not a source-independent
  peer. [Official manual](https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/plugins/Harmor.htm).
- **Analyzed-resynthesis ceiling — Apple Alchemy, exact Logic Pro and
  Alchemy build recorded at capture.** Use additive or spectral resynthesis
  only; disable sampler and granular playback. Alchemy's spectral engine stores
  amplitude and phase in frequency bins and synthesizes sine or filtered-noise
  components.
  [Overview](https://support.apple.com/guide/logicpro/alchemy-overview-lgsi2618652a/mac),
  [spectral synthesis](https://support.apple.com/en-euro/guide/logicpro/lgsibf71c5f6/mac).

Harmor and Alchemy may not enter a run until their numeric product/host builds
and binary hashes have been populated in that run's lock file.

Novum, FORM, Grainferno, and ordinary samplers may appear in a separate creative
sample-instrument study. They are excluded from the source-independent headline
because their official workflows retain or granulate source audio.

## Twenty-four-item gold corpus

All competitors receive the identical mono WAV segment, no longer than two
seconds, with DC removed, peak below -1 dBFS, and no added effects. The manifest
stores the original-file hash, crop boundaries, conditioning gain, known root,
and rights metadata. A larger development corpus may run nightly, but only the
frozen gold corpus supports comparison claims.

### Analytic ground truth: 8 items

Generate these deterministically at 96 kHz/32-bit float, then create 48 kHz
inputs with a documented band-limited resampler. Generate a mathematically
correct target independently at every requested pitch.

1. Exponential harmonic roll-off.
2. Alternating odd/even spectrum with a missing fundamental.
3. Two narrow, fixed-frequency formants.
4. Time-moving formant and spectral tilt.
5. Harmonics plus time-varying shaped noise.
6. Delayed partial onsets and a short broadband transient.
7. Inharmonic damped bell modes with known decay constants.
8. Stable vibrato followed by a bounded pitch bend.

Include both pitch-following and fixed-formant target variants. This makes the
intended answer explicit instead of rewarding one register-mapping assumption.

### Public acoustic notes: 8 items

Use dry ordinary notes from [TinySOL](https://zenodo.org/records/3659365):
contrabass, violoncello, violin, flute, B-flat clarinet, oboe, trumpet, and alto
saxophone. TinySOL contains 2,478 isolated notes from 14 instruments at
44.1 kHz/16-bit under CC BY 4.0. Exclude every filename marked `R`, because the
dataset documentation identifies those notes as digitally resampled.

Choose an interior root for each instrument and match instrument, technique,
dynamics, and string across held-out target notes. A genuine target note tests
same-instrument resemblance, not exact reconstruction of an unknowable
performance.

### Private hard cases: 8 items

1. Dry sustained vocal `/a/`.
2. Dry sustained vocal `/i/`.
3. Plucked steel string.
4. Electric-piano tine.
5. Subtractive saw with a moving resonant filter.
6. FM or phase-modulation metallic tone.
7. Bell or struck-metal impact.
8. Breath, scrape, or tonal foley with a noisy onset.

Capture three genuine takes per available pitch with the same performer,
instrument, microphone, distance, technique, and dynamic. Distances between
same-pitch genuine takes establish the irreducible performance-variation floor.
Keep at least 20% of the private material sealed until the design is frozen.

[Good-sounds](https://zenodo.org/records/820937) is an optional internal
extension because it provides professional players, 12 instruments, full
semitone coverage, 48 kHz/32-bit audio, and manual envelope annotations. Its
record page gives conflicting BY-NC and BY license indications, so treat it as
non-commercial and do not redistribute it until the authors clarify the terms.

## Render matrix and fairness controls

Render these semitone offsets wherever the destination pitch remains physically
valid:

```text
-24, -19, -12, -7, -3, 0, +3, +7, +12, +19, +24
```

For analytic sources, also render the interpolation stress matrix:

```text
-18.5, -11.5, -6.5, -0.5, +0.5, +6.5, +11.5, +18.5
```

Report root (`0`), near (`1-7` semitones), octave (`12`), and far (`19-24`)
strata separately. Never substitute a pitch-shifted recording for a genuine
held-out acoustic target.

Use two explicitly separate protocols:

- **Engine quality:** provide the verified root to every system.
- **One-click quality:** retain each product's automatic root result and report
  root error and import time independently of renderer quality.

Freeze one Neuramar `Match` state before inspecting results: full learned
Imprint, Core, Air, and Bone; Mutation, Orbit, spread, and effects off; neutral
tone controls; zero added attack; ordinary evolution rate; linear output; and a
preregistered Body Lock value. Other settings are ablations, not candidates
from which to choose after hearing the test set.

Apply one active-region RMS gain scalar per output for timbre metrics and blind
listening. Record that scalar as a separate level-error result. Align only the
onset; do not use local time warping. Use at least five renders when a system's
stochastic layer is not repeatable.

## Objective measurements

Analyze at 48 kHz float unless the test explicitly varies host sample rate. The
statistical unit is a source item, not an STFT frame, so long notes cannot
dominate the result.

### Multi-resolution spectrum

Use Hann windows with `(window, hop)` pairs:

```text
(256, 64), (1024, 256), (4096, 1024), (16384, 4096)
```

For reference magnitude `X` and render magnitude `Y`, report each resolution
and their arithmetic mean:

```text
spectral convergence = ||X - Y||F / max(||X||F, epsilon)
log-magnitude MAE    = mean(|20 log10(X + epsilon)
                            - 20 log10(Y + epsilon)|)
```

Set `epsilon` to -100 dB relative to the reference peak. Evaluate the log term
on the union of bins where either signal exceeds -80 dB relative to that peak,
so added energy in reference-silent bins is penalized. Publish individual
resolutions: multi-scale spectral results depend materially on their exact
configuration. [Schwär and Müller](https://www.audiolabs-erlangen.de/content/05_fau/professor/00_mueller/03_publications/2023_SchwaerM_MultiScaleSpecLoss_IEEE-SPL.pdf).

### Pitch and harmonic identity

- Run pYIN and the official [CREPE](https://github.com/marl/crepe) model at
  10 ms hops. Accept an estimated frame automatically only when they agree
  within 25 cents; retain disagreement rate as a diagnostic.
- Report median and 95th-percentile absolute cents error, gross pitch error over
  50 cents, octave-error rate, voiced recall, and pitch-contour error after
  subtracting mean tuning.
- At the accepted F0, fit sine and cosine coefficients jointly at every valid
  expected partial. Report harmonic-ratio dB MAE, harmonic-energy recall and
  precision, odd/even balance, and spectral-centroid error.
- Report onset-phase error only as a diagnostic; independent sustained phase
  is not a headline timbre measure.

### Envelope, transient, residual, and modes

- Extract an F0-adaptive [CheapTrick](https://doi.org/10.1016/j.specom.2014.09.003)
  envelope using the known or consensus F0. Report ERB-spaced envelope dB RMSE,
  spectral-tilt error, and prominent formant center, bandwidth, and peak-gain
  error.
- Report onset error, T10-T90 attack-time error, cumulative-energy error at
  1, 5, 10, 20, and 50 ms, first-100-ms multi-resolution spectral error, crest
  factor, transient-to-sustain ratio, and release T60 when the target has a
  controlled release.
- Jointly subtract fitted harmonic and accepted modal components from both
  signals. For the residual, report ERB-band power dB MAE over time, spectral
  flatness, residual autocorrelation, stochastic-energy fraction, and the
  0-50 Hz modulation spectrum of every band envelope. Do not use waveform MSE
  for independently generated noise.
- For analytic modal sources, report peak-frequency error in cents, active-mode
  precision and recall, amplitude error, and T60 error.

### Aliasing and high-band preservation

Use steady analytic sine and harmonic fixtures with stochastic layers, effects,
unison, modulation, and nonlinear output processing disabled. Render at 44.1,
48, and 96 kHz through the upper MIDI range. Discard the onset and jointly fit
sine/cosine coefficients at every known below-Nyquist target partial. Define
`Evalid` as the energy of that fitted signal and `Espurious` as the residual
energy after subtraction. Report:

```text
alias-to-signal ratio = 10 log10(Espurious / Evalid)
maximum spur          = largest unexpected spectral line in dBc
```

Measure the maximum spur with an eight-times-zero-padded Hann spectrum,
excluding the two-bin Hann main lobe on either side of every valid partial.

Use an analytic reference or a 192 kHz render followed by documented
band-limiting and downsampling. Separately report 8-12, 12-16, and 16-20 kHz
energy error. This prevents a dark renderer from winning by deleting valid high
frequencies.

### Runtime and latency

Measure one and eight voices at root and at the worst lower-register oscillator
count, at 48/96 kHz and 64/128/512-sample blocks. After warm-up, record median,
99th percentile, and maximum block time over ten minutes, deadline misses,
offline real-time factor, allocations, locks, peak resident memory, import
time, cancellation response, declared latency, and measured note-on latency.

## Proposed target gates

These values are **future acceptance targets, not current results**.

| Measurement | Proposed target |
| --- | --- |
| Clean pitched root detection | At least 98% correct MIDI semitone; at most 0.5% octave errors. |
| Played pitch, near | Median at most 5 cents; p95 at most 15 cents; at most 1% gross errors. |
| Played pitch, far | Median at most 10 cents; p95 at most 30 cents; at most 2% gross errors. |
| Analytic harmonic-ratio MAE | At most 0.5 dB root, 1.5 dB near/octave, and 3 dB far. |
| Spectral-envelope RMSE | At most 1.5 dB root, 2.5 dB near, and 4 dB far. |
| Formant-center error | Median at most 2% near and 5% far. |
| Early cumulative-energy MAE | At most 1.5 dB. |
| Onset and attack | Onset within 2 ms; T10-T90 within 5 ms or 10%, whichever is larger. |
| Residual ERB-power MAE | At most 2 dB root and 3 dB far. |
| Modal frequency and decay | Frequency within 20 cents; T60 within 15%. |
| Aliasing | Aggregate alias-to-signal ratio below -80 dB; maximum spur below -70 dBc. |
| Valid high-band retention | No more than 3 dB unintended loss in an evidence-backed band. |
| Apple-silicon render speed | At least 4x real time at 48 kHz and 3x at 96 kHz for the worst eight-voice case. |
| Rosetta x86_64 render speed | At least 3x real time at 48 kHz for the same case. |
| Audio callback | p99 below 50% and maximum below 80% of deadline; zero misses in ten minutes; zero allocations, locks, or I/O. |
| Import and cancellation | Five-second import p95 below 10 seconds; cancellation acknowledged within 100 ms. |
| Plug-in latency | Zero declared algorithmic latency and no unexplained measured delay beyond event/sample scheduling. |

Do not collapse these measurements into one opaque quality number. Publish the
full distribution by item, source class, and register, with paired bootstrap
95% confidence intervals.

## Blind listening protocol

Use [ITU-R BS.1534-3](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1534-3-201510-I%21%21PDF-E.pdf)
as written: double-blind multi-stimulus presentation, open and hidden reference,
hidden 3.5 kHz low anchor, hidden 7 kHz mid anchor, listener training, and no
more than 12 hidden signals in one trial.

- Recruit 30 experienced critical listeners and retain at least 24 after the
  preregistered screening.
- Run 24 trials in two 12-trial sessions: 8 root, 8 near/octave, and 8 far.
- Keep stimuli between 2 and 8 seconds and loudness-match them within 0.1 dB
  using the single recorded gain scalar.
- Present the open reference, hidden reference, both standard anchors,
  Neuramar, Synplant, MYTH, Sumu, and one sample-linked/analyzed ceiling.
- At root, use the imported sample as reference. At another pitch, use a genuine
  matched note. When available, include another genuine same-pitch take as the
  natural-performance-variation condition.
- Ask one question: overall similarity to the reference in pitch, timbre,
  attack, evolution, and noisy/resonant detail. Test beauty and playability in
  a separate blinded pairwise-preference study; a reference-free preference
  test is not MUSHRA.
- Repeat at least 10% of trials to measure listener consistency.
- Apply the standard post-screening: exclude a listener who scores hidden
  references below 90 on more than 15% of items, or the mid anchor above 90 on
  more than 15%, including the standard's item exception.

The proposed superiority rule is also a **target**, not a current claim:

1. Against the strongest direct competitor, Neuramar's paired MUSHRA advantage
   is at least 5 points and its two-sided 95% confidence interval excludes zero,
   both overall and in the root, near/octave, and far strata.
2. In every major source class, the lower confidence bound is above the
   -5-point non-inferiority margin.
3. Neuramar also passes every pitch, aliasing, callback, and latency gate.
4. Planned competitor comparisons are multiplicity-corrected, and the raw
   anonymized scores and exclusions are published.

If systems cluster near the reference and MUSHRA cannot resolve them, use
[ITU-R BS.1116-3](https://www.itu.int/rec/R-REC-BS.1116-3-201502-I/en), which is
intended for small impairments, rather than changing the MUSHRA anchors after
seeing results.

## Reproducibility and legal constraints

The reproducibility package must contain:

- an immutable corpus manifest with file IDs, rights, SHA-256 hashes, crop
  boundaries, conditioning, roots, targets, and train/challenge membership;
- the benchmark lock with operating system, CPU, architecture, host, plug-in
  binaries and hashes, sample rates, blocks, states, render seeds, and quality
  settings;
- deterministic analytic-source generation and anchor-generation code;
- raw WAV renders where redistribution is permitted, otherwise IDs and hashes;
- raw metric tables, analysis configuration, anonymized listening scores,
  exclusion reasons, and statistical scripts;
- import, render, and selection logs, including Synplant's best-of-four rule
  and Sumu's expert interventions.

Commercial plug-in binaries and presets must never be redistributed. Review
each product EULA before publishing its rendered output. Keep restricted corpus
audio and private challenge material out of the repository. Attribution must
travel with TinySOL-derived test material. Any public result must name only the
exact tested products and versions and must preserve losing cases as well as
aggregate results.

Locally, Neuramar import, rendering, timing, and analytic metrics can be fully
automated through a headless C++ benchmark target plus a pinned Python analysis
environment. Competitor sample import and patch generation remain a manual,
logged stage because their GUI-specific analysis controls are not exposed as a
portable headless API. Once states are frozen, a JUCE host can render the common
MIDI matrix automatically.
