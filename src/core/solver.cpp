#include "solver.h"

#include <algorithm>

namespace bridge {

namespace {

// A card is bit suit * 16 + rank, rank 0 for the two up to 12 for the ace.
int bitOf(const Card &c) { return c.suit * 16 + c.high() - 2; }
int suitOf(int bit) { return bit >> 4; }
uint64_t suitMask(int suit) { return uint64_t(0x1fff) << (suit * 16); }

uint64_t mask(const Cards &cards)
{
    uint64_t m = 0;
    for (const Card &c : cards)
        m |= uint64_t(1) << bitOf(c);
    return m;
}

int count(uint64_t m) { return __builtin_popcountll(m); }

} // namespace

size_t Solver::KeyHash::operator()(const Key &k) const
{
    uint64_t h = 0x9e3779b97f4a7c15ull * (k.leader + 1);
    for (uint64_t v : k.hands) {
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h *= 0xff51afd7ed558ccdull;
    }
    return static_cast<size_t>(h ^ (h >> 33));
}

void Solver::load(const Cards hands[4], int trump, int leader, const std::vector<Play> &trick)
{
    for (int p = 0; p < 4; ++p)
        m_hands[p] = mask(hands[p]);
    m_trump = trump;
    m_leader = leader;
    m_count = static_cast<int>(trick.size());
    for (int i = 0; i < m_count; ++i)
        m_trick[i] = bitOf(trick[i].card);
    m_nodes = 0;
    m_table.clear();
}

int Solver::solve(const Cards hands[4], int trump, int leader, const std::vector<Play> &trick)
{
    load(hands, trump, leader, trick);
    return search(-1, 14);
}

std::vector<int> Solver::solveMoves(const Cards hands[4], int trump, int leader, const std::vector<Play> &trick,
                                    const Cards &moves)
{
    load(hands, trump, leader, trick);
    std::vector<int> out;
    const int player = (m_leader + m_count) % 4;
    for (const Card &c : moves) {
        const uint64_t bit = uint64_t(1) << bitOf(c);
        m_hands[player] &= ~bit;
        m_trick[m_count++] = bitOf(c);
        int v;
        if (m_count == 4) {
            const int w = winner();
            const int won = w % 2 == 0;
            const int saved = m_leader;
            int cards[4];
            std::copy(m_trick, m_trick + 4, cards);
            m_leader = w;
            m_count = 0;
            v = won + search(-1, 14);
            std::copy(cards, cards + 4, m_trick);
            m_count = 4;
            m_leader = saved;
        } else {
            v = search(-1, 14);
        }
        --m_count;
        m_hands[player] |= bit;
        out.push_back(v);
    }
    return out;
}

int Solver::winner() const
{
    int best = 0;
    for (int i = 1; i < 4; ++i) {
        const int c = m_trick[i];
        const int b = m_trick[best];
        if (suitOf(c) == suitOf(b)) {
            if (c > b)
                best = i;
        } else if (suitOf(c) == m_trump) {
            best = i;
        }
    }
    return (m_leader + best) % 4;
}

int Solver::moves(int player, int out[13]) const
{
    uint64_t h = m_hands[player];
    int current = -1; // the card winning the trick so far
    if (m_count > 0) {
        const uint64_t follow = h & suitMask(suitOf(m_trick[0]));
        if (follow)
            h = follow;
        current = 0;
        for (int i = 1; i < m_count; ++i) {
            const int c = m_trick[i];
            const int b = m_trick[current];
            if ((suitOf(c) == suitOf(b) && c > b) || (suitOf(c) == m_trump && suitOf(b) != m_trump))
                current = i;
        }
    }
    const bool partnerWins = current >= 0 && (m_leader + current) % 4 % 2 == player % 2;
    uint64_t all = m_hands[0] | m_hands[1] | m_hands[2] | m_hands[3];
    for (int i = 0; i < m_count; ++i)
        all |= uint64_t(1) << m_trick[i];

    // One card of each run: cards with nothing left between them are the same.
    int n = 0;
    int score[13];
    for (int s = 0; s < 4; ++s) {
        bool previousMine = false;
        bool top = true;
        for (int bit = s * 16 + 12; bit >= s * 16; --bit) {
            const uint64_t b = uint64_t(1) << bit;
            if (!(all & b))
                continue;
            if (h & b) {
                if (!previousMine) {
                    int value;
                    if (m_count == 0) {
                        value = (top ? 40 : 0) + (bit & 15);
                    } else {
                        const int w = m_trick[current];
                        const bool beats = (suitOf(bit) == suitOf(w) && bit > w) || (suitOf(bit) == m_trump && suitOf(w) != m_trump);
                        value = partnerWins || !beats ? -(bit & 15) : 40 - (bit & 15);
                    }
                    score[n] = value;
                    out[n++] = bit;
                }
                previousMine = true;
            } else {
                previousMine = false;
            }
            top = false;
        }
    }
    // Most promising first.
    for (int i = 1; i < n; ++i) {
        for (int j = i; j > 0 && score[j] > score[j - 1]; --j) {
            std::swap(score[j], score[j - 1]);
            std::swap(out[j], out[j - 1]);
        }
    }
    return n;
}

int Solver::search(int alpha, int beta)
{
    ++m_nodes;
    const bool start = m_count == 0;
    Key key;
    if (start) {
        const uint64_t all = m_hands[0] | m_hands[1] | m_hands[2] | m_hands[3];
        const int left = count(all) / 4;
        if (left == 0)
            return 0;
        key = {{m_hands[0], m_hands[1], m_hands[2], m_hands[3]}, m_leader};
        auto it = m_table.find(key);
        if (it != m_table.end()) {
            if (it->second.lo >= beta)
                return it->second.lo;
            if (it->second.hi <= alpha)
                return it->second.hi;
            alpha = std::max(alpha, it->second.lo);
            beta = std::min(beta, it->second.hi);
        }
    }
    const int alpha0 = alpha;
    const int beta0 = beta;
    const int player = (m_leader + m_count) % 4;
    const bool maximise = player % 2 == 0;
    int list[13];
    const int n = moves(player, list);
    int best = maximise ? -1 : 99;
    for (int i = 0; i < n; ++i) {
        const int bit = list[i];
        const uint64_t b = uint64_t(1) << bit;
        m_hands[player] &= ~b;
        m_trick[m_count++] = bit;
        int v;
        if (m_count == 4) {
            const int w = winner();
            const int won = w % 2 == 0;
            // The next trick reuses the table; keep this one's cards.
            const int saved = m_leader;
            int cards[4];
            std::copy(m_trick, m_trick + 4, cards);
            m_leader = w;
            m_count = 0;
            v = won + search(alpha - won, beta - won);
            std::copy(cards, cards + 4, m_trick);
            m_count = 4;
            m_leader = saved;
        } else {
            v = search(alpha, beta);
        }
        --m_count;
        m_hands[player] |= b;
        if (maximise) {
            best = std::max(best, v);
            alpha = std::max(alpha, best);
        } else {
            best = std::min(best, v);
            beta = std::min(beta, best);
        }
        if (alpha >= beta)
            break;
    }
    if (start) {
        Bounds &e = m_table.try_emplace(key, Bounds{0, 13}).first->second;
        if (best <= alpha0)
            e.hi = std::min(e.hi, best);
        else if (best >= beta0)
            e.lo = std::max(e.lo, best);
        else
            e.lo = e.hi = best;
        if (m_table.size() > 4000000)
            m_table.clear();
    }
    return best;
}

} // namespace bridge
