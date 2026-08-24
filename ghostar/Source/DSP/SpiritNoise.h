// The Spirit's complete P1013 audio-noise source: an MM5837 clocked
// 17-stage maximal PRBS followed by the coupling, passive colouring and
// 1458 feedback networks drawn in the service manual. Kept as a small
// block of its own so the circuit response and the source period can be
// tested directly instead of inferred through the rest of the voice.
#pragma once

#include <array>
#include <cstdint>

namespace ghostar
{

class SpiritNoise
{
public:
    // The MM5837 datasheet gives a 1.1--2.4 s cycle but no typical part.
    // 75 kHz is the midpoint-cycle nominal (131071 / 75000 = 1.748 s),
    // retained as one explicit per-unit voicing until a Spirit is captured.
    static constexpr double nominalClockHz = 75000.0;
    static constexpr std::uint32_t sequenceLength = 131071u;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Advance the physical source and return the coloured audio output.
    [[nodiscard]] double process() noexcept;

    // Run one sample of only the resolved P1013 linear network. At low
    // requested rates prepare() raises the physical circuit grid for source
    // substepping; circuitSampleRate() reports the grid this seam advances.
    [[nodiscard]] double processCircuit(double input) noexcept;
    [[nodiscard]] double circuitSampleRate() const noexcept
    {
        return sampleRate_;
    }

    // The raw held MM5837 bit is available for the period test. red() is
    // P1013's separate R6/C8 -> IC4B modulation output at the same source
    // scale as process(); only its later conversion to the X bus is voiced.
    [[nodiscard]] double heldBit() const noexcept { return heldBit_; }
    [[nodiscard]] double red() const noexcept { return redOutput_; }
    [[nodiscard]] double redCircuitOutput() const noexcept
    {
        return redCircuitOutput_;
    }

private:
    [[nodiscard]] double onePoleLowpass(double input,
                                        std::size_t section) noexcept;
    void advanceLfsr() noexcept;

    double sampleRate_ { 176400.0 };
    int substeps_ { 1 };
    double clockIncrement_ { nominalClockHz / 176400.0 };
    double substepAverage_ { 1.0 };
    double clockPhase_ { 0.0 };
    std::uint32_t lfsr_ { 0x1ffffu };
    double heldBit_ { 1.0 };

    std::array<double, 6> coefficient_ {};
    std::array<double, 6> state_ {};
    double redCircuitOutput_ { 0.0 };
    double redOutput_ { 0.0 };
};

} // namespace ghostar
