#pragma once

#include "core/Document.h"
#include "core/UndoStack.h"
#include "ui/CanvasView.h"
#include "ui/ProjectArchive.h"

#include <QMainWindow>
#include <QRect>
#include <QSettings>

#include <memory>
#include <optional>

class QAction;
class QActionGroup;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QDockWidget;
class QPushButton;
class QSlider;
class QCloseEvent;

namespace qco::ui {

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

private slots:
  void newDocument();
  void openImage();
  void openProject();
  void saveProject();
  void exportImage();
  void addRasterLayer();
  void duplicateSelectedLayer();
  void renameSelectedLayer();
  void moveSelectedLayerUp();
  void moveSelectedLayerDown();
  void layerSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);
  void layerItemChanged(QListWidgetItem* item);
  void selectedLayerOpacityChanged(int value);
  void selectCanvasLayer(std::uint64_t id);
  void beginCanvasLayerMove(std::uint64_t id);
  void previewCanvasLayerMove(std::uint64_t id, QPoint position);
  void commitCanvasLayerMove(std::uint64_t id, QPoint oldPosition, QPoint newPosition);
  void setToolFromAction(QAction* action);
  void beginRasterStroke(CanvasView::Tool tool, QPoint documentPoint);
  void previewRasterStroke(CanvasView::Tool tool, QPoint fromDocumentPoint, QPoint toDocumentPoint);
  void commitRasterStroke(CanvasView::Tool tool);
  void handleToolDocumentClick(CanvasView::Tool tool, QPoint documentPoint);
  void fitCanvasToView();
  void setCanvasToActualSize();
  void undo();
  void redo();
  void updateZoomLabel(qreal zoom);

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  struct DocumentState {
    qco::core::Document document;
    QVector<CanvasView::LayerImage> layers;
    std::uint64_t selectedLayerId = 0;
  };

  void createActions();
  void createMenus();
  void createToolRail();
  void createPanels();
  void createInitialDocument();
  void setDocument(qco::core::Document document, QVector<CanvasView::LayerImage> layers);
  void applyState(const DocumentState& state);
  void pushStateCommand(QString label, DocumentState before, DocumentState after);
  void updateLayerPanel();
  void updatePropertiesPanel();
  void updateHistoryPanel();
  void updateActions();
  void updateWindowTitle();
  void setDirty(bool dirty);
  void setSelectedLayerId(std::uint64_t id);
  void syncCanvasLayers();
  void applyStrokeToSelectedLayer(CanvasView::Tool tool, QPoint fromDocumentPoint, QPoint toDocumentPoint);
  void fillSelectedLayerAt(QPoint documentPoint);
  void addTextLayerAt(QPoint documentPoint);
  void addShapeLayerAt(QPoint documentPoint);
  void cropToSelectedLayer();
  void rememberDirectory(const QString& filePath);

  [[nodiscard]] bool maybeSaveChanges();
  [[nodiscard]] bool saveProjectToPath(QString path);
  [[nodiscard]] DocumentState captureState() const;
  [[nodiscard]] qco::core::Layer* selectedLayer() noexcept;
  [[nodiscard]] const qco::core::Layer* selectedLayer() const noexcept;
  [[nodiscard]] CanvasView::LayerImage* selectedLayerImage() noexcept;
  [[nodiscard]] const CanvasView::LayerImage* selectedLayerImage() const noexcept;
  [[nodiscard]] QRect selectedLayerDocumentRect() const;
  [[nodiscard]] QString lastDirectory() const;
  [[nodiscard]] QString documentTitle() const;
  [[nodiscard]] QString withDefaultSuffix(QString filePath, const QString& suffix) const;
  [[nodiscard]] QImage renderDocumentComposite(const QColor& background = Qt::transparent) const;
  [[nodiscard]] QVector<ProjectRasterLayer> rasterLayerPayloads() const;

  CanvasView* canvas_ = nullptr;
  QListWidget* layersList_ = nullptr;
  QLabel* propertiesLabel_ = nullptr;
  QListWidget* historyList_ = nullptr;
  QLabel* zoomLabel_ = nullptr;
  QSlider* opacitySlider_ = nullptr;
  QActionGroup* toolActionGroup_ = nullptr;
  QPushButton* addLayerButton_ = nullptr;
  QPushButton* duplicateLayerButton_ = nullptr;
  QPushButton* renameLayerButton_ = nullptr;
  QPushButton* layerUpButton_ = nullptr;
  QPushButton* layerDownButton_ = nullptr;

  QAction* newAction_ = nullptr;
  QAction* openImageAction_ = nullptr;
  QAction* openProjectAction_ = nullptr;
  QAction* saveAction_ = nullptr;
  QAction* exportAction_ = nullptr;
  QAction* addRasterLayerAction_ = nullptr;
  QAction* duplicateLayerAction_ = nullptr;
  QAction* renameLayerAction_ = nullptr;
  QAction* layerUpAction_ = nullptr;
  QAction* layerDownAction_ = nullptr;
  QAction* fitAction_ = nullptr;
  QAction* actualSizeAction_ = nullptr;
  QAction* undoAction_ = nullptr;
  QAction* redoAction_ = nullptr;

  std::unique_ptr<qco::core::Document> document_;
  QVector<CanvasView::LayerImage> layers_;
  qco::core::UndoStack undoStack_;
  std::uint64_t selectedLayerId_ = 0;
  std::optional<DocumentState> pendingMoveBeforeState_;
  std::optional<DocumentState> pendingRasterEditBeforeState_;
  QString currentProjectPath_;
  QSettings settings_;
  bool dirty_ = false;
  bool refreshingLayerPanel_ = false;
};

}  // namespace qco::ui
