# VCF `tanh` optimization research note

Status: `Exact` remains the reference and default. `Hermite512` is the first
audition candidate. Native Apple M1 Max whole-engine timing and the 48 kHz / 4x
null renders are recorded below; cross-platform ranking and the player's blind
hearing judgment remain pending. No result is inferred from library papers.

## Scope and current call count

Huovilainen derives a virtual-analogue ladder with embedded `tanh`
nonlinearities, while Zavalishin discusses nonlinear feedback and numerical
realizations in virtual-analogue filters. They motivate treating the transfer
shape as part of the model, but neither source benchmarks or proves an
approximation for this engine. YouKnow106 advances its four OTA capacitor
states with two fixed five-evaluation Merson half-steps; it is an explicit ODE
solver, not a zero-delay algebraic solve.

The following counts are derived from the current source, not from a profiler:

- One right-hand-side evaluation calls the selected nonlinearity once for the
  resonance return and four times for the stage currents: **5 calls**.
- With the Early-effect path enabled and calibration above zero, it makes four
  additional calls: **9 calls**.
- Ten right-hand-side evaluations therefore make **50 or 90 calls per VCF
  interval per voice card**.
- The six continuously powered hardware cards advance even behind closed VCAs:
  **300 or 540 calls per internal frame**. Each active extension voice adds
  another 50 or 90. Per host frame, multiply by the applied 1x/2x/4x factor.

This establishes leverage, not cost: the share of CPU consumed by those calls
must be measured in the complete engine.

## Candidate comparison

| Candidate | Numerical and implementation shape | Current conclusion |
|---|---|---|
| Exact double libm | Existing `std::tanh(double)` with platform-defined implementation and standard special-value behavior. | Reference/default. Its native Apple M1 Max project cost is measured below; other architectures remain pending. |
| Float libm | Convert the argument to `float`, evaluate `std::tanh(float)`, and widen the result. | Smallest alternative and a useful audition control. Accuracy, monotonicity after rounding, and speed on each libc/architecture must be measured. |
| Bounded rational/Padé | Odd numerator/denominator evaluated with Horner/FMA, an exact unit slope at zero, and an explicit bounded saturation tail. | Promising scalar follow-up. A raw Padé form is unacceptable if it overshoots or grows outside its fit interval. Coefficients and the rounded evaluation order need independent dense and interval validation. [Sollya `fpminimax`](https://sollya.org/sollya-8.0/help.php?name=fpminimax) is suitable for constrained floating-point polynomial work, but does not by itself prove boundedness or monotonicity of a final rational function. |
| Hermite512 | 512 uniform positive-domain intervals over `[0, 19]`; four double coefficients per interval (16 KiB), exact-mode node values, analytic slopes `1-y²` flattened where adjacent rounded nodes coincide, cubic Horner evaluation, sign reconstruction, and explicit saturation. | First implemented audition candidate. It removes per-sample libm calls without changing the solver or adding a dependency. It wins the native Apple M1 Max whole-engine benchmark below; audibility and other architectures remain pending. |
| Local SIMD | After the scalar feedback return, the four stage-current arguments can be evaluated together; the four optional Early arguments form another batch. Arm's current AdvSIMD kernels use two double or four float lanes, range-reduced `expm1`, division, and explicit saturation. SLEEF supplies portable vector libm machinery for AArch64 AdvSIMD and x86 AVX2/AVX-512. | Best quality-preserving structural candidate after scalar measurements. On AArch64 the four double stages require two vectors; on AVX2 they fit one. Call, packing, masking, and saturated-lane costs are project measurements still pending. |
| Cross-voice SIMD | Restructure voices into lane-friendly state/control batches and evaluate corresponding nonlinearities across cards. | Potentially fills wider vectors, but requires a larger SoA/scheduler rewrite and careful handling of inactive extension voices. Defer until local SIMD and scalar candidates are measured. |

The Arm sources are evidence for a current high-quality vector construction,
not a speed prediction for YouKnow106: [double AdvSIMD
`tanh`](https://github.com/ARM-software/optimized-routines/blob/master/math/aarch64/advsimd/tanh.c)
and [float AdvSIMD
`tanhf`](https://github.com/ARM-software/optimized-routines/blob/master/math/aarch64/advsimd/tanhf.c).
SLEEF documents its low-branch, FMA/mask-based vector approach in the
[paper](https://arxiv.org/abs/2001.09258) and lists supported targets in the
[official repository](https://github.com/shibatch/sleef).

## Why Hermite512 is first

Hermite512 is the narrowest experiment that answers the product question. It
is a scalar drop-in at the existing nonlinearity seam, keeps `Exact` available
in the same binary, needs no third-party runtime or architecture dispatch, and
does not rearrange voice or integrator state. Building the table from exact
values and analytic slopes preserves the intended local shape more directly
than a deliberately coarse waveshaper; slopes are zeroed only where adjacent
nodes already round equal. Magnitude lookup plus `copysign` provides exact odd
construction, while the `|x| >= 19` branch supplies a hard finite bound.

The focused test contract scans one million positive-domain points, checks
oddness, monotonicity, bounds, table nodes, signed zero, NaN, infinities, and
rejects a dense-grid absolute error above `2.1e-8`. Next-representable values
on both sides of every table boundary are checked separately, as is the local
unit slope near zero. Adjacent exact nodes that round to the same double become
an explicit flat plateau, with their shared slopes zeroed, instead of retaining
sub-ULP analytic slopes. The
observed dense maximum is `2.017e-8` at `x=0.426759`. These are sampled tests,
not a formal proof between samples and not a hearing result. The choice of 512
intervals is not declared optimal across platforms until the shipping
architectures have both been measured.

## Numerical and runtime admission requirements

These are engineering gates for this feedback system, not a transferred
stability theorem from another filter topology:

- finite inputs produce finite outputs within `[-1, 1]`;
- the function is odd, preserves signed zero, and is nondecreasing;
- value and derivative are continuous at interval joins, with `f(0)=0` and
  `f'(0)=1`, so small-signal cutoff and loop gain are not silently retuned;
- the saturation join occurs where the exact derivative is negligible and
  cannot create an overshoot;
- quiet NaNs propagate, infinities map to signed one, and no NaN reaches an
  integer table-index conversion;
- tiny finite values remain approximately identity and introduce no new
  denormal slow path; verify both ordinary IEEE behavior and the plug-in's
  no-denormal audio scope;
- adversarial high-resonance renders produce no nonfinite state, recovery, or
  unexpected limit-cycle onset, amplitude, pitch, or decay change.

Do not use global `-ffast-math` as the implementation. Clang documents that it
also relaxes NaN/infinity and signed-zero behavior, permits reassociation and
reciprocal approximations, and can change floating-point environment handling;
that would alter more than the isolated `tanh` A/B. See the [Clang User's
Manual](https://clang.llvm.org/docs/UsersManual.html#cmdoption-ffast-math).

## Benchmark protocol

Run a release build with shipping compiler flags and no work counters. The
existing `YouKnow106OversamplingAudit --tanh-benchmark` protocol uses 48 kHz /
4x, 256-frame blocks, two seconds of preroll, 128 timed blocks, seven paired
repetitions in alternating order, state snapshots copied outside the timer,
and a per-thread CPU clock. It covers idle dry, six-voice plain dry, six-voice
resonant dry, and six-voice full-mixer Chorus II, reporting median/minimum/MAD,
paired exact-over-candidate ratios, and deterministic audio fingerprints.

Before promotion:

1. Run that protocol on native Apple arm64 and native x86_64; do not substitute
   Rosetta results for x86 hardware.
2. Repeat representative 1x and 2x paths and both Early-effect states.
3. Collect the actual `tanh` argument distribution in a separately
   instrumented run, including the fraction in the near-linear and saturated
   regions; do not contaminate the timed build with histogram work.
4. Supplement whole-engine timing with approximation error, derivative error,
   monotonicity/bounds, self-oscillation pitch/level, spectrum, and recovery
   counts. A helper microbenchmark is diagnostic only.

The protocol was fixed before taking the measurement; the first native result
follows rather than being used to tune the test.

### Native Apple M1 Max result

Release arm64, macOS 26.5.1, 48 kHz / 4x, using the protocol above:

| Scenario | Exact median CPU | Hermite512 median CPU | Paired Exact/Fast | CPU reduction |
|---|---:|---:|---:|---:|
| Idle dry | 460.436 ms | 367.211 ms | 1.254x | 20.2% |
| Six-voice plain dry | 466.531 ms | 376.073 ms | 1.228x | 19.4% |
| Six-voice resonant dry | 570.561 ms | 372.956 ms | 1.533x | 34.6% |
| Six-voice full mixer, Chorus II | 486.027 ms | 376.678 ms | 1.293x | 22.5% |

The timed window contains 0.682667 seconds of audio, so the resonant case moves
from `0.836x` to `0.546x` realtime thread CPU. These are machine-specific
whole-engine measurements, not a promise for x86_64 or another libm/compiler.

## Blinded audition protocol

Use `YouKnow106RenderTanhAuditions` at 48 kHz / 4x first, then repeat at 2x and
1x. Each A/B member must start from a fresh deterministic engine with identical
preset, MIDI, quality, and state. Chorus and noise are disabled to expose the
VCF. A single gain derived from the larger peak is applied to both members, so
the test does not normalize away a real level difference.

The current fixture set covers A82-derived self-oscillation, B87 Froggy, B44
Contact Wah, B15 Harpsichord 1, B42 Harpsichord 2, and B88 Owgan. The tool
counterbalances hidden A/B labels and writes the reveal key and difference
metrics separately. Before opening the key, record per fixture: whether a
difference is heard, preference, confidence, and observations about
self-oscillation onset/pitch/level, resonance decay, transient brightness, and
harshness. Re-listen in randomized order; use the revealed difference file and
numeric null metrics only after the blind judgment.

Audibility and preference results are pending and remain independent of the
CPU decision.

The generated 48 kHz / 4x float32 pairs use a shared per-fixture gain and report
the following unboosted RMS nulls: A82-derived `-99.3 dBc`, B87 `-134.4 dBc`,
B44 `-132.0 dBc`, B15 `-29.1 dBc`, B42 `-140.7 dBc`, and B88 `-138.6 dBc`.
B15 is intentionally phase/resonance sensitive; its two RMS levels differ by
only `0.0040 dB`, while its peaks differ by `0.0262 dB`. The null therefore
identifies the stress case but does not decide preference or acceptability.

## Primary background sources

- Antti Huovilainen, [*Non-Linear Digital Implementation of the Moog Ladder
  Filter*](https://dafx.de/paper-archive/2004/P_061.PDF), DAFx-04.
- Vadim Zavalishin, [*The Art of VA Filter Design*](https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_1.1.1.pdf).
- Arm, [Optimized Routines](https://github.com/ARM-software/optimized-routines),
  including the vector `tanh` sources linked above.
- Naoki Shibata and Francesco Petrogalli, [*SLEEF: A Portable Vectorized
  Library of C Standard Mathematical Functions*](https://arxiv.org/abs/2001.09258),
  with [official source](https://github.com/shibatch/sleef).
- Sollya, [`fpminimax` reference](https://sollya.org/sollya-8.0/help.php?name=fpminimax).
- LLVM, [Clang Compiler User's Manual](https://clang.llvm.org/docs/UsersManual.html#cmdoption-ffast-math).
