# Dear ImGui vendoring note

- Upstream: <https://github.com/ocornut/imgui>
- Release: `v1.92.9`
- Commit: `01380c5`
- License: MIT; see `LICENSE.txt`

ElysiaEngine compiles `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
`imgui_widgets.cpp`, `imgui_impl_sdl2.cpp`, and
`imgui_impl_sdlrenderer2.cpp` only when `ELYSIA_ENABLE_IMGUI=ON`.
`imgui_demo.cpp` is retained as unmodified upstream reference source but is not
part of `imgui_lib`.

