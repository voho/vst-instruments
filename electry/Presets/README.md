# Electry sound-design recipes

Electry state is stored by the host. These original recipes are starting
points for the version-1.2 31-parameter set; percentages are approximate and
meant to be adjusted by ear. Parameters not listed in a recipe remain at
their defaults; see the
[complete parameter contract](../README.md#exact-31-parameter-contract).
Remember that the performance comes from the two latched keyswitch banks (or
the PICK STROKE and PLAY STYLE strips), not from a parameter: C0..D0 latch
the picking style - down, up, alternate - and D#0..A0 latch the play style -
sustain, palm mute, hammer-on, natural harmonic, pinch harmonic, slide, dead
note - and any combination of the two works. Material controls span the named
solid-body endpoints; Scale spans 25.5" to 28" for the Drop-E instrument.

With the five FX controls at their 0% defaults the output is exactly dry. Mono
is the summed DI; Stereo is the physical low-to-high divided-pickup string field
and adds no widening effect. These recipes describe the guitar itself; reach for
the built-in amplifier and cabinet, or put your favourite amp simulation after
Electry, for finished tones.

## Drop-E Rhythm (through the built-in amplifier)

The voicing behind
[`Docs/audio/05-drop-e-rhythm-amp.wav`](../Docs/audio/README.md), a Drop-E
metal rhythm sound that needs nothing after it:

- Pickup selector `Bridge`; pickup type 32% (toward a hot humbucker); tone 100%.
- Scale 27.6" (85%), string gauge 80%, string age 10%.
- Pick position 18% — close to the bridge; hardness 85%.
- Mute damping 85% so the palm-muted notes chug rather than half-mute.
- Velocity 70%; sympathetic ring 25%; output +6 dB, because a single chugged
  low note is a quiet signal next to a full eight-string chord and the default
  level leaves headroom for the chord.
- Distortion 45%, amp 95%, compressor 60%; delay and room at 0%.
- Latch E0 (Palm Mute) for the muted notes and D#0 (Sustain) for the open
  accents; the picking bank is free for down or alternate strokes throughout.
  The bridge hand also damps the coupled strings, so the chug stays tight
  without turning the sympathetic ring off.

## Factory Default (default check)

Everything at its default, which is no longer a blend of the anchors: the
four material controls sit at 0 - the thick carved mahogany/maple set-neck
end of each - on a 27.63" scale with the heaviest string set, a
humbucker-leaning bridge pickup at 32%, and tone at 70%. Not neutral and not
the midpoint of any axis; it is one specific Drop-E rhythm guitar, which is
what makes it the right reference to hang an amp chain off. See
[why the defaults are an instrument rather than an average](../README.md#guitar-construction-axes).

(Formerly "Between Worlds", when the defaults really were 50/50 on both
anchors with tone at 80%.)

## Vintage Twang

- Pickup selector `Bridge`; pickup type 100%; tone 90%.
- Body wood 100%, size 85%, shape 100%, construction 100%, scale 28.00".
- String gauge 30%, string age 10%; pick position 20%, hardness 75%.
- Pick noise 55%, finger noise 35%, release noise 45%.
- Artifacts 22% for restrained saddle detail.
- Sympathetic ring 45% and strum spread 14 ms: open chords bloom and sweep
  like a real strummed guitar.
- Play mostly downstrokes with occasional up-stroke accents (C#0, back to C0);
  a slow pre-bent entry - wheel down, pick, release the wheel over a long
  Bend Time - gives the classic behind-the-beat pedal-steel gesture.

## Thick Set-Neck Rhythm

- Pickup selector `Neck`; pickup type 0%; tone 55%.
- Body wood 0%, size 15%, shape 0%, construction 0%, scale 25.50".
- String gauge 80%, string age 35%; pick position 55%, hardness 45%.
- Body resonance 45%; velocity 55%; sympathetic ring 30%.
- Artifacts 12% for a controlled studio-clean rhythm sound.
- Downstrokes for chords; hammer-on (F0) for legato figures.

## Palm-Muted Chug

- Keyswitch E0 (Palm Mute) latched; mute damping 85% - the firm end of the
  mute's travel is the tight chug.
- Pickup selector `Bridge`; pickup type 25%; tone 75%.
- Pick noise 70%, hardness 85%, velocity 80%.
- Artifacts 42% so hard low-string attacks occasionally catch a fret.
- Palm mute 25% under the keyswitch for an even tighter stop, or leave it
  at 0% and ride MIDI CC 2 through the riff instead.
- Latch D0 (Alternate) and drive fast even strokes low on the Drop-E and B
  strings; lower mute damping toward 40% for half-muted grit.

## Singing Lead Bends

- Pickup selector `Neck`; pickup type 15%; tone 65%.
- Construction 20%; string gauge 25% (light strings glide more).
- Bend time 320 ms - the wheel takes that long to reach its target, so bends
  travel like a finger rather than snapping; finger noise 55%; release
  noise 50%.
- Resonance depth 60% so a raised modulation wheel makes the instrument sing
  through the amp instead of merely ringing.
- Ride the pitch wheel for bends and releases - each string follows with its
  own compliance, the plain G deeper than its neighbours - with F0 hammer-ons
  for a legato solo voice.

## Feedback Sustainer

- The singing-feedback setting behind the close of
  [`Docs/audio/14-whammy-and-feedback.wav`](../Docs/audio/README.md).
- Distortion 55%, amp 85%, compressor 30% - the rig has to be loud in the
  room before the strings can feed.
- Resonance depth 100%; sympathetic ring 45%.
- Play a note, raise the modulation wheel, and release the key: the amplifier
  keeps the instrument singing. Close the wheel, or land the palm (CC 2), to
  stop the howl. Half a wheel blooms instead of howling.

## Old Strings, Small Room

- String age 85%, gauge 60%; tone 45%; body resonance 25%.
- Pick noise 65%, finger noise 60%, release noise 65%, hardness 30%.
- The dead, thumpy setting for lo-fi indie parts; upstrokes (C#0) keep it
  conversational.
