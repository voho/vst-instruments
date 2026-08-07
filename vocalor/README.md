# Vocalor

Vocalor is a real-time vocal instrument for macOS. It turns MIDI notes into
original sung vowels, from a single singer to an ensemble or major/minor chord.
Three preset vowels anchor a **continuous vowel space** that can be morphed,
automated, and played from a draggable pad, and the whole vocal tract can be
lengthened or shortened independently of pitch. The engine is procedural and
runs locally: it does not clone a named singer, load recordings, or contact a
service while rendering audio.

> **Listen first.** Nine [rendered demonstrations](Docs/audio/README.md) cover
> a solo legato phrase, the preset vowels on both voice profiles, the vowel
> space and formant shift on held notes, the twelve-singer ensemble, chord mode
> locking onto just intervals, a choir swelled on the Dynamics control, tension
> and breath closing into a hum, and the room at both sizes. They are rendered
> by the shipping engine, so they cannot drift from what the plug-in does.

![Vocalor Standalone instrument interface](Docs/screenshots/vocalor-standalone.png)

The screenshot above was captured from the 1.0 Standalone application and shows
the previous layout. The 1.1 editor added the vowel-space pad, the live
vocal-tract analyser with output meters, and the phrasing and room-geometry
controls; 1.2 adds Dynamics, Intonation and Nasal to the same row and regroups
it into voice character, performance, space and master. The visual language is
unchanged. It has not been re-captured because the screenshots can only be
produced from a macOS build. The VST3 and Audio Unit use the same resizable
JUCE editor.

The interface remains fully resolution-independent: the layered hardware knobs,
choice states, panel depth, vowel pad, response curve, and complete vocal-range
keyboard are drawn as native JUCE graphics. This keeps labels and interaction
crisp while resizing and avoids bitmap controls that would obscure automation or
focus state.

The project builds three products from one JUCE codebase:

- VST3 instrument for hosts such as Ableton Live, REAPER, Cubase, and Bitwig
- Audio Unit v2 music device for Logic Pro and GarageBand
- Standalone application for direct MIDI-keyboard testing

> **Just want to try it?** The scheduled Nightly workflow publishes the latest
> successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> The bundles are ad-hoc signed and not notarized; check the repository's Nightly
> badge for the latest workflow result.

## Interface and controls

Vocalor exposes 23 automatable host parameters. Every parameter keeps its
identifier, range, default, and host ordering, so existing sessions and any
automation written against them recall in place.

**Ensemble size** advertises 2 – 16 but used to quantise to three tiers of 4, 8,
or 12 singers, so ten of its fifteen settings were silently identical to a
neighbour. Every value from 2 to 12 is now rendered exactly. The engine carries
12 singer identities, so 13 – 16 still render 12 — the published range is kept
at 2 – 16 rather than narrowed, because a host stores automation as a normalised
0 – 1 position and renumbering the range would move every existing lane onto a
different singer count.

| Parameter ID | Control | Range | Default | Added |
| --- | --- | --- | --- | --- |
| `profile` | Voice profile | Female / Male | Female | 1.0 |
| `mode` | Performance mode | Solo / Choir / Chord | Solo | 1.0 |
| `vowel` | Vowel anchor | AAH / OOH / UUH | AAH | 1.0 |
| `chordQuality` | Chord quality | Major / Minor | Major | 1.0 |
| `choirSize` | Ensemble size | 2 – 16 (13 – 16 render 12) | 8 | 1.0 |
| `breath` | Breath | 0 – 100 % | 30 % | 1.0 |
| `resonance` | Resonance | 0 – 100 % | 64 % | 1.0 |
| `vibrato` | Vibrato | 0 – 100 % | 38 % | 1.0 |
| `humanize` | Humanize | 0 – 100 % | 52 % | 1.0 |
| `spread` | Stereo spread | 0 – 100 % | 62 % | 1.0 |
| `tension` | Vocal tension | 0 – 100 % | 36 % | 1.0 |
| `room` | Room | 0 – 100 % | 24 % | 1.0 |
| `output` | Output | −24 – +6 dB | −6 dB | 1.0 |
| `legato` | Legato | Off / On | Off | 1.1 |
| `vowelX` | Vowel front-back | 0 – 100 % | 50 % | 1.1 |
| `vowelY` | Vowel open-close | 0 – 100 % | 50 % | 1.1 |
| `vowelMorph` | Vowel morph | 0 – 100 % | 0 % | 1.1 |
| `formantShift` | Formant shift | −12 – +12 st | 0 st | 1.1 |
| `glide` | Glide | 0 – 100 % | 0 % | 1.1 |
| `roomSize` | Room size | 0 – 100 % | 50 % | 1.1 |
| `dynamics` | Dynamics | 0 – 100 % | 100 % | 1.2 |
| `intonation` | Just intonation | 0 – 100 % | 0 % | 1.2 |
| `nasal` | Nasality | 0 – 100 % | 0 % | 1.2 |

The top row selects the voice profile, the performance mode, the chord quality,
and the vowel anchor. **Ensemble size** renders exactly that many independently
humanised singers, from 2 to 12 — the number of distinct singer identities the
engine models — and Chord renders six singers distributed across the triad.
**Legato** switches between retriggering every note and bending the sounding
voices into the new pitch, with a held-note stack so releasing the top of a
phrase falls back to the note underneath.

The **vowel-space pad** places a morph target anywhere between five cardinal
vowels (front to back on the horizontal axis, close to open on the vertical
axis). **Morph** crossfades from the selected preset vowel to that target, so
at 0 % the pad and its two axes have no effect at all and the vowel anchor
alone decides the tract. **Shift** moves every formant by up to an octave in either
direction without touching the pitch, which is an effective vocal-tract-length
or body-size control; it changes the timbre without doubling as a fader. Next
to the pad, the **vocal-tract response** display draws the live magnitude
response of the five formant resonators of a sounding voice on a logarithmic
axis, marks F1 to F5, and carries the stereo output meters.

Thirteen continuous controls fill the bottom row in four groups. **Voice
character** holds **Breath**, **Resonance**, **Tension**, **Nasal**,
**Vibrato** and **Humanize**; **performance** holds **Dynamics**,
**Intonation** and **Glide**; **space** holds **Spread**, **Room** and room
**Size**; and **master** holds the **Output** level. The status display reports
active voices and sample rate, the Panic button mutes immediately, and the
on-screen keyboard is also mapped to the computer keys shown above it. The
editor's minimum width rose from 1020 to 1120 px with the three added knobs, so
that a cell still holds the value box under its dial.

## Factory presets

Vocalor ships twelve factory programs, published through the host's own program
interface: Audio Unit factory presets in Logic, the program list in a VST3 host.
Program 0 is the shipping default sound, so a host that opens on the first
program opens on what the plug-in opens on. The rest cover the solo voice
(intimate, pressed, legato, breath), the ensembles (bass choir, cathedral,
closed-mouth hum, small voices, morph pad) and the chord modes (locked major
chorale, airy minor pad).

The table itself lives in
[`Source/DSP/Presets.cpp`](Source/DSP/Presets.cpp), inside the JUCE-free core
rather than in the processor, so the DSP test suite renders every one of them
and checks that each is finite, audible, bounded, releases fully, and carries no
value the engine would clamp away. See
[`Presets/README.md`](Presets/README.md) for the full list.

## Performance expression

Vocalor 1.1 responded to note-on, note-off, and CC 120/123 and discarded every
other MIDI message, pitch bend included. The whole dynamic response of a note
was decided by its note-on velocity and never moved again. 1.2 adds the
continuous performance inputs the instrument is actually played from:

| Message | Effect |
| --- | --- |
| Pitch bend | ±2 semitones on every sounding and subsequent voice |
| CC 1 (mod wheel) | Dynamics |
| Channel pressure | Dynamics — the same control, so a controller with only one of the two is fully expressive |
| CC 11 (expression) | Output trim, applied after the room so a swell shapes the tail with the dry signal |
| CC 64 (sustain) | Holds note-offs; pedal-up delivers each of them through the ordinary release path |
| CC 121 | Lifts the pedal and returns bend, wheel and expression to neutral |
| CC 120 / CC 123 | All sound off / all notes off, as before |

**Dynamics** is the control the instrument is meant to be played from, and it is
not a fader. Falling from full to empty it takes 18.1 dB off the voiced source
but only 7.2 dB off the aspiration, so a soft note is proportionally breathier;
it lowers the vocal effort, which drops the source tilt corner and therefore
dulls the note; it relaxes the glottal pulse toward the lax prototype, which is
a change in the pulse shape rather than a filter over a fixed one; and it
narrows the vibrato. Measured on a held middle C, dropping the dynamic from 100 %
to 30 % costs 10.9 dB at the fundamental and 13.9 dB across 2.4 – 4.7 kHz: the
presence band falls 3 dB further than the level does. The test suite asserts
that difference, because a dynamic that moved both by the same amount would be
an output trim wearing a different name.

The `dynamics` host parameter owns the level until a mod wheel or channel
pressure message arrives; from the first such message the controller owns it,
and CC 121 hands it back. A session that predates 1.2 recalls with the parameter
at 100 %, which is exactly the 1.1 behaviour.

## Sound engine

The JUCE-free C++20 DSP core uses a low-latency, source-filter vocal model.

**Glottal source.** The excitation is a band-limited Liljencrants-Fant style
glottal flow derivative rather than an arbitrary harmonic roll-off. Two
prototypes — a lax pulse (open quotient 0.78, speed quotient 2.6, long return
phase) and a firmly adducted one (0.46, 3.4, short return phase) — are analysed
once at `prepare()` time and rendered into nine band-limited mip levels. Vocal
tension crossfades between them, so the control changes the shape of the pulse
and the resulting spectral tilt for a physical reason instead of interpolating
two hand-tuned spectra. A one-pole tilt driven by velocity and tension is
applied on top, which is why a soft note is now dull as well as quiet.

**Vocal tract.** Five two-pole resonators in parallel model the formants. The
three preset vowels anchor a continuous inverse-distance-weighted vowel space,
and every formant target is resolved once per 64-sample chunk and then shaped
per voice by that singer's tract-length and per-formant dispersion, by
note-dependent formant tuning, and by the shared ensemble drift.

Each formant then glides to that target on its own timescale, because the
articulators that set them differ in mass: the jaw carrying F1 cannot change a
vowel as quickly as the tongue tip and larynx that set the formants above it.
Tension narrows the epilarynx tube, which clusters the upper formants into the
singer's formant rather than brightening the whole tract. **Coarticulation** and
**Singer's formant** below give both mechanisms and their measurements.

Radiation from the lips is not a separate stage. The excitation is already a
glottal flow *derivative*, and differentiating the flow is precisely what lip
radiation does to it, so the model accounts for it once at the source.

Each resonator is normalised to unit gain at its own centre frequency, adjacent
formants alternate in polarity, and the five amplitudes are derived from the
all-pole cascade the same poles would realise, with half of that cascade's
absolute gain compensated. Those three properties are what make the bank behave
like a tract rather than like five independent peaking filters:

- **The level no longer follows the sample rate.** An unnormalised two-pole
  resonator's peak height is proportional to the sample rate. The same patch
  used to measure 12.7 dB louder at 192 kHz than at 44.1 kHz, harmonic for
  harmonic; it now holds to within 0.05 dB across 44.1, 48, 88.2, 96 and
  192 kHz.
- **The vowel pad and the formant shift are timbre controls.** Peak height also
  ran inversely with centre frequency, so the level fell monotonically as the
  formants rose: the pad swung 16 dB across its corners and shifting an octave
  up cost 17.5 dB against an octave down. The trend is gone. On a middle C the
  pad now spans 7.5 dB and the two octave extremes differ by under 1 dB; what
  remains is the genuine interaction between the fundamental and where F1
  happens to land, which is note dependent rather than a fader.
- **The bank no longer cancels between its formants.** Summed with a common
  sign, adjacent resonators are close to antiphase in the valley between them.
  On a close front vowel that dug a 64 dB notch between F1 and F2, tens of dB
  deeper than any vocal tract produces; alternating the polarity brings it to
  32 dB. Cascade-derived amplitudes are also why a front vowel now sounds
  front: F2 and F3 of /i/ are carried nearly as strongly as F1 instead of
  sitting 22 dB below it.

**Singer's formant.** Vocal tension used to raise the amplitude of F3 by 12 %
and of F4 by 6 % and leave them where they were. Three formants 700 Hz apart
with a little more gain each are still three formants: the spectral peak at
2.5 – 3.5 kHz that lets an unamplified voice carry over an orchestra comes from
narrowing the epilaryngeal tube until F3, F4 and F5 collapse into one. Tension
now does that — it pulls the three upper formants toward a profile-dependent
cluster centre (2.9 kHz male, 3.2 kHz female, scaled with the formant shift like
the rest of the tract) and narrows their bandwidths together. Vocal effort
strengthens the same configuration, so the dynamic reaches it as well.

On a held male D3 the F3-to-F5 span closes from 1860 Hz to 1065 Hz as Tension
goes from 0 to 95 %, and the share of the output between 2.05 and 4 kHz rises
8.8 dB. At Tension 0 the upper formants sit exactly where the vowel puts them.

**Coarticulation.** A vowel change is a jaw and a tongue moving. The formant
glides ran on 16, 9, 5, 4 and 3 ms time constants, which put a whole vowel
switch inside about 50 ms — a de-zipper rather than an articulation, where a
sung vowel-to-vowel transition runs 100 – 200 ms. Each formant now has a speed
rather than a deadline: a move of a quarter of that formant's own nominal
frequency or more takes the full jaw-and-tongue time, and anything smaller is
proportionally quicker, so the transition also accelerates as it closes on the
target instead of crawling into it. The lower formants stay the slowest, since
they follow the larger cavity adjustments.

Stepping the pad from the close-front corner to the open one on a held A3 gives
10 – 90 % rise times of 117 ms for F1, 92 ms for F2, 28 ms for F3 and 21 ms for
F4. F3 and F4 are quicker because their targets move 500 and 250 Hz where F1
moves 540 and F2 1570 — a small adjustment is a small adjustment. A 6 % pad
nudge takes F1 45 ms rather than the full 117.

**Nasal branch.** A parallel bank of poles does have zeros, but they land
wherever its sections happen to cancel; there is no way to place one. That ruled
out the nasal branch, and therefore ruled out a hum — the most common choir
colour after "ah". **Nasality** opens the velum: it adds a murmur pole at the
nasal cavity's own resonance (280 Hz, heavily damped at 150 Hz of bandwidth),
places a notch at 950 Hz where the closed mouth loads the tract, and drops the
oral formants to 45 %, because a closed mouth is a side branch rather than the
radiator. Both frequencies scale with the formant shift, since the nose belongs
to the same head as the tract.

The notch is a matched pole-zero pair rather than Klatt's bare antiresonator.
That antiresonator is normalised to unity at DC, which for a zero this low
leaves 48 dB of gain at Nyquist and would make a hum the brightest sound the
instrument produces; the matched pole returns the response to unity either side
of the notch, so the branch removes only the band it names. A trim for the
nostrils — a far smaller and more damped aperture than an open mouth — keeps the
control from doubling as a fader.

Measured on a held male A2 with the velum fully open: 880 – 1100 Hz falls
27.8 dB, 220 – 330 Hz rises 7.5 dB, 1.65 – 2.2 kHz falls 11.2 dB, and the
overall level moves 0.1 dB.

**Intonation.** An a cappella ensemble does not sing equal temperament. It
narrows the major third and widens the minor one until the overtones align,
which is the "ring" of a locked chord. Chord mode stacked equal-tempered
semitones, so a one-finger triad beat where a real section locks. **Just
intonation** blends each sounding voice from equal temperament toward the
five-limit interval it makes with the lowest sounding root: 13.7 cents narrow
for the major third, 15.6 wide for the minor, 2.0 wide for the fifth. The
octave is left alone, because it is the same either way.

It applies to played polyphony as well as to chord mode, and the reference
follows the bass: play the third and the fifth first and add the root
underneath, and the two upper voices re-tune onto it. They do not snap — the
adjustment glides over about 90 ms, which is roughly how long a singer takes to
hear the beating and move. The control defaults to 0 %, so a session written
before it existed recalls in equal temperament.

**Formant tuning.** A speech tract keeps F1 where the vowel puts it; a singer
cannot. Once the fundamental climbs past F1 the whole spectrum sits above the
lowest resonance, the radiated power collapses and the timbre breaks, which is
why sopranos open the jaw and take F1 up with the pitch. The 1.1 engine raised
F1 by a flat 0.32 % per semitone above A4, so a female OOH at C6 kept F1 at
367 Hz with the fundamental at 1047 Hz. The engine now resolves F1 per voice
against that voice's own pitch: below the fundamental the vowel is returned
untouched, the strategy fades in as the fundamental comes up on F1 and is
complete 15 % above it, and F1 stops at the highest one that jaw reaches
(1.55 × the profile's open vowel, scaled with the formant shift, because a
shortened tract reaches proportionally higher). F2 is pushed clear of a tracked
F1 rather than being crossed by it, which is why a soprano's vowels lose their
identity at the top — as real ones do.

Two things follow, both asserted by the test suite:

- **The vowel no longer decides whether the top octave exists.** At C6 the
  close anchor measured 25.1 dB below the open one and 20.1 dB below itself at
  C4. It now measures 7.0 dB below the open anchor and 0.8 dB *above* itself at
  C4. The residual is the cascade amplitude weighting, which is still resolved
  once per chunk for the untuned tract and therefore still knows the close vowel
  by its speech formants.
- **The resonance it wins is spent on efficiency, not on volume.** Putting the
  fundamental exactly on F1 is worth 7 – 12 dB, which would leave the top octave
  shouting over the middle. A singer uses that to reduce subglottal pressure
  instead, so the voice gain is trimmed in proportion to how far F1 actually had
  to move: nothing while F1 has not moved, and 6 dB once it has moved by a fifth
  or more.

**Resonance** sets the formant bandwidths and nothing else. Because narrower
formants make the cascade peakier, it now widens the contrast between the
formant amplitudes instead of simply raising the output level.

**Aspiration.** Breath noise is injected at the glottis and passes through the
same tract as the voiced excitation, which is both more faithful than a separate
noise filter and two resonators per voice cheaper. A small unfiltered component
keeps the consonantal air audible. The noise is scaled by the square root of the
sample rate so its density in the audio band, rather than its power per sample,
is what the Breath control sets.

**Sample rate.** Every filter coefficient in the engine is derived from a corner
frequency or a time constant at `prepare()` time: the room damping and low cut,
the aspiration pre-emphasis, the source tilt, the shimmer and pitch-jitter
smoothers, the control-rate formant and pan glides, and the envelope and drift
rates. Correct coefficients are only half of it. A one-pole smoother driven by
white noise settles at output variance `c / (2 - c)`, so once `c` is tied to the
sample rate the shimmer and the pitch jitter arrive with the right spectrum at
the wrong depth: uncompensated they measured 6.5 dB and 6.2 dB smaller at
192 kHz than at 44.1 kHz. Both drives are renormalised against a 48 kHz
reference, as is the unsmoothed aspiration noise.

Two tests cover this, because one cannot. The five-rate render measures the
tract alone — its patch sets humanisation to zero, so it says nothing about the
shimmer or the jitter — and compares the overall level and individual harmonic
magnitudes. A second test holds a note at full humanisation and measures both
halves of the problem at 44.1, 48, 96 and 192 kHz: the standard deviation of the
shimmer's modulation depth and of the jitter's pitch deviation, which agree to
within 0.6 dB, and their autocorrelation at a fixed 4 ms lag, which agrees to
within 0.025 and is what pins the spectrum. With the old fixed per-sample
coefficients that correlation ran from 0.33 at 44.1 kHz to 0.003 at 192 kHz.

**Parameter smoothing.** Resonance and formant shift reach the pole radius,
which cannot be smoothed after the filter has run, so both are smoothed at the
chunk rate before the coefficients are built; the bandwidth scale reads the
per-sample breath smoother at the same chunk boundary. Formant targets, pan
gains, breath, tension, room mix and output gain are smoothed downstream of
that.

**Humanisation.** Each singer has slowly moving pitch, vibrato rate and depth,
spectral balance, breath, and timing rather than sharing a perfectly periodic
LFO. Pitch jitter runs through two nested smoothers for a 1/f-like spectrum, and
ensemble and chord modes instantiate independent singers, preventing the
phase-locked oscillator sound common to simple choir patches.

**Ensemble dispersion.** How far apart twelve singers sit is what separates a
section from one thick voice, and Vocalor's section used to be far too tight.
Each singer carried a uniform ±5.6 cents of static detune — 3.2 cents of
standard deviation for the distribution it drew from, and 4.4 cents measured
across the twelve identities at full Humanize. Jers and Ternström measured
25 – 30 cents of dispersion between real choir singers, and listeners were
reported to tolerate 14 cents of standard deviation, so the section was running
at roughly a third of what it was allowed and a quarter of what a real one does.

The static detune is now drawn from a triangular distribution — two hashes
averaged, which concentrates the section near the target rather than spacing it
evenly across the extremes, as a real one does — and each singer carries two
incommensurate slow wanders instead of one, so the section never returns to the
same relative configuration. Twelve singers now measure 12.6 cents of standard
deviation at full Humanize and the largest singer moves 10.4 cents over nine
seconds. Humanize is the dial between the two: at zero the section is perfectly
locked, at its 52 % default it sits at about 6 cents, and at full it is a
rehearsal room.

**Room.** The four-tap cross-coupled network reads through interpolated,
slowly modulated taps that break up the metallic ringing a static comb produces
on sustained vowels, and a gentle low cut keeps the tail out of the low mids.
Room size scales both the tap geometry and the feedback, so a large room spaces
its reflections further apart and rings far longer than a booth; it reaches
exactly the historical geometry at its 50 % default. The damping filter in the
feedback path is now anchored to a fixed corner frequency, so the tail decays at
the same rate at every sample rate.

This is a synthesizer, not a speech model or voice-cloning system. It is suited
to sustained vowels and expressive musical parts; it does not generate words.

## Performance

The peak-normalised formant bank is a strictly more expensive filter than the
one it replaces — every voice now derives its own feed-forward gain per
formant — and the engine is still slightly faster than before, because the work
that was being repeated needlessly was removed at the same time. Measured with
an offline benchmark on one core, `-O3`, 48 kHz, 128-sample blocks, best of four
two-second runs:

| Case | Before | After | Change |
| --- | --- | --- | --- |
| Solo, one note | 46.6 ns/sample | 45.0 ns/sample | −3.4 % |
| Solo, six notes | 161.2 ns/sample | 157.6 ns/sample | −2.2 % |
| Choir, 12 singers | 290.9 ns/sample | 288.4 ns/sample | −0.9 % |
| Choir, 12 singers × 4 notes | 1100.4 ns/sample | 1075.9 ns/sample | −2.2 % |
| Chord, 6 singers × 3 notes | 437.8 ns/sample | 426.8 ns/sample | −2.5 % |
| Fully idle | 2.5 ns/sample | 1.6 ns/sample | −36 % |

The savings come from resolving the tract only when one of its inputs actually
moves (a held note re-ran seven exponentials and a five-by-five cascade
evaluation every 64 samples for an answer that had not changed), from sampling
the ensemble-drift oscillators once per chunk instead of once per voice control
update, from advancing drift only for the singer identities a note actually
uses, from caching the vocal-effort and pan coefficients against their inputs,
and from folding the formant amplitude, polarity and peak normalisation into a
single per-resonator coefficient so the render loop multiplies once instead of
twice per formant per sample.

Earlier work that still stands: voices render voice-major over aligned 64-sample
chunks, the two glottal wavetables are interleaved so the oscillator touches half
as many cache lines, the aspiration path runs through the tract rather than
through its own filter, and the wavetables are built from the sine table instead
of two million `sin()` calls, which keeps `prepare()` at a few milliseconds.

Chunk boundaries are aligned to the absolute sample position, so the rendered
output is bit-identical no matter how a host splits its buffers; the test suite
asserts this.

The recursive filter states are kept out of denormal range by a vanishingly
small DC bias on the tract excitation rather than a per-tick compare, which is
worth about a third of the per-voice budget on its own, and the room network
clears itself once its tail is provably inaudible.

**What 1.2 costs.** The table above predates it and has not been re-measured,
because the offline benchmark that produced it is not in this repository. What
can be compared is the test suite's own twelve-singer 96 kHz render, which runs
the same harness on both engines: best of three, 493.6 ns/sample before against
500.5 after, or about 1.4 % — the difference is close to the run-to-run spread.

That is deliberate. The dynamic response is resolved once per chunk and its two
gains are folded into the per-sample voiced and aspiration level arrays that
already existed, so the render loop is unchanged. Formant tuning and the
epilarynx cluster are control-rate arithmetic and the efficiency trim is folded
into a per-voice gain the loop already reads. Coarticulation added a multiply
against a precomputed reciprocal rather than a division. The nasal branch is the
one addition that costs per-sample work — a biquad and a resonator per voice —
and it is skipped entirely on a chunk-constant branch while the velum is closed,
which is its default.

## Requirements

- macOS 11 or newer for running the built products
- A current full Xcode installation selected for command-line use
- CMake 3.22 or newer
- Internet access for the default first configure, or a local JUCE 8.0.14
  checkout supplied through `JUCE_PATH` to the helper
  (`VOCALOR_JUCE_PATH` when configuring CMake directly)

JUCE 8.0.14 is fetched at configure time and is not vendored into this
repository.

## Build on macOS

The helper creates an Xcode build, compiles universal `arm64`/`x86_64`
binaries, and runs the DSP tests:

```bash
./scripts/build-macos.sh
```

Equivalent commands, useful when opening and developing in Xcode, are:

```bash
cmake -S . -B build-macos -G Xcode \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DVOCALOR_BUILD_UNIVERSAL=ON \
  -DVOCALOR_BUILD_PLUGIN=ON \
  -DBUILD_TESTING=ON

cmake --build build-macos --config Release --parallel
ctest --test-dir build-macos -C Release --output-on-failure
open build-macos/Vocalor.xcodeproj
```

To avoid the FetchContent download, point the configure at a local checkout of
the exact JUCE release:

```bash
JUCE_PATH="$HOME/SDKs/JUCE-8.0.14" ./scripts/build-macos.sh
```

For a native-only development build, use `BUILD_UNIVERSAL=OFF`. Override
`BUILD_DIR`, `CONFIG`, or `MACOSX_DEPLOYMENT_TARGET` in the environment when
needed.

The Release bundles are written to:

| Format | Build artifact |
| --- | --- |
| VST3 | `build-macos/Vocalor_artefacts/Release/VST3/Vocalor.vst3` |
| Audio Unit | `build-macos/Vocalor_artefacts/Release/AU/Vocalor.component` |
| Standalone | `build-macos/Vocalor_artefacts/Release/Standalone/Vocalor.app` |

## Run the DSP tests without JUCE

The core is deliberately independent of JUCE. This gives a quick test path on
any C++20 development machine without downloading the application framework:

```bash
cmake -S . -B build-dsp \
  -DCMAKE_BUILD_TYPE=Release \
  -DVOCALOR_BUILD_PLUGIN=OFF \
  -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The test executable renders solo, choir, major-chord, and minor-chord notes at
44.1, 48, and 96 kHz. It checks that held and released notes produce finite,
non-silent audio, that the release decays, and that a short offline render does
not exceed a generous performance guardrail. It also covers the vowel-space
model and the display maths, proves that the vowel pad is inaudible while morph
is at zero, checks the formant-shift scaling, the glide and legato note stack,
the room tap geometry and tail length, the absence of denormal state during a
long release, the behaviour under non-finite parameters, and that a full-range
parameter jump glides instead of stepping.

The formant bank has its own checks, since it is where the audible defects were:
the rendered level and the individual harmonic magnitudes must agree across
44.1, 48, 88.2, 96 and 192 kHz; the depth *and* the correlation time of the
shimmer and of the pitch jitter must likewise agree across rates, measured with
humanisation at full so the noise-driven smoothers are actually running; the
vowel pad and the formant shift
must not move the level more than a bounded amount; every resonator must measure
unit gain at its own centre frequency for every bandwidth and sample rate; the
onset stage must measure the same peak gain as the main tract it crossfades
into, despite running wider bandwidths; the bank must not cancel between F1 and
F2 at any of three vowel corners; an ensemble of *n* must render *n* singers; and
a resonance or formant-shift jump must glide the pole radii rather than step
them.

Everything 1.2 adds is asserted numerically rather than described, because each
of these controls is one that could easily have been a fader instead:

- the dynamic must roll the 2.4 – 4.7 kHz band off by more than it drops the
  fundamental, and must raise the aspiration-to-voiced ratio as it falls; a
  ±2 semitone bend must produce the ±2 semitone frequency ratio; the sustain
  pedal must hold a note through its note-off and release it on pedal-up;
  expression must reproduce the same render at half the amplitude sample for
  sample; and the mod wheel must take the dynamic over for good the first time
  it moves
- F1 must track the fundamental once the fundamental passes it, stop at the
  jaw's ceiling, and leave the vowel alone below that; the level at C6 must
  neither collapse against C4 nor shout over it
- the just intervals must measure their own ratios to within a cent, in chord
  mode and in played polyphony, and must re-tune when a bass arrives after them,
  gliding rather than stepping
- twelve singers must disperse inside the band real choirs measure, lock
  perfectly at zero Humanize, and drift without running away
- the nasal branch must cut its own band by 20 dB or more, raise the murmur
  band, leave the overall level alone, and stay stable at every coupling
- a full vowel step must produce 10 – 90 % formant rise times inside a stated
  window, ordered by the cavity that carries each formant, and a small step must
  settle in a fraction of the time
- the epilarynx must contract the F3-to-F5 span and raise the 2 – 4 kHz share,
  and must leave the vowel's own upper formants alone at rest
- every factory preset must carry values the engine does not clamp away, render
  finite, audible, bounded audio on a held note and a held interval, and release
  fully

This catches regressions; it is not a substitute for listening tests, host
automation tests, or profiling on the oldest supported Mac. The suite also
smoke-tests the demonstration renderer. The JUCE-side test, which builds only on
macOS, additionally covers the new MIDI messages reaching the engine and the
factory bank reaching the host parameters.

## Regenerate the demonstration audio

```bash
cmake --build build-dsp --parallel --target VocalorRenderDemos
./build-dsp/VocalorRenderDemos Docs/audio
```

The render is deterministic and the tool rewrites the level table in
[`Docs/audio/README.md`](Docs/audio/README.md) in place, so the committed audio
and its documented levels stay in lockstep with the code. See that file for the
full manifest.

## Install locally

For per-user installation, copy only the formats you need:

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"

ditto build-macos/Vocalor_artefacts/Release/VST3/Vocalor.vst3 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Vocalor.vst3"
ditto build-macos/Vocalor_artefacts/Release/AU/Vocalor.component \
  "$HOME/Library/Audio/Plug-Ins/Components/Vocalor.component"
```

Standard discovery locations are:

| Scope | VST3 | Audio Unit |
| --- | --- | --- |
| Current user | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |
| All users | `/Library/Audio/Plug-Ins/VST3/` | `/Library/Audio/Plug-Ins/Components/` |

The standalone app can be copied to `/Applications` or launched directly from
the artifacts directory. Quit and reopen the host after installing. If Logic
retains an older AU cache during development, log out and back in or restart
the Audio Component Registrar before rescanning.

## Validate the plug-in

Run the DSP tests first, then validate the actual bundles. The AU component IDs
configured by this project are type `aumu`, subtype `Vcl1`, and manufacturer
`Vclr`:

```bash
auval -v aumu Vcl1 Vclr
```

With [pluginval](https://github.com/Tracktion/pluginval) installed, validate
the VST3 at the highest strictness level:

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 10 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Vocalor.vst3"
```

Also test note overlap, rapid note-off, parameter automation, project-state
recall, sample-rate changes, and buffer sizes from 32 to 2048 samples in at
least two hosts. A validator passing does not guarantee musical or host-level
correctness.

## Sign, package, and notarize

For local testing, the packaging helper uses ad-hoc signing by default:

```bash
./scripts/sign-and-package-macos.sh
```

It stages the VST3, AU, and standalone app, verifies their signatures, and
creates a ZIP and installer package under `build-macos/dist/`. The current
helper's filenames use the `macOS-universal` suffix, so run it only after the
default universal build and confirm all three executables report both `arm64`
and `x86_64` with `lipo -archs` before publishing.

For public distribution, first import valid `Developer ID Application` and
`Developer ID Installer` certificates. Store notarization credentials once in
the login keychain; do not put credentials in this repository:

```bash
xcrun notarytool store-credentials vocalor-notary \
  --apple-id "developer@example.com" \
  --team-id "YOURTEAMID" \
  --password "APP-SPECIFIC-PASSWORD"
```

Then sign, package, submit, wait for Apple's result, and staple the ticket in
one command:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Company (YOURTEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Company (YOURTEAMID)" \
NOTARY_PROFILE="vocalor-notary" \
./scripts/sign-and-package-macos.sh
```

With `NOTARY_PROFILE` set, the helper submits and staples the installer package.
The ZIP still contains signed bundles but is not itself the notarized
distribution artifact.

Before publishing, verify the package from a clean user account and inspect it:

```bash
pkgutil --check-signature build-macos/dist/Vocalor-1.1.0-macOS-universal.pkg
spctl --assess --type install --verbose=4 \
  build-macos/dist/Vocalor-1.1.0-macOS-universal.pkg
```

The placeholder bundle identifier and four-character manufacturer/plugin codes
must be replaced with identifiers controlled by the publisher before a public
release. Once released, do not change those identifiers: hosts use them to
recall the correct plug-in.

## Project layout

```text
Source/DSP/              JUCE-free synthesis engine
Source/PluginProcessor.* MIDI, parameters, state, and audio bridge
Source/PluginEditor.*    Keyboard and editor UI
Tools/RenderDemos.cpp    Renders the committed demonstration WAVs
Docs/                    Rendered demonstrations, screenshots, documentation
Tests/                   JUCE-free DSP regression tests
Source/DSP/Presets.cpp   Factory preset table, rendered by the test suite
Presets/                 Factory preset documentation
scripts/                 macOS build and release helpers
```

## Licensing

The original Vocalor source is offered under the MIT License. JUCE is a separate
dependency and is **not** covered by that license. JUCE 8 framework modules are
available under the AGPLv3 or a commercial JUCE licence. Distributing a closed-
source or otherwise AGPL-incompatible binary generally requires an appropriate
commercial JUCE licence. Confirm the current terms for the publisher and use
case before shipping; see `THIRD_PARTY_NOTICES.md` and JUCE's official licence.

No neural model weights, voice datasets, samples, or third-party presets are
included. If those are added later, document their provenance and redistribution
rights before committing or packaging them.
