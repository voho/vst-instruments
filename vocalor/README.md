# Vocalor

Vocalor is a real-time vocal instrument for macOS. It turns MIDI notes into
original sung vowels, from a single singer to an ensemble or major/minor chord.
Three preset vowels anchor a **continuous vowel space** that can be morphed,
automated, and played from a draggable pad, and the whole vocal tract can be
lengthened or shortened independently of pitch. The engine is procedural and
runs locally: it does not clone a named singer, load recordings, or contact a
service while rendering audio.

![Vocalor Standalone instrument interface](Docs/screenshots/vocalor-standalone.png)

The screenshot above was captured from the 1.0 Standalone application and shows
the previous layout. The 1.1 editor keeps the same visual language and adds the
vowel-space pad, the live vocal-tract analyser with output meters, and the
phrasing and room-geometry controls described below; it has not been
re-captured because the screenshots can only be produced from a macOS build.
The VST3 and Audio Unit use the same resizable JUCE editor.

The interface remains fully resolution-independent: the layered hardware knobs,
choice states, panel depth, vowel pad, response curve, and complete vocal-range
keyboard are drawn as native JUCE graphics. This keeps labels and interaction
crisp while resizing and avoids bitmap controls that would obscure automation or
focus state.

The project builds three products from one JUCE codebase:

- VST3 instrument for hosts such as Ableton Live, REAPER, Cubase, and Bitwig
- Audio Unit v2 music device for Logic Pro and GarageBand
- Standalone application for direct MIDI-keyboard testing

> **Just want to try it?** The scheduled Nightly workflow publishes the latest
> successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> The bundles are ad-hoc signed and not notarized; check the repository's Nightly
> badge for the latest workflow result.

## Interface and controls

Vocalor exposes 20 automatable host parameters. The 13 parameters that shipped
in version 1 keep their identifiers, ranges, defaults, and host ordering, so
existing sessions recall exactly as before. The seven parameters added in 1.1
are appended and all default to a neutral setting.

| Parameter ID | Control | Range | Default | Added |
| --- | --- | --- | --- | --- |
| `profile` | Voice profile | Female / Male | Female | 1.0 |
| `mode` | Performance mode | Solo / Choir / Chord | Solo | 1.0 |
| `vowel` | Vowel anchor | AAH / OOH / UUH | AAH | 1.0 |
| `chordQuality` | Chord quality | Major / Minor | Major | 1.0 |
| `choirSize` | Ensemble size | 2 – 16 | 8 | 1.0 |
| `breath` | Breath | 0 – 100 % | 30 % | 1.0 |
| `resonance` | Resonance | 0 – 100 % | 64 % | 1.0 |
| `vibrato` | Vibrato | 0 – 100 % | 38 % | 1.0 |
| `humanize` | Humanize | 0 – 100 % | 52 % | 1.0 |
| `spread` | Stereo spread | 0 – 100 % | 62 % | 1.0 |
| `tension` | Vocal tension | 0 – 100 % | 36 % | 1.0 |
| `room` | Room | 0 – 100 % | 24 % | 1.0 |
| `output` | Output | −24 – +6 dB | −6 dB | 1.0 |
| `legato` | Legato | Off / On | Off | 1.1 |
| `vowelX` | Vowel front-back | 0 – 100 % | 50 % | 1.1 |
| `vowelY` | Vowel open-close | 0 – 100 % | 50 % | 1.1 |
| `vowelMorph` | Vowel morph | 0 – 100 % | 0 % | 1.1 |
| `formantShift` | Formant shift | −12 – +12 st | 0 st | 1.1 |
| `glide` | Glide | 0 – 100 % | 0 % | 1.1 |
| `roomSize` | Room size | 0 – 100 % | 50 % | 1.1 |

The top row selects the voice profile, the performance mode, the chord quality,
and the vowel anchor. **Ensemble size** is available from 2 to 16; the engine
renders Choir in 4-, 8-, or 12-singer tiers and Chord as six singers distributed
across the triad. **Legato** switches between retriggering every note and
bending the sounding voices into the new pitch, with a held-note stack so
releasing the top of a phrase falls back to the note underneath.

The **vowel-space pad** places a morph target anywhere between five cardinal
vowels (front to back on the horizontal axis, close to open on the vertical
axis). **Morph** crossfades from the selected preset vowel to that target, so at
0 % the pad has no effect at all and the instrument sounds exactly as it did in
version 1. **Shift** moves every formant by up to an octave in either direction
without touching the pitch, which is an effective vocal-tract-length or body-size
control. Next to the pad, the **vocal-tract response** display draws the live
magnitude response of the five formant resonators of a sounding voice on a
logarithmic axis, marks F1 to F5, and carries the stereo output meters.

Ten continuous controls shape **Breath**, **Resonance**, **Tension**,
**Vibrato**, **Humanize**, **Glide**, **Spread**, **Room**, room **Size**, and
the **Output** level. The status display reports active voices and sample rate,
the Panic button mutes immediately, and the on-screen keyboard is also mapped to
the computer keys shown above it.

## Sound engine

The JUCE-free C++20 DSP core uses a low-latency, source-filter vocal model.

**Glottal source.** The excitation is a band-limited Liljencrants-Fant style
glottal flow derivative rather than an arbitrary harmonic roll-off. Two
prototypes — a lax pulse (open quotient 0.78, speed quotient 2.6, long return
phase) and a firmly adducted one (0.46, 3.4, short return phase) — are analysed
once at `prepare()` time and rendered into nine band-limited mip levels. Vocal
tension crossfades between them, so the control changes the shape of the pulse
and the resulting spectral tilt for a physical reason instead of interpolating
two hand-tuned spectra. A one-pole tilt driven by velocity and tension is
applied on top, which is why a soft note is now dull as well as quiet.

**Vocal tract.** Five two-pole resonators model the formants. The three preset
vowels anchor a continuous inverse-distance-weighted vowel space, and every
formant target is resolved once per 64-sample chunk and then shaped per voice by
that singer's tract-length and per-formant dispersion, by note-dependent formant
tuning, and by the shared ensemble drift.

**Aspiration.** Breath noise is injected at the glottis and passes through the
same tract as the voiced excitation, which is both more faithful than a separate
noise filter and two resonators per voice cheaper. A small unfiltered component
keeps the consonantal air audible.

**Humanisation.** Each singer has slowly moving pitch, vibrato rate and depth,
spectral balance, breath, and timing rather than sharing a perfectly periodic
LFO. Pitch jitter runs through two nested smoothers for a 1/f-like spectrum, and
ensemble and chord modes instantiate independent singers, preventing the
phase-locked oscillator sound common to simple choir patches.

**Room.** The four-tap cross-coupled network reads through interpolated,
slowly modulated taps that break up the metallic ringing a static comb produces
on sustained vowels, and a gentle low cut keeps the tail out of the low mids.
Room size scales the tap geometry from a tight booth to a large space and
reaches exactly the historical geometry at its 50 % default.

This is a synthesizer, not a speech model or voice-cloning system. It is suited
to sustained vowels and expressive musical parts; it does not generate words.

## Performance

The 1.1 engine renders roughly a third faster than 1.0 in the audio path.
Measured with an offline benchmark on one core of a 2.1 GHz Xeon, `-O3`,
48 kHz, 128-sample blocks, best of five two-second runs:

| Case | 1.0 | 1.1 | Change |
| --- | --- | --- | --- |
| Solo, one note | 67.2 ns/sample | 56.0 ns/sample | −16.6 % |
| Solo, six notes | 291.7 ns/sample | 196.6 ns/sample | −32.6 % |
| Choir, 12 singers | 558.2 ns/sample | 353.9 ns/sample | −36.6 % |
| Choir, 12 singers × 4 notes | 2134.6 ns/sample | 1368.6 ns/sample | −35.9 % |
| Chord, 6 singers × 3 notes | 833.2 ns/sample | 544.1 ns/sample | −34.7 % |

The savings come from rendering voices over aligned 64-sample chunks instead of
sample-major across all voices, from hoisting the resonator bandwidth, radius,
and gain out of the per-voice control update (only the pole angle still varies
per voice, and it comes from the shared sine table rather than `exp` and `cos`),
from interleaving the two glottal wavetables so the oscillator touches half as
many cache lines, and from folding the aspiration path into the tract.

Building the wavetables now uses the sine table for both the analysis and the
additive synthesis instead of calling `sin()` two million times, which cuts
`prepare()` from about 8 ms to about 3.5 ms on the same machine. The one place
that is slightly slower is the fully idle path, which rose from 2.1 to about
3.4 ns/sample because the tract targets and drift are still refreshed while the
instrument is silent so the editor keeps following the controls. That is about
0.03 % of one core at 48 kHz.

Chunk boundaries are aligned to the absolute sample position, so the rendered
output is bit-identical no matter how a host splits its buffers; the test suite
asserts this.

The recursive filter states are kept out of denormal range by a vanishingly
small DC bias on the tract excitation rather than a per-tick compare, which is
worth about a third of the per-voice budget on its own, and the room network
clears itself once its tail is provably inaudible.

## Requirements

- macOS 11 or newer for running the built products
- A current full Xcode installation selected for command-line use
- CMake 3.22 or newer
- Internet access for the default first configure, or a local JUCE 8.0.14
  checkout supplied through `JUCE_PATH` to the helper
  (`VOCALOR_JUCE_PATH` when configuring CMake directly)

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
  -DCMAKE_BUILD_TYPE=Release \
  -DVOCALOR_BUILD_PLUGIN=OFF \
  -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The test executable renders solo, choir, major-chord, and minor-chord notes at
44.1, 48, and 96 kHz. It checks that held and released notes produce finite,
non-silent audio, that the release decays, and that a short offline render does
not exceed a generous performance guardrail. It also covers the vowel-space
model and the display maths, proves that the vowel pad is inaudible while morph
is at zero, checks the formant-shift scaling, the glide and legato note stack,
the room tap geometry, the absence of denormal state during a long release,
the behaviour under non-finite parameters, and that a full-range parameter jump
glides instead of stepping. This catches regressions; it is not a substitute for
listening tests, host automation tests, or profiling on the oldest supported
Mac.

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
creates a ZIP and installer package under `build-macos/dist/`. The current
helper's filenames use the `macOS-universal` suffix, so run it only after the
default universal build and confirm all three executables report both `arm64`
and `x86_64` with `lipo -archs` before publishing.

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

With `NOTARY_PROFILE` set, the helper submits and staples the installer package.
The ZIP still contains signed bundles but is not itself the notarized
distribution artifact.

Before publishing, verify the package from a clean user account and inspect it:

```bash
pkgutil --check-signature build-macos/dist/Vocalor-1.1.0-macOS-universal.pkg
spctl --assess --type install --verbose=4 \
  build-macos/dist/Vocalor-1.1.0-macOS-universal.pkg
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
Docs/                    Real interface screenshots and supporting documentation
Tests/                   JUCE-free DSP regression tests
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
