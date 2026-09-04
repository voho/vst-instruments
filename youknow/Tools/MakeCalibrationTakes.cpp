// Calibration takes: the MIDI files, patch dumps and matching model renders
// for measuring an instrument against this model.
//
// ---------------------------------------------------------------------------
// PROTOCOL
//
// Each take is one panel setup and one short performance, chosen so that ONE
// mechanism dominates the recording and one feature of the result identifies
// it. The operator loads the patch, arms a recorder, and plays the take's MIDI
// file; this tool writes the model's own render of the identical take, so the
// two are compared without any further alignment than a start offset.
//
// What the operator's chain does to the signal is unknown -- gain, EQ,
// converter, and any limiting. Every measurement below is therefore a RATIO, a
// TIME, a RATE or a NULL between two takes recorded in the same session, never
// an absolute level, with one exception (take 12) which exists only to supply
// the session's own level reference and is marked as such.
//
// Evidence class. A recording of REAL HARDWARE, from an identified and
// serviced unit, is a measurement and can close an open question. A render
// from a SOFTWARE INSTRUMENT is a model, not the instrument: it can corroborate
// (or contradict) a bracketed value and show whether this model is an outlier
// among independent models of the same circuit, and that is all. The
// manufacturer's own model ranks above a third-party model and below hardware.
// Whichever it is, it is recorded in the decision log as what it is.
//
// The patch travels three ways, in falling order of trust:
//   1. the .syx file, or the patch dump embedded at the head of the .mid --
//      the hardware's own 18-byte tone format, so no setting is transcribed;
//   2. the printed panel table in the manifest, for an instrument that will
//      not take the dump;
//   3. a factory-bank program number, for an instrument that has the same
//      bank -- zero setup, but the patch is not an isolator.
// Takes 1-12 use route 1 or 2. Takes marked FACTORY use route 3 and exist so
// that a session with no SysEx path still produces something comparable.
//
// Route 1 is narrower than it looks, which is why route 2 gets its own output.
// Original hardware takes the dump, and so does this plug-in. Among software
// Junos surveyed on 2026-09-04, only Cherry Audio's DCO-106 documents Juno-106
// SysEx compatibility, and it receives from a live MIDI stream rather than
// opening a .syx file. Roland's own Cloud JUNO-106, TAL-U-NO-LX, Arturia
// Jun-6 V, Softube Model 84, u-he Diva and the JU-06A Boutique all publish no
// SysEx receive for this format. So this tool writes TWO sets:
//
//   with-sysex/ -- the takes above, patch embedded, controls stepped mid-take.
//   manual/     -- the same measurements with every step expanded into its own
//                  file at a static panel, for an instrument where the operator
//                  sets the controls by hand. Nothing is stepped, nothing is
//                  transcribed except the printed panel, and each file names
//                  the single control that differs from the take before it.
//
// The manual set cannot see a switching transient -- the mute drive's 81 ms,
// the high-pass leg tails -- because those live in the moment of the change
// itself. Those measurements need route 1 or hardware.
//
// The modelled keyboard is the hardware's own 61 keys, C2..C7 = MIDI 36..96;
// no take steps outside that span. The keybed is not velocity sensitive, so
// every note is written at velocity 100 and velocity carries no information.
// Every take ends with at least two seconds of silence, which is where the
// release tail and the noise floor are read.
// ---------------------------------------------------------------------------

#include "DSP/YouKnowEngine.h"
#include "DSP/YouKnowPresets.h"
#include "DSP/YouKnowSysEx.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
using namespace youknow;

// --- Standard MIDI file ----------------------------------------------------
// Type 0, one track, 1000 ticks per quarter against a 1 000 000 us quarter, so
// one tick is exactly one millisecond and every time below is in ms.
constexpr int ticksPerQuarter = 1000;
constexpr int microsecondsPerQuarter = 1000000;

void pushBig(std::vector<std::uint8_t>& out, std::uint32_t value, int bytes)
{
    for (int index = bytes - 1; index >= 0; --index)
        out.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xffu));
}

// Variable-length quantity, high bit set on every byte but the last.
void pushVariable(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    std::uint8_t buffer[5];
    int count = 0;
    buffer[count++] = static_cast<std::uint8_t>(value & 0x7fu);
    while ((value >>= 7) != 0)
        buffer[count++] = static_cast<std::uint8_t>((value & 0x7fu) | 0x80u);
    while (count > 0)
        out.push_back(buffer[--count]);
}

void pushText(std::vector<std::uint8_t>& out, const std::string& text)
{
    for (const char character : text)
        out.push_back(static_cast<std::uint8_t>(character));
}

struct MidiEvent
{
    int timeMs { 0 };
    std::vector<std::uint8_t> bytes;   // the complete event, status byte first
    bool isSysEx { false };
};

// Writes an SMF type 0 whose only track carries the events, sorted by time.
bool writeMidiFile(const std::filesystem::path& path,
                   const std::string& trackName,
                   std::vector<MidiEvent> events, int endMs)
{
    std::stable_sort(events.begin(), events.end(),
                     [](const MidiEvent& a, const MidiEvent& b) {
                         return a.timeMs < b.timeMs;
                     });

    std::vector<std::uint8_t> track;
    // Track name, then tempo, both at tick zero.
    pushVariable(track, 0);
    track.push_back(0xffu);
    track.push_back(0x03u);
    pushVariable(track, static_cast<std::uint32_t>(trackName.size()));
    pushText(track, trackName);

    pushVariable(track, 0);
    track.push_back(0xffu);
    track.push_back(0x51u);
    track.push_back(0x03u);
    pushBig(track, microsecondsPerQuarter, 3);

    int previousMs = 0;
    for (const auto& event : events)
    {
        pushVariable(track, static_cast<std::uint32_t>(event.timeMs - previousMs));
        previousMs = event.timeMs;
        if (event.isSysEx)
        {
            // F0 is the event type; the length counts the bytes AFTER it,
            // which include the terminating F7 and exclude the leading F0.
            track.push_back(0xf0u);
            pushVariable(track,
                         static_cast<std::uint32_t>(event.bytes.size() - 1));
            track.insert(track.end(), event.bytes.begin() + 1, event.bytes.end());
        }
        else
        {
            track.insert(track.end(), event.bytes.begin(), event.bytes.end());
        }
    }

    pushVariable(track, static_cast<std::uint32_t>(std::max(0, endMs - previousMs)));
    track.push_back(0xffu);
    track.push_back(0x2fu);
    track.push_back(0x00u);

    std::vector<std::uint8_t> file;
    pushText(file, "MThd");
    pushBig(file, 6u, 4);
    pushBig(file, 0u, 2);                 // format 0
    pushBig(file, 1u, 2);                 // one track
    pushBig(file, ticksPerQuarter, 2);
    pushText(file, "MTrk");
    pushBig(file, static_cast<std::uint32_t>(track.size()), 4);
    file.insert(file.end(), track.begin(), track.end());

    std::ofstream stream(path, std::ios::binary);
    if (!stream)
        return false;
    stream.write(reinterpret_cast<const char*>(file.data()),
                 static_cast<std::streamsize>(file.size()));
    return stream.good();
}

// --- 32-bit float WAV ------------------------------------------------------
// Float, because the noise floors these takes measure sit far below what a
// 16-bit file can carry.
constexpr double renderSampleRate = 48000.0;

bool writeFloatWav(const std::filesystem::path& path,
                   const std::vector<float>& left,
                   const std::vector<float>& right)
{
    const auto frames = static_cast<std::uint32_t>(left.size());
    const std::uint32_t dataBytes = frames * 2u * 4u;
    std::vector<std::uint8_t> file;
    pushText(file, "RIFF");
    const auto pushLittle = [&file](std::uint32_t value, int bytes) {
        for (int index = 0; index < bytes; ++index)
            file.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xffu));
    };
    pushLittle(36u + dataBytes, 4);
    pushText(file, "WAVE");
    pushText(file, "fmt ");
    pushLittle(16u, 4);
    pushLittle(3u, 2);                    // IEEE float
    pushLittle(2u, 2);
    pushLittle(static_cast<std::uint32_t>(renderSampleRate), 4);
    pushLittle(static_cast<std::uint32_t>(renderSampleRate) * 2u * 4u, 4);
    pushLittle(8u, 2);
    pushLittle(32u, 2);
    pushText(file, "data");
    pushLittle(dataBytes, 4);
    for (std::uint32_t index = 0; index < frames; ++index)
        for (int channel = 0; channel < 2; ++channel)
        {
            const float value = channel == 0 ? left[index] : right[index];
            std::uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            pushLittle(bits, 4);
        }

    std::ofstream stream(path, std::ios::binary);
    if (!stream)
        return false;
    stream.write(reinterpret_cast<const char*>(file.data()),
                 static_cast<std::streamsize>(file.size()));
    return stream.good();
}

// --- Takes -----------------------------------------------------------------

struct Note
{
    int note { 60 };
    int onMs { 0 };
    int offMs { 1000 };
};

struct Take
{
    std::string id;
    std::string purpose;      // the quantity, and what cancels the chain gain
    std::string panel;        // the printed setup, for route 2
    sysex::Patch patch {};
    std::vector<Note> notes;
    int endMs { 0 };
    const char* factoryNumber { nullptr };  // route 3, when set
};

// The panel that every isolation take starts from: one source at a time, the
// filter wide open and out of the way, the amplifier a plain gate at full
// level, no chorus, no modulation. What each take does is turn exactly one
// thing on.
sysex::Patch openPanel()
{
    sysex::Patch patch {};
    patch.lfoRate = 0.0f;
    patch.lfoDelay = 0.0f;
    patch.dcoLfo = 0.0f;
    patch.pwm = 0.0f;
    patch.noise = 0.0f;
    patch.cutoff = 1.0f;
    patch.resonance = 0.0f;
    patch.vcfEnv = 0.0f;
    patch.vcfLfo = 0.0f;
    patch.keyFollow = 0.0f;
    patch.vcaLevel = 1.0f;
    patch.attack = 0.0f;
    patch.decay = 0.0f;
    patch.sustain = 1.0f;
    patch.release = 0.0f;
    patch.sub = 0.0f;
    patch.range = DcoRange::Eight;
    patch.saw = false;
    patch.pulse = false;
    patch.pwmSource = PwmSource::Manual;
    patch.vcaMode = VcaMode::Gate;
    patch.envPolarity = EnvPolarity::Normal;
    patch.highPass = HighPassMode::One;    // the flat, straight-through leg
    patch.chorus = ChorusMode::Off;
    return patch;
}

// One note per octave up the modelled keybed, each held two seconds with a one
// second gap. The gaps are where the between-note floor is read.
std::vector<Note> octaveLadder(int startMs = 500)
{
    std::vector<Note> notes;
    int cursor = startMs;
    for (const int note : { 36, 48, 60, 72, 84, 96 })
    {
        notes.push_back({ note, cursor, cursor + 2000 });
        cursor += 3000;
    }
    return notes;
}

std::vector<Take> buildTakes()
{
    std::vector<Take> takes;

    // 1-3. The three oscillator sources alone, on the same ladder. Their
    // relative levels are OQ-15's whole content, and a ratio between two takes
    // of one session cancels the chain. The ladder also reads the high-note
    // amplitude taper the DCO CV table produces once it saturates.
    {
        Take take;
        take.id = "01-saw";
        take.purpose = "Saw level and spectrum against pitch (OQ-15 saw "
                       "coordinate; the high-note taper once the DCO CV "
                       "saturates). Measured as the ratio to takes 02/03 and "
                       "as the shape of level against note, both chain-free.";
        take.panel = "SAW on, PULSE off, SUB 0, NOISE 0, RANGE 8', "
                     "VCF FREQ max, RES 0, ENV 0, LFO 0, KYBD 0, "
                     "HPF flat (position 1), VCA GATE, VCA LEVEL max, "
                     "CHORUS off";
        take.patch = openPanel();
        take.patch.saw = true;
        take.notes = octaveLadder();
        take.endMs = 20000;
        takes.push_back(take);
    }
    {
        Take take;
        take.id = "02-pulse";
        take.purpose = "Pulse level and spectrum at the trimmed 50% duty "
                       "(OQ-15 pulse coordinate). Its odd-harmonic-only "
                       "spectrum also reads the real duty: the second "
                       "harmonic's depth against the first is the duty error, "
                       "which the service procedure bounds at 48-52%.";
        take.panel = "As take 01 but PULSE on, SAW off, PWM source MANUAL, "
                     "PWM slider fully down (50% duty)";
        take.patch = openPanel();
        take.patch.pulse = true;
        take.notes = octaveLadder();
        take.endMs = 20000;
        takes.push_back(take);
    }
    {
        Take take;
        take.id = "03-sub";
        take.purpose = "Sub level and spectrum (OQ-15 sub coordinate, the one "
                       "the earlier corpus pass already found contradicted). "
                       "The sub is a half-wave gated square an octave down, so "
                       "its even harmonics carry the diode gate's signature.";
        take.panel = "As take 01 but SAW off, PULSE off, SUB slider at max";
        take.patch = openPanel();
        take.patch.sub = 1.0f;
        take.notes = octaveLadder();
        take.endMs = 20000;
        takes.push_back(take);
    }

    // 4. Noise alone, stepped through the control's bottom. The first
    // conducting step is OQ-16's deadband, bracketed at 4-11% of travel, and
    // the level ratio against take 01 is the crest reading the ear could not
    // separate.
    {
        Take take;
        take.id = "04-noise";
        take.purpose = "NOISE level, deadband and spectrum (OQ-16). The four "
                       "held notes step the NOISE slider by SysEx: bytes 4, 8, "
                       "64, 127. Which of the first two is silent brackets the "
                       "Tr22 onset; the 64-to-127 ratio reads the control law; "
                       "the level against take 01 reads the crest convention. "
                       "If the instrument ignores parameter SysEx this take "
                       "degrades to NOISE at whatever the patch set, and the "
                       "deadband must instead be found by hand.";
        take.panel = "As take 01 but SAW off, NOISE slider at max. If stepping "
                     "by hand: record four passes at NOISE = 4, 8, 64 and 127 "
                     "of 127.";
        take.patch = openPanel();
        take.patch.noise = 1.0f;
        take.notes = { { 60, 500, 3000 }, { 60, 4000, 6500 },
                       { 60, 7500, 10000 }, { 60, 11000, 13500 } };
        take.endMs = 16000;
        takes.push_back(take);
    }

    // 5. Everything at once. This is the null: if takes 01-04 have fixed each
    // coordinate, this take must follow without another free parameter.
    {
        Take take;
        take.id = "05-mixed";
        take.purpose = "All four sources together, as the null that the three "
                       "coordinates fixed by takes 01-04 must predict with "
                       "nothing left free (OQ-15). Also the WAVE node's "
                       "summing behaviour when the sub's half-wave mean rides "
                       "on it.";
        take.panel = "As take 01 but SAW on, PULSE on, SUB max, NOISE at 64";
        take.patch = openPanel();
        take.patch.saw = true;
        take.patch.pulse = true;
        take.patch.sub = 1.0f;
        take.patch.noise = 64.0f / 127.0f;
        take.notes = octaveLadder();
        take.endMs = 20000;
        takes.push_back(take);
    }

    // 6. The filter swept by its own envelope, which needs no slider to move:
    // a full ENV amount with a slow attack walks the cutoff from the bottom of
    // its range to the top on one held note. Time is the measurand, so the
    // chain cancels completely.
    {
        Take take;
        take.id = "06-cutoff-sweep";
        take.purpose = "The cutoff law across its whole range and the upper "
                       "saturation knee (OQ-18), swept by the filter envelope "
                       "so no slider moves. The source is NOISE, not an "
                       "oscillator: a flat spectrum shows the filter's own "
                       "magnitude response directly, where a saw's 1/n "
                       "spectrum hides the cutoff behind its own rolloff once "
                       "the filter is past the tenth harmonic. The measurand "
                       "is the trajectory of the high-to-low band ratio "
                       "against time, a shape, not a level.";
        take.panel = "NOISE at max, SAW off, PULSE off, SUB 0, VCF FREQ 0, "
                     "RES 0, ENV amount max, polarity normal, ATTACK max, "
                     "DECAY max, SUSTAIN max, RELEASE 0, VCA GATE, "
                     "VCA LEVEL max, CHORUS off, HPF flat";
        take.patch = openPanel();
        take.patch.noise = 1.0f;
        take.patch.cutoff = 0.0f;
        take.patch.vcfEnv = 1.0f;
        take.patch.attack = 1.0f;
        take.patch.decay = 1.0f;
        take.patch.sustain = 1.0f;
        take.notes = { { 36, 500, 12500 }, { 60, 13500, 25500 } };
        take.endMs = 28000;
        takes.push_back(take);
    }

    // 7. The filter as the only source. Its pitch against the keyboard is the
    // cutoff law read as a frequency, and a frequency needs no level
    // reference at all -- the single most chain-immune measurement here.
    {
        Take take;
        take.id = "07-self-oscillation";
        take.purpose = "The filter at full resonance with full key follow, "
                       "played as a voice (OQ-09 endpoint, OQ-18 law). Pitch "
                       "is the measurand, so the chain cannot touch it: the "
                       "service manual's own 248 Hz check point at converter "
                       "code 6272 lives on this curve, and the octave-to-"
                       "octave error reads the WIDTH trim.";
        take.panel = "SAW off, PULSE off, SUB 0, NOISE 0, VCF FREQ at 49/127, "
                     "RES max, ENV 0, KYBD max, VCA GATE, VCA LEVEL max, "
                     "CHORUS off, HPF flat";
        take.patch = openPanel();
        take.patch.cutoff = 49.0f / 127.0f;
        take.patch.resonance = 1.0f;
        take.patch.keyFollow = 1.0f;
        take.notes = octaveLadder();
        take.endMs = 20000;
        takes.push_back(take);
    }

    // 8. Resonance stepped under a saw, which is the loop-gain law itself.
    {
        Take take;
        take.id = "08-resonance-steps";
        take.purpose = "The resonance byte-to-loop-gain law (OQ-09), stepped "
                       "by SysEx across six held notes at bytes 0, 4, 16, 48, "
                       "80 and 112. The source is NOISE so the resonant peak "
                       "stands alone against a flat background; driven by a "
                       "saw instead, the peak lands on a harmonic and cannot "
                       "be separated from it. The measurand is the peak's "
                       "height above the same take's own passband, a ratio "
                       "within one note, so the chain cancels twice over. "
                       "Where the peak first appears is the onset the +0.26 V "
                       "standoff predicts, and the cutoff sits at 60/127 so "
                       "the peak lands near 580 Hz, clear of the low end.";
        take.panel = "NOISE at max, SAW off, PULSE off, SUB 0, "
                     "VCF FREQ at 60/127, ENV 0, KYBD 0, VCA GATE, "
                     "VCA LEVEL max, CHORUS off, HPF flat. If stepping by "
                     "hand: six passes at RES = 0, 4, 16, 48, 80, 112 of 127.";
        take.patch = openPanel();
        take.patch.noise = 1.0f;
        take.patch.cutoff = 60.0f / 127.0f;
        take.notes = { { 48, 500, 3000 }, { 48, 4000, 6500 },
                       { 48, 7500, 10000 }, { 48, 11000, 13500 },
                       { 48, 14500, 17000 }, { 48, 18000, 20500 } };
        take.endMs = 23000;
        takes.push_back(take);
    }

    // 9. The envelope, timed. Digital and ROM-resolved, so this take is the
    // session's self-check: if these times do not null, the transcription or
    // the recording is wrong and nothing else in the session can be trusted.
    {
        Take take;
        take.id = "09-envelope-times";
        take.purpose = "Envelope segment times (OQ-12) and the session's own "
                       "SELF-CHECK: the envelope is digital and ROM-resolved, "
                       "so these times must already null. A systematic offset "
                       "here means the recording or the clock is wrong, and "
                       "invalidates every other take in the session. Four "
                       "notes step A/D/S/R by SysEx: fast, slow attack, slow "
                       "decay to zero sustain, long release.";
        take.panel = "SAW on, VCF FREQ max, RES 0, ENV 0, VCA ENV mode, "
                     "VCA LEVEL max, CHORUS off, HPF flat. Envelope per note; "
                     "if stepping by hand, four passes: (A0 D0 S127 R0), "
                     "(A96 D0 S127 R0), (A0 D96 S0 R0), (A0 D0 S127 R96).";
        take.patch = openPanel();
        take.patch.saw = true;
        take.patch.vcaMode = VcaMode::Envelope;
        take.notes = { { 60, 500, 3000 }, { 60, 5000, 9000 },
                       { 60, 11000, 15000 }, { 60, 17000, 19000 } };
        take.endMs = 24000;
        takes.push_back(take);
    }

    // 10. The chorus, on one held chord, all three states. Rate and depth are
    // times and frequencies; the hiss is a ratio to the same take's own note.
    {
        Take take;
        take.id = "10-chorus";
        take.purpose = "Chorus rate, depth, stereo image and hiss (OQ-01, "
                       "OQ-03) plus the wet-mute drive's switching delay "
                       "(OQ-20). One held chord crosses off, I, II and back to "
                       "off by SysEx. Rate is read from the L-R modulation "
                       "period and depth from the pitch deviation, both "
                       "chain-free; hiss is the between-note floor ratioed to "
                       "the chord, and the mute delay is the time from the "
                       "SysEx byte to the wet return appearing.";
        take.panel = "SAW on, SUB at 64, VCF FREQ at 90/127, RES 0, ENV 0, "
                     "VCA GATE, VCA LEVEL max, HPF flat. Chorus stepped "
                     "off -> I -> II -> off; if stepping by hand, three "
                     "passes at OFF, I and II.";
        take.patch = openPanel();
        take.patch.saw = true;
        take.patch.sub = 64.0f / 127.0f;
        take.patch.cutoff = 90.0f / 127.0f;
        take.notes = { { 48, 500, 24500 }, { 55, 500, 24500 },
                       { 60, 500, 24500 } };
        take.endMs = 28000;
        takes.push_back(take);
    }

    // 11. The four high-pass positions on one held chord. Relative transfer
    // between the positions of the same take: entirely chain-free.
    {
        Take take;
        take.id = "11-high-pass";
        take.purpose = "The four high-pass positions and their switching "
                       "transients (OQ-21). Each position's transfer relative "
                       "to the flat position within this one take is chain-"
                       "free; the boost leg's departing tail and its x11 low "
                       "band are what the model changed most recently.";
        take.panel = "SAW on, SUB max, VCF FREQ max, RES 0, ENV 0, VCA GATE, "
                     "VCA LEVEL max, CHORUS off. HPF stepped through its four "
                     "positions; if stepping by hand, four passes.";
        take.patch = openPanel();
        take.patch.saw = true;
        take.patch.sub = 1.0f;
        take.notes = { { 36, 500, 20500 }, { 48, 500, 20500 } };
        take.endMs = 24000;
        takes.push_back(take);
    }

    // 12. The level reference. Everything above is a ratio; this take exists so
    // the session has one absolute anchor, and it is the only take whose
    // reading depends on the operator writing down what the chain was doing.
    {
        Take take;
        take.id = "12-level-reference";
        take.purpose = "THE SESSION'S LEVEL REFERENCE, and the VCA LEVEL law "
                       "(OQ-02) and output clipping (OQ-05). VCA LEVEL steps "
                       "127, 96, 64, 32 by SysEx under a fixed saw. This is "
                       "the ONE take whose value depends on the chain, so the "
                       "operator must record the gain settings and leave them "
                       "untouched for every other take in the session.";
        take.panel = "SAW on, PULSE on, SUB max, VCF FREQ max, RES 0, ENV 0, "
                     "VCA GATE, CHORUS off, HPF flat. VCA LEVEL stepped "
                     "127, 96, 64, 32; if stepping by hand, four passes.";
        take.patch = openPanel();
        take.patch.saw = true;
        take.patch.pulse = true;
        take.patch.sub = 1.0f;
        take.notes = { { 48, 500, 3000 }, { 48, 4000, 6500 },
                       { 48, 7500, 10000 }, { 48, 11000, 13500 } };
        take.endMs = 16000;
        takes.push_back(take);
    }

    // 13-15. Factory programs, for a session with no SysEx path at all. These
    // are not isolators -- they are the zero-setup fallback, and they are the
    // same 18 tone bytes on both sides by construction.
    struct FactoryTake { const char* number; const char* id; const char* why; };
    for (const auto& entry : {
             FactoryTake { "A11", "13-factory-a11-brass",
                           "A bright saw-and-pulse patch with a filter "
                           "envelope: the broadest single check of source "
                           "balance and envelope shape with zero setup." },
             FactoryTake { "B18", "14-factory-b18-noise-sweep",
                           "A noise-dominant sweep: the NOISE level and the "
                           "cutoff trajectory with zero setup (OQ-16)." },
             FactoryTake { "A48", "15-factory-a48-synth-bass",
                           "A unison sub-heavy bass: the sub coordinate and "
                           "the unison stack with zero setup (OQ-15)." } })
    {
        const auto* preset = presets::findByNumber(entry.number);
        if (preset == nullptr)
            continue;
        Take take;
        take.id = entry.id;
        take.purpose = std::string("FACTORY PROGRAM ") + entry.number + ". "
                     + entry.why + " Select the program on the instrument; no "
                       "patch loading is needed, and both sides carry the same "
                       "18 tone bytes by construction.";
        take.panel = std::string("Select factory program ") + entry.number
                   + " (" + preset->name + "). Touch nothing else.";
        take.patch = preset->patch;
        take.factoryNumber = entry.number;
        take.notes = { { 36, 500, 3000 }, { 48, 3500, 6000 },
                       { 60, 6500, 9000 }, { 72, 9500, 12000 },
                       { 48, 13000, 17000 }, { 55, 13000, 17000 },
                       { 60, 13000, 17000 }, { 64, 13000, 17000 } };
        take.endMs = 21000;
        takes.push_back(take);
    }

    return takes;
}

// Long takes, for a reference whose output is corrupted at random.
//
// A demo build that injects distortion at random times cannot be cleaned by
// filtering, but it can be outvoted. Every measurement these takes carry is
// either a RATE, recovered from tens of cycles so a corrupted second perturbs
// it negligibly, or a level read as the MEDIAN of many independent windows,
// which a minority of corrupted windows cannot move. Nothing here is measured
// from a single moment.
//
// The subject is the chorus, because it is the one P0 mechanism whose primary
// measurand is a frequency: the modulation rates this project derives from the
// instrument's own T-network as 0.5533 and 0.8983 Hz, and the 1.6235 ratio
// between them, which no recording chain and no injected distortion can shift.
// The silences at either end read the idle floor for OQ-03 in the same file,
// and the ratio between the two chorus states' floors is the mode-II delta.
//
// One note, held. The panel is otherwise the plainest the instrument has, so
// the only thing modulating is the effect under test.
std::vector<Take> buildLongTakes()
{
    struct LongTake { const char* id; ChorusMode chorus; const char* label; };
    std::vector<Take> takes;
    for (const auto& entry : {
             LongTake { "L1-chorus-off", ChorusMode::Off, "CHORUS off" },
             LongTake { "L2-chorus-one", ChorusMode::One, "CHORUS I" },
             LongTake { "L3-chorus-two", ChorusMode::Two, "CHORUS II" } })
    {
        Take take;
        take.id = entry.id;
        take.purpose =
            std::string("Chorus rate, depth and idle floor with ") + entry.label
            + ". The held note carries the modulation: its rate is recovered "
              "from about fifty cycles, so neither the chain nor a randomly "
              "corrupted second can move it, and it tests the derived "
              "0.5533/0.8983 Hz pair and their 1.6235 ratio (OQ-01). The two "
              "silences read the idle floor as the median of many independent "
              "windows, which a minority of corrupted windows cannot move, and "
              "the difference between this take's floor and the chorus-off "
              "take's is the chorus hiss (OQ-03). Every figure is a rate or a "
              "median ratio; none is an absolute level.";
        take.panel =
            std::string("SAW on, PULSE off, SUB 0, NOISE 0, RANGE 8', "
                        "VCF FREQ max, RES 0, ENV 0, LFO 0, KYBD 0, HPF flat, "
                        "VCA GATE, VCA LEVEL max, and ") + entry.label
            + ". Set it once; nothing moves during the take.";
        take.patch = openPanel();
        take.patch.saw = true;
        take.patch.chorus = entry.chorus;
        // 15 s of silence, 100 s of held note, 20 s of silence.
        take.notes = { { 60, 15000, 115000 } };
        take.endMs = 135000;
        takes.push_back(take);
    }
    return takes;
}

// The parameter-change steps a take asks for, as (timeMs, parameter, value).
// The parameter numbers are this codec's own indices; see ToneParameter.
struct ParameterStep { int timeMs; int parameter; int value; };

std::vector<ParameterStep> parameterStepsFor(const std::string& id)
{
    const int noiseIndex = static_cast<int>(sysex::ToneParameter::DcoNoise);
    const int resIndex = static_cast<int>(sysex::ToneParameter::VcfRes);
    const int attackIndex = static_cast<int>(sysex::ToneParameter::EnvAttack);
    const int decayIndex = static_cast<int>(sysex::ToneParameter::EnvDecay);
    const int sustainIndex = static_cast<int>(sysex::ToneParameter::EnvSustain);
    const int releaseIndex = static_cast<int>(sysex::ToneParameter::EnvRelease);
    const int vcaIndex = static_cast<int>(sysex::ToneParameter::VcaLevel);
    const int switchesOne = static_cast<int>(sysex::ToneParameter::SwitchesOne);
    const int switchesTwo = static_cast<int>(sysex::ToneParameter::SwitchesTwo);

    if (id == "04-noise")
        return { { 200, noiseIndex, 4 }, { 3700, noiseIndex, 8 },
                 { 7200, noiseIndex, 64 }, { 10700, noiseIndex, 127 } };
    if (id == "08-resonance-steps")
        return { { 200, resIndex, 0 }, { 3700, resIndex, 4 },
                 { 7200, resIndex, 16 }, { 10700, resIndex, 48 },
                 { 14200, resIndex, 80 }, { 17700, resIndex, 112 } };
    if (id == "09-envelope-times")
        return { { 200, attackIndex, 0 }, { 210, decayIndex, 0 },
                 { 220, sustainIndex, 127 }, { 230, releaseIndex, 0 },
                 { 4000, attackIndex, 96 },
                 { 10000, attackIndex, 0 }, { 10010, decayIndex, 96 },
                 { 10020, sustainIndex, 0 },
                 { 16000, decayIndex, 0 }, { 16010, sustainIndex, 127 },
                 { 16020, releaseIndex, 96 } };
    if (id == "10-chorus")
    {
        // Switch byte one carries the chorus bits: bit 5 clear means on, bit 6
        // selects I. Saw on (bit 4) and the 8' range (bit 1) stay set.
        const int base = (1 << 1) | (1 << 4);
        const int off = base | (1 << 5);
        const int one = base | (1 << 6);
        const int two = base;
        return { { 200, switchesOne, off }, { 6000, switchesOne, one },
                 { 12000, switchesOne, two }, { 18000, switchesOne, off } };
    }
    if (id == "11-high-pass")
    {
        // Switch byte two: bits 3-4 are the position, counting down, and bit 0
        // holds PWM on manual, bit 2 the gate amplifier.
        const int base = (1 << 0) | (1 << 2);
        return { { 200, switchesTwo, base | (3 << 3) },   // Boost
                 { 5200, switchesTwo, base | (2 << 3) },  // flat
                 { 10200, switchesTwo, base | (1 << 3) }, // cut II
                 { 15200, switchesTwo, base | (0 << 3) } };
    }
    if (id == "12-level-reference")
        return { { 200, vcaIndex, 127 }, { 3700, vcaIndex, 96 },
                 { 7200, vcaIndex, 64 }, { 10700, vcaIndex, 32 } };
    return {};
}

EngineParameters engineParametersFor(const sysex::Patch& patch)
{
    EngineParameters parameters {};
    parameters.lfoRate = patch.lfoRate;
    parameters.lfoDelay = patch.lfoDelay;
    parameters.dcoLfoDepth = patch.dcoLfo;
    parameters.pwmDepth = patch.pwm;
    parameters.pwmSource = patch.pwmSource;
    parameters.range = patch.range;
    parameters.sawEnabled = patch.saw;
    parameters.pulseEnabled = patch.pulse;
    parameters.subLevel = patch.sub;
    parameters.noiseLevel = patch.noise;
    parameters.highPass = patch.highPass;
    parameters.cutoff = patch.cutoff;
    parameters.resonance = patch.resonance;
    parameters.envPolarity = patch.envPolarity;
    parameters.envDepth = patch.vcfEnv;
    parameters.vcfLfoDepth = patch.vcfLfo;
    parameters.keyFollow = patch.keyFollow;
    parameters.vcaMode = patch.vcaMode;
    parameters.vcaLevel = patch.vcaLevel;
    parameters.attack = patch.attack;
    parameters.decay = patch.decay;
    parameters.sustain = patch.sustain;
    parameters.release = patch.release;
    parameters.chorus = patch.chorus;
    // Product defaults, so the model's side of the comparison is the product:
    // the full modelled tolerance profile and the chosen converter placement.
    parameters.volume = 1.0f;
    parameters.polyphony = 6;
    return parameters;
}

// One take rendered to the end, with the parameter-change SysEx inside the
// file applied on the way, exactly as an instrument reading the file would.
struct RenderResult
{
    std::vector<float> left, right;
    double peak { 0.0 };
};

RenderResult renderTake(const std::vector<MidiEvent>& events,
                        const sysex::Patch& startPatch, int endMs,
                        float calibration)
{
    YouKnowEngine engine;
    engine.selectConverterTimingProfile(
        YouKnowEngine::ConverterTimingProfile::MeasuredChartGeometry);
    engine.prepare(renderSampleRate, 256, 4);

    auto patch = startPatch;
    auto parameters = engineParametersFor(patch);
    parameters.calibration = calibration;
    engine.setParameters(parameters);

    std::vector<MidiEvent> ordered = events;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const MidiEvent& a, const MidiEvent& b) {
                         return a.timeMs < b.timeMs;
                     });

    RenderResult result;
    std::array<float, 256> blockLeft {}, blockRight {};
    std::size_t nextEvent = 0;
    int renderedMs = 0;
    while (renderedMs < endMs)
    {
        while (nextEvent < ordered.size()
               && ordered[nextEvent].timeMs <= renderedMs)
        {
            const auto& event = ordered[nextEvent++];
            if (event.isSysEx)
            {
                if (event.bytes.size() == 7
                    && event.bytes[2] == sysex::parameterOpcode)
                {
                    (void) sysex::applyParameter(patch, event.bytes[4],
                                                 event.bytes[5]);
                    parameters = engineParametersFor(patch);
                    parameters.calibration = calibration;
                    engine.setParameters(parameters);
                }
            }
            else if ((event.bytes[0] & 0xf0u) == 0x90u)
                engine.noteOn(event.bytes[1], 1.0f);
            else
                engine.noteOff(event.bytes[1]);
        }
        const int nextBoundary = nextEvent < ordered.size()
                                     ? ordered[nextEvent].timeMs : endMs;
        const int spanMs =
            std::max(1, std::min(nextBoundary, endMs) - renderedMs);
        int remaining =
            static_cast<int>(std::lround(spanMs * renderSampleRate / 1000.0));
        while (remaining > 0)
        {
            const int count = std::min<int>(256, remaining);
            engine.process(blockLeft.data(), blockRight.data(), count);
            result.left.insert(result.left.end(), blockLeft.begin(),
                               blockLeft.begin() + count);
            result.right.insert(result.right.end(), blockRight.begin(),
                                blockRight.begin() + count);
            remaining -= count;
        }
        renderedMs += spanMs;
    }
    for (const float value : result.left)
        result.peak = std::max(result.peak, static_cast<double>(std::abs(value)));
    return result;
}
} // namespace

// The human-readable name of the one control a manual step moves, and its
// value, for the panel line the operator actually reads.
std::string stepDescription(int parameter, int value)
{
    switch (static_cast<sysex::ToneParameter>(parameter))
    {
        case sysex::ToneParameter::DcoNoise:
            return "NOISE = " + std::to_string(value) + " of 127";
        case sysex::ToneParameter::VcfRes:
            return "VCF RES = " + std::to_string(value) + " of 127";
        case sysex::ToneParameter::VcaLevel:
            return "VCA LEVEL = " + std::to_string(value) + " of 127";
        case sysex::ToneParameter::EnvAttack:
            return "ENV ATTACK = " + std::to_string(value) + " of 127";
        case sysex::ToneParameter::EnvDecay:
            return "ENV DECAY = " + std::to_string(value) + " of 127";
        case sysex::ToneParameter::EnvSustain:
            return "ENV SUSTAIN = " + std::to_string(value) + " of 127";
        case sysex::ToneParameter::EnvRelease:
            return "ENV RELEASE = " + std::to_string(value) + " of 127";
        case sysex::ToneParameter::SwitchesOne:
        {
            sysex::Patch probe {};
            (void) sysex::applyParameter(probe, parameter, value);
            switch (probe.chorus)
            {
                case ChorusMode::Off:  return "CHORUS = off";
                case ChorusMode::One:  return "CHORUS = I";
                default:               return "CHORUS = II";
            }
        }
        case sysex::ToneParameter::SwitchesTwo:
        {
            sysex::Patch probe {};
            (void) sysex::applyParameter(probe, parameter, value);
            switch (probe.highPass)
            {
                case HighPassMode::Boost: return "HPF = bass boost (position 0)";
                case HighPassMode::One:   return "HPF = flat (position 1)";
                case HighPassMode::Two:   return "HPF = cut II (position 2)";
                default:                  return "HPF = cut III (position 3)";
            }
        }
        default:
            return "parameter " + std::to_string(parameter) + " = "
                 + std::to_string(value);
    }
}

int main(int argc, char** argv)
{
    const std::filesystem::path root =
        argc > 1 ? argv[1] : "calibration-takes";
    const std::filesystem::path outputDirectory = root / "with-sysex";
    const std::filesystem::path manualDirectory = root / "manual";
    std::error_code error;
    std::filesystem::create_directories(outputDirectory, error);
    std::filesystem::create_directories(manualDirectory, error);

    const auto takes = buildTakes();
    std::ofstream manifest(root / "manifest.txt");
    manifest << "YouKnow calibration takes\n"
                "Render each .mid through the instrument and return the WAV "
                "under the same name.\n"
                "48 kHz or better, 24-bit or float, no limiting, no "
                "normalisation, one gain setting for the whole session.\n\n"
                "with-sysex/  the patch is embedded in each .mid and controls "
                "step during the take. Original hardware and this plug-in "
                "accept it; among software Junos only Cherry Audio's DCO-106 "
                "documents Juno-106 SysEx receive, from a live MIDI stream.\n"
                "manual/      the same measurements with every step expanded "
                "into its own file at a static panel. Set the panel by hand "
                "from the 'panel' line, play the file, record. The switching "
                "transients (chorus mute delay, high-pass leg tails) cannot be "
                "measured this way -- they live in the change itself.\n\n";

    std::size_t manualFiles = 0;
    for (const auto& take : takes)
    {
        std::vector<std::uint8_t> dump(sysex::patchMessageBytes);
        if (sysex::writePatchMessage(take.patch, 0, dump.data(), dump.size())
            != dump.size())
        {
            std::fprintf(stderr, "FAIL %s: cannot encode hardware patch\n",
                         take.id.c_str());
            return 1;
        }

        std::ofstream syx(outputDirectory / (take.id + ".syx"), std::ios::binary);
        syx.write(reinterpret_cast<const char*>(dump.data()),
                  static_cast<std::streamsize>(dump.size()));

        // The dump is what another instrument will actually receive, so read
        // it back through this codec's own decoder and check it describes the
        // patch that was intended. A take whose dump does not round-trip would
        // silently measure a different panel than the one it documents.
        sysex::Patch decoded {};
        int decodedChannel = -1;
        const bool roundTrips =
            sysex::readPatchMessage(dump.data(), dump.size(), decoded,
                                    decodedChannel)
            && decoded.saw == take.patch.saw
            && decoded.pulse == take.patch.pulse
            && decoded.range == take.patch.range
            && decoded.chorus == take.patch.chorus
            && decoded.highPass == take.patch.highPass
            && decoded.vcaMode == take.patch.vcaMode
            && decoded.pwmSource == take.patch.pwmSource
            && decoded.envPolarity == take.patch.envPolarity
            && std::abs(decoded.cutoff - take.patch.cutoff) <= 0.004f
            && std::abs(decoded.resonance - take.patch.resonance) <= 0.004f
            && std::abs(decoded.noise - take.patch.noise) <= 0.004f
            && std::abs(decoded.sub - take.patch.sub) <= 0.004f
            && std::abs(decoded.vcaLevel - take.patch.vcaLevel) <= 0.004f;
        if (!roundTrips)
        {
            std::fprintf(stderr,
                         "FAIL %s: the patch dump does not decode back to the "
                         "take's own panel\n", take.id.c_str());
            return 1;
        }

        std::vector<MidiEvent> events;
        // The whole panel, at the head of the file, in the hardware's own
        // format. An instrument that takes it needs no manual setup at all.
        events.push_back({ 0, dump, true });
        const auto steps = parameterStepsFor(take.id);
        for (const auto& step : steps)
            events.push_back({ step.timeMs,
                               { 0xf0u, sysex::manufacturerId,
                                 sysex::parameterOpcode, 0x00u,
                                 static_cast<std::uint8_t>(step.parameter),
                                 static_cast<std::uint8_t>(step.value), 0xf7u },
                               true });
        for (const auto& note : take.notes)
        {
            events.push_back({ note.onMs,
                               { 0x90u, static_cast<std::uint8_t>(note.note),
                                 100u }, false });
            events.push_back({ note.offMs,
                               { 0x80u, static_cast<std::uint8_t>(note.note),
                                 0u }, false });
        }
        writeMidiFile(outputDirectory / (take.id + ".mid"), take.id,
                      events, take.endMs);

        // Two renders of the identical take. The product one carries the
        // instrument as it ships, per-card tolerances and all; the nominal one
        // is the calibrated-nominal model with every tolerance at zero. A LAW
        // is measured against the nominal render, because the product render's
        // own seeded per-card trims are noise on that question; the product
        // render is what says whether the shipped instrument as a whole lands
        // where the recording does.
        const auto product = renderTake(events, take.patch, take.endMs, 1.0f);
        const auto nominal = renderTake(events, take.patch, take.endMs, 0.0f);
        writeFloatWav(outputDirectory / (take.id + "-model.wav"),
                      product.left, product.right);
        writeFloatWav(outputDirectory / (take.id + "-model-nominal.wav"),
                      nominal.left, nominal.right);
        const double peak = product.peak;

        manifest << "== " << take.id << " ==\n"
                 << "purpose : " << take.purpose << "\n"
                 << "panel   : " << take.panel << "\n"
                 << "steps   : " << (steps.empty() ? "none"
                                                   : "parameter SysEx inside "
                                                     "the .mid")
                 << "\n"
                 << "length  : " << (take.endMs / 1000.0) << " s\n"
                 << "model   : peak " << (peak > 0.0
                                              ? 20.0 * std::log10(peak) : -144.0)
                 << " dBFS\n\n";

        std::printf("%-28s %5.1f s  model peak %7.2f dBFS  %s\n",
                    take.id.c_str(), take.endMs / 1000.0,
                    peak > 0.0 ? 20.0 * std::log10(peak) : -144.0,
                    take.factoryNumber ? take.factoryNumber : "");

        // --- the same take, expanded for an instrument with no SysEx path ---
        // Each step becomes its own file at a static panel. A take with no
        // steps is copied across unchanged, because it already is one.
        struct ManualVariant
        {
            std::string suffix;
            std::string control;
            sysex::Patch patch;
            std::vector<Note> notes;
            int endMs;
        };
        std::vector<ManualVariant> variants;
        if (steps.empty())
        {
            variants.push_back({ "", "as the panel line above", take.patch,
                                 take.notes, take.endMs });
        }
        else
        {
            // Steps are cumulative writes, so the panel at step i is the base
            // patch with steps 0..i applied. The notes that belong to a step
            // are the ones that start inside its window; a take whose notes
            // are all held across every step (the chord takes) repeats its
            // whole chord in each variant instead.
            auto running = take.patch;
            for (std::size_t index = 0; index < steps.size(); ++index)
            {
                (void) sysex::applyParameter(running, steps[index].parameter,
                                             steps[index].value);
                const int from = steps[index].timeMs;
                const int to = index + 1 < steps.size()
                                   ? steps[index + 1].timeMs : take.endMs;
                // Fold a run of writes at the same instant into one variant:
                // take 09 sets four envelope bytes before a single note.
                if (index + 1 < steps.size()
                    && steps[index + 1].timeMs - from < 100)
                    continue;

                std::vector<Note> windowNotes;
                for (const auto& note : take.notes)
                    if (note.onMs >= from && note.onMs < to)
                        windowNotes.push_back(note);
                // A chord take holds its notes across every step, so nothing
                // STARTS inside a window. Take the notes that OVERLAP it and
                // clip them to it; requiring a note to span the window
                // entirely silently dropped the last step of every chord take,
                // because the chord is released before the take ends.
                if (windowNotes.empty())
                    for (const auto& note : take.notes)
                        if (note.onMs <= from && note.offMs > from)
                            windowNotes.push_back({ note.note, from,
                                                    std::min(note.offMs, to) });
                if (windowNotes.empty())
                    continue;

                int earliest = windowNotes.front().onMs;
                for (const auto& note : windowNotes)
                    earliest = std::min(earliest, note.onMs);
                std::vector<Note> shifted;
                int latest = 0;
                for (const auto& note : windowNotes)
                {
                    shifted.push_back({ note.note, note.onMs - earliest + 500,
                                        note.offMs - earliest + 500 });
                    latest = std::max(latest, shifted.back().offMs);
                }
                char suffix[32];
                std::snprintf(suffix, sizeof suffix, "-%02zu", variants.size() + 1);
                variants.push_back({ suffix,
                                     stepDescription(steps[index].parameter,
                                                     steps[index].value),
                                     running, shifted, latest + 2500 });
            }
        }

        for (const auto& variant : variants)
        {
            const std::string name = take.id + variant.suffix;
            std::vector<MidiEvent> manualEvents;
            for (const auto& note : variant.notes)
            {
                manualEvents.push_back(
                    { note.onMs, { 0x90u, static_cast<std::uint8_t>(note.note),
                                   100u }, false });
                manualEvents.push_back(
                    { note.offMs, { 0x80u, static_cast<std::uint8_t>(note.note),
                                    0u }, false });
            }
            writeMidiFile(manualDirectory / (name + ".mid"), name,
                          manualEvents, variant.endMs);
            const auto manualProduct =
                renderTake(manualEvents, variant.patch, variant.endMs, 1.0f);
            const auto manualNominal =
                renderTake(manualEvents, variant.patch, variant.endMs, 0.0f);
            writeFloatWav(manualDirectory / (name + "-model.wav"),
                          manualProduct.left, manualProduct.right);
            writeFloatWav(manualDirectory / (name + "-model-nominal.wav"),
                          manualNominal.left, manualNominal.right);
            manifest << "  manual/" << name << ".mid  --  " << take.panel
                     << ", and " << variant.control << ". Length "
                     << (variant.endMs / 1000.0) << " s.\n";
            ++manualFiles;
        }
        manifest << "\n";
    }

    // The long takes, for a reference whose output is corrupted at random.
    // They carry no SysEx and no steps, so one file is one static panel.
    const std::filesystem::path longDirectory = root / "long";
    std::filesystem::create_directories(longDirectory, error);
    manifest << "\n== long takes (for a reference that corrupts at random) ==\n";
    for (const auto& take : buildLongTakes())
    {
        std::vector<MidiEvent> longEvents;
        for (const auto& note : take.notes)
        {
            longEvents.push_back(
                { note.onMs, { 0x90u, static_cast<std::uint8_t>(note.note),
                               100u }, false });
            longEvents.push_back(
                { note.offMs, { 0x80u, static_cast<std::uint8_t>(note.note),
                                0u }, false });
        }
        writeMidiFile(longDirectory / (take.id + ".mid"), take.id, longEvents,
                      take.endMs);
        const auto product = renderTake(longEvents, take.patch, take.endMs, 1.0f);
        writeFloatWav(longDirectory / (take.id + "-model.wav"),
                      product.left, product.right);
        manifest << "  long/" << take.id << ".mid  --  " << take.panel
                 << " Length " << (take.endMs / 1000.0) << " s.\n"
                 << "      " << take.purpose << "\n";
        std::printf("%-28s %5.1f s  model peak %7.2f dBFS  (long)\n",
                    take.id.c_str(), take.endMs / 1000.0,
                    product.peak > 0.0 ? 20.0 * std::log10(product.peak)
                                       : -144.0);
    }

    std::printf("\nWrote %zu takes to %s, %zu static-panel files to %s, "
                "and 3 long takes to %s\n",
                takes.size(), outputDirectory.string().c_str(), manualFiles,
                manualDirectory.string().c_str(),
                longDirectory.string().c_str());
    return 0;
}
