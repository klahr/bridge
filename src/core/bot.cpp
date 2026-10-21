#include "bot.h"

#include <algorithm>

namespace bridge {

namespace {

// Guessed deals per card: more where they are only played out by rules of
// thumb, fewer where they are solved exactly.
const int PlayoutSamples = 40;
const int SolveSamples = 20;
// Positions with this many cards left are solved exactly.
const int SolveCards = 24;

Card lowest(const Cards &cards)
{
    return *std::min_element(cards.begin(), cards.end(), [](const Card &a, const Card &b) { return a.high() < b.high(); });
}

Card highest(const Cards &cards)
{
    return *std::max_element(cards.begin(), cards.end(), [](const Card &a, const Card &b) { return a.high() < b.high(); });
}

Cards ofSuit(const Cards &cards, int suit)
{
    Cards out;
    for (const Card &c : cards)
        if (c.suit == suit)
            out.push_back(c);
    return out;
}

// What the player choosing a card knows.
struct View {
    View(const Game &game, int p)
        : g(game)
        , player(p)
        , viewer(game.controller(p))
        , trump(game.contract().strain)
        , declaring(sideOf(p) == sideOf(game.declarer()))
    {
        for (const Play &x : g.played())
            played[x.card.id()] = true;
        const std::vector<Play> &all = g.played();
        for (size_t i = 0; i < all.size(); ++i) {
            const int led = all[i - i % 4].card.suit;
            if (all[i].card.suit != led)
                shownOut[all[i].player][led] = true;
        }
        const int partner = partnerOf(p);
        if (declaring && g.dummyVisible())
            partnerHand = &g.hand(partner);
        if (g.dummyVisible() && !declaring)
            dummyHand = &g.hand(g.dummy());
    }

    bool mine(const Card &c) const { return contains(g.hand(player), c); }
    bool ours(const Card &c) const { return mine(c) || (partnerHand && contains(*partnerHand, c)); }
    // No card of the suit above it is left with the opponents, as far as we know.
    bool master(const Card &c) const
    {
        for (int r = c.high() + 1; r <= 14; ++r) {
            const Card higher(r == 14 ? 1 : r, c.suit);
            if (!played[higher.id()] && !ours(higher))
                return false;
        }
        return true;
    }
    // Trumps the opponents may still hold.
    int theirTrumps() const
    {
        if (trump == NoTrump)
            return 0;
        int n = 0;
        for (int r = 1; r <= 13; ++r) {
            const Card c(r, trump);
            if (!played[c.id()] && !ours(c))
                ++n;
        }
        // Defenders that showed out of trumps have none.
        if (declaring && shownOut[(g.declarer() + 1) % 4][trump] && shownOut[(g.declarer() + 3) % 4][trump])
            return 0;
        return n;
    }
    // A defender is known to be out of the suit.
    bool opponentVoid(int suit) const
    {
        return shownOut[(player + 1) % 4][suit] || shownOut[(player + 3) % 4][suit];
    }

    const Game &g;
    int player;
    int viewer;
    int trump;
    bool declaring;
    bool played[52] = {};
    bool shownOut[4][4] = {};
    const Cards *partnerHand = nullptr; // the declarer's side sees both hands
    const Cards *dummyHand = nullptr;   // the defenders see dummy
};

bool beats(const Card &c, const Card &w, int trump)
{
    if (c.suit == w.suit)
        return c.high() > w.high();
    return c.suit == trump;
}

// The lowest card that wins as surely as `c`: everything between them is gone.
Card lowestEquivalent(const View &v, const Cards &hand, const Card &c)
{
    Card best = c;
    for (int r = c.high() - 1; r >= 2; --r) {
        const Card lower(r, c.suit);
        if (contains(hand, lower))
            best = lower;
        else if (!v.played[lower.id()])
            break;
    }
    return best;
}

Card discard(const View &v, const Cards &legal)
{
    const Cards &hand = v.g.hand(v.player);
    const Card *best = nullptr;
    int bestValue = 0;
    for (const Card &c : legal) {
        int value = c.high();
        if (v.master(c))
            value += 20;
        if (c.suit == v.trump)
            value += 30;
        // Keep the guard of an honour.
        const int n = countSuit(hand, c.suit);
        for (int r : {13, 12})
            if (c.high() < r && contains(hand, Card(r, c.suit)) && n <= r - 10)
                value += 8;
        if (!best || value < bestValue) {
            best = &c;
            bestValue = value;
        }
    }
    return *best;
}

// The suits partner has bid.
std::vector<int> partnerSuits(const Game &g, int player)
{
    std::vector<int> out;
    const Analysis an = g.bidding().analyse(g.auction());
    for (size_t i = 0; i < an.meanings.size(); ++i) {
        const Meaning &m = an.meanings[i];
        if (g.auction().seatOf(i) == partnerOf(player) && m.call.isBid() && m.natural() && m.call.strain < NoTrump
            && std::find(out.begin(), out.end(), m.call.strain) == out.end())
            out.push_back(m.call.strain);
    }
    return out;
}

bool touching(const Cards &suit)
{
    // Sorted from the top: two honours in a row, the jack at least.
    return suit.size() >= 2 && suit[0].high() >= 11 && suit[1].high() == suit[0].high() - 1;
}

Card openingLead(const View &v, const Cards &legal)
{
    const Game &g = v.g;
    const Cards &hand = g.hand(v.player);
    for (int s : partnerSuits(g, v.player)) {
        Cards suit = ofSuit(legal, s);
        if (suit.empty())
            continue;
        if (suit.size() <= 2 || touching(suit))
            return highest(suit);
        const bool honour = contains(suit, Card(1, s)) || contains(suit, Card(13, s)) || contains(suit, Card(12, s));
        return honour ? lowest(suit) : highest(suit);
    }

    int best = -1;
    int bestScore = -1000;
    for (int s = 0; s < 4; ++s) {
        Cards suit = ofSuit(legal, s);
        if (suit.empty())
            continue;
        std::sort(suit.begin(), suit.end(), [](const Card &a, const Card &b) { return a.high() > b.high(); });
        int score;
        if (v.trump == NoTrump) {
            score = static_cast<int>(suit.size()) * 10 + hcp(suit) + (touching(suit) ? 4 : 0);
        } else {
            score = 0;
            if (s == v.trump)
                score -= 6;
            if (touching(suit))
                score += suit[0].high() >= 13 ? 8 : 6;
            if (suit.size() == 1 && s != v.trump && countSuit(hand, v.trump) > 0 && suit[0].high() < 11)
                score += 4;
            if (suit.size() == 2 && s != v.trump)
                score += 1;
            const bool ace = contains(suit, Card(1, s));
            const bool king = contains(suit, Card(13, s));
            const bool queen = contains(suit, Card(12, s));
            if (ace && !king)
                score -= 5;
            if (king && !queen && !ace)
                score -= 3;
            if (queen && !king && !contains(suit, Card(11, s)))
                score -= 1;
        }
        if (score > bestScore) {
            bestScore = score;
            best = s;
        }
    }
    Cards suit = ofSuit(legal, best);
    std::sort(suit.begin(), suit.end(), [](const Card &a, const Card &b) { return a.high() > b.high(); });
    if (touching(suit) || suit.size() <= 2)
        return suit[0];
    if (v.trump != NoTrump && contains(suit, Card(1, best)))
        return suit[0];
    // Fourth from the top.
    return suit[std::min<size_t>(3, suit.size() - 1)];
}

Card lead(const View &v, const Cards &legal, bool careful)
{
    const Game &g = v.g;
    const Cards &mine = g.hand(v.player);
    Cards sorted = legal;
    std::sort(sorted.begin(), sorted.end(), [](const Card &a, const Card &b) { return a.high() > b.high(); });

    if (v.declaring && v.partnerHand) {
        const Cards &other = *v.partnerHand;
        if (v.trump != NoTrump && v.theirTrumps() > 0) {
            // Draw trumps.
            for (const Card &c : sorted)
                if (c.suit == v.trump && v.master(c))
                    return c;
            const int ourTrumps = countSuit(mine, v.trump) + countSuit(other, v.trump);
            if (countSuit(mine, v.trump) > 0 && (ourTrumps >= v.theirTrumps() + 4 || !careful))
                for (const Card &c : other)
                    if (c.suit == v.trump && v.master(c))
                        return lowest(ofSuit(legal, v.trump));
        }
        // Cash winners.
        for (const Card &c : sorted)
            if (c.suit != v.trump && v.master(c) && (v.trump == NoTrump || v.theirTrumps() == 0 || !v.opponentVoid(c.suit)))
                return c;
        if (careful && v.trump != NoTrump && countSuit(other, v.trump) > 0) {
            // Ruff in the other hand.
            for (int s = 0; s < 4; ++s)
                if (s != v.trump && countSuit(mine, s) > 0 && countSuit(other, s) == 0)
                    return lowest(ofSuit(legal, s));
        }
        // Lead towards winners in the other hand.
        for (const Card &c : other)
            if (c.suit != v.trump && v.master(c) && countSuit(mine, c.suit) > 0)
                return lowest(ofSuit(legal, c.suit));
        // Low from the longest suit between us.
        int best = -1;
        for (int s = 0; s < 4; ++s) {
            if (countSuit(mine, s) == 0)
                continue;
            const int n = countSuit(mine, s) + countSuit(other, s) - (s == v.trump ? 20 : 0);
            if (best < 0 || n > countSuit(mine, best) + countSuit(other, best) - (best == v.trump ? 20 : 0))
                best = s;
        }
        return lowest(ofSuit(legal, best));
    }

    if (g.played().empty())
        return openingLead(v, legal);

    // Defending: cash winners, unless dummy ruffs them.
    const Cards *dummy = v.dummyHand;
    for (const Card &c : sorted) {
        if (c.suit == v.trump || !v.master(c))
            continue;
        if (v.trump != NoTrump && dummy && countSuit(*dummy, c.suit) == 0 && countSuit(*dummy, v.trump) > 0)
            continue;
        return c;
    }
    // Back to partner's suit.
    const std::vector<Play> &all = g.played();
    for (size_t i = 0; i < all.size(); i += 4) {
        if (all[i].player != partnerOf(v.player))
            continue;
        const Cards suit = ofSuit(legal, all[i].card.suit);
        if (!suit.empty())
            return suit.size() <= 2 ? highest(suit) : lowest(suit);
    }
    // Where dummy is weak, or our own long suit at notrump.
    int best = -1;
    int bestScore = -1000;
    for (int s = 0; s < 4; ++s) {
        const Cards suit = ofSuit(legal, s);
        if (suit.empty())
            continue;
        int score = static_cast<int>(suit.size()) * (v.trump == NoTrump ? 3 : 1);
        if (s == v.trump)
            score -= 4;
        if (dummy) {
            score -= 2 * hcp(ofSuit(*dummy, s));
            if (v.trump != NoTrump && countSuit(*dummy, s) == 0 && countSuit(*dummy, v.trump) > 0)
                score -= 20;
        }
        if (score > bestScore) {
            bestScore = score;
            best = s;
        }
    }
    Cards suit = ofSuit(legal, best);
    std::sort(suit.begin(), suit.end(), [](const Card &a, const Card &b) { return a.high() > b.high(); });
    return touching(suit) ? suit[0] : lowest(suit);
}

Card follow(const View &v, const Cards &legal, bool careful)
{
    const Game &g = v.g;
    const std::vector<Play> &trick = g.trick();
    const int position = static_cast<int>(trick.size());
    const Play *win = &trick.front();
    for (const Play &p : trick)
        if (beats(p.card, win->card, v.trump))
            win = &p;
    const Card w = win->card;
    const bool partnerWinning = win->player == partnerOf(v.player);
    const bool canFollow = legal.front().suit == g.ledSuit();

    Cards beaters;
    for (const Card &c : legal)
        if (beats(c, w, v.trump))
            beaters.push_back(c);

    if (canFollow) {
        if (position == 3)
            return partnerWinning || beaters.empty() ? lowest(legal) : lowest(beaters);
        if (position == 2) {
            const int fourth = (v.player + 1) % 4;
            const Cards *fourthHand = nullptr;
            if (g.dummyVisible() && fourth == g.dummy())
                fourthHand = &g.hand(fourth);
            Card fourthBest(0, 0);
            if (fourthHand) {
                const Cards suit = ofSuit(*fourthHand, g.ledSuit());
                if (!suit.empty())
                    fourthBest = highest(suit);
            }
            if (partnerWinning) {
                // Leave partner's card alone when it will hold.
                bool holds = !careful || v.master(w);
                if (fourthHand && fourthBest.rank != 0)
                    holds = holds || !beats(fourthBest, w, v.trump);
                else if (fourthHand)
                    holds = holds || v.trump == NoTrump || countSuit(*fourthHand, v.trump) == 0;
                if (holds)
                    return lowest(legal);
            }
            if (beaters.empty())
                return lowest(legal);
            if (fourthHand && fourthBest.rank != 0) {
                Cards over;
                for (const Card &c : beaters)
                    if (beats(c, fourthBest, v.trump))
                        over.push_back(c);
                if (over.empty())
                    return lowest(legal);
                return lowest(over);
            }
            // Third hand high, but no higher than needed.
            return careful ? lowestEquivalent(v, g.hand(v.player), highest(beaters)) : highest(beaters);
        }
        // Second hand low, but cover an honour with an honour.
        if (careful && position == 1 && w.high() >= 11 && !beaters.empty() && !partnerWinning) {
            const Card cover = lowest(beaters);
            if (cover.high() >= 11)
                return cover;
        }
        return lowest(legal);
    }

    Cards trumps = ofSuit(legal, v.trump);
    if (partnerWinning && (position == 3 || v.master(w) || !careful))
        return discard(v, legal);
    if (!trumps.empty()) {
        if (w.suit == v.trump) {
            Cards over;
            for (const Card &c : trumps)
                if (c.high() > w.high())
                    over.push_back(c);
            if (!over.empty())
                return lowest(over);
        } else {
            return lowest(trumps);
        }
    }
    Cards rest;
    for (const Card &c : legal)
        if (c.suit != v.trump)
            rest.push_back(c);
    return discard(v, rest.empty() ? legal : rest);
}

} // namespace

Bot::Bot(Difficulty difficulty, unsigned seed)
    : m_difficulty(difficulty)
    , m_rng(seed)
{
}

Call Bot::chooseCall(const Game &game, int player)
{
    if (game.auction().turn() != player)
        return Call::pass();
    return game.bidding().choose(game.auction(), game.hand(player));
}

Card Bot::choosePlay(const Game &game, int player)
{
    const Cards legal = game.legalCards(player);
    if (legal.size() == 1)
        return legal.front();
    switch (m_difficulty) {
    case Difficulty::Easy:
        if (game.trick().empty())
            return legal[std::uniform_int_distribution<size_t>(0, legal.size() - 1)(m_rng)];
        return rulePlay(game, player, false);
    case Difficulty::Medium:
        return rulePlay(game, player, true);
    case Difficulty::Hard:
        break;
    }
    return playSimulated(game, player, legal);
}

Card Bot::rulePlay(const Game &game, int player, bool careful)
{
    const Cards legal = game.legalCards(player);
    if (legal.size() == 1)
        return legal.front();
    const View v(game, player);
    return game.trick().empty() ? lead(v, legal, careful) : follow(v, legal, careful);
}

bool Bot::guessHands(const Game &game, int viewer, const Analysis &analysis, Cards hands[Game::Players])
{
    bool visible[Game::Players] = {};
    visible[viewer] = true;
    if (game.dummyVisible())
        visible[game.dummy()] = true;

    bool shownOut[Game::Players][4] = {};
    const std::vector<Play> &played = game.played();
    for (size_t i = 0; i < played.size(); ++i) {
        const int led = played[i - i % 4].card.suit;
        if (played[i].card.suit != led)
            shownOut[played[i].player][led] = true;
    }

    int need[Game::Players];
    Cards unseen;
    for (int p = 0; p < Game::Players; ++p) {
        hands[p] = visible[p] ? game.hand(p) : Cards();
        need[p] = visible[p] ? 0 : static_cast<int>(game.hand(p).size());
        if (!visible[p])
            unseen.insert(unseen.end(), game.hand(p).begin(), game.hand(p).end());
    }
    Cards playedBy[Game::Players];
    for (const Play &p : played)
        playedBy[p.player].push_back(p.card);

    const int attempts = 60;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        const bool strict = attempt < attempts - 10;
        Cards deal[Game::Players];
        int left[Game::Players];
        std::copy(need, need + Game::Players, left);
        Cards order = unseen;
        std::shuffle(order.begin(), order.end(), m_rng);
        auto open = [&](int p, const Card &c) { return left[p] > 0 && !shownOut[p][c.suit]; };
        auto choices = [&](const Card &c) {
            int n = 0;
            for (int p = 0; p < Game::Players; ++p)
                n += open(p, c);
            return n;
        };
        // The most constrained cards first.
        std::stable_sort(order.begin(), order.end(), [&](const Card &a, const Card &b) { return choices(a) < choices(b); });
        bool ok = true;
        for (const Card &c : order) {
            int total = 0;
            for (int p = 0; p < Game::Players; ++p)
                if (open(p, c))
                    total += left[p];
            if (total == 0) {
                ok = false;
                break;
            }
            int pick = std::uniform_int_distribution<int>(0, total - 1)(m_rng);
            for (int p = 0; p < Game::Players; ++p) {
                if (!open(p, c))
                    continue;
                if (pick < left[p]) {
                    deal[p].push_back(c);
                    --left[p];
                    break;
                }
                pick -= left[p];
            }
        }
        if (!ok)
            continue;
        // What the bidding showed, with a little room for judgement.
        if (strict) {
            for (int p = 0; p < Game::Players && ok; ++p) {
                if (visible[p])
                    continue;
                Cards dealt = deal[p];
                dealt.insert(dealt.end(), playedBy[p].begin(), playedBy[p].end());
                const HandEval e(dealt);
                const Shown &s = analysis.shown[p];
                if (e.hcp < s.points.min - 2 || e.hcp > s.points.max + 1)
                    ok = false;
                for (int suit = 0; suit < 4 && ok; ++suit)
                    if (!s.len[suit].contains(e.len[suit]))
                        ok = false;
            }
            if (!ok)
                continue;
        }
        for (int p = 0; p < Game::Players; ++p)
            if (!visible[p])
                hands[p] = deal[p];
        return true;
    }
    return false;
}

Card Bot::playSimulated(const Game &game, int player, const Cards &legal)
{
    const View v(game, player);
    // One card of each run of equivalent cards.
    Cards options;
    for (const Card &c : legal) {
        bool same = false;
        for (const Card &o : options) {
            if (o.suit != c.suit)
                continue;
            bool between = false;
            for (int r = std::min(o.high(), c.high()) + 1; r < std::max(o.high(), c.high()); ++r) {
                const Card x(r == 14 ? 1 : r, c.suit);
                if (!v.played[x.id()])
                    between = true;
            }
            // Nor on the table now.
            for (const Play &p : game.trick())
                if (p.card.suit == c.suit && p.card.high() > std::min(o.high(), c.high()) && p.card.high() < std::max(o.high(), c.high()))
                    between = true;
            if (!between)
                same = true;
        }
        if (!same)
            options.push_back(c);
    }
    if (options.size() == 1)
        return options.front();

    const int viewer = game.controller(player);
    const int side = sideOf(player);
    const bool declaring = sideOf(game.declarer()) == side;
    const int target = game.contract().target();
    const Analysis analysis = game.bidding().analyse(game.auction());
    std::vector<double> value(options.size(), 0.0);
    int cardsLeft = 0;
    for (int p = 0; p < Game::Players; ++p)
        cardsLeft += static_cast<int>(game.hand(p).size());

    auto utility = [&](int ours) {
        const int needed = declaring ? target : 14 - target;
        return ours + (ours >= needed ? 4.0 : 0.0);
    };

    Cards hands[Game::Players];
    int samples = 0;
    const int wanted = cardsLeft <= SolveCards ? SolveSamples : PlayoutSamples;
    for (int s = 0; s < wanted; ++s) {
        if (!guessHands(game, viewer, analysis, hands))
            break;
        ++samples;
        if (cardsLeft <= SolveCards) {
            const std::vector<int> ns = m_solver.solveMoves(hands, game.contract().strain, game.leader(), game.trick(), options);
            const int remaining = Game::HandSize - game.tricksPlayed();
            for (size_t i = 0; i < options.size(); ++i)
                value[i] += utility(game.tricks(side) + (side == 0 ? ns[i] : remaining - ns[i]));
            continue;
        }
        for (size_t i = 0; i < options.size(); ++i) {
            Game sim = game;
            for (int p = 0; p < Game::Players; ++p)
                sim.replaceHand(p, hands[p]);
            sim.play(player, options[i]);
            while (sim.phase() == Phase::Play) {
                if (sim.trickComplete())
                    sim.collectTrick();
                else
                    sim.play(sim.turn(), rulePlay(sim, sim.turn(), true));
            }
            value[i] += utility(sim.tricks(side));
        }
    }
    if (samples == 0)
        return rulePlay(game, player, true);
    return options[std::max_element(value.begin(), value.end()) - value.begin()];
}

} // namespace bridge
