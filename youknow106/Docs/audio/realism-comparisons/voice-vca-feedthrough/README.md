# Voice-VCA feedthrough realism comparison

This fixture opens all six physical voices in Unison while every intentional
oscillator, sub, and noise source is off. Gate-open and gate-close events occur
at the same nominal converter-scan phase, isolating any signal invented by the
voice-VCA path from musical masking.

The `-listen` files apply a protocol-fixed **+30 dB diagnostic magnification**.
They do not represent the defect at its original loudness and are never
adaptively normalized. The raw float32 files carry the actual rendered level.
Before, after, and signed difference always receive the identical fixed gain.

The engine intentionally retains deterministic microscopic filter excitation,
so the full render is not expected to become exact digital silence. No calibrated
hardware capture yet establishes the residual feedthrough of a trimmed unit.

Only the text between the generated markers below is renderer-owned.

```bash
cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison
./build-dsp/YouKnow106RenderRealismComparison voice-vca-feedthrough before \
  Docs/audio/realism-comparisons/voice-vca-feedthrough
# Apply and rebuild the single DSP change, then:
./build-dsp/YouKnow106RenderRealismComparison voice-vca-feedthrough after \
  Docs/audio/realism-comparisons/voice-vca-feedthrough
```

<!-- BEGIN GENERATED REALISM COMPARISON -->
Generated comparison for scenario protocol 1. `difference` is signed `after - before`.

| Signal | Raw peak | Raw RMS |
| --- | ---: | ---: |
| Before | 3.872434900e-04 | 4.880099365e-05 |
| After | 3.793245540e-08 | 9.186652847e-09 |
| Signed difference | 3.872655216e-04 | 4.880328716e-05 |

| Relative metric | Value |
| --- | ---: |
| Difference peak / before peak | 0.00 dBc |
| Difference RMS / before RMS | 0.00 dBc |
| Listening gain | +30.000 dB fixed diagnostic magnification |

| Window | Before RMS | After RMS | Difference RMS |
| --- | ---: | ---: | ---: |
| pre-event-floor | 0.000000e+00 | 0.000000e+00 | 0.000000e+00 |
| gate-open-transient | 1.027468e-04 | 1.895220e-08 | 1.027503e-04 |
| held-settled-floor | 1.617372e-06 | 1.077361e-08 | 1.618157e-06 |
| gate-close-transient | 1.029726e-04 | 6.341943e-09 | 1.029787e-04 |
| released-settled-floor | 1.603462e-06 | 8.370253e-11 | 1.603546e-06 |

See `voice-vca-feedthrough-comparison-manifest.json` for complete
machine-readable window metrics and unrounded values.
<!-- END GENERATED REALISM COMPARISON -->
