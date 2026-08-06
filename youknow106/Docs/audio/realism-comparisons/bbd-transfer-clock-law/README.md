# BBD transfer/clock-law realism comparison

This full-engine fixture holds the same bright 4-foot saw/pulse dyad dry,
through Chorus I, and through Chorus II. Each wet section begins after its
support network settles and spans 3.5 complete modulation cycles, repeatedly
visiting both BBD clock extremes. Chorus noise and speculative clock bleed are
off, so the comparison concentrates on deterministic wet-path transfer loss.

The `before` render uses the former
`alpha·(1+(clock−26000)·1.5e−6)` coefficient. Because the residual
`transferLossStep` state advances once per modeled BBD shift (one fCP period), that multiplier
applies clock scaling twice: a fixed alpha already makes the absolute pole move
with clock and preserves its response versus normalized `f/Fclock`. The old law
therefore gives −2.757 dB versus DC, or −2.732 dB versus the datasheet's 1 kHz
reference, at 40 kHz clock/12 kHz signal, and sweeps the normalized 0.3-cycle
response from roughly −3.04 to −2.14 dB over 23.9–77.1 kHz.

The `after` render holds alpha at 0.8654743, retaining the explicit zero-order
hold and producing −3.000 dB versus DC, or −2.972 dB versus the datasheet's
1 kHz reference. The remaining 0.028 dB is documented rather than retuned as an
inaudible change. This is an implementation and
one-anchor correction, not evidence that a physical MN3009 is invariant versus
normalized frequency. Panasonic does provide low-resolution typical curves at
fCP 10, 40 and 100 kHz; they still need quantitative extraction and preferably
de-embedded installed-unit confirmation before fitting real clock dependence.

The raw float32 baseline is archival evidence and must be rendered before the DSP
change. Listening files use one shared gain; no side is independently normalized.
Only the text between the generated markers below is renderer-owned.

```bash
cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison
./build-dsp/YouKnow106RenderRealismComparison bbd-transfer-clock-law before \
  Docs/audio/realism-comparisons/bbd-transfer-clock-law
# Apply and rebuild the single Chorus change, then:
./build-dsp/YouKnow106RenderRealismComparison bbd-transfer-clock-law after \
  Docs/audio/realism-comparisons/bbd-transfer-clock-law
```

<!-- BEGIN GENERATED REALISM COMPARISON -->
Generated comparison for scenario protocol 1. `difference` is signed `after - before`.

| Signal | Raw peak | Raw RMS |
| --- | ---: | ---: |
| Before | 0.360279 (-8.87 dBFS) | 0.093817 (-20.55 dBFS) |
| After | 0.359557 (-8.88 dBFS) | 0.093797 (-20.56 dBFS) |
| Signed difference | 0.004139 (-47.66 dBFS) | 0.000361 (-68.86 dBFS) |

| Correct relative metric | Value |
| --- | ---: |
| Difference peak / before peak | -38.79 dBc |
| Difference RMS / before RMS | -48.30 dBc |
| Shared listening gain | 1.391108099 (2.867 dB) |

See `bbd-transfer-clock-law-comparison-manifest.json` for machine-readable
metadata and exact values.
<!-- END GENERATED REALISM COMPARISON -->
