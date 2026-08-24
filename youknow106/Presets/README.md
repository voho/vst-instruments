# YouKnow106 sound-design recipes

YouKnow106 state is stored by the host. These original recipes are starting
points for the version-1.1 parameter set. Every control is quoted as its panel
position in percent, because that is what the panel is: the engine maps each
position through the modelled circuit's own law, and the plug-in displays the
resulting time or frequency next to the control.

Parameters not listed stay at their version-1.1 defaults — 8' range, saw on,
pulse off, HPF 1, POLY 1, chorus off, and the non-hardware controls at
their hardware-aligned/product defaults (velocity 0%, six voices, chorus noise
100%, Unit Character 100%, Aging 0% — a freshly serviced unit; like QUALITY it
is not part of a patch, so presets never move it). Unit Character is the one of those whose
hardware-aligned value is its top: 100% is the "matches real hardware"
reference a fresh instance starts on, and 0% is the calibrated nominal model
with every per-card tolerance removed. QUALITY is a persisted processing
setting, not part of a patch, and changes take effect once the engine is idle.

Three modelling habits are worth knowing before you start: envelope attack is
linear while decay and release are exponential, and the current provisional
`VoicedVoiceVcaCompatibilityProfile` is quasi-linear over most of its control
range; cutoff
modulation is summed before the antilog stage, so the ENV and LFO sliders are
octaves, not hertz; and VCA LEVEL is a shared post-voice patch trim before the
chorus, not a multiplier on each voice's envelope.

The shipped program bank keeps the byte-exact historical 128 tone records.
Its product presets also store attenuation-only positions for the visible VR1
output-volume shaft so hot programs remain bounded; no tone byte is changed and
there is no hidden corrective gain, EQ, or limiting. The recipes below are
original YouKnow106 sound-design examples, not extra bundled factory entries.
They may use the plug-in's extension controls.
The [complete factory gain audit](../Docs/audio/factory-presets/README.md)
publishes unnormalised metrics for all 128 tones and common-gain previews of a
representative subset.

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

- `POLY 1` + `POLY 2` together, which is how the panel selects unison; portamento 22%; bender DCO 30%, bender LFO 65%.
- LFO rate 46%, delay 40%; DCO LFO 0% (the lever supplies it).
- HPF 1; VCF freq 38%, res 28%, ENV `+` 48%, KYBD 70%.
- ENV 6% / 40% / 55% / 30%.
- Chorus off. Solo Unison sums six undetuned, free-running DCOs. Their timer
  counts agree, but their phases are not forcibly reset together, so this is
  neither a detuned stack nor six phase-locked copies. It is intentionally much
  louder than one voice because the hardware does not normalise the sum.

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
- Unit Character 80% — a small step down from the shipped 100%: the voiced
  per-card spread still stops a fast repeated figure sounding machine-stamped
  without letting the cards wander as far apart. Zero remains the nominal
  model.

## Self-Oscillating Sine

- Saw off, pulse off, sub 0%, noise 0%: only the model's provisional
  microscopic per-card excitation reaches the filter.
- VCF res 100%, freq to taste, KYBD 100% so it tracks the keyboard.
- ENV 0% / 0% / 100% / 20%; VCA `ENV`, level 60%.
- The oscillation uses the voiced resonance profile's nonlinear return rather
  than a hard clamp. A voiced model correction targets the service procedure's
  published cutoff pitch; its coefficient and feedback-dependent curve are not
  established by that procedure.

Only original parameter settings and redistributable assets belong in this
directory. Exported patches should record the YouKnow106 version they target so
later parameter migrations stay explicit.
