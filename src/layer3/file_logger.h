// file_logger.h
// FileLogger 单例：写 /tmp/can-dash/dashboard.log，1MB x 3 轮转，线程安全
//
// 使用方式：
//   FileLogger::instance().info("ShmDataSource", "Opened shm at /dev/shm/can_display");
//   FileLogger::instance().warn("AlarmManager", "High temperature alarm triggered");
//   FileLogger::instance().error("CanProcessor", "Frame CRC mismatch");
//   FileLogger::instance().debug("FieldMonitor", "Value within normal range");

#pragma once

#include <QString>
#include <QMutex>
#include <atomic>

class FileLogger {
public:
    // 单例访问
    static FileLogger& instance();

    // 禁止拷贝/移动
    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    // 日志级别接口
    void info(const QString& source, const QString& message);
    void warn(const QString& source, const QString& message);
    void error(const QString& source, const QString& message);
    void debug(const QString& source, const QString& message);

    // qInstallMessageHandler 集成用：level 0=DEBUG 1=INFO 2=WARN 3=ERROR
    void log(int level, const QString& source, const QString& message);

    // QmlLogger::addLog 桥接用（静态方法，ui-agent T1 协拔）
    // level: 0=DEBUG 1=INFO 2=WARN 3=ERROR
    static void qmlAddLog(int level, const QString& source, const QString& message);

    // 安装 qInstallMessageHandler（内部调用，确保目录存在）
    void installMessageHandler();

private:
    FileLogger();
    ~FileLogger();

    // 内部写入（线程安全，已加锁）
    void writeImpl(int level, const QString& source, const QString& message);

    // 文件轮转检查
    void rotateIfNeeded();

    // 确保目录存在
    void ensureDirectory();

    static const QString kLogDir;
    static const QString kLogFile;
    static const int kMaxFileSize;    // 1MB
    static const int kMaxBackups;     // 3 个备份

    QMutex m_mutex;
    QString m_filePath;

    // 缓冲（1KB 后 flush）
    QString m_buffer;
    static const int kFlushThreshold = 1024;
};

