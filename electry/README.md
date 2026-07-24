# Electry

Electry is an original, physically modeled dry electric guitar instrument.
Eight string voices run dual-polarisation waveguide loops with physically
derived stiffness dispersion, decay-targeted damping, tension-modulation
pitch glide, collision-informed slap behavior, one-way bridge coupling into
the strings you are *not* fingering, and a published pickup signal
structure (position comb, finite magnetic aperture, nonlinear flux, induced
EMF, and loaded coil resonance). Play styles are selected with latching keyswitches below
the playable range. The individual models have named research references
(see the [physical-modeling research
contract](Docs/physical-modeling-research.md)); Electry does not claim to be
a capture-accurate clone of any one instrument.

The compact FX panel provides parallel distortion, amp/cab simulation,
compression, lead delay, and a stereo room; every effect defaults to a true
0% dry setting. Mono is the authentic summed dry DI; Stereo is a phase-coherent
divided-pickup view of the eight physical strings, not an effect or delay.
Both are ready for the amp simulation of your choice. Material, body, pickup,
and construction controls span deliberately contrasting solid-body anchors;
scale length spans a conventional 25.5-inch electric to a modern 28-inch
baritone/8-string build.

![Electry electric guitar interface](Docs/screenshots/electry-standalone.png)

**Note:** this committed screenshot was rendered from the 1.0 editor. The 1.1
interface adds the live FRETBOARD panel and the PERFORMANCE controls described
below, and the image will be regenerated on the next macOS build. The
screenshot is produced by the plug-in regression suite itself
(`ELECTRY_EDITOR_SNAPSHOT`), so it is always a real editor render rather than a
mock-up. The Standalone, VST3, and Audio Unit use that same
JUCE component. Panels, knobs, the fretboard, the keyswitch strip, and the
on-screen keyboard are drawn as resolution-independent JUCE graphics;
interactive controls stay native for automation, keyboard operation, and
accessibility.

The editor uses audible impact as its visual hierarchy: pickup/tone,
excitation, age, body resonance, and velocity are oversized in the Core row;
guitar-build controls are medium; articulation-specific noise and artifact
details are compact.
The walnut-and-amber chassis nods to a workbench electric guitar without
reproducing a branded hardware panel.

### Live fretboard

Under the play-style strip, a 22-fret eight-string fingerboard shows the model
as it plays: which physical string each note was allocated to, exactly where it
is stopped (with its note name), how hard it is ringing, and which strings are
ringing only through the sympathetic bridge coupling. Sounding strings are
amber and vibrate as the fundamental standing wave of their sounding length, so
the part of the string behind the fretting finger correctly stays still;
bridge-coupled strings are cool blue. A per-string meter on the right shows the
level with proper attack/release ballistics, and the header readout reports
both counts, for example `3 STRINGS +4 RING`.

All of the fretboard's geometry, ballistics, colour mapping and the lock-free
audio-to-editor transfer live in the JUCE-free `Source/DSP/ElectryVisuals.*`
module, so they are unit-tested on every platform and the editor stays a thin
renderer. The PERFORMANCE panel beside the fretboard holds the four controls
that change what it shows.

> **Just want to try it?** The scheduled Nightly workflow publishes the
> latest successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> The bundles are ad-hoc signed and not notarized; check the repository's
> Nightly badge for the latest workflow result.

## Keyswitches and playable range

MIDI notes 12..27 (C0..D#1) are latching keyswitches; they never sound, and
the most recent one selects the play style for every following note until
changed. Keyswitch note-offs are ignored. The editor's PLAY STYLE strip
sends the same keyswitches and always shows the currently latched style. The
on-screen keyboard colours and labels the entire keyswitch bank separately
from the playable instrument.

| MIDI note | Key | Play style |
| --- | --- | --- |
| 12 | C0 | Downstroke (default) |
| 13 | C#0 | Upstroke — opposite displacement polarity, slightly closer to the bridge, thinner and brighter |
| 14 | D0 | Alternate stroke — starts down, then alternates down/up for each accepted note-on |
| 15 | D#0 | Hammer-on / pull-off — continues a sounding string legato within a nine-fret reach, fingered attack, no plectrum noise |
| 16 | E0 | Tap — a fresh, focused fingerboard attack without plectrum scrape |
| 17 | F0 | Palm mute — damping follows the Mute Damp control |
| 18 | F#0 | Chug — harder contact and a tighter, shorter metal rhythm mute |
| 19 | G0 | Dead note — short, percussive fretting-hand choke |
| 20 | G#0 | Natural harmonic — glassy octave harmonic with a soft node-focused touch |
| 21 | A0 | Pinch harmonic — bright octave-plus-fifth squeal with a hard pick edge |
| 22 | A#0 | Tremolo pick — repeated alternating strokes while the note remains held |
| 23 | B0 | Bend 1 up — picks the played note, bends +1 semitone over the Bend Time |
| 24 | C1 | Bend 2 up — picks the played note, bends +2 semitones |
| 25 | C#1 | Bend 1 down — picks 1 semitone above, releases onto the played note |
| 26 | D1 | Bend 2 down — picks 2 semitones above, releases onto the played note |
| 27 | D#1 | Slap — hard attack, fret-collision buzz window, deeper tension glide, thumb knock into the body |

Notes 28..86 are playable on a 22-fret, eight-string Drop-E instrument tuned
E1-B1-E2-A2-D3-G3-B3-E4; notes outside both ranges are ignored. Each note is
allocated to one of the eight physical strings: a repick of a sounding note
grabs the same string, hammer-on continues the nearest sounding string,
otherwise the free string with the lowest fret wins (which reproduces
open-position chord shapes), and when everything sounds, the oldest string
is stolen. The pitch wheel bends ±2 semitones and the sustain pedal (CC 64)
holds released strings; the modulation wheel (CC 1) adds a delayed, natural
5.4 Hz vibrato whose full-scale depth is set by the Vibrato Depth parameter
(0..100 cents, 35 by default); breath/CC 2 adds continuous bridge-hand damping
on top of the Palm Mute parameter; CC 120/123 behave as All Sound Off and All
Notes Off.

## Sound architecture

- **Strings:** one voice per physical string. Each voice runs two
  single-delay-loop waveguides (the two transverse polarisations) with
  third-order Lagrange fractional delays, a one-pole damping filter solved
  from per-string decay targets, an eight-stage factored allpass dispersion
  cascade fitted at low and high partials from the string's physical
  inharmonicity (diameter, effective wound core, scale length,
  tension), and a contractive bridge coupling. The horizontal polarisation
  decays 1.7x slower and is detuned by a fraction of a cent, giving the
  natural two-stage decay and slow beating. Loop-filter phase is
  compensated analytically, holding the fundamental within a few cents
  across the fretboard at 44.1-384 kHz. At host rates through 96 kHz the
  complete physical and nonlinear signal path runs internally at 2x and is
  returned through a 63-tap halfband FIR; higher-rate hosts run natively.
- **Pick position:** the picking hand stays at a fixed distance from the
  bridge; it does not follow the fretting hand up the neck. Pick Position is
  therefore a fraction of the *open* string, and the pluck comb as a fraction of
  the sounding length grows by `2^(fret/12)`, so a note fretted high up moves
  toward the hollow, mid-string comb of a real guitar. At the nut this is
  identical to the previous behaviour.
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
- **Velocity:** one coherent response profile drives attack level, pulse
  width and brightness, the string-scaled modal release, contact noise,
  tension glide, and collision likelihood. The principal release passes two
  low-pass stages whose time scale follows the string period, approximating
  the `1/n^2` modal falloff of a triangular pluck displacement; for ordinary
  sustained pick styles, a much smaller broadband component preserves the
  pick edge. Their delay-line projection is normalised against open E4, so
  equal player effort does not lose tens of decibels on the much longer E1
  loop. At 0% response those
  dimensions are velocity-invariant; at 100% they span soft finger-light
  notes through aggressive metal attacks.
- **Pickups and coils:** per-string position combs at morphing
  bridge/neck distances, a wave-speed-scaled finite rectangular magnetic
  aperture with its exact sinc response (wide humbucker window to narrow
  single-coil window), a bounded second-order-dominant flux nonlinearity,
  induced-EMF differentiation with an oversampled ultrasonic guard, and one
  loaded resonant coil filter per pickup (2.0 kHz / Q 1.0 humbucker anchor to 6.0 kHz /
  Q 2.4 single-coil anchor). A bounded string-mass/pole-balance calibration
  keeps the thick low strings at practical guitar-pickup levels. The selector
  fades Neck, Both (with the
  paired-coil resonance shift), or Bridge; the passive tone control moves
  the loaded resonance down and damps it.
- **Body:** four modal resonators voiced along strongly separated wood, size,
  shape, and construction endpoints receive bridge motion, slap knocks, and
  contact noise. Each resonator is normalised at its own modal peak so low
  modes do not acquire an unintended frequency-dependent boost. The resulting
  structural drive is converted once to induced voltage before the modal
  bank, guarded, then joined with string pickup voltage before the same coil;
  displacement is never mixed directly into an electrical signal. Their
  positive, bounded modal bridge conductance also drains string energy when a
  fundamental or strong partial meets a body mode, so the build changes
  sustain as well as timbre. Solid-body coupling, not an acoustic radiator.
- **Sympathetic bridge coupling:** every string you are not fingering is a
  real waveguide, not a resonator bank. The plucked strings' bridge force is
  summed into a one-sample-delayed bus that drives the idle strings' own loops
  at their open pitch, with their own damping and their own bridge pickup tap.
  Only played voices write to the bus and only idle voices read it, so the
  coupling graph is acyclic and unconditionally stable at any coupling gain;
  each coupled loop additionally carries a bounded soft limit. The bridge hand
  that mutes a palm-muted, chugged or dead-note passage covers every string, so
  those styles damp and starve the coupled strings automatically and a Drop-E
  chug stays tight. At 0% the coupled loops are never configured, rendered or
  injected into, so the control is an exact bypass and not a small residue.
  Coupled strings reuse the idle voice's own delay line, so the feature costs
  no extra memory, and they retire as soon as they fall below audibility.
- **Bridge-hand damping:** Palm Mute is a continuous pressure, independent of
  the Muted and Chug keyswitches, that applies to every play style. It
  interpolates the string's decay geometrically from its own T60 toward a hard
  40 ms stop and darkens it as the heel of the hand covers more of the string.
  Because it re-solves the same loop filters and re-applies the analytic phase
  compensation, a damped string stays exactly in tune. Zero pressure is a
  mathematical no-op. MIDI CC 2 adds to it live.
- **Strum travel:** simultaneous note-ons inside a 35 ms window are one pick
  stroke. The first string the pick meets fires immediately and every further
  string is offset by the Strum Spread travel time per string crossed, so a
  chord sweeps instead of landing as a block and its stacked initial peak drops.
  At 0 ms chords are exactly simultaneous, as before.
- **Play noise:** deterministic seeded plectrum scrape, finger contact,
  release damping noise, and slap knock, band-shaped per string (wound
  versus plain) with independent level controls. Identical MIDI always
  renders identical audio.
- **Artifacts:** a separate deterministic imperfection path adds controllable
  bridge-hardware ring, velocity-dependent incidental fret contact, and
  string-specific saddle buzz. This is the mechanical noise of the instrument's
  hardware, distinct from the Sympathetic Ring control above, which vibrates
  the actual strings. At 0% it is exactly bypassed and silent; the 18% default
  is intentionally subtle.
- **Cost:** the model only pays for what is audible. A pickup the selector has
  faded out is skipped outright, including its two fractional reads, aperture
  window, flux polynomial and induced-EMF guard. Mono runs one shared coil, DC
  and decimation chain instead of two and mirrors it, which is exact because
  the channels are identical; opening the stereo field copies the state across
  at that sample. Damping-only control moves reuse the existing dispersion fit
  instead of re-running its grid search. Once nothing is vibrating and the
  shared path has fallen below -120 dBFS the engine freezes: state is cleared,
  the output is exactly zero, and no arithmetic runs until the next note. All
  rate-derived smoothing constants are solved in `prepare()` rather than with
  `std::pow` inside the sample loop.
- **Output field:** Mono is exact dual mono and preserves the conventional
  electric-guitar DI. Stereo spreads per-string pickup signals left-to-right
  in physical low-to-high string order, keeps the body centred, folds down
  coherently, and adds no chorus, modulation, random phase, or Haas delay.

## Guitar construction axes

The material controls use contrasting classic solid-body anchors and default
to 0.5. Scale length is widened for the Drop-E instrument:

| Control | 0 | 1 |
| --- | --- | --- |
| Body wood | Mahogany/maple blank | Swamp ash slab |
| Body size | Thick, heavy (lower modes) | Thin, light (higher modes) |
| Body shape | Carved single-cut pattern | Flat slab pattern |
| Construction | Set neck + stopbar | Bolt-on + through-body |
| Scale length | 25.5 in conventional electric | 28 in baritone / 8-string |
| Pickup type | Humbucker | Narrow single coil |

## Exact 31-parameter contract

The original 20 version-1 host parameters remain in their exact order; the
Artifacts control is parameter 21 and Output field is appended as parameter
22. The five FX controls are appended as parameters 23..27, and the four
version-1.1 performance controls as parameters 28..31, so every existing host
automation index keeps pointing at the same control. A session saved before
1.1 loads unchanged and picks up the new defaults. Continuous controls are
smoothed inside the engine; pickup and output mode changes crossfade over
roughly 4 ms.

| # | ID | Name | Range and default |
| --- | --- | --- | --- |
| 1 | `pickupSelector` | Pickup selector | Neck / Both / **Bridge** |
| 2 | `pickupType` | Pickup type | 0..100%, default 50% |
| 3 | `tone` | Tone | 0..100%, default 80% |
| 4 | `bodyWood` | Body wood | 0..100%, default 50% |
| 5 | `bodySize` | Body size | 0..100%, default 50% |
| 6 | `bodyShape` | Body shape | 0..100%, default 50% |
| 7 | `construction` | Construction | 0..100%, default 50% |
| 8 | `scaleLength` | Scale length | 25.50"..28.00", default 26.75" |
| 9 | `bodyResonance` | Body resonance | 0..100%, default 35% |
| 10 | `stringGauge` | String gauge | Drop-E .009-.080 set to .011-.098 set, default 50% |
| 11 | `stringAge` | String age | 0..100%, default 15% |
| 12 | `pickPosition` | Pick position | bridge..neck, default 35% |
| 13 | `pickHardness` | Pick hardness | 0..100%, default 60% |
| 14 | `pickNoise` | Pick noise | 0..100%, default 50% |
| 15 | `fingerNoise` | Finger noise | 0..100%, default 40% |
| 16 | `releaseNoise` | Release noise | 0..100%, default 40% |
| 17 | `muteDamping` | Mute damping | 0..100%, default 55% |
| 18 | `bendTime` | Bend time | 40 ms..2 s, default 280 ms |
| 19 | `velocity` | Velocity response | 0..100% multi-dimensional response, default 65% |
| 20 | `output` | Output level | -24..+6 dB, default -6 dB |
| 21 | `artifacts` | Artifacts | clean bypass..ring/contact/saddle detail, default 18% |
| 22 | `outputMode` | Output field | **Mono** / Stereo divided-pickup field |
| 23 | `distortion` | Distortion | dry..high-gain saturation, default 0% |
| 24 | `amp` | Amp simulation | dry..amp/cab saturation, default 0% |
| 25 | `compressor` | Compressor | dry..fast rhythm levelling, default 0% |
| 26 | `delay` | Delay | dry..360 ms lead delay, default 0% |
| 27 | `room` | Room | dry..compact stereo ambience, default 0% |
| 28 | `sympathetic` | Sympathetic ring | exact bypass..full bridge coupling into the unfingered strings, default 20% |
| 29 | `palmMute` | Palm mute | 0..100% continuous bridge-hand damping for every play style (adds to MIDI CC 2), default 0% |
| 30 | `strumSpread` | Strum spread | 0..40 ms of pick travel per string crossed, default 0 ms (block chord) |
| 31 | `vibratoDepth` | Vibrato depth | 0..100 cents of full-scale CC 1 vibrato, default 35 cents |

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
`build-macos/dist/Electry-1.1.0-macOS-universal.zip` and `.pkg`. For
distribution, provide `APP_SIGN_IDENTITY` (Developer ID Application),
`INSTALLER_SIGN_IDENTITY` (Developer ID Installer), and optionally
`NOTARY_PROFILE` for `notarytool` submission and stapling.

## Project layout

```text
CMakeLists.txt       DSP library, JUCE plug-in, and CTest targets
Source/DSP/          ElectryEngine (JUCE-free physical model) and
                     ElectryVisuals (JUCE-free fretboard geometry/ballistics)
Source/              PluginProcessor and PluginEditor (JUCE shell)
Tests/               Engine regression suite and plug-in contract tests
Docs/                Physical-modeling research and implementation contract
Presets/             Sound-design recipes for the 31-parameter set
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
