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
| Loop damping and tuning | Decay-time-targeted loop-filter design from the plucked-string literature; a dry electric low-E reference recording for the targets themselves | Per-string, per-fret one-pole loop filters solved by bisection from independent T60 targets at the fundamental and a high reference frequency, with all loop-filter phase delays compensated analytically at the fundamental. The wound strings' fundamental targets are tens of seconds and their high-frequency ratio two orders of magnitude smaller, following the reference | Decay-targeted loop design with exact fundamental tuning (regression bound: under 8 cents across E1..D6 at tested host rates through 384 kHz), whose fundamental and high-frequency targets are calibrated against one reference recording; not per-partial measured decay matching across a fretboard, and not a model of the reference instrument |
| Dead spots | Fleischer's electric-guitar dead-spot studies relating neck conductance to decay time | A per-string fret-position Gaussian that locally shortens decay, deepened by the bolt-on end of the construction axis | The documented mechanism direction with voiced positions and depths; not measured conductance maps of specific instruments |
| Tension modulation | Tolonen, Välimäki, and Karjalainen's tension-modulation nonlinearity | A string-energy envelope drives a bounded shortening of the loop delay, producing the attack pitch glide that relaxes over hundreds of milliseconds; slaps deepen it | The published phenomenon in its energy-envelope shortcut form; not the exact elongation integral or a time-varying-fraction-delay implementation |
| Plectrum and finger excitation | Plectrum and touch interaction modeling by Germain and Evangelista and by Evangelista and Eckerholm | A three-phase excitation combines contact retention and scrape, a principal string-period-scaled two-pole modal release that approximates triangular pluck displacement, one further release pole whose corner follows the square root of the string's open frequency (a heavier string leaves the pick more slowly) with its own attenuation at the fundamental divided back out, and a normally much smaller broadband pick-edge transient for sustained pick styles; the release window is asymmetric (a slow load and a fast slip, both smoothsteps, at constant area) and its reflected image is distributed over a hardness-dependent contact patch of 0.5 to 1.5 mm; delay-line projection is level-calibrated to open E4 so equal effort remains usable on E1, while polarity, polarisation split, spectrum, and comb position differ per style | A realtime modal approximation to released-string displacement plus a separate pick edge, a distributed contact width, and bounded register calibration; not an exact delay-line initial-condition solve, beam-mechanics plectrum profile, or force-based finger contact solver |
| Fret collisions (slap) | Bilbao and Torin's energy-balanced string/fretboard collision modeling | A decaying collision window whose soft limit clips vertical displacement against a velocity-dependent threshold and re-radiates deterministic rattle noise | Collision-informed slap behavior in a bounded, stable form; not an FDTD distributed-contact simulation |
| Hammer-on and pull-off | Touch/legato interaction models from Evangelista and Eckerholm | Keyswitched legato: a sounding string within reach retargets its delay over about 10 ms while the loop state is preserved, with a soft finger excitation and no plectrum noise | Continuous-state legato with fingered attacks; not a distributed finger-force model |
| Pickups | Paiva, Pakarinen, and Välimäki's pickup acoustics and modeling; low-frequency pickup nonlinearity measurements (Novak et al.); engineering aperture analyses | Per-string pickup-position combs follow each fret, with the delayed tap weighted 0.60 so the null is 12 dB deep rather than infinite, as a real aperture, two-coil sum and three-dimensional field never cancel exactly; an O(1) fractional rectangular moving average gives the finite aperture's exact sinc response; bounded flux nonlinearity plus shallow string-mass/pole balance is differentiated into induced EMF, guarded ultrasonically, then passed through the loaded coil/tone circuit | The published pickup signal structure (position comb of measured rather than ideal null depth, finite aperture, nonlinear flux, induced voltage, electrical resonance) with datasheet-plausible level calibration; not a magnetic finite-element, per-coil, or capture-fitted model of named pickups |
| Solid body | Solid-body bridge-admittance and dead-spot literature; geometric estimates | Structural bridge displacement is differentiated before four double-precision, peak-normalised modal resonators and a 4 kHz guard, producing body-induced voltage before the loaded pickup coils; positive real modal conductance across each note's first six partials can only shorten loop T60 | Geometry-informed structural pickup voltage plus passive mode-dependent energy extraction; not undifferentiated acoustic body displacement mixed into pickup voltage, and the mode tables remain voicing estimates rather than measured admittance data |
| Construction controls | Solid-body material/geometry contrasts, humbucker vs single-coil construction, set-neck vs bolt-on, and modern extended-range scale practice | Wood, size, shape, construction, and pickup type interpolate between contrasting reference voicings; scale length spans 25.5 to 28 inches for Drop-E | Parametrized construction and extended-range voicing; not a licensed or capture-verified reproduction of a named instrument |
| Play noise | Handling-noise observations in the virtual slide guitar work of Pakarinen, Puputti, and Välimäki | Deterministic seeded plectrum scrape, finger contact, release damping noise, and slap body knock, band-shaped per string (wound vs plain) and split between a one-percent string trace and local pickup/body paths | Procedural, deterministic contact noise consistent with the documented mechanisms; not convolved recordings or measured contact-noise spectra |
| Sympathetic string coupling | Bank and Karjalainen's passive admittance modeling and the sympathetic-string literature | The plucked strings' bridge force drives a one-sample-delayed bus; every string that is not being fingered runs its own single-polarisation waveguide at its open pitch, with its own T60-derived loop filter, exact fundamental phase compensation and bridge pickup tap. Only played voices write to the bus and only idle voices read it | A one-directional (loss-only from the driver's point of view) slice of bridge coupling, provably acyclic and therefore unconditionally stable; not a shared multiport bridge scattering junction with mutual re-radiation |
| Bridge-hand damping | Palm-muting practice, the same decay-targeted loop design, and dry muted power-chord reference recordings for the depths | The hand is an absorber whose loss adds to the string's own in parallel, so decay rates sum at each fitted frequency independently; the raw hand rate is multiplied by three at the high reference and divided by twenty-two at the fundamental, an effective 66:1 ratio between the two fitted points, because a contact near the bridge removes far more energy from high modes than from a fundamental that barely moves there; a relief that large only works paired with a band of loss centred on five times the fundamental, which removes the harmonics the longer tail would otherwise let ring - alone, each of the two is worse than neither; the Muted and Chug keyswitches and the continuous pressure are one absorber at different depths and combine the same way, re-solving the same loop filters and the analytic phase compensation so the note stays in tune; the coupled strings are damped and starved with it | Progressive contact damping as an additive loss with reference-calibrated depths and a bounded, conservative frequency tilt, applied identically to every play style; not a distributed hand/string contact solve or a resolved mode-shape weighting |
| Strum travel | Ordinary plectrum kinematics | Note-ons inside a 35 ms window are treated as one stroke; the first string fixes the edge the pick starts from and every further string's excitation is delayed by the travel time per string crossed | Constant-velocity pick travel across the string plane; not a model of pick angle, chord voicing, or the player's hand position |
| Controllable artifacts | The same touch/collision literature plus bridge-hardware behavior | An exactly bypassable deterministic path combines a bridge-hardware modal bank driven through the selected pickup mix, partial non-slap fret contact, and per-string saddle rattle, all driven by played energy. It is mechanical hardware noise, distinct from the sympathetic string coupling above | Plausible procedural imperfection with bounded feed-forward resonators; not measured hardware-noise statistics |
| Audible-work culling | Standard realtime-DSP practice | A pickup faded out by the selector is skipped entirely; Mono runs one shared coil/DC/decimation chain and mirrors it; damping-only control moves reuse the existing dispersion fit; the whole engine freezes to exact zero once nothing vibrates and the shared path is below -120 dBFS | Removal of inaudible arithmetic with the audible result unchanged; not a quality/latency trade |
| Oversampling | Standard nonlinear-audio antialiasing practice | The complete physical, body, collision, and nonlinear pickup path runs at 2x for host rates through 96 kHz, followed by a fixed 63-tap halfband FIR; higher-rate hosts run 1x | Genuine internal oversampling and filtered decimation, not a quality label applied to a native-rate nonlinear stage |
| Output field | Phase-coherent divided/hex pickup practice | Mono is the conventional summed DI. Stereo weights each modeled string by its physical lateral position, keeps shared body modes centered, uses linked output limiting and independent matched decimation, and folds coherently to mono | A virtual divided-pickup string field with no time or phase widening; not room, amplifier, cabinet, chorus, or acoustic stereo |
| Amplifier and cabinet | Pakarinen and Yeh's review of vacuum-tube amplifier modeling; standard antialiasing practice for cascaded nonlinear stages; sealed-guitar-cabinet response measurements; extended-range metal rhythm practice for the voicing | Two cascaded smooth triode ceilings driven off a standing grid bias with a level-tracking bias drift and an interstage Miller roll-off, a tight input coupling network, and a five-section cabinet (box high-pass, low-mid thump, scooped mid, presence peak, fourth-order roll-off), all inside a 4x oversampled domain reached through Kaiser-windowed halfband stages designed at prepare time | Structurally motivated static-nonlinearity amplifier voicing with genuine oversampling and a filter-modelled cabinet; not a circuit-solved (Wave Digital or nodal state-space) amplifier, a measured impulse response, or a model of any named amplifier or speaker |

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

A bridge hand loads a string; it does not stop it. Electry therefore models it
as a loss that runs *in parallel* with the string's own, so the decay rates add
and the decay times combine reciprocally:

    1 / T60' = 1 / T60_string + 1 / T60_hand

applied independently at the fundamental and at the high reference frequency.
The hand dominates wherever it is tighter than the string and vanishes wherever
it is not, which is the whole point: a muted note keeps a body instead of having
its top end scaled into nothing.

The hand's own rate is not equal at those two points. The heel rests near the
bridge, and mode `n`'s displacement under it goes as `sin(n pi x / L)`, so the
energy the contact can remove rises steeply with `n` while the fundamental,
which barely moves that close to the bridge, is left comparatively free. The
high reference therefore takes the hand's rate multiplied by three. Treating the
hand as a genuinely broadband absorber - the same rate at both points - damped
the fundamental as hard as the top end, and that is what made a palm mute read
as a thin, cut-off pick rather than a heavy chug. The mode-shape ratio between
the fundamental and the 3.6 kHz point is nearly two orders of magnitude and
oscillates once `n pi x / L` passes its first quarter period, so the factor of
three is a deliberately conservative monotone stand-in rather than a resolved
mode shape; it is fitted against the muted reference power chords, where it
recovered 5.4 dB in the 60-85 Hz band and removed 2.4 dB of the 1.4-2.7 kHz
excess.

The dead-note choke is tracked as a separate rate and stays broadband. It is
the fretting hand somewhere up the neck rather than the heel resting by the
bridge, so the mode-shape argument above does not describe it. The two rates are
added rather than switched between, so a dead note played under palm-mute
pressure carries both contacts with each one's own frequency behaviour. In
isolation the distinction turns out to be inaudible - a dead note's 32 ms decay
means what is heard is the percussion transient rather than the loop's ring, and
measured high-frequency energy moves by 0.04 percentage points either way - but
the model now says what it means.

That distinction is not cosmetic. The previous model applied the hand as a
minimum on the fundamental's T60 and then multiplied *that* result by the
string's high-frequency ratio. With the wound strings' corrected ratio - around
0.035 - a half-second mute target implied a seventeen millisecond
high-frequency target, and a muted power chord collapsed 36 dB inside its first
25 ms. Measured against dry muted power-chord reference recordings, a real short
muted chord falls 2 dB over that span and takes about half a second to reach
-40 dB; a looser one holds a low tail for seconds. The old behaviour was an
impulse where the reference is a note.

The Muted and Chug keyswitches, the Dead Note choke and the continuous Palm Mute
pressure are all the same absorber at different depths, and they combine in
parallel with each other as well. Their reference-derived targets are 2.60 s to
0.32 s across Mute Damp for the Muted style, 1.40 s to 0.20 s for the firmer
Chug, and 4.0 s to 0.080 s across the continuous pressure; the pressure also
multiplies the high-frequency ratio by `1 - 0.38 p`, because the heel of the hand
is a soft, lossy contact. Zero pressure leaves `T60_hand` at zero and the
parallel combination is skipped outright, so an unmuted string is bit-for-bit
what it would be without the feature.

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
  much smaller pulse of roughly 0.10-1.15 ms before velocity/style scaling
  supplies the pick edge.
  Both components enter the polarisations with a style-dependent split and
  polarity: downstrokes and upstrokes displace the string in opposite
  directions, upstrokes sit slightly closer to the bridge and brighter,
  hammer-ons are wider, darker, and fingered, and slaps retain the sharpest
  edge.
- **Release geometry.** The release window itself is asymmetric, because a
  plectrum is: it draws the string aside over most of the contact and then
  slips off it in a fraction of that time, and a stiffer pick lets go later and
  more abruptly (the slip point runs from 62% to 82% of the window with pick
  hardness). Both halves are smoothsteps, so the product is continuous with a
  continuous derivative and its area over the window is exactly one half
  whatever the slip point is — the same area the symmetric raised cosine this
  replaced had. The asymmetry therefore changes the attack's spectrum without
  changing how hard the note lands. Measured over the first 85 ms of an open
  E1, the difference from the symmetric window is small (about 5 Hz of spectral
  centroid at constant level), because the two string-period-scaled modal
  sections downstream dominate the released displacement; the change is made
  because the physical shape is known, not because it is a large effect.
- **Release duration.** The two modal sections give the ideal 1/n^2 spectrum of a
  point pluck of a perfectly flexible string, which the pickup's induced-EMF
  differentiation turns into a 1/n voltage spectrum. A real plectrum does not
  leave a heavy wound string as quickly as a light plain one, and the duration of
  that release low-passes what enters the string. One further pole supplies it,
  with a corner following the square root of the string's own open frequency and
  scaled by pick hardness, player effort, string age and the style's own
  brightness factor. Its attenuation at the sounding fundamental is divided back
  out, so the release governs the excitation's spectrum rather than how hard the
  note lands. The corner itself is calibrated against a reference recording
  rather than derived; the voicing section below records the measurement.
- **Contact width.** A plectrum touches the string over a patch rather than at
  a point, so the reflected image of the excitation is distributed with a
  1/4, 1/2, 1/4 kernel spanning a hardness-dependent 0.5 mm (stiff and sharp)
  to 1.5 mm (soft and rounded) of contact, mapped into delay-line samples
  through the same sounding-length geometry the pluck comb uses. On an open
  Drop-E eighth string that is a little over one internal sample and on the
  top string a small fraction of one, so the comb notches wash out with
  frequency as a real finite contact does rather than staying razor sharp to
  Nyquist. The weights sum to one, so a zero width reduces exactly to the
  previous single-point image.
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

- **Position comb.** Per string and pickup, `y(n) = s(n) - b * s(n - D)` with
  `D = (d / L_sounding) * period`. The distances from the bridge morph
  between the anchors (bridge pickup 43 mm to 28 mm, neck pickup 155 mm to
  163 mm), and because `D` follows the sounding length, fretting up the neck
  moves the comb exactly as the geometry does. Partials with a node at the
  pickup are attenuated; the bridge position's weak fundamental sensing is
  why it reads thin and bright, which the regression suite checks as a
  centroid ordering.

  The delayed tap carries weight `b = 0.60` rather than one. Equal weights
  would put an exact zero at DC and an infinitely deep null at every multiple
  of `c / 2d`, which is what a point sensor on an ideal one-dimensional string
  would give. A real pickup does not cancel exactly: it senses through a
  finite aperture, a humbucker sums two coils at two different distances, and
  the field is three-dimensional. Measured responses notch by roughly 6 to
  15 dB, and `b = 0.60` places the null at 12 dB, mid-range of that. The
  perfect cancellation was costing the fundamental most: against the dry
  reference recordings this recovered 4.7 dB in the 60-85 Hz band on an open
  low E and 5.4 dB on a muted power chord, the hollow, bodyless low register
  the references do not have. Because the comb no longer rejects DC exactly,
  the 5 Hz output blocker is now the only stage removing an offset rather
  than a second line of defence.

  The same weight applies to the bridge-coupled sympathetic strings, which are
  sensed by the same physical pickup and so cancel no better there than on a
  played string.

  Modelling the humbucker's two coils explicitly was tried as the alternative,
  summing two position combs 19 mm apart. The arithmetic is attractive - two
  combs sum to `2 - 2b cos(w Δ/c) e^{-j w 2 d̄ / c}`, whose depth modulation
  vanishes at `c / 4Δ`, almost exactly where the single-tap model's null sits -
  but measured against the same references it scored no better than the finite
  weight above, with or without narrowing the aperture to a single coil, at the
  cost of two extra fractional reads per string. It is not in the model.
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

The material axes interpolate between contrasting solid-body references. They
default to 0 and scale length to 0.85, described below under the default
voicing; the midpoints they replaced are recorded there together with the
control range the change cost. Scale length is independent and extended for
Drop-E:

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

## Amplifier, cabinet, and time effects

`Source/DSP/ElectryFx.cpp` holds the five FX-panel controls. It is downstream of
the instrument and is not part of the physical model, but it is where most of
what a listener calls "the metal guitar sound" is actually produced, so it is
held to the same explicitness as the string engine — and, like the engine, it is
JUCE-free and regression tested on every platform.

Pakarinen and Yeh,
[*A Review of Digital Techniques for Modeling Vacuum-Tube Guitar
Amplifiers*](https://direct.mit.edu/comj/article/33/2/85/94374/A-Review-of-Digital-Techniques-for-Modeling-Vacuum)
(Computer Music Journal 33(2), 2009), survey the field from static waveshapers
through circuit-solved methods, and are explicit about the antialiasing problem:
a cascaded gain stage generates harmonics far above the audio band, and at host
rate those fold straight back into it. Electry takes the static-nonlinearity
route with genuine oversampling rather than the circuit-solved route:

- **Oversampling.** Both clipping blocks run at 4x, reached through two
  cascaded halfband stages. The kernels are Kaiser-windowed ideal halfband
  responses designed in `prepare()` rather than transcribed constants, with the
  odd taps normalised so the kernel sums to exactly one; the regression suite
  measures unity DC gain, the -6 dB halfband symmetry point, passband ripple
  under 0.05 dB to 0.15 of the stage rate, and better than 50 dB of stopband
  rejection from 0.35 of it. Above 96 kHz one stage is dropped and above
  192 kHz both are, on the same grounds as the engine's own 2x policy: the
  point is a fixed absolute bandwidth for the stages, and a host already
  running that fast supplies it. Engaged, the chain adds 17.25 host samples of
  fixed group delay at 4x and 11.5 at 2x. With both gain controls at zero the
  block is skipped outright and adds neither cost nor delay.
- **Measured effect.** Non-harmonic energy in the output of a steady tone,
  relative to the energy that legitimately belongs to the harmonic series, is
  the direct measure of folded intermodulation. Against the previous host-rate
  chain on the same probes: -128 dB versus -128 dB for the pedal on a quiet
  signal, -86 versus -44 dB for the pedal at full drive, -86 versus -39 dB for
  the amplifier at full drive, and -102 versus -28 dB for the pedal stacked
  into the amplifier. The regression suite asserts that all four stay below
  -60 dB.
- **Smooth transfer functions.** Every nonlinearity is bounded and infinitely
  differentiable. The pedal uses a diode-pair form `x / sqrt(1 + x^2)`; the
  triode stages use `x / sqrt(1 + 0.85 x^2)`. Neither selects a curve by the
  sign of its argument: an asymmetric transfer built that way leaves a
  third-derivative kink at the origin, and because a cascaded second stage sees
  a near-square waveform it crosses that kink at full slew. Measured on a quiet
  signal, that one detail cost 69 dB of alias floor on its own — -43 dB with a
  sign-selected knee against -112 dB for the same stage carrying a single smooth
  curve — which is why the asymmetry is produced by the operating point instead.
- **Operating point.** Each triode stage is driven off a standing grid bias
  plus a drift term that follows the rectified signal with a 45 ms follower, and
  the transfer at the bias point is subtracted so the stage stays centred rather
  than pumping DC into the cabinet. Driving a symmetric ceiling off centre is
  the physically motivated source of even-order harmonics, and the drift is the
  reason a held chord thickens and thins again as it decays. An interstage
  one-pole stands in for Miller capacitance, so each stage is progressively
  darker.
- **Cabinet.** Five biquad sections inside the oversampled domain: a
  second-order high-pass at the box frequency (a sealed cabinet has no useful
  output below it), a low-mid thump, a scooped boxy mid, a presence peak, and a
  fourth-order Butterworth roll-off from 5 kHz, because a twelve-inch speaker is
  essentially gone an octave above that. Running it before decimation rather
  than after removes the alias-generating content first. The regression suite
  asserts each of those five features relative to 1 kHz. This is a filter model
  of the *class* of response, deliberately not an impulse response of a
  measured cabinet: no capture is included and none is claimed.
- **Level.** Each stage divides its own small-signal gain back out, so the
  control travels through tone rather than through level. Measured on a loud
  Drop-E rhythm figure from the string model, the whole travel of the amp
  control stays inside a few decibels of the dry DI; a saturating stage still
  ends up slightly louder, because compressing a signal raises its average, and
  the suite bounds rather than denies that.
- **Compressor, delay, room.** The compressor eases into roughly 3.5:1 above
  -20 dBFS through a soft knee, with makeup. The 360 ms lead delay damps and
  thins its feedback path, so repeats darken as an analogue delay's do rather
  than returning identical copies. The room is three allpass diffusers into two
  damped combs per channel at coprime, channel-offset lengths, with no
  modulation, Haas delay or randomised phase — the same constraint the
  instrument's own stereo field obeys.
- **Bypass.** All five mixes are smoothed per sample and snap to exactly zero,
  so a control left at zero is a bit-exact bypass; the regression suite compares
  the output to the input with `memcmp`, and checks that the chain returns to
  that state after the gain block has been engaged and released. Engagement and
  disengagement are crossfaded, and the suite bounds the largest sample step
  across both transitions against the dry and settled-wet slew.

What is deliberately not claimed: no circuit is solved (no Wave Digital
Filters, no nodal state-space, no K-method), there is no power-supply sag or
output-transformer model, the cabinet is not a measured impulse response, and
none of it is a model of any named amplifier or speaker.

## Voicing against a reference recording

Everything above is structural: the models and their references. Their constants
still have to be *voiced*, and for the low register that was done by measuring a
dry electric low-open-E reference recording (82 Hz, unknown instrument, pickup
and chain) and comparing Electry at the same pitch, onset-aligned, in successive
25 ms windows. The recording is not committed - its provenance is not ours to
redistribute - and it is a voicing reference, not a calibration standard: one
guitar, one player, one signal chain, uncalibrated in absolute level.

What the comparison showed, and what changed:

- **The wound strings decayed far too fast at the bottom.** The reference's
  overall level falls about 24 dB over the eight seconds after the attack, and
  its fundamental partial more slowly still; Electry's fundamental was falling at
  6.6 dB/s against the reference's 0.5 dB/s early on. The wound fundamental T60
  targets went from 6.4-8.2 s to 15-20 s, and the clamp with them.
- **They decayed far too slowly at the top.** A wound string's wrap slides over
  its core and dissipates bending energy, so its high end dies while the
  fundamental is still ringing. The high-frequency decay ratio was an order of
  magnitude too generous; it is now roughly 0.01-0.04 of the fundamental's, and
  differs by a factor of 7.5 between wound and plain strings rather than 1.4.
- **The excitation gave the upper partials too much to begin with.** The ideal
  1/n^2 released-displacement law, which the two modal sections implement, is
  what a point pluck of a perfectly flexible string produces, and the pickup's
  induced-EMF differentiation turns it into a 1/n voltage spectrum. The reference
  falls roughly 9 dB faster than 1/n by its fourteenth partial. One further
  release pole supplies the difference, with a corner following the square root
  of the string's open frequency - a heavier string leaves the pick more slowly -
  and its attenuation at the fundamental divided back out so the release governs
  timbre rather than level.
- **The pluck comb cancelled too perfectly.** The reflected excitation image was
  an exact copy, giving notches twenty decibels deep where the reference's comb
  ripple is a few decibels. It now returns through a unity-DC-gain low-pass,
  because it has travelled to the pick and back through the same lossy string.
  Unity DC gain matters: the loop deliberately carries no DC blocker, so the
  comb still has to reject a net displacement exactly.

Taken together, across the partials whose comb nulls the two instruments share,
the mean absolute per-partial error against the reference falls from 4.5 dB to
2.9 dB. The two partials excluded from that figure are where Electry's own comb
nulls fall; null *positions* depend on the pluck point and pickup distance and
differ between any two guitars, so they are not a target.

The perceptual point of all of it: a low note whose fundamental sustains and
whose upper partials die quickly reads as round and full-bodied, and the
opposite - a weak fundamental under persistent 1-2 kHz partials - is what a
clavinet sounds like. That was the character the low register had.

The palm mute was measured the same way, from dry muted power-chord references
at two mute depths. Electry's chug was falling 36 dB in its first 25 ms where the
reference falls 2 dB - an impulse rather than a note, which is what "cut chugs"
describes. The cause was not the mute's decay target but the *shape* of the
model: the hand was applied as a minimum on the fundamental's T60 and that
result then multiplied by the string's high-frequency ratio, so a half-second
mute implied a seventeen millisecond top end. Modelling the hand as a broadband
loss in parallel with the string's own - decay rates adding at each frequency
independently - fixed it, and the reference-derived depths land the Muted style
at 0/-1/-3/-10/-13 dB over the first 150 ms against the reference's
0/-2/-4/-10/-13.

A third round addressed what the envelope work did not: a muted note whose
length was right still read as a short pick rather than as a muted note, because
the hand damped only the *loop*. A real hand is already resting on the string
when the pick arrives, so it absorbs the attack as the pluck forms. Measured, a
palm-muted chord's attack centroid sat within one percent of an open one's, and
the Chug style was actually *brighter* than an open note - a hand on a string
cannot do that. The bridge hand now also darkens the excitation: the release
corner, the contact-noise band and, most importantly, the level of the plectrum
edge, which was 2.6x the modal path at 2 kHz and therefore dominated the muted
attack's whole upper band. Chug's brightness multipliers above one were removed.

That closed part of the gap and not all of it. The muted attack's upper bands
came down (Chug 1.3-2.6 kHz from 25% to 22%, Muted from 17% to 14%, and the
2.6-5.1 kHz band roughly halved), but its centre of mass still sat about an
octave above the reference's, and the instrument was still described as hollow.

A fourth round found why, and it was neither the mute nor the excitation. Scored
on half-octave band energies against the same references - per-partial scoring
turned out to be dominated by comb-null alignment, where a one-partial offset
between two different guitars swamped everything else - Electry was missing 5 to
11 dB in the 60-85 Hz band across three independent windows, including the open
unmuted note. The same deficit on an *unmuted* note is what ruled the mute out
as the cause. Two candidate explanations were measured and rejected outright:
moving the excitation's two modal sections' corner across a wide range changed
the fundamental's relative level by a fraction of a decibel, and switching the
plectrum-edge path off entirely changed it by 0.1 dB, so the excitation was not
the lever its comments implied it was. What remained was the pickup, and
specifically that its position comb subtracted two taps of *equal* weight: an
exact zero at DC and an infinitely deep null, neither of which a real pickup
has. The reference showed this directly - it has no dip at all in 1360-1920 Hz
where Electry had a 34 dB hole. Weighting the delayed tap at 0.60 recovered
4.7 dB of fundamental on the open low E and 5.4 dB on the muted chord, and the
frequency-tilted hand above supplied the rest of the muted case. The mean
absolute half-octave band error against the references fell from 5.55 dB to
4.31 dB, and the 60-85 Hz error on the open attack from 5.4 dB to 0.7 dB.

The remaining residual is honest to record: the open note's sustain window still
scores 6.05 dB, mostly in 1360-1920 Hz where Electry's comb null and the
reference's do not coincide, and null *positions* are a property of each
instrument's pluck point and pickup distance rather than a target. Explicitly
modelling the humbucker as two coils 19 mm apart was the obvious candidate for
filling that hole and is described above; it measured no better and is not in
the model.

A fifth round fitted the mute against nine dry muted power-chord references
spanning five pitches, which is the first time the set could distinguish a
pitch-scaling error from a constant offset. The decisive measurement is
per-harmonic, and it overturns how the hand had been modelled:

| 50 to 350 ms drop | h1 | h2 | h3 | h4 | h8 | span |
| --- | --- | --- | --- | --- | --- | --- |
| reference, lowest pitch | +1.0 | -4.4 | -12.7 | -24.4 | -58.5 | 59.5 dB |
| Electry before | -20.1 | -19.4 | -20.0 | -19.4 | -26.3 | 6.2 dB |

A palm mute does not shorten the fundamental. Across all five reference pitches
the fundamental's own level moves by +1.0 to -2.8 dB over the first third of a
second while the third harmonic drops 7 to 13 dB and the fourth as much as 24 dB.
The loss climbs steeply with harmonic number, and more steeply at low pitch.
Electry's loss was flat in harmonic number at every pitch: a broadband gate with
an absolute-frequency tilt, not a mode-selective absorber. That is what "muted
but not muted-sounding" was describing, and the regression suite had been saying
it from the other side - a check asks a muted note to sit 12 dB below an open one
around a second in, and Electry sat 57 dB below it.

Charging the hand's full rate at the fundamental left a muted power chord with no
bottom and no tail: measured against the references it sat 13.6 dB low at 400 ms,
30.5 dB low at 800 ms, and at -100 dBFS a second after the pick where every
reference still has audible content. The hand's rate at the fundamental is now divided by six. The mode shape on its own argues for a divisor near eight - a heel a
tenth of the sounding length from the bridge leaves the fundamental at a few per
cent of the plateau the upper modes reach - but that number cannot be used, and
the reason is the model's real limit: the loop is a single one-pole whose corner
sits far above these partials, so relieving the fundamental relieves the second,
third and fourth harmonics by very nearly the same amount. A divisor of eight
therefore over-relieves the whole low-mid comb and overshoots the reference
contour. Swept against the five matched reference pitches on a joint objective of tilt
shape plus peak-relative energy contour, the contour error falls 11.74, 6.64,
4.84, 4.11, 4.00, 4.38, 5.31 dB at divisors of 2, 3, 4, 5, 6, 8 and 12 - a clear
minimum at six. Measured in absolute level, the muted chord's 150-500 ms window
gains 12.5 dB against the old voicing, its 500-1000 ms window 29.9 dB and its
1-2 s window 48.9 dB, with the attack peak unchanged within 0.5 dB.
Once each voicing is allowed its own Mute Damp setting - the honest comparison,
since that is a user control and this change moves its optimum from 0.45 to 0.70 -
the gain is 0.3 to 0.5 dB. The attack is bit-identical, and so is the string
model for every unmuted
articulation, because the term is multiplied by a hand rate that is exactly zero
without one.

Three other candidates were fitted against the same references and rejected, all
for reasons worth keeping:

- **Anchoring the fitted high point to the string's own series** - `fHigh = k*f0`
  rather than an absolute 3.6 kHz - is almost certainly the right answer to the
  harmonic-number problem, and it produced by far the largest improvement
  measured anywhere in this work: body-window band error 11.03 to 4.09 dB, with
  the error's pitch slope flattening from -2.11 to -0.73 dB per semitone. It
  cannot ship as it stands. `highRatio` was calibrated at 3.6 kHz, so moving the
  point requires re-deriving it, and re-deriving it in log frequency raises it to
  a power near 0.6 in the low register, which compresses every parameter that
  acts through it. The Les Paul to Telecaster spread is 3.95x at stock and 2.26x
  after, which collapses the centroid separation the suite pins, and halves the
  construction axis's audibility on the lowest string. Un-compressing needs
  k >= 19, by which point the improvement is gone. Fixing this needs the loop to
  carry a steeper loss curve, not a re-anchored two-point fit.
- **An intrinsic strum spread for the stroke articulations** improved the
  objective on its own (11.64 to 11.33) but must not be combined with the shipped
  change: it improves the decay contour by removing attack peak while the shipped
  change improves it by adding body energy, and applied together they overshoot
  into positive error - 11.72, worse than doing nothing. Measured at 2, 3 and 6 ms
  of travel; there is no spread at which the pair beats the shipped change alone.
- **Scaling the hand's decay time by the string's mass** measured exactly zero
  change to the objective.

What remains wrong, named because it has been understated before. The
harmonic-number tilt is still shallow: the tilt error is 9.07 dB against the
references and roughly half the per-round-trip shortfall is still there. The
body-window brightness excess above 480 Hz is essentially untouched at +15.6 dB
mean against +16.2 before. The root partial still sits well below Electry's own
strongest low peak where every reference has it at the top. And the model still
cannot represent the tight-versus-loose mute distinction at all - the same note's
two reference takes differ by 11 to 17 dB at 400 ms, and no single setting of Mute
Damp matches both.

One earlier entry on this list was wrong and is worth correcting rather than
quietly dropping, because it nearly sent the next pass in the wrong direction. The
attack was recorded as 1 to 4 ms of onset-to-peak against the references' 17 to
26 ms, which reads as a transient far too sharp. Measured with one definition
applied identically to both - a 2 ms sliding envelope from the 25% threshold to
the maximum - Electry rises **more slowly** than every reference: 22 to 44 ms
against 10 to 35 ms. The attack is not too spiky, and softening it would move away
from the references, not toward them. The two figures in the old entry were not
measured the same way as each other.

That hand-specific loss filter is now in the model, and getting it to work turned
on one detail rather than on any constant. Gated to the muted articulations so it
cannot reach the thresholds that blocked the re-anchored high point, it is a shelf: unity at DC, falling above a corner set as a multiple of the
fundamental, so its slope is expressed in harmonic number and the same shape
works an octave down.

The detail that matters is that its magnitude at both fitted points is divided
back out of the decay targets, so the one-pole is solved for what is left. A first
attempt left the shelf outside the solve, and the result was the clearest negative
in this whole body of work: across its entire usable range it traded the two terms
of the objective almost exactly one for one - tilt 17.9 to 13.7 dB against contour
6.6 to 11.3 dB - so the joint score never improved, and at the depths that
produced real tilt the note went inaudible again. The extra loss was simply being
added on top of a decay that had already been fitted.

Inside the solve the trade breaks. Swept over depths of 0 to 0.50 against corners
of 2 to 15 times the fundamental, the minimum is at a depth of 0.35 and a corner
of eight, where the tilt error falls 17.69 to 11.50 dB while the contour term
rises only 4.00 to 5.45 - six decibels bought for one and a half, against one for
one outside the solve. The joint objective improves 10.85 to 8.48 dB.

This is possible only because a muted note has the headroom for it. Its loop gain
is around 0.69 per round trip where an unmuted twenty-second decay runs at 0.996,
so there is room to redistribute loss between the fitted points; the same
compensation on an open string would ask for a loop gain above unity and clamp.
That is also why the shelf is gated rather than global.

### The shelf was the wrong shape, and the reason is measurable

The shelf has since been removed. Deleting it entirely costs 0.01 dB on the joint
objective once the band dip below is present, which is to say it does nothing that
the dip does not do better, and it was shipping a stage in the audio loop for no
measured benefit. Why it could not work is worth recording, because the diagnosis
took a wrong turn first.

The stated reason for going further was that the model needed a steeper loss curve
- a higher-order loop. That was wrong, and one measurement settles it. A loop
filter controls loss **per round trip**, and the string makes f0 round trips a
second, so a drop of D dB over a window of t seconds needs only D/(f0*t) dB of
loss per round trip. At the bottom of this instrument's range that divisor is
around 13.8. The references' towering 59.5 dB h1-to-h8 span is therefore asking
for about 4.3 dB per round trip at the very bottom and a mean of 1.35 dB across
the five pitches - a *shallow* requirement that a single one-pole produces across
three octaves without effort. The span is large because it accumulates, not
because the curve is steep. Order was never the constraint.

The real constraint was where the shelf spends its depth. A shelf is flat above
its corner, so it applies its full depth at the 3.6 kHz fitted point, where the
references ask for no extra loss at all. The one-pole cannot get flatter than flat
to pay that back, so the feasibility bisection refuses anything deeper - and it
refuses it silently. Measured, requesting depths of 0.50, 0.70 and 0.90 produced
*bit-identical* decay, because the outer bisection converged on the same feasible
maximum every time. The shelf was saturated at roughly a fifth of the loss the
lowest reference pitch needed, and no constant could have changed that.

Lowering the corner does not help either, and the reason is the same fit geometry
seen from the other side. For the lowest reference pitch, h1 to h8 spans 46 to
368 Hz - the bottom sixth of the f0-to-3.6 kHz fitted span, with both pins outside
it. A corner at 2.5*f0 is nearly flat across that whole span, so it reads to the
solve as a constant gain and is almost entirely compensated away: it shifts level
without reshaping anything. Measured at a depth of 0.08 and that corner, tilt
moved 11.82 to 11.54 while contour worsened 5.04 to 6.63.

### Two degrees of freedom, and neither one works alone

What ships instead is a band of loss - a peaking section with sub-unity gain,
centred on a multiple of the fundamental and returning to unity above it. Because
it returns to unity it costs almost nothing at the high fitted point, so the
feasibility ceiling stops binding and the depth can be what the references ask
for. It is centred at five times the fundamental with a Q of 0.7; the centre is a
clear minimum at five and does not move with anything else tried, and Q is a clear
minimum at 0.7. Its depth saturates - 4 dB and 14 dB score within 0.02 dB of each
other, because feasibility takes over well before the constant does.

Alongside it, the hand's relief at the fundamental moves from a divisor of six to
twenty-two. That is a large change and it needs the pairing to be legible, because
the two are complementary in a way neither is alone:

| configuration | tilt | contour | joint |
| --- | --- | --- | --- |
| relief 6, no dip (before) | 17.69 | 4.00 | 10.85 |
| relief 22, no dip | 17.55 | 6.90 | **12.23** |
| relief 6, dip 10 dB | 9.41 | 8.86 | 9.13 |
| relief 22, dip 10 dB | 9.08 | 2.92 | **6.00** |

The relief alone is **worse than doing nothing**: it lengthens the tail and gives
the contour term back more than it gains, because nothing is removing the
harmonics that then ring on. The dip alone trades one for one, as the shelf did.
Together they are 4.85 dB better than the state before any of this work and 2.4 dB
better than the shelf they replace. This is the point at which the tilt and the
note's total energy stop being one degree of freedom - the thing the previous pass
correctly identified as necessary and incorrectly diagnosed as filter order.

The relief is bounded above by the suite's own requirement that a muted note decay
dramatically faster than an open one. That still holds at a divisor of 70 and
fails at 110, so twenty-two sits with about a factor of four in hand rather than
against a cliff. Its own sweep is a flat minimum - 6.20, 6.05, 6.03, 6.05, 6.09 dB
at 16, 19, 22, 25 and 28 - so the middle of the flat region is used rather than an
edge.

Measured where the defect actually was, which was the tail rather than the tilt.
Electry's energy relative to its own attack peak, against the five references:

| window | reference | before | after |
| --- | --- | --- | --- |
| 150-500 ms | -9.4 to -13.5 | -11.0 to -12.4 | -11.8 to -13.0 |
| 500 ms-1 s | -13.9 to -22.4 | -21.1 to -22.3 | -18.5 to -19.6 |
| 1-1.8 s | -18.0 to -29.4 | -31.6 to -32.2 | -23.7 to -25.0 |

The one-to-two-second window was 10 to 14 dB short of every reference and is now
within 2 to 6 dB of four of them, overshooting the fifth - the fastest-decaying
take - by 4.7 dB. Note also how nearly pitch-invariant the old column is: -11,
-21, -32 at every pitch, against references spanning 4 dB, 8 dB and 11 dB across
the same five notes. Per round trip, the shortfall against the reference tilt at
h2 through h8 falls from -0.10, -0.58, -0.57, -0.85, -0.58, -0.93, -0.88 dB to
-0.00, -0.26, -0.23, -0.41, -0.37, -0.64, -0.58: a little over half of it closed,
and exact at the second harmonic.

One implementation detail is load-bearing rather than fastidious. The dip runs in
double, and its numerator is renormalised so its gain at DC is exactly one. The
section is analytically unity at DC - numerator and denominator both sum to
(2 - 2cos w0)/a0 - but at the bottom of this range that quantity is around 3.6e-5
formed by subtracting two numbers near two, so in float the two sums disagree in
their leading digits. Measured, the section's real gain at DC was 1.00066. It sits
inside the string's feedback loop, and the only DC blocker in this engine is on
the output where it cannot reach: against a loop gain at its 0.99999 ceiling that
is a mode growing 0.07% per round trip and never decaying. Normalising in float
alone brought it to 1.00033, which is the float representation error rather than
the algebra; double brings it to exactly 1.00000000, asserted over 3920 live
configurations by `testHandDipNeverExpands`. This is the same trap the modal
resonator bank already documents, met a second time in a different filter.

### The default voicing, and what moving it cost

The shipped defaults were the midpoint of every axis, which is not an instrument
anyone owns. They are now a thick carved set-neck blank strung with the heaviest
set on a 27.63-inch scale, a humbucker-leaning bridge pickup, the tone control a
little back, and a softer pick close to the bridge. Against the same nine muted
references at five pitches the joint error is 5.03 dB where the midpoints
measured 6.31 - so this is not only a preference, it is a measurably closer
instrument.

The gain does not decompose, and that is worth recording because it nearly went
in wrong. The four "weight" fields - body wood, size, shape, construction - moved
on their own score **6.41**, slightly *worse* than the midpoints they replace,
because the pick sitting at 0.35 costs more than a thick blank recovers. Swept
separately: moving the pick out from 0.18 costs 2.1 dB and selecting the neck
pickup costs 1.5 dB. The eleven fields are one voicing.

What it cost is control range, and the number is not small. Sweeping each axis
end to end on the new instrument, by the same normalised-difference measure the
suite uses:

| axis | on the old midpoints | on the new defaults | suite floor |
| --- | --- | --- | --- |
| body wood | 0.058 | 0.047 | 0.055 |
| body size | 0.106 | 0.045 | 0.055 |
| body shape | 0.066 | 0.042 | 0.055 |
| construction | 0.065 | 0.058 | 0.055 |
| string gauge | 0.125 | 0.050 | 0.080 |
| body resonance | 0.080 | 0.028 | 0.080 |
| pick position | 1.341 | 1.346 | 0.350 |

Five axes lose between a tenth and two thirds of their audible range, and four
now sit below the floors the suite had set for them. Nothing in the model
changed: a darker, heavier, louder note simply makes every structural axis a
smaller fraction of itself. Body resonance is worst hit, going from a clearly
audible endpoint change to a subtle one.

That is why `testMaterialAndControlAudibility` and `testArtifactsControl` now
state the instrument they measure on rather than inheriting the defaults. Seven
checks in those two functions failed when the defaults moved, without one line of
the model changing, and the thresholds were calibrated on the mid-scale,
tone-open instrument - so pinning it keeps the numbers meaning what they were
measured to mean. It is worth being explicit that this is a real limitation of
the suite as it now stands: with the instrument pinned, no check verifies that
the *shipped* voicing keeps its controls audible. The table above is that check,
run by hand, and it is the reason the cost is written down here rather than
discovered later.

Pick position is untouched at 1.35 and remains by far the largest lever a player
has, so the structural colour is reachable - it is the default position that is
darker, not the range that is smaller.

Separately, and on listening feedback rather than measurement, the release-noise
burst was too loud. Its coefficient drops from 0.34 to 0.20 on the wound strings
and 0.20 to 0.13 on the plain ones. This is a global voicing change and it is
worth being precise about that, because the surrounding work is not: it applies to
every articulation, so unmuted notes are no longer bit-for-bit what they were even
though the string model underneath them is untouched. An earlier phrasing here
attributed it to the restored muted tail, which would only have justified a
muted-only reduction; the reduction is wanted everywhere. The control's own range
is unchanged, and Release Noise at zero remains an exact no-op.

The amplifier chain was voiced against the same goal on a chugged Drop-E figure.
Its input stage now passes the whole eighth-string fundamental instead of cutting
it at 84 Hz, because clipping that fundamental is what generates the harmonics
the cabinet turns into weight; the pre-gain mid emphasis moved from 560 Hz to
850 Hz so it no longer pushes gain into the one region the cabinet then scoops;
the cabinet's thump is deeper and its boxy region cut harder; and the
compressor's attack is 18 ms rather than 3 ms, so a pick attack survives instead
of being levelled away. Measured on that figure, the 80-160 Hz octave carries
55% of the amplified energy against 24% of the dry DI's, while 320-640 Hz falls
from 22% to 4%.

Two regression thresholds were relaxed as a direct consequence, and are marked
as such in the suite: the picked attack is now legitimately darker, so the
absolute centroid gap between a fingered and a picked attack, and the centroid
range Pick Hardness spans, are both smaller than they were against the previous
brighter voicing. The orderings those checks exist to pin are unchanged.

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

The amplifier chain has its own suite: halfband unity DC gain, the -6 dB
halfband symmetry point, passband ripple and stopband rejection; a bit-exact
dry bypass with every control at zero and an audible effect from each control
on its own at 100%; the alias floor of the pedal, the amplifier, and the two
stacked, at two input levels; each of the cabinet's five voicing features
relative to 1 kHz; loudness bounds across the whole amp travel and every
combination of the gain and compressor controls on a rendered Drop-E rhythm
figure, dry and palm muted; the lead delay's first repeat at 360 ms with a
clean gap before it; a decaying, decorrelated room tail; bounded sample steps
across gain-stage engagement and disengagement and a return to bit-exact
bypass afterwards; render determinism; finiteness, output-clamp headroom and
the expected group delay at eight host rates from 22.05 to 384 kHz; and
recovery from NaN, infinite, and out-of-range input as well as null and
zero-length calls, including a single non-finite sample in the middle of an
otherwise clean block. The cabinet's low-frequency probe sits below the modelled
box corner, which is deliberately low enough that a Drop-E eighth string's
fundamental reaches the cabinet rather than being cut before it. A further test renders a short take through the demo
renderer, so the committed demonstration audio's toolchain is covered too.

Ten rendered examples of the whole path are committed under
[`Docs/audio/`](audio/README.md) and produced from this same JUCE-free code by
`Tools/RenderDemos.cpp`, so what the document describes can be listened to
rather than only read. They are demonstrations, not evidence: an audible
example is not a measurement, and none of the claims above rest on them.

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
