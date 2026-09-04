# Hardware validation — 2026-09-04

This pass fixes three demonstrated errors and compares the engine against an
identified hardware recording. It does **not** establish that YouKnow106 is
the most faithful emulation on the market. The largest measured mismatch in
this recording remains the noise-source level.

## Hardware reference

Lewis Francis identifies the instrument as **Juno-106 #439522**, with Borish
replacement voice chips installed and calibrated in 2022. The [corrected-bank
96 kHz recording](https://github.com/kayrockscreenprinting/ultramaster_kr106/issues/16#issuecomment-4184997000)
contains `106_calibration_bip.aif`; the matching
[calibration MIDI archive](https://kayrock.org/kr106/106_calibration.zip)
supplies its exact patch messages and score. This is a serviced hardware
instrument, with replacement voice electronics, not a software reference or
evidence that original 80017A modules behave identically.

The owner [confirms that the oscillator chips remain original and identifies
an M-Audio M-Track 2x2 C-Series recording interface](https://github.com/kayrockscreenprinting/ultramaster_kr106/issues/41#issuecomment-4189199115).
The interface gain and master tuning setting are not documented.

| Artifact | SHA-256 |
| --- | --- |
| Original calibration AIFF | `a9282c4a287e7adf1a8cf46879d037633225db6f0e56f16014c08373ffb683aa` |
| Calibration MIDI | `c9727669f08ff27b3c1ccdd4faf4cf1d586537c156483d55d735d0b75dc855e3` |

The analyzer checks normalized PCM identity, so changing the WAV container
does not lose provenance. Unknown recordings are explicitly unverified.
Third-party audio and MIDI remain in the ignored `out/hardware-validation/`
folder, alongside comparison JSON and model audio.

The local [source audition](../out/hardware-validation/source-ab.wav) plays
hardware then model for saw, pulse, sub and noise. One constant gain per
instrument matches the saw RMS, preserving the other sources' relative
differences. Its resampling and short fades are for listening only; measurements
use the untouched recordings.

## What changed

**Filter calibration now includes the complete voice.** The previous model
added static capacitor mismatch, converter carry error and temperature offsets
after drawing supposedly final trim residuals. A real service adjustment
absorbs those fixed errors. The model now applies fixed FREQ/WIDTH corrections
using the existing nonlinear harmonic-balance calculation at the prescribed
4.8 Vpp resonance amplitude. Capacitor differences, converter discontinuities
between the trim points, and subsequent thermal movement remain modeled.
No audio recording was used to fit this correction.

At the declared ten-minute service state, actual rendered output from all six
cards is within **6 cents** of the 248/992 Hz targets; the same conditions with
the new corrections disabled reached approximately **53 cents** error.
Roland requires ±10 cents at both points in the
[Service Notes, p.19](https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=19).
The test suppresses random wander to measure static calibration. The warm-up
temperature model remains provisional. Calibration coefficients are refreshed
on character changes, with about 30 µs measured setter cost, and cached for
audio processing.

**Chorus coupling follows the mute transistors.** C28/C25's 39 kΩ mixer load
previously switched immediately on the button command, while the modeled
transistor drive switched later. The load now follows the transistor state,
preserving capacitor charge during the delayed Off/On transition. The
[jack-board circuit](https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=15)
supports that relationship. Regression checks at 48/192 kHz compare both audio
and capacitor histories across both delayed switching boundaries. This fixes
circuit consistency; it does not measure the installed JFET's switching curve.

**Patch transfer now uses the documented hardware frame.** All 57 patch
messages in the recording's MIDI use 24-byte Manual dumps. The old codec
accepted and emitted a 23-byte `0x30` frame missing the program byte. It now
reads the documented numbered (`0x30`) and Manual (`0x31`) frames and writes
Manual frames, following the [Owner's Manual, MIDI implementation
§3.1–3.2](https://synthfool.com/docs/Roland/Juno_Series/Roland_Juno_106/Roland_Juno106_Owners_Manual.pdf#page=35).
Existing YouKnow106 exports remain readable. The live MIDI path, file transfer
and calibration-take generator all use the corrected payload layout.

The 16 DSP checks and both host checks pass. VST3, AU and Standalone builds
and all maintained audio previews were regenerated. The full 128-preset audit
found A51 just 0.08 dB over its existing product loudness limit after filter
recalibration. Reducing its VR1 position from 0.660 to 0.645 gives −28.622 dBFS
gated in a targeted full-score rerender; all 128 measured rows then meet the
existing limits. Its original tone bytes are unchanged.

## What the recording establishes

The score isolates noise, saw, pulse and sub, then the four HPF positions and
three resonance settings. Its actual bytes select 16-foot range, MIDI note
60, VCA gate/level 64 and chorus Off. Pulse uses **manual PWM byte 64**, not
the 50% pulse described in the website prose. The initial noise segment is
silent on the hardware; the identical repeated flat-HPF noise segment at
10 seconds supplies that measurement instead.

Steady AC RMS ratios below use the left channel and a common 1.3-second window,
starting 0.5 seconds into each section. Overall recording gain cancels;
different frequencies, nonlinearities and replacement-card behavior do not.

| Source relative to saw | Hardware | Nominal model | Difference |
| --- | ---: | ---: | ---: |
| Noise, full level | +1.60 dB | −10.10 dB | −11.70 dB |
| Pulse, manual PWM 64 | +1.62 dB | +2.92 dB | +1.30 dB |
| Sub, one octave below saw | +6.92 dB | +6.66 dB | −0.26 dB |

The default character profile gives nearly the same conclusion: approximately
−11.7 dB noise error, +1.3 dB pulse error and −0.1 dB sub error. The sub
coordinate therefore needs no adjustment from this recording. The pulse's
second harmonic relative to its fundamental also closely agrees (about
−2.12 dB hardware / −2.19 dB model), distinguishing its level difference from
an incorrect PWM law.

An [independent noise recording from the same owner at the same gain](https://github.com/kayrockscreenprinting/ultramaster_kr106/issues/16#issuecomment-4145617345)
agrees with the calibration take within approximately 0.3 dB. The noise gap
is much larger than the observed subwindow variation. Relative to the output
of the fully resonant, noise-driven filter, roughly 9 dB of disagreement
remains, rather than all of it being explained by saw level.

A later [same-file oscillator calibration](https://github.com/kayrockscreenprinting/ultramaster_kr106/issues/44#issuecomment-4400136262)
isolates noise and then true self-oscillation with every sound source off.
It measures noise/self-oscillation at **−3.544 dB on hardware versus −11.873 dB
in the matching nominal render: an 8.329 dB deficit**. The hardware oscillator
is a clean 244.828 Hz tone and subwindow level variation stays below 0.07 dB.
This removes the noise-driven-resonator ambiguity without assuming equal gain
between separate recording sessions. Only the first 11.5 seconds of that
192 kHz AIFF were downloaded; the saved hashes identify the extracted prefix,
not the complete remote file. Its exact patches, metrics and reproduction
script are preserved in `out/hardware-validation/`.

This **withdraws the certainty** of the earlier decision-log assertion that
the stronger noise in other comparisons must be a mistaken crest-factor
reading. It does not establish a universal +11.7 dB correction: noise trim,
original-module gain and the physical TP8 waveform are still unmeasured here.
The source-level defaults remain unchanged pending those measurements.

The HPF Boost upper-band gain agrees at about +1.4 dB relative to Flat.
The filter-sweep measurements are **noise-driven spectral peaks**, not
isolated self-oscillation fundamentals. Subwindow ranges expose the broad
and unstable peaks; results beyond the shared recording bandwidth are
excluded. They are not suitable for fitting a cents-accurate tuning law.
The approximately nine-cent oscillator tuning difference also cannot identify
a DCO error without the owner's tuning setting and recording-clock reference.

## Reproduce

Build the DSP tools with the commands in the main README. Python analysis uses
NumPy and SciPy; their versions and the analyzer hash are recorded in its JSON.
With the downloaded files in `out/hardware-validation/`:

```sh
python3 Tools/AnalyzeHardwareCalibration.py \
  --midi out/hardware-validation/reference-106_calibration.mid \
  --events out/hardware-validation/events.txt
./build-dsp/YouKnow106RenderCalibrationEvents \
  out/hardware-validation/events.txt out/hardware-validation/model.wav 1
python3 Tools/AnalyzeHardwareCalibration.py \
  --reference out/hardware-validation/reference-439522.wav \
  --model out/hardware-validation/model.wav \
  --output out/hardware-validation/comparison.json
python3 Tools/AnalyzeHardwareCalibration.py --self-test
ctest --test-dir build-dsp --output-on-failure
```

Use character `0` instead of `1` for the nominal comparison. The renderer
uses 48 kHz, 4x processing, Exact/Merson, six voices and volume 1, with no
normalization. It preserves MIDI timestamps and bytes; it does not simulate
the serial transmission time of a complete SysEx message. These steady
windows do not qualify hardware onset or switching timing.

To record an original-card unit, regenerate the project's own capture set:
`./build-dsp/YouKnow106MakeCalibrationTakes out/calibration-takes`.
Use one unchanged recording gain, no normalization or effects, and preserve
the received patch bytes. Original-card source isolation, TP8 noise crest
and level, and an original chorus capture are the most useful next evidence.
