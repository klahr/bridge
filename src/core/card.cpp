#include "card.h"

#include <algorithm>
#include <stdexcept>

namespace bridge {

int hcp(const Cards &cards)
{
    int total = 0;
    for (const Card &c : cards)
        total += hcp(c);
    return total;
}

void sortHand(Cards &cards)
{
    std::sort(cards.begin(), cards.end(), [](const Card &a, const Card &b) {
        if (a.suit != b.suit)
            return a.suit < b.suit;
        return a.high() < b.high();
    });
}

bool contains(const Cards &cards, const Card &card)
{
    return std::find(cards.begin(), cards.end(), card) != cards.end();
}

void removeCard(Cards &cards, const Card &card)
{
    auto it = std::find(cards.begin(), cards.end(), card);
    if (it != cards.end())
        cards.erase(it);
}

Call Call::fromId(int id)
{
    if (id == Double)
        return dbl();
    if (id == Redouble)
        return redbl();
    if (id < 3 || id > 37)
        return pass();
    const int rank = id - 2;
    return Call((rank - 1) / 5 + 1, (rank - 1) % 5);
}

Call parseCall(const std::string &text)
{
    if (text == "P" || text == "p")
        return Call::pass();
    if (text == "X" || text == "x")
        return Call::dbl();
    if (text == "XX" || text == "xx")
        return Call::redbl();
    if (text.size() < 2 || text[0] < '1' || text[0] > '7')
        throw std::invalid_argument("bad call: " + text);
    const std::string s = text.substr(1);
    int strain = -1;
    if (s == "C") strain = Clubs;
    else if (s == "D") strain = Diamonds;
    else if (s == "H") strain = Hearts;
    else if (s == "S") strain = Spades;
    else if (s == "N" || s == "NT") strain = NoTrump;
    if (strain < 0)
        throw std::invalid_argument("bad call: " + text);
    return Call(text[0] - '0', strain);
}

std::vector<Call> parseCalls(const std::string &text)
{
    std::vector<Call> out;
    std::string token;
    for (char ch : text + " ") {
        if (ch == ' ' || ch == ',') {
            if (!token.empty())
                out.push_back(parseCall(token));
            token.clear();
        } else {
            token += ch;
        }
    }
    return out;
}

} // namespace bridge
