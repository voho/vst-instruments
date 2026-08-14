#include "DSP/TaikoEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace taikor
{
namespace
{
constexpr float piFloat = 3.14159265358979f;

// Air at 20 degrees Celsius. Both constants are load-bearing rather than
// decorative: the first sets how much the air hanging off the head lowers its
// modes, the second sets how hard the enclosed body pushes back.
constexpr float airDensity = 1.2041f;
constexpr float soundSpeed = 343.0f;

// The head's areal density, in kg/m^2, from a thin synthetic film to a thick
// cowhide. Geometric, because the audible parameter is the wave speed and that
// goes as the square root of the ratio.
constexpr float minimumArealDensity = 0.30f;
constexpr float maximumArealDensity = 1.60f;

// The head as a material rather than as an areal density, which is what its
// bending stiffness needs. Ando's measurements of nagado-daiko diaphragms put
// chemically treated cow skin at about 3.5 GPa and conclude that a taiko head
// has to be treated as a stretched plate - a stiff membrane - rather than as an
// ideal one. A synthetic film is a little stiffer per unit volume and a great
// deal denser, so the thickness that carries the same areal density is far
// smaller; since the flexural rigidity goes as the cube of that thickness, the
// two ends of Head Material are three hundred times apart in stiffness even
// though their moduli are within fifteen per cent of each other.
constexpr float filmModulus = 4.0e9f;    // biaxially oriented polyester
constexpr float hideModulus = 3.5e9f;    // treated cow skin
constexpr float filmDensity = 1390.0f;   // kg/m^3
constexpr float hideDensity = 1000.0f;   // kg/m^3
constexpr float headPoisson = 0.30f;
// The (0,1) Bessel zero squared, which is the reference the stiffness stretch
// below is taken against.
constexpr float fundamentalZeroSquared = 5.7831859629467f;

// Head tension in N/m. A tacked taiko head is under enormous tension compared
// with a drum-kit head, which is why it sits so far above its own diameter.
constexpr float minimumTension = 1200.0f;
constexpr float maximumTension = 22000.0f;
// Backstops on the resolved geometry. They are set wide enough that nothing
// reachable from the (already bounded) controls can touch them, because an
// octave that clamped would stop being an octave: two adjacent octaves would
// resolve to the same drum and the keyboard would go flat at the top. They
// exist only so a future change to the ranges above degrades rather than
// divides by zero.
constexpr float tensionFloor = 3.0f;
constexpr float tensionCeiling = 3.0e7f;
constexpr float radiusFloor = 0.008f;
// The largest radius the geometry may resolve to. It has to clear the widest
// head the control offers, taken to the bottom of the keyboard with the octave
// bought entirely by size: 1.80 m across, halved to a radius, times four for two
// octaves down, is 3.6 m. Clamping below that would stop the bottom octaves
// being octaves - they would come out 1.39x apart rather than 2x.
constexpr float radiusCeiling = 3.75f;

// The bachi. A taiko stick is a heavy piece of hardwood; the hardness control
// moves the Hertz contact stiffness across three orders of magnitude, from a
// felt-wrapped odaiko beater to a stick of seasoned oak.
//
// The mass is quoted for the reference drum and scales with the one actually
// being played, because nobody hits a shime-daiko with an odaiko club: a stick
// is chosen to suit the drum. Leaving it fixed made the smallest drums roughly
// twenty-five decibels louder than the largest, which is a property of the
// wrong stick rather than of the instrument.
constexpr float bachiMass = 0.19f;
constexpr float referenceRadius = 0.275f;
constexpr float minimumBachiScale = 0.30f;
// The ceiling has to clear the widest head at the bottom of the keyboard, or it
// stops being a scaling law and becomes a wall. At 2.2 it bound for every radius
// above 0.605 m - the whole battle-drum end of the instrument - so the two
// lowest octaves were handed the same stick as each other, and the largest drum
// in the family came out the quietest thing on it. The value now clears
// radiusCeiling over referenceRadius, the same reasoning radiusCeiling itself
// is written from.
constexpr float maximumBachiScale = 13.7f;
constexpr float minimumContactStiffness = 2.0e6f;
constexpr float maximumContactStiffness = 6.0e8f;
constexpr float restitution = 0.42f;
// Exact Gonthier damping factor for restitution 0.42, obtained from
//   (1 + d/e) / (1 - d) = exp (d (1 + 1/e)).
// Sun's closed-form approximation gave 2.20952 and an actual restitution of
// about 0.384; this root gives the coefficient the control claims, without any
// extra work in the audio loop.
constexpr double contactDampingFactor = 1.9314911227;

// A muted Tsu is made with the free hand resting on the hide. The palm remains
// for a short articulation rather than only for the millisecond in which the
// bachi touches, and removes energy locally from the head that was already
// ringing. These are the only fitted parts of that contact; the per-mode share
// comes from the mode shape integrated over a physical palm-sized patch.
constexpr float muteContactSeconds = 0.18f;
// Distributed dashpot density under the palm, in kg / (m^2 s). Integrating it
// over the physical patch and dividing by each mode's mass produces that
// mode's velocity-loss rate; large heads therefore do not choke as if the same
// hand covered the same fraction of them as it does on a shime-daiko.
constexpr float muteSurfaceDamping = 5000.0f;
constexpr float handPatchRadiusMetres = 0.055f;

// The head's high-frequency continuum. The first band sits just above the
// highest resolved mode and each one after it is an octave up; the bandwidth is
// wide enough that neighbouring bands overlap into a continuous region rather
// than reading as five separate whistles. The density law says how the number
// of modes grows through that region, and the calibration is its overall weight
// against the resolved bank - the one number here set against recordings of
// real drums rather than derived.
constexpr float continuumBandRatio = 2.0f;
constexpr float continuumBandwidth = 1.35f;
// A two-dimensional membrane has nearly constant modal density per hertz, so
// every octave contains about twice as many unresolved modes as the last. For
// comparable energy per mode their uncorrelated amplitudes add in quadrature:
// RMS therefore grows as sqrt(f). Contact bandwidth and the hide's measured
// loss then impose the steep downward slope heard in the finished stroke.
constexpr float continuumTilt = -0.5f;
// How much the rim takes from the continuum, per unit of a mode's dimensionless
// wavenumber. The resolved bank uses 0.12 per circumferential order; up here the
// orders are in the tens and hundreds, which is the whole reason this region
// empties long before the body does.
constexpr float continuumEdgeOrder = 0.08f;

// How much more the rim takes from a mode with a circumferential order. Those
// modes are pressed against the boundary rather than spread across the head.
constexpr float edgeOrderFactor = 0.12f;

// How hard a stroke couples into the continuum: a flat term, and the part that
// climbs as the stroke walks out towards the rim.
constexpr float edgeBoostBase = 0.30f;
constexpr float edgeBoostSlope = 1.80f;
// What the continuum's level is worth against the head's own modal receptance.
// In seconds, because the receptance it multiplies is a velocity per unit force
// and the level it produces is not: this constant carries the sample period the
// calibration used to inherit from mode.drive. It is re-anchored after the
// orthogonal-band rewrite so the factory five-shaku centre stroke carries about
// nine per cent of its analysed body from 250 Hz to 4 kHz. That deliberately
// sits below the twelve and nineteen per cent in the two reference recordings:
// the factory drum is larger and its resolved bank reaches less of that range.
constexpr float continuumCalibration = 34.0f / 48000.0f;

// Impact speed in m/s at the softest and hardest MIDI velocity. The bottom is a
// ghost stroke - a bachi tip barely leaving the head - and the top is a
// full-arm blow.
//
// The floor used to be 0.45 m/s, which is not a ghost stroke, it is a
// deliberate quiet note; and the whole instrument covered eight to thirteen
// times the impact speed end to end, which is about twenty-seven decibels of
// force. That is the single most common complaint about the sampled taiko
// libraries this competes with - "very little variation and limited dynamics" -
// and it is not a thing a physical model has any reason to inherit. The top of
// the range is left exactly where it was, so the loudest stroke the instrument
// can make has not moved and the factory output level still leaves it under
// full scale.
constexpr float minimumImpactSpeed = 0.12f;
constexpr float maximumImpactSpeed = 6.0f;

// One overall level constant. The model produces physical head displacements,
// which are on the order of tens of microns; this is the only place a number
// is chosen for how it sounds rather than for what it means, and it is a
// single scalar so it cannot distort any relationship inside the model.
//
// It came down by ten decibels when the head gained its high-frequency
// continuum, which added a great deal to every stroke, and back up by seventeen
// when that continuum was cut to the share it should always have had.
// directCalibration moves with it in both directions, because the airborne
// click is the one path that does not pass through this scalar and would
// otherwise drift against everything else.
constexpr float modelScale = 292.0f;

// The shape constant of the attack pitch glide: how much of the head's in-plane
// stiffness a displacement of a given size actually calls on. The physics is
// the Berger/von Karman result that a membrane clamped at its rim gains tension
// as the square of its transverse displacement; what this absorbs is the modal
// shape factor between the resonator states the engine has and the mean square
// slope the tension rise really depends on. It is dimensionless, and it is the
// only number here chosen rather than derived - deliberately placed after the
// division by modelScale, so the engine's output calibration cannot reach the
// drum's pitch. It used to: the term this replaced summed the raw resonator
// states, which are in units of that calibration, and at full velocity a third
// of the bend came from it.
constexpr float tensionStretchCalibration = 0.10f;
// Where that expansion stops describing the head. Berger's result is the first
// term of a series in the displacement, and it is meaningless once the tension
// a single stroke adds is comparable with the tension already there, so it is
// applied through a form that agrees with it exactly while the displacement is
// small and asymptotes here. The bound is not decoration: the fractional
// tension rise goes as the fourth inverse power of the radius, so a 15 cm head
// taken to the top of the keyboard with the octave bought entirely by size -
// which is a tiny slack membrane rather than a shime-daiko - reached fifteen
// semitones of attack bend without it. Nothing near the factory drum comes
// close; a full-arm stroke there raises the tension by a tenth.
constexpr float tensionStretchLimit = 0.30f;
// Release of the peak follower that stands in for the head's squared
// displacement. Long against a cycle of any mode that matters and short against
// the head's own decay, so the glide follows the ring rather than a clock.
constexpr float tensionFollowerSeconds = 0.040f;

// Radiation damping is the one loss term whose absolute size depends on how
// the drum is mounted and how much of the body is free to move, none of which
// this model represents. Its shape - which modes lose energy and how that
// changes with size and material - is physical; this scalar sets the overall
// depth so the default drum's fundamental lands where an ō-daiko's does.
constexpr float radiationCalibration = 0.020f;

// Loss into the shell, the hoops and the stand, and the corner below which a
// mode is long enough to move them at all.
constexpr float mountLossScale = 20.0f;
constexpr float mountLossCorner = 55.0f;
// The radius the corner above was calibrated at - the default head at octave 0 -
// so a reference drum comes out exactly where it always did and only the drums
// either side of it move.
constexpr float mountReferenceRadius = 0.475f;

// The viscous share of the hide's loss, as a damping rate per radian squared.
// This is what separates the head's body from its crack: it is worth about a
// twentieth of an inverse second at the fundamental and several hundred at six
// kilohertz.
constexpr float viscousScale = 1.5e-7f;

// The hysteretic share, as a loss angle. Hide is a lossy material and this is
// most of what stops a mode that cannot radiate.
constexpr float hysteresisScale = 0.00048f;

// How efficiently the shell's ring modes reach the microphones compared with
// the head's. The body radiates from a curved surface the pair is beside
// rather than in front of, and this model does not describe that geometry, so
// one scalar stands in for it - the same arrangement as radiationCalibration.
constexpr float shellCalibration = 4200.0f;

// The tack line of a byo-uchi drum. Iron tacks are driven round the head at a
// spacing, not in a fixed number: a nagado carries about forty-eight of them
// round a 1.8-shaku hoop, which is a nail every 36 mm, and a bigger drum is
// nailed with the same nails at the same spacing rather than with forty-eight
// nails spread further apart. So what each tack holds down is the head's tension
// over its own arc - that much force and no more, whatever the drum's size - and
// that is the force a stroke has to beat before anything rattles.
//
// It used to be a count, which made the preload rise with the circumference and
// left the rattle a mechanism only the smaller half of the family had: on a
// five-shaku o-daiko forty-eight tacks are one every 98 mm, each holding down
// nearly three times what a nagado's does, and a full rim shot could no longer
// lift one. Nothing else about the tack line scales with the drum - a tack is a
// nail - so the rattle still keeps its own pitch across the whole family.
constexpr float tackSpacingMetres = 0.036f;
constexpr float tackLowCorner = 2600.0f;
// How long a lifted tack goes on chattering while the head settles back onto
// it. Short, but several times the contact that started it.
constexpr float tackRattleSeconds = 0.004f;
constexpr float tackHighCorner = 9000.0f;
// How loudly a rattling tack reaches the pair, per newton it is being lifted
// with. In the same role as directCalibration and for the same reason: the
// geometry is computed, the radiating efficiency of a 6 mm iron head against a
// wooden shell is not.
constexpr float tackCalibration = 0.16f;

// Level of the airborne impact path relative to what the head radiates. It
// stands in for the contact patch's radiating area and directivity, neither of
// which this model describes; everything about how that path varies with
// stroke, position and distance is geometry and is computed, not chosen.
constexpr float directCalibration = 0.00478f;

[[nodiscard]] float clampFloat (float value, float low, float high) noexcept
{
    if (! (value == value)) // NaN
        return low;
    return value < low ? low : (value > high ? high : value);
}

[[nodiscard]] float lerp (float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

// Geometric interpolation, for the quantities whose audible spacing is
// multiplicative rather than additive.
[[nodiscard]] float geometricLerp (float a, float b, float t) noexcept
{
    return a * std::pow (b / a, t);
}

const std::array<ArticulationMetadata, articulationCount> articulationTable {{
    { Articulation::Don, "Don", "don", "don",
      "Full open stroke, a hand's width in from the middle", 0 },
    { Articulation::Ka, "Ka", "ka", "ka",
      "Out on the head near the tacks, thin and cutting", 1 },
    { Articulation::Tsu, "Tsu", "tsu", "tsu",
      "Damped centre, the free hand resting on the head", 2 },
    { Articulation::DonRim, "Don Rim", "donrim", "don",
      "Head and hoop struck together, the loud accent", 3 },
}};

// The four drums, one per octave. Nothing here is a scaling of anything else:
// each row is a real instrument of the taiko family, and every number in it is
// that instrument's own.
//
// Where the numbers come from, column by column.
//
// *Diameter* is the head as the drum is actually built - a 5-shaku o-daiko is
// about 1.50 m, a 2.5-shaku nagado about 78 cm, a standing okedo about 40, and a
// tsuke-shime about 30.
//
// The two large drums are the size they are because of what the family has to
// span. Three octaves of sounding pitch is a factor of eight, and the drums at
// the two ends of it are not heard at the same mode of their own heads: a shime
// is heard at its fundamental and an o-daiko is heard at the (1,1) mode a fifth
// and a half above its own, because the mounting empties a fundamental that low
// before anyone has taken a pitch from it (see soundingMode). So the family has
// to span a factor of fourteen in the fundamental to span eight in what is
// heard, and there is no 95 cm drum with a tacked cowhide head whose
// fundamental is low enough to be the bottom of that. A 5-shaku o-daiko's is,
// at an ordinary tacked tension, and 5-shaku o-daiko are what the bottom of a
// kumi-daiko set actually is.
//
// *Body depth* is in control units, where the engine reads depth / diameter =
// 0.40 + 0.90 * value. The four ratios are the instruments' own proportions: an
// o-daiko is a barrel roughly as deep as it is wide (0.85); a nagado-daiko is
// named for its long body and runs about 1.2 diameters; an okedo is a stave-
// built tub, longer again at about 1.25; and a shime is a shallow ring of wood
// at about 0.70. That single column is most of why the four drums split their
// axisymmetric pair so differently: the enclosed air is a column of that
// length, so a shallow shime is a far stiffer spring against its own head than
// a deep okedo is.
//
// *Head material* is in control units too, and what it really sets is the
// hide's areal density and therefore its thickness: 0.30 + geometric to 1.60
// kg/m^2, over a hide at about 1000 kg/m^3. The four rows are 1.05, 0.85, 0.55
// and 0.40 kg/m^2, which is roughly 1.05, 0.85, 0.55 and 0.40 mm of skin - the
// heavy cowhide an o-daiko carries, the lighter cowhide of a nagado, and the
// thin horse or calf hide an okedo and a shime are headed with. Because the
// head's bending stiffness goes as the cube of that thickness, this column is
// most of why the modal ratios of the four drums differ.
//
// *Tension* is in control units over 1.2-22 kN/m, and it is the one column a
// player sets rather than a maker: a drum's tension is whatever brings it to
// the pitch it is wanted at. These four are the tensions that put each drum on
// its own key of the keyboard, and they come out at 7.3 / 5.8 / 11.5 / 19.1
// kN/m - the tacked drums at much the same tension as each other, and the
// rope-laced ones far above them, with the shime at two and a half times the
// o-daiko on a head less than half as thick. That is exactly the difference
// between byo-uchi and shirabe-laced construction, and it is why a shime cracks
// where an o-daiko booms.
//
// *Shell* is in control units from light laminated stave work to dense carved
// zelkova, and it sets the body's ring modes, their Q, the wall's thickness and
// how much the rim absorbs from the head. The o-daiko and the chu-daiko are
// both carved keyaki, the chu-daiko a little below the o-daiko because a
// smaller log is not the same timber as the heart of a large one. The okedo is
// the outlier and has to be: it is not carved at all, it is thin cedar staves
// bound with hoops, so it is light, it rings, and it takes far more out of the
// head at the rim than a solid log does. The shime is carved keyaki again but
// small and proportionally thick-walled, which is the stiffest, deadest body of
// the four.
const std::array<DrumDescription, static_cast<std::size_t> (drumCount)>
    drumDescriptionTable {{
        { "O-daiko", "odaiko",
          "5-shaku carved zelkova barrel, thick tacked cowhide", 1.50f, 0.5000f,
          0.6200f, 0.7500f, 0.80f },
        { "Chu-daiko", "chudaiko",
          "2.5-shaku nagado-daiko: long carved body, tacked cowhide", 0.78f,
          0.8889f, 0.5437f, 0.6200f, 0.74f },
        { "Okedo-daiko", "okedo",
          "Stave-built tub, rope-laced thin hide, light ringing shell", 0.40f,
          0.9444f, 0.7760f, 0.3621f, 0.20f },
        { "Shime-daiko", "shime",
          "Shallow carved ring, thin hide laced to enormous tension", 0.30f,
          0.3333f, 0.9516f, 0.1800f, 0.92f },
    }};
} // namespace

const ArticulationMetadata& getArticulationMetadata (Articulation articulation) noexcept
{
    const auto index = static_cast<std::size_t> (articulation);
    return articulationTable[index < articulationCount ? index : 0u];
}

std::string_view getArticulationDisplayName (Articulation articulation) noexcept
{
    return getArticulationMetadata (articulation).displayName;
}

std::string_view getArticulationSlug (Articulation articulation) noexcept
{
    return getArticulationMetadata (articulation).slug;
}

const DrumDescription& getDrumDescription (int octaveOffset) noexcept
{
    const auto index = std::clamp (octaveOffset, lowestOctaveOffset, highestOctaveOffset)
                     - lowestOctaveOffset;
    return drumDescriptionTable[static_cast<std::size_t> (index)];
}

std::optional<Articulation> articulationForMidiNote (int midiNote) noexcept
{
    if (midiNote < lowestPlayableNote || midiNote > highestPlayableNote)
        return std::nullopt;
    // The octave is twelve semitones, because the octave is what chooses the
    // drum and that has to line up with the keyboard. Four of those twelve
    // carry a stroke - the bottom four - so the grid is four drums by four
    // strokes and the eight keys above each drum's strokes are silent. Better a
    // gap a player learns once than eight more keys that sound like ones they
    // already have.
    const auto pitchClass = midiNote % 12;
    if (pitchClass >= static_cast<int> (articulationCount))
        return std::nullopt;
    return static_cast<Articulation> (pitchClass);
}

std::optional<int> octaveOffsetForMidiNote (int midiNote) noexcept
{
    if (midiNote < lowestPlayableNote || midiNote > highestPlayableNote)
        return std::nullopt;
    // Integer division is only floor division for non-negative operands, and
    // the playable range starts well above zero, so this is exact.
    return midiNote / 12 - referenceNote / 12;
}

int midiNoteFor (Articulation articulation, int octaveOffset) noexcept
{
    const auto pitchClass = static_cast<int> (articulation);
    const auto octave = std::clamp (octaveOffset, lowestOctaveOffset, highestOctaveOffset);
    return referenceNote + octave * 12 + pitchClass;
}

// ---------------------------------------------------------------------------
// Static physical tables
// ---------------------------------------------------------------------------

const std::array<TaikoEngine::MembraneModeEntry, TaikoEngine::modeEntryCount>&
TaikoEngine::membraneModes() noexcept
{
    // Zeros of J_m, in the order (m, n). Everything up to about 12.3 is
    // included: past that the modes are both inaudibly quiet and so heavily
    // damped that they have gone before the attack has finished. The four
    // axisymmetric entries must come first, because they are the ones the
    // cavity splits and the code below relies on that grouping.
    static const std::array<MembraneModeEntry, modeEntryCount> table {{
        { 0, 2.4048255576957728 },
        { 0, 5.5200781102863106 },
        { 0, 8.6537279129110122 },
        { 0, 11.791534439014282 },
        { 1, 3.8317059702075123 },
        { 1, 7.0155866698156188 },
        { 1, 10.173468135062722 },
        { 2, 5.1356223018406826 },
        { 2, 8.4172441403998649 },
        { 2, 11.619841172149059 },
        { 3, 6.3801618959239835 },
        { 3, 9.7610231299816697 },
        { 3, 13.015200721698434 },
        { 4, 7.5883424345038044 },
        { 4, 11.064709488501185 },
        { 5, 8.7714838159599540 },
        { 5, 12.338604197466944 },
        { 6, 9.9361095242176849 },
        { 7, 11.086370019245084 },
        { 8, 12.225092264004655 },
    }};
    return table;
}

const TaikoEngine::StrikeProfile& TaikoEngine::strikeProfile (
    Articulation articulation) noexcept
{
    // The whole articulation vocabulary is strike geometry plus what the other
    // hand is doing. Nothing here re-voices the drum: a Ka is bright because
    // hitting the head at 0.91 of its radius drives the modes with a
    // circumferential order and barely moves the axisymmetric ones, which is
    // exactly why it is bright on a real taiko too.
    //
    // For the same reason no stroke here lands on the geometric centre. Every
    // mode with a circumferential order has J_m(0) = 0, so a strike at radius
    // zero drives the axisymmetric modes and nothing else, and those are two
    // modes on a drum that has forty. The result is a note with an attack and
    // no body behind it - which is also what a real taiko does if you manage to
    // hit its exact centre, and why players do not: a full Don lands a hand's
    // width in from the middle, close enough to keep the fundamental and far
    // enough out to wake the rest of the head.
    //
    // Four strokes, spread deliberately. Where the stick lands decides which
    // modes it can reach, so two strokes a few centimetres apart are the same
    // stroke however differently they are labelled or levelled. The four run
    // 0.15, 0.20, 0.91 and 0.97 of the radius: two over the middle of the head
    // and two out by the tacks.
    //
    // Each pair is separated by a mechanism rather than by a distance, which is
    // what makes four keys worth having. Don and Tsu are five centimetres apart
    // on a 1.50 m head and are nothing like each other because one of them has
    // the free hand resting on the hide. Ka and Don Rim are six centimetres
    // apart out by the tacks and are nothing like each other because one is on
    // the head and the other is on the head and the hoop at once.
    static const std::array<StrikeProfile, articulationCount> table {{
        // radius, hardness, membrane, shell, noise, level, mute, palm,
        //   contacts, rim, shellFreq, shellDecay
        { 0.15f, 1.00f, 1.00f, 0.18f, 1.00f, 1.00f, 0.00f, false,
          1, 0.00f, 1.0f, 1.0f },  // Don
        { 0.91f, 1.28f, 0.74f, 0.42f, 1.35f, 0.82f, 0.12f, false,
          1, 0.30f, 1.0f, 1.0f },  // Ka
        { 0.20f, 0.98f, 0.88f, 0.12f, 0.95f, 0.72f, 0.95f, true,
          1, 0.00f, 1.0f, 1.0f },  // Tsu
        { 0.97f, 1.32f, 0.90f, 0.82f, 1.70f, 0.94f, 0.00f, false,
          1, 0.95f, 1.0f, 0.78f }, // Don Rim
    }};

    const auto index = static_cast<std::size_t> (articulation);
    return table[index < articulationCount ? index : 0u];
}

double TaikoEngine::besselJ (int order, double x) noexcept
{
    if (order < 0)
        return 0.0;

    if (x < 0.0)
    {
        const double mirrored = besselJ (order, -x);
        return (order % 2 == 0) ? mirrored : -mirrored;
    }

    // Ascending series. Only ever evaluated while a stroke or a drum is being
    // set up, never in the render loop, and the largest argument the mode
    // table can produce is about 12.3, where the series is comfortably stable
    // in double precision.
    double term = 1.0;
    for (int i = 1; i <= order; ++i)
        term *= (x * 0.5) / static_cast<double> (i);

    double sum = term;
    const double quarterSquared = 0.25 * x * x;

    for (int k = 1; k <= 64; ++k)
    {
        term *= -quarterSquared
              / (static_cast<double> (k) * static_cast<double> (k + order));
        sum += term;
        if (k > 24 && std::abs (term) < 1.0e-17)
            break;
    }

    return sum;
}

float TaikoEngine::stiffnessStretch (float besselZero, float stiffness) noexcept
{
    if (! (stiffness > 0.0f))
        return 1.0f;

    const float squared = besselZero * besselZero;
    const float numerator = 1.0f + stiffness * squared;
    const float denominator = 1.0f + stiffness * fundamentalZeroSquared;
    return std::sqrt (numerator / denominator);
}

// Loss into the shell, the hoops and the stand. Steeply low-pass in frequency:
// only a mode long enough to move the whole instrument loses anything this way.
float TaikoEngine::mountingLoss (const DrumState& drum, float frequency) noexcept
{
    return mountingLossAt (drum.mountLoss, drum.mountCorner, frequency);
}

float TaikoEngine::mountingLossAt (float mountLoss, float mountCorner,
                                   float frequency) noexcept
{
    // Fourth order. A gentler skirt was tried and measures worse against real
    // recordings by a wide margin: reaching only a little way above the corner
    // costs the body most of what makes it a body. Whatever the shell and the
    // stand take, they take it from the very bottom of the drum and from almost
    // nothing else.
    const float ratio = frequency / std::max (mountCorner, 1.0f);
    const float squared = ratio * ratio;
    return mountLoss / (1.0f + squared * squared);
}

// The same sum the modes are built with, re-evaluated at a new frequency. The
// two halves of the hide's loss are stored as coefficients rather than as a
// total precisely so this can be done.
float TaikoEngine::membraneDecayAt (const Voice& voice, const Mode& mode,
                                    float omega) noexcept
{
    const float ka = omega * voice.radiusMetres / soundSpeed;
    const float efficiency =
        radiationEfficiency (static_cast<int> (mode.circumferentialOrder), ka);

    return mode.decayFixed + mode.lossOmega * omega
         + mode.lossOmegaSquared * omega * omega
         + mode.radiationPrefactor * efficiency
         + mountingLossAt (voice.mountLoss, voice.mountCorner,
                           omega / (2.0f * piFloat));
}

// The hide's own loss: a hysteretic part that damps as omega and a viscous part
// that damps as omega squared. Extra damping is the hand on the head, which
// bears on both.
float TaikoEngine::materialDamping (const DrumState& drum, float omega,
                                    float extraDamping) noexcept
{
    const float hysteretic = 0.5f * drum.headLossFactor * omega;
    const float viscous = drum.headViscousFactor * omega * omega;
    return (hysteretic + viscous) * (1.0f + 2.4f * extraDamping);
}

float TaikoEngine::continuumBandVariance (float lowCoefficient,
                                          float highCoefficient) noexcept
{
    // State-space covariance of the exact filter used in renderVoice. The
    // input is unit-variance white noise and the nine states are, in order,
    // the two high-pass low-pole memories and the seven low-pass high-pole
    // outputs. At stationarity P = A P A^T + B B^T; a squared Smith iteration
    // gives the output variance as P[8,8].
    //
    // Doing this analytically on a coefficient-cache miss matters. A rule of thumb
    // based only on bandwidth drifts near Nyquist and would make the same drum
    // change level with the host sample rate. The solve is a few thousand
    // scalar operations per trigger and none per rendered sample.
    const double cLow = static_cast<double> (
        clampFloat (lowCoefficient, 1.0e-7f, 1.0f));
    const double cHigh = static_cast<double> (
        clampFloat (highCoefficient, 1.0e-7f, 1.0f));
    const double pLow = 1.0 - cLow;
    const double pHigh = 1.0 - cHigh;

    constexpr int highPassCount = 2;
    constexpr int lowPassCount = 7;
    constexpr int stateCount = highPassCount + lowPassCount;
    double a[stateCount][stateCount] {};
    double b[stateCount] {};

    // Carry the current cascade signal as an affine combination of the old
    // states and the new white-noise sample. This builds the exact state-space
    // update without a brittle page of hand-expanded coefficients.
    double signal[stateCount + 1] {};
    signal[stateCount] = 1.0;

    const auto addStage = [&] (int state, double coefficient, double pole,
                               bool highPass)
    {
        double next[stateCount + 1] {};
        next[state] = pole;
        for (int source = 0; source <= stateCount; ++source)
            next[source] += coefficient * signal[source];

        for (int source = 0; source < stateCount; ++source)
            a[state][source] = next[source];
        b[state] = next[stateCount];

        if (highPass)
            for (int source = 0; source <= stateCount; ++source)
                signal[source] -= next[source];
        else
            for (int source = 0; source <= stateCount; ++source)
                signal[source] = next[source];
    };

    for (int stage = 0; stage < highPassCount; ++stage)
        addStage (stage, cLow, pLow, true);
    for (int stage = highPassCount; stage < stateCount; ++stage)
        addStage (stage, cHigh, pHigh, false);

    // P_N = sum(k=0..N-1) A^k B B^T (A^T)^k. Doubling N each iteration via
    // P_2N = P_N + A^N P_N (A^N)^T converges to the discrete Lyapunov solution
    // in a handful of small matrix multiplies. It is both cheaper and better
    // conditioned than flattening the covariance into a 64-by-64 solve.
    double covariance[stateCount][stateCount] {};
    double transition[stateCount][stateCount] {};
    for (int row = 0; row < stateCount; ++row)
    {
        for (int column = 0; column < stateCount; ++column)
        {
            covariance[row][column] = b[row] * b[column];
            transition[row][column] = a[row][column];
        }
    }

    for (int iteration = 0; iteration < 20; ++iteration)
    {
        double left[stateCount][stateCount] {};
        double addition[stateCount][stateCount] {};
        double squared[stateCount][stateCount] {};

        for (int row = 0; row < stateCount; ++row)
            for (int column = 0; column < stateCount; ++column)
                for (int inner = 0; inner < stateCount; ++inner)
                {
                    left[row][column] +=
                        transition[row][inner] * covariance[inner][column];
                    squared[row][column] +=
                        transition[row][inner] * transition[inner][column];
                }

        for (int row = 0; row < stateCount; ++row)
            for (int column = 0; column < stateCount; ++column)
                for (int inner = 0; inner < stateCount; ++inner)
                    addition[row][column] +=
                        left[row][inner] * transition[column][inner];

        double maximumTransition = 0.0;
        for (int row = 0; row < stateCount; ++row)
            for (int column = 0; column < stateCount; ++column)
            {
                covariance[row][column] += addition[row][column];
                transition[row][column] = squared[row][column];
                maximumTransition =
                    std::max (maximumTransition, std::abs (squared[row][column]));
            }

        if (maximumTransition < 1.0e-14)
            break;
    }

    const double variance = covariance[stateCount - 1][stateCount - 1];
    return static_cast<float> (
        std::isfinite (variance) && variance > 1.0e-12 ? variance : 1.0);
}

float TaikoEngine::radiationEfficiency (int order, float ka) noexcept
{
    // A membrane mode of circumferential order m has 2m sign changes around
    // the head, so the air it pushes out on one side it pulls in on the other
    // and it radiates as a 2m-pole: efficiency rises as (ka)^(2m+2) until ka
    // reaches the mode's own order and then saturates. This single expression
    // is why a centre strike is heard as a boom and an edge strike as a slap.
    const float x = ka > 1.0e-4f ? ka : 1.0e-4f;
    const float exponent = 2.0f * static_cast<float> (order) + 2.0f;
    const float kneeBase = 1.0f + 1.4f * static_cast<float> (order);

    // Algebraically x^n/(x^n + knee^n), but evaluated as 1/(1 + (knee/x)^n) so
    // neither term is ever formed on its own. The direct version overflows: the
    // top modes carry a circumferential order of eight, so the exponent is
    // eighteen, and x^18 leaves float range once ka passes about 138. That is
    // reachable - a small, hard, high-tension head two octaves up, rendered at
    // 384 kHz - and it produced inf/inf, a NaN decay rate, and seven high
    // partials silently retired out of an edge stroke.
    //
    // In this form the extremes are exactly the limits they should be: a large
    // ka drives the ratio to zero and the efficiency to one, and a small ka
    // overflows the ratio to infinity, which divides to a clean zero rather
    // than to NaN.
    return 1.0f / (1.0f + std::pow (kneeBase / x, exponent));
}

float TaikoEngine::readDelayLine (const std::array<float, directLineSize>& line,
                                  int writeIndex, float delaySamples) noexcept
{
    static_assert ((directLineSize & (directLineSize - 1)) == 0,
                   "the airborne delay line must be a power of two");
    constexpr int mask = directLineSize - 1;

    delaySamples = clampFloat (delaySamples, 0.0f,
                               static_cast<float> (directLineSize - 2));

    const float position = static_cast<float> (writeIndex) - delaySamples;
    const int index = static_cast<int> (std::floor (position));
    const float fraction = position - static_cast<float> (index);

    // `index` sits just past the requested delay, so the other end of the
    // interpolation is the sample after it, towards the present. Reaching
    // backwards instead adds the fractional part to the delay rather than
    // subtracting it - a quarter of a sample becomes one and three quarters,
    // with a step at every integer - and this delay is the inter-microphone
    // timing cue that places a stroke across the image.
    const float older = line[static_cast<std::size_t> (index & mask)];
    const float newer = line[static_cast<std::size_t> ((index + 1) & mask)];
    return older + (newer - older) * fraction;
}

void TaikoEngine::solveAxisymmetricBranch (float diagonalB, float diagonalR,
                                           float offDiagonal, int branch,
                                           float& eigenvalue, float& vectorB,
                                           float& vectorR) noexcept
{
    const float centre = 0.5f * (diagonalB + diagonalR);
    const float half = 0.5f * (diagonalB - diagonalR);
    const float discriminant = std::sqrt (half * half + offDiagonal * offDiagonal);

    eigenvalue = branch == 0 ? centre + discriminant : centre - discriminant;

    vectorB = offDiagonal;
    vectorR = eigenvalue - diagonalB;

    // Whether the two heads are coupled at all is a structural question, not a
    // question of magnitude: the deciding quantity is the off-diagonal, and
    // testing the vector's own length against a fixed epsilon does not answer
    // it. These values run from 1e5 to 1e10, so the float residue left in
    // `eigenvalue - diagonalB` when the heads are uncoupled is itself far
    // larger than any absolute epsilon.
    const float couplingScale =
        1.0e-6f * std::max (std::abs (diagonalB - diagonalR), 1.0f);

    if (std::abs (offDiagonal) > couplingScale)
    {
        const float length = std::sqrt (vectorB * vectorB + vectorR * vectorR);
        vectorB /= length;
        vectorR /= length;
        return;
    }

    // Uncoupled: this eigenvector is one head or the other, and which one is
    // decided by the diagonal the eigenvalue came from, not by the branch
    // index. The resonant head is lighter than the batter head and so is
    // usually the higher of the two; picking by branch handed both branches to
    // it, the batter share was zero on both, and a centre strike lost its boom
    // entirely the moment Air Coupling reached zero.
    const float toBatter = std::abs (eigenvalue - diagonalB);
    const float toResonant = std::abs (eigenvalue - diagonalR);
    const bool tied = std::abs (toBatter - toResonant)
                    <= 1.0e-6f * std::max (1.0f, std::abs (eigenvalue));
    // Two identical heads give one repeated eigenvalue; there the branch index
    // is the only thing that can separate them.
    const bool isBatter = tied ? (branch == 0) : (toBatter < toResonant);
    vectorB = isBatter ? 1.0f : 0.0f;
    vectorR = isBatter ? 0.0f : 1.0f;
}

float TaikoEngine::columnStiffnessFactor (float x) noexcept
{
    // A rigidly terminated column of air driven at one end presents the
    // stiffness rho c omega cot(omega l / c) per unit area, which is
    // (rho c^2 / l) * x cot x with x = omega l / c. The x -> 0 limit of that
    // is the lumped spring the model used to be, so this factor is exactly one
    // where the cavity is short against the wavelength and falls away as the
    // body gets deep against it. The caller supplies l: for a two-headed drum
    // it is half the body, because the volume-changing motion is symmetric
    // about the midplane and that plane is a velocity node.
    constexpr float quarterWave = 0.5f * piFloat;

    if (! (x > 0.0f))
        return 1.0f;

    // At the quarter-wave the column is resonant and its input stiffness is
    // zero: the head sees a pressure release and the air stops tying the two
    // heads together. Above it cot goes negative, which is a real thing - the
    // air is mass-like there - but it is a mass this model has nowhere to put,
    // because the enclosed air is a stiffness and not a degree of freedom, and
    // past the second pole at x = pi the expression turns positive again on a
    // branch that means something else entirely. So the correction is taken
    // over the one branch on which a lumped stiffness has a meaning, and above
    // it the answer is the one the engine already knows how to report: an
    // uncoupled pair of heads, which is what measure() describes at Air
    // Coupling zero. The truncation is continuous, because x cot x reaches
    // zero at the quarter-wave rather than jumping to it.
    if (x >= quarterWave)
        return 0.0f;

    return x * std::cos (x) / std::sin (x);
}

float TaikoEngine::volumeBranchOmega (const DrumState& drum,
                                       float cavityStiffness) noexcept
{
    // The (0,1) pair, built exactly as measure() and buildVoiceModes build it:
    // no stiffness stretch, because the stretch is normalised at this mode, and
    // the air load's shape factor is one here for the same reason.
    const auto lambda = static_cast<float> (membraneModes()[0].besselZero);

    const float idealBatter =
        drum.waveSpeed * lambda / (2.0f * piFloat * drum.radius);
    const float idealResonant =
        drum.resonantWaveSpeed * lambda / (2.0f * piFloat * drum.radius);
    const float loadBatter = 1.0f / std::sqrt (
        1.0f + 0.85f * airDensity * drum.radius / drum.batterDensity);
    const float loadResonant = 1.0f / std::sqrt (
        1.0f + 0.85f * airDensity * drum.radius / drum.resonantDensity);

    const float omegaBatter = 2.0f * piFloat * idealBatter * loadBatter;
    const float omegaResonant = 2.0f * piFloat * idealResonant * loadResonant;

    const float cavity = cavityStiffness * 4.0f / (lambda * lambda);
    const float diagonalB = omegaBatter * omegaBatter + cavity / drum.batterDensity;
    const float diagonalR = omegaResonant * omegaResonant + cavity / drum.resonantDensity;
    const float offDiagonal =
        cavity / std::sqrt (drum.batterDensity * drum.resonantDensity);

    float eigenvalue = 0.0f;
    float vectorB = 0.0f;
    float vectorR = 0.0f;
    // Branch zero is the higher eigenvalue, and with a positive off-diagonal
    // its eigenvector has both heads moving the same way in the symmetrised
    // coordinates - that is the branch that changes the body's volume, and it
    // is the only one the enclosed air stiffens.
    solveAxisymmetricBranch (diagonalB, diagonalR, offDiagonal, 0, eigenvalue,
                             vectorB, vectorR);
    return eigenvalue > 0.0f ? std::sqrt (eigenvalue) : 0.0f;
}

TaikoEngine::AxisymmetricPair
TaikoEngine::solveAxisymmetricPair (const DrumState& drum) noexcept
{
    // The (0,1) pair, built exactly as buildVoiceModes builds it. No stiffness
    // stretch on either branch: the stretch is taken relative to the (0,1) mode
    // and this is the (0,1) mode, so it is unity by construction - which is the
    // whole point of normalising it there. The air load's shape factor is one
    // here for the same reason.
    const auto lambda = static_cast<float> (membraneModes()[0].besselZero);

    const float idealBatter =
        drum.waveSpeed * lambda / (2.0f * piFloat * drum.radius);
    const float idealResonant =
        drum.resonantWaveSpeed * lambda / (2.0f * piFloat * drum.radius);
    const float loadBatter = 1.0f / std::sqrt (
        1.0f + 0.85f * airDensity * drum.radius / drum.batterDensity);
    const float loadResonant = 1.0f / std::sqrt (
        1.0f + 0.85f * airDensity * drum.radius / drum.resonantDensity);

    const float omegaBatter = 2.0f * piFloat * idealBatter * loadBatter;
    const float omegaResonant = 2.0f * piFloat * idealResonant * loadResonant;

    const float cavity = drum.cavityStiffness * 4.0f / (lambda * lambda);
    const float diagonalB = omegaBatter * omegaBatter + cavity / drum.batterDensity;
    const float diagonalR = omegaResonant * omegaResonant + cavity / drum.resonantDensity;
    const float offDiagonal =
        cavity / std::sqrt (drum.batterDensity * drum.resonantDensity);

    AxisymmetricPair pair;
    float upperEigen = 0.0f;
    float lowerEigen = 0.0f;
    solveAxisymmetricBranch (diagonalB, diagonalR, offDiagonal, 0, upperEigen,
                             pair.upperBatter, pair.upperResonant);
    solveAxisymmetricBranch (diagonalB, diagonalR, offDiagonal, 1, lowerEigen,
                             pair.lowerBatter, pair.lowerResonant);

    pair.upperHz = std::sqrt (std::max (upperEigen, 0.0f)) / (2.0f * piFloat);
    pair.lowerHz = std::sqrt (std::max (lowerEigen, 0.0f)) / (2.0f * piFloat);

    // Only the branches the batter head moves in are worth reporting, because
    // those are the only ones a stroke on the batter head can excite. The
    // eigenvectors are unit length, so a batter share this small is forty
    // decibels down and is not what anyone hears.
    //
    // It matters at zero Air Coupling, where the two heads are independent and
    // one branch belongs entirely to the far head. With the resonant head slack
    // it is also the lower of the two, so the readout named a silent 88.5 Hz as
    // the fundamental while the drum actually sounded 92.9 Hz - and named the
    // batter head's own mode the breathing mode, which on an open body does not
    // exist at all.
    constexpr float audibleShare = 0.01f;
    pair.upperAudible = std::abs (pair.upperBatter) > audibleShare;
    pair.lowerAudible = std::abs (pair.lowerBatter) > audibleShare;

    if (pair.upperAudible && pair.lowerAudible)
    {
        pair.breathingHz = pair.upperHz;
        pair.loadedFundamentalHz = pair.lowerHz;
    }
    else
    {
        // One branch only: the drum has a single axisymmetric mode you can
        // hear, and both figures are it. Reporting the same number twice is the
        // honest description of a body with no cavity to split it.
        const float audible = pair.upperAudible ? pair.upperHz : pair.lowerHz;
        pair.breathingHz = audible;
        pair.loadedFundamentalHz = audible;
    }

    return pair;
}

std::uint32_t TaikoEngine::hash32 (std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float TaikoEngine::signedUnitFromHash (std::uint32_t value) noexcept
{
    return static_cast<float> (static_cast<double> (hash32 (value)) / 2147483647.5 - 1.0);
}

float TaikoEngine::nextNoise (std::uint32_t& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    const auto bits = (state >> 9) | 0x3f800000u;
    float unit;
    std::memcpy (&unit, &bits, sizeof (unit));
    return 2.0f * (unit - 1.5f);
}

// ---------------------------------------------------------------------------
// Construction and lifecycle
// ---------------------------------------------------------------------------

TaikoEngine::TaikoEngine() noexcept
{
    applied_ = sanitise (EngineParameters {});
    prepare (48000.0, 512);
}

EngineParameters TaikoEngine::sanitise (const EngineParameters& parameters) noexcept
{
    EngineParameters result;
    result.headDiameter = clampFloat (parameters.headDiameter, 0.15f, 1.80f);
    result.bodyDepth = clampFloat (parameters.bodyDepth, 0.0f, 1.0f);
    result.tension = clampFloat (parameters.tension, 0.0f, 1.0f);
    result.headMaterial = clampFloat (parameters.headMaterial, 0.0f, 1.0f);
    result.shellMaterial = clampFloat (parameters.shellMaterial, 0.0f, 1.0f);
    result.resonantTension = clampFloat (parameters.resonantTension, 0.0f, 1.0f);
    result.cavityCoupling = clampFloat (parameters.cavityCoupling, 0.0f, 1.0f);
    result.headDamping = clampFloat (parameters.headDamping, 0.0f, 1.0f);
    result.shellResonance = clampFloat (parameters.shellResonance, 0.0f, 1.0f);
    result.pitch = clampFloat (parameters.pitch, -24.0f, 24.0f);
    result.bachiHardness = clampFloat (parameters.bachiHardness, 0.0f, 1.0f);
    result.strikePosition = clampFloat (parameters.strikePosition, -1.0f, 1.0f);
    result.velocityDepth = clampFloat (parameters.velocityDepth, 0.0f, 1.0f);
    result.tensionModulation = clampFloat (parameters.tensionModulation, 0.0f, 1.0f);
    result.strikeNoise = clampFloat (parameters.strikeNoise, 0.0f, 1.0f);
    result.humanise = clampFloat (parameters.humanise, 0.0f, 1.0f);
    // The old continuous Octave Body morph exposed a necessary modal-identity
    // handover inside the range. Drum Layout now presents only its two useful
    // physical endpoints; threshold here as well as in the host parameter so
    // direct API callers and legacy state restore get the same result.
    result.octaveBody = clampFloat (parameters.octaveBody, 0.0f, 1.0f) < 0.5f
        ? 0.0f : 1.0f;
    result.micDistance = clampFloat (parameters.micDistance, 0.0f, 1.0f);
    result.micSpread = clampFloat (parameters.micSpread, 0.0f, 1.0f);
    result.stereoWidth = clampFloat (parameters.stereoWidth, 0.0f, 1.0f);
    result.drive = clampFloat (parameters.drive, 0.0f, 1.0f);
    result.outputGain = clampFloat (parameters.outputGain, 0.0f, 2.0f);
    return result;
}

void TaikoEngine::prepare (double sampleRate, int maxBlockSize) noexcept
{
    sampleRate_ = std::clamp (sampleRate, minimumSupportedSampleRate,
                              maximumSupportedSampleRate);
    inverseSampleRate_ = static_cast<float> (1.0 / sampleRate_);
    maxBlockSize_ = maxBlockSize > 0 ? maxBlockSize : 512;

    const auto rate = static_cast<float> (sampleRate_);
    handDampingCoefficient_ = 1.0f - std::exp (-1.0f / (0.020f * rate));
    pitchBendCoefficient_ = 1.0f - std::exp (-1.0f / (0.035f * rate));
    gainSmoothing_ = 1.0f - std::exp (-1.0f / (0.015f * rate));
    // 12 Hz corner: low enough to leave an odaiko's fundamental alone, high
    // enough to remove the offset a one-sided strike leaves behind.
    dcCoefficient_ = std::exp (-2.0f * piFloat * 12.0f / rate);
    meterReleaseMultiplier_ = std::exp (-1.0f / (0.30f * rate));
    visualDecayMultiplier_ = std::exp (-1.0f / (0.35f * rate));

    prepared_ = true;
    drumCacheValid_ = false;
    ++physicalConfigurationRevision_;
    if (physicalConfigurationRevision_ == 0)
        physicalConfigurationRevision_ = 1;
    reset();
}

void TaikoEngine::reset() noexcept
{
    for (auto& voice : voices_)
        silenceVoice (voice);
    for (auto& physical : physicalDrums_)
    {
        silenceVoice (physical);
        physical.physicalBank = true;
    }

    handDamping_ = handDampingTarget_;
    pitchBend_ = pitchBendTarget_;
    if (std::abs (pitchBend_ - drumCacheBend_) > 5.0e-4f)
        drumCacheValid_ = false;
    smoothedOutputGain_ = applied_.outputGain;
    smoothedDrive_ = applied_.drive;
    smoothedWidth_ = applied_.stereoWidth;
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    driveAdaaLeft_ = driveAdaaRight_ = 0.0f;
    meterLeft_ = meterRight_ = 0.0f;
    visualLevel_ = 0.0f;
    noteSequence_ = 0;
    silentSamples_ = idleFreezeSamples;
    idleFrozen_ = true;

    outputLevelLeft_.store (0.0f, std::memory_order_relaxed);
    outputLevelRight_.store (0.0f, std::memory_order_relaxed);
    visualStrikeLevel_.store (0.0f, std::memory_order_relaxed);
    activeVoiceCount_.store (0, std::memory_order_relaxed);
}

void TaikoEngine::setParameters (const EngineParameters& parameters) noexcept
{
    const auto next = sanitise (parameters);

    // Only terms that enter the solved drum invalidate its lazy strike cache.
    // Pitch belongs here because a new contact needs projections at its current
    // frequencies, but it does not change the topology of an already-ringing
    // bank: applyTensionShift moves those poles continuously and cheaply.
    const bool physicalConfigurationChanged =
        next.headDiameter != applied_.headDiameter
        || next.bodyDepth != applied_.bodyDepth
        || next.tension != applied_.tension
        || next.headMaterial != applied_.headMaterial
        || next.shellMaterial != applied_.shellMaterial
        || next.resonantTension != applied_.resonantTension
        || next.cavityCoupling != applied_.cavityCoupling
        || next.headDamping != applied_.headDamping
        || next.octaveBody != applied_.octaveBody
        || next.micDistance != applied_.micDistance
        || next.micSpread != applied_.micSpread;
    const bool drumCacheChanged = physicalConfigurationChanged
                               || next.pitch != applied_.pitch
                               || next.shellResonance != applied_.shellResonance;

    applied_ = next;

    if (drumCacheChanged)
        drumCacheValid_ = false;

    if (physicalConfigurationChanged)
    {
        ++physicalConfigurationRevision_;
        if (physicalConfigurationRevision_ == 0)
            physicalConfigurationRevision_ = 1;
    }
}

void TaikoEngine::setHandDamping (float normalised) noexcept
{
    handDampingTarget_ = clampFloat (normalised, 0.0f, 1.0f);
}

void TaikoEngine::setPitchBend (float normalisedBipolar) noexcept
{
    pitchBendTarget_ = clampFloat (normalisedBipolar, -1.0f, 1.0f);
}

void TaikoEngine::allSoundsOff() noexcept
{
    for (auto& voice : voices_)
        silenceVoice (voice);
    for (auto& physical : physicalDrums_)
    {
        silenceVoice (physical);
        physical.physicalBank = true;
    }
    updateActiveVoiceCount();

    // Cutting every voice is already a discontinuity, so there is nothing left
    // for the shared DC and drive path to be continuous with. Clearing it is
    // what makes a panic actually silent instead of merely inaudible: the DC
    // blocker's own corner would otherwise take a further quarter of a second
    // to ring out.
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    driveAdaaLeft_ = driveAdaaRight_ = 0.0f;
    silentSamples_ = idleFreezeSamples;
    idleFrozen_ = true;

    // The two performance gestures are smoothed over twenty milliseconds, which
    // is right while the drum is sounding and wrong the instant it stops: a
    // panic leaves nothing for a gesture in transit to be continuous with, and
    // whatever it had reached would go on pressing into the next stroke. A hand
    // lifted a block ago is still 40 percent down after one buffer, so the first
    // note after a panic came out damped by a gesture the player had already
    // released. They snap to wherever the controls are actually being held -
    // which removes the lag, not the gesture: a hand held down stays down.
    handDamping_ = handDampingTarget_;
    pitchBend_ = pitchBendTarget_;
    // The wheel is geometry, so moving it here has to invalidate the drum that
    // geometry was solved for. The render loop notices a bend that has drifted
    // from the cache and rebuilds, but a panic moves it between blocks - and a
    // note arriving in the same block as the panic would otherwise be built on
    // the drum as it stood at whatever intermediate value the smoother had
    // reached, with tuningAtStrike recording the snapped target, so nothing
    // afterwards would ever correct it.
    drumCacheValid_ = false;

    visualLevel_ = 0.0f;
    visualStrikeLevel_.store (0.0f, std::memory_order_relaxed);
}

void TaikoEngine::silenceVoice (Voice& voice) noexcept
{
    voice.active = false;
    voice.configurationRevision = 0;
    voice.configurationPitch = 0.0f;
    voice.articulationLevelScale = 1.0f;
    voice.modeCount = 0;
    voice.activeModeCount = 0;
    voice.contactCount = 0;
    voice.nextContact = 0;
    voice.contactRemaining = 0u;
    voice.ageSamples = 0;
    voice.localMuteTicksRemaining = 0;
    voice.continuumMuteDampingRate = 0.0f;
    voice.palmDampingActive = false;
    voice.continuumHandDampingRate = 0.0f;
    voice.retireGain = 1.0f;
    voice.retireStep = 0.0f;
    voice.peakLevel = 0.0f;
    voice.tensionEnvelope = 0.0f;
    // Belongs to the stroke, like the schedule below it: it carries the drum's
    // in-plane stiffness against its tension, and a slot reused by a stroke on
    // a different drum must not inherit the last one's.
    voice.tensionDepth = 0.0f;
    voice.modalInput.fill (0.0f);
    voice.modeProjection.fill (0.0f);
    voice.contactProjection.fill (0.0f);
    voice.continuumInjection.fill (0.0f);
    voice.nonlinearContactActive = false;
    voice.nonlinearContactHasForce = false;
    voice.continuumInjected = false;
    voice.stickPosition = 0.0;
    voice.stickPrevious = 0.0;
    voice.stickMass = 0.1;
    voice.contactStiffness = 0.0;
    voice.contactDamping = 0.0;
    voice.residualImpedance = 1.0;
    voice.referenceContactEnergy = 1.0;
    voice.contactEnergyAdmittance = 0.0;
    voice.solvedContactEnergyStep = 0.0;
    voice.solvedContactForce = 0.0f;
    voice.appliedTensionShift = 1.0f;
    // Belongs to the stroke, not to the slot. The attack glide runs before the
    // new contact schedule is known - Tension Mod is on by default, so that is
    // every ordinary stroke - and it rebuilds the lifetimes; leaving the last
    // stroke's schedule here meant a voice reused after a flam or a press roll
    // was handed that stroke's offset, and then given its own on top. It kept
    // its modes alive past their floor and its slot alive past its deadline,
    // which costs resonators and invites voice stealing.
    voice.retirementOffset = 0;
    voice.noiseBandState = 0.0f;
    voice.contactReference = 0.0f;
    voice.tackScale = 0.0f;
    voice.tackRimGain = 0.0f;
    voice.tackEnvelope = 0.0f;
    voice.tackLowState = 0.0f;
    voice.tackHighState = 0.0f;
    for (auto& band : voice.continuum)
    {
        band.envelope = 0.0f;
        band.level = 0.0f;
        band.lowStateLeft = 0.0f;
        band.lowStateLeft2 = 0.0f;
        band.highStateLeft = 0.0f;
        band.highStateLeft2 = 0.0f;
        band.highStateLeft3 = 0.0f;
        band.highStateLeft4 = 0.0f;
        band.highStateLeft5 = 0.0f;
        band.highStateLeft6 = 0.0f;
        band.highStateLeft7 = 0.0f;
        band.lowStateRight = 0.0f;
        band.lowStateRight2 = 0.0f;
        band.highStateRight = 0.0f;
        band.highStateRight2 = 0.0f;
        band.highStateRight3 = 0.0f;
        band.highStateRight4 = 0.0f;
        band.highStateRight5 = 0.0f;
        band.highStateRight6 = 0.0f;
        band.highStateRight7 = 0.0f;
    }
    for (auto& mode : voice.modes)
    {
        mode.resonator.clear();
        mode.drive = 0.0f;
        mode.inverseModalMass = 0.0f;
        mode.contactShape = 0.0f;
        mode.micLeft = 0.0f;
        mode.micRight = 0.0f;
        mode.liveOmega = 0.0;
        mode.poleRadius = 0.0;
        mode.decayRate = 0.0f;
        mode.appliedPalmDecay = 0.0f;
        mode.localMuteDampingRate = 0.0f;
        mode.handDampingRate = 0.0f;
    }
}

void TaikoEngine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    for (const auto& drum : physicalDrums_)
        if (drum.active)
            ++count;
    activeVoiceCount_.store (count, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Resolving the physical drum
// ---------------------------------------------------------------------------

TaikoEngine::DrumState TaikoEngine::resolveDrum (int octaveOffset) const noexcept
{
    return resolveDrumFor (applied_, 2.0f * pitchBend_, octaveOffset);
}

void TaikoEngine::resolveDrumGeometry (const EngineParameters& applied,
                                       float radiusFactor,
                                       float tensionOctaveFactor,
                                       float tensionPitchFactor,
                                       DrumState& drum) noexcept
{
    drum.radius = clampFloat (0.5f * applied.headDiameter * radiusFactor,
                              radiusFloor, radiusCeiling);
    // The ceiling has to clear the geometry the controls can actually ask for,
    // exactly as radiusCeiling does: the widest head at the deepest body, taken
    // two octaves down with the octave bought by size, is 1.80 * 1.30 * 4. At
    // 2.0 m the factory drum was already sitting on the clamp at its lowest
    // octave, so Body Depth did nothing over the top half of its travel and the
    // cavity was reported as a shorter, stiffer spring than it is - which
    // pushes the one mode that radiates upward, and is most of why going down
    // the keyboard stopped making the drum lower.
    drum.depth = clampFloat (applied.headDiameter * radiusFactor
                                 * (0.40f + 0.90f * applied.bodyDepth),
                             0.04f, 9.5f);

    const float baseTension =
        geometricLerp (minimumTension, maximumTension, applied.tension);
    drum.tension = clampFloat (baseTension * tensionOctaveFactor * tensionPitchFactor,
                               tensionFloor, tensionCeiling);
    drum.resonantTension = clampFloat (
        drum.tension * (0.85f + 0.30f * applied.resonantTension),
        tensionFloor, tensionCeiling);

    drum.batterDensity =
        geometricLerp (minimumArealDensity, maximumArealDensity, applied.headMaterial);
    // The far head of a taiko is cut from the same hide but is not the one
    // being hit, so it is traditionally a touch lighter.
    drum.resonantDensity = drum.batterDensity * 0.92f;

    drum.waveSpeed = std::sqrt (drum.tension / drum.batterDensity);
    drum.resonantWaveSpeed = std::sqrt (drum.resonantTension / drum.resonantDensity);

    // The head's bending stiffness, which is what makes a taiko head a
    // stretched plate rather than an ideal membrane. Head Material already sets
    // the areal density; dividing by the material's own volumetric density
    // recovers the thickness the hide actually has, and the flexural rigidity
    // D = E h^3 / 12(1 - nu^2) follows. It is stored as the dimensionless
    // D / (T a^2), because that is the only combination the mode frequencies
    // depend on - and it is a comparison between the head's stiffness and its
    // tension, which is exactly the ratio Ando's eigenvalue tables are indexed
    // by.
    //
    // The far head is cut from the same hide and is a touch lighter, so it is
    // thinner in the same proportion and its rigidity falls as the cube of it.
    const float headDensity = lerp (filmDensity, hideDensity, applied.headMaterial);
    const float headModulus = lerp (filmModulus, hideModulus, applied.headMaterial);
    const float thickness = drum.batterDensity / headDensity;
    const float rigidity = headModulus * thickness * thickness * thickness
                         / (12.0f * (1.0f - headPoisson * headPoisson));
    const float radiusSquared = drum.radius * drum.radius;
    const float densityRatio = drum.resonantDensity / drum.batterDensity;

    drum.stiffnessBatter = rigidity / (drum.tension * radiusSquared);
    drum.stiffnessResonant = rigidity * densityRatio * densityRatio * densityRatio
                           / (drum.resonantTension * radiusSquared);

    // The same material seen in the plane of the head rather than across it.
    // A membrane clamped at its rim cannot move without stretching, and the
    // tension it gains goes as E h / (1 - nu^2) times the square of the
    // displacement over the radius. Divided by the tension it already has,
    // because what moves the pitch is the fractional change - which is why a
    // slack o-daiko head bends a long way sharp on a hard stroke and a shime
    // held at four times the tension barely moves at all.
    drum.stretchStiffness = headModulus * thickness
                          / ((1.0f - headPoisson * headPoisson) * drum.tension);

    // Hysteretic loss in the head material. A thick hide loses more per cycle
    // than a thin film; the damping control scales what the material gives.
    //
    // The cubic term is what gives the top of the control any authority. Half
    // of the fundamental's loss is radiation, which no amount of damping can
    // touch, so a linear map spends its whole range shortening the tail by
    // about a third - far less than laying a cloth over a real head does. The
    // curve leaves the lower two thirds of the control roughly linear and lets
    // the top of it genuinely deaden the drum.
    // Hysteretic loss in the hide, which goes as omega and therefore gives a
    // T60 proportional to 1/f. A hide is viscoelastic, though, and that is only
    // half of its loss: alongside the hysteretic part, whose loss angle is
    // frequency-independent, there is a viscous part whose stress follows the
    // rate of strain and which therefore damps as omega squared. The two
    // together are what make a struck head behave the way recordings of real
    // taiko do - a low body that rings for the better part of a second, and a
    // bright top that is gone in a tenth of one.
    //
    // Splitting them matters here more than it would on a smaller drum, because
    // this model resolves the low modes individually and treats everything
    // above the modal overlap as a continuum. One loss law had to serve both,
    // and no single power of omega can: set it for the body and the continuum
    // rings for the best part of a second as a bed of noise behind the drum,
    // set it for the continuum and the body is gone before it is heard. With
    // the viscous term carrying the top, the hysteretic term is free to be as
    // small as the low modes need.
    const float damping = applied.headDamping;
    const float dampingShape =
        0.55f + 1.30f * damping + 6.0f * damping * damping * damping;

    const float materialLoss = hysteresisScale * (0.33f + 1.00f * applied.headMaterial);
    drum.headLossFactor = materialLoss * dampingShape;

    // The viscous share. It is negligible at the fundamental - a few hundredths
    // of an inverse second at fifty hertz - and dominant by a kilohertz, which
    // is exactly the division of labour a hide has.
    const float viscousLoss = viscousScale * (0.55f + 0.90f * applied.headMaterial);
    drum.headViscousFactor = viscousLoss * dampingShape;

    // Loss into the hoop and the tacks. A soft laminated shell absorbs far
    // more of what reaches the rim than a dense carved log does.
    //
    // This is the only loss here that does not scale with frequency, which
    // makes it the ceiling on how long anything can ring: a mode cannot outlast
    // 6.9 / edgeLoss however little else takes from it. Measured against
    // recordings, a real head wants a second and a half in its body, so this
    // has to stay under about two inverse seconds - seven times smaller than it
    // once was here, when it was standing in for the mode-to-mode spread that
    // the radiation term now accounts for properly. Rim loss is a real
    // mechanism and a small one; it was carrying work that belonged elsewhere.
    //
    // Head Damping scales it from almost nothing to a great deal, rather than
    // from a little to a lot: a drum mounted so that the hoop is free really
    // can ring for several seconds, and collapsing that end of the control
    // would take the long ō-daiko boom off the instrument altogether.
    drum.edgeLoss = (0.34f + 1.50f * (1.0f - applied.shellMaterial))
                  * (0.15f + 1.85f * applied.headDamping);

    // Cavity stiffness per unit area. The per-mode 4/lambda^2 volume weighting
    // is applied where the modes are built, since it belongs to the mode.
    //
    // rho c^2 / L is the omega -> 0 limit of a cavity, and this instrument runs
    // out of that limit inside its own range: c/2L is 212 Hz on the factory
    // drum and 139 Hz at the deepest body, which is well under the top of the
    // resolved bank. The enclosed air is really a column, and the volume
    // changing motion of a two-headed drum is symmetric about the midplane, so
    // that plane is a velocity node and each head drives a rigidly terminated
    // column of length L/2. Its exact input stiffness is the lumped value times
    // x cot x with x = omega L / 2c.
    //
    // That makes the eigenproblem implicit: the stiffness depends on the
    // frequency it sets. It is solved here, once per drum, and never in the
    // render loop, which sees only the converged number.
    //
    // The map from the factor to the factor the branch it produces asks for is
    // monotone decreasing - a stiffer cavity raises the branch, which raises x,
    // which lowers the stiffness - and it lands in [0, 1]. So it has exactly
    // one fixed point, it is bracketed by the endpoints, and it is why the
    // correction is self-limiting rather than runaway: lowering the frequency
    // lowers x, which brings the factor back up.
    //
    // It is solved by bisection on that bracket rather than by relaxing the
    // iteration towards it, which was the first thing tried. Damped iteration
    // converges over most of the controls and does not converge everywhere: the
    // map's slope reaches about -100 where the cavity dominates the branch and
    // the factor is small, because omega then goes as the square root of the
    // factor while x cot x is falling steeply towards the quarter-wave, and no
    // fixed relaxation is stable against that. Over the full control scan a
    // half-damped iteration failed to settle in 0.4 % of configurations and
    // stopped wherever its iteration cap left it, which would have made the
    // reported factor a number about the solver. Bisection on a monotone
    // bracket cannot do that, and it converges in a fixed count.
    const float lumpedCavity = applied.cavityCoupling * airDensity * soundSpeed
                             * soundSpeed / drum.depth;

    {
        const auto asked = [&drum, lumpedCavity] (float factor)
        {
            const float omega = volumeBranchOmega (drum, lumpedCavity * factor);
            return columnStiffnessFactor (omega * drum.depth / (2.0f * soundSpeed));
        };

        // Zero unless the bracket opens, which is the case where even an
        // unstiffened head already sits past the column's quarter-wave: there
        // is then no frequency at which this cavity stiffens this drum at all,
        // and the honest answer is the decoupled pair the readout already
        // describes at Air Coupling zero.
        float factor = 0.0f;

        if (asked (0.0f) > 0.0f)
        {
            // Twenty-four halvings takes a unit bracket to six parts in a
            // hundred million, which is where a float runs out either way, so
            // the answer is a function of the drum rather than of the iteration
            // count. It is also what this costs: the solve roughly doubles the
            // time a drum resolve takes - 1.4 to 2.9 microseconds, measured -
            // and a drum resolve happens when a control moves or the wheel
            // passes a tenth of a cent, at most once per block, never per
            // sample.
            float low = 0.0f;
            float high = 1.0f;

            for (int iteration = 0; iteration < 24; ++iteration)
            {
                const float middle = 0.5f * (low + high);
                (asked (middle) > middle ? low : high) = middle;
            }

            factor = 0.5f * (low + high);
        }

        drum.cavityColumnFactor = clampFloat (factor, 0.0f, 1.0f);
    }

    drum.cavityStiffness = lumpedCavity * drum.cavityColumnFactor;

    drum.radiationScale = radiationCalibration;

    // What the mounting takes. The lowest modes of a large drum do not stay in
    // the head: they move the shell, the hoops and whatever the drum is stood
    // on, and that energy is gone. It is why a real o-daiko's fundamental dies
    // away faster than its body does - measured at a third of the time - while
    // a head modelled on its own damping alone rings longest exactly where it
    // should ring shortest. The term is steep, because a mode has to be low
    // enough to move the whole instrument before any of this applies at all.
    //
    // Where that begins is a comparison between the mode and the instrument, not
    // an absolute pitch: a mode moves the shell when its wavelength is on the
    // order of the drum's own size, so the corner scales with the drum the way
    // every other frequency in this function already does. Leaving it at a fixed
    // 55 Hz meant a larger drum slid its whole modal set down through a shelf
    // that did not move, and the stand ate more of the instrument the bigger the
    // instrument got - which is backwards, and it is why the o-daiko end of the
    // keyboard was both the quietest and the shortest.
    //
    // It is resolved here rather than after the octave transform because it is
    // the term that decides which of a drum's modes is the one heard: it is the
    // only loss steep enough to separate two modes a fifth apart, and the
    // sounding mode has to be identifiable at every trial the transform makes,
    // not only at the answer.
    drum.mountLoss = mountLossScale * (0.55f + 0.90f * applied.headDamping);
    drum.mountCorner = mountLossCorner * mountReferenceRadius
                     / std::max (drum.radius, radiusFloor);

    // The close pair. At zero spread both microphones sit over the centre of
    // the head and the instrument is exactly mono; opening it walks them out
    // towards the rim, where every mode with a circumferential order reaches
    // them with a different sign.
    //
    // Fully open is about fifty degrees of arc between the two, which is what a
    // close pair over one head actually is. It used to be a hundred and
    // twenty-six, and that is not a close pair, it is one microphone either
    // side of the drum: at that angle the two capsules sit on opposite sides of
    // the nodal diameter of every mode of order one, and the edge strokes -
    // which are the ones that drive those modes hardest - came out of phase.
    // The head's continuum used to bury that under enough uncorrelated noise to
    // keep the sum positive, which is not the same as the drum being mono-safe;
    // cutting the continuum to its proper share simply stopped hiding it.
    //
    // Resolved here for the same reason the mounting is: what a mode is worth
    // to the pair decides which mode the drum is heard at, and the transform
    // has to be able to ask that question of every trial drum it builds.
    drum.micRadius = drum.radius * (0.10f + 0.68f * applied.micSpread);
    constexpr float micReference = 0.60f;      // radians, off the mode axis
    const float separation = 0.9f * applied.micSpread;
    drum.micAngleLeft = micReference + 0.5f * separation;
    drum.micAngleRight = micReference - 0.5f * separation;

    drum.micDistanceMetres = lerp (0.03f, 0.40f, applied.micDistance);
    // Close microphones lift the low end. The depth follows the same distance,
    // so backing the pair off thins the drum exactly as it does in a room.
    drum.micProximity = 1.20f * (0.12f / (0.12f + drum.micDistanceMetres));
    // And the width trim the output stage will put on the finished pair, which
    // is part of the microphone geometry rather than part of the mix: it
    // decides how much of the difference between the two capsules survives, and
    // therefore what a mode is worth once the pair is combined.
    drum.stereoWidth = applied.stereoWidth;

    // How long a full open stroke stays on this head. Resolved here with the
    // mounting and the microphones, and for the same reason: a stroke is a
    // force pulse of finite length rather than an impulse, so how long it lasts
    // decides how much of it reaches each mode - and therefore which mode the
    // drum is heard at. Every trial drum the octave transform builds has to be
    // able to answer that.
    //
    // At the neutral impact speed, which is the one the velocity map leaves
    // alone: `shaped` in trigger() is lerp(0.72, velocity, velocityDepth), so
    // 0.72 is the speed every stroke has when Velocity Depth is zero, and it is
    // the one figure that describes this drum rather than one blow on it. The
    // readout has no velocity to report against and must not acquire one.
    // Across the playable range the contact time moves about 30 % either way,
    // which is a decibel in the weighting where two modes compete.
    {
        float strikerMass = 0.0f;
        float impedance = 0.0f;
        drumContactTerms (drum, strikerMass, impedance);
        const auto& profile = strikeProfile (Articulation::Don);
        const float collisionMass = contactCollisionMass (
            drum, profile, tuningStrikeRadius(), strikerMass);
        float peakForce = 0.0f;
        solveContact (collisionMass, impedance, profile, applied.bachiHardness,
                      geometricLerp (minimumImpactSpeed, maximumImpactSpeed, 0.72f),
                      drum.contactSeconds, peakForce);
    }
}

// What a stroke of contact time tau is worth in a mode at omega, relative to
// what an impulse would be worth in it. The render drives the bank with the
// Hertz force pulse - a sin^1.5 arch of length tau, see renderVoice - and a
// mode's free ringing after the contact is that pulse's own transform at the
// mode's frequency. It is flat well below 1/tau and falls away above it, which
// is why a soft beater on a large head sounds an octave lower than a hard one
// on the same drum: it is not that the low mode is louder, it is that the high
// one was never driven.
//
// The exact quantity has no elementary form, so this is a fit to it: the
// integral of sin(pi u)^1.5 e^(-i x u) over [0,1], divided by its value at
// x = 0. Measured against a two-hundred-thousand-point quadrature it is inside
// 0.05 dB everywhere out to x = 6. That covers every close comparison this
// function is asked for: on the two large drums, which are the ones whose modes
// come within a decibel of each other, the competing set stays below x = 6.5
// even with the softest beater the control offers. A felt beater on the two
// small ones does put their whole bank past x = 5 - the shime's fundamental
// lands at 9.5 - but there the winner leads the runner-up by 17.6 and 26.8 dB
// and nothing this fit can do reaches that.
//
// Above that it runs high: 1.6 dB at x = 8 and 9.5 dB at x = 10. The real
// transform has a null just past x = 9 - a pulse that vanishes as u^1.5 at both
// ends rings its own spectrum - and this is deliberately monotone through it
// rather than following it down, because a notch in the weighting would make
// the readout step as a control walked a mode across it. What it costs is
// accuracy in a region where the stroke has already lost thirteen decibels in
// that mode, and where being high is the conservative direction: it can only
// keep a mode in a comparison it would otherwise be dropped from.
float TaikoEngine::contactSpectrum (float omegaTau) noexcept
{
    const float x = std::abs (omegaTau);
    const float x2 = x * x;
    const float x4 = x2 * x2;
    const float denominator =
        1.0f + 0.039680f * x2 + 5.4298e-4f * x4 + 3.1752e-5f * x4 * x2;
    return denominator > 0.0f ? 1.0f / std::sqrt (denominator) : 0.0f;
}

// Every membrane mode of a resolved drum, one at a time, with what a stroke on
// the middle of the head is worth in it and how fast it empties. This is the
// same construction buildVoiceModes performs, reduced to the three numbers a
// comparison between two modes needs - where it is, how loudly it reaches the
// pair, and how long it lasts - and with the per-sample integration gain and
// the model's output calibration left out, because both are the same constant
// on every mode and cannot change which of them wins.
//
// `branch` is the two halves of the cavity-split pair for an axisymmetric mode
// and the cos member of the degenerate pair for every other, struck on the mode
// axis: that is the loudest member a stroke can drive, and the sin member is
// the same mode rotated a quarter period, so nothing is lost by leaving it out.
TaikoEngine::ModeObservation TaikoEngine::observeMode (const DrumState& drum,
                                                       int entryIndex, int branch,
                                                       float strikeRadius) noexcept
{
    ModeObservation result;

    const auto& entry = membraneModes()[static_cast<std::size_t> (entryIndex)];
    const int order = entry.circumferentialOrder;
    const auto lambda = static_cast<float> (entry.besselZero);

    const float radius = std::max (drum.radius, radiusFloor);
    const float sigmaB = drum.batterDensity;
    const float sigmaR = drum.resonantDensity;
    const float area = piFloat * radius * radius;
    // The full open stroke, which is the one a drum's pitch is heard in. Where
    // the stick lands decides which modes it can reach at all, so this has to
    // be a real stroke rather than a point at the centre - a strike on the
    // exact middle of the head drives the axisymmetric family and nothing else.
    // The caller supplies it, because the two questions asked of this function
    // want two different strokes: the readout wants the stroke the controls
    // actually produce, and the octave transform wants the centred one, so that
    // moving Strike Position cannot retune the keyboard.
    const float rho = clampFloat (strikeRadius, 0.0f, 0.995f);
    const float micRho = drum.micRadius / radius;
    const float micDistance = drum.micDistanceMetres;
    const float propagatingSpread = 1.0f / (1.0f + micDistance / 0.12f);

    const float idealBatter = drum.waveSpeed * lambda / (2.0f * piFloat * radius)
                            * stiffnessStretch (lambda, drum.stiffnessBatter);
    const float idealResonant =
        drum.resonantWaveSpeed * lambda / (2.0f * piFloat * radius)
        * stiffnessStretch (lambda, drum.stiffnessResonant);

    const float loadShape =
        (2.4048f / lambda) / (1.0f + 0.6f * static_cast<float> (order));
    const float loadBatter =
        1.0f / std::sqrt (1.0f + 0.85f * loadShape * airDensity * radius / sigmaB);
    const float loadResonant =
        1.0f / std::sqrt (1.0f + 0.85f * loadShape * airDensity * radius / sigmaR);

    const float omegaBatter = 2.0f * piFloat * idealBatter * loadBatter;
    const float omegaResonant = 2.0f * piFloat * idealResonant * loadResonant;

    const auto besselAtZero =
        static_cast<float> (besselJ (order + 1, entry.besselZero));
    const float besselSquared = std::max (besselAtZero * besselAtZero, 1.0e-9f);
    const auto shapeStrike =
        static_cast<float> (besselJ (order, entry.besselZero * rho));
    const auto shapeMic =
        static_cast<float> (besselJ (order, entry.besselZero * micRho));

    float omega = 0.0f;
    float amplitude = 0.0f;
    float decay = 0.0f;

    if (order == 0)
    {
        const float geometricMass = area * besselSquared;   // per unit density
        const float cavity = drum.cavityStiffness * 4.0f / (lambda * lambda);
        const float diagonalB = omegaBatter * omegaBatter + cavity / sigmaB;
        const float diagonalR = omegaResonant * omegaResonant + cavity / sigmaR;
        const float offDiagonal = cavity / std::sqrt (sigmaB * sigmaR);

        float eigenvalue = 0.0f;
        float vectorB = 0.0f;
        float vectorR = 0.0f;
        solveAxisymmetricBranch (diagonalB, diagonalR, offDiagonal, branch,
                                 eigenvalue, vectorB, vectorR);
        if (! (eigenvalue > 0.0f))
            return result;

        omega = std::sqrt (eigenvalue);
        const float frequency = omega / (2.0f * piFloat);
        const float sqrtSigmaB = std::sqrt (sigmaB);
        const float batterShare = vectorB / sqrtSigmaB;
        const float volumeShare = batterShare + vectorR / std::sqrt (sigmaR);
        const float efficiency =
            radiationEfficiency (0, omega * radius / soundSpeed);
        const float netVolume = 2.0f / lambda;
        const float volumeCoupling = netVolume * volumeShare;

        decay = drum.edgeLoss
              + drum.radiationScale * airDensity * soundSpeed * volumeCoupling
                    * volumeCoupling * efficiency
              + materialDamping (drum, omega, 0.0f)
              + mountingLoss (drum, frequency);

        const float drive = shapeStrike * batterShare / (geometricMass * omega);
        const float spatialWavenumber = lambda / radius;
        const float airWavenumber = omega / soundSpeed;
        const float nearField = std::exp (
            -std::sqrt (std::max (spatialWavenumber * spatialWavenumber
                                      - airWavenumber * airWavenumber,
                                  0.0f))
            * micDistance);
        const float observed =
            nearField * shapeMic * batterShare
            + efficiency * (2.0f * besselAtZero / lambda) * volumeShare
                  * propagatingSpread;
        const float proximity =
            1.0f + drum.micProximity
                       / (1.0f + (frequency / 190.0f) * (frequency / 190.0f));
        amplitude = std::abs (drive * observed * proximity);
    }
    else
    {
        if (branch != 0)
            return result;

        const float geometricMass = 0.5f * area * besselSquared * sigmaB;
        const auto orderFloat = static_cast<float> (order);

        omega = omegaBatter;
        const float frequency = omega / (2.0f * piFloat);
        const float efficiency =
            radiationEfficiency (order, omega * radius / soundSpeed);

        decay = drum.edgeLoss * (1.0f + edgeOrderFactor * orderFloat)
              + drum.radiationScale * airDensity * soundSpeed * efficiency
                    / (2.0f * sigmaB)
              + materialDamping (drum, omega, 0.0f)
              + mountingLoss (drum, frequency);

        const float drive = shapeStrike / (geometricMass * omega);
        const float spatialWavenumber = lambda / radius;
        const float airWavenumber = omega / soundSpeed;
        const float nearField = std::exp (
            -std::sqrt (std::max (spatialWavenumber * spatialWavenumber
                                      - airWavenumber * airWavenumber,
                                  0.0f))
            * micDistance);
        const float proximity =
            1.0f + drum.micProximity
                       / (1.0f + (frequency / 190.0f) * (frequency / 190.0f));
        // Both capsules, and then the width trim the output stage puts on them.
        //
        // Only the near field carries the shape of the head, so only it differs
        // between the two capsules, and for a mode of order m it differs by
        // cos(m theta) at each of their two angles. The width stage is
        // mid ± width·(L−R), so at 0.5 it hands the pair through untouched, at
        // 0 it sums them, and above 0.5 it exaggerates the difference. Reading
        // the left capsule alone described the instrument only at 0.5.
        //
        // It matters most where it used to be ignored hardest. With the pair
        // fully opened the two capsules straddle the nodal diameters of the low
        // orders, so at width 0 a mode of order three on the chu-daiko arrives
        // at the two of them within a per cent of anti-phase and all but
        // cancels in the sum - and the readout, reading the left capsule, named
        // it: 210.1 Hz against a rendered 119.8, with the named partial at 11 %
        // of the strongest.
        //
        // Still the left channel, and that is deliberate: it is the left channel
        // the instrument puts out rather than the left capsule, which is what
        // this used to read and what it should always have read. Written as a
        // pair of gains rather than as mid plus side so that at width 0.5 the
        // two are exactly 1 and 0 and the answer is bit-identical to the
        // capsule term - the octave transform reads this function, and a
        // difference in the last place there would move a latched mode. It
        // does: taking the louder of the two finished channels instead moves
        // the chu-daiko's handover from Octave Body 0.359 to 0.209.
        const float propagating = 0.35f * efficiency * propagatingSpread;
        const float observedLeft =
            nearField * shapeMic * std::cos (orderFloat * drum.micAngleLeft)
                * proximity
            + propagating;
        const float observedRight =
            nearField * shapeMic * std::cos (orderFloat * drum.micAngleRight)
                * proximity
            + propagating;
        const float ownGain = 0.5f + drum.stereoWidth;
        const float otherGain = 0.5f - drum.stereoWidth;
        amplitude = std::abs (
            drive * (ownGain * observedLeft + otherGain * observedRight));
    }

    if (! (omega > 0.0f) || ! (decay > 0.0f))
        return result;

    // The stroke that drives it. Everything above is the receptance of one mode
    // to a unit impulse; a bachi is not an impulse, it is a force pulse a
    // millisecond or so long, and a mode whose period is not much longer than
    // that never receives the stroke at all. Leaving it out meant the readout
    // compared the modes of a drum struck by something no player owns: measured
    // on the chu-daiko with a felt beater, where the contact runs to 4.7 ms, it
    // put the (1,2) mode nine decibels above where the rendered take has it.
    // See contactSpectrum.
    amplitude *= contactSpectrum (omega * drum.contactSeconds);

    result.frequencyHz = omega / (2.0f * piFloat);
    result.decayRate = decay;
    result.amplitude = amplitude;
    // What a partial is worth over the window a struck note's pitch is taken
    // from. A mode leaves A exp(-d t) sin(omega t) behind it, and the size of
    // that over [t0, t1] - which is what any measurement of "the strongest
    // partial" reads, however it is windowed - is A/d times the difference of
    // the two exponentials. Both halves matter and they pull opposite ways: the
    // loudest mode of a large drum is also the one the mounting empties first,
    // and a quieter mode that outlasts it by three times is the one a listener
    // ends up naming the drum by.
    result.weight = amplitude / decay
                  * (std::exp (-decay * pitchWindowStart)
                     - std::exp (-decay * pitchWindowEnd));

    // And the same quantity in nepers, written so that it survives where the
    // line above does not.
    //
    // On a very small head at the tension ceiling every mode of the drum is
    // emptied long before the pitch window opens: at 15 cm, Head Tension 1.0,
    // a thin film and Pitch +12, the okedo pad's slowest mode decays at nine
    // hundred inverse seconds, so exp(-d t0) and exp(-d t1) both underflow to
    // exactly zero and every weight on the drum is zero. A comparison on
    // `weight` then has nothing to choose between, accepts no mode, and reports
    // no pitch at all - the panel read 0.00 Hz for a drum that plainly sounds.
    //
    // exp(-d t0) - exp(-d t1) is exp(-d t0) (1 - exp(-d (t1 - t0))), and both
    // factors have well-behaved logarithms however large d is: the first is a
    // product rather than an exponential, and the second tends to zero from
    // below as log1p of something that has itself underflowed. Nothing here is
    // an approximation of the line above - it is the same number, taken through
    // the exponents instead of through the exponentials, and it only decides
    // anything where that line has lost all of its digits. See soundingMode.
    const double decayed = static_cast<double> (decay);
    const double surviving = -std::expm1 (
        -decayed * static_cast<double> (pitchWindowEnd - pitchWindowStart));
    result.logWeight = static_cast<float> (
        std::log (static_cast<double> (amplitude)) - std::log (decayed)
        - decayed * static_cast<double> (pitchWindowStart)
        + std::log (surviving));
    return result;
}

// Which of a drum's modes is the one it is heard at: the loudest of them over
// the window above, with nothing excluded.
//
// There used to be a bound - first twice the fundamental's wavenumber, then the
// head's third radial order - and it is gone, because it was never a statement
// about which modes can carry a pitch. It was a guard against this comparison
// being wrong above it, and the reason it looked wrong was an artefact of how
// the comparison was checked rather than anything in the weights.
//
// What was measured is this. Rendering a take and reading the level at each
// mode's frequency with a 0.9 s window, the weights below appeared to drift
// several decibels high by the fourth radial order. They do not. The attack
// glide leaves the head stretched for a good part of that window, so every
// membrane partial sits sharp of where it settles - by the same *ratio*,
// because a tension shift scales the whole head at once. Nine cents is a
// quarter of a hertz at 68 Hz and well inside the window's 1.1 Hz resolution;
// the same nine cents is 1.2 Hz at 230 Hz and a whole bin away from it, so a
// probe parked on the settled frequency reads a high mode ten decibels down
// while a low one loses half a decibel. Measured where each partial actually
// sounds, over the strike position, both microphone controls, Pitch and Head
// Tension on all four drums, the weights are flat to about 1.5 dB across the
// whole resolved bank out to the ninth radial order. There is nothing up there
// to guard against, and a bound cost real answers: the o-daiko struck at its
// middle is heard at its (0,3) lower branch and the chu-daiko with the pair
// backed off is heard at its (1,3), and both were excluded.
//
// The glide is why nothing in this function accounts for the glide. It moves
// every mode by the same ratio, so it cannot change which of them is loudest.
//
// On the o-daiko struck at the very centre three partials land within a decibel
// of one another and the ranking is inside the weights' own accuracy; that drum
// has no single pitch there, and no weighting can give it one.
//
// This is not the same question as which mode is lowest, and on half of this
// family it is not the same answer. The (0,1) pair's lower branch moves the two
// heads against each other, so it displaces no net air, radiates almost
// nothing, and reaches the pair only through the near field - and on a drum big
// enough for that mode to sit near the mounting's corner it is also the mode
// the stand empties first. What is left ringing is the (1,1) mode, a fifth and
// a half above it, and that is the pitch the drum is heard at. On a small
// tightly laced head the fundamental sits far above the corner, keeps its ring,
// and wins by fifteen decibels. Both are the same physics read at two sizes.
//
// `ceilingHz` is the one thing that is excluded, and it is not a claim about
// which modes can carry a pitch - it is a claim about which modes exist in the
// audio at all. buildVoiceModes drops every mode at or above 0.98 of Nyquist,
// because configureResonator cannot make one, so a comparison run over the
// whole bank can name a partial the render has already thrown away. It did: on
// a 15 cm head at the tension ceiling with a thin film and Pitch +12, the top
// pad's own fundamental is 25565 Hz, and the readout named it at a 48 kHz host
// where nothing at or above 23520 Hz is ever instantiated.
//
// The readout passes the renderer's cutoff and the octave transform passes
// infinity. That is two behaviours in one function and it is deliberate: which
// mode an instrument is tuned by is a property of the instrument, and a
// keyboard that retuned itself when the host changed its clock would be a far
// worse defect than the one this bound fixes. It is also a bound rather than a
// re-ranking - at infinity this is exactly the function it was - so the tuning
// path is bit-identical to there being no bound at all.
//
// If nothing survives it, nothing is returned: `frequencyHz` stays zero, and
// zero is the caller's marker for a drum with no membrane tone at this sample
// rate. Naming the lowest mode anyway would be the defect rather than the fix -
// the render contains no partial there, and the drum genuinely has no pitch at
// that rate. See DrumMeasurements::soundingHz.
TaikoEngine::SoundingMode TaikoEngine::soundingMode (const DrumState& drum,
                                                     float strikeRadius,
                                                     float ceilingHz) noexcept
{
    SoundingMode best;
    bool found = false;
    float bestLogWeight = -std::numeric_limits<float>::infinity();

    for (int entryIndex = 0; entryIndex < modeEntryCount; ++entryIndex)
    {
        const auto& entry = membraneModes()[static_cast<std::size_t> (entryIndex)];
        const int branches = entry.circumferentialOrder == 0 ? 2 : 1;

        for (int branch = 0; branch < branches; ++branch)
        {
            const auto observation =
                observeMode (drum, entryIndex, branch, strikeRadius);

            if (! (observation.frequencyHz > 0.0f))
                continue;

            // Exactly the test buildVoiceModes makes on the same frequency, so
            // the set compared here is the set that will be rendered.
            if (observation.frequencyHz >= ceilingHz)
                continue;

            // The comparison is on `weight`, and it is only on `logWeight` when
            // `weight` has run out of exponent - on a drum whose every mode is
            // emptied before the pitch window opens, where every weight is
            // exactly zero and there is otherwise nothing to choose between
            // them. Written this way rather than as a comparison on `logWeight`
            // alone so that wherever this function had an answer before it
            // returns the same one: the two orderings agree mathematically and
            // to a few units in the last place numerically, and a comparison
            // that changed in the last place would be a comparison that could
            // hand the octave transform a different latched mode.
            //
            // The first valid mode is always taken, so a drum that sounds is
            // never reported as having no pitch. It used to be: nothing was
            // accepted unless it beat a zero, and on a 15 cm head at the
            // tension ceiling nothing did.
            const bool better =
                ! found
                || observation.weight > best.weight
                || (! (best.weight > 0.0f) && ! (observation.weight > 0.0f)
                    && observation.logWeight > bestLogWeight);

            if (! better)
                continue;

            found = true;
            best.frequencyHz = observation.frequencyHz;
            best.weight = observation.weight;
            bestLogWeight = observation.logWeight;
            best.identity.entryIndex = static_cast<std::uint8_t> (entryIndex);
            best.identity.branch = static_cast<std::uint8_t> (branch);
        }
    }

    return best;
}

// The highest frequency a resonator will ever be built at.
//
// Written once and read by both sides of the question so they cannot drift:
// configureResonator refuses `frequencyHz >= 0.98 * nyquist` and returns a
// silent biquad, buildVoiceModes skips the mode before it gets that far, and
// soundingMode uses this to keep the readout inside the same set. The 0.98 is
// configureResonator's, not a margin added here - a pole that close to the
// Nyquist limit is already a resonator with no cycles left in it.
float TaikoEngine::renderedModeCeilingHz (double sampleRateHz) noexcept
{
    const auto clamped = std::clamp (sampleRateHz, minimumSupportedSampleRate,
                                     maximumSupportedSampleRate);
    const auto rate = static_cast<float> (clamped);
    return 0.5f * rate * 0.98f;
}

// Where the octave transform reads a pitch, and where the readout does.
//
// The transform is anchored at the centred stroke on purpose. An off-centre
// strike really does excite a different balance of modes and really is heard at
// a different pitch - that physics is in observeMode above and stays there - but
// making the octave solve follow it would turn Strike Position into a tuning
// control: nudging the stick towards the middle of the head would retune the
// whole keyboard and re-solve every drum's size. So the solve always asks what
// the drum sounds under a centred full open stroke, and Strike Position moves
// only what is heard and what is reported.
float TaikoEngine::tuningStrikeRadius() noexcept
{
    return clampFloat (strikeProfile (Articulation::Don).radius, 0.0f, 0.995f);
}

float TaikoEngine::readoutStrikeRadius (const EngineParameters& parameters) noexcept
{
    // The same arithmetic trigger() uses to place a Don, minus the humanising
    // jitter: the readout describes the stroke the controls ask for, not the
    // scatter around it.
    const float offset = clampFloat (parameters.strikePosition, -1.0f, 1.0f) * 0.32f;
    return clampFloat (strikeProfile (Articulation::Don).radius + offset, 0.0f,
                       0.985f);
}

// The mode each octave of the family is tuned by.
//
// This is the fix for a regression the octave solve shipped with. Solving the
// transform against soundingMode() - an argmax over the drum's modes - is what
// puts the four heard octaves where they belong, but an argmax is a
// discontinuous function of everything that feeds it. Two of this family's modes
// are within a decibel of each other over a wide stretch of the controls, and
// wherever they crossed, the reference drum's reported pitch stepped by up to a
// tenth of an octave and every transformed octave re-solved for radically
// different geometry. Measured on the shipping code at factory settings, a
// single 0.01-semitone step of Pitch at 7.48 -> 7.49 dropped the chu-daiko from
// 183.8 Hz to 100.6 Hz - 1043 cents - and moved its head from 0.238 m to
// 0.404 m, so the timbre lurched with the pitch. Head Tension crossed the same
// balance at 0.4142 and 0.9175, Head Diameter at 0.5958, and Resonant Tension
// and Mic Spread further in, all of them doing the same thing.
//
// So the identity is latched instead of re-chosen. Which mode an instrument is
// heard at is a property of the instrument - the o-daiko and the chu-daiko are
// named by their (1,1), because their own fundamentals displace no net air and
// the mounting empties them in half a second, while the okedo and the shime are
// named by their fundamentals - and that is a fact about the four drums the
// family table describes, not about where the player has left Head Tension. So
// it is read off those four drums, at the factory controls, and the solve then
// tracks that same mode as the player's controls move.
//
// The one control it does depend on is Octave Body, and it has to: that control
// decides what the four drums *are*. At Family they are four instruments and the
// chu-daiko is heard at its (1,1); at Tuned they are one o-daiko retuned, and an
// o-daiko taken up an octave is a drum whose own fundamental has climbed clear
// of the mounting and become the loudest thing it has - which is the mode
// handover the README describes as the reason the first octave at Tuned costs
// x13.49 in tension rather than x4. No single assignment can serve both ends:
// tuning the chu-daiko's fundamental onto the octave breaks the family grid, and
// tuning a retuned o-daiko's (1,1) onto it makes the first octave at Tuned a
// step of 157 cents. So this reads the identity off the drum the family would
// build at the current Octave Body, and one handover survives, in the control
// whose whole job is to change which instrument an octave plays.
//
// Everything else the latch has to be, it is, because past Octave Body it is a
// constant: it cannot depend on what was struck, because no stroke is involved;
// a host that sets the parameters in a different order lands on the same drum,
// because this is a pure function of one of them; and it persists across
// setParameters for the same reason. Anything narrower - latching on the first
// setParameters, or latching from the current controls with only Pitch held back
// - is either path dependent or still discontinuous in whatever control was left
// live.
//
// Which drum to read it off is the one subtlety. Not the untransformed
// instrument: at Tuned that is the reference drum itself, and it is heard at its
// (1,1), which is exactly the answer that costs the first octave 1043 cents of
// the handover. It has to be the instrument as the key will actually leave it,
// and that is not known until the solve has run. So it is taken at the transform
// that would put the drum's *ideal* membrane fundamental on its octave - a
// closed form, continuous in the controls, and within a per cent of the answer
// at Family, where the drums are already an octave apart, and exactly an octave
// of tension at Tuned, where they are not.
TaikoEngine::ModeIdentity TaikoEngine::tuningModeFor (int octaveOffset,
                                                      float octaveBody) noexcept
{
    const int clamped =
        std::clamp (octaveOffset, lowestOctaveOffset, highestOctaveOffset);

    // The family as the table builds it, at the factory controls. Nothing the
    // player has done to the drum reaches this.
    auto controls = sanitise (EngineParameters {});
    controls.octaveBody = clampFloat (octaveBody, 0.0f, 1.0f);
    const float body = controls.octaveBody;

    DrumState referenceState;
    resolveDrumGeometry (controls, 1.0f, 1.0f, 1.0f, referenceState);

    // No ceiling on either of these. Which mode an instrument is tuned by is a
    // property of the instrument; putting the render's Nyquist cutoff on it
    // would let the host's clock retune the keyboard. See soundingMode.
    constexpr float noCeiling = std::numeric_limits<float>::infinity();

    if (clamped == 0)
        return soundingMode (referenceState, tuningStrikeRadius(), noCeiling).identity;

    const auto applied = parametersForOctave (controls, clamped);
    DrumState untransformed;
    resolveDrumGeometry (applied, 1.0f, 1.0f, 1.0f, untransformed);

    // How much transform it takes to put the ideal membrane fundamental - wave
    // speed over radius, up to a constant - where the key asks. Both halves of
    // the transform move that quantity by exactly 2^amount, whatever the mixture,
    // so this inverts in closed form.
    float amount = static_cast<float> (clamped);
    const float idealReference = referenceState.waveSpeed / referenceState.radius;
    const float idealHere = untransformed.waveSpeed / untransformed.radius;

    if (idealReference > 0.0f && idealHere > 0.0f)
        amount += std::log2 (idealReference / idealHere);

    DrumState canonical;
    resolveDrumGeometry (applied, std::exp2 (-body * amount),
                         std::exp2 (2.0f * (1.0f - body) * amount), 1.0f, canonical);

    return soundingMode (canonical, tuningStrikeRadius(), noCeiling).identity;
}

// The player's controls carried onto whichever of the four drums this octave
// plays. Head Diameter is a ratio, because it is a length and the family spans
// a factor of three in it; the other four are offsets in control units, which
// for Head Tension is a constant ratio in newtons per metre because that
// control's own map is geometric. Either way a control moved from the factory
// setting moves the whole family by the same amount rather than moving one drum
// out of the set.
//
// At Octave Body 0 the four drums collapse onto the reference one and this
// returns the parameter block untouched, which is what "the same drum, retuned"
// has to mean. At octave 0 it returns it untouched at every Octave Body,
// because the reference drum is the o-daiko the controls describe.
EngineParameters TaikoEngine::parametersForOctave (const EngineParameters& applied,
                                                   int octaveOffset) noexcept
{
    const auto& reference = getDrumDescription (0);
    const auto& drum = getDrumDescription (octaveOffset);
    const float body = clampFloat (applied.octaveBody, 0.0f, 1.0f);

    if (! (body > 0.0f))
        return applied;

    // Geometric on the diameter and linear on the rest, each in the space its
    // own control is even in.
    const float diameterRatio =
        std::pow (drum.headDiameterMetres / reference.headDiameterMetres, body);

    EngineParameters result = applied;
    result.headDiameter =
        clampFloat (applied.headDiameter * diameterRatio, 0.15f, 1.80f);
    result.bodyDepth = clampFloat (
        applied.bodyDepth + body * (drum.bodyDepth - reference.bodyDepth), 0.0f, 1.0f);
    result.tension = clampFloat (
        applied.tension + body * (drum.tension - reference.tension), 0.0f, 1.0f);
    result.headMaterial = clampFloat (
        applied.headMaterial + body * (drum.headMaterial - reference.headMaterial),
        0.0f, 1.0f);
    result.shellMaterial = clampFloat (
        applied.shellMaterial + body * (drum.shellMaterial - reference.shellMaterial),
        0.0f, 1.0f);
    return result;
}

TaikoEngine::DrumState TaikoEngine::resolveDrumFor (const EngineParameters& raw,
                                                    float pitchBendSemitones,
                                                    int octaveOffset) noexcept
{
    const auto controls = sanitise (raw);
    const auto applied = parametersForOctave (controls, octaveOffset);
    DrumState drum;

    const auto octave = static_cast<float> (
        std::clamp (octaveOffset, lowestOctaveOffset, highestOctaveOffset));

    // What is left for the transform to do, and which axis it does it on. Going
    // up the keyboard is no longer a rescaling: the drum has already changed
    // into a different instrument, and all that remains is to bring that
    // instrument to the pitch the key asks for - which is what tuning a drum
    // is. Octave Body chooses whether that residual is taken as head tension
    // (0, and then it is the whole octave, because the drum has not changed) or
    // as the drum's own size (1, and then it is a couple of per cent).
    const float body = applied.octaveBody;

    // The pitch control and the wheel are head tension, because that is what
    // tuning a drum is.
    const float semitones = applied.pitch + pitchBendSemitones;
    const float tensionPitchFactor = std::exp2 (2.0f * semitones / 12.0f);

    // Halving the drum and quadrupling the tension used to be written down as
    // landing on exactly the same pitch, and that was only ever true of the
    // ideal membrane frequency - a quantity that is never audible on its own.
    // The air load goes as rho_air a / sigma while the cavity goes as
    // rho c^2 / L. Neither of them scales with the transform, so the two ways
    // of buying an octave land a long way apart in the pitch a listener names,
    // and the transform that doubles the ideal frequency does not double the
    // real one. Measured on the shipping code the octave steps at Octave Body
    // 1.0 were 1545 / 1443 / 1353 / 1286 / 1244 cents - a keyboard whose bottom
    // octave is nearly three semitones wide.
    //
    // So the tuning is solved for rather than written down: how much transform,
    // in octaves, puts the mode this drum is heard at exactly `octave` octaves
    // above the mode the reference drum is heard at. The mixture is untouched -
    // the solve moves along the axis Octave Body already chose, so at Octave
    // Body 0 the radius still never moves and at 1.0 the tension still never
    // does - and it is the principle the head's stiffness stretch already
    // follows and the README already states: a drum is tuned by the pitch it
    // sounds.
    //
    // The mode it is heard at, and not its loaded fundamental. Those are the
    // same thing on a small tightly laced head and they are not on a large
    // slack one: the (0,1) pair's lower branch moves the two heads against each
    // other, displaces no net air, and reaches the pair only through the near
    // field, so on a drum whose fundamental sits down near the mounting's
    // corner that mode is emptied by the stand in half a second while the (1,1)
    // mode a fifth and a half above it rings for two. Solving against the lower
    // branch put the four drums' inaudible fundamentals on exact octaves and
    // left what anyone actually hears stepping 0 / 11.7 / 14.3 / 26.3
    // semitones. See soundingMode.
    //
    // The reference is the drum the controls describe, resolved untransformed.
    // It used to be this octave's own untransformed drum, which was the same
    // thing when every octave was one drum rescaled; now that the four octaves
    // are four instruments it is not, and taking each drum's own pitch as the
    // reference would have tuned each of them an octave above itself and left
    // the keyboard reading the family's own intervals rather than octaves.
    //
    // Which mode that is, on both sides of the comparison, is latched rather
    // than taken as an argmax over the drum's modes - see tuningModeFor. It has
    // to be: an argmax is discontinuous in every control that feeds it, so
    // re-running it per parameter update made a hundredth of a semitone of Pitch
    // automation drop a drum by a tenth of an octave and re-solve its size.
    // Re-deciding it inside the bisection would be worse still, because a
    // bisection cannot bracket a step it creates itself.
    //
    // That frequency is monotone increasing in the amount of transform applied,
    // because more tension and less radius both raise every mode of the head
    // and the mixture moves them together, so a bisection on a bracket widened
    // until it straddles the answer is well posed. The bracket has to reach both
    // ways: a real drum can sound above the key it is put on as easily as below
    // it, and at Octave Body 0 the answer is still very nearly the whole octave.
    // Twenty-four halvings of a bracket that starts two octaves wide is a
    // ten-millionth of an octave, well under a thousandth of a cent.
    //
    // Inside the loop is the head, the air behind it, the mounting and the
    // microphones; the shell is computed once, from the answer. It is not cheap
    // - resolving all four octaves takes about 200 microseconds - but a drum
    // resolve happens when a control moves or the wheel passes a tenth of a
    // cent, which is at most once per block and never per sample.
    const auto transformed = [&applied, body, tensionPitchFactor] (float amount,
                                                                   DrumState& state)
    {
        resolveDrumGeometry (applied, std::exp2 (-body * amount),
                             std::exp2 (2.0f * (1.0f - body) * amount),
                             tensionPitchFactor, state);
    };

    // The reference drum is the controls themselves, so at octave zero the
    // target is the pitch this very drum already sounds and the transform is the
    // identity for every Octave Body. Taking that case out keeps the reference
    // octave bit-identical rather than nearly so - and it is also the only place
    // the reference resolve can be skipped.
    if (octaveOffset == 0)
    {
        transformed (0.0f, drum);
    }
    else
    {
        // The two latched identities: the mode the reference drum is tuned by
        // and the mode this octave's instrument is tuned by. They are not the
        // same row of the table across this family - the two large drums are
        // named by their (1,1) and the two small ones by their fundamentals -
        // and that difference is exactly what puts the four heard pitches on
        // octaves. What matters here is only that neither of them is re-decided
        // when a control moves.
        const auto referenceIdentity = tuningModeFor (0, applied.octaveBody);
        const auto identity = tuningModeFor (octaveOffset, applied.octaveBody);
        // Always the centred stroke, on both sides. Strike Position moves what
        // the drum is heard at, and it must not move what it is tuned to.
        const float strikeRadius = tuningStrikeRadius();

        DrumState referenceState;
        resolveDrumGeometry (controls, 1.0f, 1.0f, tensionPitchFactor,
                             referenceState);
        const float reference =
            observeMode (referenceState, referenceIdentity.entryIndex,
                         referenceIdentity.branch, strikeRadius)
                .frequencyHz;

        if (! (reference > 0.0f))
        {
            transformed (octave, drum);
        }
        else
        {
            // The bracket is carried as a pair of resolved drums rather than as
            // a pair of numbers, and the one that wins is the drum that is
            // handed on. Re-resolving the winning amount at the end instead
            // would be the same arithmetic written twice, and the two copies do
            // not always round the same way: where the tracked mode steps - see
            // below - a difference in the last place of the exponentials is
            // enough to put the final resolve the other side of the step from
            // the trial that chose it, and the drum that ships is then not the
            // drum that was measured.
            DrumState lowState;
            DrumState highState;

            // In octaves above where the reference drum sounds, so the answer
            // wanted is exactly `octave` and the function is increasing in the
            // amount of transform whichever side of zero it starts.
            const auto reached = [&transformed, reference, identity, strikeRadius] (
                                     float amount, DrumState& state)
            {
                transformed (amount, state);
                const float sounded =
                    observeMode (state, identity.entryIndex, identity.branch,
                                 strikeRadius)
                        .frequencyHz;
                return sounded > 0.0f ? std::log2 (sounded / reference) : -100.0f;
            };

            // One octave either side of the answer if the drum were already in
            // tune, which straddles it for every drum in the table at Octave
            // Body 1 and is widened for the settings where it does not - at
            // Octave Body 0 the drum has not changed at all and the whole octave
            // has to come out of the tension.
            float low = octave - 1.0f;
            float high = octave + 1.0f;
            float atLow = reached (low, lowState);
            float atHigh = reached (high, highState);

            for (int widen = 0; widen < 5 && atLow > octave; ++widen)
            {
                high = low;
                atHigh = atLow;
                highState = lowState;
                low -= 1.0f;
                atLow = reached (low, lowState);
            }

            for (int widen = 0; widen < 5 && atHigh < octave; ++widen)
            {
                low = high;
                atLow = atHigh;
                lowState = highState;
                high += 1.0f;
                atHigh = reached (high, highState);
            }

            DrumState middleState;

            for (int iteration = 0; iteration < 24; ++iteration)
            {
                const float middle = 0.5f * (low + high);
                const float here = reached (middle, middleState);

                if (here < octave)
                {
                    low = middle;
                    atLow = here;
                    lowState = middleState;
                }
                else
                {
                    high = middle;
                    atHigh = here;
                    highState = middleState;
                }
            }

            // Whichever end of the converged bracket is nearer, rather than its
            // midpoint. Wherever the tracked mode is continuous in the transform
            // the two ends agree to a ten-millionth of an octave and this is the
            // same answer either way.
            //
            // Where they do not agree, the bracket has not failed to converge -
            // it has found a step in the quantity being solved for, and the drum
            // is saying that the octave it is being asked for does not exist. At
            // the transform where the air column reaches its quarter-wave the
            // two heads stop being tied together; the lower branch becomes the
            // far head's alone, a stroke on the batter head can no longer sound
            // it, and the mode being tracked steps up to the batter head's own.
            // There is a band of pitches on the far side of that step which no
            // amount of transform reaches. The near side of it is the closest an
            // octave can be got to, and taking the midpoint would land past it.
            const bool takeLow =
                std::abs (atLow - octave) <= std::abs (atHigh - octave);
            drum = takeLow ? lowState : highState;
        }
    }

    // The wooden shell's ring modes. This is the standard thin-cylinder result
    // f_n = n(n^2-1)/sqrt(n^2+1) * h/(2 pi R^2) * sqrt(E/(12 rho (1-nu^2))),
    // so the shell material moves both the frequencies and their spacing.
    const float youngsModulus = lerp (4.0e9f, 14.0e9f, applied.shellMaterial);
    const float woodDensity = lerp (450.0f, 850.0f, applied.shellMaterial);
    constexpr float poisson = 0.30f;
    const float plateSpeed = std::sqrt (
        youngsModulus / (12.0f * woodDensity * (1.0f - poisson * poisson)));
    const float wallThickness = 2.0f * drum.radius
                              * (0.035f + 0.045f * applied.shellMaterial);
    const float geometry = wallThickness / (2.0f * piFloat * drum.radius * drum.radius);
    // A drum shell is a thick, short, heavily loaded piece of wood clamped at
    // both ends by the hoops, not a free bar, so it does not ring for long. It
    // used to, and on a large drum its ring outlasted the head's - which put a
    // wooden pitch on top of the drum where the body should only be adding
    // weight, and left a hand laid on the head unable to damp what anyone could
    // still hear, because a hand on the head does not touch the body.
    const float shellQ = 12.0f + 40.0f * applied.shellMaterial;

    for (int index = 0; index < shellResonatorCount; ++index)
    {
        const auto n = static_cast<float> (index + 2);
        const float coefficient = n * (n * n - 1.0f) / std::sqrt (n * n + 1.0f);
        const float frequency = coefficient * geometry * plateSpeed;
        drum.shellFrequencies[static_cast<std::size_t> (index)] = frequency;
        drum.shellDecays[static_cast<std::size_t> (index)] = piFloat * frequency / shellQ;
    }

    // Half the mass of the cylinder wall, which is what a ring mode actually
    // has to move. A carved zelkova body is heavy and barely responds; a light
    // laminated shell rings far more for the same stroke.
    drum.shellModalMass = std::max (
        0.5f * woodDensity * 2.0f * piFloat * drum.radius * drum.depth * wallThickness,
        0.05f);

    drum.shellLevel = applied.shellResonance;

    return drum;
}

void TaikoEngine::refreshDrumIfNeeded() noexcept
{
    if (drumCacheValid_)
        return;

    for (int octave = lowestOctaveOffset; octave <= highestOctaveOffset; ++octave)
    {
        const auto index = static_cast<std::size_t> (octave - lowestOctaveOffset);
        drumCache_[index] = resolveDrum (octave);
    }

    drumCacheBend_ = pitchBend_;
    drumCacheValid_ = true;

    // A canonical bank must follow structural automation just as the cache
    // used for a new strike does. Rebuild only banks that already exist;
    // untouched keyboard octaves remain lazy. ensurePhysicalDrum maps every
    // running mode by stable physical ID, so this changes coefficients and
    // observation without resetting the tail.
    for (int octave = lowestOctaveOffset; octave <= highestOctaveOffset; ++octave)
    {
        const auto index = static_cast<std::size_t> (octave - lowestOctaveOffset);
        if (physicalDrums_[index].modeCount > 0
            && physicalDrums_[index].configurationRevision
                   != physicalConfigurationRevision_)
            ensurePhysicalDrum (octave, drumCache_[index]);
    }
}

// ---------------------------------------------------------------------------
// The stick meeting the head
// ---------------------------------------------------------------------------

void TaikoEngine::drumContactTerms (const DrumState& drum, float& strikerMass,
                                    float& impedance) noexcept
{
    strikerMass = bachiMass
                * clampFloat (drum.radius / referenceRadius, minimumBachiScale,
                              maximumBachiScale);
    // Legacy per-unit-length mobility calibration used only to scale the
    // observed statistical tail and the direct-path contact estimate. It is
    // not loaded into the reciprocal resolved-mode solve: a physical point
    // impedance needs a measured contact length or complex mobility fit.
    impedance = 8.0f * std::sqrt (drum.tension * drum.batterDensity);
}

float TaikoEngine::contactCollisionMass (const DrumState& drum,
                                         const StrikeProfile& profile,
                                         float strikeRadius,
                                         float strikerMass) noexcept
{
    const float area = piFloat * drum.radius * drum.radius;
    const float rho = clampFloat (strikeRadius, 0.0f, 0.995f);
    const float coupling = profile.membraneGain * profile.levelScale;
    double inverseHeadMass = 0.0;

    for (const auto& entry : membraneModes())
    {
        const int order = entry.circumferentialOrder;
        const double lambda = entry.besselZero;
        const double shape = besselJ (order, lambda * rho);
        const double boundary = besselJ (order + 1, lambda);
        const double angularNorm = order == 0 ? 1.0 : 0.5;
        const double modalMass = angularNorm * static_cast<double> (area)
                               * boundary * boundary * drum.batterDensity;
        if (modalMass > 0.0)
            inverseHeadMass += static_cast<double> (coupling) * coupling
                             * shape * shape / modalMass;
    }

    const double inverseStickMass = 1.0 / std::max (
        static_cast<double> (strikerMass), 1.0e-6);
    if (! (inverseHeadMass > 0.0) || ! std::isfinite (inverseHeadMass))
        return static_cast<float> (1.0 / inverseStickMass);
    return static_cast<float> (1.0 / (inverseStickMass + inverseHeadMass));
}

void TaikoEngine::solveContact (float collisionMass, float targetImpedance,
                                const StrikeProfile& profile, float bachiHardness,
                                float impactSpeed, float& contactSeconds,
                                float& peakForce) noexcept
{
    const float stiffness = contactStiffnessFor (profile, bachiHardness);

    // Hertz impact of a rounded tip against a stiff surface. The contact time
    // falls as the fifth root of the impact speed, which is the whole reason a
    // hard stroke is brighter and not merely louder.
    const float speed = std::max (impactSpeed, 0.05f);
    const float mass = std::max (collisionMass, 1.0e-4f);
    const float hertzTime = 2.94f
                          * std::pow (5.0f * mass / (4.0f * stiffness), 0.4f)
                          * std::pow (speed, -0.2f);

    // Residual/direct-path duration prior. The dynamic contact below does not
    // use this as a mechanical resistance; an uncalibrated memoryless load
    // captures soft and edge strokes instead of returning high-mode energy.
    const float impedanceTime = mass / std::max (targetImpedance, 1.0f);

    contactSeconds = std::sqrt (hertzTime * hertzTime + impedanceTime * impedanceTime);
    contactSeconds = clampFloat (contactSeconds, 4.0e-5f, 0.05f);

    // Impulse of the collision, spread over the sin^1.5 Hertz force pulse.
    // The integral of sin(x)^1.5 over one arch is 2.3963.
    const float impulse = mass * speed * (1.0f + restitution);
    peakForce = impulse * piFloat / (2.3963f * contactSeconds);
}

float TaikoEngine::contactStiffnessFor (const StrikeProfile& profile,
                                        float bachiHardness) noexcept
{
    return clampFloat (
        geometricLerp (minimumContactStiffness, maximumContactStiffness,
                       bachiHardness) * profile.hardnessScale,
        minimumContactStiffness * 0.25f, maximumContactStiffness * 4.0f);
}

float TaikoEngine::measureContactSeconds (Articulation articulation, int octaveOffset,
                                          float velocity) const noexcept
{
    return measureContact (applied_, articulation, octaveOffset, velocity);
}

float TaikoEngine::measureContact (const EngineParameters& raw,
                                   Articulation articulation, int octaveOffset,
                                   float velocity) noexcept
{
    const auto parameters = sanitise (raw);
    const auto& profile = strikeProfile (articulation);
    const float shaped = clampFloat (velocity, 0.0f, 1.0f);
    const float normalised = lerp (0.72f, shaped, parameters.velocityDepth);
    const float speed =
        geometricLerp (minimumImpactSpeed, maximumImpactSpeed, normalised);

    float strikerMass = 0.0f;
    float impedance = 0.0f;
    const auto drum = resolveDrumFor (parameters, 0.0f, octaveOffset);
    drumContactTerms (drum, strikerMass, impedance);
    const float radius = clampFloat (
        profile.radius + parameters.strikePosition * 0.32f, 0.0f, 0.985f);
    const float collisionMass = contactCollisionMass (drum, profile, radius,
                                                       strikerMass);

    float contactSeconds = 0.0f;
    float peakForce = 0.0f;
    solveContact (collisionMass, impedance, profile, parameters.bachiHardness, speed,
                  contactSeconds, peakForce);
    return contactSeconds;
}

// ---------------------------------------------------------------------------
// Building the modal bank for one stroke
// ---------------------------------------------------------------------------

void TaikoEngine::configureResonator (Resonator& resonator, float frequencyHz,
                                      float decayRate, float gain) const noexcept
{
    const auto rate = static_cast<float> (sampleRate_);
    const float nyquist = 0.5f * rate;

    if (! (frequencyHz > 0.0f) || frequencyHz >= nyquist * 0.98f)
    {
        resonator.a1 = 0.0;
        resonator.a2 = 0.0;
        resonator.b0 = 0.0;
        return;
    }

    // Solved in double for the same reason the resonator runs in double: at
    // fifty hertz against a high sample rate, cos(omega) differs from one by
    // less than a float mantissa can hold, and rounding the coefficient is
    // itself enough to mistune the drum audibly.
    const auto omega = 2.0 * static_cast<double> (piFloat)
                     * static_cast<double> (frequencyHz) / sampleRate_;
    const auto radius = std::exp (-static_cast<double> (decayRate) / sampleRate_);
    resonator.a1 = -2.0 * radius * std::cos (omega);
    resonator.a2 = radius * radius;
    // Impulse-invariant scaling: with this numerator the resonator's impulse
    // response is exactly the sampled r^n sin(omega n), so the drive gain the
    // caller computes carries its physical meaning through unchanged.
    resonator.b0 = static_cast<double> (gain) * std::sin (omega);
}

void TaikoEngine::setPalmDecay (Mode& mode, float amplitudeDecay) noexcept
{
    const float requested = std::isfinite (amplitudeDecay)
        ? std::max (amplitudeDecay, 0.0f) : 0.0f;
    if (requested == mode.appliedPalmDecay)
        return;

    auto& resonator = mode.resonator;
    const double displacement = resonator.y1;
    double sine = resonator.b0;
    double cosine = 1.0;
    double velocity = 0.0;
    if (mode.poleRadius > 0.0 && std::abs (resonator.b0) > 1.0e-12
        && mode.liveOmega > 0.0)
    {
        cosine = -resonator.a1 / (2.0 * mode.poleRadius);
        const double quadrature =
            (displacement * cosine - mode.poleRadius * resonator.y2)
            / sine;
        velocity = mode.liveOmega * quadrature
                 - static_cast<double> (mode.decayRate + mode.appliedPalmDecay)
                       * displacement;
    }
    else
    {
        velocity = (displacement - resonator.y2) * sampleRate_;
    }

    mode.appliedPalmDecay = requested;
    if (! (mode.liveOmega > 0.0))
        return;

    if (std::abs (sine) <= 1.0e-12)
    {
        const double angle = mode.liveOmega / sampleRate_;
        sine = std::sin (angle);
        cosine = std::cos (angle);
    }

    // Keep the mode's damped frequency and change only its pole radius. This
    // applies the palm continuously on every recurrence sample instead of as a
    // 32-sample velocity kick, which can repeatedly land at a high mode's
    // turning points and almost miss it at one host rate while overdamping it
    // at another.
    const double totalDecay = static_cast<double> (
        mode.decayRate + mode.appliedPalmDecay);
    const double radius = std::exp (-totalDecay / sampleRate_);
    resonator.a1 = -2.0 * radius * cosine;
    resonator.a2 = radius * radius;
    resonator.b0 = sine;
    mode.poleRadius = radius;

    // Changing coefficients is a control action, not another collision. Map
    // the old physical q and qdot into the new pole so the hand cannot create a
    // displacement or velocity step when it touches or leaves the hide.
    if (std::abs (sine) > 1.0e-12 && radius > 0.0)
    {
        const double quadrature =
            (velocity + totalDecay * displacement) / mode.liveOmega;
        resonator.y2 =
            (displacement * cosine - quadrature * sine) / radius;
    }
    else
    {
        resonator.y2 = displacement - velocity / sampleRate_;
    }
}

void TaikoEngine::applyCollisionRetention (Mode& mode, float retention) noexcept
{
    auto& resonator = mode.resonator;
    const double displacement = resonator.y1;
    const double radius = mode.poleRadius;
    const double sine = resonator.b0;

    if (! (radius > 0.0) || std::abs (sine) < 1.0e-12
        || ! (mode.liveOmega > 0.0))
    {
        // Degenerate defensive path. The ordinary modes never reach it, but a
        // backward difference still preserves displacement and scales velocity
        // if hostile parameters have produced a collapsed pole.
        const double velocity = displacement - resonator.y2;
        resonator.y2 = displacement - static_cast<double> (retention) * velocity;
        return;
    }

    // q(t) = exp(-gamma t) [q0 cos(Omega t) + B sin(Omega t)]. Recover B
    // from q[n] and q[n-1], scale only qdot(0) = Omega B - gamma q0, then
    // encode the unchanged q0 and the new velocity back into q[n-1]. The live
    // radius and angular frequency were cached when these coefficients were
    // built or retuned; mode.omega is only the resting build value.
    const double cosine = -resonator.a1 / (2.0 * radius);
    const double quadrature =
        (displacement * cosine - radius * resonator.y2) / sine;
    const double dampingRatio = static_cast<double> (
        mode.decayRate + mode.appliedPalmDecay) / mode.liveOmega;
    const double retainedQuadrature =
        static_cast<double> (retention) * quadrature
        + (1.0 - static_cast<double> (retention)) * dampingRatio * displacement;

    resonator.y2 =
        (displacement * cosine - retainedQuadrature * sine) / radius;
}

std::array<float, 5> TaikoEngine::palmPatchRadii (float centreRadius,
                                                  float patchRadius) noexcept
{
    const float rho = clampFloat (centreRadius, 0.0f, 0.995f);
    const float offset = std::max (patchRadius, 0.0f);
    const float tangent = clampFloat (
        std::sqrt (rho * rho + offset * offset), 0.0f, 0.995f);
    return {{
        rho,
        clampFloat (std::abs (rho - offset), 0.0f, 0.995f),
        clampFloat (rho + offset, 0.0f, 0.995f),
        tangent,
        tangent,
    }};
}

void TaikoEngine::buildVoiceModes (Voice& voice, const DrumState& drum,
                                   const StrikeProfile& profile,
                                   float extraDamping) noexcept
{
    const auto& entries = membraneModes();
    const auto rate = static_cast<float> (sampleRate_);
    const float nyquist = 0.5f * rate;

    const float radius = drum.radius;
    const float rho = clampFloat (voice.strikeRadius, 0.0f, 0.995f);
    const float theta = voice.strikeAngle;
    const float sigmaB = drum.batterDensity;
    const float sigmaR = drum.resonantDensity;
    // Depend only on the drum, not on the mode entry or which axisymmetric
    // branch is being resolved, so each is worth resolving once rather than
    // redoing it for every entry x branch pair the loop below visits.
    const float sqrtSigmaB = std::sqrt (sigmaB);
    const float sqrtSigmaR = std::sqrt (sigmaR);
    const float area = piFloat * radius * radius;

    const float micRho = drum.micRadius / radius;
    const float micAngleL = drum.micAngleLeft;
    const float micAngleR = drum.micAngleRight;
    const float micDistance = drum.micDistanceMetres;
    // Everything that propagates out of the drum arrives at both microphones
    // alike, and spreads on the way; only the near field carries the shape of
    // the head, and that is what separates the pair.
    const float propagatingSpread = 1.0f / (1.0f + micDistance / 0.12f);

    const float edgeLoss = drum.edgeLoss * (1.0f + 3.0f * extraDamping);
    // The hide's own loss, kept as the two coefficients materialDamping sums
    // rather than as that sum, so a retuned mode can be re-damped at its new
    // frequency. The hand bears on both, exactly as it does there.
    const float handShare = 1.0f + 2.4f * extraDamping;
    const float lossOmega = 0.5f * drum.headLossFactor * handShare;
    const float lossOmegaSquared = drum.headViscousFactor * handShare;
    voice.mountLoss = drum.mountLoss;
    voice.mountCorner = drum.mountCorner;
    voice.radiusMetres = drum.radius;

    int count = 0;
    float peakMagnitude = 1.0e-12f;
    // The head's own share of that, which is what the high-frequency continuum
    // is measured against - but carrying the sample rate back out of it.
    //
    // mode.drive is a per-sample integration gain: it holds a 1/rate because
    // the resonator it feeds accumulates a force over a sample period. The
    // continuum integrates nothing - it multiplies a noise sequence whose
    // variance is already normalised to unity - so calibrating it against
    // mode.drive handed it a sample rate it has no use for, and the whole
    // region lost 6 dB per doubling of the host's clock. Multiplying the rate
    // back in measures the continuum against the modal receptance
    // shapeStrike * batterShare / (geometricMass * omega), a velocity per unit
    // force and a property of the drum alone, observed through the same
    // microphone factor the modes are observed through. The microphone factor
    // has to stay: it is the continuum's only distance dependence.
    float membranePeak = 1.0e-12f;

    // A real head is never quite uniform, so each degenerate pair sits a
    // fraction of a percent apart and beats. That asymmetry is a property of
    // the hide rather than of the stroke, so it is seeded from a fixed
    // constant: the same drum must split the same way every time it is hit.
    constexpr std::uint32_t headSeed = 0x9e3779b9u;
    constexpr float splitDepth = 0.0016f;

    for (int entryIndex = 0; entryIndex < modeEntryCount; ++entryIndex)
    {
        const auto& entry = entries[static_cast<std::size_t> (entryIndex)];
        const int order = entry.circumferentialOrder;
        const auto lambda = static_cast<float> (entry.besselZero);

        // Membrane modes: f = c * lambda / (2 pi a), opened out by the head's
        // own bending stiffness. The Bessel zeros set the wavenumbers, and
        // stiffness adds a term in the fourth power of the wavenumber to
        // omega^2, so the ratios are no longer constants of the geometry: they
        // spread with the mode's order, and they spread further the smaller and
        // the thicker the head is. Taken relative to the (0,1) mode, so the
        // pitch the drum is tuned to does not move and only the spread above it
        // does.
        const float idealBatter = drum.waveSpeed * lambda / (2.0f * piFloat * radius)
                                * stiffnessStretch (lambda, drum.stiffnessBatter);
        const float idealResonant =
            drum.resonantWaveSpeed * lambda / (2.0f * piFloat * radius)
            * stiffnessStretch (lambda, drum.stiffnessResonant);

        // Air hanging off the head as added mass. Modes with a high radial or
        // circumferential order shift less air per unit of displacement, so
        // they are loaded far less than the fundamental. A light synthetic
        // head is loaded much more than a heavy hide, which is exactly why it
        // sounds lower than its tension alone predicts.
        const float loadShape = (2.4048f / lambda) / (1.0f + 0.6f * static_cast<float> (order));
        const float loadBatter =
            1.0f / std::sqrt (1.0f + 0.85f * loadShape * airDensity * radius / sigmaB);
        const float loadResonant =
            1.0f / std::sqrt (1.0f + 0.85f * loadShape * airDensity * radius / sigmaR);

        const float omegaBatter = 2.0f * piFloat * idealBatter * loadBatter;
        const float omegaResonant = 2.0f * piFloat * idealResonant * loadResonant;

        const auto besselAtZero = static_cast<float> (besselJ (order + 1, entry.besselZero));
        const float besselSquared = std::max (besselAtZero * besselAtZero, 1.0e-9f);

        // Mode shape at the strike point and at each microphone.
        const auto shapeStrike =
            static_cast<float> (besselJ (order, entry.besselZero * rho));
        const auto shapeMic =
            static_cast<float> (besselJ (order, entry.besselZero * micRho));

        if (order == 0)
        {
            // Axisymmetric modes are the only ones that change the body's
            // volume, so they are the only ones the enclosed air can couple.
            // Projecting the uniform cavity pressure onto J0 leaves a clean
            // 4/lambda^2 weighting, which is why the fundamental is lifted so
            // much further than the modes above it.
            const float geometricMass = area * besselSquared;   // per unit density
            const float cavity = drum.cavityStiffness * 4.0f / (lambda * lambda);

            // Symmetrised by w = sqrt(sigma) q, so the two-by-two is symmetric
            // and its eigenvectors are orthonormal.
            const float diagonalB = omegaBatter * omegaBatter + cavity / sigmaB;
            const float diagonalR = omegaResonant * omegaResonant + cavity / sigmaR;
            const float offDiagonal = cavity / std::sqrt (sigmaB * sigmaR);

            for (int branch = 0; branch < 2; ++branch)
            {
                if (count >= membraneResonatorCount)
                    break;

                float eigenvalue = 0.0f;
                float vectorB = 0.0f;
                float vectorR = 0.0f;
                solveAxisymmetricBranch (diagonalB, diagonalR, offDiagonal, branch,
                                         eigenvalue, vectorB, vectorR);
                if (! (eigenvalue > 0.0f))
                    continue;

                const float omega = std::sqrt (eigenvalue);
                const float frequency = omega / (2.0f * piFloat);
                if (frequency >= nyquist * 0.98f)
                    continue;

                // The batter head is the one being struck and the one the
                // microphones are in front of, so the batter component of the
                // eigenvector appears on both the drive and the observation.
                const float batterShare = vectorB / sqrtSigmaB;
                // Net volume flow: what the pair hears once it has backed far
                // enough off the head to stop reading the membrane's shape.
                const float volumeShare = batterShare + vectorR / sqrtSigmaR;

                const float ka = omega * radius / soundSpeed;
                const float efficiency = radiationEfficiency (0, ka);

                // Radiation damping is set by how much air the mode actually
                // moves, and a mode with nodal circles moves very little: its
                // annuli alternate in sign and cancel each other out before the
                // sound has left the head. Integrating J0(lambda r/a) over the
                // disc gives a net volume of 2 J1(lambda)/lambda, and the same
                // J1(lambda)^2 appears in the modal mass, so the two cancel and
                // what is left is a bare 4/lambda^2 - the identical weighting
                // the cavity coupling carries, for the identical reason. Both
                // are net-volume couplings.
                //
                // Leaving it out cost the drum its body. Every axisymmetric
                // mode radiated as though it were a piston, which put fifteen
                // to nineteen inverse seconds of loss on the whole two hundred
                // to six hundred hertz region and emptied it in a third of a
                // second, while the microphone path - which does carry the
                // J1 factor - never received the energy the model was throwing
                // away. The head was being damped by sound it did not make.
                const float netVolume = 2.0f / lambda;
                const float volumeCoupling = netVolume * volumeShare;
                // Power goes as the square of the volume velocity and the
                // eigenvectors are unit length, so the share enters squared.
                const float radiationPrefactor =
                    drum.radiationScale * airDensity * soundSpeed
                    * volumeCoupling * volumeCoupling;
                const float radiationLoss = radiationPrefactor * efficiency;
                const float decayFixed = edgeLoss;
                const float decay = decayFixed + radiationLoss
                                  + materialDamping (drum, omega, extraDamping)
                                  + mountingLoss (drum, frequency);

                const float drive = shapeStrike * batterShare
                                  / (geometricMass * omega * rate);

                // What a microphone in front of the head actually receives.
                // A mode whose pattern on the head is finer than the sound it
                // makes cannot radiate: its field is evanescent and dies as
                // exp(-sqrt(ks^2 - k^2) d) above the surface. That single
                // exponential is the whole close-microphone story - right on
                // the head the pair reads the membrane's shape and separates,
                // and a hand's width back it reads only what the drum radiates
                // and collapses towards mono.
                const float spatialWavenumber = lambda / radius;
                const float airWavenumber = omega / soundSpeed;
                const float evanescentRate = std::sqrt (std::max (
                    spatialWavenumber * spatialWavenumber
                        - airWavenumber * airWavenumber,
                    0.0f));
                const float nearField = std::exp (-evanescentRate * micDistance);

                const float observed =
                    nearField * shapeMic * batterShare
                    + efficiency * (2.0f * besselAtZero / lambda) * volumeShare
                          * propagatingSpread;
                // Proximity lift: a close microphone reads pressure that has
                // not yet spread, and low modes gain most from it.
                const float proximity =
                    1.0f + drum.micProximity / (1.0f + (frequency / 190.0f)
                                                        * (frequency / 190.0f));

                auto& mode = voice.modes[static_cast<std::size_t> (count)];
                mode.omega = omega;
                mode.liveOmega = 2.0 * static_cast<double> (piFloat)
                               * static_cast<double> (frequency);
                mode.decayRate = decay;
                mode.appliedPalmDecay = 0.0f;
                mode.decayFixed = decayFixed;
                mode.lossOmega = lossOmega;
                mode.lossOmegaSquared = lossOmegaSquared;
                mode.radiationPrefactor = radiationPrefactor;
                mode.circumferentialOrder = 0;
                mode.modeEntry = static_cast<std::uint8_t> (entryIndex);
                mode.physicalIndex = static_cast<std::uint8_t> (2 * entryIndex + branch);
                mode.membrane = true;
                mode.localMuteDampingRate = 0.0f;
                mode.handDampingRate = 0.0f;
                mode.inverseModalMass = 1.0f / std::max (geometricMass, 1.0e-9f);
                mode.contactShape = shapeStrike * batterShare
                                  * profile.membraneGain * profile.levelScale;
                mode.batterParticipation = batterShare;
                mode.stretchNorm = lambda * lambda * besselSquared;
                mode.drive = drive * profile.membraneGain * modelScale;
                // Axisymmetric modes look identical from both sides of the
                // head, so they are the part of the image that stays centred.
                mode.micLeft = observed * proximity;
                mode.micRight = observed * proximity;
                configureResonator (mode.resonator, frequency, decay, 1.0f);
                mode.poleRadius = std::sqrt (mode.resonator.a2);
                ++count;

                peakMagnitude = std::max (peakMagnitude,
                                          std::abs (mode.drive * mode.micLeft));
                // Times the rate, so that what the continuum is calibrated
                // against is the receptance rather than the per-sample
                // integration gain. See the note on membranePeak's
                // declaration.
                membranePeak = std::max (membranePeak,
                                         std::abs (mode.drive * rate * mode.micLeft));
            }
        }
        else
        {
            // Every other mode comes as a degenerate pair whose two members
            // are the same shape rotated by a quarter of their own period. A
            // real head is never quite uniform, so the pair sits a fraction of
            // a percent apart and beats - which is most of what makes a drum
            // sound like an object rather than an oscillator.
            const float geometricMass = 0.5f * area * besselSquared * sigmaB;
            const auto orderFloat = static_cast<float> (order);

            for (int branch = 0; branch < 2; ++branch)
            {
                if (count >= membraneResonatorCount)
                    break;

                const float detune =
                    1.0f + splitDepth * (branch == 0 ? 1.0f : -1.0f)
                         * (1.0f + 0.5f * signedUnitFromHash (
                                headSeed + static_cast<std::uint32_t> (entryIndex)));
                const float omega = omegaBatter * detune;
                const float frequency = omega / (2.0f * piFloat);
                if (frequency >= nyquist * 0.98f)
                    continue;

                // Branch 0 is the cos(m theta) member, branch 1 the sin one.
                const float strikeAngular =
                    branch == 0 ? std::cos (orderFloat * theta)
                                : std::sin (orderFloat * theta);
                const float micAngularL =
                    branch == 0 ? std::cos (orderFloat * micAngleL)
                                : std::sin (orderFloat * micAngleL);
                const float micAngularR =
                    branch == 0 ? std::cos (orderFloat * micAngleR)
                                : std::sin (orderFloat * micAngleR);

                const float ka = omega * radius / soundSpeed;
                const float efficiency = radiationEfficiency (order, ka);
                const float radiationPrefactor =
                    drum.radiationScale * airDensity * soundSpeed / (2.0f * sigmaB);
                const float radiationLoss = radiationPrefactor * efficiency;
                const float decayFixed =
                    edgeLoss * (1.0f + edgeOrderFactor * orderFloat);
                const float decay = decayFixed + radiationLoss
                                  + materialDamping (drum, omega, extraDamping)
                                  + mountingLoss (drum, frequency);

                const float drive = shapeStrike * strikeAngular
                                  / (geometricMass * omega * rate);

                // These modes move no net air at all, so almost everything the
                // pair hears from them is near field, and it dies faster with
                // distance than the fundamental's does because their pattern
                // on the head is finer. That is why backing the microphones
                // off narrows the image and softens the slap at the same time:
                // it is one mechanism, not two.
                const float spatialWavenumber = lambda / radius;
                const float airWavenumber = omega / soundSpeed;
                const float evanescentRate = std::sqrt (std::max (
                    spatialWavenumber * spatialWavenumber
                        - airWavenumber * airWavenumber,
                    0.0f));
                const float nearField = std::exp (-evanescentRate * micDistance);

                const float proximity =
                    1.0f + drum.micProximity / (1.0f + (frequency / 190.0f)
                                                        * (frequency / 190.0f));
                // The small part that does escape arrives at both microphones
                // alike, which is what keeps the pair from going anti-phase
                // once the near field has decayed away.
                const float propagating =
                    0.35f * efficiency * propagatingSpread;
                const float observedL =
                    nearField * shapeMic * micAngularL * proximity + propagating;
                const float observedR =
                    nearField * shapeMic * micAngularR * proximity + propagating;

                auto& mode = voice.modes[static_cast<std::size_t> (count)];
                mode.omega = omega;
                mode.liveOmega = 2.0 * static_cast<double> (piFloat)
                               * static_cast<double> (frequency);
                mode.decayRate = decay;
                mode.appliedPalmDecay = 0.0f;
                mode.decayFixed = decayFixed;
                mode.lossOmega = lossOmega;
                mode.lossOmegaSquared = lossOmegaSquared;
                mode.radiationPrefactor = radiationPrefactor;
                mode.circumferentialOrder = static_cast<std::uint8_t> (order);
                mode.modeEntry = static_cast<std::uint8_t> (entryIndex);
                mode.physicalIndex = static_cast<std::uint8_t> (2 * entryIndex + branch);
                mode.membrane = true;
                mode.localMuteDampingRate = 0.0f;
                mode.handDampingRate = 0.0f;
                mode.inverseModalMass = 1.0f / std::max (geometricMass, 1.0e-9f);
                mode.contactShape = shapeStrike * strikeAngular
                                  * profile.membraneGain * profile.levelScale;
                mode.batterParticipation = 1.0f;
                mode.stretchNorm = 0.5f * lambda * lambda * besselSquared;
                mode.drive = drive * profile.membraneGain * modelScale;
                mode.micLeft = observedL;
                mode.micRight = observedR;
                configureResonator (mode.resonator, frequency, decay, 1.0f);
                mode.poleRadius = std::sqrt (mode.resonator.a2);
                ++count;

                peakMagnitude = std::max (
                    peakMagnitude,
                    std::max (std::abs (mode.drive * mode.micLeft),
                              std::abs (mode.drive * mode.micRight)));
                membranePeak = std::max (
                    membranePeak,
                    std::max (std::abs (mode.drive * rate * mode.micLeft),
                              std::abs (mode.drive * rate * mode.micRight)));
            }
        }
    }

    // The wooden bank: the drum's own shell. It is struck directly by a rim
    // shot, which catches the hoop and the body with the head, and picked up
    // faintly the rest of the time, because a head cannot move without the body
    // it is stretched over moving too. The Shell Resonance control scales how
    // much of the body colours an ordinary head stroke, but it never silences
    // the body completely: hitting wood makes a sound whatever the player has
    // decided about sympathetic ring.
    const float bodyLevel = 0.30f + 0.70f * drum.shellLevel;
    const float shellGain = profile.shellGain * bodyLevel
                          + profile.rimGain * 0.35f;
    // The head's own volume displacement gives the membrane modes their
    // radiating area for free; the wooden bank has to be told what it is.
    const float radiatingArea = drum.radius * drum.radius;
    const float woodModalMass = drum.shellModalMass;

    for (int index = 0; index < shellResonatorCount; ++index)
    {
        if (count >= resonatorCount)
            break;

        const auto slot = static_cast<std::size_t> (index);
        const float frequency =
            drum.shellFrequencies[slot] * profile.shellFrequencyScale;
        if (! (frequency > 0.0f) || frequency >= nyquist * 0.98f)
            continue;

        const float decay = drum.shellDecays[slot]
                          / std::max (profile.shellDecayScale, 0.05f)
                          * (1.0f + 1.5f * extraDamping);

        // The shell is a ring, so the pair reads it from two places on it and
        // the wooden component of the image opens with the microphones.
        const auto ring = static_cast<float> (index + 2);
        constexpr float spread = 0.20f;
        // The pair stands in front of the head, not around the body, so it
        // reads the shell mostly in common. A modest difference keeps the wood
        // from collapsing to a point without ever putting it out of phase.
        const float shellL = (1.0f - spread) + spread * std::cos (ring * micAngleL);
        const float shellR = (1.0f - spread) + spread * std::cos (ring * micAngleR);
        const float omega = 2.0f * piFloat * frequency;
        // Same force-over-modal-mass path the head uses, so a heavy body
        // genuinely refuses to move and a light one genuinely rings - times the
        // area it radiates from, which is the term the membrane modes get for
        // free through their own volume displacement and the shell was missing.
        // Without it a small body, whose modal mass falls as the cube of its
        // radius, ran away from the head it is stretched under.
        const float level = shellGain * shellCalibration * radiatingArea
                          / (woodModalMass * omega * rate)
                          / (1.0f + 0.35f * static_cast<float> (index));

        auto& mode = voice.modes[static_cast<std::size_t> (count)];
        mode.omega = omega;
        mode.liveOmega = 2.0 * static_cast<double> (piFloat)
                       * static_cast<double> (frequency);
        mode.decayRate = decay;
        mode.appliedPalmDecay = 0.0f;
        mode.membrane = false;
        mode.physicalIndex = static_cast<std::uint8_t> (
            membraneResonatorCount + index);
        mode.localMuteDampingRate = 0.0f;
        mode.handDampingRate = 0.0f;
        mode.inverseModalMass = 1.0f / std::max (woodModalMass, 1.0e-9f);
        mode.contactShape = 0.0f;
        mode.batterParticipation = 0.0f;
        mode.stretchNorm = 0.0f;
        mode.drive = level * modelScale;
        mode.micLeft = shellL;
        mode.micRight = shellR;
        configureResonator (mode.resonator, frequency, decay, 1.0f);
        mode.poleRadius = std::sqrt (mode.resonator.a2);
        ++count;

        peakMagnitude = std::max (peakMagnitude,
                                  std::abs (mode.drive * mode.micLeft));
    }

    voice.modeCount = count;

    // The head's high-frequency continuum. The resolved bank stops at the
    // highest Bessel zero in the table, which on a large drum is only a few
    // hundred hertz; a real head goes on having modes for another five octaves
    // above that, packed far closer together than their own bandwidths. Nobody
    // hears those individually - what reaches the ear is a shaped burst that
    // empties from the top down - so they are modelled as bands of noise rather
    // than as several hundred more resonators.
    //
    // The bands start above the resolved bank and climb by octaves. Each one
    // carries the head's own loss law, so the top of the drum goes first
    // exactly as it does in the resolved region, and each is lit by the same
    // contact the modes are, so a harder and therefore shorter stroke reaches
    // further up - the v^(-1/5) contact law arriving by a third route.
    {
        float highestResolved = 0.0f;
        for (int index = 0; index < count; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.membrane)
                highestResolved = std::max (highestResolved,
                                            mode.omega / (2.0f * piFloat));
        }

        // Where the resolved bank hands over to the continuum is the modal
        // overlap frequency, and that falls with the drum: a bigger head has
        // more modes in the same span, so they stop being separable sooner. A
        // fixed 120 Hz floor stopped it tracking exactly where it mattered - the
        // largest drum's top resolved mode is 77 Hz, so its crossover was pinned
        // an octave above where the head puts it, and because the band tilt is
        // normalised against this number every band on that drum came out
        // roughly two decibels loud as well. The remaining guard only catches an
        // empty bank.
        const float first = std::max (highestResolved * 1.25f, 20.0f);

        // Short wavelengths live at the edge. The high-order mode shapes pile
        // up against the rim, so a stroke out there couples into the continuum
        // far harder than one over the middle - which is the same reason a Ka
        // is bright and a Don is not, carried into the region the resolved bank
        // cannot represent. A shot that catches the hoop is brighter still.
        //
        // Quadratic, because a straight line badly understates it: J_m(lambda
        // rho) for the orders up here is not merely smaller near the middle of
        // the head, it is vanishing, and the coupling climbs steeply as the
        // stroke walks out. With a linear law the far edge came out no brighter
        // than a Don once the continuum was cut back to its proper share - and
        // an edge stroke that is not brighter than a centre one is not an edge
        // stroke.
        const float edgeBoost = edgeBoostBase
                              + edgeBoostSlope * rho * (1.0f + rho)
                              + 0.85f * profile.rimGain;
        for (int band = 0; band < continuumBandCount; ++band)
        {
            auto& entry = voice.continuum[static_cast<std::size_t> (band)];
            const float centre =
                first * std::pow (continuumBandRatio, static_cast<float> (band));

            if (centre >= nyquist * 0.9f || profile.membraneGain <= 0.0f)
            {
                // This storage is reused by both scratch contacts and a
                // reconfigured physical bank. Clear the entire band, not just
                // its gain: a stale centre would otherwise make the rebuild
                // mapper treat an above-Nyquist band as still present and carry
                // old filter energy into it.
                entry = {};
                continue;
            }

            // Two high-pass stages at the lower edge followed by seven low-pass
            // stages at the upper one. Wide on purpose: this is a region of
            // the spectrum, not a partial. The asymmetric order spends the
            // extra states on the upper skirt, where energy from a lower band
            // could otherwise masquerade as all the octaves above it.
            const float low = centre / continuumBandwidth;
            const float high = centre * continuumBandwidth;
            entry.lowCoefficient =
                1.0f - std::exp (-2.0f * piFloat * std::min (low, nyquist * 0.9f) / rate);
            entry.highCoefficient =
                1.0f - std::exp (-2.0f * piFloat * std::min (high, nyquist * 0.9f) / rate);
            entry.centre = centre;
            entry.lowStateLeft = 0.0f;
            entry.lowStateLeft2 = 0.0f;
            entry.highStateLeft = 0.0f;
            entry.highStateLeft2 = 0.0f;
            entry.highStateLeft3 = 0.0f;
            entry.highStateLeft4 = 0.0f;
            entry.highStateLeft5 = 0.0f;
            entry.highStateLeft6 = 0.0f;
            entry.highStateLeft7 = 0.0f;
            entry.lowStateRight = 0.0f;
            entry.lowStateRight2 = 0.0f;
            entry.highStateRight = 0.0f;
            entry.highStateRight2 = 0.0f;
            entry.highStateRight3 = 0.0f;
            entry.highStateRight4 = 0.0f;
            entry.highStateRight5 = 0.0f;
            entry.highStateRight6 = 0.0f;
            entry.highStateRight7 = 0.0f;

            // Two microphones a fixed distance apart hear a long wavelength in
            // common and a short one independently. The crossover is where the
            // wavelength matches the spacing, so the bottom of the continuum
            // arrives as one signal and the top as two - which is also why
            // opening the pair widens the drum's air rather than just its
            // partials.
            const float separation =
                2.0f * drum.micRadius
                * std::abs (std::sin (0.5f * (micAngleL - micAngleR)));
            const float correlation =
                std::exp (-2.0f * separation * centre / soundSpeed);
            entry.common = std::sqrt (clampFloat (correlation, 0.0f, 1.0f));
            entry.independent = std::sqrt (1.0f - entry.common * entry.common);

            const float omega = 2.0f * piFloat * centre;

            // The head's own loss law, the same one the resolved modes sit on -
            // the continuum is the same piece of hide carrying on above where
            // its modes can be told apart.
            //
            // The rim is where they part company, and they have to. A band here
            // does not hold the axisymmetric modes that happen to land at its
            // centre frequency; almost everything at these frequencies is
            // high circumferential order, and a mode of high order hugs the
            // boundary - J_m(lambda r/a) is pressed against the rim and nearly
            // flat across the middle of the head. Those modes therefore lose at
            // the hoop many times over what a centre-weighted mode loses, by
            // the same per-order law the resolved bank already applies. The
            // highest order a membrane carries at a given frequency is its own
            // dimensionless wavenumber, omega a / c, so the factor climbs with
            // the band.
            //
            // Without it one edge loss had to serve both regions, and no single
            // value works: set it for the resolved bank and the continuum hangs
            // behind the drum as a half-second bed of noise that buries the body
            // it is supposed to sit above, and set it for the continuum and the
            // body goes with it.
            // Kept as coefficients rather than as the sum, so that retuning
            // the head can re-damp the band wherever it lands - and split by
            // what actually moves. The rim's share does not: it is set by the
            // band's dimensionless wavenumber, and stretching a head raises the
            // frequency and the wave speed by the same factor, so omega a / c
            // is exactly where it was. Only the hide's two terms follow a bend.
            const float wavenumber = omega * radius / std::max (drum.waveSpeed, 1.0f);
            voice.continuumLossOmega = lossOmega;
            voice.continuumLossOmegaSquared = lossOmegaSquared;
            entry.lossFixed =
                drum.edgeLoss * (1.0f + continuumEdgeOrder * wavenumber);

            const float decay = entry.lossFixed
                              + voice.continuumLossOmega * omega
                              + voice.continuumLossOmegaSquared * omega * omega;
            entry.envelopeDecay = std::exp (-decay / rate);
            // Left dark. Every band is lit by the contacts themselves, each in
            // proportion to how hard it lands, so a flam's grace note gets its
            // share and its main stroke gets the whole of it. Starting them
            // alight handed the first contact everything regardless of how
            // gently it touched the head.
            entry.envelope = 0.0f;

            // Modal density per octave grows with frequency. The contact pulse
            // and measured hide loss provide the downward spectral slope after
            // this statistical count has been applied.
            const float tilt = std::pow (first / centre, continuumTilt);
            // Most of a white-noise input lies outside one octave and is thrown
            // away. Exact state-space normalisation makes the physical level
            // below independent of filter geometry and sample rate.
            const auto drumIndex = static_cast<std::size_t> (
                std::clamp (voice.octaveOffset, lowestOctaveOffset,
                            highestOctaveOffset) - lowestOctaveOffset);
            auto& cachedVariance = continuumVarianceCache_[drumIndex]
                [static_cast<std::size_t> (band)];
            if (! cachedVariance.valid
                || cachedVariance.lowCoefficient != entry.lowCoefficient
                || cachedVariance.highCoefficient != entry.highCoefficient)
            {
                cachedVariance.lowCoefficient = entry.lowCoefficient;
                cachedVariance.highCoefficient = entry.highCoefficient;
                cachedVariance.variance = continuumBandVariance (
                    entry.lowCoefficient, entry.highCoefficient);
                cachedVariance.valid = true;
            }
            const float variance = cachedVariance.variance;

            // Relative to the resolved bank; trigger() scales the whole set by
            // the force of the stroke, exactly as the modes are scaled by the
            // excitation they are driven with.
            // Against the head's own drive, not the whole bank's. The
            // continuum is the head going on above where its modes can be told
            // apart, so a stroke that is loud because it caught the hoop and
            // rang the body must not drag it up with them.
            // No membraneGain here: membranePeak is measured from mode.drive,
            // which already carries it. Applying it twice made the continuum
            // scale as the square of the articulation's head gain, so the
            // quieter strokes came out far darker than their profile asks for -
            // Katsu's 0.06 became 0.0036 and lost its continuum altogether.
            entry.targetRms = continuumCalibration * edgeBoost * tilt * membranePeak;
            entry.level = entry.targetRms / std::sqrt (variance);
        }
    }

    // How long each mode stays above the retirement floor, so the render loop
    // can drop them as they go. A drum starts with everything ringing and ends
    // on two or three modes; paying for forty the whole way is pure waste.
    for (int index = 0; index < count; ++index)
    {
        auto& mode = voice.modes[static_cast<std::size_t> (index)];
        const float magnitude =
            std::max (std::abs (mode.drive * mode.micLeft),
                      std::abs (mode.drive * mode.micRight));
        const float relative = magnitude / peakMagnitude;

        if (mode.decayRate <= 0.0f || relative <= modeRetirementFloor)
        {
            mode.retirementLog = 0.0f;
            mode.audibleSamples = 0;
            continue;
        }

        mode.retirementLog = std::log (relative / modeRetirementFloor);
        const float seconds = mode.retirementLog / mode.decayRate;
        const float bounded = clampFloat (seconds, 0.0f,
                                          static_cast<float> (maximumTailSeconds));
        mode.audibleSamples = static_cast<std::uint64_t> (bounded * rate);
    }

    // Descending, so the active count only ever shrinks. Insertion sort: the
    // array is forty-six entries and this runs once per stroke.
    for (int i = 1; i < count; ++i)
    {
        const Mode key = voice.modes[static_cast<std::size_t> (i)];
        int j = i - 1;
        while (j >= 0
               && voice.modes[static_cast<std::size_t> (j)].audibleSamples
                      < key.audibleSamples)
        {
            voice.modes[static_cast<std::size_t> (j + 1)] =
                voice.modes[static_cast<std::size_t> (j)];
            --j;
        }
        voice.modes[static_cast<std::size_t> (j + 1)] = key;
    }

    voice.activeModeCount = count;
    while (! voice.physicalBank && voice.activeModeCount > 0
           && voice.modes[static_cast<std::size_t> (voice.activeModeCount - 1)]
                      .audibleSamples
                  == 0)
        --voice.activeModeCount;

    voice.maximumSamples = voice.activeModeCount > 0
        ? voice.modes[0].audibleSamples + static_cast<std::uint64_t> (rate * 0.02f)
        : static_cast<std::uint64_t> (rate * 0.05f);
    voice.maximumSamples = std::min (
        voice.maximumSamples,
        static_cast<std::uint64_t> (maximumTailSeconds * sampleRate_));
}

void TaikoEngine::ensurePhysicalDrum (int octave, const DrumState& drum) noexcept
{
    const auto drumIndex = static_cast<std::size_t> (
        std::clamp (octave, lowestOctaveOffset, highestOctaveOffset)
        - lowestOctaveOffset);
    auto& physical = physicalDrums_[drumIndex];
    if (physical.modeCount > 0
        && physical.configurationRevision == physicalConfigurationRevision_
        && physical.configurationPitch == applied_.pitch)
        return;

    struct SavedModeState
    {
        double displacement { 0.0 };
        double velocity { 0.0 };
        float muteDampingRate { 0.0f };
        bool valid { false };
    };

    std::array<SavedModeState, resonatorCount> savedModes {};
    const bool rebuilding = physical.modeCount > 0;
    const bool wasActive = physical.active;
    const auto savedAge = physical.ageSamples;
    const auto savedDeadline = physical.maximumSamples;
    const auto savedCountdown = physical.controlCountdown;
    const auto savedInput = physical.modalInput;
    const auto savedContinuum = physical.continuum;
    const float savedTensionEnvelope = physical.tensionEnvelope;
    const int savedMuteTicks = physical.localMuteTicksRemaining;
    const float savedContinuumMute = physical.continuumMuteDampingRate;
    const bool savedPalmActive = physical.palmDampingActive;
    const float savedRetireGain = physical.retireGain;
    const float savedRetireStep = physical.retireStep;

    if (rebuilding)
    {
        for (int index = 0; index < physical.modeCount; ++index)
        {
            const auto& mode = physical.modes[static_cast<std::size_t> (index)];
            const auto id = static_cast<std::size_t> (mode.physicalIndex);
            if (id >= savedModes.size())
                continue;

            auto& saved = savedModes[id];
            saved.displacement = mode.resonator.y1;
            saved.muteDampingRate = mode.localMuteDampingRate;

            const double radius = mode.poleRadius;
            const double sine = mode.resonator.b0;
            if (radius > 0.0 && std::abs (sine) > 1.0e-12
                && mode.liveOmega > 0.0)
            {
                const double cosine = -mode.resonator.a1 / (2.0 * radius);
                const double quadrature =
                    (mode.resonator.y1 * cosine
                     - radius * mode.resonator.y2) / sine;
                saved.velocity = mode.liveOmega * quadrature
                               - static_cast<double> (
                                     mode.decayRate + mode.appliedPalmDecay)
                                     * mode.resonator.y1;
            }
            else
            {
                saved.velocity = (mode.resonator.y1 - mode.resonator.y2)
                               * sampleRate_;
            }
            saved.valid = std::isfinite (saved.displacement)
                       && std::isfinite (saved.velocity);
        }
    }
    else
    {
        silenceVoice (physical);
    }

    physical.physicalBank = true;
    physical.octaveOffset = octave;
    physical.strikeRadius = strikeProfile (Articulation::Don).radius;
    physical.strikeAngle = 0.0f;
    if (! rebuilding)
        physical.noiseState = hash32 (
            0x6d2b79f5u + static_cast<std::uint32_t> (drumIndex) * 0x9e3779b9u) | 1u;

    // Eigenfrequencies, losses and observation belong to the instrument, not
    // to the articulation that happens to wake it. Don is only a neutral way
    // through the existing builder while strike-specific coupling remains in
    // the transient slot. No state from that scratch excitation is retained.
    buildVoiceModes (physical, drum, strikeProfile (Articulation::Don), 0.0f);
    // CC1 has pressure but no position channel. Anchor its palm at the same
    // central hand position as Tsu, and cache physical loss rates. Control
    // updates change each pole's loss; that pole then damps every audio sample.
    std::array<float, modeEntryCount> handRates {};
    palmDampingRates (drum, strikeProfile (Articulation::Tsu).radius,
                      handRates, physical.continuumHandDampingRate);
    for (int index = 0; index < physical.modeCount; ++index)
    {
        auto& mode = physical.modes[static_cast<std::size_t> (index)];
        float batterFraction = 1.0f;
        if (mode.circumferentialOrder == 0)
            batterFraction = clampFloat (
                drum.batterDensity * mode.batterParticipation
                    * mode.batterParticipation,
                0.0f, 1.0f);
        mode.handDampingRate = mode.membrane
            ? handRates[static_cast<std::size_t> (mode.modeEntry)] * batterFraction
            : 0.0f;
    }
    physical.configurationRevision = physicalConfigurationRevision_;
    physical.configurationPitch = applied_.pitch;
    physical.activeModeCount = physical.modeCount;
    physical.modalInput = rebuilding ? savedInput
                                     : std::array<float, resonatorCount> {};
    physical.contactCount = 0;
    physical.nextContact = 0;
    physical.contactRemaining = 0u;
    physical.tuningAtStrike = applied_.pitch + 2.0f * pitchBend_;
    physical.appliedTensionShift = 1.0f;

    // Map the old canonical coordinates into the new poles. Displacement is
    // continuous and physical velocity is preserved, rather than copying y2
    // between recurrences whose frequency and decay may be unrelated.
    for (int index = 0; index < physical.modeCount; ++index)
    {
        auto& mode = physical.modes[static_cast<std::size_t> (index)];
        const auto id = static_cast<std::size_t> (mode.physicalIndex);
        mode.resonator.y1 = 0.0;
        mode.resonator.y2 = 0.0;
        if (! rebuilding || id >= savedModes.size() || ! savedModes[id].valid)
            continue;

        const auto& saved = savedModes[id];
        mode.resonator.y1 = saved.displacement;
        mode.localMuteDampingRate = saved.muteDampingRate;
        const double radius = mode.poleRadius;
        const double sine = mode.resonator.b0;
        if (radius > 0.0 && std::abs (sine) > 1.0e-12
            && mode.liveOmega > 0.0)
        {
            const double cosine = -mode.resonator.a1 / (2.0 * radius);
            const double quadrature =
                (saved.velocity
                 + static_cast<double> (mode.decayRate) * saved.displacement)
                / mode.liveOmega;
            mode.resonator.y2 =
                (saved.displacement * cosine - quadrature * sine) / radius;
        }
        else
        {
            mode.resonator.y2 = saved.displacement
                              - saved.velocity / sampleRate_;
        }
    }

    for (std::size_t index = 0; index < physical.continuum.size(); ++index)
    {
        auto& band = physical.continuum[index];
        const auto configured = band;
        if (rebuilding && configured.centre > 0.0f
            && savedContinuum[index].centre > 0.0f)
        {
            // Filter memory and unresolved modal energy are physical state.
            // Keep them while replacing only the coefficients and calibrated
            // metadata derived from the new drum.
            band = savedContinuum[index];
            band.lowCoefficient = configured.lowCoefficient;
            band.highCoefficient = configured.highCoefficient;
            band.lossFixed = configured.lossFixed;
            band.targetRms = configured.targetRms;
            band.envelopeDecay = configured.envelopeDecay;
            band.centre = configured.centre;
            band.common = configured.common;
            band.independent = configured.independent;
        }
        else
        {
            band = configured;
            band.envelope = 0.0f;
        }
        // A strike slot supplies the physically calibrated, variance-normalised
        // amplitude. The canonical band owns only one persistent filter and
        // its current RMS energy.
        band.level = band.centre > 0.0f ? 1.0f : 0.0f;
    }

    constexpr float inverseModelScaleSquared = 1.0f / (modelScale * modelScale);
    physical.tensionDepth = tensionStretchCalibration
                          * drum.stretchStiffness * inverseModelScaleSquared
                          / std::max (drum.radius * drum.radius, 1.0e-6f);
    physical.tensionDecay =
        std::exp (-static_cast<float> (controlPeriod)
                  / (tensionFollowerSeconds * static_cast<float> (sampleRate_)));

    physical.ageSamples = rebuilding ? savedAge : 0;
    physical.maximumSamples = rebuilding
        ? savedDeadline
        : static_cast<std::uint64_t> (maximumTailSeconds * sampleRate_);
    physical.controlCountdown = rebuilding && ! savedPalmActive ? savedCountdown : 0;
    physical.active = rebuilding ? wasActive : false;
    physical.tensionEnvelope = rebuilding ? savedTensionEnvelope : 0.0f;
    physical.localMuteTicksRemaining = rebuilding ? savedMuteTicks : 0;
    physical.continuumMuteDampingRate = rebuilding ? savedContinuumMute : 0.0f;
    physical.palmDampingActive = rebuilding ? savedPalmActive : false;
    physical.retireGain = rebuilding ? savedRetireGain : 1.0f;
    physical.retireStep = rebuilding ? savedRetireStep : 0.0f;
}

void TaikoEngine::palmDampingRates (
    const DrumState& drum, float strikeRadius,
    std::array<float, modeEntryCount>& modeRates,
    float& continuumRate) noexcept
{
    const auto& entries = membraneModes();
    const float rho = clampFloat (strikeRadius, 0.0f, 0.995f);
    const float area = piFloat * drum.radius * drum.radius;
    modeRates.fill (0.0f);
    // The hand has a size in metres, not as a fraction of the instrument. On a
    // head too small to contain the full palm, use the largest centred patch
    // the radial quadrature can represent without crossing the rim.
    const float physicalPalmRadius =
        std::min (handPatchRadiusMetres, 0.35f * drum.radius);
    const float palmRadius = physicalPalmRadius / drum.radius;
    const float palmArea = piFloat * physicalPalmRadius * physicalPalmRadius;
    for (int index = 0; index < modeEntryCount; ++index)
    {
        const auto& entry = entries[static_cast<std::size_t> (index)];
        const int order = entry.circumferentialOrder;
        const auto besselAtZero =
            static_cast<float> (besselJ (order + 1, entry.besselZero));
        const float besselSquared = std::max (besselAtZero * besselAtZero, 1.0e-9f);
        // The same modal mass buildVoiceModes drives through, so the two cannot
        // disagree about how heavy a mode is.
        const float modalMass = (order == 0 ? 1.0f : 0.5f) * area * besselSquared
                              * drum.batterDensity;
        // Five-point area quadrature over a palm-sized patch. A point damper
        // disappears whenever it happens to land on a nodal line; a real hand
        // covers both sides and absorbs the surrounding motion. For a
        // degenerate pair the unknown angular orientation contributes its mean
        // square, one half.
        const auto radii = palmPatchRadii (rho, palmRadius);
        float meanSquare = 0.0f;
        for (std::size_t sample = 0; sample < radii.size(); ++sample)
        {
            const float sampleShape = static_cast<float> (
                besselJ (order, entry.besselZero * radii[sample]));
            const float weight = sample == 0 ? 0.5f : 0.125f;
            meanSquare += weight * sampleShape * sampleShape;
        }
        meanSquare *= order == 0 ? 1.0f : 0.5f;

        // Project the distributed viscous contact onto this mode. The patch
        // integral is A_p <phi^2>. Its share of the whole modal norm cannot
        // exceed one; c_A / sigma then gives a velocity-loss rate in 1/s.
        const float modalNorm = modalMass / drum.batterDensity;
        const float participation = clampFloat (
            palmArea * meanSquare / std::max (modalNorm, 1.0e-9f), 0.0f, 1.0f);
        // Axisymmetric cavity branches apply their batter-head energy fraction
        // when this base rate is assigned; the table entry itself is shared by
        // both orthogonal eigen-coordinates.
        modeRates[static_cast<std::size_t> (index)] =
            muteSurfaceDamping / drum.batterDensity * participation;
    }

    // In the high-density limit the local mean square cancels between patch
    // and whole-head modal norms. The area ratio is the unresolved field's
    // physical size law.
    continuumRate = muteSurfaceDamping / drum.batterDensity
                  * clampFloat (palmArea / area, 0.0f, 1.0f);
}

void TaikoEngine::dampPhysicalDrum (Voice& physical,
                                    const StrikeProfile& profile, float strikeRadius,
                                    const DrumState& drum) noexcept
{
    // Bachi/head momentum exchange belongs to the coupled dynamic contact.
    // This function is only the free hand that remains on the hide for a Tsu.
    if (! (profile.membraneGain > 0.0f) || ! profile.palmContact)
        return;

    std::array<float, modeEntryCount> dampingRates {};
    float continuumRate = 0.0f;
    palmDampingRates (drum, strikeRadius, dampingRates, continuumRate);
    const float muteAmount = profile.muteAmount * profile.muteAmount;

    if (muteAmount > 0.0f)
    {
        for (int index = 0; index < physical.activeModeCount; ++index)
        {
            auto& mode = physical.modes[static_cast<std::size_t> (index)];
            if (mode.membrane)
            {
                float batterFraction = 1.0f;
                if (mode.circumferentialOrder == 0)
                    batterFraction = clampFloat (
                        drum.batterDensity * mode.batterParticipation
                            * mode.batterParticipation,
                        0.0f, 1.0f);
                mode.localMuteDampingRate = std::max (
                    mode.localMuteDampingRate,
                    dampingRates[static_cast<std::size_t> (mode.modeEntry)]
                        * batterFraction * muteAmount);
            }
        }

        physical.continuumMuteDampingRate = std::max (
            physical.continuumMuteDampingRate, continuumRate * muteAmount);
        const int ticks = std::max (
            1, static_cast<int> (std::ceil (
                   muteContactSeconds * static_cast<float> (sampleRate_)
                   / static_cast<float> (controlPeriod))));
        physical.localMuteTicksRemaining =
            std::max (physical.localMuteTicksRemaining, ticks);
        // The held palm must reach the pole before the next audio sample, not
        // whenever this drum's previous control interval happens to end.
        physical.controlCountdown = 0;
    }
}

void TaikoEngine::scheduleContacts (Voice& voice, const StrikeProfile& profile,
                                    float contactSeconds, float peakForce,
                                    float noiseLevel) noexcept
{
    const auto rate = static_cast<float> (sampleRate_);
    const auto lengthSamples =
        static_cast<std::uint32_t> (std::max (2.0f, contactSeconds * rate));

    voice.contactCount = 0;
    voice.nextContact = 0;
    voice.contactRemaining = 0u;

    const int requested = std::clamp (profile.contactCount, 1, maxContactEvents);
    const auto seed = static_cast<std::uint32_t> (voice.startOrder * 22695477u + 5u);

    if (requested == 1)
    {
        voice.contacts[0] = { 0u, lengthSamples, peakForce, peakForce * noiseLevel };
        voice.contactCount = 1;
        return;
    }

    // A press roll. The stick is pushed into the head and bounces, so each
    // contact is weaker and closer than the one before it.
    float offsetSeconds = 0.0f;
    float amplitude = peakForce * 0.85f;
    float spacing = 0.019f;

    for (int index = 0; index < requested; ++index)
    {
        const float jitter =
            1.0f + 0.30f * applied_.humanise
                 * signedUnitFromHash (seed + static_cast<std::uint32_t> (index));
        voice.contacts[static_cast<std::size_t> (index)] = {
            static_cast<std::uint32_t> (offsetSeconds * rate),
            std::max<std::uint32_t> (2u,
                static_cast<std::uint32_t> (lengthSamples * (1.0f + 0.12f * static_cast<float> (index)))),
            amplitude,
            amplitude * noiseLevel * 1.4f
        };
        offsetSeconds += spacing * jitter;
        spacing *= 0.82f;
        amplitude *= 0.72f;
    }

    voice.contactCount = requested;
}

// ---------------------------------------------------------------------------
// Striking the drum
// ---------------------------------------------------------------------------

int TaikoEngine::findVoiceSlot() noexcept
{
    for (int index = 0; index < maxVoices; ++index)
        if (! voices_[static_cast<std::size_t> (index)].active)
            return index;

    // Everything is sounding, so take the quietest. On a drum this is very
    // nearly inaudible: by the time sixteen strokes overlap, the oldest is
    // buried under the newest ones anyway.
    //
    // A voice that has not been rendered yet is passed over, because its peak
    // level is still zero and it would therefore look like the quietest thing
    // in the engine to every stroke that followed it. A block carrying several
    // strokes at the same sample - a flam, a chord, a roll landing on one
    // buffer boundary - would drop all of them onto the one slot and sound a
    // single note.
    int quietest = -1;
    float lowest = 0.0f;
    std::uint64_t quietestOrder = 0;
    for (int index = 0; index < maxVoices; ++index)
    {
        const auto& voice = voices_[static_cast<std::size_t> (index)];
        if (voice.ageSamples == 0)
            continue;

        // Ties go to the oldest, so a rank of equally quiet voices is consumed
        // in the order it was struck rather than from slot zero every time.
        if (quietest < 0 || voice.peakLevel < lowest
            || (voice.peakLevel == lowest && voice.startOrder < quietestOrder))
        {
            lowest = voice.peakLevel;
            quietestOrder = voice.startOrder;
            quietest = index;
        }
    }

    if (quietest >= 0)
        return quietest;

    // Sixteen strokes arrived without a single sample rendered between them.
    // Nothing has a level to compare, so the oldest goes.
    int oldest = 0;
    for (int index = 1; index < maxVoices; ++index)
        if (voices_[static_cast<std::size_t> (index)].startOrder
            < voices_[static_cast<std::size_t> (oldest)].startOrder)
            oldest = index;
    return oldest;
}

void TaikoEngine::trigger (Articulation articulation, int octaveOffset,
                           float velocity) noexcept
{
    if (! prepared_)
        return;
    if (static_cast<std::size_t> (articulation) >= articulationCount)
        return;
    if (! (velocity > 0.0f) || ! std::isfinite (velocity))
        return;

    refreshDrumIfNeeded();

    const int octave = std::clamp (octaveOffset, lowestOctaveOffset, highestOctaveOffset);
    const auto cacheIndex = static_cast<std::size_t> (octave - lowestOctaveOffset);
    const auto& drum = drumCache_[cacheIndex];
    const auto& profile = strikeProfile (articulation);
    ensurePhysicalDrum (octave, drum);
    auto& physical = physicalDrums_[cacheIndex];

    // A bank near its hard deadline may already be part-way through the final
    // output fade. Retriggering extends that bank instead of replacing it, so
    // first fold the audible fade into its stored energy; simply restoring the
    // gain to one would reveal the hidden, unfaded tail as a step.
    if (physical.retireGain < 1.0f)
    {
        const float gain = clampFloat (physical.retireGain, 0.0f, 1.0f);
        for (int index = 0; index < physical.modeCount; ++index)
        {
            auto& resonator = physical.modes[static_cast<std::size_t> (index)].resonator;
            resonator.y1 *= gain;
            resonator.y2 *= gain;
        }
        for (auto& band : physical.continuum)
            band.envelope *= gain;
        physical.tensionEnvelope *= gain * gain;
    }

    const int slot = findVoiceSlot();
    auto& voice = voices_[static_cast<std::size_t> (slot)];
    silenceVoice (voice);
    voice.physicalBank = false;

    const auto order = ++noteSequence_;
    const auto seed = static_cast<std::uint32_t> (order * 2654435761u + 11u);

    voice.startOrder = order;
    voice.articulation = articulation;
    voice.articulationLevelScale = profile.levelScale;
    voice.octaveOffset = octave;
    voice.physicalDrumIndex = static_cast<std::uint8_t> (cacheIndex);
    voice.velocity = clampFloat (velocity, 0.0f, 1.0f);

    // No two strokes drag across the hide the same way, so the contact noise
    // is reseeded per stroke - unless Humanise is off, which asks for a machine
    // and must therefore repeat exactly.
    const float humanise = applied_.humanise;
    voice.noiseState = hash32 (humanise > 0.0f
                                   ? seed
                                   : static_cast<std::uint32_t> (articulation) * 131u
                                         + static_cast<std::uint32_t> (octave + 8) * 17u)
                     | 1u;

    // Where the stick lands. The articulation sets the radius, the position
    // control offsets it, and humanising scatters both the radius and the
    // angle - which is what stops a roll from sounding like one sample.
    const float positionOffset = applied_.strikePosition * 0.32f;
    const float radiusJitter = 0.055f * humanise * signedUnitFromHash (seed + 1u);
    voice.strikeRadius = clampFloat (profile.radius + positionOffset + radiusJitter,
                                     0.0f, 0.985f);
    // A player works around the head rather than hitting one spot, and with
    // the microphones apart that is what walks successive strokes across the
    // image. At zero humanising the hand stops wandering and every stroke of an
    // articulation lands in exactly the same place.
    voice.strikeAngle = signedUnitFromHash (seed + 2u) * piFloat * humanise;

    // MIDI velocity to impact speed, geometrically and with nothing shaping it.
    // The map used to square the normalised value first, on the grounds that it
    // gave the bottom of the range resolution; squaring in front of a
    // logarithmic map does the opposite of that. Level goes as v^1.2 and speed
    // goes as a power of the control, so the plain map is already even in
    // decibels - equal steps of MIDI velocity are equal steps of loudness,
    // which is what a player's arm does. The squared one piled the whole lower
    // half of the keyboard's velocity range into half a decibel of each other
    // just above the floor, which is exactly the complaint players make about
    // the sampled libraries.
    const float shaped = lerp (0.72f, voice.velocity, applied_.velocityDepth);
    const float speedJitter =
        1.0f + 0.10f * humanise * signedUnitFromHash (seed + 3u);
    const float impactSpeed = clampFloat (
        geometricLerp (minimumImpactSpeed, maximumImpactSpeed, shaped) * speedJitter,
        0.05f, 12.0f);

    // What is being struck: a bachi scaled to this drum, meeting a head whose
    // own resistive impedance is the floor on how fast the stick can leave it.
    float strikerMass = 0.0f;
    float targetImpedance = 0.0f;
    drumContactTerms (drum, strikerMass, targetImpedance);
    const float collisionMass = contactCollisionMass (
        drum, profile, voice.strikeRadius, strikerMass);

    // A Tsu leaves the free hand on the one canonical head, so it damps motion
    // already ringing on this physical drum as well as energy the new contact
    // injects while the palm remains there.
    dampPhysicalDrum (physical, profile, voice.strikeRadius, drum);

    const float extraDamping = profile.muteAmount;
    buildVoiceModes (voice, drum, profile, extraDamping);
    voice.modeProjection.fill (0.0f);
    voice.contactProjection.fill (0.0f);
    for (int index = 0; index < voice.modeCount; ++index)
    {
        auto& mode = voice.modes[static_cast<std::size_t> (index)];
        const auto physicalIndex = static_cast<std::size_t> (mode.physicalIndex);
        voice.modeProjection[physicalIndex] = mode.drive;
        voice.contactProjection[physicalIndex] = mode.contactShape;
        // A strike slot must not retain a second physical state. The remaining
        // metadata array is temporary migration storage only and is outside
        // every render/control path after trigger returns.
        mode.resonator.clear();
    }
    voice.modeCount = 0;
    voice.activeModeCount = 0;

    // Start the bachi exactly at the moving surface with its incoming speed in
    // the preceding sample. From here the contact force is solved from the
    // shared head and stick states; the Hertz calculation above remains only a
    // useful duration/peak estimate for residual and direct-path calibration.
    double surface = 0.0;
    double previousSurface = 0.0;
    for (int index = 0; index < physical.modeCount; ++index)
    {
        const auto& mode = physical.modes[static_cast<std::size_t> (index)];
        if (! mode.membrane)
            continue;
        const auto id = static_cast<std::size_t> (mode.physicalIndex);
        surface += static_cast<double> (voice.contactProjection[id])
                 * mode.resonator.y1 / static_cast<double> (modelScale);
        previousSurface += static_cast<double> (voice.contactProjection[id])
                         * mode.resonator.y2 / static_cast<double> (modelScale);
    }

    voice.stickMass = std::max (static_cast<double> (strikerMass), 1.0e-6);
    const double step = 1.0 / sampleRate_;
    const float relativeImpactSpeed = std::max (
        impactSpeed - static_cast<float> ((surface - previousSurface) / step),
        0.05f);
    float contactSeconds = 0.0f;
    float peakForce = 0.0f;
    solveContact (collisionMass, targetImpedance, profile, applied_.bachiHardness,
                  relativeImpactSpeed, contactSeconds, peakForce);
    contactSeconds *= 1.0f + 0.08f * humanise * signedUnitFromHash (seed + 4u);
    const float noiseLevel = applied_.strikeNoise * profile.noiseGain * 0.35f;
    const float excitationScale = peakForce * profile.levelScale;
    scheduleContacts (voice, profile, contactSeconds, excitationScale, noiseLevel);

    voice.stickPosition = surface;
    voice.stickPrevious = surface - step * static_cast<double> (impactSpeed);
    // IMP-2 is implicit and passive even when a deliberately hostile host rate
    // leaves only a handful of samples in the collision. Softening K to force a
    // six-sample pulse instead made the stick follow the returning fundamental
    // for seventeen milliseconds at 8 kHz; preserve the physical stiffness.
    voice.contactStiffness = contactStiffnessFor (profile, applied_.bachiHardness);
    voice.contactDamping = contactDampingFactor
                         / static_cast<double> (relativeImpactSpeed);
    voice.residualImpedance = std::max (static_cast<double> (targetImpedance), 1.0);
    const double coupling = static_cast<double> (profile.membraneGain)
                          * profile.levelScale;
    const double referenceTailAdmittance = coupling * coupling
                                         / voice.residualImpedance;
    // Same admittance advancePhysicalContacts multiplies the solved contact
    // force by on every sample this contact stays active; cached here so that
    // per-sample loop can read it rather than re-deriving it from the strike
    // profile and residualImpedance on every one of its own iterations.
    voice.contactEnergyAdmittance = referenceTailAdmittance;

    // Integral of sin(pi t/tau)^3 over the contact, through the same omitted-
    // mode admittance the dynamic solve uses. Dividing solved residual work by
    // this reference preserves the recording-led tail calibration without
    // applying the articulation transformer a second time.
    constexpr double squaredHertzPulseIntegral = 4.0 / (3.0 * piFloat);
    voice.referenceContactEnergy = static_cast<double> (peakForce) * peakForce
                                 * static_cast<double> (contactSeconds)
                                 * squaredHertzPulseIntegral
                                 * referenceTailAdmittance;
    voice.solvedContactEnergyStep = 0.0;
    voice.contactAmplitude = excitationScale;
    voice.contactNoiseAmplitude = excitationScale * noiseLevel;
    voice.nonlinearContactActive = profile.membraneGain > 0.0f;
    voice.nonlinearContactHasForce = false;
    voice.continuumInjected = false;
    voice.solvedContactForce = 0.0f;
    voice.nextContact = voice.contactCount;

    // The continuum was built against the resolved bank's drive; the modes are
    // then driven by the contact, so the continuum has to be scaled by the same
    // force to sit where it belongs against them.
    //
    // And shaded by how long that contact lasted. A force pulse of duration tau
    // has no useful content much above 1/tau, so a soft stroke - which rests on
    // the head nearly twice as long - simply cannot reach the top of the
    // continuum, while a hard one lights all of it. This is the same v^(-1/5)
    // contact law that shortens the pulse and brightens the resolved modes,
    // finally reaching the region where most of the brightness actually lives.
    voice.contactReference = excitationScale;

    const float corner = 1.0f / std::max (contactSeconds, 1.0e-5f);
    for (std::size_t index = 0; index < voice.continuum.size(); ++index)
    {
        auto& band = voice.continuum[index];
        const float ratio = band.centre / corner;
        band.level *= excitationScale / (1.0f + ratio * ratio);
        voice.continuumInjection[index] = band.level;
    }

    // What a contact's amplitude is measured against when it relights the
    // continuum: the force the continuum was scaled by, not the first contact's
    // amplitude. A flam's first contact is its quiet grace note, so measuring
    // against that gave the main stroke well over twice the continuum it should
    // have had - and put the loudest articulation on the limiter.

    // Contact noise is stick and hide texture, so it sits an octave or so
    // above the drum and follows how sharp the contact was.
    const float noiseCorner = clampFloat (0.55f / std::max (contactSeconds, 1.0e-4f),
                                          200.0f, 0.45f * static_cast<float> (sampleRate_));
    voice.noiseBandCoefficient =
        1.0f - std::exp (-2.0f * piFloat * noiseCorner * inverseSampleRate_);
    voice.noiseBandState = 0.0f;

    // The tack line. Each tack holds down the head's tension over its share of
    // the circumference, so the force a stroke has to beat before anything
    // rattles is a property of the drum - a tighter or a larger head is held
    // harder - while the rattle's own band is a property of the nail and does
    // not move with the drum at all.
    voice.tackRimGain = profile.rimGain;
    voice.tackPreload = drum.tension * tackSpacingMetres;
    voice.tackScale = profile.rimGain > 0.0f
        ? tackCalibration * applied_.strikeNoise * profile.noiseGain
        : 0.0f;
    voice.tackNoiseState = hash32 (voice.noiseState + 0x5bf03635u) | 1u;
    voice.tackEnvelope = 0.0f;
    voice.tackEnvelopeDecay =
        std::exp (-1.0f / (tackRattleSeconds * static_cast<float> (sampleRate_)));
    voice.tackLowState = 0.0f;
    voice.tackHighState = 0.0f;
    voice.tackLowCoefficient = 1.0f - std::exp (
        -2.0f * piFloat
        * std::min (tackLowCorner, 0.45f * static_cast<float> (sampleRate_))
        * inverseSampleRate_);
    voice.tackHighCoefficient = 1.0f - std::exp (
        -2.0f * piFloat
        * std::min (tackHighCorner, 0.45f * static_cast<float> (sampleRate_))
        * inverseSampleRate_);

    // Stretching the head raises its tension, so a hard stroke starts sharp and
    // settles as it decays. Nothing here schedules that: the coefficient below
    // is the drum's, not the stroke's, and what makes the glide big on a hard
    // stroke and absent on a soft one is that the head is displaced further.
    //
    // It carries 1/modelScale^2 because the resonator states it will be applied
    // to are in units of that calibration and the displacement it stands for is
    // in metres. That division is the point of the whole rewrite: the engine's
    // one loudness constant is documented as unable to distort any relationship
    // inside the model, and the term this replaces let it set a third of the
    // bend.
    constexpr float inverseModelScaleSquared = 1.0f / (modelScale * modelScale);
    physical.tensionDepth = tensionStretchCalibration
                          * drum.stretchStiffness * inverseModelScaleSquared
                          / std::max (drum.radius * drum.radius, 1.0e-6f);
    physical.tensionDecay =
        std::exp (-static_cast<float> (controlPeriod)
                  / (tensionFollowerSeconds * static_cast<float> (sampleRate_)));

    // The airborne path from the stick to each microphone. Plain geometry: the
    // strike lands somewhere on the head, each mic sits somewhere in front of
    // it, and the impact reaches them at the speed of sound with the usual
    // inverse-distance spreading. The difference between the two distances is
    // both the level and the time cue that places the stroke across the image.
    {
        const float strikeX = drum.radius * voice.strikeRadius * std::cos (voice.strikeAngle);
        const float strikeY = drum.radius * voice.strikeRadius * std::sin (voice.strikeAngle);
        const float height = drum.micDistanceMetres;

        const auto pathTo = [&] (float angle) noexcept
        {
            const float micX = drum.micRadius * std::cos (angle);
            const float micY = drum.micRadius * std::sin (angle);
            const float dx = micX - strikeX;
            const float dy = micY - strikeY;
            return std::sqrt (dx * dx + dy * dy + height * height);
        };

        const float pathLeft = std::max (pathTo (drum.micAngleLeft), 0.02f);
        const float pathRight = std::max (pathTo (drum.micAngleRight), 0.02f);
        const float nearest = std::min (pathLeft, pathRight);

        voice.directGainLeft = directCalibration / pathLeft;
        voice.directGainRight = directCalibration / pathRight;
        voice.directDelayLeft =
            clampFloat ((pathLeft - nearest) / soundSpeed * static_cast<float> (sampleRate_),
                        0.0f, static_cast<float> (directLineSize - 4));
        voice.directDelayRight =
            clampFloat ((pathRight - nearest) / soundSpeed * static_cast<float> (sampleRate_),
                        0.0f, static_cast<float> (directLineSize - 4));
        voice.directLine.fill (0.0f);
        voice.directWriteIndex = 0;
        voice.directPrevious = 0.0f;
        voice.directLowpassState = 0.0f;
        // A contact patch a few millimetres across stops radiating somewhere
        // around 3.5 kHz, and a softer bachi spreads over more of the head.
        const float patchCorner = 3500.0f * (0.55f + 0.75f * applied_.bachiHardness);
        voice.directLowpassCoefficient = 1.0f - std::exp (
            -2.0f * piFloat * std::min (patchCorner, 0.45f * static_cast<float> (sampleRate_))
            * inverseSampleRate_);
    }

    // A flam's main stroke lands 32 ms after its grace note, and a press roll
    // goes on bouncing for a tenth of a second. Those later contacts drive the
    // same modal bank, but a mode's lifetime was measured from the voice's
    // start, so the bank was being retired out from under them - a press roll
    // at high damping reached its last bounce with nine of its thirty modes
    // left. Shift every lifetime by however long the schedule runs.
    if (voice.contactCount > 0)
    {
        const auto& lastContact =
            voice.contacts[static_cast<std::size_t> (voice.contactCount - 1)];
        const auto scheduleEnd = static_cast<std::uint64_t> (lastContact.startSample)
                               + lastContact.lengthSamples;

        // Adding a constant preserves the descending sort, and leaves the modes
        // that were never audible at zero so the trailing trim still finds them.
        voice.retirementOffset = scheduleEnd;
        for (int index = 0; index < voice.modeCount; ++index)
        {
            auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.audibleSamples > 0)
                mode.audibleSamples += scheduleEnd;
        }

        voice.maximumSamples = std::min (
            voice.maximumSamples + scheduleEnd,
            static_cast<std::uint64_t> (maximumTailSeconds * sampleRate_));
    }

    // This slot owns only contact/direct-path transients. The physical tail has
    // its own deadline, extended from the latest hit without resetting any
    // resonator or statistical-band state.
    std::uint64_t scheduleEnd = 0;
    if (voice.contactCount > 0)
    {
        const auto& last = voice.contacts[static_cast<std::size_t> (
            voice.contactCount - 1)];
        scheduleEnd = static_cast<std::uint64_t> (last.startSample)
                    + static_cast<std::uint64_t> (last.lengthSamples);
    }
    const auto directTail = static_cast<std::uint64_t> (
        std::ceil (std::max (voice.directDelayLeft, voice.directDelayRight)))
        + static_cast<std::uint64_t> (0.08 * sampleRate_);
    voice.maximumSamples = scheduleEnd + directTail;

    physical.active = true;
    physical.activeModeCount = physical.modeCount;
    physical.maximumSamples = physical.ageSamples
                            + static_cast<std::uint64_t> (
                                  maximumTailSeconds * sampleRate_);
    physical.retireGain = 1.0f;
    physical.retireStep = 0.0f;

    voice.tuningAtStrike = applied_.pitch + 2.0f * pitchBend_;
    voice.ageSamples = 0;
    voice.peakLevel = 0.0f;
    voice.controlCountdown = 0;
    idleFrozen_ = false;
    silentSamples_ = 0;
    voice.active = voice.activeModeCount > 0 || voice.contactCount > 0;

    updateActiveVoiceCount();

    // Publish the stroke for the editor's head display.
    visualStrikeRadius_.store (voice.strikeRadius, std::memory_order_relaxed);
    visualStrikeAngle_.store (voice.strikeAngle, std::memory_order_relaxed);
    visualArticulation_.store (static_cast<int> (articulation), std::memory_order_relaxed);
    visualOctave_.store (octave, std::memory_order_relaxed);
    visualLevel_ = std::max (visualLevel_, voice.velocity);

    // This atomic belongs only to the lightweight visual snapshot. The editor's
    // numerical readout calls measure(), which runs the exact dry-contact audit
    // off the audio thread. Keep trigger bounded by using the analytic estimate
    // here; doing another contact render in a MIDI callback would undo the
    // shared-bank real-time work above.
    const auto visualPitch = soundingMode (
        drum, voice.strikeRadius, renderedModeCeilingHz (sampleRate_));
    fundamentalHz_.store (visualPitch.frequencyHz, std::memory_order_relaxed);
}

bool TaikoEngine::triggerMidi (int midiNote, float velocity) noexcept
{
    const auto articulation = articulationForMidiNote (midiNote);
    const auto octave = octaveOffsetForMidiNote (midiNote);
    if (! articulation.has_value() || ! octave.has_value())
        return false;

    trigger (*articulation, *octave, velocity);
    return true;
}

// Every membrane mode is moved by the same factor, which is exact for an ideal
// membrane and a first-order approximation once the head has bending stiffness
// and a cavity behind it: neither the stiff term nor the air spring scales with
// the head's tension, so a mode that owes part of its frequency to them should
// move slightly less than the fundamental does. The residue is small over the
// gestures that use this - the attack glide is under a tenth of a semitone of
// tension and the wheel is two - and correcting it exactly would mean carrying
// the tension, stiffness and cavity shares of every mode separately through a
// path that already has to keep the retirement sort, the deadline and the fade
// consistent with each other.
void TaikoEngine::applyTensionShift (Voice& voice, float shift) noexcept
{
    const auto rate = static_cast<float> (sampleRate_);
    const float nyquist = 0.5f * rate;

    for (int index = 0; index < voice.activeModeCount; ++index)
    {
        auto& mode = voice.modes[static_cast<std::size_t> (index)];
        // Stretching the head does not stretch the body it is nailed to, and
        // the bank is sorted by lifetime so the shell modes are scattered
        // through it rather than sitting at the end.
        if (! mode.membrane)
            continue;

        const float frequency = mode.omega * shift / (2.0f * piFloat);
        if (frequency >= nyquist * 0.98f || ! (frequency > 0.0f))
        {
            mode.resonator.a1 = 0.0;
            mode.resonator.a2 = 0.0;
            mode.resonator.b0 = 0.0;
            mode.liveOmega = 0.0;
            mode.poleRadius = 0.0;
            continue;
        }

        // Re-damped, not merely retuned. Most of what damps a mode depends on
        // where the mode is: the hide's loss goes as omega and as omega
        // squared, and the mounting is steeply low-pass, so on a large drum a
        // mode sitting under that corner is losing most of its energy to the
        // stand. Carrying the old rate to the new frequency meant a note
        // automated upward kept the mounting loss of the note it started on and
        // emptied far too fast, and a note automated down kept too little and
        // rang past where it should have stopped.
        mode.decayRate = membraneDecayAt (voice, mode, 2.0f * piFloat * frequency);

        // In double, exactly as configureResonator does it. This path runs on
        // every stroke that has any Tension Mod at all - which is the default -
        // and again on every wheel move and Pitch automation step, so rounding
        // the coefficients here would put the mistuning straight back after the
        // careful build.
        const auto omega = 2.0 * static_cast<double> (piFloat)
                         * static_cast<double> (frequency) / sampleRate_;
        const auto poleRadius = std::exp (-static_cast<double> (
                                              mode.decayRate
                                              + mode.appliedPalmDecay)
                                          / sampleRate_);
        mode.resonator.a1 = -2.0 * poleRadius * std::cos (omega);
        mode.resonator.a2 = poleRadius * poleRadius;
        mode.resonator.b0 = std::sin (omega);
        mode.liveOmega = 2.0 * static_cast<double> (piFloat)
                       * static_cast<double> (frequency);
        mode.poleRadius = poleRadius;

        // The state is deliberately left where it is. Rewriting a1 and a2 under
        // a running (y1, y2) does move the next output by da1*y[n-1] +
        // da2*y[n-2], and it is tempting to read the pair's amplitude and phase
        // out of the old pole and write them back at the new one so that the
        // coefficient change is continuous in the output rather than only in
        // the poles. That was prototyped and measured, and it is not worth
        // having. What the rewrite actually leaves behind, measured over 30 to
        // 80 ms after a full-velocity Don with the head's continuum silenced
        // and an eight-pole high-pass run from the strike so that nothing in
        // the number is a window artefact, is -119.9 dB above 1.2 kHz against
        // a stroke at -18.3 dB, and Tension Mod 0 to 1 moves it by -0.00 dB.
        // Rotating the state does not lower that. It raises it, above 4 kHz
        // from -183.2 dB to -164.4 dB, because the shift it would then be
        // tracking exactly is a peak follower over the modal states and is
        // itself corner-rich at audio rate, and a state that lags the retune
        // smooths that where a state that follows it does not. It also moves
        // the 400 Hz to 16 kHz band enough across four sample rates to fail
        // testTheContinuumDoesNotDependOnTheSampleRate. The measurement is
        // kept in testTheGlideDoesNotBrightenTheTopOfTheSpectrum; the reasoning
        // is under gap 14 in the second pass of the plan.
    }

    // The continuum is the same head above where its modes can be told apart,
    // so it moves with them. Leaving it fixed let a two-octave Pitch move carry
    // the resolved bank away while the region that now dominates the upper
    // spectrum stood still, and put a smaller version of the same mismatch into
    // the attack glide of every hard stroke.
    for (auto& band : voice.continuum)
    {
        if (! (band.centre > 0.0f))
            continue;

        const float centre = band.centre * shift;
        const float low = std::min (centre / continuumBandwidth, nyquist * 0.9f);
        const float high = std::min (centre * continuumBandwidth, nyquist * 0.9f);
        band.lowCoefficient = 1.0f - std::exp (-2.0f * piFloat * low / rate);
        band.highCoefficient = 1.0f - std::exp (-2.0f * piFloat * high / rate);

        // And its decay with it, for the same reason the modes' does: the loss
        // that empties this region is the head's own, and that is a function of
        // where the band now sits rather than of where it was built.
        const float bandOmega = 2.0f * piFloat * centre;
        const float bandDecay = band.lossFixed
                              + voice.continuumLossOmega * bandOmega
                              + voice.continuumLossOmegaSquared * bandOmega * bandOmega;
        band.envelopeDecay = std::exp (-bandDecay / rate);
    }

    // A canonical drum's lifetime is twelve seconds from its latest contact,
    // not from the instant the bank was first created. The legacy per-strike
    // recomputation below uses absolute voice age and clamps the deadline to
    // twelve seconds from zero; applying it to a shared bank would undo a
    // retrigger's extension and could cut a new hit a few milliseconds later.
    if (voice.physicalBank)
    {
        voice.appliedTensionShift = shift;
        return;
    }

    // The lifetimes were worked out from the rates the modes were built with,
    // and those rates have just moved. A mode that has been slowed has to be
    // allowed to finish, or it is cut off mid-ring - a step in the output
    // rather than a mode quietly ending.
    //
    // Lengthened only, never shortened. The retirement walk drops modes from
    // the end of a bank sorted by lifetime, and once the rates move apart that
    // sort no longer strictly holds; extending an entry can only leave a mode
    // active past its floor, which costs a resonator, while shortening one
    // could drop a mode the walk has not reached yet while it is still
    // sounding, which costs a click. The wooden bank is not touched at all -
    // stretching the head does not stretch the body it is nailed to.
    std::uint64_t longest = 0;
    for (int index = 0; index < voice.modeCount; ++index)
    {
        auto& mode = voice.modes[static_cast<std::size_t> (index)];
        if (! mode.membrane || mode.retirementLog <= 0.0f || mode.decayRate <= 0.0f)
            continue;

        const float seconds = mode.retirementLog / mode.decayRate;
        const float bounded = clampFloat (seconds, 0.0f,
                                          static_cast<float> (maximumTailSeconds));
        const auto samples = static_cast<std::uint64_t> (bounded * rate)
                           + voice.retirementOffset;
        mode.audibleSamples = std::max (mode.audibleSamples, samples);
        longest = std::max (longest, mode.audibleSamples);
    }

    // And the voice's own deadline with them. It is the longest mode plus a
    // little, and the render loop silences everything at it: extending the
    // modes without extending this would move where a retuned drum is cut off
    // from the mode to the voice and change nothing else. The cap on the whole
    // tail still stands - that one is a promise to the host about latency, not
    // a statement about the drum.
    if (longest > 0)
        voice.maximumSamples = std::min (
            std::max (voice.maximumSamples,
                      longest + static_cast<std::uint64_t> (rate * 0.02f)),
            static_cast<std::uint64_t> (maximumTailSeconds * sampleRate_));

    // And a fade already armed against the old deadline has to be called off,
    // or extending the deadline achieves nothing: the render loop is
    // multiplying the voice down to zero on the old schedule and would simply
    // hold it there until the later one arrived. The arming test runs before
    // the glide does, so a bend that lands inside the last sixty milliseconds
    // of a voice is exactly the case this happens in.
    //
    // Only while the fade has not actually moved the gain yet. Once it has,
    // the voice is already some way down and putting it back to unity would be
    // a step up rather than a rescue - a click, and a louder one the longer the
    // fade has run. A fade that has started is left to finish; what this
    // catches is the far commoner case of one armed a moment ago and not yet
    // acted on.
    const auto fadeSamples =
        static_cast<std::uint64_t> (forcedFadeSeconds * sampleRate_);
    if (voice.retireStep > 0.0f
        && voice.retireGain >= 1.0f
        && voice.ageSamples + fadeSamples < voice.maximumSamples)
        voice.retireStep = 0.0f;

    voice.appliedTensionShift = shift;
}

void TaikoEngine::updateVoiceControl (Voice& voice) noexcept
{
    // Arm the forced fade once the hard cap is close enough that the voice
    // would otherwise be cut while still sounding.
    if (voice.retireStep <= 0.0f)
    {
        const auto fadeSamples =
            static_cast<std::uint64_t> (forcedFadeSeconds * sampleRate_);
        if (voice.maximumSamples > fadeSamples
            && voice.ageSamples + fadeSamples >= voice.maximumSamples)
            voice.retireStep = 1.0f / static_cast<float> (fadeSamples);
    }

    // Retire the modes that have fallen below the floor. They are stored in
    // descending order of lifetime, so this is a walk from the end.
    while (! voice.physicalBank && voice.activeModeCount > 0
           && voice.modes[static_cast<std::size_t> (voice.activeModeCount - 1)]
                      .audibleSamples
                  <= voice.ageSamples)
    {
        --voice.activeModeCount;
        voice.modes[static_cast<std::size_t> (voice.activeModeCount)].resonator.clear();
    }

    // CC1 and Tsu are palms on the hide, not gain controls. Give each mode a
    // continuous extra pole decay derived from its finite-area velocity-loss
    // rate. Updating the radius only at control rate is cheap; the new radius
    // then acts on every audio sample, so a high mode cannot evade the hand by
    // landing near a turning point whenever a 32-sample damping kick arrives.
    const float handAmount = handDamping_ * handDamping_;
    const bool localMuteActive = voice.localMuteTicksRemaining > 0;
    const bool palmRequested = handAmount > 1.0e-6f || localMuteActive;
    if (voice.physicalBank && (palmRequested || voice.palmDampingActive))
    {
        const float secondsPerTick = static_cast<float> (controlPeriod)
                                   / static_cast<float> (sampleRate_);
        for (int index = 0; index < voice.activeModeCount; ++index)
        {
            auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane)
                continue;
            const float velocityLoss = mode.handDampingRate * handAmount
                                     + (localMuteActive
                                            ? mode.localMuteDampingRate : 0.0f);
            setPalmDecay (mode, 0.5f * velocityLoss);
        }
        voice.palmDampingActive = palmRequested;

        // The statistical field's state is already RMS amplitude, so it can
        // take the equivalent phase-averaged exponent directly at this tick.
        const float continuumVelocityLoss =
            voice.continuumHandDampingRate * handAmount
            + (localMuteActive ? voice.continuumMuteDampingRate : 0.0f);
        const float continuumGain = std::exp (
            -0.5f * continuumVelocityLoss * secondsPerTick);
        for (auto& band : voice.continuum)
            band.envelope *= continuumGain;
    }

    if (localMuteActive)
    {
        --voice.localMuteTicksRemaining;
        if (voice.localMuteTicksRemaining == 0)
        {
            for (int index = 0; index < voice.modeCount; ++index)
                voice.modes[static_cast<std::size_t> (index)].localMuteDampingRate = 0.0f;
            voice.continuumMuteDampingRate = 0.0f;
        }
    }

    // The attack glide and the wheel are one thing: both press the head, both
    // raise its tension, and both therefore scale the membrane's frequencies.
    // Applying them together also means a stroke that is already ringing bends
    // with the wheel, rather than the wheel only reaching the next stroke.
    //
    // The glide is the head's own doing. A membrane clamped at its rim gets
    // longer when it moves, so its tension rises with the area-mean square of
    // its slope - the Berger/von Karman term - and the frequency with the
    // square root of the tension. The Bessel gradient norm weights each mode;
    // the two cavity-coupled axisymmetric branches are combined coherently
    // because they are two coordinates of the same batter-head shape.
    //
    // A short peak follower removes cycle-rate ripple but contributes no attack
    // schedule: the glide is over when the head has stopped moving. That is why
    // a slack o-daiko bends further and for longer than a shime-daiko does at
    // four times the tension.
    const float liveTensionDepth = voice.tensionDepth * applied_.tensionModulation;
    if (liveTensionDepth > 0.0f)
    {
        std::array<double, modeEntryCount> axisymmetricAmplitude {};
        std::array<double, modeEntryCount> axisymmetricNorm {};
        double squaredSlope = 0.0;
        for (int index = 0; index < voice.activeModeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane)
                continue;
            const double state = mode.resonator.y1;
            if (mode.circumferentialOrder == 0)
            {
                const auto entry = static_cast<std::size_t> (mode.modeEntry);
                axisymmetricAmplitude[entry] +=
                    static_cast<double> (mode.batterParticipation) * state;
                axisymmetricNorm[entry] = mode.stretchNorm;
            }
            else
            {
                squaredSlope += static_cast<double> (mode.stretchNorm)
                              * state * state;
            }
        }
        for (std::size_t entry = 0; entry < axisymmetricAmplitude.size(); ++entry)
            squaredSlope += axisymmetricNorm[entry]
                          * axisymmetricAmplitude[entry]
                          * axisymmetricAmplitude[entry];

        voice.tensionEnvelope = std::max (
            static_cast<float> (squaredSlope),
            voice.tensionEnvelope * voice.tensionDecay);
    }
    else
    {
        voice.tensionEnvelope = 0.0f;
    }

    // The voice's modes were built with whatever tuning stood at the strike, so
    // only the movement since then is left to apply. Both the Pitch control and
    // the wheel are head tension - a host automating Pitch through a ringing
    // tail retunes it for the same reason the wheel does - so they are carried
    // together as one offset in semitones.
    const float tuningNow = applied_.pitch + 2.0f * pitchBend_;
    const float raw = liveTensionDepth * voice.tensionEnvelope;
    const float rise = tensionStretchLimit * raw / (tensionStretchLimit + raw);
    const float stretch = std::sqrt (1.0f + rise);
    const float shift = stretch * std::exp2 ((tuningNow - voice.tuningAtStrike) / 12.0f);

    if (std::abs (shift - voice.appliedTensionShift) > 1.0e-5f)
        applyTensionShift (voice, shift);
}

void TaikoEngine::advancePhysicalContacts (Voice& physical) noexcept
{
    std::array<Voice*, maxVoices> contacts {};
    int contactCount = 0;
    for (auto& voice : voices_)
        if (voice.active && voice.nonlinearContactActive
            && voice.octaveOffset == physical.octaveOffset)
            contacts[static_cast<std::size_t> (contactCount++)] = &voice;

    if (contactCount == 0)
        return;

    // Slot reuse must not make a simultaneous solve order-dependent. Stroke
    // order is stable even when an overflowing transient pool chooses different
    // storage slots.
    std::sort (contacts.begin(), contacts.begin() + contactCount,
               [] (const Voice* left, const Voice* right)
               {
                   return left->startOrder < right->startOrder;
               });

    const double h = 1.0 / sampleRate_;
    const double hSquared = h * h;
    constexpr double inverseModelScale = 1.0 / static_cast<double> (modelScale);

    std::array<double, maxVoices> xFree {};
    std::array<double, maxVoices> deltaPrevious {};
    std::array<double, maxVoices> deltaCurrent {};
    std::array<double, maxVoices> sFree {};
    std::array<double, maxVoices> midpointCompression {};
    std::array<double, maxVoices> s {};
    std::array<double, maxVoices> force {};
    std::array<double, maxVoices> forceDerivative {};
    std::array<double, resonatorCount> matchedCompliance {};
    std::array<std::array<double, maxVoices>, maxVoices> compliance {};

    for (int contact = 0; contact < contactCount; ++contact)
    {
        auto& voice = *contacts[static_cast<std::size_t> (contact)];
        xFree[static_cast<std::size_t> (contact)] =
            2.0 * voice.stickPosition - voice.stickPrevious;
        compliance[static_cast<std::size_t> (contact)]
                  [static_cast<std::size_t> (contact)] =
            hSquared / voice.stickMass;
    }

    // Each exact free pole is matched by the IMP-2 compliance
    //   C = h^2 (1 + r^2 + 2 r cos(theta)) / (4 M)
    //     = h^2 (1 + a2 - a1) / (4 M).
    // The same p senses the surface and spreads force, making the contact
    // matrix symmetric positive definite and simultaneous sticks independent
    // of processing order.
    for (int index = 0; index < physical.modeCount; ++index)
    {
        const auto& mode = physical.modes[static_cast<std::size_t> (index)];
        if (! mode.membrane || ! (mode.inverseModalMass > 0.0f)
            || std::abs (mode.resonator.b0) < 1.0e-14)
            continue;

        const auto id = static_cast<std::size_t> (mode.physicalIndex);
        const double c = 0.25 * hSquared
                       * static_cast<double> (mode.inverseModalMass)
                       * (1.0 + mode.resonator.a2 - mode.resonator.a1);
        if (! (c > 0.0) || ! std::isfinite (c))
            continue;
        matchedCompliance[id] = c;

        const double qCurrent = mode.resonator.y1 * inverseModelScale;
        const double qPrevious = mode.resonator.y2 * inverseModelScale;
        const double qFree = (-mode.resonator.a1 * mode.resonator.y1
                              - mode.resonator.a2 * mode.resonator.y2)
                           * inverseModelScale;

        for (int contact = 0; contact < contactCount; ++contact)
        {
            const double p = contacts[static_cast<std::size_t> (contact)]
                                 ->contactProjection[id];
            deltaCurrent[static_cast<std::size_t> (contact)] -= p * qCurrent;
            deltaPrevious[static_cast<std::size_t> (contact)] -= p * qPrevious;
            sFree[static_cast<std::size_t> (contact)] -= p * qFree;
        }

        for (int row = 0; row < contactCount; ++row)
        {
            const double pRow = contacts[static_cast<std::size_t> (row)]
                                    ->contactProjection[id];
            for (int column = 0; column < contactCount; ++column)
            {
                const double pColumn = contacts[static_cast<std::size_t> (column)]
                                           ->contactProjection[id];
                compliance[static_cast<std::size_t> (row)]
                          [static_cast<std::size_t> (column)] += c * pRow * pColumn;
            }
        }
    }

    for (int contact = 0; contact < contactCount; ++contact)
    {
        auto& voice = *contacts[static_cast<std::size_t> (contact)];
        const auto slot = static_cast<std::size_t> (contact);
        deltaCurrent[slot] += voice.stickPosition;
        deltaPrevious[slot] += voice.stickPrevious;
        sFree[slot] += xFree[slot] - deltaPrevious[slot];
        midpointCompression[slot] =
            0.5 * (deltaCurrent[slot] + deltaPrevious[slot]);
        s[slot] = sFree[slot];

        // A second bachi can be triggered while the head is moving away faster
        // than the stick. Its actual collision then begins later, when the
        // membrane turns back. Sun's constrained Hunt-Crossley eta is defined
        // from that onset velocity, not from the MIDI-time estimate, so update
        // it while the contact is still in free approach and freeze it at the
        // first positive force.
        if (! voice.nonlinearContactHasForce && sFree[slot] > 0.0)
        {
            const double impactSpeed = std::max (sFree[slot] / (2.0 * h), 0.05);
            voice.contactDamping = contactDampingFactor / impactSpeed;
        }
    }

    const auto evaluateLaw = [h] (const Voice& voice, double midpoint, double step,
                                  double& value, double& derivative) noexcept
    {
        constexpr double alpha = 1.5;
        constexpr double alphaPlusOne = alpha + 1.0;
        const auto positive = [] (double x) noexcept { return std::max (x, 0.0); };

        // z and sqrt(z) at the midpoint and at the half-step-ahead point each
        // feed both the potential and its slope below; solving for sqrt(z)
        // once per point and reusing it avoids recomputing an identical square
        // root up to three times per call.
        const double z0 = positive (midpoint);
        const double sqrtZ0 = std::sqrt (z0);
        const double phi0 = voice.contactStiffness / alphaPlusOne * z0 * z0 * sqrtZ0;

        const double nextMidpoint = midpoint + 0.5 * step;
        const double z1 = positive (nextMidpoint);
        const double sqrtZ1 = std::sqrt (z1);
        const double phi1 = voice.contactStiffness / alphaPlusOne * z1 * z1 * sqrtZ1;

        const double scale = 1.0 + std::abs (midpoint);
        double discreteGradient = 0.0;
        double gradientDerivative = 0.0;
        if (std::abs (step) > 1.0e-12 * scale)
        {
            const double difference = phi1 - phi0;
            discreteGradient = 2.0 * difference / step;
            // potentialSlope (nextMidpoint), inlined to reuse z1 / sqrtZ1.
            const double potentialSlopeNext = voice.contactStiffness * z1 * sqrtZ1;
            gradientDerivative = (potentialSlopeNext * step
                                  - 2.0 * difference) / (step * step);
        }
        else
        {
            // potentialSlope (midpoint), inlined to reuse z0 / sqrtZ0.
            discreteGradient = voice.contactStiffness * z0 * sqrtZ0;
            gradientDerivative = 0.25 * alpha * voice.contactStiffness * sqrtZ0;
        }

        // Roundoff at a vanishing crossing can only make these infinitesimally
        // negative; convexity says their physical values are non-negative.
        discreteGradient = std::max (discreteGradient, 0.0);
        gradientDerivative = std::max (gradientDerivative, 0.0);
        const double multiplier = 1.0 + voice.contactDamping * step / (2.0 * h);
        if (! (multiplier > 0.0))
        {
            value = 0.0;
            derivative = 0.0;
            return;
        }

        value = discreteGradient * multiplier;
        derivative = gradientDerivative * multiplier
                   + discreteGradient * voice.contactDamping / (2.0 * h);
    };

    const auto residual = [&] (const std::array<double, maxVoices>& candidate,
                               std::array<double, maxVoices>* values,
                               std::array<double, maxVoices>* derivatives,
                               std::array<double, maxVoices>* equations) noexcept
    {
        std::array<double, maxVoices> localForce {};
        std::array<double, maxVoices> localDerivative {};
        for (int contact = 0; contact < contactCount; ++contact)
            evaluateLaw (*contacts[static_cast<std::size_t> (contact)],
                         midpointCompression[static_cast<std::size_t> (contact)],
                         candidate[static_cast<std::size_t> (contact)],
                         localForce[static_cast<std::size_t> (contact)],
                         localDerivative[static_cast<std::size_t> (contact)]);

        double norm = 0.0;
        for (int row = 0; row < contactCount; ++row)
        {
            const auto rowSlot = static_cast<std::size_t> (row);
            double equation = candidate[rowSlot] - sFree[rowSlot];
            for (int column = 0; column < contactCount; ++column)
                equation += compliance[rowSlot][static_cast<std::size_t> (column)]
                          * localForce[static_cast<std::size_t> (column)];
            if (equations != nullptr)
                (*equations)[rowSlot] = equation;
            norm = std::max (norm, std::abs (equation));
        }
        if (values != nullptr)
            *values = localForce;
        if (derivatives != nullptr)
            *derivatives = localDerivative;
        return norm;
    };

    double scale = 1.0;
    for (int contact = 0; contact < contactCount; ++contact)
        scale = std::max (scale, std::abs (sFree[static_cast<std::size_t> (contact)]));
    const double tolerance = 2.0e-12 * scale;
    bool converged = false;

    for (int iteration = 0; iteration < 14; ++iteration)
    {
        std::array<double, maxVoices> equations {};
        const double norm = residual (s, &force, &forceDerivative, &equations);
        if (norm <= tolerance)
        {
            converged = true;
            break;
        }

        std::array<std::array<double, maxVoices>, maxVoices> jacobian {};
        std::array<double, maxVoices> increment {};
        for (int row = 0; row < contactCount; ++row)
        {
            const auto rowSlot = static_cast<std::size_t> (row);
            increment[rowSlot] = -equations[rowSlot];
            for (int column = 0; column < contactCount; ++column)
            {
                const auto columnSlot = static_cast<std::size_t> (column);
                jacobian[rowSlot][columnSlot] = (row == column ? 1.0 : 0.0)
                    + compliance[rowSlot][columnSlot] * forceDerivative[columnSlot];
            }
        }

        bool solvable = true;
        for (int pivot = 0; pivot < contactCount; ++pivot)
        {
            int best = pivot;
            for (int row = pivot + 1; row < contactCount; ++row)
                if (std::abs (jacobian[static_cast<std::size_t> (row)]
                                      [static_cast<std::size_t> (pivot)])
                    > std::abs (jacobian[static_cast<std::size_t> (best)]
                                        [static_cast<std::size_t> (pivot)]))
                    best = row;
            if (std::abs (jacobian[static_cast<std::size_t> (best)]
                                  [static_cast<std::size_t> (pivot)]) < 1.0e-14)
            {
                solvable = false;
                break;
            }
            if (best != pivot)
            {
                std::swap (jacobian[static_cast<std::size_t> (best)],
                           jacobian[static_cast<std::size_t> (pivot)]);
                std::swap (increment[static_cast<std::size_t> (best)],
                           increment[static_cast<std::size_t> (pivot)]);
            }
            for (int row = pivot + 1; row < contactCount; ++row)
            {
                const double factor = jacobian[static_cast<std::size_t> (row)]
                                              [static_cast<std::size_t> (pivot)]
                                    / jacobian[static_cast<std::size_t> (pivot)]
                                              [static_cast<std::size_t> (pivot)];
                for (int column = pivot; column < contactCount; ++column)
                    jacobian[static_cast<std::size_t> (row)]
                            [static_cast<std::size_t> (column)] -=
                        factor * jacobian[static_cast<std::size_t> (pivot)]
                                         [static_cast<std::size_t> (column)];
                increment[static_cast<std::size_t> (row)] -=
                    factor * increment[static_cast<std::size_t> (pivot)];
            }
        }
        if (! solvable)
            break;

        for (int row = contactCount - 1; row >= 0; --row)
        {
            double value = increment[static_cast<std::size_t> (row)];
            for (int column = row + 1; column < contactCount; ++column)
                value -= jacobian[static_cast<std::size_t> (row)]
                                 [static_cast<std::size_t> (column)]
                       * increment[static_cast<std::size_t> (column)];
            increment[static_cast<std::size_t> (row)] =
                value / jacobian[static_cast<std::size_t> (row)]
                                [static_cast<std::size_t> (row)];
        }

        bool accepted = false;
        double fraction = 1.0;
        for (int search = 0; search < 10; ++search)
        {
            auto candidate = s;
            for (int contact = 0; contact < contactCount; ++contact)
                candidate[static_cast<std::size_t> (contact)] +=
                    fraction * increment[static_cast<std::size_t> (contact)];
            const double candidateNorm = residual (candidate, nullptr, nullptr, nullptr);
            if (std::isfinite (candidateNorm) && candidateNorm < norm)
            {
                s = candidate;
                accepted = true;
                break;
            }
            fraction *= 0.5;
        }
        if (! accepted)
            break;
    }

    const double finalResidual = residual (s, &force, nullptr, nullptr);
    if (! converged && finalResidual <= 16.0 * tolerance)
        converged = true;
    if (! converged)
    {
        // A failed solve must never manufacture energy or an adhesive force.
        // Free flight is the passive fallback; the still-active contact retries
        // on the next sample rather than committing a non-finite state.
        force.fill (0.0);
        s = sFree;
    }

    for (int contact = 0; contact < contactCount; ++contact)
    {
        auto& voice = *contacts[static_cast<std::size_t> (contact)];
        const auto slot = static_cast<std::size_t> (contact);
        const double next = xFree[slot] - hSquared * force[slot] / voice.stickMass;
        voice.stickPrevious = voice.stickPosition;
        voice.stickPosition = next;
        voice.solvedContactForce = static_cast<float> (force[slot]);
        // The stochastic field is currently an observed high-mode residual,
        // not a memoryless mechanical dashpot. A pure resistance captured soft
        // and edge strikes for several milliseconds because it omitted the
        // reactive storage that returns energy from real high modes. Use the
        // solved force history to drive the calibrated observation until that
        // omitted-mode impedance is represented by fitted dynamic states.
        // contactEnergyAdmittance is (membraneGain * levelScale)^2 /
        // residualImpedance, cached once by trigger() since neither operand
        // changes for the life of the contact - see the field comment.
        voice.solvedContactEnergyStep = h * force[slot] * force[slot]
                                      * voice.contactEnergyAdmittance;
        if (force[slot] > 1.0e-6)
            voice.nonlinearContactHasForce = true;

        const double nextMidpoint = midpointCompression[slot] + 0.5 * s[slot];
        if (voice.nonlinearContactHasForce
            && nextMidpoint <= 0.0 && s[slot] <= 0.0)
            voice.nonlinearContactActive = false;
    }

    for (int index = 0; index < physical.modeCount; ++index)
    {
        const auto& mode = physical.modes[static_cast<std::size_t> (index)];
        if (! mode.membrane)
            continue;
        const auto id = static_cast<std::size_t> (mode.physicalIndex);
        const double c = matchedCompliance[id];
        if (! (c > 0.0) || std::abs (mode.resonator.b0) < 1.0e-14)
            continue;

        double generalisedForce = 0.0;
        for (int contact = 0; contact < contactCount; ++contact)
            generalisedForce += contacts[static_cast<std::size_t> (contact)]
                                    ->contactProjection[id]
                              * force[static_cast<std::size_t> (contact)];
        const double storedIncrement = static_cast<double> (modelScale) * c
                                     * generalisedForce;
        physical.modalInput[id] += static_cast<float> (
            storedIncrement / mode.resonator.b0);
    }
}

float TaikoEngine::renderVoice (Voice& voice, Voice* physical,
                                float& rightOut) noexcept
{
    const bool nonlinearContact = voice.nonlinearContactActive
                               || voice.nonlinearContactHasForce;
    // Start the next scheduled contact if it is due.
    if (! nonlinearContact && voice.contactRemaining == 0u
        && voice.nextContact < voice.contactCount)
    {
        const auto& contact =
            voice.contacts[static_cast<std::size_t> (voice.nextContact)];
        if (voice.ageSamples >= contact.startSample)
        {
            voice.contactRemaining = contact.lengthSamples;
            voice.contactLength = contact.lengthSamples;
            voice.contactAmplitude = contact.amplitude;
            voice.contactNoiseAmplitude = contact.noiseAmplitude;
            ++voice.nextContact;

            // Every impact lights the head's continuum, not just the first. A
            // flam is two strikes and a press roll is seven, and the later ones
            // are real blows on the head - leaving them out gave the grace note
            // the whole stroke's brightness and the main hit thirty-two
            // milliseconds later nothing but the residue.
            //
            // In proportion to how hard this particular contact lands, and
            // taken as the louder of what is already ringing and what the new
            // blow brings, so a bounce cannot cut the tail of the one before.
            if (voice.contactReference > 0.0f && physical != nullptr)
            {
                const float share = contact.amplitude / voice.contactReference;
                for (std::size_t index = 0; index < voice.continuum.size(); ++index)
                {
                    if (! (physical->continuum[index].centre > 0.0f))
                        continue;
                    const float injection = voice.continuumInjection[index] * share;
                    auto& destination = physical->continuum[index].envelope;
                    // Distinct unresolved modes add as energy, not as coherent
                    // amplitude and not by replacing the older tail.
                    destination = std::hypot (destination, injection);
                }
            }
        }
    }

    float force = 0.0f;
    float contactEnvelope = 0.0f;

    if (nonlinearContact)
    {
        // articulationLevelScale is strikeProfile(voice.articulation).levelScale,
        // cached once by trigger() since articulation never changes for the
        // life of the voice - see the field comment - so the per-sample
        // nonlinear-contact path can read it directly instead of looking the
        // profile up by articulation on every rendered sample.
        force = std::max (voice.solvedContactForce, 0.0f)
              * voice.articulationLevelScale;
        contactEnvelope = force
            / std::max (voice.contactAmplitude, 1.0e-9f);

        // Feed the unresolved field from the actual solved contact, one energy
        // increment at a time. Its existing calibration describes the complete
        // reference Hertz pulse, so each sample contributes the square root of
        // its share of that pulse's F^2/Z energy; hypot then adds the independent
        // high modes in energy. This removes the old full-strength burst that
        // appeared on the first positive force sample regardless of what the
        // moving head let the stick deliver.
        if (voice.solvedContactEnergyStep > 0.0
            && voice.referenceContactEnergy > 0.0 && physical != nullptr)
        {
            const float share = static_cast<float> (std::sqrt (
                voice.solvedContactEnergyStep / voice.referenceContactEnergy));
            for (std::size_t index = 0; index < voice.continuumInjection.size(); ++index)
            {
                if (! (physical->continuum[index].centre > 0.0f))
                    continue;
                auto& destination = physical->continuum[index].envelope;
                destination = std::hypot (destination,
                                          voice.continuumInjection[index] * share);
            }
            voice.continuumInjected = true;
        }
    }
    else if (voice.contactRemaining > 0u)
    {
        const float position =
            1.0f - static_cast<float> (voice.contactRemaining)
                       / static_cast<float> (voice.contactLength);
        // Hertz contact force: a sin^1.5 arch rather than a symmetric bump,
        // because a rounded tip loads the surface faster than it leaves it.
        const float arch = std::sin (piFloat * position);
        contactEnvelope = arch > 0.0f ? arch * std::sqrt (arch) : 0.0f;
        force = voice.contactAmplitude * contactEnvelope;
        --voice.contactRemaining;
    }

    // Broadband contact noise. Most of it goes into the head, because that is
    // what it is: the stick dragging across the hide as it lands.
    float noise = 0.0f;
    if (contactEnvelope > 0.0f && voice.contactNoiseAmplitude > 0.0f)
    {
        const float white = nextNoise (voice.noiseState);
        voice.noiseBandState += voice.noiseBandCoefficient * (white - voice.noiseBandState);
        noise = (white - voice.noiseBandState) * voice.contactNoiseAmplitude
              * contactEnvelope;
    }

    const float excitation = force + noise;

    // The tack line, which rattles only once the stroke has beaten the preload
    // holding the head down. It used to be a fixed 0.08 of broadband noise
    // added to a contact whose amplitude is in the thousands of newtons - 87 dB
    // under the stroke, inaudible at every setting, and answering to nothing:
    // not the Stick Noise control, not the velocity, not the drum. This is the
    // same idea done as a threshold, so it appears when a rim shot lands hard
    // and is simply absent below that. It goes out through the air rather than
    // into the head, because a chattering nail is a small metal source at the
    // rim and not something driving the membrane.
    float tack = 0.0f;
    if (voice.tackScale > 0.0f)
    {
        if (force > 0.0f)
        {
            const float excess = force * voice.tackRimGain - voice.tackPreload;
            if (excess > 0.0f)
                voice.tackEnvelope = std::max (voice.tackEnvelope, excess);
        }

        if (voice.tackEnvelope > 1.0e-3f)
        {
            const float white = nextNoise (voice.tackNoiseState);
            voice.tackLowState += voice.tackLowCoefficient * (white - voice.tackLowState);
            const float above = white - voice.tackLowState;
            voice.tackHighState +=
                voice.tackHighCoefficient * (above - voice.tackHighState);
            tack = voice.tackHighState * voice.tackEnvelope * voice.tackScale;
            voice.tackEnvelope *= voice.tackEnvelopeDecay;
        }
        else
        {
            voice.tackEnvelope = 0.0f;
        }
    }

    float left = 0.0f;
    float right = 0.0f;

    // The head and the body are summed apart so a hand laid on the head can
    // damp what it is actually touching.
    float membraneLeft = 0.0f;
    float membraneRight = 0.0f;

    // The head's high-frequency continuum: everything above the resolved bank,
    // as bands of noise gated by the strike and decaying from the top down. It
    // is part of the head, so it is summed with the head and damped with it.
    if (voice.physicalBank)
    for (auto& band : voice.continuum)
    {
        if (band.envelope <= 1.0e-5f || band.level <= 0.0f)
            continue;

        const float shared = nextNoise (voice.noiseState);
        const float sideLeft = nextNoise (voice.noiseState);
        const float sideRight = nextNoise (voice.noiseState);
        const float inLeft = band.common * shared + band.independent * sideLeft;
        const float inRight = band.common * shared + band.independent * sideRight;

        // Two high-pass stages at the lower edge, then seven low-pass stages at
        // the upper one. The previous difference of two low-pass cascades had
        // only a first-order zero at DC: both sides approach one there, so the
        // leading terms of their difference leaked the loud crossover band up
        // through every octave above it. This serial cascade has a real
        // twelve-decibel-per-octave lower skirt and a forty-two-decibel upper
        // skirt. The asymmetry is intentional: the contact and loss laws make
        // the crossover bands the persistent masking risk, so their upward
        // leakage must die before the higher octaves begin.
        band.lowStateLeft += band.lowCoefficient * (inLeft - band.lowStateLeft);
        const float highPassedLeft = inLeft - band.lowStateLeft;
        band.lowStateLeft2 +=
            band.lowCoefficient * (highPassedLeft - band.lowStateLeft2);
        const float highPassedLeft2 = highPassedLeft - band.lowStateLeft2;
        band.highStateLeft +=
            band.highCoefficient * (highPassedLeft2 - band.highStateLeft);
        band.highStateLeft2 +=
            band.highCoefficient * (band.highStateLeft - band.highStateLeft2);
        band.highStateLeft3 +=
            band.highCoefficient * (band.highStateLeft2 - band.highStateLeft3);
        band.highStateLeft4 +=
            band.highCoefficient * (band.highStateLeft3 - band.highStateLeft4);
        band.highStateLeft5 +=
            band.highCoefficient * (band.highStateLeft4 - band.highStateLeft5);
        band.highStateLeft6 +=
            band.highCoefficient * (band.highStateLeft5 - band.highStateLeft6);
        band.highStateLeft7 +=
            band.highCoefficient * (band.highStateLeft6 - band.highStateLeft7);

        band.lowStateRight += band.lowCoefficient * (inRight - band.lowStateRight);
        const float highPassedRight = inRight - band.lowStateRight;
        band.lowStateRight2 +=
            band.lowCoefficient * (highPassedRight - band.lowStateRight2);
        const float highPassedRight2 = highPassedRight - band.lowStateRight2;
        band.highStateRight +=
            band.highCoefficient * (highPassedRight2 - band.highStateRight);
        band.highStateRight2 +=
            band.highCoefficient * (band.highStateRight - band.highStateRight2);
        band.highStateRight3 +=
            band.highCoefficient * (band.highStateRight2 - band.highStateRight3);
        band.highStateRight4 +=
            band.highCoefficient * (band.highStateRight3 - band.highStateRight4);
        band.highStateRight5 +=
            band.highCoefficient * (band.highStateRight4 - band.highStateRight5);
        band.highStateRight6 +=
            band.highCoefficient * (band.highStateRight5 - band.highStateRight6);
        band.highStateRight7 +=
            band.highCoefficient * (band.highStateRight6 - band.highStateRight7);

        const float gain = band.level * band.envelope;
        band.envelope *= band.envelopeDecay;
        membraneLeft += band.highStateLeft7 * gain;
        membraneRight += band.highStateRight7 * gain;
    }

    if (! voice.physicalBank)
    {
        if (physical != nullptr)
        {
            if (nonlinearContact)
            {
                // The normal membrane force was integrated by the reciprocal
                // IMP-2 solve. Texture remains an explicitly external roughness
                // source, while the wooden/rim bank follows the solved force
                // through its established linear observation path.
                for (std::size_t index = 0; index < voice.modeProjection.size(); ++index)
                {
                    float input = noise * voice.modeProjection[index];
                    if (index >= static_cast<std::size_t> (membraneResonatorCount))
                        input += force * voice.modeProjection[index];
                    physical->modalInput[index] += input;
                }
            }
            else if (excitation != 0.0f)
            {
                for (std::size_t index = 0; index < voice.modeProjection.size(); ++index)
                    physical->modalInput[index] += excitation * voice.modeProjection[index];
            }
        }
    }

    const int renderModeCount = voice.physicalBank ? voice.activeModeCount : 0;
    for (int index = 0; index < renderModeCount; ++index)
    {
        auto& mode = voice.modes[static_cast<std::size_t> (index)];
        // The wooden bank is driven linearly, like the head. A shaper used to
        // sit here, labelled as soft odd-harmonic saturation in the zelkova: a
        // clamp after a pre-gain, then a cubic term, then a trim. None of it
        // did what it said. The drive the shell bank is handed never came near
        // the clamp and the cubic term sat far under the linear one, so what
        // was left was a fixed gain gated on Shell Resonance passing 1 %, which
        // put a step in the middle of a continuous control. A drum shell struck
        // by a stick is nowhere near its elastic limit, so there is nothing
        // here for a saturator to do. testShellResonanceHasNoStepInIt keeps it
        // that way.
        const float value = mode.resonator.tick (
            voice.modalInput[static_cast<std::size_t> (mode.physicalIndex)]);
        if (mode.membrane)
        {
            membraneLeft += value * mode.micLeft;
            membraneRight += value * mode.micRight;
        }
        else
        {
            left += value * mode.micLeft;
            right += value * mode.micRight;
        }
    }

    if (voice.physicalBank)
        voice.modalInput.fill (0.0f);

    // The impact heard straight through the air. Pressure follows the rate of
    // change of the contact force rather than the force itself, so a shorter,
    // harder contact makes a proportionally sharper and louder click - the
    // same v^(-1/5) law that brightens the head, arriving by a second route.
    if (! voice.physicalBank)
    {
        const float slope = (excitation - voice.directPrevious)
                          * static_cast<float> (sampleRate_) * 1.0e-5f;
        voice.directPrevious = excitation;
        voice.directLowpassState +=
            voice.directLowpassCoefficient * (slope - voice.directLowpassState);
        // The tack rattle joins the airborne path after the contact patch's
        // own low-pass rather than before it: that corner describes how large
        // the bachi's contact with the hide is, and a tack head is a great deal
        // smaller than that. It does take the same distances and delays, since
        // the tacks a stroke can rattle are the ones it landed among.
        const float radiated = voice.directLowpassState + tack;

        voice.directLine[static_cast<std::size_t> (voice.directWriteIndex)] = radiated;

        left += readDelayLine (voice.directLine, voice.directWriteIndex,
                               voice.directDelayLeft) * voice.directGainLeft;
        right += readDelayLine (voice.directLine, voice.directWriteIndex,
                                voice.directDelayRight) * voice.directGainRight;

        voice.directWriteIndex = (voice.directWriteIndex + 1) & (directLineSize - 1);
    }

    // Only the membrane is damped. The wooden shell is not what the hand is
    // resting on, the stick-on-stick stroke has nothing to do with the drum at
    // all, and the airborne click of the impact itself reaches the microphones
    // through the air rather than through the head.
    left += membraneLeft;
    right += membraneRight;

    // The forced fade at the tail cap. Zero step until the cap is in sight, so
    // an ordinary stroke - which retires because its modes ran out, not because
    // it hit the cap - never touches this.
    if (voice.retireStep > 0.0f)
    {
        voice.retireGain -= voice.retireStep;
        if (voice.retireGain < 0.0f)
            voice.retireGain = 0.0f;
        left *= voice.retireGain;
        right *= voice.retireGain;
    }

    ++voice.ageSamples;

    const float magnitude = std::max (std::abs (left), std::abs (right));
    if (magnitude > voice.peakLevel)
        voice.peakLevel = magnitude;
    else
        voice.peakLevel *= 0.99995f;

    rightOut = right;
    return left;
}

// ---------------------------------------------------------------------------
// Block processing
// ---------------------------------------------------------------------------

void TaikoEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    if (! prepared_)
    {
        std::fill (left, left + numSamples, 0.0f);
        std::fill (right, right + numSamples, 0.0f);
        return;
    }

    // Pitch bend invalidates the lazy geometry cache for the next strike, but
    // live banks already follow it through applyTensionShift. Re-solving four
    // drums here on every small host block is necessary only when a structural
    // control changed underneath an existing bank.
    if (! drumCacheValid_
        && std::any_of (physicalDrums_.begin(), physicalDrums_.end(),
                        [this] (const Voice& drum)
                        {
                            return drum.modeCount > 0
                                && drum.configurationRevision
                                       != physicalConfigurationRevision_;
                        }))
        refreshDrumIfNeeded();

    const float targetGain = applied_.outputGain;
    const float targetDrive = applied_.drive;
    const float targetWidth = applied_.stereoWidth;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        handDamping_ += handDampingCoefficient_ * (handDampingTarget_ - handDamping_);
        pitchBend_ += pitchBendCoefficient_ * (pitchBendTarget_ - pitchBend_);
        // A tenth of a cent, measured against where the cache actually stands.
        if (std::abs (pitchBend_ - drumCacheBend_) > 0.0005f)
            drumCacheValid_ = false;

        smoothedOutputGain_ += gainSmoothing_ * (targetGain - smoothedOutputGain_);
        smoothedDrive_ += gainSmoothing_ * (targetDrive - smoothedDrive_);
        // Width multiplies the side signal, so a step in it steps the audio.
        smoothedWidth_ += gainSmoothing_ * (targetWidth - smoothedWidth_);

        float mixLeft = 0.0f;
        float mixRight = 0.0f;
        bool anyVoiceActive = false;

        // Retune/control the canonical poles before forming the matched contact
        // compliance. Changing a1/a2 between the solve and the recurrence would
        // make the force step describe a different free system from the one
        // actually advanced below.
        for (auto& physical : physicalDrums_)
        {
            if (! physical.active)
                continue;
            if (physical.controlCountdown <= 0)
            {
                updateVoiceControl (physical);
                physical.controlCountdown = controlPeriod;
                if (physical.ageSamples >= physical.maximumSamples)
                {
                    silenceVoice (physical);
                    physical.physicalBank = true;
                    continue;
                }
            }
            --physical.controlCountdown;
        }

        for (auto& voice : voices_)
        {
            voice.solvedContactForce = 0.0f;
            voice.solvedContactEnergyStep = 0.0;
        }
        for (auto& physical : physicalDrums_)
            if (physical.active)
                advancePhysicalContacts (physical);

        // Gather every due contact before any physical recurrence advances.
        // Same-sample hits therefore see the same pre-step head and their
        // projected forces enter one canonical modal update.
        for (auto& voice : voices_)
        {
            if (! voice.active)
                continue;
            anyVoiceActive = true;

            if (voice.controlCountdown <= 0)
            {
                voice.controlCountdown = controlPeriod;
                if (voice.ageSamples >= voice.maximumSamples
                    && voice.contactRemaining == 0u
                    && ! voice.nonlinearContactActive
                    && voice.nextContact >= voice.contactCount)
                {
                    silenceVoice (voice);
                    continue;
                }
            }
            --voice.controlCountdown;

            // physicalDrumIndex was resolved once, at trigger(), from the
            // same already-clamped octave; re-clamping voice.octaveOffset
            // here every sample was pure overhead.
            float voiceRight = 0.0f;
            const float voiceLeft = renderVoice (
                voice, &physicalDrums_[voice.physicalDrumIndex], voiceRight);
            mixLeft += voiceLeft;
            mixRight += voiceRight;
        }

        // Now advance each sounding instrument once, regardless of how many
        // contacts fed it above. A dense roll costs one bank per drum, not one
        // bank per overlapping MIDI event.
        for (auto& physical : physicalDrums_)
        {
            if (! physical.active)
                continue;
            anyVoiceActive = true;

            float drumRight = 0.0f;
            const float drumLeft = renderVoice (physical, nullptr, drumRight);
            mixLeft += drumLeft;
            mixRight += drumRight;
        }

        if (idleFrozen_ && ! anyVoiceActive)
        {
            left[sample] = 0.0f;
            right[sample] = 0.0f;
            meterLeft_ *= meterReleaseMultiplier_;
            meterRight_ *= meterReleaseMultiplier_;
            visualLevel_ *= visualDecayMultiplier_;
            continue;
        }

        // Width trim on the finished pair. At 0 the two close microphones are
        // summed to mono, at 0.5 they are left exactly as the head presented
        // them - already a real stereo image rather than a widened one - and
        // above that the difference is exaggerated.
        const float mid = 0.5f * (mixLeft + mixRight);
        const float side = 0.5f * (mixLeft - mixRight) * (2.0f * smoothedWidth_);
        float outLeft = mid + side;
        float outRight = mid - side;

        // Remove the offset a one-sided strike leaves behind before anything
        // nonlinear can turn it into a thump.
        const float dcInLeft = outLeft;
        const float dcInRight = outRight;
        dcOutputLeft_ = dcInLeft - dcInputLeft_ + dcCoefficient_ * dcOutputLeft_;
        dcOutputRight_ = dcInRight - dcInputRight_ + dcCoefficient_ * dcOutputRight_;
        dcInputLeft_ = dcInLeft;
        dcInputRight_ = dcInRight;
        outLeft = dcOutputLeft_;
        outRight = dcOutputRight_;

        if (smoothedDrive_ > 1.0e-4f)
        {
            const float amount = 1.0f + 7.0f * smoothedDrive_;
            const float makeup = 1.0f / std::sqrt (amount);

            // Antiderivative antialiasing, so pushing the drive does not fold
            // the drum's own high partials back down into its fundamental.
            const auto shape = [] (float x, float& previous) noexcept
            {
                const float current = x;
                const float delta = current - previous;
                float result;
                if (std::abs (delta) > 1.0e-5f)
                {
                    const auto antiderivative = [] (float value) noexcept
                    {
                        const float absolute = std::abs (value);
                        return absolute + std::log1p (std::exp (-2.0f * absolute))
                             - 0.6931472f;
                    };
                    result = (antiderivative (current) - antiderivative (previous)) / delta;
                }
                else
                {
                    result = std::tanh (0.5f * (current + previous));
                }
                previous = current;
                return result;
            };

            // Blended against the dry signal rather than switched into, so the
            // control converges on bypass as it approaches zero. Driving into
            // tanh alone jumps to a fully shaped signal at the first nonzero
            // step, because the shaper's gain approaches one rather than the
            // identity.
            const float wetLeft = shape (outLeft * amount, driveAdaaLeft_) * makeup;
            const float wetRight = shape (outRight * amount, driveAdaaRight_) * makeup;
            outLeft += smoothedDrive_ * (wetLeft - outLeft);
            outRight += smoothedDrive_ * (wetRight - outRight);
        }
        else
        {
            driveAdaaLeft_ = outLeft;
            driveAdaaRight_ = outRight;
        }

        outLeft *= smoothedOutputGain_;
        outRight *= smoothedOutputGain_;

        // Final safety. The model is bounded by its own damping, but a fully
        // wound drive on sixteen simultaneous odaiko strokes should still
        // never leave the buffer.
        outLeft = clampFloat (outLeft, -1.0f, 1.0f);
        outRight = clampFloat (outRight, -1.0f, 1.0f);

        left[sample] = outLeft;
        right[sample] = outRight;

        // Freeze once nothing is sounding and the shared path has rung out.
        if (! anyVoiceActive
            && std::abs (dcOutputLeft_) < idleFreezeLevel
            && std::abs (dcOutputRight_) < idleFreezeLevel
            && std::abs (outLeft) < idleFreezeLevel
            && std::abs (outRight) < idleFreezeLevel)
        {
            if (++silentSamples_ >= idleFreezeSamples)
            {
                dcInputLeft_ = dcInputRight_ = 0.0f;
                dcOutputLeft_ = dcOutputRight_ = 0.0f;
                driveAdaaLeft_ = driveAdaaRight_ = 0.0f;
                idleFrozen_ = true;
            }
        }
        else
        {
            silentSamples_ = 0;
        }

        const float absoluteLeft = std::abs (outLeft);
        const float absoluteRight = std::abs (outRight);
        meterLeft_ = absoluteLeft > meterLeft_
            ? absoluteLeft
            : meterLeft_ * meterReleaseMultiplier_;
        meterRight_ = absoluteRight > meterRight_
            ? absoluteRight
            : meterRight_ * meterReleaseMultiplier_;
        visualLevel_ *= visualDecayMultiplier_;
    }

    updateActiveVoiceCount();
    outputLevelLeft_.store (meterLeft_, std::memory_order_relaxed);
    outputLevelRight_.store (meterRight_, std::memory_order_relaxed);
    visualStrikeLevel_.store (visualLevel_, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Readouts
// ---------------------------------------------------------------------------

int TaikoEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount_.load (std::memory_order_relaxed);
}

float TaikoEngine::getOutputLevel (int channel) const noexcept
{
    return channel <= 0 ? outputLevelLeft_.load (std::memory_order_relaxed)
                        : outputLevelRight_.load (std::memory_order_relaxed);
}

float TaikoEngine::getFundamentalHz() const noexcept
{
    return fundamentalHz_.load (std::memory_order_relaxed);
}

void TaikoEngine::getVisualState (DrumVisualState& destination) const noexcept
{
    destination.strikeRadius = visualStrikeRadius_.load (std::memory_order_relaxed);
    destination.strikeAngle = visualStrikeAngle_.load (std::memory_order_relaxed);
    destination.strikeLevel = visualStrikeLevel_.load (std::memory_order_relaxed);
    destination.fundamentalHz = fundamentalHz_.load (std::memory_order_relaxed);
    const int articulation = visualArticulation_.load (std::memory_order_relaxed);
    destination.lastArticulation = static_cast<Articulation> (
        articulation >= 0 && articulation < static_cast<int> (articulationCount)
            ? articulation : 0);
    destination.lastOctaveOffset = visualOctave_.load (std::memory_order_relaxed);
    destination.activeVoices = activeVoiceCount_.load (std::memory_order_relaxed);
}

TaikoEngine::DrumMeasurements TaikoEngine::measureDrum (int octaveOffset) const noexcept
{
    // The engine's own prepared rate, because one of the figures - which
    // partial the drum is heard at - is a question about what this instance
    // will actually render.
    return measure (applied_, octaveOffset, 2.0f * pitchBend_, sampleRate_);
}

TaikoEngine::SoundingMode TaikoEngine::dynamicSoundingMode (
    const EngineParameters& rawParameters, int octaveOffset,
    float pitchBendSemitones, double sampleRateHz) noexcept
{
    auto parameters = sanitise (rawParameters);
    // The panel names the settled drum under a neutral open stroke. Noise has
    // no stable pitch, humanising deliberately moves the contact, and the
    // nonlinear tension glide shifts every mode by the same ratio rather than
    // changing which one wins.
    parameters.humanise = 0.0f;
    parameters.strikeNoise = 0.0f;
    parameters.tensionModulation = 0.0f;
    parameters.drive = 0.0f;
    parameters.outputGain = 0.0f;

    struct ReadoutCache
    {
        EngineParameters parameters {};
        int octave { 0 };
        float pitchBend { 0.0f };
        double sampleRate { 0.0 };
        SoundingMode result {};
        bool valid { false };
    };
    static thread_local ReadoutCache cache;
    const int octave = std::clamp (octaveOffset, lowestOctaveOffset,
                                   highestOctaveOffset);
    if (cache.valid && cache.parameters == parameters && cache.octave == octave
        && cache.pitchBend == pitchBendSemitones
        && cache.sampleRate == sampleRateHz)
        return cache.result;

    const auto cacheResult = [&] (SoundingMode result) noexcept
    {
        cache.parameters = parameters;
        cache.octave = octave;
        cache.pitchBend = pitchBendSemitones;
        cache.sampleRate = sampleRateHz;
        cache.result = result;
        cache.valid = true;
        return result;
    };

    // Reusing one scratch engine per caller avoids both an 800 kB stack object
    // and allocation in this noexcept readout. trigger() deliberately uses the
    // cheap analytic visual estimate above, so this cannot recurse.
    static thread_local TaikoEngine audit;
    audit.setParameters (parameters);
    audit.setPitchBend (0.5f * pitchBendSemitones);
    audit.prepare (sampleRateHz, 1);

    constexpr float neutralVelocity = 0.72f;
    audit.trigger (Articulation::Don, octave, neutralVelocity);

    auto& contact = audit.voices_[0];
    const auto drumIndex = static_cast<std::size_t> (octave - lowestOctaveOffset);
    auto& physical = audit.physicalDrums_[drumIndex];
    if (! contact.active || physical.modeCount <= 0)
        return cacheResult ({});

    // Only the normal force is being audited. These paths do not feed back into
    // the solve, so silencing them saves work and prevents a future observation
    // change from affecting the physical ranking.
    contact.continuumInjection.fill (0.0f);
    contact.contactNoiseAmplitude = 0.0f;
    contact.tackScale = 0.0f;
    contact.directGainLeft = 0.0f;
    contact.directGainRight = 0.0f;

    bool receivedForce = false;
    int elapsedSamples = 0;
    const int maximumSamples = static_cast<int> (
        std::ceil (0.05 * audit.sampleRate_)) + 2;
    for (int sample = 0; sample < maximumSamples; ++sample)
    {
        float left = 0.0f;
        float right = 0.0f;
        audit.process (&left, &right, 1);
        const double force = contact.solvedContactForce;
        receivedForce = receivedForce || force > 0.0;
        ++elapsedSamples;

        if (receivedForce && ! contact.nonlinearContactActive)
            break;
    }

    SoundingMode best;
    double bestWeight = 0.0;
    const double releaseSeconds = static_cast<double> (elapsedSamples)
                                / audit.sampleRate_;
    const double windowStart = std::max (
        static_cast<double> (pitchWindowStart) - releaseSeconds, 0.0);
    const double windowEnd = std::max (
        static_cast<double> (pitchWindowEnd) - releaseSeconds, windowStart);
    for (int index = 0; index < physical.modeCount; ++index)
    {
        const auto& mode = physical.modes[static_cast<std::size_t> (index)];
        if (! mode.membrane || ! (mode.omega > 0.0f))
            continue;

        // The exact-pole contact recurrence has already integrated the force
        // into this modal state. Reading its quadrature is both more exact than
        // reconstructing a continuous force transfer (especially near Nyquist)
        // and avoids one complex accumulator per mode per contact sample.
        const double radius = std::sqrt (mode.resonator.a2);
        const double sine = mode.resonator.b0;
        if (! (radius > 0.0) || std::abs (sine) < 1.0e-14)
            continue;
        const double cosine = -mode.resonator.a1 / (2.0 * radius);
        const double quadrature =
            (mode.resonator.y1 * cosine - radius * mode.resonator.y2) / sine;
        const double amplitude = std::hypot (mode.resonator.y1, quadrature);
        const double decay = mode.decayRate;
        const double window = decay > 0.0
            ? (std::exp (-decay * windowStart)
               - std::exp (-decay * windowEnd)) / decay
            : 0.0;
        const double ownGain = 0.5 + static_cast<double> (parameters.stereoWidth);
        const double otherGain = 0.5 - static_cast<double> (parameters.stereoWidth);
        const double observed = ownGain * static_cast<double> (mode.micLeft)
                              + otherGain * static_cast<double> (mode.micRight);
        const double weight = amplitude * std::abs (observed)
                            * window;
        if (! (weight > bestWeight))
            continue;

        bestWeight = weight;
        best.frequencyHz = mode.omega / (2.0f * piFloat);
        best.weight = static_cast<float> (weight);
        best.identity.entryIndex = mode.modeEntry;
        best.identity.branch = static_cast<std::uint8_t> (
            static_cast<int> (mode.physicalIndex) - 2 * mode.modeEntry);
    }

    if (bestWeight > 0.0)
        return cacheResult (best);

    const auto drum = resolveDrumFor (parameters, pitchBendSemitones, octave);
    return cacheResult (soundingMode (drum, readoutStrikeRadius (parameters),
                                     renderedModeCeilingHz (sampleRateHz)));
}

TaikoEngine::DrumMeasurements TaikoEngine::measure (const EngineParameters& parameters,
                                                     int octaveOffset,
                                                     float pitchBendSemitones,
                                                     double sampleRateHz) noexcept
{
    const auto applied = sanitise (parameters);
    const auto drum = resolveDrumFor (applied, pitchBendSemitones, octaveOffset);
    const auto& entry = membraneModes()[0]; // the (0,1) mode
    const auto lambda = static_cast<float> (entry.besselZero);

    DrumMeasurements result;
    result.radiusMetres = drum.radius;
    result.depthMetres = drum.depth;
    result.tensionNewtonsPerMetre = drum.tension;
    result.arealDensityKgPerSquareMetre = drum.batterDensity;
    result.waveSpeedMetresPerSecond = drum.waveSpeed;
    result.headStiffnessParameter = drum.stiffnessBatter;
    result.cavityStiffnessFactor = drum.cavityColumnFactor;
    // No stiffness stretch on either of these: the stretch is taken relative to
    // the (0,1) mode and this is the (0,1) mode, so it is unity by
    // construction. That is the whole point of normalising it there - the pitch
    // the drum is tuned to is a membrane frequency however stiff the head is,
    // and only the modes above it move.
    result.idealFundamentalHz =
        drum.waveSpeed * lambda / (2.0f * piFloat * drum.radius);

    // Both branches, solved through the same routine the render path and the
    // octave transform use, so the readout, the audio and the keyboard cannot
    // disagree about the drum - including at zero coupling, where the pair is
    // degenerate. The transform reads the same drum through the same
    // observeMode this readout does - it differs only in tracking a latched mode
    // at the centred stroke rather than the loudest one at the player's - so the
    // keyboard's octave and the panel's figures stay one claim about one drum
    // rather than two that have to be kept in step by hand.
    const auto pair = solveAxisymmetricPair (drum);
    // The tail sweep below solves every mode's own branches, including this
    // one's, so it needs the threshold rather than the pair's own verdict.
    constexpr float audibleShare = 0.01f;

    result.breathingModeHz = pair.breathingHz;
    result.loadedFundamentalHz = pair.loadedFundamentalHz;
    // What the panel should say the drum is at: the mode it is actually heard
    // at rather than the lowest one it has, and heard under the stroke the
    // controls currently describe rather than under a centred one.
    //
    // Strike Position is the reason those are two different questions. An
    // off-centre stroke drives a different balance of modes - it is why a Ka out
    // by the tacks is thin and cutting - and on the two large drums of this
    // family it moves which mode wins by a fourth and more. The readout has to
    // follow that, or the number on the panel is the pitch of a stroke nobody
    // played. The octave transform deliberately does not: it is anchored at the
    // centred stroke inside resolveDrumFor, so Strike Position stays a timbre
    // control and cannot retune the keyboard. See tuningStrikeRadius.
    //
    // Bounded to the modes this sample rate will actually put a resonator on.
    // The drum has whatever modes it has, but the render refuses every one at
    // or above 0.98 of Nyquist, and on a very small head at the tension ceiling
    // that can be all of them - at which point this is zero, which is the
    // marker for a drum with no membrane tone rather than a frequency. See
    // soundingMode and DrumMeasurements::soundingHz.
    result.soundingHz = dynamicSoundingMode (
        applied, octaveOffset, pitchBendSemitones, sampleRateHz).frequencyHz;

    // How long a branch rings, with its own radiation share. Two branches of the
    // same mode differ a great deal on a sealed drum, because only the one that
    // changes the body's volume radiates - so reporting one branch's decay
    // beside the other branch's frequency described neither.
    //
    // The two density square roots are a property of the drum, not of the
    // branch, so they are resolved once here rather than inside the lambda -
    // which the tail sweep below calls up to twice per axisymmetric mode entry.
    const float sqrtBatterDensity = std::sqrt (drum.batterDensity);
    const float sqrtResonantDensity = std::sqrt (drum.resonantDensity);
    const auto branchTail = [&drum, sqrtBatterDensity, sqrtResonantDensity] (
                                 float branchLambda, float omega,
                                 float vectorB, float vectorR)
    {
        if (! (omega > 0.0f))
            return 0.0f;

        const float volumeShare = vectorB / sqrtBatterDensity
                                + vectorR / sqrtResonantDensity;
        const float efficiency =
            radiationEfficiency (0, omega * drum.radius / soundSpeed);
        // The same net-volume weighting the sounded modes carry, so the
        // readout describes the drum the listener actually hears.
        const float volumeCoupling = (2.0f / branchLambda) * volumeShare;
        const float decay = materialDamping (drum, omega, 0.0f)
                          + drum.radiationScale * airDensity * soundSpeed * efficiency
                                * volumeCoupling * volumeCoupling
                          + drum.edgeLoss
                          + mountingLoss (drum, omega / (2.0f * piFloat));
        return decay > 0.0f ? 6.9078f / decay : 0.0f;
    };

    // What the editor labels a tail is how long the drum rings, and that is not
    // a question about the fundamental. The fundamental is the one mode of a
    // head that moves enough air to radiate properly, which makes it the one
    // that empties first; the radial orders above it push their air in and out
    // of their own nodal rings and can barely get a sound out at all, so they
    // outlast it several times over. Reporting the fundamental's decay as the
    // drum's tail understated a sealed drum by a factor of four.
    //
    // So the whole bank is swept and the longest-lived mode any stroke can
    // drive is the answer - including the modes with a circumferential order.
    // Those used to be left out on the grounds that they are silent under the
    // centre of the head, which was true when a Don was struck there and is not
    // now that no stroke is: they are driven by every articulation the
    // instrument has, they radiate as multipoles and so barely at all, and on a
    // lightly damped drum with a dense shell they outlast the axisymmetric
    // family that used to be the whole of this figure.
    result.tailSeconds = 0.0f;

    for (const auto& radial : membraneModes())
    {
        const auto radialLambda = static_cast<float> (radial.besselZero);

        if (radial.circumferentialOrder != 0)
        {
            // No cavity to couple to: a mode with a circumferential order moves
            // the same air in and out of the body and leaves its volume alone,
            // so there is no eigenproblem here and no pair of branches - just
            // the batter head's own mode, air-loaded and damped.
            const auto order = static_cast<float> (radial.circumferentialOrder);
            const float ideal =
                drum.waveSpeed * radialLambda / (2.0f * piFloat * drum.radius)
                * stiffnessStretch (radialLambda, drum.stiffnessBatter);
            const float shape =
                (2.4048f / radialLambda) / (1.0f + 0.6f * order);
            const float load = 1.0f / std::sqrt (
                1.0f + 0.85f * shape * airDensity * drum.radius / drum.batterDensity);

            const float omega = 2.0f * piFloat * ideal * load;
            const float frequency = omega / (2.0f * piFloat);
            if (! (frequency > 0.0f) || frequency >= 20000.0f)
                continue;

            const float efficiency = radiationEfficiency (
                radial.circumferentialOrder, omega * drum.radius / soundSpeed);
            const float decay =
                materialDamping (drum, omega, 0.0f)
                + drum.radiationScale * airDensity * soundSpeed * efficiency
                      / (2.0f * drum.batterDensity)
                + drum.edgeLoss * (1.0f + edgeOrderFactor * order)
                + mountingLoss (drum, frequency);

            if (decay > 0.0f)
                result.tailSeconds = std::max (result.tailSeconds, 6.9078f / decay);

            continue;
        }

        const float radialBatter =
            drum.waveSpeed * radialLambda / (2.0f * piFloat * drum.radius)
            * stiffnessStretch (radialLambda, drum.stiffnessBatter);
        const float radialResonant =
            drum.resonantWaveSpeed * radialLambda / (2.0f * piFloat * drum.radius)
            * stiffnessStretch (radialLambda, drum.stiffnessResonant);

        // The air load falls off with the mode's own order exactly as it does
        // where the modes are built: a mode with nodal rings shifts far less
        // air per unit of displacement than the fundamental does.
        const float radialShape = 2.4048f / radialLambda;
        const float radialLoadB = 1.0f / std::sqrt (
            1.0f + 0.85f * radialShape * airDensity * drum.radius / drum.batterDensity);
        const float radialLoadR = 1.0f / std::sqrt (
            1.0f + 0.85f * radialShape * airDensity * drum.radius / drum.resonantDensity);

        const float radialOmegaB = 2.0f * piFloat * radialBatter * radialLoadB;
        const float radialOmegaR = 2.0f * piFloat * radialResonant * radialLoadR;

        const float radialCavity =
            drum.cavityStiffness * 4.0f / (radialLambda * radialLambda);
        const float radialDiagB =
            radialOmegaB * radialOmegaB + radialCavity / drum.batterDensity;
        const float radialDiagR =
            radialOmegaR * radialOmegaR + radialCavity / drum.resonantDensity;
        const float radialOff =
            radialCavity / std::sqrt (drum.batterDensity * drum.resonantDensity);

        for (int branch = 0; branch < 2; ++branch)
        {
            float eigenvalue = 0.0f;
            float vectorB = 0.0f;
            float vectorR = 0.0f;
            solveAxisymmetricBranch (radialDiagB, radialDiagR, radialOff, branch,
                                     eigenvalue, vectorB, vectorR);
            if (! (eigenvalue > 0.0f))
                continue;

            const float omega = std::sqrt (eigenvalue);
            if (omega / (2.0f * piFloat) >= 20000.0f)
                continue;

            // Same restriction as the frequencies above: a branch the batter
            // head does not move cannot be how long the drum rings, however
            // long it would ring if something did drive it.
            if (std::abs (vectorB) <= audibleShare)
                continue;

            result.tailSeconds = std::max (
                result.tailSeconds,
                branchTail (radialLambda, omega, vectorB, vectorR));
        }
    }

    return result;
}

} // namespace taikor
