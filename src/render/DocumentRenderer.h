#pragma once

#include "core/Document.h"
#include "render/TileScheduler.h"

#include <QColor>
#include <QImage>
#include <QRect>
#include <QVector>

#include <cstdint>

namespace qco::render {

struct LayerImage {
  std::uint64_t id = 0;
  QImage image;
};

struct RenderSettings {
  QSize tileSize = QSize(512, 512);
};

class DocumentRenderer {
public:
  virtual ~DocumentRenderer() = default;

  [[nodiscard]] QImage render(
    const qco::core::Document& document,
    const QVector<LayerImage>& layerImages,
    const QColor& background = Qt::transparent) const;

  [[nodiscard]] virtual QImage render(
    const qco::core::Document& document,
    const QVector<LayerImage>& layerImages,
    const QColor& background,
    const RenderSettings& settings) const = 0;
};

class QtDocumentRenderer final : public DocumentRenderer {
public:
  using DocumentRenderer::render;

  [[nodiscard]] QImage render(
    const qco::core::Document& document,
    const QVector<LayerImage>& layerImages,
    const QColor& background,
    const RenderSettings& settings) const override;

  [[nodiscard]] QImage renderTile(
    const qco::core::Document& document,
    const QVector<LayerImage>& layerImages,
    QRect tileRect,
    const QColor& background = Qt::transparent) const;
};

}  // namespace qco::render
