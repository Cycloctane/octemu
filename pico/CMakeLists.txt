cmake_minimum_required(VERSION 3.16)

include(pico-sdk/pico_sdk_init.cmake)

project(octemu-pico C CXX)

add_compile_options(-Werror -Wall)

pico_sdk_init()

add_executable(octemu-pico ../core.c sh1106.c octemu_pico.c _rom.c)

pico_set_program_name(octemu-pico "octemu pico")
pico_set_program_description(octemu-pico "Octane's CHIP-8/SUPER-CHIP Emulator on Raspberry Pi Pico")  
pico_set_program_url(octemu-pico "https://github.com/Cycloctane/octemu/pico/")

execute_process(
    COMMAND git describe --always
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE OCTEMU_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
pico_set_program_version(octemu-pico ${OCTEMU_VERSION})

set(OCTEMU_PICO_ROM "" CACHE PATH "Path to CHIP-8 ROM configure json file")
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/_rom.c
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/render_romc.py ${CMAKE_CURRENT_SOURCE_DIR}/_rom.c ${OCTEMU_PICO_ROM}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating _rom.c..."
    VERBATIM)

set(OCTEMU_PICO_ROTATE_SCREEN OFF CACHE BOOL "Flip the screen (upside down)")
if(OCTEMU_PICO_ROTATE_SCREEN)
    target_compile_definitions(octemu-pico PRIVATE SH1106_ROTATE_SCREEN)
endif()

target_link_libraries(octemu-pico PRIVATE pico_stdlib pico_rand hardware_spi hardware_pwm)
target_link_options(octemu-pico PRIVATE -Wl,--print-memory-usage)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(octemu-pico PRIVATE OCTEMU_DEBUG)
    pico_enable_stdio_uart(octemu-pico 1)
endif()

pico_add_extra_outputs(octemu-pico)
