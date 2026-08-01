# Mars

Mars is an original virtual-analog polyphonic synthesizer built around a direct,
one-panel workflow. Each oscillator can independently switch between a
Moog-like free-running VCO and a Juno-like clocked DCO. Mars combines those
cores with controlled voice-to-voice movement, a published nonlinear
Moog-ladder model, an SEM-inspired multimode filter, and a global stereo
ensemble. The individual models have named research and hardware references;
Mars does not claim to be a complete clone of any one vintage instrument.

There is **no modulation matrix**. Every sound parameter is exposed as a
front-panel knob, slider, or switch and as a host-automatable parameter. The
separate HQ oversampling switch is persisted with the plug-in state but
intentionally cannot be automated. Version 1.6 adds a free-running,
JUNO-style **arpeggiator** section; it is Off by default and drives the ordinary
voice allocator rather than a hidden second engine.

![Mars synthesizer interface](Docs/screenshots/mars-standalone.png)

The screenshot above still shows the version-1.5 panel: it predates the 1.6
arpeggiator section and header scope and has not been regenerated, because
rendering it requires a macOS plug-in build. It is produced by the actual
minimum-size JUCE editor in the
plug-in regression suite; the Standalone, VST3, and Audio Unit use that same
resizable component. Panel materials,
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

- **Two independently modeled oscillators per render slot:** `Moog-like VCO`
  keeps a free-running fundamental with deterministic component calibration and
  seeded, non-periodic thermal wander. Its saw pairs the paper's four-sample
  integrated third-order B-spline PolyBLEP with the matching frequency-dependent
  first-order contour fitted to measured Minimoog Voyager waveforms, remapped
  from 44.1 kHz to the active internal rate. `Juno-like DCO`
  follows the JUNO-106 chain: an 8 MHz master is range-divided to 1, 2, or
  4 MHz, an 8253-style timer holds one integer divisor until the next control
  write and terminal count, and an event-driven MC5534A surrogate generates
  audio. The roughly 4.2 ms control scan holds pitch current and PWM voltage;
  there is no invented adjacent-count dithering. The MC5534A path uses a 12 V
  Miller ramp, finite reset recovery and charge injection, a held-threshold PWM
  comparator with asymmetric output slew, and rate-invariant reconstruction.
  Its sub path is divider-locked to oscillator I's selected clock period. Saw
  reset, both pulse edges, the triangle's square driver, and the derived DCO sub
  all use the same
  fractional-event, four-sample integrated B-spline correction. The LFO runs
  inside the HQ island, and comparator moves that cross the current ramp add
  their own correction instead of becoming untracked PWM steps. All three
  waveform states stay warm and crossfade for 3 ms. VCO and DCO own separate
  clocks; a 2 ms audio crossfade seeds the destination phase, then lets both
  advance at their physical rates during the transition. At a settled endpoint
  the inactive analogue oscillator core is frozen. Triangle remains
  an explicit Mars extension because the MC5534A exposes saw, pulse, and sub,
  not triangle. Each oscillator selects its model independently, so hybrid
  VCO/DCO patches are possible.
- **Independent oscillator mixer switches:** each oscillator can be removed
  from the audible mix without stopping its phase. Oscillator II therefore
  remains available to cross modulation while its audio switch is Off, and
  sub/noise remain independent.
  A lone enabled oscillator runs at unity regardless of `Balance`; changes use
  a short gain ramp rather than a hard sample edge. Oscillator II has octave,
  semitone, and fine tuning, while the mixer adds pulse width, a pulse sub
  oscillator one octave below oscillator I, noise, and bounded II-to-I cross
  modulation.
- **Antialiased nonlinear mixer:** the active oscillator feeds, sub, and noise
  pass through a fixed first-order ADAA soft saturator. This reduces waveshaper
  aliasing without coupling the filter's `Drive` control into multiple stages.
  The noise generator is calibrated as a density rather than as a fixed
  amplitude, so the `Noise` control delivers the same audible-band level at
  every session rate from 44.1 kHz to 192 kHz and with HQ either on or off. The
  regression suite measures a 0.67 dB spread over those twelve configurations,
  against 5.7 dB before the change. The calibration is referenced to 192 kHz, so
  above that rate the amplitude exceeds unity and the ADAA saturator begins to
  compress it: the engine still accepts up to 768 kHz, where the same reading
  falls 1.4 dB.
- **Two filter models:** `Ladder` uses a bounded, residual-decreasing damped
  Newton solution of the original implicit bilinear four-stage transistor-
  ladder equations described by D'Angelo and Valimaki. A differential-pair
  nonlinearity remains inside every stage; feedback has no artificial sample
  delay, no state above the documented equation-residual ceiling is committed,
  the full 20 kHz control range remains stable, and `(1 + k)` DC-gain
  compensation restores the severe low-band loss that otherwise accompanies
  rising resonance. The Newton updates target a fixed *relative* reduction of
  the residual each step starts from rather than a fixed voltage. A fixed
  voltage target behaved as an amplitude dead zone proportional to `1 / cutoff`:
  at a 100 Hz cutoff every mixer signal below about -26 dBFS was gated to
  silence and at 400 Hz everything below about -40 dBFS was, so quiet material
  simply vanished into a bass patch. The residual floor beneath that relative
  target is scaled to the magnitudes the residual is built from rather than left
  as a fixed voltage, because any fixed floor reinstates the same `1 / cutoff`
  dead zone further down. Passband gain is now level-independent within 1.2%
  over a 60 dB input range at every cutoff across the whole 20 Hz - 20 kHz
  control range. The solve does more work at low signal levels than it used to,
  because it previously did none there: a 16-voice ladder worst case measures
  about 2.5% slower, and the default eight-voice patch is unchanged. The
  resonance
  map crosses the ladder's own `k = 4`
  oscillation threshold in the last few percent of the control, so maximum
  resonance **self-oscillates** at the cutoff instead of merely ringing; the
  limit cycle now also builds itself out of a millivolt-scale disturbance
  instead of needing a large excitation, as a circuit past its threshold does,
  and a voice at maximum resonance sings at every cutoff setting from 20 Hz to
  20 kHz where before it stayed silent below roughly 500 Hz. The disturbance
  still has to clear the deliberate 5.2 uV per-stage idle-silence guard that
  snaps a genuinely resting filter to zero, which is why the isolated-core
  fixture pins the millivolt kick at 200 Hz and above; inside a voice the
  amplifier envelope's own opening transient clears it at every cutoff. The
  transistor pairs bound the limit cycle and the regression suite measures both
  its frequency and its amplitude. Below that knee the feedback gain is
  unchanged. The modulated cutoff is bounded by 20 kHz as well as by 0.45 of the
  host rate, so a bright patch driven to the top of its filter envelope keeps
  the same top octave at 44.1 kHz as at 192 kHz; those two used to differ by
  10 dB at 19.8 kHz. `SEM` is a
  nonlinear, two-integrator TPT state-variable design inspired by the Oberheim
  topology; `Filter shape` sweeps low-pass through notch to high-pass. Its
  resonance loop now saturates where the circuit's OTA does, so a high-Q peak
  compresses into a stable limit cycle (measured 12.5x small-signal gain falling
  to 3.0x at programme level) and gains odd-order colour instead of behaving
  like an ideal linear resonator. A 3 ms
  transition runs both models to prevent switching clicks; at steady state the
  voice executes only the selected algorithm.
- **Configurable rate-aware oversampling:** HQ is persisted, non-automatable,
  and On by default. On a hot, resonant, driven high-note patch the regression
  suite holds its worst audible-band inharmonic product at least 10 dB below the
  native path's at 44.1 and 48 kHz, and prints the margin it actually measures.
  At standard production rates it holds the complete
  nonlinear voice, ensemble, VCA, and output-colour island at a minimum
  176.4 kHz: 4x at 44.1/48 kHz, 2x at 88.2/96 kHz, and native at 176.4 kHz
  and above. Each 2:1 return stage is a
  137-tap equiripple half-band FIR with less than 0.0002 dB passband ripple and
  more than 100 dB stopband rejection. The staged path reports 51 host samples
  of latency at 4x and 34 at 2x. The host latency remains fixed for the prepared
  sample rate; when HQ is Off, a transparent bounded delay aligns its native
  path to that contract. A requested change waits until the engine is idle, so
  held notes are never reset.
- **Deterministic voice cards:** 16 render voices carry controlled
  component-like offsets for tuning, cutoff, resonance, drive, envelope time,
  pan, and pulse skew. Two bounded seeded Ornstein-Uhlenbeck processes retain
  continuous slow/fast pitch wander on each card through notes and silence
  instead of restarting a repeating sine with every note. The same state and
  MIDI input render deterministically; there is no simulated-parts-wear control.
- **Dedicated modulation:** separate filter and amplifier ADSRs sit beside a
  triangle, sine, or sample-and-hold LFO with direct pitch, filter, and PWM
  depths. The mod wheel deepens those fixed LFO routes; it does not open a
  hidden routing matrix.
- **Free-running arpeggiator:** `Arp` plays the held keys through the ordinary
  allocator, so Poly, Unison, Fifth, and Mono all behave exactly as they do from
  the keyboard, including the 2 ms retrigger tail. `Mode` selects Up, Down,
  Up-down, Random, or As played; `Range` repeats the pattern over one to four
  octaves; `Rate` is a free-running control in steps per second rather than a
  host-tempo division, matching the JUNO-6/60/106 convention; `Gate` sets how
  much of each step the note is held, with a fully open gate playing legato;
  `Hold` latches the last chord so the pattern continues after the keys are
  released, and a fresh press starts a new chord. The first key of a phrase
  plays immediately. Turning the arpeggiator off releases its current step and
  hands note handling straight back to the keyboard.
- **Panel signal scope:** the header carries a triggered oscilloscope, an output
  meter with a held peak marker, and the arpeggiator's current step and note.
  The trace reduction, meter ballistics, decibel mapping, and colour ramp live
  in the JUCE-free DSP library and are covered by the regression suite; the
  editor only fetches a lock-free trace and draws it.
- **Bounded performance controls:** a host-automatable `Polyphony` control caps
  active DSP render voices from 1 to 16 and is enforced immediately when
  lowered. `Poly`, `Unison`, and `Fifth` consume that physical budget in
  different group sizes. `Unison detune` is an independent panel control rather
  than a side effect of `Drift`, so a stack can beat tightly without freezing
  the card-to-card component spread; its 9.6-cent default reproduces the
  previous drift-derived spread exactly. The separate `Mono` override uses one continuous
  voice with last-note priority, legato phase/envelopes, glide, held-note
  fallback, overlapping same-pitch hold counts, seamless conversion of held
  layered notes, and fresh-envelope retrigger after the final physical key is
  released. Velocity response, stereo spread, fixed ±2-semitone pitch bend,
  MIDI CC 1 mod wheel, and MIDI CC 64 sustain are implemented. CC 123 follows
  note-off and sustain-pedal semantics; CC 120 and Panic mute immediately.
- **Safe preset mutation:** `1%`, `10%`, and `100%` randomizer buttons move each
  sound-design parameter toward an independent legal target in normalized
  space. One and ten percent are bounded mutations of the current patch; 100%
  is a full-range draw. Output gain, oscillator power, HQ quality, Mono, the
  arpeggiator's own On and Hold switches, and
  the polyphony budget are deliberately preserved.
- **Full-range on-screen keyboard:** all MIDI notes 0–127 are reachable with
  scroll controls; key width follows the editor size so the keyboard does not
  terminate in an unused blank panel. The computer-key map is printed on the
  panel.
- **Global stereo ensemble:** two complementary variable-clock BBD paths replace
  a generic interpolated chorus. Each path stores the 128 signal-bearing pairs
  of a two-phase 256-stage device, uses the
  measured Juno-60 fifth-order input/output-filter poles and residues, retains
  the measured +2.3 dB reference gain after a provisionally fitted charge-loss
  curve, and is
  clocked from 26–74 kHz by an antiphase triangle LFO. An O(1) rolling history
  follows clock-dependent charge retention, a BBD-rate pole captures incomplete
  transfer, and independently seeded event noise sits inside each wet line.
  Imperfect cancellation of the MN3009's complementary outputs produces a
  fractional, two-phase clock-feedthrough residue before the physical output
  filter; it is faded before low-rate Nyquist folding. This runs inside the HQ
  island and does not use a fractional-delay read pointer. Clock crossings use
  fractional-time input capture and time-weighted held-output integration, so
  HQ-Off operation does not shift several buckets with one quantized sample.
  `Ensemble mix` and `Ensemble rate` remain direct modern controls. The `COMP`
  switch adds a nominal-gain-matched, click-smoothed NE570-style
  compressor/expander around the BBD as an explicitly non-Juno studio option;
  authentic mode leaves it Off.
  Parasitics fade only after the signal and complete BBD tail disappear, then
  snap to digital zero so hosts can suspend the instrument. A 1.5 Hz output
  servo removes accumulated DC
  without thinning deep notes and sub-octave fundamentals. Mars reports a
  conservative 24-second host tail so maximum-release notes are not truncated
  during offline rendering.

The modeling rationale, primary papers, neural-modeling decision, and precise
claims boundary are in
[`Docs/analog-modeling-research.md`](Docs/analog-modeling-research.md).

## Polyphony, Mono, and slot allocation

Mars owns 16 fixed DSP render slots, and `Polyphony` sets the active physical
budget `L` from 1 to 16. Lowering `L` retires complete groups immediately, so
the audio thread cannot continue above the requested CPU budget. Allocation
depends on `Voice mode`:

| Mode | Slots per note group | Maximum simultaneous groups |
| --- | ---: | ---: |
| `Poly` | 1 | `L` |
| `Unison` | `min(Unison voices, L)`, normally 2–8 | `floor(L / layers)` |
| `Fifth` | `min(2, L)`: root plus fifth when available | `floor(L / layers)` |
| `Mono` override | 1 continuous voice | 1 |

When a new polyphonic group needs room, Mars searches complete released groups
first, then active groups, selecting the lowest current envelope/output energy
from an 8 ms output-power average and using the oldest generation as a
deterministic tie-breaker. A 15 ms attack guard prevents a newly played
articulation from disappearing before it speaks.
Every layer in the selected group is removed together. Retriggers and steals
preserve a fixed 2 ms fading tail to avoid a hard sample discontinuity.
`Unison voices` and `Unison detune` are active only in
`Unison`; `Mono` overrides all three polyphonic allocation modes without
changing their stored/automated value. When the arpeggiator is running it feeds
this same allocator one step at a time, so a step in `Unison` still consumes a
full layer group and a step in `Mono` still uses the single continuous voice.

## Exact 55-parameter contract

| Section | Front-panel controls (parameter IDs) |
| --- | --- |
| Oscillator I | Model: Moog-like VCO / Juno-like DCO (`osc1Model`), mixer feed On/Off (`osc1Enabled`), waveform (`osc1Wave`), octave (`osc1Octave`) |
| Oscillator II | Model: Moog-like VCO / Juno-like DCO (`osc2Model`), mixer feed On/Off (`osc2Enabled`), waveform (`osc2Wave`), octave (`osc2Octave`), tune (`osc2Tune`), fine tune (`osc2Fine`) |
| Mixer | Oscillator balance (`oscMix`), pulse width (`pulseWidth`), sub level (`subLevel`), noise level (`noiseLevel`), cross modulation (`crossMod`) |
| Filter | Model: `Ladder` / `SEM` (`filterModel`), cutoff (`cutoff`), resonance (`resonance`), drive (`filterDrive`), SEM shape (`filterShape`), envelope amount (`filterEnvAmount`), key tracking (`keyTrack`) |
| Filter envelope | Attack (`fAttack`), decay (`fDecay`), sustain (`fSustain`), release (`fRelease`) |
| Amplifier envelope | Attack (`aAttack`), decay (`aDecay`), sustain (`aSustain`), release (`aRelease`) |
| LFO | Waveform: triangle / sine / sample & hold (`lfoWave`), rate (`lfoRate`), pitch depth (`lfoPitch`), filter depth (`lfoFilter`), PWM depth (`lfoPwm`) |
| Voice | Mode: `Poly` / `Unison` / `Fifth` (`voiceMode`), Mono override (`monoMode`), physical render-voice ceiling 1–16 (`polyphonyLimit`), unison voices (`unisonVoices`), unison detune (`unisonDetune`), voice-card drift (`drift`), stereo spread (`spread`), glide time (`glide`), velocity response (`velocity`) |
| Arpeggiator | On/Off (`arpEnabled`), mode: Up / Down / Up-down / Random / As played (`arpMode`), rate in steps per second (`arpRate`), octave range 1–4 (`arpOctaves`), gate (`arpGate`), hold/latch (`arpHold`) |
| Output | Ensemble mix (`chorusMix`), ensemble rate (`chorusRate`), non-Juno studio compander (`chorusCompander`), output level (`output`) |
| Quality | HQ oversampling (`hqOversampling`): persisted, non-automatable, default On |

These are the complete host parameter IDs for version 1.6: 54 automatable sound
and performance controls plus one persisted quality setting. The original 40
IDs retain their order and version hint; `osc1Enabled` and `osc2Enabled` remain
the version-2 additions, and non-automatable `hqOversampling` retains version
hint 3. `osc1Model`, `osc2Model`, `polyphonyLimit`, and `monoMode` are appended
with version hint 4. `chorusCompander` is appended with version hint 5 and
defaults Off. `unisonDetune`, `arpEnabled`, `arpMode`, `arpRate`, `arpOctaves`,
`arpGate`, and `arpHold` are appended in that order with version hint 6, so
every previously shipped host parameter keeps its index and
normalized meaning. Legacy states default both models to Moog-like VCO,
polyphony to 16, Mono and the non-Juno compander to Off, both mixer switches to
On, HQ to On, unison detune to 9.6 cents (the exact version-1.5 spread at the
default drift), and the arpeggiator to Off / Up / 5 steps per second / one
octave / 55% gate / Hold off. The three
randomizer buttons are commands rather than host parameters; there are no
hidden sound controls behind a matrix or alternate panel.

New parameter ranges and defaults:

| Parameter ID | Range | Default |
| --- | --- | --- |
| `unisonDetune` | 0-50 cents, 0.1 ct step | 9.6 ct |
| `arpEnabled` | Off / On | Off |
| `arpMode` | Up / Down / Up-down / Random / As played | Up |
| `arpRate` | 0.5-20 steps per second, skewed to 4 | 5.00 Hz |
| `arpOctaves` | 1-4 | 1 |
| `arpGate` | 5-100% | 55% |
| `arpHold` | Off / On | Off |

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
VCO/DCO outputs, source-matched VCO saw contour, DCO clock/divider behavior,
held 8253 divisors across the 1/2/4 MHz ranges,
phase-continuous oscillator-model changes, oscillator and filter responses,
the 137-tap return filter's coefficient symmetry, ripple, >100 dB stopband,
exact 34/51-sample latency, the clocked BBD's N/(2*fClock) delay, fractional
multi-crossing capture and +2.3 dB gain, moving-PWM correction, measured
clock-dependent charge retention, deterministic gated feedthrough/noise,
paired compander laws,
high-note oscillator alias suppression, the Ladder's full cutoff range,
bass-gain compensation, and implicit-equation residual/state error against an
independent double-precision reference. They also cover an adversarial ladder
control jump, mixer isolation and clickless switching, cross modulation with
oscillator II's audio feed disabled, deferred HQ changes including large host
blocks, meaningful glide and modulation, dynamic physical-voice limits, Mono
mode-transition, duplicate-hold, legato/fallback/retrigger semantics,
voice-mode allocation, and a CPU guardrail.
The suite also pins level-independent ladder passband gain across a 60 dB
input range at seven cutoffs spanning the whole 20 Hz - 20 kHz control range,
proportional voice output for a quiet mixer feed, a
ladder limit cycle that starts itself from a millivolt kick at 200 Hz, 1 kHz
and 4 kHz cutoffs, mixer-noise level invariance
across 44.1-192 kHz with HQ on and off, a bright patch's top-octave spectrum
across 44.1-192 kHz, and HQ's audible-band inharmonic-folding advantage over the
native path.
Version 1.6 adds measurements of ladder self-oscillation frequency and
boundedness at maximum resonance against the unchanged sub-threshold behaviour,
the SEM's level-dependent resonance compression and its stability at every
shape, transient-free waveform switching and settled-state equivalence for the
frozen oscillator generators, unison-detune spread and its decoupling from
drift, the arpeggiator's pattern order in every mode with range, gate, hold, and
mode-exit semantics plus determinism across all four voice modes, the scope
reduction, meter ballistics, trigger search and decibel mapping, and a default
eight-voice CPU measurement. Plug-in builds additionally test
the 55-parameter order/default/text contract, legacy migration including the
version-6 additions, all randomizer
strengths and safety exclusions, MIDI, arpeggiated MIDI playback, the scope
trace, state, and editor rendering.

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
