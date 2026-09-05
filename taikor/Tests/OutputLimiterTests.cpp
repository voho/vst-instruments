#include "DSP/OutputLimiter.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
int failures = 0;

void expect (bool passed, const char* message)
{
    if (! passed)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main()
{
    using taikor::OutputLimiter;
    for (const double rate : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        OutputLimiter limiter;
        limiter.prepare (rate);

        // Ordinary hits keep their samples, phase and channel balance exactly.
        for (int sample = 0; sample < 1000; ++sample)
        {
            const float dryLeft = 0.8f * std::sin (0.173f * sample);
            const float dryRight = 0.5f * std::cos (0.317f * sample);
            float left = dryLeft, right = dryRight;
            limiter.process (left, right);
            expect (left == dryLeft && right == dryRight,
                    "below-threshold audio must pass unchanged without latency");
        }

        float left = 8.0f, right = -2.0f;
        limiter.process (left, right);
        expect (left == OutputLimiter::ceiling && right == -0.25f * left,
                "first overloaded sample must be caught with linked stereo gain");

        double previous = 0.0;
        double gainAfter80ms = 0.0;
        const int holdSamples = static_cast<int> (std::ceil (0.001 * rate));
        for (int sample = 0; sample < static_cast<int> (rate); ++sample)
        {
            left = 0.25f;
            right = -0.125f;
            limiter.process (left, right);
            const double gain = left / 0.25;
            expect (gain >= previous && gain <= 1.0,
                    "gain must recover monotonically without overshoot");
            expect (right == -0.5f * left, "release must preserve stereo ratio");
            if (sample < holdSamples)
                expect (std::abs (gain - OutputLimiter::ceiling / 8.0) < 1.0e-7,
                        "gain hold must last one millisecond at every sample rate");
            if (sample == holdSamples + static_cast<int> (0.080 * rate) - 1)
                gainAfter80ms = gain;
            previous = gain;
        }
        const double expectedRelease = 1.0 - (1.0 - OutputLimiter::ceiling / 8.0)
                                             * std::exp (-1.0);
        expect (std::abs (gainAfter80ms - expectedRelease) < 1.0e-6,
                "80 ms gain recovery must be sample-rate independent");
        expect (previous > 0.99999, "limiter must recover after an overload");

        // Both channels, polarities, changing peaks and sustained overload.
        for (int sample = 0; sample < 12000; ++sample)
        {
            left = 100.0f * std::sin (sample * 0.731f);
            right = 150.0f * std::cos (sample * 0.179f);
            const float dryLeft = left, dryRight = right;
            limiter.process (left, right);
            expect (std::isfinite (left) && std::isfinite (right)
                    && std::max (std::abs (left), std::abs (right))
                           <= OutputLimiter::ceiling,
                    "arbitrary overload must stay finite and below the ceiling");
            expect (std::abs (left * dryRight - right * dryLeft) < 3.0e-5f,
                    "both channels must receive the same attenuation");
        }

        limiter.reset();
        left = 0.2f; right = -0.1f;
        limiter.process (left, right);
        expect (left == 0.2f && right == -0.1f,
                "reset must remove reduction left by a preceding overload");
        for (const float invalid : { std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::quiet_NaN() })
        {
            left = invalid; right = 0.25f;
            limiter.process (left, right);
            expect (left == 0.0f && right == 0.0f,
                    "invalid frame must be silent and cannot poison future output");
        }
        left = std::numeric_limits<float>::max();
        right = -left;
        limiter.process (left, right);
        expect (left == OutputLimiter::ceiling && right == -left,
                "even the largest finite input must remain bounded");
        left = right = 0.0f;
        limiter.process (left, right);
        expect (left == 0.0f && right == 0.0f, "limiter must never invent a tail");
    }

    if (failures == 0)
        std::cout << "Output limiter checks passed\n";
    return failures == 0 ? 0 : 1;
}
