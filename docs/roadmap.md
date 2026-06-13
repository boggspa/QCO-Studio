# QCO Studio Roadmap

## Phase 1 Closeout

- Open and reopen native `.qco` projects.
- Track dirty documents and prompt to save before close, new document, or open.
- Wire undo/redo to real document actions.
- Add raster layer creation and basic layer visibility/reorder controls.
- Keep a small app smoke test in CI.

## Milestone 2: Native Round Trip And Layer Basics

- Preserve layered `.qco` documents with raster payloads and metadata.
- Add duplicate, rename, reorder, visibility, opacity, and move operations.
- Export from document state.
- Keep undo/redo attached to all layer operations.

## V1 Spine

- Expand the initial brush, eraser, fill, crop, text, and shape tools beyond the current tool options and drag-crop baseline.
- Add native project reopen tests for text/shape/vector payloads as they land.
- Keep destructive operations explicit and undoable.

## Rendering And File Pipeline

- Introduce the Skia-ready renderer abstraction. The first `DocumentRenderer` boundary is in place with a Qt backend for export.
- Add tiled rendering and dirty rectangle scheduling. The Qt renderer now composes full exports from render tiles, and `TileScheduler` can map dirty rectangles to tile work.
- Add OpenImageIO, OpenColorIO/LittleCMS, LibRaw, and metadata support after license review and integration seams are ready.

## Collaboration

- Maintain labels for `phase-1`, `v1`, `rendering`, `layers`, `tools`, `file-format`, and `ui`.
- Track milestones in GitHub issues/projects.
- Protect `main` and use pull requests once the initial infrastructure commit is pushed.
- Finish creating the GitHub Projects board after refreshing GitHub CLI auth with `project` and `read:project` scopes.
