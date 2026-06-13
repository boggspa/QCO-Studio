#include "ui/ImageExport.h"

namespace qco::ui {

QVector<ExportImageFormat> exportImageFormatsForWriters(const QList<QByteArray>& supportedFormats)
{
  return qco::image::writableImageFormatsForFormats(supportedFormats);
}

QVector<ExportImageFormat> availableExportImageFormats()
{
  const qco::image::QtImageCodec codec;
  return codec.writeFormats();
}

QString exportImageFileFilter(const QVector<ExportImageFormat>& formats)
{
  return qco::image::imageWriteFileFilter(formats);
}

std::optional<ExportImageFormat> exportImageFormatForPathOrFilter(
  const QVector<ExportImageFormat>& formats,
  const QString& filePath,
  const QString& selectedFilter)
{
  return qco::image::imageWriteFormatForPathOrFilter(formats, filePath, selectedFilter);
}

QImage imageForExportFormat(const QImage& image, const ExportImageFormat& format)
{
  return qco::image::imageForWriteFormat(image, format);
}

bool writeExportImage(
  const QString& filePath,
  const QImage& image,
  const ExportImageFormat& format,
  QString* errorMessage)
{
  const qco::image::QtImageCodec codec;
  return codec.write(filePath, image, format, errorMessage);
}

}  // namespace qco::ui
