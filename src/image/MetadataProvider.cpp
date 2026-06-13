#include "image/MetadataProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

namespace qco::image {
namespace {

constexpr auto sidecarSuffix = ".qco-meta.json";

void setError(QString* errorMessage, QString message)
{
  if (errorMessage != nullptr) {
    *errorMessage = std::move(message);
  }
}

void clearError(QString* errorMessage)
{
  if (errorMessage != nullptr) {
    errorMessage->clear();
  }
}

[[nodiscard]] QJsonObject metadataToJson(const qco::core::MetadataMap& metadata)
{
  QJsonObject metadataJson;
  for (const auto& [key, value] : metadata) {
    if (!key.empty()) {
      metadataJson.insert(QString::fromStdString(key), QString::fromStdString(value));
    }
  }
  return metadataJson;
}

[[nodiscard]] std::optional<qco::core::MetadataMap> metadataFromJson(
  const QJsonObject& metadataJson,
  QString* errorMessage)
{
  qco::core::MetadataMap metadata;
  for (auto it = metadataJson.constBegin(); it != metadataJson.constEnd(); ++it) {
    if (it.key().isEmpty() || !it.value().isString()) {
      setError(errorMessage, QStringLiteral("The metadata sidecar must contain string values with non-empty keys."));
      return std::nullopt;
    }
    metadata.emplace(it.key().toStdString(), it.value().toString().toStdString());
  }

  clearError(errorMessage);
  return metadata;
}

}  // namespace

QString metadataSidecarPathForImagePath(const QString& imagePath)
{
  if (imagePath.isEmpty()) {
    return {};
  }

  const QFileInfo imageInfo(imagePath);
  if (imageInfo.fileName().isEmpty()) {
    return {};
  }

  return imageInfo.absoluteDir().filePath(imageInfo.fileName() + QString::fromLatin1(sidecarSuffix));
}

std::optional<qco::core::MetadataMap> JsonSidecarMetadataProvider::readForImage(
  const QString& imagePath,
  QString* errorMessage) const
{
  const auto sidecarPath = metadataSidecarPathForImagePath(imagePath);
  if (sidecarPath.isEmpty()) {
    setError(errorMessage, QStringLiteral("The image path is empty."));
    return std::nullopt;
  }

  QFile file(sidecarPath);
  if (!file.exists()) {
    clearError(errorMessage);
    return qco::core::MetadataMap{};
  }
  if (!file.open(QIODevice::ReadOnly)) {
    setError(errorMessage, file.errorString());
    return std::nullopt;
  }

  const auto document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) {
    setError(errorMessage, QStringLiteral("The metadata sidecar is not a JSON object."));
    return std::nullopt;
  }

  return metadataFromJson(document.object(), errorMessage);
}

bool JsonSidecarMetadataProvider::writeForImage(
  const QString& imagePath,
  const qco::core::MetadataMap& metadata,
  QString* errorMessage) const
{
  const auto sidecarPath = metadataSidecarPathForImagePath(imagePath);
  if (sidecarPath.isEmpty()) {
    setError(errorMessage, QStringLiteral("The image path is empty."));
    return false;
  }

  QSaveFile file(sidecarPath);
  if (!file.open(QIODevice::WriteOnly)) {
    setError(errorMessage, file.errorString());
    return false;
  }

  const auto bytes = QJsonDocument(metadataToJson(metadata)).toJson(QJsonDocument::Indented);
  if (file.write(bytes) != bytes.size()) {
    setError(errorMessage, file.errorString());
    return false;
  }
  if (!file.commit()) {
    setError(errorMessage, file.errorString());
    return false;
  }

  clearError(errorMessage);
  return true;
}

}  // namespace qco::image
