#ifndef LLAMA_RESPONSE_GENERATOR_H
#define LLAMA_RESPONSE_GENERATOR_H

#include <QObject>
#include <QString>

#include "llama.h"

class LlamaResponseGenerator : public QObject
{
    Q_OBJECT
public:
    LlamaResponseGenerator() = delete;
    LlamaResponseGenerator(QObject *parent = nullptr, llama_model* model = nullptr, llama_context* ctx = nullptr);
    ~LlamaResponseGenerator() override;

public slots:
    // "generate" スロット: QMLやメインスレッド側から呼び出してもらう想定
    // スレッド内で動作し、文字列を段階的に通知していく
    void generate(const QString &prompt);
signals:
    // 部分的に生成された文字列を通知するシグナル
    // たとえばトークンごと、または一定文字数ごと、行ごとなど
    void partialResponseReady(const QString &textSoFar);

    // 生成処理が完了したら通知するシグナル
    void generationFinished(const QString &finalResponse);

    // もしエラー等を通知したい場合はこういうシグナルを用意
    void generationError(const QString &errorMessage);

private:
    llama_model* m_model {nullptr};
    llama_context* m_ctx {nullptr};
    llama_sampler* m_sampler {nullptr};

    void initializeSampler();
};

#endif // LLAMA_RESPONSE_GENERATOR_H
