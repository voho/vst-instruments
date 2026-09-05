#include "../Source/DSP/YouKnowChorus.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace youknow
{
struct YouKnowTestAccess
{
    static void flushWetMuteDenormal(Chorus& chorus)
    {
        // The plug-in uses JUCE ScopedNoDenormals. Reproduce its wet-gain
        // flush portably without architecture-specific control-register code.
        if (std::abs(chorus.wetGain_) < std::numeric_limits<float>::min())
            chorus.wetGain_ = 0.0f;
    }
};
}

namespace
{
using youknow::Chorus;
using youknow::ChorusMode;

struct Interval
{
    ChorusMode mode;
    double seconds;
};

void checkControlContinuity(float rate, bool enableMuteDrive)
{
    Chorus exact;
    Chorus skipped;
    exact.prepare(rate);
    skipped.prepare(rate);

    // Long Off reaches the bypass optimization. The short intervals also
    // interrupt capacitor charging and discharging before either has settled.
    constexpr std::array intervals {
        Interval { ChorusMode::One, 0.02 },
        Interval { ChorusMode::Off, 1.0 },
        Interval { ChorusMode::One, 0.2 },
        Interval { ChorusMode::Off, 0.06 },
        Interval { ChorusMode::Two, 0.025 },
        Interval { ChorusMode::Off, 0.25 },
        Interval { ChorusMode::One, 0.035 },
        Interval { ChorusMode::Off, 0.15 },
        Interval { ChorusMode::Two, 0.2 }
    };
    int skippedSamples = 0;
    int reopened = 0;
    bool previouslyMuted = false;
    for (const auto interval : intervals)
    {
        bool mismatch = false;
        int exactOpening = -1;
        int skippedOpening = -1;
        const int frames = static_cast<int>(interval.seconds * rate);
        for (int frame = 0; frame < frames; ++frame)
        {
            float exactLeft {}, exactRight {}, skippedLeft {}, skippedRight {};
            const auto fullStep = [&](Chorus& chorus, float& left, float& right) {
                chorus.process(0.0f, interval.mode, 0.0f, left, right,
                               false, false, 1.0f, false, true,
                               enableMuteDrive, false);
                youknow::YouKnowTestAccess::flushWetMuteDenormal(chorus);
            };
            fullStep(exact, exactLeft, exactRight);
            if (interval.mode == ChorusMode::Off
                && skipped.processBypassedWhenSettled(
                    0.0f, skippedLeft, skippedRight))
                ++skippedSamples;
            else
                fullStep(skipped, skippedLeft, skippedRight);

            // The two wet audio histories are intentionally different under
            // the fast-mode policy. Tr4 must still switch on the same sample.
            mismatch |= exact.muteDriveMuted() != skipped.muteDriveMuted();
            if (!exact.muteDriveMuted() && exactOpening < 0)
                exactOpening = frame;
            if (!skipped.muteDriveMuted() && skippedOpening < 0)
                skippedOpening = frame;
            if (previouslyMuted && !exact.muteDriveMuted())
                ++reopened;
            previouslyMuted = exact.muteDriveMuted();
        }
        if (mismatch)
        {
            std::cerr << "Mute timing changed after settled bypass at "
                      << rate << " Hz: exact opened at "
                      << 1000.0 * exactOpening / rate << " ms; skipped at "
                      << 1000.0 * skippedOpening / rate << " ms\n";
            std::exit(1);
        }
    }
    if (skippedSamples == 0 || reopened < 2)
    {
        std::cerr << "Regression did not exercise bypass and re-engagement\n";
        std::exit(1);
    }
    std::cout << rate << " Hz, mute drive " << enableMuteDrive << ": "
              << skippedSamples << " skipped samples; " << reopened
              << " matching openings\n";
}
} // namespace

int main()
{
    for (const float rate : { 48000.0f, 192000.0f })
        for (const bool enabled : { false, true })
            checkControlContinuity(rate, enabled);
}
