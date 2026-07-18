# Electry physical-modeling research and implementation contract

Electry 1.0 is a white-box physically modeled dry electric guitar with named
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
| String core | Karjalainen, Välimäki, and Tolonen's single-delay-loop condensation of digital waveguides | Six independent strings, each with two transverse-polarisation single-delay-loop waveguides, third-order Lagrange fractional reads, and a contractive bridge coupling matrix | The published SDL string family with two coupled polarisations per string; not a bidirectional multi-rail scattering simulation |
| Two-stage decay and beating | Two-polarisation string behavior described in the same plucked-string literature | The polarisation parallel to the body carries a 1.7x longer decay target and a sub-cent detune, so the mixed output beats slowly and decays in two stages | A qualitative reproduction of the documented mechanism with voiced constants; not calibrated polarisation data from a measured instrument |
| Stiffness dispersion | Stiff-string inharmonicity `B = pi^3 E d^4 / (64 T L^2)` (Fletcher and Rossing) and allpass dispersion-filter design practice (Rauhala and Välimäki; Abel and Smith) | A per-note `B` computed from string diameter, wound-core fraction, scale length, and tension, mapped onto two first-order loop allpasses whose solved coefficient reproduces the stiffness delay deficit at a reference partial, with exact phase compensation at the fundamental | A physically derived, bounded low-order dispersion approximation; not the very high-order allpass designs that match piano-class inharmonicity across the full band |
| Loop damping and tuning | Decay-time-targeted loop-filter design from the plucked-string literature | Per-string, per-fret one-pole loop filters solved by bisection from independent T60 targets at the fundamental and a high reference frequency, with all loop-filter phase delays compensated analytically at the fundamental | Decay-targeted loop design with exact fundamental tuning (regression bound: under 8 cents across E2..D6 at 44.1-96 kHz); not per-partial measured decay matching |
| Dead spots | Fleischer's electric-guitar dead-spot studies relating neck conductance to decay time | A per-string fret-position Gaussian that locally shortens decay, deepened by the bolt-on end of the construction axis | The documented mechanism direction with voiced positions and depths; not measured conductance maps of specific instruments |
| Tension modulation | Tolonen, Välimäki, and Karjalainen's tension-modulation nonlinearity | A string-energy envelope drives a bounded shortening of the loop delay, producing the attack pitch glide that relaxes over hundreds of milliseconds; slaps deepen it | The published phenomenon in its energy-envelope shortcut form; not the exact elongation integral or a time-varying-fraction-delay implementation |
| Plectrum and finger excitation | Plectrum and touch interaction modeling by Germain and Evangelista and by Evangelista and Eckerholm | A three-phase excitation (contact choke and scrape, sub-millisecond shaped release pulse, decaying tail) whose polarity, angle-dependent polarisation split, width, spectrum, and comb position differ per play style; the pluck-point comb is realised exactly as the excitation's second travelling-wave image | Behaviorally faithful player interaction in signal form; not the beam-mechanics plectrum profile or force-based finger contact solvers of the cited papers |
| Fret collisions (slap) | Bilbao and Torin's energy-balanced string/fretboard collision modeling | A decaying collision window whose soft limit clips vertical displacement against a velocity-dependent threshold and re-radiates deterministic rattle noise | Collision-informed slap behavior in a bounded, stable form; not an FDTD distributed-contact simulation |
| Hammer-on and pull-off | Touch/legato interaction models from Evangelista and Eckerholm | Keyswitched legato: a sounding string within reach retargets its delay over about 10 ms while the loop state is preserved, with a soft finger excitation and no plectrum noise | Continuous-state legato with fingered attacks; not a distributed finger-force model |
| Pickups | Paiva, Pakarinen, and Välimäki's pickup acoustics and modeling; low-frequency pickup nonlinearity measurements (Novak et al.); engineering aperture analyses | Per-string pickup-position combs whose delay follows the sounding length each fret, a wave-speed-scaled aperture lowpass (wide humbucker vs narrow single-coil window), a bounded distance-flux polynomial nonlinearity, and a loaded RLC-style resonant second-order coil filter with tone-pot behavior | The published pickup signal structure (position comb, aperture average, flux distortion, electrical resonance) with datasheet-plausible constants; not a magnetic finite-element or capture-fitted model of named pickups |
| Solid body | Solid-body admittance and dead-spot literature; geometric estimates | Four modal resonators fed from the bridge, morphed by the body wood, size, and shape axes; body colour is mixed into the coil inputs and receives knock energy from slaps and contact noise | Geometry-informed structural colour; the mode tables are voicing estimates, not measured modal data of a Les Paul or Telecaster |
| Guitar-model axes | Documented anchor geometry: 24.75 in vs 25.5 in scales, humbucker vs single-coil construction, set-neck vs bolt-on | Every axis (body wood, size, shape, construction, scale length, pickup type) interpolates between a Gibson Les Paul-style anchor at 0 and a Fender Telecaster-style anchor at 1, defaulting between the two | Parametrized placement between two reference styles; not a licensed or capture-verified reproduction of either trademarked instrument |
| Play noise | Handling-noise observations in the virtual slide guitar work of Pakarinen, Puputti, and Välimäki | Deterministic seeded plectrum scrape, finger contact, release damping noise, and slap body knock, band-shaped per string (wound vs plain) and injected through the same string and body paths as the tone | Procedural, deterministic contact noise consistent with the documented mechanisms; not convolved recordings or measured contact-noise spectra |

## Implemented signal path

The authoritative implementation is `Source/DSP/ElectryEngine.cpp`:

1. MIDI notes 24..32 are latching keyswitches that select the play style:
   downstroke, upstroke, hammer-on/pull-off, palm mute, bend to +1 or +2
   semitones, release bends from +1 or +2 semitones onto the played note, and
   slap. Notes 40..86 (open E2 to fret 22 on the high E) are playable; a
   deterministic allocator maps each note to one of six string voices,
   preferring a repick of an already-sounding note, then the hammer-on
   continuation of the nearest sounding string, then the free string with the
   lowest fret (which reproduces open-position chord shapes), and finally an
   oldest-first steal.
2. Each string voice runs two single-delay-loop waveguides (vertical and
   horizontal polarisation). Each loop has a third-order Lagrange fractional
   read, two first-order dispersion allpasses solved from the string's
   physical inharmonicity, a one-pole damping filter solved from T60 targets,
   and a release/mute gain ramp. A contractive bridge matrix exchanges a
   small amount of energy between the polarisations.
3. The loop delay compensates the exact phase delay of every loop filter at
   the sounding fundamental. Bends, hammer-on glides, the pitch wheel, and
   tension modulation move the delay target; a short smoother keeps the
   motion click-free. There is deliberately no DC filter inside the loop: a
   fixed-corner blocker's steep phase lead near a low fundamental would
   detune the upper partials against the compensated fundamental, and the
   pickup position comb already rejects DC exactly.
4. Excitation runs in phases: a contact stage that chokes the ringing string
   slightly and plays band-shaped scrape or finger noise, then a
   sub-millisecond raised-cosine release pulse shaped by pick hardness and
   play style, injected into both polarisations with a style-dependent split
   and polarity. The pluck-position comb is realised by scattering the same
   excitation with opposite sign one comb delay behind the write head — the
   second travelling-wave image of the excitation point.
5. Slap opens a decaying fret-collision window that soft-limits vertical
   displacement against a velocity-dependent clearance and adds deterministic
   rattle proportional to the clipped excess, plus a thumb knock into the
   body. A string-energy envelope shortens the loop delay (tension
   modulation), so hard attacks start audibly sharp and relax; slaps deepen
   the effect.
6. Each pickup reads every string's displacement as the freshly written
   bridge sample minus a fractional read at the pickup delay; that delay
   follows the sounding length, so fretting up the neck moves the comb
   exactly as the geometry does. The tap passes an aperture one-pole scaled
   by the string's wave speed and the selected coil's magnetic window, then a
   bounded distance-flux polynomial. String sums (plus body colour) drive one
   resonant second-order coil filter per pickup, morphing humbucker to
   single-coil resonance and Q, loaded further by the passive tone control;
   the selector fades neck, both (with the paired-coil resonance shift), or
   bridge.
7. Four modal body resonators fed from bridge motion and contact/knock noise
   add solid-body colour into the coil inputs. The summed output passes a
   5 Hz DC blocker and a bounded soft guard. The result is dual-mono: a dry
   electric guitar signal with no amplifier, cabinet, or effect processing.

## Dual-polarisation string loops

Karjalainen, Välimäki, and Tolonen,
[*Plucked-String Models: From the Karplus-Strong Algorithm to Digital
Waveguides and Beyond*](http://users.spa.aalto.fi/vpv/publications/cmj98.pdf)
(Computer Music Journal 22(3), 1998), show that a bidirectional waveguide
string with consolidated losses reduces exactly to a single delay loop, and
that output combs equivalent to pluck and pickup positions can be factored
out of the loop. Electry uses that condensation: per polarisation, one
fractional delay line, one damping filter, and one dispersion pair; the pluck
comb is realised as the excitation's second travelling-wave image written
directly into the line, and each pickup tap is the current bridge sample
minus a delayed read.

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
spirit, at deliberately low order: two identical first-order allpasses whose
coefficient is solved by bisection so their extra low-frequency phase delay
reproduces the delay deficit stiffness causes at a reference partial
(`min(16, 0.3 fs / f0)`). Electric-guitar `B` values (about `1e-5` for a
plain high E to about `1.5e-4` for a wound low E at these scales) are small
enough for this bounded approximation; the solved coefficient is clamped to
`[-0.55, 0]`, and the deficit is therefore also bounded on the lowest wound
strings. Electry does not claim the full-band partial-frequency accuracy of
the cited high-order designs.

Tuning is exact at the fundamental: the loop delay subtracts the analytic
phase delay of the damping one-pole and both allpasses at `f0`, evaluated
from their closed-form responses. The regression suite bounds the sounding
fundamental within 8 cents of equal temperament across E2..D6 at 44.1, 48,
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
- **Release phase.** A raised-cosine pulse of 0.16-0.85 ms (hardness
  dependent, style scaled) passes a hardness-mapped one-pole and enters both
  polarisations with a style-dependent split and polarity: downstrokes and
  upstrokes displace the string in opposite directions, upstrokes sit
  slightly closer to the bridge and brighter, hammer-ons are wider, darker,
  and fingered, slaps are the narrowest and brightest.
- **Noise controls.** Plectrum, finger, and release noise have independent
  levels; all noise is seeded deterministically per note, so identical MIDI
  renders identical audio. The noise feeds the string loops and the body,
  never a parallel dry bus alone.

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
Electry's per-voice budget; the slap style instead opens an 85 ms collision
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
  between the anchors (bridge pickup 46 mm to 31 mm, neck pickup 155 mm to
  163 mm), and because `D` follows the sounding length, fretting up the neck
  moves the comb exactly as the geometry does. Partials with a node at the
  pickup cancel; the bridge position's weak fundamental sensing is why it
  reads thin and bright, which the regression suite checks as a centroid
  ordering.
- **Aperture.** A one-pole lowpass whose cutoff is `0.443 c / w`: the
  transverse wave speed `c = 2 L f_open` over the magnetic window width,
  17.5 mm for the humbucker anchor and 6.4 mm for the single-coil anchor
  (window magnitudes consistent with engineering analyses such as
  [Cycfi Research's virtual pickup
  series](https://www.cycfi.com/2014/08/virtual-pickups-part-3/)).
- **Flux nonlinearity.** A bounded polynomial `x (1 + 0.55 x + 0.30 x^2)` on
  the drive-scaled displacement, second-order dominant as in the
  low-frequency pickup distortion measurements of
  [*Measurements and Modeling of the Nonlinear Behavior of a Guitar Pickup
  at Low
  Frequencies*](https://www.researchgate.net/publication/312046898_Measurements_and_Modeling_of_the_Nonlinear_Behavior_of_a_Guitar_Pickup_at_Low_Frequencies);
  the hotter humbucker anchor drives it harder. DC produced by the even
  terms is removed by the output blocker.
- **Coil resonance and tone.** One resonant second-order lowpass per pickup:
  2.45 kHz at Q 1.35 for the loaded humbucker anchor, 4.05 kHz at Q 1.95
  for the loaded single coil, values inside the ranges those circuits
  measure under typical pot and cable loading. Selecting both pickups
  shifts the shared resonance down 7%, and the passive tone control moves
  the resonance toward 780 Hz while damping its Q, so rolling the tone off
  genuinely darkens rather than merely relocating the peak. Humbucker
  output is 1.32x hotter.

These constants are datasheet-plausible anchors, not fitted measurements of
named pickup models.

## Solid body and the Les Paul-Telecaster axes

A solid body has low bridge admittance and mainly colours the sound and its
decay rather than radiating it. Electry feeds bridge motion, contact noise,
and slap knocks into four modal resonators whose frequencies, Q, and level
tilt morph along the body wood, size, and shape axes between a
mahogany/maple carved-blank voicing and a lighter ash-slab voicing; the body
signal joins the coil inputs, as body vibration reaches the pickups through
string-pickup distance modulation. The mode tables are geometry-informed
estimates and are documented as voicing, not measured modal data.

Every guitar-model axis interpolates between the two anchor instruments and
defaults to 0.5, placing the default instrument between a Les Paul-style and
a Telecaster-style build, as the instrument contract requires:

| Axis | 0 (Les Paul-style anchor) | 1 (Telecaster-style anchor) |
| --- | --- | --- |
| Body wood | Mahogany with maple cap, longer-ringing modes | Swamp ash, lighter and brighter-tilted modes |
| Body size | Thick, heavy blank (lower modes) | Thin, light slab (higher modes) |
| Body shape | Carved single-cut mode pattern | Flat slab mode pattern |
| Construction | Set neck and stopbar (more sustain, shallower dead spots) | Bolt-on and through-body (snappier, deeper dead spots) |
| Scale length | 24.75 in (628.65 mm) | 25.5 in (647.7 mm), higher tension and slightly lower inharmonicity for the same pitch |
| Pickup type | Wide-aperture humbucker, 2.45 kHz loaded resonance, hotter | Narrow single coil, 4.05 kHz loaded resonance |

Scale length enters the string physics directly: tension, wave speed,
aperture cutoff, inharmonicity, and pickup comb fractions all follow it.

## Why the runtime remains analytic

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
six-string engine renders at about 5% of real time at 96 kHz on the CI
reference hardware. Differentiable methods remain the obvious future path
for fitting Electry's voicing constants (T60 maps, body modes, pickup
resonances) to calibrated captures.

## Validation boundary

Current automated tests establish: finite, bounded, non-silent output for
all nine play styles at 44.1-192 kHz; exact-silence idle output;
sample-identical renders for identical MIDI (including across engine reuse,
which caught a real aperture-state leak during development); fundamental
accuracy within 8 cents across E2..D6 at three rates; keyswitch latching,
silence, and range gating; measurably distinct attack spectra and levels for
picked, hammered, muted, and slapped styles; palm-mute decay contraction;
bend start/end/travel targets for all four bend styles; hammer-on
same-string continuation, pitch settling, and click-free transition; slap
collision-window engagement and a sharp-to-true tension glide between 1.5
and 80 cents; bridge-brighter-than-neck centroid ordering; tone-control
high-band reduction; audibly different Les Paul-style and Telecaster-style
endpoints that both stay in tune; plectrum contact noise in the pre-attack
window; release noise that appears only after note-off; six-string
polyphony with open-position chord mapping, repick reuse, and stealing;
pitch-wheel travel and sustain-pedal hold; hostile parameter and performance
input safety; and a portable CPU ceiling with the six-string render ratio
printed on every run. The plug-in suite additionally pins the 20-parameter
contract, formatted values, state round-trips, bus layout, sample-accurate
note starts, MIDI controller behavior (sustain, all-sound-off,
all-notes-off), UI articulation triggering, panic, output-gain effect,
offscreen editor rendering, and prepare/release cycles at three rates.

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
