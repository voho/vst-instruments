# Drumalor

Drumalor is a real-time procedural drum instrument for macOS. It uses classic
analogue-drum synthesis techniques as a starting point, then generates every
hit locally from oscillators, noise, filters, envelopes, and nonlinear shaping.
It does not load samples, copy a ROM, model a particular machine circuit, or
contact a service while rendering audio.

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

All synthesis happens in the audio callback without sample files. The engine is
prepared for the host sample rate, accepts sample-accurate MIDI event offsets,
and clears completed one-shot voices after their tails finish.

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
and a generous offline performance guardrail. Plug-in builds add a JUCE-backed
processor contract suite for parameter defaults and state, sample-accurate MIDI,
CC panic, and the UI-trigger lifecycle. These checks do not replace listening
tests, host automation tests, or profiling on the oldest supported Mac.

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
