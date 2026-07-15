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
   difference function, reject weak periodicity estimates, align octave-related
   candidates in log frequency, and score half/current/double-root hypotheses
   using harmonic support. The mathematical basis is
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
4. **Fit a temporal neural field** — deterministic Adam optimization trains a
   compact Fourier-feature multilayer perceptron on log-amplitude targets.
   Physical-time exponential features give the onset extra resolution. The
   fixed periodic input mapping follows the motivation of
   [Fourier Features](https://arxiv.org/abs/2006.10739). Neuramar uses ordinary
   tanh hidden units rather than the sinusoidal activations proposed by
   [SIREN](https://arxiv.org/abs/2006.09661), and inference is deliberately at
   control rate rather than audio rate.
5. **Publish atomically** — the completed immutable model replaces the prior
   model at an audio block boundary. No decoding, FFT, fitting, allocation, or
   file access occurs in the render loop.

The learned state stores model coefficients and analysis metadata, not the
source recording or a source path. Sessions therefore recall the instrument
without depending on an external file, while remaining far smaller than an
embedded sample.

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
- **Body Lock** interpolates between fixed-frequency resonances and a spectrum
  that follows the played pitch.
- **Imprint / Dream** trades strict reconstruction for a smoother, more fluid
  interpretation of the learned field.
- **Memory** changes traversal speed; **Orbit** revisits a stable region while a
  note is held.

Analysis and rendering share the same Air coefficient and response equations.
When transposition or `Gravity` moves a band toward a host's upper frequency
limit, its gain tapers smoothly to zero by 0.45 times the sample rate. This
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
continuous peak tracking with robust modal decay fits, denser or multirate Air
filterbanks, mipmapped dynamic wavetables, perceptual multi-resolution losses,
an optional small legally-clean pitch model, SIMD matrix inference, and a
user-trainable local prior library. Any upgrade must retain the
offline/real-time boundary, bounded state decoding, deterministic fallback, and
model-version migration.
