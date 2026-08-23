# VCF `tanh` optimization research note

Status: `Exact` remains the reference and default. `ZonedHermite` with
reciprocal normalization is the current implementation behind the
experimental persisted `Fast` choice (ordinal 1). A separate persisted
`VCF Fast Early` choice keeps `Hermite` as its default and exposes the C1
`Cubic` candidate for blind comparison. C1 Poly9 and Poly5 inner-zone
replacements have also been measured in isolated candidate builds and rendered
for blind comparison; neither is in the plug-in selector yet. Native Apple M1
Max profiling, paired whole-engine timing, and 48 kHz blind renders are
recorded below; cross-platform ranking and the player's hearing judgment remain
pending. No project result is inferred from library papers.

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

### Observed hot path and argument range

Native Apple M1 Max sampling profiles of the release arm64 engine at 48 kHz /
4x put `std::tanh` at **62–65% of top-of-stack samples** in the high-resonance
sound-generation cases. The instrument executes **540 calls per internal
frame**, or **103.68 million calls/second**, with Character enabled. Six
physical voice cards remain powered and advance even when only one VCA is
open, which explains the measured single-note and six-note costs being within
1% before changing the kernel; polyphony is not multiplying the six-card base
cost.

After the final sound-identical Fast cleanups, four independent six-second
native captures produced the following exclusive top-of-stack shares. These
are sample shares, not elapsed-time estimates or additive inclusive stacks:

| Fixture | Fast VCF derivative | Voice render | Engine process | Voice update | Chorus/support |
|---|---:|---:|---:|---:|---:|
| Single plain | 59.256% | 25.688% | 5.910% | 4.161% | 3.558% |
| Six-note plain | 60.032% | 26.989% | 6.155% | 2.389% | 3.624% |
| Single resonant | 58.322% | 26.977% | 6.350% | 3.822% | 3.114% |
| Six-note resonant | 60.455% | 26.783% | 5.313% | 2.536% | 3.855% |

The separately symbolized derivative consumes `58.322–60.455%`. Passing its
four-double state by value makes arm64 carry the state in `d0`–`d3`; Merson
candidate construction and combination remain in the outer `renderVoice`
attribution. Current-binary disassembly locates all ten derivative calls and
counts only the unambiguous regions between the first nine returns, proving a
conservative derivative-plus-Merson numerical-core floor of
`68.433–70.861%`. The post-tenth-call region mixes final filter work with
coupling/VCA code and is excluded. Adding all of `renderVoice` would instead
give only an `84.945–87.238%` upper bound because it also contains the DCO,
mixer and VCA. Neither number is claimed as exact total VCF cost. Every
remaining individual libm leaf is below 0.33%.

The Early-only follow-up counted `123,617,280` arguments per two-second-preroll
plus timed fixture. Single-note and chord counts are identical because all six
cards advance continuously:

| Fixture | Mean `abs(x)` | Maximum `abs(x)` | `abs(x)>=0.5` | `>=1.0` |
|---|---:|---:|---:|---:|
| Single plain | 0.15526 | 0.41321 | 0% | 0% |
| Six-note plain | 0.15537 | 0.41321 | 0% | 0% |
| Single resonant | 0.25295 | 1.46495 | 12.20% | 0.277% |
| Six-note resonant | 0.26776 | 1.46519 | 13.41% | 0.302% |

Only `0.0044%` of resonant Early arguments reached 1.4. This distribution is
specific to the recorded fixtures; the helper retains a bounded saturation
path for arbitrary standalone-engine inputs.

A separate histogram build observed the actual scalar arguments rather than
guessing a fit interval. In the plain single-note/six-note probes their maximum
magnitudes were `0.895`/`0.962`. In the high-resonance probes they were
`2.489`/`2.503`; `99.52–99.58%` were below 1 and `99.995%` were below 2. The
histogram code was not present in the timed binary.

The same Character-on assembly audit found nine scalar divisions in every
derivative call: one fixed feedback normalization plus eight divisions by a
headroom value that is shared by every calculation at that Merson node. Ten
derivatives therefore performed 90 divisions per card interval even though
only seven distinct headroom nodes exist.

## Candidate comparison

| Candidate | Numerical and implementation shape | Current conclusion |
|---|---|---|
| Exact double libm | Existing `std::tanh(double)` with platform-defined implementation and standard special-value behavior. | Reference/default. Its native Apple M1 Max project cost is measured below; other architectures remain pending. |
| Float libm | Convert the argument to `float`, evaluate `std::tanh(float)`, and widen the result. | Rejected on this target: maximum error `5.779e-8`, but `tanhf` was 5% slower in the plain probe and 34% slower in the resonant probe than the first table candidate. |
| Bounded rational P5/Q5 | Odd numerator/denominator evaluated with Horner/FMA, unit slope at zero, and explicit saturation. | Numerically excellent (`6.03e-10` maximum error) but rejected here: division and the longer dependency chain made it 38–40% slower than the first table candidate. [Sollya `fpminimax`](https://sollya.org/sollya-8.0/help.php?name=fpminimax) remains useful for future target-specific work. |
| Uniform Hermite512 (historical) | 512 positive-domain intervals over `[0, 19]`, four doubles each (16 KiB), exact-mode nodes, analytic slopes, cubic Horner evaluation and sign reconstruction. | The first implemented experiment. Maximum error `2.017e-8`; whole-engine results are retained below as historical evidence. |
| Uniform Hermite256 / quintic Hermite96 | Smaller cubic table, or a smaller table with value/first/second derivative matching. | Rejected: Hermite256 raised error to `3.222e-7` for essentially no speed gain; the quintic reached `4.307e-9` but was 2–4% slower. |
| ZonedHermite + reciprocal normalization (current Fast) | 160 cubic intervals over `[0,5]` at `1/32`, then 56 over `[5,19]` at `1/4`; four doubles each (6.75 KiB total, 5 KiB hot), exact-mode nodes and analytic slopes, odd sign reconstruction, and explicit saturation. Seven per-node reciprocals replace 90 repeated divisions per Character-on card interval; Exact retains division. | Current scalar Pareto point. The table was 5.6–5.8% faster than Hermite512; reciprocal normalization and bit-identical invariant hoists reduce complete-engine cost further. |
| Equal-headroom reciprocal broadcast | When scalar headroom is unchanged, compute one Fast reciprocal and broadcast it to all seven nodes instead of letting LLVM emit three packed and one scalar division. | Rejected despite bit-identical output: 60 rotating candidate/baseline pairs measured only `0.104%` median reduction against `0.310%` paired MAD, with 37/60 wins and order-dependent signs. The extra branch was not justified. |
| C1 cubic Fast Early (opt-in) | Replace only the four Character/Early transfers with `x * (1 - 4*x*x/27)` for `abs(x)<1.5`, saturating to signed one outside. Feedback and the four circuit-defining main stages remain ZonedHermite. The function is odd, monotone, bounded, unit-slope at zero and C1 at saturation. | Selectable audition candidate. Maximum transfer error is `0.112847`, but the modeled multiplier is `1 + 0.005*C*f(x)`, so its maximum multiplier error is `0.0564%` at Character 1 and `0.1128%` at Character 2. The integrated selector saves `5.3–7.2%` over Fast-Hermite across all six whole-engine scenarios below; hearing remains mandatory. |
| C1 Poly9 inner zone (audition) | For `abs(x)<1`, an odd degree-9 polynomial constrained to `f(0)=0`, `f'(0)=1`, and the exact ZonedHermite value and slope at `abs(x)=1`; the existing ZonedHermite table handles larger arguments. | High-fidelity audition candidate, not current Fast. Maximum absolute error is `7.25672e-6` (`0.000725672%` of full scale), maximum relative error is `0.00223719%`, and the derivative remains in `[0.419974,1]`. It saves `13.465%` median whole-engine CPU over current Fast-Hermite across the strict 60-pair sound-generation run; 60/60 pairs win. |
| C1 Poly5 inner zone (audition) | The same boundary constraints with the minimum useful odd degree: `x + x^3 * (0.06759593687336579*x^2 - 0.30600178091760094)` for `abs(x)<1`, then the existing table. | Deliberate sub-1% stress endpoint, not current Fast. Maximum absolute error is `0.00214236` (`0.214236%` of full scale), maximum relative error is `0.395585%`, and the derivative remains in `[0.419974,1]`. It saves `20.893%` median whole-engine CPU over current Fast-Hermite; 60/60 pairs win. Hearing, especially self-oscillation, is mandatory. |
| Double linear tables | Linear interpolation either across all 160 fine intervals, or across 256 intervals below one before rejoining the existing Hermite table. | Not promoted. The compact 160-chord table is monotone and saves 5.688% in the isolated measured-distribution kernel, but its `9.39290e-5` maximum error is larger than Poly9's. The 256 hybrid reaches `1.46828e-6` maximum error but saves only 3.049% in that kernel. Neither justified another audition preset before the existing hearing verdict. Float-node variants were rejected for downward seams. |
| Local SIMD | After the scalar feedback return, the four stage-current arguments can be evaluated together; the four optional Early arguments form another batch. Prototypes exercised Apple's four-float vector `tanh` and explicit two-double-lane evaluation of the final Poly9. | Rejected on the measured M1 Max. Vector `tanh` was 67–76% slower than current scalar Fast and introduced one-ULP downward steps. Explicit two-lane Poly9 was bit-identical to scalar Poly9 but 7.23–9.26% slower across the four sound-generation fixtures; the ordinary fixed-size loop did not auto-vectorize. Packing and external-call overhead outweighed smaller assembly. |
| Cross-voice SIMD | Restructure voices into lane-friendly state/control batches and evaluate corresponding nonlinearities across cards. | Potentially fills wider vectors, but requires a larger SoA/scheduler rewrite and careful handling of inactive extension voices. Defer until local SIMD and scalar candidates are measured. |

The Arm sources are evidence for a current high-quality vector construction,
not a speed prediction for YouKnow106: [double AdvSIMD
`tanh`](https://github.com/ARM-software/optimized-routines/blob/master/math/aarch64/advsimd/tanh.c)
and [float AdvSIMD
`tanhf`](https://github.com/ARM-software/optimized-routines/blob/master/math/aarch64/advsimd/tanhf.c).
SLEEF documents its low-branch, FMA/mask-based vector approach in the
[paper](https://arxiv.org/abs/2001.09258) and lists supported targets in the
[official repository](https://github.com/shibatch/sleef).

## Why ZonedHermite is the current Fast kernel

The zoned table keeps the first experiment's narrow scalar drop-in and removes
its unhelpful uniform allocation. The measured audio path never exceeded
`|x|=2.503`, so all observed calls use the 5 KiB fine table and a direct branch.
The coarse 1.75 KiB tail preserves safe behavior for hostile standalone-engine
inputs through the prior `|x| >= 19` clamp. It needs no dependency or
architecture dispatch and does not rearrange voice or integrator state.

Fast also computes one reciprocal at each of the seven sanitized Merson
headroom nodes and reuses it for that node's normalization. It does not
interpolate a reciprocal or change the modeled headroom trajectory. A
two-million-value scalar probe plus every table boundary and adjacent input at
the production headroom extrema found at most `3.55e-15` change in a normalized
argument and `1.11e-16` change after ZonedHermite, over 100 million times below
the table's maximum transfer error. The Exact specialization retains the
original divisions. Common Early-effect, widened card constants and
node-by-stage omega products are hoisted without changing operation order; the
frozen Exact whole-engine fingerprints remain identical.

The focused test contract scans one million positive-domain points, checks
oddness, monotonicity, bounds, table nodes, signed zero, NaN, infinities, and
rejects a dense-grid absolute error above `1.3e-8` or a relative error above
`1.0e-7`. Next-representable values
on both sides of every table boundary are checked separately, as is the local
unit slope near zero. Adjacent exact nodes that round to the same double become
an explicit flat plateau, with their shared slopes zeroed, instead of retaining
sub-ULP analytic slopes. The
observed dense maxima are `1.15546197e-8` absolute at about `x=5.12344` and
`4.42220926e-8` relative at about `x=0.011438`. An independent coefficient
extrema audit found every analytic interval derivative in `[0,1]`, with
`9.99688943e-7` maximum derivative error. At `x=19`, the last rounded table
plateau joins explicit one with a monotone one-ULP upward step; it is not
claimed to be literally C0. These are engineering checks, not a formal proof
or a hearing result.

The requested 1% objective is defined conservatively as full-scale scalar
transfer error, `sup |fast(x)-tanh(x)| / 1 <= 0.01`. The measured value is
`1.15546e-8`, or `1.15546e-6%`, about 865,000 times below that ceiling. This is
not a promise that every rendered sample nulls within 1%: nonlinear feedback
can amplify tiny trajectory differences, so stability and blind listening are
separate gates.

## Numerical and runtime admission requirements

These are engineering gates for this feedback system, not a transferred
stability theorem from another filter topology:

- finite inputs produce finite outputs within `[-1, 1]`;
- the function is odd, preserves signed zero, and is nondecreasing;
- value and derivative are continuous at ordinary interval joins, including
  the zone seam at 5, with `f(0)=0` and `f'(0)=1`, so small-signal cutoff and
  loop gain are not silently retuned;
- the explicit saturation join is monotone, bounded, no larger than one output
  ULP, and occurs where the exact derivative is negligible;
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
resonant dry, and six-voice full-mixer Chorus II, plus explicit single-voice
plain and resonant cases. It reports median/minimum/MAD, paired Exact/Fast and
Fast-Hermite/Cubic-Early ratios, and deterministic audio fingerprints.

Before promotion:

1. Run that protocol on native Apple arm64 and native x86_64; do not substitute
   Rosetta results for x86 hardware.
2. Repeat representative timing at 1x and 2x and on both Early-effect states.
3. Repeat the actual `tanh` argument distribution on each shipping target in a
   separately instrumented run, including the fraction in the near-linear and
   saturated regions; do not contaminate the timed build with histogram work.
4. Supplement whole-engine timing with approximation error, derivative error,
   monotonicity/bounds, self-oscillation pitch/level, spectrum, and recovery
   counts. A helper microbenchmark is diagnostic only.

The protocol and candidate names are emitted with every result so measurements
of different Fast internals cannot be conflated.

### Native Apple M1 Max result

Release arm64, macOS 26.5.1, 48 kHz / 4x, using the protocol above:

| Scenario | Exact CPU | Current Fast CPU | Paired median | Worst pair | CPU reduction |
|---|---:|---:|---:|---:|---:|
| Idle dry | 412.854 ms | 244.932 ms | 1.687x | 1.667x | 40.7% |
| Single-voice plain dry | 410.627 ms | 246.559 ms | 1.669x | 1.570x | 40.1% |
| Six-voice plain dry | 416.053 ms | 248.031 ms | 1.699x | 1.651x | 41.1% |
| Single-voice resonant dry | 514.113 ms | 245.517 ms | 2.092x | 2.083x | 52.2% |
| Six-voice resonant dry | 523.985 ms | 244.593 ms | 2.146x | 2.096x | 53.4% |
| Six-voice full mixer, Chorus II | 442.743 ms | 249.016 ms | 1.775x | 1.706x | 43.7% |

The timed window contains 0.682667 seconds of audio. Current Fast therefore
uses `0.358–0.365x` realtime thread CPU, versus `0.602–0.768x` for Exact. All
42 paired Fast runs were faster; the worst paired ratios remain far above the
observed MAD. Single and six-note cost is again nearly equal because the six
physical cards always advance. This final run includes reciprocal
normalization, invariant hoists and the five byte-identical specializations
described below. These are machine-specific whole-engine measurements, not a
promise for x86_64 or another libm/compiler.

The integrated `VCF Fast Early=Cubic` selector was then timed in the same
binary against `Fast Early=Hermite`. The accepted implementation resolves the
two public settings once per parameter snapshot and dispatches once per
internal frame, outside the fixed-card loop; template specialization leaves
the Exact/Fast-Hermite filter kernels unchanged while giving Cubic its own
inlined Early transfer. Seven paired alternating repetitions keep all three
kernels resident rather than benchmarking a single-purpose prototype:

| Scenario | Fast Hermite CPU | Cubic Early CPU | Paired Hermite/Cubic | Worst pair | CPU reduction |
|---|---:|---:|---:|---:|---:|
| Idle dry | 244.932 ms | 231.984 ms | 1.060x | 1.025x | 5.7% |
| Single-voice plain dry | 246.559 ms | 231.120 ms | 1.065x | 1.062x | 6.1% |
| Six-voice plain dry | 248.031 ms | 233.994 ms | 1.058x | 1.012x | 5.5% |
| Single-voice resonant dry | 245.517 ms | 231.585 ms | 1.061x | 1.043x | 5.7% |
| Six-voice resonant dry | 244.593 ms | 232.813 ms | 1.055x | 1.037x | 5.2% |
| Six-voice full mixer, Chorus II | 249.016 ms | 234.831 ms | 1.060x | 1.051x | 5.7% |

Every one of the 42 paired Cubic runs was faster, and each non-idle mode had a
stable distinct fingerprint. A stricter 60-pair direct run measured
`5.783–6.486%` scenario-median reductions in the four single/chord
plain/resonant fixtures, `6.113%` in aggregate against `0.369%` paired MAD,
and 59/60 Cubic wins. Its one loss was an isolated `-3.154%` scheduler outlier;
the two order splits still measured `6.138%` and `6.022%`. Separate 60-pair
checks of Exact and Fast-Hermite against the frozen pre-selector engine kept
all fingerprints exact and showed no regression signal: their aggregate
changes were `-0.327%` and `-0.416%` (negative means faster), both within
sub-percent paired variability. This is CPU admission evidence only, not an
audibility result.

Two isolated C1 inner-zone candidates were then built against the current
reciprocal-normalized Fast baseline. They keep the same outer ZonedHermite
table and change only `abs(x)<1`, where more than 99.5% of measured arguments
landed. The final Poly9 is

```text
z = x*x
p = ((0.0080134882598101656*z - 0.042828271313479417)*z
       + 0.12921201472089403)*z - 0.33280307571145989
x + x*z*p
```

and Poly5 is

```text
z = x*x
x + x*z*(0.06759593687336579*z - 0.30600178091760094)
```

Both are odd, strictly monotone, unit-slope at zero, and C1 at the join to the
table. A refined dense scan gives Poly9 `7.25672082e-6` maximum absolute error,
`2.23719462e-5` maximum relative error, and `1.00823e-4` maximum derivative
error. Poly5 gives `0.00214236478`, `0.00395585344`, and `0.00930622`
respectively. Thus both pass the requested 1% full-scale transfer bound, but
Poly5 intentionally approaches it much more closely.

The strict benchmark used 15 paired rotating repetitions at 48 kHz / 4x, a
two-second preroll, 128 timed 256-frame blocks, alternating execution order,
and the per-thread CPU clock. Percentages below are reductions versus current
Fast-Hermite; MAD is the median absolute deviation of the paired reductions:

| Fixture | Poly9 reduction | MAD | Wins | Poly5 reduction | MAD | Wins |
|---|---:|---:|---:|---:|---:|---:|
| Single plain | 13.338% | 0.140% | 15/15 | 20.727% | 0.251% | 15/15 |
| Six-note plain | 13.783% | 0.322% | 15/15 | 21.426% | 0.301% | 15/15 |
| Single resonant | 13.447% | 0.089% | 15/15 | 20.888% | 0.266% | 15/15 |
| Six-note resonant | 13.430% | 0.218% | 15/15 | 20.648% | 0.418% | 15/15 |
| Aggregate | **13.465%** | **0.227%** | **60/60** | **20.893%** | **0.372%** | **60/60** |

These are single-purpose candidate binaries, not selector-overhead estimates.
Their stable but deliberately different fingerprints confirm that the intended
audio paths ran. A degree-7 middle point was numerically scanned and smoke
timed, but was not promoted to this strict matrix: Poly9 already supplies the
high-fidelity end and Poly5 supplies the useful hearing ceiling. Earlier notes
had conflated timings from a different least-squares coefficient family with
the final C1 family; the numbers above are the first strict whole-engine timing
of the final C1 Poly9 and Poly5 implementations.

One sound-identical scalar cleanup was also retained. The VCF state check now
uses `abs(value) <= 64` directly, which already rejects NaNs and infinities,
instead of preceding it with redundant `isfinite`. Across two independent
60-pair rotations all 120 audio fingerprints matched; the combined aggregate
median reduction was `0.513%` against `0.289%` MAD, with 100/120 wins. Two other
exact-output source simplifications were rejected because their aggregate
medians were zero or negative.

A second sound-identical Fast specialization removes a much hotter redundant
guard. The checked `zonedHermiteTanh` entry point still propagates a direct NaN,
but the integrated Fast path now validates all four bounded states once before
the Merson step and calls a private unchecked table evaluator thereafter.
Finite histories, sanitized controls and card trims preserve the invariant;
hostile NaN, infinity and out-of-range states still take the original recovery
path. Character 1 previously repeated the helper check in all 90 nonlinear
evaluations per card interval. Four up-front state comparisons replace those
90 checks, removing 99.072 million comparisons/s across six cards at 48 kHz/4×.
A strict 75-pair complete-engine cohort measured **1.048168%** aggregate median
reduction with `0.168865` pp MAD, 74/75 wins and byte-identical fingerprints.
The selected implementation adds 176 bytes of text; a duplicated checked/
unchecked kernel was both slightly slower at 0.925105% and four times larger.

The production unchecked evaluator then stopped repeating one more public
API concern: its `abs(x) < DBL_MIN` bypass. The checked entry point retains the
original signed-subnormal behavior, and a 20,000,155-value signed scan matched
checked and unchecked results bit for bit under the production state
invariant. This removes 90 comparisons per Character-on card interval, or
103.68 million/s at 48 kHz/4×. A strict 90-pair cohort measured
**2.028914%** aggregate median reduction with `0.293491` pp MAD, 85/90 wins,
both order splits positive and no fingerprint mismatch.

A one-token ABI change passes the four-double derivative state by value rather
than by `const&`. Arm64 classifies it as a homogeneous floating-point
aggregate and supplies it in `d0`–`d3`; the Fast Merson shell shrinks from
1,308 to 1,128 bytes, the derivative by another 24 bytes, and linked text by
432 bytes. Disassembly removes roughly 40 helper state loads and 31 shell
stores per interval. All six raw scenario fingerprints remained identical.
The strict 90-pair cohort measured **4.935256%** aggregate median reduction
with `0.424318` pp MAD, 88/90 wins, positive `4.707464–5.530715%` scenario
medians and agreeing executable-order splits.

Together with the up-front state check, ordinary no-event one-pole path and a
separate chorus invariant cleanup, the five byte-identical changes save
**9.188726%** aggregate median whole-engine CPU over 90 paired runs
(`0.422565` pp MAD, 90/90 wins, no fingerprint mismatch). That measured
combined result, rather than the sum of isolated medians, is the production
claim.

Two structural prototypes were also rejected on complete-engine evidence. A
hot-path split for the VCF input-history startup reduced the Fast-Hermite
derivative from 2,881 to 2,501 arm64 instructions and removed an estimated
13.824 million dynamic branches plus 1.152 million stores per second. Despite
bit-identical fingerprints, its strict 60-pair aggregate reduction was only
`0.029%` against `0.287%` MAD, with 31/60 wins. The smaller assembly did not
translate into measurable engine time.

Two later branch/arithmetic reductions reinforce that result. Hoisting the
feedback gain removed three multiplications per card interval but measured
only `0.006832%` aggregate against `0.270956` pp MAD, with execution-order
signs disagreeing. Cloning a Character-on early-effect integration body removed
39 branches per interval, about 44.9 million/s, but added 3.1 KiB of hot text
and made all five scenario medians roughly 6% slower (`-6.050798%` aggregate,
2/75 wins). Both remain rejected.

Three final compile-shape experiments were likewise rejected. Marking the
fine-table branch `[[likely]]` improved fall-through layout but screened at
only `0.483242%` against `0.571062` pp MAD, with 12/18 wins and disagreeing
execution-order signs. Changing table indices to `uint32_t` removed 44 static
arm64 conversions yet made all 18 pairs slower by about 4%. Capturing two
derivative invariants by value saved 80 text bytes but screened at
`-0.238945%` with 7/18 wins. Instruction counts remained diagnostic; paired
whole-engine time decided admission.

Local vectorization was worse. Apple's four-float vector `tanh` reduced the
derivative's static assembly from 1,872 bytes / 468 instructions to 640 bytes /
160 instructions, but required two external vector-math calls per derivative.
Complete 128-block sound-generation probes were 67–76% slower than scalar
Fast. Its dense scan stayed within `9.9638e-8` absolute error, but also found
82,656 one-float-ULP downward steps over `[-20,20]`; an exhaustive positive
binary32 audit through the observed range likewise found more than 560,000
downward transitions. An inline two-double-lane Poly9 avoided both external
calls and numerical differences, matching scalar Poly9 fingerprints exactly,
but averaged `8.20%` slower (`7.23–9.26%`) across single/chord plain/resonant
fixtures. A `std::array` Horner loop did not auto-vectorize. These results reject
local SIMD without changing production code; cross-voice state restructuring
remains a separate, much larger design change.

For historical comparison, the earlier uniform Hermite512 run measured
19.4–34.6% reduction over the same four original scenario shapes. Its raw
medians were Exact/Fast `460.436/367.211`, `466.531/376.073`,
`570.561/372.956`, and `486.027/376.678` ms for idle, six-voice plain,
six-voice resonant, and full Chorus II. Those results remain labelled
Hermite512 and are not presented as ZonedHermite measurements.

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

Pass `--early-cubic` for the independent Fast-Hermite/Cubic-Early comparison.
That corpus forces Unit Character to 2, uses a fresh balanced hidden mapping at
each quality, and writes to `tanh-auditions-cubic-early-{1,2,4}x` by default.
The unflagged command remains the Exact/Fast comparison. Revealing either key
does not reveal the other mapping.

The Exact/Fast ZonedHermite corpus uses fresh balanced masks at every quality,
distinct from the previously revealed Hermite512 assignment. The renderer
refuses a non-empty output root so it cannot silently replace an earlier
bundle. Independent 1x, 2x, and 4x rerenders of that corpus hash identically
across all 60 WAV/CSV files. The new Cubic-Early corpus was also rendered twice
from independent fresh engines; every one of its 60 WAV/CSV files compares
byte-for-byte.

Audibility and preference results are pending and remain independent of the
CPU decision.

The current 48 kHz / 4x Fast pairs report RMS nulls of `-106.4`, `-138.6`,
`-136.8`, `-29.2`, `-142.1`, and `-141.3 dBc` in fixture order.
B15 is intentionally phase/resonance sensitive; at 1x its trajectory null is
`-8.0 dBc` even though the scalar transfer error remains `1.16e-8`. That makes
it the highest-priority hearing case, not an automatic rejection or
preference result.

For the Character-2 Fast-Hermite/Cubic-Early corpus, the blind-pair difference
levels are:

| Fixture | 1x | 2x | 4x |
|---|---:|---:|---:|
| A82-derived self oscillation | -39.9 dBc | -39.9 dBc | -39.9 dBc |
| B87 Froggy | -57.8 dBc | -51.7 dBc | -55.1 dBc |
| B44 Contact Wah | -69.9 dBc | -69.9 dBc | -70.0 dBc |
| B15 Harpsichord 1 | -7.7 dBc | -29.6 dBc | -28.6 dBc |
| B42 Harpsichord 2 | -69.1 dBc | -79.4 dBc | -82.2 dBc |
| B88 Owgan | -71.0 dBc | -71.0 dBc | -71.0 dBc |

B15 is therefore the first hearing target, followed by the A82-derived
self-oscillation fixture. These nonlinear trajectory nulls identify where to
listen; they do not establish that the difference is audible or preferred.

The isolated Exact/Poly9 corpus lives in
`tanh-auditions-poly9-{1,2,4}x`. Its RMS nulls are:

| Fixture | 1x | 2x | 4x |
|---|---:|---:|---:|
| A82-derived self oscillation | -36.9 dBc | -36.9 dBc | -36.9 dBc |
| B87 Froggy | -82.8 dBc | -82.9 dBc | -82.8 dBc |
| B44 Contact Wah | -82.2 dBc | -82.2 dBc | -82.2 dBc |
| B15 Harpsichord 1 | -7.0 dBc | -28.6 dBc | -28.6 dBc |
| B42 Harpsichord 2 | -87.9 dBc | -99.5 dBc | -101.4 dBc |
| B88 Owgan | -91.6 dBc | -91.6 dBc | -91.6 dBc |

The isolated Exact/Poly5 corpus lives in
`tanh-auditions-poly5-{1,2,4}x`. It is the intentional stress endpoint:

| Fixture | 1x | 2x | 4x |
|---|---:|---:|---:|
| A82-derived self oscillation | +0.9 dBc | +0.9 dBc | +0.9 dBc |
| B87 Froggy | -36.0 dBc | -35.8 dBc | -35.8 dBc |
| B44 Contact Wah | -32.2 dBc | -32.1 dBc | -32.1 dBc |
| B15 Harpsichord 1 | -5.7 dBc | -27.8 dBc | -27.6 dBc |
| B42 Harpsichord 2 | -44.8 dBc | -53.4 dBc | -53.8 dBc |
| B88 Owgan | -43.0 dBc | -43.0 dBc | -43.0 dBc |

All 54 Poly5 WAV files compare byte-for-byte with an independent rerender.
The large A82 null is trajectory/phase divergence in deliberate
self-oscillation, not a listening verdict; it is precisely the fixture that
should reject Poly5 if the changed oscillator character is objectionable.
Judge each `blind` directory before opening its sibling `reveal/key.csv`.

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
