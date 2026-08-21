# Ghost

A circuit-modelled monophonic dual-filter analog synthesizer, built as a
self-contained C++20 project. Ghost is currently in its **DSP-first phase**:
the complete engine, its test suites and the demo renderer exist before any
JUCE wrapper or editor does, so the sound can be judged before a single
pixel is drawn.

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

What is modelled from documentation and what remains a voiced choice is set
out control by control in the
[circuit-modelling research and implementation contract](Docs/circuit-modelling-research.md).
Every constant still voiced is listed as a standing research task with an
explicit evidence gap in [open questions](Docs/open-questions.md). The
committed demonstration audio in [Docs/audio](Docs/audio/README.md) is
rendered by `GhostRenderDemos` from the same engine, so it cannot drift from
what Ghost actually sounds like.

## Building

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
./build-dsp/GhostRenderDemos Docs/audio
```

`GHOST_BUILD_PLUGIN` exists but errors when enabled: the plug-in wrapper is
the next phase, and asking for it today should fail loudly rather than
silently produce nothing.
