#include "render/TileScheduler.h"

#include <QPoint>

#include <algorithm>

namespace qco::render {
namespace {

constexpr int defaultTileDimension = 512;

[[nodiscard]] QRect canvasRect(QSize canvasSize)
{
  return QRect(QPoint(0, 0), canvasSize);
}

void appendUnique(QVector<QRect>& rects, const QRect& rect)
{
  if (!rects.contains(rect)) {
    rects.push_back(rect);
  }
}

}  // namespace

QSize normalizedTileSize(QSize tileSize)
{
  if (tileSize.width() <= 0) {
    tileSize.setWidth(defaultTileDimension);
  }
  if (tileSize.height() <= 0) {
    tileSize.setHeight(defaultTileDimension);
  }
  return tileSize;
}

QVector<QRect> tileRectsForCanvas(QSize canvasSize, QSize tileSize)
{
  QVector<QRect> tiles;
  if (canvasSize.width() <= 0 || canvasSize.height() <= 0) {
    return tiles;
  }

  tileSize = normalizedTileSize(tileSize);
  for (int y = 0; y < canvasSize.height(); y += tileSize.height()) {
    const int height = std::min(tileSize.height(), canvasSize.height() - y);
    for (int x = 0; x < canvasSize.width(); x += tileSize.width()) {
      const int width = std::min(tileSize.width(), canvasSize.width() - x);
      tiles.push_back(QRect(x, y, width, height));
    }
  }
  return tiles;
}

QVector<QRect> tileRectsForDirtyRect(QSize canvasSize, QSize tileSize, QRect dirtyRect)
{
  QVector<QRect> tiles;
  if (canvasSize.width() <= 0 || canvasSize.height() <= 0 || dirtyRect.isEmpty()) {
    return tiles;
  }

  const QRect clippedDirtyRect = dirtyRect.intersected(canvasRect(canvasSize));
  if (clippedDirtyRect.isEmpty()) {
    return tiles;
  }

  tileSize = normalizedTileSize(tileSize);
  const int firstColumn = clippedDirtyRect.left() / tileSize.width();
  const int lastColumn = clippedDirtyRect.right() / tileSize.width();
  const int firstRow = clippedDirtyRect.top() / tileSize.height();
  const int lastRow = clippedDirtyRect.bottom() / tileSize.height();

  for (int row = firstRow; row <= lastRow; ++row) {
    for (int column = firstColumn; column <= lastColumn; ++column) {
      const int x = column * tileSize.width();
      const int y = row * tileSize.height();
      const QRect tileRect(x, y, tileSize.width(), tileSize.height());
      tiles.push_back(tileRect.intersected(canvasRect(canvasSize)));
    }
  }
  return tiles;
}

DirtyTileScheduler::DirtyTileScheduler(QSize canvasSize, QSize tileSize)
  : canvasSize_(canvasSize),
    tileSize_(normalizedTileSize(tileSize))
{
}

void DirtyTileScheduler::reset(QSize canvasSize)
{
  canvasSize_ = canvasSize;
  dirtyTiles_.clear();
}

void DirtyTileScheduler::invalidateAll()
{
  dirtyTiles_ = tileRectsForCanvas(canvasSize_, tileSize_);
}

void DirtyTileScheduler::invalidate(QRect dirtyRect)
{
  for (const auto& tileRect : tileRectsForDirtyRect(canvasSize_, tileSize_, dirtyRect)) {
    appendUnique(dirtyTiles_, tileRect);
  }
}

QVector<QRect> DirtyTileScheduler::takeDirtyTiles()
{
  auto dirtyTiles = dirtyTiles_;
  dirtyTiles_.clear();
  return dirtyTiles;
}

bool DirtyTileScheduler::hasDirtyTiles() const noexcept
{
  return !dirtyTiles_.isEmpty();
}

}  // namespace qco::render
