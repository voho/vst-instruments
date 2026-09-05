# Modeling research and next experiments — 2026-09-05

YouKnow already uses substantial circuit modeling. Its main fidelity constraint
is now identifying the remaining physical parameters from appropriate hardware
recordings. A different solver or a neural network is useful only when it
reduces a demonstrated error. Neither the present evidence nor this research
establishes market leadership.

## Current state

The engine contains a nonlinear four-stage transconductor VCF with a qualified
RK4 path, schematic-derived passive networks and converter holds, firmware
behavior, and service-point filter calibration. The chorus already implements
256-stage MN3009 lines, physical clock-edge sampling, full-period output holds,
polyBLEP reconstruction, and a coupled state-space reconstruction network.
Its support transitions are prepared outside the audio callback. These are
appropriate modern techniques, not a generic oscillator/filter/chorus sketch.
The domain audits separate numerical error from physical-model assumptions.

The [hardware report](hardware-validation.md) binds an identified Juno-106
recording to its MIDI and PCM hashes. That instrument has replacement voice
cards and original oscillator chips. Its results therefore establish behavior
of that serviced unit, with useful source and control evidence, while leaving
original 80017A transfer and population variation open. The same report records
the measured discrepancies and current benchmark commands; it is the source of
numerical hardware verdicts. A software reference is useful for comparative
testing but cannot calibrate hardware behavior by itself.

The most consequential open chorus parameters remain installed-unit delay
endpoints, absolute wet gain, hiss spectrum/level/correlation, and the physical
both-button result. Published part limits and a reconstruction's measurements
cannot establish all four. The
[Owner's Manual](https://cdn.roland.com/assets/media/pdf/JUNO-106_OM.pdf)
does not specify a simultaneous I+II state. Preserve the existing explicit
product policy until an identified-unit recording resolves it.

## Recent methods and their actual relevance

These sources were opened during this review, including the DAFx26 papers
published for September 1–4, 2026. Applications to YouKnow below are proposals,
not results claimed by those authors or implementations shipped here.

| Method and primary source | What the work establishes | Application and acceptance condition |
| --- | --- | --- |
| [Differentiable white-box modeling, Esqueda, Kuznetsov & Parker, 2021](https://dafx.de/paper-archive/2021/proceedings/papers/DAFx20in21_paper_39.pdf) | Circuit parameters can be optimized through differentiable circuit simulation against measured input/output audio. | First choice for fitting uncertain component values after capture gain, alignment and actual patch bytes are established. Preserve schematic topology and bounds; validate unseen settings and takes before updating a nominal value. |
| [Differentiable all-pole filters, Yu et al., 2024](https://diffapf.github.io/web/) | Sample-wise gradients through time-varying recursive filters avoid the mismatch of frame-based approximations. The synth example uses a simplified biquad and hardware-clone samples. | Useful for an offline envelope/filter calibration tool. It does not justify replacing the 106's four-pole nonlinear circuit with a biquad or establish original-unit accuracy. |
| [Training nonlinear multi-port elements inside WDF simulation, Massi et al., 2025](https://dafx.de/paper-archive/2025/DAFx25_paper_69.pdf) | Nonlinear element models can be trained inside a wave digital circuit using its input/output voltages. The example is a Big Muff input-stage transistor. | Consider a compact learned residual for an inadequately characterized OTA or follower only after a reproducible, level-dependent residual survives the physical model. Keep connectivity, DC behavior and controllable parameters explicit; compare with a simpler fitted transfer first. |
| [BBD antialiasing using BLEP, Gabrielli, D'Angelo & Squartini, 2025](https://dafx25.dii.univpm.it/wp-content/uploads/2025/09/DAFx25_paper_29.pdf) | Distinguishes BBD-generated aliasing from additional simulation-generated aliasing; BLEP targets the latter. | Already reflected in the engine and its BGA/SGA audits. More aggressive suppression is not automatically more faithful: retain the aliases caused by sampling at the physical BBD clock. |
| [Interpolation filters for ADAA, Zheleznov & Bilbao, 2024](https://dafx.de/paper-archive/2024/papers/DAFx24_paper_33.pdf) | Cubic interpolation improves some memoryless cases; its stateful examples show restrictions and weak or adverse gains. | Prototype ADAA only around an isolated nonlinear stage with measurable host-rate aliasing. Do not insert its delay or compensation filter into VCF feedback without requalifying modulation, resonance and oscillation amplitude. Do not remove physical BBD folding with an antialiasing wrapper around clock writes. |
| [Performance-oriented wave digital circuit emulation, Chowdhury & Rau, 2026](https://dafx26.mit.edu/assets/papers/DAFx26_paper_23.pdf) | Static code generation reduces persistent state, abstraction overhead and memory traffic in WDF implementations. | Useful if a future transistor-network model warrants WDFs; compare generated C++ with the existing small state-space kernels. A framework rewrite supplies no evidence of better sound. The paper's compiler is written in Jai, which its footnote says is not publicly available; reproducible builds need an accessible toolchain or checked generated code. |
| [Time-varying VA filter stability, McClellan, 2026](https://dafx26.mit.edu/assets/papers/DAFx26_paper_28.pdf) | Common quadratic Lyapunov functions provide stability results for specific trapezoidally discretized linear time-varying systems; instantaneous stable poles alone are insufficient. | Add a certificate or adversarial modulation oracle for a candidate linearized filter. These theorems do not certify YouKnow's nonlinear RK4 OTA path. Preserve its existing nonlinear and dynamic tests, including intentional self-oscillation. |
| [Residual-driven adaptive multirate QP, Zea & Rivera, 2026](https://dafx26.mit.edu/assets/papers/DAFx26_paper_31.pdf) | A nonlinear DAE residual guides adaptive steps; pseudoinverse and equality-constrained QP variants agree in tested examples against SPICE. The oscillator example reaches roughly 128× local oversampling. | Promising independent offline oracle for switched/transistor networks. A shipping adaptive solver requires bounded work, preserved state, and callback deadline measurements; the paper leaves systematic latency benchmarking and broader musical tests open. Do not replace the qualified VCF merely on average CPU results. |
| [Neural audio under DAW contention, Balasubramaniam et al., 2026](https://dafx26.mit.edu/assets/papers/DAFx26_paper_16.pdf) | Isolated inference rankings can change under competing plug-ins; callback tail latency and deadline violations matter. Its single-M3, mono/virtual-driver study favors BNNSGraph for convolutional models. | If a neural residual becomes justified, benchmark a complete stereo synth instance at small buffers under real DAW load. The study does not establish the best backend on every Mac or favor neural processing over this engine. |

## Ranked work

1. **Resolve the largest measured hardware residuals.** Reuse the exact SysEx
   and note sequence in the hardware benchmark. Keep source-level ratios,
   spectra and control-byte responses separate. First test documented signal
   routing and gain laws, especially NOISE and pulse width; establish the
   recording's usable control range before fitting anything. Repeat the same
   experiment on an original-card unit before treating a replacement card's
   response as universal. A nominal change needs an independent test segment
   and a reason its fitted coordinate is identifiable.
2. **Capture the actual chorus.** Record both outputs, modes Off/I/II and the
   physical both-button combination, with exact patch bytes, the same interface
   gain, no effects, and warm-up duration logged. Use stable excitation and
   silence, repeated across at least eight modulation cycles. Measure LFO
   period, each wet delay trajectory, stereo correlation, frequency-dependent
   wet level and noise PSD separately. Repeated original-unit captures are
   needed to distinguish component spread from an incorrect nominal model.
3. **Fit physical parameters with uncertainty.** Use a small bounded optimizer
   before adding an autodiff dependency. Start with the documented uncertain
   gains or a passive network's component values. Adopt differentiable
   simulation only when correlated parameters and data volume make it useful.
   Reject fits that trade capture EQ, recorder gain or oscillator duty against
   unrelated circuit parameters, or land systematically on the bounds.
4. **Qualify the remaining switching circuits.** Derive a bidirectional nodal
   reference for the mute-drive circuit, include a declared transistor model,
   and compare long and interrupted button intervals. Measure C16/C13 and the
   resulting return switch where a technician can obtain those nodes. Apply
   the same state-preservation discipline to parameter changes and quality
   transitions. Use the new adaptive DAE method as an optional independent
   oracle if ordinary SPICE/RK integration is insufficient.
5. **Optimize only qualified domains.** Profile six-voice, noise-rich,
   self-oscillating and chorus cases. Trial generated WDF kernels, limited
   local substeps or ADAA where those profiles show a real cost/error tradeoff.
   Require the existing numerical error gates, state continuity and worst
   callback timing to hold. Keep real BBD aliases and measured noise intact.
6. **Establish a comparative claim.** Run the same held-out scores and patch
   translations through currently available competitors and several identified
   units. Publish per-mechanism results and listening trials with level
   matching. A win against one recording or one preset cannot establish
   "most faithful on the market."

## Proposed identification and benchmark protocol

The combination proposed here is constrained circuit identification followed
by independent physical and numerical validation. It is an application of
the cited methods, not a claim to have invented a new modeling algorithm.

For each recording, archive its hash, serial number, original/replacement
parts, service history, temperature/warm-up, output selector, interface and
gain, MIDI/SysEx, sample rate and channel mapping. Capture several repeats.
Normalize only nuisance parameters supported by the experiment: one constant
gain per capture chain, an initial alignment, and separately reported pitch
offset. Do not use per-patch EQ or time warping that can hide a model error.
Absolute voltage/noise claims require an actual voltage reference; dBFS and an
unknown gain knob do not provide one.

For steady sources use AC RMS ratios, pitch-normalized harmonic magnitude,
duty cycle and noise PSD. For envelopes use onset latency, attack/decay/release
time and sustain versus control byte. For the VCF use low-drive gain/phase,
high-drive harmonics and intermodulation, resonance onset, oscillation level,
and rapid cutoff changes. For chorus report physical delay and modulation
separately from the supporting filter response and noise. Free-running phase
and stochastic noise make unrestricted waveform subtraction an unsuitable
sole metric; use repeat variability to set the comparison floor.

Fit on one subset of levels, notes and control settings; reserve other
settings and complete takes for validation. Validate unit-specific parameters
on repeated takes from that same unit and population claims on separate
units. Keep high-rate circuit-oracle results separate from hardware results:
a numerical solver can agree perfectly with the wrong physical model.

## Chorus correction from this review

The fast bypass path previously advanced the chorus LFO while freezing C16 and
C13 after the wet return became numerically zero. Those capacitors remain
connected to their drive network in the
[jack-board schematic, p. 15](https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=15).
The two paths now share the existing mute-drive update and continue charging
while the BBD audio calculation is skipped. No new component, junction
voltage, JFET transfer or noise term is introduced.

The new `YouKnow.ChorusBypass` regression compares the continuously processed
reference with fast bypass after a one-second Off interval and interrupted
charging/discharging intervals, at 48 and 192 kHz with the mute-drive model
enabled and disabled. It emulates the plug-in's denormal flushing only for the
wet gain so the optimization is exercised in a portable standalone test.
Against the previous implementation at 48 kHz, the return opens at 112.833 ms
instead of the reference's 114.750 ms: 1.917 ms early. With the correction,
the switch state matches on every tested sample. This is a control-state
consistency result, not a measured original-unit switching time.

The nominal mute equations remain an approximation: R48 loads the C16/R50
node bidirectionally, and Tr4's base current/junction behavior affects C13's
resting charge. The current cascade and 0.6 V threshold should therefore not
be advertised as an exact transistor-level solve. The existing 5 ms JFET glide
remains declared product policy. A coupled candidate belongs in the independent
oracle/capture work above; changing several uncertain transistor assumptions
at once would obscure the confirmed state-continuity fix.
