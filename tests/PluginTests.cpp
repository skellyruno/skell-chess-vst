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

        PlayHead ph;
        p.setPlayHead(&ph);
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
            auto editor = std::unique_ptr<juce::AudioProcessorEditor>(visual.createEditor());

            for (auto tab : {"MATCH", "MATRIX", "SOUND LAB", "ARCHIVE"})
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
                juce::Image small(juce::Image::ARGB, 810, 540, true);
                juce::Graphics sg(small);
                editor->paintEntireComponent(sg, true);
                auto smallFile = juce::File::getCurrentWorkingDirectory().getChildFile("Small-" + name);
                smallFile.deleteFile();
                juce::FileOutputStream smallOut(smallFile);
                require(png.writeImageToStream(small, smallOut), "small preview render");
            }
        }
        std::cout << "PASS: five complete archives; MIDI on/off; matrix play/stop; state round-trip; MIDI "
                     "export; host sync/seek/stop; variable blocks; editor render\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
