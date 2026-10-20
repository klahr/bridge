// Rule tests for the bridge engine. Plain C++ so they build anywhere:
//   make -C tests

#include "../src/core/bot.h"
#include "../src/core/game.h"
#include "../src/core/solver.h"
#include "cards/testing.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace bridge;

namespace {

// A deck that deals each player the cards given for them, south first;
// the rest of the pack fills up the hands in order.
Cards stackedDeck(const std::vector<std::string> &given, int dealer = 0)
{
    Cards hands[Game::Players];
    Cards used;
    for (size_t p = 0; p < given.size(); ++p) {
        hands[p] = parseCards(given[p]);
        used.insert(used.end(), hands[p].begin(), hands[p].end());
    }
    int p = 0;
    for (const Card &c : fullDeck()) {
        if (contains(used, c))
            continue;
        while (hands[p].size() == Game::HandSize)
            ++p;
        hands[p].push_back(c);
    }
    // Card i goes to the player i after the dealer.
    Cards deck;
    for (int i = 0; i < Game::HandSize * Game::Players; ++i)
        deck.push_back(hands[(dealer + 1 + i) % Game::Players][i / Game::Players]);
    return deck;
}

Auction auction(int dealer, const std::string &calls)
{
    Auction a(dealer);
    for (const Call &c : parseCalls(calls))
        CHECK(a.add(c));
    return a;
}

// What a hand bids next in the auction, by the system given to both sides.
Call next(SystemId system, const std::string &hand, const std::string &calls, int dealer = 0)
{
    Bidding b(system, system);
    return b.choose(auction(dealer, calls), parseCards(hand));
}

Meaning meaningOf(SystemId system, const std::string &calls, size_t i, int dealer = 0)
{
    Bidding b(system, system);
    return b.analyse(auction(dealer, calls)).meanings[i];
}

bool is(const Call &c, const std::string &text) { return c == parseCall(text); }

int score(const std::string &contract, int doubled, bool vul, int tricks)
{
    const Call c = parseCall(contract);
    Contract k;
    k.level = c.level;
    k.strain = c.strain;
    k.doubled = doubled;
    k.declarer = 0;
    const ContractScore s = scoreContract(k, vul, tricks);
    return s.below + s.above - s.penalty;
}

// --------------------------------------------------------------- cards

TEST(cards_and_calls)
{
    CHECK_EQ(hcp(parseCards("AS KH QD JC 10S")), 10);
    Cards hand = parseCards("2C AD KS 3H AH 5D");
    sortHand(hand);
    CHECK(hand == parseCards("KS AH 3H 2C AD 5D"));

    for (int id = 0; id <= 37; ++id)
        CHECK_EQ(Call::fromId(id).id(), id);
    CHECK(Call(1, Clubs).rank() < Call(1, NoTrump).rank());
    CHECK(Call(1, NoTrump).rank() < Call(2, Clubs).rank());
    CHECK(is(Call(3, NoTrump), "3NT"));
    CHECK(is(Call::dbl(), "X"));
}

TEST(hand_evaluation)
{
    const HandEval e(parseCards("AS KS 5S 4S 3S KH 2H QD 3D 2D 4C 3C 2C"));
    CHECK_EQ(e.hcp, 12);
    CHECK_EQ(e.len[Spades], 5);
    CHECK_EQ(e.total, 13);
    CHECK(e.balanced);
    CHECK(e.stopper(Spades) && e.stopper(Hearts) && e.stopper(Diamonds) && !e.stopper(Clubs));
    // Three hearts and a singleton diamond: two for the singleton.
    const HandEval f(parseCards("AS KS QS JS 3H 2H 4H 2D 5C 4C 3C 2C 6C"));
    CHECK(!f.balanced);
    CHECK_EQ(f.support(Hearts), 10 + 2);
}

// ------------------------------------------------------------- auction

TEST(auction_order_and_end)
{
    Auction a(1);
    CHECK_EQ(a.turn(), 1);
    CHECK(!a.isLegal(Call::dbl()));     // nothing to double
    CHECK(a.add(parseCall("1H")));
    CHECK(!a.isLegal(Call::redbl()));
    CHECK(!a.isLegal(parseCall("1D"))); // too low
    CHECK(!a.isLegal(parseCall("1H")));
    CHECK(a.isLegal(parseCall("1S")));
    CHECK(a.add(Call::dbl()));
    CHECK(!a.isLegal(Call::dbl()));     // partner's double
    CHECK(a.isLegal(Call::redbl()));
    CHECK(a.add(Call::pass()));
    CHECK(!a.isLegal(Call::dbl()));     // already doubled
    CHECK(a.add(Call::pass()));
    CHECK(!a.finished());
    CHECK(a.add(Call::pass()));
    CHECK(a.finished());
    const Contract c = a.contract();
    CHECK_EQ(c.level, 1);
    CHECK_EQ(c.strain, Hearts);
    CHECK_EQ(c.doubled, 1);
    CHECK_EQ(c.declarer, 1);
    CHECK(!a.add(Call::pass()));

    Auction out(0);
    for (int i = 0; i < 3; ++i)
        CHECK(out.add(Call::pass()));
    CHECK(!out.finished());
    CHECK(out.add(Call::pass()));
    CHECK(out.passedOut());
}

TEST(declarer_named_the_strain_first)
{
    // South opens 1H, north raises to 4H: south declares.
    Auction a = auction(0, "1H P 2H P 4H P P P");
    CHECK_EQ(a.contract().declarer, 0);
    // West bids spades first, east ends up in them.
    Auction b = auction(1, "1D P 1S P 2S X XX P P P");
    CHECK_EQ(b.contract().declarer, 3);
    CHECK_EQ(b.contract().doubled, 2);
    CHECK_EQ(b.cheapest(Clubs), 3);
}

// ------------------------------------------------------------- scoring

TEST(contract_scores)
{
    CHECK_EQ(score("4S", 0, false, 10), 120);
    CHECK_EQ(score("4S", 0, false, 11), 150);
    CHECK_EQ(score("3NT", 0, true, 10), 130);
    CHECK_EQ(score("1NT", 1, false, 7), 80 + 50);
    CHECK_EQ(score("2H", 1, false, 9), 120 + 100 + 50);
    CHECK_EQ(score("2H", 1, true, 9), 120 + 200 + 50);
    CHECK_EQ(score("4S", 1, true, 9), -200);
    CHECK_EQ(score("3NT", 1, false, 6), -500);
    CHECK_EQ(score("3NT", 1, false, 5), -800);
    CHECK_EQ(score("3NT", 2, true, 7), -1000);
    CHECK_EQ(score("4H", 0, true, 7), -300);
    CHECK_EQ(score("6S", 0, false, 12), 180 + 500);
    CHECK_EQ(score("7NT", 2, true, 13), 880 + 100 + 1500);
}

TEST(honours_in_one_hand)
{
    Cards hands[4] = {parseCards("AH KH QH JH 2S"), parseCards("10H"), parseCards("AS AD AC"), parseCards("AH")};
    Honors h = honors(hands, Hearts);
    CHECK_EQ(h.seat, 0);
    CHECK_EQ(h.points, 100);
    hands[0].push_back(parseCard("10H"));
    CHECK_EQ(honors(hands, Hearts).points, 150);
    CHECK_EQ(honors(hands, NoTrump).points, 0);
    Cards aces[4] = {parseCards("AS AH AD AC"), {}, {}, {}};
    CHECK_EQ(honors(aces, NoTrump).points, 150);
}

TEST(rubber_games_and_bonus)
{
    ScoreSheet s(Scoring::Rubber);
    Contract c;
    c.level = 2;
    c.strain = Spades;
    c.declarer = 0;
    CHECK(!s.vulnerable(0, 0));
    s.record(0, c, 8, Honors()); // 60 towards game
    CHECK_EQ(s.partScore(0), 60);
    c.declarer = 1;
    c.level = 3;
    c.strain = NoTrump;
    s.record(1, c, 9, Honors()); // east-west game
    CHECK_EQ(s.games(1), 1);
    CHECK_EQ(s.partScore(0), 0); // the partscore no longer counts towards game
    CHECK(s.vulnerable(1, 0) && !s.vulnerable(0, 0));
    c.declarer = 0;
    c.level = 4;
    c.strain = Hearts;
    s.record(2, c, 10, Honors());
    CHECK_EQ(s.games(0), 1);
    CHECK(!s.finished());
    c.level = 1;
    c.strain = NoTrump;
    s.record(3, c, 6, Honors()); // one down vulnerable: 100 to them
    c.level = 5;
    c.strain = Clubs;
    s.record(0, c, 11, Honors());
    CHECK(s.finished());
    CHECK_EQ(s.deals().back().rubberWon, 0);
    // 60 + 120 + 100 below, a 500 rubber bonus.
    CHECK_EQ(s.total(0), 60 + 120 + 100 + 500);
    CHECK_EQ(s.total(1), 100 + 100);
    CHECK_EQ(s.winner(), 0);
}

TEST(chicago_vulnerability_and_bonuses)
{
    ScoreSheet s(Scoring::Chicago);
    CHECK(!s.vulnerable(0, 0) && !s.vulnerable(1, 0));
    Contract c;
    c.level = 4;
    c.strain = Spades;
    c.declarer = 0;
    s.record(0, c, 10, Honors());
    CHECK_EQ(s.deals().back().total(0), 120 + 300);
    // Deal two, dealt by west: east-west vulnerable.
    CHECK(s.vulnerable(1, 1) && !s.vulnerable(0, 1));
    Contract none;
    s.record(1, none, 0, Honors()); // passed out: dealt again
    CHECK_EQ(s.dealNumber(), 2);
    c.declarer = 1;
    c.level = 2;
    c.strain = Hearts;
    s.record(1, c, 8, Honors());
    CHECK_EQ(s.deals().back().total(1), 60);
    c.level = 3;
    c.strain = Diamonds;
    s.record(2, c, 9, Honors()); // 60 + 60 makes a vulnerable game? no: deal three, north deals
    CHECK_EQ(s.deals().back().gameWon, 1);
    CHECK_EQ(s.deals().back().total(1), 60 + 300);
    CHECK(s.vulnerable(0, 3) && s.vulnerable(1, 3));
    c.declarer = 2;
    c.level = 2;
    c.strain = Clubs;
    s.record(3, c, 8, Honors());
    // A partscore on the fourth deal gets 100.
    CHECK_EQ(s.deals().back().total(0), 40 + 100);
    CHECK(s.finished());
}

// ---------------------------------------------------------------- play

TEST(play_follows_the_auction)
{
    Game g(1);
    g.newGame(stackedDeck({"AS KS QS JS 10S 9S AH KH QH 2C 3C 4C 5C"}), 0);
    CHECK(g.phase() == Phase::Bidding);
    CHECK_EQ(g.turn(), 0);
    CHECK(!g.call(1, parseCall("1S"))); // not west's turn
    for (const Call &c : parseCalls("4S P P P"))
        CHECK(g.call(g.turn(), c));
    CHECK(g.phase() == Phase::Play);
    CHECK_EQ(g.declarer(), 0);
    CHECK_EQ(g.dummy(), 2);
    CHECK_EQ(g.controller(2), 0);
    CHECK_EQ(g.controller(1), 1);
    // West leads, and only then does dummy go down.
    CHECK_EQ(g.turn(), 1);
    CHECK(!g.dummyVisible());
    const Card lead = g.legalCards(1).front();
    CHECK(g.play(1, lead));
    CHECK(g.dummyVisible());
    CHECK_EQ(g.turn(), 2);
    // Following suit when possible.
    for (const Card &c : g.hand(2))
        CHECK_EQ(g.canPlayCard(2, c), c.suit == lead.suit || std::none_of(g.hand(2).begin(), g.hand(2).end(),
                                                                         [&](const Card &x) { return x.suit == lead.suit; }));
}

TEST(trumps_win_the_trick)
{
    Game g(1);
    g.newGame(stackedDeck({"2S AH KH QH JH 10H 9H 8H 7H 6H 5H 4H 3H", "AC KC QC JC 10C 9C 8C 7C 6C AD KD QD JD",
                           "5C 4C 3C 2C 10D 9D 8D 7D 6D 5D 4D 3D 2D"}), 0);
    for (const Call &c : parseCalls("1H P P P"))
        CHECK(g.call(g.turn(), c));
    // West leads a club; east and south are out of clubs, and south ruffs.
    CHECK(g.play(1, parseCard("AC")));
    CHECK(g.play(2, parseCard("2C")));
    CHECK(g.play(3, g.legalCards(3).front()));
    CHECK(!g.canPlayCard(0, parseCard("AC")));
    CHECK(g.canPlayCard(0, parseCard("2S")));
    CHECK(g.play(0, parseCard("3H")));
    CHECK_EQ(g.trickWinner(), 0);
    CHECK(g.collectTrick());
    CHECK_EQ(g.tricks(0), 1);
    CHECK_EQ(g.turn(), 0);
}

TEST(a_whole_deal_is_counted)
{
    Game g(3);
    g.setScoring(Scoring::Chicago);
    g.newGame();
    Bot bot(Difficulty::Medium, 3);
    while (g.phase() == Phase::Bidding)
        CHECK(g.call(g.turn(), bot.chooseCall(g, g.turn())));
    while (g.phase() == Phase::Play) {
        if (g.trickComplete())
            CHECK(g.collectTrick());
        else
            CHECK(g.play(g.turn(), bot.choosePlay(g, g.turn())));
    }
    CHECK(g.phase() == Phase::Score);
    CHECK(g.lastDeal() != nullptr);
    const int dealer = g.dealer();
    CHECK(g.nextDeal());
    CHECK_EQ(g.dealer(), g.lastDeal()->passedOut() ? dealer : (dealer + 1) % 4);
    for (int p = 0; p < 4; ++p)
        CHECK_EQ(g.hand(p).size(), 13u);
}

// ------------------------------------------------------------- bidding

TEST(standard_openings)
{
    const SystemId s = SystemId::Standard;
    CHECK(is(next(s, "KS QS 5S KH QH 3H AD 4D 3D 2D QC 3C 2C", ""), "1NT"));    // 16, balanced
    CHECK(is(next(s, "AS KS 5S 4S 3S KH 2H QD 3D 2D 4C 3C 2C", ""), "1S"));     // 12, five spades
    CHECK(is(next(s, "AS 5S 4S KH 3H 2H QD JD 3D 2D AC 3C 2C", ""), "1D"));     // diamonds the longer minor
    CHECK(is(next(s, "AS 5S 4S KH 3H 2H 4H QD 3D 2D AC 3C 2C", ""), "1C"));     // three and three
    CHECK(is(next(s, "AS KS QS 4S AH KH 3H AD KD 2D AC 3C 2C", ""), "2C"));     // 23
    CHECK(is(next(s, "5S 4S KH QH 10H 9H 3H 2H 4D 3D 2D 3C 2C", ""), "2H"));    // weak two
    CHECK(is(next(s, "5S 4S KH QH 3H 2H 4D 3D 2D 6C 4C 3C 2C", ""), "P"));      // 5
    CHECK(is(next(s, "AS KS 5S KH QH 3H AD KD 3D 2D QC 4C 2C", ""), "2NT"));    // 21
}

TEST(other_systems_open_differently)
{
    const std::string fourteen = "AS QS 5S KH 3H 2H KD 4D 3D 2D QC 3C 2C";  // 3-3-4-3 with 14
    CHECK(is(next(SystemId::Standard, fourteen, ""), "1D"));
    CHECK(is(next(SystemId::Acol, fourteen, ""), "1NT"));
    CHECK(is(next(SystemId::Precision, fourteen, ""), "1NT"));
    const std::string seventeen = "AS KS 5S 4S 3S KH QH 3H AD 3D QC JC 2C";
    CHECK(is(next(SystemId::Standard, seventeen, ""), "1S"));
    CHECK(is(next(SystemId::Precision, seventeen, ""), "1C"));
    // Acol opens four-card suits, the lower of two.
    CHECK(is(next(SystemId::Acol, "AS KS 5S 4S AH QH 3H 2H 4D 3D QC 3C 2C", ""), "1H"));
    // Precision's 2D: short diamonds.
    CHECK(is(next(SystemId::Precision, "AS KS 5S 4S KH QH 3H 2H 4D QC 4C 3C 2C", ""), "2D"));
}

TEST(stayman_and_transfers)
{
    const SystemId s = SystemId::Standard;
    // Responder with a four-card major and game values asks.
    CHECK(is(next(s, "KS 5S AH JH 3H 2H QD 4D 3D 2D QC 3C 2C", "1NT P"), "2C"));
    // Opener shows hearts.
    CHECK(is(next(s, "AS KS 5S KH QH 3H 2H AD 4D 3D QC 3C 2C", "1NT P 2C P"), "2H"));
    // With a fit, responder bids game.
    CHECK(is(next(s, "KS 5S AH JH 3H 2H QD 4D 3D 2D QC 3C 2C", "1NT P 2C P 2H P"), "4H"));
    // Without, notrump.
    CHECK(is(next(s, "KS 5S AH JH 3H 2H QD 4D 3D 2D QC 3C 2C", "1NT P 2C P 2D P"), "3NT"));
    // A weak hand with five spades transfers and passes.
    CHECK(is(next(s, "QS 9S 6S 5S 4S 3H 2H 4D 3D 2D 6C 3C 2C", "1NT P"), "2H"));
    CHECK(is(next(s, "AS KS 3S KH QH 3H 2H AD 4D 3D QC 3C 2C", "1NT P 2H P"), "2S"));
    CHECK(is(next(s, "QS 9S 6S 5S 4S 3H 2H 4D 3D 2D 6C 3C 2C", "1NT P 2H P 2S P"), "P"));
    const Meaning m = meaningOf(s, "1NT P 2H", 2);
    CHECK(m.convention == Convention::Transfer && m.suit == Spades);
    // Acol's notrump is weaker, so responder needs more for game.
    CHECK(is(next(SystemId::Acol, "QS 5S 4S AH 3H 2H 5D 4D 3D 2D QC JC 2C", "1NT P"), "P"));
    CHECK(is(next(SystemId::Standard, "QS 5S 4S AH 3H 2H 5D 4D 3D 2D QC JC 2C", "1NT P"), "2NT"));
}

TEST(raises_and_responses)
{
    const SystemId s = SystemId::Standard;
    CHECK(is(next(s, "QS 5S 4S 3H 2H AD 4D 3D 2D 6C 5C 3C 2C", "1S P"), "2S"));    // 6
    CHECK(is(next(s, "QS 5S 4S 2S 3H AD KD 3D 2D 6C 5C 3C 2C", "1S P"), "3S"));    // a limit raise
    CHECK(is(next(s, "QS JS 4S 2S AH QD 5D 3D 2D AC 5C 3C 2C", "1S P"), "2NT"));   // Jacoby
    CHECK(is(next(SystemId::Acol, "QS JS 4S 2S AH QD 5D 3D 2D AC 5C 3C 2C", "1S P"), "4S"));
    CHECK(is(next(s, "5S 4S KH QH JH 2H 4D 3D 2D 6C 5C 3C 2C", "1D P"), "1H"));    // a new suit
    CHECK(is(next(s, "5S 4S KH 3H 2H 4D 3D 2D QC 6C 5C 3C 2C", "1S P"), "P"));     // 5
    CHECK(is(next(s, "5S KH 3H 2H 4D 3D 2D KC QC 6C 5C 3C 2C", "1S P"), "1NT"));
    CHECK(is(next(s, "5S KH 3H 2H AD 3D 2D KC QC 6C 5C 3C 2C", "1S P"), "2C"));
    CHECK(is(next(SystemId::TwoOverOne, "5S KH 3H 2H AD 3D 2D KC QC 6C 5C 3C 2C", "1S P"), "1NT"));
    // Partner's two-over-one forces to game in that system.
    CHECK(meaningOf(SystemId::TwoOverOne, "1S P 2C", 2).force == Force::Game);
    CHECK(meaningOf(SystemId::Standard, "1S P 2C", 2).force == Force::OneRound);
}

TEST(competition)
{
    const SystemId s = SystemId::Standard;
    // An overcall, a takeout double and a notrump overcall.
    CHECK(is(next(s, "AS KS 10S 5S 4S 3H 2H 4D 3D 2D QC 3C 2C", "1H"), "1S"));
    CHECK(is(next(s, "AS KS 5S 4S 3H AD 4D 3D 2D QC 4C 3C 2C", "1H"), "X"));
    CHECK(is(next(s, "AS KS 5S AH QH 3H AD 4D 3D 2D 5C 3C 2C", "1H"), "1NT"));
    const Meaning x = meaningOf(s, "1H X", 1);
    CHECK(x.convention == Convention::TakeoutDouble);
    // Partner must answer a takeout double, majors first.
    CHECK(is(next(s, "5S 4S 3S 2S 3H 2H 4D 3D 2D 6C 5C 3C 2C", "1H X P"), "1S"));
    // Negative double after an overcall.
    CHECK(is(next(s, "5S 4S KH QH 3H 2H AD 3D 2D 6C 5C 3C 2C", "1C 1S"), "X"));
    CHECK(meaningOf(s, "1C 1S X", 2).convention == Convention::NegativeDouble);
}

TEST(blackwood_to_slam)
{
    const SystemId s = SystemId::Standard;
    // Opener with 20 and a fit for responder's spades.
    Bidding b(s, s);
    Auction a = auction(0, "1H P 1S P");
    const Cards opener = parseCards("AS KS QS 5S AH KH QH 3H 2H 4D 3D AC 2C");
    const Call c = b.choose(a, opener);
    CHECK(c.isBid());
    // After 4NT the answer shows the aces.
    CHECK(is(next(s, "JS 10S 9S 8S 7S 6H KD QD JD 2D QC JC 3C", "1H P 1S P 3S P 4NT P"), "5C"));
    CHECK(is(next(s, "JS 10S 9S 8S 7S 6H AD QD JD 2D QC JC 3C", "1H P 1S P 3S P 4NT P"), "5D"));
    const Meaning m = meaningOf(s, "1H P 1S P 3S P 4NT P 5D", 8);
    CHECK(m.convention == Convention::AceReply);
    // The asker, missing one ace, bids the small slam.
    CHECK(is(next(s, "AS KS QS 5S AH KH 4D 3D KC QC 4C 3C 2C", "1H P 1S P 3S P 4NT P 5D P"), "6S"));
    // Missing two, stops at five.
    CHECK(is(next(s, "AS KS QS 5S AH KH 4D 3D KC QC 4C 3C 2C", "1H P 1S P 3S P 4NT P 5C P"), "5S"));
}

TEST(every_call_is_explained)
{
    // Whatever is bid, the meaning is there and its call matches.
    Game g(11);
    Bot bot(Difficulty::Medium, 11);
    for (int sys = 0; sys < SystemCount; ++sys) {
        g.bidding().setSystem(0, static_cast<SystemId>(sys));
        g.bidding().setSystem(1, static_cast<SystemId>((sys + 1) % SystemCount));
        for (int deal = 0; deal < 40; ++deal) {
            g.newGame();
            int calls = 0;
            while (g.phase() == Phase::Bidding && calls < 60) {
                CHECK(g.call(g.turn(), bot.chooseCall(g, g.turn())));
                ++calls;
            }
            CHECK(g.phase() != Phase::Bidding);
            const Analysis an = g.bidding().analyse(g.auction());
            CHECK_EQ(an.meanings.size(), g.auction().size());
            for (size_t i = 0; i < an.meanings.size(); ++i)
                CHECK(an.meanings[i].call == g.auction().calls()[i]);
        }
    }
}

// -------------------------------------------------------------- solver

// Plain minimax with every card tried, to check the solver against.
int bruteForce(Game &g)
{
    if (g.phase() != Phase::Play)
        return g.tricks(0);
    if (g.trickComplete()) {
        Game next = g;
        next.collectTrick();
        return bruteForce(next);
    }
    const int p = g.turn();
    int best = sideOf(p) == 0 ? -1 : 99;
    for (const Card &c : g.legalCards(p)) {
        Game next = g;
        next.play(p, c);
        const int v = bruteForce(next);
        best = sideOf(p) == 0 ? std::max(best, v) : std::min(best, v);
    }
    return best;
}

TEST(solver_agrees_with_brute_force)
{
    Solver solver;
    Bot bot(Difficulty::Medium, 5);
    for (int seed = 0; seed < 30; ++seed) {
        Game g(100 + seed);
        g.newGame();
        // Some contract, any declarer.
        for (const Call &c : parseCalls(seed % 2 ? "1NT P P P" : "1S P P P"))
            g.call(g.turn(), c);
        // Play down to three tricks, stopping somewhere in a trick.
        while (g.tricksPlayed() < 10 || (int(g.trick().size()) < seed % 4 && !g.trickComplete())) {
            if (g.trickComplete())
                g.collectTrick();
            else
                g.play(g.turn(), bot.choosePlay(g, g.turn()));
        }
        if (g.trickComplete())
            g.collectTrick();
        Cards hands[4];
        for (int p = 0; p < 4; ++p)
            hands[p] = g.hand(p);
        const int solved = solver.solve(hands, g.contract().strain, g.leader(), g.trick());
        CHECK_EQ(g.tricks(0) + solved, bruteForce(g));
    }
}

TEST(solver_finds_the_squeeze)
{
    // A classic simple squeeze at notrump: south leads the last spade and
    // west, guarding both red suits, must let one go.
    Cards hands[4] = {
        parseCards("2S AH 2D"),     // south
        parseCards("KH QH KD"),     // west
        parseCards("JH AD 3D"),     // north
        parseCards("5C 4C 3C"),     // east
    };
    Solver solver;
    CHECK_EQ(solver.solve(hands, NoTrump, 0, {}), 3);
}

TEST(solver_is_quick_enough)
{
    Solver solver;
    Bot bot(Difficulty::Medium, 9);
    Game g(9);
    g.newGame();
    for (const Call &c : parseCalls("1NT P P P"))
        g.call(g.turn(), c);
    while (g.tricksPlayed() < 6) {
        if (g.trickComplete())
            g.collectTrick();
        else
            g.play(g.turn(), bot.choosePlay(g, g.turn()));
    }
    Cards hands[4];
    for (int p = 0; p < 4; ++p)
        hands[p] = g.hand(p);
    const auto start = std::chrono::steady_clock::now();
    solver.solve(hands, NoTrump, g.leader(), g.trick());
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::printf("  seven tricks solved in %.1f ms, %ld nodes\n", ms, solver.nodes());
    CHECK(ms < 2000);
}

// ---------------------------------------------------------------- bots

// Tricks the north-south bots take as declarer at `level`, east-west at
// medium, over the same deals.
double declarerTricks(Difficulty level, int deals, unsigned seed)
{
    Bot ns(level, seed);
    Bot ew(Difficulty::Medium, seed + 1);
    int total = 0;
    int count = 0;
    for (int d = 0; d < deals; ++d) {
        Game g(seed + d);
        g.newGame();
        while (g.phase() == Phase::Bidding)
            g.call(g.turn(), ns.chooseCall(g, g.turn()));
        if (g.phase() != Phase::Play || sideOf(g.declarer()) != 0)
            continue;
        while (g.phase() == Phase::Play) {
            if (g.trickComplete()) {
                g.collectTrick();
                continue;
            }
            Bot &bot = sideOf(g.turn()) == 0 ? ns : ew;
            const Card c = bot.choosePlay(g, g.turn());
            CHECK(g.play(g.turn(), c));
        }
        total += g.tricks(0);
        ++count;
    }
    return count ? double(total) / count : 0;
}

TEST(stronger_levels_take_more_tricks)
{
    const int deals = 60;
    const double easy = declarerTricks(Difficulty::Easy, deals, 500);
    const double medium = declarerTricks(Difficulty::Medium, deals, 500);
    const auto start = std::chrono::steady_clock::now();
    const double hard = declarerTricks(Difficulty::Hard, deals, 500);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("  declarer's tricks: easy %.2f, medium %.2f, hard %.2f (hard took %.1f s)\n", easy, medium, hard, s);
    CHECK(medium >= easy);
    CHECK(hard > medium);
}

} // namespace

int main()
{
    return cards::testing::run();
}
