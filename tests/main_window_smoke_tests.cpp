#include "app/Logging.h"
#include "ui/CanvasView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStringList>
#include <QTemporaryDir>
#include <QToolBar>

#include <algorithm>
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

[[nodiscard]] QAction* findAction(QObject& root, const QString& text)
{
  const auto actions = root.findChildren<QAction*>();
  const auto it = std::find_if(actions.begin(), actions.end(), [&text](QAction* action) {
    return actionText(action) == text;
  });
  return it == actions.end() ? nullptr : *it;
}

void clickCanvas(qco::ui::CanvasView& canvas)
{
  const QPointF position(canvas.rect().center());
  QMouseEvent press(
    QEvent::MouseButtonPress,
    position,
    position,
    Qt::LeftButton,
    Qt::LeftButton,
    Qt::NoModifier);
  QApplication::sendEvent(&canvas, &press);

  QMouseEvent release(
    QEvent::MouseButtonRelease,
    position,
    position,
    Qt::LeftButton,
    Qt::NoButton,
    Qt::NoModifier);
  QApplication::sendEvent(&canvas, &release);
}

[[nodiscard]] const qco::ui::CanvasView::LayerImage* findCanvasLayer(
  const qco::ui::CanvasView& canvas,
  std::uint64_t id)
{
  const auto& layers = canvas.layers();
  const auto it = std::find_if(layers.begin(), layers.end(), [id](const qco::ui::CanvasView::LayerImage& layer) {
    return layer.id == id;
  });
  return it == layers.end() ? nullptr : &(*it);
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
    auto* canvas = window.findChild<qco::ui::CanvasView*>();
    CHECK(canvas != nullptr);
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

    CHECK(!window.windowTitle().contains(QLatin1Char('*')));
    auto* addLayerAction = findAction(window, QStringLiteral("Add Raster Layer"));
    auto* deleteLayerAction = window.findChild<QAction*>(QStringLiteral("deleteLayerAction"));
    auto* undoAction = window.findChild<QAction*>(QStringLiteral("undoAction"));
    auto* textAction = findAction(window, QStringLiteral("Text"));
    auto* shapeAction = findAction(window, QStringLiteral("Shape"));
    auto* historyList = window.findChild<QListWidget*>(QStringLiteral("historyList"));
    CHECK(addLayerAction != nullptr);
    CHECK(deleteLayerAction != nullptr);
    CHECK(undoAction != nullptr);
    CHECK(textAction != nullptr);
    CHECK(shapeAction != nullptr);
    CHECK(historyList != nullptr);
    auto* lockLayerButton = window.findChild<QPushButton*>(QStringLiteral("toggleLayerLockButton"));
    auto* propertiesLabel = window.findChild<QLabel*>(QStringLiteral("propertiesLabel"));
    CHECK(lockLayerButton != nullptr);
    CHECK(propertiesLabel != nullptr);

    addLayerAction->trigger();
    QApplication::processEvents();
    CHECK(window.windowTitle().contains(QLatin1Char('*')));
    const auto rasterLayerId = canvas->selectedLayerId();
    CHECK(findCanvasLayer(*canvas, rasterLayerId) != nullptr);
    CHECK(lockLayerButton->isEnabled());
    CHECK(lockLayerButton->text() == QStringLiteral("Lock"));

    lockLayerButton->click();
    QApplication::processEvents();
    CHECK(lockLayerButton->text() == QStringLiteral("Unlock"));
    CHECK(!deleteLayerAction->isEnabled());
    CHECK(propertiesLabel->text().contains(QStringLiteral("Locked")));

    undoAction->trigger();
    QApplication::processEvents();
    CHECK(lockLayerButton->text() == QStringLiteral("Lock"));
    CHECK(deleteLayerAction->isEnabled());
    CHECK(propertiesLabel->text().contains(QStringLiteral("Unlocked")));

    deleteLayerAction->trigger();
    QApplication::processEvents();
    CHECK(findCanvasLayer(*canvas, rasterLayerId) == nullptr);
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Delete Layer")));

    undoAction->trigger();
    QApplication::processEvents();
    CHECK(findCanvasLayer(*canvas, rasterLayerId) != nullptr);

    undoAction->trigger();
    QApplication::processEvents();
    CHECK(!window.windowTitle().contains(QLatin1Char('*')));

    auto* textContentInput = window.findChild<QLineEdit*>(QStringLiteral("textContentInput"));
    auto* textSizeInput = window.findChild<QSpinBox*>(QStringLiteral("textSizeInput"));
    auto* updateTextLayerButton = window.findChild<QPushButton*>(QStringLiteral("updateTextLayerButton"));
    auto* shapeTypeInput = window.findChild<QComboBox*>(QStringLiteral("shapeTypeInput"));
    auto* shapeWidthInput = window.findChild<QSpinBox*>(QStringLiteral("shapeWidthInput"));
    auto* shapeHeightInput = window.findChild<QSpinBox*>(QStringLiteral("shapeHeightInput"));
    auto* updateShapeLayerButton = window.findChild<QPushButton*>(QStringLiteral("updateShapeLayerButton"));
    CHECK(textContentInput != nullptr);
    CHECK(textSizeInput != nullptr);
    CHECK(updateTextLayerButton != nullptr);
    CHECK(shapeTypeInput != nullptr);
    CHECK(shapeWidthInput != nullptr);
    CHECK(shapeHeightInput != nullptr);
    CHECK(updateShapeLayerButton != nullptr);

    textContentInput->setText(QStringLiteral("Editable"));
    textAction->trigger();
    clickCanvas(*canvas);
    QApplication::processEvents();

    const auto textLayerId = canvas->selectedLayerId();
    const auto* textLayer = findCanvasLayer(*canvas, textLayerId);
    CHECK(textLayer != nullptr);
    CHECK(textLayer->name == QStringLiteral("Editable"));
    const auto initialTextSize = textLayer->image.size();
    CHECK(updateTextLayerButton->isEnabled());

    lockLayerButton->click();
    QApplication::processEvents();
    CHECK(propertiesLabel->text().contains(QStringLiteral("Locked")));
    CHECK(!updateTextLayerButton->isEnabled());
    textContentInput->setText(QStringLiteral("Blocked Edit"));
    updateTextLayerButton->click();
    QApplication::processEvents();
    textLayer = findCanvasLayer(*canvas, textLayerId);
    CHECK(textLayer != nullptr);
    CHECK(textLayer->name == QStringLiteral("Editable"));

    undoAction->trigger();
    QApplication::processEvents();
    CHECK(propertiesLabel->text().contains(QStringLiteral("Unlocked")));
    CHECK(updateTextLayerButton->isEnabled());

    textContentInput->setText(QStringLiteral("Edited"));
    textSizeInput->setValue(textSizeInput->value() + 8);
    updateTextLayerButton->click();
    QApplication::processEvents();

    textLayer = findCanvasLayer(*canvas, textLayerId);
    CHECK(textLayer != nullptr);
    CHECK(textLayer->name == QStringLiteral("Edited"));
    CHECK(textLayer->image.size() != initialTextSize);
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Update Text Layer")));
    CHECK(undoAction->isEnabled());

    undoAction->trigger();
    QApplication::processEvents();
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Add Text Layer")));
    textLayer = findCanvasLayer(*canvas, textLayerId);
    CHECK(textLayer != nullptr);
    CHECK(textLayer->name == QStringLiteral("Editable"));
    CHECK(textLayer->image.size() == initialTextSize);

    shapeAction->trigger();
    clickCanvas(*canvas);
    QApplication::processEvents();

    const auto shapeLayerId = canvas->selectedLayerId();
    const auto* shapeLayer = findCanvasLayer(*canvas, shapeLayerId);
    CHECK(shapeLayer != nullptr);
    const auto initialShapeSize = shapeLayer->image.size();
    CHECK(updateShapeLayerButton->isEnabled());

    const auto ellipseIndex = shapeTypeInput->findData(QStringLiteral("ellipse"));
    CHECK(ellipseIndex >= 0);
    shapeTypeInput->setCurrentIndex(ellipseIndex);
    shapeWidthInput->setValue(96);
    shapeHeightInput->setValue(72);
    updateShapeLayerButton->click();
    QApplication::processEvents();

    shapeLayer = findCanvasLayer(*canvas, shapeLayerId);
    CHECK(shapeLayer != nullptr);
    CHECK(shapeLayer->name == QStringLiteral("Ellipse"));
    CHECK(shapeLayer->image.size() == QSize(96, 72));

    undoAction->trigger();
    QApplication::processEvents();
    shapeLayer = findCanvasLayer(*canvas, shapeLayerId);
    CHECK(shapeLayer != nullptr);
    CHECK(shapeLayer->image.size() == initialShapeSize);

    undoAction->trigger();
    QApplication::processEvents();
    CHECK(findCanvasLayer(*canvas, shapeLayerId) == nullptr);

    undoAction->trigger();
    QApplication::processEvents();
    CHECK(findCanvasLayer(*canvas, textLayerId) == nullptr);
    CHECK(!window.windowTitle().contains(QLatin1Char('*')));

    const QImage screenshot = window.grab().toImage();
    CHECK(!screenshot.isNull());
    CHECK(screenshot.width() > 0);
    CHECK(screenshot.height() > 0);

    window.close();
  }

  {
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());
    const auto projectPath = tempDir.filePath(QStringLiteral("app-roundtrip.qco"));

    std::uint64_t textLayerId = 0;
    std::uint64_t shapeLayerId = 0;
    {
      qco::ui::MainWindow window;
      window.resize(1024, 720);
      window.show();
      QApplication::processEvents();

      auto* canvas = window.findChild<qco::ui::CanvasView*>();
      auto* textAction = findAction(window, QStringLiteral("Text"));
      auto* shapeAction = findAction(window, QStringLiteral("Shape"));
      auto* textContentInput = window.findChild<QLineEdit*>(QStringLiteral("textContentInput"));
      CHECK(canvas != nullptr);
      CHECK(textAction != nullptr);
      CHECK(shapeAction != nullptr);
      CHECK(textContentInput != nullptr);

      textContentInput->setText(QStringLiteral("Saved Text"));
      textAction->trigger();
      clickCanvas(*canvas);
      QApplication::processEvents();
      textLayerId = canvas->selectedLayerId();
      CHECK(findCanvasLayer(*canvas, textLayerId) != nullptr);

      shapeAction->trigger();
      clickCanvas(*canvas);
      QApplication::processEvents();
      shapeLayerId = canvas->selectedLayerId();
      CHECK(findCanvasLayer(*canvas, shapeLayerId) != nullptr);

      CHECK(window.saveProjectToPath(projectPath));
      QApplication::processEvents();
      CHECK(!window.windowTitle().contains(QLatin1Char('*')));
    }

    {
      qco::ui::MainWindow reopenedWindow;
      reopenedWindow.resize(1024, 720);
      reopenedWindow.show();
      QApplication::processEvents();

      CHECK(reopenedWindow.openProjectFromPath(projectPath));
      QApplication::processEvents();
      auto* reopenedCanvas = reopenedWindow.findChild<qco::ui::CanvasView*>();
      auto* historyList = reopenedWindow.findChild<QListWidget*>(QStringLiteral("historyList"));
      CHECK(reopenedCanvas != nullptr);
      CHECK(historyList != nullptr);

      const auto* textLayer = findCanvasLayer(*reopenedCanvas, textLayerId);
      const auto* shapeLayer = findCanvasLayer(*reopenedCanvas, shapeLayerId);
      CHECK(textLayer != nullptr);
      CHECK(shapeLayer != nullptr);
      CHECK(textLayer->name == QStringLiteral("Saved Text"));
      CHECK(shapeLayer->name == QStringLiteral("Rectangle"));
      CHECK(!reopenedWindow.windowTitle().contains(QLatin1Char('*')));
      CHECK(historyList->item(0) != nullptr);
      CHECK(historyList->item(0)->text().contains(QStringLiteral("No edits yet")));
    }
  }

  QSettings().clear();
  return 0;
}
