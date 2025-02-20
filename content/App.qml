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

            OperationPhaseIcon {
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
                LlamaChatEngine.operationPhase = 4 //LlamaChatEngine.WaitingUserInput
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
            LlamaChatEngine.operationPhase = 5 //LlamaChatEngine.Speaking
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
                property string inputIpAddress: ""
                property int portNumber: 0
                model: [
                    Component {
                        RowLayout {
                            id: engineModeRowLayout
                            width: connectionSettingsExpander.delegateWidth
                            spacing: 8
                            Label {
                                id: modeLabel
                                text: qsTr("Engine Mode:")
                                font.pointSize: 16
                                Layout.alignment: Qt.AlignVCenter
                            }
                            ComboBox {
                                id: modeCombo
                                Layout.fillWidth: true
                                model: [qsTr("Local"), qsTr("Remote")]
                                currentIndex: mainWindow.isRemote ? 1 : 0
                                font.pointSize: 16
                                onCurrentIndexChanged: {
                                    connectionSettingsExpander.modeComboCurrentIndex = modeCombo.currentIndex
                                }
                            }
                        }
                    },
                    Component {
                        // IP入力フィールド (regularExpressionValidatorの例)
                        RowLayout {
                            width: connectionSettingsExpander.delegateWidth
                            TextField {
                                id: ipField
                                Layout.fillWidth: true
                                placeholderText: qsTr("Enter server IP (e.g. 192.168.0.220)")
                                enabled: connectionSettingsExpander.modeComboCurrentIndex === 1
                                validator: RegularExpressionValidator {
                                    // IPv4用の簡易的な正規表現例
                                    regularExpression: /^(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)$/
                                }
                                font.pointSize: 16
                                onTextChanged: {
                                    connectionSettingsExpander.inputIpAddress = ipField.text
                                }
                            }
                        }
                    },
                    Component {
                        // ポート入力フィールド (IntValidatorで範囲チェック)
                        RowLayout {
                            width: connectionSettingsExpander.delegateWidth
                            TextField {
                                id: portField
                                Layout.fillWidth: true
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
                property bool voiceRecognitionEnabled: false

                onVoiceRecognitionEnabledChanged: {
                    if(voiceRecognitionEnabled) {
                        LlamaChatEngine.initiateVoiceRecognition()
                    } else {
                        LlamaChatEngine.stopVoiceRecognition()
                    }
                }

                Connections {
                    target: LlamaChatEngine
                    function onDetectedVoiceLocaleChanged() {
                        // ユーザーが喋った言語が LlamaChatEngine によって検出された
                        let detectedLocale = LlamaChatEngine.detectedVoiceLocale

                        // 安全策：無効なロケールならスキップ
                        if (!detectedLocale || detectedLocale.language === "") {
                            // TODO: エラーダイアログを表示
                            console.log("Detected locale is invalid")
                            return
                        }

                        // TextToSpeech が今使っているエンジンを覚えておく
                        let originalEngine = tts.engine

                        // ロケールを設定しようと試みる補助関数
                        //  - 現在のエンジンで tts.availableLocales() をチェックし、
                        //    "同じ言語" を持つ QLocale があれば tts.locale にセットして true を返す
                        //  - 見つからなければ false を返す
                        function trySetTtsLocale(locale) {
                            let locales = tts.availableLocales()
                            console.log("locales = ", locales)
                            for (let i = 0; i < locales.length; i++) {
                                // QMLではオブジェクト比較が難しいため、language や name() を比較するのが簡単
                                if (locales[i].name === locale.name) {
                                    tts.locale = locales[i]
                                    // UI 表示を最新化
                                    voiceSettingsExpander.updateLocales()
                                    voiceSettingsExpander.updateVoices()
                                    return true
                                }
                            }
                            return false
                        }

                        // 1) まずは現在のエンジンで試してみる
                        if (trySetTtsLocale(detectedLocale)) {
                            return
                        }

                        // 2) 対応していなければ、ほかのエンジンを順に試す
                        let engines = tts.availableEngines()
                        for (let i = 0; i < engines.length; i++) {
                            // 今のエンジンは既に試したのでスキップ
                            if (engines[i] === originalEngine) {
                                continue
                            }

                            // エンジン切り替え
                            tts.engine = engines[i]
                            // 再度トライ
                            if (trySetTtsLocale(detectedLocale)) {
                                return
                            }
                        }

                        // 3) すべてのエンジンを試してもダメなら、元のエンジンに戻してエラーダイアログ
                        tts.engine = originalEngine

                        // ここでは例として、すでにあるエラーダイアログを再利用か、
                        // または別のダイアログを用意してもOK
                        remoteAIErrorDialog.errorMessage = "No TTS engine found that supports: "
                                + detectedLocale.nativeLanguageName
                        remoteAIErrorDialog.text = qsTr("No Speech Engine Found")
                        remoteAIErrorDialog.informativeText = qsTr("Failed to find a TTS engine supporting this language")
                        remoteAIErrorDialog.open()
                    }
                }

                Component.onCompleted: {
                    // some engines initialize asynchronously
                    if (tts.state == TextToSpeech.Ready) {
                        engineReady()
                    } else {
                        tts.stateChanged.connect(voiceSettingsExpander.engineReady)
                    }
                }

                function engineReady() {
                    tts.stateChanged.disconnect(voiceSettingsExpander.engineReady)
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
                                text: qsTr("Enable Voice Recognition")
                                font.pointSize: 16
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: voiceSettingsExpander.width * 0.3
                            }
                            Switch {
                                id: voiceRecognitionSwitch
                                enabled: tts.state === TextToSpeech.Ready
                                Binding {
                                    target: voiceSettingsExpander
                                    property: "voiceRecognitionEnabled"
                                    value: voiceRecognitionSwitch.checked
                                }
                            }
                        }
                    },
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
                                    // 初期インデックスをセット
                                    currentIndex = 0
                                    // すでに tts.engine が "mock" になっていた場合は、
                                    // このコンボボックスの先頭要素に差し替える
                                    if (tts.engine === "mock" && model.length > 0) {
                                        tts.engine = textAt(currentIndex)
                                    } else {
                                        // そうでなければ、現在のエンジンに合うindexに補正
                                        currentIndex = model.indexOf(tts.engine)
                                        // もし見つからない場合は0でOK
                                        if (currentIndex === -1 && model.length > 0) {
                                            currentIndex = 0
                                            tts.engine = textAt(currentIndex)
                                        }
                                    }
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
