# VST Instruments

[![CI](https://github.com/voho/vst-instruments/actions/workflows/ci.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/ci.yml)
[![Nightly](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml/badge.svg)](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml)

A collection of original virtual instruments built with C++20 and JUCE 8: VST3 and Standalone for macOS, Linux, and Windows, plus Audio Unit for macOS.

---

## [Vocalor](vocalor/)

[![Vocalor](vocalor/Docs/screenshots/vocalor-standalone.png)](vocalor/README.md)

- **Nightly builds**: download the 14-day `vst-instruments-nightly-all-platforms` artifact from the latest successful main-branch [Nightly workflow run](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml).

Source-filter vocal and choir synthesizer with an LF-style glottal source, a continuous morphable vowel space, formant shifting independent of pitch, and legato phrasing across solo, ensemble, and chord modes. Vowel transitions run on articulator timescales, F1 tracks the fundamental once pitch climbs past it, the singer's formant is a resonance cluster, and a nasal branch allows humming. Formants sound from the first sample, velocity sets attack speed (9.3 ms to 86.4 ms), vibrato operates at 5.7–6.8 Hz with 3 dB AM, and spatial ensemble positioning places singers at 1.5 to 6 m with individual propagation delays and early reflections.

- **Documentation**: [Vocalor README](vocalor/README.md) · [Rendered Demos](vocalor/Docs/audio/README.md)

---

## [Drumalor](drumalor/)

[![Drumalor](drumalor/Docs/screenshots/drumalor-standalone.png)](drumalor/README.md)

- **Nightly builds**: download the 14-day `vst-instruments-nightly-all-platforms` artifact from the latest successful main-branch [Nightly workflow run](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml).

Thirteen-voice procedural drum synthesizer with struck-membrane modal models, velocity-dependent timbre, per-voice level/pan/choke mixing, humanised organic variation, a shared drive and compression bus, and GM-oriented MIDI mapping. Membrane banks bend with head tension estimates, repeated strikes absorb energy from ringing heads, snare responds to strike position with rimshots and cross-sticks, hi-hat follows a continuous CC 4 pedal, and Kit Bleed couples the kit through sympathetic snare and tom beds. Cymbals feature Hertzian contact dynamics, membrane tails warble via split degenerate mode pairs, and pitch glides scale naturally with blow strength.

- **Documentation**: [Drumalor README](drumalor/README.md) · [Rendered Demos](drumalor/Docs/audio/README.md)

---

## [Neuramar](neuramar/)

[![Neuramar](neuramar/Docs/screenshots/neuramar-standalone.png)](neuramar/README.md)

- **Nightly builds**: download the 14-day `vst-instruments-nightly-all-platforms` artifact from the latest successful main-branch [Nightly workflow run](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml).

Drop in a monophonic sound, infer its root, and fit a compact local DDSP-inspired neural synthesis model — with fitted stiff-string inharmonicity, formant shifting, and velocity-driven timbre — whose harmonic Core, noisy Air, and resonant Bone remain playable across pitches. Harmonic analysis is solved jointly over a 40 ms aperture; the body carries 16 Air bands and 12 Bone modes across 16 voices. Features Core-targeted register normalisation, fixed note-on voice placement, forward golden-ratio Orbit looping with level normalization, frequency-dependent Dissolve damping, key-tracked model clocking, and velocity-varied strike dynamics.

- **Documentation**: [Neuramar README](neuramar/README.md) · [Rendered Demos](neuramar/Docs/audio/README.md) · [Resynthesis Benchmark](neuramar/Docs/resynthesis-quality-benchmark.md)

---

## [Electry](electry/)

[![Electry](electry/Docs/screenshots/electry-standalone.png)](electry/README.md)

- **Nightly builds**: download the 14-day `vst-instruments-nightly-all-platforms` artifact from the latest successful main-branch [Nightly workflow run](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml).

Oversampled physically modeled Drop-E eight-string guitar: eight dual-polarisation waveguides, fitted stiff-string dispersion, sympathetic strings, continuous palm mute, induced-EMF pickups, modal body loss, Mono/divided-pickup Stereo, two independent keyswitch banks (three pick strokes against seven play styles allocated by a fretting hand with position and reach), pitch wheel vibrato bar, resonance wheel feedback, and a 4x-oversampled amplifier with supply sag, output transformer, and modeled cabinet. Velocity controls pick deflection across an 18.2 dB range; pick contact position/force varies per stroke; dual-coil humbucker spacing places notches at 3046–5508 Hz; vibrato redraws per cycle across stopped strings; strummed chords propagate with string-to-string delays (14.7 to 10.1 ms); and fingered strings exchange energy through a shared bridge.

- **Documentation**: [Electry README](electry/README.md) · [Rendered Demos](electry/Docs/audio/README.md)

---

## [Taikor](taikor/)

[![Taikor](taikor/Docs/screenshots/taikor-standalone.png)](taikor/README.md)

- **Nightly builds**: download the 14-day `vst-instruments-nightly-all-platforms` artifact from the latest successful main-branch [Nightly workflow run](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml).

Physically modeled taiko drum suite: struck circular membranes solved from Bessel-zero modes with air loading, two heads coupled through an enclosed body, a thin-cylinder wooden shell, and Hertzian stick contact scaling duration with impact speed. Features 16 pads covering four distinct instruments (ō-daiko, chū-daiko, okedo, shime) with authentic head tension, diameter, body depth, hide, and shell characteristics across 4 octaves (from C3). Don, Ka, Tsu, and Don Rim strokes per drum, near-field decay stereo positioning, stiff membrane tension glide, head stroke damping, preload-threshold byō tack line, and authentic ghost strokes.

- **Documentation**: [Taikor README](taikor/README.md) · [Rendered Demos](taikor/Docs/audio/README.md)

---

## [YouKnow106](youknow106/)

[![YouKnow106](youknow106/Docs/screenshots/youknow106-standalone.png)](youknow106/README.md)

- **Nightly builds**: download the 14-day `vst-instruments-nightly-all-platforms` artifact from the latest successful main-branch [Nightly workflow run](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml).

Circuit-modelled six-voice DCO polysynth: integer-divided note timers with true pitch quantisation, scanned control converter with per-hold slew and 7-bit patch digitisation, firmware envelopes with exponential decay into a measured quasi-linear amplifier, four-pole transconductor filter with implicitly solved resonance loop, input-side resonance compensation, non-stealing key assigner, and an uncompanded two-line bucket-brigade chorus with authentic saturation and MN3009 noise. Features balanced dual-nonlinearity resonance, filter-output DC block, LFO delay width pulse holding, rate-independent 34.48 °C warm-up model, and optional velocity-to-filter envelope routing.

- **Documentation**: [YouKnow106 README](youknow106/README.md) · [Rendered Demos](youknow106/Docs/audio/README.md) · [Circuit Research](youknow106/Docs/circuit-modelling-research.md)

---

## [YouKnow201](youknow201/)

[![YouKnow201](youknow201/Docs/screenshots/youknow201-standalone.png)](youknow201/README.md)

- **Nightly builds**: download the 14-day `vst-instruments-nightly-all-platforms` artifact from the latest successful main-branch [Nightly workflow run](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml).

Ten-voice virtual-analog synthesizer modelling the Roland SH-201's documented architecture: two complete tones of OSC1+OSC2 → MIX/MOD → FILTER → AMP with three envelopes and two LFOs each, a shared modulation-delay → reverb chain, and SINGLE/DUAL/SPLIT keyboard modes with 10 voices halved in DUAL. The seven-saw SUPER SAW implements Szabo's measured JP-8000 detune polynomial, offsets, free-running phases and pitch-tracked high-pass; FB OSC is a soft-clipped feedback comb; the multimode filter reaches bounded self-oscillation as the manual warns; and the analog output stage carries the service notes' component values. Every parameter and range is quoted from the MIDI implementation's address map, and the complete documented CC map is honoured for both tones.

- **Documentation**: [YouKnow201 README](youknow201/README.md) · [Rendered Demos](youknow201/Docs/audio/README.md) · [Replica Research](youknow201/Docs/sh201-replica-research.md)

---

## [Ghost](ghost/)

[![Ghost](ghost/Docs/screenshots/ghost-standalone.png)](ghost/README.md)

- **Nightly builds**: download the 14-day `vst-instruments-nightly-all-platforms` artifact from the latest successful main-branch [Nightly workflow run](https://github.com/voho/vst-instruments/actions/workflows/nightly.yml).

Circuit-modelled monophonic dual-filter analog synthesizer, built from the documentation of a 1983 Moog-designed Italian mono synth: two bandlimited oscillators with hard sync and the panel's exact duty-cycle sets, a triangle-cross ring modulator with un-nulled carrier bleed, and the signature series dual filter — a lower multimode section (parametric boost, inter-filter overdrive, resonant highpass) sliding against a 12/24 dB upper lowpass with a frozen-formant tracking mode — feeding two parallel audio paths with independent VCAs. Modulation includes the RIPPLE/ARPEGGIO/LEAP arpeggiator, patterned and random sample-and-hold, red-noise drift, and the Shaper Y variable-rate integrator routed through performance wheels. The modelled hardware shipped no presets — its manual taught eleven Sound Charts instead — so those charts are the factory program bank, from the deliberately silent Preparatory Pattern to the Inverted Guitar, and every continuous control glides over ~25 ms so host automation and 7-bit CCs never step the audio.

- **Documentation**: [Ghost README](ghost/README.md) · [Rendered Demos](ghost/Docs/audio/README.md) · [Circuit Research](ghost/Docs/circuit-modelling-research.md)

---

## macOS Gatekeeper Note

Nightly workflow artifacts are ad-hoc signed. If macOS Gatekeeper prevents opening a downloaded build, clear the quarantine attribute:

```bash
xattr -dr com.apple.quarantine \
  ~/Library/Audio/Plug-Ins/VST3/*.vst3 \
  ~/Library/Audio/Plug-Ins/Components/*.component \
  /Applications/*.app
```

## Linux Compatibility

Nightly Linux packages are built on Ubuntu 24.04 x64 and target that runtime or newer.
