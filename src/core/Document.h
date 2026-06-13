#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace qco::core {

struct Size {
  int width = 0;
  int height = 0;

  [[nodiscard]] bool isValid() const noexcept;
};

struct Point {
  int x = 0;
  int y = 0;
};

enum class LayerType {
  Raster,
  Vector,
  Text,
  Shape,
  Group,
  Adjustment,
  Asset,
  Mask
};

enum class BlendMode {
  Normal,
  Multiply,
  Screen,
  Overlay,
  SoftLight,
  HardLight,
  ColorDodge,
  ColorBurn,
  Difference,
  Exclusion,
  Hue,
  Saturation,
  Color,
  Luminosity,
  Add,
  Subtract,
  Darken,
  Lighten
};

struct TextLayerPayload {
  std::string text;
  std::string color = "#FF181818";
  int pointSize = 48;
};

struct ShapeLayerPayload {
  std::string shape = "rectangle";
  std::string fillColor = "#782D9CDB";
  std::string strokeColor = "#FF181818";
  int strokeWidth = 4;
};

struct Layer {
  std::uint64_t id = 0;
  std::string name;
  LayerType type = LayerType::Raster;
  bool visible = true;
  bool locked = false;
  double opacity = 1.0;
  BlendMode blendMode = BlendMode::Normal;
  Point position;
  Size size;
  std::optional<TextLayerPayload> textPayload;
  std::optional<ShapeLayerPayload> shapePayload;
};

class Document {
public:
  Document(std::string title, Size canvasSize);

  [[nodiscard]] static Document create(std::string title, Size canvasSize);

  [[nodiscard]] const std::string& title() const noexcept;
  void setTitle(std::string title);

  [[nodiscard]] Size canvasSize() const noexcept;
  void resizeCanvas(Size canvasSize);

  [[nodiscard]] bool isValid() const noexcept;

  [[nodiscard]] std::uint64_t addLayer(
    std::string name,
    LayerType type,
    Size size,
    Point position = {});
  [[nodiscard]] bool addLayer(Layer layer);

  [[nodiscard]] bool removeLayer(std::uint64_t id);
  [[nodiscard]] bool moveLayer(std::uint64_t id, std::size_t newIndex);
  [[nodiscard]] bool setLayerName(std::uint64_t id, std::string name);
  [[nodiscard]] bool setLayerVisibility(std::uint64_t id, bool visible);
  [[nodiscard]] bool setLayerOpacity(std::uint64_t id, double opacity);
  [[nodiscard]] bool setLayerPosition(std::uint64_t id, Point position);

  [[nodiscard]] Layer* findLayer(std::uint64_t id) noexcept;
  [[nodiscard]] const Layer* findLayer(std::uint64_t id) const noexcept;
  [[nodiscard]] std::optional<std::size_t> layerIndex(std::uint64_t id) const noexcept;

  [[nodiscard]] const std::vector<Layer>& layers() const noexcept;

private:
  std::string title_;
  Size canvasSize_;
  std::uint64_t nextLayerId_ = 1;
  std::vector<Layer> layers_;
};

[[nodiscard]] std::string_view toString(LayerType type) noexcept;
[[nodiscard]] std::string_view toString(BlendMode mode) noexcept;
[[nodiscard]] LayerType layerTypeFromString(std::string_view value) noexcept;
[[nodiscard]] BlendMode blendModeFromString(std::string_view value) noexcept;

}  // namespace qco::core
