#include "bidding.h"

#include <algorithm>

namespace bridge {

namespace {

using Rules = std::vector<Meaning>;
using Test = std::function<bool(const HandEval &)>;

const System Systems[SystemCount] = {
    // Standard American: five-card majors, a strong notrump, weak twos.
    {SystemId::Standard, {15, 17}, {20, 21}, 22, true, false, false, true, true, true,
     {11, 21}, {12, 14}, {18, 19}, {6, 10}, {13, 15}, {16, 17},
     false, false, false, {22, 24}, {5, 10}, 4, false, 10},
    // The same, with a two-level response forcing to game and 1NT forcing over a major.
    {SystemId::TwoOverOne, {15, 17}, {20, 21}, 22, true, false, true, true, true, true,
     {11, 21}, {12, 14}, {18, 19}, {6, 10}, {13, 15}, {16, 17},
     false, false, false, {22, 24}, {5, 10}, 4, false, 10},
    // Acol: four-card suits and a weak notrump.
    {SystemId::Acol, {12, 14}, {20, 22}, 23, false, false, false, false, true, true,
     {11, 22}, {15, 17}, {18, 19}, {6, 9}, {10, 12}, {13, 15},
     false, false, false, {23, 25}, {5, 10}, 4, false, 10},
    // Precision: a strong club, everything else limited to 15.
    {SystemId::Precision, {13, 15}, {22, 23}, 0, true, true, false, true, true, false,
     {11, 15}, {11, 12}, {1, 0}, {6, 10}, {11, 12}, {13, 15},
     false, false, false, {22, 24}, {5, 10}, 4, false, 10},
    // Enkel modern standard, as taught in Sweden: four-card suits, a strong
    // notrump, and 2NT raising any suit.
    {SystemId::ModernStandard, {15, 17}, {20, 21}, 22, false, false, false, false, true, true,
     {11, 21}, {12, 14}, {18, 19}, {6, 10}, {11, 12}, {13, 15},
     true, true, true, {22, 23}, {6, 11}, 1, true, 11},
};

bool isMajor(int s) { return s == Hearts || s == Spades; }
int gameLevel(int strain) { return strain == NoTrump ? 3 : isMajor(strain) ? 4 : 5; }

Meaning bid(int level, int strain) { return Meaning(Call(level, strain)); }
Meaning passCall() { return Meaning(Call::pass()); }
Meaning doubleCall() { return Meaning(Call::dbl()); }

// The suit to bid of `suits`, holding at least `minLen`: the longest, the
// higher of two five-card suits and the lower of two fours. -1 for none.
int pickSuit(const HandEval &e, const std::vector<int> &suits, int minLen)
{
    int best = -1;
    for (int s : suits) {
        if (e.len[s] < minLen)
            continue;
        if (best < 0 || e.len[s] > e.len[best])
            best = s;
        else if (e.len[s] == e.len[best] && (e.len[s] >= 5) == (s > best))
            best = s;
    }
    return best;
}

// The four-card opening of Enkel modern standard: the longest suit and the
// higher of two fives, but of fours hearts, spades and the better minor.
int heartsFirst(const HandEval &e)
{
    const int longest = *std::max_element(e.len, e.len + 4);
    if (longest >= 5)
        return pickSuit(e, {Clubs, Diamonds, Hearts, Spades}, 4);
    if (e.len[Hearts] == 4)
        return Hearts;
    if (e.len[Spades] == 4)
        return Spades;
    if (e.len[Clubs] < 4 || e.len[Diamonds] < 4)
        return e.len[Clubs] == 4 ? Clubs : Diamonds;
    int points[4] = {};
    for (const Card &card : e.cards)
        points[card.suit] += hcp(card);
    return points[Diamonds] > points[Clubs] ? Diamonds : Clubs;
}

bool stopped(const HandEval &e, const std::vector<int> &suits)
{
    for (int s : suits)
        if (!e.stopper(s))
            return false;
    return true;
}

// Good enough to show on its own: two of the top three honours, or three
// of the top five.
bool goodSuit(const HandEval &e, int s) { return e.topHonors(s) >= 2 || e.honors(s) >= 3; }

int twoLongest(const HandEval &e)
{
    int l[4];
    std::copy(e.len, e.len + 4, l);
    std::sort(l, l + 4);
    return l[3] + l[2];
}

Range intersect(const Range &a, const Range &b)
{
    Range r{std::max(a.min, b.min), std::min(a.max, b.max)};
    return r.empty() ? b : r;
}

// Everything the rules need to know about the auction so far, from the
// point of view of whoever calls next.
struct Ctx {
    Ctx(const Auction &auction, const Analysis &analysis, const System &system)
        : a(auction)
        , an(analysis)
        , sys(system)
        , n(auction.size())
        , me(auction.turn())
        , partner(partnerOf(me))
        , lho((me + 1) % 4)
        , rho((me + 3) % 4)
    {
        for (size_t i = 0; i < n; ++i) {
            if (a.calls()[i].isBid()) {
                opener = a.seatOf(i);
                openIdx = i;
                break;
            }
        }
        for (size_t i = 0; i < n; ++i) {
            const Meaning &m = an.meanings[i];
            const int seat = a.seatOf(i);
            if (m.call.isBid() && m.natural() && m.call.strain < NoTrump) {
                if (sideOf(seat) == sideOf(me))
                    addSuit(ours, m.call.strain);
                else
                    addSuit(theirs, m.call.strain);
            }
        }
    }

    static void addSuit(std::vector<int> &v, int s)
    {
        if (std::find(v.begin(), v.end(), s) == v.end())
            v.push_back(s);
    }

    const Meaning &meaning(size_t i) const { return an.meanings[i]; }
    const Shown &shown(int seat) const { return an.shown[seat]; }
    int cheapest(int strain) const { return a.cheapest(strain); }
    // The call just made, by the right-hand opponent.
    Call last() const { return n ? a.calls().back() : Call::pass(); }
    bool rhoPassed() const { return n == 0 || a.calls().back().kind == Call::Pass; }

    int lastIdx(int seat) const
    {
        for (int i = static_cast<int>(n) - 1; i >= 0; --i)
            if (a.seatOf(i) == seat)
                return i;
        return -1;
    }
    const Meaning *lastMeaning(int seat) const
    {
        const int i = lastIdx(seat);
        return i < 0 ? nullptr : &an.meanings[i];
    }
    int callsBy(int seat) const
    {
        int count = 0;
        for (size_t i = 0; i < n; ++i)
            count += a.seatOf(i) == seat;
        return count;
    }
    // Anything but passes.
    bool acted(int seat) const
    {
        for (size_t i = 0; i < n; ++i)
            if (a.seatOf(i) == seat && a.calls()[i].kind != Call::Pass)
                return true;
        return false;
    }
    bool bidNaturally(int seat) const
    {
        for (size_t i = 0; i < n; ++i)
            if (a.seatOf(i) == seat && a.calls()[i].isBid() && an.meanings[i].natural())
                return true;
        return false;
    }
    int sideBids(int side) const
    {
        int count = 0;
        for (size_t i = 0; i < n; ++i)
            count += sideOf(a.seatOf(i)) == side && a.calls()[i].isBid();
        return count;
    }
    // The first non-pass call of `seat`, -1 for none.
    int firstAction(int seat) const
    {
        for (size_t i = 0; i < n; ++i)
            if (a.seatOf(i) == seat && a.calls()[i].kind != Call::Pass)
                return static_cast<int>(i);
        return -1;
    }
    bool bidSuit(int seat, int suit) const
    {
        for (size_t i = 0; i < n; ++i)
            if (a.seatOf(i) == seat && a.calls()[i].isBid() && a.calls()[i].strain == suit && an.meanings[i].natural())
                return true;
        return false;
    }
    bool used(Convention c) const
    {
        for (size_t i = 0; i < n; ++i)
            if (sideOf(a.seatOf(i)) == sideOf(me) && an.meanings[i].convention == c)
                return true;
        return false;
    }

    const Auction &a;
    const Analysis &an;
    const System &sys;
    size_t n;
    int me, partner, lho, rho;
    int opener = -1;
    size_t openIdx = 0;
    std::vector<int> ours;   // suits bid naturally by our side
    std::vector<int> theirs; // and by theirs
};

// ------------------------------------------------------------- openings

void openings(const Ctx &c, Rules &r)
{
    const System &s = c.sys;
    const bool fourth = c.n == 3;
    const int lo = s.opening.min;
    const int hi = s.opening.max;
    // Twelve points, or eleven and the rule of twenty; fourth in hand only
    // with the spades to compete (Pearson's rule of fifteen).
    const Test openable = [fourth](const HandEval &e) {
        if (fourth && e.hcp < 15 && e.hcp + e.len[Spades] < 15)
            return false;
        return e.hcp >= 12 || e.hcp + twoLongest(e) >= 20;
    };

    if (s.strongTwo > 0) {
        r.push_back(bid(2, Clubs).hcp(s.strongTwo).as(Convention::StrongTwo).forcing(Force::Game));
        r.push_back(bid(2, Clubs).hcp(16).as(Convention::StrongTwo).forcing(Force::Game).when([](const HandEval &e) {
            return e.playingTricks >= 10;
        }));
    }
    r.push_back(bid(2, NoTrump).hcp(s.nt2).bal());
    if (s.strongClub)
        r.push_back(bid(1, Clubs).hcp(16).as(Convention::StrongClub).forcing(Force::OneRound));
    r.push_back(bid(1, NoTrump).hcp(s.nt1).bal().when(openable));

    if (s.strongClub) {
        r.push_back(bid(2, Clubs).hcp(lo, hi).length(Clubs, 5).when([openable](const HandEval &e) {
            return openable(e) && e.len[Hearts] < 5 && e.len[Spades] < 5
                   && (e.len[Clubs] >= 6 || e.len[Hearts] == 4 || e.len[Spades] == 4);
        }));
        r.push_back(bid(2, Diamonds).hcp(lo, hi).length(Diamonds, 0, 1).length(Hearts, 3, 4).length(Spades, 3, 4)
                        .length(Clubs, 4, 5).as(Convention::ShortDiamonds).when(openable));
    }

    if (s.fiveCardMajors) {
        r.push_back(bid(1, Spades).hcp(lo, hi).length(Spades, 5).when([openable](const HandEval &e) {
            return openable(e) && e.len[Spades] >= e.len[Hearts] && e.len[Spades] >= e.len[Diamonds]
                   && e.len[Spades] >= e.len[Clubs];
        }));
        r.push_back(bid(1, Hearts).hcp(lo, hi).length(Hearts, 5).when([openable](const HandEval &e) {
            return openable(e) && e.len[Hearts] >= e.len[Diamonds] && e.len[Hearts] >= e.len[Clubs];
        }));
        if (s.strongClub) {
            // Precision's 1D holds whatever the other openings do not.
            r.push_back(bid(1, Diamonds).hcp(lo, hi).length(Diamonds, 2).when(openable));
        } else {
            r.push_back(bid(1, Diamonds).hcp(lo, hi).length(Diamonds, 3).when([openable](const HandEval &e) {
                return openable(e) && (e.len[Diamonds] > e.len[Clubs] || (e.len[Diamonds] == e.len[Clubs] && e.len[Diamonds] >= 4));
            }));
            r.push_back(bid(1, Clubs).hcp(lo, hi).length(Clubs, 3).when(openable));
        }
    } else {
        // Four-card suits: the longest, the higher of two five-card suits
        // and the lower of two fours.
        const bool majorsFirst = s.heartsFirst;
        for (int suit = Spades; suit >= Clubs; --suit) {
            r.push_back(bid(1, suit).hcp(lo, hi).length(suit, 4).when([openable, suit, majorsFirst](const HandEval &e) {
                return openable(e) && (majorsFirst ? heartsFirst(e) : pickSuit(e, {Clubs, Diamonds, Hearts, Spades}, 4)) == suit;
            }));
        }
    }

    if (!fourth) {
        for (int suit : {Spades, Hearts})
            r.push_back(bid(4, suit).hcp(s.preempt.min, 11).length(suit, 8).as(Convention::Preempt));
        for (int suit = Spades; suit >= Clubs; --suit)
            r.push_back(bid(3, suit).hcp(s.preempt).length(suit, 7, 7).as(Convention::Preempt).when([suit](const HandEval &e) {
                return goodSuit(e, suit);
            }));
        for (int suit = Spades; suit >= (s.weakTwoDiamonds ? Diamonds : Hearts); --suit) {
            r.push_back(bid(2, suit).hcp(s.preempt).length(suit, 6, 6).withAces(0, s.preemptAces).as(Convention::WeakTwo).when([suit](const HandEval &e) {
                // No four-card major on the side.
                for (int m : {Hearts, Spades})
                    if (m != suit && e.len[m] >= 4)
                        return false;
                return goodSuit(e, suit);
            }));
        }
    }
    r.push_back(passCall().hcp(0, s.strongClub ? 10 : 11));
}

// ------------------------------------------------------------ notrump

// Partner's notrump is the one to answer as an opening: a narrow balanced
// range, and nothing natural said by us before it.
bool answersNotrump(const Ctx &c)
{
    const Meaning *m = c.lastMeaning(c.partner);
    if (!m || !m->call.isBid() || m->call.strain != NoTrump || m->call.level > 2 || !m->balanced || !m->natural())
        return false;
    if (m->points.max - m->points.min > 3 || !c.rhoPassed() || c.bidNaturally(c.me))
        return false;
    // Partner has not bid a suit before it.
    for (size_t i = 0; i < c.n; ++i)
        if (c.a.seatOf(i) == c.partner && c.a.calls()[i].isBid() && c.meaning(i).natural() && c.a.calls()[i].strain < NoTrump)
            return false;
    return true;
}

void notrumpResponses(Rules &r, const Range &nt, int level)
{
    const int base = level + 1;
    const int invite = 25 - nt.max;
    const int game = 25 - nt.min;
    const int quant = 33 - nt.max;
    const int slam = 33 - nt.min;
    const int grand = 37 - nt.min;

    r.push_back(bid(base, Hearts).length(Spades, 5).as(Convention::Transfer, Spades).forcing(Force::OneRound)
                    .when([](const HandEval &e) { return e.len[Spades] >= e.len[Hearts]; }));
    r.push_back(bid(base, Diamonds).length(Hearts, 5).as(Convention::Transfer, Hearts).forcing(Force::OneRound));
    r.push_back(bid(base, Clubs).hcp(std::max(0, invite)).as(Convention::Stayman).forcing(Force::OneRound)
                    .when([](const HandEval &e) {
                        const bool flat = e.len[0] == 3 || e.len[1] == 3 || e.len[2] == 3 || e.len[3] == 3;
                        const bool fourThree = flat && e.balanced && (e.len[0] == 4) + (e.len[1] == 4) + (e.len[2] == 4) + (e.len[3] == 4) == 1;
                        return (e.len[Hearts] == 4 || e.len[Spades] == 4) && !fourThree;
                    }));
    if (level == 1)
        r.push_back(bid(2, NoTrump).hcp(invite, game - 1).forcing(Force::Invitational));
    r.push_back(bid(3, NoTrump).hcp(std::max(0, game), quant - 1));
    r.push_back(bid(4, NoTrump).hcp(quant, slam - 1).bal().as(Convention::Quantitative));
    r.push_back(bid(6, NoTrump).hcp(slam, grand - 1));
    r.push_back(bid(7, NoTrump).hcp(grand));
    r.push_back(passCall().hcp(0, std::max(0, invite - 1)));
}

// The notrump bidder's range, and the level it was at.
bool myNotrump(const Ctx &c, Range &nt, int &level)
{
    for (int i = static_cast<int>(c.n) - 1; i >= 0; --i) {
        if (c.a.seatOf(i) != c.me)
            continue;
        const Meaning &m = c.meaning(i);
        if (m.call.isBid() && m.call.strain == NoTrump && m.natural() && m.balanced) {
            nt = c.shown(c.me).points;
            level = m.call.level;
            return true;
        }
        // Answers to partner's conventions keep the notrump in view.
        if (m.call.isBid() && m.convention != Convention::StaymanReply && m.convention != Convention::TransferAccept)
            return false;
    }
    return false;
}

// Stayman and transfers, answered and followed up.
bool notrumpConventions(const Ctx &c, Rules &r)
{
    const Meaning *partner = c.lastMeaning(c.partner);
    const Meaning *mine = c.lastMeaning(c.me);
    if (!partner || !c.rhoPassed())
        return false;
    Range nt;
    int level = 0;

    // Opener answers.
    if (partner->convention == Convention::Stayman && myNotrump(c, nt, level)) {
        const int l = c.cheapest(Diamonds);
        r.push_back(bid(l, Hearts).length(Hearts, 4).as(Convention::StaymanReply));
        r.push_back(bid(l, Spades).length(Spades, 4).length(Hearts, 0, 3).as(Convention::StaymanReply));
        r.push_back(bid(l, Diamonds).length(Hearts, 0, 3).length(Spades, 0, 3).as(Convention::StaymanReply));
        return true;
    }
    if (partner->convention == Convention::Transfer) {
        r.push_back(bid(c.cheapest(partner->suit), partner->suit).as(Convention::TransferAccept, partner->suit));
        return true;
    }

    // Responder follows up a transfer.
    if (mine && mine->convention == Convention::Transfer && partner->convention == Convention::TransferAccept) {
        const int t = mine->suit;
        nt = c.shown(c.partner).points;
        const int invite = 25 - nt.max;
        const int game = 25 - nt.min;
        if (c.cheapest(NoTrump) == 2)
            r.push_back(bid(2, NoTrump).hcp(invite, game - 1).length(t, 5, 5).forcing(Force::Invitational));
        r.push_back(bid(3, t).hcp(invite, game - 1).length(t, 6).forcing(Force::Invitational));
        r.push_back(bid(3, NoTrump).hcp(game, 32 - nt.min).length(t, 5, 5));
        r.push_back(bid(4, t).hcp(game, 32 - nt.min).length(t, 6));
        r.push_back(passCall().hcp(0, invite - 1));
        return false; // stronger hands go on as usual
    }

    // Opener after responder's follow-up.
    if (!myNotrump(c, nt, level) || c.callsBy(c.me) < 2)
        return false;
    const Meaning &reply = c.meaning(c.lastIdx(c.me));
    const int accept = nt.max - 1; // the top of the range takes up an invitation
    const Call p = partner->call;
    if (reply.convention == Convention::TransferAccept) {
        const int t = reply.suit;
        if (p == Call(2, NoTrump)) {
            r.push_back(bid(4, t).hcp(accept).length(t, 3));
            r.push_back(bid(3, NoTrump).hcp(accept));
            r.push_back(bid(3, t).length(t, 3));
            r.push_back(passCall());
            return true;
        }
        if (p == Call(3, NoTrump)) {
            r.push_back(bid(4, t).length(t, 3));
            r.push_back(passCall());
            return true;
        }
        if (p == Call(3, t)) {
            r.push_back(bid(4, t).hcp(accept));
            r.push_back(passCall());
            return true;
        }
        return false;
    }
    if (reply.convention == Convention::StaymanReply) {
        const int shownMajor = reply.call.strain == Hearts || reply.call.strain == Spades ? reply.call.strain : -1;
        // Responder has the other major if not ours.
        const int other = shownMajor == Hearts ? Spades : -1;
        if (p.isBid() && p.strain == NoTrump && (p.level == 2 || p.level == 3)) {
            if (other >= 0) {
                if (p.level == 3) {
                    r.push_back(bid(4, other).length(other, 4));
                } else {
                    r.push_back(bid(4, other).hcp(accept).length(other, 4));
                    r.push_back(bid(3, other).length(other, 4));
                }
            }
            if (p.level == 2)
                r.push_back(bid(3, NoTrump).hcp(accept));
            r.push_back(passCall());
            return true;
        }
        if (shownMajor >= 0 && p == Call(3, shownMajor)) {
            r.push_back(bid(4, shownMajor).hcp(accept));
            r.push_back(passCall());
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------- slam conventions

bool slamConventions(const Ctx &c, Rules &r)
{
    const Meaning *partner = c.lastMeaning(c.partner);
    const Meaning *mine = c.lastMeaning(c.me);
    if (!partner)
        return false;
    const Shown &p = c.shown(c.partner);

    if (partner->convention == Convention::Blackwood && c.rhoPassed()) {
        r.push_back(bid(5, Clubs).as(Convention::AceReply, 0).when([](const HandEval &e) { return e.aces == 0 || e.aces == 4; }));
        r.push_back(bid(5, Diamonds).withAces(1, 1).as(Convention::AceReply, 1));
        r.push_back(bid(5, Hearts).withAces(2, 2).as(Convention::AceReply, 2));
        r.push_back(bid(5, Spades).withAces(3, 3).as(Convention::AceReply, 3));
        return true;
    }
    if (partner->convention == Convention::KingAsk && c.rhoPassed()) {
        for (int k = 0; k <= 4; ++k)
            r.push_back(bid(6, k).withKings(k, k).as(Convention::KingReply, k));
        return true;
    }
    if (partner->convention == Convention::Quantitative && c.rhoPassed()) {
        r.push_back(bid(6, NoTrump).hcp(c.shown(c.me).points.max - 1));
        r.push_back(passCall());
        return true;
    }
    if (!mine)
        return false;

    if (mine->convention == Convention::Blackwood && partner->convention == Convention::AceReply) {
        const int t = mine->suit;
        const int shown = partner->suit;
        const Call reply = partner->call;
        const int pMin = p.points.min;
        auto total = [shown](const HandEval &e) { return e.aces + (shown == 0 ? (e.aces == 0 ? 4 : 0) : shown); };
        if (c.cheapest(NoTrump) == 5)
            r.push_back(bid(5, NoTrump).sp(t, 36 - pMin).as(Convention::KingAsk, t).when([total](const HandEval &e) { return total(e) == 4; }));
        r.push_back(bid(6, t).when([total](const HandEval &e) { return total(e) >= 3; }));
        if (reply.strain == t)
            r.push_back(passCall());
        else if (c.cheapest(t) == 5)
            r.push_back(bid(5, t));
        else
            r.push_back(bid(6, t));
        return true;
    }
    if (mine->convention == Convention::KingAsk && partner->convention == Convention::KingReply) {
        const int t = mine->suit;
        const int kings = partner->suit;
        r.push_back(bid(7, t).sp(t, 37 - p.points.min).when([kings](const HandEval &e) { return e.kings + kings == 4; }));
        if (c.cheapest(t) == 6)
            r.push_back(bid(6, t));
        else if (partner->call.strain != NoTrump)
            r.push_back(bid(6, NoTrump));
        r.push_back(passCall());
        return true;
    }
    return false;
}

// -------------------------------------------------- opening conventions

bool openingConventions(const Ctx &c, Rules &r)
{
    const Meaning *partner = c.lastMeaning(c.partner);
    const Meaning *mine = c.lastMeaning(c.me);
    if (!partner || !c.rhoPassed())
        return false;
    const bool first = !c.acted(c.me);

    // Precision 1C.
    if (partner->convention == Convention::StrongClub && first) {
        r.push_back(bid(1, Diamonds).hcp(0, 7).as(Convention::ClubNegative));
        for (int s : {Spades, Hearts, Diamonds, Clubs}) {
            r.push_back(bid(isMajor(s) ? 1 : 2, s).hcp(8).length(s, 5).forcing(Force::Game).when([s](const HandEval &e) {
                return pickSuit(e, {Clubs, Diamonds, Hearts, Spades}, 5) == s;
            }));
        }
        r.push_back(bid(1, NoTrump).hcp(8, 13).bal().forcing(Force::Game));
        r.push_back(bid(2, NoTrump).hcp(14).bal().forcing(Force::Game));
        r.push_back(bid(1, NoTrump).hcp(8, 13).forcing(Force::Game));
        r.push_back(bid(2, NoTrump).hcp(14).forcing(Force::Game));
        return true;
    }
    if (mine && mine->convention == Convention::StrongClub && partner->convention == Convention::ClubNegative) {
        r.push_back(bid(1, NoTrump).hcp(16, 18).bal());
        r.push_back(bid(2, NoTrump).hcp(19, 21).bal());
        r.push_back(bid(3, NoTrump).hcp(24).bal());
        for (int s : {Spades, Hearts, Clubs, Diamonds}) {
            const Test pick = [s](const HandEval &e) { return pickSuit(e, {Clubs, Diamonds, Hearts, Spades}, 5) == s; };
            r.push_back(bid(c.cheapest(s) + 1, s).hcp(20).length(s, 5).forcing(Force::Game).when(pick));
            r.push_back(bid(c.cheapest(s), s).hcp(16).length(s, 5).when(pick));
        }
        for (int s : {Hearts, Spades, Clubs})
            r.push_back(bid(c.cheapest(s), s).hcp(16).length(s, 4));
        return true;
    }

    // Strong 2C.
    if (partner->convention == Convention::StrongTwo && first) {
        if (!c.sys.relayTwo) {
            for (int s : {Spades, Hearts, Clubs, Diamonds}) {
                r.push_back(bid(c.cheapest(s), s).hcp(8).length(s, 5).forcing(Force::Game).when([s](const HandEval &e) {
                    return goodSuit(e, s) && pickSuit(e, {Clubs, Diamonds, Hearts, Spades}, 5) == s;
                }));
            }
            r.push_back(bid(2, NoTrump).hcp(8).bal().forcing(Force::Game));
        }
        r.push_back(bid(2, Diamonds).as(Convention::Waiting).forcing(Force::Game));
        return true;
    }
    if (mine && mine->convention == Convention::StrongTwo && partner->convention == Convention::Waiting) {
        const Range nt = c.sys.strongTwoNT;
        r.push_back(bid(2, NoTrump).hcp(nt).bal());
        r.push_back(bid(3, NoTrump).hcp(nt.max + 1).bal());
        for (int s : {Spades, Hearts, Clubs, Diamonds}) {
            r.push_back(bid(c.cheapest(s), s).length(s, 5).forcing(Force::Game).when([s](const HandEval &e) {
                return pickSuit(e, {Clubs, Diamonds, Hearts, Spades}, 5) == s;
            }));
        }
        for (int s : {Hearts, Spades, Clubs, Diamonds})
            r.push_back(bid(c.cheapest(s), s).length(s, 4).forcing(Force::Game));
        r.push_back(bid(2, NoTrump).forcing(Force::Game));
        return true;
    }

    // Jacoby 2NT, answered.
    if (partner->convention == Convention::Jacoby && mine && mine->call.isBid()) {
        const int t = mine->call.strain;
        for (int s = Clubs; s <= Spades; ++s)
            if (s != t)
                r.push_back(bid(3, s).length(s, 0, 1).as(Convention::Shortness, s).forcing(Force::Game));
        r.push_back(bid(4, t).tp(0, 14));
        r.push_back(bid(3, NoTrump).tp(15, 17).forcing(Force::Game));
        r.push_back(bid(3, t).tp(18).forcing(Force::Game));
        return true;
    }

    // The 2NT raise, answered.
    if (partner->convention == Convention::RaiseNT && mine && mine->call.isBid()) {
        const int t = mine->call.strain;
        if (isMajor(t))
            r.push_back(bid(3, t).tp(16).forcing(Force::Game));
        r.push_back(bid(3, NoTrump).hcp(12, 14).bal());
        r.push_back(isMajor(t) ? bid(4, t) : bid(3, t).forcing(Force::Game));
        return true;
    }

    // Precision 2D, answered.
    if (partner->convention == Convention::ShortDiamonds && first) {
        for (int s : {Spades, Hearts})
            r.push_back(bid(4, s).hcp(12).length(s, 4).when([s](const HandEval &e) { return pickSuit(e, {Hearts, Spades}, 4) == s; }));
        r.push_back(bid(3, NoTrump).hcp(13).when([](const HandEval &e) { return e.stopper(Diamonds) && e.len[Diamonds] >= 4; }));
        r.push_back(passCall().hcp(0, 11).length(Diamonds, 6));
        r.push_back(bid(3, Clubs).hcp(0, 11).length(Clubs, 5).length(Hearts, 0, 2).length(Spades, 0, 2));
        r.push_back(bid(2, Spades).hcp(0, 11).when([](const HandEval &e) { return e.len[Spades] >= e.len[Hearts]; }));
        r.push_back(bid(2, Hearts).hcp(0, 11));
        return false;
    }

    // Preempts, raised or not.
    if ((partner->convention == Convention::WeakTwo || partner->convention == Convention::Preempt) && first) {
        const int t = partner->call.strain;
        const int level = partner->call.level;
        std::vector<int> side;
        for (int s = Clubs; s <= Spades; ++s)
            if (s != t)
                side.push_back(s);
        const bool invite = c.sys.weakTwoInvite && level == 2;
        if (isMajor(t) && c.cheapest(t) <= 4) {
            r.push_back(bid(4, t).hcp(level == 2 ? (invite ? 18 : 16) : 14).length(t, 2));
            if (level == 2)
                r.push_back(bid(4, t).length(t, 4));
        }
        if (c.cheapest(NoTrump) <= 3)
            r.push_back(bid(3, NoTrump).hcp(16).when([side, t](const HandEval &e) {
                return stopped(e, side) || (!isMajor(t) && e.len[t] >= 2 && e.hcp >= 18);
            }));
        if (!isMajor(t) && c.cheapest(t) <= 5)
            r.push_back(bid(5, t).hcp(15).length(t, 3));
        if (invite) {
            for (int s : side) {
                if (c.cheapest(s) > 3)
                    continue;
                r.push_back(bid(c.cheapest(s), s).hcp(15).length(s, 5).forcing(Force::OneRound).when([s, side](const HandEval &e) {
                    return goodSuit(e, s) && pickSuit(e, side, 5) == s;
                }));
            }
            r.push_back(bid(3, t).hcp(15, 17).length(t, 2).forcing(Force::Invitational));
        } else if (c.cheapest(t) <= 3) {
            r.push_back(bid(c.cheapest(t), t).hcp(0, 15).length(t, 3));
        }
        r.push_back(passCall());
        return true;
    }
    return false;
}

// ----------------------------------------------------- responses

void respondToSuit(const Ctx &c, Rules &r, int opened)
{
    const System &s = c.sys;
    const Call rho = c.last();
    const bool interfered = rho.isBid();
    const bool doubled = rho.kind == Call::Double;
    const bool passedHand = c.callsBy(c.me) > 0;
    const int theirSuit = interfered && rho.strain < NoTrump ? rho.strain : -1;
    const bool major = isMajor(opened);
    const int need = major ? (s.fiveCardMajors ? 3 : 4) : opened == Clubs ? (s.fiveCardMajors ? 5 : 4) : (s.strongClub ? 5 : 4);
    const std::vector<int> stops = theirSuit >= 0 ? std::vector<int>{theirSuit} : std::vector<int>{};
    const int top = passedHand ? 11 : 37;
    const bool raiseNT = s.raiseNT && !interfered && !doubled && !passedHand;
    const Test noMajor = [opened](const HandEval &e) {
        for (int m : {Hearts, Spades})
            if (m != opened && e.len[m] >= 4)
                return false;
        return true;
    };

    if (doubled)
        r.push_back(Meaning(Call::redbl()).hcp(10).as(Convention::Redouble).when([opened, need](const HandEval &e) {
            return e.len[opened] < need;
        }));

    // Raises.
    const int L = c.cheapest(opened);
    if (major) {
        if (s.jacoby && !interfered && !doubled && !passedHand)
            r.push_back(bid(2, NoTrump).sp(opened, 13).length(opened, 4).as(Convention::Jacoby).forcing(Force::Game));
        if (raiseNT)
            r.push_back(bid(2, NoTrump).sp(opened, 13).length(opened, 4).as(Convention::RaiseNT, opened).forcing(Force::Game));
        if (L == 2)
            r.push_back(bid(3, opened).sp(opened, 10, 12).length(opened, need).forcing(Force::Invitational));
        if (L <= 4) {
            if (!(s.jacoby || s.raiseNT) || interfered || doubled)
                r.push_back(bid(4, opened).sp(opened, 13, passedHand ? 12 : 16).length(opened, 4));
            r.push_back(bid(4, opened).hcp(0, 9).length(opened, 5).when([](const HandEval &e) {
                return *std::min_element(e.len, e.len + 4) <= 1;
            }));
        }
        if (L <= 3)
            r.push_back(bid(L, opened).sp(opened, L == 2 ? 6 : 8, 9).length(opened, L == 2 ? need : need + 1));
    }

    // New suits at the one level.
    std::vector<int> one;
    for (int t = opened + 1; t <= Spades; ++t)
        if (c.cheapest(t) == 1 && t != theirSuit)
            one.push_back(t);
    for (int t : one)
        r.push_back(bid(1, t).tp(6, top).length(t, 4).forcing(Force::OneRound).when([one, t](const HandEval &e) {
            return pickSuit(e, one, 4) == t;
        }));

    // Negative double: the unbid majors.
    if (theirSuit >= 0 && rho.level <= 2) {
        Meaning m = doubleCall().hcp(rho.level == 1 ? 6 : 8).as(Convention::NegativeDouble).forcing(Force::OneRound);
        int majors = 0;
        for (int t : {Hearts, Spades}) {
            if (t != opened && t != theirSuit) {
                m.length(t, 4);
                ++majors;
            }
        }
        if (majors > 0)
            r.push_back(m);
    }

    // Notrump.
    if (c.cheapest(NoTrump) == 1) {
        const bool forcingNT = s.twoOverOne && major && !interfered && !passedHand;
        Meaning m = bid(1, NoTrump).hcp(forcingNT ? Range{6, 12} : interfered ? Range{8, 10} : s.respondNT1)
                        .when([stops](const HandEval &e) { return stopped(e, stops); });
        if (forcingNT)
            m.forcing(Force::OneRound);
        r.push_back(m);
    }

    // New suits at the two level.
    std::vector<int> two;
    for (int t = Clubs; t <= Spades; ++t)
        if (t != opened && t != theirSuit && c.cheapest(t) == 2)
            two.push_back(t);
    const bool gf = s.twoOverOne && !interfered && !passedHand;
    for (int t : two) {
        const int minLen = interfered || (t == Hearts && opened == Spades) ? 5 : 4;
        Meaning m = bid(2, t).tp(gf ? 13 : s.newSuitTwo, top).length(t, minLen)
                        .forcing(gf ? Force::Game : Force::OneRound).when([two, t, minLen](const HandEval &e) {
                            return pickSuit(e, two, minLen) == t;
                        });
        r.push_back(m);
    }

    if (c.cheapest(NoTrump) == 2 && (!major || !s.jacoby || interfered || doubled) && !raiseNT) {
        r.push_back(bid(2, NoTrump).hcp(interfered ? Range{11, 12} : s.respondNT2).bal()
                        .forcing(s.respondNT2.min >= 13 && !interfered ? Force::Game : Force::Invitational)
                        .when([stops, noMajor](const HandEval &e) { return stopped(e, stops) && noMajor(e); }));
    }
    if (interfered && c.cheapest(NoTrump) == 1)
        r.push_back(bid(2, NoTrump).hcp(11, 12).forcing(Force::Invitational).when([stops](const HandEval &e) {
            return stopped(e, stops) && e.balanced;
        }));
    if (c.cheapest(NoTrump) <= 3)
        r.push_back(bid(3, NoTrump).hcp(interfered ? Range{13, 15} : s.respondNT3).bal()
                        .when([stops, noMajor](const HandEval &e) { return stopped(e, stops) && noMajor(e); }));

    // Minor raises, with no major to show.
    if (!major) {
        if (raiseNT)
            r.push_back(bid(2, NoTrump).sp(opened, 13).length(opened, 4).as(Convention::RaiseNT, opened).forcing(Force::Game)
                            .when(noMajor));
        if (L == 2)
            r.push_back(bid(3, opened).sp(opened, 10, 12).length(opened, need).forcing(Force::Invitational).when(noMajor));
        if (L <= 3)
            r.push_back(bid(L, opened).sp(opened, 6, 9).length(opened, need).when(noMajor));
    }

    if (!interfered && !doubled)
        r.push_back(passCall().tp(0, 5));
}

// ---------------------------------------------------- opener's rebid

void openerRebid(const Ctx &c, Rules &r, int opened, const Meaning &response)
{
    const System &s = c.sys;
    const int hi = s.opening.max;
    // Partner's own suit, not a raise of ours.
    const int P = response.call.isBid() && response.natural() && response.call.strain < NoTrump && response.call.strain != opened
                      ? response.call.strain : -1;
    const bool limited = hi <= 15;
    const int mid = limited ? 13 : 15;   // the top of a minimum opening
    const int strong = limited ? 14 : 16;

    // Raise partner's major.
    if (P >= 0 && isMajor(P)) {
        const int L = c.cheapest(P);
        if (L <= 4)
            r.push_back(bid(4, P).sp(P, limited ? 15 : 19).length(P, 4));
        if (L <= 2)
            r.push_back(bid(3, P).sp(P, strong, limited ? 15 : 18).length(P, 4).forcing(Force::Invitational));
        if (L <= 3)
            r.push_back(bid(L, P).sp(P, 11, mid).length(P, 4));
    }

    // A new suit at the one level, up the line.
    std::vector<int> one;
    for (int t = Clubs; t <= Spades; ++t)
        if (t != opened && t != P && c.cheapest(t) == 1)
            one.push_back(t);
    for (int t : one)
        r.push_back(bid(1, t).tp(11, limited ? 15 : 18).length(t, 4).when([one, t](const HandEval &e) {
            for (int u : one)
                if (e.len[u] >= 4)
                    return u == t;
            return false;
        }));

    // Notrump.
    if (c.cheapest(NoTrump) == 1)
        r.push_back(bid(1, NoTrump).hcp(s.rebidNT1).bal());
    if (c.cheapest(NoTrump) == 2) {
        if (response.call.level == 1 && !s.rebidNT2.empty())
            r.push_back(bid(2, NoTrump).hcp(s.rebidNT2).bal().forcing(Force::Invitational));
        else if (response.call.level == 2)
            r.push_back(bid(2, NoTrump).hcp(11, 14).bal());
    }

    if (response.call == Call(1, NoTrump) && response.force == Force::None)
        r.push_back(passCall().hcp(11, 17).when([](const HandEval &e) { return e.balanced; }));

    // Our own suit again, with six.
    const int L = c.cheapest(opened);
    if (L <= 3)
        r.push_back(bid(L + 1, opened).tp(strong, limited ? 15 : 18).length(opened, 6).forcing(Force::Invitational));
    if (L <= 3)
        r.push_back(bid(L, opened).tp(11, mid).length(opened, 6));

    // A second suit: lower ones at minimum strength, higher ones (a reverse) strong.
    for (int t = Spades; t >= Clubs; --t) {
        if (t == opened || t == P || c.cheapest(t) != 2)
            continue;
        if (t < opened)
            r.push_back(bid(2, t).tp(11, limited ? 15 : 18).length(t, 4));
        else if (!limited)
            r.push_back(bid(2, t).tp(17).length(t, 4).length(opened, 5).forcing(Force::OneRound));
    }
    if (!limited) {
        for (int t = Spades; t >= Clubs; --t) {
            if (t == opened || t == P || c.cheapest(t) + 1 > 3)
                continue;
            r.push_back(bid(c.cheapest(t) + 1, t).tp(19).length(t, 4).forcing(Force::Game));
        }
    }

    // Minimum hands with nothing better.
    if (L <= 2)
        r.push_back(bid(L, opened).tp(11, mid).length(opened, 5));
    if (P >= 0 && !isMajor(P) && c.cheapest(P) <= 3)
        r.push_back(bid(c.cheapest(P), P).sp(P, 11).length(P, 4));
}

// ------------------------------------------------------ competition

void overcalls(const Ctx &c, Rules &r)
{
    const Call last = c.a.lastBid();
    const int lastIdx = c.a.lastBidIndex();
    const Meaning &lm = c.meaning(lastIdx);
    // In the pass-out seat we may come in lighter.
    const bool balancing = c.n >= 3 && c.a.calls()[c.n - 1].kind == Call::Pass && c.a.calls()[c.n - 2].kind == Call::Pass
                           && c.a.lastBidder() == c.lho;
    const int light = balancing ? 3 : 0;
    const std::vector<int> theirs = c.theirs;
    const bool natural = lm.natural() || lm.convention == Convention::WeakTwo || lm.convention == Convention::Preempt;
    std::vector<int> unbid;
    for (int s = Clubs; s <= Spades; ++s)
        if (std::find(theirs.begin(), theirs.end(), s) == theirs.end())
            unbid.push_back(s);

    if (c.cheapest(NoTrump) == 1)
        r.push_back(bid(1, NoTrump).hcp(15 - light, 18 - light).bal().when([theirs](const HandEval &e) { return stopped(e, theirs); }));

    // Simple overcalls in a five-card suit.
    for (int s = Spades; s >= Clubs; --s) {
        if (std::find(theirs.begin(), theirs.end(), s) != theirs.end())
            continue;
        const int L = c.cheapest(s);
        if (L > 3)
            continue;
        const int lo = (L == 1 ? 8 : L == 2 ? 11 : 13) - light;
        const int minLen = L == 3 ? 6 : 5;
        r.push_back(bid(L, s).hcp(lo, 16).length(s, minLen).when([s, unbid, minLen](const HandEval &e) {
            return goodSuit(e, s) && pickSuit(e, unbid, minLen) == s;
        }));
    }

    // Takeout double: short in their suits, something in the others.
    if (natural && last.level <= 3 && c.a.doubled() == 0 && last.strain < NoTrump) {
        Meaning m = doubleCall().hcp((last.level == 1 ? 12 : 13) - light).as(Convention::TakeoutDouble).forcing(Force::OneRound);
        for (int s : theirs)
            m.length(s, 0, 2);
        for (int s : unbid)
            m.length(s, isMajor(s) ? 3 : 2);
        r.push_back(m);
    }

    // Weak jump overcalls.
    if (c.sys.weakJumpOvercalls && !balancing) {
        for (int s = Spades; s >= Clubs; --s) {
            if (std::find(theirs.begin(), theirs.end(), s) != theirs.end())
                continue;
            const int L = c.cheapest(s) + 1;
            if (L > 3)
                continue;
            r.push_back(bid(L, s).hcp(6, 10).length(s, 6).as(Convention::Preempt).when([s](const HandEval &e) { return goodSuit(e, s); }));
        }
    }

    // Strong hands double first.
    if (natural && last.level <= 4 && c.a.doubled() == 0)
        r.push_back(doubleCall().hcp(17).as(Convention::TakeoutDouble).forcing(Force::OneRound));
    if (c.cheapest(NoTrump) == 2 && last.level == 2)
        r.push_back(bid(2, NoTrump).hcp(15, 18).bal().when([theirs](const HandEval &e) { return stopped(e, theirs); }));
    if (c.cheapest(NoTrump) == 3)
        r.push_back(bid(3, NoTrump).hcp(16).when([theirs](const HandEval &e) { return stopped(e, theirs); }));
    r.push_back(passCall());
}

void advance(const Ctx &c, Rules &r, const Meaning &partner)
{
    const std::vector<int> theirs = c.theirs;
    const Test stop = [theirs](const HandEval &e) { return stopped(e, theirs); };
    std::vector<int> unbid;
    for (int s = Clubs; s <= Spades; ++s)
        if (std::find(theirs.begin(), theirs.end(), s) == theirs.end())
            unbid.push_back(s);

    if (partner.convention == Convention::TakeoutDouble) {
        // Majors first.
        const std::function<int(const HandEval &)> best = [unbid](const HandEval &e) {
            std::vector<int> majors;
            for (int s : unbid)
                if (isMajor(s))
                    majors.push_back(s);
            const int m = pickSuit(e, majors, 4);
            return m >= 0 ? m : pickSuit(e, unbid, 0);
        };
        const int nt = c.cheapest(NoTrump);
        if (nt <= 3) {
            r.push_back(bid(3, NoTrump).hcp(13).when(stop));
            if (nt <= 2)
                r.push_back(bid(2, NoTrump).hcp(11, 12).when(stop));
            if (nt == 1)
                r.push_back(bid(1, NoTrump).hcp(6, 10).when([stop](const HandEval &e) {
                    return stop(e) && e.len[Hearts] < 4 && e.len[Spades] < 4;
                }));
        }
        for (int s : unbid) {
            const int L = c.cheapest(s);
            const Test pick = [best, s](const HandEval &e) { return best(e) == s; };
            if (isMajor(s) && L <= 4)
                r.push_back(bid(4, s).hcp(12).length(s, 4).when(pick));
            if (L + 1 <= 3)
                r.push_back(bid(L + 1, s).hcp(9, 11).length(s, 4).forcing(Force::Invitational).when(pick));
            // Forced to bid, unless the next hand did.
            if (L <= 3)
                r.push_back(bid(L, s).hcp(c.rhoPassed() ? 0 : 6, 8).when(pick));
        }
        return;
    }

    // Partner overcalled in a suit.
    const int t = partner.call.strain;
    if (t < NoTrump && partner.natural()) {
        const int L = c.cheapest(t);
        if (isMajor(t) && L <= 4)
            r.push_back(bid(4, t).sp(t, 14).length(t, 3));
        if (L + 1 <= 3)
            r.push_back(bid(L + 1, t).sp(t, 11, 13).length(t, 3).forcing(Force::Invitational));
        if (L <= 3)
            r.push_back(bid(L, t).sp(t, 7, 10).length(t, 3));
    }
    const int nt = c.cheapest(NoTrump);
    if (nt == 1)
        r.push_back(bid(1, NoTrump).hcp(8, 11).when(stop));
    if (nt <= 2)
        r.push_back(bid(2, NoTrump).hcp(12, 14).when(stop));
    if (nt <= 3)
        r.push_back(bid(3, NoTrump).hcp(15).when(stop));
    for (int s = Spades; s >= Clubs; --s) {
        if (s == t || std::find(theirs.begin(), theirs.end(), s) != theirs.end() || c.cheapest(s) > 2)
            continue;
        r.push_back(bid(c.cheapest(s), s).hcp(8).length(s, 5).when([s](const HandEval &e) { return goodSuit(e, s); }));
    }
}

// ----------------------------------------------------------- generic

// Later rounds: put what partner has shown with this hand and place the
// contract, or go looking for it.
void generic(const Ctx &c, Rules &r)
{
    const Shown &P = c.shown(c.partner);
    const Meaning *pl = c.lastMeaning(c.partner);
    const bool invited = pl && pl->force == Force::Invitational;
    const int pMin = P.points.min;
    const int pMax = P.points.max;
    const int base = invited ? (pMin + pMax + 1) / 2 : pMin;
    const Call last = c.a.lastBid();
    const int bidder = c.a.lastBidder();
    const bool ours = bidder >= 0 && sideOf(bidder) == sideOf(c.me);
    const bool theirs = bidder >= 0 && !ours;
    const std::vector<int> oppSuits = c.theirs;
    const bool partnerBid = c.bidNaturally(c.partner);

    // How many cards make a fit with partner. A long suit we bid
    // ourselves plays opposite little, unless partner is short.
    int need[4];
    for (int s = 0; s < 4; ++s) {
        need[s] = std::max(partnerBid ? 2 : 8, 8 - P.len[s].min);
        if (partnerBid && P.len[s].max >= 2 && std::find(c.ours.begin(), c.ours.end(), s) != c.ours.end()
            && !c.bidSuit(c.partner, s))
            need[s] = std::min(need[s], 6);
    }
    const auto fit = [need](const HandEval &e, int s) { return e.len[s] >= need[s]; };
    const Test majorFit = [fit](const HandEval &e) { return fit(e, Hearts) || fit(e, Spades); };
    const Test anyFit = [fit](const HandEval &e) { return fit(e, 0) || fit(e, 1) || fit(e, 2) || fit(e, 3); };
    // The suit to play in: the most trumps between us, majors first.
    const auto fitOrder = [&]() {
        std::vector<int> order = {Spades, Hearts, Diamonds, Clubs};
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            if (isMajor(a) != isMajor(b))
                return isMajor(a);
            return P.len[a].min > P.len[b].min;
        });
        return order;
    }();
    // Short suits count when supporting partner, not in a suit we bid first.
    const auto fitPoints = [&c](Meaning m, int s, int lo, int hi = 37) {
        for (size_t i = 0; i < c.n; ++i) {
            const Call x = c.a.calls()[i];
            if (x.isBid() && x.strain == s && c.meaning(i).natural() && sideOf(c.a.seatOf(i)) == sideOf(c.me))
                return c.a.seatOf(i) == c.me ? m.tp(lo, hi) : m.sp(s, lo, hi);
        }
        return m.sp(s, lo, hi);
    };
    const Test notrumpOk = [oppSuits, P](const HandEval &e) {
        return stopped(e, oppSuits) && (e.balanced || P.balanced || *std::min_element(e.len, e.len + 4) >= 2);
    };

    // Doubles for penalty.
    const int theirLevel = last.level;
    const bool canDouble = theirs && c.a.doubled() == 0;
    const bool ourGame = c.sideBids(sideOf(c.me)) > 0 && pMin >= 10;
    if (canDouble && theirLevel >= 4 && partnerBid)
        r.push_back(doubleCall().hcp(std::max(10, 23 - pMin)).as(Convention::PenaltyDouble).when([ourGame, last](const HandEval &e) {
            return ourGame && (last.strain == NoTrump || e.len[last.strain] <= 3 || e.honors(last.strain) >= 2);
        }));

    // Slams.
    if (partnerBid && pMin >= 6) {
        // Trumps: a suit we have both bid, or partner's last.
        int agreed = -1;
        for (int i = static_cast<int>(c.n) - 1; i >= 0 && agreed < 0; --i) {
            const Call x = c.a.calls()[i];
            if (sideOf(c.a.seatOf(i)) == sideOf(c.me) && x.isBid() && x.strain < NoTrump && c.meaning(i).natural()
                && c.bidSuit(c.me, x.strain) && c.bidSuit(c.partner, x.strain))
                agreed = x.strain;
        }
        if (agreed < 0 && pl && pl->call.isBid() && pl->natural() && pl->call.strain < NoTrump)
            agreed = pl->call.strain;
        if (agreed >= 0 && !c.used(Convention::Blackwood) && c.cheapest(NoTrump) <= 4)
            r.push_back(fitPoints(bid(4, NoTrump), agreed, 33 - pMin).length(agreed, need[agreed]).as(Convention::Blackwood, agreed)
                            .forcing(Force::OneRound));
        if (pl && pl->call.isBid() && pl->call.strain == NoTrump && pl->natural() && c.cheapest(NoTrump) == 4)
            r.push_back(bid(4, NoTrump).hcp(33 - pMax, 32 - pMin).as(Convention::Quantitative).when([anyFit](const HandEval &e) {
                return !anyFit(e);
            }));
        if (c.cheapest(NoTrump) <= 7)
            r.push_back(bid(7, NoTrump).hcp(37 - base).when([anyFit](const HandEval &e) { return !anyFit(e) && e.balanced; }));
        if (c.cheapest(NoTrump) <= 6)
            r.push_back(bid(6, NoTrump).hcp(33 - base).when([anyFit](const HandEval &e) { return !anyFit(e) && e.balanced; }));
    }

    // Games, unless we are in one already.
    const bool inGame = ours && last.level >= gameLevel(last.strain);
    for (int s : {Spades, Hearts}) {
        if (c.cheapest(s) <= 4 && !inGame)
            r.push_back(fitPoints(bid(4, s), s, 25 - base).length(s, need[s]).when([s, fit](const HandEval &e) {
                for (int t : {Spades, Hearts})
                    if (fit(e, t))
                        return t == s || e.len[s] > e.len[t];
                return false;
            }));
    }
    if (c.cheapest(NoTrump) <= 3 && !inGame)
        r.push_back(bid(3, NoTrump).hcp(25 - base).when([majorFit, notrumpOk](const HandEval &e) {
            return !majorFit(e) && notrumpOk(e);
        }));
    for (int s : {Diamonds, Clubs})
        if (c.cheapest(s) <= 5 && !inGame)
            r.push_back(fitPoints(bid(5, s), s, 28 - base).length(s, need[s]).when([majorFit](const HandEval &e) { return !majorFit(e); }));

    // Invitations, close to game.
    const int inviteFrom = std::max(25 - pMax, 23 - pMin);
    if (!invited && partnerBid && !inGame) {
        for (int s : {Spades, Hearts})
            if (c.cheapest(s) <= 3)
                r.push_back(fitPoints(bid(3, s), s, inviteFrom, 24 - pMin).length(s, need[s]).forcing(Force::Invitational));
        if (c.cheapest(NoTrump) <= 2)
            r.push_back(bid(2, NoTrump).hcp(inviteFrom, 24 - pMin).forcing(Force::Invitational).when([majorFit, notrumpOk](const HandEval &e) {
                return !majorFit(e) && notrumpOk(e);
            }));
    }

    // Penalty doubles with their trumps in our hand.
    if (canDouble && theirLevel >= 2 && last.strain < NoTrump && pMin >= 10)
        r.push_back(doubleCall().hcp(10).length(last.strain, 4).as(Convention::PenaltyDouble).when([last](const HandEval &e) {
            return e.honors(last.strain) >= 2;
        }));

    // Look for a fit while game is still possible.
    if (partnerBid && c.sideBids(sideOf(c.me)) < 5) {
        std::vector<int> fresh;
        for (int s = Clubs; s <= Spades; ++s)
            if (std::find(c.ours.begin(), c.ours.end(), s) == c.ours.end()
                && std::find(oppSuits.begin(), oppSuits.end(), s) == oppSuits.end() && c.cheapest(s) <= 3)
                fresh.push_back(s);
        for (int s : fresh)
            r.push_back(bid(c.cheapest(s), s).tp(std::max(25 - pMax, 22 - pMin) + (c.cheapest(s) >= 3 ? 3 : 0)).length(s, 4).forcing(Force::OneRound).when([anyFit, fresh, s](const HandEval &e) {
                return !anyFit(e) && pickSuit(e, fresh, 4) == s;
            }));
    }

    // Partscores: our fit, as long as it is safe.
    for (int s : fitOrder) {
        const int L = c.cheapest(s);
        if (L > 4 || (ours && last.strain == s) || inGame)
            continue;
        const int trumps = L + 6;
        const int needed = L <= 2 ? 17 : L == 3 ? 20 : 23;
        r.push_back(fitPoints(bid(L, s), s, needed - pMin).length(s, std::max(need[s], trumps - P.len[s].min)).when([s, fitOrder, fit](const HandEval &e) {
            for (int t : fitOrder)
                if (fit(e, t))
                    return t == s;
            return false;
        }));
    }

    // A long suit of our own.
    for (int s = Spades; s >= Clubs; --s) {
        const int L = c.cheapest(s);
        if (L > 3 || (ours && last.strain == s) || std::find(oppSuits.begin(), oppSuits.end(), s) != oppSuits.end())
            continue;
        r.push_back(bid(L, s).tp((L <= 2 ? 17 : 21) - pMin).length(s, L <= 2 ? 6 : 7).when([anyFit, s](const HandEval &e) {
            return !anyFit(e) && e.longest() == s;
        }));
    }

    // Partner has passed: a strong hand still competes.
    if (!c.acted(c.partner) && c.opener == c.me && theirs && last.level <= 2) {
        for (int s = Spades; s >= Clubs; --s)
            if (c.bidSuit(c.me, s) && c.cheapest(s) <= 2)
                r.push_back(bid(c.cheapest(s), s).tp(15).length(s, 6));
        if (c.cheapest(NoTrump) <= 2)
            r.push_back(bid(c.cheapest(NoTrump), NoTrump).hcp(18, 19).bal().when([oppSuits](const HandEval &e) {
                return stopped(e, oppSuits);
            }));
        if (canDouble && last.strain < NoTrump)
            r.push_back(doubleCall().hcp(16).length(last.strain, 0, 2).as(Convention::TakeoutDouble));
    }

    // Must we bid?
    bool forced = pl && (pl->force == Force::OneRound || pl->force == Force::Game) && c.rhoPassed() && pl->call.kind != Call::Pass
                  && c.lastIdx(c.partner) == static_cast<int>(c.n) - 2;
    bool gameForced = false;
    for (size_t i = 0; i < c.n; ++i)
        if (sideOf(c.a.seatOf(i)) == sideOf(c.me) && c.meaning(i).force == Force::Game)
            gameForced = true;
    gameForced = gameForced && ours && last.level < gameLevel(last.strain) && c.a.doubled() == 0;
    forced = forced || gameForced;

    if (!forced) {
        r.push_back(passCall());
        return;
    }

    if (gameForced) {
        for (int s : {Spades, Hearts})
            if (c.cheapest(s) <= 4)
                r.push_back(bid(4, s).length(s, need[s]).asLastResort());
        if (c.cheapest(NoTrump) <= 3)
            r.push_back(bid(3, NoTrump).when([majorFit](const HandEval &e) { return !majorFit(e); }).asLastResort());
        for (int s : {Diamonds, Clubs})
            if (c.cheapest(s) <= 5)
                r.push_back(bid(5, s).length(s, need[s]).asLastResort());
    }
    // Back to partner's suit, our own again, or notrump.
    if (pl->call.isBid() && pl->natural() && pl->call.strain < NoTrump && c.cheapest(pl->call.strain) <= 4)
        r.push_back(bid(c.cheapest(pl->call.strain), pl->call.strain).length(pl->call.strain, 3).asLastResort());
    for (int s = Spades; s >= Clubs; --s)
        if (c.cheapest(s) <= 4 && std::find(c.ours.begin(), c.ours.end(), s) != c.ours.end())
            r.push_back(bid(c.cheapest(s), s).length(s, 5).asLastResort());
    if (c.cheapest(NoTrump) <= 3)
        r.push_back(bid(c.cheapest(NoTrump), NoTrump).when(notrumpOk).asLastResort());
    for (int s = Clubs; s <= Spades; ++s)
        if (c.cheapest(s) <= 4)
            r.push_back(bid(c.cheapest(s), s).length(s, 4).asLastResort());
    if (c.cheapest(NoTrump) <= 7)
        r.push_back(bid(c.cheapest(NoTrump), NoTrump).asLastResort());
}

// The meaning of a call that no rule makes: natural, by level.
Meaning fallback(const Ctx &c, const Call &call)
{
    Meaning m(call);
    switch (call.kind) {
    case Call::Pass:
        return m;
    case Call::Double: {
        const Call last = c.a.lastBid();
        if (!c.acted(c.partner) && last.level <= 2 && last.strain < NoTrump)
            m.as(Convention::TakeoutDouble).hcp(11);
        else
            m.as(Convention::PenaltyDouble);
        return m;
    }
    case Call::Redouble:
        return m.as(Convention::Redouble);
    case Call::Bid:
        break;
    }
    static const int levelPoints[] = {0, 6, 8, 10, 12, 14, 16, 18};
    m.tp(levelPoints[call.level]);
    if (call.strain < NoTrump)
        m.length(call.strain, call.level >= 3 ? 6 : 4);
    return m;
}

} // namespace

bool Meaning::matches(const HandEval &e) const
{
    const int p = measure == Hcp ? e.hcp : measure == Support ? e.support(trump) : e.total;
    if (!points.contains(p))
        return false;
    for (int s = 0; s < 4; ++s)
        if (!len[s].contains(e.len[s]))
            return false;
    if (balanced && !e.balanced)
        return false;
    if (!aces.contains(e.aces) || !kings.contains(e.kings))
        return false;
    return !test || test(e);
}

Meaning &Meaning::when(std::function<bool(const HandEval &)> f)
{
    if (test) {
        auto before = test;
        test = [before, f](const HandEval &e) { return before(e) && f(e); };
    } else {
        test = std::move(f);
    }
    return *this;
}

void Shown::add(const Meaning &m)
{
    points = intersect(points, m.points);
    for (int s = 0; s < 4; ++s)
        len[s] = intersect(len[s], m.len[s]);
    balanced = balanced || m.balanced;
    aces = intersect(aces, m.aces);
    kings = intersect(kings, m.kings);
}

const System &systemFor(SystemId id)
{
    return Systems[static_cast<int>(id)];
}

Bidding::Bidding(SystemId ns, SystemId ew)
    : m_system{ns, ew}
{
}

Analysis Bidding::analyse(const Auction &auction) const
{
    Analysis an;
    for (size_t i = 0; i < auction.size(); ++i) {
        const Auction before = auction.prefix(i);
        const Meaning m = explain(before, an, auction.calls()[i]);
        an.meanings.push_back(m);
        // A pass says little but in a few places.
        an.shown[auction.seatOf(i)].add(m);
    }
    return an;
}

std::vector<Meaning> Bidding::candidates(const Auction &auction, const Analysis &analysis) const
{
    const Ctx c(auction, analysis, system(sideOf(auction.turn())));
    Rules r;
    if (c.opener < 0) {
        openings(c, r);
        return r;
    }
    if (slamConventions(c, r))
        return r;
    if (openingConventions(c, r))
        return r;
    if (answersNotrump(c)) {
        const Meaning *m = c.lastMeaning(c.partner);
        notrumpResponses(r, c.shown(c.partner).points, m->call.level);
        return r;
    }
    if (notrumpConventions(c, r))
        return r;

    const bool ourOpening = sideOf(c.opener) == sideOf(c.me);
    const Meaning &opening = c.meaning(c.openIdx);
    if (ourOpening) {
        const bool suitOpening = opening.natural() && opening.call.level == 1 && opening.call.strain < NoTrump;
        if (c.opener == c.partner && !c.acted(c.me) && suitOpening) {
            respondToSuit(c, r, opening.call.strain);
        } else if (c.opener == c.me && c.lastIdx(c.me) == static_cast<int>(c.openIdx) && suitOpening) {
            const int response = c.firstAction(c.partner);
            if (response >= 0 && response == c.lastIdx(c.partner) && c.meaning(response).call.isBid())
                openerRebid(c, r, opening.call.strain, c.meaning(response));
        }
    } else {
        if (!c.acted(c.me) && !c.acted(c.partner)) {
            overcalls(c, r);
            return r;
        }
        if (!c.acted(c.me) && c.acted(c.partner)) {
            const int first = c.firstAction(c.partner);
            const Meaning &m = c.meaning(first);
            if (m.call == Call(1, NoTrump) && m.natural() && c.rhoPassed() && c.lastIdx(c.partner) == first) {
                notrumpResponses(r, m.points, 1);
                return r;
            }
            if (c.lastIdx(c.partner) == first)
                advance(c, r, m);
        }
    }
    const size_t general = r.size();
    generic(c, r);
    for (size_t i = general; i < r.size(); ++i)
        r[i].general = true;
    return r;
}

Meaning Bidding::explain(const Auction &auction, const Analysis &analysis, const Call &call) const
{
    // Several rules may make the same call; it shows what they have in
    // common. A pass is read by the first, before anything that passes
    // whatever the hand.
    const Rules all = candidates(auction, analysis);
    bool considered = false;
    for (const Meaning &m : all)
        considered = considered || (m.call == call && !m.lastResort);
    const Meaning *first = nullptr;
    Meaning out;
    for (const Meaning &m : all) {
        if (m.call != call || (considered && m.lastResort))
            continue;
        if (!first) {
            first = &m;
            out = m;
            out.test = nullptr;
            if (call.kind == Call::Pass)
                return out;
            continue;
        }
        if (m.convention != first->convention || m.general != first->general)
            continue;
        out.points = {std::min(out.points.min, m.points.min), std::max(out.points.max, m.points.max)};
        for (int s = 0; s < 4; ++s)
            out.len[s] = {std::min(out.len[s].min, m.len[s].min), std::max(out.len[s].max, m.len[s].max)};
        out.balanced = out.balanced && m.balanced;
        out.aces = {std::min(out.aces.min, m.aces.min), std::max(out.aces.max, m.aces.max)};
        out.kings = {std::min(out.kings.min, m.kings.min), std::max(out.kings.max, m.kings.max)};
    }
    if (first)
        return out;
    const Ctx c(auction, analysis, system(sideOf(auction.turn())));
    return fallback(c, call);
}

Call Bidding::choose(const Auction &auction, const Cards &hand) const
{
    return choose(auction, analyse(auction), hand);
}

Call Bidding::choose(const Auction &auction, const Analysis &analysis, const Cards &hand) const
{
    const HandEval e(hand);
    for (const Meaning &m : candidates(auction, analysis))
        if (auction.isLegal(m.call) && m.matches(e))
            return m.call;
    return Call::pass();
}

} // namespace bridge
