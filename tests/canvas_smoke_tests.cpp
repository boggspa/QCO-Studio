#include "ui/CanvasView.h"

#include <QApplication>
#include <QColor>
#include <QImage>

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

  return 0;
}
