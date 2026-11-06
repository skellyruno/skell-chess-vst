#include "PluginEditor.h"
#include <initializer_list>
namespace
{
const juce::Colour bg{0xff090c12}, panel{0xff121823}, border{0xff293341}, gold{0xffeac27b}, muted{0xff8e9bad},
    white{0xffe9edf2};
const char *pieceNames[] = {"Pawn", "Knight", "Bishop", "Rook", "Queen", "King"};
const char *presetNames[] = {"Immortal Arpeggio", "Deep Blue Acid", "Queen's Gambit",
                             "Fischer Lo-Fi",     "Endgame Zen",    "Phrygian Assault"};
juce::Font uiFont(float size)
{
#if JUCE_MAJOR_VERSION >= 8
    return juce::Font(juce::FontOptions(size));
#else
    return juce::Font(size);
#endif
}
void text(juce::Graphics &g, const juce::String &s, float x, float y, float w, float h, float size,
          juce::Colour c = white, int align = juce::Justification::centredLeft)
{
    g.setColour(c);
    g.setFont(uiFont(size));
    g.drawText(s, juce::Rectangle<float>(x, y, w, h), align);
}
} // namespace
CheckmateEditor::CheckmateEditor(CheckmateProcessor &p) : AudioProcessorEditor(p), proc(p)
{
    look.setColour(juce::Slider::rotarySliderFillColourId, gold);
    look.setColour(juce::Slider::rotarySliderOutlineColourId, border);
    look.setColour(juce::Slider::thumbColourId, gold);
    look.setColour(juce::Slider::textBoxTextColourId, white);
    look.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    look.setColour(juce::ComboBox::backgroundColourId, panel);
    look.setColour(juce::ComboBox::outlineColourId, border);
    look.setColour(juce::ComboBox::textColourId, white);
    look.setColour(juce::TextButton::buttonColourId, panel);
    look.setColour(juce::TextButton::buttonOnColourId, gold.darker(.5f));
    look.setColour(juce::TextButton::textColourOffId, white);
    setLookAndFeel(&look);
    const char *tabNames[] = {"MATCH", "MATRIX", "SOUND LAB", "ARCHIVE", "GAME LAB", "SAMPLES"};
    for (int i = 0; i < 6; ++i)
    {
        addAndMakeVisible(tabs[i]);
        tabs[i].setButtonText(tabNames[i]);
        tabs[i].onClick = [this, i] { showTab(i); };
    }
    for (auto *b : {&play, &reset, &step, &replay, &flip, &panic, &exportMidi})
        addAndMakeVisible(*b);
    for (auto *b : {&sync, &midiOut, &bot, &coupling})
        addAndMakeVisible(*b);
    for (auto *c : {&preset, &root, &scale, &division, &pattern, &voice, &wave, &archive, &midiVoice})
        addAndMakeVisible(*c);
    for (int i = 0; i < 6; ++i)
        preset.addItem(presetNames[i], i + 1);
    preset.setText("Factory preset", juce::dontSendNotification);
    preset.onChange = [this] { proc.loadPreset(preset.getSelectedId() - 1); };
    root.addItemList({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
    for (int i = 0; i < 10; ++i)
        scale.addItem(checkmate::scaleNames[i], i + 1);
    division.addItemList({"1/4", "1/8", "1/16", "1/32"}, 1);
    pattern.addItemList({"Starter matrix", "Bishop diagonals", "Pawn groove", "Knight jumps", "Clear matrix"},
                        1);
    pattern.setSelectedId(1, juce::dontSendNotification);
    pattern.onChange = [this]
    {
        proc.loadPattern(pattern.getSelectedId() - 1);
        repaint();
    };
    for (int i = 0; i < 6; ++i)
    {
        voice.addItem(pieceNames[i], i + 1);
        midiVoice.addItem(pieceNames[i], i + 1);
    }
    midiVoice.addItem("Channel 1-6", 7);
    voice.setSelectedId(1, juce::dontSendNotification);
    voice.onChange = [this]
    {
        pieceEdit = voice.getSelectedId() - 1;
        buildDials();
        resized();
    };
    wave.addItemList({"Saw", "Square", "Triangle", "Sine"}, 1);
    archive.addItemList(checkmate::archiveNames, 1);
    archive.setSelectedId(1, juce::dontSendNotification);
    archive.onChange = [this]
    {
        archiveIndex = archive.getSelectedId() - 1;
        newMatch();
        persist();
    };
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    rootAttach = std::make_unique<CA>(proc.parameters, "root", root);
    scaleAttach = std::make_unique<CA>(proc.parameters, "scale", scale);
    divisionAttach = std::make_unique<CA>(proc.parameters, "division", division);
    midiVoiceAttach = std::make_unique<CA>(proc.parameters, "midiPiece", midiVoice);
    syncAttach = std::make_unique<BA>(proc.parameters, "hostSync", sync);
    midiAttach = std::make_unique<BA>(proc.parameters, "midiOut", midiOut);
    botAttach = std::make_unique<BA>(proc.parameters, "bot", bot);
    couplingAttach = std::make_unique<BA>(proc.parameters, "tensionMod", coupling);
    play.onClick = [this] { proc.setValue("run", proc.value("run") > .5f ? 0.f : 1.f); };
    reset.onClick = [this]
    {
        if (tab == 1)
        {
            proc.loadPattern(4);
            pattern.setSelectedId(5, juce::dontSendNotification);
            repaint();
        }
        else
        {
            newMatch();
            persist();
        }
    };
    step.onClick = [this] { archiveStep(); };
    replay.onClick = [this]
    {
        archivePlaying = !archivePlaying;
        nextArchive = 0;
    };
    flip.onClick = [this]
    {
        flipped = !flipped;
        persist();
        repaint();
    };
    panic.onClick = [this]
    {
        proc.panic();
        proc.setValue("run", 0);
        archivePlaying = false;
    };
    exportMidi.onClick = [this] { exportMatrix(); };
    for (auto *b : {&importGame, &exportGame, &captureMatch, &loadArchiveGame, &sampleLoad,
                    &sampleClear, &savePack, &openPack, &saveSlot, &loadSlot})
        addAndMakeVisible(*b);
    for (auto *c : {&botStyle, &samplePiece, &gameLane, &matrixSlot})
        addAndMakeVisible(*c);
    for (auto *t : {&gamePlayback, &drumMap, &harmony, &chessFX})
        addAndMakeVisible(*t);
    for (int i = 0; i < 3; ++i)
    {
        laneOn[size_t(i)].setButtonText(juce::String::charToString(char('A' + i)));
        addAndMakeVisible(laneOn[size_t(i)]);
        laneAttach[size_t(i)] = std::make_unique<BA>(proc.parameters, "lane" + juce::String(i), laneOn[size_t(i)]);
    }
    botStyle.addItemList({"Balanced", "Aggressive", "Defensive", "Experimental"}, 1);
    botStyleAttach = std::make_unique<CA>(proc.parameters, "botStyle", botStyle);
    for (int i = 0; i < 6; ++i) samplePiece.addItem(pieceNames[i], i + 1);
    samplePiece.setSelectedId(1, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
        gameLane.addItem("TRACK " + juce::String::charToString(char('A' + i)), i + 1);
    gameLane.setSelectedId(1, juce::dontSendNotification);
    for (int i = 0; i < 16; ++i) matrixSlot.addItem("BANK " + juce::String(i + 1), i + 1);
    matrixSlot.setSelectedId(1, juce::dontSendNotification);
    gamePlaybackAttach = std::make_unique<BA>(proc.parameters, "gamePlayback", gamePlayback);
    drumMapAttach = std::make_unique<BA>(proc.parameters, "drumMap", drumMap);
    harmonyAttach = std::make_unique<BA>(proc.parameters, "chessHarmony", harmony);
    chessFXAttach = std::make_unique<BA>(proc.parameters, "chessFX", chessFX);
    for (auto *slider : {&noteEditor, &velocityEditor, &durationEditor, &matrixProbability,
                         &matrixRatchet, &matrixSwing})
    {
        addAndMakeVisible(*slider);
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 22);
        slider->setColour(juce::Slider::trackColourId, gold);
        slider->setColour(juce::Slider::backgroundColourId, panel);
    }
    noteEditor.setRange(0, 128, 1); // 0 = automatic; other values are MIDI note + 1.
    velocityEditor.setRange(1, 127, 1);
    durationEditor.setRange(10, 100, 1);
    probabilityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.parameters, "matrixProb", matrixProbability);
    ratchetAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.parameters, "matrixRatchet", matrixRatchet);
    swingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.parameters, "matrixSwing", matrixSwing);
    for (auto *label : {&gameInfo, &sampleInfo, &editInfo})
    {
        addAndMakeVisible(*label);
        label->setColour(juce::Label::textColourId, white);
        label->setFont(uiFont(13.f));
        label->setJustificationType(juce::Justification::centredLeft);
    }
    movesList.setModel(this);
    movesList.setRowHeight(27);
    movesList.setColour(juce::ListBox::backgroundColourId, panel);
    movesList.setColour(juce::ListBox::outlineColourId, border);
    movesList.setOutlineThickness(1);
    addAndMakeVisible(movesList);
    importGame.onClick = [this] { importPgnFile(); };
    exportGame.onClick = [this] { exportGameMidi(); };
    captureMatch.onClick = [this] { captureCurrentMatch(); };
    loadArchiveGame.onClick = [this] { useArchiveInGameLab(); };
    savePack.onClick = [this] { savePresetPack(); };
    openPack.onClick = [this] { openPresetPack(); };
    sampleLoad.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Choose a WAV or AIFF sample", juce::File(),
                                                        "*.wav;*.aif;*.aiff");
        auto safe = juce::Component::SafePointer<CheckmateEditor>(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser &c)
            {
                if (!safe || c.getResult() == juce::File()) return;
                juce::String error;
                const bool ok = safe->proc.loadPieceSample(safe->samplePiece.getSelectedId() - 1,
                                                            c.getResult(), error);
                if (ok) safe->refreshSampleInfo();
                else safe->sampleInfo.setText(error, juce::dontSendNotification);
            });
    };
    sampleClear.onClick = [this]
    {
        proc.clearPieceSample(samplePiece.getSelectedId() - 1);
        refreshSampleInfo();
    };
    samplePiece.onChange = [this] { refreshSampleInfo(); };
    gameLane.onChange = [this] { selectedMove = -1; refreshGameList(); };
    noteEditor.onValueChange = [this] { editSelectedMove(); };
    velocityEditor.onValueChange = [this] { editSelectedMove(); };
    durationEditor.onValueChange = [this] { editSelectedMove(); };
    saveSlot.onClick = [this]
    {
        proc.saveMatrixSlot(matrixSlot.getSelectedId() - 1);
        status = "Saved matrix bank " + juce::String(matrixSlot.getSelectedId());
        repaint();
    };
    loadSlot.onClick = [this]
    {
        proc.loadMatrixSlot(matrixSlot.getSelectedId() - 1);
        status = "Loaded matrix bank " + juce::String(matrixSlot.getSelectedId());
        repaint();
    };
    restore();
    revision = proc.stateRevision.load();
    setResizable(true, true);
    setResizeLimits(810, 540, 1620, 1080);
    getConstrainer()->setFixedAspectRatio(1.5);
    setSize(945, 630);
    startTimerHz(30);
}
CheckmateEditor::~CheckmateEditor()
{
    stopTimer();
    archivePlaying = false;
    setLookAndFeel(nullptr);
}
void CheckmateEditor::showTab(int t)
{
    if (t == 3 && tab != 3)
        newMatch();
    tab = t;
    selected = -1;
    archivePlaying = false;
    for (int i = 0; i < 6; ++i)
        tabs[i].setToggleState(i == tab, juce::dontSendNotification);
    buildDials();
    resized();
    persist();
    repaint();
}
void CheckmateEditor::addDial(const juce::String &id, const juce::String &name)
{
    auto d = std::make_unique<Dial>();
    d->id = id;
    d->label.setText(name, juce::dontSendNotification);
    d->label.setJustificationType(juce::Justification::centred);
    d->label.setColour(juce::Label::textColourId, muted);
    d->label.setFont(uiFont(12.f));
    d->slider.setLookAndFeel(&look);
    d->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    d->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 18);
    d->slider.setNumDecimalPlacesToDisplay((id == "cutoff" || id == "bpm") ? 0 : 2);
    d->slider.setPopupDisplayEnabled(true, false, this);
    if (auto *p = proc.parameters.getParameter(id))
        d->slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
    addAndMakeVisible(d->slider);
    addAndMakeVisible(d->label);
    d->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.parameters,
                                                                                           id, d->slider);
    dials.push_back(std::move(d));
}
void CheckmateEditor::buildDials()
{
    dials.clear();
    waveAttach.reset();
    if (tab == 2)
    {
        const char *ids[] = {"cutoff",   "resonance", "drive",  "volume",      "attack", "decay",
                             "sustain",  "release",   "reverb", "reverbDecay", "delay",  "delayTime",
                             "feedback", "width",     "grit",   "octave"};
        const char *labels[] = {"CUTOFF / Hz", "RESONANCE",   "DRIVE",  "MASTER",   "ATTACK / s", "DECAY / s",
                                "SUSTAIN",     "RELEASE / s", "REVERB", "ROOM / s", "DELAY",      "TIME / s",
                                "FEEDBACK",    "WIDTH",       "LO-FI",  "OCTAVE"};
        for (int i = 0; i < 16; ++i)
            addDial(ids[i], labels[i]);
        auto prefix = "p" + juce::String(pieceEdit) + "_";
        const char *f[] = {"octave", "filter", "attack", "release", "velocity", "pan"};
        const char *n[] = {"OCTAVE", "FILTER MOD", "ATTACK / s", "RELEASE / s", "VELOCITY", "PAN"};
        for (int i = 0; i < 6; ++i)
            addDial(prefix + f[i], n[i]);
        waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            proc.parameters, prefix + "wave", wave);
    }
    else
    {
        addDial("bpm", "TEMPO / BPM");
        addDial("cutoff", "CUTOFF / Hz");
        addDial("reverb", "REVERB");
        addDial("volume", "MASTER");
    }
}
void CheckmateEditor::resized()
{
    const float sx = getWidth() / 1080.f, sy = getHeight() / 720.f;
    auto bounds = [&](juce::Component &c, int x, int y, int w, int h)
    {
        c.setBounds(juce::roundToInt(x * sx), juce::roundToInt(y * sy), juce::roundToInt(w * sx),
                    juce::roundToInt(h * sy));
    };
    for (int i = 0; i < 6; ++i)
        bounds(tabs[size_t(i)], 264 + i * 99, 21, 94, 30);
    bounds(preset, 861, 21, 191, 30);
    bounds(root, 28, 83, 72, 30);
    bounds(scale, 110, 83, 216, 30);
    bounds(sync, 344, 83, 109, 30);
    bounds(midiOut, 455, 83, 102, 30);
    bounds(coupling, 564, 83, 114, 30);
    bounds(chessFX, 686, 83, 104, 30);
    bounds(harmony, 793, 83, 112, 30);
    bounds(panic, 949, 83, 103, 30);
    bounds(play, 594, 158, 142, 38);
    bounds(division, 748, 158, 90, 38);
    bounds(exportMidi, 850, 158, 202, 38);
    bounds(pattern, 594, 210, 260, 30);
    bounds(reset, 868, 210, 184, 30);
    bounds(bot, 594, 254, 160, 30);
    bounds(botStyle, 762, 254, 173, 30);
    bounds(flip, 943, 254, 109, 30);
    bounds(archive, 594, 320, 458, 32);
    bounds(step, 594, 364, 220, 34);
    bounds(replay, 828, 364, 224, 34);
    bounds(voice, 28, 471, 200, 32);
    bounds(wave, 244, 471, 200, 32);
    bounds(midiVoice, 774, 471, 278, 32);
    const bool lab = tab == 2, matrixPage = tab == 1, gamePage = tab == 4,
               samplePage = tab == 5;
    reset.setButtonText(matrixPage ? "CLEAR MATRIX" : "NEW MATCH");
    play.setVisible(tab == 0 || matrixPage || tab == 3 || gamePage);
    if (gamePage) bounds(play, 594, 308, 218, 36);
    division.setVisible(!lab && !samplePage);
    if (gamePage) bounds(division, 828, 308, 224, 36);
    exportMidi.setVisible(tab == 0 || matrixPage || tab == 3);
    pattern.setVisible(matrixPage);
    reset.setVisible(tab == 0 || matrixPage || tab == 3);
    bot.setVisible(tab == 0);
    botStyle.setVisible(tab == 0);
    flip.setVisible(tab == 0 || tab == 3);
    archive.setVisible(tab == 3);
    step.setVisible(tab == 3);
    replay.setVisible(tab == 3);
    voice.setVisible(lab);
    wave.setVisible(lab);
    midiVoice.setVisible(lab);
    bounds(matrixSlot, 594, 383, 120, 30);
    bounds(saveSlot, 720, 383, 151, 30);
    bounds(loadSlot, 878, 383, 174, 30);
    bounds(matrixProbability, 594, 464, 155, 31);
    bounds(matrixRatchet, 752, 464, 142, 31);
    bounds(matrixSwing, 899, 464, 153, 31);
    for (auto *c : std::initializer_list<juce::Component *>{&matrixSlot, &saveSlot, &loadSlot,
                    &matrixProbability, &matrixRatchet, &matrixSwing})
        c->setVisible(matrixPage);
    bounds(importGame, 594, 158, 146, 36);
    bounds(captureMatch, 746, 158, 145, 36);
    bounds(loadArchiveGame, 897, 158, 155, 36);
    bounds(gameLane, 594, 210, 212, 32);
    bounds(laneOn[0], 816, 210, 68, 32);
    bounds(laneOn[1], 887, 210, 68, 32);
    bounds(laneOn[2], 958, 210, 87, 32);
    bounds(gamePlayback, 594, 257, 220, 29);
    bounds(drumMap, 816, 257, 220, 29);
    bounds(movesList, 28, 198, 540, 468);
    bounds(gameInfo, 28, 676, 990, 22);
    bounds(editInfo, 594, 371, 450, 34);
    bounds(noteEditor, 594, 411, 458, 36);
    bounds(velocityEditor, 594, 473, 458, 36);
    bounds(durationEditor, 594, 535, 458, 36);
    bounds(exportGame, 594, 606, 209, 36);
    bounds(savePack, 810, 606, 115, 36);
    bounds(openPack, 932, 606, 120, 36);
    for (auto *c : std::initializer_list<juce::Component *>{&importGame, &captureMatch,
                    &loadArchiveGame, &gameLane, &gamePlayback, &drumMap, &movesList,
                    &gameInfo, &editInfo, &noteEditor, &velocityEditor, &durationEditor,
                    &exportGame, &savePack, &openPack})
        c->setVisible(gamePage);
    for (auto &enabled : laneOn) enabled.setVisible(gamePage);
    bounds(samplePiece, 594, 190, 326, 36);
    bounds(sampleLoad, 594, 243, 209, 38);
    bounds(sampleClear, 818, 243, 234, 38);
    bounds(sampleInfo, 594, 304, 458, 66);
    samplePiece.setVisible(samplePage);
    sampleLoad.setVisible(samplePage);
    sampleClear.setVisible(samplePage);
    sampleInfo.setVisible(samplePage);
    for (size_t i = 0; i < dials.size(); ++i)
    {
        int x, y, w, h;
        if (lab && i < 16)
        {
            x = 28 + int(i % 8) * 128; y = 168 + int(i / 8) * 135;
            w = 112; h = 110;
        }
        else if (lab)
        {
            x = 170 + int(i - 16) * 145; y = 540;
            w = 120; h = 114;
        }
        else
        {
            x = 594 + int(i % 4) * 116;
            y = matrixPage ? 534 : 481;
            w = 108; h = matrixPage ? 120 : 124;
        }
        bounds(dials[i]->label, x, y, w, 20);
        bounds(dials[i]->slider, x, y + 22, w, h - 22);
        dials[i]->label.setVisible(!gamePage && !samplePage);
        dials[i]->slider.setVisible(!gamePage && !samplePage);
    }
}
void CheckmateEditor::pieceIcon(juce::Graphics &g, int p, juce::Rectangle<float> r)
{
    // Portable vector silhouettes; no bitmap assets or platform chess font dependency.
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::translation(r.getX(), r.getY()).scaled(1.f));
    float w = r.getWidth(), h = r.getHeight();
    g.addTransform(juce::AffineTransform::scale(w / 50, h / 50));
    auto ink = p > 0 ? juce::Colour(0xffe6dcc4) : juce::Colour(0xff7a91ac);
    g.setColour(ink);
    int t = std::abs(p);
    juce::Path path;
    path.addRoundedRectangle(10, 39, 30, 5, 1);
    path.addRoundedRectangle(13, 34, 24, 4, 1);
    if (t == 1)
    {
        path.addEllipse(19, 8, 12, 12);
        path.addTriangle(25, 19, 16, 34, 34, 34);
    }
    if (t == 2)
    {
        path.startNewSubPath(14, 34);
        path.lineTo(19, 24);
        path.lineTo(13, 21);
        path.lineTo(18, 10);
        path.lineTo(22, 12);
        path.lineTo(24, 6);
        path.lineTo(33, 14);
        path.lineTo(36, 34);
        path.closeSubPath();
    }
    if (t == 3)
    {
        path.addTriangle(25, 7, 17, 20, 33, 20);
        path.addEllipse(17, 16, 16, 10);
        path.addTriangle(25, 23, 16, 34, 34, 34);
    }
    if (t == 4)
    {
        path.addRectangle(16, 17, 18, 17);
        path.addRectangle(13, 10, 24, 9);
        path.addRectangle(13, 6, 5, 7);
        path.addRectangle(22, 6, 6, 7);
        path.addRectangle(32, 6, 5, 7);
    }
    if (t == 5)
    {
        path.startNewSubPath(17, 33);
        path.lineTo(12, 14);
        path.lineTo(22, 22);
        path.lineTo(25, 10);
        path.lineTo(28, 22);
        path.lineTo(38, 14);
        path.lineTo(33, 33);
        path.closeSubPath();
        for (float x : {12.f, 25.f, 38.f})
            path.addEllipse(x - 2, 10, 4, 4);
    }
    if (t == 6)
    {
        path.addRectangle(23, 5, 4, 12);
        path.addRectangle(19, 9, 12, 4);
        path.addEllipse(17, 17, 16, 9);
        path.addTriangle(25, 23, 16, 34, 34, 34);
    }
    g.fillPath(path);
    if (t == 2)
    {
        g.setColour(panel);
        g.fillEllipse(22, 16, 2.5f, 2.5f);
    }
}
void CheckmateEditor::paint(juce::Graphics &g)
{
    g.fillAll(bg);
    g.addTransform(juce::AffineTransform::scale(getWidth() / 1080.f, getHeight() / 720.f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1b2330), 0, 0, panel, 1080, 68, false));
    g.fillRect(0, 0, 1080, 68);
    g.setColour(juce::Colour(0xffc9eb85));
    g.fillRect(0, 0, 5, 720);
    g.setColour(border);
    g.drawHorizontalLine(68, 0, 1080);
    g.drawHorizontalLine(129, 28, 1052);
    text(g, "CHECKMATE", 28, 16, 230, 28, 27, gold);
    g.setColour(juce::Colour(0xffd0ec8b).withAlpha(.7f));
    g.fillRoundedRectangle(29, 59, 172, 2, 1);
    text(g, "SKELL AUDIO  /  CHESS SYNTH", 29, 43, 245, 15, 10, muted);
    if (tab == 0 || tab == 1 || tab == 3)
    {
        g.setColour(panel);
        g.fillRoundedRectangle(28, 150, 540, 540, 8);
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 8; ++col)
            {
                int rr = flipped && tab != 1 ? 7 - row : row, cc = flipped && tab != 1 ? 7 - col : col,
                    s = rr * 8 + cc;
                juce::Rectangle<float> cell(42.f + col * 64, 164.f + row * 64, 64, 64);
                g.setColour((rr + cc) % 2 ? juce::Colour(0xff1d2936) : juce::Colour(0xff354352));
                g.fillRect(cell);
                if ((tab == 1 && proc.activeColumn.load() == cc) || s == selected)
                {
                    g.setColour(gold.withAlpha(.28f));
                    g.fillRect(cell);
                }
                if (tab != 1 && !history.empty() && (s == history.back().from || s == history.back().to))
                {
                    g.setColour(gold.withAlpha(.12f));
                    g.fillRect(cell);
                }
                int p = tab == 1 ? proc.matrix[s].load() : game.board[s];
                if (p)
                    pieceIcon(g, p, cell.reduced(7));
                if (tab != 1 && selected >= 0)
                    for (auto m : legal)
                        if (m.from == selected && m.to == s)
                        {
                            g.setColour(gold);
                            g.fillEllipse(cell.getCentreX() - 4, cell.getCentreY() - 4, 8, 8);
                        }
                if (row == 7)
                    text(g, juce::String::charToString(char('a' + cc)), cell.getX() + 51,
                         cell.getBottom() - 15, 12, 12, 9, muted);
                if (col == 0)
                    text(g, juce::String(8 - rr), cell.getX() + 3, cell.getY() + 2, 12, 12, 9, muted);
            }
        if (tab == 1)
        {
            text(g, "PIECE PALETTE", 594, 256, 250, 20, 11, muted);
            for (int i = 0; i < 7; ++i)
            {
                juce::Rectangle<float> r(594.f + i * 64, 285, 56, 62);
                g.setColour(palette == i + 1 ? gold.withAlpha(.22f) : panel);
                g.fillRoundedRectangle(r, 5);
                if (i < 6)
                    pieceIcon(g, i + 1, r.reduced(5));
                else
                    text(g, "X", r.getX(), r.getY(), r.getWidth(), r.getHeight(), 24, muted,
                         juce::Justification::centred);
            }
            text(g, "Click to place / right-click to erase", 594, 363, 458, 22, 13, muted);
            text(g, "16 BANKS / PROBABILITY / RATCHET / SWING", 594, 422, 458, 22, 11, gold);
            text(g, "PROBABILITY", 594, 448, 148, 16, 10, muted);
            text(g, "RATCHETS", 752, 448, 134, 16, 10, muted);
            text(g, "SWING", 899, 448, 148, 16, 10, muted);
            text(g, status, 594, 692, 458, 16, 11, muted);
        }
        else
        {
            text(g, tab == 3 ? "GRANDMASTER ARCHIVE" : "MATCH SONIFIER", 594, tab == 3 ? 258 : 312, 458, 22,
                 13, gold);
            if (tab == 0)
            {
                text(g, status, 594, 346, 458, 28, 18);
                text(g, "Select a piece, then a marked destination.", 594, 383, 458, 22, 13, muted);
            }
            if (tab == 3)
            {
                text(g, status, 594, 411, 458, 28, 16);
            }
        }
        if (tab != 1)
        {
            text(g, "BOARD TENSION", 594, 445, 240, 18, 10, muted);
            g.setColour(border);
            g.fillRoundedRectangle(745, 450, 307, 5, 2);
            g.setColour(gold);
            g.fillRoundedRectangle(745, 450, 307 * proc.tension.load(), 5, 2);
            text(g, "MOVES  >  NOTES / SIX PIECE VOICES", 594, 631, 458, 20, 12, gold);
            text(g, "MIDI CHANNELS 1-6  /  PAWN  KNIGHT  BISHOP  ROOK  QUEEN  KING", 594, 657,
                 458, 24, 10, muted);
        }
    }
    else if (tab == 2)
    {
        text(g, "SOUND LAB", 28, 144, 500, 20, 12, gold);
        g.setColour(border);
        g.drawHorizontalLine(438, 28, 1052);
        text(g, "PIECE PERSONALITY", 28, 442, 480, 20, 12, gold);
        text(g, "INCOMING MIDI VOICE", 774, 442, 278, 20, 11, muted);
        text(g, "Shape each piece independently. Double-click any dial to restore its default.", 28, 683,
             1024, 20, 12, muted);
    }
    else if (tab == 4)
    {
        g.setColour(panel);
        g.fillRoundedRectangle(28, 149, 540, 525, 8);
        g.fillRoundedRectangle(582, 149, 484, 520, 8);
        text(g, "MOVE TIMELINE  /  PGN MAIN LINE", 40, 153, 510, 26, 13, gold);
        text(g, "SELECT / EDIT / EXPORT", 594, 352, 450, 20, 13, gold);

        text(g, "MIDI NOTE / 0 = AUTO", 600, 393, 250, 16, 10, muted);
        text(g, "VELOCITY", 600, 457, 250, 17, 10, muted);
        text(g, "DURATION  /  % OF STEP", 600, 519, 250, 17, 10, muted);
    }
    else if (tab == 5)
    {
        text(g, "CUSTOM PIECE SAMPLES", 28, 147, 500, 24, 13, gold);
        text(g, "Select a chess piece to replace its oscillator with a WAV or AIFF one-shot.",
             594, 150, 458, 24, 12, muted);
        g.setColour(panel);
        g.fillRoundedRectangle(580, 184, 486, 338, 8);
        const auto paths = proc.pieceSamplePaths();
        for (int i = 0; i < 6; ++i)
        {
            juce::Rectangle<float> r(28.f, 185.f + i * 78, 538.f, 66.f);
            const bool selected = samplePiece.getSelectedId() == i + 1;
            g.setColour(selected ? juce::Colour(0xff243627) : panel);
            g.fillRoundedRectangle(r, 7);
            g.setColour(selected ? juce::Colour(0xffd0ec8b) : border);
            g.drawRoundedRectangle(r.reduced(.5f), 7, 1.5f);
            pieceIcon(g, i + 1, {45.f, 193.f + i * 78, 54.f, 48.f});
            text(g, juce::String(pieceNames[i]).toUpperCase(), 116, 191.f + i * 78, 160, 22, 15, white);
            auto label = paths[size_t(i)].isEmpty() ? "OSCILLATOR / FACTORY" :
                                                          juce::File(paths[size_t(i)]).getFileName();
            text(g, label, 116, 216.f + i * 78, 407, 20, 12,
                 paths[size_t(i)].isEmpty() ? muted : juce::Colour(0xffc8ee9f));
        }
        text(g, "SAMPLE ENGINE", 594, 399, 455, 26, 13, gold);
        text(g, "12-second maximum  /  stereo support  /  transposed playback", 594, 434, 458, 24,
             12, muted);
        text(g, "Samples are referenced by file path in saved projects and local packs.",
             594, 473, 458, 37, 12, muted);
        text(g, "Tip: Keep your sound files in a permanent sample-library folder.",
             594, 566, 458, 40, 13, gold);
    }
    g.setColour(border);
    g.fillRoundedRectangle(710, 91, 215, 10, 3);
    g.setColour(gold);
    g.fillRoundedRectangle(710, 91, 215 * juce::jlimit(0.f, 1.f, proc.peak.load()), 10, 3);
}
void CheckmateEditor::mouseDown(const juce::MouseEvent &e)
{
    float x = e.position.x * 1080 / getWidth(), y = e.position.y * 720 / getHeight();
    if (tab == 2 || tab == 4)
        return;
    if (tab == 5)
    {
        if (x >= 28 && x < 566 && y >= 185 && y < 185 + 6 * 78)
        {
            samplePiece.setSelectedId(1 + int((y - 185) / 78), juce::sendNotification);
            repaint();
        }
        return;
    }
    if (tab == 1 && x >= 594 && x < 1042 && y >= 285 && y < 347)
    {
        palette = juce::jlimit(1, 7, 1 + int((x - 594) / 64));
        persist();
        repaint();
        return;
    }
    if (x < 42 || x >= 554 || y < 164 || y >= 676)
        return;
    int row = int((y - 164) / 64), col = int((x - 42) / 64);
    if (flipped && tab != 1)
    {
        row = 7 - row;
        col = 7 - col;
    }
    int s = row * 8 + col;
    if (tab == 1)
    {
        int p = e.mods.isRightButtonDown() || palette == 7 ? 0 : palette;
        proc.matrix[s] = p;
        pattern.setSelectedId(0, juce::dontSendNotification);
        pattern.setText("Custom matrix", juce::dontSendNotification);
        if (p)
            proc.audition(s, p);
        repaint();
        return;
    }
    if (tab == 3)
        return;
    for (auto m : legal)
        if (m.from == selected && m.to == s)
        {
            execute(m);
            return;
        }
    if (game.board[s] * game.turn > 0 && !(proc.value("bot") > .5f && game.turn < 0))
    {
        selected = s;
        proc.audition(s, game.board[s]);
    }
    else
        selected = -1;
    repaint();
}
void CheckmateEditor::newMatch()
{
    archivePlaying = false;
    archivePly = 0;
    game.reset();
    history.clear();
    legal = game.legal();
    selected = -1;
    status = "White to move";
    proc.tension = game.tension();
    proc.panic();
    repaint();
}
void CheckmateEditor::execute(checkmate::Move m, bool sound)
{
    int p = game.board[m.from];
    bool capture = game.board[m.to] != 0 || (std::abs(p) == 1 && m.to == game.ep);
    juce::String san = game.san(m);
    game.applyUnchecked(m);
    history.push_back(m);
    legal = game.legal();
    selected = -1;
    bool check = game.check(game.turn);
    status = legal.empty()
                 ? (check ? "Checkmate" : "Stalemate")
                 : juce::String(game.turn == 1 ? "White" : "Black") + " to move" + (check ? " / CHECK" : "");
    if (sound && !proc.audition(m.to, p, legal.empty() && check ? 3 : check ? 2 : capture ? 1 : 0))
        status = "Audio queue full";
    if (tab == 3)
        status = juce::String(archivePly + 1) + ". " + san + "  /  " + status;
    proc.tension = game.tension();
    persist();
    repaint();
}
void CheckmateEditor::archiveStep()
{
    if (archivePly >= checkmate::archiveMoves[archiveIndex].size())
    {
        archivePlaying = false;
        return;
    }
    checkmate::Move m;
    auto san = checkmate::archiveMoves[archiveIndex][archivePly];
    if (!game.playSan(san.toStdString(), m))
    {
        status = "Archive stopped: invalid move " + san;
        archivePlaying = false;
        repaint();
        return;
    }
    execute(m);
    ++archivePly;
    persist();
}
void CheckmateEditor::timerCallback()
{
    if (revision != proc.stateRevision.load())
    {
        revision = proc.stateRevision.load();
        restore();
    }
    play.setButtonText(proc.value("run") > .5f ? "STOP MATRIX" : "PLAY MATRIX");
    replay.setButtonText(archivePlaying ? "PAUSE REPLAY" : "REPLAY");
    if (tab == 0 && proc.value("bot") > .5f && game.turn < 0 && !legal.empty())
        execute(game.bot(int(proc.value("botStyle"))));
    double now = juce::Time::getMillisecondCounterHiRes();
    if (archivePlaying && now >= nextArchive)
    {
        archiveStep();
        nextArchive = now + 60000 / proc.value("bpm");
    }
    repaint();
}
void CheckmateEditor::persist()
{
    juce::ValueTree s("SESSION");
    juce::String moves;
    for (auto m : history)
        moves += juce::String(m.from) + ":" + juce::String(m.to) + ",";
    s.setProperty("moves", moves, nullptr);
    s.setProperty("tab", tab, nullptr);
    s.setProperty("palette", palette, nullptr);
    s.setProperty("flipped", flipped, nullptr);
    s.setProperty("archive", archiveIndex, nullptr);
    s.setProperty("ply", archivePly, nullptr);
    proc.setSession(s);
}
void CheckmateEditor::restore()
{
    auto s = proc.sessionCopy();
    game.reset();
    history.clear();
    selected = -1;
    archivePlaying = false;
    auto moves = juce::StringArray::fromTokens(s["moves"].toString(), ",", "");
    for (auto item : moves)
    {
        auto parts = juce::StringArray::fromTokens(item, ":", "");
        if (parts.size() != 2)
            continue;
        checkmate::Move m{parts[0].getIntValue(), parts[1].getIntValue()};
        auto valid = game.legal();
        if (std::find(valid.begin(), valid.end(), m) == valid.end())
            break;
        game.applyUnchecked(m);
        history.push_back(m);
    }
    legal = game.legal();
    palette = juce::jlimit(1, 7, int(s.getProperty("palette", 2)));
    flipped = bool(s.getProperty("flipped", false));
    archiveIndex = juce::jlimit(0, 4, int(s.getProperty("archive", 0)));
    archivePly = juce::jlimit(0, checkmate::archiveMoves[archiveIndex].size(), int(s.getProperty("ply", 0)));
    archive.setSelectedId(archiveIndex + 1, juce::dontSendNotification);
    proc.tension = game.tension();
    status = legal.empty() ? (game.check(game.turn) ? "Checkmate" : "Stalemate")
                           : juce::String(game.turn == 1 ? "White" : "Black") + " to move";
    tab = juce::jlimit(0, 5, int(s.getProperty("tab", 0)));
    uiGames = proc.gameSnapshot();
    refreshGameList();
    refreshSampleInfo();
    showTab(tab);
}
int CheckmateEditor::getNumRows()
{
    return int(uiGames.tracks[size_t(juce::jlimit(0, 2, gameLane.getSelectedId() - 1))].moves.size());
}
void CheckmateEditor::paintListBoxItem(int row, juce::Graphics &g, int width, int height, bool isSelected)
{
    const auto &moves = uiGames.tracks[size_t(juce::jlimit(0, 2, gameLane.getSelectedId() - 1))].moves;
    if (row < 0 || row >= int(moves.size())) return;
    if (isSelected)
    {
        g.setColour(gold.withAlpha(.18f));
        g.fillRect(0, 0, width, height);
    }
    else if (row % 2 == 1)
    {
        g.setColour(white.withAlpha(.025f));
        g.fillRect(0, 0, width, height);
    }
    const auto &m = moves[size_t(row)];
    auto c = isSelected ? gold : white;
    g.setColour(c);
    g.setFont(uiFont(13));
    g.drawText(juce::String(row + 1), 12, 0, 45, height, juce::Justification::centredLeft);
    g.drawText(juce::String(m.san), 64, 0, 130, height, juce::Justification::centredLeft);
    g.setColour(muted);
    g.drawText(pieceNames[juce::jlimit(0, 5, std::abs(m.piece) - 1)], 208, 0, 105, height,
               juce::Justification::centredLeft);
    g.drawText(m.note >= 0 ? "MIDI " + juce::String(m.note) : "AUTO", 330, 0, 92, height,
               juce::Justification::centredLeft);
    g.drawText(juce::String(m.velocity), 442, 0, 66, height, juce::Justification::centredLeft);
}
void CheckmateEditor::selectedRowsChanged(int row)
{
    selectedMove = row;
    updatingMoveEditor = true;
    const auto &moves = uiGames.tracks[size_t(juce::jlimit(0, 2, gameLane.getSelectedId() - 1))].moves;
    const bool valid = row >= 0 && row < int(moves.size());
    for (auto *s : {&noteEditor, &velocityEditor, &durationEditor}) s->setEnabled(valid);
    if (valid)
    {
        const auto &m = moves[size_t(row)];
        noteEditor.setValue(m.note + 1, juce::dontSendNotification);
        velocityEditor.setValue(m.velocity, juce::dontSendNotification);
        durationEditor.setValue(m.duration, juce::dontSendNotification);
        editInfo.setText("MOVE " + juce::String(row + 1) + " / " + juce::String(m.san) +
                        "     NOTE  (0 = AUTO)   /   VELOCITY   /   LENGTH %", juce::dontSendNotification);
    }
    else
        editInfo.setText("SELECT A MOVE TO EDIT NOTE, VELOCITY AND LENGTH", juce::dontSendNotification);
    updatingMoveEditor = false;
}
void CheckmateEditor::refreshGameList()
{
    const int lane = juce::jlimit(0, 2, gameLane.getSelectedId() - 1);
    const auto &game = uiGames.tracks[size_t(lane)];
    movesList.updateContent();
    if (selectedMove >= int(game.moves.size())) selectedMove = -1;
    if (selectedMove < 0) movesList.deselectAllRows();
    else movesList.selectRow(selectedMove, false, true);
    selectedRowsChanged(selectedMove);
    gameInfo.setText((game.moves.empty() ? "EMPTY TRACK / IMPORT A PGN OR CAPTURE MATCH" :
                      juce::String(game.name) + " / " + juce::String(game.moves.size()) + " MOVES"),
                     juce::dontSendNotification);
}
void CheckmateEditor::editSelectedMove()
{
    if (updatingMoveEditor) return;
    auto &moves = uiGames.tracks[size_t(juce::jlimit(0, 2, gameLane.getSelectedId() - 1))].moves;
    if (selectedMove < 0 || selectedMove >= int(moves.size())) return;
    auto &m = moves[size_t(selectedMove)];
    m.note = int(noteEditor.getValue()) - 1;
    m.velocity = int(velocityEditor.getValue());
    m.duration = int(durationEditor.getValue());
    proc.publishGames(uiGames);
    movesList.repaintRow(selectedMove);
}
void CheckmateEditor::importPgnFile()
{
    chooser = std::make_unique<juce::FileChooser>("Import PGN main-line games", juce::File(), "*.pgn");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<CheckmateEditor>(this)](const juce::FileChooser &c)
        {
            if (!safe || c.getResult() == juce::File()) return;
            auto file = c.getResult();
            if (file.getSize() > 2 * 1024 * 1024)
            {
                safe->gameInfo.setText("PGN file exceeds 2 MB", juce::dontSendNotification);
                return;
            }
            auto parsed = checkmate::importPgn(file.loadFileAsString().toStdString());
            if (!parsed)
            {
                safe->gameInfo.setText(juce::String(parsed.error), juce::dontSendNotification);
                return;
            }
            const int start = juce::jlimit(0, 2, safe->gameLane.getSelectedId() - 1);
            const auto count = std::min(int(parsed.games.size()), 3 - start);
            for (int i = 0; i < count; ++i)
                safe->uiGames.tracks[size_t(start + i)] = std::move(parsed.games[size_t(i)]);
            safe->proc.publishGames(safe->uiGames);
            safe->selectedMove = -1;
            safe->refreshGameList();
            if (int(parsed.games.size()) > count)
                safe->gameInfo.setText("Imported " + juce::String(count) + " of " +
                                       juce::String(parsed.games.size()) + " games (three track limit)",
                                       juce::dontSendNotification);
        });
}
void CheckmateEditor::captureCurrentMatch()
{
    if (history.empty())
    {
        gameInfo.setText("No moves to capture. Play a match first.", juce::dontSendNotification);
        return;
    }
    checkmate::Chess board;
    checkmate::MusicalGame gameSequence;
    gameSequence.name = "Live match";
    for (auto m : history)
    {
        if (gameSequence.moves.size() >= 4096) break;
        const int piece = board.board[m.from];
        const bool capture = board.board[m.to] != 0 || (std::abs(piece) == 1 && m.to == board.ep);
        checkmate::MusicalMove event;
        event.move = m;
        event.square = m.to;
        event.piece = piece;
        event.san = board.san(m);
        board.applyUnchecked(m);
        event.action = board.check(board.turn) ? (board.legal().empty() ? 3 : 2) : (capture ? 1 : 0);
        event.tension = board.tension();
        gameSequence.moves.push_back(std::move(event));
    }
    uiGames.tracks[size_t(juce::jlimit(0, 2, gameLane.getSelectedId() - 1))] = std::move(gameSequence);
    proc.publishGames(uiGames);
    selectedMove = -1;
    refreshGameList();
}
void CheckmateEditor::useArchiveInGameLab()
{
    checkmate::Chess board;
    checkmate::MusicalGame gameSequence;
    gameSequence.name = checkmate::archiveNames[archiveIndex].toStdString();
    for (auto san : checkmate::archiveMoves[archiveIndex])
    {
        checkmate::Move m;
        if (!board.playSan(san.toStdString(), m))
        {
            gameInfo.setText("Cannot parse built-in archive", juce::dontSendNotification);
            return;
        }
        const int piece = board.board[m.from];
        const bool capture = board.board[m.to] != 0 || (std::abs(piece) == 1 && m.to == board.ep);
        checkmate::MusicalMove event;
        event.move = m;
        event.square = m.to;
        event.piece = piece;
        event.san = board.san(m);
        board.applyUnchecked(m);
        event.action = board.check(board.turn) ? (board.legal().empty() ? 3 : 2) : (capture ? 1 : 0);
        event.tension = board.tension();
        gameSequence.moves.push_back(std::move(event));
    }
    uiGames.tracks[size_t(juce::jlimit(0, 2, gameLane.getSelectedId() - 1))] = std::move(gameSequence);
    proc.publishGames(uiGames);
    selectedMove = -1;
    refreshGameList();
}
void CheckmateEditor::exportGameMidi()
{
    auto fileMidi = proc.gameMidi();
    if (fileMidi.getNumTracks() == 0)
    {
        gameInfo.setText("Enable a track containing moves before exporting", juce::dontSendNotification);
        return;
    }
    chooser = std::make_unique<juce::FileChooser>("Export edited chess games as MIDI",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Checkmate-Games.mid"),
        "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<CheckmateEditor>(this)](const juce::FileChooser &c)
        {
            if (!safe || c.getResult() == juce::File()) return;
            juce::TemporaryFile temp(c.getResult());
            bool ok = false;
            {
                juce::FileOutputStream out(temp.getFile());
                if (out.openedOk()) ok = safe->proc.gameMidi().writeTo(out);
                out.flush();
                ok = ok && out.getStatus().wasOk();
            }
            if (ok) ok = temp.overwriteTargetFileWithTemporary();
            safe->gameInfo.setText(ok ? "Game MIDI exported" : "MIDI export failed",
                                    juce::dontSendNotification);
        });
}
void CheckmateEditor::refreshSampleInfo()
{
    const auto paths = proc.pieceSamplePaths();
    const int piece = juce::jlimit(0, 5, samplePiece.getSelectedId() - 1);
    sampleInfo.setText(paths[size_t(piece)].isEmpty() ?
            "No sample loaded / original synthesized voice active" :
            juce::String(pieceNames[piece]) + "  /  " + juce::File(paths[size_t(piece)]).getFileName() +
            (juce::File(paths[size_t(piece)]).existsAsFile() ? "  /  READY" : "  /  FILE MISSING"),
            juce::dontSendNotification);
    repaint();
}
void CheckmateEditor::savePresetPack()
{
    chooser = std::make_unique<juce::FileChooser>("Save local Checkmate preset pack",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("My-Checkmate.cmate"),
        "*.cmate");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<CheckmateEditor>(this)](const juce::FileChooser &c)
        {
            if (!safe || c.getResult() == juce::File()) return;
            juce::MemoryBlock state;
            safe->proc.getStateInformation(state);
            juce::TemporaryFile temp(c.getResult());
            bool ok = false;
            {
                juce::FileOutputStream out(temp.getFile());
                if (out.openedOk()) ok = out.write(state.getData(), state.getSize());
                out.flush();
                ok = ok && out.getStatus().wasOk();
            }
            if (ok) ok = temp.overwriteTargetFileWithTemporary();
            safe->gameInfo.setText(ok ? "Preset pack saved (samples remain external files)" :
                                       "Pack save failed", juce::dontSendNotification);
        });
}
void CheckmateEditor::openPresetPack()
{
    chooser = std::make_unique<juce::FileChooser>("Open local Checkmate preset pack", juce::File(), "*.cmate");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<CheckmateEditor>(this)](const juce::FileChooser &c)
        {
            if (!safe || c.getResult() == juce::File()) return;
            auto f = c.getResult();
            if (f.getSize() > 16 * 1024 * 1024 || f.getSize() < 8)
            {
                safe->gameInfo.setText("Invalid or oversized preset pack", juce::dontSendNotification);
                return;
            }
            juce::MemoryBlock block;
            if (!f.loadFileAsData(block))
            {
                safe->gameInfo.setText("Cannot read preset pack", juce::dontSendNotification);
                return;
            }
            safe->proc.setStateInformation(block.getData(), int(block.getSize()));
            safe->restore();
        });
}

void CheckmateEditor::exportMatrix()
{
    chooser = std::make_unique<juce::FileChooser>(
        "Export one matrix loop",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Checkmate.mid"),
        "*.mid");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<CheckmateEditor>(this)](const juce::FileChooser &c)
        {
            if (!safe)
                return;
            auto file = c.getResult();
            if (file == juce::File())
                return;
            juce::TemporaryFile temp(file);
            bool ok = false;
            {
                juce::FileOutputStream out(temp.getFile());
                if (out.openedOk())
                    ok = safe->proc.matrixMidi().writeTo(out);
                out.flush();
                ok = ok && out.getStatus().wasOk();
            }
            if (ok)
                ok = temp.overwriteTargetFileWithTemporary();
            safe->status = ok ? "Matrix MIDI exported" : "MIDI export failed";
            safe->repaint();
        });
}
