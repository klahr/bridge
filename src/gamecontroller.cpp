#include "gamecontroller.h"

#include "cards/settings.h"
#include "cards/variant.h"

#include <QStringList>

#include <algorithm>

using namespace bridge;

namespace {

// How long the bots think, and how long a finished trick stays on the
// table, from slow to very fast.
const int BotDelayMs[] = {1100, 750, 450, 200};
const int TrickHoldMs[] = {1800, 1200, 800, 450};

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

GameController::GameController(Session *online, QObject *parent)
    : QObject(parent)
    , m_online(online)
    , m_settings(cards::settingsPath(), QSettings::IniFormat)
{
    m_difficulty = qBound(0, m_settings.value(QStringLiteral("difficulty"), 1).toInt(), 2);
    m_scoring = qBound(0, m_settings.value(QStringLiteral("scoring"), 0).toInt(), 1);
    m_system = qBound(0, m_settings.value(QStringLiteral("system"), 0).toInt(), SystemCount - 1);
    m_theirSystem = qBound(0, m_settings.value(QStringLiteral("theirSystem"), 0).toInt(), SystemCount - 1);
    m_speed = qBound(0, m_settings.value(QStringLiteral("speed"), 1).toInt(), 3);
    m_tapTwice = m_settings.value(QStringLiteral("tapTwice"), false).toBool();
    m_botTimer.setSingleShot(true);
    connect(&m_botTimer, &QTimer::timeout, this, &GameController::botStep);
    connect(m_online, &Session::reset, this, &GameController::replay);
    connect(m_online, &Session::action, this, [this](const QVariantMap &a) {
        if (!m_online->active())
            return;
        applyAction(a);
        afterAction();
    });
    connect(m_online, &Session::tableChanged, this, &GameController::tableChanged);
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

QVariantList GameController::lastTrick() const
{
    return cards::lastTrick(m_game.played(), m_game.tricksPlayed(), Game::Players, m_game.leader());
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
    // Online you play your own cards, and dummy's when you declare.
    if (m_online->active())
        return (m_game.phase() == bridge::Phase::Play ? m_game.controller(seat) : seat) == Human;
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

void GameController::setTapTwice(bool on)
{
    if (on == m_tapTwice)
        return;
    m_tapTwice = on;
    m_settings.setValue(QStringLiteral("tapTwice"), on);
    emit tapTwiceChanged();
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
    if (m_online->active() && player > 0 && player < Game::Players)
        return m_online->playerName(m_online->toTable(player));
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
    case SystemId::ModernStandard:
        return tr("Enkel modern standard");
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
    case SystemId::ModernStandard:
        return tr("Sweden's teaching system: four-card suits, hearts before spades, a strong notrump of 15 to 17, 2NT raising any suit, a strong 2♣ and weak twos.");
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
    case Convention::RaiseNT:
        parts << tr("A raise to game or more");
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

void GameController::start()
{
    if (m_online->active())
        return;
    m_botTimer.stop();
    m_game = bridge::Game();
    m_started = false;
    bool ok = true;
    for (const QVariant &a : m_log.load())
        ok = ok && applyAction(a.toMap());
    if (!ok || !m_started) {
        newGame();
        return;
    }
    // Whatever was counted before the app closed is not counted again.
    m_countedDeals = static_cast<int>(m_game.score().deals().size());
    m_recorded = m_game.phase() == bridge::Phase::GameOver;
    afterAction();
}

void GameController::newGame()
{
    if (m_online->active()) {
        if (!m_started || m_game.phase() == bridge::Phase::GameOver)
            sendNew();
        return;
    }
    m_botTimer.stop();
    m_log.clear();
    m_started = false;
    perform(newAction());
}

bool GameController::canCall(int callId) const
{
    return m_started && m_game.phase() == bridge::Phase::Bidding && m_game.turn() == Human
           && m_game.auction().isLegal(Call::fromId(callId));
}

bool GameController::call(int callId)
{
    if (!canCall(callId))
        return false;
    return perform({{QStringLiteral("t"), QStringLiteral("call")},
                    {QStringLiteral("seat"), m_online->toTable(Human)},
                    {QStringLiteral("call"), callId}});
}

bool GameController::canPlay(int cardId) const
{
    return humanTurn() && m_game.phase() == bridge::Phase::Play && m_game.canPlayCard(m_game.turn(), Card::fromId(cardId));
}

bool GameController::play(int cardId)
{
    if (!canPlay(cardId))
        return false;
    return perform({{QStringLiteral("t"), QStringLiteral("play")},
                    {QStringLiteral("seat"), m_online->toTable(m_game.turn())},
                    {QStringLiteral("card"), cardId}});
}

bool GameController::nextDeal()
{
    if (m_online->active()) {
        if (m_game.phase() != bridge::Phase::Score)
            return false;
        m_online->send({{QStringLiteral("t"), QStringLiteral("next")}, {QStringLiteral("deck"), shuffledDeck()}});
        return true;
    }
    return perform({{QStringLiteral("t"), QStringLiteral("next")}, {QStringLiteral("deck"), shuffledDeck()}});
}

void GameController::afterAction()
{
    if (!m_started)
        return;
    const bridge::Phase p = m_game.phase();
    // Online games stay out of the statistics, which are about the bots.
    if (m_online->active()) {
        m_botTimer.stop();
        if (!m_online->driver()) {
            emit changed();
            return;
        }
    }
    const int counted = static_cast<int>(m_game.score().deals().size());
    if ((p == bridge::Phase::Score || p == bridge::Phase::GameOver) && counted != m_countedDeals && !m_online->active()) {
        m_countedDeals = counted;
        const DealRecord *r = m_game.lastDeal();
        // Your contracts, made or not, whichever of you two declared.
        if (r && r->contract.valid() && sideOf(r->contract.declarer) == sideOf(Human)) {
            const QString key = QStringLiteral("stats/%1/%2").arg(gameDifficulty()).arg(r->made() ? QStringLiteral("made") : QStringLiteral("down"));
            m_settings.setValue(key, m_settings.value(key, 0).toInt() + 1);
            emit statsChanged();
        }
    }
    if (p == bridge::Phase::GameOver && !m_online->active())
        recordResult();

    if (p == bridge::Phase::Bidding && botPlays(m_game.turn())) {
        m_botTimer.setInterval(BotDelayMs[m_speed]);
        m_botTimer.start();
    } else if (p == bridge::Phase::Play) {
        if (m_game.trickComplete()) {
            m_botTimer.setInterval(TrickHoldMs[m_speed]);
            m_botTimer.start();
        } else if (botPlays(m_game.turn())) {
            m_botTimer.setInterval(BotDelayMs[m_speed]);
            m_botTimer.start();
        }
    }
    emit changed();
}

void GameController::botStep()
{
    if (m_online->active() && !m_online->driver())
        return;
    if (m_game.phase() == bridge::Phase::Bidding) {
        const int player = m_game.turn();
        if (!botPlays(player))
            return;
        perform({{QStringLiteral("t"), QStringLiteral("call")},
                 {QStringLiteral("seat"), m_online->toTable(player)},
                 {QStringLiteral("call"), m_bot.chooseCall(m_game, player).id()}});
    } else if (m_game.phase() == bridge::Phase::Play) {
        if (m_game.trickComplete()) {
            perform({{QStringLiteral("t"), QStringLiteral("collect")}});
        } else {
            const int player = m_game.turn();
            if (!botPlays(player))
                return;
            perform({{QStringLiteral("t"), QStringLiteral("play")},
                     {QStringLiteral("seat"), m_online->toTable(player)},
                     {QStringLiteral("card"), m_bot.choosePlay(m_game, player).id()}});
        }
    }
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

bool GameController::botPlays(int seat) const
{
    if (seat < 0)
        return false;
    if (!m_online->active())
        return !controls(seat);
    const int chooser = m_game.phase() == bridge::Phase::Play ? m_game.controller(seat) : seat;
    return m_online->isBot(m_online->toTable(chooser));
}

bool GameController::perform(const QVariantMap &action)
{
    if (m_online->active()) {
        m_online->send(action);
        return true;
    }
    if (!applyAction(action))
        return false;
    m_log.append(action);
    afterAction();
    return true;
}

bool GameController::applyAction(const QVariantMap &a)
{
    const QString t = a.value(QStringLiteral("t")).toString();
    const int seat = a.value(QStringLiteral("seat")).toInt();
    if (t == QLatin1String("new")) {
        Cards deck;
        if ((m_started && m_game.phase() != bridge::Phase::GameOver) || !deckFrom(a.value(QStringLiteral("deck")), &deck))
            return false;
        // The systems come by the table's sides; yours is side 0 here.
        const QVariantList systems = a.value(QStringLiteral("systems")).toList();
        m_bot.setDifficulty(static_cast<Difficulty>(qBound(0, a.value(QStringLiteral("difficulty"), m_difficulty).toInt(), 2)));
        m_game.setScoring(static_cast<Scoring>(qBound(0, a.value(QStringLiteral("scoring")).toInt(), 1)));
        for (int side = 0; side < 2; ++side) {
            const int system = qBound(0, systems.value(m_online->toTable(side) % 2).toInt(), SystemCount - 1);
            m_game.bidding().setSystem(side, static_cast<SystemId>(system));
        }
        m_game.newGame(deck, m_online->toLocal(qBound(0, a.value(QStringLiteral("dealer")).toInt(), Game::Players - 1)));
        m_started = true;
        m_recorded = false;
        m_countedDeals = 0;
        return true;
    }
    if (!m_started || seat < 0 || seat >= Game::Players)
        return false;
    if (t == QLatin1String("call"))
        return m_game.call(m_online->toLocal(seat), Call::fromId(a.value(QStringLiteral("call")).toInt()));
    if (t == QLatin1String("play"))
        return m_game.play(m_online->toLocal(seat), Card::fromId(qBound(0, a.value(QStringLiteral("card")).toInt(), 51)));
    if (t == QLatin1String("collect"))
        return m_game.collectTrick();
    if (t == QLatin1String("next")) {
        Cards deck;
        return deckFrom(a.value(QStringLiteral("deck")), &deck) && m_game.nextDeal(deck);
    }
    return false;
}

void GameController::replay()
{
    if (!m_online->active())
        return;
    m_botTimer.stop();
    m_game = bridge::Game();
    m_started = false;
    for (const QVariant &a : m_online->actions())
        applyAction(a.toMap());
    if (!m_started && m_online->synced() && m_online->actions().isEmpty() && m_online->driver())
        sendNew();
    afterAction();
    emit changed();
}

void GameController::tableChanged()
{
    const bool active = m_online->active();
    if (m_wasActive != active) {
        m_wasActive = active;
        // Back from the table to your own game against the bots.
        if (!active)
            start();
        else
            replay();
        return;
    }
    if (active && !m_started && m_online->synced() && m_online->actions().isEmpty() && m_online->driver())
        sendNew();
    // The driver may have changed, or a bot taken over a seat.
    afterAction();
    emit changed();
}

void GameController::sendNew()
{
    m_online->send(newAction());
}

QVariantMap GameController::newAction()
{
    // The table's sides: the one you sit on bids your system.
    const int seat = m_online->toTable(0);
    QVariantList systems = {0, 0};
    systems[seat % 2] = m_system;
    systems[(seat + 1) % 2] = m_theirSystem;
    return {{QStringLiteral("t"), QStringLiteral("new")},
            {QStringLiteral("scoring"), m_scoring},
            {QStringLiteral("systems"), systems},
            {QStringLiteral("difficulty"), m_difficulty},
            {QStringLiteral("dealer"), std::uniform_int_distribution<int>(0, Game::Players - 1)(m_rng)},
            {QStringLiteral("deck"), shuffledDeck()}};
}

QVariantList GameController::shuffledDeck()
{
    Cards deck = fullDeck();
    std::shuffle(deck.begin(), deck.end(), m_rng);
    return ids(deck);
}
