// Export the prepared mechanical coefficients for PrototypeModalPreload.py.
// Standalone analysis only: includes the engine implementation for friend access.
// From the repository root:
//   mkdir -p build
//   c++ -std=c++20 -O2 -I Source Tools/ModalPreloadProbe.cpp -o build/ModalPreloadProbe
//   shasum -a 256 Tools/ModalPreloadProbe.cpp Source/DSP/AcustraEngine.cpp \
//     Source/DSP/AcustraEngine.h Source/DSP/FittedPhysicalData.h \
//     Source/DSP/MeasuredBodyData.h Source/DSP/MeasuredBridgeData.h \
//     Source/DSP/MeasuredSteelBridgeData.h > build/modal-preload-source-hashes.txt
//   build/ModalPreloadProbe > build/modal-preload-coefficients.jsonl
//   python3 Tools/PrototypeModalPreload.py \
//     --coefficients build/modal-preload-coefficients.jsonl \
//     --output build/modal-preload-analysis
// Preserve the hash record with the export. The Python tool's workspace hashes
// are observation context, not proof of the supplied coefficients' origin.

#include "DSP/AcustraEngine.cpp"

#include <iomanip>
#include <iostream>
#include <memory>

namespace acustra
{
struct AcustraEngineTestAccess
{
    static void exportCoefficients()
    {
        constexpr int rate = 48000;
        for (int variant = 0; variant < 2; ++variant)
        {
            auto engine = std::make_unique<AcustraEngine>();
            EngineParameters parameters;
            parameters.stringMaterial = variant == 0
                ? StringMaterial::Nylon : StringMaterial::Steel;
            parameters.bridgeModel = BridgeModel::Original;
            engine->setParameters(parameters);
            engine->prepare(rate, 64);

            float stiffness0, stiffness1, stiffness2;
            engine->bridgeAnchorMoments(stiffness0, stiffness1, stiffness2);
            std::cout << "{\"kind\":\"configuration\",\"rate\":" << rate
                      << ",\"variant\":" << variant << ",\"K\":["
                      << stiffness0 << ',' << stiffness1 << ',' << stiffness2
                      << "]}\n";

            const auto bank = measuredBridgeBank(parameters.stringMaterial,
                                                 parameters.bridgeModel);
            const auto plate = plateConductanceMode(engine->physicalCalibration_);
            const auto& bridge = engine->bridgeLoad_;
            for (std::size_t index = 0; index < bridge.heaveModes.size(); ++index)
            {
                const float heave = bridge.residueHeave[index];
                const float cross = bridge.residueCross[index];
                const float rock = bridge.residueRock[index];
                if (heave == 0.0f && cross == 0.0f && rock == 0.0f)
                    continue;

                const bool measured = index
                    < static_cast<std::size_t>(AcustraEngine::bridgeModeCount);
                const float frequency = measured ? bank[index].frequency
                                                 : plate.frequency;
                const float q = measured ? bank[index].q : plate.q;
                // Retain the native float prewarp and its operation order.
                const float fs = static_cast<float>(rate);
                const float bilinear = 2.0f * fs;
                const float omega = bilinear * std::tan(pi * frequency / fs);
                std::cout << "{\"kind\":\"bridge_mode\",\"rate\":" << rate
                          << ",\"variant\":" << variant << ",\"mode\":" << index
                          << ",\"omega_prewarped\":" << omega << ",\"q\":" << q
                          << ",\"R\":[" << heave << ',' << cross << ',' << rock
                          << "]}\n";
            }

            // Only the low-E normal polarization speaks in this reference.
            // bridgeAnchorMoments above still includes all six fixed tails.
            const auto& voice = engine->voices_[0];
            const auto& loop = voice.loops[0];
            const double length = variant == 0 ? double(0.650f) : double(0.648f);
            std::cout << "{\"kind\":\"string\",\"rate\":" << rate
                      << ",\"variant\":" << variant << ",\"string\":0"
                      << ",\"gain\":" << loop.loopGain
                      << ",\"Z\":" << double(voice.characteristicImpedance)
                      << ",\"T\":" << voice.tensionNewtons
                      << ",\"L\":" << length
                      << ",\"arm\":" << saddleLeverArm(0)
                      << ",\"B\":" << voice.dispersionDesignInharmonicity
                      << ",\"broad_coefficient\":" << loop.broadLossCoefficient
                      << ",\"broad_mix\":" << loop.broadLossMix
                      << ",\"high_coefficient\":" << loop.lowpassCoefficient
                      << ",\"high_mix\":" << loop.highLossMix << "}\n";
        }
    }
};
}

int main()
{
    std::cout << std::setprecision(17);
    acustra::AcustraEngineTestAccess::exportCoefficients();
    return std::cout ? 0 : 1;
}
