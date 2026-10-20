#pragma once

#include "card.h"

#include <vector>

namespace bridge {

enum class Scoring {
    Rubber = 0,  // the best of three games
    Chicago = 1, // four deals with set vulnerability
};

// What a played contract is worth, before any game bonus.
struct ContractScore {
    int below = 0;   // declarer's trick score, when made
    int above = 0;   // declarer's overtricks, insult and slam bonus
    int penalty = 0; // the defenders', for undertricks
};

ContractScore scoreContract(const Contract &contract, bool vulnerable, int tricks);

// Four or five trump honours, or all four aces at no trumps, in one hand.
struct Honors {
    int seat = -1;
    int points = 0;
};

Honors honors(const Cards hands[4], int strain);

struct DealRecord {
    int number = 0; // within the rubber, or 1 to 4 in Chicago
    int dealer = 0;
    Contract contract; // not valid when passed out
    int tricks = 0;    // taken by the declarer
    bool vulnerable[2] = {};
    int below[2] = {}; // trick score, towards game
    int above[2] = {}; // everything else
    Honors honors;
    int gameWon = -1;   // the side whose game this deal completed
    int rubberWon = -1; // and whose rubber

    bool passedOut() const { return !contract.valid(); }
    bool made() const { return contract.valid() && tricks >= contract.target(); }
    int total(int side) const { return below[side] + above[side]; }
};

class ScoreSheet {
public:
    explicit ScoreSheet(Scoring mode = Scoring::Rubber) { reset(mode); }

    void reset(Scoring mode);
    Scoring mode() const { return m_mode; }

    // For the deal about to be played, dealt by `dealer`.
    bool vulnerable(int side, int dealer) const;
    // The deal about to be played: rubber deals count on, Chicago's run 1 to 4.
    int dealNumber() const { return m_number; }
    void record(int dealer, const Contract &contract, int tricks, const Honors &honors);

    bool finished() const { return m_finished; }
    // The side with more points, or -1 on a tie.
    int winner() const;
    int games(int side) const { return m_games[side]; }
    // Below the line towards the game being played.
    int partScore(int side) const { return m_part[side]; }
    int total(int side) const;
    const std::vector<DealRecord> &deals() const { return m_deals; }

private:
    Scoring m_mode = Scoring::Rubber;
    std::vector<DealRecord> m_deals;
    int m_number = 1;
    int m_games[2] = {};
    int m_part[2] = {};
    bool m_finished = false;
};

} // namespace bridge
