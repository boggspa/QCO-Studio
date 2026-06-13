#include "image/ImageCodec.h"

#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QSet>

#include <algorithm>
#include <utility>

namespace qco::image {
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
  QVector<ImageWriteFormat>& formats,
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

QStringList readableImageSuffixesForFormats(const QList<QByteArray>& supportedFormats)
{
  QStringList suffixes;
  suffixes.reserve(supportedFormats.size());
  for (const auto& format : supportedFormats) {
    const auto suffix = QString::fromLatin1(format).toLower();
    if (!suffix.isEmpty()) {
      suffixes.push_back(suffix);
    }
  }
  suffixes.removeDuplicates();
  suffixes.sort();
  return suffixes;
}

QString imageOpenFileFilterForSuffixes(const QStringList& suffixes)
{
  QStringList patterns;
  patterns.reserve(suffixes.size());
  for (const auto& suffix : suffixes) {
    patterns.push_back(QStringLiteral("*.%1").arg(suffix));
  }
  return QStringLiteral("Images (%1);;All files (*)").arg(patterns.join(QLatin1Char(' ')));
}

QString availableImageOpenFileFilter()
{
  return imageOpenFileFilterForSuffixes(readableImageSuffixesForFormats(QImageReader::supportedImageFormats()));
}

QVector<ImageWriteFormat> writableImageFormatsForFormats(const QList<QByteArray>& supportedFormats)
{
  const auto formats = normalizedFormats(supportedFormats);

  QVector<ImageWriteFormat> writeFormats;
  if (formats.contains(QByteArrayLiteral("png"))) {
    appendFormat(writeFormats,
                 QStringLiteral("PNG Image"),
                 QStringLiteral("png"),
                 {QStringLiteral("png")},
                 QByteArrayLiteral("png"));
  }
  if (containsAny(formats, {QByteArrayLiteral("jpg"), QByteArrayLiteral("jpeg"), QByteArrayLiteral("jfif")})) {
    appendFormat(writeFormats,
                 QStringLiteral("JPEG Image"),
                 QStringLiteral("jpg"),
                 {QStringLiteral("jpg"), QStringLiteral("jpeg")},
                 QByteArrayLiteral("jpg"),
                 true);
  }
  if (containsAny(formats, {QByteArrayLiteral("tif"), QByteArrayLiteral("tiff")})) {
    appendFormat(writeFormats,
                 QStringLiteral("TIFF Image"),
                 QStringLiteral("tif"),
                 {QStringLiteral("tif"), QStringLiteral("tiff")},
                 QByteArrayLiteral("tiff"));
  }
  if (formats.contains(QByteArrayLiteral("webp"))) {
    appendFormat(writeFormats,
                 QStringLiteral("WebP Image"),
                 QStringLiteral("webp"),
                 {QStringLiteral("webp")},
                 QByteArrayLiteral("webp"));
  }

  return writeFormats;
}

QString imageWriteFileFilter(const QVector<ImageWriteFormat>& formats)
{
  QStringList filters;
  filters.reserve(formats.size());
  for (const auto& format : formats) {
    filters.push_back(QStringLiteral("%1 (%2)").arg(format.label, suffixPattern(format.suffixes)));
  }
  return filters.join(QStringLiteral(";;"));
}

std::optional<ImageWriteFormat> imageWriteFormatForPathOrFilter(
  const QVector<ImageWriteFormat>& formats,
  const QString& filePath,
  const QString& selectedFilter)
{
  const auto suffix = QFileInfo(filePath).suffix().toLower();
  const auto suffixIt = std::find_if(formats.begin(), formats.end(), [&suffix](const ImageWriteFormat& format) {
    return format.suffixes.contains(suffix, Qt::CaseInsensitive);
  });
  if (suffixIt != formats.end()) {
    return *suffixIt;
  }

  const auto filterIt = std::find_if(formats.begin(), formats.end(), [&selectedFilter](const ImageWriteFormat& format) {
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

QImage imageForWriteFormat(const QImage& image, const ImageWriteFormat& format)
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

std::optional<DecodedImage> QtImageCodec::read(const QString& filePath, QString* errorMessage) const
{
  if (filePath.isEmpty()) {
    if (errorMessage != nullptr) {
      *errorMessage = QStringLiteral("The image path is empty.");
    }
    return std::nullopt;
  }

  QImageReader reader(filePath);
  reader.setAutoTransform(true);
  const auto detectedFormat = reader.format().toLower();

  QImage image = reader.read();
  if (image.isNull()) {
    if (errorMessage != nullptr) {
      *errorMessage = reader.errorString();
    }
    return std::nullopt;
  }

  if (errorMessage != nullptr) {
    errorMessage->clear();
  }

  return DecodedImage{
    image.convertToFormat(QImage::Format_ARGB32_Premultiplied),
    detectedFormat.isEmpty() ? QFileInfo(filePath).suffix().toLatin1().toLower() : detectedFormat,
  };
}

QVector<ImageWriteFormat> QtImageCodec::writeFormats() const
{
  return writableImageFormatsForFormats(QImageWriter::supportedImageFormats());
}

bool QtImageCodec::write(
  const QString& filePath,
  const QImage& image,
  const ImageWriteFormat& format,
  QString* errorMessage) const
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

  if (!writer.write(imageForWriteFormat(image, format))) {
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

}  // namespace qco::image
