// // Copyright (C) 2021 The Qt Company Ltd.
// // SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <QDir>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

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

#include "app_environment.h"

int main(int argc, char ** argv) {

    printAllResources();

    set_qt_environment();  // プロジェクト独自の設定

    QGuiApplication app(argc, argv);

    // QMLを読み込む（GUIが必要な場合）
    QQmlApplicationEngine engine;
    const QUrl url(u"qrc:/qt/qml/Main/main.qml"_qs);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
        );

    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
    engine.addImportPath(":/");
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
