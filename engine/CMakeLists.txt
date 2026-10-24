file(GLOB_RECURSE ENGINE CONFIGURE_DEPENDS "*.cpp")

if(NOT ELYSIA_ENABLE_IMGUI)
    list(FILTER ENGINE EXCLUDE REGEX "/tools/imgui/.*\\.cpp$")
endif()

add_library(engine_lib ${ENGINE})

target_include_directories(engine_lib PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(engine_lib PUBLIC ${ELYSIA_SDL_LIBRARIES} PRIVATE box2d enet)

if(ELYSIA_ENABLE_IMGUI)
    target_link_libraries(engine_lib PUBLIC imgui_lib)
    target_compile_definitions(engine_lib PUBLIC ELYSIA_ENABLE_IMGUI=1)
else()
    target_compile_definitions(engine_lib PUBLIC ELYSIA_ENABLE_IMGUI=0)
endif()
