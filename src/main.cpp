// // Copyright (C) 2021 The Qt Company Ltd.
// // SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

// #include <QGuiApplication>
// #include <QQmlApplicationEngine>
// #include <QProcess>
// #include <QDebug>
// #include <QTextStream>
// #include <QIODevice>
// #include <QTimer>

// // あなたの環境に合わせた独自のヘッダ (必要なら)
// #include "app_environment.h"
// #include "import_qml_components_plugins.h"
// #include "import_qml_plugins.h"

// //---------------------------------------------------------------------------
// // グローバル（PoC用）。本番では適切なクラスにメンバとして持つなど検討を。
// QProcess* g_llamaProc = nullptr;

// //---------------------------------------------------------------------------
// // 1秒後に呼び出して llama-simple-chat の標準出力を取得する関数
// void delayedReadLlamaOutput()
// {
//     if (!g_llamaProc) {
//         qWarning() << "[Error] llama-simple-chat process is not running (delayedRead).";
//         return;
//     }

//     // 1秒後にまとめて取り出す
//     const QByteArray output = g_llamaProc->readAllStandardOutput();
//     if (!output.isEmpty()) {
//         QTextStream cout(stdout, QIODevice::WriteOnly);
//         // ここで標準出力へ表示
//         cout << "Assistant> " << QString::fromUtf8(output) << Qt::flush;
//     }
// }

// //---------------------------------------------------------------------------
// // llama-simple-chat に文字列を送る関数
// void sendUserInputToLlama(const QString &userInput)
// {
//     if (!g_llamaProc) {
//         qWarning() << "[Error] llama-simple-chat process is not running (sendUserInput).";
//         return;
//     }

//     // 改行付きで送信
//     QByteArray inputData = userInput.toUtf8() + "\n";
//     qint64 written = g_llamaProc->write(inputData);
//     if (written == -1) {
//         qWarning() << "Failed to write to llama-simple-chat process.";
//         return;
//     }

//     // 書き込みが完了するまで待機
//     if (!g_llamaProc->waitForBytesWritten(3000)) {
//         qWarning() << "Timeout or error while waiting for data to be written.";
//         return;
//     }

//     // ここで1秒後に delayedReadLlamaOutput() を呼び出す
//     // （&app を親にしておき、lambdaの中で delayedReadLlamaOutput() を呼ぶ）
//     QTimer::singleShot(1000, [=]() {
//         delayedReadLlamaOutput();
//     });
// }

// //---------------------------------------------------------------------------
// // llama-simple-chat の標準エラー(エラー出力) だけはリアルタイムで取りたい場合の関数
// void handleLlamaError()
// {
//     if (!g_llamaProc) return;

//     const QByteArray err = g_llamaProc->readAllStandardError();
//     if (!err.isEmpty()) {
//         QTextStream cout(stderr, QIODevice::WriteOnly);
//         cout << "[llama-simple-chat stderr] " << QString::fromUtf8(err) << Qt::endl;
//     }
// }

// //---------------------------------------------------------------------------

// int main(int argc, char *argv[])
// {
//     // もし QML UIが不要なら QCoreApplication でもOK
//     set_qt_environment();  // プロジェクト独自の設定

//     QGuiApplication app(argc, argv);

//     // QMLを読み込む（GUIが必要な場合）
//     QQmlApplicationEngine engine;
//     const QUrl url(u"qrc:/qt/qml/Main/main.qml"_qs);
//     QObject::connect(
//         &engine, &QQmlApplicationEngine::objectCreated,
//         &app, [url](QObject *obj, const QUrl &objUrl) {
//             if (!obj && url == objUrl)
//                 QCoreApplication::exit(-1);
//         },
//         Qt::QueuedConnection
//         );

//     engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
//     engine.addImportPath(":/");
//     engine.load(url);

//     if (engine.rootObjects().isEmpty()) {
//         return -1;
//     }

//     // ----------------------------------------------------------------------
//     // llama-simple-chat プロセスの起動
//     // ----------------------------------------------------------------------
//     g_llamaProc = new QProcess(&app);

//     // llama-simple-chat の実行ファイルパス（適宜修正）
//     // 例: "/Users/mainuser/Documents/Projects/QllamaTalk/3rdparty/llama.cpp/build/bin/llama-simple-chat"
//     QString llamaExecutable = "/Users/mainuser/Documents/Projects/QllamaTalk/3rdparty/llama.cpp/build/bin/llama-simple-chat";

//     // モデルファイルパス（適宜修正）
//     // 例: "/Users/mainuser/Documents/Projects/QllamaTalk/3rdparty/models/Llama-3.1-8B-Open-SFT.Q4_K_M.gguf"
//     QString modelFile = "/Users/mainuser/Documents/Projects/QllamaTalk/3rdparty/llama.cpp/models/Llama-3.1-8B-Open-SFT.Q4_K_M.gguf";

//     // 引数
//     QStringList arguments;
//     arguments << "-m" << modelFile;

//     g_llamaProc->setProgram(llamaExecutable);
//     g_llamaProc->setArguments(arguments);

//     // llama-simple-chat の標準エラーだけはリアルタイム処理にしておく
//     QObject::connect(g_llamaProc, &QProcess::readyReadStandardError, &handleLlamaError);

//     // 【ポイント】readyReadStandardOutput は使わない（シグナルで読むのをやめる）
//     // 代わりに1秒後に delayedReadLlamaOutput() を手動呼び出しするため、シグナル接続は削除/未使用

//     // プロセス開始
//     g_llamaProc->start();
//     if (!g_llamaProc->waitForStarted()) {
//         qWarning() << "Failed to start llama-simple-chat";
//     }

//     // ----------------------------------------------------------------------
//     // ターミナルでユーザー入力を受け取る (PoC)
//     // ----------------------------------------------------------------------
//     QTextStream cin(stdin, QIODevice::ReadOnly);
//     QTextStream cout(stdout, QIODevice::WriteOnly);

//     cout << "[ChatBot PoC with llama-simple-chat] Type 'exit' to quit.\n" << Qt::flush;

//     while (true) {
//         cout << "\nUser> " << Qt::flush;
//         const QString userInput = cin.readLine().trimmed();
//         if (userInput.isEmpty()) {
//             continue;
//         }
//         if (userInput.toLower() == "exit") {
//             cout << "[ChatBot PoC] Bye!\n" << Qt::endl;
//             break;
//         }

//         // llama-simple-chat に入力
//         sendUserInputToLlama(userInput);
//         // ↑の中で 1秒後に delayedReadLlamaOutput() が呼ばれる
//     }

//     // 終了処理
//     if (g_llamaProc) {
//         g_llamaProc->closeWriteChannel();  // EOFを送る
//         g_llamaProc->kill();              // 強制終了 or terminate()
//         g_llamaProc->waitForFinished();
//         g_llamaProc = nullptr;
//     }

//     return 0;
// }

#include "llama.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <QDir>
#include <QDebug>

void printAllResources(const QString &path = ":/")
{
    QDir dir(path);

    // ディレクトリ内容を取得 ('.' '..' は除外)
    const QStringList entries = dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);

    for (const QString &entry : entries) {
        // 例: path=":/", entry="images" → itemPath=":/images"
        //     path=":/images/", entry="icon.png" → itemPath=":/images/icon.png"
        QString itemPath = path + entry;

        // 出力
        qDebug() << itemPath;

        // ディレクトリ(サブフォルダ)なら再帰的に探索
        if (QDir(itemPath).exists()) {
            // ディレクトリの場合、末尾に "/" を付けて再帰呼び出し
            printAllResources(itemPath + "/");
        }
    }
}

static void print_usage(int, char ** argv) {
    printf("\nexample usage:\n");
    printf("\n    %s -m model.gguf [-c context_size] [-ngl n_gpu_layers]\n", argv[0]);
    printf("\n");
}

int main(int argc, char ** argv) {
    const std::string model_path {"/Users/mainuser/Documents/Projects/QllamaTalk/3rdparty/llama.cpp/models/Llama-3.1-8B-Open-SFT.Q4_K_M.gguf"};
    int ngl = 99;
    int n_ctx = 2048;

    if (model_path.empty()) {
        print_usage(argc, argv);
        return 1;
    }

    // only print errors
    llama_log_set([](enum ggml_log_level level, const char * text, void * /* user_data */) {
        if (level >= GGML_LOG_LEVEL_ERROR) {
            fprintf(stderr, "%s", text);
        }
    }, nullptr);

    // load dynamic backends
    ggml_backend_load_all();

    // initialize the model
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = ngl;

    llama_model * model = llama_load_model_from_file(model_path.c_str(), model_params);
    if (!model) {
        fprintf(stderr , "%s: error: unable to load model\n" , __func__);
        return 1;
    }

    // initialize the context
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = n_ctx;
    ctx_params.n_batch = n_ctx;

    llama_context * ctx = llama_new_context_with_model(model, ctx_params);
    if (!ctx) {
        fprintf(stderr , "%s: error: failed to create the llama_context\n" , __func__);
        return 1;
    }

    // initialize the sampler
    llama_sampler * smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // helper function to evaluate a prompt and generate a response
    auto generate = [&](const std::string & prompt) {
        std::string response;

        // tokenize the prompt
        const int n_prompt_tokens = -llama_tokenize(model, prompt.c_str(), prompt.size(), NULL, 0, true, true);
        std::vector<llama_token> prompt_tokens(n_prompt_tokens);
        if (llama_tokenize(model, prompt.c_str(), prompt.size(), prompt_tokens.data(), prompt_tokens.size(), llama_get_kv_cache_used_cells(ctx) == 0, true) < 0) {
            GGML_ABORT("failed to tokenize the prompt\n");
        }

        // prepare a batch for the prompt
        llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
        llama_token new_token_id;
        while (true) {
            // check if we have enough space in the context to evaluate this batch
            int n_ctx = llama_n_ctx(ctx);
            int n_ctx_used = llama_get_kv_cache_used_cells(ctx);
            if (n_ctx_used + batch.n_tokens > n_ctx) {
                printf("\033[0m\n");
                fprintf(stderr, "context size exceeded\n");
                exit(0);
            }

            if (llama_decode(ctx, batch)) {
                GGML_ABORT("failed to decode\n");
            }

            // sample the next token
            new_token_id = llama_sampler_sample(smpl, ctx, -1);

            // is it an end of generation?
            if (llama_token_is_eog(model, new_token_id)) {
                break;
            }

            // convert the token to a string, print it and add it to the response
            char buf[256];
            int n = llama_token_to_piece(model, new_token_id, buf, sizeof(buf), 0, true);
            if (n < 0) {
                GGML_ABORT("failed to convert token to piece\n");
            }
            std::string piece(buf, n);
            printf("%s", piece.c_str()); // h: pieceには、トークンごとの文字列が入る。このプログラムでは、pieceごとに出力している。
            fflush(stdout);
            response += piece;

            // prepare the next batch with the sampled token
            batch = llama_batch_get_one(&new_token_id, 1);
        }

        return response; // h: responseに完全な回答が入る。
    };

    std::vector<llama_chat_message> messages;
    std::vector<char> formatted(llama_n_ctx(ctx));
    int prev_len = 0;
    while (true) {
        // get user input
        printf("\033[32m> \033[0m");
        std::string user;
        std::getline(std::cin, user); // h: この"user"が入力文字列なので、これをQMLから取得する。

        if (user.empty()) {
            break;
        }

        // add the user input to the message list and format it
        messages.push_back({"user", strdup(user.c_str())});
        int new_len = llama_chat_apply_template(model, nullptr, messages.data(), messages.size(), true, formatted.data(), formatted.size());
        if (new_len > (int)formatted.size()) {
            formatted.resize(new_len);
            new_len = llama_chat_apply_template(model, nullptr, messages.data(), messages.size(), true, formatted.data(), formatted.size());
        }
        if (new_len < 0) {
            fprintf(stderr, "failed to apply the chat template\n");
            return 1;
        }

        // remove previous messages to obtain the prompt to generate the response
        std::string prompt(formatted.begin() + prev_len, formatted.begin() + new_len);

        // generate a response
        printf("\033[33m");
        std::string response = generate(prompt);
        printf("\n\033[0m");

        // add the response to the messages
        messages.push_back({"assistant", strdup(response.c_str())});
        prev_len = llama_chat_apply_template(model, nullptr, messages.data(), messages.size(), false, nullptr, 0); // h: コメントアウトしても問題なく動く。
        if (prev_len < 0) {
            fprintf(stderr, "failed to apply the chat template\n");
            return 1;
        }
    }

    // free resources
    for (auto & msg : messages) {
        free(const_cast<char *>(msg.content));
    }
    llama_sampler_free(smpl);
    llama_free(ctx);
    llama_free_model(model);

    return 0;
}
