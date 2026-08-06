// Renders one listening take per realism fix, before and after that fix, so
// each change ships with the evidence a listener can judge it by.
//
//   YouKnow106RenderRealismFixes <outputDir> <fix-slug> before
//   ... apply the fix ...
//   YouKnow106RenderRealismFixes <outputDir> <fix-slug> after
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
using youknow106::ChorusMode;
using youknow106::DcoRange;
using youknow106::EngineParameters;
using youknow106::HighPassMode;
using youknow106::KeyMode;
using youknow106::tools::decibels;
using youknow106::tools::readWav;
using youknow106::tools::Take;
using youknow106::tools::writeWav;

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

const std::array<Demo, 2> demos {{
    { "01-chorus-timing", chorusTiming },
    { "02-sweep-trajectory", sweepTrajectory },
}};

// metrics.csv keeps one row per fix so the README can be regenerated whole
// after any individual fix is re-rendered.
std::map<std::string, std::pair<double, double>> readMetrics(
    const std::filesystem::path& path)
{
    std::map<std::string, std::pair<double, double>> rows;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
    {
        std::istringstream stream(line);
        std::string slug;
        std::string peak;
        std::string rms;
        if (std::getline(stream, slug, ',') && std::getline(stream, peak, ',')
            && std::getline(stream, rms, ','))
        {
            try
            {
                rows[slug] = { std::stod(peak), std::stod(rms) };
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
    for (const auto& demo : demos)
    {
        if (requested != "all" && requested != demo.slug)
            continue;
        rendered = true;

        auto take = demo.render();
        const std::string slug = demo.slug;
        take.normalise();
        writeWav(outputDir / (slug + "-" + stage + ".wav"),
                 take.left(), take.right());
        std::printf("rendered %s-%s\n", slug.c_str(), stage.c_str());

        if (!comparing)
            continue;

        std::vector<float> beforeLeft;
        std::vector<float> beforeRight;
        if (!readWav(outputDir / (slug + "-before.wav"), beforeLeft,
                     beforeRight))
        {
            std::printf("%s: no before take found\n", slug.c_str());
            continue;
        }
        const Take before(std::move(beforeLeft), std::move(beforeRight));
        const auto diff = take.diffWith(before);
        const auto diffLevel = diff.measure();
        const auto reference = std::max(before.measure().rms, 1.0e-12);
        const double peakDbc = decibels(diffLevel.peak / reference);
        const double rmsDbc = decibels(diffLevel.rms / reference);
        writeWav(outputDir / (slug + "-diff.wav"), diff.left(), diff.right());
        std::printf("%-26s diff peak %+6.1f dBc   diff RMS %+6.1f dBc\n",
                    slug.c_str(), peakDbc, rmsDbc);
        metrics[slug] = { peakDbc, rmsDbc };
    }

    if (!rendered)
    {
        std::printf("unknown fix slug: %s\n", requested.c_str());
        return 1;
    }

    if (comparing)
    {
        std::ofstream csv(outputDir / "metrics.csv");
        for (const auto& [slug, row] : metrics)
            csv << slug << ',' << row.first << ',' << row.second << '\n';

        std::ofstream readme(outputDir / "README.md");
        readme <<
            "# Realism-fix listening takes\n\n"
            "Generated by `YouKnow106RenderRealismFixes`, run once before each fix\n"
            "and once after it, comparing two *builds* -- the only way to hear a\n"
            "change with no runtime flag left to switch. Each fix has its own take,\n"
            "composed to expose that fix rather than sharing one generic panel.\n\n"
            "Both stages are level-matched to the same peak, so an A/B compares\n"
            "character rather than loudness; `-diff` carries the sample\n"
            "difference at the same gain, so its loudness is its true loudness.\n"
            "The before take passes through a 16-bit file, so an unchanged build\n"
            "still measures about -80 to -90 dBc: that is this measurement's\n"
            "floor, not a difference, and it is below audibility.\n\n"
            "| Take | Diff peak (dBc) | Diff RMS (dBc) |\n"
            "| --- | ---: | ---: |\n";
        for (const auto& [slug, row] : metrics)
        {
            std::array<char, 256> line {};
            std::snprintf(line.data(), line.size(), "| `%s` | %+.1f | %+.1f |\n",
                          slug.c_str(), row.first, row.second);
            readme << line.data();
        }
    }
    return 0;
}
