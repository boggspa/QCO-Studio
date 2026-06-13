#pragma once

#include "core/Document.h"

#include <QImage>
#include <QString>
#include <QVector>

#include <cstdint>

namespace qmx::ui {

struct ProjectRasterLayer {
  std::uint64_t id = 0;
  QString name;
  QImage image;
};

class ProjectArchive {
public:
  static bool save(
    const QString& filePath,
    const qmx::core::Document& document,
    const QVector<ProjectRasterLayer>& rasterLayers,
    QString* errorMessage);
};

}  // namespace qmx::ui
