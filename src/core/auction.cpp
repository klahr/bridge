#include "auction.h"

#include <algorithm>

namespace bridge {

bool Auction::finished() const
{
    const size_t n = m_calls.size();
    if (n < 4)
        return false;
    for (size_t i = n - 3; i < n; ++i)
        if (m_calls[i].kind != Call::Pass)
            return false;
    // Three passes end it after a bid; four are needed when nobody bid.
    return lastBidIndex() >= 0 || m_calls[n - 4].kind == Call::Pass;
}

int Auction::lastBidIndex() const
{
    for (int i = static_cast<int>(m_calls.size()) - 1; i >= 0; --i)
        if (m_calls[i].isBid())
            return i;
    return -1;
}

Call Auction::lastBid() const
{
    const int i = lastBidIndex();
    return i < 0 ? Call::pass() : m_calls[i];
}

int Auction::lastBidder() const
{
    const int i = lastBidIndex();
    return i < 0 ? -1 : seatOf(i);
}

int Auction::doubled() const
{
    const int last = lastBidIndex();
    int d = 0;
    for (size_t i = last + 1; last >= 0 && i < m_calls.size(); ++i) {
        if (m_calls[i].kind == Call::Double)
            d = 1;
        else if (m_calls[i].kind == Call::Redouble)
            d = 2;
    }
    return d;
}

int Auction::cheapest(int strain) const
{
    const Call last = lastBid();
    if (!last.isBid())
        return 1;
    return strain > last.strain ? last.level : last.level + 1;
}

bool Auction::isLegal(const Call &call) const
{
    if (finished() || !call.valid())
        return false;
    const int bidder = lastBidder();
    const int me = turn();
    switch (call.kind) {
    case Call::Pass:
        return true;
    case Call::Double:
        return bidder >= 0 && sideOf(bidder) != sideOf(me) && doubled() == 0;
    case Call::Redouble:
        return bidder >= 0 && sideOf(bidder) == sideOf(me) && doubled() == 1;
    case Call::Bid:
        break;
    }
    return call.rank() > lastBid().rank();
}

bool Auction::add(const Call &call)
{
    if (!isLegal(call))
        return false;
    m_calls.push_back(call);
    return true;
}

Auction Auction::prefix(size_t n) const
{
    Auction a(m_dealer);
    a.m_calls.assign(m_calls.begin(), m_calls.begin() + std::min(n, m_calls.size()));
    return a;
}

Contract Auction::contract() const
{
    Contract c;
    const int last = lastBidIndex();
    if (!finished() || last < 0)
        return c;
    c.level = m_calls[last].level;
    c.strain = m_calls[last].strain;
    c.doubled = doubled();
    // The declarer is whoever of the side first named the strain.
    const int side = sideOf(seatOf(last));
    for (int i = 0; i <= last; ++i) {
        if (m_calls[i].isBid() && m_calls[i].strain == c.strain && sideOf(seatOf(i)) == side) {
            c.declarer = seatOf(i);
            break;
        }
    }
    return c;
}

} // namespace bridge
