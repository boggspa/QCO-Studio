#include "app/Logging.h"
#include "ui/CanvasView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDockWidget>
#include <QImage>
#include <QMenuBar>
#include <QSettings>
#include <QToolBar>

#include <iostream>

namespace {

bool check(bool condition, const char* expression, const char* file, int line)
{
  if (condition) {
    return true;
  }

  std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
  return false;
}

[[nodiscard]] QString actionText(QAction* action)
{
  return action == nullptr ? QString() : action->text().remove(QLatin1Char('&'));
}

}  // namespace

#define CHECK(condition) \
  do { \
    if (!check((condition), #condition, __FILE__, __LINE__)) { \
      return 1; \
    } \
  } while (false)

int main(int argc, char** argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  }

  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("QCOStudioTests"));
  QCoreApplication::setApplicationName(QStringLiteral("QCO Studio Smoke Tests"));
  qco::app::installMessageHandler();

  QSettings().clear();

  {
    qco::ui::MainWindow window;
    window.resize(1024, 720);
    window.show();
    QApplication::processEvents();

    CHECK(window.isVisible());
    CHECK(window.windowTitle().contains(QStringLiteral("QCO Studio")));
    CHECK(window.centralWidget() != nullptr);
    CHECK(window.findChild<qco::ui::CanvasView*>() != nullptr);
    CHECK(window.findChild<QToolBar*>(QStringLiteral("toolRail")) != nullptr);
    CHECK(window.findChild<QDockWidget*>(QStringLiteral("layersDock")) != nullptr);
    CHECK(window.findChild<QDockWidget*>(QStringLiteral("toolOptionsDock")) != nullptr);
    CHECK(window.findChild<QDockWidget*>(QStringLiteral("propertiesDock")) != nullptr);
    CHECK(window.findChild<QDockWidget*>(QStringLiteral("historyDock")) != nullptr);

    QStringList menus;
    for (auto* action : window.menuBar()->actions()) {
      menus.push_back(actionText(action));
    }
    CHECK(menus.contains(QStringLiteral("File")));
    CHECK(menus.contains(QStringLiteral("Edit")));
    CHECK(menus.contains(QStringLiteral("Layer")));
    CHECK(menus.contains(QStringLiteral("View")));

    const QImage screenshot = window.grab().toImage();
    CHECK(!screenshot.isNull());
    CHECK(screenshot.width() > 0);
    CHECK(screenshot.height() > 0);

    window.close();
  }

  QSettings().clear();
  return 0;
}
