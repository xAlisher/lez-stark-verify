#ifndef ZK_GUESS_GAME_BACKEND_H
#define ZK_GUESS_GAME_BACKEND_H

#include <QString>
#include <QHash>
#include <QByteArray>
#include <QJsonArray>

#include "rep_zk_guess_game_source.h"     // ZkGuessGameSimpleSource (repc from src/zk_guess_game.rep)
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
    QString leaveRoom() override;

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

    struct Player { QString name; QString role; qint64 lastSeenMs = 0; };
    QHash<QString, Player> m_players;   // keyed by player id
    QJsonArray m_chat;                  // [{id,name,text,ts}]

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
