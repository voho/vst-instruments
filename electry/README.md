# Electry

Electry is an original, physically modeled dry electric guitar instrument.
Six string voices run dual-polarisation waveguide loops with physically
derived stiffness dispersion, decay-targeted damping, tension-modulation
pitch glide, collision-informed slap behavior, and a published pickup signal
structure (position comb, magnetic aperture, flux nonlinearity, and loaded
coil resonance). Play styles are selected with latching keyswitches below
the playable range. The individual models have named research references
(see the [physical-modeling research
contract](Docs/physical-modeling-research.md)); Electry does not claim to be
a capture-accurate clone of any one instrument.

There are **no effects**: no amplifier, cabinet, reverb, chorus, or
compression. The output is the dry electric guitar signal a DI box would
carry, ready for the amp simulation of your choice. Every guitar-model axis
is parametrized to sit between a Gibson Les Paul-style anchor at 0% and a
Fender Telecaster-style anchor at 100%, and defaults to the midpoint.

> **Just want to try it?** The scheduled Nightly workflow publishes the
> latest successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> The bundles are ad-hoc signed and not notarized; check the repository's
> Nightly badge for the latest workflow result.

## Keyswitches and playable range

MIDI notes 24..32 (C1..G#1) are latching keyswitches; they never sound, and
the most recent one selects the play style for every following note until
changed. Keyswitch note-offs are ignored. The editor's PLAY STYLE strip
sends the same keyswitches and always shows the currently latched style.

| MIDI note | Key | Play style |
| --- | --- | --- |
| 24 | C1 | Downstroke (default) |
| 25 | C#1 | Upstroke — opposite displacement polarity, slightly closer to the bridge, thinner and brighter |
| 26 | D1 | Hammer-on / pull-off — continues a sounding string legato within a nine-fret reach, fingered attack, no plectrum noise |
| 27 | D#1 | Muted — palm mute with damping set by the Mute Damp control |
| 28 | E1 | Bend 1 up — picks the played note, bends +1 semitone over the Bend Time |
| 29 | F1 | Bend 2 up — picks the played note, bends +2 semitones |
| 30 | F#1 | Bend 1 down — picks 1 semitone above, releases onto the played note |
| 31 | G1 | Bend 2 down — picks 2 semitones above, releases onto the played note |
| 32 | G#1 | Slap — hard attack, fret-collision buzz window, deeper tension glide, thumb knock into the body |

Notes 40..86 (open E2 to fret 22 on the high E string) are playable in
standard tuning; notes outside both ranges are ignored. Each note is
allocated to one of the six physical strings: a repick of a sounding note
grabs the same string, hammer-on continues the nearest sounding string,
otherwise the free string with the lowest fret wins (which reproduces
open-position chord shapes), and when everything sounds, the oldest string
is stolen. The pitch wheel bends ±2 semitones and the sustain pedal (CC 64)
holds released strings; CC 120/123 behave as All Sound Off and All Notes
Off.

## Sound architecture

- **Strings:** one voice per physical string. Each voice runs two
  single-delay-loop waveguides (the two transverse polarisations) with
  third-order Lagrange fractional delays, a one-pole damping filter solved
  from per-string decay targets, two dispersion allpasses solved from the
  string's physical inharmonicity (diameter, wound core, scale length,
  tension), and a contractive bridge coupling. The horizontal polarisation
  decays 1.7x slower and is detuned by a fraction of a cent, giving the
  natural two-stage decay and slow beating. Loop-filter phase is
  compensated analytically, holding the fundamental within a few cents
  across the fretboard at 44.1-192 kHz.
- **Frets:** fretting position drives sounding length, inharmonicity,
  pickup comb geometry, and Fleischer-style dead-spot damping (deeper on
  the bolt-on end of the construction axis). Slap opens a decaying
  fret-collision window that soft-limits displacement and re-radiates
  deterministic rattle. Hammer-ons retarget a sounding loop without
  clearing its state.
- **Tension modulation:** a string-energy envelope shortens the loop delay,
  so hard attacks start audibly sharp and relax over hundreds of
  milliseconds; slaps deepen the effect. Bend styles move the same pitch
  program along a finger-shaped curve.
- **Pickups and coils:** per-string position combs at morphing
  bridge/neck distances, a wave-speed-scaled magnetic aperture lowpass
  (wide humbucker window to narrow single-coil window), a bounded
  second-order-dominant flux nonlinearity, and one loaded resonant coil
  filter per pickup (2.45 kHz / Q 1.35 humbucker anchor to 4.05 kHz /
  Q 1.95 single-coil anchor). The selector fades Neck, Both (with the
  paired-coil resonance shift), or Bridge; the passive tone control moves
  the loaded resonance down and damps it.
- **Body:** four modal resonators voiced along the wood, size, and shape
  axes colour the coil inputs and receive slap knocks and contact noise.
  Solid-body colour, not an acoustic radiator.
- **Play noise:** deterministic seeded plectrum scrape, finger contact,
  release damping noise, and slap knock, band-shaped per string (wound
  versus plain) with independent level controls. Identical MIDI always
  renders identical audio.

## Guitar-model axes

Every axis reads "Les Paul-style at 0, Telecaster-style at 1" and defaults
to 0.5:

| Control | 0 | 1 |
| --- | --- | --- |
| Body wood | Mahogany/maple blank | Swamp ash slab |
| Body size | Thick, heavy (lower modes) | Thin, light (higher modes) |
| Body shape | Carved single-cut pattern | Flat slab pattern |
| Construction | Set neck + stopbar | Bolt-on + through-body |
| Scale length | 24.75 in | 25.5 in |
| Pickup type | Humbucker | Narrow single coil |

## Exact 20-parameter contract

All parameters are version-1 host parameters in this order. Continuous
controls are smoothed inside the engine; the pickup selector fades over
4 ms.

| # | ID | Name | Range and default |
| --- | --- | --- | --- |
| 1 | `pickupSelector` | Pickup selector | Neck / Both / **Bridge** |
| 2 | `pickupType` | Pickup type | 0..100%, default 50% |
| 3 | `tone` | Tone | 0..100%, default 80% |
| 4 | `bodyWood` | Body wood | 0..100%, default 50% |
| 5 | `bodySize` | Body size | 0..100%, default 50% |
| 6 | `bodyShape` | Body shape | 0..100%, default 50% |
| 7 | `construction` | Construction | 0..100%, default 50% |
| 8 | `scaleLength` | Scale length | 24.75"..25.50", default 25.13" |
| 9 | `bodyResonance` | Body resonance | 0..100%, default 35% |
| 10 | `stringGauge` | String gauge | light 9s..medium 11s, default 50% |
| 11 | `stringAge` | String age | 0..100%, default 15% |
| 12 | `pickPosition` | Pick position | bridge..neck, default 35% |
| 13 | `pickHardness` | Pick hardness | 0..100%, default 60% |
| 14 | `pickNoise` | Pick noise | 0..100%, default 50% |
| 15 | `fingerNoise` | Finger noise | 0..100%, default 40% |
| 16 | `releaseNoise` | Release noise | 0..100%, default 40% |
| 17 | `muteDamping` | Mute damping | 0..100%, default 55% |
| 18 | `bendTime` | Bend time | 40 ms..2 s, default 280 ms |
| 19 | `velocity` | Velocity response | 0..100%, default 65% |
| 20 | `output` | Output level | -24..+6 dB, default -6 dB |

## Build products

`scripts/build-macos.sh` writes three bundles below `build-macos/`:

- `Electry_artefacts/Release/VST3/Electry.vst3`
- `Electry_artefacts/Release/AU/Electry.component`
- `Electry_artefacts/Release/Standalone/Electry.app`

## Requirements

- macOS 11 or newer (Apple silicon or Intel; universal by default)
- Full Xcode installation selected with `xcode-select`
- CMake 3.22 or newer
- Internet access for the first configure (JUCE 8.0.14 is fetched and
  pinned by checksum), or a local checkout supplied via `JUCE_PATH`

## Build on macOS

```bash
cd electry
./scripts/build-macos.sh
```

The script configures the Xcode generator, builds universal binaries, runs
the CTest suite (engine and plug-in contract tests), and ad-hoc signs the
three bundles. Environment overrides: `BUILD_UNIVERSAL=OFF` for a
native-arch build, `CONFIG=Debug`, `BUILD_DIR`, and `JUCE_PATH` for a local
JUCE 8.0.14 checkout.

## JUCE-free DSP build

The complete string, interaction, and pickup engine builds and tests
without JUCE on any platform with CMake and a C++20 toolchain (this is what
Linux CI runs):

```bash
cd electry
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DELECTRY_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

## Install and validate locally

```bash
ditto build-macos/Electry_artefacts/Release/VST3/Electry.vst3 \
  ~/Library/Audio/Plug-Ins/VST3/Electry.vst3
ditto build-macos/Electry_artefacts/Release/AU/Electry.component \
  ~/Library/Audio/Plug-Ins/Components/Electry.component
```

Run `auval -v aumu Elc1 Eltr` after installing the Audio Unit, and rescan
plug-ins in your host. The Standalone app runs directly from the build
tree.

## Sign, package and notarize

```bash
cd electry
./scripts/sign-and-package-macos.sh
```

Without arguments the script stages the three bundles with their licence
documentation, ad-hoc signs them, and writes
`build-macos/dist/Electry-1.0.0-macOS-universal.zip` and `.pkg`. For
distribution, provide `APP_SIGN_IDENTITY` (Developer ID Application),
`INSTALLER_SIGN_IDENTITY` (Developer ID Installer), and optionally
`NOTARY_PROFILE` for `notarytool` submission and stapling.

## Project layout

```text
CMakeLists.txt       DSP library, JUCE plug-in, and CTest targets
Source/DSP/          ElectryEngine (JUCE-free physical model)
Source/              PluginProcessor and PluginEditor (JUCE shell)
Tests/               Engine regression suite and plug-in contract tests
Docs/                Physical-modeling research and implementation contract
Presets/             Sound-design recipes for the 20-parameter set
scripts/             macOS build and packaging helpers
ThirdParty/          JUCE licence notice
```

## Licensing

Electry's original source is under the [MIT License](LICENSE); see the
[third-party notices](THIRD_PARTY_NOTICES.md). Electry builds against JUCE,
which is dual-licensed under AGPLv3 or a commercial JUCE licence
([notice](ThirdParty/JUCE-LICENSE.md)); confirm the applicable terms before
distributing binaries. No samples, impulse responses, or third-party preset
libraries are included; "Les Paul" and "Telecaster" name the reference
styles of the modeling axes and are trademarks of their respective owners,
with no affiliation or endorsement implied.
