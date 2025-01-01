#ifndef LLAMACHATENGINE_H
#define LLAMACHATENGINE_H

#include <QObject>
#include <QQmlEngine>
#include "llama.h"
#include "chatmessagemodel.h"
#include "llamaresponsegenerator.h"

// This class serves as the main controller for the chat logic.
// It interacts with a ChatMessageModel to store messages
// and uses LlamaResponseGenerator to generate AI responses.
class LlamaChatEngine : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Q_PROPERTY: Exposes a chat message model and a user_input string to QML.
    Q_PROPERTY(ChatMessageModel* messages READ messages CONSTANT)
    Q_PROPERTY(QString user_input READ user_input WRITE setUser_input RESET resetUser_input NOTIFY user_inputChanged FINAL)
    Q_PROPERTY(bool engine_initialized READ engine_initialized NOTIFY engine_initializedChanged FINAL)
public:
    explicit LlamaChatEngine(QObject *parent = nullptr);
    ~LlamaChatEngine() override;

    // Returns the chat message model used by QML for display.
    ChatMessageModel* messages();
    // Getter/setter for the user's input text.
    QString user_input() const;
    Q_INVOKABLE void setUser_input(const QString &newUser_input);
    void resetUser_input();

    bool engine_initialized() const;

public slots:
    // Triggered when user_input changes. Prepares the prompt and requests generation.
    void handle_new_user_input();

signals:
    // Notifies QML that user_input has changed.
    void user_inputChanged();
    // Request generation of a response from the LlamaResponseGenerator.
    void requestGeneration(const QString &prompt);

    void engine_initializedChanged();

private slots:
    // Updates the UI when partial responses arrive from the generator.
    void onPartialResponse(const QString &textSoFar);
    // Finalizes the UI update when the generation finishes.
    void onGenerationFinished(const QString &finalResponse);
    // Finalizes the initialization of the LLaMA model and context.
    void onEngineInitFinished();

private:
    // Configuration for LLaMA model usage.
    static constexpr int m_ngl {99};
    static constexpr int m_n_ctx {2048};
    static const std::string m_model_path;
    llama_model_params m_model_params;
    llama_model* m_model;
    llama_context_params m_ctx_params;
    llama_context* m_ctx;

    // Worker object for text generation in another thread.
    LlamaResponseGenerator* m_response_generator;

    // Tracks whether a response is currently being generated.
    bool m_inProgress {false};
    // Index of the last AI message in the ChatMessageModel.
    int m_currentAssistantIndex {-1};

    // The user's current input text (exposed to QML).
    QString m_user_input;
    // The model holding all chat messages (system, user, assistant).
    ChatMessageModel m_messages;
    // Whether the engine has been initialized.
    bool m_engine_initialized {false};

    // Optional helper for synchronous generation (if needed).
    void generate(const std::string &prompt, std::string &response);
    // Initializes the LLaMA model and context for text generation.
    void doEngineInit();
    // Private, as it's only called by this class, not from QML
    void setEngine_initialized(bool newEngine_initialized);
};

#endif // LLAMACHATENGINE_H
