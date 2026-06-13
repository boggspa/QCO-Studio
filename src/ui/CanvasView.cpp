#include "ui/CanvasView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>

namespace qco::ui {

namespace {

constexpr qreal minZoom = 0.02;
constexpr qreal maxZoom = 64.0;
constexpr int checkerSize = 16;

[[nodiscard]] qreal clampedZoom(qreal zoom)
{
  return std::clamp(zoom, minZoom, maxZoom);
}

[[nodiscard]] QPoint clampedDocumentPoint(QPoint point, QSize size)
{
  return {
    std::clamp(point.x(), 0, size.width()),
    std::clamp(point.y(), 0, size.height()),
  };
}

[[nodiscard]] QRect documentRectBetween(QPoint start, QPoint end)
{
  const int left = std::min(start.x(), end.x());
  const int top = std::min(start.y(), end.y());
  const int right = std::max(start.x(), end.x());
  const int bottom = std::max(start.y(), end.y());
  return QRect(QPoint(left, top), QSize(right - left, bottom - top));
}

}  // namespace

CanvasView::CanvasView(QWidget* parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
}

void CanvasView::setDocumentSize(QSize size)
{
  if (!size.isValid() || size.isEmpty()) {
    return;
  }

  documentSize_ = size;
  fitToView();
  update();
}

QSize CanvasView::documentSize() const noexcept
{
  return documentSize_;
}

void CanvasView::setLayers(QVector<LayerImage> layers)
{
  layers_ = std::move(layers);
  update();
}

const QVector<CanvasView::LayerImage>& CanvasView::layers() const noexcept
{
  return layers_;
}

void CanvasView::setSelectedLayerId(std::uint64_t id)
{
  if (selectedLayerId_ == id) {
    return;
  }

  selectedLayerId_ = id;
  update();
}

std::uint64_t CanvasView::selectedLayerId() const noexcept
{
  return selectedLayerId_;
}

void CanvasView::setActiveTool(Tool tool)
{
  if (activeTool_ == tool) {
    return;
  }

  activeTool_ = tool;
  cropping_ = false;
  drawingStroke_ = false;
  movingLayer_ = false;
  unsetCursor();
  update();
}

CanvasView::Tool CanvasView::activeTool() const noexcept
{
  return activeTool_;
}

void CanvasView::setZoom(qreal zoom)
{
  const auto nextZoom = clampedZoom(zoom);
  if (qFuzzyCompare(nextZoom, zoom_)) {
    return;
  }

  zoom_ = nextZoom;
  emit zoomChanged(zoom_);
  update();
}

qreal CanvasView::zoom() const noexcept
{
  return zoom_;
}

void CanvasView::fitToView()
{
  if (!documentSize_.isValid() || width() <= 0 || height() <= 0) {
    return;
  }

  constexpr qreal padding = 96.0;
  const auto availableWidth = std::max<qreal>(1.0, static_cast<qreal>(width()) - padding);
  const auto availableHeight = std::max<qreal>(1.0, static_cast<qreal>(height()) - padding);
  zoom_ = clampedZoom(std::min(availableWidth / documentSize_.width(), availableHeight / documentSize_.height()));
  pan_ = QPointF(0.0, 0.0);
  emit zoomChanged(zoom_);
  update();
}

QImage CanvasView::renderComposite(const QColor& background) const
{
  QImage output(documentSize_, QImage::Format_ARGB32_Premultiplied);
  output.fill(background);

  QPainter painter(&output);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  for (const auto& layer : layers_) {
    if (!layer.visible || layer.image.isNull()) {
      continue;
    }

    painter.setOpacity(std::clamp(layer.opacity, 0.0, 1.0));
    painter.drawImage(layer.position, layer.image);
  }

  painter.end();
  return output;
}

void CanvasView::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.fillRect(rect(), QColor(32, 34, 38));

  if (!documentSize_.isValid()) {
    painter.setPen(QColor(180, 184, 190));
    painter.drawText(rect(), Qt::AlignCenter, tr("No document"));
    return;
  }

  painter.save();
  painter.translate(width() / 2.0 + pan_.x(), height() / 2.0 + pan_.y());
  painter.scale(zoom_, zoom_);
  painter.translate(-documentSize_.width() / 2.0, -documentSize_.height() / 2.0);

  const QRectF documentRect(QPointF(0.0, 0.0), QSizeF(documentSize_));
  drawCheckerboard(painter, documentRect);

  for (const auto& layer : layers_) {
    if (!layer.visible || layer.image.isNull()) {
      continue;
    }

    painter.save();
    painter.setOpacity(std::clamp(layer.opacity, 0.0, 1.0));
    painter.drawImage(layer.position, layer.image);
    painter.restore();

    if (layer.id == selectedLayerId_) {
      QPen selectionPen(QColor(45, 156, 219));
      selectionPen.setCosmetic(true);
      selectionPen.setWidth(2);
      painter.setPen(selectionPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(QRectF(layer.position, QSizeF(layer.image.size())));
    }
  }

  QPen borderPen(QColor(98, 105, 116));
  borderPen.setCosmetic(true);
  painter.setPen(borderPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(documentRect);

  if (cropping_) {
    const QRect cropRect = documentRectBetween(cropStartPoint_, cropEndPoint_)
                             .intersected(QRect(QPoint(0, 0), documentSize_));
    if (!cropRect.isEmpty()) {
      const QRectF cropPreview(cropRect);
      painter.fillRect(cropPreview, QColor(45, 156, 219, 32));

      QPen cropPen(QColor(255, 255, 255));
      cropPen.setCosmetic(true);
      cropPen.setWidth(1);
      cropPen.setStyle(Qt::DashLine);
      painter.setPen(cropPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(cropPreview);
    }
  }

  painter.restore();
}

void CanvasView::wheelEvent(QWheelEvent* event)
{
  if (!documentSize_.isValid()) {
    return;
  }

  const auto before = widgetToDocument(event->position());
  const auto direction = event->angleDelta().y() >= 0 ? 1.12 : 1.0 / 1.12;
  const auto nextZoom = clampedZoom(zoom_ * direction);

  if (!qFuzzyCompare(nextZoom, zoom_)) {
    zoom_ = nextZoom;
    const auto afterWidget = documentToWidget(before);
    pan_ += event->position() - afterWidget;
    emit zoomChanged(zoom_);
    update();
  }

  event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
    panning_ = true;
    lastMousePosition_ = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton) {
    const auto documentPoint = widgetToDocument(event->position());
    const QPoint roundedDocumentPoint = clampedDocumentPoint(
      QPoint(qRound(documentPoint.x()), qRound(documentPoint.y())),
      documentSize_);

    if (activeTool_ == Tool::Crop) {
      cropping_ = true;
      cropStartPoint_ = roundedDocumentPoint;
      cropEndPoint_ = roundedDocumentPoint;
      setCursor(Qt::CrossCursor);
      update();
      event->accept();
      return;
    }

    if (activeTool_ == Tool::Brush || activeTool_ == Tool::Eraser) {
      drawingStroke_ = true;
      lastStrokePoint_ = roundedDocumentPoint;
      emit rasterStrokeStarted(activeTool_, roundedDocumentPoint);
      emit rasterStrokePreview(activeTool_, roundedDocumentPoint, roundedDocumentPoint);
      event->accept();
      return;
    }

    if (activeTool_ == Tool::Fill || activeTool_ == Tool::Text || activeTool_ == Tool::Shape) {
      emit toolDocumentClicked(activeTool_, roundedDocumentPoint);
      event->accept();
      return;
    }

    if (activeTool_ == Tool::Move || activeTool_ == Tool::Select || activeTool_ == Tool::Pick) {
      const auto id = layerAt(documentPoint);
      setSelectedLayerId(id);
      emit layerSelected(id);

      if (activeTool_ == Tool::Move && id != 0) {
        const auto it = std::find_if(layers_.begin(), layers_.end(), [id](const LayerImage& layer) {
          return layer.id == id;
        });
        if (it != layers_.end()) {
          movingLayer_ = true;
          moveStartPosition_ = it->position;
          moveLastPosition_ = it->position;
          moveOffset_ = documentPoint - QPointF(it->position);
          setCursor(Qt::SizeAllCursor);
          emit layerMoveStarted(id);
          event->accept();
          return;
        }
      }
    }
  }

  QWidget::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
  if (cropping_) {
    const auto documentPoint = widgetToDocument(event->position());
    cropEndPoint_ = clampedDocumentPoint(
      QPoint(qRound(documentPoint.x()), qRound(documentPoint.y())),
      documentSize_);
    update();
    event->accept();
    return;
  }

  if (drawingStroke_) {
    const auto documentPoint = widgetToDocument(event->position());
    const QPoint roundedDocumentPoint = clampedDocumentPoint(
      QPoint(qRound(documentPoint.x()), qRound(documentPoint.y())),
      documentSize_);
    if (roundedDocumentPoint != lastStrokePoint_) {
      emit rasterStrokePreview(activeTool_, lastStrokePoint_, roundedDocumentPoint);
      lastStrokePoint_ = roundedDocumentPoint;
    }
    event->accept();
    return;
  }

  if (movingLayer_ && selectedLayerId_ != 0) {
    const auto documentPoint = widgetToDocument(event->position());
    const QPoint nextPosition(
      qRound(documentPoint.x() - moveOffset_.x()),
      qRound(documentPoint.y() - moveOffset_.y()));

    if (nextPosition != moveLastPosition_) {
      moveLastPosition_ = nextPosition;
      emit layerMovePreview(selectedLayerId_, nextPosition);
    }

    event->accept();
    return;
  }

  if (panning_) {
    pan_ += event->pos() - lastMousePosition_;
    lastMousePosition_ = event->pos();
    update();
    event->accept();
    return;
  }

  QWidget::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
  if (cropping_ && event->button() == Qt::LeftButton) {
    cropping_ = false;
    unsetCursor();

    const auto documentPoint = widgetToDocument(event->position());
    cropEndPoint_ = clampedDocumentPoint(
      QPoint(qRound(documentPoint.x()), qRound(documentPoint.y())),
      documentSize_);

    const QRect cropRect = documentRectBetween(cropStartPoint_, cropEndPoint_)
                             .intersected(QRect(QPoint(0, 0), documentSize_));
    update();
    if (cropRect.width() >= 2 && cropRect.height() >= 2) {
      emit cropCommitted(cropRect);
    }

    event->accept();
    return;
  }

  if (drawingStroke_ && event->button() == Qt::LeftButton) {
    drawingStroke_ = false;
    emit rasterStrokeCommitted(activeTool_);
    event->accept();
    return;
  }

  if (movingLayer_ && event->button() == Qt::LeftButton) {
    movingLayer_ = false;
    unsetCursor();
    emit layerMoveCommitted(selectedLayerId_, moveStartPosition_, moveLastPosition_);
    event->accept();
    return;
  }

  if (panning_ && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
    panning_ = false;
    unsetCursor();
    event->accept();
    return;
  }

  QWidget::mouseReleaseEvent(event);
}

void CanvasView::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  if (event->oldSize().isEmpty()) {
    fitToView();
  }
}

QPointF CanvasView::documentToWidget(QPointF point) const
{
  return QPointF(width() / 2.0 + pan_.x(), height() / 2.0 + pan_.y())
         + (point - QPointF(documentSize_.width() / 2.0, documentSize_.height() / 2.0)) * zoom_;
}

QPointF CanvasView::widgetToDocument(QPointF point) const
{
  return (point - QPointF(width() / 2.0 + pan_.x(), height() / 2.0 + pan_.y())) / zoom_
         + QPointF(documentSize_.width() / 2.0, documentSize_.height() / 2.0);
}

std::uint64_t CanvasView::layerAt(QPointF documentPoint) const
{
  for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
    if (!it->visible || it->image.isNull()) {
      continue;
    }

    const QRectF bounds(QPointF(it->position), QSizeF(it->image.size()));
    if (bounds.contains(documentPoint)) {
      return it->id;
    }
  }

  return 0;
}

void CanvasView::drawCheckerboard(QPainter& painter, const QRectF& rect) const
{
  QPixmap tile(checkerSize * 2, checkerSize * 2);
  tile.fill(QColor(226, 228, 232));

  QPainter tilePainter(&tile);
  tilePainter.fillRect(0, 0, checkerSize, checkerSize, QColor(196, 200, 207));
  tilePainter.fillRect(checkerSize, checkerSize, checkerSize, checkerSize, QColor(196, 200, 207));
  tilePainter.end();

  painter.fillRect(rect, QBrush(tile));
}

}  // namespace qco::ui
