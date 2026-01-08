#ifndef _OCTEMU_CORE_H_
#define _OCTEMU_CORE_H_

#include <stdbool.h>
#include <stdint.h>

#define OCTEMU_STACK_SIZE 16
#define OCTEMU_MEM_SIZE 4096
#define OCTEMU_GFX_WIDTH 128
#define OCTEMU_GFX_HEIGHT 64

extern const uint8_t OctEmu_Keypad[16];

typedef enum OctEmuMode {
    OCTEMU_MODE_CHIP8,
    OCTEMU_MODE_SCHIP,
    OCTEMU_MODE_OCTO
} OctEmuMode;

typedef struct OctEmu {
    // mode
    OctEmuMode mode;
    // registers
    uint16_t pc, i;
    uint8_t v[0x10], sp;
    // timers
    uint8_t delay, sound;
    // states
    bool hires, gfx_dirty;
    uint16_t keypad;
    // memory
    uint16_t stack[OCTEMU_STACK_SIZE];
    uint8_t mem[OCTEMU_MEM_SIZE];
    uint8_t gfx[OCTEMU_GFX_HEIGHT][OCTEMU_GFX_WIDTH / 8];
    uint8_t rpl[0x10];
    // ROM
    uint16_t rom_size;
    uint8_t *rom;
} OctEmu;

OctEmu *octemu_new(OctEmuMode);
void octemu_free(OctEmu *);

/* Reset emulator states and reload ROM (or empty the memory if no ROM loaded). */
void octemu_reset(OctEmu *);

/**
 * Execute one instruction cycle.
 * @param keypad Current keypad state bitmask
 * @return 0 on success, 1 if any error occurs
 */
int octemu_eval(OctEmu *, const uint16_t keypad);

/* Decrease internal timers by one. */
void octemu_tick(OctEmu *);

/**
 * Load a ROM from file path and copy into emulator memory.
 * Cannot load if a ROM is already loaded.
 * @param rom_path Path to the ROM file
 * @return 0 on success, 1 on failure
 */
int octemu_load_rom_file(OctEmu *, const char *rom_path);

/**
 * Load a ROM from buffer and copy into emulator memory.
 * Cannot load if a ROM is already loaded.
 * @param rom_data Pointer to the ROM data buffer
 * @param size Size of the ROM data buffer
 * @return 0 on success, 1 on failure
 */
int octemu_load_rom(OctEmu *, const uint8_t *rom_data, const size_t size);

/* Clear current ROM. Also reset emulator states and memory. */
void octemu_clear_rom(OctEmu *);

/* Print emulator's current internal states (to stderr). */
void octemu_print_states(const OctEmu *);

#endif // _OCTEMU_CORE_H_
