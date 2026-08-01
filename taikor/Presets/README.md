# Taikor presets

The current plug-in stores its
[22-parameter drum state](../README.md#controls) in the host session and exposes
the version-1 defaults on first launch. Those defaults are not the midpoint of
every control: they describe one specific instrument — a 55 cm nagado-daiko with
a thick cowhide head on a heavy carved zelkova shell, struck with a
medium-hard oak bachi and heard through a close pair 16 cm off the head.

Factory preset files are intentionally not baked into the project yet. Keeping
them out avoids silently changing sound design while the physical model is still
being refined, and every parameter here is a physical quantity rather than a
voicing choice, so a preset is a description of a drum rather than a patch.

When presets are added, name them after the instrument they describe — `Odaiko`,
`Shime-daiko`, `Hira-daiko`, `Okedo` — rather than after a mood. Only include
original settings. Taikor contains no samples, and presets must not introduce
uncleared sample or impulse-response assets.

## Building a drum from the controls

The parameters are physical, so a drum can be dialled in by describing it rather
than by ear:

| Instrument | Diameter | Body depth | Tension | Head | Shell |
| --- | ---: | ---: | ---: | ---: | ---: |
| Odaiko | 100–120 cm | 60–80 % | 45–60 % | 85–100 % | 85–100 % |
| Nagado-daiko (default) | 55 cm | 50 % | 55 % | 75 % | 80 % |
| Hira-daiko (shallow) | 60–75 cm | 5–20 % | 50–65 % | 75–90 % | 70–85 % |
| Shime-daiko | 30–35 cm | 25–40 % | 85–100 % | 55–70 % | 60–80 % |
| Okedo (stave, lighter) | 45–60 cm | 55–75 % | 55–70 % | 65–80 % | 15–35 % |

Remember that the octave being played already rescales the drum, so these
describe the instrument as it sounds in the reference octave (C3). Playing an
octave lower with the default Octave Body setting gives a drum roughly twice the
size without touching any of the controls above.
