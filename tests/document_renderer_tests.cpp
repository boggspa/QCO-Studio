#include "core/Document.h"
#include "render/DocumentRenderer.h"

#include <QColor>
#include <QCoreApplication>
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

[[nodiscard]] QImage solidImage(QSize size, QColor color)
{
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(color);
  return image;
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
  QCoreApplication app(argc, argv);

  auto document = qco::core::Document::create("Renderer Test", {8, 6});
  const auto bottomId = document.addLayer("Bottom", qco::core::LayerType::Raster, {8, 6}, {0, 0});
  const auto hiddenId = document.addLayer("Hidden", qco::core::LayerType::Raster, {8, 6}, {0, 0});
  const auto topId = document.addLayer("Top", qco::core::LayerType::Raster, {2, 2}, {2, 1});
  const auto transparentId = document.addLayer("Transparent", qco::core::LayerType::Raster, {2, 2}, {4, 2});

  CHECK(document.setLayerVisibility(hiddenId, false));
  CHECK(document.setLayerOpacity(transparentId, 0.0));

  QVector<qco::render::LayerImage> layerImages;
  layerImages.push_back({topId, solidImage(QSize(2, 2), QColor(0, 0, 255, 255))});
  layerImages.push_back({transparentId, solidImage(QSize(2, 2), QColor(255, 255, 0, 255))});
  layerImages.push_back({hiddenId, solidImage(QSize(8, 6), QColor(0, 255, 0, 255))});
  layerImages.push_back({bottomId, solidImage(QSize(8, 6), QColor(255, 0, 0, 255))});
  layerImages.push_back({9999, solidImage(QSize(8, 6), QColor(255, 0, 255, 255))});

  const qco::render::QtDocumentRenderer renderer;
  const QImage rendered = renderer.render(document, layerImages);

  CHECK(!rendered.isNull());
  CHECK(rendered.size() == QSize(8, 6));
  CHECK(rendered.pixelColor(0, 0) == QColor(255, 0, 0, 255));
  CHECK(rendered.pixelColor(2, 1) == QColor(0, 0, 255, 255));
  CHECK(rendered.pixelColor(4, 2) == QColor(255, 0, 0, 255));

  return 0;
}
