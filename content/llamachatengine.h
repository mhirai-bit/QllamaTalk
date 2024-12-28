#ifndef LLAMACHATENGINE_H
#define LLAMACHATENGINE_H

#include <QObject>
#include <QQmlEngine>
#include "llama.h"
#include "chatmessagemodel.h"
#include "llamaresponsegenerator.h"

class LlamaChatEngine: public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(ChatMessageModel *messages READ messages CONSTANT)
    Q_PROPERTY(QString user_input READ user_input WRITE setUser_input RESET resetUser_input NOTIFY user_inputChanged FINAL)
public:
    explicit LlamaChatEngine(QObject *parent = nullptr);
    ~LlamaChatEngine() override;

    ChatMessageModel *messages();

    QString user_input() const;
    Q_INVOKABLE void setUser_input(const QString &newUser_input);
    void resetUser_input();

public slots:
    void handle_new_user_input();

signals:
    void user_inputChanged();
    void requestGeneration(const QString &prompt);

private slots:
    // 生成スレッドからの通知を受け取るスロット
    void onPartialResponse(const QString &textSoFar);
    void onGenerationFinished(const QString &finalResponse);

private:
    static constexpr int m_ngl {99};
    static constexpr int m_n_ctx = {2048};
    static const std::string m_model_path;
    llama_model_params m_model_params;
    llama_model* m_model;
    llama_context_params m_ctx_params;
    llama_context* m_ctx;

    LlamaResponseGenerator* m_response_generator;

    bool m_inProgress {false};
    int m_currentAssistantIndex {-1};

    // Exposed to QML
    QString m_user_input;
    ChatMessageModel m_messages;

    void generate(const std::string &prompt, std::string& response);
};

#endif // LLAMACHATENGINE_H
