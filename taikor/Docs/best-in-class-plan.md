# Making Taikor best in class

This document records what the expensive commercial taiko instruments actually
do, where Taikor stands against them today, and the numbered steps taken to
close the distance. It is written to be checkable: every claim about Taikor is a
number measured from the shipping engine, and every step states what would fail
if the step were reverted.

## The instruments this competes with

| Product | What it is | Price | Scale |
| --- | --- | ---: | --- |
| Sonica **TAIKO THUNDER: The Ultimate Collection** | Kontakt Player, 20 percussion instruments played by Japanese taiko performers | $695 | Solo or two- to four-person ensemble function; seven microphone channels at 24-bit/96 kHz; 19 sampled positions total across one diameter—nine left, centre, nine right—with independently controlled hands; dozens of articulations; 1,400+ MIDI grooves |
| In Session Audio **Taiko Creator** | Kontakt, 24 drums plus gongs, cymbals, sticks and vocalisations | $139 | 9,881 samples; every head hit is 7 round robins × 7 velocity layers; several mic perspectives; suite-based MIDI content |
| 8Dio **Epic Taiko Ensemble** / **Solo Taiko** | Kontakt, ensemble and solo taiko | $148 / $198 | 2,900 and 4,200 samples; up to 8 velocity layers at 10 round robins; two microphone positions; articulation browser and step sequencer |
| Impact Soundworks **Kageyama Taikos** | Kontakt, 9 solo instruments played by Isaku Kageyama | $99 | Two mono spot positions plus a stereo overhead, 24-bit/48 kHz |
| Sound Magic **Supreme Drums Taiko** | The only modelled competitor. "Hybrid modelling" Epic Engine | $199 | 200 MB rather than 20 GB; claims unlimited round robins and up to 65,536 velocity steps via MIDI 2.0 |

Sources:

- [TAIKO THUNDER: The Ultimate Collection — Sonica Instruments](https://sonica.jp/instruments/en/product/taiko-thunder-the-ultimate-collection/)
  and its [KVR listing](https://www.kvraudio.com/product/taiko-thunder-the-ultimate-collection-by-sonica-instruments);
  Sonica's development blogs define [nine left + centre + nine right](https://sonica.jp/instruments/en/taikothunder_dev_blog2/)
  and the [independent hand controls](https://sonica.jp/instruments/en/taikothunder_dev_blog3/)
- [Taiko Creator — In Session Audio](https://insessionaudio.com/products/taiko-creator/) and the
  [Sound On Sound review](https://www.soundonsound.com/reviews/insession-audio-taiko-creator)
- [Epic Taiko Ensemble — 8Dio](https://8dio.com/products/epic-taiko-ensemble-vst) and
  [Solo Taiko — 8Dio](https://8dio.com/products/the-new-solo-taiko-drum-vst-au-aax-kontakt-instruments-samples)
- [Kageyama Taikos — Impact Soundworks](https://impactsoundworks.com/product/kageyama-taikos-kp/)
- [Supreme Drums Taiko — Sound Magic](https://neovst.com/product/supreme-drums-taiko__trashed/?lang=ja),
  plus developer notes on the hybrid approach on [KVR](https://www.kvraudio.com/forum/viewtopic.php?p=9203245)
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

**What it already wins on.** Each articulation's strike point is continuous in
radius and azimuth rather than selected from nineteen sampled points on one
diameter. Velocity is continuous rather than seven layers, and the timbre
follows it through the Hertz contact law rather than through crossfades. An
identity-default curve now calibrates light and heavy controllers before that
physical map without revoicing the drum.
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

6. **One instance is one performer.** Kumi-daiko is an ensemble form and every
   library above sells ensemble content. The initial engine gave separate
   instances the same deterministic Humanise sequence, so layering the same MIDI
   made coherent copies rather than distinct players. The appended Performer
   P1–P4 identity now closes that phase-locking gap for multi-instance ensembles;
   it does not claim a staged ensemble inside one instance.

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
  *Verified by*: the preload must rise with Head Tension and remain unchanged
  with diameter at fixed tension; a light rim shot must not reach it and a full one must clear it
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
  instrument can make does not move; factory headroom is independently guarded
  by the hostile-order calibration recorded below.
  Closes gap 5.
  *Verified by*: the peak level span from velocity 0.02 to 1.00 at full Velocity
  Depth must exceed 30 dB for every stroke (it is 34 to 42; it was 22 to 29);
  five equal steps of velocity must be within a factor of 1.8 of each other in
  decibels (they are within 1.2; they were within 2.2); and the loudest single
  stroke at the factory output must still stay clear of the safety limiter.

- [x] **7. Give layered instances stable performer identities.** Append a
  persisted, non-automatable four-position Performer choice after every
  established host slot. P1's salt is zero and retains the established
  random-sequence arithmetic; P2–P4 salt only the position, speed, contact time
  and stochastic variation already enabled by Humanise. At Humanise zero all four
  remain bit-identical. No choice changes tuning, diameter, level or any physical
  drum coefficient.
  *Verified by*: each identity must repeat bit-exactly, all six identity pairs
  must differ in both the complete hit and an isolated resolved-mode hit, and a
  unity-sum P1–P4 layer must not be only a gain-scaled P1 waveform. Humanise-zero
  renders must remain identical across identities; state, legacy-default,
  editor and hostile-headroom tests cover the appended host parameter.

## What was investigated and not done

**An ensemble inside one instance (gap 6, narrowed).** The first pass correctly
rejected a hidden chorus made by detuning, resizing or re-levelling one drum, and
also rejected raising the voice pool for a speculative internal four-player
stage. That larger feature remains undone. The later Performer step solves the
concrete failure users hit when doing the honest compositional thing in a DAW:
layer two to four instances, route the same part to them and select distinct
P1–P4 identities. Timing and authored drum differences remain the arranger's;
Taikor supplies repeatable non-coherent contact gestures without inventing a
second drum or room.

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
with eight steps. Five survived review, and two of those five were then struck
on implementation, so three are left.

What survived review is the sample rate reaching the continuum's level, an
attack glide that steps its own resonators, a striker that never rings, a cavity
treated as a spring of infinite extent, and a keyboard octave that is not an
octave in the drum's own lowest mode. Two of those have since gone as well. The
glide went because the spray the step was written against is the leakage of a
rectangular window rather than anything the engine emits; that correction is
under gap 14. The striker went for a different reason — the mechanism is right
and the level is not derivable, because the constant that would set the level of
the striker's ring against the drum is pinned by nothing: it can be moved six
decibels either way without changing the one stroke it is calibrated on, and six
decibels is the whole distance between a striker that damages the instrument and
one that cannot be heard. Both are with the rest of the rejects. What did not
survive review is everything that tried to shape the continuum band by band —
its ceiling, its source size, its contact patch, its parametric pump. All four
were prototyped on a scratch copy of the engine and measured, and all four moved
the audio by less
than a decibel where they had promised six, for one shared reason recorded as
gap 2: the continuum's bands are one-pole differences with 12 dB/octave skirts
and a level law falling as *f*^−1.5, so the lowest band is louder than every
band above it in that band's own octave and no per-band physics can be heard
through it. Making the bands separable is now the gate on that whole line of
work, and it is written up with the rest of what is not planned.

### What changed in the field

**This began as a search-only refresh, and every competitive claim in the
original pass was second-hand.** One later exception is now explicit: Sonica's
own development blogs were opened to verify the 19-position topology and its
independent hand controls. Everything else in this historical subsection keeps
the original search-only evidence boundary. When this section was first written no research was possible at
all: the session's web-search budget was exhausted and every outbound fetch was
refused. Budget has since refreshed and this subsection was rewritten from about
two dozen distinct searches. Direct page fetches were refused at that time:
`WebFetch` returned `EGRESS_BLOCKED` for every publisher and vendor domain. In
that pass **not one primary source was opened**; the two Sonica pages cited above
are a later, narrow verification. Everything else below is what search-result
summaries quoted from pages that were never read. No price, version, date or
quotation appears unless a search result stated it. The list of what remains
unverified, at the end of this subsection, is the honest bottom line.

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
  (2015) — both colliding bodies carry the contact force. Was the authority for
  the striker step, which was implemented and struck because the level of the
  striker's ring is not determined by anything in the engine; like Morse and
  Ingard above, this reference now supports a finding rather than a commit.
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

**What is still unverified, and why.** Searching is not reading. Apart from the
two Sonica development pages explicitly cited above, the constraint that
produced this subsection still applies.

- **Every figure above except Sonica's 19-position topology.** Search summaries
  paraphrase, can be stale, and can quote a reseller or cached page rather than
  the vendor. The other numbers in the product table were not re-read at source.
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
- **What any of these products actually measures.** No dynamic range or
  round-robin behaviour under repetition has been independently measured here.
  Sonica's own opened pages verify its vendor topology of 19 positions total;
  "7 × 7", "65,536 velocity steps" and "127 dynamic layers" remain claims
  relayed at second hand, and the Soundpaint criticism above is the only public
  evidence that one of those claims does not deliver what it sounds like.
- **Non-English sources.** Only English-language material was reviewed. Japanese
  retail listings and Japanese forum opinion remain the obvious next place to
  look; Sonica's English development blogs are the one vendor-primary addition.
- **All the acoustics references listed above**, which remain unopened in this
  competitive refresh.

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
first pass. At that time the ensemble needed a build environment for the plugin
target that the pass did not have, and a room perspective needed impulse
responses this repository could not obtain. The later Performer tranche closes
the multi-instance phase-locking failure without reopening the rejected hidden
detune/chorus design; a captured room remains unavailable. The Unity control
and the "mixing solo recordings is not an effective trick" line remain recorded
above as the boundary on any larger internal ensemble. One finding actively
supports the premise the steps serve: the
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

4. **The attack glide transfers no energy up the spectrum.** ~~— but it does
   inject broadband noise, which is worse.~~ The first half stands and the
   second half is withdrawn with gap 14: what the glide injects above 1.2 kHz
   is 101.6 dB under the stroke that made it, and Tension Mod 0 → 1 moves it by
   −0.00 dB. The last sentence of this entry is wrong for the same reason and
   is struck below. `applyTensionShift`
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
   louder, shorter and briefly sharper; ~~the one thing that does change its
   character above 1 kHz is an artefact.~~ what little changes its character
   above 1 kHz is the continuum's bands retuning with the head, which is the
   mechanism the engine means to have, and it is worth about a decibel.

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
   **Still open, and one thing is added to it.** The step written against this
   gap was implemented and struck, twice and independently: the missing modes
   are easy and their level is not. ~~because `drumContactTerms` and
   `resolveStickFor` describe the same bachi with masses 1.61× to 3.21× apart
   across the keyboard — 0.3282 kg against 0.1348 kg at octave 0 — and the
   striker's ring at the microphones is 4.1 to 10.1 dB louder or quieter
   depending on which is believed.~~ The two descriptions of the bachi do
   disagree — by 3.52× to 5.67× against the mass that carries a mode's level,
   re-measured — but that is not the binding constraint. The level of the
   striker's ring is `stickCalibration`, which stands in for a directivity the
   model does not have and which the Bachi stroke pins only at the bottom two
   octaves of the keyboard, where it is degenerate with that stroke's own output
   trim. So the gap is about the level as much as about the modes, and the
   sentence above should be read as: the engine cannot tell a stick what it hit,
   it does not agree with itself about what the stick is, and it has no
   measurement anywhere that says how loud a stick is against a drum. The
   measurements are with the rejects.

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
   **Half closed by step 5.** The loaded fundamental's half is closed: the octave
   transform is now solved against that branch, every octave step at Octave Body
   0.0, 0.7 and 1.0 reads 1200.0 cents, and the worst error anywhere in a
   155,520-point control scan is 50.9 cents against 1106.3 before. Two things in
   this entry are untouched and one is worse. The breathing mode's octave is
   untouched as a defect and is now further off — 508.7 / 610.8 / 775.8 / 956.1 /
   1093.7 cents at the factory body, because the keyboard applies less transform
   than it used to and the breathing branch was being dragged along by the
   surplus. And the loudest partial below C4 is still on neither branch, so the
   two lowest boundaries of a strongest-partial scan still measure which mode
   won: what closing this entry needs is not another transform but the reason a
   Don's loudest partial at C1 and C2 belongs to a mode with a circumferential
   order.

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
   **Half closed by step 4.** The reactive half is now in the engine: the drum
   resolve converges on the *x·cot x* factor and the render path is handed the
   converged stiffness. The three figures above reproduced to the digit when
   step 4 landed; two of them then moved when step 5 changed which drum sits at
   octave −2, and on the tree this pass hands on they read **0.8675** at the
   factory drum (unchanged, because the octave transform is the identity there),
   **0.8003** at octave −2 and **0.7082** at octave −2 with Body Depth 1. Both
   of step 4's test clauses — below 0.80 and below 0.72 — still hold, at 0.8003
   against 0.82 as the clause was re-taken and 0.7082 against 0.72. What
   is still open is everything in the last three sentences — the air remains
   lossless and massless, so Body Depth still cannot change the length of a
   boom, and above the column's quarter-wave the air is treated as absent
   rather than as the mass it becomes.

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

14. ~~**The attack glide sprays broadband noise across the top of the
    spectrum.**~~ **Withdrawn. It was the estimator, not the engine**, and the
    correction is written out under step 2 below, which it was the whole reason
    for. The mechanism the gap names is real and its size is not: what the
    coefficient rewrite leaves above 1.2 kHz on a full-velocity Don with the
    continuum silenced, over the same 30–80 ms and measured with an eight-pole
    high-pass run from the strike instead of through a rectangular window, is
    **−119.9 dB against a stroke at −18.3 dB, and Tension Mod 0 → 1 moves it by
    −0.00 dB**. The +7.15 / +7.23 / +7.24 dB below reproduces — I read
    +7.38 / +7.48 / +7.48 with the same estimator — but it is the leakage of the
    drum's own bottom two octaves through the window's sidelobes, and the
    flatness that was taken for the signature of an artefact is the signature of
    that leakage. The original text is kept below because three other entries in
    this document lean on it.

    ~~Found during review; not in the original reading, and it is the reason step
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
    after a hard stroke, and it is a filter artefact.~~

    What is true, measured on the shipping instrument with the continuum in,
    over the same window: Tension Mod 0 → 1 buys **+1.26 dB above 1.2 kHz and
    +0.79 dB above 4 kHz**, and that is the continuum's own bands retuning with
    the head — the mechanism the engine means to have — rather than an artefact.
    A hard stroke on Taikor is still, as gap 4 says, the same stroke louder and
    shorter; it is not sitting on six decibels of filter noise.

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
review and three did not; steps 2 and 3 were then struck on implementation,
leaving three. All five rejects are recorded with the rest below.

- [x] **1. Make the head's continuum independent of the host sample rate.**
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
  *What actually shipped*: the step as written, with the mechanism and both
  constants unchanged. `membranePeak` is now taken from
  `|mode.drive · rate · mode.micLeft|` at both accumulation sites
  (`TaikoEngine.cpp:1587` and `:1688`) and `continuumCalibration` is written
  `26.0f / 48000.0f` rather than `26.0f`, with the note at the declaration
  saying what it is now in units of: seconds, because what it multiplies is a
  receptance. Every baseline figure the step quotes reproduced to the digit —
  4–10 kHz relative to 48 kHz at +0.60 / −5.26 / −6.05 / −8.13 dB across 44.1 /
  88.2 / 96 / 192 kHz (and −9.78 at 384 kHz, where the step says −9.77), a
  Bachi at −2.11 and −5.51 dB in the same band, 400 Hz–16 kHz at −4.43 dB for a
  Don against −0.00 for a Katsu and +0.14 for a Bachi from 48 to 96 kHz, and
  the Mic Distance sweep at 13.53 / 17.23 / 20.54 dB. After the change the four
  rates measure +0.03 / 0.00 / −1.51 / −0.85 dB, exactly what the prototype
  predicted.
  Three numbers came out other than as written, all of them in the step's
  favour and all of them recorded in the test. The 400 Hz–16 kHz spread is
  **1.09 dB, not the prototyped 1.34**, so the 1.5 dB clause has more margin
  than it was given. The 40–200 Hz spread across the four rates went from 0.24
  to 0.29 dB, still well inside its 0.5 dB clause. And "no behaviour at 48 kHz
  changes" is true to 0.0000 dB of band level but is **not bit-identity**:
  re-anchoring a `float` constant by a power that is not a power of two rounds
  differently, and a render of all twelve strokes at 48 kHz differs from the
  pre-step one by 1.04e-07 against a peak of 0.443, which is 132.6 dB down. The
  step did not ask for bit-identity and the difference is a mantissa away from
  nothing, but it is not zero and is recorded rather than rounded away.
  On the revert the test fails on two clauses and not on the other three: the
  4–10 kHz spread reads 8.73 dB against its 2.0 dB limit and the 400 Hz–16 kHz
  spread 6.63 dB against 1.5. The 40–200 Hz clause, the 48 kHz level clause and
  the Mic Distance guard all pass on the unfixed engine, which is what they were
  written to do — they are there to catch a fix that overshoots, not to detect
  the defect.
  *Revert re-run on the shipping tree, after steps 4 and 5 and the four-by-four
  grid.* It still bites, and harder: taking the `rate` factor back out of
  `membranePeak` at both accumulation sites and putting
  `continuumCalibration` back to `26.0f` fails **three** clauses rather than two,
  the third being the Hann-windowed one step 4 added. The spreads are no longer
  the ones taken here, because the drums are not the same drums: 4–10 kHz reads
  −53.6311 / −54.7593 / −60.8324 / −65.8944 dB across 44.1 / 48 / 96 / 192 kHz,
  a spread of **12.26 dB** rather than 8.73 (48 kHz to 96 kHz alone is 6.07),
  the Hann-windowed spread is **13.68 dB** against its 1.5 dB clause, and
  400 Hz–16 kHz is **7.54 dB** against 1.5. On the shipping engine the same four
  readings are −54.1934 / −54.7593 / −56.5948 / −56.7029, a rectangular spread of
  2.51 dB and a Hann-windowed one of 0.9329. The test's own comment carries the
  re-taken figures; the 8.73 above is left as the number this step was measured
  against.
  **One hazard handed forward to step 2.** The 48 kHz level clause pins a
  literal, −54.7339 dB, on a render at the factory Tension Mod of 0.4, and step
  2 changes what the glide does to exactly this band. Measured so that the next
  agent does not have to guess how much room it has: over this window the whole
  glide is worth **0.68 dB** in 4–10 kHz (Tension Mod 0 reads −54.0569, 0.4 reads
  −54.7339, 1.0 reads −54.4193, and the 40–200 Hz band moves 0.11 dB across the
  three). That is far less than gap 14's 6 dB because gap 14 measures 30–80 ms,
  past the attack, where the drum has almost nothing of its own left; an 85 ms
  window that starts at the strike is dominated by content the glide does not
  make. So step 2 can move this pin, but only within seven tenths of a decibel,
  and if it does the honest repair is to re-take the literal on the tree step 2
  lands on rather than to widen the clause.
  *Resolved*: step 2 was struck without touching the engine, so the literal
  stands as taken and the hazard lapses. The 0.68 dB measured here is, in
  hindsight, the first sign that gap 14's 6 dB was not real — the same glide
  cannot be worth six decibels in one window and seven tenths in another that
  contains it. It was read at the time as the two windows holding different
  content; it was the two windows leaking differently.

- [ ] **2. Stop the attack glide from spraying the top of the spectrum.
  Struck.** The premise is a measurement artefact: there is no spray. Both the
  step and gap 14 rest on a rectangular-window band level, and a rectangular
  window's sidelobes fall only as one over the frequency offset, so a drum whose
  whole voice is two octaves wide leaks across the entire spectrum at a level
  that has nothing to do with what is there. The reasoning and every number are
  moved to "considered and not planned" below, the checkbox stays unticked, the
  engine is unchanged, and the measurement that killed it is kept in the suite as
  `testTheGlideDoesNotBrightenTheTopOfTheSpectrum`.

- [ ] **3. Ring the striker as well as the struck. Struck.** The mechanism is
  right and the level is not derivable. A contact force does act equally and
  oppositely on both bodies, the engine does carry a correct free-free bar
  model, and it is used on one stroke in eight — all of that stands. ~~What
  killed the step is that the engine holds two mutually inconsistent
  descriptions of the bachi, 4.1 to 10.1 dB apart, and the striker's level
  against the drum is whichever of them is picked.~~ What killed the step is
  that the striker's level against the drum is `stickCalibration`, a constant
  whose own comment admits it stands in for a directivity the model does not
  have, and which the one stroke that reads it today does not pin: six decibels
  of it can be traded against that stroke's output trim for four ten-thousandths
  of a decibel of change in the stroke, and those same six decibels are the
  difference between a striker that breaks two suite clauses and one that is
  quieter than the step requires. The reasoning, the implementation and every
  number are moved to "considered and not planned" below, the checkbox stays
  unticked, and the engine is unchanged: the tree handed on is bit-identical to
  the tree step 2 left, verified by rendering all eight strokes before and
  after. Gap 6 stays open and is now gated on giving the wooden bank a radiation
  term, not on reconciling the two masses.
  *Re-measured*: the step was implemented and struck a second time,
  independently, on the tree steps 1 and 2 left. The verdict holds and both of
  the suite figures the entry below turns on reproduce to the fourth decimal,
  but the mass table it gave as the reason does not reproduce at all and is
  withdrawn there. One test landed with the strike,
  `testTheStickBankIsOnlyCalibratedAtTheBottomOfTheKeyboard`, because the
  measurement that actually kills the step is a render rather than a constant.

- [x] **4. Give the enclosed air the impedance of a finite column instead of an
  infinite spring.** `ρc²/L` is the *ω → 0* limit of a cavity, and the README
  already records that the limit fails inside the drum's own range. The exact
  input stiffness of a rigidly terminated column of length *ℓ* driven at one
  end is *ρcω·cot(ωℓ/c)*; the volume-changing motion of a two-headed drum is
  symmetric about the midplane, so each head sees *ℓ = L/2*, and the stiffness
  entering the two-by-two becomes *k(ω) = (ρc²/L)·x·cot x* with *x = ωL/2c*.
  That is exactly the present value at *x → 0* and falls away as the drum gets
  deep relative to the wavelength. It makes the eigenvalue problem implicit, so
  solve it by ~~damped fixed-point iteration from the lumped value~~ **bisection
  on the monotone bracket the map provides — damped iteration does not converge
  everywhere, see what shipped** at
  drum-resolve time — never in the render loop. Solved to convergence,
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
  560.4 Hz and the loaded fundamental at 562.8 — ~~**the breathing branch falls
  below the other one**, and `testOctavesRaisePitch`'s "the cavity must lift
  the volume-changing mode above the other one" fails.~~ Either the factor keeps
  a floor, or that assertion and the `DrumMeasurements` contract behind it
  change in the same commit. Deciding which is part of the step.
  **Corrected on implementation.** That corner converges to exactly 0, not
  0.0002, and reports breathing = loaded = 560.4384 Hz — the 0.0002 and the
  560.4 / 562.8 split are an unconverged iterate. Floored at zero the branches
  meet and never cross, over 16200 configurations, so neither
  `testOctavesRaisePitch` nor the `DrumMeasurements` contract had to change; see
  what shipped, below.
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
  branch: the reported loaded fundamental must move by less than ~~1 %~~ **1.5 %
  — the step's prototyped 0.47 % does not reproduce; the measured worst case
  over the scan is 1.17 %** — anywhere
  in the parameter scan. Without it an implementation that dragged both branches
  down together would meet every breathing-mode clause above, and step 5 —
  which solves the keyboard against that lower branch — would be built on a
  moving quantity. Second, **convergence**: two identical `measure()` calls
  must return bit-identical factors, and the factor must be monotone in Body
  Depth at fixed octave. A damped fixed point that has not converged is a
  function of its iteration count rather than of the drum, and every number
  above would then be a number about the solver.
  *What actually shipped*: the mechanism exactly as written, and every figure
  the step prototyped reproduced to three or four digits. `resolveDrumFor`
  (`TaikoEngine.cpp:1123-1199`) now solves for the factor once per drum and
  writes `drum.cavityStiffness = (ρc²/L) · factor`, so `buildVoiceModes` and
  `measure()` are untouched and the render path only ever sees a converged
  number. Two static helpers carry the physics: `columnStiffnessFactor`
  (`:637`) is *x·cot x*, and `volumeBranchOmega` (`:670`) is the
  volume-changing branch of the (0,1) pair built exactly as the other two sites
  build it. `DrumState` gains
  `cavityColumnFactor` and `DrumMeasurements` gains `cavityStiffnessFactor`,
  which is the field the step said it would have to add. Measured: the factor is
  **0.8675** at the factory drum (breathing 88.1024 → 84.1197 Hz, −4.52 %),
  **0.7824** at octave −2 (45.4701 → 40.5043, −10.92 %), **0.6878** at octave −2
  with Body Depth 1 (37.2596 → 31.4282, −15.65 %) and **0.9408** at Body Depth
  0, octave +3 (518.5178 → 515.8969, −0.505 %). The breathing branch's octave
  steps at the factory Octave Body go from 509.4 / 635.7 / 829.5 / 1019.3 /
  1139.8 cents to **582.4 / 682.8 / 859.5 / 1038.5 / 1153.1**, which is the
  step's prototype to the tenth of a cent.
  Four things came out other than as written.
  **The solve is a bisection, not a damped fixed point, and it had to be.** The
  map from the factor to the factor the branch it produces asks for is monotone
  decreasing into [0, 1], so its fixed point is unique and bracketed by the
  endpoints — but its slope reaches about −100 where the cavity dominates the
  branch and the factor is small, because ω then goes as the square root of the
  factor while *x·cot x* is falling steeply towards the quarter-wave. Half-damped
  iteration was implemented first and measured: it fails to settle in **60 of
  16200** configurations of the step's own scan (0.37 %) and stops wherever its
  cap leaves it, which is exactly the "a number about the solver" the preflight
  convergence clause was added to forbid. Twenty-four halvings of the unit
  bracket take it to six parts in a hundred million, which is where a `float`
  runs out either way, and they agree with an independent re-solve written
  outside the engine to 3e-08 across all 16200 — the width of the bracket, and
  four orders below the loosest clause. The cost is worth recording because it
  lands on the audio thread: the solve roughly doubles a drum resolve, 1.40 to
  2.85 microseconds measured, and a drum resolve happens when a control moves or
  the wheel passes a tenth of a cent, at most once per process block and never
  per sample.
  **The clamp decision is the step's second alternative: a decoupled pair is the
  reported answer, and neither `testOctavesRaisePitch` nor the
  `DrumMeasurements` contract needed changing for it.** The factor is floored at
  zero at the quarter-wave, where the column's input stiffness genuinely is
  zero; above it the air is mass-like, which this model has nowhere to put, and
  past the second pole at *x = π* the expression turns positive again on a
  branch that means something else. The floor is continuous, because *x·cot x*
  reaches zero at the quarter-wave rather than jumping to it, and it lands the
  drum in the state the readout already describes at Air Coupling 0: one
  axisymmetric mode, reported twice. The step's fear that "the breathing branch
  falls below the other one" turns out to belong to the *unclamped* expression
  and not to the floor — with the factor floored at zero the two branches meet
  and stop, and over 16200 configurations the breathing mode is reported below
  the fundamental **exactly zero times**. 2130 of the 16200 land on the floor,
  every one of them a body *longer* than half its own head's wavelength — the
  same statement as the half column having passed its quarter-wave, and the
  opposite way round from how this sentence first read (an 8 cm body under a
  head at 3.5 kHz, whose half wavelength is 4.9 cm). `testOctavesRaisePitch`
  measures at the factory body, where the factor is 0.75 to 0.89 across the
  whole keyboard, and passed unchanged. One correction to the step's own corner:
  at Body Depth 0, Head Material 0, Octave Body 0, Air Coupling 0.2, octave +3
  the solve converges to **exactly 0** and reports breathing = loaded =
  **560.4384 Hz**, not the 560.4 / 562.8 split the draft quotes — that split was
  an unconverged iterate of 0.0002 rather than the fixed point.
  **The lower-branch clause is 1.5 %, not 1 %, and the step text's 0.47 % is
  wrong.** Measured over the same 16200-configuration scan, the reported loaded
  fundamental moves by at most **1.1668 %**, and 79 configurations exceed 1 %.
  Seventy-four of those 79 have the factor at zero, and what is happening in
  them is the lower branch relaxing back to the uncoupled head as the cavity is
  taken out from under it — not the branch being dragged anywhere. It is bounded
  above by a quantity the test now also asserts: the cavity's *whole* authority
  over the lower branch, at any depth and any coupling, is **1.95 %**, so no
  implementation of this step can move that branch further than that. At the
  factory drum the anchor step 5 solves against moves 50.7490 → 50.7475 Hz,
  which is 0.003 %.
  **Step 1's sample-rate test had to be widened, and was given a stronger clause
  in exchange.** `testTheContinuumDoesNotDependOnTheSampleRate` required the
  4–10 kHz spread across 44.1 / 48 / 96 / 192 kHz to be under 2.0 dB against a
  measured 1.54; with this step it reads **2.51**. That is the estimator, not
  the audio, and it is the same leakage step 2 was struck over: `bandLevelDb`
  takes a rectangular window, this band of a Don at C3 has almost nothing in it,
  and what it reads is mostly the sidelobes of the drum's bottom two octaves —
  so moving the breathing branch by four hertz rearranges the interference at
  each rate. Measured through the same Parseval sum under a Hann window, whose
  sidelobes fall as the cube of the offset, the spread is **0.9045 dB before
  this step and 0.9329 after**: three hundredths of a decibel, on a change that
  moved the rectangular reading by a decibel. The rectangular clause is now 3.0
  dB and the Hann-windowed one, added alongside at 1.5 dB, is tighter than the
  original ever was. Step 1's other clauses were untouched and all pass: the
  48 kHz literal moves −54.7339 → −54.7593 dB, a quarter of its 0.1 dB
  allowance, so it is left as taken rather than re-pinned.
  One clause was added beyond the step's contract, because every clause the step
  asked for reads `measure()` and an implementation that scaled the readout
  alone would have passed all of them. The test now strikes a Don dead centre,
  where *J_m(0) = 0* kills every mode with a circumferential order and the
  axisymmetric pair is the whole of the drum, and requires the strongest partial
  in the region to agree with the reported breathing mode to 1.5 % — it does on
  both trees, 88.522 against a reported 88.1024 before and 84.484 against
  84.1197 after — *and* to have come down off the 88.522 Hz the lumped spring
  rendered.
  On the revert the test fails on **twelve** assertions: both octave −2 factor
  clauses, both octave −2 breathing-mode clauses, all five octave steps (509.4 /
  635.7 / 829.5 / 1019.3 / 1139.8 cents against floors of 560 / 660 / 845 /
  1030 / 1145), the decoupled count (0 of 16200), the floor clause, and the
  audio clause. The clauses written as guards all pass on the unfixed engine, as
  they should: the 8 % clause at the short cavity, the 1 % clause on its
  breathing mode, the loaded-fundamental anchor and all five corner literals,
  the branch-ordering clause, bit-identity, monotonicity in Body Depth, and the
  1.95 % bound.
  One audible consequence that no clause pins and that is worth recording: at
  48 kHz the factory Don's 40–200 Hz band falls **0.82 dB** (−19.90 to −20.72
  dB over the 85 ms window), which is the breathing mode moving down four hertz
  and out of the part of that band where it was loudest.
  *Re-verified on the shipping tree, after step 5 landed.* Every figure above
  that is taken at the reference octave still reads exactly as written: the
  factory factor is 0.867519, the breathing mode 88.1024 → 84.1197 Hz (−4.52 %)
  and the loaded fundamental 50.7490 → 50.7475 Hz. Every figure taken away from
  the reference octave was superseded when step 5 landed, because step 5 changed
  which drum sits at those octaves — a drum an octave down is now a different
  size and tension, so its column is a different length. The claims are all
  unchanged; the numbers they are measured against are not. Re-taken, each pair
  being the lumped spring and the column on the *same* drum: octave −2 gives a
  factor of **0.8003** and 48.8434 → 44.0626 Hz (−9.79 %); octave −2 at Body
  Depth 1 gives **0.7082** and 40.1941 → 34.5117 Hz (−14.14 %); Body Depth 0 at
  octave +3 gives **0.9442** and 458.2408 → 455.7300 Hz (−0.55 %); and the
  breathing branch's five octave steps go from 451.0 / 570.2 / 747.8 / 936.7 /
  1078.8 cents to **508.7 / 610.8 / 775.8 / 956.1 / 1093.7**, so the column
  still widens every one of the five, from a smaller starting point than this
  entry was written against. Over the same 16200-configuration scan **2064** land
  on the floor rather than 2130, the cavity's whole authority over the lower
  branch is **1.9086 %** rather than 1.95, the branches cross **zero** times, and
  this step's own drift of the lower branch is at most **1.0542 %** — 24
  configurations over 1 %, 17 of them at the floor — rather than the 1.1668 % in
  79 measured before step 5. The draft corner (Body Depth 0, Head Material 0,
  Octave Body 0, Air Coupling 0.2, octave +3) still converges to exactly zero and
  still reports breathing = loaded, now at **564.6117 Hz** rather than 560.4384.
  The test carries every one of these as its literal and says which are re-takes.
  The revert was re-run on the shipping tree rather than trusted: forcing the
  factor to one fails **thirteen** assertions, the twelve this entry names plus
  one in step 5's test — the recorded literal for the strongest partial's step
  into octave −1, which reads −36.4 cents against −378.9. That literal was taken
  with this step in place, so it is collateral rather than a second
  discriminator, and it is the plainest statement of how far into the audio this
  step reaches. Every clause written as a guard still passes on the reverted
  engine.
  **Superseded by “Per-mode finite-column cavity resolve — 2026-08-16” below.**
  The following records the pre-change measurement and decision.
  **What the single factor does not cover, measured.** `drum.cavityStiffness` is
  solved on the (0,1) volume branch and is then what every axisymmetric mode
  reads, so the modes above the pair are given a column stiffness evaluated at a
  frequency that is not theirs. At the factory drum the (0,2) volume branch sits
  at 133.428 Hz, where *x* = 0.987 and the column would ask 0.652 rather than
  0.868, and (0,3) and (0,4) at 213.2 and 294.9 Hz are past the quarter-wave
  entirely, where the exact column stiffness is zero. Solving each pair on its
  own factor instead moves them by **−8.0, −3.6 and −1.0 cents** at the factory
  drum and **+9.2, −5.7 and −4.0** at octave −2 — the sign flips because that
  drum's (0,2) branch sits below its (0,1) one — and leaves the (0,1) pair, which
  is what every clause here reads, untouched by construction. Nine cents on the
  second axisymmetric mode is smaller than the step's own effect on the first by
  more than an order of magnitude, nothing asks for it, and taking it would move
  the resolved bank and add three more bisections to every drum resolve. It is
  recorded rather than done.

- [x] **5. Hold the drum's own fundamental on the keyboard, not the ideal
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
  1199.7 / 1199.0 / 1196.2 / 1191.2 cents — re-measured on the tree step 4 left,
  1199.9 / 1199.7 / **1198.5 / 1181.5 / 1196.7**, because at Octave Body 0 the
  drum does not change size and the top of the keyboard drives its own air
  column past the quarter-wave — and Pitch ±12 st is already good to 1 cent
  (measured −1199.70 and **+1198.54**, so a cent and a half at the top). And the
  step does **not** close the musical half of gap 7 as that gap
  is written, because the loudest partial is not the loaded fundamental below
  C4: measured, it is 58.50 / 42.75 / 89.50 Hz at C1 / C2 / C3, on neither
  branch at the bottom two and on the breathing branch at C3, where it sits
  5.0 dB above the fundamental. What this step buys is precise and worth having
  on its own — the drum's own lowest mode lands on the key that names it, at
  every setting of Octave Body — and the step must claim that and not more.
  It does not have to come last: the cavity moves the *upper* branch, and step
  4, as shipped, changes the loaded fundamental by **0.003 %** at the factory
  drum (50.7490 → 50.7475 Hz) and by at most **1.17 %** anywhere measured, all
  of the latter in configurations where the column has passed its quarter-wave
  and the cavity has been taken out from under the branch entirely. The two are
  independent and either order works. Note for whoever takes step 5: the anchor
  clause below quotes 50.7490 Hz, which is the pre-step-4 figure. Step 4 leaves
  it at 50.7475, so re-take that literal rather than reading a failure into it. *Verified by*: `testTheDrumIsTunedByThePitchItSounds` requires the
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
  −543.2 / +1279.9 / +340.4 / +1272.4 / +1244.2 cents — re-measured with
  Humanise off, **−532.9 / +1278.7 / +331.6 / +1270.0 / +1243.3** — **with 20
  cents of slack** — which strongest partial wins a scan can flip on a fraction
  of a decibel, and without the slack this clause fails on jitter rather than on
  the step. ~~As drafted this clause is unsatisfiable~~: the C3-to-C4 boundary
  gets 115 cents worse and the reason is the step's own caveat above. What
  shipped instead is under *What actually shipped*.
  (The ±6 % local maximum holds today at every octave, to within 0.25 dB
  of the peak inside that band — measured **0.32 dB**, worst 0.267 after the
  step and 0.262 before it — so it is a do-no-harm clause and the test should
  say so.)
  Two clauses added in preflight, because the assertions above are all about a
  number the step is also free to redefine. **The anchor**: the octave transform
  is the identity at octave 0 for every Octave Body — `radiusFactor` and
  `tensionOctaveFactor` are both 1 there — so the reported loaded fundamental at
  octave 0 must stay at today's **50.7490 Hz** — step 4 left it at **50.7475**,
  which is what shipped, and every stroke at octave 0 renders bit-identically to
  the tree before this step — and must be identical across
  Octave Body 0, 0.7 and 1.0, as it is today. A bisection that met the octave
  ratios by moving the whole keyboard would pass every other clause.
  **Octave Body must keep its meaning**: the bisection runs on the mixture
  Octave Body already chooses, so at Octave Body 0 the reported radius must stay
  at 0.4750 m at every octave from C1 to C6 while the tension quadruples per
  octave, and at Octave Body 1 the reported tension must stay at 5942.4 N/m
  while the radius halves. Both hold today, and they are what stops the solve
  buying an octave on an axis the control does not own. The two halves that hold
  after the step are the two that are exact: the radius at Octave Body 0 and the
  tension at Octave Body 1 are untouched by the transform at any share, and both
  are asserted. The other two are no longer exact and are not asserted — that is
  the step working. The tension at Octave Body 0 now goes ×4.000 / 4.001 / 4.006
  / 4.093 / 4.010 per octave, and the radius at Octave Body 1 ×0.578 / 0.560 /
  0.542 / 0.526 / 0.514, which is the solve buying the octave the drum actually
  sounds rather than the one an ideal membrane would.
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

  *What actually shipped*: the mechanism as written, with the solve carried on
  resolved drums rather than on numbers, and with four of the verification
  clauses corrected against measurement.

  `resolveDrumFor` (`Source/DSP/TaikoEngine.cpp:1256`) no longer writes the
  octave transform down. Everything the (0,1) pair depends on — geometry,
  tension, wave speeds, bending stiffness, the losses and the converged cavity
  stiffness — moved into a new `resolveDrumGeometry` (`:1048`) taking the
  transform's two factors as arguments; the shell, the mounting and the
  microphones stayed where they were and are computed once from the answer. The
  branch solve `measure()` did inline became `solveAxisymmetricPair` (`:708`),
  so the keyboard is solved against exactly the number the panel reports rather
  than against a second copy of it. The solve bisects on the *share* of the old
  transform — one share means all of it, so the bracket runs from none of it to
  all of it and beyond in whichever direction the keyboard is going, and the
  function is increasing in the share both ways. Twenty halvings, widened up to
  four times if a share of one still undershoots.

  Measured. Every octave step at every one of Octave Body 0.0 / 0.7 / 1.0 now
  reads **1200.0 cents**, against 1409.5 / 1359.6 / 1314.5 / 1276.3 / 1244.6 at
  0.7 and 1545.3 / 1443.2 / 1352.6 / 1286.2 / 1243.6 at 1.0 before. Over a
  155,520-point scan of eight controls crossed with the six octaves the worst
  error goes from **−1106.3 cents to −50.9**, and the count outside ±20 cents
  from 101,957 to **102** — 0.066 %, every one of them a drum whose air column
  has passed its quarter-wave or whose geometry has hit a clamp, and none of
  them a taiko. An emergent property worth recording: the sounding pitch is now
  the *same* at every octave for every Octave Body (12.6869 / 25.3737 / 50.7475
  / 101.4949 / 202.9899 / 405.98 Hz at 0.0, 0.7 and 1.0 alike), so the control
  now changes only the body and never the pitch, which is what its name claims.
  At octave 0 all eight strokes render **bit-identically** to the previous tree,
  because the transform is the identity there and that case is taken out of the
  solve before it starts.

  Four things differ from the step text.

  1. **The bracket carries drums, not numbers.** Re-resolving the winning share
     at the end is the same arithmetic written twice, and the two copies do not
     round the same way. Where the sounding pitch *steps* — at the share where
     the column reaches its quarter-wave, the heads come apart, the lower branch
     becomes the far head's alone and what the drum sounds jumps up to the batter
     head's own mode — a difference in the last place put the final resolve the
     other side of the step from the trial that chose it: 219.04 Hz against
     235.20, a 122.6-cent swing on one corner, decided by the compiler. The
     bracket now holds two resolved `DrumState`s and the winner is handed on. The
     end nearer the target is taken rather than the midpoint, for the same
     reason: where the pitch steps, the octave being asked for does not exist,
     and the near side of the gap is the closest it can be got to.
  2. **The anchor literal is 50.7475 Hz**, as step 4 predicted, and it is exact
     rather than approximate.
  3. **The ±6 % local-maximum clause needs 0.32 dB, not 0.25.** Measured worst
     0.267 dB after and 0.262 before; it is a do-no-harm clause on both trees and
     the test says so.
  4. **The strongest-partial clause as drafted is unsatisfiable, and the reason
     is the step's own caveat.** Measured with Humanise off, the six octaves'
     strongest partials in 8–900 Hz go from 58.50 / 43.00 / 90.00 / 109.00 /
     227.00 / 465.50 Hz to 58.50 / 47.00 / 90.00 / 102.00 / 203.50 / 406.25, so
     the five steps go from −532.9 / +1278.7 / +331.6 / +1270.0 / +1243.3 cents
     to −378.9 / +1124.7 / +216.7 / +1195.8 / +1196.8. The top two boundaries —
     where the strongest partial is the loaded fundamental at both ends — improve
     from 70.0 and 43.3 cents of error to **4.2 and 3.2**, and they are asserted
     as octaves; both fail on the reverted tree. The C3-to-C4 boundary gets
     **115 cents worse**, outside the 20 cents of slack the step allowed, and it
     is not a statement about tuning: C3's loudest partial is 90.00 Hz, which is
     neither the reported fundamental (50.75) nor the reported breathing mode
     (84.12), while C4's is the fundamental, so the ratio measures which mode won
     rather than what the drum is tuned to. Both readings are about nine hundred
     cents from an octave. That is gap 7's other half, which this step says in as
     many words it does not close. The three lower boundaries are now recorded as
     literals within 25 cents rather than bounded, so that a change which moves
     them has to be looked at.

  Two of step 4's clauses were superseded and re-taken, and one regression is
  recorded rather than rounded away.

  - Every literal in `testTheCavityIsAColumnNotAnInfiniteSpring` taken away from
    the reference octave was re-taken, because this step changed which drum sits
    at those octaves. They are now stated as the lumped spring against the column
    **on the same drum**, which is what that test was always about: 458.2408 →
    455.7300 Hz at the shallow body, 48.8434 → 44.0626 at octave −2, 40.1941 →
    34.5117 at the deepest body, and the five worst corners of the lower-branch
    guard move by at most 0.086 %. The scan clause that bounds the cavity's whole
    authority over the lower branch at 1.95 % was untouched and still passes.
  - That test's Body Depth monotonicity clause is now strict at octave 0 only.
    Away from the reference octave the drum is no longer a fixed function of the
    controls — deepening the body lowers the sounding pitch and the solve answers
    with a different size and tension — so the factor of a *different* drum has
    no reason to be monotone in Body Depth, and over the scan it is not, in 262
    of 13,608 steps by at most 0.0016. At octave 0 the rise is exactly zero in
    every one of them, which is where the clause says what step 4 wrote it to
    say.
  - **The breathing branch's octave steps got worse, and that is this step's
    doing.** They read 508.7 / 610.8 / 775.8 / 956.1 / 1093.7 cents where before
    this step they read 582.4 / 682.8 / 859.5 / 1038.5 / 1153.1: the keyboard now
    applies as much of the transform as puts the *sounding* pitch an octave away,
    which is less than a full octave of it, and the breathing branch — which used
    to be dragged along by the full transform — moves less. Step 4's own claim is
    untouched by this and was checked rather than assumed: measured on the same
    drums with the column replaced by the lumped spring it replaced, the five
    steps are 451.0 / 570.2 / 747.8 / 936.7 / 1078.8, so the column still widens
    every one of the five. Step 4's floors were re-taken to 500 / 600 / 765 / 945
    / 1080. The breathing branch's octave is gap 7's larger half and neither step
    closes it.

  Cost, since the solve lands on the audio thread: resolving all six octaves goes
  from **15.7 to 179.8 microseconds**, measured, which is 25 head-and-cavity
  resolves per octave instead of one. It runs from `refreshDrumIfNeeded`, which
  is called once per `process` block and on each trigger, so the worst case is a
  pitch-wheel sweep at a small block size — 13 % of a 64-sample block at 48 kHz,
  1.7 % of a 512-sample one. Nothing per sample.

  One audible consequence no clause pins. Away from the reference octave every
  stroke moves, because the drum is a different drum: peak levels move by up to
  0.08 of full scale, mostly upward at the top of the keyboard and downward at
  octave −1. The one place that was already on the limiter came off it — Don Rim
  at octave −2 renders 0.9907 where it rendered exactly 1.000000 before — so the
  pre-existing condition recorded under struck step 3 is slightly better and
  nothing new was pushed into it.

  *Re-verified on the shipping tree, after the 4x4 playing grid landed.* The
  mechanism above is intact and every clause it is verified by still holds. The
  numbers were taken on a keyboard of six octaves of one drum rescaled, and that
  keyboard no longer exists; the grid section below records which of them it
  re-took, and this note records the rest, together with the revert, which was
  re-run rather than carried over.

  The clause the step is for is now exact. Every octave step at every one of
  Octave Body 0.0 / 0.7 / 1.0 reads **1200.000 cents** across the four drums, and
  the four sounding pitches are the same at all three settings — 50.7475 /
  101.4949 / 202.9899 / 405.9798 Hz — so the emergent property this entry
  recorded survived the change of keyboard: Octave Body changes the body and
  never the pitch. Pitch ±12 st reads −1199.702 and +1198.540 cents, unmoved.

  The anchor is exact, and it was checked as audio rather than as a readout. At
  octave 0 the reported fundamental is **50.7475 Hz**, the radius 0.4750 m and
  the tension 5942.4297 N/m at every Octave Body, and all four strokes render
  **bit-identically** to the reverted engine over 19,200 samples. Every other
  octave differs from it by up to 0.70 of full scale.

  Two of the Octave Body clauses changed what they are able to say, because the
  four octaves are now four instruments. At Octave Body 0 the radius still never
  moves — 0.4750 m at all four octaves — and the tension still buys the whole
  octave, now ×4.0068 / ×4.0926 / ×4.0094. At Octave Body 1 the tension is no
  longer one number held fixed: it is each drum's own, 5942.43 / 5536.91 /
  8300.62 / 14799.36 N/m, and that is what the test asserts, to a thousandth. The
  radius there goes 0.4750 / 0.2761 / 0.2002 / 0.1496 m, which is not a halving
  per octave and is not meant to be — it is the four diameters the table states
  plus under half a per cent of residual tuning.

  Re-taken over the same eight-control scan this entry used, now crossed with
  four octaves instead of six: 25,920 configurations, 103,680 octave readings, of
  which 25,920 are the reference octave and exact by construction. The worst
  error is **+50.9 cents** (Octave Body 0.25, Body Depth 0.70, Air Coupling 1.00,
  Head Diameter 1.80 m, Pitch +12 st) and **99** readings miss by more than ±20
  cents, three of them by more than 50. On the reverted engine the worst is
  **4213.2 cents** and **66,926** miss, which is worse than the −1106.3 this entry
  measured before the grid because the transform written down is now applied on
  top of a drum that has already changed.

  Cost, re-measured: **143.1 µs** to resolve all four octaves against **10.6 µs**
  reverted. The ratio the entry recorded stands and the absolute figure came down
  with the two octaves that went.

  The revert proof was re-run rather than trusted. Replacing the bisection with
  `transformed (octave, drum)` — the transform written down, which is what this
  step replaced — fails **32** assertions, **13** of them in this step's own test:
  the six octave-ratio clauses at Octave Body 0.7 and 1.0 (+2145.9 / +2093.8 /
  +2061.3 and +2532.4 / +2458.4 / +2401.6 cents against 1200 ± 20), the two
  do-no-harm clauses at octaves 1 and 2, and the five strongest-partial clauses
  (+1791.1 / +2195.2 / −8176.5 cents against the recorded +229.4 / +1183.1 /
  +1196.8). The other nineteen are in the cavity test and the four-drums test,
  which measure the drums this step puts on the keys. Two groups do *not* fail,
  and that is the point of them: every clause at octave 0, because the transform
  is the identity there on both trees, and the three octave steps at Octave Body
  0, which read **+1198.5 / +1181.5 / +1196.7** cents reverted — the three figures
  this step's own text quotes as the re-measurement, reproduced to the digit, and
  the reason the step says the defect is entirely the Octave Body transform.

  Three figures above are void rather than stale, because they name octaves that
  no longer exist: the six-octave pitch list, the ×0.578-to-×0.514 radius ladder
  at Octave Body 1, and "Don Rim at octave −2 renders 0.9907". Nothing is on the
  limiter now — over the sixteen strokes of the grid at velocity 0.92 the loudest
  render is Don Rim at C5 at **0.5651** of full scale.

  The breathing branch is still short of an octave and neither step closes it: it
  steps 759.6 / 956.6 / 1181.8 cents at the shipping default, 761.2 / 961.2 /
  1144.8 at Octave Body 0.7 and 599.2 / 974.9 / 1151.0 at 0, and those are the
  literals the cavity test now carries. The comparison this entry drew — that the
  branch got *worse* by this step — cannot be drawn on the new keyboard, because
  on the reverted engine the fundamental is not stepping an octave either (the
  branch reads 1882.8 / 2308.7 / 2395.2 cents against the fundamental's 2532.4 /
  2458.4 / 2401.6), so the two trees are not measuring the same interval. What
  survives is the statement gap 7 needs: the drum's own lowest mode lands on the
  key that names it, and the branch above it does not.

### Considered and not planned

**Ringing the striker as well as the struck (gap 6). Struck on
implementation.** It was step 3 of this pass. The mechanism is not in doubt and
is not withdrawn: a contact force acts equally and oppositely on both bodies
(Chatziioannou and van Walstijn 2015), `strikeProfile` sets `usesDrumBody` on
the seven strokes that touch the drum, and the free-free bar model at
`resolveStickFor` — which is correct, and which this pass verified to the digit
— is therefore reached by exactly one stroke in eight. A Katsu is oak on
zelkova with the oak missing. All of that is still true and gap 6 stays open.

**Every baseline the step quotes reproduces exactly.** Measured with a
standalone program linked against `TaikorDSP`. `resolveStickFor` at the factory
Bachi Hardness and octave 0 gives 497.71 / 1371.95 / 2689.56 / 4445.99 /
6641.54 / 9276.20 Hz. `voice.contactReference` is 1983.7157 for a Katsu and
582.2498 for a Su at velocity 1.00 and octave 0, a ratio of 10.649 dB. The
Bachi stroke's stick bank peaks at `|drive · micLeft|` = 4.077e-05 against
2.309e-05 for a Don's loudest membrane mode and 1.065e-05 for a Su's. The
shell's six run 56.78 / 160.60 / 307.94 / 498.00 / 730.56 / 1005.52 Hz at Shell
Material 0 and 176.67 / 499.69 / 958.11 / 1549.47 / 2273.05 / 3128.56 Hz at 1.
`activeModeCount` is 29 / 30 / 30 for a Don, a Su and a Katsu.
Re-taken on the finished tree, all of that stands except the three
`|drive · micLeft|` figures, which read 3.782e-05, 2.319e-05 and 7.900e-06 —
0.65, 0.04 and 2.58 dB from the values above. The frequencies, both shell sets,
`contactReference` and `activeModeCount` are identical to every digit, so the
three that moved are a difference of estimator or of tree rather than of engine,
and none of them is load-bearing.

**It was implemented, twice, and both implementations work.** The six stick
slots stop sharing the shell's (`resonatorCount` 46 → 52, the `static_assert`
at `TaikoEngine.h:308` withdrawn), `buildVoiceModes` builds the stick's bank
alongside the shell's for the seven drum strokes at the stick's own
frequencies, decays and radiating area, with no per-articulation gain and with
`extraDamping` deliberately not reaching it — a palm on the head does not damp
the stick, which is also what keeps a Su's stick identical to a Katsu's. The
stick is kept out of the `peakMagnitude` that sets mode retirement. Every
structural clause the step asked for was met: `woodFrequencies` for a Katsu
returns twelve entries, the stick's six unmoved between Shell Material 0 and 1
while the shell's six move as tabulated above, and the summed membrane
`audibleSamples / rate` came out **unchanged to every printed digit** —
84.7068 / 69.7793 / 53.0711 s for a Don, a Su and a Katsu — with no membrane
mode falling from a non-zero lifetime to zero. (Those three read
84.3978 / 69.6241 / 52.8863 s on the finished tree, 0.3 % lower and flat in
velocity to five digits. The difference is step 4, which landed afterwards and
moved the axisymmetric pair: this entry was written before it.)

**The level is the whole of it, and the engine cannot supply one.** The stick's
level against the drum is `stickCalibration · radiatingArea / (modalMass · ω)`.
`stickCalibration = 150` is an admitted stand-in — its own comment says the
model has the stick's area and mass "but not the directivity of a small
cylinder held over a drum" — and it was set on the one stroke where the stick
is the entire sound. That, re-measured, is the whole of the problem, and the
paragraph that follows blamed the wrong term for it: ~~`modalMass` is worse: the
engine describes the bachi twice, and the two disagree.~~ The engine does
describe the bachi twice and the two do disagree, but reconciling them would
leave `stickCalibration` exactly as free as it is now.

~~| octave | drum radius | `drumContactTerms` striker mass | `resolveStickFor` bar mass | ratio |~~
~~|---|---|---|---|---|~~
~~| −2 | 1.2535 m | 0.8661 kg | 0.2696 kg | 3.21 |~~
~~| −1 | 0.7716 m | 0.5331 kg | 0.1907 kg | 2.80 |~~
~~| 0 | 0.4750 m | 0.3282 kg | 0.1348 kg | 2.43 |~~
~~| +1 | 0.2924 m | 0.2020 kg | 0.0953 kg | 2.12 |~~
~~| +2 | 0.1800 m | 0.1244 kg | 0.0674 kg | 1.84 |~~
~~| +3 | 0.1108 m | 0.0766 kg | 0.0477 kg | 1.61 |~~

**The table above does not reproduce and is withdrawn**; the re-measurement is
below and the two masses are not what killed the step. `bachiMass · a/a_ref` is
a lumped striking mass that scales with the drum ("nobody hits a shime-daiko
with an odaiko club"); `resolveStickFor` is a 24 mm × 400 mm dowel that scales
with the octave; they are the same object in the same collision and they do
differ. But the size of the difference as written belongs to no state of this
tree. Re-measured on the tree steps 1 and 2 left, Humanise 0:

| octave | drum radius | `drumContactTerms` striker mass | `resolveStickFor` bar mass | `stick.modalMass` | ratio to modal mass |
|---|---|---|---|---|---|
| −2 | 1.1058 m | 0.7640 kg | 0.2696 kg | 0.1348 kg | 5.67 |
| −1 | 0.7296 m | 0.5041 kg | 0.1907 kg | 0.0953 kg | 5.29 |
| 0 | 0.4750 m | 0.3282 kg | 0.1348 kg | 0.0674 kg | 4.87 |
| +1 | 0.3052 m | 0.2109 kg | 0.0953 kg | 0.0477 kg | 4.42 |
| +2 | 0.1937 m | 0.1338 kg | 0.0674 kg | 0.0337 kg | 3.97 |
| +3 | 0.1215 m | 0.0840 kg | 0.0477 kg | 0.0238 kg | 3.52 |

Two things were wrong. The radius column is a clean geometric series in 2^−0.7 —
the exponent the factory Octave Body of 0.7 names — and `resolveDrumFor` does
not produce one: the measured ratio between neighbouring octaves runs 0.660 at
the bottom of the keyboard to 0.627 at the top, and only octave 0 agrees with
the withdrawn table. The striking-mass column follows the radius, so it is wrong
wherever the radius is. And the bar-mass column is right but is the wrong
quantity: what sets a mode's level in this engine is `stick.modalMass`, half the
bar's mass, and it is the column added above. Against the mass that actually
carries the level the two descriptions are **3.52× to 5.67× apart, or 10.9 to
15.1 dB**; against the bar as tabulated they are 1.76× to 2.83×, or 4.9 to
9.0 dB. Neither is the 1.61× to 3.21× and 4.1 to 10.1 dB the entry gives.

**It was implemented a second time, independently, and the verdict holds while
the reason does not.** Built exactly as the step asks — `resonatorCount` 46 → 52
so the stick stops sharing the shell's slots, the `static_assert` withdrawn, the
stick's own frequencies, decays, radiating area and modal mass driven at unity
gain for the seven strokes that touch the drum, `extraDamping` not reaching it,
and the bank kept out of the `peakMagnitude` that sets mode retirement. Unity
gain is not a choice: Newton's third law hands the stick the whole contact
force, which is why the stick-on-stick stroke already drives this bank at unity,
whereas `profile.shellGain` describes a path through the hoop that the stick has
no equivalent of.

It works, and it is far too loud. The step's headline effect — a stroke's summed
level in four ±10 % bands around the first four stick frequencies (497.71,
1371.95, 2689.56, 4445.99 Hz) over the first 10 ms, Hann-windowed, velocity
1.00, octave 0, factory Shell Material — rises by **+10.95 dB on a Katsu, +14.76
on a Su and +15.06 on a plain Don**, against the 6 dB the step required. The
Bachi stroke is untouched at −37.83 dB, as it must be. Two suite clauses go red,
and the two figures the previous entry gives for them reproduce to the fourth
decimal: the loudest single stroke goes from 0.8056 to **0.9587** against the
0.95 the limiter clause allows (Don Rim), and Shell Resonance's authority over a
Katsu falls from 2.4168× to **1.7363×** against a floor of 2. The third failure
that entry reports, the brightness of a full-velocity stroke over a ghost note,
did not occur here.

The audible verdict is worse than the two red clauses. A **Don** — an open head
stroke a hand's width in from the middle of a 95 cm ō-daiko — gains 15.1 dB
across 500 Hz to 4.4 kHz and 3.0 dB of peak (0.3631 → 0.5113), and at octave −2
five further strokes that were nowhere near it are driven into the safety
limiter: Don 0.5345 → 1.0000, Katsu 0.7757 → 1.0000, Ka 0.5721 → 1.0000, Tsu
0.4238 → 1.0000, Buzz 0.3536 → 1.0000. That is not a bachi heard under a drum;
it is the drum replaced by a woodblock.

**The level is a free parameter, and this is what kills it.** The striker's
level against the head is `stickCalibration · radiatingArea / (modalMass · ω)`,
and `stickCalibration = 150` is the only term in it that is not derived — its
own comment says the model has the stick's area and mass "but not the
directivity of a small cylinder held over a drum". What pins it is that the
Bachi stroke should sound right, and that stroke pins it almost nowhere.
Measured by silencing the stick's own bank on a Bachi and comparing r.m.s., the
bank is worth **33.85 dB of that stroke at octave −2, 18.39 dB at −1, 5.63 dB at
0, and 0.26, −0.04 and 0.02 dB at +1, +2 and +3**. Above the reference octave
the stick-on-stick stroke is its own airborne click and nothing else.

And where it is pinned it is degenerate with an output trim. `excitationScale =
peakForce · profile.levelScale` multiplies the whole bank, so halving
`stickCalibration` to 75 and doubling the Bachi stroke's `levelScale` from 0.55
to 1.10 leaves that stroke **unchanged at the octave where it is the stick**:
peak 0.239505 against 0.239517 and r.m.s. 0.0232855 against 0.0233200, which is
0.0004 dB and 0.013 dB, and 0.001 dB and 0.08 dB at octave −1. The same edit
moves the striker on every drum stroke by exactly 6 dB and **takes both red
clauses green** — 0.8648 against the limiter's 0.95 and 2.0478 against Shell
Resonance's floor of 2, the same figures a plain 6 dB attenuation of the striker
gives, on which the whole suite passes — while the Katsu's headline rise falls
to +5.18 dB, under the 6 dB the step asks for. Attenuating by 12 dB instead
leaves +1.12 dB.

The trade is not exact everywhere, and the place it breaks is the second half of
the argument rather than a hole in it. Above the reference octave the Bachi
stroke does change under it, because `levelScale` scales that stroke's airborne
click as well as its bank — but up there the bank is worth a quarter of a
decibel of the stroke, so what is being compensated is the click's level, which
is its own free constant. Wherever the stick is actually audible in the stroke
that names it, the trade is silent.

So the window in which the striker is audible and the window in which it does
not damage the instrument do not overlap, and the number that decides which side
of that boundary it lands on can be moved six decibels without changing the one
stroke it is calibrated by anywhere that stroke is the stick. Shipping it means
choosing a level so that the suite passes, which is the definition of drawing a
curve, and this document already refuses that for the stand modes on weaker
grounds.

**Two clauses of the verification contract were wrong and are corrected here**,
because whoever re-attempts this will start from them. `activeModeCount`
**cannot** stay at 29 / 30 / 30: it is a total over the whole bank, and its
present value is 23 / 24 / 24 membrane modes plus the shell's six, so admitting
six audible stick modes necessarily takes it to 35 / 36 / 36. The invariant the
guard is actually about is the membrane's 23 / 24 / 24, and it held. And "Su's
rise is the larger of the two" holds only at Shell Material 0. At the factory
Shell Material of 0.8 the two strokes' existing content in those four bands is
4.94 dB apart, not the 18.0 dB measured at Shell Material 0, which is less than
the 10.65 dB their stick excitations are apart — so **Katsu's rise is the
larger there**, +8.35 dB against Su's +4.04 dB at the quieter derivation and
+15.84 against +10.77 at the louder. The clause is Shell-Material-dependent and
was derived at one end of a control the audio measurement never pinned. The
better clause, and the one worth keeping for a future attempt, is that after
the change Katsu minus Su in those bands moves from 4.94 dB towards the stick's
own 10.65 dB ratio: it reads 10.02 dB at the louder derivation, which is the
mechanism showing itself.

Preflight's own audio baselines for those four bands did not reproduce exactly
either: it read −24.07 / −33.79 dB at Shell Material 0 and 1 over a rectangular
10 ms and −38.49 / −35.84 over a Hann 10 ms, where the same estimator here
reads −24.19 / −34.53 and −40.92 / −37.59. Same phenomenon, up to 2.4 dB apart,
which is a further warning that this region is not measurable without pinning
the render length as well as the window.

**What has to be true before this can be re-attempted.** ~~One bachi.~~ The
gate is radiation, not mass. Reconciling the two bachi descriptions is worth
doing on its own account — either `drumContactTerms` solves the contact against
the stick `resolveStickFor` describes, which moves contact times and therefore
level and brightness on all seven drum strokes and is a step in its own right
with the first pass's velocity calibration to re-establish, or `resolveStickFor`
learns the drum's size the way the striking mass already does, which contradicts
the reason that function exists — but it is not what stands between the engine
and a striker. Even with one bachi the level would still be `stickCalibration`,
and `stickCalibration` would still be free.

What would fix that is the term the constant openly stands in for. Every
membrane mode in this engine is radiated through `radiationEfficiency (order,
ka)` at `:539`; the wooden bank is not, and takes a bare `radiatingArea`
instead. The stick is the case where the difference is enormous: a 12 mm bar at
its first bending mode of 497.71 Hz has *ka* = 0.109, which that same function
puts at 0.0118, or 19.3 dB down, while at the fourth mode of 4446 Hz *ka* = 0.98
and it is 3.1 dB down — a 16 dB tilt across exactly the region the step
measures, on top of about 11 dB of level. A bachi is nearly silent at 500 Hz for
the same reason a tuning fork is, and the head it is hitting is a piston. So the
striker's level against the drum is derivable the moment the stick radiates
through the same law the head does, and not before. That is a change to the
wooden bank on all eight strokes, it moves the shell as well as the stick, and
it belongs to whichever pass takes it on. Until then the striker has a spectrum
and no level, and gap 6 is a gap about the level as much as about the modes.

**One test was added**, where the previous entry says none was. Its reasoning
was that everything here is a fact about two constants a reader can check in ten
lines of source; that was true of the withdrawn mass table and is not true of
what actually killed the step. How far `stickCalibration` is pinned by the
stroke it is named for is a property of a render, not of a constant, and it is
the first thing a re-attempt needs to know.
`testTheStickBankIsOnlyCalibratedAtTheBottomOfTheKeyboard` measures the stick
bank's share of the Bachi stroke over the keyboard, through a new
`silenceWoodenBank` hook in `TaikoEngineTestAccess` that is the exact analogue
of the `silenceContinuum` hook struck step 2 left behind, and requires it to be
above 25 dB at octave −2, above 12 dB at −1, under 1 dB from +1 up, and
monotonically falling. It is a guard and cannot fail today. It bites on both
ends of what it pins: at twenty times the calibration it reads 17.92 dB at
octave +1 and 6.40 dB at +2 against the 1 dB clause, and at zero it reads
0.000000 dB against the 25 dB and 12 dB clauses.

**Stopping the attack glide from spraying the top of the spectrum (gap 14).
Struck on implementation.** It was step 2 of this pass. The mechanism it names
is real: `applyTensionShift` rewrites `a1`, `a2` and `b0` under a running
`(y1, y2)`, and that does move the next output by *Δa1·y[n−1] + Δa2·y[n−2]*.
What is wrong is the size, and the size was the entire case for the step —
"at the factory Tension Mod every band above 1 kHz is currently sitting on
about 6 dB of this."

**The 7.2 dB is the estimator.** Every band level in gap 14 and in the step's
verification clause is a Parseval sum over the integer DFT bins of a
rectangular window. A rectangular window's sidelobes fall as one over the
frequency offset, so a partial that is not exactly on a bin leaks across the
whole spectrum, and a Don at C3 puts nearly all of its energy into two octaves
that the 30–80 ms window cuts through mid-cycle. Demonstrated on a signal whose
spectrum is not in doubt, and now asserted in the suite: **a single 0.3-amplitude
sinusoid at 1000.3 Hz reads −68.60 dB in the 4–10 kHz band through that
estimator and −164.25 dB through the same sum under a Hann window** — 96 dB of
content that is not there. On unit white noise, where there is genuinely
something in every band, rectangular, Hann and Blackman–Harris agree to 0.3 dB,
so the estimator is sound everywhere except the case gap 14 used it for. And the
flatness the pass took as proof that nothing physical was involved — "flat to a
tenth of a decibel across four octaves" — is the signature of that leakage
rather than of a step train: the leakage of one strong partial has the same
shape in every band, so the ratio of two renders of it is flat by construction.

**Measured honestly, over the same 30–80 ms window with an eight-pole
high-pass run from the strike so that the filter is settled and no window is
involved at all:**

| | Tension Mod 0 | Tension Mod 1 | rise |
|---|---|---|---|
| continuum silenced, >1.2 kHz | −119.9 dB | −119.9 dB | −0.00 dB |
| continuum silenced, broadband | −18.3 dB | −18.3 dB | +0.02 dB |
| shipping, >1.2 kHz | −57.20 dB | −55.94 dB | +1.26 dB |
| shipping, >4 kHz | −84.26 dB | −83.47 dB | +0.79 dB |

So the rewrite leaves 101.6 dB of headroom under the stroke that made it, and
the glide moves it by nothing. What Tension Mod really buys above 1 kHz is about
a decibel, and it is the continuum's own bands retuning with the head — the
mechanism the engine means to have. For scale, the instrument's ordinary onset
makes one-sample steps 8.5 dB under its running peak (gap 13); this one is 100 dB
under.

**What the step asked for, for the record**, since its body has been removed
from the checklist: a `testTheGlideRetunesWithoutSplattering` rendering a Don at
velocity 1.00 and Humanise 0 at Tension Mod 0 and 1 with the voice's continuum
silenced, measuring 1.2–2.4 kHz, 4–10 kHz and 12–20 kHz over 30–80 ms; each rise
under 1.5 dB, the top band under 0.5 dB, the spread between the three under
1.0 dB, 40–125 Hz moving under 0.3 dB, `appliedTensionShift` unchanged sample for
sample, and a Don at Tension Mod 0 bit-identical. The continuum-silencing hook it
specified was worth building and is now in `TaikoEngineTestAccess`; the bands and
the window were not, and are replaced by the settled high-pass described above.

**The fix was implemented anyway, and it fails the step's own test.** The
tree this step was picked up on carried a draft of it — the `(y1, y2)` pair read
as a sampled sinusoid, its quadrature partner recovered from the old
coefficients as *(y1·cos ω − r·y2)/sin ω*, and `y2` rewritten at the new pole
angle. The arithmetic is correct and the retune is continuous in the output.
Run against the step's own acceptance clause — three bands over 30–80 ms with
the continuum silenced, each of which "must rise by less than 1.5 dB" —
it reads **+7.66 / +7.76 / +7.77 dB**, against +7.38 / +7.48 / +7.48 without it.
The step's fix does not move the step's number, because the number is not about
the engine.

Two further findings from that draft, both reasons not to keep it on its own
merits. It makes the honest quantity **worse**, not better: above 4 kHz with the
continuum silenced, Tension Mod 1 reads −181.9 dB without the rotation and
−164.4 dB with it. The shift it would then be tracking exactly is a peak
follower over the modal states, which is corner-rich at audio rate, and a state
that lags the retune smooths that where a state that follows it does not. And it
**broke two existing tests** — `testTheContinuumDoesNotDependOnTheSampleRate`'s
400 Hz–16 kHz clause, which step 1 had just landed at 1.09 dB against a 1.5 dB
limit, and the ghost-stroke bite in the voice-stealing test. The draft was
removed and the suite went green.

The one thing in it worth salvaging is not what the step claimed. Preserving a
mode's amplitude across a retune is the physically right thing to do, and the
un-rotated rewrite does not: across an instantaneous two-semitone wheel jump on
a ringing tail it costs **0.39 dB** of 40–125 Hz. That is a claim about the
wheel and about Pitch automation, not about the top of the spectrum, and it
wants its own step, its own measurement over the wheel's whole range, and a test
that fails without it. It is a candidate for the next pass alongside the hand.

**One consequence for a step already struck.** "Letting the head's own
stretching pump the continuum (gap 4)" was struck because its proposed test —
4–10 kHz over 30–80 ms rising at least 6 dB from Tension Mod 0 to 1 — "already
passes on the shipping engine, at +7.38 dB", and gap 14 was the explanation.
That +7.38 dB is the same leakage. Measured honestly the figure is +0.79 dB, so
that test would *not* pass today and the step is measurable now rather than
after step 2. It still has gap 2 in front of it, and that is the reason it stays
struck; the reason recorded there is wrong and this is the correction.

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
relighting bands 2 through 5 changes nothing a listener reaches.
*Corrected when step 2 was struck.* The +7.38 dB is a rectangular window's
leakage, not `applyTensionShift`: measured with a settled high-pass the same
quantity is +0.79 dB, so the proposed test does **not** already pass and there
is nothing in the way of seeing a new mechanism. The premise quoted above is
also, as far as this can be measured, correct — the state-preserving retune adds
0.00 dB above 1.2 kHz. What keeps this step struck is gap 2 alone, and it is
measurable today rather than after step 2. See the step 2 entry above. The whole-range
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
spectrum. Note that it shares a mechanism with the striker step, which was
struck for the neighbouring reason: both want the contact solve to know more
about what is being struck, and about what is doing the striking, than
`drumContactTerms` currently tells it.

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

**Two-handed structure in Humanise.** *Superseded by “Explicit polar strike
placement — 2026-08-16” below.* `triggerVoice` reseeds per stroke and
draws position, angle, speed and contact time independently, so a roll varies
randomly where a player's two hands vary periodically — in level, in timing and,
because the stereo image comes from the mode shape at each microphone, in
left–right position. It is genuinely differentiating, since round robins are
random too and no sampler can produce the periodic structure either. It was not
inferred because the engine cannot know the sticking: a MIDI stream does not say
which hand played a note, and assuming strict alternation would make a
one-handed passage rock left and right when it should not. The later tranche
adds explicit coordinates and still refuses that heuristic.

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

**Documentation and demonstration audio.** Steps 1, 3, 4 and 5 all move
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
*Done, and it needed more than two.* What the README carried that the pass made
false is listed in the closing section below.

### What the pass came to

**Three steps landed, two were struck on implementation, and four had been
struck in review before that.** What shipped is: the head's high-frequency
continuum weighed against the drum's own modal receptance instead of against a
per-sample integration gain, so its level no longer follows the host clock; the
enclosed air given the reactive input stiffness of a finite column, solved to
convergence once per drum; and the octave transform solved against the pitch the
drum sounds rather than written down as an ideal membrane's. Each is guarded by
a test that fails on its revert — 2 assertions, 12, and 21 of which 17 are in
the step's own test — and the suite is green on the tree handed on.

One accounting note, because the counts in this document do not add up. The
opening paragraph says the pass was written with eight steps; the checklist
carries five and the rejects four, which is nine described. It reads as the
cavity column having been carried in from the first pass's deferred list rather
than drafted alongside the others, and nothing about the work turns on which
total is meant, but the arithmetic should not be taken as a record of anything.

**Against what the pass set out to do, that is a miss on its own subject.** It
was written about the top of the spectrum, and nothing in the top of the
spectrum moved except its independence from the sample rate. Every step that
would have shaped that region band by band is now struck, and all of them for
one reason, recorded as gap 2: the continuum's bands are differences of doubled
one-poles with 12 dB/octave skirts and a level law falling as *f*^−1.5, so the
crossover band is louder than every band above it in that band's own octave and
no per-band physics can be heard through it. What the pass did instead is the
keyboard and the cavity, which were the two largest measurable defects it
found, and it should be read as a pass about the bottom of the instrument that
was commissioned about the top.

**What was struck on implementation, and why.**

*Step 2, stopping the attack glide from spraying the top of the spectrum.* The
premise was a measurement artefact. Gap 14 and the step both rested on a
Parseval sum over the integer bins of a rectangular window, whose sidelobes fall
only as one over the frequency offset; a Don at C3 puts nearly all of its energy
into two octaves and leaks across the rest of the spectrum at a level unrelated
to what is there. Measured with a settled eight-pole high-pass and no window,
what the coefficient rewrite leaves above 1.2 kHz is 101.6 dB under the stroke
that made it, and Tension Mod 0 → 1 moves it by −0.00 dB. The step's own fix was
implemented and made the step's own number *worse* (+7.66 / +7.76 / +7.77 dB
against +7.38 / +7.48 / +7.48 without it), which is what a number that is not
about the engine does. The finding is pinned in the suite on a synthetic sine,
so the half of it that cannot rot is asserted rather than remembered.

*Step 3, ringing the striker as well as the struck.* The mechanism is right, was
implemented three times across two independent attempts, and met every
structural clause the step asked for. What killed it is that the new component's
level is `stickCalibration`, a constant that stands in for the directivity of a
small cylinder held over a drum and that nothing in the engine measures. At the
level Newton's third law gives it — unity gain, which is what the stick-on-stick
stroke already uses — the striker takes the loudest single stroke from 0.8056 to
0.9587 against a 0.95 limiter clause, costs Shell Resonance a third of its
authority over a Katsu, adds 15.1 dB across 500 Hz to 4.4 kHz and 3.0 dB of peak
to a plain open Don, and drives five more strokes at the bottom octave into the
limiter. Six decibels down the suite is green and the headline effect is +5.18
dB against the 6 dB the step requires. Those same six decibels are invisible in
the only stroke that calibrates the constant: halving it and doubling that
stroke's output trim moves a Bachi at octave −2 by 0.0004 dB. So there is no
level that is both audible and safe, and the boundary between them is a number
chosen so that the suite passes. Gap 6 stays open and is now gated on giving the
wooden bank the same radiation law the head already has, which is the term
`stickCalibration` stands in for.

**Where reality differed from the plan.** Every one of these is written out in
full under the step it belongs to; this is the list.

- *Step 1.* The 400 Hz–16 kHz spread after the change is 1.09 dB, not the
  prototyped 1.34. The 40–200 Hz spread went 0.24 → 0.29 dB rather than staying
  put. "No behaviour at 48 kHz changes" is true to 0.0000 dB of band level but
  is not bit-identity: re-anchoring a `float` constant by a factor that is not a
  power of two moved a twelve-stroke render by 1.04e-07 against a peak of 0.443.
- *Step 4.* The solve is a bisection and not the damped fixed point the step
  specified, because half-damped iteration fails to settle in 60 of 16200
  configurations of the step's own scan. The clamp corner the draft named
  converges to exactly 0 and reports breathing = loaded = 560.4384 Hz, not the
  0.0002 and the 560.4 / 562.8 split, which were an unconverged iterate. The
  preflight's lower-branch clause is 1.5 % and not 1 %: the prototyped 0.47 %
  does not reproduce and the measured worst case is 1.1668 %. And step 1's
  rectangular 4–10 kHz clause had to be widened from 2.0 to 3.0 dB, with a
  Hann-windowed clause added alongside at 1.5 dB — tighter than the original
  ever was — because the same leakage that killed step 2 makes that band's
  rectangular reading move a decibel when the breathing branch moves four hertz.
- *Step 5.* The bisection has to carry resolved drums rather than the share it
  chose, because re-resolving the winner at the end is the same arithmetic
  written twice and the two copies round differently across the share where the
  sounding pitch steps: 219.04 Hz against 235.20 on one corner, decided by the
  compiler. The step's strongest-partial clause is unsatisfiable as drafted and
  was corrected rather than met — the top two boundaries improve from 70.0 and
  43.3 cents of error to 4.2 and 3.2 and are asserted as octaves, while the
  C3-to-C4 boundary gets 115 cents worse and measures which mode won rather than
  what the drum is tuned to. The ±6 % local-maximum clause needs 0.32 dB, not
  0.25. Three of step 4's clauses were superseded and re-taken on the same
  drums, and one of them is an honest regression: the breathing branch's octave
  steps went 582.4 / 682.8 / 859.5 / 1038.5 / 1153.1 → 508.7 / 610.8 / 775.8 /
  956.1 / 1093.7 cents, because the keyboard now applies less transform than it
  used to and that branch was being dragged along by the surplus. Step 4's own
  claim survives it and was checked rather than assumed: on the same drums with
  the column replaced by the lumped spring, the five steps are 451.0 / 570.2 /
  747.8 / 936.7 / 1078.8, so the column still widens every one.
- *Baselines that did not reproduce.* At 384 kHz the 4–10 kHz band reads
  −9.78 dB where gap 1 says −9.77. Gap 14's +7.15 / +7.23 / +7.24 dB reads
  +7.38 / +7.48 / +7.48 with the same estimator, and its continuum-on companion
  +3.24 / +7.38 / +8.28 reads +1.74 / +7.92 / +9.60. The striker step's
  preflight audio baselines are up to 2.4 dB from what the same estimator reads
  here, which is a warning that the four-band 10 ms measurement it used needs
  the render length pinned as well as the window. And the striker step's own
  mass table does not reproduce at all: its radius column is a pure geometric
  series in 2^−0.7 that `resolveDrumFor` does not produce, agreeing only at
  octave 0, so the striking-mass column that follows the radius is wrong
  wherever the radius is, and the bar-mass column — right in itself — is not the
  mass that carries a mode's level. The table is withdrawn with the rejects and
  re-taken there. Everything else quoted in this
  pass reproduced to the digit, including all six stick frequencies, both sets of
  shell frequencies, `contactReference` at 1983.7157 and 582.2498, the
  `activeModeCount` triple of 29 / 30 / 30, the contact-time pair and the Mic
  Distance sweep — and, on the second striker implementation, the two suite
  figures the strike turns on, 0.9587 and 1.7363.
- *Two clauses of the striker step's verification contract are wrong* and are
  corrected with the rejects, because whoever re-attempts it will start from
  them: `activeModeCount` cannot stay at 29 / 30 / 30 (the invariant is the
  membrane's 23 / 24 / 24), and "Su's rise is the larger of the two" holds only
  at Shell Material 0.
- *Step 3 was worked twice.* The second agent handed step 3 found the strike
  already written up here, with the checkbox unticked and the engine unchanged,
  and re-did it from the source rather than ratifying it: the striker was built
  again, measured again, and struck again. That was worth the duplicate work.
  The verdict and the two suite figures it turns on survived, the mass table
  given as the reason did not survive at all, and the argument that replaces it —
  the calibration constant is degenerate with an output trim over six decibels —
  is stronger than the one it replaces and could not have been reached by
  reading the entry.
- *Three process facts the orchestrator should have.* The tree step 2 was picked
  up on arrived red, carrying an undocumented complete draft of step 2 inside
  `applyTensionShift`; removing it is what took the suite green, and if that
  draft was deliberately committed then its removal is a revert of committed
  code. And during step 5 `Source/DSP/TaikoEngine.cpp` was destroyed by a
  scripted edit and recovered by reading and decompressing the HEAD blob out of
  `.git` by hand — no git command was run, but a file in the working tree was
  written from the object store, and it was verified to carry steps 1, 2 and 4
  and to take the suite green before work continued. A single read-only
  `git status --porcelain` was also run during step 4, against the brief.

**The honest bounds on what can now be claimed.**

- *The continuum.* Its level is a property of the drum rather than of the host
  clock: 4–10 kHz of a Don over a fixed 85 ms window holds within 0.93 dB
  through a Hann-windowed Parseval sum and 2.51 dB through a rectangular one
  across 44.1 / 48 / 96 / 192 kHz, against 8.73 dB before. Nothing about its
  *shape* changed, its bands are still not separable, and the 8 kHz region of the
  largest drums is still 2.7 ms of decay that Head Damping cannot move. Gap 2 is
  open and is now the gate on four struck steps.
- *The cavity.* Reactive, and only reactive. The air has the input stiffness of
  a rigidly terminated half-column and no mass, no loss and no resonances of its
  own; past the quarter-wave it is treated as absent rather than as the mass it
  becomes. Body Depth still cannot change the length of a boom, and the reason
  is now a number rather than an omission — thermal exchange with the walls
  gives *η* ≈ 1.1e-4, three orders under what radiation already takes. Gap 8 is
  half closed.
- *The keyboard.* An octave is an octave in the drum's own lowest mode, at every
  setting of Octave Body: 1200.0 cents at 0.0, 0.7 and 1.0, worst error 50.9
  cents over a 155,520-drum control scan with 102 outside ±20 cents, every one
  of those a drum whose column has passed its quarter-wave or whose geometry has
  hit a clamp. It is *not* an octave in the breathing branch — 508.7 / 610.8 /
  775.8 / 956.1 / 1093.7 cents — and it is not an octave in the loudest partial
  below C4, which at C1 and C2 belongs to a mode with a circumferential order
  and to neither axisymmetric branch. Gap 7 is half closed and the open half is
  the larger one.
- *What did not move.* Everything a listener hears above about a kilohertz,
  except its level at sample rates other than 48 kHz. The striker still does not
  ring on the seven strokes that touch the drum. CC1 is still a flat gain
  envelope rather than a damper (1.37 dB of tilt over seven octaves against the
  articulation mute's 6.19). Mic Distance still moves the continuum's level and
  not its tilt. An articulation's loudness is still a gain applied after the
  contact is solved. Strike Position still has dead travel.
- *Cost.* Both landed solves are bisections at drum-resolve time and neither is
  per sample. A drum resolve roughly doubles, 1.40 → 2.85 µs; a six-octave cache
  refresh goes 15.7 → 179.8 µs, which is 13 % of a 64-sample block at 48 kHz in
  the worst case of a pitch-wheel sweep and 1.7 % of a 512-sample one.

**What the README needed, and it was more than the two corrections predicted
above.** The finite column and the continuum's units were both written in, as
planned. Beyond them: the octave table's fundamentals were the pre-step-5 ones
and are now 13 / 25 / 51 / 101 / 203 / 406 Hz; the Octave Body section described
a transform that is written down; the breathing branch's "with the column it
steps 582" is a figure step 5 superseded, and is replaced by the shipped 509 and
the lumped spring's 451 on the same drums; the cavity factors at octave −2 moved
to 0.80 and 0.71; the mounting section's per-octave figures were re-measured; the
head's top-mode stretch three octaves up is 120 cents rather than 150, because
that is a different drum now; the microphone section claimed that backing the
pair off "softens the slap", which measurement contradicts by 4 dB in the other
direction; the ghost-to-full span and the factory rim shot's peak were re-taken;
and the stick-on-stick section still called the Bachi stroke the twelfth of
twelve. What is *not* in the README is anything about a striker that rings or a
glide that sprays, because neither shipped.

**Everything above this paragraph was written before the 4x4 playing grid
landed, and this block re-takes what the grid moved.** None of the three landed
steps changed in the source; what changed under them is the keyboard they are
measured on, and each of the three was re-verified end to end on the shipping
tree rather than carried over — the mechanisms read out of the source, the
figures reproduced with scratch programs linked against `TaikorDSP`, and every
revert re-run. The per-step entries above carry those notes in full. The suite is
green on the tree this documentation was written against (2 tests, 23.1 s).

- *The revert counts.* Step 1 still fails two assertions. Step 4 fails
  **thirteen**: the twelve in its own test, plus one literal in step 5's test
  that was taken with the column in place, which is collateral rather than a
  second discriminator. Step 5 fails **thirty-two**, thirteen of them in its own
  test and the other nineteen in the cavity test and the four-drums test, which
  measure the drums it puts on the keys.
- *The breathing branch, and a claim that does not survive.* "The column still
  widens every one of the five octave steps" was true of a keyboard made by
  rescaling one drum. It is not true of four instruments. On the four drums the
  branch steps **759.6 / 956.6 / 1181.8** cents; with the column replaced by the
  lumped spring on the same drums it steps **768.9 / 964.3 / 1134.0**, so the
  column narrows the bottom two boundaries and widens the top one. The old
  argument was that a lumped spring stiffens as 1/L while a scaled drum grows in
  every dimension at once, and four drums whose bodies run 0.81, 0.66, 0.50 and
  0.21 m at 0.85, 1.20, 1.25 and 0.70 diameters are not a scaling, so it does not
  point one way any more. What survives is the statement gap 7 needs, and it
  survives at either stiffness: the branch is not an octave. The test asserts the
  three literals and that the steps rise, and records why it no longer asserts
  the comparison.
- *The keyboard's scan.* 25,920 configurations of eight controls crossed with
  four octaves, 103,680 octave readings: worst **+50.9 cents** and **99** outside
  ±20, against 4213.2 cents and 66,926 on the reverted engine. The grid section
  below scans the same claim on a different grid and reads 43,740 boundaries,
  worst 56.2 cents, 81 outside ±20. Both are the shipping tree.
- *The cavity's scan* is 10,800 configurations rather than 16,200, the cavity's
  whole authority over the lower branch measures 1.95 %, and the rendered
  breathing mode moves from the lumped spring's 88.522 Hz to the column's 84.484.
  The four drums' column factors are 0.868 / 0.782 / 0.611 / 0.740, the okedo
  being the softest because it is the longest body in the family relative to its
  head.
- *Cost.* A cache refresh is four drums rather than six octaves: **143.1 µs**
  against 10.6 reverted.
- *Two entries in "what did not move" name a keyboard that is gone.* The striker
  does not ring on the **four** strokes of the grid, and the free-free bar model
  it would have used was deleted with the stick-on-stick stroke, so gap 6 now
  needs the model built again as well as calibrated. Everything else in that
  list — CC1, Mic Distance's missing tilt, the articulation level trim, Strike
  Position's dead travel — is exactly as open as it was.
- *Three README corrections this section did not predict, each measured here.*
  The list of calibrated constants said five and the engine carries six: the
  radiating efficiency of a lifted tack was missing, and it is in the same role
  as the airborne click's. The worst left-right correlation the README quoted
  over the microphone controls was +0.08; re-measured here over the four strokes
  on all four drums, with Stereo Width, Mic Distance and Mic Spread swept in
  tenths, it is **+0.19** — a Ka on the shime with the pair down on the head and
  fully opened. The suite's own coarser sweep, which reads octave 0 only, reaches
  +0.31 and asserts nothing inverts and that the sweep gets below +0.5.
  And the Mic Distance sweep, which step 1 measured at 17.23 and 13.53 dB
  before step 4 moved the drum under it, now reads **17.62 dB** at 400–1200 Hz
  and **13.62 dB** at 4–10 kHz, so backing the pair off leaves the top of the
  drum four decibels brighter relative to itself rather than the 3.71 dB step 1
  recorded.

## The 4x4 playing grid — 2026-08-08

A user-requested design change rather than a step out of the passes above, and
it is the largest change the instrument has had to what a player touches. The
keyboard was eight strokes over six octaves of one drum rescaled. It is now
**four drums by four strokes**, and each of the four drums is its own instrument
with its own physics.

### What the keyboard is now

Sixteen notes, C3 to D♯6, and exact silence everywhere else.

| Octave | Note | Drum | Head | Body | Hide | Shell | Sounds at |
| --- | ---: | --- | ---: | ---: | ---: | --- | ---: |
| C3 | 48 | Ō-daiko | 95 cm | 0.81 m (0.85×) | 0.96 mm | carved zelkova | 50.75 Hz |
| C4 | 60 | Chū-daiko | 55 cm | 0.66 m (1.20×) | 0.67 mm | carved zelkova | 101.49 Hz |
| C5 | 72 | Okedo-daiko | 40 cm | 0.50 m (1.25×) | 0.44 mm | stave-built, light | 202.99 Hz |
| C6 | 84 | Shime-daiko | 30 cm | 0.21 m (0.70×) | 0.35 mm | carved, thick-walled | 405.98 Hz |

The bottom four semitones of each octave are C = Don, C♯ = Ka, D = Tsu,
D♯ = Don Rim.

### Where the four drums come from

`drumDescriptionTable` in `Source/DSP/TaikoEngine.cpp` states each drum in the
same control units the parameter block uses, and every column has a source.

- **Diameter** is the instrument as built: a 3-shaku ō-daiko, a 1.8-shaku
  nagado, a standing okedo, a tsuke-shime.
- **Body depth** is the instrument's own proportion. The engine reads
  *depth / diameter = 0.40 + 0.90 × control*, so the four rows are 0.85, 1.20,
  1.25 and 0.70 diameters: a barrel about as deep as it is wide, a *nagado*
  ("long body") at a fifth longer than wide, a stave tub longer still, and a
  shime that is a shallow ring of wood.
- **Head material** is the hide. The control maps geometrically onto 0.30–1.60
  kg/m², and over a hide at about 1000 kg/m³ the four rows are 0.96, 0.67, 0.44
  and 0.35 mm of skin — heavy cowhide, lighter cowhide, and the thin horse or
  calf hide an okedo and a shime carry.
- **Shell** sets the body's ring modes, their Q, the wall thickness and how much
  the rim absorbs. The ō-daiko and the chū-daiko are both carved keyaki (0.80 and
  0.74); the okedo is the outlier and has to be, at 0.20, because it is thin
  cedar staves bound with hoops rather than a carved log; the shime is carved
  keyaki again at 0.92, small and proportionally thick-walled.
- **Tension** is the one column a player sets rather than a maker, because a
  drum's tension is whatever brings it to the pitch it is wanted at. The four
  come out at **5.94 / 5.54 / 8.30 / 14.80 kN/m**: the two *byō-uchi* drums at
  much the same tension as each other, and the *shirabe*-laced ones far above
  them, with the shime at 2.5× the ō-daiko on a head a third as thick.

The o-daiko row is bit-identical to the shipping parameter defaults, so the
reference drum did not move at all: 50.7475 Hz before and after.

### That they are four instruments and not four sizes

A similarity transform of one drum into another preserves every dimensionless
ratio it has, and so does the octave transform to the extent that it is one. So
the claim is made in ratios, measured at the shipping defaults:

| | Ō-daiko | Chū-daiko | Okedo | Shime |
| --- | ---: | ---: | ---: | ---: |
| Depth ÷ diameter | 0.850 | 1.200 | 1.250 | 0.700 |
| Breathing ÷ fundamental | 1.658 | 1.285 | 1.117 | 1.105 |
| Head stiffness *B* (×10⁻⁴) | 2.186 | 2.405 | 0.898 | 0.449 |
| Top of the resolved bank above ideal | 30.4 ¢ | 33.4 ¢ | 12.6 ¢ | 6.3 ¢ |
| Tail × fundamental (cycles) | 169 | 251 | 295 | 780 |
| Hide (kg/m²) | 1.053 | 0.780 | 0.550 | 0.450 |
| Tail (s) | 3.32 | 2.47 | 1.45 | 1.92 |
| Full Don, 20–63 Hz, vs the ō-daiko | 0 dB | −16.5 | −21.3 | −31.1 |

Every pair of drums differs by more than a tenth in at least one of the first
five, and by more than a sixth in the hide. The closest pair in the modal ratios
is the ō-daiko against the chū-daiko (2.186 against 2.405, 10 %), because both
are thick tacked cowhide, and they are 41 % apart in the shape of their bodies.
The closest pair in the cavity split is the okedo against the shime (1.117
against 1.105, 1 %), and they are a factor of two apart in head stiffness and
79 % apart in body proportion.

Two of those are worth reading as sound rather than as numbers. The shime's
shallow body barely splits its axisymmetric pair, so it is a far more nearly
pure pitch than the ō-daiko, whose air lifts the second branch by a sixth. And
the okedo is the driest of the four in absolute time despite not being the
smallest, because a light stave shell takes 1.656 s⁻¹ at the rim against the
ō-daiko's 0.688 and the shime's 0.495 — a mechanism no rescaling has.

### Octave Body kept its name and changed its job

It used to choose whether an octave was bought by halving the drum or by
quadrupling its tension. There is nothing left to buy that way, because the
octave now selects a different instrument, so the control now chooses **how much
of the family there is**: at *Tuned* all four octaves collapse onto the ō-daiko
the controls describe and the keyboard is one drum retuned (which is bit-for-bit
the old Octave Body 0 behaviour); at *Family* each octave is its own instrument.
In between they are blended.

It keeps its second job unchanged: the residual tuning that brings each drum
onto its key is taken on the axis the control chose. At *Tuned* that is the head
tension and it is the whole octave (×4.007 / ×4.093 / ×4.009). At *Family* it is
the drum's size and it is under half a per cent — the resolved diameters are
95.0 / 55.2 / 40.0 / 29.9 cm against a table that says 95 / 55 / 40 / 30.

**The default moved from 0.7 to 1.0** (*Family*), because the point of the
change is four drums.

The octave is still exactly an octave in the drum's own lowest mode: 50.75 /
101.49 / 202.99 / 405.98 Hz at every Octave Body. Over a scan of eight controls
crossed with the four octaves — 43,740 boundaries — the worst error is 56.2
cents and 81 (0.19 %) miss by more than ±20, every one of those a drum whose air
column has passed its quarter-wave or whose geometry has hit a clamp.

The solve itself changed in one place: the reference the transform is measured
against used to be *this octave's own untransformed drum*, which was the right
reference when every octave was one drum rescaled. It is now the drum the
controls describe, resolved untransformed, because each octave's own drum is a
different instrument and tuning each of them an octave above itself would have
left the keyboard reading the family's intervals rather than octaves. The
bracket now reaches both ways — a real drum can sound above the key it is put on
as easily as below it — and runs 24 halvings of a bracket at least two octaves
wide.

### What was retired, and what was checked before retiring it

**Four articulations came off the map.** Su, Katsu, Buzz and Bachi. Su was a
light Don and velocity already covers it (33.6 dB on the ō-daiko at full
Velocity Depth, 39.1 on the shime). The other three were one technique each.

**The stick-on-stick model was retired with the Bachi stroke, and this is the
one genuine deletion.** `StickState`, `resolveStickFor`, `stickCache_`,
`StrikeProfile::usesDrumBody`, `stickResonatorCount`, `stickCalibration` and the
nine stick constants are gone. It was checked first: `usesDrumBody` was read in
exactly three places (`trigger`, `measureContact`, `dampRingingHeads`) and the
stick bank in one (`buildVoiceModes`), and the only articulation that set
`usesDrumBody = false` was Bachi. Nothing else in the engine, the plug-in, the
editor or the demo renderer reads the stick. Two tests went with it —
`testStickStrokeIsIndependentOfTheDrum` and
`testTheStickBankIsOnlyCalibratedAtTheBottomOfTheKeyboard` — because both
trigger an articulation that no longer exists. The consequence for gap 6 ("ring
the striker as well as the struck") is that the free-free bar model would have
to be built again; the second pass had already struck that step because
`stickCalibration` was pinned by nothing but the Bachi stroke, so what is lost
is code that could not be calibrated rather than a calibration.

**The wooden shell bank stayed, and Katsu was not the only thing driving it.**
Every stroke has a `shellGain` — Don 0.18, Ka 0.42, Tsu 0.12, Don Rim 0.82 —
and Don Rim adds `rimGain × 0.35` on top, so the body is driven hardest by the
stroke that catches the hoop. `shellDecayScale` is still used (Don Rim, 0.78).
`shellFrequencyScale` is now unity on all four strokes; it was already unity on
all eight and is left in place as a column of the profile.

*Superseded on 2026-08-17:* the later Okedo audit below found that this copied
the solved head force into an uncoupled shell observer. The release bank is now
driven only by Don Rim's explicit hoop contact.

**The byō tack line is untouched.** Don Rim (`rimGain` 0.95) and the near-rim
Ka (0.30) reach the tack line, but only Don Rim directly catches the hoop and
beats the preload at ordinary velocities, which is the mechanism that separates
that pair.

**One mechanism is now dormant rather than deleted: the multi-contact
schedule.** `scheduleContacts` still builds a train of bouncing contacts when a
profile asks for more than one, and no surviving profile does — Buzz was the
only one. It is kept because it is the same function that schedules the single
contact every stroke has, and because `voice.retirementOffset`, the contact
relight of the continuum and the mode-lifetime shift all exist to serve it. A
press roll is now played rather than provided, and the demo renderer plays one.

### Tests

Three new tests in `Tests/TaikoEngineTests.cpp`, each proved to bite by
reverting the change it guards, running, and restoring:

- `testTheGridIsFourByFourAndTheRestIsSilent` — all 128 MIDI notes, the three
  mapping functions agreed against each other, and each note rendered: exactly
  16 sound and the other 112 are *exactly* zero. Reverting
  `articulationForMidiNote` to wrap the pitch class produced 226 failures.
- `testTheFourDrumsAreFourInstruments` — the resolved drums are the sizes,
  shapes and hides the table names; four fundamentals an octave apart in the
  readout *and* in the rendered partial; and every pair of drums separated in
  the dimensionless ratios above. Reverting `parametersForOctave` to return the
  parameter block unchanged — one drum rescaled — produced 33 failures.
- `testTheFourStrokesAreMutuallyDistinct` — pairwise band-level signatures,
  each normalised to its own loudest band so level cannot stand in for timbre,
  with the closest pair at 4.5 dB (Don against Tsu) and the widest at 18.1 dB
  (Tsu against Don Rim); plus Tsu's sustain at 49 % of Don's, and Don Rim
  reaching the hoop 3.2× harder than Ka. Flattening the strike profiles to four
  copies of one strike fired all three clauses.

Six existing tests were re-taken rather than relaxed, because the drums they
measured are different drums now: the cavity column test's shallow-body and
deep-body cases (moved from octaves 3 and −2 onto the shime and the okedo, with
the lumped-spring literals re-measured by forcing the column factor to one), the
breathing-branch step floors, the corner table's two octave −2 rows, the scan
count (16,200 → 10,800), Octave Body's tension clause, and the strongest-partial
ladder.

Two clauses changed the quantity they measure, and the reason is the same in
both: with the bachi-on-the-shell stroke retired, no articulation isolated the
wooden bank from the head and hoop transient. "Shell Resonance has no step in
it" therefore reads the wooden bank's own drive; "the glide must not retune the
wooden shell" reads the wooden modes' resonator coefficients and asserts exact
equality, which is stronger than the old audio proxy.

One clause was loosened with a measured reason rather than re-taken. The
do-no-harm clause that the reported fundamental is where the rendered peak is
went from −0.32 dB to −0.92 dB, because the chū-daiko costs 0.909 dB and what
costs it is the attack glide: the analysis window opens 50 ms after a stroke
that starts the head sharp, and the strongest bin sits 13 cents above the pitch
the drum is tuned to. With Tension Mod at zero all four drums come in under
0.01 dB.

### The demonstration set

Twenty-three takes became **twenty-five**, and `expected_demos` for Taikor is
updated in both `.github/workflows/ci.yml` and `.github/workflows/nightly.yml`.
The set is rebuilt around the grid: the four strokes on one drum, the four drums,
the whole sixteen-note grid read out, two strokes taken across the drums, a
phrase on each of the four instruments, and a rolls-and-flams take that plays a
press roll and a flam from the notes the grid has. `20-octave-body.wav` is the
one to hear the change in — the same four pitches played first as one drum
retuned four times and then as the four instruments.

The committed WAVs under `Docs/audio/` are stale until the nightly render
refreshes them: this change was made under a brief that forbids running the
renderer against that directory. The manifest's level table was taken from a
render to a temporary directory and is exact.

### What did not change

The physics. No loss law, no radiation term, no cavity solve, no contact solve,
no microphone geometry and no calibration constant moved, apart from
`stickCalibration` being deleted with the model it belonged to. The reference
drum at C3 is bit-identical. Every open gap recorded above is exactly as open as
it was, and gap 6 is one deletion further from being closed.

---

## Step 6 — the keyboard's octaves in the pitch a listener names

### The regression

Step 5 solved the octave transform against the drum's **loaded fundamental** —
the lower branch of the air-loaded axisymmetric pair — and `measureDrum` duly
reported four fundamentals on exact octaves: 50.747 / 101.495 / 202.990 /
405.980 Hz, 0.0 cents out on every boundary.

That is not the pitch the instrument sounded. Measured from rendered audio as
the strongest partial of a 0.9 s window opening 80 ms after a Don, searched
blind over 0.4× to 4× the fundamental:

| Drum | loaded (0,1) | dominant radiated | ratio | heard interval | intended |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ō-daiko | 50.75 | 88.96 | 1.753 | +0.00 | +0 |
| Chū-daiko | 101.50 | 174.92 | 1.724 | +11.71 | +12 |
| Okedo | 202.99 | 203.09 | 1.001 | **+14.29** | +24 |
| Shime | 405.98 | 405.97 | 1.000 | **+26.28** | +36 |

The four pads stepped 0 / 11.7 / 14.3 / 26.3 semitones. The okedo sat barely two
semitones above the chū-daiko and the family spanned 26 semitones where it should
have spanned 36. And the chū-daiko had no stable pitch at all: a Don at velocity
0.85 read 174.92 Hz, the same stroke at 1.00 read 102.51, and a Tsu at 0.85 read
101.59 — two of its modes were within a decibel of each other and which one won
depended on how it was hit.

### The cause

On a large drum the (0,1) lower branch is not what is heard. It moves the two
heads against each other, so it has no monopole moment and reaches the current
front observer only through the near field — and it sits down where the mounting
term is steepest. The later finite-separation correction restores its dipole
radiation without changing that observer. On the shipping ō-daiko its decay was 12.5 s⁻¹, of
which 11.6 came from the stand and the hoops: a T60 of 0.55 s. The (1,1) mode a
fifth and a half above it decayed at 3.8 s⁻¹, a T60 of 1.84 s, and over the
window a listener takes a pitch from it won by five to seven decibels — at every
microphone distance and spread, so this was a property of the drum and not of the
pair.

Whether it wins is decided almost entirely by where the fundamental sits against
the mounting's corner, which scales as 1/a: the deciding quantity is *f₀₁·a*, and
therefore the head's wave speed. Measured across the family it ran 24.1 / 28.0 /
40.7 / 60.7 (m/s, up to a constant), with the crossover at about 35. The two
tacked drums are below it and the two rope-laced ones are above it, which is the
*byō-uchi* / *shirabe* divide and not a coincidence: a tacked cowhide simply
cannot be brought to the wave speed a laced shime runs at.

### The fix

**The transform is now solved against the mode the drum is actually heard at.**
`TaikoEngine::observeMode` builds one membrane mode exactly as `buildVoiceModes`
builds it, reduced to the three numbers a comparison needs — where it is, what a
full open stroke is worth in it at the pair, and how fast it empties — and
weights it by `A/d · (e^{−d·t₀} − e^{−d·t₁})`, which is the closed form of what
any measurement of "the strongest partial" over that window reads.
`TaikoEngine::soundingMode` takes the largest of those over the modes low enough
to be a drum's pitch at all: under twice the fundamental's wavenumber, which
admits the two branches of the (0,1) pair and the (1,1) mode and nothing else.
Measured against rendered audio the weights are good to about a decibel and a
half over that set.

`resolveDrumFor` picks that mode once, solves the bisection against its frequency,
then asks the answer what *it* is heard at and solves again if it disagrees. The
second pass only ever matters at Octave Body 0, where the whole octave is bought
with tension and a drum retuned that far genuinely changes which mode it is heard
at. `DrumMeasurements::soundingHz` reports it, and the editor's pitch readout is
now that rather than the fundamental.

**The drum table was retuned around what that implies.** A family heard at its
(1,1) at the bottom and at its fundamental at the top has to span a factor of
fourteen in the fundamental to span eight in what is heard. The shime's end of
that is fixed by the tension a laced hide can hold — about 500 Hz on a 30 cm head
at the model's 22 kN/m ceiling — so the ō-daiko's has to be near 33 Hz, and no
95 cm head with a tacked cowhide on it is. A five-*shaku* ō-daiko at an ordinary
7.3 kN/m is, and is also the drum the bottom of a kumi-daiko set actually is. The
chū-daiko went to 2.5 *shaku* for the same reason and to get it decisively onto
one of its modes: at 55 cm its fundamental, its breathing branch and its (1,1)
were within a decibel of one another, which is exactly the tie that made its pitch
unstable.

| | Was | Now |
| --- | --- | --- |
| Ō-daiko | 95 cm, 5.9 kN/m, 1.05 kg/m² | **150 cm, 7.3 kN/m, 1.05 kg/m²** |
| Chū-daiko | 55 cm, 5.5 kN/m, 0.78 kg/m² | **78 cm, 5.8 kN/m, 0.85 kg/m²** |
| Okedo | 40 cm, 8.3 kN/m, 0.55 kg/m² | 40 cm, **11.5 kN/m**, 0.55 kg/m² |
| Shime | 30 cm, 14.8 kN/m, 0.45 kg/m² | 30 cm, **19.1 kN/m, 0.41 kg/m²** |

The anchor is the reference drum's own sounding pitch, as it has always been —
the transform is the identity at octave 0 for every Octave Body — and that is now
**59.66 Hz**. Anchoring on the shipping ō-daiko's 88.96 would have put the shime
at 711.7 Hz, which needs a 24 cm head at the tension ceiling and is not a shime.

### The measurements

Factory settings, 48 kHz, strongest partial of a 0.9 s window opening 80 ms after
the strike, each channel scanned on its own, over Don and Tsu at velocities 0.35,
0.85 and 1.00:

| Drum | Sounds at | Heard, over six strokes | Spread | Step |
| --- | ---: | ---: | ---: | ---: |
| Ō-daiko | 59.660 | 59.56 – 59.68 Hz | 3.3 ¢ | — |
| Chū-daiko | 119.320 | 119.49 – 119.97 | 7.0 ¢ | +1207.1 ¢ |
| Okedo | 238.639 | 238.65 – 238.81 | 1.1 ¢ | +1194.7 ¢ |
| Shime | 477.279 | 477.25 – 477.39 | 0.5 ¢ | +1199.5 ¢ |

Worst error from exact octaves: **7.1 cents**. Worst spread on any one pad across
the six strokes: **7.0 cents**. Both were 970 and 948 cents before.

The model's own figures step +1200.0000 / +1199.9999 / +1200.0000 cents, and the
four fundamentals are now deliberately *not* octaves: 32.650 / 68.048 / 238.639 /
477.278 Hz.

### What guards it

`testTheFourDrumsStepInHeardOctaves`, which reads nothing but rendered audio.
Three clauses per drum: the strongest partial may not move more than 15 cents
across Don and Tsu at three velocities each; it must sit within 20 cents of the
pitch the engine reports; and the step onto each pad must be an octave within 20
cents. The tolerances are twice the worst measured and no more; what is left in
them is the attack glide, which starts the head sharp and is worth six cents on
the slackest head of the family.

Reverting the solve to the loaded fundamental and the drum table to the shipping
one fails all three: the chū-daiko's strongest partial moves 101.52 to 175.49 Hz
(948 cents) across the six strokes, its step reads 857 cents and the okedo's 571.
Reverting only the solve still fails the two step clauses at 1140 and 219 cents.

### Tests re-taken

Four existing tests encoded the old (0,1)-based contract and now state the new
one, because the claim they were written to make is about the pitch the
instrument sounds:

- `testTheDrumIsTunedByThePitchItSounds` — every octave boundary at three Octave
  Body settings, the reference anchor, and the blind strongest-partial ladder,
  all moved from `loadedFundamentalHz` onto `soundingHz`. Its Pitch-control guard
  stayed on the fundamental, and the comment says why: an octave of Pitch really
  does hand the reference ō-daiko from its (1,1) to its own fundamental, and the
  strongest partial goes 59.7 → 84.4 → 94.7 → 65.3 Hz across that range. The
  shipping tree does the same thing (89.0 → 101.5 over an octave of Pitch); it is
  a property of the instrument and is **not fixed by this step**.
- `testTheFourDrumsAreFourInstruments` — the octave clause moved onto
  `soundingHz`; the built sizes, hides and dimensionless signatures re-taken.
- `testTheCavityIsAColumnNotAnInfiniteSpring` — every literal re-measured against
  the drums the table now describes, the lumped-spring references re-taken by
  forcing the column factor to one. Two clauses — the cavity factor's
  monotonicity in Body Depth, and the bound on how far the cavity can move the
  lower branch — are now asserted at the reference octave only. The reason is in
  the test: away from octave 0 the drum is not a fixed function of the controls,
  and now that the transform follows whichever mode the drum is heard at, opening
  the body far enough can hand it from one mode to another and move the answer by
  a fifth and a half. At octave 0 the transform is the identity and both clauses
  say exactly what they were written to say.
- `testTheDrumSoundsLikeADrumAndNotLikeATone` — the register clause moved onto
  `soundingHz`, where the reference drum reads 59.7 Hz.

### Floors restated, with the measurement

Five measured floors were fitted to a three-*shaku* reference drum and are
restated against a five-*shaku* one. Each is a statement about the same
mechanism at a different size, not a mechanism that has gone:

| Clause | Was | Now | Why |
| --- | ---: | ---: | --- |
| Body between 250 Hz and 4 kHz | 10 % | 8.5 % | The whole resolved bank sits most of an octave lower; the same sum reads 10.9 % at 95 cm and 8.6 % at 150 |
| A roll against eight strokes added offline | −3.0 dB | −0.5 dB | The head-stretch interaction goes as the fourth inverse power of the radius, so 1.58× in size is 6.2× in that term. Measured −0.95 dB |
| Rim-shot high band against a centre stroke's | +2.0 dB | +1.5 dB | Measured 1.78 dB |
| Slack head's modal spread against a tight one | ×1.02 | ×1.015 | *B* = D/(T a²) falls as the square of the radius: 0.72e−4 here against 2.19e−4 before. Measured 1.017 |
| Sounding pitch against the rendered peak | −0.92 dB | −1.41 dB | The attack glide again, on the slackest head of the family. Measured 0.978 / 0.858 / 0.994 / 0.997 |

The 4–10 kHz sample-rate spread went from 3.0 to 4.0 dB and is the estimator, not
the audio: a rectangular window's sidelobes fall only as one over the offset and
the drum's bottom two octaves have just moved. The windowed clause beside it —
"which is the audio and not the estimator" — is unchanged at 1.5 dB and reads
0.98.

### Two model changes came with it

- **The tack line is a spacing, not a count.** `tackPreload` was the head's
  tension over a forty-eighth of the circumference, which made the force a stroke
  has to beat rise with the drum: on a five-*shaku* head forty-eight tacks is one
  every 98 mm, each holding down nearly three times what a nagado's does, and a
  full rim shot could no longer lift one. Iron tacks are driven at a spacing —
  about 36 mm, which is what forty-eight of them round a 1.8-*shaku* hoop is — so
  the preload is now `tension × 0.036 m` and is the same force on every drum of
  the family. Nothing else about the tack line scales with the drum.
- **The factory Output default came down 2.5 dB**, from −20.0 to −22.5 dBFS. A rim
  shot catches the hoop and the body as well as the head, and the body of a
  five-*shaku* drum is a great deal more of the stroke; the loudest of the sixteen
  keys in that single-seed audit peaked at −0.80 dBFS. The later million-order
  audit in **Bounded tack spectrum** found a rarer crest and supersedes this
  headroom evidence without changing the public Output parameter.

### What is still open

The Pitch control does not transpose the heard pitch of the reference drum by an
octave, because an octave of tension moves the ō-daiko across the balance between
its (1,1) mode and its own fundamental. It is not a regression — the shipping
tree does the same — but it is the obvious next thing: the fix would be to solve
the pitch control the way the octave transform is solved, which means giving up
the identity at octave 0.

At Octave Body 0 the model's four sounding pitches are exact octaves —
59.66 / 119.32 / 238.64 / 477.28 Hz, against 0 / 157 / 1341 / 2538 cents of step
before this — but the C5 pad's Don is not: all three velocities read 96.6 Hz,
which is the reference ō-daiko's own shell ringing. *Tuned* means one drum
retuned, the shell is not retuned with the head, and a five-*shaku* carved body's
first ring mode is at 98 Hz. Turning Shell Resonance down removes it. It is a
statement about the body rather than about this solve, and it is new only in that
the reference body is now big enough for its ring to land under the head two
octaves up.

Nothing in `soundingMode` accounts for the attack glide, which smears a partial
out of its own bin for a time proportional to how high it is. That is why the
comparison is bounded to the modes below twice the fundamental's wavenumber:
above it the weights drift several decibels high. A glide-aware weight would let
the bound go.

The per-fix audio previews under `Docs/audio/` are one-time review evidence and
are not re-rendered here.

### Step 6, follow-up — the argmax that tuned the keyboard

Two defects, one cause. Both were introduced by this step and both are fixed
here.

**Defect 1: Pitch automation lurched.** `resolveDrumFor` solved the octave
transform against `soundingMode()` — the loudest of a drum's modes over the
window a pitch is taken from. That argmax is what puts the four heard octaves
where they belong, and it is also a step function of every control that feeds
it. Two of this family's modes sit within a decibel of each other over wide
stretches of the control space, and wherever they crossed on the *reference*
drum, the quantity every other octave was solved against jumped and every
transformed drum re-solved for radically different geometry. Measured at factory
settings, from rendered audio:

| Pitch | chū-daiko heard | reported | head radius |
| ---: | ---: | ---: | ---: |
| 7.48 st | 183.84 Hz | 183.77 | 0.238 m |
| **7.49 st** | **100.65 Hz** | 100.63 | **0.404 m** |

A hundredth of a semitone — one step of ordinary Pitch automation — dropped the
drum 1043 cents and moved its head by 70 %, so the timbre lurched with it. It
was never a property of the Pitch control. A geometry scan of eleven controls
crossed with three Octave Body settings, 9,600 steps each, found the same
crossing reachable from nine of them. At Octave Body 1, thirty-six crossings in
all:

| Control | crossings | worst single-step change in the solved drum |
| --- | ---: | ---: |
| Pitch | 10 | 117.8 % |
| Head Tension | 9 | 70.5 % |
| Head Diameter | 6 | 62.3 % |
| Mic Spread | 4 | 63.8 % |
| Air Coupling | 2 | 65.5 % |
| Resonant Tension | 2 | 7.2 % |
| Head Material | 1 | 68.3 % |
| Mic Distance | 1 | 23.2 % |
| Octave Body | 1 | 52.5 % |

Head Damping joins them at Octave Body 0.5 (one crossing, 40.9 %), and at
Octave Body 0 the worst single step is 234.8 % of the drum's tension, on Pitch
at −4.52 semitones.

**Defect 2: the readout lied when the strike moved.** `observeMode` always
evaluated a Don at the profile's factory radius, ignoring
`EngineParameters::strikePosition`. An off-centre stroke genuinely excites a
different balance of modes — that physics was right and stays — but the reported
pitch did not follow it:

| Strike Position | chū-daiko heard | chū-daiko reported | error |
| ---: | ---: | ---: | ---: |
| 0.00 | 119.80 Hz | 119.32 | −7 cents |
| −0.25 | 171.94 | 119.32 | −633 |
| −0.50 | 171.96 | 119.32 | −633 |

At −0.50 the reported frequency sat 23.4 dB below the strongest partial in the
take. Same cause: one function was being asked two different questions, and its
one fixed stroke could only answer one of them.

#### The fix

`TaikoEngine::tuningModeFor` latches a mode *identity* — a row of the mode table
and a branch of the cavity-split pair — rather than taking an argmax. The octave
solve uses it on both sides of the comparison: the reference drum's tuning mode
and this octave's tuning mode, both read off the family the drum table describes
at the factory controls. Which mode an instrument is heard at is a property of
the instrument, not of where the player has left Head Tension. The two-round
fixed point inside `resolveDrumFor` is gone with it; the solve is now one
bisection tracking one named mode.

It depends on exactly one control, and it has to. Octave Body decides what the
four drums *are*: at *Family* the chū-daiko is heard at its (1,1), and at *Tuned*
the four collapse onto one ō-daiko, which taken up an octave is a drum whose own
fundamental has climbed clear of the mounting and become the loudest thing it
has. That handover is what makes the first octave at *Tuned* cost ×13.49 in
tension rather than ×4, and it is documented in the README. No single assignment
serves both ends: tuning the chū-daiko's fundamental onto the octave breaks the
family grid, and tuning a retuned ō-daiko's (1,1) onto it makes the first octave
at *Tuned* a step of 157 cents. So the identity is read off the drum the family
would build at the current Octave Body, transformed by the amount that would put
its *ideal* membrane fundamental on its octave — a closed form, so nothing
circular and nothing iterative. Past Octave Body it is a constant, which is what
makes it deterministic, independent of what was struck, and independent of the
order a host sets parameters in.

`observeMode` and `soundingMode` now take the strike radius from the caller.
The octave solve passes `tuningStrikeRadius()` — the centred full open stroke —
and says so in a comment, so Strike Position remains a timbre control with no
tuning side effect. `measure()` passes `readoutStrikeRadius(parameters)`, which
is the same arithmetic `trigger()` uses to place a Don, minus the humanising
jitter. `getFundamentalHz` and the editor's readout both come through
`measure()`, so both follow.

One consequence: the pitch-bearing bound in `soundingMode` had to widen from
twice the fundamental's wavenumber to the head's third radial order. The old
bound admitted the (0,1) pair and the (1,1) and nothing else, which was tenable
while the function was only ever asked about one fixed stroke. Asked where the
stick actually lands, it was excluding the mode the stroke drives: the
chū-daiko struck at the centre is heard at its (0,2) lower branch, and the old
bound left the readout naming a mode 1035 cents below anything audible.

#### The measurements

Continuity, from rendered audio, sweeping across each crossing:

| Sweep | worst step before | worst step after |
| --- | ---: | ---: |
| Pitch, 7.46 → 7.52 st, 0.01 st steps | 1042.9 cents | 1.2 |
| Pitch, −5.21 → −5.15 st, 0.01 st steps | 506.5 | 2.3 |
| Head Tension, 0.9160 → 0.9190, 0.0005 steps | 1042.7 | 1.4 |

A 0.01-semitone step is a cent of transposition in itself and a 0.0005 step of
Head Tension is 1.26 cents through that control's geometric map, so the "after"
column is the sweep itself plus estimator noise. The geometry scan above now
finds **one** crossing across all three Octave Body settings instead of
eighty-six, and it is the Octave Body handover, which was already there: 0.3189
before, 0.3589 after, 52.5 % and 50.1 % of the head's radius, with the sounding
pitch continuous through it either way.

> **Corrected by the fourth follow-up.** Recording that crossing as "already
> there" and leaving it is what this section got wrong. It is a step of 52 % in
> the solved head at one ten-thousandth of a control, and calling it survivable
> because the pitch is continuous through it was not the same as showing it had
> to be there. It does have to be there, and the fourth follow-up shows why with
> an argument this section never made.

The readout against the strike, from rendered audio, twelve takes:

| | before | after |
| --- | ---: | ---: |
| worst reported-vs-heard error, takes with one clear pitch | 632.7 cents | 7.0 |
| worst rendered level at the reported pitch | 0.068 (−23.4 dB) | 0.798 (−2.0 dB) |

Ten of the twelve takes have a partial at least 1.5 dB clear of everything else.
The two that do not are the ō-daiko at −0.25 and −0.50, where three partials land
within 0.6 dB; that drum has no single pitch struck near its middle, and the
model's ranking of the three is inside its own stated accuracy.

The factory heard-octave grid is bit-identical: 59.66 / 119.32 / 238.64 /
477.28 Hz reported and 59.64 / 119.76 / 238.74 / 477.33 heard, stepping
1207 / 1194 / 1200 cents, at every Octave Body setting the suite checks.
*Tuned*'s ×13.49 / ×4.05 / ×4.00 tension ladder and *Family*'s
150.0 / 78.0 / 40.0 / 30.0 cm diameters are unchanged.

#### What it costs

The keyboard is now exact octaves in the latched mode at every setting, and what
drifts instead is whether that mode is still the one the drum is heard at. Over
twelve settings spread across the controls, measured from rendered audio, five
have all three octave steps within 7 cents: the factory drum, Octave Body 0.5,
the body opened, a shallow body and a damped head. Before the fix eight of the
twelve were within 10 cents. The seven that are not, after, are Pitch ±7
semitones, a much tighter head, a much slacker one, a thin film, a much smaller
drum, and Octave Body 0 — the last of which was already broken and for an
unrelated reason, the reference shell's own 96.6 Hz ring, recorded above. On the
other six it is the bottom step that goes, and it goes a long way: C3 → C4 reads
33 cents on a thin film, 76 at Pitch −7, and 224 / 224 / 308 / 327 on the rest,
with C4 → C5 taking up the difference. The four instruments cross from their
(1,1) to their fundamental at different settings, and between one drum's
crossing and the next the two pads are heard in different modes.

That is the trade. The argmax chase hid those crossings by rebuilding the drum,
which is what produced the lurch in the first place — and which also meant Pitch
at +7 semitones shrank the chū-daiko's head from 39 cm to 24 cm, which is not
something a transposition control should do. Pitch now transposes and leaves the
instrument alone, the tuning is exact in one named mode everywhere, and nothing
moves in a step.

#### Tests

`testThePitchTransformIsContinuousUnderAutomation` sweeps the three crossings
above and asserts, from rendered audio, that no step moves the heard pitch by
more than 8 cents. `testTheReadoutFollowsTheStrikePosition` pins the reported
pitch to the rendered one across Strike Position 0.00 / −0.25 / −0.50 on all four
drums, within 20 cents wherever the take has one pitch, and pins the rendered
level at the reported frequency to within 2.5 dB of the strongest partial
everywhere. Both fail on the pre-fix engine — by 1043 cents and 633 cents
respectively — and pass after.

Two clauses of `testTheCavityIsAColumnNotAnInfiniteSpring` were re-taken. Two of
its five recorded corners moved because the drum under them did (167.8403 →
284.9433 Hz at octave 1 and 458.8257 → 988.7020 at octave 3, both away from the
reference octave and both at settings a long way from the four instruments); they
are kept with the new drums recorded, and the two corners the scan now finds
worst are added beside them, at 1.07 % and 1.00 % against a 1.5 % tolerance. The
count of scanned drums whose air column has passed its quarter-wave went from
2513 of 10800 (23.27 %) to 3045 (28.19 %) — all of the change away from the
reference octave, where the latched mode leaves a drum at a different tension
than the argmax chase did — so the sanity band moved from a quarter to a third
with both numbers recorded.

#### Still open

`soundingMode`'s weights and the rendered spectrum disagree by about a decibel
at a near-tie, which is inside the accuracy the function claims but is enough to
flip an argmax. It shows at the reference octave around Pitch 7.49, where the
readout steps from 91.9 to 50.3 Hz while the rendered take moves smoothly from
91.8 to 92.3 — the drum has not moved at all there, only the model's opinion of
which of its two partials is louder. That is a readout accuracy question rather
than a tuning one now that the tuning no longer reads the argmax, and it wants
the glide-aware weight the previous section already asks for.

### Step 6, second follow-up — the readout above the (0,2), and the glide that was never a level

Two defects in the comparison the readout makes between a drum's modes, one
thing that looked like a third and was not, and the glide-aware weight the two
sections above keep asking for — which turns out to be the same root cause as
the thing that was not a defect.

#### What was reported

Octave 1, chū-daiko, Don at velocity 0.85, Strike Position +0.50, Mic Spread
0.00, channel L, strongest partial of a 0.9 s window opening 80 ms after the
strike: **heard 68.3 Hz, reported 227.2 Hz, 2081 cents apart, with the rendered
level at the reported frequency 10.1 dB below the strongest partial.**

#### The 10.1 dB is the estimator, not the drum

Every membrane partial of a struck head sits *sharp* of where it settles for a
good part of that window: the attack glide is the head stretching itself, it is
a tension shift, and a tension shift scales the whole head at once — so it is
the same number of *cents* on every mode and a very different number of *hertz*.
A 0.9 s window resolves 1.11 Hz. Measured on the take above, with Humanise off:

| mode | settles at | sounds at | glide | level at where it sounds | level at where it settles |
| --- | ---: | ---: | ---: | ---: | ---: |
| (0,1) lower | 68.05 | 68.32 | +7.0 ¢ | 0.00 dB | −0.06 dB |
| (0,1) upper | 94.51 | 95.03 | +9.6 | −1.18 | −1.74 |
| (1,1) | 119.32 | 119.89 | +8.3 | −1.28 | −3.32 |
| (0,2) lower | 171.55 | 171.92 | +3.7 | −1.39 | −2.47 |
| (1,2) | 227.23 | 228.30 | +8.1 | **−0.76** | **−12.85** |

Eight cents is a fifth of a hertz at 68 Hz and 1.2 Hz — a whole window width — at
227. The partial the old readout named is 0.76 dB off the loudest thing in that
take, not ten decibels; a probe parked on its settled frequency simply misses it.

The same effect is what the two sections above record as *"the weights drift
several decibels high by the fourth radial order"* and what they propose a
glide-aware weight to correct. There is nothing to correct. **Measured where each
partial actually sounds — over Strike Position × Mic Spread × Mic Distance on
all four drums, 252 takes — the weights carry no drift with the radial order at
all, out to the ninth.** Per mode row, mean and standard deviation of
weight-minus-rendered in dB, over the modes the model ranks within 20 dB of the
top:

| mode | (0,1) | (0,2) | (0,3) | (0,4) | (1,1) | (1,2) | (1,3) | (2,1) | (3,1) | (4,1) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| mean | −0.06 | −1.34 | −0.44 | −0.04 | −0.36 | −0.26 | +0.60 | −1.53 | +0.33 | −0.40 |
| s.d. | 1.5 | 3.2 | 1.8 | 2.0 | 1.2 | 0.9 | 1.4 | 4.0 | 1.1 | 0.8 |

Every mean is inside a decibel and a half and none of them grows with the mode's
order. What scatter there is belongs to the microphone geometry rather than to
the mode: the two wide rows, the (0,2) and the (2,1), are wide because of what
happens at Mic Distance 1.00 and Mic Spread 1.00, and they are as wide at the
bottom of the bank as at the top.

A glide term cannot change a ranking anyway, for the reason the table above
shows: it multiplies every mode by the same ratio. That is now written down in
`soundingMode`, and the two open items asking for it are closed by it.

#### The two defects that were real

**The pitch-bearing bound excluded modes that win.** It stopped at the head's
third radial order. Struck at its middle with the pair open, the ō-daiko is
heard at its **(0,3) lower branch at 140.0 Hz** — the strongest partial of all
six of Don and Tsu at three velocities, 3 cents apart across them — and the
readout could not name it: it said 85.75 Hz, 849 cents below. With the pair
backed off, the chū-daiko is heard at its **(1,3) at 337 Hz** and the readout
said 94.51, 2202 cents below. The bound is gone.

**The stroke was modelled as an impulse.** A bachi rests on the head between one
and six milliseconds depending on how hard it is — 1.42 / 1.05 / 0.75 / 0.66 ms
across the four drums at the factory hardness, and 6.06 / 4.65 / 3.55 / 3.16 ms
with the beater at its softest — and a force pulse that long cannot drive a mode
whose period is not much longer than it. The render has always known this: it
drives the bank with the Hertz sin^1.5 arch. The comparison did not, so it
ranked the modes of a drum struck by something nobody owns. Measured on the
chū-daiko with the beater soft and the glide off, it put the (1,2) mode **8.9 dB
above** where the rendered take has it. `DrumState` now carries the contact time
for a neutral full open stroke, resolved beside the mounting and the microphones
and for the same reason, and `observeMode` multiplies by
`contactSpectrum (omega * tau)` — the Hertz pulse's own transform, fitted to
0.05 dB out to omega·tau = 6.

That second term is what decides the reported case. The five partials above are
within 1.4 dB of one another; the contact time is 1.05 ms there, which is worth
0.03 dB at 68 Hz and 0.38 dB at 227, and that is the difference between naming
one and naming the other.

#### The measurements

252 settings, Humanise off, each rendered six times — Don and Tsu at 0.35, 0.85
and 1.00. **A setting has a pitch when at least five of the six strokes are
heard at the same partial**, within 50 cents. One stroke of six landing
elsewhere is a near-tie falling the other way; two or more means the pitch
depends on how the drum is hit, and then no single number is right and the
readout is not asked for one.

| | before | after |
| --- | ---: | ---: |
| settings with a pitch | 201 of 252 | 201 of 252 |
| of those, readout wrong by more than 50 ¢ | **10** | **6** |
| worst error where there is a pitch | 2084 ¢ | 1552 ¢ |
| worst error on the ones it gets right | — | 19 ¢ |

and on the twenty settings the suite pins, which are the region rather than the
whole sweep:

| | before | after |
| --- | ---: | ---: |
| settings with a pitch | 17 of 20 | 17 of 20 |
| of those, readout wrong by more than 25 ¢ | **8** | **0** |
| worst error where there is a pitch | 2088 ¢ | **6 ¢** |
| worst rendered level at the reported pitch | 0.412 | **0.999** |

The reported case itself: **227.23 → 68.05 Hz**, against 68.19 Hz measured, an
error of 4 cents where it was 2084.

The estimator had to be fixed first, and it is worth recording. The suite's
`blindStrongestPartial` walks its band in constant *ratio* steps and refines
around whichever step read highest. A window of fixed length has a fixed
resolution in hertz, so a constant-ratio step is a wider slice of it the higher
it lands: 1.005 is an eighth of a window width at 68 Hz and a whole one at 228,
which costs a high partial up to 4 dB before the comparison that picks the
winner has even happened. It always favours the lower partial. The new test
refines every local maximum within ten decibels of the coarse best and compares
those; checked against a 262144-point transform of the same window over
twenty-four takes it agrees on all of them, and the single-pass search does not.
The twelve takes `testTheReadoutFollowsTheStrikePosition` pins read identically
under both, so nothing it records was an artefact.

#### Hard constraints, re-verified

- The factory heard-octave grid is **bit-identical**: 59.659817 / 119.319633 /
  238.639252 / 477.278503 Hz at Octave Body 1, stepping 1200.0000 / 1199.9999 /
  1200.0000 cents, and the same at Octave Body 0, 0.5 and 0.7. The latched
  tuning identities are e4/b0, e4/b0, e0/b1, e0/b1 at *Family* as before, and
  so are the solved tensions and radii to the last digit printed. The heard
  figures are 59.6 / 119.7 / 238.7 / 477.3 as before. Swept over Octave Body in
  hundredths the one thing that moves is the chū-daiko's handover, by a single
  step: it was at 0.36 and is at 0.37. That control is the one whose whole job
  is to change which instrument an octave plays, the pitch is continuous
  through it either way, and no rendered audio moves. (**Corrected by the
  fourth follow-up**: "that control's whole job" is a reason the handover has
  to happen, not a reason it has to be a step. The step is forced too, and by a
  different argument — see below.)
- `testThePitchTransformIsContinuousUnderAutomation`, `testTheReadoutFollowsThe`
  `StrikePosition`, `testTheFourDrumsStepInHeardOctaves` and
  `testTheFourDrumsAreFourInstruments` all pass unchanged. Nothing here touches
  the tuning path: `resolveDrumFor` reads only `ModeObservation::frequencyHz`,
  which no term added here moves, and `tuningModeFor`'s latched identities were
  re-measured and are the same rows of the same table.
- `Tests/PluginProcessorTests.cpp` asserts `measureDrum (0).soundingHz` is
  between 40 and 65 Hz at the default Strike Position. It reads 59.659817 before
  and after, so that file needed no change.

#### Tests

`testTheReadoutNamesThePartialTheDrumIsHeardAt` renders twenty settings six ways
each — the strike walked across the head crossed with both microphone controls
on all four drums, plus Pitch −7, Head Tension 0.40 and the beater at its
softest — and asserts, from rendered audio and from nothing the engine says
about itself, that wherever five of the six strokes agree on a partial the
readout is within 25 cents of it, and that at every setting the rendered level
at the reported frequency is within 0.85 of the strongest partial. It carries
the two estimator pieces the measurement needed: an unbiased blind search, and a
±20-cent band on the level clause for the glide, both documented at their
definitions.

Reverting the two changes by hand — the bound put back in `soundingMode` and the
`contactSpectrum` factor commented out of `observeMode` — fails it fifteen
times: eight pitch clauses, worst 2088 cents and including the reported case at
2084, and seven level clauses, worst 0.412. Restoring them passes all of it, and
the rest of the suite passes either way.

#### What is still open

Six of the 201 settings with a pitch are still wrong, and none of them is a
near-tie: the reported partial sits 1.1 to 3.0 dB below the strongest.

| setting | reported | heard | error |
| --- | ---: | ---: | ---: |
| Head Tension 0.20, okedo, centred | 321.3 Hz | 131.1 Hz | +1552 ¢ |
| Head Tension 0.20, okedo, +0.50 | 223.2 | 131.9 | +911 |
| Pitch −7, chū-daiko, −0.50 | 115.2 | 82.7 | +573 |
| Pitch −7, ō-daiko, centred | 93.9 | 77.5 | +333 |
| Mic Spread 1.00, Mic Distance 1.00, chū-daiko, centred | 335.7 | 171.7 | +1161 |
| Mic Distance 0.00, chū-daiko, at the rim | 277.4 | 119.8 | +1453 |

The first four are the slackest heads the controls reach — Head Tension at 0.20
and Pitch at −7 semitones — where the glide runs to 23 and 41 cents and the
mounting term is at its steepest, and where the weights are further out than the
1.5 dB the table above records. The last two are the model inside its own stated
accuracy and losing: it puts the chū-daiko's (1,3) 0.70 dB above its (0,2)
lower branch where the render has it 0.46 dB below, and its (0,3) upper 0.04 dB
above its (1,1) where the render has it 1.40 dB below. Both were named correctly
before this step only because the bound forbade the answer, which is not the same
as getting it right.

Closing those wants the weights better than a decibel and a half, and that is a
different piece of work from this one: the residual is not in any single term
but in the near-field and radiation shares at the extremes of the microphone
geometry, where a decibel of error is enough to swap two partials that are
themselves within a decibel.

The per-fix audio previews under `Docs/audio/` are one-time review evidence and
are not re-rendered here.

### Step 6, third follow-up — a readout with no number in it, and a pair read as one capsule

Two more defects in the same comparison, both found by review of the step above.
Neither touches the audio: all 25 demonstration WAVs are byte-identical before
and after, which is the sharpest check this repository has and the one that says
a readout change stayed a readout change.

#### Defect A — the readout returned 0 Hz

`ModeObservation::weight` is `A/d · (e^{−d·t₀} − e^{−d·t₁})`, and `soundingMode`
accepted a mode only if that beat a zero-initialised best. On a small head at
the tension ceiling every mode of the drum is emptied before the pitch window
opens — at Head Diameter 15 cm, Head Tension 1.0, Head Material 0 and Pitch +12,
the okedo pad's longest-lived mode decays at nine hundred inverse seconds — so
both exponentials underflow to exactly zero, every weight on the drum is zero,
nothing is ever accepted, and the panel prints the default:

| octave | reported, before | reported, after | loaded fundamental |
| ---: | ---: | ---: | ---: |
| 0 | 2466.33 Hz | 2466.33 Hz | 2466.33 |
| 1 | 5097.77 | 5097.77 | 5097.77 |
| 2 | **0.00** | **16793.35** | 16793.35 |
| 3 | **0.00** | **25565.09** | 25565.08 |

It is not one corner. Swept over the drum controls — diameter, tension, head
material, air coupling, Pitch, Octave Body, body depth, resonant tension and
head damping — it was **23265 of 729000** combinations, and over the stroke and
microphone controls at the extremes of the drum a further **7125 of 162000**:
3.4 % of the space, all of it on small tight heads, every one of them a drum
that plainly sounds. **After: 0 of 891000.**

The fix keeps `weight` exactly as it was and adds the same quantity in nepers
beside it, computed through the exponents rather than the exponentials —
`log A − log d − d·t₀ + log(−expm1(−d·(t₁−t₀)))`, which is finite as far down as
a float exponent reaches. `soundingMode` compares on `weight` and falls back to
`logWeight` only when `weight` has run out of exponent on both sides of the
comparison, and it always takes the first valid mode, so a drum that sounds can
no longer be reported as having no pitch. Where the old comparison had an
answer it returns the same one, by construction rather than by measurement.

#### Defect B — Stereo Width was not in the ranking

`observeMode` ranked the modes with a nodal diameter through the **left
capsule**. The output stage does not put the left capsule out: it puts out
`mid ± width·(L−R)`, so at Stereo Width 0.5 the pair is handed through untouched,
at 0 the two capsules are summed, and above 0.5 their difference is exaggerated.

With the pair fully opened the two capsules straddle the nodal diameters of the
low orders, and at width 0 a mode of order three arrives at them within a per
cent of anti-phase and all but cancels in the sum. Measured, octave 1, Mic
Spread 1.00, Mic Distance 0.00, Strike Position +1.00, Stereo Width 0.00:

| | before | after |
| --- | ---: | ---: |
| reported | 210.13 Hz | **119.32 Hz** |
| rendered strongest partial | 119.89 Hz | 119.89 Hz |
| error | **+972 ¢** | **−8 ¢** |
| rendered amplitude at the reported pitch | 3.4 % (−29.3 dB) | 100 % (0.0 dB) |

`observeMode` now builds both capsule terms and combines them with the width the
output stage will use. It is still the **left channel** that is ranked — the left
*output* rather than the left capsule, which is what the function always claimed
and what it should always have computed.

Ranking on the louder of the two finished channels instead would be a better
description of a stereo instrument, and it is not done, for a measured reason:
at width 0.5 that is max(|L|,|R|) rather than |L|, which is up to 1.8 dB on the
modes of order one — enough to move the chū-daiko's Octave Body handover from
0.359 to 0.209 and with it the drum every octave above it is solved from. The
tuning path reads this function. So the combination is written as a pair of
gains, `0.5 ± width`, which are exactly 1 and 0 at the factory width, and the
association of the surviving product is left exactly as it was, so the factory
answer is bit-identical rather than nearly so. Both were checked: with the
combination written the other way the latched identity moves at Octave Body 0.21
to 0.35, and `testTheCavityIsAColumnNotAnInfiniteSpring` catches it.

#### The sweep

180 settings — Strike Position × Mic Spread × Stereo Width, and Strike Position ×
Mic Distance × Stereo Width with the pair fully opened, on all four drums —
Humanise off, each rendered six times, judged by the same majority-of-six rule
and measured on the left output channel:

| | before | after |
| --- | ---: | ---: |
| settings with a pitch | 155 of 180 | 155 of 180 |
| of those, readout wrong by more than 50 ¢ | **12** | **2** |
| worst rendered level at the reported pitch | 0.036 | 0.603 |

The two that remain are the chū-daiko's (1,3) with the pair fully open and
backed all the way off, which is the residual near-tie the section above already
records and is not a Stereo Width failure: it reads the same at width 0 and at
width 1.

On the suite's own twenty-six pinned settings, which now include six at Stereo
Width 0.00 and 1.00:

| | committed tree | after |
| --- | ---: | ---: |
| settings with a pitch | 23 of 26 | 23 of 26 |
| of those, readout wrong by more than 25 ¢ | **4** | **0** |
| worst error where there is a pitch | 1019 ¢ | **6 ¢** |
| worst rendered level at the reported pitch | 0.034 | 0.999 |

#### Hard constraints, re-verified

- **All 25 demonstration WAVs are byte-identical** to the committed ones,
  rendered to a scratch directory and compared with `cmp`. No audio moved.
- The factory grid is bit-identical: 59.659817 / 119.319626 / 238.639236 /
  477.278473 Hz at Octave Body 0 and 59.659817 / 119.319633 / 238.639252 /
  477.278503 at *Family*, stepping 1200.0000 / 1199.9999 / 1200.0000 cents, with
  the same tensions and radii to the last digit printed.
- The latched tuning identities are identical to the committed tree at **all 101
  hundredths of Octave Body**, checked directly rather than at four points.
- `testThePitchTransformIsContinuousUnderAutomation`,
  `testTheReadoutFollowsTheStrikePosition`,
  `testTheFourDrumsStepInHeardOctaves`, `testTheFourDrumsAreFourInstruments`,
  `testTheCavityIsAColumnNotAnInfiniteSpring` and
  `testTheReadoutNamesThePartialTheDrumIsHeardAt` — including octave 1 at Strike
  Position +0.50 with the pair coincident, which reads 68.05 Hz against 68.19
  measured — all pass.

#### Tests

`testTheReadoutIsAlwaysAFrequency` is new and reads the reported value directly,
because the defect is a literal zero and there is no pitch to measure it
against. It pins the reported case by name and sweeps 6336 more drums across the
drum, stroke and microphone controls. `testTheReadoutNamesThePartialTheDrumIsHeardAt`
gains six Stereo Width settings, including the reported one, and now measures
the left *output* channel rather than the left capsule — which is the same
signal only at width 0.5, and saying which one is meant is half of what defect B
was.

Reverting the two changes by hand — the width gains forced to 1 and 0, and the
`logWeight` fallback taken out of `soundingMode` — fails eleven assertions: four
pitch clauses at 973, 1019, 416 and 402 cents, two level clauses at 0.034 and
0.117, the reported zero at two octaves, and the sweep at 340 of 6336 drums.
Restoring them passes all of it.

#### What is still open

The six settings the section above lists are unchanged, and the two the sweep
here finds are two of them. Nothing in this step moves that residual: it is the
weights being good to about a decibel and a half where two partials are within a
decibel and a half of each other.

One thing this step deliberately did not do is rank on both output channels. A
partial that survives in the right channel and cancels in the left is a partial
a listener hears, and the readout will not name it — measured, that costs the
okedo at the rim with the pair fully open and the width exaggerated, where the
left channel is heard at 238.6 Hz and the right at 412.3 and the readout reports
238.6. Fixing it means giving `soundingMode` two behaviours, one for the readout
and one for the octave transform, and that split is exactly the shape of the
defect the first follow-up removed. It wants the tuning identity taken off this
function first.

The per-fix audio previews under `Docs/audio/` are one-time review evidence and
are not re-rendered here.

### Step 6, fourth follow-up — the Octave Body handover, and a pitch nothing plays

Two defects found by review of the step above. One is fixed; the other turns out
to be a genuine trade the instrument has to make, and the interesting part of
this section is the argument that says so — because two earlier passages, both
corrected above, brushed past it.

#### Defect C — the Octave Body handover is a step, and has to be

Sweeping Octave Body at octave 1, factory settings otherwise, the solved drum
steps at 0.3652:

| Octave Body | radius (m) | tension (N/m) | loaded fundamental | soundingHz | tail |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 0.3600 | 0.4182 | 23240.5 | 119.32 | 119.32 | 2.48 s |
| **0.3651** | **0.4154** | **22863.1** | **119.32** | 119.32 | 2.48 |
| **0.3652** | **0.5059** | **11510.1** | **67.33** | 119.32 | 3.48 |
| 0.3660 | 0.5056 | 11492.0 | 67.33 | 119.32 | 3.48 |

Radius +21.80 %, tension −49.66 %, loaded fundamental −43.57 %, tail 2.48 → 3.48 s,
in one ten-thousandth of the control. Swept in ten-thousandths across the whole
range at all four octaves this is the **only** crossing there is: the worst step
anywhere else is 4.30e−4 in the radius and 1.10e−3 in the tension, both at the
*Tuned* end of octave 3 where the control's own gradient is steepest, and the
heard pitch never moves by more than 4.8e−7 — a ten-thousandth of a cent —
anywhere at all.

**What was already known.** The handover is forced. *Family* needs the four
octaves tuned by (1,1), (1,1), (0,1), (0,1) or the factory grid is not octaves;
*Tuned* needs octave 1 on its (0,1) or its first octave is 157 cents wide
instead of the documented ×13.49. No single assignment serves both ends. That
argument shows the transition has to happen somewhere. It says nothing about
whether it has to be a *step*, and both passages corrected above treated the two
as the same claim. They are not.

**What was missing, and is the actual reason.** The solve puts one named mode of
a **one-parameter** family of drums exactly on the octave: Octave Body fixes the
mixture and the bisection picks a point along it. The two candidate modes are the
(0,1) and the (1,1), whose wavenumbers are the Bessel zeros 2.4048 and 3.8317, a
ratio of 1.5934 fixed by the geometry of a circular membrane. Every other term in
the model only opens that ratio further: the air load bears hardest on the lowest
mode and pushes it down, the head's bending stiffness bears hardest on the higher
one and pushes it up, and the cavity leaves the lower branch where the uncoupled
head has it. Measured on this drum at the handover it is **1.766**.

So there is no drum this model can build whose (0,1) and (1,1) are at the same
frequency, and therefore no continuous path of drums from "the (0,1) is on the
octave" to "the (1,1) is on the octave". Along any such path every partial moves
by the full 1.766 while the reported pitch is the loudest of them, so wherever
the two cross, the pitch jumps by that ratio — 975 cents — and if it crosses at
neither end, it also glides most of the way there first. **Geometry continuity
and an in-tune keyboard are incompatible at a mode handover.** One of them has
to be given up, and it cannot be the tuning.

**Measured, not argued.** The morph was implemented — both identities solved
either side of the crossing, the transform between them interpolated with a
smoothstep across a band 0.10 wide in Octave Body, resolved once at the blended
amount so the result is a real drum rather than two drums averaged field by
field. It does exactly what it was supposed to do to the geometry:

| worst 0.0001 step, octave 1 | stepped | morphed |
| --- | ---: | ---: |
| radius | 21.80 % | **0.020 %** |
| tension | 49.66 % | **0.130 %** |
| loaded fundamental | 43.57 % | **0.086 %** |
| **heard pitch** | **0.00004 %** | **76.5 %** |

and it takes the pad out of tune to pay for it. The morphed drum's loudest
partial, by the engine's own weights:

| Octave Body | loudest | margin over the next | the key asks for |
| ---: | ---: | ---: | ---: |
| 0.3150 (below the band) | 119.32 Hz | 12.2 dB | 119.32 |
| 0.3652 | **89.68** | 12.2 dB | 119.32 (−497 ¢) |
| 0.3900 | **73.82** | 0.8 dB | 119.32 (−835 ¢) |
| 0.4150 (above the band) | 119.32 | 1.4 dB | 119.32 |

Five hundred to eight hundred cents flat over a tenth of the control's travel,
with twelve decibels of margin — this is not a near-tie the readout is getting
wrong, it is the drum. The morph also moved
`testTheCavityIsAColumnNotAnInfiniteSpring`'s octave-1 corner at Octave Body
0.35, which is that clause doing its job: it is a corner inside the band, and
the drum under it had changed.

A keyboard that plays a fifth to an octave flat across a stretch of a timbre
control is a far worse instrument than one that changes timbre at a point, so
**the morph was removed and the step stays**. It is placed where the pitch is
continuous through it, which is the one place it can be inaudible in pitch: at
0.3651 the pad is heard at 119.32 with 11.1 dB of margin and at 0.3652 it is
heard at 119.32 with 0.7 dB of margin.

What is left open is the only thing that could actually remove it: rendering
both drums across the band and crossfading them in the audio. Every quantity a
listener hears would then move smoothly and both drums sound 119.32, so the
pitch would be exact throughout. It needs two modal banks per voice against a
forty-resonator budget that is currently exactly full, and it makes "the
radius" two numbers rather than one, so it is a piece of work rather than a fix.

#### Defect D — the readout named a pitch the renderer never builds

`configureResonator` refuses every mode at or above 0.98 of Nyquist and
`buildVoiceModes` drops it before it gets that far, so which partials exist in
the audio is a function of the host's clock. The comparison that picks the
reported pitch ran over the whole modal bank regardless. At Head Diameter 15 cm,
Head Tension 100 %, Head Material 0 %, Pitch +12, Octave Body 100 %, 48 kHz:

| octave | reported, before | reported, after | |
| ---: | ---: | ---: | --- |
| 0 | 2466.33 | 2466.33 | ok |
| 1 | 5097.77 | 5097.77 | ok |
| 2 | 16793.35 | 16793.35 | ok |
| **3** | **25565.09** | **no pitch** | above 0.98·Nyquist = 23520 Hz |

`soundingMode` now takes a ceiling and skips anything at or above it, using
exactly the test `buildVoiceModes` makes on exactly the same frequency. The
readout passes the renderer's cutoff; **the octave transform passes infinity**,
because which mode an instrument is tuned by is a property of the instrument and
a keyboard that retuned itself when the host changed its clock would be a worse
defect than the one being fixed. At infinity the function is bit-identical to
what it was, so the tuning path does not move: the factory grid still reads
59.659817 / 119.319633 / 238.639252 / 477.278503 Hz.

`measure()` takes the sample rate, `measureDrum()` supplies the engine's own,
and `TaikorAudioProcessor::measureDrum` supplies the host's.

**What is reported when nothing survives, and why.** Zero, meaning *no membrane
tone at this sample rate*, and the editor prints "no pitch" rather than
"0.0 Hz". Three options were weighed:

- *Report the lowest membrane mode anyway.* This is the defect. 25565.09 Hz at a
  48 kHz host is a number the plug-in will never sound; the readout would be
  describing a drum the instrument is not producing.
- *Report the highest mode that does render.* At the reported corner none does,
  so it does not answer the question at all — and where modes do render,
  "highest" is the wrong ranking: the readout names the loudest partial, and the
  highest surviving mode is usually twenty decibels down. It would corrupt the
  figure everywhere to patch one corner.
- *Say there is no membrane tone.* Chosen. It is the only one of the three that
  is true. The drum is not silent there — the head's continuum and the shell
  still sound — but it has no partial to be tuned to, and that is worth saying
  rather than hiding behind a number.

This does collide with the previous round's rule that the readout is always a
positive frequency, and the collision is real rather than a bound that needed
loosening. It is settled by measurement: swept over the drum controls at four
sample rates, **every** drum that reports no pitch has an empty membrane bank,
and **no** drum with a bank reports no pitch.

| sample rate | drums swept | reporting no pitch | of those, with a non-empty bank |
| ---: | ---: | ---: | ---: |
| 44100 | 4608 | 68 | **0** |
| 48000 | 4608 | 44 | **0** |
| 96000 | 4608 | 4 | **0** |
| 192000 | 4608 | 0 | **0** |

The count falling to zero at 192 kHz is the shape the physics predicts: it is a
statement about the host's clock and not about the drum. The reported corner
reads 25565.09 Hz at 96 and 192 kHz, where the renderer does build it.

#### Tests

`testOctaveBodyHandsOverWithoutMovingThePitch` sweeps Octave Body in
ten-thousandths across the whole range at every octave and asserts three things:
the heard pitch never moves (bound 1e−5 relative, against 4.8e−7 measured), the
solved geometry never moves by more than 2e−3 in one step **except** at a
handover (against 4.30e−4 / 1.10e−3 measured away from one), and there is
**exactly one** handover, at octave 1, at Octave Body 0.3652, with its size
recorded. It also pins *Tuned*'s ×13.49 / ×4.05 / ×4.00 ladder and unmoving
radius and *Family*'s four diameters, which are the two things the interior of
the control is not allowed to buy smoothness with. The morph fails its pitch
clause by 76.5 % against a bound of 0.001 %.

`testTheReadoutNamesAPartialTheRendererBuilds` checks, at 44.1 / 48 / 96 and
192 kHz, that every reported pitch is below the renderer's cutoff and matches a
mode the engine actually built — read out of a triggered voice rather than from
the arithmetic that produced the number, so a readout that agrees only with
itself fails. It allows a quarter of a per cent for the degenerate split, whose
lower member is always the one below the frequency the comparison ranks.
Reverting the bound by hand fails the suite **27 times**; restoring it passes.

`testTheReadoutIsAlwaysAFrequency` is **restated, not relaxed**. It asserted a
positive frequency everywhere at one implicit sample rate. It now asserts an
equivalence checked against the built bank: the readout is positive exactly
where the renderer builds at least one membrane mode and zero exactly where it
builds none, over the same 6336 drums. The original zero-hertz defect — a drum
that sounds reported as having no pitch — fails it exactly as it did before, and
98 of the 6336 are drums with no membrane mode at 48 kHz, every one of them a
15 cm head carried up the keyboard or transposed two octaves sharp.

#### Hard constraints, re-verified

- **All 25 demonstration WAVs are byte-identical** to the committed ones,
  rendered to a scratch directory and compared with `cmp`.
- The factory grid is bit-identical: **59.659817 / 119.319633 / 238.639252 /
  477.278503 Hz** at *Family*, stepping 1200.0000 / 1199.9999 / 1200.0000 cents,
  and 59.659817 / 119.319626 / 238.639236 / 477.278473 at *Tuned*. Checked at
  Octave Body 0, 0.5, 0.7 and 1.0.
- *Tuned*'s tension ladder is ×13.4879 / ×4.0517 / ×4.0000 and its radius is
  0.750000000 m at every octave; *Family*'s diameters are 150.0 / 78.0 / 40.0 /
  30.0 cm.
- `testThePitchTransformIsContinuousUnderAutomation`,
  `testTheReadoutFollowsTheStrikePosition`, `testTheReadoutIsAlwaysAFrequency`,
  `testTheFourDrumsStepInHeardOctaves`, `testTheFourDrumsAreFourInstruments`,
  `testTheCavityIsAColumnNotAnInfiniteSpring` and the rest of the suite all
  pass. Nothing in `testTheCavityIsAColumnNotAnInfiniteSpring` needed re-taking
  once the morph was removed.
- `Tests/PluginProcessorTests.cpp` needed no change and was checked by
  inspection: its three measurement clauses read `loadedFundamentalHz`, which no
  ceiling touches, and `soundingHz` on the default ō-daiko, which reads
  59.659817 Hz at every supported rate. It compiles only on macOS CI.

The per-fix audio previews under `Docs/audio/` are one-time review evidence and
are not re-rendered here.

## Interaction and continuum correction — 2026-08-10

This tranche began by correcting three approximations found while auditing the
next physical-model architecture, and now also contains the first persistent
shared-state migration. The four keyboard drums own canonical modal and
statistical-tail state; strikes are scheduled contacts that project force into
those banks. It is not yet the finished architecture: contact is still a
prescribed force pulse, and the temporary storage type still contains fields
that belong on only one side of the physical/contact split.

### What landed

**Collision state continuity.** A strike on an already-ringing head now applies
restitution to modal velocity while leaving modal displacement continuous. The
former implementation multiplied both resonator histories and therefore
removed potential energy instantaneously. State recovery uses live pole
coordinates cached whenever the resonator is built or retuned, rather than a
mode's unshifted build frequency, so it remains correct after Tension Mod, pitch
automation or the wheel has moved a ringing mode without decomposing every pole
again at every hit.

**A muted stroke leaves a physical local contact.** Tsu now leaves its free
hand on motion already ringing on that drum for 180 ms. A symmetric five-point
quadrature covers a 55 mm-radius palm patch. Its projection is divided by each
mode's physical norm, so the velocity-loss rate is

`d_i = (c_A / sigma) mute^2 clamp(A_p <phi_i^2>_p / N_i, 0, 1)`,

with `c_A = 5000 kg m^-2 s^-1`; each control interval retains velocity by
`exp(-d_i dt)` without stepping displacement. The statistical continuum uses
the same physical patch/head area law and half the velocity exponent because
its envelope is phase-averaged RMS amplitude. The contact reaches older voices
belonging to the same drum and no other octave.

**The unresolved head owns five real octaves.** Each continuum band is now a
serial two-pole high-pass and seven-pole low-pass rather than a difference of
low-passes. Its exact discrete white-noise variance is solved once at trigger
time from `P = A P A^T + B B^T` by squared Smith iteration, so the requested
RMS does not inherit filter geometry or host sample rate. The level law includes
the approximately constant modes-per-hertz density of a two-dimensional
membrane: unresolved amplitudes add in quadrature as `sqrt(f)` before contact
bandwidth and hide loss shape the audible slope.

**One ringing object per physical drum.** The renderer now advances at most one
46-mode recurrence and one residual field per playable drum per sample.
Simultaneous contacts accumulate into stable physical-mode IDs before that one
tick; no contact retains resonator state after trigger setup. Structural
automation rebuilds a canonical bank by stable ID while preserving modal
displacement and velocity, residual energy, age, deadline, local constraints
and RNG state. A new hit extends the shared tail from the time it lands rather
than resetting or stealing it.

### Regression evidence

- `testCollisionChangesVelocityNotDisplacement` checks positive, zero and
  negative retention at three live-pole shifts. Displacement agrees to `1e-14`,
  velocity is multiplied by the requested retention to relative `1e-10`, and a
  modal turning point cannot acquire velocity.
- `testMutedStrokeChokesTheRingingHead` compares the same Tsu contact with only
  its subsequent palm hold enabled or cancelled. Another drum remains unchanged
  within `1e-6`; modal gains must be nonuniform; and the held Don tail must fall
  below 65 percent of its collision-only control. The recovered fundamental
  damping rate for 0.75 m versus 1.50 m heads must scale within the physically
  expected inverse-area interval 2.5–6.5, and the recovered rate at 44.1 and
  96 kHz must agree within 0.1 percent. The same regression proves that Ka's
  small voicing loss does not schedule a palm, that a centre-crossing patch is
  radially symmetric, and that one real mute-control tick leaves displacement
  exact while multiplying velocity and kinetic energy by `g` and `g^2`; shell
  state stays bit-identical.
- `testTheContinuumDoesNotDependOnTheSampleRate` isolates the upper statistical
  band at 44.1, 48, 96 and 192 kHz. The complete 4–10 kHz response must stay
  within 1.5 dB, the isolated band within 2 dB, the resolved low bank within
  0.5 dB, and the whole response above 400 Hz within 1.5 dB.
- `testContinuumBandsOwnTheirOctaves` requires the first band's leakage two
  octaves above it to be at least 24 dB down and every higher band to beat all
  lower-band leakage in its own octave by at least 0.5 dB.
- `testContinuumVarianceCacheLifecycle` requires a coefficient-cache hit after
  reset to reproduce its miss bit for bit. A reused engine must also match a
  fresh one exactly after sample-rate, diameter and pitch changes, in both its
  continuum band data and 4096 rendered stereo samples.

### Validation and cost

- The interaction/continuum snapshot immediately before the shared-state
  migration passed the native and complete JUCE suites. Those figures are
  historical evidence for that snapshot, not a green claim for the worktree
  after the topology change.
- The current complete JUCE Release build succeeds for Standalone, VST3 and AU;
  `Taikor.RenderDemos` and `Taikor.PluginProcessor` pass. The focused engine
  suite currently has 14 assertions still being reconciled: a narrow modal
  handoff in the pitch estimator, finite-window sample-rate statistics, two
  articulation-tail expectations inherited from per-strike poles, and the
  collision roll-energy comparison. This is therefore a checkpoint of the
  migration, not a release-qualified final state.
- Immediately before the shared-state migration, a 120-second 48 kHz/256
  dense-roll stress at 18.75 hits/s sustained all 16 voices at 10.400 percent
  of one CPU core, or 9.62 times real time. That was an 11.05 percent relative
  increase over the preserved clean engine, or 1.035 percentage points of one
  core. The final cache only removes trigger work, so this is a conservative
  measurement for the interaction/continuum snapshot, not for the current
  shared renderer. A new paired benchmark is required once its regressions are
  closed.
- The deliberately harsher 16-simultaneous-note case shows that final trigger
  optimization directly. At 48 kHz/64, lazy exact-variance caching reduces
  median trigger cost from 629.467 to 331.892 microseconds and complete callback
  work from 59.48 to 37.15 percent of the 1.333 ms budget. At 48 kHz/32 the
  cached engine uses 60.79 percent of the 0.667 ms budget and records no
  CPU-time deadline miss in 10,000 trials (worst 542 microseconds). At 64
  samples, four of 10,000 aggregate trials cross the deadline in one clustered
  run (worst 1.445 ms), while the other four runs' 99.9th percentiles remain at
  or below 620 microseconds; those tails are retained here rather than hidden
  behind the median.
- Rendering all 25 demonstrations produces 147.5 seconds of audio at 56.30
  times real time, averaging 1.776 percent of one core. The relative CPU change
  is 9.17 percent. A 1 ms profile puts 81.65 percent of samples in
  `renderVoice`; cached live pole coordinates remove `acos`, `log` and scalar
  `sin` from the collision hot path.

### Boundary of the claim

The engine now owns one rendered modal bank and one continuum per drum, and a
new Tsu constraint acts on that same bank before its own energy enters. During
the migration, however, contacts and physical drums still reuse the old `Voice`
storage type; contact slots clear their temporary mode states but continue to
carry unused arrays. The palm projection is a mode-dependent diagonal passive
operator, not yet the exact low-rank cross-modal patch. The two heads and
finite-column cavity remain folded into coupled eigenmodes rather than explicit
reciprocal runtime states. The residual field is shared per drum, but is still
statistically shaped audio rather than a calibrated physical modal-energy
reservoir. Most importantly, bachi contact is still a scheduled `sin^1.5` force
pulse plus instantaneous restitution, not the active passive nonlinear solve
specified below.

### Next architecture tranche, ranked

1. **Finish the persistent-state split.** Replace the temporary shared `Voice`
   storage with explicit physical-drum and lightweight contact types, remove
   unused per-contact mode/filter arrays, and give the bank explicit batter,
   rear-head, cavity and residual-energy ownership. Rolls, flams, muting and
   retuning already feed one canonical recurrence; the remaining work is to
   make that ownership complete and energy-auditable. Repeated excitation must
   remain energy-stable; see
   [Risse, Hélie and Bilbao (2025)](https://www.dafx.de/paper-archive/2025/DAFx25_paper_24.pdf).

2. **Active nonlinear bachi contact.** Replace the scheduled force pulse with a
   dynamic stick mass and finite footprint using the passive Hunt–Crossley law
   `F = K[delta]+^alpha (1 + beta deltaDot)`. Solve its one scalar implicit
   contact equation per active strike, oversampling only this local block.
   Contact duration, rebound, brightness and energy exchange then follow
   velocity and the head's current velocity. Primary method:
   [Bilbao, Torin and Chatziioannou (2014)](https://arxiv.org/abs/1405.2589).

3. **Promote the second head and cavity to dynamic states.** Keep the current
   finite-column solve for calibration, but render batter and carry-head modes
   plus a rank-one volume-compression coupling and several higher cavity poles.
   Validate the coupled doublet and its rear-tension dependence against
   [Suzuki and Hwang (2008)](https://www.jstage.jst.go.jp/article/ast/29/3/29_3_215/_pdf/-char/ja).

4. **Exact local loss and a shared statistical tail.** Project a patch into the
   modal basis as `f_d = -c g(g^T qDot)`: one dot product and one vector update
   per patch, symmetric positive-semidefinite and `O(M)`. This restores
   cross-modal damping omitted by the present diagonal gains; see
   [Zheng and James (2011)](https://www.cs.cornell.edu/projects/Sound/mc/ModalContactSound2011.pdf).
   Keep repeatable modes deterministic and cross to a shared band-energy field
   only where measured modal bandwidth exceeds spacing. Hybrid deterministic /
   statistical vibroacoustics is supported by
   [Zhu et al. (2022)](https://doi.org/10.1016/j.jsv.2022.117221), with
   modal-plus-residual synthesis demonstrated by
   [Ren, Yeh and Lin (2012)](https://gamma-web.iacs.umd.edu/AUDIO_MATERIAL/examplebasedsoundsynthesis.pdf).

## Passive contact and controller palm — 2026-08-11

This section supersedes the preceding checkpoint's statements that bachi
contact was still a prescribed pulse and that the engine suite was red. The
older measurements remain above as history of the shared-state migration.

### What landed

**The bachi is now a dynamic contact.** Each stroke owns a stick mass and two
position histories. Compression against the one already-moving physical head
drives a Hertzian `delta^1.5` potential with constrained Hunt–Crossley loss. A
matched-pole IMP-2 discrete gradient advances stick and membrane together; the
same dimensionless contact vector senses `p^T q` and spreads `p F`, and all
same-drum contacts in a sample are solved in one symmetric system. The force is
non-adhesive, release follows geometric separation, and the free modal poles are
exactly the poles the renderer already uses.

**CC1 is now the physical palm operator, not a volume envelope.** The controller
uses the same 55 mm finite-area, five-point projection as Tsu at a fixed central
hand position. Full-pressure loss is cached per mode when the drum is built and
scaled by squared controller pressure at each control tick. The split update
preserves displacement and physical velocity while moving the loss into a pole
that acts on every audio sample; the shell and airborne click are untouched.
The unresolved RMS envelope receives the corresponding phase-averaged half
exponent.

**Diagnostic renders isolate their stated axes.** Parameter sweeps, Octave Body
and the velocity/hand demonstrations force Humanise to zero. Performance takes
retain variation. A sweep can therefore be used as evidence without hidden
changes in strike radius, angle, speed or contact duration.

### Proof and cost gates

- `testPassiveNonlinearContact` checks finite, non-negative force, release and
  exact half-step energy monotonicity from 8 to 384 kHz. Ordinary-rate Don
  duration, impulse and peak converge within 6, 1 and 4 percent respectively.
  `testStrokesShareOnePhysicalDrumState` owns the same-sample recurrence/order
  check; `testSimultaneousStrokesDoNotShareOneVoice` owns pool-full allocation
  and finite-output coverage.
- `testHandControllerIsAPhysicalPalm` checks one real control tick directly:
  membrane displacement and physical velocity are unchanged as the predicted
  continuous pole loss is applied, the shell is bit-identical, modal rates are
  nonuniform, the continuum follows the same area law, high modes cannot strobe
  past the palm, and recovered rates agree from 44.1 to 96 kHz.
- On the Apple M1 Max acceptance run, sparse CC1-off audio remained bit-exact
  and used 0.726 percent of one core. Dense mixed articulation used 1.93 percent
  (51.8x real time), while full-pressure CC1 on all four drums used 1.50 percent
  (66.6x). Its callback p50/p99/p99.9 was 18.2 / 46.0 / 111 microseconds at
  48 kHz/64 samples, with zero deadline misses in 67,500 callbacks and a
  431-microsecond worst case against the 1.333 ms budget.

### Boundary of the claim

The five-band continuum is still an observation-only statistical tail. Its
force-squared energy estimate is formed after the contact solve, so omitted
high modes do not mechanically load the bachi. On the factory ō-daiko the forty
resolved membrane resonators end near 226 Hz and the first statistical band is
centred near 283 Hz, while an independent Weyl/modal-bandwidth estimate does not
reach overlap until roughly 412 Hz and about 128 resonators. The other estimated
overlap counts are about 114 / 70 / 52 for chū-daiko, okedo and shime.

Neither obvious shortcut is release-ready. Extending every bank to 118 modes
roughly doubled dense CPU and changed the reciprocal contact itself; a ten-state
positive-real Foster prototype was passive and sample-rate stable but, without
measured mobility calibration, captured Ka and rim contacts for several
milliseconds. The correct conclusion is not to pick the cheaper wrong answer.

### Next work, in order

1. Record controlled force/velocity driving-point mobility, repeated strikes
   and two microphone distances on representative taiko, following the
   [capture contract](calibration/README.md). The current repository has no
   real-taiko calibration set, so external single-mic recordings are leads
   rather than acceptance references.
2. Give the bachi a measured finite footprint and fit a positive-real omitted-
   mode mobility jointly with deterministic modes carried to measured overlap.
   The fit must preserve contact duration/rebound across sample rates before its
   energy may drive the stochastic observer.
3. Fit complex near/far radiation for the continuum. The coherent-patch law
   below now prevents the old far-field brightening, but it remains a scalar
   magnitude approximation with no measured phase, directivity or capsule.
4. Promote higher cavity modes and the rear head to reciprocal runtime states,
   then split the temporary `Voice` storage and add energy-based mode sleep.

## Two-state Drum Layout — 2026-08-11

This note supersedes the historical sections above that describe **Octave Body**
as a continuous morph. The parameter keeps its established `octaveBody` ID,
version, order, 0/1 endpoints and default so existing projects and automation
remain attached. Its product name is now **Drum Layout**, with two meanings:
**1 Drum** retunes one physical design across the keyboard, and **4 Drums** uses
the four independent family instruments. Legacy values below 0.5 map to 1 Drum;
values at or above 0.5 map to 4 Drums.

The DSP performs the same threshold in `TaikoEngine::sanitise`, before cache
invalidation. Direct engine callers therefore cannot recover an undocumented
intermediate drum, and redundant legacy automation within either half does not
rebuild the four solved instruments. The two endpoint sounds and their tuning
contract are unchanged; only the physically unhelpful interior and its forced
modal-identity handover are gone.

`testDrumLayoutHasOnlyPhysicalEndpoints` checks endpoint equivalence for legacy
and out-of-range values at every octave, the exact 0.5 boundary, cache-revision
behaviour, the 1 Drum tension ladder and the 4 Drums family diameters. The demo
asset keeps its stable `20-octave-body.wav` filename, but already renders only
the two endpoint layouts with Humanise disabled.

## Per-mode finite-column cavity resolve — 2026-08-16

The four axisymmetric membrane pairs no longer reuse the `(0,1)` cavity factor.
Each pair now solves its own fixed point of `x cot x` against its own
volume-changing branch. The renderer, analytic observation and tail readout all
consume the matching cached stiffness. Entry zero deliberately keeps its old
arithmetic and remains the public readout and octave anchor.

On the factory ō-daiko the factors are **0.822614 / 0.609730 / 0 / 0**. The
upper branches are **61.3723 / 89.1345 / 144.442 / 202.078 Hz**; reusing entry
zero would put the last three at **89.7667 / 144.844 / 202.225 Hz**. The change
is therefore **−12.2 / −4.8 / −1.3 cents**, with no movement of the tuned pair,
its 32.6503 Hz lower branch or its 61.3723 Hz breathing branch.

The implementation does not pay those three extra solves inside every step of
the octave transform. A trial resolves only the cavity entries its latched
tuning mode can observe, then completes entries one to three once on the exact
winning endpoint. A future higher-axisymmetric tuning identity is explicitly
guarded and keeps the full trial solve. In paired 10,000-trigger Release runs at
48 kHz/64 samples, the factory median moved from about **274 to 290 µs** and a
shallow 1.8 m slack-head stress from about **274 to 303 µs**. The naïve inner-
loop implementation measured about 515 µs on that stress. No sample-loop work
was added.

The focused regression checks all four fixed points, the deferred octave path,
analytic/rendered frequency agreement, the unchanged `(0,1)` path and the
downward movement from the old shared solve. The complete JUCE-free Release
suite passes **2/2 in about 53 s**. A fresh 25-take render completed with a
worst peak of **−3.5 dBFS**;
the committed review WAVs were not rewritten in this worktree, so comparisons
use same-machine fresh renders. The canonical Linux nightly refreshes those
tracked assets after a change lands on `main`.

### Boundary and next gate

This is the last honest improvement available from the scalar positive-
stiffness cavity approximation. At and above the half-column quarter-wave,
zero is a continuous truncation, not the mass-like acoustic impedance or its
poles; higher radial head modes also excite spatial cavity fields rather than a
uniform one-dimensional pressure. The next cavity step is an explicit passive
rear-head/cavity state validated against controlled pressure and head-velocity
captures, following [Suzuki and Hwang
(2008)](https://www.jstage.jst.go.jp/article/ast/29/3/29_3_215/_pdf/-char/ja).

The broader audit found microphone observation, not another voicing constant,
to be the largest remaining perceptual gap. Resolved modes still reach each
capsule through real scalar gains with no propagation phase or complete modal
radiation pattern. The continuum's flat distance law is corrected in the next
section; exact coherent radiation remains release-gated on the repository's
owned force/head-motion/near/far capture contract. Without those measurements a
compatibility gain cannot distinguish batter radiation, rear-head radiation,
finite shell, capsule and room effects.

## Wavelength-dependent continuum perspective — 2026-08-16

The unresolved head no longer borrows one resolved mode's microphone gain for
all five of its octaves. `buildVoiceModes` first measures the same modal-
receptance anchor at the factory capsule distance, **15.95 cm**, then gives each
continuum band a coherent patch radius

```text
R(f) = c_head / (2 f)
```

and relative pressure gain

```text
G(f,d) = sqrt((R(f)^2 + d_ref^2) / (R(f)^2 + d^2)).
```

This is exactly one at the factory position, so the calibrated preset and every
articulation's continuum balance retain their former level there. Moving the
pair changes perspective rather than applying another voicing scalar. Long
membrane wavelengths retain near-field reach; short, locally coherent patches
approach the bounded `1/d` pressure law. On the factory ō-daiko, moving from 3
to 40 cm now drops the five band targets by approximately **9.06 / 14.18 /
18.55 / 21.12 / 22.11 dB**, instead of giving them one flat attenuation. The
upper limit is the physical point-source ratio, **22.50 dB**.

The direct regression reads every band's calibrated `targetRms`, checks the
closed equation above, requires the attenuation to rise strictly with centre
frequency and requires more than 8 dB of additional near-to-far loss from the
lowest to the highest octave. The isolated middle-band audio guard moves from
the former 8–15 dB interval to 17–20 dB. Sample-rate, cache-lifecycle and
per-octave ownership tests remain in place. Structural automation preserves the
continuum's filter memory and physical decay but rescales every already-ringing
envelope by its new-to-old distance gain, so moving a live pair affects the tail
already in the air. A contact that was triggered before the move converts each
later continuum injection from its saved perspective to the live gain; a focused
regression matches all five envelopes against a strike triggered directly at the
new distance. The complete DSP suite passes in about 56 s, and all 25
demonstrations render; the worst peak remains **−3.5 dBFS**.

### Phase-aware resolved observer: implemented, not released

The released scalar observer now respects the exact azimuthal covariance it can
represent: a non-axisymmetric circular mode's `cos(m theta)` or `sin(m theta)`
factor multiplies both the local and propagating terms. The former placement
added the propagating term afterwards, giving every multipole a false
omnidirectional floor and nonzero output on a nodal azimuth. All 32 released
multipole resonators and the matching analytic readout are now checked at a
node and a lobe. The statistical continuum keeps its former reference
arithmetic exactly; it is not one coherent multipole.

All 25 demonstrations render after the correction. Against the immediately
preceding validated render, the largest whole-file RMS move is **0.05 dB** and
the worst output peak remains **-3.5 dBFS**: the change removes a directionally
impossible residue without using a hidden level re-voice to make it pass.

The minimum coherent observation architecture is also exercised behind an
explicit false release gate. For non-axisymmetric resolved modes it evaluates
the baffled Rayleigh integral over the moving disk, stores a complex residue per
capsule and reconstructs the damped oscillator's exact quadrature from its two
states. The released path remains a real scalar radial approximation when the
gate is off. Axisymmetric observation is deliberately untouched: its present scalar
contains batter- and rear-head participation, and replacing that with a front
baffled disk would silently discard the only rear contribution before the rear
head and cavity become explicit states.

The integration rule uses a 24-point Gauss–Legendre rule after a squared-
distance mapping for `ka <= 16`; above that it uses one adaptive high-order rule
in propagation distance. A 256-point reference, the on-axis piston solution,
non-axisymmetric on-axis nulls, the `ka = 16` transition and a synthetic complex
delay exercise the numerical and phase conventions. The former eight-point
prototype was rejected after a shipping shime mode measured about 20.6 dB and
177 degrees wrong.

A phase-factored `H(ka)` table made runtime interpolation cheap but did not make
activation real-time safe. Building it measured about **1.72 ms** on the factory
drum and roughly **210 ms** at hostile supported geometry, and structural
changes can request that work from the audio thread. A built complex transfer
also goes stale while live controls can move an already-ringing bank by as much
as roughly ±52 semitones. The cache therefore does not remove the required
tuning/readout migration, rear-head separation or owned near/far calibration,
and the release gate remains false.

Activation is intentionally blocked by three facts, not by missing plumbing:

1. changing the observation changes which partial the current octave solve is
   heard at, so it needs an explicit tuning migration rather than a hidden side
   effect;
2. Pitch automation can move a ringing bank by two octaves while a built
   radiation residue is frequency-dependent;
3. no owned near/far pressure plus head-motion capture yet separates absolute
   batter radiation from the rear head, shell, capsule and room.

The next release gate is therefore the controlled capture contract, followed by
a fitted positive global pressure calibration, live-frequency residue update,
readout migration and trigger-cost sweep. Until then this architecture changes
no production samples.

## Local-palm and shell prototype audit — 2026-08-16

An orientation-resolved diagonal palm prototype was rejected rather than
released. It reused the existing centre-plus-cardinals rule as angular
quadrature and assigned separate losses to the cosine and sine members. That
rule was designed only for the phase-averaged radial projection: four angular
directions alias `cos²(m theta)` badly at the resolved orders. On an ō-daiko
`m=8` pair it predicted a **216:1** loss contrast where dense disk integration
gave **1.14:1**; on a shime `m=3` pair it predicted roughly **14,000:1** where
the reference was essentially unity. At a centred circular palm it could even
leave one even-order orientation undamped, which is impossible.

Keeping only separate diagonal losses would also omit
`integral(phi_cos phi_s)`: at a general palm angle the correct 2-by-2 damping
block rotates with the patch and has an off-diagonal term. The shortcut instead
changes its eigenvalues when the coordinate basis rotates. Its large audible
tail and stereo changes therefore could not be claimed as realism gains. The
released phase-averaged diagonal palm remains in place.

The mathematically defensible local-palm form remains the positive-semidefinite
operator

```text
D = c_A A_p sum_s w_s p_s p_s^T,
```

A 4-by-19 disk rule verified that operator, but the exact runtime was rejected
on callback cost: it added about **19.5% of one core at 48 kHz/64 samples** and
**39.3–41.2% at 96 kHz/32 samples**. With contact active, p99.9 reached
**462.6 microseconds**, beyond the 96 kHz/32 callback's **333.3 microseconds**
budget. Compression still required ranks about **21–27** for `1e-4` action
accuracy, while the Woodbury path remained roughly **9.6% / 19.2%** incremental
at 48/96 kHz. The released phase-averaged diagonal palm therefore remains in
place. A coupled local palm is performance- and capture-gated, including an
independently authored free-hand position for Tsu.

A simultaneous shell prototype duplicated each of the six ring modes into a
pure cosine/sine radiation doublet. It too was rejected: removing the legacy
common pickup inverted several wide-pair Ka/rim cases, raised the worst rim
peak to within 0.03 dB of full scale and broke the required near-to-far image
narrowing. A calibrated shell migration remains worthwhile, but it needs owned
body/pressure captures and an explicit mono/side revoice.

## Physical-coordinate automation remap — 2026-08-16

Structural controls can rotate an axisymmetric pair's normal-mode basis even
when no new strike occurs. Copying state by the old lower/upper branch label was
therefore not state continuity: on the factory drum, moving Air Coupling from
zero to 0.85 rotates the first pair by about **43.27 degrees** and the second by
about **19.17 degrees**.

Each axisymmetric resonator now retains both physical participation coefficients,
`v_B / sqrt(sigma_B)` and `v_R / sqrt(sigma_R)`. Before a rebuild, the two branch
states are summed into batter/rear displacement, velocity and pending force
increment. The resulting physical coordinates are inverse-projected into the
new eigenbasis. This also handles head-density changes; a hostile one-branch
Nyquist case uses the corresponding mass-orthogonal projection instead of
copying an unrelated branch. A live Tsu rate is preserved as a pair total and
redistributed by the new batter-energy fractions.

A bachi can straddle a structural-control update, so state remapping alone is
not enough. Every still-active contact receives freshly built sensing and force
projections from the same new basis, keeping that interaction reciprocal. The
focused regression crosses both cavity coupling and a simultaneous head-
material/rear-tension change, seeds independent displacement, velocity and
pending increments in all four pairs, and holds every physical invariant below
`2e-5`; refreshed sensing and force projections agree below `2e-6`. A separate
8 kHz hostile case begins with only a rear-head branch in band, brings its batter
partner back during the held Tsu, and requires the cached physical patch loss to
be restored on both rebuilt branches.

Only the five demonstrations that automate structural controls changed in the
final render; the other 20 remain byte-identical to the preceding validated
build. All 25 stay below full scale, with a worst peak of **−3.5 dBFS**. This is
a lifecycle/state-continuity correction, not a new acoustic calibration.

## Strike Position without dead travel — 2026-08-16

This supersedes the historical gap notes above which say Strike Position still
has dead travel. The released mapping added one fixed `0.32 p` offset to every
articulation and clamped the result. Because the written radii range from 0.15
to 0.97, that made **18.8–47.7%** of the control produce a bit-identical contact
point. The demonstration even placed two Don examples inside the same flat.

The control now retains every established centre, default and rim endpoint but
uses all of the travel between them. For written radius `rho0` and bipolar
position `p`, its two available excursions are

```text
d- = min(0.32, rho0)
d+ = min(0.32, 0.985 - rho0)
rho(p) = rho0 + p * (p < 0 ? d- : d+).
```

| Stroke | `p = -1` | `p = 0` | `p = +1` |
| --- | ---: | ---: | ---: |
| Don | 0.000 | 0.150 | 0.470 |
| Ka | 0.590 | 0.910 | 0.985 |
| Tsu | 0.000 | 0.200 | 0.520 |
| Don Rim | 0.650 | 0.970 | 0.985 |

One shared helper feeds trigger geometry, the pitch readout and the contact-time
measurement. Humanise still adds its physical scatter afterwards. The half of
each articulation which already had the full 0.32 radius available keeps the
old multiply/add sequence exactly. The whole shorter half is deliberately
rescaled, including values which previously moved before reaching the clamp;
existing nonzero automation on that side therefore lands at a different radius.
Zero and both endpoints remain bit-compatible, so the migration removes the
flat without expanding any articulation's established physical range.

The focused regression sweeps 201 controller values for every stroke and
requires strictly increasing radius, pins all twelve endpoint/default values,
and proves the already-active half-ranges remain bit-identical. Four audio pairs
which were formerly sample-identical—one for each stroke—must now differ. This
is a performance-realism repair, not a timbre calibration: no drum, contact or
microphone coefficient changed. The complete Release suite passes **2/2 in
57.5 s**. A fresh 25-take render keeps the previous worst peak of **−3.5 dBFS**;
24 files are byte-identical to the preceding validated build and only
`13-strike-position.wav` changes.

## Explicit polar strike placement — 2026-08-16

Strike radius was already a physical input, and the engine already carried a
real strike azimuth through cosine/sine membrane drive, reciprocal contact and
the airborne two-capsule path. What was missing was authored control. Strike
Azimuth is now a ±180° host parameter appended after all 22 established slots,
so every earlier ID and automation index is unchanged and a legacy state gets
the 0° default.

CC16 supplies an absolute azimuth and CC17 supplies the existing bipolar radial
coordinate. Their standard 7-bit centre, 64, maps exactly to zero / As written;
events are applied sample-accurately before or after a same-sample note according
to MIDI insertion order. CC121, panic and state restore clear both live
overrides back to the host controls. The exact numerical pitch readout consumes
the same live coordinates. The lightweight trigger estimate ranks both members
of an intentionally detuned cosine/sine pair independently at nonzero angles;
a 2-octave × 4-position × 73-angle audit now stays within **2.55 cents** of the
exact dry-contact result, where collapsing the pair formerly missed by as much
as **628 cents**.

No automatic hand alternation was added. MIDI notes carry no hand identity, and
inferred alternation would rotate a genuine one-hand roll. A sequencer instead
sends the coordinates before the note they belong to. This adds no fitted
acoustic constant and no sample-loop work. At the 0° host default with no live
override, the former angle-jitter expression stays exact: the existing 25
demonstrations are byte-identical to the preceding spatial baseline. The new
26th renderer take holds Humanise at zero, plays fixed left/centre/right points
and walks one Don around eight azimuths; its raw peak is **−16.7 dBFS**.

The contract is pinned at all layers: DSP tests cover deterministic authored
points, audible rotation, exact-zero compatibility, split-branch pitch ranking,
override priority and reset; processor tests pin the 23 slots present in this
tranche (expanded to 24 by Performer below), legacy state restore, CC16/17
endpoint/centre mapping, same-sample order, live readout and state-reset
lifecycle; the renderer keeps takes 1–25 byte-identical while adding only take
26. Sonica's verified 19-position diameter and independent-hand
controls are therefore table stakes Taikor now exceeds continuously rather than
an unimplemented performance gap.

## Contact-spectrum and direct-attack cleanup — 2026-08-16

The hit contained two independent routes that could turn physical contact into
an artificial noise layer.

First, the resolved bank and the statistical head did not use the same contact
spectrum. The resolved modes are driven by the solved force, and the analytic
readout already carries a smooth fit to the Fourier magnitude of the
`sin(pi t/tau)^1.5` Hertz pulse. The five continuum octaves instead used
`1 / (1 + (f tau)^2)`, which falls too slowly once a band is above the contact
corner. At the default hardness and velocity it left the top octave between
**3.2 and 17.3 dB too hot** across the four drums and four articulations. The
continuum now calls the shared `contactSpectrum(2 pi f tau)` helper. No level,
tilt or brightness constant was added.

That helper is an accurate point-frequency fit through `omega tau` about six
and deliberately stays monotone through the exact pulse transform's later
nulls. For a statistical octave this is a smooth population envelope, not a
claim of exact single-frequency Hertz magnitude above the fitted range; owned
force/head-motion captures remain the gate for replacing it with a calibrated
band average.

The correction retains the established serial two-pole high-pass and seven-pole
low-pass partition, with exact nine-state white-noise variance normalisation and
a **42 dB/octave** upper skirt. A counterfactual build confirmed that adding an
eighth pole was unnecessary and moved each response peak farther from its named
centre, so it was removed rather than retained as speculative filtering. The
hard-bachi ownership regression still requires every octave to beat all lower
band leakage in its own passband; soft/long-contact suppression is owned by a
separate closed-form spectrum test rather than hidden by weakening that gate.

Second, the direct-air path differentiated `force + roughness`. Differentiation
rises at 6 dB/octave while its single contact-patch pole falls at 6 dB/octave,
so the stochastic stick/hide term became a flat shelf to Nyquist. At the factory
Stick Noise setting that shelf alone sat only **1.64 / 2.05 / 0.60 dB** below
the complete Don / Ka / Tsu hit above 8 kHz. The direct path now differentiates
only the solved normal force; roughness still excites the resonant object and
the tack remains its own thresholded source. Isolated old/new probes reduce the
same three attacks above 8 kHz by **7.71 / 3.78 / 8.25 dB** without lowering the
body or the Stick Noise control globally.

`testContinuumUsesTheHertzContactSpectrum` compares soft and hard contacts after
factoring out their force and requires every band to follow the shared pulse
transform. `testContactRoughnessDoesNotBecomeAnAirborneNoiseShelf` requires the
isolated direct pressure waveform to be bit-identical from Stick Noise 0 to 1,
while the isolated struck object must still change. The complete JUCE-free
Release suite passes **2/2 in 56.78 s**. A fresh 26-take render completes with a
worst pre-normalisation peak of **−3.1 dBFS**; the canonical Linux nightly, not
this macOS worktree, owns the tracked WAV refresh.

### Remaining identity boundary

This removes wrong noise routing; it does not claim that five filtered-noise
octaves are the final head. On the factory ō-daiko the 40 resolved coordinates
end near **226 Hz** and the first continuum band is centred near **283 Hz**,
while the current loss/spacing estimate does not reach sustained modal overlap
until roughly **412–437 Hz** and about **128 coordinates**. That prematurely
stochastic interval is the deeper remaining source of generic attack identity.

A fixed Bessel extension through `lambda <= 23` can cover it, but doing so makes
those modes part of the reciprocal contact rather than decorative output: it
changes driving-point mobility, contact duration and rebound, and the existing
118-mode prototype roughly doubled dense CPU. Release therefore still requires
the force/velocity mobility capture and 48/96 kHz callback gates in the
[calibration contract](calibration/README.md). Observation-only tones would hide
the noise without fixing the collision and are not an acceptable substitute.

A fresh 128-coordinate implementation made that gate concrete and was reverted.
With the existing point contact, full hits collapsed to about **0.32–0.34 ms**
across 44.1–384 kHz, Don and Ka lost their required dynamic span, one adjacent
velocity step fell by **5.01 dB**, and hard-hit glide, live tension modulation,
palm and tack regressions all failed. Those are coupled-contact failures, not
new baselines to bless. The next attempt must introduce a measured finite bachi
footprint or passive residual mobility alongside the extra poles and must keep
contact duration, rebound and velocity monotonicity inside capture uncertainty.
It also missed the realtime gate: sixteen simultaneous contacts at 96 kHz with
32-sample buffers exceeded the 333 us deadline in **10 of 500** callbacks
(p99 **384 us**, maximum **2.06 ms**) on the review M1 Max. The corresponding
48 kHz/64-sample stress stayed within its 1.333 ms budget (p99 **578 us**), so
the failure is specifically at a supported low-latency configuration rather
than a vague request for future optimisation.

## Construction-aware tack attack — 2026-08-16

One remaining metallic attack was not a filter-calibration problem at all. The
drum catalogue already says the ō-daiko and chū-daiko are *byō-uchi*, with
their cowhide nailed on, while the Okedo and Shime are rope- or cord-laced.
The tack source nevertheless looked only at the stroke profile, so a rim hit on
all four family members excited the same synthetic iron-nail chatter.

That false source dominated the part of the hit most likely to read as cheap
noise. Across 64 deterministic full rim strikes, it supplied **98.9 %** of the
Okedo and **91.6 %** of the Shime energy above 10 kHz in the first 5 ms.
Removing only that source lowers the same band by **23.07 / 10.83 dB** and the
region above 5 kHz by **10.11 / 2.58 dB**, while complete-stroke RMS moves by
only **0.004 / 0.050 dB**. The correction therefore removes nonexistent metal
without thinning either drum's membrane or shell.

`DrumDescription` now records whether a head is tacked. In **4 Drums**, the
selected family's construction owns the source: ō-daiko and chū-daiko retain
it, Okedo and Shime zero its scale, rim coupling and preload. In **1 Drum**, all
four pads are one tacked ō-daiko retuned, so all four retain the same source.
This is one table lookup at trigger time and adds no sample-loop work; an absent
source skips the existing tack branch altogether.

The focused contract independently pins the physical catalogue as
`{ tacked, tacked, laced, laced }`, exercises both layout endpoints across all
four pads, and keeps the existing articulation, Stick Noise, preload threshold
and audible-detail checks. It also corrects an older premise: with fixed tack
spacing, one tack carries `tension * spacing`, so diameter alone does not raise
its preload. A fresh 26-take render changes only the four demonstrations which
actually contain Family-layout Okedo/Shime edge strokes (`03`, `05`, `08` and
`09`) and retains **−3.1 dBFS** worst-case pre-normalisation headroom.

The tack filter's own shape was deliberately not folded into this construction
patch. Its isolated level fell **1.76 dB** from 48 to 96 kHz and **7.06 dB** by
384 kHz, and a variance scalar alone would merely have made the same broad
shelf equally loud at every host rate. That separate defect is superseded by
**Bounded tack spectrum** below, which changes the topology before normalising
it.

### Remaining low-velocity contact boundary

The articulation audit found a separate mechanical problem which must not be
hidden by another output curve. At factory controls, Tsu loses about
**3.8–4.2 dB** of energy when velocity rises from 0.35 to 0.50 across
44.1/48/96 kHz. The narrow branch crossings are now pinned more precisely:
positive-force contact drops from **10.833 to 1.146 ms** for Tsu at velocity
0.40 → 0.45, **5.708 to 1.146 ms** for Ka at 0.60 → 0.65, and **3.792 to
1.688 ms** for Don Rim at 0.45 → 0.50. The basic solver is rate-converged, so
this is the current point-mobility/contact model following the moving head, not
a sample-rate instability.

The existing articulation `levelScale` also participates in reciprocal contact
projection. Moving it naively into impact speed is not the fix: the measured
counterfactual worsened the minimum adjacent Tsu energy step from about
**−4.17 dB** to **−6.41 dB** at 48 kHz. Clipping contact at the analytic
Hertz duration would conceal the branch change and break the passive solve.
Release work here remains gated on measured driving-point mobility and bachi
tip/contact data. A finer 48/96 kHz scan brackets the current model's Tsu,
Don Rim and Ka transitions near **0.79 / 1.00 / 1.51 m/s** respectively, while
peak force stays smooth. The revised [capture protocol](calibration/README.md)
therefore measures those three articulations from 0.25 to 2.00 m/s in at most
0.15 m/s steps, then retains 3.5 and 4.5 m/s; its old approximately 2 m/s low
point stepped over the entire failure region. Those model thresholds are not
targets—a smooth real-drum result is a valid falsification.

The capture-independent audit matrix is all four articulations × velocities
`{0.20, 0.35, 0.40, 0.45, 0.50, 0.60, 0.65, 0.70, 1.00}` ×
44.1/48/96 kHz. Until owned measurements provide uncertainty, it may assert only
finite/passive contact, release/rebound and cross-rate convergence—duration
within 6 %, impulse within 1 %, peak force within 4 % and normalised observer
energy within 1 %. It must report the non-monotone branches rather than bless
them as new targets. Monotone rendered energy becomes a release acceptance only
after the fitted mobility/contact model agrees with the captures.

That matrix is now executable. Its worst cross-rate spreads are **3.262 %** in
duration, **0.068 %** in impulse, **1.068 %** in peak force and effectively zero
in normalised residual exposure, so the branch is not a host-rate artefact. Its
assertion-free diagnostics retain both sides of the defect: Tsu 0.40 → 0.45
shortens positive-force contact by **89.42 %** at 48 kHz and loses **5.884 dB**
of 0–30 ms stereo energy at 96 kHz. The test deliberately reports those values
without accepting them as physical targets.

A free-flight prototype identifies the state-machine edge exactly. Just above
each threshold the first force run crosses zero, production permanently retires
the bachi, and the freely extrapolated stick would meet the returning head again
roughly 0.06–0.23 ms later. Preserving the ballistic stick and recomputing the
Hunt–Crossley onset damping for each force run removes the cliffs: over
velocities 0.01–1.00 the worst adjacent duration ratio falls from **8.88× / 4.87×
/ 2.18×** for Tsu / Ka / Rim to at most **1.111×**, and the worst 80 ms
energy loss falls from **7.34 dB** to **0.175 dB**. This is a real topology
result, not a new scalar; multiple contacts during one membrane rebound are
also predicted and observed in [Agüero et al.'s membrane-impact
study](https://arxiv.org/abs/2208.11089).

That prototype is not released on its own. Its Don 0.50 hit grows a second
**15.29 ms** force run carrying 91.5 % of the impulse, and complete demonstration
RMS moves by as much as **5.47 dB**. The resolved bank supplies global head
displacement but not the local high-wavenumber reactive mobility which ejects a
real bachi. A pure-tension point membrane (`D = 0`) has no parameter-free
high-frequency completion; the conditional stiff-plate point limit derived
below still does not identify a real bachi's finite traction footprint.
The missing contact footprint depends on tip geometry which the present Hertz
stiffness does not identify separately. A duration cap would only hide that
boundary. The eventual release path is therefore the verified free-flight and
recontact topology together with the measured finite footprint and positive-real
residual mobility, passing this same matrix.

#### Rejected rim-return delay

A second private prototype tried to postpone recontact until a nondispersive
membrane ray could travel to the nearest rim and back:

```text
c = sqrt(T / sigma)
t_gate = 2 a (1 - rho) / c
N_gate = ceil(sample_rate t_gate)
```

That delay is not a valid unilateral-contact update. Compression can become
positive while the bachi is excluded from the solve; switching Hunt--Crossley
back on then creates `K delta^2.5 / 2.5` potential without contact work. The
problem is also dispersive: for the underlying unnormalised tensioned-plate
relation, with
`u = D k^2 / T`, the group-speed ratio is

```text
c_g / c = (1 + 2 u) / sqrt(1 + u),
```

so retained high modes already return before the membrane-only estimate and
omitted higher modes are faster again.

The 120 ms audit covered six rates from 8 to 384 kHz, four octaves, four
articulations and nine velocities (**864 cases**). Production produced one
force run and retired in every case. The delayed prototype produced multiple
runs in **203**, left **97** contacts active at the horizon, produced **221
positive-compression rearm events** and reached **18** runs. On the 48 kHz factory
odaiko Don at velocity 0.50, the first run ended at 0.708 ms; the 15.333 ms
gate then rearmed the bachi **14.727 mm through the head**. The recreated
potential was about **3,978 times** the initial bachi energy, peak force rose
from 297 to 6,697 N, impulse from 0.0694 to 0.4696 N·s, and the explicit energy
audit reached **70,746 times** its initial value. It failed 299 DSP assertions
across contact, energy, dynamics and output bounds. The formula and
counterexample are retained here so a travel-time gate is not rediscovered;
recontact must remain continuously coupled through a passive residual mobility.

### Added-mass consistency boundary

The membrane-frequency law is algebraically an added surface mass,

```text
sigma_eff = sigma + 0.85 rho_air a
                    (2.4048 / lambda) / (1 + 0.6 m),
omega_loaded = omega_dry sqrt(sigma / sigma_eff).
```

The released model uses that law to lower each pole but still uses the dry hide
`sigma` in several mechanical consumers: modal force and contact compliance,
the cavity matrix and its two-head coordinate normalisation, radiation and palm
loss, the collision-mass prior and one hostile state-remap fallback. Interpreted
literally as added mass, that is inconsistent; it makes the loaded pole behave
as though its stiffness had fallen while its mobility stayed light.

A complete isolated migration propagated `sigma_eff` through all of those
consumers and through matching observation/tail paths. It remained finite and
passive, retained the existing host-rate contact tolerances, and every live
membrane pole followed a seven-semitone test shift by the exact requested
factor. The result was nevertheless a deliberate revoice rather than a safe
denominator repair:

- the factory breathing branches moved **61.372 -> 51.925 Hz** and
  **94.506 -> 87.442 Hz** on the two large drums (the small-drum changes were
  about 1.4--1.6 %), while the tuned/sounding pitches stayed fixed;
- a full reference Don moved from **0.521 to 0.563 ms**, gained **18.2 %** in
  impulse and **9.5 %** in peak force, but lost **3.46 dB** over its first
  30 ms;
- the Tsu branch discontinuity was not removed; it moved from 0.40 -> 0.45 to
  0.45 -> 0.50 and the adjacent audio loss became **6.38 dB**;
- an open centred drum's strongest partial moved from the roughly 32 Hz lower
  branch to the next radial mode near 85 Hz, so spectral-winner readout and live
  Pitch tests need a semantic migration even though their individual poles are
  correct; and
- every demonstration changed, with raw RMS shifts spanning roughly
  **-5.73 to +2.05 dB**. The unresolved continuum anchor also fell to about
  0.575 of its released amplitude because it is referenced to the resolved
  receptance.

The coefficient `0.85` and its mode-shape falloff are an existing compact
approximation, not an owned mobility fit. Extending their authority from pitch
to the whole collision and output calibration without capture would therefore
turn one internally consistent approximation into a much larger unverified
product decision. The migration remains the correct architecture, but release
requires measured force/head mobility and pressure or an explicit versioned
revoicing review. It is not the capture-independent fix for the cheap-hit
contact cliff.

### Split-mode orientation boundary

Each non-axisymmetric membrane row already has two slightly different poles,
but all rows currently use the same global cosine/sine axes. A zero-azimuth,
Humanise-off hit therefore drives exactly one member of every split pair and no
pair beats. A prototype assigned each row one fixed hashed principal-axis phase
and used it consistently for contact, both capsule residues and analytic
readout. It removed all silent partners; at factory Humanise 0.4 the fraction
of pair observations with less than one tenth in the weaker branch fell from
**39.4 % to 12.1 %**. Across a 16-hit matrix its largest peak, attack and body
changes were only **0.55 / 0.91 / 0.73 dB**, with no contact, stability or
headroom failure.

That is not enough to call the hash physics. A globally aligned basis is
plausible for a hide dominated by one grain direction or directional tension,
while independent mode phases describe a different random irregularity. Once
the pair is split, its orientation relative to the strike and capsules is
audible and is no longer a harmless basis choice. The hash moved the sounding
anchor by about five cents and changed every Humanise-off hit; seven existing
tuning/readout/node clauses consequently required migration. It is a reasonable
statistical head-realisation policy, but it is not identifiable from the
present scalar controls.

The prototype is therefore not released. Supplement Experiment A's minimum
mobility matrix with a fixed-radius ring of at least 17, preferably 32, LDV
azimuths: resolving an unknown field through the released `m=8` family needs
that angular coverage rather than two points and an assumed cosine. The scan
must recover each resolved pair's principal axes, establish whether one global
material axis explains them, and validate held-out azimuths before those phases
become factory data. If a future product deliberately exposes generated head instances
instead, the seed and its phase prior become a versioned public voicing
contract, not a hidden helper constant.

### Solved-force continuum boundary

The five unresolved head bands are still shaded at trigger time by the Fourier
transform of the analytic Hertz pulse, while the resolved membrane and direct
pressure paths receive the force which the nonlinear contact actually solves.
That distinction is small on an ordinary short hit but large on the known
low-velocity contact branch. At the five factory band centres the solved-force
spectrum differs from the trigger-time spectrum by roughly **7--19 dB** for
long Ka and Tsu contacts. A private oracle, allowed to know the completed force
history in advance, reduced Tsu 0.40 by **3.45 dB** over 5--30 ms and by about
**10 dB** in both 250--1000 Hz and 1--4 kHz; full-velocity hits moved by at most
about 0.2 dB broadband. This isolates a remaining source of the cheap soft-hit
bed rather than another global noise-level problem.

No scalar or duration-only correction is released. The physical excitation of
an unresolved band depends on the complex Fourier integral of the complete
force history: a later part of the pulse can cancel an earlier part. The
current continuum stores only a non-negative RMS envelope and merges overlapping
hits with `hypot`, so energy emitted early cannot be removed when a contact
later proves long. Rescaling at release would step the level, delaying the
whole continuum would move the attack, and attenuating the shared drum envelope
would corrupt earlier strikes.

The eventual correction therefore needs a separate signed/complex residual
excitation for each active contact, or the measured positive-real deterministic
mobility which replaces this statistical hand-off. Its acceptance test must
recover the normalised transform of each solved force at every band, close on a
reference Hertz pulse, and prove that an overlapping second strike does not
rewrite the first strike's contribution. Until that representation exists, do
not hide the mismatch with a brightness trim.

For a completed contact the reference-compatible estimator is explicit. If
`f_H` is the analytic Hertz arch and `H_b` is the existing response of continuum
band `b`, then

```text
A_b(f) = A_b(f_H) || H_b * f ||_2 / || H_b * f_H ||_2.
```

The ratio returns the released fitted level exactly for `f = f_H` and avoids
the transform-null instability of a centre-frequency-only correction. It is
not causal in the current architecture: the norm and any later phase
cancellation are known only after the complete contact, while the shared RMS
envelope has already emitted the prefix.

A smooth causal scalar ceiling does not repair that representation. In a
private prefix-preserving prototype it cut an ordinary chū Don by about
**3 dB**, worsened the known Tsu adjacent-energy cliff from **-5.884 to
-7.146 dB**, and still compressed very different rapid shime contacts to nearly
the same residual level. The failure is structural: a short contact and a
longer one share the same prefix, but only the completed complex history says
how much later force cancels the early spectrum. A causal scalar cannot both
leave the first case transparent and retroactively rescale the shared prefix of
the second.

Nor is a finite patch or local Foster bank identifiable from the released
controls. Hunt--Crossley stiffness fixes `E*sqrt(R_tip)`, not tip curvature;
plausible tips moved the residual mobility by several decibels and tens of
degrees. A five-branch positive-real prototype remained local/diagonal, lacked
the cross-contact mobility needed by simultaneous strikes, added roughly
two-thirds to the hostile callback cost, and materially worsened soft-contact
duration at plausible footprint scale. The capture inventory therefore stores
raw traction maps, bachi mass and tip profilometry; the analyzer must derive the
traction-weighted complex mobility and fit one reciprocal positive-real spatial
residual from those data.

### Conditional ideal-point residual derivation

The lossless infinite-domain tensioned Kirchhoff plate underlying the current
model does admit a parameter-free *ideal point* mobility. With surface density
`mu`, tension `T`, bending stiffness `D`, Laplace variable `s` in `Re(s) > 0`
and the passive analytic branches of `sqrt`, `log` and `K0`, define

```text
Delta = T^2 - 4 D mu s^2,
alpha^2 = (T - sqrt(Delta)) / (2 D),
beta^2  = (T + sqrt(Delta)) / (2 D).
```

The infinite-domain cross mobility is

```text
Y(r,s) = s [K0(alpha r) - K0(beta r)] / (2 pi sqrt(Delta)),
```

and its finite point limit and high-frequency driving-point limit are

```text
Y(0,s) = s log((T + sqrt(Delta)) / (T - sqrt(Delta)))
         / (4 pi sqrt(Delta)),
Y_inf = lim_(s -> infinity) Y(0,s) = 1 / (8 sqrt(D mu)),
Z_inf = 1 / Y_inf = 8 sqrt(D mu).
```

A common-pole quadrature with positive-semidefinite spatial residues can
therefore be reciprocal and positive-real without a fitted resistance. That is
a valid ideal-point research model, not a complete bachi contact. A real
traction patch changes the high-frequency topology and introduces the missing
second moment

```text
a2^2(F) = <r^2> / 4,
a2^2(F) = b^2 / 10 = (R_tip / 10) (F / K)^(2/3)
           for a spherical Hertz patch.
```

The released `K = (4/3) E* sqrt(R_tip)` cannot identify `R_tip`, hence cannot
identify that form factor. On the factory odaiko the ideal-point plate length
is about 6.34 mm and its asymptotic point impedance about 4.44 kg/s; plausible
millimetre-scale patches change the residual by several decibels in the first
five continuum bands. The derivation is retained for a future shared spatial
state, but release still requires measured traction geometry and complex
cross-mobility rather than treating `D` as a hidden tip-radius control.

The private audit directories themselves are not product artifacts: each is a
copied source tree plus build output, and the downloaded papers remain the
copyright holders' files. This plan retains the original equations, numerical
counterexamples, source links and release decision instead. Failed prototype
code is deliberately not vendored as a stale patch that could be mistaken for
a supported alternative.

### Live continuum variance under retuning

One narrower error did not need a new mobility model. Every continuum octave
is normalised by the exact stationary variance of its two-high-pass,
seven-low-pass filter when the drum is built, but live Pitch and Tension Mod
subsequently moved the filter coefficients while leaving the stored RMS
envelope unchanged. An upward octave therefore gained roughly **2.5--2.9 dB**
from filter bandwidth alone. A structural rebuild had the same coordinate
mismatch, and a later flam could inject a base-filter amplitude into a filter
which had already moved.

The released state now stores the exact build variance and the current live
variance. On a pole move it multiplies the envelope by
`sqrt(V_old / V_new)` and every left/right filter state by the reciprocal.
Their product, and therefore the current output sample, is continuous while the
new recurrence settles into its own covariance. New contacts are converted by
`sqrt(V_strike / V_live)` before the shared drum envelope adds them in
quadrature; a structural rebuild uses the same coordinate change while keeping
its separate microphone-distance remap.

The control path uses a small prepared `log2(V)` table rather than solving a
nine-state covariance every 32 samples. A dense sweep including both Nyquist
clamp seams stays below **0.05 dB** power error. Regression probes seed live
filter memory, retune it, inject a later contact and rebuild the physical drum,
then independently compare current products and stationary power. Lookup,
live-power and later-injection errors stay below **0.05 dB**, structural rebuild
power below **1e-5 dB**, and output-product discontinuity below **2e-7**. The
complete DSP/tool suite remains green.

This closes automation arithmetic, not the main cheap-hit boundary above.
Factory attack stretch reaches only **1.00296 / 1.00826 / 1.00481** for Don /
Ka / Tsu, so the correction is just **-0.009 / -0.016 / -0.013 dB** there.
Stepped pitch automation is material: the isolated continuum moved by about
**+4.82 / -2.73 dB** at plus/minus two octaves before correction. Do not cite
this small ordinary-hit delta as a replacement for the signed residual drive,
finite traction footprint or measured reciprocal high-mode mobility.

### Zero-azimuth split-pole consistency

One smaller inconsistency was independent of those capture boundaries and is
fixed. The renderer always builds a slightly detuned cosine/sine pair, but the
zero-azimuth analytic path ranked the unsplit parent frequency while rejecting
the silent sine member. It now still rejects that member and ranks the cosine
pole at the exact detuning the renderer uses. A focused test reads the built
pole and the analytic observation independently; reverting the one source line
fails it.

The correction closes both Drum Layouts on **59.747 / 119.495 / 238.990 /
477.979 Hz**. In 1 Drum the three upper rows moved by about 2.54 cents; in
4 Drums the two small instruments moved by the same amount while the large two
were already on the correct identity. Tensions move by at most 0.305 %, small
drum radii by at most 0.130 %, and the worst peak in a 16-hit sweep gains
0.009 dB of headroom. Earlier dated sections which record 59.660 Hz or the
13.4879 first tension-ladder ratio are historical compatibility snapshots;
the renderer-consistent values supersede those pins.

## Humanise in head coordinates — 2026-08-16

The former angular jitter used `pi * Humanise` regardless of strike radius.
That made the same nominal uncertainty physically tiny near the middle and
enormous at the edge. At the factory 40 % setting, the maximum tangential travel
was **0.188 / 1.144 / 0.251 / 1.219 head radii** for Don / Ka / Tsu / Don Rim,
while radial travel was at most **0.022 radius**. On the factory ō-daiko an edge
hit could therefore jump roughly 90 cm between nominally repeated strokes.

Humanise now draws independently bounded radial and tangential components in
head coordinates and converts the latter to an angle using the final strike
radius. Each component is at most `0.055 * Humanise`; the combined vector can
therefore reach `sqrt(2)` times that bound. The radius is guarded near the
centre, where azimuth is physically undefined. In a 256-hit factory sweep the
mean Cartesian travel becomes **0.016 radius for all four articulations**; the
95th percentile is about **0.026**, and the systematic
21–22 % inward bias of the old angular arc falls below **0.0013 radius**. Ka no
longer jumps across the close pair: its direct-path delay remains
**+19.43..+20.20 samples** instead of spanning **−18.4..+23.5**, and its ILD
stays on the authored side at **−2.70..−2.54 dB** instead of crossing
**−2.66..+2.52 dB**.

This is not an output randomiser. The one reciprocal modal projection built at
the resulting point still senses and spreads the same contact force. Humanise
zero is bit-identical, and no work enters the sample loop. The regression covers
all four written radii plus full-Humanise Don/Tsu at Centre 100, requiring both
finite audio and tangential travel no greater than **0.055 head radius**. Pitch
and family-octave tests disable Humanise explicitly, as the rest of the readout
suite already does, so an acceptance test for one authored drum is not coupled
to a particular random performance gesture.

Two fresh 26-take renders were byte-identical. Against the construction-aware
tack build, all **14 Humanise-off/deterministic takes remain byte-identical** and
exactly the **12 performance takes which use Humanise** change. Worst raw peak is
**−2.7 dBFS**, still clear of the limiter. The JUCE-free DSP suite passes 2/2
and the processor/state contracts pass with the same parameter layout.

## Bounded tack spectrum — 2026-08-16

Construction gating removed iron from the laced drums, but the two byō-uchi
drums still rendered each real tack as white noise through only one high-pass
and one low-pass. The source called 9 kHz its upper edge while placing about
**55 %** of its stationary power above that edge at 48 kHz. Its centroid then
moved from about **10.5 / 11.0 / 15.8 kHz** at 44.1 / 48 / 96 kHz. That was a
host-dependent hiss shelf, not bounded metal-on-wood chatter.

The tack now uses the same serial two-high-pass/seven-low-pass topology already
validated for one continuum octave, at the tack's existing 2.6 and 9 kHz
corners. No pitch, duration or listening-fit level was added. The exact
nine-state covariance is solved once in `prepare()`, and its gain is anchored
to the released filter's **energy inside 2.6–9 kHz at 48 kHz**. Energy which the
old filter leaked above its own named band is removed rather than pushed down
into the new one. An initial total-energy anchor did push it down, drove a
worst-case Rim hit into the limiter, and was rejected.

The eight-seed, 20 ms isolated-tack regression reads:

| Host rate | Tack RMS | power above 9 kHz / power from 2.6–9 kHz |
| ---: | ---: | ---: |
| 44.1 kHz | −38.453 dBFS | 0.0923 |
| 48 kHz | −38.573 dBFS | 0.0608 |
| 96 kHz | −38.184 dBFS | 0.0295 |

The level spread is **0.389 dB** and the new regression caps it at 0.75 dB;
every upper/band ratio must remain below 0.15, where the former filter was above
one around the reference rate. On a full 48 kHz o-daiko rim stroke the first
5 ms lose **13.64 dB above 9 kHz** while the declared tack band gains 1.22 dB;
over 10 ms the band moves only 0.25 dB. The tack remains audible at **6.614 %**
of the complete stroke RMS (−23.59 dB), and its threshold, construction, seed,
four-millisecond envelope and airborne routing are unchanged.

The generalized covariance helper is the only new setup machinery; rendering
is two short fixed-size loops while a tack envelope is alive. An exact
counterfactual puts the old one-high-pass/one-low-pass filter's worst rare crest
at 1.28012 before the limiter and this filter's at 1.28449: **+0.030 dB**. With
the tack disabled the same released model still reaches 1.11672. The filter did
not create the headroom defect and trimming it cannot solve it without erasing
the audible source.

The actual root was a factory reference calibrated from one favourable
Humanise/noise sequence. An offline sweep of 1,048,576 generated orders found
the hostile order 421768. Taikor now applies one fixed **−4 dB** reference
calibration after Drive and before the safety clamp. It changes no contact,
mode, continuum balance or saturation; the public **−24…+6 dB** Output range,
**−22.5 dB** default, saved values and normalised host-automation mapping remain
unchanged. The cheap regression sweeps orders 1–1024 and pins order 421768
explicitly over the 50 ms attack. The contiguous sweep peaks at **0.81046**
(order 351); the hostile sentinel peaks at **0.86448**, 0.82 dB below the 0.95
gate. This is strong sampled protection, not a mathematical bound over every
possible order. The complete JUCE-free suite passes **2/2 in 56.9 s**,
processor/state contracts pass, and a fresh 26-take render is
byte-identical to the pre-calibration set because its per-take normalisation
cancels this one global scalar.

## Shell orientation boundary — 2026-08-16

The shell bank's remaining frozen image is real but not safely repairable by
adding only the missing sine partner. A prototype represented each `n=2..7`
ring as the canonical pair

```text
d = [cos(n theta_strike), sin(n theta_strike)]
h = C_n [cos(n theta_mic), sin(n theta_mic)].
```

This restores rotational covariance, nodes, polarity and angle-invariant
injected energy. Anchoring each `C_n` to the released pair's mean stereo power
also needs no listening fit. It is nevertheless not a complete shell observer:
the full Rim sweep reached **0.985** peak and about **+11 dB side/mid**, and 20
existing headroom, mono and distance-image clauses failed. A gain trim cannot
repair the missing field.

Nor can a cheap Green-function wrapper. Any rotationally symmetric linear
observer of an ideal circular `n` doublet retains the same cosine/sine nodes,
so its side/mid ratio is unbounded at some strike azimuth. In Taikor's geometry
the capsules also sit inside the head radius; the exterior cylindrical wall is
not directly visible from them. Whole-wall `1/R` integration therefore sends
sound through the head/body, while the real path is rim diffraction and
scattering. Phase-free and coherent wall prototypes both retained roughly
**+11 to +12 dB** worst-case side/mid at 44.1/48/96 kHz.

The prototype was reverted. Adding a positive common floor would be an
invariant scalar glued to a non-axisymmetric representation, not a circular
shell mode; a nonlinear magnitude path would break the linear reciprocal
model. Release-grade shell orientation therefore remains capture-backed work:
joint shell/head mobility, rim diffraction/scattering and capsule pressure must
be fitted together before the six doublets replace the current bounded body
observer.

## Okedo direct-shell correction — 2026-08-17

The hit-side shell boundary had a simpler safe half. The shell bank has zero
contact projection and is absent from IMP-2 sensing and compliance, but every
head strike used to feed it a one-way copy of the solved normal force. On the
light Okedo this fixed six-pole ladder overwhelmed the membrane by **26.8 dB on
Ka** and **39.3 dB on Don Rim** over 5–30 ms; its 191 Hz pole was within 0.7 dB
of the entire shell bank. That was the cheap, invariant body tone heard on the
family's third drum, not stochastic continuum noise.

Taikor now makes the articulation topology explicit with `strikesHoop`. Don,
Ka and Tsu hit only the membrane, so their shell projection is exactly zero;
the shell material still changes the head's resolved boundary/edge loss. Don
Rim catches the hoop and retains the existing shell formula, decay and Shell
Resonance curve byte-for-byte. No radiation scalar, EQ or replacement gain was
added. In the fixed-seed 48 kHz stereo-RMS audit, factory Okedo Ka changes by
**−17.6 / −22.0 / −18.8 dB** over 0–5 / 5–30 / 30–120 ms and Tsu by
**−4.0 / −10.3 / −20.5 dB**; Don Rim is bit-identical.

The deletion is consistent with the closest published head-struck, rope-laced
double-head experiment, [Ono et al. (2009)](https://doi.org/10.1250/ast.30.410),
on a 20 mm wood-plastic p-wadaiko:
its isolated shell resonance did not appear among the complete drum's peaks,
while the approximately 200 Hz component was assigned to the membrane. It does
not prove zero mechanical shell drive or calibrate a replacement transfer. A
nearby published shell frequency is not provenance for Taikor's 191 Hz pole:
[Hwang and Suzuki (2016)](https://doi.org/10.1250/ast.37.115) measured hollow
zelkova barrels with their diaphragms removed, and the smooth barrel's 192 Hz
mode was the axial-varying `(1,2)` family; the closest axial-uniform `(0,2)`
family was 137 Hz. The construction boundary is independently consistent with
[Asano's Okedo patent](https://patents.google.com/patent/JP3097219U/ja), which
calls 10--10.5 mm the usual thickness for conventional cedar boards and
specifies quarter-sawn paulownia at 8--9 mm. Neither source supplies the
coupled residue, Q, directivity or phase needed to define a replacement gain.
That remaining path still requires joint head/shell mobility and pressure
capture; adding an acoustic `levelScale`, infinite-cylinder efficiency or a
fixed common floor would merely choose one of infinitely many mechanical
projection × observer factorizations. The release keeps the measured boundary
loss and the unambiguous direct hoop strike, and deletes the unsupported source.

## Performer identity for multi-instance ensembles — 2026-08-16

Four Taikor instances receiving the same MIDI formerly began from the same
stroke count and therefore made the same Humanise choices. Summing them made a
louder phase-locked performance, not four people. The new Performer choice is
appended after Strike Azimuth as P1–P4. Its identity salt reaches the existing
Humanise-owned contact point, impact speed, contact time, press-roll schedule
and stochastic head/contact sources. Because position, speed and duration feed
the reciprocal solve, the identity changes the performed resonant hit rather
than merely swapping a cheap noise texture.

The scope is intentionally exact. P1 contributes a zero salt and therefore
retains the established random-sequence arithmetic path. Humanise zero disables
the salt, so P1–P4 all render the same authored machine-perfect stroke. No
identity detunes or resizes a drum, changes an articulation calibration, adds a
fixed onset offset, alters Output, or hides an ensemble gain law. To arrange a
group, layer two to four instances on the same MIDI part, assign distinct
identities and place or time those instances explicitly in the host. Performer
is persisted but non-automatable and should be chosen before playback; changing
it in the UI affects new hits while an existing physical tail stays continuous
until retirement, panic or reset.

That workflow addresses a market expectation rather than a speculative extra.
Sonica's official [TAIKO THUNDER](https://sonica.jp/instruments/en/product/taiko-thunder-the-ultimate-collection/)
page sells a solo or two- to four-person ensemble function; In Session Audio's
[Taiko Creator](https://insessionaudio.com/products/taiko-creator/) creates
multiple solo and ensemble groups; and the hybrid-modelled
[Supreme Drums Taiko](https://neovst.com/product/supreme-drums-taiko__trashed/?lang=ja)
also covers solo and ensemble drums. Taikor's differentiator is that these
identities perturb a live contact solve instead of selecting another captured
round robin, while leaving the physical instrument definition visible and
unchanged.

The DSP regression renders all four identities twice and requires exact repeat
determinism, separation for all six identity pairs in both the complete waveform
and the resolved resonant object with continuum/contact noise/tack/direct
pressure removed, and a unity-sum P1–P4 layer whose waveform is not merely
gain-scaled P1. It separately requires Humanise-zero equality and runs the
maximum-Humanise rim headroom window plus the known hostile-order sentinel for
every identity.

The processor contract pins the appended 24th slot, its four discrete labels,
P1 default, state round trip, P1 injection into legacy states and a laid-out
editor control.

## Finite-separation two-head radiation — 2026-08-16

The released axisymmetric loss had one remaining zero-depth assumption. It
formed the two head shares as `(b + r)^2`, so an opposing pair whose monopole
moments cancel lost almost no energy to the air even when its heads were more
than a wavelength apart. That made the small drums carry a second, conspicuously
long tonal body which was not present in the hit onset.

Within the existing net-volume monopole approximation, the exact
finite-separation power is

```text
P = b^2 + r^2 + 2 b r sinc(omega L / c).
```

It is the standard mutual-radiation term for two coherent sources, tends to the
old expression as `L -> 0`, and is passive because `|2br| <= b^2+r^2` and
`|sinc(x)| <= 1`. Taikor now uses that power consistently in the analytic
observer's decay, the built resonator bank, live tension/pitch retuning and the
reported tail. The released front-pressure observation itself is unchanged:
this correction says how fast the pair loses mechanical energy, not how the
rear source diffracts around the shell into the front capsules.

The factory shime lower `(0,1)` branch moves from **0.18462 to 1.29699** in
normalised radiated power and from **1.685 to 0.851 seconds T60**. Its complete
Don changes by only **0.001 dB in the first 80 ms**, then by **-4.30 dB from
80–500 ms** and **-20.28 dB from 0.5–1.5 s**. The okedo lower branch moves from
1.456 to 0.934 seconds T60; the ō-daiko and chū-daiko changes are small. This
is deliberately a body/tail correction rather than another attack EQ.

`testFiniteHeadSeparationRadiatesADipole` owns the zero-depth limit, closed-form
`sinc(kd)` power, non-negative bound, shime opposing branch, agreement between
analytic and built decay, live retune phase, and exact preservation of the
single-head/non-axisymmetric arithmetic. The complete DSP/tool suite passes
three registered tests. Rear-head pressure, phase and a complete radiation
matrix remain capture-backed work under the controlled calibration contract.

## Hertz residual exposure and controller calibration — 2026-08-16

The residual audit found one exact reference error before it found a level to
tune. Taikor's analytic force arch is `sin(πt/τ)^1.5`, whose impulse integral is

```text
sqrt(pi) Gamma(5/4) / Gamma(7/4) = 1.7480383695280799.
```

The former **2.3963** is the integral of `sqrt(sin(x))`. Correcting it restores
the collision impulse represented by the reference peak. In the uncapped
continuum path the matching powers cancel, so this is a normalization repair
rather than an independent brightness fit.

The real product defect was one long low-mode contact reusing the stochastic
tail calibration hundreds of times. Each contact now receives the exact direct
rigid-target Hunt–Crossley squared-force integral as its maximum legacy
`integral(F^2/Z dt)` exposure. This is explicitly **not mechanical energy or a
passivity bound**: the current residual impedance is per unit length. The limit
only bounds the share sent to `injectContinuumEnergy`; the coupled force still
drives the resolved head, contact roughness, direct pressure and tack line
unchanged. At 48 kHz the factory chū Don requests about **0.953** of the limit
and remains uncapped, while the hostile shime Don requests about **446×** and
spends one bounded exposure.

The cap-disabled A/B covered all four drums and articulations plus flams,
alternating rolls and 18 ms retriggers. The large shime correction removes the
noise-dominated onset, but all four shime **80–500 ms tails are unchanged**.
Main-flam, alternating-roll and rapid-retrigger onset contrasts remain about
35, 16 and 14 dB respectively. That is enough to ship the bound; it is not an
absolute calibration of the shime or of roll transfer. Those remain gated on
the controlled driving-point mobility, force and microphone-pressure captures,
and the cap must not be loosened to make unsupported stochastic level hide that
missing evidence.

The same tranche closes a smaller market-facing playability gap without adding
another sound effect. **Velocity Curve** is appended after Performer as the 25th
host parameter, preserving every established ID and automation index.

This is established pad-instrument calibration rather than feature invention:
Sonica's official [TAIKO THUNDER manual](https://sonica.jp/instruments/images/TAIKO-THUNDER-THE-ULTIMATE-COLLECTION-UserManual-En.pdf)
provides separate pad response settings and four velocity-curve types. Taikor
keeps the first tranche narrower—one continuous identity-default curve—because
Min, Max, Fixed and play-mode copies are not needed to solve the mapping gap.

It applies

```text
v_curve = pow(v, exp2(c)),  -1 <= c <= 1
```

before Velocity Depth. Soft 100 (`c = -1`) is `sqrt(v)`, Linear is `v`, and
Hard 100 (`c = +1`) is `v^2`; zero and full velocity remain fixed. Linear uses
an explicit identity branch, is the default, and is injected into legacy states,
so the established response and default audio do not move. This calibrates the
player's pads and technique to the model's impact-speed range. It does not claim
that a response curve is an acoustic property of a taiko.

## Fifteen proposed mechanisms, adjudicated — 2026-08-19

Two batches of proposed acoustic and performance mechanisms were put to the
engine. None of them survives to a released change, and this section records
what each one asked for, what the shipping tree already does, and what the
number is where a number decides it. Four of the fifteen are re-statements of
mechanisms this document already struck, and those entries say where; the rest
are adjudicated here for the first time. Two corrections to the README fell out
of the work and are described at the end, along with the two guards added to the
suite.

Everything below was measured on this tree with a standalone program linked
against `TaikorDSP`, at 48 kHz, factory controls unless stated.

### 1. A ground-reflection image source and a far perspective

Already adjudicated under "A single image-source floor reflection" in the second
pass, and the verdict is unchanged: the Allen–Berkley construction is exact,
needs no impulse responses, and needs the height of the head above the floor,
which Taikor does not model. What this pass adds is how much that missing number
decides, because the earlier entry states the objection without sizing it.

The first comb notch of a single specular floor image sits at *c/2Δ* with
*Δ = √(d² + 4h²) − d*. At the factory capsule distance of 15.95 cm:

| head height | extra path | first notch | image level re direct |
|---|---|---|---|
| 0.4 m | 0.656 m | 261.3 Hz | −14.18 dB |
| 0.5 m | 0.853 m | 201.0 Hz | −16.05 dB |
| 0.9 m | 1.648 m | 104.1 Hz | −21.08 dB |
| 1.3 m | 2.445 m | 70.1 Hz | −24.26 dB |
| 1.6 m | 3.044 m | 56.3 Hz | −26.06 dB |

The factory ō-daiko's resolved bank runs from 32.65 Hz to 226.0 Hz. So across
the range of stand heights a kumi-daiko stage plausibly spans, the first notch
sweeps the whole of that bank — it lands above every resolved mode at one end
and between the two branches of the fundamental pair at the other. The drawn
number does not set a subtle depth cue; it sets which of the drum's own partials
gets a notch cut in it.

The proposal's own 1.5–3.0 m perspective makes this worse rather than better,
because that is where the image stops being a decoration: at 1.5 m the reflected
path is only **1.1 to 7.4 dB** below the direct one across the same height
range, so the comb approaches full depth. A mechanism whose audible result is a
±6 dB comb positioned by an invented constant is not an improvement on having no
room at all, which is what the near-field pair honestly is.

The far half of the proposal has a second blocker that is already documented:
the released observer is a real scalar near-field approximation, and the complex
Rayleigh observer that would be valid at metres exists behind a false release
gate pending the controlled near/far pressure capture. Extending Mic Distance
past 40 cm would run the near-field law outside its own derivation.

*Still the right feature. It becomes available the day the stand does, and the
stand is an absence finding with no numbers behind it, not an oversight.*

### 2. A Hertz contact patch as a spatial low-pass

Already struck in review under "Letting the contact patch widen with the force
(gap 5)". Re-measured on this tree, on the resolved bank rather than on the
continuum the earlier prototype tested, and the verdict holds by a wider margin
than the earlier entry claims.

*a_c = √R_tip · (F/K)^(1/3)* follows from the Hertz pair *K = (4/3)E\*√R_tip*
and *a_c³ = 3FR_tip/4E\**. The engine carries only the product *K* — a
geometric interpolation from 2e6 to 6e8 scaled by `profile.hardnessScale` — so
*R_tip* has to be supplied from outside. The nearest thing this repository ever
had to one is the 24 mm dowel of the retired `resolveStickFor`, whose 12 mm
radius is used below as a stand-in for the tip's curvature — which is itself the
objection, since a dowel end is flat and a flat punch is not a Hertz contact at
all. At full velocity, for a Don:

| drum | head radius | peak force | *a_c* | *k·a_c* | *2J₁/x* |
|---|---|---|---|---|---|
| ō-daiko | 0.7500 m | 1717 N | 2.751 mm | 0.0477 | −0.0025 dB |
| chū-daiko | 0.3900 m | 716 N | 2.055 mm | 0.0686 | −0.0051 dB |
| okedo | 0.1997 m | 252 N | 1.452 mm | 0.0946 | −0.0097 dB |
| shime | 0.1498 m | 149 N | 1.219 mm | 0.1059 | −0.0122 dB |

Worst over all four strokes on those four drums is **0.0330 dB**. Halving the
assumed tip to 6 mm takes it to **0.0165 dB**: *a_c* goes as *√R_tip*, so the
attenuation — *x²/8* to leading order — is linear in the number that had to be
drawn. The largest value anywhere the controls reach is **2.92 dB**, on a 3 cm
head at Bachi Hardness 0, which is not an instrument.

Two further findings, neither in the earlier entry:

- **The mechanism runs the wrong way for the audible impact claimed for it.**
  The proposal offers it as rounding off harsh high-frequency ping on heavy
  blows while keeping light taps crisp. Measured, the patch is *largest* for the
  *softest* stick — 6.14 mm at Bachi Hardness 0 against 2.75 mm at the factory
  0.7 on the ō-daiko — because contact stiffness rises across that control far
  faster than peak force does. Releasing it would dull soft strokes, which are
  already dull, and leave hard strokes essentially untouched.
- The small-deformation limit the Hertz solution is derived under is respected
  at the factory hardness (16.7 % of the tip) and is not at Bachi Hardness 0
  (51 % against a 12 mm tip, 72 % against 6 mm), which is the earlier entry's
  second objection, now located on the control that causes it.

*Struck for the resolved bank, as it was already struck for the continuum.*
`testTheContactPatchWouldNotBeAudibleOnTheResolvedBank` recomputes the whole
table from the engine's own contact solve so the claim cannot go stale.

### 3. Reciprocal head-to-shell coupling on membrane strikes

The proposal is to route a fraction of the membrane's rim reaction force
*F = ∮ T ∂w/∂r ds* into the shell's six ring modes, so that a Don wakes the
wooden body. Three things stop it, and the first is decisive.

**The net rim force is orthogonal to every mode the shell bank carries.** The
ring frequencies come from the thin-cylinder flexural result

```text
f_n = n(n^2-1)/sqrt(n^2+1) * h/(2 pi R^2) * sqrt(E/(12 rho (1-nu^2)))
```

evaluated at `n = index + 2`, so the bank is orders 2 through 7. That formula is
zero at *n* = 0 and *n* = 1 by construction: the bank has no breathing mode and
no translation. A *net* downward rim force is exactly the *n* = 0 component of
the edge load, and a membrane mode with circumferential order *m* delivers its
rim load as *cos(mθ)*, whose only nonzero projection is onto ring order *m*.
Coupling the net axisymmetric force into orders 2–7 is projecting a load onto
modes it is orthogonal to; the result would not be the chest thump the proposal
describes, because the chest thump is the *n* = 0 mode and that mode is not in
the bank.

**The order-matched version that is legal needs a coefficient the engine cannot
supply.** Membrane order *m* → ring order *m* is the correct projection and has
the right behaviour for free — a centred Don barely excites *m* ≥ 2, an edge Ka
excites it strongly — but the rim load is *axial* and a flexural ring mode is
*radial*, so the two are joined by the shell end's axial-to-radial edge
compliance. The bank is six frequencies with a drawn *Q* of `12 + 40 ·
shellMaterial`, not a shell solve; putting a Donnell edge-load compliance on top
of it would be a precise mechanism attached to an imprecise object.

**The proposal's own magnitude is drawn.** "≈2–5 % of the net force", scaled by
*h_wall/r_drum*. The 2–5 % is exactly the kind of number this document exists to
refuse, and the code comment that currently sits where the coupling would go
says so: the previous attempt copied the solved normal force into the ring modes
and made the light Okedo body overwhelm its head; what it needs is "a measured
mechanical projection rather than that duplicate force".

*Not planned. The mechanism is real; the engine's shell is not solved finely
enough to receive it, and the head's own boundary loss already accounts for the
work sent into the mounting.* Note also that the proposal's description of the
code is of an older tree: `usesDrumBody` does not exist here — the field that
gates the shell is `profile.strikesHoop`, and only Don Rim sets it.

### 4. Thermoviscous cavity losses tied to Body Depth

The physics is right and the arithmetic kills it, in two separate ways.

**The stated scaling is backwards.** For a closed cylinder,
*S/V = 2/R + 2/L*, which *falls* as the body gets deeper. The proposal's
*γ ∝ (√ω/R)(1 + L/R)* rises with *L*. The measured surface-to-volume ratio on
the factory ō-daiko is 4.235 /m at a 1.275 m body; the shime at its shallowest
is 29.99 /m. Deep drums have *less* boundary-layer loss per unit volume, not
more, so releasing this would shorten shallow drums rather than lengthen deep
ones — the opposite of the claimed audible impact.

**The size is inaudible.** Kirchhoff's boundary-layer result for an enclosed gas
gives the compliance a loss factor *(γ−1)·δ_t·S/2V* with
*δ_t = √(2α/ω)*; on the resolved drums that runs 2.8e-4 to 6.6e-4. It reaches a
mode only through the share of that mode's stiffness the cavity actually holds,
which is exact here because the cavity's contribution to the symmetrised
two-by-two is a rank-one *c·vvᵀ*. Over four octaves × eleven Body Depths × three
Air Couplings:

| where | branch | cavity share | *η_cav* | added decay | T60 |
|---|---|---|---|---|---|
| worst anywhere | 567.68 Hz | 0.2846 | 6.57e-4 | **1.486 %** | 0.3078 → 0.3032 s |
| factory ō-daiko, breathing | 61.37 Hz | 0.7162 | 2.82e-4 | 0.573 % | 1.0163 → 1.0105 s |
| factory ō-daiko, lower | 32.65 Hz | 0.0002 | 3.87e-4 | 0.0001 % | 0.5670 → 0.5670 s |

A 1.49 % worst case is 4.5 ms on a 308 ms tail. There is also a validity
objection: the lumped column already runs to *x* ≈ 0.94 against the half-column
quarter-wave at *π/2*, and a boundary-layer loss factor derived for a compliance
would be applied outside its own range at the same corners the stiffness is.

*Struck on audibility, as gap 13 was.* `testTheEnclosedAirIsLosslessOnlyWhereThatIsInaudible`
recomputes the bound from the resolved drum every run.

### 5. Spatially localised and frequency-dependent hand muting

Half of this shipped in "Passive contact and controller palm — 2026-08-11" and
the proposal's premise — "MIDI CC1 currently applies a single uniform decay
multiplier across all 40 modes" — is not true of this tree. CC1 anchors a palm
patch at Tsu's radius and takes each mode's rate from `palmDampingRates`, which
integrates the mode shape over a physical hand-sized disc. Measured on the
factory ō-daiko, the per-mode rates run from **0.0001 /s to 122.4 /s** across
the resolved bank — six orders of magnitude — and they do it in exactly the way
the proposal asks for:

| mode | frequency | hand rate | tail kept under full CC1 |
|---|---|---|---|
| entry 19, *m*=8 | 214.73 Hz | 0.0001 /s | 100.0 % |
| entry 17, *m*=6 | 172.92 Hz | 0.0034 /s | 99.9 % |
| entry 13, *m*=4 | 130.14 Hz | 0.115 /s | 96.9 % |
| entry 10, *m*=3 | 107.71 Hz | 0.676 /s | 83.1 % |
| entry 8, *m*=2 | 142.50 Hz | 29.56 /s | 10.5 % |
| entry 1, *m*=0 | 85.64 Hz | 99.07 /s | 3.9 % |
| entry 6, *m*=1 | 171.59 Hz | 122.38 /s | 5.6 % |

A mode with a node under the palm keeps its tail; a mode with an antinode there
is quenched. Note that this ordering is not frequency: the highest resolved mode
at 226.01 Hz keeps 12.2 % while a mode 11 Hz below it keeps all of it, because
the *m* = 8 shape has almost nothing under a hand at the middle of the head.
That is the proposal's own second bullet, shipped. The suite already guards it:
`testHandControllerIsAPhysicalPalm` fails if the palm collapses to one global
gain. Two axisymmetric branches read exactly zero, which is also right — they
live on the rear head, and a hand on the batter head does not touch them.

What genuinely remains uniform is the *continuum* — one rate for all five bands
— and its comment gives the reason: in the high-modal-density limit the local
mean square cancels between the patch and the whole-head modal norm, leaving the
area ratio, which carries no frequency. That is correct for a viscous surface
damper and is not an oversight.

That leaves the proposal's *η_hand = η₀(1 + c₁ω/ω₀)*, which is the request to
replace a dashpot with a viscoelastic contact. `muteSurfaceDamping = 5000
kg/(m²s)` is already named in the source as one of the two fitted parts of that
contact; adding *c₁* would be a second fitted constant sizing a term whose only
evidence is that it sounds plausible. A hand's mechanical impedance against a
hide is measurable, and it is on the capture contract as head mobility.

*Not planned as stated. The spatial half is shipped and measured; the frequency
half is one drawn constant away and belongs to the capture.*

### 6, and batch two's 8 and 9. Sympathetic coupling, stage bleed, ensemble staging

Three statements of one mechanism, already adjudicated as "An ensemble inside
one instance (gap 6, narrowed)" and answered by Performer identity. Nothing has
changed: a cross-drum bus needs the distance between two drums, their relative
angles and a coupling level, and Taikor models one drum with a stated head
diameter and no position in any room. The proposal supplies "<−40 dBFS", the
staging variant supplies "±3–12 ms" and "0.5 dB/10 m at 10 kHz", and each of
those is drawn. The air-absorption figure is the only derivable one in the set,
and it is derivable precisely because it is a property of air rather than of a
stage this instrument does not have.

The honest compositional route remains what the Performer step built for: layer
instances, route the same part, select distinct P1–P4 identities, and let the
arranger own timing and placement.

*Not planned, for the third time.*

### Batch two, 1. Hide anisotropy splitting the degenerate pairs

The premise is not true of this tree, and the physics does not say what the
proposal says it says.

**The pairs are already split.** `nonAxisymmetricDetune` puts every
non-axisymmetric doublet a fraction of a per cent apart with a fixed per-entry
hash, so nothing here is degenerate. Measured on the factory ō-daiko, the (1,1)
pair sits at 59.747 and 59.572 Hz — 5.08 cents, a 0.175 Hz beat.

**A grain axis cannot split the pairs the proposal names.** Write the doublet as
*e^{±imθ}*. The off-diagonal element that lifts the degeneracy at first order is
*⟨e^{−imθ}|V|e^{imθ}⟩*, which requires the perturbation to carry an *e^{−2imθ}*
component; the two diagonal elements need only its *p* = 0 part and are equal.
So a perturbation whose angular dependence is *cos(pθ)* splits the *m*-doublet
at first order if and only if *p* = 2*m*. A hide with one stiff direction is
two-fold — *p* = 2 — so it splits *m* = 1 and nothing else at first order;
*m* = 2 and above split at second order in the anisotropy and below. The
proposal's own list, "(1,1), (2,1), (3,1)", is therefore right about the first
entry and wrong about the other two.

**And on the one doublet it can reach, it is smaller than what is already
there.** *E∥* ≈ 4.2 GPa against *E⊥* ≈ 2.8 GPa is the hide's *bending* modulus,
which enters this engine as `stiffnessBatter`, the dimensionless *B* of
`stiffnessStretch`. Measured across the family, *B* runs 2.4e-5 on the shime to
1.5e-4 on the chū-daiko, and what it is worth on the (1,1) mode — the only one a
two-fold axis splits at first order — is **0.19 to 1.18 cents**:

| drum | *B* | bending on (1,1) | bending at the top of the bank |
|---|---|---|---|
| ō-daiko | 7.15e-5 | 0.55 cents | 10.1 cents |
| chū-daiko | 1.53e-4 | 1.18 cents | 21.4 cents |
| okedo | 6.53e-5 | 0.50 cents | 9.2 cents |
| shime | 2.42e-5 | 0.19 cents | 3.4 cents |

Those quoted moduli are a fractional anisotropy of about 0.2, and the first-order
split cannot exceed that fraction of the bending contribution, so the largest
grain split available on a family instrument is around **0.24 cents** — against
the **5.08 cents** the engine's own irregularity model already applies to the
same pair. A listener would hear the existing split and not the grain. (The
corner where bending is large — a 15 cm head at full tension in the thickest
hide, where *B* reaches 2.4e-2 and bending is worth 151 cents on the (1,1) mode
— is the same corner that is not an instrument.) The split the proposal actually
describes would have to come from anisotropic *tension*, which the quoted moduli
say nothing about, and a hide creeps towards uniform tension as it is mounted
and pulled.

**And the orientation is already adjudicated.** "Split-mode orientation
boundary" rejected a hashed global principal axis after building it — it removed
all silent partners and moved the sounding anchor by five cents — on the ground
that a global material axis and independent random irregularity are two
different head models, and nothing in the present scalar controls distinguishes
them. Resolving that needs the fixed-radius ring of 17 to 32 LDV azimuths the
capture contract specifies. Naming the axis "grain" does not identify it.

*Not planned. Same gate as before, now with the selection rule written down.*

### Batch two, 2. Non-linear inter-modal energy cascade

This is gap 4, "Letting the head's own stretching pump the continuum", struck in
review and corrected once since. The mechanism is not in doubt — the engine
already carries the Berger/von Kármán term as the attack glide, driven from the
area-mean squared slopes of every resolved batter-head mode — but what the
proposal wants is energy transported *into* the unresolved bands, and that is
gated on gap 2: each continuum band is the difference of two doubled one-poles
and the crossover band is louder than every band above it in its own octave, so
relighting bands 2 through 5 changes nothing a listener reaches. The gate is
still shut.

*Struck, unchanged, and behind a gate that is somebody else's step.*

### Batch two, 3. The bachi as a flexural resonator on rim strikes

This is gap 6, and it is the most thoroughly adjudicated entry in this document:
implemented twice, independently, both times working and both times far too
loud, with the second attempt raising a plain Don by 15.06 dB across 500 Hz to
4.4 kHz and driving five strokes at octave −2 into the safety limiter. The
level is `stickCalibration · radiatingArea / (modalMass · ω)` and
`stickCalibration` is free: it can be moved six decibels without changing the
one stroke it was calibrated by, anywhere that stroke is the stick.

Two corrections to the proposal's framing. The frequencies it quotes — 500,
1370, 2690, 4450 Hz — are `resolveStickFor`'s output at the factory Bachi
Hardness, and that function is not in this tree at all: it went when the
stick-against-stick stroke left the grid, rather than sit unreachable. And "when
striking the wooden hoop (Ka or Don Rim)" is wrong about Ka, which has
`strikesHoop` false and drives the ring modes not at all —
`testOnlyTheHoopStrikeDrivesTheShell` requires exactly that. Ka's `rimGain` of
0.30 reaches the continuum's edge boost and the tack line, which are the head
and its iron, not the body.

The gate the earlier entry identified is unchanged and is worth restating
because it is the one derivable thing in the area: every membrane mode radiates
through `radiationEfficiency(order, ka)` and the wooden bank does not, taking a
bare `radiatingArea` instead. A 12 mm bar at 497.71 Hz has *ka* = 0.109, which
that function puts 19.3 dB down, against 3.1 dB down at 4446 Hz — a 16 dB tilt
across exactly the region the step measures. Until the wooden bank radiates
through the same law the head does, the striker has a spectrum and no level.

*Struck. Its gate is a change to the shell as well as the stick and belongs to a
pass that owns the shell revoice.*

### Batch two, 4. A press roll solved from the bounce

Already adjudicated under "A press roll solved from the bounce rather than
written down", and the ground has moved under it since: the Buzz stroke is no
longer on the playing grid. All four strokes carry `contacts = 1`, so there is
no scripted 19 ms × 0.82^k train left in the engine to replace. The wrist
oscillator the proposal writes down would be a new articulation, and it would
need a press force and a bounce restitution the model does not have — the
restitution it does carry, 0.42, is the impulsive figure for a bachi meeting a
head and collapses a roll inside twenty milliseconds.

*Not planned. The thing it proposed to fix no longer exists.*

### Batch two, 5. Frequency-dependent directivity and shell shadowing

Split in three, and each third has a different answer.

The *ka* law for the head is already the engine's central radiation term:
`radiationEfficiency(order, ka)` gives a mode of circumferential order *m* an
efficiency rising as *(ka)^(2m+2)* until *ka* reaches the mode's own order.
That is what makes a centre strike a boom and an edge strike a slap, and it is
not missing.

The complex directivity — real polar lobes rather than a scalar efficiency —
is built, tested and behind a false release gate, for three stated reasons that
are unchanged: changing the observation changes which partial the octave solve
is heard at, Pitch automation can move a ringing bank two octaves while a built
radiation residue is frequency-dependent, and no owned capture separates batter
radiation from the rear head, shell, capsule and room.

Shadowing by the body is absent, and so is the neighbouring term that gap 6
already named as its gate: the wooden bank does not radiate through any *ka* law
at all, taking a bare `radiatingArea` where every membrane mode goes through
`radiationEfficiency(order, ka)`. The two are not the same thing — one is
diffraction around the shell on the way to a capsule, the other is how well the
shell's own ring modes couple to air — but they are the same missing physics
seen twice, and the second is computable from what the engine already has.
Fixing it is not free: it moves the shell on all four strokes and
`shellCalibration` would have to be re-pinned, so it belongs to a pass that
takes on the shell, with the mono/side revoice "Local-palm and shell prototype
audit" already identified.

*The first third is shipped, the second is capture-gated, the third is a real
gap already named as somebody's next step.*

### Batch two, 6. Oblique strike angle and shear friction

New to this document. It does not survive.

The normal half is degenerate with a control that already exists: resolving the
impact into *F cos θ* scales the impact speed, and impact speed is exactly what
MIDI velocity sets. A naname-dai stroke at 40° arrives with 77 % of the normal
speed of the same blow struck square, and the engine already reaches every
impact speed between 0.12 and 6 m/s from the keyboard. Nothing in the timbre
would distinguish the two, because contact time follows impact speed as
*v^(−1/5)* down the same path either way.

The tangential half needs three things the engine does not have: a stand angle
(no stand), a friction coefficient for oak on treated cowhide (not in the
model, not in the literature this document has been able to reach), and an
in-plane degree of freedom on the head. Taikor solves transverse displacement
*w* only; a stick-slip scrape is a shear wave in a coordinate that does not
exist here, and adding one is a larger change than the whole of any step in this
document.

*Not planned. One half is a rename of Velocity, the other is a new field
variable plus two drawn constants.*

### Batch two, 7. Climate: temperature and humidity

Already adjudicated under "Temperature and humidity drift", where the sole
blocker is that Ando's AST 33(4) (2012) measurements could not be opened and
"the size of the shift is the whole content of the step". That is still the
blocker. The paper was located this session —
["Resonance frequency changes of Japanese drum (nagado daiko) diaphragms due to
temperature, humidity, and aging"](https://www.jstage.jst.go.jp/article/ast/33/4/33_E1209/_article),
*Acoustical Science and Technology* 33(4), 277–278 — and J-STAGE is not
reachable from this environment, directly or through the DOI.

The proposal supplies its own figures instead: 20 % off Young's modulus above
75 % RH, and a rise in *tan δ*. Implementing from those would be inventing the
measurement the step exists to carry, which is the same reason it was left out
the first time. Note also that the *mechanism* the proposal names is the wrong
one to reach for first: on a tension-dominated head, *E* enters through the
bending term, and what humidity actually moves on a nagadō is the tension, via
the hide's length and the tacked rim.

*Still the best cheap step nobody can take. It unblocks the moment the paper is
in hand.*

### Batch two, 10. A 3D modal displacement visualiser

Withdrawn by the requester during this pass.

### What changed on the tree

Two claims in the README were wrong and are corrected, and both were found by
validating proposal 4 rather than by looking for them.

**Body Depth does move the decay.** The README said "Body Depth moves the pitch
of the split and never the decay of either branch". Measured on the factory
ō-daiko, the breathing branch's T60 runs 0.8927 s at Body Depth 0, 1.0163 s at
0.5 and 0.9408 s at 1.0 — a 13.9 % spread, and not monotone, because radiation
falls as the branch comes down in frequency while the mounting loss rises
towards its corner. The lower branch moves 1.5 %. What the model lacks is a loss
belonging to the *air*, which is a much narrower statement.

**The cavity loss factor was quoted an order of magnitude low, against the wrong
term.** The README gave "around 1e-4 … three orders of magnitude under what
radiation is already taking out of the same mode". Kirchhoff's result on the
resolved drums is 2.8e-4 to 6.6e-4, and on the factory ō-daiko's lower branch —
where the loss factor is largest — radiation is 0.7 % of the loss and the
mounting is 92.6 %, so radiation was not what the comparison should have been
against. The conclusion survives: 1.49 % of the decay at worst, anywhere.

Two guards were added, each recomputing its finding from the resolved drum
rather than remembering a figure, on the precedent of
`testTheStickBankIsOnlyCalibratedAtTheBottomOfTheKeyboard`:

- `testTheEnclosedAirIsLosslessOnlyWhereThatIsInaudible` scans four octaves ×
  eleven Body Depths × three Air Couplings, requires the cavity's own
  thermoviscous loss to stay under 2 % of every axisymmetric branch's decay,
  requires the scan to actually find a cavity (so the bound cannot pass for the
  wrong reason), and requires Body Depth to keep its authority over the
  breathing branch's tail.
- `testTheContactPatchWouldNotBeAudibleOnTheResolvedBank` rebuilds the Hertz
  patch from the engine's contact solve, requires the spatial low-pass to stay
  under 0.1 dB on the four family instruments, and requires the answer to still
  halve when the assumed tip radius halves — because that sensitivity is the
  finding.

No rendered audio moves: nothing in the signal path was touched, and the
committed demonstration takes under `Docs/audio` are unchanged.

### What a future pass should take from this

Ranked by what is actually blocking each one, rather than by claimed impact:

1. **The wooden bank radiating through `radiationEfficiency`.** The only term
   named anywhere in these fifteen that the engine is missing, could compute
   from what it already carries, *and* would be audible. (The cavity's
   thermoviscous loss meets the first two and fails the third by two orders of
   magnitude.) It gates gap 6 and it is half of batch two's fifth proposal. It
   costs a re-pin of `shellCalibration` and a shell revoice.
2. **The stand.** It unblocks the floor image, which is the highest-impact
   proposal in either batch, and it is currently an absence finding with no
   numbers at all.
3. **Ando 2012.** One two-page letter stands between this instrument and a
   climate control no sample library can have.
4. **The LDV azimuth ring.** It decides between a global material axis and
   random irregularity, and therefore whether the hide has a grain in this
   model.

## The body moves with the pair — 2026-08-19

Mic Distance moved the head's own near field and the head's continuum and left
the wooden shell at one fixed level. Backing the capsules off therefore thinned
the drum around a body that never moved: two thirds of the instrument had a
perspective and the third that gives a taiko its weight did not.

### What the plan's own note asked for, and why it was wrong

The previous section listed "the wooden bank radiating through
`radiationEfficiency`" as the one derivable term the engine was missing. Built
and measured, that construction does not survive.

For the head, `radiationEfficiency(order, ka)` sets the **damping** —
`radiationLoss = radiationPrefactor * efficiency` — while the **observation** is
a separate near-field-plus-propagating split. The note asked for a far-field
power law to be applied to a close-mic *observation*, which is the wrong term in
the wrong place. The pair sits 15.95 cm from the head and about 40 cm from the
wall, deep in the shell's near field, where a low ring mode's multipole
cancellation is nowhere near complete.

The arithmetic makes the size of the error plain. On the factory ō-daiko the law
puts the *n* = 2 ring mode **27.4 dB** down, and that mode carries almost all of
the bank's energy — its `|drive · micLeft|` is 6.02e-05 against 1.29e-05 for
*n* = 3 and it has the slowest decay of the six. Holding the Don Rim's wooden
energy across the change therefore needs a **29.5×** re-pin of
`shellCalibration`, which leaves *n* = 4 through 7 — modes the law barely
touches — nearly thirty times louder than they were. The body stops being a
thump and becomes a mid ring. The evanescent reading of that same mode is about
6 dB down, not 27.

Applying it to the damping instead is not available either: the shell's decay is
a drawn *Q* of `12 + 40 · shellMaterial`, a lumped stand-in that already
contains whatever radiation the body does. Adding a derived radiation loss on
top would double-count it, and separating the two needs a measured body
mobility.

### What shipped instead

A ring mode is now read the way a membrane mode is. Its shape around the body is
`cos(n theta)`, so its spatial wavenumber is *n/R* and `nearFieldAttenuation`
takes the ring order where a membrane mode hands it a Bessel zero; the path is
from the wall to the capsule rather than from the head to it, because that is
where the wood is. On top of that evanescent term sit the same `proximityLift`
and propagating share the head carries, so the body and the head answer the pair
through one construction rather than two.

The result is taken as a **ratio against this drum's own capsule distance at the
factory Mic Distance**, the same arrangement the continuum's perspective law
uses. Per drum, not against one fixed distance: the capsules are scaled
proportionally closer to the small heads, so a single reference has the okedo
and the shime reading their bodies from a position the pair never occupies.

Measured, in decibels relative to each drum's factory position:

| drum | ring | frequency | Mic 0.00 | Mic 0.35 | Mic 1.00 |
|---|---|---|---|---|---|
| ō-daiko | 2 | 96.59 Hz | +2.46 | 0.000000 | −3.62 |
| | 7 | 1710.55 Hz | +0.96 | 0.000000 | −0.56 |
| chū-daiko | 2 | 176.96 Hz | +3.30 | 0.000000 | −7.46 |
| okedo | 2 | 191.58 Hz | +6.52 | 0.000000 | −15.95 |
| | 3 | 541.87 Hz | +6.61 | 0.000000 | −18.32 |
| | 5 | 1680.26 Hz | +0.09 | 0.000000 | −0.05 |
| shime | 2 | 529.48 Hz | +5.13 | 0.000000 | −13.28 |

The factory column is exactly zero on every drum and every ring mode, which is
what keeps `shellCalibration` meaning what it was pinned to mean and leaves
every factory preset rendering as it did. The low ring modes move far more than
the high ones, and that is the physics rather than a taper: the evanescent rate
is `sqrt((n/R)^2 - (w/c)^2)`, and a ring mode's frequency climbs as about *n²*
while its wavenumber climbs as *n*, so the high modes are already propagating
and barely care where the pair stands. The light stave okedo moves most, which
is the drum whose body is loudest against its own head.

### The verdict was taken by ear

Five candidates were rendered against the shipping engine as an A–F listening
test under the convention added to the repository's `CLAUDE.md` in the same
pass: Don Rim at three Mic Distances on the ō-daiko and the okedo, level-matched
on the factory-position sections, letters carrying no hint of the mechanism.

| letter | what it was | outcome |
|---|---|---|
| A | shipping engine | baseline |
| B | perspective, normalised per drum | **chosen** |
| C | `radiationEfficiency` on the level, re-pinned 29.5× | not chosen |
| D | perspective, normalised against one fixed 15.95 cm | not chosen; peak 0.985, past the suite's 0.95 clause |
| E | `radiationEfficiency` on the level, un-re-pinned | not chosen; the body nearly disappears |
| F | perspective, near field only | not chosen |

The user chose **B**. This is recorded as a choice made by ear: the measurements
rule out C, D and E on their own terms — C needs a 29.5× fitted re-pin, D breaks
a suite clause, E is uncalibrated — but nothing measurable separates B from F,
which differ only in whether the propagating share the head carries reaches the
body as well. B keeps the construction identical to the head's; that it also
sounds right is the listener's finding, not a derivation.

### What guards it

`testTheBodyMovesWithThePair` holds four clauses, each recomputed from the
resolved drum:

1. the factory Mic Distance moves nothing, on every drum and every ring mode —
   reverting the per-drum reference to a single fixed one fails this on the
   okedo at gains of 1.22, 1.25 and 1.13;
2. every other distance does move it, monotonically, closer being louder;
3. the highest ring mode falls off with distance at less than half the rate of
   the lowest, which is the evanescent signature rather than a taper;
4. a rim shot's rendered wooden bank recedes as the pair backs off, and a
   head-only stroke still has no wooden bank at all.

Making the gain a constant 1 fails clauses 2, 3 and 4.

### Cost, and what moved

`shellPerspectiveGain` is evaluated six times per voice build, never in the
render loop. Two expressions that `resolveDrumFor` held inline — the close-mic
proximity shelf and the size scaling that puts the pair proportionally closer to
a small head — were extracted to `micProximityFor` and `micDistanceSizeScale`,
because the shell's perspective has to rebuild the pair's factory position from
the drum alone. Both produce the values they produced before.

**Twenty-six of the twenty-seven demonstration takes are bit-identical.** The
twenty-seventh is `21-mic-distance.wav`, and it changed because it could not
show the change: it swept Mic Distance on a **Ka**, which does not touch the
hoop, so its wooden bank is silent and the take that exists to demonstrate the
control could only ever show two thirds of it. It now takes the same five
positions twice, on a Ka and then on a Don Rim. 4.5 s to 8.5 s, −11.8 to
−8.6 dBFS peak.

### Still open

The shell's *damping* remains a drawn `Q`, so the body's decay does not know
what it radiates; separating material, mounting and radiation losses in it needs
the body mobility on the capture contract. The wooden bank also still has no
angular doublet — the legacy common pickup with a 0.20 spread stands, and the
prototype that replaced it was rejected in "Local-palm and shell prototype
audit" for inverting wide-pair rim cases. Neither is touched here: this pass
gives the body a perspective, not a directivity.
