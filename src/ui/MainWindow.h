#pragma once

#include "core/Document.h"
#include "core/UndoStack.h"
#include "ui/CanvasView.h"
#include "ui/ProjectArchive.h"

#include <QColor>
#include <QMainWindow>
#include <QRect>
#include <QSettings>
#include <QSize>

#include <memory>
#include <optional>

class QAction;
class QActionGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QDockWidget;
class QPushButton;
class QSlider;
class QSpinBox;
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
  void toggleSelectedLayerLock();
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
  void cropDocumentToRect(QRect documentRect);
  void updateSelectedTextLayer();
  void updateSelectedShapeLayer();
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
  void createToolOptionsPanel();
  void createInitialDocument();
  void setDocument(qco::core::Document document, QVector<CanvasView::LayerImage> layers);
  void applyState(const DocumentState& state);
  void pushStateCommand(QString label, DocumentState before, DocumentState after);
  void updateLayerPanel();
  void updatePropertiesPanel();
  void updateHistoryPanel();
  void updateActions();
  void updateToolOptionsFromSelectedLayer();
  void updateWindowTitle();
  void setDirty(bool dirty);
  void markCurrentUndoStateClean();
  void updateDirtyFromUndoStack();
  void setSelectedLayerId(std::uint64_t id);
  void syncCanvasLayers();
  void applyStrokeToSelectedLayer(CanvasView::Tool tool, QPoint fromDocumentPoint, QPoint toDocumentPoint);
  void fillSelectedLayerAt(QPoint documentPoint);
  void addTextLayerAt(QPoint documentPoint);
  void addShapeLayerAt(QPoint documentPoint);
  void cropToSelectedLayer();
  [[nodiscard]] QImage renderTextLayerImage(const QString& text, const QColor& color, int pointSize) const;
  [[nodiscard]] QImage renderShapeLayerImage(
    const QString& shapeKey,
    QSize size,
    const QColor& fillColor,
    const QColor& strokeColor,
    int strokeWidth) const;
  void updateColorButton(QPushButton* button, const QColor& color);
  void chooseToolColor(const QString& title, QColor& color, QPushButton* button);
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
  QSpinBox* brushSizeInput_ = nullptr;
  QSpinBox* brushOpacityInput_ = nullptr;
  QSpinBox* eraserSizeInput_ = nullptr;
  QSpinBox* fillToleranceInput_ = nullptr;
  QLineEdit* textContentInput_ = nullptr;
  QSpinBox* textSizeInput_ = nullptr;
  QSpinBox* shapeWidthInput_ = nullptr;
  QSpinBox* shapeHeightInput_ = nullptr;
  QSpinBox* shapeStrokeWidthInput_ = nullptr;
  QComboBox* shapeTypeInput_ = nullptr;
  QPushButton* brushColorButton_ = nullptr;
  QPushButton* fillColorButton_ = nullptr;
  QPushButton* textColorButton_ = nullptr;
  QPushButton* shapeFillColorButton_ = nullptr;
  QPushButton* shapeStrokeColorButton_ = nullptr;
  QPushButton* cropSelectedLayerButton_ = nullptr;
  QPushButton* updateTextLayerButton_ = nullptr;
  QPushButton* updateShapeLayerButton_ = nullptr;
  QActionGroup* toolActionGroup_ = nullptr;
  QPushButton* addLayerButton_ = nullptr;
  QPushButton* duplicateLayerButton_ = nullptr;
  QPushButton* renameLayerButton_ = nullptr;
  QPushButton* lockLayerButton_ = nullptr;
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
  QAction* toggleLayerLockAction_ = nullptr;
  QAction* layerUpAction_ = nullptr;
  QAction* layerDownAction_ = nullptr;
  QAction* fitAction_ = nullptr;
  QAction* actualSizeAction_ = nullptr;
  QAction* undoAction_ = nullptr;
  QAction* redoAction_ = nullptr;

  std::unique_ptr<qco::core::Document> document_;
  QVector<CanvasView::LayerImage> layers_;
  qco::core::UndoStack undoStack_;
  std::optional<std::size_t> cleanUndoIndex_;
  std::uint64_t selectedLayerId_ = 0;
  std::optional<DocumentState> pendingMoveBeforeState_;
  std::optional<DocumentState> pendingRasterEditBeforeState_;
  QString currentProjectPath_;
  QSettings settings_;
  QColor brushColor_ = QColor(24, 24, 24, 255);
  QColor fillColor_ = QColor(24, 24, 24, 255);
  QColor textColor_ = QColor(24, 24, 24, 255);
  QColor shapeFillColor_ = QColor(45, 156, 219, 120);
  QColor shapeStrokeColor_ = QColor(24, 24, 24, 255);
  int brushSize_ = 24;
  int brushOpacity_ = 100;
  int eraserSize_ = 24;
  int fillTolerance_ = 0;
  int textPointSize_ = 48;
  int shapeWidth_ = 240;
  int shapeHeight_ = 160;
  int shapeStrokeWidth_ = 4;
  bool dirty_ = false;
  bool refreshingLayerPanel_ = false;
};

}  // namespace qco::ui
