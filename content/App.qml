pragma ComponentBehavior: Bound

// main.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.VectorImage
import QtQuick.Controls
import QtQuick.Dialogs
import QtTextToSpeech

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
                height: connectionStatusLabel.height
                width: height
            }

            Label {
                id: connectionStatusLabel
                // isRemote なら ipAddress:portNumber を表示
                text: mainWindow.isRemote
                      ? qsTr("Remote") + " " + LlamaChatEngine.ipAddress + ":" + LlamaChatEngine.portNumber
                      : qsTr("Local")
                anchors.verticalCenter: headerRow.verticalCenter
                font.pointSize: 14
            }

            VectorImage {
                id: remoteInferenceErrorIcon
                visible: LlamaChatEngine.remoteAiInError
                source: "icons/remote_error.svg"
                anchors.verticalCenter: headerRow.verticalCenter
                height: connectionStatusLabel.height
                width: height
            }

            VectorImage {
                id: localInferenceErrorIcon
                visible: LlamaChatEngine.localAiInError
                source: "icons/local_error.svg"
                anchors.verticalCenter: headerRow.verticalCenter
                height: connectionStatusLabel.height
                width: height
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

    TextToSpeech {
        id: tts

        onStateChanged: {
            switch (state) {
            case TextToSpeech.Ready:
                LlamaChatEngine.resumeVoiceDetection()
                break
            case TextToSpeech.Speaking:
                break
            case TextToSpeech.Paused:
                break
            case TextToSpeech.Error:
                break
            }
        }
    }
    Connections {
        target: LlamaChatEngine
        function onGenerationFinishedToQML(text) {
            // 自分が読み上げたテキストを検知してしまわないように、voice detectionを一時的に止める
            LlamaChatEngine.pauseVoiceDetection()
            tts.say(text)
        }
    }

    // Drawer
    Drawer {
        id: settingsDrawer
        edge: Qt.LeftEdge
        width: drawerContent.width
        height: parent.height
        opacity: 0.9

        Column {
            id: drawerContent
            spacing: 24
            padding: 20

            Expander {
                id: connectionSettingsExpander
                title: qsTr("Connection Settings")
                property int modeComboCurrentIndex: 0
                property int engineModeRowWidth
                property string inputIpAddress: ""
                property int portNumber: 0
                model: [
                    Component {
                        RowLayout {
                            id: engineModeRowLayout
                            spacing: 8
                            Label {
                                id: modeLabel
                                text: qsTr("Engine Mode:")
                                font.pointSize: 16
                                Layout.alignment: Qt.AlignVCenter
                            }
                            ComboBox {
                                id: modeCombo
                                width: modeLabel.width * 1.5
                                model: [qsTr("Local"), qsTr("Remote")]
                                currentIndex: mainWindow.isRemote ? 1 : 0
                                font.pointSize: 16
                                onCurrentIndexChanged: {
                                    connectionSettingsExpander.modeComboCurrentIndex = modeCombo.currentIndex
                                }
                            }
                            Component.onCompleted: {
                                connectionSettingsExpander.engineModeRowWidth = width
                            }
                        }
                    },
                    Component {
                        // IP入力フィールド (regularExpressionValidatorの例)
                        TextField {
                            id: ipField
                            width: connectionSettingsExpander.engineModeRowWidth
                            placeholderText: qsTr("Enter server IP (e.g. 192.168.0.220)")
                            enabled: connectionSettingsExpander.modeComboCurrentIndex === 1
                            validator: RegularExpressionValidator {
                                // IPv4用の簡易的な正規表現例 (完全には厳密ではありません)
                                // 0~255の範囲もすべてはカバーできないシンプル例
                                regularExpression: /^(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)$/
                            }
                            font.pointSize: 16
                            onTextChanged: {
                                connectionSettingsExpander.inputIpAddress = ipField.text
                            }
                        }
                    },
                    Component {
                        // ポート入力フィールド (IntValidatorで範囲チェック)
                        TextField {
                            id: portField
                            width: connectionSettingsExpander.engineModeRowWidth
                            placeholderText: qsTr("Enter port (1-65535)")
                            enabled: connectionSettingsExpander.modeComboCurrentIndex === 1
                            validator: IntValidator {
                                bottom: 1
                                top: 65535
                            }
                            font.pointSize: 16
                            onTextChanged: {
                                connectionSettingsExpander.portNumber = portField.text
                            }
                        }
                    },
                    Component {
                        Button {
                            text: qsTr("Apply")
                            onClicked: {
                                if (connectionSettingsExpander.modeComboCurrentIndex === 0) {
                                    // Local
                                    LlamaChatEngine.switchEngineMode(LlamaChatEngine.Mode_Local)
                                    settingsDrawer.close()
                                    return
                                }

                                LlamaChatEngine.ipAddress = connectionSettingsExpander.inputIpAddress
                                LlamaChatEngine.portNumber = connectionSettingsExpander.portNumber
                                LlamaChatEngine.switchEngineMode(LlamaChatEngine.Mode_Remote)
                                settingsDrawer.close()
                            }
                            font.pointSize: 16
                        }
                    }
                ]
            }

            Expander {
                id: voiceSettingsExpander
                title: qsTr("Voice Settings")

                property list<string> allLocales: []
                property int currentLocaleIndex: 0
                property list<string> allVoices: []
                property int currentVoiceIndex: 0

                Component.onCompleted: {
                    // some engines initialize asynchronously
                    if (tts.state == TextToSpeech.Ready) {
                        engineReady()
                    } else {
                        tts.stateChanged.connect(voiceSettingsExpander.engineReady)
                    }

                    tts.updateStateLabel(tts.state)
                }

                function engineReady() {
                    tts.stateChanged.disconnect(voiceSettingsExpander.engineReady)
                    if (tts.state != TextToSpeech.Ready) {
                        tts.updateStateLabel(tts.state)
                        return;
                    }
                    updateLocales()
                    updateVoices()
                }

                function updateLocales() {
                    voiceSettingsExpander.allLocales = tts.availableLocales().map((locale) => locale.nativeLanguageName)
                    voiceSettingsExpander.currentLocaleIndex = allLocales.indexOf(tts.locale.nativeLanguageName)
                }

                function updateVoices() {
                    voiceSettingsExpander.allVoices = tts.availableVoices().map((voice) => voice.name)
                    voiceSettingsExpander.currentVoiceIndex = tts.availableVoices().indexOf(tts.voice)
                }

                model: [
                    Component {
                        RowLayout {
                            width: voiceSettingsExpander.width
                            spacing: 8
                            Label {
                                text: qsTr("Engine:")
                                font.pointSize: 16
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: voiceSettingsExpander.width * 0.3
                            }
                            ComboBox {
                                id: enginesComboBox
                                model: tts.availableEngines()
                                font.pointSize: 16
                                enabled: tts.state === TextToSpeech.Ready
                                Component.onCompleted: {
                                    currentIndex = tts.availableEngines().indexOf(tts.engine)
                                    tts.engine = textAt(currentIndex)
                                }
                                onActivated: {
                                    tts.engine = textAt(currentIndex)
                                    voiceSettingsExpander.updateLocales()
                                    voiceSettingsExpander.updateVoices()
                                }
                                Layout.fillWidth: true
                            }
                        }
                    },
                    Component {
                        RowLayout {
                            width: voiceSettingsExpander.width
                            spacing: 8
                            Label {
                                text: qsTr("Locale:")
                                font.pointSize: 16
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: voiceSettingsExpander.width * 0.3
                            }
                            ComboBox {
                                id: localesComboBox
                                font.pointSize: 16
                                enabled: tts.state === TextToSpeech.Ready
                                model: voiceSettingsExpander.allLocales
                                currentIndex: voiceSettingsExpander.currentLocaleIndex
                                onActivated: {
                                    let locales = tts.availableLocales()
                                    tts.locale = locales[currentIndex]
                                    voiceSettingsExpander.updateVoices()
                                }
                                Layout.fillWidth: true
                            }
                        }
                    },
                    Component {
                        RowLayout {
                            width: voiceSettingsExpander.width
                            spacing: 8
                            Label {
                                text: qsTr("Voice:")
                                font.pointSize: 16
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: voiceSettingsExpander.width * 0.3
                            }
                            ComboBox {
                                id: voicesComboBox
                                font.pointSize: 16
                                enabled: tts.state === TextToSpeech.Ready
                                model: voiceSettingsExpander.allVoices
                                currentIndex: voiceSettingsExpander.currentVoiceIndex
                                onActivated: {
                                    tts.voice = tts.availableVoices()[currentIndex]
                                }
                                Layout.fillWidth: true
                            }
                        }
                    },
                    Component {
                        RowLayout {
                            width: voiceSettingsExpander.width
                            spacing: 8
                            Label {
                                text: qsTr("Volume:")
                                font.pointSize: 16
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: voiceSettingsExpander.width * 0.3
                            }
                            Slider {
                                id: volumeSlider
                                enabled: tts.state === TextToSpeech.Ready
                                from: 0
                                to: 1.0
                                stepSize: 0.2
                                value: 0.8
                                font.pointSize: 16
                                Layout.fillWidth: true
                                Binding {
                                    target: tts
                                    property: "volume"
                                    value: volumeSlider.value
                                }
                            }
                        }
                    },
                    Component {
                        RowLayout {
                            width: voiceSettingsExpander.width
                            spacing: 8
                            Label {
                                text: qsTr("Pitch:")
                                font.pointSize: 16
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: voiceSettingsExpander.width * 0.3
                            }
                            Slider {
                                id: pitchSlider
                                enabled: tts.state === TextToSpeech.Ready
                                from: -1.0
                                to: 1.0
                                stepSize: 0.5
                                value: 0
                                font.pointSize: 16
                                Layout.fillWidth: true
                                Binding {
                                    target: tts
                                    property: "pitch"
                                    value: pitchSlider.value
                                }
                            }
                        }
                    },
                    Component {
                        RowLayout {
                            width: voiceSettingsExpander.width
                            spacing: 8
                            Label {
                                text: qsTr("Rate:")
                                font.pointSize: 16
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: voiceSettingsExpander.width * 0.3
                            }
                            Slider {
                                id: rateSlider
                                enabled: tts.state === TextToSpeech.Ready
                                from: -1.0
                                to: 1.0
                                stepSize: 0.5
                                value: 0
                                font.pointSize: 16
                                Layout.fillWidth: true
                                Binding {
                                    target: tts
                                    property: "rate"
                                    value: rateSlider.value
                                }
                            }
                        }
                    }
                ]
            }
        }
    }
}
