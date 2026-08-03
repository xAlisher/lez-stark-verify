import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// zk-guess — the anon guessing room (live). Type a number; the game HOST proves
// whether it's above/below the sealed secret (~8s) and the module verifies the
// receipt. Your guess never leaves as plaintext to opponents; only the direction
// is revealed. teal = proven/verified · amber = stake/proving.
Rectangle {
    id: root
    color: "#0a0d0c"

    readonly property var backend: logos.module("zk_guess_ui")
    readonly property color teal:  "#2ee6a6"
    readonly property color amber: "#e8b24a"
    readonly property color red:   "#e8663a"
    readonly property color dim:   "#5f6f68"
    readonly property color fg:    "#c6d0cb"
    readonly property string mono: "monospace"

    property bool   busy: false
    property bool   won:  false
    property string sealed: "0x…"
    property int    lo: 0
    property int    hi: 1000000

    function dirName(d) { return d === 0 ? "BELOW" : d === 1 ? "EQUAL" : d === 2 ? "ABOVE" : "?" }
    function sys(t)          { logModel.append({ kind: "sys",  line: t }) }
    function verdict(t, k)   { logModel.append({ kind: k, line: t }) }

    Connections {
        target: root.backend
        enabled: root.backend !== null
        ignoreUnknownSignals: true
        function onVerifyResult(valid, commitment, guess, dir, error) {
            root.busy = false
            if (error && error.length > 0) { root.verdict("✗ " + error, "bad"); return }
            if (!valid) { root.verdict("✗ REJECTED — receipt failed verification", "bad"); return }
            if (commitment.length >= 10) root.sealed = "0x" + commitment.substring(0, 8) + "…"
            if (dir === 1) {
                root.won = true
                root.verdict("★ EXACT — " + guess + " is the number. you win! (proven, not trusted)", "win")
            } else {
                root.verdict("✓ " + guess + " · " + root.dirName(dir) + " · verified on LEZ", "ok")
                if (dir === 0 && guess + 1 > root.lo) root.lo = guess + 1   // below -> go higher
                if (dir === 2 && guess - 1 < root.hi) root.hi = guess - 1   // above -> go lower
            }
        }
    }

    function submit(n) {
        if (!root.backend) { sys("backend unavailable"); return }
        if (root.busy || root.won) return
        if (isNaN(n) || n < 0 || n > 1000000) { sys("enter a number 0–1,000,000"); return }
        root.busy = true
        sys("▓ you · guess " + n + " · proving…")
        root.backend.submitGuess(n)
        guessField.clear()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Text { text: "⌗ guess"; color: root.teal; font.family: root.mono; font.pixelSize: 15; font.bold: true }
            Text { text: "· you know: " + root.lo + "…" + root.hi; color: root.dim; font.family: root.mono; font.pixelSize: 13 }
            Item { Layout.fillWidth: true }
            Text { text: "🔒 sealed " + root.sealed; color: root.dim; font.family: root.mono; font.pixelSize: 12 }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#1c2622" }

        ListView {
            id: log
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            model: ListModel { id: logModel }
            spacing: 3
            onCountChanged: positionViewAtEnd()
            delegate: Text {
                width: log.width; wrapMode: Text.WordWrap
                font.family: root.mono; font.pixelSize: 13
                text: line
                color: kind === "ok" ? root.teal : kind === "win" ? "#7ef0c4"
                     : kind === "bad" ? root.red : root.dim
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#1c2622" }

        RowLayout {
            Layout.fillWidth: true; spacing: 10
            Text {
                text: root.won ? "game over" : (root.busy ? "proving…" : "your move ›")
                color: root.busy ? root.amber : (root.won ? root.dim : root.teal)
                font.family: root.mono; font.pixelSize: 13; font.bold: true
            }
            TextField {
                id: guessField
                Layout.fillWidth: true
                enabled: !root.busy && !root.won
                placeholderText: qsTr("type a number 0–1,000,000, then Enter")
                color: root.fg
                font.family: root.mono; font.pixelSize: 13
                background: Rectangle { color: "#0f1614"; border.color: "#1c2622"; radius: 4 }
                validator: IntValidator { bottom: 0; top: 1000000 }
                onAccepted: root.submit(parseInt(text))
            }
            LogosButton {
                text: qsTr("Guess")
                enabled: !root.busy && !root.won && guessField.text.length > 0
                onClicked: root.submit(parseInt(guessField.text))
            }
        }
    }

    Component.onCompleted: {
        sys("— room #guess · a number 0–1,000,000 was sealed by the host 🔒 —")
        sys("type a guess. the host proves above/below without revealing the number; you verify the proof.")
        if (backend) backend.verifyReceipt("valid")   // pull the live commitment into the header
    }
}
