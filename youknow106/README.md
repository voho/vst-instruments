# YouKnow106

A six-voice circuit-modelled DCO polysynth for macOS, built as a self-contained
JUCE project: VST3, Audio Unit and Standalone, universal `arm64`/`x86_64`.

YouKnow106 models the voice architecture of a 1984 six-voice polysynth — the
Roland Juno-106 — block by block, from its integer-divided note timers through
its four-pole transconductor filter to its uncompanded bucket-brigade chorus. It
is an independent original implementation, not affiliated with or licensed by
Roland Corporation, and it contains no firmware, ROM data, samples or captured
audio. Its panel reproduces that instrument's functional layout because that
layout *is* its control set; its palette, typography and name are its own.

What is modelled from documentation and what remains a voiced choice is set out
control by control in the
[circuit-modelling research and implementation contract](Docs/circuit-modelling-research.md).

> **Listen first.** Ten [rendered demonstrations](Docs/audio/README.md) cover
> the classic pad and PWM strings, the 16' bass, the self-oscillating filter,
> the chorus modes, unison glide, the delayed vibrato, the high-pass ladder
> and the instrument's own dispersion. They are rendered by the shipping
> engine, so they cannot drift from what the plug-in does.

## What makes it a circuit model rather than a lookalike

- **The oscillator is a divider, not a phase accumulator.** Pitch is one
  reference clock divided by a 16-bit integer, so it is quantised exactly as the
  hardware's is — A4 at 8' programmes count 4545 and sounds 440.044 Hz. The
  RANGE switch changes the clock reaching the counter, not the count, so it
  transposes by whole octaves and the tuning error is the same in all three.
- **The control path is a scanned converter.** One converter serves 36 control
  points — six per voice, including the pulse threshold, sub and noise levels
  and the oscillator's amplitude compensation — walking the voices in turn
  across a 4.2 ms pass and slewing each hold on its own time constant. Every
  continuous panel control is digitised to seven bits, which is what makes it
  patch-storable. Slow bends and deep vibrato audibly step, six voices step
  out of phase with one another, and the shortest attack the instrument can
  actually produce is one scan pass — not the published 1.5 ms.
- **The envelope attacks in a straight line and falls exponentially, into a
  quasi-linear amplifier** — a 14-bit firmware accumulator whose falling
  segments multiply their way down and end by integer truncation, driving an
  amplifier that tracks its control voltage linearly with an exponential knee
  confined to the bottom tenth. That factorisation, measured on the hardware,
  is what the dB-linear decay tails actually come from.
- **Cutoff modulation is summed in converter counts before the antilog stage**,
  at 1143 counts per octave, so every modulation source is exponential in hertz.
  The law is anchored on the instrument's own service calibration: converter
  code 6272 self-oscillates at 248 Hz, and the test suite asserts it.
- **Resonance compensates on the input side.** Raising resonance drives *more*
  signal into the filter to offset its passband loss, so a high-Q patch here
  gets dirtier rather than thinner.
- **The key assigner drops notes rather than stealing them**, because that is
  what the hardware does with seven keys held on six voices.
- **Unison has no detune**, because six timers dividing one reference by one
  count cannot disagree. What separates the voices is the analogue block after
  them.
- **The chorus has no compander**, so it hisses — the hiss is modelled, and
  there is a control to defeat it that the hardware does not have.

## Interface

The panel keeps the reference instrument's section order and control set:

```
VOLUME · BENDER · MODE · LFO · DCO · HPF · VCF · VCA · ENV · CHORUS
```

Sliders, switches and buttons are placed by the JUCE-free layout description in
`Source/DSP/YouKnow106Panel.cpp`, so the regression suite can check that nothing
overlaps, nothing escapes its section and every group is complete without
opening a window. The palette is deliberately not the reference instrument's: a
matte slate charcoal faceplate with alternating magenta and cyan section
highlights, cool silver-grey caps and neon-green indicators, over a procedurally
generated moulded-plastic texture. There are no image assets.

Below the panel, separated by a rule, sit the six controls the hardware does not
have — Transpose, Master Tune, Velocity, Calibration, Chorus Noise and
Polyphony — plus HQ, Panic and two randomisers. Each defaults to the value that
reproduces hardware behaviour, so the default patch is a hardware-faithful patch:
velocity does nothing, six voices, and the delay lines at their modelled noise
floor.

Under those, the patch bar recalls the factory bank: a stepper, a name list and
an EDITED lamp that lights as soon as the panel stops matching the patch that
was recalled. It shows the same programs the host's own program menu does, and
the two stay in step whichever one is used. Re-picking the patch already showing
reloads it, which is how edits are discarded. A patch that cannot be written to
the hardware's format without loss is marked in the list — see MIDI, below.
Volume, the bender depths, portamento and the assign mode are performance
controls rather than patch contents, so recalling a patch leaves them alone,
exactly as the hardware does.

## MIDI

The reference instrument answers to modulation (CC 1), hold (CC 64), all-notes-off
and pitch bend, and to nothing else — it has no continuous controllers for its
panel and its keyboard sends no velocity. YouKnow106 does the same. Host
automation reaches every parameter through the plug-in's own parameter list.

### System exclusive

Patches interchange with the hardware in the hardware's own format, both ways:

| Message | Bytes | Direction |
| --- | --- | --- |
| Patch data | `F0 41 30 0n <18 tone bytes> F7` | in and out |
| Parameter change | `F0 41 32 0n <parameter> <value> F7` | in and out |

An incoming patch dump moves the whole panel; an incoming parameter change
moves only the controls that one byte names and leaves the rest of the patch
alone, so a librarian editing one control does not overwrite the others. The
SEND button emits the current panel as a patch dump on the plug-in's MIDI
output, addressed to the basic channel of the last message that arrived — so a
unit that has already sent anything gets its reply back on its own channel,
without a setting to keep in step. Messages from other manufacturers, other
opcodes, and bodies of the wrong length are ignored rather than partially
applied.

The layout is the instrument's: sixteen continuous controls at 0..127, then two
packed switch bytes. `Source/DSP/YouKnow106SysEx.h` is JUCE-free, so the suite
asserts the byte layout directly.

One setting cannot make the trip. The patch memory holds chorus as an on/off
bit plus a mode bit, so it can say off, I or II but not I+II — that is a limit
of the format, not of this writer, and the hardware cannot store it either.
Such a patch is written out as II, the nearer of the two in rate, and the patch
bar marks it `(I+II)` so a bank about to be sent can be checked first.

## Build on macOS

```bash
cd youknow106
./scripts/build-macos.sh
```

The script configures with Xcode, builds universal `arm64`/`x86_64` binaries,
runs the CTest suite, and ad-hoc signs the resulting bundles under
`build-macos/YouKnow106_artefacts/Release/`. It needs CMake 3.22+ and a full
Xcode installation selected for command-line use. First-time configuration
fetches JUCE 8.0.14, pinned to an immutable archive and SHA-256; a local
checkout of that exact release can be supplied through `JUCE_PATH` instead.

Set `BUILD_UNIVERSAL=OFF` for a native-architecture-only build.

## Build and test without JUCE

The DSP core, the chorus and the panel description are JUCE-free, so both
non-plug-in suites build and run on any C++20 toolchain — which is what Linux CI
exercises:

```bash
cmake -S youknow106 -B youknow106/build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DYOUKNOW106_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build youknow106/build-dsp --parallel
ctest --test-dir youknow106/build-dsp --output-on-failure
```

There are three suites:

- **`YouKnow106.Circuit`** compares the model against something independent for
  every block: the four transconductor stages against a fourth-order
  Runge-Kutta solve of the same ODE at 16x *and* against the closed-form
  `1/(4 − k)`; the note timer against integer division; the cutoff law against
  the instrument's two service calibration anchors; and the delay line against
  its part's datasheet delay range.
- **`YouKnow106.Engine`** checks what the instrument does when it is played:
  that RANGE transposes by octaves, that the sub is an octave down, that the
  alias floor stays below −55 dB, that the ramp's harmonics follow `1/n`, that a
  seventh held key is dropped rather than stealing a voice, that unison does not
  beat, that output level is independent of host rate and of oversampling, that
  the engine is deterministic and exactly silent when idle, and that hostile
  automation cannot produce a non-finite sample.
- **`YouKnow106.PluginProcessor`** (macOS/plug-in builds only) checks the
  parameter contract, state round-tripping and migration, controller transport,
  and that the editor lays out and renders at its extreme sizes.

## Sign, package and notarize

```bash
cd youknow106
./scripts/sign-and-package-macos.sh
```

With no signing identities set this ad-hoc signs and produces an unsigned
installer, which is what the nightly workflow ships. For public distribution,
supply your own identities:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Name (TEAMID)" \
NOTARY_PROFILE=your-notary-profile \
./scripts/sign-and-package-macos.sh
```

## Layout

```text
CMakeLists.txt   Self-contained project; the DSP target builds without JUCE
Source/DSP/      Engine, chorus and the JUCE-free panel description
Source/          Plug-in processor and editor
Tests/           Circuit, engine and plug-in suites
Docs/            Circuit-modelling research and the committed editor screenshot
Presets/         Sound-design recipes
scripts/         macOS build and packaging helpers
```

## Licensing

Original source under the [MIT License](LICENSE); see the
[third-party notices](THIRD_PARTY_NOTICES.md). YouKnow106 builds against JUCE,
which is separately licensed — review the JUCE 8 terms before distributing a
binary.
