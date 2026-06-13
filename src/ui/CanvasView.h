#pragma once

#include <QImage>
#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QVector>
#include <QWidget>

#include <cstdint>

namespace qco::ui {

class CanvasView final : public QWidget {
  Q_OBJECT

public:
  struct LayerImage {
    std::uint64_t id = 0;
    QString name;
    QImage image;
    QPoint position;
    bool visible = true;
    qreal opacity = 1.0;
  };

  explicit CanvasView(QWidget* parent = nullptr);

  void setDocumentSize(QSize size);
  [[nodiscard]] QSize documentSize() const noexcept;

  void setLayers(QVector<LayerImage> layers);
  [[nodiscard]] const QVector<LayerImage>& layers() const noexcept;

  void setSelectedLayerId(std::uint64_t id);
  [[nodiscard]] std::uint64_t selectedLayerId() const noexcept;

  void setZoom(qreal zoom);
  [[nodiscard]] qreal zoom() const noexcept;

  void fitToView();
  [[nodiscard]] QImage renderComposite(const QColor& background = Qt::transparent) const;

signals:
  void zoomChanged(qreal zoom);
  void layerSelected(std::uint64_t id);
  void layerMoveStarted(std::uint64_t id);
  void layerMovePreview(std::uint64_t id, QPoint position);
  void layerMoveCommitted(std::uint64_t id, QPoint oldPosition, QPoint newPosition);

protected:
  void paintEvent(QPaintEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  [[nodiscard]] QPointF documentToWidget(QPointF point) const;
  [[nodiscard]] QPointF widgetToDocument(QPointF point) const;
  [[nodiscard]] std::uint64_t layerAt(QPointF documentPoint) const;
  void drawCheckerboard(QPainter& painter, const QRectF& rect) const;

  QSize documentSize_ = QSize(1280, 800);
  QVector<LayerImage> layers_;
  std::uint64_t selectedLayerId_ = 0;
  qreal zoom_ = 1.0;
  QPointF pan_;
  bool panning_ = false;
  bool movingLayer_ = false;
  QPoint moveStartPosition_;
  QPoint moveLastPosition_;
  QPointF moveOffset_;
  QPoint lastMousePosition_;
};

}  // namespace qco::ui
