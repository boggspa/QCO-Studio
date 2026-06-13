#include "image/RawImageProvider.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <utility>

namespace qco::image {
namespace {

struct RawSuffixInfo {
  QString suffix;
  QByteArray format;
};

[[nodiscard]] const std::array<RawSuffixInfo, 26>& rawSuffixTable()
{
  static const std::array<RawSuffixInfo, 26> suffixes = {{
    {QStringLiteral("3fr"), QByteArrayLiteral("3fr")},
    {QStringLiteral("arw"), QByteArrayLiteral("arw")},
    {QStringLiteral("cr2"), QByteArrayLiteral("cr2")},
    {QStringLiteral("cr3"), QByteArrayLiteral("cr3")},
    {QStringLiteral("crw"), QByteArrayLiteral("crw")},
    {QStringLiteral("dcr"), QByteArrayLiteral("dcr")},
    {QStringLiteral("dng"), QByteArrayLiteral("dng")},
    {QStringLiteral("erf"), QByteArrayLiteral("erf")},
    {QStringLiteral("fff"), QByteArrayLiteral("fff")},
    {QStringLiteral("gpr"), QByteArrayLiteral("gpr")},
    {QStringLiteral("iiq"), QByteArrayLiteral("iiq")},
    {QStringLiteral("kdc"), QByteArrayLiteral("kdc")},
    {QStringLiteral("mef"), QByteArrayLiteral("mef")},
    {QStringLiteral("mos"), QByteArrayLiteral("mos")},
    {QStringLiteral("mrw"), QByteArrayLiteral("mrw")},
    {QStringLiteral("nef"), QByteArrayLiteral("nef")},
    {QStringLiteral("nrw"), QByteArrayLiteral("nrw")},
    {QStringLiteral("orf"), QByteArrayLiteral("orf")},
    {QStringLiteral("pef"), QByteArrayLiteral("pef")},
    {QStringLiteral("raf"), QByteArrayLiteral("raf")},
    {QStringLiteral("raw"), QByteArrayLiteral("raw")},
    {QStringLiteral("rw2"), QByteArrayLiteral("rw2")},
    {QStringLiteral("rwl"), QByteArrayLiteral("rwl")},
    {QStringLiteral("sr2"), QByteArrayLiteral("sr2")},
    {QStringLiteral("srf"), QByteArrayLiteral("srf")},
    {QStringLiteral("x3f"), QByteArrayLiteral("x3f")},
  }};
  return suffixes;
}

[[nodiscard]] QString suffixPattern(const QStringList& suffixes)
{
  QStringList patterns;
  patterns.reserve(suffixes.size());
  for (const auto& suffix : suffixes) {
    patterns.push_back(QStringLiteral("*.%1").arg(suffix));
  }
  return patterns.join(QLatin1Char(' '));
}

void setError(QString* errorMessage, QString message)
{
  if (errorMessage != nullptr) {
    *errorMessage = std::move(message);
  }
}

}  // namespace

QStringList rawImageSuffixes()
{
  QStringList suffixes;
  suffixes.reserve(static_cast<qsizetype>(rawSuffixTable().size()));
  for (const auto& suffix : rawSuffixTable()) {
    suffixes.push_back(suffix.suffix);
  }
  suffixes.removeDuplicates();
  suffixes.sort();
  return suffixes;
}

QString rawImageOpenFileFilter()
{
  return QStringLiteral("Camera RAW (%1);;All files (*)").arg(suffixPattern(rawImageSuffixes()));
}

std::optional<QByteArray> rawFormatForPath(const QString& filePath)
{
  const auto suffix = QFileInfo(filePath).suffix().toLower();
  const auto& suffixes = rawSuffixTable();
  const auto it = std::find_if(suffixes.begin(), suffixes.end(), [&suffix](const RawSuffixInfo& candidate) {
    return candidate.suffix == suffix;
  });
  if (it == suffixes.end()) {
    return std::nullopt;
  }
  return it->format;
}

bool RawDecodeUnavailableProvider::canRead(const QString& filePath) const
{
  return rawFormatForPath(filePath).has_value();
}

std::optional<RawImage> RawDecodeUnavailableProvider::read(const QString& filePath, QString* errorMessage) const
{
  if (filePath.isEmpty()) {
    setError(errorMessage, QStringLiteral("The RAW image path is empty."));
    return std::nullopt;
  }

  const auto format = rawFormatForPath(filePath);
  if (!format.has_value()) {
    setError(errorMessage, QStringLiteral("The file extension is not a supported camera RAW format."));
    return std::nullopt;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    setError(errorMessage, file.errorString());
    return std::nullopt;
  }

  if (file.peek(16).isEmpty()) {
    setError(errorMessage, QStringLiteral("The RAW image file is empty."));
    return std::nullopt;
  }

  setError(
    errorMessage,
    QStringLiteral("Camera RAW decoding for %1 files requires a LibRaw-backed RawImageProvider.")
      .arg(QString::fromLatin1(format->toUpper())));
  return std::nullopt;
}

}  // namespace qco::image
