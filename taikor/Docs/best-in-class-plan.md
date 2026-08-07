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
  *Verified by*: the ratio of the highest resolved membrane mode to the lowest
  rises with Head Material and rises as the drum gets smaller, and the octave
  contract and the reported ideal fundamental are untouched.

- [ ] **2. Delete the shell gain step disguised as saturation.** Closes gap 4a.
  *Verified by*: a test that sweeps Shell Resonance across 0.01 and requires the
  rendered Katsu level to be continuous, which fails by 1.59 dB today.

- [ ] **3. Derive the attack pitch glide from the head's own stretching.**
  Replace the fixed envelope and the `modelScale`-dependent energy term with a
  von Kármán/Berger tension rise: the mean tension a clamped membrane gains from
  a transverse displacement `w` is `ΔT/T ∝ (E h / T(1−ν²))·(w/a)²`, and `w` is
  recovered from the resonator states in metres by dividing out the calibration
  the drive carries. The glide then decays with the head rather than on a
  115 ms clock, falls as tension rises, and no longer depends on the output
  calibration. Closes gap 2.
  *Verified by*: the applied tension shift after a hard stroke must fall when
  Head Tension is raised at constant velocity and rise with a thicker head,
  neither of which is true today; and it must still rise with velocity.

- [ ] **4. Let a stroke damp the head it lands on.** A bachi meeting a ringing
  membrane is a mass arriving on a moving surface: with restitution `e` it
  removes the fraction `(1−e²)·ψ²·m/(M+m)` of what is moving, where `ψ` is the
  mode shape at the new strike point, `m` the stick's mass and `M` the head's
  modal mass. Applied over the contact duration to the membrane modes and the
  continuum of every voice still sounding on the same drum. Closes gap 3.
  *Verified by*: eight identical strokes rendered by the engine must be
  measurably quieter than eight copies of one stroke summed offline, while the
  first stroke's own attack is bit-identical.

- [ ] **5. Give the tack line a real voice.** Replace the inaudible chatter with
  byō rattle that scales with the rim contact force and with the Stick Noise
  control, band-limited around the tacks' own ring rather than added broadband.
  Closes gap 4b.
  *Verified by*: the rattle must vanish at Stick Noise 0, must grow with
  velocity, and must appear on Don Rim and Katsu but not on a Don.

- [ ] **6. Open the bottom of the dynamic range.** Lower the minimum impact
  speed from 0.45 m/s to 0.12 m/s and drop the squaring, so velocity is even in
  decibels across its range instead of piling the bottom half onto the floor.
  The top of the range is left exactly where it is, so the loudest stroke the
  instrument can make does not move and the factory Output stays correct.
  Closes gap 5.
  *Verified by*: the peak level span from velocity 0.02 to 1.00 at full Velocity
  Depth must exceed 34 dB, and the loudest stroke must not get louder.

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

**The enclosed air as a distributed column.** The cavity is a lumped spring,
`ρc²/L`, which is only valid below the body's first axial air resonance —
`c/2L` is 212 Hz on the factory drum and moves from 139 Hz to 451 Hz across Body
Depth, so the assumption fails inside the drum's own range. Solving it properly
turns the two-by-two axisymmetric eigenproblem into a three-by-three with the
air column as its own degree of freedom, and touches the render path, the
readout and the tail sweep together. It is the best remaining physics in this
instrument and it is too large to land safely alongside the six steps above.

**Demonstration audio.** Steps 1, 3, 4 and 6 all move levels, so the committed
takes under `Docs/audio/` and the level table in their manifest are stale until
they are re-rendered. They are re-rendered centrally rather than here. No new
take was added for any step, because the number of takes is asserted in shared
CI workflow files that this work may not edit.
