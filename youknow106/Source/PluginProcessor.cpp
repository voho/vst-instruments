#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
using namespace youknow106;

constexpr auto stateSchemaVersionProperty = "stateSchemaVersion";
constexpr int currentStateSchemaVersion = 3;
constexpr int calibrationDefaultSchemaVersion = 1;
constexpr int originalFactoryBankSchemaVersion = 3;
constexpr float legacyCalibrationDefault = 0.35f;

// JUCE's AudioParameterBool deliberately retains fractional normalised values
// written by a host. The switch reads correctly, but that fractional value can
// remain visible to the host after a state restore when the logical value did
// not change. A two-step integer parameter has the same public contract and
// snaps every incoming value before storing it.
class SnappingBoolParameter final : public juce::AudioParameterInt
{
public:
    SnappingBoolParameter (
        const juce::ParameterID& id, const juce::String& parameterName,
        bool defaultValue,
        const juce::AudioParameterIntAttributes& attributes)
        : juce::AudioParameterInt (id, parameterName, 0, 1,
                                   defaultValue ? 1 : 0, attributes)
    {
    }

private:
    bool isDiscrete() const override { return true; }
    bool isBoolean() const override { return true; }
};

juce::AudioParameterIntAttributes boolAttributes()
{
    return juce::AudioParameterIntAttributes()
        .withStringFromValueFunction (
            [] (int enabled, int) { return enabled != 0 ? "On" : "Off"; })
        .withValueFromStringFunction (
            [] (const juce::String& text)
            {
                const auto value = text.trim().toLowerCase();
                if (value == "on" || value == "yes" || value == "true")
                    return 1;
                if (value == "off" || value == "no" || value == "false")
                    return 0;
                return text.getIntValue() != 0 ? 1 : 0;
            });
}

float migrateLegacyVolumePosition (float legacyPosition) noexcept
{
    // Schema 1 applied x^2 after the unloaded C17/R54/VR1 boundary. Schema 2
    // uses the nominal-linear track as part of the fixed loaded network. Map a
    // saved shaft value to the new position with the closest former static
    // attenuation; the small top-end excess of the unloaded law saturates at
    // the real loaded full-travel value. Host-owned automation lanes cannot be
    // rewritten here, but self-contained plug-in state remains level-stable.
    const float legacy = std::clamp (legacyPosition, 0.0f, 1.0f);
    const float target = YouKnow106Engine::outputCouplingHighGain()
                       * legacy * legacy;
    if (target <= 0.0f)
        return 0.0f;
    if (target >= YouKnow106Engine::outputCouplingHighGain (1.0f))
        return 1.0f;

    float lower = 0.0f;
    float upper = 1.0f;
    for (int iteration = 0; iteration < 24; ++iteration)
    {
        const float middle = 0.5f * (lower + upper);
        if (YouKnow106Engine::outputCouplingHighGain (middle) < target)
            lower = middle;
        else
            upper = middle;
    }
    return 0.5f * (lower + upper);
}

juce::String percentText (float value, int)
{
    return juce::String (juce::roundToInt (value * 100.0f)) + "%";
}

float percentValue (const juce::String& text)
{
    return text.retainCharacters ("0123456789.-").getFloatValue() / 100.0f;
}

juce::AudioParameterFloatAttributes percentAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("%")
        .withStringFromValueFunction (percentText)
        .withValueFromStringFunction (percentValue);
}

juce::String centsText (float value, int)
{
    const auto sign = value > 0.0f ? "+" : "";
    return sign + juce::String (value, 1) + " ct";
}

float centsValue (const juce::String& text)
{
    return text.retainCharacters ("0123456789.-+").getFloatValue();
}

juce::AudioParameterFloatAttributes centsAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("ct")
        .withStringFromValueFunction (centsText)
        .withValueFromStringFunction (centsValue);
}

juce::String secondsText (float value, int)
{
    if (value < 1.0f)
        return juce::String (juce::roundToInt (value * 1000.0f)) + " ms";
    return juce::String (value, value < 10.0f ? 2 : 1) + " s";
}

// A control whose *display* is a time or a frequency needs a way back from the
// text a host may let someone type. Without it, "1.00 kHz" would be read as a
// panel position of 1.0 and drive the control to its top.
float secondsFromText (const juce::String& text)
{
    const auto number = text.retainCharacters ("0123456789.-").getFloatValue();
    return text.containsIgnoreCase ("ms") ? number / 1000.0f : number;
}

float hertzFromText (const juce::String& text)
{
    const auto number = text.retainCharacters ("0123456789.-").getFloatValue();
    return text.containsIgnoreCase ("k") ? number * 1000.0f : number;
}

// A panel position that stands for a time is shown as the time the modelled
// circuit produces, not as a percentage of travel.
juce::AudioParameterFloatAttributes attackAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            return secondsText (YouKnow106Engine::envelopeAttackSeconds (value), 0);
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForAttack (secondsFromText (text));
        });
}

juce::AudioParameterFloatAttributes decayAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            return secondsText (YouKnow106Engine::envelopeDecaySeconds (value), 0);
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForDecay (secondsFromText (text));
        });
}

juce::AudioParameterFloatAttributes releaseAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            return secondsText (YouKnow106Engine::envelopeReleaseSeconds (value), 0);
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForRelease (secondsFromText (text));
        });
}

juce::AudioParameterFloatAttributes lfoRateAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            const auto hz = YouKnow106Engine::lfoRateHz (value);
            return juce::String (hz, hz < 10.0f ? 2 : 1) + " Hz";
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForLfoRate (hertzFromText (text));
        });
}

juce::AudioParameterFloatAttributes lfoDelayAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            return secondsText (YouKnow106Engine::lfoDelaySeconds (value), 0);
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForLfoDelay (secondsFromText (text));
        });
}

juce::AudioParameterFloatAttributes cutoffAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            const auto hz = YouKnow106Engine::vcfCutoffHz (
                YouKnow106Engine::vcfPanelCounts (value));
            if (hz >= 1000.0f)
                return juce::String (hz / 1000.0f, 2) + " kHz";
            return juce::String (hz, hz < 100.0f ? 1 : 0) + " Hz";
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return YouKnow106Engine::panelPositionForCutoff (hertzFromText (text));
        });
}

juce::AudioParameterFloatAttributes portamentoAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            const auto seconds = YouKnow106Engine::portamentoSeconds (value);
            if (seconds <= 0.0f)
                return juce::String ("OFF");
            return secondsText (seconds, 0) + "/oct";
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            if (text.trim().equalsIgnoreCase ("off"))
                return 0.0f;
            return YouKnow106Engine::panelPositionForPortamento (
                secondsFromText (text));
        });
}

bool containsParameterState (const juce::ValueTree& state, const char* parameterId)
{
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };

    for (const auto& child : state)
        if (child.hasType (parameterType)
            && child.getProperty (idProperty).toString() == parameterId)
            return true;

    return false;
}

void addDefaultParameterStateIfMissing (juce::ValueTree& state,
                                        juce::AudioProcessorValueTreeState& parameters,
                                        const char* parameterId)
{
    if (containsParameterState (state, parameterId))
        return;

    if (const auto* parameter = parameters.getParameter (parameterId))
    {
        static const juce::Identifier parameterType { "PARAM" };
        static const juce::Identifier idProperty { "id" };
        static const juce::Identifier valueProperty { "value" };
        juce::ValueTree parameterState { parameterType };
        parameterState.setProperty (idProperty, parameterId, nullptr);
        parameterState.setProperty (
            valueProperty, parameter->convertFrom0to1 (parameter->getDefaultValue()),
            nullptr);
        state.appendChild (parameterState, nullptr);
    }
}

// Reads a parameter's stored value out of a saved state, or returns `fallback`.
float storedParameterValue (const juce::ValueTree& state, const char* parameterId,
                            float fallback)
{
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };
    static const juce::Identifier valueProperty { "value" };

    for (const auto& child : state)
        if (child.hasType (parameterType)
            && child.getProperty (idProperty).toString() == parameterId)
            return static_cast<float> (child.getProperty (valueProperty, fallback));
    return fallback;
}

void setStoredParameterValue (juce::ValueTree& state, const char* parameterId,
                              float value)
{
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };
    static const juce::Identifier valueProperty { "value" };

    for (auto child : state)
        if (child.hasType (parameterType)
            && child.getProperty (idProperty).toString() == parameterId)
        {
            child.setProperty (valueProperty, value, nullptr);
            return;
        }

    juce::ValueTree added { parameterType };
    added.setProperty (idProperty, parameterId, nullptr);
    added.setProperty (valueProperty, value, nullptr);
    state.appendChild (added, nullptr);
}
} // namespace

// Sessions saved before the paired switches were split carry a three-way
// `keyMode` (0 Poly 1, 1 Poly 2, 2 Unison) and a three-way `chorus` (0 off,
// 1 mode I, 2 mode II). Translate them into the button pairs that replaced
// them. A complete new pair remains authoritative over its stale legacy entry;
// when exactly one member is missing, the surviving member and the old choice
// together provide the most faithful reconstruction available.
void YouKnow106AudioProcessor::migrateSplitModeParameters (juce::ValueTree& state)
{
    using namespace youknow106::parameters;

    const bool hasPoly1 = containsParameterState (state, poly1);
    const bool hasPoly2 = containsParameterState (state, poly2);
    if (containsParameterState (state, "keyMode") && ! hasPoly1 && ! hasPoly2)
    {
        const auto legacy = juce::roundToInt (storedParameterValue (state, "keyMode", 0.0f));
        const bool unison = legacy == 2;
        setStoredParameterValue (state, poly1, (legacy == 0 || unison) ? 1.0f : 0.0f);
        setStoredParameterValue (state, poly2, (legacy == 1 || unison) ? 1.0f : 0.0f);
    }
    else if (hasPoly1 != hasPoly2)
    {
        // A truncated pair still tells us which solo mode it meant: if the
        // surviving lamp is on, the missing one was off; if it is off, the
        // missing lamp must have been the selected one. Filling a missing
        // Poly 1 from its default (`on`) would otherwise turn Poly 2 into
        // Unison merely because one XML child was absent.
        if (hasPoly1)
            setStoredParameterValue (
                state, poly2,
                storedParameterValue (state, poly1, 1.0f) > 0.5f ? 0.0f : 1.0f);
        else
            setStoredParameterValue (
                state, poly1,
                storedParameterValue (state, poly2, 0.0f) > 0.5f ? 0.0f : 1.0f);
    }

    // The assigner firmware always latches Poly 1, Poly 2 or both. Obsolete or
    // hand-edited states with neither bit set already sound as Poly 1 for
    // compatibility, so make their public state and lamps say the same thing.
    if (storedParameterValue (state, poly1, 0.0f) <= 0.5f
        && storedParameterValue (state, poly2, 0.0f) <= 0.5f)
        setStoredParameterValue (state, poly1, 1.0f);

    const bool hasLegacyChorus = containsParameterState (state, "chorus");
    const bool hasChorusI = containsParameterState (state, chorusI);
    const bool hasChorusII = containsParameterState (state, chorusII);
    if (hasLegacyChorus && ! hasChorusI && ! hasChorusII)
    {
        const auto legacy = juce::roundToInt (storedParameterValue (state, "chorus", 0.0f));
        setStoredParameterValue (state, chorusI, legacy == 1 ? 1.0f : 0.0f);
        setStoredParameterValue (state, chorusII, legacy == 2 ? 1.0f : 0.0f);
    }
    else if (hasChorusI != hasChorusII)
    {
        // Unlike POLY, both chorus lamps off is legal, so an off surviving
        // member is ambiguous by itself. The compatibility choice resolves
        // that ambiguity when it names the missing mode; an on modern member
        // remains authoritative over a stale legacy choice.
        const int legacy = hasLegacyChorus
                         ? juce::jlimit (0, 2, juce::roundToInt (
                               storedParameterValue (state, "chorus", 0.0f)))
                         : 0;
        if (hasChorusI)
        {
            const bool survivingOn =
                storedParameterValue (state, chorusI, 0.0f) > 0.5f;
            setStoredParameterValue (state, chorusII,
                                     ! survivingOn && legacy == 2 ? 1.0f : 0.0f);
        }
        else
        {
            const bool survivingOn =
                storedParameterValue (state, chorusII, 0.0f) > 0.5f;
            setStoredParameterValue (state, chorusI,
                                     ! survivingOn && legacy == 1 ? 1.0f : 0.0f);
        }
    }

    // Builds that briefly exposed I+II could save both booleans. A JUNO-106
    // has one enable line and one I/II line, so canonicalise that obsolete
    // fourth state to the hardware's stronger Mode II on restore.
    if (storedParameterValue (state, chorusI, 0.0f) > 0.5f
        && storedParameterValue (state, chorusII, 0.0f) > 0.5f)
        setStoredParameterValue (state, chorusI, 0.0f);

    // The two-state HQ switch became the three-rung quality ladder. Its stored
    // value is a boolean, so it names the two endpoints and nothing in between:
    // on is the deepest rung, off the cheapest. A state that already carries the
    // ladder is authoritative and the obsolete entry is ignored.
    if (containsParameterState (state, legacyHq)
        && ! containsParameterState (state, quality))
        setStoredParameterValue (
            state, quality,
            static_cast<float> (storedParameterValue (state, legacyHq, 1.0f) > 0.5f
                                    ? qualityChoiceCount - 1
                                    : 0));
}

juce::AudioProcessorValueTreeState::ParameterLayout
YouKnow106AudioProcessor::createParameterLayout()
{
    using namespace youknow106::parameters;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto travel = [] (const char* id, const char* name, float defaultValue,
                            juce::AudioParameterFloatAttributes attributes)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f }, defaultValue,
            std::move (attributes));
    };
    const auto toggle = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<SnappingBoolParameter> (
            juce::ParameterID { id, 1 }, name, defaultValue, boolAttributes());
    };

    // --- Front panel, in panel order --------------------------------------
    layout.add (travel (volume, "Volume", 0.80f, percentAttributes()));
    layout.add (travel (benderDco, "Bender DCO", 0.30f, percentAttributes()));
    layout.add (travel (benderVcf, "Bender VCF", 0.0f, percentAttributes()));
    layout.add (travel (benderLfo, "Bender LFO", 0.0f, percentAttributes()));
    layout.add (travel (portamento, "Portamento", 0.0f, portamentoAttributes()));
    // The panel has two momentary contacts and no separate unison button;
    // firmware latches Poly 1, Poly 2, or both when they are pressed together.
    // The previous release's key-mode id, in the slot it occupied then. An
    // Audio Unit binds automation by parameter *index*, so putting a restored
    // id anywhere else would bind an old lane to the wrong control -- worse
    // than leaving it dead. Everything that existed before keeps its position
    // and version hint; the switches that replaced it are appended at the end.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { legacyKeyMode, 1 }, "Key Mode (legacy)",
        juce::StringArray { "Poly 1", "Poly 2", "Unison" }, 0));

    layout.add (travel (lfoRate, "LFO Rate", 0.42f, lfoRateAttributes()));
    layout.add (travel (lfoDelay, "LFO Delay", 0.0f, lfoDelayAttributes()));

    layout.add (travel (dcoLfo, "DCO LFO", 0.0f, percentAttributes()));
    layout.add (travel (pwm, "PWM", 0.30f, percentAttributes()));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { pwmMode, 1 }, "PWM Mode",
        juce::StringArray { "LFO", "Manual" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { range, 1 }, "Range",
        juce::StringArray { "16'", "8'", "4'" }, 1));
    layout.add (toggle (saw, "Saw", true));
    layout.add (toggle (pulse, "Pulse", false));
    layout.add (travel (sub, "Sub", 0.0f, percentAttributes()));
    layout.add (travel (noise, "Noise", 0.0f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { highPass, 1 }, "HPF",
        juce::StringArray { "0", "1", "2", "3" }, 1));

    layout.add (travel (cutoff, "VCF Freq", 0.62f, cutoffAttributes()));
    layout.add (travel (resonance, "VCF Res", 0.10f, percentAttributes()));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { envPolarity, 1 }, "VCF Env Polarity",
        juce::StringArray { "Normal", "Inverted" }, 0));
    layout.add (travel (vcfEnv, "VCF Env", 0.35f, percentAttributes()));
    layout.add (travel (vcfLfo, "VCF LFO", 0.0f, percentAttributes()));
    layout.add (travel (keyFollow, "VCF Kybd", 0.50f, percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { vcaMode, 1 }, "VCA Mode",
        juce::StringArray { "Env", "Gate" }, 0));
    layout.add (travel (vcaLevel, "VCA Level", 0.80f, percentAttributes()));

    layout.add (travel (attack, "Attack", 0.0f, attackAttributes()));
    layout.add (travel (decay, "Decay", 0.45f, decayAttributes()));
    layout.add (travel (sustain, "Sustain", 0.70f, percentAttributes()));
    layout.add (travel (release, "Release", 0.30f, releaseAttributes()));

    // Likewise in its historical slot, after the envelope.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { legacyChorus, 1 }, "Chorus (legacy)",
        juce::StringArray { "Off", "I", "II" }, 0));

    // --- Controls the modelled instrument does not have -------------------
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { transpose, 1 }, "Transpose", -12, 12, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { masterTune, 1 }, "Master Tune",
        juce::NormalisableRange<float> { -50.0f, 50.0f, 0.0f }, 0.0f,
        centsAttributes()));
    layout.add (travel (velocity, "Velocity", 0.0f, percentAttributes()));
    // Keep the historical id and parameter slot so existing automation remains
    // attached; a stored session value still wins over this default, so only
    // new instances move. Zero is the calibrated nominal model and one is the
    // "matches real hardware" reference every rendered fixture in this
    // repository defaults to.
    //
    // The range used to continue to 100, to reach the exaggerated-for-contrast
    // territory the comparison-rendering tools used. Those tools no longer
    // touch this control -- they toggle one mechanism at a time with Unit
    // Character held at its default -- so the only thing the upper range still
    // did was extrapolate. Each mechanism is written as
    // nominal + (physical - nominal) * calibration, which is a blend only on
    // [0, 1]: past that it walks straight through the nominal value and out the
    // other side. The voice mixer inverted its own polarity above 1.8, and the
    // output summer's blend inverted too. Two is as far as the affine form
    // stays meaningful, and it still leaves room to exaggerate.
    juce::NormalisableRange<float> calibrationRange { 0.0f, 2.0f, 0.0f };
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { calibration, 1 }, "Unit Character",
        calibrationRange, 1.0f, percentAttributes()));
    layout.add (travel (chorusNoise, "Chorus Noise", 1.0f, percentAttributes()));
    // Taken from the engine rather than written out, so the host can never be
    // offered a voice count the engine would clamp away. The default is the
    // hardware's own six.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { polyphony, 1 }, "Polyphony", 1,
        youknow106::YouKnow106Engine::maxVoices,
        youknow106::YouKnow106Engine::hardwareVoices));
    // Persisted with the patch, but deliberately not automatable: changing the
    // internal rate empties the whole output path, so the engine holds the
    // change until it has been quiet long enough for that to be inaudible. An
    // automation point would therefore not take effect where it was written --
    // or at all, if it were automated back before the engine went idle.
    // Appended after every pre-existing parameter, with a later version hint,
    // so no historical Audio Unit index moves.
    layout.add (std::make_unique<SnappingBoolParameter> (
        juce::ParameterID { poly1, 2 }, "Poly 1", true, boolAttributes()));
    layout.add (std::make_unique<SnappingBoolParameter> (
        juce::ParameterID { poly2, 2 }, "Poly 2", false, boolAttributes()));
    layout.add (std::make_unique<SnappingBoolParameter> (
        juce::ParameterID { chorusI, 2 }, "Chorus I", false, boolAttributes()));
    layout.add (std::make_unique<SnappingBoolParameter> (
        juce::ParameterID { chorusII, 2 }, "Chorus II", false, boolAttributes()));

    // Keep the exact public parameter from the previous release. Removing it
    // would reorder Audio Unit parameters and detach saved Logic automation.
    // It is now an inert compatibility anchor; state migration below maps old
    // saved values to the quality ladder's two historical endpoints.
    layout.add (std::make_unique<SnappingBoolParameter> (
        juce::ParameterID { legacyHq, 1 }, "HQ", true,
        boolAttributes()
            .withAutomatable (false)
            .withStringFromValueFunction (
                [] (int enabled, int) { return enabled != 0 ? "On" : "Off"; })
            .withValueFromStringFunction (
                [] (const juce::String& text)
                {
                    return (text.containsIgnoreCase ("on")
                        || text.containsIgnoreCase ("hq")
                        || text.containsIgnoreCase ("2")
                        || text.containsIgnoreCase ("4")) ? 1 : 0;
                })));

    // The quality ladder, cheapest first, so a larger index is always more
    // internal work. New instances start at 2x: it keeps practical scheduling
    // headroom while 4x remains an explicit highest-fidelity choice. Version 3
    // follows every parameter shipped by the two earlier public layouts.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { quality, 3 }, "Quality",
        juce::StringArray { "1x", "2x", "4x" },
        choiceForOversamplingFactor (2),
        juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    return layout;
}

YouKnow106AudioProcessor::YouKnow106AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "YOUKNOW106_STATE", createParameterLayout())
{
    using namespace youknow106::parameters;
    // Size deduced, not restated: hard-coding it is what silently broke when
    // the paired switches each turned into two parameters.
    const auto ids = std::to_array<const char*> ({
        volume, benderDco, benderVcf, benderLfo, portamento, legacyKeyMode,
        lfoRate, lfoDelay, dcoLfo, pwm, pwmMode, range, saw, pulse, sub, noise,
        highPass, cutoff, resonance, envPolarity, vcfEnv, vcfLfo, keyFollow,
        vcaMode, vcaLevel, attack, decay, sustain, release, legacyChorus,
        transpose, masterTune, velocity, calibration, chorusNoise, polyphony,
        poly1, poly2, chorusI, chorusII, legacyHq, quality
    });

    // The pointer table has to be able to hold every id. Growing the list above
    // without growing the table would otherwise write past its end.
    jassert (ids.size() == parameterPointers.size());
    const auto count = std::min (ids.size(), parameterPointers.size());
    for (std::size_t index = 0; index < count; ++index)
        parameterPointers[index] = { ids[index],
                                     parameters.getRawParameterValue (ids[index]) };

    // Pair writes can come from the editor, direct modern host automation,
    // randomisation, recall or SysEx. Listening at the parameters is the only
    // place all of those paths meet, and lets a later pair write supersede an
    // obsolete-id write that is still waiting for the 20 ms bridge timer.
    for (const auto* id : { poly1, poly2, chorusI, chorusII })
        parameters.addParameterListener (id, this);

    keyboardState.addListener (this);

    // INIT is not merely the name shown for program zero: make the complete
    // product program authoritative at cold start. This deliberately repeats
    // the APVTS defaults through the same path every later recall uses, so a
    // future default edit cannot leave the preset bar and visible controls
    // describing different states on the first editor frame.
    setCurrentProgram (0);

    // Poll the incoming-MIDI handoff. The audio thread must not signal the
    // message thread directly, so nothing wakes this; it simply looks.
    startTimer (20);
}

YouKnow106AudioProcessor::~YouKnow106AudioProcessor()
{
    stopTimer();
    using namespace youknow106::parameters;
    for (const auto* id : { poly1, poly2, chorusI, chorusII })
        parameters.removeParameterListener (id, this);
    keyboardState.removeListener (this);
}

float YouKnow106AudioProcessor::valueOf (const char* parameterId) const noexcept
{
    // Every production call uses the shared parameter-id constants, so avoid
    // repeatedly comparing strings in the audio callback. Keep the string
    // path for callers that provide an equal ID from different storage.
    for (const auto& pointer : parameterPointers)
        if (pointer.id == parameterId && pointer.value != nullptr)
            return pointer.value->load (std::memory_order_relaxed);

    for (const auto& pointer : parameterPointers)
        if (pointer.id != nullptr && std::strcmp (pointer.id, parameterId) == 0
            && pointer.value != nullptr)
            return pointer.value->load (std::memory_order_relaxed);
    return 0.0f;
}

int YouKnow106AudioProcessor::choiceOf (const char* parameterId, int maximum) const noexcept
{
    return juce::jlimit (0, maximum, juce::roundToInt (valueOf (parameterId)));
}

void YouKnow106AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engineReady.store (false, std::memory_order_release);
    engine.prepare (sampleRate, samplesPerBlock,
                    oversamplingFactorForChoice (getQualityChoice()));
    (void) updateEngineParameters();
    uiLeverMailbox.store (emptyUiLeverMailbox, std::memory_order_release);
    discardUiMidiEvents();
    keyboardResetRequested.store (false, std::memory_order_release);
    keyboardState.reset();

    // The engine's own figure, not the host's: prepare() clamps a rate outside
    // the supported range and substitutes one for a non-finite report, and the
    // panel must describe what is running rather than what was asked for.
    displaySampleRate.store (engine.getSampleRate(), std::memory_order_relaxed);
    displayOversamplingFactor.store (engine.getOversamplingFactor(),
                                     std::memory_order_relaxed);
    setLatencySamples (engine.getProcessingLatencySamples());
    // prepare() is the cold path: the engine has just been returned to the
    // moment it was switched on, so the panel has to be too. Without this the
    // readouts kept the previous configuration's voice lamps and chassis
    // temperature until the first block of the new one arrived.
    clearDisplayTelemetry();
    engineReady.store (true, std::memory_order_release);
}

void YouKnow106AudioProcessor::clearDisplayTelemetry() noexcept
{
    // Everything the panel draws from the audio thread. The readouts are the
    // one part of the surface whose job is to say what is coming out, so any
    // path that stops the instrument has to retire them together rather than
    // leave a lit voice lamp and a frozen trace behind.
    activeVoiceCount.store (0, std::memory_order_relaxed);
    displayVoiceMask.store (0, std::memory_order_relaxed);
    displayEnvelope.store (0.0f, std::memory_order_relaxed);
    displayLfo.store (0.0f, std::memory_order_relaxed);
    displayTemperature.store (engine.getDisplayTemperatureC(),
                              std::memory_order_relaxed);
    displayRailDroop.store (engine.getDisplayRailDroopVolts(),
                            std::memory_order_relaxed);
    for (auto& sample : scopeBuffer)
        sample.store (0.0f, std::memory_order_relaxed);
    scopeWriteIndex.store (0, std::memory_order_release);
}

void YouKnow106AudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    engine.allNotesOff();
    engine.reset();
    uiLeverMailbox.store (emptyUiLeverMailbox, std::memory_order_release);
    discardUiMidiEvents();
    keyboardResetRequested.store (true, std::memory_order_release);
    // The host has torn the instrument down. Without this the panel kept the
    // last block's voice lamps, envelope meter and oscilloscope trace beside
    // its own STANDBY legend for as long as the window stayed open.
    clearDisplayTelemetry();
}

void YouKnow106AudioProcessor::reset()
{
    // Hosts use reset when transport/processing is torn down without a new
    // prepareToPlay call. Clear voices, tails and transient controllers while
    // retaining the negotiated rate, parameters and selected program -- and,
    // because a transport stop is not a power cycle, the modelled chassis
    // warm-up the engine has been accumulating.
    const bool wasReady = engineReady.exchange (false, std::memory_order_acq_rel);
    if (wasReady)
        engine.resetForHostStop();

    keyboardResetRequested.store (true, std::memory_order_release);
    discardUiMidiEvents();
    uiLeverMailbox.store (emptyUiLeverMailbox, std::memory_order_release);
    panicRequested.store (false, std::memory_order_release);
    keyModeReassertRequested.store (false, std::memory_order_release);
    pendingMidiToneShadowActive = false;
    clearDisplayTelemetry();

    if (wasReady)
        engineReady.store (true, std::memory_order_release);
}

bool YouKnow106AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannels() != 0)
        return false;
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::stereo();
}

bool YouKnow106AudioProcessor::updateEngineParameters() noexcept
{
    using namespace youknow106;
    using namespace youknow106::parameters;

    const unsigned parameterGeneration =
        parameterWriteGeneration.load (std::memory_order_acquire);
    if ((parameterGeneration & 1u) != 0u)
        return false;
    // Reflection publishes this only after its matching APVTS writes. Sampling
    // it on both sides of the parameter gather catches even a one-parameter
    // reflection, which intentionally does not open a multi-write generation.
    const auto reflectedBeforeSnapshot =
        reflectedMidiSequence.load (std::memory_order_acquire);

    EngineParameters engineParameters;
    engineParameters.volume = valueOf (volume);
    engineParameters.benderDcoDepth = valueOf (benderDco);
    engineParameters.benderVcfDepth = valueOf (benderVcf);
    engineParameters.benderLfoDepth = valueOf (benderLfo);
    engineParameters.portamento = valueOf (portamento);

    // A restore replaces both the legacy ids and the pairs at once, so whatever
    // the bridges were holding is stale. Adopting the restored legacy values as
    // the new baseline is what stops the session's own saved pair -- a Unison
    // saved as POLY 1 + POLY 2, say -- from being overwritten by a legacy id
    // that has not actually moved.
    auto nextKeyModeBridge = keyModeBridge;
    auto nextChorusBridge = chorusBridge;
    const bool reseeding = reseedLegacyBridges.exchange (
        false, std::memory_order_acq_rel);
    if (reseeding)
    {
        nextKeyModeBridge = {
            restoredLegacyKeyMode.load (std::memory_order_acquire), 0, false,
            restoredKeyModeForwardGeneration.load (std::memory_order_acquire)
        };
        nextChorusBridge = {
            restoredLegacyChorus.load (std::memory_order_acquire), 0, false,
            restoredChorusForwardGeneration.load (std::memory_order_acquire)
        };
    }

    const int keyForwardGeneration =
        keyModeForwardGeneration.load (std::memory_order_acquire);
    const int keyForwardedValue =
        forwardedLegacyKeyMode.load (std::memory_order_relaxed);

    engineParameters.keyMode = static_cast<KeyMode> (resolveLegacyMode (
        choiceOf (legacyKeyMode, 2),
        static_cast<int> (keyModeFor (valueOf (poly1) > 0.5f, valueOf (poly2) > 0.5f)),
        keyForwardedValue, keyForwardGeneration, nextKeyModeBridge));

    engineParameters.lfoRate = valueOf (lfoRate);
    engineParameters.lfoDelay = valueOf (lfoDelay);

    engineParameters.dcoLfoDepth = valueOf (dcoLfo);
    engineParameters.pwmDepth = valueOf (pwm);
    engineParameters.pwmSource = static_cast<PwmSource> (choiceOf (pwmMode, 1));
    engineParameters.range = static_cast<DcoRange> (choiceOf (range, 2));
    engineParameters.sawEnabled = valueOf (saw) > 0.5f;
    engineParameters.pulseEnabled = valueOf (pulse) > 0.5f;
    engineParameters.subLevel = valueOf (sub);
    engineParameters.noiseLevel = valueOf (noise);

    engineParameters.highPass = static_cast<HighPassMode> (choiceOf (highPass, 3));

    engineParameters.cutoff = valueOf (cutoff);
    engineParameters.resonance = valueOf (resonance);
    engineParameters.envPolarity = static_cast<EnvPolarity> (choiceOf (envPolarity, 1));
    engineParameters.envDepth = valueOf (vcfEnv);
    engineParameters.vcfLfoDepth = valueOf (vcfLfo);
    engineParameters.keyFollow = valueOf (keyFollow);

    engineParameters.vcaMode = static_cast<VcaMode> (choiceOf (vcaMode, 1));
    engineParameters.vcaLevel = valueOf (vcaLevel);

    engineParameters.attack = valueOf (attack);
    engineParameters.decay = valueOf (decay);
    engineParameters.sustain = valueOf (sustain);
    engineParameters.release = valueOf (release);

    const int chorusGeneration =
        chorusForwardGeneration.load (std::memory_order_acquire);
    const int chorusForwardedValue =
        forwardedLegacyChorus.load (std::memory_order_relaxed);
    engineParameters.chorus = static_cast<ChorusMode> (resolveLegacyMode (
        choiceOf (legacyChorus, 2),
        static_cast<int> (chorusModeFor (valueOf (chorusI) > 0.5f, valueOf (chorusII) > 0.5f)),
        chorusForwardedValue, chorusGeneration, nextChorusBridge));

    engineParameters.keyTranspose = juce::roundToInt (valueOf (transpose));
    engineParameters.masterTuneCents = valueOf (masterTune);
    engineParameters.velocityDepth = valueOf (velocity);
    engineParameters.calibration = valueOf (calibration);
    engineParameters.chorusNoise = valueOf (chorusNoise);
    engineParameters.polyphony = juce::roundToInt (valueOf (polyphony));

    // If a recall began while these atomics were being gathered, keep the
    // previous engine snapshot for this block. The next one will see the whole
    // new patch; no block ever sees half of each.
    const auto generationAfterSnapshot =
        parameterWriteGeneration.load (std::memory_order_acquire);
    if (generationAfterSnapshot != parameterGeneration)
    {
        if (reseeding)
            reseedLegacyBridges.store (true, std::memory_order_release);
        return false;
    }

    if (auto* captured = midiReflectionSnapshotCapturedForTest;
        captured != nullptr && midiReflectionSnapshotResumeForTest != nullptr)
    {
        auto* resume = midiReflectionSnapshotResumeForTest;
        captured->store (true, std::memory_order_release);
        while (! resume->load (std::memory_order_acquire))
            juce::Thread::yield();
        midiReflectionSnapshotCapturedForTest = nullptr;
        midiReflectionSnapshotResumeForTest = nullptr;
    }

    const auto reflectedAfterSnapshot =
        reflectedMidiSequence.load (std::memory_order_acquire);
    // A reflection can finish after the first generation check. If its ack
    // advanced, or its bracketed writes changed the generation after that
    // check, this local APVTS copy may predate the acknowledged patch. Keep the
    // previous engine/shadow for one block and gather again instead of briefly
    // reverting to that stale copy.
    if (reflectedAfterSnapshot != reflectedBeforeSnapshot
        || parameterWriteGeneration.load (std::memory_order_acquire)
               != parameterGeneration)
    {
        if (reseeding)
            reseedLegacyBridges.store (true, std::memory_order_release);
        return false;
    }

    keyModeBridge = nextKeyModeBridge;
    chorusBridge = nextChorusBridge;
    audioBaseParameters = engineParameters;

    // The message thread may currently be reflecting an older event from the
    // same queue. Do not let that intermediate APVTS state pull the DSP back
    // from a newer MIDI event which has already happened in sample time.
    if (pendingMidiToneShadowActive)
    {
        if (reflectedAfterSnapshot >= pendingMidiToneShadowSequence)
            pendingMidiToneShadowActive = false;
        else
            applyPatchToEngineParameters (engineParameters,
                                          pendingMidiToneShadow);
    }
    engine.setParameters (engineParameters);

    // The editor draws one lamp per available voice, so it needs the count the
    // engine actually settled on rather than the raw parameter.
    displayVoiceLimit.store (
        juce::jlimit (1, youknow106::YouKnow106Engine::maxVoices,
                      engineParameters.polyphony),
        std::memory_order_relaxed);
    return true;
}

void YouKnow106AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int channel = getTotalNumInputChannels(); channel < buffer.getNumChannels();
         ++channel)
        buffer.clear (channel, 0, numSamples);

    if (! engineReady.load (std::memory_order_acquire))
    {
        buffer.clear();
        midiMessages.clear();
        return;
    }

    // If a previous block overflowed the reflection-history FIFO, publish its
    // final tone before any newer events from this block. This preserves FIFO
    // ordering while keeping the callback entirely bounded and lock-free.
    tryStagePendingMidiResync();

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        engine.allNotesOff();
        discardUiMidiEvents();
    }

    // The engine reports one latency for every configuration and pads the
    // shallower ones out to it, so the quality setting can change here without
    // the host having to be told anything. Calling setLatencySamples from the
    // audio callback would be a notification into host code that is free to
    // lock or allocate.
    engine.setOversamplingFactor (
        oversamplingFactorForChoice (getQualityChoice()));
    displayOversamplingFactor.store (engine.getOversamplingFactor(),
                                     std::memory_order_relaxed);

    // Latch the momentary contact before sampling its paired parameters. The
    // request publishes pair authority before its release-store, so this
    // acquire makes a pre-boundary click visible to the snapshot below. A
    // click arriving after the exchange stays queued for the next block rather
    // than reasserting a mode this block did not yet adopt.
    const bool reassertKeyMode =
        keyModeReassertRequested.exchange (false, std::memory_order_acq_rel);
    const bool parametersUpdated = updateEngineParameters();
    if (reassertKeyMode)
    {
        if (parametersUpdated)
            engine.reassertKeyMode();
        else
            keyModeReassertRequested.store (true, std::memory_order_release);
    }
    dispatchUiPerformanceControls();
    dispatchUiMidiEvents();

    // Render up to each event before applying it. Dispatching the whole buffer's
    // MIDI at the block boundary would collapse a short note-on/note-off pair
    // onto the same instant and lose the note entirely.
    int renderedTo = 0;
    for (const auto metadata : midiMessages)
    {
        // JUCE permits occasional zero-frame callbacks which still carry MIDI.
        // Give every event timestamp zero explicitly in that case; there is no
        // audio range to clamp against, but its state transition still counts.
        const int eventSample = numSamples > 0
                              ? juce::jlimit (0, numSamples,
                                             metadata.samplePosition)
                              : 0;
        if (eventSample > renderedTo)
        {
            engine.process (buffer.getWritePointer (0, renderedTo),
                            buffer.getWritePointer (1, renderedTo),
                            eventSample - renderedTo);
            renderedTo = eventSample;
        }

        // MidiMessageMetadata is a non-owning view, but getMessage() makes an
        // owning copy. JUCE's inline MidiMessage storage is pointer-sized, so
        // a 23-byte patch dump would malloc/free here in the audio callback.
        // Recognise and parse SysEx from the view before constructing anything.
        const auto* raw = metadata.data;
        const auto length = metadata.numBytes;
        if (raw == nullptr || length <= 0)
            continue;

        if (raw[0] == 0xf0)
        {
            // A patch dump from the hardware, or from a librarian speaking to
            // it. The parameters cannot be written from here, so the tone bytes
            // are staged and applied on the message thread. Every accepted
            // message keeps its place in the fixed queue, so a complete bank
            // transfer is applied in arrival order without touching APVTS
            // parameters from the real-time callback.
            sysex::Patch incoming {};
            int channel = 0;
            int parameter = 0;
            int value = 0;

            if (sysex::readPatchMessage (raw, static_cast<std::size_t> (length),
                                         incoming, channel))
            {
                sysExChannel.store (channel, std::memory_order_relaxed);
                // A whole patch. The tone bytes are the message's own
                // representation, so hand them over exactly as they arrived.
                // Re-encoding the parsed Patch here would canonicalise packed
                // switch combinations before the consumer had even seen them.
                PendingMidiEvent event;
                event.kind = PendingMidiEventKind::FullPatch;
                std::copy_n (raw + 4, sysex::toneByteCount, event.bytes.begin());
                stageAndApplyPendingMidiEvent (event);
            }
            else if (sysex::readParameterMessage (
                         raw, static_cast<std::size_t> (length), parameter, value,
                         channel))
            {
                sysExChannel.store (channel, std::memory_order_relaxed);
                // One control moved on the hardware. Only its number and value
                // cross over: the panel it applies to is read on the message
                // thread, so nothing else is quantised and no stale mirror of
                // the panel can revert an edit made in between.
                PendingMidiEvent event;
                event.kind = PendingMidiEventKind::SingleParameter;
                event.parameter = parameter;
                event.value = value;
                if (parameter >= 0 && parameter < sysex::toneByteCount)
                    stageAndApplyPendingMidiEvent (event);
            }
            continue;
        }

        // Every supported non-SysEx event is a one-, two- or three-byte MIDI
        // message, which stays inside MidiMessage's inline storage. Ignore an
        // unrelated long event rather than allocating merely to discover that
        // the instrument does not handle it.
        if (length > 3)
            continue;

        const auto message = metadata.getMessage();
        if (message.isNoteOn())
            engine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff())
            engine.noteOff (message.getNoteNumber());
        else if (message.isAllSoundOff())
            engine.allNotesOff();
        else if (message.isAllNotesOff())
            // All notes off means release the keys, not cut the sound: the
            // exact B-2 release can take about 25.55 seconds to reach digital
            // zero, and truncating it would be an all-sound-off.
            engine.releaseAllNotes();
        else if (message.isPitchWheel())
            engine.setPitchBend ((static_cast<float> (message.getPitchWheelValue())
                                  - 8192.0f) / 8192.0f);
        else if (message.isProgramChange())
        {
            // The full factory memory follows the owner's Patch Selection
            // numbering directly: 0..63 address A11..A88 and 64..127 address
            // B11..B88.
            const int programIndex = hostProgramIndexForMidiProgram (
                message.getProgramChangeNumber());
            if (programIndex > 0)
            {
                PendingMidiEvent event;
                event.kind = PendingMidiEventKind::ProgramChange;
                event.programIndex = programIndex;
                stageAndApplyPendingMidiEvent (event);
            }
        }
        else if (message.isController())
        {
            // The owner's MIDI implementation chart recognizes modulation
            // (CC 1) and hold (CC 64). Modulation drives the bender lever's
            // forward/LFO axis; its audible amount is still set by BENDER LFO
            // on the panel.
            if (message.getControllerNumber() == 1)
                engine.setModWheel (static_cast<float> (message.getControllerValue())
                                    / 127.0f);
            else if (message.getControllerNumber() == 64)
                engine.setSustainPedal (message.getControllerValue() >= 64);
            else if (message.getControllerNumber() == 121)
            {
                // Reset All Controllers. Lifting the pedal matters most: with
                // it down and keys already released, ignoring this message
                // would leave those voices held until a panic.
                engine.setSustainPedal (false);
                engine.setModWheel (0.0f);
                engine.setPitchBend (0.0f);
            }
        }
    }

    if (renderedTo < numSamples)
        engine.process (buffer.getWritePointer (0, renderedTo),
                        buffer.getWritePointer (1, renderedTo),
                        numSamples - renderedTo);

    // Nothing this instrument plays goes back out, so the incoming events stop
    // here rather than passing through.
    midiMessages.clear();

    // Bypass exposes silence. Clearing it here rather than after this call
    // returns is what makes the oscilloscope below describe the block the host
    // will actually receive: it used to draw a full-amplitude trace of audio
    // the bypass had already thrown away.
    if (renderingBypassed)
        buffer.clear();

    // The message thread may have made room while this block was rendering.
    // A second attempt lets an offline render which ends on the overflow block
    // publish its final coalesced state without waiting for another MIDI event.
    tryStagePendingMidiResync();
    if (pendingMidiNeedsResync)
        publishPendingMidiResyncMailbox();

    activeVoiceCount.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);
    displayVoiceMask.store (engine.getDisplayVoiceMask(), std::memory_order_relaxed);
    displayEnvelope.store (engine.getDisplayEnvelope(), std::memory_order_relaxed);
    displayLfo.store (engine.getDisplayLfo(), std::memory_order_relaxed);
    displayTemperature.store (engine.getDisplayTemperatureC(), std::memory_order_relaxed);
    displayRailDroop.store (engine.getDisplayRailDroopVolts(), std::memory_order_relaxed);

    // Push output audio into oscilloscope ring buffer
    const float* leftOut = buffer.getReadPointer (0);
    const float* rightOut = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : leftOut;
    std::size_t writeIdx = scopeWriteIndex.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        scopeBuffer[writeIdx].store (0.5f * (leftOut[i] + rightOut[i]),
                                     std::memory_order_relaxed);
        writeIdx = (writeIdx + 1) % scopeBufferSize;
    }
    scopeWriteIndex.store (writeIdx, std::memory_order_release);
}

void YouKnow106AudioProcessor::processBlockBypassed (
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // JUCE's default bypass assumes zero latency and ignores the instrument's
    // MIDI stream. Keep the synth clock and note/controller state moving so a
    // note-off received while bypassed cannot become a stuck note, then expose
    // silence as an instrument with no input is expected to do.
    const juce::ScopedValueSetter<bool> bypassing (renderingBypassed, true);
    processBlock (buffer, midiMessages);
}

void YouKnow106AudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int,
                                             int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, true);
}

void YouKnow106AudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int,
                                              int midiNoteNumber, float)
{
    enqueueUiMidiEvent (midiNoteNumber, 0.0f, false);
}

void YouKnow106AudioProcessor::postUiLeverPosition (
    float normalisedBipolar, float modulation) noexcept
{
    const float bend = std::isfinite (normalisedBipolar)
                     ? std::clamp (normalisedBipolar, -1.0f, 1.0f)
                     : 0.0f;
    const float mod = std::isfinite (modulation)
                    ? std::clamp (modulation, 0.0f, 1.0f)
                    : 0.0f;
    const auto pitchWheel = static_cast<std::uint32_t> (juce::jlimit (
        0, 16383, juce::roundToInt (8192.0f + bend * 8192.0f)));
    const auto modWheel = static_cast<std::uint32_t> (juce::jlimit (
        0, 127, juce::roundToInt (mod * 127.0f)));
    uiLeverMailbox.store (pitchWheel | (modWheel << 14),
                          std::memory_order_release);
}

void YouKnow106AudioProcessor::parameterChanged (const juce::String& parameterId,
                                                 float)
{
    using namespace youknow106::parameters;
    if (parameterId == poly1 || parameterId == poly2)
        publishKeyModePairAuthority();
    else if (parameterId == chorusI || parameterId == chorusII)
        publishChorusPairAuthority();
}

void YouKnow106AudioProcessor::enqueueUiMidiEvent (int note, float velocity,
                                                   bool isNoteOn) noexcept
{
    const auto write = uiWriteIndex.load (std::memory_order_relaxed);
    const auto read = uiReadIndex.load (std::memory_order_acquire);
    if (write - read >= uiQueueCapacity)
    {
        // The queue only fills if processing has stalled for a long time. A
        // press dropped here is a note that never sounds, which is a shrug; a
        // release dropped here is a note held down for good, which is not. So
        // releases are never dropped -- they are remembered as a pending key
        // lift and applied once the backlog has been worked through.
        if (! isNoteOn)
        {
            const auto index = static_cast<unsigned> (juce::jlimit (0, 127, note));
            uiPendingNoteOff[index >> 6].fetch_or (1ull << (index & 63u),
                                                   std::memory_order_release);
        }
        return;
    }

    uiMidiQueue[write % uiQueueCapacity] = { note, velocity, isNoteOn };
    uiWriteIndex.store (write + 1, std::memory_order_release);
}

void YouKnow106AudioProcessor::discardUiMidiEvents() noexcept
{
    uiReadIndex.store (uiWriteIndex.load (std::memory_order_acquire),
                       std::memory_order_release);
    // Nothing is held any more, so there is no release left to honour.
    for (auto& pending : uiPendingNoteOff)
        pending.store (0, std::memory_order_release);
}

void YouKnow106AudioProcessor::dispatchUiMidiEvents() noexcept
{
    auto read = uiReadIndex.load (std::memory_order_relaxed);
    const auto write = uiWriteIndex.load (std::memory_order_acquire);

    // The on-screen keyboard has no sample clock, so its events arrive at the
    // block boundary with nothing to place them within the block. Applying a
    // whole backlog at that one instant would let a press and its release land
    // together and cancel out -- which is what happens when the audio thread
    // was stalled or the host buffer is long. So the drain stops at the second
    // event for any one key and leaves the rest queued: every press then gets
    // at least a block of its own, at the cost of a block of latency in the
    // case that would otherwise have lost the note entirely.
    std::array<std::uint64_t, 2> touched { 0u, 0u };

    while (read != write)
    {
        const auto event = uiMidiQueue[read % uiQueueCapacity];
        const auto note = static_cast<unsigned> (juce::jlimit (0, 127, event.note));
        const auto word = static_cast<std::size_t> (note >> 6);
        const std::uint64_t bit = 1ull << (note & 63u);
        if ((touched[word] & bit) != 0)
            break;
        touched[word] |= bit;

        if (event.noteOn)
            engine.noteOn (event.note, event.velocity);
        else
            engine.noteOff (event.note);
        ++read;
    }

    uiReadIndex.store (read, std::memory_order_release);

    // Keys whose release was dropped by a full queue. Two kinds of key have to
    // wait: one already touched in this drain, whose press would otherwise be
    // cancelled before anything is rendered, and one whose press is still
    // sitting further down the queue, which the release must not overtake.
    std::array<std::uint64_t, 2> deferred = touched;
    for (auto scan = read; scan != write; ++scan)
    {
        const auto queued = uiMidiQueue[scan % uiQueueCapacity];
        const auto note = static_cast<unsigned> (juce::jlimit (0, 127, queued.note));
        deferred[static_cast<std::size_t> (note >> 6)] |= 1ull << (note & 63u);
    }

    for (std::size_t word = 0; word < uiPendingNoteOff.size(); ++word)
    {
        const auto pending = uiPendingNoteOff[word].load (std::memory_order_acquire)
                           & ~deferred[word];
        if (pending == 0)
            continue;

        for (unsigned bit = 0; bit < 64; ++bit)
        {
            const std::uint64_t mask = 1ull << bit;
            if ((pending & mask) == 0)
                continue;
            uiPendingNoteOff[word].fetch_and (~mask, std::memory_order_acq_rel);
            engine.noteOff (static_cast<int> (word * 64u + bit));
        }
    }
}

void YouKnow106AudioProcessor::dispatchUiPerformanceControls() noexcept
{
    const auto packed = uiLeverMailbox.exchange (emptyUiLeverMailbox,
                                                  std::memory_order_acq_rel);
    if (packed == emptyUiLeverMailbox)
        return;

    const auto pitchWheel = static_cast<int> (packed & 0x3fffu);
    const auto modWheel = static_cast<int> ((packed >> 14) & 0x7fu);
    engine.setPitchBend ((static_cast<float> (pitchWheel) - 8192.0f) / 8192.0f);
    engine.setModWheel (static_cast<float> (modWheel) / 127.0f);
}

thread_local YouKnow106AudioProcessor::ScopedParameterWrite*
    YouKnow106AudioProcessor::activeParameterWrite = nullptr;

YouKnow106AudioProcessor::ScopedParameterWrite::ScopedParameterWrite (
    YouKnow106AudioProcessor& owner) noexcept
    : processor (owner), previous (activeParameterWrite)
{
    // A synchronous host callback may make another same-processor change. It
    // belongs to the outer transaction and must not wait on its own odd marker.
    for (auto* scope = previous; scope != nullptr; scope = scope->previous)
        if (&scope->processor == &processor)
        {
            activeParameterWrite = this;
            return;
        }

    // Different writer threads serialize here. Once this CAS owns the odd
    // generation, neither the audio thread nor a state-save thread can accept
    // a partially written parameter set.
    auto observed =
        processor.parameterWriteGeneration.load (std::memory_order_acquire);
    for (;;)
    {
        if ((observed & 1u) != 0u)
        {
            juce::Thread::yield();
            observed = processor.parameterWriteGeneration.load (
                std::memory_order_acquire);
            continue;
        }
        if (processor.parameterWriteGeneration.compare_exchange_weak (
                observed, observed + 1u, std::memory_order_acq_rel,
                std::memory_order_acquire))
            break;
    }

    ownsGeneration = true;
    for (std::size_t index = 0; index < values.size(); ++index)
        if (const auto* raw = processor.parameterPointers[index].value)
            values[index] = raw->load (std::memory_order_relaxed);
    program = processor.currentProgram.load (std::memory_order_relaxed);
    activeParameterWrite = this;
}

YouKnow106AudioProcessor::ScopedParameterWrite::~ScopedParameterWrite()
{
    jassert (activeParameterWrite == this);
    activeParameterWrite = previous;
    if (ownsGeneration)
    {
        const auto old = processor.parameterWriteGeneration.fetch_add (
            1u, std::memory_order_release);
        jassert ((old & 1u) != 0u);
        (void) old;
    }
}

const YouKnow106AudioProcessor::ScopedParameterWrite*
YouKnow106AudioProcessor::activeWriteForThisThread() const noexcept
{
    for (auto* scope = activeParameterWrite;
         scope != nullptr; scope = scope->previous)
        if (&scope->processor == this && scope->ownsGeneration)
            return scope;
    return nullptr;
}

void YouKnow106AudioProcessor::serialiseWriteSnapshot (
    const ScopedParameterWrite& snapshot,
    juce::MemoryBlock& destinationData)
{
    juce::ValueTree state { parameters.state.getType() };
    for (std::size_t index = 0; index < snapshot.values.size(); ++index)
        if (const auto* id = parameterPointers[index].id)
            setStoredParameterValue (state, id, snapshot.values[index]);
    serialiseStateSnapshot (std::move (state), snapshot.program,
                            destinationData);
}

void YouKnow106AudioProcessor::serialiseStateSnapshot (
    juce::ValueTree state, int program, juce::MemoryBlock& destinationData)
{
    if (! state.isValid())
        return;

    // Host automation can address the two compatibility booleans without
    // going through the interlocked panel. Persist the audio engine's
    // canonical interpretation, mode II, rather than another both-on state.
    if (storedParameterValue (state, youknow106::parameters::chorusI, 0.0f) > 0.5f
        && storedParameterValue (state, youknow106::parameters::chorusII, 0.0f)
               > 0.5f)
        setStoredParameterValue (state, youknow106::parameters::chorusI, 0.0f);
    if (storedParameterValue (state, youknow106::parameters::poly1, 0.0f) <= 0.5f
        && storedParameterValue (state, youknow106::parameters::poly2, 0.0f)
               <= 0.5f)
        setStoredParameterValue (state, youknow106::parameters::poly1, 1.0f);

    state.setProperty (stateSchemaVersionProperty, currentStateSchemaVersion,
                       nullptr);
    state.setProperty ("program", program, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destinationData);
}

void YouKnow106AudioProcessor::randomizeParameters (float amount)
{
    if (const auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
        messageManager != nullptr && ! messageManager->isThisTheMessageThread())
    {
        // Host gesture callbacks belong to the message thread. Refuse an
        // accidental call from anywhere else rather than racing the host's own
        // listeners.
        jassertfalse;
        return;
    }

    if (! std::isfinite (amount))
        return;
    amount = juce::jlimit (0.0f, 1.0f, amount);
    if (amount <= 0.0f)
        return;

    using namespace youknow106::parameters;
    // Deliberately sound-design controls only. Main volume, voice count,
    // oversampling, and the two controls that describe the *instrument* rather
    // than the patch — Unit Character and Chorus Noise — are excluded. Stored VCA
    // LEVEL remains included because it is the hardware's per-patch balance
    // trim, not the player's output-volume control.
    static constexpr auto soundParameterIds = std::to_array<const char*> ({
        benderDco, benderVcf, benderLfo, portamento, poly1, poly2,
        lfoRate, lfoDelay,
        dcoLfo, pwm, pwmMode, range, saw, pulse, sub, noise,
        highPass,
        cutoff, resonance, envPolarity, vcfEnv, vcfLfo, keyFollow,
        vcaMode, vcaLevel,
        attack, decay, sustain, release,
        chorusI, chorusII
    });

    ScopedParameterWrite write { *this };
    juce::Random random;
    for (const auto* parameterId : soundParameterIds)
    {
        auto* parameter = parameters.getParameter (parameterId);
        jassert (parameter != nullptr);
        if (parameter == nullptr)
            continue;

        const float current = parameter->getValue();
        const float destination = random.nextFloat();
        const float requested = juce::jlimit (
            0.0f, 1.0f, current + amount * (destination - current));
        // Round through the parameter's own range before notifying the host, so
        // a switch cannot report a value between two of its positions.
        const float legal = parameter->convertTo0to1 (
            parameter->convertFrom0to1 (requested));
        // A switch's step can be wider than a subtle strength. Where it is,
        // leaving the control alone is more honest than letting quantisation
        // move it further than the button advertises.
        const float movement = std::abs (legal - current);
        if (movement <= 1.0e-7f || movement > amount + 1.0e-7f)
            continue;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (legal);
        parameter->endChangeGesture();
    }

    // The hardware buttons interlock. A full randomisation can independently
    // land both automatable booleans high, so resolve that obsolete state to
    // mode II just as state restore and patch recall do.
    if (valueOf (chorusI) > 0.5f && valueOf (chorusII) > 0.5f)
        if (auto* first = parameters.getParameter (chorusI))
            first->setValueNotifyingHost (first->convertTo0to1 (0.0f));
    if (valueOf (poly1) <= 0.5f && valueOf (poly2) <= 0.5f)
        if (auto* first = parameters.getParameter (poly1))
            first->setValueNotifyingHost (first->convertTo0to1 (1.0f));

}

void YouKnow106AudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    // Parameter notifications are synchronous, so a host may save state from
    // the same call stack as a multi-parameter write. Return the fixed snapshot
    // taken immediately before that transaction rather than waiting on itself.
    if (const auto* snapshot = activeWriteForThisThread())
    {
        serialiseWriteSnapshot (*snapshot, destinationData);
        return;
    }

    for (;;)
    {
        const unsigned before =
            parameterWriteGeneration.load (std::memory_order_acquire);
        if ((before & 1u) != 0u)
        {
            juce::Thread::yield();
            continue;
        }

        auto state = parameters.copyState();
        const int program = currentProgram.load (std::memory_order_relaxed);
        const unsigned after =
            parameterWriteGeneration.load (std::memory_order_acquire);
        if (before != after || (after & 1u) != 0u)
            continue;

        serialiseStateSnapshot (std::move (state), program, destinationData);
        return;
    }
}

void YouKnow106AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid())
        return;

    // Schema-less chunks predate Unit Character entirely. Most such chunks
    // explicitly carry `calibration`, which must be preserved as-is; the
    // historical 35% fallback is needed only for an actually missing child, and
    // it stays 35% because that is what those sessions sounded like. A
    // current-schema partial state instead receives today's layout default.
    const int restoredStateSchema = static_cast<int> (
        state.getProperty (stateSchemaVersionProperty, 0));
    if (restoredStateSchema < 2
        && containsParameterState (state, youknow106::parameters::volume))
        setStoredParameterValue (
            state, youknow106::parameters::volume,
            migrateLegacyVolumePosition (storedParameterValue (
                state, youknow106::parameters::volume, 0.8f)));
    if (restoredStateSchema < calibrationDefaultSchemaVersion
        && ! containsParameterState (state, youknow106::parameters::calibration))
        setStoredParameterValue (state, youknow106::parameters::calibration,
                                 legacyCalibrationDefault);

    // Sessions written before the paired switches were split still carry the
    // old three-way `keyMode` and `chorus` choices. Filling in defaults alone
    // would silently reopen a saved Unison as Poly 1 and a saved chorus as off,
    // so the old values are translated first.
    // Chunks written before the property existed describe an edited panel, not
    // a known factory selection, and therefore migrate to INIT instead of
    // retaining this instance's stale previous index. The value is published
    // only inside the same stable generation as the replacement parameter tree.
    int restoredProgram = 0;
    // Schema 2 and earlier numbered the former 32 original YouKnow106 sounds.
    // The parameters remain authoritative and are restored below, but carrying
    // one of those indices into the historical 128 would falsely identify it
    // as an unrelated factory tone. Leave such states as an edited INIT/custom
    // panel instead. Current-schema states retain their exact selection.
    if (restoredStateSchema >= originalFactoryBankSchemaVersion
        && state.hasProperty ("program"))
        restoredProgram = juce::jlimit (
            0, getNumPrograms() - 1,
            static_cast<int> (state.getProperty ("program")));

    migrateSplitModeParameters (state);

    // A state written by an earlier build will not carry parameters added
    // since. Filling them with their defaults keeps the rest of the patch
    // rather than discarding the whole thing.
    for (const auto& pointer : parameterPointers)
        if (pointer.id != nullptr)
            addDefaultParameterStateIfMissing (state, parameters, pointer.id);

    const int restoredKeyMode = juce::jlimit (
        0, 2, juce::roundToInt (storedParameterValue (
                  state, youknow106::parameters::legacyKeyMode, 0.0f)));
    const int restoredChorusMode = juce::jlimit (
        0, 2, juce::roundToInt (storedParameterValue (
                  state, youknow106::parameters::legacyChorus, 0.0f)));

    // Deliberately no engine update here. This runs on the message thread while
    // the audio thread may be inside process(), and the engine's parameter
    // structs are plain values it reads without synchronisation. The next
    // processBlock picks the restored patch up on the thread that owns them.
    ScopedParameterWrite write { *this };
    parameters.replaceState (state);

    // The restored legacy values are the new baseline on both sides of the
    // bridge. Without this the first tick after a restore reads them as a fresh
    // edit and forwards them over the pair the session actually saved.
    lastLegacyKeyMode = restoredKeyMode;
    lastLegacyChorus = restoredChorusMode;

    forwardedLegacyKeyMode.store (restoredKeyMode, std::memory_order_relaxed);
    const int keyRestoreGeneration =
        keyModeForwardGeneration.fetch_add (1, std::memory_order_release) + 1;
    forwardedLegacyChorus.store (restoredChorusMode, std::memory_order_relaxed);
    const int chorusRestoreGeneration =
        chorusForwardGeneration.fetch_add (1, std::memory_order_release) + 1;

    restoredLegacyKeyMode.store (restoredKeyMode, std::memory_order_relaxed);
    restoredLegacyChorus.store (restoredChorusMode, std::memory_order_relaxed);
    restoredKeyModeForwardGeneration.store (keyRestoreGeneration,
                                             std::memory_order_relaxed);
    restoredChorusForwardGeneration.store (chorusRestoreGeneration,
                                            std::memory_order_relaxed);
    reseedLegacyBridges.store (true, std::memory_order_release);
    currentProgram.store (restoredProgram, std::memory_order_relaxed);
    // Publish the parameter tree and its matching bridge baseline as one
    // stable snapshot. Ending the write before the baseline was published let
    // the audio thread briefly interpret a restore as fresh legacy automation.
}

juce::AudioProcessorEditor* YouKnow106AudioProcessor::createEditor()
{
    return new YouKnow106AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YouKnow106AudioProcessor();
}

// ---------------------------------------------------------------------------
// Patches and the factory bank
// ---------------------------------------------------------------------------

int YouKnow106AudioProcessor::hostProgramIndexForMidiProgram (
    int midiProgram) noexcept
{
    // Owner-manual Patch Selection numbering is zero based and contiguous:
    // 0..63 are bank A, 64..127 are bank B. Program zero in the plug-in is
    // INIT, hence the one-based returned index.
    return midiProgram >= 0 && midiProgram < youknow106::presets::presetCount
        ? midiProgram + 1
        : -1;
}

// Program 0 is the init patch -- the layout's own defaults, which is what a
// freshly constructed processor actually holds. Without it, a new instance
// would report "A11 Brass Set 1" while sounding like nothing of the sort.
// The factory bank follows from index 1.
int YouKnow106AudioProcessor::getNumPrograms()
{
    return youknow106::presets::presetCount + 1;
}

int YouKnow106AudioProcessor::getCurrentProgram()
{
    return currentProgram.load (std::memory_order_relaxed);
}

void YouKnow106AudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;

    // A host program is a complete product preset. Publish its index and every
    // visible control as one stable generation so a concurrent state save can
    // retry instead of serialising half of each program.
    ScopedParameterWrite write { *this };
    if (index == 0)
    {
        applyProgramValues (youknow106::sysex::Patch {},
                            youknow106::presets::Preset::Controls {});
    }
    else
    {
        const auto& bank = youknow106::presets::factoryBank();
        const auto& preset = bank[static_cast<std::size_t> (index - 1)];
        applyProgramValues (preset.patch, preset.controls);
    }
    currentProgram.store (index, std::memory_order_relaxed);
}

void YouKnow106AudioProcessor::applyMidiProgramSelection (int index)
{
    if (index <= 0 || index >= getNumPrograms())
        return;

    // Incoming Program Change models the JUNO's own patch selector. Its
    // real-time shadow carries only tone memory, so reflection must retain the
    // same boundary instead of changing performance controls a timer tick later.
    ScopedParameterWrite write { *this };
    applyPatchValues (programPatch (index));
    currentProgram.store (index, std::memory_order_relaxed);
}

youknow106::sysex::Patch YouKnow106AudioProcessor::programPatch (int index) const
{
    if (index <= 0 || index >= youknow106::presets::presetCount + 1)
        return {};
    return youknow106::presets::factoryBank()[static_cast<std::size_t> (index - 1)].patch;
}

bool YouKnow106AudioProcessor::currentProgramIsEdited() const
{
    const auto panelPatch = currentPatch();
    const int selectedIndex = currentProgram.load (std::memory_order_relaxed);
    const auto selectedPatch = programPatch (selectedIndex);
    std::array<std::uint8_t, youknow106::sysex::toneByteCount> panel {}, program {};
    youknow106::sysex::toneBytesFromPatch (panelPatch, panel.data());
    youknow106::sysex::toneBytesFromPatch (selectedPatch, program.data());
    // The byte representation intentionally ignores sub-step slider movement,
    // which the hardware also quantises away. Compare the categorical chorus
    // mode explicitly too so compatibility values can never hide an edit.
    if (panel != program || panelPatch.chorus != selectedPatch.chorus)
        return true;

    const auto expected = selectedIndex > 0
        ? youknow106::presets::factoryBank()[static_cast<std::size_t> (
              selectedIndex - 1)].controls
        : youknow106::presets::Preset::Controls {};
    const auto differs = [] (float first, float second)
    {
        return std::abs (first - second) > 1.0e-5f;
    };
    using namespace youknow106::parameters;
    const bool panelPoly1 = valueOf (poly1) > 0.5f;
    const bool panelPoly2 = valueOf (poly2) > 0.5f;
    return differs (valueOf (volume), expected.volume)
        || differs (valueOf (benderDco), expected.benderDco)
        || differs (valueOf (benderVcf), expected.benderVcf)
        || differs (valueOf (benderLfo), expected.benderLfo)
        || differs (valueOf (portamento), expected.portamento)
        || panelPoly1 != youknow106::poly1Engaged (expected.keyMode)
        || panelPoly2 != youknow106::poly2Engaged (expected.keyMode)
        || juce::roundToInt (valueOf (transpose)) != expected.transpose
        || differs (valueOf (masterTune), expected.masterTune)
        || differs (valueOf (velocity), expected.velocity)
        || differs (valueOf (calibration), expected.calibration)
        || differs (valueOf (chorusNoise), expected.chorusNoise)
        // Quality is deliberately absent: it is not part of a preset, so it
        // cannot mark one as edited.
        || juce::roundToInt (valueOf (polyphony)) != expected.polyphony;
}

const juce::String YouKnow106AudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};
    if (index == 0)
        return "INIT";
    const auto& preset =
        youknow106::presets::factoryBank()[static_cast<std::size_t> (index - 1)];
    juce::String name = juce::String (preset.number) + " " + preset.name;
    return name;
}

void YouKnow106AudioProcessor::applyPatch (const youknow106::sysex::Patch& patch)
{
    ScopedParameterWrite write { *this };
    applyPatchValues (patch);
}

void YouKnow106AudioProcessor::applyPatchValues (
    const youknow106::sysex::Patch& patch)
{
    using namespace youknow106::parameters;

    const auto set = [this] (const char* id, float value)
    {
        if (auto* parameter = parameters.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set (lfoRate, patch.lfoRate);
    set (lfoDelay, patch.lfoDelay);
    set (dcoLfo, patch.dcoLfo);
    set (pwm, patch.pwm);
    set (noise, patch.noise);
    set (cutoff, patch.cutoff);
    set (resonance, patch.resonance);
    set (vcfEnv, patch.vcfEnv);
    set (vcfLfo, patch.vcfLfo);
    set (keyFollow, patch.keyFollow);
    set (vcaLevel, patch.vcaLevel);
    set (attack, patch.attack);
    set (decay, patch.decay);
    set (sustain, patch.sustain);
    set (release, patch.release);
    set (sub, patch.sub);

    set (range, static_cast<float> (patch.range));
    set (saw, patch.saw ? 1.0f : 0.0f);
    set (pulse, patch.pulse ? 1.0f : 0.0f);
    set (pwmMode, static_cast<float> (patch.pwmSource));
    set (vcaMode, static_cast<float> (patch.vcaMode));
    set (envPolarity, static_cast<float> (patch.envPolarity));
    set (highPass, static_cast<float> (patch.highPass));
    const auto chorus = patch.chorus == youknow106::ChorusMode::OneTwo
                           ? youknow106::ChorusMode::Two : patch.chorus;
    set (chorusI, youknow106::chorusOneEngaged (chorus) ? 1.0f : 0.0f);
    set (chorusII, youknow106::chorusTwoEngaged (chorus) ? 1.0f : 0.0f);

    // A compatibility automation write can arrive just before this recall and
    // still be waiting for both the audio bridge and the message-thread timer.
    // The patch is later, so establish that ordering before publishing the
    // stable parameter snapshot. Otherwise the pending legacy value would put
    // the just-recalled chorus back on its previous mode one block/tick later.
    publishChorusPairAuthority();

    // Volume, the bender depths, portamento and the assign mode are performance
    // controls. The instrument does not store them in a patch, so loading one
    // deliberately leaves them where the player set them.
}

void YouKnow106AudioProcessor::applyProgramValues (
    const youknow106::sysex::Patch& patch,
    const youknow106::presets::Preset::Controls& controls)
{
    using namespace youknow106::parameters;

    const auto set = [this] (const char* id, float value)
    {
        if (auto* parameter = parameters.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    const auto chorus = patch.chorus == youknow106::ChorusMode::OneTwo
                          ? youknow106::ChorusMode::Two : patch.chorus;
    set (legacyKeyMode, static_cast<float> (controls.keyMode));
    set (legacyChorus, static_cast<float> (chorus));

    set (volume, controls.volume);
    set (benderDco, controls.benderDco);
    set (benderVcf, controls.benderVcf);
    set (benderLfo, controls.benderLfo);
    set (portamento, controls.portamento);
    set (poly1, youknow106::poly1Engaged (controls.keyMode) ? 1.0f : 0.0f);
    set (poly2, youknow106::poly2Engaged (controls.keyMode) ? 1.0f : 0.0f);
    publishKeyModePairAuthority();

    set (transpose, static_cast<float> (controls.transpose));
    set (masterTune, controls.masterTune);
    set (velocity, controls.velocity);
    set (calibration, controls.calibration);
    set (chorusNoise, controls.chorusNoise);
    set (polyphony, static_cast<float> (controls.polyphony));
    // Quality is deliberately not recalled: see Preset::Controls.

    applyPatchValues (patch);
}

youknow106::sysex::Patch YouKnow106AudioProcessor::patchFromEngineParameters (
    const youknow106::EngineParameters& source) noexcept
{
    youknow106::sysex::Patch patch {};
    patch.lfoRate = source.lfoRate;
    patch.lfoDelay = source.lfoDelay;
    patch.dcoLfo = source.dcoLfoDepth;
    patch.pwm = source.pwmDepth;
    patch.noise = source.noiseLevel;
    patch.cutoff = source.cutoff;
    patch.resonance = source.resonance;
    patch.vcfEnv = source.envDepth;
    patch.vcfLfo = source.vcfLfoDepth;
    patch.keyFollow = source.keyFollow;
    patch.vcaLevel = source.vcaLevel;
    patch.attack = source.attack;
    patch.decay = source.decay;
    patch.sustain = source.sustain;
    patch.release = source.release;
    patch.sub = source.subLevel;
    patch.range = source.range;
    patch.saw = source.sawEnabled;
    patch.pulse = source.pulseEnabled;
    patch.pwmSource = source.pwmSource;
    patch.vcaMode = source.vcaMode;
    patch.envPolarity = source.envPolarity;
    patch.highPass = source.highPass;
    patch.chorus = source.chorus;
    return patch;
}

void YouKnow106AudioProcessor::applyPatchToEngineParameters (
    youknow106::EngineParameters& destination,
    const youknow106::sysex::Patch& patch) noexcept
{
    destination.lfoRate = patch.lfoRate;
    destination.lfoDelay = patch.lfoDelay;
    destination.dcoLfoDepth = patch.dcoLfo;
    destination.pwmDepth = patch.pwm;
    destination.noiseLevel = patch.noise;
    destination.cutoff = patch.cutoff;
    destination.resonance = patch.resonance;
    destination.envDepth = patch.vcfEnv;
    destination.vcfLfoDepth = patch.vcfLfo;
    destination.keyFollow = patch.keyFollow;
    destination.vcaLevel = patch.vcaLevel;
    destination.attack = patch.attack;
    destination.decay = patch.decay;
    destination.sustain = patch.sustain;
    destination.release = patch.release;
    destination.subLevel = patch.sub;
    destination.range = patch.range;
    destination.sawEnabled = patch.saw;
    destination.pulseEnabled = patch.pulse;
    destination.pwmSource = patch.pwmSource;
    destination.vcaMode = patch.vcaMode;
    destination.envPolarity = patch.envPolarity;
    destination.highPass = patch.highPass;
    destination.chorus = patch.chorus == youknow106::ChorusMode::OneTwo
                           ? youknow106::ChorusMode::Two : patch.chorus;
}

void YouKnow106AudioProcessor::applyPendingMidiEventToEngine (
    const PendingMidiEvent& event) noexcept
{
    if (! pendingMidiToneShadowActive)
        pendingMidiToneShadow = patchFromEngineParameters (audioBaseParameters);

    if (event.kind == PendingMidiEventKind::FullPatch)
        pendingMidiToneShadow =
            youknow106::sysex::patchFromToneBytes (event.bytes.data());
    else if (event.kind == PendingMidiEventKind::SingleParameter)
        (void) youknow106::sysex::applyParameter (
            pendingMidiToneShadow, event.parameter, event.value);
    else if (event.kind == PendingMidiEventKind::ProgramChange)
        pendingMidiToneShadow = programPatch (event.programIndex);
    else
        pendingMidiToneShadow = event.snapshot;

    pendingMidiToneShadowActive = true;
    pendingMidiToneShadowSequence = event.sequence;

    auto immediate = audioBaseParameters;
    applyPatchToEngineParameters (immediate, pendingMidiToneShadow);
    engine.setParameters (immediate);
}

youknow106::sysex::Patch YouKnow106AudioProcessor::currentPatch() const
{
    using namespace youknow106::parameters;

    youknow106::sysex::Patch patch {};
    patch.lfoRate = valueOf (lfoRate);
    patch.lfoDelay = valueOf (lfoDelay);
    patch.dcoLfo = valueOf (dcoLfo);
    patch.pwm = valueOf (pwm);
    patch.noise = valueOf (noise);
    patch.cutoff = valueOf (cutoff);
    patch.resonance = valueOf (resonance);
    patch.vcfEnv = valueOf (vcfEnv);
    patch.vcfLfo = valueOf (vcfLfo);
    patch.keyFollow = valueOf (keyFollow);
    patch.vcaLevel = valueOf (vcaLevel);
    patch.attack = valueOf (attack);
    patch.decay = valueOf (decay);
    patch.sustain = valueOf (sustain);
    patch.release = valueOf (release);
    patch.sub = valueOf (sub);

    patch.range = static_cast<youknow106::DcoRange> (choiceOf (range, 2));
    patch.saw = valueOf (saw) > 0.5f;
    patch.pulse = valueOf (pulse) > 0.5f;
    patch.pwmSource = static_cast<youknow106::PwmSource> (choiceOf (pwmMode, 1));
    patch.vcaMode = static_cast<youknow106::VcaMode> (choiceOf (vcaMode, 1));
    patch.envPolarity =
        static_cast<youknow106::EnvPolarity> (choiceOf (envPolarity, 1));
    patch.highPass = static_cast<youknow106::HighPassMode> (choiceOf (highPass, 3));
    patch.chorus = youknow106::chorusModeFor (valueOf (chorusI) > 0.5f,
                                              valueOf (chorusII) > 0.5f);
    return patch;
}

juce::MidiMessage YouKnow106AudioProcessor::currentPatchAsSysEx (int channel) const
{
    std::array<std::uint8_t, youknow106::sysex::patchMessageBytes> raw {};
    const auto written = youknow106::sysex::writePatchMessage (
        currentPatch(), channel, raw.data(), raw.size());
    if (written == 0)
        return {};
    // JUCE wants the body without the leading F0 and trailing F7.
    return juce::MidiMessage::createSysExMessage (raw.data() + 1,
                                                  static_cast<int> (written) - 2);
}

// Message thread. A .syx file is the live stream written to disk, so it gets
// the live stream's parser: scan each F0..F7 span and let readPatchMessage
// keep its exact framing rules. Only the first patch is applied -- with one
// edit buffer and no user bank, applying a whole bank file in order would
// silently keep nothing but its last patch -- but every patch is counted so
// the editor can say what the file actually held.
bool YouKnow106AudioProcessor::importPatchSysExBytes (const void* data,
                                                      std::size_t size,
                                                      int& patchesFound)
{
    patchesFound = 0;
    const auto* bytes = static_cast<const std::uint8_t*> (data);
    if (bytes == nullptr)
        return false;

    bool applied = false;
    std::size_t index = 0;
    while (index < size)
    {
        if (bytes[index] != 0xf0)
        {
            ++index;
            continue;
        }

        std::size_t end = index + 1;
        while (end < size && bytes[end] != 0xf7)
            ++end;
        if (end == size)
            break;

        youknow106::sysex::Patch patch {};
        int channel = 0;
        if (youknow106::sysex::readPatchMessage (bytes + index, end - index + 1,
                                                 patch, channel))
        {
            ++patchesFound;
            if (!applied)
            {
                // Mirror the live path: remember the dump's channel so later
                // exports and replies speak it back.
                sysExChannel.store (channel, std::memory_order_relaxed);
                applyPatch (patch);
                applied = true;
            }
        }
        index = end + 1;
    }
    return applied;
}

// Audio thread. Claims the next slot, fills it, then publishes it. A slot is
// only written while its `ready` flag is clear, so the message thread is never
// reading the event this is writing.
bool YouKnow106AudioProcessor::stagePendingMidiEvent (
    const PendingMidiEvent& event) noexcept
{
    const int index = pendingMidiWriteIndex.load (std::memory_order_relaxed);
    auto& slot = pendingMidiQueue[static_cast<std::size_t> (index)];
    if (slot.ready.load (std::memory_order_acquire))
    {
        // The message thread has not caught up. Never overwrite a published
        // slot: its reader may be partway through copying it. The caller keeps
        // rendering every event and coalesces reflection into one later slot.
        return false;
    }

    slot.event = event;
    slot.ready.store (true, std::memory_order_release);
    pendingMidiWriteIndex.store ((index + 1) % pendingMidiQueueSlots,
                                 std::memory_order_relaxed);
    // Deliberately no wake-up call from here: the consumer polls. Anything that
    // signals the message thread takes a lock, and this is the audio callback.
    return true;
}

void YouKnow106AudioProcessor::stageAndApplyPendingMidiEvent (
    PendingMidiEvent event) noexcept
{
    event.sequence = nextPendingMidiSequence + 1;
    nextPendingMidiSequence = event.sequence;
    if (event.kind == PendingMidiEventKind::ProgramChange)
    {
        pendingMidiLatestProgramIndex = event.programIndex;
        pendingMidiLatestProgramSequence = event.sequence;
    }

    // DSP timing is independent of message-thread capacity. Even if APVTS
    // reflection is already behind, this event belongs at its MIDI sample.
    applyPendingMidiEventToEngine (event);

    const auto includeInResync = [this, &event]
    {
        if (event.kind == PendingMidiEventKind::FullPatch
            || event.kind == PendingMidiEventKind::ProgramChange)
            pendingMidiResyncReplacesTone = true;
        else if (event.kind == PendingMidiEventKind::SingleParameter)
            pendingMidiResyncTouchedToneBytes |=
                1u << static_cast<unsigned> (event.parameter);
    };

    if (pendingMidiNeedsResync)
    {
        includeInResync();
        return;
    }

    if (stagePendingMidiEvent (event))
        return;

    // The first event that does not fit starts a coalescing epoch. All later
    // events continue to update pendingMidiToneShadow, but none may enter the
    // FIFO ahead of the complete snapshot which represents this gap.
    pendingMidiNeedsResync = true;
    pendingMidiResyncReplacesTone = false;
    pendingMidiResyncTouchedToneBytes = 0;
    includeInResync();
}

void YouKnow106AudioProcessor::tryStagePendingMidiResync() noexcept
{
    if (! pendingMidiNeedsResync)
        return;

    // The timer may already have consumed the atomic fallback mailbox after
    // the previous block ended. In that case there is no gap left to enqueue.
    if (reflectedMidiSequence.load (std::memory_order_acquire)
            >= pendingMidiToneShadowSequence)
    {
        pendingMidiNeedsResync = false;
        pendingMidiResyncReplacesTone = false;
        pendingMidiResyncTouchedToneBytes = 0;
        return;
    }

    PendingMidiEvent event;
    event.kind = PendingMidiEventKind::ResyncSnapshot;
    event.sequence = pendingMidiToneShadowSequence;
    event.snapshot = pendingMidiToneShadow;
    event.programIndex = pendingMidiLatestProgramIndex;
    event.programSequence = pendingMidiLatestProgramSequence;
    event.replacesTone = pendingMidiResyncReplacesTone;
    event.touchedToneBytes = pendingMidiResyncTouchedToneBytes;
    if (! stagePendingMidiEvent (event))
        return;

    pendingMidiNeedsResync = false;
    pendingMidiResyncReplacesTone = false;
    pendingMidiResyncTouchedToneBytes = 0;
}

void YouKnow106AudioProcessor::publishPendingMidiResyncMailbox() noexcept
{
    static_assert (sizeof (float) == sizeof (std::uint32_t));
    static_assert (std::atomic<std::uint32_t>::is_always_lock_free);
    static_assert (std::atomic<std::uint64_t>::is_always_lock_free);
    static_assert (std::atomic<int>::is_always_lock_free);

    const auto generation =
        pendingMidiResyncMailboxGeneration.load (std::memory_order_seq_cst);
    pendingMidiResyncMailboxGeneration.store (generation + 1,
                                               std::memory_order_seq_cst);

    const auto& patch = pendingMidiToneShadow;
    const float continuous[] = {
        patch.lfoRate, patch.lfoDelay, patch.dcoLfo, patch.pwm,
        patch.noise, patch.cutoff, patch.resonance, patch.vcfEnv,
        patch.vcfLfo, patch.keyFollow, patch.vcaLevel, patch.attack,
        patch.decay, patch.sustain, patch.release, patch.sub
    };
    for (std::size_t index = 0; index < pendingMidiResyncContinuous.size(); ++index)
        pendingMidiResyncContinuous[index].store (
            std::bit_cast<std::uint32_t> (continuous[index]),
            std::memory_order_seq_cst);

    const std::uint32_t switches =
          (static_cast<std::uint32_t> (patch.range) & 0x3u)
        | (static_cast<std::uint32_t> (patch.saw) << 2u)
        | (static_cast<std::uint32_t> (patch.pulse) << 3u)
        | ((static_cast<std::uint32_t> (patch.pwmSource) & 0x1u) << 4u)
        | ((static_cast<std::uint32_t> (patch.vcaMode) & 0x1u) << 5u)
        | ((static_cast<std::uint32_t> (patch.envPolarity) & 0x1u) << 6u)
        | ((static_cast<std::uint32_t> (patch.highPass) & 0x3u) << 7u)
        | ((static_cast<std::uint32_t> (patch.chorus) & 0x3u) << 9u);
    pendingMidiResyncSwitches.store (switches, std::memory_order_seq_cst);
    pendingMidiResyncMailboxFlags.store (
        pendingMidiResyncReplacesTone ? 1u : 0u,
        std::memory_order_seq_cst);
    pendingMidiResyncMailboxTouchedToneBytes.store (
        pendingMidiResyncTouchedToneBytes, std::memory_order_seq_cst);
    pendingMidiResyncMailboxProgramIndex.store (
        pendingMidiLatestProgramIndex, std::memory_order_seq_cst);
    pendingMidiResyncMailboxProgramSequence.store (
        pendingMidiLatestProgramSequence, std::memory_order_seq_cst);
    pendingMidiResyncMailboxSequence.store (
        pendingMidiToneShadowSequence, std::memory_order_seq_cst);

    // The even release publishes one coherent snapshot. Every payload field is
    // itself atomic, so a reader which detects a concurrent rewrite can safely
    // discard its partial copy and retry without invoking undefined behaviour.
    pendingMidiResyncMailboxGeneration.store (generation + 2,
                                               std::memory_order_seq_cst);
}

bool YouKnow106AudioProcessor::readPendingMidiResyncMailbox (
    PendingMidiEvent& event) const noexcept
{
    // Never wait on the audio thread. If it is pre-empted midway through a
    // publication, the 20 ms timer can simply try again on its next tick.
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const auto before =
            pendingMidiResyncMailboxGeneration.load (std::memory_order_seq_cst);
        if ((before & 1u) != 0u)
            continue;

        youknow106::sysex::Patch patch {};
        float continuous[16] {};
        for (std::size_t index = 0; index < pendingMidiResyncContinuous.size();
             ++index)
            continuous[index] = std::bit_cast<float> (
                pendingMidiResyncContinuous[index].load (
                    std::memory_order_seq_cst));
        const auto switches =
            pendingMidiResyncSwitches.load (std::memory_order_seq_cst);
        const auto flags =
            pendingMidiResyncMailboxFlags.load (std::memory_order_seq_cst);
        const auto touchedToneBytes =
            pendingMidiResyncMailboxTouchedToneBytes.load (
                std::memory_order_seq_cst);
        const int programIndex =
            pendingMidiResyncMailboxProgramIndex.load (std::memory_order_seq_cst);
        const auto programSequence =
            pendingMidiResyncMailboxProgramSequence.load (
                std::memory_order_seq_cst);
        const auto sequence = pendingMidiResyncMailboxSequence.load (
            std::memory_order_seq_cst);

        const auto after =
            pendingMidiResyncMailboxGeneration.load (std::memory_order_seq_cst);
        if (before != after || (after & 1u) != 0u)
            continue;
        if (sequence == 0)
            return false;

        patch.lfoRate = continuous[0];
        patch.lfoDelay = continuous[1];
        patch.dcoLfo = continuous[2];
        patch.pwm = continuous[3];
        patch.noise = continuous[4];
        patch.cutoff = continuous[5];
        patch.resonance = continuous[6];
        patch.vcfEnv = continuous[7];
        patch.vcfLfo = continuous[8];
        patch.keyFollow = continuous[9];
        patch.vcaLevel = continuous[10];
        patch.attack = continuous[11];
        patch.decay = continuous[12];
        patch.sustain = continuous[13];
        patch.release = continuous[14];
        patch.sub = continuous[15];
        patch.range = static_cast<youknow106::DcoRange> (switches & 0x3u);
        patch.saw = ((switches >> 2u) & 0x1u) != 0;
        patch.pulse = ((switches >> 3u) & 0x1u) != 0;
        patch.pwmSource = static_cast<youknow106::PwmSource> (
            (switches >> 4u) & 0x1u);
        patch.vcaMode = static_cast<youknow106::VcaMode> (
            (switches >> 5u) & 0x1u);
        patch.envPolarity = static_cast<youknow106::EnvPolarity> (
            (switches >> 6u) & 0x1u);
        patch.highPass = static_cast<youknow106::HighPassMode> (
            (switches >> 7u) & 0x3u);
        patch.chorus = static_cast<youknow106::ChorusMode> (
            (switches >> 9u) & 0x3u);

        event = {};
        event.kind = PendingMidiEventKind::ResyncSnapshot;
        event.sequence = sequence;
        event.snapshot = patch;
        event.programIndex = programIndex;
        event.programSequence = programSequence;
        event.replacesTone = (flags & 0x1u) != 0;
        event.touchedToneBytes = touchedToneBytes;
        return true;
    }
    return false;
}

void YouKnow106AudioProcessor::timerCallback()
{
    if (keyboardResetRequested.exchange (false, std::memory_order_acq_rel))
        keyboardState.reset();
    drainPendingMidiQueue();
    forwardLegacyModeParameters();
}

void YouKnow106AudioProcessor::publishKeyModePairAuthority() noexcept
{
    const int legacy = choiceOf (youknow106::parameters::legacyKeyMode, 2);
    lastLegacyKeyMode.store (legacy, std::memory_order_relaxed);
    forwardedLegacyKeyMode.store (legacy, std::memory_order_relaxed);
    keyModeForwardGeneration.fetch_add (1, std::memory_order_release);
}

void YouKnow106AudioProcessor::publishChorusPairAuthority() noexcept
{
    const int legacy = choiceOf (youknow106::parameters::legacyChorus, 2);
    lastLegacyChorus.store (legacy, std::memory_order_relaxed);
    forwardedLegacyChorus.store (legacy, std::memory_order_relaxed);
    chorusForwardGeneration.fetch_add (1, std::memory_order_release);
}

// Which mode the audio should render this block, given the legacy id, the pair
// that replaced it, and what this thread has seen before.
//
// Only a *change* in the legacy id takes control, and it gives control back the
// moment the pair reads the same -- so a lane that never touches the legacy id
// costs nothing, and one that does is heard in the very next block instead of
// after the message thread's next tick.
int YouKnow106AudioProcessor::resolveLegacyMode (int legacyValue, int fromPair,
                                                 int forwardedLegacyValue,
                                                 int forwardGeneration,
                                                 LegacyBridge& bridge) noexcept
{
    if (legacyValue != bridge.seen)
    {
        bridge.seen = legacyValue;
        // The message thread may have forwarded this value before the audio
        // thread first observed it. In that order the pair is already the
        // authority -- especially if a subsequent preset recall changed the
        // pair again -- so do not start a hold on stale legacy data.
        const bool alreadyForwarded =
            forwardGeneration != bridge.seenForwardGeneration
            && forwardedLegacyValue == legacyValue;
        bridge.holding = !alreadyForwarded;
        bridge.held = legacyValue;
    }

    if (bridge.holding)
    {
        const bool matchingForward =
            forwardGeneration != bridge.seenForwardGeneration
            && forwardedLegacyValue == bridge.held;
        if (fromPair == bridge.held || matchingForward)
            bridge.holding = false;
        else
        {
            bridge.seenForwardGeneration = forwardGeneration;
            return bridge.held;
        }
    }

    bridge.seenForwardGeneration = forwardGeneration;
    return fromPair;
}

// A lane automating one of the previous release's ids moves the pair that
// replaced it. Only a *change* forwards, so the bridge never fights the panel:
// touching POLY 2 directly does not get overwritten on the next tick.
void YouKnow106AudioProcessor::forwardLegacyModeParameters()
{
    using namespace youknow106::parameters;

    const auto set = [this] (const char* id, bool on)
    {
        if (auto* target = parameters.getParameter (id))
        {
            const float wanted = on ? 1.0f : 0.0f;
            if (std::abs (target->convertFrom0to1 (target->getValue()) - wanted) > 0.5f)
                target->setValueNotifyingHost (target->convertTo0to1 (wanted));
        }
    };

    const int keyMode = choiceOf (legacyKeyMode, 2);
    if (keyMode != lastLegacyKeyMode.load (std::memory_order_relaxed))
    {
        // The pair has two public parameters but is one firmware transition.
        // Keep the audio thread on its previous stable snapshot until both
        // writes and their bridge publication are complete.
        ScopedParameterWrite write { *this };
        lastLegacyKeyMode.store (keyMode, std::memory_order_relaxed);
        const auto mode = static_cast<youknow106::KeyMode> (keyMode);
        set (poly1, youknow106::poly1Engaged (mode));
        set (poly2, youknow106::poly2Engaged (mode));
        forwardedLegacyKeyMode.store (keyMode, std::memory_order_relaxed);
        keyModeForwardGeneration.fetch_add (1, std::memory_order_release);
    }

    const int chorusMode = choiceOf (legacyChorus, 2);
    if (chorusMode != lastLegacyChorus.load (std::memory_order_relaxed))
    {
        ScopedParameterWrite write { *this };
        lastLegacyChorus.store (chorusMode, std::memory_order_relaxed);
        const auto mode = static_cast<youknow106::ChorusMode> (chorusMode);
        set (chorusI, youknow106::chorusOneEngaged (mode));
        set (chorusII, youknow106::chorusTwoEngaged (mode));
        forwardedLegacyChorus.store (chorusMode, std::memory_order_relaxed);
        chorusForwardGeneration.fetch_add (1, std::memory_order_release);
    }

    // Direct host automation can bypass the editor's button interlock. The
    // audio side already interprets both high as II; bring the public state and
    // lamps to the same canonical value on this message-thread tick.
    if (valueOf (chorusI) > 0.5f && valueOf (chorusII) > 0.5f)
        if (auto* first = parameters.getParameter (chorusI))
            first->setValueNotifyingHost (first->convertTo0to1 (0.0f));

    // Likewise, direct automation can clear both assign lamps even though the
    // firmware always leaves Poly 1, Poly 2 or both latched. The audio fallback
    // for 00 is already Poly 1; repair the public pair to match it.
    if (valueOf (poly1) <= 0.5f && valueOf (poly2) <= 0.5f)
        if (auto* first = parameters.getParameter (poly1))
            first->setValueNotifyingHost (first->convertTo0to1 (1.0f));
}

void YouKnow106AudioProcessor::reflectPendingMidiEvent (
    const PendingMidiEvent& event)
{
    if (event.kind == PendingMidiEventKind::FullPatch)
        applyPatch (youknow106::sysex::patchFromToneBytes (event.bytes.data()));
    else if (event.kind == PendingMidiEventKind::SingleParameter)
        applyToneParameter (event.parameter, event.value);
    else if (event.kind == PendingMidiEventKind::ProgramChange)
    {
        applyMidiProgramSelection (event.programIndex);
        reflectedMidiProgramSequence = event.sequence;
        // This is a host/UI state change, distinct from transmitting a MIDI
        // Program Change. No MIDI output event is generated.
        updateHostDisplay (
            juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    }
    else
    {
        // Granular history overflowed. A patch/program event in the gap owns
        // the complete tone; an all-parameter-message gap owns only the bytes
        // it touched, so delayed reflection cannot overwrite unrelated newer
        // UI automation. Never call setCurrentProgram here: a later hardware
        // edit may have made that selected program intentionally edited.
        const bool hasNewProgram =
            event.programIndex >= 0
            && event.programSequence > reflectedMidiProgramSequence;
        {
            ScopedParameterWrite write { *this };
            if (event.replacesTone)
                applyPatchValues (event.snapshot);
            else
                for (int parameter = 0;
                     parameter < youknow106::sysex::toneByteCount; ++parameter)
                    if ((event.touchedToneBytes
                         & (1u << static_cast<unsigned> (parameter))) != 0)
                        applyToneParameterValues (
                            parameter, youknow106::sysex::parameterValue (
                                           event.snapshot, parameter));
            if (hasNewProgram)
                currentProgram.store (event.programIndex,
                                      std::memory_order_relaxed);
        }

        if (hasNewProgram)
        {
            reflectedMidiProgramSequence = event.programSequence;
            updateHostDisplay (
                juce::AudioProcessorListener::ChangeDetails()
                    .withProgramChanged (true));
        }
    }
}

void YouKnow106AudioProcessor::drainPendingMidiQueue()
{
    // Drain everything published so far. Single-parameter edits are cumulative,
    // so applying only the newest would lose the rest.
    for (int drained = 0; drained < pendingMidiQueueSlots; ++drained)
    {
        auto& slot = pendingMidiQueue[static_cast<std::size_t> (pendingMidiReadIndex)];
        if (! slot.ready.load (std::memory_order_acquire))
            break;

        const auto event = slot.event;
        // Release the slot only after the event has been copied out of it.
        slot.ready.store (false, std::memory_order_release);
        pendingMidiReadIndex = (pendingMidiReadIndex + 1) % pendingMidiQueueSlots;

        // A mailbox snapshot may already have reflected this same sequence
        // while the audio thread was concurrently putting it into a freed FIFO
        // slot. Never apply that duplicate over a newer reflected event.
        if (event.sequence
            <= reflectedMidiSequence.load (std::memory_order_acquire))
            continue;

        reflectPendingMidiEvent (event);

        // Only now does APVTS/currentProgram contain this event. The audio
        // thread may stop overlaying its shadow once it observes an
        // acknowledgement at least as new as the shadow it is rendering.
        reflectedMidiSequence.store (event.sequence, std::memory_order_release);
    }

    // Do not jump a mailbox snapshot over FIFO entries which arrived while
    // this bounded drain was running. Once the ring is genuinely empty, every
    // published resync is later than the prefix just reflected.
    auto& next = pendingMidiQueue[static_cast<std::size_t> (pendingMidiReadIndex)];
    if (next.ready.load (std::memory_order_acquire))
        return;

    PendingMidiEvent resync;
    if (! readPendingMidiResyncMailbox (resync))
        return;

    // Re-check after acquiring the coherent mailbox. If the audio thread
    // staged an earlier snapshot before publishing this newer mailbox, its
    // ready release is ordered before the mailbox publication and this acquire
    // must observe it. Apply that FIFO prefix first; otherwise advancing the
    // acknowledgement to the mailbox could skip its Program Change metadata.
    if (next.ready.load (std::memory_order_acquire)
        || resync.sequence
               <= reflectedMidiSequence.load (std::memory_order_acquire))
        return;

    reflectPendingMidiEvent (resync);
    reflectedMidiSequence.store (resync.sequence, std::memory_order_release);
}

// One tone parameter, applied to the controls it names and to nothing else.
//
// Writing the whole patch back would notify the host about parameters the
// message never mentioned, and would race automation: a lane moving another
// control between the read and the write would be reverted by the stale value
// that came back with it.
void YouKnow106AudioProcessor::applyToneParameter (int parameter, int value)
{
    if (parameter < 0 || parameter >= youknow106::sysex::toneByteCount)
        return;

    ScopedParameterWrite write { *this };
    applyToneParameterValues (parameter, value);
}

void YouKnow106AudioProcessor::applyToneParameterValues (int parameter, int value)
{
    using namespace youknow106::parameters;

    const auto set = [this] (const char* id, float newValue)
    {
        if (auto* target = parameters.getParameter (id))
            target->setValueNotifyingHost (target->convertTo0to1 (newValue));
    };

    if (parameter < 0 || parameter >= youknow106::sysex::toneByteCount)
        return;

    // The sixteen continuous controls, in the order the message carries them.
    static constexpr const char* continuous[] = {
        lfoRate, lfoDelay, dcoLfo, pwm, noise, cutoff, resonance, vcfEnv,
        vcfLfo, keyFollow, vcaLevel, attack, decay, sustain, release, sub
    };
    if (parameter < 16)
    {
        set (continuous[parameter], static_cast<float> (value & 0x7f) / 127.0f);
        return;
    }

    // A switch byte. Decode it against the patch it belongs to, then write out
    // only the switches that byte encodes.
    auto patch = currentPatch();
    if (! youknow106::sysex::applyParameter (patch, parameter, value))
        return;

    if (parameter == static_cast<int> (youknow106::sysex::ToneParameter::SwitchesOne))
    {
        set (range, static_cast<float> (patch.range));
        set (saw, patch.saw ? 1.0f : 0.0f);
        set (pulse, patch.pulse ? 1.0f : 0.0f);
        set (chorusI, youknow106::chorusOneEngaged (patch.chorus) ? 1.0f : 0.0f);
        set (chorusII, youknow106::chorusTwoEngaged (patch.chorus) ? 1.0f : 0.0f);
        publishChorusPairAuthority();
        return;
    }

    set (pwmMode, static_cast<float> (patch.pwmSource));
    set (vcaMode, static_cast<float> (patch.vcaMode));
    set (envPolarity, static_cast<float> (patch.envPolarity));
    set (highPass, static_cast<float> (patch.highPass));
}
