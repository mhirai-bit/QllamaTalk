// main.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.VectorImage
import QtQuick.Controls

import QllamaTalk
import content

ApplicationWindow {
    id: mainWindow
    title: qsTr("QllamaTalk")
    visible: true
    width: Screen.width
    height: Screen.height

    property bool isRemote: LlamaChatEngine.currentEngineMode === LlamaChatEngine.Mode_Remote

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
            }

            Text {
                // isRemote なら ip_address:port_number を表示
                text: mainWindow.isRemote
                    ? qsTr("Remote") + " " + LlamaChatEngine.ip_address + ":" + LlamaChatEngine.port_number
                    : qsTr("Local")
                anchors.verticalCenter: onlineOfflineIcon.verticalCenter
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

            Text {
                id: connectionSettingText
                text: qsTr("Connection Settings")
                font.bold: true
                font.pointSize: 16
            }

            Row {
                id: engineModeRow
                spacing: 8
                Text {
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

                    LlamaChatEngine.ip_address = ipField.text
                    LlamaChatEngine.port_number = portField.text
                    LlamaChatEngine.switchEngineMode(LlamaChatEngine.Mode_Remote)
                    settingsDrawer.close()
                }
            }
        }
    }
}
