# Making Taikor best in class

This document records what the expensive commercial taiko instruments actually
do, where Taikor stands against them today, and the numbered steps taken to
close the distance. It is written to be checkable: every claim about Taikor is a
number measured from the shipping engine, and every step states what would fail
if the step were reverted.

## The instruments this competes with

| Product | What it is | Price | Scale |
| --- | --- | ---: | --- |
| Sonica **TAIKO THUNDER: The Ultimate Collection** | Kontakt Player, 20 percussion instruments played by Japanese taiko performers | $695 | Seven microphone channels at 24-bit/96 kHz, 19 sampled hit positions per hand, dozens of articulations, 1,400+ MIDI grooves |
| In Session Audio **Taiko Creator** | Kontakt, 24 drums plus gongs, cymbals, sticks and vocalisations | $139 | 9,881 samples; every head hit is 7 round robins × 7 velocity layers; several mic perspectives; suite-based MIDI content |
| 8Dio **Epic Taiko Ensemble** / **Solo Taiko** | Kontakt, ensemble and solo taiko | $148 / $198 | 2,900 and 4,200 samples; up to 8 velocity layers at 10 round robins; two microphone positions; articulation browser and step sequencer |
| Impact Soundworks **Kageyama Taikos** | Kontakt, 9 solo instruments played by Isaku Kageyama | $99 | Two mono spot positions plus a stereo overhead, 24-bit/48 kHz |
| Sound Magic **Supreme Drums Taiko** | The only modelled competitor. "Hybrid modelling" Epic Engine | $199 | 200 MB rather than 20 GB; claims unlimited round robins and up to 65,536 velocity steps via MIDI 2.0 |

Sources:

- [TAIKO THUNDER: The Ultimate Collection — Sonica Instruments](https://sonica.jp/instruments/en/product/taiko-thunder-the-ultimate-collection/)
  and its [KVR listing](https://www.kvraudio.com/product/taiko-thunder-the-ultimate-collection-by-sonica-instruments)
- [Taiko Creator — In Session Audio](https://insessionaudio.com/products/taiko-creator/) and the
  [Sound On Sound review](https://www.soundonsound.com/reviews/insession-audio-taiko-creator)
- [Epic Taiko Ensemble — 8Dio](https://8dio.com/products/epic-taiko-ensemble-vst) and
  [Solo Taiko — 8Dio](https://8dio.com/products/the-new-solo-taiko-drum-vst-au-aax-kontakt-instruments-samples)
- [Kageyama Taikos — Impact Soundworks](https://impactsoundworks.com/product/kageyama-taikos-kp/)
- [Supreme Drums Taiko — Sound Magic](https://www.kvraudio.com/product/supreme-drums-taiko-by-sound-magic),
  developer notes on the hybrid approach on [KVR](https://www.kvraudio.com/forum/viewtopic.php?p=9203245)
- Player opinion on what separates them:
  [VI-Control: what are the best sample libraries for taiko percussion](https://vi-control.net/community/threads/what-are-the-best-sample-libraries-for-taiko-style-percussion.75225/),
  [VI-Control: which taiko library to buy](https://vi-control.net/community/threads/poll-which-taiko-low-percussion-deep-drum-library-to-buy.147283/),
  [VI-Control: taikos and toms](https://vi-control.net/community/threads/taikos-and-toms.52420/page-3)

What the top tier is sold on, in the order buyers argue about it: round-robin
depth and machine-gun avoidance; the number of usable microphone perspectives;
ensemble as well as solo material; hit-position coverage; and dynamic range.
What buyers complain about, in the same threads, is narrower than the marketing:
"very little variation and limited dynamics (LOUD!)", and smaller drums that
"sound flat and boring" with playability that "isn't all that inspiring".

The acoustics literature on this instrument is thin. Two papers matter here,
both by Ando in *Acoustical Science and Technology*:

- ["Theoretical and experimental studies on the resonance frequencies of a
  stretched circular plate: Application to Japanese drum
  diaphragms"](https://www.jstage.jst.go.jp/article/ast/30/5/30_5_348/_article),
  AST 30(5) 348 (2009). A nagado-daiko head is chemically treated cow skin with
  a Young's modulus near 3.5 GPa, stretched to a tension far above a drum-kit
  head's, and is therefore a *stretched plate* — a stiff membrane — rather than
  an ideal membrane. The paper tabulates the eigenvalues of the fifteen lowest
  modes as a function of the tension-to-bending-stiffness ratio. Measured on a
  48 cm nagado, the effect on the spacing of the *lowest several* modes is
  smaller than the plate theory predicts.
- ["Resonance frequency changes of Japanese drum (nagado daiko) diaphragms due
  to temperature, humidity and
  aging"](https://www.jstage.jst.go.jp/article/ast/33/4/33_E1209/_article),
  AST 33(4) (2012).

Applied acoustics has almost nothing else: as of
[Piana et al. (2021)](https://www.sciencedirect.com/science/article/abs/pii/S0003682X21005284)
there were essentially no scientific publications on taiko acoustics at all.

## Where Taikor actually stands

Measured from the shipping engine at its factory settings, 48 kHz.

**What it already wins on.** Strike position is continuous rather than nineteen
sampled points. Velocity is continuous rather than seven layers, and the timbre
follows it through the Hertz contact law rather than through crossfades.
Round robins do not exist because nothing is replayed. The stereo image comes
from evanescent near-field decay across two points of the same membrane. Head
size, tension, material, shell material, body depth and head coupling are
physical quantities, not EQ. None of the commercial libraries can offer any of
that, and the one modelled competitor does not describe its model at all.

**Where it is behind, specifically.**

1. **The head is solved as an ideal membrane.** Mode frequencies are exactly
   `c·λ(m,n)/2πa` with λ the Bessel zeros, so the modal *ratios* are constants —
   the README says so explicitly. Ando's measurement says a taiko head is a
   stiff membrane. The consequence in Taikor is that the ratio of the highest
   resolved mode to the fundamental barely moves across the family (7.70 at two
   octaves down, 6.37 at the reference, 5.75 two octaves up — and all of that
   variation is air loading, none of it is the head). A shime-daiko's
   characteristic stretched, metallic upper spectrum is not reachable, and Head
   Material moves only density and loss.

2. **The attack pitch glide is partly driven by an arbitrary constant.** The
   glide is a fixed 115 ms exponential whose depth is
   `tensionModulation · 0.115 · v/v_max · membraneGain` — it does not know the
   head's tension, size or stiffness, so a slack ō-daiko and a tight shime-daiko
   bend by the same amount, which is not what either does. On top of it sits a
   term proportional to `sqrt(Σ y²)` over the resonator states with a bare 0.005
   coefficient. Those states are in units of `modelScale`, the single scalar the
   engine documents as *"the only place a number is chosen for how it sounds
   rather than for what it means … a single scalar so it cannot distort any
   relationship inside the model"*. It now sets the depth of the pitch glide: at
   full velocity and full Tension Mod the scripted glide is 1.39 semitones and
   the applied shift reaches 2.07, so a third of the bend is calibration.

3. **A stroke on a ringing head does not know the head is ringing.** Every
   trigger builds an independent voice with its own copy of the modal bank, and
   voices only sum. Eight identical Dons 62 ms apart are bit-identical to eight
   copies of one Don added offline. On a real drum the bachi lands on a moving
   membrane and takes energy out of it — which is exactly the mechanism that a
   sample library can only approximate with round robins, and the one place
   where a model should be unarguably ahead.

4. **Two blocks of the signal path do not do what they are labelled.**
   *(Both of these, and the `modelScale`-dependent half of gap 2, were removed
   on `main` in `dd23e6e` after this assessment was written and before this work
   was reconciled onto it. The descriptions below are of the tree the plan was
   drawn up against; what each step actually shipped is recorded under the step
   itself.)*
   - The "soft odd-harmonic Zelkova wood shell saturation" in `renderVoice` is a
     clamp at ±1.2 applied after a 1.5× pre-gain, followed by `(x − 0.04x³)·0.8`.
     The largest shell-mode drive input the engine can produce is 0.178, so the
     clamp is never approached and the cubic term is 58 dB below the linear one.
     What is left is a 1.2× gain, gated on `shellResonance > 0.01`: a Katsu
     measures −17.57 dBFS at Shell Resonance 0.99 % and −15.98 dBFS at 1.01 %.
     A continuous control has a 1.59 dB step in it, and the step is documented
     as saturation.
   - The "iron tack (byō) micro-chatter" on rim and shell strokes adds noise of
     amplitude 0.08 to a contact whose amplitude is 2535 (Don Rim) or 1879
     (Katsu): 87 dB down, inaudible at every setting. It ignores the Stick Noise
     control, is not mentioned in the README, and its only measurable effect is
     to advance the voice's noise state. Byō chatter is a real mechanism on a
     tacked nagado; this is not it.

5. **The dynamic range is narrow, which is the single most common complaint
   about the competition.** MIDI velocity maps to impact speed through
   `geometricLerp(0.45, 6.0, s²)`. Squaring before a geometric map compresses
   the bottom rather than expanding it: at the default Velocity Depth the whole
   lower half of the keyboard's velocity range lives between 0.49 and 0.99 m/s.
   Measured peak level from velocity 0.15 to 1.00 is 17.5 dB for a Don and
   17.0 dB for a Buzz. A real taiko covers a great deal more than that between a
   ghost tap and a full-arm ō-daiko stroke.

6. **There is no ensemble.** Kumi-daiko is an ensemble form and every library
   above sells ensemble content; Taikor plays exactly one drum struck by exactly
   one player. Sixteen voices of the same drum are sixteen coherent copies, not
   an ensemble.

7. **There is one microphone perspective.** A close pair from 3 to 40 cm.
   Taiko Thunder ships seven channels. This one is not addressed below and the
   reason is given at the end.

## The steps

Each step is a single commit. The DSP suite must be green before each one, and
each lands with a test in `Tests/` that fails without it.

- [x] **1. Solve the head as a stiff membrane.** Add the bending term to the
  membrane's dispersion relation, `ω² = (T k² + D k⁴)/σ` with
  `D = E h³ / 12(1−ν²)`, the hide's thickness `h = σ/ρ` and its Young's modulus
  interpolated from 4.0 GPa (thin synthetic film) to 3.5 GPa (treated cowhide,
  Ando 2009). The stretch is taken relative to the (0,1) mode, because a drum is
  tuned by the pitch it sounds: what stiffness leaves behind after tuning is the
  spread above the fundamental, not an overall transposition. Closes gap 1.
  *Verified by*: Head Tension is the control that isolates the effect, because
  it moves the stiffness parameter as 1/T and leaves the air load - which
  depends only on areal density and radius - exactly where it was. The highest
  resolved membrane mode must therefore sit further above the drum's ideal
  fundamental on a slack head than on a tight one, which is not true of an ideal
  membrane at all. Head Material must move the reported stiffness parameter by
  two orders of magnitude, that parameter must stay finite at every corner of
  the controls, and the reported ideal fundamental must remain exactly a
  membrane frequency so the octave contract is untouched. The first draft of
  this plan proposed testing it against Head Material directly; that is
  confounded, because a thin film is loaded far more heavily by the air than a
  thick hide and the air moves the same ratio the other way.

- [x] **2. Delete the shell gain step disguised as saturation.** Closes gap 4a.
  *Verified by*: a test that sweeps Shell Resonance across 0.01 and requires the
  rendered Katsu level to be continuous, which fails by 1.59 dB today.
  *What actually shipped*: `main` deleted this block independently in `dd23e6e`
  while this work was in progress, and deleted it identically, so the reconciled
  branch carries no engine change for this step — only the guard test
  (`testShellResonanceHasNoStepInIt`) and the comment recording why the wooden
  bank is driven linearly. The step is kept in the list because the test is what
  stops the block coming back.

- [x] **3. Derive the attack pitch glide from the head's own stretching.**
  Replace the fixed envelope with a
  von Kármán/Berger tension rise: the mean tension a clamped membrane gains from
  a transverse displacement `w` is `ΔT/T ∝ (E h / T(1−ν²))·(w/a)²`, and `w` is
  recovered from the resonator states in metres by dividing out the calibration
  the drive carries. The glide then decays with the head rather than on a
  115 ms clock, falls as tension rises, and no longer depends on the output
  calibration. Closes gap 2.
  *Verified by*: the applied tension shift after a hard stroke must fall when
  Head Tension is raised at constant velocity - a slack head must bend at least
  three times as far as a tight one - which is not true today because the depth
  does not read the tension at all. It must also rise with velocity, vanish
  exactly at Tension Mod 0, and stay near unity for a stroke on the shell.
  The claim that a thicker head bends further was in the first draft of this
  plan and is dropped: a thicker head is stiffer in the plane *and* heavier, and
  measurement puts the two within a factor of two of cancelling, so it is not a
  prediction worth asserting.
  *What actually shipped*: the `modelScale`-dependent energy term this step also
  set out to replace was deleted independently on `main` in `dd23e6e`, for the
  same reason given here — it let the output calibration retune hard strokes.
  `main` closed that by removing the coupling outright; this step puts a derived
  one back in its place, with the calibration divided out rather than left in.
  The two are not in conflict, and the reconciled branch keeps the derived
  version: the resonator states carry one power of `modelScale` each, the depth
  carries `1/modelScale²`, and the product is invariant. Verified by rebuilding
  the engine with `modelScale` doubled — the peak applied tension shift is
  bit-identical at 292 and 584, and removing the `1/modelScale²` factor pins
  every stroke at the saturation limit, so the compensation is load-bearing
  rather than decorative.

- [x] **4. Let a stroke damp the head it lands on.** A bachi meeting a ringing
  membrane is a collision, and a collision with restitution `e` leaves a mass
  `M` moving at `v` going at `v(M − em)/(M + m)`. The mass the stick meets is
  each mode's own referred to the contact point, `M/ψ²` with `ψ` the mode shape
  there, so a centre stroke resets the boom, an edge stroke leaves it and dries
  the edge instead, and the high modes — lighter than a bachi — are turned
  round. Applied to the membrane modes and the continuum of every voice still
  sounding on the same drum. Closes gap 3.
  *Verified by*: what eight identical strokes leave ringing must be at least
  3 dB under eight copies of one stroke summed offline (it is 3.2), while the
  first stroke is bit-identical to the single one; and a ghost stroke on the
  same drum must take a real bite out of what is ringing while the same stroke
  an octave away takes none.

- [x] **5. Give the tack line a real voice.** Replace the inaudible chatter with
  byō rattle: a threshold in the force a stroke has to beat before it lifts the
  head off its tacks, the excess above it as the drive, a band around the nail's
  own ring rather than broadband noise, its own noise source so it cannot shift
  the hide's, and a few milliseconds of chatter while the head settles back.
  Closes gap 4b.
  *Verified by*: the preload must rise with Head Tension and with Head
  Diameter; a light rim shot must not reach it and a full one must clear it
  threefold; Stick Noise 0 must silence it; only the strokes that reach the hoop
  may have it at all; and in the rendered audio a rim shot's high band must open
  up with velocity at least 2 dB faster than a centre stroke's, once each is
  measured against its own level - it is 5.3 dB ahead with the tacks and 2.3 dB
  behind without them. The level itself is checked in the built state rather
  than in the spectrum, for the reason the wooden bank already is: the rattle
  lasts about a millisecond and shares its band with the head's continuum.

- [x] **6. Open the bottom of the dynamic range.** Lower the minimum impact
  speed from 0.45 m/s to 0.12 m/s and drop the squaring, so velocity is even in
  decibels across its range instead of piling the bottom half onto the floor.
  The top of the range is left exactly where it is, so the loudest stroke the
  instrument can make does not move and the factory Output stays correct.
  Closes gap 5.
  *Verified by*: the peak level span from velocity 0.02 to 1.00 at full Velocity
  Depth must exceed 30 dB for every stroke (it is 34 to 42; it was 22 to 29);
  five equal steps of velocity must be within a factor of 1.8 of each other in
  decibels (they are within 1.2; they were within 2.2); and the loudest single
  stroke at the factory output must still stay clear of the safety limiter.

- [ ] **7. Play the drum as an ensemble.** *Dropped after investigation — see
  below.*

## What was investigated and not done

**The ensemble (gap 6).** The physically honest form is several drums of
slightly different size and tension, standing at their own distances and angles
from the close pair, struck with human timing spread — not a chorus. That is a
new automatable parameter, and a new parameter means editing
`PluginProcessor.*`, `PluginEditor.*` and `Tests/PluginProcessorTests.cpp`,
none of which can be compiled in this environment. It also needs the voice pool
raised well above sixteen to keep a roll playable with four players, which
changes the engine's memory footprint by a factor it is not obviously worth. The
work is real and the gap is real; it wants a session that can build the JUCE
target, so it is left undone rather than shipped unverified.

**A room or distance perspective (gap 7).** Every competitor sells microphone
perspectives, and the honest way to add one here would be a room, which this
repository does not have and cannot buy: no impulse responses, no assets. An
algorithmic reverb bolted to the output would not be part of the model and would
not be defensible next to the rest of the instrument. The near-field pair stays
the whole of the microphone story.

**A press roll solved from the bounce rather than written down.** The Buzz
stroke's contact schedule is a hand-written train: a first gap of 19 ms
shrinking by 0.82 per bounce, an amplitude shrinking by 0.72, neither of them
reading the impact speed, the bachi or the drum. Replacing it with the bounce of
a pressed stick needs the press force and the bounce restitution, and the model
has neither: the restitution it does carry, 0.42, is the impulsive figure for a
bachi meeting a head and collapses the roll inside twenty milliseconds, which is
not a buzz. Deriving it would mean inventing two constants to replace two
constants, which is not an improvement in honesty, so the schedule is left as it
is and named for what it is.

**The enclosed air as a distributed column.** The cavity is a lumped spring,
`ρc²/L`, which is only valid below the body's first axial air resonance —
`c/2L` is 212 Hz on the factory drum and moves from 139 Hz to 451 Hz across Body
Depth, so the assumption fails inside the drum's own range. Solving it properly
turns the two-by-two axisymmetric eigenproblem into a three-by-three with the
air column as its own degree of freedom, and touches the render path, the
readout and the tail sweep together. It is the best remaining physics in this
instrument and it is too large to land safely alongside the six steps above.

**Documentation.** A final commit reconciles the prose with the model: the
head's modal ratios are no longer constants of the geometry, and the list of
calibrated constants in the README named two of the six the engine actually
carries. The factory drum being a 95 cm ō-daiko rather than the 55 cm nagado,
and the loudest stroke sitting far above the 9.7 dB the processor's own comment
claimed, were both corrected on `main` in `dd23e6e` before this work was
reconciled onto it; the figure was re-measured on the reconciled tree and is
about 22 dB above unity for a full-velocity rim shot on the largest drum, with a
ghost stroke some 34 dB below a full blow on the same drum.

**Demonstration audio.** Steps 1, 3, 4 and 6 all move levels, so the committed
takes under `Docs/audio/` and the level table in their manifest are stale until
they are re-rendered. They are re-rendered centrally rather than here. No new
take was added for any step, because the number of takes is asserted in shared
CI workflow files that this work may not edit.

## Second pass — 2026-08-07

The first pass fixed what the model was missing at the bottom of the spectrum:
the head's stiffness, the glide, the collision, the dynamic range. This pass set
out to do the same for the top of it and for the keyboard, and it was written
with eight steps. Five survived review.

What survived is the sample rate reaching the continuum's level, an attack glide
that steps its own resonators and sprays noise to 20 kHz, a striker that never
rings, a cavity treated as a spring of infinite extent, and a keyboard octave
that is not an octave in the drum's own lowest mode. What did not survive is
everything that tried to shape the continuum band by band — its ceiling, its
source size, its contact patch, its parametric pump. All four were prototyped on
a scratch copy of the engine and measured, and all four moved the audio by less
than a decibel where they had promised six, for one shared reason recorded as
gap 2: the continuum's bands are one-pole differences with 12 dB/octave skirts
and a level law falling as *f*^−1.5, so the lowest band is louder than every
band above it in that band's own octave and no per-band physics can be heard
through it. Making the bands separable is now the gate on that whole line of
work, and it is written up with the rest of what is not planned.

### What changed in the field

**This is a search-only refresh, and every competitive claim in it is
second-hand.** When this section was first written no research was possible at
all: the session's web-search budget was exhausted and every outbound fetch was
refused. Budget has since refreshed and this subsection was rewritten from about
two dozen distinct searches. Direct page fetches are *still* refused —
`WebFetch` returns `EGRESS_BLOCKED` for every publisher and vendor domain, which
is an organisation policy denial and was not routed around. So **not one primary
source was opened.** Everything below is what search-result summaries quote from
pages that were never read. Nothing is written as though a page had been read,
no price, version, date or quotation appears unless a search result stated it,
and — following the rule this document already applies to its literature — no
URLs are given for the new material either, because a URL that has not been
fetched is not a citation. The list of what remains unverified, at the end of
this subsection, is the honest bottom line of the whole exercise.

**The product table survives contact with search.** Results continue to report
Taiko Thunder at a $695 regular price with seven microphone channels and
1,400-plus MIDI grooves; Taiko Creator with seven round robins at seven
velocities per head hit and four range-adjustable mic positions; Epic Taiko
Ensemble at over 2,900 samples and Epic Solo Taiko at over 4,200, two mic
positions each; Kageyama Taikos at 24-bit/48 kHz with two mono spots and a
stereo overhead, list $99; and Supreme Drums Taiko at about 200 MB against a
20 GB library, claiming unlimited round robins and up to 65,536 velocity steps
over MIDI 2.0. Nothing found contradicts a line of it. That is corroboration of
the table's *claims*, not of the products — see the unverified list.

**What has actually moved since the first pass.**

- **Kageyama Taikos is now at 1.5.** Results describe it as adding a 16-inch
  chudaiko and reverse-side samples for the odaiko and both chudaikos, being
  re-encoded for the free Kontakt Player, and carrying UI changes and bug fixes;
  the list price stays $99 with a $20 upgrade for existing owners. One result
  showed a $49 street price at search time. The row's "9 solo instruments" is
  therefore probably now stale on the low side.
- **8Dio's two taiko titles now also exist on Soundpaint**, as *Epic Taiko
  Ensemble 2.0* and *Solo Taikos*, with 127 dynamic velocity layers and a figure
  of about $40 attached. One retailer listing shows Epic Taiko Ensemble as
  discontinued while 8dio.com still sells it; which of those is current could not
  be settled.
- **Sound Magic's Epic engine has moved to a V2** — Supreme Drums Orange V2 and
  the new Supreme Drums Blue, covered in early 2026 — but no result indicates
  Supreme Drums Taiko itself received it. Supreme Drums Taiko still dates to its
  October 2023 release and appears on a MIDI Association innovation-award page.
- **Sonica** shipped Taiko Thunder around late January 2025 after a 35 %
  pre-order at $451.75, and has run recurring sitewide sales through 2026. No
  successor and no new Sonica taiko product surfaced.
- **In Session Audio** has shipped nothing new for Taiko Creator: the two
  expansion packs and the "Complete Suite" addition all predate this decade's
  threads.

**No new dedicated modelled taiko competitor exists.** Supreme Drums Taiko is
still the only one, and it still does not describe its model. What did change is
the floor underneath the category — sample-free modal synthesis is now free and
commonplace:

- **RipplerX** (tiagolr), free and open source, built on JUCE, dual coupled
  resonators with nine models including Membrane and Drumhead and up to 64
  partials each. Its author is quoted saying it "started as a research project
  into physically modeled drums and ended up a synth heavily based on AAS
  Chromaphone". Covered February 2025.
- **reFX Rippler**, a commercial modal synth, reviewed in August 2026 as having
  "Chromaphone-like DNA".
- **FX-Mechanics MechanOdd**, free polyphonic physical modelling, June 2026,
  whose resonator menu includes a drum membrane.
- **AAS Chromaphone 3** remains what forum posters reach for when they want a
  modelled taiko. No taiko-specific evaluation of it was found.

None of these is a taiko instrument: none carries head tension, hide, shell,
cavity or strike position as a physical quantity, and none of the coverage found
claims otherwise. What they change is the answer to "why not just use a free
modal synth" — for a generic percussive hit, there is now no reason not to, and
the differentiator has to be the drum-specific physics rather than the fact of
being modelled. Sample-side, names that recur in 2025–26 threads but are absent
from the table are Strezov Sampling *Taikos X3M* (with a free THUNDER X3M taiko
alongside it), Evolution Series' taiko, Heavyocity *Damage 2*, *Metropolis
Ark 3*, and free packs from SampleScience, Prisma Sounds, Floe and Pianobook.
One hardware note: Roland's TAIKO-1 is modelled rather than sampled, and forum
posts describe its hits changing timbre with position and force enough to sound
like "two totally different hits".

**What 2025–26 reviewers and owners single out**, all at second hand:

1. **Microphone perspective and mixing flexibility** are named again and again
   as what separates convincing from fake, together with the claim that mixing
   solo recordings into an ensemble "isn't really an effective trick".
2. **Ensemble timing spread is a sold feature, not an incidental one.** Taiko
   Creator's Unity control is described as setting the timing spread of the
   drums within each group, with zero collapsing a group into one composite hit.
3. **Taiko Thunder is the current favourite.** A June 2025 VI-Control post calls
   it "the most stunning taiko library I've ever found" and says the poster's
   Spitfire, Orchestral Tools and VSL purchases are overshadowed by it. No
   Sound On Sound or comparable magazine review of it was found.
4. **The sharpest criticism found is aimed at interpolated velocity**, and it
   lands on Soundpaint rather than on any taiko title specifically: users report
   the 127 layers behaving as a volume ramp, one calling it "kinda lifeless" and
   another finding roughly four real velocity switches at the top after drawing
   all 127 in a DAW. A KVR poster who performs with a taiko group says they are
   not sure any of these libraries is actually good — only adequate for dramatic
   cinematic drums.

**No listening test, measurement or shootout was found.** Searches for A/B
comparisons and head-to-head videos returned nothing about virtual taiko at all;
the phrase collides with Taiko Audio, a hi-fi server maker, and with a
browser-automation library of the same name. The category is still evaluated by
vendor demos and forum opinion, which is worth recording: there is no published
number anywhere in this field for anyone to be measured against, including
Taikor.

Alongside the competitive pass the engine was checked against the acoustics
literature. Nine mechanisms were found that the model does not carry; two of
them drive surviving steps and four were prototyped and abandoned. The
references used are given in full here rather than as links, because **none of
them could be opened this session either** and a URL that has not been fetched
is not a citation:

- P. M. Morse and K. U. Ingard, *Theoretical Acoustics* (McGraw-Hill, 1968),
  Ch. 5 — the mode count of a circular membrane below *f* goes as *A f² / c²*,
  so modal density per hertz rises with the square of the radius. It says the
  opposite of what the engine's fixed ceiling does, and it was the authority for
  the ceiling step; that step was prototyped in review and struck for being
  inaudible, so this reference now supports a finding rather than a commit.
- N. H. Fletcher and T. D. Rossing, *The Physics of Musical Instruments*,
  2nd ed. (Springer, 1998), Ch. 3 (membranes), Ch. 2 (free-free bars), Ch. 18.
- L. Rhaouti, A. Chaigne and P. Joly, "Time-domain modeling and numerical
  simulation of a kettledrum", *JASA* 105(6), 3545–3562 (1999) — the mallet
  force is applied as a distribution over the contact area, not at a point.
  Was the authority for the contact-patch step, struck in review.
- V. Chatziioannou and M. van Walstijn, "Energy conserving schemes for the
  simulation of musical instrument contact dynamics", *JSV* 339, 262–279
  (2015) — both colliding bodies carry the contact force. Authority for step 3.
- C. Touzé, S. Bilbao and O. Cadot, "Transition scenario to turbulence in thin
  vibrating plates", *JSV* 331(2), 412–433 (2012); M. Ducceschi and C. Touzé,
  "Modal approach for nonlinear vibrations of damped impacted plates",
  *JSV* 344, 313–331 (2015) — the von Kármán term the engine already uses for
  its mean-tension retune also couples modes. Was the authority for the
  parametric-pump step, struck in review.
- Y. Ando, AST 30(5) 348 (2009) and AST 33(4) (2012), both already cited above.
- S. Dahl, "Playing the accent", *Acta Acustica united with Acustica* 90(4),
  762–776 (2004); J. B. Allen and D. A. Berkley, "Image method for efficiently
  simulating small-room acoustics", *JASA* 65(4), 943–950 (1979); A. W. Leissa,
  *Vibration of Plates*, NASA SP-160 (1969); R. S. Christian *et al.*, "Effects
  of air loading on timpani membrane vibrations", *JASA* 76(5), 1336–1345
  (1984) — all consulted, none turned into a step; the reasons are at the end.

One absence is worth recording as an absence rather than as a gap: **no
published measurement of a taiko stand's modes, or of the reaction-force path
from the bachi into the mounting, could be found.** The mechanism is ordinary
structural dynamics — the impulse the stick gives the head is given equally to
the hoop, the shell and whatever the drum stands on, and a hundred-kilogram
ō-daiko on a compliant stand moves as a rigid body at a few tens of hertz — and
it would put a component under a hard stroke that does *not* move when the drum
is retuned. Taikor's entire sub-40 Hz region tracks the tuning exactly, so that
component is missing. It is not implemented because the numbers do not exist,
and inventing them would be drawing a curve.

Search adds one thing to that absence and nothing else: **no taiko acoustics
work published after Ando's 2012 paper surfaced.** Queries for post-2012
nagado-daiko measurement returned the 2009 and 2012 papers already cited and
nothing newer. A 2025 *Frontiers in Signal Processing* editorial on sound
synthesis through physical modelling appeared, but it is not about taiko and it
was not opened. Piana et al.'s finding that the field has essentially no
literature still stands, so the stand-mode gap above is unlikely to close from
outside.

**What is still unverified, and why.** This list is the part of this subsection
that matters most, because the constraint that produced it has not gone away:
searching is not reading, and no page was read.

- **Every figure above.** Search summaries paraphrase, can be stale, and can be
  quoting a reseller or a cached page rather than the vendor. None of the
  numbers in the product table was re-read at its source.
- **Today's price of any product in the table.** Taiko Thunder's $695 and
  Kageyama's $99 were repeated by several independent results and are the safest
  of them; the 8Dio pair's $148 and $198 were not corroborated at all this pass;
  the Soundpaint ~$40 and the Kageyama $49 are promotional figures with unknown
  end dates. Sale prices seen in results are deliberately not carried into the
  table.
- **Whether Supreme Drums Taiko has been updated since release**, and what
  version it is at. The V2 engine is confirmed only for other titles in the line.
- **Whether Kageyama Taikos is at 1.5 or 1.6.** A 1.6 appears in a third-party
  listing that was not corroborated anywhere else; only 1.5 has real coverage.
- **Whether 8Dio still sells the Kontakt editions**, given one retailer marks
  Epic Taiko Ensemble discontinued and the vendor page still offers it.
- **What any of these products actually measures.** No dynamic range, no
  round-robin behaviour under repetition, no hit-position count has been
  measured by anyone, here or elsewhere. "19 sampled hit positions", "7 × 7",
  "65,536 velocity steps" and "127 dynamic layers" are all vendor claims relayed
  at second hand, and the Soundpaint criticism above is the only public evidence
  that one of those claims does not deliver what it sounds like.
- **Non-English sources.** Only English-language search was done. Sonica's own
  development blog, Japanese retail listings and Japanese forum opinion were not
  searched and would be the obvious next place to look.
- **All the acoustics references listed above**, which remain unopened.

**Effect on the steps: none.** No step was added, reordered or struck as a
result of this research, and the five that survived review are left exactly as
they were. The reasoning is worth stating so that it can be disagreed with.
Nothing found suggests any of the five chases a property the category has
abandoned — they are internal-physics steps, and the field's stated values
(dynamic resolution, variation under repetition, believable ensemble and
perspective) have not moved at all since the first pass. The two capabilities
that this research does confirm are table stakes — several microphone
perspectives, and an ensemble that is a spread of drums rather than a chorus —
are both already recorded under "What was investigated and not done" from the
first pass, with reasons that this research does not weaken: the ensemble needs
a build environment for the plugin target that this work does not have, and a
room perspective needs impulse responses this repository cannot obtain. The new
evidence strengthens those entries rather than reopening them, and the Unity
control and the "mixing solo recordings is not an effective trick" line are
recorded above so that a future pass with a working build has the shape of what
to aim at. One finding actively supports the premise the steps serve: the
field's answer to expressive resolution is interpolation between recorded
layers, and the loudest published complaint about it is that listeners hear the
interpolation as volume rather than as timbre — which is the exact thing a
contact-law model gets for free and the reason these steps are spent on the
physics rather than on layer counts.

### Where the engine actually stands

Measured on the shipping code at 48 kHz, factory settings, Humanise 0 unless
stated. Gap numbers below are local to this pass.

**How these were re-measured.** Every number in this section and in the steps
below was re-taken during review with standalone programs linked against
`TaikorDSP`, not against the suite: band levels by Parseval over integer DFT
bins of a fixed-duration window (so a band level is the same quantity at every
sample rate), decays by a fourth-order Q=4 band-pass, and the drum's branch
frequencies from `TaikoEngine::measure`. Four of the steps were additionally
**prototyped** on a scratch copy of `Source/DSP/` and measured before and after,
because whether the region they touch can be moved at all is not a question the
source answers. Where the review's number differs from the one the pass was
written with, the review's number stands and the difference is called out.

1. **The head's high-frequency continuum falls with the host sample rate.**
   `TaikoEngine.cpp:1936`, `entry.level = continuumCalibration * edgeBoost * tilt
   * membranePeak / sqrt(variance)`. `membranePeak` is taken from `mode.drive`
   (`:1553`, `:1653`), and `drive` carries `/ rate` (`:1514`, `:1613`) because it
   is a per-sample integration gain for a resonator. The continuum is not
   integrated — it multiplies a band-passed noise whose variance is already
   normalised to unity — so it inherits a spurious 1/sampleRate. Measured, 4–10
   kHz band of a Don at velocity 0.92 over a fixed 85 ms window, relative to 48
   kHz: 44.1 kHz +0.60 dB, 88.2 −5.25, 96 −6.05, 192 −8.13, 384 −9.77. The first
   doubling tracks the 1/rate prediction (−6.02) and the ones above it do not,
   because by 192 kHz the click and contact-noise path — which has its own,
   smaller rate dependence — has become the floor of that band: a Bachi, which
   has no continuum at all, moves −2.11 dB at 96 kHz and −5.51 at 192 in the same
   band. So the honest statement of the defect is that the continuum itself loses
   6 dB per doubling and the audible consequence saturates at about 9 dB. Over
   400 Hz–16 kHz a Don loses 4.43 dB from 48 to 96 kHz; Katsu (membraneGain 0.06)
   moves −0.00 dB and Bachi +0.14 dB, which is the bisection. The 40–200 Hz band
   moves 0.24 dB across 44.1–192 kHz. A session moved from 48 to 96 kHz is a
   different instrument, and `testEveryArticulationAndSampleRate` and
   `testSampleRateConsistency` only check finiteness, audibility, clipping and
   pitch, so nothing catches it.

2. **The continuum's upper bands are inaudible underneath its lowest one.**
   This is the finding that replaces the ceiling complaint the pass was written
   around, and it disposes of three of its steps. `TaikoEngine.h:301` fixes
   `continuumBandCount = 5`; `:1785` sets `first = max(highestResolved * 1.25,
   20)` and `:1807` places band centres at `first * 2^band`, so the top band
   centre is always 16× the crossover. Instrumented per octave (highest
   resolved / crossover / top centre): C1 79.4 / 99.2 / 1587 Hz; C2
   161.8 / 202.2 / 3235; C3 328.9 / 411.1 / 6578; C4 670.0 / 837.4 / 13399; C5
   1373.4 / 1716.8 / 27468 — above the Nyquist guard at `:1810`, so one band is
   silently dropped; C6 2849.9 / 3562.4 / 56998, where two are.
   That much of the original reading is right. What is wrong is the assumption
   that the ceiling is what the listener is up against. Each band is a
   difference of two doubled one-poles, so its skirts fall at 12 dB per octave,
   and its level is `tilt / sqrt(variance)` with `tilt = first/centre` — the
   variance of a constant-Q band driven by unit white noise rising as its
   centre, so the level falls as roughly *f*^−1.5, measured 3.3× to 9.7× per
   octave. The lowest band therefore outruns every band above it in that band's
   own octave. Measured at C3, Don at velocity 0.92, 85 ms window, 4–10 kHz:
   the whole continuum gives −54.73 dB, the crossover band **alone** gives
   −56.19 dB, and the topmost band — the one that nominally belongs at
   6578 Hz — gives −63.55 dB on its own. Deleting the topmost band entirely
   moves 4–10 kHz by 0.5 dB. The continuum is not five bands; it is one band
   with a long tail and four decorations, and no per-band physics can be heard
   through it.
   The audible consequence at the ō-daiko end is narrower than the pass
   claimed. Full-velocity Don, 20 dB decay of a Q=4 band: at C1 the 1 kHz band
   rings 1056 ms, 4 kHz **484 ms** and 8 kHz 2.7 ms; at C2, 754 / 612 / 2.2 ms;
   at C3, 377 / 210 / 259 ms. The 4 kHz region on the largest drum is alive and
   is the hide — Head Damping 0 → 1 moves its level 300–350 ms after the strike
   by 17.2 dB. What is genuinely dead is 8 kHz: 2.7 ms of decay, and Head
   Damping changes that decay not at all (2.7 → 2.7 ms).

3. **Everything above about 300 Hz is a fixed-shape burst, so Mic Distance
   moves its level and very little of its shape.** `:1936` is all scalars; the
   only per-stroke shaping is the single `1/(1+(centre·tau)²)` at `:2316-2321`.
   The frequency-dependent microphone terms — `nearField` (`:1525-1531`,
   `:1622-1628`) and `proximity` (`:1539-1541`, `:1630-1632`) — exist only
   inside `voice.modes`, so the whole region inherits the distance law of a
   51 Hz mode as a flat gain. Measured, Mic Distance 3 cm → 40 cm, 85 ms
   window: 400–1200 Hz falls 17.23 dB and 4–10 kHz falls 13.52 dB, so the
   internal tilt moves 20.99 → 17.28 dB. That is 3.71 dB, not the 0.11 dB this
   pass was written with, and it moves in the direction opposite to the one a
   distance control should: backing the pair off currently makes the drum
   *relatively brighter*. The number is window-dependent — 0.85 dB over 20 ms,
   3.71 over 85 ms, 3.38 over 300 ms — which is itself a warning about
   measuring this region with a single window. Strike Position −1 → +1 moves
   the level 12.35 dB for 3.58 dB of tilt; Head Material 0 → 1 moves it 9.87 dB
   for 2.14 dB. The README claims more than this in as many words: "Backing the
   pair off narrows it and softens the slap at the same time, because that is
   one mechanism and not two."

4. **The attack glide transfers no energy up the spectrum — but it does inject
   broadband noise, which is worse.** See gap 14. `applyTensionShift`
   (`:2497-2542`) multiplies every membrane mode's ω by one scalar and rebuilds
   the coefficients while leaving `y1`/`y2` alone, which is exactly
   energy-conserving in the continuous limit and is not what the discrete
   filter does. Measured at velocity 1.00 over an 85 ms window, tensionMod
   0.00/0.50/1.00: 40–125 Hz −18.29/−18.29/−18.02 dB, 400–1200 Hz
   −32.20/−30.82/−30.53, 1200–4000 Hz −39.48/−38.15/−38.67, 4–10 kHz
   −51.91/−51.11/−51.29 — non-monotone and inside the ±1.7 dB that modes
   crossing band edges under a 55-cent bend explain by themselves. Over the
   velocity range 0.01 → 1.00, 4–10 kHz gains 5.17 dB relative to 40–125 Hz for
   a 25.30 dB change in level. A hard stroke on Taikor is the same stroke
   louder, shorter and briefly sharper; the one thing that does change its
   character above 1 kHz is an artefact.

5. **The stick is a point force, not a contact patch whose radius grows with
   the force.** The modal drive is `besselJ(order, besselZero * rho)` at a
   single radius (`:1429-1446`); there is no spatial average over the contact
   area anywhere. The engine's one patch term, `patchCorner = 3500 * (0.55 +
   0.75 * bachiHardness)` Hz at `:2420`, is applied only to the airborne
   click's radiation, never to the modal or continuum drive, and does not read
   velocity at all. Measured: a Don at C3 gains 24.15 dB in a 3.6–4.4 kHz band
   between velocity 0.2 and 1.0 and 20.56 dB in 40–125 Hz. The defect is real
   and, as gap 2 explains, unreachable: the Hertz patch radius runs from
   0.73 mm to 7.35 mm across the hardness and velocity ranges, and the largest
   spatial wavenumber anything audible in this engine carries — the crossover
   band's ω/c at C3 — puts *k_s a_c* at 0.21, worth 0.09 dB.

6. **Only one of the two bodies in the collision is allowed to ring.**
   `strikeProfile` (`:417-432`) sets `usesDrumBody = true` for all seven strokes
   that touch the drum, and the six wooden resonator slots are therefore the
   shell's; the stick's free-free bending series is reached only when
   `usesDrumBody` is false (`:1685`, `:2276`), which is the Bachi stroke alone —
   the one stroke that never touches the drum. So on a Katsu (oak on zelkova)
   and a Don Rim (oak on hoop) the engine renders the struck body and never the
   striker, even though it already carries a correct free-free bar model
   (*f_n = (β_n L)² κ √(E/ρ) / 2πL²*, series 1 : 2.757 : 5.404 : 8.933, at
   **497.7 / 1371.9 / 2689.6 / 4446.0 Hz** for the octave-0 bachi at the factory
   Bachi Hardness of 0.7 — not the 534 / 1473 / 2887 / 4776 this pass was
   written with, which are about 7 % sharp and belong to no setting of the
   control) and even though the contact it solves is symmetric in the two
   bodies. Note also that the contact is *not* solved against what is struck:
   `drumContactTerms` (`:1230-1238`) returns the head's impedance
   *8√(Tσ)* for all seven, so a bachi on oak and a bachi on hide separate in
   1.018 ms and 1.193 ms respectively — a 17 % spread, corners 982 and 838 Hz.
   The engine cannot currently tell a stick what it hit.

7. **A keyboard octave is not an octave in the partial the drum sounds, and the
   defect is the Octave Body transform.** `:1047-1048` sets
   `cavityStiffness = cavityCoupling · ρc² / depth`; the axisymmetric pair is
   solved at `:1446-1465` and `:582-628`; the octave transform and the Pitch
   map are at `:909-934`, and both hold `idealFundamentalHz` — a pre-air-load
   quantity that is never audible on its own. The loaded fundamental's octave
   steps, C1 upward: at Octave Body 0.00, 1199.9 / 1199.7 / 1199.0 / 1196.2 /
   1191.2 cents — already right; at 0.70 (the factory setting), 1409.5 / 1359.6
   / 1314.6 / 1276.9 / 1246.4; at 1.00, 1545.3 / 1443.2 / 1352.7 / 1286.4 /
   1243.9. **The worst error is 345 cents, not 209**, and the whole of it is
   bought by Octave Body: the air load depends on *ρ_air a / σ* and does not
   scale with a transform that changes *a*. Pitch is not implicated — Pitch
   ±12 st already moves the loaded fundamental by −1199.7 and +1199.0 cents.
   The breathing mode is a separate and larger failure: it steps 509.4 / 635.7
   / 829.5 / 1019.3 / 1139.8 cents at Octave Body 0.70 and 696.3 / 801.1 /
   929.4 / 1040.8 / 1116.3 at 1.00, and Pitch ±12 st moves it by −248.6 and
   +600.0. And the loudest partial is on neither branch at the bottom of the
   keyboard: scanning 8–900 Hz of a Don at velocity 0.92, 50 ms after the
   strike, the strongest partial per octave is 58.50 / 42.75 / 89.50 / 109.00 /
   227.25 / 466.25 Hz — steps of **−543.2 / +1279.9 / +340.4 / +1272.4 /
   +1244.2 cents**. At C1 and C2 it belongs to neither axisymmetric branch (it
   is a mode with a circumferential order, woken because a Don lands off
   centre); at C3 it is the breathing mode, 5.0 dB above the loaded fundamental
   in a 16384-sample window; from C4 up it is the loaded fundamental.
   `testOctavesRaisePitch` asserts only that `idealFundamentalHz` doubles and
   confines its audio search to ±12 % of the loaded fundamental, with a comment
   recording that a wider sweep "reported the two as a fifth apart rather than
   an octave".

8. **The enclosed air is a lossless spring of infinite extent.** `:1047-1048`
   is the low-frequency limit *ρc²/L* of a cavity, which is exact only while
   the cavity is short against the wavelength; the README already concedes that
   *c/2L* is 212 Hz on the factory drum and runs from 139 to 451 Hz across Body
   Depth, so the assumption gives out inside the drum's own range. Solving the
   exact half-column stiffness *x·cot x* self-consistently against the branch it
   sets, *x = ωL/2c*: the factor is 0.868 at the factory drum, 0.782 at
   octave −2, and 0.688 at octave −2 with Body Depth 1 — **not the 0.31 this
   pass was written with**, which no reachable configuration produces, because
   the correction lowers the frequency, which lowers *x*, which raises the
   factor back. Nothing anywhere associates a loss with the enclosed air. Body
   Depth therefore moves the pitch of the split and never the decay of either
   branch: a shallow drum and a deep one have exactly the same boom length at
   the same tuning.

9. **The modal bank hands over to noise nearly an octave below where the head's
   modes actually overlap.** `:1785` starts the noise at 1.25× the highest
   resolved mode; the mode table (`:362-383`) stops at Bessel zero 12.225.
   Solving *(modal density) × (half-power bandwidth) = 1* with the engine's own
   `edgeLoss`, `headLossFactor` and `headViscousFactor`: at the factory drum the
   bank stops at 307.7 Hz, the noise starts at 385 Hz and true overlap is at
   566.5 Hz — 0.88 octaves short, with about 127 modes below overlap of which 40
   are modelled. At octave −1 it is 153.9 against 318.1 Hz (1.05 octaves, ~160
   modes) and at octave −2, 76.9 against 159.8. The audio agrees: spectral
   peak-to-median is 24.81 / 25.75 / 21.10 dB in the 40–80 / 80–160 / 160–320 Hz
   bands and collapses to 9.69 dB in 320–640 Hz, and the stroke-to-stroke
   log-spectrum correlation of six nominally identical Dons is +0.928 / +0.867 /
   +0.386 below 320 Hz and +0.041 in 320–640 Hz. Above the crossover the drum
   has no fixed identity at all. (Carried forward from the pass unre-measured;
   it drives no step.)

10. **CC1, "a hand laid on the head", is a flat gain envelope rather than a
    damper.** `:2991-2992` computes `handRate = 12 · handDamping²` and
    `handStep = exp(−handRate/rate)`, applied identically to every membrane
    mode and every continuum band at `:3031-3032`, `:2661-2680` and
    `:2928-2929`. Measured at CC1 = 1.00, attenuation over an 85 ms window
    beginning 100 ms after the strike: 50–150 Hz −10.13 dB, 400–1200 −10.59,
    1200–4000 −11.26, 4–10 kHz −11.50 — a 1.37 dB spread over seven octaves.
    The engine's own articulation mute is physical by contrast: Tsu minus Don
    gives −6.26 / −12.45 / −12.52 / −12.41 dB, a 6.19 dB tilt, because
    `profile.muteAmount` feeds `extraDamping` into `edgeLoss` and both terms of
    `materialDamping`. Two mechanisms sit side by side and only one is a
    damper.

11. **An articulation's loudness is a gain applied after the contact is
    solved.** `:2301`, `excitationScale = peakForce * profile.levelScale`, with
    `levelScale` from the table at `:417-432`. It cannot reach the contact
    duration, the continuum's corner or the click's slope. Su's 0.24 is
    −12.40 dB of pure gain; its contact at velocity 1.00 is 1.149 ms where the
    impact speed that would actually produce that level (×0.304) gives
    1.458 ms, a 27 % error. Confirmed in audio: a Su at v = 1.00 and a Don at
    v = 0.310 matched to 0.02 dB of 85 ms RMS differ by +9.3 dB in 400–1200 Hz
    and +9.3 dB in 4–10 kHz. The README says the eight strokes "are not eight
    presets"; on this one axis they are.

12. **Strike Position has bit-identical dead travel and puts three strokes on
    the geometric centre.** `:2244-2247` adds `strikePosition * 0.32` to a
    profile radius that ranges from 0.15 to 0.99 and then clamps to [0, 0.985].
    `max|render(−1.00) − render(−0.50)|` is exactly 0.000e+00 against a peak of
    0.215, and −0.47 is identical too. Dead travel, re-derived from the profile
    table: Katsu 50.7 %, Don Rim 47.8 %, Ka 38.3 %, Don 26.9 %, Tsu 18.9 %, Su
    and Buzz 0 %. Don reaches radius 0 at strikePosition −0.469 — the engine's
    own comment at `:396-404` explains why no stroke lands there.

13. **Voice stealing cuts the stolen voice dead.** `:2218-2220` calls
    `findVoiceSlot()` then `silenceVoice`, which zeroes every resonator state
    (`:824-878`), while `forcedFadeSeconds` and `retireGain` exist precisely to
    fade a voice that reaches the tail cap. With 16 voices and a 2.07 s T60 the
    pool saturates at about 16 strokes per second. The pass measured the
    instantaneous discontinuity at 0.008563 against a running peak of 0.2233;
    review could not isolate that event again, but it did establish the scale it
    has to be judged against — the ordinary onset of a Don produces one-sample
    steps of 0.128 against a running peak of 0.341, 8.5 dB down. Whatever the
    steal's step is exactly, it is well under the ones the instrument makes on
    purpose.

14. **The attack glide sprays broadband noise across the top of the spectrum.**
    Found during review; not in the original reading, and it is the reason step
    6 of this pass was struck. `applyTensionShift` rewrites `a1`, `a2` and `b0`
    on every running membrane resonator whenever the shift moves by more than
    1e-5 (`:2727-2729`), which during the attack is every control period, while
    holding `y1`/`y2` fixed. Each rewrite steps the resonator's output, and the
    train of steps is broadband. Measured, Don at velocity 1.00, band levels
    over 30–80 ms after the strike, Tension Mod 0 → 1: 1.2–2.4 kHz +7.15 dB,
    4–10 kHz +7.23 dB, 12–20 kHz +7.24 dB. Flat to a tenth of a decibel across
    four octaves, with the continuum switched off entirely and with no modelled
    membrane content above 330 Hz on this drum: nothing physical in this engine
    is flat to 20 kHz. At the factory Tension Mod of 0.4 it is already worth
    about 6 dB. It is currently the largest single source of
    brightness-with-dynamics the instrument has above 1 kHz thirty milliseconds
    after a hard stroke, and it is a filter artefact.

**What must not regress.** The resolved bank is in good order and the surviving
steps run straight through it. Velocity is monotone over 41 steps from
−41.65 dB to −15.97 dB with no plateau and contact time falling smoothly from
83.7 to 51.0 samples. Two identical MIDI notes really do differ: eight identical
Dons span 3.36 dB of peak and correlate at +0.71 to +0.93; four Buzz strokes
span 5.6 dB. Bachi Hardness moves 400–1200 Hz by 28.6 dB over a 300 ms window at
full velocity while moving 40–125 Hz by 0.5 dB — the figure is strongly
window-dependent (20.8 dB over 85 ms) and any test of it must fix the window.
The ADAA output stage agrees between 48 and 384 kHz to within 0.05 dB from
100 Hz to 6 kHz at Drive 1.0. The resolved bank's tuning is rate-independent to
0.24 dB across five rates. The attack is click-free and the idle path writes
exact zeros. Below the crossover the drum is an object: peak-to-median 24.8 /
25.8 / 21.1 dB and stroke-to-stroke correlation +0.93 / +0.87 in the two lowest
bands. The DSP suite is green on the tree this review was taken on (2 tests,
10.6 s). Every step below states what it may not move.

### The steps

Each step is a single commit. The DSP suite must be green before each one, and
each lands with a test in `Tests/` that fails without it. Five steps survived
review; three did not, and are recorded with the rest of the rejects below.

- [ ] **1. Make the head's continuum independent of the host sample rate.**
  `mode.drive` is a per-sample integration gain and carries `1/rate` so that
  the resonator it feeds integrates a force; the continuum multiplies a
  variance-normalised noise sequence and integrates nothing, so calibrating it
  against `membranePeak` hands it a sample rate it has no use for. The change
  is to take `membranePeak` from `|mode.drive · rate · mode.micLeft|` rather
  than from `|mode.drive · mode.micLeft|` — that is, to measure the continuum
  against the rate-free modal receptance `shapeStrike · batterShare /
  (geometricMass · ω)`, a velocity per unit force, *observed through the same
  microphone factor it is observed through today* — and to re-anchor
  `continuumCalibration` from 26.0 to 26.0 / 48 kHz = 5.42e-4 s, which is the
  same number in the units the receptance now has. The microphone factor must
  stay: dropping it, which the first draft of this step read as doing, would
  change the 48 kHz level and would take the continuum's only distance
  dependence away with it. No behaviour at 48 kHz changes; the rate dependence
  disappears. Closes gap 1. *Verified by*:
  `testTheContinuumDoesNotDependOnTheSampleRate` renders a Don at velocity
  0.92, Humanise 0, at 44.1, 48, 96 and 192 kHz over a fixed 85 ms window with
  the band level taken by Parseval over integer DFT bins of that window — the
  same physical quantity at every rate, which a time-domain RMS through a
  fixed-Hz filter is not — and requires the 4–10 kHz band to agree across all
  four rates within **2.0 dB**; today the spread is 8.73 dB (+0.60 to −8.13
  relative to 48 kHz). The tolerance is 2.0 and not 1.0 because the step was
  prototyped: with the change applied, the four rates measure +0.03 / 0.00 /
  −1.51 / −0.85 dB, the residual being the click and contact-noise path, which
  this step does not touch and which a Bachi shows on its own at −2.11 dB by 96
  kHz. It also requires the 40–200 Hz band to stay within its present 0.5 dB,
  so the fix does not reach the resolved bank, the 400 Hz–16 kHz band to agree
  within 1.5 dB across the four rates (prototyped at 1.34), and the 48 kHz 4–10
  kHz level to stay within 0.1 dB of its pre-step value.
  One clause added in preflight, because the step's own prose names a failure
  mode nothing was measuring. The microphone factor is what carries the
  continuum's distance dependence, and an implementation that re-anchored the
  calibration and replaced `mode.micLeft` with the constant it happens to take
  at the factory Mic Distance would leave the 48 kHz level untouched and pass
  every clause above. So the test also sweeps Mic Distance 0 → 1 (3 cm → 40 cm)
  at 48 kHz and requires the 4–10 kHz band to keep falling by **13.5 ± 1.5
  dB**, which is what it does today (13.53, against 17.23 dB at 400–1200 Hz and
  20.54 dB at 40–125 Hz). This clause cannot fail today and is carried as a
  guard, not as a discriminator. The 0.1 dB level clause does most of the work
  on its own — `membranePeak`'s own `micLeft` is 0.3352 at the factory
  distance, so dropping it outright moves the 48 kHz level by 12.11 dB — but it
  only ever looks at one distance.

- [ ] **2. Stop the attack glide from spraying the top of the spectrum.**
  `applyTensionShift` rewrites each membrane resonator's coefficients in place
  and leaves its state alone. In the continuous problem that is
  energy-conserving; in a two-pole difference equation it steps the output by
  `Δa1 · y[n−1] + Δa2 · y[n−2]` at every rewrite, and the glide rewrites on
  every control period through the whole attack. The result is a step train,
  and a step train is white. Rotate the state into the new tuning instead of
  leaving it — the resonator's `(y1, y2)` pair is a sampled sinusoid whose
  phase and amplitude are recoverable, and re-deriving `y2` for the new pole
  angle at the same amplitude and phase makes the coefficient change continuous
  in the output rather than only in the poles. The alternative — crossfading
  two resonators — doubles the bank and is not worth it for a term that is a
  few tens of cents. Nothing about the glide's size, shape or timing changes.
  Closes gap 14, and it must come before anything else that measures this
  region, because at the factory Tension Mod every band above 1 kHz is
  currently sitting on about 6 dB of this.
  *Verified by*: `testTheGlideRetunesWithoutSplattering` renders a Don at
  velocity 1.00, Humanise 0, at Tension Mod 0 and 1, **with the voice's
  continuum silenced** — a test-access hook that zeroes the voice's
  `continuum[*].level` after `trigger` and before the first block, which holds
  for a Don because a Don schedules one contact and the relight at
  `:2758-2762` moves `band.envelope` and not `band.level` — and measures three
  bands over 30–80 ms
  after the strike, past the contact entirely, where the drum at C3 has no
  modelled content at all. **Silencing the continuum is not optional and was
  missing from the first draft of this clause, which preflight corrected.** The
  +7.15 / +7.23 / +7.24 dB the pass recorded are the rises with the continuum
  off; through an ordinary public render the same three bands rise by
  +3.24 / +7.38 / +8.28 dB, so the flatness the assertion turns on is 5.04 dB
  wide rather than 0.09 and the test would have failed on the tree it was
  written against, before anybody touched the engine. With the continuum
  silenced the figures reproduce exactly.
  After the change each of the three must rise by less than 1.5 dB, and the
  12–20 kHz band, which no mechanism in the instrument is entitled to reach, by
  less than 0.5 dB. The flatness is the signature and the test asserts it as
  one: the spread between the three bands' rises must not exceed 1.0 dB today
  (it is 0.09) and the test would be meaningless without recording that,
  because a genuine mechanism would not be flat. The 40–125 Hz band over the
  same window must move by less than 0.3 dB (with the continuum silenced
  Tension Mod 0 → 1 moves it −0.03 dB; the +0.22 dB the first draft quoted is
  the same band with the continuum on, and the two must not be mixed inside one
  test). The pitch of the glide, measured through `appliedTensionShift`, must
  be unchanged sample for sample — which is what stops the rises being met by
  widening the 1e-5 rewrite threshold at `:2730` or by disabling the glide
  outright. And a Don at Tension Mod 0 must be **bit-identical** to before the
  step: at Tension Mod 0 the shift never leaves 1.0, so `applyTensionShift` is
  never called at all, and that identity is what stops the rises being met by
  filtering the top off the instrument.

- [ ] **3. Ring the striker as well as the struck.** A contact force acts
  equally and oppositely on both bodies (Chatziioannou and van Walstijn 2015),
  so the bachi's own free-free bending modes are excited by every stroke, not
  only by the one that has no drum in it. Give the seven `usesDrumBody` strokes
  the stick's series alongside the shell's, driven by the same contact force
  through the stick's own modal mass and carrying the grip resistance the stick
  model already has. Two corrections to how this pass first stated it, both
  found by reading the source. `stickResonatorCount` is **6**, not 3, and a
  `static_assert` at `TaikoEngine.h:308` ties it to the shell's count because
  today the stick *shares* the shell's slots; the per-voice resonator count
  therefore goes from 46 to 52 unless the stick bank is deliberately truncated,
  and it may not be truncated to three, because the fourth mode — 4446 Hz at
  octave 0 — is the one that makes a bachi sound like oak.
  And the claim that no shaping is needed, because "a soft long contact on hide
  barely drives a 2.9 kHz bending mode while a hard short one on oak drives all
  four", is **false**: `drumContactTerms` returns the *head's* impedance for
  all seven strokes, so a Katsu on the shell and a Su on the hide separate in
  1.018 ms and 1.193 ms — corners 982 and 838 Hz, a 17 % spread, not a factor.
  The contact pulse cannot tell those two strokes apart and the step must not
  claim it does. What does separate them honestly is the excitation the stroke
  already carries: Katsu's `peakForce · levelScale` is 10.65 dB above Su's
  (1983.7 against 582.2 at velocity 1.00 and octave 0, read from
  `voice.contactReference`), and the stick, being driven by the contact force,
  inherits exactly that. What the step *adds* is that and nothing more.
  What it also has to **decide**, in the same commit, is where the new modes
  sit in mode retirement: they must be kept out of the `peakMagnitude` that
  sets it at `:1950`, or the reference must be taken per family. That quantity
  is a maximum over the whole bank — membrane at `:1561` and `:1659`, wood at
  `:1747` — and it is the denominator of the `relative` that becomes every
  mode's `retirementLog` and `audibleSamples` at `:1954-1963`. Admitted at the
  level the Bachi stroke already gives it, the stick bank's peak
  `|drive · mic|` is 4.08e-05 at octave 0 and the factory Bachi Hardness,
  against 2.31e-05 for a Don's loudest membrane mode and 1.07e-05 for a Su's:
  that raises `peakMagnitude` by 4.94 dB on a Don and 11.65 dB on a Su and
  takes 0.082 and 0.194 of each membrane mode's own T60 off its scheduled life,
  and drops outright any mode already within that much of the 1e-7 retirement
  floor. Closes gap 6.
  *Verified by*: `testTheStrikerRingsAsWellAsTheStruck`. The two discriminating
  assertions are read from the built bank rather than from the audio, because a
  spectrum of the first ten milliseconds cannot separate the stick's
  contribution from the shell's — they overlap in every one of these bands, and
  which of them a band contains moves with Shell Material.
  **Structure.** `TaikoEngineTestAccess::woodFrequencies` for a Katsu must
  return **twelve** entries where it returns six today: the shell's six and the
  stick's six at the frequencies `resolveStickFor` actually produces at the
  factory Bachi Hardness — **497.71 / 1371.95 / 2689.56 / 4445.99 / 6641.54 /
  9276.20 Hz** at octave 0 — read from the engine rather than written down, so
  that moving the hardness control moves the test with it.
  **Independence.** The six stick modes' `|drive · micLeft|` must agree within
  0.1 dB between Shell Material 0 and 1, while the shell's six move their whole
  set from 56.78 / 160.60 / 307.94 / 498.00 / 730.56 / 1005.52 Hz to 176.67 /
  499.69 / 958.11 / 1549.47 / 2273.05 / 3128.56 Hz across the same sweep. That
  is a component that does not read the drum. (The first draft asked for the
  shell's first ring frequency "reported by `measure()`"; `DrumMeasurements`
  reports no such quantity, and the seam that does is `woodFrequencies`.
  Corrected in preflight.)
  **One force.** The stick's modes must be identical between a Katsu and a Su,
  so that the whole difference between the two strokes' stick components is
  `voice.contactReference`: 1983.7 against 582.2, a ratio of **10.65 dB**,
  which the test asserts to 0.2 dB.
  **In audio**, a Katsu's summed level in four ±10 % bands around the first
  four stick frequencies over the first 10 ms must rise by at least 6 dB
  against the pre-step render, and the Bachi stroke must be bit-identical to
  before. The window and its shape have to be pinned in the test and the
  pre-step baselines re-taken with that estimator, the way this section already
  insists for Bachi Hardness. The −25.36 dB at Shell Material 0 and −36.12 at 1
  the first draft quoted were taken with an estimator the step did not record
  and preflight could not reproduce: the same four bands read −24.07 and
  −33.79 over a rectangular 10 ms, −24.02 and −36.26 over a rectangular 20 ms,
  and −38.49 and −35.84 over a Hann-tapered 10 ms. Fourteen decibels of that
  spread is the window alone.
  **Two clauses of the first draft are struck, both corrected in preflight.**
  It required Katsu's and Su's rises to agree within 1.5 dB, and the step's own
  mechanism forbids that as surely as it forbade the clause that one replaced.
  A Su's existing content in these bands is 18.0 dB below Katsu's at Shell
  Material 0 over the first 10 ms (23.1 dB over 20 ms, which is where the
  pass's 23.3 came from) while its stick excitation is only 10.65 dB below, so
  if Katsu rises the required 6 dB the same two ratios put Su at about 12.4 dB.
  What the two strokes share is the stick, not the rise, and the stick is
  compared at the bank seam above; in audio the test asserts only the direction
  those ratios force, that Su's rise is the larger of the two. It also required
  the summed level to agree within 1.5 dB between Shell Material 0 and 1, which
  is a statement about the shell as much as about the stick — it can only be
  met by a stick loud enough to bury a shell that today differs by about 10 dB
  between those two settings, and that is a level requirement nobody derived.
  The stick's independence of the drum is asserted at the bank seam instead.
  **The membrane-lifetime guard is replaced, and this is the one that could not
  bite at all.** The first draft asked that
  `TaikoEngineTestAccess::membraneT60s` return the same membrane modes as
  before the step, and gave the right reason — a loud new stick would retire
  head modes as a side effect. But `membraneT60s` returns
  `6.9078 / mode.decayRate`, and `decayRate` comes from the head's loss model
  and never from `peakMagnitude`, so the quantity is unchanged by construction
  no matter how far the retirement is cut. Measured directly: moving Shell
  Resonance 0 → 1 on a Katsu raises `peakMagnitude` by 7.78 dB and leaves the
  summed
  `membraneT60s` at 95.0102 s to every printed digit, while the summed
  scheduled lifetime falls from 56.78 s to 49.51 s and `voice.maximumSamples`
  from 4.065 s to 3.661 s. What the test must read instead is the schedule
  itself: a new `TaikoEngineTestAccess::membraneAudibleSeconds`, returning each
  membrane mode's `audibleSamples / rate`, together with `activeModeCount`. For
  a Don, a Su and a Katsu at velocity 1.00 those must be unchanged within 2 %
  of their pre-step values, no membrane mode may fall from a non-zero lifetime
  to zero, and `activeModeCount` must stay at its present 29 / 30 / 30. Keep
  the `membraneT60s` clause as well — it still guards `decayRate` — but it is
  not the guard this paragraph is about.

- [ ] **4. Give the enclosed air the impedance of a finite column instead of an
  infinite spring.** `ρc²/L` is the *ω → 0* limit of a cavity, and the README
  already records that the limit fails inside the drum's own range. The exact
  input stiffness of a rigidly terminated column of length *ℓ* driven at one
  end is *ρcω·cot(ωℓ/c)*; the volume-changing motion of a two-headed drum is
  symmetric about the midplane, so each head sees *ℓ = L/2*, and the stiffness
  entering the two-by-two becomes *k(ω) = (ρc²/L)·x·cot x* with *x = ωL/2c*.
  That is exactly the present value at *x → 0* and falls away as the drum gets
  deep relative to the wavelength. It makes the eigenvalue problem implicit, so
  solve it by damped fixed-point iteration from the lumped value at
  drum-resolve time — never in the render loop. Solved that way to convergence,
  the factor is 0.868 at the factory drum (breathing 88.10 → 84.12 Hz, −4.5 %),
  0.782 at octave −2 (45.47 → 40.50 Hz, −10.9 %) and 0.688 at octave −2 with
  Body Depth 1 (37.26 → 31.43 Hz, −15.7 %). It is *not* 0.31 anywhere: the
  iteration is self-limiting, because lowering the stiffness lowers the
  frequency, which lowers *x*, which raises the factor back.
  The clamp needs care and the first draft's was unsafe. Clamping *x* below
  *π/2* and letting the factor go to zero above it decouples the two heads, and
  a parameter scan (Body Depth × Head Material × Tension × Air Coupling ×
  Head Diameter × Octave Body × octave) finds configurations where the fixed
  point converges there: at Body Depth 0, Head Material 0, Octave Body 0, Air
  Coupling 0.2, octave +3 it converges to 0.0002, the breathing mode lands at
  560.4 Hz and the loaded fundamental at 562.8 — **the breathing branch falls
  below the other one**, and `testOctavesRaisePitch`'s "the cavity must lift
  the volume-changing mode above the other one" fails. Either the factor keeps
  a floor, or that assertion and the `DrumMeasurements` contract behind it
  change in the same commit. Deciding which is part of the step.
  This is the reactive half of gap 8: the biggest drums stop being air springs
  with a hide attached and their breathing branch comes down. It is *not* the
  physics half of gap 7 — see step 5.
  *Verified by*: `testTheCavityIsAColumnNotAnInfiniteSpring`. The "reported
  stiffness factor" it reads does not exist yet; the step adds it to
  `DrumMeasurements` alongside the two branch frequencies, because the whole
  point of the change is a number the drum resolve now has to converge on and
  every assertion below is about it. It requires that factor to be **within 8 %
  of 1** where the cavity is
  shortest against the wavelength (Body Depth 0, octave +3, otherwise factory:
  it is 0.941 there, so the first draft's 3 % could not be met), to fall below
  0.80 at octave −2 at factory Body Depth and below 0.72 at octave −2 with Body
  Depth 1, and never to reach zero anywhere in the parameter scan above unless
  the step has decided that a decoupled pair is the reported answer, in which
  case the test must say so and `testOctavesRaisePitch` must be updated in the
  same commit. The reported breathing mode at octave −2 must fall by at least
  9 % at factory Body Depth and 14 % at Body Depth 1. The breathing branch's
  octave steps at Octave Body 0.70 must widen at every one of the five
  boundaries, from 509.4 / 635.7 / 829.5 / 1019.3 / 1139.8 cents to at least
  560 / 660 / 845 / 1030 / 1145 — the prototype gives 582.4 / 682.8 / 859.5 /
  1038.5 / 1153.1, so the flattest becomes 582 cents and not the "about 730"
  this pass was written with, and a test asserting 700 would fail. At Body
  Depth 0, octave +3 the breathing mode must move by less than 1 % (prototyped
  at 0.5 %), because a short cavity was already nearly right.
  Two clauses added in preflight, because the step is the reactive half of one
  branch and nothing was holding the other one still. First, the **lower**
  branch: the reported loaded fundamental must move by less than 1 % anywhere
  in the parameter scan, which is the step's own prototyped worst case of 0.47
  % with room to spare. Without it an implementation that dragged both branches
  down together would meet every breathing-mode clause above, and step 5 —
  which solves the keyboard against that lower branch — would be built on a
  moving quantity. Second, **convergence**: two identical `measure()` calls
  must return bit-identical factors, and the factor must be monotone in Body
  Depth at fixed octave. A damped fixed point that has not converged is a
  function of its iteration count rather than of the drum, and every number
  above would then be a number about the solver.

- [ ] **5. Hold the drum's own fundamental on the keyboard, not the ideal
  membrane's.** The octave transform and the Pitch control both scale radius
  and tension so that `idealFundamentalHz` doubles, and that quantity is never
  audible on its own: the air load hangs off the real mode and depends on
  *ρ_air a / σ*, which does not scale with the transform. Solve the transform
  against the loaded lower axisymmetric branch instead, by bisection on the
  same size-and-tension mixture Octave Body already chooses (the branch is
  monotone in both, so the solve is well posed and runs at drum-resolve time
  only). This is the principle the head stiffness already follows and the
  README already states — "a drum is tuned by the pitch it sounds" — applied to
  the term that was left out of it.
  Two corrections to the reason the pass gave. The defect is **entirely** the
  Octave Body transform: at Octave Body 0 the octave steps are already 1199.9 /
  1199.7 / 1199.0 / 1196.2 / 1191.2 cents, and Pitch ±12 st is already good to
  1 cent. And the step does **not** close the musical half of gap 7 as that gap
  is written, because the loudest partial is not the loaded fundamental below
  C4: measured, it is 58.50 / 42.75 / 89.50 Hz at C1 / C2 / C3, on neither
  branch at the bottom two and on the breathing branch at C3, where it sits
  5.0 dB above the fundamental. What this step buys is precise and worth having
  on its own — the drum's own lowest mode lands on the key that names it, at
  every setting of Octave Body — and the step must claim that and not more.
  It does not have to come last: the cavity moves the *upper* branch, and step
  4 changes the loaded fundamental by 0.004 % at the factory drum and by at
  most 0.47 % anywhere measured. The two are independent and either order
  works. *Verified by*: `testTheDrumIsTunedByThePitchItSounds` requires the
  reported loaded fundamental to double within ±20 cents at every octave
  boundary from C1 to C6 at Octave Body 0, 0.7 and 1.0 — the worst error today
  is **345 cents**, at Octave Body 1.0, not the 209 this pass was written with,
  which is the worst at the factory 0.7 alone. It requires Pitch ±12 st to move
  it by 1200 ± 20 cents; that clause cannot fail today and is carried as a
  guard against the bisection breaking it, which the test says in as many words
  so that nobody reads it as a discriminator. In audio, the DFT magnitude at
  the reported loaded fundamental must remain a local maximum within ±6 % at
  every octave, and — the part that would actually have caught this — the ratio
  of the strongest partial found by scanning 8–900 Hz between adjacent octaves
  must be recorded at every boundary and must not get *worse* than today's
  −543.2 / +1279.9 / +340.4 / +1272.4 / +1244.2 cents, **with 20 cents of
  slack** — which strongest partial wins a scan can flip on a fraction of a
  decibel, and without the slack this clause fails on jitter rather than on the
  step. (The ±6 % local maximum holds today at every octave, to within 0.25 dB
  of the peak inside that band, so it is a do-no-harm clause and the test
  should say so.)
  Two clauses added in preflight, because the assertions above are all about a
  number the step is also free to redefine. **The anchor**: the octave transform
  is the identity at octave 0 for every Octave Body — `radiusFactor` and
  `tensionOctaveFactor` are both 1 there — so the reported loaded fundamental at
  octave 0 must stay at today's **50.7490 Hz** and must be identical across
  Octave Body 0, 0.7 and 1.0, as it is today. A bisection that met the octave
  ratios by moving the whole keyboard would pass every other clause.
  **Octave Body must keep its meaning**: the bisection runs on the mixture
  Octave Body already chooses, so at Octave Body 0 the reported radius must stay
  at 0.4750 m at every octave from C1 to C6 while the tension quadruples per
  octave, and at Octave Body 1 the reported tension must stay at 5942.4 N/m
  while the radius halves. Both hold today, and they are what stops the solve
  buying an octave on an axis the control does not own.
  `testOctavesRaisePitch`'s assertion on `idealFundamentalHz` is replaced by this
  one, and its comment recording that a wider audio sweep "reported the two as a
  fifth apart rather than an octave" stays, because the two branches are still a
  fifth apart and this step does not change that. The suite's other uses of
  `idealFundamentalHz` — the head diameter and tension controls, Pitch and the
  wheel, and `testTheHeadIsAStiffMembrane`'s identity that it is the ideal
  membrane frequency of the reported radius and wave speed — all survive,
  because the step changes what the octave transform solves for and not what
  the quantity means. `testOctavesRaisePitch`'s own "a higher octave must be a
  smaller drum" at the factory Octave Body has to survive the bisection too, and
  the test must be run to confirm it rather than assumed.

### Considered and not planned

**Taking the continuum's ceiling from the hide rather than the drum's size
(gap 2). Struck in review.** It was step 3 as the pass was first drafted. The
physics of the premise is right — membrane modal density is *2π a² f / c²*, so
a larger head has more unresolved short-wavelength modes at a given frequency,
not fewer, and five octave-spaced bands anchored at the crossover do encode
the opposite. The step was prototyped anyway: `continuumBandCount` raised to
10, bands run from the crossover to `min(20 kHz, 0.45 · rate)`. Measured
before and after at octave −2, full-velocity Don: the 20 dB decay of a Q=4 8
kHz band goes 2.7 ms →
**2.8 ms**, and the 8 kHz level 300–350 ms after the strike goes −83.83 →
−83.72 dB. Nothing moves, because the added bands arrive underneath the
crossover band's own 12 dB/octave skirt — at C3 the topmost band is already
8.8 dB below that skirt in its own octave, and deleting it entirely costs
0.5 dB. Two smaller faults were found on the way: the proposed count
`ceil(log2(ceiling/first)) + 1` overshoots the ceiling by one band at every
octave and hands that band straight to the Nyquist guard, which the step's own
"no band dropped while a slot is unused" clause forbids (it wants `floor`); and
the audio criterion it proposed — a 4 kHz band ringing longer than 150 ms at
octave −2 — already passes, at 484 ms. Relighting the top of the ō-daiko's hide
is still worth doing. It cannot be done by extending the extent at a fixed tilt,
and the tilt is the thing the README's third-octave calibration rests on, so it
needs a measurement first and a pass of its own.

**Giving the continuum a source size (gap 3). Struck in review.** It was step 4
as first drafted. The derivation is sound and the constant is not drawn: above
modal overlap the region moving in phase at *f* is about one membrane wavelength across,
*R(f) = c/2f*, and a source of radius *R* on axis at *d* gives
*p ∝ R/√(R² + d²)*. It was prototyped as a per-band factor, applied relative to
the crossover band so the calibration point does not move. Measured: the
Mic Distance tilt (400–1200 minus 4–10 kHz, 85 ms) moves from −3.71 dB to
−3.89 dB across the full sweep, against the 6 dB the step asked for. Same reason
as above — the region it shapes is the crossover band's skirt, and a per-band
gain cannot reach it. Two errors in the step's own numbers were found on the
way: the −22.1 dB it quoted for 4 kHz at 40 cm is arithmetically wrong (the law
gives −32.6 dB), and with the quoted figure the tilt would have moved the wrong
way; and the baseline it measured against, 0.11 dB, is really 3.71 dB and in the
opposite direction to the one the step wants. The mechanism is still the right
one. It becomes available the day the continuum's bands are separable — which
means steeper skirts, or a synthesis that is not a difference of one-poles at
all — and that is the prerequisite step, not this one.

**Letting the contact patch widen with the force (gap 5). Struck in review.** It
was step 5 as first drafted. Prototyped as *2J₁(k_s a_c)/(k_s a_c)* on every
continuum band with *k_s = ω/c* and *a_c = √R · (F/k)^(1/3)*: the 4 kHz gain
from velocity 0.2 to 1.0 moves from 24.15 dB to **24.13 dB**, against the 3 dB
the step asked for. The membrane half is a no-op by arithmetic and needed no
prototype: the highest resolved mode at C3 has *k_s a_c* ≈ 0.15, worth 0.05
dB, and the crossover band 0.21, worth 0.09 dB. Everything with a *k_s a_c*
large enough to matter is in a band nobody can hear. Two further objections
stand whatever happens to gap 2. "*E\** the effective modulus `solveContact`
already solves for" is not what the engine has: `solveContact` reads a
geometric interpolation between 2e6 and 6e8 scaled by `profile.hardnessScale`,
from which an *E\** would have to be back-derived, so the cube-root law would
be riding on a drawn constant. And at Bachi Hardness 0 and full velocity the
resulting patch radius is 7.35 mm on a 12 mm tip — 61 % of the tip radius,
outside the small-deformation assumption the Hertz solution is derived under —
so the law would need a cap that is itself drawn. The measured facts the step
was built on do hold: the patch radius runs 0.73 mm to 7.35 mm and grows by
2.8× to 3.0× from velocity 0.05 to 1.00, the force ratio being 22.5 to 28.2.

**Letting the head's own stretching pump the continuum (gap 4). Struck in
review.** It was step 6 as first drafted, and it was struck for its
verification rather than its physics. The test it proposed — 4–10 kHz over
30–80 ms rising at least 6 dB from Tension Mod 0 to 1 at full velocity, and
less than 1 dB at velocity 0.1 —
**already passes on the shipping engine**, at +7.38 dB and +0.12 dB. Chasing
that down produced gap 14: the rise is not the head, it is `applyTensionShift`
stepping running resonators, and it is flat to a tenth of a decibel from 1.2 to
20 kHz with the continuum switched off. So the step's premise — "retuning a
resonator while preserving its state is energy-conserving, so Tension Mod bends
pitch and adds nothing" — is wrong in the one way that makes the step
unmeasurable: there is already something there, it is the wrong thing, and no
test of a new mechanism can see past it. Step 2 above removes it. After that the
step becomes measurable again, and it still has gap 2 to get past, since
relighting bands 2 through 5 changes nothing a listener reaches. The whole-range
figure it quoted has also been corrected: 4–10 kHz gains 5.17 dB relative to
40–125 Hz over velocity 0.01 → 1.00, not 6.91.

**The hand as a real damper (gap 10).** It is right, it is cheap, and the engine
already has the physical path — `extraDamping` reaching `edgeLoss` and both
terms of `materialDamping`. Re-measured, the defect is a little larger than
the pass thought: 1.37 dB of tilt over seven octaves against the articulation
mute's 6.19 dB. It is left out because it is still the smallest measured
defect on the list, and because with the three continuum steps of the original
draft struck, the thing a hand would be sizing itself against — the continuum
above 2 kHz — is no longer about to move, so it can be sized honestly. It is
the first candidate for the next pass, and it is now the cheapest one.

**Buying an articulation's loudness with the stick's speed (gap 11).** Moving
`levelScale` in front of `solveContact` would make Su quiet because the stick
arrives slower rather than because it is multiplied by 0.24, and would remove
the one place in the instrument where level and timbre are decoupled. It is a
small change with a large blast radius: it re-levels all eight strokes, the
demonstration manifest and every level assertion in the suite at once. It wants
its own commit and its own re-render, not a place in a pass about the top of the
spectrum. Note that it now shares a mechanism with step 3: both want the contact
solve to know more about what is being struck than `drumContactTerms` currently
tells it.

**Strike Position's dead travel and the centre strike (gap 12).** Rescaling the
offset per articulation so the control spans the reachable radii, and stopping
it short of the geometric centre the engine documents as never being struck, is
half an hour's work. It is not here because it changes what every existing
Strike Position test measures. The reason the pass gave for deferring it — that
the right endpoint is a different number after the contact-patch step — has
lapsed with that step, so this is now free of entanglements and is a candidate
for the next pass alongside the hand.

**Fading a stolen voice (gap 13).** A step well below the ones the instrument
makes on purpose at every stroke onset, and masked by the incoming stroke.
`forcedFadeSeconds` and `retireGain` already exist to do it. Left out on
audibility alone.

**Resolving the modal bank up to true overlap (gap 9).** Reaching 566 Hz on the
factory drum needs Bessel zeros to about λ = 22.5, which is roughly 127 membrane
modes against the 40 the engine carries — a threefold rise in per-voice
resonator count and in the render loop's cost, for a region the continuum
already fills with noise of the right level and the wrong identity. It is the
largest single honest improvement left in the head and it does not fit beside
the rest. Review raises its standing: with three continuum steps struck for
being inaudible under the crossover band, the bank reaching further up is now
the *only* route to a head with any identity above 300 Hz.

**A continuum whose bands can be told apart.** Not in the original pass; it
falls out of gap 2 and is now the gate on three of its steps. As built, each
band is the difference of two doubled one-poles, 12 dB per octave a side, and
the level law falls as about *f*^−1.5, so the crossover band is louder than
every band above it in that band's own octave — measured at C3, the top band is
8.8 dB below the crossover band's skirt at 6.6 kHz, and deleting it costs 0.5 dB
of 4–10 kHz. The struct's own comment records that this problem was found once
before and answered by doubling the poles; the doubling was not enough. Until it
is answered properly nothing per-band — a source size, a contact patch, a
parametric pump, a ceiling — can be heard. It needs a design decision about what
the continuum *is* before it needs a commit.

**The stretched-plate eigenfunctions.** The previous pass took Ando's
eigenvalues and applied `stiffnessStretch` to the mode frequencies, but the mode
shape at the strike point, the shape at each microphone, the modal mass and the
net-volume radiation weight are all still ideal-membrane Bessel quantities. A
clamped stiff membrane has *J_m*/*I_m* combinations with a boundary layer at the
rim, and the correction is largest exactly where three of the eight strokes land
(0.91, 0.97, 0.99 of the radius). It is not here because it touches the modal
mass and the radiation weight together — the factor the README calls "most of
what a listener calls body" — and because the size of the audible change cannot
be estimated without building it. It deserves a pass of its own, starting with a
measurement rather than with a commit.

**The full von Kármán mode coupling.** Quadratic combination tones between the
resolved low modes need a cross-mode term in a bank that has none, with the
stability questions that brings. With the parametric-pump step struck, this is
what is left of the von Kármán idea, and it is the half that lands in the region
the instrument can actually resolve — which, given gap 2, may make it the better
half. Deferred deliberately, not overlooked.

**Cavity loss (the dissipative half of gap 8).** Derived and then dropped on its
own number. Thermal exchange with the walls gives a loss factor
*η ≈ (γ−1)·δ_t·S/2V* with *δ_t = √(2κ/ρc_p ω)*; on the factory drum
*S/V = 2/L + 2/a* = 6.7 m⁻¹ and *δ_t* at 88 Hz is 84 µm, so *η* ≈ 1.1e-4. With
the breathing mode storing about two thirds of its energy in the air, that adds
about 0.02 s⁻¹ to its decay against several s⁻¹ from radiation — three orders
too small to hear. So Body Depth will still not change how fast either branch
empties, and the reason is now a number rather than an omission. Anything that
would make it audible (leakage past the tack line, transmission through the
shell wall) needs a constant no measurement fixes.

**The air column as its own degree of freedom.** Still the best remaining
physics in this instrument, and step 4 sharpens the case: solved
self-consistently, the factory drum sits at *x* ≈ 0.62 and octave −2 at Body
Depth 1 at *x* ≈ 0.94, against the half-column quarter-wave at *π/2*, so the
biggest drums are being run well into the range where a stiffness of any kind is
an approximation — and the parameter scan finds corners where the fixed point
runs past *π/2* altogether and the two heads decouple. Doing it properly turns
the two-by-two into a three-by-three and touches the render path, the readout and
the tail sweep together. It is also the honest answer to step 4's clamp problem.

**A single image-source floor reflection.** Allen and Berkley's construction is
exact for one rigid plane and needs no impulse responses at all — one mirrored
source, one delay, one distance law, one reflection coefficient — and it would
give the instrument an honest answer to the competitors' microphone-perspective
bullet without buying an asset. It is not here because the construction needs
the height of the head above the floor, and Taikor does not model a stand: every
candidate number for that height is drawn rather than derived, which is exactly
what this document exists to refuse. It becomes available the day the stand
does.

**Two-handed structure in Humanise.** `triggerVoice` reseeds per stroke and
draws position, angle, speed and contact time independently, so a roll varies
randomly where a player's two hands vary periodically — in level, in timing and,
because the stereo image comes from the mode shape at each microphone, in
left–right position. It is genuinely differentiating, since round robins are
random too and no sampler can produce the periodic structure either. It is not
here because the engine cannot know the sticking: a MIDI stream does not say
which hand played a note, and assuming strict alternation would make a
one-handed passage rock left and right when it should not. It needs a control or
a heuristic, and both are guesses.

**Temperature and humidity drift.** Ando's AST 33(4) (2012) measurements of a
nagado head's tension against temperature, humidity and age are cited at the top
of this document and have still never been turned into a step. A slow,
correlated drift of pitch and tail length across a performance is something no
sample library can do at all. It is not here because the paper could not be
opened this session and the size of the shift is the whole content of the step —
implementing it from an assumed constant would be inventing the measurement it
is supposed to carry.

**The stand.** Recorded above as an absence finding. The mechanism is real and
the numbers do not exist.

**Documentation and demonstration audio.** Steps 1, 2, 3, 4 and 5 all move
levels or frequencies, so the committed takes under `Docs/audio/` and their level
table are stale until re-rendered centrally. The README needs two corrections
after this pass and not before: "What is not modelled" must record that the
cavity is a finite column rather than a lumped spring, together with whatever
step 4 decides about the clamp; and the list of calibrated constants must record
that the continuum's weight is now in seconds because it multiplies a
receptance. The README's claim about what the microphones hear above 400 Hz
stays aspirational, because the step that would have made it true did not
survive review — and the note in "What is not modelled" should say so rather
than leaving the claim standing unqualified.
