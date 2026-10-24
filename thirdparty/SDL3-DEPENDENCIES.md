# SDL3 dependency sources

All dependencies below are checked-in source snapshots. CMake configures them locally; it does not clone submodules or download codecs. SDL3 is shared; image, ttf, mixer, gfx and their selected codecs are static. MSVC uses the dynamic CRT (Debug /MDd, Release /MD) consistently.

## Official releases

| Directory | Release source | Archive SHA-256 |
| --- | --- | --- |
| `SDL3` | [3.4.16](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.16) | `7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68` |
| `SDL3_image` | [3.4.6](https://github.com/libsdl-org/SDL_image/releases/tag/release-3.4.6) | `d2e4637ae700f72e5196b8fbd749850ed2e5e1e09c5a5be8d06ff55aaccf3b01` |
| `SDL3_ttf` | [3.2.2](https://github.com/libsdl-org/SDL_ttf/releases/tag/release-3.2.2) | `63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d` |
| `SDL3_mixer` | [3.2.4](https://github.com/libsdl-org/SDL_mixer/releases/tag/release-3.2.4) | `182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901` |

## Fixed codec snapshots

The following commits are the submodule revisions recorded by the corresponding official extension release. Their source contents are present directly in `external/`, not as Git submodules.

| Dependency | Directory | Commit |
| --- | --- | --- |
| [freetype](https://github.com/libsdl-org/freetype/tree/9973564cfa63763a3e4ac67c09147899539b1e07) | `SDL3_ttf/external/freetype` | `9973564cfa63763a3e4ac67c09147899539b1e07` |
| [harfbuzz](https://github.com/libsdl-org/harfbuzz/tree/564bf9818a18709776856533829c0c04950773d6) | `SDL3_ttf/external/harfbuzz` | `564bf9818a18709776856533829c0c04950773d6` |
| [jpeg](https://github.com/libsdl-org/jpeg/tree/9db4612eee1913e689f58ce62cd3b8708b290fd3) | `SDL3_image/external/jpeg` | `9db4612eee1913e689f58ce62cd3b8708b290fd3` |
| [libpng](https://github.com/libsdl-org/libpng/tree/4b9e071b2fc4216369372a3ed260d335dd036f15) | `SDL3_image/external/libpng` | `4b9e071b2fc4216369372a3ed260d335dd036f15` |
| [zlib](https://github.com/libsdl-org/zlib/tree/0e68590d11e618d60866aa86629fbda128bc068a) | `SDL3_image/external/zlib` | `0e68590d11e618d60866aa86629fbda128bc068a` |

PNG/JPEG use libpng, zlib and IJG JPEG. Fonts use FreeType and HarfBuzz. WAV, Ogg Vorbis and MP3 use the wave, stb_vorbis and dr_mp3 sources shipped inside the fixed mixer release; dr_flac is also available. Optional codecs whose external source is absent are explicitly disabled. Do not run the upstream dependency download scripts as part of normal configuration.

## SDL3_gfx

Source: [sabdul-khabir/SDL3_gfx](https://github.com/sabdul-khabir/SDL3_gfx/tree/0bbee988bb0caa3e98a9d78c7a2d106925c8275a), commit `0bbee988bb0caa3e98a9d78c7a2d106925c8275a`.

The project-owned `elysia_sdl_gfx` target builds the primitives translation unit only. Local modification: the bitmap-font implementation and its rotozoom include are omitted from `SDL3_gfxPrimitives.c`; Elysia uses SDL_ttf for text. The circle and rounded-box implementations are unchanged. Upstream build files and other source files are retained for provenance but are not built. Bitmap-font and rotozoom APIs are not part of this target.

## Dear ImGui

Version 1.92.9 is retained. `imgui_impl_sdl3.{h,cpp}` and `imgui_impl_sdlrenderer3.{h,cpp}` come from the matching [v1.92.9 tag](https://github.com/ocornut/imgui/tree/v1.92.9/backends). `ELYSIA_ENABLE_IMGUI` controls the platform and renderer integration together.

## Import and licenses

Original license files remain within each source directory. Official archives were extracted on Windows; unused Xcode framework skeleton symlinks and the HarfBuzz README symlink could not be created. These are not inputs to the CMake build. No SDL3, image, ttf, mixer or codec implementation patches are applied. Elysia's explicit stroke mesh ordering avoids a software-renderer rectangle optimization that truncates scaled hairlines; this change is in engine code.
