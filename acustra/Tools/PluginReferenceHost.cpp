// Minimal local VST3 reference host. Requires an installed, normally licensed
// instrument and opaque state saved through its ordinary editor. No audio
// device is opened. Explicit BPM and 4/4 transport override MIDI tempo/meter
// metadata; only channel MIDI is accepted. Output is 48 kHz stereo float WAV.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <filesystem>
#include <iostream>

namespace
{
constexpr double sampleRate = 48000;
constexpr int blockSize = 128;

juce::File file(const char* path)
{
    return juce::File::getCurrentWorkingDirectory().getChildFile(path);
}

void requireNewFile(const juce::File& target)
{
    if (target.exists() || !target.getParentDirectory().isDirectory())
        throw std::runtime_error("output exists or its parent directory is missing");
}

// Both files are in the same directory. Publishing a hard link fails if the
// destination already exists, including one created after the initial check.
// TemporaryFile removes the private temporary name on success or exception.
void publish(const juce::TemporaryFile& temporary, const juce::File& target)
{
    std::filesystem::create_hard_link(
        temporary.getFile().getFullPathName().toStdString(),
        target.getFullPathName().toStdString());
}

double number(const char* text, double low, double high)
{
    std::size_t end = 0;
    const std::string token(text);
    const double value = std::stod(token, &end);
    if (end != token.size() || !std::isfinite(value) || value < low || value > high)
        throw std::runtime_error("invalid numeric argument: " + token);
    return value;
}

struct Transport : juce::AudioPlayHead
{
    double bpm = 120;
    juce::int64 frame = 0;
    bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo position;
        const double ppq = frame * bpm / (60 * sampleRate);
        position.setTimeInSamples(frame);
        position.setTimeInSeconds(frame / sampleRate);
        position.setBpm(bpm);
        position.setTimeSignature(TimeSignature {4, 4});
        position.setPpqPosition(ppq);
        position.setPpqPositionOfLastBarStart(4 * std::floor(ppq / 4));
        position.setIsPlaying(playing);
        position.setIsRecording(false);
        position.setIsLooping(false);
        return position;
    }
};

struct Event { juce::int64 frame; juce::MidiMessage message; };
std::vector<Event> readMidi(const juce::File& path, double bpm, juce::int64 frames)
{
    juce::FileInputStream input(path);
    juce::MidiFile midi;
    if (!input.openedOk() || !midi.readFrom(input) || midi.getTimeFormat() <= 0)
        throw std::runtime_error("expected a standard PPQ MIDI file");
    std::vector<Event> events;
    for (int track = 0; track < midi.getNumTracks(); ++track)
        for (int i = 0; i < midi.getTrack(track)->getNumEvents(); ++i)
        {
            auto message = midi.getTrack(track)->getEventPointer(i)->message;
            if (message.isMetaEvent()) continue;
            if (message.getChannel() == 0)
                throw std::runtime_error("only channel MIDI is supported");
            const double time = message.getTimeStamp() * 60 * sampleRate
                                / (bpm * midi.getTimeFormat());
            if (!std::isfinite(time) || time < 0 || time >= frames - 0.5)
                throw std::runtime_error("MIDI event lies outside the render duration");
            events.push_back({static_cast<juce::int64>(std::llround(time)), message});
        }
    if (events.empty()) throw std::runtime_error("MIDI has no channel events");
    std::stable_sort(events.begin(), events.end(),
        [](const Event& a, const Event& b) { return a.frame < b.frame; });
    return events;
}

struct Host
{
    juce::AudioPluginFormatManager formats;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    Transport transport;

    explicit Host(const juce::File& path)
    {
        formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
        juce::OwnedArray<juce::PluginDescription> types;
        formats.getFormat(0)->findAllTypesForFile(types, path.getFullPathName());
        if (types.size() != 1)
            throw std::runtime_error("expected exactly one plugin in the VST3 bundle");
        juce::String error;
        plugin = formats.createPluginInstance(*types[0], sampleRate, blockSize, error);
        if (!plugin) throw std::runtime_error("plugin creation failed: " + error.toStdString());
        auto layout = plugin->getBusesLayout();
        for (auto& bus : layout.inputBuses) bus = juce::AudioChannelSet::disabled();
        for (int i = 0; i < layout.outputBuses.size(); ++i)
            layout.outputBuses.getReference(i) = i == 0 ? juce::AudioChannelSet::stereo()
                                                       : juce::AudioChannelSet::disabled();
        if (!plugin->setBusesLayout(layout) || plugin->getTotalNumOutputChannels() != 2)
            throw std::runtime_error("plugin does not support a stereo-only instrument layout");
        plugin->setPlayHead(&transport);
        plugin->prepareToPlay(sampleRate, blockSize);
        std::cout << "Loaded " << plugin->getName() << std::endl;
    }
    ~Host() { plugin->releaseResources(); plugin->setPlayHead(nullptr); }

    void load(const juce::File& path)
    {
        juce::MemoryBlock state;
        if (!path.loadFileAsData(state) || state.isEmpty()
            || state.getSize() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("could not read a nonempty plugin state");
        plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        // Some sample players schedule their normal loading work on the message
        // thread. This bounded wait is not a universal sample-readiness guarantee.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(2000);
    }

    void save(const juce::File& path)
    {
        requireNewFile(path);
        juce::MemoryBlock state;
        plugin->getStateInformation(state);
        juce::TemporaryFile temporary(path);
        if (state.isEmpty() || !temporary.getFile().replaceWithData(state.getData(), state.getSize()))
            throw std::runtime_error("could not save plugin state");
        publish(temporary, path);
        std::cout << "Saved state (" << state.getSize() << " bytes)" << std::endl;
    }

    void idle()
    {
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        for (int block = 0; block < 4; ++block)
        {
            buffer.clear();
            midi.clear();
            plugin->processBlock(buffer, midi);
        }
    }

    void render(const std::vector<Event>& events, const juce::File& outputPath,
                double bpm, juce::int64 frames)
    {
        requireNewFile(outputPath);
        juce::TemporaryFile temporary(outputPath);
        std::unique_ptr<juce::OutputStream> output(temporary.getFile().createOutputStream());
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor(output, juce::AudioFormatWriterOptions()
            .withSampleRate(sampleRate).withNumChannels(2).withBitsPerSample(32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        if (!writer) throw std::runtime_error("could not create WAV");
        plugin->setNonRealtime(true);
        transport.bpm = bpm;
        transport.frame = 0;
        transport.playing = true;
        juce::AudioBuffer<float> buffer(2, blockSize);
        double energy = 0, maximum = 0;
        std::size_t next = 0;
        for (juce::int64 frame = 0; frame < frames; frame += blockSize)
        {
            const int count = static_cast<int>(std::min<juce::int64>(blockSize, frames - frame));
            buffer.setSize(2, count, false, false, true);
            buffer.clear();
            juce::MidiBuffer midi;
            while (next < events.size() && events[next].frame < frame + count)
            {
                midi.addEvent(events[next].message, static_cast<int>(events[next].frame - frame));
                ++next;
            }
            transport.frame = frame;
            plugin->processBlock(buffer, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < count; ++i)
            {
                const double value = buffer.getSample(c, i);
                if (!std::isfinite(value)) throw std::runtime_error("non-finite plugin audio");
                maximum = std::max(maximum, std::abs(value));
                energy += value * value;
            }
            if (!writer->writeFromAudioSampleBuffer(buffer, 0, count))
                throw std::runtime_error("WAV write failed");
        }
        if (!(energy > 0)) throw std::runtime_error("plugin rendered only silence; no reference WAV saved");
        if (!writer->flush()) throw std::runtime_error("WAV finalization failed");
        writer.reset();
        transport.playing = false;
        plugin->setNonRealtime(false);
        publish(temporary, outputPath);
        std::cout << "Rendered " << frames << " frames; peak=" << maximum
                  << "; rms=" << std::sqrt(energy / (2 * frames)) << std::endl;
    }
};

struct EditorWindow : juce::DocumentWindow, private juce::Timer
{
    Host& host;
    juce::File output;
    EditorWindow(Host& h, const juce::File& target)
        : DocumentWindow("Reference setup — close to save state", juce::Colours::darkgrey,
                         DocumentWindow::closeButton), host(h), output(target)
    {
        auto* editor = host.plugin->createEditorAndMakeActive();
        if (!editor) throw std::runtime_error("plugin has no editor");
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true);
        centreWithSize(editor->getWidth(), editor->getHeight());
        setVisible(true);
        toFront(true);
        startTimer(10);
    }
    void timerCallback() override { host.idle(); }
    void closeButtonPressed() override
    {
        try
        {
            host.save(output);
            stopTimer();
            juce::MessageManager::getInstance()->stopDispatchLoop();
        }
        catch (const std::exception& error)
        {
            std::cerr << error.what() << std::endl;
            setName("State not saved — check output path and close again");
        }
    }
};

void selfTest()
{
    for (const char* bad : {"120junk", "nan", "inf", "39", "241"})
    {
        bool rejected = false;
        try { (void) number(bad, 40, 240); } catch (...) { rejected = true; }
        if (!rejected) throw std::runtime_error("numeric validation regression");
    }
    Transport transport;
    transport.frame = 192000;
    const auto position = transport.getPosition();
    if (*position->getPpqPosition() != 8 || *position->getPpqPositionOfLastBarStart() != 8)
        throw std::runtime_error("transport regression");
    juce::MidiMessageSequence track;
    track.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(91)), 480);
    track.addEvent(juce::MidiMessage::noteOff(1, 60), 960);
    juce::MidiFile midi;
    midi.setTicksPerQuarterNote(480);
    midi.addTrack(track);
    juce::TemporaryFile temporary(".mid");
    {
        juce::FileOutputStream stream(temporary.getFile());
        if (!stream.openedOk() || !midi.writeTo(stream)) throw std::runtime_error("self-test MIDI write failed");
    }
    const auto events = readMidi(temporary.getFile(), 120, 96000);
    if (events.size() != 2 || events[0].frame != 24000 || events[1].frame != 48000)
        throw std::runtime_error("MIDI timing regression");
    juce::TemporaryFile destination(".mid");
    publish(temporary, destination.getFile());
    bool rejected = false;
    try { publish(temporary, destination.getFile()); } catch (...) { rejected = true; }
    if (!rejected || !destination.getFile().hasIdenticalContentTo(temporary.getFile()))
        throw std::runtime_error("output overwrite regression");
    std::cout << "Reference host self-test passed" << std::endl;
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--self-test") { selfTest(); return 0; }
        if (argc == 8 && std::string(argv[1]) == "render")
        {
            const double bpm = number(argv[6], 40, 240);
            const auto frames = static_cast<juce::int64>(std::llround(number(argv[7], 0.001, 120) * sampleRate));
            const auto output = file(argv[5]);
            requireNewFile(output);
            const auto events = readMidi(file(argv[4]), bpm, frames);
            juce::ScopedJuceInitialiser_GUI initialiser;
            Host host(file(argv[2]));
            host.load(file(argv[3]));
            host.render(events, output, bpm, frames);
            return 0;
        }
        if ((argc == 4 || argc == 5) && std::string(argv[1]) == "editor")
        {
            const auto output = file(argv[3]);
            requireNewFile(output);
            juce::ScopedJuceInitialiser_GUI initialiser;
            Host host(file(argv[2]));
            if (argc == 5) host.load(file(argv[4]));
            EditorWindow window(host, output);
            juce::MessageManager::getInstance()->runDispatchLoop();
            return 0;
        }
        throw std::runtime_error(
            "usage: AcustraPluginReferenceHost render PLUGIN STATE MIDI OUTPUT.wav BPM SECONDS\n"
            "       AcustraPluginReferenceHost editor PLUGIN OUTPUT.state [INPUT.state]\n"
            "       AcustraPluginReferenceHost --self-test\n"
            "48 kHz stereo, 128 frames, 4/4; BPM 40–240, duration 0.001–120 seconds.\n"
            "Editor uses normal plugin controls; closing saves to a NEW state file.");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
