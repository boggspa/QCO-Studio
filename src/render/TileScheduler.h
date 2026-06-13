#pragma once

#include <QRect>
#include <QSize>
#include <QVector>

namespace qco::render {

[[nodiscard]] QSize normalizedTileSize(QSize tileSize);
[[nodiscard]] QVector<QRect> tileRectsForCanvas(QSize canvasSize, QSize tileSize);
[[nodiscard]] QVector<QRect> tileRectsForDirtyRect(
  QSize canvasSize,
  QSize tileSize,
  QRect dirtyRect);

class DirtyTileScheduler final {
public:
  explicit DirtyTileScheduler(QSize canvasSize, QSize tileSize = QSize(512, 512));

  void reset(QSize canvasSize);
  void invalidateAll();
  void invalidate(QRect dirtyRect);

  [[nodiscard]] QVector<QRect> takeDirtyTiles();
  [[nodiscard]] bool hasDirtyTiles() const noexcept;

private:
  QSize canvasSize_;
  QSize tileSize_;
  QVector<QRect> dirtyTiles_;
};

}  // namespace qco::render
