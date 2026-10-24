cmake_minimum_required(VERSION 3.24)

# CMake 3.25+ also applies this during the initial compiler checks.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<IF:$<CONFIG:Debug>,Embedded,$<$<CONFIG:RelWithDebInfo>:ProgramDatabase>>")
endif()

project(FlyMe2TheMoon LANGUAGES C CXX)

if(MSVC)
    # CMake 3.24 stores /Zi in flags instead of using the debug format property.
    foreach(language C CXX)
        string(REGEX REPLACE "/Z[iI]([ \t]|$)" "/Z7\\1"
            CMAKE_${language}_FLAGS_DEBUG "${CMAKE_${language}_FLAGS_DEBUG}")
    endforeach()
endif()

add_subdirectory(thirdparty/box2d EXCLUDE_FROM_ALL)
add_subdirectory(thirdparty/enet EXCLUDE_FROM_ALL  )

# ENet 1.3.18 links these libraries for MinGW itself. Add the equivalent
# transitive dependencies for the bundled MSVC build.

if(MSVC)
    target_link_libraries(enet PUBLIC ws2_32 winmm)
endif()

option(ELYSIA_ENABLE_ASAN "Enable AddressSanitizer for project targets" OFF)
option(ELYSIA_ENABLE_IMGUI "Build the optional Dear ImGui development overlay" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

include(cmake/SDLDependencies.cmake)

if(ELYSIA_ENABLE_IMGUI)
    add_subdirectory(thirdparty/imgui)
endif()

add_subdirectory(engine)
add_subdirectory(game)


if(ELYSIA_ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
    check_cxx_source_compiles("int main() { return 0; }" ELYSIA_ASAN_AVAILABLE)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(NOT ELYSIA_ASAN_AVAILABLE)
        message(FATAL_ERROR "ELYSIA_ENABLE_ASAN=ON, but this compiler has no usable AddressSanitizer runtime.")
    endif()
    foreach(ELYSIA_ASAN_TARGET engine_lib game_lib)
        target_compile_options(${ELYSIA_ASAN_TARGET} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${ELYSIA_ASAN_TARGET} INTERFACE -fsanitize=address)
    endforeach()
endif()


add_executable(FlyMe2TheMoon "main.cpp")

target_link_libraries(FlyMe2TheMoon PRIVATE game_lib)

elysia_copy_sdl_runtime(FlyMe2TheMoon)

set_property(TARGET FlyMe2TheMoon PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
