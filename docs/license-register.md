# License Register

This register tracks intended and active dependencies. Do not add a runtime dependency without recording its license, role, and risk notes here.

| Dependency | Status | Role | Typical license | Notes |
| --- | --- | --- | --- | --- |
| Qt 6 | Active in app skeleton | Desktop UI, image IO for Phase 1 | LGPL/commercial depending on distribution model | Confirm final licensing model before binary distribution. |
| Qt Image Formats | Active in app packaging | TIFF/WebP and additional Qt image IO plugins | LGPL/commercial depending on distribution model; transitive codec licenses vary | Installed as `qtimageformats` where available. Review libtiff/libwebp and other codec notices before signed distribution. |
| CMake | Active | Build system | BSD-3-Clause | Build-time only. |
| vcpkg | Manifest only | Dependency acquisition | MIT | Optional; not vendored. |
| Skia | Deferred | GPU/canvas renderer | BSD-3-Clause | Bring in after tile/render abstraction lands. |
| OpenImageIO | Deferred | Professional image import/export | BSD-3-Clause | Review transitive codecs. |
| OpenColorIO | Deferred | Color pipeline | BSD-3-Clause | Needed before serious color-managed export. |
| LittleCMS | Deferred | ICC profile handling | MIT-like | Already present locally through Homebrew as `little-cms2`. |
| LibRaw | Deferred | RAW decoding | LGPL/CDDL dual license | Distribution choice needs review. |
| Exiv2 | Deferred | EXIF/IPTC/XMP metadata | GPL/commercial depending on version/terms | License risk; evaluate before adoption. |
| FreeType | Deferred | Text/glyph rendering support | FreeType License/GPLv2 option | Usually acceptable; record exact terms. |
| HarfBuzz | Deferred | Text shaping | MIT | Useful for advanced typography. |
| zstd or zip library | Deferred | Native project packaging | Varies | Phase 1 uses a tiny internal stored-ZIP writer for skeleton saves only. |
| GoogleTest or Catch2 | Deferred | Unit tests | BSD-3-Clause/BSL-1.0 | Core tests currently use standard assertions. |
| spdlog | Deferred | Logging | MIT | Phase 1 uses Qt logging in the app shell. |
