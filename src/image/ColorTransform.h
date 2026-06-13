#pragma once

#include <QColorSpace>
#include <QImage>
#include <QString>

#include <optional>

namespace qco::image {

enum class ColorProfile {
  SRgb,
  LinearSRgb,
  DisplayP3,
  AdobeRgb,
  ProPhotoRgb,
  Bt2020
};

struct ColorTransformOptions {
  ColorProfile targetProfile = ColorProfile::SRgb;
  QImage::Format targetFormat = QImage::Format_ARGB32_Premultiplied;
  ColorProfile assumedSourceProfile = ColorProfile::SRgb;
};

[[nodiscard]] QColorSpace colorSpaceForProfile(ColorProfile profile);
[[nodiscard]] QString colorProfileName(ColorProfile profile);
[[nodiscard]] QImage imageTaggedWithProfile(const QImage& image, ColorProfile profile);

class ColorTransform {
public:
  virtual ~ColorTransform() = default;

  [[nodiscard]] virtual std::optional<QImage> convert(
    const QImage& image,
    const ColorTransformOptions& options,
    QString* errorMessage = nullptr) const = 0;
};

class QtColorTransform final : public ColorTransform {
public:
  [[nodiscard]] std::optional<QImage> convert(
    const QImage& image,
    const ColorTransformOptions& options,
    QString* errorMessage = nullptr) const override;
};

}  // namespace qco::image
