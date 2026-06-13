#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace qmx::core {

class Command {
public:
  virtual ~Command() = default;

  [[nodiscard]] virtual std::string name() const = 0;
  virtual void undo() = 0;
  virtual void redo() = 0;
};

class LambdaCommand final : public Command {
public:
  using Callback = std::function<void()>;

  LambdaCommand(std::string name, Callback undoCallback, Callback redoCallback);

  [[nodiscard]] std::string name() const override;
  void undo() override;
  void redo() override;

private:
  std::string name_;
  Callback undoCallback_;
  Callback redoCallback_;
};

class UndoStack {
public:
  void push(std::unique_ptr<Command> command);
  [[nodiscard]] bool undo();
  [[nodiscard]] bool redo();

  void clear();

  [[nodiscard]] bool canUndo() const noexcept;
  [[nodiscard]] bool canRedo() const noexcept;
  [[nodiscard]] std::string undoText() const;
  [[nodiscard]] std::string redoText() const;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t index() const noexcept;

private:
  std::vector<std::unique_ptr<Command>> commands_;
  std::size_t nextIndex_ = 0;
};

}  // namespace qmx::core
