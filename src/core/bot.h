#pragma once

#include "game.h"
#include "solver.h"

#include <random>

namespace bridge {

enum class Difficulty { Easy = 0, Medium = 1, Hard = 2 };

// Bids by the system of its side, at every level, and plays the cards
// better the higher the level.
class Bot {
public:
    explicit Bot(Difficulty difficulty = Difficulty::Medium, unsigned seed = std::random_device{}());

    void setDifficulty(Difficulty d) { m_difficulty = d; }
    Difficulty difficulty() const { return m_difficulty; }

    Call chooseCall(const Game &game, int player);
    // A legal card for `player`, whose turn it is; dummy's cards are
    // chosen as the declarer sees them.
    Card choosePlay(const Game &game, int player);

    // Rules of thumb: second hand low, third hand high, cash winners and
    // draw trumps, lead partner's suit. Careful play looks further, and
    // also plays out every deal the hard bot tries.
    static Card rulePlay(const Game &game, int player, bool careful);

private:
    Card playSimulated(const Game &game, int player, const Cards &legal);
    // Deals the cards `viewer` cannot see to the others, consistent with
    // the suits they have shown out of and, as far as it goes, with what
    // they have shown in the bidding.
    bool guessHands(const Game &game, int viewer, const Analysis &analysis, Cards hands[Game::Players]);

    Difficulty m_difficulty;
    std::mt19937 m_rng;
    Solver m_solver;
};

} // namespace bridge
