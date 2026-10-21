#include "game.h"

#include <algorithm>

namespace bridge {

Game::Game(unsigned seed)
    : m_rng(seed)
{
}

void Game::newGame()
{
    Cards deck = fullDeck();
    std::shuffle(deck.begin(), deck.end(), m_rng);
    newGame(deck, std::uniform_int_distribution<int>(0, Players - 1)(m_rng));
}

void Game::newGame(const Cards &deck, int dealer)
{
    m_sheet.reset(m_scoring);
    m_dealer = dealer;
    dealFrom(deck);
}

void Game::dealFrom(const Cards &deck)
{
    for (int p = 0; p < Players; ++p)
        m_hand[p].clear();
    for (int i = 0; i < Players * HandSize; ++i)
        m_hand[(m_dealer + 1 + i) % Players].push_back(deck[i]);
    for (int p = 0; p < Players; ++p) {
        sortHand(m_hand[p]);
        m_dealt[p] = m_hand[p];
    }
    m_auction = Auction(m_dealer);
    m_contract = Contract();
    m_trick.clear();
    m_played.clear();
    m_tricks[0] = m_tricks[1] = 0;
    m_turn = m_dealer;
    m_phase = Phase::Bidding;
}

int Game::turn() const
{
    if (m_phase == Phase::Bidding)
        return m_auction.turn();
    return m_phase == Phase::Play ? m_turn : -1;
}

bool Game::call(int player, const Call &call)
{
    if (m_phase != Phase::Bidding || player != m_auction.turn() || !m_auction.add(call))
        return false;
    if (!m_auction.finished())
        return true;
    m_contract = m_auction.contract();
    if (m_contract.valid())
        startPlay();
    else
        scoreDeal();
    return true;
}

void Game::startPlay()
{
    m_phase = Phase::Play;
    m_leader = (m_contract.declarer + 1) % Players;
    m_turn = m_leader;
}

bool Game::canPlayCard(int player, const Card &card) const
{
    if (m_phase != Phase::Play || m_turn != player || !contains(m_hand[player], card))
        return false;
    const int led = ledSuit();
    if (led < 0 || card.suit == led)
        return true;
    for (const Card &c : m_hand[player])
        if (c.suit == led)
            return false;
    return true;
}

Cards Game::legalCards(int player) const
{
    Cards out;
    for (const Card &c : m_hand[player])
        if (canPlayCard(player, c))
            out.push_back(c);
    return out;
}

bool Game::play(int player, const Card &card)
{
    if (!canPlayCard(player, card))
        return false;
    removeCard(m_hand[player], card);
    m_trick.push_back({player, card});
    m_played.push_back({player, card});
    m_turn = trickComplete() ? -1 : (player + 1) % Players;
    return true;
}

int Game::trickWinner() const
{
    if (!trickComplete())
        return -1;
    const int trump = m_contract.strain;
    const Play *best = &m_trick.front();
    for (const Play &p : m_trick) {
        if (p.card.suit == best->card.suit) {
            if (p.card.high() > best->card.high())
                best = &p;
        } else if (p.card.suit == trump) {
            best = &p;
        }
    }
    return best->player;
}

bool Game::collectTrick()
{
    if (m_phase != Phase::Play || !trickComplete())
        return false;
    const int winner = trickWinner();
    ++m_tricks[sideOf(winner)];
    m_trick.clear();
    m_leader = winner;
    m_turn = winner;
    if (tricksPlayed() == HandSize)
        scoreDeal();
    return true;
}

void Game::scoreDeal()
{
    m_turn = -1;
    const int tricks = m_contract.valid() ? m_tricks[sideOf(m_contract.declarer)] : 0;
    const Honors h = m_contract.valid() ? honors(m_dealt, m_contract.strain) : Honors();
    m_sheet.record(m_dealer, m_contract, tricks, h);
    m_phase = m_sheet.finished() ? Phase::GameOver : Phase::Score;
}

bool Game::nextDeal()
{
    Cards deck = fullDeck();
    std::shuffle(deck.begin(), deck.end(), m_rng);
    return nextDeal(deck);
}

bool Game::nextDeal(const Cards &deck)
{
    if (m_phase != Phase::Score)
        return false;
    // A deal passed out in Chicago is dealt again by the same dealer.
    if (!(m_sheet.mode() == Scoring::Chicago && lastDeal() && lastDeal()->passedOut()))
        m_dealer = (m_dealer + 1) % Players;
    dealFrom(deck);
    return true;
}

} // namespace bridge
