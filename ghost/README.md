# Ghost

A circuit-modelled monophonic dual-filter analog synthesizer, built as a
self-contained JUCE project: VST3, Audio Unit and Standalone, universal
`arm64`/`x86_64` on macOS, plus Linux and Windows builds.

![Ghost](Docs/screenshots/ghost-standalone.png)

Start with the [user guide](Docs/USER_GUIDE.md), see
[customer-facing changes](CHANGELOG.md) and
[privacy information](PRIVACY.md); framework licensing is described in
[third-party notices](THIRD_PARTY_NOTICES.md). Ghost is at version 0.9.0: a
complete instrument whose voiced constants are still open refinement targets
before a 1.0.

Ghost models the voice architecture of a 1983 monophonic analog synthesizer
— the Crumar Spirit, designed by Jim Scott, Tom Rhea and Bob Moog — block by
block: two bandlimited oscillators with one-directional hard sync and the
panel's exact duty-cycle sets, a triangle-cross ring modulator with its
un-nulled carrier bleed, a white-plus-pink noise source, and the signature
**series dual filter** — a lower multimode section (parametric boost,
inter-filter overdrive, resonant highpass, or out) sliding against a
12/24 dB upper lowpass, with the frozen-formant tracking mode — feeding two
parallel audio paths with independent VCAs. Modulation is the instrument's
own: MOD X (LFO, patterned and random sample-and-hold, red-noise drift,
Osc B) with the RIPPLE/ARPEGGIO/LEAP arpeggiator, and the SHAPER Y
variable-rate integrator with its four gate modes, both routed through
performance wheels to the panel's destination sets. It is an independent
original implementation, not affiliated with or licensed by Crumar or its
successors, and contains no firmware, ROM data, samples or captured audio.

The hardware shipped no presets: its manual taught eleven **Sound Charts**
instead, drawn panel settings with a lesson attached. Those charts are
Ghost's factory program bank — from the deliberately silent Preparatory
Pattern the lessons all start from, through Fat Filter, Sync and Sample &
Hold, to the Inverted Guitar — behind an Init program that is the default
voice itself. Selecting one writes the whole panel, so the bank reads as
the tutorial it was ([user guide](Docs/USER_GUIDE.md)).

What is modelled from documentation and what remains a voiced choice is set
out control by control in the
[circuit-modelling research and implementation contract](Docs/circuit-modelling-research.md).
Every constant still voiced is listed as a standing research task with an
explicit evidence gap in [open questions](Docs/open-questions.md), and the
field Ghost competes in, its audited standing and the ordered work that
closes the gap live in the
[best-in-class plan](Docs/best-in-class-plan.md). The
committed demonstration audio in [Docs/audio](Docs/audio/README.md) is
rendered by `GhostRenderDemos` from the same engine, so it cannot drift from
what Ghost actually sounds like.

## Building

The JUCE-free DSP core, tests and demo renderer:

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DGHOST_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
./build-dsp/GhostRenderDemos Docs/audio
```

The full plug-in (JUCE 8.0.14 is fetched pinned to its release commit, or
pass `-DGHOST_JUCE_PATH=/path/to/JUCE`):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On macOS, `./scripts/build-macos.sh` drives the same build through Xcode as
a universal binary and renders the committed editor screenshot while the
suite runs.
