# VST Instruments

[![CI](https://github.com/voho/vst-instruments/actions/workflows/ci.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/ci.yml)
[![Nightly](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml)

A collection of six original macOS instruments built on one shared technical
contract: JUCE 8.0.14, CMake 3.22+, C++20, CTest, and VST3, Audio Unit, and
Standalone targets. Each instrument remains a self-contained project with its
own DSP core, interface, tests, release helpers, and documentation.

Vocalor, Drumalor, Electry, Taikor, and YouKnow106 generate sound procedurally
without loading samples. Neuramar instead learns a compact synthesis model from
audio supplied by its user, then renders that model without replaying the
recording. All six engines run locally, contact no service while rendering, and
ship without factory samples, pretrained neural weights, or third-party preset
libraries.

| [Vocalor](vocalor/) | [Drumalor](drumalor/) | [Neuramar](neuramar/) | [Electry](electry/) | [Taikor](taikor/) | [YouKnow106](youknow106/) |
| :---: | :---: | :---: | :---: | :---: | :---: |
| [![Vocalor standalone interface](vocalor/Docs/screenshots/vocalor-standalone.png)](vocalor/README.md)<br>[9 rendered demos](vocalor/Docs/audio/README.md) | [![Drumalor standalone interface](drumalor/Docs/screenshots/drumalor-standalone.png)](drumalor/README.md)<br>[7 rendered demos](drumalor/Docs/audio/README.md) | [![Neuramar standalone interface](neuramar/Docs/screenshots/neuramar-standalone.png)](neuramar/README.md)<br>[8 rendered demos](neuramar/Docs/audio/README.md) | [![Electry standalone interface](electry/Docs/screenshots/electry-standalone.png)](electry/README.md) | [**Taikor** — screenshot lands with the first Nightly](taikor/README.md)<br>[23 rendered demos](taikor/Docs/audio/README.md) | [**YouKnow106** — screenshot lands with the first Nightly](youknow106/README.md)<br>[10 rendered demos](youknow106/Docs/audio/README.md) |

Each screenshot above is rendered by its instrument's own regression suite
during the Nightly workflow's macOS build and committed automatically when the
editor has changed, so the images track the real editors — all six suites now
render one. Taikor and YouKnow106 are new: their cells above stay text links
until the first Nightly commits their images. The Nightly also re-renders and
commits every instrument's demonstration audio the same way, from the same
JUCE-free engines the plug-ins run:
[Vocalor](vocalor/Docs/audio/README.md),
[Drumalor](drumalor/Docs/audio/README.md),
[Neuramar](neuramar/Docs/audio/README.md),
[Electry](electry/Docs/audio/README.md),
[Taikor](taikor/Docs/audio/README.md), and
[YouKnow106](youknow106/Docs/audio/README.md).

## Instruments

| Instrument | Description | Formats | Platform | Docs |
| --- | --- | --- | --- | --- |
| [Vocalor](vocalor/) | Source-filter vocal and choir synthesizer with an LF-style glottal source, a continuous morphable vowel space, formant shifting independent of pitch, and legato phrasing across solo, ensemble, and chord modes. [Rendered demos.](vocalor/Docs/audio/README.md) | VST3 · AU · Standalone | macOS 11+ | [README](vocalor/README.md) |
| [Drumalor](drumalor/) | Thirteen-voice procedural drum synthesizer with struck-membrane modal models, velocity-dependent timbre, per-voice level/pan/choke mixing, humanised organic variation, a shared drive and compression bus, and GM-oriented MIDI mapping. [Rendered demos.](drumalor/Docs/audio/README.md) | VST3 · AU · Standalone | macOS 11+ | [README](drumalor/README.md) |
| [Neuramar](neuramar/) | Drop in a mostly monophonic sound, infer its root, and fit a compact local DDSP-inspired neural synthesis model — with fitted stiff-string inharmonicity, formant shifting, and velocity-driven timbre — whose harmonic Core, noisy Air, and resonant Bone remain playable across pitches. [Rendered demos.](neuramar/Docs/audio/README.md) | VST3 · AU · Standalone | macOS 11+ | [README](neuramar/README.md) |
| [Taikor](taikor/) | Physically modeled taiko: a struck circular membrane solved from Bessel-zero modes with air loading, two heads coupled through the enclosed body, a thin-cylinder wooden shell, and a Hertz stick contact whose duration follows impact speed. Twelve kumi-daiko strokes per octave, the octave selecting the drum from odaiko to shime-daiko, and a stereo close pair whose image comes from evanescent near-field decay rather than a widener. [Rendered demos.](taikor/Docs/audio/README.md) | VST3 · AU · Standalone | macOS 11+ | [README](taikor/README.md) |
| [YouKnow106](youknow106/) | Circuit-modelled six-voice DCO polysynth: integer-divided note timers with their real pitch quantisation, a scanned control converter walking the voices with per-hold slew and 7-bit patch digitisation, firmware envelopes with exponential falling segments into a measured quasi-linear amplifier, a four-pole transconductor filter with its divider-limited resonance loop solved implicitly and checked against a Runge-Kutta solve of the same circuit, input-side resonance compensation, a key assigner that drops notes rather than stealing them, and an uncompanded two-line bucket-brigade chorus with the line's own saturation. [Claims boundary.](youknow106/Docs/circuit-modelling-research.md) [Rendered demos.](youknow106/Docs/audio/README.md) | VST3 · AU · Standalone | macOS 11+ | [README](youknow106/README.md) |
| [Electry](electry/) | Oversampled physically modeled Drop-E eight-string guitar: eight dual-polarisation waveguides, fitted stiff-string dispersion, bridge-coupled sympathetic strings, a continuous palm mute, strum travel, induced-EMF pickups, modal body loss, Mono/divided-pickup Stereo, two independent keyswitch banks (three pick strokes against four play styles), a pitch wheel that bends every string like a vibrato bar, a resonance wheel that pushes a distorted tone into self-sustaining amplifier feedback, and a 4x-oversampled amplifier and modelled cabinet. [Rendered demos.](electry/Docs/audio/README.md) | VST3 · AU · Standalone | macOS 11+ | [README](electry/README.md) |

## Download (nightly)

The scheduled and manually dispatchable Nightly workflow builds and tests all
six instruments as universal `arm64`/`x86_64` binaries. After every build,
test, package, and twelve-file manifest check succeeds, it uploads a uniquely
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
  Library/Audio/Plug-Ins/VST3/Neuramar.vst3 \
  Library/Audio/Plug-Ins/Components/Neuramar.component \
  Applications/Neuramar.app
xattr -dr com.apple.quarantine \
  Library/Audio/Plug-Ins/VST3/Electry.vst3 \
  Library/Audio/Plug-Ins/Components/Electry.component \
  Applications/Electry.app
xattr -dr com.apple.quarantine \
  Library/Audio/Plug-Ins/VST3/Taikor.vst3 \
  Library/Audio/Plug-Ins/Components/Taikor.component \
  Applications/Taikor.app
xattr -dr com.apple.quarantine \
  Library/Audio/Plug-Ins/VST3/YouKnow106.vst3 \
  Library/Audio/Plug-Ins/Components/YouKnow106.component \
  Applications/YouKnow106.app
```

For public distribution, build from source with your own Developer ID signing
and notarization. Each instrument README contains its own distribution guide:
[Vocalor](vocalor/README.md#sign-package-and-notarize),
[Drumalor](drumalor/README.md#sign-package-and-notarize),
[Neuramar](neuramar/README.md#sign-package-and-notarize),
[Electry](electry/README.md#sign-package-and-notarize),
[Taikor](taikor/README.md#sign-package-and-notarize), and
[YouKnow106](youknow106/README.md#sign-package-and-notarize).

## Building

There is no top-level CMake target: build each self-contained instrument from its
own directory. All six helpers use the same Xcode/CMake/JUCE toolchain,
compile universal `arm64`/`x86_64` binaries, run the instrument's CTest suite,
and write VST3, Audio Unit, and Standalone bundles below that instrument's
`build-macos/` directory. Drumalor, Neuramar, Electry, Taikor, and
YouKnow106
additionally include JUCE processor contract tests; Vocalor currently exercises
its JUCE-free DSP suite.

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

**Taikor** ([full instructions](taikor/README.md#build-on-macos)):

```bash
cd taikor
./scripts/build-macos.sh
```

**YouKnow106** ([full instructions](youknow106/README.md#build-on-macos)):

```bash
cd youknow106
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
  until all twelve uniquely named replacement assets have uploaded. It also
  keeps the committed documentation media honest: a Linux job re-renders
  every instrument's demonstration audio through its JUCE-free renderer, the
  macOS build renders every instrument's editor screenshot through its test
  suite, and either is committed back to `main` only when the bytes actually
  changed.

The JUCE source dependency is pinned to an immutable 8.0.14 archive and SHA-256
checksum in every project. Runner images and Xcode are supplied by GitHub Actions,
so the workflow badges remain the source of truth for the current hosted build.

## Repository layout

```text
LICENSE       Repository license (Apache-2.0)
vocalor/      Vocalor vocal and choir synthesizer (self-contained JUCE project)
drumalor/     Drumalor thirteen-voice drum synthesizer (self-contained JUCE project)
neuramar/     Neuramar sample-learned neural synthesizer (self-contained JUCE project)
electry/      Electry physically modeled dry electric guitar (self-contained JUCE project)
taikor/       Taikor physically modeled taiko drum (self-contained JUCE project)
youknow106/   YouKnow106 circuit-modelled six-voice DCO polysynth (self-contained JUCE project)
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
- **Neuramar** — original source under the [MIT License](neuramar/LICENSE); see
  its [third-party notices](neuramar/THIRD_PARTY_NOTICES.md).
- **Electry** — original source under the [MIT License](electry/LICENSE); see
  its [third-party notices](electry/THIRD_PARTY_NOTICES.md).
- **Taikor** — original source under the [MIT License](taikor/LICENSE); see
  its [third-party notices](taikor/THIRD_PARTY_NOTICES.md).
- **YouKnow106** — original source under the [MIT License](youknow106/LICENSE);
  see its [third-party notices](youknow106/THIRD_PARTY_NOTICES.md). YouKnow106
  models the voice architecture of a documented 1984 polysynth and is not
  affiliated with, endorsed by, or licensed by that instrument's manufacturer;
  it contains no firmware, ROM data, samples, or captured audio.

All instruments build against JUCE, which is not covered by those MIT
licences. JUCE 8 is dual-licensed under AGPLv3 or a commercial JUCE licence, so
confirm the applicable terms before distributing a binary.

No pretrained neural-network weights, voice datasets, factory samples, impulse
responses, third-party preset libraries, or vendor firmware or ROM images are
included in this repository.
Neuramar accepts user audio at runtime and stores a compact derived synthesis
model in host state; it does not embed or replay the imported recording.
