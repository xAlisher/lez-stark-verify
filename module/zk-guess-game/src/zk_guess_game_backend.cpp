#include "zk_guess_game_backend.h"
#include "station_crypto.h"

#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QDateTime>
#include <QByteArray>

namespace {
constexpr int HEARTBEAT_MS = 15000;
constexpr int PRUNE_MS     = 5000;
constexpr qint64 TTL_MS    = 45000;
const QString ROOM_TITLE   = QStringLiteral("zk-guess/room/v1");   // fixed title; the CODE is the pass

QString randomId() {
    return QString::number(QRandomGenerator::global()->generate64(), 16).right(12);
}
QString randomCode() {
    static const char* A = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    QString c;
    for (int i = 0; i < 6; ++i) c.append(QChar(A[QRandomGenerator::global()->bounded(31)]));
    return c;
}
qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }
}

ZkGuessGameBackend::ZkGuessGameBackend(QObject* parent)
    : ZkGuessGameSimpleSource(parent)
{
    StationCrypto::init();   // sodium_init (idempotent) before any KDF/AEAD
    m_myId = randomId();
    setMyId(m_myId);
}

ZkGuessGameBackend::~ZkGuessGameBackend() = default;

void ZkGuessGameBackend::onContextReady()
{
    // nothing until the user creates or joins a room
    log(QStringLiteral("ready"));
}

void ZkGuessGameBackend::log(const QString& line) { emit activity(line); }

QString ZkGuessGameBackend::createRoom(QString roomName, QString displayName)
{
    if (!isContextReady()) return QStringLiteral("context_not_ready");
    if (inRoom()) return QStringLiteral("already in a room");
    const QString code = randomCode();
    setIsCreator(true);
    setRoomName(roomName.trimmed().isEmpty() ? QStringLiteral("ZK Guess") : roomName.trimmed());
    enterRoom(code, displayName, true, roomName);
    return QString();
}

QString ZkGuessGameBackend::joinRoom(QString code, QString displayName)
{
    if (!isContextReady()) return QStringLiteral("context_not_ready");
    if (inRoom()) return QStringLiteral("already in a room");
    code = code.trimmed().toUpper();
    if (code.isEmpty()) return QStringLiteral("enter a room code");
    setIsCreator(false);
    enterRoom(code, displayName, false, QString());
    return QString();
}

void ZkGuessGameBackend::enterRoom(const QString& code, const QString& displayName, bool creator, const QString& roomName)
{
    Q_UNUSED(roomName)
    m_display = displayName.trimmed().isEmpty() ? (creator ? QStringLiteral("host") : QStringLiteral("player")) : displayName.trimmed();
    m_topic   = StationCrypto::deriveTopic(ROOM_TITLE, code);
    m_seg     = StationCrypto::deriveTopicSegment(ROOM_TITLE, code);
    m_key     = StationCrypto::deriveKey(ROOM_TITLE, code);
    setRoomCode(code);
    setInRoom(true);
    // seed the roster with myself
    m_players[m_myId] = { m_display, creator ? QStringLiteral("creator") : QStringLiteral("player"), nowMs() };
    publishRoster();
    log(QStringLiteral("%1 room %2 on topic %3").arg(creator ? "created" : "joined", code, m_seg));
    bringUpNodeThenJoin();
}

void ZkGuessGameBackend::bringUpNodeThenJoin()
{
    if (m_nodeUp) { subscribe(m_topic); wireEvents(); announcePresence(); return; }

    // logos.dev preset ships no bootstrap nodes → supply the entry multiaddrs explicitly (receiver #20).
    QJsonArray entry{
        QStringLiteral("/dns4/delivery-01.do-ams3.logos.dev.status.im/tcp/30303/p2p/16Uiu2HAmTUbnxLGT9JvV6mu9oPyDjqHK4Phs1VDJNUgESgNSkuby"),
        QStringLiteral("/dns4/delivery-02.do-ams3.logos.dev.status.im/tcp/30303/p2p/16Uiu2HAmMK7PYygBtKUQ8EHp7EfaD3bCEsJrkFooK8RQ2PVpJprH"),
        QStringLiteral("/dns4/delivery-01.gc-us-central1-a.logos.dev.status.im/tcp/30303/p2p/16Uiu2HAm4S1JYkuzDKLKQvwgAhZKs9otxXqt8SCGtB4hoJP1S397"),
        QStringLiteral("/dns4/delivery-02.gc-us-central1-a.logos.dev.status.im/tcp/30303/p2p/16Uiu2HAm8Y9kgBNtjxvCnf1X6gnZJW5EGE4UwwCL3CCm55TwqBiH")
    };
    QJsonObject cfg{{"logLevel","INFO"},{"mode","Core"},{"preset","logos.dev"},{"relay",true},{"entryNodes",entry}};
    const QString cfgJson = QString::fromUtf8(QJsonDocument(cfg).toJson(QJsonDocument::Compact));

    setConnectionStatus(QStringLiteral("connecting"));
    modules().delivery_module.createNodeAsync(cfgJson, [](LogosResult){}, Timeout());   // fire-and-forget (#20)
    setNodeReady(true);
    m_nodeUp = true;
    QTimer::singleShot(3000, this, [this]{
        modules().delivery_module.startAsync([](LogosResult){}, Timeout());
        subscribe(m_topic);
        wireEvents();
        announcePresence();
        setConnectionStatus(QStringLiteral("connected"));
        if (!m_heartbeat) {
            m_heartbeat = new QTimer(this);
            connect(m_heartbeat, &QTimer::timeout, this, [this]{ announcePresence(); });
            m_heartbeat->start(HEARTBEAT_MS);
        }
        if (!m_prune) {
            m_prune = new QTimer(this);
            connect(m_prune, &QTimer::timeout, this, [this]{ pruneRoster(); });
            m_prune->start(PRUNE_MS);
        }
    });
}

void ZkGuessGameBackend::subscribe(const QString& topic)
{
    modules().delivery_module.subscribeAsync(topic, [](LogosResult){}, Timeout());
}

void ZkGuessGameBackend::wireEvents()
{
    if (m_eventsWired) return;
    m_eventsWired = true;
    modules().delivery_module.on("messageReceived", [this](const QVariantList& d){
        if (d.size() > 2) ingest(d.at(2));
    });
    modules().delivery_module.on("connectionStateChanged", [this](const QVariantList& d){
        if (!d.isEmpty()) setConnectionStatus(d.at(0).toString().toLower());
    });
}

void ZkGuessGameBackend::sendEnvelope(const QJsonObject& obj)
{
    const QByteArray plain = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    const QString env = StationCrypto::encryptAnnounce(m_key, plain, m_seg);
    if (env.isEmpty()) return;
    modules().delivery_module.sendAsync(m_topic, env.toUtf8(), [](LogosResult){}, Timeout());
}

void ZkGuessGameBackend::announcePresence()
{
    if (m_topic.isEmpty()) return;
    QJsonObject o{{"t","presence"},{"id",m_myId},{"name",m_display},
                  {"role", isCreator() ? "creator" : "player"},{"room", roomName()}};
    sendEnvelope(o);
}

void ZkGuessGameBackend::ingest(const QVariant& payload)
{
    const QByteArray env = QByteArray::fromBase64(payload.toString().toUtf8());
    const QString envStr = QString::fromUtf8(env);
    if (!StationCrypto::isEnvelope(envStr)) return;
    QByteArray plain;
    if (!StationCrypto::decryptAnnounce(m_key, envStr, m_seg, plain)) return;
    const QJsonObject o = QJsonDocument::fromJson(plain).object();
    const QString t = o.value("t").toString();
    const QString id = o.value("id").toString();
    if (id.isEmpty() || id == m_myId) return;   // ignore self-echo

    if (t == QLatin1String("presence")) {
        m_players[id] = { o.value("name").toString(), o.value("role").toString(), nowMs() };
        if (isCreator() && roomName().isEmpty()) {} // creator keeps its own room name
        publishRoster();
    } else if (t == QLatin1String("chat")) {
        QJsonObject m{{"id",id},{"name",o.value("name").toString()},
                      {"text",o.value("text").toString()},{"ts",o.value("ts")}};
        m_chat.append(m);
        setChatJson(QString::fromUtf8(QJsonDocument(m_chat).toJson(QJsonDocument::Compact)));
    } else if (t == QLatin1String("start")) {
        setStarted(true);
        log(QStringLiteral("game started by the host"));
    }
}

void ZkGuessGameBackend::publishRoster()
{
    const qint64 now = nowMs();
    QJsonArray arr;
    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
        arr.append(QJsonObject{{"id",it.key()},{"name",it.value().name},
                               {"role",it.value().role},
                               {"online", (now - it.value().lastSeenMs) < TTL_MS}});
    }
    setRosterJson(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void ZkGuessGameBackend::pruneRoster()
{
    const qint64 now = nowMs();
    bool changed = false;
    for (auto it = m_players.begin(); it != m_players.end();) {
        if (it.key() != m_myId && (now - it.value().lastSeenMs) > (TTL_MS * 2)) { it = m_players.erase(it); changed = true; }
        else ++it;
    }
    // refresh my own liveness
    if (m_players.contains(m_myId)) m_players[m_myId].lastSeenMs = now;
    publishRoster();
    Q_UNUSED(changed)
}

QString ZkGuessGameBackend::sendChat(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) return QStringLiteral("empty");
    if (m_topic.isEmpty()) return QStringLiteral("not in a room");
    const qint64 ts = nowMs();
    QJsonObject o{{"t","chat"},{"id",m_myId},{"name",m_display},{"text",text},{"ts",ts}};
    sendEnvelope(o);
    // echo my own message locally
    m_chat.append(QJsonObject{{"id",m_myId},{"name",m_display},{"text",text},{"ts",ts}});
    setChatJson(QString::fromUtf8(QJsonDocument(m_chat).toJson(QJsonDocument::Compact)));
    return QString();
}

QString ZkGuessGameBackend::startGame()
{
    if (!isCreator()) return QStringLiteral("only the host can start");
    if (m_players.size() < 2) return QStringLiteral("need at least 2 players");
    sendEnvelope(QJsonObject{{"t","start"},{"id",m_myId}});
    setStarted(true);
    log(QStringLiteral("game started"));
    return QString();
}

QString ZkGuessGameBackend::leaveRoom()
{
    if (m_heartbeat) { m_heartbeat->stop(); }
    if (!m_topic.isEmpty())
        modules().delivery_module.unsubscribeAsync(m_topic, [](LogosResult){}, Timeout());
    m_topic.clear(); m_players.clear(); m_chat = QJsonArray();
    setInRoom(false); setStarted(false); setRoomCode(QString()); setRosterJson("[]"); setChatJson("[]");
    setConnectionStatus(QStringLiteral("idle"));
    return QString();
}
