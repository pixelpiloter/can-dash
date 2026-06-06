// main.cpp
// CAN-Dash 仪表盘主程序

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqmlcontext.h>
#include <QtGlobal>
#include <QDebug>

#include "layer3/dashboard_backend.h"

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);
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

    // 设置 QML 搜索路径
    engine.addImportPath(QStringLiteral("qml"));
    engine.addImportPath(QStringLiteral("."));

    // 加载主 QML（从文件系统）
    const QUrl url(QStringLiteral("qml/DashboardMain.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl) QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    engine.load(url);

    return app.exec();
}
