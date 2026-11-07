#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <climits>
#include <cstdint>
namespace
{
const char *ids[] = {"volume",  "cutoff",     "resonance",   "drive",     "attack",    "decay",    "sustain",
                     "release", "reverb",     "reverbDecay", "delay",     "delayTime", "feedback", "width",
                     "grit",    "tensionMod", "bpm",         "root",      "scale",     "octave",   "division",
                     "run",     "hostSync",   "midiOut",     "midiPiece", "bot"};
const char *pieceFieldNames[] = {"wave", "octave", "filter", "attack", "release", "velocity", "pan"};
} // namespace
CheckmateProcessor::CheckmateProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "CHECKMATE", createLayout())
{
    for (size_t i = 0; i < globals.size(); ++i)
        globals[i] = parameters.getRawParameterValue(ids[i]);
    for (int p = 0; p < 6; ++p)
        for (int f = 0; f < 7; ++f)
            pieceParams[p][f] = parameters.getRawParameterValue("p" + juce::String(p) + "_" + pieceFieldNames[f]);
    for (int lane = 0; lane < 3; ++lane)
        laneParameters[size_t(lane)] = parameters.getRawParameterValue("lane" + juce::String(lane));
    loadPattern(0);
    saveMatrixSlot(0);
    publishGames(GameBank{});
    auto emptyBank = std::make_unique<SampleBank>();
    sampleData.store(emptyBank.get(), std::memory_order_release);
    sampleVersions.push_back(std::move(emptyBank));
}
juce::AudioProcessorValueTreeState::ParameterLayout CheckmateProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto add = [&](const juce::String &id, const juce::String &name, float min, float max, float def,
                   float skew = 1.f)
    {
        const int decimals =
            id == "cutoff" || id == "bpm"
                ? 0
                : (id.containsIgnoreCase("attack") || id.containsIgnoreCase("release") || id == "delayTime"
                       ? 3
                       : 2);
        auto attributes =
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([decimals](float value, int)
                                             { return juce::String(value, decimals); })
                .withValueFromStringFunction([](const juce::String &text) { return text.getFloatValue(); });
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name,
                                                          juce::NormalisableRange<float>(min, max, 0.f, skew),
                                                          def, attributes));
    };
    auto choice = [&](const char *id, const char *name, juce::StringArray options, int def)
    { l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, 1}, name, options, def)); };
    auto boolean = [&](const char *id, const char *name, bool def)
    { l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1}, name, def)); };
    add("volume", "Master", 0, 1, .8f);
    add("cutoff", "Cutoff", 100, 18000, 3400, .3f);
    add("resonance", "Resonance", .5f, 20, 4.5f);
    add("drive", "Drive", 0, 1, .2f);
    add("attack", "Attack", .002f, 2, .03f, .3f);
    add("decay", "Decay", .05f, 2, .3f, .4f);
    add("sustain", "Sustain", 0, 1, .6f);
    add("release", "Release", .05f, 4, .5f, .4f);
    add("reverb", "Reverb", 0, 1, .35f);
    add("reverbDecay", "Room decay", .5f, 6, 2.5f);
    add("delay", "Delay", 0, 1, .28f);
    add("delayTime", "Delay time", .02f, 1.9f, .267f, .4f);
    add("feedback", "Feedback", 0, .85f, .45f);
    add("width", "Stereo width", 0, 1, .7f);
    add("grit", "Lo-fi grit", 0, 1, .1f);
    boolean("tensionMod", "Board tension", true);
    add("bpm", "Tempo", 30, 240, 112);
    choice("root", "Root", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 2);
    juce::StringArray scales;
    for (auto s : checkmate::scaleNames)
        scales.add(s);
    choice("scale", "Scale", scales, 0);
    l.add(
        std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"octave", 1}, "Octave offset", -2, 2, 0));
    choice("division", "Step division", {"1/4", "1/8", "1/16", "1/32"}, 1);
    boolean("run", "Matrix play", false);
    boolean("hostSync", "Host sync", false);
    boolean("midiOut", "Generated MIDI out", true);
    choice("midiPiece", "MIDI voice", {"Pawn", "Knight", "Bishop", "Rook", "Queen", "King", "Channel 1-6"},
           6);
    boolean("bot", "Black bot", false);
    choice("botStyle", "Bot style", {"Balanced", "Aggressive", "Defensive", "Experimental"}, 0);
    boolean("gamePlayback", "Game sequencer", false);
    boolean("lane0", "Track A", true);
    boolean("lane1", "Track B", false);
    boolean("lane2", "Track C", false);
    boolean("drumMap", "Chess beat map", false);
    boolean("chessHarmony", "Chess harmony", false);
    boolean("chessFX", "Chess effects", false);
    add("matrixProb", "Step probability", 0, 100, 100);
    l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"matrixRatchet", 1}, "Ratchets", 1, 4, 1));
    add("matrixSwing", "Swing", 0, 75, 0);
    const char *names[] = {"Pawn", "Knight", "Bishop", "Rook", "Queen", "King"};
    for (int p = 0; p < 6; ++p)
    {
        auto cfg = checkmate::defaultPieces[p];
        auto prefix = "p" + juce::String(p) + "_";
        auto name = juce::String(names[p]) + " ";
        l.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "wave", 1}, name + "Wave",
            juce::StringArray{"Saw", "Square", "Triangle", "Sine"}, cfg.wave));
        l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{prefix + "octave", 1},
                                                        name + "Octave", 1, 7, cfg.octave));
        add(prefix + "filter", name + "Filter mod", 0, 1, cfg.filter);
        add(prefix + "attack", name + "Attack", .002f, 2, cfg.attack, .3f);
        add(prefix + "release", name + "Release", .05f, 4, cfg.release, .4f);
        add(prefix + "velocity", name + "Velocity", .5f, 1.5f, cfg.velocity);
        add(prefix + "pan", name + "Pan", -1, 1, cfg.pan);
    }
    return l;
}
checkmate::Settings CheckmateProcessor::currentSettings() const
{
    checkmate::Settings s;
    float *target[] = {&s.volume, &s.cutoff,    &s.resonance, &s.drive,  &s.attack,
                       &s.decay,  &s.sustain,   &s.release,   &s.reverb, &s.reverbDecay,
                       &s.delay,  &s.delayTime, &s.feedback,  &s.width,  &s.grit};
    for (int i = 0; i < 15; ++i)
        *target[i] = globals[i]->load();
    s.tensionMod = globals[15]->load() > .5f;
    s.tension = tension.load();
    for (int p = 0; p < 6; ++p)
    {
        auto &cfg = s.pieces[p];
        cfg.wave = int(pieceParams[p][0]->load());
        cfg.octave = int(pieceParams[p][1]->load());
        cfg.filter = pieceParams[p][2]->load();
        cfg.attack = pieceParams[p][3]->load();
        cfg.release = pieceParams[p][4]->load();
        cfg.velocity = pieceParams[p][5]->load();
        cfg.pan = pieceParams[p][6]->load();
    }
    return s;
}
double CheckmateProcessor::getTailLengthSeconds() const
{
    const auto cfg = currentSettings();
    double voiceTail = cfg.release + .18;
    for (const auto &p : cfg.pieces)
        voiceTail = std::max(voiceTail, double(cfg.release + p.release) + .18);
    const double fb = std::clamp(double(cfg.feedback + (cfg.tensionMod ? .15f : 0.f)), 0.0, .85);
    const double delayTail =
        cfg.delay > .0001f ? cfg.delayTime * (fb > .0001 ? std::log(.001) / std::log(fb) : 1.0) : 0.0;
    return voiceTail + delayTail + (cfg.reverb > .0001f ? cfg.reverbDecay : 0.0);
}
void CheckmateProcessor::setValue(const juce::String &id, float v)
{
    if (auto *p = parameters.getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(v));
        p->endChangeGesture();
    }
}
bool CheckmateProcessor::isBusesLayoutSupported(const BusesLayout &l) const
{
    return l.getMainInputChannelSet().isDisabled() &&
           (l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() ||
            l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}
void CheckmateProcessor::prepareToPlay(double sr, int)
{
    synth.prepare(sr);
    profilerTicksPerSecond = double(juce::Time::getHighResolutionTicksPerSecond());
    smoothedLoad = heldPeakLoad = 0.f;
    dspLoadPercent = dspPeakPercent = 0.f;
    activeVoices = 0;
    voiceSteals = overBudgetBlocks = 0;
    for (auto &c : gates)
        c.fill(0);
    for (auto &f : future)
        f.when = 0;
    audioClock = 0;
    freeBeat = 0;
    lastStep = -1;
    wasRunning = wasHostPlaying = false;
    output.ensureSize(65536);
    setLatencySamples(0);
}
bool CheckmateProcessor::audition(int square, int signedPiece, int action)
{
    int a, n, b, m;
    fifo.prepareToWrite(1, a, n, b, m);
    if (!n)
        return false;
    events[size_t(a)] = {square, signedPiece, action};
    fifo.finishedWrite(1);
    return true;
}
void CheckmateProcessor::generated(int note, int piece, float velocity, int offset, juce::MidiBuffer &out,
                                   int delaySamples, int gateSamples, int synthNote)
{
    if (delaySamples > 0)
    {
        for (auto &f : future)
            if (!f.when)
            {
                f = {audioClock + offset + delaySamples, note, piece, velocity, gateSamples, synthNote};
                return;
            }
        return;
    }
    const auto *bank = sampleData.load(std::memory_order_acquire);
    const auto *sample = bank != nullptr ? bank->pieces[size_t(piece)] : nullptr;
    synth.start(synthNote >= 0 ? synthNote : note, piece + 1, piece, velocity, 0, false, 0, sample, gateSamples);
    if (value("midiOut") > .5f)
    {
        auto &gate = gates[piece][note];
        if (gate > 0)
            out.addEvent(juce::MidiMessage::noteOff(piece + 1, note), offset);
        out.addEvent(juce::MidiMessage::noteOn(piece + 1, note, velocity), offset);
        gate = audioClock + offset + std::max(1, gateSamples > 0 ? gateSamples : int(synth.rate * .18));
    }
}
void CheckmateProcessor::trigger(int square, int signedPiece, int action, int offset, juce::MidiBuffer &out, int delaySamples)
{
    if (square < 0 || square >= 64 || signedPiece == 0 || std::abs(signedPiece) > 6)
        return;
    int piece = std::abs(signedPiece) - 1;
    int note = checkmate::squareNote(square, piece, int(value("root")), int(value("scale")),
                                     int(value("octave")), synth.settings.pieces);
    float velocity = juce::jlimit(
        .16f, 1.f, 90.f / 127 * synth.settings.pieces[piece].velocity * (action == 1 ? 1.2f : 1.f));
    generated(note, piece, velocity, offset, out, delaySamples);
    // Piece personalities: knight fifth, bishop overtone, king foundation triad.
    if (piece == 1)
        generated(std::min(127, note + 7), piece, velocity * .65f, offset, out, delaySamples + int(synth.rate * .065));
    if (piece == 5)
    {
        generated(std::min(127, note + 4), piece, velocity * .6f, offset, out, delaySamples);
        generated(std::min(127, note + 7), piece, velocity * .6f, offset, out, delaySamples);
    }
    if (action >= 2)
        generated(std::min(127, note + 12), piece, velocity * .6f, offset, out, delaySamples);
}
void CheckmateProcessor::triggerGame(const checkmate::MusicalMove &m, int offset, juce::MidiBuffer &out,
                                     bool drums, double playbackBpm, int delaySamples)
{
    if (m.piece == 0 || m.square < 0 || m.square >= 64) return;
    int p = std::abs(m.piece) - 1;
    int note = checkmate::moveNote(m, int(value("root")), int(value("scale")), int(value("octave")),
                                     synth.settings.pieces, drums);
    tension.store(m.tension, std::memory_order_relaxed);
    synth.settings.tension = m.tension;
    const auto *bank = sampleData.load(std::memory_order_acquire);
    const int sampleKey = drums && bank && bank->pieces[size_t(p)] != nullptr ? 60 : -1;
    const float velocity = juce::jlimit(.01f, 1.f, m.velocity / 127.f * synth.settings.pieces[p].velocity);
    const int gate = std::max(1, int(synth.rate * (60. / std::max(30., playbackBpm)) *
                                 std::pow(.5, int(value("division"))) * m.duration / 100.));
    generated(note, p, velocity, offset, out, delaySamples, gate, sampleKey);
    if (value("chessHarmony") > .5f && !drums && m.action >= 1)
    {
        generated(std::min(127, note + (m.tension > .65f ? 3 : 4)), p, velocity * .55f,
                  offset, out, delaySamples, gate);
        generated(std::min(127, note + 7), p, velocity * .55f, offset, out, delaySamples, gate);
    }
}
void CheckmateProcessor::stopGenerated(int offset, juce::MidiBuffer &out)
{
    for (int c = 0; c < 6; ++c)
        for (int n = 0; n < 128; ++n)
            if (gates[c][n] > 0)
            {
                out.addEvent(juce::MidiMessage::noteOff(c + 1, n), offset);
                gates[c][n] = 0;
            }
    for (auto &f : future)
        f.when = 0;
    synth.allOff();
}
void CheckmateProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    const auto startTicks = juce::Time::getHighResolutionTicks();
    juce::ScopedNoDenormals guard;
    gameReaders.fetch_add(1, std::memory_order_seq_cst);
    buffer.clear();
    output.clear();
    synth.settings = currentSettings();
    if (value("chessFX") > .5f)
    {
        auto t = juce::jlimit(0.f, 1.f, tension.load());
        synth.settings.drive = std::min(1.f, synth.settings.drive + t * .16f);
        synth.settings.delay = std::min(1.f, synth.settings.delay + t * .12f);
    }
    blockSamples = buffer.getNumSamples();
    int a, n, b, m;
    fifo.prepareToRead(fifo.getNumReady(), a, n, b, m);
    bool panicNow = panicRequested.exchange(false);
    if (panicNow)
    {
        stopGenerated(0, output);
        synth.reset();
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            auto e = events[size_t(a + i)];
            trigger(e.square, e.piece, e.action, 0, output);
        }
        for (int i = 0; i < m; ++i)
        {
            auto e = events[size_t(b + i)];
            trigger(e.square, e.piece, e.action, 0, output);
        }
    }
    fifo.finishedRead(n + m);
    bool sync = value("hostSync") > .5f, play = value("run") > .5f;
    double bpm = value("bpm"), ppq = 0;
    bool hostPlaying = false, hasHost = false;
    if (auto *ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hasHost = true;
            hostPlaying = pos->getIsPlaying();
            if (sync)
                if (auto t = pos->getBpm())
                    if (std::isfinite(*t) && *t > 0)
                        bpm = *t;
            if (auto q = pos->getPpqPosition())
                ppq = *q;
            else
                hasHost = false;
        }
    bool running = play && (!sync || (hasHost && hostPlaying));
    if ((wasRunning && !running) || (sync && wasHostPlaying && !hostPlaying))
        stopGenerated(0, output);
    if (running && !wasRunning)
    {
        lastStep = -1;
        freeBeat = 0;
    }
    if (!running)
        activeColumn = -1;
    double stepBeats = std::pow(.5, int(value("division"))), increment = bpm / (60 * synth.rate);
    auto it = midi.begin(), end = midi.end();
    float blockPeak = 0;
    int64_t nextOff = INT64_MAX;
    auto findOff = [&]
    {
        nextOff = INT64_MAX;
        for (const auto &c : gates)
            for (auto deadline : c)
                if (deadline > 0)
                    nextOff = std::min(nextOff, deadline);
        for (auto f : future)
            if (f.when > 0)
                nextOff = std::min(nextOff, f.when);
    };
    findOff();
    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        if (audioClock + s >= nextOff)
        {
            for (int c = 0; c < 6; ++c)
                for (int note = 0; note < 128; ++note)
                    if (gates[c][note] > 0 && gates[c][note] <= audioClock + s)
                    {
                        output.addEvent(juce::MidiMessage::noteOff(c + 1, note), s);
                        gates[c][note] = 0;
                    }
            for (auto &f : future)
                if (f.when > 0 && f.when <= audioClock + s)
                {
                    generated(f.note, f.piece, f.velocity, s, output, 0, f.gateSamples, f.synthNote);
                    f.when = 0;
                }
            findOff();
        }
        while (it != end && (*it).samplePosition <= s)
        {
            if ((*it).numBytes > 3)
            {
                ++it;
                continue;
            }
            auto msg = (*it).getMessage();
            int channel = msg.getChannel();
            if (msg.isNoteOn())
            {
                int p = int(value("midiPiece"));
                if (p == 6)
                    p = (channel - 1) % 6;
                const auto *bank = sampleData.load(std::memory_order_acquire);
                const auto *sample = bank ? bank->pieces[size_t(p)] : nullptr;
                synth.start(msg.getNoteNumber(), channel, p, msg.getFloatVelocity(), 0, true, 0, sample);
            }
            else if (msg.isNoteOff())
                synth.off(msg.getNoteNumber(), channel);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                synth.allOff(channel);
            ++it;
        }
        if (running)
        {
            double beat = sync ? ppq + s * increment : freeBeat;
            int64_t step = int64_t(std::floor((beat + 1.e-9) / stepBeats));
            if (step != lastStep)
            {
                lastStep = step;
                int col = int((step % 8 + 8) % 8);
                activeColumn = col;
                const int swingSamples = (step & 1) ? int(synth.rate * 60. / bpm * stepBeats *
                                                              value("matrixSwing") * .005) : 0;
                const int ratchets = juce::jlimit(1, 4, int(value("matrixRatchet")));
                const double probability = value("matrixProb") / 100.;
                for (int r = 0; r < 8; ++r)
                {
                    const int cell = r * 8 + col;
                    const int piece = matrix[size_t(cell)].load();
                    if (!piece) continue;
                    if (!checkmate::stepPassesProbability(step, cell, probability)) continue;
                    for (int repeat = 0; repeat < ratchets; ++repeat)
                    {
                        int delay = swingSamples + int(repeat * stepBeats * 60. / bpm * synth.rate / ratchets);
                        trigger(cell, piece, 0, s, output, delay);
                    }
                }
                const auto *games = gameData.load(std::memory_order_seq_cst);
                if (value("gamePlayback") > .5f && games != nullptr)
                    for (int lane = 0; lane < 3; ++lane)
                    {
                        if (laneParameters[size_t(lane)]->load() < .5f) continue;
                        const auto &moves = games->tracks[size_t(lane)].moves;
                        if (moves.empty()) continue;
                        auto pos = size_t((step % int64_t(moves.size()) + int64_t(moves.size())) %
                                          int64_t(moves.size()));
                        triggerGame(moves[pos], s, output, value("drumMap") > .5f, bpm, swingSamples);
                    }
                findOff();
            }
            if (!sync)
                freeBeat += increment;
        }
        float l, r;
        synth.render(l, r);
        blockPeak = std::max(blockPeak, std::max(std::abs(l), std::abs(r)));
        if (buffer.getNumChannels() > 1)
        {
            buffer.setSample(0, s, l);
            buffer.setSample(1, s, r);
        }
        else if (buffer.getNumChannels() == 1)
            buffer.setSample(0, s, (l + r) * .5f);
    }
    audioClock += buffer.getNumSamples();
    wasRunning = running;
    wasHostPlaying = hostPlaying;
    peak = blockPeak;
    midi.swapWith(output);
    gameReaders.fetch_sub(1, std::memory_order_seq_cst);
    // No locks, heap use, or file I/O in the callback. 100% means one callback
    // consumed its entire sample budget. It does NOT imply a confirmed host xrun.
    const auto elapsed = juce::Time::getHighResolutionTicks() - startTicks;
    const double budgetTicks = profilerTicksPerSecond * double(buffer.getNumSamples()) /
                               std::max(1.0, synth.rate);
    if (budgetTicks > 0 && buffer.getNumSamples() >= 16)
    {
        const float instant = float(std::clamp(100.0 * double(elapsed) / budgetTicks, 0.0, 999.0));
        smoothedLoad += .1f * (instant - smoothedLoad);
        heldPeakLoad = std::max(instant, heldPeakLoad * .995f);
        dspLoadPercent.store(smoothedLoad, std::memory_order_relaxed);
        dspPeakPercent.store(heldPeakLoad, std::memory_order_relaxed);
        if (instant >= 100.f) overBudgetBlocks.fetch_add(1, std::memory_order_relaxed);
    }
    int sounding = 0;
    for (const auto &v : synth.voices)
        if (v.active) ++sounding;
    activeVoices.store(sounding, std::memory_order_relaxed);
    voiceSteals.store(synth.stolenVoices, std::memory_order_relaxed);
}
juce::ValueTree CheckmateProcessor::sessionCopy() const
{
    const juce::ScopedLock lock(sessionLock);
    return session.createCopy();
}
void CheckmateProcessor::setSession(juce::ValueTree s)
{
    const juce::ScopedLock lock(sessionLock);
    session = s.createCopy();
}
void CheckmateProcessor::getStateInformation(juce::MemoryBlock &data)
{
    auto tree = parameters.copyState();
    juce::String cells;
    for (auto &cell : matrix)
        cells += juce::String(cell.load()) + ",";
    tree.setProperty("matrix", cells, nullptr);
    while (tree.getChildWithName("SESSION").isValid())
        tree.removeChild(tree.getChildWithName("SESSION"), nullptr);
    tree.addChild(sessionCopy(), -1, nullptr);
    auto games = gameSnapshot();
    juce::ValueTree gameTree("GAMEBANK");
    for (int trackIndex = 0; trackIndex < 3; ++trackIndex)
    {
        auto &game = games.tracks[size_t(trackIndex)];
        juce::ValueTree track("TRACK");
        track.setProperty("name", juce::String(game.name), nullptr);
        for (const auto &m : game.moves)
        {
            juce::ValueTree note("MOVE");
            note.setProperty("from", m.move.from, nullptr);
            note.setProperty("to", m.move.to, nullptr);
            note.setProperty("square", m.square, nullptr);
            note.setProperty("piece", m.piece, nullptr);
            note.setProperty("action", m.action, nullptr);
            note.setProperty("note", m.note, nullptr);
            note.setProperty("velocity", m.velocity, nullptr);
            note.setProperty("duration", m.duration, nullptr);
            note.setProperty("tension", m.tension, nullptr);
            note.setProperty("san", juce::String(m.san), nullptr);
            track.addChild(note, -1, nullptr);
        }
        gameTree.addChild(track, -1, nullptr);
    }
    tree.addChild(gameTree, -1, nullptr);
    juce::String matrixBanks;
    {
        const juce::ScopedLock lock(dataLock);
        for (const auto &slot : matrixSlots)
            for (auto cell : slot) matrixBanks += juce::String(cell) + ",";
    }
    tree.setProperty("matrixBanks", matrixBanks, nullptr);
    auto paths = pieceSamplePaths();
    for (int p = 0; p < 6; ++p)
        tree.setProperty("samplePath" + juce::String(p), paths[size_t(p)], nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, data);
}
void CheckmateProcessor::setStateInformation(const void *data, int bytes)
{
    if (auto xml = getXmlFromBinary(data, bytes))
        if (xml->hasTagName("CHECKMATE"))
        {
            auto tree = juce::ValueTree::fromXml(*xml);
            auto cells = juce::StringArray::fromTokens(tree["matrix"].toString(), ",", "");
            if (cells.size() >= 64)
                for (int i = 0; i < 64; ++i)
                    matrix[i] = juce::jlimit(-6, 6, cells[i].getIntValue());
            if (auto s = tree.getChildWithName("SESSION"); s.isValid())
                setSession(s);
            GameBank bank;
            auto gameTree = tree.getChildWithName("GAMEBANK");
            if (gameTree.isValid())
                for (int lane = 0; lane < std::min(3, gameTree.getNumChildren()); ++lane)
                {
                    auto track = gameTree.getChild(lane);
                    auto &game = bank.tracks[size_t(lane)];
                    game.name = track.getProperty("name").toString().toStdString();
                    for (int index = 0; index < std::min(4096, track.getNumChildren()); ++index)
                    {
                        auto move = track.getChild(index);
                        checkmate::MusicalMove m;
                        m.move = {int(move.getProperty("from", 0)), int(move.getProperty("to", 0))};
                        m.square = int(move.getProperty("square", 0));
                        m.piece = int(move.getProperty("piece", 0));
                        m.action = juce::jlimit(0, 3, int(move.getProperty("action", 0)));
                        m.note = juce::jlimit(-1, 127, int(move.getProperty("note", -1)));
                        m.velocity = juce::jlimit(1, 127, int(move.getProperty("velocity", 90)));
                        m.duration = juce::jlimit(10, 100, int(move.getProperty("duration", 80)));
                        m.tension = juce::jlimit(.1f, 1.f, float(move.getProperty("tension", .25f)));
                        m.san = move.getProperty("san").toString().toStdString();
                        if (m.square >= 0 && m.square < 64 && m.piece != 0 && std::abs(m.piece) <= 6)
                            game.moves.push_back(std::move(m));
                    }
                }
            publishGames(std::move(bank));
            auto banks = juce::StringArray::fromTokens(tree.getProperty("matrixBanks").toString(), ",", "");
            if (banks.size() >= 16 * 64)
            {
                const juce::ScopedLock lock(dataLock);
                for (int slot = 0; slot < 16; ++slot)
                    for (int cell = 0; cell < 64; ++cell)
                        matrixSlots[size_t(slot)][size_t(cell)] =
                            juce::jlimit(-6, 6, banks[slot * 64 + cell].getIntValue());
            }
            std::array<juce::String, 6> paths;
            for (int p = 0; p < 6; ++p)
                paths[size_t(p)] = tree.getProperty("samplePath" + juce::String(p)).toString();
            for (int p = 0; p < 6; ++p)
                clearPieceSample(p);
            // Keep the saved location when a sample has moved or is temporarily unavailable.
            for (int p = 0; p < 6; ++p)
            {
                if (paths[size_t(p)].isEmpty()) continue;
                juce::String error;
                if (!loadPieceSample(p, juce::File(paths[size_t(p)]), error))
                {
                    const juce::ScopedLock lock(dataLock);
                    samplePaths[size_t(p)] = paths[size_t(p)];
                }
            }
            // Reopening a project never starts the free-running sequencer unexpectedly.
            tree.setProperty("run", 0, nullptr);
            for (auto child : tree)
                if (child["id"].toString() == "run")
                    child.setProperty("value", 0, nullptr);
            tree.removeChild(tree.getChildWithName("SESSION"), nullptr);
            tree.removeChild(tree.getChildWithName("GAMEBANK"), nullptr);
            tree.removeProperty("matrix", nullptr);
            tree.removeProperty("matrixBanks", nullptr);
            for (int p = 0; p < 6; ++p)
                tree.removeProperty("samplePath" + juce::String(p), nullptr);
            parameters.replaceState(tree);
            panicRequested = true;
            ++stateRevision;
        }
}
void CheckmateProcessor::loadPattern(int index)
{
    for (auto &c : matrix)
        c = 0;
    if (index == 0)
    {
        int row[] = {6, 5, 4, 3, 4, 2, 1, 0}, p[] = {1, 2, 3, 5, 6, 3, 2, 4};
        for (int c = 0; c < 8; ++c)
            matrix[row[c] * 8 + c] = p[c];
    }
    if (index == 1)
        for (int c = 0; c < 8; ++c)
        {
            matrix[c * 8 + c] = 3;
            matrix[(7 - c) * 8 + c] = -3;
        }
    if (index == 2)
        for (int c = 0; c < 8; ++c)
        {
            matrix[48 + c] = 1;
            if (c % 2 == 0)
                matrix[56 + c] = 4;
        }
    if (index == 3)
    {
        const int rows[] = {1, 3, 5, 2, 4, 6, 0, 7}, cols[] = {2, 1, 4, 5, 6, 3, 7, 0};
        for (int i = 0; i < 8; ++i)
            matrix[rows[i] * 8 + cols[i]] = i % 2 == 0 ? 2 : -2;
    }
}
void CheckmateProcessor::loadPreset(int i)
{
    const float table[6][15] = {
        {128, 2, 0, 4200, 6.2f, .15f, .02f, .45f, .42f, 3.2f, .3f, .234f, .4f, .1f, 1},
        {140, 7, 7, 2800, 11.5f, .65f, .01f, .3f, .25f, 1.8f, .45f, .214f, .6f, .35f, 1},
        {104, 5, 4, 5200, 3.8f, .1f, .06f, .8f, .5f, 4, .22f, .288f, .35f, .1f, 1},
        {84, 9, 9, 1900, 2.2f, .25f, .05f, .6f, .38f, 2.8f, .35f, .357f, .5f, .45f, 0},
        {68, 2, 6, 2400, 4, .05f, .3f, 1.8f, .65f, 5.5f, .4f, .441f, .65f, .05f, 1},
        {132, 4, 5, 4600, 7.8f, .4f, .015f, .35f, .3f, 2.2f, .32f, .227f, .48f, .1f, 1}};
    const char *keys[] = {"bpm",   "root",      "scale",    "cutoff", "resonance",
                          "drive", "attack",    "release",  "reverb", "reverbDecay",
                          "delay", "delayTime", "feedback", "grit",   "tensionMod"};
    for (auto *p : getParameters())
        if (auto *ranged = dynamic_cast<juce::RangedAudioParameter *>(p))
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
            ranged->endChangeGesture();
        }
    i = juce::jlimit(0, 5, i);
    for (int k = 0; k < 15; ++k)
        setValue(keys[k], table[i][k]);
    panic();
}
juce::MidiFile CheckmateProcessor::matrixMidi() const
{
    juce::MidiMessageSequence seq;
    const auto cfg = currentSettings();
    double step = std::pow(.5, int(value("division"))) * 960;
    const int ratchets = juce::jlimit(1, 4, int(value("matrixRatchet")));
    const double swing = value("matrixSwing") * .005 * step;
    const double probability = value("matrixProb") / 100.;
    auto tempo = juce::MidiMessage::tempoMetaEvent(int(60000000 / value("bpm")));
    tempo.setTimeStamp(0);
    seq.addEvent(tempo);
    for (int col = 0; col < 8; ++col)
        for (int row = 0; row < 8; ++row)
        {
            int p = std::abs(matrix[row * 8 + col].load()) - 1;
            if (p < 0 || !checkmate::stepPassesProbability(col, row * 8 + col, probability))
                continue;
            int note = checkmate::squareNote(row * 8 + col, p, int(value("root")), int(value("scale")),
                                             int(value("octave")), cfg.pieces);
            auto add = [&](int key, float vel, double delay = 0)
            {
                for (int repeat = 0; repeat < ratchets; ++repeat)
                {
                    const double when = col * step + (col & 1 ? swing : 0.) +
                                        repeat * step / ratchets + delay;
                    auto on = juce::MidiMessage::noteOn(p + 1, key, vel);
                    on.setTimeStamp(when);
                    seq.addEvent(on);
                    auto off = juce::MidiMessage::noteOff(p + 1, key);
                    off.setTimeStamp(when + std::min(step * .8 / ratchets,
                                                    960 * value("bpm") * .18 / 60));
                    seq.addEvent(off);
                }
            };
            float velocity = juce::jlimit(.16f, 1.f, 90.f / 127 * cfg.pieces[p].velocity);
            add(note, velocity);
            if (p == 1)
                add(std::min(127, note + 7), velocity * .65f, 960 * value("bpm") * .065 / 60);
            if (p == 5)
            {
                add(std::min(127, note + 4), velocity * .6f);
                add(std::min(127, note + 7), velocity * .6f);
            }
        }
    auto end = juce::MidiMessage::endOfTrack();
    end.setTimeStamp(std::max(8 * step, seq.getEndTime() + 1));
    seq.addEvent(end);
    seq.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote(960);
    file.addTrack(seq);
    return file;
}
CheckmateProcessor::GameBank CheckmateProcessor::gameSnapshot() const
{
    gameReaders.fetch_add(1, std::memory_order_seq_cst);
    const auto *bank = gameData.load(std::memory_order_seq_cst);
    auto copy = bank ? *bank : GameBank{};
    gameReaders.fetch_sub(1, std::memory_order_seq_cst);
    return copy;
}
void CheckmateProcessor::publishGames(GameBank updated)
{
    for (auto &track : updated.tracks)
    {
        if (track.name.size() > 200) track.name.resize(200);
        if (track.moves.size() > 4096) track.moves.resize(4096);
        for (auto &m : track.moves)
        {
            m.note = juce::jlimit(-1, 127, m.note);
            m.velocity = juce::jlimit(1, 127, m.velocity);
            m.duration = juce::jlimit(10, 100, m.duration);
            m.tension = juce::jlimit(.1f, 1.f, m.tension);
        }
    }
    auto bank = std::make_unique<GameBank>(std::move(updated));
    const juce::ScopedLock lock(dataLock);
    const auto *current = bank.get();
    gameVersions.push_back(std::move(bank));
    gameData.store(current, std::memory_order_seq_cst);
    if (gameReaders.load(std::memory_order_seq_cst) == 0 && gameVersions.size() > 2)
    {
        gameVersions.erase(std::remove_if(gameVersions.begin(), gameVersions.end(),
            [current](const std::unique_ptr<GameBank> &p) { return p.get() != current; }),
            gameVersions.end());
    }
}
void CheckmateProcessor::saveMatrixSlot(int slot)
{
    if (slot < 0 || slot >= 16) return;
    const juce::ScopedLock lock(dataLock);
    for (int cell = 0; cell < 64; ++cell) matrixSlots[size_t(slot)][size_t(cell)] = matrix[size_t(cell)].load();
}
void CheckmateProcessor::loadMatrixSlot(int slot)
{
    if (slot < 0 || slot >= 16) return;
    const juce::ScopedLock lock(dataLock);
    for (int cell = 0; cell < 64; ++cell) matrix[size_t(cell)] = matrixSlots[size_t(slot)][size_t(cell)];
}
std::array<juce::String, 6> CheckmateProcessor::pieceSamplePaths() const
{
    const juce::ScopedLock lock(dataLock);
    return samplePaths;
}
bool CheckmateProcessor::loadPieceSample(int piece, const juce::File &file, juce::String &error)
{
    if (piece < 0 || piece >= 6) { error = "Invalid piece"; return false; }
    if (!file.existsAsFile() || file.getSize() > 96 * 1024 * 1024)
    {
        error = "Sample file missing or exceeds 96 MB";
        return false;
    }
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(file));
    if (!reader || reader->sampleRate <= 1000 || reader->numChannels < 1 || reader->numChannels > 8 ||
        reader->lengthInSamples < 2 || reader->lengthInSamples > 12 * reader->sampleRate ||
        reader->lengthInSamples > 2 * 1024 * 1024 || reader->sampleRate > 384000)
    {
        error = "Load an audio sample between 2 frames and 12 seconds";
        return false;
    }
    const int length = int(reader->lengthInSamples);
    juce::AudioBuffer<float> buffer(2, length);
    buffer.clear();
    if (!reader->read(&buffer, 0, length, 0, true, reader->numChannels > 1))
    {
        error = "Sample decoder failed";
        return false;
    }
    auto data = std::make_unique<checkmate::SampleData>();
    data->sampleRate = reader->sampleRate;
    data->left.assign(buffer.getReadPointer(0), buffer.getReadPointer(0) + length);
    data->right.assign(buffer.getReadPointer(reader->numChannels > 1 ? 1 : 0),
                       buffer.getReadPointer(reader->numChannels > 1 ? 1 : 0) + length);
    if (!data->valid()) { error = "Invalid sample"; return false; }
    for (auto *channel : {&data->left, &data->right})
        for (auto &sample : *channel)
            sample = std::isfinite(sample) ? juce::jlimit(-1.f, 1.f, sample) : 0.f;
    // All previously published buffers remain alive until processor teardown.
    const juce::ScopedLock lock(dataLock);
    auto bank = std::make_unique<SampleBank>(*sampleData.load(std::memory_order_acquire));
    bank->pieces[size_t(piece)] = data.get();
    sampleStorage.push_back(std::move(data));
    samplePaths[size_t(piece)] = file.getFullPathName();
    sampleData.store(bank.get(), std::memory_order_release);
    sampleVersions.push_back(std::move(bank));
    error.clear();
    return true;
}
void CheckmateProcessor::clearPieceSample(int piece)
{
    if (piece < 0 || piece >= 6) return;
    const juce::ScopedLock lock(dataLock);
    auto bank = std::make_unique<SampleBank>(*sampleData.load(std::memory_order_acquire));
    bank->pieces[size_t(piece)] = nullptr;
    samplePaths[size_t(piece)].clear();
    sampleData.store(bank.get(), std::memory_order_release);
    sampleVersions.push_back(std::move(bank));
}
void CheckmateProcessor::reloadSamplePaths()
{
    auto paths = pieceSamplePaths();
    for (int p = 0; p < 6; ++p)
    {
        if (paths[size_t(p)].isEmpty()) continue;
        juce::String error;
        loadPieceSample(p, juce::File(paths[size_t(p)]), error);
    }
}
juce::MidiFile CheckmateProcessor::gameMidi() const
{
    juce::MidiFile file;
    file.setTicksPerQuarterNote(960);
    auto games = gameSnapshot();
    auto cfg = currentSettings();
    const double beatTicks = 960 * std::pow(.5, int(value("division")));
    const bool drums = value("drumMap") > .5f;
    const bool harmony = value("chessHarmony") > .5f;
    for (int lane = 0; lane < 3; ++lane)
    {
        const auto &moves = games.tracks[size_t(lane)].moves;
        if (moves.empty() || laneParameters[size_t(lane)]->load() < .5f) continue;
        juce::MidiMessageSequence seq;
        auto tempo = juce::MidiMessage::tempoMetaEvent(int(60000000 / value("bpm")));
        tempo.setTimeStamp(0);
        seq.addEvent(tempo);
        for (size_t i = 0; i < moves.size(); ++i)
        {
            const auto &m = moves[i];
            if (m.piece == 0) continue;
            int p = std::abs(m.piece) - 1;
            const int note = checkmate::moveNote(m, int(value("root")), int(value("scale")),
                                                  int(value("octave")), cfg.pieces, drums);
            const double start = double(i) * beatTicks;
            const double end = start + beatTicks * m.duration / 100.;
            const auto add = [&](int key, float weight)
            {
                auto on = juce::MidiMessage::noteOn(p + 1, key,
                        juce::jlimit(.01f, 1.f, m.velocity / 127.f * weight));
                on.setTimeStamp(start);
                seq.addEvent(on);
                auto off = juce::MidiMessage::noteOff(p + 1, key);
                off.setTimeStamp(end);
                seq.addEvent(off);
            };
            add(note, cfg.pieces[p].velocity);
            if (harmony && !drums && m.action >= 1)
            {
                add(std::min(127, note + (m.tension > .65f ? 3 : 4)), .55f);
                add(std::min(127, note + 7), .55f);
            }
        }
        auto end = juce::MidiMessage::endOfTrack();
        end.setTimeStamp(std::max(beatTicks * moves.size(), seq.getEndTime() + 1.));
        seq.addEvent(end);
        seq.updateMatchedPairs();
        file.addTrack(seq);
    }
    return file;
}

juce::AudioProcessorEditor *CheckmateProcessor::createEditor()
{
    return new CheckmateEditor(*this);
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new CheckmateProcessor();
}
