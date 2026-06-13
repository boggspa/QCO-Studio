#include "core/Document.h"
#include "core/UndoStack.h"

#include <cassert>
#include <memory>

namespace {

void documentKeepsLayerOrder()
{
  auto document = qco::core::Document::create("Test", {800, 600});
  const auto background = document.addLayer("Background", qco::core::LayerType::Raster, {800, 600});
  const auto paint = document.addLayer("Paint", qco::core::LayerType::Raster, {800, 600});

  assert(document.layers().size() == 2);
  assert(document.layers()[0].id == background);
  assert(document.layers()[1].id == paint);

  const auto moved = document.moveLayer(background, 1);
  assert(moved);
  assert(document.layers()[0].id == paint);
  assert(document.layers()[1].id == background);
}

void documentRejectsInvalidCanvasResize()
{
  auto document = qco::core::Document::create("Test", {320, 240});
  document.resizeCanvas({0, 0});
  assert(document.canvasSize().width == 320);
  assert(document.canvasSize().height == 240);
}

void documentPreservesExplicitLayerMetadata()
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

  assert(document.addLayer(layer));
  assert(document.addLayer(layer) == false);
  assert(document.setLayerVisibility(42, true));
  assert(document.setLayerOpacity(42, 1.5));
  assert(document.setLayerPosition(42, {9, 10}));
  assert(document.setLayerName(42, "Renamed"));

  const auto* imported = document.findLayer(42);
  assert(imported != nullptr);
  assert(imported->name == "Renamed");
  assert(imported->visible);
  assert(imported->opacity == 1.0);
  assert(imported->position.x == 9);
  assert(imported->position.y == 10);

  const auto nextId = document.addLayer("Next", qco::core::LayerType::Raster, {1, 1});
  assert(nextId == 43);
}

void undoStackDropsRedoBranch()
{
  int value = 0;
  qco::core::UndoStack stack;

  stack.push(std::make_unique<qco::core::LambdaCommand>(
    "increment",
    [&value]() { --value; },
    [&value]() { ++value; }));

  assert(value == 1);
  assert(stack.canUndo());
  assert(stack.undo());
  assert(value == 0);
  assert(stack.canRedo());

  stack.push(std::make_unique<qco::core::LambdaCommand>(
    "add ten",
    [&value]() { value -= 10; },
    [&value]() { value += 10; }));

  assert(value == 10);
  assert(!stack.canRedo());
  assert(stack.size() == 1);
  assert(stack.undoText() == "add ten");
}

}  // namespace

int main()
{
  documentKeepsLayerOrder();
  documentRejectsInvalidCanvasResize();
  documentPreservesExplicitLayerMetadata();
  undoStackDropsRedoBranch();
  return 0;
}
