# Taikor

Taikor is a real-time **physically modeled taiko** for macOS. It does not load
samples, replay a recording, or emulate a particular branded instrument. Every
stroke is solved from a struck circular membrane: the head's modes come from the
zeros of a Bessel function, the air hanging off it lowers them, the enclosed body
couples the two heads together, the wooden shell rings underneath, and a bachi
meets the hide through a Hertz contact whose duration follows the impact speed.

Change the diameter and the pitch moves as one over the radius. Change the head
material and the drum gets heavier, darker, and more strongly loaded by the air.
Seal the body and the fundamental splits in two. None of that is scripted — it
falls out of the same solve.

> **Listen first.** Twenty-three [rendered demonstrations](Docs/audio/README.md)
> cover the stroke vocabulary, all six octaves, three drums of the taiko family
> and every physical control swept across its range. They are rendered by the
> shipping engine, so they cannot drift from what the plug-in does.

The project builds three products from one JUCE codebase:

- VST3 instrument for hosts such as Ableton Live, REAPER, Cubase, and Bitwig
- Audio Unit v2 music device for Logic Pro and GarageBand
- Standalone application for direct MIDI-pad and on-screen testing

> **Just want to try it?** The scheduled Nightly workflow publishes the latest
> successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> The bundles are ad-hoc signed and not notarized; check the repository's Nightly
> badge for the latest workflow result.

## How it is played

**Within an octave, the twelve notes are twelve different strokes. Between
octaves, the drum itself changes: higher octave, higher drum.**

That is the whole mapping. There are no keyswitches and no articulation menu —
the stroke is the pitch class and the drum is the octave.

| Note | Stroke | Spoken as | What it is |
| --- | --- | --- | --- |
| C | Don | *don* | Full centre strike: the open voice of the drum |
| C♯ | Do | *do* | Open stroke a little off centre, quicker than a Don |
| D | Tsu | *tsu* | Damped centre, the free hand resting on the head |
| D♯ | Su | *su* | Ghost stroke, barely sounded |
| E | Don Rim | *don* | Head and rim struck together for a rim shot |
| F | Ka | *ka* | On the edge of the head, near the tacks |
| F♯ | Kara | *kara* | Extreme edge, thin and cutting |
| G | Ko | *ko* | Light tap at mid radius |
| G♯ | Katsu | *katsu* | Bachi on the wooden shell |
| A | Buzz | *zu* | Press roll: the stick stays on the head |
| A♯ | Flam | *doko* | Grace note into a full stroke |
| B | Bachi | *kata* | Stick against stick, with no drum at all |

These are not twelve presets. Each one is a strike position, a contact stiffness
and a mute state fed into the same model. A Ka is bright because striking the
head at 0.78 of its radius drives the modes that have a circumferential order and
barely moves the axisymmetric ones — which is exactly why it is bright on a real
taiko.

| Octave | Notes | Drum |
| --- | --- | --- |
| C1 | 24–35 | Odaiko: the largest drum, a low boom with a long tail |
| C2 | 36–47 | A large drum |
| **C3** | **48–59** | **The drum the controls describe, unscaled** |
| C4 | 60–71 | A smaller drum |
| C5 | 72–83 | Shime-daiko territory: tight, high, and short |
| C6 | 84–95 | Smaller still |

Notes outside 24–95 are silent.

**Velocity** sets the impact speed of the stick. The timbre change that comes
with it is not a separate control, because it is not a separate effect: Hertz
contact time falls as the fifth root of impact speed, so a harder stroke is
shorter, brighter and louder at once.

**MIDI CC1** lays a hand on the head. It damps whatever is still ringing, and
it goes on damping while it is held — so a stroke played with the hand down is
a muted stroke, exactly as it would be on the real drum. Release the wheel and
the head is open again. **The pitch wheel** presses the head, which raises its
tension and bends the drum sharp; a stroke that is already ringing bends with
it rather than waiting for the next one.

## Controls

Twenty-two automatable parameters. Every one of them is a physical quantity, not
a voicing offset.

### The drum

| Control | Range | Default | What it changes |
| --- | --- | --- | --- |
| Head Diameter | 15–120 cm | 55 cm | The membrane radius. Pitch moves as 1/a; the modal *ratios* are fixed by the Bessel zeros and do not move at all |
| Body Depth | 0–100 % | 50 % | Enclosed volume. A shallow body is a stiffer air spring, so it splits the two heads further apart |
| Head Tension | 0–100 % | 55 % | 1.2–22 kN/m. Wave speed is √(T/σ) |
| Head Material | 0–100 % | 75 % | Thin synthetic film → thick cowhide. Sets areal density *and* internal loss, because both come from the same hide |
| Shell Material | 0–100 % | 80 % | Light laminated ply → dense carved zelkova. Moves the body's ring modes, their Q, and how much the rim absorbs |
| Resonant Head | 0–100 % | 50 % | Far head's tension relative to the batter head, 0.85×–1.15× |
| Air Coupling | 0–100 % | 85 % | How strongly the enclosed air ties the two heads together |
| Head Damping | 0–100 % | 35 % | Extra loss on top of the material's own |
| Shell Resonance | 0–100 % | 40 % | How much the body colours an ordinary head stroke |
| Pitch | ±24 st | 0.0 | Musical transposition, applied as head tension |

### The stroke

| Control | Range | Default | What it changes |
| --- | --- | --- | --- |
| Bachi Hardness | 0–100 % | 70 % | Felt beater → seasoned oak. Sets the Hertz contact stiffness |
| Strike Position | Centre 100 → Rim 100 | As written | Offsets every stroke's own radius |
| Velocity Depth | 0–100 % | 75 % | How far MIDI velocity moves the impact speed |
| Tension Mod | 0–100 % | 40 % | Attack pitch glide: a hard stroke stretches the head |
| Stick Noise | 0–100 % | 35 % | Broadband contact noise on the hide |
| Humanise | 0–100 % | 40 % | Per-stroke variation in position, angle, speed and contact time. At 0 the drum is a machine and repeats exactly |
| Octave Body | Tuned → Family | 70 % | How an octave is realised (see below) |

### The close pair and the output

| Control | Range | Default | What it changes |
| --- | --- | --- | --- |
| Mic Distance | 3–40 cm | 16 cm | How far the pair stands off the head |
| Mic Spread | 0–100 % | 55 % | How far apart the two microphones sit across the head |
| Stereo Width | 0–100 % | 50 % | Width trim. 50 % is exactly what the pair picked up, and is the default; 0 is an exact mono sum; above 50 % exaggerates the side signal past the measurement |
| Drive | 0–100 % | 0 % | Output-stage saturation, exactly bypassed at 0 |
| Output | −24 to +6 dB | −10.0 dB | Output level |

The default output is quieter than a synthesizer's usually is, deliberately: the
loudest stroke the instrument can make — a full-velocity rim shot on the largest
drum — sits about 9.7 dB above unity, and the default leaves that stroke just
under full scale rather than making a middling stroke as loud as possible.

## Sound engine

### The head

A circular membrane of radius *a* under tension *T* with areal density *σ* has
modes at *f(m,n) = c·λ(m,n) / 2πa*, where *c = √(T/σ)* and *λ(m,n)* is the *n*-th
zero of the Bessel function *J(m)*. Taikor runs twenty such modes — four
axisymmetric and sixteen with a circumferential order — and the ratios between
them are fixed constants of the geometry, which is why size and tension move the
whole drum together and only the air changes its shape.

Modes with a circumferential order come in degenerate pairs, the same shape
rotated by a quarter of its own period. A real head is never quite uniform, so
the pair sits a fraction of a percent apart and beats. That asymmetry belongs to
the hide rather than to the stroke, so it is seeded from a fixed constant: the
same drum splits the same way every time it is hit.

### The air on the head

The air a mode has to move rides along with it as added mass, and lowers it. How
much depends on how much air the mode actually displaces, so the fundamental is
loaded far more than the high modes, and a light synthetic head is loaded far
more than a heavy hide. That is why a thin head sounds lower than its tension
alone predicts.

### The air inside the body

A taiko is a closed drum, and the enclosed air is a spring between its two heads.
Only the axisymmetric modes can compress it — every other mode moves the same
amount of air in and out and leaves the volume unchanged — so the coupling is
applied to those modes alone, weighted by how much volume each one displaces.

The result is that each axisymmetric mode splits in two: a **breathing** mode
where both heads move outward together, lifted well above its uncoupled
frequency by the air spring, and a volume-preserving mode that is left roughly
where it was. On the default drum the pair lands at about 94 Hz and 134 Hz. The
breathing mode is also the one that radiates, because it is the one that changes
the drum's volume — which is why a sealed taiko is heard higher than its
membrane fundamental.

### The shell

The wooden body's ring modes come from the standard thin-cylinder result, so
the shell material moves their frequencies, their spacing and their Q together.
It is driven through the same force-over-modal-mass path the head uses, so a
heavy carved log genuinely refuses to move while a light laminated shell
genuinely rings — audibly so on the Katsu stroke, which hits the body directly.

### The stick

A bachi meets the hide as a Hertz contact. Contact duration goes as
*v^(−1/5)*, and is floored by the membrane's own resistive impedance
*8√(Tσ)*, because a stick cannot leave the head faster than the wave does. The
force pulse is the asymmetric *sin^1.5* arch a rounded tip actually produces, not
a symmetric bump.

The stick's mass scales with the drum being played, because nobody hits a
shime-daiko with an odaiko club. Leaving it fixed made the smallest drums about
twenty-five decibels louder than the largest — a property of the wrong stick
rather than of the instrument.

A hard stroke also stretches the head, raising its tension until it decays: the
attack pitch glide every large drum has.

### Two microphones, and where the stereo comes from

Taikor's output is a **close stereo pair**, and its image is a consequence of the
model rather than a widener bolted onto it.

A mode whose pattern on the head is finer than the sound it makes cannot
radiate: its field is evanescent and dies as *e^(−√(k(s)² − k²)·d)* above the
surface. Right on the head the two microphones therefore read the *shape* of the
membrane under them, and because every mode with a circumferential order reaches
two different points with a different sign and amplitude, the pair genuinely
decorrelates. A hand's width back, only what the drum radiates survives, and the
image closes towards mono. Backing the pair off narrows it and softens the slap
at the same time, because that is one mechanism and not two.

On top of that, each microphone hears the impact **through the air** from
wherever the stick landed, at its own distance and so at its own level and its
own arrival time. That is what places a stroke somewhere on the drum rather than
in the middle of it, and it is what keeps a spaced pair in phase on an edge
strike that the membrane modes alone would cancel.

Up to and including the default 50 % width — everything the microphones actually
captured — no stroke ever inverts, anywhere in the microphone range: the worst
case across all twelve strokes, both microphone controls fully swept, is a
correlation of about −0.03, which is a decorrelated pair rather than an
out-of-phase one. The regression suite sweeps that whole space. Past 50 % the
width control exaggerates the side signal beyond the measurement, and with the
pair close in and fully opened that can push strokes out of phase — the same
thing that happens when a real wide spaced pair is pushed through a widener, and
worth a phase check if the mix has to fold down.

### Octave Body

An octave can be bought either by halving the drum or by quadrupling its
tension. Both land on exactly the same pitch, and **Octave Body** chooses the
mixture. They do not sound the same, because the air load, the cavity stiffness
and the radiation efficiency all depend on the radius and none of them scale with
the tension. At *Tuned* the same drum is retuned; at *Family* the whole taiko
family sits under the hands at once, and a smaller drum sounds smaller rather
than merely higher.

### What is not modelled

The room, the player's body, the stand, and the far head's own radiation into
the space behind the drum. Two constants are calibrated rather than derived —
the overall depth of radiation damping, and how efficiently the shell reaches
the microphones — because both depend on how the drum is mounted, which this
model does not describe. Everything about how those terms *vary* with size,
material and stroke is computed.

## Interface

A resizable editor built around a drawing of the head itself. The twelve stroke
pads sit across the top with the note each one currently answers to; the octave
strip below selects the drum. The head display shows where the last stroke
landed, where the close pair is standing, and what the model says the drum is —
its sounding fundamental, its breathing mode and its tail length — all read from
the same solve the audio comes from.

The panel is drawn procedurally, so the project carries no binary image assets.

## Requirements

- macOS 11 or newer, Intel or Apple silicon
- CMake 3.22 or newer
- A full Xcode installation selected for command-line use
- Internet access on first configure, to fetch JUCE 8.0.14

The JUCE-free DSP target needs only a C++20 toolchain and CMake, which is the
path exercised by Linux CI.

## Build on macOS

```bash
cd taikor
./scripts/build-macos.sh
```

This configures an Xcode generator build, compiles universal `arm64`/`x86_64`
binaries, runs the CTest suites, and writes:

```text
build-macos/Taikor_artefacts/Release/VST3/Taikor.vst3
build-macos/Taikor_artefacts/Release/AU/Taikor.component
build-macos/Taikor_artefacts/Release/Standalone/Taikor.app
```

Set `BUILD_UNIVERSAL=OFF` for a native-architecture-only build, or `JUCE_PATH`
to point at a local JUCE 8.0.14 checkout instead of downloading it.

## Run the DSP tests without JUCE

```bash
cd taikor
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DTAIKOR_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The JUCE-free suite covers the stroke vocabulary and MIDI mapping, the octave
contract at every Octave Body setting, all twelve strokes at five sample rates,
sample-rate and block-size invariance, bit-exact determinism, the velocity and
contact-time laws, every physical control's effect on the solved drum *and* on
the rendered audio, the close pair's decorrelation and mono compatibility, tail
termination and exact idle silence, voice stealing, hostile input, and the
presentation mathematics the editor draws with. It also smoke-tests the
demonstration renderer.

## Regenerate the demonstration audio

```bash
cmake --build build-dsp --parallel --target TaikorRenderDemos
./build-dsp/TaikorRenderDemos Docs/audio
```

The render is deterministic and the tool rewrites the level table in
[`Docs/audio/README.md`](Docs/audio/README.md) in place, so the committed audio
and its documented levels stay in lockstep with the code. See that file for the
full manifest.

## Install locally

```bash
cp -R build-macos/Taikor_artefacts/Release/VST3/Taikor.vst3 \
  ~/Library/Audio/Plug-Ins/VST3/
cp -R build-macos/Taikor_artefacts/Release/AU/Taikor.component \
  ~/Library/Audio/Plug-Ins/Components/
```

Logic and GarageBand cache Audio Unit scans; run
`killall -9 AudioComponentRegistrar` and reopen the host if the new build does
not appear.

## Validate the plug-in

```bash
auval -v aumu Tko1 Tkor
```

`pluginval` is worth running against the VST3 as well:

```bash
pluginval --strictness-level 10 \
  build-macos/Taikor_artefacts/Release/VST3/Taikor.vst3
```

## Sign, package, and notarize

```bash
cd taikor
./scripts/sign-and-package-macos.sh
```

By default this ad-hoc signs the bundles and writes a ZIP and a PKG below
`build-macos/dist/`. The release version is read from the built bundles'
`CFBundleShortVersionString` rather than kept separately in the script, and the
three bundles are checked to agree with each other on both version and
architecture — so a mislabelled package cannot be produced. Setting `VERSION`
asserts an expected value rather than overriding it, and the script fails if it
disagrees with what was actually built.

Taikor's licence, its third-party notices and the JUCE licence are staged into
the installer and into each independently copyable bundle before signing, so the
signatures cover them and a bundle carried out of the package on its own still
carries the notices the MIT licence requires it to retain.

For distribution, supply your own identities:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Name (TEAMID)" \
NOTARY_PROFILE="your-notary-profile" \
./scripts/sign-and-package-macos.sh
```

Notarization is applied to the installer package rather than to the ZIP.

## Project layout

```text
CMakeLists.txt              Build definition for the DSP library, plug-in, tools and tests
Source/DSP/TaikoEngine.*    The physical model: membrane, cavity, shell, contact, microphones
Source/DSP/UiMath.*         JUCE-free presentation mathematics used by the editor
Source/PluginProcessor.*    JUCE processor, parameter layout, MIDI handling, state
Source/PluginEditor.*       Resizable editor, stroke pads, head display, metering
Tests/TaikoEngineTests.cpp  JUCE-free DSP and presentation regression suite
Tests/PluginProcessorTests.cpp  JUCE processor and editor contract tests
Tools/RenderDemos.cpp       Renders the committed demonstration WAVs
ThirdParty/                 Vendored JUCE licence text, staged into every package
Docs/audio/                 Twenty-three rendered demonstrations and their manifest
Presets/                    Preset guidance and drum-building reference
scripts/                    macOS build and packaging helpers
```

## Licensing

Taikor's original source is released under the [MIT License](LICENSE). It builds
against JUCE, which is not covered by that licence: JUCE 8 is dual-licensed
under AGPLv3 or a commercial JUCE licence, so confirm the applicable terms
before distributing a binary. See the
[third-party notices](THIRD_PARTY_NOTICES.md).

No samples, impulse responses, pretrained weights or third-party preset
libraries are included. The demonstration audio in `Docs/audio/` is generated by
this repository's own code.
