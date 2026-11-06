#include "GameSequence.h"
#include <iostream>
#include <stdexcept>
void check(bool b, const char *what) { if (!b) throw std::runtime_error(what); }
int main()
{
    try
    {
        auto r = checkmate::importPgn(R"([Event "Test"]
[White "Alice"]
[Black "Bob"]
1. e4 {comment} e5 2. Nf3 (2. d4 d5) Nc6 3. Bb5 a6 1-0
[Event "Game two"]
1. f3 e5 2. g4 Qh4# 0-1)");
        check(bool(r), r.error.c_str());
        check(r.games.size() == 2, "two games parsed");
        check(r.games[0].name == "Alice vs Bob", "game title");
        check(r.games[0].moves.size() == 6, "comments and variations ignored");
        check(r.games[1].moves.back().action == 3, "checkmate event");
        auto pawnCapture = checkmate::importPgn("1. e4 d5 2. exd5 Qxd5 *");
        check(bool(pawnCapture), pawnCapture.error.c_str());
        check(pawnCapture.games[0].moves[2].action == 1, "capture event");
        check(!checkmate::importPgn("1. e4 e5 2. O-O-O").error.empty(), "illegal castle rejected");
        check(!checkmate::importPgn("[SetUp \"1\"] 1. e4").error.empty(), "nonstandard FEN rejected");
        check(!checkmate::importPgn("1. e9").error.empty(), "invalid token rejected");
        auto semi = checkmate::importPgn("1.e4;comment\n1...e5 2.Nf3 Nc6 *");
        check(bool(semi) && semi.games[0].moves.size() == 4, "inline numbering and line comments");
        check(checkmate::drumNotes[0] == 36, "drum note mapping");
        check(checkmate::stepPassesProbability(2, 10, 1), "100 percent probability");
        check(!checkmate::stepPassesProbability(2, 10, 0), "zero probability");
        check(checkmate::stepPassesProbability(123, 31, .4) ==
              checkmate::stepPassesProbability(123, 31, .4), "probability deterministic");
        check(r.games[0].moves[0].tension >= .1f && r.games[0].moves[0].tension <= 1.f,
              "per-move chess tension computed");
        checkmate::Chess board;
        for (int style = 0; style < 4; ++style)
        {
            auto move = board.bot(style);
            auto legal = board.legal();
            check(std::find(legal.begin(), legal.end(), move) != legal.end(),
                  "every bot style returns a legal move");
        }
        std::cout << "PASS: PGN multi-game, SAN validation, capture/mate, comments, variations, results, bounds\n";
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
