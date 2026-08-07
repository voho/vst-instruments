# Electry physical-modeling research and implementation contract

Electry 1.2 is a white-box physically modeled dry electric guitar with named
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
| Two-stage decay and beating | Two-polarisation string behavior described in the same plucked-string literature | The polarisation parallel to the body carries a 1.7x longer decay target and a sub-cent detune, so the mixed output beats slowly and decays in two stages. Both the detune and the exchange between the polarisations are fractions of a round trip rather than fixed numbers of samples, so neither follows the host clock; the loop filter's own two-frequency fit still does, leaving a measured 4.5 dB residual spread across 44.1-192 kHz at the top of the range | A qualitative reproduction of the documented mechanism with voiced constants; not calibrated polarisation data from a measured instrument |
| Stiffness dispersion | Stiff-string inharmonicity `B = pi^3 E d^4 / (64 T L^2)` (Fletcher and Rossing) and robust factored allpass design practice (Rauhala and Välimäki; Abel and Smith) | A per-note `B` from string diameter, effective wound-core bending fraction, scale length, and tension drives an eight-stage factored first-order cascade; two coefficients are fitted jointly at low and high partials, with exact fundamental phase compensation | A physically derived, bounded two-band fit whose regression error is under 20% at both references for the worst heavy Drop-E case; not a capture-fitted very-high-order piano dispersion filter |
| Loop damping and tuning | Decay-time-targeted loop-filter design from the plucked-string literature; a dry electric low-E reference recording for the targets themselves | Per-string, per-fret one-pole loop filters solved by bisection from independent T60 targets at the fundamental and a high reference frequency, with all loop-filter phase delays compensated analytically at the fundamental. The wound strings' fundamental targets are tens of seconds and their high-frequency ratio two orders of magnitude smaller, following the reference | Decay-targeted loop design with exact fundamental tuning (regression bound: under 8 cents across E1..D6 at tested host rates through 384 kHz), whose fundamental and high-frequency targets are calibrated against one reference recording; not per-partial measured decay matching across a fretboard, and not a model of the reference instrument |
| Dead spots | Fleischer's electric-guitar dead-spot studies relating neck conductance to decay time | A per-string fret-position Gaussian that locally shortens decay, deepened by the bolt-on end of the construction axis | The documented mechanism direction with voiced positions and depths; not measured conductance maps of specific instruments |
| Tension modulation | Tolonen, Välimäki, and Karjalainen's tension-modulation nonlinearity | A string-energy envelope drives a bounded shortening of the loop delay, producing the attack pitch glide that relaxes over hundreds of milliseconds | The published phenomenon in its energy-envelope shortcut form; not the exact elongation integral or a time-varying-fraction-delay implementation |
| Plectrum and finger excitation | Plectrum and touch interaction modeling by Germain and Evangelista and by Evangelista and Eckerholm | A three-phase excitation combines contact retention and scrape, a principal string-period-scaled two-pole modal release that approximates triangular pluck displacement, one further release pole whose corner follows the square root of the string's open frequency (a heavier string leaves the pick more slowly) with its own attenuation at the fundamental divided back out, and a normally much smaller broadband pick-edge transient for sustained pick styles; the release window is asymmetric (a slow load and a fast slip, both smoothsteps, at constant area) and its reflected image is distributed over a hardness-dependent contact patch of 0.5 to 1.5 mm; delay-line projection is level-calibrated to open E4 so equal effort remains usable on E1, while polarity, polarisation split, spectrum, and comb position differ per style | A realtime modal approximation to released-string displacement plus a separate pick edge, a distributed contact width, and bounded register calibration; not an exact delay-line initial-condition solve, beam-mechanics plectrum profile, or force-based finger contact solver |
| Fret collisions | Bilbao and Torin's energy-balanced string/fretboard collision modeling | The Artifacts path's incidental fret contact: a decaying collision window whose soft limit clips vertical displacement against a velocity-dependent clearance and re-radiates deterministic rattle noise on hard-picked notes | Collision-informed contact behavior in a bounded, stable form; not an FDTD distributed-contact simulation |
| Pinch harmonic | The same touch model driven by the picking hand; standard descriptions of the technique as a thumb contact immediately after the plectrum | The touch position is the pluck fraction, so Pick Position selects the partial; a firmer (depth 1.0) and longer (90 ms) contact than the fretting finger's, because the mode-shape law gives a near-bridge touch little purchase on the low partials | Node selection by hand position with the technique's own asymmetry between low and high partials preserved; not a model of thumb geometry, pick grip, or the exact contact area |
| Touch harmonics | The touch-interaction half of Evangelista and Eckerholm's player/instrument models, and the classical mode-shape result that a point contact removes energy as `sin^2(n pi p)` | A one-tap FIR `(1 - d/2) + (d/2) z^-M` with `M = p * period` inside each polarisation loop, which *is* the `sin^2(n pi p)` weighting rather than an approximation of it; unity at a node, `1 - d` at an antinode, magnitude bounded by one at every depth. The natural harmonic touches the midpoint, so the octave is the string's own even series with its own inharmonicity, decay and pickup comb; the finger lifts once the note has formed | An exact first-order point-contact loss condensed into the delay loop, exact in magnitude and phase at the surviving partials whenever the touch sits on a node; not a distributed finger-force contact solve, and not exact at a non-node touch position |
| Slide | Pakarinen, Puputti, and Välimäki's virtual slide guitar, whose string algorithm carries a parametric model of the tube/string contact noise produced by a wound string's surface ridges | The finger stays down and the sounding length glides at a hand speed in frets per second rather than over a fixed time; the friction is a noise band centred at `v / w`, the hand's speed along the string over the winding pitch, with its level following the derivative of the glide | A time-varying delay length plus a velocity-dependent friction band, with the winding pitch a fitted linear stand-in for real wrap-wire practice; not an energy-compensated time-varying waveguide, and not a measured contact-noise spectrum |
| Hammer-on and pull-off | Touch/legato interaction models from Evangelista and Eckerholm | Keyswitched legato: a sounding string within reach retargets its delay over about 10 ms while the loop state is preserved, with a soft finger excitation and no plectrum noise | Continuous-state legato with fingered attacks; not a distributed finger-force model |
| Pickups | Paiva, Pakarinen, and Välimäki's pickup acoustics and modeling; low-frequency pickup nonlinearity measurements (Novak et al.); engineering aperture analyses | Per-string pickup-position combs follow each fret, with the delayed tap weighted 0.60 so the null is 12 dB deep rather than infinite, as a real aperture, two-coil sum and three-dimensional field never cancel exactly; an O(1) fractional rectangular moving average gives the finite aperture's exact sinc response; bounded flux nonlinearity plus shallow string-mass/pole balance is differentiated into induced EMF, guarded ultrasonically, then passed through the loaded coil/tone circuit | The published pickup signal structure (position comb of measured rather than ideal null depth, finite aperture, nonlinear flux, induced voltage, electrical resonance) with datasheet-plausible level calibration; not a magnetic finite-element, per-coil, or capture-fitted model of named pickups |
| Solid body | Solid-body bridge-admittance and dead-spot literature; geometric estimates | Structural bridge displacement is differentiated before four double-precision, peak-normalised modal resonators and a 4 kHz guard, producing body-induced voltage before the loaded pickup coils; positive real modal conductance across each note's first six partials can only shorten loop T60 | Geometry-informed structural pickup voltage plus passive mode-dependent energy extraction; not undifferentiated acoustic body displacement mixed into pickup voltage, and the mode tables remain voicing estimates rather than measured admittance data |
| Construction controls | Solid-body material/geometry contrasts, humbucker vs single-coil construction, set-neck vs bolt-on, and modern extended-range scale practice | Wood, size, shape, construction, and pickup type interpolate between contrasting reference voicings; scale length spans 25.5 to 28 inches for Drop-E | Parametrized construction and extended-range voicing; not a licensed or capture-verified reproduction of a named instrument |
| Play noise | Handling-noise observations in the virtual slide guitar work of Pakarinen, Puputti, and Välimäki | Deterministic seeded plectrum scrape, finger contact, and release damping noise, band-shaped per string (wound vs plain) and split between a one-percent string trace and local pickup/body paths | Procedural, deterministic contact noise consistent with the documented mechanisms; not convolved recordings or measured contact-noise spectra |
| Sympathetic string coupling | Bank and Karjalainen's passive admittance modeling and the sympathetic-string literature | The plucked strings' bridge force drives a one-sample-delayed bus; every string that is not being fingered runs its own single-polarisation waveguide at its open pitch, with a loop filter solved from the same pair of decay targets a played string of the same steel gets - the high-frequency one backed off toward the fundamental's wherever the pair would ask the loop for a gain above unity, so the fundamental's target is never the one given up - exact fundamental phase compensation and bridge pickup tap. Only played voices write to the bus and only idle voices read it | A one-directional (loss-only from the driver's point of view) slice of bridge coupling, provably acyclic and therefore unconditionally stable; not a shared multiport bridge scattering junction with mutual re-radiation |
| Dead note | The same additive-loss contact model as the bridge hand, applied by the fretting hand instead | A broadband 30 ms loss added in parallel to the string's own at both fitted points, with none of the palm mute's mode-shape relief or loss band, because this contact is nowhere near the bridge and is the whole hand rather than its heel | A contact loss inside the loop, so the pick's attack is untouched and the note decays through its own solved filter; not a gate, and not a model of hand pressure or coverage |
| Bridge-hand damping | Palm-muting practice, the same decay-targeted loop design, and dry muted power-chord reference recordings for the depths | The hand is an absorber whose loss adds to the string's own in parallel, so decay rates sum at each fitted frequency independently; the raw hand rate is multiplied by three at the high reference and divided by twenty-two at the fundamental, an effective 66:1 ratio between the two fitted points, because a contact near the bridge removes far more energy from high modes than from a fundamental that barely moves there; a relief that large only works paired with a band of loss centred on five times the fundamental, which removes the harmonics the longer tail would otherwise let ring - alone, each of the two is worse than neither; the Palm Mute style (whose depth the Mute Damp control spans from a loose half-mute to a tight chug) and the continuous pressure are one absorber at different depths and combine the same way, re-solving the same loop filters and the analytic phase compensation so the note stays in tune; the coupled strings are damped and starved with it | Progressive contact damping as an additive loss with reference-calibrated depths and a bounded, conservative frequency tilt, applied identically to every play style; not a distributed hand/string contact solve or a resolved mode-shape weighting |
| Fretting hand | Ordinary left-hand kinematics; the position/reach/fretting-mode controls the sampled field exposes (Orange Tree Samples' floating fret position, Impact Soundworks' Set Hand and Fretting Mode) | A floating hand position with a four-fret reach above the index finger drives string allocation through a fret-distance cost; open strings are free at the nut and progressively expensive as the hand travels; the hand shifts only when the note is out of reach and only at the start of a chord, and relaxes to the nut when the phrase ends | A single-position hand with a fixed reach and a deterministic cost; not a fingering solver, a chord recogniser, or a model of alternative fingerings for a whole phrase |
| Strum travel | Ordinary plectrum kinematics | Note-ons inside a 35 ms window are treated as one stroke; the first string fixes the edge the pick starts from and every further string's excitation is delayed by the travel time per string crossed | Constant-velocity pick travel across the string plane; not a model of pick angle, chord voicing, or the player's hand position |
| Pitch-wheel bar | The elastic string-tension relation `dF/F = dT/2T` with `dT = E A dl/l` (Fletcher and Rossing) applied to a whole-bridge stretch, as a vibrato bar applies it | The wheel stretches every string - fingered and sympathetically ringing alike - over a nominal +/-2 semitone range; each string's share follows its elastic core stiffness against its tension (which reduces to core-fraction squared over open frequency squared for one scale length), compressed toward the two-to-one spread measured on real tremolo bridges and normalised so the most compliant string spans the full range; the strings travel over the Bend Time glide rather than snapping | The documented per-string compliance direction with a voiced compression exponent; not a model of a specific bridge's geometry, spring balance, or friction |
| Fretting-hand vibrato | The same elastic tension relation the bar uses, applied by one finger rather than by the whole bridge; ordinary rock vibrato practice for the rate and depth | Channel pressure drives a shared 4.8-6.4 Hz raised-cosine offset of up to 40 cents on the fingered strings only, easing in over 90 ms and never going below the fretted pitch | The documented asymmetry and locality of a fingered vibrato against a bar's; not a model of finger force, string displacement geometry, or a per-string bend |
| Amplifier feedback | Acoustic guitar-to-amplifier feedback practice: a loudspeaker's pressure field re-excites the strings, and each string answers at its own resonances | The host pushes its previous processed block back as a bounded mono acoustic return with one block of latency (the air path); a soft-clipped, gain-scaled copy drives the string loops and the sympathetic bus, scaled by the CC1 resonance, the Resonance Depth parameter and the rig's acoustic loudness derived from the amplifier controls, so a distorted tone at full wheel regenerates while a dry DI never can; every element of the loop is bounded, so the howl saturates instead of growing | A one-block-latent, level-gated, saturating regeneration path; not a room acoustics, speaker directivity, or standing-wave model |
| Controllable artifacts | The same touch/collision literature plus bridge-hardware behavior | An exactly bypassable deterministic path combines a bridge-hardware modal bank driven through the selected pickup mix, incidental fret contact on hard-picked notes, and per-string saddle rattle, all driven by played energy. It is mechanical hardware noise, distinct from the sympathetic string coupling above | Plausible procedural imperfection with bounded feed-forward resonators; not measured hardware-noise statistics |
| Audible-work culling | Standard realtime-DSP practice | A pickup faded out by the selector is skipped entirely; Mono runs one shared coil/DC/decimation chain and mirrors it; damping-only control moves reuse the existing dispersion fit; the whole engine freezes to exact zero once nothing vibrates and the shared path is below -120 dBFS | Removal of inaudible arithmetic with the audible result unchanged; not a quality/latency trade |
| Oversampling | Standard nonlinear-audio antialiasing practice | The complete physical, body, collision, and nonlinear pickup path runs at 2x for host rates through 96 kHz, followed by a fixed 63-tap halfband FIR; higher-rate hosts run 1x | Genuine internal oversampling and filtered decimation, not a quality label applied to a native-rate nonlinear stage |
| Output field | Phase-coherent divided/hex pickup practice | Mono is the conventional summed DI. Stereo weights each modeled string by its physical lateral position, keeps shared body modes centered, uses linked output limiting and independent matched decimation, and folds coherently to mono | A virtual divided-pickup string field with no time or phase widening; not room, amplifier, cabinet, chorus, or acoustic stereo |
| Amplifier and cabinet | Pakarinen and Yeh's review of vacuum-tube amplifier modeling; published supply-sag behaviour (a plate rail falling from around 350 V to around 250 V within 100 ms and recovering over 300-600 ms); transformer core saturation as a volt-second limit; standard antialiasing practice for cascaded nonlinear stages; sealed-guitar-cabinet response measurements; extended-range metal rhythm practice for the voicing | Two cascaded smooth triode ceilings driven off a standing grid bias with a level-tracking bias drift and an interstage Miller roll-off, a tight input coupling network, a power stage whose supply droops by up to 28% under its own output current with a 70 ms attack and a 400 ms recovery and whose rail sets the headroom rather than the gain, an output transformer modelled as a normalised flux integral with the excess the core cannot carry subtracted back out, and a five-section cabinet (box high-pass, low-mid thump, scooped mid, presence peak, fourth-order roll-off), all inside a 4x oversampled domain reached through Kaiser-windowed halfband stages designed at prepare time | Structurally motivated static-nonlinearity amplifier voicing with genuine oversampling and a filter-modelled cabinet; not a circuit-solved (Wave Digital or nodal state-space) amplifier, a measured impulse response, or a model of any named amplifier or speaker |

## Implemented signal path

The authoritative implementation is `Source/DSP/ElectryEngine.cpp`:

1. MIDI notes 12..21 are two independent banks of latching keyswitches:
   12..14 (C0..D0) latch the picking style - downstroke, upstroke,
   alternating strokes - and 15..21 (D#0..A0) latch the play style -
   sustain, palm mute, hammer-on/pull-off, natural harmonic, pinch harmonic,
   slide, dead note. The banks compose, so any of the twenty-one combinations
   is reachable in at most two keyswitches; a hammered or slid note has no
   plectrum, so it neither takes a stroke colour nor consumes a step of the
   alternate sequence. Notes 22..27 are ignored. Notes 28..86 are playable on eight physical strings in Drop-E
   tuning (E1-B1-E2-A2-D3-G3-B3-E4); a deterministic allocator maps each note,
   preferring a repick of an already-sounding note, then the hammer-on
   continuation of the nearest sounding string, then the free string that costs
   the fretting hand least, and finally an oldest-first steal. The hand's cost
   is a fret distance: zero inside the span its four fingers reach, growing
   linearly outside it and 1.6 times faster below the index finger than above
   the little one, because the hand pivots forward from the thumb. An open
   string needs no finger and so costs nothing at the nut, and 0.25 frets per
   fret of hand travel once the hand has left it. The hand moves only when the
   note it is asked for lies outside its span, and only on the first note of a
   chord window, and it relaxes to the nut when nothing is held and no note has
   arrived for a second and a half.
2. Each string voice runs two single-delay-loop waveguides (vertical and
   horizontal polarisation). Each loop has a third-order Lagrange fractional
   read, eight factored first-order dispersion allpasses jointly fitted from
   the string's physical inharmonicity at two partials, a one-pole damping filter solved from T60 targets,
   and a release/mute gain ramp. A contractive bridge matrix exchanges a
   small, fixed fraction of the wave between the polarisations per round trip
   of the string, not per rendered sample.
3. The loop delay compensates the exact phase delay of every loop filter at
   the sounding fundamental. Hammer-on glides, the pitch wheel, and
   tension modulation move the delay target; a short smoother keeps the
   motion click-free. The wheel is a bar: it bends every string - the
   sympathetically ringing open strings included - each by its own elastic
   compliance, and the strings travel to the wheel over the Bend Time
   parameter's glide rather than snapping. There is deliberately no DC filter
   inside the loop: a fixed-corner blocker's steep phase lead near a low
   fundamental would detune the upper partials against the compensated
   fundamental. The pluck comb's image still rejects a net displacement
   exactly, and the 5 Hz output blocker removes the offset the weighted
   pickup comb now passes.
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
5. A string-energy envelope shortens the loop delay (tension modulation), so
   hard attacks start audibly sharp and relax over hundreds of milliseconds.
   The Artifacts path can additionally open a decaying fret-collision window
   on hard-picked notes that soft-limits vertical displacement against a
   velocity-dependent clearance and adds deterministic rattle proportional to
   the clipped excess.
6. Every string that no note owns is a bridge-coupled sympathetic string. The
   plucked voices' bridge force is accumulated into a bus that the coupled
   strings read one sample later, which removes any dependence on voice order
   and any algebraic loop. Each coupled string reuses its own otherwise idle
   delay line at its open pitch with a loop filter solved from the same two
   decay targets - the fundamental and the wound/plain high-frequency ratio -
   that the same string gets when it is played, with exact
   fundamental phase compensation, is bounded by a rational soft limit, and is
   read through a bridge-position tap and an induced-EMF difference before
   joining the pickup sums. Because only `active` voices write the bus and only
   inactive voices read it, the coupling graph is a DAG and no coupling gain
   can create a growing loop. The muting hand of a palm-muted note damps and
   starves the coupled strings, and the whole path is exactly bypassed at 0%.
   The CC1 resonance control lifts the coupling amount from the Sympathetic
   Ring parameter toward total, scaled by the Resonance Depth parameter, and
   opens the acoustic feedback path: the host's previous processed block
   returns as a bounded mono loudspeaker signal whose soft-clipped,
   gain-scaled copy drives the string loops and the sympathetic bus, further
   scaled by the rig's acoustic loudness derived from the amplifier controls.
   Every element of that loop saturates, so a full-wheel distorted howl is
   bounded, and with the wheel down the stored return is never injected -
   bit-exact bypass.
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
`[[1-c, c], [c, 1-c]]`, whose eigenvalues never exceed one.

The two polarisations meet where the string is terminated - the saddle and the
nut or fret - so the exchange is charged **per round trip**, not per rendered
sample: `c = 0.04 / N` for a loop of `N` samples. Charging a fixed `c = 0.004`
on every sample instead made the exchange proportional to the loop length,
which is proportional to the sample rate and inversely proportional to the
pitch. Measured, that worked out at 33% of the wave per round trip at the top
of the range and over 900% on the open low E, so the low strings' two
polarisations were averaged into one long before they could produce the
two-stage decay they exist for; and because the matrix is contractive and the
two loops have different lengths, the mismatch was dissipated. The 22nd-fret
high E then sat 55.7 dB under its own attack a second later against a fitted
T60 of eight seconds, and it did so 22 dB differently on a 44.1 kHz host than
on a 96 kHz one. Per round trip it is one number at every pitch and every host
rate: `0.04` was chosen from a sweep of 0.33, 0.16, 0.08, 0.04 and 0.02 in
which everything at or below the open G3 moves by under 0.3 dB in every decay
window while the top of the range gains monotonically. The rate and the value
are voicing decisions; the mechanism is the documented one.

The horizontal detune is likewise expressed as a fraction of the period rather
than as a fixed number of samples, for the same reason: a bare 0.11-sample
offset made the beat rate follow the host clock, 45% faster at 48 kHz than at
192 kHz on the top string.

Neither change makes the model exactly rate-invariant, and the residual belongs
in the record rather than in a footnote. What is left is the loop filter itself:
a one-pole fitted at two frequencies expressed in normalised radians is a
different filter shape at a different sample rate, and no amount of expressing
the coupling per round trip changes that. Measured on the 22nd-fret high E, the
1-2 s level relative to its own attack reads -20.03, -19.29 and -15.51 dB at
44.1, 48 and 96 kHz: a 4.5 dB spread, against 32.7 dB (-54.47 / -55.65 /
-22.94 dB) before. The regression bound is deliberately set at that scale -
under 3 dB through 1.5 s and under 8 dB through 3 s - rather than at something
tighter the model cannot honour.

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
mute style adds a parallel bridge-hand loss whose reference-derived targets
run from 2.60 s to 0.32 s across the mute-damping control (the bridge-hand
damping section below), with a darker excitation and stronger contact noise.

## Sympathetic bridge coupling

An electric guitar's unfingered strings are not silent while you play. Energy
crosses the bridge and drives them at their own open pitches, which is a large
part of why an eight-string instrument sounds dense and why players use fret
wraps to stop it. Electry models the mechanism directly instead of colouring
the output with a resonator bank.

The plucked voices accumulate their bridge-bound wave `0.5 (v + h)` into a bus.
Every string that no note owns reuses its own otherwise idle delay line as a
single-polarisation waveguide tuned to its open pitch, solves its loop filter
from the same two decay targets the played strings use - the fundamental's T60
and the same wound/plain high-frequency ratio, through the same bisection -
compensates the loop filter's phase delay at the fundamental, and is read
through a bridge-position tap and an induced-EMF difference before joining the
pickup sums.

Sharing the decay law matters: a coupled string is the same piece of steel as a
played one. The fixed one-pole this replaced was a mild lowpass whatever the
string (0.45 at the default string age), so the wound strings' top end, which
a played low E loses inside a tenth of a second, rang for over three seconds in
the coupled bank. Measured on the coupled ring left by a picked open A2, energy
between 1.5 and 6 kHz sat 37.8 dB under the 60-700 Hz band; solved from the
string it sits 87.1 dB under. The coupled bank now reads as strings ringing
rather than as a bright plate.

The two targets are two constraints on one first-order filter and one scalar,
and they are not independent, which is the part that has to be handled
explicitly rather than assumed away. A one-pole steep enough to meet a demanding
high-frequency target has its pole close to the unit circle, where its own
magnitude at a low fundamental collapses toward `(1 - a)/w0`; the loop gain that
would have to buy that back exceeds one, and a loop gain above one is not
available. Solving the ratio and then clamping the gain keeps the tilt and
silently throws the fundamental's target away with it - which is the wrong trade
in both directions, because the top end is lost either way and now the note is
too. Measured, that clamp turned the coupled open low E's 8.97 s target into a
realised 0.099 s at String Age 1.0, and every coupled string into an 8-to-53 ms
click under the bridge hand.

What can be given up is the tilt. A high-frequency target equal to the
fundamental's is always solvable - the ratio is one, the pole is at zero, the
filter is unity everywhere and the loop gain is exactly the fundamental's, which
is below one for any positive T60 - so a bracket always exists, and feasibility
is monotone in the high target because a gentler tilt only moves the pole toward
zero. The engine bisects that bracket for the darkest realisable filter that
still leaves the fundamental where the reference put it. This is where the
coupled solve genuinely differs from the played one: a played string's solve
carries the bridge-hand loss dip inside the same loop and backs *that* off
instead when the pair does not fit, because the dip is the term the references
put a tolerance on. The two paths share the loss law, the ratio law and the
bisection; they do not share which term gives way.

The bus is read one sample late, which makes the result independent of
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

Because the mechanism is physical, the muting hand is too. A palm-muted
passage puts the heel of the hand across every string, so the style damps the
coupled strings with a per-sample contact loss and cuts the injection at the
same time. That is what keeps a Drop-E chug tight instead of washing it in
open-string ring.

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

That framing is not cosmetic. The previous model applied the hand as a
minimum on the fundamental's T60 and then multiplied *that* result by the
string's high-frequency ratio. With the wound strings' corrected ratio - around
0.035 - a half-second mute target implied a seventeen millisecond
high-frequency target, and a muted power chord collapsed 36 dB inside its first
25 ms. Measured against dry muted power-chord reference recordings, a real short
muted chord falls 2 dB over that span and takes about half a second to reach
-40 dB; a looser one holds a low tail for seconds. The old behaviour was an
impulse where the reference is a note.

The Palm Mute keyswitch style and the continuous Palm Mute pressure are the
same absorber at different depths, and they combine in parallel with each
other as well. The style's reference-derived targets are 2.60 s to 0.32 s
across the Mute Damp control - the firm end of that travel is the tight chug
the former dedicated Chug keyswitch provided - and 4.0 s to 0.080 s across
the continuous pressure; the pressure also multiplies the high-frequency
ratio by `1 - 0.38 p`, because the heel of the hand is a soft, lossy contact.
Zero pressure leaves `T60_hand` at zero and the parallel combination is
skipped outright, so an unmuted string is bit-for-bit what it would be
without the feature.

The important detail is that this runs through the ordinary loop-filter solve
rather than as a gain after the fact. The one-pole is re-bisected against the
new decay targets and the loop delay subtracts the new analytic phase delay at
the fundamental, so a heavily muted string sounds the played pitch instead of
drifting sharp as a naive extra damping filter would make it.

## The dead note

The bridge hand and the fretting hand are two different contacts in two
different places, and Electry keeps them apart for the reason the palm-mute
work established: where a contact sits decides which modes it can take energy
from. The heel of the bridge hand rests a tenth of the sounding length from the
saddle, where the fundamental barely moves, which is why its loss is relieved
by a factor of twenty-two at the fundamental and multiplied by three at the
high fitted point. The fretting hand laid flat across the strings is neither
near the bridge nor a single point: it takes the fundamental as hard as it
takes everything else.

So the dead note is a broadband 30 ms loss added in parallel with the string's
own at both fitted points, with none of the mute's relief and none of its loss
band. Being a loss in the loop rather than a gate on the output matters twice
over. The pick's attack is untouched - a dead note's peak sits within 1.1 dB of
the same note picked open - and the note then decays through the loop filter
the ordinary solve produced for it, so it is still falling rather than being
cut. Nothing of the fretted pitch is left after 150 ms: every partial through
the eighth sits more than 40 dB under the same partial of the picked note.

The two contacts combine in parallel with each other as everything else in this
model does, so a dead note played under palm-mute pressure gets both.

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
scales the delay target by `1 / (1 + k E)`, with `k` from the gauge axis.
The envelope's release tracks hundreds of milliseconds, so a hard attack
starts audibly sharp and relaxes to true pitch, which the regression suite
verifies directly. This is the published phenomenon in its efficient form,
not the exact string-elongation integral.

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
  directions, upstrokes sit slightly closer to the bridge and brighter, and
  hammer-ons are wider, darker, and fingered. The stroke composes with every
  picked play style - an upstroke palm mute keeps the mute's hand with the
  upstroke's geometry - while a hammered note, having no plectrum, takes no
  stroke colour at all.
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

Pitch bends belong to the wheel: the strings glide to the wheel's target
along the Bend Time control, exactly as a fretting hand or a bar arm takes
time to travel, and each string moves by its own compliance (the section on
the pitch-wheel bar below). Hammer-on/pull-off onto a sounding string
retargets the same loop over about 10 ms without clearing its state, so the
vibration genuinely continues.

## The touching finger

A finger laid lightly across a vibrating string does not terminate it; it
removes energy in proportion to how much the string is moving underneath it.
Mode `n`'s displacement at a fraction `p` of the sounding length goes as
`sin(n pi p)`, so the energy a light contact takes per round trip goes as

```text
sin^2(n pi p) = (1 - cos(2 pi n p)) / 2
```

That is a comb in harmonic number, and condensing it into the single delay
loop makes it a comb in the delay line as well. Writing `M = p * period` in
samples and `omega_n = 2 pi n / period`, the round-trip loss factor is
`1 - d (1 - cos(omega_n M)) / 2`, which is the magnitude of

```text
H(z) = (1 - d/2) + (d/2) z^-M
```

to first order in `d`: unity where `omega M` is a multiple of `2 pi` (the touch
is on a node of that mode) and `1 - d` where it is an odd multiple of `pi` (the
touch is on an antinode). Electry uses that filter directly. Both coefficients
are non-negative and sum to one for `d` in `[0, 1]`, so the magnitude is
bounded by one at every frequency and every depth, which is what makes it safe
inside the string's feedback loop; the regression suite checks that bound
against the closed form.

At an exact node position `p = 1/k` the filter is unity in magnitude **and**
phase at every surviving partial - `exp(-j 2 pi n / k) = 1` whenever `k`
divides `n` - so the harmonic series above the node keeps both its levels and
its tuning, and no phase compensation is needed. That is the reason the natural
harmonic is produced this way rather than by transposing the note: the loop
still runs at the fretted pitch, so the sounding octave carries the fretted
string's inharmonicity, its solved decay targets and its pickup-comb geometry
rather than those of a string half as long.

The finger lifts once the note has formed. The partials it removed are gone by
then and the excitation is over, so nothing can put them back; releasing the
touch therefore costs nothing audible and stops paying for two extra delay
reads per sample. It also removes the clamp that used to stand in for the
finger - the harmonic style capped T60 at 3.8 s - which on the open A2 was
charging the octave partial 16 dB per second of loss that no physical
mechanism was asking for. A natural harmonic now outlasts the fretted note, as
it does on the instrument.

Away from a node the filter is still bounded and still shaped by the same
mode-shape law, but no partial is perfectly preserved and the surviving one
carries a little extra loss and a little phase. That is physically right - an
artificial harmonic taken off a node is weaker, dirtier and shorter - and it
means such a harmonic's pitch is a function of where the hand is rather than a
member of the equal-tempered scale. It is left that way rather than quantised.

The pinch harmonic is the same filter driven by the other hand. The picking
hand's thumb catches the string immediately after the pick, at the pick's own
position, so `p` is the pluck fraction and Pick Position chooses which partial
squeals - which is exactly what moving the picking hand does on the instrument.
Measured on a fretted E3 with the pick at its 18% default (a touch at 0.120 of
the sounding length, so the nearest node belongs to the eighth partial), the
strongest surviving partial is the eighth or ninth and it gains 15 dB on the
fundamental against the same note picked ordinarily; the note's energy-weighted
mean partial index moves from 4.0 to 7.4. With the picking hand over the neck
the touch sits at the clamped 0.49 of the string and the squeal is the octave,
mean partial 2.5.

The thumb is modelled as a firmer contact than the fretting finger - depth 1.0
against 0.92 - and it stays on the string for 90 ms rather than 45, because the
mode-shape law gives a contact this close to the bridge little purchase on the
low partials: at a tenth of the sounding length `sin^2(pi/10)` is 0.095, so the
fundamental loses about seven per cent of its energy per round trip where a
midpoint touch would take nearly all of it. The fundamental therefore survives
a pinch far better than it survives a natural harmonic, which is what the
physics says and what the instrument does; the squeal reads as a squeal because
it is 15 dB up on the fundamental in relative terms and because the amplifier
that a pinch is normally played through compresses the difference further.

## The slide

Pakarinen, Puputti, and Välimäki's
[*Virtual Slide Guitar*](https://research.aalto.fi/en/publications/virtual-slide-guitar)
(Computer Music Journal 32(3), 2008, with the
[NIME 2008 companion](https://www.nime.org/proceedings/2008/nime2008_049.pdf))
makes the point Electry's slide is built on: a wound string is not smooth. The
wrap wire lies in ridges along its surface, and anything dragged along it -
their slide tube, a fingertip - is excited at the rate those ridges pass under
the contact. Electry takes that mechanism rather than their energy-compensated
time-varying waveguide, and takes the finger rather than the tube.

Two things follow. First, the duration of a slide is a distance divided by a
hand speed, not a time: the finger travels, so a twelve-fret slide takes six
times as long as a two-fret one. The Bend Time control - the same travel-time
control the pitch wheel uses - sets 8% of itself per fret, so its 280 ms
default is 22 ms per fret and the whole control spans a very fast hand to a
deliberate one. The glide is the existing legato retarget with that duration,
so the loop state is preserved throughout and the sounding length passes
through every intermediate fret.

Second, the friction. The position of fret `n` along the string is
`L (1 - 2^(-n/12))` from the nut, so the distance the hand actually covers
shrinks as the slide moves up the neck exactly as the fret spacing does; the
speed is that distance over the glide time. The winding pitch runs from about
0.36 mm on a .080 to about 0.18 mm on a .024 - much flatter than the string
diameter itself, because a heavier string is mostly a heavier core - and the
engine uses a linear stand-in fitted to that pair, which is a voicing estimate
rather than a measurement. Two one-poles form a band at `v / w`: at a
twelve-fret slide taken in 190 ms that is around 8 kHz, and at 770 ms around
2 kHz. The level follows `6 b (1 - b)`, the derivative of the glide's own
smoothstep, so the squeak swells and dies with the movement and is exactly zero
when the finger is still.

A plain string has no winding, so its friction is a tenth of a wound string's
and the band is fixed. The Finger Noise control - the fretting hand's own
contact level - scales the whole thing and silences it exactly at zero.

The friction is routed like every other contact noise in this engine: a
one-percent trace into the string and the rest as a local pickup and body
transient. That matters for how it is measured, and the measurement is worth
recording because it is the reason the regression suite asserts the band at
the source rather than at the output. The loaded pickup coil is a second-order
low-pass at a couple of kilohertz, so it flattens most of the difference
between a two-kilohertz squeak and an eight-kilohertz one before either
reaches the output. That is the instrument behaving correctly - a real pickup
does the same thing to a real squeak - but it means an output-side centroid
measures the coil rather than the friction.

## Fret collisions

Bilbao and Torin,
[*Numerical Modeling and Sound Synthesis for Articulated String/Fretboard
Interactions*](https://www.research.ed.ac.uk/en/publications/numerical-modeling-and-sound-synthesis-for-articulated-stringfret/)
(JAES 63(5), 2015), simulate distributed string-fret contact with an
energy-balanced penalty formulation. A full FDTD contact solve is outside
Electry's per-voice budget; the Artifacts path's incidental contact instead
opens a velocity-shaped collision window on hard-picked notes in which
vertical displacement beyond a velocity-dependent clearance is soft-limited
(a bounded rational excess law) and the clipped excess re-radiates as
deterministic rattle noise. The clearance relaxes as the window decays, so
the buzz dies exactly as the displacement falls below the frets. This is
documented as collision-informed behavior, not a contact simulation.

## The pitch wheel as a vibrato bar

A bar does not transpose a guitar; it stretches it. Rocking the bridge
changes every string's speaking length by a comparable amount, and the pitch
that change buys each string follows the elastic relation `dF/F = dT/2T`
with `dT = E A dl/l` (Fletcher and Rossing): the string's elastic core
stiffness against its standing tension. For strings tuned across one scale
length that ratio reduces to `(core fraction)^2 / (mass factor * f_open^2)` -
the overall gauge cancels, because core area and tension both scale with the
diameter squared - so the slack low eighth string and the plain G are the
deep benders and the stiff wound D-string the shallow one, which is exactly
the chord smear a real tremolo bridge produces.

Electry drives the wheel through that law. The raw physical spread across
this string set is about six to one; a real bridge's geometry evens the
per-string travel out, so the spread is compressed with a 0.35 exponent
toward the roughly two-to-one range measured on hardware, then normalised so
the most compliant string spans the wheel's nominal +/-2 semitones and no
string exceeds it. The sympathetically ringing open strings are retuned in
place whenever the wheel moves - a bar bends the strings nobody is fingering
too - and the whole instrument glides to the wheel over the Bend Time
parameter, the same travel-time control the keyswitch finger bends used
before the wheel replaced them. The compression exponent is a voicing
decision; the compliance ordering and the full-range normalisation are the
documented mechanism.

## The fretting hand's vibrato

The pitch wheel is a bar: it stretches every string, the sympathetically
ringing ones included, and each answers with its own elastic compliance.
Channel pressure is the other gesture, and it is a different one in three
measurable ways rather than in degree.

It is **local**. A finger moves the string it is on, so the vibrato reaches
only the fingered voices; the bridge-coupled strings are configured from the
wheel alone and never see it. The regression suite pins both halves of that -
a coupled open low E whose delay target does not move under full pressure, and
the same fixture under a full wheel where it moves by more than a sample, so
the first check is proving something.

It is **one-sided**. A fretting finger raises a string's tension by pushing it
across the fingerboard; it cannot lower the tension below what the fret sets.
The oscillation is therefore a raised cosine, `d (1 - cos)/2`, whose minimum is
the fretted pitch rather than its mean, and the note is only ever pushed sharp.
A symmetric vibrato would be a bar's, or a violinist's.

It is **equal in semitones**, deliberately, where the bar's is not. A bar
stretches every string by the same length and each string answers with its own
`dF/F = dT/2T`; a finger is aiming at a pitch and adjusts its displacement
until it gets there. Compliance-weighting a fingered vibrato would make the
low strings wobble more than the high ones, which is not what a hand does.

The depth is 40 cents at full pressure and the rate runs 4.8 to 6.4 Hz, faster
as the player leans in. Both are ordinary rock-vibrato values rather than
derived ones, and are labelled as voicing. The depth eases in with a 90 ms time
constant, because a player lands a note before starting to move on it; the
suite measures that the first 60 ms carries less than 40% of the settled
movement. At zero pressure the offset is exactly zero and the render is
bit-for-bit identical to one from an engine whose pressure control was never
touched.

## Amplifier feedback through the acoustic return

A guitar in front of a loud amplifier is a closed loop: the loudspeaker's
pressure field pushes the strings, each string answers at its own
resonances, and with enough level the loop regenerates into the singing
feedback that is part of the instrument's vocabulary. Electry closes that
loop explicitly. The host pushes its previous processed block back into the
engine as a bounded mono acoustic return - one block of latency standing in
for the speaker-to-string air path - and a soft-clipped, gain-scaled copy of
it drives the fingered string loops and the sympathetic bus.

Three factors scale the injection, and all three must be up for the loop to
regenerate: the CC1 resonance wheel (squared, so half a wheel blooms while
the top of the wheel howls), the Resonance Depth parameter (the wheel's
full-scale reach), and the rig's acoustic loudness, which the plug-in shell
derives from its amplifier and distortion controls. The last factor is the
physical honesty of the model: the effect chain manages its own listening
level, but in the room a cranked amplifier is deafening while a clean DI is
not audible at all, and it is the room level that decides whether the
strings can feed. Every element of the loop - the rational soft clip on the
return, the strings' own contractive loops, the coupled strings' saturation
and the output guard - is bounded, so the howl saturates into a stable
singing tone instead of growing, and it decays the moment the wheel closes
or a hand lands on the strings. With the wheel at zero the stored return is
never injected, so the path is bit-exact absent.

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
decay rather than radiating it. Electry feeds bridge motion and contact
noise into four modal resonators whose frequencies, Q, and level
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
- **Cabinet.** Six biquad sections inside the oversampled domain: a
  second-order high-pass at the box frequency (a sealed cabinet has no useful
  output below it), a low-mid thump, a scooped boxy mid, a presence peak, and a
  fourth-order Butterworth roll-off (two cascaded sections) from 5 kHz, because
  a twelve-inch speaker is essentially gone an octave above that. Running it
  before decimation rather than after removes the alias-generating content
  first. The regression suite
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

- **Power stage.** Two preamp triode ceilings into a filter cabinet is the
  front half of an amplifier, and the two mechanisms that make a real one
  answer to how hard a part is played both live in the back half.

  *Supply sag.* The current the output stage draws follows its own output, not
  its grid signal, so the follower reads the stage's own last sample. It
  attacks in 70 ms and recovers over 400 ms - a reservoir discharges far faster
  than it recharges, which is the whole character of the effect - and the rail
  droops by up to 28%, the 350 V to 250 V a real supply measures. What the rail
  sets is the *headroom*, not the gain, so the stage is
  `droop * triode(u / droop)`: the transfer curve is scaled uniformly in both
  axes, the small-signal slope is exactly unchanged, and the ceiling falls in
  proportion. Measured on a held 375 Hz tone, a loud passage ducks 1.16 dB
  between 16 ms and 640 ms in while a quiet one ducks 0.22, and the level
  returns within 1.1 dB after a second and a half of rest. A droop of one is
  the identity, so a quiet passage is bit-for-bit what it was.

  *Output transformer.* A core saturates at a flux limit, and flux is the
  integral of the voltage, so the limit is a volt-second limit: at the same
  level the low end reaches it long before the top does. A one-pole at the
  45 Hz primary-inductance corner is that integral normalised - unity at DC,
  falling as 1/f above it - and the excess the core cannot carry is subtracted
  back out, which leaves the stage transparent well above the corner and
  compressing and thickening underneath it. A second-order high-pass at 26 Hz
  in front is the transformer's own inability to pass DC, and it also keeps the
  bias drift's residue out of the flux.

  The transformer is measured at the stage rather than at the chain's output,
  and the reason is worth recording because it is a real limit on what an
  output-side measurement can say here. The cabinet's second-order high-pass at
  the box frequency shapes a low tone and its harmonics so differently from a
  mid tone and its own that a distortion figure taken after it is worth about
  nine decibels of bias in the effect's own direction - more than the effect
  measured through it. At the stage the picture is unambiguous: a full-level
  tone distorts at -25.3 dB at 48 Hz, -71.5 dB at 480 Hz and -130.9 dB at
  4.8 kHz, a fall of about 46 dB per decade that is the cubic-in-flux law seen
  directly, and a tone 24 dB quieter distorts 41 dB less at 48 Hz.

  The alias floor pays for the sag, because a drooping rail pushes the stage's
  argument further into saturation: the amplifier at full drive moves from
  -86 dB to -69 dB, still comfortably inside the suite's -60 dB bound. The
  alias fixture itself needed lengthening, and that is worth naming as a
  measurement error rather than a model one: with the two settling passes it
  used before, the 0.7% of sag still converging inside the analysed window
  smeared enough energy off the harmonic bins to read as a -37 dB alias floor
  that was not aliasing at all.

What is deliberately not claimed: no circuit is solved (no Wave Digital
Filters, no nodal state-space, no K-method), the power stage is a behavioural
model of sag and core saturation rather than a solved supply or a measured
transformer, the cabinet is not a measured impulse response, and none of it is
a model of any named amplifier or speaker.

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
the chug voicing (a dedicated Chug keyswitch then; since 1.2 the firm end of
the Palm Mute style's Mute Damp travel) was actually *brighter* than an open
note - a hand on a string cannot do that. The bridge hand now also darkens the excitation: the release
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

### The mute varies over the note

Five mute mechanisms were built and compared on the same figure, and the choice
between them was made by ear because the measurement could not make it: on the
shipped instrument the five score 6.22 to 6.29 dB on the joint objective, a
spread of 0.07 dB against the roughly one decibel that separated things that
mattered elsewhere. What ships is the pair that was preferred, at a measured cost
of 0.05 dB against the best of the five - which is noise.

Both are ways the loss varies with time rather than with frequency, and they act
at opposite ends of the note, so they multiply rather than compete:

- **The contact settles.** The heel's contact area grows over the first 40 ms, so
  the attack rings briefly before the mute takes hold. A function of note age.
- **The grip slackens.** As the string stops pressing into the hand the loss
  falls back, so the tail opens up instead of staying clamped. A function of how
  hard the string is still driving the contact, tracked by a one-pole follower on
  the loop itself.

Both re-push a scaled depth into the loss band and leave the solved loop gain and
damping pole alone, so at a factor of one they are exactly the static model and
the fitted decay still holds; away from one the decay departs from the fit
deliberately, and that departure is the behaviour.

Three mechanisms were measured and rejected. A **second loss band** at thirteen
times the fundamental, aimed at the h7-h8 shortfall, scored 6.09 against 5.99 -
no better, and it puts another biquad in the loop. **One much wider band** at
Q 0.32 was clearly worse at 6.98, so what is missing is not reach. And an
**absolutely-referenced** version of the grip model is worth recording in full,
because it is the one that should have worked and did not.

The premise was that a real mute reads tight when struck hard and loose when
struck soft, which no depth-independent model can produce from one setting, and
which the references demand - the same note's two takes differ by 11 to 17 dB at
400 ms. The grip model as shipped normalises the envelope by the note's own peak,
which is scale-invariant: measured, its velocity spread matched the static model
to within 0.1 dB, so it does not do this at all. Referencing an absolute level
instead does change the spread, but *narrows* it - 0.52, 0.71, 1.07 dB against
the static model's 0.94, 1.18, 1.33 - and costs 1.45 dB on the joint objective.
Swept across references from 0.0005 to 0.3 the score is monotone: its only good
operating point is the one where it degenerates into the model without it. The
tight-versus-loose distinction remains unrepresented, and this was a real attempt
at it rather than an untried idea.

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
- **Articulations that cost nothing when they are not playing.** The node
  touch's two extra delay taps run only while a finger or thumb is on the
  string, and the finger lifts once the note has formed rather than staying
  down for the note's length. The slide's friction band is generated only while
  the finger is moving and its amplitude is cleared the moment the glide
  settles. The vibrato adds nothing per sample at all; it moves the same delay
  target the wheel does, at a fraction of the wheel's excursion. What is left is
  three float comparisons per voice per rendered sample, all of them false for
  an ordinary note. Measured best of five against the sources before this work,
  the default Bridge + Mono eight-string render moved from 0.179x to 0.189x
  realtime and the worst-case Both + Stereo from 0.220x to 0.221x, against a
  run-to-run spread on the same machine of around ten per cent - so the worst
  case is inside the noise and the default costs a few per cent. Moving the new
  per-voice fields to the end of the voice struct rather than into the middle of
  it was worth about half of that.

Measured with an eight-string Drop-E chord held for two seconds, best of five
runs, comparing 1.0 and 1.1 sources built identically: the default
Bridge/Mono configuration went from 0.28x to 0.15x realtime at 96 kHz, the
worst-case Both/Stereo configuration from 0.27x to 0.19x, and an idle engine
from 0.013x to 0.003x. The regression suite prints both eight-string ratios on
every run; the culling itself is asserted structurally, by reading the engine's
own culling and link flags for every selector position, because the few-per-cent
wall-clock saving proved smaller than the run-to-run spread of a shared CI
runner and a timing assertion on it was flaky.

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
all twelve pick-stroke/play-style combinations at 44.1-384 kHz; the 2x/1x
internal-rate policy, exact
host-to-physical clock timing, and filtered-decimation pitch stability;
exact-silence idle output;
sample-identical renders for identical MIDI (including across engine reuse,
which caught a real aperture-state leak during development); fundamental
accuracy within 8 cents across E1..D6 at three rates; stable allpass bounds
and under-20% low/high dispersion-deficit fit error on the heavy short-scale
Drop-E case; positive bounded modal conductance and exact structural-loss
bypass at 0%; independent latching of the two keyswitch banks, keyswitch
silence, dead-zone and range gating; the alternate sequence surviving style
changes and skipping hammered notes; measurably distinct attack spectra and
levels for down, up, hammered, muted and harmonic playing, an audibly
composed upstroke palm mute, a stroke-independent harmonic octave, and a
bit-identical hammered note under either latched stroke; a node touch that
removes the odd partials by more than 20 dB while leaving the even ones, a
touch filter whose closed-form magnitude never exceeds one at any depth, an
exactly absent touch on every other articulation, a finger that lifts, and a
harmonic whose octave partial decays within 2 dB of the same partial of the
ordinarily picked note - the direct evidence that the loop is no longer
retuned; a pinch harmonic whose energy-weighted mean partial index sits well
above an ordinary pick stroke's, whose strongest partial is at least the sixth
near the bridge and exactly the octave with the hand over the neck, which gains
more than 10 dB on the fundamental, and which renders as neither a pick stroke
nor a natural harmonic; palm-mute decay
contraction; the per-string wheel-compliance table's physical ordering and
the rendered audio following it on two strings; wheel travel time following
Bend Time with an exact settle on the target; a ringing coupled string
retuned by the wheel; hammer-on
same-string continuation, pitch settling, and click-free transition; a
sharp-to-true attack tension glide between 0.4
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
a decay envelope that agrees across 44.1, 48, 88.2, 96 and 192 kHz to within
3 dB in every window through 1.5 s, a polarisation exchange that is one number
per round trip at every pitch with neither of its clamps engaged, a 22nd-fret
high E that still sustains on 44.1, 48 and 96 kHz hosts, a coupled ring whose
kilohertz band sits 60 dB or more under its low band, and coupled loops whose
realised round-trip decay - read back from the gain and coefficient they
actually run - holds its fundamental target at String Age 1.0 and at half and
full bridge-hand pressure, to the same value on 44.1, 48 and 96 kHz hosts;
monotonic palm-mute decay contraction, an exact no-op at zero pressure, an
in-tune heavily muted string, the solved loop coefficient actually moving, and
CC 2 pressure including hostile input; strum travel offsets in physical string
order, an undelayed leading string, a lower stacked chord peak, a fresh stroke
outside the chord window, and no premature retirement of a delayed string;
the CC1 resonance lifting the sympathetic ring with Resonance Depth scaling
its reach and a bit-exact bypass at a lowered wheel; the closed
engine-amplifier loop self-sustaining after note release at full wheel and
distortion while the same loop decays with the wheel down or the amplifier
dry, all bounded; pluck position following the fretted sounding length by 2^(fret/12);
fretboard geometry, meter ballistics, standing-wave shape, colour knee and a
lossless packed audio-to-editor round trip; per-string display readout naming
the right string, fret, note and articulation; selector-driven pickup culling,
click-free restoration of a culled pickup, Mono channel linking and a
click-free stereo-field opening; exact digital silence from an untouched
engine, a subnormal-free ring-out that reaches exact zero, and a clean wake
from the frozen state; contrasting construction
endpoints that both stay in tune; plectrum contact noise in the pre-attack
window; release noise that appears only after note-off; a dead note that lands like a
picked one, leaves no partial of the fretted pitch after 150 ms, and decays
through its own loop rather than being gated; eight-string
polyphony with open-position chord mapping, repick reuse, and stealing; a
slide whose pitch travels through the intermediate semitones rather than
jumping, whose travel time scales with the interval, whose friction band
follows the speed of the hand, which is far louder on a wound string than on a
plain one and exactly absent at a silent Finger Noise control; a
fretting hand that keeps a lead phrase in one position instead of falling back
to open strings, leaves the open-position shapes untouched, and relaxes to the
nut when the phrase ends;
pitch-wheel travel and sustain-pedal hold; a
fretting-hand vibrato at the modelled depth and rate that is sharp of the fret
and never flat of it, eases in rather than switching on, leaves the
bridge-coupled strings alone where the bar moves them, and is a bit-exact no-op
at zero pressure; hostile parameter and performance
input safety; and a portable CPU ceiling with the eight-string render ratio
printed on every run in worst-case Stereo, maximum Body Resonance, and maximum
Artifacts mode. Mono is checked sample-for-sample dual mono; Stereo tests pin
physical low/high string orientation, coherent fold-down, bounded side level,
energy balance, determinism, and opposite string endpoints. The plug-in suite
additionally pins the 31-parameter
contract, formatted values, state round-trips including a pre-1.1 session that
picks up the new defaults, bus layout, sample-accurate
note starts, MIDI controller behavior (sustain, all-sound-off,
all-notes-off), UI keyswitch triggering of both banks, panic, output-gain and APVTS
output-field effects, two visible non-overlapping mode buttons,
the sympathetic, palm-mute (parameter and CC 2) and strum-spread controls
reaching the rendered audio, offscreen editor rendering including the live
fretboard's bounds, and prepare/release cycles at three rates.

The amplifier chain has its own suite: halfband unity DC gain, the -6 dB
halfband symmetry point, passband ripple and stopband rejection; a bit-exact
dry bypass with every control at zero and an audible effect from each control
on its own at 100%; the alias floor of the pedal, the amplifier, and the two
stacked, at two input levels; a supply that droops on a loud sustained passage
and far less on a quiet one, develops over its modelled time constant and
recovers during a rest; an output transformer whose distortion falls about
46 dB per decade of frequency and 41 dB for 24 dB of level, measured at the
stage rather than through the cabinet; each of the cabinet's five voicing features
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

Fourteen rendered examples of the whole path are committed under
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
