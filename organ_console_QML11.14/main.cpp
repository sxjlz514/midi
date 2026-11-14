/**
 * @file main.cpp
 * @brief 程序入口：创建 QApplication 与 MainWindow。
 */

#include "mainwindow.h"
#include "waterfallitem.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <cstdlib>
#include <QtQml>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("organ_console"));
    app.setOrganizationName(QStringLiteral("shengxuejisuan"));

    // Windows 原生样式不允许改 Button/CheckBox 的 background；改用 Basic 才能自定义外观。
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    qmlRegisterType<WaterfallItem>("Organ", 1, 0, "WaterfallItem");

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
