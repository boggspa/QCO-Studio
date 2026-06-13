#pragma once

#include "image/ImageCodec.h"

#include <QImage>
#include <QList>
#include <QString>
#include <QVector>

#include <optional>

namespace qco::ui {

using ExportImageFormat = qco::image::ImageWriteFormat;

[[nodiscard]] QVector<ExportImageFormat> exportImageFormatsForWriters(const QList<QByteArray>& supportedFormats);
[[nodiscard]] QVector<ExportImageFormat> availableExportImageFormats();
[[nodiscard]] QString exportImageFileFilter(const QVector<ExportImageFormat>& formats);
[[nodiscard]] std::optional<ExportImageFormat> exportImageFormatForPathOrFilter(
  const QVector<ExportImageFormat>& formats,
  const QString& filePath,
  const QString& selectedFilter);
[[nodiscard]] QImage imageForExportFormat(const QImage& image, const ExportImageFormat& format);
[[nodiscard]] bool writeExportImage(
  const QString& filePath,
  const QImage& image,
  const ExportImageFormat& format,
  QString* errorMessage = nullptr);

}  // namespace qco::ui
