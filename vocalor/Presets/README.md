# Vocalor presets

Vocalor ships a bank of twelve factory programs. They are published through the
host's own program interface, so they appear as Audio Unit factory presets in
Logic and as the program list in a VST3 host, and selecting one writes its
values into the
[24 automatable parameters](../README.md#interface-and-controls).

| Program | What it is |
| --- | --- |
| Init Soprano | The shipping default sound, so a host that opens on program 0 opens on what the plug-in opens on |
| Intimate Alto | Close solo voice, breathy, legato with a short glide and a shortened tract |
| Pressed Tenor | Male solo at high tension, with deliberately restrained cycle instability |
| Legato Soloist | Solo voice set up for phrasing: legato on, a long glide, dynamics at 82 % |
| Breath And Air | Aspiration at full and the dynamic low, which is the breathiest the model goes |
| Warm Bass Choir | Eight male singers on the close-back anchor, tract lengthened four semitones |
| Cathedral Ensemble | Twelve singers, the largest room, just intonation nearly full, and looser cycles |
| Closed Mouth Hum | The velum fully open: the nasal branch's murmur pole and notch |
| Small Voices | Six singers with the tract shortened six semitones toward the close-front corner |
| Vowel Morph Pad | Ten singers with the morph at full, so the pad and its two axes own the vowel |
| Locked Major Chorale | Chord mode in full just intonation: one key, a locked triad |
| Airy Minor Pad | Minor chord mode, breathy and wide, with the bank's highest cycle instability |

Every program sets Instability explicitly. Vibrato chooses the intended extent;
Instability adds bounded deterministic variation to each cycle's period, depth
and contour, while Humanize controls the singers' ensemble identity, timing and
vocal-tract differences. Pressed sounds stay disciplined, and airy or large
choir programs are allowed to wander further without becoming nondeterministic.

## Where they live

The table is in
[`Source/DSP/Presets.cpp`](../Source/DSP/Presets.cpp), inside the JUCE-free
core rather than in the processor. That is deliberate: the DSP test suite
renders every preset on a held note and a held interval and checks that each
one is finite, audible, bounded, releases fully, and carries no value the
engine would clamp away. A preset that produces silence is a build failure
rather than something a player discovers.

The processor only translates. It converts the engine's linear output gain into
the decibels the host parameter publishes, and writes each value through
`setValueNotifyingHost` so the host sees the change as automation.

Re-selecting the program already in force is a no-op. A host restores a session
by setting the stored program and replacing the parameter state, in either
order, and without that guard the program write would overwrite whatever the
player had edited away from the preset.

## Adding one

Use names that describe the voice rather than a real person, keep the settings
original, and add the entry to the table above. Only include settings and
assets that are cleared for redistribution; no samples, recordings, model
weights or third-party preset material belong in this repository.
