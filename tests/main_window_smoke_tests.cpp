#include "app/Logging.h"
#include "ui/CanvasView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStringList>
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
    auto* undoAction = window.findChild<QAction*>(QStringLiteral("undoAction"));
    auto* textAction = findAction(window, QStringLiteral("Text"));
    auto* shapeAction = findAction(window, QStringLiteral("Shape"));
    CHECK(addLayerAction != nullptr);
    CHECK(undoAction != nullptr);
    CHECK(textAction != nullptr);
    CHECK(shapeAction != nullptr);

    addLayerAction->trigger();
    QApplication::processEvents();
    CHECK(window.windowTitle().contains(QLatin1Char('*')));

    undoAction->trigger();
    QApplication::processEvents();
    CHECK(!window.windowTitle().contains(QLatin1Char('*')));

    auto* textContentInput = window.findChild<QLineEdit*>(QStringLiteral("textContentInput"));
    auto* textSizeInput = window.findChild<QSpinBox*>(QStringLiteral("textSizeInput"));
    auto* updateTextLayerButton = window.findChild<QPushButton*>(QStringLiteral("updateTextLayerButton"));
    auto* historyList = window.findChild<QListWidget*>(QStringLiteral("historyList"));
    auto* shapeTypeInput = window.findChild<QComboBox*>(QStringLiteral("shapeTypeInput"));
    auto* shapeWidthInput = window.findChild<QSpinBox*>(QStringLiteral("shapeWidthInput"));
    auto* shapeHeightInput = window.findChild<QSpinBox*>(QStringLiteral("shapeHeightInput"));
    auto* updateShapeLayerButton = window.findChild<QPushButton*>(QStringLiteral("updateShapeLayerButton"));
    CHECK(textContentInput != nullptr);
    CHECK(textSizeInput != nullptr);
    CHECK(updateTextLayerButton != nullptr);
    CHECK(historyList != nullptr);
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

  QSettings().clear();
  return 0;
}
