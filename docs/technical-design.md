# QCO Studio Technical Design Pass

## 1. Recommended Final Tech Stack

- Language: C++20 now, C++23 where toolchains are ready.
- UI: Qt 6 Widgets first. QML can be introduced later for highly custom panels if needed.
- Rendering: Skia behind a renderer interface, with CPU fallback and GPU acceleration where stable.
- Image IO: OpenImageIO for broad raster formats, Qt image IO only for Phase 1 bootstrap.
- Color: OpenColorIO plus LittleCMS for ICC profile handling.
- Metadata: Exiv2 or a license-reviewed alternative for EXIF/IPTC/XMP.
- RAW: LibRaw behind a sidecar-based, non-destructive development pipeline.
- Vector/text: custom scene graph, FreeType/HarfBuzz, SVG import/export.
- Build: CMake with vcpkg manifest support.
- Tests: core unit tests, render golden tests, file round-trip tests, and UI interaction tests.

## 2. Repository Structure

```text
src/app        Application bootstrap, settings, logging
src/ui         Qt windows, panels, canvas widget, project archive adapter
src/core       Document, layer, tool contracts, undo stack
src/render     Future Skia renderer and compositor
src/image      Future OpenImageIO, color, metadata, RAW pipeline
src/vector     Future path, shape, SVG, glyph outline model
src/tools      Future tool implementations
src/plugins    Future internal plugin SDK and host
tests          Unit and integration tests
docs           Architecture, licensing, roadmap, legal notes
```

## 3. Dependency List With Licenses

See [license-register.md](license-register.md). Phase 1 activates only Qt 6 and CMake. Skia, OpenImageIO, OpenColorIO, LibRaw, Exiv2, FreeType, HarfBuzz, and logging/test libraries are intentionally deferred until their integration points are real.

## 4. V1 Feature List

- New document, open image, save native project, reopen native project.
- Export PNG/JPEG/TIFF/WebP/PDF.
- Pan, zoom, fit, 100 percent, rulers, guides, grid, snapping.
- Raster layers, vector layers, text layers, shape layers, groups, masks, opacity, blend modes.
- Move, crop, selection, brush, eraser, fill, gradient, text, shape, and pen tools.
- Non-destructive adjustment layers for core tonal/color operations.
- Undo/redo, history, dirty tracking, autosave and recovery.
- Asset panel, simple photo browser, ratings/flags, metadata view, batch export.

## 5. Deferred Feature List

- PSD as a primary workflow, plugin marketplace, cloud accounts, telemetry, AI features, advanced RAW development, OpenFX bridge, tablet companion app, artboards, symbols/components, pressure brush system, lens correction, camera profiles, and complex filters.

## 6. Native File Format Design

Native documents use `.qco`, a ZIP-compatible package:

```text
manifest.json
document.json
layers/
masks/
assets/
previews/
metadata/
colour/
history/
```

The format must preserve layer structure, masks, editable text, vectors, adjustments, embedded assets, color profiles, metadata, autosave/recovery data, and migration metadata. Phase 1 writes a stored-ZIP skeleton with `manifest.json`, `document.json`, and raster layer PNG payloads. It is not yet the final archival implementation.

## 7. Canvas And Rendering Architecture

The long-term canvas is an infinite pasteboard around fixed document bounds. Rendering should run through a tile renderer, dirty rectangle scheduler, layer compositor, mask/vector/text renderers, and GPU abstraction. Phase 1 uses a Qt `QWidget` canvas with checkerboard, document bounds, pan/zoom, and raster layer drawing so UI workflows can progress before Skia lands.

## 8. Layer Model Architecture

Layers have stable ids, names, type, visibility, lock state, opacity, blend mode, bounds, and type-specific payload references. The core model owns ordering and metadata; renderer/image modules own pixel payloads. Groups and masks should be explicit graph relationships rather than hidden UI-only nesting.

## 9. Tool System Architecture

Tools should expose activation, pointer/keyboard events, options, cursors, preview overlay rendering, and command emission. Tool implementations should not mutate documents directly; they emit commands so undo/history and autosave remain consistent.

## 10. Undo And History Architecture

Every edit is a reversible command. Commands can be grouped for gestures such as brush strokes or transforms. The app maintains dirty state, command labels, and a history panel. Large raster commands should use tile deltas or snapshots under a memory budget.

## 11. Plugin Architecture Proposal

Start with an internal C++ SDK loaded from signed or user-approved plugin bundles. Plugin classes should include filters, importers/exporters, panels, and tool extensions. Use strict ABI versioning, metadata manifests, crash isolation plans, and capability declarations. OpenFX-style bridges can be researched after the internal API is stable.

## 12. UX Layout Plan

The default workspace uses a left tool rail, top context bar, center canvas, right inspector docks, bottom status bar, and optional asset/photo browser docks. Workspace modes are Photo Browser, Edit, Vector, Export, and Review. The visual language must be original, restrained, and built for repeated professional work.

## 13. Legal Risk Review

- Do not copy Photoshop, Illustrator, Lightroom, Canva, GIMP, or other product artwork, icons, UI text, panel styling, or proprietary behaviors.
- Do not use Adobe or other third-party trademarks in product UI.
- Keep dependency licenses and attributions current.
- Treat the name "QCO Studio" as provisional until proper legal clearance. The project moved away from Qimage-branded wording because existing Qimage photo printing products are publicly visible, including Qimage Ultimate and Qimage One.
- Avoid GPL dependencies unless the product license strategy explicitly allows them.

## 14. Build Plan

Phase 1 uses CMake presets. The core library and tests build without Qt. The desktop target requires Qt 6.5 or newer with Widgets and can be supplied by vcpkg, Homebrew `qtbase`, a system package manager, or a commercial Qt installation. CI should be added for macOS, Windows, and Linux once dependency installation is scripted.

## 15. Test Plan

- Core unit tests for document/layer invariants and undo stack behavior.
- File round-trip tests for `.qco` archive structure, multi-layer ordering, image payloads, and layer metadata.
- Golden image tests for renderer determinism after Skia lands.
- UI interaction tests for canvas pan/zoom, tool gestures, dialogs, and export.
- Performance tests for large images, many layers, tile invalidation, and cancellation.
