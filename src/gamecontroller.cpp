#include "gamecontroller.h"

#include <QStringList>

using namespace bridge;

namespace {

// How long the bots think, and how long a finished trick stays on the
// table, from slow to very fast.
const int BotDelayMs[] = {1100, 750, 450, 200};
const int TrickHoldMs[] = {1800, 1200, 800, 450};

QVariantList ids(const Cards &cards)
{
    QVariantList out;
    for (const Card &c : cards)
        out << c.id();
    return out;
}

QVariantMap contractMap(const Contract &c)
{
    QVariantMap m;
    if (!c.valid())
        return m;
    m.insert(QStringLiteral("level"), c.level);
    m.insert(QStringLiteral("strain"), c.strain);
    m.insert(QStringLiteral("doubled"), c.doubled);
    m.insert(QStringLiteral("declarer"), c.declarer);
    return m;
}

QVariantMap dealMap(const DealRecord &r)
{
    QVariantMap m = contractMap(r.contract);
    m.insert(QStringLiteral("number"), r.number);
    m.insert(QStringLiteral("dealer"), r.dealer);
    m.insert(QStringLiteral("passedOut"), r.passedOut());
    m.insert(QStringLiteral("tricks"), r.tricks);
    m.insert(QStringLiteral("made"), r.made());
    // Over- or undertricks, negative when down.
    m.insert(QStringLiteral("result"), r.contract.valid() ? r.tricks - r.contract.target() : 0);
    m.insert(QStringLiteral("points"), QVariantList() << r.total(0) << r.total(1));
    m.insert(QStringLiteral("below"), QVariantList() << r.below[0] << r.below[1]);
    m.insert(QStringLiteral("above"), QVariantList() << r.above[0] << r.above[1]);
    m.insert(QStringLiteral("vulnerable"), QVariantList() << r.vulnerable[0] << r.vulnerable[1]);
    m.insert(QStringLiteral("honorsSeat"), r.honors.seat);
    m.insert(QStringLiteral("honorsPoints"), r.honors.points);
    m.insert(QStringLiteral("gameWon"), r.gameWon);
    m.insert(QStringLiteral("rubberWon"), r.rubberWon);
    return m;
}

} // namespace

GameController::GameController(QObject *parent)
    : QObject(parent)
{
    m_difficulty = qBound(0, m_settings.value(QStringLiteral("difficulty"), 1).toInt(), 2);
    m_scoring = qBound(0, m_settings.value(QStringLiteral("scoring"), 0).toInt(), 1);
    m_system = qBound(0, m_settings.value(QStringLiteral("system"), 0).toInt(), SystemCount - 1);
    m_theirSystem = qBound(0, m_settings.value(QStringLiteral("theirSystem"), 0).toInt(), SystemCount - 1);
    m_speed = qBound(0, m_settings.value(QStringLiteral("speed"), 1).toInt(), 3);
    m_botTimer.setSingleShot(true);
    connect(&m_botTimer, &QTimer::timeout, this, &GameController::botStep);
}

int GameController::phase() const
{
    switch (m_game.phase()) {
    case bridge::Phase::Bidding:
        return Bidding;
    case bridge::Phase::Play:
        return Play;
    case bridge::Phase::Score:
        return Score;
    case bridge::Phase::GameOver:
        break;
    }
    return GameOver;
}

QVariantList GameController::vulnerable() const
{
    return QVariantList() << m_game.vulnerable(0) << m_game.vulnerable(1);
}

QVariantList GameController::hands() const
{
    QVariantList out;
    for (int p = 0; p < Game::Players; ++p)
        out << QVariant(ids(m_game.hand(p)));
    return out;
}

QVariantList GameController::dealt() const
{
    QVariantList out;
    for (int p = 0; p < Game::Players; ++p)
        out << QVariant(ids(m_game.dealt(p)));
    return out;
}

QVariantList GameController::auction() const
{
    QVariantList out;
    const Auction &a = m_game.auction();
    const Analysis an = m_game.bidding().analyse(a);
    for (size_t i = 0; i < a.size(); ++i) {
        QVariantMap m;
        m.insert(QStringLiteral("call"), a.calls()[i].id());
        m.insert(QStringLiteral("player"), a.seatOf(i));
        // Calls that do not mean what they say get an alert.
        const Convention c = an.meanings[i].convention;
        const bool alert = !an.meanings[i].natural() && c != Convention::TakeoutDouble && c != Convention::PenaltyDouble
                           && c != Convention::WeakTwo && c != Convention::Preempt;
        m.insert(QStringLiteral("alert"), alert);
        out << m;
    }
    return out;
}

int GameController::minLevel() const
{
    const Auction &a = m_game.auction();
    if (m_game.phase() != bridge::Phase::Bidding)
        return 8;
    const Call last = a.lastBid();
    if (!last.isBid())
        return 1;
    return last.strain == NoTrump ? last.level + 1 : last.level;
}

bool GameController::canDouble() const
{
    return m_game.phase() == bridge::Phase::Bidding && m_game.auction().isLegal(Call::dbl());
}

bool GameController::canRedouble() const
{
    return m_game.phase() == bridge::Phase::Bidding && m_game.auction().isLegal(Call::redbl());
}

QVariantMap GameController::contract() const
{
    return contractMap(m_game.contract());
}

QVariantList GameController::trick() const
{
    QVariantList out;
    for (const bridge::Play &p : m_game.trick()) {
        QVariantMap m;
        m.insert(QStringLiteral("card"), p.card.id());
        m.insert(QStringLiteral("player"), p.player);
        out << m;
    }
    return out;
}

QVariantList GameController::tricks() const
{
    return QVariantList() << m_game.tricks(0) << m_game.tricks(1);
}

QVariantList GameController::taken() const
{
    // By the player who won each finished trick.
    QVariantList byPlayer[Game::Players];
    const std::vector<bridge::Play> &played = m_game.played();
    const int trump = m_game.contract().strain;
    const size_t finished = played.size() - m_game.trick().size();
    for (size_t t = 0; t + 4 <= finished; t += 4) {
        size_t best = t;
        for (size_t i = t + 1; i < t + 4; ++i) {
            const Card &c = played[i].card;
            const Card &b = played[best].card;
            if ((c.suit == b.suit && c.high() > b.high()) || (c.suit != b.suit && c.suit == trump))
                best = i;
        }
        for (size_t i = t; i < t + 4; ++i)
            byPlayer[played[best].player] << played[i].card.id();
    }
    QVariantList out;
    for (const QVariantList &l : byPlayer)
        out << QVariant(l);
    return out;
}

bool GameController::controls(int seat) const
{
    if (seat == Human)
        return true;
    return seat == partnerOf(Human) && m_game.contract().valid() && sideOf(m_game.declarer()) == sideOf(Human);
}

bool GameController::humanTurn() const
{
    if (!m_started)
        return false;
    if (m_game.phase() == bridge::Phase::Bidding)
        return m_game.turn() == Human;
    return m_game.phase() == bridge::Phase::Play && m_game.turn() >= 0 && controls(m_game.turn());
}

QVariantList GameController::scores() const
{
    return QVariantList() << m_game.score().total(0) << m_game.score().total(1);
}

QVariantList GameController::games() const
{
    return QVariantList() << m_game.score().games(0) << m_game.score().games(1);
}

QVariantList GameController::partScores() const
{
    return QVariantList() << m_game.score().partScore(0) << m_game.score().partScore(1);
}

QVariantMap GameController::lastDeal() const
{
    const DealRecord *r = m_game.lastDeal();
    return r ? dealMap(*r) : QVariantMap();
}

QVariantList GameController::deals() const
{
    QVariantList out;
    for (const DealRecord &r : m_game.score().deals())
        out << dealMap(r);
    return out;
}

int GameController::winner() const
{
    return m_game.phase() == bridge::Phase::GameOver ? m_game.score().winner() : -1;
}

QVariantList GameController::gameSystems() const
{
    return QVariantList() << static_cast<int>(m_game.bidding().systemId(0)) << static_cast<int>(m_game.bidding().systemId(1));
}

void GameController::setDifficulty(int difficulty)
{
    difficulty = qBound(0, difficulty, 2);
    if (difficulty == m_difficulty)
        return;
    m_difficulty = difficulty;
    m_settings.setValue(QStringLiteral("difficulty"), difficulty);
    emit difficultyChanged();
}

void GameController::setScoring(int scoring)
{
    scoring = qBound(0, scoring, 1);
    if (scoring == m_scoring)
        return;
    m_scoring = scoring;
    m_settings.setValue(QStringLiteral("scoring"), scoring);
    emit scoringChanged();
}

void GameController::setSystem(int system)
{
    system = qBound(0, system, SystemCount - 1);
    if (system == m_system)
        return;
    m_system = system;
    m_settings.setValue(QStringLiteral("system"), system);
    emit systemChanged();
}

void GameController::setTheirSystem(int system)
{
    system = qBound(0, system, SystemCount - 1);
    if (system == m_theirSystem)
        return;
    m_theirSystem = system;
    m_settings.setValue(QStringLiteral("theirSystem"), system);
    emit systemChanged();
}

void GameController::setSpeed(int speed)
{
    speed = qBound(0, speed, 3);
    if (speed == m_speed)
        return;
    m_speed = speed;
    m_settings.setValue(QStringLiteral("speed"), speed);
    emit speedChanged();
}

QVariantList GameController::stats() const
{
    QVariantList out;
    for (int level = 0; level < 3; ++level) {
        const QString key = QStringLiteral("stats/%1/").arg(level);
        QVariantMap m;
        for (const QString &name : {QStringLiteral("won"), QStringLiteral("lost"), QStringLiteral("made"), QStringLiteral("down")})
            m.insert(name, m_settings.value(key + name, 0).toInt());
        out << m;
    }
    return out;
}

void GameController::resetStats()
{
    m_settings.remove(QStringLiteral("stats"));
    emit statsChanged();
}

QString GameController::playerName(int player) const
{
    switch (player) {
    case 0:
        return tr("You");
    case 1:
        return tr("West");
    case 2:
        return tr("North");
    case 3:
        return tr("East");
    }
    return QString();
}

QString GameController::sideName(int side) const
{
    return side == 0 ? tr("We") : tr("They");
}

QString GameController::systemName(int system) const
{
    switch (static_cast<SystemId>(system)) {
    case SystemId::Standard:
        return tr("Standard American");
    case SystemId::TwoOverOne:
        return tr("Two over one");
    case SystemId::Acol:
        return tr("Acol");
    case SystemId::Precision:
        return tr("Precision");
    }
    return QString();
}

QString GameController::systemDescription(int system) const
{
    switch (static_cast<SystemId>(system)) {
    case SystemId::Standard:
        return tr("Five-card majors, a strong notrump of 15 to 17, a strong 2♣ and weak twos.");
    case SystemId::TwoOverOne:
        return tr("Standard American where a new suit at the two level in answer forces to game, and 1NT over a major is forcing.");
    case SystemId::Acol:
        return tr("The British way: four-card suits, a weak notrump of 12 to 14, a strong 2♣ and weak twos.");
    case SystemId::Precision:
        return tr("A strong, artificial 1♣ for 16 or more; every other opening shows 11 to 15. 1NT is 13 to 15.");
    }
    return QString();
}

QVariantList GameController::systemSummary(int system) const
{
    QVariantList out;
    const SystemId id = static_cast<SystemId>(qBound(0, system, SystemCount - 1));
    const bridge::Bidding b(id, id);
    const struct {
        QString title;
        const char *calls;
    } sections[] = {
        {tr("Openings"), ""},
        {tr("Answers to 1NT"), "1NT P"},
        {tr("Answers to one of a major"), "1S P"},
    };
    for (const auto &section : sections) {
        Auction a(0);
        for (const Call &c : parseCalls(section.calls))
            a.add(c);
        const Analysis an = b.analyse(a);
        QVariantMap header;
        header.insert(QStringLiteral("header"), section.title);
        out << header;
        std::vector<int> seen;
        for (const Meaning &m : b.candidates(a, an)) {
            if (m.general || m.lastResort || std::find(seen.begin(), seen.end(), m.call.id()) != seen.end())
                continue;
            seen.push_back(m.call.id());
            QVariantMap item;
            item.insert(QStringLiteral("call"), m.call.id());
            item.insert(QStringLiteral("text"), describe(b.explain(a, an, m.call)));
            out << item;
        }
    }
    return out;
}

QString GameController::suitSymbol(int strain) const
{
    static const char *symbols[] = {"♣", "♦", "♥", "♠"};
    return strain >= 0 && strain < 4 ? QString::fromUtf8(symbols[strain]) : tr("NT");
}

QString GameController::callText(int callId) const
{
    const Call c = Call::fromId(callId);
    switch (c.kind) {
    case Call::Pass:
        return tr("Pass");
    case Call::Double:
        return tr("X", "double");
    case Call::Redouble:
        return tr("XX", "redouble");
    case Call::Bid:
        break;
    }
    return QString::number(c.level) + suitSymbol(c.strain);
}

QString GameController::contractText(const QVariantMap &contract) const
{
    if (contract.isEmpty())
        return QString();
    const int doubled = contract.value(QStringLiteral("doubled")).toInt();
    const Call c(contract.value(QStringLiteral("level")).toInt(), contract.value(QStringLiteral("strain")).toInt());
    return callText(c.id()) + (doubled == 2 ? tr("XX", "redoubled") : doubled == 1 ? tr("X", "doubled") : QString());
}

QString GameController::describe(const Meaning &m) const
{
    QStringList parts;
    switch (m.convention) {
    case Convention::Natural:
        break;
    case Convention::StrongClub:
        parts << tr("Strong and artificial");
        break;
    case Convention::ClubNegative:
        parts << tr("Negative, artificial");
        break;
    case Convention::StrongTwo:
        parts << tr("Strong and artificial");
        break;
    case Convention::Waiting:
        parts << tr("Waiting, artificial");
        break;
    case Convention::WeakTwo:
        parts << tr("Weak two");
        break;
    case Convention::Preempt:
        parts << tr("Preemptive");
        break;
    case Convention::ShortDiamonds:
        parts << tr("Three-suited, short in %1").arg(suitSymbol(Diamonds));
        break;
    case Convention::Stayman:
        parts << tr("Stayman: asks for a four-card major");
        break;
    case Convention::StaymanReply:
        parts << (m.call.strain == Diamonds ? tr("No four-card major") : tr("Answers Stayman"));
        break;
    case Convention::Transfer:
        parts << tr("Transfer: shows %1").arg(suitSymbol(m.suit));
        break;
    case Convention::TransferAccept:
        parts << tr("Completes the transfer");
        break;
    case Convention::Jacoby:
        parts << tr("Jacoby: a game raise");
        break;
    case Convention::Shortness:
        parts << tr("A singleton or void");
        break;
    case Convention::Blackwood:
        parts << tr("Blackwood: asks for aces");
        break;
    case Convention::AceReply:
        parts << (m.suit == 0 ? tr("No aces, or all four") : tr("%n ace(s)", "", m.suit));
        break;
    case Convention::KingAsk:
        parts << tr("Asks for kings");
        break;
    case Convention::KingReply:
        parts << tr("%n king(s)", "", m.suit);
        break;
    case Convention::Quantitative:
        parts << tr("Invites 6NT");
        break;
    case Convention::TakeoutDouble:
        parts << tr("Takeout double");
        break;
    case Convention::NegativeDouble:
        parts << tr("Negative double");
        break;
    case Convention::PenaltyDouble:
        parts << tr("Penalty double");
        break;
    case Convention::Redouble:
        parts << tr("Shows strength");
        break;
    }

    const Range &p = m.points;
    if (p.min > 0 && p.max < 37)
        parts << (p.min == p.max ? tr("%1 points").arg(p.min) : tr("%1–%2 points").arg(p.min).arg(p.max));
    else if (p.min > 0)
        parts << tr("%1+ points").arg(p.min);
    else if (p.max < 37)
        parts << tr("up to %1 points").arg(p.max);

    for (int s : {Spades, Hearts, Diamonds, Clubs}) {
        const Range &r = m.len[s];
        if (r.min == 0 && r.max == 13)
            continue;
        const QString suit = suitSymbol(s);
        if (r.min == r.max)
            parts << QStringLiteral("%1 %2").arg(r.min).arg(suit);
        else if (r.max == 13)
            parts << QStringLiteral("%1+ %2").arg(r.min).arg(suit);
        else if (r.min == 0)
            parts << tr("at most %1 %2").arg(r.max).arg(suit);
        else
            parts << QStringLiteral("%1–%2 %3").arg(r.min).arg(r.max).arg(suit);
    }
    if (m.balanced)
        parts << tr("balanced");

    switch (m.force) {
    case Force::None:
        break;
    case Force::Invitational:
        parts << tr("invitational");
        break;
    case Force::OneRound:
        parts << tr("forcing");
        break;
    case Force::Game:
        parts << tr("forcing to game");
        break;
    }
    if (parts.isEmpty())
        return m.call.kind == Call::Pass ? tr("Nothing more to say") : tr("Natural");
    return parts.join(QStringLiteral(", "));
}

QString GameController::explain(int index) const
{
    const Auction &a = m_game.auction();
    if (index < 0 || index >= static_cast<int>(a.size()))
        return QString();
    return describe(m_game.bidding().analyse(a).meanings[index]);
}

QString GameController::explainCall(int callId) const
{
    const Auction &a = m_game.auction();
    if (m_game.phase() != bridge::Phase::Bidding)
        return QString();
    const bridge::Bidding &b = m_game.bidding();
    return describe(b.explain(a, b.analyse(a), Call::fromId(callId)));
}

int GameController::hint() const
{
    if (m_game.phase() != bridge::Phase::Bidding || m_game.turn() != Human)
        return -1;
    return m_game.bidding().choose(m_game.auction(), m_game.hand(Human)).id();
}

void GameController::newGame()
{
    m_botTimer.stop();
    m_bot.setDifficulty(static_cast<Difficulty>(m_difficulty));
    m_game.setScoring(static_cast<Scoring>(m_scoring));
    m_game.bidding().setSystem(0, static_cast<SystemId>(m_system));
    m_game.bidding().setSystem(1, static_cast<SystemId>(m_theirSystem));
    m_game.newGame();
    m_started = true;
    m_recorded = false;
    m_countedDeals = 0;
    afterAction();
}

bool GameController::canCall(int callId) const
{
    return m_started && m_game.phase() == bridge::Phase::Bidding && m_game.turn() == Human
           && m_game.auction().isLegal(Call::fromId(callId));
}

bool GameController::call(int callId)
{
    if (!canCall(callId) || !m_game.call(Human, Call::fromId(callId)))
        return false;
    afterAction();
    return true;
}

bool GameController::canPlay(int cardId) const
{
    return humanTurn() && m_game.phase() == bridge::Phase::Play && m_game.canPlayCard(m_game.turn(), Card::fromId(cardId));
}

bool GameController::play(int cardId)
{
    if (!canPlay(cardId) || !m_game.play(m_game.turn(), Card::fromId(cardId)))
        return false;
    afterAction();
    return true;
}

bool GameController::nextDeal()
{
    if (!m_game.nextDeal())
        return false;
    afterAction();
    return true;
}

void GameController::afterAction()
{
    if (!m_started)
        return;
    const bridge::Phase p = m_game.phase();
    const int counted = static_cast<int>(m_game.score().deals().size());
    if ((p == bridge::Phase::Score || p == bridge::Phase::GameOver) && counted != m_countedDeals) {
        m_countedDeals = counted;
        const DealRecord *r = m_game.lastDeal();
        // Your contracts, made or not, whichever of you two declared.
        if (r && r->contract.valid() && sideOf(r->contract.declarer) == sideOf(Human)) {
            const QString key = QStringLiteral("stats/%1/%2").arg(gameDifficulty()).arg(r->made() ? QStringLiteral("made") : QStringLiteral("down"));
            m_settings.setValue(key, m_settings.value(key, 0).toInt() + 1);
            emit statsChanged();
        }
    }
    if (p == bridge::Phase::GameOver)
        recordResult();

    if (p == bridge::Phase::Bidding && m_game.turn() != Human) {
        m_botTimer.setInterval(BotDelayMs[m_speed]);
        m_botTimer.start();
    } else if (p == bridge::Phase::Play) {
        if (m_game.trickComplete()) {
            m_botTimer.setInterval(TrickHoldMs[m_speed]);
            m_botTimer.start();
        } else if (!controls(m_game.turn())) {
            m_botTimer.setInterval(BotDelayMs[m_speed]);
            m_botTimer.start();
        }
    }
    emit changed();
}

void GameController::botStep()
{
    if (m_game.phase() == bridge::Phase::Bidding) {
        const int player = m_game.turn();
        if (player == Human)
            return;
        m_game.call(player, m_bot.chooseCall(m_game, player));
    } else if (m_game.phase() == bridge::Phase::Play) {
        if (m_game.trickComplete()) {
            m_game.collectTrick();
        } else {
            const int player = m_game.turn();
            if (controls(player))
                return;
            m_game.play(player, m_bot.choosePlay(m_game, player));
        }
    } else {
        return;
    }
    afterAction();
}

void GameController::recordResult()
{
    if (m_recorded)
        return;
    m_recorded = true;
    const int w = m_game.score().winner();
    if (w < 0)
        return;
    const QString key = QStringLiteral("stats/%1/%2").arg(gameDifficulty()).arg(w == sideOf(Human) ? QStringLiteral("won") : QStringLiteral("lost"));
    m_settings.setValue(key, m_settings.value(key, 0).toInt() + 1);
    emit statsChanged();
}
