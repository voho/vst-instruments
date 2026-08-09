# Realism-change audition index

These are deterministic before/after comparisons for individual signal-path
changes. Each folder keeps raw stereo 48 kHz float32 audio, a signed
`after - before` file, exact manifests, and listening copies. Before and after
are never normalized independently: an ordinary fixture uses one shared gain;
the exceptionally quiet voice-VCA fixture uses a protocol-fixed +30 dB
diagnostic gain that is plainly labelled.

For a useful audition, level-match your monitor once, loop the named section,
and switch between `before-listen` and `after-listen` without changing gain.
Then use `difference-listen` to locate what changed. A difference track is an
analysis aid, not what the synth sounds like, and audibility still depends on
the raw level recorded in each folder's README and manifest.

## Recommended order

1. **Common VCA LEVEL circuit** — the clearest broad tonal/level change.
   Listen to [before](common-vca-level/common-vca-level-before-listen-f32.wav),
   [after](common-vca-level/common-vca-level-after-listen-f32.wav), then the
   [signed difference](common-vca-level/common-vca-level-difference-listen-f32.wav).
   Difference RMS is −8.70 dBc. The corrected law follows the stored-DAC,
   resistor and uPC1252 path instead of the former cubic curve.

2. **Archived BBD host-grid alias reconstruction** — the clearest historical
   chorus-quality comparison. Listen to
   [before](bbd-host-grid-alias/bbd-host-grid-alias-before-listen-f32.wav),
   [after](bbd-host-grid-alias/bbd-host-grid-alias-after-listen-f32.wav), then the
   [signed difference](bbd-host-grid-alias/bbd-host-grid-alias-difference-listen-f32.wav).
   The ten labeled sections include direct tones and musical stabs at HQ and LQ;
   the stronger audible contrast is intentionally in the LQ sections. At the
   fixed minimum clock, its two false LQ second-image folds fall by 28.35 and
   26.19 dB. In HQ they were already about -116 dBc and fall another 54--55 dB,
   while the wanted first image moves only -0.04 dB. The LQ near-Nyquist wanted
   image is not claimed transparent: it moves -5.30 dB from an already
   -100.47 dBc baseline. Whole-file difference is -15.95 dBc peak and -27.66
   dBc RMS at the same fixed 0 dB gain. This protocol predates the current
   derived clock schedule and Step-8 input-edge interpolation; its immutable
   baseline frame count intentionally prevents a misleading current `after`
   render. The current numerical result is in the common-host quality audit.

3. **Voice-VCA silent thump** — the most isolated artifact comparison.
   Listen to [before](voice-vca-feedthrough/voice-vca-feedthrough-before-listen-f32.wav),
   [after](voice-vca-feedthrough/voice-vca-feedthrough-after-listen-f32.wav),
   and the [signed difference](voice-vca-feedthrough/voice-vca-feedthrough-difference-listen-f32.wav).
   These three files all use the same fixed +30 dB diagnostic magnification;
   the actual raw peak moves from −68.24 to −148.42 dBFS. The old artifact is
   therefore clear here without pretending it was 30 dB louder in normal use.

4. **BBD transfer clock law** — a subtle moving wet-path coloration.
   Listen to [before](bbd-transfer-clock-law/bbd-transfer-clock-law-before-listen-f32.wav),
   [after](bbd-transfer-clock-law/bbd-transfer-clock-law-after-listen-f32.wav),
   and the [signed difference](bbd-transfer-clock-law/bbd-transfer-clock-law-difference-listen-f32.wav).
   Concentrate on high harmonics through the full Chorus I and II sweeps;
   difference RMS is −48.30 dBc. This fixes duplicated clock scaling, not the
   still-unmeasured installed-unit transfer.

5. **Retrigger release-tail path** — a short, event-dependent transient.
   Listen to [before](retrigger-release-tail/retrigger-release-tail-before-listen-f32.wav),
   [after](retrigger-release-tail/retrigger-release-tail-after-listen-f32.wav),
   and the [signed difference](retrigger-release-tail/retrigger-release-tail-difference-listen-f32.wav).
   Focus on repeated note reassignments during release. Difference peak is
   −25.02 dBc and RMS is −49.53 dBc; the correction removes a host-event-time
   VCA jump that bypassed the shared converter scan.

The per-folder README is controlling for patch, timing, shared gain, caveats
and exact unrounded measurements.
