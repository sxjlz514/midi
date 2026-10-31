/**
 * @file main.cpp
 * @brief 程序入口：创建 QApplication 与 MainWindow。
 */

#include "mainwindow.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <cstdlib>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("organ_console_linux"));
    app.setOrganizationName(QStringLiteral("shengxuejisuan"));

    MainWindow backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("console"), &backend);
    const QUrl mainUrl(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [mainUrl](QObject *object, const QUrl &url) {
        if (!object && url == mainUrl) {
            QCoreApplication::exit(EXIT_FAILURE);
        }
    }, Qt::QueuedConnection);
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }
    return app.exec();
}
