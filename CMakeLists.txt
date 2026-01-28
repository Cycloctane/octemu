cmake_minimum_required(VERSION 3.16)
project(octemu C)

add_compile_options(-Werror -Wall)

add_executable(octemu core.c octemu.c)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(octemu PRIVATE OCTEMU_DEBUG)
endif()

set(OCTEMU_SDL_PATH "" CACHE PATH "Path to vendored SDL3 library")

if(OCTEMU_SDL_PATH)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)

    # Disable unused subsystems
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
    set(SDL_POWER OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "" FORCE)
    add_subdirectory(${OCTEMU_SDL_PATH} EXCLUDE_FROM_ALL)
    target_link_options(octemu PRIVATE -Wl,--gc-sections)
    target_link_libraries(octemu PRIVATE SDL3::SDL3-static)
else()
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
    target_link_libraries(octemu PRIVATE SDL3::SDL3)
endif()
