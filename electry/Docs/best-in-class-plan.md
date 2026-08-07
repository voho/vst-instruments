# Electry: closing the gap to the commercial top tier

This document records what Electry is competing against, where it measurably
falls short, and the ordered set of changes that close those gaps. It is a
working plan, not a marketing sheet: the gap analysis names things Electry does
badly, and steps that turn out to be wrong when implemented are struck out here
rather than shipped.

## The field

Two families of product occupy the high end of virtual electric guitar, and
they fail in opposite directions.

**Deeply sampled libraries.** Prominy's *SC Electric Guitar 2* ships roughly
147 GB and 200,000 samples at $399, and its headline features are performance
ones - "Realtime Legato Slide", "Realtime Hammer-on & Pull-off", and a
"Super Performance Multi" that reaches every technique without stopping the
performance ([Prominy](https://prominy.com/products/sc-electric-guitar-2/),
[retail listing](https://pluginfox.com/products/prominy-sc-electric-guitar-2)).
Impact Soundworks' *Shreddage 3* line ($149 per instrument, bundles above that)
lists sustains, power chords, palm mutes, harmonics, **pinch harmonics**,
tapping, hammer-on/pull-off, **portamento**, tremolo, **fingered vibrato**,
pitched and unpitched release noises, rakes, muted chokes and DI line noise,
and exposes keyswitch functions named Force String, Picking Mode, **Set Hand**
and **Fretting Mode**
([Impact Soundworks](https://impactsoundworks.com/product/shreddage-3-jupiter/),
[Sample Library Review](https://www.samplelibraryreview.com/the-reviews/review-shreddage-3-virtual-guitar-instruments-line-by-impact-soundworks/)).
Orange Tree Samples' *Evolution* series is repeatedly described as the most
convincing of them, and what reviewers single out is not the samples but the
engine: "individually sampled strings, powered by a performance modeled
string/fret selection engine", pick-position modelling, chord recognition, and
a strum engine
([Sample Library Review](https://www.samplelibraryreview.com/the-reviews/review-evolution-strawberry-kontakt-player-edition-electric-guitar-orange-tree-samples/)).
Three-Body Technology's *Heavier7Strings* (~$200) and MusicLab's *RealLPC*
occupy the same tier for metal and for Les Paul rhythm respectively
([KVR](https://www.kvraudio.com/product/heavier7strings-by-three-body-technology),
[MusicRadar round-up](https://www.musicradar.com/music-tech/plugins/best-guitar-vsts)).

**Physically modelled instruments.** Applied Acoustics Systems' *Strum GS-2*
is the only mainstream sample-free guitar: 36 electric parameters, an 84-chord
recogniser, and note-to-string assignment by "most likely" string
([MusicRadar review](https://www.musicradar.com/reviews/tech/applied-acoustics-systems-strum-gs-2-626643),
[AAS](https://www.applied-acoustics.com/strum-gs-2/)). Reviewers consistently
say the same two things about it: the model is remarkably clear and expressive,
and it "sometimes sounds almost too clean"
([All Things Gear](https://allthingsgear.com/aas-strum-gs-2-review/),
[Sweetwater user reviews](https://www.sweetwater.com/store/detail/StrumGS2--applied-acoustics-systems-strum-gs-2-virtual-guitar/reviews)).
IK Multimedia's *MODO BASS 2* is the closest structural relative to Electry -
strings as nonlinear resonators, a modelled playing hand, string gauge, age and
winding type, action height driving fret noise, and a slide-noise model tied to
the winding
([IK Multimedia](https://www.ikmultimedia.com/news/?id=IKannouncesMODOBASS2),
[Sound On Sound](https://www.soundonsound.com/reviews/ik-multimedia-modo-bass-2)) -
and it is the proof that a physical model can win this category outright when
the *player* is modelled as carefully as the string.

**What separates the top tier, as reviewers describe it.** Three themes recur.
First, articulation coverage and the ability to reach articulations without
stopping - a library that cannot slide, squeal or vibrato is immediately
identifiable. Second, string and fret assignment: "controlling where notes are
played on different strings allows for much better realism, because the same
notes can sound very different depending on which strings they're played on",
and the engines that do this well expose a hand position, a fret reach, and
several fretting modes (Natural, Sweep, Moving Lead, Polyphonic)
([Orange Tree forum](https://www.kvraudio.com/forum/viewtopic.php?t=441134),
[Sample Library Review](https://www.samplelibraryreview.com/the-reviews/review-shreddage-3-virtual-guitar-instruments-line-by-impact-soundworks/)).
Third, the giveaways players list are performance details rather than tone:
"an unrealistically-sequenced or too-loud slide", missing velocity variation,
palm mutes that trigger only in a velocity band, and slides that "sound a bit
awkward"
([VI-Control](https://vi-control.net/community/threads/virtual-guitars-on-commercial-recordings.83535/)).

## Where Electry actually stands

Electry is already ahead of every product above on the parts of the instrument
below the strings: dual-polarisation waveguides with a fitted stiff-string
dispersion cascade, decay targets solved per string and per fret against
reference recordings, a published pickup signal structure with a finite
magnetic aperture and induced EMF, loss-only modal bridge conductance, real
bridge-coupled sympathetic strings rather than a resonator bank, and a
4x-oversampled amplifier with a measured alias floor. Nothing in the commercial
field documents that much of its string physics, and no sample library can
produce a genuinely bridge-coupled ring at all.

It is behind on the player. Specifically:

1. **The fretting hand does not exist.** `chooseString()` picks "the free
   string that plays the note at the lowest fret". That is a good model of
   first position and of nothing else, because it has no memory: the same pitch
   is always fingered at the same place regardless of what the hand was doing a
   moment ago. Measured on the descending lead phrase B4 A4 G4 E4 D4, the rule
   places the first two notes at the seventh and fifth frets of the top string
   and then falls off the position - G4 at the third fret, **E4 on the open
   string**, D4 at the third fret of the B string - where a hand at the fifth
   position stays inside the pentatonic box on the B and G strings. An open
   string differs from a fretted one in timbre, in decay, in dead-spot
   behaviour and in whether the fretting hand is touching it at all, and it can
   be neither bent nor vibratoed. The wider consequence is that most of the
   fretboard is unreachable: the model documents that fretting up the neck
   moves the pluck comb toward mid-string and changes the inharmonicity, and
   then never gets there. Every competitor in the field exposes a hand
   position, a fret reach and a fretting mode; Electry has none of the three.

2. **No slide.** Hammer-on/pull-off retargets a sounding loop over about 10 ms.
   There is no articulation in which the finger stays in contact and travels,
   which is the single most-cited legato feature in the sampled field
   ("Realtime Legato Slide", "portamento") and the one a physical model should
   win most easily, because a slide is a continuously moving delay length plus
   friction noise, not a crossfade between two recordings.

3. **No pinch harmonic.** The Harmonics play style transposes the note up an
   octave and brightens the excitation. It is not a touch model: the loop is
   retuned to the harmonic's frequency, so the string runs with the
   inharmonicity, decay and pickup-comb geometry of a string an octave shorter
   rather than of the string that is actually vibrating. Nothing in the model
   corresponds to a finger or a thumb resting at a node, so the signature metal
   articulation - the picking hand's thumb catching the string right after the
   pick and selecting a high partial - cannot be produced at all.

4. **No fretting-hand vibrato, and no single-string bend.** The pitch wheel is
   a vibrato *bar*: it stretches all eight strings, sympathetically ringing
   ones included, each by its own compliance. That is a good model of a bar and
   a bad model of the far more common gesture, a finger bending one string.
   There is no vibrato of any kind. Both are on every competitor's articulation
   list; "fingered vibrato" is an explicit Shreddage 3 articulation.

5. **The amplifier has no power stage.** The research contract already says so:
   "there is no power-supply sag or output-transformer model". Two cascaded
   preamp triode ceilings into a filter cabinet is the front half of an
   amplifier. Sag - plate voltage falling from 350 V to around 250 V within
   100 ms of a loud passage and recovering over 300-600 ms
   - and output-transformer core saturation, which compresses and thickens the
   low end specifically because a core's flux limit is a volt-second limit, are
   exactly the two mechanisms that make a real amplifier respond to how hard the
   part is played
   ([Hughes & Kettner on sag](https://hughes-and-kettner.com/news/blog-of-tone-tube-amp-sag-and-preamp-vs-power-amp-distortion-explained/);
   Pakarinen and Yeh's review, already cited by the research contract, surveys
   the modelling side). Their absence is a large part of what "too clean" means
   when reviewers say it about a modelled instrument.

6. **No dead notes.** A fully damped percussive stroke is a rhythm-guitar
   staple and appears on every competitor's articulation list as "muted choke"
   or "dead note". Electry's Palm Mute at full pressure still sounds a pitch.

Three further gaps are named and deliberately not addressed here, with reasons:
double/quad tracking (an arrangement effect, not physics, and reachable by
running two instances), a chord recogniser (a musical-intent layer that belongs
above the instrument, and one whose absence the hand model in step 1 largely
covers), and per-partial capture-fitted decay maps (needs licensed captures the
repository cannot ship).

## Steps

Each step states what changes, which gap it closes, and how it is verified. All
verification is by a test in `Tests/` that fails without the change.

- [x] **1. A fretting hand with a position and a reach.** Replace the
  lowest-fret string chooser with one that models a hand: a floating position,
  a four-fret reach that the index finger anchors, open strings always available
  to the fretting-free hand, and a cost that trades fret distance from the hand
  against string preference. The hand follows the part - it is pulled toward
  whatever the last few notes needed - so an open-position figure keeps its open
  strings and a barre figure at the fifth fret keeps its fretted ones.
  *Closes gap 1.* Verified by `testFrettingHandPosition`: the open C shape must
  still map to exactly the strings it maps to today with the hand left at the
  nut; the descending lead phrase above must stay inside one position, placing
  E4 at the fifth fret of the B string rather than on the open top string; and
  the hand must relax to the nut once the phrase ends, so the same E4 is open
  again.

- [x] **2. Harmonics become a real node touch.** Replace the octave transpose
  with a point-touch loss inside the loop: a one-tap FIR `(1 - d/2) + (d/2)z^-M`
  at `M = p * period`, whose magnitude is exactly the `sin^2(n*pi*p)`
  mode-shape weighting of a light finger at fractional position `p`. At
  `p = 1/2` the filter is exactly unity in magnitude *and* phase at every even
  partial and `1 - d` at every odd one, so the octave arises from the physics
  instead of from a transposition, and the string keeps its own length,
  inharmonicity, decay targets and pickup comb.
  *Closes half of gap 3.* Verified by `testTouchHarmonics`: the sounding
  partial for touches at 1/2, 1/3 and 1/4 of the sounding length is the second,
  third and fourth partial of the *unretuned* string; the fundamental is
  suppressed by a stated margin; the touch filter's magnitude never exceeds one
  at any frequency; and a zero touch depth is bit-identical to no touch at all.

- [x] **3. Pinch harmonic as a fifth play style.** The picking hand's thumb
  catches the string a fraction of a millisecond after the pick, at the pick's
  own position. Reuse the step-2 touch at `p = pick position`, so which partial
  squeals follows the Pick Position control exactly as it does on a real
  guitar, and let the touch decay so the squeal blooms and then the string
  returns to normal. Composes with all three pick strokes and with the
  continuous palm mute, as it does in the hand.
  *Closes the rest of gap 3.* Verified by `testPinchHarmonic`: the dominant
  partial is high, moves down the series as the pick position moves toward the
  neck, and the articulation is measurably distinct from both Sustain and
  Harmonics.

- [x] **4. Slide as a sixth play style.** The finger stays down and travels:
  the sounding length glides at a constant fret-per-second finger velocity
  rather than over a fixed time, so a two-fret slide is short and a
  twelve-fret slide is long, and the loop state is preserved throughout. While
  the finger is moving it crosses fret wires, and on a wound string the winding
  ridges scrape over each one; the crossing rate is the finger velocity in
  frets per second, and the noise is band-shaped per string exactly as the
  existing contact noise is, following the virtual slide guitar literature
  already cited by the research contract.
  *Closes gap 2.* Verified by `testSlideArticulation`: pitch travels
  continuously through the intermediate semitones rather than jumping; the
  travel time scales with the interval; the scrape is present on wound strings
  and far quieter on plain ones; and the slide is silent when Pick Noise is at
  zero.

- [x] **5. Fretting-hand vibrato on channel aftertouch.** Aftertouch is the
  finger's pressure on the string it is already holding. It drives a vibrato
  that is (a) applied only to fingered strings, never to the sympathetically
  ringing ones - which is exactly what distinguishes a finger from the bar the
  wheel already models; (b) upward-biased, because a fretting hand can raise a
  string's tension and cannot lower it below the fret; (c) delayed in onset,
  because a player lands the note before starting the vibrato; and (d) routed
  through the same delay-target machinery as the bend, so it modulates the
  string's tension rather than being a pitch LFO on the output.
  *Closes gap 4.* Verified by `testFrettingHandVibrato`: the sounding pitch
  oscillates at the modelled rate with the stated depth, its mean is sharp of
  the fretted pitch rather than centred on it, a bridge-coupled open string is
  unaffected while the wheel does affect it, zero aftertouch is a bit-exact
  no-op, and the onset delay is measurable.

- [x] **6. Amplifier power stage: supply sag and output transformer.** Add the
  back half of the amplifier inside the existing oversampled domain. Sag: a
  rectified follower on the power-stage drive lowers the stage's headroom with
  a fast attack and a 400 ms recovery, so a hard chug ducks and blooms and a
  quiet passage does not. Output transformer: a bounded flux integrator whose
  saturation is level-over-frequency, so the low end saturates first exactly as
  a core does, plus the primary-inductance low roll-off and leakage-inductance
  high roll-off that give a real transformer its bandwidth.
  *Closes gap 5.* Verified in `ElectryFxTests`: a loud sustained passage
  measurably ducks and recovers on the stated time constants while a quiet one
  does not; the transformer's harmonic generation is larger at 80 Hz than at
  1 kHz for the same input level; the alias floor stays under the existing
  -60 dB bound; and the bit-exact dry bypass at zero amp is unchanged.

- [x] **7. Dead notes as a seventh play style.** A fully damped percussive
  stroke: the fretting hand rests on the strings without stopping them, so the
  pick produces the attack and the noise of a real stroke with no sounding
  pitch. Modelled as the existing bridge-hand absorber taken past the point
  where a pitch survives, plus the fretting hand's own broadband contact, not
  as a gate on the output.
  *Closes gap 6.* Verified by `testDeadNote`: no partial of the fretted pitch
  survives above a stated floor after the attack window; the attack itself is
  as loud as a picked note; and the note is over inside 150 ms.

- [x] **8. Documentation and demonstration audio.** `README.md` and
  `Docs/physical-modeling-research.md` updated for every behaviour above, with
  the new claims placed in the claims-boundary table and the new references
  cited. Demonstration takes added for the new articulations.

## Outcome

All eight steps landed. Four things measured during the work bound what the
model can now claim, and belong here rather than in a commit message.

**The gap analysis's first example was wrong, and is corrected above.** The
original draft claimed a G barre chord at the third fret came out as open
strings. It does not: on this instrument that allocation is the open G shape,
which is a perfectly good voicing. The real defect is that the lowest-fret rule
has no memory at all, and the corrected example - the descending lead phrase
that drops onto an open string in the middle of a fifth-position line - was
measured on the shipping engine before the fix and is the case the regression
test now pins.

**The touch harmonic is exact only at exact node positions.** At `p = 1/k` the
touch filter is unity in magnitude and phase at every surviving partial, so the
harmonic series above the node is untouched - that is the whole reason this
implementation beats a transpose. At an arbitrary `p`, which is what the pinch
harmonic uses, no partial is perfectly preserved and the surviving one carries a
little extra loss and phase. That is physically right, but it means the pinch
harmonic's pitch is a function of the Pick Position control and is not
equal-tempered. It is left that way rather than quantised to the nearest node.

**Two effects had to be measured at the stage rather than at the output, and
both for the same reason.** The slide's friction band and the output
transformer's core saturation are both shaped so heavily by what follows them -
the loaded pickup coil in one case, the cabinet's box high-pass in the other -
that an output-side measurement measures the filter instead of the effect. Both
are asserted at the stage, and the reason is written into the research contract
next to each.

**Cost.** Measured with the suite's own `testCpuGuardrail`, best of five runs on
the same machine, against the sources at the plan commit: the default
Bridge + Mono eight-string render moved from 0.179x to 0.189x realtime and the
worst-case Both + Stereo from 0.220x to 0.221x. Run-to-run spread on this
machine is around 10%, so the honest statement is that the worst case is inside
the noise and the default configuration costs a few per cent. The per-sample
additions are three float comparisons per voice - the node touch, its decay, and
the slide's friction - all of which are false for an ordinary note, and moving
the new per-voice fields to the end of the voice struct rather than into the
middle of it was worth about half of what they cost. The vibrato adds nothing
per sample; it re-crosses the dispersion fit's 6-cent quantum a few times per
cycle, which is the same path a pitch-wheel glide already takes and cheaper. The
amplifier's power stage is two one-poles, a follower and one bounded
nonlinearity inside a block that is skipped outright when the amp control is at
zero, and its measured cost is the alias floor rather than the clock: the
amplifier at full drive moves from -86 dB to -69 dB against a -60 dB bound.
