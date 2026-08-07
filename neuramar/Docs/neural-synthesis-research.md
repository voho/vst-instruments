# Neuramar neural-synthesis design

## Scope

Neuramar learns a compact, playable synthesis model from one user-supplied,
mostly monophonic sound. It does not retain or repitch the recording during
performance. Import is an offline analysis/training operation; rendering uses
an immutable model made from a small neural controller and explicit oscillators.

That distinction matters. A single note contains useful evidence about its
fundamental, harmonic distribution, residual noise, transients, and evolution,
but no evidence about articulations or registers that were never recorded.
Neuramar therefore performs a controlled extrapolation rather than claiming to
recover the original instrument.

## Why a neural DDSP hybrid

Raw-waveform neural generators are a poor fit for a self-contained polyphonic
plug-in that must adapt on a laptop CPU:

- RAVE reports real-time decoding, but its published model was trained for six
  days on roughly 30 hours of audio. It is an excellent corpus model, not a
  bounded one-sample fitting method. See
  [Caillon and Esling, 2021](https://arxiv.org/abs/2111.05011).
- Few-sample neural instrument cloning demonstrates adaptation from seconds of
  audio, but the published system uses a 12.5-million-parameter pretrained
  decoder and reports minutes of GPU adaptation. See
  [Jin et al., DAFx-23](https://www.dafx.de/paper-archive/details/zWyI3tFAzaieWGQuM45sEg).
- Neural codecs such as
  [SoundStream](https://research.google/pubs/soundstream-an-end-to-end-neural-audio-codec/)
  and [EnCodec](https://github.com/facebookresearch/encodec) reconstruct audio;
  by themselves they do not provide musically controlled pitch extrapolation.

Neuramar instead uses the factorisation established by
[DDSP](https://openreview.net/forum?id=B1x1ma4tDr) as a design prior: a learned
low-rate controller drives an explicit harmonic source and stochastic branch.
Unlike the DDSP paper's end-to-end differentiable training, Neuramar first
extracts spectral targets and then fits their trajectories; it does not
backpropagate through its renderer. This still makes pitch explicit, keeps
inference bounded, and supplies strong priors when only one example is available.

[Differentiable Wavetable Synthesis](https://arxiv.org/abs/2111.10003) shows why
a periodic source is attractive for one-shot sounds: learned dynamic
wavetables retain waveform character while being much cheaper than a large
partial bank. Neuramar's first implementation takes the closely related
harmonic-coordinate route. It predicts a dynamic harmonic spectrum and renders
phase-continuous oscillators. This preserves a simple antialiasing boundary and
allows a continuous `Body Lock` control between absolute-frequency resonances
and pitch-following harmonic identity.

## Stiff-string partial placement

An ideal harmonic bank is the wrong prior for a large family of useful sources.
Struck and plucked strings, electric-piano tines, and struck bars all sharpen
their upper partials because the vibrating body has bending stiffness, and the
classical description of that is
[Fletcher's stiff-string law](https://doi.org/10.1121/1.1908504),
`f(n) = n f0 sqrt(1 + B n^2)`. Forcing such a sound onto exact integer multiples
misplaces its partials by tens of cents in the region where the ear is most
sensitive to it, and it also corrupts the amplitude analysis, because the
sinusoidal projection then measures the wrong frequency.

Neuramar fits one coefficient `B` before any other analysis. Squaring the law
linearises it exactly:

```text
(f(n) / n)^2 = f0^2 + (f0^2 B) n^2
```

so a single amplitude-weighted straight-line fit over the measured partial
positions recovers the fundamental and the coefficient *together*. That joint
form matters: solving for `B` against an assumed fundamental biases both, because
a period-based detector already reads a stiff string slightly sharp and that
sharpness absorbs part of the stretch it is supposed to measure. Partial
positions come from three long-aperture probes in the steady part of the sound,
each refined below the bin spacing by the parabolic vertex of three
log-magnitude bins. The estimate is iterated over a widening partial set so an
early mis-prediction cannot lock a high partial onto its neighbour.

The fit is deliberately easy to reject. It is discarded unless the accepted
partials form an uninterrupted run from the fundamental, the `n^2` trend
explains most of the weighted deviation energy, and the coefficient exceeds an
audibility floor. A sparse spectrum with a few strong resonances - exactly the
material the Bone branch exists for - therefore reports an ideal harmonic series
rather than a spurious stretch, and an ordinary sustained note reports exactly
zero, leaving every earlier code path bit-identical. This is a one-parameter
musical prior, not a physical model: it cannot describe an arbitrary inharmonic
body, and the explicit modal branch remains the representation for those.

Once accepted, the coefficient propagates consistently. The harmonic analysis
projects at the stretched positions, Bone candidate rejection measures its
distance to the nearest stretched partial rather than to the nearest integer
ratio, and the renderer derives every oscillator frequency from a shared ratio
table. The `Stretch` control scales that coefficient at render time, so a user
can hear the fitted series, an ideal harmonic bank, or an exaggerated stretch
without re-analysing anything.

That control follows the source-filter motivation in
[Schwarz and Rodet's spectral-envelope work](https://quod.lib.umich.edu/i/icmc/bbp2372.1999.417?rgn=main;view=fulltext): preserving an envelope in absolute
frequency can retain source-like resonances while harmonic fine structure moves
with pitch. At each control frame, Neuramar separates the learned magnitudes
into a short power-smoothed envelope and the complementary harmonic-index
excitation residual. The smoothing kernel is chosen so that exactly half of its
weight sits on even offsets and half on the odd +/-1 taps: an alternating
odd/even excitation then cancels identically at every index, which is what puts
the parity contrast entirely in the excitation residual where it belongs. Within
that constraint the shoulder taps are kept as light as the cancellation allows
(1/32 of the weight each), because kernel width, not parity leakage, is what
limits held-out register accuracy - a formant only a fifth of an octave wide
spans fewer source harmonics than a wide kernel averages over. The kernel
reflects about the first and last observation instead of renormalising a
truncated weight sum, which would reintroduce a parity bias exactly at the
evidence boundaries. Their product remains exact at every observed root-note
partial. When Body Lock moves a note, odd/even, reed, and bow-like excitation
character therefore stays attached to harmonic index while only the smooth
envelope is read in absolute-frequency coordinates. The magnitude crossfade is
performed in a `log1p`-companded domain rather than linear amplitude.

This compact factorisation is a stable one-note synthesis prior, not a claim
that a unique physical filter has been recovered. A single note cannot reveal
the filter between every excited partial, and virtual partials beyond the 64th
use a neutral excitation residual rather than an invented repeating pattern.
At runtime Neuramar also circularly interpolates onset phases and samples only
coordinates inside the observed envelope. Magnitude resampling uses a local
`log1p`-companded monotone cubic: log-amplitude interpolation between sinusoidal
peaks has direct precedent in spectral-envelope work by
[Jensen and Hansen](https://crss.utdallas.edu/Publications/Jensen2001.pdf), while
the slope limiting follows the local shape-preserving construction of
[Fritsch and Butland](https://doi.org/10.1137/0905021). Every measured harmonic
is an exact knot, each fractional value remains between its two neighbours, and
a sharp learned resonance cannot make a ringing overshoot elsewhere. The
compander remains linear near true silence, avoiding the artificial floor of a
plain logarithm. This is a smooth interpolation prior; it cannot reconstruct
spectral evidence that the supplied note never contained.

Below the first coordinate and above the 64th, the previous amplitude-linear
shoulders remain intact. In particular, a log fade from zero would suppress the
fractional low-edge partials needed by notes below the source. The low edge
also retains a bounded fundamental anchor. A 256-slot adaptive oscillator bank
therefore restores the complete observed bandwidth as far as two octaves down,
without expanding the neural controller or serialized state. Pitch-follow mode
never fabricates those virtual partials, and strict Body Lock at full Imprint
fades coordinates above the observed envelope instead of reverting to unbounded
pitch-following energy. Inside the learned 64-partial range, lower Imprint
crossfades toward Dream's synthetic neutral harmonic slope; outside that range,
it fades the Body-Locked virtual extension toward silence.

The non-harmonic branch follows the motivation of
[NoiseBandNet](https://arxiv.org/abs/2307.08007): narrow, explicit noise bands
can represent breath, scrape, bow noise, and attacks more faithfully than one
global filtered-noise colour. Neuramar adopts only this filterbank motivation:
it is not an implementation of NoiseBandNet and does not use that project's
architecture, training procedure, source code, or weights. Its sixteen Air bands
have adjacent logarithmic edges from 80 Hz to 16 kHz at the fixed analysis rate,
0.51 octaves apiece. Eight bands of 1.03 octaves each put a breath formant, a
scrape peak, and a hiss shelf inside one number; NoiseBandNet's own answer to
the same problem is 2048 filters, and Alchemy's is a second synthesis engine.
A joint weighted least-squares sinusoid fit first removes the pitch-following
Core from each waveform frame. Core and Air targets use a
pitch-adaptive power-of-two aperture bounded to
512–4096 samples at the fixed 48 kHz analysis rate, avoiding one fixed 85 ms
window that would smear short attacks. It targets four root periods in the body
of the sound and two over the first 40 ms: the four-period rule keeps adjacent
partials' Hann main lobes apart, which is a constraint on a sequential
projection and not on the joint solve above, so the attack — where the analysis
grid already places its frames 2.5 ms apart — no longer measures through an
aperture five times longer than that spacing. The 4096-sample ceiling
necessarily contains fewer than four periods below 46.875 Hz. Bone selection and decay
retain a parallel 4096-sample residual for modal frequency resolution. Active
Bone neighbourhood exclusions scale with the real Hann aperture, not its
zero-padding, so modal main lobes do not reappear as Air. The remaining
single-sided residual power is corrected for the actual analysis window and
accumulated into 48 log-frequency cells. Deterministic
projected coordinate descent then fits non-negative band powers against the
analytic response of the exact normalized biquads used by the renderer, with a
small adjacent-band regularizer. This follows the deterministic-plus-stochastic
decomposition of
[Serra and Smith's spectral modelling synthesis](https://doi.org/10.2307/3680788),
but it remains a compact expected-power approximation rather than
phase-coherent source separation.

For inharmonic candidate peaks, the model reserves a small modal branch inspired
by [Differentiable Modal Synthesis](https://proceedings.neurips.cc/paper_files/paper/2024/file/0232cafe8d1909a01019abe8af32f3e1-Paper-Conference.pdf).
Candidate seeds come from several early Core-subtracted spectra, then earn a
reliability score from their time-weighted local prominence over the active
sound. Only persistent candidates receive learned amplitude trajectories and
estimated decay; unused slots remain explicitly inactive. This can help with
bells, strings, and body resonances that should not be forced onto exact
multiples of the fundamental. These are compact sinusoidal tracks, not claims
that the analyser has identified physical eigenmodes.

## Learning pipeline

1. **Validate and condition** — downmix, remove DC, trim leading/trailing
   silence, reject clips that are too short or effectively silent, and bound
   the analyzed duration.
2. **Find the root before fitting** — estimate candidate fundamentals across
   several window sizes and note regions with YIN's cumulative-mean normalized
   difference function, prefer a genuinely deep periodic minimum before using
   the looser noisy-signal threshold, align octave-related candidates in log
   frequency, and score half/current/double-root hypotheses using compressed,
   distributed harmonic support. An octave hypothesis must materially beat the
   coherent YIN estimate rather than win on one loud overtone. The mathematical
   basis is
   [de Cheveigne and Kawahara, 2002](https://pubmed.ncbi.nlm.nih.gov/12002874/).
3. **Measure the partial series** — fit the stiff-string coefficient and a
   jointly estimated fundamental as described above, or report an exactly
   harmonic series when the evidence does not support one.
4. **Track and factor the sound** — follow a constrained local pitch contour
   around the root; at each frame, use pitch-adaptive weighted cosine/sine least
   squares to estimate harmonic amplitude and phase and subtract the fitted
   sinusoids from the waveform. The solve is joint rather than sequential: a
   single ordered projection pass is the least-squares answer only when the
   partial bases are orthogonal, and at an aperture of a few fundamental periods
   adjacent Hann main lobes touch, so each subtraction leaks into its
   neighbours and a quiet partial beside a loud one absorbs the leakage. The
   pass is therefore repeated with each partial's current estimate added back
   before it is re-solved against the others' residual, which is Gauss-Seidel on
   the joint normal equations and converges to the joint solution without
   forming the full `2N x 2N` system. Least-squares estimation is the standard
   method for this precisely because, unlike FFT-peak or analysis-by-synthesis
   estimation, it tolerates overlapping frequency responses and short apertures;
   see
   [Smith, *Spectral Audio Signal Processing*](https://ccrma.stanford.edu/~jos/sasp/Least_Squares_Sinusoidal_Parameter.html).
   Past about eight periods per aperture the bases are orthogonal to working
   precision, so the parallel long-window modal analysis keeps a single sweep
   and the refinement cost is confined to the short apertures that need it. The shorter Core/Air branch preserves temporal
   detail, while a parallel long-window residual supplies modal resolution.
   Score persistent inharmonic peaks there, suppress active modal regions in
   the transient residual, and collect the remaining window-corrected power in
   log-frequency cells. Fit non-negative Air powers to the exact analytic
   response of the shared eight-band runtime filterbank, then learn all
   trajectories over normalized note age.
5. **Fit a temporal neural field and bounded detail correction** — deterministic
   Adam optimization trains a compact Fourier-feature multilayer perceptron on
   log-amplitude targets. Analysis uses 128 strictly ordered times: 48 cover the
   first 120 ms in physical time and 80 cover sustain and decay. After fitting,
   per-output `int16` residual keyframes correct detail omitted by the smooth
   neural base. The decoder interpolates raw log-amplitude and pitch corrections
   at control rate without allocating. Physical-time exponential features give
   the onset extra resolution. The fixed periodic input mapping follows the
   motivation of
   [Fourier Features](https://arxiv.org/abs/2006.10739). Neuramar uses ordinary
   tanh hidden units rather than the sinusoidal activations proposed by
   [SIREN](https://arxiv.org/abs/2006.09661), and inference is deliberately at
   control rate rather than audio rate.
6. **Publish atomically** — the completed immutable model replaces the prior
   model at an audio block boundary. No decoding, FFT, fitting, allocation, or
   file access occurs in the render loop.

The learned state stores model coefficients, quantized trajectory corrections,
and analysis metadata, not the source recording or a source path. Version 5
models remain small (about 41 KiB for the current representation) and widen the
version-4 body arrays from 8 Air bands and 6 Bone modes to 16 and 12. The strict decoder retains an
exact zero-correction path for version 2 and an exact zero-stretch path for
version 3, so a memory saved by any shipped release still renders the way it
did. Sessions therefore recall the instrument without depending on an external
file.

Two decoding costs were removed rather than absorbed. The band-limited
resampling used during conditioning and pitch analysis evaluates its
windowed-sinc kernel into a 1024-phase polyphase table instead of calling
`sin` and `cos` forty-eight times for each of hundreds of thousands of output
samples, and one decimation now feeds both the multi-window root search and the
local pitch contour. Training reuses the forward pass it already performs for
the gradient to score the loss, rather than running a second full forward pass
after every update; the same set of candidate weight states is still scored, so
the fit is unchanged.

## Generative initialization and variation

The same compact controller can be used without a source recording. Neuramar
starts from a deliberately voiced C4-anchored Core/Air/Bone field, then applies
seeded coefficient, bias, phase, spectrum, modal, and partial-stretch
perturbations. The generated stretch is drawn from a squared distribution and
bounded far below the fitted-model limit, so most generated identities stay
close to an ideal harmonic series and none becomes a bell. This is
procedural initialization of the instrument's existing neural/DDSP
representation, not sampling from a pretrained corpus model and not a claim to
have learned the distribution of acoustic instruments.

Randomization is local, allocation-allowed work outside the audio callback. A
per-processor seed is combined with a monotonic operation counter, and the model
derives independent named random streams so adding randomness to one subsystem
does not silently reorder every other subsystem. A fixed seed and strength
therefore produce an identical serialized model in regression tests, while
normal UI operations still create a fresh memory.

The visible 1%, 10%, and 100% choices linearly scale bounded *musical* mutation
ranges. They are not percentages of the decoder's broad corruption-rejection
limits. When a learned model is varied, its root, duration, and loop semantics
remain fixed. Its quantized detail correction is relaxed in proportion to the
chosen amount: subtle changes retain almost all learned detail, while a full
variation lets the changed neural field establish a substantially new
trajectory. Every result is published and persisted through the same immutable
model path as a learned memory. Each operation constructs a fresh bounded target
with independently seeded subsystems. The 1% and 10% choices interpolate the
current model toward it; 100% takes the complete reroll. Target Core, Air, and
Bone trajectories are calibrated to bounded layer energy, preventing repeated
Wild presses from becoming an additive loudness random walk.

## Runtime model

The controller is evaluated at a low control rate. After the exact onset frame,
each update predicts one interval ahead and the renderer interpolates toward
that future target, so the trajectory arrives at its described time rather than
one control period late. Each voice has independent phase, note age, envelope,
deterministic variation state, and a separately seeded noise stream for every
Air band.

- **Core** renders the learned harmonic distribution and bounded source pitch
  contour at the played MIDI pitch.
- **Air** renders fitted harmonic-subtracted residual-power trajectories through
  sixteen overlapping log-spaced biquads, with a decorrelated second realization
  supplying stereo width that cancels exactly in a mono sum. The measured
  residual excludes both active Bone neighbourhoods and the narrowband residue
  left at each subtracted partial, and each fit cell is weighted by how many
  spectrum bins it actually observed. Each filtered noise stream is normalized
  to unit expected RMS, so a learned amplitude has a consistent power meaning
  across band centre and ordinary host sample rates.
- **Bone** renders persistence-selected inharmonic candidates when the model
  contains them; inactive candidate slots contribute nothing.
- **Body Lock** interpolates between a pitch-following spectrum and a
  source/filter factorization whose excitation follows harmonic index while its
  smooth resonances remain in absolute frequency.
- **Stretch** scales the fitted stiff-string coefficient into a shared
  per-partial frequency-ratio table. The table is rebuilt only when the
  effective coefficient changes, never per voice and never per sample, and a
  memory without a coefficient is unaffected at every setting.
- **Formant** divides the Body-Locked envelope lookup coordinate by the shift
  and multiplies the Air and Bone centre frequencies by it. The resonant body
  therefore moves in absolute frequency while every oscillator's played pitch
  stays exactly where it was.
- **Touch** treats MIDI velocity as an excitation strength rather than a volume
  control: above a mezzo-forte reference the harmonic tilt leans brighter and
  more of the learned Air is let through, below it the opposite. The learned
  timbre is reproduced unchanged at the reference velocity for any depth, and at
  zero depth the response is the pure amplitude behaviour of earlier releases.
- **Imprint / Dream** trades strict reconstruction for a smoother, more fluid
  interpretation of the learned field.
- **Memory** changes traversal speed; **Orbit** revisits a stable region while a
  note is held.
- **Noise** drives a deterministic, voice-local, slowly varying latent
  coordinate through the controller's existing time/Fourier input manifold.
  It therefore changes Core, Air, Bone, and pitch trajectories only through the
  model. The normalized displacement is also capped at 180 ms of model time so
  long recordings remain an evolution rather than a scrub. Noise never mixes a
  noise waveform into the output. This is distinct from **Air**, which is an
  audible stochastic synthesis branch, and from **Mutation**, which supplies
  bounded per-voice identity offsets.

The Core renderer also separates spectral shape from register level. This is
motivated by the harmonic-oscillator factorization in
[DDSP, section 3.1](https://arxiv.org/abs/2001.04643), whose implementation
removes above-Nyquist harmonics and then
[normalizes the surviving distribution](https://github.com/magenta/ddsp/blob/main/ddsp/core.py#L784-L794).
Neuramar preserves its absolute root-note partial amplitudes, computes the
expected squared power of that audible reference and of the mapped/tapered
register, then applies a control-rate-smoothed correction bounded to -6/+4 dB.
The squared-power form matches independently phased sinusoidal RMS; the bound
prevents missing evidence from turning into an excessive boost.

Core partials retain their smooth 0.43-to-0.49-sample-rate Nyquist taper, but it
is now evaluated at control rate and folded into the ramped amplitude instead of
being recomputed per sample. That has two consequences beyond the arithmetic
saved: the audio loop performs no band-limiting work at all, and every partial
the taper has silenced drops out of the render entirely rather than being
phase-advanced for nothing. A partial re-entering the audible range starts from
a zero amplitude ramp, so the discarded phase continuity is inaudible. Stored
phases are kept reduced to `[0, 1)` and the phase increment is non-negative, so
each audible partial costs one quarter-period-folded polynomial sine, one
multiply-add, and one truncating wrap. There is no oscillator table: the
4096-entry interpolated one was replaced by a six-term odd polynomial that is
both more accurate (measured peak error -133.6 dB against -125.6 dB, RMS
-148.5 dB against -134.6 dB, over 2e7 uniformly spaced phases against
double-precision `sin()`) and free of the random gather that kept the partial
pass scalar. At control rate the Body-Locked spectral coordinate is always a
fixed multiple of the harmonic index, which lets
`pow(index * scale, tilt)` factor into one cached table lookup and one scalar
and removes a `pow` and a `sin` from every rendered partial of every frame. The
mixed output uses floating-point host headroom and remains linear at ordinary
operating levels; only a pathological ±7.95 finite-output guard remains.
Avoiding a base-rate waveshaper is important at high pitch: otherwise newly
generated nonlinear harmonics can fold below Nyquist even though every source
oscillator is band-limited. This aliasing mechanism and antialiased alternatives
are analysed by
[Parker, Zavalishin, and Le Bivic](https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf).

Analysis and rendering share the same Air coefficient and response equations.
When transposition, the learned pitch contour, or `Gravity` moves a band toward
a host's upper frequency limit, its gain tapers smoothly to zero by
`min(0.45 * sample rate, 20 kHz)`, over a fade width of
`min(0.07 * sample rate, 2 kHz)`; the Bone modes take the matching taper,
`min(0.49 * sample rate, 20 kHz)` with a `min(0.04 * sample rate, 1.4 kHz)`
fade. This prevents several out-of-range bands from being clamped onto one
Nyquist-edge frequency. The fixed 20 kHz term is what makes the two layers rate-invariant:
without it a high-rate host kept body content the listener cannot hear while a
44.1 or 48 kHz host removed the same content, so one memory had a different
audible top octave on different hosts. The Nyquist term still means that a
low-rate host deliberately loses content it cannot represent.

The original root frequency, cents offset, and confidence remain visible. A
manual semitone correction is part of the playable state because no pitch
detector can disambiguate every chord, noisy attack, missing fundamental, or
inharmonic source. The correction relabels the MIDI key that reproduces the
learned source pitch; it is intentionally not a tuning control. Automatic
search is bounded to 35–2000 Hz. Its displayed
confidence is the average YIN periodicity score, not a calibrated probability;
it does not include the harmonic hypothesis margin or predict timbre similarity.
The local contour is constrained to four semitones around the global root; it
can retain sufficiently stable bends, slow vibrato, and glide without allowing
octave errors to destabilize synthesis.

## Deliberate exclusions

- No pretrained pitch model or dataset is bundled in version 1. SwiftF0 is a
  promising small modern estimator ([paper](https://arxiv.org/abs/2508.18440),
  [official implementation](https://github.com/lars76/swift-f0)), but including
  fixed weights adds provenance, packaging, universal-build, and long-term
  state-compatibility obligations. The deterministic signal-based detector is
  the portable baseline.
- No TensorFlow, PyTorch, ONNX, Core ML, GPU, or network service is needed.
- No source sample is played back, even for the transient. Transient character
  is represented by learned Core, Air, and reliable Bone amplitude
  trajectories.
- Chords, percussion, full mixes, and highly unstable pitch can still create
  interesting models, but their displayed root should be treated as a guess.

## Future quality path

The current representation leaves a clean upgrade route:
confidence-aware residual subtraction,
continuous peak tracking with robust modal decay fits, a time-varying rather
than fixed stiff-string coefficient, a more rigorous
F0-adaptive/minimum-phase envelope, multirate Air filterbanks,
mipmapped dynamic wavetables, perceptual multi-resolution losses, an optional
small legally-clean pitch model, SIMD matrix inference, and a user-trainable
local prior library. Any upgrade must retain the
offline/real-time boundary, bounded state decoding, deterministic fallback, and
model-version migration.

Two items measured in
[`resynthesis-quality-benchmark.md`](resynthesis-quality-benchmark.md) point at
the same missing branch. On a fixture whose onset is a short broadband burst,
the render's T10-T90 attack is 5 ms *shorter* than the source's, and the finest
spectral resolution scores worse than the coarser ones. Both come from the same
cause: the burst has nowhere to go except the harmonic branch, whose partials
are phase-aligned at note-on, so it is reproduced as a sharper click than it
was. Simionato and Fasciani give transients their own module for exactly this
reason — an inverse-DCT-shaped impulsive component alongside the sines and the
noise
([Frontiers in Signal Processing, 2024](https://www.frontiersin.org/journals/signal-processing/articles/10.3389/frsip.2024.1494864/full)).
Neuramar has no such branch. That is the clearest structural gap in the current
factorisation and the natural next one to close.

The other stated limit is velocity. `Touch` applies a fixed excitation-strength
prior rather than a fitted dynamic response, because one recording contains no
cross-velocity evidence at all. No amount of analysis recovers it; only a
multi-velocity import would, and that would change the instrument's premise.
