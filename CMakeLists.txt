cmake_minimum_required(VERSION 3.16)
project(octemu C)

add_compile_options(-Werror -Wall)

option(OCTEMU_SDL_SHARED "Use shared SDL3 library" OFF)
if(OCTEMU_SDL_SHARED)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
else()
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
endif()

set(OCTEMU_SDL_PATH "" CACHE PATH "Path to vendored SDL3 library")
if(OCTEMU_SDL_PATH)
    include(sdl-settings.cmake)
    add_subdirectory(${OCTEMU_SDL_PATH} EXCLUDE_FROM_ALL)
else()
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
endif()

if(EMSCRIPTEN)
    add_executable(octemu core.c wasm/octemu_wasm.c)
    target_link_options(octemu PRIVATE -sASYNCIFY -sEXPORTED_RUNTIME_METHODS=ccall,cwrap)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/wasm/index.html DESTINATION ${CMAKE_BINARY_DIR}/)
else()
    add_executable(octemu core.c octemu.c)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(octemu PRIVATE OCTEMU_DEBUG)
endif()

execute_process(
    COMMAND git describe --always --tags
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE OCTEMU_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
target_compile_definitions(octemu PRIVATE OCTEMU_VERSION="${OCTEMU_VERSION}")

if(OCTEMU_SDL_SHARED)
    target_link_libraries(octemu PRIVATE SDL3::SDL3-shared)
else()
    target_link_options(octemu PRIVATE -Wl,--gc-sections)
    target_link_libraries(octemu PRIVATE SDL3::SDL3-static)
endif()
