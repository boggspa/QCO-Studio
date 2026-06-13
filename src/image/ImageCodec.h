#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace qco::image {

struct DecodedImage {
  QImage image;
  QByteArray format;
};

struct ImageWriteFormat {
  QString label;
  QString defaultSuffix;
  QStringList suffixes;
  QByteArray writerFormat;
  bool flattenToWhite = false;
};

[[nodiscard]] QStringList readableImageSuffixesForFormats(const QList<QByteArray>& supportedFormats);
[[nodiscard]] QString imageOpenFileFilterForSuffixes(const QStringList& suffixes);
[[nodiscard]] QString availableImageOpenFileFilter();
[[nodiscard]] QVector<ImageWriteFormat> writableImageFormatsForFormats(const QList<QByteArray>& supportedFormats);
[[nodiscard]] QString imageWriteFileFilter(const QVector<ImageWriteFormat>& formats);
[[nodiscard]] std::optional<ImageWriteFormat> imageWriteFormatForPathOrFilter(
  const QVector<ImageWriteFormat>& formats,
  const QString& filePath,
  const QString& selectedFilter);
[[nodiscard]] QImage imageForWriteFormat(const QImage& image, const ImageWriteFormat& format);

class ImageCodec {
public:
  virtual ~ImageCodec() = default;

  [[nodiscard]] virtual std::optional<DecodedImage> read(
    const QString& filePath,
    QString* errorMessage = nullptr) const = 0;
  [[nodiscard]] virtual QVector<ImageWriteFormat> writeFormats() const = 0;
  [[nodiscard]] virtual bool write(
    const QString& filePath,
    const QImage& image,
    const ImageWriteFormat& format,
    QString* errorMessage = nullptr) const = 0;
};

class QtImageCodec final : public ImageCodec {
public:
  [[nodiscard]] std::optional<DecodedImage> read(
    const QString& filePath,
    QString* errorMessage = nullptr) const override;
  [[nodiscard]] QVector<ImageWriteFormat> writeFormats() const override;
  [[nodiscard]] bool write(
    const QString& filePath,
    const QImage& image,
    const ImageWriteFormat& format,
    QString* errorMessage = nullptr) const override;
};

}  // namespace qco::image
