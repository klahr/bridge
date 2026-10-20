#include "hand.h"

#include <algorithm>

namespace bridge {

HandEval::HandEval(const Cards &c)
    : cards(c)
{
    for (const Card &card : cards) {
        hcp += bridge::hcp(card);
        ++len[card.suit];
        aces += card.rank == 1;
        kings += card.rank == 13;
    }
    int doubletons = 0;
    bool shortSuit = false;
    for (int s = 0; s < 4; ++s) {
        doubletons += len[s] == 2;
        shortSuit = shortSuit || len[s] < 2;
        total += std::max(0, len[s] - 4);
    }
    total += hcp;
    balanced = !shortSuit && doubletons <= 1;

    // Losers among the top three cards of each suit; the rest of a suit
    // are winners.
    for (int s = 0; s < 4; ++s) {
        const int top = std::min(len[s], 3);
        int losers = top;
        if (holds(1, s))
            --losers;
        if (top >= 2 && holds(13, s))
            --losers;
        if (top >= 3 && holds(12, s))
            --losers;
        playingTricks += len[s] - losers;
    }
}

bool HandEval::holds(int rank, int suit) const
{
    return contains(cards, Card(rank, suit));
}

int HandEval::support(int trump) const
{
    int points = hcp;
    if (trump < 0 || trump > 3 || len[trump] < 3)
        return points;
    const bool four = len[trump] >= 4;
    for (int s = 0; s < 4; ++s) {
        if (s == trump)
            continue;
        if (len[s] == 0)
            points += four ? 5 : 3;
        else if (len[s] == 1)
            points += four ? 3 : 2;
        else if (len[s] == 2)
            points += 1;
    }
    return points;
}

bool HandEval::stopper(int suit) const
{
    if (holds(1, suit))
        return true;
    if (holds(13, suit) && len[suit] >= 2)
        return true;
    if (holds(12, suit) && len[suit] >= 3)
        return true;
    return holds(11, suit) && len[suit] >= 4;
}

int HandEval::honors(int suit) const
{
    return holds(1, suit) + holds(13, suit) + holds(12, suit) + holds(11, suit) + holds(10, suit);
}

int HandEval::topHonors(int suit) const
{
    return holds(1, suit) + holds(13, suit) + holds(12, suit);
}

int HandEval::longest() const
{
    int best = Spades;
    for (int s = Hearts; s >= Clubs; --s)
        if (len[s] > len[best])
            best = s;
    return best;
}

} // namespace bridge
