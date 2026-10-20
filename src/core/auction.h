#pragma once

#include "card.h"

#include <vector>

namespace bridge {

// The calls of one deal, starting with the dealer and going clockwise.
class Auction {
public:
    explicit Auction(int dealer = 0) : m_dealer(dealer) {}

    int dealer() const { return m_dealer; }
    const std::vector<Call> &calls() const { return m_calls; }
    size_t size() const { return m_calls.size(); }
    // Who makes call `i`.
    int seatOf(size_t i) const { return static_cast<int>((m_dealer + i) % 4); }
    // Whose call it is next.
    int turn() const { return seatOf(m_calls.size()); }

    // Three passes after a bid, or four passes.
    bool finished() const;
    bool passedOut() const { return finished() && lastBidIndex() < 0; }

    bool isLegal(const Call &call) const;
    bool add(const Call &call);
    // The first `n` calls, as the auction stood then.
    Auction prefix(size_t n) const;

    // The last bid and who made it; -1 and a pass before anyone has bid.
    int lastBidIndex() const;
    Call lastBid() const;
    int lastBidder() const;
    // 0, or 1 or 2 once the last bid is doubled or redoubled.
    int doubled() const;
    // The cheapest legal level for a bid in `strain`, 8 if none is left.
    int cheapest(int strain) const;

    // Valid once finished, unless passed out.
    Contract contract() const;

private:
    int m_dealer;
    std::vector<Call> m_calls;
};

} // namespace bridge
