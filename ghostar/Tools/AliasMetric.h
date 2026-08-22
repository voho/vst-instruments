// The alias audit's measuring instrument, kept apart from the strokes it
// measures so it can be tested on signals whose alias content is known
// exactly. Header-only, JUCE-free and engine-free: the test that exercises
// it links nothing.
//
// WHAT IS BEING MEASURED. Aliasing is content the shipping render has that
// the ground truth does not. Comparing two short-time magnitude spectra
// would say exactly that, except for one nuisance: the shipping render and
// a reference rendered at a different rate put their partials at very
// slightly different frequencies, and a partial displaced by a fraction of
// a bin moves its whole leakage skirt. A plain per-bin difference reads
// that skirt as added content and buries the alias energy under it.
//
// HOW THE FIRST ATTEMPT GOT IT WRONG, because the lesson is the design.
// The first version tolerated the displacement by comparing each bin
// against the largest reference bin within +/-3 bins, times +1 dB. That
// turns every partial into a 70 Hz-wide plateau of permission: a bin whose
// own reference is empty inherits its neighbour's magnitude as its ceiling,
// so it can hold an added component nearly as loud as that partial and
// still contribute nothing. Measured on the real strokes, an injected alias
// twenty dB ABOVE the acceptance gate reported the metric's -200 dB floor.
// The comment that justified it — that alias images land far from the
// partials that produce them — is false: a fold family sits at |k*f0 - n*fs|,
// which for any f0 that is not a neat fraction of fs lands tens of Hz from
// the harmonic grid, and hard sync, ring modulation and a driven nonlinearity
// make the grid dense enough that "tens of Hz away" means "on top of
// something else".
//
// WHAT IT DOES INSTEAD. The disagreement being tolerated is a small
// difference in PITCH, and a pitch difference displaces the k-th partial by
// k times as much as the first. So the tolerance is proportional to
// frequency, not a fixed number of bins: a bin's ceiling is taken over the
// span that a relative frequency error of `drift` could have moved content
// into. Near the bottom of the spectrum that is a bin or two; at 15 kHz it
// is wider, because that is where the displacement really is wider. And
// `drift` is not chosen — it is measured, as the largest relative frequency
// offset between the two spectra's matched peaks.
//
// AND IT PUBLISHES ITS OWN BLINDNESS, which is the part the first version
// most needed. Any ceiling can hide something beneath it, and one case can
// never be resolved at all: a component landing exactly on a partial is
// indistinguishable from that partial being slightly louder, by this or any
// other magnitude comparison. `blindDb` is the loudest component the
// measurement could have missed anywhere — the ceiling's own headroom over
// the reference. An excess figure means nothing without it, so the two are
// always reported together, and a row whose blind floor sits above the
// acceptance gate is undecidable by this method rather than passing it.
//
// WHAT IT STILL CANNOT DO. The blind floor on ordinary tonal material sits
// around -15 dB, because the +1 dB level tolerance alone leaves that much
// room under a partial. So this method cannot certify a -60 dB
// alias-to-signal gate on a tonal stroke, and does not claim to. A
// reference-free measure would — for a stroke that holds a pitch,
// everything off the harmonic grid is alias and noise, with no reference to
// disagree with and no tolerance to hide behind. That is recorded as an open
// question rather than shipped here: a first attempt reported a stroke's own
// inharmonicity as though it were alias, and half a measurement is what
// produced the defect this file exists to correct.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ghostar::aliasmetric
{
inline constexpr double pi = 3.14159265358979323846;

// In-place radix-2 complex FFT (size a power of two).
inline void fft(std::vector<double>& real, std::vector<double>& imag)
{
    const std::size_t n = real.size();
    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }
    for (std::size_t length = 2; length <= n; length <<= 1)
    {
        const double angle = -2.0 * pi / static_cast<double>(length);
        const double wRe = std::cos(angle);
        const double wIm = std::sin(angle);
        for (std::size_t start = 0; start < n; start += length)
        {
            double curRe = 1.0;
            double curIm = 0.0;
            for (std::size_t k = 0; k < length / 2; ++k)
            {
                const std::size_t even = start + k;
                const std::size_t odd = start + k + length / 2;
                const double tRe = real[odd] * curRe - imag[odd] * curIm;
                const double tIm = real[odd] * curIm + imag[odd] * curRe;
                real[odd] = real[even] - tRe;
                imag[odd] = imag[even] - tIm;
                real[even] += tRe;
                imag[even] += tIm;
                const double nextRe = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = nextRe;
            }
        }
    }
}

struct Residual
{
    double excessDb;    // added content only, <=20 kHz
    double blindDb;     // the loudest component this measurement could hide
    double audibleDb;   // total magnitude difference <=20 kHz
    double fullDb;      // total magnitude difference, whole baseband
    double drift;       // the measured relative pitch disagreement allowed
};

// A partial whose level differs by less than this is the same partial, not
// added content: one dB covers the second-decimal-place level agreement
// measured on rate-convergent self-oscillating tones. It is also what makes
// a component sitting on a partial invisible, which is why blindDb exists.
inline constexpr double excessTolerance = 1.1220184543019633; // +1 dB
// A relative frequency disagreement larger than this is a detuning to
// report, not a skirt to forgive: a tenth of a percent is already two cents.
inline constexpr double maximumDrift = 1.0e-3;
// Blackman-Harris puts a peak's energy inside about this many bins either
// side, so the ceiling is never narrower than the window's own main lobe.
inline constexpr int mainLobeBins = 4;

namespace detail
{
// Blackman-Harris: a -92 dB sidelobe floor, so the window's own leakage
// cannot be mistaken for the content being measured.
inline void blackmanHarris(std::vector<double>& window)
{
    const std::size_t n = window.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const double x = 2.0 * pi * static_cast<double>(i)
                       / static_cast<double>(n);
        window[i] = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2.0 * x)
                  - 0.01168 * std::cos(3.0 * x);
    }
}

// The sub-bin position of a peak, by parabolic interpolation on the three
// magnitudes around it.
[[nodiscard]] inline double peakOffset(double left, double centre,
                                       double right) noexcept
{
    const double denominator = left - 2.0 * centre + right;
    if (std::abs(denominator) < 1.0e-300)
        return 0.0;
    return std::clamp(0.5 * (left - right) / denominator, -0.5, 0.5);
}

// Magnitudes of one windowed frame.
inline void frameMagnitude(const std::vector<double>& windowed,
                           std::vector<double>& magnitude,
                           std::vector<double>& re, std::vector<double>& im)
{
    const std::size_t n = windowed.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        re[i] = windowed[i];
        im[i] = 0.0;
    }
    fft(re, im);
    for (std::size_t bin = 0; bin <= n / 2; ++bin)
        magnitude[bin] = std::sqrt(re[bin] * re[bin] + im[bin] * im[bin]);
}

// The largest reference magnitude that a relative frequency error of
// `drift` could have placed at `bin` — the span widening with frequency,
// because that is how a pitch difference moves partials, and never
// narrower than the analysis window's own main lobe.
[[nodiscard]] inline double ceilingAt(const std::vector<double>& refMag,
                                      std::size_t bin, std::size_t topBin,
                                      double drift) noexcept
{
    const double reach = static_cast<double>(bin) * drift
                       + static_cast<double>(mainLobeBins);
    const std::size_t span = static_cast<std::size_t>(std::ceil(reach));
    const std::size_t low = bin < span ? 0 : bin - span;
    const std::size_t high = std::min(topBin, bin + span);
    double largest = 0.0;
    for (std::size_t near = low; near <= high; ++near)
        largest = std::max(largest, refMag[near]);
    return largest;
}
} // namespace detail

// `sampleRate` is the rate both signals are at; they are compared frame by
// frame from `skipSeconds` in, so a settling smoother is not measured
// against its own onset.
[[nodiscard]] inline Residual
spectralResidualDb(const std::vector<float>& reference,
                   const std::vector<float>& shipping, double sampleRate,
                   double skipSeconds = 0.3, std::size_t window = 16384)
{
    const std::size_t hop = window / 2;
    const std::size_t skip = static_cast<std::size_t>(skipSeconds * sampleRate);
    const std::size_t audibleBins = std::min(
        window / 2,
        static_cast<std::size_t>(20000.0 * static_cast<double>(window)
                                 / sampleRate));

    std::vector<double> win(window);
    detail::blackmanHarris(win);

    const std::size_t usable = std::min(reference.size(), shipping.size());
    if (usable < skip + window)
        return { 0.0, 0.0, 0.0, 0.0, 0.0 };

    std::vector<double> refWindowed(window), shipWindowed(window);
    std::vector<double> re(window), im(window);
    std::vector<double> refMag(window / 2 + 1), shipMag(window / 2 + 1);
    std::vector<double> refMinus(window / 2 + 1), refPlus(window / 2 + 1);

    const auto loadFrame = [&](const std::vector<float>& source,
                               std::size_t start,
                               std::vector<double>& windowed) {
        for (std::size_t i = 0; i < window; ++i)
            windowed[i] = win[i] * static_cast<double>(source[start + i]);
    };

    // ---- Pass one: how far apart do the two spectra actually put a partial?
    // The tolerance the ceiling grants is this measurement, not a choice.
    //
    // The median across matched peaks, not the worst of them. A pitch
    // disagreement moves every partial alike, so the middle of the
    // distribution is the measurement; the tail is whatever added content
    // has skewed. Taking the worst would let a loud alias inflate the very
    // tolerance that then hides it.
    std::vector<double> offsets;
    for (std::size_t start = skip; start + window <= usable; start += hop)
    {
        loadFrame(reference, start, refWindowed);
        loadFrame(shipping, start, shipWindowed);
        detail::frameMagnitude(refWindowed, refMag, re, im);
        detail::frameMagnitude(shipWindowed, shipMag, re, im);

        double framePeak = 0.0;
        for (std::size_t bin = 0; bin <= audibleBins; ++bin)
            framePeak = std::max(framePeak, refMag[bin]);
        if (framePeak <= 0.0)
            continue;
        // Only peaks worth matching: 60 dB down from the frame's loudest is
        // where a displacement still moves meaningful energy.
        const double floor = framePeak * 1.0e-3;
        for (std::size_t bin = 1; bin + 1 <= audibleBins; ++bin)
        {
            if (refMag[bin] < floor || refMag[bin] <= refMag[bin - 1]
                || refMag[bin] < refMag[bin + 1])
                continue;
            if (shipMag[bin] <= shipMag[bin - 1]
                || shipMag[bin] < shipMag[bin + 1])
                continue;   // the shipping render has no peak to match here
            const double refOffset =
                detail::peakOffset(refMag[bin - 1], refMag[bin], refMag[bin + 1]);
            const double shipOffset = detail::peakOffset(
                shipMag[bin - 1], shipMag[bin], shipMag[bin + 1]);
            // Relative, not absolute: a pitch disagreement moves the k-th
            // partial k times as far, so what the two renders disagree
            // about is a ratio.
            const double centre = static_cast<double>(bin) + refOffset;
            if (centre > 0.0)
                offsets.push_back(std::abs(shipOffset - refOffset) / centre);
        }
    }
    double drift = 0.0;
    if (!offsets.empty())
    {
        const std::size_t middle = offsets.size() / 2;
        std::nth_element(offsets.begin(), offsets.begin() + middle,
                         offsets.end());
        drift = offsets[middle];
    }
    drift = std::min(drift, maximumDrift);

    // ---- Pass two: the comparison itself.
    double audibleExcess = 0.0;
    double audibleHideable = 0.0;
    double audibleResidual = 0.0;
    double audibleSignal = 0.0;
    double fullResidual = 0.0;
    double fullSignal = 0.0;
    for (std::size_t start = skip; start + window <= usable; start += hop)
    {
        loadFrame(reference, start, refWindowed);
        loadFrame(shipping, start, shipWindowed);
        detail::frameMagnitude(refWindowed, refMag, re, im);
        detail::frameMagnitude(shipWindowed, shipMag, re, im);

        for (std::size_t bin = 0; bin <= window / 2; ++bin)
        {
            const double difference = shipMag[bin] - refMag[bin];
            fullResidual += difference * difference;
            fullSignal += refMag[bin] * refMag[bin];
            if (bin > audibleBins)
                continue;
            audibleResidual += difference * difference;
            audibleSignal += refMag[bin] * refMag[bin];

            // The ceiling: the loudest the reference could have put here if
            // its partials were displaced by up to `drift` in frequency.
            const double ceiling =
                excessTolerance
                * detail::ceilingAt(refMag, bin, audibleBins, drift);
            const double excess = std::max(0.0, shipMag[bin] - ceiling);
            audibleExcess += excess * excess;

            // …and what that ceiling could hide: the room between this
            // bin's own reference and it. Counted everywhere, the bins
            // under a partial included — a component landing on a partial
            // is exactly the case this measurement cannot resolve, so it is
            // the case the blind floor most needs to cover.
            const double hideable = std::max(0.0, ceiling - refMag[bin]);
            audibleHideable = std::max(audibleHideable, hideable * hideable);
        }
    }
    if (audibleSignal <= 0.0 || fullSignal <= 0.0)
        return { 0.0, 0.0, 0.0, 0.0, drift };
    return { 10.0 * std::log10(audibleExcess / audibleSignal + 1.0e-20),
             10.0 * std::log10(audibleHideable / audibleSignal + 1.0e-20),
             10.0 * std::log10(audibleResidual / audibleSignal + 1.0e-20),
             10.0 * std::log10(fullResidual / fullSignal + 1.0e-20),
             drift };
}
} // namespace ghostar::aliasmetric
