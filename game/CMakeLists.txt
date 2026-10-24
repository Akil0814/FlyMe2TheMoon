file(GLOB_RECURSE GAME CONFIGURE_DEPENDS "*.cpp")

add_library(game_lib ${GAME})

target_include_directories(game_lib PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(game_lib PUBLIC engine_lib)

# Propagate objects directly to executables so registration-only units are not
# discarded as unreferenced members of a static library.
target_link_libraries(game_lib)
