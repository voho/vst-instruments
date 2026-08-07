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
