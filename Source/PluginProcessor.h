#pragma once
#include "Chess.h"
#include "Synth.h"
#include <JuceHeader.h>
class CheckmateProcessor final : public juce::AudioProcessor
{
  public:
    CheckmateProcessor();
    using juce::AudioProcessor::processBlock;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Checkmate"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float value(const juce::String &id) const { return parameters.getRawParameterValue(id)->load(); }
    void setValue(const juce::String &id, float v);
    std::array<std::atomic<int>, 64> matrix{};
    std::atomic<int> activeColumn{-1};
    std::atomic<float> peak{0}, tension{.25f};
    std::atomic<unsigned> stateRevision{0};
    bool audition(int square, int signedPiece, int action = 0);
    void panic() { panicRequested = true; }
    void loadPreset(int index);
    void loadPattern(int index);
    juce::ValueTree sessionCopy() const;
    void setSession(juce::ValueTree);
    checkmate::Settings currentSettings() const;
    juce::MidiFile matrixMidi() const;

  private:
    struct Event
    {
        int square = 0, piece = 0, action = 0;
    };
    std::array<Event, 256> events{};
    juce::AbstractFifo fifo{256};
    std::atomic<bool> panicRequested{false};
    checkmate::Synth synth;
    std::array<std::array<int64_t, 128>, 6> gates{};
    std::array<std::atomic<float> *, 26> globals{};
    std::array<std::array<std::atomic<float> *, 7>, 6> pieceParams{};
    juce::MidiBuffer output;
    struct Future
    {
        int64_t when = 0;
        int note = 0, piece = 0;
        float velocity = 0;
    };
    std::array<Future, 128> future{};
    int blockSamples = 0;
    int64_t audioClock = 0;
    double freeBeat = 0;
    int64_t lastStep = -1;
    bool wasRunning = false, wasHostPlaying = false;
    mutable juce::CriticalSection sessionLock;
    juce::ValueTree session{"SESSION"};
    void trigger(int square, int signedPiece, int action, int offset, juce::MidiBuffer &);
    void generated(int note, int piece, float velocity, int offset, juce::MidiBuffer &, int delaySamples = 0);
    void stopGenerated(int offset, juce::MidiBuffer &);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CheckmateProcessor)
};
