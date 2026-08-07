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

Source: `Tests/ResynthesisQualityTests.cpp` and `Tests/NeuramarEngineTests.cpp`,
run through `ctest --test-dir build-dsp`. Every figure below is printed by a test
binary and
is reproducible from a clean checkout. The fixtures are analytic, the targets
are generated independently at every rendered pitch, and the machine was an
x86-64 Linux container; absolute timings will differ elsewhere, ratios much
less so.

### Root-note reconstruction

Added in 1.3. This is the measurement the document previously specified and
never reported: how close the fitted model's render is to the sound that was
dropped in, at the pitch it was analysed at. Two fixtures are learned, then
rendered at the source pitch under the frozen `Match` state described below —
full Imprint, Core, Air, and Bone; Mutation, Orbit, Noise, and spread off;
neutral tone controls; zero added attack; Body Lock at the preregistered 0.65.
Onsets are aligned at sample zero, the reference is conditioned the way
`learn()` conditions it, and one RMS scalar is applied to the render. Metric
definitions are the ones in *Objective measurements* below.

The `16384/4096` resolution is omitted: its window is 341 ms, which is longer
than the useful part of either fixture. The three shorter pairs are reported
individually as required.

The four columns per fixture are the sequential solve this measurement first
found (a), the joint solve (b), the joint solve with a halved onset aperture
(c), and the widened body representation (d). Each change is described in its
own section below.

| Metric, mean over the three resolutions | s/f (a) | s/f (b) | s/f (c) | s/f (d) | n+t (a) | n+t (b) | n+t (c) | n+t (d) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Spectral convergence | 0.0412 | 0.0392 | 0.0365 | **0.0364** | 0.0571 | 0.0594 | 0.0757 | 0.0754 |
| Log-magnitude MAE | 10.75 dB | 10.76 dB | 10.76 dB | **10.29 dB** | 5.31 dB | 5.33 dB | 5.40 dB | **5.22 dB** |
| Residual ERB-band power MAE | 5.00 dB | 4.73 dB | 4.69 dB | 5.00 dB | 3.54 dB | 3.54 dB | 3.55 dB | **2.35 dB** |
| Cumulative-energy MAE, 1/5/10/20/50 ms | 3.16 dB | 3.45 dB | **1.32 dB** | 1.40 dB | 0.87 dB | 1.09 dB | **0.34 dB** | 0.46 dB |
| Cumulative energy at 1 ms | -12.46 dB | -13.31 dB | **-5.66 dB** | -5.84 dB | -2.96 dB | -3.27 dB | **+1.34 dB** | +1.37 dB |
| T10-T90 attack-time error | +1 ms | +1 ms | 0 ms | 0 ms | 0 ms | 0 ms | -5 ms | -5 ms |

Per-resolution figures for tree (c), the last state before the body widening:

| Resolution `(window, hop)` | s/f convergence | s/f log MAE | n+t convergence | n+t log MAE |
| --- | --- | --- | --- | --- |
| (256, 64) | 0.0578 | 12.06 dB | 0.1558 | 4.96 dB |
| (1024, 256) | 0.0236 | 8.26 dB | 0.0311 | 4.86 dB |
| (4096, 1024) | 0.0280 | 11.95 dB | 0.0401 | 6.38 dB |

Three of these numbers are worse than the instrument's documentation implies and
are recorded here without softening.

The **log-magnitude MAE of 10.75 dB on the source/filter fixture** is the
largest. That fixture alternates odd and even partial levels by about 25 dB, and
the log term is evaluated on every bin either signal puts above -80 dB relative
to the reference peak, so it is dominated by how well the quiet even partials
and the between-partial floor are reproduced rather than by the loud ones. The
noise+transient fixture, whose spectrum has no such contrast, scores 5.31 dB on
the identical metric.

The **residual ERB MAE of 5.00 dB on the source/filter fixture** is worse than
on the fixture that actually contains noise. That is not a paradox: the
source/filter fixture is noise-free, so its residual is only analysis leakage
between the partials, and the render puts a fitted Air layer there. Neuramar
invents Air on a source that has none. The floor at 60 dB below the loudest
observed residual cell keeps this from being an artefact of comparing two
silences.

The **cumulative energy at 1 ms was 12.46 dB too high on the source/filter
fixture** when this was first measured — the render was louder than the source
over the first millisecond, converging to -2.75 dB at 5 ms, -0.59 dB at 10 ms
and under 0.01 dB by 20 ms. The analysis aperture that measures the attack was
21 ms wide at 220 Hz, so the first frame's amplitudes described an average of
the first 21 ms and the renderer applied them from sample zero. Halving that
aperture over the first 40 ms brings it to -5.66 dB; see *Onset aperture* below.

Regression guards on these rows: 0.15 convergence, 13 dB log-magnitude MAE,
7 dB residual ERB MAE, 2 dB early-energy MAE, and 8 ms attack error. The
early-energy guard is the one that is tight enough to fail: a four-period
aperture measures 3.45 dB on the source/filter fixture. The rest are bounds on
numbers that had never been measured, not acceptance gates. The gates in
*Proposed target gates* remain targets.

### Onset aperture

Added in 1.3, after the joint solve made it possible. The analysis grid reserves
48 of its 128 frames for the first 120 ms, 2.5 ms apart, and then measured every
one of them through a four-period aperture — 21 ms at 220 Hz — so the frame
spacing described a resolution the measurement did not have. The four-period
rule exists to keep adjacent partials' Hann main lobes apart, and that is a
constraint on a sequential projection, not on a joint solve. Frames inside the
first 40 ms therefore ask for two periods instead of four; the power-of-two
aperture bank means this is 512 samples rather than 1024 at a mid pitch, and
unchanged at pitches where four periods already fitted in 512.

| Measurement | four periods | two periods over the first 40 ms |
| --- | --- | --- |
| source/filter cumulative energy at 1 ms | -13.31 dB | **-5.66 dB** |
| source/filter cumulative-energy MAE | 3.45 dB | **1.32 dB** |
| source/filter spectral convergence | 0.0392 | **0.0365** |
| source/filter T10-T90 error | +1 ms | **0 ms** |
| noise+transient cumulative energy at 1 ms | -3.27 dB | **+1.34 dB** |
| noise+transient cumulative-energy MAE | 1.09 dB | **0.34 dB** |
| noise+transient spectral convergence | 0.0594 | 0.0757 |
| noise+transient T10-T90 error | 0 ms | -5 ms |
| `learn()` on a 1.6 s / 44.1 kHz source | 0.765 s | 0.760 s |

The trade is visible and is recorded rather than argued away. The noise+transient
fixture's spectral convergence gets worse, and all of that degradation is at the
`(256, 64)` resolution: 0.1063 to 0.1558, while `(1024, 256)` and `(4096, 1024)`
move by less than 0.001. That resolution's frames are 5.3 ms long, so it is
measuring the transient itself — a 3 ms broadband noise burst whose waveform the
renderer draws an independent realisation of and can never match. The same
fixture's T10-T90 error moves from 0 to -5 ms: the shorter aperture attributes
more of the burst to the harmonic branch, whose partials are phase-aligned at
note-on, so the rendered attack is sharper than the source's rather than softer.
Applying the shorter aperture over the whole 120 ms dense region instead of the
first 40 ms did not improve the attack any further and made the same fixture's
residual ERB MAE worse (3.55 to 3.62 dB), so it is confined to the attack.

### Held-out source/filter family

A 220 Hz note with three fixed formants, an alternating odd/even excitation, and
a boosted fundamental is learned once and then rendered at ten register offsets
from -24 to +24 semitones. `shape MAE` is the mean absolute deviation, in dB,
between each rendered partial and the independently generated target after
removing one median level offset; `parity error` is the error in the mean
odd-minus-even partial level.

| Metric (mean over -24 … +24 st) | 1.0 | 1.1 | 1.2 | 1.3 |
| --- | --- | --- | --- | --- |
| Body-Locked shape MAE | 3.495 dB | 2.898 dB | 2.790 dB | **2.714 dB** |
| Pitch-following shape MAE (reference) | 6.453 dB | 6.453 dB | 6.453 dB | 6.449 dB |
| Excitation parity error | 0.609 dB | 0.898 dB | 1.043 dB | **0.923 dB** |

The 1.3 column is the joint harmonic solve. Its parity result is the first
reversal of a three-release drift: 1.1 and 1.2 both traded excitation parity for
envelope accuracy, and the joint solve recovers 0.120 dB of it while improving
shape by a further 0.076 dB. See *Joint harmonic estimation* below.

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

The 1.2 column is the current tree, re-run on the container described above. Its
renderer work is deliberately outside the fit path and the numbers confirm that:
measured immediately before and immediately after the 1.2 render changes, the
aggregate moved from 2.78976 dB to 2.78975 dB. The 1.1 column is reproduced as
it was published; the small difference from 1.2 predates this release and was
not re-derived here.

### Body capacity: 16 Air bands and 12 Bone modes

Added in 1.3, as model format version 5. The Air filterbank was eight bands
log-spaced from 80 Hz to 16 kHz, which is 1.03 octaves each: a breath formant, a
scrape peak, and a hiss shelf inside one band are one number. It is now sixteen
bands at 0.51 octaves. The modal branch was six persistence-scored candidates,
which cannot describe a struck body; it is now twelve.

| Measurement | 8 bands / 6 modes | 16 bands / 12 modes |
| --- | --- | --- |
| noise+transient residual ERB-band power MAE | 3.55 dB | **2.35 dB** |
| noise+transient log-magnitude MAE | 5.40 dB | **5.22 dB** |
| source/filter log-magnitude MAE | 10.76 dB | **10.29 dB** |
| source/filter residual ERB-band power MAE | 4.69 dB | 5.00 dB |
| Struck-body active-mode recall | 0.60 (capacity bound) | **0.90** |
| Struck-body active-mode precision | — | 0.90 |
| Struck-body modal frequency error | — | 8.09 cents |
| `learn()` on a 1.6 s / 44.1 kHz source | 0.760 s | 0.768 s |

The 1.19 dB fall in residual ERB MAE on the fixture that actually contains noise
is what this change is for. The source/filter fixture moves 0.31 dB the other
way, and that is the same finding as before in a new form: that fixture has no
noise at all, so its residual is analysis leakage, the render fits an Air layer
to it either way, and a finer filterbank follows the leakage more closely. More
resolution cannot help a layer that should not be there.

The modal rows come from a new fixture with ten known inharmonic modes at ratios
between 1.43 and 13.47, learned once and compared against the model's selected
candidates. Six slots cap recall at 0.6 arithmetically, so the 0.70 guard on
that row is what holds the modal branch open.

Cost, measured on this container immediately before and after the change so the
two runs see the same machine load. These are not comparable with the 1.2 column
of the *Cost* table below, which was taken on an unloaded container.

| Benchmark scenario, 8 s at 48 kHz / 256-sample blocks | 8/6 | 16/12 |
| --- | --- | --- |
| Low chord, C1 root, 8 voices | 1.998 s (4.0x) | 2.213 s (3.6x) |
| Mid chord, C3 root, 8 voices | 1.063 s (7.5x) | 1.219 s (6.6x) |
| High chord, C6 root, 8 voices | 0.706 s (11.3x) | 0.835 s (9.6x) |
| High chord with Air and Bone at zero | 0.277 s (28.9x) | 0.287 s (27.9x) |
| Note-on at C1 | 20.0 us | 19.5 us |

The body layers cost 11% to 18% more per voice; the case with both layers muted
and the two note-on rows do not move, which is what confirms the cost is in the
sixteen biquads and twelve oscillators and not somewhere unintended. Model size
grows from about 35 KiB to about 41 KiB and remains inside the 128 KiB decoder
bound.

### Voice ceiling

Raised from 8 to 16 in 1.3. The per-voice render path is unchanged, so this is
additive: the eight-voice benchmark rows do not move, and the sixteen-voice row
costs what sixteen voices cost.

| Benchmark scenario, 8 s at 48 kHz / 256-sample blocks | 8-voice ceiling | 16-voice ceiling |
| --- | --- | --- |
| Low chord, C1 root, 8 voices | 2.213 s (3.6x) | 2.129 s (3.8x) |
| Mid chord, C3 root, 8 voices | 1.219 s (6.6x) | 1.232 s (6.5x) |
| High chord, C6 root, 8 voices | 0.835 s (9.6x) | 0.823 s (9.7x) |
| Mid cluster, C3 root, 16 voices | not renderable | 2.065 s (3.9x) |
| Note-on at C1 | 19.5 us | 19.7 us |

The eight-voice differences are container-load noise in both directions. The
sixteen-voice row is 1.7x the eight-voice cost at the same root rather than 2x,
because the model's control-rate evaluation and its band design are shared work
that a denser cluster amortises. A twelve-note held cluster — a four-note chord
added under a sustain pedal while its predecessor still rings — now sounds
twelve voices instead of stealing four, and that is asserted directly in
`Tests/NeuramarEngineTests.cpp`.

Versions 2, 3, and 4 keep an exact read path. Their eight bands and six modes
load into the low slots with their stored centre frequencies and ratios intact;
the added slots are given a valid layout and decoded as silence — an amplitude
output of `exp(-16)` with no network contribution, and a Bone reliability of
zero, which is what the renderer already tests to skip a mode. The decoder rows
are gathered through one index map, so a stored memory evaluates to the same
frame it always did. That is asserted directly: a crafted version-4 payload is
loaded, re-serialised in the current format, reloaded, and compared frame by
frame against the migrated model.

### Joint harmonic estimation

Added in 1.3. The analysis previously estimated partial 1, subtracted it from
the waveform frame, estimated partial 2 from what was left, and so on. That is
only the least-squares answer when the partial bases are orthogonal to each
other, and at an aperture of a few fundamental periods they are not: a Hann main
lobe is four bins wide, so an aperture of `P` fundamental periods separates
adjacent partials by `P/2` main-lobe half-widths. The pitch-adaptive aperture
targets four periods and rounds up to a power of two, which lands between four
and eight, so adjacent main lobes touch and each subtraction leaks into its
neighbours. The damage falls on quiet partials beside loud ones, which is where
it is most audible.

The pass is now repeated three times with each partial's current estimate added
back before it is re-solved against the others' residual — Gauss-Seidel on the
joint normal equations, which converges to the joint least-squares solution
without forming or factorising the `2N x 2N` system. The first sweep is
bit-identical to the old behaviour because every estimate it adds back is still
zero, and only the change in each estimate is written back, so the stored
residual never accumulates an add/subtract rounding pair.

Refinement is bought only where it is needed. Past about eight periods the bases
are orthogonal to working precision, so the parallel 4096-sample modal aperture
— eighteen periods at a mid pitch, and the most expensive frame in the pass —
keeps its single sweep. Applying refinement there as well cost 2.2x on
`learn()` and made the noisy fixture's residual ERB MAE *worse* (3.54 → 3.66 dB)
rather than better, which is what the orthogonality argument predicts.

| Case | sequential | joint |
| --- | --- | --- |
| `learn()` on a 1.6 s / 44.1 kHz source | 0.609 s | 0.765 s |
| Excitation parity error | 1.043 dB | **0.923 dB** |
| Held-out Body-Locked shape MAE | 2.790 dB | **2.714 dB** |
| Root spectral convergence, source/filter | 0.0412 | **0.0392** |
| Root residual ERB MAE, source/filter | 5.00 dB | **4.73 dB** |
| Root cumulative-energy MAE, source/filter | 3.16 dB | 3.45 dB |

The `learn()` row is this container's own before/after measurement of the same
scratch fixture, best of three, and is not comparable with the 0.557 s figure
published for 1.1 on another machine; the pre-change number on this container is
0.609 s. The 26% increase is bounded and offline.

The one row that moved the wrong way is early cumulative energy, and it moved by
0.28 dB against a 12.5 dB error that the joint solve did not create. That error
is the analysis aperture, not the solve: the aperture is 21 ms at 220 Hz, so the
first frame's amplitudes describe an average of the first 21 ms and the renderer
applies them from sample zero. Removing the four-period constraint is what the
joint solve is *for*; shortening the aperture is the next step and is where it
is expected to pay.

### Stiff-string partial placement

A struck-string fixture with a known coefficient `B = 4.0e-4` is learned once and
rendered at -12, 0, and +12 semitones. Partial placement error is the mean
absolute deviation, in cents, between rendered partials 6, 10, and 14 and the
independently generated stiff-string targets, located by an analytic sweep with
parabolic refinement.

| Metric | 1.0 (no stiff-string model) | 1.1 | 1.2 |
| --- | --- | --- | --- |
| Fitted coefficient | not modelled | 4.0145e-4 (true 4.0e-4) | 4.0145e-4 |
| Detected root | — | 219.94 Hz (true 220 Hz) | 219.94 Hz |
| Partial placement, mean over -12/0/+12 st | 37.44 cents | **0.095 cents** | **0.094 cents** |

The 1.0 column is measured by rendering the same 1.1 model with `Stretch` at 0%,
which reproduces the ideal harmonic bank every earlier release used. The
engine-side test in `Tests/NeuramarEngineTests.cpp` repeats the experiment with
a different coefficient (`B = 3.0e-4`) and a different fixture and reports
0.20 cents fitted against 27.81 cents ideal-harmonic.

This measures placement of the partial series only. It says nothing about
amplitude, decay, or perceived similarity to a real piano.

### Render-path quality

Added in 1.2 and printed by `Tests/NeuramarEngineTests.cpp`. Every figure is a
render measurement, not a fit measurement, and every one of them is now pinned
by a regression guard in the same binary.

| Measurement | 1.2 result | Regression guard |
| --- | --- | --- |
| Worst spectral line below the played fundamental, MIDI root+24/+36/+45 at 44.1/48/96 kHz | -108.2 dB relative to the rendered signal | -85 dB |
| Worst partial deviation across 44.1/48/88.2/96/192 kHz, root and root+12/+24 | 0.078 dB | 0.35 dB |
| Worst Air+Bone layer power deviation across 44.1/48/96/192 kHz | 0.126 dB | 1.0 dB |
| Bone mode driven to 22.7 kHz, relative to the audible mode below it, at 48 and 96 kHz | -128.3 dB | -60 dB |
| Level of that audible Bone mode between 48 and 96 kHz | 0.049 dB | 0.5 dB |
| Awaken fade level at 5% / 50% / 105% of its stated time | 0.008 / 0.501 / 1.000 | < 0.05 / 0.42-0.58 / > 0.99 |
| Peak output after 3200 note-ons one sample apart into eight voices, Output at 0.08 | 0.43 | 2.0, and no growth over the same burst at 200 note-ons |

Nothing may live below the played fundamental: the partial series starts there
and the anti-alias taper deletes everything that would fold back, so the first
row is simultaneously an aliasing floor and a bound on the oscillator's own
approximation error. The oscillator was measured separately against `sin()` in
double precision over 2e7 uniformly spaced phases in `[0, 1)`: the 1.2
polynomial has a peak error of 2.08e-7 (-133.6 dB) and an RMS error of 3.78e-8
(-148.5 dB), against 5.28e-7 (-125.6 dB) and 1.87e-7 (-134.6 dB) for the
4096-entry linearly interpolated table it replaces - 8.1 dB better on peak and
13.9 dB on RMS. These figures come from a scratch harness rather than the test
binary, so they are the one row here that a reader has to reproduce rather than
run; the shipped guard on the same property is the spur-floor row above.

The Air+Bone power row is a guard on the two layers as a whole and not a
discriminator for the 20 kHz taper anchor that 1.2 added: on that fixture the
anchor is worth at most a few tenths of a dB of total layer power, because the
memory's top Air band carries little of it, and reverting the anchor still
passes. The two Bone rows are the discriminating measurement. Bone is modal, so
it can be probed one spectral line at a time: three octaves up with Body Lock
open puts the fixture's top mode at 22.7 kHz, which is inaudible at every host
rate, yet sits well inside `0.49 * 96 kHz`, part-way down the fade below
`0.49 * 48 kHz`, and above `0.49 * 44.1 kHz` altogether. With the previous
host-anchored taper that mode renders at +0.4 dB relative to the audible mode
below it at 96 kHz, at -5.1 dB at 48 kHz and not at all at 44.1 kHz - the same
memory, three different top ends. With the audible-band anchor it is gone at
every rate, 128 dB down. The Air taper is the same one-line change against the
same fixed ceiling, but it is applied to a noise layer whose bands cannot be
separated line by line, so it is covered by inspection of `prepare()` plus the
whole-layer power row above rather than by a discriminating render test.

The voice-steal row is a stress bound, not a musical measurement: 3200 note-ons
one sample apart is roughly 48000 note-ons per second, far past any real MIDI
stream. It is pinned because the fade tail carries a still-running tail into a
newly stolen slot, and an earlier version of that carry re-armed the fade window
each time, which made it an integrator whose peak grew with the length of the
burst - 1.38 after 200 note-ons and 7.61 after 3200, against the +-7.95
finite-output guard - instead of a bounded hand-off.

The Awaken row is the control's contract rather than a tolerance: dividing a
faded render by an otherwise identical unfaded one recovers the envelope
exactly. Before 1.2 the same knob drove a one-pole whose stated seconds were a
time constant, so a 0.25 s setting stood at 0.393 halfway through and 0.65 just
past its stated time, and reached full level only after about 1.15 s.

### Cost

Measured with the same binaries, best of three runs, on the same container.
The 1.0 column was produced by reverting only the changed hot paths in a
scratch copy of the tree, so the workload is identical. The 1.1 column of the
first two rows holds this container's own pre-1.2 measurement rather than the
figure published with 1.1, because absolute times from another machine are not
comparable; the 1.1 release numbers for those two rows were 0.067 s and
0.211 s.

| Case | 1.0 | 1.1 | 1.2 |
| --- | --- | --- | --- |
| Eight voices, two octaves above the root, 1 s at 48 kHz | 0.156 s (6.40x realtime) | 0.0573 s (17.5x realtime) | **0.0487 s (20.5x realtime)** |
| Eight voices, two octaves below the root, 0.5 s at 48 kHz | 0.354 s (1.41x realtime) | 0.1848 s (2.71x realtime) | **0.0890 s (5.62x realtime)** |
| `learn()` on a 1.6 s / 44.1 kHz source | 0.715 s | **0.557 s** | unchanged |
| Tabulated vs direct windowed-sinc resampling of 5 s at 48 → 12 kHz | 0.060 s | **0.0033 s** | unchanged |

The resampler row is an in-test A/B: the same regression binary runs a literal
transcription of the pre-1.1 direct-evaluation kernel and the shipped polyphase
table over identical input and reports both the speedup and the difference
between their outputs, which is below -150 dB. The 1.1 `learn()` figure includes
the new stiff-string estimation pass, which 1.0 did not perform at all. Neither
row moved in 1.2: the analysis and fit path is offline and was deliberately left
alone, because only `NeuramarEngine::process()` and `noteOn()` run on the audio
thread.

`Tests/EngineBenchmark.cpp` covers the same render path with a longer musical
workload. It is built with the suite but not registered with CTest, so it never
gates a build on wall time.

| Benchmark scenario, 8 s at 48 kHz / 256-sample blocks | 1.1 | 1.2 |
| --- | --- | --- |
| Low chord, C1 root, 8 voices | 2.868 s (2.8x realtime) | **1.352 s (5.9x realtime)** |
| Mid chord, C3 root, 8 voices | 1.279 s (6.3x realtime) | **0.743 s (10.8x realtime)** |
| High chord, C6 root, 8 voices | 0.528 s (15.1x realtime) | **0.497 s (16.1x realtime)** |
| High chord with Air and Bone at zero | 0.499 s (16.0x realtime) | **0.190 s (42.2x realtime)** |
| Single mid note | 0.150 s (53.4x realtime) | **0.083 s (96.0x realtime)** |
| Note-on at C1 | 20.9 us | **13.4 us** |
| Note-on at C4 | 5.77 us | **4.45 us** |

The last three rows are targeted A/Bs against the current tree with one change
reverted, because those cases did not exist as measurements before 1.2: the
silent-layer rows were produced by forcing the Air and Bone loops to run
unconditionally again, and the note-on rows by recomputing the learned onset
phases' unit vectors per call instead of caching them per model.

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
