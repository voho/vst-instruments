# Common VCA LEVEL realism comparison

This fixture holds a three-note tone through stored VCA LEVEL bytes 0, 32,
64, 96 and 127, followed by rapid transitions that expose the shared hold's
scan latency and settling. It repeats the sequence dry and through Chorus II
because the common VCA precedes the chorus and therefore changes its drive.

The `before` render preserves the former voiced
`gain_dB=−15+20·(b/127)³` curve and borrowed hold slew. The `after` render
uses the nominal circuit derivation: physical code `d=b<<5`, ideal 12-bit R-2R
`Vhold=4−10d/4096`, the p. 15 R30/C7/R32/R31/R165 network, and NEC's
−5.9 mV/dB typical GC1 constant. That reduces to
`gain_dB=−16.3196647+0.165581014·b`; C7's loaded 908.249 Ω resistance gives
`τ=9.08249 ms` (`fc=17.523 Hz`). The `/4096` DAC convention and the scanned
R32=1.5 kΩ reading are stated assumptions. Real component, +15 V rail and IC
variation remain an installed-unit measurement task, so these files compare
implementations and do not claim a hardware null.

The raw float32 baseline is archival evidence and must be rendered before the DSP
change. Listening files use one shared gain; no side is independently normalized.
Only the text between the generated markers below is renderer-owned.

From the project directory, render the two stages around exactly one rebuilt DSP
change:

```bash
cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison
./build-dsp/YouKnow106RenderRealismComparison common-vca-level before \
  Docs/audio/realism-comparisons/common-vca-level
# Apply and rebuild the DSP change, then:
./build-dsp/YouKnow106RenderRealismComparison common-vca-level after \
  Docs/audio/realism-comparisons/common-vca-level
```

<!-- BEGIN GENERATED REALISM COMPARISON -->
Generated comparison for scenario protocol 1. `difference` is signed `after - before`.

| Signal | Raw peak | Raw RMS |
| --- | ---: | ---: |
| Before | 0.678248 (-3.37 dBFS) | 0.085207 (-21.39 dBFS) |
| After | 0.633889 (-3.96 dBFS) | 0.093616 (-20.57 dBFS) |
| Signed difference | 0.375842 (-8.50 dBFS) | 0.031286 (-30.09 dBFS) |

| Correct relative metric | Value |
| --- | ---: |
| Difference peak / before peak | -5.13 dBc |
| Difference RMS / before RMS | -8.70 dBc |
| Shared listening gain | 0.738944189 (-2.628 dB) |

See `common-vca-level-comparison-manifest.json` for machine-readable
metadata and exact values.
<!-- END GENERATED REALISM COMPARISON -->
