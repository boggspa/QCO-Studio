#include "core/Document.h"
#include "core/UndoStack.h"

#include <iostream>
#include <memory>

namespace {

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " << #condition << '\n'; \
      return false; \
    } \
  } while (false)

bool documentKeepsLayerOrder()
{
  auto document = qco::core::Document::create("Test", {800, 600});
  const auto background = document.addLayer("Background", qco::core::LayerType::Raster, {800, 600});
  const auto paint = document.addLayer("Paint", qco::core::LayerType::Raster, {800, 600});

  CHECK(document.layers().size() == 2);
  CHECK(document.layers()[0].id == background);
  CHECK(document.layers()[1].id == paint);

  const auto moved = document.moveLayer(background, 1);
  CHECK(moved);
  CHECK(document.layers()[0].id == paint);
  CHECK(document.layers()[1].id == background);
  return true;
}

bool documentRejectsInvalidCanvasResize()
{
  auto document = qco::core::Document::create("Test", {320, 240});
  document.resizeCanvas({0, 0});
  CHECK(document.canvasSize().width == 320);
  CHECK(document.canvasSize().height == 240);
  return true;
}

bool documentPreservesExplicitLayerMetadata()
{
  auto document = qco::core::Document::create("Test", {320, 240});

  qco::core::Layer layer;
  layer.id = 42;
  layer.name = "Imported";
  layer.type = qco::core::LayerType::Raster;
  layer.position = {5, 7};
  layer.size = {10, 11};
  layer.visible = false;
  layer.opacity = 0.4;

  CHECK(document.addLayer(layer));
  CHECK(document.addLayer(layer) == false);
  CHECK(document.setLayerVisibility(42, true));
  CHECK(document.setLayerOpacity(42, 1.5));
  CHECK(document.setLayerPosition(42, {9, 10}));
  CHECK(document.setLayerName(42, "Renamed"));

  const auto* imported = document.findLayer(42);
  CHECK(imported != nullptr);
  CHECK(imported->name == "Renamed");
  CHECK(imported->visible);
  CHECK(imported->opacity == 1.0);
  CHECK(imported->position.x == 9);
  CHECK(imported->position.y == 10);

  const auto nextId = document.addLayer("Next", qco::core::LayerType::Raster, {1, 1});
  CHECK(nextId == 43);
  return true;
}

bool documentPreservesTypeSpecificLayerPayloads()
{
  auto document = qco::core::Document::create("Test", {320, 240});

  qco::core::Layer textLayer;
  textLayer.id = 1;
  textLayer.name = "Headline";
  textLayer.type = qco::core::LayerType::Text;
  textLayer.size = {120, 40};
  qco::core::TextLayerPayload textPayload;
  textPayload.text = "Hello";
  textPayload.color = "#FF102030";
  textPayload.pointSize = 36;
  textLayer.textPayload = textPayload;
  CHECK(document.addLayer(textLayer));

  const auto* importedTextLayer = document.findLayer(1);
  CHECK(importedTextLayer != nullptr);
  CHECK(importedTextLayer->textPayload.has_value());
  CHECK(importedTextLayer->textPayload->text == "Hello");
  CHECK(importedTextLayer->textPayload->color == "#FF102030");
  CHECK(importedTextLayer->textPayload->pointSize == 36);
  CHECK(!importedTextLayer->shapePayload.has_value());

  qco::core::Layer shapeLayer;
  shapeLayer.id = 2;
  shapeLayer.name = "Badge";
  shapeLayer.type = qco::core::LayerType::Shape;
  shapeLayer.size = {120, 40};
  qco::core::ShapeLayerPayload shapePayload;
  shapePayload.shape = "ellipse";
  shapePayload.fillColor = "#802D9CDB";
  shapePayload.strokeColor = "#FF010203";
  shapePayload.strokeWidth = 3;
  shapeLayer.shapePayload = shapePayload;
  CHECK(document.addLayer(shapeLayer));

  const auto* importedShapeLayer = document.findLayer(2);
  CHECK(importedShapeLayer != nullptr);
  CHECK(importedShapeLayer->shapePayload.has_value());
  CHECK(importedShapeLayer->shapePayload->shape == "ellipse");
  CHECK(importedShapeLayer->shapePayload->fillColor == "#802D9CDB");
  CHECK(importedShapeLayer->shapePayload->strokeColor == "#FF010203");
  CHECK(importedShapeLayer->shapePayload->strokeWidth == 3);
  CHECK(!importedShapeLayer->textPayload.has_value());

  qco::core::Layer rasterLayer;
  rasterLayer.id = 3;
  rasterLayer.name = "Pixels";
  rasterLayer.type = qco::core::LayerType::Raster;
  rasterLayer.size = {120, 40};
  rasterLayer.textPayload = textPayload;
  rasterLayer.shapePayload = shapePayload;
  CHECK(document.addLayer(rasterLayer));

  const auto* importedRasterLayer = document.findLayer(3);
  CHECK(importedRasterLayer != nullptr);
  CHECK(!importedRasterLayer->textPayload.has_value());
  CHECK(!importedRasterLayer->shapePayload.has_value());
  return true;
}

bool undoStackDropsRedoBranch()
{
  int value = 0;
  qco::core::UndoStack stack;

  stack.push(std::make_unique<qco::core::LambdaCommand>(
    "increment",
    [&value]() { --value; },
    [&value]() { ++value; }));

  CHECK(value == 1);
  CHECK(stack.canUndo());
  CHECK(stack.undo());
  CHECK(value == 0);
  CHECK(stack.canRedo());

  stack.push(std::make_unique<qco::core::LambdaCommand>(
    "add ten",
    [&value]() { value -= 10; },
    [&value]() { value += 10; }));

  CHECK(value == 10);
  CHECK(!stack.canRedo());
  CHECK(stack.size() == 1);
  CHECK(stack.undoText() == "add ten");
  return true;
}

}  // namespace

int main()
{
  if (!documentKeepsLayerOrder() || !documentRejectsInvalidCanvasResize() || !documentPreservesExplicitLayerMetadata()
      || !documentPreservesTypeSpecificLayerPayloads() || !undoStackDropsRedoBranch()) {
    return 1;
  }
  return 0;
}
