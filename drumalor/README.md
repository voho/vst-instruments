# Drumalor

Drumalor is a real-time procedural drum instrument for macOS. It uses classic
analogue-drum synthesis techniques as a starting point, then generates every
hit locally from oscillators, noise, filters, envelopes, resonators, and
circuit-inspired nonlinear shaping. It does not load samples, copy a ROM,
emulate a particular branded machine, or contact a service while rendering
audio.

The original brief called this a “10-sound” instrument, but its explicit sound
list expands to **13 separately playable synthesized voices**: the low, mid, and
high toms are three voices, followed by shaker, Perc 1, and Perc 2. Drumalor
implements all 13 rather than dropping entries to force the total to ten.

The project builds three products from one JUCE codebase:

- VST3 instrument for hosts such as Ableton Live, REAPER, Cubase, and Bitwig
- Audio Unit v2 music device for Logic Pro and GarageBand
- Standalone application for direct MIDI-pad and on-screen-pad testing

> **Just want to try it?** Prebuilt ad-hoc-signed, un-notarized macOS bundles
> are published from `main` on the repository's rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).

## Voices, MIDI notes, and controls

The primary note map follows General MIDI percussion assignments. MIDI velocity
controls hit strength.

Each row has exactly four automatable controls: two voice-specific character
controls, **Pitch** from -24 to +24 semitones, and **Decay**. Character and decay
controls run from 0% to 100%.

| Voice | GM note | General MIDI assignment | Character A | Character B |
| --- | ---: | --- | --- | --- |
| Kick | 36 | Bass Drum 1 | Punch | Drive |
| Snare | 38 | Acoustic Snare | Wires | Snap |
| Clap | 39 | Hand Clap | Spread | Tone |
| Closed Hat | 42 | Closed Hi-Hat | Metal | Tone |
| Open Hat | 46 | Open Hi-Hat | Metal | Tone |
| Ride | 51 | Ride Cymbal 1 | Bell | Tone |
| Crash | 49 | Crash Cymbal 1 | Spread | Brightness |
| Low Tom | 45 | Low Tom | Punch | Skin |
| Mid Tom | 47 | Low-Mid Tom | Punch | Skin |
| High Tom | 50 | High Tom | Punch | Skin |
| Shaker | 82 | Shaker | Density | Color |
| Perc 1 | 56 | Cowbell | Ratio | Drive |
| Perc 2 | 75 | Claves | Hollow | Click |

Common kit-layout aliases are accepted too: 35 for Kick; 40 for Snare; 44 for
Closed Hat; 53 and 59 for Ride; 57 for Crash; 41 and 43 for Low Tom; 48 for Mid
Tom; 70 for Shaker; and 37, 76, or 77 for Perc 2. Other notes are silent.

Closed Hat chokes a ringing Open Hat, as expected from a shared hi-hat pedal
group. The remaining voices can overlap and retrigger independently. The labels
describe musical intent rather than exposing implementation-specific constants;
hosts store the stable parameter IDs behind them for automation and recall.

## Sound engine

The JUCE-free C++20 DSP core combines short pitch envelopes and tuned bodies for
kick and toms, noise and resonant energy for snare and clap, inharmonic metallic
partials for hats and cymbals, and compact stochastic or resonant models for the
shaker and percussion voices. Voice-specific controls move several related
synthesis values together so each voice remains useful across the full range.

Every trigger also advances a slowly correlated component-drift model for that
instrument, then applies tightly bounded hit-level tolerances to oscillator
pitch and starting phase, envelope decay, transient energy, tone, circuit drive,
and bias. This makes repeated equal-velocity notes differ in timbre and feel
without turning them into random changes of kit, level, or timing. The sequence
is deterministic: resetting the engine or reopening the same project starts the
same variation sequence again, and audio remains independent of host block size.

Each voice finishes through a lightweight asymmetric diode/transistor-style
transfer with a variable operating point and a virtual supply rail that sags
quickly on strong transients and recovers more slowly. First-order analytic
antiderivative antialiasing (ADAA) is applied to these nonlinear stages and the
stereo output shaper, reducing fold-back artifacts without an oversampled audio
path. The undelayed linear component is preserved so quiet hits keep their
transient definition.

The Kick has a dedicated charged-energy model: a virtual capacitor discharges
into a contractive two-state resonator whose frequency and loss change with the
stored energy. Its default body settles around 48 Hz, while **Punch** controls
the initial pitch movement and contact noise and **Drive** moves the nonlinear
operating point, branch mismatch, harmonic density, and modest makeup gain. The
resonator update is an explicit rotation followed by contraction, so even rapid
pitch modulation cannot inject unbounded state energy.

These are circuit-inspired behavioral models, not a claim of
component-for-component emulation of a TR-808, TR-909, or another specific
machine. No neural-network weights are needed, so the audio path stays
allocation-free, deterministic, and suitable for real-time use.

All synthesis happens in the audio callback without sample files. The engine is
prepared for the host sample rate, accepts sample-accurate MIDI event offsets,
and clears completed one-shot voices after their tails finish.

## Vintage interface

The editor combines an original aged-enamel faceplate texture with code-drawn
Bakelite-style knobs, calibration marks, rubber pads, hardware details, and
accessible focus states. The texture is compiled into the plug-in as binary data;
there is no external image file to install or locate at runtime. The visual
direction is intentionally era-inspired rather than a copy of any historical
drum machine's panel or trade dress.

## Research influences and modeling scope

The implementation follows recent virtual-analog work where it fits a
self-contained real-time instrument:

- Gabrielli and Squartini's [2025 ADAA study](https://www.dafx.de/paper-archive/2025/DAFx25_paper_30.pdf)
  motivates antiderivative treatment of nonlinear stages as a lower-cost route
  to reduced aliasing.
- Pines' [2025 diode-VCA model](https://dafx25.dii.univpm.it/wp-content/uploads/2025/07/DAFx25_paper_44.pdf)
  motivates explicit fixed nonlinearities with variable operating points.
- Werner, Abel, and Smith's [physically informed bass-drum analysis](https://dafx.de/paper-archive/2014/dafx14_kurt_james_werner_a_physically_informed%2C_ci.pdf)
  and Germain's [time-varying numerical study](https://www.dafx.de/paper-archive/2021/proceedings/papers/DAFx20in21_paper_43.pdf)
  motivate charged state, resonant feedback, changing pitch/loss, and stable
  time-varying updates for the Kick.
- Esqueda and Murai's [2025 antialiased recurrent model](https://dafx25.dii.univpm.it/wp-content/uploads/2025/09/DAFx25_paper_61.pdf)
  shows that compact learned state-space models can run in real time. Drumalor
  deliberately does not use one: without measurements from a defined target
  circuit, weights would be an uncalibrated black box rather than a more
  defensible analog model.

The result is a modern behavioral VA design with original sound architecture,
not a calibrated hardware replica. Listening comparisons and profiling on the
oldest supported Mac remain part of release qualification even though the
automated stability, performance, and spectral contracts pass.

## Requirements

- macOS 11 or newer for running the built products
- A current full Xcode installation selected for command-line use
- CMake 3.22 or newer
- Git and internet access for the default first configure, or a local JUCE
  8.0.14 checkout supplied with `DRUMALOR_JUCE_PATH`

JUCE 8.0.14 is fetched at configure time and is not vendored into this
repository.

## Build on macOS

The helper creates an Xcode build, compiles universal `arm64`/`x86_64`
binaries, and runs both the DSP and JUCE processor-contract tests:

```bash
./scripts/build-macos.sh
```

Equivalent commands, useful when opening and developing in Xcode, are:

```bash
cmake -S . -B build-macos -G Xcode \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DDRUMALOR_BUILD_UNIVERSAL=ON \
  -DDRUMALOR_BUILD_PLUGIN=ON \
  -DBUILD_TESTING=ON

cmake --build build-macos --config Release --parallel
ctest --test-dir build-macos -C Release --output-on-failure
open build-macos/Drumalor.xcodeproj
```

To avoid the FetchContent download, point the configure at a local checkout of
the exact JUCE release:

```bash
JUCE_PATH="$HOME/SDKs/JUCE-8.0.14" ./scripts/build-macos.sh
```

For a native-only development build, use `BUILD_UNIVERSAL=OFF`. Override
`BUILD_DIR`, `CONFIG`, or `MACOSX_DEPLOYMENT_TARGET` in the environment when
needed.

Release bundles are written to:

| Format | Build artifact |
| --- | --- |
| VST3 | `build-macos/Drumalor_artefacts/Release/VST3/Drumalor.vst3` |
| Audio Unit | `build-macos/Drumalor_artefacts/Release/AU/Drumalor.component` |
| Standalone | `build-macos/Drumalor_artefacts/Release/Standalone/Drumalor.app` |

## Run the DSP tests without JUCE

The synthesis core deliberately has no JUCE dependency. This provides a quick
test path on any C++20 development machine without downloading the framework:

```bash
cmake -S . -B build-dsp \
  -DCMAKE_BUILD_TYPE=Release \
  -DDRUMALOR_BUILD_PLUGIN=OFF \
  -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The JUCE-free regression executable renders every voice from 8 to 192 kHz. It
checks finite, non-silent, bounded output, completed tails, hi-hat choking, all
four controls on every voice, sample-rate consistency, saturated voice stealing,
and a generous offline performance guardrail. Organic-model contracts verify
that six equal strikes differ for all 13 voices while RMS, peak, and natural-tail
spread stay bounded; they also verify bit-exact reset replay and block-partition
invariance. Kick-specific contracts cover a 43–55 Hz settled body, dominant
sub-100 Hz energy, controlled transient and crest factor, Drive harmonics without
excess settled energy above 8 kHz, pitch tracking, DC safety, and consistency
from 8 to 192 kHz. Plug-in builds add a JUCE-backed processor contract suite for
parameter defaults and state, sample-accurate MIDI, CC panic, the UI-trigger
lifecycle, and off-screen rendering of the embedded vintage editor. These checks
do not replace listening tests, host automation tests, or profiling on the oldest
supported Mac.

## Install locally

For per-user installation, copy only the formats you need:

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"

ditto build-macos/Drumalor_artefacts/Release/VST3/Drumalor.vst3 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Drumalor.vst3"
ditto build-macos/Drumalor_artefacts/Release/AU/Drumalor.component \
  "$HOME/Library/Audio/Plug-Ins/Components/Drumalor.component"
```

Standard discovery locations are:

| Scope | VST3 | Audio Unit |
| --- | --- | --- |
| Current user | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |
| All users | `/Library/Audio/Plug-Ins/VST3/` | `/Library/Audio/Plug-Ins/Components/` |

The standalone app can be copied to `/Applications` or launched directly from
the artifacts directory. Quit and reopen the host after installing. If Logic
retains an older AU during development, log out and back in or restart the
Audio Component Registrar before rescanning.

## Validate the plug-in

Run the DSP tests first, then check that each release executable is universal:

```bash
lipo -archs build-macos/Drumalor_artefacts/Release/VST3/Drumalor.vst3/Contents/MacOS/Drumalor
lipo -archs build-macos/Drumalor_artefacts/Release/AU/Drumalor.component/Contents/MacOS/Drumalor
lipo -archs build-macos/Drumalor_artefacts/Release/Standalone/Drumalor.app/Contents/MacOS/Drumalor
```

Each command should report both `arm64` and `x86_64`. After installing the AU,
validate its type `aumu`, subtype `Drm1`, and manufacturer `Dral`:

```bash
auval -v aumu Drm1 Dral
```

With [pluginval](https://github.com/Tracktion/pluginval) installed, validate
the VST3 at the highest strictness level:

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 10 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Drumalor.vst3"
```

Also exercise all 13 note mappings, velocity extremes, rapid retriggers, the
open/closed-hat choke, all 52 voice parameters, project-state recall, sample-
rate changes, and buffer sizes from 32 to 2048 samples in at least two hosts.
A validator passing does not guarantee musical or host-level correctness.

## Sign, package, and notarize

For local testing, the packaging helper uses ad-hoc signing by default:

```bash
./scripts/sign-and-package-macos.sh
```

It stages the VST3, AU, and standalone app, verifies their signatures, and
creates a ZIP and installer package under `build-macos/dist/`. The default
universal build uses the `macOS-universal` filename suffix; a native-only build
is labelled with its actual architecture instead.

For public distribution, first import valid `Developer ID Application` and
`Developer ID Installer` certificates. Store notarization credentials once in
the login keychain; do not put credentials in this repository:

```bash
xcrun notarytool store-credentials drumalor-notary \
  --apple-id "developer@example.com" \
  --team-id "YOURTEAMID" \
  --password "APP-SPECIFIC-PASSWORD"
```

Then sign, package, submit, wait for Apple's result, and staple the ticket:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Company (YOURTEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Company (YOURTEAMID)" \
NOTARY_PROFILE="drumalor-notary" \
./scripts/sign-and-package-macos.sh
```

With `NOTARY_PROFILE` set, the helper submits and staples the installer package.
The ZIP is still produced, but it is not the notarized distribution artifact;
publish the `.pkg`, or run a separate bundle/ZIP notarization workflow before
distributing the ZIP.

Before publishing, verify the package from a clean user account and inspect its
signature and Gatekeeper assessment:

```bash
pkgutil --check-signature build-macos/dist/Drumalor-1.0.0-macOS-universal.pkg
spctl --assess --type install --verbose=4 \
  build-macos/dist/Drumalor-1.0.0-macOS-universal.pkg
```

The bundle identifier `audio.drumalor.synth`, manufacturer code `Dral`, and
plug-in code `Drm1` are the host-facing identity. Confirm that the publisher
controls them before the first public release, then never change them: hosts
use these values to associate saved projects with the correct plug-in. Keep the
CMake project version and packaging-script version in sync for each release.

## Project layout

```text
Source/DSP/              JUCE-free synthesis engine and voice metadata
Source/PluginProcessor.* MIDI mapping, parameters, state, and audio bridge
Source/PluginEditor.*    Thirteen-pad editor and four-knob voice controls
Assets/                  Embedded original vintage faceplate texture
Tests/                   DSP and JUCE processor-contract regression tests
Presets/                 Preset guidance and future factory presets
scripts/                 macOS build and release helpers
```

## Licensing

The original Drumalor source is offered under the MIT License. JUCE is a
separate dependency and is not covered by that licence. JUCE 8 framework
modules are available under the AGPLv3 or a commercial JUCE licence.
Distributing a closed-source or otherwise AGPL-incompatible binary generally
requires an appropriate commercial JUCE licence. Confirm the current terms for
the publisher and use case before shipping; see `THIRD_PARTY_NOTICES.md` and
JUCE's official licence.

No drum samples, impulse responses, neural model weights, factory ROMs, or
third-party presets are included.
