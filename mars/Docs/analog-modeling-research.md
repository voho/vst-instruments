# Mars analog-modeling research and implementation contract

Mars 1.2 is a white-box virtual-analog synthesizer with named reference targets,
not a black-box claim that one plug-in reproduces an entire vintage instrument.
This document separates circuit- or measurement-derived blocks from the parts
that remain stable, efficient Mars designs.

## Claims boundary

| Block | Reference target | What Mars 1.2 implements | Precise claim |
| --- | --- | --- | --- |
| Saw waveform | Minimoog Voyager recordings analyzed by Pekonen, Lazzarini, Timoney, Kleimola, and Valimaki | An event-corrected saw followed by the paper's pitch-dependent first-order post-EQ, with its 44.1 kHz pole and zero bilinearly remapped to the active internal rate | Measured contour over the fitted 86 Hz-8.3 kHz range, a neutral blend below that range, and a clamped fit above it; not a capture-perfect Voyager VCO |
| Pulse and triangle | Classic subtractive analog VCO behavior | PolyBLEP correction at pulse edges; bounded leaky integration for triangle; the same symmetric output-stage shaping used on both VCO paths | Antialiased, analog-conditioned classic waves; no named-hardware measurement claim |
| Cross modulation | Oberheim-style separation of oscillator mixer and X-Mod routing | VCO II keeps running and frequency-modulates VCO I even when VCO II's audio feed is Off | Hardware-inspired routing with bounded digital FM; not a transistor-level VCO interaction model |
| `Ladder` | Four-stage nonlinear Moog ladder model by D'Angelo and Valimaki | An independent bounded damped-Newton solution of the original implicit bilinear transistor equations, with residual-decreasing steps, a `tanh` differential pair in every stage, delay-free feedback, and `(1 + k)` DC-gain compensation | Circuit-derived generalized Moog ladder algorithm across the full 20 kHz control range; not a tolerance-by-tolerance model of one physical serial number |
| `SEM` | Oberheim/Sequential two-pole state-variable mode behavior | Nonlinear TPT state-variable core with the schematic's linear low-pass -> notch -> high-pass mode pot | SEM-inspired core and schematic-derived mode law; not a component-level SEM clone |
| Voice cards | Vintage polyphonic component spread | Fixed seeded offsets for pitch, cutoff, resonance, drive, envelopes, pan, pulse skew, and slow drift | Deterministic component-like variation, not measured calibration data |

## Implemented signal path

The authoritative implementation is `Source/DSP/MarsEngine.cpp`:

1. VCO I and VCO II generate saw, variable-width pulse, or triangle. Saw and
   pulse discontinuities receive local polyBLEP correction; triangle is a
   bounded leaky integration of a corrected square. The measured saw contour,
   including its sample-rate remap and low-note neutral blend, and a symmetric
   bounded output stage follow the waveform cores.
2. The two VCO mixer feeds have independent On/Off parameters. With both On,
   `Balance` uses an equal-power law. With only one On, that VCO has unity gain
   at every Balance position. A 4 ms gain smoother prevents a hard automation
   edge. Oscillator phases always advance, VCO II remains a cross-modulation
   source, and sub/noise remain independent.
3. Active VCO feeds, sub, and noise enter a fixed first-order antiderivative-
   antialiased (ADAA) mixer. `Filter drive` is then converted once into the
   voltage scale used by the filter; it no longer changes the mixer and VCA as
   unrelated extra gain stages.
4. The selected `Ladder` or `SEM` algorithm processes the mixer signal. The
   ladder uses a bounded residual-decreasing Newton solve without an artificial
   feedback delay and compensates its resonance-dependent static bass loss. A
   3 ms crossfade protects live model changes; at a steady endpoint only the
   chosen algorithm runs.
5. Independent exponential filter and amplifier ADSRs, velocity gain, bounded
   VCA shaping, deterministic pan, a host-rate stereo ensemble, a 1.5 Hz DC
   servo, and the final finite-output guard complete the path.

The persisted, non-automatable `hqOversampling` quality setting is On by
default. On renders the complete per-voice path at 2x for 44.1/48 kHz hosts,
and at every lower host rate, while running natively above 48 kHz. Standard
44.1/48 kHz sessions therefore use an 88.2/96 kHz internal path. Off always
renders natively. A requested change is deferred until the engine is idle
rather than resetting held voices.

## Measured saw contour

Pekonen et al.,
[*Discrete-Time Modelling of the Moog Sawtooth Oscillator Waveform*](https://doi.org/10.1155/2011/785103),
fit a first-order post-equalizer to recorded Minimoog Voyager saw waveforms:

```text
H(z) = g(f0) * (1 - b(f0) z^-1) / (1 - a(f0) z^-1)

g = 0.5400 + 4.473e-5 f0
b = 0.3894 - 3.102e-4 f0 + 2.417e-8 f0^2
a = 0.6398 - 2.417e-4 f0 + 1.335e-8 f0^2
```

The published discrete-time coefficients were identified at 44.1 kHz. Mars
evaluates the fits at the current VCO frequency, maps their pole and zero from
that reference rate to the active internal rate with a bilinear transform, and
restores the fitted DC gain. The same note therefore retains the intended
analogue corner when HQ oversampling changes the internal sample rate.

The fit input is clamped to the paper's approximately 86 Hz-8.3 kHz measurement
span. Above the range Mars retains the clamped contour. Over the octave below
86 Hz it blends toward the neutral event-corrected saw and is fully neutral by
approximately 43 Hz, instead of imposing the lowest measured contour on every
deep note. The filter state is guarded against non-finite input.

The source oscillator here is compact polyBLEP; the paper's best reported
post-EQ result used a fourth-order B-spline BLEP. Consequently Mars claims the
published measured spectral contour inside its fitted range, not the paper's
lowest-error complete configuration. Pulse, triangle, and sub do not reuse
saw-only coefficients.

## Oscillator event correction and output stage

A naively sampled saw or pulse aliases because its discontinuities contain
energy above Nyquist. Mars applies a compact polynomial correction around each
wrap and pulse-width edge, then uses a bounded leaky integrator for triangle.
This follows the efficient event-correction family described by Stilson and
Smith's
[*Alias-Free Digital Synthesis of Classic Analog Waveforms*](https://quod.lib.umich.edu/i/icmc/bbp2372.1996.101/--alias-free-digital-synthesis-of-classic-analog-waveforms?rgn=main%3Bview%3Dfulltext)
and Valimaki and Huovilainen's
[*Antialiasing Oscillators in Subtractive Synthesis*](https://research.aalto.fi/en/publications/antialiasing-oscillators-in-subtractive-synthesis/).

Both primary VCOs then pass through the same bounded, level-normalized output
shaper. This gives pulse and triangle finite analog-like edge/level behavior but
does not turn them into measured Minimoog or Oberheim waveform models. An exact
named-hardware claim for those shapes would require schematic-revision-specific
device equations or calibrated captures across pitch, pulse width, level, and
temperature.

## ADAA mixer

The mixer nonlinearity is

```text
f(x) = x / sqrt(1 + x^2)
F(x) = sqrt(1 + x^2)
```

where `F` is an antiderivative of `f`. For successive inputs, first-order ADAA
uses `(F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])`, with a midpoint evaluation for
nearly equal samples and a finite fallback. The method follows Bilbao, Esqueda,
Parker, and Valimaki,
[*Antiderivative Antialiasing for Memoryless Nonlinearities*](https://www.dafx.de/paper-archive/details/vem_XXF5qBbfiWOH2RVVAA).

ADAA applies only to this memoryless mixer. Mars does not claim ADAA for the
stateful filters or the final bus guard.

## Nonlinear Moog ladder

`Ladder` is informed by the four-stage case developed by D'Angelo and Valimaki,
[*Generalized Moog Ladder Filter: Part II - Explicit Nonlinear Model through a
Novel Delay-Free Loop Implementation Method*](https://doi.org/10.1109/TASLP.2014.2352556).
Mars does not port the paper's separately published non-iterative reference
implementation. It independently solves the original implicit, bilinear-
discretized four-stage transistor equations at each sample.

Mars assembles the nonlinear system's small cyclic-bidiagonal 4-by-4 Jacobian
and solves it analytically. Each Newton update is accepted only when it reduces
the squared equation residual. The normal pass permits 16 updates and eight
successively halved trial steps. If a hostile input/control jump still misses
the target, one bounded rescue pass permits 16 more updates and up to 20
halvings; the normal path pays no rescue cost unless the first pass misses. The
committed stage nonlinearities and same-feedback-gain history are cached exactly
for the next sample, avoiding redundant table evaluation without changing the
equations.
There is no allocation, unbounded convergence loop, or artificial sample of
feedback delay.

Production commits a candidate only when the equations evaluated with its
runtime `tanh` table are within `3e-4 * (2VT)`. If both fixed-cost passes miss
that ceiling, the last coherent state and input are held for one sample instead
of publishing an out-of-contract estimate. The next sample starts a fresh
bounded solve from that committed history.

The regression suite drives the entire reachable filter-input voltage range
with high-frequency square, sine, and deterministic-noise sequences at low,
medium, and maximum prewarped cutoff and six resonance anchors. Every production
step is checked against both the original equations evaluated with double-
precision `tanh` and an independently assembled double-precision 4-by-4 Newton
reference. The permitted maximum equation residual is `6e-4 * (2VT)` and the
per-stage reference error is `2e-3 * (2VT)`; current Release fixtures stay below
those explicit bounds. A deterministic control-jump fixture also covers a case
that requires the deep-backtracking rescue. This validation avoids the finite-
precision cutoff limitation of the paper's non-iterative form while keeping the
production work bounded across Mars's full 20 kHz range in native and HQ modes.

For four stages, normalized resonance maps to feedback `k = 4r`. Mars applies
the paper's resonance-dependent cutoff correction and bilinear prewarping, and
Panel Drive maps once into the physical voltage domain based on a 26 mV
transistor thermal voltage. Every stage evaluates the differential-pair
nonlinearity; `(1 + k)` DC-gain compensation restores the otherwise severe
static low-band loss as resonance rises without removing nonlinear resonance or
self-oscillation. The state is reset on any non-finite result and the engine
retains its final bounded-output guard.

This is materially different from placing one saturator around four linear
poles: the nonlinearity is inside every ladder stage. It is still a generalized
large-signal model rather than a simulation of power rails, temperature drift,
component tolerances, and every surrounding circuit in a particular Minimoog.

## SEM-inspired state-variable filter

The official
[*OB-6 Operation Manual*](https://www.sequential.com/wp-content/uploads/2021/02/OB-6-Operation-Manual-1.2.pdf)
documents the characteristic two-pole mode control as low-pass -> notch ->
high-pass, with band-pass selected separately. The
[*SEM-1A schematic*](https://synthfool.com/docs/Oberheim/Oberheim_SEM1A/Oberheim_SEM_1A_Schematics.pdf)
shows a linear 50 kOhm mode pot between the simultaneous low- and high-pass
outputs. Mars therefore uses `(1 - shape) * low + shape * high`; the midpoint
is the half-level notch sum `0.5 * (low + high)`, not band-pass.

The core uses trapezoidal/TPT state-variable equations, bounded input
nonlinearity, continuous cutoff and resonance, and finite state. This reproduces
the topology's useful mode relationship, but it does not yet include a selected
SEM revision's transistor/OTA device equations or capture-derived error curves;
the UI and documentation therefore say `SEM`-inspired rather than
circuit-accurate SEM.

## Oversampling and deterministic variation

HQ oversampling is a persisted, non-automatable quality setting and defaults to
On. At host rates through 48 kHz, On runs each active render slot's oscillator,
mixer, filter, envelope, and pan path at 2x. The summed stereo voice bus returns
through one 15-tap halfband FIR. Above 48 kHz, the host is already in the high-
rate range and On runs the nonlinear island at the native host rate; Off uses
the native rate at every host rate. Requested changes wait until the engine is
idle before rebuilding rate-dependent state, so an active note is not reset or
retuned mid-hit.

The global ensemble remains at host rate. A 1.5 Hz output DC servo removes
accumulated offset while preserving the weight and tuning of bottom-octave and
sub-octave fundamentals. The suite exercises rates through 384 kHz; the engine
guards API input up to 768 kHz.

Mars owns 32 fixed render slots. Each receives a deterministic voice card, so
identical state, MIDI, and automation produce identical audio. `Drift` scales
the fixed component-like spread and slow motion; it does not simulate worn
parts or randomize a saved sound on each playback.

## Why the runtime remains analytic

A neural emulator learns a target dataset; its architecture alone is not
evidence of hardware accuracy. Mars has no licensed, revision-specific capture
set covering oscillator wave, pitch, PWM, cutoff, resonance, drive, modulation,
temperature, and state history. A generic network would therefore add opaque
behavior rather than verified fidelity. The present analytic blocks are
auditable, deterministic, allocation-free in the audio path, and practical
across 32 render slots.

Neural or differentiable methods remain useful for offline fitting once a
target and calibration chain exist. Any future runtime surrogate must beat the
analytic block in capture error while also passing aliasing, interpolation,
state-stability, latency, and CPU tests.

## Validation boundary

Current automated tests establish finite bounded output, pitch and envelope
timing across sample rates, deterministic rendering, distinct waveform and
filter responses, deep-note and long-triangle stability, full-range ladder
stability, bass-gain compensation, implicit-equation residual and state error
against an independent double-precision reference, click-resistant steals and
filter changes, VCO switch isolation and smoothing, lone-VCO unity behavior,
cross-modulation with VCO II audio disabled, deferred HQ mode changes,
MIDI/state migration, and a 32-slot CPU guardrail.

The Release CPU fixture renders all 32 slots at a 96 kHz host rate with HQ On,
which correctly selects the native high-rate path. It warms the engine and uses
the best of three equal renders to reject scheduler interruptions. Every run
keeps a loose portable runaway ceiling, and the Ladder must remain below `2.5x`
the SEM baseline in the same process. Set
`MARS_STRICT_REALTIME_BENCHMARK=1` on stable local hardware to additionally
require at least five percent real-time headroom (`< 0.95x`). That strict wall-
clock target is deliberately opt-in because GitHub runner hardware and
contention are not pinned. Sanitizer builds retain a separate diagnostic ceiling
because their instrumentation is not a shipping-performance measurement.

Those engineering tests do not replace hardware validation. A stronger claim
for any complete named instrument would additionally require:

- a documented unit, revision, temperature, loading, and capture chain;
- waveform, harmonic-envelope, and alias-energy error across pitch and PWM;
- filter cutoff, Q, gain loss, self-oscillation, THD, IMD, and multitone sweeps;
- transient and modulation comparisons across the control space; and
- null/error analysis against captures or a validated small-timestep circuit
  reference.

Mars 1.2 therefore makes a deliberately testable statement: its saw contour and
Moog ladder derive from published measured/circuit models, its SEM mode law and
VCO routing follow documented hardware behavior, and every remaining boundary
is labeled rather than implied.
