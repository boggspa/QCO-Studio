#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace qco::image {

struct DecodedImage {
  QImage image;
  QByteArray format;
};

[[nodiscard]] QStringList readableImageSuffixesForFormats(const QList<QByteArray>& supportedFormats);
[[nodiscard]] QString imageOpenFileFilterForSuffixes(const QStringList& suffixes);
[[nodiscard]] QString availableImageOpenFileFilter();

class ImageCodec {
public:
  virtual ~ImageCodec() = default;

  [[nodiscard]] virtual std::optional<DecodedImage> read(
    const QString& filePath,
    QString* errorMessage = nullptr) const = 0;
};

class QtImageCodec final : public ImageCodec {
public:
  [[nodiscard]] std::optional<DecodedImage> read(
    const QString& filePath,
    QString* errorMessage = nullptr) const override;
};

}  // namespace qco::image
