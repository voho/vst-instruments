# Drumalor best-in-class plan

This document states what Drumalor is measured against, where it actually
stands against that today, and the ordered set of changes that closes the
difference. It is a working contract, not a release note: each step names the
gap it closes and the measurement that proves it closed. Steps are ticked off
as they land.

## What Drumalor is competing with

Drumalor sits between two markets that are usually reviewed separately, and it
is judged by both.

**Modelled acoustic kits.** IK Multimedia's MODO DRUM (about £250) is the only
mainstream *physically modelled* acoustic drum instrument and is therefore
Drumalor's closest competitor by method rather than by price. Its advertised
modelled controls are shell profile, diameter and height; resonant head type,
tuning and damping; **where on the head the strike lands**; room, overhead and
bleed mic editing; and **sympathetic vibration between the kick, snare and
toms**.[^modo][^modo-sos][^modo-mr] Its 1.5 revision added kits, MIDI grooves
and a dedicated mixer, which is what reviewers most often asked for.

**Sampled acoustic kits.** Toontrack's Superior Drummer 3 (about €400, a 230 GB
library) is the realism benchmark the whole category is measured against, and
its selling points are the ones a synthesist has to answer: velocity depth,
round robins, articulations, and mixing.[^sd3] Toontrack's own material is
explicit that leakage is not an artefact to be removed: "natural leakage,
called bleed, is one of the key elements that gives a studio drum recording its
sense of cohesion and realism", and "toms ringing sympathetically when the
snare is hit creates the natural resonance that ties the kit together".[^sd3-bleed]

**Modal and physical-modelling percussion synths.** AAS Chromaphone 3 (about
$199) builds instruments from eight modelled resonators - string, open tube,
closed tube, plate, drumhead, bar, marimba bar, membrane - which can be run in
parallel or in a *coupled* mode where "modeling takes account of complex
interactions between the two resonators"; reviewers note its CPU
appetite.[^chromaphone][^chromaphone-manual] Physical Audio's Modus and Tetrad
are the research end of the same field, selling nonlinear collision behaviour
"from subtle pitch glides to high energy crescendos".[^modus]

**Synthesised drum machines.** Sonic Charge Microtonic (about $99) and D16's
Nepheton/Drumazon/Punchbox are the analogue-machine end: no samples, circuit or
behavioural models, and very low CPU - Microtonic advertises under 5 % on a
low-end machine.[^microtonic] This is the part of the market Drumalor's metallic
relaxation-oscillator banks and its circuit output stages already answer well.

### What reviewers and users say separates the top tier

- **Repetition.** The standing complaint against sampled kits is the
  "machine-gun effect": the same waveform on every sixteenth. The sampled
  answer is velocity layers plus round robins, and users still report kits that
  "sound like a machinegun" even with four hits at four levels.[^rr-kvr][^rr-fa]
  A modelled instrument's answer is supposed to be that no two strikes are the
  same event in the first place - which is only true if something physical
  actually differs between them.
- **Hi-hat pedal continuity.** Electronic kits send pedal position as MIDI
  CC 4, 127 down to 1 from tightly closed to fully open, and the expectation is
  that "as the pedal is loosened, the sound of hats changes their timbre, decay,
  and character".[^cc4-toontrack][^cc4-handy][^cc4-hydrogen] Two discrete notes
  with a choke link is the low-tier behaviour.
- **Articulations.** The Roland/Toontrack de-facto convention for a snare is
  note 38 head, 40 rimshot, 37 cross-stick. A snare with one articulation is
  not a snare a drummer can play.
- **Bleed and shared space.** See the Toontrack quotation above; MODO DRUM
  sells the same thing as a modelled feature.
- **Snare-wire behaviour.** Wire buzz excited by *other* drums is a recognised,
  separately sold effect.[^snarebuzz]

### Literature the remaining gaps point at

- Bilbao, *Time domain simulation and sound synthesis for the snare drum*
  (JASA 131(1), 2012) and Bilbao, Torin & Chatziioannou, *Numerical modeling of
  collisions in musical instruments*, for the snare-wire/membrane collision as
  a displacement-gated contact rather than an envelope.[^bilbao-snare][^bilbao-collision]
- Avanzini & Marogna, *Efficient synthesis of tension modulation in strings and
  membranes based on energy estimation* (JASA 131(1), 2012), and the DAFx-10
  paper behind it: the short-time average tension change responsible for pitch
  glide is approximately proportional to the system *energy*, so a modal engine
  can carry it "with only slightly more computational resources than linear
  models".[^tension-jasa][^tension-dafx]
- Torin & Bilbao's snare-membrane collision in modal form, for doing the same
  in a modal rather than finite-difference engine.[^snare-modal]

## Where Drumalor actually stands

Drumalor's modal and circuit layers are genuinely competitive, and in places
ahead: the two-head air-coupling split, the radiation-order damping law, the
Hertzian contact spectrum, the displacement-gated snare wires, the fixed-grid
noise density, the frozen-bank efficiency work and the ADAA antiderivative
repair are all things the commercial field either does not do or does not
document. The gaps are not in the physics it has. They are in the physics it
declines to use.

1. **The strike always lands in exactly the same place.** `buildHeadBank()`
   already weights each mode by `J_m(lambda_mn * r/a)`, which is the correct and
   complete account of where a stick landed - and it is then handed a
   per-instrument literal that never moves: 0.18 for the Kick, 0.36 for the
   Snare, 0.34 for every Tom. Every kick in a session is struck at exactly 18 %
   of the head radius, forever. This is MODO DRUM's headline modelled control,
   and Drumalor has the entire mechanism already built and wired to a constant.

2. **There is no hi-hat pedal.** Closed Hat and Open Hat are two independent
   voices on two GM notes sharing choke group A. There is no half-open hat, no
   progressive closing of a ringing open hat, and no foot chick. An e-kit's CC 4
   stream is discarded by `dispatchMidiData()`. The README already describes the
   correct physics - clamping stiffens the pair and damps it by friction between
   two faces rather than by anything inside the bronze - and then exposes it as
   two fixed points on a continuum.

3. **Nothing in the kit can hear anything else.** Thirteen voices are summed
   into a stereo pair and that is the whole of the coupling. A kick does not
   buzz the snare wires; a snare does not ring the toms. The engine has no
   object that survives between voices except the metallic oscillator banks.

4. **Humanise moves parameters, not the strike.** Its seven deviations - pitch,
   decay, character A and B, transient energy, circuit drive, bias - are all
   jitter applied to *controls*. The one thing that genuinely differs between
   two strokes of a real drummer, which spot on the head the stick found and
   therefore which modes it fed, is fixed. That is why the existing organic
   contract can only assert that six equal strikes "differ", and cannot assert
   that they differ in the way a player's do.

5. **Tension modulation reaches one oscillator.** `renderTom()` bends
   `phaseIncrements[0]` by an `envelope`-squared term; the twelve-mode head bank
   behind it is linear, and so are the Kick's and Snare's. The Kick's *body*
   resonator has an amplitude term, but it is gated on `characterB`, so a Kick
   with Drive at zero has no amplitude-dependent pitch at all. The literature
   result above says this belongs on the bank and can be afforded.

6. **A re-struck head becomes a second drum.** `findVoiceSlot()` returns the
   first inactive slot, so striking a ringing tom allocates a fresh voice and
   the old one keeps ringing untouched. A flam is therefore two independent
   membranes summed, and a buzz roll is thirty. `Resonator::strike()` was
   written to superpose into existing state, and nothing outside its own voice
   ever uses that.

7. **The snare has one articulation.** Note 40 is an alias for the same head
   hit; note 37 is an alias for the claves. There is no rimshot and no
   cross-stick, and both are strike-position and contact-time variants of a
   model Drumalor already has.

Two further things are worth naming and not fixing here. Drumalor has no room
or overhead path at all, which is a genuine difference from every commercial
competitor; a synthesised early-reflection network is defensible under the
no-assets rule but is a larger piece of work than this pass, and inter-voice
coupling is the part of it that carries most of the perceived "one kit"
quality. And there is no velocity-curve control; that is a workflow gap, not a
modelling one.

## Steps

Each step is one commit, self-contained, with the JUCE-free DSP build and its
regression suite green.

- [x] **1. Withdrawn: strike position as a humanisation and velocity axis.**
  Planned as a per-hit strike position moved by velocity and scattered by
  Humanise. Built, measured, and dropped, because the measurement did not
  support it. On a level-matched null test between a full-velocity and a
  quarter-velocity strike, moving the aim across the whole realistic range a
  drummer covers - from `r/a = 0.14` at an accent to `0.54` at a ghost stroke -
  changed the residual by 1.5 dB on the Snare and by 0.2 dB or less on the Kick
  and all three Toms, against a residual of about -8 dB that the existing
  velocity terms already produce. Per-hit scatter at Humanise 1.0 moved the
  hit-to-hit residual by 0.1 to 0.4 dB against the -10 dB the other six
  deviations already reach.
  The reason is structural rather than a tuning failure: `buildHeadBank()`
  normalises the bank's gains to a constant sum and then tilts them by
  `ratio^(-0.8)` and weights them by radiated efficiency, so three or four
  low-order modes carry almost all of the bank's energy - and those are exactly
  the modes whose `J_m(lambda r/a)` is least sensitive to `r`, because near the
  centre a mode of order `m` scales as `r^m`. The strike position therefore
  redistributes energy almost entirely among modes that are already 20 dB down.
  Making it audible would mean re-voicing the bank's tilt and radiation
  weighting, which is a change that could only be justified by ear, so it is not
  made here. The mechanism itself is kept and used where the position change is
  large enough to matter on its own: a rimshot lands at the rim, and that is
  step 3.

- [x] **2. Tension modulation on the whole membrane bank.** Carry an energy
  estimate for each head bank and detune every mode by a bounded, energy
  proportional amount, following Avanzini and Marogna. Implemented as a
  first-order update of each resonator's `a1` around its nominal value, which
  leaves the pole radius, and therefore the decay time, exactly untouched.
  *Closes gap 5.* Landed. Verified by a contract that tracks the dominant head
  partial of the Kick and all three Toms: at full velocity it sits 96, 30, 134
  and 250 cents above the same partial at a ghost stroke, where the engine
  immediately before this model measured -73, -69, +15 and +108 cents, none of
  which was tension. The toms are additionally required to settle at the same
  pitch once the energy has gone, and to hold their decay range at both ends of
  Skin, which is what proves only `a1` moves.

- [x] **3. Snare rimshot and cross-stick.** Add articulations to the trigger
  path and map them to the standard notes: 38 head, 40 rimshot, 37 cross-stick.
  A rimshot is the same snare struck at 0.92 of the radius with the shortest
  contact in the kit and the wires fully engaged; a cross-stick is a stick laid
  across a hand-damped head with the shaft struck on the rim, so the membrane is
  heavily loaded, the wires stay down and what radiates is the shell.
  *Closes gap 7.* Landed. Verified by a contract on the mapping and on the
  sound: the rimshot engages the wires 1.2 times harder and reaches 1.3 times
  further up the head's series at the strike, the cross-stick drops the wire
  band to a fifth and its tail to under a third of a head strike's, and neither
  nulls against a peak-matched head hit - the residuals are -8.8 dB and -0.7 dB
  where both used to be exactly minus infinity, because notes 40 and 37 were
  second names for a plain snare and for the claves. No new parameter and no new
  voice: a strike position and a contact time are what the head bank is already
  built from.

- [x] **4. A stick landing on a moving head takes energy out of it.** When a
  membrane voice is struck while a voice of the same instrument is still
  ringing, remove a velocity-scaled share of the ringing bank's modal state and
  envelopes instead of letting two independent drums sum.
  *Closes gap 6.* Landed. Verified by a contract that a 15 ms flam on each of
  the five membranes carries less energy over the following 220 ms than the two
  strokes rendered separately do, that a ghost stroke leaves more of the ringing
  head alive than a full stroke does, that a forty-eight-stroke press roll stays
  bounded and holds well under half as many voices as it has strokes, and that a
  second ride strike still adds - a cymbal is not a drum head and is deliberately
  left out of this.

- [x] **5. A hi-hat pedal that is a pedal.** A continuous aperture drives the
  plate model: clamping raises the stiffness and replaces the plate's own
  frequency-dependent loss with friction that takes every partial alike, and it
  interpolates the decay geometrically between the two voices' own decay
  settings. MIDI CC 4 sets it, sample-accurately; closing on a ringing hat damps
  it progressively; a fast close emits a foot chick.
  *Closes gap 2.* Landed, with one claim dropped. A fully closed pedal
  reproduces the Closed Hat and a fully open one the Open Hat sample for sample;
  five pedal positions each ring shorter than the last and each differs from its
  neighbour by more than a level change; closing the pedal on a ringing open hat
  damps it and shutting it cuts it; a fast close makes a foot chick while a lift
  or a rest makes none; reset releases the pedal. The plan also expected the hat
  to darken monotonically as the pedal closes, and it does not: the top of a
  Drumalor hat comes from the free-running Schmitt bank through fixed filters
  rather than from the plate bank, so the measured 8-16 kHz to 2-6 kHz ratio
  stays within 3.03 to 3.11 across the whole travel. Making that claim true
  would mean moving the hiss path's corners with the pedal, which is a voicing
  decision that could only be justified by ear, so the contract asserts what is
  actually modelled instead.

- [ ] **6. The kit hears itself.** A persistent snare bed - the resonant head
  and its wires - driven by a one-sample-delayed copy of the kit mix, gated on
  the bed's own displacement by the same contact law `renderSnare()` uses, plus
  sympathetic ring in the tom shells. Exposed as one kit control, `Kit Bleed`,
  defaulting to zero so every existing session and every existing contract is
  bit-identical.
  *Closes gap 3.* Verified by: at zero the output is bit-identical to the
  current engine; above zero a kick alone produces measurable energy in the
  snare's wire band that a kick alone cannot produce today; the path is strictly
  feed-forward and stays bounded at every rate and block partitioning.

## Claims boundary

These are behavioural models with named mechanisms, not calibrated captures.
The mode ratios, geometries, damping shares and coupling depths are physically
derived where a derivation exists and voiced where it does not, and none of them
is fitted to a measured drum. Nothing in this plan introduces a sample, an
impulse response, a neural weight or a third-party preset. Listening comparison
against the instruments named above, and profiling on the oldest supported Mac,
remain part of release qualification regardless of what the automated contracts
report.

[^modo]: IK Multimedia, MODO DRUM 1.5 product page. <https://www.ikmultimedia.com/products/mododrum/index.php>
[^modo-sos]: Sound On Sound, *IK Multimedia MODO Drum*. <https://www.soundonsound.com/reviews/ik-multimedia-modo-drum>
[^modo-mr]: MusicRadar, *IK Multimedia MODO Drum 1.5 review*. <https://www.musicradar.com/reviews/ik-multimedia-modo-drum-15>
[^sd3]: Toontrack, Superior Drummer 3. <https://www.toontrack.com/product/superior-drummer-3/>
[^sd3-bleed]: Toontrack, *How to achieve a realistic programmed drum sound with bleed in Superior Drummer 3*. <https://www.toontrack.com/blog/how-to-achieve-a-realistic-programmed-drum-sound-with-bleed-in-superior-drummer-3/>
[^chromaphone]: Applied Acoustics Systems, Chromaphone 3. <https://www.applied-acoustics.com/chromaphone-3/>
[^chromaphone-manual]: AAS Chromaphone 3 user manual. <https://www.applied-acoustics.com/chromaphone-3/manual/>
[^modus]: Physical Audio, Modus. <https://physicalaudio.co.uk/products/modus/>
[^microtonic]: Sonic Charge, Microtonic. <https://soniccharge.com/microtonic>
[^rr-kvr]: KVR Audio, *Drum multi-samples: round robin vs. velocities*. <https://www.kvraudio.com/forum/viewtopic.php?t=579176>
[^rr-fa]: Fractal Audio forum, *Best drum plugin with round robin?* <https://forum.fractalaudio.com/threads/best-drum-plugin-with-round-robin.209680/>
[^cc4-toontrack]: Toontrack forums, *TD3KW, what does MIDI CC#4 of the CY5 hihat do exactly?* <https://www.toontrack.com/forums/topic/td3kw-what-does-midi-cc4-of-the-cy5-hihat-do-exactly/>
[^cc4-handy]: GoranGrooves, *Variable hi-hats in Handy Drums*. <https://library.gorangrooves.com/docs/how-to-use-variable-hi-hats-in-handy-drums/>
[^cc4-hydrogen]: Spaghetti Wires, *Realistic, continuous hi-hat control for Hydrogen*. <http://spaghettiwires.com/blog/2018/12/30/continuous-hh/>
[^snarebuzz]: Waves Factory SnareBuzz, which "simulates the sympathetic resonance produced by the wires of a snare drum when another sound source is playing near". <https://www.pluginboutique.com/product/2-Effects/42-Enhancer/6314-SnareBuzz-2-0>
[^bilbao-snare]: S. Bilbao, *Time domain simulation and sound synthesis for the snare drum*, JASA 131(1), 2012. <https://www.semanticscholar.org/paper/Time-domain-simulation-and-sound-synthesis-for-the-Bilbao/073c0039f2b9f77d9983aa197c4812719b263a6b>
[^bilbao-collision]: S. Bilbao, A. Torin, V. Chatziioannou, *Numerical modeling of collisions in musical instruments*. <https://arxiv.org/pdf/1405.2589>
[^tension-jasa]: F. Avanzini, R. Marogna, *Efficient synthesis of tension modulation in strings and membranes based on energy estimation*, JASA 131(1), 2012. <https://pubmed.ncbi.nlm.nih.gov/22280712/>
[^tension-dafx]: *Energy based synthesis of tension modulation in membranes*, DAFx. <https://www.dafx.de/paper-archive/details.php?id=FF0VCsWcetDbuSbluOmOiQ>
[^snare-modal]: *Simulation of the snare-membrane collision in modal form*, Forum Acusticum 2023. <https://dael.euracoustics.org/confs/fa2023/data/articles/000995.pdf>
</content>
</invoke>
