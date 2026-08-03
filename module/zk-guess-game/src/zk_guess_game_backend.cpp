#include "zk_guess_game_backend.h"
#include "station_crypto.h"

#include <QTimer>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QDateTime>
#include <QByteArray>
#include <QCryptographicHash>

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
    // Robust decode (receiver's proven path): the payload may arrive base64 OR raw JSON — try both.
    QByteArray json;
    const QString asStr = payload.toString();
    if (!asStr.isEmpty()) {
        const QByteArray b = QByteArray::fromBase64(asStr.toUtf8());
        if (!b.isEmpty() && b.trimmed().startsWith('{')) json = b;
        else if (asStr.trimmed().startsWith('{'))        json = asStr.toUtf8();
    }
    if (json.isEmpty()) {
        const QByteArray raw = payload.toByteArray();
        if (raw.trimmed().startsWith('{')) json = raw;
    }
    if (json.isEmpty()) return;

    // encrypted announce → decrypt with the room key (AAD = topic segment); fail closed.
    if (StationCrypto::isEnvelope(QString::fromUtf8(json))) {
        QByteArray plain;
        if (!StationCrypto::decryptAnnounce(m_key, QString::fromUtf8(json), m_seg, plain)) return;
        json = plain;
    }
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    const QString t = o.value("t").toString();
    const QString id = o.value("id").toString();
    if (id.isEmpty() || id == m_myId) return;   // ignore self-echo

    if (t == QLatin1String("presence")) {
        m_players[id] = { o.value("name").toString(), o.value("role").toString(), nowMs() };
        if (!isCreator() && roomName().isEmpty()) {           // joiner learns the room name from the host
            const QString rn = o.value("room").toString();
            if (!rn.isEmpty()) setRoomName(rn);
        }
        qDebug() << "zk_guess_game: presence from" << id << o.value("name").toString() << "roster now" << m_players.size();
        publishRoster();
    } else if (t == QLatin1String("chat")) {
        QJsonObject m{{"id",id},{"name",o.value("name").toString()},
                      {"text",o.value("text").toString()},{"ts",o.value("ts")}};
        m_chat.append(m);
        setChatJson(QString::fromUtf8(QJsonDocument(m_chat).toJson(QJsonDocument::Compact)));
    } else if (t == QLatin1String("seal")) {
        setSealedCommitment(o.value("commitment").toString());
        setStarted(true);
        log(QStringLiteral("host sealed a number — start guessing"));
    } else if (t == QLatin1String("guess")) {
        if (isCreator()) {   // only the host holds the secret → only the host answers
            const int g = o.value("guess").toInt();
            const QString name = o.value("name").toString();
            const int dir = (quint64(g) < m_secret) ? 0 : (quint64(g) == m_secret ? 1 : 2);
            QJsonObject v{{"t","verdict"},{"id",m_myId},{"guess",g},{"dir",dir},{"name",name}};
            if (dir == 1) {   // exact — reveal so everyone can verify the seal
                v.insert("winner", name);
                v.insert("secret", int(m_secret));
                v.insert("blind",  int(m_blind));
            }
            sendEnvelope(v);
            addTurn(g, dir, name);                 // host records locally (won't get its own echo)
            if (dir == 1) { setWon(true); setWinnerName(name); setSecretRevealed(int(m_secret)); }
        }
    } else if (t == QLatin1String("verdict")) {
        const int g = o.value("guess").toInt();
        const int dir = o.value("dir").toInt();
        addTurn(g, dir, o.value("name").toString());
        if (dir == 1) {
            const quint64 s = quint64(o.value("secret").toInt());
            const quint64 b = quint64(o.value("blind").toInt());
            // provably-fair win: the revealed number must hash to the commitment sealed at start.
            if (commitHex(s, b) == sealedCommitment()) {
                setWon(true);
                setWinnerName(o.value("winner").toString());
                setSecretRevealed(int(s));
            } else {
                log(QStringLiteral("reveal did NOT match the seal — rejected"));
            }
        }
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

// SHA256(secret_le ‖ blind_le) — matches the zk-guess guest/CLI commitment exactly.
static QString commitHex(quint64 secret, quint64 blind)
{
    QByteArray pre;
    for (int i = 0; i < 8; ++i) pre.append(char((secret >> (8 * i)) & 0xFF));
    for (int i = 0; i < 8; ++i) pre.append(char((blind  >> (8 * i)) & 0xFF));
    return QString::fromLatin1(QCryptographicHash::hash(pre, QCryptographicHash::Sha256).toHex());
}

QString ZkGuessGameBackend::startGame()
{
    if (!isCreator()) return QStringLiteral("only the host can start");
    if (m_players.size() < 2) return QStringLiteral("need at least 2 players");
    // seal a number the host holds; broadcast only the commitment.
    m_secret = QRandomGenerator::global()->bounded(1000001u);
    m_blind  = QRandomGenerator::global()->bounded(uint(0x7FFFFFFF));   // fits a JS number → win is client-verifiable
    const QString c = commitHex(m_secret, m_blind);
    setSealedCommitment(c);
    setStarted(true);
    sendEnvelope(QJsonObject{{"t","seal"},{"id",m_myId},{"commitment",c}});
    log(QStringLiteral("sealed a number; game started"));
    return QString();
}

QString ZkGuessGameBackend::submitGuess(int guess)
{
    if (!started()) return QStringLiteral("game not started");
    if (won())      return QStringLiteral("game over");
    if (isCreator()) return QStringLiteral("the host can't guess (they sealed it)");
    if (guess < 0 || guess > 1000000) return QStringLiteral("enter 0–1,000,000");
    sendEnvelope(QJsonObject{{"t","guess"},{"id",m_myId},{"name",m_display},{"guess",guess}});
    return QString();
}

void ZkGuessGameBackend::addTurn(int guess, int dir, const QString& byName)
{
    m_turns.append(QJsonObject{{"name",byName},{"guess",guess},{"dir",dir}});
    setTurnsJson(QString::fromUtf8(QJsonDocument(m_turns).toJson(QJsonDocument::Compact)));
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
