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
| [![Vocalor standalone interface](vocalor/Docs/screenshots/vocalor-standalone.png)](vocalor/README.md)<br>[9 rendered demos](vocalor/Docs/audio/README.md) | [![Drumalor standalone interface](drumalor/Docs/screenshots/drumalor-standalone.png)](drumalor/README.md)<br>[7 rendered demos](drumalor/Docs/audio/README.md) | [![Neuramar standalone interface](neuramar/Docs/screenshots/neuramar-standalone.png)](neuramar/README.md)<br>[8 rendered demos](neuramar/Docs/audio/README.md) | [![Electry standalone interface](electry/Docs/screenshots/electry-standalone.png)](electry/README.md)<br>[14 rendered demos](electry/Docs/audio/README.md) | [![Taikor standalone interface](taikor/Docs/screenshots/taikor-standalone.png)](taikor/README.md)<br>[25 rendered demos](taikor/Docs/audio/README.md) | [![YouKnow106 standalone interface](youknow106/Docs/screenshots/youknow106-standalone.png)](youknow106/README.md)<br>[10 rendered demos](youknow106/Docs/audio/README.md) |

Each screenshot above is rendered by its instrument's own regression suite
during the Nightly workflow's macOS build and committed automatically when the
editor has changed, so the images track the real editors. A build that does not
produce its screenshot fails rather than republishing the committed one, so a
suite that quietly stopped rendering cannot go unnoticed. The Nightly also
re-renders and commits every instrument's demonstration audio the same way,
from the same JUCE-free engines the plug-ins run:
[Vocalor](vocalor/Docs/audio/README.md),
[Drumalor](drumalor/Docs/audio/README.md),
[Neuramar](neuramar/Docs/audio/README.md),
[Electry](electry/Docs/audio/README.md),
[Taikor](taikor/Docs/audio/README.md), and
[YouKnow106](youknow106/Docs/audio/README.md).

## Instruments

| Instrument | Description | Formats | Platform | Docs |
| --- | --- | --- | --- | --- |
| [Vocalor](vocalor/) | Source-filter vocal and choir synthesizer with an LF-style glottal source, a continuous morphable vowel space, formant shifting independent of pitch, and legato phrasing across solo, ensemble, and chord modes. Vowel transitions run on articulator timescales rather than as a de-zipper, F1 tracks the fundamental once the pitch climbs past it, the singer's formant is a resonance cluster rather than an amplitude trim, a nasal branch lets the engine hum, and chords blend onto just intervals above the bass. A second pass rebuilt the parts of a note that are not its steady state: all five formants and the nasal branch sound from the first sample, so the onset is a source gesture instead of a filter one and velocity sets the attack between 9.3 ms accented and 86.4 ms soft across a 33 dB dynamic; the vibrato sits at 5.7 to 6.8 Hz and carries 3 dB of amplitude modulation at its own rate; the breath outlives the voice at a lax release and stops with it at a pressed one; the aspiration is modulated by the glottal cycle rather than sitting behind the voice as a separate source; the ensemble draws its entries and releases at every note instead of from twelve constants, so no two takes of a chord line up; and the singers stand at 1.5 to 6 m with their own propagation delays and early reflections rather than being panned, which takes the L/R correlation from 0.88 to 0.23. [Rendered demos.](vocalor/Docs/audio/README.md) [Plan.](vocalor/Docs/best-in-class-plan.md) | VST3 · AU · Standalone | macOS 11+ | [README](vocalor/README.md) |
| [Drumalor](drumalor/) | Thirteen-voice procedural drum synthesizer with struck-membrane modal models, velocity-dependent timbre, per-voice level/pan/choke mixing, humanised organic variation, a shared drive and compression bus, and GM-oriented MIDI mapping. The membrane banks bend with an energy estimate of head tension, a second strike absorbs energy from a still-ringing head, the snare answers strike position with rimshot and cross-stick, the hi-hat follows a continuous CC 4 pedal rather than three fixed states, and Kit Bleed couples the kit through sympathetic snare and tom beds. A second pass put a stick on both cymbals — a Hertzian contact tilts each machine's own carrier and the digital leg opens over an attack constant rather than in three samples, so velocity moves 3.1 and 3.8 dB of spectral balance where it moved 1.3 and 1.8 — split every membrane's loudest circumferential modes into the degenerate pairs they physically are, so the tails warble 5.9 to 6.6 dB at 2.1 to 4.7 Hz and Humanise moves where the stick lands, took the drawn pitch glide's depth from the blow, so a ghost stroke glides 1 to 5 % as far as an accent rather than over 90 %, and answered the rest of what a kit sends: a foot chick on note 44, bell, china and splash on 53, 52 and 55, aftertouch as a progressive cymbal choke, and CC 88's fourteen-bit velocity. [Rendered demos.](drumalor/Docs/audio/README.md) [Plan.](drumalor/Docs/best-in-class-plan.md) | VST3 · AU · Standalone | macOS 11+ | [README](drumalor/README.md) |
| [Neuramar](neuramar/) | Drop in a mostly monophonic sound, infer its root, and fit a compact local DDSP-inspired neural synthesis model — with fitted stiff-string inharmonicity, formant shifting, and velocity-driven timbre — whose harmonic Core, noisy Air, and resonant Bone remain playable across pitches. The harmonic analysis is solved jointly over a first-40 ms aperture halved to catch the attack, the body carries 16 Air bands and 12 Bone modes, and sixteen voices sound at once. A second pass took the render side: the register normalisation reaches the Core alone, so the isolated Air and Bone layers hold 0.4 and 0.0 dB across MIDI 12 to 108 where they moved 23 dB; a voice is placed by its own pitch and fixed there at note-on, so striking a second key no longer drags the first note 5.7 dB across the image and the order a chord arrives in no longer decides it; Orbit reads its loop forward only on a golden-ratio advance with the region's own level trend divided back out, so a held note neither pumps nor repeats; Dissolve damps by frequency on an exponent fitted from the source's own partials, so a released tail darkens rather than fading whole; the model clock key-tracks by that same exponent, so a struck memory rings shorter at the top of the keyboard and longer at the bottom; and Mutation varies how hard a note is struck instead of skipping into its attack. [Rendered demos.](neuramar/Docs/audio/README.md) [Plan.](neuramar/Docs/best-in-class-plan.md) [Fidelity benchmark.](neuramar/Docs/resynthesis-quality-benchmark.md) | VST3 · AU · Standalone | macOS 11+ | [README](neuramar/README.md) |
| [Taikor](taikor/) | Physically modeled taiko: a struck circular membrane solved from Bessel-zero modes with air loading, two heads coupled through the enclosed body, a thin-cylinder wooden shell, and a Hertz stick contact whose duration follows impact speed. Sixteen pads: four drums by four strokes, laid out four to an octave from C3. The four drums are four instruments rather than four sizes — ō-daiko, chū-daiko, okedo and shime each carry their own diameter, head tension, body depth, hide and shell, and differ by more than a tenth in dimensionless ratios such as depth over diameter and the breathing mode, which transposing one model cannot do. Don, Ka, Tsu and Don Rim on each, and a stereo close pair whose image comes from evanescent near-field decay rather than a widener. The head is solved as a stiff membrane rather than an ideal one, the attack's pitch glide falls out of the head stretching itself instead of being drawn, a stroke damps the head it lands on, the byō tack line has a preload threshold and its own voice, and the bottom of the velocity range opens onto a real ghost stroke. [Rendered demos.](taikor/Docs/audio/README.md) [Plan.](taikor/Docs/best-in-class-plan.md) | VST3 · AU · Standalone | macOS 11+ | [README](taikor/README.md) |
| [YouKnow106](youknow106/) | Circuit-modelled six-voice DCO polysynth: integer-divided note timers with their real pitch quantisation, a scanned control converter with per-hold slew and 7-bit patch digitisation, firmware envelopes with exponential falling segments into a measured quasi-linear amplifier, a four-pole transconductor filter with its divider-limited resonance loop solved implicitly, input-side resonance compensation, a key assigner that drops notes rather than stealing them, and an uncompanded two-line bucket-brigade chorus with the line's own saturation, at roughly half its former real-time cost. A second pass took out six mechanisms that mis-shaped the output rather than adding any: the fitted resonance trim gives way to a harmonic balance of the model's own two nonlinearities, so RESONANCE stops lifting the corner by up to 116 cents and acting as a second CUTOFF; C59 sits between the filter output and the amplifier's input, so a deep-PWM patch no longer thumps as it opens; the chorus hiss is the MN3009's own datasheet noise row, 14.4 dB down; LFO DELAY holds the pulse width alongside the vibrato and the filter sweep; the modelled warm-up reaches 34.48 °C at every sample rate and quality setting instead of freezing part way; and the optional velocity extension reaches the filter envelope as well as the amplifier, opening the corner to 190 Hz at velocity 0.2 where all three velocities read 1985. [Claims boundary.](youknow106/Docs/circuit-modelling-research.md) [Rendered demos.](youknow106/Docs/audio/README.md) [Plan.](youknow106/Docs/best-in-class-plan.md) [Market comparison.](youknow106/Docs/comparative-assessment.md) | VST3 · AU · Standalone | macOS 11+ | [README](youknow106/README.md) |
| [Electry](electry/) | Oversampled physically modeled Drop-E eight-string guitar: eight dual-polarisation waveguides, fitted stiff-string dispersion, sympathetic strings, a continuous palm mute, induced-EMF pickups, modal body loss, Mono/divided-pickup Stereo, two independent keyswitch banks (three pick strokes against seven play styles, allocated by a fretting hand with a position and a reach, with the natural harmonic a real node touch rather than a transpose), a pitch wheel that bends every string like a vibrato bar, a resonance wheel that pushes a distorted tone into self-sustaining feedback, and a 4x-oversampled amplifier with supply sag, an output transformer and a modelled cabinet. A second pass put a player in front of it: velocity is the pick's deflection and spans 18.2 dB across the keyboard where it spanned 5.2, the picking hand draws a new contact position and force at every stroke, so two identical note-ons spread 2.5 dB of peak and 17 Hz of attack centroid where they spread 0.01 dB and 0.4 Hz, the humbucker is two coils 19 mm apart whose first notch falls from 5508 to 3046 Hz on string 2, the vibrato is a hand that redraws every cycle and puts a separate finger on each stopped string, a strummed chord travels from the stroke's own edge of the neck in gaps compressing from 14.7 to 10.1 ms whatever order the host sent it in, and the strings a chord is fingering exchange energy through the bridge they share rather than only the open ones. [Rendered demos.](electry/Docs/audio/README.md) [Plan.](electry/Docs/best-in-class-plan.md) | VST3 · AU · Standalone | macOS 11+ | [README](electry/README.md) |

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
- **Nightly** runs the same macOS helpers in universal mode, one job per
  instrument, packages ZIP and PKG artifacts containing ad-hoc-signed bundles,
  retains a combined workflow artifact for 14 days, and preserves the prior
  complete rolling-release set until all twelve uniquely named replacement
  assets have uploaded. A release is published only when all six instruments
  built, so it is always a complete set; one instrument failing costs only its
  own packages rather than everything queued behind it. The Nightly also keeps
  the committed documentation media honest: a Linux job re-renders every
  instrument's demonstration audio through its JUCE-free renderer, each macOS
  build renders that instrument's editor screenshot through its test suite, and
  either is committed back to `main` only when the bytes actually changed. Both
  media jobs check the full set — seventy-three demonstration WAVs and six editor
  screenshots — and fail when anything is missing, after committing whatever was
  regenerated successfully.

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
