#include "EmbeddedGuitarBank.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace acustra::dense {
namespace {

constexpr double kPreAttackSeconds = 0.004;
constexpr double kPreRolledAttackFadeSeconds = 0.00075;
// Eight source frames are the shortest ramp whose added derivative remains
// below the 99th-percentile attack derivative in every embedded steel take.
constexpr double kImmediateAttackFadeSeconds = 8.0 / 44100.0;
constexpr float kRetriggerFadeMilliseconds = 5.0f;
constexpr float kInt16Scale = 1.0f / 32768.0f;
constexpr float kLayeredBankGain = 0.55f;

void setError(std::string* error, const char* message) {
    if (error != nullptr)
        *error = message;
}

bool decodeAscii85(std::vector<std::uint8_t>& output, std::string* error) {
    output.clear();
    output.reserve(generated::kPackedByteCount);
    std::uint32_t group[5]{};
    std::size_t groupSize = 0;
    std::size_t characterCount = 0;

    for (std::size_t chunk = 0; chunk < generated::kAscii85ChunkCount; ++chunk) {
        for (const unsigned char* source = reinterpret_cast<const unsigned char*>(generated::kAscii85Chunks[chunk]);
             *source != 0; ++source) {
            if (*source <= ' ')
                continue;
            if (*source < '!' || *source > 'u') {
                setError(error, "invalid ASCII85 character");
                return false;
            }
            group[groupSize++] = *source - '!';
            ++characterCount;
            if (groupSize != 5)
                continue;
            std::uint64_t value = 0;
            for (const auto digit : group)
                value = value * 85 + digit;
            if (value > std::numeric_limits<std::uint32_t>::max()) {
                setError(error, "invalid ASCII85 group");
                return false;
            }
            for (int shift = 24; shift >= 0 && output.size() < generated::kPackedByteCount; shift -= 8)
                output.push_back(static_cast<std::uint8_t>(value >> shift));
            groupSize = 0;
        }
    }
    if (groupSize != 0 || characterCount != generated::kAscii85CharacterCount
        || output.size() != generated::kPackedByteCount) {
        setError(error, "truncated ASCII85 payload");
        return false;
    }
    return true;
}

std::uint16_t readU16(const std::uint8_t*& source) noexcept {
    const auto value = static_cast<std::uint16_t>(source[0] | (source[1] << 8));
    source += 2;
    return value;
}

std::uint32_t readU32(const std::uint8_t*& source) noexcept {
    const auto value = static_cast<std::uint32_t>(source[0])
        | (static_cast<std::uint32_t>(source[1]) << 8)
        | (static_cast<std::uint32_t>(source[2]) << 16)
        | (static_cast<std::uint32_t>(source[3]) << 24);
    source += 4;
    return value;
}

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t bytes) noexcept : data_(data), bitCount_(bytes * 8) {}

    bool bit(std::uint32_t& value) noexcept {
        if (position_ >= bitCount_)
            return false;
        value = (data_[position_ / 8] >> (position_ % 8)) & 1u;
        ++position_;
        return true;
    }

    bool bits(std::uint8_t count, std::uint32_t& value) noexcept {
        value = 0;
        for (std::uint8_t index = 0; index < count; ++index) {
            std::uint32_t current{};
            if (!bit(current))
                return false;
            value |= current << index;
        }
        return true;
    }

private:
    const std::uint8_t* data_{};
    std::size_t bitCount_{};
    std::size_t position_{};
};

std::uint64_t hashPcm(const std::int16_t* samples, std::size_t count) noexcept {
    std::uint64_t value = 0xcbf29ce484222325ULL;
    for (std::size_t index = 0; index < count; ++index) {
        const auto sample = static_cast<std::uint16_t>(samples[index]);
        value ^= sample & 0xffu;
        value *= 0x100000001b3ULL;
        value ^= sample >> 8;
        value *= 0x100000001b3ULL;
    }
    return value;
}

bool decodeZone(const std::vector<std::uint8_t>& packed,
                const generated::PackedZoneRecord& record,
                std::int16_t* output,
                std::string* error) {
    if (record.channels != 1 && record.channels != 2) {
        setError(error, "unsupported channel count");
        return false;
    }
    const auto* source = packed.data() + record.packedOffset;
    const auto* const end = source + record.packedBytes;
    std::uint32_t frame = 0;
    while (frame < record.frames) {
        if (end - source < 2) {
            setError(error, "truncated Rice block");
            return false;
        }
        const auto count = readU16(source);
        if (count == 0 || frame + count > record.frames) {
            setError(error, "invalid Rice block frame count");
            return false;
        }
        for (std::uint8_t component = 0; component < record.channels; ++component) {
            if (end - source < 5) {
                setError(error, "truncated Rice component header");
                return false;
            }
            const auto k = *source++;
            const auto bytes = readU32(source);
            if (k > 20 || bytes > static_cast<std::uint32_t>(end - source)) {
                setError(error, "invalid Rice component");
                return false;
            }
            BitReader bits(source, bytes);
            source += bytes;
            std::int32_t previous = 0;
            for (std::uint16_t local = 0; local < count; ++local) {
                std::uint32_t quotient = 0;
                std::uint32_t current = 0;
                while (true) {
                    if (!bits.bit(current) || quotient > (1u << 20)) {
                        setError(error, "truncated Rice unary value");
                        return false;
                    }
                    if (current == 0)
                        break;
                    ++quotient;
                }
                std::uint32_t remainder = 0;
                if (!bits.bits(k, remainder)) {
                    setError(error, "truncated Rice remainder");
                    return false;
                }
                const auto encoded = (quotient << k) | remainder;
                const auto delta = static_cast<std::int32_t>((encoded >> 1) ^ (0u - (encoded & 1u)));
                const auto componentValue = static_cast<std::int64_t>(previous) + delta;
                previous = static_cast<std::int32_t>(componentValue);
                auto value = componentValue;
                const auto index = static_cast<std::size_t>(frame + local) * record.channels + component;
                if (component == 1)
                    value += output[index - 1];
                if (value < -32768 || value > 32767) {
                    setError(error, "Rice sample outside int16 range");
                    return false;
                }
                output[index] = static_cast<std::int16_t>(value);
            }
        }
        frame += count;
    }
    if (source != end) {
        setError(error, "trailing Rice bytes");
        return false;
    }
    return true;
}

float cubic(float y0, float y1, float y2, float y3, float fraction) noexcept {
    const auto a0 = y3 - y2 - y0 + y1;
    const auto a1 = y0 - y1 - a0;
    const auto a2 = y2 - y0;
    return ((a0 * fraction + a1) * fraction + a2) * fraction + y1;
}

} // namespace

bool Library::prepare(std::string* error) {
    if (ready())
        return true;

    std::vector<std::uint8_t> packed;
    if (!decodeAscii85(packed, error))
        return false;

    std::size_t sampleCount = 0;
    for (std::size_t index = 0; index < generated::kZoneCount; ++index) {
        const auto& record = generated::kZones[index];
        const bool unspecifiedLocation = record.physicalStringIndex == -1
            && record.capturedOpenMidi == -1 && record.capturedFret == -1;
        const bool specifiedLocation = record.physicalStringIndex >= 0
            && record.capturedOpenMidi >= 0 && record.capturedFret >= 0;
        if (!unspecifiedLocation
            && (!specifiedLocation
                || record.physicalStringIndex
                     >= static_cast<int>(Sampler::kVoiceCount)
                || record.capturedFret > 20
                || static_cast<int>(record.capturedOpenMidi)
                     + record.capturedFret != record.rootMidi
                || record.rootMidi < record.lowKey
                || record.rootMidi > record.highKey)) {
            setError(error, "invalid captured string location");
            return false;
        }
        if (record.packedOffset + record.packedBytes > packed.size()) {
            setError(error, "zone outside packed payload");
            return false;
        }
        sampleCount += static_cast<std::size_t>(record.frames) * record.channels;
    }

    samples_.assign(sampleCount, 0);
    zones_.resize(generated::kZoneCount);
    std::size_t sampleOffset = 0;
    for (std::size_t index = 0; index < generated::kZoneCount; ++index) {
        const auto& record = generated::kZones[index];
        auto* destination = samples_.data() + sampleOffset;
        if (!decodeZone(packed, record, destination, error)) {
            samples_.clear();
            zones_.clear();
            return false;
        }
        const auto count = static_cast<std::size_t>(record.frames) * record.channels;
        if (hashPcm(destination, count) != record.decodedHash) {
            setError(error, "decoded PCM hash mismatch");
            samples_.clear();
            zones_.clear();
            return false;
        }
        zones_[index] = {
            record.bank, record.name, record.lowKey, record.highKey, record.rootMidi, record.rootHz,
            record.sampleRate, record.channels, record.frames, record.onsetFrame, record.peak,
            record.lowVelocity, record.highVelocity, record.roundRobin,
            record.terminalFadeFrames, record.endJump, record.decodedHash, destination,
        };
        zones_[index].physicalStringIndex = record.physicalStringIndex;
        zones_[index].capturedOpenMidi = record.capturedOpenMidi;
        zones_[index].capturedFret = record.capturedFret;
        sampleOffset += count;
    }

    // The source session contains intentionally different timbres at each
    // velocity, but its captured levels and four takes are not calibrated.
    // Equalise one-second energy within each root so a continuous MIDI curve
    // owns loudness while the recordings retain the layer/RR timbre changes.
    for (const auto& reference : zones_) {
        if (reference.bank != Bank::SteelPicked || reference.roundRobin != 0
            || reference.lowVelocity != 1)
            continue;
        std::vector<double> levels;
        for (const auto& zone : zones_) {
            if (zone.bank != Bank::SteelPicked
                || zone.rootMidi != reference.rootMidi
                || zone.physicalStringIndex != reference.physicalStringIndex
                || zone.capturedOpenMidi != reference.capturedOpenMidi
                || zone.capturedFret != reference.capturedFret)
                continue;
            const auto first = std::min(zone.onsetFrame, zone.frames);
            const auto count = std::min<std::uint32_t>(
                zone.sampleRate, zone.frames - first);
            double energy = 0.0;
            for (std::uint32_t frame = 0; frame < count; ++frame) {
                double mono = 0.0;
                for (std::uint8_t channel = 0; channel < zone.channels; ++channel)
                    mono += zone.samples[(first + frame) * zone.channels + channel];
                mono /= zone.channels;
                energy += mono * mono;
            }
            levels.push_back(std::sqrt(energy / std::max<std::uint32_t>(1, count)));
        }
        std::sort(levels.begin(), levels.end());
        if (levels.empty())
            continue;
        const auto middle = levels.size() / 2;
        const double target = levels.size() % 2 != 0
            ? levels[middle] : 0.5 * (levels[middle - 1] + levels[middle]);
        for (auto& zone : zones_) {
            if (zone.bank != Bank::SteelPicked
                || zone.rootMidi != reference.rootMidi
                || zone.physicalStringIndex != reference.physicalStringIndex
                || zone.capturedOpenMidi != reference.capturedOpenMidi
                || zone.capturedFret != reference.capturedFret)
                continue;
            const auto first = std::min(zone.onsetFrame, zone.frames);
            const auto count = std::min<std::uint32_t>(
                zone.sampleRate, zone.frames - first);
            double energy = 0.0;
            for (std::uint32_t frame = 0; frame < count; ++frame) {
                double mono = 0.0;
                for (std::uint8_t channel = 0; channel < zone.channels; ++channel)
                    mono += zone.samples[(first + frame) * zone.channels + channel];
                mono /= zone.channels;
                energy += mono * mono;
            }
            const double level = std::sqrt(
                energy / std::max<std::uint32_t>(1, count));
            zone.playbackTrim = static_cast<float>(std::clamp(
                target / std::max(1.0, level), 0.25, 4.0));
        }
    }
    return true;
}

const ZoneView* Library::find(Bank bank, int midiNote, int midiVelocity,
                              std::uint8_t roundRobin,
                              int physicalStringIndex, int openMidi,
                              int fret) const noexcept {
    const ZoneView* best = nullptr;
    int bestScore = -1;
    for (const auto& zone : zones_) {
        if (zone.bank != bank
            || midiVelocity < zone.lowVelocity || midiVelocity > zone.highVelocity)
            continue;
        const bool generic = zone.physicalStringIndex < 0;
        const bool sameStringAndFret = !generic
            && zone.physicalStringIndex == physicalStringIndex
            && zone.capturedFret == fret;
        const bool exactLocation = sameStringAndFret
            && zone.capturedOpenMidi == openMidi;
        if ((generic && (midiNote < zone.lowKey || midiNote > zone.highKey))
            || (!generic && !sameStringAndFret))
            continue;
        const int score = (exactLocation ? 4 : sameStringAndFret ? 2 : 0)
            + (zone.roundRobin == roundRobin ? 1 : 0);
        if (score > bestScore) {
            best = &zone;
            bestScore = score;
        }
    }
    return best;
}

const ZoneView* Library::zone(std::size_t index) const noexcept {
    return index < zones_.size() ? &zones_[index] : nullptr;
}

std::size_t Library::packedBytes() noexcept {
    return generated::kPackedByteCount;
}

void Sampler::setOutputSampleRate(double sampleRate) noexcept {
    if (sampleRate >= 8000.0 && sampleRate <= 384000.0) {
        outputSampleRate_ = sampleRate;
        for (auto& voice : voices_) {
            updateStep(voice);
            voice.attackIncrement = static_cast<float>(
                1.0 / std::max(1.0,
                    static_cast<double>(voice.attackSeconds) * outputSampleRate_));
        }
        for (auto& voice : retiringVoices_) {
            updateStep(voice);
            voice.attackIncrement = static_cast<float>(
                1.0 / std::max(1.0,
                    static_cast<double>(voice.attackSeconds) * outputSampleRate_));
        }
    }
}

void Sampler::setPitchBendSemitones(float semitones) noexcept {
    pitchBendSemitones_ = std::clamp(semitones, -24.0f, 24.0f);
    for (auto& voice : voices_)
        updateStep(voice);
    for (auto& voice : retiringVoices_)
        updateStep(voice);
}

bool Sampler::noteOn(std::size_t voiceIndex, Bank bank, int midiNote,
                     float velocity, float pan, float stringAge,
                     int openMidi) noexcept {
    const auto bankIndex = static_cast<std::size_t>(bank);
    if (library_ == nullptr || voiceIndex >= voices_.size()
        || bankIndex >= roundRobin_.size()
        || midiNote < 0 || midiNote > 127 || outputSampleRate_ <= 0.0
        || !std::isfinite(velocity) || velocity <= 0.0f)
        return false;
    const auto midiVelocity = std::clamp(
        static_cast<int>(std::lround(std::clamp(velocity, 0.0f, 1.0f) * 127.0f)),
        1, 127);
    auto& bankRoundRobin = roundRobin_[bankIndex];
    const auto roundRobin = bankRoundRobin[static_cast<std::size_t>(midiNote)];
    const int physicalStringIndex = openMidi < 0
        ? -1 : static_cast<int>(voiceIndex);
    const int fret = openMidi < 0 ? -1 : midiNote - openMidi;
    const auto* zone = library_->find(bank, midiNote, midiVelocity, roundRobin,
                                      physicalStringIndex, openMidi, fret);
    if (zone == nullptr)
        return false;

    auto& selected = voices_[voiceIndex];
    if (selected.active) {
        auto* retiring = &*std::min_element(
            retiringVoices_.begin(), retiringVoices_.end(),
            [] (const Voice& left, const Voice& right) {
                if (left.active != right.active)
                    return !left.active;
                return left.releaseGain < right.releaseGain;
            });
        *retiring = selected;
        beginRelease(*retiring, kRetriggerFadeMilliseconds);
    }
    const auto targetHz = 440.0 * std::exp2((midiNote - 69) / 12.0);
    const auto ratio = targetHz / zone->rootHz;
    selected = {};
    selected.zone = zone;
    selected.baseStep = ratio * zone->sampleRate / outputSampleRate_;
    updateStep(selected);
    const auto sourceLeadFrames = selected.step * outputSampleRate_
                                * kPreAttackSeconds;
    selected.position = std::max(0.0,
        static_cast<double>(zone->onsetFrame) - sourceLeadFrames);
    if (bank == Bank::SteelPicked) {
        const auto continuousVelocity = static_cast<float>(midiVelocity) / 127.0f;
        selected.gain = kLayeredBankGain * zone->playbackTrim
                      * std::pow(continuousVelocity, 0.82f);
    } else {
        const auto peak = std::max(1.0f, static_cast<float>(zone->peak));
        const auto peakNormalisation = std::clamp(
            0.55f / (peak * kInt16Scale), 0.25f, 4.0f);
        selected.gain = std::pow(std::clamp(velocity, 0.0f, 1.0f), 0.82f)
                      * peakNormalisation;
    }
    selected.pan = std::clamp(pan, -1.0f, 1.0f);
    selected.attackGain = 0.0f;
    selected.attackSeconds = static_cast<float>(
        selected.position > 0.0
            ? kPreRolledAttackFadeSeconds : kImmediateAttackFadeSeconds);
    selected.attackIncrement = static_cast<float>(
        1.0 / std::max(1.0,
            static_cast<double>(selected.attackSeconds) * outputSampleRate_));
    selected.decayGain = 1.0f;
    const auto age = std::clamp(stringAge, 0.0f, 1.0f);
    selected.decayMultiplier = static_cast<float>(std::exp(
        std::log(0.5) * age / (3.0 * outputSampleRate_)));
    selected.releaseGain = 1.0f;
    selected.releaseMultiplier = 1.0f;
    selected.age = ++ageCounter_;
    selected.midiNote = midiNote;
    selected.releasing = false;
    selected.active = selected.gain > 0.0f;
    if (selected.active)
        bankRoundRobin[static_cast<std::size_t>(midiNote)]
            = static_cast<std::uint8_t>((roundRobin + 1) % 4);
    return selected.active;
}

void Sampler::beginRelease(Voice& voice, float releaseMilliseconds) noexcept {
    if (!voice.active)
        return;
    const auto seconds = std::max(0.001f, releaseMilliseconds * 0.001f);
    const auto multiplier = static_cast<float>(std::exp(std::log(1.0e-4) / (seconds * outputSampleRate_)));
    voice.releasing = true;
    voice.releaseMultiplier = multiplier;
}

void Sampler::noteOff(std::size_t voiceIndex, float releaseMilliseconds) noexcept {
    if (voiceIndex < voices_.size())
        beginRelease(voices_[voiceIndex], releaseMilliseconds);
}

void Sampler::releaseAll(float releaseMilliseconds) noexcept {
    for (auto& voice : voices_)
        beginRelease(voice, releaseMilliseconds);
}

void Sampler::allNotesOff() noexcept {
    for (auto& voice : voices_)
        voice = {};
    for (auto& voice : retiringVoices_)
        voice = {};
    for (auto& bank : roundRobin_)
        bank.fill(0);
}

bool Sampler::isActive(std::size_t voiceIndex) const noexcept {
    return voiceIndex < voices_.size() && voices_[voiceIndex].active;
}

const ZoneView* Sampler::activeZone(std::size_t voiceIndex) const noexcept {
    return isActive(voiceIndex) ? voices_[voiceIndex].zone : nullptr;
}

std::size_t Sampler::activeVoiceCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        voices_.begin(), voices_.end(),
        [] (const Voice& voice) { return voice.active; }));
}

void Sampler::updateStep(Voice& voice) noexcept {
    voice.step = voice.baseStep
        * std::exp2(static_cast<double>(pitchBendSemitones_) / 12.0);
}

void Sampler::process(float* left, float* right, std::size_t frames) noexcept {
    std::fill_n(left, frames, 0.0f);
    std::fill_n(right, frames, 0.0f);
    renderAdd(left, right, frames);
}

float Sampler::interpolate(const Voice& voice, std::uint8_t channel) const noexcept {
    const auto& zone = *voice.zone;
    const auto center = static_cast<std::int64_t>(voice.position);
    const auto fraction = static_cast<float>(voice.position - center);
    const auto at = [&](std::int64_t frame) {
        frame = std::clamp<std::int64_t>(frame, 0, zone.frames - 1);
        const auto actualChannel = zone.channels == 1 ? 0 : channel;
        return zone.samples[static_cast<std::size_t>(frame) * zone.channels + actualChannel] * kInt16Scale;
    };
    return cubic(at(center - 1), at(center), at(center + 1), at(center + 2), fraction);
}

void Sampler::renderAdd(float* left, float* right, std::size_t frames) noexcept {
    const auto render = [&] (auto& voices) {
        for (auto& voice : voices) {
            if (!voice.active)
                continue;
            for (std::size_t frame = 0; frame < frames; ++frame) {
                if (voice.position >= voice.zone->frames - 1.0 || voice.releaseGain < 1.0e-4f) {
                    voice.active = false;
                    break;
                }
                const auto amplitude = voice.gain * voice.attackGain
                                     * voice.decayGain * voice.releaseGain;
                const auto pan = 0.18f * voice.pan;
                left[frame] += interpolate(voice, 0) * amplitude * (1.0f - pan);
                right[frame] += interpolate(voice, 1) * amplitude * (1.0f + pan);
                voice.position += voice.step;
                voice.attackGain = std::min(
                    1.0f, voice.attackGain + voice.attackIncrement);
                voice.decayGain *= voice.decayMultiplier;
                if (voice.releasing)
                    voice.releaseGain *= voice.releaseMultiplier;
            }
        }
    };
    render(retiringVoices_);
    render(voices_);
}

} // namespace acustra::dense
