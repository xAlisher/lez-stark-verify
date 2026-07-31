import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// ZK Eligibility verifier — drives the backend's verifyReceipt() over the bundled
// fixtures and shows the verdict. Verify is milliseconds; only the claim is revealed.
Rectangle {
    id: root
    color: Theme.palette.background

    readonly property var backend: logos.module("zk_eligibility_ui")

    property string verdict: ""
    property color  verdictColor: Theme.palette.textSecondary
    property string detail: ""
    property bool   busy: false

    Connections {
        target: root.backend
        enabled: root.backend !== null
        ignoreUnknownSignals: true
        function onVerifyResult(valid, threshold, eligible, error) {
            root.busy = false
            if (error && error.length > 0) {
                root.verdict = qsTr("ERROR"); root.verdictColor = Theme.palette.error; root.detail = error; return
            }
            if (valid) {
                root.verdict = qsTr("VERIFIED ✓")
                root.verdictColor = Theme.palette.success
                root.detail = qsTr("journal = (threshold=%1, eligible=%2) — the secret stayed private.")
                    .arg(threshold).arg(eligible)
            } else {
                root.verdict = qsTr("REJECTED ✗")
                root.verdictColor = Theme.palette.error
                root.detail = qsTr("The receipt failed verification against the program image id.")
            }
        }
    }

    function _verify(name) {
        if (!root.backend) return
        root.busy = true; root.verdict = qsTr("Verifying…"); root.verdictColor = Theme.palette.textSecondary; root.detail = ""
        root.backend.verifyReceipt(name)
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 540)
        spacing: Theme.spacing.large

        LogosText {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("ZK Eligibility Verifier")
            font.pixelSize: Theme.typography.pageTitleText
            font.weight: Theme.typography.weightMedium
            color: Theme.palette.text
        }
        LogosText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Verify a RISC0 STARK proof that a secret value clears a public threshold — "
                       + "on the Logos Execution Zone. Verification is milliseconds; nothing but the "
                       + "claim is revealed.")
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.subtitleText
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacing.medium
            LogosButton {
                text: qsTr("Verify honest proof")
                enabled: !root.busy
                onClicked: root._verify("valid")
            }
            LogosButton {
                text: qsTr("Verify tampered receipt")
                enabled: !root.busy
                onClicked: root._verify("tampered")
            }
        }

        LogosText {
            visible: root.verdict.length > 0
            Layout.alignment: Qt.AlignHCenter
            text: root.verdict
            color: root.verdictColor
            font.pixelSize: Theme.typography.titleText
            font.weight: Theme.typography.weightMedium
        }
        LogosText {
            visible: root.detail.length > 0
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.detail
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }
    }
}
