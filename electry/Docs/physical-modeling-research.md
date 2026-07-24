# Electry physical-modeling research and implementation contract

Electry 1.1 is a white-box physically modeled dry electric guitar with named
reference literature, not a black-box claim that one plug-in reproduces a
specific instrument. This document separates the published models the engine
implements from the parts that remain efficient Electry voicing decisions.
These choices make Electry physically explicit and measurable; they do not by
themselves establish that it is indistinguishable from a recorded guitar. Such
a claim would require calibrated captures of identified hardware and
level-matched blind listening.

## Claims boundary

| Block | Reference | What Electry 1.0 implements | Precise claim |
| --- | --- | --- | --- |
| String core | Karjalainen, Välimäki, and Tolonen's single-delay-loop condensation of digital waveguides | Eight independent strings in Drop-E tuning, each with two transverse-polarisation single-delay-loop waveguides, third-order Lagrange fractional reads, and a contractive bridge coupling matrix | The published SDL string family with two coupled polarisations per string; not a bidirectional multi-rail scattering simulation |
| Two-stage decay and beating | Two-polarisation string behavior described in the same plucked-string literature | The polarisation parallel to the body carries a 1.7x longer decay target and a sub-cent detune, so the mixed output beats slowly and decays in two stages | A qualitative reproduction of the documented mechanism with voiced constants; not calibrated polarisation data from a measured instrument |
| Stiffness dispersion | Stiff-string inharmonicity `B = pi^3 E d^4 / (64 T L^2)` (Fletcher and Rossing) and robust factored allpass design practice (Rauhala and Välimäki; Abel and Smith) | A per-note `B` from string diameter, effective wound-core bending fraction, scale length, and tension drives an eight-stage factored first-order cascade; two coefficients are fitted jointly at low and high partials, with exact fundamental phase compensation | A physically derived, bounded two-band fit whose regression error is under 20% at both references for the worst heavy Drop-E case; not a capture-fitted very-high-order piano dispersion filter |
| Loop damping and tuning | Decay-time-targeted loop-filter design from the plucked-string literature | Per-string, per-fret one-pole loop filters solved by bisection from independent T60 targets at the fundamental and a high reference frequency, with all loop-filter phase delays compensated analytically at the fundamental | Decay-targeted loop design with exact fundamental tuning (regression bound: under 8 cents across E1..D6 at tested host rates through 384 kHz); not per-partial measured decay matching |
| Dead spots | Fleischer's electric-guitar dead-spot studies relating neck conductance to decay time | A per-string fret-position Gaussian that locally shortens decay, deepened by the bolt-on end of the construction axis | The documented mechanism direction with voiced positions and depths; not measured conductance maps of specific instruments |
| Tension modulation | Tolonen, Välimäki, and Karjalainen's tension-modulation nonlinearity | A string-energy envelope drives a bounded shortening of the loop delay, producing the attack pitch glide that relaxes over hundreds of milliseconds; slaps deepen it | The published phenomenon in its energy-envelope shortcut form; not the exact elongation integral or a time-varying-fraction-delay implementation |
| Plectrum and finger excitation | Plectrum and touch interaction modeling by Germain and Evangelista and by Evangelista and Eckerholm | A three-phase excitation combines contact retention and scrape, a principal string-period-scaled two-pole modal release that approximates triangular pluck displacement, and a normally much smaller broadband pick-edge transient for sustained pick styles; delay-line projection is level-calibrated to open E4 so equal effort remains usable on E1, while polarity, polarisation split, spectrum, and comb position differ per style | A realtime modal approximation to released-string displacement plus a separate pick edge and bounded register calibration; not an exact delay-line initial-condition solve, beam-mechanics plectrum profile, or force-based finger contact solver |
| Fret collisions (slap) | Bilbao and Torin's energy-balanced string/fretboard collision modeling | A decaying collision window whose soft limit clips vertical displacement against a velocity-dependent threshold and re-radiates deterministic rattle noise | Collision-informed slap behavior in a bounded, stable form; not an FDTD distributed-contact simulation |
| Hammer-on and pull-off | Touch/legato interaction models from Evangelista and Eckerholm | Keyswitched legato: a sounding string within reach retargets its delay over about 10 ms while the loop state is preserved, with a soft finger excitation and no plectrum noise | Continuous-state legato with fingered attacks; not a distributed finger-force model |
| Pickups | Paiva, Pakarinen, and Välimäki's pickup acoustics and modeling; low-frequency pickup nonlinearity measurements (Novak et al.); engineering aperture analyses | Per-string pickup-position combs follow each fret; an O(1) fractional rectangular moving average gives the finite aperture's exact sinc response; bounded flux nonlinearity plus shallow string-mass/pole balance is differentiated into induced EMF, guarded ultrasonically, then passed through the loaded coil/tone circuit | The published pickup signal structure (position comb, finite aperture, nonlinear flux, induced voltage, electrical resonance) with datasheet-plausible level calibration; not a magnetic finite-element or capture-fitted model of named pickups |
| Solid body | Solid-body bridge-admittance and dead-spot literature; geometric estimates | Structural bridge displacement is differentiated before four double-precision, peak-normalised modal resonators and a 4 kHz guard, producing body-induced voltage before the loaded pickup coils; positive real modal conductance across each note's first six partials can only shorten loop T60 | Geometry-informed structural pickup voltage plus passive mode-dependent energy extraction; not undifferentiated acoustic body displacement mixed into pickup voltage, and the mode tables remain voicing estimates rather than measured admittance data |
| Construction controls | Solid-body material/geometry contrasts, humbucker vs single-coil construction, set-neck vs bolt-on, and modern extended-range scale practice | Wood, size, shape, construction, and pickup type interpolate between contrasting reference voicings; scale length spans 25.5 to 28 inches for Drop-E | Parametrized construction and extended-range voicing; not a licensed or capture-verified reproduction of a named instrument |
| Play noise | Handling-noise observations in the virtual slide guitar work of Pakarinen, Puputti, and Välimäki | Deterministic seeded plectrum scrape, finger contact, release damping noise, and slap body knock, band-shaped per string (wound vs plain) and split between a one-percent string trace and local pickup/body paths | Procedural, deterministic contact noise consistent with the documented mechanisms; not convolved recordings or measured contact-noise spectra |
| Sympathetic string coupling | Bank and Karjalainen's passive admittance modeling and the sympathetic-string literature | The plucked strings' bridge force drives a one-sample-delayed bus; every string that is not being fingered runs its own single-polarisation waveguide at its open pitch, with its own T60-derived loop filter, exact fundamental phase compensation and bridge pickup tap. Only played voices write to the bus and only idle voices read it | A one-directional (loss-only from the driver's point of view) slice of bridge coupling, provably acyclic and therefore unconditionally stable; not a shared multiport bridge scattering junction with mutual re-radiation |
| Bridge-hand damping | Palm-muting practice and the same decay-targeted loop design | A continuous pressure interpolates the string's T60 geometrically toward a 40 ms stop and scales the high-frequency decay ratio, re-solving the same loop filters and the analytic phase compensation so the note stays in tune; the coupled strings are damped and starved with it | Progressive contact damping as a decay target, applied identically to every play style; not a distributed hand/string contact solve |
| Strum travel | Ordinary plectrum kinematics | Note-ons inside a 35 ms window are treated as one stroke; the first string fixes the edge the pick starts from and every further string's excitation is delayed by the travel time per string crossed | Constant-velocity pick travel across the string plane; not a model of pick angle, chord voicing, or the player's hand position |
| Controllable artifacts | The same touch/collision literature plus bridge-hardware behavior | An exactly bypassable deterministic path combines a bridge-hardware modal bank driven through the selected pickup mix, partial non-slap fret contact, and per-string saddle rattle, all driven by played energy. It is mechanical hardware noise, distinct from the sympathetic string coupling above | Plausible procedural imperfection with bounded feed-forward resonators; not measured hardware-noise statistics |
| Audible-work culling | Standard realtime-DSP practice | A pickup faded out by the selector is skipped entirely; Mono runs one shared coil/DC/decimation chain and mirrors it; damping-only control moves reuse the existing dispersion fit; the whole engine freezes to exact zero once nothing vibrates and the shared path is below -120 dBFS | Removal of inaudible arithmetic with the audible result unchanged; not a quality/latency trade |
| Oversampling | Standard nonlinear-audio antialiasing practice | The complete physical, body, collision, and nonlinear pickup path runs at 2x for host rates through 96 kHz, followed by a fixed 63-tap halfband FIR; higher-rate hosts run 1x | Genuine internal oversampling and filtered decimation, not a quality label applied to a native-rate nonlinear stage |
| Output field | Phase-coherent divided/hex pickup practice | Mono is the conventional summed DI. Stereo weights each modeled string by its physical lateral position, keeps shared body modes centered, uses linked output limiting and independent matched decimation, and folds coherently to mono | A virtual divided-pickup string field with no time or phase widening; not room, amplifier, cabinet, chorus, or acoustic stereo |

## Implemented signal path

The authoritative implementation is `Source/DSP/ElectryEngine.cpp`:

1. MIDI notes 12..27 (C0..D#1) are latching keyswitches that select the play
   style: downstroke, upstroke, alternating strokes, hammer-on/pull-off, tap,
   palm mute, chug, dead note, natural and pinch harmonics, repeated tremolo
   picking, bend to +1 or +2 semitones, release bends from +1 or +2 semitones,
   and slap. Notes 28..86 are playable on eight physical strings in Drop-E
   tuning (E1-B1-E2-A2-D3-G3-B3-E4); a deterministic allocator maps each note,
   preferring a repick of an already-sounding note, then the hammer-on
   continuation of the nearest sounding string, then the free string with the
   lowest fret (which reproduces open-position chord shapes), and finally an
   oldest-first steal.
2. Each string voice runs two single-delay-loop waveguides (vertical and
   horizontal polarisation). Each loop has a third-order Lagrange fractional
   read, eight factored first-order dispersion allpasses jointly fitted from
   the string's physical inharmonicity at two partials, a one-pole damping filter solved from T60 targets,
   and a release/mute gain ramp. A contractive bridge matrix exchanges a
   small amount of energy between the polarisations.
3. The loop delay compensates the exact phase delay of every loop filter at
   the sounding fundamental. Bends, hammer-on glides, the pitch wheel, and
   tension modulation move the delay target; a short smoother keeps the
   motion click-free. There is deliberately no DC filter inside the loop: a
   fixed-corner blocker's steep phase lead near a low fundamental would
   detune the upper partials against the compensated fundamental, and the
   pickup position comb already rejects DC exactly.
4. Excitation runs in phases: a contact stage applies a bounded total
   retention over the pick/string engagement and plays band-shaped scrape or
   finger noise. At release, the principal signal passes two low-pass stages
   whose coefficient follows the string period, giving the low partials the
   approximate `1/n^2` displacement tilt of a triangular pluck; for ordinary
   sustained pick styles, a much smaller short raised-cosine component retains
   the broadband pick edge. Both enter
   the polarisations with a style-dependent split and polarity. The
   pluck-position comb scatters the release with opposite sign one comb delay
   behind the write head, the second travelling-wave image of the excitation
   point. A single velocity profile drives release level and brightness,
   contact noise, tension, and collision response, so the Velocity knob
   changes expression coherently. The compact release is projection-normalised
   for the sounding delay length against open E4, which keeps E1/B1 at a
   practical level without changing their modal tilt. Only a trace of the contact-noise burst
   enters the recirculating string; most remains a local pickup/body transient
   so scrape does not turn into a sustained bright pitch.
5. Slap opens a decaying fret-collision window that soft-limits vertical
   displacement against a velocity-dependent clearance and adds deterministic
   rattle proportional to the clipped excess, plus a thumb knock into the
   body. A string-energy envelope shortens the loop delay (tension
   modulation), so hard attacks start audibly sharp and relax; slaps deepen
   the effect.
6. Every string that no note owns is a bridge-coupled sympathetic string. The
   plucked voices' bridge force is accumulated into a bus that the coupled
   strings read one sample later, which removes any dependence on voice order
   and any algebraic loop. Each coupled string reuses its own otherwise idle
   delay line at its open pitch with a T60-derived loop filter and exact
   fundamental phase compensation, is bounded by a rational soft limit, and is
   read through a bridge-position tap and an induced-EMF difference before
   joining the pickup sums. Because only `active` voices write the bus and only
   inactive voices read it, the coupling graph is a DAG and no coupling gain
   can create a growing loop. The muting hand of a palm-muted, chugged or dead
   note damps and starves the coupled strings, and the whole path is exactly
   bypassed at 0%.
7. Each pickup reads every string's displacement as the freshly written
   bridge sample minus a fractional read at the pickup delay; that delay
   follows the sounding length, so fretting up the neck moves the comb
   exactly as the geometry does. The tap passes a fractional finite-window
   spatial average scaled by wave speed and magnetic aperture, then a bounded
   distance-flux polynomial with shallow per-string magnetic-mass/pole balance.
   Its time derivative is the induced pickup EMF;
   an oversampled 16 kHz guard bounds the differentiator. The resulting
   electrical string sums drive one resonant second-order coil filter per pickup, morphing humbucker to
   single-coil resonance and Q, loaded further by the passive tone control;
   the selector fades neck, both (with the paired-coil resonance shift), or
   bridge.
8. Four modal body resonators fed from bridge motion and contact/knock noise
   are normalised to their requested gain at each configured modal frequency;
   this prevents low-frequency modes from receiving the old implicit
   `1/sin(omega)` boost. Bridge displacement is differentiated once before
   the slowly automated double-precision modal bank, then guarded at 4 kHz
   and combined with the electrical string sums before the same loaded-coil
   circuit. Differentiation commutes with fixed modal filters; placing it first
   prevents coefficient automation from becoming a voltage spike. This is a
   body-induced pickup voltage, never an
   undifferentiated displacement-to-voltage sum. The same modal frequencies,
   Q, and levels define a bounded positive
   bridge-conductance map across the first six string partials; it only removes
   loop energy, making body modes alter sustain without risking feedback
   growth. An optional eight-mode open-string bank plus per-string saddle/fret
   contact adds deterministic Artifacts detail without feeding energy back
   into the string loops.
9. Mono sums the normal pickup field to exact dual mono. Stereo derives a
   modest side field from each string's physical low-to-high position while
   keeping body motion centered; there is no delay or modulation. Both
   channels pass matched coil filters, 5 Hz DC blockers, and one linked
   bounded soft guard. At host
   rates through 96 kHz, steps 2-9 run at 2x and a fixed 63-tap halfband FIR
   decimates to the host rate; 192 and 384 kHz hosts run natively. The result
   is dry guitar with no amplifier, cabinet, room, or effect processing.

## Dual-polarisation string loops

Karjalainen, Välimäki, and Tolonen,
[*Plucked-String Models: From the Karplus-Strong Algorithm to Digital
Waveguides and Beyond*](http://users.spa.aalto.fi/vpv/publications/cmj98.pdf)
(Computer Music Journal 22(3), 1998), show that a bidirectional waveguide
string with consolidated losses reduces exactly to a single delay loop, and
that output combs equivalent to pluck and pickup positions can be factored
out of the loop. Electry uses that condensation: per polarisation, one
fractional delay line, one damping filter, and one factored dispersion
cascade. The pluck comb receives a two-pole, string-period-scaled modal release
whose spectral tilt approximates triangular displacement, plus a smaller
pick-edge transient; its opposite travelling-wave image is written directly
into the line. Each pickup tap is the current bridge sample minus a delayed
read.

Real strings vibrate in two transverse polarisations that decay differently
and detune slightly, which produces beating and two-stage decay. Electry's
horizontal loop targets a 1.7x longer T60 and a fraction-of-a-cent detune,
and both loops exchange energy through the contractive bridge matrix
`[[1-c, c], [c, 1-c]]` with `c = 0.004`, whose eigenvalues never exceed one.
The constants are voicing decisions; the mechanism is the documented one.

## Stiffness dispersion

The stiff-string inharmonicity coefficient follows the standard physics
(Fletcher and Rossing, *The Physics of Musical Instruments*):

```text
B = pi^3 E d_b^4 / (64 T L^2)
T = mu (2 L f0)^2
```

with Young's modulus `E = 200 GPa`, the bending diameter `d_b` equal to the
plain diameter or the wound core fraction, linear density `mu` from the
gauge-scaled diameter (wound strings carry a mass packing factor), and the
sounding length `L` from the scale-length axis and fret. Partial `n` of such
a string is sharpened by `sqrt(1 + B n^2)`.

Digital waveguides model dispersion with allpass filters in the loop.
Electry follows the design practice of Rauhala and Välimäki
([*Dispersion modeling in waveguide piano synthesis using tunable allpass
filters*](https://www.researchgate.net/publication/229009513_Dispersion_modeling_in_waveguide_piano_synthesis_using_tunable_allpass_filters))
and Abel and Smith
([*Robust Design of Very High-Order Allpass Dispersion
Filters*](https://www.dafx.de/paper-archive/2006/papers/p_013.pdf)) in
factored form. Each polarisation uses eight first-order allpass sections:
four share one coefficient and four share a second. At note configuration, a
bounded two-pass search jointly minimises relative delay-deficit error at a
low partial (`2..4`, depending on available bandwidth) and a high partial
(`min(16, 0.3 fs / f0)`). Coefficients remain strictly inside the unit circle
in `[-0.995, 0]`. Wound strings use an effective bending-core fraction smaller
than the geometric core because the wrap slips under flexure instead of acting
like one solid rod; the full diameter still determines mass and tension. The
heavy, short-scale Drop-E regression bounds fit error at both references,
rather than checking only that a coefficient is nonzero. Electry does not
claim the full-band accuracy of the cited very-high-order capture-fitted
designs.

Tuning is exact at the fundamental: the loop delay subtracts the analytic
phase delay of the damping one-pole and all eight allpasses at `f0`, evaluated
from their closed-form responses. The regression suite bounds the sounding
fundamental within 8 cents of equal temperament across E1..D6 at 44.1, 48,
and 96 kHz.

## Loop damping, dead spots, and release

Each note solves its one-pole loop filter from two targets: T60 at the
fundamental and T60 at `min(3.6 kHz, 0.32 fs)`, derived from the string's
base decay, the string-age axis (old strings lose highs first), the gauge
axis, wound versus plain construction, and the construction axis (a set neck
sustains slightly longer than a bolt-on in this voicing). The magnitude
ratio of a one-pole is monotonic in its coefficient, so 18 bisection steps
suffice; the loop gain then sets the exact fundamental decay through the
filter's magnitude at `f0`.

Helmut Fleischer's dead-spot studies
([*Investigating Dead Spots of Electric
Guitars*](https://www.researchgate.net/publication/233653803_Investigating_Dead_Spots_of_Electric_Guitars),
with the companion conductance-measurement work) relate locally raised neck
conductance to abnormally fast string decay at specific fret positions.
Electry implements the documented direction of that mechanism: each string
carries a Gaussian fret-position dip in T60 whose depth grows with the
bolt-on end of the construction axis. The centre positions and depths are
voicing estimates, not conductance measurements.

Note release ramps an extra loop-gain factor toward a roughly 60 ms T60 over
about 22 ms — the fretting or picking hand damping the string — and
optionally injects a short wound- or plain-voiced damping noise. The palm
mute style caps T60 between 0.6 s and 90 ms depending on the mute-damping
control, with a darker excitation and stronger contact noise.

## Sympathetic bridge coupling

An electric guitar's unfingered strings are not silent while you play. Energy
crosses the bridge and drives them at their own open pitches, which is a large
part of why an eight-string instrument sounds dense and why players use fret
wraps to stop it. Electry models the mechanism directly instead of colouring
the output with a resonator bank.

The plucked voices accumulate their bridge-bound wave `0.5 (v + h)` into a bus.
Every string that no note owns reuses its own otherwise idle delay line as a
single-polarisation waveguide tuned to its open pitch, solves its loop gain
from the same T60 machinery the played strings use, compensates the loop
filter's phase delay at the fundamental, and is read through a
bridge-position tap and an induced-EMF difference before joining the pickup
sums. The bus is read one sample late, which makes the result independent of
the order the voices happen to be rendered in.

The stability argument is structural rather than numerical: only voices with
`active` set write to the bus, and only voices without it read from the bus.
The coupling graph is therefore a directed acyclic graph with exactly one
edge class, so no coupling gain can close a loop. Each coupled loop is
individually a comb with gain strictly below one and additionally carries a
bounded rational soft limit, so a driving partial landing exactly on a coupled
mode still cannot exceed a fixed ceiling. The regression suite drives all eight
strings at maximum coupling, maximum artifacts and maximum output gain at
44.1, 96 and 192 kHz and requires the result to stay inside the analytic guard
ceiling and to ring out to exact silence.

This is deliberately the loss-only, one-directional slice of the passive
admittance direction that Bank and Karjalainen and Maestre, Scavone and Smith
describe. A full shared bridge junction would let the coupled strings
re-radiate into the played ones; Electry does not claim that, and the second
-order term is small enough that the omission is honest rather than
convenient.

Because the mechanism is physical, the muting hand is too. A palm-muted,
chugged or dead-note passage puts the heel of the hand across every string, so
those articulations damp the coupled strings with a per-sample contact loss and
cut the injection at the same time. That is what keeps a Drop-E chug tight
instead of washing it in open-string ring.

## Bridge-hand damping

The Muted and Chug keyswitches cap a note's T60 at fixed points. Palm Mute is
the continuous version of the same physics, available to every play style and
performable from MIDI CC 2: the pressure interpolates the string's decay
geometrically from its own T60 toward a 40 ms stop,
`T60' = exp((1-p) ln T60 + p ln 0.040)`, and multiplies the high-frequency
decay ratio by `1 - 0.5 p` as more of the string is covered. Zero pressure is
therefore a mathematical no-op, not a small effect.

The important detail is that this runs through the ordinary loop-filter solve
rather than as a gain after the fact. The one-pole is re-bisected against the
new decay targets and the loop delay subtracts the new analytic phase delay at
the fundamental, so a heavily muted string sounds the played pitch instead of
drifting sharp as a naive extra damping filter would make it.

## Strum travel

A pick crosses the strings at a finite speed. Electry treats note-ons that
arrive inside one 35 ms window as a single stroke: the first of them fixes the
edge the pick starts from, and every later string's excitation is armed and
released after `spread * |string - anchor|` seconds. The voice is allocated,
fretted and choked immediately, so string allocation, voice counts and note-off
handling are unchanged; only the excitation waits. At a zero spread the
behaviour is bit-identical to a block chord.

## Tension modulation

Tolonen, Välimäki, and Karjalainen,
[*Modeling of tension modulation nonlinearity in plucked
strings*](https://www.researchgate.net/publication/3333696_Modeling_of_tension_modulation_nonlinearity_in_plucked_strings)
(IEEE Transactions on Speech and Audio Processing 8(3), 2000), model the
audible fundamental-frequency glide of a hard pluck as displacement-driven
tension variation shortening the effective period. Electry implements the
energy-envelope shortcut: a two-rate envelope of the squared loop signals
scales the delay target by `1 / (1 + k E)`, with `k` from the gauge axis and
multiplied 3.2x for slaps. The envelope's release tracks hundreds of
milliseconds, so a hard slap starts several cents sharp and relaxes to true
pitch, which the regression suite verifies directly. This is the published
phenomenon in its efficient form, not the exact string-elongation integral.

## Player excitation and play noise

Germain and Evangelista,
[*Synthesis of guitar by digital waveguides: Modeling the plectrum in the
physical interaction of the player with the
instrument*](https://ieeexplore.ieee.org/document/5346502/) (WASPAA 2009),
and Evangelista and Eckerholm,
[*Player-Instrument Interaction Models for Digital Waveguide Synthesis of
Guitar: Touch and
Collisions*](https://www.researchgate.net/publication/224130817_Player-Instrument_Interaction_Models_for_Digital_Waveguide_Synthesis_of_Guitar_Touch_and_Collisions)
(IEEE TASLP 18(4), 2010), model how the plectrum and the player's touch
shape the excitation, and demonstrate pluck styles, muting, and collision
effects within waveguide synthesis. Electry adopts the behavioral structure
in signal form:

- **Contact phase.** The pick or finger touches a possibly ringing string:
  loop feedback is briefly choked (a repick audibly chokes the old note) and
  a band-shaped scrape or thud plays. Wound strings are voiced darker and
  longer than plain ones, following the handling-noise observations in
  Pakarinen, Puputti, and Välimäki's virtual slide guitar work
  ([NIME 2008 companion paper](https://www.nime.org/proceedings/2008/nime2008_049.pdf)).
- **Release phase.** The main release passes two low-pass sections scaled from
  the sounding string period. Their combined modal slope approximates the
  `1/n^2` falloff of triangular released-string displacement without filling
  or replacing a complete delay line. For ordinary sustained pick styles, a
  much smaller raised-cosine pulse of roughly 0.10-1.15 ms before
  velocity/style scaling supplies the pick edge.
  Both components enter the polarisations with a style-dependent split and
  polarity: downstrokes and upstrokes displace the string in opposite
  directions, upstrokes sit slightly closer to the bridge and brighter,
  hammer-ons are wider, darker, and fingered, and slaps retain the sharpest
  edge.
- **Noise controls.** Plectrum, finger, and release noise have independent
  levels; all noise is seeded deterministically per note, so identical MIDI
  renders identical audio. A one-percent trace excites the string while the
  principal short event reaches the pickup-voltage and body paths; this keeps
  handling noise local instead of circulating a false high-frequency pitched
  tone. The Artifacts path uses a separate PRNG, so changing it never changes
  the ordinary plectrum/finger-noise sequence.

Bend styles start the pitch program at the played note (upward bends) or
above it (release bends), hold for about 55 ms, then travel along a
smoothstep curve over the bend-time control, exactly as a fretting finger
pushes or releases a string. Hammer-on/pull-off onto a sounding string
retargets the same loop over about 10 ms without clearing its state, so the
vibration genuinely continues.

## Slap and fret collisions

Bilbao and Torin,
[*Numerical Modeling and Sound Synthesis for Articulated String/Fretboard
Interactions*](https://www.research.ed.ac.uk/en/publications/numerical-modeling-and-sound-synthesis-for-articulated-stringfret/)
(JAES 63(5), 2015), simulate distributed string-fret contact with an
energy-balanced penalty formulation. A full FDTD contact solve is outside
Electry's per-voice budget; the slap style instead opens a velocity-shaped
55-100 ms collision
window in which vertical displacement beyond a velocity-dependent clearance
is soft-limited (a bounded rational excess law) and the clipped excess
re-radiates as deterministic rattle noise. The threshold relaxes as the
window decays, so the buzz dies exactly as the displacement falls below the
frets. This is documented as collision-informed behavior, not a contact
simulation.

## Pickup and coil model

Paiva, Pakarinen, and Välimäki,
[*Acoustics and Modeling of
Pickups*](https://www.researchgate.net/publication/234034228_Acoustics_and_Modeling_of_Pickups)
(JAES 60(10), 2012), describe the pickup as a position-dependent comb, a
finite sensing aperture, a distance-dependent magnetic-flux distortion, and
a resonant electrical circuit. Electry implements each element:

- **Position comb.** Per string and pickup, `y(n) = s(n) - s(n - D)` with
  `D = (d / L_sounding) * period`. The distances from the bridge morph
  between the anchors (bridge pickup 43 mm to 28 mm, neck pickup 155 mm to
  163 mm), and because `D` follows the sounding length, fretting up the neck
  moves the comb exactly as the geometry does. Partials with a node at the
  pickup cancel; the bridge position's weak fundamental sensing is why it
  reads thin and bright, which the regression suite checks as a centroid
  ordering.
- **Aperture.** A fractional rectangular moving average of length `Fs*w/c`,
  where transverse wave speed `c = 2 L f_open`. A cumulative-sum ring gives
  this finite spatial window in O(1) per sample, including fractional window
  length, so its response is the exact aperture sinc with the expected
  `0.443 c / w` -3 dB point. Width morphs from 21.0 mm for the humbucker
  anchor to 4.8 mm for the narrow single coil (window magnitudes consistent with engineering analyses such as
  [Cycfi Research's virtual pickup
  series](https://www.cycfi.com/2014/08/virtual-pickups-part-3/)).
- **Flux nonlinearity.** A bounded polynomial `x (1 + 0.55 x + 0.30 x^2)` on
  the drive-scaled displacement, second-order dominant as in the
  low-frequency pickup distortion measurements of
  [*Measurements and Modeling of the Nonlinear Behavior of a Guitar Pickup
  at Low
  Frequencies*](https://www.researchgate.net/publication/312046898_Measurements_and_Modeling_of_the_Nonlinear_Behavior_of_a_Guitar_Pickup_at_Low_Frequencies);
  the hotter humbucker anchor drives it harder.
- **Induced EMF.** The nonlinear result represents magnetic flux, not output
  voltage. A finite difference multiplied by `Fs / (2 pi 220 Hz)` produces
  the induced EMF and its physically correct frequency weighting; a one-pole
  at up to 16 kHz bounds the differentiator inside the oversampled path. DC
  from the even flux terms therefore vanishes before the output blocker.
- **Coil resonance and tone.** One resonant second-order lowpass per pickup:
  2.0 kHz at Q 1.0 for the loaded humbucker anchor, 6.0 kHz at Q 2.4
  for the loaded single coil, values inside the ranges those circuits
  measure under typical pot and cable loading. Selecting both pickups
  shifts the shared resonance down 7%, and the passive tone control moves
  the resonance toward 600 Hz while damping its Q, so rolling the tone off
  genuinely darkens rather than merely relocating the peak. Humbucker
  output is 1.40x hotter.

These constants are datasheet-plausible anchors, not fitted measurements of
named pickup models.

## Solid body and construction axes

A solid body has low bridge admittance and mainly colours the sound and its
decay rather than radiating it. Electry feeds bridge motion, contact noise,
and slap knocks into four modal resonators whose frequencies, Q, and level
tilt morph along the body wood, size, and shape axes between a
mahogany/maple carved-blank voicing and a lighter ash-slab voicing. Each
all-pole resonator uses its denominator magnitude at the configured modal
frequency,
`g = G |1 + a1 exp(-j w_m) + a2 exp(-j 2 w_m)|`, so `G` is the actual modal
peak rather than a low-frequency-dependent numerator gain.

Electry differentiates structural bridge drive once, processes that velocity
through the double-precision modal bank, applies a 4 kHz guard, and combines
the result with the electrical string sums before the same loaded-coil
circuit. This ordering is equivalent for fixed modes and avoids derivative
spikes while their smoothed parameters move. It never adds undifferentiated
acoustic-body displacement directly to pickup voltage. The mode tables are
geometry-informed estimates and are documented as voicing, not measured data.

Bank and Karjalainen's
[*Passive admittance-based physical modeling of musical instruments in
real time*](https://home.mit.bme.hu/~bank/publist/dafx10adm.pdf) and Maestre,
Scavone, and Smith's
[*Joint Modeling of Bridge Admittance and Body Radiativity for Efficient
Synthesis of String Instrument Sound by Digital Waveguides*](https://caml.music.mcgill.ca/lib/exe/fetch.php?media=publications%3Amaestre_jointmodeling_ieeeaslp_2017.pdf)
show how a positive-real modal bridge admittance can couple string energy to
an instrument body without creating energy. Electry uses a conservative
loss-only slice of that direction, not their complete shared multiport bridge
scattering network. For each body mode, the engine evaluates the positive
real part of normalised modal mobility,
`G=(c*w)^2 / ((w_m^2-w^2)^2 + (c*w)^2)`, with `c=w_m/Q`. `G` stays in
`[0,1]`, peaks at the mode, and is averaged across the note's first six
partials. Body Resonance and construction scale that conductance, which can
only shorten T60; at 0% Body Resonance the additional structural loss is
exactly bypassed. This gives note-dependent material sustain without an
additive feedback loop.

The material axes interpolate between contrasting solid-body references and
default to 0.5. Scale length is independent and extended for Drop-E:

| Axis | 0 | 1 |
| --- | --- | --- |
| Body wood | Mahogany with maple cap, longer-ringing modes | Swamp ash, lighter and brighter-tilted modes |
| Body size | Thick, heavy blank (lower modes) | Thin, light slab (higher modes) |
| Body shape | Carved single-cut mode pattern | Flat slab mode pattern |
| Construction | Set neck and stopbar (more sustain, shallower dead spots) | Bolt-on and through-body (snappier, deeper dead spots) |
| Scale length | 25.5 in (647.7 mm) conventional electric | 28 in (711.2 mm) baritone/8-string, higher tension and lower inharmonicity for the same pitch |
| Pickup type | Wide-aperture humbucker, 2.0 kHz loaded resonance, hotter | Narrow single coil, 6.0 kHz loaded resonance |

Scale length enters the string physics directly: tension, wave speed,
aperture cutoff, inharmonicity, and pickup comb fractions all follow it.

## Cost model

The engine's arithmetic is bounded by what is audible rather than by what is
declared:

- **Culled pickup path.** A pickup whose selector mix has faded to zero is
  skipped in full: two fractional delay reads, the spatial aperture window, the
  flux polynomial, the induced-EMF difference and its guard. Its per-voice
  aperture ring and EMF memory are cleared when the selector brings it back, so
  the 4 ms crossfade starts from a clean path and the regression suite bounds
  the largest sample-to-sample step across the transition.
- **Linked Mono chain.** Mono output is exact dual mono, so only one coil pair,
  DC blocker and halfband decimator runs and the result is mirrored. Opening
  the stereo field copies channel zero's state across at that sample, which is
  exact because both channels have seen identical inputs up to it.
- **Split voicing refresh.** Only the geometry axes (gauge, scale length)
  invalidate the dispersion fit. Damping-only moves - string age, body loss,
  mute damping, palm-mute pressure - reuse the fit and redo only the analytic
  phase compensation, which removes several hundred `atan2` evaluations per
  automated control tick per string.
- **Idle freeze.** Once no string is rendering and the shared body/coil/DC path
  has fallen below -120 dBFS, the engine clears that state, outputs exactly
  zero and stops running the shared chain until the next note. A silent guitar
  track costs essentially nothing, and the float path cannot sit generating
  denormals, which the regression suite checks sample by sample.
- **Constants where they belong.** Every rate-derived smoothing coefficient and
  the per-string magnetic balance are solved in `prepare()` and at note setup.
  They used to be recomputed with `std::pow` inside the per-sample render loop,
  three times per string per sample.
- **Inlined hot primitives.** The fractional delay read, the aperture window and
  the noise generator are defined in the header so they inline into the render
  loop instead of costing a call six times per string per sample.

Measured with an eight-string Drop-E chord held for two seconds, best of five
runs, comparing 1.0 and 1.1 sources built identically: the default
Bridge/Mono configuration went from 0.28x to 0.15x realtime at 96 kHz, the
worst-case Both/Stereo configuration from 0.27x to 0.19x, and an idle engine
from 0.013x to 0.003x. The regression suite prints both eight-string ratios on
every run and asserts that the default configuration is measurably cheaper than
the worst case, so the culling cannot silently regress.

## Why the runtime remains analytic

Bilbao, Russo, Webb, and Ducceschi's 2024
[*Real-Time Guitar Synthesis*](https://www.pure.ed.ac.uk/ws/portalfiles/portal/470239305/BilbaoEtal2024RealTimeGuitarSynthesis.pdf)
shows that energy-stable FDTD guitar strings with simultaneous geometric,
fretboard, fret, and finger nonlinearities can now run in real time using
IEQ/SAV methods and a small low-rank solve. That reference models one
polarisation and explicitly leaves body coupling and radiation for future
work. Replacing Electry's complete eight-string runtime with that solver
would therefore trade away its two-polarisation pickup/body architecture and
its broad host-rate contract. Electry instead combines the efficient
single-delay-loop family with bounded collision laws, exact finite-aperture
pickup sensing, induced EMF, factored dispersion, and loss-only modal bridge
conductance. The choice is architectural, not a claim that the FDTD method is
impractical.

Learning-based string synthesis is advancing quickly — differentiable modal
and waveguide models such as
[*Differentiable Modal Synthesis for Physical Modeling of Planar String
Sound and Motion Simulation*](https://arxiv.org/abs/2407.05516) (NeurIPS
2024) and the string-specific differentiable guitar modeling presented at
recent DAFx conferences fit physical parameters from recordings. Electry has
no licensed capture set of identified instruments across pitch, style,
pickup, and control space, so a learned runtime would add opaque behavior
rather than verified fidelity. The analytic blocks above are auditable,
deterministic, allocation-free in the audio path, and cheap: the complete
eight-string engine remains comfortably faster than real time at 96 kHz on
the CI reference hardware. Differentiable methods remain the obvious future path
for fitting Electry's voicing constants (T60 maps, body modes, pickup
resonances) to calibrated captures.

## Validation boundary

Current automated tests establish: finite, bounded, non-silent output for
all 16 play styles at 44.1-384 kHz; the 2x/1x internal-rate policy, exact
host-to-physical clock timing, and filtered-decimation pitch stability;
exact-silence idle output;
sample-identical renders for identical MIDI (including across engine reuse,
which caught a real aperture-state leak during development); fundamental
accuracy within 8 cents across E1..D6 at three rates; stable allpass bounds
and under-20% low/high dispersion-deficit fit error on the heavy short-scale
Drop-E case; positive bounded modal conductance and exact structural-loss
bypass at 0%; keyswitch latching,
silence, and range gating; measurably distinct attack spectra and levels for
picked, hammered, tapped, muted, chugged, dead, harmonic, tremolo, and slapped
styles; palm-mute decay contraction;
bend start/end/travel targets for all four bend styles; hammer-on
same-string continuation, pitch settling, and click-free transition; slap
collision-window engagement and a sharp-to-true tension glide between 1.5
and 80 cents; bridge-brighter-than-neck centroid ordering; tone-control
high-band reduction; independently audible wood, size, shape, construction,
scale, gauge, body-level, position, hardness, and age endpoints; monotonic
multi-dimensional velocity response plus an exactly flat 0% setting;
deterministic, monotonic, exactly silent-at-zero Artifacts behavior and a
bounded maximum-artifact eight-string strum; bridge-coupled sympathetic ring
of an open string that the played note does not itself produce, exact bypass
and never-configured coupled loops at 0%, coupling determinism, a coupled
string handed back to the player when it is picked, and bounded, ring-out-to-
exact-silence behaviour at maximum coupling across three host rates;
monotonic palm-mute decay contraction, an exact no-op at zero pressure, an
in-tune heavily muted string, the solved loop coefficient actually moving, and
CC 2 pressure including hostile input; strum travel offsets in physical string
order, an undelayed leading string, a lower stacked chord peak, a fresh stroke
outside the chord window, and no premature retirement of a delayed string;
vibrato depth scaling the loop-delay excursion with an exactly still zero
setting; pluck position following the fretted sounding length by 2^(fret/12);
fretboard geometry, meter ballistics, standing-wave shape, colour knee and a
lossless packed audio-to-editor round trip; per-string display readout naming
the right string, fret, note and articulation; selector-driven pickup culling,
click-free restoration of a culled pickup, Mono channel linking and a
click-free stereo-field opening; exact digital silence from an untouched
engine, a subnormal-free ring-out that reaches exact zero, and a clean wake
from the frozen state; contrasting construction
endpoints that both stay in tune; plectrum contact noise in the pre-attack
window; release noise that appears only after note-off; eight-string
polyphony with open-position chord mapping, repick reuse, and stealing;
pitch-wheel travel and sustain-pedal hold; hostile parameter and performance
input safety; and a portable CPU ceiling with the eight-string render ratio
printed on every run in worst-case Stereo, maximum Body Resonance, and maximum
Artifacts mode. Mono is checked sample-for-sample dual mono; Stereo tests pin
physical low/high string orientation, coherent fold-down, bounded side level,
energy balance, determinism, and opposite string endpoints. The plug-in suite
additionally pins the 31-parameter
contract, formatted values, state round-trips including a pre-1.1 session that
picks up the new defaults, bus layout, sample-accurate
note starts, MIDI controller behavior (sustain, all-sound-off,
all-notes-off), UI articulation triggering, panic, output-gain and APVTS
output-field effects, two visible non-overlapping mode buttons,
the sympathetic, palm-mute (parameter and CC 2) and strum-spread controls
reaching the rendered audio, offscreen editor rendering including the live
fretboard's bounds, and prepare/release cycles at three rates.

Those engineering tests do not replace hardware validation. A stronger claim
about any named instrument would additionally require documented capture
chains, waveform and decay-map error analysis across the fretboard and
control space, pickup response fits against measured coils, and
level-matched blind listening with enough trials to report uncertainty.
Electry 1.0 therefore makes a deliberately testable statement: its string,
interaction, and pickup structures come from the published models cited
above; its constants are labeled as voicing wherever they are not
literature-derived; and every remaining boundary is stated rather than
implied.
