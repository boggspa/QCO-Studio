#include "ui/CanvasView.h"

#include <QApplication>
#include <QColor>
#include <QImage>

#include <cassert>

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
  assert(composite.size() == QSize(8, 8));
  assert(composite.pixelColor(0, 0).alpha() == 0);
  assert(composite.pixelColor(2, 2) == QColor(0, 128, 255, 255));

  return 0;
}
