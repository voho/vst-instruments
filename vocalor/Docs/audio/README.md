# Vocalor demonstration audio

Ten rendered examples of what Vocalor produces: a solo legato phrase, the
three preset vowels on both voice profiles, the continuous vowel space and the
formant shift each explored on a single held note, the twelve-singer ensemble,
chord mode's one-finger harmony locking onto just intervals, a choir phrase
sung from pianissimo to forte and then swelled on the Dynamics control, the
glottal tension and breath controls closing into a hum, and the room at both
ends of its size range. The final take carries one soprano from C4 to C6 through
the register-dependent upper-tract release that none of the original nine
examples reached, with the same bounded breath-support response the plug-in
uses when a harmonic crosses a tract resonance.

Every file here is rendered by [`Tools/RenderDemos.cpp`](../../Tools/RenderDemos.cpp)
from the shipping JUCE-free signal path — the same `VoiceEngine` the VST3, Audio
Unit and Standalone run — so a demonstration cannot drift away from what the
plug-in actually sounds like. No samples, recordings, impulse responses or
external processing are involved anywhere in this directory: the whole set comes
out of the source-filter model, including the room tail and the stereo image,
which is a stage of individually panned singers rather than a widener.

The renderer spells out every setting instead of inheriting the DSP struct's
backward-compatibility defaults. It starts from the published plug-in values,
with a deliberately less-resonant neutral baseline at **Resonance 42 %** and
**Tension 32 %**, **Instability 44 %**, and a reduced 40 % output for headroom.
Vibrato chooses the intended extent; Instability adds bounded deterministic
cycle-to-cycle variation in period, depth and contour, while Humanize controls
ensemble identity, timing and vocal-tract differences. Against the old demo
baseline, the female AAH F3–F5 bandwidths widen from
106.6/157.8/221.8 Hz to approximately 133.2/197.2/277.1 Hz. More importantly,
the female profile no longer inherits one narrow lower-voice cluster: the outer
F3–F5 coverage stays about 2 kHz wide at low and middle pitches, then releases
smoothly from E-flat5 to B-flat5. That structural proxy is not an individual
resonator bandwidth or a claim that every frequency between the poles has equal
support.

The engine also resolves register support from each voice's intentional pitch,
LF source and current tract. It compares the power at the first eight harmonics
with a local eight-point average, returns half the implied amplitude correction,
limits it to ±3 dB and glides it over 40 ms. It is a scalar voiced-source
gesture, not an EQ or post-render normalisation, and it deliberately ignores
vibrato. On take 10 it reduces the equal-velocity plateau span from 11.68 to
6.48 dB and the largest adjacent-note change from 7.37 to 4.67 dB while leaving
the changing vowel colour audible.

## How to listen to them

The set is ordered so it can be played straight through:

- **01** is the instrument sung the way it is meant to be played: a solo
  soprano phrase with Legato on, so each new key bends the sounding voice
  instead of re-attacking it, and a light glide carries the pitch between
  notes. Listen for the release at the end falling back down the phrase.
- **02** is the raw material: the AAH, OOH and UUH anchor vowels on the
  less-resonant neutral female voice, then the same three an octave and a bit
  lower on the male voice. Only the tract changes between profiles — the
  glottal source is the same model.
- **03–04** each hold a single note and move nothing but the tract. 03 engages
  the vowel morph and walks the pad through every cardinal vowel, so the pitch
  is constant while the vowel is continuously somewhere between real targets.
  04 shifts every formant an octave up and then an octave down: a body-size
  control at work, clearly not a pitch bend.
- **05–07** are the ensembles. 05 is twelve independently humanised singers
  holding a chord pad — no two enter, drift or waver together. 06 is chord
  mode: each single key is rendered as six singers spread across the triad,
  with the major/minor switch audible inside the progression, and the final
  tonic is held while the intonation control walks from equal temperament to
  five-limit just — listen for the third settling as it narrows 13.7 cents and
  stops beating against the root. 07 is an eight-singer phrase where only the
  MIDI velocity changes, then a held fifth swelled on the Dynamics control
  alone: the keys are struck once, so everything that moves after that is the
  dynamic reaching the source tilt, the pulse shape and the breath balance.
- **08** is the voice itself under the three most physical controls, all on
  one held male note: driven from a lax to a firmly pressed glottis — a
  closure-aligned interpolation through nine analysed LF pulse shapes that
  also closes the singer's-formant cluster, not an EQ — dissolved into breath, which is
  aspiration noise injected at the glottis and filtered by the very same
  tract, and finally closed into a hum as the velum opens and the nasal
  branch's murmur pole and anti-resonance take over.
- **09** sings the same three-note motif in a small dry booth and then in a
  large hall. Room size moves the reflections apart and lengthens the tail; it
  is geometry, not just a wet/dry fader.
- **10** is the register the other examples omit: one continuously articulated
  female voice climbs from C4 to C6. From E-flat5 to B-flat5 its residual broad
  upper reinforcement releases into the ordinary vowel poles instead of
  carrying one metallic 3 kHz peak to the top, while R3 and R4 follow the
  measured non-harmonic-locked soprano rise. Listen for a continuous change of
  colour rather than a switched filter at either boundary, and for the bounded
  support gesture to soften — not erase — the level change when a harmonic
  passes directly through F1 or F2.

## Levels

Each take is rendered with generous headroom and then normalised to −3 dBFS, so
the files can be auditioned one after another without reaching for the volume.
The renderer refuses to write a take that reached full scale before
normalisation, because the demo gain staging is meant to prove the engine's own
headroom rather than hide a hot mix.

The **rendered peak** column below is the level the model produced before that
normalisation, relative to the plug-in's own output stage at the reduced gain
the renderer uses. It is the honest measure of how loud each take actually is:
the twelve-singer pad genuinely sums far louder than a single lax voice.

<!-- peaks-table-begin: regenerated by VocalorRenderDemos; edits between the markers are overwritten -->
| File | What it is | Length | Rendered peak | Normalisation |
| --- | --- | ---: | ---: | ---: |
| `01-solo-legato-phrase.wav` | A solo soprano phrase sung legato, with a light glide between notes | 10.5 s | −18.6 dBFS | +15.6 dB |
| `02-vowel-anchors.wav` | The three preset vowels on a less-resonant neutral female and male voice | 11.3 s | −18.5 dBFS | +15.5 dB |
| `03-vowel-space-morph.wav` | One held note while the morph target walks every cardinal vowel | 13.2 s | −18.9 dBFS | +15.9 dB |
| `04-formant-shift.wav` | The whole tract shifted an octave up and down at a fixed pitch | 12.2 s | −17.7 dBFS | +14.7 dB |
| `05-ensemble-pad.wav` | Twelve independently humanised singers holding a chord pad | 11.6 s | −9.6 dBFS | +6.6 dB |
| `06-chord-mode-harmony.wav` | Single keys each rendered as a six-singer chord, major and minor | 15.1 s | −18.4 dBFS | +15.4 dB |
| `07-choir-dynamics.wav` | An eight-singer choir phrase sung from pianissimo to forte | 11.5 s | −12.7 dBFS | +9.7 dB |
| `08-tension-and-breath.wav` | A held male note from a lax to a pressed glottis, then into breath | 16.9 s | −18.6 dBFS | +15.6 dB |
| `09-room-small-and-large.wav` | The same motif sung in a dry booth and in a large hall | 11.5 s | −17.3 dBFS | +14.3 dB |
| `10-soprano-register.wav` | A connected soprano line from C4 to C6 through the cluster release | 10.1 s | −17.2 dBFS | +14.2 dB |
<!-- peaks-table-end -->

## Regenerating them

```bash
cd vocalor
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DVOCALOR_BUILD_PLUGIN=OFF -DBUILD_TESTING=OFF
cmake --build build-dsp --parallel --target VocalorRenderDemos
./build-dsp/VocalorRenderDemos Docs/audio
```

The render is deterministic, so running it on an unchanged model rewrites the
same bytes and commits nothing. All of the engine's variation — singer
identities, pitch jitter, shimmer, ensemble drift and the cycle-to-cycle
Instability draws — is seeded by counters and hashes rather than by a clock,
which is what makes an expressive render repeatable. The table above is
regenerated in place by the same command, so the documented levels cannot drift
away from the committed files either.

A take that is renamed or removed from the renderer's table is deleted from this
directory on the next full render, so a stale WAV cannot survive here after the
demo that produced it has gone.
