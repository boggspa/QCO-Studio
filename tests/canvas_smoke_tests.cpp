#include "ui/CanvasView.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QMouseEvent>
#include <QRect>

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

  qco::ui::CanvasView canvas;
  canvas.setDocumentSize(QSize(8, 8));

  QImage image(QSize(4, 4), QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(0, 128, 255, 255));

  qco::ui::CanvasView::LayerImage layer;
  layer.id = 7;
  layer.name = QStringLiteral("Smoke");
  layer.image = image;
  layer.position = QPoint(2, 2);

  QVector<qco::ui::CanvasView::LayerImage> layers;
  layers.push_back(layer);
  canvas.setLayers(layers);

  const auto composite = canvas.renderComposite(Qt::transparent);
  CHECK(composite.size() == QSize(8, 8));
  CHECK(composite.pixelColor(0, 0).alpha() == 0);
  CHECK(composite.pixelColor(2, 2) == QColor(0, 128, 255, 255));

  QRect cropRect;
  QObject::connect(&canvas, &qco::ui::CanvasView::cropCommitted, [&cropRect](QRect documentRect) {
    cropRect = documentRect;
  });

  canvas.resize(200, 200);
  canvas.setDocumentSize(QSize(100, 100));
  canvas.setZoom(1.0);
  canvas.setActiveTool(qco::ui::CanvasView::Tool::Crop);

  QMouseEvent press(
    QEvent::MouseButtonPress,
    QPointF(60, 62),
    QPointF(60, 62),
    Qt::LeftButton,
    Qt::LeftButton,
    Qt::NoModifier);
  QApplication::sendEvent(&canvas, &press);

  QMouseEvent move(
    QEvent::MouseMove,
    QPointF(90, 90),
    QPointF(90, 90),
    Qt::NoButton,
    Qt::LeftButton,
    Qt::NoModifier);
  QApplication::sendEvent(&canvas, &move);

  QMouseEvent release(
    QEvent::MouseButtonRelease,
    QPointF(90, 90),
    QPointF(90, 90),
    Qt::LeftButton,
    Qt::NoButton,
    Qt::NoModifier);
  QApplication::sendEvent(&canvas, &release);
  CHECK(cropRect == QRect(10, 12, 30, 28));

  return 0;
}
