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
#include <cstring>
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

[[nodiscard]] quint16 readUInt16(const QByteArray& bytes, qsizetype offset)
{
  return static_cast<quint16>(static_cast<quint8>(bytes[offset]))
         | static_cast<quint16>(static_cast<quint8>(bytes[offset + 1]) << 8U);
}

[[nodiscard]] quint32 readUInt32(const QByteArray& bytes, qsizetype offset)
{
  return static_cast<quint32>(static_cast<quint8>(bytes[offset]))
         | (static_cast<quint32>(static_cast<quint8>(bytes[offset + 1])) << 8U)
         | (static_cast<quint32>(static_cast<quint8>(bytes[offset + 2])) << 16U)
         | (static_cast<quint32>(static_cast<quint8>(bytes[offset + 3])) << 24U);
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

[[nodiscard]] std::optional<QVector<ZipEntry>> readStoredZip(const QString& filePath, QString* errorMessage)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    if (errorMessage) {
      *errorMessage = file.errorString();
    }
    return std::nullopt;
  }

  const auto bytes = file.readAll();
  QVector<ZipEntry> entries;
  qsizetype offset = 0;

  while (offset + 30 <= bytes.size()) {
    const auto signature = readUInt32(bytes, offset);
    if (signature == 0x02014B50U || signature == 0x06054B50U) {
      break;
    }
    if (signature != 0x04034B50U) {
      if (errorMessage) {
        *errorMessage = QStringLiteral("The project package is not a supported QCO Studio document.");
      }
      return std::nullopt;
    }

    const auto flags = readUInt16(bytes, offset + 6);
    const auto method = readUInt16(bytes, offset + 8);
    const auto compressedSize = readUInt32(bytes, offset + 18);
    const auto uncompressedSize = readUInt32(bytes, offset + 22);
    const auto nameLength = readUInt16(bytes, offset + 26);
    const auto extraLength = readUInt16(bytes, offset + 28);

    if ((flags & 0x0008U) != 0U || method != 0U || compressedSize != uncompressedSize) {
      if (errorMessage) {
        *errorMessage = QStringLiteral("This project package uses ZIP features not supported by the Phase 1 reader.");
      }
      return std::nullopt;
    }

    const auto nameOffset = offset + 30;
    const auto dataOffset = nameOffset + nameLength + extraLength;
    const auto nextOffset = dataOffset + static_cast<qsizetype>(compressedSize);
    if (dataOffset < nameOffset || nextOffset < dataOffset || nextOffset > bytes.size()) {
      if (errorMessage) {
        *errorMessage = QStringLiteral("The project package is truncated or corrupt.");
      }
      return std::nullopt;
    }

    const auto name = QString::fromUtf8(bytes.mid(nameOffset, nameLength));
    const auto data = bytes.mid(dataOffset, static_cast<qsizetype>(compressedSize));
    if (crc32(data) != readUInt32(bytes, offset + 14)) {
      if (errorMessage) {
        *errorMessage = QStringLiteral("The project package failed its integrity check.");
      }
      return std::nullopt;
    }

    entries.push_back({name, data});
    offset = nextOffset;
  }

  return entries;
}

[[nodiscard]] std::optional<QByteArray> entryData(const QVector<ZipEntry>& entries, const QString& path)
{
  const auto it = std::find_if(entries.begin(), entries.end(), [&path](const ZipEntry& entry) {
    return entry.path == path;
  });
  if (it == entries.end()) {
    return std::nullopt;
  }
  return it->data;
}

[[nodiscard]] std::uint64_t layerIdFromJson(const QJsonObject& object)
{
  const auto stringValue = object.value(QStringLiteral("id")).toString();
  bool ok = false;
  const auto id = stringValue.toULongLong(&ok);
  return ok ? id : 0;
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

std::optional<ProjectDocument> ProjectArchive::load(const QString& filePath, QString* errorMessage)
{
  const auto entries = readStoredZip(filePath, errorMessage);
  if (!entries.has_value()) {
    return std::nullopt;
  }

  const auto manifestBytes = entryData(*entries, QStringLiteral("manifest.json"));
  const auto documentBytes = entryData(*entries, QStringLiteral("document.json"));
  if (!manifestBytes.has_value() || !documentBytes.has_value()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("The project package is missing required metadata.");
    }
    return std::nullopt;
  }

  const auto manifest = QJsonDocument::fromJson(*manifestBytes);
  if (!manifest.isObject()
      || manifest.object().value(QStringLiteral("format")).toString() != QStringLiteral("app.qcostudio.document")) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("The selected file is not a QCO Studio document.");
    }
    return std::nullopt;
  }

  const auto documentJson = QJsonDocument::fromJson(*documentBytes);
  if (!documentJson.isObject()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("The project document metadata is invalid.");
    }
    return std::nullopt;
  }

  const auto root = documentJson.object();
  auto document = qco::core::Document::create(
    root.value(QStringLiteral("title")).toString(QStringLiteral("Untitled")).toStdString(),
    {
      root.value(QStringLiteral("width")).toInt(1),
      root.value(QStringLiteral("height")).toInt(1),
    });

  QVector<ProjectRasterLayer> rasterLayers;
  const auto layersJson = root.value(QStringLiteral("layers")).toArray();
  rasterLayers.reserve(layersJson.size());

  for (const auto& layerValue : layersJson) {
    if (!layerValue.isObject()) {
      continue;
    }

    const auto layerJson = layerValue.toObject();
    qco::core::Layer layer;
    layer.id = layerIdFromJson(layerJson);
    layer.name = layerJson.value(QStringLiteral("name")).toString(QStringLiteral("Layer")).toStdString();
    layer.type = qco::core::layerTypeFromString(layerJson.value(QStringLiteral("type")).toString().toStdString());
    layer.blendMode =
      qco::core::blendModeFromString(layerJson.value(QStringLiteral("blendMode")).toString().toStdString());
    layer.visible = layerJson.value(QStringLiteral("visible")).toBool(true);
    layer.locked = layerJson.value(QStringLiteral("locked")).toBool(false);
    layer.opacity = layerJson.value(QStringLiteral("opacity")).toDouble(1.0);
    layer.position = {
      layerJson.value(QStringLiteral("x")).toInt(0),
      layerJson.value(QStringLiteral("y")).toInt(0),
    };
    layer.size = {
      layerJson.value(QStringLiteral("width")).toInt(document.canvasSize().width),
      layerJson.value(QStringLiteral("height")).toInt(document.canvasSize().height),
    };

    if (layer.id == 0 || !document.addLayer(layer)) {
      if (errorMessage) {
        *errorMessage = QStringLiteral("The project document contains invalid layer metadata.");
      }
      return std::nullopt;
    }

    const auto imagePath = layerJson.value(QStringLiteral("image")).toString();
    if (!imagePath.isEmpty()) {
      const auto imageBytes = entryData(*entries, imagePath);
      if (!imageBytes.has_value()) {
        if (errorMessage) {
          *errorMessage = QStringLiteral("The project package is missing layer image data.");
        }
        return std::nullopt;
      }

      QImage image;
      if (!image.loadFromData(*imageBytes, "PNG")) {
        if (errorMessage) {
          *errorMessage = QStringLiteral("A layer image in the project package could not be decoded.");
        }
        return std::nullopt;
      }

      rasterLayers.push_back({
        layer.id,
        QString::fromStdString(layer.name),
        image.convertToFormat(QImage::Format_ARGB32_Premultiplied),
        QPoint(layer.position.x, layer.position.y),
        layer.visible,
        layer.opacity,
      });
    }
  }

  return ProjectDocument{std::move(document), std::move(rasterLayers)};
}

}  // namespace qco::ui
