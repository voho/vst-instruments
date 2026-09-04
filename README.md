# VST Instruments

[![CI](https://github.com/voho/vst-instruments/actions/workflows/ci.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/ci.yml)
[![Nightly](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml)

Seven original virtual instruments built with C++20 and JUCE 8: VST3 and
Standalone for macOS, Linux and Windows, plus Audio Unit for macOS.

Each instrument has one README covering how it works block by block, what it
does not model, and its release history. The committed demo audio under each
`Docs/audio/` is rendered from that instrument's own shipping engine by CI, so
it cannot drift from the code.

**Nightly builds:** download the 14-day `vst-instruments-nightly-all-platforms`
artifact from the latest successful main-branch
[Nightly workflow run](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml).

---

## [Drumalor](drumalor/README.md)

[![Drumalor](drumalor/Docs/screenshots/drumalor-standalone.png)](drumalor/README.md)

Thirteen-voice procedural drum synthesizer with struck-membrane modal models,
velocity-dependent timbre, per-voice level/pan/choke mixing, humanised organic
variation, a shared drive and compression bus, and GM-oriented MIDI mapping.
Membrane banks bend with head tension, repeated strikes absorb energy from
ringing heads, the snare responds to strike position, and the two cymbal
channels follow published analyses of a 1980 analogue and a 1983 digital
machine.

[Seven audio demos](drumalor/README.md#audio-demos) · [How it works](drumalor/README.md#how-it-works)

---

## [Electry](electry/README.md)

[![Electry](electry/Docs/screenshots/electry-standalone.png)](electry/README.md)

Oversampled physically modeled Drop-E eight-string guitar: eight
dual-polarisation waveguides, fitted stiff-string dispersion, sympathetic
strings, continuous palm mute, induced-EMF pickups, modal body loss,
Mono/divided-pickup Stereo, two independent keyswitch banks, a pitch wheel that
bends every string like a vibrato bar, a resonance wheel that reaches
self-sustaining feedback, and a 4×-oversampled amplifier with supply sag, output
transformer and modeled cabinet.

[Twenty-three audio demos](electry/README.md#audio-demos) · [How it works](electry/README.md#how-it-works)

---

## [Taikor](taikor/README.md)

[![Taikor](taikor/Docs/screenshots/taikor-standalone.png)](taikor/README.md)

Physically modeled taiko drum suite: struck circular membranes solved from
Bessel-zero modes with air loading, two heads coupled through an enclosed body,
a thin-cylinder wooden shell, and Hertzian stick contact scaling duration with
impact speed. Sixteen pads cover four distinct instruments — ō-daiko, chū-daiko,
okedo and shime — each with its own head tension, diameter, body depth, hide and
shell, across four octaves.

[Twenty-seven audio demos](taikor/README.md#audio-demos) · [How it works](taikor/README.md#how-it-works)

---

## [Acustra](acustra/README.md)

[![Acustra](acustra/Docs/screenshots/acustra-standalone.png)](acustra/README.md)

Physically modelled six-string acoustic guitar: fitted stiff-string waveguides
drive a passive two-point measured bridge (each string ending on its own
saddle point), sympathetic idle strings and a stereo body-radiation model
measured per string material. A plate conductance floor restores the damping
a finite modal fit loses above the soundboard's modal overlap, a string taken
for a new note rings on under the hand rather than being cut, and CC2 is
continuous bridge-hand damping, CC68 legato hammers on with a strike sized by
velocity, CC1 is the fretting hand's vibrato, and note-off velocity lifts the
fretting finger off the string. MPE gives a member channel's own pitch bend a
real string-tension bend, plus per-note pluck point and pressure. A
fret/tuning allocator enforces guitar playability while bounded Shape,
Material, String Age, Touch and Position controls keep the surface simple.

[Ten audio demos](acustra/README.md#audio-demos) · [How it works](acustra/README.md#how-it-works)

---

## [YouKnow](youknow/README.md)

[![YouKnow](youknow/Docs/screenshots/youknow-standalone.png)](youknow/README.md)

Circuit-modelled six-voice DCO polysynth: integer-divided note timers with true
pitch quantisation, a scanned control converter with per-hold slew and 7-bit
patch digitisation, firmware envelopes with exponential decay into a measured
quasi-linear amplifier, a four-pole transconductor filter with implicitly solved
resonance loop and input-side compensation, a non-stealing key assigner, and an
uncompanded two-line bucket-brigade chorus with authentic saturation and MN3009
noise.

[Ten audio demos](youknow/README.md#audio-demos) · [How it works](youknow/README.md#how-it-works)

---

## [Septum](septum/README.md)

[![Septum](septum/Docs/screenshots/septum-standalone.png)](septum/README.md)

Ten-voice virtual-analog synthesizer modelling the Roland SH-201's documented
architecture: two complete tones of OSC1+OSC2 → MIX/MOD → FILTER → AMP with
three envelopes and two LFOs each, a shared modulation-delay → reverb chain, and
SINGLE/DUAL/SPLIT keyboard modes. The seven-saw SUPER SAW implements Szabo's
measured JP-8000 detune polynomial; the multimode filter reaches bounded
self-oscillation as the manual warns; and the arpeggiator runs the documented
32 × 16 style grid, held to the manual's own three worked examples.

[Eleven audio demos](septum/README.md#audio-demos) · [How it works](septum/README.md#how-it-works)

---

## [Ghostar](ghostar/README.md)

[![Ghostar](ghostar/Docs/screenshots/ghostar-standalone.png)](ghostar/README.md)

Circuit-modelled monophonic dual-filter analog synthesizer, built from the
documentation of a 1983 Italian mono synth: two bandlimited
oscillators with hard sync, a triangle-cross ring modulator, and the signature
series dual filter — a lower multimode section sliding against a 12/24 dB upper
lowpass with a frozen-formant tracking mode — feeding two parallel audio paths.
Its character-defining laws are derived from primary documents rather than
voiced: the resonance curve from the filter chip's own Q scale, both filter
nonlinearities from a diode's forward characteristic, and the envelope timing
from the 556 timer circuit the service drawing shows.

[Thirteen audio demos](ghostar/README.md#audio-demos) · [How it works](ghostar/README.md#how-it-works)

---

## macOS Gatekeeper

Nightly workflow artifacts are ad-hoc signed. If macOS Gatekeeper prevents
opening a downloaded build, clear the quarantine attribute:

```bash
xattr -dr com.apple.quarantine \
  ~/Library/Audio/Plug-Ins/VST3/*.vst3 \
  ~/Library/Audio/Plug-Ins/Components/*.component \
  /Applications/*.app
```

## Linux compatibility

Nightly Linux packages are built on Ubuntu 24.04 x64 and target that runtime or
newer.

## Licensing

Original code under the MIT license (`LICENSE`). JUCE is used under its own
terms; each instrument's `THIRD_PARTY_NOTICES.md` records its own dependencies.
