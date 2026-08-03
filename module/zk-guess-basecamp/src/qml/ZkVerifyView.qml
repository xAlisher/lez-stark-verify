import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// zk-guess — the anon guessing room. Each "turn" verifies a real RISC0 STARK
// receipt (bundled fixtures) via the backend and drops the verdict into the log.
// Guesses stay private; only the direction (above/below) is ever revealed.
// teal = proven/verified · amber = stake/proving.
Rectangle {
    id: root
    color: "#0a0d0c"

    readonly property var backend: logos.module("zk_guess_ui")
    readonly property color teal:  "#2ee6a6"
    readonly property color amber: "#e8b24a"
    readonly property color dim:   "#5f6f68"
    readonly property color text:  "#c6d0cb"
    readonly property string mono: "monospace"

    property bool   busy: false
    property string sealed: "0x8f3a…c1"

    function dirName(d) { return d === 0 ? "BELOW" : d === 1 ? "EQUAL" : d === 2 ? "ABOVE" : "?" }

    function sys(t)            { logModel.append({ kind: "sys",  line: t }) }
    function verdict(t, ok)    { logModel.append({ kind: ok ? "ok" : "bad", line: t }) }

    Connections {
        target: root.backend
        enabled: root.backend !== null
        ignoreUnknownSignals: true
        function onVerifyResult(valid, commitment, guess, dir, error) {
            root.busy = false
            if (error && error.length > 0) { root.verdict("✗ ERROR — " + error, false); return }
            if (valid) {
                if (commitment.length >= 10) root.sealed = "0x" + commitment.substring(0, 8) + "…"
                root.verdict("✓ verified · guess " + guess + " · " + root.dirName(dir)
                             + " · proof checked on LEZ", true)
            } else {
                root.verdict("✗ REJECTED — receipt failed verification against the image id", false)
            }
        }
    }

    function _verify(name) {
        if (!root.backend) { root.sys("backend unavailable"); return }
        root.busy = true
        root.sys(name === "tampered" ? "▓ submitting a tampered turn · proving…"
                                     : "▓ submitting an honest turn · proving…")
        root.backend.verifyReceipt(name)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // header
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Text { text: "⌗ guess"; color: root.teal; font.family: root.mono; font.pixelSize: 15; font.bold: true }
            Text { text: "· anon room"; color: root.dim; font.family: root.mono; font.pixelSize: 13 }
            Item { Layout.fillWidth: true }
            Text { text: "🔒 sealed " + root.sealed; color: root.dim; font.family: root.mono; font.pixelSize: 12 }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#1c2622" }

        // log
        ListView {
            id: log
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: ListModel { id: logModel }
            spacing: 3
            onCountChanged: positionViewAtEnd()
            delegate: Text {
                width: log.width
                wrapMode: Text.WordWrap
                font.family: root.mono
                font.pixelSize: 13
                text: line
                color: kind === "ok" ? root.teal
                     : kind === "bad" ? "#e8663a"
                     : root.dim
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#1c2622" }

        // input row — the two turn actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Text {
                text: root.busy ? "proving…" : "your move ›"
                color: root.busy ? root.amber : root.teal
                font.family: root.mono; font.pixelSize: 13; font.bold: true
            }
            Item { Layout.fillWidth: true }
            LogosButton {
                text: qsTr("Verify honest turn")
                enabled: !root.busy
                onClicked: root._verify("valid")
            }
            LogosButton {
                text: qsTr("Verify tampered turn")
                enabled: !root.busy
                onClicked: root._verify("tampered")
            }
        }
    }

    Component.onCompleted: {
        sys("— room #guess · a number was sealed 🔒 · guesses stay private, only verdicts hit the log —")
        sys("submit an honest turn (a real STARK proof of above/below) — or a tampered one, and watch it get rejected.")
    }
}
