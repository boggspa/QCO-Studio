#include "image/ColorTransform.h"

namespace qco::image {

QColorSpace colorSpaceForProfile(ColorProfile profile)
{
  switch (profile) {
    case ColorProfile::SRgb:
      return QColorSpace(QColorSpace::SRgb);
    case ColorProfile::LinearSRgb:
      return QColorSpace(QColorSpace::SRgbLinear);
    case ColorProfile::DisplayP3:
      return QColorSpace(QColorSpace::DisplayP3);
    case ColorProfile::AdobeRgb:
      return QColorSpace(QColorSpace::AdobeRgb);
    case ColorProfile::ProPhotoRgb:
      return QColorSpace(QColorSpace::ProPhotoRgb);
    case ColorProfile::Bt2020:
      return QColorSpace(QColorSpace::Bt2020);
  }
  return QColorSpace(QColorSpace::SRgb);
}

QString colorProfileName(ColorProfile profile)
{
  switch (profile) {
    case ColorProfile::SRgb:
      return QStringLiteral("sRGB");
    case ColorProfile::LinearSRgb:
      return QStringLiteral("Linear sRGB");
    case ColorProfile::DisplayP3:
      return QStringLiteral("Display P3");
    case ColorProfile::AdobeRgb:
      return QStringLiteral("Adobe RGB");
    case ColorProfile::ProPhotoRgb:
      return QStringLiteral("ProPhoto RGB");
    case ColorProfile::Bt2020:
      return QStringLiteral("BT.2020");
  }
  return QStringLiteral("sRGB");
}

QImage imageTaggedWithProfile(const QImage& image, ColorProfile profile)
{
  if (image.isNull()) {
    return {};
  }

  QImage tagged = image;
  tagged.setColorSpace(colorSpaceForProfile(profile));
  return tagged;
}

std::optional<QImage> QtColorTransform::convert(
  const QImage& image,
  const ColorTransformOptions& options,
  QString* errorMessage) const
{
  if (image.isNull()) {
    if (errorMessage != nullptr) {
      *errorMessage = QStringLiteral("The image data is empty.");
    }
    return std::nullopt;
  }

  QImage source = image;
  if (!source.colorSpace().isValid()) {
    source.setColorSpace(colorSpaceForProfile(options.assumedSourceProfile));
  }

  const auto targetColorSpace = colorSpaceForProfile(options.targetProfile);
  if (!targetColorSpace.isValidTarget()) {
    if (errorMessage != nullptr) {
      *errorMessage = QStringLiteral("The target color profile is not valid for conversion.");
    }
    return std::nullopt;
  }

  QImage converted = source.convertedToColorSpace(targetColorSpace, options.targetFormat);
  if (converted.isNull()) {
    if (errorMessage != nullptr) {
      *errorMessage = QStringLiteral("The image could not be converted to %1.").arg(colorProfileName(options.targetProfile));
    }
    return std::nullopt;
  }

  if (!converted.colorSpace().isValid()) {
    converted.setColorSpace(targetColorSpace);
  }

  if (errorMessage != nullptr) {
    errorMessage->clear();
  }
  return converted;
}

}  // namespace qco::image
