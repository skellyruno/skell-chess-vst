#pragma once
#include "Chess.h"
#include "Synth.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace checkmate
{
struct MusicalMove
{
    Move move{};
    int square = 0;          // Destination of the move, 0..63.
    int piece = 0;           // Signed chess piece, -6..6 (nonzero).
    int action = 0;          // 0=normal, 1=capture, 2=check, 3=checkmate.
    int note = -1;           // -1 means calculate from the active scale/voice.
    int velocity = 90;       // MIDI velocity, 1..127.
    int duration = 80;       // Percent of a step, 10..100.
    float tension = .25f;    // Board tension immediately after this move.
    std::string san;
};
struct MusicalGame
{
    std::string name;
    std::vector<MusicalMove> moves;
};
struct ImportResult
{
    std::vector<MusicalGame> games;
    std::string error;
    explicit operator bool() const { return error.empty() && !games.empty(); }
};

inline std::string trimPgn(std::string s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    auto i = s.begin();
    while (i != s.end() && std::isspace(static_cast<unsigned char>(*i))) ++i;
    return std::string(i, s.end());
}
inline bool isResultToken(const std::string &s)
{
    return s == "1-0" || s == "0-1" || s == "1/2-1/2" || s == "*";
}
inline std::string cleanMoveToken(std::string token)
{
    if (token.empty() || token[0] == '$') return {};
    if (isResultToken(token)) return token;
    if (token.rfind("0-0", 0) == 0)
        std::replace(token.begin(), token.end(), '0', 'O');
    // Portable format: 1.e4, 12...Qxd4, 22... etc.
    while (!token.empty() && std::isdigit(static_cast<unsigned char>(token[0])))
        token.erase(token.begin());
    while (!token.empty() && token[0] == '.') token.erase(token.begin());
    if (token.empty()) return {};
    if (token == "e.p." || token == "ep" || token == "e.p") return {};
    while (!token.empty() && (token.back() == '!' || token.back() == '?')) token.pop_back();
    if (token.rfind("0-0", 0) == 0)
        std::replace(token.begin(), token.end(), '0', 'O');
    return token;
}
// Strictly validate the main line against Chess::legal(), including en-passant,
// castling, check, and promotion; never silently drop a bad SAN move.
inline ImportResult importPgn(const std::string &input)
{
    ImportResult out;
    if (input.size() > 2 * 1024 * 1024)
    {
        out.error = "PGN exceeds the 2 MB safety limit";
        return out;
    }
    std::string flattened;
    flattened.reserve(input.size());
    bool brace = false, lineComment = false, tag = false, quoted = false;
    int variations = 0;
    for (size_t i = 0; i < input.size(); ++i)
    {
        const char c = input[i];
        if (lineComment)
        {
            if (c == '\n' || c == '\r') { lineComment = false; flattened += ' '; }
            continue;
        }
        if (brace)
        {
            if (c == '}') brace = false;
            continue;
        }
        if (tag)
        {
            flattened += c;
            if (c == '"' && (i == 0 || input[i - 1] != '\\')) quoted = !quoted;
            if (c == ']' && !quoted) tag = false;
            continue;
        }
        if (c == '{') { brace = true; flattened += ' '; continue; }
        if (c == ';') { lineComment = true; flattened += ' '; continue; }
        if (c == '(') { ++variations; flattened += ' '; continue; }
        if (c == ')')
        {
            if (variations == 0) { out.error = "Unexpected ')' in PGN"; return out; }
            --variations;
            flattened += ' ';
            continue;
        }
        if (variations) continue;
        if (c == '[') { tag = true; quoted = false; flattened += "\n["; continue; }
        flattened += c;
    }
    if (brace || tag || variations)
    {
        out.error = "Unclosed comment, tag, or variation in PGN";
        return out;
    }
    Chess board;
    MusicalGame game;
    std::string white, black, event;
    auto finish = [&]
    {
        if (game.moves.empty()) return;
        if (game.name.empty()) game.name = !white.empty() && !black.empty() ? white + " vs " + black :
                                             !event.empty() ? event : "Imported game " + std::to_string(out.games.size() + 1);
        out.games.push_back(std::move(game));
        game = {};
        board.reset();
        white.clear(); black.clear(); event.clear();
    };
    size_t pos = 0;
    while (pos < flattened.size())
    {
        if (flattened[pos] == '[')
        {
            // Many PGN exports omit the result token. The next tag block still
            // starts a new game; do not attach those moves to the preceding game.
            if (!game.moves.empty())
            {
                finish();
                if (out.games.size() > 32)
                {
                    out.error = "PGN exceeds the 32-game limit";
                    return out;
                }
            }
            auto end = flattened.find(']', pos);
            if (end == std::string::npos) { out.error = "Invalid PGN tag"; return out; }
            std::string tagValue = flattened.substr(pos + 1, end - pos - 1);
            pos = end + 1;
            auto space = tagValue.find_first_of(" \t");
            if (space == std::string::npos) continue;
            std::string key = tagValue.substr(0, space);
            auto q1 = tagValue.find('"', space), q2 = tagValue.rfind('"');
            if (q1 == std::string::npos || q2 <= q1) continue;
            auto value = tagValue.substr(q1 + 1, q2 - q1 - 1);
            if (key == "SetUp" && value == "1")
            {
                out.error = "FEN-start games are not supported in this version";
                return out;
            }
            if (key == "White") white = value;
            if (key == "Black") black = value;
            if (key == "Event") event = value;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(flattened[pos]))) { ++pos; continue; }
        auto end = flattened.find_first_of(" \t\r\n[", pos);
        if (end == std::string::npos) end = flattened.size();
        auto raw = flattened.substr(pos, end - pos);
        pos = end;
        auto moveText = cleanMoveToken(raw);
        if (moveText.empty()) continue;
        if (isResultToken(moveText))
        {
            finish();
            if (out.games.size() > 32)
            {
                out.error = "PGN exceeds the 32-game limit";
                return out;
            }
            continue;
        }
        if (moveText.size() > 32)
        {
            out.error = "Invalid PGN move token (too long)";
            return out;
        }
        Move move;
        if (!board.playSan(moveText, move))
        {
            out.error = "Illegal or unsupported move '" + moveText + "' at ply " +
                        std::to_string(game.moves.size() + 1);
            return out;
        }
        const int p = board.board[move.from];
        const bool capture = board.board[move.to] != 0 ||
                             (std::abs(p) == 1 && move.to == board.ep);
        MusicalMove musical;
        musical.move = move;
        musical.square = move.to;
        musical.piece = p;
        musical.san = board.san(move);
        board.applyUnchecked(move);
        musical.action = board.check(board.turn) ? (board.legal().empty() ? 3 : 2) : (capture ? 1 : 0);
        musical.tension = board.tension();
        game.moves.push_back(std::move(musical));
        if (game.moves.size() > 4096)
        {
            out.error = "A single game exceeds the 4096-move limit";
            return out;
        }
        if (out.games.size() >= 32)
        {
            out.error = "PGN exceeds the 32-game limit";
            return out;
        }
    }
    finish();
    if (out.games.size() > 32) out.error = "PGN exceeds the 32-game limit";
    if (out.games.empty() && out.error.empty()) out.error = "No playable main-line moves found";
    return out;
}
inline bool stepPassesProbability(int64_t step, int cell, double probability)
{
    if (probability >= 1.) return true;
    if (probability <= 0.) return false;
    uint64_t h = uint64_t(step) * UINT64_C(0x9e3779b97f4a7c15) +
                 uint64_t(cell + 1) * UINT64_C(0xbf58476d1ce4e5b9);
    h ^= h >> 30; h *= UINT64_C(0xbf58476d1ce4e5b9);
    h ^= h >> 27; h *= UINT64_C(0x94d049bb133111eb);
    h ^= h >> 31;
    return double(h % 10000) / 10000. < probability;
}
inline constexpr std::array<int, 6> drumNotes{{36, 42, 46, 38, 49, 51}};
inline int moveNote(const MusicalMove &m, int root, int scale, int octave,
                    const std::array<PieceConfig, 6> &pieces, bool drums)
{
    int p = std::clamp(std::abs(m.piece) - 1, 0, 5);
    return m.note >= 0 ? std::clamp(m.note, 0, 127) :
           drums ? drumNotes[size_t(p)] : squareNote(m.square, p, root, scale, octave, pieces);
}
}
