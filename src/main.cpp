// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QProcess>
#include <QDebug>

#include "app_environment.h"
#include "import_qml_components_plugins.h"
#include "import_qml_plugins.h"

int main(int argc, char *argv[])
{
    set_qt_environment();

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    const QUrl url(u"qrc:/qt/qml/Main/main.qml"_qs);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
    engine.addImportPath(":/");

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    // 1. whisperの動作確認
    QProcess whisperProc;
    // 例："whisper --help"を呼ぶ
    whisperProc.setProgram("/Users/mainuser/Documents/Projects/QllamaTalk/3rdparty/whisper.cpp/build/bin/whisper-cli");
    whisperProc.setArguments(QStringList() << "--help");

    qDebug() << "[PoC] Starting whisper process...";
    whisperProc.start();
    if (!whisperProc.waitForStarted()) {
        qDebug() << "[PoC] Failed to start whisper!";
    } else {
        // プロセス終了を待つ
        whisperProc.waitForFinished();
        QByteArray out = whisperProc.readAllStandardOutput();
        QByteArray err = whisperProc.readAllStandardError();
        qDebug() << "[PoC] whisper stdout: " << out;
        qDebug() << "[PoC] whisper stderr: " << err;
    }

    // 2. llama の動作確認
    {
        QProcess llamaProc;
        // 例: "llama --help" を呼ぶ
        // 実際は "/path/to/llama" に書き換え、引数も適宜変更
        llamaProc.setProgram("/Users/mainuser/Documents/Projects/QllamaTalk/3rdparty/llama.cpp/build/bin/llama-cli");
        llamaProc.setArguments(QStringList() << "--help");

        qDebug() << "[PoC] Starting llama process...";
        llamaProc.start();
        if (!llamaProc.waitForStarted()) {
            qDebug() << "[PoC] Failed to start llama!";
        } else {
            // プロセス終了を待つ
            llamaProc.waitForFinished();
            QByteArray out = llamaProc.readAllStandardOutput();
            QByteArray err = llamaProc.readAllStandardError();
            qDebug() << "[PoC] llama stdout:" << out;
            qDebug() << "[PoC] llama stderr:" << err;
        }
    }


    return app.exec();
}
