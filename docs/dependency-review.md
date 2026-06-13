# Dependency Review

This is a planning review for future runtime dependencies. It is not legal advice. Before any signed or commercial distribution, confirm final license obligations, notices, dynamic/static linking choices, and transitive codec licenses with the chosen distribution model.

## Current Decisions

| Area | Preferred path | Reason | Gate before enabling |
| --- | --- | --- | --- |
| Renderer | Skia behind `DocumentRenderer` | Skia is the intended GPU/canvas backend and has a permissive BSD-style license. | Add optional build integration, notices, and CPU fallback tests. |
| Image IO | OpenImageIO behind an image codec boundary | OpenImageIO covers professional raster formats; current upstream licensing is primarily Apache-2.0, not the older BSD-only assumption. | Review bundled/transitive codec notices and package size before CI packaging. |
| Color | OpenColorIO for view/display transforms, LittleCMS for ICC transforms | Both are suitable for a color-managed pipeline; LittleCMS keeps ICC profile handling focused. | Add color profile test assets with clear licenses and deterministic transform tests. |
| RAW | LibRaw behind a non-destructive RAW provider boundary | LibRaw is purpose-built for camera RAW and can be isolated from the document model. | Choose LGPL-2.1 or CDDL-1.0 distribution strategy, and review RawSpeed/dcraw transitive notes. |
| Metadata | Do not link Exiv2 by default | Exiv2 is GPL-2.0-or-later, which is a high-risk default for a non-GPL desktop app. | Prefer sidecar metadata first; evaluate Adobe XMP Toolkit for XMP and libexif for EXIF, or get legal clearance before Exiv2. |

## Dependency Notes

| Dependency | Upstream license signal | Adoption decision | Source |
| --- | --- | --- | --- |
| Skia | BSD-style license in project docs/repository. | Acceptable for renderer backend work once build packaging is ready. | https://skia.org/docs/ and https://github.com/google/skia/blob/main/LICENSE |
| OpenImageIO | Original code is Apache-2.0; docs are CC-BY-4.0; some incorporated code has compatible third-party licenses. | Suitable for the image pipeline after transitive codec/notice review. | https://github.com/AcademySoftwareFoundation/OpenImageIO and https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/LICENSE.md |
| OpenColorIO | BSD-3-Clause. | Suitable for color pipeline work. | https://opencolorio.readthedocs.io/en/latest/aswf/license.html and https://github.com/AcademySoftwareFoundation/OpenColorIO/blob/main/LICENSE |
| LittleCMS | MIT license. | Suitable for ICC profile handling; include attribution notice. | https://littlecms.com/color-engine/ |
| LibRaw | LGPL-2.1 or CDDL-1.0 choice in current copyright/license files. | Use only behind an optional RAW provider boundary until distribution strategy is decided. | https://github.com/LibRaw/LibRaw/blob/master/COPYRIGHT and https://www.libraw.org/news/libraw-0-18-released |
| Exiv2 | GPL-2.0-or-later. | Avoid as a default linked dependency unless the product license strategy changes or legal clearance is obtained. | https://exiv2.org/download.html and https://github.com/Exiv2/exiv2/blob/main/LICENSE.txt |
| Adobe XMP Toolkit SDK | BSD-3-Clause. | Candidate for XMP-specific metadata support. Does not replace all EXIF/IPTC needs. | https://github.com/adobe/XMP-Toolkit-SDK and https://github.com/adobe/XMP-Toolkit-SDK/blob/main/LICENSE |
| libexif | LGPL-2.1-family license signal. | Candidate for EXIF-specific support if dynamic linking and notice obligations fit the distribution model. | https://github.com/libexif/libexif |
| ExifTool | Same terms as Perl; broad command-line metadata coverage. | Candidate only as an optional out-of-process tool; needs command sanitization and packaging review. | https://exiftool.org/ |

## Implementation Rules

- Add runtime dependencies behind narrow interfaces first; do not let Qt UI code depend directly on OpenImageIO, OpenColorIO, LibRaw, or metadata libraries.
- Keep dependency features optional until CI packaging and notices are proven on macOS, Linux, and Windows.
- Add deterministic tests at the boundary where the dependency enters the app.
- Record notice files, linked library mode, and transitive codec/profile/test-asset licenses before publishing signed builds.
- Keep GPL-family dependencies out of the default linked desktop app unless the whole product license strategy deliberately accepts that obligation.
