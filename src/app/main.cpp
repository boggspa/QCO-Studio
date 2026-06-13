#include "app/Logging.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("QCOStudio"));
  QCoreApplication::setApplicationName(QStringLiteral("QCO Studio"));
  QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

  qco::app::installMessageHandler();

  qco::ui::MainWindow window;
  window.show();

  return QApplication::exec();
}
