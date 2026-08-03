import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ZK Guess — lobby + room (EPIC A). Create/join a room by code over Logos Messaging;
// roster + chat over the room topic. The guess/turn view (C) mounts when `started`.
Rectangle {
    id: root
    color: "#0a0d0c"

    readonly property var backend: logos.module("zk_guess_game")
    readonly property color teal:  "#2ee6a6"
    readonly property color amber: "#e8b24a"
    readonly property color red:   "#e8663a"
    readonly property color dim:   "#5f6f68"
    readonly property color fg:    "#c6d0cb"
    readonly property string mono: "monospace"

    property var roster: []
    property var chat: []
    property var turns: []
    function statusColor(s) { return s === "connected" ? teal : (s === "connecting" ? amber : dim) }
    function dirName(d) { return d === 0 ? "BELOW" : d === 1 ? "EQUAL" : d === 2 ? "ABOVE" : "?" }
    function rangeLo() { var lo = 0; for (var i=0;i<turns.length;i++){ var t=turns[i]; if(t.dir===0 && t.guess+1>lo) lo=t.guess+1 } return lo }
    function rangeHi() { var hi = 1000000; for (var i=0;i<turns.length;i++){ var t=turns[i]; if(t.dir===2 && t.guess-1<hi) hi=t.guess-1 } return hi }
    function submitInput(s) {
        if (!backend || !s) return
        if (backend.started) { var n = parseInt(s); if (!isNaN(n)) backend.submitGuess(n) }
        else backend.sendChat(s)
    }

    // reusable delivery/Logos status pill (booth/receiver style)
    component StatusPill : Rectangle {
        property string status: "idle"
        radius: 10; color: "#0f1614"; border.color: "#1c2622"
        implicitHeight: 22; implicitWidth: pr.implicitWidth + 16
        Row {
            id: pr; anchors.centerIn: parent; spacing: 6
            Rectangle { width: 7; height: 7; radius: 4; anchors.verticalCenter: parent.verticalCenter
                        color: root.statusColor(status) }
            Text { text: "delivery: " + status; color: root.dim; font.family: root.mono; font.pixelSize: 12 }
        }
    }
    // compact themed button (fixes the giant default QtQuick buttons)
    component GButton : Button {
        id: gb
        padding: 7
        font.family: root.mono; font.pixelSize: 13
        contentItem: Text {
            text: gb.text; font: gb.font
            color: gb.enabled ? root.teal : root.dim
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 5; implicitHeight: 26
            color: gb.down ? "#14201c" : "#0f1614"
            border.color: gb.enabled ? root.teal : "#1c2622"
            opacity: gb.enabled ? 1.0 : 0.55
        }
    }

    function refresh() {
        try { roster = JSON.parse(backend ? backend.rosterJson : "[]") } catch(e) { roster = [] }
        try { chat   = JSON.parse(backend ? backend.chatJson   : "[]") } catch(e) { chat = [] }
        try { turns  = JSON.parse(backend ? backend.turnsJson  : "[]") } catch(e) { turns = [] }
    }
    Connections {
        target: root.backend
        enabled: root.backend !== null
        ignoreUnknownSignals: true
        function onRosterJsonChanged() { root.refresh() }
        function onChatJsonChanged()   { root.refresh() }
        function onTurnsJsonChanged()  { root.refresh() }
    }
    Component.onCompleted: refresh()

    // ── LOBBY ───────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 440)
        spacing: 16
        visible: !backend || !backend.inRoom

        Text { text: "⌗ ZK Guess"; color: root.teal; font.family: root.mono; font.pixelSize: 26; font.bold: true; Layout.alignment: Qt.AlignHCenter }
        Text {
            Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: "A provably-fair guessing party. The machine seals a number; you guess; it proves above/below without revealing it — and can't cheat."
            color: root.dim; font.family: root.mono; font.pixelSize: 13
        }
        TextField {
            id: nameField; Layout.fillWidth: true
            placeholderText: "your name"; color: root.fg; font.family: root.mono
            background: Rectangle { color: "#0f1614"; border.color: "#1c2622"; radius: 4 }
        }
        GButton {
            text: "Start new game"; Layout.fillWidth: true
            onClicked: backend && backend.createRoom("ZK Guess", nameField.text)
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#1c2622" }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            TextField {
                id: codeField; Layout.fillWidth: true
                placeholderText: "room code"; color: root.fg; font.family: root.mono
                inputMethodHints: Qt.ImhUppercaseOnly
                background: Rectangle { color: "#0f1614"; border.color: "#1c2622"; radius: 4 }
            }
            GButton { text: "Join created game"; onClicked: backend && backend.joinRoom(codeField.text, nameField.text) }
        }
        Text {
            visible: backend && backend.lastError.length > 0
            text: backend ? backend.lastError : ""; color: root.red; font.family: root.mono; font.pixelSize: 12
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // lobby status pill (top-right)
    StatusPill {
        visible: !backend || !backend.inRoom
        status: backend ? backend.connectionStatus : "idle"
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 14
    }

    // ── ROOM ────────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 14; spacing: 10
        visible: backend && backend.inRoom

        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Text { text: "⌗ " + (backend ? backend.roomName : ""); color: root.teal; font.family: root.mono; font.pixelSize: 15; font.bold: true }
            RowLayout {
                visible: backend && backend.isCreator; spacing: 6
                Text { text: "· invite code " + (backend ? backend.roomCode : ""); color: root.amber; font.family: root.mono; font.pixelSize: 13 }
                TextEdit { id: codeClip; visible: false; text: backend ? backend.roomCode : "" }
                GButton { text: "⧉ copy"; padding: 4; font.family: root.mono
                         onClicked: { codeClip.selectAll(); codeClip.copy() } }
            }
            Item { Layout.fillWidth: true }
            StatusPill { status: backend ? backend.connectionStatus : "idle" }
            GButton { text: "leave"; onClicked: backend && backend.leaveRoom() }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#1c2622" }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12

            // left: game board when started, else chat
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8

                Text {   // win banner
                    visible: backend && backend.won
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; font.family: root.mono; font.pixelSize: 14
                    text: "★ " + (backend ? backend.winnerName : "") + " won — the number was "
                          + (backend ? backend.secretRevealed : "") + "  ✓ matches the seal"
                    color: "#7ef0c4"
                }
                Text {   // game info: sealed commitment + narrowed range
                    visible: backend && backend.started
                    Layout.fillWidth: true; font.family: root.mono; font.pixelSize: 12
                    text: "🔒 sealed " + (backend && backend.sealedCommitment.length >= 8
                              ? "0x" + backend.sealedCommitment.substring(0,8) + "…" : "…")
                          + "   ·   you know " + root.rangeLo() + "…" + root.rangeHi()
                    color: root.dim
                }
                ListView {   // feed: turns in-game, chat pre-game
                    id: feed; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: (backend && backend.started) ? root.turns : root.chat
                    onCountChanged: positionViewAtEnd()
                    delegate: Text {
                        width: feed.width; wrapMode: Text.WordWrap; font.family: root.mono; font.pixelSize: 13
                        text: (modelData.dir !== undefined)
                              ? ("✓ " + (modelData.name || "?") + " · " + modelData.guess + " · " + root.dirName(modelData.dir))
                              : ((modelData.name || "?") + "  " + (modelData.text || ""))
                        color: modelData.dir === 1 ? "#7ef0c4" : (modelData.dir !== undefined ? root.teal : root.fg)
                    }
                }
                RowLayout {   // input: guess (in-game player) or chat
                    Layout.fillWidth: true; spacing: 8
                    Text {
                        visible: backend && backend.started && backend.isCreator
                        text: "you sealed the number — waiting for guesses…"
                        color: root.dim; font.family: root.mono; font.pixelSize: 12
                    }
                    TextField {
                        id: inputField; Layout.fillWidth: true
                        visible: !(backend && backend.started && backend.isCreator)
                        enabled: !(backend && backend.won)
                        placeholderText: (backend && backend.started) ? "your guess 0–1,000,000…" : "message the room…"
                        color: root.fg; font.family: root.mono
                        background: Rectangle { color: "#0f1614"; border.color: "#1c2622"; radius: 4 }
                        onAccepted: { root.submitInput(text); text = "" }
                    }
                    GButton {
                        visible: !(backend && backend.started && backend.isCreator)
                        text: (backend && backend.started) ? "Guess" : "Send"
                        enabled: !(backend && backend.won)
                        onClicked: { root.submitInput(inputField.text); inputField.text = "" }
                    }
                }
            }

            // roster sidebar
            ColumnLayout {
                Layout.preferredWidth: 200; Layout.fillHeight: true; spacing: 6
                Text { text: "in the room (" + root.roster.length + ")"; color: root.dim; font.family: root.mono; font.pixelSize: 12 }
                Repeater {
                    model: root.roster
                    delegate: RowLayout {
                        spacing: 8
                        Rectangle { width: 7; height: 7; radius: 4; color: modelData.online ? root.teal : root.dim }
                        Text {
                            text: (modelData.name || "?") + (modelData.role === "creator" ? " ★" : "")
                            color: root.fg; font.family: root.mono; font.pixelSize: 13
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                GButton {
                    visible: backend && backend.isCreator && !backend.started
                    enabled: root.roster.length >= 2
                    text: "Start game"; Layout.fillWidth: true
                    onClicked: backend && backend.startGame()
                }
                Text {
                    visible: backend && backend.started && !backend.won
                    text: "guessing in progress…"; color: root.teal; font.family: root.mono; font.pixelSize: 12; wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Text {
                    visible: backend && backend.won
                    text: "★ " + (backend ? backend.winnerName : "") + " won"; color: "#7ef0c4"
                    font.family: root.mono; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }
    }
}
