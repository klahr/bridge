#pragma once

#include "game.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace bridge {

// Plays a position out with every hand in view and best play all round.
class Solver {
public:
    // Tricks north-south take from here on, the trick in progress
    // included. `trick` holds its cards so far, led by `leader`.
    int solve(const Cards hands[4], int trump, int leader, const std::vector<Play> &trick);
    // The same after each of `moves`, played by whoever is next.
    std::vector<int> solveMoves(const Cards hands[4], int trump, int leader, const std::vector<Play> &trick,
                                const Cards &moves);
    // Positions searched by the last solve.
    long nodes() const { return m_nodes; }

private:
    struct Key {
        uint64_t hands[4];
        int leader;
        bool operator==(const Key &o) const
        {
            return leader == o.leader && hands[0] == o.hands[0] && hands[1] == o.hands[1] && hands[2] == o.hands[2]
                   && hands[3] == o.hands[3];
        }
    };
    struct KeyHash {
        size_t operator()(const Key &k) const;
    };
    struct Bounds {
        int lo, hi;
    };

    void load(const Cards hands[4], int trump, int leader, const std::vector<Play> &trick);
    int search(int alpha, int beta);
    int moves(int player, int out[13]) const;
    int winner() const;

    uint64_t m_hands[4] = {};
    int m_trump = 0;
    int m_leader = 0;
    int m_count = 0;
    int m_trick[4] = {};
    long m_nodes = 0;
    std::unordered_map<Key, Bounds, KeyHash> m_table;
};

} // namespace bridge
