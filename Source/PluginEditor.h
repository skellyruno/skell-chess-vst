#pragma once
#include "Archive.h"
#include "CheckmateLookAndFeel.h"
#include "PluginProcessor.h"
#include <SkellCore/GuidedTour.h>
class CheckmateEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::ListBoxModel
{
  public:
    explicit CheckmateEditor(CheckmateProcessor &);
    ~CheckmateEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent &) override;

  private:
    CheckmateProcessor &proc;
    CheckmateLookAndFeel look;
    checkmate::Chess game;
    checkmate::Chess archiveBoard;
    checkmate::Move archiveLastMove{};
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
    std::array<juce::TextButton, 6> tabs;
    juce::TextButton helpTour{"?"};
    juce::TextButton play{"PLAY MATRIX"}, reset{"NEW MATCH"}, step{"STEP GAME"}, replay{"REPLAY"},
        flip{"FLIP"}, panic{"PANIC"}, exportMidi{"EXPORT MIDI"};
    juce::ToggleButton sync{"Host sync"}, midiOut{"MIDI out"}, bot{"Black bot"}, coupling{"Tension"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach, midiAttach, botAttach,
        couplingAttach;
    juce::ComboBox preset, root, scale, division, pattern, voice, wave, archive, midiVoice;
    juce::ComboBox botStyle, samplePiece, gameLane, matrixSlot;
    juce::TextButton importGame{"IMPORT PGN"}, exportGame{"EXPORT GAME MIDI"}, captureMatch{"CAPTURE MATCH"},
        loadArchiveGame{"USE ARCHIVE"}, sampleLoad{"LOAD SAMPLE"}, sampleClear{"REMOVE SAMPLE"},
        savePack{"SAVE PACK"}, openPack{"OPEN PACK"}, saveSlot{"SAVE SLOT"}, loadSlot{"LOAD SLOT"};
    juce::ToggleButton gamePlayback{"Game sequencer"}, drumMap{"Beat mapping"},
        harmony{"Harmony"}, chessFX{"Chess FX"};
    std::array<juce::ToggleButton, 3> laneOn;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 3> laneAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gamePlaybackAttach,
        drumMapAttach, harmonyAttach, chessFXAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> botStyleAttach;
    juce::Slider noteEditor, velocityEditor, durationEditor, matrixProbability, matrixRatchet, matrixSwing;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> probabilityAttach,
        ratchetAttach, swingAttach;
    juce::ListBox movesList;
    juce::Label gameInfo, sampleInfo, editInfo;
    std::array<bool, 6> sampleMissingCache{};
    std::array<juce::String, 6> sampleDisplayPaths{};
    juce::uint32 lastSampleHealthCheck = 0;
    int idleRefreshCounter = 0;
    CheckmateProcessor::GameBank uiGames;
    int selectedMove = -1;
    bool updatingMoveEditor = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttach, scaleAttach,
        divisionAttach, waveAttach, midiVoiceAttach;
    std::unique_ptr<juce::FileChooser> chooser;
    skellcore::ui::GuidedTourController guidedTour{*this,"skell-checkmate",1};
    void showGuidedTour(bool firstRun);
    void timerCallback() override;
    void showTab(int);
    void buildDials();
    void addDial(const juce::String &, const juce::String &);
    void newMatch();
    void restartArchive();
    void execute(checkmate::Move, bool sound = true);
    void archiveStep();
    void persist();
    void restore();
    void exportMatrix();
    void importPgnFile();
    void exportGameMidi();
    void captureCurrentMatch();
    void useArchiveInGameLab();
    void refreshGameList();
    void editSelectedMove();
    void refreshSampleInfo();
    void refreshSampleHealth();
    void savePresetPack();
    void openPresetPack();
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics &, int width, int height, bool selected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    static void pieceIcon(juce::Graphics &, int, juce::Rectangle<float>);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CheckmateEditor)
};
