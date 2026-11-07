#include "Chess.h"
#include "Synth.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
static std::atomic<bool> watchAlloc{false};
static std::atomic<int> allocs{0};
void *operator new(std::size_t n)
{
    if (watchAlloc)
        ++allocs;
    if (auto p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept
{
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept
{
    std::free(p);
}
using namespace checkmate;
void require(bool x, const char *s)
{
    if (!x)
        throw std::runtime_error(s);
}
uint64_t perft(const Chess &b, int depth)
{
    if (!depth)
        return 1;
    uint64_t n = 0;
    for (auto m : b.legal())
    {
        Chess c = b;
        c.applyUnchecked(m);
        n += perft(c, depth - 1);
    }
    return n;
}
void play(Chess &b, const char *san)
{
    Move m;
    require(b.playSan(san, m), san);
    b.applyUnchecked(m);
}
int main()
{
    try
    {
        Chess b;
        require(b.legal().size() == 20, "initial legal moves");
        require(perft(b, 2) == 400, "perft 2");
        require(perft(b, 3) == 8902, "perft 3");
        require(perft(b, 4) == 197281, "perft 4");
        play(b, "e4");
        play(b, "a6");
        play(b, "e5");
        play(b, "d5");
        Move ep;
        require(b.playSan("exd6", ep), "en passant legal");
        b.applyUnchecked(ep);
        require(b.board[27] == 0 && b.board[19] == pawn, "en passant captures pawn");
        b.reset();
        for (auto s : {"f3", "e5", "g4", "Qh4#"})
            play(b, s);
        require(b.check(1) && b.legal().empty(), "fool's mate");
        b.reset();
        for (auto s : {"e4", "e5", "Nf3", "Nc6", "Bc4", "Nf6", "O-O"})
            play(b, s);
        require(b.board[62] == king && b.board[61] == rook && !(b.rights & 3), "castling rights and rook");
        b.board.fill(0);
        b.board[60] = king;
        b.board[4] = -king;
        b.board[56] = rook;
        b.board[63] = rook;
        b.turn = 1;
        b.rights = 3;
        auto castleMoves = b.legal();
        require(std::find(castleMoves.begin(), castleMoves.end(), Move{60, 62}) != castleMoves.end(),
                "kingside castle");
        b.applyUnchecked({60, 61});
        b.applyUnchecked({4, 5});
        b.applyUnchecked({61, 60});
        b.applyUnchecked({5, 4});
        require(!(b.rights & 3), "king return never restores rights");
        b.board.fill(0);
        b.board[60] = king;
        b.board[4] = -king;
        b.board[63] = rook;
        b.board[12] = -rook;
        b.turn = 1;
        b.rights = 1;
        auto lm = b.legal();
        require(std::find(lm.begin(), lm.end(), Move{60, 62}) == lm.end(), "cannot castle out of check");
        b.board.fill(0);
        b.board[63] = king;
        b.board[7] = -king;
        b.board[8] = pawn;
        b.turn = 1;
        play(b, "a8=Q+");
        for (int scale = 0; scale < 10; ++scale)
            for (int root = 0; root < 12; ++root)
                for (int sq = 0; sq < 64; ++sq)
                    for (int p = 0; p < 6; ++p)
                    {
                        int n = squareNote(sq, p, root, scale, 0, defaultPieces);
                        require(n >= 24 && n <= 108, "square note range");
                    }
        require(squareNote(36, 0, 2, 0, 0, defaultPieces) == 45, "Dorian pawn on e4 is A2");
        for (double sr : {22050., 44100., 48000., 96000., 192000.})
        {
            Synth synth;
            synth.prepare(sr);
            synth.settings.cutoff = 18000;
            synth.settings.resonance = 20;
            synth.settings.drive = 1;
            synth.settings.feedback = .85f;
            for (int p = 0; p < 6; ++p)
                synth.start(36 + p * 7, p + 1, p, .8f);
            double energy = 0;
            watchAlloc = true;
            for (int i = 0; i < int(sr * .8); ++i)
            {
                float l, r;
                synth.render(l, r);
                if (!std::isfinite(l) || !std::isfinite(r) || std::abs(l) > 1.001f || std::abs(r) > 1.001f)
                {
                    watchAlloc = false;
                    throw std::runtime_error("audio finite / output bound");
                }
                energy += l * l + r * r;
            }
            watchAlloc = false;
            require(energy > .01, "audible synth");
            require(allocs == 0, "no render allocations");
            synth.reset();
            float l, r;
            synth.render(l, r);
            require(l == 0 && r == 0, "reset silence");
            synth.start(69, 1, 0, 1, 0, true);
            for (int i = 0; i < int(sr * .2); ++i)
                synth.render(l, r);
            synth.off(69, 1);
            for (int i = 0; i < int(sr * 2); ++i)
                synth.render(l, r);
            for (auto v : synth.voices)
                require(!v.active, "MIDI note-off releases voice");
        }
        // One-shot sample mode must remain finite and allocation-free in render().
        SampleData loaded;
        loaded.sampleRate = 48000;
        loaded.left.assign(4800, .8f);
        loaded.right.assign(4800, -.8f);
        Synth sampler;
        sampler.prepare(48000);
        sampler.start(60, 1, 0, .9f, 0, false, 0, &loaded);
        double sampledEnergy = 0;
        watchAlloc = true;
        for (int i = 0; i < 12000; ++i)
        {
            float l = 0, r = 0;
            sampler.render(l, r);
            sampledEnergy += l * l + r * r;
            if (!std::isfinite(l) || !std::isfinite(r)) { watchAlloc = false; throw std::runtime_error("sample finite"); }
        }
        watchAlloc = false;
        require(sampledEnergy > 0, "custom sample produces audio");
        require(allocs == 0, "sample playback no allocations");
        for (auto v : sampler.voices) require(!v.active, "sample terminates at buffer end");

        // A constant-polarity one-shot must not leave persistent DC at the output.
        // Test also exercises the sample-tail fade and high-resolution triangle tables.
        SampleData dcSample;
        dcSample.sampleRate = 48000;
        dcSample.left.assign(48000, .75f);
        dcSample.right.assign(48000, .75f);
        Synth dcTest;
        dcTest.prepare(48000);
        dcTest.settings.reverb = 0;
        dcTest.settings.delay = 0;
        dcTest.settings.drive = 0;
        dcTest.settings.volume = 1;
        dcTest.start(60, 1, 0, 1.f, 0, false, 0, &dcSample);
        double dcMean = 0;
        for (int i = 0; i < 48000; ++i)
        {
            float l, r;
            dcTest.render(l, r);
            require(std::isfinite(l) && std::isfinite(r), "DC suppression finite");
            if (i >= 28800 && i < 38400) dcMean += l;
        }
        require(std::abs(dcMean / 9600) < .01, "10 Hz DC blocker removes steady offset");

        Synth triangleStress;
        triangleStress.prepare(48000);
        for (int i = 0; i < 72; ++i)
            triangleStress.start(24 + i, 1, 1, .75f);
        // All of these voices use the triangle wavetable (octave-dependent band limiting).
        for (auto &v : triangleStress.voices)
            if (v.active) { v.wave = 2; v.piece = 0; }
        double triangleEnergy = 0;
        watchAlloc = true;
        for (int i = 0; i < 4800; ++i)
        {
            float l, r;
            triangleStress.render(l, r);
            triangleEnergy += l * l + r * r;
            if (!std::isfinite(l) || !std::isfinite(r))
            {
                watchAlloc = false;
                throw std::runtime_error("triangle stress finite");
            }
        }
        watchAlloc = false;
        require(triangleEnergy > .001, "wavetable oscillators remain audible");
        require(allocs == 0, "triangle wavetable render allocation-free");
        // Fill the complete pool, release one voice, then confirm the allocator
        // reclaims that tail and preserves its last sample across the transition.
        Synth stealing;
        stealing.prepare(48000);
        stealing.settings.reverb = 0;
        stealing.settings.delay = 0;
        SampleData voiceSample;
        voiceSample.sampleRate = 48000;
        voiceSample.left.assign(48000, .7f);
        voiceSample.right.assign(48000, .6f);
        for (int i = 0; i < 96; ++i)
            stealing.start(i == 0 ? 60 : 61 + i % 2, 1, 0, .8f, 0, true, 0, &voiceSample);
        require(stealing.stolenVoices == 0, "free pool does not steal");
        for (int i = 0; i < 640; ++i)
        {
            float l, r;
            stealing.render(l, r);
        }
        stealing.off(60, 1);
        for (int i = 0; i < 48; ++i)
        {
            float l, r;
            stealing.render(l, r);
        }
        stealing.start(72, 1, 0, 1.f, 0, true, 0, &voiceSample);
        require(stealing.stolenVoices == 1, "pool pressure steals one released voice");
        bool preservedTail = false, stolenReleased = true;
        for (const auto &v : stealing.voices)
        {
            if (v.active && v.note == 60) stolenReleased = false;
            if (v.active && v.note == 72 && v.stealRemaining > 0 &&
                (std::abs(v.stealLeft) + std::abs(v.stealRight)) > 0.000001f)
                preservedTail = true;
        }
        require(stolenReleased && preservedTail, "oldest released voice carries a continuity tail");
        // Many simultaneous replacements must not allocate or produce NaN/Inf.
        const int fadesPerSample = 48;
        watchAlloc = true;
        for (int i = 0; i < 256; ++i)
        {
            stealing.start(50 + i % 36, 1, 0, .8f, 0, true, 0, &voiceSample);
            float l, r;
            for (int j = 0; j < fadesPerSample; ++j)
            {
                stealing.render(l, r);
                if (!std::isfinite(l) || !std::isfinite(r) || std::abs(l) > 1.001f || std::abs(r) > 1.001f)
                {
                    watchAlloc = false;
                    throw std::runtime_error("polyphonic steal finite and bounded");
                }
            }
        }
        watchAlloc = false;
        require(allocs == 0, "polyphonic voice stealing without allocations");
        require(stealing.stolenVoices > 100, "steal path covered repeatedly");
        Synth stress;
        stress.prepare(48000);
        std::mt19937 rng(17);
        stress.settings.reverbDecay = 6;
        stress.settings.reverb = 1;
        for (int block = 0; block < 200; ++block)
        {
            stress.start(24 + int(rng() % 85), 1, int(rng() % 6), 1);
            for (int s = 0; s < 128; ++s)
            {
                float l, r;
                stress.render(l, r);
                require(std::isfinite(l) && std::isfinite(r), "voice stealing stress");
            }
        }
        std::cout
            << "PASS: chess perft through depth 4; castling, en passant, mate, promotion; all scale maps; "
               "DSP at five rates; reset, MIDI release, voice stealing; zero render allocations\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
