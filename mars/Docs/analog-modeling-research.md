# Mars analog-modeling research and implementation contract

Mars is a white-box virtual-analog synthesizer, not a black-box emulation of a
particular hardware unit. Its DSP combines established antialiasing and
topology-preserving techniques with bounded nonlinear processing so that a
polyphonic instrument remains practical in a DAW. This document distinguishes
the research lineage from what the current source actually implements.

## Decision

| Candidate | What it offers | Limitation for Mars | Decision |
| --- | --- | --- | --- |
| Full transient SPICE / modified nodal analysis | Direct relationship to a chosen schematic | Nonlinear solves and device equations multiply across every oscillator and render slot | Use only as a future offline reference for a measured circuit |
| Wave Digital Filters (WDF) | Circuit-oriented, modular modeling | Coupled nonlinear devices require a joint solve; Mars currently has no WDF block | Reconsider only for a small, measured subsystem |
| Runtime neural VCO or filter | Can interpolate a captured device when trained on representative data | Mars has no selected hardware target or measurement/training set, and inference cost multiplies by oscillator and render slot | Do not use until measurements demonstrate a clear accuracy benefit |
| Event-corrected oscillators, first-order ADAA, and nonlinear delay-free/TPT filters | Low aliasing, continuous controls, bounded behavior, deterministic rendering, and practical polyphony | Must be validated explicitly and must not be described as a transistor-level clone | **Implemented** |

## Implemented signal path

The authoritative implementation is `Source/DSP/MarsEngine.cpp`:

1. VCO I and VCO II generate saw, pulse, or triangle waveforms. Saw and pulse
   discontinuities receive a standard local polyBLEP correction. Triangle is
   formed with a stable leaky integrator driven by a corrected 50-percent square
   wave. A corrected pulse sub oscillator runs one octave below VCO I, and each
   render slot has a deterministic noise source.
2. The oscillator balance uses equal-power gains. Sub and noise are added, then
   the driven per-voice mixer passes through first-order antiderivative
   antialiasing (ADAA).
3. The selected filter algorithm runs from the mixer signal. `Ladder` is a
   bounded nonlinear four-stage, four-pole ladder-inspired path with a
   current-sample feedback solve. `Orbit` is a two-integrator
   topology-preserving state-variable filter; `Filter shape` moves continuously
   from low-pass through band-pass to high-pass. Both algorithms run only during
   a 3 ms model transition; steady-state voices process the selected filter
   alone, which keeps per-slot cost bounded as polyphony grows.
4. Independent exponential filter and amplifier ADSRs, velocity gain, a bounded
   per-slot VCA output, and equal-power pan complete each render slot.
5. A global cross-fed stereo modulated delay supplies the ensemble. Its only
   controls are `Ensemble mix` and `Ensemble rate`. A DC blocker, output gain,
   and bounded finite-output guard finish the host-rate path.

The current engine contains exactly the two filter models, listed modulation
sources, and single ensemble described above. It has no circuit-device solver,
learned inference block, arpeggiator, modulation matrix, or additional spatial
network.

## Oscillator antialiasing

A naively sampled saw or pulse aliases because an instantaneous discontinuity
contains energy above Nyquist. Increasing the sample rate helps nonlinear
stages, but it does not by itself remove the discontinuity. Mars applies a
compact polynomial correction around each wrap and pulse-width edge, then uses
a bounded leaky integration of a corrected square wave for triangle. This
prevents a long-held triangle from accumulating floating-point DC error.

This choice follows the efficient event-correction family described by:

- Stilson and Smith,
  [*Alias-Free Digital Synthesis of Classic Analog Waveforms*](https://quod.lib.umich.edu/i/icmc/bbp2372.1996.101/--alias-free-digital-synthesis-of-classic-analog-waveforms?rgn=main%3Bview%3Dfulltext),
  which frames discontinuity correction for classic subtractive waveforms;
- Välimäki and Huovilainen,
  [*Antialiasing Oscillators in Subtractive Synthesis*](https://research.aalto.fi/en/publications/antialiasing-oscillators-in-subtractive-synthesis/),
  which compares practical oscillator-antialiasing families; and
- Kleimola and Välimäki,
  [*Reducing Aliasing from Synthetic Audio Signals Using Polynomial Transition Regions*](https://research.aalto.fi/en/publications/reducing-aliasing-from-synthetic-audio-signals-using-polynomial-t/),
  which develops local polynomial transition corrections.

These papers establish the design family; Mars uses its own compact standard
polyBLEP implementation. It does not claim a BLEP table, BLAMP triangle, hard
sync model, or neural oscillator that is absent from the source.

## Nonlinear mixer and ADAA

The mixer nonlinearity is

```text
f(x) = x / sqrt(1 + x^2)
F(x) = sqrt(1 + x^2)
```

where `F` is an antiderivative of `f`. For successive mixer inputs, the
first-order ADAA output is the divided difference
`(F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])`. The implementation uses the
midpoint value when the difference is very small and a finite-value fallback.
ADAA is applied to the driven per-voice mixer; it is not claimed for the
stateful filters or final bus saturator.

That boundary follows Bilbao, Esqueda, Parker, and Välimäki,
[*Antiderivative Antialiasing for Memoryless Nonlinearities*](https://www.dafx.de/paper-archive/details/vem_XXF5qBbfiWOH2RVVAA).
The method reduces aliasing from a known memoryless function without requiring
a learned surrogate, while its use here remains small and auditable.

## Filters and feedback

The `Ladder` path cascades four TPT-style one-pole stages around a bounded
nonlinear input and feedback path. Resonance feeds the current output back to
the current input, so the code performs two slope-bounded Newton updates rather
than inserting a one-sample delay; the final response is soft-saturated and the
host-rate output has a finite safety guard. This is a computationally bounded
ladder-inspired model; it is **not** a transistor-pair or component-value model.

The relevant virtual-analog lineage includes:

- Huovilainen,
  [*Non-Linear Digital Implementation of the Moog Ladder Filter*](https://www.dafx.de/paper-archive/details/UervgvkeeDC1a4sWDluM4Q),
  for efficient nonlinear ladder modeling;
- D'Angelo and Välimäki,
  [*Generalized Moog Ladder Filter: Part II — Explicit Nonlinear Model through a Novel Delay-Free Loop Implementation Method*](https://research.aalto.fi/en/publications/generalized-moog-ladder-filter-part-ii-explicit-nonlinear-model-t/),
  for delay-free nonlinear feedback; and
- Zavalishin,
  [*The Art of VA Filter Design*](https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.0.pdf),
  for topology-preserving transform integrators and zero-delay-feedback
  formulations.

`Orbit` uses the trapezoidal/TPT state-variable equations with integrator-
equivalent states. Its input has bounded drive, and its low-, band-, and
high-pass outputs are blended by `Filter shape`. `Orbit` is an original Mars
response family, not a measured emulation of a named circuit.

## Oversampling boundary

At host sample rates up to and including 96 kHz, Mars renders each slot's
oscillator, mixer, filter, envelope, pan, and voice-bus path at 2x, sums the
slots, then decimates the stereo sum through one 15-tap halfband FIR. Above 96
kHz it renders that path at 1x. Parameter smoothing, global LFO updates,
ensemble, DC blocking, and output gain advance at the host rate; the current
LFO value is held for the internal sub-samples.

This fixed boundary targets the alias-producing oscillator and nonlinear voice
path while avoiding unnecessary 2x work when the host rate is already above
96 kHz. It is not an adaptive quality mode and it does not oversample the
ensemble.

Pitch, envelope timing, level, and stability are regression-tested through
384 kHz. The implementation accepts rates up to a guarded 768 kHz ceiling;
384 kHz is the highest rate for which the current suite makes an explicit
accuracy claim.

## Deterministic voice-card variation

Mars owns 32 fixed render slots, each paired with a deterministically seeded
voice card. A card supplies small offsets for oscillator tuning, filter cutoff
and resonance, drive, envelope time, pan, pulse-width skew, and a distinct
slow-drift phase/rate/depth. Identical state, notes, and automation render
identically; no per-sample random detune is injected.

`Voice-card drift` scales the audible tuning wander and much of the calibration
spread. The implementation does not simulate worn parts.

The 32 slots are allocated in groups:

- `Poly` uses one slot per note, with a 16-note-group ceiling;
- `Unison` uses 2–8 slots per note, so note capacity is
  `min(16, floor(32 / Unison voices))`; and
- `Fifth` uses two slots per note (root plus seven semitones), allowing up to 16
  note groups.

When room is needed, released groups are stolen oldest-first, followed by the
oldest still-held group. Retriggered or stolen slots contribute their final
sample through a fixed 2 ms, -60 dB tail so reallocation does not create a hard
sample discontinuity.

## Why Mars does not use runtime neural VCO/filter inference

A neural emulator learns a target dataset; the network architecture alone does
not make its output physically accurate. Mars currently has no chosen hardware
VCO/filter, capture chain, calibrated sweeps, or representative training set
over pitch, cutoff, resonance, drive, modulation rate, temperature, and state
history. Training a generic network under those conditions would add an opaque
nonlinearity, not evidence of analog realism.

Mikkonen, Wright, and Välimäki,
[*Sampling the User Controls in Neural Modeling of Audio Devices*](https://link.springer.com/article/10.1186/s13636-024-00347-5),
shows that coverage of the conditioned control space is a central part of
device-model accuracy. That requirement is especially important for a filter,
whose response depends jointly on controls, signal level, and internal state.

The 2025 paper by Simionato and Fasciani,
[*Towards Neural Emulation of Voltage-Controlled Oscillators*](https://www.dafx.de/paper-archive/2025/DAFx25_paper_33.pdf),
reports 9,216, 34,816, and 141,312 floating-point operations per sample for its
16-, 32-, and 64-unit LSTM variants, while also discussing low-frequency and
frequency-generalization limitations. At 48 kHz, the smallest result is about
0.44 GFLOP/s per oscillator. Two such models across 16 active slots imply about
14.2 GFLOP/s; across all 32 Mars render slots they imply about 28.3 GFLOP/s,
before filters, envelopes, mixing, and effects. These are arithmetic estimates
from the paper's FLOP/sample counts, not measured Mars CPU benchmarks.

Neural or differentiable methods remain useful for **offline** fitting once a
target exists. A future measured component surrogate should be compared with
the analytic block for aliasing, control interpolation, state stability,
latency, and CPU before it enters the runtime. Until then, the transparent
polyBLEP/ADAA/delay-free/TPT path is the stronger accuracy-per-cost choice.

## Validation boundary

Current tests establish engineering invariants such as finite bounded output,
release completion, deterministic rendering, distinct oscillator/filter
responses, long-note triangle stability, click-resistant voice stealing and
filter switching, meaningful glide/mod-wheel behavior, voice allocation,
sample-rate coverage through 384 kHz, and a CPU guardrail. They do not turn
Mars into a hardware clone.

A claim of measured circuit accuracy would additionally require:

- a named target and documented capture/calibration chain;
- oscillator spectra and alias-energy comparisons across pitch and PWM;
- cutoff, resonance, drive, self-oscillation, THD, IMD, and multitone sweeps;
- modulation and transient comparisons across the full control space; and
- null/error analysis against hardware captures or a validated small-timestep
  circuit reference.

The present claim is deliberately narrower: Mars is an original, deterministic
nonlinear virtual-analog instrument whose DSP choices are traceable to the
implementation and the cited primary literature.
