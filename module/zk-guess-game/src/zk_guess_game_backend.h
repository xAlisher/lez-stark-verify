#ifndef ZK_GUESS_GAME_BACKEND_H
#define ZK_GUESS_GAME_BACKEND_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QByteArray>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <functional>

#include "rep_zk_guess_game_source.h"     // ZkGuessGameSimpleSource (repc from src/zk_guess_game.rep)
#include "logos_sdk.h"                    // LogosModules (full def) + typed modules().delivery_module
#include "logos_types.h"                  // LogosResult, Timeout
#include "logos_ui_plugin_context.h"      // LogosUiPluginContext: modules() + onContextReady()

class QTimer;

// Universal ui_qml backend (EPIC A) — a game room over the delivery_module.
// Room topic is derived from an invite CODE (StationCrypto), presence is announce/heartbeat/TTL.
// Delivery calls use the *Async variants (sync createNode deadlocks the single ui-host thread, receiver #20).
class ZkGuessGameBackend : public ZkGuessGameSimpleSource,
                           public LogosUiPluginContext
{
public:
    explicit ZkGuessGameBackend(QObject* parent = nullptr);
    ~ZkGuessGameBackend() override;

    QString createRoom(QString roomName, QString displayName) override;
    QString joinRoom(QString code, QString displayName) override;
    QString sendChat(QString text) override;
    QString startGame() override;
    QString submitEntropy(QString contribution) override;
    QString submitGuess(int guess) override;
    QString settleOnLez() override;
    QString leaveRoom() override;
    // ── TOK pot (EPIC D) ──
    QString setBet(int amount) override;      // host, pre-start: set the room's stake size (0 = free)
    QString fundOnZone() override;            // any player: create + faucet-fund my on-zone pot account
    QString placeBet() override;              // player: stake betAmount into the pot
    QString refundOnLez() override;           // reclaim my stake if the game abandoned

protected:
    void onContextReady() override;

private:
    void enterRoom(const QString& code, const QString& displayName, bool creator, const QString& roomName);
    void bringUpNodeThenJoin();
    void wireEvents();
    void subscribe(const QString& topic);
    void announcePresence();
    void sendEnvelope(const QJsonObject& obj);       // encrypt + send on the room topic
    void ingest(const QVariant& payload);            // decode base64 → decrypt → dispatch by t:
    void publishRoster();
    void pruneRoster();
    void log(const QString& line);

    void addTurn(int guess, int dir, const QString& byName, bool proven);   // append + narrow range + win
    void proveGuess(int guess, const QString& byName);         // host: zk-verify prove-turn → verify → broadcast
    void broadcastVerdict(int guess, const QString& byName, int dir, bool proven);
    QString zkVerifyBin() const;
    void sealFromEntropy();                                     // host: fold all contributions → seal the number
    void advanceTurn();                                         // host: move to the next player + broadcast

    // ── TOK pot helpers (EPIC D) — each on-zone action shells out to the bundled `zkg_pot` binary ──
    QString potBinary() const;                                  // ZKG_POT_BIN or bundled "zkg_pot" beside plugin
    QString potHome() const;                                    // persistent pot-wallet dir (survives across turns)
    void    launchPot(const QString& action, const QHash<QString,QString>& env,
                      std::function<void(int, const QString&)> cb);   // build base env + run + parse output
    void    initPotIfBetting();                                 // host: after seal, create the on-zone pot PDA
    void    recomputePot();                                     // host: potTotal = Σ observed stakes
    QString settlePotOnLez();                                   // host: 3-way split settle via zkg_pot

    quint64               m_hostSeed = 0;      // host's own committed entropy (kept secret)
    QHash<QString,QString> m_contribs;         // player id → mouse-draw contribution
    QStringList           m_turnOrder;         // ordered non-host player ids (host-authoritative)
    int                   m_turnIdx = -1;

    struct Player { QString name; QString role; qint64 lastSeenMs = 0; QString onZoneAddr; };
    QHash<QString, Player> m_players;   // keyed by player id
    QJsonArray m_chat;                  // [{id,name,text,ts}]
    QJsonArray m_turns;                 // [{name,guess,dir}]

    // game state (host holds the secret; players only ever see the commitment + verdicts)
    quint64 m_secret = 0;
    quint64 m_blind  = 0;
    int     m_lo = 0;
    int     m_hi = 1000000;

    // ── TOK pot state (EPIC D) ──
    int     m_bet = 0;                 // room stake size in TOK; 0 = free game (no pot)
    QString m_onZoneAddr;              // my on-zone pot account (from fundOnZone)
    QString m_gameId;                  // host's on-zone game account (needed to stake / settle)
    bool    m_potInitStarted = false;  // host: init_pot already launched
    bool    m_recordedWin = false;     // winner: record-win already launched
    QString m_winnerAddr;              // host: the winner's on-zone payout address (from "winbound")
    QHash<QString,int> m_stakes;       // onZoneAddr → staked amount (host-side pot tally)

    QString    m_myId;
    QString    m_display;
    QString    m_topic;
    QByteArray m_key;
    QString    m_seg;

    QTimer* m_heartbeat = nullptr;
    QTimer* m_prune     = nullptr;
    bool    m_eventsWired = false;
    bool    m_nodeUp      = false;
};

#endif // ZK_GUESS_GAME_BACKEND_H
