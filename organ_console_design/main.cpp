#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <cstdio>

#include "backend/consolebridge.h"

static void messageHandler(QtMsgType type, const QMessageLogContext &context,
                           const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    fprintf(stderr, "%s\n", msg.toUtf8().constData());
    fflush(stderr);
    QFile file(QDir(QCoreApplication::applicationDirPath()).filePath("debug.log"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write((QDateTime::currentDateTime().toString("hh:mm:ss ") + msg + QLatin1Char('\n'))
                       .toUtf8());
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(messageHandler);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("organ_console_design"));
    app.setOrganizationName(QStringLiteral("organ_console"));

    ConsoleBridge console;
    console.initialize();

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
                         for (const QQmlError &warning : warnings) {
                             qWarning().noquote() << warning.toString();
                         }
                     });
    engine.rootContext()->setContextProperty(QStringLiteral("console"), &console);

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);

    const QStringList args = app.arguments();
    if (args.size() > 1 && QFileInfo::exists(args.at(1))) {
        const QString path = args.at(1);
        QTimer::singleShot(0, &console, [&console, path]() {
            console.openFileAuto(path);
        });
    }

    return app.exec();
}
