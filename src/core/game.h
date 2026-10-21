#pragma once

#include "auction.h"
#include "bidding.h"
#include "scoring.h"

#include <random>

namespace bridge {

enum class Phase {
    Bidding,
    Play,     // thirteen tricks
    Score,    // the deal is over and counted
    GameOver, // the rubber, or the four deals of Chicago
};

struct Play {
    int player;
    Card card;
};

// Four players round the table in playing order: 0 is south (you), 1 west,
// 2 north and 3 east, south and north against west and east. Dealing,
// bidding and play go clockwise, to the next number.
class Game {
public:
    static constexpr int Players = 4;
    static constexpr int HandSize = 13;

    explicit Game(unsigned seed = std::random_device{}());

    // Takes effect with the next game.
    void setScoring(Scoring scoring) { m_scoring = scoring; }
    Scoring scoring() const { return m_sheet.mode(); }
    Bidding &bidding() { return m_bidding; }
    const Bidding &bidding() const { return m_bidding; }

    // Clears the score and deals the first deal, from a random dealer.
    void newGame();
    // As above, from `deck` instead of a shuffled pack; used by tests.
    void newGame(const Cards &deck, int dealer = 0);
    // Deals the next deal from `deck`, card i to the player i after the dealer.
    void dealFrom(const Cards &deck);

    bool call(int player, const Call &call);
    bool play(int player, const Card &card);
    // Gives a finished trick to its winner, who leads the next.
    bool collectTrick();
    // After the deal is counted, deals the next one.
    bool nextDeal();
    // As above, from `deck` instead of a shuffled pack.
    bool nextDeal(const Cards &deck);

    bool canPlayCard(int player, const Card &card) const;
    Cards legalCards(int player) const;

    Phase phase() const { return m_phase; }
    int dealer() const { return m_dealer; }
    const Auction &auction() const { return m_auction; }
    // Valid once the bidding is over, unless it was passed out.
    const Contract &contract() const { return m_contract; }
    int declarer() const { return m_contract.declarer; }
    int dummy() const { return m_contract.valid() ? partnerOf(m_contract.declarer) : -1; }
    // Dummy's cards go down after the opening lead.
    bool dummyVisible() const { return m_contract.valid() && (!m_played.empty() || m_phase != Phase::Play); }
    // Who chooses the cards for `seat`: the declarer plays dummy's.
    int controller(int seat) const { return seat == dummy() ? m_contract.declarer : seat; }
    bool vulnerable(int side) const { return m_sheet.vulnerable(side, m_dealer); }

    const Cards &hand(int player) const { return m_hand[player]; }
    // The hands as dealt.
    const Cards &dealt(int player) const { return m_dealt[player]; }
    const std::vector<Play> &trick() const { return m_trick; }
    // Every card played this deal, in order, the current trick included.
    const std::vector<Play> &played() const { return m_played; }
    // The suit of the trick's first card, -1 before it is led.
    int ledSuit() const { return m_trick.empty() ? -1 : m_trick.front().card.suit; }
    int leader() const { return m_leader; }
    // Whose call or card it is; -1 while a finished trick is still on the table.
    int turn() const;
    bool trickComplete() const { return m_trick.size() == Players; }
    int trickWinner() const;
    int tricksPlayed() const { return m_tricks[0] + m_tricks[1]; }
    // Tricks taken by north-south (0) and east-west (1).
    int tricks(int side) const { return m_tricks[side]; }

    const ScoreSheet &score() const { return m_sheet; }
    // The deal just counted.
    const DealRecord *lastDeal() const { return m_sheet.deals().empty() ? nullptr : &m_sheet.deals().back(); }

    // Lets the bots try out guessed hands on a copy of the game.
    void replaceHand(int player, const Cards &cards) { m_hand[player] = cards; }

private:
    void startPlay();
    void scoreDeal();

    std::mt19937 m_rng;
    Scoring m_scoring = Scoring::Rubber;
    Bidding m_bidding;
    ScoreSheet m_sheet;
    Phase m_phase = Phase::Bidding;
    int m_dealer = 0;
    Auction m_auction;
    Contract m_contract;
    Cards m_hand[Players];
    Cards m_dealt[Players];
    std::vector<Play> m_trick;
    std::vector<Play> m_played;
    int m_leader = 0;
    int m_turn = -1;
    int m_tricks[2] = {};
};

} // namespace bridge
