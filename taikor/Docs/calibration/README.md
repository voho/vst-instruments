# Taikor controlled calibration capture

This is the measurement contract for the next physical-model tranche. It is a
recording protocol, not a curve-fitting recipe: Taikor does not yet have an
owned real-taiko reference set, and writing a parser or residual model before
the first capture would only turn assumptions into code.

Two experiments are required and must remain separate:

1. low-amplitude linear identification of the drum's mechanical mobility and
   force-to-pressure transfer;
2. full-level bachi strikes for nonlinear contact, repetition and perceptual
   validation.

Ordinary audio alone cannot separate contact mobility, radiation, room sound
and microphone colour.

## Instrumentation and acquisition

Record from one synchronized clock without processing:

| Channel | Quantity | Calibration |
| --- | --- | --- |
| Force | modal-hammer or instrumented-bachi normal force | newtons |
| Head | normal velocity at each observation coordinate, preferably by LDV | metres/second |
| Bachi | axial position or velocity before impact through rebound | metres or metres/second |
| Near | front-head sound pressure at known coordinates | pascals |
| Far | simultaneous front-head sound pressure at known coordinates | pascals |
| Trigger | common acquisition trigger if not embedded | seconds |

Define positive force and velocity as motion from the batter head into the drum.
Measure and compensate every sensor's gain, polarity, phase and latency before
estimating a transfer function. Record the instrumented bachi's added sensor
mass. A point LDV reading is only an approximation to the traction-weighted
velocity of a finite contact patch; validate that approximation with a small
spatial scan or export the measured area average used by the fit.

Put small probe microphones nominally 0.03 m and 0.40 m above the batter head
to span Taikor's Mic Distance control, but move them out of the striker and LDV
paths so neither capsule shadows another. Record exact xyz coordinates and
orientation instead of assuming the nominal distances. Rear-head velocity and
cavity pressure are the first optional channels for the later reciprocal
rear-head/cavity model; a rear microphone pair comes after those state
measurements.

Capture at one common rate of at least 96 kHz, with calibrated analogue/sensor
bandwidth and anti-aliasing. Retain the acquisition system's native resolution
of at least 24 bits; a 32-bit float export is fine but does not add converter
resolution. Keep at least 250 ms of pre-trigger noise and record until every
band-limited energy decay reaches the measured noise floor: at least four
seconds for the first fixture and at least 12 seconds for an ō-daiko. Keep gains
fixed and leave headroom for the hardest strike.

Use an anechoic or sufficiently large damped space for the pressure transfer,
or define a direct-sound window before the first reflection. Archive room sound
and a room impulse response, but do not fit room modes into the drum's poles.
Do not normalise, gate, denoise, compress, equalise, align channels separately,
or discard the native acquisition files.

## Experiment A: linear mobility and radiation

Start with one fully documented nagado or chū-daiko. Use an instrumented modal
hammer at two low force levels and at least 10 accepted repeats per input
coordinate. Verify that the averaged transfer functions agree within the
measurement uncertainty between those levels; otherwise lower the force until
the fixture is demonstrably linear.

Measure a matrix, not only one driving point. The input and LDV observation
coordinates must cover centre and edge radii plus at least two azimuths, so
degenerate sine/cosine branches and the cross-mobility between simultaneous
contact locations are identifiable. For every force point `j`, measure velocity
at every point `i` needed by the contact model, producing `Y_ij = V_i / F_j`.
Sequential scanning is acceptable if the fixture, tension and a fixed reference
channel remain stable. Reciprocity is a measured validation result, not an
assumption used to fill missing cells.

Capture near and far pressure during the same impacts. These hammer measurements
identify the linear mechanical and acoustic transfers. Palm, rim, paired-hit and
roll conditions do not belong in this fit.

## Experiment B: nonlinear bachi validation

Use a controlled striker or synchronized optical/LDV bachi tracking so incoming
and rebound velocities are observed rather than inferred from audio. Record the
following minimum set with 10 accepted repeats per row and speed:

| Stroke | Bachi | Radius | Incoming speeds |
| --- | --- | ---: | --- |
| Don | hard wood | 0.20, 0.75 | about 2.0, 3.5, 4.5 m/s |
| Ka | hard wood | 0.85 | about 2.0, 3.5, 4.5 m/s |
| Don | soft/wrapped | 0.20 | about 2.0, 3.5, 4.5 m/s |

Bin takes by measured speed, not player labels such as *soft* and *hard*. Keep
misses and double contacts in the archive with a rejection reason; silently
deleting them biases the duration and rebound distributions.

After the minimum set, add rim strikes, Tsu and held palms, same-position pairs
at 20/50/100/200 ms, and short constant-speed rolls. Those are held-out
interaction checks, not extra data for making a linear mobility fit look good.
Repeat the proven protocol across the ō-daiko, okedo and shime family only after
the first fixture closes the model/data loop.

## Session metadata

Record beside the raw channels:

- drum family, maker, serial or stable session identifier;
- batter and rear-head diameter, thickness, material, surface density and
  measured tension, including the tension method;
- body depth, wall thickness, material, mass, openings and mounting/stand;
- temperature, humidity, room dimensions and direct-window limits;
- bachi dimensions, mass, tip radius/footprint, material and sensor mass;
- strike radius, azimuth, incidence angle, incoming speed and articulation;
- microphone, preamp, LDV and force-sensor models, calibration transfer,
  coordinates, orientation, gains and compensated latency;
- sample rate, time origin and every acquisition or export operation.

Use the head centre as the origin, the inward batter-head normal as +z, radius
divided by head radius, and radians counter-clockwise from a photographed
reference mark. SI units stay in the data; friendly units belong only in notes.

## Analysis contract after capture one

The first real export will fix the smallest file schema and one dependency-free
offline analyzer. Report peak force, impulse, contact duration, incoming and
rebound velocity, and restitution per accepted bachi strike. Estimate transfer
functions and coherence per condition from cross/auto spectra averaged over the
repeat ensemble, not from one transient.

The linear report must include:

- the complex matrix H1 mobility `Y_ij = G_v_i_f_j / G_f_j_f_j`, coherence,
  reciprocity error and uncertainty;
- modal frequency, decay/Q and spatial residue or mode shape;
- absolute complex `P_near/F` and `P_far/F` transfers with coherence;
- `P_far/P_near` only as an additional distance diagnostic, not as the
  force-to-pressure model;
- onset-relative third-octave energy in 0-30, 30-250 and 250-1500 ms windows;
- Schroeder energy-decay slopes per band, truncated above the measured noise
  floor;
- force-binned repeatability and pairwise correlation of mean-removed
  third-octave spectra.

Physical FRFs use each channel's absolute amplitude and phase calibration and
receive no comparison gain. A perceptual audio comparison may fit one global
nuisance gain on training captures, but that gain is then frozen across
microphones, windows and holdouts. MP3 previews, per-window normalisation,
rectangular-window band sums and contact force inferred from audio are not
acceptance measurements.

Thresholds come from repeat distributions and sensor uncertainty, not from the
current model. Identify the explicit modes and passive residual together, or
validate the explicit modes first and freeze them only after a feasibility
check. Do not pointwise subtract a noisy explicit response and clip negative
residues. Hold out repeats, speeds and at least one position for validation.

[Passive Vector Fitting](https://publications.pvandenhof.nl/Paperfiles/Rodrigues%26etal_SCL2024.pdf)
is a candidate offline method to evaluate, not a drop-in fitter: the published
method is discrete-time and does not directly preserve fixed Taikor poles. The
shipped result must be a continuous-time positive-real mobility, or use an
explicitly passivity-preserving mapping rebuilt per host rate and verified from
8 to 384 kHz. The acoustic fit is a separate one-way force-to-pressure
observation with shared physical poles and microphone-specific complex
residues, following the drum transfer-function approach of
[Nagata and Saito](https://doi.org/10.1177/10775463241272937); it is not part of
the positive-real mechanical feedback port. The fitted engine must retain the
existing discrete energy proof, contact convergence and real-time CPU gates.

## Public leads are not acceptance references

No public corpus found on 2026-08-11 contains all four required pieces: real
taiko, calibrated force, driving-point velocity and simultaneous known-distance
microphones under a licence suitable for commercial product development.

- [AIST RWC Instruments v1](https://zenodo.org/records/17170844) contains the
  strongest isolated nagado, hirado, okedo and shime vocabulary found, but it is
  CC BY-NC 4.0 and has no force or microphone geometry. Treat it only as a
  noncommercial research lead; obtain written permission before using it in
  commercial development or calibration.
- [Freesound Nagado 95624](https://freesound.org/people/whatsanickname4u/sounds/95624/)
  is CC BY 3.0 and contains a few individual hits varying in volume, recorded
  on a Zoom H2 in a slightly echoey hall with unknown geometry. The
  uploader-labelled [Taiko.wav 261468](https://freesound.org/people/dirtydowntowner/sounds/261468/)
  is CC0 but supplies no instrument, performer, microphone, room or isolated-
  strike provenance. Neither may calibrate mechanics.
- The [Dynamic Percussion Dataset](https://zenodo.org/records/3780109) is a
  CC BY 4.0 Western-percussion set with five ordinal dynamic labels whose class
  coverage varies. It releases mono near-field samples; contaminated mid/far
  recordings were discarded. It can suggest attack trends, not taiko transfer
  functions.
- [RealImpact](https://samuelpclarke.com/realimpact/) is the best public example
  found of synchronized impact force and multi-distance radiation capture, but
  it contains rigid everyday objects rather than drums and the dataset audio
  has no explicit licence. Its method is a guide; obtain permission before
  using its data.
- Fukamachi's [2025 feature](https://doi.org/10.20697/jasj.81.7_481)
  qualitatively illustrates repeatable strike-position, mallet type/angle and
  fingertip-mute effects on timpani and tom-tom modes. It publishes spectra,
  not a force-controlled reusable dataset.

These sources can falsify an obviously implausible result. Only an owned or
explicitly commercially licensed calibrated session can set Taikor's mechanical
and radiation coefficients.
