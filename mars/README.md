# Mars

Mars is an original virtual-analog polyphonic synthesizer built around a direct,
one-panel workflow. It combines broad dual-oscillator tone, calibrated
voice-to-voice movement, two complementary nonlinear filters, and a global
stereo ensemble without reproducing a particular vintage product or circuit.

There is **no arpeggiator** and **no modulation matrix**. Every sound parameter
is exposed as a front-panel knob, slider, or switch and as a host-automatable
parameter.

![Mars Standalone instrument interface](Docs/screenshots/mars-standalone.png)

The screenshot is the actual Standalone application built from this source;
the VST3 and Audio Unit use the same resizable JUCE editor. Its generated panel
texture is stored at
[`Assets/mars-panel-texture.png`](Assets/mars-panel-texture.png) and compiled
into the plug-in. Labels and interactive controls remain native JUCE components
for crisp resizing, automation, keyboard operation, and accessibility.

> **Just want to try it?** The scheduled Nightly workflow publishes the latest
> successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> The bundles are ad-hoc signed and not notarized; check the repository's Nightly
> badge for the latest workflow result.

## Sound architecture

- **Two event-corrected oscillators per render slot:** VCO I and VCO II provide
  saw, pulse, and stable leaky-integrated triangle waveforms. Standard polyBLEP
  correction is applied to saw resets and both pulse edges. VCO II has octave,
  semitone, and fine tuning; the mixer also includes pulse-width control, a
  pulse sub oscillator one octave below VCO I, noise, and bounded phase
  modulation from VCO II to VCO I.
- **Antialiased nonlinear mixer:** equal-power oscillator balance feeds a
  driven soft saturator implemented with first-order ADAA. This reduces
  waveshaper aliasing without a neural runtime or a full circuit solve.
- **Two filter models:** `Ladder` is a bounded nonlinear four-stage/four-pole
  ladder-inspired filter with a two-iteration delay-free feedback solve and
  finite fallback. `Orbit` is a two-integrator TPT state-variable filter;
  `Filter shape` sweeps low-pass through band-pass to high-pass. A 3 ms
  transition runs both models to prevent switching clicks; at steady state the
  voice executes only the selected algorithm, keeping nonlinear work bounded
  as polyphony grows.
- **Rate-aware oversampling:** at host rates up to and including 96 kHz, the
  complete per-slot voice paths run at 2x, are summed, and return through one
  stereo 15-tap halfband FIR. Above 96 kHz they run at 1x. The global ensemble
  remains at host rate.
- **Deterministic voice cards:** 32 render slots carry fixed calibration offsets
  for tuning, cutoff, resonance, drive, envelope time, pan, pulse skew, and
  slow per-card drift. The same state and MIDI input render deterministically;
  there is no simulated-parts-wear control.
- **Dedicated modulation:** separate filter and amplifier ADSRs sit beside a
  triangle, sine, or sample-and-hold LFO with direct pitch, filter, and PWM
  depths. The mod wheel deepens those fixed LFO routes; it does not open a
  hidden routing matrix.
- **Performance controls:** `Poly`, `Unison`, and `Fifth` allocate the 32 render
  slots in different groups. Glide, velocity response, stereo spread, fixed
  ±2-semitone pitch bend, MIDI CC 1 mod wheel, and MIDI CC 64 sustain are
  implemented. CC 123 follows note-off and sustain-pedal semantics; CC 120 and
  the panel Panic button mute immediately.
- **Global stereo ensemble:** a cross-fed, low-pass-shaped modulated delay has
  direct `Ensemble mix` and `Ensemble rate` controls. It is one global
  algorithm rather than a selectable family, with no additional hidden spatial
  stages.

The modeling rationale, primary papers, neural-modeling decision, and precise
claims boundary are in
[`Docs/analog-modeling-research.md`](Docs/analog-modeling-research.md).

## Polyphony and slot allocation

Mars has 32 DSP render slots and a maximum of 16 simultaneously held note
groups. Allocation depends on `Voice mode`:

| Mode | Slots per note group | Maximum note groups |
| --- | ---: | ---: |
| `Poly` | 1 | 16 |
| `Unison` | selected `Unison voices` value, 2–8 | `min(16, floor(32 / voices))` |
| `Fifth` | 2: root plus a perfect fifth | 16 |

When a new group needs room, Mars steals the oldest released group first and
then the oldest held group. Retriggers and steals preserve a fixed 2 ms fading
tail to avoid a hard sample discontinuity. `Unison voices` is active only in
`Unison` mode.

## Exact 40-control contract

| Section | Front-panel controls (parameter IDs) |
| --- | --- |
| VCO I | Waveform (`osc1Wave`), octave (`osc1Octave`) |
| VCO II | Waveform (`osc2Wave`), octave (`osc2Octave`), tune (`osc2Tune`), fine tune (`osc2Fine`) |
| Mixer | Oscillator balance (`oscMix`), pulse width (`pulseWidth`), sub level (`subLevel`), noise level (`noiseLevel`), cross modulation (`crossMod`) |
| Filter | Model: `Ladder` / `Orbit` (`filterModel`), cutoff (`cutoff`), resonance (`resonance`), drive (`filterDrive`), Orbit shape (`filterShape`), envelope amount (`filterEnvAmount`), key tracking (`keyTrack`) |
| Filter envelope | Attack (`fAttack`), decay (`fDecay`), sustain (`fSustain`), release (`fRelease`) |
| Amplifier envelope | Attack (`aAttack`), decay (`aDecay`), sustain (`aSustain`), release (`aRelease`) |
| LFO | Waveform: triangle / sine / sample & hold (`lfoWave`), rate (`lfoRate`), pitch depth (`lfoPitch`), filter depth (`lfoFilter`), PWM depth (`lfoPwm`) |
| Voice | Mode: `Poly` / `Unison` / `Fifth` (`voiceMode`), unison voices (`unisonVoices`), voice-card drift (`drift`), stereo spread (`spread`), glide time (`glide`), velocity response (`velocity`) |
| Output | Ensemble mix (`chorusMix`), ensemble rate (`chorusRate`), output level (`output`) |

These are the complete host parameter IDs for version 1. There are no other
sound controls hidden behind a matrix or alternate panel.

## Build products

One JUCE codebase produces:

- VST3 instrument;
- Audio Unit v2 music device on macOS; and
- standalone application.

## Requirements

- macOS 11 or newer for the supported release bundles;
- a current full Xcode installation selected for command-line use;
- CMake 3.22 or newer; and
- internet access on first configure, or a local JUCE 8.0.14 checkout supplied
  through `JUCE_PATH` to the helper (`MARS_JUCE_PATH` at the CMake layer).

JUCE 8.0.14 is pinned to an immutable release archive and checksum. It is not
vendored into the repository.

## Build on macOS

From the `mars` directory:

```bash
./scripts/build-macos.sh
```

Use a local JUCE checkout without a FetchContent download:

```bash
JUCE_PATH="$HOME/SDKs/JUCE-8.0.14" ./scripts/build-macos.sh
```

The helper configures an Xcode project, builds universal `arm64` + `x86_64`
Release products by default, runs the CTest suite, and ad-hoc signs and strictly
verifies the local VST3, AU, and standalone bundles. Set
`BUILD_UNIVERSAL=OFF` for a faster native-architecture development build.

Equivalent configure, build, test, and local-signing commands:

```bash
cmake -S . -B build-macos -G Xcode \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DMARS_BUILD_UNIVERSAL=ON \
  -DMARS_BUILD_PLUGIN=ON \
  -DBUILD_TESTING=ON

cmake --build build-macos --config Release --parallel
ctest --test-dir build-macos -C Release --output-on-failure

for bundle in \
  build-macos/Mars_artefacts/Release/VST3/Mars.vst3 \
  build-macos/Mars_artefacts/Release/AU/Mars.component \
  build-macos/Mars_artefacts/Release/Standalone/Mars.app; do
  codesign --force --sign - "$bundle"
  codesign --verify --deep --strict "$bundle"
done
```

| Format | Release artifact |
| --- | --- |
| VST3 | `build-macos/Mars_artefacts/Release/VST3/Mars.vst3` |
| Audio Unit | `build-macos/Mars_artefacts/Release/AU/Mars.component` |
| Standalone | `build-macos/Mars_artefacts/Release/Standalone/Mars.app` |

## JUCE-free DSP build

The synthesis engine and its regression suite do not depend on JUCE:

```bash
cmake -S . -B build-dsp \
  -DCMAKE_BUILD_TYPE=Release \
  -DMARS_BUILD_PLUGIN=OFF \
  -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The DSP tests cover rates through 384 kHz, finite output, release completion,
long-note triangle stability, deterministic rendering, distinct oscillator and
filter responses, click-resistant steals and model changes, meaningful glide
and modulation, voice-mode allocation, and a CPU regression guardrail. Plug-in
builds additionally test the parameter, state, MIDI, and editor contract.

## Install and validate locally

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"

ditto build-macos/Mars_artefacts/Release/VST3/Mars.vst3 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Mars.vst3"
ditto build-macos/Mars_artefacts/Release/AU/Mars.component \
  "$HOME/Library/Audio/Plug-Ins/Components/Mars.component"
```

Validate the Audio Unit (`aumu`, subtype `Mar1`, manufacturer `Mars`):

```bash
auval -v aumu Mar1 Mars
```

Validate the VST3 with pluginval if it is installed:

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 10 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Mars.vst3"
```

## Sign, package and notarize

Build first, then run:

```bash
./scripts/sign-and-package-macos.sh
```

With no environment overrides the helper ad-hoc signs the VST3, AU, and app,
then writes a ZIP and unsigned installer package under `build-macos/dist`. The
filename records the architectures actually present (`universal`, `arm64`, or
`x86_64`) so a native development build cannot be mislabeled.
The helper derives its version from the three bundle property lists, rejects a
conflicting `VERSION` override, and includes the Mars license, third-party
notices, and pinned JUCE dual-license notice in the installer tree and inside
each signed bundle.
For distribution, provide `APP_SIGN_IDENTITY`, `INSTALLER_SIGN_IDENTITY`, and
optionally a `NOTARY_PROFILE` created for `xcrun notarytool`. Replace the sample
bundle identifier and manufacturer/plugin codes with identifiers
controlled by the publisher before shipping public binaries.

With `NOTARY_PROFILE` set, the helper submits and staples the installer package.
The ZIP still contains signed bundles but is not itself the notarized
distribution artifact.

## Project layout

```text
Assets/                  Bundled generated panel artwork
Docs/                    Real interface screenshots, research, and modeling decisions
Source/DSP/              JUCE-free synthesis engine
Source/PluginProcessor.* MIDI, automation, state, and engine bridge
Source/PluginEditor.*    Resizable direct-control hardware panel
Tests/                   DSP and plug-in regression checks
Presets/                 Original sound-design recipes
scripts/                 macOS build and distribution helpers
```

## Licensing

The original Mars source is offered under the MIT License. JUCE is a separate
dependency and is **not** covered by that license. JUCE 8 is available under
AGPLv3 or a commercial JUCE licence; confirm the applicable terms before
distributing binaries. In particular, a publisher must establish its own JUCE
commercial or AGPLv3 distribution basis; bundling the notice does not grant or
replace that licence.
