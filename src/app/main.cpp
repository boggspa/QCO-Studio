#include "app/Logging.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("QimageMax"));
  QCoreApplication::setApplicationName(QStringLiteral("Qimage Max"));
  QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

  qmx::app::installMessageHandler();

  qmx::ui::MainWindow window;
  window.show();

  return QApplication::exec();
}
