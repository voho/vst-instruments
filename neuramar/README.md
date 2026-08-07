# Neuramar

Neuramar is a neural instrument for macOS: drop in one sound, let it infer the
root and fit a compact local synthesis model, then play the result across the
keyboard. Or start without a recording and grow a playable neural seed from
the **Randomize** controls. Its **Core**, **Air**, and **Bone** layers preserve
or invent different parts of a sound's identity while the front panel invites
the memory to stay faithful, drift, breathe, or become something new.

> **Listen first.** Eight [rendered demos](Docs/audio/README.md) cover the
> whole learning loop: a procedurally synthesized training tone, the fitted
> model replaying it, pitch and velocity generalisation, the Core/Air/Bone
> anatomy, formant and stretch control, and a polyphonic pad. Even the
> training input is code — no recording is involved anywhere.

Neuramar is not a conventional sampler. The imported recording is analysed in
the background but is not repitched or played back when notes sound. Completed
learning produces a small neural controller plus explicit harmonic, stochastic,
and modal synthesizers. Rendering is local and does not require a cloud service,
GPU, pretrained model, or source file.

![Neuramar standalone interface](Docs/screenshots/neuramar-standalone.png)

The screenshot is rendered by the real JUCE editor in the plug-in regression
suite. It shows the 1.0 layout and predates the 1.1 model-anatomy display and
the Stretch, Formant, and Touch controls; it will be regenerated on the next
macOS release build. VST3, Audio Unit, and Standalone builds use the same
resizable interface.

The project builds three products from one JUCE codebase:

- VST3 instrument for hosts such as Ableton Live, REAPER, Cubase, and Bitwig
- Audio Unit v2 music device for Logic Pro and GarageBand
- Standalone application for direct MIDI and drag-and-drop exploration

> **Just want to try it?** The scheduled Nightly workflow publishes the latest
> successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> Those bundles are ad-hoc signed and not notarized; check the repository's
> Nightly badge for the latest workflow result.

## Teach a sound, then play its memory

1. Drag a `.wav`, `.wave`, `.aif`, `.aiff`, `.flac`, or `.ogg` file onto the
   neural pool, or click **Drop / Open** to choose one.
2. Neuramar conditions the audio, finds a stable root, measures its harmonic,
   non-harmonic, and resonant behaviour, and fits the neural controller locally. The
   display follows the reading, root-finding, analysis, and training stages;
   **Cancel** stops an in-progress pass.
3. Check the inferred note, cents offset, and confidence. Use the `-` and `+`
   buttons to correct an octave or pitch-class mistake by up to 12 semitones in
   either direction.
4. Play MIDI or the on-screen keyboard. A completed model is published as one
   immutable snapshot at an audio-block boundary; training and file access do
   not run in the real-time render path.

Learning is an offline operation within the plug-in, not a network request.
Loading a new sound starts a new fit; the playable model changes only when that
fit completes successfully. The file loader accepts at least 60 ms and reads at
most the first 12 seconds; stereo input is downmixed for the monophonic analysis.

To begin without a sample, choose **1%**, **10%**, or **100%**, then press
**Randomize**. With an empty pool, Neuramar creates a playable C4-anchored neural
seed. With a learned or generated memory already loaded, it derives a fresh
immutable variation instead. The three buttons scale the same bounded musical
mutation ranges: 1% is a near-neighbour, 10% is an audible evolution, and 100%
can establish a new identity. Randomization never runs in the audio callback,
and the resulting compact model is saved with the session like a learned one.
At 1% and 10%, each press moves the current published memory toward a new
bounded target, creating a controlled evolutionary walk. At 100%, the mutable
neural field is fully rerolled while its Core/Air/Bone energy remains calibrated,
so repeated Wild presses do not accumulate runaway loudness.

## Choosing a useful source

Neuramar learns most predictably from one clean, isolated, mostly monophonic
note. A source that includes a clear onset, a stable pitched body, and some of
its natural evolution gives the model more useful evidence than a tiny clipped
fragment.

For the clearest root and most controllable instrument:

- use a single note rather than a chord, full mix, or layered loop;
- keep the wanted sound louder than room noise, accompaniment, and reverb;
- avoid hard clipping and excessive limiting;
- include the characteristic attack and enough sustain or decay to reveal how
  the spectrum changes;
- start with acoustic notes, voices without words, struck or bowed objects,
  drones, or designed one-shots, then experiment with stranger material.

Percussion, chords, and unstable or strongly inharmonic sounds are accepted and
can create interesting results, but their inferred root is necessarily less
reliable. The correction buttons are part of the instrument for exactly that
reason. Only analyse recordings you are allowed to use.

## Interface and controls

The neural pool combines waveform feedback, learning stage, progress, source
name, model generation, active voices, and sample rate. Below it, the **model
anatomy** display draws what the memory actually contains rather than a
decoration: the Core partial spectrum, the fitted Air bands, and the active Bone
modes on a shared 20 Hz - 20 kHz logarithmic axis, sweeping through the learned
trajectory while faintly outlining the spectrum's full reach across that whole
trajectory. Clicking it swaps to an evolution view of Core, Air, and Bone
loudness over the learned duration; hovering reads out the frequency under the
cursor, and the footer reports how far the fitted stiff-string coefficient
stretches the sixteenth partial at the current Stretch setting. The whole
reduction is computed once per published memory in the JUCE-free DSP layer, so
the editor never touches the model on a paint call.

The neighbouring root display shows the inferred note, tuning offset,
confidence, and any manual semitone correction. The editor is resizable and all
controls are drawn with native JUCE graphics.

Neuramar exposes 18 host parameters: sixteen continuous front-panel controls,
**Orbit**, and the persisted root correction.

| Control | Musical role |
| --- | --- |
| **Imprint** | 0–100%; moves from a smooth, dream-like harmonic contour toward the learned, more faithful Core. |
| **Body Lock** | 0–100%; moves from resonances that follow the played pitch toward source-like fixed resonances. |
| **Air** | 0–100%; sets the renderer-matched, harmonic-subtracted noise-filterbank layer: breath, scrape, hiss, and noisy attack energy. |
| **Bone** | 0–100%; sets persistence-selected inharmonic modes: body resonances, impact, and ringing. |
| **Stretch** | 0–200%; scales the partial stretch fitted from the source. 100% renders the learned stiff-string series, 0% forces an ideal harmonic bank, 200% exaggerates it. A memory with no fitted stretch is unaffected at any setting. |
| **Formant** | -24 to +24 semitones; moves the learned resonant body — the Body-Locked Core envelope, the Air bands, and the Bone modes — without moving the played pitch. |
| **Touch** | 0–100%; sets how much MIDI velocity shapes timbre as well as level. Harder notes become brighter and breathier; at 0% velocity only sets level. |
| **Register** | -100% to +100%; key tracking. Positive values darken notes played above the learned root and open up notes played below it, the way an acoustic instrument's spectrum changes across its compass; negative values invert that. It changes tone, not level. |
| **Gravity** | -100% to +100%; tilts the reconstructed spectrum from darker to brighter. |
| **Memory** | 0.25x–4.00x; changes how quickly a note travels through the learned time evolution. |
| **Mutation** | 0–100%; adds bounded, voice-local movement around the learned character. |
| **Noise** | 0–100%; sends smooth voice-local randomness through the neural time/Fourier input coordinate, making the model wander through its own learned or generated behaviour. It does not add an audio-noise layer. |
| **Awaken** | 0–2 s; at zero, preserves the learned attack. Above zero it adds a performance fade-in that takes exactly the stated time, end to end, and leaves the note at full level from then on. |
| **Dissolve** | 20 ms–8 s; shapes the performance release. |
| **Horizon** | 0–100%; widens voices across the stereo field and gives the Air layer a decorrelated second realization, which cancels exactly in a mono sum. |
| **Output** | -24 to +6 dB; sets the final level. |
| **Orbit** | Revisits a stable learned region while a note remains held. |
| **Root correction** | Shifts the inferred source root from -12 to +12 semitones without retraining. |

**Randomize** creates a model when the pool is empty or varies the current
model's neural state. Its **1% / 10% / 100%** selector controls the breadth of
that operation. **Panic** immediately silences sounding voices. **Drop / Open**,
**Randomize**, **Cancel**, and **Panic** are actions rather than host parameters.

Root correction is sampler-style key relabelling: the corrected MIDI key
reproduces the learned source pitch. It does not retune the source itself or act
as a master-tuning control.

## Neural synthesis engine

The JUCE-free C++20 core uses a DDSP-inspired, factorized
neural-controller architecture chosen for the severe one-example data limit
and the real-time constraints of a polyphonic plug-in. It is not trained
end-to-end through a differentiable renderer:

- a stiff-string partial law `f(n) = n f0 sqrt(1 + B n^2)` is fitted to the
  measured partial positions before anything else is analysed. Squaring it
  linearises the law, so one amplitude-weighted straight-line fit recovers the
  fundamental and the coefficient together instead of biasing both; the fit is
  only accepted when the partial series is dense and uninterrupted and the `n^2`
  trend explains most of the measured deviation, so an ordinary harmonic source
  reports exactly zero and behaves exactly as before. The harmonic analysis then
  projects at the stretched positions, Bone candidate rejection measures its
  distance to the nearest stretched partial, and the renderer places every
  oscillator on that series;
- a multi-window YIN-style difference-function detector rejects weak
  periodicity estimates, prefers deep periodic minima before its looser noisy
  fallback, aligns octave-related candidates in log frequency, and changes
  octave only when compressed, distributed harmonic support materially beats
  the coherent YIN result;
- a constrained local autocorrelation track can retain sufficiently stable
  bends, slow vibrato, and glide around that root, and drives pitch-tracked
  harmonic-coordinate analysis;
- the harmonic solve is joint rather than sequential. A single ordered
  projection pass is the least-squares answer only when the partial bases are
  orthogonal, and at an aperture of a few fundamental periods adjacent Hann main
  lobes touch, so each subtraction leaks into its neighbours and a quiet partial
  beside a loud one absorbs the leakage. Each partial's current estimate is
  added back before it is re-solved against the others' residual, which
  converges to the joint solution without forming the full system; past about
  eight periods per aperture the bases are orthogonal to working precision, so
  the long modal analysis keeps a single sweep;
- 128 spectral frames at a fixed 48 kHz analysis rate reserve 48 strictly
  ordered physical-time observations for the first 120 ms, then cover sustain
  and decay; pitch-adaptive 512–4096-sample Hann apertures target four root
  periods in the body of the sound and two over the first 40 ms, so the attack
  is no longer measured through an aperture five times longer than the frame
  spacing that exists to resolve it; a parallel 4096-sample
  residual supplies the steadier evidence used for twelve persistence-scored
  Bone candidates;
- residual power, excluding both active Bone neighbourhoods and the narrowband
  residue left at each subtracted partial, is accumulated into 48
  log-frequency cells weighted by how many spectrum bins each one actually
  observed; a deterministic non-negative solver then fits sixteen
  overlapping log-spaced Air bands against the analytic power response of the
  same normalized biquads used by the renderer. Excluding a bin from the
  measured power also excludes it from the modelled response, so the fit
  measures the noise between the partials rather than the imperfection of
  their removal, and band amplitudes are floored 60 dB below the loudest band
  the sound produces so a momentarily rejected band cannot gate the layer;
- unmeasured pitch-contour frames are bridged rather than snapped back to the
  root, which the correction trajectory would otherwise reproduce exactly as an
  audible warble;
- the Orbit loop is searched in normalized time rather than in analysis-frame
  indices. The grid is deliberately warped so that 48 of its 128 frames
  describe the first 120 ms, so an index-based search placed both loop points
  inside the attack and a decaying source was re-attacked on every orbit;
- deterministic Adam optimization fits a 32-unit, time-conditioned neural
  controller with polynomial, Fourier, and onset-aware inputs to the low-rate
  log-amplitude targets; a bounded 128-frame `int16` correction trajectory then
  restores fast and irregular detail omitted by the smooth neural base without
  adding allocation or locks to evaluation;
- the same controller can be initialized as a musically constrained procedural
  neural field when no sample is present; subsequent randomization uses
  deterministic per-operation seeds and independent named random streams,
  scales bounded coefficient, phase, spectrum, modal, and partial-stretch
  changes by exactly 1%, 10%, or 100%, and progressively releases learned residual corrections so
  larger variations can express their new network state;
- **Noise** supplies a slow, smoothly interpolated, voice-local latent signal
  to the controller's existing input manifold; it moves evaluation through
  nearby model states at control rate, with displacement capped in seconds so
  long recordings do not turn into coarse temporal scrubbing, while **Air**
  remains the explicit stochastic audio layer and **Mutation** remains the
  per-voice character variation control;
- **Core** renders the learned harmonic distribution at each played MIDI pitch,
  **Air** drives an independent deterministic white-noise stream through each
  fitted band, and **Bone** renders only candidate modes that remain locally
  prominent across the sound, all with learned amplitude trajectories;
- the compact 64-partial neural Core can drive an adaptive 256-oscillator
  runtime bank; Body Lock separates a smooth fixed-frequency envelope from its
  complementary harmonic-index excitation residual, so odd/even and reed-like
  character follows the played harmonic while resonances remain source-like;
  lower notes use only observed envelope evidence on their denser grid, while
  version-2 learned states retain exact decoder/model-evaluation compatibility;
  the source/filter envelope uses a parity-balanced power kernel whose shoulder
  taps carry only 1/32 of the weight each, so an alternating odd/even excitation
  still cancels exactly while a narrow formant is blurred over a much smaller
  span, and the kernel reflects at both evidence boundaries instead of
  renormalising a truncated, parity-biased weight sum;
- fractional Body-Locked coordinates use a positive, local, shape-preserving
  cubic in a log-like magnitude domain; learned harmonics remain exact and
  sharp spectral turns cannot overshoot. Below the first observed partial the
  envelope holds its lowest observed value, which makes Body Lock degenerate
  cleanly to pitch-following there rather than notching the bottom partials of
  a deeply transposed note; above the last observed partial it still fades to
  silence, because inventing energy there is what Body Lock exists to prevent;
- circular onset-phase mapping, a smooth capacity edge, and register power
  normalization keep wide transpositions coherent and hold the level steady
  across the keyboard; the full-Imprint learned path does not invent
  out-of-range energy, and lower Imprint fades virtual Body-Locked harmonics
  outside the learned envelope toward silence. The register reference covers
  only as many partials as the played note actually renders: measuring against
  the model's full bank asks a high note to make up energy that cannot exist
  past the anti-alias limit, a demand that grows without bound. Core, Air, and
  Bone all take the same compensation, so the balance between the three layers
  is the source's at every point on the keyboard;
- **Register** applies a key-tracked spectral tilt that is deliberately left out
  of that reference, so it changes tone without changing level;
- each Air filter is normalized to unit expected RMS for its noise input, and a
  smooth gain taper prevents transposed or brightened bands from accumulating at
  the band edge. That taper, and the matching one on the Bone modes, is anchored
  to the top of the audible band as well as to the host Nyquist, so a memory
  keeps the same audible top octave at 44.1 kHz as at 96 kHz instead of losing
  it at one rate and rendering it purely ultrasonically at the other;
- stereo placement is ramped like every other control-rate target, so adding or
  removing a chord note glides the remaining voices into their new positions
  instead of stepping them; Output carries its own smoother because it is the
  one control applied straight to the summed signal; and the voice-steal tail is
  windowed with a raised cosine rather than a linear ramp, which removes the
  corner that a frozen last sample would otherwise leave at both ends. A slot
  stolen twice inside one fade carries the value the first tail was about to
  emit into the second instead of discarding it, so the hand-off steps by
  exactly the stolen voice's own last sample wherever in the fade it lands —
  including the closing third, where scaling that sample by the window rather
  than carrying it left a step behind. The window is re-cut across the samples
  the first steal already budgeted rather than re-armed: all the energy stacked
  into one slot still dies within one fade of the first steal, and the carried
  level is bounded, so even an unmusically dense note-on burst cannot walk the
  tail into the output guard;
- **Awaken** advances a position rather than driving a one-pole, so its stated
  seconds are the whole fade and not an exponential time constant, and the fade
  has zero slope at both ends rather than its steepest slope at the note-on;
- the final output uses the host's floating-point headroom and remains linear
  at ordinary operating levels; only a pathological ±7.95 guard is
  retained, avoiding the folded high-register harmonics produced by an
  always-on base-rate saturator;
- **Touch** turns velocity into an excitation strength: above a mezzo-forte
  reference the harmonic tilt leans brighter and more of the learned Air is let
  through, below it the opposite. At zero the response is the pure amplitude
  behaviour every earlier build had;
- **Formant** divides the Body-Locked envelope lookup coordinate by the shift
  and multiplies the Air and Bone centre frequencies by it, so the resonant body
  moves in absolute frequency while the played pitch does not move at all;
- the band-limiting taper is folded into the control-rate amplitude ramp, so the
  audio loop performs no per-sample anti-alias arithmetic and skips every
  partial the taper has silenced. The oscillator evaluates a quarter-period
  folded polynomial rather than reading an interpolated table: it is both more
  accurate than the 4096-entry table it replaces (peak error -133.6 dB against
  -125.6 dB, RMS -148.5 dB against -134.6 dB, over 2e7 uniformly spaced phases
  against double-precision `sin()`) and free of the memory gather that stopped
  the partial loop vectorising. Stored phases are kept reduced to `[0, 1)` and
  the phase increment is non-negative, so the wrap is a truncation rather than a
  rounding instruction;
- the Air and Bone layers are skipped outright for any voice where they are
  silent for a whole control period - not just their filtering, oscillators and
  noise draws, but their band design and their amplitude ramps as well, each
  layer independently of the other - and the learned onset phases' unit vectors
  are cached per published memory rather than recomputed on every note-on,
  because note-on runs on the audio thread as well;
- each voice owns its envelope, phase, note age, per-band noise, and variation
  state, while controller outputs are forward-interpolated between low-rate
  evaluations so a learned target is reached at the time it describes.

The current engine has a fixed ceiling of sixteen synthesis voices, raised from
eight so that a chord held under a sustain pedal, or a pad played over its own
release tails, stops stealing. The per-voice cost did not change, so an
eight-note performance renders for exactly what it did before; the ceiling is an
option rather than a cost every performance pays. It evaluates
the controller, spectral-envelope mapping, and register normalization at control
rate and interpolates their parameters while the oscillators, filters,
envelopes, and note handling remain in the audio path.

The representation is deliberately structured rather than a raw-waveform
generator. Explicit pitch and oscillator priors make one-note extrapolation
possible and keep inference bounded. The rationale, primary papers, claims
boundary, and future quality path are documented in
[`Docs/neural-synthesis-research.md`](Docs/neural-synthesis-research.md).
The preregistered competitor set, held-out corpus, objective targets, and blind
listening acceptance rule are specified separately in
[`Docs/resynthesis-quality-benchmark.md`](Docs/resynthesis-quality-benchmark.md).

## Scientific scope and limitations

A single recording cannot reveal notes, velocities, articulations, or registers
that were never captured. Neuramar learns the evidence in that recording and
uses musical synthesis priors to extrapolate it; it does not recover an unknown
original instrument in full. Large transpositions can expose that limitation,
and noisy or ambiguous sources can lead to a wrong root or a less literal model.
Likewise, sample-free Randomize is a procedural generator inside Neuramar's
bounded neural/DDSP architecture, not a pretrained generative model of real
instruments. “100%” means the full designed musical mutation range, not arbitrary
unsafe values across the serialized decoder's validation limits.

The current release uses no pretrained corpus or bundled weights. That keeps the
fit private and self-contained, but it also means Neuramar has no external
knowledge of what the source "should" be. The design prioritizes a bounded local
fit, deterministic recall, and a real-time-safe render path. Listening tests,
host validation, and profiling remain necessary; this README makes no measured
training-time, similarity, or perceptual-quality claim.

The stiff-string law is a one-parameter musical prior, not a physical model of
the source. It captures the systematic sharpening of upper partials on wound
strings, tines, and struck bars; it cannot describe an arbitrary inharmonic
body, which is what the explicit Bone modes are for. The fit is deliberately
conservative and reports an exactly harmonic series whenever the evidence is
short, sparse, or does not follow the law.

The Core/Air split is a local sinusoidal decomposition, not a perfect physical
source separation. Window boundaries, rapidly moving partials, and inharmonic
tonal energy can leak into the residual. Bone tracks are compact,
persistence-scored sinusoidal candidates rather than identified physical
eigenmodes. Air is an expected-power match through a compact sixteen-filter
bank,
not phase-coherent reconstruction of the original residual; its stochastic
waveform therefore preserves an estimated spectral evolution rather than the
recording's exact noise realization. Bands that approach the Nyquist limit at a
low host sample rate are deliberately faded instead of folded onto the edge.

**Touch** is a musical control, not a model of the instrument's dynamics. One
recording contains no evidence about how the source behaves at any velocity
other than the one that was played, so Touch applies a fixed excitation-strength
prior — brighter and breathier above a mezzo-forte reference, the opposite below
it — rather than a fitted velocity response. Reviewers of this class of
instrument name dynamic response as a specific weakness of one-shot resynthesis,
and that criticism applies here; the honest position is that the evidence for it
does not exist in a single note.

Automatic root search is bounded to 35–2000 Hz. Its correct-semitone rate is
measured on a twelve-item analytic corpus in
[`Docs/resynthesis-quality-benchmark.md`](Docs/resynthesis-quality-benchmark.md)
and is not a claim about real recordings. The displayed confidence is an
average YIN periodicity score, not a calibrated probability and not a direct
measure of timbre-match quality. The local pitch contour is constrained to four
semitones around the inferred root, so sufficiently stable bends, slow vibrato,
and glide can be learned, while broad sweeps, octave jumps, fast modulation, and
unvoiced regions are deliberately bounded.

## State, privacy, and presets

Host state stores the ordinary parameters, root analysis metadata, display-only
source filename, a low-resolution waveform preview, and the compact learned
model in a versioned payload. The current payload is version 5, which widens the
body representation from 8 Air bands and 6 Bone modes to 16 and 12; version 4
appended the fitted stiff-string coefficient. Version-2, version-3, and
version-4 memories saved by earlier releases still load and render exactly as
they did: their bands and modes occupy the low slots, the added slots decode as
silence, and the missing fields stay at their neutral zero values. Sessions saved before Noise, Stretch,
Formant, or Touch existed restore those controls at their defaults rather than
inheriting whatever the running instance happened to hold. One stored value now
means something different: **Awaken** used to be an exponential time constant,
so a saved non-zero setting took roughly 4.6 times its stated seconds to open
fully. It is now the fade duration itself, and a recalled session will therefore
open about that much sooner; multiply an old setting by roughly 4.6 to
reproduce the previous feel. No parameter was added, removed, or renamed, so
every other stored value recalls exactly as before. It does not store the source recording or its full
path, so a saved session can recall the synthesized instrument after the
original file moves or disappears. Imported audio stays local during learning,
and performance performs no file or network access.

A host project or exported preset can therefore contain source-derived model
coefficients even though it contains no sample playback data. Treat that state
as derived user content and confirm that you have the necessary rights before
sharing it. No factory samples, pretrained weights, or third-party preset
library are included; see [`Presets/README.md`](Presets/README.md). The
demonstration audio in `Docs/audio/` is generated entirely by this
repository's own code, training input included.

## Requirements and formats

- macOS 11 or newer for the released VST3, Audio Unit, and Standalone products
- A current full Xcode installation selected for command-line use
- CMake 3.22 or newer and a C++20 compiler
- Internet access for the default first configure, or a local JUCE 8.0.14
  checkout supplied through `JUCE_PATH` (`NEURAMAR_JUCE_PATH` when configuring
  CMake directly)

JUCE 8.0.14 is pinned and fetched at configure time; it is not vendored into
this repository. The CMake project also selects VST3 and Standalone targets on
non-Apple systems, while the maintained packaging and release workflow is for
macOS.

## Build and test on macOS

The helper creates an Xcode build, compiles universal `arm64`/`x86_64` binaries,
ad-hoc signs the development bundles, and runs both engine and plug-in contract
tests:

```bash
./scripts/build-macos.sh
```

Use `BUILD_UNIVERSAL=OFF` for a native-architecture development build, or point
the configure at an existing checkout of the exact JUCE release:

```bash
BUILD_UNIVERSAL=OFF ./scripts/build-macos.sh
JUCE_PATH="/path/to/JUCE-8.0.14" ./scripts/build-macos.sh
```

Equivalent universal configure, build, and test commands are:

```bash
cmake -S . -B build-macos -G Xcode \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DNEURAMAR_BUILD_UNIVERSAL=ON \
  -DNEURAMAR_BUILD_PLUGIN=ON \
  -DBUILD_TESTING=ON
cmake --build build-macos --config Release --parallel
ctest --test-dir build-macos -C Release --output-on-failure
```

Release bundles are written to:

| Format | Build artifact |
| --- | --- |
| VST3 | `build-macos/Neuramar_artefacts/Release/VST3/Neuramar.vst3` |
| Audio Unit | `build-macos/Neuramar_artefacts/Release/AU/Neuramar.component` |
| Standalone | `build-macos/Neuramar_artefacts/Release/Standalone/Neuramar.app` |

## Run the neural-engine tests without JUCE

The synthesis and learning core is independent of JUCE, so it has a portable
test path on any C++20 development machine:

```bash
cmake -S . -B build-dsp \
  -DCMAKE_BUILD_TYPE=Release \
  -DNEURAMAR_BUILD_PLUGIN=OFF \
  -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

The JUCE-free suite covers analysis, deterministic learning and rendering,
finite output, pitch response, and model serialization. It also pins the
render-path measurements reported in
[`Docs/resynthesis-quality-benchmark.md`](Docs/resynthesis-quality-benchmark.md):
the worst spectral line below the played fundamental (-108 dB relative to the
rendered signal, guard -85 dB), partial-level agreement across 44.1, 48, 88.2,
96, and 192 kHz for the root and for root+12 and root+24 (0.08 dB, guard
0.35 dB), Air and Bone layer power across those rates (0.13 dB, guard 1.0 dB),
the removal of a Bone mode pushed above the audible ceiling (128 dB down, guard
60 dB), the Awaken fade contract, the bound on the voice-steal fade tail under a
note-on burst dense enough to steal the same slot inside one fade window, and
the continuity of that hand-off across every phase of the window (worst
inter-sample step 0.0124, guard 0.016).

It also pins the reconstruction measurements added in 1.3, which compare a
rendered root note against the fixture that produced it: multi-resolution
spectral convergence and log-magnitude MAE at three window/hop pairs, ERB-band
residual power MAE with the harmonic bins excluded, cumulative-energy error at
1, 5, 10, 20, and 50 ms, and T10-T90 attack-time error, on both a source/filter
fixture and one combining harmonics, shaped noise, and a broadband transient. A
struck-body fixture with ten known inharmonic modes reports modal recall,
precision, and frequency error, and a twelve-item analytic corpus reports the
automatic root detector's correct-semitone and octave-error rates. Plug-in
builds add
processor, parameter, MIDI, editor, and host-state contracts. These regression
checks complement rather than replace listening, validator, multi-host, and CPU
profiling passes.

## Regenerate the demonstration audio

```bash
cmake --build build-dsp --parallel --target NeuramarRenderDemos
./build-dsp/NeuramarRenderDemos Docs/audio
```

The renderer synthesizes its own training tone, fits the model from it exactly
as the plug-in fits a dropped file, and renders the demos from that fit. The
whole chain is deterministic, and the tool rewrites the level table in
[`Docs/audio/README.md`](Docs/audio/README.md) in place, so the committed audio
and its documented levels stay in lockstep with the code. See that file for the
full manifest.

## Install and validate locally

Copy only the formats you need:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
mkdir -p ~/Library/Audio/Plug-Ins/Components
ditto build-macos/Neuramar_artefacts/Release/VST3/Neuramar.vst3 \
  ~/Library/Audio/Plug-Ins/VST3/Neuramar.vst3
ditto build-macos/Neuramar_artefacts/Release/AU/Neuramar.component \
  ~/Library/Audio/Plug-Ins/Components/Neuramar.component
```

Quit and reopen the host after installing. The Audio Unit identifiers are type
`aumu`, subtype `Nrm1`, and manufacturer `Nram`:

```bash
auval -v aumu Nrm1 Nram
```

With [pluginval](https://github.com/Tracktion/pluginval) installed, validate
the VST3 and then test sample loading, cancellation, root correction, automation,
state recall, note overlap, sample-rate changes, and common buffer sizes in at
least two hosts:

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 10 \
  ~/Library/Audio/Plug-Ins/VST3/Neuramar.vst3
```

## Sign, package, and notarize

After a successful default universal build, create ZIP and PKG artifacts under
`build-macos/dist/`. Their three embedded bundles are ad-hoc signed by default;
the installer container itself remains unsigned unless an Installer identity is
provided:

```bash
./scripts/sign-and-package-macos.sh
```

For public distribution, provide Developer ID identities and a keychain profile
created with `notarytool store-credentials`:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Company (YOURTEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Company (YOURTEAMID)" \
NOTARY_PROFILE="neuramar-notary" \
./scripts/sign-and-package-macos.sh
```

The helper checks bundle versions and architectures, embeds the licence notices,
signs every independently copyable bundle, creates the package, and—when a
notary profile is supplied—submits and staples the installer. Replace the
placeholder bundle identifier and four-character plug-in codes with identifiers
controlled by the publisher before a public release; do not change them after
shipping because hosts use them for recall.

## Project layout

```text
Source/DSP/              JUCE-free analysis, model, and polyphonic engine
Source/PluginProcessor.* MIDI, parameters, learning worker, and host state
Source/PluginEditor.*    Drag-and-drop neural-pool interface and keyboard
Tools/RenderDemos.cpp    Renders the committed demonstration WAVs
Docs/                    Neural-synthesis research and claims boundary
Docs/audio/              Eight rendered demonstrations and their manifest
Tests/                   Engine and JUCE processor regression tests
Presets/                 Preset and learned-model provenance guidance
scripts/                 macOS build, signing, packaging, and notarization helpers
```

## Licensing

Neuramar's original source is offered under the MIT License. JUCE is a separate
dependency and is **not** covered by that licence. JUCE 8 framework modules are
available under the AGPLv3 or a commercial JUCE licence; confirm which terms
apply before distributing a binary. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
and the bundled JUCE licence notice for details.
