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
hand never puts the plectrum down in exactly the same place twice. Without an
MPE zone, the standard pitch wheel moves every string together over a uniform
±2 semitone range. Once a valid MIDI MPE zone and its pitch ranges are
declared, each member channel owns a physical string and bends only that
performed note while the zone master adds a chord-wide interval. A resonance
wheel can push a distorted tone into self-sustaining amplifier
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
Electry's established metal circuit topology and voicing.
The nonlinear blocks run inside an up-to-8× oversampled domain, so a high-gain
tone saturates instead of folding its own harmonics back into the guitar band.
These are meticulously bounded circuit-derived families, not capture-accurate
replicas of named amplifiers, individual power-tube specimens, microphones or
rooms. Mono is the
authentic summed dry DI; Stereo is one engine's
phase-coherent divided-pickup view of the eight physical strings; Double is two
separately seeded deterministic Electry performances, one per channel, with
independent contact detail and a bounded 0-6 ms second-player pick clock.
Neither stereo choice is a delayed copy or synthetic widening effect.
The shipping Guitar Build control moves the supported walnut-to-ash material
coordinate through three paired pickup-observed modes while visiting six
distinct 25.5-to-28-inch light/heavy string setups. Unsupported size, shape and
joint coordinates remain neutral; the scale/gauge path is explicit voicing, not
a claim that material determines either one. The former four-mode construction
path remains available only as an explicit legacy comparator.
Pickup construction, selector, tone, body-colour amount and all player controls
remain independent.

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
| `01-range-open-strings.wav` | −12.6 dBFS | +9.6 dB |
| `02-range-full-fretboard.wav` | −14.7 dBFS | +11.7 dB |
| `03-play-styles.wav` | −3.8 dBFS | +0.8 dB |
| `04-drop-e-rhythm-dry.wav` | −13.2 dBFS | +10.2 dB |
| `05-drop-e-rhythm-amp.wav` | −12.6 dBFS | +9.6 dB |
| `06-lead-amp-delay-room.wav` | −11.8 dBFS | +8.8 dB |
| `07-pickups-and-tone.wav` | −12.5 dBFS | +9.5 dB |
| `08-sympathetic-strum-stereo.wav` | −16.5 dBFS | +13.5 dB |
| `09-guitar-build-contrasts.wav` | −8.9 dBFS | +5.9 dB |
| `10-velocity-dynamics.wav` | −12.1 dBFS | +9.1 dB |
| `11-power-chords-dry.wav` | −8.6 dBFS | +5.6 dB |
| `12-power-chords-amp.wav` | −12.4 dBFS | +9.4 dB |
| `13-long-rhythm-arrangement.wav` | −13.0 dBFS | +10.0 dB |
| `14-whammy-and-feedback.wav` | −13.2 dBFS | +10.2 dB |
| `15-mute-and-dead-audition.wav` | −12.9 dBFS | +9.9 dB |
| `16-mute-and-dead-metal.wav` | −13.5 dBFS | +10.5 dB |
| `17-extended-technique-solo.wav` | −12.4 dBFS | +9.4 dB |
| `18-syncopated-djent-study.wav` | −12.3 dBFS | +9.3 dB |
| `19-modern-metalcore-study.wav` | −12.5 dBFS | +9.5 dB |
| `20-odd-meter-prog-study.wav` | +1.0 dBFS | −4.0 dB |
| `21-blues-rock-lead-study.wav` | −9.8 dBFS | +6.8 dB |
| `22-tremolo-picking-study.wav` | −13.7 dBFS | +10.7 dB |
| `23-amp-voices.wav` | −15.4 dBFS | +12.4 dB |
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
continue to use that same Modern topology and voicing.

Files 18–22 are original Electry studies, not endorsements or artist
sound-alikes.
The finger-vibrato moments in files 06, 17, 21 and 22 use the same model available
from the visible A#0 **VIB** key. Hold it before or while a physically held
stopped note rings; pre-held intent waits for the finger to land, and velocity
chooses width. Channel and polyphonic pressure remain unassigned, so normal
aftertouch cannot bend a chord by surprise. MPE currently maps its pitch
dimension only; per-note pressure, CC 74 timbre and Note Off velocity await a
capture- and listening-calibrated physical mapping.

## Factory rigs and quick start

The editor's **RIG** selector provides four deterministic starting points and
sets all 28 host parameters, so it cannot inherit a forgotten control from the
previous patch. Rigs deliberately leave Pick Stroke, the base Play Style and
the `LATCH | HOLD` choice alone.
Compact knob captions remain short enough for the faceplate, while each knob's
accessibility title uses its complete host parameter name (for example,
**Mute pressure**, **Compressor** and **Output level**).

- **Factory Default:** the dry Drop-E guitar documented in the parameter table.
- **Drop-E Metal:** the demonstrated 45% distortion / 95% Modern High-Gain amp /
  60% compressor chain and tight rhythm setup used by the matched Mute/Dead
  demo.
- **Mute / Dead DI:** dry, +6 dB, hard near-bridge pick, no sympathetic ring
  and middle Mute Tightness, exposing the two hand contacts without an amp.
- **Blues Rock Lead:** the host-loadable version of the warm neck-pickup Stereo
  voicing behind `21-blues-rock-lead-study.wav`, with responsive 150 ms bends,
  moderate Modern amp drive, compression, delay and room. Its +3.2 dB output is
  the host control's nearest 0.1 dB step to the demo renderer's 1.45 gain.

1. Load **Drop-E Metal**, leave the built-in amp on, and play E1..D6
   (MIDI notes 28..86), the full range drawn on the on-screen piano.
   C0/C#0/D0 select Down, Up or Alternate picking.
2. Keep a note or chord held and click its row in the live fretboard for one
   hard repick without releasing the fretting key. The visible 8..1 column
   uses the same physical-string order as the number keys. Velocity-sensitive
   host or controller playing uses MIDI E6..B6 for physical strings 8..1; those
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

Before a valid MPE zone is declared, pitch wheels retain the original
channel-agnostic contract: the latest wheel applies the same standard ±2
semitone bend to every played and sympathetically ringing string, so a chord
stays in tune. MIDI MPE Configuration RPNs activate lower or upper zones;
Pitch Bend Sensitivity RPN 0 then sets their member and master ranges. JUCE's
public zone layout keeps the whole-semitone part
for routing, while Electry separately retains RPN 0's Data Entry LSB so the
performed member and master ranges include their declared cents. Each member
channel then remains bound to its physical string, including two channels
playing the same pitch; its wheel uses the zone's per-note range, and the zone
master's wheel adds its declared interval to every member. Both paths travel
over Bend Time rather than snapping, and a layout change cannot orphan the Note
Off of a note that began in the preceding zone. A released string freezes only
its performed member interval while it damps; the zone master remains live for
that natural or CC 64-held tail. Member-wheel data received on the idle channel
is cached for its next finger instead of retuning the old tail. With no new
member wheel, the next finger starts centred while the zone-master bend remains
live. CC 121 recentres both wheels, nulls the current RPN/NRPN selection and
keeps the exact declared ranges; CC 123 retains Electry's historical
whole-instrument release semantics. Channel
pressure, polyphonic aftertouch, MPE CC 74 timbre and Note Off velocity remain
deliberately unassigned, so no uncalibrated message silently changes the
guitar. The modulation wheel (CC 1) is the
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
  natural two-stage decay and slow beating. The detune is a fixed fraction of
  the sounding period. Polarisation cross-coupling is the amplitude gain `c`
  of a memoryless, contractive two-by-two matrix at the folded-loop seam: a
  returned travelling wave encounters that matrix once per loop. Its nominal
  linear-with-`f0` voicing is now written explicitly as
  `c = 0.04 f0 / 96000`, so neither the host clock nor damping/dispersion phase
  compensation can retune it. The
  former `0.04 / compensatedDelay` approximated that nominal trajectory only
  within one internal-rate family; at the 96 kHz host boundary it halved `c`
  before doubling it again immediately above the boundary. On the 22nd-fret
  high E, host-rate spreads in the 0.1-0.5, 0.5-1.5 and 1.5-3.0 s windows fell
  from 0.80/2.64/6.65 dB to 0.063/0.092/0.153 dB across 44.1-192 kHz, including
  the 96/96.001 kHz seam. The regression bounds are now 1/1/2 dB in those
  windows. Removing filter phase from the coefficient raises that 48 kHz
  high-E late window by 1.56 dB; the numeric `0.04` period-product anchor is
  unchanged and is not presented as measured bridge data. Loop-filter phase is
  compensated analytically, holding the fundamental within a few cents
  across the fretboard at 44.1-384 kHz. At host rates through 96 kHz the
  complete physical and nonlinear signal path runs internally at 2x and is
  returned through a 63-tap halfband FIR; higher-rate hosts run natively.
- **Pick position:** the picking hand stays at a fixed distance from the
  bridge; it does not follow the fretting hand up the neck. Pick Position is
  therefore a fraction of the *open* string, and the pluck comb as a fraction of
  the sounding length grows by `2^(fret/12)`, so a note fretted high up moves
  toward the hollow, mid-string comb of a real guitar. The distance is mapped
  through the complete round-trip sounding period `P = fs/f0`: one-way travel
  to the pick is `x/c`, and the opposite travelling-wave image that forms the
  comb is separated by `2x/c = (x/L)P`. It is not mapped through the shorter raw
  delay line left after damping and dispersion phase compensation. A fixed hand
  therefore remains fixed through fret and String Age changes, while tension
  bends move its delay by the physically required inverse wave-speed ratio.
  Positions past the speaking string's midpoint retain their own modal phase
  instead of being mirrored or snapped to 49%; only the final two per cent are
  reserved as the endpoint/contact guard, symmetric with the bridge end. An Up
  stroke applies its established two-per-cent-of-open-scale bridgeward shift
  before the hand's metre variation and the fret stretch, then shares that one
  final physical guard with every other stroke.
- **Pick release:** the plectrum does not leave every string equally quickly. A
  wound .080 carries far more mass per unit length than a plain .009 at
  comparable tension, so it leaves the pick more slowly, and the duration of
  that release low-passes what enters the string. The corner follows the square
  root of the string's own open frequency, so the treble register keeps its
  brightness while the wound low strings lose the upper-partial surplus the
  ideal law gives them, and the pole's own attenuation at the fundamental is
  divided back out so the release governs timbre rather than level. The
  shipping `ELECTRY_DECOUPLED_PICK_RELEASE=ON` path uses the implemented
  digital pole's exact magnitude and keeps Pick Hardness on the
  contact/stiffness axis rather than multiplying the player's displacement a
  second time. `OFF` retains the former approximate release makeup and coupled
  hardness gain only as an explicit legacy comparator.
- **Pick contact:** the plectrum is neither a point nor symmetric. It touches
  the string over a patch — half a millimetre for a stiff sharp pick, around a
  millimetre and a half for a soft rounded one — so the reflected image of the
  excitation is spread over that width through the same full-period `2x/c`
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
  delay loop as `(1 - d/2) + (d/2) z^-M`, with `M` equal to the touch fraction
  times each polarisation's complete live fundamental period. For an ideal
  nondispersive string this is unity at a node and `1 - d` at an antinode; its
  positive mix coefficients sum to one, so the ideal contact law is
  contractive. Adding the damping and dispersion filters' phase back to the
  raw line delay keeps the apparent finger fixed through String Age, Mute
  Pressure, bends and the existing pitch smoother. The natural harmonic rests
  that finger on the midpoint, removing the odd series so the octave arises
  from the string instead of from retuning the loop: the string keeps its own
  length, inharmonicity, decay targets and pickup comb. A single temporal tap
  cannot reproduce every spatial node of a dispersive stiff string exactly;
  that would require a local bidirectional contact model. The finger lifts once
  the note has formed — the partials it removed cannot be re-excited — which
  stops paying for the two extra delay reads and lets the harmonic ring as long
  as the string does. The clamp that used to stand in for the finger cost the
  open A2's octave partial 16 dB per second of loss nothing physical was asking
  for. The pinch harmonic is the same filter driven by the other hand: the
  thumb catches the string at the pick's own position, so Pick Position chooses
  which partial squeals. Near the bridge it selects a high partial; approaching
  the midpoint leaves a low even partial rather than guaranteeing the octave,
  because modes at the touch node also begin near the pluck's own null. Passing
  the midpoint walks that sequence upward again with the far side's distinct
  modal phase—it is not another midpoint setting. The current E3 regression
  selects partials eight/four/nine at the bridge, near-midpoint and far-side
  settings, moves the tracked mean 6.03 -> 4.74 -> 7.06, and lifts the bridge
  squeal 9.23 dB against an ordinary stroke. It is a
  firmer, longer contact than the fretting finger's because the mode-shape law
  gives a touch that close to the bridge little purchase on the low partials —
  the fundamental loses about seven per cent of its energy per round trip where
  a midpoint touch takes nearly all of it — and that asymmetry is the technique
  rather than a shortcoming of the model.
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
  the stiffness coefficient follows that same fractional speaking length at
  the string's established tension instead of jumping to the destination fret.
  Phase-compensated delay translation keeps the effective pitch continuous at
  the handoff, through in-flight dispersion refits and at the exact endpoint.
- **Frets:** fretting position drives sounding length, inharmonicity,
  pickup comb geometry, and Fleischer-style dead-spot damping (deeper on
  the bolt-on end of the construction axis). The Artifacts path can open a
  decaying fret-collision window on hard-picked notes that soft-limits
  displacement and re-radiates deterministic rattle. Hammer-ons retarget a
  sounding loop without clearing its state.
- **Attack pitch:** shipping hard strokes keep the written pitch across all
  eight strings. A compile-time, default-off energy experiment now seeds a
  bounded common tension coordinate only when an ordinary plectrum contact
  releases, using the wound steel core for axial elasticity and both
  polarisations for transverse energy. It passes a frozen conventional-E2
  compatibility gate and adds under 1% CPU in the registered eight-string
  stress cases, but remains disabled: the real cohort has one take per pickup
  on a six-string SG, not matched multi-velocity exact-eight train and holdout
  captures. Legacy pitch bend moves every delay target by the
  same ±2 semitone ratio; an active MPE zone instead composes its declared
  member and master intervals on each channel-owned string. Both use the Bend
  Time glide. Hold the visible A#0 **VIB** key
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
  E2 at the shipping default, the peak of the first 50 ms spans 18.3 dB from
  velocity 1 to 127 and 3.7 dB across the top half of the keyboard alone,
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
  string's own transverse wave speed `c = 2 L f`: 3043 Hz on the unbent E2
  string and 4062 Hz on the A2 string, against Lemme's approximate 3000 Hz and
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
  rectangular-window FIR for both types, because it is one bobbin either way.
  Wheel/MPE bend and fretting vibrato now move the position tap, representable
  aperture and coil spacing together through their live `1/c` delays; the
  implementation clamps an aperture shorter than one internal sample to its
  one-sample identity floor. A pure fret or slide remains neutral because `L`
  shortens by the reciprocal of its unbent frequency rise, while a tension
  gesture changes `c`. Putting the notch where it belongs also rebalances
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
- **Body:** three pickup-observed modes from Ray, Kaljun and Straže's controlled
  one-walnut/one-ash comparison receive bridge motion and contact noise. Each
  frequency and loss factor remains paired only with its measured mate and each
  resonator is normalised at its own modal peak. Structural drive is converted
  once to induced voltage before the bank, guarded, then joined with string
  pickup voltage before the same coil; displacement is never mixed directly
  into an electrical signal. The unpublished residues remain deliberately quiet
  direct-body voicing, and the modes do not become a broadband string-loss law:
  `bodyConductance` is zero and `bodyLossFactor` is one. Body Size, Shape and
  Construction are exact no-ops because the study held them fixed and the
  reviewed sources do not identify reusable independent laws for them. Guitar
  Build follows six curated short, balanced and extended scale/gauge setups
  while Body Wood moves monotonically between the specimens. Those setup choices
  are ordinary guitar voicing, not a material law. One specimen per material,
  unequal body mass and no released raw transfer data make this a narrow
  matched-material model, not a general tonewood taxonomy. This is the default
  `ELECTRY_MEASURED_BODY_RESPONSE=ON` shipping path; `OFF` retains the former
  four-mode geometry-informed voicing only for explicit legacy comparisons.
- **Bridge coupling:** every string you are not fingering is a
  real waveguide, not a resonator bank. A bounded displacement-domain proxy for
  each played string's bridge motion is summed into a one-sample-delayed bus
  that drives the idle strings' own loops
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
  all, and it now differs from the same chord at Resonance 0 by -60.11 dB at the
  20% default and -50.55 dB at maximum over the first 1.5 s. At the original
  coupling checkpoint those differences grew to -45.9 dB and -36.2 dB over
  10-12 s - the effect grows in the tail, where coupling belongs.
  It does not get louder than that: the difference is exactly linear in the
  injection gain, and rendering the same chord with the cap lifted, the network
  stops being a filter and diverges at roughly twenty times the gain the bound
  permits. The alias floor
  is 162.2 dB below the spectral peak because the
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
  isolated E1 comparison, its 30-100/100-250/250-380 ms levels are
  -7.31/-14.16/-22.88 dB. Its picked onset is +0.30 dB versus Sustain, 99.93%
  of tracked harmonic power is below 250 Hz, and its 100-250 ms tail is
  -14.59 dB versus Sustain: a dark periodic thunk, not a gate. Mute Pressure
  can stack as the separate bridge hand; Mute Tightness changes only the Mute
  style.
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

- **Oversampled clipping.** At 44.1 and 48 kHz the distortion pedal and the
  amplifier run inside an 8x oversampled domain, reached through three cascaded
  Kaiser-windowed halfband stages whose kernels are designed at `prepare()`
  time rather than tabulated. A
  gain stage fed at host rate folds its own upper harmonics straight back into
  the guitar band, and that folded intermodulation is most of what makes a
  modelled high-gain tone read as digital: across pedal, amplifier, stacked
  and quiet-amplifier steady-tone probes at both standard rates, the current
  non-harmonic floor is -70.2 to -83.2 dB; the previous host-rate chain
  measured -28 to -40 dB on the same class of probes. From a 48 kHz host upward,
  the smallest 1x, 2x, 4x or 8x path that reaches a 384 kHz internal clock is
  selected; 44.1 kHz is the maximum-8x exception at 352.8 kHz. The resulting
  stage boundaries are 96, 192 and 384 kHz, and both sides of each remain below
  the same -70 dB alias rail. Power-of-two staging puts the 88.2, 176.4 and
  352.8 kHz host family at a 705.6 kHz internal clock; every rate-derived
  circuit state uses that actual frame clock rather than the 384 kHz host-rate
  ceiling.
  While engaged it adds 20.125 host samples of fixed group delay below 96 kHz
  (0.42 ms at 48 kHz), then 17.25/11.5/0 samples across the 96/192/384 kHz
  stage retirements. With both gain controls at zero it is skipped outright,
  costs nothing, and adds no delay at all.
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
  measured 1 kHz insertion losses at 384 kHz are −13.0063 and −5.84533 dB;
  recovery happens before the output section, not as post-cabinet EQ. Modern
  High-Gain retains the established Electry circuit topology and voicing; the
  higher internal rate intentionally changes its wet samples.
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
  plate and leaves a real common negative bias displacement after overload. Its
  companion conductances use the exact nonlinear frame rate: the checked
  384/705.6 kHz small-signal output-capacitor response agrees within the 0.05
  dB regression bound at 40 Hz relative to 1 kHz.
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
  responses, individual speaker solves, microphones or room captures. The
  default-off `ELECTRY_MEASURED_MODERN_CABINET` CMake option replaces only the
  Modern cabinet filters with the vendored, phase-preserved CC0 impulse prefix
  for controlled comparison; it is not the shipping path pending blind
  listening.
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
- **Cost.** On an Apple M1 Max, a comparable optimized render of all 23 demos
  (289.33 seconds of audio, including the string model and mixed dry/wet
  material) moved from 21.94 seconds CPU / 22.71 seconds wall time with the
  previous 4x standard-rate path to 33.65 / 35.27 seconds with the current 8x
  path: +53.4% CPU, +55.3% wall, and 0.116x/0.122x aggregate realtime. This is
  an end-to-end workload, not a worst-case amp-switch benchmark. The earlier
  isolated harness was not retained, so its 48 kHz rows are historical rather
  than current claims. Its still-current 96 kHz 4x rows—the same 384 kHz
  nonlinear rate with twice the host-rate work—measured the metal/all-max
  chains at 0.169x/0.178x, settled American/British at 0.421x/0.434x, Modern at
  0.055x, continuous A↔B at 0.842x and pathological three-way switching below
  0.90x. A fresh isolated standard-rate benchmark remains due. Timed process
  rows make zero heap allocations; the two 3.288 MiB pair tables take about
  0.65 seconds once per process to construct during the first `prepare()`,
  while same-rate reprepare is about 0.55 ms.

### Guitar Build

One host control replaces the six separate Wood, Size, Shape, Neck Join, Scale
and Gauge dials. **Guitar Build** travels through six curated shipping setups.
Between adjacent anchors a smoothstep interpolation moves Body Wood
monotonically through Ray's walnut-to-ash pair and changes scale length and
Drop-E gauge inside their ordinary ranges. The scale/gauge associations are
voicing, not evidence that material causes scale or string tension; Body Size,
Shape and Construction remain exactly neutral.

| Position | Shipping anchor | Voiced setup |
| ---: | --- | --- |
| 0.0 | Walnut short/heavy | walnut endpoint, 25.5-inch scale, heavy set |
| 0.2 | Short medium-heavy | 26-inch scale, medium-heavy set |
| 0.4 | Medium balanced | 26.5-inch scale, balanced set |
| 0.6 | Long light | 27-inch scale, lighter set |
| **0.8** | **Extended heavy** | **27.63-inch scale and heavy `.011-.098` set** |
| 1.0 | Ash extended/light | ash endpoint, 28-inch scale, light `.009-.080` set |

The 0.8 anchor is the current fitted default. Pickup selector and coil type,
Tone, Body Resonance amount, string age and all picking/hand controls remain
independent, so Guitar Build changes the instrument under the player without
silently moving the pickup or the performance. The former six-anchor
construction trajectory is retained only by an explicit
`ELECTRY_MEASURED_BODY_RESPONSE=OFF` legacy-comparison build.

The historical fit that selected the retained scale/gauge/player default
remains the same evidence: against nine muted references at five pitches the
legacy path's joint error was 5.03 dB, compared with 6.31 dB at the old
all-midpoint prototype. Moving only the four structural weight coordinates
scored 6.41 dB; moving Pick Position away from 0.18 cost 2.1 dB and selecting
the neck pickup cost 1.5 dB. The result selected a whole voicing, not evidence
that any single material coordinate identifies a guitar.

The earlier isolated-coordinate audit is retained as history because it
motivated replacing the legacy construction trajectory. Sweeping each legacy
coordinate end to end by the same normalised-difference measure the suite uses
produced:

| Axis | On the old midpoints | On the new defaults | Suite floor |
| --- | --- | --- | --- |
| body wood | 0.058 | 0.047 | 0.055 |
| body size | 0.106 | 0.045 | 0.055 |
| body shape | 0.066 | 0.042 | 0.055 |
| construction | 0.065 | 0.058 | 0.055 |
| string gauge | 0.125 | 0.050 | 0.080 |
| body resonance | 0.080 | 0.028 | 0.080 |
| pick position | 1.341 | 1.346 | 0.350 |

On that legacy path's fitted default, five isolated structural/gauge sweeps
lose between a tenth and two thirds of their relative range, and four sit below
their original suite floors. A darker, heavier, louder note makes each isolated
coordinate a smaller fraction of the whole; Body Resonance is worst hit. The
current `testGuitarBuildRangeIsAudible` therefore measures the six complete anchors,
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
| 4 | `guitarBuild` | Guitar build | 0..100%; default **80% Extended heavy / 27.63-inch / `.098` low E** |
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
| Two-stage decay and beating | Two-polarisation string behavior described in the same plucked-string literature; [Evangelista and Raspaud's vector bridge scattering](https://dafx.de/paper-archive/2009/papers/paper_97.pdf); [Bank and Karjalainen's passive admittance-matrix bridge model](https://www.dafx.de/paper-archive/2010/DAFx10/BankKarjalainen_DAFx10_P60.pdf) | The polarisation parallel to the body carries a 1.7x longer decay target and a sub-cent detune, so the mixed output beats slowly and decays in two stages. A memoryless contractive two-by-two seam matrix uses cross-amplitude gain `c = 0.04 f0 / 96000`, preserving the nominal linear-with-`f0` voicing while making the same-pitch coefficient independent of the host clock and digital filter phase. The top-note decay spread is 0.153 dB in the latest 1.5-3.0 s regression window across 44.1-192 kHz, including the 96/96.001 kHz rate-family seam | A qualitative reproduction of the documented mechanism with a rate-invariant voiced scalar; not calibrated polarisation data, a measured frequency-dependent bridge reflectance, or a characteristic-impedance-normalised passive multiport |
| Stiffness dispersion | [Kemp's bass-guitar derivation](https://doi.org/10.1007/s42452-020-2391-2), `B = pi^2 E S kappa^2 / (T L^2)`, and robust factored allpass design practice (Rauhala and Välimäki; Abel and Smith) | A per-note `B` from string diameter, an effective wound-core bending fraction, live fractional speaking length and performance tension drives an eight-stage factored first-order cascade; two coefficients are fitted jointly at low and high partials, with exact fundamental phase compensation. Played voices and one-polarisation sympathetic opens share that fit. Pure fret/slide travel retains the established tension and follows `1/L^2`; wheel/MPE bend, the idle bank's global bend and fretting-hand vibrato use `T proportional to f^2`, so an `s`-semitone tension gesture targets a `2^(-s/6)` scaling of `B` through the existing bounded, quantised refits | A formula-derived, bounded two-band fit whose regression error is under 20% at both references for the worst heavy Drop-E case, with empirical construction values rather than measured bend/vibrato calibration. The compile-time default-off energy-attack pitch remains deliberately outside this static dispersion fit; this is not a capture-fitted very-high-order piano dispersion filter |
| Loop damping and tuning | Decay-time-targeted loop-filter design from the plucked-string literature; a dry electric low-E reference recording for the targets themselves | Per-string, per-fret one-pole loop filters solved by bisection from independent T60 targets at the fundamental and a high reference frequency, with all loop-filter phase delays compensated analytically at the fundamental. The wound strings' fundamental targets are tens of seconds and their high-frequency ratio two orders of magnitude smaller, following the reference | Decay-targeted loop design with exact fundamental tuning (regression bound: under 8 cents across E1..D6 at tested host rates through 384 kHz), whose fundamental and high-frequency targets are calibrated against one reference recording; not per-partial measured decay matching across a fretboard, and not a model of the reference instrument |
| Dead spots | Fleischer's electric-guitar dead-spot studies relating neck conductance to decay time | A per-string fret-position Gaussian that locally shortens decay, deepened by the bolt-on end of the construction axis | The documented mechanism direction with voiced positions and depths; not measured conductance maps of specific instruments |
| Attack pitch | Tolonen, Välimäki, and Karjalainen's tension-modulation nonlinearity; Avanzini, Marogna, and Bank's quasistatic energy store; Lee et al.'s measured common partial glide | Shipping keeps its compensated fundamental delay independent of pick velocity. The compile-time default-off experiment converts the resolved two-axis release energy through the steel core's axial `E A`, clamps the shared tension ratio to a seven-cent ceiling, and moves the complete compensated period without refitting static dispersion | A bounded research candidate that passes a frozen conventional-E2 descriptive compatibility gate with under 1% measured CPU overhead; not enabled shipping behavior, an exact-eight calibration, a causal identification, or evidence of market superiority |
| Plectrum and finger excitation | Plectrum and touch interaction modeling by Germain and Evangelista and by Evangelista and Eckerholm | A three-phase picked excitation combines a conservative contact-loss placeholder and scrape, a live-target-period-scaled modal release approximating triangular pluck displacement, a mass-dependent release pole and a smaller broadband pick edge; its fixed-metre position and asymmetric 0.5-1.5 mm contact patch map through the complete sounding period rather than filter-phase-shortened raw delay, retain far-side positions through the midpoint and reserve only a symmetric two-per-cent guard at either termination, while deterministic per-stroke draws vary force, position, angle and tip contact. Hammer/tap contacts bypass the wrist, plectrum-contact and pick-control paths; their modal projection retains the source target period at a settled legato contact, pull-off release position uses the same physical-period coordinate, and legato slides preserve the ringing loop and add only finger friction | A realtime modal approximation to released-string displacement plus bounded contact and pick detail, with one explicit physical boundary between plectrum and fretting-hand gestures; not an exact delay-line initial-condition solve, beam-mechanics plectrum profile, force-based finger contact solver, or local bidirectional plectrum-scattering junction |
| Fret collisions | Bilbao and Torin's energy-balanced string/fretboard collision modeling; [Poirot et al.'s perceptual study of collision location](https://doi.org/10.1109/TASLP.2023.3284515) | Shipping uses the Artifacts path's decaying collision window, whose soft limit clips vertical displacement against a velocity-dependent clearance and re-radiates deterministic rattle noise. The compile-time default-off positioned-loss candidate instead compares the unfiltered return with a following-fret tap at `D * 2^(-1/12)`, retaining the old zero-slope knee while giving the loss a `sin^2(n pi p)` modal location cue; distributed Dead contact keeps the shipping law | Shipping is bounded collision-informed contact behavior. The candidate is a one-read longitudinal loss surrogate that deliberately retains shipping's bilateral absolute-value barrier for an isolated A/B; it is not a unilateral fretboard obstacle, reciprocal two-rail junction, passive nonlinear contact proof, measured action/fret geometry, or FDTD distributed-contact simulation |
| Pinch harmonic | The same touch model driven by the picking hand; standard descriptions of the technique as a thumb contact immediately after the plectrum | The touch position is the pluck fraction, so Pick Position selects the spectral region; a firmer (depth 1.0) and longer (90 ms) contact than the fretting finger's, because the mode-shape law gives a near-bridge touch little purchase on the low partials. The co-located ideal pick/touch product tends to `abs(sin(2 pi n p))`, so approaching the midpoint selects a low even partial without promising the octave, while crossing it reverses that sequence with distinct relative modal phase | Position-dependent harmonic selection on both halves of the speaking string with the technique's own asymmetry preserved; not a measured thumb/pick separation, thumb geometry, pick grip, or exact contact-area model |
| Touch harmonics | The touch-interaction half of Evangelista and Eckerholm's player/instrument models, and the classical mode-shape result that a point contact removes energy as `sin^2(n pi p)` | A one-tap contact `(1 - d/2) + (d/2) z^-M` inside each polarisation loop. `M` is `p` times the complete live fundamental period—raw current delay plus its cached damping/dispersion phase—so build damping cannot move a fixed finger and the existing bend smoother and horizontal detune remain continuous. The ideal nondispersive response is unity at a node and `1 - d` at an antinode. The natural harmonic touches the midpoint, so its octave comes from the string's own series, inharmonicity, decay and pickup comb; the finger lifts once the note has formed | A low-cost fundamental-period realization that targets the ideal nondispersive node law subject to cubic fractional-delay interpolation; additionally approximate for a stiff string's inharmonic spatial modes, and not a distributed finger-force or local bidirectional contact solve |
| Slide | Pakarinen, Puputti, and Välimäki's virtual slide guitar, whose string algorithm carries a parametric model of the tube/string contact noise produced by a wound string's surface ridges | The finger stays down and the sounding length glides at a hand speed in frets per second rather than over a fixed time; the friction is a noise band centred at `v / w`, the hand's speed along the string over the winding pitch, with its level following the derivative of the glide. A chained gesture samples the live log-frequency and fractional fret, derives duration and friction from the remaining physical path, moves stiffness with the live `1/L^2` coordinate, and translates raw delay for filter-phase changes so the complete loop period remains continuous | A finger-position-, stiffness- and effective-pitch-continuous time-varying waveguide plus a velocity-dependent friction band, with the winding pitch a fitted linear stand-in for real wrap-wire practice; not a velocity-continuous spline, an energy-compensated time-varying waveguide, or a measured contact-noise spectrum |
| Hammer-on and pull-off | Touch/legato interaction models from Evangelista and Eckerholm | Keyswitched legato: a sounding string within reach retargets its delay over about 10 ms while the loop state is preserved. An ascending note gets the established soft finger impact; a descending note, including a release to an open string, excites the old fret's position in the lateral plane, both without plectrum noise. A mid-glide Hammer takes its source and direction from the live fractional fret while phase-compensated delay translation preserves effective pitch. Neither gesture moves a planted Palm hand or rewrites sibling damping | Pitch-continuous, direction-aware legato with conservatively voiced finger attacks; not a velocity-continuous spline or a distributed or capture-fitted finger-force model |
| Pickups | [Paiva, Pakarinen, and Välimäki's pickup acoustics and modeling](https://research.aalto.fi/en/publications/acoustics-and-modeling-of-pickups/); low-frequency pickup nonlinearity measurements (Novak et al.); engineering aperture analyses | Played and sympathetically ringing idle strings use distinct neck/bridge position combs, with each delayed tap weighted 0.60 so the null is 12 dB deep rather than infinite; O(1) fractional moving averages implement the rectangular apertures and factored two-coil sums. Position, aperture and coil spacing share the live transverse wave speed during wheel/MPE bend and finger vibrato. Played strings then use bounded flux nonlinearity plus shallow string-mass/pole balance, while the low-level one-polarisation idle path stays linear; both are differentiated into induced EMF, guarded ultrasonically, then passed through the loaded coil/tone circuit | The published pickup signal structure (position comb of measured rather than ideal null depth, finite aperture, nonlinear played-string flux, induced voltage, electrical resonance) with one geometry-derived live-tension correction and datasheet-plausible level calibration; not a magnetic finite-element, per-coil, or capture-fitted model of named pickups |
| Solid body | Ray's controlled walnut/ash pair; Paté's bridge-admittance and decay analysis; Elliott, Magill and Kendrick's vibro-electric transfer measurements | Structural bridge displacement is differentiated before three double-precision, peak-normalised modal resonators and a 4 kHz guard, producing quiet body-induced voltage before the loaded pickup coils. Each Ray frequency/loss-factor pole stays paired with its measured mate; no unsupported termination loss is added | A narrow specimen-to-specimen structural pickup-colour morph with voiced residues inside measured transfer bounds; not undifferentiated acoustic displacement, transferable complex bridge mobility, or a general material taxonomy |
| Guitar Build | Ray's controlled walnut/ash pair and modern extended-range scale and gauge practice | Body Wood moves monotonically through the paired modes while six curated short/extended and light/heavy string setups provide the audible path. Unsupported body size, shape and joint coordinates remain neutral; Pickup construction, Tone and Body Resonance amount remain independent | An evidence-limited material morph plus an explicitly voiced setup path; not evidence that material determines scale/gauge or that Shape, Size or Construction has been identified |
| Play noise | Handling-noise observations in the virtual slide guitar work of Pakarinen, Puputti, and Välimäki | Deterministic seeded plectrum scrape, finger contact, and release damping noise, band-shaped per string (wound vs plain) and split between a one-percent string trace and local pickup/body paths | Procedural, deterministic contact noise consistent with the documented mechanisms; not convolved recordings or measured contact-noise spectra |
| Sympathetic string coupling | Bank and Karjalainen's passive admittance modeling and the sympathetic-string literature | Every played string publishes a bounded displacement-domain bridge-motion proxy to a one-sample-delayed bus. A played voice reads the sum minus its own previous contribution through a dynamically capped, zero-diagonal feedback gain; an idle open-string waveguide reads the same bus feed-forward and never writes it. The cap holds the played-string network's row-sum norm at or below 0.25. Each one-polarisation idle loop uses the same two decay targets, eight-stage stiff-string dispersion fit, live global-bend tension law and exact total loop-phase compensation as a played-open string of the same steel, then reuses its otherwise-idle selected neck/bridge position, aperture, coil-pair and Faraday histories. Exact bypass clears every inactive loop and pickup memory before dropping its ready flag | A reciprocal scalar played-string feedback approximation with explicit small-gain stability, plus a feed-forward idle-string bank and full spatial pickup observation; not a characteristic-impedance-normalised, positive-real passive bridge-admittance multiport or a second two-polarisation idle solve |
| Dead note | Fretting-hand dead-note distinctions and distributed player/string contact; four CC0 Drop-E eight-string ghost attacks | An independent additive fretting-hand loss with a 1.6 s low-order target, its upper fit at the eighth partial, and a lightly darkened pick attack; Mute Pressure may stack as the separate bridge hand | A contact loss inside the loop whose stateful E1 envelope and centroid track the four-hit reference; not a gate, maximum palm mute, distributed finger-force solve, or universal calibration of dead-note hand coverage |
| Bridge-hand damping | Palm-muting practice; pressure sensing by Biral, d'Alessandro and Freed; post-attack spectral evidence from Reboursiere et al. and Guitar-TECHS; HiMMP's score-matched rhythm DIs; the CC0 extended-range `50hz-guitar` muted/sustained matrix; the same decay-targeted loop design; and dry muted power-chord references for the depths | The hand is an absorber whose loss adds to the string's own in parallel, so decay rates sum at each fitted frequency independently; its solved spectral loss is present when the pick releases and can relax only after the string establishes a measured energy peak. The raw hand rate is multiplied by 4.5 at the high reference and divided by twenty-two at the fundamental, an effective 99:1 ratio between the two fitted points. Mute Tightness continuously controls that loss for the Mute style; Mute Pressure retains it on every style, the newest actual picked contact updates the shared hand on already-ringing strings without rewriting their attack style, and an old-to-new filter-phase translation keeps those live contact changes pitch-continuous. Non-plectrum legato retains an existing Palm | Progressive additive damping with an independently voiced pick attack; not a distributed hand/string force solve, measured heel footprint, commissioned per-harmonic fit, or capture fit to a named eight-string |
| Fretting hand | Ordinary left-hand kinematics; [Itoh and Hayashida's constrained fingering optimisation](https://www.jstage.jst.go.jp/article/ieejeiss/124/7/124_7_1396/_article/-char/en) and [Yazawa et al.'s playable-configuration enumeration](https://cir.nii.ac.jp/crid/1573387452726377216); the position/reach controls exposed by sampled guitars | Exact-sample chord attacks are matched across all eight strings as one bounded problem: held-note and legato continuity, occupied strings, a four-fret hand shape, fret effort and uncrossed pitch order resolve before any voice starts. The chosen shape then enters the ordinary physical attack path in canonical pitch order, so host event order cannot alter the fingering or player-variation stream. Single notes retain the floating hand, out-of-reach shift and phrase return | A deterministic chord-local configuration solver with a fixed four-fret reach; not finger-by-finger anatomy, chord naming, or phrase-wide look-ahead |
| Strum travel | Ordinary plectrum kinematics | Note-ons on different strings no more than 35 ms from the chord's first event are one stroke; its direction and extreme string set an accelerating travel order, every crossed string shares that direction, and Alternate advances once for the chord. Reusing a string starts a new stroke; a fully cancelled pre-contact chord consumes none | Deterministic, jittered accelerating pick travel across the string plane; not a model of pick angle, chord recognition, or the player's wrist trajectory |
| Tremolo picking | Official Shreddage Hydra, RealEight, Electric Storm Deluxe, Evolution Dracus and Heavier7Strings repetition workflows; Armondes' five-player direct/progressive tremolo experiment; Electry's planned exact-eight capture protocol at 8/12/16 strokes/s | Hold visible B0 to run one deterministic wrist through the existing physical repick path. One shared fractional phase preserves Alternate/chord direction, velocity remains force, rate is 4..20 strokes/s with a 12/s default, an in-flight Strum traversal cannot be overwritten, a same-string legato move retargets rather than cancels its travelling pick, and each contact on a live, ringing held note leaves its vibrato finger intact. Because B0 and E6..B6 explicitly command the picking hand, a latched Hammer becomes neutral Sustain for that dedicated contact only and remains latched for later playable notes | A playable sample-accurate repeat scheduler whose 12/s default overlaps the published conventional-guitar direct-speed cluster and whose 8/12/16 anchors are ready for commissioned exact-eight capture; not host-synced pattern generation, a human timing distribution, or an exact eight-string rate/force fit |
| Fretting vibrato | Guitar-TECHS CC-BY raw DIs; Magalhães et al.'s eight-player electric-guitar vibrato study | Hold the visible A#0 gesture; velocity controls a smooth-onset, upward-only width, with independent rate, depth and phase draws per physically held stopped string. Pre-held intent waits for a stopped finger; open and key-released strings remain fixed. Picking-hand repicks and overlapping ownership of the same held or delayed note preserve its onset and finger, while a released/refretted note assigns another. Velocity 64 is nominally about 5.6 Hz / 20 cents and 127 about 6.4 Hz / 40 cents | A playable finger-rock model whose existing range overlaps published six-string players; not an exact eight-string calibration or a finger/string force solve |
| Pitch control | Legacy MIDI pitch bend, the official [MIDI MPE specification](https://midi.org/midi-polyphonic-expression-mpe-specification-adopted), and [MIDI 1.0 RPN/controller semantics](https://midi.org/midi-1-0-control-change-messages) | With no active zone, the latest wheel applies the same ±2 semitone offset to every played and sympathetically ringing string. In a declared lower or upper MPE zone, each member channel owns a physical string and uses the exact semitone-plus-cent per-note range retained alongside JUCE's integer routing state; the exact master range adds to every zone member. Same-pitch member notes remain distinct, layout changes retain outstanding Note Off ownership, a released tail freezes its member interval while its zone master stays live, idle-channel wheel data initializes the next finger, and both paths use Bend Time. CC 121 recentres wheels and nulls RPN selection without deleting the zone or its exact ranges; CC 123 retains whole-instrument release semantics | Standards-gated, fixed-state per-note pitch without changing ordinary MIDI; not a physical vibrato-bar simulation. Per-note pressure, CC 74 timbre and release velocity remain unassigned |
| Amplifier feedback | Acoustic guitar-to-amplifier feedback practice: a loudspeaker's pressure field re-excites the strings, while high-gain players control unused strings | A sample-rate-derived FIFO holds a voiced nominal 5.805 ms acoustic delay (256 samples at 44.1 kHz), while the plug-in renders, amplifies and returns causal chunks no longer than that delay so DAW block size cannot select the howl. A soft-clipped, gain-scaled copy drives played strings fully and idle sympathetic strings at a voiced one-quarter direct share while leaving their bridge drive unchanged. CC1 resonance, Resonance Depth and rig acoustic loudness scale the path, so a distorted tone at full wheel regenerates while a dry DI never can; every element is bounded | A fixed-delay, level-gated, saturating regeneration path with a performance-voiced unused-string share; not a measured player-to-speaker distance, finger-by-finger muting, room acoustics, speaker directivity, or a standing-wave model |
| Controllable artifacts | The same touch/collision literature plus bridge-hardware behavior | An exactly bypassable deterministic path combines a bridge-hardware modal bank driven through the selected pickup mix, incidental fret contact on hard-picked notes, and per-string saddle rattle, all driven by played energy. It is mechanical hardware noise, distinct from the sympathetic string coupling above | Plausible procedural imperfection with bounded feed-forward resonators; not measured hardware-noise statistics |
| Audible-work culling | Standard realtime-DSP practice | A pickup faded out by the selector is skipped entirely; Mono runs one shared coil/DC/decimation chain and mirrors it; damping-only control moves reuse the existing dispersion fit; the whole engine freezes to exact zero once nothing vibrates and the shared path is below -120 dBFS | Removal of inaudible arithmetic with the audible result unchanged; not a quality/latency trade |
| Oversampling | Standard nonlinear-audio antialiasing practice | The physical, body, collision, and nonlinear pickup path runs at 2x for host rates through 96 kHz, followed by a fixed 63-tap halfband FIR; higher-rate hosts run 1x. The separate distortion/amplifier domain uses the smallest 1x, 2x, 4x or 8x path reaching 384 kHz from a 48 kHz host upward, with 44.1 kHz bounded at 8x/352.8 kHz | Genuine internal oversampling and filtered decimation, with all tested gain profiles below a -70 dB alias floor on both sides of the 96/192/384 kHz stage transitions; not a quality label applied to a native-rate nonlinear stage |
| Output modes | Phase-coherent divided/hex pickup practice, ordinary double-tracked guitar performance, and four CC-BY HiMMP rhythm DIs | Mono is the conventional summed DI. Stereo weights one engine's modeled strings by physical lateral position and folds coherently to mono. Double runs two complete, differently seeded mono engines into left and right before one shared FX chain; the second gets one deterministic 0-6 ms causal timing offset per picked wrist stroke, shared across a chord and composed with strum travel | Stereo is a virtual divided-pickup string field and Double is two deterministic modeled performances rather than a delayed copy. Its tight timing envelope is directionally grounded in conventional Drop-C takes, not capture-fitted eight-string timing, and it does not claim to reproduce the decisions of two human performances |
| Distortion pedal | [Yeh, Abel and Smith's antiparallel-diode RC formulation](https://dafx.de/paper-archive/2007/Papers/p197.pdf) and standard nonlinear-audio antialiasing practice | A 2.2 kOhm / 10 nF Shockley-diode node whose capacitor state is trapezoidally integrated and solved with bounded Newton iterations at every oversampled step, surrounded by Electry's eighth-string input and voice filters | A real circuit solve of the documented clipping node; not a full named pedal schematic, component-tolerance study, or SPICE validation |
| Amplifier and speaker/cabinet | [Yeh and Smith's exact passive tone-stack derivation](https://dafx.de/paper-archive/2006/papers/p_001.pdf); [Dempwolf and Zölzer's measured 12AX7 model](https://dafx.de/paper-archive/2011/Papers/76_e.pdf); Reefman's measured [uTracer TubeLib ECC81/ECC83/6L6GC/EL34 fits](https://www.dos4ever.com/uTracer3/TubeLib.inc) and [model derivation](https://www.dos4ever.com/uTracer3/Theory.pdf); [Fender Twin Reverb AB763](https://schematicheaven.net/fenderamps/twin_reverb_ab763_schem.pdf), [Marshall 1959](https://www.prowessamplifiers.com/schematics/Marshall/1959_superlead.pdf) and [Macak/Schimmel](https://www.dafx.de/paper-archive/2010/DAFx10/MacakSchimmel_DAFx10_P12.pdf) phase-splitter and grid-coupling circuits; [RCA 6L6GC](https://frank.pocnet.net/sheets/049/6/6L6GC.pdf) and [Mullard EL34](https://frank.pocnet.net/sheets/129/e/EL34.pdf) push-pull operating points; official [Fender amplifier](https://www.fmicassets.com/Damroot/Original/10001/021730_gamp_manual_all_revE.pdf), [Marshall 1959](https://www.marshall.com/us/en/product/1959-handwired-head?pid=1007086) and [Peavey 6505](https://assets.peavey.com/literature/manuals/00575680.pdf) architecture references; official [Jensen C12K](https://www.jensentone.com/vintage-ceramic/c12k), [Celestion G12M](https://celestion.com/product/g12m-greenback/) and [Vintage 30](https://celestion.com/product/vintage-30/) response boundaries | Three complete paths share a residual-checked measured-current 12AX7 preamp table. American and British add exact unloaded third-order passive RC stacks; nonlinear ECC81/ECC83 long-tailed pairs jointly solve two source-valued output capacitors, 220 kOhm bias returns, factory grid stoppers and TubeLib overload-grid conduction; common/differential-sensitive 6L6GC or EL34 load-line tables retain the resulting blocking shift. The American 6L6GC pair solves individual AB763-derived 470 Ohm screen branches around the RCA operating family; the British EL34 pair solves Mullard's common 800 Ohm resistor from its matching pair condition. These screen equations are coupled to the plate load-line solve and baked into the preparation-time tables. Voiced negative feedback, plate-plus-screen-current-driven sag and independent transformer state follow; Modern preserves the established cascaded path. Six biquads per path supply distinct zero-latency speaker/cabinet response voices, and all nonlinear work stays inside the up-to-8× oversampled block | Exact passive RC transfers, translated measured-current preamp/phase-inverter/power-tube formulae, residual-bounded coupled LTP/grid solves and bounded ideal push-pull load-line/screen-resistor solves inside three deliberately voiced paths. Terminal grid conduction and blocking recovery are modeled while positive-grid plate transfer remains explicitly AB1-bounded. The American screen components are a documented-source hybrid, not a complete Twin DC stage; the upstream 1 kOhm / 20 uF dynamic screen node, Marshall quartet/choke supply, reactive transformer/speaker load, full positive-grid plate data, complete named schematic, component-tolerance or specimen fit, cabinet IR, loudspeaker mechanics, microphone and room capture remain outside the claim |

## Known gaps

### What the automated tests do and do not establish

Ordinary tests deliberately do not assert callback deadlines on unknown
hardware. An opt-in native-processor benchmark now times `processBlock()` after
warmup at 48, 96 and 384 kHz with 64- and 512-frame callbacks. It covers the
default Bridge/Mono path, Both/Stereo, Double, eight-string Palm, Modern with
all five FX amounts at maximum, and continuous American/British amp switching.
Every row uses the same repeatedly struck eight-open-string chord and reports
nearest-rank p50/p95/p99/max callback times plus deadline misses. Preparation,
host parameter writes and output validation are outside the timed interval.
Palm uses Both/Stereo; Modern/all-max and amp switching use Bridge/Mono, with
the latter holding Amp at 100% and changing American/British before every block.
Only silence, non-finite output and a deliberately loose one-second-per-block
runaway guard fail the test; realtime results remain machine- and load-specific.

One Release run on an Apple M1 Max under macOS 26.5.1 on 2026-08-29 recorded
zero misses in all 30 steady rows. The tightest steady p99 was Double at 384
kHz/64 frames: 0.0760 ms of its 0.1667 ms deadline; the largest individual
steady callback was a Palm outlier at 0.1541 ms. The deliberately hostile
per-block American/British switch missed 147/375 and 174/256 callbacks at 48
kHz, 598/750 and 256/256 at 96 kHz, and 2,987/3,000 and 375/375 at 384 kHz for
64 and 512 frames respectively. That row identifies an automation boundary,
not the cost of a selected, settled amplifier.

Current automated tests establish: finite, bounded, non-silent output for
all twenty-one pick-stroke/play-style combinations at 44.1-384 kHz; the 2x/1x
internal-rate policy, exact
host-to-physical clock timing, and filtered-decimation pitch stability;
exact-silence idle output;
sample-identical renders for identical MIDI (including across engine reuse,
which caught a real aperture-state leak during development); fundamental
accuracy within 8 cents across E1..D6 at three rates; stable allpass bounds
and under-20% low/high dispersion-deficit fit error on the heavy short-scale
Drop-E case; exact played/idle-open stiffness-fit parity, all eight idle
allpass histories populated by the rendered loop, live-bend `1/T` scaling,
gauge rebuilds and phase-continuous idle wheel travel; positive bounded modal
conductance and exact structural-loss
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
touch law whose ideal closed-form magnitude never exceeds one at any depth, an
exactly absent touch on every other articulation, a finger that lifts, and a
harmonic whose octave partial decays within 2 dB of the same partial of the
ordinarily picked note - the direct evidence that the loop is no longer
retuned; a pinch harmonic whose energy-weighted mean partial index sits well
above an ordinary pick stroke's, whose strongest partial is at least the sixth
near the bridge and a low even partial with the hand over the neck, which gains
more than 6 dB on the fundamental, and which renders as neither a pick stroke
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
and never-configured coupled loops at 0%, a live full-to-zero bypass that
clears the inactive delay line and every loop-filter memory, coupling
determinism, a coupled
string handed back to the player when it is picked, and bounded, ring-out-to-
exact-silence behaviour at maximum coupling across three host rates; an idle
open string whose neck/bridge tap, aperture and coil-pair geometry exactly
matches the same played open string, whose spatial delays follow live bend,
whose single-coil endpoint removes both delayed coil reads, and whose culled
pickup histories stay exactly clear, with its damping and allpass memories
choked by the same factor as its retained line when the player takes ownership;
a decay envelope that agrees across 44.1, 48, 88.2, 96, 96.001 and 192 kHz to
within 1 dB through 1.5 s and 2 dB through 3 s; a polarisation seam whose
coefficient follows only sounding pitch and remains continuous across 44.1,
48, 88.2, 96, 96.001, 192 and 384 kHz with neither clamp engaged; a 22nd-fret
high E that still sustains on both sides of that host-rate seam; a coupled ring
whose
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
length by 2^(fret/12), while its fixed-metre centre and contact width remain
invariant through fretting and loop-filter phase and follow inverse wave speed
under a pre-bend;
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
and remaining distance, whose stiffness follows the live fractional fret
monotonically in both directions and lands on the independently settled
endpoint, whose in-flight dispersion refits cannot reverse the physical pitch
trajectory, whose friction band follows the speed of the hand,
which is far louder on a wound string than on a plain one and exactly absent at
a silent Finger Noise control, which injects no second modal attack and cannot
cancel or re-anchor a pending picked strum; a fretting hand that keeps a lead
phrase in one position instead of falling back to open strings, leaves the
open-position shapes untouched, retains a planted Palm hand through hammers,
pull-offs and legato slides without changing sibling damping, and relaxes to the
nut when the phrase ends;
legacy pitch-wheel travel plus fixed-state MPE member/master pitch composition,
same-pitch channel ownership in both directions across a zone-layout change,
an exact RPN semitone-plus-cent member range and a separately live master
interval, bounded-overlap rejection, idle-wheel
initialization, member-frozen/master-live release tails including a delayed
repick, new-finger thaw exactly at delayed contact, null RPN selection on CC
121 and re-prepare, and ownership clearing at Panic, prepare, All Sound Off and
All Notes Off;
sustain-pedal hold; the balanced A#0 fretting-vibrato
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
around an enabled module; and a portable CPU ceiling with the eight-active and
one-active/seven-idle settled and wheel-glide render ratios printed on every
run. The active worst case uses Stereo, maximum Body Resonance, and maximum
Artifacts mode. Mono is checked sample-for-sample dual mono; Stereo tests pin
physical low/high string orientation, coherent fold-down, bounded side level,
energy balance, determinism, and opposite string endpoints. The plug-in suite
additionally pins the 28-parameter layout, including Tremolo Rate's and Amp
Voice's appended indices, Guitar Build's named anchors,
formatted values, current-state round trips, bus layout, sample-accurate
note starts, MIDI controller behavior (bit-identical channel-agnostic pitch
before zone activation; same-sample MPE setup; selective member and additive
master bends using declared ranges, measured from shared search intervals that
reject negligible captures; an exact semitone-plus-cent member range; a
member-frozen, master-live CC 64 tail; retained layout and range but nulled RPN
selection through CC 121 and re-prepare; unassigned channel pressure and
polyphonic aftertouch; cached pre-note bends; release-tail isolation; bounded
ownership overflow; sustain,
all-sound-off, all-notes-off, Panic and prepare lifecycle clearing), UI
keyswitch triggering of both banks, panic, output-gain and APVTS
output-mode effects, three visible non-overlapping mode buttons, three visible
non-overlapping Amp Voice buttons, Modern default and missing-field migration,
Space activation for every keyboard-focusable PANIC and exclusive-choice
button with disabled controls rejecting it, plus wrapping arrow-key selection
across every exclusive-choice strip,
all 25 knob accessibility handlers exposing their canonical full parameter
names while retaining the compact visible captions,
deterministic
distinct Double channels, an unchanged primary lane and clean Double re-entry,
the sympathetic, Mute Pressure (parameter and CC 2) and Strum Spread controls
reaching the rendered audio, offscreen editor rendering including the live
fretboard's bounds, and prepare/release cycles at three rates.
Black-box artifact tests additionally load the exact built VST3 bundle, and on
macOS the exact AU binary, rather than linking the processor class or searching
installed plug-in directories. They pin bundle identity, zero-input stereo
instrument buses, MIDI rendering, the 28 named Electry controls, all four
factory rigs and serialized-state restoration. The ten raw evaluation WAVs are
also read back from disk and must retain a silent lead-in, the intended E1/E2
pitch class, distinct articulations and monotonically shorter Palm tails from
light through hard; mutation fixtures prove that silence, duplicate audio,
wrong pitch and reversed mute depth are rejected.
The engine suite separately pins bounded/repeatable second-player pick timing,
one offset per chord, strum composition and pre-contact cancellation.

The amplifier chain has its own suite: the shipping rapid-Mute body's current
30-80 ms medians are 7.5623% upper-band share and 0.897633 harmonicity, above
the 6% body floor while remaining nonperiodic; halfband unity DC gain, the -6 dB
halfband symmetry point, a dense passband sweep below 0.01 dB deviation and a
dense stopband sweep above 70 dB rejection; the diode
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
same-performance Drop-E level bound; 48 alias probes covering the pedal, every
amplifier, stacked gain and a quiet input at both 44.1 and 48 kHz and on both
sides of the 96/192/384 kHz oversampling transitions, each below -70 dB;
direct A/B circuit-state checks that
plate-plus-screen demand rises under a loud hold and recovers during silence,
plus the established full-path supply droop/recovery probe; an output transformer whose distortion falls about
46 dB per decade of frequency and 41 dB for 24 dB of level, measured at the
stage rather than through the cabinet; each Modern cabinet voicing feature
relative to 1 kHz; loudness bounds across every model's whole amp travel and every
combination of the gain and compressor controls on a rendered Drop-E rhythm
figure, dry and palm muted; 48/384 kHz response agreement for every Amp Voice;
the exact nonlinear frame-rate map, invariant 40 Hz coupled-phase-inverter
response at 384 and 705.6 kHz and the same invariant through public 48/88.2 kHz
processing;
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

### Mute/Dead body checkpoint

The previous Palm-only finite heel has been retired. User feedback identified its
rendered chugs as squishy and short of body; a same-build waveform, spectrum and
envelope A/B confirmed that the extra 100 ms contact stacked a second transient
loss on top of the already selective steady hand. Removing that layer and moving
Palm's existing modal-excitation factor from 0.74 to 0.85 restores the periodic
low body without adding gain compensation, a parameter or another resonator.
At that Palm-only correction, Open and Dead evaluator WAVs were byte-identical.

At that Palm-body correction, the unnormalised dry evaluator measured:

| default probe | retired finite heel | body correction | change |
| --- | ---: | ---: | ---: |
| E1 30-80 / 0-30 ms body | -5.4608 dB | -4.1961 dB | +1.2647 dB |
| E2 30-80 / 0-30 ms body | -4.8192 dB | -3.2264 dB | +1.5928 dB |
| E1 raw peak | -28.03 dBFS | -26.29 dBFS | +1.74 dB |
| E2 raw peak | -26.70 dBFS | -25.11 dBFS | +1.59 dB |

At that checkpoint, the noise-free engine guard independently read
-4.060/-3.150 dB of E1/E2 body. It also kept the intended selective loss: over
150-500 ms the Palm-minus-Open low/high changes were -10.327/-19.832 dB on E1
and -8.347/-22.812 dB on E2. The early Palm high/low balance remained
4.499/0.385 dB darker than Open. Light, default and hard body remained strictly
ordered, so Mute Tightness was one understandable loose-to-tight performance
axis rather than three samples.

There is one explicit tradeoff. The tracked Guitar-TECHS F2 development proxy
moves from the retired contact's -4.009 dB contraction to -0.644 dB at 44.1 kHz
(-0.644..-0.658 dB across 44.1-192 kHz), farther from the two public player cells
at -6.099/-15.290 dB. Those two conventional-guitar cells establish the
direction of selective loss, not a defensible coefficient. The old local fit was
therefore demoted from a hard target after it conflicted with low-body evidence
and the rendered result; the commissioned E1/E2 TRAIN/HOLDOUT comparison remains
the actual fit gate.

At that checkpoint, the common high-gain rapid-Mute fixture had 8.8260% of its
30-80 ms power above 500 Hz and 0.882806 harmonicity, passing its intentionally
loose body and nonperiodicity rails. The shared-hand transition remained
smooth: Palm -> Open moved the old E1 by +9.8741 dB above 500 Hz and +0.279835
percentage points; Open -> Palm moved it by -7.2850 dB and -0.154347 points.
Adjacent CC2 attack moves were 0.0409/0.0548 dB on E1/E2. The stateful Dead
medians shifted only to
-8.178/-15.086/-23.257 dB with a 4.85398 dB contextual RMSE because the earlier
Palm ring and its live damping phase are different; no Dead coefficient changed.

All demonstrations affected by that correction were regenerated from the same
code. Its checkpoint SHA-256 hashes were
`5988919af56d2ba0f669011709e3468254db8664c9dd783d434918809c5296ed`
for demo 04 and
`20c612d85fd0b81ef4ae24885a7314afbdd47af92586b80c03aa2380ec3065e5`
for demo 16. These demos remain demonstrations, not validation recordings.

### Exact-eight fret-decay direction check

The strongest new low-cost realism lead is measured, per-partial loop loss.
[Vodka, Shapovalova, Ovcharenko and Avdieieva (2026)](https://doi.org/10.3390/vibration9030046)
estimate frequency- and fret-dependent damping from three electric basses and
show why a single two-point loss tilt cannot represent every string and fret.
Their E strings trend toward less damping higher up the fretboard, their G and
D strings trend toward more, and their A-string direction depends on the
instrument. The paper is CC BY 4.0, but its linked
[data/code repository](https://github.com/a-vodka/stingsoundgenerator) has no
licence; neither its code nor recordings enter Electry. Bass coefficients are
not transferred to an eight-string guitar.

The lawful exact-eight direction check uses the thirteen public high-quality
previews in
[cabled_mess's CC0 F#-string pack](https://freesound.org/people/cabled_mess/packs/29585/).
The author identifies one clean/dry lowest-string stroke at each fret 0-12,
recorded through an RME Babyface into Cubase 10.5. Each page identifies its
login-only original as a mono 96 kHz, 24-bit WAV. The unauthenticated previews
fetched on 2026-08-29 are mono 48 kHz MP3s; no third-party audio is committed.
Their IDs, in fret order, and SHA-256 receipts are:

```text
fret  0  525010  bba14e50cf747a9e17426db0d9e378f5f24f8ddae8ed1f9772dd1fb830d8ac09
fret  1  525013  c9f182f15d005ee0590e74db66d71be1edc7b4ff0657b2b4e63011092e3c972d
fret  2  525011  d1e7638e2462014f4e0ad927c2a1f64f878b4238f0a7db5a4543b97325cacd6b
fret  3  525003  8211ea474dd8db1bf537234dc4fafae51b1c291f67ba19b31073f485979c5953
fret  4  525004  4f2227ed70bf3208f29285ebf2b3ce04a705ed3a4937545c1e69e253320d6ebc
fret  5  525002  41999dea30b450c737131c1e6b22c00d40da69907678d5db414fc7e7d2659cd2
fret  6  525008  2cc521f211871371a75bcc17030ff8e9bbf9c134dae7cd6b14c04c154bd5f346
fret  7  525001  18cd049b1b9e0b6f3693d808094bdb8e469c750018c123b53c2bfbe012fed773
fret  8  525006  ec558d3830448600157d8214ca90f193f8c1b9cf31a64ad548b3d1f5987ba472
fret  9  525007  f92ea2b0ec9d4830ead8010a324654f7f0a3da968453a520a12fe40197afc7fe
fret 10  525005  7fad5b916ab291fcaa25499b67d21b84d269b780c7c455bc19aed244e0508ad6
fret 11  525012  a1d947ab03d148d64cedaea7a3b41796d1a6dbacca454c5bb6d70f30a205e6a7
fret 12  525009  541754d503d91831e6ba0fdd739a0452566374605b1c32eaca48f6b69029f34c
```

Every preview URL follows
`https://cdn.freesound.org/previews/525/<ID>_5450487-hq.mp3`; the individual
sound page is `https://freesound.org/people/cabled_mess/sounds/<ID>/`.
Receipts used FFmpeg/ffprobe 8.1.1. Analysis used Python 3.11.0, NumPy 1.26.4
and SciPy 1.14.1. A centred 2 ms RMS envelope sets onset at 25% of its own
first-second maximum. An 8,192-sample Hann STFT with a 512-sample hop and
32,768-point FFT tracks each expected partial inside +/-0.28 fundamental.
Theil-Sen lines fit 0.20-1.20 seconds; tracks need at least 8 dB of tail
headroom, a median residual under 4 dB and a positive loss under 120 dB/s.
The comparison below is the median amplitude loss of retained H2-H8 tracks.

Shipping Electry at commit `d74a03761fe10070d6470dcff3f4859a1253bd92`
was rendered dry at 48 kHz/256 frames, Bridge/Mono, Sustain/Down, velocity
0.95, with sympathetic coupling, bridge-hand pressure and acoustic return off.
The regression seam forced physical string 8, so F#1-F#2 are model frets 2-14;
the real F# string uses frets 0-12.

| step / note | real loss | Electry loss | Electry - real |
| ---: | ---: | ---: | ---: |
| 0 / F#1 | 23.74 dB/s | 5.15 dB/s | -18.59 dB/s |
| 1 / G1 | 23.51 dB/s | 4.89 dB/s | -18.62 dB/s |
| 2 / G#1 | 19.07 dB/s | 5.30 dB/s | -13.77 dB/s |
| 3 / A1 | 21.29 dB/s | 5.66 dB/s | -15.64 dB/s |
| 4 / A#1 | 26.50 dB/s | 6.09 dB/s | -20.40 dB/s |
| 5 / B1 | 20.04 dB/s | 5.93 dB/s | -14.11 dB/s |
| 6 / C2 | 24.92 dB/s | 6.36 dB/s | -18.55 dB/s |
| 7 / C#2 | 11.58 dB/s | 6.68 dB/s | -4.90 dB/s |
| 8 / D2 | 33.15 dB/s | 7.11 dB/s | -26.04 dB/s |
| 9 / D#2 | 27.52 dB/s | 7.60 dB/s | -19.92 dB/s |
| 10 / E2 | 31.43 dB/s | 7.94 dB/s | -23.50 dB/s |
| 11 / F2 | 38.13 dB/s | 7.65 dB/s | -30.49 dB/s |
| 12 / F#2 | 27.32 dB/s | 8.50 dB/s | -18.82 dB/s |

The real median is 24.92 dB/s and Electry's is 6.36 dB/s. The paired model
minus real median is -18.625 dB/s, with a 5,000-resample whole-fret bootstrap
95% interval of -20.402..-15.639 dB/s. Electry also rises almost perfectly
with pitch (`rho = 0.9835`) where the take series is irregular (`rho =
0.6484`). That is strong directional evidence that this slice of Electry's
upper partials ring too long and too smoothly. It is not a calibration target:
there is one MP3 stroke per fret, with unknown gauge, pick, pickup and player,
and the instruments use different actual frets.

The irregularity should not automatically be smoothed away. [Le Carrou, Pate
and Chomette's operational-modal study of a played electric
guitar](https://doi.org/10.1121/1.5130894) finds that string-modal damping is
driven mainly by the real part of the neck driving-point admittance: a string
partial near a structural mode can be selectively shortened, while the
player's grip damps the structure, lowers that conductance and can lengthen the
string decay near a dead spot. Contact point and structural mode interact, so
this supplies a physical explanation for local fret/partial departures but no
coefficient that transfers to Electry's eight-string. A later low-CPU
experiment should measure complex neck admittance with the commissioned guitar
held in playing position, evaluate its sparse modal loss offline, and compress
that response into the same minimum-phase loop filter. The public F# previews
contain neither the impedance measurement nor repeated held/free conditions,
so no player-conditioned loss is added here.

#### Measured two-axis fret-termination plan

The strongest actionable follow-on is now directly measured rather than a
smoother version of the Gaussian dead-spot guess. [Yudasaka, Scavone, Rau and
Ishizaka (2025)](https://doi.org/10.1121/2.0002142) measured 24 termination
locations—all 22 frets, the nut and the bridge—in vertical, horizontal and
cross-axis configurations, five times per location. Their
[author manuscript](https://caml.music.mcgill.ca/lib/exe/fetch.php?media=publications%3Ayudasaka_ismra2025.pdf)
reports vertical-admittance changes exceeding 40 dB and neck-side admittance
equal to or greater than bridge admittance. Importing the measured
fret-specific reflection functions into a coupled digital waveguide reproduced
the measured locations of partial-decay changes. In the demonstrated 175 Hz,
fret-1/fret-13 comparison, omitting neck reflection made the synthesized decay
rates nearly identical. Above 700 Hz the real string decayed faster than the
model, plausibly because its capo added a soft termination, and the authors
explicitly leave human finger/neck contact for future work.

That result supplies a plausible, testable mechanism for the lawful exact-eight
mismatch above: Electry's upper partials decay too slowly and too smoothly
while its current `deadSpotFactor()` is one frequency-independent Gaussian per
string. It does not supply transferable coefficients. The production
experiment therefore starts with the actual target eight-string in playing
orientation. An automated impact hammer and two-axis LDV measure the same 24
locations in both direct axes and both cross directions, with at least five
accepted repeats in
free, repeatable capo and normal-grip conditions. Repeatable wire-break plucks
then record string and bridge motion with the magnetic pickup removed; a
separate registered-height installation records synchronized DI for pickup-path
validation. Gauge, tension, fret action, hand force, support and temperature
are frozen with the audio and calibration receipts.

Offline fitting compares the present scalar loss with passive two- and
four-mode 2×2 termination-admittance candidates; modal frequencies remain
shared while fret-specific residues vary. At runtime that admittance is
converted to a reflection matrix using the active string's characteristic
impedance and live tension. The existing intrinsic termination loss is jointly
refitted or subtracted so the measured neck loss is not counted twice.
Stability and contractivity are verified for the complete matrix over
frequency, supported tensions and sample rates. A cross-axis section is
admitted only if it improves the alternating-fret model-selection folds beyond
the diagonal model; the complete measurement-repeat holdout and later remount
session remain unopened for final validation. The candidate reuses Electry's
two existing polarisation loops and updates coefficients only when the physical
fret coordinate or quantized live tension changes, at the existing control
rate and never per sample. It replaces rather than stacks the Gaussian loss.
Model order and fret interpolation are chosen with alternating frets hidden.
Promotion requires at least 50% lower held-out H1-H12 median decay-rate error
than shipping, no worse maximum error, improved polarisation-beating error, stable
44.1-384 kHz stress renders and less than 2% eight-string CPU overhead at a
96 kHz host. Only then may all frets be refitted for a default-off listening
candidate.

#### Ranked measurement-backed follow-ons

1. Capture-fit the existing wound-string dispersion cascade. [Murray and
   Whitfield (2022)](https://doi.org/10.1119/5.0064373) found good agreement
   between theoretical and measured inharmonicity for monofilament strings but
   poor agreement for wound guitar strings. `Tools/FitDispersion.py` now
   provides a non-promotable calibration gate for supplied H1-H12 frequencies:
   it fixes one joint `(f1, B_take)` observation per take, fits Electry's Drop-E
   open-string `B` from even-fret TRAIN H2-H12 only, and scores odd-fret HOLDOUT
   under the same fixed-`f1` constraint. Captured F# fret `r` maps to model fret
   `r + 2`;
   the fit changes no source and adds no render-loop work. Original-WAVE hashes
   and reviewed commercial-calibration rights fail closed, but the tool does
   not extract partials and therefore cannot prove that supplied measurements
   came from those bytes. Final promotion still needs a separate installed
   exact-eight string-set holdout, at least 50% lower median cents error, no
   worse p95 or maximum error, and rendered confirmation that the production
   allpass approximation preserves fundamental tuning.
2. Measure the target pickup as a genuinely bivariate transducer. [Novak et
   al. (2020)](https://doi.org/10.17743/jaes.2020.0002) measured distinct
   two-dimensional nonlinear functions for single-coil, pole-piece humbucker
   and rail-humbucker geometries. The first candidate is only a compact fitted
   `flux(vertical, horizontal)` map in place of Electry's scalar mixed-axis
   polynomial. [Giampiccolo, Bernardini and Sarti
   (2023)](https://re.public.polimi.it/handle/11311/1236166)
   show an explicit, non-iterative wave-digital pickup/tone/volume/cable/load
   circuit, but that larger replacement is considered only if impedance and
   clean-DI measurements first falsify Electry's current loaded circuit. The
   bivariate stage must halve held-out normalized complex H1-H5 error across
   registered heights, amplitudes and elliptical trajectories, with no worse
   worst-case condition and below 1% CPU overhead.
3. Keep transverse/longitudinal coupling exploratory. [Lee et al.'s
   differentiable planar-string model](https://papers.nips.cc/paper_files/paper/2024/hash/0232cafe8d1909a01019abe8af32f3e1-Abstract-Conference.html)
   and [Bank and Sujbert's reduced longitudinal-resonator
   method](https://www.dafx.de/paper-archive/2004/P_089.PDF) offer an offline
   nonlinear teacher and a compact resonator reduction for pitch glide and
   phantom partials, but neither establishes an exact-eight residual. No
   runtime candidate is permitted until repeated three-axis motion and
   synchronized clean-DI captures show level-dependent longitudinal motion,
   pitch glide or phantom components above noise, and a coupled model improves
   unopened captures beyond the fixed dispersion/polarisation/pickup null.

Most importantly, fret and frequency are confounded. A robust fit of loss to
log partial frequency plus fret gives a direct fret coefficient of +0.1348
dB/s/fret for the real files and +0.0207 for Electry; their difference is
-0.1141 with a paired bootstrap interval of -0.8026..+0.6117. The data do not
identify a shipping fret coefficient, and the bass paper's opposite E-string
direction reinforces that boundary.

The CPU-bounded experiment now exists behind the default-off
`ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2` build flag. It follows the
low-order, decay-time-weighted direction of
[Bank and Välimäki](https://research.aalto.fi/en/publications/robust-loss-filter-design-for-digital-waveguide-synthesis-of-stri/),
but makes only the narrow claim this public series supports: one fixed-Hz,
second-order RBJ loss dip is added to both played polarisations of physical
string 8 for F#1-F#2 (MIDI 30-42). It is minimum phase, never exceeds unity
magnitude, and its phase is included in the fundamental delay compensation.
Open E1, every other string, notes outside that range and all sympathetic opens
are exact bypasses.

The fit used every retained H1-H12 row, not the H2-H8 median headline above.
At 48 kHz, SciPy soft-L1 (`f_scale=1`) minimised
`(shipping + correction - real) / sqrt(rows in fret)` on even frets. The
frozen result is a 441.418112 Hz centre, Q 0.40618486 and 22.9327503 dB/s peak;
the loop depth is `22.9327503 / f0` dB per round trip. Coefficients were frozen
before any odd-fret result was read. The local 149-row JSON and CSV measurement
receipts have SHA-256
`e9b790b503221bdb23b575a491046683057d09b972cf278f242016df2ba2e4aa`
and `a010206ac1effd4461d55d514d65182e88e0790d1315f37bc361c52896cf8696`.
An order-four target's small, uncertain holdout advantage did not justify a
second realtime section.

The candidate was then rendered through the real mixed-polarisation engine,
not its fitting equation. A frozen real-track mask kept all 149 comparisons
fixed so changing decay could not change which model partials were admitted.
On the six untouched odd frets, equal-fret MAE fell from 16.292 to 8.533 dB/s
and row RMSE from 19.314 to 11.200 dB/s. A 5,000-resample whole-fret bootstrap
put the median MAE change at -7.97 dB/s, 95% interval -12.70..-0.74 and
`P(change >= 0) = 0.008`. Five of six frets improved; fret 7 worsened from
4.962 to 13.836 dB/s, so this is an aggregate result, not universal parity.
With the flag off, all thirteen rendered WAVs remain byte-identical to the
frozen shipping baseline.

The active-F# cost is one double biquad per polarisation on one voice. In 24
alternating 96 kHz, 512-frame, eight-note runs, OFF and ON used 0.12325x and
0.12239x realtime at their independent medians; paired overhead averaged 0.69%
inside a -5.05..+3.77% clock-noise range, and the worst ON run was 0.12634x.
CPU is not the promotion blocker. The two evidence boundaries now follow the
same continuous fret coordinate as Hammer and Slide: a one-fret C1 smoothstep
crossfades loss between frets 1/2 and 14/15, retains the exact source section
until the finger actually moves, and lands on exact full-correction and bypass
endpoints. Each tick rebuilds the passive physical filter rather than
interpolating coefficients, retains the two independent filter states, and
translates both raw delays for the changed filter phase. Legacy and MPE bends
do not move the evidence gate.

The boundary regression covers both directions at both ends for Hammer and
Slide, chained reversal, finite output, monotone loss, fractional passivity,
legacy/MPE bends and both-polarisation stationary-pitch error below 0.25 cent
at 44.1, 48, 96, 192 and 384 kHz. All thirteen settled ON renders remain
byte-identical to the frozen candidate and all thirteen OFF renders remain
byte-identical to shipping. An intentionally continuous, alternating MIDI
29/30 boundary-Slide microbenchmark at 96 kHz and eight-sample process quanta
used 0.04459x OFF and 0.04750x ON at the six-run medians: 0.00291x realtime,
or 0.29 percentage points of one core, for coefficient motion on every control
tick. The flag therefore remains off only pending repeated licensed exact-eight
captures and phrase-level blind listening. No harmonic bank, neural network or
FDTD solver enters the realtime path.

### Conventional-E2 post-fit falsification check

After the coefficients and odd-fret result above were frozen, a no-refit
adjacent-domain audit compared the candidate with all five EGFxSet clean
`6-0.wav` open-E2 takes. This asks whether the fitted loss can transfer merely
because another string has the same pitch; it is not another calibration set.
The files were recovered from the 431,036,829-byte Zenodo archive with five
single-range requests totalling 3,428,190 bytes, then checked against both the
ZIP CRCs and mirdata MD5 values. The source receipt SHA-256 is
`edca2335844399ba54ea7c8635a2a0639399daf114325e1cc21322757d849e2a`;
no third-party audio is committed.

Before rendering or reading a decay result, the audit froze the existing five
real files, the string-8 fret-12 OFF and ON E2 renders, a fresh shipping-OFF
renderer for physical string 3 open E2, and the exact estimator used above.
The preregistration and final eight-input manifest have SHA-256
`85695336c6671cc16883dcd0e89373b9017a48f3a0c763181ee0145a7db3b72d`
and `9bf8e2ef53f986e8f3db3d6222db88b444a577da59f257896ac97f42f34dcea3`.
The fresh role-matched WAV is MIDI 40 on physical string index 2, open fret,
velocity 0.95, 48 kHz/256 frames, Mono/default build with sympathy, bridge hand
and acoustic return disabled; its SHA-256 is
`9005acb7949786116014b87fdb9f13feabc575b367a2d8de0b892580475ae993`.

The frozen H2-H8 gates were intersected across all five real takes and all
three model renders. Exactly H4-H7 survived, meeting the predeclared four-track
minimum without relaxing a threshold. A first attempt stopped before onset or
spectral analysis because SciPy could not decode the EGFxSet PCM24 container.
The disclosed decoder-only amendment, SHA-256
`0b89834deb970e947894d08e5d2b818e1a3d563676f30f825f2283d79be9e055`,
added a signed little-endian PCM24 reader; every numerical rule remained
unchanged.

| common H4-H7 comparison | median loss | median signed error | MAE | RMSE |
| --- | ---: | ---: | ---: | ---: |
| five real pickup takes | 4.586 dB/s (3.701..7.696) | - | - | - |
| string 8 / fret 12, candidate OFF | 7.105 dB/s | -0.055 dB/s | 3.808 dB/s | 4.496 dB/s |
| string 8 / fret 12, candidate ON | 30.783 dB/s | +23.193 dB/s | 23.917 dB/s | 24.405 dB/s |
| string 3 / open, shipping OFF | 7.816 dB/s | +0.630 dB/s | 3.935 dB/s | 4.728 dB/s |

Errors aggregate the five pickups by four common partials. Turning the
candidate on worsens MAE for every pickup by 16.974..21.542 dB/s and aggregate
MAE by 20.109 dB/s. The result rejects pitch-only transfer of the exact-eight
F# correction to a conventional Strat low E2 and reinforces keeping the flag
off. It does not refute the frozen exact-eight F# holdout: string construction,
scale position and fret geometry differ, while the EGFxSet pickup files are
separate normalised plucks. The OFF result is directionally much closer, not a
claim that either mapping is capture-fitted. The intersection only meets its
minimum, so it says nothing about H2, H3 or H8. Its 0.20-1.20-second slopes end
before the dataset's forced fade at 3 seconds, although that fade makes the
final-frame headroom gate permissive for the real files.

The read-only final receipt and result have SHA-256
`eb740fadf6fd7499f2703cd5f3e335b28e13ea791af0b2fc08ad46f690eacc6c`
and `d5b10c339563f1511365bcf68885013e90a4c9b55ac497aeead8022402fef84f`;
the partial and comparison CSV hashes are
`43321623cdbc174fd6259f5278a26e646b8bcce35248b98532c31ad93435d7c1`
and `6529050170f35f067803b8c94a0799d727d33d2f96c5579075e21689959e1079`.

### Attack-pitch and resolvable-beating audit

A second no-refit audit asked whether the same lawful recordings expose a
missing pitch trajectory or a resolvable modal doublet. Its protocol and input
manifest were SHA-256 frozen as
`ffa0c7c4d19eca02de0f742f49810a4b8b42bec532d64d7ca36833578cc5a27e`
and `0539cc3eb280ef04ecacc26b249933b88caac25eff724802179e5773a365e506`
before any sample-derived pitch, phase, envelope or modulation result was read.
The same 13 exact-eight previews were paired by fret with shipping-OFF and
loss-candidate-ON renders; the five EGFxSet open-E2 DIs and shipping string-6
open-E2 role control remained descriptive robustness checks.

After a 2 ms RMS onset, the frozen estimator tracked H1-H6 independently from
unwrapped analytic phase, using an 80 ms phase line every 10 ms. Each partial
was expressed relative to its own 0.75-1.20 s reference before a quality-gated
cross-partial median formed the 60-180 ms attack excess and 80-500 ms Theil-Sen
relaxation slope. A detection required at least +2 cents at attack, at most
-3 cents/s thereafter, a near-zero late residual and fixed frame/dispersion
coverage. This avoids treating a changing broadband spectral centroid as
pitch, although it still cannot isolate the physical cause.

| cohort | sufficient / files | detections | median attack excess | median slope | frozen support |
| --- | ---: | ---: | ---: | ---: | ---: |
| exact-eight real | 5 / 13 | 5 | +6.218 c | -7.670 c/s | no: coverage below 10 / 13 |
| Electry shipping OFF | 13 / 13 | 0 | +0.007 c | +0.003 c/s | no |
| loss candidate ON | 13 / 13 | 0 | +0.004 c | -0.007 c/s | no |
| EGFxSet real E2 | 5 / 5 | 5 | +7.550 c | -14.282 c/s | yes |

All 13 exact-eight previews had a positive descriptive attack excess and a
negative slope, including the eight excluded from the formal cohort. That sign
agreement and the independent EGFxSet pass make Electry's stationary attack a
high-priority realism discrepancy, but the strict cross-dataset result remains
false. On the five sufficient exact-eight triplets, candidate attack MAE was
8.594 cents against shipping's 8.608 cents and therefore failed the frozen
at-least-1-cent and 20% improvement gate. The loss experiment neither fixes nor
materially changes pitch motion.

The companion detector required coherent 1-12 Hz periodic envelope and pitch
motion in at least two partials, with at least three observable cycles. No
exact-eight real/OFF/ON file passed and only one of five EGFxSet takes passed,
below its 4/5 gate. Median best-case H6 resolution floors were 8.712 cents for
the short exact-eight previews, 6.150 cents for their model renders and 4.879
cents for EGFxSet. Thus no fast modal doublet is supported, while Electry's
slow sub-cent polarisation beating is explicitly below this audit's resolution
and is not rejected.

The byte-deterministic rerun produced an identical result. Its read-only result
and final receipt have SHA-256
`e836d01e7c5ad96aa45c211916bbb60df11aa930474b284c2be24b02b2c2bc0b`
and `3b196778284f2d7d39c6b3743fe2c96d29a739fba3c4263ea0481f5d0b4e00c7`;
no third-party audio is committed. These unmatched recordings support testing
a bounded amplitude-dependent attack-pitch mechanism, not choosing tension,
fret/contact motion or pickup-magnet back-action, fitting a coefficient, or
changing the shipping path.

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
not a fit set.

The expanded lawful search adds useful controls but no missing extended-range
gate. [EGFxSet](https://egfxset.github.io/) and its
[CC-BY-4.0 Zenodo deposit](https://zenodo.org/records/7044411) provide 690
five-second clean-DI captures: all 138 string/fret positions of a standard
six-string Strat at five pickup settings, made with one 2.0 mm pick and one
documented instrument-input chain. They are a strong adjacent-fret and
conventional-E2 sustain check across separately played pickup settings, but
their normalisation precludes absolute-level fitting and they contain no Palm
or Dead notes.
[GuitarJam](https://huggingface.co/datasets/Julian-br/GuitarJam/blob/main/README.md)
adds 580 CC0 raw-DI monophonic phrases for timing, dynamics and incidental-noise
listening, but has no string, fret, tuning or technique labels. Neither source
can promote a Drop-E/F# coefficient.

The formerly unavailable EG-IPT lead is now a published
[CC-BY-4.0 Zenodo dataset](https://doi.org/10.5281/zenodo.15205644). The main
corpus contains 52,320 audio files (28 h 22 min 56 s) recorded at 96 kHz/24 bit
from a six-string 2005 Gibson SG Standard: three humbucker configurations and
six simultaneous sources, including a BSS AR 133 DI through a Midas XL48. The
paper's 1,038-file Strandberg seven-string corpus is a separate generalisation
test set, not the deposited EG-IPT archive; conflating the two had made this
source look both smaller and unavailable.

A range-only audit of the 23,757,983,313-byte archive selected the literal
sixth-string note-identifier `02` DI under `ordinario`, `muted` and `snap-pizz`.
Eight files match. The HB-bridge snap-pizz technique is not absent: it has 23
sixth-string files under a different, unexplained identifier sequence beginning
at `03`, so none was guessed into the frozen `02` subset. ZIP CRC and SHA-256
checks passed after 36 HTTP range requests totalling 30,512,346 bytes, or
0.1284% of the archive. The read-only source receipt and follow-up filename
forensics have SHA-256
`1a60c1c1b4cffecd90f5c47d5f180bda92716f9653cfd84fe46573dac964a5f1`
and `31dd194e792f2ae38d23d8ef7fd249d23e46ded951053ea5f5709897a6395724`;
no third-party audio is committed.

The paper defines `_6s` as string 6 but calls `02` only a unique note
identifier; it specifies neither the numeric grammar nor the guitar tuning.
The common 23-identifier lattice is compatible with open plus frets 1-22, but
does not prove that order or standard-tuned open E2. Released-WAV normalization
is also unstated. Pitch must therefore establish note identity before analysis,
and even a confirmed E2 remains a controlled conventional six-string
technique/pickup check rather than licensed Drop-E calibration data.

#### Frozen EG-IPT conventional-E2 result

The partial-frequency protocol was SHA-256 frozen as
`09fb828a5f8d4326227f24061f63a610e9c6dfb42fb596671f44aff5a254324c`
before any waveform sample was decoded. Its synthetic suite rejected silence,
clipping, a neighbouring note and a static-inharmonicity/unequal-decay false
positive, while detecting a shared H1-H6 glide. The analyzer and exact 23-file
code freeze have SHA-256
`fc03d69007edfb99a1f74b1a9fa58019fa44e169e6d8b811683cb6e4be025c17`
and
`cc5eed5fe93c7c7bc00e778fca995f76aa1054692054e5f5b0595ef2c8c28a76`.
No threshold, file, window, mapping or model control changed after that freeze.

All three `ordinario` files pass the note and coverage rails. Their late H1
references are 82.318251, 82.973623 and 82.525740 Hz (-1.863, +11.865 and
+2.495 cents from E2), establishing the E2-like endpoint for this stratum
without claiming that identifier `02` means open E2 throughout the corpus.
The frozen result is:

| source / pickup | aggregate attack | aggregate slope | H1 attack | H1 slope | coherent |
| --- | ---: | ---: | ---: | ---: | ---: |
| EG-IPT / bridge | +2.368 c | -9.596 c/s | +2.686 c | -11.053 c/s | yes, H1-H6 |
| EG-IPT / couple | +2.110 c | -9.097 c/s | +2.199 c | -10.036 c/s | yes, H1-H4 |
| EG-IPT / neck | +1.045 c | -4.970 c/s | +1.198 c | -5.545 c/s | no; below the 2-cent/four-partial rails |
| Electry / Bridge | +0.0049 c | -0.0193 c/s | +0.0090 c | +0.0033 c/s | no |
| Electry / Both | +0.0048 c | -0.0194 c/s | +0.0091 c | +0.0282 c/s | no |
| Electry / Neck | +0.0060 c | -0.0265 c/s | +0.0168 c | +0.0129 c/s | no |

Thus `ordinario` passes its preregistered real-trajectory gate: 3/3 files are
sufficient, 2/3 share a positive attack offset followed by negative
relaxation, and the cohort median attack magnitude is +2.110 cents. Shipping
Electry is effectively stationary and fails four of five compatibility rails.
Its median/max aggregate-attack errors are 2.105/2.363 cents, median/max slope
errors are 9.078/9.577 cents/s, and median H1-attack error is 2.190 cents; only
the four-cent maximum-attack bound passes.

The negative controls matter. Electry PalmMute shows only a rejected
upper-partial spectral drift: its aggregate attack is about +0.65 cent, while
H1 moves by -0.030..-0.045 cent with a positive slope. All three real `muted`
files decay before the frozen tracker can form the required 300-440 ms late H1
reference, and both `snap-pizz` files lack common partial coverage. Those
strata are inconclusive, not evidence of a wrong note or a stationary attack.

This is one separately played take per pickup/technique cell on a conventional
six-string SG. It supports a bounded, default-off common amplitude-pitch
experiment; it does not identify tension, magnet or contact causality, fit an
exact-eight coefficient, or authorize a shipping change. Two complete runs
produced byte-identical JSON and CSV with SHA-256
`c82e6cb447f27f2fb4258f5b93b42f39cf6cbf733ce1d1373a8f80b57f13e5b2`
and
`74a4806f195780ecee57bef20e76df1b046c1a4547b5834edb8a3cc8f461b866`;
the read-only final receipt is
`2760dd5c9248aca031afc7d1281471e8f2725493c1ea5affd8a20af6e1c7a5cf`.

[GOAT](https://zenodo.org/records/15690894),
[IDMT-SMT-Guitar](https://www.idmt.fraunhofer.de/en/publications/datasets/guitar.html)
and the [pitx muted pack](https://freesound.org/people/pitx/packs/898/) are
noncommercial; [GADA](https://gingerpianist.github.io/GADA_demo_page/) exposes
no dataset licence. They cannot be used to tune a commercial product.

The [cabled_mess profile](https://freesound.org/people/cabled_mess/) offers custom
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

#### Energy-derived attack-pitch research

The first-ranked low-CPU pitch experiment is now a common string-tension glide,
but not a return to the removed force-to-cents envelope. [Lee et al.'s
per-partial measurements](https://www.dafx.de/paper-archive/2009/papers/paper_86.pdf)
show the first 15 partials of one plucked guitar string relaxing along nearly
one normalized exponential trajectory; a softer pluck mainly changed its
starting offset. [Bank's energy formulation](https://dafx.de/paper-archive/2009/papers/paper_76.pdf)
relates the quasistatic tension increase directly to transverse string energy,
`T_qs = E_modulus A E_string / (2 L T0)`, and permits evaluation every 1-10 ms with
linear interpolation. That is one bounded scalar per string, not a nonlinear
solver or per-partial oscillator bank.

The wound-string scaling is crucial. [Kemp's measured string-construction
model](https://doi.org/10.1371/journal.pone.0184803) supports treating winding
tension as negligible, so the elastic term uses the steel core's `E A_core`
while inertia still uses the complete string's linear density. Using the .080
outer diameter as load-bearing area would grossly overstate Drop-E modulation
and repeats the kind of cross-string mismatch that made the earlier prototype
sharpen E1 by about 30 cents but B1/E2 by only 9-13 cents.

Broadband tuner motion is not enough evidence. In [Kemp's .132 bass-string
experiment](https://doi.org/10.1007/s42452-020-2391-2), static inharmonicity and
faster upper-partial decay produced a strong downward broadband pitch trace
while the isolated fundamental and spectrogram did not descend. Any Electry
gate must therefore normalize and track individual partial frequencies, require
the fundamental plus at least three partials to share the trajectory, and leave
its existing static dispersion/loss model responsible for purely apparent
spectral glide.

Electry's existing `outputEnergy` is not reused. Under the compile-time
`ELECTRY_ENERGY_ATTACK_PITCH` flag, an ordinary Sustain plectrum contact
instead latches a physical release candidate. For pluck fraction `p`,
two-axis peak displacement `y` and axis weights `w_v`, `w_h`, it evaluates
`E_string = T y^2 (w_v^2 + w_h^2) / (2 L p (1-p))`, then converts that
energy to the dimensionless tension ratio
`q = E A_core E_string / (2 L T^2)`. Sounding frequency follows Bank's
square-root law, `f / f0 = sqrt(1 + q)`. The winding remains inertial while
only the estimated steel core supplies axial `E A`; the complete static
dispersion fit stays at the written pitch.

The physical energy and elastic scale exist only while that plectrum contact
is pending. On real release they seed a bounded phenomenological `q`; delayed
or interrupted contacts do nothing, repicks retain the larger old/new value
without summing it, legato carries only live `q`, and reset, silence and a
large voice steal close the state explicitly. The common dynamic period is
phase-compensated in both polarisations before their fixed relative split.
`q` is capped at `2^(7/600) - 1`, exactly seven cents, and decays with a
0.3049 s time constant. That time constant is borrowed from Lee et al.'s one
hard-plucked open high-E measurement, not fitted to EG-IPT and not asserted as
an eight-string material constant. The 4.8 N full-force contact anchor and
wound-core fractions are existing heuristic/construction coordinates, not
coefficients fitted after seeing the comparison.

The v2 candidate protocol was frozen before the final candidate audio was
decoded by the analyzer, with SHA-256
`3db89fcbc411ea16d34ba89a0b11b3b15067cd7742053411cb687700f633093e`.
Its wrapper and unchanged estimator core have SHA-256
`10e78e687682541c6a28be68144a85c8073bb2bc27c50a1911b77c9eb4e4737c`
and
`745d31494b5ddca33f092a3d8208a533892c8a1dd2d21fd9263bdc3fc16d5de3`;
the exact 29-file source/audio/executable freeze is
`c50d86d4fea7603bd650b0a96eb1b8d1a21973199e46a93993e8e4c27eb760f1`.
The wrapper rebinds the byte-identical estimator core; no numeric estimator
constant or gate changed.

All six model files were sufficient, all three Sustain renders had a positive
coherent attack trajectory, and every registered absolute-error measure
strictly improved:

| error against the three ordinary EG-IPT cells | shipping | candidate |
| --- | ---: | ---: |
| median aggregate attack | 2.105 c | 0.347 c |
| maximum aggregate attack | 2.363 c | 0.980 c |
| median aggregate slope | 9.078 c/s | 2.072 c/s |
| maximum aggregate slope | 9.577 c/s | 2.569 c/s |
| median H1 attack | 2.190 c | 0.684 c |

The three Palm renders remain byte-identical with the flag on, while all six
flag-off release renders remain byte-identical to the pre-candidate baseline.
Two complete analyzer runs produced identical JSON and CSV with SHA-256
`bce5c0b612ebd2e8dea8b15dc584b6ece62705b300ab79ded1b3d6eeaa7219a8`
and
`23fe3f59f23c3bfc3b4c773b433b9f431e46c380ba5bd340b6b23bc5fad8ba62`.
Both flag-off and flag-on engine and FX suites pass. Nine alternating
ABBA/BAAB rounds at 96 kHz, block 256, all eight strings, Both pickup and
Stereo measured median overhead of 0.397% for Sustain and 0.735% for Palm;
paired-round medians were 0.403% and 0.764%, below the frozen 3% ceiling.
The CPU result has SHA-256
`982c791df5695d54512c4f4ab150b2d54297e27d7a81d2e0cdf62cd64d906f48`.

The default-off candidate is committed at `7807944`; its read-only final
receipt has SHA-256
`1b826e0b5683d7c035cb5bbf8a7e2caea7b0f0470eadcdb8a225f82169071144`.
This is a descriptive compatibility pass against one separately played take
per pickup on a conventional six-string SG. Muted and snap-pizzicato cells
remain inconclusive. The result does not identify the physical cause, provide
matched multi-velocity exact-eight calibration or holdout evidence, include a
user blind verdict, authorize enabling the flag, or establish market
superiority.

#### Phase-conditioned repick research

The next-ranked metal-specific mechanism is the plectrum touching a string
that is already moving. Electry correctly keeps the ringing loop through a
same-string repick, but its short Contact phase still applies one scalar loss
to both loop polarisations at the commuted seam. It is not a local contact at
the pick position. The existing
[repick audit](Docs/evaluation.md#repick-contact-mechanism-audit) found that
this loss changes the full E1/E2 state by only about -0.002/-0.004 dB and also
proved an integer-position reciprocal two-port reference. A stronger local
junction failed the public rapid-mute proxy, so topology alone is not a licence
to tune it.

[Evangelista and Smith's passive pluck junction](https://www.dafx.de/paper-archive/2010/DAFx10/EvangelistaSmith_DAFx10_P21.pdf)
supplies the useful integer-position, low-cost structure. Electry's derived
fractional extension uses a signed interpolation vector `w`, loop state `x`
and dimensionless contact amount `0 <= gamma <= 1`. Its normalized adjoint
gather/scatter
`c = w^T x; x' = x - gamma w c / (w^T w)` changes squared state energy by
`-gamma (2 - gamma) c^2 / (w^T w)`, which cannot be positive. This proves the
delay-line Euclidean norm only, not passivity of moving delays and filter
states in the full engine. The extension reduces to the tested two-port at an
integer pick position, requires no allocation or filter bank and runs only
during the existing 0.55-3 ms Contact phase. The current whole-loop
`contactRetention` is not a measured local reflectance and must not be reused
as `gamma`. Repeated application also makes a raw per-sample `gamma` depend on
contact duration and sample rate, so any later experiment must freeze it as a
one-shot total amount or convert a measured rate to per-step amounts.

The physical evidence explains why the missing coefficient matters.
[Pluta, Tokarczyk and Wiciak's robotic re-excitation](https://www.mdpi.com/2076-3417/12/3/1659)
shows that simply adding a fresh initial condition misses pre-release damping,
ringing and lasting spectral change, while a moving one-sided boundary follows
their measured direction more closely. [Brauchler et al.'s measured string and
plectrum motion](https://acta-acustica.edpsciences.org/articles/aacus/pdf/2020/03/aacus200019.pdf)
shows that detachment phase contributes both displacement and velocity to the
two-axis release. A newer robotic
[192-micrometre depth sweep](https://arxiv.org/abs/2606.24356) finds shallow
plateaus or thresholds in several amplitude and spectral features whose depth
depends on plectrum material, but its acoustic six-string rig and remounted
plectra prevent transfer of an absolute depth to Electry.

The first falsification gate is therefore a separate commissioned 96 kHz
exact-eight session, not another conventional-guitar fit or a mutation of the
validated 44.1 kHz Palm/Dead capture contract. On one fixed measured rig it
records E1 and E2 at one hard intended force as 64 independent two-stroke pairs
per string. Four cue phases receive sixteen randomized pairs each; every phase
balances eight Down and eight Up second strokes, with the opposite first stroke.
Exactly three disconnected TRAIN and two untouched HOLDOUT performance
clusters use that same rig; other rigs are external-validity tests, not mixed
into its coefficient fit. The operational
[`electry-repick-phase/v1` capture contract](Docs/capture/electry-repick-phase-v1/README.md)
freezes the file, sensor, schedule and rights requirements.

The cue phase is only scheduling metadata. A fixed nonmagnetic felt fixture
damps the seven non-target strings while leaving E1 or E2 untouched. A
synchronized contact-onset channel and nonmagnetic two-axis LDV or equivalent
motion reference measure the actual state. The frozen primary phase is
plectrum-normal H1 displacement at contact,
with zero fixed at its positive displacement maximum (positive-real phasor) and
four fixed +/-45-degree quadrants; it is never rotated after decoding.
Tangential H1 phase remains a
recorded covariate. Pickup/gap, gauge, pick geometry, grip and pick point stay
fixed or measured. A magnetic DI is already spatially filtered; the
[electric-guitar measurement comparison](https://pmc.ncbi.nlm.nih.gov/articles/PMC12609871/)
also warns that microphones do not recover an exact string waveform and that
an LDV target can mass-load a thin string.

Before the first scored pair, the protocol, sensor-latency calibration and
analyzer are frozen. After acquisition but before any TRAIN bridge-DI response
is decoded, a first receipt binds all five completed manifests, raw hashes,
blinded integrity records, equations, fixtures, fixed quadrants, shipping
renderer/build/preset/seed and the TRAIN/HOLDOUT split. Its one primary response
is bridge-DI H1-H4 energy gain from joint 48 ms
Hann-weighted fits in `[-55, -7) ms` and `[+7, +55) ms` around measured
contact; 0-12 ms remains a
broadband attack window, not an identifiable E1 complex-partial estimate. Each
string/direction/quadrant must retain at least three eligible pairs. The real
phase effect must first resolve on both strings and directions in all three
TRAIN clusters with mutually aligned centred profiles. Only then may one
default-off fractional junction derive its sample-rate-independent contact
amount entirely on TRAIN. A second receipt freezes that candidate and both
still-unopened HOLDOUT manifests. Exact-once HOLDOUT decoding must replicate
the registered effect and aligned profiles. Promotion additionally requires
at least 50% lower
median absolute real/model cell error in each HOLDOUT with no worse maximum
cell, plus identity, energy, lifecycle, level, tuning and stability rails and
at most 1% median overhead at 96 kHz and 3% worst case at 384 kHz. The permissible
result is narrower than “realistic plectrum”: more accurate phase-dependent
open E1/E2 repicks on the captured rig and protocol.

#### Pickup-magnet back-action research

The second-ranked low-CPU direction is the force a pickup magnet applies back
to the string, not another pickup-output waveshaper. Electry already reads two
transverse polarisations through a nonlinear magnetic sensor and gives them a
small fixed split, but its pickup cannot change their restoring stiffness.
[Feinberg and Yang's measured pickup-like field](https://doi.org/10.1121/1.5080465)
changed that stiffness differently in the two axes, split their frequencies
and produced beating; their parameter-free model matched the experiment and
put a commercial-magnet effect near the order of one hertz. The later
[two-axis force/displacement measurements](https://doi.org/10.1121/10.0027283)
show that the split depends on vibration amplitude and magnetic force, so it
can evolve through the note rather than acting as one static detune. The
[official Yamaha/CIRMMT project](https://www.cirmmt.org/en/research/projects/yamaha-rnd_guitar-analysis)
establishes the programme's provenance, and its 2025
[ISMRA abstract](https://ismra2025.org/wp-content/uploads/2025/05/ISMRA-2025-Final-1.3.pdf)
reports pickup-height measurements and a single-recursion realtime model.

The smallest defensible Electry experiment is static, diagonal and first-mode,
not an invented amplitude law. Its first-order Rayleigh reduction evaluates
the signed stiffness `k_a,p = -dF_a/du_a` at static equilibrium. For axis `a`,
sounding length `L`, tension `T` and pickup-row positions `x_p`, use
`r_a = 2 L sum(k_a,p sin^2(pi x_p/L)) / (T pi^2)` and
`f_a = f0 sqrt(1 + r_a)`. Cache the two mode-shape sums at note/pitch geometry
changes, evaluate each axis's existing phase compensation at its corrected
frequency, and feed the two delay targets through their current smoothing.
Stationary notes add no render-loop work; 0.25% of the physical-string engine
is the preregistered CPU ceiling, not a promised result. Summing point rows is
an approximation too and requires target measurements to verify additivity.

The mechanical correction must remain active for both physical magnets even
when the selector culls one electrical pickup path, must not relabel or remove
Electry's existing small polarisation split, and must not add damping. A string
centered over a suitably symmetric field makes the cross-axis terms zero; an
off-centre target would require the measured symmetric 2x2 stiffness matrix.
The necessary scalar-modal condition is `1 + r_a > 0`; the matrix form requires
every eigenvalue of `I + R` to be positive, and neither condition proves
moving-delay passivity. Harazono et al. report negative-stiffness magnitudes
0.8/1.3 N/m for one two-row specimen. Mapping those to Electry's signed
`k = -0.8/-1.3 N/m` convention produces a useful -1.752574-cent arithmetic
fixture in this reduction, but does not calibrate Electry.

No published coefficient transfers to a high-output eight-string humbucker or
its wound .080 string. Promotion requires force-versus-displacement grids in
both axes for the actual pickups, per-string gaps at open and high frets,
synchronised two-axis motion plus dry DI over several velocities, and a
magnet-removed or very-low-pickup control. The evidence supports evolving
stiffness, polarisation split and beating; it does not justify inventing extra
sustain loss. Only after those curves exist should a control-rate amplitude
backbone replace the static first mode; Electry's summed retirement follower
cannot supply it because it loses axis identity.

A third, cheaper measured residual is pickup-localised partial warping.
[Harazono et al.](https://www.jstage.jst.go.jp/article/ast/33/5/33_E1148/_article)
model two humbucker poles as local negative stiffness and reproduce long-period
waveform/envelope motion with pickup-specific fitted values. Rather than add a
modal bank, a future offline solve can project a target pickup-height residual
onto Electry's existing dispersion/allpass fit at note start. No published pole
stiffness transfers to Electry's pickup, and longitudinal/phantom resonators
remain below priority until repeatable clean magnetic-DI sum/difference
partials survive a target high-gain chain.

#### Setup-conditioned fretted-intonation research

A premium fretted guitar is close to equal temperament, not mathematically
identical to it. Pressing a real string changes its total length, tension,
linear density and stiffness while nut/saddle compensation only reduces the
residual. [Varieschi and Gower](https://arxiv.org/abs/0906.0127) derive that
coupled geometry and measured one compensated steel sonometer string from
-7.9 to +7.3 cents across its frets. [Anderson and
Beausoleil](https://doi.org/10.1121/10.0026483) separately decompose classical-
guitar intonation into resonant length, density, tension and bending stiffness,
with [open model and experiment data](https://github.com/beausol/classical-guitar).
Those results establish the mechanism; neither instrument supplies coefficients
for Electry's preferred 27.625-inch/.098 extended-range anchor or its
28-inch/.080 endpoint.

Electry currently commands exact nominal 12-TET note targets and ideal
`L = L0 2^(-fret/12)` speaking lengths. Stiffness dispersion therefore bends
partials without giving a fretted fundamental the deterministic residual of one
installed setup. This missing mechanism is not random detune. The first
identification experiment deliberately freezes one real guitar, string set and
setup; it does not infer a continuous Guitar Build/gauge law from that one
instrument. An Anderson-style low-dimensional static model is evaluated offline
into one 8-by-23 table of fundamental cents for each independently measured
setup: 736 bytes per table as floats, or about 4.3 KiB for all six curated build
anchors. Separate measured speaking length, elongation, unit mass, bending
stiffness, load-extension and open-tension inputs identify the physical model;
the DI pitch is its outcome, not enough data to identify those terms by itself.

The existing live-fret control-rate pitch solve, not note allocation alone,
interpolates the table in cents and multiplies the target by
`2^(cents/1200)`, so slides and legato consume the same continuous residual.
The first runtime candidate corrects only the fundamental: ideal speaking-length
pickup geometry remains explicit, rather than pretending that a cents residual
separately identified length and tension. The residual moves only the compensated
total-period target and is explicitly excluded from the nominal construction,
tension and inharmonicity coordinates, which retain their ideal unbent-fret
inputs. It is bypassed away from its measured setup. Production interpolation
across Guitar Build is forbidden until every curated scale/gauge anchor has its
own capture, the interpolation law is frozen on training anchors and one
complete setup is held out. No oscillator, filter, delay read or sample-loop
branch is added; the preregistered 96 kHz eight-string ceiling is 0.25% CPU.

Promotion is capture-gated on the actual target guitars, installed string sets
and setups. Record mechanically repeated raw DI at every string/fret while
logging scale, each saddle setback, nut position, action, relief, fret crown,
gauge and temperature. At each stopped fret, optically measure speaking length
and total extension under a preregistered jig contact position and normal force;
matched string samples supply unit-mass, flexural-rigidity and load-extension
data.
Record the tested string's own open fundamental immediately before and after
each block, with an adjacent open only as a secondary drift check. Freeze jig
acceptance and exclusion rules before capture. Use at least five accepted jig
repetitions for identification and human-fretted repeats for external validity.

Fit the low-dimensional physical model to even-fret pitch outcomes only and
score its predicted odd frets unopened. If that gate passes, refit the same
model on all frets, freeze its table, and reserve a later remove/reinstall
session for final validation. Multi-anchor interpolation additionally leaves
one complete guitar/setup unopened. This makes the odd cells predictions of a
named model rather than fitting a direct lookup and then calling its missing
entries holdout. Existing public DIs do not document or repeat these inputs, so
their pitch offsets cannot identify the table.

The residual itself must repeat above a two-cent practical floor or the feature
is rejected. Every string must show at least 50% lower held-out median absolute
cents error than exact 12-TET, with no worse p95 or maximum and at least 80%
correct residual signs among cells whose repeated residual clears that floor.
Hidden distorted chord voicings must also reduce
inter-string beat-frequency error and win an RMS-matched blind A/B. Open strings
stay exact and alternate string assignments retain their own deterministic
setup. The initial candidate adds only a static unbent offset in cents; it does
not claim that bending leaves tension and stiffness unchanged. Preregistered
low/middle/high-fret physical bends and equivalent wheel/MPE trajectories must
show no pitch-error regression before that approximation may ship, otherwise a
separately identified bent-state correction is required. The 44.1--384 kHz
tuning/stability and CPU ceilings remain hard rails. Until these captures pass,
importing published sonometer or classical-guitar residuals would be invented
realism.

The frozen perceptual gate scores ten real/model cells plus three hidden repeats
with exactly 30 listeners: 15 extended-range guitarists and 15 metal producers.
A separate licensed-leader session compares Electric Storm Deluxe, Shreddage
3.5 Hydra and Uproar RAW. Real parity is not a market win: “best sounding”
requires predeclared superiority. Until those commissioned captures and the
frozen blind comparison pass, Electry claims a research-grounded,
regression-measured model—not capture parity or market leadership.

## Development checkpoints

### 2026-08-29 choice-strip arrow navigation

- Left/Up and Right/Down now move to the previous or next option in every
  exclusive-choice strip, wrapping at both ends and moving keyboard focus with
  the selection. The target button enters the same choice-action path as a
  click exactly once, so the highlight, callback and bound parameter cannot
  diverge. Disabled options are skipped; PANIC remains an ordinary action
  button and does not consume arrows.
- The plug-in regression covers all four arrow keys, both wrap directions,
  single-callback delivery, disabled-option skipping, and rejection by disabled
  or non-radio buttons. Space and Return retain their established activation
  paths. This adds no layout,
  painting, parameter, audio or steady-CPU work; it runs only for a focused
  button's key event.

### 2026-08-29 sounding-period attack projection

- The compact modal attack now normalises against the compensated period at
  physical contact, not only the newly written MIDI destination. Its event-time
  gain is
  `clamp((f_E4/f_written) (P_contact/(fs/f_written)), 0.24, 9.0)`, which
  reduces to `f_E4/f_contact`: a settled legato contact retains its sounding
  source period and a fresh pre-bent note retains its bent period. The former
  destination-only law made equal-force hammer and pull gestures aimed at the
  same note project equally even when their source loops differed by four
  semitones.
- Independent regressions require the fixed-target hammer/pull amplitude ratio
  `2^(4/12) = 1.259921` and open-E1 displacement ratios `2^(2/12) = 1.122462`
  and `2^(-2/12) = 0.890899` at the standard wheel endpoints after removing
  the release pole's exact response. The old law fails all three at 1.0. The
  upper safety rail moved from 8.25 to 9.0 because open E1 bent down two
  semitones requires 8.979696; more extreme member/master MPE remains bounded.
- A same-compiler baseline/candidate render changed exactly the seven demos
  containing hammer/pull contacts: 03, 06, 15, 17, 18, 21 and 22. The other 16
  WAV pairs were byte-identical; only demo 21 crossed a tenth-decibel peak-table
  boundary. An ordinary unbent attack forms an exact `x/x = 1` scale, and the
  added arithmetic runs only when an excitation starts, never in the sample
  loop.
- This corrects the delay-line projection's own discretisation. No admissible
  recording in the current corpus holds finger force, target fret and rig
  fixed while varying only the sounding source period, so it is not presented
  as a capture fit or a real-recording parity claim. Residual-aware projection
  during an unfinished delay glide is a broader repick model and remains a
  separate evaluation rather than being bundled into this correction.

### 2026-08-29 keyboard Space activation

- PANIC and all six exclusive-choice strips now activate with Space as well as
  Return. They already requested keyboard focus and drew a focus outline, but
  JUCE 8.0.14's base text-button key handler accepts Return only. One shared
  Electry text-button type maps Space into that same JUCE activation path, so
  asynchronous click delivery, radio selection and existing callbacks retain
  one implementation rather than acquiring a second UI action path.
- The plug-in regression proves an enabled instance accepts Space, a disabled
  instance rejects it through JUCE's existing enabled-state guard, and PANIC
  plus every button in every choice strip uses the shared type. This changes no
  bounds, colours, labels, screenshots, parameters or audio and adds no steady
  UI or render CPU work; it is one key-event comparison when a focused button
  receives input.

### 2026-08-29 bridgeward upstroke endpoints

- Up strokes now retain their established `-0.020` open-string position shift
  through the complete Pick Position range. The former private `0.03..0.45`
  clamp contradicted its own “closer to the bridge” direction: at Pick Position
  zero it moved the nominal `.025 - .020 = .005` result to `.030`, neckward of
  the `.025` Down stroke, while at Pick Position one it moved the intended
  `.460` result to `.450` and made the offset 50% too large. For a Palm Up at
  the bridge end it changed `.025 * .8 - .020 = 0` to `.030`, also beyond the
  corresponding Down contact.
- Removing that redundant clamp leaves the one physical coordinate law in
  charge: `p = clamp((p_style + dx/L_open) 2^(fret/12), 0.02, 0.98)`.
  Independent regressions pin both Sustain endpoints for Down and Up, the
  exact two-per-cent neck-end separation, and Palm Up against the shared bridge
  guard while including deterministic metre variation. The change removes one
  note-start clamp and adds no state, render-loop operation or steady CPU.
- A same-compiler baseline/candidate render left all 23 canonical WAVs
  byte-identical because their Up/Alternate contacts stay outside the removed
  private clamp. The retained two-per-cent stroke-direction offset is existing
  voicing, not a capture fit: no admissible recording here holds player, pick,
  string and absolute contact position fixed while changing only direction.
  This corrects an internal geometry contradiction rather than claiming a
  real-recording win or using a listening preference to select a coefficient.

### 2026-08-29 far-side pick geometry

- Pick and co-located thumb positions now retain the complete speaking-string
  coordinate through the midpoint and up to the symmetric two-per-cent
  endpoint/contact guard at `p = 0.98`. The former 49% ceiling was not a safety
  bound: it moved every valid `0.5 < p < 1` contact to the midpoint and erased
  relative modal phase even though ideal comb magnitude can look mirrored.
  Nominal Pick Position 0.30 at top-string fret 22 is `p = 0.57552`; the old
  clamp displaced it 16.8 mm bridgeward on the default 27.625-inch scale. At
  fret 12 the nominal neck endpoint is `p = 0.96`, which the clamp displaced
  164.9 mm. Regressions pin the far-side absolute coordinate, the unchanged
  below-midpoint path, the unmirrored 98% endpoint and both pick/thumb identity.
- The existing positive `pP` touch image remains valid: because
  `p = p_open 2^(fret/12)` while `P = P_open 2^(-fret/12)`, its extra delay is
  the fixed hand distance `pP = p_open P_open`, not an allocation that grows
  with fret. Far-side lowest-string Pinch reads remain inside the delay line at
  44.1, 48, 96, 96.001, 192 and 384 kHz under -2/0/+2-semitone bends. This is
  one event-time clamp change with no new state or render-loop operation.
- The E3 Pinch mechanism now distinguishes bridge, near-midpoint and far-side
  settings: strongest partials are eight/four/nine and tracked means are
  6.03/4.74/7.06. That non-monotone return is the co-located
  `abs(sin(2 pi n p))` sequence crossing the midpoint, not a reason to restore
  the false clamp. Public recordings do not control an absolute far-side pick
  point, so this analytically corrects geometry rather than claiming a capture
  win; a decisive fit needs mechanically repeated raw DI at measured mirrored
  positions such as `p = 0.42/0.58`, scored for complex modal phase as well as
  magnitude.
- A same-compiler baseline/candidate render changed exactly the five canonical
  demos containing affected far-side notes: 02, 06, 17, 20 and 21. The other 18
  baseline/candidate WAV pairs were byte-identical, so their committed assets
  remain untouched; the level table records only the five changed candidates.

### 2026-08-29 live status accessibility announcements

- The live engine status now sends a JUCE accessibility `titleChanged` event
  whenever its displayed text actually changes. Accessibility clients can
  therefore discover current voice and sympathetic-string counts, engine
  readiness, sample rate, MIDI mute pressure and vibrato/tremolo gestures
  without rediscovering the component. Comparing the formatted title suppresses
  redundant notifications when hidden fields change or adjacent controller
  values round to the same percentage, while optional repaint scheduling stays
  independent. This changes metadata only on status transitions; audio, layout
  and steady render CPU are unchanged.

### 2026-08-29 physical-period touch geometry

- Natural- and pinch-harmonic contacts now place their extra loop tap at `p`
  times each polarisation's complete live fundamental period. The former
  `p * currentDelay` separation omitted damping and dispersion phase: on open
  E1, raw/full ratios are 0.87470 with fresh strings and 0.79329 with old
  strings, so String Age visibly moved a nominal midpoint finger. The new
  production helper is pinned against an independent per-axis oracle, both age
  extremes, a two-semitone pre-bend and a 64-sample in-flight bend where raw
  delay has not yet reached its target. It retains the horizontal mode's
  detune, the existing smoother and every filter-phase continuity translation.
  Ordinary notes remain on the exact zero-touch branch; an active touch keeps
  the same one extra cubic read and adds only the phase-restoration arithmetic,
  then the existing lift envelope removes that work. One local whole-engine
  96 kHz run measured 0.1208x realtime in the hostile Both/Stereo eight-string
  case, below the unchanged portable guard.
- Correcting the coordinate exposed one over-specific historical regression:
  with pick and thumb co-located at the then-existing 49% clamp, their ideal
  modal product is
  `abs(sin(2 pi n p))`, making the fourth start about 5.95 dB above the octave.
  The raw-delay error had accidentally reversed that ordering. The revised
  mechanism gate required a low even partial (two or four) near the midpoint,
  a bridge partial of at least six, a lower spectral mean toward that midpoint
  and more than 6 dB of squeal lift. The later far-side checkpoint retains that
  rail at Pick Position 0.90 and adds the distinct post-midpoint result instead
  of preserving the clamp. No arbitrary thumb offset was introduced; a
  measured fixed-metre thumb/pick separation remains capture-gated.
- This is corrected fundamental-period geometry, not an exact spatial contact
  for every inharmonic partial: a single temporal tap cannot represent all
  stiff-string nodes. The correction and that claim boundary are decided
  analytically, so no A--Z preference gate selects them.
- The five canonical demonstrations containing natural or pinch harmonics were
  regenerated from the validated engine; their rounded peak-normalisation
  table entries remain unchanged.

### 2026-08-29 physical-period pick geometry

- Pick-comb centre, 0.5-1.5 mm plectrum contact width and pull-off release
  position now map physical distance through the complete sounding period
  `P = fs/f0`. The former raw-delay coordinate was `P` minus damping and
  dispersion phase, so changing String Age subtly moved a stationary hand and
  fretting changed a fixed-width tip. Regressions pin absolute centre/width on
  one string from open to fret 12, the inverse-wave-speed law under a
  two-semitone pre-bend, invariance across String Age extremes, and both
  fretted and open pull-offs. At pull-off contact the loop is still sounding at
  the source fret, so that live period is already the released finger's
  `2L_old/c` image delay; no second `L_old/L_new` ratio is applied. The shipping
  regression also pins that delay to the nominal source-note period. The raw
  line/full-period probe reads 0.94413 on open E4 and 0.87422 on default E1,
  falling from 0.87470 to 0.79329 across E1's String Age extremes, which
  quantifies the false hand motion removed. The cached-period substitution is
  event-only: it adds no render-loop operation, memory or steady CPU. This
  corrects internal geometry; it is not a claim of capture parity.
- The existing lawful CC0 Drop-E four-ghost comparison improved without
  retuning a Dead coefficient. Against the real 30-100/100-250/250-380 ms
  median of -3.57/-12.66/-20.75 dB, shipping moved from
  -8.114/-14.919/-23.040 dB to -7.466/-14.073/-22.059 dB: three-window RMSE
  fell from 3.21 to 2.51 dB. Its first-versus-repick contextual RMSE also moved
  from about 4.85 to 4.82 dB. Every individual render remains inside the same
  broad four-take real ranges; deterministic whole-source spectral snapshots
  were refreshed, but those internal snapshots are not capture targets.
- All 23 committed demonstrations were regenerated from the validated engine,
  and the rendered-peak table above was refreshed. The objective geometry and
  existing real-envelope comparison decided this correction; no A--Z listening
  gate selected it.

### 2026-08-29 full accessible knob names

- All 25 rotary controls now expose the APVTS parameter's complete canonical
  name to accessibility clients while retaining their compact faceplate labels.
  This removes ambiguous screen-reader titles such as `MUTE`, `COMP`, `AGE`
  and `OUTPUT` without adding a parallel naming table. The plug-in regression
  resolves every knob by parameter ID and checks both its component title and
  JUCE accessibility-handler title. Rendering is unchanged; this is metadata
  only, with no audio, steady CPU or visual-layout cost.

### 2026-08-29 sympathetic exact-bypass state

- Automating Sympathetic to its exact zero endpoint now clears each inactive
  vertical delay line and every retained damping/dispersion state before the
  loop is marked unconfigured. A later played note therefore cannot inherit a
  hidden ring from state the bypass declared absent. The live full-to-zero
  regression starts with a populated coupled string and pins the line and all
  nine filter memories to exact zero. This is one endpoint clear with no
  steady-state CPU or memory cost; no listening gate was needed.

### 2026-08-29 sympathetic stiff-string dispersion

- Every unfingered ringing string now runs the same formula-derived `B` and
  eight first-order allpass sections as that physical string when played open.
  The idle bank follows the global wheel's live-tension law, refits only on the
  existing six-cent or build-geometry quantum, and compensates damping plus
  dispersion phase at every smaller move. Raw delay is translated against any
  filter-phase replacement, so a rising bend stays monotone across fit cells.
- This reuses the inactive voice's existing coefficients and filter state:
  one active plus seven idle strings adds 56 first-order sections but no
  allocation or memory. Played/idle-open fit parity, populated histories,
  target tuning, live `1/T` bend scaling, gauge invalidation, current-pitch
  continuity and proportional sympathetic-to-played state choking are pinned
  structurally. The 96 kHz one-active/seven-idle render measured `0.0731x`
  realtime for Both / Stereo and `0.0539x` for Bridge / Mono, versus about
  `0.0594x` and `0.0454x` before dispersion; its full wheel glide measured
  `0.0766x`. No A--Z gate selected this same-string physical parity correction.

### 2026-08-29 sympathetic pickup-path parity

- An unfingered ringing string now reaches the selected neck and bridge buses
  through the same distinct pickup-position combs, 4.8 mm aperture and 19 mm
  humbucker coil pair as that string when played open. The spatial delays
  follow the global wheel's live `1/c` coordinate, and Pickup Type's
  exact single-coil endpoint removes both second-coil reads. The quiet idle
  loop remains one-polarisation and uses a linear low-level magnetic map.
- The implementation reuses each inactive voice's existing fixed histories,
  advances only selected pickup paths and allocates no additional memory.
  Structural tests pin played/idle geometry parity, bend scaling, both pickup
  endpoints, selector culling and the exact one-active/seven-idle topology.
  Report-only local 96 kHz renders were about `0.059x` realtime for Both /
  Stereo and `0.046x` for Bridge / Mono, remaining below 6% realtime in the
  measured worst case (the prior simplified idle path measured about `0.043x`
  and `0.039x`).

### 2026-08-29 live-wave-speed pickup geometry

- Wheel/MPE bend and fretting vibrato now carry every representable pickup
  position, 4.8 mm aperture and 19 mm coil-spacing delay in the sounding
  string's live `1/c` coordinate; pure fretting/slide motion stays neutral and
  sub-sample apertures stay at the implementation's identity floor. A
  +2-semitone bend now targets approximately `0.8909x` the resting spatial
  delay instead of leaving that delay unchanged.

### 2026-08-29 live-tension stiffness dispersion

- Wheel/MPE bend and fretting-hand vibrato now carry stiffness through the
  static stiff-string model's tension coordinate. At fixed speaking length an
  `s`-semitone gesture scales tension by `2^(s/6)` and gives `B` the unclamped
  target `2^(-s/6)`; +2 semitones therefore moves `B` to `0.7937x` instead of
  leaving it fixed.
  Pure fret/slide travel retains its established tension and `1/L^2` law;
  combined gestures follow `1/(T L^2)` within the model's existing bounds.
- Regression coverage pins global and MPE bends in both directions, quantised
  finger-vibrato refits and an opposing bend/slide at constant total pitch.
  The default-off energy-attack factor remains outside the static fit. The
  shipping and attack-candidate DSP suites pass, local full wheel-glide stress
  renders remain around `0.13x` realtime, and the correction adds no grid
  search or per-sample work. No A--Z gate selected this objective equation.
  The public recordings audited can support a descriptive direction check,
  but lack measured tension and construction controls for capture calibration.

### 2026-08-29 provenance-locked exact-eight comparison

- `CompareReferenceFixture.py` closes the mechanical gap between the existing
  successor-only exact-eight receipt validator and the frozen v3 model probes.
  A separate frozen plan must name each upstream asset, zero-based channel,
  isolated crop, the onset-search start following at least 50 ms of pre-roll,
  nominal frequency and same-note model probe; it also binds the analyzer,
  validator, receipt, model manifest and every consumed model WAV by SHA-256.
  The comparator accepts only the full ten-probe, dry-DI 44.1 kHz v3 renderer
  contract and protects every listed probe plus every fixture input from an
  aliased output path.
- The resulting `electry-evaluation/v4` record detects each onset independently
  at or after the declared search start with the same native-rate 2 ms envelope,
  requires 20 dB onset SNR (or an exactly silent measured pre-roll) and a full
  two-second tail, then reports within-file-normalized RMS decay, onset-to-peak
  timing and centered-envelope first-cycle return. Peak times use the earliest
  frame within 0.05 dB of the window maximum, avoiding arbitrary later-cycle
  choices at low E1 rates; the contract exercises the same sine at 8, 44.1, 48
  and 96 kHz. It neither downmixes nor resamples and has no fit, aggregate score,
  distance threshold or promotion verdict. The record names its Python/runtime
  host because floating analysis is only promised deterministic there. This is
  a descriptive comparison surface, not evidence of capture parity or market
  superiority. No third-party recording or generated fixture artifact is
  committed; temporary contract tests synthesize their own reference audio. A
  real result still requires a reviewed upstream-original exact-eight WAV with
  usable quiet pre-roll.

### 2026-08-29 positioned following-fret loss candidate (default off)

- The shipping Artifacts limiter sees only the vertical wave at the loop seam,
  so every partial encounters the same threshold regardless of where a real
  downstream fret would touch. Poirot et al. identify that longitudinal modal
  distribution as a collision-location cue. `ELECTRY_POSITIONED_FRET_COLLISION`
  adds one cached linear tap at the equal-tempered following fret before touch,
  dispersion and fitted damping. Its fixed-depth law leaves node modes intact:
  the production regression measures gains 0.101/1.000 for the near-antinode
  ninth and near-node eighteenth harmonics.
- The experiment retains the established contact window, clearance opening,
  zero-slope soft knee, deterministic noise and saddle rattle. It does not arm
  its following-fret branch at fret 22, where this instrument has no following
  metal fret. Dead notes retain their old distributed-hand contact, including
  at fret 22, because they have no stopped speaking fret. The OFF build
  preprocesses to the shipping branch.
- This is intentionally a bounded one-read loss surrogate, not a reciprocal
  local two-rail scatter and not a claim of nonlinear passivity. It deliberately
  retains shipping's bilateral absolute-value barrier to isolate position in
  the A/B, rather than pretending to be a unilateral fretboard obstacle. A
  correct paired fractional gather/scatter needs another outgoing write and an
  adjoint interpolation law; inventing that machinery or a per-fret action map
  without measurements would overstate the evidence.
- Both OFF and ON pass the complete DSP suite; the candidate reaches contact
  from 44.1 through 384 kHz, including 96.001 kHz, stays finite in the maximum
  eight-string stability cases, and keeps the last fret's following-fret branch
  exact-null. Across three interleaved Release runs on the current machine, its
  extra tap is confined to the 25-100 ms opportunity; the median two-second
  default case costs 2.3 percent over OFF and the hostile all-eight-string 110
  ms Palm burst costs 6.9 percent. Other full-window timings stayed inside
  run-order noise.
- No admissible controlled exact-eight collision capture is currently local.
  The thirteen CC0 exact-eight Freesound previews are lossy, uncontrolled and
  show no stable H9/H18 trajectory; they remain descriptive no-fit evidence.
  Promotion therefore requires a whole-file-RMS-matched private OFF/ON verdict
  plus a populated original-PCM reference receipt with documented action,
  relief, fret heights and attack force.

### 2026-08-29 sample-rate-invariant polarisation seam

- The two returned travelling waves encounter their contractive mixing matrix
  once per loop, but its coefficient was calculated from the rate-specific
  compensated delay. The same pitch therefore received half the cross-amplitude
  gain at a 96 kHz host that it received immediately above the engine's
  2x-to-native boundary.
- The existing `0.04` anchor and nominal linear-with-`f0` voicing are now
  expressed against the sounding period at the 96 kHz reference clock. Damping
  and dispersion phase remain responsible for tuning, but can no longer retune
  the polarisation seam. This adds no work to the per-sample path.
- The 22nd-fret high-E decay spread across the three 0.1-0.5, 0.5-1.5 and
  1.5-3.0 s windows fell from 0.80/2.64/6.65 dB to
  0.063/0.092/0.153 dB across 44.1-192 kHz, including the 96/96.001 kHz seam.
  The eight-string 96 kHz worst-case CPU ratio remained 0.1197x. No listening
  gate or absolute coupling refit was used: this checkpoint enforces one
  digital/physical invariant and does not claim measured bridge calibration.

### 2026-08-28 force-decoupled exact pick release

- A sealed, whole-file-RMS-matched dry/metal/rock/blues comparison used A =
  the former force-coupled approximate release makeup and B = the
  force-decoupled exact-digital-pole path. The user selected B by ear.
- The shipping path now keeps MIDI velocity as player force and Pick Hardness
  as plectrum contact/stiffness, preserves the established default displacement
  exactly, and normalises the implemented release pole at the live picked
  fundamental. `ELECTRY_DECOUPLED_PICK_RELEASE=OFF` retains A only as a legacy
  comparator.
- The canonical 23 demos were regenerated. The force-normalised invariant is
  pinned for unbent attacks, the standard ±2-semitone wheel and ±24-semitone
  per-note expression; the macOS release build explicitly selects the promoted
  path even when reusing an older CMake cache.

### 2026-08-27 internal-rate phase-inverter capacitors

- The American/British output-coupling-capacitor solve was called once per
  oversampled frame but upper-clamped that frame clock to the 384 kHz host-rate
  ceiling. Hosts at 88.2, 176.4 and 352.8 kHz all run the nonlinear path at
  705.6 kHz, so their 100/22 nF companion models advanced 384 kHz coefficients
  705,600 times per second.
- Before correction, a quiet full British path alternated by rate family:
  40 Hz relative to 1 kHz moved from -20.938 to -23.373 dB and 80 Hz moved from
  -0.099 to -0.890 dB. The solver now retains its actual finite internal clock
  and applies only the existing lower safety bound.
- The direct 40 Hz circuit regression failed before the fix by 2.548 dB and now
  passes a 0.05 dB 384/705.6 kHz bound; the same public-path 48/88.2 kHz rail
  also closes the production-callsite gap. A separate
  full-path sweep across 48, 88.2, 96, 176.4, 192, 352.8 and 384 kHz leaves
  only 0.00376/0.00176 dB of spread. The 44.1 kHz canonical renderer already
  ran at 352.8 kHz, below the bad clamp, so none of its twenty-three WAVs
  changes. No A–Z gate was needed because the analogue time constant and the
  clock that advances it are objective.

### 2026-08-27 continuous slide stiffness

- A slide previously selected the destination integer fret's inharmonicity at
  contact even though its delay and audible pitch travelled continuously. An
  octave ascent therefore installed four times the source `B` before the
  fretting finger had moved.
- Stiffness now uses the same live fractional fret as the legato glide. The
  established string tension stays fixed while `B` follows `1/L^2` monotonically
  through ascending and chained descending travel; the existing phase-delay
  translation keeps effective pitch continuous and one endpoint-only dirty mark
  lands on the independently settled target fit.
- Regression coverage pins contact and chained-retarget continuity, the full
  geometric `B` trajectory, exact endpoint identity and a post-slide sub-quantum
  wheel move that must not reopen the expensive dispersion search. The existing
  portable CPU guardrails remain unchanged. No A–Z gate was needed because this
  repairs a violated physical formula without adding a voicing choice, control
  or dependency.

### 2026-08-27 MPE string-owned pitch expression

- Added MIDI MPE advertisement and JUCE's fixed-size lower/upper-zone RPN
  parser. A same-sample configuration sequence conditions its attacks before
  string allocation, independent of the host's event insertion order.
- Member channels now own physical strings and use the zone's exact declared
  semitone-plus-cent pitch range; a master wheel adds its own exact declared
  range to every member. Electry retains those float ranges beside JUCE's
  integer zone-routing state. Two member channels at the same MIDI pitch
  allocate separate strings and release independently, even if the zone layout
  changes before Note Off.
- The realtime path uses a fixed 128-entry FIFO for each MIDI pitch, fixed
  16-channel owner counts and bend arrays, and rejects an excess attack before
  it can become an unreleasable engine owner. With no valid zone, explicit
  expression ID zero and multichannel MIDI remain sample-identical to the
  legacy channel-agnostic path. A released tail freezes its performed member
  interval while its zone-master interval remains live. A released pending
  contact retains that snapshot through the exact contact sample; a genuinely
  new finger keeps the old ring unchanged during pick travel, then thaws to its
  cached member bend at contact. CC 121 recentres expression and nulls the
  current RPN/NRPN selection without deleting the zone or its exact ranges;
  prepare and release likewise clear that parser transaction while retaining
  the zone and ranges. CC 121/123 retain
  whole-instrument reset/release semantics, and prepare, release, Panic, All
  Sound Off and All Notes Off cannot leave stale note ownership.
- This checkpoint maps standardized pitch only. Pressure, CC 74 timbre and
  release velocity remain unassigned until captures and a predeclared listening
  comparison justify an audible physical mapping. No A–Z gate was needed for
  the routing itself because MIDI ownership and pitch intervals are objective.

### 2026-08-27 standard-rate nonlinear antialiasing

- Raised the distortion/amplifier domain from 4x to 8x at 44.1 and 48 kHz and
  retuned the shared 23-tap halfband window from Kaiser beta 8.6 to 7.5. The
  dense kernel sweep moved from 55.45 to 75.55 dB minimum stopband rejection
  while holding worst passband deviation to 0.00145 dB.
- Twelve full-path pedal, amplifier, stacked-gain and quiet-signal probes now
  sit below -70 dB at both standard rates. The complete range is
  -70.16..-83.25 dB; at 48 kHz, Modern High-Gain moved from -61.28 to
  -72.70 dB without changing its circuit topology or voicing.
- From 48 kHz upward, the rate policy now retains the smallest oversampling
  factor that reaches 384 kHz; 44.1 kHz remains the maximum-8x exception at
  352.8 kHz. All six nonlinear profiles remain below -70 dB on both sides of
  the resulting 96/192/384 kHz stage transitions.
- Engaged gain-stage delay rose from 17.25 to 20.125 host samples (0.42 ms at
  48 kHz). The comparable 23-demo renderer costs about 55% more wall time but
  still completes at 0.122x aggregate realtime on the documented M1 Max.
  Canonical Ubuntu regeneration changed the 13 wet/amped references while all
  ten dry/reference performances remained byte-identical.
- No A–Z listening gate was run: the predeclared non-harmonic-energy and kernel
  response measurements objectively selected this correction.

### 2026-08-27 blues-rock factory rig

- Promoted the host-representable version of demo 21's neck-pickup lead voicing
  to the **Blues Rock Lead** factory rig. It restores all 28 host controls from
  a deterministic starting point while leaving Pick Stroke, Play Style and
  LATCH/HOLD under the player's control, like the existing rigs.

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
  curve error but broke complete-chain level and muted-body rails. A default-off
  phase-preserved partitioned-convolution candidate now permits the controlled
  listening comparison while the shipping zero-latency cabinet stays unchanged.
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
- Point-touch harmonics as a bounded low-cost temporal contact surrogate —
  natural, pinch and touch — plus slides, hammer-ons and pull-offs, dead notes,
  palm muting as an additive absorber, and incidental fret collisions.
- A uniform ±2 semitone legacy MIDI pitch wheel plus lower/upper-zone MPE
  member/master pitch composition with exact semitone-plus-cent ranges over the
  Bend Time glide, and a resonance
  wheel that drives a bounded, saturating amplifier-feedback path through a
  fixed nominal 5.805 ms acoustic return, independent of host block size;
  pressure messages, MPE CC 74 timbre and release velocity are unassigned.
- A published pickup signal structure: per-string position comb following each
  fret, an O(1) fractional moving average for its chosen rectangular aperture,
  bounded flux nonlinearity differentiated into induced EMF, and a
  loaded coil/tone circuit.
- A four-mode solid-body structural path, geometry-informed and passive, with
  a six-anchor Guitar Build macro and independent pickup/tone/body-colour
  controls.
- An FX chain — distortion; selectable American, British and Modern amplifier,
  sag, transformer and parametric speaker/cabinet paths; compression; lead
  delay; and stereo room — with the circuit-derived nonlinear stages inside an
  up-to-8× oversampled domain, and every amount defaulting to a true 0 % dry
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
publishing scores. For a rights-reviewed upstream-original exact-eight fixture,
print and fill the receipt/evidence and frozen mapping-plan templates, validate
the receipt, then write the derived result under an ignored build directory:

```bash
python3 Tools/ValidateReferenceFixture.py --print-template receipt
python3 Tools/ValidateReferenceFixture.py --print-template evidence
python3 Tools/CompareReferenceFixture.py --print-template
python3 Tools/ValidateReferenceFixture.py /absolute/fixture/receipt.json
python3 Tools/CompareReferenceFixture.py \
  --plan /absolute/fixture/comparison-plan.json \
  --receipt /absolute/fixture/receipt.json \
  --model-manifest build-dsp/evaluation/manifest.json \
  --output build-dsp/reference-comparison-v4.json
```

The v4 output is descriptive-only and cannot authorize fitting or promotion.
Validate one commissioned session or a complete split-safe collection with:

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

Run the opt-in full-chain callback benchmark directly from that Release build:

```bash
ELECTRY_BENCHMARK_REALTIME=1 ./build/ElectryPluginProcessorTests
```

Its `BENCH` rows include rate, frame count, scenario, measured-block count,
deadline, p50/p95/p99/max milliseconds and `misses=N/blocks`. The benchmark is
diagnostic rather than a portable pass/fail performance claim.

On macOS, `./scripts/build-macos.sh` drives the same build through Xcode as a
universal VST3, Audio Unit and Standalone app and renders the committed editor
screenshot while the suite runs. After final signing it runs
`./scripts/validate-macos-artifacts.sh` against those exact bundles, including
strict signature and identity checks plus direct AU and VST3 load/render/state
smokes. After installation, `auval -strict -v aumu Elc1 Eltr` remains a useful
registry check, but it selects by type/subtype/manufacturer and therefore is not
proof that it found the just-built component. `./scripts/sign-and-package-macos.sh`
stages, signs and packages all three products; provide the signing and
notarization environment described by that script for distribution.

## Licensing

Original code under the MIT license (`LICENSE`). JUCE is used under its own
terms — see `THIRD_PARTY_NOTICES.md`. No guitar performances, sample libraries,
neural-network weights or third-party presets are included. The sole bundled
third-party audio-derived data is the CC0 1,024-sample cabinet-impulse prefix
identified in `THIRD_PARTY_NOTICES.md`.
