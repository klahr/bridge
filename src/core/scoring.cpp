#include "scoring.h"

namespace bridge {

namespace {

int trickValue(int strain) { return strain <= Diamonds ? 20 : 30; }

} // namespace

ContractScore scoreContract(const Contract &c, bool vulnerable, int tricks)
{
    ContractScore s;
    if (!c.valid())
        return s;
    const int multiplier = c.doubled == 2 ? 4 : c.doubled == 1 ? 2 : 1;
    if (tricks >= c.target()) {
        s.below = (trickValue(c.strain) * c.level + (c.strain == NoTrump ? 10 : 0)) * multiplier;
        const int over = tricks - c.target();
        if (c.doubled == 0)
            s.above = over * trickValue(c.strain);
        else
            s.above = over * (vulnerable ? 200 : 100) * (c.doubled == 2 ? 2 : 1) + (c.doubled == 2 ? 100 : 50);
        if (c.level == 6)
            s.above += vulnerable ? 750 : 500;
        else if (c.level == 7)
            s.above += vulnerable ? 1500 : 1000;
        return s;
    }
    const int under = c.target() - tricks;
    if (c.doubled == 0) {
        s.penalty = under * (vulnerable ? 100 : 50);
        return s;
    }
    // Doubled: 100, 200, 200, then 300 each not vulnerable; 200, then 300 each vulnerable.
    for (int i = 1; i <= under; ++i) {
        if (vulnerable)
            s.penalty += i == 1 ? 200 : 300;
        else
            s.penalty += i == 1 ? 100 : i <= 3 ? 200 : 300;
    }
    if (c.doubled == 2)
        s.penalty *= 2;
    return s;
}

Honors honors(const Cards hands[4], int strain)
{
    Honors h;
    for (int p = 0; p < 4; ++p) {
        int count = 0;
        for (const Card &c : hands[p]) {
            if (strain == NoTrump)
                count += c.rank == 1;
            else
                count += c.suit == strain && (c.rank == 1 || c.rank >= 10);
        }
        if (strain == NoTrump ? count == 4 : count == 5)
            h.points = 150;
        else if (strain != NoTrump && count == 4)
            h.points = 100;
        if (h.points) {
            h.seat = p;
            return h;
        }
    }
    return h;
}

void ScoreSheet::reset(Scoring mode)
{
    m_mode = mode;
    m_deals.clear();
    m_number = 1;
    m_games[0] = m_games[1] = 0;
    m_part[0] = m_part[1] = 0;
    m_finished = false;
}

bool ScoreSheet::vulnerable(int side, int dealer) const
{
    if (m_mode == Scoring::Rubber)
        return m_games[side] > 0;
    // Chicago: nobody on the first deal, the dealer's side on the second
    // and third, everybody on the fourth.
    switch (m_number) {
    case 1:
        return false;
    case 2:
    case 3:
        return sideOf(dealer) == side;
    }
    return true;
}

void ScoreSheet::record(int dealer, const Contract &contract, int tricks, const Honors &h)
{
    if (m_finished)
        return;
    DealRecord r;
    r.number = m_number;
    r.dealer = dealer;
    r.contract = contract;
    r.tricks = tricks;
    for (int side = 0; side < 2; ++side)
        r.vulnerable[side] = vulnerable(side, dealer);

    if (!contract.valid()) {
        // Passed out: in Chicago the same dealer deals again.
        m_deals.push_back(r);
        if (m_mode == Scoring::Rubber)
            ++m_number;
        return;
    }

    const int decl = sideOf(contract.declarer);
    const int def = 1 - decl;
    const ContractScore s = scoreContract(contract, r.vulnerable[decl], tricks);
    r.below[decl] = s.below;
    r.above[decl] = s.above;
    r.above[def] = s.penalty;
    r.honors = h;
    if (h.seat >= 0)
        r.above[sideOf(h.seat)] += h.points;

    m_part[decl] += s.below;
    if (m_part[decl] >= 100) {
        r.gameWon = decl;
        m_part[0] = m_part[1] = 0;
        ++m_games[decl];
        if (m_mode == Scoring::Chicago)
            r.above[decl] += r.vulnerable[decl] ? 500 : 300;
    } else if (m_mode == Scoring::Chicago && m_number == 4 && s.below > 0) {
        // A partscore on the last deal that does not make game.
        r.above[decl] += 100;
    }

    if (m_mode == Scoring::Rubber && m_games[decl] == 2) {
        r.rubberWon = decl;
        r.above[decl] += m_games[def] == 0 ? 700 : 500;
        m_finished = true;
    }
    if (m_mode == Scoring::Chicago && m_number == 4)
        m_finished = true;
    m_deals.push_back(r);
    ++m_number;
}

int ScoreSheet::total(int side) const
{
    int t = 0;
    for (const DealRecord &r : m_deals)
        t += r.total(side);
    return t;
}

int ScoreSheet::winner() const
{
    const int ns = total(0);
    const int ew = total(1);
    return ns == ew ? -1 : ns > ew ? 0 : 1;
}

} // namespace bridge
