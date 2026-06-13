#pragma once

#include "core/Document.h"

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QStringList>

#include <optional>

namespace qco::image {

struct RawImage {
  QImage image;
  qco::core::MetadataMap metadata;
  QByteArray format;
};

[[nodiscard]] QStringList rawImageSuffixes();
[[nodiscard]] QString rawImageOpenFileFilter();
[[nodiscard]] std::optional<QByteArray> rawFormatForPath(const QString& filePath);

class RawImageProvider {
public:
  virtual ~RawImageProvider() = default;

  [[nodiscard]] virtual bool canRead(const QString& filePath) const = 0;
  [[nodiscard]] virtual std::optional<RawImage> read(
    const QString& filePath,
    QString* errorMessage = nullptr) const = 0;
};

class RawDecodeUnavailableProvider final : public RawImageProvider {
public:
  [[nodiscard]] bool canRead(const QString& filePath) const override;
  [[nodiscard]] std::optional<RawImage> read(
    const QString& filePath,
    QString* errorMessage = nullptr) const override;
};

}  // namespace qco::image
