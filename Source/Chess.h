#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>
namespace checkmate
{
// Signed pieces: white positive, black negative. Rows run from rank 8 to rank 1.
enum Piece
{
    empty,
    pawn,
    knight,
    bishop,
    rook,
    queen,
    king
};
struct Move
{
    int from = -1, to = -1;
    bool operator==(const Move &m) const { return from == m.from && to == m.to; }
};
struct Chess
{
    std::array<int, 64> board{};
    int turn = 1, rights = 15, ep = -1, halfmoves = 0;
    Chess() { reset(); }
    static bool inside(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
    static std::string square(int s) { return std::string(1, char('a' + s % 8)) + char('8' - s / 8); }
    void reset()
    {
        board.fill(0);
        const int back[] = {rook, knight, bishop, queen, king, bishop, knight, rook};
        for (int c = 0; c < 8; ++c)
        {
            board[c] = -back[c];
            board[8 + c] = -pawn;
            board[48 + c] = pawn;
            board[56 + c] = back[c];
        }
        turn = 1;
        rights = 15;
        ep = -1;
        halfmoves = 0;
    }
    bool attacked(int sq, int by) const
    {
        const int r = sq / 8, c = sq % 8;
        for (int s = 0; s < 64; ++s)
        {
            int p = board[s];
            if (p * by <= 0)
                continue;
            int dr = r - s / 8, dc = c - s % 8, t = std::abs(p);
            if (t == pawn && dr == -by && std::abs(dc) == 1)
                return true;
            if (t == knight && std::abs(dr) * std::abs(dc) == 2)
                return true;
            if (t == king && std::max(std::abs(dr), std::abs(dc)) == 1)
                return true;
            bool ray = (t == bishop || t == queen) && std::abs(dr) == std::abs(dc) && dr != 0;
            ray = ray || ((t == rook || t == queen) && ((dr == 0) != (dc == 0)));
            if (!ray)
                continue;
            int rr = s / 8, cc = s % 8, sr = (dr > 0) - (dr < 0), sc = (dc > 0) - (dc < 0);
            rr += sr;
            cc += sc;
            bool clear = true;
            while (rr != r || cc != c)
            {
                if (board[rr * 8 + cc])
                {
                    clear = false;
                    break;
                }
                rr += sr;
                cc += sc;
            }
            if (clear)
                return true;
        }
        return false;
    }
    bool check(int side) const
    {
        auto k = std::find(board.begin(), board.end(), side * king);
        return k == board.end() || attacked(int(k - board.begin()), -side);
    }
    void applyUnchecked(Move m)
    {
        int p = board[m.from], captured = board[m.to], side = p > 0 ? 1 : -1;
        if (std::abs(p) == pawn && m.to == ep && !captured)
            board[m.to + side * 8] = 0;
        if (std::abs(p) == king && std::abs(m.to - m.from) == 2)
        {
            int r = m.from / 8 * 8;
            if (m.to > m.from)
            {
                board[r + 5] = board[r + 7];
                board[r + 7] = 0;
            }
            else
            {
                board[r + 3] = board[r];
                board[r] = 0;
            }
        }
        if (m.from == 60)
            rights &= ~3;
        if (m.from == 4)
            rights &= ~12;
        if (m.from == 63 || m.to == 63)
            rights &= ~1;
        if (m.from == 56 || m.to == 56)
            rights &= ~2;
        if (m.from == 7 || m.to == 7)
            rights &= ~4;
        if (m.from == 0 || m.to == 0)
            rights &= ~8;
        ep = std::abs(p) == pawn && std::abs(m.to - m.from) == 16 ? (m.from + m.to) / 2 : -1;
        board[m.from] = 0;
        board[m.to] = (std::abs(p) == pawn && (m.to / 8 == 0 || m.to / 8 == 7)) ? side * queen : p;
        halfmoves = (std::abs(p) == pawn || captured) ? 0 : halfmoves + 1;
        turn = -turn;
    }
    std::vector<Move> legal() const
    {
        std::vector<Move> out;
        for (int s = 0; s < 64; ++s)
        {
            int p = board[s];
            if (p * turn <= 0)
                continue;
            int r = s / 8, c = s % 8, t = std::abs(p);
            auto add = [&](int rr, int cc)
            {
                if (!inside(rr, cc))
                    return;
                int target = board[rr * 8 + cc];
                if (target * turn > 0 || std::abs(target) == king)
                    return;
                Move m{s, rr * 8 + cc};
                Chess next = *this;
                next.applyUnchecked(m);
                if (!next.check(turn))
                    out.push_back(m);
            };
            if (t == pawn)
            {
                int rr = r - turn;
                if (inside(rr, c) && !board[rr * 8 + c])
                {
                    add(rr, c);
                    if (r == (turn == 1 ? 6 : 1) && !board[(rr - turn) * 8 + c])
                        add(rr - turn, c);
                }
                for (int dc : {-1, 1})
                    if (inside(rr, c + dc))
                    {
                        int target = rr * 8 + c + dc;
                        if (board[target] * turn < 0 ||
                            (target == ep && board[target + turn * 8] == -turn * pawn))
                            add(rr, c + dc);
                    }
            }
            else if (t == knight)
            {
                for (int dr : {-2, -1, 1, 2})
                    for (int dc : {-2, -1, 1, 2})
                        if (std::abs(dr * dc) == 2)
                            add(r + dr, c + dc);
            }
            else if (t == king)
            {
                for (int dr = -1; dr <= 1; ++dr)
                    for (int dc = -1; dc <= 1; ++dc)
                        if (dr || dc)
                            add(r + dr, c + dc);
                int home = turn == 1 ? 60 : 4, bit = turn == 1 ? 1 : 4;
                if (s == home && !check(turn))
                {
                    if ((rights & bit) && board[s + 3] == turn * rook && !board[s + 1] && !board[s + 2] &&
                        !attacked(s + 1, -turn) && !attacked(s + 2, -turn))
                        add(r, 6);
                    if ((rights & (bit * 2)) && board[s - 4] == turn * rook && !board[s - 1] &&
                        !board[s - 2] && !board[s - 3] && !attacked(s - 1, -turn) && !attacked(s - 2, -turn))
                        add(r, 2);
                }
            }
            else
            {
                for (int dr = -1; dr <= 1; ++dr)
                    for (int dc = -1; dc <= 1; ++dc)
                    {
                        if (!dr && !dc)
                            continue;
                        if (t == bishop && (!dr || !dc))
                            continue;
                        if (t == rook && dr && dc)
                            continue;
                        for (int k = 1; inside(r + dr * k, c + dc * k); ++k)
                        {
                            int rr = r + dr * k, cc = c + dc * k;
                            add(rr, cc);
                            if (board[rr * 8 + cc])
                                break;
                        }
                    }
            }
        }
        return out;
    }
    std::string san(Move m) const
    {
        int p = std::abs(board[m.from]);
        bool capture = board[m.to] != 0 || (p == pawn && m.to == ep);
        std::string s;
        if (p == king && std::abs(m.to - m.from) == 2)
            s = m.to > m.from ? "O-O" : "O-O-O";
        else
        {
            if (p != pawn)
            {
                s += " PNBRQK"[p];
                bool sameFile = false, sameRank = false, other = false;
                for (auto x : legal())
                    if (x.to == m.to && x.from != m.from && board[x.from] == board[m.from])
                    {
                        other = true;
                        sameFile |= x.from % 8 == m.from % 8;
                        sameRank |= x.from / 8 == m.from / 8;
                    }
                if (other)
                {
                    if (!sameFile)
                        s += char('a' + m.from % 8);
                    else if (!sameRank)
                        s += char('8' - m.from / 8);
                    else
                        s += square(m.from);
                }
            }
            else if (capture)
                s += char('a' + m.from % 8);
            if (capture)
                s += 'x';
            s += square(m.to);
            if (p == pawn && (m.to / 8 == 0 || m.to / 8 == 7))
                s += "=Q";
        }
        Chess next = *this;
        next.applyUnchecked(m);
        if (next.check(next.turn))
            s += next.legal().empty() ? '#' : '+';
        return s;
    }
    bool playSan(std::string text, Move &result)
    {
        auto strip = [](std::string s)
        {
            s.erase(std::remove_if(s.begin(), s.end(),
                                   [](char c) { return c == '+' || c == '#' || c == '!' || c == '?'; }),
                    s.end());
            return s;
        };
        for (auto m : legal())
            if (strip(san(m)) == strip(text))
            {
                result = m;
                return true;
            }
        return false;
    }
    float tension() const
    {
        int pieces = 0, pressure = 0, center = 0;
        for (int s = 0; s < 64; ++s)
            if (board[s])
            {
                ++pieces;
                if ((s / 8 == 3 || s / 8 == 4) && (s % 8 == 3 || s % 8 == 4))
                    ++center;
                if (attacked(s, board[s] > 0 ? -1 : 1))
                    ++pressure;
            }
        return std::clamp(pressure * .08f + center * .075f + (32 - pieces) * .015f, .1f, 1.f);
    }
    Move bot() const
    {
        auto moves = legal();
        Move best;
        int score = -1000000;
        constexpr int values[] = {0, 100, 320, 330, 500, 900, 20000};
        for (auto m : moves)
        {
            Chess n = *this;
            n.applyUnchecked(m);
            int v = 0;
            for (int s = 0; s < 64; ++s)
                if (n.board[s])
                    v += (n.board[s] * turn > 0 ? 1 : -1) *
                         (values[std::abs(n.board[s])] +
                          ((s / 8 >= 2 && s / 8 <= 5 && s % 8 >= 2 && s % 8 <= 5) ? 20 : 0));
            int worst = 0;
            for (auto reply : n.legal())
                worst = std::max(worst, values[std::abs(n.board[reply.to])]);
            v -= worst;
            if (n.check(n.turn))
                v += 50;
            if (n.legal().empty() && n.check(n.turn))
                v += 100000;
            if (v > score)
            {
                score = v;
                best = m;
            }
        }
        return best;
    }
};
} // namespace checkmate
