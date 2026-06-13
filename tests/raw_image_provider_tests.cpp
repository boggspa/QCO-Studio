#include "image/RawImageProvider.h"

#include <QCoreApplication>
#include <QFile>
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

[[nodiscard]] bool writeBytes(const QString& filePath, const QByteArray& bytes)
{
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  return file.write(bytes) == bytes.size();
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

  const auto suffixes = qco::image::rawImageSuffixes();
  CHECK(suffixes.contains(QStringLiteral("arw")));
  CHECK(suffixes.contains(QStringLiteral("cr2")));
  CHECK(suffixes.contains(QStringLiteral("cr3")));
  CHECK(suffixes.contains(QStringLiteral("dng")));
  CHECK(suffixes.contains(QStringLiteral("nef")));
  CHECK(suffixes.contains(QStringLiteral("rw2")));
  CHECK(suffixes == qco::image::rawImageSuffixes());

  const auto filter = qco::image::rawImageOpenFileFilter();
  CHECK(filter.contains(QStringLiteral("Camera RAW (")));
  CHECK(filter.contains(QStringLiteral("*.dng")));
  CHECK(filter.contains(QStringLiteral("*.cr3")));
  CHECK(filter.contains(QStringLiteral("All files (*)")));

  CHECK(qco::image::rawFormatForPath(QStringLiteral("/tmp/photo.DNG")).value() == QByteArrayLiteral("dng"));
  CHECK(qco::image::rawFormatForPath(QStringLiteral("/tmp/photo.nef")).value() == QByteArrayLiteral("nef"));
  CHECK(!qco::image::rawFormatForPath(QStringLiteral("/tmp/photo.png")).has_value());

  const qco::image::RawDecodeUnavailableProvider provider;
  CHECK(provider.canRead(QStringLiteral("/tmp/photo.CR2")));
  CHECK(!provider.canRead(QStringLiteral("/tmp/photo.jpg")));

  QString error;
  CHECK(!provider.read(QString(), &error).has_value());
  CHECK(error.contains(QStringLiteral("empty")));

  QTemporaryDir tempDir;
  CHECK(tempDir.isValid());

  const auto nonRawPath = tempDir.filePath(QStringLiteral("not-raw.txt"));
  CHECK(writeBytes(nonRawPath, QByteArrayLiteral("not raw")));
  CHECK(!provider.read(nonRawPath, &error).has_value());
  CHECK(error.contains(QStringLiteral("supported camera RAW")));

  const auto emptyRawPath = tempDir.filePath(QStringLiteral("empty.dng"));
  CHECK(writeBytes(emptyRawPath, QByteArray()));
  CHECK(!provider.read(emptyRawPath, &error).has_value());
  CHECK(error.contains(QStringLiteral("empty")));

  const auto dngPath = tempDir.filePath(QStringLiteral("sample.DNG"));
  CHECK(writeBytes(dngPath, QByteArray::fromHex("49492a0008000000")));
  CHECK(!provider.read(dngPath, &error).has_value());
  CHECK(error.contains(QStringLiteral("LibRaw-backed RawImageProvider")));
  CHECK(error.contains(QStringLiteral("DNG")));

  return 0;
}
