# Electry demonstration audio

Fourteen rendered examples of what Electry produces: its full playable range,
the twelve combinations of the independent picking-style and play-style
keyswitch banks, the guitar-build and pickup axes, the pitch-wheel bar and the
resonance-wheel amplifier feedback, and the Drop-E metal rhythm and lead tones
through the built-in amplifier chain.

Every file here is rendered by `Tools/RenderDemos.cpp` from the shipping
JUCE-free signal path — `ElectryEngine` into `ElectryFx`, the same code the
VST3, AU and Standalone run — so a demonstration cannot drift away from what
the plug-in actually sounds like. No samples, impulse responses or external
processing are involved anywhere in this directory: the whole set comes out of
the physical model and the modelled amplifier.

The set was last extended with three longer takes built around power chords —
dry, amped, and inside a two-bar arrangement — because a chord loads the model
very differently from a single note: three strings share one bridge, so the
sympathetic coupling, the strum travel and the amplifier's intermodulation all
have something to work with. The two single-note rhythm takes are now two bars
rather than one.

The audio itself reflects the pickup position comb's finite null depth and the
bridge hand's frequency-dependent absorption rate - including its much lighter rate at
the fundamental, which is what gives the muted takes a tail instead of going
silent a second after the pick; both touch every note, and
the measurements behind them are in the
[research contract](../physical-modeling-research.md#voicing-against-a-reference-recording).

The muted takes were revoiced when the hand's loss stopped being a shelf and
became a band centred on the string's own fifth harmonic, paired with a much
lighter rate at the fundamental. Seven of the thirteen files changed at that
point; the six that did not - the open-string, fretboard, lead, pickup, strum and
build takes - were byte-identical, because both terms multiply a rate that is
exactly zero when no hand is on the string.

The mute then stopped being one fixed amount of loss for the whole note. The
heel's contact area now grows over the first 40 ms, so an attack rings briefly
before the mute takes hold, and its grip slackens as the string stops pressing
into it, so the tail opens back up instead of staying clamped. The two act at
opposite ends of the note and multiply. Seven files carry it - the same seven,
for the same reason.

Every file also moved for a different reason again: the instrument's default
voicing changed. The defaults were the midpoint of every axis, which is not a
guitar anyone owns; they are now a thick carved set-neck blank with the heaviest
set on a 27.63-inch scale, a humbucker-leaning bridge pickup, the tone a little
back and a softer pick close to the bridge. Against nine dry muted power-chord
references at five pitches that voicing measures 5.03 dB of joint tilt-and-contour
error where the midpoints measured 6.31.

The whole set was re-rendered for the version 1.2 performance model. The
sixteen single-enum play styles became two independent keyswitch banks - three
picking styles against four play styles - so `03` now walks their
combinations; the former chug takes ride the Palm Mute style with the Mute Damp
control at the firm end of its travel; the lead take's keyswitch bends became
pitch-wheel bends riding the Bend Time glide and its close is the new resonance
wheel feeding the note back through the amplifier; and `14` demonstrates the
wheel as a bar and the feedback loop on their own. The re-scored takes' peaks
moved (`03`-`06`, and `14` is new); the other rows kept their previous values
at this table's precision, and four takes - `01`, `02`, `07` and `09`, whose
scores use only the default sustained downstroke - re-render byte-identical,
which pins the untouched signal paths.

| File | Channels | Length | What it demonstrates |
| --- | --- | --- | --- |
| [`01-range-open-strings.wav`](01-range-open-strings.wav) | mono | 6.2 s | The eight open strings of the Drop-E instrument, low to high (E1 B1 E2 A2 D3 G3 B3 E4), then all eight ringing together. Default settings. |
| [`02-range-full-fretboard.wav`](02-range-full-fretboard.wav) | mono | 4.1 s | Every playable note, MIDI 28 to 86 — E1 to D6, five and a half octaves — with the string allocator choosing which physical string each one lands on. |
| [`03-play-styles.wav`](03-play-styles.wav) | mono | 6.7 s | The two keyswitch banks combined on the same two notes: sustain, palm mute and natural harmonic each under a down stroke, an up stroke and an alternate-picked pair, and the hammer-on run under two latched strokes — audibly identical, because a fingered note takes no stroke. |
| [`04-drop-e-rhythm-dry.wav`](04-drop-e-rhythm-dry.wav) | mono | 7.5 s | A chugged Drop-E rhythm figure as the raw DI: bridge humbucker, 28-inch-leaning build, heavy set, picked close to the bridge. |
| [`05-drop-e-rhythm-amp.wav`](05-drop-e-rhythm-amp.wav) | mono | 7.5 s | The identical MIDI and identical guitar settings through the oversampled gain stage, the modelled cabinet and the rhythm compressor. Compare directly with `04`. |
| [`06-lead-amp-delay-room.wav`](06-lead-amp-delay-room.wav) | stereo | 9.4 s | A lead phrase — alternate-picked notes, a hammer-on, a wheel bend up a step and a dive released onto the note (both riding the Bend Time glide), a natural harmonic, and a final note pushed into amplifier feedback by the resonance wheel — into the amplifier, the 360 ms lead delay and the room. |
| [`07-pickups-and-tone.wav`](07-pickups-and-tone.wav) | mono | 7.0 s | One phrase played three times through Neck, Both and Bridge, then a single note repeated as the guitar's own passive tone control closes from 100% to 0%. |
| [`08-sympathetic-strum-stereo.wav`](08-sympathetic-strum-stereo.wav) | stereo | 7.3 s | Strum travel (22 ms per string crossed), the divided-pickup stereo field, and bridge-coupled sympathetic strings: a down-strum, an up-strum, the same chord with the coupling at exactly zero, then one low note setting the unfingered strings ringing. |
| [`09-guitar-build-contrasts.wav`](09-guitar-build-contrasts.wav) | mono | 7.7 s | The same short figure on three instruments: a 25.5-inch light-strung build, the default 27.63-inch guitar, and a 28-inch baritone with a heavy set and older strings. |
| [`10-velocity-dynamics.wav`](10-velocity-dynamics.wav) | mono | 4.4 s | Velocity response at full travel — six strokes from a finger-light touch to a hard metal attack, then four more with the palm mute at 60%. Level, brightness, contact noise and attack pitch glide all move together. |
| [`11-power-chords-dry.wav`](11-power-chords-dry.wav) | mono | 19.9 s | Power chords as the raw DI. Four held root-fifth-octave shapes so the decay and the bridge coupling are audible, a twelve-chord chugged progression, then eight tight stabs under 55% continuous palm-mute pressure. Roots stay in MIDI 28–32, where the allocator gives the idiomatic three-adjacent-string shape. Nothing after the pickups. |
| [`12-power-chords-amp.wav`](12-power-chords-amp.wav) | mono | 20.1 s | The identical MIDI and guitar settings through the amplifier, cabinet and compressor. Compare directly with `11`: three fundamentals and their harmonic series intermodulating in the clipping stages is what the oversampling is there for. |
| [`13-long-rhythm-arrangement.wav`](13-long-rhythm-arrangement.wav) | stereo | 14.0 s | The longest take here: two bars of the chugged single-note figure, a bar of descending power chords, then a full eight-string open ring-out, amped with a little room behind it. The instrument in a part rather than under a microscope. |
| [`14-whammy-and-feedback.wav`](14-whammy-and-feedback.wav) | mono | 11.2 s | The pitch wheel as a vibrato bar and the modulation wheel as amplifier feedback: a ringing chord dived two semitones and pushed sharp — every string following at its own compliance, the ringing open strings included — then one note released into a self-sustaining howl that the closing wheel and a landed palm shut off. |

All files are 44.1 kHz 16-bit PCM.

The low register and the palm-muted rhythm tone in this set were voiced against
a dry electric low-E reference recording; `Docs/physical-modeling-research.md`
records what was compared and what changed.

## Mono files are exact, not downmixed

Electry's Mono output field is exact dual mono: the engine renders one channel
and mirrors it, and with the delay and room at zero the effect chain keeps both
channels bit-identical. The renderer verifies that before it writes, so a
single-channel file here is a lossless record of the take rather than a
downmix. Only the takes that genuinely use a stereo field — the divided-pickup
output mode, or the delay and room — are written as two channels.

## Levels

Each file is peak normalised to −3 dBFS by one constant gain applied to the
whole take. The takes deliberately use different voicings and different
settings of the instrument's own output control, and their raw peaks span more
than fifteen decibels, so without this the set would be uncomfortable to
audition one after another. Because it is a single gain per file, nothing
inside a take is altered: the velocity ramp in `10`, the decay of each chug in
`04`, and the dry-versus-amplified comparisons between `04` and `05` and
between `11` and `12` all keep their shape and their internal balance.

`11` renders about 4.7 dB hotter than the single-note figure in `04` from the same
output setting: three strings struck as one stroke stack their initial peaks,
which is why a player tracking chords leaves the instrument's own output control
lower than for a single chugged note.

The rendered peak before normalisation, for the record:

| File | Rendered peak | Normalisation applied |
| --- | --- | --- |
| `01-range-open-strings.wav` | −12.4 dBFS | +9.4 dB |
| `02-range-full-fretboard.wav` | −11.5 dBFS | +8.5 dB |
| `03-play-styles.wav` | −5.9 dBFS | +2.9 dB |
| `04-drop-e-rhythm-dry.wav` | −14.9 dBFS | +11.9 dB |
| `05-drop-e-rhythm-amp.wav` | −20.2 dBFS | +17.2 dB |
| `06-lead-amp-delay-room.wav` | −7.0 dBFS | +4.0 dB |
| `07-pickups-and-tone.wav` | −13.1 dBFS | +10.1 dB |
| `08-sympathetic-strum-stereo.wav` | −16.6 dBFS | +13.6 dB |
| `09-guitar-build-contrasts.wav` | −9.7 dBFS | +6.7 dB |
| `10-velocity-dynamics.wav` | −13.2 dBFS | +10.2 dB |
| `11-power-chords-dry.wav` | −10.9 dBFS | +7.9 dB |
| `12-power-chords-amp.wav` | −19.5 dBFS | +16.5 dB |
| `13-long-rhythm-arrangement.wav` | −18.1 dBFS | +15.1 dB |
| `14-whammy-and-feedback.wav` | −13.6 dBFS | +10.6 dB |

`05` renders about 5 dB *below* `04` at the peak while sitting above it in
average level: the amplifier compresses and the cabinet removes the top of the
pick transient, which is what an amplifier is supposed to do to a rhythm part.
`06` and `14` peak during their feedback passages — a howl is the loudest thing
the instrument does, exactly as it is on a real rig.

## Regenerating

The renderer is an ordinary CMake target, built by default alongside the
JUCE-free DSP library:

```bash
cd electry
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DELECTRY_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
./build-dsp/ElectryRenderDemos Docs/audio
```

The engine is deterministic — identical MIDI always renders identical audio —
so re-rendering an unchanged model reproduces byte-identical files, and any
difference in `git status` after a re-render is a real change in the sound.
`ctest` runs the renderer in `--smoke` mode as `Electry.RenderDemos`, which
renders a short take to a temporary directory and checks that it is finite,
audible and a readable WAV, so the tool cannot rot unnoticed.

The score for each demonstration is a short, readable function in
`Tools/RenderDemos.cpp`; add a `Demo` entry there to add a file.
