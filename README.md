# VST Instruments

[![CI](https://github.com/voho/vst-instruments/actions/workflows/ci.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/ci.yml)
[![Nightly](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml)

A collection of five original macOS instruments built on one shared technical
contract: JUCE 8.0.14, CMake 3.22+, C++20, CTest, and VST3, Audio Unit, and
Standalone targets. Each instrument remains a self-contained project with its
own DSP core, interface, tests, release helpers, and documentation.

Vocalor, Drumalor, Mars, and Electry generate sound procedurally without
loading samples. Neuramar instead learns a compact synthesis model from audio
supplied by its user, then renders that model without replaying the recording.
All five engines run locally, contact no service while rendering, and ship
without factory samples, pretrained neural weights, or third-party preset
libraries.

| [Vocalor](vocalor/) | [Drumalor](drumalor/) | [Mars](mars/) | [Neuramar](neuramar/) | [Electry](electry/) |
| :---: | :---: | :---: | :---: | :---: |
| [![Vocalor standalone interface](vocalor/Docs/screenshots/vocalor-standalone.png)](vocalor/README.md) | [![Drumalor standalone interface](drumalor/Docs/screenshots/drumalor-standalone.png)](drumalor/README.md) | [![Mars standalone interface](mars/Docs/screenshots/mars-standalone.png)](mars/README.md) | [![Neuramar standalone interface](neuramar/Docs/screenshots/neuramar-standalone.png)](neuramar/README.md) | [![Electry standalone interface](electry/Docs/screenshots/electry-standalone.png)](electry/README.md) |

## Instruments

| Instrument | Description | Formats | Platform | Docs |
| --- | --- | --- | --- | --- |
| [Vocalor](vocalor/) | Source-filter vocal and choir synthesizer with solo, ensemble, and chord performance modes across three sustained vowels. | VST3 · AU · Standalone | macOS 11+ | [README](vocalor/README.md) |
| [Drumalor](drumalor/) | Thirteen-voice procedural drum synthesizer with deterministic organic variation, circuit-inspired colour, and GM-oriented MIDI mapping. | VST3 · AU · Standalone | macOS 11+ | [README](drumalor/README.md) |
| [Mars](mars/) | Dual-oscillator virtual-analog polysynth with switchable VCO mixer feeds, measured saw contouring, nonlinear Moog-ladder and SEM-inspired filters, deterministic voice cards, 42 direct sound controls, and persisted HQ oversampling. | VST3 · AU · Standalone | macOS 11+ | [README](mars/README.md) |
| [Neuramar](neuramar/) | Drop in a mostly monophonic sound, infer its root, and fit a compact local DDSP-inspired neural synthesis model whose harmonic Core, noisy Air, and resonant Bone remain playable across pitches. | VST3 · AU · Standalone | macOS 11+ | [README](neuramar/README.md) |
| [Electry](electry/) | Oversampled physically modeled Drop-E eight-string guitar: eight dual-polarisation waveguides, fitted stiff-string dispersion, induced-EMF pickups, modal body loss, Mono/divided-pickup Stereo, and 16 labeled keyswitched play styles. | VST3 · AU · Standalone | macOS 11+ | [README](electry/README.md) |

## Download (nightly)

The scheduled and manually dispatchable Nightly workflow builds and tests all
five instruments as universal `arm64`/`x86_64` binaries. After every build,
test, package, and ten-file manifest check succeeds, it uploads a uniquely
named complete set before switching the single rolling
**[nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly)**.
Check the Nightly badge above for the latest workflow result before downloading.

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
xattr -dr com.apple.quarantine \
  Library/Audio/Plug-Ins/VST3/Neuramar.vst3 \
  Library/Audio/Plug-Ins/Components/Neuramar.component \
  Applications/Neuramar.app
xattr -dr com.apple.quarantine \
  Library/Audio/Plug-Ins/VST3/Electry.vst3 \
  Library/Audio/Plug-Ins/Components/Electry.component \
  Applications/Electry.app
```

For public distribution, build from source with your own Developer ID signing
and notarization. Each instrument README contains its own distribution guide:
[Vocalor](vocalor/README.md#sign-package-and-notarize),
[Drumalor](drumalor/README.md#sign-package-and-notarize),
[Mars](mars/README.md#sign-package-and-notarize),
[Neuramar](neuramar/README.md#sign-package-and-notarize), and
[Electry](electry/README.md#sign-package-and-notarize).

## Building

There is no top-level CMake target: build each self-contained instrument from its
own directory. All five helpers use the same Xcode/CMake/JUCE toolchain,
compile universal `arm64`/`x86_64` binaries, run the instrument's CTest suite,
and write VST3, Audio Unit, and Standalone bundles below that instrument's
`build-macos/` directory. Drumalor, Mars, Neuramar, and Electry additionally
include JUCE processor contract tests; Vocalor currently exercises its
JUCE-free DSP suite.

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

**Neuramar** ([full instructions](neuramar/README.md#build-and-test-on-macos)):

```bash
cd neuramar
./scripts/build-macos.sh
```

**Electry** ([full instructions](electry/README.md#build-on-macos)):

```bash
cd electry
./scripts/build-macos.sh
```

Full plug-in builds require CMake 3.22+ and a full Xcode installation selected
for command-line use. The JUCE-free DSP targets need only a C++20 toolchain and
CMake, which is the path exercised by Linux CI. First-time plug-in configuration
also needs internet access to fetch JUCE 8.0.14; a local checkout of that exact
release can instead be supplied through each build script's `JUCE_PATH` variable.

## Continuous integration

The two GitHub Actions workflows cover every instrument explicitly:

- **CI** runs the JUCE-free DSP build and tests on Linux, plus a native-architecture
  macOS plug-in build and CTest run, for every pull request and push to `main`.
- **Nightly** runs the same macOS helpers in universal mode, packages ZIP and
  PKG artifacts containing ad-hoc-signed bundles, retains a combined workflow
  artifact for 14 days, and preserves the prior complete rolling-release set
  until all eight uniquely named replacement assets have uploaded.

The JUCE source dependency is pinned to an immutable 8.0.14 archive and SHA-256
checksum in every project. Runner images and Xcode are supplied by GitHub Actions,
so the workflow badges remain the source of truth for the current hosted build.

## Repository layout

```text
LICENSE       Repository license (Apache-2.0)
vocalor/      Vocalor vocal and choir synthesizer (self-contained JUCE project)
drumalor/     Drumalor thirteen-voice drum synthesizer (self-contained JUCE project)
mars/         Mars nonlinear virtual-analog polysynth (self-contained JUCE project)
neuramar/     Neuramar sample-learned neural synthesizer (self-contained JUCE project)
electry/      Electry physically modeled dry electric guitar (self-contained JUCE project)
.github/      Per-push CI and universal Nightly release workflows
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
- **Neuramar** — original source under the [MIT License](neuramar/LICENSE); see
  its [third-party notices](neuramar/THIRD_PARTY_NOTICES.md).
- **Electry** — original source under the [MIT License](electry/LICENSE); see
  its [third-party notices](electry/THIRD_PARTY_NOTICES.md).

All instruments build against JUCE, which is not covered by those MIT
licences. JUCE 8 is dual-licensed under AGPLv3 or a commercial JUCE licence, so
confirm the applicable terms before distributing a binary.

No pretrained neural-network weights, voice datasets, factory samples, impulse
responses, or third-party preset libraries are included in this repository.
Neuramar accepts user audio at runtime and stores a compact derived synthesis
model in host state; it does not embed or replay the imported recording.
