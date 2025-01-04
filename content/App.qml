// main.qml (例)
// 必要に応じて import は調整
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.VectorImage
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
        onEngineModeChanged: {
            console.log("Engine mode changed: " + LlamaChatEngine.currentEngineMode)
        }
    }

    // ヘッダー部分
    header: Row {
        spacing: 12

        VectorImage {
            id: onlineOfflineIcon
            source: mainWindow.isRemote ? "icons/online.svg" : "icons/offline.svg"
        }

        Text {
            text: mainWindow.isRemote ? qsTr("Remote") : qsTr("Local")
            anchors.verticalCenter: onlineOfflineIcon.verticalCenter
        }

    }


    // メインのチャットビュー
    ChatView {
        id: mainScreen
        anchors.fill: parent
        // 必要に応じてマージンや padding を調整
    }

    // Drawer（右からスライドする設定画面の例）
    Drawer {
        id: settingsDrawer
        edge: Qt.LeftEdge
        width: parent.width * 0.6
        height: parent.height

        // Drawer 内のレイアウト
        Column {
            id: drawerContent
            anchors.fill: parent
            spacing: 16
            padding: 20

            Text {
                text: qsTr("Connection Settings")
                font.bold: true
                font.pointSize: 16
            }

            // エンジンモード切り替え (ローカル / リモート)
            Row {
                spacing: 8
                Text { text: qsTr("Engine Mode:") }
                ComboBox {
                    id: modeCombo
                    model: [qsTr("Local"), qsTr("Remote")]
                    currentIndex: mainWindow.isRemote ? 1 : 0
                }
            }

            // IP入力フィールド
            TextField {
                id: ipField
                placeholderText: qsTr("Enter server IP (e.g. 192.168.0.220)")
            }

            // ポート入力フィールド
            TextField {
                id: portField
                placeholderText: qsTr("Enter port (e.g. 12345)")
            }

            // 現在の接続先を表示するためのラベル例
            Text {
                id: currentConnectionLabel
                text: mainWindow.isRemote ? qsTr("Remote") : qsTr("Local")
                // 例: Mode_Remote の場合には "Current: 192.168.0.220:12345" といった表示に変える
            }

            Button {
                text: qsTr("Apply")
                onClicked: {
                    LlamaChatEngine.ip_address = ipField.text
                    LlamaChatEngine.port_number = portField.text
                    if (modeCombo.currentIndex === 0) { LlamaChatEngine.switchEngineMode(LlamaChatEngine.Mode_Local) }
                    else { LlamaChatEngine.switchEngineMode(LlamaChatEngine.Mode_Remote) }
                    settingsDrawer.close()
                }
            }
        }
    }
}
