# Mars

Mars is an original virtual-analog polyphonic synthesizer built around a direct,
one-panel workflow. It combines independently switchable dual-VCO tone,
controlled voice-to-voice movement, a published nonlinear Moog-ladder model,
an SEM-inspired multimode filter, and a global stereo ensemble. The individual
models have named research and hardware references; Mars does not claim to be a
complete clone of any one vintage instrument.

There is **no arpeggiator** and **no modulation matrix**. Every sound parameter
is exposed as a front-panel knob, slider, or switch and as a host-automatable
parameter. The separate HQ oversampling switch is persisted with the plug-in
state but intentionally cannot be automated.

![Mars Standalone instrument interface](Docs/screenshots/mars-standalone.png)

The screenshot is the actual Standalone application built from this source;
the VST3 and Audio Unit use the same resizable JUCE editor. Panel materials,
hardware pots, faders, switches, calibration marks, and shadows are drawn as
resolution-independent JUCE graphics, so the background and controls scale
together. Interactive controls remain native components for automation,
keyboard operation, and accessibility. The graphite-and-ivory chassis, orange
signal markings, cyan controls, and restrained red accents nod to early-1980s
Japanese polysynths without reproducing a branded hardware panel.

> **Just want to try it?** The scheduled Nightly workflow publishes the latest
> successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> The bundles are ad-hoc signed and not notarized; check the repository's Nightly
> badge for the latest workflow result.

## Sound architecture

- **Two analog-conditioned, event-corrected oscillators per render slot:** VCO I
  and VCO II provide saw, pulse, and stable leaky-integrated triangle waveforms.
  Standard polyBLEP correction is applied to saw resets and both pulse edges;
  the saw then receives the frequency-dependent first-order contour published
  for measured Minimoog Voyager waveforms. Its 44.1 kHz pole and zero are
  bilinear-remapped to the active internal rate, while notes below the measured
  86 Hz boundary blend toward the neutral antialiased saw. Both VCO paths use
  the same bounded output-stage shaping. VCO II has octave, semitone, and fine
  tuning, while the mixer adds pulse width, a pulse sub oscillator one octave
  below VCO I, noise, and bounded VCO II-to-I cross modulation.
- **Independent VCO mixer switches:** each VCO can be removed from the audible
  mix without stopping its phase. VCO II therefore remains available to cross
  modulation while its audio switch is Off, and sub/noise remain independent.
  A lone enabled VCO runs at unity regardless of `Balance`; changes use a short
  gain ramp rather than a hard sample edge.
- **Antialiased nonlinear mixer:** the active oscillator feeds, sub, and noise
  pass through a fixed first-order ADAA soft saturator. This reduces waveshaper
  aliasing without coupling the filter's `Drive` control into multiple stages.
- **Two filter models:** `Ladder` uses a bounded, residual-decreasing damped
  Newton solution of the original implicit bilinear four-stage transistor-
  ladder equations described by D'Angelo and Valimaki. A differential-pair
  nonlinearity remains inside every stage; feedback has no artificial sample
  delay, no state above the documented equation-residual ceiling is committed,
  the full 20 kHz control range remains stable, and `(1 + k)` DC-gain
  compensation restores the severe low-band loss that otherwise accompanies
  rising resonance. `SEM` is a
  nonlinear, two-integrator TPT state-variable design inspired by the Oberheim
  topology; `Filter shape` sweeps low-pass through notch to high-pass. A 3 ms
  transition runs both models to prevent switching clicks; at steady state the
  voice executes only the selected algorithm.
- **Configurable rate-aware oversampling:** HQ is persisted, non-automatable,
  and On by default. At host rates through 48 kHz, On runs complete per-slot
  voice paths at 2x and returns their stereo sum through a 15-tap halfband FIR;
  above 48 kHz the host is already in the target high-rate range, so it runs
  natively. Off always runs natively. A requested change waits until the engine
  is idle before its processing rate changes, so held notes are never reset.
  The global ensemble remains at host rate.
- **Deterministic voice cards:** 32 render slots carry controlled
  component-like offsets for tuning, cutoff, resonance, drive, envelope time,
  pan, pulse skew, and slow per-card drift. The same state and MIDI input
  render deterministically; there is no simulated-parts-wear control.
- **Dedicated modulation:** separate filter and amplifier ADSRs sit beside a
  triangle, sine, or sample-and-hold LFO with direct pitch, filter, and PWM
  depths. The mod wheel deepens those fixed LFO routes; it does not open a
  hidden routing matrix.
- **Performance controls:** `Poly`, `Unison`, and `Fifth` allocate the 32 render
  slots in different groups. Glide, velocity response, stereo spread, fixed
  ±2-semitone pitch bend, MIDI CC 1 mod wheel, and MIDI CC 64 sustain are
  implemented. CC 123 follows note-off and sustain-pedal semantics; CC 120 and
  the panel Panic button mute immediately.
- **Full-range on-screen keyboard:** all MIDI notes 0–127 are reachable with
  scroll controls; key width follows the editor size so the keyboard does not
  terminate in an unused blank panel. The computer-key map is printed on the
  panel.
- **Global stereo ensemble:** a cross-fed, low-pass-shaped modulated delay has
  direct `Ensemble mix` and `Ensemble rate` controls. It is one global
  algorithm rather than a selectable family, with no additional hidden spatial
  stages. A 1.5 Hz output servo removes accumulated DC without thinning deep
  notes and sub-octave fundamentals.

The modeling rationale, primary papers, neural-modeling decision, and precise
claims boundary are in
[`Docs/analog-modeling-research.md`](Docs/analog-modeling-research.md).

## Polyphony and slot allocation

Mars has 32 DSP render slots and a maximum of 16 simultaneously held note
groups. Allocation depends on `Voice mode`:

| Mode | Slots per note group | Maximum note groups |
| --- | ---: | ---: |
| `Poly` | 1 | 16 |
| `Unison` | selected `Unison voices` value, 2–8 | `min(16, floor(32 / voices))` |
| `Fifth` | 2: root plus a perfect fifth | 16 |

When a new group needs room, Mars steals the oldest released group first and
then the oldest held group. Retriggers and steals preserve a fixed 2 ms fading
tail to avoid a hard sample discontinuity. `Unison voices` is active only in
`Unison` mode.

## Exact 43-parameter contract

| Section | Front-panel controls (parameter IDs) |
| --- | --- |
| VCO I | Mixer feed On/Off (`osc1Enabled`), waveform (`osc1Wave`), octave (`osc1Octave`) |
| VCO II | Mixer feed On/Off (`osc2Enabled`), waveform (`osc2Wave`), octave (`osc2Octave`), tune (`osc2Tune`), fine tune (`osc2Fine`) |
| Mixer | Oscillator balance (`oscMix`), pulse width (`pulseWidth`), sub level (`subLevel`), noise level (`noiseLevel`), cross modulation (`crossMod`) |
| Filter | Model: `Ladder` / `SEM` (`filterModel`), cutoff (`cutoff`), resonance (`resonance`), drive (`filterDrive`), SEM shape (`filterShape`), envelope amount (`filterEnvAmount`), key tracking (`keyTrack`) |
| Filter envelope | Attack (`fAttack`), decay (`fDecay`), sustain (`fSustain`), release (`fRelease`) |
| Amplifier envelope | Attack (`aAttack`), decay (`aDecay`), sustain (`aSustain`), release (`aRelease`) |
| LFO | Waveform: triangle / sine / sample & hold (`lfoWave`), rate (`lfoRate`), pitch depth (`lfoPitch`), filter depth (`lfoFilter`), PWM depth (`lfoPwm`) |
| Voice | Mode: `Poly` / `Unison` / `Fifth` (`voiceMode`), unison voices (`unisonVoices`), voice-card drift (`drift`), stereo spread (`spread`), glide time (`glide`), velocity response (`velocity`) |
| Output | Ensemble mix (`chorusMix`), ensemble rate (`chorusRate`), output level (`output`) |
| Quality | HQ oversampling (`hqOversampling`): persisted, non-automatable, default On |

These are the complete host parameter IDs for version 1.2: 42 automatable sound
controls plus one persisted quality setting. The original 40 IDs retain their
order and version hint; `osc1Enabled` and `osc2Enabled` are appended as
version-2 automation parameters, and non-automatable `hqOversampling` is
appended with version hint 3. States that predate any appended parameter migrate
the missing VCO switches and HQ oversampling to On. There are no other sound
controls hidden behind a matrix or alternate panel.

## Build products

One JUCE codebase produces:

- VST3 instrument;
- Audio Unit v2 music device on macOS; and
- standalone application.

## Requirements

- macOS 11 or newer for the supported release bundles;
- a current full Xcode installation selected for command-line use;
- CMake 3.22 or newer; and
- internet access on first configure, or a local JUCE 8.0.14 checkout supplied
  through `JUCE_PATH` to the helper (`MARS_JUCE_PATH` at the CMake layer).

JUCE 8.0.14 is pinned to an immutable release archive and checksum. It is not
vendored into the repository.

## Build on macOS

From the `mars` directory:

```bash
./scripts/build-macos.sh
```

Use a local JUCE checkout without a FetchContent download:

```bash
JUCE_PATH="$HOME/SDKs/JUCE-8.0.14" ./scripts/build-macos.sh
```

The helper configures an Xcode project, builds universal `arm64` + `x86_64`
Release products by default, runs the CTest suite, and ad-hoc signs and strictly
verifies the local VST3, AU, and standalone bundles. Set
`BUILD_UNIVERSAL=OFF` for a faster native-architecture development build.

Equivalent configure, build, test, and local-signing commands:

```bash
cmake -S . -B build-macos -G Xcode \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DMARS_BUILD_UNIVERSAL=ON \
  -DMARS_BUILD_PLUGIN=ON \
  -DBUILD_TESTING=ON

cmake --build build-macos --config Release --parallel
ctest --test-dir build-macos -C Release --output-on-failure

for bundle in \
  build-macos/Mars_artefacts/Release/VST3/Mars.vst3 \
  build-macos/Mars_artefacts/Release/AU/Mars.component \
  build-macos/Mars_artefacts/Release/Standalone/Mars.app; do
  codesign --force --sign - "$bundle"
  codesign --verify --deep --strict "$bundle"
done
```

| Format | Release artifact |
| --- | --- |
| VST3 | `build-macos/Mars_artefacts/Release/VST3/Mars.vst3` |
| Audio Unit | `build-macos/Mars_artefacts/Release/AU/Mars.component` |
| Standalone | `build-macos/Mars_artefacts/Release/Standalone/Mars.app` |

## JUCE-free DSP build

The synthesis engine and its regression suite do not depend on JUCE:

```bash
cmake -S . -B build-dsp \
  -DCMAKE_BUILD_TYPE=Release \
  -DMARS_BUILD_PLUGIN=OFF \
  -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The DSP tests cover rates through 384 kHz, finite output, release completion,
deep-note and long-triangle stability, deterministic rendering, distinct
oscillator and filter responses, the Ladder's full cutoff range, bass-gain
compensation, and implicit-equation residual/state error against an independent
double-precision reference, an adversarial ladder control-jump regression, VCO
mixer isolation and clickless switching, cross modulation with VCO II's audio
feed disabled, deferred HQ mode changes,
meaningful glide and modulation, voice-mode allocation, and a CPU regression
guardrail. Plug-in builds additionally test the 43-parameter, persistence,
migration, MIDI, and editor contracts.

## Install and validate locally

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"

ditto build-macos/Mars_artefacts/Release/VST3/Mars.vst3 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Mars.vst3"
ditto build-macos/Mars_artefacts/Release/AU/Mars.component \
  "$HOME/Library/Audio/Plug-Ins/Components/Mars.component"
```

Validate the Audio Unit (`aumu`, subtype `Mar1`, manufacturer `Mars`):

```bash
auval -v aumu Mar1 Mars
```

Validate the VST3 with pluginval if it is installed:

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 10 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Mars.vst3"
```

## Sign, package and notarize

Build first, then run:

```bash
./scripts/sign-and-package-macos.sh
```

With no environment overrides the helper ad-hoc signs the VST3, AU, and app,
then writes a ZIP and unsigned installer package under `build-macos/dist`. The
filename records the architectures actually present (`universal`, `arm64`, or
`x86_64`) so a native development build cannot be mislabeled.
The helper derives its version from the three bundle property lists, rejects a
conflicting `VERSION` override, and includes the Mars license, third-party
notices, and pinned JUCE dual-license notice in the installer tree and inside
each signed bundle.
For distribution, provide `APP_SIGN_IDENTITY`, `INSTALLER_SIGN_IDENTITY`, and
optionally a `NOTARY_PROFILE` created for `xcrun notarytool`. Replace the sample
bundle identifier and manufacturer/plugin codes with identifiers
controlled by the publisher before shipping public binaries.

With `NOTARY_PROFILE` set, the helper submits and staples the installer package.
The ZIP still contains signed bundles but is not itself the notarized
distribution artifact.

## Project layout

```text
Docs/                    Real interface screenshots, research, and modeling decisions
Source/DSP/              JUCE-free synthesis engine
Source/PluginProcessor.* MIDI, automation, state, and engine bridge
Source/PluginEditor.*    Resizable direct-control hardware panel
Tests/                   DSP and plug-in regression checks
Presets/                 Original sound-design recipes
scripts/                 macOS build and distribution helpers
```

## Licensing

The original Mars source is offered under the MIT License. JUCE is a separate
dependency and is **not** covered by that license. JUCE 8 is available under
AGPLv3 or a commercial JUCE licence; confirm the applicable terms before
distributing binaries. In particular, a publisher must establish its own JUCE
commercial or AGPLv3 distribution basis; bundling the notice does not grant or
replace that licence.
