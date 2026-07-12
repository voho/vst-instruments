# vst-instruments

[![CI](https://github.com/voho/vst-instruments/actions/workflows/ci.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/ci.yml)
[![Nightly](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml)

A collection of original audio plug-in instruments. Each instrument lives in its
own sub-directory as a self-contained project with its own source, build system,
tests, and documentation.

Every instrument here is **procedural and original**: the synthesis runs locally
and does not clone a named artist, load recordings, or contact a service while
rendering audio.

## Instruments

| Instrument | Description | Formats | Platform | Docs |
| --- | --- | --- | --- | --- |
| [Vocalor](vocalor/) | Real-time vocal & choir synthesizer — turns MIDI notes into expressive `aah` / `ooh` / `uuh` voices, from a single singer to an ensemble or major/minor chord. | VST3 · AU · Standalone | macOS 11+ | [README](vocalor/README.md) |

## Download (nightly)

Prebuilt macOS bundles are published automatically from `main` to a single
rolling **[nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly)**,
so the latest build is always one click away — no need to compile anything.

These bundles are **ad-hoc signed and not notarized**, so Gatekeeper will warn.
After unzipping, clear the quarantine flag:

```bash
xattr -dr com.apple.quarantine Vocalor.vst3 Vocalor.component Vocalor.app
```

For anything beyond local testing, build from source (below) with your own
Developer ID signing and notarization — see the
[distribution guide](vocalor/README.md#sign-package-and-notarize).

## Building

There is no top-level build. Build each instrument from its own directory using
the instructions in that instrument's README.

**Vocalor** (macOS, [full instructions](vocalor/README.md#build-on-macos)):

```bash
cd vocalor
./scripts/build-macos.sh
```

This configures an Xcode build, compiles universal `arm64`/`x86_64` binaries,
runs the DSP tests, and produces the VST3, Audio Unit, and Standalone bundles
under `vocalor/build-macos/`. It requires CMake 3.22+, the Xcode command-line
tools, and (on first configure) internet access to fetch JUCE 8.0.14.

## Repository layout

```text
LICENSE      Repository license (Apache-2.0)
vocalor/     Vocalor — vocal & choir synthesizer (self-contained JUCE/CMake project)
```

New instruments are added as additional top-level directories and linked from
the table above.

## Licensing

This repository is licensed under the **Apache License 2.0** (see [LICENSE](LICENSE)).

Individual instruments may carry their own license and third-party notices:

- **Vocalor** — original source under the [MIT License](vocalor/LICENSE);
  see its [third-party notices](vocalor/THIRD_PARTY_NOTICES.md). It builds
  against [JUCE](https://juce.com/), which is **not** covered by that license —
  JUCE 8 is dual-licensed under AGPLv3 or a commercial JUCE licence, so confirm
  the applicable terms before distributing a binary.

No neural-network model weights, voice datasets, or third-party sample libraries
are included in this repository.
