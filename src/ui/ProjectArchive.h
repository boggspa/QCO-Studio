#pragma once

#include "core/Document.h"

#include <QImage>
#include <QPoint>
#include <QString>
#include <QVector>

#include <cstdint>
#include <optional>

namespace qco::ui {

struct ProjectRasterLayer {
  std::uint64_t id = 0;
  QString name;
  QImage image;
  QPoint position;
  bool visible = true;
  qreal opacity = 1.0;
};

struct ProjectDocument {
  qco::core::Document document;
  QVector<ProjectRasterLayer> rasterLayers;
};

class ProjectArchive {
public:
  static bool save(
    const QString& filePath,
    const qco::core::Document& document,
    const QVector<ProjectRasterLayer>& rasterLayers,
    QString* errorMessage);

  [[nodiscard]] static std::optional<ProjectDocument> load(
    const QString& filePath,
    QString* errorMessage);
};

}  // namespace qco::ui
