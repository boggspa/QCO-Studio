#include "ui/CanvasView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>

namespace qmx::ui {

namespace {

constexpr qreal minZoom = 0.02;
constexpr qreal maxZoom = 64.0;
constexpr int checkerSize = 16;

[[nodiscard]] qreal clampedZoom(qreal zoom)
{
  return std::clamp(zoom, minZoom, maxZoom);
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
  }

  QPen borderPen(QColor(98, 105, 116));
  borderPen.setCosmetic(true);
  painter.setPen(borderPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(documentRect);
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

  QWidget::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
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

}  // namespace qmx::ui
