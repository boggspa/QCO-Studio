#include "ui/ProjectArchive.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>

namespace qco::ui {
namespace {

struct ZipEntry {
  QString path;
  QByteArray data;
};

struct CentralDirectoryEntry {
  QByteArray name;
  quint32 crc = 0;
  quint32 size = 0;
  quint32 offset = 0;
};

[[nodiscard]] QString fromView(std::string_view value)
{
  return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] quint32 crc32(const QByteArray& bytes)
{
  static const auto table = [] {
    std::array<quint32, 256> values{};
    for (quint32 i = 0; i < values.size(); ++i) {
      quint32 crc = i;
      for (int bit = 0; bit < 8; ++bit) {
        crc = (crc & 1U) ? (0xEDB88320U ^ (crc >> 1U)) : (crc >> 1U);
      }
      values[i] = crc;
    }
    return values;
  }();

  quint32 crc = 0xFFFFFFFFU;
  for (const auto byte : bytes) {
    crc = table[(crc ^ static_cast<quint8>(byte)) & 0xFFU] ^ (crc >> 8U);
  }
  return crc ^ 0xFFFFFFFFU;
}

void writeUInt16(QIODevice& device, quint16 value)
{
  const char bytes[] = {
    static_cast<char>(value & 0xFFU),
    static_cast<char>((value >> 8U) & 0xFFU),
  };
  device.write(bytes, 2);
}

void writeUInt32(QIODevice& device, quint32 value)
{
  const char bytes[] = {
    static_cast<char>(value & 0xFFU),
    static_cast<char>((value >> 8U) & 0xFFU),
    static_cast<char>((value >> 16U) & 0xFFU),
    static_cast<char>((value >> 24U) & 0xFFU),
  };
  device.write(bytes, 4);
}

[[nodiscard]] bool writeStoredZip(const QString& filePath, const QVector<ZipEntry>& entries, QString* errorMessage)
{
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    if (errorMessage) {
      *errorMessage = file.errorString();
    }
    return false;
  }

  QVector<CentralDirectoryEntry> centralDirectory;
  centralDirectory.reserve(entries.size());

  for (const auto& entry : entries) {
    const auto name = entry.path.toUtf8();
    if (name.size() > std::numeric_limits<quint16>::max()
        || entry.data.size() > std::numeric_limits<quint32>::max()
        || file.pos() > std::numeric_limits<quint32>::max()) {
      if (errorMessage) {
        *errorMessage = QStringLiteral("Project package is too large for the Phase 1 ZIP writer.");
      }
      return false;
    }

    CentralDirectoryEntry central;
    central.name = name;
    central.crc = crc32(entry.data);
    central.size = static_cast<quint32>(entry.data.size());
    central.offset = static_cast<quint32>(file.pos());
    centralDirectory.push_back(central);

    writeUInt32(file, 0x04034B50U);
    writeUInt16(file, 20);
    writeUInt16(file, 0);
    writeUInt16(file, 0);
    writeUInt16(file, 0);
    writeUInt16(file, 0);
    writeUInt32(file, central.crc);
    writeUInt32(file, central.size);
    writeUInt32(file, central.size);
    writeUInt16(file, static_cast<quint16>(central.name.size()));
    writeUInt16(file, 0);
    file.write(central.name);
    file.write(entry.data);
  }

  if (file.pos() > std::numeric_limits<quint32>::max()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Project package is too large for the Phase 1 ZIP writer.");
    }
    return false;
  }

  const auto centralDirectoryOffset = static_cast<quint32>(file.pos());

  for (const auto& entry : centralDirectory) {
    writeUInt32(file, 0x02014B50U);
    writeUInt16(file, 20);
    writeUInt16(file, 20);
    writeUInt16(file, 0);
    writeUInt16(file, 0);
    writeUInt16(file, 0);
    writeUInt16(file, 0);
    writeUInt32(file, entry.crc);
    writeUInt32(file, entry.size);
    writeUInt32(file, entry.size);
    writeUInt16(file, static_cast<quint16>(entry.name.size()));
    writeUInt16(file, 0);
    writeUInt16(file, 0);
    writeUInt16(file, 0);
    writeUInt16(file, 0);
    writeUInt32(file, 0);
    writeUInt32(file, entry.offset);
    file.write(entry.name);
  }

  if (file.pos() > std::numeric_limits<quint32>::max()
      || centralDirectory.size() > std::numeric_limits<quint16>::max()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Project package is too large for the Phase 1 ZIP writer.");
    }
    return false;
  }

  const auto centralDirectorySize = static_cast<quint32>(file.pos() - centralDirectoryOffset);

  writeUInt32(file, 0x06054B50U);
  writeUInt16(file, 0);
  writeUInt16(file, 0);
  writeUInt16(file, static_cast<quint16>(centralDirectory.size()));
  writeUInt16(file, static_cast<quint16>(centralDirectory.size()));
  writeUInt32(file, centralDirectorySize);
  writeUInt32(file, centralDirectoryOffset);
  writeUInt16(file, 0);

  if (!file.commit()) {
    if (errorMessage) {
      *errorMessage = file.errorString();
    }
    return false;
  }

  return true;
}

[[nodiscard]] QByteArray toJson(const QJsonObject& object)
{
  return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

}  // namespace

bool ProjectArchive::save(
  const QString& filePath,
  const qco::core::Document& document,
  const QVector<ProjectRasterLayer>& rasterLayers,
  QString* errorMessage)
{
  QVector<ZipEntry> entries;

  QJsonObject manifest;
  manifest.insert(QStringLiteral("format"), QStringLiteral("app.qcostudio.document"));
  manifest.insert(QStringLiteral("formatVersion"), 1);
  manifest.insert(QStringLiteral("appVersion"), QCoreApplication::applicationVersion());
  manifest.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
  entries.push_back({QStringLiteral("manifest.json"), toJson(manifest)});

  QJsonObject documentJson;
  documentJson.insert(QStringLiteral("title"), QString::fromStdString(document.title()));
  documentJson.insert(QStringLiteral("width"), document.canvasSize().width);
  documentJson.insert(QStringLiteral("height"), document.canvasSize().height);

  QJsonArray layersJson;
  for (const auto& layer : document.layers()) {
    QJsonObject layerJson;
    layerJson.insert(QStringLiteral("id"), QString::number(layer.id));
    layerJson.insert(QStringLiteral("name"), QString::fromStdString(layer.name));
    layerJson.insert(QStringLiteral("type"), fromView(qco::core::toString(layer.type)));
    layerJson.insert(QStringLiteral("blendMode"), fromView(qco::core::toString(layer.blendMode)));
    layerJson.insert(QStringLiteral("visible"), layer.visible);
    layerJson.insert(QStringLiteral("locked"), layer.locked);
    layerJson.insert(QStringLiteral("opacity"), layer.opacity);
    layerJson.insert(QStringLiteral("x"), layer.position.x);
    layerJson.insert(QStringLiteral("y"), layer.position.y);
    layerJson.insert(QStringLiteral("width"), layer.size.width);
    layerJson.insert(QStringLiteral("height"), layer.size.height);

    const auto layerImage = std::find_if(
      rasterLayers.begin(),
      rasterLayers.end(),
      [id = layer.id](const ProjectRasterLayer& rasterLayer) {
        return rasterLayer.id == id;
      });

    if (layerImage != rasterLayers.end() && !layerImage->image.isNull()) {
      const auto imagePath = QStringLiteral("layers/%1.png").arg(QString::number(layer.id));
      layerJson.insert(QStringLiteral("image"), imagePath);

      QByteArray pngBytes;
      QBuffer buffer(&pngBytes);
      buffer.open(QIODevice::WriteOnly);
      layerImage->image.save(&buffer, "PNG");
      entries.push_back({imagePath, pngBytes});
    }

    layersJson.push_back(layerJson);
  }

  documentJson.insert(QStringLiteral("layers"), layersJson);
  entries.push_front({QStringLiteral("document.json"), toJson(documentJson)});

  return writeStoredZip(filePath, entries, errorMessage);
}

}  // namespace qco::ui
