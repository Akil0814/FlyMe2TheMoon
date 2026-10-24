# Third-Party Notices

This document identifies third-party components used by the default CMake
configuration. Third-party material under `thirdparty/` is not relicensed by
this project. The listed upstream files are the authoritative copies of the
applicable license text and notices.

| Component | Default use and source location | License | Authoritative license or notice |
| --- | --- | --- | --- |
| SDL3 | Core platform, input, window, rendering, and audio API: `thirdparty/SDL3/` | zlib License | `thirdparty/SDL3/LICENSE.txt` |
| SDL3_image | PNG and JPEG image loading: `thirdparty/SDL3_image/` | zlib License | `thirdparty/SDL3_image/LICENSE.txt` |
| SDL3_ttf | Font rendering: `thirdparty/SDL3_ttf/` | zlib License | `thirdparty/SDL3_ttf/LICENSE.txt` |
| SDL3_mixer | WAVE, MP3, and Ogg Vorbis audio decoding/playback: `thirdparty/SDL3_mixer/` | zlib License | `thirdparty/SDL3_mixer/LICENSE.txt` |
| SDL3_gfx | SDL rendering primitives: `thirdparty/SDL3_gfx/` | zlib License | `thirdparty/SDL3_gfx/COPYING` |
| Box2D | 2D physics: `thirdparty/box2d/` | MIT License | `thirdparty/box2d/LICENSE` |
| ENet | Networking: `thirdparty/enet/` | MIT License | `thirdparty/enet/LICENSE` |
| Dear ImGui | Optional developer UI enabled by default: `thirdparty/imgui/` | MIT License | `thirdparty/imgui/LICENSE.txt` |
| JSON for Modern C++ | JSON parsing and serialization: `thirdparty/nlohmann/json.hpp` | MIT License | SPDX notice at the top of `thirdparty/nlohmann/json.hpp` |
| zlib | PNG support used by SDL3_image: `thirdparty/SDL3_image/external/zlib/` | zlib License | `thirdparty/SDL3_image/external/zlib/LICENSE` |
| libpng | PNG support used by SDL3_image: `thirdparty/SDL3_image/external/libpng/` | PNG Reference Library License | `thirdparty/SDL3_image/external/libpng/LICENSE` |
| libjpeg | JPEG support used by SDL3_image: `thirdparty/SDL3_image/external/jpeg/` | Independent JPEG Group License | `thirdparty/SDL3_image/external/jpeg/COPYING` |
| FreeType 2.13.2 | Font rasterization used by SDL3_ttf: `thirdparty/SDL3_ttf/external/freetype/` | FreeType License | `thirdparty/SDL3_ttf/external/freetype/LICENSE.TXT` |
| HarfBuzz | Text shaping used by SDL3_ttf: `thirdparty/SDL3_ttf/external/harfbuzz/` | Old MIT License | `thirdparty/SDL3_ttf/external/harfbuzz/COPYING` |
| dr_libs | MP3 decoding used by SDL3_mixer: `thirdparty/SDL3_mixer/src/dr_libs/` | Public Domain or MIT-0 | `thirdparty/SDL3_mixer/src/dr_libs/LICENSE` |
| stb_vorbis | Ogg Vorbis decoding used by SDL3_mixer: `thirdparty/SDL3_mixer/src/stb_vorbis/` | Public Domain or MIT License | `thirdparty/SDL3_mixer/src/stb_vorbis/README.txt` |

## Required attributions

This product includes libjpeg software and is based in part on the work of the
Independent JPEG Group.

Portions of this software are copyright © 2023 The FreeType Project
(www.freetype.org). All rights reserved.

## Bundled optional source

The `thirdparty/` directories may also contain source for optional backends or
platform integrations that the default CMake configuration disables. Those
components are not enumerated above as default runtime dependencies. Their
original license and notice files remain in their respective source directories
and continue to govern redistribution of those files.

## Vendored source trimming

Unused upstream tests, examples, and ordinary documentation have been removed
from the SDL3 family, SDL3_gfx, and HarfBuzz snapshots. License, copyright, and
attribution files remain at their original paths, including notices embedded in
README files. FreeType documentation, dependency implementation sources, and
build support files required by this project's CMake configuration are retained.
These snapshots support the project's dependency configuration with upstream
tests and examples disabled; they are not complete upstream development trees.

A second cleanup removes unused upstream release and maintenance scripts,
dependency download scripts, CMake package tests, FreeType tests, HarfBuzz
benchmarks, and libpng/zlib tests and examples. Notices remain at their original
paths, including the libpng examples README. The SDL family retains its
`wikiheaders.pl` tools, and libpng configuration-generation scripts, FreeType
platform build support, and HarfBuzz configuration and source lists are retained.

A third cleanup removes unused upstream VisualC, VisualC-GDK, and Xcode project
material, bundled prebuilt libraries and framework copies, SDL3_gfx screenshots,
and the unbuilt Dear ImGui demo source. License, attribution, and README files
in those directories remain at their original paths. Dependency builds use this
project's CMake configuration; the bundled native project entry points are no
longer provided. Platform implementation sources, Android Java and ProGuard
files, CMake modules and templates, and resources required by the CMake platform
branches are retained, including support for CMake-generated IDE projects.
