#include "core/Document.h"
#include "core/UndoStack.h"

#include <cassert>
#include <memory>

namespace {

void documentKeepsLayerOrder()
{
  auto document = qmx::core::Document::create("Test", {800, 600});
  const auto background = document.addLayer("Background", qmx::core::LayerType::Raster, {800, 600});
  const auto paint = document.addLayer("Paint", qmx::core::LayerType::Raster, {800, 600});

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
  auto document = qmx::core::Document::create("Test", {320, 240});
  document.resizeCanvas({0, 0});
  assert(document.canvasSize().width == 320);
  assert(document.canvasSize().height == 240);
}

void undoStackDropsRedoBranch()
{
  int value = 0;
  qmx::core::UndoStack stack;

  stack.push(std::make_unique<qmx::core::LambdaCommand>(
    "increment",
    [&value]() { --value; },
    [&value]() { ++value; }));

  assert(value == 1);
  assert(stack.canUndo());
  assert(stack.undo());
  assert(value == 0);
  assert(stack.canRedo());

  stack.push(std::make_unique<qmx::core::LambdaCommand>(
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
  undoStackDropsRedoBranch();
  return 0;
}
