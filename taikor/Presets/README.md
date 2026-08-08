# Taikor presets

The current plug-in stores its
[22-parameter drum state](../README.md#controls) in the host session and exposes
the version-1 defaults on first launch. Those defaults are not the midpoint of
every control: they describe one specific instrument — a 95 cm ō-daiko with
a thick cowhide head on a heavy carved zelkova shell, struck with a
medium-hard oak bachi and heard through a close pair 16 cm off the head.

That is the drum the C3 octave plays. The other three octaves are three other
instruments of the family — a chū-daiko, an okedo-daiko and a shime-daiko, each
with its own diameter, body depth, hide and shell — and the controls below are
carried across all four as a trim rather than describing each of them
separately. See [the four drums](../README.md#the-four-drums).

Factory preset files are intentionally not baked into the project yet. Keeping
them out avoids silently changing sound design while the physical model is still
being refined, and every parameter here is a physical quantity rather than a
voicing choice, so a preset is a description of a drum rather than a patch.

When presets are added, name them after the *set* of drums they describe —
`Kumi-daiko`, `Hira-daiko set`, `Studio kit` — rather than after a mood or after
a single instrument, because a preset is now four drums rather than one. Only
include original settings. Taikor contains no samples, and presets must not
introduce uncleared sample or impulse-response assets.

## What the four drums already are

At the default Octave Body (*Family*) the four octaves are already four
instruments, and these are the numbers the engine's own table holds. They are
the reference for anything dialled in on top of them.

| Octave | Instrument | Diameter | Body depth | Tension | Head | Shell |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| C3 | Ō-daiko | 95 cm | 50 % | 55 % | 75 % | 80 % |
| C4 | Chū-daiko | 55 cm | 89 % | 53 % | 57 % | 74 % |
| C5 | Okedo-daiko | 40 cm | 94 % | 66 % | 36 % | 20 % |
| C6 | Shime-daiko | 30 cm | 33 % | 86 % | 24 % | 92 % |

## Moving the whole family from the controls

The parameters are physical, so a set of drums can be dialled in by describing
the largest of them. Head Diameter is a scale factor on all four; the other
four drum controls are offsets carried across all four. So these describe the
C3 ō-daiko, and the rest of the family follows it:

| Character | Diameter | Body depth | Tension | Head | Shell |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ō-daiko (factory) | 95 cm | 50 % | 55 % | 75 % | 80 % |
| Larger battle drum | 100–120 cm | 60–80 % | 45–60 % | 85–100 % | 85–100 % |
| Hira-daiko (shallow set) | 60–75 cm | 5–20 % | 50–65 % | 75–90 % | 70–85 % |
| Tight and dry | 70–85 cm | 40–55 % | 75–90 % | 55–70 % | 60–80 % |
| Light stave-built set | 70–90 cm | 55–75 % | 55–70 % | 65–80 % | 15–35 % |

Turning Octave Body down towards *Tuned* collapses the family onto whichever
drum these controls describe, so the four octaves become one drum retuned. That
is a useful thing to reach for and it is not what the instrument is for.
