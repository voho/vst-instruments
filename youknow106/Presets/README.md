# YouKnow106 sound-design recipes

YouKnow106 state is stored by the host. These original recipes are starting
points for the version-1.0 parameter set. Every control is quoted as its panel
position in percent, because that is what the panel is: the engine maps each
position through the modelled circuit's own law, and the plug-in displays the
resulting time or frequency next to the control.

Parameters not listed stay at their version-1.0 defaults — 8' range, saw on,
pulse off, HPF 1, POLY 1, chorus off, and the six non-hardware controls at the
values that reproduce hardware behaviour (velocity 0%, six voices, chorus noise
100%, calibration 35%). HQ is a persisted quality setting, not part of a patch,
and changes take effect once the engine is idle.

Two habits carry over from the hardware and are worth knowing before you start:
the envelope generator is linear and the amplifier exponential, so a decay set
by ear will be shorter than the same number on a synth with an exponential
generator; and cutoff modulation is summed before the antilog stage, so the ENV
and LFO sliders are octaves, not hertz.

## Glass Pad

- LFO rate 18%, delay 55%; DCO LFO 12%, PWM 62% (`MAN`), sub 20%.
- HPF 1; VCF freq 44%, res 18%, ENV `+` 26%, LFO 8%, KYBD 45%.
- ENV 62% / 70% / 78% / 74%; VCA `ENV`, level 78%.
- Chorus `I`. The delay lines' hiss is part of this sound; leave it in.

## Bass, Short and Hard

- 16' range, saw on, pulse off, sub 55%.
- HPF 0 (the boost position) for weight; VCF freq 22%, res 34%, ENV `+` 58%,
  KYBD 20%.
- ENV 0% / 34% / 0% / 12%; VCA `ENV`, level 85%.
- Chorus off, POLY 2 — reusing the low voices immediately is what keeps a fast
  bass line tight.

## Hollow Fifths

- Pulse on, saw off, PWM 88% (`MAN`) for a narrow pulse; sub 30%.
- HPF 2; VCF freq 52%, res 46%, ENV `+` 20%, LFO 6%, KYBD 60%.
- ENV 8% / 52% / 62% / 40%.
- Chorus `II`. Push the resonance further and the input compensation drives the
  filter harder — the sound thickens rather than thinning.

## Vibrato Lead

- `UNISON`; portamento 22%; bender DCO 30%, bender LFO 65%.
- LFO rate 46%, delay 40%; DCO LFO 0% (the lever supplies it).
- HPF 1; VCF freq 38%, res 28%, ENV `+` 48%, KYBD 70%.
- ENV 6% / 40% / 55% / 30%.
- Chorus off. Six phase-locked voices on one key is not a detuned stack; it is
  the same waveform six times, which is why it is loud and hard-edged.

## Slow Sweep

- Saw on, pulse on, PWM 50% (`LFO`), noise 6%.
- LFO rate 6%, delay 0%; VCF LFO 62%.
- HPF 1; VCF freq 30%, res 62%, ENV `-` 40%, KYBD 0%.
- ENV 80% / 88% / 100% / 85%.
- Chorus `I`. With the polarity inverted the envelope closes the filter, so the
  attack opens and the decay shuts.

## Organ

- 8' saw on, sub 70%, pulse off.
- HPF 1; VCF freq 68%, res 0%, ENV 0%, KYBD 100%.
- VCA `GATE` — the amplitude switches instantly and the envelope still drives
  the filter, which is exactly what that switch is for.
- Chorus `II`.

## Percussive Comb

- Pulse on, saw off, PWM 30% (`LFO`), LFO rate 78%.
- HPF 3; VCF freq 58%, res 70%, ENV `+` 66%, KYBD 55%.
- ENV 0% / 22% / 0% / 16%.
- Calibration 80% — the per-voice spread is what stops a fast repeated figure
  sounding machine-stamped.

## Self-Oscillating Sine

- Saw off, pulse off, sub 0%, noise 0%: nothing reaches the filter but its own
  thermal noise.
- VCF res 100%, freq to taste, KYBD 100% so it tracks the keyboard.
- ENV 0% / 0% / 100% / 20%; VCA `ENV`, level 60%.
- The oscillation is limited by the transconductors' own saturation, not by a
  clamp, and its pitch is trimmed to land on the cutoff law — the same trim the
  hardware's service procedure sets per voice.

Only original parameter settings and redistributable assets belong in this
directory. Exported patches should record the YouKnow106 version they target so
later parameter migrations stay explicit.
