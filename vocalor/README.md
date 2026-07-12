# Vocalor

Vocalor is a real-time vocal instrument for macOS. It turns MIDI notes into
original `aah`, `ooh`, and `uuh` voices, from a single singer to an ensemble or
major/minor chord. The engine is procedural and runs locally: it does not clone
a named singer, load recordings, or contact a service while rendering audio.

The project builds three products from one JUCE codebase:

- VST3 instrument for hosts such as Ableton Live, REAPER, Cubase, and Bitwig
- Audio Unit v2 music device for Logic Pro and GarageBand
- Standalone application for direct MIDI-keyboard testing

> **Just want to try it?** Prebuilt (ad-hoc signed, un-notarized) macOS bundles
> are published for every day's `main` on the
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> To build from source instead, continue below.

## Sound engine

The JUCE-free C++20 DSP core uses a low-latency, source-filter vocal model. A
fast onset is available immediately, then the richer tract and noise detail is
introduced over the first fraction of a note. Each singer has slowly moving
pitch, vibrato rate and depth, spectral balance, breath, and timing rather than
sharing a perfectly periodic LFO. Ensemble and chord modes instantiate
independent singers, preventing the phase-locked oscillator sound common to
simple choir patches.

This is a synthesizer, not a speech model or voice-cloning system. It is suited
to sustained vowels and expressive musical parts; it does not generate words.

## Requirements

- macOS 11 or newer for running the built products
- A current Xcode command-line toolchain with a macOS SDK
- CMake 3.22 or newer
- Git and internet access for the default first configure, or a local JUCE
  8.0.14 checkout supplied with `VOCALOR_JUCE_PATH`

JUCE 8.0.14 is fetched at configure time and is not vendored into this
repository.

## Build on macOS

The helper creates an Xcode build, compiles universal `arm64`/`x86_64`
binaries, and runs the DSP tests:

```bash
./scripts/build-macos.sh
```

Equivalent commands, useful when opening and developing in Xcode, are:

```bash
cmake -S . -B build-macos -G Xcode \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DVOCALOR_BUILD_UNIVERSAL=ON \
  -DVOCALOR_BUILD_PLUGIN=ON \
  -DBUILD_TESTING=ON

cmake --build build-macos --config Release --parallel
ctest --test-dir build-macos -C Release --output-on-failure
open build-macos/Vocalor.xcodeproj
```

To avoid the FetchContent download, point the configure at a local checkout of
the exact JUCE release:

```bash
JUCE_PATH="$HOME/SDKs/JUCE-8.0.14" ./scripts/build-macos.sh
```

For a native-only development build, use `BUILD_UNIVERSAL=OFF`. Override
`BUILD_DIR`, `CONFIG`, or `MACOSX_DEPLOYMENT_TARGET` in the environment when
needed.

The Release bundles are written to:

| Format | Build artifact |
| --- | --- |
| VST3 | `build-macos/Vocalor_artefacts/Release/VST3/Vocalor.vst3` |
| Audio Unit | `build-macos/Vocalor_artefacts/Release/AU/Vocalor.component` |
| Standalone | `build-macos/Vocalor_artefacts/Release/Standalone/Vocalor.app` |

## Run the DSP tests without JUCE

The core is deliberately independent of JUCE. This gives a quick test path on
any C++20 development machine without downloading the application framework:

```bash
cmake -S . -B build-dsp \
  -DVOCALOR_BUILD_PLUGIN=OFF \
  -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The test executable renders solo, choir, major-chord, and minor-chord notes at
44.1, 48, and 96 kHz. It checks that held and released notes produce finite,
non-silent audio, that the release decays, and that a short offline render does
not exceed a generous performance guardrail. This catches regressions; it is
not a substitute for listening tests, host automation tests, or profiling on
the oldest supported Mac.

## Install locally

For per-user installation, copy only the formats you need:

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"

ditto build-macos/Vocalor_artefacts/Release/VST3/Vocalor.vst3 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Vocalor.vst3"
ditto build-macos/Vocalor_artefacts/Release/AU/Vocalor.component \
  "$HOME/Library/Audio/Plug-Ins/Components/Vocalor.component"
```

Standard discovery locations are:

| Scope | VST3 | Audio Unit |
| --- | --- | --- |
| Current user | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |
| All users | `/Library/Audio/Plug-Ins/VST3/` | `/Library/Audio/Plug-Ins/Components/` |

The standalone app can be copied to `/Applications` or launched directly from
the artifacts directory. Quit and reopen the host after installing. If Logic
retains an older AU cache during development, log out and back in or restart
the Audio Component Registrar before rescanning.

## Validate the plug-in

Run the DSP tests first, then validate the actual bundles. The AU component IDs
configured by this project are type `aumu`, subtype `Vcl1`, and manufacturer
`Vclr`:

```bash
auval -v aumu Vcl1 Vclr
```

With [pluginval](https://github.com/Tracktion/pluginval) installed, validate
the VST3 at the highest strictness level:

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 10 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Vocalor.vst3"
```

Also test note overlap, rapid note-off, parameter automation, project-state
recall, sample-rate changes, and buffer sizes from 32 to 2048 samples in at
least two hosts. A validator passing does not guarantee musical or host-level
correctness.

## Sign, package, and notarize

For local testing, the packaging helper uses ad-hoc signing by default:

```bash
./scripts/sign-and-package-macos.sh
```

It stages the VST3, AU, and standalone app, verifies their signatures, and
creates a ZIP and installer package under `build-macos/dist/`.

For public distribution, first import valid `Developer ID Application` and
`Developer ID Installer` certificates. Store notarization credentials once in
the login keychain; do not put credentials in this repository:

```bash
xcrun notarytool store-credentials vocalor-notary \
  --apple-id "developer@example.com" \
  --team-id "YOURTEAMID" \
  --password "APP-SPECIFIC-PASSWORD"
```

Then sign, package, submit, wait for Apple's result, and staple the ticket in
one command:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Company (YOURTEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Company (YOURTEAMID)" \
NOTARY_PROFILE="vocalor-notary" \
./scripts/sign-and-package-macos.sh
```

Before publishing, verify the package from a clean user account and inspect it:

```bash
pkgutil --check-signature build-macos/dist/Vocalor-1.0.0-macOS-universal.pkg
spctl --assess --type install --verbose=4 \
  build-macos/dist/Vocalor-1.0.0-macOS-universal.pkg
```

The placeholder bundle identifier and four-character manufacturer/plugin codes
must be replaced with identifiers controlled by the publisher before a public
release. Once released, do not change those identifiers: hosts use them to
recall the correct plug-in.

## Project layout

```text
Source/DSP/              JUCE-free synthesis engine
Source/PluginProcessor.* MIDI, parameters, state, and audio bridge
Source/PluginEditor.*    Keyboard and editor UI
Tests/                   Standalone DSP regression tests
Presets/                 Preset guidance and future factory presets
scripts/                 macOS build and release helpers
```

## Licensing

The original Vocalor source is offered under the MIT License. JUCE is a separate
dependency and is **not** covered by that license. JUCE 8 framework modules are
available under the AGPLv3 or a commercial JUCE licence. Distributing a closed-
source or otherwise AGPL-incompatible binary generally requires an appropriate
commercial JUCE licence. Confirm the current terms for the publisher and use
case before shipping; see `THIRD_PARTY_NOTICES.md` and JUCE's official licence.

No neural model weights, voice datasets, samples, or third-party presets are
included. If those are added later, document their provenance and redistribution
rights before committing or packaging them.
