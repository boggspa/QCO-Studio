#include "core/Document.h"
#include "ui/ProjectArchive.h"

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QPoint>
#include <QTemporaryDir>

#include <cmath>
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

  auto document = qco::core::Document::create("Archive Test", {64, 48});
  const auto rasterId = document.addLayer("Pixels", qco::core::LayerType::Raster, {16, 16}, {3, 4});
  const auto textId = document.addLayer("Headline", qco::core::LayerType::Text, {32, 12}, {10, 8});
  const auto shapeId = document.addLayer("Badge", qco::core::LayerType::Shape, {24, 18}, {30, 20});

  CHECK(document.setLayerOpacity(rasterId, 0.5));
  CHECK(document.setLayerVisibility(rasterId, false));
  CHECK(document.setLayerOpacity(textId, 0.65));
  CHECK(document.setLayerOpacity(shapeId, 0.8));

  auto* textLayer = document.findLayer(textId);
  CHECK(textLayer != nullptr);
  textLayer->locked = true;
  textLayer->blendMode = qco::core::BlendMode::Screen;

  auto* shapeLayer = document.findLayer(shapeId);
  CHECK(shapeLayer != nullptr);
  shapeLayer->blendMode = qco::core::BlendMode::Multiply;

  QVector<qco::ui::ProjectRasterLayer> layers;
  layers.push_back({
    rasterId,
    QStringLiteral("Pixels"),
    solidImage(QSize(16, 16), QColor(255, 0, 0, 255)),
    QPoint(3, 4),
    false,
    0.5,
  });
  layers.push_back({
    textId,
    QStringLiteral("Headline"),
    solidImage(QSize(32, 12), QColor(0, 255, 0, 255)),
    QPoint(10, 8),
    true,
    0.65,
  });
  layers.push_back({
    shapeId,
    QStringLiteral("Badge"),
    solidImage(QSize(24, 18), QColor(0, 0, 255, 180)),
    QPoint(30, 20),
    true,
    0.8,
  });

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
  CHECK(bytes.contains(QStringLiteral("layers/%1.png").arg(rasterId).toUtf8()));
  CHECK(bytes.contains(QStringLiteral("layers/%1.png").arg(textId).toUtf8()));
  CHECK(bytes.contains(QStringLiteral("layers/%1.png").arg(shapeId).toUtf8()));

  errorMessage.clear();
  const auto loaded = qco::ui::ProjectArchive::load(outputPath, &errorMessage);
  if (!loaded.has_value()) {
    std::cerr << "load failed: " << errorMessage.toStdString() << '\n';
    return 1;
  }
  CHECK(errorMessage.isEmpty());
  CHECK(loaded->document.title() == "Archive Test");
  CHECK(loaded->document.canvasSize().width == 64);
  CHECK(loaded->document.canvasSize().height == 48);
  CHECK(loaded->document.layers().size() == 3);
  CHECK(loaded->document.layers()[0].id == rasterId);
  CHECK(loaded->document.layers()[1].id == textId);
  CHECK(loaded->document.layers()[2].id == shapeId);

  const auto* loadedRasterLayer = loaded->document.findLayer(rasterId);
  CHECK(loadedRasterLayer != nullptr);
  CHECK(loadedRasterLayer->name == "Pixels");
  CHECK(loadedRasterLayer->type == qco::core::LayerType::Raster);
  CHECK(loadedRasterLayer->position.x == 3);
  CHECK(loadedRasterLayer->position.y == 4);
  CHECK(loadedRasterLayer->size.width == 16);
  CHECK(loadedRasterLayer->size.height == 16);
  CHECK(std::abs(loadedRasterLayer->opacity - 0.5) < 0.001);
  CHECK(!loadedRasterLayer->visible);

  const auto* loadedTextLayer = loaded->document.findLayer(textId);
  CHECK(loadedTextLayer != nullptr);
  CHECK(loadedTextLayer->name == "Headline");
  CHECK(loadedTextLayer->type == qco::core::LayerType::Text);
  CHECK(loadedTextLayer->locked);
  CHECK(loadedTextLayer->blendMode == qco::core::BlendMode::Screen);
  CHECK(loadedTextLayer->position.x == 10);
  CHECK(loadedTextLayer->position.y == 8);
  CHECK(loadedTextLayer->size.width == 32);
  CHECK(loadedTextLayer->size.height == 12);
  CHECK(std::abs(loadedTextLayer->opacity - 0.65) < 0.001);

  const auto* loadedShapeLayer = loaded->document.findLayer(shapeId);
  CHECK(loadedShapeLayer != nullptr);
  CHECK(loadedShapeLayer->name == "Badge");
  CHECK(loadedShapeLayer->type == qco::core::LayerType::Shape);
  CHECK(loadedShapeLayer->blendMode == qco::core::BlendMode::Multiply);
  CHECK(loadedShapeLayer->position.x == 30);
  CHECK(loadedShapeLayer->position.y == 20);
  CHECK(loadedShapeLayer->size.width == 24);
  CHECK(loadedShapeLayer->size.height == 18);
  CHECK(std::abs(loadedShapeLayer->opacity - 0.8) < 0.001);

  CHECK(loaded->rasterLayers.size() == 3);
  CHECK(loaded->rasterLayers[0].id == rasterId);
  CHECK(loaded->rasterLayers[0].position == QPoint(3, 4));
  CHECK(!loaded->rasterLayers[0].visible);
  CHECK(std::abs(loaded->rasterLayers[0].opacity - 0.5) < 0.001);
  CHECK(loaded->rasterLayers[0].image.pixelColor(0, 0) == QColor(255, 0, 0, 255));
  CHECK(loaded->rasterLayers[1].id == textId);
  CHECK(loaded->rasterLayers[1].position == QPoint(10, 8));
  CHECK(std::abs(loaded->rasterLayers[1].opacity - 0.65) < 0.001);
  CHECK(loaded->rasterLayers[1].image.pixelColor(0, 0) == QColor(0, 255, 0, 255));
  CHECK(loaded->rasterLayers[2].id == shapeId);
  CHECK(loaded->rasterLayers[2].position == QPoint(30, 20));
  CHECK(std::abs(loaded->rasterLayers[2].opacity - 0.8) < 0.001);
  CHECK(loaded->rasterLayers[2].image.pixelColor(0, 0) == QColor(0, 0, 255, 180));

  return 0;
}
