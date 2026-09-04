#include "YouKnow106Presets.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace youknow106::presets
{
namespace
{
// The original instrument stores names nowhere; these are the position-ordered
// descriptive names carried by the public Juno-106 Librarian factory archive.
// The complete name sequence and tone corpus independently match KR-106's
// decoded factory dataset at commit bc15caee5843ab238a25d0969e68d57db2b1615f.
constexpr std::array<const char*, presetCount> factoryNames {{
    "Brass Set 1",
    "Brass Swell",
    "Trumpet",
    "Flutes",
    "Moving Strings",
    "Brass & Strings",
    "Choir",
    "Piano I",
    "Organ I",
    "Organ II",
    "Combo Organ",
    "Calliope",
    "Donald Pluck",
    "Celeste* (1 oct.up)",
    "Elect. Piano I",
    "Elect. Piano II",
    "Clock Chimes* (1 oct. up)",
    "Steel Drums",
    "Xylophone",
    "Brass III",
    "Fanfare",
    "String III",
    "Pizzicato",
    "High Strings",
    "Bass clarinet",
    "English Horn",
    "Brass Ensemble",
    "Guitar",
    "Koto",
    "Dark Pluck",
    "Funky I",
    "Synth Bass I (unison)",
    "Lead I",
    "Lead II",
    "Lead III",
    "Funky II",
    "Synth Bass II",
    "Funky III",
    "Thud Wah",
    "Going Up",
    "Piano II",
    "Clav",
    "Frontier Organ",
    "Snare Drum (unison)",
    "Tom Toms (unison)",
    "Timpani (unison)",
    "Shaker",
    "Synth Pad",
    "Sweep I",
    "Pluck Sweep",
    "Repeater",
    "Sweep II",
    "Pluck Bell",
    "Dark Synth Piano",
    "Sustainer",
    "Wah Release",
    "Gong (play low chords)",
    "Resonance Funk",
    "Drum Booms* (1 oct. down)",
    "Dust Storm",
    "Rocket Men",
    "Hand Claps",
    "FX Sweep",
    "Caverns",
    "Strings",
    "Violin",
    "Chorus Vibes",
    "Organ 1",
    "Harpsichord 1",
    "Recorder",
    "Perc. Pluck",
    "Noise Sweep",
    "Space Chimes",
    "Nylon Guitar",
    "Orchestral Pad",
    "Bright Pluck",
    "Organ Bell",
    "Accordion",
    "FX Rise 1",
    "FX Rise 2",
    "Brass",
    "Helicopter",
    "Lute",
    "Chorus Funk",
    "Tomita",
    "FX Sweep 1",
    "Sharp Reed",
    "Bass Pluck",
    "Resonant Rise",
    "Harpsichord 2",
    "Dark Ensemble",
    "Contact Wah",
    "Noise Sweep 2",
    "Glassy Wah",
    "Phase Ensemble",
    "Chorused Bell",
    "Clav",
    "Organ 2",
    "Bassoon",
    "Auto Release Noise Sweep",
    "Brass Ensemble",
    "Ethereal",
    "Chorus Bell 2",
    "Blizzard",
    "E. Piano with Tremolo",
    "Clarinet",
    "Thunder",
    "Reedy Organ",
    "Flute / Horn",
    "Toy Rhodes",
    "Surf's Up",
    "OW Bass",
    "Piccolo",
    "Melodic Taps",
    "Meow Brass",
    "Violin (high)",
    "High Bells",
    "Rolling Wah",
    "Ping Bell",
    "Brassy Organ",
    "Low Dark Strings",
    "Piccolo Trumpet",
    "Cello",
    "High Strings",
    "Rocket Men",
    "Forbidden Planet",
    "Froggy",
    "Owgan",
}};

// A11..A88 followed by B11..B88, exactly eighteen 7-bit bytes per tone.
// SHA-256 of the decoded 2,304-byte corpus:
// 394ae874da33aa63fa4833932fbf415546d2ad66b1b6b9a36315601799eeec21
constexpr std::string_view factoryToneHex =
    "1431006600230d3a00566c03312d20005111"
    "06300038002b111a00544b40762625465219"
    "342d086600372218013b7f05423010003209"
    "3c2b01000037200a0b297f17510012003201"
    "3f000027004d1404006f220d5758230e1a10"
    "23000038004c110400294e2c42352c174918"
    "3b0e0d19003b5e02003e7f440b7f30004a09"
    "1431005000410c0a001b670042001e562a11"
    "360f0035002b4c0e017f64000a520017291c"
    "2c0f003500354c0e01554a000a52003a4a1c"
    "4b150939003f4604006d6000302b2e3d2c0d"
    "52280b0000571b11003859077f7f06304a0b"
    "4c1509390049690f004e5202052b0a7f2c07"
    "1c00000000211836002660002c00510f2c19"
    "3b00000000101f3d06237f01552b28002901"
    "000000470032450700506600440016004911"
    "3b000000162c7f00007f68003000337f441b"
    "2135002009472e1a007f7f001a0025214a1b"
    "00000000001d183600337f001d1d260f2c19"
    "341400230042180b000c7f3a645e25165219"
    "2f000046002c002000432148684b31325918"
    "301b006600470e000054493f1f7f2d001a10"
    "3c12006600420205002a7f000b000c005a00"
    "3a0e00660054080200474d122c7f28000c08"
    "342d080000302419083a680b4b0019002911"
    "2f2d096600463007001b7f08511a10002a01"
    "342d0066002e2c1d013b67106761221f5211"
    "563a0041000c0f47001e600034361f003911"
    "5a1c005e00283a1d004b7f00380027024a01"
    "1f00005700263d1b00475a00340f3f610a19"
    "04000049001f1733002947001e240240591d"
    "3a1001393220004200234b00220024662919"
    "4858120000286124003463002d4900002a1d"
    "18580035002d002b00343801175b4b4f3914"
    "56471566003c1f12014c7f0042300b003215"
    "0400003000051751002964001e2702394a15"
    "5a150000002b002f00004e002509162d311c"
    "040000096526362d000a4f001800027f391d"
    "3e000066084e592200006a0000662d201a1a"
    "000000641c4f7f2500414b006c127f002213"
    "01000048002f002700054e00620020732a10"
    "420f016600070b560200780027300e004915"
    "4f0f03550045360000347f00007f001d2a1c"
    "341b00005b5e0b110000650019001e402211"
    "3e1008007f350428001565001e0f283a2118"
    "000000237f19003600187f01381a472f2111"
    "1547003a7f592d0111507f0a090000002201"
    "3800052b00250054007f3300554b3e250a18"
    "4e0000661c732843000061003f7f522a0c12"
    "44300044003c6f0906622d005b004d7d1a08"
    "4d14096500611e380039570e0029002c4a17"
    "58480f61004266440046650059354e304a19"
    "3300051100270031007f3100554b35310c18"
    "7f0000000038560600474f002f00327f1a1a"
    "4700052e00270054007f3900554b46312a18"
    "590e0119004858130042680054001d570a13"
    "7f00006638466b0500485b03307f62004913"
    "3b000000001a7f2d00597f00130016002111"
    "000000667f2a0022003a7f00240f312e2119"
    "000000007f3455002c5e7f585b1c55002111"
    "082000666149720800387f00597f68002403"
    "3b0000007f11583600377f010b0008002105"
    "7f4174667f472c53005e7f005e00707f2912"
    "000000667f44760000456b000d262f002108"
    "392d003700550000006c343b205628001a18"
    "422d1400004d000000786e2b2d391a003218"
    "482d0000000a003b00175800592f4a004a19"
    "2d2a0049003c480e007f3a00000000610a1d"
    "17000036007f7f5a00567d003c001f232c00"
    "4e0000080006002e007f7f05157f1e002a11"
    "3e2608000000005a00494100104e74004a19"
    "7f0000007f02004d00685c0e4e6c78002118"
    "6300000000467f00004a550019003c004c18"
    "482d001d0039001a00197300590020002a01"
    "162d0069002100370024001d5832347f1a18"
    "0000003a0039002800566800192a2c002a11"
    "4e000644003d0000007f3e00007f1a555a19"
    "00000040004d000e004a42080b6e09505a01"
    "74000000006c7f107f400000007f5400441a"
    "6c000000005e7f50177f00007f334700021a"
    "337f00490000005e007f48032c330b00521c"
    "6a0000300052055d7f00480000234c7f0a1b"
    "34000269001d002300567f00302257002a19"
    "4d5e0049002f223500412a000b22004f5a1d"
    "4e00006900317d0f027f39000e4f0000241f"
    "7f000000003a7f104c400000007f55004418"
    "150000490021003500413702127000003c1c"
    "0000003c00251d1c002b5c002a2300003918"
    "34000f0000426b1f00493e0034270000111e"
    "00000069007f5f00007f6b002d0000233c11"
    "0000003900370000006b4315007f234b5a1b"
    "4e000051003d652e007f7f00000000002a17"
    "7f0000007f7f00530068747f4f1a5300441a"
    "005e001c0018223d00417006123226002a19"
    "0b2d0569004f0000007f63542d3931000a18"
    "000b0069003b7f00007f3e003e0039654c18"
    "320b005b0020003f00006c002b3700072909"
    "5100006800366a07007f4f0000000000511d"
    "5426095a0023002b09627f057f6a02002901"
    "7f0000007f7f007f00686b004f395300441a"
    "195e0049002f222300412706444326005a18"
    "45000069002f7800007f3c5c327f373b1a18"
    "482d001d0000663e005a1e00750078005a19"
    "010000697f38590726797f4e62574e002400"
    "2c00001500160023076b6700413c627f2c10"
    "451609000023002b076268057f6a07002a01"
    "7f0000007f02004d00686b00166c78002118"
    "2312004900210035004145032c6d00002a1c"
    "2200003b0019002a00411615494730001918"
    "1e2d7f00003a7f00007f6b00590027002400"
    "010000697f540b0031364b21617f7900240a"
    "32000032003f5441007f467f006b002f391f"
    "461c000e0004003f10625710676a15003c09"
    "630000007f487000007f7f00180018004100"
    "337f0049002d6423004154045a001b00321c"
    "472d1600005900000378552b2d391a003418"
    "1e2d7f00004c7f00007f6f0016343b002400"
    "24000069003c09007f004d22377f69002a18"
    "000b0068004c7f00007f5300171739004c18"
    "100000440008005700340e00246d00385918"
    "152d05510047000000672853176d2a001918"
    "337f00490000005e007f61032c330b00321c"
    "392d1500004b0003012d6d30415a22003118"
    "50000a46005c00000047301b393929001c18"
    "6c000000006e7f503f7f00007f335900021a"
    "320b002c001d0458055f4f0030172a094c01"
    "4e00000000377f2b007f4d000000007f2917"
    "3200002d00265420007f6500313700383919";

static_assert(factoryToneHex.size()
              == static_cast<std::size_t>(presetCount * sysex::toneByteCount * 2));

constexpr std::uint8_t hexNibble(char value) noexcept
{
    return value >= '0' && value <= '9'
        ? static_cast<std::uint8_t>(value - '0')
        : static_cast<std::uint8_t>(value - 'a' + 10);
}

constexpr bool factoryToneHexIsValid() noexcept
{
    for (std::size_t index = 0; index < factoryToneHex.size(); ++index)
    {
        const char value = factoryToneHex[index];
        if (!((value >= '0' && value <= '9')
              || (value >= 'a' && value <= 'f')))
            return false;
    }

    for (std::size_t index = 0; index < factoryToneHex.size(); index += 2)
    {
        const auto byte = static_cast<std::uint8_t>(
            (hexNibble(factoryToneHex[index]) << 4u)
            | hexNibble(factoryToneHex[index + 1]));
        if (byte > static_cast<std::uint8_t>(0x7f))
            return false;
    }

    return true;
}

static_assert(factoryToneHexIsValid());

constexpr std::uint64_t factoryCorpusFnv1a() noexcept
{
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (std::size_t index = 0; index < factoryToneHex.size(); index += 2)
    {
        const auto byte = static_cast<std::uint8_t>(
            (hexNibble(factoryToneHex[index]) << 4u)
            | hexNibble(factoryToneHex[index + 1]));
        hash ^= byte;
        hash *= 0x100000001b3ull;
    }
    return hash;
}

static_assert(factoryCorpusFnv1a() == 0xa78dab9d5bafb386ull);

// Per-preset output-volume trim, in panel position, one entry per bank slot.
//
// The eighteen tone bytes are the hardware's and are never touched; VR1's shaft
// position is not part of them, and on the instrument it is exactly what a
// player moves when one patch arrives hotter than the last. These trims are
// that shaft position, chosen so the product bank behaves like a bank:
//
//   * No factory preset may exceed -1 dBFS sample peak. This is no longer the
//     binding rule: once digital full scale was referred to the output summer's
//     modelled asymptote, the untrimmed bank peaked no higher than -3.67 dBFS.
//     It stays as a floor under the contract rather than as the thing the
//     trims are chosen to satisfy.
//   * No factory preset may exceed -31 dBFS gated loudness. Derived once, as
//     three decibels above the -34.20 dBFS corpus median measured with every
//     preset at the panel default, then frozen as an absolute figure. It is
//     deliberately not re-expressed against the median: pulling the loud end
//     down moves the median too, so a relative rule would chase itself and
//     could never be satisfied. Tools/AuditFactoryPresets enforces both as
//     absolutes.
//
// Only attenuation is applied: no entry exceeds the 0.80 panel default, so a
// preset is never made louder than the instrument's own nominal setting. The
// intrinsically quiet percussion and noise-sweep patches keep their level --
// VR1 has only about +2.25 dB of travel left above the default, which cannot
// close the remaining gap, and their quietness is the hardware's own. Far less
// of that gap is the model's doing than it once was: restoring the shared noise
// rail to its service anchor lifted the twelve noise-only patches from 21.5 dB
// below the corpus median to 12.9 dB below it.
constexpr std::array<float, presetCount> factoryVolume {{
    0.800f, 0.591f, 0.800f, 0.800f, 0.800f, 0.580f, 0.383f, 0.800f,
    0.462f, 0.557f, 0.800f, 0.769f, 0.800f, 0.800f, 0.800f, 0.800f,
    0.800f, 0.744f, 0.800f, 0.394f, 0.596f, 0.617f, 0.800f, 0.775f,
    0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.354f, 0.254f,
    0.645f, 0.800f, 0.800f, 0.800f, 0.790f, 0.337f, 0.322f, 0.800f,
    0.800f, 0.760f, 0.321f, 0.800f, 0.611f, 0.458f, 0.800f, 0.800f,
    0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.530f, 0.800f, 0.650f,
    0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.800f,
    0.640f, 0.800f, 0.591f, 0.653f, 0.800f, 0.800f, 0.800f, 0.800f,
    0.800f, 0.800f, 0.800f, 0.800f, 0.450f, 0.800f, 0.800f, 0.800f,
    0.800f, 0.800f, 0.800f, 0.754f, 0.800f, 0.800f, 0.766f, 0.800f,
    0.800f, 0.800f, 0.523f, 0.356f, 0.800f, 0.800f, 0.534f, 0.800f,
    0.800f, 0.760f, 0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.800f,
    0.800f, 0.800f, 0.800f, 0.727f, 0.800f, 0.800f, 0.800f, 0.617f,
    0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.800f, 0.750f,
    0.539f, 0.800f, 0.613f, 0.800f, 0.800f, 0.800f, 0.800f, 0.611f,
}};

struct BankStorage
{
    std::array<std::array<char, 4>, presetCount> numbers {};
    std::array<Preset, presetCount> presets {};

    BankStorage() noexcept
    {
        for (std::size_t index = 0; index < presets.size(); ++index)
        {
            const int hardwareBank = static_cast<int>(index / 64);
            const int withinBank = static_cast<int>(index % 64);
            numbers[index] = {
                static_cast<char>('A' + hardwareBank),
                static_cast<char>('1' + withinBank / 8),
                static_cast<char>('1' + withinBank % 8),
                '\0'
            };

            std::array<std::uint8_t, sysex::toneByteCount> bytes {};
            const std::size_t firstHex =
                index * static_cast<std::size_t>(sysex::toneByteCount) * 2;
            for (std::size_t byte = 0; byte < bytes.size(); ++byte)
            {
                const std::size_t hex = firstHex + byte * 2;
                bytes[byte] = static_cast<std::uint8_t>(
                    (hexNibble(factoryToneHex[hex]) << 4u)
                    | hexNibble(factoryToneHex[hex + 1]));
            }

            // These directions accompanied the public archival names but are
            // not part of the eighteen hardware bytes. A host/product recall
            // supplies the playing setup; hardware Program Change and SysEx
            // deliberately leave the player's performance controls alone.
            Preset::Controls controls {};
            controls.volume = factoryVolume[index];
            switch (index)
            {
                case 13: // A26 Celeste: play one octave up
                case 16: // A31 Clock Chimes: play one octave up
                    controls.transpose = 12;
                    break;
                case 58: // A83 Drum Booms: play one octave down
                    controls.transpose = -12;
                    break;
                case 31: // A48 Synth Bass I
                case 43: // A64 Snare Drum
                case 44: // A65 Tom Toms
                case 45: // A66 Timpani
                    controls.keyMode = KeyMode::Unison;
                    break;
                default:
                    break;
            }

            presets[index] = Preset {
                numbers[index].data(),
                factoryNames[index],
                sysex::patchFromToneBytes(bytes.data()),
                controls
            };
        }
    }
};

const BankStorage bankStorage;
} // namespace

const std::array<Preset, presetCount>& factoryBank() noexcept
{
    return bankStorage.presets;
}

const Preset* findByNumber(const char* number) noexcept
{
    if (number == nullptr)
        return nullptr;
    for (const auto& preset : bankStorage.presets)
        if (std::strcmp(preset.number, number) == 0)
            return &preset;
    return nullptr;
}

} // namespace youknow106::presets
