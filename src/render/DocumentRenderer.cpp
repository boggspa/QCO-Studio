#include "render/DocumentRenderer.h"

#include <QPainter>
#include <QSize>

#include <algorithm>

namespace qco::render {
namespace {

[[nodiscard]] QSize toQtSize(qco::core::Size size)
{
  return QSize(size.width, size.height);
}

[[nodiscard]] QPoint toQtPoint(qco::core::Point point)
{
  return QPoint(point.x, point.y);
}

[[nodiscard]] const LayerImage* findLayerImage(
  const QVector<LayerImage>& layerImages,
  std::uint64_t id)
{
  const auto it = std::find_if(layerImages.begin(), layerImages.end(), [id](const LayerImage& layerImage) {
    return layerImage.id == id;
  });
  return it == layerImages.end() ? nullptr : &(*it);
}

}  // namespace

QImage QtDocumentRenderer::render(
  const qco::core::Document& document,
  const QVector<LayerImage>& layerImages,
  const QColor& background) const
{
  const auto canvasSize = document.canvasSize();
  if (!canvasSize.isValid()) {
    return {};
  }

  QImage output(toQtSize(canvasSize), QImage::Format_ARGB32_Premultiplied);
  output.fill(background);

  QPainter painter(&output);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  for (const auto& layer : document.layers()) {
    if (!layer.visible) {
      continue;
    }

    const auto* layerImage = findLayerImage(layerImages, layer.id);
    if (layerImage == nullptr || layerImage->image.isNull()) {
      continue;
    }

    painter.setOpacity(std::clamp(layer.opacity, 0.0, 1.0));
    painter.drawImage(toQtPoint(layer.position), layerImage->image);
  }

  painter.end();
  return output;
}

}  // namespace qco::render
