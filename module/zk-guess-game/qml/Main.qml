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
    property var feedItems: []   // turns + chat, one time-ordered stream
    function statusColor(s) { return s === "connected" ? teal : (s === "connecting" ? amber : dim) }
    function tsFmt(ms) { if (!ms) return "--:--:--"; var d = new Date(ms)
        return ("0"+d.getHours()).slice(-2)+":"+("0"+d.getMinutes()).slice(-2)+":"+("0"+d.getSeconds()).slice(-2) }
    function dirName(d) { return d === 0 ? "BELOW" : d === 1 ? "EQUAL" : d === 2 ? "ABOVE" : "?" }
    function rangeLo() { var lo = 0; for (var i=0;i<turns.length;i++){ var t=turns[i]; if(t.dir===0 && t.guess+1>lo) lo=t.guess+1 } return lo }
    function rangeHi() { var hi = 1000000; for (var i=0;i<turns.length;i++){ var t=turns[i]; if(t.dir===2 && t.guess-1<hi) hi=t.guess-1 } return hi }
    function submitInput(s) {
        if (!backend || !s) return
        if (backend.started) { var n = parseInt(s); if (!isNaN(n)) backend.submitGuess(n) }
        else backend.sendChat(s)
    }

    // ── proving/settling animation state (proto-style braille spinner + live timers) ──
    readonly property string spinFrames: "⣾⣽⣻⢿⡿⣟⣯⣷"
    property int  spinIdx: 0
    property real nowMs: 0
    readonly property bool proving: backend && backend.provingName !== ""       // per-turn STARK (fast)
    readonly property bool settling: backend && backend.settling                // on-zone win (real ~16min)
    readonly property int  settleEtaMs: 16 * 60 * 1000                           // ~16 min real proof
    function clockFmt(ms) { if (ms < 0) ms = 0; var s = Math.floor(ms/1000)
        return ("0"+Math.floor(s/60)).slice(-2) + ":" + ("0"+(s%60)).slice(-2) }
    // one ticker drives both spinners; runs only while something is proving.
    Timer {
        interval: 90; repeat: true; running: root.proving || root.settling
        onRunningChanged: if (running) root.nowMs = Date.now()
        onTriggered: { root.spinIdx = (root.spinIdx + 1) % root.spinFrames.length; root.nowMs = Date.now() }
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
        var merged = (backend && backend.started) ? root.turns.concat(root.chat) : root.chat.slice()
        merged.sort(function(a, b) { return (a.ts || 0) - (b.ts || 0) })
        feedItems = merged
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
            text: "A provably-fair number-guessing party. Everyone stirs a secret number into a sealed commitment, then takes turns guessing over Logos Messaging.\n\n"
                + "Each guess is answered with a RISC0 STARK proof verified on the Logos Execution Zone (LEZ) — so the host can't lie about \"higher / lower\" and can't change the number. The winner's reveal is checked against the seal."
            color: root.dim; font.family: root.mono; font.pixelSize: 13
        }
        TextField {
            id: nameField; Layout.fillWidth: true
            placeholderText: "your name"; color: root.fg; font.family: root.mono
            background: Rectangle { color: "#0f1614"; border.color: "#1c2622"; radius: 4 }
            Component.onCompleted: if (backend && backend.suggestedName.length > 0) text = backend.suggestedName
            Connections {   // suggestedName syncs over QtRO after onCompleted — fill in when it arrives
                target: root.backend; enabled: root.backend !== null; ignoreUnknownSignals: true
                function onSuggestedNameChanged() { if (nameField.text.length === 0) nameField.text = root.backend.suggestedName }
            }
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

    // design-doc link — pinned bottom-center of the welcome screen + copy
    RowLayout {
        visible: !backend || !backend.inRoom
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 14
        spacing: 8
        Text { text: "📖 game design + how it works →"; color: root.dim; font.family: root.mono; font.pixelSize: 11 }
        TextEdit { id: ghClip; visible: false; text: "https://github.com/xAlisher/lez-stark-verify/blob/master/docs/GAME-DESIGN.md" }
        GButton { text: "⧉ copy link"; padding: 3; onClicked: { ghClip.selectAll(); ghClip.copy() } }
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
                ListView {   // one time-ordered stream: turns + chat interleaved by timestamp
                    id: feed; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: root.feedItems
                    onCountChanged: positionViewAtEnd()
                    delegate: Text {
                        width: feed.width; wrapMode: Text.WordWrap; font.family: root.mono; font.pixelSize: 13
                        text: "<font color='#3f4c47'>" + root.tsFmt(modelData.ts) + "</font>  "
                              + ((modelData.dir !== undefined)
                                 ? ((modelData.name || "?") + " · " + modelData.guess + " · " + root.dirName(modelData.dir)
                                    + (modelData.proven ? " · verified on LEZ ✓" : " · (unverified)"))
                                 : ((modelData.name || "?") + "  " + (modelData.text || "")))
                        textFormat: Text.StyledText
                        color: modelData.dir === 1 ? "#7ef0c4" : (modelData.dir !== undefined ? root.teal : root.fg)
                    }
                }
                RowLayout {   // chat input — always available (anyone can chat, any time)
                    Layout.fillWidth: true; spacing: 8
                    TextField {
                        id: chatInput; Layout.fillWidth: true
                        placeholderText: "message the room…"; color: root.fg; font.family: root.mono
                        background: Rectangle { color: "#0f1614"; border.color: "#1c2622"; radius: 4 }
                        onAccepted: { if (backend) backend.sendChat(text); text = "" }
                    }
                    GButton { text: "Send"; onClicked: { if (backend) backend.sendChat(chatInput.text); chatInput.text = "" } }
                }
                Text {   // host sees whose turn it is
                    visible: backend && backend.started && backend.isCreator && !backend.won
                    text: "▸ " + (backend ? backend.currentTurnName : "") + " to guess…  (you sealed the number)"
                    color: root.dim; font.family: root.mono; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                Text {   // player waiting for someone else's turn
                    visible: backend && backend.started && !backend.isCreator && !backend.won && backend.currentTurnId !== backend.myId
                    text: "▸ waiting — " + (backend ? backend.currentTurnName : "") + "'s turn"
                    color: root.amber; font.family: root.mono; font.pixelSize: 12
                }
                RowLayout {   // MY turn: slide within the known range, then Guess
                    Layout.fillWidth: true; spacing: 10
                    visible: backend && backend.started && !backend.isCreator && !backend.won && backend.currentTurnId === backend.myId
                    Text { text: "your turn ▸"; color: root.teal; font.family: root.mono; font.pixelSize: 12; font.bold: true }
                    Text { text: root.rangeLo(); color: root.dim; font.family: root.mono; font.pixelSize: 12 }
                    Slider {
                        id: guessSlider; Layout.fillWidth: true; implicitHeight: 22
                        from: root.rangeLo(); to: Math.max(root.rangeLo(), root.rangeHi()); stepSize: 1
                        Component.onCompleted: value = Math.round((root.rangeLo() + root.rangeHi()) / 2)
                        Connections {   // re-center on every verdict AND when the turn arrives (range narrows in all UIs)
                            target: root.backend; enabled: root.backend !== null; ignoreUnknownSignals: true
                            function onTurnsJsonChanged()    { guessSlider.value = Math.round((root.rangeLo() + root.rangeHi()) / 2) }
                            function onCurrentTurnIdChanged() { guessSlider.value = Math.round((root.rangeLo() + root.rangeHi()) / 2) }
                        }
                        background: Rectangle {   // dark track + teal fill (default QML style is white)
                            x: guessSlider.leftPadding
                            y: guessSlider.topPadding + guessSlider.availableHeight / 2 - height / 2
                            width: guessSlider.availableWidth; height: 5; radius: 2.5
                            color: "#1c2622"
                            Rectangle {
                                width: guessSlider.visualPosition * parent.width; height: parent.height
                                radius: 2.5; color: root.teal
                            }
                        }
                        handle: Rectangle {
                            x: guessSlider.leftPadding + guessSlider.visualPosition * (guessSlider.availableWidth - width)
                            y: guessSlider.topPadding + guessSlider.availableHeight / 2 - height / 2
                            implicitWidth: 16; implicitHeight: 16; radius: 8
                            color: guessSlider.pressed ? root.teal : "#0f1614"
                            border.color: root.teal; border.width: 2
                        }
                    }
                    Text { text: root.rangeHi(); color: root.dim; font.family: root.mono; font.pixelSize: 12 }
                    Text { text: "→ " + Math.round(guessSlider.value); color: root.teal; font.family: root.mono; font.pixelSize: 15; font.bold: true }
                    GButton { text: "Guess"; onClicked: backend && backend.submitGuess(Math.round(guessSlider.value)) }
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
                            property bool isTurn: root.backend && root.backend.started && modelData.id === root.backend.currentTurnId
                            text: (isTurn ? "▸ " : "") + (modelData.name || "?") + (modelData.role === "creator" ? " ★" : "")
                            color: isTurn ? root.teal : root.fg; font.family: root.mono; font.pixelSize: 13
                            font.bold: isTurn
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
                // per-turn STARK proving — proto's amber braille spinner + live elapsed
                RowLayout {
                    visible: root.proving && backend && !backend.won
                    Layout.fillWidth: true; spacing: 7
                    Text { text: root.spinFrames.charAt(root.spinIdx); color: root.amber
                           font.family: root.mono; font.pixelSize: 15; font.bold: true }
                    Text {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: "proving " + (backend ? backend.provingName : "") + "'s guess "
                              + (backend && backend.provingGuess >= 0 ? backend.provingGuess : "")
                              + " — verifying on LEZ"
                        color: root.amber; font.family: root.mono; font.pixelSize: 12
                    }
                }
                Text {
                    visible: backend && backend.started && !backend.won && !root.proving
                    text: "waiting for a guess…"; color: root.dim; font.family: root.mono; font.pixelSize: 11
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }
    }

    // ── B · ENTROPY PHASE (overlay) — everyone stirs the number ──────────────
    Rectangle {
        visible: backend && backend.collectingEntropy
        anchors.fill: parent
        color: "#f2060a08"
        MouseArea { anchors.fill: parent }   // block room clicks (canvas has its own)
        ColumnLayout {
            anchors.centerIn: parent; spacing: 12; width: Math.min(parent.width - 48, 480)
            Text { text: "🎲 stir the number"; color: root.teal; font.family: root.mono; font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignHCenter }
            Text {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                text: "Everyone scribbles. Your randomness is folded into the sealed number — so no single player (not even the host) picks it."
                color: root.dim; font.family: root.mono; font.pixelSize: 12
            }
            Text {
                visible: backend && backend.isCreator
                text: "collecting entropy from players…"; color: root.amber; font.family: root.mono; font.pixelSize: 13
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {   // draw surface (players)
                visible: backend && !backend.isCreator && !backend.entropySubmitted
                Layout.fillWidth: true; Layout.preferredHeight: 220
                color: "#0f1614"; border.color: "#1c2622"; radius: 6
                Canvas {
                    id: ecanvas; anchors.fill: parent
                    property string strokes: ""
                    property var pts: []      // {x,y,pen} — pen=false starts a new stroke
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = root.teal; ctx.lineWidth = 2; ctx.lineJoin = "round"; ctx.lineCap = "round"
                        ctx.beginPath()
                        for (var i = 0; i < pts.length; i++) {
                            var p = pts[i]
                            if (p.pen) ctx.lineTo(p.x, p.y); else ctx.moveTo(p.x, p.y)
                        }
                        ctx.stroke()
                    }
                    MouseArea {
                        anchors.fill: parent
                        onPressed: (mouse) => {
                            ecanvas.pts.push({x: mouse.x, y: mouse.y, pen: false})
                            ecanvas.strokes += Math.round(mouse.x) + "," + Math.round(mouse.y) + ";"
                            ecanvas.requestPaint()
                        }
                        onPositionChanged: (mouse) => {
                            if (!pressed) return
                            ecanvas.pts.push({x: mouse.x, y: mouse.y, pen: true})
                            ecanvas.strokes += Math.round(mouse.x) + "," + Math.round(mouse.y) + ";"
                            ecanvas.requestPaint()
                        }
                    }
                }
            }
            GButton {
                visible: backend && !backend.isCreator && !backend.entropySubmitted
                text: "Submit my draw"; Layout.alignment: Qt.AlignHCenter
                enabled: ecanvas.strokes.length > 20
                onClicked: backend && backend.submitEntropy(ecanvas.strokes)
            }
            Text {
                visible: backend && !backend.isCreator && backend.entropySubmitted
                text: "✓ entropy submitted — waiting for the seal…"; color: root.teal
                font.family: root.mono; font.pixelSize: 13; Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    // ── E · WIN SCREEN (overlay) ─────────────────────────────────────────────
    Rectangle {
        visible: backend && backend.won
        anchors.fill: parent
        color: "#f2060a08"
        MouseArea { anchors.fill: parent }   // swallow clicks to the room behind
        ColumnLayout {
            anchors.centerIn: parent; spacing: 14; width: Math.min(parent.width - 64, 440)
            Text { text: "★"; color: "#7ef0c4"; font.pixelSize: 44; Layout.alignment: Qt.AlignHCenter }
            Text {
                text: (backend ? backend.winnerName : "") + " won"
                color: root.teal; font.family: root.mono; font.pixelSize: 26; font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: "the number was " + (backend ? backend.secretRevealed : "")
                color: root.fg; font.family: root.mono; font.pixelSize: 18; Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: "✓ revealed number hashes to the sealed commitment — provably fair"
                color: root.dim; font.family: root.mono; font.pixelSize: 12; wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true
            }

            // ── on-zone settlement (real STARK, ~16 min) — non-blocking, opt-in ──
            Rectangle {
                Layout.fillWidth: true; Layout.topMargin: 6
                radius: 8; color: "#0d1512"; border.color: "#1c2622"
                implicitHeight: settleCol.implicitHeight + 24
                ColumnLayout {
                    id: settleCol
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                              leftMargin: 14; rightMargin: 14 }
                    spacing: 8

                    // idle: offer to notarize the win on-zone
                    ColumnLayout {
                        visible: backend && backend.settleBlock < 0 && !root.settling && backend.settleError === ""
                        Layout.fillWidth: true; spacing: 6
                        Text { text: "Settle this win on the LEZ"; color: root.fg; font.family: root.mono
                               font.pixelSize: 14; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                        Text { text: "a real STARK, verified on-zone — takes ~16 min, runs in the background"
                               color: root.dim; font.family: root.mono; font.pixelSize: 11; wrapMode: Text.WordWrap
                               horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
                        GButton { text: "Settle on LEZ"; Layout.alignment: Qt.AlignHCenter
                                  onClicked: backend && backend.settleOnLez() }
                    }

                    // settling: braille spinner + countdown + progress
                    ColumnLayout {
                        visible: root.settling
                        Layout.fillWidth: true; spacing: 8
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter; spacing: 9
                            Text { text: root.spinFrames.charAt(root.spinIdx); color: root.amber
                                   font.family: root.mono; font.pixelSize: 16; font.bold: true }
                            Text { text: "settling on LEZ — proving the win"; color: root.amber
                                   font.family: root.mono; font.pixelSize: 13 }
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            property real remaining: root.settleEtaMs - (root.nowMs - (backend ? backend.settleStartMs : 0))
                            text: remaining > 0 ? "~" + root.clockFmt(remaining) + " left" : "taking a bit longer than usual…"
                            color: remaining > 0 ? root.fg : root.amber
                            font.family: root.mono; font.pixelSize: remaining > 0 ? 20 : 14; font.bold: true
                        }
                        Rectangle {   // progress bar (elapsed / ETA)
                            Layout.fillWidth: true; height: 5; radius: 2.5; color: "#1c2622"
                            Rectangle {
                                height: parent.height; radius: 2.5; color: root.amber
                                width: parent.width * Math.max(0, Math.min(1,
                                    (root.nowMs - (backend ? backend.settleStartMs : 0)) / root.settleEtaMs))
                            }
                        }
                        Text { text: "you can leave — the block lands whether or not you watch"
                               color: root.dim; font.family: root.mono; font.pixelSize: 10
                               Layout.alignment: Qt.AlignHCenter }
                    }

                    // settled ✓ — with the on-zone tx hash as proof (selectable to copy)
                    ColumnLayout {
                        visible: backend && backend.settleBlock >= 0
                        Layout.fillWidth: true; spacing: 3
                        Text { text: "✓ settled on LEZ — block " + (backend ? backend.settleBlock : "")
                               color: root.teal; font.family: root.mono; font.pixelSize: 14; font.bold: true
                               Layout.alignment: Qt.AlignHCenter }
                        Text { visible: backend && backend.settleTx !== ""
                               text: "proof — on-zone tx (select to copy)"; color: root.dim
                               font.family: root.mono; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                        TextEdit { visible: backend && backend.settleTx !== ""
                               text: backend ? backend.settleTx : ""
                               readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
                               color: root.teal; font.family: root.mono; font.pixelSize: 10
                               Layout.fillWidth: true; horizontalAlignment: TextEdit.AlignHCenter }
                    }
                    // error
                    Text {
                        visible: backend && backend.settleError !== ""
                        text: "settlement: " + (backend ? backend.settleError : "")
                        color: root.red; font.family: root.mono; font.pixelSize: 11; wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true
                    }
                }
            }

            GButton { text: "leave room"; Layout.alignment: Qt.AlignHCenter; onClicked: backend && backend.leaveRoom() }
        }
    }
}
