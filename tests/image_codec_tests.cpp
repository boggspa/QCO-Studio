#include "image/ImageCodec.h"

#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

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

[[nodiscard]] QImage solidImage(QSize size, QColor color)
{
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(color);
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

  const auto suffixes = qco::image::readableImageSuffixesForFormats({
    QByteArrayLiteral("PNG"),
    QByteArrayLiteral("jpg"),
    QByteArrayLiteral("png"),
    QByteArrayLiteral("JPEG"),
  });
  CHECK(suffixes == QStringList({QStringLiteral("jpeg"), QStringLiteral("jpg"), QStringLiteral("png")}));

  const auto filter = qco::image::imageOpenFileFilterForSuffixes(suffixes);
  CHECK(filter == QStringLiteral("Images (*.jpeg *.jpg *.png);;All files (*)"));
  CHECK(qco::image::availableImageOpenFileFilter().contains(QStringLiteral("Images (")));
  CHECK(qco::image::availableImageOpenFileFilter().contains(QStringLiteral("All files (*)")));

  const auto writeFormats = qco::image::writableImageFormatsForFormats({
    QByteArrayLiteral("png"),
    QByteArrayLiteral("jpg"),
    QByteArrayLiteral("tiff"),
    QByteArrayLiteral("webp"),
  });
  CHECK(writeFormats.size() == 4);
  CHECK(writeFormats[0].defaultSuffix == QStringLiteral("png"));
  CHECK(writeFormats[1].defaultSuffix == QStringLiteral("jpg"));
  CHECK(writeFormats[1].flattenToWhite);

  const auto writeFilter = qco::image::imageWriteFileFilter(writeFormats);
  CHECK(writeFilter.contains(QStringLiteral("PNG Image (*.png")));
  CHECK(writeFilter.contains(QStringLiteral("JPEG Image (*.jpg *.jpeg")));

  const auto suffixFormat = qco::image::imageWriteFormatForPathOrFilter(
    writeFormats,
    QStringLiteral("/tmp/export.jpeg"),
    QStringLiteral("PNG Image (*.png)"));
  CHECK(suffixFormat.has_value());
  CHECK(suffixFormat->defaultSuffix == QStringLiteral("jpg"));

  const auto filterFormat = qco::image::imageWriteFormatForPathOrFilter(
    writeFormats,
    QStringLiteral("/tmp/export"),
    QStringLiteral("WebP Image (*.webp)"));
  CHECK(filterFormat.has_value());
  CHECK(filterFormat->defaultSuffix == QStringLiteral("webp"));

  QTemporaryDir tempDir;
  CHECK(tempDir.isValid());

  const auto sourcePath = tempDir.filePath(QStringLiteral("source.png"));
  CHECK(solidImage(QSize(3, 2), QColor(16, 64, 128, 255)).save(sourcePath));

  const qco::image::QtImageCodec codec;
  QString error;
  CHECK(!codec.writeFormats().isEmpty());

  const auto decoded = codec.read(sourcePath, &error);
  CHECK(decoded.has_value());
  CHECK(error.isEmpty());
  CHECK(!decoded->image.isNull());
  CHECK(decoded->image.size() == QSize(3, 2));
  CHECK(decoded->image.format() == QImage::Format_ARGB32_Premultiplied);
  CHECK(decoded->image.pixelColor(1, 1) == QColor(16, 64, 128, 255));
  CHECK(decoded->format == QByteArrayLiteral("png"));

  CHECK(!codec.read(QString(), &error).has_value());
  CHECK(!error.isEmpty());

  const auto invalidPath = tempDir.filePath(QStringLiteral("not-an-image.txt"));
  QFile invalidFile(invalidPath);
  CHECK(invalidFile.open(QIODevice::WriteOnly));
  CHECK(invalidFile.write("not image data") > 0);
  invalidFile.close();

  CHECK(!codec.read(invalidPath, &error).has_value());
  CHECK(!error.isEmpty());

  QImage transparentImage(QSize(2, 2), QImage::Format_ARGB32_Premultiplied);
  transparentImage.fill(Qt::transparent);
  transparentImage.setPixelColor(1, 1, QColor(16, 64, 128, 255));

  const auto flattened = qco::image::imageForWriteFormat(transparentImage, writeFormats[1]);
  CHECK(flattened.format() == QImage::Format_RGB32);
  CHECK(flattened.pixelColor(0, 0) == QColor(Qt::white));
  CHECK(flattened.pixelColor(1, 1) == QColor(16, 64, 128, 255));

  const auto exportPath = tempDir.filePath(QStringLiteral("codec-export.png"));
  CHECK(codec.write(exportPath, transparentImage, writeFormats[0], &error));
  CHECK(error.isEmpty());
  const auto roundTrip = codec.read(exportPath, &error);
  CHECK(roundTrip.has_value());
  CHECK(roundTrip->image.size() == QSize(2, 2));
  CHECK(roundTrip->image.pixelColor(1, 1) == QColor(16, 64, 128, 255));

  CHECK(!codec.write(QString(), transparentImage, writeFormats[0], &error));
  CHECK(!error.isEmpty());

  return 0;
}
