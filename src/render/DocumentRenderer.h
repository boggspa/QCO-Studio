#pragma once

#include "core/Document.h"

#include <QColor>
#include <QImage>
#include <QVector>

#include <cstdint>

namespace qco::render {

struct LayerImage {
  std::uint64_t id = 0;
  QImage image;
};

class DocumentRenderer {
public:
  virtual ~DocumentRenderer() = default;

  [[nodiscard]] virtual QImage render(
    const qco::core::Document& document,
    const QVector<LayerImage>& layerImages,
    const QColor& background = Qt::transparent) const = 0;
};

class QtDocumentRenderer final : public DocumentRenderer {
public:
  [[nodiscard]] QImage render(
    const qco::core::Document& document,
    const QVector<LayerImage>& layerImages,
    const QColor& background = Qt::transparent) const override;
};

}  // namespace qco::render
