# Retrigger/release-tail realism comparison

This focused fixture reassigns one still-releasing voice to the same note at
several deterministic control-scan phases. Keeping the pitch unchanged isolates
assignment/VCA behaviour from oscillator restart and portamento effects.

The raw float32 baseline is archival evidence and must be rendered before the DSP
change. Listening files use one shared gain; no side is independently normalized.
Only the text between the generated markers below is renderer-owned.

From the project directory, render the two stages around exactly one rebuilt DSP
change:

```bash
cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison
./build-dsp/YouKnow106RenderRealismComparison retrigger-release-tail before \
  Docs/audio/realism-comparisons/retrigger-release-tail
# Apply and rebuild the DSP change, then:
./build-dsp/YouKnow106RenderRealismComparison retrigger-release-tail after \
  Docs/audio/realism-comparisons/retrigger-release-tail
```

<!-- BEGIN GENERATED REALISM COMPARISON -->
Generated comparison for scenario protocol 1. `difference` is signed `after - before`.

| Signal | Raw peak | Raw RMS |
| --- | ---: | ---: |
| Before | 0.158556 (-16.00 dBFS) | 0.068552 (-23.28 dBFS) |
| After | 0.158541 (-16.00 dBFS) | 0.068532 (-23.28 dBFS) |
| Signed difference | 0.008893 (-41.02 dBFS) | 0.000229 (-72.81 dBFS) |

| Correct relative metric | Value |
| --- | ---: |
| Difference peak / before peak | -25.02 dBc |
| Difference RMS / before RMS | -49.53 dBc |
| Shared listening gain | 3.160949855 (9.996 dB) |

See `retrigger-release-tail-comparison-manifest.json` for machine-readable
metadata and exact values.
<!-- END GENERATED REALISM COMPARISON -->
