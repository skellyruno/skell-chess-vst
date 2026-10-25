#pragma once
#include "Archive.h"
#include "PluginProcessor.h"
class CheckmateEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit CheckmateEditor(CheckmateProcessor &);
    ~CheckmateEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent &) override;

  private:
    CheckmateProcessor &proc;
    juce::LookAndFeel_V4 look;
    checkmate::Chess game;
    std::vector<checkmate::Move> history;
    std::vector<checkmate::Move> legal;
    int selected = -1, tab = 0, palette = 2, archiveIndex = 0, archivePly = 0, pieceEdit = 0;
    bool flipped = false, archivePlaying = false;
    double nextArchive = 0;
    unsigned revision = 0;
    juce::String status;
    struct Dial
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String id;
    };
    std::vector<std::unique_ptr<Dial>> dials;
    std::array<juce::TextButton, 4> tabs;
    juce::TextButton play{"PLAY MATRIX"}, reset{"NEW MATCH"}, step{"STEP GAME"}, replay{"REPLAY"},
        flip{"FLIP"}, panic{"PANIC"}, exportMidi{"EXPORT MIDI"};
    juce::ToggleButton sync{"Host sync"}, midiOut{"MIDI out"}, bot{"Black bot"}, coupling{"Tension"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach, midiAttach, botAttach,
        couplingAttach;
    juce::ComboBox preset, root, scale, division, pattern, voice, wave, archive, midiVoice;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttach, scaleAttach,
        divisionAttach, waveAttach, midiVoiceAttach;
    std::unique_ptr<juce::FileChooser> chooser;
    void timerCallback() override;
    void showTab(int);
    void buildDials();
    void addDial(const juce::String &, const juce::String &);
    void newMatch();
    void execute(checkmate::Move, bool sound = true);
    void archiveStep();
    void persist();
    void restore();
    void exportMatrix();
    static void pieceIcon(juce::Graphics &, int, juce::Rectangle<float>);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CheckmateEditor)
};
