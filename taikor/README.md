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

**Within an octave, eight notes are eight different strokes, one per way of
hitting the drum. Between octaves, the drum itself changes: higher octave,
higher drum.**

That is the whole mapping. There are no keyswitches and no articulation menu —
the stroke is the pitch class and the drum is the octave. The octave stays
twelve semitones, because that is what has to line up with the keyboard, so the
four keys above the last stroke carry nothing and do not sound.

| Note | Stroke | Spoken as | Where the stick lands | What it is |
| --- | --- | --- | ---: | --- |
| C | Don | *don* | 0.15 | Full open stroke, a hand's width in from the middle |
| C♯ | Tsu | *tsu* | 0.20 | Damped centre, the free hand resting on the head |
| D | Su | *su* | 0.46 | Ghost stroke, light and well out from the middle |
| D♯ | Don Rim | *don* | 0.97 | Head and hoop struck together for a rim shot |
| E | Ka | *ka* | 0.91 | Out on the head near the tacks, thin and cutting |
| F | Katsu | *katsu* | 0.99 | Bachi on the wooden shell |
| F♯ | Buzz | *zu* | 0.32 | Press roll: the stick stays on the head |
| G | Bachi | *kata* | — | Stick against stick, with no drum at all |

Eight, and each of them is a different thing done to the drum rather than a
different amount of the same thing. There were twelve, and four were duplicates:
measured as band levels normalised to each stroke's own loudest band, Do sat
1.3 dB from Don, Kara 1.3 dB from Don Rim, Flam 1.8 dB from Do and Ko 3.0 dB
from Buzz — differences no listener can name, on keys a player has to remember.
The closest pair now is Don and Tsu at 2.9 dB, and those are the same strike
with and without a hand on the head, so they part company in time instead:
Tsu's sustain is a fifth of Don's.

Where the stick lands is why. It decides which modes the strike can reach at
all, so two strokes a few centimetres apart are the same stroke however
differently they are labelled — and the old set had three pairs inside four
centimetres of each other. Ka and Don Rim are the exception that proves it: they
sit close together out by the tacks and are still nothing like one another,
because one is on the head and the other is on the head and the hoop at once.
A difference of mechanism beats any amount of distance.

These are not eight presets. Each one is a strike position, a contact stiffness
and a mute state fed into the same model. A Ka is bright because striking the
head at 0.78 of its radius drives the modes that have a circumferential order and
barely moves the axisymmetric ones — which is exactly why it is bright on a real
taiko.

| Octave | Notes | Drum | Fundamental |
| --- | --- | --- | ---: |
| C1 | 24–35 | Past the end of the family: felt rather than heard | 10 Hz |
| C2 | 36–47 | Larger than anything ever actually built | 23 Hz |
| **C3** | **48–59** | **The ō-daiko the controls describe, unscaled** | **51 Hz** |
| C4 | 60–71 | Nagado-daiko: the everyday drum | 108 Hz |
| C5 | 72–83 | Shime-daiko territory: tight, high and short | 227 Hz |
| C6 | 84–95 | Smaller still | 466 Hz |

Notes outside 24–95 are silent.

The instrument opens on the big drum rather than on a middling one, because the
big drum is the point of a taiko. That does put the bottom two octaves below
where a drum can usefully be pitched — they are sub-bass, useful under something
else rather than on their own. Turn Head Diameter down if you would rather have
the family centred higher; the octave mapping scales with whatever drum the
controls describe.

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
| Head Diameter | 15–180 cm | 95 cm | The membrane radius. Pitch moves as 1/a; the modal *ratios* are fixed by the Bessel zeros and do not move at all. The default is an ō-daiko, so the instrument opens on its heaviest voice rather than asking you to go and find it |
| Body Depth | 0–100 % | 50 % | Enclosed volume. A shallow body is a stiffer air spring, so it splits the two heads further apart |
| Head Tension | 0–100 % | 55 % | 1.2–22 kN/m. Wave speed is √(T/σ) |
| Head Material | 0–100 % | 75 % | Thin synthetic film → thick cowhide. Sets areal density *and* internal loss, because both come from the same hide |
| Shell Material | 0–100 % | 80 % | Light laminated ply → dense carved zelkova. Moves the body's ring modes, their Q, and how much the rim absorbs |
| Resonant Head | 0–100 % | 50 % | Far head's tension relative to the batter head, 0.85×–1.15× |
| Air Coupling | 0–100 % | 85 % | How strongly the enclosed air ties the two heads together |
| Head Damping | 0–100 % | 50 % | Extra loss on top of the material's own, in the hide and at the rim. At zero the hoop is left free and a large drum will ring for seconds |
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
| Output | −24 to +6 dB | −20.0 dB | Output level |

The default output is far quieter than a synthesizer's usually is, deliberately.
A taiko is a very loud instrument with a very large crest factor, and this one
models the whole of it: the loudest stroke it can make — a full-velocity rim
shot on the largest drum — sits more than twenty decibels above unity. The
default leaves that stroke just under full scale rather than making a middling
stroke as loud as possible, so nothing in the instrument's range reaches the
safety limiter at the factory setting.

## Sound engine

### The head

A circular membrane of radius *a* under tension *T* with areal density *σ* has
modes at *f(m,n) = c·λ(m,n) / 2πa*, where *c = √(T/σ)* and *λ(m,n)* is the *n*-th
zero of the Bessel function *J(m)*. Taikor runs twenty such modes — four
axisymmetric and sixteen with a circumferential order — and the ratios between
them are fixed constants of the geometry, which is why size and tension move the
whole drum together and only the air changes its shape.

No stroke lands on the geometric centre, because every mode with a
circumferential order has *J(m)(0) = 0* and a strike at radius zero drives the
four axisymmetric modes and nothing else — a note with an attack and no body
behind it. A real taiko does the same thing if you manage to hit its exact
middle, which is why players do not: a full Don lands a hand's width in, close
enough to keep the fundamental and far enough out to wake the rest of the head.

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
where it was. On the default drum the pair lands at about 51 Hz and 88 Hz. The
breathing mode is also the one that radiates, because it is the one that changes
the drum's volume — which is why a sealed taiko is heard higher than its
membrane fundamental.

Because the breathing mode is the one that radiates, it is also the one that
empties first — on the default drum it is gone in about half a second while the
radial orders above it are still sounding after three. The tail figure the editor
shows is therefore not the fundamental's decay but the longest-lived branch any
stroke can drive, swept across the whole axisymmetric family; reporting the
fundamental's own decay understated a sealed drum by a factor of four.

Turn Air Coupling all the way down and there is no split at all: the two heads
are independent, and a stroke on the batter head cannot reach the far one. The
editor then shows the same figure for both, because an open body has one
axisymmetric mode you can hear rather than two. It matters which one is named:
with the resonant head slack, the far head's mode is the *lower* of the pair,
so reading off the lower frequency reported a mode that nothing was driving.

### The shell

The wooden body's ring modes come from the standard thin-cylinder result, so
the shell material moves their frequencies, their spacing and their Q together.
It is driven through the same force-over-modal-mass path the head uses, so a
heavy carved log genuinely refuses to move while a light laminated shell
genuinely rings — audibly so on the Katsu stroke, which hits the body directly.

The body's Q is low, because a drum shell is a thick, short piece of wood
clamped at both ends by the hoops rather than a free bar. That matters more than
it sounds: when the shell rang longer than the head, it put a wooden pitch on
top of the drum where the body should only have been adding weight, and it left
a hand laid on the head unable to damp anything anyone could still hear — since
a hand on the head does not touch the body.

### Above the modes: the head's continuum

A modal bank can only resolve so far. The mode table runs to the Bessel zeros
around *λ = 13*, which on a large drum puts the highest resolved mode a couple
of hundred hertz up — and a real head goes on having modes for another five
octaves above that, spaced far closer together than their own bandwidths. Nobody
hears those individually. What reaches the ear is a shaped burst that empties
from the top down, so that is what Taikor models: five overlapping bands of
noise, each carrying the head's own loss law and each lit by the same contact
that drives the modes.

Each band edge is a pair of one-poles in series, falling at twelve decibels an
octave rather than six. That is not a detail. A single pole's skirt falls so
slowly that the lowest band — which is also the loudest — was louder four
octaves up than the band that belonged there, so the whole continuum above its
first octave was inaudible underneath it and nothing that shaped the upper
bands could be heard at all.

It is not a decoration. Third-octave analysis of recorded taiko shows the attack
is nearly flat from sixty hertz to a kilohertz and still within twenty-five
decibels at ten; a bank that stops at three hundred hertz is short of that by
twenty to thirty-five decibels across the entire upper half of the spectrum, and
what is missing is exactly what a listener calls body. With the continuum in
place the model tracks those measurements to within a few decibels from two
hundred hertz upward.

It has to stay in its place, though, and its place is much smaller than it
looks. Left too loud it does not sit above the resolved bank, it buries it: the
sustain becomes a bed of noise with the drum's pitched ring somewhere
underneath, which measures as a body and does not sound like one. In the region
that matters most — the sustained low-mid both reference recordings put their
weight in — the continuum is a component and the modes are the instrument.

Three things fall out of modelling it as the head rather than as an effect. It
follows the contact: a force pulse of duration *τ* has nothing much above *1/τ*,
so a soft stroke — resting on the head nearly twice as long — cannot reach the
top of it, while a full-arm stroke lights all of it. It follows the strike
position, because the short-wavelength mode shapes pile up against the rim, so a
Ka reaches into it far harder than a Don. And the two microphones hear the
bottom of it in common and the top of it independently, because a wavelength
long against their spacing arrives at both alike and a short one does not —
which is why opening the pair now widens the drum's air and not merely its
partials.

### Where the body comes from

Four things take energy out of a struck head, and which of them dominates
decides whether the drum has a body at all.

**Radiation** is the largest, and it is the one that separates the modes. How
fast a mode loses energy to the air goes as the square of the air it actually
moves, and a mode with nodal circles moves very little: its annuli alternate in
sign and cancel before the sound has left the head. Integrating *J(0)(λr/a)*
over the disc gives a net volume of *2·J(1)(λ)/λ*, and the same *J(1)(λ)²*
appears in the modal mass, so the two cancel and what is left is a bare
*4/λ²* — the identical weighting the cavity coupling carries, for the identical
reason. Both are net-volume couplings.

That single factor is most of what a listener calls body. The fundamental is the
one mode of a head that behaves like a piston, so it radiates properly and is
also the first to go; everything above it is a poor radiator and rings two to
five times longer. Recordings of real taiko show exactly that ordering — the
band below eighty hertz drops twenty-three decibels from strike to body while
the band above it drops eleven.

**The hide's own loss** is viscoelastic, and that is two terms rather than one.
The hysteretic part has a frequency-independent loss angle and damps as *ω*; the
viscous part follows the rate of strain and damps as *ω²*. Only the pair works
here, because this model resolves the low modes individually and treats
everything above the modal overlap as a continuum, and no single power of *ω*
serves both: set for the body, the continuum rings for the best part of a second
as a bed of noise behind the drum; set for the continuum, the body is gone
before it is heard.

**The rim** takes the rest. It is the only term that does not scale with
frequency, which makes it the ceiling on how long anything can ring — a mode
cannot outlast *6.9/edgeLoss* however little else touches it — and measured
against recordings a real head wants a second and a half in its body, so it has
to stay small. Head Damping scales it from almost nothing to a great deal, so
the long ō-daiko boom is still there at the bottom of the control. A mode with a
circumferential order pays more of it, because those shapes are pressed against
the boundary rather than spread across the head.

**The mounting** takes what is left, and only at the very bottom. The lowest
modes of a drum do not stay in the head: they move the shell, the hoops and
whatever the drum is stood on. The term is steeply low-pass, because a mode has
to be long enough to move the whole instrument before any of this applies — and
that steepness is measured, not chosen. A gentler skirt reaches far enough up to
cost the body most of what makes it a body.

Where that shelf begins is a property of the drum, not an absolute pitch: a mode
moves the shell when its wavelength is on the order of the instrument's own
size, which is a comparison and therefore scale-invariant. The corner tracks the
radius, as every other frequency in the model already does. Pinned at a fixed
55 Hz it did the opposite of what it describes — a bigger drum slid its whole
modal set down through a shelf that did not move, so the stand ate more of the
instrument the larger the instrument got, and the o-daiko end of the keyboard
came out both the quietest and the shortest thing on it. Measured on the factory
drum, from an octave above the reference down two octaves below it: 108 / 51 /
23 / 10 Hz of loaded fundamental against 2.4 / 3.3 / 4.4 / 5.5 s of tail and
−14 / +5 / +10 / +15 dB in the 20–63 Hz band. The drum gets bigger in every way
that matters, rather than only in name.

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

### Two sticks, and nothing else

The twelfth stroke claps the bachi together and never touches the drum, so it is
modelled as an object in its own right rather than as a retuning of the body. A
bachi is a plain wooden dowel, so it rings in the free-free bending modes of a
solid cylinder, *f_n = (β_n L)² κ √(E/ρ) / (2πL²)* with *κ = r/2* and the *β_n L*
the roots of *cos x cosh x = 1*. That series — 1 : 2.76 : 5.40 : 8.93 — is
inharmonic, which is why a stick click reads as a clack and not as a note.

Bachi Hardness moves the wood's stiffness, its density and its internal loss
together, because they are three properties of one piece of material: a
felt-wrapped beater at one end, seasoned kashi oak at the other. On top of the
wood's own Q there is the hand, which is a resistance rather than a Q and so
takes a fixed number of decibels per second out of every mode. Without that term
the model is a *free* bar and the bottom octave rings for half a second, which is
a marimba rather than two sticks.

The octave picks a different pair: shortening a bar by √2 raises its bending
modes by exactly an octave while leaving it the same thickness, which is both
what a rack of bachi looks like and what keeps the stroke at a usable level.
Nothing in this path reads the drum — not its shell material, its head diameter,
its depth or its tension — and the collision is solved against the other stick's
own bending impedance rather than against the head's. Sharing the shell's modes
meant Shell Material moved the click by two octaves and, once the stretched modes
ran past Nyquist, the click actually fell in pitch going up the keyboard.

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

Fully opened, the two sit about fifty degrees of arc apart, which is what a
close pair over one head actually is. It is worth being strict about that: at a
hundred and twenty-six degrees the capsules sit either side of the nodal
diameter of every mode of order one, and the edge strokes — the ones that drive
those modes hardest — come out of phase.

Up to and including the default 50 % width — everything the microphones actually
captured — no stroke ever inverts, anywhere in the microphone range: the worst
case across all eight strokes, both microphone controls fully swept, is a
correlation of about +0.08, which is a decorrelated pair rather than an
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

A resizable editor built around a drawing of the head itself. The eight stroke
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
contract at every Octave Body setting, all eight strokes at five sample rates,
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
