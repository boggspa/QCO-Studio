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

  QTemporaryDir tempDir;
  CHECK(tempDir.isValid());

  const auto sourcePath = tempDir.filePath(QStringLiteral("source.png"));
  CHECK(solidImage(QSize(3, 2), QColor(16, 64, 128, 255)).save(sourcePath));

  const qco::image::QtImageCodec codec;
  QString error;
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

  return 0;
}
