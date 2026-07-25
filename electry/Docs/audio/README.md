# Electry demonstration audio

Ten rendered examples of what Electry produces: its full playable range, all
sixteen keyswitched play styles, the guitar-build and pickup axes, and the
Drop-E metal rhythm and lead tones through the built-in amplifier chain.

Every file here is rendered by `Tools/RenderDemos.cpp` from the shipping
JUCE-free signal path — `ElectryEngine` into `ElectryFx`, the same code the
VST3, AU and Standalone run — so a demonstration cannot drift away from what
the plug-in actually sounds like. No samples, impulse responses or external
processing are involved anywhere in this directory: the whole set comes out of
the physical model and the modelled amplifier.

| File | Channels | Length | What it demonstrates |
| --- | --- | --- | --- |
| [`01-range-open-strings.wav`](01-range-open-strings.wav) | mono | 6.2 s | The eight open strings of the Drop-E instrument, low to high (E1 B1 E2 A2 D3 G3 B3 E4), then all eight ringing together. Default settings. |
| [`02-range-full-fretboard.wav`](02-range-full-fretboard.wav) | mono | 4.1 s | Every playable note, MIDI 28 to 86 — E1 to D6, five and a half octaves — with the string allocator choosing which physical string each one lands on. |
| [`03-play-styles.wav`](03-play-styles.wav) | mono | 9.6 s | All sixteen play styles in keyswitch order on the same two notes: downstroke, upstroke, alternate, hammer-on, tap, palm mute, chug, dead note, natural and pinch harmonic, tremolo picking, the four bend programs, and slap. |
| [`04-drop-e-rhythm-dry.wav`](04-drop-e-rhythm-dry.wav) | mono | 4.2 s | A chugged Drop-E rhythm figure as the raw DI: bridge humbucker, 28-inch-leaning build, heavy set, picked close to the bridge. |
| [`05-drop-e-rhythm-amp.wav`](05-drop-e-rhythm-amp.wav) | mono | 4.2 s | The identical MIDI and identical guitar settings through the oversampled gain stage, the modelled cabinet and the rhythm compressor. Compare directly with `04`. |
| [`06-lead-amp-delay-room.wav`](06-lead-amp-delay-room.wav) | stereo | 7.5 s | A lead phrase — picked notes, a hammer-on, a bend up held with CC 1 vibrato, a release bend, tremolo picking and a pinch harmonic — into the amplifier, the 360 ms lead delay and the room. |
| [`07-pickups-and-tone.wav`](07-pickups-and-tone.wav) | mono | 7.0 s | One phrase played three times through Neck, Both and Bridge, then a single note repeated as the guitar's own passive tone control closes from 100% to 0%. |
| [`08-sympathetic-strum-stereo.wav`](08-sympathetic-strum-stereo.wav) | stereo | 7.3 s | Strum travel (22 ms per string crossed), the divided-pickup stereo field, and bridge-coupled sympathetic strings: a down-strum, an up-strum, the same chord with the coupling at exactly zero, then one low note setting the unfingered strings ringing. |
| [`09-guitar-build-contrasts.wav`](09-guitar-build-contrasts.wav) | mono | 7.7 s | The same short figure on three instruments: a 25.5-inch light-strung build, the default 26.75-inch guitar, and a 28-inch baritone with a heavy set and older strings. |
| [`10-velocity-dynamics.wav`](10-velocity-dynamics.wav) | mono | 4.4 s | Velocity response at full travel — six strokes from a finger-light touch to a hard metal attack, then four more with the palm mute at 60%. Level, brightness, contact noise and attack pitch glide all move together. |

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
`04`, and the dry-versus-amplified comparison between `04` and `05` all keep
their shape and their internal balance.

The rendered peak before normalisation, for the record:

| File | Rendered peak | Normalisation applied |
| --- | --- | --- |
| `01-range-open-strings.wav` | −12.6 dBFS | +9.6 dB |
| `02-range-full-fretboard.wav` | −10.9 dBFS | +7.9 dB |
| `03-play-styles.wav` | −2.5 dBFS | −0.5 dB |
| `04-drop-e-rhythm-dry.wav` | −13.8 dBFS | +10.8 dB |
| `05-drop-e-rhythm-amp.wav` | −19.3 dBFS | +16.3 dB |
| `06-lead-amp-delay-room.wav` | −15.9 dBFS | +12.9 dB |
| `07-pickups-and-tone.wav` | −13.2 dBFS | +10.2 dB |
| `08-sympathetic-strum-stereo.wav` | −15.2 dBFS | +12.2 dB |
| `09-guitar-build-contrasts.wav` | −8.9 dBFS | +5.9 dB |
| `10-velocity-dynamics.wav` | −14.8 dBFS | +11.8 dB |

`05` renders 5.5 dB *below* `04` at the peak while sitting above it in average
level: the amplifier compresses and the cabinet removes the top of the pick
transient, which is what an amplifier is supposed to do to a rhythm part.

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
