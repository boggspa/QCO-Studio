#include "image/ColorTransform.h"

#include <QColor>
#include <QCoreApplication>
#include <QImage>

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

[[nodiscard]] QImage sampleImage()
{
  QImage image(QSize(2, 2), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  image.setPixelColor(0, 0, QColor(16, 64, 128, 255));
  image.setPixelColor(1, 0, QColor(220, 20, 48, 255));
  return image;
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

  CHECK(qco::image::colorSpaceForProfile(qco::image::ColorProfile::SRgb).isValid());
  CHECK(qco::image::colorSpaceForProfile(qco::image::ColorProfile::DisplayP3).isValid());
  CHECK(qco::image::colorProfileName(qco::image::ColorProfile::AdobeRgb) == QStringLiteral("Adobe RGB"));

  const auto untagged = sampleImage();
  CHECK(!untagged.colorSpace().isValid());

  const auto tagged = qco::image::imageTaggedWithProfile(untagged, qco::image::ColorProfile::SRgb);
  CHECK(tagged.colorSpace().isValid());
  CHECK(tagged.colorSpace() == qco::image::colorSpaceForProfile(qco::image::ColorProfile::SRgb));
  CHECK(tagged.pixelColor(0, 0) == QColor(16, 64, 128, 255));

  const qco::image::QtColorTransform transform;
  QString error;

  const auto srgb = transform.convert(
    tagged,
    {qco::image::ColorProfile::SRgb, QImage::Format_ARGB32_Premultiplied, qco::image::ColorProfile::SRgb},
    &error);
  CHECK(srgb.has_value());
  CHECK(error.isEmpty());
  CHECK(srgb->size() == tagged.size());
  CHECK(srgb->format() == QImage::Format_ARGB32_Premultiplied);
  CHECK(srgb->colorSpace() == qco::image::colorSpaceForProfile(qco::image::ColorProfile::SRgb));
  CHECK(srgb->pixelColor(0, 0) == QColor(16, 64, 128, 255));

  const auto displayP3 = transform.convert(
    tagged,
    {qco::image::ColorProfile::DisplayP3, QImage::Format_RGBA8888, qco::image::ColorProfile::SRgb},
    &error);
  CHECK(displayP3.has_value());
  CHECK(error.isEmpty());
  CHECK(displayP3->size() == tagged.size());
  CHECK(displayP3->format() == QImage::Format_RGBA8888);
  CHECK(displayP3->colorSpace() == qco::image::colorSpaceForProfile(qco::image::ColorProfile::DisplayP3));

  const auto assumedSrgb = transform.convert(
    untagged,
    {qco::image::ColorProfile::SRgb, QImage::Format_ARGB32_Premultiplied, qco::image::ColorProfile::SRgb},
    &error);
  CHECK(assumedSrgb.has_value());
  CHECK(assumedSrgb->colorSpace() == qco::image::colorSpaceForProfile(qco::image::ColorProfile::SRgb));
  CHECK(assumedSrgb->pixelColor(1, 0) == QColor(220, 20, 48, 255));

  CHECK(!transform.convert(QImage(), {}, &error).has_value());
  CHECK(!error.isEmpty());

  return 0;
}
