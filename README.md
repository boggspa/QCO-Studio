# QCO Studio

QCO Studio is a greenfield, legally original desktop image editor foundation. The initial target is a C++20/CMake/Qt 6 application with a clean path toward Skia rendering, OpenImageIO import/export, OpenColorIO color workflows, LibRaw RAW support, Exiv2 metadata, and an internal plugin SDK.

This repository is currently Phase 1. The app skeleton includes:

- Qt 6 desktop shell with main window, canvas, tool rail, layers panel, properties panel, and history placeholder.
- New document and open raster image workflows.
- Pan and zoom canvas view with checkerboard transparency.
- PNG/JPEG export through Qt image IO.
- Native `.qco` project package skeleton saved as a ZIP-compatible archive.
- Native `.qco` project reopen for Phase 1 raster-layer packages.
- Basic raster layer add, duplicate, rename, reorder, visibility, opacity, and canvas move controls.
- Initial editing tools: configurable brush, eraser, flood fill, drag crop, crop-to-layer, raster-backed text layer, and raster-backed shape layer.
- Core document/layer model and command undo stack.
- Core, archive round-trip, and canvas smoke tests.

## Build

Core-only build, useful before Qt is installed:

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

Desktop app build requires Qt 6.5 or newer with Qt Widgets:

```sh
cmake --preset desktop -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build --preset desktop
```

With vcpkg:

```sh
cmake --preset desktop -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset desktop
```

On macOS with Homebrew `qtbase`, use:

```sh
brew install qtbase
cmake --preset desktop -DCMAKE_PREFIX_PATH="$(brew --prefix qtbase)"
cmake --build --preset desktop
```

## Current Dependency Reality

The local machine has CMake, Git, and Homebrew `qtbase` installed. Both the core-only target and the Qt desktop target have been configured, built, and tested locally.
GitHub Actions runs macOS, Linux, and Windows CI.

## Project Notes

- Product naming still needs legal clearance before public launch. The project was renamed away from Qimage-branded wording because existing Qimage photo-printing products are in the market.
- The UI must remain familiar to professional raster/vector users without copying Adobe, Canva, GIMP, or other product artwork, naming, icons, or exact layouts.
- See [docs/technical-design.md](docs/technical-design.md) for the Phase 1 design pass and architecture plan.
- See [docs/roadmap.md](docs/roadmap.md) for the active milestone order.
