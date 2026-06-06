// file_logger.cpp
// FileLogger 单例实现

#include "file_logger.h"
#include "qml_logger_bridge.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QMutexLocker>
#include <QCoreApplication>

// 静态成员初始化
const QString FileLogger::kLogDir = QStringLiteral("/tmp/can-dash");
const QString FileLogger::kLogFile = QStringLiteral("dashboard.log");
const int FileLogger::kMaxFileSize = 1024 * 1024;        // 1MB
const int FileLogger::kMaxBackups = 3;
// kFlushThreshold is inline in header (C++17)

FileLogger& FileLogger::instance() {
    static FileLogger inst;
    return inst;
}

FileLogger::FileLogger() : m_buffer() {
    ensureDirectory();
    m_filePath = kLogDir + QStringLiteral("/") + kLogFile;
}

FileLogger::~FileLogger() {
    // 程序退出前 flush 剩余缓冲
    if (!m_buffer.isEmpty()) {
        QFile file(m_filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream ts(&file);
            ts << m_buffer;
            ts.flush();
            file.flush();
            file.close();
        }
    }
}

// 级别数字到字符串
static const char* levelToString(int level) {
    switch (level) {
        case 0: return "DEBUG";
        case 1: return "INFO ";
        case 2: return "WARN ";
        case 3: return "ERROR";
        default: return "UNK  ";
    }
}

void FileLogger::info(const QString& source, const QString& message) {
    log(1, source, message);
}

void FileLogger::warn(const QString& source, const QString& message) {
    log(2, source, message);
}

void FileLogger::error(const QString& source, const QString& message) {
    log(3, source, message);
}

void FileLogger::debug(const QString& source, const QString& message) {
    log(0, source, message);
}

void FileLogger::log(int level, const QString& source, const QString& message) {
    writeImpl(level, source, message);
}

void FileLogger::qmlAddLog(int level, const QString& source, const QString& message) {
    instance().log(level, source, message);
}

void FileLogger::writeImpl(int level, const QString& source, const QString& message) {
    // ISO 8601 时间戳（UTC）
    QString ts_str = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);

    // 构造一行日志
    QString line = QStringLiteral("[%1] %2 %3 | %4\n")
                       .arg(ts_str, levelToString(level), source, message);

    QMutexLocker locker(&m_mutex);

    m_buffer += line;

    // 缓冲达到 1KB 或 ERROR 级别立即 flush
    if (m_buffer.size() >= kFlushThreshold || level == 3) {
        rotateIfNeeded();

        QFile file(m_filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream ts(&file);
            ts << m_buffer;
            ts.flush();
            file.flush();
            file.close();
        }
        m_buffer.clear();
    }
}

void FileLogger::ensureDirectory() {
    QDir dir(kLogDir);
    if (!dir.exists()) {
        dir.mkpath(kLogDir);
    }
}

void FileLogger::rotateIfNeeded() {
    QFile file(m_filePath);
    if (file.exists() && file.size() >= kMaxFileSize) {
        file.close();

        // 删除最旧的备份
        QString oldest = QStringLiteral("%1/%2.%3")
                            .arg(kLogDir, kLogFile).arg(kMaxBackups);
        QFile::remove(oldest);

        // 轮转备份 dashboard.log.2 -> dashboard.log.3, etc.
        for (int i = kMaxBackups - 1; i >= 1; --i) {
            QString src = QStringLiteral("%1/%2.%3")
                             .arg(kLogDir, kLogFile).arg(i);
            QString dst = QStringLiteral("%1/%2.%3")
                             .arg(kLogDir, kLogFile).arg(i + 1);
            QFile::rename(src, dst);
        }

        // 当前日志 -> .1
        QString first = QStringLiteral("%1/%2.%3")
                            .arg(kLogDir, kLogFile).arg(1);
        QFile::rename(m_filePath, first);
    }
}

// ─── qInstallMessageHandler 集成 ─────────────────────────────────────────────

// 前向声明（供 Qt 消息处理函数使用）
static FileLogger* s_loggerInstance = nullptr;

// Qt 消息处理函数
static void qtMessageHandler(QtMsgType type, const QMessageLogContext& /*context*/, const QString& msg) {
    if (!s_loggerInstance) return;

    // 转换 Qt 级别到我们的级别
    // QtDebugMsg=0, QtInfoMsg=1, QtWarningMsg=2, QtCriticalMsg=3, QtFatalMsg=4
    int level = 0; // 默认 DEBUG
    switch (type) {
        case QtDebugMsg:   level = 0; break;
        case QtInfoMsg:    level = 1; break;
        case QtWarningMsg: level = 2; break;
        case QtCriticalMsg: {
            // QtCritical 仍然输出到 stderr（是 fatal）
            fprintf(stderr, "[CRITICAL] %s\n", msg.toUtf8().constData());
            level = 3;
            break;
        }
        case QtFatalMsg: {
            fprintf(stderr, "[FATAL] %s\n", msg.toUtf8().constData());
            level = 3;
            break;
        }
    }

    // source 固定为 "Qt"
    s_loggerInstance->log(level, QStringLiteral("Qt"), msg);
    // 同时转发到 QML 日志面板 (QmlLoggerBridge 会自动丢弃直到 QML 注册)
    QmlLoggerBridge::instance().forwardLog(level, QStringLiteral("Qt"), msg);
}

void FileLogger::installMessageHandler() {
    s_loggerInstance = this;
    qInstallMessageHandler(qtMessageHandler);
}
