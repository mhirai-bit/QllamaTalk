// main.cpp

#include <cstring>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QDirIterator>
#include "app_environment.h"

// リソースシステムに登録されているファイルのパスをすべて列挙する関数
static void printAllResourcePaths()
{
    qDebug() << "----- List of all registered resources -----";
    // ":/" という仮想ディレクトリから下位を再帰的に探索する
    QDirIterator it(":/", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        qDebug() << path;
    }
    qDebug() << "--------------------------------------------";
}

int main(int argc, char ** argv) {
    set_qt_environment();

    QGuiApplication app(argc, argv);

#ifdef Q_OS_IOS
    // iOS
    QQuickStyle::setStyle("iOS");
#elif defined(Q_OS_MACOS)
    // macOS
    QQuickStyle::setStyle("macOS");
#elif defined(Q_OS_WIN)
    // Windows
    QQuickStyle::setStyle("FluentWinUI3");
#elif defined(Q_OS_ANDROID)
    // Android
    QQuickStyle::setStyle("Material");
#elif defined(Q_OS_LINUX)
    // Linux
    QQuickStyle::setStyle("Fusion");
#else
    // その他
    QQuickStyle::setStyle("Fusion");
#endif

    qDebug() << "style:" << QQuickStyle::name();

    // リソースパスをすべてプリントアウト
    printAllResourcePaths();

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
