#include "ui/ImageExport.h"

#include <QColor>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

#include <algorithm>
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

[[nodiscard]] const qco::ui::ExportImageFormat* findFormat(
  const QVector<qco::ui::ExportImageFormat>& formats,
  const QString& defaultSuffix)
{
  const auto it = std::find_if(formats.begin(), formats.end(), [&defaultSuffix](const qco::ui::ExportImageFormat& format) {
    return format.defaultSuffix == defaultSuffix;
  });
  return it == formats.end() ? nullptr : &(*it);
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

  const auto syntheticFormats = qco::ui::exportImageFormatsForWriters({
    QByteArrayLiteral("png"),
    QByteArrayLiteral("jpg"),
    QByteArrayLiteral("tiff"),
    QByteArrayLiteral("webp"),
  });
  CHECK(syntheticFormats.size() == 4);
  CHECK(findFormat(syntheticFormats, QStringLiteral("png")) != nullptr);
  CHECK(findFormat(syntheticFormats, QStringLiteral("jpg")) != nullptr);
  CHECK(findFormat(syntheticFormats, QStringLiteral("tif")) != nullptr);
  CHECK(findFormat(syntheticFormats, QStringLiteral("webp")) != nullptr);

  const auto filters = qco::ui::exportImageFileFilter(syntheticFormats);
  CHECK(filters.contains(QStringLiteral("PNG Image (*.png")));
  CHECK(filters.contains(QStringLiteral("JPEG Image (*.jpg *.jpeg")));
  CHECK(filters.contains(QStringLiteral("TIFF Image (*.tif *.tiff")));
  CHECK(filters.contains(QStringLiteral("WebP Image (*.webp")));

  const auto webpByFilter = qco::ui::exportImageFormatForPathOrFilter(
    syntheticFormats,
    QStringLiteral("/tmp/export"),
    QStringLiteral("WebP Image (*.webp)"));
  CHECK(webpByFilter.has_value());
  CHECK(webpByFilter->defaultSuffix == QStringLiteral("webp"));

  const auto tiffBySuffix = qco::ui::exportImageFormatForPathOrFilter(
    syntheticFormats,
    QStringLiteral("/tmp/export.tiff"),
    QString());
  CHECK(tiffBySuffix.has_value());
  CHECK(tiffBySuffix->defaultSuffix == QStringLiteral("tif"));

  const auto suffixBeatsFilter = qco::ui::exportImageFormatForPathOrFilter(
    syntheticFormats,
    QStringLiteral("/tmp/export.jpeg"),
    QStringLiteral("PNG Image (*.png)"));
  CHECK(suffixBeatsFilter.has_value());
  CHECK(suffixBeatsFilter->defaultSuffix == QStringLiteral("jpg"));

  const auto fallback = qco::ui::exportImageFormatForPathOrFilter(
    syntheticFormats,
    QStringLiteral("/tmp/export.unknown"),
    QString());
  CHECK(fallback.has_value());
  CHECK(fallback->defaultSuffix == QStringLiteral("png"));

  QImage transparentImage(QSize(2, 2), QImage::Format_ARGB32_Premultiplied);
  transparentImage.fill(Qt::transparent);
  transparentImage.setPixelColor(1, 1, QColor(32, 64, 128, 255));

  const auto* jpegFormat = findFormat(syntheticFormats, QStringLiteral("jpg"));
  CHECK(jpegFormat != nullptr);
  const auto flattened = qco::ui::imageForExportFormat(transparentImage, *jpegFormat);
  CHECK(flattened.format() == QImage::Format_RGB32);
  CHECK(flattened.pixelColor(0, 0) == QColor(Qt::white));
  CHECK(flattened.pixelColor(1, 1) == QColor(32, 64, 128, 255));

  QTemporaryDir tempDir;
  CHECK(tempDir.isValid());

  const auto availableFormats = qco::ui::availableExportImageFormats();
  CHECK(findFormat(availableFormats, QStringLiteral("png")) != nullptr);
  CHECK(findFormat(availableFormats, QStringLiteral("jpg")) != nullptr);

  for (const auto& format : availableFormats) {
    const auto outputPath = tempDir.path() + QStringLiteral("/export.") + format.defaultSuffix;
    QString error;
    CHECK(qco::ui::writeExportImage(outputPath, transparentImage, format, &error));
    CHECK(error.isEmpty());
    CHECK(QFileInfo(outputPath).size() > 0);
  }

  return 0;
}
