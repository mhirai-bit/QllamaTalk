#ifndef OPERATIONPHASE_H
#define OPERATIONPHASE_H
enum OperationPhase {
    Listening,       // 音声を聞いている状態
    VadRunning,      // VAD実行中
    WhisperRunning,  // whisper推論中
    LlamaRunning,    // llama推論中
    WaitingUserInput, // ユーザー入力待ち
    Speaking
};

#endif // OPERATIONPHASE_H
