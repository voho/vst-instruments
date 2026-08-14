# Electry

Electry is an original, physically modeled dry electric guitar instrument.
Eight string voices run dual-polarisation waveguide loops with physically
derived stiffness dispersion, decay-targeted damping, tension-modulation
pitch glide, a fretting hand with a position and a reach that decides where
each note is played, a point-touch model that produces natural and pinch
harmonics as the string's own mode shapes rather than as transpositions,
slides whose length is a distance over a hand speed and whose squeak is the
winding passing under the finger, a picking hand that never puts the plectrum
down in exactly the same place twice, a pitch wheel that bends every string like
a vibrato bar, channel pressure that is the fretting hand's own one-sided
vibrato with a separate finger on each string it is actually stopping, a
resonance wheel that can push a distorted tone into self-sustaining
amplifier feedback, one bridge shared by all eight strings — the ones you are
not fingering ring off it and the ones you are exchange energy through it — and
a published pickup signal structure (position comb, two coils for the humbucker
and one for the single coil, finite magnetic aperture, nonlinear flux, induced
EMF, and loaded coil resonance). The performance is selected with two
independent banks of latching keyswitches below the playable range - one for
the picking style, one for the play style - so any stroke can drive any
style. The individual models have named research references
(see the [physical-modeling research
contract](Docs/physical-modeling-research.md)); Electry does not claim to be
a capture-accurate clone of any one instrument.

The compact FX panel provides a distortion pedal, an amplifier and modelled
cabinet, compression, lead delay, and a stereo room; every effect defaults to a
true 0% dry setting, and the two clipping stages, the power supply's sag and the
output transformer's core all run inside a 4x oversampled domain so a high-gain
metal tone saturates instead of folding its own harmonics back into the guitar
band. Mono is the authentic summed dry DI; Stereo is a
phase-coherent divided-pickup view of the eight physical strings, not an effect
or delay. Both are also ready for the amp simulation of your choice. Material,
body, pickup, and construction controls span deliberately contrasting
solid-body anchors; scale length spans a conventional 25.5-inch electric to a
modern 28-inch baritone/8-string build.

![Electry electric guitar interface](Docs/screenshots/electry-standalone.png)

## Contents

- [Hear it](#hear-it)
- [Keyswitches and playable range](#keyswitches-and-playable-range)
- [Sound architecture](#sound-architecture)
- [Amplifier chain](#amplifier-chain)
- [Guitar construction axes](#guitar-construction-axes)
- [Exact 31-parameter contract](#exact-31-parameter-contract)
- [Build products](#build-products)
- [Requirements](#requirements)
- [Build on macOS](#build-on-macos)
- [JUCE-free DSP build](#juce-free-dsp-build)
- [Install and validate locally](#install-and-validate-locally)
- [Sign, package and notarize](#sign-package-and-notarize)
- [Project layout](#project-layout)
- [Licensing](#licensing)

## Hear it

Fourteen rendered examples — the full playable range, every pick-stroke and
play-style combination including the pinch harmonic, the slide and the dead
note, the guitar-build and pickup axes, the pitch-wheel bar, the fretting
hand's vibrato and the resonance-wheel feedback, and the Drop-E rhythm and
lead tones dry and through the amplifier — are committed under
[`Docs/audio/`](Docs/audio/README.md), with the score for each one in
`Tools/RenderDemos.cpp`. They are produced by the shipping JUCE-free signal
path, so they cannot drift away from what the plug-in sounds like, and they are
reproducible on any platform with a C++20 toolchain.

The screenshot is produced by the plug-in regression suite itself
(`ELECTRY_EDITOR_SNAPSHOT`), so it is always a real editor render rather than
a mock-up; the Nightly workflow re-renders it on every run and commits it
when the editor has changed. The Standalone, VST3, and
Audio Unit use that same
JUCE component. Panels, knobs, the fretboard, the keyswitch strips, and the
on-screen keyboard are drawn as resolution-independent JUCE graphics;
interactive controls stay native for automation, keyboard operation, and
accessibility.

The editor uses audible impact as its visual hierarchy: pickup/tone,
excitation, age, body resonance, and velocity are oversized in the Core row;
guitar-build controls are medium; articulation-specific noise and artifact
details are compact.
The walnut-and-amber chassis nods to a workbench electric guitar without
reproducing a branded hardware panel.

### Live fretboard

Under the play-style strip, a 22-fret eight-string fingerboard shows the model
as it plays: which physical string each note was allocated to, exactly where it
is stopped (with its note name), how hard it is ringing, and which strings are
ringing only through the sympathetic bridge coupling. Sounding strings are
amber and vibrate as the fundamental standing wave of their sounding length, so
the part of the string behind the fretting finger correctly stays still;
bridge-coupled strings are cool blue. A per-string meter on the right shows the
level with proper attack/release ballistics, and the header readout reports
both counts, for example `3 STRINGS +4 RING`.

All of the fretboard's geometry, ballistics, colour mapping and the lock-free
audio-to-editor transfer live in the JUCE-free `Source/DSP/ElectryVisuals.*`
module, so they are unit-tested on every platform and the editor stays a thin
renderer. The PERFORMANCE panel beside the fretboard holds the four controls
that change what it shows.

> **Just want to try it?** The scheduled Nightly workflow publishes the
> latest successful universal build from `main` to the rolling
> [nightly release](https://github.com/voho/vst-instruments/releases/tag/nightly).
> The bundles are ad-hoc signed and not notarized; check the repository's
> Nightly badge for the latest workflow result.

## Keyswitches and playable range

MIDI notes 12..21 are two independent banks of latching keyswitches; they
never sound, and each bank keeps its most recent selection for every
following note until changed. Notes 12..14 (C0..D0) latch how the pick
moves; notes 15..21 (D#0..A0) latch what the hands do. The two latch
independently, so any of the twenty-one combinations — an up-stroke palm mute,
alternate-picked pinch harmonics — is reachable in at most two keyswitches.
Keyswitch note-offs are ignored. The editor's PICK STROKE and PLAY STYLE
strips send the same keyswitches and always show the currently latched pair.
The on-screen keyboard colours and labels both keyswitch banks separately
from the playable instrument.

| MIDI note | Key | Picking style |
| --- | --- | --- |
| 12 | C0 | Down (default) |
| 13 | C#0 | Up — opposite displacement polarity, slightly closer to the bridge, thinner and brighter |
| 14 | D0 | Alternate — starts down, then alternates down/up for each accepted picked note-on; hammered notes neither take nor consume a stroke |

| MIDI note | Key | Play style |
| --- | --- | --- |
| 15 | D#0 | Sustain (default) — the ordinary ringing pick |
| 16 | E0 | Palm mute — damping follows the Mute Damp control, from a loose half-mute to a tight metal chug at the firm end |
| 17 | F0 | Hammer-on / pull-off — continues a sounding string legato within a nine-fret reach, fingered attack, no plectrum noise |
| 18 | F#0 | Natural harmonic — a finger resting on the string's midpoint node, so the octave is what the string does rather than a transposition of it |
| 19 | G0 | Pinch harmonic — the picking hand's thumb catches the string at the pick's own position, so Pick Position chooses which partial squeals |
| 20 | G#0 | Slide — the finger stays down and travels; the travel time is a distance over a hand speed, and the winding scrapes under it the whole way |
| 21 | A0 | Dead note — the fretting hand lies across the strings without pressing them to the fret, so the pick lands normally and no pitch survives |

Notes 22..27 are ignored, and notes 28..86 are playable on a 22-fret,
eight-string Drop-E instrument tuned
E1-B1-E2-A2-D3-G3-B3-E4; notes outside these ranges are ignored. Each note is
allocated to one of the eight physical strings by a fretting hand that has a
position and a reach: a repick of a sounding note grabs the same string,
hammer-on continues the nearest sounding string, otherwise the free string
that puts the note nearest the fingers wins, and when everything sounds, the
oldest string is stolen. The hand's index finger covers four further frets,
open strings need no finger and are free at the nut, and the hand moves only
when the note it is asked for lies outside its span - and then only on the
first note of a chord, because a chord is one hand shape. It relaxes back to
the nut when the phrase ends. That is what lets a phrase played up the neck
stay up the neck: the rule this replaced always chose the lowest fret that
could produce the note, so an open string won every contest and most of the
fretboard - and the sounding length, inharmonicity and pickup-comb geometry
that come with it - was unreachable.

The pitch wheel is a vibrato bar: it bends every string — the
sympathetically ringing open strings included — over a nominal ±2 semitone
range, each string by its own physically derived compliance (the slack low
E1 and the plain G bend deepest, the stiff wound D-string least, the
two-to-one smear a real bar puts on a chord), and the strings travel to the
wheel over the Bend Time parameter rather than snapping. Channel pressure is
the other half of that distinction: it is a finger rather than the bar, so it
leans only into the strings the hand is stopping, pushes them sharp and never
flat, and leaves an open string exactly where it is — nothing is holding an
open string down for the hand to rock, and reaching it would need the bar. It
is a hand rather than an oscillator: every stopped string gets its own finger,
with its own phase and its own rate and excursion redrawn each cycle, and the
hand leans into and out of the gesture from rest instead of at full slew (see
**Fretting-hand vibrato** below). The modulation wheel (CC 1) is the
performance resonance: it raises the sympathetic coupling from the
Sympathetic Ring parameter toward total and opens an acoustic feedback path
from the amplified output back into the strings, scaled by the Resonance
Depth parameter — with the amp or distortion up and the wheel raised, a held
note regenerates into self-sustaining feedback, and a dry DI never can. The
sustain pedal (CC 64) holds released strings; breath/CC 2 adds continuous
bridge-hand damping on top of the Palm Mute parameter; CC 120/123 behave as
All Sound Off and All Notes Off.

## Sound architecture

- **Strings:** one voice per physical string. Each voice runs two
  single-delay-loop waveguides (the two transverse polarisations) with
  third-order Lagrange fractional delays, a one-pole damping filter solved
  from per-string decay targets, an eight-stage factored allpass dispersion
  cascade fitted at low and high partials from the string's physical
  inharmonicity (diameter, effective wound core, scale length,
  tension), and a contractive bridge coupling. The horizontal polarisation
  decays 1.7x slower and is detuned by a fraction of a cent, giving the
  natural two-stage decay and slow beating. Both that detune and the exchange
  between the two polarisations are fractions of a round trip rather than fixed
  numbers of samples, which is most of what makes the decay envelope the same
  at every host rate: charged per rendered sample instead, they left the
  22nd-fret high E 36.5 dB under its own attack half a second later on a
  44.1 kHz host and 14.5 dB under it on a 96 kHz one, and dissipated the
  mismatch between the two loop lengths as loss rather than exchanging it -
  36 dB of the top string's sustain, against its own fitted decay target. It is
  most of it rather than all of it, and the remainder is worth naming: the loop
  filter itself is a one-pole interpolating between two fitted frequencies in
  normalised radians, which is not exactly rate-invariant. The 22nd-fret high E
  measures 20.0, 19.3 and 15.5 dB under its attack at 1-2 s on 44.1, 48 and
  96 kHz hosts - a 4.5 dB residual spread, against 32.7 dB before. The regression
  bound is 3 dB through 1.5 s and 8 dB through 3 s. Loop-filter phase is
  compensated analytically, holding the fundamental within a few cents
  across the fretboard at 44.1-384 kHz. At host rates through 96 kHz the
  complete physical and nonlinear signal path runs internally at 2x and is
  returned through a 63-tap halfband FIR; higher-rate hosts run natively.
- **Pick position:** the picking hand stays at a fixed distance from the
  bridge; it does not follow the fretting hand up the neck. Pick Position is
  therefore a fraction of the *open* string, and the pluck comb as a fraction of
  the sounding length grows by `2^(fret/12)`, so a note fretted high up moves
  toward the hollow, mid-string comb of a real guitar. At the nut this is
  identical to the previous behaviour.
- **Pick release:** the plectrum does not leave every string equally quickly. A
  wound .080 carries far more mass per unit length than a plain .009 at
  comparable tension, so it leaves the pick more slowly, and the duration of
  that release low-passes what enters the string. The corner follows the square
  root of the string's own open frequency, so the treble register keeps its
  brightness while the wound low strings lose the upper-partial surplus the
  ideal law gives them, and the pole's own attenuation at the fundamental is
  divided back out so the release governs timbre rather than level.
- **Pick contact:** the plectrum is neither a point nor symmetric. It touches
  the string over a patch — half a millimetre for a stiff sharp pick, around a
  millimetre and a half for a soft rounded one — so the reflected image of the
  excitation is spread over that width through the same sounding-length
  geometry the comb uses, and the comb notches wash out with frequency instead
  of staying razor sharp up to Nyquist. Its release is asymmetric: the string is
  drawn aside over most of the contact and then slips off in a fraction of that
  time, and a stiffer pick lets go later and more abruptly. Both halves of the
  window are smoothsteps, so its area is exactly what the symmetric raised
  cosine it replaced had, and the asymmetry changes the attack's spectrum
  without changing how hard the note lands.
- **Stroke-to-stroke variation:** a hand does not put the plectrum down twice in
  the same place, and four quantities are drawn fresh for every attack: where
  along the string the pick lands (4 mm of standard deviation, about 5% of the
  pick-to-bridge distance at the default Pick Position, so the pluck comb's
  first notch moves by about as much), how hard it is driven (0.6 dB), how far
  off the plane it is held (6 degrees, which is not a free parameter but exactly
  the split between the two polarisations, so it is applied as a rotation of
  that split and takes no energy with it), and how much of the tip is touching
  (8%, carried by the release pulse length). Each draw is three summed uniforms,
  so it has unit variance exactly and cannot leave ±3 sigma — the bound the
  plectrum's own width and the hand anchored on the bridge set. The variation
  rides on top of the up/down stroke colouring rather than replacing it, so
  alternate picking gets it too, and the angle alone is skipped for a hammer-on,
  which has no plectrum to hold at an angle. Measured on twelve identical note-ons twelve
  seconds apart with the noise controls and Artifacts at zero, successive
  strokes differ over their first 150 ms by -16.5 dB on the mean where they
  differed by -84.6 dB before, peak level spreads 2.49 dB where it spread
  0.012 dB, and the attack's spectral centroid spreads 17.2 Hz on a 456 Hz
  centroid where it spread 0.38 Hz. Every draw is a pure function of the note's
  index and the string, so identical MIDI still renders identical audio.
- **Touch harmonics:** a light finger on the string is a point loss, and mode
  `n`'s displacement under it goes as `sin(n pi p)`, so the energy the contact
  removes per round trip goes as `sin^2(n pi p)`. Condensed into the single
  delay loop that is exactly a one-tap FIR, `(1 - d/2) + (d/2) z^-M` with
  `M = p * period` — unity where the touch sits on a node, `1 - d` where it
  sits on an antinode. Both coefficients are positive and sum to one, so it
  can never exceed unity gain inside the string's feedback loop. The natural
  harmonic rests that finger on the midpoint, which removes every odd partial
  including the fundamental and leaves every even one alone in magnitude *and*
  phase, so the octave arises from the string instead of from retuning the
  loop: the string keeps its own length, inharmonicity, decay targets and
  pickup comb. The finger lifts once the note has formed — the partials it
  removed cannot be re-excited — which stops paying for the two extra delay
  reads and lets the harmonic ring as long as the string does. The clamp that
  used to stand in for the finger cost the open A2's octave partial 16 dB per
  second of loss nothing physical was asking for. The pinch harmonic is the
  same filter driven by the other hand: the thumb catches the string at the
  pick's own position, so Pick Position chooses which partial squeals. Measured
  on a fretted E3 with the pick near the bridge, the surviving partial is the
  eighth or ninth and it gains 15 dB on the fundamental against the ordinary
  pick stroke; with the picking hand over the neck the touch sits near the
  midpoint and the squeal is the octave. It is a firmer, longer contact than
  the fretting finger's because the mode-shape law gives a touch that close to
  the bridge little purchase on the low partials — the fundamental loses about
  seven per cent of its energy per round trip where a midpoint touch takes
  nearly all of it — and that asymmetry is the technique rather than a
  shortcoming of the model.
- **Slide:** the finger stays down and travels, so the sounding length moves
  continuously through every intermediate fret and the loop state is preserved
  the whole way. Its duration is a distance divided by a hand speed rather than
  a fixed time — the Bend Time control sets 8% of itself per fret, so the
  280 ms default is 22 ms per fret and a twelve-fret slide takes six times as
  long as a two-fret one. While the finger moves it drags across the winding,
  and the ridges pass under it at `v / w`, the hand's speed along the string
  over the winding pitch, which is exactly why a fast slide squeaks high and a
  slow one low; the level follows the derivative of the glide, so the squeak
  swells and dies with the movement and is exactly zero when the finger is
  still. A plain string has no winding and barely squeaks. The Finger Noise
  control sets the level and silences it exactly at zero.
- **Frets:** fretting position drives sounding length, inharmonicity,
  pickup comb geometry, and Fleischer-style dead-spot damping (deeper on
  the bolt-on end of the construction axis). The Artifacts path can open a
  decaying fret-collision window on hard-picked notes that soft-limits
  displacement and re-radiates deterministic rattle. Hammer-ons retarget a
  sounding loop without clearing its state.
- **Tension modulation:** a string-energy envelope shortens the loop delay, so a
  hard attack starts fractionally sharp and relaxes as the string decays, deeper
  on the thinner strings and with the square of the pick's force. It is far
  smaller than a real string's own stretch and it is documented rather than
  advertised: on the open E2 at full velocity with Velocity Response at 100%
  the peak deviation is +0.16 cents and it arrives 184 ms after the attack,
  which is both well under the pitch a player hears and later than the attack it
  belongs to. Recalibrating it against the stretch law the rest of the model
  uses was scheduled and struck; the measurements and the reason — the suite's
  fixed-bin spectral estimators cannot score a signal whose pitch moves during
  the attack — are in the
  [plan](Docs/best-in-class-plan.md#considered-and-not-planned). The pitch wheel
  moves the same delay target along the Bend Time glide, each string by its own
  compliance.
- **Fretting-hand vibrato:** channel pressure rocks the finger, and the pitch
  follows the *square* of the finger's displacement, because rocking a stopped
  string sideways by `x` lengthens its path by `k x^2` — the same `dL/L`
  relation the pitch wheel's per-string compliance is solved from. That is not a
  flat-topped wave: the note dwells at the fretted pitch between excursions and
  the excursions themselves are briefer and sharper-cornered than the rock that
  makes them, so each cycle spends 36.4% of itself above half its own peak where
  a raised cosine spends 50%. Every stopped string carries its own finger — its
  own phase, drawn at note-on and deliberately not redrawn by a hammer-on or a
  slide, which are the same finger arriving somewhere else, and its own rate and
  excursion redrawn once per cycle at 12% and 15% of standard deviation. So a
  double stop's two strings drift apart instead of moving in lockstep (mean
  phase separation 0.19 cycles, exactly zero before) and the cycle period varies
  by 14.2% of its mean where the single shared oscillator this replaced repeated
  to 0.05%. The pressure ramps at a bounded rate and is then shaped by a
  smoothstep, so the hand leaves rest with zero slope instead of at its
  steepest: at a tenth of the time it takes to reach 90% of settled depth the
  excursion is 1.9% of settled, where a one-pole sits at 20.6% whatever its time
  constant, and 90% of settled still arrives at 207 ms. The nominal excursion is
  40 cents with the per-cycle draw riding on top of it, so the widest cycles
  reach about 55 cents. Zero pressure is bit-exact identical to no pressure.
- **Low-register voicing:** the wound strings' decay law and the plectrum's
  release spectrum are calibrated against a dry electric low-E reference
  recording. A solid-body electric's low strings ring for tens of seconds while
  their content above a kilohertz is gone inside a tenth of a second, and the
  ideal 1/n^2 pluck the literature describes - which the pickup's induced-EMF
  differentiation turns into a 1/n voltage spectrum - overstates a real string's
  upper partials by roughly 9 dB by the fourteenth. Correcting both is what
  removed the nasal, clavinet-like character the low register used to
  have: the mean per-partial error against that reference falls from 4.5 dB to
  2.9 dB, and a palm-muted chug is now dominated by its own fundamental instead
  of by its low mids. The hollowness that outlasted this work turned out not to
  be an excitation problem at all - moving these corners changes the
  fundamental's relative level by a fraction of a decibel - but a pickup one;
  see the position comb below. Scored on half-octave bands against the same
  references, the mean error is now 4.31 dB where it was 5.55 dB, and the
  60-85 Hz error on an open attack is 0.7 dB where it was 5.4 dB.
- **Velocity:** level is the plectrum's deflection. A pick holding the string at
  fraction `p` of its length needs a lateral force `F = T y0 / (p (1-p) L)` to
  hold it at `y0`, so the deflection is linear in the hand's force and a pickup,
  which senses displacement, is linear in the deflection. MIDI velocity is read
  as that force — `0.05 + 0.95 v`, the floor being the lightest stroke that
  still slips off the pick — and Velocity Response is the *exponent* on it, so
  the control scales the decibel range linearly and is an exact no-op at zero,
  where `F^0` is one for every stroke. Force and contact spectrum are then two
  axes rather than one, and separating them is what un-flattened the top of the
  keyboard: force decides how far the string swings, and thence how hard it
  meets the frets and how far it stretches itself sharp, while what the pick
  puts into the string is set by its slip time `t_s = Z d / F + Z / k` — the
  string leaves at the kink velocity `F / Z` over a grip depth the stroke does
  not change plus the pick tip's own elastic recoil. That second term is a floor
  no amount of force gets under, which is the sense in which the plectrum's
  stiffness and not the hand bounds the contact spectrum: a 0.73 mm celluloid
  medium recoils about 0.8 mm past a grip depth near 0.2 mm, so four fifths of
  the slip at full force is the pick letting go of itself. Measured on the open
  E2 at the shipping default, the peak of the first 50 ms spans 18.2 dB from
  velocity 1 to 127 and 3.5 dB across the top half of the keyboard alone,
  where the blend this replaced spanned 5.2 dB and 1.5 dB and turned over above
  velocity 110 instead of staying monotone. A hard stroke is still the brighter
  one — the attack's spectral centroid rises 7% from a soft stroke to a hard one
  at full response — but the brightness no longer eats the accent. The response
  also drives pulse width, the string-scaled modal release, contact noise and
  collision likelihood. The principal release passes two
  low-pass stages whose time scale follows the string period, approximating
  the `1/n^2` modal falloff of a triangular pluck displacement; for ordinary
  sustained pick styles, a much smaller broadband component preserves the
  pick edge. Their delay-line projection is normalised against open E4, so
  equal player effort does not lose tens of decibels on the much longer E1
  loop. At 0% response every velocity renders bit-identically; at 100% the
  response spans soft finger-light notes through aggressive metal attacks.
- **Pickups and coils:** per-string position combs at morphing
  bridge/neck distances, whose delayed tap is weighted below unity so the
  notch is about 12 dB deep rather than infinite - a real pickup senses through
  an aperture, sums two coils at two distances and sits in a three-dimensional
  field, so its taps never cancel exactly, and making them cancel exactly was
  costing the fundamental most. Correcting it recovered nearly 5 dB in the
  60-85 Hz band and is what removed the last of the hollowness. Then the coil
  pair: a humbucker is two coils, so each pickup sums a second tap 19 mm further
  along the string, weighted at 0.60 so the pair dips by `(1-b)/(1+b)` = 12 dB
  rather than nulling infinitely — the screw coil sits further from the string
  than the slug coil and reads quieter for it, and no real pickup is a pair of
  point sensors reading one plane of motion. The dip lands at `c / 2d` with the
  string's own transverse wave speed `c = 2 L f_open`: 3046 Hz on the E2
  string and 4066 Hz on the A2 string, against Lemme's measured 3000 Hz and
  4000 Hz, where the single wide rectangular window this replaced first nulled
  at 5507 Hz and 7351 Hz — most of an octave too high, because a rectangle of
  width `W` nulls at `c / W` and a two-point sum at `c / 2d`. The two coils need
  only one position comb: for coils at `centre ± d/2` the sum factors exactly
  into the centre-anchored comb above times the two-point sum, so the pair costs
  one extra fractional read per pickup rather than a second comb. Pickup type
  closes the spacing to exactly zero, at which point the stage reports itself
  unpaired and returns its input untouched, so the single coil is structurally
  one coil and not a cancelled pair — its measured partials move by at most
  0.008 dB. The magnetic aperture is then the same wave-speed-scaled 4.8 mm
  finite rectangular window with its exact sinc response for both types, because
  it is one bobbin either way. Putting the notch where it belongs also rebalances
  the humbucker across the string set, since the wide window was throwing away
  top octave on the wound strings and keeping it on the plain ones; on a full
  chord the corrected pickup comes out 0.86 dB darker overall, so it stays the
  dark pickup of the pair. After the aperture come a bounded
  second-order-dominant flux nonlinearity,
  induced-EMF differentiation with an oversampled ultrasonic guard, and one
  loaded resonant coil filter per pickup (2.0 kHz / Q 1.0 humbucker anchor to 6.0 kHz /
  Q 2.4 single-coil anchor). A bounded string-mass/pole-balance calibration
  keeps the thick low strings at practical guitar-pickup levels. The selector
  fades Neck, Both (with the
  paired-coil resonance shift), or Bridge; the passive tone control moves
  the loaded resonance down and damps it.
- **Body:** four modal resonators voiced along strongly separated wood, size,
  shape, and construction endpoints receive bridge motion and
  contact noise. Each resonator is normalised at its own modal peak so low
  modes do not acquire an unintended frequency-dependent boost. The resulting
  structural drive is converted once to induced voltage before the modal
  bank, guarded, then joined with string pickup voltage before the same coil;
  displacement is never mixed directly into an electrical signal. Their
  positive, bounded modal bridge conductance also drains string energy when a
  fundamental or strong partial meets a body mode, so the build changes
  sustain as well as timbre. Solid-body coupling, not an acoustic radiator.
- **Bridge coupling:** every string you are not fingering is a
  real waveguide, not a resonator bank. The plucked strings' bridge force is
  summed into a one-sample-delayed bus that drives the idle strings' own loops
  at their open pitch, with their own bridge pickup tap and with their loop
  filter solved from the same two decay targets a played string of the same
  steel gets - its fundamental's T60 and the same wound/plain high-frequency
  ratio. That last part is what makes the bank read as strings rather than as a
  bright plate: a fixed loss coefficient left the wound strings' top end -
  which a played low E loses inside a tenth of a second - ringing for over three
  seconds, and the coupled ring's 1.5-6 kHz band sat 37.8 dB under its
  60-700 Hz band where it now sits 87.1 dB under. The two targets are not always
  both reachable: a one-pole steep enough to hit the high one costs gain at the
  fundamental, and the loop gain cannot exceed one. Where they collide - which
  is most of the range above String Age 0.8 and anywhere the bridge hand is on
  the strings - the high-frequency target is bisected back toward the
  fundamental's until the pair fits, so a coupled string always decays at the
  rate its steel says and only the top of the tilt is given up.
  The strings you *are* fingering terminate on that same saddle, so they read
  the bus as well - each one minus its own contribution to it, which is what
  reciprocity of a passive junction requires and what stops a string driving its
  own bridge termination a second time, since the model already carries that
  termination in the body conductance. Every decay target, T60 and timbre
  calibration in the instrument sits downstream of that subtraction, so it is
  made exact rather than approximate: with one voice sounding the bus is that
  voice's own contribution, the difference is identically zero, and a single note
  renders bit-identically at every setting of the control. Closing the graph
  costs the acyclic stability guarantee the one-way path had, and an explicit
  bound replaces it. The one-sample publication delay makes the network a Jacobi
  iteration whose diagonal is exactly zero, and its row-sum norm
  `(N - 1) g max_j 1/(1 - G_j)` - written in the *receiving* string's loop
  amplification `1/(1 - G)`, which on an eight-string open chord reaches 584 -
  is held at or below 0.25, 12 dB of margin. That is not a calibration hope: the
  gain is re-solved against the sounding voices at every control tick and every
  note-on, so the bound holds at every parameter setting rather than only where a
  test looks. Each coupled loop additionally carries a bounded soft limit.
  What this buys is a structural absence closed rather than a loud effect, and
  the honest measurement is the one worth quoting: a chord that fingers all eight
  strings leaves no idle string to ring and until now had no coupling path at
  all, and it now differs from the same chord at Resonance 0 by -60.1 dB at the
  20% default and -50.5 dB at maximum over the first 1.5 s, and by -45.3 dB and
  -35.7 dB over 10-12 s - the effect grows in the tail, where coupling belongs.
  It does not get louder than that: the difference is exactly linear in the
  injection gain, and rendering the same chord with the cap lifted, the network
  stops being a filter and diverges at roughly twenty times the gain the bound
  permits. The alias floor
  is 158.6 dB below the spectral peak, 7.3 dB better than before, because the
  injection is one more lossy path into loops that already low-pass. The bridge
  hand that mutes a palm-muted passage covers every string, so the style damps
  and starves both the coupled strings and the played strings' share of the bus
  automatically, and a Drop-E
  chug stays tight. At 0% the coupled loops are never configured, rendered or
  injected into, the played strings' injection gain is exactly zero, and an
  eight-string chord is bit-identical to what it was before the path existed, so
  the control is an exact bypass and not a small residue.
  Coupled strings reuse the idle voice's own delay line, so the feature costs
  no extra memory, and they retire as soon as they fall below audibility. The
  CC 1 resonance raises the coupling live toward total and opens the
  amplifier-feedback path described under the keyswitch section, so the same
  mechanism spans a polite studio ring to a howling wall of amp.
- **Dead notes:** the fretting hand laid across the strings without pressing
  them to a fret. It is the whole hand rather than the heel and it is nowhere
  near the bridge, so unlike the palm mute it is broadband — it takes the
  fundamental as hard as everything else, which is the difference between a
  dead note and a very tight mute. It is a loss inside the loop rather than a
  gate on the output, so the pick lands at the same level (within 1.1 dB of an
  ordinarily picked note's peak) and the note then decays through its own
  solved filter; nothing of the fretted pitch survives 150 ms, 40 dB or more
  below the same partial of the picked note. It combines in parallel with the
  bridge hand, so a dead note played under palm-mute pressure gets both.
- **Bridge-hand damping:** the heel of the hand is an absorber, so its
  loss is added to the string's own rather than rescaling it: the decay rates
  sum, which is to say the reciprocals of the decay times do. That is what makes
  a palm mute behave like a hand instead of like a gate. It is not equally
  absorbent at every frequency: resting near the bridge, it removes far more
  energy from the high modes, which move a great deal there, than from a
  fundamental that barely moves at all, so its rate at the high reference is
  three times its rate at the fundamental. Treating it as genuinely broadband
  damped the fundamental as hard as the top end, which is what made a mute read
  as a thin, cut-off pick rather than a heavy chug. The Palm Mute keyswitch
  style and the continuous Palm Mute pressure are the same absorber at
  different depths, and they combine in parallel too. Its rate at the
  fundamental is divided by twenty-two while the top is multiplied by three - an
  effective 66:1 ratio between the two fitted points - because a palm mute
  measurably does not shorten the fundamental at all: across nine dry muted
  power-chord references at five pitches the fundamental moves +1.0 to -2.8 dB
  over the first third of a second while the fourth harmonic drops as much as
  24 dB. Charging the full rate at the fundamental is what left a muted chord
  with no bottom and no tail - inaudible a second after the pick, where every
  reference still rings. On its own a relief that large is *worse* than none,
  because it lets the harmonics ring on alongside the fundamental; it works
  because a band of loss centred on five times the fundamental removes them. The
  two are one mechanism and neither ships without the other. Their targets follow dry
  muted power-chord reference recordings: a real short muted chord falls a couple
  of decibels in its first 25 ms and reaches -40 dB around half a second later,
  while a looser one holds a low tail for seconds. Because the damping
  re-solves the same loop filters and re-applies the analytic phase
  compensation, a damped string stays exactly in tune. The same hand also
  darkens the attack, because it is already on the string when the pick arrives:
  it lowers the release corner, the contact-noise band and the level of the
  plectrum edge. Zero pressure is a
  mathematical no-op. MIDI CC 2 adds to it live.
- **Strum travel:** simultaneous note-ons inside a 35 ms window are one pick
  stroke, and the stroke decides which edge of the chord the pick starts from -
  a downstroke from its lowest string, an upstroke from its highest - rather
  than whichever note-on the host happened to send first. Because a chord's
  note-ons routinely arrive across several `process()` blocks, every voice of a
  chord is held back by a fixed 20 ms pre-roll: inside that window a
  later-arriving string can still turn out to be where the stroke began, and
  re-anchoring reschedules the voices that have not sounded against the chord's
  own clock. That is what makes a chord's onsets identical whichever order the
  host sends its notes in, and it is a fixed time rather than a block count, so
  they do not depend on the host's buffer size either. A note-on cannot know
  whether the rest of a chord is still coming, so the pre-roll is charged
  whenever Strum Spread is non-zero — including to a note that turns out to be
  alone, which therefore sounds 20 ms after its note-on. Chords whose note-ons
  are
  spread wider than the pre-roll travel from their first arrival, which is the
  stated limit of the mechanism rather than an undefined case. Under alternate
  picking the direction is captured at the chord's first note-on and holds for
  the whole chord, so one strum is not asked to travel both ways; the per-string
  up/down colouring still alternates exactly as before.
  The wrist also accelerates through the strings instead of sweeping at a
  constant speed: entering the string plane at `v0`, `v(x) = sqrt(v0^2 + 2 a x)`
  and the crossing intervals compress as `1 / v(x)`, with the acceleration set
  so the last of seven crossings takes 0.70 of the first. The seven gaps are
  then rescaled to sum to seven times Strum Spread, so the control states the
  *mean* crossing time and keeps its meaning: a short chord at the edge the
  stroke starts from is spread a little wider than the knob says (1.21 times it
  on the first crossing) and one at the far end a little narrower (0.85 on the
  last), because the pick is still speeding up. At a 12 ms spread the eight
  strings sound 20.0, 34.7, 48.1, 60.7, 72.4, 83.5, 93.9 and 104.0 ms after the
  chord's first note-on - 84.0 ms of travel, in gaps falling from 14.7 to
  10.1 ms. The acceleration is drawn once per chord (15% of standard deviation)
  with a small per-crossing draw on top (0.5%), so no two strums lay down the
  same ramp, and a chord anchored on a middle string no longer travels outward
  in both directions at once. A chord therefore sweeps instead of landing as a
  block, and its stacked initial peak drops. At 0 ms no pre-roll is charged and no draw
  is made: chords are exactly simultaneous and the engine is bit-unchanged.
- **Play noise:** deterministic seeded plectrum scrape, finger contact,
  and release damping noise, band-shaped per string (wound
  versus plain) with independent level controls. Identical MIDI always
  renders identical audio.
- **Artifacts:** a separate deterministic imperfection path adds controllable
  bridge-hardware ring, velocity-dependent incidental fret contact, and
  string-specific saddle buzz. This is the mechanical noise of the instrument's
  hardware, distinct from the Sympathetic Ring control above, which vibrates
  the actual strings. At 0% it is exactly bypassed and silent; the 18% default
  is intentionally subtle.
- **Cost:** the model only pays for what is audible. The four pickup position
  taps read at a delay that only moves when the voice is reconfigured, so their
  interpolation weights are solved there instead of being rebuilt from a clamp,
  a floor and an eight-product polynomial on every sample of every string; the
  magnetic aperture window is split and inverted in the same place, which
  removes the last division from the pickup path. Together those took the
  eight-string render at 96 kHz from 0.190x to 0.166x realtime worst case
  (Both + Stereo), 0.146x to 0.129x at the default Bridge + Mono, and a
  full-throw wheel glide on the same chord from 0.210x to 0.172x. The seven
  play styles added since cost three float comparisons per voice per sample -
  the node touch, its decay, and the slide's friction - all of which are false
  for an ordinary note: measured best of five against the same fixture, the
  default configuration moved about five per cent and the worst case stayed
  inside the run-to-run spread. The bridge coupling between played strings is
  one subtract, one multiply and two adds per voice per sample and leaves the
  CPU guardrail unmoved; the humbucker's second coil is one fractional read per
  pickup per voice; and the picking hand's per-attack draws, the vibrato's
  per-cycle draws and the strum's ramp solve are once per note, per cycle and
  per chord rather than per sample. A pickup the
  selector has
  faded out is skipped outright, including its two fractional reads, aperture
  window, flux polynomial and induced-EMF guard. Mono runs one shared coil, DC
  and decimation chain instead of two and mirrors it, which is exact because
  the channels are identical; opening the stereo field copies the state across
  at that sample. Damping-only control moves reuse the existing dispersion fit
  instead of re-running its grid search. Once nothing is vibrating and the
  shared path has fallen below -120 dBFS the engine freezes: state is cleared,
  the output is exactly zero, and no arithmetic runs until the next note. All
  rate-derived smoothing constants are solved in `prepare()` rather than with
  `std::pow` inside the sample loop.
- **Output field:** Mono is exact dual mono and preserves the conventional
  electric-guitar DI. Stereo spreads per-string pickup signals left-to-right
  in physical low-to-high string order, keeps the body centred, folds down
  coherently, and adds no chorus, modulation, random phase, or Haas delay.

## Amplifier chain

The five FX controls run in the same JUCE-free library as the string model
(`Source/DSP/ElectryFx.*`), so the complete signal path is regression tested on
every platform rather than only inside a host.

- **Oversampled clipping.** The distortion pedal and the amplifier run inside a
  4x oversampled domain, reached through two cascaded Kaiser-windowed halfband
  stages whose kernels are designed at `prepare()` time rather than tabulated. A
  gain stage fed at host rate folds its own upper harmonics straight back into
  the guitar band, and that folded intermodulation is most of what makes a
  modelled high-gain tone read as digital: measured on a steady tone, the
  non-harmonic floor is 43 to 74 dB below what the previous host-rate chain
  produced at every setting where that chain aliased at all. Above 96 kHz one halfband stage is
  dropped and above 192 kHz both are, because a host already running that fast
  supplies the bandwidth the stages need. While the block is engaged it adds
  17.25 host samples of fixed group delay (0.36 ms at 48 kHz); with both gain
  controls at zero it is skipped outright, costs nothing, and adds no delay at
  all.
- **Pedal.** A tight 88 Hz input coupling network and a mid-focused voice ahead
  of a bounded diode-pair clipper: passing the whole low end of a Drop-E eighth
  string into a clipper turns the fundamental into intermodulation mud instead
  of a note.
- **Voiced for the eighth string.** The input stage passes the whole Drop-E
  fundamental rather than cutting it at 84 Hz, because clipping it is what
  generates the second and third harmonics the cabinet turns into a chug's
  weight; the pre-gain mid emphasis sits at 850 Hz, above the cabinet's scoop
  instead of inside it; the cabinet's thump is deeper and its boxy region cut
  harder; and the compressor's attack is 18 ms rather than 3 ms, so a pick
  attack passes instead of being levelled away. Measured on a chugged Drop-E
  figure, the 80-160 Hz octave carries 55% of the amplified energy against 24%
  of the dry DI's, while 320-640 Hz drops from 22% to 4%.
- **Amplifier.** Two cascaded triode stages with a standing grid bias, an
  interstage Miller roll-off, and a grid-current bias drift that moves the
  operating point under sustained level, so a held chord thickens and thins
  again as it decays. Each stage is one smooth curve rather than a different
  curve above and below zero: selecting a knee by sign leaves a
  third-derivative kink at the origin that a near-square second-stage waveform
  crosses at full slew, which radiates far more high-order content than the
  saturation itself. Asymmetry — and the even-order harmonics that come with it
  — comes from the operating point, which is where a real stage's comes from.
- **Power stage.** The back half of the amplifier, inside the same oversampled
  domain. The supply sags: the current the stage draws follows its own output,
  a follower tracks that with a 70 ms attack and a 400 ms recovery, and the
  rail droops by up to 28% — the 350 V to 250 V a real supply measures. What
  the rail sets is the headroom rather than the gain, so the stage is
  `droop * triode(u / droop)`: the small-signal slope is untouched and the
  ceiling falls in proportion, which is why a held chug blooms and then ducks
  by 1.2 dB while a quiet passage ducks by 0.2, and why the level comes back
  during a rest. The output transformer follows: a core saturates at a flux
  limit and flux is the integral of the voltage, so the limit is a volt-second
  limit and the low end reaches it first. A one-pole at the 45 Hz
  primary-inductance corner is that integral normalised, what the core cannot
  carry is subtracted back out, and a second-order high-pass in front is the
  transformer's own inability to pass DC. Measured at the stage, a full-level
  tone distorts at −25 dB at 48 Hz, −71 dB at 480 Hz and −131 dB at 4.8 kHz,
  and a tone 24 dB quieter distorts 41 dB less at 48 Hz.
- **Cabinet.** A second-order high-pass at the box frequency, a low-mid thump,
  a scooped boxy region, a presence peak, and a fourth-order Butterworth
  roll-off from 5 kHz, all inside the oversampled domain so the
  alias-generating content is removed before decimation rather than after it.
  This replaces a single one-pole low-pass, which had none of the four features
  that make a recorded metal guitar recognisable as a guitar.
- **Level.** Each gain stage divides its own small-signal gain back out, so
  reaching for the amp control is a change of tone rather than a jump in level;
  a saturating stage still ends up a few decibels louder than the dry DI,
  because compressing a signal raises its average.
- **Compressor, delay, room.** The compressor eases into roughly 3.5:1 above
  -20 dBFS through a soft knee, with makeup, so a palm-muted part sits still
  instead of the level grabbing at every pick attack. The 360 ms lead delay
  damps its feedback path, so repeats darken and thin as an analogue delay's
  do. The room is three allpass diffusers into two damped combs per channel at
  coprime lengths, with no modulation, Haas delay or randomised phase — the
  same constraint the instrument's own stereo field obeys.
- **Bypass and smoothing.** All five mixes are smoothed per sample rather than
  stepped per block, and each snaps to exactly zero, so a control left at zero
  is a bit-exact dry bypass — verified by the regression suite — while engaging
  or disengaging the gain block is crossfaded and cannot click.
- **Cost.** Stereo at 48 kHz, best of three runs: 0.003x realtime with all five
  controls at zero, the same with only the compressor, delay and room open, and
  0.048x with the oversampled gain block engaged. For reference the eight-string
  model itself runs at roughly 0.13-0.17x realtime at 96 kHz.

## Guitar construction axes

The material controls use contrasting classic solid-body anchors. They default
to 0, the thick carved mahogany/maple set-neck end of every axis, because the
shipped instrument is a specific guitar rather than the average of the range -
the reasoning and what it costs in control range are in the
[research contract](Docs/physical-modeling-research.md#the-default-voicing-and-what-moving-it-cost).
Scale length is widened for the Drop-E instrument and defaults to 27.63":

| Control | 0 | 1 |
| --- | --- | --- |
| Body wood | Mahogany/maple blank | Swamp ash slab |
| Body size | Thick, heavy (lower modes) | Thin, light (higher modes) |
| Body shape | Carved single-cut pattern | Flat slab pattern |
| Construction | Set neck + stopbar | Bolt-on + through-body |
| Scale length | 25.5 in conventional electric | 28 in baritone / 8-string |
| Pickup type | Humbucker, two coils 19 mm apart | Single coil, one bobbin |

## Exact 31-parameter contract

The original 20 version-1 host parameters remain in their exact order; the
Artifacts control is parameter 21 and Output field is appended as parameter
22. The five FX controls are appended as parameters 23..27, and the four
version-1.1 performance controls as parameters 28..31, so every existing host
automation index keeps pointing at the same control. A session saved before
1.1 loads unchanged and picks up the new defaults. Version 1.2 repurposes two
existing slots rather than moving anything: Bend Time (parameter 18) now
governs the pitch wheel's travel — the same finger-shaped glide it gave the
former keyswitch bends — and parameter 31 became Resonance Depth, the
full-scale reach of the CC 1 resonance wheel, keeping its stored ID and
0..100 range so a saved session's value carries over sensibly. Continuous
controls are smoothed inside the engine; pickup and output mode changes
crossfade over roughly 4 ms.

| # | ID | Name | Range and default |
| --- | --- | --- | --- |
| 1 | `pickupSelector` | Pickup selector | Neck / Both / **Bridge** |
| 2 | `pickupType` | Pickup type | 0..100%, default 32% |
| 3 | `tone` | Tone | 0..100%, default 70% |
| 4 | `bodyWood` | Body wood | 0..100%, default 0% (mahogany/maple set blank) |
| 5 | `bodySize` | Body size | 0..100%, default 0% (thick heavy blank) |
| 6 | `bodyShape` | Body shape | 0..100%, default 0% (carved single-cut) |
| 7 | `construction` | Construction | 0..100%, default 0% (set neck + stopbar) |
| 8 | `scaleLength` | Scale length | 25.50"..28.00", default 27.63" |
| 9 | `bodyResonance` | Body resonance | 0..100%, default 35% |
| 10 | `stringGauge` | String gauge | Drop-E .009-.080 set to .011-.098 set, default 100% (.011-.098) |
| 11 | `stringAge` | String age | 0..100%, default 30% |
| 12 | `pickPosition` | Pick position | bridge..neck, default 18% |
| 13 | `pickHardness` | Pick hardness | 0..100%, default 58% |
| 14 | `pickNoise` | Pick noise | 0..100%, default 50% |
| 15 | `fingerNoise` | Finger noise | 0..100%, default 40% |
| 16 | `releaseNoise` | Release noise | 0..100%, default 40% |
| 17 | `muteDamping` | Mute damping | 0..100%, default 55% |
| 18 | `bendTime` | Bend time | pitch-wheel travel time, 40 ms..2 s, default 280 ms |
| 19 | `velocity` | Velocity response | 0..100% exponent on the pick's force (0% is velocity-invariant), default 85% |
| 20 | `output` | Output level | -24..+6 dB, default -6 dB |
| 21 | `artifacts` | Artifacts | clean bypass..ring/contact/saddle detail, default 18% |
| 22 | `outputMode` | Output field | **Mono** / Stereo divided-pickup field |
| 23 | `distortion` | Distortion | dry..oversampled pedal-style clipping, default 0% |
| 24 | `amp` | Amp simulation | dry..oversampled cascaded gain stages into the modelled cabinet, default 0% |
| 25 | `compressor` | Compressor | dry..fast rhythm levelling, default 0% |
| 26 | `delay` | Delay | dry..360 ms lead delay, default 0% |
| 27 | `room` | Room | dry..compact stereo ambience, default 0% |
| 28 | `sympathetic` | Sympathetic ring | exact bypass..full bridge coupling, into the unfingered strings and between the fingered ones, default 20% |
| 29 | `palmMute` | Palm mute | 0..100% continuous bridge-hand damping for every play style (adds to MIDI CC 2), default 0% |
| 30 | `strumSpread` | Strum spread | 0..40 ms mean pick travel per string crossed, plus a 20 ms pre-roll whenever it is non-zero, default 0 ms (block chord) |
| 31 | `vibratoDepth` | Resonance depth | 0..100% full-scale reach of the CC 1 resonance (coupling lift and amplifier feedback), default 35% |

## Build products

`scripts/build-macos.sh` writes three bundles below `build-macos/`:

- `Electry_artefacts/Release/VST3/Electry.vst3`
- `Electry_artefacts/Release/AU/Electry.component`
- `Electry_artefacts/Release/Standalone/Electry.app`

## Requirements

- macOS 11 or newer (Apple silicon or Intel; universal by default)
- Full Xcode installation selected with `xcode-select`
- CMake 3.22 or newer
- Internet access for the first configure (JUCE 8.0.14 is fetched and
  pinned by checksum), or a local checkout supplied via `JUCE_PATH`

## Build on macOS

```bash
cd electry
./scripts/build-macos.sh
```

The script configures the Xcode generator, builds universal binaries, runs
the CTest suite (engine and plug-in contract tests), and ad-hoc signs the
three bundles. Environment overrides: `BUILD_UNIVERSAL=OFF` for a
native-arch build, `CONFIG=Debug`, `BUILD_DIR`, and `JUCE_PATH` for a local
JUCE 8.0.14 checkout.

## JUCE-free DSP build

The complete string, interaction, pickup, and amplifier path builds and tests
without JUCE on any platform with CMake and a C++20 toolchain (this is what
Linux CI runs):

```bash
cd electry
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DELECTRY_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
```

That configuration also builds `ElectryRenderDemos`, which re-renders the
committed demonstration audio:

```bash
./build-dsp/ElectryRenderDemos Docs/audio
```

`ELECTRY_BUILD_TOOLS=OFF` leaves the renderer out.

## Install and validate locally

```bash
ditto build-macos/Electry_artefacts/Release/VST3/Electry.vst3 \
  ~/Library/Audio/Plug-Ins/VST3/Electry.vst3
ditto build-macos/Electry_artefacts/Release/AU/Electry.component \
  ~/Library/Audio/Plug-Ins/Components/Electry.component
```

Run `auval -v aumu Elc1 Eltr` after installing the Audio Unit, and rescan
plug-ins in your host. The Standalone app runs directly from the build
tree.

## Sign, package and notarize

```bash
cd electry
./scripts/sign-and-package-macos.sh
```

Without arguments the script stages the three bundles with their licence
documentation, ad-hoc signs them, and writes
`build-macos/dist/Electry-1.2.0-macOS-universal.zip` and `.pkg`. For
distribution, provide `APP_SIGN_IDENTITY` (Developer ID Application),
`INSTALLER_SIGN_IDENTITY` (Developer ID Installer), and optionally
`NOTARY_PROFILE` for `notarytool` submission and stapling.

## Project layout

```text
CMakeLists.txt       DSP library, demo renderer, JUCE plug-in, and CTest targets
Source/DSP/          ElectryEngine (JUCE-free physical model), ElectryFx
                     (JUCE-free amplifier, cabinet and time effects), and
                     ElectryVisuals (JUCE-free fretboard geometry/ballistics)
Source/              PluginProcessor and PluginEditor (JUCE shell)
Tests/               Engine, amplifier-chain, and plug-in contract tests
Tools/               RenderDemos, which produces the committed Docs/audio set
Docs/                Physical-modeling research, implementation contract, and
                     the rendered demonstration audio
Presets/             Sound-design recipes for the 31-parameter set
scripts/             macOS build and packaging helpers
ThirdParty/          JUCE licence notice
```

## Licensing

Electry's original source is under the [MIT License](LICENSE); see the
[third-party notices](THIRD_PARTY_NOTICES.md). Electry builds against JUCE,
which is dual-licensed under AGPLv3 or a commercial JUCE licence
([notice](ThirdParty/JUCE-LICENSE.md)); confirm the applicable terms before
distributing binaries. No samples, impulse responses, or third-party preset
libraries are included; "Les Paul" and "Telecaster" name the reference
styles of the modeling axes and are trademarks of their respective owners,
with no affiliation or endorsement implied.
