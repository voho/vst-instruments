# Taikor

Taikor is a real-time **physically modeled taiko** for macOS. It does not load
samples, replay a recording, or emulate a particular branded instrument. Every
stroke is solved from a struck circular membrane: the head's modes come from the
zeros of a Bessel function, the air hanging off it lowers them, the enclosed body
couples the two heads together, the wooden shell rings underneath, and a dynamic
Hertzian bachi contact exchanges force with whatever the head is already doing.

Change the diameter and the pitch moves as one over the radius. Change the head
material and the drum gets heavier, darker, and more strongly loaded by the air.
Seal the body and the fundamental splits in two. None of that is scripted — it
falls out of the same solve.

> **Listen first.** Twenty-five [rendered demonstrations](Docs/audio/README.md)
> cover the four strokes, the four drums, the whole sixteen-note grid and every
> physical control swept across its range. They are the engine's own output
> rather than a recording of it, and the renderer rewrites their level table as
> it goes, so the manifest describes the audio beside it.

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

**Four drums, four strokes: a 4×4 grid. The octave chooses which drum, and the
bottom four semitones of it choose what is done to that drum.**

That is the whole mapping. There are no keyswitches and no articulation menu.
Sixteen notes, C3 to D♯6, and everything else on the keyboard is silent.

| | C — Don | C♯ — Ka | D — Tsu | D♯ — Don Rim |
| --- | --- | --- | --- | --- |
| **C6** Shime-daiko | 84 | 85 | 86 | 87 |
| **C5** Okedo-daiko | 72 | 73 | 74 | 75 |
| **C4** Chū-daiko | 60 | 61 | 62 | 63 |
| **C3** Ō-daiko | 48 | 49 | 50 | 51 |

### The four drums

Each octave is a **different instrument**, not the same drum at a different
size. Every one of them has its own diameter, its own body depth, its own hide
and its own shell, and those are the numbers a maker would read off the real
drum — a stave-built okedo is light and rings, a carved ō-daiko is heavy and
does not.

| Note | Drum | Head | Body | Hide | Shell | Sounds at | Fundamental |
| --- | --- | ---: | ---: | ---: | --- | ---: | ---: |
| C3 | **Ō-daiko** | 150 cm | 1.28 m, 0.85× wide | 1.05 mm cowhide | carved zelkova | 59.7 Hz | 32.7 Hz |
| C4 | **Chū-daiko** | 78 cm | 0.94 m, 1.20× wide | 0.85 mm cowhide | carved zelkova | 119.5 Hz | 68.0 Hz |
| C5 | **Okedo-daiko** | 40 cm | 0.50 m, 1.25× wide | 0.55 mm hide | stave-built, light | 238.6 Hz | 238.6 Hz |
| C6 | **Shime-daiko** | 30 cm | 0.21 m, 0.70× wide | 0.41 mm hide | carved, thick-walled | 477.3 Hz | 477.3 Hz |

Two columns, because on this family they are not the same number. A drum is
heard at whichever of its modes is loudest over the second or so a listener
takes a pitch from, and on the two large drums that is not the lowest one: the
(0,1) mode moves the two heads against each other, displaces no net air, and is
emptied by the stand and the hoops in half a second, while the (1,1) mode a
fifth and a half above it rings for two. On the two small drums the fundamental
sits far above where the mounting reaches and wins by fifteen decibels. **The
keyboard is octaves in the column that is heard, and it is deliberately not
octaves in the other one** — see [Drum Layout](#drum-layout).

Two things scope that claim, and both are properties of the instrument rather
than of the tuning.

The first is the stroke. The table is a centred full open stroke, which is what
Strike Position leaves the vocabulary at, and moving the stick changes which
partial a drum is heard at because it changes which modes the stroke can reach
at all. Towards the middle of the head every mode with a nodal diameter goes to
nothing — *J<sub>m</sub>*(0) = 0 — and what is left is the radial family, which
is higher. Measured from rendered audio, the chū-daiko is heard at 119.5 Hz at
Strike Position 0.00 and at 171.5 Hz at −0.25 and −0.50, a tritone up, because
its (0,2) branch has taken over from its (1,1). The panel's pitch
readout follows the stroke, so the number shown is the pitch of the stroke you
are playing; the keyboard's tuning does not, so Strike Position stays a timbre
control and cannot retune the instrument. On the ō-daiko struck at or very near
the middle there is no single answer to report: three partials land within a
decibel of one another and the drum has no one pitch there.

The second is how far the controls have been taken from the four instruments the
table describes. The mode each drum is tuned by is fixed to the mode that
instrument is heard at, rather than re-chosen from whatever is loudest at the
current settings, because re-choosing it makes the tuning a step function of
every control that feeds it — a hundredth of a semitone of Pitch automation used
to drop a drum by 1043 cents and re-solve its head from 24 cm to 40 cm. The
price is that a drum retuned a long way does eventually change which partial it
is heard at, and the four do not all change at the same point: with Pitch at
+7 semitones the ō-daiko is still heard at its (1,1) and the chū-daiko has
already crossed to its fundamental, and the step between those two pads reads
224 cents instead of 1200 until the ō-daiko crosses too, half a semitone later.
The same happens at a tight or a slack head, or a much smaller drum. Near the
factory instruments — which is where the table above is, and where the octave
strip's four drums are the four drums — every step is within seven cents of an
octave.

That is also why the ō-daiko is five *shaku* and not three. Three octaves of
sounding pitch is a factor of eight, and a family heard at its (1,1) at one end
and at its fundamental at the other has to span a factor of fourteen in the
fundamental to do it. No 95 cm head with a tacked cowhide on it is low enough to
be the bottom of that, and a five-*shaku* ō-daiko at an ordinary tacked tension
is — which is also the drum the bottom of a kumi-daiko set actually is.

The tensions follow from what each drum has to be brought to, which is how a
drum's tension is decided in life: **7.3 / 5.8 / 11.5 / 19.1 kN/m**. The two
*byō-uchi* drums — head nailed to the shell with iron tacks — sit at much the
same tension as each other; the two rope-laced ones sit far above them, and the
shime is held at two and a half times the ō-daiko on a head a third as
thick. That is the difference between tacking a head on and lacing it with
*shirabe* cord, and it is why a shime cracks where an ō-daiko booms.

They are four instruments and not four sizes, and the difference is in the
numbers no rescaling can move. A drum halved and its head tightened fourfold
keeps every ratio it had; these four do not share one:

| | Ō-daiko | Chū-daiko | Okedo | Shime |
| --- | ---: | ---: | ---: | ---: |
| Body depth ÷ diameter | 0.85 | 1.20 | 1.25 | 0.70 |
| Breathing mode ÷ fundamental | 1.88 | 1.39 | 1.07 | 1.08 |
| Head stiffness *B* (×10⁻⁴) | 0.72 | 1.53 | 0.65 | 0.24 |
| Top of the modal bank, above ideal | 9 ¢ | 19 ¢ | 8 ¢ | 3 ¢ |
| Ring, in cycles of its own fundamental | 136 | 204 | 347 | 804 |
| Hide, kg/m² | 1.05 | 0.85 | 0.55 | 0.41 |

Read that as sound. The ō-daiko's air splits its lowest pair by nearly a
seventh; the shime's shallow body barely splits it at all, so a shime is a much
more nearly pure pitch. The ō-daiko's thick hide opens the top of its modal bank
nine cents wide; the shime's thin one opens it three, so a shime is far closer to an
ideal membrane and reads as a *tone* where an ō-daiko reads as a *thud*. And
the okedo dies away soonest of the four in absolute time, not because it is
small but because a light stave shell absorbs two and a half times as much at
the rim as a solid zelkova log does.

### The four strokes

| Note | Stroke | Spoken as | Where the stick lands | What it is |
| --- | --- | --- | ---: | --- |
| C | Don | *don* | 0.15 | Full open stroke, a hand's width in from the middle |
| C♯ | Ka | *ka* | 0.91 | Out on the head near the tacks, thin and cutting |
| D | Tsu | *tsu* | 0.20 | Damped centre, the free hand resting on the head |
| D♯ | Don Rim | *don* | 0.97 | Head and hoop struck together, the loud accent |

Four, and each of them is a different thing done to the drum rather than a
different amount of the same thing. They come in two pairs, and each pair is
separated by a mechanism rather than by a distance. Don and Tsu land five
centimetres apart on a 150 cm head and are nothing like each other, because one
of them has the free hand resting on the hide: Tsu's sustain is half of Don's.
Ka and Don Rim land six centimetres apart out by the tacks and are nothing like
each other, because one is on the head and the other is on the head and the hoop
at once — only Don Rim beats the preload holding the tack line down.

Measured as band levels normalised to each stroke's own loudest band, so that
level cannot stand in for timbre, the closest pair of the four is Don against
Tsu at 4.5 dB and the widest is Tsu against Don Rim at 18.1 dB.

These are not four presets. Each one is a strike position, a contact stiffness
and a mute state fed into the same model. A Ka is bright because striking the
head at 0.91 of its radius drives the modes that have a circumferential order
and barely moves the axisymmetric ones — which is exactly why it is bright on a
real taiko. Their relative loudness still includes one calibrated articulation
factor, so it is not claimed to fall wholly out of impact speed; that same
factor now also participates reciprocally in the contact, so it is no longer
only a post-solve output trim.

There were eight. **Su** was a light Don, and velocity already covers it —
33 dB of it at full Velocity Depth. **Katsu** (bachi on the bare shell),
**Buzz** (press roll) and **Bachi** (stick against stick) were one technique
each; a press roll and a flam are things a player does with the notes they have,
and the demonstration audio plays both without a key of their own. The shell
still sounds under every stroke, and the tack line a rim shot needs is untouched.

**Velocity** sets the impact speed of the stick, from 0.12 m/s — a tip barely
leaving the head — to 6 m/s. The timbre change that comes with it is not a
separate control, because it is not a separate effect: Hertz contact time falls
as the fifth root of impact speed, so a harder stroke is shorter, brighter and
louder at once.

The mapping is geometric and nothing shapes it, so equal steps of MIDI velocity
are equal steps of decibels — which is what an arm does. At full Velocity Depth
that is 33.6 dB between a ghost stroke and a full blow on an open Don on the
ō-daiko, and 39.1 dB on the shime. The single most common complaint about the
sampled taiko libraries this competes with is that they have very little of
that; it is not a limitation a model has any reason to inherit.

**MIDI CC1** lays a finite palm-sized damping patch on the head. It damps
whatever is still ringing, and it goes on damping while it is held — so a stroke
played with the hand down is a muted stroke. Each mode loses energy according to
how much of its shape lies under that patch; the wooden shell and airborne stick
click remain untouched. Release CC1 and the head is open again.
**The pitch wheel** presses the head, which raises its
tension and bends the drum sharp; a stroke that is already ringing bends with
it rather than waiting for the next one.

The controls describe the ō-daiko at C3, and every control is carried across the
family as a trim on all four drums: turn Head Diameter down and the whole set
shrinks, keeping its proportions. **Drum Layout** switches between one design
retuned over the keyboard and the four independently scaled family members —
see below.

## Controls

Twenty-two automatable parameters. Every one of them is a physical quantity, not
a voicing offset.

### The drum

| Control | Range | Default | What it changes |
| --- | --- | --- | --- |
| Head Diameter | 15–180 cm | 150 cm | The ō-daiko's membrane radius, and a scale factor on the other three drums. Pitch moves as 1/a, and the modal ratios open out as the drum gets smaller because the head's own stiffness stops being negligible |
| Body Depth | 0–100 % | 50 % | Enclosed volume. A shallow body is a stiffer air spring, so it splits the two heads further apart |
| Head Tension | 0–100 % | 62 % | 1.2–22 kN/m. Wave speed is √(T/σ), and the tension is also what the head's stiffness has to compete with, so a slack head is more inharmonic than a tight one |
| Head Material | 0–100 % | 75 % | Thin synthetic film → thick cowhide. Sets areal density, internal loss *and* bending stiffness, because all three come from the same piece of material |
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
| Strike Position | Centre 100 → Rim 100 | As written | Offsets every stroke's own radius. It moves which partial the drum is heard at, and the pitch readout follows it; it does not retune the keyboard |
| Velocity Depth | 0–100 % | 75 % | How far MIDI velocity moves the impact speed |
| Tension Mod | 0–100 % | 40 % | Depth of the attack pitch glide, which is the head stretching itself: a hard stroke displaces the hide, a displaced hide is a longer and therefore tighter one, and the drum starts sharp. At 0 the head is treated as linear |
| Stick Noise | 0–100 % | 35 % | Broadband contact noise on the hide, and the rattle of the tack line when a stroke beats the preload holding the head down |
| Humanise | 0–100 % | 40 % | Per-stroke variation in position, angle, speed and contact time. At 0 the drum is a machine and repeats exactly |
| Drum Layout | 1 Drum / 4 Drums | 4 Drums | What a keyboard row represents: one design retuned, or an independently sized family member (see below) |

### The close pair and the output

| Control | Range | Default | What it changes |
| --- | --- | --- | --- |
| Mic Distance | 3–40 cm | 16 cm | How far the pair stands off the head |
| Mic Spread | 0–100 % | 55 % | How far apart the two microphones sit across the head |
| Stereo Width | 0–100 % | 50 % | Width trim. 50 % is exactly what the pair picked up, and is the default; 0 is an exact mono sum; above 50 % exaggerates the side signal past the measurement |
| Drive | 0–100 % | 0 % | Output-stage saturation, exactly bypassed at 0 |
| Output | −24 to +6 dB | −22.5 dB | Output level |

The default output is far quieter than a synthesizer's usually is, deliberately.
A taiko is a very loud instrument with a very large crest factor, and this one
models the whole of it: the loudest stroke it can make — a full-velocity rim
shot — sits well above unity, and the bottom of the range is a ghost stroke
thirty-three decibels below a full blow on the same drum. At the default the
whole grid stays clear of the safety limiter: the hardest rim shot peaks at
−0.8 dBFS on the ō-daiko, which is the loudest of the sixteen keys, and −2.7 on
the okedo. It came down two and a half decibels when the reference ō-daiko went
from three *shaku* to five, because a rim shot catches the hoop and the body as
well as the head and the body of a five-*shaku* drum is a great deal more of the
stroke.

## Sound engine

### The head

A circular membrane of radius *a* under tension *T* with areal density *σ* has
modes at *f(m,n) = c·λ(m,n) / 2πa*, where *c = √(T/σ)* and *λ(m,n)* is the *n*-th
zero of the Bessel function *J(m)*. Taikor runs twenty such modes — four
axisymmetric and sixteen with a circumferential order.

A taiko head is not that membrane, though, and the difference is audible. It is
chemically treated cowhide with a Young's modulus around 3.5 GPa, several
tenths of a millimetre thick, held at a tension far above a drum-kit head's — a
*stretched plate*, in the acoustics literature, rather than an ideal membrane.
A plate resists bending as well as stretching, and bending adds a term in the
fourth power of the wavenumber to *ω²*: *ω² = (T k² + D k⁴)/σ*, with the
flexural rigidity *D = E h³ / 12(1 − ν²)*. So the ratios between the modes are
not constants of the geometry. They open out with the mode's order, and they
open out further the smaller the drum and the thicker its hide.

That single term is most of what separates a shime-daiko's spectrum from an
ō-daiko's, and on this instrument it is a measured difference between two of the
four drums rather than a claim. The top of the resolved bank sits 30 cents above
where an ideal membrane would put it on the ō-daiko, 33 on the chū-daiko, 13 on
the okedo and 6 on the shime — because *B = D/(Ta²)* falls as the hide gets
thinner (as the cube of its thickness) and as it is pulled tighter, and both of
those go the same way up the family. Head Material moves it as hard again: a
thin synthetic film is an ideal membrane to within half a cent, and a thick hide
stretches its top mode by well over a semitone.

The stretch is taken relative to the *(0,1)* mode rather than applied
absolutely, because a drum is tuned by the pitch it sounds. A player brings the
fundamental back where it belongs with the ropes or the tacks, and what
stiffness leaves behind afterwards is the spread above it. That is also what
keeps an octave an octave: the stiffness parameter falls as the tension rises
and as the square of the radius, so the two Drum Layout constructions reach the
same tuning by different physical routes and an absolute shift would put the
keyboard out of tune with itself.

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
where it was. On the default drum the pair lands at about 51 Hz and 84 Hz. The
breathing mode is also the one that radiates, because it is the one that changes
the drum's volume — which is why a sealed taiko is heard higher than its
membrane fundamental.

The spring is a column and not an infinite one. *ρc²/L* is what a cavity is
worth only while the wavelength runs away from the body, and this instrument
leaves that limit inside its own range: the body's first axial resonance is
212 Hz on the ō-daiko, 259 on the chū-daiko, 343 on the okedo and 819 on the
shime, every one of them inside the resolved bank of the drum it belongs to.
What each head actually drives is a rigidly terminated column of length *L/2* —
the volume-changing motion is symmetric about the midplane, so that plane
behaves like a wall — and its stiffness is *x cot x* times the lumped value,
with *x = ωL/2c*. That is the same number at low frequency and less than it as
the body gets deep against the wavelength: 0.82 on the ō-daiko, 0.77 on the
chū-daiko, 0.49 on the okedo — the longest body in the family relative to its
head — and 0.65 on the shime, falling to 0.05 there at full Body Depth. It has
to be solved for rather than computed, because the stiffness depends on the
frequency it sets, so the drum resolve bisects on it once per drum and the audio
never sees the iteration. The model reports it for the same reason: it is an
answer the drum has to converge on rather than an expression anything can write
down.

Musically this is what stops a long-bodied drum being an air spring with a hide
attached, and it is why the four drums split their lowest pair so differently:
1.88, 1.39, 1.07 and 1.08 times the fundamental going up the family. The
keyboard is an octave in the pitch each drum is heard at and it is not an octave
in the breathing branch, which steps 747 / 1727 / 1206 cents — because the branch
above the fundamental is lifted by a column whose length is a property of each
instrument rather than of a scaling.

Where the column passes its own quarter-wave the stiffness reaches zero and the
two heads stop being tied together at all. Above that the air is mass-like
rather than stiff, which is a real thing this model has nowhere to put, so the
answer there is the one an open body already gets: one axisymmetric mode,
reported twice. It takes a body longer than half its head's own wavelength to
reach, which is the same thing as the half column passing its quarter-wave — an
8 cm body under a head at three and a half kilohertz, whose half wavelength is
4.9 cm — so no drum with a taiko's proportions is near it, but the controls will
build one that is.

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
genuinely rings — audibly so on a Don Rim, which catches the hoop and the body
along with the head, and audibly so between the drums: the okedo's stave shell
takes two and a half times as much out of the head at the rim as the ō-daiko's
solid zelkova does, which is why it is the driest of the four.

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

Each octave is a serial two-pole high-pass followed by a seven-pole low-pass.
Its lower skirt therefore falls at twelve decibels per octave and its upper one
at forty-two, keeping the loud crossover region out of the four octaves above
it. The exact stationary variance of that nine-state filter is solved once when
the stroke is built, so its target RMS does not inherit the filter geometry or
where the band lies against Nyquist.

It is not a decoration. Third-octave analysis of recorded taiko shows the attack
is nearly flat from sixty hertz to a kilohertz and still within twenty-five
decibels at ten; a bank that stops at three hundred hertz is short of that by
twenty to thirty-five decibels across the entire upper half of the spectrum, and
what is missing is exactly what a listener calls body. With the continuum in
place the model tracks those measurements to within a few decibels from two
hundred hertz upward.

What sets its weight is the head's own modal receptance — the velocity a unit
force gets out of the drum — observed through the same microphone factor the
resolved modes are observed through. That is a property of the drum and of
nothing else. Regression tests isolate the uppermost statistical band at 44.1,
48, 96 and 192 kHz and require it to remain within 2 dB; the complete 4–10 kHz
response remains within 1.5 dB. Every higher octave is also measured in
isolation, so a lower band's skirt cannot masquerade as the whole statistical
tail.

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
Ka reaches into it eleven decibels harder than a Don. And the two microphones hear the
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
instrument the larger the instrument got, and the ō-daiko end of the keyboard
came out both the quietest and the shortest thing on it. Measured across the
four drums: 60 / 119 / 239 / 477 Hz of sounding pitch against 4.2 / 3.0 /
1.5 / 1.7 s of tail, and a full-velocity Don's 20–63 Hz band running 0 / −13.9 /
−28.0 / −36.9 dB against the ō-daiko's. The big drum is bigger in every way that
matters, rather than only in name — and the shime outlasts the okedo despite
being smaller, because its dense carved body absorbs a third of what the okedo's
staves do.

### The stick

A bachi is a moving mass, not a prescribed force envelope. MIDI velocity gives
it an incoming speed; its compression against the moving head produces a
Hertzian *δ^1.5* force with constrained Hunt–Crossley loss, and contact ends
when stick and hide actually separate. Duration, peak force and rebound all
emerge from that coupled motion.

The contact is advanced with a discrete-gradient IMP-2 scheme whose free modal
poles exactly match the drum's existing resonators. The same spatial projection
senses head displacement and spreads force back into the modes, so the coupling
is reciprocal and cannot pull on the hide. Simultaneous bachi contacts are solved
together through one small symmetric compliance system, rather than in an
audibly order-dependent sequence.

The stick's mass scales with the drum being played, because nobody hits a
shime-daiko with an odaiko club. Leaving it fixed made the smallest drums about
twenty-five decibels louder than the largest — a property of the wrong stick
rather than of the instrument.

### The attack pitch glide

A membrane clamped at its rim cannot move without getting longer, and a longer
head is a tighter one. The tension it gains goes as the square of its
displacement — the von Kármán / Berger term — and the pitch as the square root
of the tension, so a struck head starts sharp and settles. That is the attack
glide every large drum has, and it is the whole of the mechanism: there is no
envelope and no time constant anywhere in it. The glide ends when the head has
stopped moving, so it decays at the head's own rate rather than on a clock.

Everything follows from that without being written down separately. A hard
stroke bends further than a light one because it pushes the head further — about
fifty-five cents against a couple at the factory setting. A slack head bends far
more than a tight one, because the tension a given displacement adds is measured
against the tension already there: the same full stroke bends about 140 cents at
a quarter of the tension range and about 15 at four fifths of it. A Ka barely
bends the head at all, because it lands at 0.91 of the radius where the modes
that carry the head's displacement are all but nodal. And the depth is computed
after the model's one output-level calibration has been divided out, so that
constant cannot reach the drum's pitch.

It is a first-order expansion, so it is applied through a form that agrees with
it exactly while the displacement is small and saturates where the expansion
stops describing the head. That matters at the edge of the controls rather than
in the middle of them: the fractional tension rise goes as the fourth inverse
power of the radius, so the smallest head at no tension reached fifteen
semitones of bend before it was bounded, and the factory drum reaches a tenth of
a tension at full velocity.

The glide carries the head's continuum with it as well as its resolved modes,
because the continuum is the same head. That is the whole of what Tension Mod
does above a kilohertz, and it is worth about a decibel across the control —
a bend, not a brightness control. Rewriting a running resonator's coefficients
under its own state is not exactly energy-conserving, and it is easy to assume
the difference is heard as spray; measured with the continuum silenced, over
30–80 ms with a settled high-pass and no analysis window involved, what the
rewrite leaves above 1.2 kHz sits 102 dB under the stroke that made it and does
not move with Tension Mod at all.

### The tack line

A nagado-daiko is *byō-uchi*: the head is not roped on, it is nailed to the
shell with a ring of iron tacks. Each of them holds down the head's tension
over its share of the circumference — a few hundred newtons on the factory drum
— and a stroke that catches the hoop has to beat that before it lifts the head
at all. Past it the tacks chatter against the wood, which is the metallic edge a
firm rim shot has and a light one has no trace of at all.

It is a threshold rather than a level, so it does not fade in: below the preload
there is nothing. Raising Head Tension or Head Diameter raises the preload,
because both raise the tension a single tack carries, so a tighter or a larger
drum wants a harder stroke before it rattles. Stick Noise owns the level, since
this is contact noise. And it is the one part of the instrument that does not
scale with the drum: a byō is a nail, and the same nails go into a chū-daiko and
an ō-daiko, so the rattle keeps its own band across the whole family. Only Don
Rim beats the preload at ordinary velocities; a Ka reaches the hoop with a third
of the force and leaves the tacks alone.

### A drum has one head

A stroke lands on whatever the head is already doing. Each bachi senses the
surface velocity made by all resolved membrane modes at its own strike point,
and its force is returned through exactly the same shapes and modal masses. A
centre stroke therefore couples strongly to the boom; an edge stroke largely
leaves that boom alone and works on the circumferential modes instead. A second
stroke can shorten, reinforce or delay its contact according to the phase of the
already-moving hide—none of those interactions is scripted.

The nonlinear solve is passive in the resolved head-plus-stick coordinates: it
never creates an adhesive force, and without player input their discrete total
energy cannot rise. Modal displacement stays continuous through every contact.
The live poles used by the solve follow attack glide, automation and pitch-wheel
retuning, so the contact never exchanges energy with a different recurrence from
the one that is actually rendered.

A Tsu goes further. Its free hand remains for 180 ms as a 55 mm-radius local
dashpot on motion already ringing on that drum. A symmetric five-point patch
quadrature projects the contact into every membrane mode; patch area and modal
mass make the same palm bite harder on a small head than on a five-shaku one.
Control updates set a continuous extra pole loss without stepping either modal
displacement or physical velocity. The continuum receives the corresponding
phase-averaged RMS loss, while every other drum remains bit-identical.

Each playable drum now has one canonical ringing state. MIDI notes allocate
contact transients, project their forces into stable physical-mode IDs, and are
summed before the bank advances once; they do not own another rendered head.
The residual continuum is likewise one persistent field per drum, with new
contacts adding energy in quadrature. Contact slots still reuse the old `Voice`
storage type and therefore carry some unused arrays; that is storage debt, not a
second physical drum.

### Two microphones, and where the stereo comes from

Taikor's output is a **close stereo pair**, and its image is a consequence of the
model rather than a widener bolted onto it.

A mode whose pattern on the head is finer than the sound it makes cannot
radiate: its field is evanescent and dies as *e^(−√(k(s)² − k²)·d)* above the
surface. Right on the head the two microphones therefore read the *shape* of the
membrane under them, and because every mode with a circumferential order reaches
two different points with a different sign and amplitude, the pair genuinely
decorrelates. A hand's width back, only what the drum radiates survives, and the
image closes towards mono. That narrowing is a mechanism rather than a width
control, and it is the whole of what Mic Distance does to the image.

It is not also a softening. The narrowing belongs to the resolved bank, where the
evanescent term is per mode; everything above the bank is carried by the
continuum, whose only distance dependence is one flat gain taken from the loudest
resolved mode's own microphone factor. Swept from 3 cm to 40 cm the drum loses
17.6 dB at 400–1200 Hz and 13.6 dB at 4–10 kHz, so backing the pair off leaves it
four decibels *brighter* relative to itself. The level law is right and the tilt
is missing; it is recorded below under what is not modelled.

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
case over the four strokes on all four drums, both microphone controls swept in
tenths, is a correlation of +0.19 — a Ka on the shime with the pair right down
on the head and fully opened — which is a decorrelated pair rather than an
out-of-phase one. The regression suite sweeps that space on the ō-daiko, at
every stroke and at both ends of both controls, and it reaches +0.31 there. Past 50 % the
width control exaggerates the side signal beyond the measurement, and with the
pair close in and fully opened that can push strokes out of phase — the same
thing that happens when a real wide spaced pair is pushed through a widener, and
worth a phase check if the mix has to fold down.

### Drum Layout

**Drum Layout** is a two-position physical choice:

- **1 Drum** uses the ō-daiko design described by the controls on every keyboard
  row and reaches the four pitches by retuning its head.
- **4 Drums** — the default — resolves each row as its own ō-daiko, chū-daiko,
  okedo-daiko or shime-daiko, with an independent diameter, body, hide and shell.

Both layouts land on the same four sounding pitches — 59.66 / 119.32 / 238.64 /
477.28 Hz — but they do not sound alike. Air load, cavity stiffness, radiation
and modal density depend on physical size and cannot be reproduced by pitch
shifting one body. The model therefore solves the required tension/size rather
than transposing a recording.

“1 Drum” describes one physical *design*, not monophony. Each row still owns an
independent ringing state, so a chord can hold four differently tuned copies and
a new row never steals another row's tail. “4 Drums” instead gives those four
states the four family geometries in the table.

This used to be a continuous morph called Octave Body. Intermediate geometry
was not physically useful: one necessary modal-identity handover changed head
size and decay abruptly even though the target pitch stayed fixed. Old projects
keep the same parameter ID and restore safely, with values below the midpoint
mapping to 1 Drum and values at or above it mapping to 4 Drums.

The mode each row is tuned by is latched to the one that instrument is heard at,
rather than re-selected from whichever peak happens to win under automation.
At the factory voicing, rendered Don and Tsu strokes across the rows are within
7 cents of exact octaves. Far from the calibrated family a different partial can
become dominant; the tuning itself remains continuous, but a listener may then
name another partial as the drum's pitch.

### What is not modelled

The room, the player's body, the stand, and the far head's own radiation into
the space behind the drum. The enclosed air carries the stiffness of a finite
column but not its mass or its own resonances: above the body's first axial
resonance — 212 Hz on the ō-daiko, 819 on the shime, and between 139 and 451 Hz
across Body Depth on the ō-daiko — the column is treated as absent rather than
as the mass it becomes.
Nothing anywhere associates a loss with the enclosed air either, so Body Depth
moves the pitch of the split and never the decay of either branch. That last one
is an omission with a number behind it rather than an oversight: thermal
exchange with the walls gives the cavity a loss factor around 1e-4, which is
three orders of magnitude under what radiation is already taking out of the same
mode, so modelling it would change nothing anyone could hear.

The column is solved once per drum, on the branch of the lowest axisymmetric
pair that changes the body's volume, and every axisymmetric mode above that pair
then reads the answer — so those modes get a column evaluated at a frequency
that is not theirs. Solving each pair on its own factor instead moves the
second, third and fourth of them by 8.0, 3.6 and 1.0 cents on the ō-daiko, and
leaves the pair the keyboard is tuned by exactly where it is.

The statistical continuum is not yet a mechanical load in the bachi solve.
Contact sees the forty resolved membrane coordinates; afterwards its force
history excites the higher stochastic bands, but those bands exert no reciprocal
force on the stick. On the two large drums the first continuum band also begins
before the deterministic modes have reached statistical overlap. Closing that
gap needs a measured complex driving-point mobility and a passive dynamic
residual—not an uncalibrated resistance, which captures soft and edge strokes
instead of returning their stored energy.

Above the resolved bank the microphones have a level and not a shape. The
near-field and proximity terms that make the pair decorrelate are computed per
mode, and the continuum sits above the modes, so it takes the distance law of
the drum's loudest resolved mode as a single flat gain. Mic Distance therefore
moves the whole region's level correctly and its tilt not at all.

Only the struck body rings. The contact force acts equally and oppositely on
both bodies, but on all four strokes the stick that struck the drum is a force
and not a sounding object: nothing here rings the bachi. The engine used to
carry a free-free bar model for one, reached by the stick-against-stick stroke,
and that stroke is no longer on the grid — so the model went with it rather than
sitting unreachable. Building it back for the striker is cheap in modes and
expensive in level, because what a bachi is worth against the drum it is hitting
was only ever pinned by how the stick-against-stick stroke sounded.

Six constants are calibrated rather than derived, and each of them sets the
*depth* of a term whose shape is computed: the overall level of radiation
damping, and how efficiently the shell, the airborne click and a lifted tack
reach the microphones — the first three of those turn on how the drum is mounted
and where the player is standing, neither of which this model describes, and the
fourth on the radiating efficiency of a 6 mm iron head against wood; the weight
of the head's high-frequency continuum against its resolved bank, which is a time in
seconds because what it multiplies is the head's receptance; and the shape
factor of the attack pitch glide, which stands in for the difference between the
modal states the engine has and the mean square slope the tension rise depends
on. Everything about how those terms *vary* with size, material, position and
stroke is computed.

## Interface

A resizable editor built around a drawing of the head itself. Each painted drum
portrait sits directly beside the four strokes that trigger it, so the playing
surface reads as the same four-by-four map as the keyboard: one drum per row and
one stroke per column. Clicking a portrait selects that drum for the live head
readout. The head display shows where the last stroke landed, where the close
pair is standing, and what the model says the drum is — the pitch it is heard
at, its breathing mode and its tail length — all read from the same solve the
audio comes from.

The pitch it names is always a partial the engine will actually build at the
host's sample rate. A resonator at or above 0.98 of Nyquist is refused, and on
the smallest tightest head this instrument reaches — 15 cm at the tension
ceiling, on a thin film, transposed up an octave — the top pad's own fundamental
is 25.6 kHz, which is above that at 44.1 and 48 kHz and below it at 96 and 192.
Where a drum has no membrane mode the engine can sound at all, the display says
**no pitch** rather than naming one nothing will play. It still has a body and a
head continuum; what it does not have is a partial to be tuned to.

The controls, strike maps and live head are drawn procedurally over an embedded
dark sumi-e landscape. The four drum portraits are a second embedded
ink-and-mineral-pigment atlas. Both ship inside every plug-in format and require
no files beside the bundle.

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

The JUCE-free suite covers the playing grid and its MIDI mapping — sixteen notes
and exact silence everywhere else — both Drum Layout endpoints, the four strokes
being mutually distinct, the octave contract including that an octave is an
octave in the pitch the drum sounds, in the readout and in the rendered partial,
that the pitch the panel reports is always
a partial the renderer will actually build at the host's sample rate, that the
tuning is continuous under automation — a fine sweep across three settings where two of a
drum's modes cross, asserting from rendered audio that no automation step moves
the heard pitch by more than a fraction of what the step itself is worth — that
the reported pitch is the pitch of the stroke actually being played across
Strike Position, all four strokes at
five sample rates, sample-rate and block-size invariance
including the level of the head's continuum, bit-exact determinism, the velocity
and contact-time laws, the instrument's dynamic range and the evenness of its
velocity response, the head's bending stiffness and the modal ratios it opens
out, the enclosed air solved as a finite column rather than an infinite spring,
the attack glide's dependence on the head rather than on a clock and its silence
above the resolved bank, the tack line's threshold, what one passive,
non-adhesive nonlinear contact does to a head another stroke left ringing —
including simultaneous-contact order invariance and energy checks at every
supported rate, finite-area Tsu and CC1 palms whose recovered damping follows
physical head area rather than host sample rate, and a muted Tsu that damps only
the already-ringing drum — every isolated continuum octave's
ownership of its own band, every physical control's effect on the
solved drum *and* on the rendered audio, the close pair's decorrelation and mono
compatibility, tail termination and exact idle silence, voice stealing, hostile
input, and the presentation mathematics the editor draws with. It also
smoke-tests the demonstration renderer.

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
Docs/audio/                 Twenty-five rendered demonstrations and their manifest
Docs/calibration/           Controlled force, head-motion and microphone capture contract
Docs/best-in-class-plan.md  Competitive landscape, gap analysis and the work it drove
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

## Changelog

- 2026-08-15: Stopped the nonlinear stick-contact solver from re-zeroing its 16x16 Jacobian on every Newton iteration, since each entry it can read is already overwritten before use, which is a bit-exact no-op confirmed by matching demo-audio checksums.
- 2026-08-15: Extracted the (0,1) pair's diagonal/off-diagonal cavity-coupling
  terms - previously written out identically in both `volumeBranchOmega`'s
  bisection trial and `solveAxisymmetricPair`'s converged solve - into one
  shared `axisymmetricDiagonals` helper, with no change to any resolved drum
  or rendered audio.
- 2026-08-15: Deduplicated the two near-identical per-band continuum-energy injection loops in `renderVoice` (scheduled-contact arrival and the running nonlinear-solve step) into one shared `injectContinuumEnergy` helper, with no change to engine output verified by bit-identical demo renders before and after.
- 2026-08-15: Extracted the near-field microphone attenuation term - written out identically four times across `buildVoiceModes`'s two membrane-mode families and `observeMode`'s matching readout - into one shared `nearFieldAttenuation` helper, with no change to any resolved drum or rendered audio.
- 2026-08-15: Extracted the air-loaded batter/resonant angular frequency of a
  general membrane mode - rebuilt identically in `observeMode`'s per-mode
  readout, `buildVoiceModes`'s per-entry solve and `measure()`'s tail-length
  sweep - into one shared `membraneModeOmegas` helper, with no change to any
  resolved drum or rendered audio, verified by matching DSP test results and
  bit-identical demo renders before and after.
- 2026-08-15: Stopped `process()`'s per-sample loop from re-zeroing every one of the sixteen voice slots' solved-contact fields regardless of activity, since `trigger()` and `silenceVoice()` already zero them the moment a voice is armed or retired, so an inactive slot was always reading zero anyway; now only active voices are touched, which is a no-op on rendered audio confirmed by bit-identical demo checksums before and after.
- 2026-08-15: Extracted the "silence every voice and every physical bank" loop pair - written out identically in both `reset()` and `allSoundsOff()` - into one shared `silenceAllVoices()` helper, a pure code-motion change with no effect on any resolved drum or rendered audio, confirmed by the DSP test suite and bit-identical demo renders before and after.
- 2026-08-15: Stopped `buildVoiceModes` and `dynamicSoundingMode` from recovering a mode's pole radius with a separate `std::sqrt (mode.resonator.a2)` right after `configureResonator` had already solved that exact radius internally; `configureResonator` now hands it back through an optional out-parameter instead, and `dynamicSoundingMode` reads the already-stored `mode.poleRadius` (as `setPalmDecay`, `applyCollisionRetention` and `applyTensionShift` already did) rather than re-deriving it. Since IEEE 754 multiplication and `sqrt` are both correctly rounded, `sqrt(radius * radius)` recovers `radius` bit-exactly, so this is a pure redundant-call removal, confirmed by the DSP test suite and bit-identical demo renders before and after.
- 2026-08-15: Widened the `axisymmetricDiagonals` helper's use from the (0,1) pair alone to every axisymmetric mode entry: `observeMode`'s and `buildVoiceModes`'s order-0 branch and `measure()`'s tail-length sweep each rebuilt the same cavity/diagonalB/diagonalR/offDiagonal formula by hand instead of calling it, so all three now pass their own entry's lambda/omegaBatter/omegaResonant through the existing helper. The arithmetic and operation order are unchanged, so this is a pure deduplication confirmed by the DSP test suite and bit-identical demo renders before and after.
- 2026-08-16: Extracted the axisymmetric batter-participation fraction - the clamped `drum.batterDensity * mode.batterParticipation * mode.batterParticipation` used only for a circumferential-order-zero mode, else unity - into one shared `batterFractionFor` helper, replacing the identical formula written out separately in `ensurePhysicalDrum`'s CC1 palm-rate cache and `dampPhysicalDrum`'s per-Tsu local mute damping. A pure code-motion change with no effect on any resolved drum or rendered audio, confirmed by the DSP test suite and bit-identical demo renders before and after.
- 2026-08-16: Added regression coverage for several `UiMath` defensive/clamp branches that were exercised by callers but never asserted directly: `onePoleCoefficient`/`decayMultiplier`'s negative time-constant and update-rate fallbacks, `meterPositionForLinear`/`linearForMeterPosition`'s non-negative-floor guard, `MeterBallistics::update`'s NaN attack/release/peak-fall coefficients (which clamp to zero via the shared `clamp()`, not to some other default), and `mix`'s out-of-range and NaN amount clamping; test-only change, verified with `ctest --test-dir taikor/build-dsp` (all tests pass) and a demo re-render showing `git status taikor/Docs/audio` clean.
- 2026-08-16: Extracted the close-microphone proximity lift - `1 + micProximity / (1 + (frequency/190)^2)` - into one shared `proximityLift` helper, replacing four identical copies written out separately across `observeMode`'s axisymmetric and circumferential branches and `buildVoiceModes`'s matching two mode families. A pure code-motion change with no effect on any resolved drum or rendered audio, confirmed by the DSP test suite and bit-identical demo renders before and after.
- 2026-08-16: Added direct regression coverage for `TaikoEngine::sanitise()`, the boundary every host-supplied `EngineParameters` block passes through: each of its twenty-one clamped fields is now asserted against its documented bound for a huge excursion, +/-infinity and NaN, and `octaveBody`'s 0.5 collapse-to-endpoint threshold is checked on both sides and exactly at the boundary. `testInvalidInputSafety` already threw every field out of range at once and checked only that the resulting audio stayed finite, which cannot tell a field clamped to the wrong bound from one clamped to the right one; test-only change, verified with `ctest --test-dir taikor/build-dsp` (all tests pass, including the new assertions) and `git status taikor/Docs/audio` clean since no engine code changed.
