#include "image/MetadataProvider.h"

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

  QTemporaryDir tempDir;
  CHECK(tempDir.isValid());

  const auto imagePath = tempDir.filePath(QStringLiteral("source.png"));
  CHECK(writeBytes(imagePath, QByteArrayLiteral("not used by metadata provider")));

  const auto sidecarPath = qco::image::metadataSidecarPathForImagePath(imagePath);
  CHECK(sidecarPath.endsWith(QStringLiteral("source.png.qco-meta.json")));

  const qco::image::JsonSidecarMetadataProvider provider;
  QString error;

  const auto missingMetadata = provider.readForImage(imagePath, &error);
  CHECK(missingMetadata.has_value());
  CHECK(missingMetadata->empty());
  CHECK(error.isEmpty());

  qco::core::MetadataMap metadata;
  metadata["caption"] = "Sidecar caption";
  metadata["rating"] = "4";
  metadata["camera.make"] = "QCO Test Camera";
  CHECK(provider.writeForImage(imagePath, metadata, &error));
  CHECK(error.isEmpty());

  const auto loadedMetadata = provider.readForImage(imagePath, &error);
  CHECK(loadedMetadata.has_value());
  CHECK(error.isEmpty());
  CHECK(loadedMetadata->size() == 3);
  CHECK(loadedMetadata->at("caption") == "Sidecar caption");
  CHECK(loadedMetadata->at("rating") == "4");
  CHECK(loadedMetadata->at("camera.make") == "QCO Test Camera");

  CHECK(writeBytes(sidecarPath, QByteArrayLiteral("{\"rating\":5}")));
  CHECK(!provider.readForImage(imagePath, &error).has_value());
  CHECK(error.contains(QStringLiteral("string values")));

  CHECK(writeBytes(sidecarPath, QByteArrayLiteral("{\"\":\"empty key\"}")));
  CHECK(!provider.readForImage(imagePath, &error).has_value());
  CHECK(error.contains(QStringLiteral("non-empty keys")));

  CHECK(writeBytes(sidecarPath, QByteArrayLiteral("[]")));
  CHECK(!provider.readForImage(imagePath, &error).has_value());
  CHECK(error.contains(QStringLiteral("JSON object")));

  CHECK(!provider.readForImage(QString(), &error).has_value());
  CHECK(error.contains(QStringLiteral("empty")));
  CHECK(!provider.writeForImage(QString(), metadata, &error));
  CHECK(error.contains(QStringLiteral("empty")));

  return 0;
}
