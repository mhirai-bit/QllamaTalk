#ifndef LLAMA_RESPONSE_GENERATOR_H
#define LLAMA_RESPONSE_GENERATOR_H

#include <QObject>
#include <QString>
#include "llama.h"

// This class handles text generation for LLaMA-based models.
// It is typically used in a worker thread to generate responses asynchronously.
class LlamaResponseGenerator : public QObject {
    Q_OBJECT

public:
    // Disallow default construction to ensure model/context are always provided.
    LlamaResponseGenerator() = delete;

    // Constructor expects a LLaMA model, a context, and an optional parent.
    // 'parent' may be nullptr when the object is moved to another thread.
    LlamaResponseGenerator(QObject *parent = nullptr,
                           llama_model* model = nullptr,
                           llama_context* ctx = nullptr);

    // Cleans up any allocated resources (e.g. the sampler) on destruction.
    ~LlamaResponseGenerator() override;

public slots:
    // Generates text from the provided prompt, emitting partial and final results.
    void generate(const QString &prompt);

signals:
    // Emitted periodically with incremental output during generation.
    void partialResponseReady(const QString &textSoFar);

    // Emitted once the entire generation process is complete.
    void generationFinished(const QString &finalResponse);

    // Emitted in case of an error (e.g., tokenization failure).
    void generationError(const QString &errorMessage);

private:
    // References to the LLaMA model and context used for generation.
    llama_model* m_model {nullptr};
    llama_context* m_ctx {nullptr};
    llama_sampler* m_sampler {nullptr};

    // Initializes the sampler (temperature, min_p, etc.) before text generation.
    void initializeSampler();
};

#endif // LLAMA_RESPONSE_GENERATOR_H
