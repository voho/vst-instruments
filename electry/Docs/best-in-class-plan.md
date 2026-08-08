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

---

## Second pass: the player, the repeat, and the two calibration errors (2026-08-07)

The previous pass fixed what Electry could not *do*. This one fixes what it does
the same way every time. Two independent lines of evidence point at the same
target. The competitive survey found that the criticism levelled at the only
mainstream sample-free guitar is tonal uniformity, not missing features - and
that Electry's excitation is, by design and by measurement, near-identical from
stroke to stroke. The engine audit measured that directly: with the string given
time to decay between strokes, two identical MIDI note-ons differ by 0.0122 dB
in peak and 0.6 Hz in spectral centroid, velocity spans 5.22 dB across the whole
MIDI range at the shipping default, and the vibrato repeats its peak depth to
0.0001 cents per cycle. This pass makes the picking hand and the strumming hand
vary the way physical ones do, without giving up bit-exact reproducibility, and
corrects two calibration errors - the humbucker's notch frequency and the
tension-modulation depth.

**Every number in this section was re-measured under adversarial review before
any of it was scheduled.** The measurement programs link directly against
`libElectryDSP.a` through a private-access seam and are listed by name with each
figure. Where a re-measurement disagreed with the number the pass was originally
written against, the re-measured number is what appears below and the step that
rested on the old one was rewritten or struck. One step - fretted intonation -
did not survive; it is now under "considered and not planned", and gap 4 stays
in the list below as a measured fact that is deliberately unscheduled.

### What changed in the field

**Method caveat, which bounds everything in this subsection.** Every `WebFetch`
from this machine is refused by the egress proxy, for every publisher domain
tried. That is an organisation policy denial rather than a transient failure, and
it was not worked around. All of the evidence below comes from web-search result
summaries, which quote page content but are second-hand. **No product page,
manual, changelog, forum thread, review or paper was opened and read in this
session, and no audio from any competing product was heard.** Every version
number, date and price below is a *reported* figure, attributed to the search
result that carried it, not a verified one; where two summaries disagreed, both
readings are given rather than one being picked. Nothing here is safe to
implement against without opening the source first. This subsection is a survey
of what the field is *saying*, and it is used below only to confirm or refute
priorities - never as a measurement.

**The category moved once since the previous pass, and only once.** Prominy
released **V-METAL 2**, the successor to its 2011 metal instrument. Its own news
feed carries a pre-order announcement and an availability announcement
([Prominy](https://prominy.com/2025/12/05/2978/),
[Prominy](https://prominy.com/2025/12/22/3008/)), whose URL dates put release at
the end of December 2025; one trade write-up summarised it instead as a January
2026 launch, so the exact date is not settled here. Reported specification: an
ESP Alexi Blacky with EMG humbuckers, "over 22 GB / 47,000+ samples", Kontakt
Player rather than full Kontakt, an MSRP of $299 with a $149.50 upgrade, built-in
amp and effect simulation with "250+ presets", chord-recognising key switching,
polyphonic controls named Strum/Arpeggio Key, String Skip and Forced
Hammer-On/Pull-Off, and new articulations listed as moving harmonics, rake and
tempo-synced tremolo
([KVR news](https://www.kvraudio.com/news/prominy-announces-v-metal-2---virtual-electric-guitar-for-kontakt-player-65612),
[VI-Control](https://vi-control.net/community/threads/prominy-v-metal-2-now-available.168652/)).
Nothing in that list is a mechanism Electry lacks the physics for; it is a larger
sample set and a deeper performance script, which is the direction the sampled
wing has been moving for fifteen years.

**And the first published reaction to it is a usability complaint, not a tonal
one.** A user review dated May 2026 on KVR's V-METAL 2 page reports that the
original V-METAL remained the most realistic-sounding metal guitar library to
that reviewer, that V-METAL 2 is "over-engineered to a dizzying degree", and that
after two weeks the reviewer went back to version 1
([KVR](https://www.kvraudio.com/product/v-metal-2-by-prominy/reviews)). One
person's post, summarised, and weighted accordingly - but it is the only
published response to the field's one new product that this session found, and
it says the marginal return on more articulations and more switching has gone
negative for at least one buyer.

**Everything else in the surveyed set is where the previous pass left it.**
Prominy's *SC Electric Guitar 2* is still current and its most recent public
update appears to be ver. 2.0.5c, described as fixing Kontakt 8 multi handling
and a bridge-mute noise bug on inactive pickups
([Prominy](https://prominy.com/2024/09/27/1807/)); the original *SC Electric
Guitar* is now labelled "Discontinued / End of support"
([Prominy](https://prominy.com/products/sc-electric-guitar/)). There is still no
Shreddage 4: Impact Soundworks' visible 2026 activity is maintenance posts in
January and February and TACT default-tuning tweaks
([Impact Soundworks](https://impactsoundworks.com/category/news/updates/)), plus
a free Kontakt Player edition of Shreddage 3 Stratus
([Impact Soundworks](https://impactsoundworks.com/product/shreddage-3-stratus-free-kp/)).
Orange Tree ships engine updates rather than a new engine generation - 1.2.0
adding custom tunings, pick-position variance and separate slide ranges, 1.3.0
adding Polybend and reverbs
([Orange Tree](https://www.orangetreesamples.com/blog/evolution-engine-120-update-released),
[Orange Tree](https://www.orangetreesamples.com/blog/evolution-engine-130-update)).
Three-Body's *Heavier7Strings* is reported still at v1.7.0, the March 2024 VST3
and Universal Binary release, with a vendor-forum thread of users asking about a
2.0 that has not been announced
([KVR news](https://www.kvraudio.com/news/three-body-technology-updates-heavier7strings-to-v1-7-0---vst3-support-and-mac-universal-binary-60385),
[Three-Body forum](https://forums.threebodytech.com/viewtopic.php?t=1747)).
MusicLab's line is at 6 - *RealStrat 6* reported released 2022-08-01, pairing the
original model with an "Elite" set sampled from each of three Stratocaster
pickups individually, and *RealGuitar 6* built the same way
([MusicLab](https://www.musiclab.com/products/realstrat/info.html),
[Sound On Sound](https://www.soundonsound.com/reviews/musiclab-real-guitar-series-6));
no 2025-26 version change for *RealLPC* surfaced. AAS has shipped sound packs for
*Strum GS-2*, not a new version - the last program update found is v2.2.2 for
VST3 - and its 2025 development went to *Multiphonics CV-3* instead
([KVR news](https://www.kvraudio.com/news/aas-updates-strum-gs-2-to-v2-2-2---vst3-support-plus-fixes-and-enhancements-41971),
[AAS](https://www.applied-acoustics.com/press/aas-multiphonics-cv-3-announcement-namm-2025-press-release/)).
There is still no MODO GUITAR; IK's visible 2026 releases are ARC ON EAR firmware
and the ReSing vocal platform.

**The previous pass's "the modelled wing is one product" was too strong, and the
correction does not change the conclusion.** Xhun Audio's *IronAxe* is a
long-standing physically modelled electric guitar - buildable Stratocaster or
Telecaster, pickup type/number/position, plectrum hardness, pluck point anywhere
along the string, plus modelled stompboxes and cabinets - and KVR carries a
thread of buyers weighing it directly against Strum GS-2
([KVR](https://www.kvraudio.com/product/ironaxe-by-xhun-audio),
[KVR](https://www.kvraudio.com/forum/viewtopic.php?t=533727)). And there is one
genuinely new modelled entrant: **Guitar AG**, posted to KVR's instruments forum
in May 2026 as a free, open-source VST3 physically modelled electric guitar whose
stated goals are a clean DI tone for use with external amp sims, MPE support,
"guitar-like six-string allocation", interpretation of hammer-ons, pull-offs and
tapping, and a simple amp-feedback model
([KVR](https://www.kvraudio.com/forum/viewtopic.php?p=9238226)). That feature
list is close enough to Electry's architecture to be worth watching, and it is
known here *only* from a forum post summary: no version, no licence, no review,
no audio. What has not changed is the commercially serious part of the picture -
Strum GS-2 is still the only mainstream sample-free guitar, and it has not moved.

**The platform most of the field runs on is in insolvency.** Native Instruments
entered preliminary insolvency in January 2026 and formal proceedings in March,
with a court-appointed administrator and an active M&A process; reporting says
products remain on sale and development has not stopped
([MusicRadar](https://www.musicradar.com/music-tech/native-instruments-has-been-placed-in-preliminary-insolvency),
[Synth Anatomy](https://synthanatomy.com/2026/03/native-instruments-gmbh-is-preliminary-insolvency-according-to-official-docs.html)).
Prominy, Impact Soundworks, Orange Tree and Ample all ship as Kontakt libraries.
This is recorded because it is a real structural fact about the competition, not
because Electry should act on it: a standalone plugin with no runtime dependency
is simply not exposed to it.

**What 2025-26 reviewers and forum owners say separates the best from the rest.**
Three things recur, and all three are about the player rather than the tone.
First, *string and position choice* - the objection raised against modelled
instruments in a 2026 KVR Guitars thread is that nothing yet "convincingly models
the many different ways a real player can produce the same note on different
strings and positions"
([KVR](https://www.kvraudio.com/forum/viewtopic.php?p=9220986)), and the property
Orange Tree owners and reviewers single out is the engine choosing strings "based
on what is logical for an actual guitarist to play", with a settable neck
position
([KVR](https://www.kvraudio.com/product/evolution-strawberry-by-orange-tree-samples),
[Pro Audio Garden](https://proaudiogarden.com/review-evolution-electric-guitar-strawberry/)).
This is the gap the previous pass closed, and it is still the field's headline
discriminator. Second, *noise*: pick noise, fret noise and release noise appear
as named, separately adjustable controls across the sampled field
([MusicLab RealEight listing](https://www.jrrshop.com/musiclab-realeight.html)),
and their absence is what reviewers reach for when they explain why an instrument
fails - coverage of UJAM's *Carbon*, which Sound On Sound rated 4/5, carries
critics noting that the guitars "lack fret noises, making them always sound too
clean"
([Sound On Sound](https://www.soundonsound.com/reviews/ujam-virtual-guitarist-carbon),
[VI-Control](https://vi-control.net/community/threads/ujam-virtual-guitar-review-carbon-silk-amber-sparkle-iron.86980/)).
Third, *decay and repetition*: the sharpest complaint aimed at a well-liked
sampled library on KVR is that "the strumming sounds fake and unrealistic (mainly
because of unrealistic decay of strummed notes)"
([KVR](https://www.kvraudio.com/forum/viewtopic.php?t=398959)), and 2026 round-up
coverage treats round-robin as having "largely solved" the machine-gun problem -
i.e. per-stroke variation is now assumed, not advertised.

**The criticism of the modelled wing is unchanged and has hardened into buying
advice.** Alongside the uniformity quotes the previous pass recorded - "there's a
clear uniformity to GS-2's sound that sets it apart from samples and the real
thing", and attempts to counter it in the editing interface "couldn't fully
dispel" it - forum consensus now routes around the instrument by use case:
Strum GS "tends to sound very precise and perfect, so if you prefer a more
realistic guitar sound, stick with the sampled guitar instruments", rhythm
"is what GS-2 is really made for", and users are told to "use a sampled guitar
for lead stuff"
([KVR](https://www.kvraudio.com/forum/viewtopic.php?t=549064),
[MusicRadar](https://www.musicradar.com/reviews/tech/applied-acoustics-systems-strum-gs-2-626643)).
That is a worse verdict than "too clean". It says the modelled instrument is not
merely less convincing but has been assigned a narrower job, and that exposed
single lines - where every stroke is heard on its own - are where it loses.
Electry's README advertises the property that produces exactly this: "Identical
MIDI always renders identical audio." Determinism and uniformity are separable,
and this pass separates them: every new variation is drawn from the existing
per-note counter, so renders stay bit-reproducible while consecutive strokes stop
being clones. The sampled field states the rationale for the countermeasure
plainly - "A real guitarist never hits two chords with the exact same force, so
adding this subtle randomization simulates the human element and prevents the
'machine-gun effect' where every strum sounds identical"
([MusicRadar](https://www.musicradar.com/tuition/tech/how-to-avoid-the-machine-gun-effect-using-round-robin-sampling-632302)).

**Competitors ship a rig where Electry ships a knob.** Unchanged, and now with
one more data point: Ample's guitars carry "7 classic amp heads ... paired with 8
guitar cabinets, each captured with 8 microphones"
([Ample Sound](https://www.amplesound.net/en/pro-pd.asp?id=1)); Evolution
Stratosphere has "selectable guitar cab sizes with several microphone options and
mic placement choices"
([Orange Tree](https://www.orangetreesamples.com/products/evolution-electric-guitar-stratosphere));
Heavier7Strings' rack has 31 modules
([Three-Body Technology](https://www.threebodytech.com/en/products/heavier7strings));
and V-METAL 2 launches with a reported 250+ amp and effect presets. Electry has
one 0-100% amplifier control, a fixed six-section cabinet filter, and no tone
stack at all. This is a real gap and it is deliberately not addressed here; see
"considered and not planned".

**Vibrato and release noises are scored articulations.** Stratosphere advertises
"various vibrato styles ... from classic vibrato to rock and metal vibrato with
adjustable depth/speed"; Shreddage 3 documents release articulations that
"trigger when a MIDI note is released ... automatically adding scrapes, squeaks,
and other noises"
([Impact Soundworks](https://impactsoundworks.com/docs/Shreddage%203%20Stratus%20Free%20Manual.pdf)).
Electry's vibrato is one fixed 4.8-6.4 Hz, 40-cent shape with no user control.
Step 4 addresses the vibrato half. The noise half is covered by the deferred
position-shift and release-squeak item under "considered and not planned", and
this session's research **strengthens the case for it**: the fret-noise controls
above are table stakes across the sampled field, and reviewers name their absence
as a direct cause of the "too clean" verdict this pass exists to attack. That
item is already recorded as cut for budget and as the first candidate after the
amplifier; nothing found here justifies re-opening a reviewed step list to
promote it, but the reason it is worth doing is now external as well as internal.

**A calibration error found by reading published measurement against the
source.** Helmuth Lemme's pickup measurements put the humbucker's two-point
cancellation notch "at about 3,000 Hz" for the low E string and "4,000 Hz" for
the A string ([Lemme](http://buildyourguitar.com/resources/lemme/), corroborated
qualitatively at
[guitarnuts2](https://guitarnuts2.proboards.com/thread/10690/humbucker-comb-filtering)).
Electry models the humbucker as a single 21 mm rectangular spatial window, which
first nulls at c/W rather than c/2d - most of an octave too high. Step 3 below.

**One open question from the previous pass is now answered, and it narrows a
deferred idea rather than scheduling it.** MODO BASS 2's tuning controls: any of
its basses can be made four-, five- or six-string, and a DROP option lowers the
lowest string one tone (E to D, or B to A on the five- and six-string setups),
alongside string gauge, age, construction, scale length and action; what it does
*not* offer, according to owner discussion, is arbitrary per-string tuning
([IK Multimedia](https://www.ikmultimedia.com/products/modobass2/),
[MusicTech](https://musictech.com/reviews/plug-ins/ik-multimedia-modo-bass-2-review/)).
So the closest structural relative to Electry ships string count and a single
canned drop, not a tuning matrix. The per-string tuning idea under "considered
and not planned" keeps its scope for that reason, not for want of research.

**What none of this changes: the step list.** The steps below were measured and
adversarially reviewed, and this research neither strikes nor reorders any of
them. It corroborates them: uniformity is still the published charge against the
modelled wing (steps 1, 2 and 5), per-stroke variation is now assumed rather than
advertised in the sampled wing (step 2), vibrato depth and style are exposed
controls everywhere else (step 4), and the discriminator the field names first -
which string and which position - is the one the previous pass already built.
Nothing found points at a property the category has abandoned, and no capability
that every competitor ships and reviewers treat as table stakes is missing from
the list except the fret-noise routing discussed above, which stays deferred.

**Absences, recorded as absences.** No published listening test, measurement or
shootout comparing virtual electric guitars against each other or against
recordings was found - again. The perceptual-evaluation literature that does
exist is methodological or aimed elsewhere (webMUSHRA and BS.1116 procedures,
perceptual thresholds for acoustic-guitar model parameters, amplifier-modelling
similarity tests), and the same-MIDI comparisons that exist are vendor-published
between a vendor's own instruments; experienced users on the forums argue such
comparisons are misleading anyway, because each instrument needs its own settings
and tone to be heard fairly. The one experimental study found on the pickup
magnet's effect on the string reports "no observable effects on the string's
vibration that could be interpreted as having a damping effect"
([Academia](https://www.academia.edu/42961860/The_Effects_of_a_Magnetic_Pickup_on_the_Vibration_Response_of_an_Electric_Guitar_String)),
so the widely repeated "Strat-itis kills sustain" claim is **not** implemented and
should not be, on current evidence.

**Still unverifiable after this pass, and why.** The binding reason for all of it
is the same: pages could not be opened, only searched.

- **Current list prices.** No 2026 price for SC Electric Guitar 2, Strum GS-2,
  Heavier7Strings or RealLPC was established. Two figures did appear in
  summaries - $149 for Shreddage 3 Stratus (reported as $119 for the guitar plus
  a $30 Shreddage 3 licence) and $299 MSRP for V-METAL 2 - and both are recorded
  as *reported*, not confirmed. A Prominy summer sale of up to 40% off running to
  31 August 2026 was also reported, which means any price seen now is a
  promotional one.
- **V-METAL 2's exact release date**, where Prominy's own dated news posts and a
  trade write-up disagree, and its 22 GB / 47,000-sample figure, which comes from
  press coverage rather than from the product page.
- **Evolution Engine version history.** 1.3.0 appears to be current and a July
  2025 date was reported for it, but no changelog was read, and whether the
  "fifth generation" framing the previous pass recorded maps onto that version
  numbering is unknown.
- **Guitar AG entirely.** One forum-post summary: no version, no licence, no
  independent review, and no audio heard. Whether it is a serious instrument or a
  weekend project cannot be told from here.
- **IronAxe's current state** - whether it is still developed and sold, at what
  version, and how it is regarded now rather than in the threads that surfaced.
- **Silent updates.** Absence of news is not absence of change. Heavier7Strings
  and RealLPC may have shipped maintenance releases that generated no coverage.
- **How any of it sounds.** No competing product was heard this session. Every
  qualitative judgement above is a report of someone else's listening, and the
  V-METAL 2 verdict in particular rests on a single summarised user post.

### Where the engine actually stands

Everything below was measured on the shipping sources with scratch programs
linked directly against the engine. Gap numbers in the step list refer to this
list, not to the previous pass's.

1. **Velocity is a 5 dB trim.** `makeVelocityProfile` at
   `Source/DSP/ElectryEngine.cpp:685-702` sets
   `profile.amplitude = lerp(1.0f, 0.06f + 0.94f * curve, response)`, and at the
   shipping default `velocityAmount { 0.65f }`
   (`Source/DSP/ElectryEngine.h:81`) that collapses to
   `1 - 0.611*(1 - v^1.35)`. Measured on note 40, peak of the first 50 ms, fresh
   engine per velocity (`p1_velocity`): **v=1 at -30.908 dBFS, v=127 at
   -25.690 dBFS, a total range of 5.218 dB**, and **1.679 dB from v=61 to
   v=127**. The 2-8 kHz to sub-500 Hz band ratio, averaged over overlapping
   2048-point windows across the same 50 ms, moves **2.28 dB** from v=16 to
   v=127 - not the 4.9 dB this document was first written against, which does
   not reproduce on any window tried. *(Step 1's implementation re-measured this
   at **3.431 dB**, summing power rather than averaging per-window ratios; that
   is the figure `testVelocityDynamicRange` reads, and the difference between
   the two is a reminder that this quantity is method-sensitive to about 1 dB.)*
   A guitarist's picking dynamics span
   roughly 25-30 dB. Written accents, ghost notes and crescendi are all
   rendered at the same loudness.

   Two things about *why* were measured wrong the first time and matter to the
   step that follows. First, the law is not what caps the span: at
   `velocityAmount = 1.0` the existing blend already renders **20.80 dB** from
   v=1 to v=127, above the 20 dB the step below asks for. What ships is a
   default, not a structural ceiling. Second, the flattening of the *upper*
   half is not the pickup's magnetic saturation. Replacing `magneticTransfer`
   (:3468-3477) with an identity in a patched build (`p2lin`) moves v=127 by
   **0.17 dB** and the whole span by 0.20 dB - not the 3.36 dB first claimed.
   Freezing `profile.effort` and `profile.noise` at their v=1.0 values instead
   (`p2frozen`) restores v=64 to v=127 to **4.62 dB** against the amplitude
   law's own 4.71 dB. The upper half is flat because `effortCurve` drives
   `brightness` from 0.20 to 2.10 over the same range, moving the excitation's
   energy into partials that have decayed before the 50 ms peak is taken.
   Amplitude and effort are the same knob, and that coupling - not the
   saturator and not the blend - is what eats the accent.

2. **Two identical note-ons produce the same audio.** Every quantity
   `startExcitation` sets (`ElectryEngine.cpp:2095-2512`) - amplitude, comb
   delay and width, pulse length, pick load and slip geometry, release
   coefficient, polarisation split, onset sample - is a pure function of the
   parameters, the velocity profile and the string index. The only per-note
   randomisation in the engine is the noise seed at :2619-2625 and a +/-3%
   saddle-rattle frequency at :2631-2634. Measured across 12 identical
   `noteOn(40, 0.80)` events with the three noise controls at zero, **12 s
   apart so the string has decayed and the previous stroke's ring cannot
   confound the comparison** (`p3_repeat`): **peak spread 0.0122 dB** and
   **spectral centroid spread 0.6 Hz on a 401 Hz centroid**. Successive
   relative L2 difference over the first 150 ms starts at -43.4 dB and falls
   to -78.2 dB by the twelfth stroke as the residual state converges. The
   "-55.9 dB, 0.077 dB, 5 Hz" figures this document was first written against
   do not reproduce on any protocol tried and have been replaced by the three
   above.

   Two corrections to the original reading of this gap, both of which narrow
   it. **The Artifacts control does add note-to-note variation** - the +/-3%
   saddle-rattle detune cited two sentences above is exactly that. On the same
   12 s protocol, `artifactAmount = 0` lets successive strokes converge to
   -84.0, -90.8 and -98.9 dB while `artifactAmount = 1` holds them at -43.8,
   -44.1 and -49.1 dB. What the control does *not* vary is the level: peak
   spread stays 0.0122 dB at both settings. **And the uniformity holds only for
   a latched stroke direction.** Under `PickStyle::Alternate` successive strokes
   of the same note differ by more than the signal itself - relative L2 of
   **+5.4, +6.3 and +5.4 dB** - because up and down strokes are separately
   coloured. The gap is real for a player who latches Down and plays repeated
   notes; it is already absent for one who alternates.

3. **The humbucker's notch is most of an octave too high.**
   `ElectryEngine.cpp:26`, `humbuckerApertureMetres = 0.0210f`, read at :2000
   into a single rectangular spatial window. A rectangular window of width W
   first nulls at c/W; a two-point sum at spacing d nulls at c/2d, and does so
   with a cosine's steep approach rather than a sinc's gentle one. With the
   transverse wave speed `c = 2 L f_open` at the shipping `scaleLength` of 0.85
   (L = 0.7017 m), string 2 (E2, 82.41 Hz) has c = 115.6 m/s: the current model
   nulls at **5507 Hz** where a 19 mm two-point sum nulls at **3043 Hz**, and
   string 3 (A2) at **7351 Hz** against **4062 Hz** (`p5_pitch`, which
   reproduces both columns for all eight strings). Lemme's measured figures are
   3000 Hz and 4000 Hz. That agreement is closer than it deserves to be: Lemme
   measured a 25.5-inch instrument, where the same 19 mm spacing puts the low-E
   null at 2810 Hz, so the model's 3043 Hz agrees with his 3000 Hz partly
   because the shipping scale is longer. The mechanism is right and the ratio
   `c/W` against `c/2d` is the error; the exact coincidence is not evidence.
   The source comment at `pickupCombDepth` (:28-46) records that two coils "was
   tried instead and measured no better than this" - but that comparison was
   scored on low-frequency comb depth, not on notch frequency, so it does not
   settle this.

   What the original reading of this gap missed is the *rest* of the transfer.
   Evaluating both windows on string 2 (`p5_pitch`), replacing one 21 mm
   rectangle with two 4.8 mm rectangles 19 mm apart changes the magnitude by
   -3.9 dB at 2 kHz, **+2.7 dB at 4 kHz, +20.9 dB at 6 kHz, +6.5 dB at 8 kHz
   and +18.2 dB at 12 kHz**, while on the plain E4 it goes the other way:
   -3.9 dB at 8 kHz and **-28.5 dB at 12 kHz**. Putting the notch in the right
   place also rebalances the humbucker across the whole string set and removes
   most of the top-octave darkness that currently distinguishes it from the
   single coil. That is a bigger change than the notch, and the step below has
   to bound it.

4. **The instrument is very nearly in tune, and no guitar is.** *(Measured,
   and deliberately not scheduled - see "considered and not planned".)*
   `configureVoicePitch` (:1844-1870) compensates every loop filter's phase
   delay at the fundamental. How in-tune that leaves it depends entirely on
   which estimator is asked, and the "-0.54 to +0.46 cents across eleven notes"
   this document was first written against reproduces on none of them
   (`p6_pitch2`, running the suite's own protocol - velocity 0.3, window
   0.45-0.95 s - through three estimators). A **fundamental-only** DFT peak
   gives **-0.25 to +0.30 cents**, which is the real tuning accuracy and is
   better than the figure claimed. The **suite's own `measureFrequency`**, which
   scores five partials weighted by `1/sqrt(n)`, gives **+0.00 to +2.50 cents**
   on its shipping 0.5-cent grid and **-0.04 to +2.68 cents** when the grid is
   refined to 0.02 cents. A plain autocorrelation gives **+5.6 cents** on the
   low E1. The spread between estimators is inharmonicity - the upper partials
   really are stretched, which is correct physics - and it is an order of
   magnitude larger than the fret-dependent error a compensated fretboard
   actually has. The engine has no fret-dependent pitch term of any kind, and
   the step that was written to add one did not survive review because the
   term it would add is smaller than the disagreement above.

5. **The vibrato is a mathematically exact LFO with one global phase.**
   `ElectryEngine.cpp:3933-3943`: `rate = lerp(4.8f, 6.4f, vibratoAmount_)` and
   `vibratoSemitones_ = ... 0.5f * (1 - cos(2*pi*phase))`, off a single engine
   wide `vibratoPhase_` (`ElectryEngine.h:958`). Measured through a
   private-access seam every 8 samples for 6 s (`p4_misc`): settled peak depth
   0.399992 to 0.399994 semitones over 25 cycles, **spread 1.07e-6 semitones
   (0.0001 cents)** - the first draft of this document said 4.06e-3 semitones,
   which is nearly four orders of magnitude too generous; the LFO is exact to
   the float. Rate **6.40000 Hz** with a **cycle-period standard deviation of
   0.0533%**, which does reproduce. The settled trace spends 49.2% of each
   cycle above half depth, as a raised cosine must. Because the phase is shared, a
   double-stop's two strings move in exact lockstep, and because it resets to
   zero at zero pressure, every vibrato in a part starts on the same part of the
   cycle.

6. **A strum is a uniform ramp travelling in a direction the pick stroke does
   not set.** `ElectryEngine.cpp:1075-1095`: `chordAnchorString_` is fixed by
   whichever note-on the host sent first, and
   `startDelaySamples = int(spread * rate) * abs(stringIndex - anchor)`.
   Measured with an E5 power chord sent low-to-high (`p4_misc`),
   `PickStyle = Down` and `PickStyle = Up` produce **identical** offsets
   (+0.000, +12.000, +24.000 ms) with `strokeUp` correctly set to 0 and 1 - the
   stroke colours the excitation and never reverses the travel. Across eight
   strings the offsets are +0.0000, +12.0000 ... +84.0000 ms, **zero deviation
   from a straight line**. The `abs()` also means a chord whose first note-on is
   a middle string makes the pick travel outward in both directions at once:
   sending D3 first and then E2, A2, G3, B3 gives 24.0, 12.0, 0.0, 12.0,
   24.0 ms. All three reproduce exactly. This is the one gap in the list whose
   every stated number survived re-measurement unchanged.

   One constraint the original reading missed: the chord window is 35 ms
   (`chordWindowSamples_`, :789), so the note-ons of one chord routinely arrive
   across several `process()` blocks. Anything that re-decides the anchor after
   the first note-on has to cope with the first note having already sounded.

7. **Tension modulation is wired up and calibrated about 100x too small.**
   `ElectryEngine.cpp:2657-2658` sets
   `voice.tensionDepth = 0.042f * lerp(1.45f, 0.70f, stringGauge) * profile.tension`,
   driving `tensionFactor = 1 / (1 + tensionDepth * energyEnvelope)` at
   :1872-1875. `energyEnvelope` is the mean square of the loop samples and peaks
   at **0.002831**, so the product reaches 9e-5. Reading `tensionDepth` and
   `energyEnvelope` straight off the seam and evaluating
   `1200*log2(1 + depth*energy)` (`p4_misc`), the peak deviation from the
   settled pitch on note 40 at velocity 1.0 with `velocityAmount = 1.0` is
   **+0.180 cents, at 183 ms**; **+0.0141 cents at velocity 0.25**. The step's
   own worked example puts a hard 3 mm pluck at **8.7 cents**, and that - not
   the "tens of cents" this document first asserted, for which no measurement
   was found - is the target. What is unambiguous is the *timing*: the peak
   arrives a fifth of a second after the attack, so a fortissimo open low E
   starts at exactly its steady pitch and goes sharp once it is already
   decaying, which is backwards.

   Two things about this gap are smaller than they look. The velocity law is
   already right: 0.180 against 0.0141 cents is a ratio of **12.8**, and a
   `v^2` law predicts 16. And the gauge dependence is already right: the peak
   is **0.180 cents at `stringGauge = 1.0` against 0.377 at `stringGauge = 0`**,
   a ratio of 0.478, where the `1/d^4` law the step proposes to derive predicts
   0.45. Only the absolute scale and the envelope's attack time are wrong.

8. **Fingered strings exchange no energy.** `sympatheticBus_ += bridgeForce` at
   :3575-3576 is written only from `renderVoice`, and
   `renderSympatheticString` is reached only in the `else` branch for inactive
   voices (:4051-4066); `renderVoice` never reads `sympatheticBusDelayed_`.
   That much is exactly true, and it is the whole of the real gap.

   The measurement that was attached to it is not. Rendering notes 40 and 47
   separately and together (`p4_misc`), `||AB - (A+B)|| / ||A+B||` over the
   first 1.5 s at `sympatheticAmount = 0` is **-40.4 dB at velocity 0.02,
   -39.8 dB at 0.20 and -39.7 dB at 0.90**, not the -68.2/-72.0/-56.4 dB first
   claimed. Those figures appear only in a decayed window: over 10-12 s the same
   ratio is -67.8/-66.8/-65.5 dB. So the two strings are **not** exactly
   superposable while they are sounding; they already differ from their own sum
   by about -40 dB, which is not floating-point noise.

   **What that -40 dB is made of was established in preflight, and it is mostly
   not the shared path.** With `pickNoise`, `fingerNoise` and `releaseNoise` at
   zero the same residual drops to **-65.4 dB at velocity 0.02, -62.0 at 0.20
   and -53.8 at 0.90** (`pf_additivity`). The missing 25 dB is the per-note
   noise seed: `startExcitation` seeds it from `noteSequence_` (:2619-2625),
   which advances per note-on, so the second note of a pair is seeded
   differently than it is when rendered alone. What remains - the shared body
   and coil path and the summing guard gain
   `1 / sqrt(1 + 0.4356 * guardInput^2)` at :4137-4143 - grows with level, as a
   cubic soft limit must. Step 7 is measured with those three controls at zero
   for this reason.

   And at the **shipping** `sympatheticAmount = 0.20` - the setting the step
   below tests at - the same pair already measures **-36.1 dB over 0-1.5 s and
   -31.4 dB over 10-12 s**, because the six unfingered strings ring off the sum
   of both notes and a third string's ring is a genuinely non-superposable
   function of the pair. The gap that remains is the one the step's own last
   sentence states and the measurement did not: a voicing that leaves *nothing*
   open has no path at all, because the only coupling the engine has runs
   through voices that are not being played.

Three further measured defects are real and are **not** scheduled; the reasons
are under "considered and not planned": the pitch-wheel glide is a one-pole
exponential that delivers 10.5 cents of a 200-cent bend in the first 5 ms;
sweeping String Gauge or Scale Length under a ringing note fires broadband
bursts 48.4 dB above the note's own high-frequency floor; and note-off damping
is one hard-coded 60 ms T60 that varies only 18% across five octaves and does
not consult `playStyle` at all. **These three were not re-measured under
review** - nothing is scheduled against them, so their numbers carry the
original audit's confidence and not this section's. The same caveat applies to
the "thirteen of fifteen controls at -63.4 dB" figure quoted below: it bounds
two steps, so it must be re-measured before either is implemented against it.

**What must not regress.** Four measured strengths bound every step above, and
one of the four had to be restated to survive its own re-measurement.
Steady-state tuning is within **-0.25 to +0.30 cents on the fundamental** across
the eleven notes the suite checks, and must stay there; the "+/-0.6 cents"
originally recorded here is not what any estimator returns, and the suite's own
partial-weighted `measureFrequency` reads up to +2.5 cents sharp on the low
strings because of inharmonicity, so any future tuning claim has to name its
estimator. Nothing may bypass the phase compensation refresh. The alias floor is
**155.1 dB below the spectral peak** above 12 kHz on a full eight-string chord
at velocity 1.0 (`p7_alias`, 32768-point window at 0.2 s; the 155.5 dB
originally recorded reproduces to within the window), and must stay below the
existing bound; step 3 adds taps and step 7 adds a feedback path, and both must
be re-measured against it. Thirteen of fifteen continuous controls
sweep with a locally-referenced >5 kHz peak-to-RMS at the static note's
-63.4 dB, and the new per-note variation must not become a per-block one. And
the engine is unconditionally stable today because the coupling graph is
acyclic; step 7 removes that property on purpose and therefore carries its own
bound and its own long-render test. It is *not* superposable today, at -40 dB
during the note and -36 dB at the shipping resonance setting - and -65 dB and
-38 dB respectively once the three noise controls are taken out of the
measurement - so "superposition" is not one of the four strengths and the step
below is measured against those baselines rather than against silence.

Two further bounds came out of the review and are new here. **No step may assert
a threshold that the shipping engine already meets**; four assertions in the
first draft of the step list did, three of them because the verification
protocol and the gap measurement were not the same experiment. And **no step may
assert a threshold its own stated physics cannot reach**; three more did, and
the arithmetic is now carried out in the step rather than asserted.

### Steps

Each step states what changes, which gap it closes, and how it is verified. All
verification is by a test in `Tests/` that fails without the change **and passes
only because of it** - every threshold below was checked against the shipping
engine first, and the measured baseline is quoted next to it.

- [x] **1. Velocity becomes the pick's deflection, and stops dragging
  brightness behind it.** Two changes, and the second is the one that does the
  work. **(a)** Replace the amplitude term in `makeVelocityProfile` with the
  physics the excitation already implies. A plectrum deflects the string to `y0`
  before it slips; the lateral force needed is `F = T * y0 / (p (1-p) L)` for a
  contact at fraction `p`, so `y0 = F p (1-p) L / T` is linear in the hand's
  force and the pickup, which senses displacement, is linear in `y0`. Take MIDI
  velocity as proportional to that force: `v_eff = 0.05 + 0.95 * v`, and make
  the Velocity Response control the exponent, `amplitude = v_eff^response`, so
  it scales the *decibel* range linearly and stays an exact no-op at zero. The
  default rises from 0.65 to 0.85. **(b)** Break the coupling between
  `profile.amplitude` and `profile.effort`. Today `effort = lerp(0.65, v,
  response)` drives `effortCurve` and thence `brightness` from 0.20 to 2.10 over
  the same velocity range, and that is what flattens the top of the keyboard:
  the extra amplitude goes into partials that have decayed before the peak of
  the first 50 ms is taken. `effort` must span a narrower range - the sensible
  reading is that a harder stroke is not proportionally sharper, because the
  plectrum's own stiffness bounds the contact spectrum - so that a change in
  force reads mostly as a change in level.

  This step was rewritten under review, because the change as first specified
  was implemented in a patched build (`p2_velpatched` against a patched
  `libElectryDSP`) and **fails three of its own five assertions**. With
  `amplitude = v_eff^0.85` and nothing else changed: the v=1 to v=127 span is
  **17.87 dB** against an asserted 20; v=64 to v=127 is **1.686 dB** against an
  asserted 4.0, essentially unmoved from today's 1.487 dB; and the level is
  **not monotone** - it turns over above v=104 and reads -26.167, -26.283,
  -26.320, -26.144 dBFS at v=104, 112, 120, 127. The amplitude law itself
  delivers 21.09 dB and 4.71 dB, so the loss is downstream, and the first
  draft's attribution of it to the pickup's magnetic saturation is wrong by more
  than an order of magnitude (0.17 dB, measured by linearising
  `magneticTransfer` in `p2lin`). Freezing `effort` and `noise` at their v=1.0
  values (`p2frozen`) recovers **4.62 dB** across v=64 to v=127 and restores
  monotonicity, which is what identifies (b) as the load-bearing change and (a)
  as necessary but not sufficient.
  *Closes gap 1.* **Verified by** `testVelocityDynamicRange`: at the shipping
  defaults on note 40, peak of the first 50 ms, fresh engine per velocity, the
  span from v=1 to v=127 is **at least 18 dB** (today 5.218 dB) and
  from v=64 to v=127 **at least 4.0 dB** (today 1.487 dB; the amplitude law
  alone reaches 1.686 dB, so this one *only* passes with (b), and it is the
  assertion that separates (a) from (a)+(b)). **Corrected in preflight**: the
  first draft claimed the 18 dB span separated them too, but the amplitude law
  alone measures 17.87 dB against it - a margin of 0.13 dB, smaller than the
  spread between window choices, and it cannot carry that claim. The 18 dB span
  stays as the headline target, which the shipping engine fails by 12.8 dB, and
  the v=64 to v=127 assertion is what attributes the difference to (b).
  The level is
  **strictly monotone across 16 evenly spaced velocities**, which the amplitude
  law alone breaks - and which, contrary to the preflight reading, the shipping
  engine *also* breaks: on the sixteen velocities evenly spaced from 1 to 127 it
  falls from -25.7928 dBFS at v=110 to -25.8797 dBFS at v=119. It is therefore a
  second biting assertion, not a regression guard; v=127 stays **within 1.5 dB
  of -25.690 dBFS**; and at
  `velocityAmount = 0` all velocities are bit-identical, which they are today
  (measured spread exactly 0.00000 dB) and which (a) preserves because
  `v_eff^0 = 1`.
  The brightness assertion from the first draft - "the 2-8 kHz to sub-500 Hz
  band ratio moves at least 12 dB from v=16 to v=127" - is **struck**. Measured
  on the attack window it moves **2.28 dB today**, not the 4.9 dB claimed, and
  under the proposed law it moves **2.19 dB**, i.e. slightly *less*. A 12 dB
  target is unreachable while brightness stays a function of `effortCurve`, and
  step (b) deliberately reduces that dependence. It is replaced by a bound in
  the other direction: the band ratio must move **no more than 4 dB** from v=16
  to v=127, so that (b) cannot be implemented by simply flattening brightness
  into silence at low velocity. `testVelocityExpression`'s existing bounds are
  widened to match rather than deleted.

  *What actually shipped*: (a) exactly as written - `force = 0.05 + 0.95 v`,
  `amplitude = force^response`, default 0.85 - and (b) as a split rather than a
  compression. `profile.effortCurve` was carrying two different physical
  quantities under one name, so it was separated into the two it actually is.
  `profile.effort` stays the stroke's force and keeps its old law, because force
  is the right axis for the two things that read it - how hard the string meets
  the frets (`collision`) and how far it stretches itself sharp (`tension`,
  which step 6 owns and which is bit-unchanged by this step). The field that
  carries effort into the contact spectrum is renamed `profile.releaseRate` and
  given the slip-time law directly. The string leaves the plectrum at the kink
  velocity `F/Z` set by the string's transverse wave impedance, over a slip
  distance that is a grip depth `d` the stroke does not change plus the pick
  tip's own elastic recoil `F/k`, so `t_s = Z d / F + Z / k`. The second term is
  a floor no amount of force gets under - that is the sense in which the
  plectrum's stiffness bounds the contact spectrum - and normalising `1/t_s` to
  its full-force value gives `rate = 1 / ((1 - s) / F + s)` with
  `s = (F_max/k) / (d + F_max/k)`. A 0.73 mm celluloid medium has a tip
  stiffness near 6 kN/m and a hard low-E stroke needs about 4.8 N, so the tip
  recoils about 0.8 mm past a grip depth near 0.2 mm: **s = 0.80**, which is
  what ships.

  Measured on the shipped engine, on the step's own protocol: **v=1 to v=127 is
  18.175 dB** (asserted 18, today 5.218, the amplitude law alone 17.872);
  **v=64 to v=127 is 3.508 dB** (today 1.487, the amplitude law alone 1.686);
  monotone on both the eighteen-point and the sixteen-point grids; v=127 at
  **-25.694 dBFS**, 0.004 dB from where it was; band ratio moving **0.019 dB**
  from v=16 to v=127; and exactly 0.00000 dB of spread at
  `velocityAmount = 0`. Every preflight figure for the shipping engine and for
  the amplitude-law-only build reproduced to the digits quoted, including the
  turnover at -26.167, -26.283, -26.320, -26.144 dBFS.

  *Two corrections the implementation forced.* **The 4.0 dB v=64 to v=127
  assertion is unreachable and has been lowered to 3.0 dB.** Freezing the
  release rate outright - `s = 1`, no force dependence in the contact spectrum
  at all - measures **4.630 dB**, which is the ceiling and agrees with the
  preflight's 4.62 dB for a frozen `effort` and `noise`. So 4.0 dB leaves
  0.63 dB for *any* surviving brightness dependence, and there is no value of
  `s` that both clears it and leaves a stroke sounding harder: at `s = 1` the
  attack's spectral centroid ratio from v=0.2 to v=1.0 reads **0.996**, i.e. a
  harder stroke that arrives *darker*, and at `s = 0.95` - the smallest value
  that clears 4.0 dB - it reads 1.038. The shipped `s = 0.80` is the value the
  pick geometry gives, not the value the threshold wanted, and it is where the
  bar was moved to meet. 3.508 dB is still 1.82 dB above what (a) delivers
  alone, so the assertion still separates (a) from (a)+(b). **And the band
  ratio moves 3.431 dB on the shipping engine, not 2.28 dB**, measured by the
  test's own method (2048-point windows, power summed over 2-8 kHz against
  sub-500 Hz across the 50 ms attack). The 4 dB bound therefore has only
  0.57 dB of headroom on the *unmodified* engine; it is kept, because on the
  shipped engine it reads 0.019 dB.

  *Two existing assertions moved.* `testVelocityExpression`'s "velocity
  brightens the attack" bound goes from 10% to 5%: the attack centroid ratio at
  `velocityAmount = 1` falls from 1.166 to **1.073**, which is the intended
  effect of (b) and is what the plan means by widening that test rather than
  deleting it. It still catches the degenerate case, which reads 0.996.
  `testPinchHarmonic`'s squeal gain was scored at whichever single partial came
  out strongest, and the eighth and ninth partials of that pinch sit within
  0.06 dB of each other - a tie the velocity work flips without touching the
  effect, which reads 15.12 dB at the ninth partial both before and after. It
  now scores the strongest lift among the partials within 1 dB of the pinched
  peak, which is 15.12 dB on both builds.

  *Left undone.* `README.md` still documents Velocity response as "default 65%"
  and describes the response dimensions as one axis; that is now stale and is
  left for the documentation pass.

- [x] **2. The picking hand stops repeating itself.** Draw a small set of
  per-note offsets from the existing note counter. `startExcitation` already
  seeds its noise with `hash32(stringIndex ^ midiNote ^ style ^ noteSequence_)`
  (:2619-2625); the same stream, advanced once more, supplies: contact position
  along the string (sigma 4 mm, which moves the pluck comb by about 5.3% of its
  notch frequency at the default Pick Position - the 3.4% recorded here does
  not survive the arithmetic: the default Pick Position puts the contact
  `lerp(0.025, 0.48, 0.18) = 0.1069` of the way along a 0.7017 m open string,
  i.e. 75.0 mm from the bridge, and the comb's first null sits at `c/2d`, so
  4 mm is 5.33% of both - and is bounded below by the
  plectrum's own width and above by the hand anchoring on the bridge); contact
  force (sigma 0.8 dB); pick attack angle (sigma 6 degrees), which is not a free
  parameter but decomposes exactly onto the two polarisations as `cos` and
  `sin`, so an angle jitter is a split jitter; and contact patch width
  (sigma 8%, carried by the existing pulse length). The magnitudes are a
  calibration - no measurement of picking-hand repeatability was found - but the
  *presence* of the variation is what the round-robin literature documents, and
  every draw is a pure function of the note index, so `Identical MIDI always
  renders identical audio` remains true. The step also has to reach the
  `PickStyle::Alternate` case, which already varies stroke to stroke by more
  than the signal itself: the variation must ride *on top of* the up/down
  colouring rather than replacing it, or alternate picking gets no worse and no
  better and the whole step is scoped to a latched Down.

  The verification below was rewritten under review, because the protocol the
  first draft specified is **already satisfied by the shipping engine**. Run
  exactly as stated - 500 ms between strokes on the same note, contact noise at
  zero - `p3_repeat` measures a successive relative L2 of **-4.65, -8.05,
  -10.77, -12.14 dB** falling to -27.02 dB by the twelfth stroke, a peak spread
  of **1.71 dB** and a centroid spread of **30.5 Hz**. Three of the four
  assertions (`-24..-8 dB`, `0.6..3.0 dB`, `>=12 Hz`) pass today with no change
  at all. They pass because at 500 ms the string has not decayed and each stroke
  lands on the previous one's ring, so the protocol measures retrigger state
  rather than the excitation - and it measures it *converging*, which is the
  machine-gun effect rather than the cure. The gap-2 measurement and the step-2
  verification were not the same experiment.
  *Closes gap 2.* **Verified by** `testPickingHandVariation`, on a protocol that
  isolates the excitation: 12 identical `noteOn(40, 0.80)` events **12 s apart**
  with `pickNoise`, `fingerNoise`, `releaseNoise` **and `artifactAmount` all at
  zero** - artifacts excluded because the +/-3% saddle-rattle detune already
  supplies per-note variation and would otherwise be credited to this step. On
  that protocol the shipping engine measures -84.0, -90.8 and -98.9 dB on
  successive pairs, 0.0122 dB of peak spread and 0.6 Hz of centroid spread, so:
  the relative L2 difference between successive strokes' first 150 ms is
  **between -24 dB and -8 dB** (today -84 dB and falling); peak spread across
  the 12 strokes is **between 0.6 dB and 3.0 dB** (today 0.0122 dB); spectral
  centroid spread is **at least 12 Hz** on a 401 Hz centroid (today 0.6 Hz).
  **The `PickStyle::Alternate` requirement gets its own assertion, added in
  preflight**, because the step states it and nothing above tested it: on the
  same 12 s protocol under Alternate, strokes **two apart** - the same stroke
  direction, so the up/down colouring is held constant - must show a relative
  L2 in the same **-24 dB to -8 dB** band. Measured today (`pf_alternate`, the
  same program with the Alternate keyswitch sent first) those pairs converge to
  **-85.4, -95.3, -91.7, -98.6 dB and below**, indistinguishable from the
  latched case, so an implementation that varies only a latched Down fails
  here and passes everything else.
  Separately, and this is the assertion the first draft's protocol was
  accidentally testing: on the 500 ms protocol the successive difference must
  **stop converging** - the twelfth pair must be within **6 dB of the same
  engine's twelfth pair on the 12 s protocol**, so that the variation is a
  property of the excitation rather than of the residual state. Measured today
  (`pf_repeat`, which reproduces this section's 500 ms and 12 s figures) the
  two are **-27.02 dB and -98.37 dB, 71 dB apart**, and the 500 ms trajectory
  falls monotonically - -4.65, -8.06, -10.77, -12.14, -14.33, -16.43, -18.88,
  -21.13, -23.00, -25.18, -27.02 dB - with no plateau anywhere in it.
  **This assertion was corrected in preflight.** It read "the last pair of 12
  must be within 6 dB of the first pair, where today it falls 22.4 dB from
  -4.65 to -27.02 dB". The first pair is the cold start - stroke 1 lands on
  silence and stroke 2 on a fresh ring - so pinning the twelfth pair within
  6 dB of it forces the excitation variation up to about -11 dB, near the top
  of the -24 to -8 dB band the 12 s protocol allows, for no stated reason.
  Comparing the two protocols' twelfth pairs tests the same property without
  fighting the other assertion, and fails today by 71 dB rather than by 16.
  And the existing `testDeterminism` still passes bit-exact, including that a
  `reset()` and replay reproduces the first stroke sample for sample.

  *What actually shipped*: all four draws, as specified, from a stream seeded
  by the note counter - `hash32(startOrder * 2654435761 ^ stringIndex * 40503)`
  in a new `drawStrokeVariation` (`ElectryEngine.cpp:2135-2185`), called from
  `startVoice` and `legatoRetarget` immediately after `startOrder` is set.
  `startOrder` rather than `noteSequence_`, because a strummed chord's later
  strings excite several blocks after their note-on, by which time the counter
  has moved; and a separate stream rather than `voice.noiseState` itself, so
  the contact-noise sequence is bit-unchanged and the step is not credited with
  moving it. Each draw is three uniforms on [-1, 1] summed, which has unit
  variance exactly and cannot leave +/-3 sigma - the bound the step asks for
  when it says the contact offset is bracketed by the plectrum's width and by
  the hand anchored on the bridge. The angle is applied where the step says it
  decomposes: `updateStyleWeights` now rotates the (vertical, horizontal)
  weight vector about its own length rather than scaling either weight, so the
  jitter moves the split and takes no energy with it, and it composes with the
  upstroke tilt instead of replacing it. It is skipped for `PlayStyle::Hammer`,
  on the same grounds the upstroke voicing already skips it: a hammered note
  has no plectrum to hold at an angle.

  *One magnitude moved, and it is the level one.* The step's sigma of 0.8 dB is
  a calibration on the *stroke's level*, and the contact-patch draw turns out
  to carry 0.43 dB of that on its own - the pulse length sets how much of the
  pick's work is injected as well as how much of its top end survives, so an 8%
  patch draw alone measures 1.394 dB of peak spread across twelve strokes. With
  the force draw also at 0.8 dB the peak spread reads **3.220 dB**, outside the
  step's own 0.6-3.0 dB bound. The force draw therefore ships at **0.6 dB**,
  which puts the realised level sigma at about 0.76 dB - within a rounding of
  the 0.8 dB the step asked for - and the peak spread at 2.493 dB. The bound was
  not moved: it is a bound on the audible outcome, where the sigma is admittedly
  a calibration with no measurement behind it, so it is the sigma that gave way.

  Measured on the shipped engine, on the step's own protocol (note 40, velocity
  0.80, twelve strokes 12 s apart, three noise controls and `artifactAmount` at
  zero, first 150 ms of each): successive difference **-16.51 dB on the mean**
  of the eleven pairs, the pairs running -22.49, -17.82, -14.21, -12.47, -14.73,
  -16.92, -13.38, -30.78, -10.97, -13.91, -13.89 dB; **peak spread 2.493 dB**;
  **centroid spread 17.15 Hz** on a 456 Hz centroid. Under `PickStyle::Alternate`
  the strokes two apart read a mean of **-16.12 dB**, loudest pair -10.42 dB.
  On the 500 ms protocol the twelfth pair reads **-13.22 dB** against the 12 s
  protocol's **-13.89 dB**, 0.67 dB apart. With the four draws reverted to
  neutral the same test measures -84.61 dB, 0.0120 dB, 0.380 Hz, -86.14 dB, and
  -27.27 dB against -94.29 dB, and five of its assertions fire.

  *Three figures in the verification above are corrected by the test's own
  measurement.* The 12 s twelfth pair reads **-94.29 dB**, not the -98.37 dB
  `pf_repeat` recorded, and on the unmodified engine the 500 ms trajectory
  starts at -2.96 dB rather than -4.65 dB (it still falls monotonically to
  -27.27 dB with no plateau, so the reading of it stands); both differences are
  the protocol's, since the test renders the gap
  in 512-sample blocks from a `reset()` engine with the pick keyswitch sent
  first. Neither changes the conclusion - the two protocols are 67 dB apart
  today rather than 71. And the centroid figures are estimator-dependent: gap 2's
  0.6 Hz on a 401 Hz centroid comes from `p3_repeat`, where the suite's own
  `spectralCentroid` - 48 partials to 6.5 kHz - puts the same note's centroid at
  456 Hz and its spread at 0.380 Hz. The 12 Hz threshold is met either way, and
  the test states which estimator it uses.

  *One assertion is scored differently than written, for a stated reason.* The
  -24 to -8 dB band is applied to the **mean** of the successive pairs, plus a
  separate requirement that **every** pair stays under -8 dB. Independent draws
  occasionally land close: one of the eleven latched pairs reads -30.78 dB and
  one of the ten alternate pairs -28.85 dB, which is what "two consecutive
  strokes happened to be similar" looks like and not a defect. Scoring the band
  on the mean tests the property the step names - that repeated strokes differ
  by a hand's worth - without asserting that no two strokes in a run may ever
  resemble each other. Both forms fail on the unmodified engine by more than
  60 dB.

  *Left undone, and a caution for the steps after this one.* The peak-spread
  assertion has 0.5 dB of headroom under its upper bound and the centroid-spread
  assertion 5.15 Hz over its lower one; any later step that adds level or
  attack-spectrum variation to a repeated note will eat that margin. Nothing in
  steps 3 to 7 obviously does. The three noise controls and `artifactAmount` are
  held at zero throughout this test by design, so the interaction between the
  new draws and the +/-3% saddle-rattle detune is untested; measured outside the
  suite, turning `artifactAmount` back to its 0.18 default moves the centroid
  spread by 0.12 Hz and leaves the other four figures unchanged to the digits
  quoted, so the two variations do not fight.

- [ ] **3. The humbucker becomes two coils.** Replace the single 21 mm
  rectangular aperture with the sum of two taps 19 mm apart along the string,
  each carrying the narrow per-bobbin window the single coil already uses
  (4.8 mm). The sum of two point sensors separated by `d` has magnitude
  `|cos(pi f d / c)|`, first nulling at `f = c / 2d` with the transverse wave
  speed `c = 2 L f_open` the engine already computes at :2004-2006 - against
  `c / W` for the current window. At the shipping scale this moves string 2's
  first null from 5507 Hz to 3043 Hz and string 3's from 7351 Hz to 4062 Hz,
  against Lemme's measured 3000 Hz and 4000 Hz. Because the two coils also sit
  at two different distances from the bridge, each gets its own position comb
  for free, which is the second-order thickening the single window cannot
  produce. The single-coil setting is one coil and is unchanged - the 4.8 mm
  window's own first null sits at 24 kHz on string 2, so nothing audible moves
  there. `pickupType` continues to morph between them.

  The step survives review, but not for the reason it was written. Putting the
  notch in the right place is the *smaller* half of what the change does. The
  larger half is that two 4.8 mm windows pass most of the top octave the single
  21 mm window was throwing away: on string 2 the replacement is **+2.7 dB at
  4 kHz, +20.9 dB at 6 kHz, +6.5 dB at 8 kHz and +18.2 dB at 12 kHz**, while on
  the plain E4 it is **-3.9 dB at 8 kHz and -28.5 dB at 12 kHz** (`p5_pitch`,
  evaluating both windows in closed form). A humbucker that is brighter than
  today on the wound strings and darker on the plain ones is a different pickup,
  not a corrected one, and whether that is an improvement is a voicing question
  the notch frequency does not answer. The step is therefore scoped to include
  re-voicing the per-coil window width and, if needed,
  `humbuckerResonanceHz`/`Q` (:49-53), against the same dry references the
  `pickupCombDepth` comment cites - and the verification below bounds the
  broadband change, which the first draft did not.
  *Closes gap 3.* **Verified by** `testHumbuckerTwoCoilNotch`: at
  `pickupType = 0`, the deepest notch of the pickup transfer between 2 and 8 kHz
  lies **within 2.8-3.3 kHz on string 2 (note 40)** and **within 3.8-4.4 kHz on
  string 3 (note 45)**, and is **at least 10 dB** below the local envelope
  (today the first null is at 5507 Hz and 7351 Hz respectively); at
  `pickupType = 1` the magnitude response is within 0.2 dB of today's at every
  measured frequency; the low-frequency recovery the `pickupCombDepth` comment
  records does not regress by more than 0.5 dB in the 60-85 Hz band on an open
  low E; and the alias floor on a full chord stays at least 150 dB below the
  spectral peak (today 155.1 dB). **And, new under review, the broadband
  balance is bounded**: at `pickupType = 0`, octave-band energy from 4 to
  16 kHz moves by **no more than 4 dB per band** against today, and the
  2-16 kHz to sub-500 Hz ratio on a full eight-string chord moves by **no more
  than 3 dB**, so the humbucker stays the dark pickup of the pair. Without those
  two, the notch assertion can be passed by a change that measurably brightens
  the setting it is meant to correct.

  Two details of this contract were corrected in preflight. The octave-band
  bound was written to be measured on an open low E only, which is not where
  this step's own numbers say the broadband change happens: the +2.7/+20.9/
  +6.5/+18.2 dB figures above are on **string 2 (note 40)** and the
  -3.9/-28.5 dB figures are on the **plain E4 (note 64)**, and a bound checked
  on string 0 alone would not see either. It is therefore measured on notes 28,
  40 and 64, and a 20.9 dB brightening at 6 kHz on string 2 fails it. And the
  notch itself is measured on the pickup transfer evaluated in closed form at
  the aperture seam, at each string's own wave speed `c = 2 L f_open`, the same
  way `p5_pitch` evaluates both windows - not off a rendered spectrum, where a
  null between two harmonics is sampled only as finely as the string's
  fundamental spacing and its measured depth is an artefact of that spacing.

- [ ] **4. Vibrato becomes a hand.** Four changes, three of them derived. (a)
  The pitch waveform is the *square* of the wrist's displacement, not the
  displacement: a finger bending a string laterally by `x` lengthens its path by
  `dL = k x^2`, so for a wrist rocking as a raised cosine `s(t)` the pitch offset
  is `depth * s(t)^2`, falling out of the same `dL/L` relation
  `ElectryEngine.cpp:391-403` already solves per string for the bar. **The first
  draft described the resulting shape backwards.** `s^2` does not give flat tops
  and sharp returns; it gives a flat *bottom* - the note dwells at pitch between
  excursions, because `s^2` is fourth-order small where `s` is second-order
  small - and a peak whose curvature is doubled, so the excursions are briefer
  and pointier. That is still the right shape for a guitar vibrato, where the
  finger returns the string to rest and waits, but it is the opposite claim and
  the step is kept for the corrected reason. (b) Each fingered string gets its
  own phase, seeded from its own note stream, because two fingers are not one
  finger; today one engine-wide `vibratoPhase_` moves a double-stop in exact
  lockstep. (c) Rate and depth are redrawn each cycle (sigma 12% and 15%) from a
  stream advanced by a **per-voice cycle counter**, not by `noteSequence_`,
  which only advances on note-on and would give a held note one fixed draw.
  (d) The onset uses the `smoothStep` the engine already applies at :1679 and
  :3343-3345 instead of the one-pole, so the vibrato starts from rest rather
  than at maximum slew.

  A single Vibrato Depth parameter exposes the maximum excursion. Note that
  **this reverses an existing calibration rather than adding one**: `rate =
  lerp(4.8f, 6.4f, vibratoAmount_)` at :3937 already couples rate to depth, in
  the direction its own comment states - "a rock finger vibrato runs around
  5 Hz and speeds up as the player leans into it". The step's
  velocity-limited-gesture argument says the opposite, that a wider arc at
  bounded wrist angular velocity is a longer period. Both are folk claims and
  neither was measured, so the reversal is not free: either it ships with a
  citation or the existing coupling stands and the parameter only scales depth.
  The competitive note this gap rests on ("adjustable depth/speed") in fact
  argues for two controls, not for a derived one, and a second control is the
  cheaper answer to it.
  *Closes gap 5.* **Verified by** `testVibratoIsAHandNotAnLfo`, reading through
  the existing private-access seam: over 18 settled cycles the cycle-period
  standard deviation is **at least 4% of the mean** (today 0.0533%) and the
  peak-depth spread is **at least 2.5 cents** (today **0.0001 cents** - the
  first draft's 0.4 cents was nearly four orders of magnitude too generous, so
  this assertion has far more headroom than it appeared to); the fraction of
  each cycle spent above half depth is **between 32% and 40%** - a raised cosine
  gives exactly 50% and measures 49.2% today, and its square gives **36.4%**,
  not the "about 39%" first written - which pins the `x^2` law from both sides
  rather than only from above. **Corrected in preflight**: that fraction is
  measured against **each cycle's own peak**, not against the run's maximum,
  because (c) redraws the depth every cycle at a 15% sigma; referred to a
  global maximum the same correct implementation reads several percent low, and
  the band is only 3.6 percentage points wide either side of 36.4%. Two
  simultaneously fingered strings' phases
  differ by **at least 0.08 cycles**; at maximum Vibrato Depth the peak is **at
  least 90 cents** and at minimum **at most 15 cents**; and zero aftertouch
  remains bit-exact identical to no aftertouch. **Change (d) gets its own
  assertion, added in preflight**, because nothing above tested it and it could
  have been skipped whole: at **10% of the time the vibrato takes to reach 90%
  of its settled depth**, the depth is **at most 5% of settled**. A one-pole
  reads **20.6%** there whatever its time constant, because it is steepest at
  `t = 0`; the `smoothStep` reads **1.8%**. The measure is scale-free, so it
  does not have to name an onset time the step has not fixed yet. The rate
  assertions ("below
  5.2 Hz at maximum, above 6.2 Hz at minimum") are held back until the direction
  of the depth-rate coupling is settled, since as written they assert the
  reverse of the shipping calibration.
  `testFrettingHandVibrato` keeps its existing assertions on upward bias,
  fingered-strings-only scope and onset delay.

- [ ] **5. The strum travels the way the pick does.** Two changes. First,
  direction: the offset is computed from the neck edge the resolved stroke
  starts at, not from whichever note-on arrived first. Maintain the chord's
  extreme string *in the stroke direction* as the anchor, and when a later
  note-on in the same window turns out to be more extreme, re-anchor and push
  the pending offsets of the not-yet-started voices out by the difference. **The
  first draft called that "safe precisely because nothing has sounded yet",
  which is not true in general.** The chord window is 35 ms
  (`chordWindowSamples_`, :789) and a `process()` block is typically 5-10 ms, so
  the note-ons of one chord routinely straddle several blocks and the
  first-arriving voice has usually already started. **Preflight replaced the
  choice the first draft made here**, because the option it preferred cannot be
  given a test that bites - see the note under the verification. The rule is a
  **bounded re-anchor window `R`**: every voice of a new chord, including the
  first, is given a pending pre-roll of `R`, and any note-on arriving within `R`
  of the chord's first note-on may re-anchor and push the pending offsets. All
  voices carry the pre-roll, so none has started during the window and the push
  is always safe. `R` is **20 ms** - enough to cover a chord whose note-ons
  straddle two or three typical blocks, against the 84 ms that giving the anchor
  voice the full `7 x spread` would cost - and it is a fixed time rather than a
  block count, so onsets do not depend on the host's buffer size. `R` is zero
  when `strumSpreadSeconds` is zero, which keeps the block chord bit-exact.
  Chords whose note-ons are spread wider than `R` still travel from the first
  arrival, and that is the stated limit of the mechanism rather than an
  undefined case. Removing the `abs()` is
  independent of that choice and stands either way, so a chord anchored on a
  middle string stops travelling outward in both directions at once. Second,
  spacing: the wrist accelerates through the
  strings, so with the pick entering the plane at speed `v0` and accelerating,
  `v(x) = sqrt(v0^2 + 2 a x)` and the crossing intervals compress monotonically
  as `dt_k is proportional to 1/v(x_k)`. The acceleration is set so the last gap is about 0.7 of
  the first, and the total travel is held to the Strum Spread control's stated
  value so the knob keeps its meaning. The step-2 stream adds per-gap jitter, so
  no two strums lay the same ramp down twice.
  *Closes gap 6, the one gap whose every measured number survived
  re-measurement unchanged.* Depends on step 2 for the jitter stream.
  **Verified by** `testStrumTravelFollowsStroke`: on the same three-note chord
  sent in the same MIDI order, `PickStyle = Down` and `PickStyle = Up` produce
  **reversed** rank orders of `startDelaySamples` - string 0 first on the down,
  last on the up - where today both produce +0.000/+12.000/+24.000 ms; across
  eight strings at a 12 ms spread the gaps are monotonically non-increasing and
  the last gap is **between 0.55 and 0.85 of the first** (today every gap is
  exactly 12.0000 ms); the first-to-last travel time stays **within 5% of
  7 x spread**; a chord whose first note-on is a middle string produces offsets
  that are all non-negative and monotone in string index (today D3-first gives
  24.0/12.0/0.0/12.0/24.0 ms); and at `strumSpreadSeconds = 0` every offset is
  exactly 0, so the block chord stays bit-exact. **The per-gap jitter this step
  takes from step 2 gets its own assertion, added in preflight**, because
  nothing above tested it: two successive strums of the same chord, 12 s apart,
  must differ in at least one string's offset by at least one internal sample,
  where today the offsets are a pure function of spread and string index and
  the two strums are identical to the sample. **And the block-straddling case
  is pinned in absolute onsets**: the same three-note chord delivered with its
  note-ons split across three `process()` calls 8 ms apart - inside the 35 ms
  chord window, across block boundaries - must produce **absolute onset sample
  indices**, each event's arrival sample plus that voice's `startDelaySamples`
  reduced to one clock, that are monotone in the stroke direction and that
  **reverse between `PickStyle = Down` and `PickStyle = Up`**, exactly as in the
  single-block delivery; the test runs it in both arrival orders, ascending and
  descending in string index, and requires the same onset order from both. Two
  latency bounds go with it: in the single-block delivery the first voice's
  onset is **exactly `R` after its note-on**, which is the whole cost of the
  pre-roll and is what the 20 ms is chosen against; and in the split-block
  delivery the first-sounding voice's onset must not exceed the single-block
  delivery's by more than the chord's own note-on arrival span - 16 ms here -
  plus one control period, which is the most a re-anchor can push it. The
  existing `testStrumSpread` assertion that the leading string of a strum is
  delayed by exactly 0 is superseded by the first of those and must be restated
  as `R`, not deleted.

  **Preflight corrected this assertion and the choice it was meant to decide.**
  As first written it compared relative `startDelaySamples` only, and the
  audible onset is the event's arrival time plus that delay. Measured on the
  shipping engine (`pf_strum`, notes 40/45/50 at a 12 ms spread, one note-on per
  `process()` call 8 ms apart), the relative delays are 0/12/24 ms in *arrival*
  order whichever way the chord is sent, so by string index they read
  0/12/24 ms ascending and 24/12/0 ms descending - monotone both times, and the
  assertion passes today with no change at all. The absolute onsets are
  0/20/40 ms in arrival order in both cases: the chord travels in whatever order
  the host sent it, which is the defect the step exists to remove. Reading the
  onsets absolutely makes the descending-arrival case fail today, and it is
  reachable only with a non-zero re-anchor window, which is why the mechanism
  above was changed with it. The preferred rule of the first draft -
  re-anchoring restricted to one block - cannot pass an absolute-onset
  assertion at all: once the first note has sounded, no causal scheduler can
  place a later-arriving string ahead of it.

- [ ] **6. The attack blooms sharp.** Recalibrate the tension modulation against
  the same stretch law the previous steps use. For a string plucked to
  `y0` at fraction `p`, `dL = y0^2 / (2 L p (1-p))`, `dT = E A dL / L`, and
  `df/f = dT/2T = E A y0^2 / (4 T L^2 p (1-p))`. With a plain .010 string at
  E4 - `A = 5.07e-8 m^2`, `E = 200 GPa`, `T = 72.6 N`, so `EA/T = 139` - a 3 mm
  hard pluck at `p = 0.18` gives `df/f = 5.0e-3`, or **8.7 cents**, decaying with
  the string's energy. That arithmetic checks out, and it - not the "tens of
  cents" the gap paragraph first asserted, for which no measurement was found -
  is the target. The mechanism at :1872-1875 is already correct; what is wrong
  is that `energyEnvelope` is a mean square in arbitrary loop units that peaks at
  0.002831, so the product never leaves the noise. Normalise the envelope to
  physical displacement squared against the known excitation amplitude, express
  `tensionDepth` as `EA / (4 T L^2 p (1-p))`, and give the envelope an attack
  fast enough that the bloom peaks during the attack rather than at 183 ms.

  **Two of the three things this step said it would derive are already right,
  and the step is narrowed accordingly.** Measured (`p4_misc`), the peak at
  velocity 1.0 against velocity 0.25 is a ratio of **12.8**, against the `v^2`
  law's 16; and the peak at `stringGauge = 1.0` against `stringGauge = 0` is
  **0.478**, against the `1/d^4` law's 0.45. The existing
  `lerp(1.45f, 0.70f, stringGauge)` already spans 2.07x where the derived law
  wants 2.23x - a difference nothing will hear. So the gauge derivation is
  cosmetic and the velocity law needs no change; **only the absolute scale and
  the envelope's attack time are actually wrong**, and the step should be costed
  as those two.
  *Closes gap 7.* Depends on step 1, because the effect is proportional to the
  square of the pick force and is only worth having once velocity carries real
  force. **Verified by** `testTensionModulationBloom`: on note 40 at velocity
  1.0 with `velocityAmount = 1.0`, the peak sharpness above the settled pitch is
  **between 6 and 20 cents** and occurs **within the first 30 ms** (today
  +0.180 cents at 183 ms); it falls **below 1.5 cents by 400 ms**; at velocity
  0.25 the peak is **between 0.3 and 1.5 cents** (today 0.0141 cents, so the
  lower bound fails today - **corrected in preflight**, where this read "at most
  1.5 cents" and so passed on the shipping engine and on any implementation that
  scaled the bloom at full velocity only. The band is the `v^2` law's own
  prediction: 8.7 cents at v = 1.0 puts v = 0.25 at 0.54 cents, and the asserted
  6 to 20 cents at full velocity puts it between 0.375 and 1.25 cents); and 3 s
  after the attack the sounding pitch is **within 0.6 cents** of nominal on the
  fundamental-only estimator, so the tuning strength survives.
  The two ratio assertions from the first draft - "the velocity peak ratio is
  between 8 and 24" and "at `stringGauge = 1.0` the peak is at most 0.7x the
  peak at `stringGauge = 0.0`" - **already pass today** at 12.8 and 0.478, so
  they cannot fail without the change and prove nothing about it. They are
  retained only as regression guards and labelled as such, not counted as
  verification.
  **And, new under review, the transient is bounded.** Reaching 8.7 cents inside
  30 ms means moving note 40's vertical delay by about 5.8 samples out of 1165
  in a few milliseconds, through an interpolated delay line and a control tick
  that runs every 16 internal samples. The test must assert that the >5 kHz
  peak-to-RMS during the bloom stays at or below the static note's **-63.4 dB**
  - the same locally-referenced measure the control-sweep audit uses - so the
  bloom is a glide and not a chirp, and that the alias floor above 12 kHz stays
  at least 150 dB below the peak. Without those, this step's own headline number
  can be met by an artefact.

- [ ] **7. The strings share a bridge.** Let active voices read the coupling bus
  they already write - **minus their own contribution to it**. Every string
  terminates on one saddle whose mechanical admittance is finite, so the saddle
  velocity driven by the summed string forces drives every *other* string back;
  that is why a chord sounds like one instrument and why a power chord's low
  interval blooms. The engine already computes the summed bridge force
  (`sympatheticBus_`, :3575) and already publishes it one sample late
  (`sympatheticBusDelayed_`, :4066); the change is to inject it into
  `renderVoice` as well as `renderSympatheticString`.

  **The self-term is the whole difficulty and the first draft did not mention
  it.** `sympatheticBusDelayed_` is the sum over *all* active voices, including
  the one about to read it, and the existing readers are inactive voices which
  therefore never see themselves. Injecting it unmodified into `renderVoice`
  would make a *single* note drive itself through the bus, which is not string
  coupling but a change to that string's own bridge termination - and the
  engine already carries that termination in `bodyConductance` and
  `bodyLossFactor`, so it would be double-counted. Every decay-time, T60 and
  timbre calibration in the instrument sits downstream of that. The bus each
  voice reads must be `sympatheticBusDelayed_` minus that voice's own
  contribution from the previous sample, which costs one extra per-voice float
  and is the difference between this step being a coupling and being a
  recalibration of the entire string model.

  The one-sample delay makes this an explicit Jacobi iteration, so the acyclic
  guarantee is replaced by an explicit spectral-radius bound. **The bound this
  step was first written with is wrong by four orders of magnitude, and was
  corrected in preflight.** It read: with `N` strings each injecting a fraction
  `g` of the bus and each loop's gain bounded by `G < 1`, the worst round trip
  is `N g^2 G^2`, held at or below 0.25. That quantity is not the loop gain of
  this network. The coupling matrix has a zero diagonal - every voice subtracts
  itself - and off-diagonal entries `g H_j`, where `H_j` is the transfer from
  an injection into string `j`'s loop to that string's own output, which at the
  string's resonance is `|H_j| = 1 / (1 - G_j)` and **not** the round-trip gain
  `G_j`. The coherent common mode drives all `N - 1` other strings in phase, so
  the safe bound is the row-sum norm

      (N - 1) * g * max_j 1/(1 - G_j)  <=  0.25

  - 12 dB of margin - at every parameter setting including maximum Resonance
  with eight strings fingered. The measured loop gains on an eight-string open
  chord (`pf_loopgain`, read through the voice seam) run from 0.993686 on
  string 0 to 0.998289 on string 7, so `max_j 1/(1 - G_j)` is **584** and the
  bound gives `g <= 6.1e-5`. Neither the struck expression nor the intermediate
  `(N - 1) g G` is conservative against that: a direct simulation of the
  network (`pf_stability`, eight comb loops at the same delays and decay times,
  each reading the delayed bus minus itself) **diverges between `g = 0.001` and
  `g = 0.002`**, where `N g^2 G^2` reads 3.2e-5 and `(N - 1) g G` reads 0.0136,
  and both would certify it. For scale, the shipping sympathetic injection gain
  is `0.0045 * effectiveSympathetic` (:861). It drives a bus in bridge-force
  units rather than loop-output units, so it is not directly comparable, but it
  is the same order as that divergence threshold: `g` for the active-voice path
  is a new, separately calibrated number and must not be inherited from the
  inactive-voice path, which is stable only because it is acyclic.
  `sympatheticAmount` continues to scale the coupling and still switches it off
  completely at zero.

  **The step's headline is smaller than it was written to be.** At the shipping
  `sympatheticAmount = 0.20` the two notes already interact at **-36.1 dB over
  0-1.5 s and -31.4 dB over 10-12 s** (`p4_misc`), through the six strings left
  open, so the -26 dB target is 10 dB of new coupling on top of existing
  coupling, not 42 dB on top of nothing. What genuinely has no path is a voicing
  that leaves nothing open, and that is the case the verification must lead with.
  *Closes gap 8.* Last because it is the only step that removes a structural
  stability guarantee. **Three of this step's assertions were corrected in
  preflight; the reasons follow the list.** **Verified by**
  `testFingeredStringsShareTheBridge`, with `pickNoise`, `fingerNoise` and
  `releaseNoise` at zero throughout so what is measured is the coupling and not
  the per-note noise seed.
  **First**, on a chord that fingers all eight open strings (28, 35, 40, 45,
  50, 55, 59, 64), the render at `sympatheticAmount = 0.20` differs from the
  render at `sympatheticAmount = 0` by a relative L2 of **at or above -20 dB**
  over 0-1.5 s. Today those two renders are **bit-identical**, difference
  exactly zero (`pf_chordref`), because with every voice active there is no
  inactive string left to ring and `sympatheticAmount` has no path at all. That
  is the case this step exists to close, and it is the assertion no
  implementation can pass without closing it.
  Then, at `sympatheticAmount = 0.20`, notes 40 and 47 rendered separately and
  together give `||AB - (A+B)|| / ||A+B||` **at or above -26 dB** over 0-1.5 s
  at velocities 0.02, 0.20 and 0.90 (today **-37.8, -37.9 and -38.0 dB** with
  the noise controls at zero, and -36.1, -36.0 and -36.1 dB with them at their
  defaults - **not** the -68.2/-72.0/-56.4 dB first recorded, which were
  measured on a decayed 10-12 s window and on `sympatheticAmount = 0`).
  The coupling is lossy rather than regenerative: the residual `AB - (A+B)`
  falls by **at least 20 dB in RMS** from the 0-1.5 s window to the 10-12 s
  window (today 31.9 to 33.8 dB, `pf_late`), and an eight-string chord at full
  velocity and maximum Resonance has **strictly lower RMS in each successive
  1 s window** over a 30 s render, which stays finite and inside the existing
  output bound. The bound of the previous paragraph is asserted directly at the
  voice seam: `(N - 1) * g * max_j 1/(1 - G_j)` over the active voices, with
  `g` and each `loopGain` read through the existing snapshot, is **at or below
  0.25** at every setting the test sweeps, including maximum Resonance with
  eight strings fingered.
  **A single note renders within 0.05 dB of today's, band by band and in T60,
  at `sympatheticAmount = 0.20` and again at `sympatheticAmount = 1.0`** -
  which is the assertion that catches the self-term and without which this step
  can silently retune the whole instrument. With one voice active the bus is
  that voice's own contribution, so `bus - own` is exactly zero and the
  injection must be exactly zero; the maximum setting is included because that
  is where an unsubtracted self-term is largest, and the shipping 0.20 alone is
  too small a gain to show it.
  **Off is off**: at `sympatheticAmount = 0` the two-note additivity residual
  stays within **0.1 dB** of its shipping values, -37.8, -37.9 and -38.0 dB at
  the three velocities with the noise controls at zero, so the new path
  contributes nothing at zero. And the alias floor above 12 kHz stays **at
  least 150 dB** below the spectral peak (today 155.1 dB).

  **What preflight corrected here, and why.** The first assertion read "on a
  chord that fingers all eight strings, `||chord - sum of the eight singles|| /
  ||sum||` is at or above -26 dB, which is the case with no existing path at
  all". Measured on the shipping engine (`pf_chord`), that ratio is already
  **-24.70 dB** at velocity 0.20 and -24.97 dB at 0.90 with the shipping noise
  controls, and -25.51 dB with them at zero: it passes today, and it passes for
  a reason that has nothing to do with the step. Each of the eight single-note
  renders has the other seven strings ringing sympathetically and the chord has
  none, so the sum-of-singles reference carries 56 sympathetic strings the
  chord does not, and that mismatch is most of the -24.7 dB. Comparing the same
  chord at two `sympatheticAmount` settings removes both that confound and the
  noise-seed confound below, and is bit-exact today.
  The second is "at `sympatheticAmount = 0` the two-note render is bit-identical
  to the sum", which is unreachable and contradicts this section's own gap-8
  measurement. Superposition already fails at about -40 dB today
  (`pf_additivity` reproduces -40.39/-39.83/-39.74 dB): the shared body and coil
  path, the summing guard gain `1 / sqrt(1 + 0.4356 * guardInput^2)` at
  :4137-4143, and - for about 25 dB of it - the per-note noise seed, which
  advances with `noteSequence_` and therefore differs between one pair render
  and two single renders. With the three noise controls at zero the same
  residual is -65.4/-62.0/-53.8 dB, which is what identifies the seed as the
  larger term and why every measurement above pins those controls at zero.
  The third is "the same ratio at 10-12 s is no more than 3 dB above the ratio
  over 0-1.5 s". Today that rise is **4.7 to 6.2 dB** (`pf_late`) with the
  shipping noise controls and 6.3 to 8.0 dB without them, so the engine already
  fails
  it, and a correct implementation will not meet it either: the ratio grows
  because the coupled content outlives the direct note, which is what coupling
  does. The absolute residual decay above tests the property it was reaching
  for.

### Considered and not planned

- **Fretted intonation: the fret-1-sharp residual a saddle setback leaves
  behind. Scheduled as step 4, struck under review because its own physics does
  not reach its own test.** The plan was to lengthen the path by the action
  height's kink, `dL = (h^2/2)(1/L_n + 1/(L - L_n))` with `L_n = L/2^(n/12)`,
  raise the tension by `dT = E A dL / L`, take `df/f = dT/2T`, and then solve a
  per-string saddle setback that nulls fret 12 - the test asserting fret 1 sharp
  by 2.0 to 8.0 cents. Carrying that arithmetic out (`p5_pitch`) with the step's
  own action ramp (0.4 mm at fret 1 to 1.6 mm at fret 12), the step's own
  `EA/T = 139` and the step's own fret-12 solve gives a residual of **+0.295
  cents at fret 1, +0.036 at fret 5 and -0.001 at fret 12**, on a solved setback
  of 0.51 mm. The asserted 2.0 to 8.0 cents is between seven and twenty-seven
  times what the geometry produces. Reaching it needs either a 1.04 mm
  first-fret action, which is unplayable, or moving the deflection span from
  nut-to-fret to finger-behind-fret - which does raise fret 1 to +2.15 cents raw
  but then solves an **11.4 mm** saddle setback against a real instrument's one
  to four, and leaves frets 2 through 7 **flat** by up to 1.75 cents, breaking
  the step's own monotone-decreasing assertion and inverting the sign of the
  effect it exists to produce. Either way a fudge factor would be doing the
  work.

  It would also be inaudible and unmeasurable. A 0.3 cent position-dependent
  difference between the same interval at position 0 and position 12 beats with
  a period of about thirty seconds, against a test asserting at least 1.5 cents.
  And it sits below the engine's own instrumentation: the suite's
  `measureFrequency` runs a 0.5-cent grid and disagrees with a fundamental-only
  estimator by up to 2.7 cents on the low strings because of inharmonicity, so
  the effect could not be measured even if it were added. Gap 4 stays in the
  list above as a true statement about the engine. If it is ever revisited, the
  mechanism worth modelling is **nut height**, not saddle setback - first-fret
  sharpness on real instruments is a nut problem, which is why the Buzz Feiten
  system compensates the nut - and it needs a reference measurement of fretted
  pitch against fret number that this repository does not have.
- **The amplifier's tone stack, cabinet and microphone choice, push-pull output
  stage and speaker nonlinearity.** All four are real, named, published
  mechanisms Electry does not have: the passive tone stack in closed form (Yeh
  and Smith, DAFx-06,
  [paper](https://ccrma.stanford.edu/~dtyeh/papers/yeh06_dafx.pdf)), the
  push-pull stage with its output transformer and the loudspeaker's influence
  (Macak and Schimmel, DAFx-11,
  [record](https://www.fit.vut.cz/research/result/c73701/.en)), the loudspeaker
  as a reactive load on a damping-factor-of-one amplifier
  ([Quilter](https://www.quilterlabs.com/blogs/news/reactive-loads-for-guitar-amps)),
  and the speaker's own nonlinearity (Yeh, Bank and Karjalainen, DAFx-08,
  [paper](http://legacy.spa.aalto.fi/dafx08/papers/dafx08_17.pdf)). Together
  they are a whole pass's work, and both lines of evidence this pass rests on
  point at the player and the string instead. The amplifier is also bypassable
  and replaceable by a third-party sim; the string is not. Scheduled next, with
  the tone stack first because it is closed-form.
- **Single-string bend, and the bend's trajectory.** The wheel remains a bar.
  Scoping it to the fingered string needs a decision about which string is "the"
  bent one in a chord, which is the musical-intent layer the previous pass
  deliberately deferred along with the chord recogniser. The exponential glide
  (`10.5 cents of a 200-cent bend in the first 5 ms`, and the last 1.5% taking
  from 200 ms to 600 ms) measures as subtle, and the same one-pole is shared
  with the sympathetic retune and the resonance follower, so replacing it is
  wider than one step. Step 4 does replace the law on the vibrato onset, which
  is the case where the step in the derivative is most exposed.
- **Per-string tuning, capo and string count.** Nearly free in this
  architecture - every voice already solves its decay targets, dispersion fit
  and compliance from an open frequency - and it would let one engine cover
  standard six-string and drop-D territory it cannot reach at all. It is a
  feature axis rather than a realism one, and it is still not scoped - but the
  reason has changed. MODO BASS 2's tuning controls have since been checked (see
  "What changed in the field"): the closest structural relative to Electry ships
  a selectable four-, five- or six-string count and a single DROP switch that
  lowers the lowest string one tone, not an arbitrary per-string tuning matrix.
  So the competitive case for a full matrix is weaker than assumed, and the
  cheap, well-precedented subset - string count plus one drop - is what a future
  pass should scope first. Reported from search summaries only; no product page
  was read.
- **Cable capacitance and volume-pot loading.** The pickup's resonance is set by
  the total capacitance hanging off it, most of which is the cable (commonly
  300-700 pF), and its height by the volume pot, tone pot and amplifier input
  resistance
  ([shootoutguitarcables](http://www.shootoutguitarcables.com/guitar-cables-explained/capacitance-resonant-frequency.html),
  [Lemme](http://buildyourguitar.com/resources/lemme/)). This is real and cheap,
  and it is what makes rolling a real guitar's volume back soften the attack
  rather than only lower it. It needs two more parameters and it belongs in the
  same pass as the other pickup-circuit work, not split across two.
- **Eddy-current losses in pole pieces, magnets and covers**, which make the
  pickup's inductance frequency-dependent and its resonance less peaky than a
  constant-coefficient second-order section
  ([Bedlam Guitars](https://bedlamguitars.wordpress.com/technical-info/pickup-inductance/)).
  Correct criticism, but fitting it needs a reference measurement the repository
  does not have.
- **Dead spots as a frequency rather than a fret number.** The centres at
  `ElectryEngine.cpp:1295-1308` are eight hard-coded fret integers, independent
  of scale length, gauge, tuning and bend, where a dead spot is the neck's first
  bending mode absorbing a fixed *frequency*. Measured, the mechanism works at
  `construction = 1.0` (decay 8.08 s at fret 0, 5.82 s at fret 7, 8.74 s at
  fret 14) and is entirely buried at the shipping default `construction = 0`
  (11.55 s rising monotonically to 19.83 s, no dip at all). Fixing where it sits
  therefore changes nothing a user hears until that control is re-voiced, and
  the two belong together.
- **The dispersion re-fit's bursts on String Gauge and Scale Length sweeps.**
  Measured at -15.0 dB and -20.0 dB against a static note's -63.4 dB, from the
  coarse two-pass grid search at :1790-1831 stepping eight allpass coefficients
  inside a live loop. It is a genuine defect, but it only sounds while
  automating two controls that do not move on a real instrument, and the fix is
  a numerical one - a continuous or interpolated fit - rather than a change to
  what the instrument sounds like when played.
- **Note-off damping that varies with string, articulation and hand.** One
  hard-coded 60 ms T60 and 22 ms ramp at :2832-2837 stand in for a lifting
  finger, a dropping palm, a choked chord and an open string the fretting hand
  cannot reach. Measured spread across five octaves and the full range of string
  mass is 18% (93 ms at note 28, 79 ms at note 86), and it is identical for
  Sustain, Palm Mute, Harmonics and Dead because the code never reads
  `playStyle`. Rated subtle by the audit and cut against the seven above.
- **Position-shift and release squeaks from the fretting hand's own movement.**
  The step-1 hand model of the previous pass knows when and how far the hand
  moves, and the slide work already implements a velocity-dependent winding
  friction band, so wiring the second to the first is a routing change with a
  good expected return
  ([Impact Soundworks](https://impactsoundworks.com/docs/Shreddage%203%20Stratus%20Free%20Manual.pdf)).
  It is cut only for budget and is the first candidate for the next pass after
  the amplifier.
- **Magnetic damping of the string ("Strat-itis").** Not implemented, and should
  not be on current evidence: the one experimental study found reports the
  magnet "exhibited no observable effects on the string's vibration that could
  be interpreted as having a damping effect", only a small reduction of the
  partials whose node sits over the magnet. Recorded here so the next pass does
  not spend effort on forum consensus. If it is ever pursued, Zollner's
  *Physik der Elektrogitarre* chapters 3-5 are the primary source and must be
  read directly.
