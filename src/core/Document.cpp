#include "core/Document.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace qco::core {

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

void normalizeLayer(Layer& layer, Size canvasSize)
{
  if (layer.name.empty()) {
    layer.name = "Layer";
  }
  if (!layer.size.isValid()) {
    layer.size = canvasSize;
  }
  layer.opacity = clampOpacity(layer.opacity);
  if (layer.type != LayerType::Text) {
    layer.textPayload.reset();
  }
  if (layer.type != LayerType::Shape) {
    layer.shapePayload.reset();
  }
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

const MetadataMap& Document::metadata() const noexcept
{
  return metadata_;
}

void Document::setMetadata(std::string key, std::string value)
{
  if (key.empty()) {
    return;
  }
  metadata_[std::move(key)] = std::move(value);
}

bool Document::removeMetadata(std::string_view key)
{
  if (key.empty()) {
    return false;
  }
  return metadata_.erase(std::string(key)) > 0;
}

void Document::clearMetadata()
{
  metadata_.clear();
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
  layer.size = size.isValid() ? size : canvasSize_;
  layer.position = position;
  normalizeLayer(layer, canvasSize_);

  layers_.push_back(std::move(layer));
  return layers_.back().id;
}

bool Document::addLayer(Layer layer)
{
  if (layer.id == 0 || findLayer(layer.id) != nullptr) {
    return false;
  }

  normalizeLayer(layer, canvasSize_);

  nextLayerId_ = std::max(nextLayerId_, layer.id + 1);
  layers_.push_back(std::move(layer));
  return true;
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

bool Document::setLayerName(std::uint64_t id, std::string name)
{
  auto* layer = findLayer(id);
  if (layer == nullptr) {
    return false;
  }

  layer->name = name.empty() ? "Layer" : std::move(name);
  return true;
}

bool Document::setLayerVisibility(std::uint64_t id, bool visible)
{
  auto* layer = findLayer(id);
  if (layer == nullptr) {
    return false;
  }

  layer->visible = visible;
  return true;
}

bool Document::setLayerLocked(std::uint64_t id, bool locked)
{
  auto* layer = findLayer(id);
  if (layer == nullptr) {
    return false;
  }

  layer->locked = locked;
  return true;
}

bool Document::setLayerOpacity(std::uint64_t id, double opacity)
{
  auto* layer = findLayer(id);
  if (layer == nullptr) {
    return false;
  }

  layer->opacity = clampOpacity(opacity);
  return true;
}

bool Document::setLayerPosition(std::uint64_t id, Point position)
{
  auto* layer = findLayer(id);
  if (layer == nullptr) {
    return false;
  }

  layer->position = position;
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

LayerType layerTypeFromString(std::string_view value) noexcept
{
  if (value == "vector") {
    return LayerType::Vector;
  }
  if (value == "text") {
    return LayerType::Text;
  }
  if (value == "shape") {
    return LayerType::Shape;
  }
  if (value == "group") {
    return LayerType::Group;
  }
  if (value == "adjustment") {
    return LayerType::Adjustment;
  }
  if (value == "asset") {
    return LayerType::Asset;
  }
  if (value == "mask") {
    return LayerType::Mask;
  }
  return LayerType::Raster;
}

BlendMode blendModeFromString(std::string_view value) noexcept
{
  if (value == "multiply") {
    return BlendMode::Multiply;
  }
  if (value == "screen") {
    return BlendMode::Screen;
  }
  if (value == "overlay") {
    return BlendMode::Overlay;
  }
  if (value == "soft-light") {
    return BlendMode::SoftLight;
  }
  if (value == "hard-light") {
    return BlendMode::HardLight;
  }
  if (value == "color-dodge") {
    return BlendMode::ColorDodge;
  }
  if (value == "color-burn") {
    return BlendMode::ColorBurn;
  }
  if (value == "difference") {
    return BlendMode::Difference;
  }
  if (value == "exclusion") {
    return BlendMode::Exclusion;
  }
  if (value == "hue") {
    return BlendMode::Hue;
  }
  if (value == "saturation") {
    return BlendMode::Saturation;
  }
  if (value == "color") {
    return BlendMode::Color;
  }
  if (value == "luminosity") {
    return BlendMode::Luminosity;
  }
  if (value == "add") {
    return BlendMode::Add;
  }
  if (value == "subtract") {
    return BlendMode::Subtract;
  }
  if (value == "darken") {
    return BlendMode::Darken;
  }
  if (value == "lighten") {
    return BlendMode::Lighten;
  }
  return BlendMode::Normal;
}

}  // namespace qco::core
