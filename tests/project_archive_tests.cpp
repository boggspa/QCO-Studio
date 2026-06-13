#include "core/Document.h"
#include "ui/ProjectArchive.h"

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QPoint>
#include <QTemporaryDir>

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
  QCoreApplication app(argc, argv);

  auto document = qco::core::Document::create("Archive Test", {16, 16});
  const auto layerId = document.addLayer("Pixels", qco::core::LayerType::Raster, {16, 16});
  CHECK(document.setLayerPosition(layerId, {3, 4}));
  CHECK(document.setLayerOpacity(layerId, 0.5));
  CHECK(document.setLayerVisibility(layerId, false));

  QImage image(QSize(16, 16), QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(255, 0, 0, 255));

  QVector<qco::ui::ProjectRasterLayer> layers;
  layers.push_back({layerId, QStringLiteral("Pixels"), image, QPoint(3, 4), false, 0.5});

  QTemporaryDir tempDir;
  CHECK(tempDir.isValid());

  const auto outputPath = tempDir.path() + QStringLiteral("/sample.qco");
  QString errorMessage;
  const auto saved = qco::ui::ProjectArchive::save(outputPath, document, layers, &errorMessage);
  if (!saved) {
    std::cerr << "save failed: " << errorMessage.toStdString() << '\n';
    return 1;
  }
  CHECK(errorMessage.isEmpty());

  QFile file(outputPath);
  if (!file.open(QIODevice::ReadOnly)) {
    std::cerr << "open saved project failed: " << file.errorString().toStdString() << '\n';
    return 1;
  }

  const auto bytes = file.readAll();
  CHECK(bytes.startsWith(QByteArray::fromHex("504b0304")));
  CHECK(bytes.contains("manifest.json"));
  CHECK(bytes.contains("document.json"));
  CHECK(bytes.contains("layers/1.png"));

  errorMessage.clear();
  const auto loaded = qco::ui::ProjectArchive::load(outputPath, &errorMessage);
  if (!loaded.has_value()) {
    std::cerr << "load failed: " << errorMessage.toStdString() << '\n';
    return 1;
  }
  CHECK(errorMessage.isEmpty());
  CHECK(loaded->document.title() == "Archive Test");
  CHECK(loaded->document.canvasSize().width == 16);
  CHECK(loaded->document.canvasSize().height == 16);
  CHECK(loaded->document.layers().size() == 1);

  const auto* loadedLayer = loaded->document.findLayer(layerId);
  CHECK(loadedLayer != nullptr);
  CHECK(loadedLayer->name == "Pixels");
  CHECK(loadedLayer->position.x == 3);
  CHECK(loadedLayer->position.y == 4);
  CHECK(loadedLayer->opacity == 0.5);
  CHECK(!loadedLayer->visible);

  CHECK(loaded->rasterLayers.size() == 1);
  CHECK(loaded->rasterLayers[0].id == layerId);
  CHECK(loaded->rasterLayers[0].position == QPoint(3, 4));
  CHECK(!loaded->rasterLayers[0].visible);
  CHECK(loaded->rasterLayers[0].opacity == 0.5);
  CHECK(loaded->rasterLayers[0].image.pixelColor(0, 0) == QColor(255, 0, 0, 255));

  return 0;
}
