// Renders representative before and after WAV files comparing each SOTA
// physical circuit simulation feature under settings tuned to make the effect
// cleanly audible.

#include "RenderSupport.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
using youknow106::ChorusMode;
using youknow106::DcoRange;
using youknow106::EngineParameters;
using youknow106::HighPassMode;
using youknow106::VcaMode;
using youknow106::tools::decibels;
using youknow106::tools::Take;
using youknow106::tools::writeWav;

EngineParameters defaultPanel()
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

// 1. VCF Stage Transistor Offsets: High resonance filter sweep on raw sawtooth
Take renderVcfOffsetsTake(bool enableOffsets)
{
    auto p = defaultPanel();
    p.enableVcfStageOffsets = enableOffsets;
    p.cutoff = 0.35f;
    p.resonance = 0.65f;
    p.envDepth = 0.50f;
    p.decay = 0.60f;
    p.sustain = 0.20f;
    Take take(p);
    take.rest(0.05);
    take.hit(48, 0.95f, 1.2, 0.4);
    take.hit(60, 0.95f, 1.2, 0.4);
    return take;
}

// 2. Op-Amp Slew-Rate Limiting: High-frequency resonant lead with Chorus II
Take renderOpAmpSlewTake(bool enableSlewLimiting)
{
    auto p = defaultPanel();
    p.enableOpAmpSlewLimiting = enableSlewLimiting;
    p.cutoff = 0.75f;
    p.resonance = 0.80f;
    p.envDepth = 0.20f;
    p.chorus = ChorusMode::Two;
    Take take(p);
    take.rest(0.05);
    take.chord({ 72, 76, 79 }, 0.95f, 1.5, 0.5);
    return take;
}

// 6. IR3109 VCF BJT Early Effect: Resonant filter sweep
Take renderVcfEarlyEffectTake(bool enableVcfEarlyEffect)
{
    auto p = defaultPanel();
    p.enableVcfEarlyEffect = enableVcfEarlyEffect;
    p.cutoff = 0.40f;
    p.resonance = 0.70f;
    p.envDepth = 0.50f;
    p.decay = 0.50f;
    Take take(p);
    take.rest(0.05);
    take.hit(48, 0.95f, 1.2, 0.4);
    return take;
}

// 7. Spatial Chassis Thermal Gradient: Sustained polyphonic chord sweep
Take renderSpatialThermalGradientTake(bool enableSpatialThermalGradient)
{
    auto p = defaultPanel();
    p.enableSpatialThermalGradient = enableSpatialThermalGradient;
    p.cutoff = 0.50f;
    p.resonance = 0.60f;
    p.attack = 0.30f;
    p.decay = 0.80f;
    p.sustain = 0.40f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 48, 55, 60, 64, 67, 72 }, 0.95f, 2.5, 0.5);
    return take;
}

// 8. Chorus Heterodyne Clock Bleed: Lush chorus pad
Take renderChorusClockBleedTake(bool enableChorusClockBleed)
{
    auto p = defaultPanel();
    p.enableChorusClockBleed = enableChorusClockBleed;
    p.sawEnabled = true;
    p.subLevel = 0.5f;
    p.chorus = ChorusMode::Two;
    p.cutoff = 0.65f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 55, 59, 62, 67 }, 0.95f, 2.0, 0.5);
    return take;
}

// 9. Chorus MN3101 Current-Controlled Hyperbolic Delay Sweep: Chorus pad
Take renderChorusHyperbolicSweepTake(bool enableChorusHyperbolicSweep)
{
    auto p = defaultPanel();
    p.enableChorusHyperbolicSweep = enableChorusHyperbolicSweep;
    p.sawEnabled = true;
    p.subLevel = 0.6f;
    p.chorus = ChorusMode::Two;
    p.cutoff = 0.60f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 48, 55, 60, 64, 67 }, 0.95f, 2.0, 0.5);
    return take;
}

// 11. C14 Non-Polar Electrolytic Voltage-Dependent HPF Modulation: Sub-bass + polyphonic chord
Take renderElectrolyticC14Take(bool enableElectrolyticC14Nonlinearity)
{
    auto p = defaultPanel();
    p.enableElectrolyticC14Nonlinearity = enableElectrolyticC14Nonlinearity;
    p.highPass = HighPassMode::Two;
    p.sawEnabled = true;
    p.subLevel = 1.0f;
    p.cutoff = 0.70f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 36, 60, 64, 67 }, 0.95f, 2.0, 0.5);
    return take;
}

// 12. MC5534A Pulse-Off pinned level through the real WAVE-node coupling.
// The held note makes the off/on events audible without confusing them with
// envelope attacks. The legacy side hard-disconnects the pulse leg; the
// circuit side leaves the comparator's documented high state on C56/C50.
Take renderPulseOffWaveNodeTake(bool enablePulseOffWaveNodeCoupling)
{
    auto p = defaultPanel();
    p.enablePulseOffWaveNodeCoupling = enablePulseOffWaveNodeCoupling;
    p.sawEnabled = false;
    p.pulseEnabled = true;
    p.subLevel = 0.0f;
    p.noiseLevel = 0.0f;
    p.cutoff = 1.0f;
    p.resonance = 0.0f;
    p.envDepth = 0.0f;
    p.vcaMode = VcaMode::Gate;
    p.vcaLevel = 1.0f;
    p.volume = 0.65f;
    Take take(p);
    take.rest(0.20);
    take.on(48, 1.0f);
    take.rest(0.60);
    p.pulseEnabled = false;
    take.setParameters(p);
    take.rest(0.70);
    p.pulseEnabled = true;
    take.setParameters(p);
    take.rest(0.70);
    take.off(48);
    take.rest(0.30);
    return take;
}

// 13. The scanned NOISE level controls the BA662 before its C41/R79 output
// pole. Repeated zero/full moves expose the short physical discharge/refill
// memory without adding a synthetic click or changing the settled spectrum.
Take renderNoiseOtaC41MemoryTake(bool enableNoiseLevelBeforeC41)
{
    auto p = defaultPanel();
    p.enableNoiseLevelBeforeC41 = enableNoiseLevelBeforeC41;
    p.sawEnabled = false;
    p.pulseEnabled = false;
    p.subLevel = 0.0f;
    p.noiseLevel = 0.0f;
    p.cutoff = 1.0f;
    p.resonance = 0.0f;
    p.envDepth = 0.0f;
    p.vcaMode = VcaMode::Gate;
    p.vcaLevel = 1.0f;
    p.volume = 0.65f;
    Take take(p);
    take.rest(0.20);
    take.on(48, 1.0f);
    take.rest(0.35);
    for (int transition = 0; transition < 8; ++transition)
    {
        p.noiseLevel = (transition % 2 == 0) ? 1.0f : 0.0f;
        take.setParameters(p);
        take.rest(0.28);
    }
    take.off(48);
    take.rest(0.30);
    return take;
}

struct Comparison
{
    const char* slug;
    Take (*render)(bool);
};

const std::array<Comparison, 9> comparisons {{
    { "01-vcf-transistor-offsets",        renderVcfOffsetsTake },
    { "02-opamp-slew-limiting",           renderOpAmpSlewTake },
    { "06-vcf-early-effect",              renderVcfEarlyEffectTake },
    { "07-spatial-thermal-gradient",      renderSpatialThermalGradientTake },
    { "08-chorus-thiran-clock-bleed",     renderChorusClockBleedTake },
    { "09-chorus-hyperbolic-sweep",       renderChorusHyperbolicSweepTake },
    { "11-electrolytic-c14-nonlinearity", renderElectrolyticC14Take },
    { "12-pulse-off-wave-node-coupling",  renderPulseOffWaveNodeTake },
    { "13-noise-ota-c41-memory",           renderNoiseOtaC41MemoryTake }
}};

struct Report
{
    std::string slug;
    double diffPeakDbc { 0.0 };
    double diffRmsDbc { 0.0 };
};

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path outputDir = "Docs/audio/sota-comparisons";
    if (argc > 1)
        outputDir = argv[1];

    std::printf("Rendering SOTA comparison WAVs into: %s\n\n", outputDir.string().c_str());
    std::printf("%-36s %12s %12s\n", "mechanism", "diff peak", "diff RMS");
    std::printf("%-36s %12s %12s\n", "", "(dBc)", "(dBc)");

    std::vector<Report> reports;
    reports.reserve(comparisons.size());

    for (const auto& comparison : comparisons)
    {
        auto before = comparison.render(false);
        auto after = comparison.render(true);
        auto diff = after.diffWith(before);

        // Measure before touching the gain. Everything is reported relative to
        // the programme the mechanism is riding on, which is the only figure
        // that says whether anyone can hear it.
        const auto beforeLevel = before.measure();
        const auto diffLevel = diff.measure();
        const double reference = std::max(beforeLevel.rms, 1.0e-12);
        Report report;
        report.slug = comparison.slug;
        report.diffPeakDbc = decibels(diffLevel.peak / reference);
        report.diffRmsDbc = decibels(diffLevel.rms / reference);
        reports.push_back(report);

        // One shared gain for all three files. The before/after pair is then
        // level-matched for honest A/B listening, and the diff keeps its true
        // size relative to them instead of being normalised up to look
        // significant.
        const double gain = before.normalise();
        after.applyGain(gain);
        diff.applyGain(gain);

        const std::string slug = comparison.slug;
        writeWav(outputDir / (slug + "-before.wav"), before.left(), before.right());
        writeWav(outputDir / (slug + "-after.wav"), after.left(), after.right());
        writeWav(outputDir / (slug + "-diff.wav"), diff.left(), diff.right());

        std::printf("%-36s %+12.1f %+12.1f\n", comparison.slug,
                    report.diffPeakDbc, report.diffRmsDbc);
    }

    // Keep the directory's own README in step with what was just rendered, so
    // the measured weight of each mechanism is documented rather than implied.
    std::string manifest =
        "# SOTA comparison renders\n\n"
        "Generated by `YouKnow106RenderSota`. Each row toggles **one** mechanism and\n"
        "leaves every other one at its shipped setting; `Unit Character` stays at its\n"
        "default in both takes, so the pair isolates the named feature rather than the\n"
        "whole character layer.\n\n"
        "`-before` and `-after` share one gain, so they are level-matched for A/B\n"
        "listening. `-diff` carries that same gain, so its loudness relative to the pair\n"
        "is its true loudness -- a diff that is inaudible in the file is inaudible in the\n"
        "instrument. The columns below are the diff measured against the before take's\n"
        "RMS.\n\n"
        "| Mechanism | Diff peak (dBc) | Diff RMS (dBc) |\n"
        "| --- | ---: | ---: |\n";
    for (const auto& report : reports)
    {
        std::array<char, 256> row {};
        std::snprintf(row.data(), row.size(), "| `%s` | %+.1f | %+.1f |\n",
                      report.slug.c_str(), report.diffPeakDbc, report.diffRmsDbc);
        manifest += row.data();
    }
    manifest +=
        "\nA mechanism whose diff RMS sits below about -80 dBc is not audible on the\n"
        "material it was given, whatever its entry in the research document claims.\n";

    std::filesystem::create_directories(outputDir);
    std::ofstream readme(outputDir / "README.md");
    readme << manifest;

    std::printf("\nWrote %zu comparisons and README.md\n", comparisons.size());
    return 0;
}
