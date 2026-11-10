#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char *s)
{
    if (!ok)
        throw std::runtime_error(s);
}
class PlayHead : public juce::AudioPlayHead
{
  public:
    bool playing = true;
    double beat = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm(120);
        p.setPpqPosition(beat);
        p.setIsPlaying(playing);
        return p;
    }
};
int main()
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        CheckmateProcessor p;
        p.setRateAndBufferSizeDetails(48000, 256);
        p.prepareToPlay(48000, 256);
        p.setValue("delayTime", 1.9f);
        p.setValue("feedback", .85f);
        require(p.getTailLengthSeconds() > 80, "long feedback tail reported");
        p.setValue("delayTime", .267f);
        p.setValue("feedback", .45f);
        for (int game = 0; game < 5; ++game)
        {
            checkmate::Chess board;
            for (auto san : checkmate::archiveMoves[game])
            {
                checkmate::Move m;
                if (!board.playSan(san.toStdString(), m))
                    throw std::runtime_error("archive " + std::to_string(game) +
                                             " invalid: " + san.toStdString());
                board.applyUnchecked(m);
            }
        }
        juce::AudioBuffer<float> audio(2, 256);
        juce::MidiBuffer midi;
        p.audition(36, 1);
        p.processBlock(audio, midi);
        bool on = false;
        for (auto e : midi)
            on |= e.getMessage().isNoteOn();
        require(on, "UI notes reach MIDI out");

        bool off = false;
        int offAt = -1;
        for (int i = 0; i < 50; ++i)
        {
            midi.clear();
            p.processBlock(audio, midi);
            for (auto e : midi)
            {
                if (e.getMessage().isNoteOff())
                {
                    off = true;
                    offAt = (i + 1) * 256 + e.samplePosition;
                }
            }
        }
        require(off, "generated MIDI off");
        require(offAt == 8640, "generated MIDI off is sample accurate across blocks");

        p.setValue("run", 1);
        midi.clear();
        p.processBlock(audio, midi);
        require(p.activeColumn == 0, "matrix starts at column 0");
        p.setValue("run", 0);
        midi.clear();
        p.processBlock(audio, midi);
        require(p.activeColumn == -1, "matrix stop");

        p.matrix[12] = -5;
        p.setValue("cutoff", 1234);
        juce::MemoryBlock state;
        p.getStateInformation(state);
        CheckmateProcessor restored;
        restored.setStateInformation(state.getData(), int(state.getSize()));
        require(restored.matrix[12] == -5, "matrix state restore");
        require(std::abs(restored.value("cutoff") - 1234) < 1, "parameter state restore");
        require(restored.value("run") < .5f, "restore stopped");
        juce::ValueTree session("SESSION");
        session.setProperty("marker", 1, nullptr);
        restored.setSession(session);
        restored.getStateInformation(state);
        CheckmateProcessor second;
        second.setStateInformation(state.getData(), int(state.getSize()));
        require(int(second.sessionCopy()["marker"]) == 1, "session round-trip");
        session.setProperty("marker", 2, nullptr);
        second.setSession(session);
        second.getStateInformation(state);
        restored.setStateInformation(state.getData(), int(state.getSize()));
        require(int(restored.sessionCopy()["marker"]) == 2, "updated session survives second round-trip");

        auto sf = restored.matrixMidi();
        juce::MemoryOutputStream stream;
        require(sf.writeTo(stream), "MIDI write");
        juce::MemoryInputStream input(stream.getData(), stream.getDataSize(), false);
        juce::MidiFile parsed;
        require(parsed.readFrom(input), "MIDI parse");
        require(parsed.getTimeFormat() == 960, "MIDI ticks");

        // Imported games are editable, produce MIDI and survive plugin state round-trips.
        auto imported = checkmate::importPgn("1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 *");
        require(bool(imported), "PGN imported in plugin test");
        CheckmateProcessor gameProcessor;
        gameProcessor.setRateAndBufferSizeDetails(48000, 256);
        gameProcessor.prepareToPlay(48000, 256);
        gameProcessor.loadPattern(4); // Isolate imported game from matrix events.
        CheckmateProcessor::GameBank bank;
        bank.tracks[0] = std::move(imported.games.front());
        bank.tracks[0].moves[0].note = 72;
        bank.tracks[0].moves[0].velocity = 123;
        bank.tracks[0].moves[0].duration = 60;
        gameProcessor.publishGames(bank);
        gameProcessor.setValue("gamePlayback", 1);
        gameProcessor.setValue("run", 1);
        auto exportedGame = gameProcessor.gameMidi();
        require(exportedGame.getNumTracks() == 1, "one enabled game track exports");
        bool editedNote = false;
        if (auto *track = exportedGame.getTrack(0))
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                auto message = track->getEventPointer(e)->message;
                if (message.isNoteOn() && message.getNoteNumber() == 72)
                    editedNote = message.getVelocity() > 100;
            }
        require(editedNote, "edited velocity and MIDI note exported");
        gameProcessor.processBlock(audio, midi);
        bool gameMidiOn = false;
        for (auto e : midi) if (e.getMessage().isNoteOn()) gameMidiOn = true;
        require(gameMidiOn, "imported game playback triggers notes");
        juce::MemoryBlock gameState;
        gameProcessor.getStateInformation(gameState);
        CheckmateProcessor gameRestored;
        gameRestored.setStateInformation(gameState.getData(), int(gameState.getSize()));
        auto restoredBank = gameRestored.gameSnapshot();
        require(restoredBank.tracks[0].moves.size() == 6, "game sequence restored");
        require(restoredBank.tracks[0].moves[0].note == 72 &&
                    restoredBank.tracks[0].moves[0].velocity == 123 &&
                    restoredBank.tracks[0].moves[0].duration == 60,
                "MIDI edits restored");
        require(gameRestored.value("run") < .5f, "new game transport disarmed after restore");
        // Decode a real temporary WAV and verify per-piece sample routing and persistence.
        auto sampleFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getNonexistentChildFile("Checkmate-Sample-Test", ".wav", false);
        {
            std::unique_ptr<juce::OutputStream> fileOutput = std::make_unique<juce::FileOutputStream>(sampleFile);
            juce::WavAudioFormat format;
            auto writer = format.createWriterFor(fileOutput,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000.0).withNumChannels(2).withBitsPerSample(16));
            require(writer != nullptr, "temporary WAV writer");
            juce::AudioBuffer<float> sample(2, 2400);
            for (int i = 0; i < sample.getNumSamples(); ++i)
            {
                sample.setSample(0, i, .4f);
                sample.setSample(1, i, -.3f);
            }
            require(writer->writeFromAudioSampleBuffer(sample, 0, sample.getNumSamples()),
                    "write sample fixture");
            writer.reset();
        }
        juce::String error;
        require(gameProcessor.loadPieceSample(0, sampleFile, error), "sample load");
        require(!gameProcessor.pieceSamplePaths()[0].isEmpty(), "piece sample path active");
        require(gameProcessor.cachedSampleBufferCount() == 1, "one decoded sample cached");
        // Replacing the same sample should reclaim the inactive older buffer.
        require(gameProcessor.loadPieceSample(0, sampleFile, error), "sample hot-swap load");
        require(gameProcessor.cachedSampleBufferCount() == 1,
                "sample reloading does not grow decoded memory without bound");
        gameProcessor.getStateInformation(gameState);
        CheckmateProcessor samplesRestored;
        samplesRestored.setStateInformation(gameState.getData(), int(gameState.getSize()));
        require(samplesRestored.pieceSamplePaths()[0] == sampleFile.getFullPathName(),
                "sample file path survives save/restore");
        sampleFile.deleteFile();
        gameProcessor.clearPieceSample(0);
        require(gameProcessor.cachedSampleBufferCount() == 0,
                "clearing an inactive sample releases decoded memory");
        // A sounding voice must continue using its old buffer after the user
        // removes/replaces the piece sample. It may be retired only after its
        // last voice finishes, and never on the audio thread.
        auto activeFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("Checkmate-Active-Sample", ".wav", false);
        {
            std::unique_ptr<juce::OutputStream> fileOutput =
                std::make_unique<juce::FileOutputStream>(activeFile);
            juce::WavAudioFormat format;
            auto writer = format.createWriterFor(fileOutput,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000.0)
                    .withNumChannels(2).withBitsPerSample(16));
            require(writer != nullptr, "active WAV fixture writer");
            juce::AudioBuffer<float> sample(2, 4800);
            for (int i = 0; i < sample.getNumSamples(); ++i)
            {
                sample.setSample(0, i, .25f);
                sample.setSample(1, i, .25f);
            }
            require(writer->writeFromAudioSampleBuffer(sample, 0, sample.getNumSamples()),
                    "write active WAV fixture");
        }
        CheckmateProcessor hotSwap;
        hotSwap.setRateAndBufferSizeDetails(48000, 256);
        hotSwap.prepareToPlay(48000, 256);
        require(hotSwap.loadPieceSample(0, activeFile, error), "active sample loaded");
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, .9f), 0);
        hotSwap.processBlock(audio, midi);
        hotSwap.clearPieceSample(0);
        require(hotSwap.cachedSampleBufferCount() == 1, "active sample retained through clearing");
        for (int block = 0; block < 22; ++block)
        {
            midi.clear();
            hotSwap.processBlock(audio, midi);
        }
        hotSwap.clearPieceSample(0);
        require(hotSwap.cachedSampleBufferCount() == 0, "retired active buffer reclaimed after voice end");
        activeFile.deleteFile();
        require(hotSwap.getLatencySamples() == 0, "plugin has no reported look-ahead latency");
        PlayHead ph;
        p.prepareToPlay(48000, 256);
        p.setPlayHead(&ph);
        p.setValue("bpm", 60);
        p.setValue("hostSync", 0);
        p.setValue("run", 1);
        for (int block = 0; block < 52; ++block)
        {
            midi.clear();
            p.processBlock(audio, midi);
        }
        require(p.activeColumn == 0, "manual tempo respected when host sync is disabled");
        p.prepareToPlay(48000, 256);
        p.setValue("hostSync", 1);
        p.setValue("run", 1);
        midi.clear();
        p.processBlock(audio, midi);
        require(p.activeColumn == 0, "host start");
        ph.beat = 2;
        midi.clear();
        p.processBlock(audio, midi);
        require(p.activeColumn == 4, "host seek");
        ph.playing = false;
        midi.clear();
        p.processBlock(audio, midi);
        require(p.activeColumn == -1, "host stop");
        p.setPlayHead(nullptr);

        for (auto block : {1, 64, 511, 4096})
        {
            p.setRateAndBufferSizeDetails(44100, block);
            p.prepareToPlay(44100, block);
            audio.setSize(2, block);
            midi.clear();
            midi.addEvent(juce::MidiMessage::noteOn(1, 69, .8f), block - 1);
            p.processBlock(audio, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                    require(std::isfinite(audio.getSample(c, i)), "block size finite");
        }
        {

            CheckmateProcessor visual;
            visual.setRateAndBufferSizeDetails(48000, 256);
            visual.prepareToPlay(48000, 256);
            juce::ValueTree persistedMatch("SESSION");
            persistedMatch.setProperty("moves", "52:36,12:28,", nullptr);
            visual.setSession(persistedMatch);
            auto editor = std::unique_ptr<juce::AudioProcessorEditor>(visual.createEditor());
            auto clickButton = [&](const juce::String &name)
            {
                for (auto *child : editor->getChildren())
                    if (auto *button = dynamic_cast<juce::TextButton *>(child))
                        if (button->getButtonText() == name)
                        {
                            button->onClick();
                            return;
                        }
                throw std::runtime_error("missing UI button");
            };
            clickButton("ARCHIVE");
            for (auto *child : editor->getChildren())
                if (auto *button = dynamic_cast<juce::TextButton *>(child))
                    if (button->getButtonText() == "PLAY MATRIX")
                        require(!button->isVisible(), "archive no longer exposes matrix transport");
            clickButton("STEP GAME");
            require(visual.sessionCopy()["moves"].toString() == "52:36,12:28,",
                    "archive replay does not erase current match");
            clickButton("MATCH");
            require(visual.sessionCopy()["moves"].toString() == "52:36,12:28,",
                    "match survives navigation back from archive");

            for (auto tab : {"MATCH", "MATRIX", "SOUND LAB", "ARCHIVE", "GAME LAB", "SAMPLES"})
            {

                juce::TextButton *target = nullptr;
                for (auto *child : editor->getChildren())
                    if (auto *button = dynamic_cast<juce::TextButton *>(child))
                        if (button->getButtonText() == tab)
                        {
                            target = button;
                            break;
                        }
                require(target != nullptr, "tab button exists");
                target->onClick();
                editor->setSize(1080, 720);
                juce::Image shot(juce::Image::ARGB, 1080, 720, true);
                juce::Graphics g(shot);
                editor->paintEntireComponent(g, true);
                auto name = juce::String("Checkmate-") + juce::String(tab).replace(" ", "-") + ".png";
                auto imageFile = juce::File::getCurrentWorkingDirectory().getChildFile(name);
                imageFile.deleteFile();
                juce::FileOutputStream out(imageFile);
                juce::PNGImageFormat png;
                require(png.writeImageToStream(shot, out), "preview render");
                editor->setSize(810, 540);
                for (auto *child : editor->getChildren())
                {
                    if (!child->isVisible()) continue;
                    const auto bounds = child->getBounds();
                    require(bounds.getWidth() > 0 && bounds.getHeight() > 0,
                            "visible UI control has positive size");
                    require(editor->getLocalBounds().contains(bounds),
                            "visible UI controls stay inside editor at minimum size");
                }
                juce::Image small(juce::Image::ARGB, 810, 540, true);
                juce::Graphics sg(small);
                editor->paintEntireComponent(sg, true);
                auto smallFile = juce::File::getCurrentWorkingDirectory().getChildFile("Small-" + name);
                smallFile.deleteFile();
                juce::FileOutputStream smallOut(smallFile);
                require(png.writeImageToStream(small, smallOut), "small preview render");
            }
        }
        std::cout << "PASS: archives; MIDI; matrix; game PGN/MIDI editing/playback/state; piece WAV ";
        std::cout << "load/state; host sync/seek/stop; variable blocks; six-page UI render\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
