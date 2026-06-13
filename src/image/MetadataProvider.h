#pragma once

#include "core/Document.h"

#include <QString>

#include <optional>

namespace qco::image {

[[nodiscard]] QString metadataSidecarPathForImagePath(const QString& imagePath);

class ImageMetadataProvider {
public:
  virtual ~ImageMetadataProvider() = default;

  [[nodiscard]] virtual std::optional<qco::core::MetadataMap> readForImage(
    const QString& imagePath,
    QString* errorMessage = nullptr) const = 0;

  [[nodiscard]] virtual bool writeForImage(
    const QString& imagePath,
    const qco::core::MetadataMap& metadata,
    QString* errorMessage = nullptr) const = 0;
};

class JsonSidecarMetadataProvider final : public ImageMetadataProvider {
public:
  [[nodiscard]] std::optional<qco::core::MetadataMap> readForImage(
    const QString& imagePath,
    QString* errorMessage = nullptr) const override;

  [[nodiscard]] bool writeForImage(
    const QString& imagePath,
    const qco::core::MetadataMap& metadata,
    QString* errorMessage = nullptr) const override;
};

}  // namespace qco::image
