# License Register

This register tracks intended and active dependencies. Do not add a runtime dependency without recording its license, role, and risk notes here.

| Dependency | Status | Role | Typical license | Notes |
| --- | --- | --- | --- | --- |
| Qt 6 | Active in app skeleton | Desktop UI, image IO for Phase 1 | LGPL/commercial depending on distribution model | Confirm final licensing model before binary distribution. |
| Qt Image Formats | Active in app packaging | TIFF/WebP and additional Qt image IO plugins | LGPL/commercial depending on distribution model; transitive codec licenses vary | Installed as `qtimageformats` where available. Review libtiff/libwebp and other codec notices before signed distribution. |
| CMake | Active | Build system | BSD-3-Clause | Build-time only. |
| vcpkg | Manifest only | Dependency acquisition | MIT | Optional; not vendored. |
| Skia | Deferred, license-reviewed | GPU/canvas renderer | BSD-style / BSD-3-Clause signal | Acceptable candidate after optional build packaging is ready. Renderer and tile boundaries are already in place. |
| OpenImageIO | Deferred, license-reviewed | Professional image import/export | Apache-2.0 for original code; compatible third-party code; docs CC-BY-4.0 | Previous BSD-only assumption is stale. Review bundled codecs and notices before packaging. |
| OpenColorIO | Deferred, license-reviewed | Color pipeline | BSD-3-Clause | Suitable candidate for view/display transforms after color test assets are selected. |
| LittleCMS | Deferred, license-reviewed | ICC profile handling | MIT | Suitable candidate for ICC transforms. Include attribution notice before signed distribution. |
| LibRaw | Boundary active, runtime dependency deferred | RAW decoding | LGPL-2.1 or CDDL-1.0 choice | `RawImageProvider` is in place; keep LibRaw optional until linking and notice strategy is decided. |
| Exiv2 | Deferred, blocked by license risk | EXIF/IPTC/XMP metadata | GPL-2.0-or-later | Do not link by default unless product license strategy changes or legal clearance is obtained. Evaluate alternatives first. |
| Adobe XMP Toolkit SDK | Deferred, candidate | XMP metadata | BSD-3-Clause | Candidate for XMP support; not a complete EXIF/IPTC replacement. |
| libexif | Deferred, candidate | EXIF metadata | LGPL-2.1-family signal | Candidate for EXIF-only support if linking/notices fit distribution strategy. |
| ExifTool | Deferred, candidate tool | Broad metadata read/write | Same terms as Perl | Consider only as optional out-of-process integration with strict command sanitization. |
| FreeType | Deferred | Text/glyph rendering support | FreeType License/GPLv2 option | Usually acceptable; record exact terms. |
| HarfBuzz | Deferred | Text shaping | MIT | Useful for advanced typography. |
| zstd or zip library | Deferred | Native project packaging | Varies | Phase 1 uses a tiny internal stored-ZIP writer for skeleton saves only. |
| GoogleTest or Catch2 | Deferred | Unit tests | BSD-3-Clause/BSL-1.0 | Core tests currently use standard assertions. |
| spdlog | Deferred | Logging | MIT | Phase 1 uses Qt logging in the app shell. |
