# Drumalor

Drumalor is a real-time procedural drum instrument for macOS. It uses classic
analogue-drum synthesis techniques as a starting point, then generates every
hit locally from oscillators, noise, filters, envelopes, resonators, and
circuit-inspired nonlinear shaping. It does not load samples, copy a ROM,
emulate a particular branded machine, or contact a service while rendering
audio.

![Drumalor instrument interface](Docs/screenshots/drumalor-standalone.png)

The screenshot is the actual Standalone application, captured from the version
1.0 build; the 1.1 editor adds the kit mixer, kit bus deck, and metering
described below and has not been re-captured, because that requires a macOS
build. The VST3 and Audio Unit use the same resizable JUCE editor.

Drumalor provides **13 separately playable synthesized voices**: Kick, Snare,
Clap, Closed and Open Hats, Ride, Crash, Low/Mid/High Toms, Shaker, and two
percussion voices. Each has its own MIDI mapping, synthesis character, pitch,
and decay controls.

> **Listen first.** Seven [rendered demonstrations](Docs/audio/README.md) cover
> the full thirteen-voice kit one hit at a time, programmed grooves with ghost
> notes and a bar played on the hi-hat pedal, snare velocity and its three
> articulations, toms and cymbals, the Humanise control and the kit bus. They
> are rendered from the shipping signal path by the repository's own renderer
> rather than drawn by hand, and are re-rendered whenever the engine moves.

The project builds three products from one JUCE codebase:

- VST3 instrument for hosts such as Ableton Live, REAPER, Cubase, and Bitwig
- Audio Unit v2 music device for Logic Pro and GarageBand
- Standalone application for direct MIDI-pad and on-screen-pad testing

> **Just want to try it?** The scheduled Nightly workflow publishes the latest
> successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> The bundles are ad-hoc signed and not notarized; check the repository's Nightly
> badge for the latest workflow result.

## Contents

- [Voices, MIDI notes, and controls](#voices-midi-notes-and-controls)
- [Kit mixer and bus](#kit-mixer-and-bus)
- [Sound engine](#sound-engine)
- [Vintage interface](#vintage-interface)
- [Research influences and modeling scope](#research-influences-and-modeling-scope)
- [Requirements](#requirements)
- [Build on macOS](#build-on-macos)
- [Run the DSP tests without JUCE](#run-the-dsp-tests-without-juce)
- [Install locally](#install-locally)
- [Validate the plug-in](#validate-the-plug-in)
- [Sign, package, and notarize](#sign-package-and-notarize)
- [Project layout](#project-layout)
- [Licensing](#licensing)

## Voices, MIDI notes, and controls

The primary note map follows General MIDI percussion assignments. MIDI velocity
controls both hit strength and timbre.

Each voice has seven automatable controls, and the kit adds five more, for
**96 host parameters** in total: 91 voice parameters plus the kit controls.

| Per-voice control | Range | Default |
| --- | --- | --- |
| Character A | 0-100% | per voice |
| Character B | 0-100% | per voice |
| Pitch | -24 to +24 st | 0.0 |
| Decay | 0-100% | per voice |
| Level | -24 to +6 dB | 0.0 dB |
| Pan | L100 to R100 | original kit position |
| Choke Group | Off, A, B, C | A for both hi-hats, Off elsewhere |

| Kit control | Range | Default |
| --- | --- | --- |
| Kit Humanise | 0-100% | 50% |
| Kit Bleed | 0-100% | 0% |
| Bus Drive | 0-100% | 0% |
| Bus Compression | 0-100% | 0% |
| Output | -24 to +6 dB | -6.0 dB |

| Voice | GM note | General MIDI assignment | Character A | Character B | Default pan |
| --- | ---: | --- | --- | --- | --- |
| Kick | 36 | Bass Drum 1 | Punch | Drive | C |
| Snare | 38 | Acoustic Snare | Wires | Snap | C |
| Clap | 39 | Hand Clap | Spread | Tone | C |
| Closed Hat | 42 | Closed Hi-Hat | Metal | Tone | R16 |
| Open Hat | 46 | Open Hi-Hat | Metal | Tone | R20 |
| Ride | 51 | Ride Cymbal 1 | Machine | Tone | R27 |
| Crash | 49 | Crash Cymbal 1 | Machine | Brightness | L27 |
| Low Tom | 45 | Low Tom | Punch | Skin | L20 |
| Mid Tom | 47 | Low-Mid Tom | Punch | Skin | C |
| High Tom | 50 | High Tom | Punch | Skin | R20 |
| Shaker | 82 | Shaker | Density | Color | R12 |
| Perc 1 | 56 | Cowbell | Ratio | Drive | L12 |
| Perc 2 | 75 | Claves | Hollow | Click | R12 |

Common kit-layout aliases are accepted too: 35 for Kick; 59 for Ride; 57 for
Crash; 41 and 43 for Low Tom; 48 for Mid Tom; 70 for Shaker; and 76 or 77 for
Perc 2. Notes 44, 52, 53 and 55 are articulations rather than aliases and are
described below. Other notes are silent.

**Snare articulations.** The snare answers three notes rather than one, on the
notes electronic kits and mainstream drum instruments send them on:

| Note | Articulation | What it is |
| ---: | --- | --- |
| 38 | Head | The stick on the batter head, a third of the way out |
| 40 | Rimshot | Head and hoop struck together, right against the rim |
| 37 | Cross-stick | The stick laid across a hand-damped head, shaft on the hoop |

These are not three samples or three levels of one: they are the same modelled
drum struck in three places, and the difference follows from the model. The
rimshot lands at 93 % of the head's radius, where `J_m(lambda r/a)` is far from
the centre and the whole circumferential series is fed at once, with the
shortest contact anything in this kit makes, and it drives the head hard enough
to throw the wires well clear. The cross-stick puts a hand on the membrane -
a heavy, frequency-independent absorber, so it goes into the fixed loss term
rather than the material ones - which takes the head down in a seventh of the
time, keeps the wires on the head, and leaves the hoop as the only thing
radiating. Measured against a head strike after matching peaks, a rimshot leaves
a 36 % residual and a cross-stick a 92 % one.

This changes two aliases that earlier versions accepted: note 40 was a second
name for a plain snare, and note 37 was a second name for Perc 2. Perc 2 keeps
its primary note 75 and its 76 and 77 aliases.

**Hat and cymbal articulations.** The same treatment reaches the metallic half
of the kit, on the four General MIDI notes an electronic kit sends for them:

| Note | Voice | Articulation | What it is |
| ---: | --- | --- | --- |
| 44 | Closed Hat | Foot chick | The pedal shutting the pair, played with no stick at all |
| 53 | Ride | Bell | The cup: faster, longer, and weighted above the bow |
| 52 | Crash | China | Slower, shorter, and weighted below the crash |
| 55 | Crash | Splash | Faster than the crash, and gone in a seventh of the time |

The foot chick is a stroke and not a wiring change. The plates are already
clamped when it happens whatever the pedal is doing, so the aperture is forced
to zero; the stick is removed in three places — the broadband burst that a
wooden tip scuffing bronze puts in front of the plate goes to zero, the contact
is six times a stick's because two plate faces meet over the whole of their
overlap rather than a point touching one, and the pair decays in 0.42 of the
time the same clamped pair takes under a stick. Under this engine's contact law
a blunt contact is a *long* one, and it is that length which makes the stroke
dull: the same reach term that tilts a struck hat's plate bank puts one pole on
the circuit source at 3.2 kHz for a chick, where a stick's own contact would put
it at 19 kHz and very nearly bypass it. Measured against note 42 at v = 0.90,
note 44 leaves a 6.59 dB level-matched third-octave residual over the first
60 ms and carries 0.054 times its 8–16 kHz energy over the first 30 ms.

The three cymbal notes are honestly voicings rather than strike positions.
Neither cymbal has a modal bank — see the two-machine section below for why —
so there is no geometry to move the stick around on; what the machine does have
is a sample clock, a recorded length, a contact time, three analogue band gains
and the digital leg's one crossover, and those are what these notes set. The
bell runs its counter 1.46 times as fast over 1.85 times the length with 0.68 of
the contact and its crossover at 5.2 kHz; the china 0.74, 0.42 and 1.35 with a
1.5 kHz crossover; the splash 1.40, 0.14 and 0.74 with a 6.4 kHz one. Two bounds
keep those honest: the variant decay is clamped to the instrument's own Decay
maximum, so a bell cannot be ended by the engine's eight-second tail bound
instead of by its own envelope, and no variant clock may pass 44.1 kHz, because
the counter is capped at one address per sample and a faster splash would ring
at a different pitch at 44.1 kHz than at 96 kHz. With that cap the four new
notes hold their band shares across sample rates better than the two notes they
are variants of. Measured level-matched, note 53 leaves an 8.03 dB residual
against note 51 and decays to −20 dB in 0.705 s against the bow's 0.455 s;
notes 52 and 55 leave 5.16 and 6.28 dB against note 49 while staying within
1.30 and 1.51 dB of its first 60 ms, so neither is a quiet cymbal pretending to
be a different one.

Every residual quoted for these four notes is the difference between two renders
after their levels are matched, taken over third-octave bands and restricted to
the bands within 40 dB of the loudest, measured with the analysis filters the
regression suite itself uses. Those bands have skirts far wider than the band,
so a loud neighbour leaks into every reading and compresses the differences: the
same four renders through a windowed transform read 15.9, 11.9, 8.1 and 9.4 dB.
Each articulation clears its contract on either estimator and by more on the
sharper one, but no decibel of this kind means anything without the estimator
that produced it.

**Beyond note-on.** The rest of the MIDI surface a kit sends is answered too,
at each event's own sample offset, and each entry is described below:

| Message | What it does |
| --- | --- |
| CC 4 | Hi-hat pedal position: 0 fully open, 127 tightly closed |
| CC 88 | High-resolution velocity: the low seven bits of the next note-on's velocity |
| Channel aftertouch | Chokes every cymbal that is still ringing, progressively with pressure |
| Polyphonic aftertouch | Chokes only the cymbal the note it names is playing |
| CC 120, CC 123 | All sound off and all notes off, which also drops any held CC 88 prefix |

**Cymbal choke (aftertouch).** Pressure on a pad chokes ringing bronze, in both
the conventions a kit sends it: channel aftertouch takes every cymbal that is
still sounding, and polyphonic aftertouch takes only the cymbal its note plays.
It is a grab and not a switch, so it damps over a time constant of
`6 ms + 460 ms · (1 − pressure)²` — a choke is a contact damper and both the
area in contact and the force behind it rise as the hand closes, so what it
removes per cycle goes roughly as the square of the pressure. Full pressure on a
ringing crash takes the window 30 ms later to digital zero; half pressure leaves
it 14.77 dB under the unchoked ring and far above the full grab. Pressure 0 does
nothing at all, because that is what a controller sends when the hand comes off
and nothing here can put a cymbal back.

**High-resolution velocity (MIDI CC 88).** A CC 88 immediately ahead of a
note-on carries the low seven bits of a fourteen-bit velocity, which is how
every MIDI 1.0 controller that resolves finer than 127 steps sends it. Drumalor
takes the prefix for exactly one note event and no other — a note-off consumes
it too, so it can never be handed to a later, unrelated stroke. The two paths
are deliberately different arithmetic: with no prefix the velocity byte is still
divided by 127 exactly as it always was, so an ordinary note-on is bit-identical
to what the engine produced before this existed. Folding it into the fourteen-bit
scaling instead would read a full-velocity note as 16256/16383, which is
0.0676 dB low and would move every existing session.

**Hi-hat pedal (MIDI CC 4).** The hats are a pedal, not two notes. Every
electronic kit sends pedal position as continuous controller 4, 0 fully open to
127 tightly closed, and Drumalor treats it as one modelled quantity: how far
apart the two plates are. That single number sets the pair's stiffness, its
decay, and which damping law it obeys — an open plate loses its top first, a
clamped pair is damped by friction between two faces and takes every partial at
much the same rate — so a half-open hat is genuinely between the two rather than
a crossfade of them.

The two Closed Hat and Open Hat channel strips remain separate instruments in
the mixer, with their own character, pitch, level, pan and Decay. Once the pedal
is live, the note chooses the channel and the pedal decides how open the pair
is; their two Decay settings become the endpoints, interpolated geometrically.
Closing the pedal on a ringing hat damps it progressively, over a few
milliseconds at the bottom of the travel and a third of a second near the top,
and a foot coming down fast enough to shut the pair produces its own chick with
no note involved.

Lifting the pedal is the foot splash, and it is a release rather than a
restoration. Opening the plates cannot put back the energy friction already
took; what it can do is stop the friction and hand what is left to the open
plate's own loss law. A pedal move in either direction therefore re-derives a
*ringing* hat's decay at the new aperture — the envelope, auxiliary and
transient constants, and each plate mode's pole radius — with the friction added
on top when the foot goes down and taken away when it comes up. The modes keep
the frequencies they are ringing at: only the radius of each pole moves, because
re-tuning them would be the pair changing pitch under the note instead of
changing how fast it dies. The release is guarded, so a choke group or a panic
that arrived in between is not undone. Measured in the 150–400 ms window, a
pedal taken to 76 at 20 ms and back to open at 40 ms leaves 94.9 dB more than
the same close held and 15.4 dB less than no pedal move at all — a hat let go
of, not a hat played again. Resting on the pedal is still silent.

Until a controller touches it, the pedal does nothing at all: the Closed Hat
note is exactly a fully closed pair and the Open Hat note exactly a free one, so
a session that never sends CC 4 renders sample for sample as it always did. That
identity is a regression contract, not an intention. Resetting the plug-in
releases the pedal.

**Choke groups** generalise the hi-hat pedal. Any voice can be placed in group
A, B, or C; triggering it then cuts every sounding voice in the same group with
a short 3 ms fade. Closed Hat and Open Hat share group A by default, so an
untouched kit behaves exactly as before, while a Ride and Crash, or two
percussion voices, can now be linked the same way. Each hit remembers the group
it was born into, so changing the control never strands a ringing tail. Voices
outside a group overlap and retrigger independently.

The labels describe musical intent rather than exposing implementation-specific
constants; hosts store the stable parameter IDs behind them for automation and
recall.

The version 1.0 parameter block is unchanged and still occupies host parameter
indices 0-52, so existing sessions and automation lanes keep working. Every
control added in 1.1 is appended after it and defaults to preserving the earlier
sound: unity channel level, the original kit pan positions, the original hi-hat
choke pair and a bypassed bus.

Two things in 1.1 do change how an existing session sounds, and neither is
recoverable by leaving a control alone. The Ride's and Crash's first character
control is now Machine rather than Bell and Spread, so those two voices will
play differently from a saved position that meant something else; the parameter
block is unchanged, so the session still loads and every other voice is
untouched. And the per-hit analogue variation is wider than the depth 1.0 used
to mean - it was sized to be measured rather than heard - so a kit at the
default Humanise no longer reproduces the older rendering sample for sample.
Setting Humanise to 0% removes the variation entirely if a session needs the
tighter behaviour back.

## Kit mixer and bus

**Level** and **Pan** turn the previously hard-coded kit balance into
automatable per-voice controls. Level is a clean gain applied to the voice, and
Pan is the same constant-power law the fixed positions always used, so a kit
left alone images and balances identically.

### Kit tuning

The factory kit is tuned rather than merely assembled. Every voice that has a
definite pitch sits on a harmonic of one root - a G an octave below the kick,
which is itself that root's second harmonic:

| Voice | Harmonic | Pitch | Note |
| --- | ---: | ---: | --- |
| Kick | 2 | 49.0 Hz | G1 |
| Low Tom | 3 | 73.5 Hz | D2 |
| Mid Tom | 5 | 122.5 Hz | B2 |
| High Tom | 6 | 147.0 Hz | D3 |
| Snare | 8 | 196.0 Hz | G3 |
| Perc 1 | 32 | 784.0 Hz | G5 |
| Perc 2 | 40 | 980.0 Hz | B5 |

Being a harmonic series rather than an equal-tempered chord, the two voices on
the fifth harmonic and its octave - Mid Tom and Perc 2 - sit about 14 cents
below where a piano would put a B. That is what a just major third is, and it
is why the kit sounds settled rather than beating against itself.

The Clap, both hats, both cymbals and the Shaker are deliberately not in that
table. They have no definite pitch to tune: a cymbal's inharmonicity is the
point of it, and the six oscillators behind the hats and cymbals are chosen so
that no two of them agree. Giving them a note would mean taking that away.

The tuning lives in each voice's default Pitch offset, so it is visible on the
panel and reversible: setting a voice's Pitch to zero returns it to its own
untuned nominal frequency rather than to the kit's root. A contract measures
every pitched voice and requires it within a quarter of a semitone of a
harmonic, so a later change to any nominal frequency cannot quietly pull the
kit apart.

**Kit Humanise** scales how much of the modelled per-hit component tolerance
actually reaches each voice: pitch, decay, transient energy, tone, circuit drive
and bias. At 0% the kit is machine-tight; at 100% the drift is twice as wide.
The underlying drift sequence is untouched by the control, so a kit remains
exactly reproducible after reset at any setting.

Two things drift, on different scales. Each voice has its own slow component
tolerance, which is why two strikes of the same drum are never the same strike.
Under all of them is the board: one supply and one ambient temperature, wandering
over a few seconds and read by every voice at the moment it is triggered, so
drums struck near each other lean the same way. That second term is what makes a
pattern sound like one instrument having a moment rather than thirteen
independent ones jittering. It is derived from elapsed time rather than from a
trigger count — a machine left switched on drifts whether or not anything is
playing — which also keeps one voice's sound from depending on another voice's
history. It reaches frequencies, envelope times and supply-dependent drive, but
deliberately not the panel controls: what a temperature moves is a time constant,
not where a knob is set.

The deviations are sized to be heard rather than merely measured — around a sixth
of a semitone of pitch at the default, which is roughly where a repeated hit
stops reading as one recording retriggered.

One thing Humanise moves is not a component tolerance at all: where the stick
lands. A strike's azimuth around the head decides how the two members of each
split mode pair are excited, and therefore how deep the warble in the tail is,
so the control carries a per-hit azimuth deviation of ±4 degrees at the
calibrated unit and ±8 degrees at the top of its travel, around a fixed nominal
of 25 degrees. It is the only variation field with no component-drift or
board-drift term behind it, because a supply rail does not decide where a
drummer's stick lands. Measured over eight hits, the beat depth spreads by 3.3
to 5.7 dB at Humanise 100% and by 0.002 to 0.011 dB at Humanise 0%, so a
machine-tight kit stays machine-tight: the nominal 25 degrees is what keeps both
members of every pair excited there, and it is chosen to keep `m·phi` off every
multiple of 90 degrees for the six circumferential orders the table carries, so
no pair is silently reduced to one member.

**Kit Bleed** is the kit hearing itself. A drum that is not being struck is
still a drum: the snare's resonant head carries a set of wires lying on it and
answers everything the rest of the kit puts into the air and the floor, and a
tom's head answers whatever lands near its own note. Toontrack calls that
leakage "one of the key elements that gives a studio drum recording its sense of
cohesion and realism"; IK sells it as sympathetic vibration between the kick,
snare and toms. Drumalor models it rather than mixing it: four undriven heads —
the snare's and the three toms' — sit permanently in the engine, tuned from
their own channels' Pitch and Decay, driven by a band-limited copy of the kit's
own mix, and placed at their own Pan positions.

The snare's is the one that matters. Its wires are gated on the head's
displacement by a lift-off law rather than a level: below the point where the
head lifts them off their resting contact there is no buzz at all, and above it
there is. That is why a bled kick makes a snare buzz suddenly as the kick gets
loud instead of buzzing a little at every level, and it is the reason a kick and
a floor tom sound like they are in a room with a snare rather than next to a
recording of one.

The path is strictly feed-forward: the beds hear the previous sample's dry mix
and nothing they added to it, so there is no loop to be stable about and no
dependence on the host's block boundaries. At its 0 % default it is not merely
scaled to nothing but skipped entirely, so a kit with Bleed off is bit-identical
to the engine before it existed and pays nothing for it.

**Bus Drive** and **Bus Compression** form a shared output stage after the mix
and DC blocker. Drive is a gain-matched asymmetric softener with the same
first-order ADAA used inside the voices, so it adds density and level dependence
rather than loudness. Compression is a stereo-linked peak-detecting glue
compressor with a 4 ms attack, a 140 ms release, an amount-dependent threshold
and ratio, and matched makeup; its gain law blends continuously between unity
and hard limiting so no per-sample transcendental is needed. Both stages are
fully bypassed at their 0% defaults - not almost bypassed, but skipped entirely,
which the regression suite verifies sample for sample.

Both bus controls are ramped at the master gain's 20 ms constant rather than
stepped once per block. Taking them straight from the block's parameter value
put a hard discontinuity into the mix whenever either was automated: on a
sustained 24 Hz kick, switching Bus Compression on between two blocks produced a
sample-to-sample jump 143 times the largest step anywhere else in that waveform,
and Bus Drive one 37 times as large. The ramp lands exactly on zero, so bypass
is still reachable and still skipped entirely.

Ramping Drive means its saturator's curvature is now swept continuously down to
zero, and that exposed a numerical fault in the antiderivative the stage's
antialiasing evaluates. Written the textbook way, as `x/c - log1p(cx)/c^2`, it
subtracts two quantities of size `x/c` to leave a result of size `x^2/2`, so
below a curvature of roughly 1e-3 the answer is pure rounding noise - and the
divided difference that follows divides it by a sample-to-sample step of about
1e-3 and amplifies it to full scale. Bus Drive reached that region on the very
first increment of its 0.1 % parameter grid, so this was never only a ramp
problem: on the engine as it stood before this fix, a kit held at a steady Drive
of 0.1 % already peaked at 0.92 with 0.80 sample-to-sample jumps, against 0.58
and 0.10 at bypass.
The stage now evaluates the algebraically identical `x^2 * h(c|x|)` with
`h(u) = (u - log1p(u))/u^2`, which has no such cancellation. Automating Drive
either way now leaves the mix within its own settled peak and its own largest
sample-to-sample motion at 44.1, 48, 96 and 192 kHz, where before the ramp down
to bypass produced about 80 ms of full-scale clipped noise - peak 1.0, jump 2.0
- a fifth of a second after the move.

## Sound engine

The JUCE-free C++20 DSP core builds each voice from the mechanism that makes the
real instrument: two heads and the air between them for the struck drums, thin
bronze plates for the hats and cymbals, a free bar over a cupped hand for the
claves, a travelling mass of grains for the shaker. Voice-specific controls move
several related synthesis values together so each voice remains useful across
the full range.

### Two heads, and the air between them

Kick, Snare and the three Toms are not one resonator each. A real drum of any of
those kinds is two heads enclosing a volume of air, and that air is a spring
across both of them, so every mode that changes the enclosed volume is split
into a pair:

- The heads moving **together** squeeze the air, so that branch is stiffened up
  to wherever the spring puts it. Both faces push outward at once, which makes
  it a monopole — the most efficient radiator there is — so it is the loudest
  thing the drum does and it is over in a tenth of a second. This is the punch.
- The heads moving **oppositely** leave the volume alone, so that branch sits
  where an unloaded head would. It moves air from one side to the other and
  radiates as a dipole, which at these sizes is barely at all: quiet for the
  energy it holds, and long. This is the weight.

A drum with one resonator has neither of those. It has an average of them, which
is a tone. The split follows from the geometry — `rho c^2` over the head's mass
per unit area and the depth it encloses — so a shallow snare, whose air column
is a third of a tom's, is pushed to well over twice its batter head's note, and
that branch is the crack that lets a snare cut through a band.

The rest of the head is an eighteen-slot bank at the zeros of `J_m`, each mode
carrying its circumferential order, because that order decides both whether the
trapped air couples to it and how badly it radiates. Air loading is added mass,
heaviest on the mode that moves the most air, so it pushes the series **apart**
rather than scaling it; **Skin** sets how much of it the head has to carry,
running each tom from a tighter, more pitched drum to a looser and clearly
inharmonic one.

### Every mode above the first is two modes

An ideal circular head has a rotational symmetry, so every mode with a
circumferential order `m > 0` is doubly degenerate: two shapes at the same
frequency, `cos(m·theta)` and `sin(m·theta)`, one rotated half a lobe from the
other. Any departure from that symmetry lifts them apart, and Worland's
measurements say ordinary non-uniform lug tension is enough. Two close
frequencies sounding together beat at their difference, and that beat is the
warble a real head's decay has — the "wow-wow" a drummer tunes out with a
tuning key — which a single pole pair cannot produce at all.

Drumalor emits both members. The mode table fills the bank's slots, and the
loudest `m > 0` modes are then split into their two members in descending gain
order for as many slots as the table leaves over: four pairs on every membrane.
How far apart is a property of the drum rather than of anybody's playing — a
key was turned once and the head has been that shape ever since — so the split
is hashed from the instrument index and reproduces after a reset: 2.438 % on the
Kick, 1.941 % on the Low Tom, 2.128 % on the Mid Tom, 1.821 % on the High Tom
and 2.412 % on the Snare, which put the four measurable m = 1 beats at 2.1, 2.5,
4.6 and 4.7 Hz. The four m = 1 modes those beats are measured on — the Kick's
and the three Toms' — sit at 86.19, 131.33, 214.51 and 255.60 Hz, which is where
air loading leaves them rather than where the ideal Bessel ratio puts them, and
each pair is emitted around its own centre. Each member
gets its own decay from its own frequency, and the pair is driven at `cos(m·phi)` and `sin(m·phi)` for a strike at azimuth `phi`.
Those two square to one, so the pair carries exactly the energy the single mode
carried: what changes is that the energy now arrives at two frequencies instead
of one.

The beat is once or twice in a decay rather than a sustained tremolo, and that
is the mechanism rather than a limitation of it: on every drum in the kit the
m = 1 mode is 60 dB down after 1.8 to 2.2 beat cycles. Measured on the fitted
periodic component of the tail envelope it is 5.9 to 6.6 dB deep, against 1.6 to
7.1 dB on a bank with the pairs collapsed — which is why the contract asserts
the beat's *rate* alongside its depth, since an unsplit bank's decay curvature
can be deep without being periodic.

Splitting needs slots, so the bank's budget went from twelve to eighteen. That
also lets in the last two entries of the mode table, which the old budget
truncated. Neither is free in sound: admitting the two truncated entries alone
moves every membrane by a best-gain null of −46.3 dB (Kick), −22.5 (Snare),
−26.4 (Low Tom), −28.0 (Mid Tom) and −27.9 (High Tom), the splitting alone by
−34.0, −21.8, −27.1, −27.0 and −27.9, and the two together — which is what
ships — by −33.4, −19.5, −22.5, −23.9 and −24.6 dB, with each voice's level
unchanged to within 0.17 dB: that figure is the Snare, and the other four hold
to 0.03 dB. It is a small, real re-voicing of the membrane half of the kit
rather than an addition on top of it.

### Damping follows radiation

A mode's loss is edge and mounting damping, plus hysteresis in the film rising
as `omega`, plus its viscous term rising as `omega^2`, plus the sound that
actually leaves — and that last term goes as `(ka)^(2m+2)`, so a monopole loses
energy hundreds of times faster than a quadrupole. On a drum, the loudest thing
and the longest thing are opposite questions, and the answer to both is the
multipole order. A bank whose modes all decay together is a bell.

### A struck head is a stretched head

A membrane's restoring force is its tension, so a head that has been pushed out
of its plane is a head under more tension than it had at rest, and every mode in
the bank is sharp while the strike energy is still in it. As the drum rings out
the tension relaxes and the pitch settles. That is the drop a real tom has, and
it is not the same thing as a pitch envelope: an envelope always starts from the
same note and always takes the same time, while a tension bend follows how hard
the drum was actually hit — a ghost stroke barely moves at all and an accent
bends audibly.

Avanzini and Marogna's result is that the short-time average of that tension
rise is approximately proportional to the system's *energy*, which is a quantity
a modal engine can afford: the whole model is a leaky mean square of the bank's
own output, scaled by the voice's velocity because the bank's internal amplitude
is deliberately normalised. It moves each mode by rewriting `a1` around its
resting value — `2r·cos(ω(1+δ))` is `nominalA1 − 2rω·sin(ω)·δ` to first order,
which is exact to well under a cent across the six per cent of bend the model
allows — and never touches `a2`. Since `a2` is `−r²`, the pole radius, and
therefore the mode's decay time, cannot drift with its amplitude. The
coefficients are rewritten every sixteenth sample, counted in the voice's own
age so the update lands on the same samples at every host block size.

The depth follows the drum. A bass drum head is wide and slack but a beater only
ever displaces it by a small fraction of its radius; a snare batter tensioned
hard enough to answer a stick has almost no room left to stretch; a tom has the
most, and **Skin** is the reason — a head carrying more air is a slacker head,
and a slacker head stretches further for the same blow. Measured at 48 kHz on
the current head bank, a full-velocity strike's dominant partial in the first
35 ms sits 32, 9, 128 and 175 cents above the same partial at a ghost stroke for
the Kick and the three Toms; with the tension model disabled on the same bank
the four numbers are −63, −83, +9 and +488 cents. The High Tom is the exception
and is honest about it: its analysis band now holds nine modes including two
split pairs, and which of them wins the window is decided by contact time rather
than by tension, so the model is what the first three drums and the
settled-pitch measurement test, and the High Tom's figure only says that a hard
strike reads well over a semitone sharper than a ghost stroke there.

### How far the pitch falls follows the blow

Beside the tension bend, the Kick and the three Toms carry a drawn pitch sweep,
which is what gives an analogue-flavoured drum its fall into the note. Its
*shape* is an envelope and its depth used to be a panel knob and nothing else:
on the Toms there was no strike term at all, and on the Kick the only one
spanned 0.84 to 1.00 of a sweep that is a factor of 4.42 at the strike. Traced
from the engine's own oscillator rather than through an analysis band, a ghost
stroke therefore glided 91.7 % as far as an accent on the Kick and 99.0 to
99.2 % as far on the three Toms — which is a machine, not a drum.

The sweep now keeps its shape and takes its *depth* from the strike. What the
strike leaves in the head is the accent voltage times the excitation scale — the
same product the modal bank is struck with — and the energy stored in a
stretched head goes as the square of it, so the depth is that square normalised
against its own value at v = 0.85 and clipped there. Saturating at v = 0.85
means every accent is exactly where it was: the three Toms are bit-identical to
the previous engine at v = 1.00, 0.95 and 0.85, and the Kick at v = 1.00. Below
it the glide collapses the way a soft stroke's does. Traced at 48 kHz, the
v = 1.00 glide is unchanged to every digit at 25.607 semitones (Kick), 10.660,
10.353 and 9.795 (the three Toms), while the v = 0.08 glide falls from
23.473/10.571/10.264/9.692 to 0.291, 0.444, 0.443 and 0.505 semitones — 1.1 to
5.1 % of the accent's. Both strengths still settle on the same note, within
0.011 cents. On the render, the Kick's window from 4 to 25 ms moves 6.55 dB of
band balance away from the sweep's upper region at a ghost stroke — 3.318 to
−3.228 dB of the energy above the settled fundamental over the energy around it,
read with the same second-order analysis pair the suite's other band assertions
use — and does not move at all at an accent, where it reads 4.131 dB either way.
The window opens at 4 ms rather than at the strike because the beater's contact
click is broadband and belongs to neither band; including it costs about a
decibel of the separation. The same statistic moves 1.8 dB on each of the three
Toms, where the head bank and the skin noise dominate the first 25 ms, which is
why the contract carries it on the Kick alone.

This is a redistribution and not a reduction: the sweep keeps its full depth
where it always had it and gives up only what a soft stroke never had the energy
to reach. Because it collapses inside one period of the settled note — the
Kick's pitch envelope is at 1/e in 7.2 ms against a settled period of 20.4 ms —
no zero-crossing estimator of the rendered audio can see it at all, so the
engine publishes the newest voice's oscillator frequency alongside its meters
and the contract measures that rather than the render.

### A drum struck twice is still one drum

Striking a head that is still moving does not produce a second drum. The stick
lands on the drum that is already there, and the contact does two things at
once: it adds the new strike, and it takes energy out of what it landed on,
because a stick or a hand against a vibrating membrane is an absorber. That is
why a press roll dies away instead of growing, and why a flam is one event with
two attacks rather than two drums a few milliseconds apart.

Re-striking one of the five membrane voices therefore scales the ringing bank's
modal state and its envelopes rather than leaving them untouched, and the new
strike is superposed into what is left — `Resonator::strike()` adds to whatever
state it finds, so this is one head with two impulses in it. How much survives
follows the new stroke: a ghost note laid on a ringing tom keeps about three
quarters of it, a full stroke about a fifth. Measured over the 220 ms after the
second stroke of a 15 ms flam, the result carries less energy than the two
strokes rendered separately do — which is what two independent drums would give,
and what the engine used to produce.

The cymbals, hats and percussion voices are deliberately left alone. A cymbal is
most of a square metre of plate against a stick tip the size of a fingernail, and
a second strike on one really does add; the regression suite requires that it
still does.

### There is a beater, and there is a stick

Contact time follows Hertz: it shortens with impact speed, and with how hard the
tip is. The transform of that contact is what decides how far up the head a
strike can reach, so a hard hit is not a loud copy of a soft one — **Punch** is
the beater on the kick and the tip on the toms, and moving it moves the contact,
which moves everything else. Above the frequency where a head's modes stop being
separable there is no series to model, only a band of noise decaying at that
region's own rate, raised by the contact while the two surfaces are actually
touching.

The Snare is played in three places rather than one, and the difference between
them is entirely where the stick landed and how long it stayed. See the
articulation table above; there is no separate rimshot voice, no separate
cross-stick sample and no extra parameter, because a strike position and a
contact time are already what the head bank is built from.

The Snare adds a nonlinear wire model. Real snare wires only rattle while the
resonant head lifts them off their resting contact and damp it below that
threshold, so the wire noise here is gated by the instantaneous head
displacement instead of following a plain exponential envelope. Hard strikes
buzz; soft ones stay dry and damped, and the wire-to-body balance therefore
changes with velocity rather than only with level.

MIDI velocity is a timbre control across the struck voices, not only a VCA
level. A soft strike puts less energy into the high, heavily damped modes of a
real drum, so velocity scales the struck-timbre filters, the modal brightness
and the stick/contact content of Kick, Snare, Clap, both hi-hats, all three
Toms, Shaker and Perc 2. The curve is unity at full velocity, so the loud end of
the established voice design is preserved and quiet hits gain the extra realism.
Ride and Crash have no modal bank to tilt, so velocity reaches them through the
Hertzian contact described with the cymbals below, which moves 3.1 dB (Ride) and
3.8 dB (Crash) of the 8–16 kHz over 2–6 kHz balance across the playable range on
top of what the accent voltage already does to the envelope. Perc 1 is the one
voice where velocity is close to a level control and nothing else: over the same
range it moves 0.44 dB of that balance against 37.30 dB of peak level. That is a
known limit rather than a claim — the voice puts 92 % of its first 60 ms into
200–800 Hz through a single band-passed square pair, and both of its auxiliary
paths measure as garnish, so the only place a stick speed could be heard in it
is a re-voicing that has to be judged by ear.

### Plates

The hi-hats are two thin bronze discs. Each hit strikes a twelve-mode plate bank
and leaves it, over the free-running circuit that carries the hiss. Closing the
pedal clamps the pair together, which both stiffens them and damps them by
friction between two faces rather than by anything inside the metal — so a
closed hat is not a short open hat: friction takes every partial at much the
same rate, where an open plate loses its top first.

That last sentence is about the plate bank and should not be read as a claim
about the whole voice, because the bank is 18.5 % of an Open Hat and 25.3 % of a
Closed one. The rest is circuit, whose own spectrum is flat for the length of
the note, and the modelled metal is the part that leaves first: measured over
40 ms windows as each band's share of its own window, an Open Hat holds
6–10 kHz within 0.01 dB, 10–16 kHz within 1.9 dB and 16–20 kHz within 2.3 dB
from the strike to a second later, while 0.4–2 kHz — the plate's own region —
falls 25.9 dB. A hat's body therefore goes before its hiss does, which is what a
hi-hat does. The voice's spectral centroid consequently *rises* by about three
semitones over its ring, and that figure is real, but it is almost entirely that
one band leaving rather than the top of the voice outlasting anything: the three
bands above 6 kHz hold their share to within 2.3 dB across a ring that spans
87 dB of level.

The same split bounds what velocity can do to a hat. A harder stick has a
shorter contact, which reaches further up the plate, so velocity tilts the plate
bank and moves the band-pass on the strike layer, and a soft hat is duller as
well as quieter. But the free-running circuit under it answers the accent's
level and not its speed, and that circuit is three quarters of a closed hat and
four fifths of an open one, so the whole-voice figure is about 2 dB of the
8–16 kHz over 2–6 kHz balance on the Closed Hat and about 2.6 dB on the Open
Hat across the playable range, against
the 3.1 and 3.8 dB the cymbals move. Widening it means setting a corner on the
circuit source from the strike, which re-voices both hats in every stroke they
make, so it is a change to be judged by ear rather than measured into a
contract, and it is not in the engine.

Ride and Crash are the two voices where the instrument and the circuit want the
same thing. A real cymbal's high partials radiate best and its own nonlinear
coupling drains them downward, so the shimmer goes first and the low-mid roar is
what is still there seconds later — and both of the machines modelled in the
next section darken for reasons of their own that point the same way. Neither of
them carries a modal plate at all; the whole cymbal path is the two circuits,
and the next section is where it is described.

Each virtual channel has fixed per-unit component tolerances. Its metallic
Schmitt/RC oscillators keep running behind the VCA, so a strike samples the
circuit's current phase instead of restarting a waveform with newly randomized
parts. Triggers add only tightly bounded, slowly correlated variations to pitch,
envelope decay, transient energy, tone, circuit drive, and bias, scaled by **Kit
Humanise**. MIDI velocity also changes trigger energy before the resonators and
VCAs, not merely the final gain. Repeated equal-velocity notes therefore differ
without becoming random changes of kit, level, or timing. The sequence remains
deterministic after reset and independent of host block size.

A relaxation-oscillator bank contributes exactly zero through a closed VCA, so
the engine no longer advances one that no voice can observe. Observability is
evaluated per sample from a reference count that voice allocation and retirement
keep exact, and a frozen bank is restored analytically the moment the next
strike opens a VCA: short gaps are replayed substep-exactly, longer gaps advance
every phase, snap the capacitors onto their settled periodic orbit, and
re-render one full reconstruction history. Because the gap is an absolute sample
count, the result is independent of host block partitioning, and a bank frozen
behind an unrelated drum wakes into the same state as one frozen during silence.
The same reasoning retires each voice's modal bank once it has rung down past
-150 dB, far below the -100 dB at which the voice already counts as silent.
Together these remove most of the engine's fixed cost: a kit without hats or
cymbals stops paying for five metallic circuits, and long tails stop paying for
a modal bank they can no longer excite.

Measured on one Linux x86-64 machine, rendering 45 seconds of 16th-note patterns
at 48 kHz in 128-sample blocks, comparing the 1.0 and 1.1 engines back to back
(lower is better; this is CPU time as a fraction of real time):

| Pattern | 1.0 | 1.1 | Change |
| --- | ---: | ---: | ---: |
| Kick, Snare, three Toms, Clap, Perc 2 | 16.0% | 10.7% | -33% |
| Kick, Snare, Closed and Open Hat | 13.9% | 8.4% | -39% |
| All thirteen voices | 28.8% | 22.2% | -23% |
| Kick only | 22.9% | 15.1% | -34% |
| Ride only | 60.3% | 36.8% | -39% |
| Aggregate of the five | 1.42 | 0.93 | -34% |

The 1.1 figures already include the added membrane models, so the saving from
the two gating changes alone is larger than the table shows. The absolute
percentages are specific to that machine and say nothing about a Mac; only the
before/after ratio is meaningful. The JUCE-free regression suite over the same
period went from 27.5 s to 17.5 s despite gaining ten new test groups.

A later pass measured the same dense thirteen-voice kit, ten seconds of audio at
48 kHz in 128-sample blocks, on one Linux x86-64 machine:

| Host FPU mode | Before | After | Change |
| --- | ---: | ---: | ---: |
| Denormals enabled (offline renderers, plain DSP use) | 11.10 s | 3.31 s | -70% |
| Flush-to-zero set by the host | 3.61 s | 3.36 s | -7% |

Most of that is the denormal floor described below; the remainder came from
resolving each voice's output-stage transfer curve once at note-on instead of
per sample, carrying the ADAA antiderivative forward instead of recomputing it
(one `log1p` per voice-sample instead of two), deriving the tonal oscillator's
two asymmetry harmonics from double- and triple-angle identities instead of two
further interpolated table reads, and skipping the RC integrators in the ride and
crash relaxation banks, whose mixes read only the Schmitt pulses. None of these
changes the audible signal.

Repairing the shaper antiderivative described under the kit bus gave part of
that back: it evaluates one double-precision `log1p` per call where the previous
form used a single-precision one, which on the same dense thirteen-voice
benchmark costs about 7% (3.47 s to 3.70 s, median of five runs with
flush-to-zero set). That buys a stage that is correct over its whole curvature
range instead of only above about 1e-3, and it is the same one `log1p` per
voice-sample, not two.

Because the old antiderivative was inaccurate wherever the curvature was small
relative to the signal, correcting it also moves every voice slightly. Nulled at
48 kHz against a reference build that evaluates the same antiderivative in long
double, the current engine is bit-identical for twelve of the thirteen voices and
-220 dB for the Ride; the previous engine sat -68 to -83 dB from that reference.
The audible signal is therefore unchanged - the whole difference is rounding
error being removed - but the earlier claim that seven voices null bit-exactly
against the 1.0 engine no longer holds. Against 1.0 the eleven voices that are
not deliberately changed now null at -68 to -83 dB RMS instead of at -70 to -86
or exactly; the Shaker still draws a different but statistically identical noise
realisation because it is the one voice that takes two noise samples per sample;
and Perc 1 sits at -23 dB, which is the deliberate Drive change and nothing else.
The only deliberately audible change at 48 kHz in that pass was Perc 1's Drive. The JUCE-free regression suite went from 16.3 s to 9.8 s on the
same machine while gaining three test groups.

The cymbal rewrite described below is the next deliberate change, and it is
confined to Ride and Crash: rendering all seven demonstration takes before and
after leaves the four that contain no cymbal byte-identical, and moves only the
three that do.

The head bank's move from twelve slots to eighteen, described under the split
mode pairs above, is the one later change that is not free in time. On the same
kind of harness the suite's dense thirteen-voice stress render goes from 0.86 s
to 0.95 s, a 9 % increase against the suite's own 20 s guardrail, and the whole
JUCE-free suite from 17.8 s to 20.1 s, of which 2.3 s is the new tail contract.

Each voice finishes through a lightweight asymmetric diode/transistor-style
transfer with a variable operating point and a virtual supply rail that sags
quickly on strong transients and recovers more slowly. First-order analytic
antiderivative antialiasing (ADAA) is applied to these nonlinear stages and the
stereo output shaper. Only the discontinuous metallic oscillator/ring-modulation
islands are adaptively oversampled and reconstructed before returning to the
host rate; the rest of the voice path is not multiplied in cost. The undelayed
linear component is preserved so quiet hits keep their transient definition.

Every noise layer in the kit - the kick click, the snare wires and snap, the
clap bursts, the hi-hat air, the stick skin on a tom, the shaker grain and the
Perc 1 click - is generated on a fixed 48 kHz grid and read with interpolation
rather than drawn fresh at the host rate. Those layers are all heard through
filters whose bandwidth is fixed in hertz, so what reaches the listener is the
noise's power *density*; a generator that spreads a fixed variance over the whole
Nyquist band loses 3 dB of it per doubling of the sample rate. Measured before
the change, the Clap lost 5.9 dB of level and the Snare's spectral centroid fell
from 1.9 kHz to 0.9 kHz between 44.1 and 192 kHz - the kit audibly thinned out
and darkened on a high-rate session. The fixed grid holds the audible band
constant instead, and because the grid rate is the reference rate, a 48 kHz
render is unchanged sample for sample.

The engine also flushes its own recursive states to zero at -600 dBFS. Every
envelope, resonator, biquad, DC blocker and detector decays geometrically for as
long as its voice lives, so without an explicit floor they all spend a stretch of
every note in the subnormal range, where x86 traps into microcode. A host that
sets flush-to-zero hides that; an offline renderer or a wrapper that does not
leaves the plug-in paying for it. On a dense thirteen-voice kit the engine used
to cost 3.1 times as much with denormals enabled as with them flushed - slower
than real time on the measuring machine. The sound can no longer depend on the
host's FPU configuration.

The Kick has a dedicated charged-energy model: a virtual capacitor discharges
into a contractive two-state resonator whose frequency and loss change with the
stored energy. Its default body settles around 48 Hz, while **Punch** controls
the initial pitch movement and contact noise and **Drive** moves the nonlinear
operating point, branch mismatch, harmonic density, and modest makeup gain. The
resonator update is an explicit rotation followed by contraction, so even rapid
pitch modulation cannot inject unbounded state energy.

Hats, Ride, Crash, and Perc 1 use persistent relaxation-oscillator banks. Exact
exponential RC charge/discharge curves, fixed threshold and tuning tolerances,
47.98%-centred duty cycles, fractional-edge PolyBLEP correction, and local
adaptive oversampling retain the unstable metallic detail with substantially
less aliasing than naive square waves or multiplied ideal sines. An
approximately 80 dB Kaiser-windowed reconstruction FIR precedes each adaptive
rate change. Perc 1 now derives its cowbell body from the familiar approximately
535/800 Hz Schmitt pair. Its **Drive** spans a wider circuit-drive range than its
neighbours and carries modest drive-dependent makeup, because the output stage's
exact 1/drive compensation otherwise cancelled almost all of it: the control used
to change the voice by 6.9 % over its whole travel, against a 90 % average for
the other character controls, and delivered that as a 0.9 dB level drop rather
than as saturation. Measured end to end over the control's whole travel, it now
takes 1.69 dB off the crest factor for a 0.15 dB level change; the regression
suite requires at least 1.4 dB of crest reduction and less than 0.5 dB of level
drift. Tonal snare and tom cores remain smooth resonators, but
add explicitly band-limited component asymmetry and subtle virtual-rail pitch
coupling instead of mathematically perfect table sines.

### Two machines made cymbals two different ways

Ride and Crash each run two channels, because the two machines that defined this
sound solved the same problem from opposite ends and a kit wants what each of
them got right. Every block below is one the published circuit analyses
identify, in the order they identify it; the sources are listed further down.

**The analogue channel** starts where the 1980 machine starts: six Schmitt-trigger
inverter oscillators in the low hundreds of hertz, summed at a virtual earth.
Four are fixed by their own resistor and capacitor; the last two are set by
trimpots reachable only with the case open, which is why they are also what the
cowbell is made from and why no two units agree about them. Drumalor gives them
the measured nominal frequencies and a fixed per-unit trim — a few tenths of a
percent on the fixed four, a couple of percent on the trimmed pair.

Two active band-passes hang off that summing node, at the measured 3.44 kHz and
7.1 kHz, with the Q a multiple-feedback section gives and the midband gain that
topology has. Their job is not to filter the six squares but to throw their
fundamentals away and keep the intermodulation above them: that is the whole
trick by which six oscillators under 800 Hz become a metallic spectrum three
octaves higher.

The trigger pulse does not open anything directly. It reaches the envelope
capacitors through an attack smoother, so every band ramps rather than
switching, and the three swing-type VCAs that follow are not multipliers: a
steered pair only begins handing signal across once the control has climbed
past its own base-emitter drop, so its law is superlinear at the bottom and
straightens out above it. That is why an 808 cymbal's decay does not sound like
the exponential that drives it, and why velocity is an accent voltage here
rather than an output trim — a quiet hit does not open the VCAs as far and
leaves sooner, which the regression suite measures directly. Each band then
passes its own Sallen-Key high-pass into the tone-control mixer that sums them.

**The digital channel** is the 1983 answer. A counter walks a ROM at a sample
clock — about 30 kHz nominal, divided down from a free-running oscillator, and
tunable, because moving that clock is the only pitch control the machine's
cymbals have. The stored waveform has had its envelope taken out so six bits go
further, so it is quantized at full scale through a companded converter and the
envelope is put back afterwards by the VCA. The service notes record six bits
and heavy compression rather than a code layout, so Drumalor uses the ordinary
one: sign, two chord bits and three step bits, four chords with a linear bottom
one, 64 codes in all. That ordering is the whole
character: the quantization error is multiplied by the same envelope as the
signal, so a cymbal is gritty where it is loud and clean where it is quiet
instead of dissolving into a fixed floor.

That envelope comes from a second converter reading the *address* lines through
an anti-log stage, so it steps with the counter rather than with a clock of its
own and always finishes exactly where the ROM does. Retune the machine and
pitch and tail length move together — a transposed cymbal is a shorter one.
Drumalor keeps that, bounded only by its own eight-second ceiling on how long
any voice may ring. The VCA restoring the envelope is an OTA, and an OTA's
control current buys bandwidth as well as gain, so the channel loses its top
before it loses its level; that, plus the analogue channel giving its highest
band the shortest envelope, is why both cymbals darken measurably as they ring
instead of fading with a frozen spectrum.

Drumalor generates what the counter reads. No sample, recording or ROM image is
embedded, and at host rates below the sample clock the clock is necessarily
limited to the host rate.

Neither machine has a modal bank, so neither cymbal here has one. A pitched
plate ringing on top of these two circuits is the one thing that stops either
of them from being recognisable, and the two voices carry nothing but the
circuits themselves.

**There is a stick on both of them.** A stick tip on a thin plate is the closest
thing in the kit to a clean Hertzian impact, so the cymbals use the derived
exponent rather than the voiced ones the membranes carry: contact time goes as
`v^(-1/5)`, with the velocity floored at 0.08 so a note-on of velocity 1 cannot
walk the result down into the cymbal's body. That single law sets two numbers,
and collapsing them into one is what makes this hard to do: a contact time long
enough to serve as an attack constant would put its spectral corner a few
hundred hertz up, which does not soften a cymbal, it deletes it.

So the contact time proper is a stick tip on bronze — 46 µs, stretched by the
law — whose corner `1/(2·tau)` sits at 10.87 kHz at full velocity and 6.56 kHz
at the floor, above the cymbal's body and below the reconstruction filter. It is
one first-order low-pass per machine, applied to each leg's own carrier ahead of
its own envelope: on the oscillator bank before the two band-passes, and on the
converter's held code before the OTA, so the quantization error is filtered with
the word it rode in on rather than after it.

The digital leg also has a trigger RC, which the machine it comes from does not
document and which it needs for the same reason the analogue one does: a step
into a resonant band-pass clicks, and at the shipping Machine defaults this is
the leg carrying 86 to 90 % of the sound. Without it that leg opened in three
samples for a brushed tip and a crash-ride accent alike. It opens over the
analogue leg's own attack constant — 0.85 ms on the Ride, 1.60 ms on the Crash
— stretched by the same `v^(-1/5)`.

Measured at 48 kHz, the digital onset to −6 dB of peak is 0.875 ms on the Ride
and 1.542 ms on the Crash against 0.062 and 0.125 ms without the smoother, and
lengthens to 3.313 and 2.688 ms at v = 0.10 — further than the bare law
predicts, because the OTA's control current closes with velocity as well and the
two are not separable. The analogue onsets are unmoved to within a sample. The
velocity brightness span is 3.11 dB (Ride) and 3.84 dB (Crash) where the
untilted circuits give 1.28 and 1.83, which is what makes velocity a stick here
rather than a fader. It is not free: the tilt
costs 0.69 dB (Ride) and 1.12 dB (Crash) of the quality probe's level and takes
the 5–14 kHz air share from 0.376 to 0.308 and from 0.561 to 0.491.

**Machine** chooses between them: fully analogue at 0, fully digital at 1, and
an equal-power crossfade in between. It is a real choice rather than a blend
control because the two channels share no source — the analogue one is its six
oscillators through the band-passes, the digital one is its own ROM — so the
ends are the two machines and not two filterings of one. Both voices default
near the digital machine, which is the one that actually had a separate ride and
crash; the analogue machine had a single cymbal, and it is still what the
control's other end gives you. The digital leg carries a measured trim so that
moving the control changes character and not loudness.

The two are voiced apart rather than being one cymbal at two speeds, because a
ride and a crash are not the same instrument played differently. The crash runs
a faster sample clock - which also lifts the reconstruction filter's corner, so
more of its top survives the converter - over a ROM whose wash sits higher and
whose partials are quieter, and it holds its upper bands far longer through both
channels. Measured at their defaults, the crash starts half again as bright as
the ride and is still brighter in its third second than the ride is a tenth of a
second in, and it rings about four times as long. The two channels' output VCAs
are biased differently for the same reason: an OTA gives up bandwidth as its
control current falls, and a ride is meant to darken as it goes where a crash is
meant to stay a splash for its whole length.

**Tone** and **Brightness** are
the tone-control mixer; because that mixer is the last stage before the buffer
amplifier, the digital channel passes through it too, split by a single
first-order crossover in place of three analogue legs it does not have.

Once a VCA is shut far enough that its band cannot reach −150 dB — and the
squared swing law means the envelope only has to fall 90 dB for the gain to be
180 dB down — the sections feeding it stop running. The boundary is derived
from the voice's own decay, so it lands at the same instant at every sample
rate and under every host block partitioning, and the suite requires the
decaying tail to contain no sample-to-sample step larger than the loud part of
the hit already made.

Removing the modal bank from both cymbals took more work out of the voice than
the ROM read put back, so the pair got cheaper as well as more like the machines
they come from. Measured on one Linux x86-64 machine at 48 kHz in 128-sample
blocks, against the previous cymbals:

| Pattern | Before | After | Change |
| --- | ---: | ---: | ---: |
| Ride, 16th notes at 120 BPM | 11.6% | 9.9% | −15% |
| Ride on 8ths with kick, snare and crash | 7.6% | 7.1% | −7% |
| All thirteen voices | 10.0% | 9.8% | −3% |

Those figures are CPU time as a fraction of real time and are specific to that
machine and harness; only the before/after ratio is meaningful. The saving comes
from the tail, so a pattern dense enough to keep all 64 voices young sees less of
it.

These are circuit-inspired behavioral models, not a claim of
component-for-component emulation of a TR-808, TR-909, or another specific
machine. No neural-network weights are needed, so the audio path stays
allocation-free, deterministic, and suitable for real-time use.

All synthesis happens in the audio callback without sample files. The engine is
prepared for the host sample rate, accepts sample-accurate MIDI event offsets,
and clears completed one-shot voices after their tails finish.

## Vintage interface

The editor uses a generated geometry-free powder-coat plate only as a restrained
material texture; all panel geometry is rendered by the responsive JUCE layout. A
compact equal-width channel grid, illuminated selection rails, separate Voice
Circuit and Kit Bus decks, scaled metal-collared Bakelite knobs, recessed value
readouts, bipolar Pitch indication, parameter-aware reset gestures, tooltips,
and clearer typography create a denser hardware hierarchy without losing
accessibility or resize support. Its near-black face, neutral hardware,
warm legends, and ordered red/orange/yellow/cream channel accents borrow the
colour rhythm associated with classic early-1980s rhythm composers while
retaining Drumalor's own branding and layout. The texture is compiled into the
plug-in as binary data, so there is no external image to install. The visual
direction is era-inspired rather than a copy of any historical drum machine's
panel or trade dress.

The panel is now also a meter bridge. Every channel pad carries a recessed
activity rail that fills with that voice's own measured level and leaves a
peak-hold marker behind, turning the thirteen-pad grid into a live channel
overview rather than a row of note-on flashes. A stereo bus meter in the header
shows the output with peak hold, silkscreen marks at -36, -24, -12 and -6 dB,
and a separate strip that grows leftwards with the bus compressor's gain
reduction. The Voice Circuit deck holds five knobs plus a horizontal Pan slider
and the Choke Group selector; the Kit Bus deck holds Humanise, Bleed, Drive,
Comp and Output in a two-column grid.

The presentation mathematics behind all of that - the decibel meter curve and
its exact inverse, the asymmetric attack/release/peak-hold ballistics, the
pad-grid geometry, and the colour-ramp curves - lives in the JUCE-free
`Source/DSP/UiMath.*` library and is unit-tested with the synthesis engine. The
JUCE layer only renders it.

**Note:** the screenshot above still shows the version 1.0 interface. It can
only be regenerated from a macOS build.

## Research influences and modeling scope

The implementation follows recent virtual-analog work where it fits a
self-contained real-time instrument:

- Gabrielli and Squartini's [2025 ADAA study](https://www.dafx.de/paper-archive/2025/DAFx25_paper_30.pdf)
  motivates antiderivative treatment of nonlinear stages as a lower-cost route
  to reduced aliasing.
- Pines' [2025 diode-VCA model](https://dafx25.dii.univpm.it/wp-content/uploads/2025/07/DAFx25_paper_44.pdf)
  motivates explicit fixed nonlinearities with variable operating points.
- Avanzini and Marogna's [energy-estimation approach to tension modulation](https://pubmed.ncbi.nlm.nih.gov/22280712/)
  motivates driving the membrane banks' pitch bend from a running estimate of
  the system's energy rather than from a solved nonlinear membrane, which is
  what makes it affordable inside a thirteen-voice kit. The same argument sets
  how deep the drawn pitch sweep goes: the depth is the square of what the
  strike leaves in the head, latched at note-on rather than followed.
- Worland's [normal modes of a drumhead under non-uniform tension](https://pubs.aip.org/asa/jasa/article/127/1/525/793705/)
  is why every mode above the first is emitted as the two modes it physically
  is. Ordinary lug-tension variation is enough to lift the degeneracy, and two
  close frequencies sounding together are the warble a drummer tunes out with a
  key.
- Werner, Abel, and Smith's [physically informed bass-drum analysis](https://dafx.de/paper-archive/2014/dafx14_kurt_james_werner_a_physically_informed%2C_ci.pdf)
  and Germain's [time-varying numerical study](https://www.dafx.de/paper-archive/2021/proceedings/papers/DAFx20in21_paper_43.pdf)
  motivate charged state, resonant feedback, changing pitch/loss, and stable
  time-varying updates for the Kick.
- Werner, Abel, and Smith's [TR-808 cymbal circuit analysis](https://pureadmin.qub.ac.uk/ws/portalfiles/portal/125044847/tr_808_cymbal_a_physically_informed_circuit_bendable_digital.pdf)
  is the source for the analogue cymbal channel: the measured six-oscillator
  frequencies and pulse duty cycle, the fixed-versus-trimmed split across those
  six, the two band-pass centres, and the block order — oscillators, band-passes,
  trigger logic and attack smoother, envelope generators, swing-type VCAs,
  Sallen-Key high-passes, tone-control mixer, output buffer. Olsen,
  Werner, and Germain's [network-variable-preserving oscillator study](https://dafx.de/paper-archive/2017/papers/DAFx17_paper_74.pdf)
  motivates combining relaxation-circuit state, accurate edge timing, and BLEP
  correction rather than choosing between physical modeling and antialiasing. The
  [TR-909 service notes](https://www.polynominal.com/site/studio/gear/drum/roland-tr909/roland-tr909-service-manual.pdf)
  document the digital channel: cymbal memories addressed by a counter at a
  tunable, divided-down sample clock, read as six-bit companded codes whose
  envelope is restored after the converter, with that envelope generated by a
  second converter on the address lines through an anti-log stage so it tracks
  the clock rather than the wall. Drumalor models that clock, converter and
  envelope with newly generated data rather than embedding the original
  recordings.
- Esqueda and Murai's [2025 antialiased recurrent model](https://dafx25.dii.univpm.it/wp-content/uploads/2025/09/DAFx25_paper_61.pdf)
  shows that compact learned state-space models can run in real time. Drumalor
  deliberately does not use one: without measurements from a defined target
  circuit, weights would be an uncalibrated black box rather than a more
  defensible analog model.

The result is a modern behavioral VA design with original sound architecture,
not a calibrated hardware replica. Listening comparisons and profiling on the
oldest supported Mac remain part of release qualification even though the
automated stability, performance, and spectral contracts pass.

## Requirements

- macOS 11 or newer for running the built products
- A current full Xcode installation selected for command-line use
- CMake 3.22 or newer
- Internet access for the default first configure, or a local JUCE 8.0.14
  checkout supplied through `JUCE_PATH` to the helper
  (`DRUMALOR_JUCE_PATH` when configuring CMake directly)

JUCE 8.0.14 is fetched at configure time and is not vendored into this
repository.

## Build on macOS

The helper creates an Xcode build, compiles universal `arm64`/`x86_64`
binaries, and runs both the DSP and JUCE processor-contract tests:

```bash
./scripts/build-macos.sh
```

Equivalent commands, useful when opening and developing in Xcode, are:

```bash
cmake -S . -B build-macos -G Xcode \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DDRUMALOR_BUILD_UNIVERSAL=ON \
  -DDRUMALOR_BUILD_PLUGIN=ON \
  -DBUILD_TESTING=ON

cmake --build build-macos --config Release --parallel
ctest --test-dir build-macos -C Release --output-on-failure
open build-macos/Drumalor.xcodeproj
```

To avoid the FetchContent download, point the configure at a local checkout of
the exact JUCE release:

```bash
JUCE_PATH="$HOME/SDKs/JUCE-8.0.14" ./scripts/build-macos.sh
```

For a native-only development build, use `BUILD_UNIVERSAL=OFF`. Override
`BUILD_DIR`, `CONFIG`, or `MACOSX_DEPLOYMENT_TARGET` in the environment when
needed.

Release bundles are written to:

| Format | Build artifact |
| --- | --- |
| VST3 | `build-macos/Drumalor_artefacts/Release/VST3/Drumalor.vst3` |
| Audio Unit | `build-macos/Drumalor_artefacts/Release/AU/Drumalor.component` |
| Standalone | `build-macos/Drumalor_artefacts/Release/Standalone/Drumalor.app` |

## Run the DSP tests without JUCE

The synthesis core deliberately has no JUCE dependency. This provides a quick
test path on any C++20 development machine without downloading the framework:

```bash
cmake -S . -B build-dsp \
  -DCMAKE_BUILD_TYPE=Release \
  -DDRUMALOR_BUILD_PLUGIN=OFF \
  -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The JUCE-free regression executable renders every voice from 8 to 192 kHz. It
checks finite, non-silent, bounded output, completed tails, hi-hat choking, all
four original controls on every voice, sample-rate consistency, saturated voice
stealing, and a generous offline performance guardrail. Organic-model contracts verify
that six equal strikes differ for all 13 voices while RMS, peak, and natural-tail
spread stay bounded; they also verify bit-exact reset replay and block-partition
invariance. A dedicated metallic-source contract verifies across all five banks
and three elapsed-time offsets that silence advances the free-running source
without leaking through closed VCAs, and that its reconstruction remains
independent of block partitioning. Restored-state and silent-automation checks
also keep the persistent bank controls synchronized. Kick-specific contracts
cover a 43–55 Hz settled body, dominant sub-100 Hz energy, controlled transient
and crest factor, Drive harmonics without excess settled energy above 8 kHz,
pitch tracking, DC safety, and consistency from 8 to 192 kHz. Cymbal contracts
reject hollow midrange gaps and sparse
ringing tails, require both ends of Machine to stand alone as complete cymbals,
verify directional Tone/Brightness response, and hold the whole Machine sweep
level so the control cannot be a volume control in disguise.

A second cymbal group tests the circuit rather than the spectrum, so each check
fails for one missing block instead of for a general loss of realism. Both
voices must open through the attack smoother rather than as a step into a
resonant band-pass, and must reach their onset peak after it, not on the first
sample. Both must darken as they ring. Retuning the sample clock must carry the
address envelope with it, so a cymbal transposed down a twelve holds at least
twice the relative tail of one transposed up. Velocity must behave as an accent
rather than as an output gain, which each channel is asked for in the terms it
can answer: measured at the analogue end, a quiet hit must leave sooner and not
merely be quieter, because the swing VCAs are superlinear in the accent voltage;
measured at the digital end, where the address envelope is walked by the counter
and no accent can shorten it, a quiet hit must instead be duller, because the
OTA restoring the envelope loses bandwidth along with gain. The decaying tail must contain
no sample-to-sample step larger than the loud part of the hit already made,
which is what retiring a shut band audibly would look like. And because the
sample clock is fixed in hertz and the ROM's length in seconds, neither may
follow the host rate: both voices hold their level within 1.5 dB from 44.1 to
192 kHz.

Version 1.1 adds contracts for everything it introduces. The kit mixer is held
to a clean -6 dB gain law, a symmetric pan law, hard-panned channel isolation,
the original default positions, and bit-identical output at unity. Choke groups
are checked for the factory hi-hat pair, for arbitrary linked voices, for
independence between groups, and for tails that keep the group they were born
into. Humanise is verified to order hit-to-hit spread across its range and to
stay reproducible after reset at every setting. Every one of the thirteen voices
is separately required to be same-but-different: no two consecutive strikes at
one velocity may be identical, the difference between them must be audible rather
than in the last bit, and the spread across the strikes must stay inside what a
machine's tolerances would explain. The kit bus is checked for exact
bypass at 0%, safe and level-matched saturation, real dynamic-range reduction,
released gain reduction, and block-partition invariance. Metering is checked for
attack, release, stereo placement and reset. Membrane contracts require audible
inharmonic head content at the strike that decays faster than the body, a Skin
control that actually moves the air loading, a level-dependent snare wire
rattle, and darker soft hits on all ten velocity-timbred voices. A
tension-modulation contract tracks the dominant head partial of the Kick and all
three Toms and requires a full-velocity strike to ring sharper than a ghost
stroke while the strike energy is still in the head, requires the two to settle
at the same pitch once it has gone, and requires the tail to keep its decay
range at both ends of Skin - which is what proves the model moves only `a1` and
leaves the pole radius, and therefore the decay time, alone. An articulation
contract checks that notes 38, 40 and 37 carry the head, rimshot and cross-stick
strokes, that 44, 53, 52 and 55 carry the foot chick, bell, china and splash,
that every other mapped note in the whole 0-127 range carries a plain head
stroke and nothing else, that the rimshot
engages the wires harder and reaches higher up the head's series than a plain
stroke, that the cross-stick both silences the wires and rings for well under
half as long, and that neither of the two nulls against a peak-matched head hit.
A re-strike contract requires a 15 ms flam on each of the five membranes to
carry less energy than the two strokes rendered separately, requires a ghost
stroke to leave more of a ringing head alive than a full one does, requires a
forty-eight-stroke press roll to stay bounded and to hold well under half as
many voices as it has strokes, and requires a second ride strike to still add
rather than being damped as though a plate were a drum head. A hi-hat pedal
contract requires a fully closed pedal to reproduce the Closed Hat and a fully
open one the Open Hat sample for sample, requires each of five pedal positions
to ring shorter than the last and to differ from its neighbour by more than a
level change, requires closing the pedal on a ringing open hat to damp it and
shutting it to cut it, requires a fast close to make a foot chick while a lift
from silence or a rest makes none, and requires reset to release the pedal and
nonsense controller values to be refused. A kit-bleed contract requires exact bypass at
zero, requires a kick alone to put energy into the snare's wire band and to put
more of it in at every higher setting, requires the buzz to lift off a threshold
rather than track the kick in proportion, requires the result to be independent
of the host block size, requires a bed that has been switched off to be at rest
rather than frozen when the control comes back on, and requires an idle kit to
stay at digital zero however long it idles.
A dedicated
efficiency contract proves that a metallic bank frozen behind an unrelated drum
wakes into exactly the same state as one frozen during silence, and measures
that adding a hi-hat costs meaningfully more than the same kick alone, which is
only true while unobservable banks stay frozen. On x86 it also renders a busy
kit twice, once with flush-to-zero set and once without, and requires the two to
cost the same, which only holds while the engine floors its own recursive states.

Three further contracts guard the later pass. A noise-density contract holds the
kick click, the snare wires, the clap burst and the shaker grain to a flat
filtered level from 44.1 to 192 kHz - each of them moved by 2.9 to 6.4 dB before
the noise generator was moved onto a fixed grid. A bus-automation contract sweeps
Bus Drive and Bus Compression both on and off mid-tail over a sustained deep
kick, at 0.1 %, 50 % and 100 % and at 44.1, 48, 96 and 192 kHz, and requires the
boundary step, the peak and the largest sample-to-sample jump of the whole half
second that follows to stay within what that tail was already doing. The step at
the boundary used to be 24 to 143 times the waveform's own motion; the ramp down
to bypass used to put jumps up to 1800 times larger into the second after the
move, which watching only the boundary sample of only the off-to-on direction
missed entirely. And a Perc 1 contract requires its Drive to reduce the crest
factor while holding the level, which
distinguishes a saturation control from the level trim it had become. The presentation library gets
its own contracts for the meter curve and its inverse, ballistics, pad-grid
geometry, and sanitisation of invalid input.

Four more guard the metallic half and the tails. A cymbal contact contract
requires both cymbals' digital legs to open inside a window a trigger RC has to
land in rather than in three samples, requires the Ride to open measurably
slower for a ghost note than for an accent, requires the analogue onsets to stay
within 0.05 ms of where they were, and requires at least 3 dB of velocity
brightness on both voices where there used to be 1.3 and 2.0 — while every
existing cymbal quality bound stays green, since a low-pass on the leg carrying
86 to 90 % of the energy is exactly what those bounds exist to catch. A membrane
glide contract reads the engine's own oscillator trace and requires a ghost
stroke to glide at most 35 % as far as an accent on the Kick and all three Toms,
requires the accent's glide to stay within 15 % of where it was so this is a
redistribution rather than a reduction, requires both strengths to settle on the
same note, and requires the Kick's early band balance to fall by at least 6 dB
at a ghost stroke while moving by less than 0.5 dB at an accent. A membrane tail
contract sets Decay to 0.85, isolates each drum's m = 1 pair by quadrature
detection — which puts the shell oscillator and every other neighbour more than
80 dB down, where a band-pass at the Q the analysis would otherwise need leaves
the drum's own fundamental above the pair it is trying to hear — and requires
the beat to run at the rate that drum's own split implies within 20 %, to be at
least 4.5 dB deep, to be identical between two hits at Humanise 0, and to spread
by at least 2.5 dB over eight hits at Humanise 100% while spreading by at most
0.10 dB at Humanise 0. Both rate and depth are asserted, because an unsplit
bank's decay curvature can be deep without being periodic. And a MIDI surface
contract renders every new note on a fresh engine and requires note 44 to differ
from note 42 and to reach less far up the plate, note 53 to differ from note 51
and to outlast it, notes 52 and 55 to differ from note 49 while staying within
6 dB of its first 60 ms — because a level-matched residual against silence is
not a small number — aftertouch to choke progressively rather than as a switch,
CC 88 to move a note's velocity in the right direction while an un-prefixed
note-on stays bit-identical, and a partial pedal close released at 40 ms to
leave at least 12 dB more tail than the same close held.

Plug-in builds add a JUCE-backed processor contract suite for parameter defaults
and state, version-1.0 host parameter index stability, restoring a session that
predates the new parameters, sample-accurate MIDI, CC panic, the UI-trigger
lifecycle, and off-screen rendering of the embedded vintage editor. These checks
do not replace listening tests, host automation tests, or profiling on the oldest
supported Mac.

## Install locally

For per-user installation, copy only the formats you need:

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"

ditto build-macos/Drumalor_artefacts/Release/VST3/Drumalor.vst3 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Drumalor.vst3"
ditto build-macos/Drumalor_artefacts/Release/AU/Drumalor.component \
  "$HOME/Library/Audio/Plug-Ins/Components/Drumalor.component"
```

Standard discovery locations are:

| Scope | VST3 | Audio Unit |
| --- | --- | --- |
| Current user | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |
| All users | `/Library/Audio/Plug-Ins/VST3/` | `/Library/Audio/Plug-Ins/Components/` |

The standalone app can be copied to `/Applications` or launched directly from
the artifacts directory. Quit and reopen the host after installing. If Logic
retains an older AU during development, log out and back in or restart the
Audio Component Registrar before rescanning.

## Validate the plug-in

Run the DSP tests first, then check that each release executable is universal:

```bash
lipo -archs build-macos/Drumalor_artefacts/Release/VST3/Drumalor.vst3/Contents/MacOS/Drumalor
lipo -archs build-macos/Drumalor_artefacts/Release/AU/Drumalor.component/Contents/MacOS/Drumalor
lipo -archs build-macos/Drumalor_artefacts/Release/Standalone/Drumalor.app/Contents/MacOS/Drumalor
```

Each command should report both `arm64` and `x86_64`. After installing the AU,
validate its type `aumu`, subtype `Drm1`, and manufacturer `Dral`:

```bash
auval -v aumu Drm1 Dral
```

With [pluginval](https://github.com/Tracktion/pluginval) installed, validate
the VST3 at the highest strictness level:

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 10 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Drumalor.vst3"
```

Also exercise all 13 note mappings, the snare's three articulation notes and the
hat and cymbal notes 44, 52, 53 and 55, velocity extremes, rapid retriggers, a
continuous CC 4 hi-hat pedal from an electronic kit or a controller lane
including a splash release, channel and polyphonic aftertouch on a ringing
cymbal, a CC 88 prefix ahead of a note-on, the open/closed-hat choke, all 91
voice parameters and the five kit controls, choke groups, project-state recall,
sample-rate changes, and buffer sizes from 32 to 2048 samples in at least two
hosts.
A validator passing does not guarantee musical or host-level correctness.

## Sign, package, and notarize

For local testing, the packaging helper uses ad-hoc signing by default:

```bash
./scripts/sign-and-package-macos.sh
```

It stages the VST3, AU, and standalone app, verifies their signatures, and
creates a ZIP and installer package under `build-macos/dist/`. The default
universal build uses the `macOS-universal` filename suffix; a native-only build
is labelled with its actual architecture instead.

For public distribution, first import valid `Developer ID Application` and
`Developer ID Installer` certificates. Store notarization credentials once in
the login keychain; do not put credentials in this repository:

```bash
xcrun notarytool store-credentials drumalor-notary \
  --apple-id "developer@example.com" \
  --team-id "YOURTEAMID" \
  --password "APP-SPECIFIC-PASSWORD"
```

Then sign, package, submit, wait for Apple's result, and staple the ticket:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Company (YOURTEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Company (YOURTEAMID)" \
NOTARY_PROFILE="drumalor-notary" \
./scripts/sign-and-package-macos.sh
```

With `NOTARY_PROFILE` set, the helper submits and staples the installer package.
The ZIP is still produced, but it is not the notarized distribution artifact;
publish the `.pkg`, or run a separate bundle/ZIP notarization workflow before
distributing the ZIP.

Before publishing, verify the package from a clean user account and inspect its
signature and Gatekeeper assessment:

```bash
pkgutil --check-signature build-macos/dist/Drumalor-1.1.0-macOS-universal.pkg
spctl --assess --type install --verbose=4 \
  build-macos/dist/Drumalor-1.1.0-macOS-universal.pkg
```

The bundle identifier `audio.drumalor.synth`, manufacturer code `Dral`, and
plug-in code `Drm1` are the host-facing identity. Confirm that the publisher
controls them before the first public release, then never change them: hosts
use these values to associate saved projects with the correct plug-in. Keep the
CMake project version and packaging-script version in sync for each release.

## Project layout

```text
Source/DSP/              JUCE-free synthesis engine, voice metadata, and UI maths
Source/PluginProcessor.* MIDI mapping, parameters, state, and audio bridge
Source/PluginEditor.*    Metered thirteen-pad editor, voice deck, and kit bus deck
Assets/                  Embedded geometry-free charcoal material texture
Docs/                    Rendered demo audio and the interface screenshot above
Tests/                   DSP and JUCE processor-contract regression tests
Tools/                   Demo renderer that keeps Docs/audio and its level table in sync
Presets/                 Preset guidance and future factory presets
scripts/                 macOS build and release helpers
```

## Licensing

The original Drumalor source is offered under the MIT License. JUCE is a
separate dependency and is not covered by that licence. JUCE 8 framework
modules are available under the AGPLv3 or a commercial JUCE licence.
Distributing a closed-source or otherwise AGPL-incompatible binary generally
requires an appropriate commercial JUCE licence. Confirm the current terms for
the publisher and use case before shipping; see `THIRD_PARTY_NOTICES.md` and
JUCE's official licence.

No drum samples, impulse responses, neural model weights, factory ROMs, or
third-party presets are included.

## Changelog

- 2026-08-16: Added regression coverage for `getInstrumentLevel()`'s own `validInstrument()` guard, which every existing caller - including `testMetering()`'s per-instrument sweep - only ever drove with one of `Instrument`'s own enumerators, in range. A new test asserts an index one past the table and a far out-of-range index both report silence rather than reading past the end of `instrumentLevels_`, both while the engine is silent and while a Kick is actually sounding, so the guard is shown to reject the index before it ever reaches the array rather than merely happening to land on a quiet slot. Verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `setKitParameters()`'s own humanise/bleed/busDrive/busCompression guards, each an independent `clampUnit()` fallback-and-clamp that the broad invalid-values stress test only ever drove far enough to confirm the render stayed finite, never far enough to pin the fallback to the kit's own documented default (0.5 for humanise, 0.0 for the other three) or the clamp to its `[0, 1]` range - and that stress test never touched `bleed` at all. A new test compares a Kick struck after each field is set to NaN, +infinity or -infinity against the same Kick struck after the field's documented default, and an out-of-range finite value against its 0.0/1.0 floor and ceiling, asserting bit-identical output in every case. Verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `process()`'s own non-positive `numSamples` guard, which every other block-partitioning contract in the suite only ever drove with a positive count. The guard is not only a no-op: the line just past it adds `numSamples` to the `uint64_t` `engineSamples_` clock after an unsigned cast, so a zero or negative count reaching it would corrupt the engine's own elapsed-time clock - the same clock the metallic banks' freeze/wake logic, the sympathetic beds, and tail retirement deadlines all read - instead of leaving it alone. A new test interleaves zero and negative-count calls both into a freshly prepared engine and into an otherwise-identical staggered thirteen-voice sequence, and asserts the guarded engine's rendered audio stays bit-identical to one that never received them, and that the guard also blocks any write through the caller's buffer; temporarily disabling the guard made the staggered-sequence assertion fail as expected. Verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `setInstrumentParameters()`'s own pitch, level and pan guards, each an independent `std::isfinite (values.field) ? values.field : defaults.field` fallback followed by a clamp to its documented range: the broad invalid-values stress test only checked that a NaN/infinite pitch, level or pan kept the render finite, never that it actually landed on the instrument's own default or on the ±24 semitone, [-24, 6] dB or ±1 pan clamp. `snapshotParameters()`, the only accessor that reads the sanitized value back, is private, so the new test compares a Kick struck after a NaN/+infinity/-infinity field against the same Kick struck after the equivalent explicit default, and an out-of-range finite field against its documented ceiling/floor, asserting bit-identical output in each case; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `setOutputGain()`'s own NaN/infinity fallback and range clamp: the existing invalid-values stress test only checked that a NaN gain kept the render finite and non-zero, never that it actually landed on the documented 0.82 fallback. A new test compares a kit struck after `setOutputGain(NaN)`, `setOutputGain(+inf)` and `setOutputGain(-inf)` against the same kit struck after the equivalent explicit `setOutputGain(0.82f)` call, and an out-of-range gain against its 2.0/0.0 ceiling and floor, asserting bit-identical output in each case; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `applyAftertouch()`'s own pressure guard on both overloads: the channel-wide form's `chokedCrash` test fixture only ever called it with a positive pressure, so its non-finite/non-positive early return had never been exercised, and the note-targeted overload's existing zero-pressure assertion checked only its return value, not that the cymbal it named was actually left ringing. A zero, negative, or NaN pressure on either overload now asserts the choked cymbal's level still matches the unchoked baseline; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `DrumEngine::prepare()`'s own sample-rate sanitization: every sampleRate-sweep test in the suite, including the per-instrument and modal-consistency ones, only ever calls `prepare()` with an already-valid rate, so the `if (! std::isfinite (sampleRate)) sampleRate = 48000.0;` guard ahead of the `[8000, 192000]` clamp had never been driven by anything. A new test strikes the Kick after `prepare()` with NaN, +infinity, -infinity, and out-of-range finite rates (500000.0, -100.0) and asserts the render is bit-identical to the same strike after the equivalent, already-sanitized `prepare()` call; temporarily disabling the guard's fallback made the new NaN/infinity assertions fail as expected. Verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-15: Deduplicated the xorshift32 noise generator that `nextNoise()` and the sympathetic-bed rattle in `renderSympatheticBeds()` each implemented separately into one shared `advanceXorshiftNoise()` helper, with no change to engine output.
- 2026-08-15: `buildHeadBank()`'s per-mode radiation-efficiency term, already computed once inside `lossPerSecond()`, is now shared with its `pow()`-based "heard" weight instead of being re-derived from the same inputs a second time, saving one transcendental call per emitted head mode with no change to engine output.
- 2026-08-15: Hoisted the duplicate `std::pow(voice.pitchRatio, 0.42f)` call out of `initialiseVoice()`'s Clap case, where the direct-impact and diffuse-tail bandpass filters each recomputed the identical pitch-tracking term, with no change to engine output verified by byte-identical rendered demo audio.
- 2026-08-15: Hoisted `initialiseVoice()`'s pitch-sweep saturation amplitude, `accentVoltage(sweepSaturationVelocity) * excitationScaleFor(sweepSaturationVelocity)`, out to a file-scope constant since `sweepSaturationVelocity` never changes; it was being re-derived (two `sqrt()` calls) on every note-on across the whole kit for an answer that is always the same, with no change to engine output (verified bit-identical against the committed demo renders).
- 2026-08-15: `buildHeadBank()`'s mode-pair splitting search recomputed each still-eligible candidate's `pow()`-based gain from scratch on every one of the (up to four) rounds it takes to find each pair, even though a candidate's ratio and excitation - and therefore its gain - do not change until it is actually chosen and split. Each candidate's gain is now resolved once into a small local cache before the search runs, cutting the redundant `pow()` calls on every struck-head note-on with no change to which pairs are chosen; verified bit-identical against the committed demo renders and via the JUCE-free regression suite.
- 2026-08-15: The final gain-and-normalize pass in `buildHeadBank()` recomputed the same `pow()`-based mode gain a further time for every emitted mode, even for the split-eligible candidates that were never picked by the search and whose ratio and excitation - and therefore whose gain - are unchanged from when the search's own `candidateGain[]` cache was built. Those unpicked candidates now reuse that cached value instead of resolving `pow()` a third time, with no change to engine output; verified bit-identical against the committed demo renders and via the JUCE-free regression suite.
- 2026-08-15: `reset()`, `dampRingingMembrane()`, `chokeGroup()`, `allSoundsOff()`, `updateActiveVoiceCount()` and `process()` each walked the live `voices_` pool and the retiring `retiringVoices_` pool with their own identical pair of range-for loops around whatever they actually did per voice. Extracted a shared private `forEachVoice()` helper (const and non-const overloads) that walks both pools in the same order and calls the given functor on each voice, and rewrote all six call sites to use it, with no change to which voice is visited or when; verified bit-identical against the committed demo renders and via the JUCE-free regression suite.
- 2026-08-15: The `forEachVoice()` dual-pool walk introduced above missed three call sites that still had their own raw `for (auto* pool : { &voices_, &retiringVoices_ })` loop: both `applyAftertouch()` overloads and `setHiHatPedal()`'s pedal-move voice update. Rewrote all three to use `forEachVoice()` like the rest of the engine, and hoisted `applyAftertouch(float)`'s per-voice `chokeSecondsForPressure()` call to once per invocation (matching the note-targeted overload, which already did this), with no change to which voice is visited, when, or with what choke duration; verified bit-identical against the committed demo renders and via the JUCE-free regression suite.
- 2026-08-15: Added regression coverage for the sanitize paths of `UiMath`'s `onePoleCoefficient`, `decayMultiplier` and `smoothStep` (NaN and out-of-range seconds, decibels, update rate and edge inputs), which were exercised by callers but not previously asserted directly; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for two more `UiMath` sanitize paths that were exercised by callers but never asserted directly: `mix()`'s fallback of a NaN start/end value to zero, and `MeterBallistics::update()`'s independent fallback of a NaN attack coefficient, release coefficient, peak-fall multiplier or hold-updates duration to its own documented default; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Closed the last two gaps in `UiMath`'s sanitize test coverage: `smoothStep()`'s fallback of a NaN second edge to its documented default (1.0), and `mix()`'s fallback of a NaN amount to its zero default, neither of which the two prior sanitize-coverage passes had asserted directly; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `linearForMeterPosition()`'s own sanitize paths, which the meter-curve round trip test only ever drove with in-range inputs: a NaN or out-of-range position clamps to the meter floor/ceiling instead of propagating, and a NaN or non-negative floor decibel value falls back to its documented default (-60 dB) before being clamped below -1 dB, matching what an explicit in-range call already produces; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Closed the remaining `UiMath` sanitize gaps the prior passes missed: `decayMultiplier()`'s fallback of a NaN update rate to its default, and `meterPositionForLinear()`/`linearForMeterPosition()`'s independent fallbacks of a NaN floor (both functions) and a NaN meter position (`linearForMeterPosition` only) to their documented defaults; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `velocityFromMidi()`'s own clamp of an out-of-range MIDI byte, which every existing CC 88 test only ever drove with in-range values: a velocity byte below 0 or above 127, and a high-resolution LSB below 0 or above 127, now clamp to 0 or 127 instead of propagating out of range, matching what an explicit in-range call already produces; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `getInstrumentMetadata()`'s own out-of-range fallback, which every existing call only ever drove with one of `Instrument`'s own enumerators: an index one past the metadata table, and a far out-of-range index, both fall back to slot zero (Kick) instead of indexing past the table; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Added regression coverage for `cellOffset()`'s own clamp of a negative gap and a negative index, which every existing pad-grid test only ever drove with the non-negative values `rowLayout()` itself hands back: both now sanitize to zero instead of pulling the offset backwards, matching what an explicit zero-gap or zero-index call already produces; verified with `ctest --test-dir drumalor/build-dsp` (all tests pass; test-only change, no engine code touched).
- 2026-08-16: Deduplicated the identical input/previous-sample sanitize-and-clamp step that `antialiasedRationalShaper()` and its cached sibling each repeated into a shared `sanitizeAdaaPair()` helper, with no change to engine output.
- 2026-08-16: Added regression coverage for `trigger()`'s own guard - `! validInstrument (instrument) || ! std::isfinite (velocity) || velocity <= 0.0f` - which every one of the suite's many thousands of prior calls only ever drove with a valid instrument and a positive, finite velocity, so the early return itself had never been exercised. A NaN, +infinity, -infinity, zero or negative velocity, and an out-of-range instrument at a valid velocity, now each assert both that no voice is allocated and that the following block renders bit-identically to an engine that was never triggered at all; disabling the guard made every new assertion fail as expected. Verified with `ctest --test-dir drumalor/build-dsp` (all tests pass) and a demo re-render showing `git status drumalor/Docs/audio` clean (test-only change, no engine code touched).
- 2026-08-16: Fixed `updateSympatheticBeds()` retuning every one of the four sympathetic beds (the Snare's resonant head and the three toms) whenever any single one's Pitch or Decay changed, because it called `configureSympatheticBeds()` - which rebuilds all four - as soon as it found the first bed whose values had moved. `configureResonator()` ends by clearing the resonator it just retuned, so the other beds were silently losing whatever they were still ringing with as a side effect of a knob that had nothing to do with them: turning the Snare's own Decay could cut off a still-ringing Low/Mid/High Tom sympathetic bed mid-kit. Split the per-bed body out into a new `configureSympatheticBed(index)` and had `updateSympatheticBeds()` call it only for the bed whose own Pitch/Decay actually changed, which also drops the wasted reconfiguration of the unaffected beds. Verified with a new regression test that retunes only the Snare and asserts the three toms' bed resonator state comes out bit-identical to a run where nothing was retuned (it fails as expected against the old code), `ctest --test-dir drumalor/build-dsp` (all tests pass), and a demo re-render showing `git status drumalor/Docs/audio` clean (no demo automates decay/pitch mid-render, so none of the seven WAVs changed).
