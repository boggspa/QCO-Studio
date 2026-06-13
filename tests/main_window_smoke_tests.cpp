#include "app/Logging.h"
#include "ui/CanvasView.h"
#include "ui/MainWindow.h"
#include "ui/ProjectArchive.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QTemporaryDir>
#include <QToolBar>
#include <QTimer>

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

[[nodiscard]] QListWidgetItem* findLayerListItem(QListWidget& list, std::uint64_t id)
{
  for (int row = 0; row < list.count(); ++row) {
    auto* item = list.item(row);
    if (item != nullptr && item->data(Qt::UserRole).toULongLong() == id) {
      return item;
    }
  }
  return nullptr;
}

[[nodiscard]] QImage solidImage(QSize size, const QColor& color)
{
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(color);
  return image;
}

[[nodiscard]] QPointF widgetPointForDocumentPoint(
  const qco::ui::CanvasView& canvas,
  const QPointF documentPoint)
{
  return QPointF(canvas.width() / 2.0, canvas.height() / 2.0)
         + (documentPoint
            - QPointF(canvas.documentSize().width() / 2.0, canvas.documentSize().height() / 2.0))
             * canvas.zoom();
}

void dragCanvasDocumentPoint(qco::ui::CanvasView& canvas, QPointF fromDocumentPoint, QPointF toDocumentPoint)
{
  const QPointF from = widgetPointForDocumentPoint(canvas, fromDocumentPoint);
  const QPointF to = widgetPointForDocumentPoint(canvas, toDocumentPoint);

  QMouseEvent press(
    QEvent::MouseButtonPress,
    from,
    from,
    Qt::LeftButton,
    Qt::LeftButton,
    Qt::NoModifier);
  QApplication::sendEvent(&canvas, &press);

  QMouseEvent move(
    QEvent::MouseMove,
    to,
    to,
    Qt::NoButton,
    Qt::LeftButton,
    Qt::NoModifier);
  QApplication::sendEvent(&canvas, &move);

  QMouseEvent release(
    QEvent::MouseButtonRelease,
    to,
    to,
    Qt::LeftButton,
    Qt::NoButton,
    Qt::NoModifier);
  QApplication::sendEvent(&canvas, &release);
}

void clickCanvasDocumentPoint(qco::ui::CanvasView& canvas, QPointF documentPoint)
{
  const QPointF position = widgetPointForDocumentPoint(canvas, documentPoint);

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

[[nodiscard]] bool clickVisibleMessageBoxButton(QMessageBox::StandardButton button)
{
  auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
  if (box == nullptr) {
    for (auto* widget : QApplication::topLevelWidgets()) {
      box = qobject_cast<QMessageBox*>(widget);
      if (box != nullptr && box->isVisible()) {
        break;
      }
    }
  }

  if (box == nullptr) {
    return false;
  }

  auto* buttonWidget = box->button(button);
  if (buttonWidget == nullptr) {
    return false;
  }

  buttonWidget->click();
  return true;
}

void clickNextMessageBoxButton(QMessageBox::StandardButton button)
{
  QTimer::singleShot(0, [button]() {
    if (!clickVisibleMessageBoxButton(button)) {
      QTimer::singleShot(10, [button]() {
        (void)clickVisibleMessageBoxButton(button);
      });
    }
  });
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
      CHECK(window.renameSelectedLayerTo(QStringLiteral("Saved Shape")));

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
      CHECK(shapeLayer->name == QStringLiteral("Saved Shape"));
      CHECK(!reopenedWindow.windowTitle().contains(QLatin1Char('*')));
      CHECK(historyList->item(0) != nullptr);
      CHECK(historyList->item(0)->text().contains(QStringLiteral("No edits yet")));
    }
  }

  {
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());
    const auto projectPath = tempDir.filePath(QStringLiteral("export-smoke.qco"));
    const auto exportPath = tempDir.filePath(QStringLiteral("export-smoke.png"));

    auto document = qco::core::Document::create("Export Smoke", {32, 24});
    const auto redLayerId = document.addLayer("Red", qco::core::LayerType::Raster, {16, 16}, {0, 0});
    const auto hiddenLayerId = document.addLayer("Hidden Green", qco::core::LayerType::Raster, {32, 24}, {0, 0});
    const auto blueLayerId = document.addLayer("Blue", qco::core::LayerType::Raster, {8, 8}, {10, 6});
    CHECK(document.setLayerVisibility(hiddenLayerId, false));

    QVector<qco::ui::ProjectRasterLayer> projectLayers;
    projectLayers.push_back({
      redLayerId,
      QStringLiteral("Red"),
      solidImage(QSize(16, 16), QColor(255, 0, 0, 255)),
      QPoint(0, 0),
      true,
      1.0,
    });
    projectLayers.push_back({
      hiddenLayerId,
      QStringLiteral("Hidden Green"),
      solidImage(QSize(32, 24), QColor(0, 255, 0, 255)),
      QPoint(0, 0),
      false,
      1.0,
    });
    projectLayers.push_back({
      blueLayerId,
      QStringLiteral("Blue"),
      solidImage(QSize(8, 8), QColor(0, 0, 255, 255)),
      QPoint(10, 6),
      true,
      1.0,
    });

    QString archiveError;
    CHECK(qco::ui::ProjectArchive::save(projectPath, document, projectLayers, &archiveError));

    qco::ui::MainWindow window;
    window.resize(1024, 720);
    window.show();
    QApplication::processEvents();

    CHECK(window.openProjectFromPath(projectPath));
    QApplication::processEvents();
    CHECK(window.exportImageToPath(exportPath));
    QApplication::processEvents();
    CHECK(!window.windowTitle().contains(QLatin1Char('*')));

    const QImage exported(exportPath);
    CHECK(!exported.isNull());
    CHECK(exported.size() == QSize(32, 24));
    CHECK(exported.pixelColor(2, 2) == QColor(255, 0, 0, 255));
    CHECK(exported.pixelColor(12, 8) == QColor(0, 0, 255, 255));
    CHECK(exported.pixelColor(24, 20).alpha() == 0);
  }

  {
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());
    const auto sourcePath = tempDir.filePath(QStringLiteral("milestone-source.png"));
    const auto projectPath = tempDir.filePath(QStringLiteral("milestone-loop.qco"));
    const auto pngExportPath = tempDir.filePath(QStringLiteral("milestone-loop.png"));
    const auto jpegExportPath = tempDir.filePath(QStringLiteral("milestone-loop.jpg"));

    CHECK(solidImage(QSize(32, 24), QColor(220, 16, 16, 255)).save(sourcePath));

    std::uint64_t sourceLayerId = 0;
    std::uint64_t overlayLayerId = 0;
    {
      qco::ui::MainWindow window;
      window.resize(1024, 720);
      window.show();
      QApplication::processEvents();

      CHECK(window.openImageFromPath(sourcePath));
      QApplication::processEvents();
      CHECK(window.windowTitle().contains(QLatin1Char('*')));

      auto* canvas = window.findChild<qco::ui::CanvasView*>();
      auto* addLayerAction = findAction(window, QStringLiteral("Add Raster Layer"));
      auto* fillAction = findAction(window, QStringLiteral("Fill"));
      auto* moveAction = findAction(window, QStringLiteral("Move"));
      auto* layerUpButton = window.findChild<QPushButton*>(QStringLiteral("layerUpButton"));
      auto* layerDownButton = window.findChild<QPushButton*>(QStringLiteral("layerDownButton"));
      CHECK(canvas != nullptr);
      CHECK(addLayerAction != nullptr);
      CHECK(fillAction != nullptr);
      CHECK(moveAction != nullptr);
      CHECK(layerUpButton != nullptr);
      CHECK(layerDownButton != nullptr);
      CHECK(canvas->layers().size() == 1);

      sourceLayerId = canvas->selectedLayerId();
      CHECK(sourceLayerId != 0);

      addLayerAction->trigger();
      QApplication::processEvents();
      overlayLayerId = canvas->selectedLayerId();
      CHECK(overlayLayerId != 0);
      CHECK(overlayLayerId != sourceLayerId);
      CHECK(canvas->layers().size() == 2);
      CHECK(canvas->layers().front().id == sourceLayerId);
      CHECK(canvas->layers().back().id == overlayLayerId);

      fillAction->trigger();
      QApplication::processEvents();
      clickCanvasDocumentPoint(*canvas, QPointF(8, 8));
      QApplication::processEvents();

      const auto* overlayLayer = findCanvasLayer(*canvas, overlayLayerId);
      CHECK(overlayLayer != nullptr);
      CHECK(overlayLayer->image.pixelColor(8, 8) == QColor(24, 24, 24, 255));

      CHECK(layerDownButton->isEnabled());
      layerDownButton->click();
      QApplication::processEvents();
      CHECK(canvas->layers().front().id == overlayLayerId);
      CHECK(layerUpButton->isEnabled());
      layerUpButton->click();
      QApplication::processEvents();
      CHECK(canvas->layers().back().id == overlayLayerId);

      moveAction->trigger();
      QApplication::processEvents();
      dragCanvasDocumentPoint(*canvas, QPointF(8, 8), QPointF(14, 13));
      QApplication::processEvents();
      overlayLayer = findCanvasLayer(*canvas, overlayLayerId);
      CHECK(overlayLayer != nullptr);
      CHECK(overlayLayer->position == QPoint(6, 5));

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
      CHECK(reopenedCanvas != nullptr);
      CHECK(reopenedCanvas->layers().size() == 2);
      CHECK(reopenedCanvas->layers().front().id == sourceLayerId);
      CHECK(reopenedCanvas->layers().back().id == overlayLayerId);

      const auto* reopenedSource = findCanvasLayer(*reopenedCanvas, sourceLayerId);
      const auto* reopenedOverlay = findCanvasLayer(*reopenedCanvas, overlayLayerId);
      CHECK(reopenedSource != nullptr);
      CHECK(reopenedOverlay != nullptr);
      CHECK(reopenedSource->position == QPoint(0, 0));
      CHECK(reopenedOverlay->position == QPoint(6, 5));
      CHECK(reopenedOverlay->image.pixelColor(8, 8) == QColor(24, 24, 24, 255));

      CHECK(reopenedWindow.exportImageToPath(pngExportPath));
      CHECK(reopenedWindow.exportImageToPath(jpegExportPath));
      QApplication::processEvents();

      const QImage exportedPng(pngExportPath);
      CHECK(!exportedPng.isNull());
      CHECK(exportedPng.size() == QSize(32, 24));
      CHECK(exportedPng.pixelColor(2, 2) == QColor(220, 16, 16, 255));
      CHECK(exportedPng.pixelColor(10, 8) == QColor(24, 24, 24, 255));

      const QImage exportedJpeg(jpegExportPath);
      CHECK(!exportedJpeg.isNull());
      CHECK(exportedJpeg.size() == QSize(32, 24));
      CHECK(exportedJpeg.pixelColor(2, 2).red() > 120);
      CHECK(exportedJpeg.pixelColor(2, 2).green() < 120);
      CHECK(exportedJpeg.pixelColor(2, 2).blue() < 120);
      CHECK(exportedJpeg.pixelColor(10, 8).red() < 100);
      CHECK(exportedJpeg.pixelColor(10, 8).green() < 100);
      CHECK(exportedJpeg.pixelColor(10, 8).blue() < 100);
    }
  }

  {
    qco::ui::MainWindow window;
    window.resize(1024, 720);
    window.show();
    QApplication::processEvents();

    auto* canvas = window.findChild<qco::ui::CanvasView*>();
    auto* layersDock = window.findChild<QDockWidget*>(QStringLiteral("layersDock"));
    auto* layersList = layersDock == nullptr ? nullptr : layersDock->findChild<QListWidget*>();
    auto* historyList = window.findChild<QListWidget*>(QStringLiteral("historyList"));
    auto* propertiesLabel = window.findChild<QLabel*>(QStringLiteral("propertiesLabel"));
    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* duplicateLayerButton = window.findChild<QPushButton*>(QStringLiteral("duplicateLayerButton"));
    auto* renameLayerButton = window.findChild<QPushButton*>(QStringLiteral("renameLayerButton"));
    auto* layerUpButton = window.findChild<QPushButton*>(QStringLiteral("layerUpButton"));
    auto* layerDownButton = window.findChild<QPushButton*>(QStringLiteral("layerDownButton"));
    auto* opacitySlider = window.findChild<QSlider*>();
    auto* undoAction = window.findChild<QAction*>(QStringLiteral("undoAction"));
    auto* redoAction = window.findChild<QAction*>(QStringLiteral("redoAction"));
    auto* moveAction = findAction(window, QStringLiteral("Move"));
    CHECK(canvas != nullptr);
    CHECK(layersDock != nullptr);
    CHECK(layersList != nullptr);
    CHECK(historyList != nullptr);
    CHECK(propertiesLabel != nullptr);
    CHECK(addLayerButton != nullptr);
    CHECK(duplicateLayerButton != nullptr);
    CHECK(renameLayerButton != nullptr);
    CHECK(layerUpButton != nullptr);
    CHECK(layerDownButton != nullptr);
    CHECK(opacitySlider != nullptr);
    CHECK(undoAction != nullptr);
    CHECK(redoAction != nullptr);
    CHECK(moveAction != nullptr);

    addLayerButton->click();
    QApplication::processEvents();
    const auto bottomLayerId = canvas->selectedLayerId();
    addLayerButton->click();
    QApplication::processEvents();
    const auto topLayerId = canvas->selectedLayerId();
    CHECK(bottomLayerId != 0);
    CHECK(topLayerId != 0);
    CHECK(bottomLayerId != topLayerId);
    CHECK(canvas->layers().size() == 2);
    CHECK(canvas->layers().front().id == bottomLayerId);
    CHECK(canvas->layers().back().id == topLayerId);

    CHECK(layerDownButton->isEnabled());
    layerDownButton->click();
    QApplication::processEvents();
    CHECK(canvas->layers().front().id == topLayerId);
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Move Layer Down")));

    CHECK(layerUpButton->isEnabled());
    layerUpButton->click();
    QApplication::processEvents();
    CHECK(canvas->layers().back().id == topLayerId);
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Move Layer Up")));

    duplicateLayerButton->click();
    QApplication::processEvents();
    const auto duplicateLayerId = canvas->selectedLayerId();
    CHECK(duplicateLayerId != 0);
    CHECK(duplicateLayerId != topLayerId);
    CHECK(canvas->layers().size() == 3);
    auto* duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->name.contains(QStringLiteral("Copy")));
    CHECK(duplicateLayer->position == QPoint(16, 16));
    const auto duplicateOriginalName = duplicateLayer->name;
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Duplicate Layer")));

    CHECK(renameLayerButton->isEnabled());
    CHECK(window.renameSelectedLayerTo(QStringLiteral("  Composite Paint  ")));
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->name == QStringLiteral("Composite Paint"));
    auto* renamedItem = findLayerListItem(*layersList, duplicateLayerId);
    CHECK(renamedItem != nullptr);
    CHECK(renamedItem->text().contains(QStringLiteral("Composite Paint")));
    CHECK(propertiesLabel->text().contains(QStringLiteral("Composite Paint")));
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Rename Layer")));

    undoAction->trigger();
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->name == duplicateOriginalName);
    redoAction->trigger();
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->name == QStringLiteral("Composite Paint"));

    auto* duplicateItem = findLayerListItem(*layersList, duplicateLayerId);
    CHECK(duplicateItem != nullptr);
    CHECK(duplicateItem->checkState() == Qt::Checked);
    duplicateItem->setCheckState(Qt::Unchecked);
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(!duplicateLayer->visible);
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Hide Layer")));

    undoAction->trigger();
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->visible);
    redoAction->trigger();
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(!duplicateLayer->visible);
    undoAction->trigger();
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->visible);

    CHECK(opacitySlider->isEnabled());
    opacitySlider->setValue(42);
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->opacity > 0.41);
    CHECK(duplicateLayer->opacity < 0.43);
    CHECK(propertiesLabel->text().contains(QStringLiteral("42% opacity")));
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Change Layer Opacity")));

    undoAction->trigger();
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->opacity > 0.99);
    redoAction->trigger();
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->opacity > 0.41);
    CHECK(duplicateLayer->opacity < 0.43);

    moveAction->trigger();
    QApplication::processEvents();
    CHECK(canvas->activeTool() == qco::ui::CanvasView::Tool::Move);
    const auto beforeMovePosition = duplicateLayer->position;
    dragCanvasDocumentPoint(*canvas, QPointF(960, 540), QPointF(992, 568));
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->position == beforeMovePosition + QPoint(32, 28));
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Move Layer")));

    undoAction->trigger();
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->position == beforeMovePosition);
    redoAction->trigger();
    QApplication::processEvents();
    duplicateLayer = findCanvasLayer(*canvas, duplicateLayerId);
    CHECK(duplicateLayer != nullptr);
    CHECK(duplicateLayer->position == beforeMovePosition + QPoint(32, 28));
  }

  {
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());
    const auto projectPath = tempDir.filePath(QStringLiteral("tool-smoke.qco"));

    auto document = qco::core::Document::create("Tool Smoke", {96, 64});
    const auto rasterLayerId = document.addLayer("Tool Pixels", qco::core::LayerType::Raster, {96, 64});
    QImage image(QSize(96, 64), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QVector<qco::ui::ProjectRasterLayer> projectLayers;
    projectLayers.push_back({
      rasterLayerId,
      QStringLiteral("Tool Pixels"),
      image,
      QPoint(0, 0),
      true,
      1.0,
    });

    QString archiveError;
    CHECK(qco::ui::ProjectArchive::save(projectPath, document, projectLayers, &archiveError));

    qco::ui::MainWindow window;
    window.resize(1024, 720);
    window.show();
    QApplication::processEvents();

    CHECK(window.openProjectFromPath(projectPath));
    QApplication::processEvents();

    auto* canvas = window.findChild<qco::ui::CanvasView*>();
    auto* historyList = window.findChild<QListWidget*>(QStringLiteral("historyList"));
    auto* brushAction = findAction(window, QStringLiteral("Brush"));
    auto* fillAction = findAction(window, QStringLiteral("Fill"));
    auto* eraseAction = findAction(window, QStringLiteral("Erase"));
    auto* cropAction = findAction(window, QStringLiteral("Crop"));
    auto* brushSizeInput = window.findChild<QSpinBox*>(QStringLiteral("brushSizeInput"));
    auto* eraserSizeInput = window.findChild<QSpinBox*>(QStringLiteral("eraserSizeInput"));
    auto* undoAction = window.findChild<QAction*>(QStringLiteral("undoAction"));
    auto* redoAction = window.findChild<QAction*>(QStringLiteral("redoAction"));
    CHECK(canvas != nullptr);
    CHECK(historyList != nullptr);
    CHECK(brushAction != nullptr);
    CHECK(fillAction != nullptr);
    CHECK(eraseAction != nullptr);
    CHECK(cropAction != nullptr);
    CHECK(brushSizeInput != nullptr);
    CHECK(eraserSizeInput != nullptr);
    CHECK(undoAction != nullptr);
    CHECK(redoAction != nullptr);
    CHECK(canvas->documentSize() == QSize(96, 64));
    CHECK(findCanvasLayer(*canvas, rasterLayerId) != nullptr);
    CHECK(!window.windowTitle().contains(QLatin1Char('*')));

    brushSizeInput->setValue(6);
    brushAction->trigger();
    QApplication::processEvents();
    CHECK(canvas->activeTool() == qco::ui::CanvasView::Tool::Brush);
    dragCanvasDocumentPoint(*canvas, QPointF(20, 20), QPointF(40, 20));
    QApplication::processEvents();

    auto* rasterLayer = findCanvasLayer(*canvas, rasterLayerId);
    CHECK(rasterLayer != nullptr);
    CHECK(rasterLayer->image.pixelColor(30, 20).alpha() > 200);
    CHECK(rasterLayer->image.pixelColor(30, 20).red() < 64);
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Brush Stroke")));

    fillAction->trigger();
    QApplication::processEvents();
    CHECK(canvas->activeTool() == qco::ui::CanvasView::Tool::Fill);
    clickCanvasDocumentPoint(*canvas, QPointF(80, 50));
    QApplication::processEvents();

    rasterLayer = findCanvasLayer(*canvas, rasterLayerId);
    CHECK(rasterLayer != nullptr);
    CHECK(rasterLayer->image.pixelColor(80, 50).alpha() > 200);
    CHECK(rasterLayer->image.pixelColor(80, 50).red() < 64);
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Fill Layer Region")));

    eraserSizeInput->setValue(8);
    eraseAction->trigger();
    QApplication::processEvents();
    CHECK(canvas->activeTool() == qco::ui::CanvasView::Tool::Eraser);
    dragCanvasDocumentPoint(*canvas, QPointF(80, 50), QPointF(84, 50));
    QApplication::processEvents();

    rasterLayer = findCanvasLayer(*canvas, rasterLayerId);
    CHECK(rasterLayer != nullptr);
    CHECK(rasterLayer->image.pixelColor(80, 50).alpha() == 0);
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Erase Stroke")));

    undoAction->trigger();
    QApplication::processEvents();
    rasterLayer = findCanvasLayer(*canvas, rasterLayerId);
    CHECK(rasterLayer != nullptr);
    CHECK(rasterLayer->image.pixelColor(80, 50).alpha() > 200);
    redoAction->trigger();
    QApplication::processEvents();
    rasterLayer = findCanvasLayer(*canvas, rasterLayerId);
    CHECK(rasterLayer != nullptr);
    CHECK(rasterLayer->image.pixelColor(80, 50).alpha() == 0);

    cropAction->trigger();
    QApplication::processEvents();
    CHECK(canvas->activeTool() == qco::ui::CanvasView::Tool::Crop);
    dragCanvasDocumentPoint(*canvas, QPointF(10, 8), QPointF(70, 48));
    QApplication::processEvents();

    rasterLayer = findCanvasLayer(*canvas, rasterLayerId);
    CHECK(rasterLayer != nullptr);
    CHECK(canvas->documentSize() == QSize(60, 40));
    CHECK(rasterLayer->position == QPoint(-10, -8));
    CHECK(historyList->item(0) != nullptr);
    CHECK(historyList->item(0)->text().contains(QStringLiteral("Crop Canvas")));

    undoAction->trigger();
    QApplication::processEvents();
    rasterLayer = findCanvasLayer(*canvas, rasterLayerId);
    CHECK(rasterLayer != nullptr);
    CHECK(canvas->documentSize() == QSize(96, 64));
    CHECK(rasterLayer->position == QPoint(0, 0));
    redoAction->trigger();
    QApplication::processEvents();
    rasterLayer = findCanvasLayer(*canvas, rasterLayerId);
    CHECK(rasterLayer != nullptr);
    CHECK(canvas->documentSize() == QSize(60, 40));
    CHECK(rasterLayer->position == QPoint(-10, -8));
  }

  {
    qco::ui::MainWindow window;
    window.resize(1024, 720);
    window.show();
    QApplication::processEvents();

    auto* addLayerAction = window.findChild<QAction*>(QStringLiteral("addRasterLayerAction"));
    auto* newAction = window.findChild<QAction*>(QStringLiteral("newAction"));
    auto* openImageAction = window.findChild<QAction*>(QStringLiteral("openImageAction"));
    auto* openProjectAction = window.findChild<QAction*>(QStringLiteral("openProjectAction"));
    CHECK(addLayerAction != nullptr);
    CHECK(newAction != nullptr);
    CHECK(openImageAction != nullptr);
    CHECK(openProjectAction != nullptr);

    addLayerAction->trigger();
    QApplication::processEvents();
    CHECK(window.windowTitle().contains(QLatin1Char('*')));

    clickNextMessageBoxButton(QMessageBox::Cancel);
    newAction->trigger();
    QApplication::processEvents();
    CHECK(window.isVisible());
    CHECK(window.windowTitle().contains(QLatin1Char('*')));

    clickNextMessageBoxButton(QMessageBox::Cancel);
    openImageAction->trigger();
    QApplication::processEvents();
    CHECK(window.isVisible());
    CHECK(window.windowTitle().contains(QLatin1Char('*')));

    clickNextMessageBoxButton(QMessageBox::Cancel);
    openProjectAction->trigger();
    QApplication::processEvents();
    CHECK(window.isVisible());
    CHECK(window.windowTitle().contains(QLatin1Char('*')));

    clickNextMessageBoxButton(QMessageBox::Cancel);
    CHECK(!window.close());
    QApplication::processEvents();
    CHECK(window.isVisible());
    CHECK(window.windowTitle().contains(QLatin1Char('*')));

    clickNextMessageBoxButton(QMessageBox::Discard);
    CHECK(window.close());
    QApplication::processEvents();
    CHECK(!window.isVisible());
  }

  QSettings().clear();
  return 0;
}
