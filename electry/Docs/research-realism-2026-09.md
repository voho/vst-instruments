# Electry realism research — 5 September 2026

The best supported direction is to preserve Electry's playable physical engine,
correct demonstrable state/geometry errors, and identify the remaining contact
models with controlled recordings. Recent neural work provides useful comparison
methods and future rendering options; it does not establish a better replacement
for this eight-string, low-latency instrument. No research result, engineering
test, or listening verdict in this note establishes a world-leading claim.

This pass inspected the runtime's excitation, damping, pickup and release paths,
the existing evaluation history, and the primary sources below. Publication dates
come from the papers or archive records, rather than search-engine crawl dates.
No new reference audio was downloaded, decoded, fitted or auditioned for this
research note. The existing source comparisons in [evaluation.md](evaluation.md)
remain historical results, not newly reproduced measurements.

## Implementation and engineering measurements

This pass corrects three existing mechanisms without adding parameters,
dependencies or fitted physical constants:

- **Fractional string reads and excitation writes:** split the fractional delay
  before subtracting the integer ring cursor. Previously the same physical
  string state interpolated differently at different memory positions. The
  regression rotates both operations through all 16,384 positions and compares
  against independent double-precision cubic/linear references, including
  non-dyadic delays, exact integers, both clamps and wraparound. The old code
  fails all three assertions; the correction gives zero origin variation,
  maximum read error `4.38e-8`, and zero write error in that fixture.
- **Saddle vibration:** preserve the existing resonator history when a ringing
  string is repicked or refretted. Previously a new note erased that history
  instantly. A real-excitation regression covers Sustain/Palm, same-note and
  octave retargets at 44.1, 48, 96 and 192 kHz, plus fresh-note and reset
  lifecycles. All 16 continuity cases fail with the old reset.
- **Palm-held legato:** retain the planted hand's existing attack absorption
  when the fretting hand hammers, pulls or slides. The continuous Palm Pressure
  path already applies that absorption; a retained Palm-style hand must do the
  same without generating another picking-hand impact. The regression compares
  those equivalent hand states across four sample rates and both directions.

An isolated delay-line spectral diagnostic used a 16,384-sample sinusoid at bin
997 (5,841.797 Hz at a 96 kHz clock), a fixed 38.314159-sample delay, one full
cursor rotation and a rectangular FFT. Total non-carrier residual fell from
**−82.16 to −146.98 dBc**; the strongest spur fell from −86.60 to −172.93 dBc.
This is numerical interpolation evidence, not a measured guitar recording or
a claimed perceptual improvement of that size. The fixed low-E metal score
barely changes under these numerical/state corrections; the added muted-legato
audition exercises the separate retained-hand correction.

Before/after raw float renders, complete-file RMS-matched listening copies,
diagnostic source files, hashes and test logs are generated under the ignored
`build-realism-20260905/` directory. Its `comparison.html` presents the same MIDI
and FX settings in both conditions. These labeled comparisons are review
artifacts, not listener responses or a completed blind study.

The final muted-legato phrase changes whole-file RMS by −0.052 dB dry and
−0.026 dB through high gain. Its difference signal is −27.30/−25.01 dB relative
to the baseline, respectively. Thus the corrected gesture changes the waveform
without relying on a general loudness increase. The unchanged 40-hit score's
differences are only −104.46 dB dry and −98.99 dB amplified; it does not exercise
the retained-hand bug and should not be presented as an audible realism win.

Validation used baseline commit `b7c30f9e` and the final working tree, native
arm64 Release builds on macOS with AppleClang 21 and the project's pinned JUCE
8.0.14. All **12 CTest targets passed**: nine DSP/rendering/evaluation-tool
targets (101.77 s for the final complete run) and three processor/VST3/AU
targets (10.81 s). The exact built VST3 and AU were loaded, rendered and
state-round-tripped. Standalone also built successfully. AddressSanitizer and
UndefinedBehaviorSanitizer reported no issues in the three new regressions.
Native VST3, AU and Standalone bundles are staged under
`build-realism-20260905/plugin/`; this pass did not test an x86_64 slice or run
a DAW listening session.

Reproduce the full DSP/tool checks with:

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DELECTRY_BUILD_PLUGIN=OFF -DELECTRY_BUILD_UNIVERSAL=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

For host-artifact checks, configure another Release build with
`ELECTRY_BUILD_PLUGIN=ON`, build it, then run
`ctest --test-dir <build> --output-on-failure -R 'PluginProcessor|VST3Artifact|AUArtifact'`.

The broader FX audit also records a remaining limit. At a 48 kHz host, strong
0.3-peak isolated 3.5, 6.5 and 10.5 kHz tones through the fully driven Modern amp
produced nonharmonic residuals of approximately −86.29, −86.69 and −79.85 dBFS.
Driving the nonlinear frames with an ideal high-rate sine and replacing output
decimation changed those results by less than 0.05 dB: the dominant residual is
generated inside the 384 kHz nonlinear circuit. The existing approximately
1.26 kHz alias test therefore cannot support a uniform −70 dBc claim across
the entire input band. No cabinet/EQ change was made to conceal this result;
a future antialiasing change needs a bandwidth, CPU and listening comparison.

## What the current engine already does

`Source/DSP/ElectryEngine.cpp` already combines two string polarisations,
stiffness dispersion, frequency-dependent loss, a phase-compensated moving
period, physical pick/pickup coordinates, shared stroke variation, independent
fretting and picking gestures, shared bridge-hand damping, body resonances and
sympathetic strings. Its force/release mapping includes a stiffness-limited
release rate; simply adding velocity-sensitive brightness or random pick noise
would duplicate existing mechanisms.

Several more ambitious mechanisms are already experiments or rejected results:
energy-derived attack pitch, positioned following-fret contact, reciprocal
repick contact, distributed Palm contact, measured pickup flux and alternate
cabinet paths. The evaluation records engineering failures and/or source
regressions. A newer paper is not a reason to bypass those results. In
particular, the 29 August decision selected phase-conditioned repick measurement
before another sound-changing contact model.

## Current primary research and its practical meaning

| Work | Evidence | Consequence for Electry |
| --- | --- | --- |
| [P-MUSE, Jing et al., August 2026](https://arxiv.org/abs/2608.01920) | A preprint unifying prompt-audio/MIDI generation and local editing, with guitar among four instrumental benchmark families. Its shared formulation generates musical segments and fills missing regions. | A current offline rendering/editing comparison lead. The inspected abstract and project demonstrations do not establish sample-causal performance, E1 eight-string technique control, or suitability for an audio callback. Those properties need separate evidence before considering runtime integration. |
| [A Semantic Timbre Dataset for the Electric Guitar, Cameron and Blackwell, March 2026](https://arxiv.org/abs/2603.16682) | Monophonic tones labeled with 19 semantic descriptors derived from guitar effects; the authors evaluate a VAE and human perceptual judgments. | Useful for studying timbral description or preset navigation. Effects-derived semantic labels do not measure dry plectrum force, palm pressure, or ringing-string phase. It does not close the contact-calibration gap. |
| [GuitarFlow, Loth et al., October 2025](https://arxiv.org/abs/2510.21872) | Guitar tablature, including bends, muted strings and legato, first drives a sample instrument; flow-matching style transfer improves its realism in the authors' listening study using under six hours of training audio. | The useful lesson is technique-aware conditioning and listening validation. Electry already encodes these gestures. The result does not demonstrate live eight-string state continuity or measured Palm/contact coefficients; it is an adjacent generative approach, not a validated drop-in improvement. |
| [Neural Audio Synthesis for Non-Keyboard Instruments, Caspe et al., WASPAA 2025](https://www.waspaa.com/waspaa25/proceedings/WASPAA2025-290.pdf) | BRAVE waveform autoencoding is demonstrated in a C++ audio plugin with approximately 10 ms input/output latency. It transfers expressive audio input toward another instrumental timbre. | Real-time neural audio is feasible. This particular evidence concerns audio-to-audio control, however, and does not solve MIDI-driven E1 picking or claim eight-string metal-guitar fidelity. |
| [Pluta et al., pluck-trajectory measurements, 2025](https://vibsys.put.poznan.pl/_journal/2025-36-2/articles/vps_2025205.pdf) | A robotic acoustic-guitar plucker changes attack depth in 192 micrometre steps and tests six pick materials. Depth and material affect level, low-frequency fullness, noisiness and harmonic structure. | Keep force, depth and release mechanics distinct. Do not assume that harder MIDI velocity universally means more high-frequency energy, or infer coefficients from uncontrolled performances. The study supports measuring joint stroke variation; it does not provide an eight-string DI calibration. This source is already cited in Electry's evaluation. |
| [Real-Time Guitar Synthesis, Bilbao et al., DAFx 2024](https://www.pure.ed.ac.uk/ws/portalfiles/portal/470239305/BilbaoEtal2024RealTimeGuitarSynthesis.pdf) | Energy-based numerical string/finger/fretboard interaction is implemented in C++. Table 1 reports 0.11 s for a conventional low-E string and approximately 0.50 s summed across six strings per second of 44.1 kHz audio on an M2 Pro. | A full finite-difference engine is a credible research alternative; calling it categorically too slow would be inaccurate. Drop-E, eight strings, Double, oversampling and the amp path require a fresh workload measurement. Adopting the full solver would be a substantial architecture experiment, not a small safe improvement. |
| [Differentiable Modal Synthesis, Lee et al., NeurIPS 2024](https://arxiv.org/abs/2407.05516) | Physics-conditioned neural modal synthesis predicts nonlinear planar string motion and outperforms the paper's simulation baselines. | Potential offline reference or parameter-identification aid. Accuracy against simulated motion is different from blind electric-guitar realism, and neither deployment cost nor the relevant captured eight-string gestures is established by that result. |
| [String-wise MIDI DDSP guitar, Jonason et al., DAFx 2024](https://www.dafx.de/paper-archive/2024/papers/DAFx24_paper_49.pdf) | The simplest unified model performs best among four neural systems, but its listening MOS is 3.38 versus 4.08 for the sample baseline and 4.10 for natural audio. It is trained on six-string acoustic GuitarSet; bidirectional recurrent models use future context and the harmonic synthesizer omits inharmonicity. | Newer neural methods do not automatically beat a well-made sampler. Preserve string identity and technique, test objective metrics against listeners, and avoid importing the model's causality and pitch restrictions into this live physical instrument. |

The practical distinction is between learning **parameters of a controllable
model** and replacing its audio output with a generator. Offline fitting of a
small passive contact or loss model could retain the existing runtime cost and
control surface. It still requires identifiable captures and held-out validation;
adding a neural network does not remove that requirement. This is an engineering
recommendation inferred from the evidence above, not a published Electry result.

## The most valuable missing measurement

The unresolved mechanism with the clearest metal-specific consequence remains
the interaction of a moving pick with a string that is already ringing. The
shipping contact attenuates the loop over a short fixed interval, then applies
the release excitation. It does not identify contact force from the local
incoming wave and the moving pick's trajectory. Rapid chugs can therefore pass
timing and stability tests while still having incorrect second-attack body,
brightness or phase dependence.

The engine already has a default-off reciprocal contact candidate and the
evaluation already records a moving unilateral-contact ablation. Repeating
either without new evidence would repeat a failed experiment. The bounded next
experiment is the existing
[electry-repick-phase/v1](capture/electry-repick-phase-v1/README.md) protocol:
controlled exact-eight E1/E2 stroke pairs, synchronized contact and phase
measurement, separate training and untouched holdout performance clusters.
Only if the physical response exceeds repeatability should one frozen contact
candidate be fitted. A passive local update must then satisfy both its own
energy invariant and whole-engine tuning, gain and CPU gates.

For code changes that can be justified today, prioritize invariants that do not
need a new voicing coefficient: the correct hand owns each action; a ringing
repick preserves the string and fretting finger; contact/pickup geometry follows
the live speaking length; silent, cancelled and released contacts cannot create
phantom gestures; sample rate or host blocks cannot change the performed event.
These are concrete opportunities for an independent correctness audit, not
assertions that each currently fails.

## Data and validation

[Guitar-TECHS](https://guitar-techs.github.io/) is a 2025 CC BY 4.0 release with
over five hours, three players, DI/amp/microphone views and string-aware MIDI.
Its six-string equipment and technique coverage make it useful for transfer
checks and timing/label audits. MIDI pickup labels are not a measured plectrum
force or calibrated contact clock.

[EG-IPT](https://zenodo.org/records/15205644), published 13 April 2025, contains
52,320 mono recordings across 19 techniques, captured at 96 kHz/24 bit on a
Gibson SG with simultaneous DI and amp microphone channels. It is particularly
useful for conventional-register articulation and common-chain comparisons.
Those files are multiple capture views, not 52,320 independent eight-string
performances, and they do not establish Drop-E contact parameters. Both datasets
have already been used in Electry's documented exploratory audits; their used
material cannot silently become untouched holdout data again.

Use the existing evaluation tools and frozen listening contract rather than
another new framework. Engineering validation should measure pitch and decay,
transient and body energy, rapid-repick behavior, contact/state invariants,
sample-rate/block invariance and release CPU cost. Render the same score and
parameters before and after each candidate. Keep dry DI analysis separate from
a second comparison through the same high-gain chain: distortion makes small
attack differences audible but can also hide a weak dry model. Level matching
must preserve the relative accents inside a phrase.

Spectral distance, transient error, FAD-like distribution metrics and a successful
stress test are diagnostic evidence. None is a substitute for blind listening
against real recordings. The existing Palm/Dead protocol already fixes scored
content, levels, listener strata, repeat checks, sample sizes and uncertainty;
its template correctly says that licensed holdout captures are missing. This
research pass does not fabricate an executed listening study from that template.

## Competitive claim

For a future licensed comparison, include current eight-string peers rather than
an old generic GM guitar. For example, the official
[Odin III product page](https://solemntones.com/collections/the-nordic-line/products/odin-iii)
currently specifies an ESP LTD SC-608, every fret on eight strings, clean DI,
31 articulations and four kinds of palm mute. Those are verified vendor feature
claims, not independent evidence of superiority. The existing evaluation also
identifies Uproar RAW and Shreddage 3 Hydra as relevant peers. No competing
plugin was purchased, installed, rendered or used as training data in this pass.

An honest leading-product claim needs a predeclared head-to-head on held-out
metal phrases and multiple players/guitars, with comparable programming effort,
shared reamping, level matching and confidence intervals. Passing the narrower
existing two-source-cluster pilot would be useful evidence for those stimuli;
it would not establish a universal ranking across players, instruments and
techniques. The current deliverable can establish specific engineering
improvements and reproducible limitations. The planetary superlative remains an
unproven aspiration until that comparison exists.
