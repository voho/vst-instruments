// The plug-in's public parameters, in the exact order the processor exposes
// them. Two suites need this list and they run on different platforms, so it
// lives in one place rather than in both.
//
// `VST3BundleSmokeTests` is macOS-only: it locates the standard Bypass and
// Program parameters at `publicParameterOrder.size()` and `+ 1`, so a
// parameter added to the layout without being added here silently pushes them
// out of position and fails as "VST3 omitted its standard bypass parameter".
// That is exactly what happened when `VCF Tanh` and `VCF Fast Early` were
// added: main went red on macOS and stayed red, because nothing on Linux or
// Windows could see it. `PluginProcessorTests` now checks the live processor
// against this same list on every platform, so the drift is caught where it
// is introduced.
//
// Anything added to `createParameterLayout` must be appended here in the same
// position.
#pragma once

#include <array>

namespace youknow106::tests
{

struct PublicParameter
{
    const char* id;
    const char* name;
};

inline constexpr std::array<PublicParameter, 45> publicParameterOrder {{
    PublicParameter { "volume", "Volume" },
    PublicParameter { "benderDco", "Bender DCO" },
    PublicParameter { "benderVcf", "Bender VCF" },
    PublicParameter { "benderLfo", "Bender LFO" },
    PublicParameter { "portamento", "Portamento" },
    PublicParameter { "keyMode", "Key Mode (legacy)" },
    PublicParameter { "lfoRate", "LFO Rate" },
    PublicParameter { "lfoDelay", "LFO Delay" },
    PublicParameter { "dcoLfo", "DCO LFO" },
    PublicParameter { "pwm", "PWM" },
    PublicParameter { "pwmMode", "PWM Mode" },
    PublicParameter { "range", "Range" },
    PublicParameter { "saw", "Saw" },
    PublicParameter { "pulse", "Pulse" },
    PublicParameter { "sub", "Sub" },
    PublicParameter { "noise", "Noise" },
    PublicParameter { "highPass", "HPF" },
    PublicParameter { "cutoff", "VCF Freq" },
    PublicParameter { "resonance", "VCF Res" },
    PublicParameter { "envPolarity", "VCF Env Polarity" },
    PublicParameter { "vcfEnv", "VCF Env" },
    PublicParameter { "vcfLfo", "VCF LFO" },
    PublicParameter { "keyFollow", "VCF Kybd" },
    PublicParameter { "vcaMode", "VCA Mode" },
    PublicParameter { "vcaLevel", "VCA Level" },
    PublicParameter { "attack", "Attack" },
    PublicParameter { "decay", "Decay" },
    PublicParameter { "sustain", "Sustain" },
    PublicParameter { "release", "Release" },
    PublicParameter { "chorus", "Chorus (legacy)" },
    PublicParameter { "transpose", "Transpose" },
    PublicParameter { "masterTune", "Master Tune" },
    PublicParameter { "velocity", "Velocity" },
    PublicParameter { "calibration", "Unit Character" },
    PublicParameter { "chorusNoise", "Chorus Noise" },
    PublicParameter { "polyphony", "Polyphony" },
    PublicParameter { "poly1", "Poly 1" },
    PublicParameter { "poly2", "Poly 2" },
    PublicParameter { "chorusI", "Chorus I" },
    PublicParameter { "chorusII", "Chorus II" },
    PublicParameter { "hq", "HQ" },
    PublicParameter { "quality", "Quality" },
    PublicParameter { "vcfTanhMode", "VCF Tanh" },
    PublicParameter { "vcfFastEarlyMode", "VCF Fast Early" },
    PublicParameter { "vcfSolverMode", "VCF Solver" },
}};

} // namespace youknow106::tests
