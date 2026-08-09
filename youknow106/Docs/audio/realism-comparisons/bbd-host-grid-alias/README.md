# BBD host-grid alias realism comparison

This deterministic fixture separates an implementation artifact from the
physical images of an asynchronously clocked bucket-brigade line. A clean
2.093 kHz sine first drives one complete modeled BBD/support chain at the
minimum sweep clock. That stationary probe places the wanted in-band
`fClock - fInput` image near, but not on, the host-folded `fClock + fInput`
image. The next four sections run the same wet-only tone through complete
Chorus I and II LFO cycles at HQ (192 kHz internal) and LQ (48 kHz
internal). Four final sections repeat those complete cycles with bright
full-engine chord stabs. Sections are separated by 100 ms of digital zero.

A physical image is always identified at its unfurled source frequency
`k*fClock +/- fInput`. If that source is above 24 kHz, its in-band 48 kHz
fold is labeled an unwanted numerical alias; suppressing that fold must not
erase the physical image when it genuinely lies below host Nyquist. The
manifest records both coordinates and the exact-frequency Hann measurement
contract.

Raw and listening WAVs are stereo 48 kHz float32. Protocol v1 uses one fixed
0 dB gain for the whole concatenation and for both comparison stages: there
is no section, quality, mode, or stage normalization. Chorus noise and clock
bleed are disabled. At the archived source fingerprints, the HQ direct probe
used the then-shipping 63-tap, two-stage half-band decimator rather than a
renderer-only resampler. The after pass refuses a baseline unless its manifest
binds the scenario, protocol, frame count, raw sample hash and a distinct
pre-change DSP source fingerprint.

This folder is frozen evidence for the earlier output-step reconstruction, not
an isolated Step-8 Lagrange comparison. Its immutable baseline contains 925,348
frames at the archived clock program; the current derived clock rates produce
891,964 frames. The renderer therefore correctly refuses to align a current
`after` pass with that baseline. Step 9 is instead controlled by the
factor-independent common-host oracle and the actual 44.1–192 kHz shipping
selector matrix. Both common-host 4× cells and all six HQ paths pass the
absolute four-case low-drive deterministic-line fixture gates; lower factors
remain absolute failures, while
HQ-off passes only its frozen Step-8 nonregression limits. A future listening
comparison needs a new protocol and immutable baseline rather than overwriting
this one.

Only the text between the generated markers below is renderer-owned.

Historical reproduction commands at the archived source fingerprints:

```bash
cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison
./build-dsp/YouKnow106RenderRealismComparison bbd-host-grid-alias before \
  Docs/audio/realism-comparisons/bbd-host-grid-alias
# Apply and rebuild exactly one future BBD host-grid change, then:
./build-dsp/YouKnow106RenderRealismComparison bbd-host-grid-alias after \
  Docs/audio/realism-comparisons/bbd-host-grid-alias
```

<!-- BEGIN GENERATED REALISM COMPARISON -->
Generated comparison for scenario protocol 1. `difference` is signed `after - before`.

| Signal | Raw peak | Raw RMS |
| --- | ---: | ---: |
| Before | 4.195987284e-01 | 1.792872886e-01 |
| After | 4.042703509e-01 | 1.791197665e-01 |
| Signed difference | 6.691306829e-02 | 7.421573531e-03 |

| Relative metric | Value |
| --- | ---: |
| Difference peak / before peak | -15.95 dBc |
| Difference RMS / before RMS | -27.66 dBc |
| Listening gain | +0.000 dB fixed; no normalization |

Stationary 2.093 kHz / minimum-clock Hann amplitudes, each
relative to that render's own input fundamental:

| Target | Quality | Observed Hz | Before | After | Change |
| --- | --- | ---: | ---: | ---: | ---: |
| wanted physical k1-minus | HQ / 192 kHz | 21832.23 | -51.51 dBc | -51.55 dBc | -0.04 dB |
| unwanted folded k1-plus | HQ / 192 kHz | 21981.77 | -83.59 dBc | -83.65 dBc | -0.06 dB |
| unwanted folded k2-minus | HQ / 192 kHz | 2242.54 | -115.91 dBc | -171.08 dBc | -55.17 dB |
| unwanted folded k2-plus | HQ / 192 kHz | 1943.46 | -115.97 dBc | -169.67 dBc | -53.70 dB |
| wanted physical k1-minus | LQ / 48 kHz | 21832.23 | -100.47 dBc | -105.77 dBc | -5.30 dB |
| unwanted folded k1-plus | LQ / 48 kHz | 21981.77 | -104.89 dBc | -114.33 dBc | -9.45 dB |
| unwanted folded k2-minus | LQ / 48 kHz | 2242.54 | -26.87 dBc | -55.23 dBc | -28.35 dB |
| unwanted folded k2-plus | LQ / 48 kHz | 1943.46 | -27.42 dBc | -53.61 dBc | -26.19 dB |

Per-section raw levels; relative columns use the matching before
section as their denominator:

| Section | Before RMS | After RMS | Difference RMS | Diff peak | Diff RMS |
| --- | ---: | ---: | ---: | ---: | ---: |
| fixed-min-clock-tone-hq | -17.75 dBFS | -17.75 dBFS | -51.90 dBFS | -28.23 dBc | -34.15 dBc |
| fixed-min-clock-tone-lq | -17.71 dBFS | -17.74 dBFS | -39.75 dBFS | -15.04 dBc | -22.04 dBc |
| swept-tone-hq-chorus-i | -10.96 dBFS | -10.96 dBFS | -48.86 dBFS | -28.44 dBc | -37.90 dBc |
| swept-tone-hq-chorus-ii | -10.96 dBFS | -10.96 dBFS | -48.85 dBFS | -28.39 dBc | -37.89 dBc |
| swept-tone-lq-chorus-i | -10.94 dBFS | -10.95 dBFS | -36.58 dBFS | -15.95 dBc | -25.64 dBc |
| swept-tone-lq-chorus-ii | -10.94 dBFS | -10.95 dBFS | -36.60 dBFS | -16.22 dBc | -25.66 dBc |
| musical-stabs-hq-chorus-i | -24.76 dBFS | -24.76 dBFS | -70.57 dBFS | -35.51 dBc | -45.81 dBc |
| musical-stabs-hq-chorus-ii | -24.58 dBFS | -24.59 dBFS | -69.82 dBFS | -36.80 dBc | -45.24 dBc |
| musical-stabs-lq-chorus-i | -24.72 dBFS | -24.72 dBFS | -58.19 dBFS | -24.32 dBc | -33.47 dBc |
| musical-stabs-lq-chorus-ii | -24.55 dBFS | -24.55 dBFS | -58.13 dBFS | -23.87 dBc | -33.58 dBc |

See `bbd-host-grid-alias-comparison-manifest.json` for complete
machine-readable targets, per-section metrics, hashes and source fingerprints.
<!-- END GENERATED REALISM COMPARISON -->
