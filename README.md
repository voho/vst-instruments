# vst-instruments

[![CI](https://github.com/voho/vst-instruments/actions/workflows/ci.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/ci.yml)
[![Nightly](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml)

A collection of original audio plug-in instruments. Each instrument lives in its
own sub-directory as a self-contained project with its own source, build system,
tests, and documentation.

Every instrument here is **procedural and original**: synthesis runs locally
without loading samples, cloning a named artist or product, or contacting a
service while rendering audio.

## Instruments

| Instrument | Description | Formats | Platform | Docs |
| --- | --- | --- | --- | --- |
| [Vocalor](vocalor/) | Real-time vocal and choir synthesizer: expressive `aah`, `ooh`, and `uuh` voices from a single singer to an ensemble or chord. | VST3 · AU · Standalone | macOS 11+ | [README](vocalor/README.md) |
| [Drumalor](drumalor/) | Thirteen-voice synthesized drum instrument with kick, snare, clap, two hi-hats, ride, crash, three toms, shaker, and two percussion voices. | VST3 · AU · Standalone | macOS 11+ | [README](drumalor/README.md) |
| [Mars](mars/) | Vintage component-modeled polysynth with direct knobs/sliders, no arpeggiator, oscillator-card drift, analog filter color, and original Mars styling. | VST3 · AU · Standalone | macOS 11+ | [README](mars/README.md) |

## Download (nightly)

Prebuilt macOS bundles for both instruments are published automatically from
`main` to a single rolling
**[nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly)**,
so the latest build is always available without compiling it locally.

These bundles are ad-hoc signed and not notarized, so Gatekeeper will warn.
After unzipping, clear the quarantine flag for the instrument you want to run:

```bash
xattr -dr com.apple.quarantine \
  Library/Audio/Plug-Ins/VST3/Vocalor.vst3 \
  Library/Audio/Plug-Ins/Components/Vocalor.component \
  Applications/Vocalor.app
xattr -dr com.apple.quarantine \
  Library/Audio/Plug-Ins/VST3/Drumalor.vst3 \
  Library/Audio/Plug-Ins/Components/Drumalor.component \
  Applications/Drumalor.app
xattr -dr com.apple.quarantine \
  Library/Audio/Plug-Ins/VST3/Mars.vst3 \
  Library/Audio/Plug-Ins/Components/Mars.component \
  Applications/Mars.app
```

For public distribution, build from source with your own Developer ID signing
and notarization. Each instrument README contains its own distribution guide:
[Vocalor](vocalor/README.md#sign-package-and-notarize) and
[Drumalor](drumalor/README.md#sign-package-and-notarize), and
[Mars](mars/README.md#sign-package-and-notarize).

## Building

There is no top-level build. Build each instrument from its own directory.
Both helpers configure Xcode, compile universal `arm64`/`x86_64` binaries, run
the instrument's CTest suite (including Drumalor's JUCE processor contracts),
and write VST3, Audio Unit, and Standalone bundles under that instrument's
`build-macos/` directory.

**Vocalor** ([full instructions](vocalor/README.md#build-on-macos)):

```bash
cd vocalor
./scripts/build-macos.sh
```

**Drumalor** ([full instructions](drumalor/README.md#build-on-macos)):

```bash
cd drumalor
./scripts/build-macos.sh
```

**Mars** ([full instructions](mars/README.md#build-on-macos)):

```bash
cd mars
./scripts/build-macos.sh
```

All projects require CMake 3.22+, a full Xcode installation selected for
command-line use, and internet access on first configure to fetch JUCE 8.0.14.
A local checkout of that exact JUCE release can be supplied through each build
script's `JUCE_PATH` variable.

## Repository layout

```text
LICENSE       Repository license (Apache-2.0)
vocalor/      Vocalor vocal and choir synthesizer (self-contained JUCE project)
drumalor/     Drumalor thirteen-voice drum synthesizer (self-contained JUCE project)
mars/         Mars vintage component-modeled polysynth (self-contained JUCE project)
```

New instruments are added as additional top-level directories and linked from
the table above.

## Licensing

This repository is licensed under the **Apache License 2.0** (see
[LICENSE](LICENSE)). Individual instruments also carry their own source licence
and third-party notices:

- **Vocalor** — original source under the [MIT License](vocalor/LICENSE); see
  its [third-party notices](vocalor/THIRD_PARTY_NOTICES.md).
- **Drumalor** — original source under the [MIT License](drumalor/LICENSE); see
  its [third-party notices](drumalor/THIRD_PARTY_NOTICES.md).
- **Mars** — original source under the [MIT License](mars/LICENSE); see
  its [third-party notices](mars/THIRD_PARTY_NOTICES.md).

All instruments build against JUCE, which is not covered by those MIT
licences. JUCE 8 is dual-licensed under AGPLv3 or a commercial JUCE licence, so
confirm the applicable terms before distributing a binary.

No neural-network model weights, voice datasets, samples, impulse responses,
or third-party preset libraries are included in this repository.
