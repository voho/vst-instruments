// Renders one listening take per realism fix, before and after that fix, so
// each change ships with the evidence a listener can judge it by.
//
//   YouKnowRenderRealismFixes <outputDir> <fix-slug> before
//   ... apply the fix ...
//   YouKnowRenderRealismFixes <outputDir> <fix-slug> after
//
// Unlike RenderFidelityFixes, which renders one fixed panel across every take,
// each fix here gets a take composed to expose that fix: a chorus-timing change
// is inaudible on a dry bass patch, and a converter change is inaudible on a
// held pad. The `after` pass reads back `<slug>-before.wav`, writes the
// difference, and accumulates every fix's measurement into `metrics.csv`, from
// which it regenerates the directory README -- so a later fix does not discard
// an earlier fix's row.
//
// Both stages are level-matched to the same peak, so an A/B compares character
// rather than loudness. The before take passes through a 16-bit file, so an
// unchanged build still measures about -80 to -90 dBc; that is the floor of
// this measurement, not a difference, and it is below audibility.

#include "RenderSupport.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace
{
using youknow::ChorusMode;
using youknow::DcoRange;
using youknow::EngineParameters;
using youknow::HighPassMode;
using youknow::KeyMode;
using youknow::tools::decibels;
using youknow::tools::readWav;
using youknow::tools::Take;
using youknow::tools::writeWav;

EngineParameters panel()
{
    EngineParameters parameters;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 0.62f;
    parameters.resonance = 0.10f;
    parameters.envDepth = 0.35f;
    parameters.keyFollow = 0.5f;
    parameters.attack = 0.04f;
    parameters.decay = 0.45f;
    parameters.sustain = 0.70f;
    parameters.release = 0.30f;
    parameters.vcaLevel = 0.84f;
    parameters.chorus = ChorusMode::Off;
    parameters.volume = 0.45f;
    return parameters;
}

// A bright sustained chord through chorus I, then II. The wet path's rate,
// sweep depth and bandwidth are all in plain view against the steady dry
// component; a timing change moves the whole vibrato figure.
Take chorusTiming()
{
    auto p = panel();
    p.chorus = ChorusMode::One;
    p.cutoff = 0.85f;
    p.resonance = 0.05f;
    p.envDepth = 0.0f;
    p.attack = 0.08f;
    p.sustain = 1.0f;
    p.subLevel = 0.4f;
    Take take(p);
    take.rest(0.05);
    take.on(52, 0.95f);
    take.on(59, 0.95f);
    take.on(64, 0.95f);
    take.rest(5.0);
    p.chorus = ChorusMode::Two;
    take.setParameters(p);
    take.rest(5.0);
    take.off(52);
    take.off(59);
    take.off(64);
    take.rest(1.2);
    return take;
}

// One held note through chorus I. With a single steady partial series against
// the dry component, the wet line's pitch trajectory is the take: a
// delay-linear sweep holds a constant detune per flank where the hyperbolic
// law slides through it.
Take sweepTrajectory()
{
    auto p = panel();
    p.chorus = ChorusMode::One;
    p.cutoff = 0.85f;
    p.resonance = 0.05f;
    p.envDepth = 0.0f;
    p.attack = 0.08f;
    p.sustain = 1.0f;
    p.subLevel = 0.2f;
    Take take(p);
    take.rest(0.05);
    take.on(64, 0.95f);
    take.rest(9.0);
    take.off(64);
    take.rest(1.2);
    return take;
}

struct Demo
{
    const char* slug;
    Take (*render)();
};

// Staccato high arpeggio through chorus II. Transient-rich content keeps
// fresh high-frequency energy entering the line at every sweep position, so
// the wet path's clock-dependent smear -- and any error in it -- reads as
// the brightness of each echoing attack against the dry component.
Take transferLoss()
{
    auto p = panel();
    p.chorus = ChorusMode::Two;
    p.cutoff = 0.95f;
    p.resonance = 0.05f;
    p.envDepth = 0.0f;
    p.attack = 0.0f;
    p.decay = 0.25f;
    p.sustain = 0.0f;
    p.release = 0.12f;
    Take take(p);
    take.rest(0.05);
    for (const int note : { 76, 81, 85, 88, 85, 81, 76, 81, 85, 88, 85, 81 })
    {
        take.on(note, 0.95f);
        take.rest(0.22);
        take.off(note);
        take.rest(0.18);
    }
    take.rest(1.4);
    return take;
}

// One low note under a slow, deep, high-resonance envelope sweep, chorus off.
// The per-stage pair offsets express themselves as even-harmonic asymmetry
// and small DC shifts that ride the resonant peak as it crosses the
// harmonics -- the exact conditions their mechanism entry names.
Take stageOffsets()
{
    auto p = panel();
    p.chorus = ChorusMode::Off;
    p.cutoff = 0.30f;
    p.resonance = 0.92f;
    p.envDepth = 0.55f;
    // Panel positions for a 1.8 s attack and 2.6 s decay -- these fields are
    // normalised slider travel, not seconds, and a first revision of this
    // take wrote seconds into them, pinning both at maximum and spending the
    // whole take inside the attack instead of the sweep it was composed for.
    p.attack = youknow::YouKnowEngine::panelPositionForAttack(1.8f);
    p.decay = youknow::YouKnowEngine::panelPositionForDecay(2.6f);
    p.sustain = 0.15f;
    p.release = 0.4f;
    p.subLevel = 0.0f;
    Take take(p);
    take.rest(0.05);
    take.on(45, 0.95f);
    take.rest(7.5);
    take.off(45);
    take.rest(1.0);
    return take;
}

// Dark staccato notes with an abrupt gate. The voice VCA's control
// feedthrough expresses itself as the small thump that rides each gate edge;
// a dark filter keeps the programme quiet at exactly those instants, so the
// click against near-silence is the figure.
Take vcaFeedthrough()
{
    auto p = panel();
    p.chorus = ChorusMode::Off;
    p.cutoff = 0.22f;
    p.resonance = 0.15f;
    p.envDepth = 0.0f;
    p.attack = 0.0f;
    p.decay = 0.3f;
    p.sustain = 1.0f;
    p.release = 0.01f;
    p.subLevel = 0.3f;
    Take take(p);
    take.rest(0.05);
    for (const int note : { 43, 43, 50, 43, 46, 43, 50, 46 })
    {
        take.on(note, 0.9f);
        take.rest(0.16);
        take.off(note);
        take.rest(0.34);
    }
    take.rest(0.8);
    return take;
}

// A unison stack gliding an octave up and back down, then a bend-like fast
// glide. Every converter pass of the glide steps six timer counts while six
// compensation CVs chase them, which is exactly where the ramp's momentary
// amplitude error -- and any rendering artefact riding it -- lives.
Take pitchGlide()
{
    auto p = panel();
    p.chorus = ChorusMode::Off;
    p.keyMode = KeyMode::Unison;
    p.portamento = 0.35f;
    p.cutoff = 0.72f;
    p.resonance = 0.12f;
    p.envDepth = 0.0f;
    p.attack = 0.02f;
    p.sustain = 1.0f;
    p.subLevel = 0.25f;
    Take take(p);
    take.rest(0.05);
    take.on(41, 0.95f);
    take.rest(1.2);
    take.on(53, 0.95f);
    take.rest(2.6);
    take.off(53);
    take.rest(2.4);
    p.portamento = 0.16f;
    take.setParameters(p);
    take.on(58, 0.95f);
    take.rest(1.4);
    take.off(58);
    take.off(41);
    take.rest(1.0);
    return take;
}

const std::array<Demo, 6> demos {{
    { "01-chorus-timing", chorusTiming },
    { "02-sweep-trajectory", sweepTrajectory },
    { "03-bbd-transfer-loss", transferLoss },
    { "04-stage-offsets", stageOffsets },
    { "05-vca-feedthrough", vcaFeedthrough },
    { "06-pitch-glide", pitchGlide },
}};

// metrics.csv keeps one row per fix so the README can be regenerated whole
// after any individual fix is re-rendered. The fourth column records the gain
// applied to the written diff file when the raw difference would not fit the
// format; rows from older manifests without it read as unscaled.
struct MetricsRow
{
    double peakDbc { 0.0 };
    double rmsDbc { 0.0 };
    double diffGainDb { 0.0 };
};

std::map<std::string, MetricsRow> readMetrics(
    const std::filesystem::path& path)
{
    std::map<std::string, MetricsRow> rows;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
    {
        std::istringstream stream(line);
        std::string slug;
        std::string peak;
        std::string rms;
        std::string gain;
        if (std::getline(stream, slug, ',') && std::getline(stream, peak, ',')
            && std::getline(stream, rms, ','))
        {
            try
            {
                MetricsRow row { std::stod(peak), std::stod(rms), 0.0 };
                if (std::getline(stream, gain, ','))
                    row.diffGainDb = std::stod(gain);
                rows[slug] = row;
            }
            catch (...) {}
        }
    }
    return rows;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::printf("usage: %s <outputDir> <fix-slug|all> <before|after>\n",
                    argv[0]);
        return 1;
    }
    const std::filesystem::path outputDir = argv[1];
    const std::string requested = argv[2];
    const std::string stage = argv[3];
    if (stage != "before" && stage != "after")
    {
        std::printf("stage must be before or after\n");
        return 1;
    }
    const bool comparing = stage == "after";
    std::filesystem::create_directories(outputDir);

    auto metrics = readMetrics(outputDir / "metrics.csv");
    bool rendered = false;
    bool missingBefore = false;
    for (const auto& demo : demos)
    {
        if (requested != "all" && requested != demo.slug)
            continue;
        rendered = true;

        auto take = demo.render();
        const std::string slug = demo.slug;
        take.normalise();
        if (!writeWav(outputDir / (slug + "-" + stage + ".wav"),
                      take.left(), take.right()))
        {
            // A stale file next to fresh metrics is an inconsistent audit;
            // stop before the manifest can say otherwise.
            std::printf("%s: could not write the %s take\n", slug.c_str(),
                        stage.c_str());
            return 1;
        }
        std::printf("rendered %s-%s\n", slug.c_str(), stage.c_str());

        if (!comparing)
            continue;

        std::vector<float> beforeLeft;
        std::vector<float> beforeRight;
        if (!readWav(outputDir / (slug + "-before.wav"), beforeLeft,
                     beforeRight))
        {
            // A comparison with no baseline is a failed comparison, not a
            // no-op: without this, a stale manifest would present an older
            // measurement as this render's result.
            std::printf("%s: no before take found\n", slug.c_str());
            missingBefore = true;
            continue;
        }
        // A baseline of a different length is a baseline of a different
        // take -- an older revision of this demo. diffWith would silently
        // truncate to the common prefix and the README would present that
        // fragment as a comparison of the whole take; refuse instead, so the
        // stale before file gets re-rendered deliberately.
        if (beforeLeft.size() != take.left().size()
            || beforeRight.size() != take.right().size())
        {
            std::printf("%s: before take is %zu/%zu frames against this "
                        "build's %zu/%zu -- a different take; re-render it\n",
                        slug.c_str(), beforeLeft.size(), beforeRight.size(),
                        take.left().size(), take.right().size());
            missingBefore = true;
            continue;
        }
        const Take before(std::move(beforeLeft), std::move(beforeRight));
        auto diff = take.diffWith(before);
        const auto diffLevel = diff.measure();
        const auto reference = std::max(before.measure().rms, 1.0e-12);
        const double peakDbc = decibels(diffLevel.peak / reference);
        const double rmsDbc = decibels(diffLevel.rms / reference);
        // Two independently normalised takes can differ by up to twice one
        // take's peak, and the file writer clamps at full scale. The metrics
        // above are taken from the unclipped difference; when writing it out,
        // scale it just under full scale and record the applied gain so the
        // artifact stays reversible instead of silently clipping.
        double diffGainDb = 0.0;
        if (diffLevel.peak > 0.999)
        {
            const double scale = 0.999 / diffLevel.peak;
            diff.applyGain(scale);
            diffGainDb = decibels(scale);
            std::printf("%s: diff written %.2f dB down to fit full scale\n",
                        slug.c_str(), -diffGainDb);
        }
        if (!writeWav(outputDir / (slug + "-diff.wav"), diff.left(),
                      diff.right()))
        {
            std::printf("%s: could not write the diff take\n", slug.c_str());
            return 1;
        }
        std::printf("%-26s diff peak %+6.1f dBc   diff RMS %+6.1f dBc\n",
                    slug.c_str(), peakDbc, rmsDbc);
        metrics[slug] = { peakDbc, rmsDbc, diffGainDb };
    }

    if (!rendered)
    {
        std::printf("unknown fix slug: %s\n", requested.c_str());
        return 1;
    }
    if (missingBefore)
    {
        std::printf("aborting without rewriting the manifest: a requested "
                    "comparison has no usable before take\n");
        return 1;
    }

    if (comparing)
    {
        std::ofstream csv(outputDir / "metrics.csv");
        for (const auto& [slug, row] : metrics)
            csv << slug << ',' << row.peakDbc << ',' << row.rmsDbc << ','
                << row.diffGainDb << '\n';
        csv.close();
        if (!csv)
        {
            std::printf("could not write the metrics manifest\n");
            return 1;
        }

        std::ofstream readme(outputDir / "README.md");
        readme <<
            "# Realism-fix listening takes\n\n"
            "Generated by `YouKnowRenderRealismFixes`, run once before each fix\n"
            "and once after it, comparing two *builds* -- the only way to hear a\n"
            "change with no runtime flag left to switch. Each fix has its own take,\n"
            "composed to expose that fix rather than sharing one generic panel.\n\n"
            "Both stages are level-matched to the same peak, so an A/B compares\n"
            "character rather than loudness; `-diff` carries the sample\n"
            "difference at the same gain, so its loudness is its true loudness.\n"
            "The one exception is a difference too large for the file format:\n"
            "that file is written just under full scale instead, and the gain\n"
            "it was given is in the last column, so the artifact is reversible\n"
            "rather than silently clipped. The metrics are always taken from\n"
            "the unscaled difference. The before take passes through a 16-bit\n"
            "file, so an unchanged build still measures about -80 to -90 dBc:\n"
            "that is this measurement's floor, not a difference, and it is\n"
            "below audibility.\n\n"
            "| Take | Diff peak (dBc) | Diff RMS (dBc) | Diff file gain (dB) |\n"
            "| --- | ---: | ---: | ---: |\n";
        for (const auto& [slug, row] : metrics)
        {
            std::array<char, 256> line {};
            std::snprintf(line.data(), line.size(),
                          "| `%s` | %+.1f | %+.1f | %+.2f |\n",
                          slug.c_str(), row.peakDbc, row.rmsDbc,
                          row.diffGainDb);
            readme << line.data();
        }
        readme.close();
        if (!readme)
        {
            std::printf("could not write the manifest README\n");
            return 1;
        }
    }
    return 0;
}
