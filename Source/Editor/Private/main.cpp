#include <QApplication>
#include <QDirIterator>
#include "UI/EditorMainWindow.h"
#include <QMutex>

enum LogLevel {
    LogDebug,
    LogInfo,
    LogWarning,
    LogCritical,
    LogFatal
};

LogLevel currentLogLevel = LogDebug;

QFile logFile;
QMutex logMutex;

void customDebug(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QMutexLocker locker(&logMutex);

    QByteArray localMsg = msg.toLocal8Bit();
    QString logLevelStr;
    LogLevel msgLevel;

    switch (type) {
        case QtDebugMsg:
            msgLevel = LogDebug;
            logLevelStr = "DEBUG";
            break;
        case QtInfoMsg:
            msgLevel = LogInfo;
            logLevelStr = "INFO";
            break;
        case QtWarningMsg:
            msgLevel = LogWarning;
            logLevelStr = "WARNING";
            break;
        case QtCriticalMsg:
            msgLevel = LogCritical;
            logLevelStr = "CRITICAL";
            break;
        case QtFatalMsg:
            msgLevel = LogFatal;
            logLevelStr = "FATAL";
            break;
    }

    // 过滤日志等级
    if (msgLevel < currentLogLevel)
        return;

    QString logMsg = QString("%1 [%2] %3 (%4:%5, %6)\n")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
            .arg(logLevelStr)
            .arg(localMsg.constData())
            .arg(context.file ? context.file : "")
            .arg(context.line)
            .arg(context.function ? context.function : "");

    QTextStream out(&logFile);
    out << logMsg;
    out.flush();

    if (type == QtFatalMsg)
        abort();
}

int main(int argc, char *argv[]) {
    logFile.setFileName("app.log");
    logFile.open(QIODevice::Append | QIODevice::Text);
    currentLogLevel = LogWarning;
    qInstallMessageHandler(customDebug);

    QApplication app(argc, argv);

    EditorMainWindow Window;
    Window.show();

    return app.exec();
}
