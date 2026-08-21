// Ghost: a circuit-modelled monophonic analog synthesizer, built block by
// block from documentation of a 1983 dual-filter mono synth. This header is
// the engine's public surface; the modelling contract and its evidence live
// in Docs/circuit-modelling-research.md.
//
// SCAFFOLD STATUS: the public API below is stable, but the parameter set and
// the voice behind it are the pre-research skeleton — a single oscillator,
// one four-pole lowpass and one ADSR — kept only so the subproject builds and
// its tools run end to end while the researched panel model lands. Nothing in
// the skeleton voice is a hardware claim yet.
#pragma once

#include <array>
#include <cstdint>

namespace ghost
{

struct EngineParameters
{
    // Panel travel is 0..1 throughout: the engine maps each control through
    // the modelled hardware law (for the skeleton, through a provisional law
    // documented at the mapping site) rather than storing pre-cooked seconds
    // or hertz.

    // --- VCF ---------------------------------------------------------------
    float cutoff { 0.62f };
    float resonance { 0.10f };
    float envToCutoff { 0.35f };

    // --- Contour -----------------------------------------------------------
    float attack { 0.02f };
    float decay { 0.45f };
    float sustain { 0.70f };
    float release { 0.30f };

    // --- Output ------------------------------------------------------------
    float volume { 0.80f };
};

class GhostEngine
{
public:
    GhostEngine() noexcept;

    // The host rates prepare() will run at. Anything outside this range is
    // clamped into it rather than reaching the internal grid.
    static constexpr double minimumSupportedSampleRate = 8000.0;
    static constexpr double maximumSupportedSampleRate = 768000.0;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void setPitchBend(float normalisedBipolar) noexcept;
    void setModWheel(float amount) noexcept;
    void process(float* left, float* right, int numSamples);

    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] bool isGateOpen() const noexcept { return gateOpen_; }

private:
    struct Envelope
    {
        enum class Stage { Idle, Attack, Decay, Release };
        Stage stage { Stage::Idle };
        float level { 0.0f };
    };

    struct Ladder
    {
        std::array<float, 4> state {};
    };

    EngineParameters parameters_ {};
    double sampleRate_ { 44100.0 };

    // Held-key memory: a released newer key falls back to the newest key that
    // is still down, as a hardware mono keyboard's key scanner does, and it
    // falls back at that key's own strike velocity, not the newer key's. The
    // capacity covers the whole MIDI note domain — each note occupies at most
    // one slot, so no held key can ever be discarded.
    struct HeldKey
    {
        std::int16_t note { 0 };
        float velocity { 0.0f };
    };
    static constexpr int keyStackCapacity = 128;
    std::array<HeldKey, keyStackCapacity> keyStack_ {};
    int keyStackSize_ { 0 };

    bool gateOpen_ { false };
    int currentNote_ { -1 };
    float velocity_ { 0.0f };
    float pitchBend_ { 0.0f };
    float modWheel_ { 0.0f };

    double oscPhase_ { 0.0 };
    Envelope envelope_ {};
    Ladder ladder_ {};
};

} // namespace ghost
