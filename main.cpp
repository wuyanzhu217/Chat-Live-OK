#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>

#include "streaming/call/CallSessionController.h"
#include "streaming/render/LocalVideoRenderer.h"
#include "streaming/render/RemoteVideoRenderer.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<CallSessionController>("ccvWriter", 1, 0, "CallSessionController");
    qmlRegisterType<LocalVideoRenderer>("ccvWriter", 1, 0, "LocalVideoView");
    qmlRegisterType<RemoteVideoRenderer>("ccvWriter", 1, 0, "RemoteVideoView");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("ccvWriter", "Main");

    return app.exec();
}
