#include "core/Document.h"
#include "ui/ProjectArchive.h"

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <cassert>

int main()
{
  auto document = qmx::core::Document::create("Archive Test", {16, 16});
  const auto layerId = document.addLayer("Pixels", qmx::core::LayerType::Raster, {16, 16});

  QImage image(QSize(16, 16), QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(255, 0, 0, 255));

  QVector<qmx::ui::ProjectRasterLayer> layers;
  layers.push_back({layerId, QStringLiteral("Pixels"), image});

  QTemporaryDir tempDir;
  assert(tempDir.isValid());

  const auto outputPath = tempDir.path() + QStringLiteral("/sample.qmxdoc");
  QString errorMessage;
  const auto saved = qmx::ui::ProjectArchive::save(outputPath, document, layers, &errorMessage);
  assert(saved);
  assert(errorMessage.isEmpty());

  QFile file(outputPath);
  assert(file.open(QIODevice::ReadOnly));

  const auto bytes = file.readAll();
  assert(bytes.startsWith(QByteArray::fromHex("504b0304")));
  assert(bytes.contains("manifest.json"));
  assert(bytes.contains("document.json"));
  assert(bytes.contains("layers/1.png"));

  return 0;
}
