#include "DSP/SpiritNoise.h"

#include <algorithm>
#include <cmath>

namespace ghostar
{
namespace
{
constexpr double pi = 3.14159265358979323846264338327950288;

// Poles and zeros derived from P1013's printed component values:
// C17=1 uF; R4=200k; R5=10k; R6=18k/C8=220n;
// R7=3k/C9=100n; C10=10n; R8=27k; R11=1M;
// R12=100k/C11=15n. Frequencies are in Hz.
constexpr double couplingPoleHz = 0.5949159047558755;
constexpr double passivePoleOneHz = 31.038807109441645;
constexpr double passiveZeroOneHz = 40.190642194923065;
constexpr double passivePoleTwoHz = 179.27543930155693;
constexpr double passiveZeroTwoHz = 530.5164769729845;
constexpr double passiveLowpassHz = 8157.417554813907;
constexpr double activePoleHz = 9.645754126781537;
constexpr double activeZeroHz = 84.01548525077179;

constexpr double passiveGain = 0.7475933743539137;
constexpr double activeDcGain = 1027.0 / 27.0;
constexpr double passiveShelfOne = passivePoleOneHz / passiveZeroOneHz;
constexpr double passiveShelfTwo = passivePoleTwoHz / passiveZeroTwoHz;
constexpr double activeShelf = activePoleHz / activeZeroHz;
constexpr double redActiveGain = 1.0 + 100.0 / 2.2;

// MM5837 output swing and CEM3340 waveform volts have not yet been captured
// on the same Spirit. Keep that one unresolved calibration visible. 0.35
// preserves the previous engine's full-slider filter-path noise RMS while
// changing its sequence and spectrum to the documented circuit.
constexpr double sourceLevel = 0.35;
} // namespace

void SpiritNoise::prepare(double sampleRate) noexcept
{
    // Ghostar's lowest supported 8 kHz host runs this block on a 32 kHz
    // internal grid. Clamping the independently testable block to that same
    // floor also prevents an unbounded substep count for invalid tiny rates.
    if (!std::isfinite(sampleRate))
        sampleRate = 176400.0;
    sampleRate = std::max(sampleRate, 32000.0);
    // The plug-in accepts host rates down to 8 kHz. Keep this physical
    // source circuit above twice its 75 kHz clock even when the voice's 4x
    // grid is lower, then box-average the substeps back onto that grid.
    // Otherwise intervening PRBS bits are discarded and low-rate hosts gain
    // about 3 dB of spurious noise energy.
    substeps_ = std::max(1, static_cast<int>(
        std::ceil(2.0 * nominalClockHz / sampleRate)));
    sampleRate_ = sampleRate * static_cast<double>(substeps_);
    clockIncrement_ = nominalClockHz / sampleRate_;
    substepAverage_ = 1.0 / static_cast<double>(substeps_);

    const auto coefficient = [this](double poleHz) noexcept {
        const double g = std::tan(
            pi * std::min(poleHz, 0.45 * sampleRate_) / sampleRate_);
        // g is constant until prepare() is called again, so fold the TPT
        // denominator here instead of dividing six times per source sample.
        return g / (1.0 + g);
    };
    coefficient_[0] = coefficient(couplingPoleHz);
    coefficient_[1] = coefficient(passivePoleOneHz);
    coefficient_[2] = coefficient(passivePoleTwoHz);
    coefficient_[3] = coefficient(passiveLowpassHz);
    coefficient_[4] = coefficient(activePoleHz);
    // The RED NOISE branch is the R6/C8 junction, i.e. the audio passive
    // output through one more lowpass whose pole cancels that output's
    // 40.19 Hz zero, followed by IC4B's 1 + 100k/2k2 gain.
    coefficient_[5] = coefficient(passiveZeroOneHz);
    reset();
}

void SpiritNoise::reset() noexcept
{
    clockPhase_ = 0.0;
    // A real MM5837 powers up in a random non-zero state. A fixed non-zero
    // state keeps offline renders and regression tests reproducible without
    // changing the maximal sequence or its spectrum.
    lfsr_ = 0x1ffffu;
    heldBit_ = 1.0;
    state_.fill(0.0);
    redCircuitOutput_ = 0.0;
    redOutput_ = 0.0;
}

void SpiritNoise::advanceLfsr() noexcept
{
    // x^17 + x^14 + 1, with the oldest stages in bits 16 and 13.
    const std::uint32_t feedback = ((lfsr_ >> 16u) ^ (lfsr_ >> 13u)) & 1u;
    lfsr_ = ((lfsr_ << 1u) & 0x1ffffu) | feedback;
    heldBit_ = (lfsr_ & 1u) != 0u ? 1.0 : -1.0;
}

double SpiritNoise::onePoleLowpass(double input,
                                   std::size_t section) noexcept
{
    const double v = (input - state_[section]) * coefficient_[section];
    const double lowpass = v + state_[section];
    state_[section] = lowpass + v;
    return lowpass;
}

double SpiritNoise::processCircuit(double input) noexcept
{
    // Passive network. Each shelf is written as
    // high*x + (1-high)*LP_pole(x), the bilinear image of
    // (1+s/zero)/(1+s/pole). This avoids a fragile fifth-order direct form.
    double lowpass = onePoleLowpass(input, 0);
    double output = input - lowpass; // C17/R4 coupling highpass

    lowpass = onePoleLowpass(output, 1);
    output = passiveShelfOne * output
           + (1.0 - passiveShelfOne) * lowpass;

    lowpass = onePoleLowpass(output, 2);
    output = passiveShelfTwo * output
           + (1.0 - passiveShelfTwo) * lowpass;

    output = passiveGain * onePoleLowpass(output, 3);

    redCircuitOutput_ =
        redActiveGain * onePoleLowpass(output, 5);

    // IC4A: non-inverting 1458 with 27k to ground and
    // 1M || (100k + 15n) feedback, a 38.037-to-4.367 low shelf.
    lowpass = onePoleLowpass(output, 4);
    output = activeShelf * output + (1.0 - activeShelf) * lowpass;
    return activeDcGain * output;
}

double SpiritNoise::process() noexcept
{
    double audio = 0.0;
    double red = 0.0;
    for (int step = 0; step < substeps_; ++step)
    {
        clockPhase_ += clockIncrement_;
        while (clockPhase_ >= 1.0)
        {
            advanceLfsr();
            clockPhase_ -= 1.0;
        }
        audio += processCircuit(heldBit_);
        red += redCircuitOutput_;
    }
    redOutput_ = sourceLevel * red * substepAverage_;
    return sourceLevel * audio * substepAverage_;
}

} // namespace ghostar
