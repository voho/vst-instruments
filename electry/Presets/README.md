# Electry sound-design recipes

Electry state is stored by the host. These original recipes are starting
points for the version-1.0 parameter set; percentages are approximate and
meant to be adjusted by ear. Parameters not listed in a recipe remain at
their defaults; see the
[complete parameter contract](../README.md#exact-20-parameter-contract).
Remember that the play style comes from the latched keyswitch (C1..G#1) or
the PLAY STYLE strip, not from a parameter, and that every guitar-model
axis reads "Les Paul-style at 0%, Telecaster-style at 100%".

The output is always dry. These recipes describe the guitar itself; put
your favourite amp simulation after Electry for finished tones.

## Between Worlds (default check)

Everything at its default: both anchors blended 50/50, bridge pickup,
tone 80%. A neutral, honest DI tone for testing amp chains.

## Vintage Twang

- Pickup selector `Bridge`; pickup type 100%; tone 90%.
- Body wood 100%, size 85%, shape 100%, construction 100%, scale 25.50".
- String gauge 30%, string age 10%; pick position 20%, hardness 75%.
- Pick noise 55%, finger noise 35%, release noise 45%.
- Play mostly downstrokes with occasional upstroke accents; the release
  bends (F#1/G1) give the classic behind-the-beat pedal-steel gesture.

## Thick Set-Neck Rhythm

- Pickup selector `Neck`; pickup type 0%; tone 55%.
- Body wood 0%, size 15%, shape 0%, construction 0%, scale 24.75".
- String gauge 80%, string age 35%; pick position 55%, hardness 45%.
- Body resonance 45%; velocity 55%.
- Downstrokes for chords; hammer-on (D1) for legato figures.

## Palm-Muted Chug

- Keyswitch D#1 (Muted) latched; mute damping 70%.
- Pickup selector `Bridge`; pickup type 25%; tone 75%.
- Pick noise 70%, hardness 85%, velocity 80%.
- Eighth-note downstrokes low on the E and A strings; raise mute damping
  toward 85% for tighter chug, lower toward 40% for half-muted grit.

## Funk Slap

- Keyswitch G#1 (Slap) latched.
- Pickup selector `Both`; pickup type 70%; tone 95%.
- Construction 100%, scale 25.50"; string gauge 40%, age 5%.
- Body resonance 55% (the thumb knock reads through the body).
- High velocities matter: the collision buzz window and the sharp-to-true
  tension glide both scale with how hard the note is struck.

## Singing Lead Bends

- Pickup selector `Neck`; pickup type 15%; tone 65%.
- Construction 20%; string gauge 25% (light strings glide more).
- Bend time 320 ms; finger noise 55%; release noise 50%.
- Alternate E1/F1 (bend 1 and 2 up) with D1 hammer-ons for a legato solo
  voice; the pitch wheel still adds vibrato-style motion on top.

## Old Strings, Small Room

- String age 85%, gauge 60%; tone 45%; body resonance 25%.
- Pick noise 65%, finger noise 60%, release noise 65%, hardness 30%.
- The dead, thumpy setting for lo-fi indie parts; upstrokes (C#1) keep it
  conversational.
