# YouKnow201

A ten-voice virtual-analog synthesizer, built as a self-contained JUCE
project: VST3 and Standalone for macOS, Linux and Windows, plus Audio Unit on
macOS.

YouKnow201 models the voice architecture of the Roland SH-201 (2006) — the
last keyboard of the calculated-supersaw family that began with the JP-8000 —
block by block from Roland's own published documents: the owner's manual, the
MIDI implementation's complete parameter address map, and the service notes'
block and circuit diagrams. It is an independent original implementation, not
affiliated with or licensed by Roland Corporation, and contains no firmware,
ROM data, samples, captured audio, or factory patch data. Its panel follows
the modelled instrument's functional layout with independent branding and
project-drawn controls.

> **Listen first.** Ten [rendered demonstrations](Docs/audio/README.md) cover
> the seven-saw SUPER SAW and its spread curve, FB OSC feedback, the −24 dB
> filter into self-oscillation, oscillator sync, ring modulation, PWM strings
> through the chorus delay template, S&H effects, and DUAL-mode pads — each
> matched to an official real-unit recording for by-ear comparison.

![YouKnow201](Docs/screenshots/youknow201-standalone.png)

## What kind of replica this is

The modelled instrument's sound engine is pure DSP: the service notes show
one Roland custom DSP ("WSP", a Toshiba-fabbed gate array) computing all
synthesis and effects, no PCM wave ROM anywhere in the design, and a
documented analog input/output stage around the codec. A faithful replica is
therefore a *behavioral model of a DSP engine* plus a component-level model
of the analog output path — not an analog circuit simulation. What is settled
by which source, what follows a published measurement of related hardware,
and what remains a voiced choice is recorded mechanism by mechanism in the
[research and implementation contract](Docs/sh201-replica-research.md),
whose open questions each name the measurement that would close them.

The engine's grounding, briefly:

- **Architecture (settled).** One patch holds two complete tones (UPPER,
  LOWER), each `OSC1 + OSC2 → MIX/MOD → FILTER → AMP` with a two-stage pitch
  envelope, filter and amp ADSRs, and two LFOs; a shared modulation-delay →
  reverb chain with per-tone sends; SINGLE/DUAL/SPLIT keyboard modes;
  10 voices, halved in DUAL. Every parameter and range comes verbatim from
  the MIDI implementation's address map, including the waveform order
  (SAW, SQU, PW-SQU, TRI, SINE, NOISE, FB-OSC, SUPER-SAW, EXT-IN), the LFO
  shape order, the 20-entry tempo-sync note table, and the effect frequency
  tables.
- **SUPER SAW (reported).** Seven sawtooth oscillators per oscillator slot,
  implemented with Adam Szabo's JP-8000 measurements quoted verbatim: the
  fixed detune offsets, the 11th-degree spread-knob polynomial, free-running
  phases randomized per note, and the pitch-tracked high-pass on the summed
  stack. The SH-201's fixed center/side mix (it has no MIX knob) is a voiced
  choice, flagged as a standing listening-test candidate.
- **FB OSC (reported mechanism, voiced constants).** A sawtooth with a
  soft-clipped feedback comb at half the fundamental period — "high
  overtones, similar to feedback on a guitar" with one feedback-amount
  control, polyphonic on both oscillator slots as the hardware allows.
- **Filter (settled behavior, voiced calibration).** LPF/HPF/BPF/BYPASS at
  −12 or −24 dB/oct; KEY FOLLOW −200…+200 pivoting at C4 with +100 tracking
  1:1 per the manual's own diagram; resonance reaching sustained bounded
  self-oscillation exactly as the manual warns. The knob-to-Hz and
  resonance-to-Q curves are voiced pending hardware measurement.
- **Analog output stage (settled).** The service-notes component values:
  22 µF/22 kΩ coupling (0.329 Hz), the 8.2 kΩ/820 pF and 4.7 kΩ/270 pF RC
  poles (23.7 kHz, 125.4 kHz), gain chain normalized to digital full scale.
- **MIDI (settled).** The control-change map from owner's manual p. 72 for
  both tones and the part controllers (sostenuto excepted, deferred),
  including the printed CC#88 collision resolved to CC#83 as documented in
  the research contract.

The demos are rendered through this exact engine by a JUCE-free tool that CI
rebuilds and verifies, so the committed audio cannot drift from the code.

## Presets

The 13 shipped programs are original sounds programmed against the engine —
Roland's 64 factory patches are data no public document publishes and none of
it ships here. The eight delay templates (Simple Delay … Chorus 1/2) and
eight reverb templates (Room 1 … Plate 2) implement the settled template
names with this project's voiced values.

## Build

Linux/CI (JUCE-free DSP, tests, demo renderer):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DYOUKNOW201_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build --parallel && ctest --test-dir build --output-on-failure
./build/YouKnow201RenderDemos Docs/audio
```

macOS (full plug-in set):

```bash
./scripts/build-macos.sh
```

The full plug-in also builds on Linux and Windows (VST3 + Standalone); CI
exercises all three platforms. A pinned JUCE 8.0.14 is fetched at configure
time, or pass `-DYOUKNOW201_JUCE_PATH=/path/to/JUCE`.

## Layout

- `Source/DSP/` — the JUCE-free engine: the parameter contract
  (`YouKnow201Patch.h`, quoting the documented ranges and tables), the
  engine with every panel-to-physics mapping in one auditable namespace
  (`YouKnow201Engine.h/.cpp`), and the original preset bank.
- `Source/` — JUCE plug-in processor (APVTS mirroring the parameter
  contract, documented CC map, factory programs) and the panel editor.
- `Tools/RenderDemos.cpp` — renders the committed demo WAVs.
- `Tests/` — engine tests (tuning, enumeration semantics, polyphony rules,
  self-oscillation boundedness, effect tails, full-bank rendering at two
  rates) and plug-in tests (layout, MIDI, programs, state, editor
  snapshot).
- `Docs/` — the research contract, demo audio, screenshots.

## Not yet modelled

The arpeggiator and step recorder, D-Beam, the external-input path (audio
filter, center cancel, EXT-IN oscillators), SysEx DT1/RQ1 I/O, and sostenuto
are documented but deferred; the research contract lists them alongside the
open calibration questions.

## Licensing

Original code under the MIT license (`LICENSE`). JUCE is used under its own
terms — see `THIRD_PARTY_NOTICES.md`.
