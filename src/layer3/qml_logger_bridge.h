// qml_logger_bridge.h
// C++ → QML 日志桥接单例（QObject，可暴露到 QML 上下文）

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

class QmlLoggerBridge : public QObject {
    Q_OBJECT
public:
    static QmlLoggerBridge& instance();

    // 禁止拷贝/移动
    QmlLoggerBridge(const QmlLoggerBridge&) = delete;
    QmlLoggerBridge& operator=(const QmlLoggerBridge&) = delete;

    // level: 0=DEBUG 1=INFO 2=WARN 3=ERROR
    Q_INVOKABLE void forwardLog(int level, const QString& source, const QString& message);

    // QML 端注册回调对象
    Q_INVOKABLE void registerQmlLogger(QObject* logger);

public slots:
    void invokeCppAddLog(QVariant level, QVariant source, QVariant message);

signals:
    void logForwarded(int level, const QString& source, const QString& message);

private:
    explicit QmlLoggerBridge(QObject* parent = nullptr);
    ~QmlLoggerBridge();

    QObject* m_logger = nullptr;
};
