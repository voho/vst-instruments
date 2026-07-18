# Electry sound-design recipes

Electry state is stored by the host. These original recipes are starting
points for the version-1.0 parameter set; percentages are approximate and
meant to be adjusted by ear. Parameters not listed in a recipe remain at
their defaults; see the
[complete parameter contract](../README.md#exact-22-parameter-contract).
Remember that the play style comes from the latched keyswitch (C0..D#1) or
the PLAY STYLE strip, not from a parameter. Material controls span the named
solid-body endpoints; Scale spans 25.5" to 28" for the Drop-E instrument.

The output is always dry. Mono is the summed DI; Stereo is the physical
low-to-high divided-pickup string field and adds no widening effect. These
recipes describe the guitar itself; put your favourite amp simulation after
Electry for finished tones.

## Between Worlds (default check)

Everything at its default: both anchors blended 50/50, bridge pickup,
tone 80%. A neutral, honest DI tone for testing amp chains.

## Vintage Twang

- Pickup selector `Bridge`; pickup type 100%; tone 90%.
- Body wood 100%, size 85%, shape 100%, construction 100%, scale 28.00".
- String gauge 30%, string age 10%; pick position 20%, hardness 75%.
- Pick noise 55%, finger noise 35%, release noise 45%.
- Artifacts 22% for restrained saddle detail and sympathetic ring.
- Play mostly downstrokes with occasional upstroke accents; the release bends
  (C#1/D1) give the classic behind-the-beat pedal-steel gesture.

## Thick Set-Neck Rhythm

- Pickup selector `Neck`; pickup type 0%; tone 55%.
- Body wood 0%, size 15%, shape 0%, construction 0%, scale 25.50".
- String gauge 80%, string age 35%; pick position 55%, hardness 45%.
- Body resonance 45%; velocity 55%.
- Artifacts 12% for a controlled studio-clean rhythm sound.
- Downstrokes for chords; hammer-on (D#0) for legato figures.

## Palm-Muted Chug

- Keyswitch F#0 (Chug) latched; mute damping 70%.
- Pickup selector `Bridge`; pickup type 25%; tone 75%.
- Pick noise 70%, hardness 85%, velocity 80%.
- Artifacts 42% so hard low-string attacks occasionally catch a fret.
- Alternate or tremolo strokes low on the Drop-E and B strings; raise mute
  damping toward 85% for tighter chug, lower toward 40% for half-muted grit.

## Funk Slap

- Keyswitch D#1 (Slap) latched.
- Pickup selector `Both`; pickup type 70%; tone 95%.
- Construction 100%, scale 27.00"; string gauge 40%, age 5%.
- Body resonance 55% (the thumb knock reads through the body).
- Artifacts 30% for extra bridge and fretboard mechanics.
- High velocities matter: the collision buzz window and the sharp-to-true
  tension glide both scale with how hard the note is struck.

## Singing Lead Bends

- Pickup selector `Neck`; pickup type 15%; tone 65%.
- Construction 20%; string gauge 25% (light strings glide more).
- Bend time 320 ms; finger noise 55%; release noise 50%.
- Alternate B0/C1 (bend 1 and 2 up) with D#0 hammer-ons for a legato solo
  voice; the pitch wheel still adds vibrato-style motion on top.

## Old Strings, Small Room

- String age 85%, gauge 60%; tone 45%; body resonance 25%.
- Pick noise 65%, finger noise 60%, release noise 65%, hardness 30%.
- The dead, thumpy setting for lo-fi indie parts; upstrokes (C#0) keep it
  conversational.
