# Neuramar

Neuramar is a sample-learned neural instrument for macOS: drop in one sound,
let it infer the root and fit a compact local synthesis model, then play the
result across the keyboard. Its **Core**, **Air**, and **Bone** layers preserve
different parts of the source's identity while the front panel invites the
memory to stay faithful, drift, breathe, or become something new.

Neuramar is not a conventional sampler. The imported recording is analysed in
the background but is not repitched or played back when notes sound. Completed
learning produces a small neural controller plus explicit harmonic, stochastic,
and modal synthesizers. Rendering is local and does not require a cloud service,
GPU, pretrained model, or source file.

![Neuramar standalone interface](Docs/screenshots/neuramar-standalone.png)

The screenshot is rendered by the real JUCE editor in the plug-in regression
suite. VST3, Audio Unit, and Standalone builds use the same resizable interface.

The project builds three products from one JUCE codebase:

- VST3 instrument for hosts such as Ableton Live, REAPER, Cubase, and Bitwig
- Audio Unit v2 music device for Logic Pro and GarageBand
- Standalone application for direct MIDI and drag-and-drop exploration

> **Just want to try it?** The scheduled Nightly workflow publishes the latest
> successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> Those bundles are ad-hoc signed and not notarized; check the repository's
> Nightly badge for the latest workflow result.

## Teach a sound, then play its memory

1. Drag a `.wav`, `.wave`, `.aif`, `.aiff`, `.flac`, or `.ogg` file onto the
   neural pool, or click **Drop / Open** to choose one.
2. Neuramar conditions the audio, finds a stable root, measures its harmonic,
   non-harmonic, and resonant behaviour, and fits the neural controller locally. The
   display follows the reading, root-finding, analysis, and training stages;
   **Cancel** stops an in-progress pass.
3. Check the inferred note, cents offset, and confidence. Use the `-` and `+`
   buttons to correct an octave or pitch-class mistake by up to 12 semitones in
   either direction.
4. Play MIDI or the on-screen keyboard. A completed model is published as one
   immutable snapshot at an audio-block boundary; training and file access do
   not run in the real-time render path.

Learning is an offline operation within the plug-in, not a network request.
Loading a new sound starts a new fit; the playable model changes only when that
fit completes successfully. The file loader accepts at least 60 ms and reads at
most the first 12 seconds; stereo input is downmixed for the monophonic analysis.

## Choosing a useful source

Neuramar learns most predictably from one clean, isolated, mostly monophonic
note. A source that includes a clear onset, a stable pitched body, and some of
its natural evolution gives the model more useful evidence than a tiny clipped
fragment.

For the clearest root and most controllable instrument:

- use a single note rather than a chord, full mix, or layered loop;
- keep the wanted sound louder than room noise, accompaniment, and reverb;
- avoid hard clipping and excessive limiting;
- include the characteristic attack and enough sustain or decay to reveal how
  the spectrum changes;
- start with acoustic notes, voices without words, struck or bowed objects,
  drones, or designed one-shots, then experiment with stranger material.

Percussion, chords, and unstable or strongly inharmonic sounds are accepted and
can create interesting results, but their inferred root is necessarily less
reliable. The correction buttons are part of the instrument for exactly that
reason. Only analyse recordings you are allowed to use.

## Interface and controls

The neural pool combines waveform feedback, learning stage, progress, source
name, model generation, active voices, and sample rate. The neighbouring root
display shows the inferred note, tuning offset, confidence, and any manual
semitone correction. The editor is resizable and all controls are drawn with
native JUCE graphics.

Neuramar exposes 13 host parameters: eleven continuous front-panel controls,
**Orbit**, and the persisted root correction.

| Control | Musical role |
| --- | --- |
| **Imprint** | 0–100%; moves from a smooth, dream-like harmonic contour toward the learned, more faithful Core. |
| **Body Lock** | 0–100%; moves from resonances that follow the played pitch toward source-like fixed resonances. |
| **Air** | 0–100%; sets the renderer-matched, harmonic-subtracted noise-filterbank layer: breath, scrape, hiss, and noisy attack energy. |
| **Bone** | 0–100%; sets persistence-selected inharmonic modes: body resonances, impact, and ringing. |
| **Gravity** | -100% to +100%; tilts the reconstructed spectrum from darker to brighter. |
| **Memory** | 0.25x–4.00x; changes how quickly a note travels through the learned time evolution. |
| **Mutation** | 0–100%; adds bounded, voice-local movement around the learned character. |
| **Awaken** | 1 ms–2 s; shapes the performance attack. |
| **Dissolve** | 20 ms–8 s; shapes the performance release. |
| **Horizon** | 0–100%; widens voices across the stereo field. |
| **Output** | -24 to +6 dB; sets the final level. |
| **Orbit** | Revisits a stable learned region while a note remains held. |
| **Root correction** | Shifts the inferred source root from -12 to +12 semitones without retraining. |

**Panic** immediately silences sounding voices. **Drop / Open**, **Cancel**, and
**Panic** are actions rather than host parameters.

Root correction is sampler-style key relabelling: the corrected MIDI key
reproduces the learned source pitch. It does not retune the source itself or act
as a master-tuning control.

## Neural synthesis engine

The JUCE-free C++20 core uses a DDSP-inspired, factorized
neural-controller architecture chosen for the severe one-example data limit
and the real-time constraints of a polyphonic plug-in. It is not trained
end-to-end through a differentiable renderer:

- a multi-window YIN-style difference-function detector rejects weak
  periodicity estimates, aligns octave-related candidates in log frequency,
  and checks half/current/double-root hypotheses using harmonic support;
- a constrained local autocorrelation track can retain sufficiently stable
  bends, slow vibrato, and glide around that root, and drives pitch-tracked
  harmonic-coordinate analysis;
- 96 spectral frames at a fixed 48 kHz analysis rate use pitch-adaptive
  512–4096-sample Hann apertures targeting four root periods (bounded at the
  lowest accepted pitches) to retain Core and Air attacks without sacrificing
  low-note resolution; a parallel 4096-sample residual supplies the steadier
  evidence used for six persistence-scored Bone candidates;
- residual power, excluding active Bone neighbourhoods, is accumulated into 48
  log-frequency cells; a deterministic non-negative solver then fits eight
  overlapping log-spaced Air bands against the analytic power response of the
  same normalized biquads used by the renderer;
- deterministic Adam optimization fits a 32-unit, time-conditioned neural
  controller with polynomial, Fourier, and onset-aware inputs to the low-rate
  log-amplitude targets;
- **Core** renders the learned harmonic distribution at each played MIDI pitch,
  **Air** drives an independent deterministic white-noise stream through each
  fitted band, and **Bone** renders only candidate modes that remain locally
  prominent across the sound, all with learned amplitude trajectories;
- each Air filter is normalized to unit expected RMS for its noise input, and a
  smooth gain taper prevents transposed or brightened bands from accumulating at
  the host Nyquist edge;
- each voice owns its envelope, phase, note age, per-band noise, and variation
  state, while controller outputs are forward-interpolated between low-rate
  evaluations so a learned target is reached at the time it describes.

The current engine has a fixed ceiling of eight synthesis voices. It evaluates
the controller at control rate and interpolates its parameters while the
oscillators, filters, envelopes, and note handling remain in the audio path.

The representation is deliberately structured rather than a raw-waveform
generator. Explicit pitch and oscillator priors make one-note extrapolation
possible and keep inference bounded. The rationale, primary papers, claims
boundary, and future quality path are documented in
[`Docs/neural-synthesis-research.md`](Docs/neural-synthesis-research.md).

## Scientific scope and limitations

A single recording cannot reveal notes, velocities, articulations, or registers
that were never captured. Neuramar learns the evidence in that recording and
uses musical synthesis priors to extrapolate it; it does not recover an unknown
original instrument in full. Large transpositions can expose that limitation,
and noisy or ambiguous sources can lead to a wrong root or a less literal model.

The current release uses no pretrained corpus or bundled weights. That keeps the
fit private and self-contained, but it also means Neuramar has no external
knowledge of what the source "should" be. The design prioritizes a bounded local
fit, deterministic recall, and a real-time-safe render path. Listening tests,
host validation, and profiling remain necessary; this README makes no measured
training-time, similarity, or perceptual-quality claim.

The Core/Air split is a local sinusoidal decomposition, not a perfect physical
source separation. Window boundaries, rapidly moving partials, and inharmonic
tonal energy can leak into the residual. Bone tracks are compact,
persistence-scored sinusoidal candidates rather than identified physical
eigenmodes. Air is an expected-power match through a compact eight-filter bank,
not phase-coherent reconstruction of the original residual; its stochastic
waveform therefore preserves an estimated spectral evolution rather than the
recording's exact noise realization. Bands that approach the Nyquist limit at a
low host sample rate are deliberately faded instead of folded onto the edge.

Automatic root search is bounded to 35–2000 Hz. The displayed confidence is an
average YIN periodicity score, not a calibrated probability and not a direct
measure of timbre-match quality. The local pitch contour is constrained to four
semitones around the inferred root, so sufficiently stable bends, slow vibrato,
and glide can be learned, while broad sweeps, octave jumps, fast modulation, and
unvoiced regions are deliberately bounded.

## State, privacy, and presets

Host state stores the ordinary parameters, root analysis metadata, display-only
source filename, a low-resolution waveform preview, and the compact learned
model in a versioned payload. It does not store the source recording or its full
path, so a saved session can recall the synthesized instrument after the
original file moves or disappears. Imported audio stays local during learning,
and performance performs no file or network access.

A host project or exported preset can therefore contain source-derived model
coefficients even though it contains no sample playback data. Treat that state
as derived user content and confirm that you have the necessary rights before
sharing it. No factory samples, pretrained weights, or third-party preset
library are included; see [`Presets/README.md`](Presets/README.md).

## Requirements and formats

- macOS 11 or newer for the released VST3, Audio Unit, and Standalone products
- A current full Xcode installation selected for command-line use
- CMake 3.22 or newer and a C++20 compiler
- Internet access for the default first configure, or a local JUCE 8.0.14
  checkout supplied through `JUCE_PATH` (`NEURAMAR_JUCE_PATH` when configuring
  CMake directly)

JUCE 8.0.14 is pinned and fetched at configure time; it is not vendored into
this repository. The CMake project also selects VST3 and Standalone targets on
non-Apple systems, while the maintained packaging and release workflow is for
macOS.

## Build and test on macOS

The helper creates an Xcode build, compiles universal `arm64`/`x86_64` binaries,
ad-hoc signs the development bundles, and runs both engine and plug-in contract
tests:

```bash
./scripts/build-macos.sh
```

Use `BUILD_UNIVERSAL=OFF` for a native-architecture development build, or point
the configure at an existing checkout of the exact JUCE release:

```bash
BUILD_UNIVERSAL=OFF ./scripts/build-macos.sh
JUCE_PATH="/path/to/JUCE-8.0.14" ./scripts/build-macos.sh
```

Equivalent universal configure, build, and test commands are:

```bash
cmake -S . -B build-macos -G Xcode \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DNEURAMAR_BUILD_UNIVERSAL=ON \
  -DNEURAMAR_BUILD_PLUGIN=ON \
  -DBUILD_TESTING=ON
cmake --build build-macos --config Release --parallel
ctest --test-dir build-macos -C Release --output-on-failure
```

Release bundles are written to:

| Format | Build artifact |
| --- | --- |
| VST3 | `build-macos/Neuramar_artefacts/Release/VST3/Neuramar.vst3` |
| Audio Unit | `build-macos/Neuramar_artefacts/Release/AU/Neuramar.component` |
| Standalone | `build-macos/Neuramar_artefacts/Release/Standalone/Neuramar.app` |

## Run the neural-engine tests without JUCE

The synthesis and learning core is independent of JUCE, so it has a portable
test path on any C++20 development machine:

```bash
cmake -S . -B build-dsp \
  -DCMAKE_BUILD_TYPE=Release \
  -DNEURAMAR_BUILD_PLUGIN=OFF \
  -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The JUCE-free suite covers analysis, deterministic learning and rendering,
finite output, pitch response, and model serialization. Plug-in builds add
processor, parameter, MIDI, editor, and host-state contracts. These regression
checks complement rather than replace listening, validator, multi-host, and CPU
profiling passes.

## Install and validate locally

Copy only the formats you need:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
mkdir -p ~/Library/Audio/Plug-Ins/Components
ditto build-macos/Neuramar_artefacts/Release/VST3/Neuramar.vst3 \
  ~/Library/Audio/Plug-Ins/VST3/Neuramar.vst3
ditto build-macos/Neuramar_artefacts/Release/AU/Neuramar.component \
  ~/Library/Audio/Plug-Ins/Components/Neuramar.component
```

Quit and reopen the host after installing. The Audio Unit identifiers are type
`aumu`, subtype `Nrm1`, and manufacturer `Nram`:

```bash
auval -v aumu Nrm1 Nram
```

With [pluginval](https://github.com/Tracktion/pluginval) installed, validate
the VST3 and then test sample loading, cancellation, root correction, automation,
state recall, note overlap, sample-rate changes, and common buffer sizes in at
least two hosts:

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 10 \
  ~/Library/Audio/Plug-Ins/VST3/Neuramar.vst3
```

## Sign, package, and notarize

After a successful default universal build, create ZIP and PKG artifacts under
`build-macos/dist/`. Their three embedded bundles are ad-hoc signed by default;
the installer container itself remains unsigned unless an Installer identity is
provided:

```bash
./scripts/sign-and-package-macos.sh
```

For public distribution, provide Developer ID identities and a keychain profile
created with `notarytool store-credentials`:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Company (YOURTEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Company (YOURTEAMID)" \
NOTARY_PROFILE="neuramar-notary" \
./scripts/sign-and-package-macos.sh
```

The helper checks bundle versions and architectures, embeds the licence notices,
signs every independently copyable bundle, creates the package, and—when a
notary profile is supplied—submits and staples the installer. Replace the
placeholder bundle identifier and four-character plug-in codes with identifiers
controlled by the publisher before a public release; do not change them after
shipping because hosts use them for recall.

## Project layout

```text
Source/DSP/              JUCE-free analysis, model, and polyphonic engine
Source/PluginProcessor.* MIDI, parameters, learning worker, and host state
Source/PluginEditor.*    Drag-and-drop neural-pool interface and keyboard
Docs/                    Neural-synthesis research and claims boundary
Tests/                   Engine and JUCE processor regression tests
Presets/                 Preset and learned-model provenance guidance
scripts/                 macOS build, signing, packaging, and notarization helpers
```

## Licensing

Neuramar's original source is offered under the MIT License. JUCE is a separate
dependency and is **not** covered by that licence. JUCE 8 framework modules are
available under the AGPLv3 or a commercial JUCE licence; confirm which terms
apply before distributing a binary. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
and the bundled JUCE licence notice for details.
