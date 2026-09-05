#include "DSP/OutputHighPass.h"
#include "DSP/TaikoEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

namespace
{
int failures = 0;
constexpr double pi = 3.14159265358979323846;

void expect (bool passed, const char* message)
{
    if (! passed)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void checkResponse (double rate, float cutoff, double frequency)
{
    taikor::OutputHighPass filter;
    filter.setCutoff (cutoff);
    filter.prepare (rate);
    double inputEnergy = 0.0, outputEnergy = 0.0;
    const int settle = static_cast<int> (rate * (cutoff < 10.0f ? 3.0 : 0.25));
    for (int sample = 0; sample < settle + static_cast<int> (rate); ++sample)
    {
        float left = static_cast<float> (0.5 * std::sin (2.0 * pi * frequency * sample / rate));
        const float dry = left;
        float right = 0.0f;
        filter.process (left, right);
        expect (right == 0.0f, "left signal must never leak into the right channel");
        if (sample >= settle)
        {
            inputEnergy += static_cast<double> (dry) * dry;
            outputEnergy += static_cast<double> (left) * left;
        }
    }

    // The bilinear transform maps digital frequency to tan(pi*f/fs).
    // This independently checks the full response, including -3 dB at cutoff.
    const double ratio = std::tan (pi * frequency / rate) / std::tan (pi * cutoff / rate);
    const double expected = ratio / std::sqrt (1.0 + ratio * ratio);
    expect (std::abs (std::sqrt (outputEnergy / inputEnergy) - expected) < 2.0e-5,
            "measured gain must match a prewarped 6 dB/octave high-pass");
}

void checkTransitions (double rate)
{
    taikor::OutputHighPass filter;
    filter.prepare (rate);
    for (int sample = 0; sample < 1000; ++sample)
    {
        float left = 0.5f * std::sin (sample * 0.173f);
        float right = 0.3f * std::cos (sample * 0.319f);
        const float dryLeft = left, dryRight = right;
        filter.process (left, right);
        expect (left == dryLeft && right == dryRight,
                "Off must preserve the exact input samples");
    }

    float previous = 0.5f;
    for (const float cutoff : { 500.0f, 0.0f })
    {
        filter.setCutoff (cutoff);
        float largestStep = 0.0f;
        for (int sample = 0; sample < static_cast<int> (0.4 * rate); ++sample)
        {
            float left = 0.5f, right = -0.25f;
            filter.process (left, right);
            largestStep = std::max (largestStep, std::abs (left - previous));
            expect (right == -0.5f * left, "smoothing must preserve the stereo ratio");
            previous = left;
        }
        expect (largestStep < 0.005f, "enabling and bypassing must fade without a DC step");
        expect (cutoff > 0.0f ? std::abs (previous) < 1.0e-7f : previous == 0.5f,
                "enabled filter must reject DC; Off must return to exact bypass");
    }

    for (int sample = 0; sample < static_cast<int> (rate); ++sample)
    {
        if (sample % 73 == 0)
            filter.setCutoff ((sample / 73) % 2 == 0 ? 1.0f : 500.0f);
        float left = 0.5f * std::sin (static_cast<float> (2.0 * pi * 83.0 * sample / rate));
        float right = -left;
        filter.process (left, right);
        expect (std::isfinite (left) && std::abs (left) < 1.0f && right == -left,
                "rapid cutoff automation must stay finite, bounded and stereo coherent");
    }
    filter.reset();
    float left = 0.0f, right = 0.0f;
    filter.process (left, right);
    expect (left == 0.0f && right == 0.0f, "reset must clear the previous filter tail");

    for (const float invalid : { std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity() })
    {
        left = invalid; right = 0.25f;
        filter.process (left, right);
        expect (left == 0.0f && right == 0.0f, "invalid active input must be silenced");
        left = 0.5f;
        filter.process (left, right);
        expect (std::isfinite (left), "filter must recover after invalid input");
    }
}

void checkEnginePath()
{
    constexpr int blockSize = 256;
    constexpr double rate = 48000.0;
    auto dry = std::make_unique<taikor::TaikoEngine>();
    auto wet = std::make_unique<taikor::TaikoEngine>();
    taikor::EngineParameters parameters;
    parameters.humanise = 0.0f;
    parameters.drive = 0.6f;
    parameters.outputGain = 0.001f; // Keep the limiter below its threshold.
    for (const float cutoff : { 100.0f, 500.0f })
    {
        parameters.outputHighPassHz = 0.0f;
        dry->prepare (rate, blockSize);
        dry->setParameters (parameters);
        dry->reset();
        wet->prepare (rate, blockSize);
        wet->setParameters (parameters);
        wet->reset();
        // A change while idle must apply immediately to the next hit.
        parameters.outputHighPassHz = cutoff;
        wet->setParameters (parameters);
        dry->trigger (taikor::Articulation::Don, 0, 0.8f);
        wet->trigger (taikor::Articulation::Don, 0, 0.8f);
        taikor::OutputHighPass reference;
        reference.setCutoff (cutoff);
        reference.prepare (rate);
        double dryEnergy = 0.0, wetEnergy = 0.0;
        float largestError = 0.0f;
        for (int block = 0; block < 100; ++block)
        {
            std::array<float, blockSize> dryLeft {}, dryRight {}, wetLeft {}, wetRight {};
            dry->process (dryLeft.data(), dryRight.data(), blockSize);
            wet->process (wetLeft.data(), wetRight.data(), blockSize);
            for (int sample = 0; sample < blockSize; ++sample)
            {
                dryEnergy += static_cast<double> (dryLeft[sample]) * dryLeft[sample];
                wetEnergy += static_cast<double> (wetLeft[sample]) * wetLeft[sample];
                reference.process (dryLeft[sample], dryRight[sample]);
                largestError = std::max ({ largestError,
                    std::abs (dryLeft[sample] - wetLeft[sample]),
                    std::abs (dryRight[sample] - wetRight[sample]) });
            }
        }
        expect (largestError < 1.0e-7f,
                "engine output must match filtering the driven signal, in both channels");
        std::cout << "Engine Low Cut " << cutoff << " Hz: energy ratio "
                  << wetEnergy / dryEnergy << ", reference error " << largestError << '\n';
        expect (dryEnergy > 1.0e-6 && wetEnergy < dryEnergy,
                "Low Cut must audibly attenuate the low drum in the real engine path");

        wet->allSoundsOff();
        std::array<float, blockSize> left {}, right {};
        wet->process (left.data(), right.data(), blockSize);
        expect (std::all_of (left.begin(), left.end(), [] (float x) { return x == 0.0f; })
                && std::all_of (right.begin(), right.end(), [] (float x) { return x == 0.0f; }),
                "Panic must clear the output filter tail immediately");
    }

    parameters.outputGain = 2.0f;
    parameters.outputHighPassHz = 500.0f;
    parameters.stereoWidth = 1.0f;
    for (const float drive : { 0.0f, 1.0f })
    {
        parameters.drive = drive;
        wet->setParameters (parameters);
        wet->reset();
        for (int drum = 0; drum < taikor::drumCount; ++drum)
            for (int stroke = 0; stroke < static_cast<int> (taikor::articulationCount); ++stroke)
                wet->trigger (static_cast<taikor::Articulation> (stroke), drum, 1.0f);
        float peak = 0.0f;
        for (int block = 0; block < 40; ++block)
        {
            std::array<float, blockSize> left {}, right {};
            wet->process (left.data(), right.data(), blockSize);
            for (int sample = 0; sample < blockSize; ++sample)
            {
                expect (std::isfinite (left[sample]) && std::isfinite (right[sample]),
                        "high-pass overload must remain finite");
                peak = std::max ({ peak, std::abs (left[sample]), std::abs (right[sample]) });
            }
        }
        expect (peak > 0.01f && peak <= taikor::OutputLimiter::ceiling,
                "final limiter must protect filtered overload with and without Drive");
    }
}
}

int main()
{
    for (const double rate : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (const float cutoff : { 40.0f, 100.0f, 500.0f })
            for (const double multiple : { 0.25, 1.0, 4.0 })
                checkResponse (rate, cutoff, cutoff * multiple);
        checkResponse (rate, 1.0f, 1.0);
        checkTransitions (rate);
    }
    checkEnginePath();
    if (failures == 0)
        std::cout << "Output high-pass checks passed\n";
    return failures == 0 ? 0 : 1;
}
