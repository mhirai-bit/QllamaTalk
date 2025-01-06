// main.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.VectorImage
import QtQuick.Controls
import QtQuick.Dialogs

import QllamaTalk
import content

ApplicationWindow {
    id: mainWindow
    title: qsTr("QllamaTalk")
    visible: true
    width: Screen.width
    height: Screen.height

    property bool isRemote: LlamaChatEngine.currentEngineMode === LlamaChatEngine.Mode_Remote

    Connections {
        target: LlamaChatEngine
        onInferenceErrorToQML: function(errorMessage) {
            if (mainWindow.isRemote) {
                remoteAIErrorDialog.errorMessage = errorMessage
                remoteAIErrorDialog.open()
            } else {
                localAIErrorDialog.errorMessage = errorMessage
                localAIErrorDialog.open()
            }
        }
    }

    MessageDialog {
        id: remoteAIErrorDialog
        property string errorMessage: ""
        text: qsTr("An Error Occurred in Remote AI")
        informativeText: qsTr("Do you want to fall back to the Local AI?")
        detailedText: qsTr("Error: %1").arg(errorMessage)
        buttons: MessageDialog.Yes | MessageDialog.No

        onButtonClicked: function(button, role){
            switch(button) {
                case MessageDialog.Yes:
                    LlamaChatEngine.switchEngineMode(LlamaChatEngine.Mode_Local)
                    break
                case MessageDialog.No:
                    break
            }
        }
    }

    MessageDialog {
        id: localAIErrorDialog
        property string errorMessage: ""
        text: qsTr("An Error Occurred in Local AI")
        informativeText: qsTr("Do you want to connect to the Remote AI?")
        detailedText: qsTr("Error: %1").arg(errorMessage)
        buttons: MessageDialog.Yes | MessageDialog.No

        onButtonClicked: function(button, role){
            switch(button) {
                case MessageDialog.Yes:
                    modeCombo.currentIndex = 1
                    settingsDrawer.open()
                    break
                case MessageDialog.No:
                    break
            }
        }
    }

    // ヘッダー部分
    header: Item {
        width: parent.width
        height: headerRow.height

        Row {
            id: headerRow
            leftPadding: 8
            spacing: 8

            VectorImage {
                id: onlineOfflineIcon
                // isRemote に基づいてアイコンを切り替え
                source: mainWindow.isRemote ? "icons/online.svg" : "icons/offline.svg"
                anchors.verticalCenter: headerRow.verticalCenter
            }

            Label {
                // isRemote なら ipAddress:portNumber を表示
                text: mainWindow.isRemote
                    ? qsTr("Remote") + " " + LlamaChatEngine.ipAddress + ":" + LlamaChatEngine.portNumber
                    : qsTr("Local")
                anchors.verticalCenter: headerRow.verticalCenter
            }

            VectorImage {
                id: remoteInferenceErrorIcon
                visible: LlamaChatEngine.remoteAiInError
                source: "icons/remote_error.svg"
                anchors.verticalCenter: headerRow.verticalCenter
            }

            VectorImage {
                id: localInferenceErrorIcon
                visible: LlamaChatEngine.localAiInError
                source: "icons/local_error.svg"
                anchors.verticalCenter: headerRow.verticalCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                settingsDrawer.open()
            }
        }
    }

    // メインのチャットビュー
    ChatView {
        id: mainScreen
        anchors.fill: parent
    }

    // Drawer
    Drawer {
        id: settingsDrawer
        edge: Qt.LeftEdge
        width: drawerContent.width
        height: parent.height

        Column {
            id: drawerContent
            spacing: 16
            padding: 20

            Label {
                id: connectionSettingText
                text: qsTr("Connection Settings")
                font.bold: true
                font.pointSize: 16
            }

            Row {
                id: engineModeRow
                spacing: 8
                Label {
                    id: modeLabel
                    text: qsTr("Engine Mode:")
                    anchors.verticalCenter: modeCombo.verticalCenter
                }
                ComboBox {
                    id: modeCombo
                    width: modeLabel.width * 1.5
                    model: [qsTr("Local"), qsTr("Remote")]
                    currentIndex: mainWindow.isRemote ? 1 : 0
                }
            }

            // IP入力フィールド (regularExpressionValidatorの例)
            TextField {
                id: ipField
                width: engineModeRow.width
                placeholderText: qsTr("Enter server IP (e.g. 192.168.0.220)")
                enabled: modeCombo.currentIndex === 1
                validator: RegularExpressionValidator {
                    // IPv4用の簡易的な正規表現例 (完全には厳密ではありません)
                    // 0~255の範囲もすべてはカバーできないシンプル例
                    regularExpression: /^(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)$/
                }
            }

            // ポート入力フィールド (IntValidatorで範囲チェック)
            TextField {
                id: portField
                width: engineModeRow.width
                placeholderText: qsTr("Enter port (1-65535)")
                enabled: modeCombo.currentIndex === 1
                validator: IntValidator {
                    bottom: 1
                    top: 65535
                }
            }

            Button {
                text: qsTr("Apply")
                onClicked: {
                    if (modeCombo.currentIndex === 0) {
                        // Local
                        LlamaChatEngine.switchEngineMode(LlamaChatEngine.Mode_Local)
                        settingsDrawer.close()
                        return
                    }

                    LlamaChatEngine.ipAddress = ipField.text
                    LlamaChatEngine.portNumber = portField.text
                    LlamaChatEngine.switchEngineMode(LlamaChatEngine.Mode_Remote)
                    settingsDrawer.close()
                }
            }
        }
    }
}
