#include "image/ImageCodec.h"

#include <QFileInfo>
#include <QImageReader>

namespace qco::image {

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

}  // namespace qco::image
