#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr OSType fourCC(char a, char b, char c, char d)
{
    return (static_cast<OSType>(static_cast<unsigned char>(a)) << 24U)
         | (static_cast<OSType>(static_cast<unsigned char>(b)) << 16U)
         | (static_cast<OSType>(static_cast<unsigned char>(c)) << 8U)
         | static_cast<OSType>(static_cast<unsigned char>(d));
}

bool check(OSStatus status, const char* operation)
{
    if (status == noErr)
        return true;

    std::cerr << "error: " << operation << " failed with OSStatus " << status << '\n';
    return false;
}

std::string stringFromCFString(CFStringRef string)
{
    if (string == nullptr)
        return {};

    std::array<char, 256> text {};
    return CFStringGetCString(string, text.data(), text.size(), kCFStringEncodingUTF8)
             ? std::string(text.data())
             : std::string {};
}

bool expectCurrentPreset(AudioUnit unit, SInt32 number, const char* name)
{
    AUPreset preset {};
    UInt32 size = sizeof(preset);
    if (!check(AudioUnitGetProperty(unit,
                                    kAudioUnitProperty_PresentPreset,
                                    kAudioUnitScope_Global,
                                    0,
                                    &preset,
                                    &size),
               "get current factory preset"))
        return false;

    const bool matches = preset.presetNumber == number
                      && stringFromCFString(preset.presetName) == name;
    if (preset.presetName != nullptr)
        CFRelease(preset.presetName);
    if (!matches)
        std::cerr << "error: current AU factory preset does not match " << number
                  << " ('" << name << "')\n";
    return matches;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: ElectryAUArtifactSmokeTests "
                     "/path/to/Electry.component/Contents/MacOS/Electry\n";
        return 2;
    }

    const std::string binaryPath(argv[1]);
    void* const bundle = dlopen(binaryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (bundle == nullptr)
    {
        std::cerr << "error: dlopen failed for " << binaryPath << ": " << dlerror() << '\n';
        return 1;
    }

    dlerror();
    void* const symbol = dlsym(bundle, "ElectryAUFactory");
    if (const char* error = dlerror(); error != nullptr)
    {
        std::cerr << "error: dlsym(ElectryAUFactory) failed: " << error << '\n';
        return 1;
    }

    AudioComponentFactoryFunction factory = nullptr;
    static_assert(sizeof(factory) == sizeof(symbol));
    std::memcpy(&factory, &symbol, sizeof(factory));

    const AudioComponentDescription expected {
        kAudioUnitType_MusicDevice,
        fourCC('E', 'l', 'c', '1'),
        fourCC('E', 'l', 't', 'r'),
        kAudioComponentFlag_SandboxSafe,
        0
    };
    const AudioComponent component = AudioComponentRegister(
        &expected, CFSTR("Electry exact-artifact validation"), 0x00010200, factory);
    if (component == nullptr)
    {
        std::cerr << "error: AudioComponentRegister failed\n";
        return 1;
    }

    AudioComponentDescription registered {};
    if (!check(AudioComponentGetDescription(component, &registered),
               "AudioComponentGetDescription")
        || registered.componentType != expected.componentType
        || registered.componentSubType != expected.componentSubType
        || registered.componentManufacturer != expected.componentManufacturer)
    {
        std::cerr << "error: registered AU identity does not match aumu/Elc1/Eltr\n";
        return 1;
    }

    AudioUnit unit = nullptr;
    if (!check(AudioComponentInstanceNew(component, &unit), "AudioComponentInstanceNew"))
        return 1;

    constexpr UInt32 blockSize = 256;
    if (!check(AudioUnitSetProperty(unit,
                                    kAudioUnitProperty_MaximumFramesPerSlice,
                                    kAudioUnitScope_Global,
                                    0,
                                    &blockSize,
                                    sizeof(blockSize)),
               "set maximum frames per slice"))
    {
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    AudioStreamBasicDescription format {};
    UInt32 formatSize = sizeof(format);
    if (!check(AudioUnitGetProperty(unit,
                                    kAudioUnitProperty_StreamFormat,
                                    kAudioUnitScope_Output,
                                    0,
                                    &format,
                                    &formatSize),
               "get output stream format"))
    {
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    format.mSampleRate = 48000.0;
    if (!check(AudioUnitSetProperty(unit,
                                    kAudioUnitProperty_StreamFormat,
                                    kAudioUnitScope_Output,
                                    0,
                                    &format,
                                    sizeof(format)),
               "set output stream format")
        || format.mFormatID != kAudioFormatLinearPCM
        || (format.mFormatFlags & kAudioFormatFlagIsFloat) == 0
        || format.mBitsPerChannel != 32
        || format.mChannelsPerFrame != 2)
    {
        std::cerr << "error: AU did not expose a renderable stereo 32-bit float output\n";
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    if (!check(AudioUnitInitialize(unit), "AudioUnitInitialize"))
    {
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    UInt32 parameterBytes = 0;
    Boolean parameterListWritable = false;
    if (!check(AudioUnitGetPropertyInfo(unit,
                                        kAudioUnitProperty_ParameterList,
                                        kAudioUnitScope_Global,
                                        0,
                                        &parameterBytes,
                                        &parameterListWritable),
               "get AU parameter-list size")
        || parameterBytes != 28 * sizeof(AudioUnitParameterID))
    {
        std::cerr << "error: AU does not expose exactly 28 global parameters\n";
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    std::vector<AudioUnitParameterID> parameterIDs(parameterBytes / sizeof(AudioUnitParameterID));
    if (!check(AudioUnitGetProperty(unit,
                                    kAudioUnitProperty_ParameterList,
                                    kAudioUnitScope_Global,
                                    0,
                                    parameterIDs.data(),
                                    &parameterBytes),
               "get AU parameter list"))
    {
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    std::vector<AudioUnitParameterValue> savedParameterValues(parameterIDs.size());
    CFArrayRef factoryPresets = nullptr;
    UInt32 presetBytes = sizeof(factoryPresets);
    const std::array<const char*, 4> expectedPresetNames {
        "Factory Default", "Drop-E Metal", "Mute / Dead DI", "Blues Rock Lead"
    };
    if (!check(AudioUnitGetProperty(unit,
                                    kAudioUnitProperty_FactoryPresets,
                                    kAudioUnitScope_Global,
                                    0,
                                    &factoryPresets,
                                    &presetBytes),
               "get AU factory presets")
        || factoryPresets == nullptr
        || CFArrayGetCount(factoryPresets) != static_cast<CFIndex>(expectedPresetNames.size()))
    {
        std::cerr << "error: AU does not expose exactly four factory presets (got "
                  << (factoryPresets != nullptr ? CFArrayGetCount(factoryPresets) : 0) << ")\n";
        if (factoryPresets != nullptr)
            CFRelease(factoryPresets);
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    for (CFIndex i = 0; i < CFArrayGetCount(factoryPresets); ++i)
    {
        const auto* const preset = static_cast<const AUPreset*>(
            CFArrayGetValueAtIndex(factoryPresets, i));
        if (preset == nullptr || preset->presetNumber != i
            || stringFromCFString(preset->presetName)
                != expectedPresetNames[static_cast<std::size_t>(i)])
        {
            std::cerr << "error: AU factory-preset contract drifted at index " << i << '\n';
            CFRelease(factoryPresets);
            AudioUnitUninitialize(unit);
            AudioComponentInstanceDispose(unit);
            return 1;
        }
    }
    CFRelease(factoryPresets);

    const AUPreset savedPreset { 0, CFSTR("Factory Default") };
    if (!check(AudioUnitSetProperty(unit,
                                    kAudioUnitProperty_PresentPreset,
                                    kAudioUnitScope_Global,
                                    0,
                                    &savedPreset,
                                    sizeof(savedPreset)),
               "select AU factory default before saving state")
        || !expectCurrentPreset(unit, 0, expectedPresetNames[0]))
    {
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    for (std::size_t i = 0; i < parameterIDs.size(); ++i)
    {
        if (!check(AudioUnitGetParameter(unit,
                                         parameterIDs[i],
                                         kAudioUnitScope_Global,
                                         0,
                                         &savedParameterValues[i]),
                   "snapshot AU parameter"))
        {
            AudioUnitUninitialize(unit);
            AudioComponentInstanceDispose(unit);
            return 1;
        }
    }

    CFPropertyListRef state = nullptr;
    UInt32 stateSize = sizeof(state);
    if (!check(AudioUnitGetProperty(unit,
                                    kAudioUnitProperty_ClassInfo,
                                    kAudioUnitScope_Global,
                                    0,
                                    &state,
                                    &stateSize),
               "get AU state")
        || state == nullptr)
    {
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    CFErrorRef stateError = nullptr;
    CFDataRef const stateData = CFPropertyListCreateData(kCFAllocatorDefault,
                                                         state,
                                                         kCFPropertyListBinaryFormat_v1_0,
                                                         0,
                                                         &stateError);
    const CFIndex serialisedStateBytes = stateData != nullptr ? CFDataGetLength(stateData) : 0;
    if (stateError != nullptr)
    {
        CFRelease(stateError);
        stateError = nullptr;
    }

    CFPropertyListRef const restoredState = stateData != nullptr
        ? CFPropertyListCreateWithData(kCFAllocatorDefault,
                                       stateData,
                                       kCFPropertyListImmutable,
                                       nullptr,
                                       &stateError)
        : nullptr;
    if (stateData == nullptr || serialisedStateBytes <= 0 || restoredState == nullptr)
    {
        std::cerr << "error: AU state did not survive property-list serialisation\n";
        if (stateError != nullptr)
            CFRelease(stateError);
        if (restoredState != nullptr)
            CFRelease(restoredState);
        if (stateData != nullptr)
            CFRelease(stateData);
        CFRelease(state);
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    if (stateError != nullptr)
        CFRelease(stateError);
    CFRelease(stateData);
    CFRelease(state);

    const AUPreset changedPreset { 1, CFSTR("Drop-E Metal") };
    if (!check(AudioUnitSetProperty(unit,
                                    kAudioUnitProperty_PresentPreset,
                                    kAudioUnitScope_Global,
                                    0,
                                    &changedPreset,
                                    sizeof(changedPreset)),
               "select a different AU factory preset")
        || !expectCurrentPreset(unit, 1, expectedPresetNames[1]))
    {
        CFRelease(restoredState);
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    AudioUnitParameterInfo firstParameterInfo {};
    UInt32 parameterInfoBytes = sizeof(firstParameterInfo);
    if (!check(AudioUnitGetProperty(unit,
                                    kAudioUnitProperty_ParameterInfo,
                                    kAudioUnitScope_Global,
                                    parameterIDs.front(),
                                    &firstParameterInfo,
                                    &parameterInfoBytes),
               "get AU parameter range"))
    {
        CFRelease(restoredState);
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    const auto originalFirstValue = savedParameterValues.front();
    const auto changedFirstValue = std::abs(originalFirstValue - firstParameterInfo.minValue)
                                     > std::abs(originalFirstValue - firstParameterInfo.maxValue)
                                 ? firstParameterInfo.minValue
                                 : firstParameterInfo.maxValue;
    if ((firstParameterInfo.flags & kAudioUnitParameterFlag_HasCFNameString) != 0
        && firstParameterInfo.cfNameString != nullptr)
        CFRelease(firstParameterInfo.cfNameString);

    AudioUnitParameterValue observedChangedValue = originalFirstValue;
    if (std::abs(changedFirstValue - originalFirstValue) <= 1.0e-5f
        || !check(AudioUnitSetParameter(unit,
                                        parameterIDs.front(),
                                        kAudioUnitScope_Global,
                                        0,
                                        changedFirstValue,
                                        0),
                  "change AU parameter before state restore")
        || !check(AudioUnitGetParameter(unit,
                                        parameterIDs.front(),
                                        kAudioUnitScope_Global,
                                        0,
                                        &observedChangedValue),
                  "verify changed AU parameter")
        || std::abs(observedChangedValue - originalFirstValue) <= 1.0e-5f)
    {
        std::cerr << "error: AU parameter mutation was not observable\n";
        CFRelease(restoredState);
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    if (!check(AudioUnitSetProperty(unit,
                                    kAudioUnitProperty_ClassInfo,
                                    kAudioUnitScope_Global,
                                    0,
                                    &restoredState,
                                    sizeof(restoredState)),
               "restore serialised AU state"))
    {
        CFRelease(restoredState);
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }
    CFRelease(restoredState);

    if (!expectCurrentPreset(unit, 0, expectedPresetNames[0]))
    {
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    for (std::size_t i = 0; i < parameterIDs.size(); ++i)
    {
        AudioUnitParameterValue restoredValue = 0.0f;
        const float tolerance = 1.0e-5f * std::max(1.0f, std::abs(savedParameterValues[i]));
        if (!check(AudioUnitGetParameter(unit,
                                         parameterIDs[i],
                                         kAudioUnitScope_Global,
                                         0,
                                         &restoredValue),
                   "verify restored AU parameter")
            || std::abs(restoredValue - savedParameterValues[i]) > tolerance)
        {
            std::cerr << "error: serialised AU state did not restore all 28 parameters\n";
            AudioUnitUninitialize(unit);
            AudioComponentInstanceDispose(unit);
            return 1;
        }
    }

    const bool nonInterleaved = (format.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
    const UInt32 bufferCount = nonInterleaved ? format.mChannelsPerFrame : 1;
    const std::size_t listBytes = offsetof(AudioBufferList, mBuffers)
                                + sizeof(AudioBuffer) * bufferCount;
    std::vector<std::byte> listStorage(listBytes);
    auto* const buffers = reinterpret_cast<AudioBufferList*>(listStorage.data());
    buffers->mNumberBuffers = bufferCount;

    const UInt32 bytesPerBuffer = blockSize * format.mBytesPerFrame;
    std::vector<std::vector<std::byte>> audio(bufferCount,
                                              std::vector<std::byte>(bytesPerBuffer));
    for (UInt32 channel = 0; channel < bufferCount; ++channel)
    {
        buffers->mBuffers[channel].mNumberChannels = nonInterleaved
                                                   ? 1
                                                   : format.mChannelsPerFrame;
        buffers->mBuffers[channel].mDataByteSize = bytesPerBuffer;
        buffers->mBuffers[channel].mData = audio[channel].data();
    }

    if (!check(MusicDeviceMIDIEvent(unit, 0x90, 40, 100, 0), "send MIDI note-on"))
    {
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        return 1;
    }

    AudioTimeStamp timestamp {};
    timestamp.mFlags = kAudioTimeStampSampleTimeValid;
    double peak = 0.0;
    for (int block = 0; block < 8; ++block)
    {
        for (auto& channel : audio)
            std::fill(channel.begin(), channel.end(), std::byte {});

        AudioUnitRenderActionFlags flags = 0;
        if (!check(AudioUnitRender(unit, &flags, &timestamp, 0, blockSize, buffers),
                   "AudioUnitRender"))
        {
            AudioUnitUninitialize(unit);
            AudioComponentInstanceDispose(unit);
            return 1;
        }

        for (const auto& channel : audio)
        {
            const auto* const samples = reinterpret_cast<const float*>(channel.data());
            const std::size_t sampleCount = channel.size() / sizeof(float);
            for (std::size_t i = 0; i < sampleCount; ++i)
            {
                if (!std::isfinite(samples[i]))
                {
                    std::cerr << "error: AU render produced a non-finite sample\n";
                    AudioUnitUninitialize(unit);
                    AudioComponentInstanceDispose(unit);
                    return 1;
                }
                peak = std::max(peak, std::abs(static_cast<double>(samples[i])));
            }
        }
        timestamp.mSampleTime += blockSize;
    }

    check(MusicDeviceMIDIEvent(unit, 0x80, 40, 0, 0), "send MIDI note-off");
    AudioUnitUninitialize(unit);
    AudioComponentInstanceDispose(unit);

    if (peak <= 1.0e-7)
    {
        std::cerr << "error: AU render was silent after note-on\n";
        return 1;
    }

    std::cout << "PASS: exact AU binary loaded, registered as aumu/Elc1/Eltr, "
              << "state round-tripped (" << serialisedStateBytes << " bytes), peak=" << peak
              << '\n';
    return 0;
}
