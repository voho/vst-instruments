// Renders a fixed set of listening takes twice -- once before a fidelity fix
// and once after it -- so the change can be judged by ear rather than from a
// commit message.
//
//   YouKnow106RenderFidelityFixes <outputDir> before
//   ... apply the fix ...
//   YouKnow106RenderFidelityFixes <outputDir> after
//
// The `after` pass reads back each `-before.wav`, writes the difference, and
// reports it in dB relative to the before take. Unlike the SOTA comparison
// tool, which toggles one engine flag, this one compares two builds: it is the
// only way to hear a change that removes a mechanism outright, since afterwards
// there is no flag left to switch.
//
// The before take is compared through a 16-bit file, so an unchanged build
// still measures about -80 to -90 dBc. That is this tool's noise floor, not a
// difference. It is also comfortably below audibility, so a fix that lands in
// it is a fix nobody can hear.

#include "RenderSupport.h"

#include <fstream>
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

// The shipped instrument, not the pristine reference: these takes exist to show
// what a player hears, so Unit Character stays at its default throughout.
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

// Six voices on one key. Any inter-card pitch error turns the steady sum into
// an audible beat.
Take unisonTuning()
{
    auto p = panel();
    p.keyMode = KeyMode::Unison;
    p.cutoff = 0.80f;
    p.resonance = 0.05f;
    p.envDepth = 0.0f;
    p.attack = 0.0f;
    p.sustain = 1.0f;
    Take take(p);
    take.rest(0.05);
    take.hit(48, 0.95f, 4.0, 0.5);
    return take;
}

// A dense chord at full stored level and full volume: the hottest the final
// summer ever sees, which is where an output-stage nonlinearity shows up.
Take outputSummerDrive()
{
    auto p = panel();
    p.vcaLevel = 1.0f;
    p.volume = 1.0f;
    p.cutoff = 0.70f;
    p.resonance = 0.20f;
    p.attack = 0.02f;
    p.sustain = 1.0f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 40, 47, 52, 56, 59 }, 0.95f, 3.0, 0.5);
    return take;
}

// Bright sustained material through the faster chorus: exposes the wet path's
// delay trajectory, its own nonlinearities and its rate.
Take chorusWet()
{
    auto p = panel();
    p.chorus = ChorusMode::Two;
    p.cutoff = 0.85f;
    p.resonance = 0.05f;
    p.envDepth = 0.0f;
    p.attack = 0.10f;
    p.sustain = 1.0f;
    p.subLevel = 0.5f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 52, 59, 64, 68 }, 0.95f, 5.0, 0.8);
    return take;
}

// A slow, deep envelope sweep at high resonance: the filter's own character,
// including anything that modulates transconductance with signal level.
Take resonantSweep()
{
    auto p = panel();
    p.cutoff = 0.25f;
    p.resonance = 0.78f;
    p.envDepth = 0.75f;
    p.attack = 0.35f;
    p.decay = 0.75f;
    p.sustain = 0.15f;
    p.release = 0.50f;
    Take take(p);
    take.rest(0.05);
    take.hit(43, 0.95f, 3.0, 1.0);
    take.hit(55, 0.95f, 3.0, 1.0);
    return take;
}

// Full-level 16' saw plus sub with the filter open: the raw oscillator, where
// any waveshaping of the ramp lands as harmonic colour on a bass note.
Take sawBass()
{
    auto p = panel();
    p.range = DcoRange::Sixteen;
    p.cutoff = 1.0f;
    p.resonance = 0.0f;
    p.envDepth = 0.0f;
    p.attack = 0.0f;
    p.sustain = 1.0f;
    p.subLevel = 0.85f;
    Take take(p);
    take.rest(0.05);
    take.hit(36, 0.95f, 2.5, 0.4);
    take.hit(43, 0.95f, 2.5, 0.4);
    return take;
}

// Unit Character swept across a held chord. The control is meant to scale each
// modelled mechanism; this take says whether it does that or something else.
Take unitCharacterSweep()
{
    auto p = panel();
    p.cutoff = 0.55f;
    p.resonance = 0.45f;
    p.envDepth = 0.30f;
    p.attack = 0.05f;
    p.sustain = 0.85f;
    p.subLevel = 0.7f;
    p.noiseLevel = 0.2f;
    p.calibration = 0.0f;
    Take take(p);
    take.rest(0.05);
    take.on(48, 0.95f);
    take.on(55, 0.95f);
    take.on(60, 0.95f);
    for (int step = 0; step <= 20; ++step)
    {
        p.calibration = 0.25f * static_cast<float>(step); // 0 .. 5
        take.setParameters(p);
        take.rest(0.2);
    }
    take.off(48);
    take.off(55);
    take.off(60);
    take.rest(0.6);
    return take;
}

// A high, bright lead in the top range: the hardest case for the oscillator's
// bandlimiting, so alias products are loudest here.
Take highLead()
{
    auto p = panel();
    p.range = DcoRange::Four;
    p.cutoff = 1.0f;
    p.resonance = 0.0f;
    p.envDepth = 0.0f;
    p.attack = 0.0f;
    p.sustain = 1.0f;
    p.pulseEnabled = true;
    p.pwmDepth = 0.35f;
    Take take(p);
    take.rest(0.05);
    for (const int note : { 84, 91, 96, 100 })
        take.hit(note, 0.95f, 0.9, 0.15);
    return take;
}

struct Demo
{
    const char* slug;
    Take (*render)();
};

const std::array<Demo, 7> demos {{
    { "01-unison-tuning",           unisonTuning },
    { "02-output-summer-drive",     outputSummerDrive },
    { "03-chorus-wet",              chorusWet },
    { "04-resonant-sweep",          resonantSweep },
    { "05-saw-bass",                sawBass },
    { "06-unit-character-sweep",    unitCharacterSweep },
    { "07-high-lead",               highLead }
}};
} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path outputDir = "Docs/audio/fidelity-fixes";
    std::string stage = "after";
    if (argc > 1)
        outputDir = argv[1];
    if (argc > 2)
        stage = argv[2];

    if (stage != "before" && stage != "after")
    {
        std::printf("usage: %s <outputDir> <before|after>\n", argv[0]);
        return 1;
    }

    const bool comparing = stage == "after";
    std::printf("Rendering fidelity-fix takes (%s) into: %s\n\n",
                stage.c_str(), outputDir.string().c_str());
    if (comparing)
        std::printf("%-30s %12s %12s\n", "take", "diff peak", "diff RMS");

    std::string manifestRows;
    for (const auto& demo : demos)
    {
        auto take = demo.render();
        const std::string slug = demo.slug;
        const auto path = outputDir / (slug + "-" + stage + ".wav");

        // Both stages are level-matched to the same peak, so an A/B is a
        // comparison of character rather than of loudness.
        take.normalise();
        writeWav(path, take.left(), take.right());

        if (!comparing)
            continue;

        std::vector<float> beforeLeft;
        std::vector<float> beforeRight;
        if (!readWav(outputDir / (slug + "-before.wav"), beforeLeft, beforeRight))
        {
            std::printf("%-30s %12s %12s\n", demo.slug, "(no before)", "");
            continue;
        }

        const Take before(std::move(beforeLeft), std::move(beforeRight));
        const auto diff = take.diffWith(before);
        const auto diffLevel = diff.measure();
        const auto reference = std::max(before.measure().rms, 1.0e-12);
        const double peakDbc = decibels(diffLevel.peak / reference);
        const double rmsDbc = decibels(diffLevel.rms / reference);

        writeWav(outputDir / (slug + "-diff.wav"), diff.left(), diff.right());
        std::printf("%-30s %+12.1f %+12.1f\n", demo.slug, peakDbc, rmsDbc);

        std::array<char, 256> row {};
        std::snprintf(row.data(), row.size(), "| `%s` | %+.1f | %+.1f |\n",
                      demo.slug, peakDbc, rmsDbc);
        manifestRows += row.data();
    }

    if (comparing && !manifestRows.empty())
    {
        std::string manifest =
            "# Fidelity-fix listening takes\n\n"
            "Generated by `YouKnow106RenderFidelityFixes`, run once before a fix and\n"
            "once after it. Both stages are level-matched to the same peak, so an A/B\n"
            "is a comparison of character rather than of loudness; `-diff` is their\n"
            "sample difference.\n\n"
            "These takes compare two *builds*. The SOTA comparison renders next door\n"
            "compare two settings of one engine flag, which cannot show a change that\n"
            "removes a mechanism outright -- afterwards there is no flag left to\n"
            "switch.\n\n"
            "Every take uses the shipped panel with `Unit Character` at its default, so\n"
            "they describe what a player hears rather than a pristine reference.\n\n"
            "The before take is compared through a 16-bit file, so an unchanged build\n"
            "still measures about -80 to -90 dBc. That is the floor of this measurement,\n"
            "not a difference -- and it is below audibility, so a change that lands in it\n"
            "is a change nobody can hear.\n\n"
            "| Take | Diff peak (dBc) | Diff RMS (dBc) |\n"
            "| --- | ---: | ---: |\n";
        manifest += manifestRows;
        std::filesystem::create_directories(outputDir);
        std::ofstream readme(outputDir / "README.md");
        readme << manifest;
    }

    std::printf("\nWrote %zu takes\n", demos.size());
    return 0;
}
