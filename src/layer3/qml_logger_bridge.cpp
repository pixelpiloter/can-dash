// qml_logger_bridge.cpp
// C++ → QML 日志桥接单例实现

#include "qml_logger_bridge.h"
#include <QTimer>
#include <QDebug>
#include <QVariant>

QmlLoggerBridge::QmlLoggerBridge(QObject* parent)
    : QObject(parent)
{
}

QmlLoggerBridge::~QmlLoggerBridge() {
}

QmlLoggerBridge& QmlLoggerBridge::instance() {
    static QmlLoggerBridge inst;
    return inst;
}

void QmlLoggerBridge::registerQmlLogger(QObject* logger) {
    m_logger = logger;
    qDebug() << "[QmlLoggerBridge] QML logger registered:" << logger;
}

void QmlLoggerBridge::forwardLog(int level, const QString& source, const QString& message) {
    if (!m_logger) {
        // QML logger not registered yet - discard
        return;
    }

    // Use Qt::QueuedConnection for thread safety
    QTimer::singleShot(0, m_logger, [=]() {
        QVariant lv = level;
        QVariant src = source;
        QVariant msg = message;
        QMetaObject::invokeMethod(m_logger, "_cppAddLog",
                                  Q_ARG(QVariant, lv),
                                  Q_ARG(QVariant, src),
                                  Q_ARG(QVariant, msg));
    });
}

void QmlLoggerBridge::invokeCppAddLog(QVariant level, QVariant source, QVariant message) {
    if (!m_logger) return;
    QMetaObject::invokeMethod(m_logger, "_cppAddLog",
                              Q_ARG(QVariant, level),
                              Q_ARG(QVariant, source),
                              Q_ARG(QVariant, message));
}
