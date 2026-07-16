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

That control follows the source-filter motivation in
[Schwarz and Rodet's spectral-envelope work](https://quod.lib.umich.edu/i/icmc/bbp2372.1999.417?rgn=main;view=fulltext): preserving an envelope in absolute
frequency can retain source-like resonances while harmonic fine structure moves
with pitch. At each control frame, Neuramar separates the learned magnitudes
into a short power-smoothed envelope and the complementary harmonic-index
excitation residual. Their product remains exact at every observed root-note
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
architecture, training procedure, source code, or weights. Its eight Air bands
have adjacent logarithmic edges from 80 Hz to 16 kHz at the fixed analysis rate.
A sequential weighted least-squares sinusoid fit first removes the
pitch-following Core from each waveform frame. Core and Air targets use a
pitch-adaptive power-of-two aperture targeting four root periods and bounded to
512–4096 samples at the fixed 48 kHz analysis rate, avoiding one fixed 85 ms
window that would smear short attacks. The 4096-sample ceiling necessarily
contains fewer than four periods below 46.875 Hz. Bone selection and decay
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
3. **Track and factor the sound** — follow a constrained local pitch contour
   around the root; at each frame, use pitch-adaptive weighted cosine/sine least
   squares to estimate harmonic amplitude and phase and subtract the fitted
   sinusoids from the waveform. The shorter Core/Air branch preserves temporal
   detail, while a parallel long-window residual supplies modal resolution.
   Score persistent inharmonic peaks there, suppress active modal regions in
   the transient residual, and collect the remaining window-corrected power in
   log-frequency cells. Fit non-negative Air powers to the exact analytic
   response of the shared eight-band runtime filterbank, then learn all
   trajectories over normalized note age.
4. **Fit a temporal neural field and bounded detail correction** — deterministic
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
5. **Publish atomically** — the completed immutable model replaces the prior
   model at an audio block boundary. No decoding, FFT, fitting, allocation, or
   file access occurs in the render loop.

The learned state stores model coefficients, quantized trajectory corrections,
and analysis metadata, not the source recording or a source path. Version 3
models remain small (about 35 KiB for the current representation), while the
strict decoder retains an exact zero-correction path for version 2 state.
Sessions therefore recall the instrument without depending on an external file.

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
  eight overlapping log-spaced biquads. Each filtered noise stream is normalized
  to unit expected RMS, so a learned amplitude has a consistent power meaning
  across band centre and ordinary host sample rates.
- **Bone** renders persistence-selected inharmonic candidates when the model
  contains them; inactive candidate slots contribute nothing.
- **Body Lock** interpolates between a pitch-following spectrum and a
  source/filter factorization whose excitation follows harmonic index while its
  smooth resonances remain in absolute frequency.
- **Imprint / Dream** trades strict reconstruction for a smoother, more fluid
  interpretation of the learned field.
- **Memory** changes traversal speed; **Orbit** revisits a stable region while a
  note is held.

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

Core partials retain their smooth 0.43-to-0.49-sample-rate Nyquist taper. The
mixed output uses floating-point host headroom and remains linear at ordinary
operating levels; only a pathological ±7.95 finite-output guard remains.
Avoiding a base-rate waveshaper is important at high pitch: otherwise newly
generated nonlinear harmonics can fold below Nyquist even though every source
oscillator is band-limited. This aliasing mechanism and antialiased alternatives
are analysed by
[Parker, Zavalishin, and Le Bivic](https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf).

Analysis and rendering share the same Air coefficient and response equations.
When transposition, the learned pitch contour, or `Gravity` moves a band toward
a host's upper frequency limit, its gain tapers smoothly to zero by 0.45 times
the sample rate. This
prevents several out-of-range bands from being clamped onto one Nyquist-edge
frequency. It also means that a low-rate host deliberately loses Air content it
cannot represent.

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

The current representation leaves a clean upgrade route: joint rather than
sequential sinusoidal estimation, confidence-aware residual subtraction,
continuous peak tracking with robust modal decay fits, a more rigorous
F0-adaptive/minimum-phase envelope, denser or multirate Air filterbanks,
mipmapped dynamic wavetables, perceptual multi-resolution losses, an optional
small legally-clean pitch model, SIMD matrix inference, and a user-trainable
local prior library. Any upgrade must retain the
offline/real-time boundary, bounded state decoding, deterministic fallback, and
model-version migration.
