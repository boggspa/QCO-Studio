#include "core/Document.h"
#include "ui/ProjectArchive.h"

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QPoint>
#include <QTemporaryDir>

#include <cassert>

int main()
{
  auto document = qco::core::Document::create("Archive Test", {16, 16});
  const auto layerId = document.addLayer("Pixels", qco::core::LayerType::Raster, {16, 16});
  assert(document.setLayerPosition(layerId, {3, 4}));
  assert(document.setLayerOpacity(layerId, 0.5));
  assert(document.setLayerVisibility(layerId, false));

  QImage image(QSize(16, 16), QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(255, 0, 0, 255));

  QVector<qco::ui::ProjectRasterLayer> layers;
  layers.push_back({layerId, QStringLiteral("Pixels"), image, QPoint(3, 4), false, 0.5});

  QTemporaryDir tempDir;
  assert(tempDir.isValid());

  const auto outputPath = tempDir.path() + QStringLiteral("/sample.qco");
  QString errorMessage;
  const auto saved = qco::ui::ProjectArchive::save(outputPath, document, layers, &errorMessage);
  assert(saved);
  assert(errorMessage.isEmpty());

  QFile file(outputPath);
  assert(file.open(QIODevice::ReadOnly));

  const auto bytes = file.readAll();
  assert(bytes.startsWith(QByteArray::fromHex("504b0304")));
  assert(bytes.contains("manifest.json"));
  assert(bytes.contains("document.json"));
  assert(bytes.contains("layers/1.png"));

  errorMessage.clear();
  const auto loaded = qco::ui::ProjectArchive::load(outputPath, &errorMessage);
  assert(loaded.has_value());
  assert(errorMessage.isEmpty());
  assert(loaded->document.title() == "Archive Test");
  assert(loaded->document.canvasSize().width == 16);
  assert(loaded->document.canvasSize().height == 16);
  assert(loaded->document.layers().size() == 1);

  const auto* loadedLayer = loaded->document.findLayer(layerId);
  assert(loadedLayer != nullptr);
  assert(loadedLayer->name == "Pixels");
  assert(loadedLayer->position.x == 3);
  assert(loadedLayer->position.y == 4);
  assert(loadedLayer->opacity == 0.5);
  assert(!loadedLayer->visible);

  assert(loaded->rasterLayers.size() == 1);
  assert(loaded->rasterLayers[0].id == layerId);
  assert(loaded->rasterLayers[0].position == QPoint(3, 4));
  assert(!loaded->rasterLayers[0].visible);
  assert(loaded->rasterLayers[0].opacity == 0.5);
  assert(loaded->rasterLayers[0].image.pixelColor(0, 0) == QColor(255, 0, 0, 255));

  return 0;
}
