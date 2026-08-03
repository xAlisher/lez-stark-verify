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
    function refresh() {
        try { roster = JSON.parse(backend ? backend.rosterJson : "[]") } catch(e) { roster = [] }
        try { chat   = JSON.parse(backend ? backend.chatJson   : "[]") } catch(e) { chat = [] }
    }
    Connections {
        target: root.backend
        enabled: root.backend !== null
        ignoreUnknownSignals: true
        function onRosterJsonChanged() { root.refresh() }
        function onChatJsonChanged()   { root.refresh() }
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
        Button {
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
            Button { text: "Join created game"; onClicked: backend && backend.joinRoom(codeField.text, nameField.text) }
        }
        Text {
            visible: backend && backend.lastError.length > 0
            text: backend ? backend.lastError : ""; color: root.red; font.family: root.mono; font.pixelSize: 12
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // ── ROOM ────────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 14; spacing: 10
        visible: backend && backend.inRoom

        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Text { text: "⌗ " + (backend ? backend.roomName : ""); color: root.teal; font.family: root.mono; font.pixelSize: 15; font.bold: true }
            Text {
                visible: backend && backend.isCreator
                text: "· invite code " + (backend ? backend.roomCode : "")
                color: root.amber; font.family: root.mono; font.pixelSize: 13
            }
            Item { Layout.fillWidth: true }
            Text { text: backend ? backend.connectionStatus : ""; color: root.dim; font.family: root.mono; font.pixelSize: 12 }
            Button { text: "leave"; onClicked: backend && backend.leaveRoom() }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#1c2622" }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12

            // chat / log
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8
                ListView {
                    id: chatView; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: root.chat
                    onCountChanged: positionViewAtEnd()
                    delegate: Text {
                        width: chatView.width; wrapMode: Text.WordWrap; font.family: root.mono; font.pixelSize: 13
                        text: (modelData.name || "?") + "  " + (modelData.text || "")
                        color: modelData.id === (root.backend ? root.backend.myId : "") ? root.teal : root.fg
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    TextField {
                        id: chatInput; Layout.fillWidth: true
                        placeholderText: "message the room…"; color: root.fg; font.family: root.mono
                        background: Rectangle { color: "#0f1614"; border.color: "#1c2622"; radius: 4 }
                        onAccepted: { if (backend) backend.sendChat(text); text = "" }
                    }
                    Button { text: "Send"; onClicked: { if (backend) backend.sendChat(chatInput.text); chatInput.text = "" } }
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
                Button {
                    visible: backend && backend.isCreator && !backend.started
                    enabled: root.roster.length >= 2
                    text: "Start game"; Layout.fillWidth: true
                    onClicked: backend && backend.startGame()
                }
                Text {
                    visible: backend && backend.started
                    text: "game started — turns view next (C)"; color: root.teal; font.family: root.mono; font.pixelSize: 12; wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
