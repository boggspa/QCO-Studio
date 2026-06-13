#include "core/Document.h"
#include "render/DocumentRenderer.h"
#include "render/TileScheduler.h"

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
  const QImage rendered = renderer.render(
    document,
    layerImages,
    Qt::transparent,
    qco::render::RenderSettings{QSize(3, 2)});

  CHECK(!rendered.isNull());
  CHECK(rendered.size() == QSize(8, 6));
  CHECK(rendered.pixelColor(0, 0) == QColor(255, 0, 0, 255));
  CHECK(rendered.pixelColor(2, 1) == QColor(0, 0, 255, 255));
  CHECK(rendered.pixelColor(3, 2) == QColor(0, 0, 255, 255));
  CHECK(rendered.pixelColor(4, 2) == QColor(255, 0, 0, 255));

  const QImage tile = renderer.renderTile(document, layerImages, QRect(2, 1, 3, 2));
  CHECK(!tile.isNull());
  CHECK(tile.size() == QSize(3, 2));
  CHECK(tile.pixelColor(0, 0) == QColor(0, 0, 255, 255));
  CHECK(tile.pixelColor(1, 1) == QColor(0, 0, 255, 255));
  CHECK(tile.pixelColor(2, 1) == QColor(255, 0, 0, 255));

  const auto canvasTiles = qco::render::tileRectsForCanvas(QSize(5, 4), QSize(3, 2));
  CHECK(canvasTiles.size() == 4);
  CHECK(canvasTiles[0] == QRect(0, 0, 3, 2));
  CHECK(canvasTiles[1] == QRect(3, 0, 2, 2));
  CHECK(canvasTiles[2] == QRect(0, 2, 3, 2));
  CHECK(canvasTiles[3] == QRect(3, 2, 2, 2));

  const auto dirtyTiles = qco::render::tileRectsForDirtyRect(QSize(10, 8), QSize(4, 3), QRect(3, 2, 5, 3));
  CHECK(dirtyTiles.size() == 4);
  CHECK(dirtyTiles[0] == QRect(0, 0, 4, 3));
  CHECK(dirtyTiles[1] == QRect(4, 0, 4, 3));
  CHECK(dirtyTiles[2] == QRect(0, 3, 4, 3));
  CHECK(dirtyTiles[3] == QRect(4, 3, 4, 3));

  qco::render::DirtyTileScheduler scheduler(QSize(10, 8), QSize(4, 3));
  CHECK(!scheduler.hasDirtyTiles());
  scheduler.invalidate(QRect(4, 3, 1, 1));
  scheduler.invalidate(QRect(5, 4, 1, 1));
  CHECK(scheduler.hasDirtyTiles());
  auto scheduledTiles = scheduler.takeDirtyTiles();
  CHECK(scheduledTiles.size() == 1);
  CHECK(scheduledTiles[0] == QRect(4, 3, 4, 3));
  CHECK(!scheduler.hasDirtyTiles());

  scheduler.invalidateAll();
  scheduledTiles = scheduler.takeDirtyTiles();
  CHECK(scheduledTiles == qco::render::tileRectsForCanvas(QSize(10, 8), QSize(4, 3)));

  return 0;
}
