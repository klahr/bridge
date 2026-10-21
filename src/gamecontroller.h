#pragma once

#include "core/bot.h"
#include "core/game.h"
#include "cards/gamelog.h"
#include "online/session.h"

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <random>

class GameController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int phase READ phase NOTIFY changed)
    Q_PROPERTY(int dealer READ dealer NOTIFY changed)
    Q_PROPERTY(int dealNumber READ dealNumber NOTIFY changed)
    Q_PROPERTY(QVariantList vulnerable READ vulnerable NOTIFY changed)
    Q_PROPERTY(QVariantList hands READ hands NOTIFY changed)
    Q_PROPERTY(QVariantList auction READ auction NOTIFY changed)
    Q_PROPERTY(int minLevel READ minLevel NOTIFY changed)
    Q_PROPERTY(bool canDouble READ canDouble NOTIFY changed)
    Q_PROPERTY(bool canRedouble READ canRedouble NOTIFY changed)
    Q_PROPERTY(QVariantMap contract READ contract NOTIFY changed)
    Q_PROPERTY(int declarer READ declarer NOTIFY changed)
    Q_PROPERTY(int dummy READ dummy NOTIFY changed)
    Q_PROPERTY(bool dummyVisible READ dummyVisible NOTIFY changed)
    Q_PROPERTY(QVariantList trick READ trick NOTIFY changed)
    Q_PROPERTY(QVariantList lastTrick READ lastTrick NOTIFY changed)
    Q_PROPERTY(int trickWinner READ trickWinner NOTIFY changed)
    Q_PROPERTY(int tricksPlayed READ tricksPlayed NOTIFY changed)
    Q_PROPERTY(QVariantList tricks READ tricks NOTIFY changed)
    Q_PROPERTY(QVariantList taken READ taken NOTIFY changed)
    Q_PROPERTY(int turn READ turn NOTIFY changed)
    Q_PROPERTY(bool humanTurn READ humanTurn NOTIFY changed)
    Q_PROPERTY(QVariantList scores READ scores NOTIFY changed)
    Q_PROPERTY(QVariantList games READ games NOTIFY changed)
    Q_PROPERTY(QVariantList partScores READ partScores NOTIFY changed)
    Q_PROPERTY(QVariantMap lastDeal READ lastDeal NOTIFY changed)
    Q_PROPERTY(QVariantList deals READ deals NOTIFY changed)
    Q_PROPERTY(QVariantList dealt READ dealt NOTIFY changed)
    Q_PROPERTY(int winner READ winner NOTIFY changed)
    Q_PROPERTY(int gameScoring READ gameScoring NOTIFY changed)
    Q_PROPERTY(bool started READ started NOTIFY changed)
    Q_PROPERTY(int gameDifficulty READ gameDifficulty NOTIFY changed)
    Q_PROPERTY(QVariantList gameSystems READ gameSystems NOTIFY changed)
    Q_PROPERTY(int difficulty READ difficulty WRITE setDifficulty NOTIFY difficultyChanged)
    Q_PROPERTY(int scoring READ scoring WRITE setScoring NOTIFY scoringChanged)
    Q_PROPERTY(int system READ system WRITE setSystem NOTIFY systemChanged)
    Q_PROPERTY(int theirSystem READ theirSystem WRITE setTheirSystem NOTIFY systemChanged)
    Q_PROPERTY(QVariantList stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool tapTwice READ tapTwice WRITE setTapTwice NOTIFY tapTwiceChanged)

public:
    enum Phase { Bidding, Play, Score, GameOver };
    Q_ENUM(Phase)

    explicit GameController(Session *online, QObject *parent = nullptr);

    int phase() const;
    int dealer() const { return m_game.dealer(); }
    int dealNumber() const { return m_game.score().dealNumber(); }
    // For north-south and east-west, this deal.
    QVariantList vulnerable() const;
    // Every player's cards, as sorted; ids let the table animate each card
    // from where it lies.
    QVariantList hands() const;
    // The calls so far: call id, player and whether it is artificial.
    QVariantList auction() const;
    // The lowest level a bid may still be made at, 8 when none.
    int minLevel() const;
    bool canDouble() const;
    bool canRedouble() const;
    // level, strain, doubled and declarer; empty before the bidding is over.
    QVariantMap contract() const;
    int declarer() const { return m_game.declarer(); }
    int dummy() const { return m_game.dummy(); }
    bool dummyVisible() const { return m_game.dummyVisible(); }
    // The cards on the table, each with who played it.
    QVariantList trick() const;
    // The trick gathered before, to look back at.
    QVariantList lastTrick() const;
    int trickWinner() const { return m_game.trickWinner(); }
    int tricksPlayed() const { return m_game.tricksPlayed(); }
    // Tricks taken by north-south and east-west.
    QVariantList tricks() const;
    // The cards of the tricks each side has taken.
    QVariantList taken() const;
    int turn() const { return m_game.turn(); }
    // Your call, or a card from your hand or from a dummy you play.
    bool humanTurn() const;
    QVariantList scores() const;
    QVariantList games() const;
    QVariantList partScores() const;
    QVariantMap lastDeal() const;
    // Every deal so far, for the score sheet.
    QVariantList deals() const;
    // The hands as they were dealt, to look back at.
    QVariantList dealt() const;
    int winner() const;
    int gameScoring() const { return static_cast<int>(m_game.scoring()); }
    bool started() const { return m_started; }
    int gameDifficulty() const { return static_cast<int>(m_bot.difficulty()); }
    // The systems north-south and east-west bid this game.
    QVariantList gameSystems() const;
    int difficulty() const { return m_difficulty; }
    void setDifficulty(int difficulty);
    // 0 rubber, 1 Chicago.
    int scoring() const { return m_scoring; }
    void setScoring(int scoring);
    int system() const { return m_system; }
    void setSystem(int system);
    int theirSystem() const { return m_theirSystem; }
    void setTheirSystem(int system);
    QVariantList stats() const;
    // 0 = slow ... 3 = very fast; how long the bots think and tricks stay.
    int speed() const { return m_speed; }
    void setSpeed(int speed);
    // A card is played by a second tap; the first raises it.
    bool tapTwice() const { return m_tapTwice; }
    void setTapTwice(bool on);

    // Carries on the game against the bots from when the app last closed,
    // or deals a new one.
    Q_INVOKABLE void start();
    Q_INVOKABLE void newGame();
    Q_INVOKABLE bool call(int callId);
    Q_INVOKABLE bool play(int cardId);
    Q_INVOKABLE bool nextDeal();
    Q_INVOKABLE bool canCall(int callId) const;
    Q_INVOKABLE bool canPlay(int cardId) const;
    // Whether you choose the cards for `seat`: your own, and dummy's when
    // your side declares.
    Q_INVOKABLE bool controls(int seat) const;
    // The call the bots would make for you.
    Q_INVOKABLE int hint() const;
    Q_INVOKABLE void resetStats();

    // What call `index` of the auction said.
    Q_INVOKABLE QString explain(int index) const;
    // What `callId` would say, made by you now.
    Q_INVOKABLE QString explainCall(int callId) const;
    Q_INVOKABLE QString callText(int callId) const;
    Q_INVOKABLE QString contractText(const QVariantMap &contract) const;
    Q_INVOKABLE QString playerName(int player) const;
    Q_INVOKABLE QString sideName(int side) const;
    Q_INVOKABLE QString systemName(int system) const;
    Q_INVOKABLE QString systemDescription(int system) const;
    // The openings and the answers to 1NT in `system`, each with what it shows.
    Q_INVOKABLE QVariantList systemSummary(int system) const;

signals:
    void changed();
    void difficultyChanged();
    void scoringChanged();
    void systemChanged();
    void statsChanged();
    void speedChanged();
    void tapTwiceChanged();

private:
    static constexpr int Human = 0;

    QString describe(const bridge::Meaning &m) const;
    QString suitSymbol(int strain) const;
    void afterAction();
    void botStep();
    void recordResult();

    // Whether a bot chooses for `seat` here: online, only the driver's.
    bool botPlays(int seat) const;
    // Makes a move: offline at once, online once it comes back from the server.
    bool perform(const QVariantMap &action);
    // Makes a move from the log, or one of your own offline; false if it does not fit.
    bool applyAction(const QVariantMap &action);
    // Starts the game over from the moves in the log.
    void replay();
    void tableChanged();
    void sendNew();
    // A new game with the settings, as a move.
    QVariantMap newAction();
    QVariantList shuffledDeck();

    bridge::Game m_game;
    // Plays all the seats you do not; it only knows what the seat may see.
    bridge::Bot m_bot;
    Session *m_online;
    // The moves of the game against the bots.
    cards::GameLog m_log;
    bool m_wasActive = false;
    // Deals for the online games.
    std::mt19937 m_rng{std::random_device{}()};
    QSettings m_settings;
    QTimer m_botTimer;
    int m_difficulty = 1;
    int m_scoring = 0;
    int m_system = 0;
    int m_theirSystem = 0;
    int m_speed = 1;
    bool m_tapTwice = false;
    bool m_started = false;
    bool m_recorded = false;
    int m_countedDeals = 0; // deals already counted for the statistics
};
