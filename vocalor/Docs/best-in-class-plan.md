# Making Vocalor best-in-class

This document records what Vocalor is actually competing against, where it
falls short of that field today, and the ordered set of changes that closes the
gap. It is written to be checked: every claim about the current engine names the
file and the constant it comes from, and every step states how it is verified.

The steps are marked off as they land.

## The field

Vocalor is a procedural source-filter vocal and choir synthesizer. That places
it between two commercial markets that do not usually meet.

**Sampled choirs.** This is where most of the money is, and it is the field a
buyer compares Vocalor against when they want "a choir".

| Product | Price | What it is |
| --- | --- | --- |
| [Fluffy Audio Dominus Choir Pro](https://fluffyaudio.com/shop/dominus-choir-pro/) | $649 ([intro $549](https://www.kvraudio.com/news/fluffyaudio-releases-dominus-choir-pro-for-kontakt-with-intro-price-46692)) | Sampled SATB with a syllable builder |
| [EastWest Hollywood Choirs Diamond](https://www.soundsonline.com/vocals/hollywood-choirs) | $599 | 59 GB multisampled male/female choirs plus WordBuilder |
| [Strezov Sampling Wotan / Freyja](https://www.strezov-sampling.com/products/view/FREYJA-Female-Choir.html) | $329 each | Male and female choirs, phrase and syllable engines |
| [Spitfire Eric Whitacre Choir](https://www.spitfireaudio.com/eric-whitacre-choir) | ~£399 | One conductor's own choir, vowel-morphing legato |
| [Cinesamples Voxos](https://www.soundonsound.com/reviews/cinesamples-voxos-epic-virtual-choirs) | — | 40 adult singers plus boys' choir, multi-mic |
| [Audio Imperia Chorus](https://www.audioimperia.com/product/chorus/) | — | Cinematic choir with a fixed articulation set |

**Singing synthesis.** This is the field Vocalor's *method* belongs to.
[Dreamtonics Synthesizer V Studio 2 Pro](https://store.dreamtonics.com/product/synthesizer-v-studio-2-pro/)
sells the editor plus voice databases at $79 each;
[VOCALOID 6](https://en.wikipedia.org/wiki/Vocaloid_6) is $225 with fourteen
voices; [Emvoice One](https://emvoiceapp.com/) runs as a plug-in inside the DAW.
All three are note-and-lyric sequencers built on recorded or learned voice
databases. They sing words. Vocalor does not, and this plan does not try to make
it.

The nearest historical relatives by method are
[VirSyn Cantor](https://en.wikipedia.org/wiki/Cantor_(music_software)) — a
formant synthesizer, no longer updated —
[Cantor Digitalis](https://link.springer.com/article/10.1186/s13636-016-0098-5),
a research chironomic formant synthesizer, and
[NUSofting Dig Vox](https://synthanatomy.com/2026/07/nusofting-dig-vox-vocal-synth-using-physical-modeling-and-formant-synthesis.html),
which pairs formant synthesis with a physically modelled tract. The commercial
formant-synthesis field is thin, which is Vocalor's opportunity: nothing
expensive is doing this well and in real time.

## What reviewers say separates the top tier

Reading the reviews of the sampled choirs, the same four things decide whether a
product is taken seriously.

1. **Continuous dynamics under a controller, and the dynamic has to change the
   timbre.** Every library in the table is played from the mod wheel. The
   Sound On Sound review of the
   [Eric Whitacre Choir](https://www.soundonsound.com/reviews/spitfire-audio-eric-whitacre-choir)
   praises "a distinct characteristic shift as the dynamics are increased,
   especially noticeable on the humming patches", and criticises the dynamics
   for being uneven across the tessitura. Hollywood Choirs is criticised in the
   opposite direction: reviewers note that CC1 drives dynamics and vibrato
   strength together, with no way to separate them. Dynamics is the control the
   instrument lives or dies by.

2. **Legato that is a transition, not a crossfade.** Reviewers describe true
   legato for voice as forced-monophonic behaviour with genuine portamento
   between notes, and single out
   [Hollywood Choirs](https://www.soundonsound.com/reviews/eastwest-hollywood-choirs)
   for legato transitions that are "unnatural-sounding crossfades between
   notes". [Strezov Choir Essentials](https://www.strezov-sampling.com/products/view/choir-essentials.html)
   is praised for legato that survives polyphonic playing with intelligent
   voice leading.

3. **Vowel movement and consonants.** The Eric Whitacre Choir is praised for a
   "seamless switch between vowel sounds" and criticised for having "no
   word-building facilities". Hollywood Choirs' WordBuilder — vowels, pitched
   consonants and unpitched consonants on three separate balance sliders — is
   the feature that justifies its price to a film composer.

4. **Ensemble believability.** The reason a sampled choir sounds like a choir is
   that it was recorded as one. A synthesized ensemble has to earn that from a
   model of what makes many singers on one note different from one singer
   multiplied.

## Where Vocalor stands today

Vocalor's tract and source model are already better than the parameter list
suggests: a Liljencrants-Fant glottal derivative analysed at `prepare()` time and
band-limited into nine mip levels, a peak-normalised parallel formant bank with
cascade-derived amplitudes and alternating polarity, sample-rate-invariant
humanisation with renormalised noise drives, and buffer-split-invariant chunk
alignment. Those are not the problem. The problems are these.

**1. There is no continuous expression input at all.** `dispatchMidiData()` in
`Source/PluginProcessor.cpp` handles exactly three message kinds: note-on,
note-off, and CC 120/123. There is **no pitch bend**, no mod wheel, no
expression, no sustain pedal, no aftertouch. The entire dynamic response of a
note is frozen at note-on: `initialiseVoice()` stores `velocity` once and
`updateVoiceControl()` computes `effort = 0.42 * velocity + 0.58 * tension` from
it forever. The only continuous timbre control is the global Tension knob, which
is a phonation-mode control shared by every sounding voice and does not move the
level. Measured against the field, this is the single largest gap: an instrument
that cannot be played from CC1 is not competing with any of the products above.

**2. The top of the female range is acoustically wrong.** `updateVoiceControl()`
raises F1 by a flat 0.32 % per semitone above A4 (`highTune`). At C6 that puts
the female /u/ F1 at 367 Hz while the fundamental is 1047 Hz — the entire
spectrum sits above the lowest resonance. The soprano literature is unambiguous
about what happens acoustically: raising f0 above F1 causes "a remarkable loss of
acoustic energy and linguistic information along with an abrupt change in the
voice timbre", which is exactly why sopranos raise F1 to track f0 by opening the
jaw ([Joliveau, Wolfe & Smith, *Vocal tract resonances in singing: the soprano
voice*](https://www.phys.unsw.edu.au/~jw/reprints/Joliveauetal.pdf);
[Vos et al., *The Perception of Formant Tuning in Soprano
Voices*](https://pure.royalholloway.ac.uk/ws/files/28187915/VosEtAl_FormTuningJVoicePURE.pdf)).
Vocalor models the fixed-tract consequence and not the singer's response to it,
so its top octave is thin in a way no real soprano is.

**3. Chord mode is tuned like a keyboard, not like a choir.**
`chordMidiForSinger()` builds triads from equal-tempered semitone intervals: a
major third is 400 cents and a fifth 700. A cappella ensembles narrow the major
third toward 386 cents and widen the minor third, because the aligned overtones
are what produce the "ring" of a locked chord
([ChoralNet, *Intonation II*](https://choralnet.org/archives/415533);
[*Equal or non-equal temperament in a capella SATB
singing*](https://www.researchgate.net/publication/6223729_Equal_or_non-equal_temperament_in_a_capella_SATB_singing)).
Vocalor's one-finger triad beats where a choir's would lock.

**4. The ensemble is far too tightly tuned.** `buildSingerIdentities()` gives
each singer `detuneCents = 5.6 * hashFloat(...)`, a uniform ±5.6 cents, about
3.2 cents standard deviation at full Humanize. Jers and Ternström measured
dispersion between real choir singers at 25–30 cents, and listeners were
reported to tolerate a 14-cent standard deviation
([*Perceptual evaluations of voice scatter in unison choir
sounds*](https://www.sciencedirect.com/science/article/abs/pii/S089219970580342X);
[*Analysis of Intonation in Unison Choir
Singing*](https://repositori.upf.edu/server/api/core/bitstreams/15ccc8b2-093e-4c8b-9839-76972057c578/content)).
Vocalor's static scatter is roughly a quarter of the tolerance and a tenth of
what is measured, which is why twelve singers can still read as one thick voice
rather than as a section. The scatter is also purely static: real singers wander
and correct, they do not sit permanently sharp.

**5. The tract has no controllable zero.** The parallel bank sums five two-pole
sections, so its transfer function does have zeros, but they land wherever the
sections happen to cancel — the README documents fighting a 64 dB spurious notch
that came from exactly this. There is no way to *place* one. That rules out the
nasal branch, and therefore rules out a hum: a nasalised vowel splits F1 into a
pole-zero-pole with a nasal pole near 300 Hz and the zero between it and F1
([Klatt synthesizer parameters,
Berkeley](https://linguistics.berkeley.edu/plab/guestwiki/index.php?title=Klatt_Synthesizer_Parameters);
[Weenink, *The KlattGrid speech
synthesizer*](https://www.isca-archive.org/interspeech_2009/weenink09_interspeech.pdf)).
"Mm" is the most common choir colour after "ah" and Vocalor cannot make it.

**6. Vowel transitions are instantaneous.** `formantGlide_` is built from time
constants of 16, 9, 5, 4 and 3 ms. A vowel change is therefore over in about
50 ms and all five formants move on the same trigger. Real articulation is
slower and asynchronous — the jaw carries F1, the tongue body carries F2, and
they have different masses. The engine has the mechanism for coarticulation and
uses it as a de-zipper.

**7. The singer's formant is an amplitude trim.** `updateChunkState()` boosts F3
by 12 % and F4 by 6 % of Tension. The real mechanism clusters F3, F4 and F5 into
one reinforced peak at 2.5–3.5 kHz by narrowing the epilaryngeal tube
([*Singer's formant*, Voice Science](https://www.voicescience.org/lexicon/singers-formant/);
[*A Formant Range Profile for Singers*](https://pmc.ncbi.nlm.nih.gov/articles/PMC5409887/)).
Boosting the amplitude of three formants that stay 700 Hz apart does not produce
a cluster, and a cluster is what makes a voice carry.

**8. There are no factory presets.** `getNumPrograms()` returns 1 and
`getProgramName()` returns an empty string. Every product in the table above
opens on something playable.

## Steps

- [x] **1. Continuous performance expression.** Add pitch bend (±2 semitones),
  mod-wheel dynamics (CC 1 and channel pressure), expression (CC 11) and the
  sustain pedal (CC 64) to the MIDI path, and a `dynamics` host parameter that
  the wheel takes over once it has moved. Dynamics is not a fader: it drives the
  level, the glottal source tilt through vocal effort, the aspiration-to-voiced
  ratio, and the vibrato depth, so a pp is breathier and duller as well as
  quieter. Closes gap 1. *Verified:* a dynamics sweep must move the 2–5 kHz band
  by materially more than it moves the broadband level; the breath-to-voiced
  ratio must rise as dynamics falls; a ±2 semitone bend must measure the right
  frequency ratio on a sounding note; the sustain pedal must hold a note through
  its note-off and release it on pedal-up; expression must be a pure level trim
  that leaves the spectrum alone; every default must reproduce the 1.1 render
  sample-for-sample.

- [x] **2. Formant tuning at high pitch.** Add a resonance strategy on top of
  the 0.32 %/semitone ramp: when the fundamental approaches F1, F1 is drawn up
  to track it, blended in over the region where a singer actually begins to
  modify, and stopped at the highest F1 that jaw reaches. Closes gap 2.
  *Verified:* F1 must track the fundamental once the fundamental passes it and
  must stop at the ceiling; the level at C6 must neither collapse against C4 nor
  shout over it; nothing may move while the fundamental is still below F1.

  Two things in the plan as written turned out to be wrong once measured, and
  are recorded here rather than quietly dropped. **"F2 gets a smaller companion
  rise on close vowels"** is backwards: opening the jaw *lowers* F2 on a front
  vowel. What F2 actually needs is a floor, because a tracked F1 otherwise
  climbs straight through it — at C6 the close anchor's F1 lands at 1047 Hz and
  its F2 sits at 815. F2 is held at 1.4 × F1 instead, which is the tightest
  spacing any real vowel presents, and the vowel loses its identity as a real
  one does up there. **"Nothing below A4 may move at all"** is also wrong: a
  female /u/ has F1 at 350 Hz, so the fundamental passes it at F4, well below
  A4. The correct statement is the one now verified — nothing moves while the
  fundamental is below F1, whatever pitch that happens to be.

- [x] **3. Just-intonation ensemble tuning.** Add an `intonation` parameter that
  blends every sounding voice from equal temperament toward the just
  interval it makes with the lowest sounding root. This covers played polyphony
  as well as chord mode, which is the more valuable half and was not in the plan
  as first written. Closes gap 3. *Verified:* at full
  just, chord-mode voices must measure 386.3 cents for the major third, 315.6
  for the minor third and 702.0 for the fifth, within a cent; at zero they must
  measure exactly the equal-tempered intervals they do today.

- [x] **4. Ensemble pitch scatter and drift correction.** Widen the static
  per-singer detune to the measured range and make the section wander instead of
  sitting at fixed offsets. The plan said "a slow random walk with a restoring
  pull"; what landed is a second incommensurate bounded oscillator per singer.
  A random walk would have had to advance from per-singer state at chunk rate,
  and the engine's whole drift model is deliberately evaluated from the absolute
  sample position so that nothing depends on how a host splits a buffer.
  Introducing stateful noise there would have cost that guarantee to gain
  nothing audible: two incommensurate oscillators already never return the
  section to the same relative configuration, and they are bounded by
  construction, which is the property a restoring pull was there to provide.
  Closes gap 4. *Verified:* the standard deviation of the sounding fundamental
  across twelve singers must land in the measured band at full Humanize and fall
  to zero at zero Humanize; the drift must be bounded over a long render; the
  per-singer offsets must not be constant over time.

- [x] **5. Nasal branch.** Add a velum-coupling parameter that inserts a murmur
  pole at the nasal cavity's own resonance and a placed notch where the closed
  mouth loads the tract, and drops the oral formants as the velum opens, because
  a closed mouth is a side branch rather than the radiator. This is what turns
  Vocalor's "ah" into a hum. Closes gap 5. *Verified:* the notch band must fall
  by at least 20 dB at full coupling, the murmur band must rise, the band above
  the notch must fall without the branch becoming a low-pass filter, the overall
  level must move less than 4 dB, and the rendered audio must stay finite and
  bounded at every coupling.

  Two things changed once measured. The plan said the zero position would
  **distinguish /m/ from /n/**; that was dropped, because it needs a second
  control for a difference that only means anything when there are consonants to
  place it in, and Vocalor does not sing words. And the obvious implementation —
  Klatt's antiresonator, normalised to unity at DC — is wrong here: for a zero
  this low it leaves 48 dB of gain at Nyquist, and it measured as a 12.8 dB
  *rise* in the 1.65 – 2.2 kHz band. A hum came out as the brightest sound the
  instrument makes. It is a matched pole-zero pair instead, which returns the
  response to unity either side of the notch.

- [x] **6. Articulator-rate coarticulation.** Make the vowel transition rate a
  property of the articulators rather than a de-zipper: each formant gets a
  speed rather than a deadline, with the transition time scaled by how far that
  formant actually has to move. Closes gap 6. *Verified:* a pad step from /i/ to
  /a/ must produce 10–90 % rise times inside a stated window for F1 and F2,
  ordered F1 > F2 > F3; a small vowel move must settle in well under half the
  time; the existing zipper-free guarantee must hold.

  The plan said **"F1 on a jaw time constant, F2 on a slower tongue-body
  constant"**. That ordering is not supportable: the jaw and the tongue body
  move at comparable speeds in vowel-to-vowel transitions and there is no robust
  general claim that one leads the other. What is defensible is the ordering the
  engine already had — the lower formants follow the larger cavity adjustments
  and are the slowest — so that was kept and moved onto articulator timescales.
  The plan also expected a timing claim for every formant; F5 is 4950 Hz for
  both /i/ and /a/ and does not move at all, which the first version of the test
  got wrong.

- [x] **7. Singer's-formant cluster.** Replace the F3/F4 amplitude trim with an
  epilarynx model that pulls F3, F4 and F5 toward a profile-dependent cluster
  centre as tension and effort rise, narrowing their bandwidths together. Closes
  gap 7. *Verified:* at high tension the spacing between F3 and F5 must contract
  by a stated fraction and the 2–4 kHz band energy must rise relative to the
  total by a stated amount; at zero tension the formants must sit where they do
  today.

- [x] **8. Factory presets.** Put a named preset table in the JUCE-free core so
  it is testable there, and expose it through the processor's program interface.
  Closes gap 8. *Verified:* every preset must carry values the engine does not clamp
  away, must render finite, audible, bounded audio on a held note and a held
  interval, and must release fully; the processor must publish the bank, write a
  selected program into its host parameters, and treat a re-assertion of the
  program already in force as a no-op, because that is the write a host makes
  while restoring a session and it would otherwise overwrite the player's
  edits.

## What the steps measured

Every figure below is printed by the DSP test suite, so it can be re-checked by
running it. "Before" is commit `732b705`, the state this plan was written
against.

| Gap | Before | After |
| --- | --- | --- |
| Dynamic timbre swing (2.4 – 4.7 kHz against the fundamental, 100 % → 30 %) | no dynamic control existed | 13.9 dB against 10.9 dB |
| Close vowel at C6 against the open vowel | 25.1 dB quieter | 7.0 dB quieter |
| Close vowel at C6 against itself at C4 | 20.1 dB quieter | 0.8 dB louder |
| Chord-mode major third | 400.0 cents | 386.3 cents at full intonation |
| Twelve-singer pitch dispersion at full Humanize | 4.4 cents | 12.6 cents |
| Largest singer drift over nine seconds | 3.5 cents | 10.4 cents |
| Nasal notch band, velum fully open | no nasal branch existed | 27.8 dB down, overall level 0.1 dB |
| Vowel step /i/ → /a/, F1 10 – 90 % rise | 35 ms, from its 16 ms time constant | 117 ms, measured |
| F3-to-F5 span, Tension 0 → 95 % | unchanged: tension never moved those frequencies | 1860 → 1065 Hz |
| 2.05 – 4 kHz share, Tension 0 → 95 % | +12 % on F3 and +6 % on F4, no clustering | 8.8 dB |
| Factory programs | 1, unnamed | 12, named and rendered by the suite |
| Twelve-singer 96 kHz render | 493.6 ns/sample | 500.5 ns/sample |

## Reconciling with main

This plan was written and executed against `732b705`. By the time the work
landed, `main` had moved on, and three of its commits touched the same code.
What follows is what that changed, so the plan matches what actually shipped.

**The lip-radiation zero is gone, and it did not come back.** The revision this
plan branched from carried a `lipZeroCoefficient_` that subtracted a fraction of
the previous excitation sample at the lips. `ef7abc5` on `main` removed it, and
its reasoning is the authority here: the excitation wavetable is already a
glottal flow *derivative*, and differentiating the flow is exactly what lip
radiation does to it, so a zero downstream applies radiation twice. It measured
as 1.89 dB of level lost on every scenario for 0.1 – 0.4 dB of broadband tilt,
with a coefficient fixed in the z plane rather than derived from a corner
frequency, so what little it did apply moved with the sample rate.

Step 6 below was authored on top of that zero and so carried a copy of the line
forward. Only the vowel-transition timing from that step was kept; the zero was
dropped. `main`'s `testSourceLevelCalibration`, which holds the reference AAH to
its calibrated level at 44.1, 48 and 96 kHz precisely so a second radiation
stage cannot be reintroduced unnoticed, is kept unchanged and passes.

**Two of `main`'s guard tests were superseded rather than deleted.** `ef7abc5`
also added tests pinning the two mechanisms it kept. Steps 6 and 7 replace both
of those mechanisms, so the tests could not survive as written:

- `testEpilarynxLiftTracksTension` asserted that tension raises the *amplitude*
  of F3 and F4 and leaves the other formants' amplitudes untouched. Step 7
  replaces that amplitude trim with frequency clustering, and because the
  formant amplitudes are derived from the all-pole cascade, moving F3 – F5
  necessarily moves every amplitude a little. Its surviving intent — that the
  epilarynx is a phonation mode and must not disturb the formants carrying the
  vowel — is now asserted on frequencies inside `testSingersFormantCluster`.
- `testArticulatorGlideOrdering` timed each formant's approach to 63 % of a
  vowel step inside a 40 ms window. Step 6 deliberately makes the transition
  slower than that window, so F1 and F2 no longer arrive inside it. Its
  surviving intent — the formants move in articulator order, and the jaw and the
  larynx must not read as one shared glide — is asserted in
  `testCoarticulationTiming`, which measures 10 – 90 % rise times over a window
  long enough for the articulator timescales this step introduces.

**The vowel pad's OPEN label.** `57ed989` moved it to the bottom edge, where the
axis it describes actually runs, and dropped includes nothing was using. Both
survive; the control-row additions in this plan's editor step sit alongside them.

## Deliberately not done

- **Word building.** Vocalor is not a lyric sequencer and the repository ships
  no sample data, so a WordBuilder equivalent would mean synthesizing plosives,
  fricatives and stops from scratch — a different instrument. The nasal branch
  in step 5 is included because it is a tract-geometry change that the existing
  model is one pole-zero pair away from, not because it is a first step toward
  words.
- **MPE.** Worth doing, but per-note expression needs a per-note voice
  allocator, and Vocalor's allocator is deliberately note-group based so one key
  can be an ensemble. That is a structural change, not a step.
- **More than twelve singer identities.** The published ensemble range stays
  2–16 with 13–16 rendering 12, because renumbering it would move every existing
  automation lane. Widening the scatter in step 4 buys more of what more
  identities would have bought.

## Second pass: the parts of a note that are not the steady state (2026-08-07)

The first pass fixed the sustain. Measured on the shipping engine, the steady
state is now at or above the commercial top tier: the alias floor is 88.9 dB
(C3) to 98.1 dB (C6) below the harmonic energy, the parallel bank is
peak-normalised and rate-invariant, the Liljencrants-Fant source is analysed
once at `prepare()` into nine band-limited levels, and the high-pitch formant
tuning is a real singer strategy rather than a curve. What is left is almost
entirely in the attack, the release, the repeat and the room — the parts of a
note a listener hears in the first ten seconds of an audition and the parts
that decide whether twelve voices read as a section or as one voice twelve
times. This pass is for those. It also answers a field that moved: in January
2026 a competitor shipped a choir built the same way Vocalor is built, and the
one feature its owners single out is the one Vocalor does not have.

### What changed in the field

A method note first, because it bounds the evidence. Every page below was read
through search-engine extraction rather than fetched: `WebFetch` was refused
for every domain tried and a direct proxy `CONNECT` returned 403. Nothing here
is invented, but figures reaching us through a secondary source are flagged as
such and must be checked against the primary before any test asserts them.

**A procedural choir now competes directly, and it undercuts the sampled
field.** [Dreamtonics Choir Voice Collections for Synthesizer V Studio 2
Pro](https://www.kvraudio.com/news/dreamtonics-releases-choir-voice-collections-for-synthesizer-v-studio-2-pro-65961)
shipped in January 2026: three collections of 16 voices each (four per part),
"built using a hybrid modeling approach that combines deep learning with
spatial signal processing", where full choir sections were recorded with a
microphone array, dissected into individual soloists, and rebuilt as "a
controllable model that scales from a single soloist to a 16-member ensemble".
Introductory price $149 each, regular $179, $299 for the bundle of three
([announcement](https://dreamtonics.com/choir-voice-collections/),
[coverage](https://bedroomproducersblog.com/2026/02/26/dreamtonics-choir-voice-collections/)).
This is the first competitor whose method is the same shape as Vocalor's, and
its advertised 1–16 singer range is the same range Vocalor publishes. The
previous section of this document argued that "the commercial formant-synthesis
field is thin, which is Vocalor's opportunity". That is no longer true of the
choir half. What remains Vocalor's is real-time playability from a keyboard —
Synthesizer V is a note-and-lyric editor even in its plug-in form — and that is
now the whole of the differentiation.

**The feature its owners name as unmatched is per-singer spatial placement.**
On [The Sound Board](https://thesoundboard.net/viewtopic.php?t=6833) owners
"praised the spacing control features, noting that few other libraries consider
this, and the ability to space out singers and overlap between sections creates
a sound no other choir library gets close to"; the vendor describes "Room Sound
Simulation" that "models spatial positioning and sound wave reflections". The
same thread names the product's weakness: "the core sound suffers slightly from
breathiness issues". Vocalor's entire ensemble spatialisation is one Spread
knob doing constant-power panning into a shared four-tap network fed from the
already-mixed stereo pair. The competitor's strength is Vocalor's largest
structural gap, and the competitor's weakness is the axis Vocalor could win on
if its aspiration model were correct.

**The vendor's stated design discovery is that the ensemble must be loose in
time, not only in pitch.** "A key discovery during development was that
individual choir singers, when recorded in isolation, tend to sing with loose
timing and pitch, but when put back together, the result snaps into place
convincingly"
([BPB](https://bedroomproducersblog.com/2026/02/26/dreamtonics-choir-voice-collections/)).
Practitioners say the same: "you need to consider note onset times and
off-times, as real vocal choirs have timing inconsistencies between individual
voices on every note, syllable, and ending point"
([KVR](https://www.kvraudio.com/forum/viewtopic.php?t=393388)). Step 4 of the
previous pass made Vocalor loose in pitch. It is still rigid in time.

**The price umbrella the table above implied has collapsed at both ends.**
Hollywood Choirs, listed there at $599, is included in the
[ComposerCloud+](https://nickcesarz.com/reviews/eastwest-composercloud-review/)
subscription at $19.99 per month. [Bela D Media Phantom
Choir](https://www.kvraudio.com/news/bela-d-media-announces-phantom-choir-for-kontakt-67402)
(June 2026, four basses and six tenors, Latin) is $119.99 for the bundle and
$69.99 for a single section. [Musical Sampling Gospel
Choir](https://www.kvraudio.com/news/musical-sampling-releases-gospel-choir-for-kontakt-with-intro-offer-62572)
launched at $99 intro, $149 regular. [Dominus Choir
Pro](https://www.kvraudio.com/product/dominus-choir-pro-by-fluffyaudio) is
still $649. A buyer's real alternative to Vocalor is now $20 a month or $70 for
a tenor section, so Vocalor cannot win on price and has to win on being played
rather than programmed. That sharpens the case for spending this pass on
performance realism.

**The control vocabulary buyers measure against has one axis Vocalor lacks.**
Synthesizer V's five continuous parameters — Loudness, Tension, Breathiness,
Gender, Tone Shift — map onto Vocalor's Dynamics, Tension, Breath and Formant
Shift almost one for one, but sit under per-voice Vocal Modes with their own
sub-parameters: "each singer has a different selection of Vocal Mode
adjustments. For example, Liam has knobs for Bright, Heavy, Rounded, Whispery,
Soft and Hoarse"
([docs](https://sv2.docs.dreamtonics.com/en/parameters)). Vocalor's twelve
singer identities differ in detune, vibrato, anatomy and drift rate. They do
not differ in phonation at all.

**On the method side, the resolution bar is older than the new arrivals.**
The nearest method competitor released since the previous pass is NUSofting Dig
Vox (July 2026, €45), whose own vendor writes that "it isn't meant to replace
snapshot-like, studio-recorded sample libraries"
([synthanatomy](https://synthanatomy.com/2026/07/nusofting-dig-vox-vocal-synth-using-physical-modeling-and-formant-synthesis.html)).
A 2015 product still sets the control-surface bar: [PPG
Phonem](https://en.wikipedia.org/wiki/PPG_Phonem) "has 12 individual resonators
which all impact the sound output" with independent frequency, gain and Q,
against Vocalor's five fixed-role formants. Both method competitors are sold as
character instruments that concede they are not realistic, so realism remains
an unclaimed position in the procedural half of the category.

**One finding cuts against an obvious move.** Ternström and Sundberg, [*Formant
frequencies of choir
singers*](https://pubs.aip.org/asa/jasa/article-pdf/86/2/517/12172019/517_1_online.pdf)
(JASA 86(2):517, 1989), report that in singing the inter-subject scatter of the
three lowest formants is *smaller* than in speech. Vocalor's per-singer formant
dispersion of ±4.5 % should therefore not be widened. The ensemble win is in
timing and space, not in more vowel scatter.

**Absences, stated as absences.** No 2026 choir release from Spitfire, Strezov,
Orchestral Tools or Audio Imperia surfaced, so every sampled benchmark the
previous pass measured against is still the benchmark. No published listening
test compares choir plug-ins with each other, and none compares procedural
vocal synthesis with sampling; the only recent controlled evaluation touching
the field is the [Singing Voice Conversion Challenge
2025](https://arxiv.org/html/2509.15629), whose result is that "no system
achieved human-level naturalness". No evidence was found that any competitor
models source-filter interaction, subglottal coupling or register mechanism, so
those are opportunities rather than catch-up.

### Where the engine actually stands

Everything below was measured on the shipping code with scratch programs, at
48 kHz unless stated. Gap numbering continues from the first pass.

**Every figure in this section was re-measured during review**, independently
of the audit that first produced it, with three scratch programs: a spectral
probe (Blackman-Harris windows, per-bin DFT, two-pole band splits), a
six-pole-cascade complex demodulator for the pitch and envelope tracks, and a
direct impulse probe of `updateRoom` through a friend accessor. Where the
re-measurement disagreed with the audit, the number below is the re-measured
one and the disagreement is named, because a step ordered by a wrong number is
a step ordered wrongly. Two counterfactual builds were also made — one with the
`early[]` branch bypassed, one with `epilarynx` forced to zero — to establish
what each proposed change can actually reach before it is written.

**9. Every note's attack has no upper formants.** For the first 50–70 ms the
voice renders through `voice.early[]` — F1 and F2 only, at 1.75× bandwidth
(`Source/DSP/VoiceEngine.cpp:1024`) — and the full five-formant tract is lerped
in over the following 65–125 ms by the `onsetMix` crossfade at
`VoiceEngine.cpp:1604-1637`, scheduled at `:788-792`. A vocal tract is fully
formed before the first glottal pulse leaves it; what develops at an onset is
the source. Male AAH, D3, Tension 0.90, Breath 0.10, Humanize 0, Vibrato 0: the
2000–3300 Hz band sits 46.81 dB below the 100–900 Hz band at t = 10–35 ms and
46.49 dB below at t = 70–95 ms, against 29.11 dB below in the sustain — the
singer's-formant band is **17.69 dB short at 10–35 ms and 17.38 dB short at
70–95 ms**, and closes to 0.05 dB by 180 ms. On a female C4 the 2–5 kHz share
is 13.74 dB under sustain at 5 ms, 8.27 dB under at 90 ms, and reaches sustain
at 180 ms. This survived because the suite checks the early stage against the
main tract in peak gain only, never in spectral content.

The counterfactual settles what the fix is worth: **bypassing the `early[]`
branch and nothing else** takes the 10–35 ms deficit from 17.69 dB to 2.79 dB
and the 70–95 ms deficit to 0.78 dB, leaves the alias floor bit-identical at
every pitch probed, lowers rather than raises the first-2 ms peak, and breaks
exactly eight assertions in the whole suite — the four sample rates × two
formants of `onsetStagePeakGains`. Everything else in gap 9 is the source, and
the source is worth much less than the tract: 2.79 dB of the 17.69.

**10. Vibrato is too shallow and too slow to read as sung vibrato at any
setting.** The cents scale is a literal 20 with ±7 of drift
(`VoiceEngine.cpp:1210-1212`) and the twelve identities are seeded to
`4.65f + 0.72f * u` (`:497`), which resolves to 4.711–5.289 Hz. Solo mode uses
singer 0, so at Humanize 0 the vibrato rate is exactly `singers_[0].vibratoRate`
= **4.711 Hz**; the audit's "4.50–4.60 Hz" is below every value the seed can
produce and does not reproduce — it is demodulator settling, not the engine.
The extent likewise follows from the code: `20 × singers_[0].vibratoDepth` =
**±21.7 cents** at Vibrato 100 %, Solo, Humanize 0, Dynamics 1.0 (a six-pole
demodulator reads ±18.5 / −19.0 through its own passband loss). The **engine
default is Vibrato 42 %, not 38 %** — 38 % is preset 0 — giving ±9.1 and
±8.3 cents respectively. Sundberg's definition is 5–7 Hz at an extent "of about
±1 semitone". Below roughly 10 cents vibrato is not heard as vibrato but as
unsteadiness, so the default sits on the threshold and every preset below it is
under it.

The knock-on is that the engine's FM-to-AM conversion has almost nothing to
work with, but the audit overstated how much. Peak-to-trough of a 10 ms
envelope is an extremum statistic dominated by shimmer and jitter, which is
where "0.37 dB with vibrato off" came from — with vibrato off there is nothing
to measure. Read as the magnitude of the envelope's component **at the singer's
own vibrato rate**, the passive contribution is **0.084 dB on a held C5 and
0.210 dB at C6**, against a 0.001 dB floor at Vibrato 0. The real figure is
less than half the audit's, and it is pitch-dependent because the tuned F1 is
what produces most of it.

**11. Note-on velocity is a volume fader.** Velocity reaches a gain
(`VoiceEngine.cpp:778`) and the corner of a one-pole source tilt
(`:1262-1273`). It never touches the glottal pulse shape, and it never touches
the envelope, because `attackCoefficient_` (`:1675`) is a per-block constant
derived from Humanize alone. Velocity 5 % → 100 %: the 150–800 Hz band rises
28.81 dB and the 2–5 kHz band 30.71 dB, a ratio of **1.066:1**, against
Sundberg's measurement that partials above 1 kHz rise roughly twice as fast in
dB as overall SPL. The 10–90 % rise of a 2 ms full-wave envelope is **32.79 ms
at velocity 0.10, 0.40, 0.70 and 1.00 alike at Humanize 0** — identical to four
significant figures across a 22.4 dB level span. The audit's 29.1 ms does not
reproduce at any Humanize setting and the setting was not stated; at the 55 %
default the rise is 47.55 ms below velocity 0.5 and 43.90 ms above, an 8 %
spread that comes from the source tilt reshaping the waveform, not from the
envelope. The mod wheel is better at 1.195:1 but still delivers about 60 % of
the physiological swing.

**12. An ensemble entry is bit-identical on every repeat.** `singer.onsetOffset`
is drawn once in `prepare()` (`VoiceEngine.cpp:496`) and merely scaled by
Humanize at `:785-786`; `voice.vibratoPhase` is reset to `0.173f * singerIndex`
at `:780`. Three successive identical `noteOn(60, 0.8)` calls in Choir/12 at
full Humanize produce **the same twelve delays every time** — 856, 365, 618,
719, 826, 663, 775, 380, 329, 308, 464, 741 samples, identical to the sample —
and the same twelve vibrato phases to four decimals (0.0000, 0.1730, 0.3460 …).
Confirmed. The scatter those delays carry is a mean of 12.23 ms with a standard
deviation of **4.10 ms**, spanning 6.42–17.83 ms. Releases are worse:
`releaseMultiplier_` is one engine-wide coefficient and every voice begins
releasing on the same sample, so a chord that entered loosely stops as one.
Vowel changes share `formantGlideFast_`/`formantGlideSlow_`, so all twelve
singers execute an identical glide.

The audit also quoted an onset-envelope correlation of "0.854, 0.879, 0.918"
between repeated chords. **That statistic is not stable and must not be built
on.** Four repeats measured against the first give 0.857, 0.656 and 0.785 in
review, because the ensemble drift oscillators advance by a different amount
between each pair and they are what makes two repeats differ at all. The
distribution already straddles the sub-0.60 threshold step 7 proposed to
assert. The exactly-measurable facts are the delays and the phases themselves.

**13. Notes get cleaner as they die instead of breathier.**
`airReleaseMultiplier_ = releaseMultiplier_ * releaseMultiplier_`
(`VoiceEngine.cpp:1676-1677`, applied at `:1545-1546`) squares the air envelope
relative to the voiced one. On the Breath 100 %, Tension 0.15, Humanize 0.60
patch the air-to-voiced ratio runs −37.12 dB at note-off, −45.45 dB at 200 ms,
−49.44 dB at 300 ms and −53.35 dB at 400 ms: **16.24 dB less breathy over
400 ms**, and 12.32 dB less at the 300 ms point step 4 asserts on. At Breath
50 % it drops 16.85 dB over 400 ms and about 21 dB over 500 ms. Reproduces. A
real offset abducts the folds and ends in air. The engine already gets the
onset right — `voice.onsetAir` gives a decaying puff at the attack — so this is
an asymmetry rather than a missing concept.

One thing the audit did not check, which step 4's own control depends on: how
much of the 5–18 kHz band is actually air. At Tension 0.90 that band sits
83.12 dB below harmonics 1–12 with Breath at 0.00 — that is pure voiced
content, the wavetable's own harmonics, which reach 16.7 kHz at C4 — against
59.12 dB at Breath 0.10 and 36.87 dB at Breath 1.00. So at note-off the band is
essentially all air even at Breath 0.10, but the voiced floor is fixed while
the air falls at twice the rate, so the floor overtakes it roughly 500 ms into
the release. The margin at the +300 ms point on the Breath 0.10 control is
about 8 dB, which is too thin to assert on without subtracting the floor.

**14. Aspiration noise is not modulated by the glottal cycle at all.**
`airDrive` (`VoiceEngine.cpp:1568-1584`) is the product of `airLevelAt_[i]`, a
per-sample parameter smoother, `airShape`, set once per control period at
`:1276`, and `airEnvelope`. **None of the three carries a glottal-phase term,
so the modulation is zero by construction, not by measurement** — the audit's
"0.26 dB" (0.18 dB in review) is the estimator's own noise floor from folding a
finite noise record, and quoting it as a finding invites someone to try to beat
it. The construction is the finding.

What the audit got wrong is the size of the prize. The isolated aspiration —
two renders with every non-noise voice field pinned and only `noiseState`
differing, then differenced, at Humanize 0 where the noise stream drives
nothing else — is **29.50 dB below the full signal at Breath 1.00** and
**42.43 dB below at the 28 % shipping default** (41.79 dB at preset 0's 30 %).
The audit measured the Breath 1.00 figure and then asserted it was "the default
sound". It is not. This is a change to the breathy patches — Breath And Air at
1.00, Airy Minor Pad at 0.62 — and a 42 dB-down redistribution everywhere else.

**15. Legato is a 0.33 ms pitch step at the default Glide.** With Glide at 0,
`chunkGlideDecay_` is 0 and the whole interval is traversed by the 16-sample
control ramp (`VoiceEngine.cpp:1251-1252`): 261.63 → 295.63 → **329.63 Hz in
0.333 ms**, with a 0.20 dB amplitude dip. The audit then claimed the next
dialable setting is already past the natural 50–150 ms window, quoting Glide
25 % as 85.8 ms, 50 % as 343.2 ms and 100 % as 1015 ms. **Those are not what
`glideTimeSeconds` returns.** `0.600f * g * g` (`VocalorMath.cpp:188-193`)
gives **37.5 ms, 150 ms and 600 ms** — the audit's figures are those time
constants multiplied by 2.30, i.e. their 90 %-settling times. Read either way
consistently, Glide 25 % settles in 86 ms, which is *inside* the 50–150 ms
window rather than past it. The step is still a step, but the "nothing usable
next to it" half of the argument is false.

**16. Chord mode changes its voicing discontinuously at hard MIDI boundaries.**
`chordMidiForSinger` (`VoiceEngine.cpp:523-542`) selects one of three interval
sets by two integer comparisons on the root and then folds each voice into
range independently. Root 47 gives {47, 54, 59, 63, 66, 71}, mean 60.00; root
48 gives {48, 48, 52, 55, 60, 64}, mean 54.50 — **a one-semitone root rise
drops the mean sounding pitch 5.5 semitones, drops the top voice 7 semitones
and duplicates a pitch**. Root 72 → 73 drops the mean 11 semitones.

**17. The room has no early reflections.** The four taps are 29.7/37.1/41.1/
43.7 ms at Size 50 % (`VoiceEngine.cpp:131-138`). Probing `updateRoom` with a
unit impulse after the smoothers have settled: first arrival **29.67 ms at
Size 50 % and 60.81 ms on Cathedral Ensemble** (roomSize 0.95, scale 2.049),
with **2 local peaks above 10 % of the window peak in the first 40 ms at
Size 50 % and 0 on Cathedral** — the audit's "zero peaks" is the Cathedral
figure only, and step 8's baseline has to say which. Real rooms deliver a first
reflection within 5–25 ms and reach a diffuse field inside 30–50 ms. RT60 from
Schroeder backward integration of that impulse response, T20 extrapolated,
is **0.231 s at Size 50 % / Room 50 % and 0.847 s on Cathedral** — not the
0.29 s and 1.17 s the audit quoted, which name no method. Any tolerance stated
against these has to name the method with them.

**18. The choir is a mono sum spread by intensity panning.** Each singer is a
mono point split by a sqrt pan law from `singer.pan * p.spread`
(`VoiceEngine.cpp:1354-1359`, positions at `:495`), and `updateRoom` takes the
already-mixed stereo pair (`:1427`). L/R correlation at Choir/12, Spread 100 %,
full Humanize, Vibrato 60 %, over 3 s from t = 1.0 s through two-pole band
splits measures **0.8758 / 0.8867 / 0.8811 / 0.8816** at 150–400, 400–1200,
1200–3000 and 3000–8000 Hz with Room at 0, and **0.8766 / 0.8876 / 0.8809 /
0.8814** at the 22 % default — flat across five octaves, which no physical
arrangement of sources is, and *the room barely moves it*, which is itself the
point: the reverb is not decorrelating anything. The lowest-to-highest band
difference is 0.0058, not the audit's 0.002. Twelve *fully independent* sources
through the same pan law and these twelve pan positions cap at **0.8392**, so
the pan law is the ceiling and no amount of humanisation can widen this.

**19. All twelve singers share one glottal source.** `effort` is built from
velocity and the global Tension with no singer term (`VoiceEngine.cpp:1262-1273`)
and `tensionAt_[]` is engine-wide (`:1759`). Across twelve active voices at
full Humanize, `tiltCoefficient` is **identical across all twelve to six
decimals** (0.634558 at Tension 0.48 / velocity 0.8 / Dynamics 1.0; the value
moves with those, the identity does not) and `airShape` spans 0.15 %. Against
that, F1 spans **800.2–875.4 Hz**. Source-spectrum dispersion across the
section is 0.00 dB. Confirmed.

**20. Every fresh note scoops up from below.** `voice.pitchScoop` is
unconditionally negative (`VoiceEngine.cpp:787`) on a fixed 72 ms exponential
(`:147`). Over 20 note-ons alternating a large ascending and a large descending
leap at Humanize 0.55: min −16.08, max −0.00, mean −5.50 cents, and **0 of 20
approached from above**. The sign is structural — the expression at `:787` is
negated whatever the hash returns — so the exact extremes depend on the note
pair and only the count of 0-from-above matters.

**21. The mod wheel commands only 18.1 dB.** `dynamicResponse`
(`VocalorMath.cpp:147`) is `exp2(-3.00f * below)` on the voiced gain, which is
exactly 18.06 dB by construction. Held C4, broadband RMS of 1 s from t = 0.8 s:
the span from Dynamics 0.00 to 1.00 measures **18.10 dB, and it measures
18.10 dB at Breath 0.00, 0.28 and 0.60 alike** — the aspiration, which only
loses 7.2 dB over the same span, is far enough down that it does not floor the
result today. It would at 30 dB, which is what step 2 has to check. A singer
from pianissimo to fortissimo covers roughly 30–40 dB.

**22. Source bandwidth halves between adjacent semitones at five points**
(structural, currently inaudible). The band-limited ladder steps in octaves
(`VoiceEngine.cpp:1254-1258`, `VoiceEngine.h:129-131`), so at MIDI 41, 53, 65,
77 and 89 the source cutoff drops from about 21.1 kHz to about 11.2 kHz between
one semitone and the next. Energy above 11.2 kHz measures −111.1 dB (MIDI 51)
against **−143.3 dB** (MIDI 53) relative to total — the audit's −124.0 dB for
MIDI 53 does not reproduce, and the true step is larger than it said. Nothing
audible rides on it either way, but any change that brightens the source would
expose it without warning.

**23. The singer's-formant cluster is applied at every pitch.**
`updateChunkState` pulls F3–F5 toward `clusterHz = male ? 2900.0f : 3200.0f`
under tension and effort with no pitch term (`VoiceEngine.cpp:965-973`), and
narrows their bandwidths together (`:1012`). Confirmed by measurement: at
Tension 0.95, Dynamics 1.0, female AAH, the chunk tract is F3 2977 / F4 3458 /
F5 4202 Hz with bandwidths 89 / 132 / 185 Hz at C4, C5 **and C6 alike, bit for
bit**. The only pitch term anywhere near it is `highTune` at `:1292`, worth
×1.0045 on F3–F5 at C6. Weiss, Brown and Morris tested 10 professional sopranos
and found no narrow cluster: at 932 Hz their bandwidths were "at least 2-kHz
wide" against under 1 kHz for tenors ([*Singer's formant in sopranos: fact or
fiction?*](https://pubmed.ncbi.nlm.nih.gov/11792022/), Journal of Voice
15(4):457–468). The centre frequency itself is fine: the cluster is applied
before `shift` (`:1006`), so it already scales with tract length.

Two numbers must be corrected before anyone builds against this gap. First,
"1860 → 1065 Hz" and "8.8 dB" are **the male D3 figures printed by
`testSingersFormantCluster`**, not a female-at-C4 baseline; the female AAH
figures at Dynamics 1.0 are a span of **2140 → 1225 Hz** (42.8 % contraction)
and a 2050–4000 Hz share rise of **+8.35 dB at C4 and +14.30 dB at C6**.
Second, and decisively: forcing `epilarynx` to zero in a counterfactual build
still leaves **+8.99 dB** of that C6 rise, because Tension also drives
`effortScale` and through it the source tilt corner at `:1267`. The cluster is
worth about 5.3 dB at C6 and 4.25 dB at C4. Disengaging it does not make
Tension inert up there, and no test written on the band share can prove it did.

**What must not regress.** The alias floor (88.9–98.1 dBc): no step here may
reintroduce a per-sample nonlinearity or a coarser wavetable read. The
peak-normalised parallel bank and `parallelFormantAmplitudes`
(`VocalorMath.cpp:214-286`), which is what makes the level independent of
sample rate, vowel and shift. The LF source and its single lip-radiation
accounting. `tunedFirstFormant` and its efficiency trim. Sample-rate invariance
of every coefficient *and* of the renormalised noise drives. Buffer-split
invariance: every drift oscillator is evaluated from the absolute sample
position, which is why the previous pass rejected a stateful random walk — step
7 below is bound by the same constraint. The nasal branch's matched pole-zero
pair, which must not be replaced by a DC-normalised antiresonator.

Two more, added in review because steps below reach them. **The factory preset
calibration**: five of the twelve presets ship Dynamics below 1.0 (Intimate
Alto 0.55, Breath And Air 0.30, Airy Minor Pad 0.48, Closed Mouth Hum 0.70,
Legato Soloist 0.82) and four ship Vibrato at or below 0.44. Any step that
rescales `dynamicResponse` or the vibrato cents scale moves those presets' level
and depth, and `testFactoryPresets` plus `testSourceLevelCalibration` are what
catch it — the re-trim belongs in the same commit as the rescale, not after it.
**The real-time cost**: `testRoughPerformance` measures 482.5 ns/sample for
twelve singers at 96 kHz on this box against a 20× guardrail. The guardrail is
generous enough to hide a doubling, so any step that adds per-sample work has
to record its measured before-and-after here rather than lean on the guardrail.

### Steps

Eight steps were proposed. Seven survive review, one is struck, and five have
had their verification rewritten because the stated test did not isolate the
effect it named. **Step numbers are frozen** so that the cross-references above
and below keep pointing at the same things — step 6 is struck in place rather
than renumbered away. The *build* order is 1, 2, 3, 4, 7, 8, 5: step 5 moves to
last, because its audibility turned out to be a tenth of what the audit
claimed, and step 1 splits into two commits for the reason given under it.

A preflight pass over the section then went through every step asking the same
question again — if this were built wrongly, or not at all, would the stated
test still pass? — and corrected the contracts where the answer was yes. Each
correction is marked **preflight correction** where it sits, in steps 1, 2, 3,
7 and 8. Three came from an external review of this document: step 7's timing
assertion, step 8's reflection seam, and step 8's per-note distance, which was
a mechanism error rather than a test one. The rest came from re-measuring the
numbers the assertions were anchored to, and from two seams that could not
have been built at all as written.

- [x] **1. Render the full tract from the first glottal pulse, and move the
  onset onto the source.** Delete the two-formant `early[]` stage and the
  `onsetMix` crossfade and render all five formants plus the nasal branch from
  sample zero. The tract is a fixed geometry with fixed poles: it is fully
  formed before the first pulse reaches it, so the singer's-formant cluster is
  present in the first cycle. What develops at a vocal onset is the source —
  the folds start abducted and lax and adduct over the first tens of
  milliseconds, which is a rise in the closed quotient and a fall in the
  aspiration-to-voiced ratio. The engine already has both halves: `onsetAir`
  is an 85 ms exponential puff (`VoiceEngine.cpp:146`) and `glottalPair()`
  crossfades a lax prototype (OQ 0.78, SQ 2.6, long return) into a pressed one
  (OQ 0.46, SQ 3.4, short return). Give each voice a source-tension ramp that
  starts below the block's tension and reaches it on the onset time constant,
  so the note speaks lax-and-breathy and firms up, and the timbre that develops
  is the phonation rather than the filter. Closes gap 9.

  Review measured the counterfactual rather than arguing about it, and it
  changes how this step should be built: **the deletion alone is worth 14.9 of
  the 17.69 dB** (deficit falls to 2.79 dB at 10–35 ms and 0.78 dB at
  70–95 ms), leaves the alias floor bit-identical at MIDI 48/60/72/84, *lowers*
  the first-2 ms peak, and breaks only the eight `onsetStagePeakGains`
  assertions. So the deletion is a near-free win that should land first and on
  its own, and the source-tension ramp is a second, separately-verified change
  worth about 2.8 dB. Do not bundle them: bundled, a ramp that does nothing is
  indistinguishable from a ramp that works.
  *Verified by*: `testOnsetSpectrum` — male AAH, D3, Tension 0.90, Breath 0.10,
  Humanize 0, Vibrato 0, 48 kHz; Blackman-Harris spectra of 25 ms windows at
  t = 10–35 ms and of the sustain at t = 1.000–1.025 s, each normalised to its
  own 100–900 Hz energy. Three assertions, because the step is two changes and
  each has to be provable on its own.

  *The ramp-off render has to be a runtime switch, not a second build.*
  Preflight correction: two of the three assertions below compare a ramp-on
  render against a ramp-off one, and "compiled to zero" cannot produce both
  inside one test binary — as written neither assertion could be built. The
  ramp depth becomes an engine field that `VoiceEngineTestAccess` can force to
  zero (the friend struct already reaches every private member), reapplied on
  each control update so a sustained note cannot smooth back onto it. Step 5
  needs the same switch for its own depth and must reuse it.

  *The tract is present from sample zero.* With the source-tension ramp depth
  forced to zero through that switch, the 2000–3300 Hz share at t = 10–35 ms
  must sit no more than **3.5 dB** below the sustain share; today it is
  17.69 dB below and the
  deletion alone measures 2.79 dB, so 3.5 dB is the measured value with a
  0.7 dB working margin. *The audit's single 3.0 dB threshold left 0.21 dB and
  would have been flaky — and it was asserted on the shipping ramp, which makes
  the onset duller and therefore makes the number larger, not smaller.*

  *The gap is closed.* With the ramp at its shipping depth the same share must
  sit no more than **8.0 dB** below sustain, against 17.69 dB today.

  *The ramp is doing the remaining work.* ~~The ramp-on deficit must exceed the
  ramp-off deficit by at least **1.5 dB**.~~ **This is unachievable at any ramp
  depth and was replaced during implementation; see *What actually shipped*
  below.** The rest of the paragraph stands: it replaces the audit's second
  assertion — "the 5–18 kHz aspiration share must be at least 4 dB above its
  sustain value" — which **verifies nothing**: that share is 18.79 dB above
  sustain today and 20.58 dB in the counterfactual build with no ramp at all,
  so it passes whether or not the ramp is ever written. Keep the aspiration
  check only as a direction test: the 5–18 kHz share must fall monotonically
  across windows at 10–35, 70–95 and 180–205 ms, which it does today
  (18.79 → 7.41 → 2.80 dB above sustain).

  And it must not click. The audit's bound — first-2 ms peak within 1.0 dB of
  the sustain peak — has about 20 dB of slack: today that peak is 21.87 dB
  below the sustain peak at female C4 Tension 0.90, 28.33 dB below at male D3,
  24.03 dB below at C6, and the counterfactual build lowers all three by 2–4 dB.
  Assert against the measured value instead: **the first-2 ms peak must stay at
  least 18 dB below the sustain peak** at male D3, female C4 and female C6, at
  Tension 0.30 and 0.90.

  The onset-stage normalisation block inside `testParallelFormantBank`
  (`Tests/VoiceEngineTests.cpp:1523-1546`, via `onsetStagePeakGains`) asserts a
  stage this step removes; it is the only thing in the suite that does, which
  review confirmed by building and running the counterfactual.

  *What actually shipped*: both halves, in one commit rather than two, because
  the second half turned out to need the first half's test to exist before it
  could be verified at all. `voice.early[]`, `voice.onsetMix`,
  `voice.onsetComplete` and the three `early*_` coefficient arrays are gone, and
  every voice ticks all five formants plus the nasal branch from its first
  sample. The source-tension ramp reuses `onsetAir` rather than adding a second
  envelope — the step names it as the onset time constant and it is already
  stepped per sample and already saved across blocks — so the glottal prototype
  crossfade is driven at `tensionAt_[i] * (1 - depth * onsetAir)` and the
  aspiration puff and the fold adduction are literally the same 85 ms gesture.
  The depth is `sourceTensionRampDepth_`, re-read at every control update, and
  `VoiceEngineTestAccess::setSourceTensionRampDepth` forces it, as the preflight
  correction required. Step 5 reuses that switch. `testOnsetSpectrum` is the new
  test; the eight `onsetStagePeakGains` assertions and the accessor behind them
  are deleted, and nothing else in the suite moved.

  **The measured baseline reproduces exactly.** 17.68 dB at 10–35 ms, 17.37 at
  70–95, −0.05 at 180–205, sustain share −29.15 dB, aspiration 18.79 → 7.41 →
  2.80 dB above sustain, first-2 ms peak 28.33 / 21.88 / 24.03 dB below the
  sustain peak at male D3, female C4 and female C6 at Tension 0.90. Those last
  three are measured on the engine's *shipped defaults*, not on the test suite's
  `steadyParameters()`, which runs a narrower resonance and a wider spread and
  reads 3.8–3.9 dB tighter on all six legs. The new test therefore builds its
  parameters from `EngineParameters` directly: under `steadyParameters()` the
  shipped configuration reads 16.01 dB at female C4, Tension 0.30, and the 18 dB
  bound the step publishes would fail against a baseline it was never measured
  on.

  **The third assertion was backwards, and no depth can satisfy it.** The step
  reasoned that a lax source is duller and so must widen the 2000–3300 Hz
  deficit. It does make the onset duller in absolute terms — the 2000–3300 Hz
  band at 10–35 ms sits 3.40 dB lower with the ramp than without it — but the
  deficit is that band *normalised to 100–900 Hz*, and the ramp takes 5.62 dB
  out of the denominator, so the deficit *falls*. Measured across depths
  0.00 / 0.35 / 0.50 / 0.60 / 0.70 / 0.85 / 1.00 the 10–35 ms deficit is
  2.79 / 1.36 / 0.76 / 0.57 / 0.70 / 1.54 / 2.80 dB: it never once exceeds the
  ramp-off value, so the 1.5 dB clause fails everywhere.

  The cause is worth recording, because it is a property of `glottalPair` that
  nothing else in this document names. The two prototypes are crossfaded in the
  *time* domain, and their return phases sit at different instants, so a
  mid-tension source is two pulses summed at different phases rather than a
  pulse of intermediate open quotient. Harmonics cancel: at table level 8 the
  second harmonic falls from 0.1489 (lax) and 0.1467 (pressed) to 0.0342 at
  tension 0.36, and the fifth from 0.0397 / 0.0723 to 0.0152. On a male D3 the
  fifth harmonic is 734 Hz, which is where the AAH F1 sits, so the 100–900 Hz
  band is exactly what the cancellation empties. The non-monotonic deficit above
  is that notch: at depth 1.00 the first pulse is the undiluted lax prototype
  and the cancellation is gone, which is why the number returns to 2.80.

  What replaced the clause measures the same thing the step's own physiology
  names — "a fall in the aspiration-to-voiced ratio" — as a differential rather
  than an absolute, which is what the paragraph above correctly says the
  absolute cannot do: **the 5–18 kHz share at 10–35 ms must be at least 3.0 dB
  higher with the ramp than with it forced out**, measured 5.62 dB, plus **the
  two renders must agree within 0.5 dB in all three bands at the sustain
  window**, measured within 0.001 dB. The ramp-on leg does not name a depth, so
  a depth shipped at zero fails rather than passing quietly. Reverted, the two
  halves fail separately and cleanly: restoring `early[]` reads 17.68 dB against
  the 3.5 dB bound and 17.51 against the 8.0 dB bound, and shipping the depth at
  zero reads 0.00 dB against the 3.0 dB bound.

  **The depth is 0.60, and the click bound is what set it.** The step left the
  depth unspecified. Physiology argues for 1.00 — the folds begin at the
  abducted configuration, which is the lax prototype's open quotient of 0.78 —
  but the lax prototype carries a much larger fundamental (its first harmonic is
  0.3512 against the pressed prototype's 0.1505, and it is normalised to a
  2.5 dB higher band RMS by design), so at depth 1.00 the first-2 ms peak on
  female C4 at Tension 0.30 rises to 18.10 dB below the sustain peak, 0.10 dB
  inside the step's own 18 dB bound. Depth 0.60 leaves 1.81 dB of margin
  (19.81 dB) and puts the first pulse of a Tension 0.90 patch at an open
  quotient of 0.665 against the 0.492 it settles on, which is the range voice
  onsets are measured over. The six click figures ship at 23.19 / 31.50 /
  19.81 / 21.60 / 29.31 / 24.95 dB, against 19.61 / 28.33 / 19.90 / 21.88 /
  28.16 / 24.03 dB on the shipping engine: the deletion widens every one of
  them and the ramp gives most of that back on the two female C4 legs, netting
  out within 0.1 dB of where the instrument already was.

  **Two knock-ons for later steps.** The per-sample cost of the ramp is one
  multiply and one subtract: `testRoughPerformance` reads a median of 498.2
  ns/sample over five runs with it and 484.8 without, so 13 ns/sample or 2.8 %,
  against a 20× guardrail — recorded here as the section demands rather than
  hidden behind the guardrail. And the onset is now materially slower and
  velocity-dependent for the first time: a four-period sliding-peak envelope on
  a held C4 at Humanize 0 measures a 10–90 % rise of 10.75 ms at velocity 0.05,
  0.40 and 1.00 alike on the shipping engine, and 20.12 / 16.33 / 16.38 ms
  after this step. **Step 2's rise-time baseline was measured before this step
  and has to be re-measured against it**, including the 6–14 ms accented window,
  which the engine no longer reaches from a standing start.

  One documentation debt is left deliberately: `README.md:543` still says "the
  onset stage must measure the same peak gain as the main tract it crossfades
  into", describing a stage and a test that no longer exist. The README is owned
  elsewhere in this pass and was not edited here.

- [ ] **2. Make the dynamic controls command a singer's range and shape the
  onset.** Three changes to the same mechanism. Velocity must set the envelope
  time constant, because a soft onset has to approach phonation threshold
  pressure slowly and an accented one arrives above it: `attackCoefficient_`
  becomes per-voice and falls with effort instead of being a per-block constant
  from Humanize (`VoiceEngine.cpp:1675`). **Humanize must survive as a
  multiplier on that time constant, not be displaced by it**: the README
  publishes Humanize as the dial that loosens a take, and today the attack time
  is one of the things it loosens. Velocity must also enter the source tension
  ramp step 1 introduces, so a soft attack is lax as well as slow. And
  `dynamicResponse`'s voiced gain moves from `exp2(-3.00f * below)` to the 30 dB
  a singer actually covers between pianissimo and fortissimo, with the effort
  and tension scales following so the ratio of presence-band change to
  broadband change reaches the roughly 2:1 Sundberg measures for partials above
  1 kHz. Closes gaps 11 and 21.

  The preset re-trim is part of this step, not a follow-up. **Preflight
  correction: it is five presets, not two, and the drops are about twice what
  was written.** Measured on a counterfactual build with only the voiced
  exponent moved (`exp2(-4.9829f * below)`, which is 30.00 dB at Dynamics 0),
  two notes held at 57 and 64, 1 s of stereo RMS from t = 0.6 s at 48 kHz, each
  preset at its shipped values:

  | preset | Dynamics | today | 30 dB build | drop |
  | --- | --- | --- | --- | --- |
  | Breath And Air | 0.30 | −29.81 dB | −38.11 dB | 8.30 dB |
  | Airy Minor Pad | 0.48 | −29.63 dB | −35.83 dB | 6.20 dB |
  | Intimate Alto | 0.55 | −25.06 dB | −30.43 dB | 5.37 dB |
  | Closed Mouth Hum | 0.70 | −34.94 dB | −38.52 dB | 3.58 dB |
  | Legato Soloist | 0.82 | −25.41 dB | −27.56 dB | 2.15 dB |

  The other seven presets ship at Dynamics 1.00 and do not move at all. The
  drop is the full voiced-gain delta, `11.94 × (1 − Dynamics)` dB, because the
  aspiration never sets the floor: at Breath 1.00 it is still 29.5 dB under the
  voiced component, so Breath And Air — the breathiest preset in the bank —
  loses the whole 8.30 dB. Recover the level either by raising `outputGain` or
  by raising the preset's own Dynamics; say which in the commit. If it is the
  output gain, Breath And Air needs 1.845 of the published 2.0 maximum, which
  is nearly out of headroom and is a reason to move its Dynamics instead.

  *Verified by*: `testVelocityShapesOnset` — Solo, held C4, **Humanize 0**
  (the audit did not pin it, and the figure moves by 92 % across the Humanize
  range: 17.85 ms at 0, 27.62 ms at 0.55, 34.26 ms at 1.0), Vibrato 0, Room 0,
  Dynamics 1.00. **Preflight correction: the envelope is four periods of the
  sounding fundamental (15.29 ms at C4), not the 2 ms the audit specified, and
  the baseline is 17.9 ms rather than 32.79 ms.** A 2 ms full-wave window at C4
  is shorter than the 3.82 ms glottal period, so it does not smooth the
  waveform out and the 10–90 % crossing lands on within-period ripple: measured
  on the shipping engine at Humanize 0 the same rise reads 7.77 ms at a 2 ms
  window, 13.2 ms at 8 ms, 17.9 ms at four periods and 27.8 ms at 30 ms. A
  metric whose answer is set by its own window length cannot anchor a
  two-sided assertion, and at the audit's window today's 7.77 ms already sits
  *below* the 8 ms floor it proposed for the accented attack — the fast case
  would have been half-satisfied by doing nothing.

  On the four-period envelope, 10–90 % rise by linear interpolation on a
  0.5 ms grid, today's rise is **17.90 / 17.90 / 17.87 / 17.86 / 17.85 ms at
  velocity 0.05, 0.10, 0.40, 0.70 and 1.00** — velocity-independent to three
  significant figures, which is the fact the step exists to change. The
  assertion is a two-sided window rather than the audit's open-ended "at least
  3×", which was a drawn number: the rise must land inside **6–14 ms at
  velocity 1.00 and 45–120 ms at velocity 0.10**, and the ratio between them
  must be at least 2.5×. Both ends bite — today's 17.85 ms fails the 14 ms
  ceiling and today's 17.90 ms fails the 45 ms floor — and the outer bounds are
  what stop an accented attack from clicking and a soft one from becoming a pad
  swell.

  Across velocity 0.05 → 1.00 the 2–5 kHz band must move at least 1.6 dB per dB
  of 150–800 Hz (today 1.066); across Dynamics 0 → 1 the same ratio must reach
  1.6 (today 1.195). `testDynamicRange` — Solo, held C4, Humanize 0, Vibrato 0,
  Room 0, output gain 1.0; broadband RMS of 1 s from t = 0.8 s at Dynamics 0.00
  and 1.00 must span at least 28 dB **at Breath 0.28**, which has to be pinned
  because the aspiration only loses 7.2 dB over the same span and becomes the
  floor once the voiced gain moves past about 35 dB. Today the span is
  18.10 dB at Breath 0.00 and 0.28 and 18.09 dB at 0.60, so the floor is not
  yet binding; assert it directly by requiring the span at Breath 0.60 to stay
  within 2 dB of the span at Breath 0.00. The 30 dB counterfactual returns
  30.04 / 30.00 / 29.82 / 29.24 dB at Breath 0.00 / 0.28 / 0.60 / 1.00, so both
  bounds are reachable with margin and the aspiration's flattening is already
  visible at Breath 1.00.

  **Preflight correction: nothing in the suite made the re-trim mandatory.**
  `testFactoryPresets` asserts only that a preset is finite, above 1e-5 RMS and
  under 4.0 peak, so a preset left 8.3 dB quiet passes it, and
  `testSourceLevelCalibration` renders at Dynamics 1.00 where the voiced gain
  does not move at all — the stated "must both still pass" contract could not
  fail whether the re-trim happened or not. Add the binding assertion:
  under the protocol tabled above, each of the five presets that carry
  Dynamics below 1.00 must render within **1.0 dB** of its pre-change level
  (Breath And Air −29.81, Airy Minor Pad −29.63, Intimate Alto −25.06, Closed
  Mouth Hum −34.94, Legato Soloist −25.41 dB), and the seven at Dynamics 1.00
  must be unchanged within the same 1.0 dB. `testFactoryPresets` and
  `testSourceLevelCalibration` must still pass alongside it.

- [ ] **3. Put the vibrato in the measured band and give it the amplitude
  modulation it is missing.** Reseed `singer.vibratoRate` across 5.6–7.0 Hz,
  which is the band Sundberg's definition and the 2022 systematic review both
  give ([*The Role of Vibrato in Group
  Singing*](https://www.sciencedirect.com/science/article/pii/S0892199722003551)),
  against today's 4.711–5.289 Hz. Scale the extent to reach about ±100 cents —
  "an extent of about ±1 semitone" — at Vibrato 100 %, but make the ceiling
  mode-dependent, because the same review records that "vibrato extent tended
  to be higher in solo singing compared to group singing" and twelve singers at
  solo extent smear: Solo reaches ±100 cents, Choir and Chord reach ±40. Then
  add the modulation the model is missing. Sundberg names three amplitude
  sources in vibrato and the engine has only the passive one — harmonics
  sweeping static formant skirts, which measures 0.084 dB at C5. The laryngeal
  one is an oscillation of subglottal pressure and glottal configuration on the
  same cycle ([*Laryngeal-Level Amplitude Modulation in
  Vibrato*](https://www.sciencedirect.com/science/article/abs/pii/S0892199707000689)),
  so drive the source amplitude and the tilt corner from the same
  `voice.vibratoPhase` the pitch already uses, at a depth proportional to the
  extent actually in force rather than to the knob. Closes gap 10.

  **The cents scale must stop being linear in the knob, or the presets move.**
  Today the extent is `p.vibrato × depth × 20`, so a fivefold rise in the
  ceiling is a fivefold rise at every setting: preset 0's 38 % goes from
  ±8.3 to ±41 cents and the engine default's 42 % from ±9.1 to ±45. **Preflight
  correction: it is eleven presets, not four.** The bank ships Vibrato at 0.38,
  0.26, 0.44, 0.46, 0.18, 0.30, 0.42, 0.28, 0.30, 0.40, 0.32 and 0.34, so every
  preset except Legato Soloist sits at or below 0.44 and re-dialling is not the
  cheaper of the two options. Reshape the curve so the 30–45 % region lands
  near ±20–25 cents, and state the reshaped mapping in the commit.
  *Verified by*: `testVibratoRateAndExtent` — held C4, Humanize 0, Vibrato
  100 %, Dynamics 1.00, rendered in 16-sample blocks so the pitch track can be
  read at the control rate. **Preflight correction: the measurement seam is
  `soundingFrequencies()` for both rate and extent, not the audio-domain
  demodulator.** It returns `phaseIncrement × sampleRate`, which is the
  frequency the oscillator is actually running at rather than an estimate of
  it, so there is no passband loss to argue about and no ambiguity over which
  of two numbers is asserted — the audit's ±21.7-versus-±18.5 question simply
  does not arise. All twelve rates come off one Choir/12 render — Solo sounds
  only singer 0, so it cannot show the other eleven. Every per-singer rate,
  taken as the reciprocal of the mean interval between rising zero crossings of
  that voice's track about its own mean over 2 s, must fall inside 5.5–7.2 Hz
  (today 4.711–5.289). Extent at Vibrato
  100 % in Solo must reach at least ±80 cents against today's **±21.7 by
  construction** (`20 × singers_[0].vibratoDepth`, `vibratoDepth` = 1.0861).
  One cross-check keeps the field honest, because a reseeded
  `singer.vibratoRate` that nothing reads would otherwise pass: the Solo rate
  read from the track must agree with `singers_[0].vibratoRate` within 2 %.

  **The Choir/12 extent assertion as written is not measurable and is
  replaced.** Complex demodulation of a twelve-voice mix does not return a
  per-voice extent: attempted in review it returned +111 / −1112 cents, because
  twelve detuned carriers inside one demodulator passband beat rather than
  resolve. Read the extent from `soundingFrequencies()`, which the suite
  already exposes per voice, sampled at the control rate over 2 s, and assert
  that **each of the twelve voices' own extent lands between ±25 and ±50 cents**
  in Choir/12.

  `testVibratoAmplitude` — Solo, Humanize 0, Dynamics 1.00, so the rate the
  metric selects on is the single known `singers_[0].vibratoRate`. The audit's
  metric, peak-to-trough of a 10 ms
  envelope, is an extremum dominated by shimmer and jitter; it is what produced
  the phantom "0.37 dB with vibrato off", and it is replaced by the **magnitude
  of the envelope's component at the singer's own vibrato rate**. Measured on a
  held C5 that is 0.001 dB at Vibrato 0 and **0.084 dB at Vibrato 100 %** (C6:
  0.001 and 0.210 dB). Require at least **1.0 dB at C5 and at C6** at Vibrato
  100 %, and at most 0.05 dB at Vibrato 0 — with a rate-selective metric the
  vibrato-off value is a true zero, so the "modulation is tied to the extent"
  intent is now actually testable.

- [ ] **4. Let the air outlive the voice at an aspirate offset.** Replace
  `airReleaseMultiplier_ = releaseMultiplier_ * releaseMultiplier_`
  (`VoiceEngine.cpp:1677`) with a coefficient derived from the adduction the
  note is already in. At a released note the folds abduct: transglottal flow
  continues while the oscillation stops, so the voiced component dies first and
  the turbulent one outlives it — an aspirate offset "tapers from voice into
  breath", where a glottal offset ends "while the folds are still approximated"
  ([Voice
  Science](https://www.voicescience.org/lexicon/aspirate-onset-offset/)). Which
  of the two a note gets is not a preference, it is the phonation the note was
  in: at high Breath and low Tension the air time constant becomes longer than
  the voiced one, and at low Breath and high Tension it stays as short as it is
  today. Closes gap 13.
  *Verified by*: `testReleaseAerodynamics` — held C4 for 1 s then note-off,
  48 kHz; voiced energy from harmonics 1–12 in ±45 Hz bands, air energy from
  5–18 kHz. At Breath 1.00, Tension 0.15, Humanize 0.60 the air-to-voiced ratio
  300 ms after note-off must be at least 6 dB *above* its value at note-off;
  today it is **12.32 dB below**. The existing `testReleaseCompletes` bound must
  still hold: the note must reach silence and free its voice.

  **The pressed-offset control has to change its Breath setting.** The audit
  put it at Breath 0.10, where the 5–18 kHz band is not purely air: measured
  against a Breath 0.00 render, that band is 83.12 dB below harmonics 1–12 with
  no aspiration at all and 59.12 dB below at Breath 0.10, and because the air
  falls at twice the voiced rate the fixed voiced floor overtakes it about
  500 ms into the release — at the +300 ms point the margin is only about 8 dB,
  so a "must not rise by more than 2 dB" assertion there is partly measuring
  harmonics against harmonics. Run the pressed control at **Breath 0.28,
  Tension 0.90** instead, where the margin is 33 dB, and keep the same
  assertion: the ratio must not rise by more than 2 dB, so a pressed note still
  stops cleanly.

- [ ] **5. Modulate the aspiration with the glottal cycle.** *(Keeps its number,
  but builds last, after steps 7 and 8: the mechanism is right and cheap, and
  its audibility is a tenth of what the audit claimed.)* Aspiration turbulence
  is generated by flow through the glottal constriction, so its envelope is the
  glottal flow itself: it rises through the open phase, peaks near maximum flow
  and again at the sharp closure, and is largely extinguished while the folds
  are closed.
  Hermes is explicit that stationary noise "is to a large extent perceived as
  coming from a separate sound source which hardly contributes to the breathy
  timbre of the vowel", and that the fix is "noise with a temporal envelope of
  the same periodicity as the pulse train" ([Speech Communication,
  1991](https://www.sciencedirect.com/science/article/abs/pii/016763939190053V));
  Klatt and Klatt use pitch-synchronous amplitude-modulated Gaussian noise for
  the same reason
  ([JASA, 1990](https://pubmed.ncbi.nlm.nih.gov/2137837/)). The engine has the
  phase accumulator in hand at exactly the point the noise is added
  (`VoiceEngine.cpp:1568-1584`), and the open quotient is already implied by
  the lax/pressed crossfade `tensionAt_` selects, so the window costs one
  multiply per sample and is derived rather than drawn. Closes gap 14.

  **Sized honestly, this is a breathy-preset fix.** The isolated aspiration is
  29.50 dB below the full signal at Breath 1.00 but **42.43 dB below at the
  28 % shipping default**; the audit measured the first and claimed the second.
  It earns its place because it is one multiply and because Breath is the axis
  the January 2026 competitor is criticised on — not because it changes the
  default patch, which it barely can.
  *Verified by*: `testAspirationIsPitchSynchronous` — isolate the noise by the
  same difference trick the audit used (two renders with every non-noise voice
  field pinned and only `noiseState` differing), which is exact **only at
  Humanize 0**, where the noise stream drives no jitter and no shimmer; the
  test must therefore pin Humanize 0 rather than merely happen to use it. Fold
  8 s of residual onto the glottal period in 24 bins at C4, Breath 1.00,
  Tension 0.30, Vibrato 0 — **binning by `voice.phase` read through the test
  accessor, not by a free-running nominal period**, so the fold stays exact if
  anything ever perturbs the pitch. Peak-to-trough across the 24 bins must be
  at least 8 dB; today it is **zero by construction** (0.18 dB of estimator
  noise, which is what the fold's own floor measures and not a property of the
  engine). The peak bin must fall in the open phase rather than the closed one.
  Broadband aspiration RMS must change by less than 1.0 dB against the same
  render with the modulation depth forced to zero, so this is a redistribution
  in time and not a level change — forced through the same
  `VoiceEngineTestAccess` depth switch step 1 introduces, since a single test
  binary cannot hold two builds. The engine's alias floor assertions must
  still pass, since this introduces a per-sample product.

- [ ] ~~**6. Disengage the epilaryngeal cluster above the soprano crossover.**~~
  **Struck in review.** Moved to "considered and not planned" below with the
  four reasons it failed. The gap is real; the step was not buildable as
  specified and its test could not have passed.

- [ ] **7. Redraw the ensemble's timing at every note, and stagger the
  release.** Onset asynchrony is the loudest cue that a choir is people, and
  today it is a table of twelve constants drawn in `prepare()`. Draw the entry
  delay and the initial vibrato phase from `voice.noiseState` instead, which is
  already a per-note hash of generation, root and singer index — the render
  stays a pure function of the note sequence, so buffer-split invariance
  survives, which a stateful random walk would have cost. Review checked that
  argument at the source and it holds: `voice.noiseState` is
  `hash32(generation_ ^ (rootMidi * 977 + singerIndex * 131))`, `generation_`
  is reset in `reset()`, and note events only arrive between `process()` calls,
  so two engines given the same note sequence draw the same hashes however the
  host slices its buffers. Then give the release the same treatment:
  `releaseMultiplier_` is one engine-wide coefficient applied on the same
  sample to every voice, so twelve people let go simultaneously. A per-voice
  release start delay and a per-voice release time constant, drawn from the
  same per-note hash, turn the cut-off into the ragged decrescendo it is in a
  real section. Closes gap 12.

  **How far to widen it is a design decision, and it has to be written as
  one.** Today the twelve entries have a standard deviation of 4.10 ms about a
  12.23 ms mean, spanning 6.42–17.83 ms. PLOS One's 30–50 ms is onset asynchrony
  in ensemble *playing* over a piece
  ([*Perception of synchronization in singing
  ensembles*](https://journals.plos.org/plosone/article?id=10.1371%2Fjournal.pone.0218162)),
  not the scatter of one choral attack, and the step itself concedes it is an
  upper bound rather than a target. The audit then turned it into an
  open-ended "at least 12 ms" with no ceiling, which is a drawn number with a
  citation standing behind it, and an unbounded floor on a scatter parameter is
  exactly how an instrument acquires a flam. **State it two-sided.**

  **And it must be reconciled with step 8.** A 1.5–6 m distance range is
  4.4–17.5 ms of propagation delay, which is about 3.8 ms of entry-time
  standard deviation on its own — comparable to the whole of today's scatter.
  If both steps land, the timing test measures their sum, and step 7's own
  contribution has to be sized against the post-step-8 total rather than
  against today's.
  *Verified by*: `testEnsembleTimingIsRedrawn` — three successive identical
  `noteOn(60, 0.8)` calls in Choir/12 at Humanize 1.0 must produce three
  different sets of twelve entry delays and three different sets of twelve
  vibrato phases; every pairwise comparison of corresponding entries must
  differ by more than 1 sample and more than 0.001 cycle. Today all three sets
  are identical to the sample and to four decimals (856, 365, 618, 719, 826,
  663, 775, 380, 329, 308, 464, 741 samples; phases 0.173 × singer index).

  **Preflight correction: the spread and the ceiling are asserted on the
  audible onset, not on the entry-delay field.** The step's own paragraph above
  says the timing test measures the sum of entry delay and step 8's propagation
  delay, but the assertion as written read only `voice.delaySamples`, so an
  implementation could sit inside the window on the field and still put voices
  55 + 13.1 ms apart at the ear once placement landed — the anti-flam ceiling
  the two-sided wording exists to impose would not have been imposed on
  anything audible. Define the audible onset of a singer as its entry delay
  plus its direct-path delay, which is zero until step 8 lands and `r/343 m/s`
  after it, and assert on that: **population standard deviation between 8 and
  18 ms** at Humanize 1.0 — against 4.098 ms today — and **no single voice more
  than 55 ms behind the earliest**. Say population standard deviation and mean
  it; the sample estimator reads 4.280 ms on the same twelve values and the
  bounds are drawn against the population one. Because the two components are
  drawn from independent hashes their variances add, so 3.8 ms of propagation
  costs at most 0.4 ms at the top of the window and step 7 can size its own
  scatter anywhere in 8–17.6 ms; the ceiling is the binding one, and step 7's
  entry-delay spread must stay under **40 ms** so the sum clears 55 ms even
  when the two extremes land on the same singer. The entry-delay field alone
  must still be exactly 0 ms at Humanize 0 — placement is a room, not a
  performer, so it is not Humanize-scaled and the audible onsets stay spread at
  Humanize 0 after step 8.

  **The onset-envelope correlation assertion is struck.** It is not a stable
  statistic: four repeats measured against the first give 0.857, 0.656 and
  0.785, because what makes two repeats differ at all is how far the sub-0.15 Hz
  drift oscillators have advanced between them, which depends on the spacing of
  the note-ons. The distribution already straddles the proposed sub-0.60
  threshold, so the test would have been green on the shipping engine roughly a
  third of the time. The delays and the phases are exactly measurable; assert on
  those. `testReleaseStagger` — after a common note-off in Choir/12 at Humanize
  1.0, the spread between the first and the last voice to fall below −40 dB
  must be at least 80 ms and at most 400 ms; today it is 0 ms. A twelve-voice
  mix cannot be resolved back into twelve envelopes, so the seam is each
  voice's own `envelope` field read through `VoiceEngineTestAccess` once per
  control period, and −40 dB is measured against that voice's own value at the
  note-off sample.
  `testSampleRateInvariance` and the buffer-split checks must still pass
  unchanged: two engines prepared identically and given the same note sequence
  must render bit-identically, and 1/17/512-sample splits must reproduce a
  single-block render.

- [ ] **8. Place the singers in the room instead of panning them into it.**
  Every singer is a mono point split by a sqrt pan law and then summed into a
  shared four-tap network fed with the already-mixed stereo pair, which is why
  the L/R correlation is 0.88 and flat across five octaves and why the pan law
  caps the achievable value at 0.839 even for twelve fully independent sources.
  Give each singer a position instead: an azimuth from the spread it already
  has and a distance from its identity hash over a stated range. Everything
  else falls out of the geometry rather than being drawn. Direct-path delay is
  r/c at 343 m/s, so a 1.5–6 m range is 4.4–17.5 ms of arrival difference,
  which is the decorrelation intensity panning cannot produce. Direct level is
  1/r. First-order image sources — two side walls, floor and ceiling at the
  dimensions `roomSizeScale()` already implies — give each singer its own
  early-reflection pattern at image distances through the same 1/r, which is
  what the engine has none of: its first arrival is 29.67 ms at Size 50 % and
  60.81 ms on Cathedral Ensemble, with two local peaks before 40 ms at Size
  50 % and none at all on Cathedral. Feed the recirculating network per voice
  from that reflection sum rather than from the stereo mix, and hold the send
  constant with distance, so a far singer is wetter than a near one because its
  direct term fell and its send did not. This is the feature owners of the
  January 2026 competitor name as unmatched. Closes gaps 17 and 18.

  **Three things the step has to decide before it is written**, all of which
  review found missing and any of which can sink it.
  *Where the delay lines live.* They must be per **singer identity** — twelve —
  not per voice: ninety-six lines carrying 17.5 ms at 192 kHz is 1.3 MB of
  engine state against 161 kB for twelve, and `VoiceEngine` holds its state in
  fixed `std::array` members. That means voices sharing a singer are summed
  into one position before the delay, which is what a singer standing in one
  place means anyway.
  *What it costs.* Twelve positions × (one direct + four image sources) is 60
  fractional-delay reads per sample against the four the room does today, on
  top of twelve writes. `testRoughPerformance` measures 482.5 ns/sample for
  twelve singers at 96 kHz on this box; the step must record the measured
  after-figure in this document, because the 20× guardrail is loose enough to
  hide a doubling. Preflight: a guardrail that cannot fail is not a contract,
  and a tight wall-clock bound on a shared four-core box would be flaky
  instead. So the figure is a written obligation with a number attached —
  past **965 ns/sample**, twice today's, the per-singer path is reworked before
  the step lands — and `testRoughPerformance`'s 20× guardrail must still pass
  alongside it.
  *When the distance is drawn.* A distance is a delay, and a delay that follows
  a knob is a pitch-shifter. **Preflight correction: the distance belongs to
  the singer identity and is drawn once in `buildSingerIdentities()`, not
  frozen at note-on from the per-note hash.** The two halves of this step
  contradicted each other: there are twelve delay lines, one per singer, but
  `maxVoices` is 96 and `noteOn` silences only voices on the same root, so
  eight Choir/12 notes can sound at once and every singer index then carries up
  to eight voices — measured, two simultaneous roots give 24 active voices,
  exactly two per singer. A per-note distance on a shared line has nowhere to
  go: the second note either drags the first note's delay to its own draw,
  which is the pitch shift this paragraph exists to forbid, or is rendered at
  the first note's distance, which is the wrong position. An identity-drawn
  distance has neither problem and is what "a singer standing in one place"
  already meant. Spread and Size then move the azimuth and the image geometry
  only; the direct-path delay never moves at all, and the image delays follow
  Size exactly as the existing four room taps already do.
  *Verified by*: `testSingerPlacement` — Choir/12, Spread 100 %, Humanize
  100 %, Vibrato 60 %, **Room pinned at 0** (the audit left it unstated; review
  measured 0.8758/0.8867/0.8811/0.8816 dry against 0.8766/0.8876/0.8809/0.8814
  at the 22 % default, so the reverb is not currently a confound — but a step
  that rebuilds the room could make it one, which is the whole reason to pin
  it), held C4, 3 s from t = 1.0 s, two-pole band split. L/R correlation must
  fall below 0.60 at 1200–3000 Hz and below 0.70 at 3000–8000 Hz (today 0.8811
  and 0.8816), and must be frequency-dependent: the 3000–8000 Hz value at least
  0.10 below the 150–400 Hz value, where today they differ by **0.0058**. The
  same measurement repeated at Room 50 % must move no correlation by more than
  0.05, so the placement and not the reverb is what did it.

  Three assertions on the geometry itself, added in preflight because a
  correlation figure alone does not distinguish a room from any other
  decorrelator, and nothing above made the frozen distance testable. Read the
  twelve direct-path delays through `VoiceEngineTestAccess`. *They are a
  geometry.* They must lie inside **4.4–17.5 ms** (1.5–6 m at 343 m/s) and span
  at least 8 ms end to end, and the direct gain of the farthest singer must sit
  below the nearest by `20·log10(r_far / r_near)` within 0.5 dB, which is what
  makes it 1/r rather than a spread control. *They do not move.* The twelve
  values must be bit-identical after `noteOn(60)`, after a second `noteOn(67)`
  issued while 60 is still sounding, and after Spread and Size are swept 0 → 1
  mid-note. *And nothing bends.* Solo, Humanize 0, Vibrato 0, held C4, with
  Spread and Size swept 0 → 1 over 1 s from t = 0.5 s: the sounding frequency
  read from `soundingFrequencies()` must stay within **1 cent** of its
  pre-sweep value throughout. On a per-note distance the second note-on alone
  moves a shared line by up to 13.1 ms in one control period, which is what
  that cent bound catches.

  `testRoomEarlyReflections` — **preflight correction: the impulse goes in at a
  positioned singer, not into `updateRoom`.** The image sources this step adds
  are per singer and sit upstream of the recirculating network, so an impulse
  injected into `updateRoom` observes only the shared four-tap tail: a correct
  implementation would fail the early-peak count because the peaks are in the
  path the fixture skipped, and an implementation that only scattered extra
  taps inside `updateRoom` — which is not this step at all — would pass it. The
  fixture instead drives one unit sample into a named singer's placement input
  through `VoiceEngineTestAccess`, with every voice silent, and captures the
  wet output. The baseline figures below were measured through `updateRoom`
  and carry over unchanged, because before this step the singer path does not
  exist and the two injection points are the same sample. Times are measured
  from that singer's own direct arrival, since an early reflection is early
  relative to the direct sound and the direct sound now has a delay of its own.

  The first reflection must arrive under 15 ms after the direct sound at Size
  50 % and under 25 ms on the Cathedral Ensemble preset (today 29.67 and
  60.81 ms), with at least 8 distinct local peaks above 10 % of the window peak
  in the first 40 ms — **today 2 at Size 50 % and 0 on Cathedral**, not zero at
  both. Repeated for singers 0 and 11, the first four reflection arrivals must
  differ between the two by more than 1 ms in at least three of the four: a
  shared tap set gives every singer the same pattern, which is the state this
  step is leaving.
  RT60 must stay within 15 % of **0.231 s at Size 50 % / Room 50 % and 0.847 s
  on Cathedral, measured by Schroeder backward integration of that impulse
  response with T20 extrapolated** — the audit's 0.29 s and 1.17 s name no
  method and do not reproduce under this one, and a tolerance without a method
  is not an assertion. Wet/dry balance within 1.0 dB, so this is a geometry
  change and not a new reverb — and that needs a method too, or it is the same
  kind of bare tolerance: the ratio of the RMS of (Room 100 % render minus
  Room 0 % render) to the RMS of the Room 0 % render, Choir/12 held C4 at Size
  50 %, 3 s from t = 1.0 s, must land within 1.0 dB of the value the same
  measurement returns on the pre-step build. `testRoomSizeGeometry` must be
  updated to the new tap set rather than deleted.

### Considered and not planned

- **Disengaging the epilaryngeal cluster above the soprano crossover (gap 23).**
  *Struck in review; it was step 6.* The gap is real — the cluster is applied
  bit-identically at C4, C5 and C6 — but four things were wrong with the step.
  *Its test could not have passed.* It required the 2050–4000 Hz share to rise
  by less than 1.5 dB from Tension 0 to 0.95 at C6. A counterfactual build with
  `epilarynx` forced to zero still rises **8.99 dB**, because Tension also
  drives `chunkResponse_.effortScale` and through it the source tilt corner at
  `:1267`. Nothing short of gutting the tilt reaches 1.5 dB, and gutting the
  tilt is a far larger regression than the gap.
  *Its baseline numbers were the wrong voice.* "1860 → 1065 Hz" and "8.8 dB"
  are what `testSingersFormantCluster` prints for a **male D3**; the female
  AAH figures the step asserts on are 2140 → 1225 Hz and +8.35 dB at C4.
  *It does not implement what it cites.* Weiss, Brown and Morris found sopranos
  have no narrow cluster because their **bandwidths** are "at least 2-kHz
  wide". The step explicitly leaves bandwidths chunk-level, so the
  `clusterWidth` narrowing at `:1012` — 89/132/185 Hz on F3–F5 at Tension 0.95 —
  would still be applied to a soprano at C6. The frequency pull is the half of
  the mechanism that does not carry the citation.
  *It breaks a guarantee this pass pins.* Per-voice F3–F5 targets read against
  chunk amplitudes that `parallelFormantAmplitudes` derived from the *clustered*
  frequencies mis-weights the disengaged voice, which is the peak-normalisation
  the "what must not regress" paragraph exists to protect. Doing it properly
  means two amplitude sets per chunk, i.e. a second
  `parallelFormantAmplitudes` call in the most expensive part of
  `updateChunkState`.
  Also worth recording for whoever picks it up: the 2050–4000 Hz share is a bad
  metric at C6, where only harmonics 2 and 3 of a 1046 Hz fundamental fall in
  the band, so the number moves with which sparse harmonic happens to land
  under F3. This comes back as its own step, with a per-voice bandwidth path,
  a measurement of what a soprano's F3–F5 bandwidths should be, and a test
  written on the tract's frequencies and bandwidths rather than on a band share.
- **The male passaggio and register mechanism.** A tenor tunes R1 to the second
  harmonic through the passaggio and R2 to the third at the top
  ([Journal of Voice,
  2025](https://www.sciencedirect.com/science/article/abs/pii/S0892199725000712)),
  and chest, mixed and falsetto differ in open quotient at the same fundamental
  ([Journal of
  Voice](https://www.sciencedirect.com/science/article/abs/pii/S0892199720304860)).
  `tunedFirstFormant` implements only the soprano strategy, which by
  construction never engages for a male voice below about F5, so between F4 and
  A4 a Vocalor male voice does nothing. This is real and it is the largest
  remaining acoustic omission, but it is a second resonance strategy that has
  to coexist with the first — one raises F1 to meet f0, the other holds or
  lowers it to meet 2f0, an octave lower — and a register model needs a third
  glottal prototype rather than a rebias of the two the engine has. It needs
  its own measurement of where the male F1 sits against 2f0 across the range
  before a target can be stated. A pass of its own.
- **Per-singer phonation dispersion (gap 19).** The right change, and the
  research is direct: the competitor's per-voice character modes are the axis
  Vocalor has no counterpart for. But `tensionAt_[]` and `airLevelAt_[]` are
  engine-wide per-sample arrays consumed by every voice, so per-singer
  phonation means moving them into the voice and paying for it in the hot loop,
  and the audit rates the audible consequence subtle against the timing and
  space work. It belongs in the pass after this one, measured against step 7's
  scatter rather than against today's.
- **Legato transition (gap 15).** A 0.33 ms pitch step at the default Glide is
  wrong, but remapping `glideTimeSeconds` would replace one drawn curve with
  another — and the second half of the audit's argument, that the next dialable
  setting overshoots the natural window, is false: `0.600f * g * g` gives a
  37.5 ms time constant at Glide 25 %, which settles in 86 ms, inside the
  50–150 ms window rather than past it. A sung legato is a transition with an
  amplitude dip and a phonation perturbation at the junction, and getting that
  right needs a decision about whether legato is forced-monophonic in Choir
  mode — a voice-allocator question, which is the same structural reason MPE
  was deferred in the first pass. That, not the glide curve, is why it waits.
- **Chord-mode voicing (gap 16).** An 11-semitone lurch for a one-semitone root
  rise is audible and the fix is not expensive, but it is a voice-leading
  change — choose the inversion nearest the previous chord — and the chord map
  is deliberately stateless so that a rendered chord depends only on its root.
  Making it stateful would break the same guarantee step 7 is written to
  preserve. A continuous register mapping that removes the two integer branches
  is the stateless version and is worth measuring first.
- **Pitch scoop direction (gap 20).** A constant-sign onset gesture is a
  mannerism, but the correct behaviour depends on interval direction, dynamic
  and attack type, and the engine does not currently know the previous sounding
  pitch at note-on outside the legato path. Cheap only after that is available.
- **Subglottal pole-zero pair.** A tracheal branch contributes 5–12 dB of
  attenuation to whichever formant sits near a subglottal resonance
  ([JASA, 2007](https://pubmed.ncbi.nlm.nih.gov/17927433/)), which is what makes
  a chromatic run uneven in a way Vocalor's is not. Perhaps twenty lines, but
  it needs the open-quotient weighting to be per voice, which is the same
  blocker as per-singer phonation.
- **Nonlinear source-filter coupling and open-phase F1 modulation.** Titze's
  level 1 interaction and Klatt's delta-formant grid both belong here, and the
  destabilisation at an f0–F1 crossing is the one place the literature says
  instability is not a defect
  ([JASA
  123(5):2733](https://pubmed.ncbi.nlm.nih.gov/18529191/)). It is deferred
  because it interacts with the tuned-F1 region the previous pass built, and
  because a within-period bandwidth change means resolving the resonators at
  something other than the chunk rate.
- **Key-centre memory and intonation drift.** A held common tone is retuned by
  up to 15.6 cents when the bass moves, because `intonationRoot_` re-references
  rather than being carried
  ([Howard, Journal of
  Voice](https://www.sciencedirect.com/science/article/abs/pii/S0892199705001657)).
  Worth doing, but it means accumulating state across note events, which is the
  determinism question again.
- **Voice type as a function of sounding pitch.** Every competitor is organised
  SATB and `p.profile` is one global switch, so a two-octave chord is sung by
  one tract length. Blending the profile with the sounding note is tempting and
  cheap, but the vowel tables, bandwidths, cluster centre and `chunkMaxF1_` are
  all resolved per chunk from that one boolean, so it is the same per-voice
  restructuring as gap 19.
- **Per-singer directivity.** A high shelf on the room send by azimuth is the
  cheap approximation of mouth-aperture directivity above 5.6 kHz, but the
  measurement paper's own finding is that a singer's projection gestures barely
  change the radiation pattern, and the payoff is second-order to step 8's
  distance work.
- **The wavetable ladder (gap 22).** Nothing audible rides on it: energy above
  11.2 kHz is 111.1 dB down at MIDI 51 and 143.3 dB down at MIDI 53. Recorded
  so that a future step which brightens the source knows to check it — and note
  that the step is 32 dB, not the 13 dB the audit's second figure implied.
- **A voiced-nasal onset.** The nasal branch from the first pass makes a
  `/m…a/` entry reachable with no phoneme inventory — briefly close the velum at
  attack, then open it. Attractive, but it is a new articulation control on top
  of a pass already carrying two structural changes, and the competitive
  evidence says spacing rather than consonants is what earns the praise.
- **A published listening test.** No vendor in this category publishes one, and
  no controlled comparison of choir plug-ins exists. Generating one — a small
  forced-choice test against rendered competitor demos — would be a genuine
  differentiator, but it is a marketing artefact rather than an engine step and
  does not belong in this list.
