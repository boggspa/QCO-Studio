#include "ui/MainWindow.h"

#include "app/Logging.h"
#include "ui/ProjectArchive.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>

namespace qmx::ui {
namespace {

constexpr int defaultDocumentWidth = 1920;
constexpr int defaultDocumentHeight = 1080;

[[nodiscard]] QString readableFileFilters()
{
  QStringList patterns;
  const auto formats = QImageReader::supportedImageFormats();
  for (const auto& format : formats) {
    patterns.push_back(QStringLiteral("*.%1").arg(QString::fromLatin1(format).toLower()));
  }
  patterns.removeDuplicates();
  patterns.sort();
  return QObject::tr("Images (%1);;All files (*)").arg(patterns.join(QLatin1Char(' ')));
}

[[nodiscard]] qmx::core::Size toCoreSize(QSize size)
{
  return {size.width(), size.height()};
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent),
    settings_(QStringLiteral("QimageMax"), QStringLiteral("Qimage Max"))
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

  restoreGeometry(settings_.value(QStringLiteral("window/geometry")).toByteArray());
  restoreState(settings_.value(QStringLiteral("window/state")).toByteArray());

  createInitialDocument();
}

MainWindow::~MainWindow()
{
  settings_.setValue(QStringLiteral("window/geometry"), saveGeometry());
  settings_.setValue(QStringLiteral("window/state"), saveState());
}

void MainWindow::newDocument()
{
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

  auto document = qmx::core::Document::create(
    "Untitled",
    {widthInput->value(), heightInput->value()});

  QVector<CanvasView::LayerImage> layers;
  setDocument(std::move(document), std::move(layers));
  currentProjectPath_.clear();
  undoStack_.clear();
  updateHistoryPanel();
  statusBar()->showMessage(tr("Created new document"), 3000);
}

void MainWindow::openImage()
{
  const auto path = QFileDialog::getOpenFileName(
    this,
    tr("Open Image"),
    lastDirectory(),
    readableFileFilters());

  if (path.isEmpty()) {
    return;
  }

  QImage image(path);
  if (image.isNull()) {
    QMessageBox::warning(this, tr("Open Image"), tr("The selected file could not be opened as an image."));
    return;
  }

  rememberDirectory(path);

  const QFileInfo info(path);
  auto document = qmx::core::Document::create(
    info.completeBaseName().toStdString(),
    toCoreSize(image.size()));

  const auto layerId = document.addLayer(
    info.fileName().toStdString(),
    qmx::core::LayerType::Raster,
    toCoreSize(image.size()));

  CanvasView::LayerImage layer;
  layer.id = layerId;
  layer.name = info.fileName();
  layer.image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  layer.position = QPoint(0, 0);

  QVector<CanvasView::LayerImage> layers;
  layers.push_back(std::move(layer));

  setDocument(std::move(document), std::move(layers));
  currentProjectPath_.clear();
  undoStack_.clear();
  updateHistoryPanel();
  statusBar()->showMessage(tr("Opened %1").arg(info.fileName()), 3000);
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
      lastDirectory() + QLatin1Char('/') + documentTitle() + QStringLiteral(".qmxdoc"),
      tr("Qimage Max Document (*.qmxdoc)"));
  }

  if (path.isEmpty()) {
    return;
  }

  path = withDefaultSuffix(path, QStringLiteral("qmxdoc"));

  QString error;
  if (!ProjectArchive::save(path, *document_, rasterLayerPayloads(), &error)) {
    QMessageBox::critical(this, tr("Save Project"), tr("The project could not be saved:\n%1").arg(error));
    return;
  }

  currentProjectPath_ = path;
  rememberDirectory(path);
  updateWindowTitle();
  statusBar()->showMessage(tr("Saved project"), 3000);
}

void MainWindow::exportImage()
{
  if (!document_) {
    return;
  }

  QString selectedFilter;
  auto path = QFileDialog::getSaveFileName(
    this,
    tr("Export Image"),
    lastDirectory() + QLatin1Char('/') + documentTitle() + QStringLiteral(".png"),
    tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg)"),
    &selectedFilter);

  if (path.isEmpty()) {
    return;
  }

  const auto wantsJpeg = selectedFilter.contains(QStringLiteral("JPEG"), Qt::CaseInsensitive)
                         || QFileInfo(path).suffix().compare(QStringLiteral("jpg"), Qt::CaseInsensitive) == 0
                         || QFileInfo(path).suffix().compare(QStringLiteral("jpeg"), Qt::CaseInsensitive) == 0;

  path = withDefaultSuffix(path, wantsJpeg ? QStringLiteral("jpg") : QStringLiteral("png"));

  QImage output = canvas_->renderComposite(Qt::transparent);
  if (wantsJpeg) {
    QImage flattened(output.size(), QImage::Format_RGB32);
    flattened.fill(Qt::white);
    QPainter painter(&flattened);
    painter.drawImage(QPoint(0, 0), output);
    painter.end();
    output = flattened;
  }

  if (!output.save(path)) {
    QMessageBox::critical(this, tr("Export Image"), tr("The image could not be exported."));
    return;
  }

  rememberDirectory(path);
  statusBar()->showMessage(tr("Exported image"), 3000);
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
    updateHistoryPanel();
    updateActions();
  }
}

void MainWindow::redo()
{
  if (undoStack_.redo()) {
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
  newAction_->setShortcut(QKeySequence::New);
  connect(newAction_, &QAction::triggered, this, &MainWindow::newDocument);

  openAction_ = new QAction(style->standardIcon(QStyle::SP_DirOpenIcon), tr("Open Image"), this);
  openAction_->setShortcut(QKeySequence::Open);
  connect(openAction_, &QAction::triggered, this, &MainWindow::openImage);

  saveAction_ = new QAction(style->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project"), this);
  saveAction_->setShortcut(QKeySequence::Save);
  connect(saveAction_, &QAction::triggered, this, &MainWindow::saveProject);

  exportAction_ = new QAction(tr("Export Image"), this);
  exportAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
  connect(exportAction_, &QAction::triggered, this, &MainWindow::exportImage);

  fitAction_ = new QAction(tr("Fit to View"), this);
  fitAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
  connect(fitAction_, &QAction::triggered, this, &MainWindow::fitCanvasToView);

  actualSizeAction_ = new QAction(tr("Actual Size"), this);
  actualSizeAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
  connect(actualSizeAction_, &QAction::triggered, this, &MainWindow::setCanvasToActualSize);

  undoAction_ = new QAction(tr("Undo"), this);
  undoAction_->setShortcut(QKeySequence::Undo);
  connect(undoAction_, &QAction::triggered, this, &MainWindow::undo);

  redoAction_ = new QAction(tr("Redo"), this);
  redoAction_->setShortcut(QKeySequence::Redo);
  connect(redoAction_, &QAction::triggered, this, &MainWindow::redo);
}

void MainWindow::createMenus()
{
  auto* fileMenu = menuBar()->addMenu(tr("File"));
  fileMenu->addAction(newAction_);
  fileMenu->addAction(openAction_);
  fileMenu->addSeparator();
  fileMenu->addAction(saveAction_);
  fileMenu->addAction(exportAction_);
  fileMenu->addSeparator();
  fileMenu->addAction(tr("Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

  auto* editMenu = menuBar()->addMenu(tr("Edit"));
  editMenu->addAction(undoAction_);
  editMenu->addAction(redoAction_);

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

  const QStringList tools = {
    tr("Move"),
    tr("Select"),
    tr("Crop"),
    tr("Brush"),
    tr("Erase"),
    tr("Fill"),
    tr("Text"),
    tr("Shape"),
    tr("Pen"),
    tr("Pick")
  };

  for (const auto& toolName : tools) {
    auto* action = toolRail->addAction(toolName);
    action->setCheckable(true);
    action->setToolTip(toolName);
  }

  if (!toolRail->actions().isEmpty()) {
    toolRail->actions().first()->setChecked(true);
  }
}

void MainWindow::createPanels()
{
  auto* layersDock = new QDockWidget(tr("Layers"), this);
  layersDock->setObjectName(QStringLiteral("layersDock"));
  layersList_ = new QListWidget(layersDock);
  layersDock->setWidget(layersList_);
  addDockWidget(Qt::RightDockWidgetArea, layersDock);

  auto* propertiesDock = new QDockWidget(tr("Properties"), this);
  propertiesDock->setObjectName(QStringLiteral("propertiesDock"));
  propertiesLabel_ = new QLabel(propertiesDock);
  propertiesLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  propertiesLabel_->setMargin(12);
  propertiesLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  propertiesDock->setWidget(propertiesLabel_);
  addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

  auto* historyDock = new QDockWidget(tr("History"), this);
  historyDock->setObjectName(QStringLiteral("historyDock"));
  historyList_ = new QListWidget(historyDock);
  historyDock->setWidget(historyList_);
  addDockWidget(Qt::RightDockWidgetArea, historyDock);
  tabifyDockWidget(propertiesDock, historyDock);
}

void MainWindow::createInitialDocument()
{
  auto document = qmx::core::Document::create(
    "Untitled",
    {defaultDocumentWidth, defaultDocumentHeight});
  setDocument(std::move(document), {});
}

void MainWindow::setDocument(qmx::core::Document document, QVector<CanvasView::LayerImage> layers)
{
  document_ = std::make_unique<qmx::core::Document>(std::move(document));
  layers_ = std::move(layers);

  canvas_->setDocumentSize(QSize(document_->canvasSize().width, document_->canvasSize().height));
  canvas_->setLayers(layers_);

  updateLayerPanel();
  updatePropertiesPanel();
  updateHistoryPanel();
  updateActions();
  updateWindowTitle();
}

void MainWindow::updateLayerPanel()
{
  layersList_->clear();

  if (!document_) {
    return;
  }

  for (auto it = document_->layers().rbegin(); it != document_->layers().rend(); ++it) {
    layersList_->addItem(QStringLiteral("%1  [%2]")
                           .arg(QString::fromStdString(it->name))
                           .arg(QString::fromUtf8(qmx::core::toString(it->type).data(),
                                                  static_cast<qsizetype>(qmx::core::toString(it->type).size()))));
  }

  if (layersList_->count() == 0) {
    layersList_->addItem(tr("No layers yet"));
  }
}

void MainWindow::updatePropertiesPanel()
{
  if (!document_) {
    propertiesLabel_->clear();
    return;
  }

  propertiesLabel_->setText(tr("Document\n%1\n\nCanvas\n%2 x %3 px\n\nLayers\n%4\n\nLog\n%5")
                              .arg(documentTitle())
                              .arg(document_->canvasSize().width)
                              .arg(document_->canvasSize().height)
                              .arg(document_->layers().size())
                              .arg(qmx::app::logFilePath()));
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
  saveAction_->setEnabled(hasDocument);
  exportAction_->setEnabled(hasDocument);
  fitAction_->setEnabled(hasDocument);
  actualSizeAction_->setEnabled(hasDocument);
  undoAction_->setEnabled(undoStack_.canUndo());
  redoAction_->setEnabled(undoStack_.canRedo());
}

void MainWindow::updateWindowTitle()
{
  setWindowTitle(tr("%1 - Qimage Max").arg(documentTitle()));
}

void MainWindow::rememberDirectory(const QString& filePath)
{
  settings_.setValue(QStringLiteral("files/lastDirectory"), QFileInfo(filePath).absolutePath());
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

QVector<ProjectRasterLayer> MainWindow::rasterLayerPayloads() const
{
  QVector<ProjectRasterLayer> payloads;
  payloads.reserve(layers_.size());

  for (const auto& layer : layers_) {
    ProjectRasterLayer payload;
    payload.id = layer.id;
    payload.name = layer.name;
    payload.image = layer.image;
    payloads.push_back(std::move(payload));
  }

  return payloads;
}

}  // namespace qmx::ui
