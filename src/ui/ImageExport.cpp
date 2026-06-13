#include "ui/ImageExport.h"

#include <QFileInfo>
#include <QImageWriter>
#include <QPainter>
#include <QSet>

#include <algorithm>
#include <utility>

namespace qco::ui {
namespace {

[[nodiscard]] QSet<QByteArray> normalizedFormats(const QList<QByteArray>& supportedFormats)
{
  QSet<QByteArray> formats;
  for (const auto& format : supportedFormats) {
    formats.insert(format.toLower());
  }
  return formats;
}

[[nodiscard]] bool containsAny(const QSet<QByteArray>& supportedFormats, std::initializer_list<QByteArray> formats)
{
  return std::any_of(formats.begin(), formats.end(), [&supportedFormats](const QByteArray& format) {
    return supportedFormats.contains(format);
  });
}

void appendFormat(
  QVector<ExportImageFormat>& formats,
  QString label,
  QString defaultSuffix,
  QStringList suffixes,
  QByteArray writerFormat,
  bool flattenToWhite = false)
{
  formats.push_back({
    std::move(label),
    std::move(defaultSuffix),
    std::move(suffixes),
    std::move(writerFormat),
    flattenToWhite,
  });
}

[[nodiscard]] QString suffixPattern(const QStringList& suffixes)
{
  QStringList patterns;
  patterns.reserve(suffixes.size());
  for (const auto& suffix : suffixes) {
    patterns.push_back(QStringLiteral("*.%1").arg(suffix));
  }
  return patterns.join(QLatin1Char(' '));
}

}  // namespace

QVector<ExportImageFormat> exportImageFormatsForWriters(const QList<QByteArray>& supportedFormats)
{
  const auto formats = normalizedFormats(supportedFormats);

  QVector<ExportImageFormat> exportFormats;
  if (formats.contains(QByteArrayLiteral("png"))) {
    appendFormat(exportFormats,
                 QStringLiteral("PNG Image"),
                 QStringLiteral("png"),
                 {QStringLiteral("png")},
                 QByteArrayLiteral("png"));
  }
  if (containsAny(formats, {QByteArrayLiteral("jpg"), QByteArrayLiteral("jpeg"), QByteArrayLiteral("jfif")})) {
    appendFormat(exportFormats,
                 QStringLiteral("JPEG Image"),
                 QStringLiteral("jpg"),
                 {QStringLiteral("jpg"), QStringLiteral("jpeg")},
                 QByteArrayLiteral("jpg"),
                 true);
  }
  if (containsAny(formats, {QByteArrayLiteral("tif"), QByteArrayLiteral("tiff")})) {
    appendFormat(exportFormats,
                 QStringLiteral("TIFF Image"),
                 QStringLiteral("tif"),
                 {QStringLiteral("tif"), QStringLiteral("tiff")},
                 QByteArrayLiteral("tiff"));
  }
  if (formats.contains(QByteArrayLiteral("webp"))) {
    appendFormat(exportFormats,
                 QStringLiteral("WebP Image"),
                 QStringLiteral("webp"),
                 {QStringLiteral("webp")},
                 QByteArrayLiteral("webp"));
  }

  return exportFormats;
}

QVector<ExportImageFormat> availableExportImageFormats()
{
  return exportImageFormatsForWriters(QImageWriter::supportedImageFormats());
}

QString exportImageFileFilter(const QVector<ExportImageFormat>& formats)
{
  QStringList filters;
  filters.reserve(formats.size());
  for (const auto& format : formats) {
    filters.push_back(QStringLiteral("%1 (%2)").arg(format.label, suffixPattern(format.suffixes)));
  }
  return filters.join(QStringLiteral(";;"));
}

std::optional<ExportImageFormat> exportImageFormatForPathOrFilter(
  const QVector<ExportImageFormat>& formats,
  const QString& filePath,
  const QString& selectedFilter)
{
  const auto suffix = QFileInfo(filePath).suffix().toLower();
  const auto suffixIt = std::find_if(formats.begin(), formats.end(), [&suffix](const ExportImageFormat& format) {
    return format.suffixes.contains(suffix, Qt::CaseInsensitive);
  });
  if (suffixIt != formats.end()) {
    return *suffixIt;
  }

  const auto filterIt = std::find_if(formats.begin(), formats.end(), [&selectedFilter](const ExportImageFormat& format) {
    return !selectedFilter.isEmpty() && selectedFilter.contains(format.label, Qt::CaseInsensitive);
  });
  if (filterIt != formats.end()) {
    return *filterIt;
  }

  if (!formats.isEmpty()) {
    return formats.front();
  }

  return std::nullopt;
}

QImage imageForExportFormat(const QImage& image, const ExportImageFormat& format)
{
  if (!format.flattenToWhite) {
    return image;
  }

  QImage flattened(image.size(), QImage::Format_RGB32);
  flattened.fill(Qt::white);

  QPainter painter(&flattened);
  painter.drawImage(QPoint(0, 0), image);
  painter.end();
  return flattened;
}

bool writeExportImage(
  const QString& filePath,
  const QImage& image,
  const ExportImageFormat& format,
  QString* errorMessage)
{
  if (filePath.isEmpty() || image.isNull()) {
    if (errorMessage != nullptr) {
      *errorMessage = QStringLiteral("The export path or image data is empty.");
    }
    return false;
  }

  QImageWriter writer(filePath, format.writerFormat);
  if (format.writerFormat == QByteArrayLiteral("jpg") || format.writerFormat == QByteArrayLiteral("webp")) {
    writer.setQuality(95);
  }

  if (!writer.write(imageForExportFormat(image, format))) {
    if (errorMessage != nullptr) {
      *errorMessage = writer.errorString();
    }
    return false;
  }

  if (errorMessage != nullptr) {
    errorMessage->clear();
  }
  return true;
}

}  // namespace qco::ui
