#include "app/Logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdlib>
#include <cstdio>
#include <memory>

namespace qmx::app {
namespace {

QMutex logMutex;
std::unique_ptr<QFile> logFile;
QString currentLogFilePath;

const char* levelName(QtMsgType type)
{
  switch (type) {
    case QtDebugMsg:
      return "debug";
    case QtInfoMsg:
      return "info";
    case QtWarningMsg:
      return "warning";
    case QtCriticalMsg:
      return "critical";
    case QtFatalMsg:
      return "fatal";
  }
  return "unknown";
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
  const auto line = QStringLiteral("%1 [%2] %3 (%4:%5)\n")
                      .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
                      .arg(QString::fromLatin1(levelName(type)))
                      .arg(message)
                      .arg(QString::fromLatin1(context.file ? context.file : "unknown"))
                      .arg(context.line);

  {
    QMutexLocker locker(&logMutex);
    if (logFile && logFile->isOpen()) {
      QTextStream stream(logFile.get());
      stream << line;
      stream.flush();
    }
  }

  std::fputs(line.toLocal8Bit().constData(), stderr);

  if (type == QtFatalMsg) {
    std::abort();
  }
}

}  // namespace

void installMessageHandler()
{
  const auto dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const auto logRoot = dataRoot.isEmpty() ? QDir::tempPath() + QStringLiteral("/QimageMax/logs")
                                          : dataRoot + QStringLiteral("/logs");

  QDir().mkpath(logRoot);
  currentLogFilePath = logRoot + QStringLiteral("/qimagemax.log");

  auto file = std::make_unique<QFile>(currentLogFilePath);
  if (file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    logFile = std::move(file);
  }

  qInstallMessageHandler(messageHandler);
}

QString logFilePath()
{
  return currentLogFilePath;
}

}  // namespace qmx::app
