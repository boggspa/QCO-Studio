#include "ui/MainWindow.h"

#include "app/Logging.h"
#include "image/ImageCodec.h"
#include "render/DocumentRenderer.h"
#include "ui/ImageExport.h"
#include "ui/ProjectArchive.h"

#include <QAction>
#include <QAbstractItemView>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QToolBar>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace qco::ui {
namespace {

constexpr int defaultDocumentWidth = 1920;
constexpr int defaultDocumentHeight = 1080;

[[nodiscard]] QString readableFileFilters()
{
  return qco::image::availableImageOpenFileFilter();
}

[[nodiscard]] QString projectFileFilter()
{
  return QObject::tr("QCO Studio Document (*.qco)");
}

[[nodiscard]] qco::core::Size toCoreSize(QSize size)
{
  return {size.width(), size.height()};
}

[[nodiscard]] QSize toQtSize(qco::core::Size size)
{
  return QSize(size.width, size.height);
}

[[nodiscard]] qco::core::Point toCorePoint(QPoint point)
{
  return {point.x(), point.y()};
}

[[nodiscard]] bool colorsWithinTolerance(const QColor& left, const QColor& right, int tolerance)
{
  return std::abs(left.red() - right.red()) <= tolerance
         && std::abs(left.green() - right.green()) <= tolerance
         && std::abs(left.blue() - right.blue()) <= tolerance
         && std::abs(left.alpha() - right.alpha()) <= tolerance;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent),
    settings_()
{
  canvas_ = new CanvasView(this);
  setCentralWidget(canvas_);

  createActions();
  createMenus();
  createToolRail();
  createPanels();

  zoomLabel_ = new QLabel(this);
  statusBar()->addPermanentWidget(zoomLabel_);
  statusBar()->showMessage(tr("Ready"));

  connect(canvas_, &CanvasView::zoomChanged, this, &MainWindow::updateZoomLabel);
  connect(canvas_, &CanvasView::layerSelected, this, &MainWindow::selectCanvasLayer);
  connect(canvas_, &CanvasView::layerMoveStarted, this, &MainWindow::beginCanvasLayerMove);
  connect(canvas_, &CanvasView::layerMovePreview, this, &MainWindow::previewCanvasLayerMove);
  connect(canvas_, &CanvasView::layerMoveCommitted, this, &MainWindow::commitCanvasLayerMove);
  connect(canvas_, &CanvasView::rasterStrokeStarted, this, &MainWindow::beginRasterStroke);
  connect(canvas_, &CanvasView::rasterStrokePreview, this, &MainWindow::previewRasterStroke);
  connect(canvas_, &CanvasView::rasterStrokeCommitted, this, &MainWindow::commitRasterStroke);
  connect(canvas_, &CanvasView::toolDocumentClicked, this, &MainWindow::handleToolDocumentClick);
  connect(canvas_, &CanvasView::cropCommitted, this, &MainWindow::cropDocumentToRect);

  restoreGeometry(settings_.value(QStringLiteral("window/geometry")).toByteArray());
  restoreState(settings_.value(QStringLiteral("window/state")).toByteArray());

  createInitialDocument();
}

MainWindow::~MainWindow()
{
  settings_.setValue(QStringLiteral("window/geometry"), saveGeometry());
  settings_.setValue(QStringLiteral("window/state"), saveState());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
  if (maybeSaveChanges()) {
    event->accept();
    return;
  }

  event->ignore();
}

void MainWindow::newDocument()
{
  if (!maybeSaveChanges()) {
    return;
  }

  QDialog dialog(this);
  dialog.setWindowTitle(tr("New Document"));

  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();

  auto* widthInput = new QSpinBox(&dialog);
  widthInput->setRange(1, 100000);
  widthInput->setValue(defaultDocumentWidth);
  widthInput->setSuffix(tr(" px"));

  auto* heightInput = new QSpinBox(&dialog);
  heightInput->setRange(1, 100000);
  heightInput->setValue(defaultDocumentHeight);
  heightInput->setSuffix(tr(" px"));

  form->addRow(tr("Width"), widthInput);
  form->addRow(tr("Height"), heightInput);
  layout->addLayout(form);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  auto document = qco::core::Document::create(
    "Untitled",
    {widthInput->value(), heightInput->value()});

  QVector<CanvasView::LayerImage> layers;
  setDocument(std::move(document), std::move(layers));
  currentProjectPath_.clear();
  undoStack_.clear();
  markCurrentUndoStateClean();
  updateHistoryPanel();
  statusBar()->showMessage(tr("Created new document"), 3000);
}

void MainWindow::openImage()
{
  if (!maybeSaveChanges()) {
    return;
  }

  const auto path = QFileDialog::getOpenFileName(
    this,
    tr("Open Image"),
    lastDirectory(),
    readableFileFilters());

  if (path.isEmpty()) {
    return;
  }

  (void)openImageFromPath(path);
}

bool MainWindow::openImageFromPath(QString path)
{
  if (path.isEmpty()) {
    return false;
  }

  const qco::image::QtImageCodec codec;
  QString error;
  const auto decoded = codec.read(path, &error);
  if (!decoded.has_value()) {
    QMessageBox::warning(
      this,
      tr("Open Image"),
      tr("The selected file could not be opened as an image.\n%1").arg(error));
    return false;
  }

  rememberDirectory(path);

  const QImage& image = decoded->image;
  const QFileInfo info(path);
  auto document = qco::core::Document::create(
    info.completeBaseName().toStdString(),
    toCoreSize(image.size()));

  const auto layerId = document.addLayer(
    info.fileName().toStdString(),
    qco::core::LayerType::Raster,
    toCoreSize(image.size()));

  CanvasView::LayerImage layer;
  layer.id = layerId;
  layer.name = info.fileName();
  layer.image = image;
  layer.position = QPoint(0, 0);

  QVector<CanvasView::LayerImage> layers;
  layers.push_back(std::move(layer));

  setDocument(std::move(document), std::move(layers));
  currentProjectPath_.clear();
  undoStack_.clear();
  cleanUndoIndex_.reset();
  setDirty(true);
  updateHistoryPanel();
  statusBar()->showMessage(tr("Opened %1").arg(info.fileName()), 3000);
  return true;
}

void MainWindow::openProject()
{
  if (!maybeSaveChanges()) {
    return;
  }

  const auto path = QFileDialog::getOpenFileName(
    this,
    tr("Open Project"),
    lastDirectory(),
    projectFileFilter());

  if (path.isEmpty()) {
    return;
  }

  (void)openProjectFromPath(path);
}

bool MainWindow::openProjectFromPath(QString path)
{
  if (path.isEmpty()) {
    return false;
  }

  QString error;
  auto loadedProject = ProjectArchive::load(path, &error);
  if (!loadedProject.has_value()) {
    QMessageBox::critical(this, tr("Open Project"), tr("The project could not be opened:\n%1").arg(error));
    return false;
  }

  QVector<CanvasView::LayerImage> canvasLayers;
  canvasLayers.reserve(loadedProject->rasterLayers.size());
  for (const auto& rasterLayer : loadedProject->rasterLayers) {
    CanvasView::LayerImage layer;
    layer.id = rasterLayer.id;
    layer.name = rasterLayer.name;
    layer.image = rasterLayer.image;
    layer.position = rasterLayer.position;
    layer.visible = rasterLayer.visible;
    layer.opacity = rasterLayer.opacity;
    canvasLayers.push_back(std::move(layer));
  }

  setDocument(std::move(loadedProject->document), std::move(canvasLayers));
  currentProjectPath_ = path;
  undoStack_.clear();
  markCurrentUndoStateClean();
  rememberDirectory(path);
  updateHistoryPanel();
  statusBar()->showMessage(tr("Opened project"), 3000);
  return true;
}

void MainWindow::saveProject()
{
  if (!document_) {
    return;
  }

  auto path = currentProjectPath_;
  if (path.isEmpty()) {
    path = QFileDialog::getSaveFileName(
      this,
      tr("Save Project"),
      lastDirectory() + QLatin1Char('/') + documentTitle() + QStringLiteral(".qco"),
      projectFileFilter());
  }

  if (path.isEmpty()) {
    return;
  }

  (void)saveProjectToPath(path);
}

bool MainWindow::saveProjectToPath(QString path)
{
  if (!document_) {
    return false;
  }

  if (path.isEmpty()) {
    return false;
  }

  path = withDefaultSuffix(path, QStringLiteral("qco"));

  QString error;
  if (!ProjectArchive::save(path, *document_, rasterLayerPayloads(), &error)) {
    QMessageBox::critical(this, tr("Save Project"), tr("The project could not be saved:\n%1").arg(error));
    return false;
  }

  currentProjectPath_ = path;
  rememberDirectory(path);
  markCurrentUndoStateClean();
  statusBar()->showMessage(tr("Saved project"), 3000);
  return true;
}

void MainWindow::exportImage()
{
  if (!document_) {
    return;
  }

  QString selectedFilter;
  const auto exportFormats = availableExportImageFormats();
  if (exportFormats.isEmpty()) {
    QMessageBox::critical(this, tr("Export Image"), tr("No supported image export formats are available."));
    return;
  }

  auto path = QFileDialog::getSaveFileName(
    this,
    tr("Export Image"),
    lastDirectory() + QLatin1Char('/') + documentTitle() + QLatin1Char('.') + exportFormats.front().defaultSuffix,
    exportImageFileFilter(exportFormats),
    &selectedFilter);

  if (path.isEmpty()) {
    return;
  }

  (void)exportImageToPath(path, selectedFilter);
}

bool MainWindow::exportImageToPath(QString path, QString selectedFilter)
{
  if (!document_) {
    return false;
  }
  if (path.isEmpty()) {
    return false;
  }

  const auto exportFormats = availableExportImageFormats();
  if (exportFormats.isEmpty()) {
    QMessageBox::critical(this, tr("Export Image"), tr("No supported image export formats are available."));
    return false;
  }

  const auto exportFormat = exportImageFormatForPathOrFilter(exportFormats, path, selectedFilter);
  if (!exportFormat.has_value()) {
    QMessageBox::critical(this, tr("Export Image"), tr("No supported image export format was selected."));
    return false;
  }

  path = withDefaultSuffix(path, exportFormat->defaultSuffix);

  QString error;
  if (!writeExportImage(path, renderDocumentComposite(Qt::transparent), *exportFormat, &error)) {
    QMessageBox::critical(this, tr("Export Image"), tr("The image could not be exported:\n%1").arg(error));
    return false;
  }

  rememberDirectory(path);
  statusBar()->showMessage(tr("Exported image"), 3000);
  return true;
}

void MainWindow::addRasterLayer()
{
  if (!document_) {
    return;
  }

  auto before = captureState();
  const auto canvasSize = document_->canvasSize();
  const auto name = tr("Raster Layer %1").arg(document_->layers().size() + 1);
  const auto layerId = document_->addLayer(
    name.toStdString(),
    qco::core::LayerType::Raster,
    canvasSize);

  CanvasView::LayerImage layer;
  layer.id = layerId;
  layer.name = name;
  layer.image = QImage(toQtSize(canvasSize), QImage::Format_ARGB32_Premultiplied);
  layer.image.fill(Qt::transparent);
  layer.position = QPoint(0, 0);
  layers_.push_back(std::move(layer));
  selectedLayerId_ = layerId;

  pushStateCommand(tr("Add Raster Layer"), std::move(before), captureState());
}

void MainWindow::duplicateSelectedLayer()
{
  const auto* sourceLayer = selectedLayer();
  const auto* sourceImage = selectedLayerImage();
  if (!document_ || sourceLayer == nullptr || sourceImage == nullptr) {
    return;
  }

  auto before = captureState();
  const auto source = *sourceLayer;
  const auto sourcePayload = *sourceImage;
  const auto name = tr("%1 Copy").arg(QString::fromStdString(source.name));
  const auto newLayerId = document_->addLayer(
    name.toStdString(),
    source.type,
    source.size,
    {source.position.x + 16, source.position.y + 16});

  auto* newLayer = document_->findLayer(newLayerId);
  if (newLayer != nullptr) {
    newLayer->visible = source.visible;
    newLayer->locked = source.locked;
    newLayer->opacity = source.opacity;
    newLayer->blendMode = source.blendMode;
    newLayer->textPayload = source.textPayload;
    newLayer->shapePayload = source.shapePayload;
  }

  CanvasView::LayerImage duplicate = sourcePayload;
  duplicate.id = newLayerId;
  duplicate.name = name;
  duplicate.position += QPoint(16, 16);
  layers_.push_back(std::move(duplicate));
  selectedLayerId_ = newLayerId;

  pushStateCommand(tr("Duplicate Layer"), std::move(before), captureState());
}

void MainWindow::deleteSelectedLayer()
{
  const auto* layer = selectedLayer();
  if (!document_ || selectedLayerId_ == 0 || layer == nullptr) {
    return;
  }
  if (layer->locked) {
    statusBar()->showMessage(tr("Unlock the layer before deleting it"), 3000);
    return;
  }

  const auto currentIndex = document_->layerIndex(selectedLayerId_);
  if (!currentIndex.has_value()) {
    return;
  }

  const auto deletedLayerId = selectedLayerId_;
  auto before = captureState();
  if (!document_->removeLayer(deletedLayerId)) {
    return;
  }

  layers_.erase(
    std::remove_if(layers_.begin(), layers_.end(), [deletedLayerId](const CanvasView::LayerImage& imageLayer) {
      return imageLayer.id == deletedLayerId;
    }),
    layers_.end());

  if (document_->layers().empty()) {
    selectedLayerId_ = 0;
  } else {
    const auto nextIndex = std::min(*currentIndex, document_->layers().size() - 1);
    selectedLayerId_ = document_->layers()[nextIndex].id;
  }
  pendingMoveBeforeState_.reset();
  pendingRasterEditBeforeState_.reset();

  pushStateCommand(tr("Delete Layer"), std::move(before), captureState());
}

void MainWindow::renameSelectedLayer()
{
  const auto* layer = selectedLayer();
  if (!document_ || layer == nullptr) {
    return;
  }

  bool ok = false;
  const auto currentName = QString::fromStdString(layer->name);
  const auto newName = QInputDialog::getText(
    this,
    tr("Rename Layer"),
    tr("Name"),
    QLineEdit::Normal,
    currentName,
    &ok);

  if (!ok) {
    return;
  }

  (void)renameSelectedLayerTo(newName);
}

bool MainWindow::renameSelectedLayerTo(QString name)
{
  const auto* layer = selectedLayer();
  if (!document_ || layer == nullptr) {
    return false;
  }

  name = name.trimmed();
  const auto currentName = QString::fromStdString(layer->name);
  if (name.isEmpty() || name == currentName) {
    return false;
  }

  auto before = captureState();
  if (!document_->setLayerName(selectedLayerId_, name.toStdString())) {
    return false;
  }
  if (auto* image = selectedLayerImage()) {
    image->name = name;
  }

  pushStateCommand(tr("Rename Layer"), std::move(before), captureState());
  return true;
}

void MainWindow::toggleSelectedLayerLock()
{
  const auto* layer = selectedLayer();
  if (!document_ || selectedLayerId_ == 0 || layer == nullptr) {
    return;
  }

  const bool nextLocked = !layer->locked;
  auto before = captureState();
  if (!document_->setLayerLocked(selectedLayerId_, nextLocked)) {
    return;
  }

  pushStateCommand(nextLocked ? tr("Lock Layer") : tr("Unlock Layer"), std::move(before), captureState());
  statusBar()->showMessage(nextLocked ? tr("Layer locked") : tr("Layer unlocked"), 2000);
}

void MainWindow::moveSelectedLayerUp()
{
  if (!document_ || selectedLayerId_ == 0) {
    return;
  }

  const auto currentIndex = document_->layerIndex(selectedLayerId_);
  if (!currentIndex.has_value() || *currentIndex + 1 >= document_->layers().size()) {
    return;
  }

  auto before = captureState();
  const auto newIndex = *currentIndex + 1;
  if (!document_->moveLayer(selectedLayerId_, newIndex)) {
    return;
  }
  layers_.move(static_cast<qsizetype>(*currentIndex), static_cast<qsizetype>(newIndex));

  pushStateCommand(tr("Move Layer Up"), std::move(before), captureState());
}

void MainWindow::moveSelectedLayerDown()
{
  if (!document_ || selectedLayerId_ == 0) {
    return;
  }

  const auto currentIndex = document_->layerIndex(selectedLayerId_);
  if (!currentIndex.has_value() || *currentIndex == 0) {
    return;
  }

  auto before = captureState();
  const auto newIndex = *currentIndex - 1;
  if (!document_->moveLayer(selectedLayerId_, newIndex)) {
    return;
  }
  layers_.move(static_cast<qsizetype>(*currentIndex), static_cast<qsizetype>(newIndex));

  pushStateCommand(tr("Move Layer Down"), std::move(before), captureState());
}

void MainWindow::layerSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
  Q_UNUSED(previous);
  if (refreshingLayerPanel_) {
    return;
  }

  const auto id = current == nullptr ? 0 : current->data(Qt::UserRole).toULongLong();
  setSelectedLayerId(id);
}

void MainWindow::layerItemChanged(QListWidgetItem* item)
{
  if (refreshingLayerPanel_ || item == nullptr || !document_) {
    return;
  }

  const auto id = item->data(Qt::UserRole).toULongLong();
  auto* layer = document_->findLayer(id);
  if (layer == nullptr) {
    return;
  }

  const auto visible = item->checkState() == Qt::Checked;
  if (layer->visible == visible) {
    return;
  }

  auto before = captureState();
  if (!document_->setLayerVisibility(id, visible)) {
    return;
  }
  const auto imageIt = std::find_if(layers_.begin(), layers_.end(), [id](const CanvasView::LayerImage& imageLayer) {
    return imageLayer.id == id;
  });
  if (imageIt != layers_.end()) {
    imageIt->visible = visible;
  }

  pushStateCommand(visible ? tr("Show Layer") : tr("Hide Layer"), std::move(before), captureState());
}

void MainWindow::selectedLayerOpacityChanged(int value)
{
  if (refreshingLayerPanel_ || selectedLayerId_ == 0 || !document_) {
    return;
  }

  const auto opacity = std::clamp(value / 100.0, 0.0, 1.0);
  const auto* layer = selectedLayer();
  if (layer == nullptr || qFuzzyCompare(layer->opacity, opacity)) {
    return;
  }

  auto before = captureState();
  if (!document_->setLayerOpacity(selectedLayerId_, opacity)) {
    return;
  }
  if (auto* image = selectedLayerImage()) {
    image->opacity = opacity;
  }

  pushStateCommand(tr("Change Layer Opacity"), std::move(before), captureState());
}

void MainWindow::selectCanvasLayer(std::uint64_t id)
{
  setSelectedLayerId(id);
}

void MainWindow::beginCanvasLayerMove(std::uint64_t id)
{
  if (id == 0) {
    pendingMoveBeforeState_.reset();
    return;
  }

  setSelectedLayerId(id);
  const auto* layer = selectedLayer();
  if (layer != nullptr && layer->locked) {
    pendingMoveBeforeState_.reset();
    statusBar()->showMessage(tr("Selected layer is locked"), 3000);
    return;
  }

  pendingMoveBeforeState_ = captureState();
}

void MainWindow::previewCanvasLayerMove(std::uint64_t id, QPoint position)
{
  if (!document_ || id == 0) {
    return;
  }

  const auto* layer = document_->findLayer(id);
  if (layer == nullptr || layer->locked || !document_->setLayerPosition(id, toCorePoint(position))) {
    return;
  }
  const auto imageIt = std::find_if(layers_.begin(), layers_.end(), [id](const CanvasView::LayerImage& imageLayer) {
    return imageLayer.id == id;
  });
  if (imageIt != layers_.end()) {
    imageIt->position = position;
  }
  syncCanvasLayers();
  updatePropertiesPanel();
}

void MainWindow::commitCanvasLayerMove(std::uint64_t id, QPoint oldPosition, QPoint newPosition)
{
  const auto* layer = document_ == nullptr ? nullptr : document_->findLayer(id);
  if (!pendingMoveBeforeState_.has_value() || id == 0 || oldPosition == newPosition || layer == nullptr || layer->locked) {
    pendingMoveBeforeState_.reset();
    return;
  }

  auto before = std::move(*pendingMoveBeforeState_);
  pendingMoveBeforeState_.reset();
  pushStateCommand(tr("Move Layer"), std::move(before), captureState());
}

void MainWindow::setToolFromAction(QAction* action)
{
  if (action == nullptr) {
    return;
  }

  const auto toolValue = action->data().toInt();
  const auto tool = static_cast<CanvasView::Tool>(toolValue);
  canvas_->setActiveTool(tool);
  statusBar()->showMessage(tr("%1 tool").arg(action->text()), 2000);
}

void MainWindow::beginRasterStroke(CanvasView::Tool tool, QPoint documentPoint)
{
  Q_UNUSED(documentPoint);
  if (tool != CanvasView::Tool::Brush && tool != CanvasView::Tool::Eraser) {
    pendingRasterEditBeforeState_.reset();
    return;
  }

  if (selectedLayerImage() == nullptr) {
    statusBar()->showMessage(tr("Select or add a raster layer first"), 3000);
    pendingRasterEditBeforeState_.reset();
    return;
  }
  const auto* layer = selectedLayer();
  if (layer != nullptr && layer->locked) {
    statusBar()->showMessage(tr("Selected layer is locked"), 3000);
    pendingRasterEditBeforeState_.reset();
    return;
  }

  pendingRasterEditBeforeState_ = captureState();
}

void MainWindow::previewRasterStroke(CanvasView::Tool tool, QPoint fromDocumentPoint, QPoint toDocumentPoint)
{
  if (!pendingRasterEditBeforeState_.has_value()) {
    return;
  }

  applyStrokeToSelectedLayer(tool, fromDocumentPoint, toDocumentPoint);
}

void MainWindow::commitRasterStroke(CanvasView::Tool tool)
{
  if (!pendingRasterEditBeforeState_.has_value()) {
    return;
  }

  auto before = std::move(*pendingRasterEditBeforeState_);
  pendingRasterEditBeforeState_.reset();
  pushStateCommand(tool == CanvasView::Tool::Eraser ? tr("Erase Stroke") : tr("Brush Stroke"),
                   std::move(before),
                   captureState());
}

void MainWindow::handleToolDocumentClick(CanvasView::Tool tool, QPoint documentPoint)
{
  switch (tool) {
    case CanvasView::Tool::Fill:
      fillSelectedLayerAt(documentPoint);
      break;
    case CanvasView::Tool::Text:
      addTextLayerAt(documentPoint);
      break;
    case CanvasView::Tool::Shape:
      addShapeLayerAt(documentPoint);
      break;
    default:
      break;
  }
}

void MainWindow::updateSelectedTextLayer()
{
  const auto* selected = selectedLayer();
  if (!document_ || selected == nullptr || selectedLayerImage() == nullptr || selected->type != qco::core::LayerType::Text) {
    statusBar()->showMessage(tr("Select a text layer to update"), 3000);
    return;
  }
  if (selected->locked) {
    statusBar()->showMessage(tr("Selected layer is locked"), 3000);
    return;
  }

  const auto text = textContentInput_ == nullptr ? QString() : textContentInput_->text().trimmed();
  if (text.isEmpty()) {
    statusBar()->showMessage(tr("Enter text in Tool Options first"), 3000);
    return;
  }

  qco::core::TextLayerPayload payload;
  payload.text = text.toStdString();
  payload.color = textColor_.name(QColor::HexArgb).toStdString();
  payload.pointSize = textPointSize_;

  const auto image = renderTextLayerImage(text, textColor_, textPointSize_);
  const auto unchanged = selected->textPayload.has_value()
                         && selected->textPayload->text == payload.text
                         && selected->textPayload->color == payload.color
                         && selected->textPayload->pointSize == payload.pointSize
                         && selected->size.width == image.width()
                         && selected->size.height == image.height();
  if (unchanged) {
    statusBar()->showMessage(tr("Selected text layer is already up to date"), 2000);
    return;
  }

  auto before = captureState();
  auto* layer = selectedLayer();
  auto* imageLayer = selectedLayerImage();
  if (layer == nullptr || imageLayer == nullptr) {
    return;
  }
  layer->name = text.toStdString();
  layer->size = toCoreSize(image.size());
  layer->textPayload = std::move(payload);
  imageLayer->name = text;
  imageLayer->image = image;

  pushStateCommand(tr("Update Text Layer"), std::move(before), captureState());
}

void MainWindow::updateSelectedShapeLayer()
{
  const auto* selected = selectedLayer();
  if (!document_ || selected == nullptr || selectedLayerImage() == nullptr || selected->type != qco::core::LayerType::Shape) {
    statusBar()->showMessage(tr("Select a shape layer to update"), 3000);
    return;
  }
  if (selected->locked) {
    statusBar()->showMessage(tr("Selected layer is locked"), 3000);
    return;
  }

  const auto shapeKey = shapeTypeInput_ == nullptr ? QStringLiteral("rectangle") : shapeTypeInput_->currentData().toString();
  const auto shape = shapeTypeInput_ == nullptr ? tr("Rectangle") : shapeTypeInput_->currentText();
  const QSize size(shapeWidth_, shapeHeight_);

  qco::core::ShapeLayerPayload payload;
  payload.shape = shapeKey.toStdString();
  payload.fillColor = shapeFillColor_.name(QColor::HexArgb).toStdString();
  payload.strokeColor = shapeStrokeColor_.name(QColor::HexArgb).toStdString();
  payload.strokeWidth = shapeStrokeWidth_;

  const auto image = renderShapeLayerImage(shapeKey, size, shapeFillColor_, shapeStrokeColor_, shapeStrokeWidth_);
  const auto unchanged = selected->shapePayload.has_value()
                         && selected->shapePayload->shape == payload.shape
                         && selected->shapePayload->fillColor == payload.fillColor
                         && selected->shapePayload->strokeColor == payload.strokeColor
                         && selected->shapePayload->strokeWidth == payload.strokeWidth
                         && selected->size.width == image.width()
                         && selected->size.height == image.height();
  if (unchanged) {
    statusBar()->showMessage(tr("Selected shape layer is already up to date"), 2000);
    return;
  }

  auto before = captureState();
  auto* layer = selectedLayer();
  auto* imageLayer = selectedLayerImage();
  if (layer == nullptr || imageLayer == nullptr) {
    return;
  }
  layer->name = shape.toStdString();
  layer->size = toCoreSize(image.size());
  layer->shapePayload = std::move(payload);
  imageLayer->name = shape;
  imageLayer->image = image;

  pushStateCommand(tr("Update Shape Layer"), std::move(before), captureState());
}

void MainWindow::cropDocumentToRect(QRect documentRect)
{
  if (!document_) {
    return;
  }

  documentRect = documentRect.intersected(QRect(QPoint(0, 0), toQtSize(document_->canvasSize())));
  if (!documentRect.isValid() || documentRect.isEmpty()) {
    statusBar()->showMessage(tr("Drag a crop area inside the document"), 3000);
    return;
  }

  if (documentRect.topLeft() == QPoint(0, 0) && documentRect.size() == toQtSize(document_->canvasSize())) {
    statusBar()->showMessage(tr("Crop area already matches the canvas"), 3000);
    return;
  }

  auto before = captureState();
  document_->resizeCanvas(toCoreSize(documentRect.size()));

  const auto layerSnapshot = document_->layers();
  for (const auto& layer : layerSnapshot) {
    (void)document_->setLayerPosition(layer.id, {layer.position.x - documentRect.x(), layer.position.y - documentRect.y()});
  }

  for (auto& imageLayer : layers_) {
    imageLayer.position -= documentRect.topLeft();
  }

  pushStateCommand(tr("Crop Canvas"), std::move(before), captureState());
}

void MainWindow::fitCanvasToView()
{
  canvas_->fitToView();
}

void MainWindow::setCanvasToActualSize()
{
  canvas_->setZoom(1.0);
}

void MainWindow::undo()
{
  if (undoStack_.undo()) {
    updateDirtyFromUndoStack();
    updateHistoryPanel();
    updateActions();
  }
}

void MainWindow::redo()
{
  if (undoStack_.redo()) {
    updateDirtyFromUndoStack();
    updateHistoryPanel();
    updateActions();
  }
}

void MainWindow::updateZoomLabel(qreal zoom)
{
  zoomLabel_->setText(tr("%1%").arg(qRound(zoom * 100.0)));
}

void MainWindow::createActions()
{
  const auto* style = QApplication::style();

  newAction_ = new QAction(style->standardIcon(QStyle::SP_FileIcon), tr("New"), this);
  newAction_->setObjectName(QStringLiteral("newAction"));
  newAction_->setShortcut(QKeySequence::New);
  connect(newAction_, &QAction::triggered, this, &MainWindow::newDocument);

  openProjectAction_ = new QAction(style->standardIcon(QStyle::SP_DirOpenIcon), tr("Open Project"), this);
  openProjectAction_->setObjectName(QStringLiteral("openProjectAction"));
  openProjectAction_->setShortcut(QKeySequence::Open);
  connect(openProjectAction_, &QAction::triggered, this, &MainWindow::openProject);

  openImageAction_ = new QAction(style->standardIcon(QStyle::SP_DirOpenIcon), tr("Open Image"), this);
  openImageAction_->setObjectName(QStringLiteral("openImageAction"));
  openImageAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
  connect(openImageAction_, &QAction::triggered, this, &MainWindow::openImage);

  saveAction_ = new QAction(style->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project"), this);
  saveAction_->setObjectName(QStringLiteral("saveProjectAction"));
  saveAction_->setShortcut(QKeySequence::Save);
  connect(saveAction_, &QAction::triggered, this, &MainWindow::saveProject);

  exportAction_ = new QAction(tr("Export Image"), this);
  exportAction_->setObjectName(QStringLiteral("exportImageAction"));
  exportAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
  connect(exportAction_, &QAction::triggered, this, &MainWindow::exportImage);

  addRasterLayerAction_ = new QAction(tr("Add Raster Layer"), this);
  addRasterLayerAction_->setObjectName(QStringLiteral("addRasterLayerAction"));
  addRasterLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
  connect(addRasterLayerAction_, &QAction::triggered, this, &MainWindow::addRasterLayer);

  duplicateLayerAction_ = new QAction(tr("Duplicate Layer"), this);
  duplicateLayerAction_->setObjectName(QStringLiteral("duplicateLayerAction"));
  duplicateLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
  connect(duplicateLayerAction_, &QAction::triggered, this, &MainWindow::duplicateSelectedLayer);

  deleteLayerAction_ = new QAction(tr("Delete Layer"), this);
  deleteLayerAction_->setObjectName(QStringLiteral("deleteLayerAction"));
  deleteLayerAction_->setShortcut(QKeySequence::Delete);
  connect(deleteLayerAction_, &QAction::triggered, this, &MainWindow::deleteSelectedLayer);

  renameLayerAction_ = new QAction(tr("Rename Layer"), this);
  renameLayerAction_->setObjectName(QStringLiteral("renameLayerAction"));
  connect(renameLayerAction_, &QAction::triggered, this, &MainWindow::renameSelectedLayer);

  toggleLayerLockAction_ = new QAction(tr("Lock Layer"), this);
  toggleLayerLockAction_->setObjectName(QStringLiteral("toggleLayerLockAction"));
  connect(toggleLayerLockAction_, &QAction::triggered, this, &MainWindow::toggleSelectedLayerLock);

  layerUpAction_ = new QAction(tr("Move Layer Up"), this);
  layerUpAction_->setObjectName(QStringLiteral("moveLayerUpAction"));
  layerUpAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
  connect(layerUpAction_, &QAction::triggered, this, &MainWindow::moveSelectedLayerUp);

  layerDownAction_ = new QAction(tr("Move Layer Down"), this);
  layerDownAction_->setObjectName(QStringLiteral("moveLayerDownAction"));
  layerDownAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
  connect(layerDownAction_, &QAction::triggered, this, &MainWindow::moveSelectedLayerDown);

  fitAction_ = new QAction(tr("Fit to View"), this);
  fitAction_->setObjectName(QStringLiteral("fitToViewAction"));
  fitAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
  connect(fitAction_, &QAction::triggered, this, &MainWindow::fitCanvasToView);

  actualSizeAction_ = new QAction(tr("Actual Size"), this);
  actualSizeAction_->setObjectName(QStringLiteral("actualSizeAction"));
  actualSizeAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
  connect(actualSizeAction_, &QAction::triggered, this, &MainWindow::setCanvasToActualSize);

  undoAction_ = new QAction(tr("Undo"), this);
  undoAction_->setObjectName(QStringLiteral("undoAction"));
  undoAction_->setShortcut(QKeySequence::Undo);
  connect(undoAction_, &QAction::triggered, this, &MainWindow::undo);

  redoAction_ = new QAction(tr("Redo"), this);
  redoAction_->setObjectName(QStringLiteral("redoAction"));
  redoAction_->setShortcut(QKeySequence::Redo);
  connect(redoAction_, &QAction::triggered, this, &MainWindow::redo);
}

void MainWindow::createMenus()
{
  auto* fileMenu = menuBar()->addMenu(tr("File"));
  fileMenu->addAction(newAction_);
  fileMenu->addAction(openProjectAction_);
  fileMenu->addAction(openImageAction_);
  fileMenu->addSeparator();
  fileMenu->addAction(saveAction_);
  fileMenu->addAction(exportAction_);
  fileMenu->addSeparator();
  fileMenu->addAction(tr("Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

  auto* editMenu = menuBar()->addMenu(tr("Edit"));
  editMenu->addAction(undoAction_);
  editMenu->addAction(redoAction_);

  auto* layerMenu = menuBar()->addMenu(tr("Layer"));
  layerMenu->addAction(addRasterLayerAction_);
  layerMenu->addAction(duplicateLayerAction_);
  layerMenu->addAction(deleteLayerAction_);
  layerMenu->addAction(renameLayerAction_);
  layerMenu->addAction(toggleLayerLockAction_);
  layerMenu->addSeparator();
  layerMenu->addAction(layerUpAction_);
  layerMenu->addAction(layerDownAction_);

  auto* viewMenu = menuBar()->addMenu(tr("View"));
  viewMenu->addAction(fitAction_);
  viewMenu->addAction(actualSizeAction_);
}

void MainWindow::createToolRail()
{
  auto* toolRail = new QToolBar(tr("Tools"), this);
  toolRail->setObjectName(QStringLiteral("toolRail"));
  toolRail->setOrientation(Qt::Vertical);
  toolRail->setMovable(false);
  toolRail->setIconSize(QSize(20, 20));
  addToolBar(Qt::LeftToolBarArea, toolRail);

  toolActionGroup_ = new QActionGroup(this);
  toolActionGroup_->setExclusive(true);
  connect(toolActionGroup_, &QActionGroup::triggered, this, &MainWindow::setToolFromAction);

  const auto addTool = [this, toolRail](const QString& toolName, CanvasView::Tool tool) {
    auto* action = toolRail->addAction(toolName);
    action->setCheckable(true);
    action->setToolTip(toolName);
    action->setData(static_cast<int>(tool));
    toolActionGroup_->addAction(action);
    return action;
  };

  auto* moveAction = addTool(tr("Move"), CanvasView::Tool::Move);
  addTool(tr("Select"), CanvasView::Tool::Select);
  addTool(tr("Crop"), CanvasView::Tool::Crop);
  addTool(tr("Brush"), CanvasView::Tool::Brush);
  addTool(tr("Erase"), CanvasView::Tool::Eraser);
  addTool(tr("Fill"), CanvasView::Tool::Fill);
  addTool(tr("Text"), CanvasView::Tool::Text);
  addTool(tr("Shape"), CanvasView::Tool::Shape);
  addTool(tr("Pen"), CanvasView::Tool::Pen);
  addTool(tr("Pick"), CanvasView::Tool::Pick);
  moveAction->setChecked(true);
  canvas_->setActiveTool(CanvasView::Tool::Move);
}

void MainWindow::createPanels()
{
  auto* layersDock = new QDockWidget(tr("Layers"), this);
  layersDock->setObjectName(QStringLiteral("layersDock"));
  auto* layersPanel = new QWidget(layersDock);
  auto* layersLayout = new QVBoxLayout(layersPanel);
  layersLayout->setContentsMargins(8, 8, 8, 8);
  layersLayout->setSpacing(6);

  layersList_ = new QListWidget(layersPanel);
  layersList_->setSelectionMode(QAbstractItemView::SingleSelection);
  layersLayout->addWidget(layersList_, 1);

  auto* buttonRow = new QHBoxLayout();
  addLayerButton_ = new QPushButton(tr("Add"), layersPanel);
  duplicateLayerButton_ = new QPushButton(tr("Duplicate"), layersPanel);
  deleteLayerButton_ = new QPushButton(tr("Delete"), layersPanel);
  renameLayerButton_ = new QPushButton(tr("Rename"), layersPanel);
  lockLayerButton_ = new QPushButton(tr("Lock"), layersPanel);
  layerUpButton_ = new QPushButton(tr("Up"), layersPanel);
  layerDownButton_ = new QPushButton(tr("Down"), layersPanel);
  addLayerButton_->setObjectName(QStringLiteral("addLayerButton"));
  duplicateLayerButton_->setObjectName(QStringLiteral("duplicateLayerButton"));
  deleteLayerButton_->setObjectName(QStringLiteral("deleteLayerButton"));
  renameLayerButton_->setObjectName(QStringLiteral("renameLayerButton"));
  lockLayerButton_->setObjectName(QStringLiteral("toggleLayerLockButton"));
  layerUpButton_->setObjectName(QStringLiteral("layerUpButton"));
  layerDownButton_->setObjectName(QStringLiteral("layerDownButton"));
  buttonRow->addWidget(addLayerButton_);
  buttonRow->addWidget(duplicateLayerButton_);
  buttonRow->addWidget(deleteLayerButton_);
  buttonRow->addWidget(renameLayerButton_);
  buttonRow->addWidget(lockLayerButton_);
  buttonRow->addWidget(layerUpButton_);
  buttonRow->addWidget(layerDownButton_);
  layersLayout->addLayout(buttonRow);

  auto* opacityRow = new QHBoxLayout();
  opacityRow->addWidget(new QLabel(tr("Opacity"), layersPanel));
  opacitySlider_ = new QSlider(Qt::Horizontal, layersPanel);
  opacitySlider_->setRange(0, 100);
  opacitySlider_->setValue(100);
  opacityRow->addWidget(opacitySlider_, 1);
  layersLayout->addLayout(opacityRow);

  layersDock->setWidget(layersPanel);
  addDockWidget(Qt::RightDockWidgetArea, layersDock);

  connect(layersList_, &QListWidget::currentItemChanged, this, &MainWindow::layerSelectionChanged);
  connect(layersList_, &QListWidget::itemChanged, this, &MainWindow::layerItemChanged);
  connect(addLayerButton_, &QPushButton::clicked, this, &MainWindow::addRasterLayer);
  connect(duplicateLayerButton_, &QPushButton::clicked, this, &MainWindow::duplicateSelectedLayer);
  connect(deleteLayerButton_, &QPushButton::clicked, this, &MainWindow::deleteSelectedLayer);
  connect(renameLayerButton_, &QPushButton::clicked, this, &MainWindow::renameSelectedLayer);
  connect(lockLayerButton_, &QPushButton::clicked, this, &MainWindow::toggleSelectedLayerLock);
  connect(layerUpButton_, &QPushButton::clicked, this, &MainWindow::moveSelectedLayerUp);
  connect(layerDownButton_, &QPushButton::clicked, this, &MainWindow::moveSelectedLayerDown);
  connect(opacitySlider_, &QSlider::valueChanged, this, &MainWindow::selectedLayerOpacityChanged);

  createToolOptionsPanel();

  auto* propertiesDock = new QDockWidget(tr("Properties"), this);
  propertiesDock->setObjectName(QStringLiteral("propertiesDock"));
  propertiesLabel_ = new QLabel(propertiesDock);
  propertiesLabel_->setObjectName(QStringLiteral("propertiesLabel"));
  propertiesLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  propertiesLabel_->setMargin(12);
  propertiesLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  propertiesDock->setWidget(propertiesLabel_);
  addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

  auto* historyDock = new QDockWidget(tr("History"), this);
  historyDock->setObjectName(QStringLiteral("historyDock"));
  historyList_ = new QListWidget(historyDock);
  historyList_->setObjectName(QStringLiteral("historyList"));
  historyDock->setWidget(historyList_);
  addDockWidget(Qt::RightDockWidgetArea, historyDock);
  tabifyDockWidget(propertiesDock, historyDock);
}

void MainWindow::createToolOptionsPanel()
{
  auto* toolOptionsDock = new QDockWidget(tr("Tool Options"), this);
  toolOptionsDock->setObjectName(QStringLiteral("toolOptionsDock"));

  auto* optionsPanel = new QWidget(toolOptionsDock);
  auto* optionsLayout = new QVBoxLayout(optionsPanel);
  optionsLayout->setContentsMargins(8, 8, 8, 8);
  optionsLayout->setSpacing(8);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignLeft);

  const auto makeSpinBox = [optionsPanel](int minimum, int maximum, int value) {
    auto* input = new QSpinBox(optionsPanel);
    input->setRange(minimum, maximum);
    input->setValue(value);
    return input;
  };

  brushColorButton_ = new QPushButton(optionsPanel);
  brushColorButton_->setObjectName(QStringLiteral("brushColorButton"));
  brushSizeInput_ = makeSpinBox(1, 256, brushSize_);
  brushSizeInput_->setObjectName(QStringLiteral("brushSizeInput"));
  brushOpacityInput_ = makeSpinBox(1, 100, brushOpacity_);
  brushOpacityInput_->setObjectName(QStringLiteral("brushOpacityInput"));
  eraserSizeInput_ = makeSpinBox(1, 256, eraserSize_);
  eraserSizeInput_->setObjectName(QStringLiteral("eraserSizeInput"));
  fillColorButton_ = new QPushButton(optionsPanel);
  fillColorButton_->setObjectName(QStringLiteral("fillColorButton"));
  fillToleranceInput_ = makeSpinBox(0, 255, fillTolerance_);
  fillToleranceInput_->setObjectName(QStringLiteral("fillToleranceInput"));
  textContentInput_ = new QLineEdit(tr("Text"), optionsPanel);
  textContentInput_->setObjectName(QStringLiteral("textContentInput"));
  textColorButton_ = new QPushButton(optionsPanel);
  textColorButton_->setObjectName(QStringLiteral("textColorButton"));
  textSizeInput_ = makeSpinBox(6, 240, textPointSize_);
  textSizeInput_->setObjectName(QStringLiteral("textSizeInput"));
  shapeTypeInput_ = new QComboBox(optionsPanel);
  shapeTypeInput_->setObjectName(QStringLiteral("shapeTypeInput"));
  shapeTypeInput_->addItem(tr("Rectangle"), QStringLiteral("rectangle"));
  shapeTypeInput_->addItem(tr("Ellipse"), QStringLiteral("ellipse"));
  shapeWidthInput_ = makeSpinBox(24, 4096, shapeWidth_);
  shapeWidthInput_->setObjectName(QStringLiteral("shapeWidthInput"));
  shapeHeightInput_ = makeSpinBox(24, 4096, shapeHeight_);
  shapeHeightInput_->setObjectName(QStringLiteral("shapeHeightInput"));
  shapeFillColorButton_ = new QPushButton(optionsPanel);
  shapeFillColorButton_->setObjectName(QStringLiteral("shapeFillColorButton"));
  shapeStrokeColorButton_ = new QPushButton(optionsPanel);
  shapeStrokeColorButton_->setObjectName(QStringLiteral("shapeStrokeColorButton"));
  shapeStrokeWidthInput_ = makeSpinBox(0, 128, shapeStrokeWidth_);
  shapeStrokeWidthInput_->setObjectName(QStringLiteral("shapeStrokeWidthInput"));

  updateColorButton(brushColorButton_, brushColor_);
  updateColorButton(fillColorButton_, fillColor_);
  updateColorButton(textColorButton_, textColor_);
  updateColorButton(shapeFillColorButton_, shapeFillColor_);
  updateColorButton(shapeStrokeColorButton_, shapeStrokeColor_);

  connect(brushColorButton_, &QPushButton::clicked, this, [this]() {
    chooseToolColor(tr("Brush Color"), brushColor_, brushColorButton_);
  });
  connect(fillColorButton_, &QPushButton::clicked, this, [this]() {
    chooseToolColor(tr("Fill Color"), fillColor_, fillColorButton_);
  });
  connect(textColorButton_, &QPushButton::clicked, this, [this]() {
    chooseToolColor(tr("Text Color"), textColor_, textColorButton_);
  });
  connect(shapeFillColorButton_, &QPushButton::clicked, this, [this]() {
    chooseToolColor(tr("Shape Fill Color"), shapeFillColor_, shapeFillColorButton_);
  });
  connect(shapeStrokeColorButton_, &QPushButton::clicked, this, [this]() {
    chooseToolColor(tr("Shape Stroke Color"), shapeStrokeColor_, shapeStrokeColorButton_);
  });
  connect(brushSizeInput_, &QSpinBox::valueChanged, this, [this](int value) { brushSize_ = value; });
  connect(brushOpacityInput_, &QSpinBox::valueChanged, this, [this](int value) { brushOpacity_ = value; });
  connect(eraserSizeInput_, &QSpinBox::valueChanged, this, [this](int value) { eraserSize_ = value; });
  connect(fillToleranceInput_, &QSpinBox::valueChanged, this, [this](int value) { fillTolerance_ = value; });
  connect(textSizeInput_, &QSpinBox::valueChanged, this, [this](int value) { textPointSize_ = value; });
  connect(shapeWidthInput_, &QSpinBox::valueChanged, this, [this](int value) { shapeWidth_ = value; });
  connect(shapeHeightInput_, &QSpinBox::valueChanged, this, [this](int value) { shapeHeight_ = value; });
  connect(shapeStrokeWidthInput_, &QSpinBox::valueChanged, this, [this](int value) { shapeStrokeWidth_ = value; });

  form->addRow(tr("Brush Color"), brushColorButton_);
  form->addRow(tr("Brush Size"), brushSizeInput_);
  form->addRow(tr("Brush Opacity"), brushOpacityInput_);
  form->addRow(tr("Eraser Size"), eraserSizeInput_);
  form->addRow(tr("Fill Color"), fillColorButton_);
  form->addRow(tr("Fill Tolerance"), fillToleranceInput_);
  form->addRow(tr("Text"), textContentInput_);
  form->addRow(tr("Text Color"), textColorButton_);
  form->addRow(tr("Text Size"), textSizeInput_);
  form->addRow(tr("Shape"), shapeTypeInput_);
  form->addRow(tr("Shape Width"), shapeWidthInput_);
  form->addRow(tr("Shape Height"), shapeHeightInput_);
  form->addRow(tr("Shape Fill"), shapeFillColorButton_);
  form->addRow(tr("Shape Stroke"), shapeStrokeColorButton_);
  form->addRow(tr("Stroke Width"), shapeStrokeWidthInput_);
  optionsLayout->addLayout(form);

  updateTextLayerButton_ = new QPushButton(tr("Update Selected Text"), optionsPanel);
  updateTextLayerButton_->setObjectName(QStringLiteral("updateTextLayerButton"));
  connect(updateTextLayerButton_, &QPushButton::clicked, this, &MainWindow::updateSelectedTextLayer);
  optionsLayout->addWidget(updateTextLayerButton_);

  updateShapeLayerButton_ = new QPushButton(tr("Update Selected Shape"), optionsPanel);
  updateShapeLayerButton_->setObjectName(QStringLiteral("updateShapeLayerButton"));
  connect(updateShapeLayerButton_, &QPushButton::clicked, this, &MainWindow::updateSelectedShapeLayer);
  optionsLayout->addWidget(updateShapeLayerButton_);

  cropSelectedLayerButton_ = new QPushButton(tr("Crop To Selected Layer"), optionsPanel);
  cropSelectedLayerButton_->setObjectName(QStringLiteral("cropSelectedLayerButton"));
  connect(cropSelectedLayerButton_, &QPushButton::clicked, this, &MainWindow::cropToSelectedLayer);
  optionsLayout->addWidget(cropSelectedLayerButton_);
  optionsLayout->addStretch(1);

  toolOptionsDock->setWidget(optionsPanel);
  addDockWidget(Qt::RightDockWidgetArea, toolOptionsDock);
}

void MainWindow::createInitialDocument()
{
  auto document = qco::core::Document::create(
    "Untitled",
    {defaultDocumentWidth, defaultDocumentHeight});
  setDocument(std::move(document), {});
  undoStack_.clear();
  markCurrentUndoStateClean();
}

void MainWindow::setDocument(qco::core::Document document, QVector<CanvasView::LayerImage> layers)
{
  document_ = std::make_unique<qco::core::Document>(std::move(document));
  layers_ = std::move(layers);
  selectedLayerId_ = document_->layers().empty() ? 0 : document_->layers().back().id;
  pendingMoveBeforeState_.reset();

  canvas_->setDocumentSize(QSize(document_->canvasSize().width, document_->canvasSize().height));
  syncCanvasLayers();

  updateLayerPanel();
  updateToolOptionsFromSelectedLayer();
  updatePropertiesPanel();
  updateHistoryPanel();
  updateActions();
  updateWindowTitle();
}

void MainWindow::applyState(const DocumentState& state)
{
  document_ = std::make_unique<qco::core::Document>(state.document);
  layers_ = state.layers;
  selectedLayerId_ = state.selectedLayerId;

  canvas_->setDocumentSize(toQtSize(document_->canvasSize()));
  syncCanvasLayers();
  updateLayerPanel();
  updateToolOptionsFromSelectedLayer();
  updatePropertiesPanel();
  updateHistoryPanel();
  updateActions();
  updateWindowTitle();
}

void MainWindow::pushStateCommand(QString label, DocumentState before, DocumentState after)
{
  if (undoStack_.index() < undoStack_.size()
      && cleanUndoIndex_.has_value()
      && *cleanUndoIndex_ > undoStack_.index()) {
    cleanUndoIndex_.reset();
  }

  undoStack_.push(std::make_unique<qco::core::LambdaCommand>(
    label.toStdString(),
    [this, before = std::move(before)]() {
      applyState(before);
      updateDirtyFromUndoStack();
    },
    [this, after = std::move(after)]() {
      applyState(after);
      updateDirtyFromUndoStack();
    }));

  updateDirtyFromUndoStack();
  updateHistoryPanel();
  updateActions();
}

void MainWindow::updateLayerPanel()
{
  refreshingLayerPanel_ = true;
  layersList_->clear();

  if (!document_) {
    refreshingLayerPanel_ = false;
    return;
  }

  for (auto it = document_->layers().rbegin(); it != document_->layers().rend(); ++it) {
    auto* item = new QListWidgetItem(
      QStringLiteral("%1  [%2]%3")
        .arg(QString::fromStdString(it->name))
        .arg(QString::fromUtf8(qco::core::toString(it->type).data(),
                               static_cast<qsizetype>(qco::core::toString(it->type).size())))
        .arg(it->locked ? tr(" locked") : QString()));
    item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(it->id));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(it->visible ? Qt::Checked : Qt::Unchecked);
    layersList_->addItem(item);
    if (it->id == selectedLayerId_) {
      layersList_->setCurrentItem(item);
    }
  }

  if (layersList_->count() == 0) {
    layersList_->addItem(tr("No layers yet"));
  }

  if (opacitySlider_ != nullptr) {
    const QSignalBlocker blocker(opacitySlider_);
    const auto* layer = selectedLayer();
    opacitySlider_->setEnabled(layer != nullptr);
    opacitySlider_->setValue(layer == nullptr ? 100 : qRound(layer->opacity * 100.0));
  }

  refreshingLayerPanel_ = false;
}

void MainWindow::updatePropertiesPanel()
{
  if (!document_) {
    propertiesLabel_->clear();
    return;
  }

  const auto* layer = selectedLayer();
  const auto selectedText = layer == nullptr
                              ? tr("None")
                              : tr("%1\n%2% opacity\nPosition %3, %4\n%5")
                                  .arg(QString::fromStdString(layer->name))
                                  .arg(qRound(layer->opacity * 100.0))
                                  .arg(layer->position.x)
                                  .arg(layer->position.y)
                                  .arg(layer->locked ? tr("Locked") : tr("Unlocked"));

  propertiesLabel_->setText(tr("Document\n%1%2\n\nCanvas\n%3 x %4 px\n\nLayers\n%5\n\nSelected Layer\n%6\n\nLog\n%7")
                              .arg(documentTitle())
                              .arg(dirty_ ? tr(" *") : QString())
                              .arg(document_->canvasSize().width)
                              .arg(document_->canvasSize().height)
                              .arg(document_->layers().size())
                              .arg(selectedText)
                              .arg(qco::app::logFilePath()));
}

void MainWindow::updateHistoryPanel()
{
  historyList_->clear();

  if (undoStack_.size() == 0) {
    historyList_->addItem(tr("No edits yet"));
    return;
  }

  historyList_->addItem(tr("Undo: %1").arg(QString::fromStdString(undoStack_.undoText())));
  historyList_->addItem(tr("Redo: %1").arg(QString::fromStdString(undoStack_.redoText())));
}

void MainWindow::updateActions()
{
  const auto hasDocument = document_ != nullptr;
  const auto* selected = selectedLayer();
  const auto hasSelectedLayer = selectedLayerId_ != 0 && selected != nullptr;
  const auto selectedLayerLocked = selected != nullptr && selected->locked;
  const auto selectedIndex = document_ == nullptr ? std::optional<std::size_t>() : document_->layerIndex(selectedLayerId_);
  saveAction_->setEnabled(hasDocument);
  exportAction_->setEnabled(hasDocument);
  addRasterLayerAction_->setEnabled(hasDocument);
  duplicateLayerAction_->setEnabled(hasSelectedLayer);
  deleteLayerAction_->setEnabled(hasSelectedLayer && !selectedLayerLocked);
  renameLayerAction_->setEnabled(hasSelectedLayer);
  toggleLayerLockAction_->setEnabled(hasSelectedLayer);
  toggleLayerLockAction_->setText(selectedLayerLocked ? tr("Unlock Layer") : tr("Lock Layer"));
  layerUpAction_->setEnabled(hasSelectedLayer && selectedIndex.has_value() && *selectedIndex + 1 < document_->layers().size());
  layerDownAction_->setEnabled(hasSelectedLayer && selectedIndex.has_value() && *selectedIndex > 0);
  fitAction_->setEnabled(hasDocument);
  actualSizeAction_->setEnabled(hasDocument);
  undoAction_->setEnabled(undoStack_.canUndo());
  redoAction_->setEnabled(undoStack_.canRedo());

  if (duplicateLayerButton_ != nullptr) {
    duplicateLayerButton_->setEnabled(hasSelectedLayer);
    deleteLayerButton_->setEnabled(deleteLayerAction_->isEnabled());
    renameLayerButton_->setEnabled(hasSelectedLayer);
    lockLayerButton_->setEnabled(hasSelectedLayer);
    lockLayerButton_->setText(selectedLayerLocked ? tr("Unlock") : tr("Lock"));
    layerUpButton_->setEnabled(layerUpAction_->isEnabled());
    layerDownButton_->setEnabled(layerDownAction_->isEnabled());
  }
  if (cropSelectedLayerButton_ != nullptr) {
    cropSelectedLayerButton_->setEnabled(hasSelectedLayer);
  }
  if (updateTextLayerButton_ != nullptr) {
    updateTextLayerButton_->setEnabled(
      hasSelectedLayer && selected != nullptr && selected->type == qco::core::LayerType::Text && !selectedLayerLocked);
  }
  if (updateShapeLayerButton_ != nullptr) {
    updateShapeLayerButton_->setEnabled(
      hasSelectedLayer && selected != nullptr && selected->type == qco::core::LayerType::Shape && !selectedLayerLocked);
  }
}

void MainWindow::updateToolOptionsFromSelectedLayer()
{
  const auto* layer = selectedLayer();
  if (layer == nullptr) {
    return;
  }

  if (layer->type == qco::core::LayerType::Text && layer->textPayload.has_value()) {
    const auto& payload = *layer->textPayload;
    if (textContentInput_ != nullptr) {
      const QSignalBlocker blocker(textContentInput_);
      textContentInput_->setText(QString::fromStdString(payload.text));
    }
    if (textSizeInput_ != nullptr) {
      textPointSize_ = std::clamp(payload.pointSize, textSizeInput_->minimum(), textSizeInput_->maximum());
      const QSignalBlocker blocker(textSizeInput_);
      textSizeInput_->setValue(textPointSize_);
    }
    const QColor payloadColor(QString::fromStdString(payload.color));
    if (payloadColor.isValid()) {
      textColor_ = payloadColor;
      updateColorButton(textColorButton_, textColor_);
    }
  } else if (layer->type == qco::core::LayerType::Shape && layer->shapePayload.has_value()) {
    const auto& payload = *layer->shapePayload;
    const auto payloadShape = QString::fromStdString(payload.shape);
    if (shapeTypeInput_ != nullptr) {
      const auto index = shapeTypeInput_->findData(payloadShape);
      if (index >= 0) {
        const QSignalBlocker blocker(shapeTypeInput_);
        shapeTypeInput_->setCurrentIndex(index);
      }
    }
    if (shapeWidthInput_ != nullptr) {
      shapeWidth_ = std::clamp(layer->size.width, shapeWidthInput_->minimum(), shapeWidthInput_->maximum());
      const QSignalBlocker blocker(shapeWidthInput_);
      shapeWidthInput_->setValue(shapeWidth_);
    }
    if (shapeHeightInput_ != nullptr) {
      shapeHeight_ = std::clamp(layer->size.height, shapeHeightInput_->minimum(), shapeHeightInput_->maximum());
      const QSignalBlocker blocker(shapeHeightInput_);
      shapeHeightInput_->setValue(shapeHeight_);
    }
    if (shapeStrokeWidthInput_ != nullptr) {
      shapeStrokeWidth_ =
        std::clamp(payload.strokeWidth, shapeStrokeWidthInput_->minimum(), shapeStrokeWidthInput_->maximum());
      const QSignalBlocker blocker(shapeStrokeWidthInput_);
      shapeStrokeWidthInput_->setValue(shapeStrokeWidth_);
    }
    const QColor fill(QString::fromStdString(payload.fillColor));
    if (fill.isValid()) {
      shapeFillColor_ = fill;
      updateColorButton(shapeFillColorButton_, shapeFillColor_);
    }
    const QColor stroke(QString::fromStdString(payload.strokeColor));
    if (stroke.isValid()) {
      shapeStrokeColor_ = stroke;
      updateColorButton(shapeStrokeColorButton_, shapeStrokeColor_);
    }
  }
}

void MainWindow::updateWindowTitle()
{
  setWindowTitle(tr("%1%2 - QCO Studio").arg(documentTitle()).arg(dirty_ ? QStringLiteral("*") : QString()));
}

void MainWindow::setDirty(bool dirty)
{
  dirty_ = dirty;
  updatePropertiesPanel();
  updateWindowTitle();
}

void MainWindow::markCurrentUndoStateClean()
{
  cleanUndoIndex_ = undoStack_.index();
  setDirty(false);
}

void MainWindow::updateDirtyFromUndoStack()
{
  setDirty(!cleanUndoIndex_.has_value() || undoStack_.index() != *cleanUndoIndex_);
}

void MainWindow::setSelectedLayerId(std::uint64_t id)
{
  if (id != 0 && (document_ == nullptr || document_->findLayer(id) == nullptr)) {
    id = 0;
  }

  selectedLayerId_ = id;
  canvas_->setSelectedLayerId(id);
  updateLayerPanel();
  updateToolOptionsFromSelectedLayer();
  updatePropertiesPanel();
  updateActions();
}

void MainWindow::syncCanvasLayers()
{
  canvas_->setSelectedLayerId(selectedLayerId_);
  canvas_->setLayers(layers_);
}

void MainWindow::applyStrokeToSelectedLayer(CanvasView::Tool tool, QPoint fromDocumentPoint, QPoint toDocumentPoint)
{
  const auto* layer = selectedLayer();
  if (layer != nullptr && layer->locked) {
    return;
  }

  auto* imageLayer = selectedLayerImage();
  if (imageLayer == nullptr || imageLayer->image.isNull()) {
    return;
  }

  const QPoint from = fromDocumentPoint - imageLayer->position;
  const QPoint to = toDocumentPoint - imageLayer->position;
  QRect imageRect(QPoint(0, 0), imageLayer->image.size());
  const int toolSize = tool == CanvasView::Tool::Eraser ? eraserSize_ : brushSize_;
  const int boundsPadding = std::max(32, toolSize);
  if (!imageRect.adjusted(-boundsPadding, -boundsPadding, boundsPadding, boundsPadding).contains(from)
      && !imageRect.adjusted(-boundsPadding, -boundsPadding, boundsPadding, boundsPadding).contains(to)) {
    return;
  }

  QPainter painter(&imageLayer->image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  if (tool == CanvasView::Tool::Eraser) {
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
  }

  QColor strokeColor = brushColor_;
  strokeColor.setAlpha(qRound(255.0 * static_cast<qreal>(brushOpacity_) / 100.0));
  QPen pen(tool == CanvasView::Tool::Eraser ? QColor(0, 0, 0, 255) : strokeColor);
  pen.setWidth(toolSize);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);
  painter.drawLine(from, to);
  painter.end();

  syncCanvasLayers();
  updatePropertiesPanel();
}

void MainWindow::fillSelectedLayerAt(QPoint documentPoint)
{
  const auto* layer = selectedLayer();
  if (layer != nullptr && layer->locked) {
    statusBar()->showMessage(tr("Selected layer is locked"), 3000);
    return;
  }

  auto* imageLayer = selectedLayerImage();
  if (imageLayer == nullptr || imageLayer->image.isNull()) {
    statusBar()->showMessage(tr("Select or add a raster layer first"), 3000);
    return;
  }

  const QPoint localPoint = documentPoint - imageLayer->position;
  QRect imageRect(QPoint(0, 0), imageLayer->image.size());
  if (!imageRect.contains(localPoint)) {
    statusBar()->showMessage(tr("Click inside the selected layer to fill"), 3000);
    return;
  }

  auto before = captureState();
  imageLayer = selectedLayerImage();
  if (imageLayer == nullptr) {
    return;
  }

  QImage& image = imageLayer->image;
  const QColor targetColor = image.pixelColor(localPoint);
  if (colorsWithinTolerance(targetColor, fillColor_, fillTolerance_)) {
    return;
  }

  QVector<QPoint> stack;
  stack.reserve(image.width() * image.height() / 8);
  stack.push_back(localPoint);

  while (!stack.isEmpty()) {
    const QPoint point = stack.takeLast();
    if (!imageRect.contains(point) || !colorsWithinTolerance(image.pixelColor(point), targetColor, fillTolerance_)) {
      continue;
    }

    image.setPixelColor(point, fillColor_);
    stack.push_back(QPoint(point.x() + 1, point.y()));
    stack.push_back(QPoint(point.x() - 1, point.y()));
    stack.push_back(QPoint(point.x(), point.y() + 1));
    stack.push_back(QPoint(point.x(), point.y() - 1));
  }

  pushStateCommand(tr("Fill Layer Region"), before, captureState());
}

void MainWindow::addTextLayerAt(QPoint documentPoint)
{
  if (!document_) {
    return;
  }

  const auto text = textContentInput_ == nullptr ? QStringLiteral("Text") : textContentInput_->text().trimmed();
  if (text.isEmpty()) {
    statusBar()->showMessage(tr("Enter text in Tool Options first"), 3000);
    return;
  }

  auto before = captureState();
  const auto image = renderTextLayerImage(text, textColor_, textPointSize_);

  const auto layerId = document_->addLayer(
    text.toStdString(),
    qco::core::LayerType::Text,
    toCoreSize(image.size()),
    toCorePoint(documentPoint));
  if (auto* layerModel = document_->findLayer(layerId)) {
    qco::core::TextLayerPayload payload;
    payload.text = text.toStdString();
    payload.color = textColor_.name(QColor::HexArgb).toStdString();
    payload.pointSize = textPointSize_;
    layerModel->textPayload = std::move(payload);
  }

  CanvasView::LayerImage layer;
  layer.id = layerId;
  layer.name = text;
  layer.image = image;
  layer.position = documentPoint;
  layers_.push_back(std::move(layer));
  selectedLayerId_ = layerId;

  pushStateCommand(tr("Add Text Layer"), std::move(before), captureState());
}

void MainWindow::addShapeLayerAt(QPoint documentPoint)
{
  if (!document_) {
    return;
  }

  const auto shapeKey = shapeTypeInput_ == nullptr ? QStringLiteral("rectangle") : shapeTypeInput_->currentData().toString();
  const auto shape = shapeTypeInput_ == nullptr ? tr("Rectangle") : shapeTypeInput_->currentText();

  auto before = captureState();
  const auto image = renderShapeLayerImage(
    shapeKey,
    QSize(shapeWidth_, shapeHeight_),
    shapeFillColor_,
    shapeStrokeColor_,
    shapeStrokeWidth_);

  const auto layerId = document_->addLayer(
    shape.toStdString(),
    qco::core::LayerType::Shape,
    toCoreSize(image.size()),
    toCorePoint(documentPoint));
  if (auto* layerModel = document_->findLayer(layerId)) {
    qco::core::ShapeLayerPayload payload;
    payload.shape = shapeKey.toStdString();
    payload.fillColor = shapeFillColor_.name(QColor::HexArgb).toStdString();
    payload.strokeColor = shapeStrokeColor_.name(QColor::HexArgb).toStdString();
    payload.strokeWidth = shapeStrokeWidth_;
    layerModel->shapePayload = std::move(payload);
  }

  CanvasView::LayerImage layer;
  layer.id = layerId;
  layer.name = shape;
  layer.image = image;
  layer.position = documentPoint;
  layers_.push_back(std::move(layer));
  selectedLayerId_ = layerId;

  pushStateCommand(tr("Add Shape Layer"), std::move(before), captureState());
}

void MainWindow::cropToSelectedLayer()
{
  if (!document_) {
    return;
  }

  const QRect cropRect = selectedLayerDocumentRect().intersected(QRect(QPoint(0, 0), toQtSize(document_->canvasSize())));
  if (!cropRect.isValid() || cropRect.isEmpty()) {
    statusBar()->showMessage(tr("Select a visible layer to crop to"), 3000);
    return;
  }

  cropDocumentToRect(cropRect);
}

QImage MainWindow::renderTextLayerImage(const QString& text, const QColor& color, int pointSize) const
{
  QFont font;
  font.setPointSize(std::clamp(pointSize, 1, 240));
  font.setBold(false);

  const QFontMetrics metrics(font);
  const QRect textBounds = metrics.boundingRect(text).adjusted(-8, -8, 8, 8);
  const QSize imageSize(std::max(1, textBounds.width()), std::max(1, textBounds.height()));

  QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setFont(font);
  painter.setPen(color);
  painter.drawText(QPoint(8, 8 + metrics.ascent()), text);
  painter.end();
  return image;
}

QImage MainWindow::renderShapeLayerImage(
  const QString& shapeKey,
  QSize size,
  const QColor& fillColor,
  const QColor& strokeColor,
  int strokeWidth) const
{
  size = size.expandedTo(QSize(1, 1));

  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  if (strokeWidth == 0) {
    painter.setPen(Qt::NoPen);
  } else {
    painter.setPen(QPen(strokeColor, strokeWidth));
  }
  painter.setBrush(fillColor);

  const auto halfShortestSide = std::max<qreal>(1.0, std::min(image.width(), image.height()) / 2.0 - 1.0);
  const auto margin = std::min<qreal>(std::max<qreal>(4.0, strokeWidth / 2.0 + 4.0), halfShortestSide);
  const QRectF bounds(margin, margin, image.width() - (margin * 2.0), image.height() - (margin * 2.0));
  if (shapeKey == QStringLiteral("ellipse")) {
    painter.drawEllipse(bounds);
  } else {
    painter.drawRect(bounds);
  }
  painter.end();
  return image;
}

void MainWindow::updateColorButton(QPushButton* button, const QColor& color)
{
  if (button == nullptr) {
    return;
  }

  const auto label = color.alpha() == 255
                       ? color.name(QColor::HexRgb).toUpper()
                       : color.name(QColor::HexArgb).toUpper();
  const int luminance = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
  const auto textColor = luminance < 128 ? QStringLiteral("#FFFFFF") : QStringLiteral("#111111");
  button->setText(label);
  button->setStyleSheet(
    QStringLiteral("background-color: %1; color: %2;").arg(color.name(QColor::HexArgb), textColor));
}

void MainWindow::chooseToolColor(const QString& title, QColor& color, QPushButton* button)
{
  const QColor selected = QColorDialog::getColor(color, this, title, QColorDialog::ShowAlphaChannel);
  if (!selected.isValid()) {
    return;
  }

  color = selected;
  updateColorButton(button, color);
}

void MainWindow::rememberDirectory(const QString& filePath)
{
  settings_.setValue(QStringLiteral("files/lastDirectory"), QFileInfo(filePath).absolutePath());
}

bool MainWindow::maybeSaveChanges()
{
  if (!dirty_) {
    return true;
  }

  const auto result = QMessageBox::warning(
    this,
    tr("Unsaved Changes"),
    tr("Save changes to %1 before continuing?").arg(documentTitle()),
    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
    QMessageBox::Save);

  if (result == QMessageBox::Cancel) {
    return false;
  }
  if (result == QMessageBox::Discard) {
    return true;
  }

  if (currentProjectPath_.isEmpty()) {
    const auto path = QFileDialog::getSaveFileName(
      this,
      tr("Save Project"),
      lastDirectory() + QLatin1Char('/') + documentTitle() + QStringLiteral(".qco"),
      projectFileFilter());
    if (path.isEmpty()) {
      return false;
    }
    return saveProjectToPath(path);
  }

  return saveProjectToPath(currentProjectPath_);
}

MainWindow::DocumentState MainWindow::captureState() const
{
  return {*document_, layers_, selectedLayerId_};
}

qco::core::Layer* MainWindow::selectedLayer() noexcept
{
  return document_ == nullptr ? nullptr : document_->findLayer(selectedLayerId_);
}

const qco::core::Layer* MainWindow::selectedLayer() const noexcept
{
  return document_ == nullptr ? nullptr : document_->findLayer(selectedLayerId_);
}

CanvasView::LayerImage* MainWindow::selectedLayerImage() noexcept
{
  const auto it = std::find_if(layers_.begin(), layers_.end(), [id = selectedLayerId_](const CanvasView::LayerImage& layer) {
    return layer.id == id;
  });
  return it == layers_.end() ? nullptr : &(*it);
}

const CanvasView::LayerImage* MainWindow::selectedLayerImage() const noexcept
{
  const auto it = std::find_if(layers_.begin(), layers_.end(), [id = selectedLayerId_](const CanvasView::LayerImage& layer) {
    return layer.id == id;
  });
  return it == layers_.end() ? nullptr : &(*it);
}

QRect MainWindow::selectedLayerDocumentRect() const
{
  const auto* imageLayer = selectedLayerImage();
  if (imageLayer == nullptr || imageLayer->image.isNull()) {
    return {};
  }

  return QRect(imageLayer->position, imageLayer->image.size());
}

QString MainWindow::lastDirectory() const
{
  return settings_.value(QStringLiteral("files/lastDirectory"), QDir::homePath()).toString();
}

QString MainWindow::documentTitle() const
{
  if (!document_) {
    return tr("Untitled");
  }
  return QString::fromStdString(document_->title());
}

QString MainWindow::withDefaultSuffix(QString filePath, const QString& suffix) const
{
  QFileInfo info(filePath);
  if (info.suffix().isEmpty()) {
    filePath += QLatin1Char('.') + suffix;
  }
  return filePath;
}

QImage MainWindow::renderDocumentComposite(const QColor& background) const
{
  if (!document_) {
    return {};
  }

  QVector<qco::render::LayerImage> layerImages;
  layerImages.reserve(layers_.size());
  for (const auto& layer : layers_) {
    layerImages.push_back({layer.id, layer.image});
  }

  const qco::render::QtDocumentRenderer renderer;
  return renderer.render(*document_, layerImages, background);
}

QVector<ProjectRasterLayer> MainWindow::rasterLayerPayloads() const
{
  QVector<ProjectRasterLayer> payloads;
  payloads.reserve(layers_.size());

  for (const auto& layer : layers_) {
    ProjectRasterLayer payload;
    payload.id = layer.id;
    payload.name = layer.name;
    payload.image = layer.image;
    payload.position = layer.position;
    payload.visible = layer.visible;
    payload.opacity = layer.opacity;
    payloads.push_back(std::move(payload));
  }

  return payloads;
}

}  // namespace qco::ui
