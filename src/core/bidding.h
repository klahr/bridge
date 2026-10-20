#pragma once

#include "auction.h"
#include "hand.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace bridge {

enum class SystemId { Standard = 0, TwoOverOne = 1, Acol = 2, Precision = 3, ModernStandard = 4 };
constexpr int SystemCount = 5;

struct Range {
    int min = 0;
    int max = 37;

    bool contains(int v) const { return v >= min && v <= max; }
    bool empty() const { return min > max; }
};

// What sets one bidding system apart from another.
struct System {
    SystemId id;
    Range nt1;              // 1NT opening
    Range nt2;              // 2NT opening
    int strongTwo;          // a strong, artificial 2C from this many points; 0 for none
    bool fiveCardMajors;
    bool strongClub;        // 1C is 16 or more, anything else is limited
    bool twoOverOne;        // a new suit at two in response forces to game
    bool jacoby;            // 2NT over a major is a game raise
    bool weakJumpOvercalls;
    bool weakTwoDiamonds;   // 2D is a weak two too
    Range opening;          // one of a suit
    Range rebidNT1;         // opener's 1NT rebid
    Range rebidNT2;         // opener's jump to 2NT; empty for none
    Range respondNT1;       // 1NT response to one of a suit
    Range respondNT2;
    Range respondNT3;
    bool heartsFirst;       // four-card suits: hearts, spades, then the better minor
    bool raiseNT;           // 2NT raises any suit: 13 or more and four trumps
    bool relayTwo;          // 2D is the only answer to 2C
    Range strongTwoNT;      // opener's 2NT after 2C 2D
    Range preempt;          // weak twos and three-level openings
    int preemptAces;        // at most this many in a weak two
    bool weakTwoInvite;     // over a weak two, three of it invites and a new suit forces
    int newSuitTwo;         // a new suit at two in answer, unless it forces to game
};

const System &systemFor(SystemId id);

enum class Convention {
    Natural,
    StrongClub,     // precision 1C
    ClubNegative,   // 1D in answer to it
    StrongTwo,      // 2C
    Waiting,        // 2D in answer to it
    WeakTwo,
    Preempt,
    ShortDiamonds,  // precision 2D: three suits, short in diamonds
    Stayman,
    StaymanReply,
    Transfer,       // shows `suit`
    TransferAccept,
    Jacoby,         // 2NT, a game raise of the major
    RaiseNT,        // 2NT, a raise with four trumps; `suit` is trumps
    Shortness,      // in `suit`, after Jacoby 2NT
    Blackwood,      // asks for aces; `suit` is trumps
    AceReply,       // `suit` holds the number of aces, 0 for none or four
    KingAsk,
    KingReply,
    Quantitative,   // 4NT, inviting 6NT
    TakeoutDouble,
    NegativeDouble,
    PenaltyDouble,
    Redouble,       // ten or more points after a takeout double
};

enum class Force { None, Invitational, OneRound, Game };

// What a call says about the hand: the rules a bot bids by, and what the
// others read from it.
struct Meaning {
    enum Measure { Hcp, Total, Support };

    Meaning() { for (Range &r : len) r = Range{0, 13}; }
    explicit Meaning(const Call &c) : Meaning() { call = c; }

    Call call;
    Convention convention = Convention::Natural;
    int suit = -1;
    Measure measure = Total;
    int trump = -1; // for support points
    Range points{0, 37};
    Range len[4];
    bool balanced = false;
    Range aces{0, 4};
    Range kings{0, 4};
    Force force = Force::None;
    // Made when nothing better fits, so it shows little.
    bool lastResort = false;
    // Rules for the later rounds, after the ones for this auction.
    bool general = false;
    // Anything else the hand needs, but nobody reads from the call.
    std::function<bool(const HandEval &)> test;

    bool matches(const HandEval &e) const;
    bool natural() const { return convention == Convention::Natural; }

    Meaning &hcp(int lo, int hi = 37) { measure = Hcp; points = {std::max(0, lo), hi}; return *this; }
    Meaning &hcp(const Range &r) { return hcp(r.min, r.max); }
    Meaning &tp(int lo, int hi = 37) { measure = Total; points = {std::max(0, lo), hi}; return *this; }
    Meaning &sp(int t, int lo, int hi = 37) { measure = Support; trump = t; points = {std::max(0, lo), hi}; return *this; }
    Meaning &length(int s, int lo, int hi = 13) { len[s] = {lo, hi}; return *this; }
    Meaning &bal() { balanced = true; return *this; }
    Meaning &as(Convention c, int s = -1) { convention = c; suit = s; return *this; }
    Meaning &forcing(Force f) { force = f; return *this; }
    Meaning &asLastResort() { lastResort = true; return *this; }
    Meaning &withAces(int lo, int hi) { aces = {lo, hi}; return *this; }
    Meaning &withKings(int lo, int hi) { kings = {lo, hi}; return *this; }
    Meaning &when(std::function<bool(const HandEval &)> f);
};

// What a player has shown with all their calls so far.
struct Shown {
    Shown() { for (Range &r : len) r = Range{0, 13}; }

    Range points{0, 37};
    Range len[4];
    bool balanced = false;
    Range aces{0, 4};
    Range kings{0, 4};

    void add(const Meaning &m);
};

struct Analysis {
    std::vector<Meaning> meanings; // one for every call
    Shown shown[4];
};

// Each side bids its own system.
class Bidding {
public:
    explicit Bidding(SystemId ns = SystemId::Standard, SystemId ew = SystemId::Standard);

    void setSystem(int side, SystemId id) { m_system[side] = id; }
    SystemId systemId(int side) const { return m_system[side]; }
    const System &system(int side) const { return systemFor(m_system[side]); }

    Analysis analyse(const Auction &auction) const;
    // The calls open to whoever's turn it is, best first; a hand bids the
    // first legal one it matches.
    std::vector<Meaning> candidates(const Auction &auction, const Analysis &analysis) const;
    Call choose(const Auction &auction, const Cards &hand) const;
    Call choose(const Auction &auction, const Analysis &analysis, const Cards &hand) const;
    // What `call` would mean, made next.
    Meaning explain(const Auction &auction, const Analysis &analysis, const Call &call) const;

private:
    SystemId m_system[2];
};

} // namespace bridge
