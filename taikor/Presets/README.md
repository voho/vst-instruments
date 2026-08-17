# Taikor presets

The current plug-in stores its
[25-parameter drum state](../README.md#controls) in the host session and exposes
the version-1 defaults on first launch. Those defaults are not the midpoint of
every control: they describe one specific instrument — a five-shaku (150 cm)
ō-daiko with a thick cowhide head on a heavy carved zelkova shell, struck with
a medium-hard oak bachi and heard through a close pair 16 cm off the head.

That is the drum the C3 octave plays. The other three octaves are three other
instruments of the family — a chū-daiko, an okedo-daiko and a shime-daiko, each
with its own diameter, body depth, hide and shell — and the controls below are
carried across all four as a trim rather than describing each of them
separately. See [the four drums](../README.md#the-four-drums).

Factory preset files are intentionally not baked into the project yet. Keeping
them out avoids silently changing sound design while the physical model is still
being refined. The drum-design controls describe physical quantities rather
than arbitrary voicing labels, so a preset should describe an instrument rather
than a mood.

When presets are added, name **4 Drums** settings after the *set* they describe —
`Kumi-daiko`, `Hira-daiko set`, `Studio kit` — rather than after a mood. A
**1 Drum** setting may instead name the one physical design it retunes across
the keyboard. Only include original settings. Taikor contains no samples, and
presets must not introduce uncleared sample or impulse-response assets.

## What the four drums already are

At the default Drum Layout (**4 Drums**) the four octaves are already four
instruments, and these are the numbers the engine's own table holds. They are
the reference for anything dialled in on top of them.

| Octave | Instrument | Diameter | Body depth | Tension | Head | Shell |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| C3 | Ō-daiko | 150 cm | 50 % | 62 % | 75 % | 80 % |
| C4 | Chū-daiko | 78 cm | 89 % | 54 % | 62 % | 74 % |
| C5 | Okedo-daiko | 40 cm | 94 % | 78 % | 36 % | 20 % |
| C6 | Shime-daiko | 30 cm | 33 % | 95 % | 18 % | 92 % |

The two large drums are the size they are because of what the family has to
span: three octaves of heard pitch is a factor of eight, and a laced hide caps
the shime near 500 Hz, so the bottom of that span cannot be a 95 cm drum. Five
shaku is what the bottom of a kumi-daiko set actually is.

## Moving the whole family from the controls

The physical-model controls let a set of drums be dialled in by describing
the largest of them. Head Diameter is a scale factor on all four; the other
four drum controls are offsets carried across all four. So these describe the
C3 ō-daiko, and the rest of the family follows it:

| Character | Diameter | Body depth | Tension | Head | Shell |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ō-daiko (factory) | 150 cm | 50 % | 62 % | 75 % | 80 % |
| Larger battle drum | 160–180 cm | 60–80 % | 45–60 % | 85–100 % | 85–100 % |
| Hira-daiko (shallow set) | 95–120 cm | 5–20 % | 50–65 % | 75–90 % | 70–85 % |
| Tight and dry | 110–135 cm | 40–55 % | 75–90 % | 55–70 % | 60–80 % |
| Light stave-built set | 110–145 cm | 55–75 % | 55–70 % | 65–80 % | 15–35 % |

Head Diameter runs 15–180 cm, so the battle-drum row is close to the top of the
control rather than in the middle of it.

Switching Drum Layout to **1 Drum** uses whichever drum these controls describe
as one physical design retuned over the four octaves. **4 Drums** is the default:
each octave instead uses the corresponding family instrument above.
