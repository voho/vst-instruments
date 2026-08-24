# YouKnow106

A six-voice circuit-modelled DCO polysynth for macOS, built as a self-contained
JUCE project: VST3, Audio Unit and Standalone, universal `arm64`/`x86_64`.

YouKnow106 is published by [Protocodus](https://protocodus.cz) and supports
macOS 11 or later. Start with the [user guide](Docs/USER_GUIDE.md), see
[customer-facing changes](CHANGELOG.md) and [privacy information](PRIVACY.md),
or contact [protocodus@proton.me](mailto:protocodus@proton.me). This repository
is currently a release candidate: no commercial binary should be published
until every blocker in the [release checklist](Docs/RELEASE_CHECKLIST.md) is
resolved and recorded.

YouKnow106 models the voice architecture of a 1984 six-voice polysynth — the
Roland Juno-106 — block by block, from its integer-divided note timers through
its four-pole transconductor filter to its uncompanded bucket-brigade chorus. It
is an independent original implementation, not affiliated with or licensed by
Roland Corporation, and it contains no firmware, ROM data, samples or captured
audio. It does include the original 128 factory tone-memory states as functional
18-byte parameter data, independently decoded and checksum-verified as described
below; no Roland Cloud content was extracted. Its panel follows that
instrument's functional geometry and 1980s colour vocabulary while retaining
independent branding, typography and project-drawn controls.

This README is the overview: what is simulated, what is not, and how to use
and build the instrument. Everything behind it — the evidence for each
constant, the open questions, the settled guardrails, the decision log and
the sources — is one document,
[Docs/research.md](Docs/research.md). Detailed measurement and change
history lives in git.

> **Listen first.** Ten [rendered demonstrations](Docs/audio/README.md) cover
> the classic pad and PWM strings, the 16' bass, the self-oscillating filter,
> the chorus modes, unison glide, the delayed vibrato, the high-pass ladder
> and the optional deterministic Unit Character profile. Ten additional
> [factory-preset previews](Docs/audio/factory-presets/README.md) retain
> their relative levels with one shared gain rather than per-file
> normalisation.

## Contents

- [User guide](Docs/USER_GUIDE.md)
- [Commercial release checklist](Docs/RELEASE_CHECKLIST.md)
- [What is simulated](#what-is-simulated)
- [What is not simulated](#what-is-not-simulated)
- [Voices, character and aging](#voices-character-and-aging)
- [Interface](#interface)
- [Original factory bank](#original-factory-bank)
- [MIDI](#midi)
- [Performance and quality](#performance-and-quality)
- [Build on macOS](#build-on-macos)
- [Build and test without JUCE](#build-and-test-without-juce)
- [Sign, package and notarize](#sign-package-and-notarize)
- [Layout](#layout)
- [Licensing](#licensing)

## What is simulated

Each mechanism carries an evidence class, defined and tracked per constant
in [Docs/research.md](Docs/research.md): **anchored** (service
documentation, datasheet, firmware analysis or calibrated measurement),
**ROM-resolved** (exact for one hash-identified firmware image),
**derived** (arithmetic from anchors), **product policy** (a plug-in
decision, no hardware claim) or **voiced** (provisional, inside a range the
evidence bounds but does not fix). "Exact" never means an arbitrary
forty-year-old unit will null against the plug-in.

**Digital control system**

- Pitch is one 8 MHz reference divided by a 16-bit integer count, quantised
  exactly as the hardware's is; RANGE switches the clock, not the count
  (anchored).
- One 12-bit converter scans 23 sample-and-hold destinations — 18 per-card,
  5 shared — over a 4.2 ms pass in the service chart's exact write order,
  on a fractional scheduler with per-destination smoothing constants
  (anchored order and constants; the intra-pass offsets are compatibility
  policy, with a pixel-measured chart-geometry profile selectable).
- Envelope recurrence, sustain mapping, DAC truncation, the delay-gated LFO
  reaching pitch, filter and pulse width together, and the portamento glide
  law are the exact digital behaviour of the hash-identified B-2 firmware
  (ROM-resolved); key assignment — including note dropping instead of
  stealing, the momentary POLY contacts and Solo Unison — is ROM-resolved
  for the A-5 assigner image.
- The portamento knob passes through its loaded 50KB pot law (derived).

**Oscillator**

- A straight constant-current ramp with finite reset, a comparator-based
  pulse against the shared scanned PWM threshold, and a divide-by-two sub
  (anchored topology); the scan-and-slew amplitude transient on pitch
  changes is rendered in the slope of each rise (derived).
- All six DCOs free-run behind their closed VCAs with staggered phase; a
  note opens a card that already has history. Oscillator edges are
  bandlimited (BLEP/BLAMP) as a numerical product mechanism.

**Mixer and noise**

- Sources mute, legs never switch: saw and pulse arrive summed on one WAVE
  node, the sub joins as a diode-gated half-cycle current, one shared Tr21
  noise source with its own 33.9 Hz–4.8 kHz band shaping feeds every voice
  (anchored topology and shaping; the level coordinates remain voiced).
- C56/C50 couple the mixer into the filter, keeping duty-dependent DC out
  of the signal path (anchored).

**Filter and voice amplifier**

- Four transconductor stages with the 68 kΩ/560 Ω attenuation, solved
  directly as their continuous nonlinear equations at fixed cost; cutoff is
  summed in converter counts (1143/octave) and anchored on the service
  calibration point — code 6272 self-oscillates at 248 Hz, which the model
  *predicts* within a cent from a derived harmonic balance rather than
  fitting (anchored law; the upper knee is voiced pending OQ-18).
- The R-2R converter's real mid-scale carry error, the resonance input-side
  compensation Roland's own module drawing prints, self-oscillation trimmed
  where the service manual trims it, and the printed ±10-cent trim
  acceptance windows at the two check points (anchored; the
  panel-to-loop-gain curve shape is voiced, with a circuit-derived
  candidate behind a flag pending a listening verdict).
- Each voice VCA is a current-controlled BA662 behind C59 coupling, with a
  quasi-linear compatibility law above conduction (anchored topology;
  transfer law awaits the OQ-19 measurement).

**Bus and output**

- Six voices sum at 0.1 each; one continuous C14 state feeds the
  four-position high-pass (derived shelf and cut corners, MNA-qualified);
  the stored VCA LEVEL byte drives the common µPC1252H2 through its derived
  gain law and 9.08 ms control settling (anchored/derived).
- The final TA75558 summer mixes dry 100/47 and wet 100/39, bounded by its
  own rail; output AC coupling and the nominal-linear dual 10K volume law
  with its real internal loading follow (anchored/derived). Digital full
  scale is referred to the output stage's own rail — the plug-in cannot
  clip before the circuit it models does (product policy).
- Power-rail droop under load is computed and measurably inert (0.1 cents
  across the full one-to-six-voice change) — reported, not tuned into
  audibility.

**Chorus**

- Two uncompanded 256-stage MN3009 lines driven anti-phase by one triangle
  whose rates (0.5533/0.8983 Hz) and mode ratio are derived from this
  instrument's own oscillator circuit; the 1.4–6.4 ms sweep endpoints are a
  third-party measurement of a designator-faithful build (below the
  anchoring bar; OQ-01).
- BBD write nonlinearity fitted to the part's datasheet, explicit
  zero-order hold plus residual charge-transfer loss at the datasheet
  anchor, full support-filter chains, and hiss at the MN3009's own noise
  row referred through the model's measured transfer — with mode II's
  reported +3.95 dB floor as a relative calibration (anchored/derived;
  absolute noise PSD is OQ-03).

**Instrument-level extensions** (product policy): Unit Character scales
every modelled tolerance from calibrated-nominal (0) through "matches real
hardware" (100%) to exaggerated (200%); Aging drifts the instrument away
from a fresh service along one documented recalibration; Velocity,
Transpose, Master Tune, Chorus Noise (HISS), Polyphony 1–16, the Quality
ladder and the VCF numerical-kernel settings described under
[Performance and quality](#performance-and-quality).

## What is not simulated

Deliberate, and documented in [Docs/research.md](Docs/research.md):

- **No pitch drift and no inter-voice detune.** Pitch is integer division
  of a crystal-derived clock: temperature, supply and ageing have no term
  in that arithmetic, so six voices on one key are always exactly in tune
  and unison carries no detune generator. The crystal's whole tolerance
  budget sits below the pitch quantisation step.
- **No mains ripple.** Derived at ~0.03 cents of cutoff through the
  regulators — below audibility; and neither ripple nor rail droop may ever
  be routed to pitch.
- **No invented behaviour where evidence is missing.** Mechanisms whose
  magnitude the sources cannot fix either ship voiced and labelled (mixer
  level coordinates, resonance curve shape, upper cutoff knee, noise
  distribution) or are left out entirely until measured: the pulse-off
  switching transient, chorus wet-mute click and leakage, HPF mode-change
  transients, converter charge injection, envelope/LFO physical timing
  against a real unit.
- **Removed on review**, with reasons recorded: a voice-VCA thump
  heuristic, a switchable-leg mixer model, a BBD clock-scaled smear
  multiplier, wet-mute distortion ~44 dB too strong, a sub-driver
  amplitude asymmetry that is really an edge-timing effect, and several
  numerically unstable or double-counting candidates.
- Twenty-one open questions (OQ-01…OQ-21) name the hardware evidence that
  would close each remaining gap — most need calibrated captures from an
  identified, serviced unit.

## Voices, character and aging

The first six slots are persistent physical voice-card models: DCO, filter,
comparator and card noise keep running behind a closed VCA, and the shared
converter visits its 23 destinations sequentially, so a unison stack is
never artificially phase-locked. There is no six-oscillator detune
generator; the LFO and envelope generator are shared and digital, exactly
as in the hardware.

At **Unit Character** 0% the engine is the deterministic calibrated-nominal
model. At 100% — the default, the "matches real hardware" reference — a
fixed-seed profile enables the full span of every modelled tolerance:
per-card ramp-current, comparator-threshold, VCA and sub/noise-level
errors (±3% class), VCF trim residuals bounded by the service manual's own
±10-cent acceptance at its two check points, per-stage input offsets and
capacitor staggering, slow cutoff wander, the R-2R carry error, and the
chassis warm-up law `25 + 15(1 − e^{−t/900})` °C with its spatial gradient
across the cards. Everything scales linearly with the knob, seeds are
fixed, and the same patch renders identically every launch. These spans are
voiced sound design, not measured population statistics — OQ-10 owns the
data that would replace them.

**Aging** sits beside Unit Character: zero is a freshly serviced
instrument; raising it drifts each voice's filter trim flat by its own
share of up to a quarter tone and lifts the noise source by up to 3.5 dB,
following one documented four-year recalibration. It describes the
instrument, not the patch, so randomisation, program recall and INIT leave
it alone.

## Interface

The interface keeps the reference instrument's control inventory **and its
reading order** in a 1360×718 opening window. The synthesis strip is LFO,
DCO, HPF, VCF, VCA, ENV and CHORUS; VOLUME and PORTAMENTO live in the left
performance cheek with the bender-depth faders, portamento switch and
spring lever; the 61-key keyboard begins beside that cheek. Below the strip
is the hardware programmer tier: POLY 1/2 plus UNISON, A/B group, BANK and
PATCH keys, the recessed red display, MANUAL, WRITE and tape
SAVE/VERIFY/LOAD — the immutable factory bank makes WRITE and VERIFY
explanatory disabled controls, while selection, MANUAL and SysEx SAVE/LOAD
are live.

Plug-in-only controls stay close to their hardware families: UNISON
completes the VOICE MODE group, HISS sits inside CHORUS, VELOCITY and
VOICES share the lower VOICE group, Transpose and tuning sit under the DCO,
and the global CHARACTER/AGING/QUALITY controls sit beneath the left cheek
with the Voice Monitor beside voice allocation. PANIC, INIT and the graded
VARIATION actions occupy the lower bay. The visual language is planar warm
charcoal with restrained oxblood and teal, project-drawn controls and
independent branding.

Hovering any interactive element updates the fixed help strip below the
keys immediately — explanation plus the control's current value in its own
units — and the same strings are exposed as accessibility metadata. The
oscilloscope ranges itself and prints its gain. The bender lever is live
performance input (pitch left/right, modulation up, spring to zero); it
drives the same controller scan as external Pitch Wheel and CC 1 and does
not enter patches or automation. The host preset rail recalls the factory
bank with a stepper, name list, RELOAD and a LOADED/EDITED indicator, in
sync with the host's own program state.

## Original factory bank

YouKnow106 includes all 128 original tone-memory states in the physical
instrument's order, A11–A88 then B11–B88 — each the hardware's complete
18-byte state with no corrective gain, hidden EQ or per-preset rebalancing.
The bytes were mechanically decoded and cross-checked with zero mismatches
across the public [Hinzen tape/PAT archive](http://www.hinzen.de/midi/juno-106/),
the [Jarvik7 librarian factory library](https://www.jarvik7.net/juno-106/)
and the [KR-106 archival transcription](https://github.com/kayrockscreenprinting/ultramaster_kr106/tree/bc15caee5843ab238a25d0969e68d57db2b1615f/tools/preset-gen);
Roland independently describes the historical 64+64 set. The test suite
locks the payload checksum, slot order and a round-trip for every tone. No
Roland Cloud content was downloaded or extracted.

What each preset carries besides its bytes is a VR1 volume shaft position —
the one control a player moves when one patch arrives hotter than the last.
The [factory gain audit](Docs/audio/factory-presets/README.md) renders all
128 tones through the shipping engine and enforces two contracts as build
failures: no preset peaks above −1 dBFS and none exceeds −31 dBFS gated
RMS. The trims are attenuation only, and below those ceilings the level
differences are measurements, not targets — the quiet noise sweeps are
quiet because the instrument makes them quiet.

The hardware stores positions, not names; labels such as "Brass Set 1" are
conventional archival descriptions shown for navigation, not Roland-authored
text. Host/patch-bar recall restores the full playing setup; hardware MIDI
Program Change and SysEx keep their narrower authentic semantics and move
only the 18-byte tone. Loading a pre-schema-3 session preserves every saved
parameter but resets the selector to an edited INIT panel rather than
attaching an unrelated factory name. See
[third-party notices](THIRD_PARTY_NOTICES.md) for provenance.

## MIDI

The on-screen keyboard matches the physical 61-key C2–C7 span; host MIDI
outside the keybed is not discarded. YouKnow106 receives external pitch
bend, modulation (CC 1), hold (CC 64 — split at zero exactly as the owner's
MIDI chart prints it, so any nonzero value holds), all-notes-off (mode
messages 123–127 are all recognised, as the chart specifies) and the
reference instrument's Patch Selection Program Changes (0..63 → A11..A88,
64..127 → B11..B88, consumed rather than echoed). The modelled keybed is
not velocity sensitive, so incoming velocity reaches the engine only
through the VELOCITY extension. There are no MIDI CC assignments for the
synthesis panel; host automation reaches every stored parameter through the
plug-in's parameter list. YouKnow106 does not transmit performance data or
Program Changes.

### System exclusive

The SysEx codec reads and constructs the hardware's own format in both
directions:

| Message | Bytes | Codec support |
| --- | --- | --- |
| Patch data | `F0 41 30 0n <18 tone bytes> F7` | decode and encode |
| Parameter change | `F0 41 32 0n <parameter> <value> F7` | decode and encode |

An incoming patch dump moves the whole panel; an incoming parameter change
moves only the controls its byte names. Foreign manufacturers, other
opcodes and wrong-length bodies are ignored rather than partially applied.
Patch files carry the same messages: LOAD (or dropping a `.syx` on the
editor) applies the first patch dump in the file, SAVE writes the current
tone as one hardware-valid dump. Performance controls stay out of the
file, exactly as they stay out of the hardware's tone memory. The chorus
field has exactly the hardware's three states — Off, I, II; sessions from
older builds carrying the invented both-buttons state canonicalise to II.

## Performance and quality

The QUALITY selector offers a 1×/2×/4× internal-rate ladder applied as a
ceiling against what the host rate needs; engine cost tracks the applied
factor nearly linearly, and the worst audited six-voice resonant scenario
measures 0.85× realtime at 4× on one Apple M1 Max core (0.23× at 1×). New
instances ship at 1× — a deliberate cheapest-first product decision the
project's own numerical audits argue against (the BBD and VCF domains pass
their absolute gates only at 4×); 2× and 4× are one menu away.

The VCF SOLVER selector descends a solver ladder (Max/High/Normal) for the
nonlinear filter. Normal — the cheapest rung, roughly half the filter's
CPU — ships as the default, chosen by a blind listening test (2026-08-23)
that returned no audible difference between any rung, beside measured
whole-file nulls of −88…−110 dBc. The engine's own default stays the
reference Merson kernel, so every frozen fingerprint keeps testing it. Two
further machine settings (VCF Tanh, VCF Fast Early) select the solver's
numerical kernels, with the exact forms as defaults. None of these is part
of a patch.

The plug-in reports a fixed 41-host-sample latency covering oscillator
reconstruction and decimation only. A quality change waits until the
instrument is idle; nothing these switches select moves a modelled physical
quantity — noise density and the warm-up clock are normalized to elapsed
time. A host transport stop is treated as a stop, not a power cycle: the
modelled chassis stays warm, while a new `prepare()` starts cold.

## Build on macOS

```bash
cd youknow106
./scripts/build-macos.sh
```

The script configures with Xcode, builds universal `arm64`/`x86_64`
binaries, runs the CTest suite, and ad-hoc signs the resulting bundles
under `build-macos/YouKnow106_artefacts/Release/`. It needs CMake 3.22+ and
a full Xcode installation selected for command-line use. First-time
configuration fetches JUCE 8.0.14, pinned to an immutable archive and
SHA-256; a local checkout of that exact release can be supplied through
`JUCE_PATH` instead. Set `BUILD_UNIVERSAL=OFF` for a native-only build.

## Build and test without JUCE

The DSP core, chorus, panel description and render tools are JUCE-free, so
the non-plug-in suites build and run on any C++20 toolchain — which is what
Linux CI exercises:

```bash
cmake -S youknow106 -B youknow106/build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DYOUKNOW106_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build youknow106/build-dsp --parallel
ctest --test-dir youknow106/build-dsp --output-on-failure
```

There are 15 JUCE-free CTest contracts, 16 with the plug-in suite enabled
and 17 on macOS (which adds the VST3 bundle smoke test). The circuit suite
compares every block against something independent — reference ODE solves,
closed forms, the service calibration anchors, datasheet ranges. The engine
suite checks what the instrument does when played, from octave transposition
and voice dropping through determinism, exact idle silence and hostile
automation. The plug-in suite covers the parameter contract, state
migration, program recall, editor layout at its extreme sizes and the help
strip printing every explanation in full. Renderer and audit contracts pin
the demo corpus, the 128-tone gain audit and the numerical-quality gates
for the oversampling ladder.

## Sign, package and notarize

For an ad-hoc local or nightly package:

```bash
cd youknow106
./scripts/sign-and-package-macos.sh
```

That development path may emit an ad-hoc-signed PKG and ZIP. It must never
be promoted to a paid/public release. Production starts from the clean,
exact `youknow106-v1.1.0` tag and fails closed unless both Developer ID
identities and a working `notarytool` profile are supplied:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Name (TEAMID)" \
NOTARY_PROFILE=your-notary-profile \
./scripts/release-macos.sh
```

The production path builds/tests universal macOS 11 artifacts and publishes
only a signed, notarized and stapled PKG plus its manifest and SHA-256 file.

## Layout

```text
CMakeLists.txt   Self-contained project; the DSP target builds without JUCE
Source/DSP/      Engine, chorus and the JUCE-free panel description
Source/          Plug-in processor and editor
Tests/           Circuit, engine and plug-in suites
Docs/            Research notes and open questions, user guide, audio, screenshots
Presets/         Sound-design recipes
Tools/           Deterministic demo and factory-audit audio renderers
scripts/         macOS build and packaging helpers
```

## Licensing

Original source under the [MIT License](LICENSE); see the
[third-party notices](THIRD_PARTY_NOTICES.md). YouKnow106 builds against JUCE,
which is separately licensed — review the JUCE 8 terms before distributing a
binary.
