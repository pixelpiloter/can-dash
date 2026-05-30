// main.cpp
// CAN-Dash 仪表盘主程序

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqmlcontext.h>
#include <QtGlobal>
#include <QDebug>

#include "layer3/dashboard_backend_qt.h"

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QCoreApplication app(argc, argv);
    app.setApplicationName("can-dash");
    app.setApplicationVersion("1.0.0");

    qDebug() << "CAN-Dash starting...";

    // 初始化 DashboardBackend（Layer 3）
    DashboardBackend backend;
    backend.init();

    // QML 引擎
    QQmlApplicationEngine engine;

    // 注册 Layer 3 类型到 QML
    qmlRegisterType<DashboardBackend>("CanDash", 1, 0, "DashboardBackend");

    // 暴露 backend 到 QML 上下文
    engine.rootContext()->setContextProperty("dashboard", &backend);

    // 加载主 QML
    const QUrl url(QStringLiteral("qrc:/ui/DashboardMain.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl) {
                qFatal("Failed to load QML: %s", url.toString().toUtf8().constData());
            }
        },
        Qt::QueuedConnection
    );

    engine.load(url);

    return app.exec();
}
