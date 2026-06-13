#include "core/UndoStack.h"

#include <cstddef>
#include <utility>

namespace qmx::core {

LambdaCommand::LambdaCommand(std::string name, Callback undoCallback, Callback redoCallback)
  : name_(std::move(name)),
    undoCallback_(std::move(undoCallback)),
    redoCallback_(std::move(redoCallback))
{
}

std::string LambdaCommand::name() const
{
  return name_;
}

void LambdaCommand::undo()
{
  if (undoCallback_) {
    undoCallback_();
  }
}

void LambdaCommand::redo()
{
  if (redoCallback_) {
    redoCallback_();
  }
}

void UndoStack::push(std::unique_ptr<Command> command)
{
  if (!command) {
    return;
  }

  if (nextIndex_ < commands_.size()) {
    commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(nextIndex_), commands_.end());
  }

  command->redo();
  commands_.push_back(std::move(command));
  nextIndex_ = commands_.size();
}

bool UndoStack::undo()
{
  if (!canUndo()) {
    return false;
  }

  --nextIndex_;
  commands_[nextIndex_]->undo();
  return true;
}

bool UndoStack::redo()
{
  if (!canRedo()) {
    return false;
  }

  commands_[nextIndex_]->redo();
  ++nextIndex_;
  return true;
}

void UndoStack::clear()
{
  commands_.clear();
  nextIndex_ = 0;
}

bool UndoStack::canUndo() const noexcept
{
  return nextIndex_ > 0;
}

bool UndoStack::canRedo() const noexcept
{
  return nextIndex_ < commands_.size();
}

std::string UndoStack::undoText() const
{
  return canUndo() ? commands_[nextIndex_ - 1]->name() : std::string();
}

std::string UndoStack::redoText() const
{
  return canRedo() ? commands_[nextIndex_]->name() : std::string();
}

std::size_t UndoStack::size() const noexcept
{
  return commands_.size();
}

std::size_t UndoStack::index() const noexcept
{
  return nextIndex_;
}

}  // namespace qmx::core
