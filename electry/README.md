# Electry

An original, physically modeled **dry electric guitar**: VST3, Audio Unit and
Standalone for macOS, plus Linux and Windows builds.

![Electry](Docs/screenshots/electry-standalone.png)

Eight string voices run dual-polarisation waveguide loops with physically
derived stiffness dispersion, decay-targeted damping and exact fundamental
phase compensation. A fretting hand with a position and a reach decides where each
note is played; a point-touch model produces natural and pinch harmonics as the
string's own mode shapes rather than as transpositions; slides are a distance
over a hand speed, their squeak the winding passing under the finger; a picking
hand never puts the plectrum down in exactly the same place twice. The standard
pitch wheel moves every string together over a uniform ±2 semitone range,
and a resonance wheel can push a distorted tone into self-sustaining amplifier
feedback through a fixed nominal air delay, independent of DAW callback size,
with performance-controlled idle-string drive. One bridge is shared by all
eight strings:
the ones you are not fingering ring off it, and the ones you are exchange
energy through it. The pickups follow a published signal structure — position
comb, two coils for the humbucker and one for the single coil, finite magnetic
aperture, nonlinear flux, induced EMF and loaded coil resonance.

The performance is selected with two independent keyswitch banks below the
playable range. Pick Stroke always latches; Play Style can either latch or act
only while its key is held, then return to the visible base choice. Any stroke
can therefore drive any style without programming an Open switch around every
muted phrase.

The compact FX panel provides five amount controls plus an **Amp Voice**
selector: a distortion pedal; American Clean, British Crunch and Modern
High-Gain amplifier/cabinet paths; compression; lead delay; and a stereo room.
Every amount defaults to a true 0 % dry setting. Once Distortion or Amp is moved
above zero it is a drive control around a fully connected circuit, so no
uncabbed DI leaks around an enabled loudspeaker. The pedal solves its
antiparallel-diode RC circuit at every oversampled step. All three amplifiers
use dense measured-12AX7 plate-load transfers generated from exact circuit
solves; the American and British paths add exact third-order passive RC tone
stacks, measured-curve-fitted 6L6GC beam-tetrode or EL34 pentode push-pull load
lines, and source-derived nonlinear 12AT7/ECC81 or ECC83 long-tailed-pair phase
splitters. Two independently solved output-coupling, grid-return and stopper
branches add power-grid conduction, blocking memory and common-bias recovery.
The power-tube load lines also solve their source-derived screen resistors:
individual 470 Ohm American branches and one common 800 Ohm British branch.
Negative feedback, plate-plus-screen-current-driven sag, transformer flux and
six-section speaker/cabinet voicing complete each path. The Modern path retains
Electry's established metal circuit and output sample-for-sample.
The nonlinear blocks run inside an up-to-4× oversampled domain, so a high-gain
tone saturates instead of folding its own harmonics back into the guitar band.
These are meticulously bounded circuit-derived families, not capture-accurate
replicas of named amplifiers, individual power-tube specimens, microphones or
rooms. Mono is the
authentic summed dry DI; Stereo is one engine's
phase-coherent divided-pickup view of the eight physical strings; Double is two
separately seeded deterministic Electry performances, one per channel, with
independent contact detail and a bounded 0-6 ms second-player pick clock.
Neither stereo choice is a delayed copy or synthetic widening effect.
One Guitar Build control follows a smooth path through six deliberately
contrasting solid-body constructions, co-moving internal material, mass,
shape, joint, scale and gauge coordinates. Pickup construction, selector,
tone, body-colour amount and all player controls remain independent.

Every model has named research references, listed below. Electry does not claim
to be a capture-accurate clone of any one instrument.

## Audio demos

Twenty-three takes, rendered by [`Tools/RenderDemos.cpp`](Tools/RenderDemos.cpp)
from the same JUCE-free code the plug-in runs: the full playable range, every
pick-stroke and play style, dry and amplified rhythm, lead tone, pickup and
tone contrasts, sympathetic strum, guitar-build contrasts, velocity dynamics,
power chords, a long arrangement, the pitch-bend and feedback wheels, a focused
dry Sustain/Mute/Dead audition, a matched high-gain Mute/Dead comparison, an
extended all-technique solo, four original genre studies, a tremolo-picking
study and one globally normalised three-amp comparison. They are
demonstrations, not evidence — an audible example is not a measurement, and
none of the claims below rest on them. The reproducible ten-probe model renderer,
real-recording boundaries and blind-study plan live in the
[`evaluation contract`](Docs/evaluation.md).

<!-- peaks-table-begin: regenerated by ElectryRenderDemos; edits between the markers are overwritten -->
| File | Rendered peak | Normalisation applied |
| --- | --- | --- |
| `01-range-open-strings.wav` | −12.3 dBFS | +9.3 dB |
| `02-range-full-fretboard.wav` | −11.3 dBFS | +8.3 dB |
| `03-play-styles.wav` | −3.8 dBFS | +0.8 dB |
| `04-drop-e-rhythm-dry.wav` | −12.9 dBFS | +9.9 dB |
| `05-drop-e-rhythm-amp.wav` | −12.7 dBFS | +9.7 dB |
| `06-lead-amp-delay-room.wav` | −11.6 dBFS | +8.6 dB |
| `07-pickups-and-tone.wav` | −11.7 dBFS | +8.7 dB |
| `08-sympathetic-strum-stereo.wav` | −16.6 dBFS | +13.6 dB |
| `09-guitar-build-contrasts.wav` | −8.1 dBFS | +5.1 dB |
| `10-velocity-dynamics.wav` | −11.6 dBFS | +8.6 dB |
| `11-power-chords-dry.wav` | −7.9 dBFS | +4.9 dB |
| `12-power-chords-amp.wav` | −12.5 dBFS | +9.5 dB |
| `13-long-rhythm-arrangement.wav` | −13.1 dBFS | +10.1 dB |
| `14-whammy-and-feedback.wav` | −13.0 dBFS | +10.0 dB |
| `15-mute-and-dead-audition.wav` | −12.4 dBFS | +9.4 dB |
| `16-mute-and-dead-metal.wav` | −13.6 dBFS | +10.6 dB |
| `17-extended-technique-solo.wav` | −12.1 dBFS | +9.1 dB |
| `18-syncopated-djent-study.wav` | −12.4 dBFS | +9.4 dB |
| `19-modern-metalcore-study.wav` | −12.5 dBFS | +9.5 dB |
| `20-odd-meter-prog-study.wav` | +0.8 dBFS | −3.8 dB |
| `21-blues-rock-lead-study.wav` | −9.5 dBFS | +6.5 dB |
| `22-tremolo-picking-study.wav` | −13.7 dBFS | +10.7 dB |
| `23-amp-voices.wav` | −15.2 dBFS | +12.2 dB |
<!-- peaks-table-end -->

`15-mute-and-dead-audition.wav` is the quickest dry vocabulary check: the same
E1 as Sustain, three Mute Tightness depths, a Palm-held E1/E2 octave whose low
string hammers and pulls without lifting the bridge hand, then fretting-hand
Dead and playable alternate ghost grooves. `16-mute-and-dead-metal.wav` keeps
the score and high-gain chain fixed between Mute and Dead. Double belongs to the
plug-in wrapper and is not used by these JUCE-free single-engine renders.

`17-extended-technique-solo.wav` is the longer lead showcase: two shred runs,
two distinct vibrato holds, ascending hammer-ons and descending pull-offs,
slides in both directions, a final descending slide redirected continuously
before its first target, and all seven play styles in one performance. The next
four files translate the requested reference points into broad, original genre
vocabularies rather than copying a composition, riff or production:

- `18-syncopated-djent-study.wav`: Periphery-adjacent extended-range traits —
  displaced Drop-E accents, tight chugs, dead punctuation and a tapped answer.
- `19-modern-metalcore-study.wav`: Bring Me the Horizon-adjacent traits — an
  anthemic power-chord hook, simple high melody and half-time breakdown.
- `20-odd-meter-prog-study.wav`: Dream Theater-adjacent traits — clean
  arpeggios, seven- and five-beat groupings and a virtuoso alternate-picked run.
- `21-blues-rock-lead-study.wav`: Joe Bonamassa-adjacent traits — a dynamic
  neck-pickup lead with slides, legato, bends, a pinch and sustained vibrato.

`22-tremolo-picking-study.wav` is the playable black/progressive-metal repeat
proof: the visible B0 **TRM** wrist runs the ordinary physical attack path at
8, 12 and 16 strokes/s, enters a moving line after being held in silence,
then pre-holds A#0 through a 240 ms rest so the next stopped finger demonstrates
a fresh vibrato bloom. That lead picks MIDI 74, genuinely hammers to 76, then
leaves Hammer latched while B0 continues making picked Sustain contacts before
Sustain is restored. A held Drop-E chord follows, with its low string sliding
while a scheduled repeat is still travelling toward it. The played entrance
becomes the wrist's first contact, so its next pick is one complete interval
later; every repick leaves the rocking fretting finger in place, and the slide
retargets the reserved low-string contact to the moving fret without stealing
the stroke from the other strings. Those three anchors equal sixteenth notes at
120, 180 and 240 BPM.

`23-amp-voices.wav` sends the same deterministic Drop-E performance through
three fresh FX instances in order: American Clean, British Crunch and Modern
High-Gain. Amp remains at 90%, every other performance and FX setting is held
fixed, each segment has the same duration, and one gain is applied to the whole
file. Near-legato overloaded chord turns expose each circuit's short-term grid
recovery before the phrase moves through chugs, a ringing chord and a bent,
vibrato lead. This compares response and nonlinear dynamics rather than a
level-matched succession of unrelated presets. Existing amplified demos 05–22
retain the sample-identical Modern path.

Files 18–22 are original Electry studies, not endorsements or artist
sound-alikes.
The finger-vibrato moments in files 06, 17, 21 and 22 use the same model available
from the visible A#0 **VIB** key. Hold it before or while a physically held
stopped note rings; pre-held intent waits for the finger to land, and velocity
chooses width. Channel and polyphonic pressure remain unassigned, so normal
aftertouch cannot bend a chord by surprise.

## Factory rigs and quick start

The editor's **RIG** selector provides three deterministic starting points and
sets all 28 host parameters, so it cannot inherit a forgotten control from the
previous patch. Rigs deliberately leave Pick Stroke, the base Play Style and
the `LATCH | HOLD` choice alone.

- **Factory Default:** the dry Drop-E guitar documented in the parameter table.
- **Drop-E Metal:** the demonstrated 45% distortion / 95% Modern High-Gain amp /
  60% compressor chain and tight rhythm setup used by the matched Mute/Dead
  demo.
- **Mute / Dead DI:** dry, +6 dB, hard near-bridge pick, no sympathetic ring
  and middle Mute Tightness, exposing the two hand contacts without an amp.

1. Load **Drop-E Metal**, leave the built-in amp on, and play E1..D6
   (MIDI notes 28..86), the full range drawn on the on-screen piano.
   C0/C#0/D0 select Down, Up or Alternate picking.
2. Keep a note or chord held and click its row in the live fretboard for one
   hard repick without releasing the fretting key. Velocity-sensitive host or
   controller playing uses MIDI E6..B6 for physical strings 8..1; those
   performance triggers are not drawn as misleading pitched piano keys. If
   Hammer is latched, these dedicated picking-hand commands still make a
   neutral Sustain pick contact and leave the latch ready for the next note.
3. Select E0 **Mute** for chugs. **Mute Tightness** is the articulation's
   loose-to-tight steady bridge-hand loss; **Mute Pressure** and MIDI CC 2 are
   the live bridge hand and also stack on Dead. A0 **Dead** is the separate
   fretting hand.
4. Use `PLAY-STYLE KEYS: HOLD` with Sustain as the visible base to hold E0 only
   over chugs or A0 over ghosts. `LATCH` keeps the choice selected.
5. Hold A#0 **VIB** over a fretted lead note. Velocity 64 is the moderate
   roughly 5.6 Hz / 20-cent nominal gesture; velocity 127 reaches the full
   roughly 6.4 Hz / 40-cent rock arc before its bounded per-cycle variation.
   Open strings stay fixed because no finger is stopping them.
6. Hold B0 **TRM** while a note is physically held for automatic picking.
   **TRM Rate** defaults to 12 strokes/s; velocity remains pick force, and
   Alternate remains one down/up wrist. Releasing B0 stops new contacts and
   leaves the current string to decay. E6..B6 remain one-shot string triggers.
7. Choose Mono for a conventional DI, Stereo for one guitar's divided-pickup
   field, or Double before the phrase for two independently performed engines.

## How it works

### Keyswitches, playable range and repick triggers

MIDI notes 12..21 are two independent banks of silent keyswitches. Notes
12..14 (C0..D0) always latch how the pick moves. Notes 15..21 (D#0..A0) either
latch what the hands do or, in HOLD mode, override the PLAY STYLE strip's saved
base only while pressed; newest held key wins and release reveals an older held
key or the base. The two banks stay independent, so any of the twenty-one
combinations — an up-stroke Mute, alternate-picked pinch harmonics — is
reachable in at most two keyswitches. The editor and on-screen keyboard expose
both banks. A play-style release changes future attacks only: sounding
notes, delayed strums and pending repicks keep the style captured at Note On.

| MIDI note | Key | Picking style |
| --- | --- | --- |
| 12 | C0 | Down (default) |
| 13 | C#0 | Up — opposite displacement polarity, slightly closer to the bridge, thinner and brighter |
| 14 | D0 | Alternate — starts down, then alternates down/up for each accepted picked note-on; hammered notes neither take nor consume a stroke |

| MIDI note | Key | Play style |
| --- | --- | --- |
| 15 | D#0 | Sustain (default) — the ordinary ringing pick |
| 16 | E0 | Mute — damping follows Mute Tightness, from a loose half-mute to a tight metal chug at the firm end |
| 17 | F0 | Hammer-on / pull-off — continues a sounding string legato within a nine-fret reach, including pull-offs to open strings; from silence it is a tap, and in either case it has no plectrum contact or wrist delay |
| 18 | F#0 | Natural harmonic — a finger resting on the string's midpoint node, so the octave is what the string does rather than a transposition of it |
| 19 | G0 | Pinch harmonic — the picking hand's thumb catches the string at the pick's own position, so Pick Position chooses which partial squeals |
| 20 | G#0 | Slide — the finger stays down and travels without a second attack; the travel time is a distance over a hand speed, and the winding scrapes under it the whole way |
| 21 | A0 | Dead note — the fretting hand lies across the strings without pressing them to a fret; it stays a dark periodic ghost rather than a gate |

Hammer-ons, pull-offs and legato Slides have no plectrum contact. If the bridge
hand is already palm muting, those fretting gestures retain it on the moving
string and every ringing sibling; only a later real picked contact can move the
shared hand. A Hammer after Dead still replaces that fretting-hand choke.
If another Slide or Hammer arrives before a glide settles, it continues from
the pitch and fractional fret under the finger at that sample. The abandoned
destination never sounds, and direction, remaining travel and scrape speed all
follow the live hand position. Phase changes from the destination's damping and
dispersion fit are absorbed into the raw delay coordinate, so the complete loop
period—not merely its MIDI control value—stays continuous.
Live Mute Pressure and shared Palm/Open hand contacts use the same complete-loop
invariant: changing loss changes decay and colour, not the pitch of a string
that was already ringing.

| MIDI note | Key | Momentary gesture |
| --- | --- | --- |
| 22 | A#0 | Vibrato — hold while physically held stopped notes ring; Note On velocity sets the hand's width and Note Off eases it back to rest. Pre-held intent waits at zero for a stopped finger, while open strings and key-released tails stay fixed. Overlapping owners balance, All Sound Off and Reset All Controllers preserve a physically held key, while All Notes Off and Panic release it |
| 23 | B0 | Tremolo picking — hold to repick every physically held string through the current Pick Stroke and Play Style; only a latched Hammer uses a neutral Sustain contact without changing the latch. Velocity is pick force and TRM Rate is speed. A newly played note is its own first contact. Overlapping owners balance; CC120/121 preserve the held wrist, while CC123, Panic, prepare and release stop it |

Notes 24..27 are ignored, and notes 28..86 are playable on a 22-fret,
eight-string Drop-E instrument tuned E1-B1-E2-A2-D3-G3-B3-E4. D#6 (87) is a
silent separator. Notes 88..95 (E6..B6) are picking-hand triggers for physical
strings 8..1, from the lowest E1 string to the highest E4 string. A trigger
repicks the note physically held on that string with its own velocity and the
current Pick Stroke and Play Style; only a latched Hammer becomes a neutral
Sustain pick for this dedicated contact, without changing the latch or the next
playable note. An unheld string stays silent. A trigger never adds
fretting-key ownership, so its Note Off is inert and the original note's Note
Off still releases normally. Held ownership survives the old sound's natural
decay, allowing a silent Mute or Dead note to be struck again. Notes
outside these ranges are ignored. The on-screen piano ends at D6: E6..B6 remain
available as MIDI-only performance triggers and are not shown as pitched keys.
Clicking a row in the live fretboard sends the same trigger at hard velocity;
it is a visible picking-hand shortcut, while the MIDI lane retains continuous
velocity and sequencer timing.
The visible B0 **TRM** key drives those same one-shot contacts from one shared
wrist clock. At the default 12 strokes/s, the first repeat is exactly 4,000
host samples after the initial contact at 48 kHz; phase remains sample-accurate
across host block partitions at every supported rate and oversampling mode.
If the wrist was already held while no string was fretted, the first played
contact re-anchors that phase instead of inheriting a nearly finished empty
cycle and creating a flam against its own next pick.
Hammer-ons and legato slides remain fretting-hand gestures: even when one lands
on the exact repeat boundary, B0 still reaches every physically held string.
Conversely, leaving Hammer latched cannot turn a B0 or E6..B6 picking-hand
command into another tap: that dedicated contact uses Sustain while the latch
remains available to the next playable note.
If that finger move arrives after a repeat has been reserved but before its
pick reaches the same string, the plectrum keeps its remaining travel time and
meets the string at its current moving pitch instead of disappearing or
finishing the glide early.
The reverse separation matters too: while a held string is still ringing, a B0
or E6..B6 plectrum contact restarts it without restarting the A#0 finger rocking
the fret. A new fretting assignment, or a contact after the damped voice has
retired, draws a new finger phase, rate and excursion.
Pre-held A#0 intent remains at zero until a physically held stopped finger
exists. A released or sustain-held tail has no fretting finger and keeps no
vibrato offset. Releasing one stopped key clears only that voice while a
still-held stopped sibling keeps the shared onset; releasing the final one
immediately zeros the onset and voice offsets without clearing A#0's target or
ownership.
A fresh refret—including an off-grid same-boundary refret or stopped-to-open-to-
stopped legato—therefore blooms from rest. Overlapping ownership of the same
held or delayed note preserves the onset already in flight.
Multiple held strings share one
stroke direction and Strum traversal. If a wide traversal is still in flight,
the next grid contact is skipped rather than replacing a pick that has not yet
reached its string. The free-running 4..20 strokes/s control avoids inventing
host-transport or pattern-editor state; DAW users can still sequence E6..B6 for
arbitrary synced rhythms.
Each playable note is
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

The pitch wheel applies the same standard ±2 semitone bend to every played and
sympathetically ringing string, so a chord stays in tune while all strings
travel over the Bend Time parameter rather than snapping. Channel pressure and
polyphonic aftertouch are deliberately unassigned, so controller pressure
cannot silently alter pitch. The modulation wheel (CC 1) is the
performance resonance: it raises the sympathetic coupling from the
Sympathetic Ring parameter toward total and opens an acoustic feedback path
from the amplified output back into the strings, scaled by the Resonance
Depth parameter — with the amp or distortion up and the wheel raised, a held
note regenerates into self-sustaining feedback, and a dry DI never can. The
amplified return crosses a sample-rate-derived 5.805 ms nominal air path
(roughly two metres at the speed of sound); causal internal chunks keep that
distance fixed at any host block size. Played strings receive the full bounded
acoustic return; idle strings retain their bridge-driven sympathetic bloom but
receive one quarter of the direct return, modeling the light unused-string
control of a high-gain performance. The
sustain pedal (CC 64) holds released strings; breath/CC 2 adds continuous
bridge-hand pressure on top of the Mute Pressure parameter. While CC 2 is above
zero, the engine status shows `CC2 MUTE +NN%`; the Mute Pressure knob remains
the independent host parameter, so remembered controller pressure cannot hide
behind a 0% knob. CC 120/123 behave as All Sound Off and All Notes Off.

### Sound architecture

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
  the same place, and four quantities are drawn once for every physical wrist
  stroke, then shared by every string it crosses: where along the string the
  pick lands (4 mm of standard deviation, about 5% of the
  pick-to-bridge distance at the default Pick Position, so the pluck comb's
  first notch moves by about as much), how hard it is driven (0.6 dB), how far
  off the plane it is held (6 degrees, which is not a free parameter but exactly
  the split between the two polarisations, so it is applied as a rotation of
  that split and takes no energy with it), and how much of the tip is touching
  (8%, carried by the release pulse length). Each draw is three summed uniforms,
  so it has unit variance exactly and cannot leave ±3 sigma — the bound the
  plectrum's own width and the hand anchored on the bridge set. The variation
  rides on top of the up/down stroke colouring rather than replacing it, so
  alternate picking gets it too. A hammer-on or legato slide has no plectrum
  and therefore cannot consume or apply a picking-hand stroke. Measured on
  twelve identical note-ons twelve
  seconds apart with the noise controls and Artifacts at zero, successive
  strokes differ over their first 150 ms by -16.5 dB on the mean where they
  differed by -84.6 dB before, peak level spreads 2.49 dB where it spread
  0.012 dB, and the attack's spectral centroid spreads 17.2 Hz on a 456 Hz
  centroid where it spread 0.38 Hz. Every draw is a pure function of the first
  accepted plectrum event's start order and assigned string, so identical MIDI
  still renders identical audio while a chord remains one coherent hand
  gesture.
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
  the whole way. It does not inject another pluck at the destination: only the
  preserved string and the travelling finger's friction sound. Its duration is
  a distance divided by a hand speed rather than
  a fixed time — the Bend Time control sets 8% of itself per fret, so the
  280 ms default is 22 ms per fret and a twelve-fret slide takes six times as
  long as a two-fret one. While the finger moves it drags across the winding,
  and the ridges pass under it at `v / w`, the hand's speed along the string
  over the winding pitch, which is exactly why a fast slide squeaks high and a
  slow one low; the level follows the derivative of the glide, so the squeak
  swells and dies with the movement and is exactly zero when the finger is
  still. A plain string has no winding and barely squeaks. The Finger Noise
  control sets the level and silences it exactly at zero. A gesture received
  mid-glide takes its source pitch, fractional fret, remaining distance and
  direction from the live smoothstep position rather than the old destination;
  phase-compensated delay translation keeps the effective pitch continuous at
  the handoff and through in-flight dispersion refits.
- **Frets:** fretting position drives sounding length, inharmonicity,
  pickup comb geometry, and Fleischer-style dead-spot damping (deeper on
  the bolt-on end of the construction axis). The Artifacts path can open a
  decaying fret-collision window on hard-picked notes that soft-limits
  displacement and re-radiates deterministic rattle. Hammer-ons retarget a
  sounding loop without clearing its state.
- **Attack pitch:** hard strokes keep the written pitch across all eight
  strings. A force-derived tension-modulation prototype was removed after an
  absolute-pitch audit found that its one-recording E1 calibration pulled
  different strings sharp by different amounts and made low chords sour. Real
  plucked strings can show nonlinear attack glide, but that path remains
  withheld until matched, absolute-pitch, multi-string and multi-velocity
  captures can bound it. The pitch wheel moves every delay target by the same
  ±2 semitone ratio over the Bend Time glide. Hold the visible A#0 **VIB** key
  for upward-only fretting-hand vibrato; its velocity sets width, each stopped
  string has an independent finger phase, repicks of a live held note preserve
  that finger, and a pre-held gesture waits at zero for a physically held
  stopped finger. Open strings and released or sustain-held tails do not move;
  a held stopped sibling keeps the shared onset, while the final stopped-key
  release resets it so a fresh refret blooms from rest.
  Channel pressure and polyphonic aftertouch remain unassigned.
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
  meets the frets, while what the pick puts into the string is set by its slip
  time `t_s = Z d / F + Z / k` — the
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
- **Dead notes:** the fretting hand lies across the strings without pressing
  them to a fret. It is a whole-hand contact away from the bridge, so it has its
  own broadband loss rather than being the tight end of Mute. The old 30 ms
  choke contradicted low-string ghost-note recordings and has been removed:
  Dead now uses a 1.6 s contact target, fits its upper loss at the eighth
  partial and applies only 15% of the hand amount to the attack path. In the
  stateful Open -> Mute -> Dead -> Dead E1 comparison, its 30-100/100-250/
  250-380 ms levels are -8.10/-15.11/-23.42 dB and its power centroids are
  209/142/94 Hz, against 221/137/85 Hz in the four-hit reference. Its picked
  onset stays within 0.47 dB of Sustain and 99.88% of tracked harmonic power is
  below 250 Hz: a dark periodic thunk, not a gate. Mute Pressure can stack as
  the separate bridge hand; Mute Tightness changes only the Mute style.
- **Bridge-hand damping:** the heel of the hand is a passive absorber, so its
  loss runs in parallel with the string's own and the decay rates add. It is
  present when the pick releases, may relax only after the string establishes
  a real energy peak, and is selective: pressure sensing and matched dry-note
  evidence agree that a bridge-near hand removes the post-attack band above
  500 Hz much faster than the low body. Electry therefore divides the hand rate
  by twenty-two at the fundamental, multiplies it by 4.5 at the high reference
  and pairs that 99:1 fitted-point tilt with a loss band centred on five times
  the fundamental. Mute Tightness spans 2.60 s to 0.32 s for the Mute style;
  Mute Pressure and MIDI CC 2 span 4.0 s to 0.080 s for the live bridge hand
  and further tilt the high-frequency ratio by `1 - 0.38 p`. Every positive
  target goes through the bounded loop-filter solve and analytic phase
  compensation. When a ringing loop is refitted, the old-to-new damping-phase
  difference at its previous compensated period is translated immediately into
  the raw delay coordinate; requested bend, vibrato and refret motion stays on
  the pitch smoother. A live contact therefore changes decay and colour without
  detuning the established string, and adjacent CC2 values remain smooth. Zero
  pressure with a non-Mute style is an exact no-op.

  Mute uses the same sustained triangular-displacement load as Open. Its
  lighter attack voicing supplies the pick edge while the selective steady
  hand loss shapes the body; there is no extra fixed-duration heel transient.
  The final hand rate scales with the stroke force already latched for that
  attack. CC2 and keyswitches
  condition an attack at the MIDI event boundary, and the short palm-impact
  state begins only when the plectrum actually reaches a delayed string. Mute
  Pressure is already included in sympathetic-loop damping and is not charged
  a second time by the shared hand. Across ringing strings, the shared physical
  hand follows the newest *actual* Mute/Open/Dead contact: a scheduled note
  cannot move it during lookahead, a new contact re-solves older active loops,
  and each voice still keeps the play style captured at its own attack. A
  Hammer, pull-off or legato Slide moves only the fretting hand, so it retains
  any planted Palm damping without claiming this shared contact clock.
- **Strum travel:** the processor collects every ordinary positive Note On at
  one exact sample position and solves that complete chord before starting a
  voice. It therefore knows the physical edge up front: a downstroke begins on
  its lowest picked string, an upstroke on its highest, and the leading contact
  is immediate. Enabling Strum Spread on a one-note riff is an exact timing and
  audio no-op. A later MIDI timestamp is performed timing and begins another
  stroke, so rapid prog-metal notes do not merge merely because they are less
  than 35 ms apart. Host insertion order, GUI/host source splitting and audio
  block partitioning cannot change the result.

  The public scalar engine API retains a causal fallback for clients that send
  an unknown chord as successive calls. Those calls may assemble one stroke
  inside a 35 ms window and carry a fixed 20 ms re-anchor pre-roll while a later
  call may still reveal the true edge; that delay is not used by the plug-in's
  complete timestamp batches or by the demo renderer. Under alternate picking
  the direction is captured once for the complete chord, so a strum is never
  asked to travel both ways; the per-string up/down colouring still follows the
  resolved stroke.

  The wrist also accelerates through the strings instead of sweeping at a
  constant speed: entering the string plane at `v0`, `v(x) = sqrt(v0^2 + 2 a x)`
  and the crossing intervals compress as `1 / v(x)`, with the acceleration set
  so the last of seven crossings takes 0.70 of the first. The seven gaps are
  then rescaled to sum to seven times Strum Spread, so the control states the
  *mean* crossing time and keeps its meaning: a short chord at the edge the
  stroke starts from is spread a little wider than the knob says (1.21 times it
  on the first crossing) and one at the far end a little narrower (0.85 on the
  last), because the pick is still speeding up. At a 12 ms spread a complete
  eight-string batch sounds at 0.0, 14.7, 28.1, 40.7, 52.4, 63.5, 73.9 and
  84.0 ms after its MIDI timestamp - 84.0 ms of travel, in gaps falling from
  14.7 to 10.1 ms. The acceleration is drawn once per chord (15% of standard deviation)
  with a small per-crossing draw on top (0.5%), so no two strums lay down the
  same ramp, and a chord anchored on a middle string no longer travels outward
  in both directions at once. A chord therefore sweeps instead of landing as a
  block, and its stacked initial peak drops. At 0 ms no pre-roll is charged and
  no draw is made: chords are exactly simultaneous and the engine is
  bit-unchanged.
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
  pickup per voice; and the picking hand's contact draws, the internal
  vibrato's cycle draws and the strum's ramp solve run only at event rate—per
  contact, cycle and chord respectively—rather than per sample. A pickup the
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
- **Output modes:** Mono is exact dual mono and preserves the conventional
  electric-guitar DI. Stereo spreads one engine's per-string pickup signals
  left-to-right in physical low-to-high string order, keeps the body centred
  and folds down coherently. Double runs two complete, separately seeded
  deterministic Electry engines as mono performances, one on each channel,
  before one shared FX chain. The second player draws one causal 0-6 ms timing
  offset per picked wrist stroke; a chord shares that offset and then keeps its
  requested strum travel. The primary performance and Hammer contacts keep
  their exact clocks. Entering Double resets and relatches the second engine
  rather than cloning already-ringing notes, so choose it before the phrase.
  Neither stereo mode adds chorus, random phase, or a delayed audio copy.

### Amplifier chain

The five FX amount controls and three-choice Amp Voice selector run in the same
JUCE-free library as the string model (`Source/DSP/ElectryFx.*`), so the
complete signal path is regression tested on every platform rather than only
inside a host. Amp Voice selects a complete nonlinear amplifier and
speaker/cabinet path; it is not an EQ preset after one shared distortion curve.

- **Oversampled clipping.** The distortion pedal and the amplifier run inside a
  4x oversampled domain, reached through two cascaded Kaiser-windowed halfband
  stages whose kernels are designed at `prepare()` time rather than tabulated. A
  gain stage fed at host rate folds its own upper harmonics straight back into
  the guitar band, and that folded intermodulation is most of what makes a
  modelled high-gain tone read as digital: across pedal, amplifier, stacked
  and quiet-amplifier steady-tone probes, the current non-harmonic floor is
  -61.3 to -77.1 dB; the previous host-rate chain measured -28 to -40 dB on
  the same class of probes. Above 96 kHz one halfband stage is
  dropped and above 192 kHz both are, because a host already running that fast
  supplies the bandwidth the stages need. While the block is engaged it adds
  17.25 host samples of fixed group delay (0.36 ms at 48 kHz); with both gain
  controls at zero it is skipped outright, costs nothing, and adds no delay at
  all.
- **Pedal.** A tight 88 Hz input coupling network and a mid-focused voice feed
  the 2.2 kOhm / 10 nF antiparallel Shockley-diode node from
  [Yeh, Abel and Smith's DAFx-07 circuit](https://dafx.de/paper-archive/2007/Papers/p197.pdf).
  Its capacitor voltage is state, not a post-filtered memoryless curve: every
  oversampled step trapezoidally integrates the RC differential equation and
  uses a bounded Newton solve for the two diode currents. Passing the whole low
  end of a Drop-E eighth string into the clipping node turns the fundamental
  into intermodulation mud instead of a note, so the input and voice filters
  remain part of Electry's metal voicing.
- **Three circuit families.** American Clean uses a mid-1960s American
  component family (250 pF / 100 nF / 47 nF capacitors, 100 kOhm slope resistor
  and fixed 6.8 kOhm mid leg); British Crunch uses a late British family
  (500 pF / 22 nF / 22 nF, 33 kOhm slope resistor and 25 kOhm mid control).
  The resulting unloaded third-order passive networks are evaluated from
  [Yeh and Smith's exact symbolic transfer](https://dafx.de/paper-archive/2006/papers/p_001.pdf)
  and bilinear transformed at the nonlinear stage's internal rate. Their
  measured 1 kHz insertion losses at 192 kHz are −13.0058 and −5.84512 dB;
  recovery happens before the output section, not as post-cabinet EQ. Modern
  High-Gain deliberately retains the previous Electry path coefficient- and
  operation-for-operation, preserving demos 01–22 byte-for-byte.
- **Voiced for the eighth string.** The Modern input stage passes the whole Drop-E
  fundamental rather than cutting it at 84 Hz, because clipping it is what
  generates the second and third harmonics the cabinet turns into a chug's
  weight; the pre-gain mid emphasis sits at 850 Hz, above the cabinet's scoop
  instead of inside it; the cabinet's thump is deeper and its boxy region cut
  harder; and the compressor's attack is 18 ms rather than 3 ms, so a pick
  attack passes instead of being levelled away. Measured on a chugged Drop-E
  figure, a whole-file Hann spectrum from 20 Hz to 8 kHz puts 29.2% of the
  amplified energy in 80-160 Hz against 12.5% of the dry DI's, while 320-640
  Hz drops from 22.3% to 10.7%.
- **Measured preamplifier.** Every voice uses the measured 12AX7 cathode- and
  grid-current equations published by
  [Dempwolf and Zölzer at DAFx-11](https://dafx.de/paper-archive/2011/Papers/76_e.pdf).
  During preparation a residual-checked bracketed Newton solve evaluates plate
  current against a 250 V supply and 100 kOhm plate resistor over a dense 2.44
  mV grid; every oversampled step linearly interpolates that fixed memoryless
  load-line transfer with at most 0.00000072 normalized error in the regression
  sweep. A standing grid bias,
  level-tracking bias drift and model-specific interstage roll-off retain the
  asymmetric, level-dependent response and darkening of cascaded stages.
- **Measured phase splitters.** American now solves TubeLib's measured-fitted
  ECC81/12AT7 `TriodeK` family in the [Fender Twin Reverb AB763](https://schematicheaven.net/fenderamps/twin_reverb_ab763_schem.pdf)
  82/100 kOhm, 470 Ohm cathode and 22 kOhm-tail component family; British uses
  the distinct ECC83/12AX7 fit with the [Marshall 1959 Super Lead](https://www.prowessamplifiers.com/schematics/Marshall/1959_superlead.pdf)
  82/100 kOhm, 470 Ohm cathode and 10 kOhm/presence-tail family; its 400 V rail
  and 4.7 kOhm presence-tail value follow [Macak and Schimmel's Fig. 11 model](https://www.dafx.de/paper-archive/2010/DAFx10/MacakSchimmel_DAFx10_P12.pdf),
  not an annotated Marshall factory voltage. Their driven
  grids retain the documented 1 nF × 1 MOhm American and 22 nF × 1 MOhm
  British input coupling. The output solve keeps the two plate branches
  independent: American uses the drawing's 100 nF capacitors, 220 kOhm bias
  returns and 1.5 kOhm grid stoppers; British uses 22 nF, 220 kOhm and 5.6
  kOhm. Each terminal then conducts through TubeLib's generic 2 kOhm plus
  `IS=1 nA`, `RS=1 Ohm` diode branch. A three-unknown analytic Newton step
  jointly solves total LTP current and both terminal-grid voltages while each
  capacitor is trapezoidally integrated, so grid current loads the correct
  plate and leaves a real common negative bias displacement after overload.
  Idle remains 4.012 mA / 232.25 V / 225.53 V for ECC81 and 3.180 mA / 261.46
  V / 250.95 V for ECC83; the checked musical burst stays below `1e-10 A`
  residual. One millisecond after the pinned overload the shared American and
  British shifts are −14.84 and −18.98 V; after 20 ms their source-set 100/22
  nF branches retain −7.98 and −1.14 V respectively. TubeLib's diode junction
  capacitance/transit time, the Marshall 47 pF plate-to-plate capacitor, PI
  grid conduction, bias-supply impedance and the complete presence network
  remain explicit later circuit stages.
- **Measured output-tube load lines.** American solves an ideal push-pull pair
  from the measured-curve-fitted uTracer/ExtractModel **6L6GC beam-tetrode**
  family; British uses its distinct **EL34 true-pentode** family. The equations,
  secondary-emission knee terms and fitted constants come from Reefman's
  [TubeLib SPICE library](https://www.dos4ever.com/uTracer3/TubeLib.inc) and
  [derivation](https://www.dos4ever.com/uTracer3/Theory.pdf). The ideal
  centre-tapped transformer reflects one quarter of the documented
  plate-to-plate load to each half: the American solve uses the
  [RCA 6L6GC 450 V plate / 400 V screen / −37 V / 5.6 kOhm AB1 pair](https://frank.pocnet.net/sheets/049/6/6L6GC.pdf),
  and British the [Mullard 400 V / −36 V / 3.5 kOhm fixed-bias pair](https://frank.pocnet.net/sheets/129/e/EL34.pdf).
  Around the retained RCA operating pair, American adds the individual 470 Ohm
  screen branches drawn by the AB763; this is a source-component hybrid, not a
  claim that the complete Twin Reverb DC output stage was copied. British uses
  Mullard's common 800 Ohm screen resistor from the same published pair
  operating condition rather than splicing in the four individual Marshall
  branches. Each screen voltage is solved from TubeLib screen current jointly
  with the plate load line, so rising screen demand compresses the driven
  transfer before the result is baked into the preparation-time tables.
  The pair receives the two solved grids as common and differential drive, so
  blocking's bias shift changes crossover, output and current demand rather
  than being discarded. Preparation solves a 257 × 257 quadratic terminal-grid
  plane over 13 plate/screen-rail levels, stores only its push-pull-symmetric
  triangle and uses 6.576 MiB for both tube families. Runtime
  trilinear interpolation stays below 0.0003 normalized output and 0.0004
  current-demand error in the checked domain; its own interpolated idle is
  subtracted before the sag clamp. The two half-drive outputs remain 0.48185
  and 0.643711 after equal small-signal normalization. A terminal may cross
  zero and load its driver through the diode, but the uTracer-fitted
  plate/screen-current surface is conservatively clamped at `Vg=0`: this is an
  AB1-bounded power-transfer surface with bounded overload grid conduction, not
  an invented ideal AB2 source. The American upstream 1 kOhm / 20 uF common
  screen node, the Marshall quartet and choke supply, full positive-grid plate-
  current data and a reactive loudspeaker-reflected load remain open circuit
  stages. Modern's separate established output path is unchanged.
- **Feedback and supply.** A causal, bandwidth-limited copy of transformer
  output represents a secondary-side negative-feedback return, with voiced
  normalized coefficients of 0.42 for tighter American response and 0.12 for
  British. Plate-plus-screen current now drives each supply follower, and its
  stored state changes the tube solve's physical plate and screen rails rather
  than scaling the result afterward. The American reservoir spans up to 20%
  rail loss with 55/550 ms attack/recovery, British 24% with 80/300 ms, and the
  established Modern path 30% with 70/400 ms. On the pinned loud/quiet probe
  the three nonlinear gain reductions are −13.70, −15.73 and −18.33 dB;
  full-chain 90% Drop-E gains are +2.86, +6.34 and +5.87 dB. Feedback amounts,
  reservoir constants and level trims are voiced and regression-pinned, not
  measured component tolerances from named products.
- **Transformer and speaker/cabinet voice.** Each path has independent
  transformer high-pass and flux-integrating saturation state; the voiced flux
  corners are 35, 55 and 45 Hz. Six zero-latency biquads then provide an
  American open-combo-style broad voice, a woody British closed-stack-style
  voice, or the Modern sealed metal thump, 430 Hz cut and 3.1 kHz presence. All
  three end in a fourth-order loudspeaker roll-off inside the oversampled domain
  before decimation. These are parametric response models—not cabinet impulse
  responses, individual speaker solves, microphones or room captures.
- **Level and topology.** Each gain stage divides its own small-signal gain
  back out, so reaching for a drive control is a change of tone rather than a
  jump in level; a saturating stage still ends up a few decibels louder than
  the dry DI because compressing a signal raises its average. Zero remains an
  exact bypass, but any nonzero steady Distortion or Amp setting places the
  whole circuit in series. The engagement crossfade uses a 15 ms exponential
  smoothing time constant only while a control crosses zero; it is not a
  permanent dry blend around the diode node, transformer or cabinet. Amp Voice
  uses the same time constant between independent recursive circuits; after the
  weights settle only the selected circuit runs and inactive state is cleared.
- **Compressor, delay, room.** The compressor eases into roughly 3.5:1 above
  -20 dBFS through a soft knee, with makeup, so a palm-muted part sits still
  instead of the level grabbing at every pick attack. The 360 ms lead delay
  damps its feedback path, so repeats darken and thin as an analogue delay's
  do. The room is three allpass diffusers into two damped combs per channel at
  coprime lengths, with no modulation, Haas delay or randomised phase — the
  same constraint the instrument's own stereo field obeys.
- **Bypass and smoothing.** All five amount controls are smoothed per sample rather
  than stepped per block, and each snaps to exactly zero, so a control left at
  zero is a bit-exact dry bypass — verified by the regression suite — while
  engaging or disengaging either gain circuit and its oversampled block is
  crossfaded and cannot click.
- **Cost.** With Modern High-Gain selected, Stereo on an Apple M1 Max, best of
  three six-second runs at 48 kHz:
  0.002x realtime in bypass, 0.075x for Distortion alone, 0.025x for Amp alone,
  0.086x for the demonstrated 45% Distortion / 95% Amp metal chain, and 0.089x
  with all five controls at maximum. At 96 kHz the corresponding metal/all-max
  figures are 0.169x/0.178x; the all-max path remains below 0.19x through 384
  kHz. A fresh optimized amp-only matrix for the two coupled circuits measures
  American/British at 0.217/0.221x realtime at 48 kHz, 0.421/0.434x at 96 kHz
  and 0.429/0.440x at 384 kHz; Modern in the same harness is 0.028/0.055/0.055x.
  A deliberately continuous A↔B crossfade is 0.430/0.842/0.845x and pathological
  three-way switching remains below 0.90x; settled selection runs only one
  circuit. All timed process rows make zero heap allocations. The two 3.288
  MiB pair tables take about 0.65 s once per process to construct during the
  first `prepare()`, while same-rate reprepare is about 0.55 ms. For reference
  the eight-string model itself runs at roughly 0.13-0.17x realtime at 96 kHz.

### Guitar Build

One host control now replaces the six separate Wood, Size, Shape, Neck Join,
Scale and Gauge dials. **Guitar Build** travels through six generic anchors;
between adjacent anchors a smoothstep interpolation co-moves the internal wood
damping, body mass, modal shape, neck/bridge joint, scale length and Drop-E
gauge coordinates. This is a curved path through plausible combinations, not
a diagonal sweep from every minimum to every maximum and not a selector for
named guitars.

| Position | Generic anchor | Coordinated direction |
| ---: | --- | --- |
| 0.0 | Slab fixed | bright/light flat body, fixed bridge, conventional scale and light set |
| 0.2 | Contoured | lighter contoured body, slightly extended scale and light set |
| 0.4 | Angular set | denser angular body with a set-style joint and slightly firmer set |
| 0.6 | Modern bolt | light modern body, bolt-style joint and intermediate extended scale |
| **0.8** | **Dense extended** | **thick carved/set-style Drop-E build, 27.63-inch scale and heavy set** |
| 1.0 | Neck-through | balanced extended-scale build with a neck-through direction and medium set |

The 0.8 anchor is the current fitted default. Pickup selector and coil type,
Tone, Body Resonance amount, string age and all picking/hand controls remain
independent, so Guitar Build changes the instrument under the player without
silently moving the pickup or the performance.

The fit that selected the default remains the same evidence: against nine
muted references at five pitches the joint error is 5.03 dB, compared with
6.31 dB at the old all-midpoint prototype. Moving only the four structural
weight coordinates scored 6.41 dB; moving Pick Position away from 0.18 cost
2.1 dB and selecting the neck pickup cost 1.5 dB. The result is a fitted whole
voicing rather than evidence that any single material coordinate identifies a
guitar.

The earlier isolated-coordinate audit is retained because it motivated the
single rich trajectory. Sweeping each internal coordinate end to end by the
same normalised-difference measure the suite uses produced:

| Axis | On the old midpoints | On the new defaults | Suite floor |
| --- | --- | --- | --- |
| body wood | 0.058 | 0.047 | 0.055 |
| body size | 0.106 | 0.045 | 0.055 |
| body shape | 0.066 | 0.042 | 0.055 |
| construction | 0.065 | 0.058 | 0.055 |
| string gauge | 0.125 | 0.050 | 0.080 |
| body resonance | 0.080 | 0.028 | 0.080 |
| pick position | 1.341 | 1.346 | 0.350 |

At the fitted default, five isolated structural/gauge sweeps lose between a
tenth and two thirds of their relative range, and four sit below their original
suite floors. A darker, heavier, louder note makes each isolated coordinate a
smaller fraction of the whole; Body Resonance is worst hit. The current
`testGuitarBuildRangeIsAudible` therefore measures the six complete anchors,
while the lower-level material tests state the instrument they measure instead
of inheriting the default.

### 28 host parameters

Electry is unreleased, so this development parameter layout makes no
compatibility promise to earlier snapshots. It exposes one Guitar Build
parameter in place of six construction axes, one three-choice Output Mode
parameter in place of a binary field plus a separate Double switch, and one
three-choice Amp Voice selector. Tonal continuous controls are smoothed inside
the engine; Tremolo Rate, Strum Spread and Bend Time intentionally reach their
schedulers directly, pickup and output-mode changes crossfade over roughly
4 ms, and Amp Voice crossfades independent circuit state with a 15 ms
exponential smoothing time constant.
The new `ampModel` field is appended, so the first 27 host indices stay fixed;
development states that do not contain it explicitly migrate to Modern
High-Gain.

| # | ID | Name | Range and default |
| --- | --- | --- | --- |
| 1 | `pickupSelector` | Pickup selector | Neck / Both / **Bridge** |
| 2 | `pickupType` | Pickup type | 0..100%, default 32% |
| 3 | `tone` | Tone | 0..100%, default 70% |
| 4 | `guitarBuild` | Guitar build | 0..100% through six smooth generic anchors, default **80% Dense extended** |
| 5 | `bodyResonance` | Body resonance | 0..100%, default 35% |
| 6 | `stringAge` | String age | 0..100%, default 30% |
| 7 | `pickPosition` | Pick position | bridge..neck, default 18% |
| 8 | `pickHardness` | Pick hardness | 0..100%, default 58% |
| 9 | `pickNoise` | Pick noise | 0..100%, default 50% |
| 10 | `fingerNoise` | Finger noise | 0..100%, default 40% |
| 11 | `releaseNoise` | Release noise | 0..100%, default 40% |
| 12 | `muteDamping` | Mute tightness | 0..100% loose-to-tight Mute-style bridge-hand loss, default 55% |
| 13 | `bendTime` | Bend time | pitch-wheel travel time, 40 ms..2 s, default 280 ms |
| 14 | `velocity` | Velocity response | 0..100% exponent on the pick's force (0% is velocity-invariant), default 85% |
| 15 | `output` | Output level | -24..+6 dB, default -6 dB |
| 16 | `artifacts` | Artifacts | clean bypass..ring/contact/saddle detail, default 18% |
| 17 | `outputMode` | Output mode | **Mono** / Stereo divided-pickup field / Double independent performances |
| 18 | `distortion` | Distortion | bypass at 0%; otherwise drive through the fully connected oversampled RC diode circuit, default 0% |
| 19 | `amp` | Amp simulation | bypass at 0%; otherwise drive through the complete selected circuit-derived preamp, output stage and speaker/cabinet path, default 0% |
| 20 | `compressor` | Compressor | dry..fast rhythm levelling, default 0% |
| 21 | `delay` | Delay | dry..360 ms lead delay, default 0% |
| 22 | `room` | Room | dry..compact stereo ambience, default 0% |
| 23 | `sympathetic` | Sympathetic ring | exact bypass..full bridge coupling, into the unfingered strings and between the fingered ones, default 20% |
| 24 | `palmMute` | Mute pressure | 0..100% continuous bridge-hand damping for every play style (adds to MIDI CC 2), default 0% |
| 25 | `strumSpread` | Strum spread | 0..40 ms mean travel per crossed string plus 20 ms pre-roll when nonzero; different-string notes up to 35 ms from the stroke's first event group, while same-string reuse starts a new stroke; default 0 ms |
| 26 | `resonanceDepth` | Resonance depth | 0..100% full-scale reach of the CC 1 resonance (coupling lift and amplifier feedback), default 35% |
| 27 | `tremoloRate` | Tremolo picking rate | 4..20 strokes/s for the momentary B0 TRM wrist, default 12 strokes/s; appended after the published controls so their host automation indices remain unchanged |
| 28 | `ampModel` | Amp voice | American Clean / British Crunch / **Modern High-Gain**; switches the complete amp, output dynamics, transformer and six-section speaker/cabinet voice, with legacy development states defaulting to Modern |

### References and claim boundaries

| Block | Reference | What Electry implements | Precise claim |
| --- | --- | --- | --- |
| String core | Karjalainen, Välimäki, and Tolonen's single-delay-loop condensation of digital waveguides | Eight independent strings in Drop-E tuning, each with two transverse-polarisation single-delay-loop waveguides, third-order Lagrange fractional reads, and a contractive bridge coupling matrix | The published SDL string family with two coupled polarisations per string; not a bidirectional multi-rail scattering simulation |
| Two-stage decay and beating | Two-polarisation string behavior described in the same plucked-string literature | The polarisation parallel to the body carries a 1.7x longer decay target and a sub-cent detune, so the mixed output beats slowly and decays in two stages. Both the detune and the exchange between the polarisations are fractions of a round trip rather than fixed numbers of samples, so neither follows the host clock; the loop filter's own two-frequency fit still does, leaving a measured 4.5 dB residual spread across 44.1-192 kHz at the top of the range | A qualitative reproduction of the documented mechanism with voiced constants; not calibrated polarisation data from a measured instrument |
| Stiffness dispersion | Stiff-string inharmonicity `B = pi^3 E d^4 / (64 T L^2)` (Fletcher and Rossing) and robust factored allpass design practice (Rauhala and Välimäki; Abel and Smith) | A per-note `B` from string diameter, effective wound-core bending fraction, scale length, and tension drives an eight-stage factored first-order cascade; two coefficients are fitted jointly at low and high partials, with exact fundamental phase compensation | A physically derived, bounded two-band fit whose regression error is under 20% at both references for the worst heavy Drop-E case; not a capture-fitted very-high-order piano dispersion filter |
| Loop damping and tuning | Decay-time-targeted loop-filter design from the plucked-string literature; a dry electric low-E reference recording for the targets themselves | Per-string, per-fret one-pole loop filters solved by bisection from independent T60 targets at the fundamental and a high reference frequency, with all loop-filter phase delays compensated analytically at the fundamental. The wound strings' fundamental targets are tens of seconds and their high-frequency ratio two orders of magnitude smaller, following the reference | Decay-targeted loop design with exact fundamental tuning (regression bound: under 8 cents across E1..D6 at tested host rates through 384 kHz), whose fundamental and high-frequency targets are calibrated against one reference recording; not per-partial measured decay matching across a fretboard, and not a model of the reference instrument |
| Dead spots | Fleischer's electric-guitar dead-spot studies relating neck conductance to decay time | A per-string fret-position Gaussian that locally shortens decay, deepened by the bolt-on end of the construction axis | The documented mechanism direction with voiced positions and depths; not measured conductance maps of specific instruments |
| Attack pitch | Tolonen, Välimäki, and Karjalainen's tension-modulation nonlinearity; Avanzini, Marogna, and Bank's quasistatic energy store | The shipping string keeps its compensated fundamental delay independent of pick velocity; a regression checks maximum-velocity Sustain on all eight isolated opens and a simultaneous low chord, then the three isolated low strings under Mute and Dead | Nonlinear tension glide is a documented future mechanism, not a shipping claim: the single-recording force prototype failed absolute multi-string tuning and remains withheld until matched captures identify a bounded model |
| Plectrum and finger excitation | Plectrum and touch interaction modeling by Germain and Evangelista and by Evangelista and Eckerholm | A three-phase picked excitation combines a conservative contact-loss placeholder and scrape, a string-period-scaled modal release approximating triangular pluck displacement, a mass-dependent release pole and a smaller broadband pick edge; an asymmetric constant-area release and a 0.5-1.5 mm contact patch vary with Pick Hardness, while deterministic per-stroke draws vary force, position, angle and tip contact. Hammer/tap contacts bypass the wrist, plectrum-contact and pick-control paths; legato slides preserve the ringing loop and add only finger friction | A realtime modal approximation to released-string displacement plus bounded contact and pick detail, with one explicit physical boundary between plectrum and fretting-hand gestures; not an exact delay-line initial-condition solve, beam-mechanics plectrum profile, force-based finger contact solver, or local bidirectional plectrum-scattering junction |
| Fret collisions | Bilbao and Torin's energy-balanced string/fretboard collision modeling | The Artifacts path's incidental fret contact: a decaying collision window whose soft limit clips vertical displacement against a velocity-dependent clearance and re-radiates deterministic rattle noise on hard-picked notes | Collision-informed contact behavior in a bounded, stable form; not an FDTD distributed-contact simulation |
| Pinch harmonic | The same touch model driven by the picking hand; standard descriptions of the technique as a thumb contact immediately after the plectrum | The touch position is the pluck fraction, so Pick Position selects the partial; a firmer (depth 1.0) and longer (90 ms) contact than the fretting finger's, because the mode-shape law gives a near-bridge touch little purchase on the low partials | Node selection by hand position with the technique's own asymmetry between low and high partials preserved; not a model of thumb geometry, pick grip, or the exact contact area |
| Touch harmonics | The touch-interaction half of Evangelista and Eckerholm's player/instrument models, and the classical mode-shape result that a point contact removes energy as `sin^2(n pi p)` | A one-tap FIR `(1 - d/2) + (d/2) z^-M` with `M = p * period` inside each polarisation loop, which *is* the `sin^2(n pi p)` weighting rather than an approximation of it; unity at a node, `1 - d` at an antinode, magnitude bounded by one at every depth. The natural harmonic touches the midpoint, so the octave is the string's own even series with its own inharmonicity, decay and pickup comb; the finger lifts once the note has formed | An exact first-order point-contact loss condensed into the delay loop, exact in magnitude and phase at the surviving partials whenever the touch sits on a node; not a distributed finger-force contact solve, and not exact at a non-node touch position |
| Slide | Pakarinen, Puputti, and Välimäki's virtual slide guitar, whose string algorithm carries a parametric model of the tube/string contact noise produced by a wound string's surface ridges | The finger stays down and the sounding length glides at a hand speed in frets per second rather than over a fixed time; the friction is a noise band centred at `v / w`, the hand's speed along the string over the winding pitch, with its level following the derivative of the glide. A chained gesture samples the live log-frequency and fractional fret, derives duration and friction from the remaining physical path, and translates raw delay for filter-phase changes so the complete loop period remains continuous | A finger-position- and effective-pitch-continuous time-varying waveguide plus a velocity-dependent friction band, with the winding pitch a fitted linear stand-in for real wrap-wire practice; not a velocity-continuous spline, an energy-compensated time-varying waveguide, or a measured contact-noise spectrum |
| Hammer-on and pull-off | Touch/legato interaction models from Evangelista and Eckerholm | Keyswitched legato: a sounding string within reach retargets its delay over about 10 ms while the loop state is preserved. An ascending note gets the established soft finger impact; a descending note, including a release to an open string, excites the old fret's position in the lateral plane, both without plectrum noise. A mid-glide Hammer takes its source and direction from the live fractional fret while phase-compensated delay translation preserves effective pitch. Neither gesture moves a planted Palm hand or rewrites sibling damping | Pitch-continuous, direction-aware legato with conservatively voiced finger attacks; not a velocity-continuous spline or a distributed or capture-fitted finger-force model |
| Pickups | Paiva, Pakarinen, and Välimäki's pickup acoustics and modeling; low-frequency pickup nonlinearity measurements (Novak et al.); engineering aperture analyses | Per-string pickup-position combs follow each fret, with the delayed tap weighted 0.60 so the null is 12 dB deep rather than infinite, as a real aperture, two-coil sum and three-dimensional field never cancel exactly; an O(1) fractional rectangular moving average gives the finite aperture's exact sinc response; bounded flux nonlinearity plus shallow string-mass/pole balance is differentiated into induced EMF, guarded ultrasonically, then passed through the loaded coil/tone circuit | The published pickup signal structure (position comb of measured rather than ideal null depth, finite aperture, nonlinear flux, induced voltage, electrical resonance) with datasheet-plausible level calibration; not a magnetic finite-element, per-coil, or capture-fitted model of named pickups |
| Solid body | Solid-body bridge-admittance and dead-spot literature; geometric estimates | Structural bridge displacement is differentiated before four double-precision, peak-normalised modal resonators and a 4 kHz guard, producing body-induced voltage before the loaded pickup coils; positive real modal conductance across each note's first six partials can only shorten loop T60 | Geometry-informed structural pickup voltage plus passive mode-dependent energy extraction; not undifferentiated acoustic body displacement mixed into pickup voltage, and the mode tables remain voicing estimates rather than measured admittance data |
| Guitar Build | Solid-body material/geometry contrasts, set/bolt/neck-through joint practice, and modern extended-range scale and gauge practice | One host parameter smoothly follows six generic anchors through internal wood damping, body mass, modal shape, joint/bridge, 25.5-to-28-inch scale and Drop-E gauge coordinates; pickup construction, Tone and Body Resonance amount remain independent | A broad continuous construction trajectory; not a licensed or capture-verified reproduction of a named instrument, nor evidence that one material alone determines electric-guitar identity |
| Play noise | Handling-noise observations in the virtual slide guitar work of Pakarinen, Puputti, and Välimäki | Deterministic seeded plectrum scrape, finger contact, and release damping noise, band-shaped per string (wound vs plain) and split between a one-percent string trace and local pickup/body paths | Procedural, deterministic contact noise consistent with the documented mechanisms; not convolved recordings or measured contact-noise spectra |
| Sympathetic string coupling | Bank and Karjalainen's passive admittance modeling and the sympathetic-string literature | The plucked strings' bridge force drives a one-sample-delayed bus; every string that is not being fingered runs its own single-polarisation waveguide at its open pitch, with a loop filter solved from the same pair of decay targets a played string of the same steel gets - the high-frequency one backed off toward the fundamental's wherever the pair would ask the loop for a gain above unity, so the fundamental's target is never the one given up - exact fundamental phase compensation and bridge pickup tap. Only played voices write to the bus and only idle voices read it | A one-directional (loss-only from the driver's point of view) slice of bridge coupling, provably acyclic and therefore unconditionally stable; not a shared multiport bridge scattering junction with mutual re-radiation |
| Dead note | Fretting-hand dead-note distinctions and distributed player/string contact; four CC0 Drop-E eight-string ghost attacks | An independent additive fretting-hand loss with a 1.6 s low-order target, its upper fit at the eighth partial, and a lightly darkened pick attack; Mute Pressure may stack as the separate bridge hand | A contact loss inside the loop whose stateful E1 envelope and centroid track the four-hit reference; not a gate, maximum palm mute, distributed finger-force solve, or universal calibration of dead-note hand coverage |
| Bridge-hand damping | Palm-muting practice; pressure sensing by Biral, d'Alessandro and Freed; post-attack spectral evidence from Reboursiere et al. and Guitar-TECHS; HiMMP's score-matched rhythm DIs; the CC0 extended-range `50hz-guitar` muted/sustained matrix; the same decay-targeted loop design; and dry muted power-chord references for the depths | The hand is an absorber whose loss adds to the string's own in parallel, so decay rates sum at each fitted frequency independently; its solved spectral loss is present when the pick releases and can relax only after the string establishes a measured energy peak. The raw hand rate is multiplied by 4.5 at the high reference and divided by twenty-two at the fundamental, an effective 99:1 ratio between the two fitted points. Mute Tightness continuously controls that loss for the Mute style; Mute Pressure retains it on every style, the newest actual picked contact updates the shared hand on already-ringing strings without rewriting their attack style, and an old-to-new filter-phase translation keeps those live contact changes pitch-continuous. Non-plectrum legato retains an existing Palm | Progressive additive damping with an independently voiced pick attack; not a distributed hand/string force solve, measured heel footprint, commissioned per-harmonic fit, or capture fit to a named eight-string |
| Fretting hand | Ordinary left-hand kinematics; [Itoh and Hayashida's constrained fingering optimisation](https://www.jstage.jst.go.jp/article/ieejeiss/124/7/124_7_1396/_article/-char/en) and [Yazawa et al.'s playable-configuration enumeration](https://cir.nii.ac.jp/crid/1573387452726377216); the position/reach controls exposed by sampled guitars | Exact-sample chord attacks are matched across all eight strings as one bounded problem: held-note and legato continuity, occupied strings, a four-fret hand shape, fret effort and uncrossed pitch order resolve before any voice starts. The chosen shape then enters the ordinary physical attack path in canonical pitch order, so host event order cannot alter the fingering or player-variation stream. Single notes retain the floating hand, out-of-reach shift and phrase return | A deterministic chord-local configuration solver with a fixed four-fret reach; not finger-by-finger anatomy, chord naming, or phrase-wide look-ahead |
| Strum travel | Ordinary plectrum kinematics | Note-ons on different strings no more than 35 ms from the chord's first event are one stroke; its direction and extreme string set an accelerating travel order, every crossed string shares that direction, and Alternate advances once for the chord. Reusing a string starts a new stroke; a fully cancelled pre-contact chord consumes none | Deterministic, jittered accelerating pick travel across the string plane; not a model of pick angle, chord recognition, or the player's wrist trajectory |
| Tremolo picking | Official Shreddage Hydra, RealEight, Electric Storm Deluxe, Evolution Dracus and Heavier7Strings repetition workflows; Armondes' five-player direct/progressive tremolo experiment; Electry's planned exact-eight capture protocol at 8/12/16 strokes/s | Hold visible B0 to run one deterministic wrist through the existing physical repick path. One shared fractional phase preserves Alternate/chord direction, velocity remains force, rate is 4..20 strokes/s with a 12/s default, an in-flight Strum traversal cannot be overwritten, a same-string legato move retargets rather than cancels its travelling pick, and each contact on a live, ringing held note leaves its vibrato finger intact. Because B0 and E6..B6 explicitly command the picking hand, a latched Hammer becomes neutral Sustain for that dedicated contact only and remains latched for later playable notes | A playable sample-accurate repeat scheduler whose 12/s default overlaps the published conventional-guitar direct-speed cluster and whose 8/12/16 anchors are ready for commissioned exact-eight capture; not host-synced pattern generation, a human timing distribution, or an exact eight-string rate/force fit |
| Fretting vibrato | Guitar-TECHS CC-BY raw DIs; Magalhães et al.'s eight-player electric-guitar vibrato study | Hold the visible A#0 gesture; velocity controls a smooth-onset, upward-only width, with independent rate, depth and phase draws per physically held stopped string. Pre-held intent waits for a stopped finger; open and key-released strings remain fixed. Picking-hand repicks and overlapping ownership of the same held or delayed note preserve its onset and finger, while a released/refretted note assigns another. Velocity 64 is nominally about 5.6 Hz / 20 cents and 127 about 6.4 Hz / 40 cents | A playable finger-rock model whose existing range overlaps published six-string players; not an exact eight-string calibration or a finger/string force solve |
| Pitch control | Channel-wide MIDI pitch bend | The wheel applies the same ±2 semitone offset to every played and sympathetically ringing string over the Bend Time glide. Channel pressure and polyphonic aftertouch are deliberately unassigned | Uniform, chord-safe MIDI pitch bend; not a physical vibrato-bar simulation, and no pressure message silently changes tuning |
| Amplifier feedback | Acoustic guitar-to-amplifier feedback practice: a loudspeaker's pressure field re-excites the strings, while high-gain players control unused strings | A sample-rate-derived FIFO holds a voiced nominal 5.805 ms acoustic delay (256 samples at 44.1 kHz), while the plug-in renders, amplifies and returns causal chunks no longer than that delay so DAW block size cannot select the howl. A soft-clipped, gain-scaled copy drives played strings fully and idle sympathetic strings at a voiced one-quarter direct share while leaving their bridge drive unchanged. CC1 resonance, Resonance Depth and rig acoustic loudness scale the path, so a distorted tone at full wheel regenerates while a dry DI never can; every element is bounded | A fixed-delay, level-gated, saturating regeneration path with a performance-voiced unused-string share; not a measured player-to-speaker distance, finger-by-finger muting, room acoustics, speaker directivity, or a standing-wave model |
| Controllable artifacts | The same touch/collision literature plus bridge-hardware behavior | An exactly bypassable deterministic path combines a bridge-hardware modal bank driven through the selected pickup mix, incidental fret contact on hard-picked notes, and per-string saddle rattle, all driven by played energy. It is mechanical hardware noise, distinct from the sympathetic string coupling above | Plausible procedural imperfection with bounded feed-forward resonators; not measured hardware-noise statistics |
| Audible-work culling | Standard realtime-DSP practice | A pickup faded out by the selector is skipped entirely; Mono runs one shared coil/DC/decimation chain and mirrors it; damping-only control moves reuse the existing dispersion fit; the whole engine freezes to exact zero once nothing vibrates and the shared path is below -120 dBFS | Removal of inaudible arithmetic with the audible result unchanged; not a quality/latency trade |
| Oversampling | Standard nonlinear-audio antialiasing practice | The complete physical, body, collision, and nonlinear pickup path runs at 2x for host rates through 96 kHz, followed by a fixed 63-tap halfband FIR; higher-rate hosts run 1x | Genuine internal oversampling and filtered decimation, not a quality label applied to a native-rate nonlinear stage |
| Output modes | Phase-coherent divided/hex pickup practice, ordinary double-tracked guitar performance, and four CC-BY HiMMP rhythm DIs | Mono is the conventional summed DI. Stereo weights one engine's modeled strings by physical lateral position and folds coherently to mono. Double runs two complete, differently seeded mono engines into left and right before one shared FX chain; the second gets one deterministic 0-6 ms causal timing offset per picked wrist stroke, shared across a chord and composed with strum travel | Stereo is a virtual divided-pickup string field and Double is two deterministic modeled performances rather than a delayed copy. Its tight timing envelope is directionally grounded in conventional Drop-C takes, not capture-fitted eight-string timing, and it does not claim to reproduce the decisions of two human performances |
| Distortion pedal | [Yeh, Abel and Smith's antiparallel-diode RC formulation](https://dafx.de/paper-archive/2007/Papers/p197.pdf) and standard nonlinear-audio antialiasing practice | A 2.2 kOhm / 10 nF Shockley-diode node whose capacitor state is trapezoidally integrated and solved with bounded Newton iterations at every oversampled step, surrounded by Electry's eighth-string input and voice filters | A real circuit solve of the documented clipping node; not a full named pedal schematic, component-tolerance study, or SPICE validation |
| Amplifier and speaker/cabinet | [Yeh and Smith's exact passive tone-stack derivation](https://dafx.de/paper-archive/2006/papers/p_001.pdf); [Dempwolf and Zölzer's measured 12AX7 model](https://dafx.de/paper-archive/2011/Papers/76_e.pdf); Reefman's measured [uTracer TubeLib ECC81/ECC83/6L6GC/EL34 fits](https://www.dos4ever.com/uTracer3/TubeLib.inc) and [model derivation](https://www.dos4ever.com/uTracer3/Theory.pdf); [Fender Twin Reverb AB763](https://schematicheaven.net/fenderamps/twin_reverb_ab763_schem.pdf), [Marshall 1959](https://www.prowessamplifiers.com/schematics/Marshall/1959_superlead.pdf) and [Macak/Schimmel](https://www.dafx.de/paper-archive/2010/DAFx10/MacakSchimmel_DAFx10_P12.pdf) phase-splitter and grid-coupling circuits; [RCA 6L6GC](https://frank.pocnet.net/sheets/049/6/6L6GC.pdf) and [Mullard EL34](https://frank.pocnet.net/sheets/129/e/EL34.pdf) push-pull operating points; official [Fender amplifier](https://www.fmicassets.com/Damroot/Original/10001/021730_gamp_manual_all_revE.pdf), [Marshall 1959](https://www.marshall.com/us/en/product/1959-handwired-head?pid=1007086) and [Peavey 6505](https://assets.peavey.com/literature/manuals/00575680.pdf) architecture references; official [Jensen C12K](https://www.jensentone.com/vintage-ceramic/c12k), [Celestion G12M](https://celestion.com/product/g12m-greenback/) and [Vintage 30](https://celestion.com/product/vintage-30/) response boundaries | Three complete paths share a residual-checked measured-current 12AX7 preamp table. American and British add exact unloaded third-order passive RC stacks; nonlinear ECC81/ECC83 long-tailed pairs jointly solve two source-valued output capacitors, 220 kOhm bias returns, factory grid stoppers and TubeLib overload-grid conduction; common/differential-sensitive 6L6GC or EL34 load-line tables retain the resulting blocking shift. The American 6L6GC pair solves individual AB763-derived 470 Ohm screen branches around the RCA operating family; the British EL34 pair solves Mullard's common 800 Ohm resistor from its matching pair condition. These screen equations are coupled to the plate load-line solve and baked into the preparation-time tables. Voiced negative feedback, plate-plus-screen-current-driven sag and independent transformer state follow; Modern preserves the established cascaded path. Six biquads per path supply distinct zero-latency speaker/cabinet response voices, and all nonlinear work stays inside the up-to-4× oversampled block | Exact passive RC transfers, translated measured-current preamp/phase-inverter/power-tube formulae, residual-bounded coupled LTP/grid solves and bounded ideal push-pull load-line/screen-resistor solves inside three deliberately voiced paths. Terminal grid conduction and blocking recovery are modeled while positive-grid plate transfer remains explicitly AB1-bounded. The American screen components are a documented-source hybrid, not a complete Twin DC stage; the upstream 1 kOhm / 20 uF dynamic screen node, Marshall quartet/choke supply, reactive transformer/speaker load, full positive-grid plate data, complete named schematic, component-tolerance or specimen fit, cabinet IR, loudspeaker mechanics, microphone and room capture remain outside the claim |

## Known gaps

### What the automated tests do and do not establish

Current automated tests establish: finite, bounded, non-silent output for
all twenty-one pick-stroke/play-style combinations at 44.1-384 kHz; the 2x/1x
internal-rate policy, exact
host-to-physical clock timing, and filtered-decimation pitch stability;
exact-silence idle output;
sample-identical renders for identical MIDI (including across engine reuse,
which caught a real aperture-state leak during development); fundamental
accuracy within 8 cents across E1..D6 at three rates; stable allpass bounds
and under-20% low/high dispersion-deficit fit error on the heavy short-scale
Drop-E case; positive bounded modal conductance and exact structural-loss
bypass at 0%; independent keyswitch banks, LATCH/HOLD ownership and lifecycle,
same-sample controller/keyswitch conditioning, keyswitch silence, dead-zone
and range gating; the alternate sequence surviving style
changes and skipping hammered notes; measurably distinct attack spectra and
levels for down, up, hammered, muted and harmonic playing, an audibly
composed upstroke palm mute, a stroke-independent harmonic octave, and a
bit-identical hammered note under either latched stroke and across the full
Pick Position/Hardness/Noise endpoints, with no Strum Spread delay or
plectrum-contact phase; a node touch that
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
contraction; the same ±2 semitone wheel ratio on played voices and a ringing
coupled string, with travel following Bend Time and settling exactly on the
target; hammer-on
same-string continuation, pitch settling, and click-free transition;
maximum-velocity attack tuning within 8 cents on every open string, less than
6 cents of cross-string spread, and the same rails on low-string Mute and Dead;
bridge-brighter-than-neck centroid ordering; tone-control
high-band reduction; independently audible internal wood, size, shape,
construction, scale and gauge endpoints plus the exposed body-level, position,
hardness and age endpoints; monotonic
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
CC 2 pressure including hostile input; live-damping phase continuity on both
loop polarisations at 44.1, 48, 96, 192 and 384 kHz for shared Palm/Open
contacts, first-sample full CC2 jumps on E1 and B2, every adjacent step of a
full up/down CC2 sweep at those five rates, and simultaneous CC2 plus the
minimum-time wheel glide at 44.1 kHz;
strum travel offsets in physical string order, an undelayed leading string, a
sample-identical single note at zero or
nonzero spread, reversed upstroke travel, exact render-block partitioning,
later-timestamp stroke separation, a lower stacked chord peak and cancellation
of an un-crossed string without a phantom attack;
the CC1 resonance lifting the sympathetic ring with Resonance Depth scaling
its reach and a bit-exact bypass at a lowered wheel; the closed
engine-amplifier loop self-sustaining after note release at full wheel and
distortion while the same loop decays with the wheel down or the amplifier
dry, all bounded; an exact stereo plug-in render across 64, 256 and 1024-sample
host blocks with non-aligned CC1, Note On and Note Off events, plus an acoustic
impulse returning at the fixed sample-rate-derived delay on 44.1, 48, 96 and
384 kHz hosts; a held A4 loop exceeding the idle high E by at least 6.02 dB
with the voiced quarter return; pluck position following the fretted sounding
length by 2^(fret/12);
fretboard geometry, meter ballistics, standing-wave shape, colour knee and a
lossless packed audio-to-editor round trip; per-string display readout naming
the right string, fret, note and articulation; selector-driven pickup culling,
click-free restoration of a culled pickup, Mono channel linking and a
click-free stereo-field opening; exact digital silence from an untouched
engine, a subnormal-free ring-out that reaches exact zero, and a clean wake
from the frozen state; all six Guitar Build anchors audibly distinct and in
tune; plectrum contact noise in the pre-attack
window; release noise that appears only after note-off; a Dead note that lands
  within 0.47 dB of Sustain, retains the calibrated dark periodic E1 body and
decays through its own loop rather than being gated; eight-string
polyphony with open-position chord mapping, repick reuse, and stealing; a
slide whose pitch travels through the intermediate semitones rather than
jumping, whose travel time scales with the interval, whose chained Slide and
Hammer retargets preserve the programmed and effective loop pitch, direction
and remaining distance, whose in-flight dispersion refits cannot reverse the
physical pitch trajectory, whose friction band follows the speed of the hand,
which is far louder on a wound string than on a plain one and exactly absent at
a silent Finger Noise control, which injects no second modal attack and cannot
cancel or re-anchor a pending picked strum; a fretting hand that keeps a lead
phrase in one position instead of falling back to open strings, leaves the
open-position shapes untouched, retains a planted Palm hand through hammers,
pull-offs and legato slides without changing sibling damping, and relaxes to the
nut when the phrase ends;
pitch-wheel travel, sustain-pedal hold, the balanced A#0 fretting-vibrato
gesture with five-rate coverage for pre-held-silence/open-string onset,
immediate final-key release, sustain-tail exclusion, held-sibling continuity,
off-grid refret, stopped-open-stopped legato, overlapping delayed-note ownership
and finger continuity through live held-note repicks; B0 tremolo
picking with exact free-running cadence, shared chord direction, Strum
deferral, legato retargeting of in-flight contacts, physical-hold filtering and
balanced lifecycle; Hammer-latched E6..B6 and B0 contacts remaining real picks
with Alternate advance, Pick Noise, shared stroke state and Strum travel while
the Hammer latch and live fretting finger remain unchanged;
channel pressure and polyphonic aftertouch remaining pitch-neutral;
hostile parameter and performance
input safety; low Distortion and Modern Amp drive retaining the same physical
input, voice and cabinet response as full drive instead of leaking dry DI
around an enabled module; and a portable CPU ceiling with the eight-string render ratio
printed on every run in worst-case Stereo, maximum Body Resonance, and maximum
Artifacts mode. Mono is checked sample-for-sample dual mono; Stereo tests pin
physical low/high string orientation, coherent fold-down, bounded side level,
energy balance, determinism, and opposite string endpoints. The plug-in suite
additionally pins the 28-parameter layout, including Tremolo Rate's and Amp
Voice's appended indices, Guitar Build's named anchors,
formatted values, current-state round trips, bus layout, sample-accurate
note starts, MIDI controller behavior (uniform pitch bend, unassigned channel
pressure and polyphonic aftertouch, sustain, all-sound-off, all-notes-off), UI
keyswitch triggering of both banks, panic, output-gain and APVTS
output-mode effects, three visible non-overlapping mode buttons, three visible
non-overlapping Amp Voice buttons, Modern default and missing-field migration,
deterministic
distinct Double channels, an unchanged primary lane and clean Double re-entry,
the sympathetic, Mute Pressure (parameter and CC 2) and Strum Spread controls
reaching the rendered audio, offscreen editor rendering including the live
fretboard's bounds, and prepare/release cycles at three rates.
The engine suite separately pins bounded/repeatable second-player pick timing,
one offset per chord, strum composition and pre-contact cancellation.

The amplifier chain has its own suite: the shipping rapid-Mute body's current
30-80 ms medians are 8.4092% upper-band share and 0.895602 harmonicity, above
the 6% body floor while remaining nonperiodic; halfband unity DC gain, the -6 dB
halfband symmetry point, passband ripple and stopband rejection; the diode
node's independent DC points, symmetry and capacitor memory; the measured
12AX7 model's cathode/plate KCL residuals, rail-to-cutoff recovery, plate-load
convergence, quiescent normalisation and asymmetry; a monotonic 16,385-point
runtime circuit table whose maximum checked error is 0.00000072; uTracer
TubeLib 6L6GC/EL34 plate- and screen-current anchors at idle and through the
knee; TubeLib power-grid-diode anchors at +0.5/+1/+2/+5 V, monotonic lookup and
stopper/grid-return KCL; coupled two-capacitor LTP idle, sub-`1e-10 A` musical
residuals, independent common mode, overload conduction and source-ordered
100/22 nF recovery; individual-American and common-British screen-resistor KCL,
screen voltages falling under drive, ideal push-pull load-line symmetry, unity
local gain,
rising current demand, common-bias sensitivity and distinct half-drive
curvature; three-dimensional off-grid lookup errors below 0.0003 for output and
0.0004 for supply demand, with exact zero demand at interpolated idle; a bit-exact dry bypass with every control at zero
and an audible effect from each amount control on its own at 100%; exact
American and British third-order tone-stack coefficients and 1 kHz insertion
loss; the six-point response fingerprint and pairwise spectral separation of
all three amp/speaker paths; model-ordered nonlinear compression and a
same-performance Drop-E level bound; the alias floor of the pedal, every
amplifier, and the two
stacked, at two input levels; direct A/B circuit-state checks that
plate-plus-screen demand rises under a loud hold and recovers during silence,
plus the established full-path supply droop/recovery probe; an output transformer whose distortion falls about
46 dB per decade of frequency and 41 dB for 24 dB of level, measured at the
stage rather than through the cabinet; each Modern cabinet voicing feature
relative to 1 kHz; loudness bounds across every model's whole amp travel and every
combination of the gain and compressor controls on a rendered Drop-E rhythm
figure, dry and palm muted; 48/384 kHz response agreement for every Amp Voice;
stable and mid-switch block-size invariance, bounded switching steps, exact
one-hot settling and inactive-state clearing; the lead delay's first repeat at 360 ms with a
clean gap before it; a decaying, decorrelated room tail; bounded sample steps
across whole-block and individual-module engagement, resting-state resets for
each independently bypassed circuit, and a return to bit-exact bypass;
render determinism; finiteness, output-clamp headroom and
the expected group delay at eight host rates from 22.05 to 384 kHz; and
recovery from NaN, infinite, and out-of-range input as well as null and
zero-length calls, including a single non-finite sample in the middle of an
otherwise clean block. The cabinet's low-frequency probe sits below the modelled
box corner, which is deliberately low enough that a Drop-E eighth string's
fundamental reaches the cabinet rather than being cut before it. A further test renders a short take through the demo
renderer, so the committed demonstration audio's toolchain is covered too.

Twenty-three rendered examples of the whole path are committed under
`Docs/audio/` and produced from this same JUCE-free code by
`Tools/RenderDemos.cpp`, so what the document describes can be listened to
rather than only read. They are demonstrations, not evidence: an audible
example is not a measurement, and none of the claims above rest on them.

### Current Mute/Dead checkpoint

The previous Palm-only finite heel has been retired. User feedback identified its
rendered chugs as squishy and short of body; a same-build waveform, spectrum and
envelope A/B confirmed that the extra 100 ms contact stacked a second transient
loss on top of the already selective steady hand. Removing that layer and moving
Palm's existing modal-excitation factor from 0.74 to 0.85 restores the periodic
low body without adding gain compensation, a parameter or another resonator.
At that Palm-only correction, Open and Dead evaluator WAVs were byte-identical.

The unnormalised dry evaluator measures:

| default probe | retired finite heel | current | change |
| --- | ---: | ---: | ---: |
| E1 30-80 / 0-30 ms body | -5.4608 dB | -4.1939 dB | +1.2669 dB |
| E2 30-80 / 0-30 ms body | -4.8192 dB | -3.2506 dB | +1.5686 dB |
| E1 raw peak | -28.03 dBFS | -25.80 dBFS | +2.23 dB |
| E2 raw peak | -26.70 dBFS | -24.26 dBFS | +2.44 dB |

The noise-free engine guard independently reads -4.065/-3.176 dB of E1/E2
body. It also keeps the intended selective loss: over 150-500 ms the Palm-minus-
Open low/high changes are -10.212/-19.709 dB on E1 and -8.187/-22.975 dB on
E2. The early Palm high/low balance remains 4.558/0.394 dB darker than Open.
Light, default and hard body remain strictly ordered, so Mute Tightness is still
one understandable loose-to-tight performance axis rather than three samples.

There is one explicit tradeoff. The tracked Guitar-TECHS F2 development proxy
moves from the retired contact's -4.009 dB contraction to -0.680 dB at 44.1 kHz
(-0.680..-0.692 dB across 44.1-192 kHz), farther from the two public player cells
at -6.099/-15.290 dB. Those two conventional-guitar cells establish the
direction of selective loss, not a defensible coefficient. The old local fit was
therefore demoted from a hard target after it conflicted with low-body evidence
and the rendered result; the commissioned E1/E2 TRAIN/HOLDOUT comparison remains
the actual fit gate.

The common high-gain rapid-Mute fixture now has 7.6947% of its 30-80 ms power
above 500 Hz and 0.889306 harmonicity, passing its intentionally loose body and
nonperiodicity rails. The shared-hand transition remains smooth: Palm -> Open
moves the old E1 by +9.9020 dB above 500 Hz and +0.271476 percentage points;
Open -> Palm moves it by -7.2759 dB and -0.150555 points. Adjacent CC2 attack
moves are 0.0362/0.0463 dB on E1/E2. The stateful Dead medians shift only to
-8.126/-14.966/-23.092 dB with a 4.83093 dB contextual RMSE because the earlier
Palm ring and its live damping phase are different; no Dead coefficient changed.

All demonstrations affected by the correction were regenerated from the same
code. The current SHA-256 hashes are
`9d2a5ac0060834b35d117d0b11036b54f11c34cc834753a5e9c194aaf46a8e6a`
for demo 04 and
`a5c34efa6bc3be3a81fe409b96d07ce18c89d83681542ee2249f1b73a7f09b63`
for demo 16. These demos remain demonstrations, not validation recordings.

### Remaining realism gates

Engineering tests and public datasets do not replace licensed Drop-E hardware
validation. No public source found closes the licensed, controlled, genuinely
dry E1 and open-string E2 Open/Mute/Dead gate. The best lawful sources each
cover only part of it: [inspektral's CC0 seven-string baritone matrix](https://freesound.org/people/inspektral/packs/42559/)
matches muted and sustained low notes but does not identify the muting hand,
raw-DI chain or exact tuning; this pass could access only its public previews.
[HiMMP](https://himmp.net/faq.html) supplies CC-BY score-matched raw Palm DIs,
but from a conventional six-string Drop-C guitar.
[Freesound 557299](https://freesound.org/people/minus_28_and_falling/sounds/557299/)
is a real Drop-E phrase but its hand labels, chain and public-preview codec are
not controlled, while [cabled_mess's eight-string pack](https://freesound.org/people/cabled_mess/packs/29585/)
documents clean dry chromatic one-shots on its lowest F# string, but only F#1
is open and there are no mutes or Dead.
[8ridgelite](https://github.com/JamesStubbsEng/8ridgelite) adds GPL-3.0,
downloadable exact-eight open-note sustains from E1 upward. Its two low files
show a much faster attack and broadly similar first-two-second decay, but the
guitar, tuning, string/fret, channel identities and recording chain are
undocumented and there is only one stroke per pitch; it is a direction check,
not a fit set. The
[cabled_mess profile](https://freesound.org/people/cabled_mess/) offers custom
recordings and is the best first commission lead; availability, Drop-E setup,
protocol compliance and commercial calibration rights still require a written
agreement. The documented
[RG8 corpus repository](https://github.com/aomartinezg/music-sheet-generator)
has neither committed raw recordings nor a licence, so it supplies no lawful
calibration audio. The
commissioned [`electry-mute-capture/v1` preflight](Docs/capture/electry-mute-capture-v1/README.md)
therefore requires 16 files per session: ten isolated E1/E2 Open, three Palm
positions and Dead probes; four rapid Palm/Dead runs; one mixed Dead E1/E2
groove; and one Palm-E1/Open-E2 lift/replant groove, at least 429,975 frames.
Commercial calibration/private-evaluation rights, stable player and guitar IDs,
and train/holdout separation are mandatory; the final engineering gate needs
at least three train clusters and exactly two untouched active holdout clusters.

The frozen perceptual gate scores ten real/model cells plus three hidden repeats
with exactly 30 listeners: 15 extended-range guitarists and 15 metal producers.
A separate licensed-leader session compares Electric Storm Deluxe, Shreddage
3.5 Hydra and Uproar RAW. Real parity is not a market win: “best sounding”
requires predeclared superiority. Until those commissioned captures and the
frozen blind comparison pass, Electry claims a research-grounded,
regression-measured model—not capture parity or market leadership.

## Development checkpoints

### 2026-08-27 source screen-grid loading

- Added the source-derived screen networks inside the American and British
  power-pair solves. American retains the RCA 6L6GC 450 V plate / 400 V screen /
  −37 V / 5.6 kOhm AB1 pair and gives each tube an AB763-derived 470 Ohm screen
  branch. That combination is explicitly a source-component hybrid rather than
  a claim to reproduce the complete Twin Reverb DC output stage.
- British keeps Mullard's 400 V / −36 V / 3.5 kOhm EL34 pair condition and its
  matching common 800 Ohm screen resistor. Both tube screen currents therefore
  pull one shared node instead of borrowing the Marshall quartet's four
  individual branches.
- The screen equations and plate load line are solved together. Their implicit
  current slopes participate in the load-line Newton step, and the resulting
  nonlinear transfer and supply demand are baked into the existing
  preparation-time tables; the audio thread gains no iterative circuit solve.
  Regression coverage checks the individual/common resistor KCL, shared-node
  identity, screen-voltage compression under drive and table agreement.
- Modern remains operation-for-operation unchanged. The American upstream
  1 kOhm / 20 uF dynamic screen node, Marshall quartet/choke supply, reactive
  loudspeaker-reflected load and full positive-grid plate-current surface remain
  explicit next circuit stages.

### 2026-08-27 output-grid blocking and recovery

*Historical metrics in this checkpoint predate the source screen-grid loading
above; its two-grid conduction, coupling and recovery topology remains.*

- Replaced the static midband handoff between each American/British phase
  splitter and power pair with the two physical branches. American uses 100 nF
  output capacitors, 220 kOhm bias returns and 1.5 kOhm grid stoppers; British
  uses 22 nF, 220 kOhm and 5.6 kOhm. TubeLib's common 2 kOhm plus `IS=1 nA`,
  `RS=1 Ohm` diode branch supplies bounded terminal-grid conduction while the
  fitted power-tube plate-current surface remains clamped at its measured
  `Vg<=0` boundary.
- Every oversampled frame jointly solves LTP current and both terminal grids
  against the two trapezoidal capacitor histories. Grid current therefore loads
  the correct unequal PI plate and leaves common negative bias memory instead
  of disappearing in a scalar differential projection. The pinned overload
  reaches 0.787/0.801 mA American/British grid current with maximum KCL residuals
  below `1e-10 A`; after 1/20 ms the shared shifts are −14.84/−7.98 V and
  −18.98/−1.14 V, preserving the slower 100 nF American recovery.
- The power pair now accepts common and differential grid drive. Its 257 × 257
  quadratic terminal-grid plane is packed by tube-swap symmetry over 13 rail
  levels (6.576 MiB for both models); trilinear interpolation stays below
  0.0003 normalized output and 0.0004 demand error and subtracts its own idle
  interpolation error before sag. The validated musical domain has headroom to
  common `[-2, 0.5]`, differential `±4` and normalized terminal grid `[-7, 0]`.
- American/British/Modern alias floors are −62.22/−65.06/−61.28 dB,
  loud-versus-quiet compression is −13.70/−15.73/−18.33 dB and the same 90%
  Drop-E fixture measures +2.86/+6.34/+5.87 dB. Modern remains
  operation-for-operation unchanged. Demo 23 alone adds near-legato overloaded
  chord turns so the different blocking recoveries are audible under one global
  normalisation; demos 01–22 retain the established Modern path.

### 2026-08-27 measured ECC81 and ECC83 phase splitters

*Historical checkpoint: its static midband output loads and scalar differential
handoff are superseded by the dynamic two-grid checkpoint above; the measured
ECC81/ECC83 models and input networks remain.*

- Replaced the American/British ideal opposed-grid driver with loaded nonlinear
  long-tailed pairs. American uses TubeLib's measured-fitted ECC81/12AT7 family
  at the Twin Reverb AB763-family 410 V, 82/100 kOhm, 470 Ohm and 22 kOhm-tail
  operating circuit; British uses its separate ECC83/12AX7 fit at the
  literature-modelled 400 V with
  the 82/100 kOhm, 470 Ohm and 10 kOhm/presence-tail Super Lead family. The
  input poles are the documented 1 nF × 1 MOhm and 22 nF × 1 MOhm networks.
- Each table point solves both plate KCL equations against the finite tail and
  midband 220 kOhm power-grid-return loads. The 769-point runtime table stays
  within the checked direct-solve error, while DC anchors, loaded KCL, unity
  local slope, asymmetric ±1 outputs, common-mode rejection and total-current
  conservation are independently pinned. Letting both grids incorrectly follow
  the live tail fails those bounds.
- The source-derived American input capacitor now tightens 90/120 Hz by
  6.80/4.40 dB relative to 1 kHz. Post-circuit trim retains the previous 90%
  Drop-E level instead of undoing that frequency shape. American/British/Modern
  alias floors are −63.27/−66.27/−61.28 dB, compression is
  −13.18/−15.43/−18.33 dB and the same riff gains are
  +2.88/+6.49/+5.87 dB.
- Modern remains operation-for-operation unchanged, so demos 01–22 retain
  their committed samples and only the globally normalised demo 23 comparison
  changes. This strict-AB1 checkpoint projects the LTP differential output into
  the existing pair table; its sub-4% boundary is regression-pinned. Separate
  output-cap/grid voltages and TubeLib's generic grid-current branch belong in
  the next commit so blocking memory is added as one coherent circuit.

### 2026-08-27 measured 6L6GC and EL34 output stages

*Historical checkpoint: the ideal phase splitter described here is superseded
by the measured ECC81/ECC83 checkpoint above; the output-tube solve remains.*

- Replaced only the American/British opposed-12AX7 output approximation with
  distinct measured-curve-fitted uTracer TubeLib families: 6L6GC beam tetrodes
  on the documented 450 V / 400 V / −37 V / 5.6 kOhm AB1 pair and EL34 true
  pentodes on Mullard's 400 V / −36 V / 3.5 kOhm fixed-bias pair. An ideal
  balanced phase splitter and Raa/4 centre-tapped load make the remaining
  topology boundary explicit.
- A preparation-time 4,097 × 17 table per family solves output and
  plate-plus-screen demand over grid drive and simultaneous plate/screen rail
  loss. Runtime bilinear interpolation stays below 0.000086 normalized output
  error and 0.000093 demand error. Its normalized drive stops at the zero-grid
  AB1 boundary (±1), rather than pretending an ideal voltage source can drive
  positive grids; supply sag now changes the rails inside that solve and
  follows tube current rather than rectified output.
- The 6L6GC/EL34 equal-slope half-drive outputs are 0.48185/0.643711. Full-path
  American/British/Modern alias floors are −62.17/−68.17/−61.28 dB,
  loud-versus-quiet compression is −13.87/−15.43/−18.33 dB and the same 90%
  Drop-E fixture measures +2.88/+6.57/+5.87 dB. Every model's full Amp travel
  remains inside the playable level rail.
- Modern remains operation-for-operation unchanged, so demos 01–22 retain
  their committed samples. Demo 23 is regenerated from the same globally
  normalised three-instance comparison to expose the new A/B output dynamics.
  Grid-current/coupling-cap memory and nonlinear phase splitting were explicit
  next stages at this historical point; source screen-grid loading now
  supersedes its fixed-screen boundary, while a reactive speaker-reflected load
  remains open.

### 2026-08-27 three complete amplifier voices

This historical checkpoint's opposed-transfer output approximation is
superseded by the measured 6L6GC/EL34 checkpoint above; the three-path surface,
tone stacks, feedback, transformer, cabinets and untouched Modern branch remain.

- Added American Clean, British Crunch and Modern High-Gain as complete
  selectable amplifier, output-dynamics, transformer and speaker/cabinet paths;
  switching crossfades independent recursive state with a 15 ms exponential
  time constant and returns to one running circuit after the weights settle.
- American and British use exact third-order Yeh/Smith passive tone-stack
  transfers, the existing measured-12AX7 circuit table, opposed-transfer output
  approximations, causal output-derived negative feedback, independent sag and
  transformer state, and distinct six-section parametric cabinet voices. The
  exact pieces and voiced approximations are identified separately; these are
  not named-amp, power-tube, cabinet-IR or microphone replicas.
- The established Modern path remains coefficient- and sample-identical. On the
  same 90% Amp Drop-E fixture, model gain spans +2.98 to +5.87 dB; pinned loud
  versus quiet compression is −5.59/−10.80/−18.33 dB, alias floors are
  −73.38/−77.14/−61.28 dB, and 48-to-384 kHz response drift stays below 0.05 dB.
- Demo 23 renders the same deterministic power-chord, chug, chord and lead
  phrase through fresh American, British and Modern instances, with one global
  normalisation and no per-model loudness hiding.

### 2026-08-27 series-connected gain circuits

- Distortion and Amp used their drive value as a permanent parallel dry/wet
  mix. Demo 20's 22% clean amp therefore bypassed its own loudspeaker with 78%
  uncabbed DI, and the 95% metal rig still leaked 5% around the cabinet.
- Zero remains the exact, cost-free bypass. At any nonzero steady setting the
  whole pedal or amplifier is now in series; the knob changes drive, and the
  existing 15 ms exponential smoothing time constant acts only as a click-free
  relay when crossing zero.
- At the UI's minimum 0.1% drive, the then-sole amp path—now Modern
  High-Gain—keeps its 8 kHz response 22.003 dB below 1 kHz. The pedal remains
  18.182 dB down at 40 Hz and 20.961 dB down at 12 kHz; each response stays
  within 0.5 dB of its full-drive circuit response. Exact bypass, aliasing,
  level, lifecycle, transition and sample-rate rails still pass.

### 2026-08-27 Hammer-latched dedicated repicks

- E6..B6 and B0 are picking-hand commands, but a latched Hammer turned them
  into finger taps: plectrum contact, contact loss, Pick Noise, Alternate
  advance and Strum travel were all bypassed. Manual and automatic note order
  advanced while both contacts stayed Down; Pick Noise 0/1 was byte-identical,
  and a two-string 3 ms B0 stroke produced delays of 0/0.
- Dedicated repicks now interpret Hammer as a neutral Sustain pick contact only
  for that attack. The global Hammer latch remains ready for later playable
  notes; Slide and every already picked style are unchanged. No state or control
  was added.
- Regression rails pin alternating manual contacts, audible Pick Noise, B0's
  next-sample contact and one shared two-string stroke with distinct nonzero
  travel delays, while preserving the live fretting finger and Hammer latch.
  Demo 22 picks MIDI 74, genuinely hammers to 76, leaves Hammer latched through
  B0's picked lead and restores Sustain afterward; its duration is unchanged.

### 2026-08-27 finger-owned vibrato lifecycle

- A#0's shared onset previously aged without a fretting finger. After 500 ms of
  pre-hold, the first stopped note began about 10.04 cents high and reached
  38.04 cents at 60 ms; a coincident gesture and note began at 0 and reached
  only 4.75 cents at 60 ms.
- Key release left the last rocking offset on a tail: an ordinary release
  reached 38.60 cents within 60 ms, a sustain-held tail reached 50.97 cents
  within 500 ms and still held 35.24 cents, and a same-event A#0 plus fretting-
  key release reached 37.19 cents within 60 ms. An off-grid final Note Off could
  retain its preceding pitch offset for up to seven host frames at 48 kHz.
- The shared onset now advances only while a physically held stopped finger
  exists. Open strings and released or sustain-held tails remain fixed; a held
  stopped sibling keeps the hand moving, while final-key release immediately
  clears the onset and voice offsets but preserves A#0 intent. Fresh/off-grid
  refrets and stopped-open-stopped legato start from rest; overlapping ownership
  of the same held or delayed note preserves the onset already under way.
- Regression coverage spans 44.1, 48, 96, 192 and 384 kHz for pre-held silence,
  an open string and the first stopped-finger tick, then pins sustain-held and
  sibling release, immediate off-grid refret, stopped-open-stopped legato and a
  delayed same-note overlap. Demo 22 now pre-holds A#0 through a 240 ms rest so
  the next stopped note exposes the fresh bloom.

### 2026-08-27 live-damping pitch correction

- Changing Mute Pressure or moving the shared bridge hand refitted damping
  filters on already-ringing strings without translating the delay coordinate
  for their new phase. A shared E1 Sustain/Palm contact could move the vertical
  loop by 22.2 cents and the horizontal loop by 11.9 cents; a direct full CC2
  state change could exceed a semitone. The resulting chirp was a physical and
  playability defect, not a voicing choice.
- The engine now preserves the prior complete-loop period when damping alone is
  refitted: it evaluates the old-to-new damping-phase difference at that period
  and applies the opposite raw-delay translation immediately. Genuine pitch
  requests from bend, vibrato and refretting remain on their existing smoother,
  and dispersion-phase translation remains a separate additive correction.
- Regression coverage measures both polarisations on shared Palm/Open contacts,
  first-sample full CC2 jumps on E1 and B2, all 254 boundaries of an adjacent
  CC2 up/down sweep at 44.1, 48, 96, 192 and 384 kHz, and CC2 during the minimum
  40 ms wheel glide at 44.1 kHz. Contact and sweep steps remain below 0.25 cent;
  the combined bend comparison remains below 0.5 cent.
- The canonical Linux renderer changed demos 02–05, 08–20 and 22 while demos
  01, 06, 07 and 21 remained byte-identical. The correction is easiest to hear
  dry at demo 04's Palm lift/replant and throughout demo 15; demos 18 and 20
  expose it in high-gain musical contexts. No A–Z voicing selection was needed
  because the complete-loop pitch invariant settled the correction.

### 2026-08-27 fixed-delay feedback correction

- Replaced host-block-sized acoustic latency with a sample-rate-derived
  5.805 ms FIFO: 256 samples at 44.1 kHz, a voiced nominal distance of roughly
  two metres. The plug-in now renders, amplifies and returns causal chunks no
  longer than that delay, including at MIDI event boundaries.
- Added a bit-exact stereo plug-in rail for 64, 256 and 1024-sample host blocks,
  with CC1 and Note On at absolute sample 137 and Note Off at sample 4099. Added
  impulse-timing and constant-FIFO-depth rails at 44.1, 48, 96 and 384 kHz.
- Before the correction, identical macOS renders selected 2.667, 5.465 and
  2.217 kHz feedback modes in demo 06 at 64, 256 and 1024 samples; demo 14's
  tail selected 0.989, 2.717 and 2.744 kHz. The corrected renderer is
  byte-identical across all three partitions. Demos 06 and 14 were regenerated;
  the other twenty same-platform WAVs remain byte-identical.

### 2026-08-27 feedback-focus correction

- Reduced only the direct loudspeaker return into idle sympathetic loops to a
  voiced one-quarter share; played/releasing voices keep the full return and
  bridge-driven sympathetic coupling is unchanged.
- Added a closed engine/amplifier rail that keeps held A4's loop at least 6.02
  dB above the idle high E and distinguishes the quarter share from a
  still-hijacked one-half share. Equal and one-half direct drive lose by 16.2
  and 11.9 dB in the selection fixture; the current fixed-delay rail leaves
  one-quarter winning by 19.1 dB.
- Corrected demo 14's stale comments: the standard MIDI pitch wheel is uniform,
  not a per-string physical whammy-bar model. Demos 06 and 14 were regenerated
  without changing their scores or durations.

### 2026-08-26 Palm-body correction

- Removed the provisional 4 mm/100 ms Palm heel after user feedback and
  controlled A/B measurements showed that it stacked an over-damped transient
  on the steady bridge-hand model and erased the 30-80 ms low body.
- Raised only the existing Palm modal-excitation factor from 0.74 to 0.85.
  Default E1/E2 body improves by 1.27/1.57 dB, Open and Dead evaluator renders
  remain byte-identical, and no gain compensation, control or dependency was
  added.
- Replaced the sparse two-cell F2 fit rail with direction, rate-invariance,
  long-band selective-loss and explicit E1/E2 body guards. The exact-eight
  commissioned TRAIN/HOLDOUT gate remains open.
- Regenerated the thirteen Palm-bearing demos and all ten evaluator probes;
  the other nine demos and four Open/Dead probe WAVs remained byte-identical.

### 2026-08-26 performance-detail checkpoint

- Removed Strum Spread's hidden 20 ms cost from normal host and on-screen
  playing. Exact-sample Note On groups now pre-anchor the known physical edge,
  begin their leading string immediately and retain only the requested
  inter-string travel; a complete one-note group is sample-identical at zero
  and nonzero spread. The causal pre-roll remains only for scalar engine clients
  that deliver an incomplete chord over successive calls.
- Made each later MIDI timestamp a new performed wrist stroke. Complete-batch
  travel is permutation- and block-partition invariant, reverses with Up,
  advances Alternate once, and cancels an un-crossed released string without a
  late attack. The deterministic demo renderer now uses that same complete
  batching contract.
- Added a 23-second all-technique solo and four original single-guitar genre
  studies covering syncopated djent, modern metalcore, odd-meter progressive
  metal and dynamic blues rock. Their scores and effect moves are deterministic
  renderer code, not copied audio or opaque backing tracks.
- Made the existing held-string repick lane visible on the live fretboard:
  clicking a held row produces one hard attack through the same bounded UI
  queue and physical repick path as MIDI E6..B6. Empty rows remain silent and
  external MIDI keeps velocity control.
- Added the adjacent momentary B0 **TRM** wrist and an append-only 4-20
  strokes/s Tremolo Rate. One shared phase repicks every physically held string
  through that same path, preserves chord-wide Alternate direction, and skips
  contacts that would overwrite an in-flight Strum traversal. A same-string
  Hammer or Slide now moves an already-reserved contact to its latest fret
  without cancelling the travelling pick. Demo 22 exposes 8, 12 and 16
  strokes/s plus moving-note, vibrato and in-flight chord-slide use.
- Made the existing plectrum predicate the complete hand boundary: a fresh
  Hammer/tap now bypasses Strum Spread, Double's wrist offset, plectrum contact
  loss and all Pick controls; a legato Slide preserves the ringing loop and
  contributes only its moving finger friction, without re-anchoring a pending
  picked chord.
- Made descending Hammer-style legato a direction-aware pull-off: it releases
  at the old fret's physical position in the lateral plane and can continue to
  an open string, while ascending hammer-ons retain their established path.
- Kept the picking hand planted through non-plectrum legato. Hammer-ons,
  pull-offs and ringing-string Slides retain existing Palm damping on their
  target and leave sibling loops untouched; a later real pick still moves the
  shared hand. Demo 15 exposes the retained heel in a dry octave hammer/pull.
- Continued chained legato from the fret the finger has actually reached. A
  mid-glide Slide or Hammer now preserves its instantaneous pitch, derives
  direction and remaining travel from the fractional live fret, translates raw
  delay after the final retained-Palm damping solve and across dispersion phase
  changes, and never visits the abandoned destination. The same phase-coordinate
  correction keeps wheel motion monotone across quantised dispersion refits.
  Demos 02-06, 09, 11-15 and 17-22 were regenerated; demo 17 exposes the
  redirected descent.
- Gave Double's seeded second engine one deterministic 0-6 ms contact offset
  per picked wrist stroke. The primary engine stays sample-exact, chords share
  one offset plus strum travel, and no additional control was added.
- Audited a CC0 SM57/V30 cabinet IR. A magnitude-only refit improved isolated
  curve error but broke complete-chain level and muted-body rails, so the
  zero-latency cabinet stayed unchanged pending a partitioned-convolution and
  listening comparison.
- Deferred a stronger unilateral repick contact: its passive topology is
  available, but public repetition audio cannot set its physical constants.

### 2026-08-26 MIDI tuning checkpoint

- Made the standard MIDI pitch wheel move every played and sympathetically
  ringing string uniformly over ±2 semitones and the existing Bend Time.
- Left channel pressure and polyphonic aftertouch unassigned so controller
  pressure cannot silently sharpen fretted notes.
- Added the visible momentary A#0 **VIB** key without restoring either pressure
  mapping; velocity controls width and open strings stay fixed.
- Ended the on-screen piano at the playable D6. MIDI E6..B6 per-string repick
  triggers remain available to hosts and external controllers without being
  drawn as pitched keys.

### 2026-08-26 licensed low-register mute checkpoint (superseded)

- Added reproducible direction-only comparisons against a CC0 seven-string
  baritone muted/sustained matrix and HiMMP's CC-BY score-matched rhythm DIs.
- Deepened the provisional finite contact from 1.10 to 1.25 times the solved
  bridge-hand depth and extended its hold from 70 to 100 ms. A 1.40 mapping
  was already saturated; 120 and 140 ms holds failed the retained E2
  soft-to-hard expression rail.
- Kept the 4 mm fixed footprint and rejected per-hit heel jitter: the public
  sources do not identify enough contact geometry to justify another random
  player dimension.
- Preserved the all-string tuning rails and the rapid-repick grid; the full
  100 ms hold plus 10 ms release renders eight Palm voices at 0.205x realtime
  at 96 kHz on the checkpoint machine.

This finite-contact experiment is retained as development history; the newer
Palm-body correction above removes it.

### 2026-08-26 tuning and picking-hand checkpoint (Palm portion superseded)

- Corrected hard-pick pitch across all eight strings; the default isolated
  open strings now measure within 0..2.5 cents after attack, while the low
  chord spans -4.25..+0.75 cents.
- Replaced arrival-order chord fingering with one bounded eight-string match
  at each exact MIDI sample. Host, on-screen and mixed-source permutations now
  retain the same playable four-fret shape and bit-identical player variation;
  releases, controllers and repick gestures remain ordered boundaries.
- Made live CC2 bridge-hand pressure visible in the engine status, independent
  of the host's Mute Pressure knob.
- Stabilized a provisional Palm-only finite heel: Mute Tightness moves its
  4 mm footprint centre from 4 to 20 mm at the saddle, and a passive symmetric
  six-cubic-read contact initially held 70 ms and released over 10 ms; the
  licensed low-register checkpoint above extends that hold. The 0.74 attack
  darkener and Dead path remain unchanged.
- Added the E6..B6 per-string repick range. Its picking hand can restart a
  fully decayed held Mute or Dead string without adding note ownership, and it
  follows current velocity, Pick Stroke, Play Style, CC2 and Double state.

### 2026-08-25 realism and performance checkpoint

- Corrected Dead from an over-choked 30 ms click to an independent 1.6 s
  fretting-hand contact with a short, dark periodic E1 body; Mute Pressure can
  stack while Mute Tightness remains specific to the Mute articulation.
- Made Palm contact selective, force-aware and event-accurate; CC2 is smooth,
  delayed attacks cannot act before contact, and the newest actual Palm/Open/
  Dead contact now re-solves older ringing loops without rewriting their attack
  style.
- Added `PLAY-STYLE KEYS: LATCH | HOLD`: latching remains the default,
  while HOLD makes D#0..A0 momentary over a saved base style for playable chug,
  open-accent and ghost-note phrases.
- Replaced six exposed construction axes with one Guitar Build trajectory
  through six generic anchors; the fitted Drop-E build is the 0.8 default.
- Made Output Mode one Mono/Stereo/Double host choice; Double runs two
  differently seeded deterministic mono engines before one shared FX chain.
- Replaced the pedal and amplifier memoryless clipping curves with the
  circuit-solved antiparallel-diode RC node and measured-12AX7 plate-load
  stages, retaining oversampling, sag, transformer and cabinet voicing.
- Removed the redundant OUTPUT FIELD label, widened the three output buttons,
  renamed the bridge-hand UI to Mute, and gave every section heading the same
  slightly larger, brighter treatment.
- Added two focused Palm/Dead demos, a deterministic ten-probe evaluator, the
  licensed capture intake/collection validators and a strict, runnable blinded
  packer/scorer. The terminal capture and market-comparison gates remain open.

### Prototype baseline

Initial implementation in one JUCE codebase producing VST3, Audio Unit and
Standalone:

- Eight Drop-E string voices as dual-polarisation single-delay-loop waveguides
  with third-order Lagrange fractional reads, per-note stiffness dispersion
  fitted at two partials, decay-targeted loop filters solved by bisection, and
  a contractive shared-bridge coupling matrix.
- A fretting hand with a floating position and a four-fret reach; a picking
  hand with per-stroke contact variation and constant-velocity strum travel;
  two independent latching keyswitch banks, so any of three pick strokes can
  drive any of seven play styles.
- Point-touch harmonics as an exact first-order contact loss inside the loop —
  natural, pinch and touch — plus slides, hammer-ons and pull-offs, dead notes,
  palm muting as an additive absorber, and incidental fret collisions.
- A uniform ±2 semitone MIDI pitch wheel over the Bend Time glide, and a
  resonance wheel that drives a bounded, saturating amplifier-feedback path
  through a fixed nominal 5.805 ms acoustic return, independent of host block
  size; pressure messages are unassigned.
- A published pickup signal structure: per-string position comb following each
  fret, an O(1) fractional moving average for the finite aperture's exact sinc
  response, bounded flux nonlinearity differentiated into induced EMF, and a
  loaded coil/tone circuit.
- A four-mode solid-body structural path, geometry-informed and passive, with
  a six-anchor Guitar Build macro and independent pickup/tone/body-colour
  controls.
- An FX chain — distortion; selectable American, British and Modern amplifier,
  sag, transformer and parametric speaker/cabinet paths; compression; lead
  delay; and stereo room — with the circuit-derived nonlinear stages inside an
  up-to-4× oversampled domain, and every amount defaulting to a true 0 % dry
  setting.
- Mono as the summed dry DI, Stereo as a phase-coherent divided-pickup view
  that folds coherently back to mono, and Double as two independent engines.
- Audible-work culling: a faded-out pickup is skipped entirely, Mono runs one
  shared coil chain and mirrors it, and the whole engine freezes to exact zero
  once nothing vibrates.

## Build

The JUCE-free DSP core, tests and demo renderer:

```bash
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DELECTRY_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp --output-on-failure
./build-dsp/ElectryRenderDemos Docs/audio
./build-dsp/ElectryRenderEvaluation build-dsp/evaluation
```

Read the [evaluation contract](Docs/evaluation.md) before comparing levels or
publishing scores. Validate one commissioned session or a complete split-safe
collection with:

```bash
cmake -DELECTRY_MUTE_CAPTURE_DIR=/absolute/session/path \
  -P cmake/ValidateMuteCapture.cmake
cmake -DELECTRY_MUTE_CAPTURE_COLLECTION_DIR=/absolute/collection/root \
  -P cmake/ValidateMuteCaptureCollection.cmake
```

The full plug-in (JUCE 8.0.14 is fetched pinned at configure time, or pass
`-DELECTRY_JUCE_PATH=/path/to/JUCE`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On macOS, `./scripts/build-macos.sh` drives the same build through Xcode as a
universal VST3, Audio Unit and Standalone app and renders the committed editor
screenshot while the suite runs. After installing the component, validate it
with `auval -v aumu Elc1 Eltr`. `./scripts/sign-and-package-macos.sh` stages,
signs and packages all three products; provide the signing and notarization
environment described by that script for distribution.

## Licensing

Original code under the MIT license (`LICENSE`). JUCE is used under its own
terms — see `THIRD_PARTY_NOTICES.md`. No samples, impulse responses or
third-party presets are included.
