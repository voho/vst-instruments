# CPU optimization — 2026-09-05

The settled four-voice Merson filter solver now inlines its small nonlinear
kernels and passes its four-vector derivative state by value. This reduces
function-call and stack traffic while retaining the same equations, precision,
fallbacks, integration steps and audio settings.

On an Apple M1 Max, the sixteen-voice full-mixer/Chorus-II cases used
**10.32–13.24% less engine CPU at 1×/2×**. The six-voice equivalent at
44.1 kHz/2× measured **about 5%** (5.09%, with a candidate timing MAD of
0.720 ms, so this result is borderline); at 48 kHz it saved 3.79–4.16%, below
the requested 5% threshold. No 5% gain was established for the default 4× setting,
dry patches,
single voices or idle. These results apply to Poly/Cubic/Normal when the
filter needs the deeper Merson solve and four adjacent voices are active.

All 20 before/after raw-float audio fingerprints matched exactly. The changes
introduce no new approximation or reduction in quality. A broader RK4 inlining
experiment regressed performance and was discarded.

## Measurement

Apple clang 21.0.0, Release `-O3`, ThinLTO, native arm64, 256-frame blocks.
Each scenario pre-rolls for two seconds, then measures seven identical
32,768-frame renders using the thread CPU clock. Copying the pre-rolled engine
and hashing output occur outside the timer. Tables report milliseconds per
render. The baseline ran before and after the candidate; reductions use the
**faster** of those two baseline medians, not wall time or the host CPU meter.
Negative reductions are slower measurements. Small control-case differences
remain below 5% and are not claimed as gains.

The original engine source has Git blob
`d0a8286e95d84e3e51a6ed041f377ede3ccb6a86`. The retained binary/library and
unabridged trial, A/B/A timing, build and test logs are in the local ignored
`out/cpu-2026-09-05/` directory (`final-a-*`, `final-b-*`, `final-c-*`).

| Host Hz | Quality | Scenario | Baseline ms | Optimized ms | CPU reduction |
| ---: | :---: | --- | ---: | ---: | ---: |
| 48000 | 2x | six-voice-full-mixer-chorus-ii | 83.819 | 80.331 | +4.16% |
| 48000 | 1x | six-voice-full-mixer-chorus-ii | 44.752 | 43.054 | +3.79% |
| 48000 | 2x | sixteen-voice-full-mixer-chorus-ii | 185.802 | 163.102 | +12.22% |
| 48000 | 1x | sixteen-voice-full-mixer-chorus-ii | 103.140 | 92.498 | +10.32% |
| 44100 | 2x | idle-dry | 21.656 | 21.635 | +0.10% |
| 44100 | 2x | single-voice-plain-dry | 30.239 | 30.422 | -0.61% |
| 44100 | 2x | six-voice-low-cutoff-dry | 57.926 | 57.530 | +0.68% |
| 44100 | 2x | six-voice-plain-dry | 61.063 | 59.415 | +2.70% |
| 44100 | 2x | single-voice-resonant-dry | 30.256 | 31.667 | -4.66% |
| 44100 | 2x | six-voice-resonant-dry | 62.593 | 64.654 | -3.29% |
| 44100 | 2x | six-voice-full-mixer-chorus-ii | 84.608 | 80.300 | +5.09% |
| 44100 | 2x | sixteen-voice-full-mixer-chorus-ii | 187.220 | 162.423 | +13.24% |
| 48000 | 4x | idle-dry | 42.587 | 42.470 | +0.27% |
| 48000 | 4x | single-voice-plain-dry | 59.007 | 59.421 | -0.70% |
| 48000 | 4x | six-voice-low-cutoff-dry | 112.206 | 115.082 | -2.56% |
| 48000 | 4x | six-voice-plain-dry | 114.253 | 115.009 | -0.66% |
| 48000 | 4x | single-voice-resonant-dry | 59.122 | 59.527 | -0.69% |
| 48000 | 4x | six-voice-resonant-dry | 117.198 | 118.558 | -1.16% |
| 48000 | 4x | six-voice-full-mixer-chorus-ii | 157.539 | 162.259 | -3.00% |
| 48000 | 4x | sixteen-voice-full-mixer-chorus-ii | 374.778 | 377.790 | -0.80% |

## Reproduce

Build and retain the baseline executable before applying the DSP change, then
build the candidate with identical Release/LTO settings. Run baseline,
candidate, baseline serially; avoid simultaneous builds or other benchmarks.
The new mode uses the plug-in's shipping Poly/Cubic/Normal defaults; the
historical no-argument audit retains its Exact/Hermite/Max reference.

```sh
cmake --build build --target YouKnowOversamplingAudit -j 4
build/YouKnowOversamplingAudit --cpu-benchmark
build/YouKnowOversamplingAudit --cpu-benchmark 44100 2
build/YouKnowOversamplingAudit --merson-benchmark
```

Timing is intentionally not a CI pass/fail gate. The existing SIMD regression
checks cover scalar equivalence/error limits, nonlinear fallbacks, occupancy
transitions and atomic rejection; run them on both slices of the universal
build with `YOUKNOW_SIMD_TEST_ONLY=1` (use `arch -x86_64` for the Intel slice).

## Validation

The final universal VST3, AU and standalone builds succeeded. All 19 CTest
checks passed (266.58 seconds), including the full engine suite, factory-preset
audit, circuit and dynamic quality contracts, plug-in processor and VST3 bundle
smoke test. The focused SIMD tests also passed on arm64 and x86_64 under
Rosetta. The benchmark CLI passed help, malformed/range/arity and instrumented
build rejection checks. CPU percentages above are native arm64 measurements;
no native Intel performance claim is made.
