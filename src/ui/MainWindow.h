#pragma once

#include "core/Document.h"
#include "core/UndoStack.h"
#include "ui/CanvasView.h"
#include "ui/ProjectArchive.h"

#include <QMainWindow>
#include <QSettings>

#include <memory>

class QAction;
class QLabel;
class QListWidget;
class QDockWidget;

namespace qco::ui {

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

private slots:
  void newDocument();
  void openImage();
  void saveProject();
  void exportImage();
  void fitCanvasToView();
  void setCanvasToActualSize();
  void undo();
  void redo();
  void updateZoomLabel(qreal zoom);

private:
  void createActions();
  void createMenus();
  void createToolRail();
  void createPanels();
  void createInitialDocument();
  void setDocument(qco::core::Document document, QVector<CanvasView::LayerImage> layers);
  void updateLayerPanel();
  void updatePropertiesPanel();
  void updateHistoryPanel();
  void updateActions();
  void updateWindowTitle();
  void rememberDirectory(const QString& filePath);

  [[nodiscard]] QString lastDirectory() const;
  [[nodiscard]] QString documentTitle() const;
  [[nodiscard]] QString withDefaultSuffix(QString filePath, const QString& suffix) const;
  [[nodiscard]] QVector<ProjectRasterLayer> rasterLayerPayloads() const;

  CanvasView* canvas_ = nullptr;
  QListWidget* layersList_ = nullptr;
  QLabel* propertiesLabel_ = nullptr;
  QListWidget* historyList_ = nullptr;
  QLabel* zoomLabel_ = nullptr;

  QAction* newAction_ = nullptr;
  QAction* openAction_ = nullptr;
  QAction* saveAction_ = nullptr;
  QAction* exportAction_ = nullptr;
  QAction* fitAction_ = nullptr;
  QAction* actualSizeAction_ = nullptr;
  QAction* undoAction_ = nullptr;
  QAction* redoAction_ = nullptr;

  std::unique_ptr<qco::core::Document> document_;
  QVector<CanvasView::LayerImage> layers_;
  qco::core::UndoStack undoStack_;
  QString currentProjectPath_;
  QSettings settings_;
};

}  // namespace qco::ui
