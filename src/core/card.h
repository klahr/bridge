#pragma once

#include "cards/card.h"

#include <string>
#include <vector>

namespace bridge {

using cards::Card;
using cards::Cards;
using cards::Suit;
using cards::Clubs;
using cards::Diamonds;
using cards::Hearts;
using cards::Spades;
using cards::parseCard;
using cards::parseCards;
using cards::fullDeck;
using cards::contains;
using cards::removeCard;
using cards::countSuit;

// Strains rank as the suits do, from clubs up, with no trumps on top.
constexpr int NoTrump = 4;

// Seats round the table in playing order: 0 is south (you), 1 west,
// 2 north and 3 east. North-south are side 0, east-west side 1.
inline int partnerOf(int seat) { return (seat + 2) % 4; }
inline int sideOf(int seat) { return seat % 2; }

// Ace four, king three, queen two, jack one.
inline int hcp(const Card &c) { return c.rank == 1 ? 4 : c.rank >= 11 ? c.rank - 10 : 0; }
int hcp(const Cards &cards);

// Clubs, diamonds, hearts, spades, each from the two up.
void sortHand(Cards &cards);

// Pass, double, redouble or a bid of one to seven in a strain.
struct Call {
    enum Kind { Pass = 0, Double = 1, Redouble = 2, Bid = 3 };

    Kind kind = Pass;
    int level = 0;
    int strain = 0;

    Call() = default;
    Call(int l, int s) : kind(Bid), level(l), strain(s) {}
    static Call pass() { return Call(); }
    static Call dbl() { Call c; c.kind = Double; return c; }
    static Call redbl() { Call c; c.kind = Redouble; return c; }

    bool isBid() const { return kind == Bid; }
    // Bids in order, 1 for one club up to 35 for seven no trumps.
    int rank() const { return isBid() ? (level - 1) * 5 + strain + 1 : 0; }
    // 0 pass, 1 double, 2 redouble, then 3 + the bid's rank - 1; the ids
    // the user interface uses.
    int id() const { return isBid() ? rank() + 2 : static_cast<int>(kind); }
    static Call fromId(int id);
    bool valid() const { return !isBid() || (level >= 1 && level <= 7 && strain >= 0 && strain <= NoTrump); }

    bool operator==(const Call &o) const { return id() == o.id(); }
    bool operator!=(const Call &o) const { return !(*this == o); }
};

// "P", "X", "XX", "1C" ... "7NT", as used by the tests.
Call parseCall(const std::string &text);
std::vector<Call> parseCalls(const std::string &text);

struct Contract {
    int level = 0; // 0 when the deal was passed out
    int strain = 0;
    int doubled = 0; // 1 doubled, 2 redoubled
    int declarer = -1;

    bool valid() const { return level > 0; }
    // Tricks the declarer needs.
    int target() const { return level + 6; }
};

} // namespace bridge
