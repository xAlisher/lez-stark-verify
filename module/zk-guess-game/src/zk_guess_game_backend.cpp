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
#include <QProcess>
#include <QProcessEnvironment>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include <dlfcn.h>   // dladdr — resolve the plugin's own .so dir so bundled tools sit beside it

namespace {

// Directory this plugin .so was loaded from — so a bundled zk-verify + r0vm sit right beside it
// (dladdr on a symbol in THIS translation unit → the .so path → its dir). Empty if unresolved.
QString pluginDir() {
    Dl_info info;
    if (dladdr(reinterpret_cast<const void*>(&pluginDir), &info) && info.dli_fname)
        return QFileInfo(QString::fromUtf8(info.dli_fname)).absolutePath();
    return QString();
}

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

// A deterministic 3-word handle from an id (BIP39-ish, small list — enough for a lobby name).
QString threeWords(const QString& id) {
    static const char* W[64] = {
        "amber","arc","ash","bark","blaze","bloom","brisk","calm","cedar","cliff","clove","coal",
        "cove","crisp","dawn","dune","ember","fern","flint","fox","frost","glade","grove","harbor",
        "haze","holt","iris","ivy","jade","kelp","lark","leaf","loam","mica","mist","moss",
        "north","oak","onyx","opal","pike","pine","quill","reef","rill","rook","sage","salt",
        "shale","slate","spark","spire","tarn","teal","thorn","tide","vale","vane","wave","wick",
        "wren","yarn","zephyr","zinc"
    };
    const QByteArray h = QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256);
    return QStringLiteral("%1-%2-%3")
        .arg(W[quint8(h.at(0)) & 63]).arg(W[quint8(h.at(1)) & 63]).arg(W[quint8(h.at(2)) & 63]);
}

// SHA256(secret_le ‖ blind_le) — matches the zk-guess guest/CLI commitment exactly.
QString commitHex(quint64 secret, quint64 blind) {
    QByteArray pre;
    for (int i = 0; i < 8; ++i) pre.append(char((secret >> (8 * i)) & 0xFF));
    for (int i = 0; i < 8; ++i) pre.append(char((blind  >> (8 * i)) & 0xFF));
    return QString::fromLatin1(QCryptographicHash::hash(pre, QCryptographicHash::Sha256).toHex());
}
}

ZkGuessGameBackend::ZkGuessGameBackend(QObject* parent)
    : ZkGuessGameSimpleSource(parent)
{
    StationCrypto::init();   // sodium_init (idempotent) before any KDF/AEAD
    m_myId = randomId();
    setMyId(m_myId);
    setSuggestedName(threeWords(m_myId));   // deterministic 3-word handle from my id
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
            connect(m_prune, &QTimer::timeout, this, [this]{
                pruneRoster();
                // Turn broadcasts go over best-effort Waku — a single dropped "turn" leaves that
                // player's currentTurnId stale forever (no slider, game stuck). The host re-asserts
                // the current turn every tick; clients already in sync no-op (change-guarded setter),
                // a client that missed it recovers within PRUNE_MS.
                if (isCreator() && started() && !won() && !currentTurnId().isEmpty())
                    sendEnvelope(QJsonObject{{"t","turn"},{"id",m_myId},
                                 {"turnId",currentTurnId()},{"turnName",currentTurnName()}});
            });
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
    } else if (t == QLatin1String("entropy_request")) {
        setEntropySubmitted(false);
        setCollectingEntropy(true);
        log(QStringLiteral("host wants entropy — draw to stir the number"));
    } else if (t == QLatin1String("entropy")) {
        if (isCreator()) {              // host collects; seal once every player has drawn
            m_contribs.insert(id, o.value("contrib").toString());
            int expected = 0;
            for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it)
                if (it.value().role != QLatin1String("creator")) ++expected;
            if (expected > 0 && m_contribs.size() >= expected) sealFromEntropy();
        }
    } else if (t == QLatin1String("seal")) {
        setSealedCommitment(o.value("commitment").toString());
        setCollectingEntropy(false);
        setStarted(true);
        log(QStringLiteral("host sealed a number — start guessing"));
    } else if (t == QLatin1String("guess")) {
        const int g = o.value("guess").toInt();
        bool resolved = false;   // Waku reorders/duplicates — ignore a guess whose verdict already landed
        for (const auto& v : m_turns) if (v.toObject().value("guess").toInt() == g) { resolved = true; break; }
        if (!resolved) {
            setProvingName(o.value("name").toString());   // all clients: show the per-turn proving spinner
            setProvingGuess(g);
            if (isCreator()) proveGuess(g, o.value("name").toString());   // host holds the secret → proves
        }
    } else if (t == QLatin1String("verdict")) {
        const int g = o.value("guess").toInt();
        const int dir = o.value("dir").toInt();
        addTurn(g, dir, o.value("name").toString(), o.value("proven").toBool());
        if (dir == 1) {
            const quint64 s = quint64(o.value("secret").toInt());
            const quint64 b = quint64(o.value("blind").toInt());
            // provably-fair win: the revealed number must hash to the commitment sealed at start.
            if (commitHex(s, b) == sealedCommitment()) {
                setWon(true);
                setWinnerName(o.value("winner").toString());
                setSecretRevealed(int(s));
                // pot: if I'm the winner, I now hold the reveal → bind myself on-zone (Tier 2),
                // then broadcast "winbound" so the host can settle the pot to me.
                if (m_bet > 0 && !m_gameId.isEmpty() && !m_recordedWin
                        && o.value("winner").toString() == m_display) {
                    m_secret = s; m_blind = b;   // store the revealed number so record-win can prove it
                    m_recordedWin = true;
                    launchPot(QStringLiteral("record-win"),
                              { {QStringLiteral("ZKG_GAME_ID"), m_gameId},
                                {QStringLiteral("ZKG_SECRET"), QString::number(s)},
                                {QStringLiteral("ZKG_BLIND"),  QString::number(b)} },
                              [this](int code, const QString&){
                        if (code == 0) sendEnvelope(QJsonObject{{"t","winbound"},{"id",m_myId},{"addr",m_onZoneAddr}});
                        else { m_recordedWin = false; setLastError(QStringLiteral("record-win failed")); }
                    });
                }
            } else {
                log(QStringLiteral("reveal did NOT match the seal — rejected"));
            }
        }
    } else if (t == QLatin1String("turn")) {
        const QString newTid = o.value("turnId").toString();
        if (!newTid.isEmpty() && newTid != currentTurnId()) {   // a REAL advance (not a 5s re-broadcast)
            setProvingName(QString()); setProvingGuess(-1);      // prior turn's proof is done → clear spinner
        }
        setCurrentTurnId(newTid);
        setCurrentTurnName(o.value("turnName").toString());
    } else if (t == QLatin1String("bet")) {                       // host announced the room stake
        if (!isCreator()) { m_bet = o.value("amount").toInt(); setBetAmount(m_bet); }
    } else if (t == QLatin1String("zaddr")) {                     // a player's on-zone payout address
        if (m_players.contains(id)) m_players[id].onZoneAddr = o.value("addr").toString();
    } else if (t == QLatin1String("potready")) {                  // host created the on-zone pot
        if (!isCreator()) {
            m_gameId = o.value("game").toString();
            setGameId(m_gameId); setPotReady(true);
            log(QStringLiteral("pot is open on-zone — place your bet"));
        }
    } else if (t == QLatin1String("staked")) {                    // a player staked → host tallies the pot
        if (isCreator()) { m_stakes.insert(o.value("addr").toString(), o.value("amount").toInt()); recomputePot(); }
    } else if (t == QLatin1String("winbound")) {                  // winner bound itself → host can settle
        if (isCreator()) { m_winnerAddr = o.value("addr").toString();
                           log(QStringLiteral("winner bound on-zone — press settle to pay the pot")); }
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
    // open the entropy phase: everyone draws before the number is sealed, so no
    // single player picks it. The host adds its own committed seed (kept secret).
    m_hostSeed = QRandomGenerator::global()->generate64();
    m_contribs.clear();
    setCollectingEntropy(true);
    sendEnvelope(QJsonObject{{"t","entropy_request"},{"id",m_myId}});
    log(QStringLiteral("collecting entropy — everyone draw"));
    return QString();
}

QString ZkGuessGameBackend::submitEntropy(QString contribution)
{
    if (!collectingEntropy()) return QStringLiteral("not collecting entropy");
    if (entropySubmitted())  return QStringLiteral("already submitted");
    setEntropySubmitted(true);
    sendEnvelope(QJsonObject{{"t","entropy"},{"id",m_myId},{"name",m_display},{"contrib",contribution}});
    return QString();
}

// host: fold host seed + every player's mouse-draw into the sealed number.
void ZkGuessGameBackend::sealFromEntropy()
{
    QByteArray pre;
    for (int i = 0; i < 8; ++i) pre.append(char((m_hostSeed >> (8 * i)) & 0xFF));
    QList<QString> keys = m_contribs.keys();
    keys.sort();
    for (const QString& k : keys) pre.append(m_contribs.value(k).toUtf8());
    const QByteArray h = QCryptographicHash::hash(pre, QCryptographicHash::Sha256);
    quint64 v = 0;
    for (int i = 0; i < 8; ++i) v |= (quint64(quint8(h.at(i))) << (8 * i));
    m_secret = v % 1000001;
    m_blind  = QRandomGenerator::global()->bounded(uint(0x7FFFFFFF));
    const QString c = commitHex(m_secret, m_blind);
    setSealedCommitment(c);
    setCollectingEntropy(false);
    setStarted(true);
    sendEnvelope(QJsonObject{{"t","seal"},{"id",m_myId},{"commitment",c}});

    // establish a deterministic turn order over the non-host players + open the first turn.
    QList<QString> ids;
    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it)
        if (it.value().role != QLatin1String("creator")) ids.append(it.key());
    ids.sort();
    m_turnOrder = ids;
    m_turnIdx = 0;
    if (!m_turnOrder.isEmpty()) {
        const QString tid = m_turnOrder.at(0);
        setCurrentTurnId(tid);
        setCurrentTurnName(m_players.value(tid).name);
        sendEnvelope(QJsonObject{{"t","turn"},{"id",m_myId},{"turnId",tid},{"turnName",m_players.value(tid).name}});
    }
    log(QStringLiteral("sealed from everyone's entropy — start guessing"));
    initPotIfBetting();   // host: if this room has a stake, create the on-zone pot now (needs the commitment)
}

void ZkGuessGameBackend::advanceTurn()
{
    if (m_turnOrder.isEmpty()) return;
    m_turnIdx = (m_turnIdx + 1) % m_turnOrder.size();
    const QString tid = m_turnOrder.at(m_turnIdx);
    setCurrentTurnId(tid);
    setCurrentTurnName(m_players.value(tid).name);
    sendEnvelope(QJsonObject{{"t","turn"},{"id",m_myId},{"turnId",tid},{"turnName",m_players.value(tid).name}});
}

QString ZkGuessGameBackend::submitGuess(int guess)
{
    if (!started()) return QStringLiteral("game not started");
    if (won())      return QStringLiteral("game over");
    if (isCreator()) return QStringLiteral("the host can't guess (they sealed it)");
    if (currentTurnId() != m_myId) return QStringLiteral("not your turn");
    if (guess < 0 || guess > 1000000) return QStringLiteral("enter 0–1,000,000");
    sendEnvelope(QJsonObject{{"t","guess"},{"id",m_myId},{"name",m_display},{"guess",guess}});
    setProvingName(m_display); setProvingGuess(guess);   // show my own proving spinner (no self-echo)
    return QString();
}

void ZkGuessGameBackend::addTurn(int guess, int dir, const QString& byName, bool proven)
{
    m_turns.append(QJsonObject{{"name",byName},{"guess",guess},{"dir",dir},{"proven",proven},
                               {"ts",double(nowMs())},{"kind","turn"}});
    setTurnsJson(QString::fromUtf8(QJsonDocument(m_turns).toJson(QJsonDocument::Compact)));
    setProvingName(QString()); setProvingGuess(-1);   // verdict resolved → clear the proving spinner
}

QString ZkGuessGameBackend::zkVerifyBin() const
{
    // 1) explicit override, 2) bundled beside the plugin (.lgx ships zk-verify + r0vm), 3) dev install.
    const QString env = qEnvironmentVariable("ZK_VERIFY_BIN");
    if (!env.isEmpty() && QFileInfo::exists(env)) return env;
    const QString bundled = pluginDir() + QStringLiteral("/zk-verify");
    if (!pluginDir().isEmpty() && QFileInfo::exists(bundled)) return bundled;
    return QDir::homePath() + QStringLiteral("/.local/share/zk-guess/zk-verify");
}

void ZkGuessGameBackend::broadcastVerdict(int guess, const QString& byName, int dir, bool proven)
{
    QJsonObject v{{"t","verdict"},{"id",m_myId},{"guess",guess},{"dir",dir},{"name",byName},{"proven",proven}};
    if (dir == 1) { v.insert("winner", byName); v.insert("secret", int(m_secret)); v.insert("blind", int(m_blind)); }
    sendEnvelope(v);
    addTurn(guess, dir, byName, proven);          // host records locally (no self-echo)
    if (dir == 1) { setWon(true); setWinnerName(byName); setSecretRevealed(int(m_secret)); }
    else          { advanceTurn(); }              // pass the turn to the next player
}

// Host proves a turn on the real zk-verify STARK engine, then broadcasts the verdict.
// Async QProcess chain (prove-turn → verify) so the ui-host never blocks. Dev-mode for snappy play.
void ZkGuessGameBackend::proveGuess(int guess, const QString& byName)
{
    const QString bin = zkVerifyBin();
    if (!QFileInfo::exists(bin)) {   // no prover available → honest fallback: compute + mark unproven
        const int dir = (quint64(guess) < m_secret) ? 0 : (quint64(guess) == m_secret ? 1 : 2);
        broadcastVerdict(guess, byName, dir, false);
        return;
    }
    const QString out = QDir::tempPath() + QStringLiteral("/zkg-%1.receipt").arg(nowMs());
    QProcessEnvironment penv = QProcessEnvironment::systemEnvironment();
    penv.insert(QStringLiteral("RISC0_DEV_MODE"), QStringLiteral("1"));
    // zk-verify spawns r0vm as its prover server — point it at the r0vm bundled beside zk-verify
    // (so proving works out of the box on a catalog install, no rzup/PATH r0vm needed).
    const QString r0vm = QFileInfo(bin).absolutePath() + QStringLiteral("/r0vm");
    if (QFileInfo::exists(r0vm)) penv.insert(QStringLiteral("RISC0_SERVER_PATH"), r0vm);

    auto* prove = new QProcess(this);
    prove->setProcessEnvironment(penv);
    connect(prove, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, prove, guess, byName, out, bin, penv](int code, QProcess::ExitStatus) {
        prove->deleteLater();
        if (code != 0) {   // prove failed (e.g. real-mode assert) → fallback
            const int dir = (quint64(guess) < m_secret) ? 0 : (quint64(guess) == m_secret ? 1 : 2);
            broadcastVerdict(guess, byName, dir, false);
            return;
        }
        auto* ver = new QProcess(this);
        ver->setProcessEnvironment(penv);
        connect(ver, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, ver, guess, byName, out](int, QProcess::ExitStatus) {
            const QByteArray o = ver->readAllStandardOutput();
            ver->deleteLater();
            QFile::remove(out);
            const QJsonObject j = QJsonDocument::fromJson(o).object();
            const bool valid = j.value(QStringLiteral("valid")).toBool();
            const int  dir   = j.value(QStringLiteral("dir")).toInt(-1);
            if (dir >= 0) broadcastVerdict(guess, byName, dir, valid);
        });
        ver->start(bin, {QStringLiteral("verify"), out});
    });
    prove->start(bin, {QStringLiteral("prove-turn"),
                       QString::number(m_secret), QString::number(m_blind),
                       QString::number(guess), out});
}

// Winner settles the win on-zone with a REAL STARK (the ~16min proof) against our public
// sequencer — non-blocking: the win screen already shows the winner; this runs in the background
// and lands a real block. Config comes from ENV (never shipped in the module): NSSA_SEQUENCER_URL
// (default sequencer.logos.live), SEQ_BASIC_AUTH, and a settle binary (env SETTLE_BIN or bundled
// beside the plugin as "settle-win"). r0vm = the bundled sibling. RISC0_DEV_MODE is cleared → real.
QString ZkGuessGameBackend::settleOnLez()
{
    if (!won())      return QStringLiteral("nothing to settle yet");
    if (settling())  return QStringLiteral("already settling");
    if (m_bet > 0 && !m_gameId.isEmpty()) return settlePotOnLez();  // pot game → 3-way split payout

    QString bin = qEnvironmentVariable("SETTLE_BIN");
    if (bin.isEmpty() || !QFileInfo::exists(bin)) {
        const QString bundled = pluginDir() + QStringLiteral("/settle-win");
        if (!pluginDir().isEmpty() && QFileInfo::exists(bundled)) bin = bundled;
    }
    if (bin.isEmpty() || !QFileInfo::exists(bin)) {
        setSettleError(QStringLiteral("on-zone settlement not configured (no settle binary)"));
        return QStringLiteral("settlement not configured");
    }

    setSettleError(QString()); setSettleBlock(-1);
    setSettling(true); setSettleStartMs(double(nowMs()));

    const QString home = QDir::tempPath() + QStringLiteral("/zkg-settle-%1").arg(nowMs());
    QDir().mkpath(home);
    QProcessEnvironment penv = QProcessEnvironment::systemEnvironment();
    penv.remove(QStringLiteral("RISC0_DEV_MODE"));                 // REAL mode for on-zone settlement
    penv.insert(QStringLiteral("LEE_WALLET_HOME_DIR"), home);
    const QString r0vm = QFileInfo(bin).absolutePath() + QStringLiteral("/r0vm");
    if (QFileInfo::exists(r0vm)) penv.insert(QStringLiteral("RISC0_SERVER_PATH"), r0vm);
    if (!penv.contains(QStringLiteral("NSSA_SEQUENCER_URL")))
        penv.insert(QStringLiteral("NSSA_SEQUENCER_URL"), QStringLiteral("https://sequencer.logos.live"));
    // Hand the real game's win to settle-win: it re-seals the commitment and submits the winning
    // guess (== the sealed number → proves EQUAL) on-zone. sequencer.logos.live is un-gated, so no
    // SEQ_BASIC_AUTH needed; if the sequencer is re-gated, set SEQ_BASIC_AUTH in the launch env.
    penv.insert(QStringLiteral("ZKG_SECRET"), QString::number(m_secret));
    penv.insert(QStringLiteral("ZKG_BLIND"),  QString::number(m_blind));
    penv.insert(QStringLiteral("ZKG_GUESS"),  QString::number(m_secret));  // winning guess = the sealed number

    // Cap the prover — ZK proving is embarrassingly parallel and will grab EVERY core (r0vm hit
    // ~1400% CPU / load 18). RAYON_NUM_THREADS bounds r0vm's rayon pool; default to HALF the cores so
    // the machine stays responsive during the ~16 min proof. Override with SETTLE_THREADS (RAM peak is
    // inherent ~9.6 GB and driven by segment size, not thread count — fewer threads mainly tames CPU).
    const int cores = qMax(1, QThread::idealThreadCount());
    QString threads = qEnvironmentVariable("SETTLE_THREADS");
    if (threads.isEmpty()) threads = QString::number(qMax(2, cores / 2));
    penv.insert(QStringLiteral("RAYON_NUM_THREADS"), threads);

    auto* p = new QProcess(this);
    p->setProcessEnvironment(penv);
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, p](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(p->readAll());
        p->deleteLater();
        setSettling(false);
        QRegularExpression reBlk(QStringLiteral("block\\s+(\\d+)"));       // last "block N" = inclusion block
        int blk = -1; auto itB = reBlk.globalMatch(out);
        while (itB.hasNext()) blk = itB.next().captured(1).toInt();
        QRegularExpression reTx(QStringLiteral("tx\\s+([0-9a-fA-F]{32,})")); // last "tx <hash>" = the settle tx
        QString tx; auto itT = reTx.globalMatch(out);
        while (itT.hasNext()) tx = itT.next().captured(1);
        if (code == 0 && blk >= 0) { setSettleBlock(blk); setSettleTx(tx); }   // tx = the on-zone proof
        else setSettleError(code == 0 ? QStringLiteral("settled but no block parsed")
                                      : QStringLiteral("settlement failed (exit %1)").arg(code));
    });
    // run at lower priority (nice) so the capped prover threads yield to interactive work
    p->start(QStringLiteral("nice"), {QStringLiteral("-n"), QStringLiteral("15"), bin});
    return QString();
}

// ── TOK pot (EPIC D) — each on-zone step shells out to the bundled `zkg_pot` binary ─────────────
QString ZkGuessGameBackend::potBinary() const
{
    const QString env = qEnvironmentVariable("ZKG_POT_BIN");
    if (!env.isEmpty() && QFileInfo::exists(env)) return env;
    const QString bundled = pluginDir() + QStringLiteral("/zkg_pot");
    if (!pluginDir().isEmpty() && QFileInfo::exists(bundled)) return bundled;
    return QString();
}

QString ZkGuessGameBackend::potHome() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::tempPath();
    const QString h = base + QStringLiteral("/zk-guess/pot-wallet");
    QDir().mkpath(h);
    return h;
}

void ZkGuessGameBackend::recomputePot()
{
    int sum = 0;
    for (auto it = m_stakes.constBegin(); it != m_stakes.constEnd(); ++it) sum += it.value();
    setPotTotal(sum);
}

// Common env (persistent wallet, sequencer, bundled r0vm, shared room id, prover cap) + per-action
// ZKG_* extras → run `zkg_pot <action>` at low priority, hand the merged output back on finish.
// Real mode by default; set ZKG_POT_DEV=1 in the launch env for the fast dev-mode sequencer.
void ZkGuessGameBackend::launchPot(const QString& action, const QHash<QString,QString>& extra,
                                   std::function<void(int, const QString&)> cb)
{
    const QString bin = potBinary();
    if (bin.isEmpty())        { setLastError(QStringLiteral("pot: zkg_pot binary not bundled")); cb(-1, QString()); return; }
    if (roomCode().isEmpty()) { setLastError(QStringLiteral("pot: no room id")); cb(-1, QString()); return; }

    QProcessEnvironment penv = QProcessEnvironment::systemEnvironment();
    if (qEnvironmentVariableIsSet("ZKG_POT_DEV")) penv.insert(QStringLiteral("RISC0_DEV_MODE"), QStringLiteral("1"));
    else                                          penv.remove(QStringLiteral("RISC0_DEV_MODE"));   // real mode
    penv.insert(QStringLiteral("LEE_WALLET_HOME_DIR"), potHome());
    const QString r0vm = QFileInfo(bin).absolutePath() + QStringLiteral("/r0vm");
    if (QFileInfo::exists(r0vm)) penv.insert(QStringLiteral("RISC0_SERVER_PATH"), r0vm);
    if (!penv.contains(QStringLiteral("NSSA_SEQUENCER_URL")))
        penv.insert(QStringLiteral("NSSA_SEQUENCER_URL"), QStringLiteral("https://sequencer.logos.live"));
    penv.insert(QStringLiteral("ZKG_ROOM_ID"), roomCode());   // shared invite code → same pot PDA for everyone
    const int cores = qMax(1, QThread::idealThreadCount());
    penv.insert(QStringLiteral("RAYON_NUM_THREADS"), QString::number(qMax(2, cores / 2)));
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) penv.insert(it.key(), it.value());

    auto* p = new QProcess(this);
    p->setProcessEnvironment(penv);
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [p, cb](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(p->readAll());
        p->deleteLater();
        cb(code, out);
    });
    p->start(QStringLiteral("nice"), {QStringLiteral("-n"), QStringLiteral("15"), bin, action});
}

// Host: after the number is sealed, create the on-zone pot PDA + commit host/deadline, then tell the room.
void ZkGuessGameBackend::initPotIfBetting()
{
    if (!isCreator() || m_bet <= 0 || m_potInitStarted) return;
    if (m_onZoneAddr.isEmpty()) { log(QStringLiteral("pot: host must fund on-zone first")); return; }
    m_potInitStarted = true;
    launchPot(QStringLiteral("init"),
              { {QStringLiteral("ZKG_SECRET"), QString::number(m_secret)},
                {QStringLiteral("ZKG_BLIND"),  QString::number(m_blind)},
                {QStringLiteral("ZKG_HOST_ADDR"), m_onZoneAddr},
                {QStringLiteral("ZKG_HOST_BPS"), QString::number(hostBps())} },
              [this](int code, const QString& out) {
        const auto m = QRegularExpression(QStringLiteral("game\\s+(\\S+)")).match(out);
        if (code == 0 && m.hasMatch()) {
            m_gameId = m.captured(1);
            setGameId(m_gameId); setPotReady(true);
            sendEnvelope(QJsonObject{{"t","potready"},{"id",m_myId},{"game",m_gameId}});
            log(QStringLiteral("pot open on-zone — everyone place your bets"));
        } else {
            m_potInitStarted = false;
            setLastError(QStringLiteral("pot init failed (exit %1)").arg(code));
        }
    });
}

// Host: pay the pot three ways (winner 92.5% / host 5% / builder 2.5%) in one atomic settle_win.
QString ZkGuessGameBackend::settlePotOnLez()
{
    if (!isCreator())           return QStringLiteral("the host settles the pot");
    if (m_winnerAddr.isEmpty()) return QStringLiteral("waiting for the winner to bind on-zone");
    if (m_onZoneAddr.isEmpty()) return QStringLiteral("host must fund on-zone first");
    setSettleError(QString()); setSettleBlock(-1);
    setSettling(true); setSettleStartMs(double(nowMs()));
    launchPot(QStringLiteral("settle"),
              { {QStringLiteral("ZKG_GAME_ID"), m_gameId},
                {QStringLiteral("ZKG_WINNER_ADDR"), m_winnerAddr},
                {QStringLiteral("ZKG_HOST_ADDR"), m_onZoneAddr} },
              [this](int code, const QString& out) {
        setSettling(false);
        int blk = -1; auto itB = QRegularExpression(QStringLiteral("block\\s+(\\d+)")).globalMatch(out);
        while (itB.hasNext()) blk = itB.next().captured(1).toInt();
        QString tx; auto itT = QRegularExpression(QStringLiteral("tx\\s+([0-9a-fA-F]{16,})")).globalMatch(out);
        while (itT.hasNext()) tx = itT.next().captured(1);
        if (code == 0 && blk >= 0) { setSettleBlock(blk); setSettleTx(tx); setPayoutTx(tx); }
        else setSettleError(QStringLiteral("pot settle failed (exit %1)").arg(code));
    });
    return QString();
}

QString ZkGuessGameBackend::setBet(int amount)
{
    if (!isCreator()) return QStringLiteral("only the host sets the stake");
    if (started())    return QStringLiteral("game already started");
    if (amount < 0)   amount = 0;
    m_bet = amount; setBetAmount(amount);
    sendEnvelope(QJsonObject{{"t","bet"},{"id",m_myId},{"amount",amount}});
    log(amount > 0 ? QStringLiteral("stake set to %1 TOK — fund + place your bet").arg(amount)
                   : QStringLiteral("free game (no stake)"));
    return QString();
}

QString ZkGuessGameBackend::fundOnZone()
{
    if (onZoneFunded()) return QStringLiteral("already funded");
    if (stakeState() == QLatin1String("funding")) return QStringLiteral("funding…");
    setStakeState(QStringLiteral("funding"));
    launchPot(QStringLiteral("fund"), {}, [this](int code, const QString& out) {
        const auto mA = QRegularExpression(QStringLiteral("addr\\s+(\\S+)")).match(out);
        const auto mB = QRegularExpression(QStringLiteral("balance\\s+(\\d+)")).match(out);
        if (code == 0 && mA.hasMatch()) {
            m_onZoneAddr = mA.captured(1);
            setMyOnZoneAddr(m_onZoneAddr); setOnZoneFunded(true);
            if (mB.hasMatch()) setMyBalance(mB.captured(1).toInt());
            setStakeState(QStringLiteral("none"));
            sendEnvelope(QJsonObject{{"t","zaddr"},{"id",m_myId},{"addr",m_onZoneAddr}});   // tell the room my payout addr
            if (isCreator()) initPotIfBetting();   // host funded after the seal → create the pot now
        } else {
            setStakeState(QStringLiteral("none"));
            setLastError(QStringLiteral("funding failed (exit %1)").arg(code));
        }
    });
    return QString();
}

QString ZkGuessGameBackend::placeBet()
{
    if (m_bet <= 0)                              return QStringLiteral("no stake in this room");
    if (!onZoneFunded())                         return QStringLiteral("fund your account first");
    if (!potReady() || m_gameId.isEmpty())       return QStringLiteral("pot not open yet");
    if (stakeState() == QLatin1String("staked")) return QStringLiteral("already staked");
    setStakeState(QStringLiteral("staking"));
    launchPot(QStringLiteral("stake"),
              { {QStringLiteral("ZKG_GAME_ID"), m_gameId}, {QStringLiteral("ZKG_BET"), QString::number(m_bet)} },
              [this](int code, const QString& out) {
        if (code == 0) {
            setStakeState(QStringLiteral("staked"));
            const auto m = QRegularExpression(QStringLiteral("tx\\s+([0-9a-fA-F]{16,})")).match(out);
            if (m.hasMatch()) setStakeTx(m.captured(1));
            sendEnvelope(QJsonObject{{"t","staked"},{"id",m_myId},{"addr",m_onZoneAddr},{"amount",m_bet}});
            if (!m_onZoneAddr.isEmpty()) { m_stakes.insert(m_onZoneAddr, m_bet); recomputePot(); }
        } else {
            setStakeState(QStringLiteral("none"));
            setLastError(QStringLiteral("stake failed (exit %1)").arg(code));
        }
    });
    return QString();
}

QString ZkGuessGameBackend::refundOnLez()
{
    if (stakeState() != QLatin1String("staked")) return QStringLiteral("nothing staked to refund");
    if (m_gameId.isEmpty())                      return QStringLiteral("no game to refund from");
    setStakeState(QStringLiteral("refunding"));
    launchPot(QStringLiteral("refund"), { {QStringLiteral("ZKG_GAME_ID"), m_gameId} },
              [this](int code, const QString&) {
        setStakeState(code == 0 ? QStringLiteral("refunded") : QStringLiteral("staked"));
        if (code != 0) setLastError(QStringLiteral("refund failed (exit %1)").arg(code));
    });
    return QString();
}

QString ZkGuessGameBackend::leaveRoom()
{
    if (m_heartbeat) { m_heartbeat->stop(); }
    if (!m_topic.isEmpty())
        modules().delivery_module.unsubscribeAsync(m_topic, [](LogosResult){}, Timeout());
    m_topic.clear(); m_players.clear();
    m_chat = QJsonArray(); m_turns = QJsonArray(); m_contribs.clear();
    m_turnOrder.clear(); m_turnIdx = -1;
    // reset EVERY game/room prop so the lobby renders clean (no stale win/room overlay).
    setInRoom(false); setIsCreator(false);
    setCollectingEntropy(false); setEntropySubmitted(false); setStarted(false);
    setWon(false); setWinnerName(QString()); setSecretRevealed(-1);
    setProvingName(QString()); setProvingGuess(-1);
    setSettling(false); setSettleStartMs(0); setSettleBlock(-1); setSettleTx(QString()); setSettleError(QString());
    setSealedCommitment(QString()); setCurrentTurnId(QString()); setCurrentTurnName(QString());
    setRoomCode(QString()); setRoomName(QString());
    setRosterJson("[]"); setChatJson("[]"); setTurnsJson("[]");
    setConnectionStatus(QStringLiteral("idle"));
    return QString();
}
