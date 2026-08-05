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
Every constant still voiced is listed as a standing, LLM-ready research task
with an explicit evidence gap and required output in
[open questions](Docs/open-questions.md).

> **Listen first.** Ten [rendered demonstrations](Docs/audio/README.md) cover
> the classic pad and PWM strings, the 16' bass, the self-oscillating filter,
> the chorus modes, unison glide, the delayed vibrato, the high-pass ladder
> and the optional deterministic Unit Character profile. They are rendered by
> the shipping engine, so they cannot drift from what the plug-in does.

## What makes it a circuit model rather than a lookalike

- **The oscillator is a divider, not a phase accumulator.** Pitch is one
  reference clock divided by a 16-bit integer, so it is quantised exactly as the
  hardware's is — A4 at 8' programmes count 4545 and sounds 440.044 Hz. The
  RANGE switch changes the clock reaching the counter, not the count, so it
  transposes by whole octaves and the tuning error is the same in all three.
  A voice-CPU timer restart still occurs at its scanned pitch write, but its
  off-phase ramp/comparator/divider discontinuities enter the existing
  BLEP/BLAMP timeline instead of clearing it into a broadband click.
- **The control path is a scanned converter.** The service timing chart shows
  18 per-card holds—DCO, VCF and ENV/GATE VCA for six cards—and five shared
  holds—SUB, stored VCA LEVEL, PWM, RESONANCE and NOISE—over a 4.2 ms pass.
  The engine executes the chart's exact 23-write logical order on a fractional
  4.2 ms scheduler. A normalized compatibility profile keeps those writes
  sequential across the pass—avoiding an artificial six-DCO phase lock—without
  claiming its offsets as measurements. Exact timestamps, jitter and several
  hold constants remain open; a phase-zero profile exists only for diagnostics.
- **The envelope attacks in a straight line and falls exponentially, into a
  quasi-linear amplifier.** Its 14-bit recurrence, `128b` sustain mapping,
  coefficient selection, rounding, physical `E>>2` 12-bit DAC truncation and
  retrigger behavior are exact for the
  explicitly hash-identified B-2 image; no ROM or coefficient-table contents
  are shipped. Physical pass timing and other firmware revisions remain open.
  The voice-VCA knee is still a voiced compatibility fit pending the dense
  original-module sweep in the evidence queue.
- **Cutoff modulation is summed in converter counts before the antilog stage**,
  at 1143 counts per octave, so every modulation source is exponential in hertz.
  The law is anchored on the instrument's own service calibration: converter
  code 6272 self-oscillates at 248 Hz, and the test suite asserts it.
- **The voiced resonance profile compensates on the input side.** In the
  current YouKnow106 compatibility sound, raising resonance drives *more*
  signal into the filter, so a high-Q patch gets dirtier rather than thinner.
  That analogue compensation law remains an explicit OQ-09 target, not an
  original-unit measurement claim.
- **The key assigner drops notes rather than stealing them**, because that is
  what the hardware does with seven keys held on six voices.
- **POLY 1 + POLY 2 is Solo Unison.** All six DCOs receive the same divider
  count, so there is no deliberate detune, but the physical oscillators keep
  free-running behind their closed VCAs. They are summed at whatever phases
  they have when the key is assigned; the engine does not reset all six onto
  one artificial phase or divide the stack by six.
- **The POLY switches are momentary firmware inputs, not independent
  toggles.** Their lamps show the assigner's latched mode, so the neither-lamp
  state cannot be stable. Re-pressing the lit mode rebuilds the held-note
  assignments; pressing one control selects that single mode, while pressing
  both together enters Solo Unison. In the mouse UI, Shift-clicking either
  POLY control is the explicit equivalent of that simultaneous press.
- **VCA LEVEL is patch matching, not another envelope depth.** ENV/GATE drives
  each voice module's VCA. The stored VCA LEVEL byte drives one shared
  uPC1252H2 after the voice sum and high-pass and before the chorus, as it does
  on the jack board. Each voice reaches that bus through 33 kOhm against the
  summer's 3.3 kOhm feedback, so it is attenuated by exactly 0.1 before it can
  drive the shared VCA or BBDs. This is why patches can store their own output
  trims without changing their envelope law.
- **The chorus has no compander**, so it hisses — the hiss is modelled, and
  there is a control to defeat it that the hardware does not have. The final
  mixer gains dry by `100/39` and wet by `100/47`, putting wet at `39/47` of
  dry (−1.62 dB). Those absolute gains occur after the BBDs, so they do not
  falsely overdrive the delay-line model. Its exact JUNO-106 sweep and absolute rates remain
  unmeasured; the current 1.66–5.35 ms and 0.513/0.863 Hz values are explicitly
  a reported JUNO-60 fallback, not a JUNO-106 claim.
- **The final outputs are AC-coupled before VOLUME.** The two service-schematic
  paths use C17/C20 10 µF and R54/R57 1.5 kΩ into the 10 kΩ pot tracks. The
  unloaded full-track reference is 1.383956 Hz and `10/11.5` settled gain; the
  engine solves the actual corner/gain against shaft position and fixed wiper
  loads while retaining independent capacitor states. This removes the large
  DC offset an asymmetric manual-PWM patch would otherwise send to a host.
- **The common signal path now includes all three established coupling
  boundaries.** C14/R39 couples the six-voice sum into the selected HPF leg;
  C12/R36 couples that result into the common uPC1252H2 VCA; C17/C20 couple the
  complete stereo IC6 outputs into VR1. C12/R36 is 0.482288 Hz. C14 is
  0.820915 Hz in Boost/Flat (`33k || 47k`) and 0.482288 Hz at the sub-hertz
  asymptote of Cut I/II, where their series cut capacitors are open.
- **Main VOLUME follows the marked part.** Service documentation identifies
  VR1 as `10KB×2`; Panasonic's later JIS/EIAJ table maps plain `B` to the
  nominal-linear `1B` group (40–60% at mid-travel), so the engine uses linear
  track resistance instead of its former squared taper guess. The table's
  separate S-shaped volume law is `3BM`, not plain `B`. The
  fixed 41.3 kΩ selector ladder and 101 kΩ headphone input load each wiper in
  the model, giving 0.4763 normalized gain at half travel. Real dual-gang
  tracking and the physical output-jack path—selected-tap loading, R64/R65,
  C21/C22, jack normaling and external loads—remain explicit measurements.
  Schema-1 saved states remap the former squared value to the nearest new
  position of equal static gain; host-owned automation lanes cannot be rewritten.
- **Noise density does not move with the HQ switch.** The shared noise source
  and microscopic voice-card excitation are normalized to elapsed time rather
  than internal sample count. A quality change still waits for voices and
  musical tails; a block-size-independent 5 ms fade hides the unavoidable
  rate-dependent rebuild while preserving the host-rate output-capacitor state.

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
have — Transpose, Master Tune, Velocity, Unit Character, Chorus Noise and
Polyphony — plus HQ, Panic and two randomisers. Unit Character is the optional
deterministic voice-variation amount; zero is the declared calibrated nominal
product baseline because real post-calibration distributions remain unmeasured.
The other extension defaults are inert or hardware-aligned: velocity does
nothing, polyphony is six voices, and the delay lines retain their modeled noise
floor.

Under those, the patch bar recalls the factory bank: a stepper, a name list and
an EDITED lamp that lights as soon as the panel stops matching the patch that
was recalled. It shows the same programs the host's own program menu does, and
the two stay in step whichever one is used. The dedicated RELOAD button recalls
the current patch again and discards panel edits; selecting an already-selected
item is not relied on because host widgets do not report that as a change.
Volume, the bender depths, portamento and the assign mode are performance
controls rather than patch contents, so recalling a patch leaves them alone,
exactly as the hardware does.

## MIDI

YouKnow106 receives notes, pitch bend, modulation (CC 1), hold (CC 64),
all-notes-off and the reference instrument's Patch Selection Program Changes.
CC 1 drives the bender lever's forward/LFO axis; the panel's BENDER LFO setting
determines its depth. The keyboard sends no velocity, and MIDI has no continuous
controller assignments for the synthesis panel. Host automation reaches every
parameter through the plug-in's own parameter list.

The incoming Program Change map follows the owner's manual exactly: A11..A18
are 0..7, A21..A28 are 8..15, B11..B18 are 64..71 and B21..B28 are 72..79.
Those are the 32 memory slots this compact bank ships. Program numbers for the
unshipped groups are ignored, leaving the selected patch and panel unchanged.
Incoming Program Changes are consumed rather than echoed. YouKnow106 does not
transmit Program Changes; that reference-keyboard transmit behavior is distinct
from patch selection receive, and SEND emits only the requested system-exclusive
patch dump.

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

The chorus field has exactly the hardware's three states: Off, I and II. The
owner's manual says I and II cannot be used simultaneously, and the jack board
receives one enable line plus one binary I/II line. The two panel buttons are
therefore mutually exclusive. Sessions made by an older YouKnow106 build may
contain its invented both-buttons state; loading one canonicalises that state
to II. It is never rendered as a fourth chorus programme or emitted as a
special patch state.

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

There are five suites:

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
  acquire artificial detune, that assign-mode changes and Solo Unison key-ups
  rebuild from the still-held physical keys, that output level is independent
  of host rate and of oversampling, that final PWM DC is removed, that an HQ
  transition cannot expose a chorus-state reset, that the engine is
  deterministic and exactly silent when idle, and that hostile automation
  cannot produce a non-finite sample.
- **`YouKnow106.PluginProcessor`** (macOS/plug-in builds only) checks the
  parameter contract, state round-tripping and migration, controller transport,
  legacy/modern automation ordering, exact patch reload, and that the editor
  lays out and renders at its extreme sizes.
- **`YouKnow106.SysEx`** checks the documented hardware messages byte for byte,
  including malformed-message rejection and single-parameter switch decoding.
- **`YouKnow106.RenderDemos`** smoke-tests the deterministic documentation-audio
  renderer against the shipping DSP path.

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
Docs/            Circuit-modelling research, open questions, editor screenshot
Presets/         Sound-design recipes
scripts/         macOS build and packaging helpers
```

## Licensing

Original source under the [MIT License](LICENSE); see the
[third-party notices](THIRD_PARTY_NOTICES.md). YouKnow106 builds against JUCE,
which is separately licensed — review the JUCE 8 terms before distributing a
binary.
