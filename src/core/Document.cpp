#include "core/Document.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace qmx::core {

namespace {

[[nodiscard]] double clampOpacity(double opacity) noexcept
{
  if (opacity < 0.0) {
    return 0.0;
  }
  if (opacity > 1.0) {
    return 1.0;
  }
  return opacity;
}

}  // namespace

bool Size::isValid() const noexcept
{
  return width > 0 && height > 0;
}

Document::Document(std::string title, Size canvasSize)
  : title_(std::move(title)),
    canvasSize_(canvasSize.isValid() ? canvasSize : Size{1, 1})
{
  if (title_.empty()) {
    title_ = "Untitled";
  }
}

Document Document::create(std::string title, Size canvasSize)
{
  return Document(std::move(title), canvasSize);
}

const std::string& Document::title() const noexcept
{
  return title_;
}

void Document::setTitle(std::string title)
{
  title_ = std::move(title);
  if (title_.empty()) {
    title_ = "Untitled";
  }
}

Size Document::canvasSize() const noexcept
{
  return canvasSize_;
}

void Document::resizeCanvas(Size canvasSize)
{
  if (canvasSize.isValid()) {
    canvasSize_ = canvasSize;
  }
}

bool Document::isValid() const noexcept
{
  return canvasSize_.isValid();
}

std::uint64_t Document::addLayer(
  std::string name,
  LayerType type,
  Size size,
  Point position)
{
  Layer layer;
  layer.id = nextLayerId_++;
  layer.name = name.empty() ? "Layer" : std::move(name);
  layer.type = type;
  layer.opacity = clampOpacity(layer.opacity);
  layer.size = size.isValid() ? size : canvasSize_;
  layer.position = position;

  layers_.push_back(std::move(layer));
  return layers_.back().id;
}

bool Document::removeLayer(std::uint64_t id)
{
  const auto before = layers_.size();
  layers_.erase(
    std::remove_if(layers_.begin(), layers_.end(), [id](const Layer& layer) {
      return layer.id == id;
    }),
    layers_.end());
  return layers_.size() != before;
}

bool Document::moveLayer(std::uint64_t id, std::size_t newIndex)
{
  const auto currentIndex = layerIndex(id);
  if (!currentIndex.has_value() || newIndex >= layers_.size()) {
    return false;
  }

  if (*currentIndex == newIndex) {
    return true;
  }

  Layer layer = std::move(layers_[*currentIndex]);
  layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(*currentIndex));
  layers_.insert(layers_.begin() + static_cast<std::ptrdiff_t>(newIndex), std::move(layer));
  return true;
}

Layer* Document::findLayer(std::uint64_t id) noexcept
{
  const auto it = std::find_if(layers_.begin(), layers_.end(), [id](const Layer& layer) {
    return layer.id == id;
  });
  return it == layers_.end() ? nullptr : &(*it);
}

const Layer* Document::findLayer(std::uint64_t id) const noexcept
{
  const auto it = std::find_if(layers_.begin(), layers_.end(), [id](const Layer& layer) {
    return layer.id == id;
  });
  return it == layers_.end() ? nullptr : &(*it);
}

std::optional<std::size_t> Document::layerIndex(std::uint64_t id) const noexcept
{
  const auto it = std::find_if(layers_.begin(), layers_.end(), [id](const Layer& layer) {
    return layer.id == id;
  });
  if (it == layers_.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::distance(layers_.begin(), it));
}

const std::vector<Layer>& Document::layers() const noexcept
{
  return layers_;
}

std::string_view toString(LayerType type) noexcept
{
  switch (type) {
    case LayerType::Raster:
      return "raster";
    case LayerType::Vector:
      return "vector";
    case LayerType::Text:
      return "text";
    case LayerType::Shape:
      return "shape";
    case LayerType::Group:
      return "group";
    case LayerType::Adjustment:
      return "adjustment";
    case LayerType::Asset:
      return "asset";
    case LayerType::Mask:
      return "mask";
  }
  return "unknown";
}

std::string_view toString(BlendMode mode) noexcept
{
  switch (mode) {
    case BlendMode::Normal:
      return "normal";
    case BlendMode::Multiply:
      return "multiply";
    case BlendMode::Screen:
      return "screen";
    case BlendMode::Overlay:
      return "overlay";
    case BlendMode::SoftLight:
      return "soft-light";
    case BlendMode::HardLight:
      return "hard-light";
    case BlendMode::ColorDodge:
      return "color-dodge";
    case BlendMode::ColorBurn:
      return "color-burn";
    case BlendMode::Difference:
      return "difference";
    case BlendMode::Exclusion:
      return "exclusion";
    case BlendMode::Hue:
      return "hue";
    case BlendMode::Saturation:
      return "saturation";
    case BlendMode::Color:
      return "color";
    case BlendMode::Luminosity:
      return "luminosity";
    case BlendMode::Add:
      return "add";
    case BlendMode::Subtract:
      return "subtract";
    case BlendMode::Darken:
      return "darken";
    case BlendMode::Lighten:
      return "lighten";
  }
  return "unknown";
}

}  // namespace qmx::core
