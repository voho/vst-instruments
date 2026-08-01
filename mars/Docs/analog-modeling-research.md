# Mars analog-modeling research and implementation contract

Mars 1.5 is a white-box virtual-analog synthesizer with named reference targets,
not a black-box claim that one plug-in reproduces an entire vintage instrument.
This document separates circuit- or measurement-derived blocks from the parts
that remain stable, efficient Mars designs. These changes make Mars more
physically explicit and measurably cleaner; they do not by themselves establish
that it sounds better than another synthesizer. A superiority claim requires
level-matched blind listening against named competitors and calibrated captures
of identified hardware.

## Claims boundary

| Block | Reference target | What Mars 1.5 implements | Precise claim |
| --- | --- | --- | --- |
| `Moog-like VCO` saw | Minimoog Voyager recordings analyzed by Pekonen, Lazzarini, Timoney, Kleimola, and Valimaki | The paper's four-sample integrated third-order B-spline PolyBLEP source and its source-specific Table 4(b) pitch-dependent post-EQ, with the 44.1 kHz pole and zero bilinearly remapped to the active internal rate | The paper's best reported measured-spectrum configuration over the fitted 86 Hz-8.3 kHz range, a neutral blend below that range, and a clamped fit above it; not a capture-perfect or circuit-level Voyager VCO |
| `Juno-like DCO` timing and saw | Roland JUNO-106 service architecture: 8 MHz master, 1/2/4 MHz range clocks, 8253 interval timers, and MC5534A wave generator | A roughly 4.2 ms held-control scan, pending/active integer count with terminal-count reload, event-driven 12 V Miller ramp, finite reset/charge injection, held-threshold PWM comparator, asymmetric edge slew, divide-by-two sub, and prewarped output pole | Service-architecture-faithful, low-cost grey-box behavior with exact steady integer divisors; not a transistor netlist or serial-number capture of an MC5534A |
| Oscillator discontinuities | Integrated B-spline event correction described by Pekonen et al. | The same fractional-event, four-sample integrated third-order B-spline correction on saw reset, both pulse edges, moving-PWM comparator steps, the square driver integrated into triangle, and the derived DCO sub; independent correction state allows coincident events to add | Measured alias reduction against naive fixtures and continuous fractional event timing; not a guarantee of zero aliasing under every modulation or nonlinear downstream condition |
| Cross modulation | Oberheim-style separation of oscillator mixer and X-Mod routing | Oscillator II keeps running and frequency-modulates oscillator I even when oscillator II's audio feed is Off | Hardware-inspired routing with bounded digital FM; not a transistor-level oscillator interaction model |
| `Ladder` | Four-stage nonlinear Moog ladder model by D'Angelo and Valimaki | An independent bounded damped-Newton solution of the original implicit bilinear transistor equations, with residual-decreasing steps, a `tanh` differential pair in every stage, delay-free feedback, and `(1 + k)` DC-gain compensation | Circuit-derived generalized Moog ladder algorithm across the full 20 kHz control range; not a tolerance-by-tolerance model of one physical serial number |
| `SEM` | Oberheim/Sequential two-pole state-variable mode behavior | Nonlinear TPT state-variable core with the schematic's linear low-pass -> notch -> high-pass mode pot | SEM-inspired core and schematic-derived mode law; not a component-level SEM clone |
| VCA | Analogue VCA signal ordering | A bounded, normalized color transfer before amplifier-envelope, velocity, and group gain | Stable harmonic color as gain closes; not an OTA, BA662, or diode device-equation model and not control-dependent VCA distortion |
| Stereo ensemble | Panasonic MN3009 behavior plus Holters and Parker's Juno-60 support-filter analysis | Two variable-clock 256-stage paths, fractional capture/held-output integration, fifth-order published filters, +2.3 dB measured reference gain, O(1) clock-history charge loss, incomplete-transfer memory, two-phase cancellation-mismatch feedthrough, and deterministic event noise | A circuit-structured, datasheet-informed Juno BBD model with provisional loss/noise/feedthrough voicing; not a transistor/capacitor simulation or measured MN3009 lot |
| Optional BBD compander | Philips/NE570 full-wave detector and variable-gain-cell behavior | One linked compressor before the BBD and one linked stereo expander after its internal noise/feedthrough, using 4.7 ms detectors and a click-smoothed bypass | Explicitly non-Juno studio mode: JUNO-60/106 chorus schematics do not contain a compander |
| Voice cards | Vintage polyphonic component spread | Fixed seeded offsets plus persistent seeded slow and fast Ornstein-Uhlenbeck processes for each card | Deterministic, non-periodic component-like movement, not measured calibration, noise, aging, or temperature data from a hardware population |
| HQ island | Antialiasing for event sources, nonlinearities, and clocked effects | The complete voice, VCA, stereo BBD, and output-color path at 4x, 2x, or native rate, followed by staged 137-tap linear-phase half-band returns | A measured rate and return-filter contract; not proof that all in-band products are inaudible or that oversampling alone produces hardware equivalence |

## Implemented signal path

The authoritative implementation is `Source/DSP/MarsEngine.cpp`:

1. Oscillator I and II independently select `Moog-like VCO` or `Juno-like DCO`
   and generate saw, variable-width pulse, or triangle. Every waveform
   discontinuity uses the fractional-event, four-sample integrated B-spline
   correction: saw reset, both pulse edges, the triangle square driver, and the
   divide-by-two DCO sub. The LFO advances at the internal rate, and an
   unexpected comparator transition caused by moving PWM adds a boundary
   correction. Triangle is a bounded leaky integration of that
   corrected driver. The VCO saw additionally uses the source-matched measured
   contour. The DCO holds a range-clock integer divisor, reloads a pending
   8253-style count only at terminal count, and drives a 12 V resettable Miller
   ramp plus held-threshold PWM comparator and divider-locked sub. Finite reset,
   charge injection, asymmetric comparator slew, and a prewarped output pole add
   the low-order analogue behavior. A 2 ms model crossfade keeps automation
   continuous; at a settled VCO endpoint the MC5534A audio state is frozen.
   Waveform changes use a separate 3 ms crossfade, and at a settled waveform
   endpoint only the audible generator runs; the two silent ones are frozen and
   re-seeded analytically when a crossfade begins.
2. The two oscillator mixer feeds have independent On/Off parameters. With both
   On, `Balance` uses an equal-power law. With only one On, that oscillator has unity gain
   at every Balance position. A 4 ms gain smoother prevents a hard automation
   edge. Oscillator phases always advance, oscillator II remains a cross-modulation
   source, and sub/noise remain independent.
3. Active oscillator feeds, sub, and noise enter a fixed first-order antiderivative-
   antialiased (ADAA) mixer. `Filter drive` is then converted once into the
   voltage scale used by the filter; it no longer changes the mixer and VCA as
   unrelated extra gain stages. The mixer's noise generator emits one
   independent sample per internal step, so its amplitude follows the square
   root of the internal rate: a physical noise source has a fixed density in
   volts per root hertz, and a fixed generator amplitude would instead spread a
   fixed total power over whatever bandwidth the HQ setting happened to choose.
   Its 5.2 kHz colour pole uses the exact one-pole coefficient at the internal
   rate rather than the small-angle approximation. The calibration is referred
   to a 192 kHz internal rate, so the amplitude is at or below unity for every
   session rate up to 192 kHz; above that it exceeds unity and the ADAA
   saturator compresses it, costing 0.4 dB at 384 kHz and 1.4 dB at the
   engine's 768 kHz ceiling.
4. The selected `Ladder` or `SEM` algorithm processes the mixer signal. Its
   modulated cutoff is bounded by 20 kHz as well as by 0.45 of the host rate, so
   a bright patch driven to the top of its filter envelope keeps the same top
   octave at every session rate from 44.1 kHz up, rather than sitting at
   19.8 kHz at 44.1 kHz and above 40 kHz at 96 kHz. The
   ladder uses a bounded residual-decreasing Newton solve without an artificial
   feedback delay and compensates its resonance-dependent static bass loss. A
   3 ms crossfade protects live model changes; at a steady endpoint only the
   chosen algorithm runs.
5. The filtered signal enters a bounded, level-normalized VCA color transfer
   before amplifier-envelope, velocity, and group gain are multiplied. Voices
   are then panned and summed. This ordering retains the color transfer through
   a decay instead of making the signal progressively cleaner as gain closes.
6. The stereo bus enters two antiphase, variable-clock 256-stage BBD paths with
   rolling charge loss, transfer memory, event noise, clock cancellation
   residue, and their published Juno-60 fifth-order support filters. An optional
   explicitly non-Juno compander surrounds the wet device path. Ensemble mix,
   output gain, and the bounded output-color guard all remain inside the HQ island.
   Staged half-band FIRs return it to the host rate, after which a 1.5 Hz DC
   servo and finite-output guard complete the path.

The persisted, non-automatable `hqOversampling` quality setting is On by
default. At standard production rates it keeps the complete nonlinear voice
and BBD island at or above 176.4 kHz: 4x at 44.1/48 kHz, 2x at 88.2/96 kHz,
and native at 176.4 kHz and above. Off renders the sound path natively while a
bounded host-rate delay preserves the prepared session's latency contract. A
requested change is deferred until the engine is idle rather than resetting
held voices.

## Measured saw contour

Pekonen et al.,
[*Discrete-Time Modelling of the Moog Sawtooth Oscillator Waveform*](https://doi.org/10.1155/2011/785103),
fit separate first-order post-equalizers to several antialiased sources. Mars
implements their best-performing fourth-order B-spline BLEP configuration and
therefore uses the source-specific Table 4(b) coefficients:

```text
H(z) = g(f0) * (1 - b(f0) z^-1) / (1 - a(f0) z^-1)

g = 0.7105 + 3.380e-5 f0
b = 1.0161 - 5.850e-4 f0 + 5.220e-8 f0^2
a = 1.0294 - 4.8921e-4 f0 + 3.974e-8 f0^2
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

The source reset uses the four BLEP residual polynomials from the integrated
third-order B-spline (a fourth-order PolyBLEP). A fixed two-sample internal
delay lets its four correction samples span both sides of a fractional reset;
there is no lookup table or allocation. Pulse, triangle's square driver, DCO
saw, and the derived sub reuse this event-correction kernel with their own
states, but they do not reuse the VCO-saw-only post-EQ coefficients above.

## Clocked DCO model

Roland's
[*JUNO-106 Service Manual*](https://synthfool.com/docs/Roland/Juno_Series/Roland_Juno_106/roland_juno106_service_manual.pdf)
separates the DCO into an 8 MHz master/range divider, three programmable
interval timers, and the MC5534A analogue wave generator. The documented
16'/8'/4' range clocks are 1/2/4 MHz. The MC5534A block contains a resettable
Miller integrator, PWM comparator, and divide-by-two flip-flop; the pitch timer
is not inside the MC5534A. The [Intel 8253 datasheet](https://www.cpcwiki.eu/imgs/e/e3/8253.pdf)
defines held integer-count operation rather than cycle-by-cycle fractional
dithering.

Mars samples pitch and PWM controls at a roughly 4.2 ms cadence corresponding
to Roland's multiplexed control scan. A write calculates one pending integer
count from the selected range clock; the current count remains active until the
next terminal event, when the pending value latches. At 440 Hz and 2 MHz the
count is 4545 and the steady output is exactly `2 MHz / 4545`, not an alternation
between 4545 and 4546. The normal 16'/8'/4' positions retain the documented
1/2/4 MHz clocks. Mars's outer octave positions use explicit 500 kHz/8 MHz
range-divider extensions, and counts beyond 16 bits act as a virtual prescaler
only for MIDI notes below the original hardware envelope rather than silently
pitching them upward. The 8 MHz extension retains the documented 4 MHz analogue
reset-drive width so the full legal PWM range remains usable. Cross modulation in DCO mode is sampled by this control
scan and remains a Mars extension, not an MC5534A feature.

The timer controls reset timing; audio follows an event-driven analogue
surrogate. A held DCO current integrates a normalized 12 V Miller ramp between
terminal events. At reset, one range-clock period drives a finite switch model
with a restrained charge-injection residue. The actual reset magnitude and
fractional event time feed an independent four-sample B-spline correction. A
held PWM threshold drives its own corrected comparator path, followed by
different rise/fall slew constants. Saw, pulse, the Mars-specific triangle
extension, and the derived sub then pass through independent states of a
prewarped output pole. The constants are conservative service-architecture
values awaiting hardware fitting; they are not presented as internal MC5534A
device measurements.

Per-card VCO calibration and wander are removed from DCO timer frequency;
intentional unison detune, panel fine tune, pitch bend, LFO pitch modulation,
PWM/component skew, and output-stage variation remain. VCO and DCO states are
deterministic. VCO and DCO own independent phase/event state. Switching seeds
the destination phase, advances both endpoints at their own clock rates during
a 2 ms audio crossfade, and freezes the inactive endpoint once settled.

## Oscillator event correction and output stage

A naively sampled saw or pulse aliases because its discontinuities contain
energy above Nyquist. Mars schedules the same four-sample B-spline PolyBLEP at
the actual fractional time of every oscillator discontinuity: VCO and DCO saw
reset, the rising and falling pulse edges including narrow-pulse events that
land in the next cycle, both edges of the triangle square driver, and the
divide-by-two DCO sub. Multiple events add into independent correction queues.
The corrected square driver is then integrated into a bounded triangle.

Through version 1.5 all three waveform generators of both oscillator endpoints
ran continuously so that a waveform change had no stale state to resume from.
Version 1.6 freezes the two inaudible generators at a settled endpoint, exactly
as the inactive VCO/DCO analogue core already was, and re-seeds them at the
instant a crossfade begins - while the destination gain is still zero. The
four-sample causal B-spline path needs two samples of history plus its scheduled
residual and refills them from the current sample; the leaky-integrator triangle
and the DCO's slewed comparator are restarted from their closed-form steady
state at the current phase, which is the same seeding `initialiseVoice` uses at
note-on. A frozen generator also stops scheduling residuals, so its correction
accumulator cannot drift while it is silent. The regression suite checks that
the switch transient never exceeds the destination waveform's own slew, that the
settled output matches an engine whose generator ran continuously, and that a
detour and return reproduce the never-switched reference to within 0.01.

This follows the efficient event-correction family described by Stilson and
Smith's
[*Alias-Free Digital Synthesis of Classic Analog Waveforms*](https://quod.lib.umich.edu/i/icmc/bbp2372.1996.101/--alias-free-digital-synthesis-of-classic-analog-waveforms?rgn=main%3Bview%3Dfulltext)
and Valimaki and Huovilainen's
[*Antialiasing Oscillators in Subtractive Synthesis*](https://research.aalto.fi/en/publications/antialiasing-oscillators-in-subtractive-synthesis/).

The deterministic high-note regression compares non-harmonic DFT energy with
naive sources at the same phase increment. Its current reductions are 27.7 dB
for saw, 25.2 dB for pulse, and 19.3 dB for triangle. These values are objective
fixture results, not an assertion of zero aliasing, an audibility threshold, or
a comparison with another product.

Both primary oscillators then pass through the same bounded, level-normalized
output shaper. This gives pulse and triangle finite analogue-like edge/level
behavior but does not turn them into measured Minimoog, Juno, or Oberheim
waveform models. An exact named-hardware claim for those shapes would require
schematic-revision-specific device equations or calibrated captures across
pitch, pulse width, level, temperature, and clock history.

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

That publish ceiling is deliberately *not* the target the updates stop at.
Every transistor current in the residual is multiplied by `2VT * g`, so the
residual of the trivial candidate - the committed previous state, which is where
each solve starts - is about `2g * |v0 + u + k * v3|`. Stopping at a fixed
voltage therefore behaved as an amplitude dead zone of `tolerance / (2g)`: at a
low cutoff the starting guess already satisfied it, no update ran, and the
filter held its previous state instead of integrating. Measured at 48 kHz with
`Filter drive` at its default, a 100 Hz cutoff gated every mixer signal below
about -26 dBFS to silence and a 400 Hz cutoff gated below about -40 dBFS, and a
`k > 4` ring could not start from small state at all. The
updates instead target a fixed *relative* reduction of whatever residual the
step actually starts from. That asks for the same number of updates as the
previous fixed target did at programme level, and leaves the publish ceiling,
the rescue pass and the one-sample hold unchanged.

The floor beneath that relative target needs the same care, because a floor is
just a smaller version of the same bug. Any *constant* floor `f` re-creates a
dead zone of `f / (2g)`, which is still inversely proportional to cutoff; a
floor of `1e-3` of the publish ceiling merely moved the knee down by three
decades, which still put it at about 24 uV at the 20 Hz bottom of the `Cutoff`
control - measurable inside the shipped range, where the passband gain at a
50 uV drive read 0.33 of its 50 mV reference. The floor is therefore a *scale*:
the residual is a voltage assembled from the previous state, the input, and
`2VT * g`, so it is floored at a few ulp of the largest of those. Because the
`2VT * g` term dominates at small signals, the resulting dead zone collapses to
about `8 eps * 2VT`, or 49 nV, which is cutoff-*independent* by construction.
Measured by bisection at `k = 1.2` and 192 kHz, the drive at which passband gain
falls to half of reference is 128 nV at a 20 Hz cutoff and 61 nV at 20 kHz - a
2.1x spread over a 1000x cutoff range, against the 1000x spread of a constant
floor, and 136 dB below the filter's own +/-0.8 V input clamp. That is the
honest bound: the dependence is not removed to zero, it is pushed below the
point where a float residual carries information at all.

The regression suite pins the ladder's passband gain to within 5% over a 60 dB
input range at 20 Hz, 40 Hz, 100 Hz, 200 Hz, 500 Hz, 2 kHz, and 20 kHz cutoffs -
deliberately including both ends of the control, since the middle of the range
is exactly where a constant floor stays invisible - and pins voice output
proportional to a quiet mixer feed down to 3% of full level. The deliberate
5.2 uV per-stage silence guard that snaps a genuinely idle filter to rest is
unchanged.

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

For four stages, normalized resonance maps to feedback
`k = 4r * (1 + 0.055 * r^16)`. The linearized four-pole cascade loses stability
at exactly `k = 4`, which is where a real Moog ladder starts to sing; the plain
`k = 4r` map used through version 1.5 topped out at `3.98` and was clamped there
internally, so the model could ring but never oscillate. The sixteenth-order
lift crosses the threshold only in the last few percent of the control: the
extra term is `6e-10` relative at the `0.28` default and about 1% at
resonance `0.9`, reaching `k = 4.18` at the top of the knob. The differential
pairs limit the loop gain as the ring grows, so the result is a bounded limit
cycle rather than divergence, and the residual ceiling below still refuses to
publish an out-of-contract state. A free-running fixture at a 1 kHz cutoff and
192 kHz processing rate measures no sustained oscillation at `k = 3.98`
(zero tail zero-crossings) and a 975 Hz limit cycle at `k = 4.18`, that is,
within 2.5% of the cutoff, with the tail bounded well inside the output guard.
A second fixture requires that limit cycle to build itself out of a millivolt
disturbance at 200 Hz, 1 kHz, and 4 kHz cutoffs, which is what a circuit above
its Hopf threshold does: before the relative convergence target above, the ring
only existed when it was kicked hard, and below roughly a 500 Hz cutoff it never
started at all.

That fixture stops at 200 Hz for a reason worth stating rather than hiding. The
excitation still has to leave the ladder above the deliberate 5.2 uV per-stage
idle-silence guard, and the state a fixed-length kick deposits is proportional
to `g`: at a 20 Hz cutoff a millivolt, eight-sample kick lands under the guard,
which snaps the filter to rest before the ring can build, and no length of
render recovers it. This is a property of the silence guard, not of the solve,
and it is not reachable inside a voice - the amplifier envelope's own opening
transient is orders of magnitude larger, and a full-engine render at maximum
resonance with every mixer source muted sustains 0.273 RMS at a 20 Hz cutoff
and 0.277 at 2 kHz.

Mars applies
the paper's resonance-dependent cutoff correction and bilinear prewarping, and
Panel Drive maps once into the physical voltage domain based on a 26 mV
transistor thermal voltage. Every stage evaluates the differential-pair
nonlinearity; `(1 + k)` DC-gain compensation restores the otherwise severe
static low-band loss as resonance rises without removing nonlinear resonance or
self-oscillation. The state is reset on any non-finite result and the engine
retains its final bounded-output guard.

The solver's arithmetic was reorganized in version 1.6 without changing the
equations it solves: the terms that depend only on the committed previous state
are folded into four per-sample offsets, node voltages are scaled by the exact
reciprocal of `2VT` instead of dividing, and the per-candidate finiteness sweep
was replaced by a NaN-safe transfer table, so a diverging trial vector still
poisons its own residual and is rejected. The measured equation residual moved
from `3.080e-4 * (2VT)` to `3.089e-4 * (2VT)`, both far inside the documented
`6e-4` ceiling.

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
nonlinearity, continuous cutoff and resonance, and finite state.

Version 1.6 adds a saturating resonance loop. The zero-delay solve itself is
untouched and still closed-form; what changed is that the band-pass integrator
state is bounded by the same smooth transfer used elsewhere in the voice, with
four units of headroom. That is where the circuit actually runs out of room: the
SEM closes its resonance loop through an OTA that leaves its linear region long
before the integrator capacitors do. Loop gain therefore falls as the resonant
ring grows. Driving the core at its own cutoff with resonance `0.99` measures a
resonant peak gain of 12.5x for a small signal, 3.0x at an input amplitude of
0.5, and 1.3x at 2.0, and the state cannot leave `+/- 4.8` by construction, so
the filter cannot latch. The practical effect is a compressing, self-limiting
resonance with odd-order colour instead of an ideal linear resonator whose peak
was previously left for the VCA and the output guard to contain. At the `0.28`
default resonance the compression is about 2% of state amplitude, and the
default filter model is `Ladder`, so no shipped default sound moves audibly.

This still does not include a selected
SEM revision's transistor/OTA device equations or capture-derived error curves;
the UI and documentation therefore say `SEM`-inspired rather than
circuit-accurate SEM.

## VCA ordering and boundary

Mars colors the selected filter output first, using a bounded and
level-normalized transfer, and only then multiplies amplifier-envelope,
velocity, and voice-group gain. The practical distinction is `g * f(x)`, not
`f(g * x)`: the waveshaper's harmonic balance no longer disappears as an
envelope closes. Per-card drive error introduces a small fixed color variation,
while the envelope and velocity remain smooth control signals.

This is a topology and gain-staging correction, not a component-level VCA. Mars
does not solve a BA662, OTA, or diode bridge, and its distortion does not move
with control-gain operating point as a physical device can. Pines's
[*Real-Time Virtual Analog Modelling of Diode-Based VCAs*](https://www.corianderpines.org/publications/dafx2025_diode_vca/)
is an example of a newer control-dependent device-modeling direction; that
model is not implemented here. The current block is intentionally documented
as bounded VCA color rather than a named-hardware VCA emulation.

## Variable-clock BBD ensemble

The ensemble follows Holters and Parker,
[*A Combined Model for a Bucket Brigade Device and its Input and Output
Filters*](https://dafx.de/paper-archive/2018/papers/DAFx2018_paper_12.pdf).
Each stereo side represents a two-phase, 256-stage BBD as 128 signal-bearing
stage pairs. A clock event shifts the held sample rather than reading a
fractional-delay pointer, so the nominal delay follows `N / (2 fClock)`. An
antiphase triangle varies the two clocks around 50 kHz by 24 kHz, producing a
26-74 kHz range and complementary pitch motion.

Clock events are resolved at fractional positions within each audio interval.
Each bucket captures a linearly interpolated, support-filtered input at its
event time, and the output filter receives the time-weighted mean of the
piecewise-held, colored output. This is especially important with HQ Off, where
more than one BBD event can occur in a host sample; those events no longer all
capture the same quantized input value.

Both lines use the paper's Juno-60 fifth-order input- and output-filter poles
and residues from Table 1. Mars maps the analogue partial fractions to the
active internal rate with an impulse-invariant parallel form, normalizes DC
gain, and gives the two lines small fixed frequency offsets. The held BBD sample
passes through a restrained bias-asymmetric transfer and the published output
network. A provisionally fitted internal charge-loss curve is compensated so the complete
small-signal line still measures the reported +2.3 dB at 50 kHz rather than
double-counting attenuation.

The [Panasonic BBD catalog](https://www.ka-electronics.com/images/pdf/Panasonic_BBD.pdf)
documents the MN3009's 256 stages, dual outputs with clock-component
cancellation, insertion behavior, noise, distortion, and clock range. Mars
stores one log-efficiency per transfer event and a rolling 128-event sum, so a
packet's complete modulated-clock charge history costs O(1) rather than 256
device updates. A small BBD-rate pole represents incomplete transfer. One
independently seeded TPDF contribution is generated per full transfer; for
linear uncorrelated stage noise this is the appropriate output statistic
without 256 random draws.

Both complementary half-clock boundaries schedule opposing, fractional cubic
impulses before the measured output filter. This represents residual clock
feedthrough from imperfect dual-output cancellation rather than adding an
arbitrary sine. Its gain smoothly reaches zero before a low native Nyquist
frequency can fold the ultrasonic clock; HQ mode represents the complete
26-74 kHz range internally. Noise and feedthrough open quickly with signal,
remain through the BBD tail, then fade and snap to exact zero so plug-in silence
and host suspension remain deterministic.

The JUNO-60/106 chorus schematics do not contain a compressor/expander, so the
authentic default remains uncompanded. The optional `chorusCompander` setting
uses an [NE570-style](https://www.onsemi.com/pdf/datasheet/ne570-d.pdf)
full-wave 4.7 ms detector, 2:1 compressor before the wet split, and linked
stereo expander after device loss/noise/feedthrough. The expander reference is
matched to the line's nominal +2.3 dB gain so enabling COMP does not square that
gain. A 20 ms blend protects automation. It is explicitly labeled non-Juno
instead of being used to mask an incorrect target.

The implementation still does not claim measured stochastic clock jitter, a
specific LFO/driver circuit, temperature behavior, aging, or a particular
MN3009 lot. Gabrielli, D'Angelo, and Squartini's newer
[*Antialiasing in BBD Chips Using BLEP*](https://dafx.de/paper-archive/2025/DAFx25_paper_29.pdf)
describes the event-correction family used for the clock-feedthrough refinement.

## HQ return path, smoothing, and deterministic variation

HQ oversampling is a persisted, non-automatable quality setting and defaults to
On. It runs the complete nonlinear voice, VCA, BBD ensemble, and output-color
island at 4x for 44.1/48 kHz hosts, 2x for 88.2/96 kHz hosts, and natively at
176.4 kHz and above. Off uses native DSP everywhere. Requested changes wait
until the engine is idle before rebuilding rate-dependent state, so an active
note is not reset or retuned mid-hit.

Every 2:1 return uses the same 137-tap equiripple linear-phase half-band FIR.
It has exact alternating zero taps, a 0.5 center tap, and 34 stored symmetric
side coefficients. The regression contract measures less than 0.0002 dB ripple
through 0.455 of the input Nyquist, more than 100 dB rejection from 0.545 of
Nyquist, and -6.0206 dB at the half-band midpoint. One stage reports 34 host
samples of latency at 2x. The two-stage 4x cascade reports 51 host samples; the
even decimation phase makes both delays exact integer host samples. Host latency
is fixed for the prepared rate so deferred HQ changes never notify the host
from the audio callback. When HQ is Off below 176.4 kHz, a fixed-size native
delay aligns the path to 51 or 34 samples; at high native rates the contract is
zero. A 1.5 Hz output DC servo follows rate conversion and preserves bottom-
octave and sub-octave fundamentals. The suite exercises rates through 384 kHz;
the engine guards API input up to 768 kHz.

Continuous host controls use a 14 ms one-pole smoother. Strictly positive,
perceptual controls use multiplicative log-domain interpolation: cutoff, all
attack/decay/release times, LFO rate, and ensemble rate. This avoids sweeping
the low end too quickly when automation or the preset randomizer makes a large
normalized move. Other continuous sound controls interpolate linearly;
pitch-bend and mod-wheel performance input use 5 ms, and oscillator mixer gates
use 4 ms. Discrete oscillator models, waveforms, and filter models use their own
short crossfades rather than interpolating enum values.

Mars owns 16 fixed render voices, with a user ceiling from 1 to 16 that is
enforced synchronously even when automated downward. Each slot owns a
deterministic voice card. Alongside fixed offsets, each card carries slow and
fast seeded Ornstein-Uhlenbeck states with approximately 2.8 s and 75 ms time
constants, mixed 84/16 and bounded before use. One global control clock advances
all 16 cards through notes and silence. The state persists across note
allocation, so drift is non-periodic but does not restart with each key. VCO
pitch and filter movement receive the full scaled process; the clocked DCO
timer rejects per-card oscillator calibration and wander while retaining
analogue-path and PWM variation. Identical initial state, MIDI, and automation
still produce identical audio. `Drift` is not captured component statistics,
injected hiss, simulated wear, or a newly randomized saved sound on each
playback.

## Arpeggiator timebase

Mars's arpeggiator is deliberately a free-running rate control in steps per
second rather than a host-tempo division. That is what the JUNO-6, JUNO-60, and
JUNO-106 arpeggiators are: a rate potentiometer plus a mode and range switch,
with synchronisation available only through an external clock input. Mars
therefore takes no playhead information at all, which also keeps the whole
feature inside the JUCE-free engine where it is unit-tested. Steps are played by
calling the ordinary allocator, so Poly, Unison, Fifth, and Mono keep their
existing group sizes, stealing rules, and 2 ms retrigger tail; the arpeggiator
adds note timing, not a second voice engine.

## Why the runtime remains analytic

A neural emulator learns a target dataset; its architecture alone is not
evidence of hardware accuracy. Mars has no licensed, revision-specific capture
set covering oscillator wave, pitch, PWM, cutoff, resonance, drive, modulation,
temperature, and state history. A generic network would therefore add opaque
behavior rather than verified fidelity. The present analytic blocks are
auditable, deterministic, allocation-free in the audio path, and practical
across 16 render voices.

Neural or differentiable methods remain useful for offline fitting once a
target and calibration chain exist. Any future runtime surrogate must beat the
analytic block in capture error while also passing aliasing, interpolation,
state-stability, latency, and CPU tests.

## Validation boundary

Current automated tests establish finite bounded output, pitch and envelope
timing across sample rates, deterministic rendering, distinct VCO/DCO output
for saw, pulse, and triangle, independent model routing for oscillator I and II,
the source-matched B-spline VCO saw signature, held DCO divisors across the
1/2/4 MHz ranges with no adjacent-count alternation, reload-residue timing,
analogue-ramp-driven PWM decisions, independent endpoint freezing,
derived-sub-divider behavior, model-switch continuity,
deep-note and long-triangle stability, and the explicit saw/pulse/triangle alias
reductions reported above. They also cover the 137-tap half-band coefficient
structure, passband/stopband contract, 4x/2x/native topology and reported
latency, finite variable-clock BBD response, the 256-stage `N/(2 fClock)`
impulse-delay relationship, small-signal +2.3 dB gain, and distinct captures
for multiple fractional events inside one host sample. BBD coverage additionally
checks clock-dependent charge retention, exact activity-gated silence,
deterministic independent line noise/feedthrough, and the optional compander's
2:1 law and nominal-BBD-gain-matched steady-level reconstruction. Oscillator coverage also
checks that a moving PWM comparator schedules an antialiasing residual. Filter coverage includes full-range ladder
stability, bass-gain compensation, implicit-equation residual and state error
against an independent double-precision reference, level-independent ladder
passband gain across a 60 dB input range at seven cutoffs spanning the whole
20 Hz - 20 kHz control range, a limit cycle that
starts itself from a millivolt disturbance at every cutoff, and distinct
Ladder/SEM responses. Sample-rate coverage additionally pins the mixer noise
density across 44.1-192 kHz with HQ both on and off, a bright patch's
top-octave spectrum across the same range, and the margin by which HQ lowers a
hot patch's worst audible-band inharmonic product below the native path's.
Integration tests cover click-resistant steals and model changes,
oscillator switch isolation and smoothing, lone-oscillator unity behavior,
cross-modulation with oscillator II audio disabled, deferred HQ changes,
dynamic physical-voice budgets, Mono mode-transition/duplicate-hold/last-note/
legato/fallback/retrigger semantics, preset-randomizer safety, MIDI/state
migration, and a 16-voice CPU guardrail.

The Release CPU fixture renders both oscillators through the MC5534A endpoint
across all 16 voices at a 96 kHz host rate with HQ On,
which correctly selects the 2x, 192 kHz internal path. It warms the engine and uses
the best of three equal renders to reject scheduler interruptions. Every run
keeps a loose portable runaway ceiling, and the Ladder must remain below `2.5x`
the SEM baseline in the same process. Set
`MARS_STRICT_REALTIME_BENCHMARK=1` on stable local hardware to additionally
require at least five percent real-time headroom (`< 0.95x`). That strict wall-
clock target is deliberately opt-in because GitHub runner hardware and
contention are not pinned. Sanitizer builds retain a separate diagnostic ceiling
because their instrumentation is not a shipping-performance measurement.

Those engineering tests do not replace hardware validation. A stronger claim
for any complete named instrument, or a claim that Mars sounds better than a
competitor, would additionally require:

- a documented unit, revision, temperature, loading, and capture chain;
- waveform, harmonic-envelope, and alias-energy error across pitch and PWM;
- filter cutoff, Q, gain loss, self-oscillation, THD, IMD, and multitone sweeps;
- transient and modulation comparisons across the control space; and
- null/error analysis against captures or a validated small-timestep circuit
  reference;
- level-matched, loudness-controlled ABX or similarly blinded listening against
  named products and hardware, with enough listeners and trials to report
  uncertainty rather than anecdotes.

Mars 1.5 therefore makes a deliberately testable statement: its VCO saw contour
and Moog ladder derive from published measured/circuit models; its DCO follows
the documented JUNO-106 timer/MC5534A block architecture with a compact analogue
surrogate rather than a transistor-netlist or capture claim; its stereo BBD uses
published circuit-derived Juno-60 support filters, reported device gain, and
datasheet-informed but provisionally voiced charge/feedthrough/noise while
stopping short of a component-level chip clone; its SEM mode law and oscillator routing
follow documented hardware behavior; and every remaining boundary is labeled
rather than implied. The result is a stronger engineering baseline for
listening tests, not a substitute for them.
