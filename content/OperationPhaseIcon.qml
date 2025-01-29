import QtQuick
import QtQuick.VectorImage

VectorImage {
    id: root

    SequentialAnimation {
        running: true
        loops: Animation.Infinite
        NumberAnimation {
            target: root
            property: "scale"
            duration: 1000
            from: 1
            to: 1.1
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: root
            property: "scale"
            duration: 1000
            from: 1.1
            to: 1
            easing.type: Easing.OutCubic
        }
    }

    states: [
        State {
            name: "Listening"
            when: LlamaChatEngine.operationPhase == 0 //LlamaChatEngine.Listening
            PropertyChanges {
                target: root
                source: "icons/listening_phase.svg"
            }
        },
        State {
            name: "VadRunning"
            when: LlamaChatEngine.operationPhase == 1 //LlamaChatEngine.VadRunning
            PropertyChanges {
                target: root
                source: "icons/vad_running_phase.svg"
            }
        },
        State {
            name: "WhisperRunning"
            when: LlamaChatEngine.operationPhase == 2 //LlamaChatEngine.WhisperRunning
            PropertyChanges {
                target: root
                source: "icons/whisper_running_phase.svg"
            }
        },
        State {
            name: "LlamaRunning"
            when: LlamaChatEngine.operationPhase == 3 //LlamaChatEngine.LlamaRunning
            PropertyChanges {
                target: root
                source: "icons/llama_running_phase.svg"
            }
        },
        State {
            name: "WaitingUserInput"
            when: LlamaChatEngine.operationPhase == 4 //LlamaChatEngine.WaitingUserInput
            PropertyChanges {
                target: root
                source: "icons/waiting_user_input_phase.svg"
            }
        },
        State {
            name: "Speaking"
            when: LlamaChatEngine.operationPhase == 5 //LlamaChatEngine.Speaking
            PropertyChanges {
                target: root
                source: "icons/speaking_phase.svg"
            }
        }
    ]
}
