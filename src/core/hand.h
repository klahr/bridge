#pragma once

#include "card.h"

namespace bridge {

// What a bidder looks at in a hand.
struct HandEval {
    explicit HandEval(const Cards &cards);

    Cards cards;
    int hcp = 0;
    int len[4] = {};
    int aces = 0;
    int kings = 0;
    // No singleton or void and at most one doubleton: 4-3-3-3, 4-4-3-2, 5-3-3-2.
    bool balanced = false;
    // High card points and one for every card past the fourth in a suit.
    int total = 0;
    // Tricks the hand takes on its own with its longest suit as trumps.
    int playingTricks = 0;

    // High card points and the short suits, with `trump` as trumps.
    int support(int trump) const;
    // A, Kx, Qxx or Jxxx: the suit can be held up at no trumps.
    bool stopper(int suit) const;
    // Of the ace, king, queen, jack and ten.
    int honors(int suit) const;
    // Of the ace, king and queen.
    int topHonors(int suit) const;
    bool holds(int rank, int suit) const;
    // The longest suit, the higher one of equal length.
    int longest() const;
};

} // namespace bridge
