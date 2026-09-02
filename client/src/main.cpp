#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QDebug>

#include "ViewModelRegistry.h"

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle("Material");

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    // 装配 ViewModel/Service 到 QML 上下文（含 VideoFrameProvider 注册）
    ViewModelRegistry registry(engine, app);
    registry.registerAll();

    const QUrl url(QStringLiteral("qrc:/views/MainView.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qWarning() << "Failed to load QML file:" << url.toString();
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
