#ifndef _OCTEMU_CORE_H_
#define _OCTEMU_CORE_H_

#include <stdint.h>

#define OCTEMU_STACK_SIZE 16
#define OCTEMU_MEM_SIZE 4096
#define OCTEMU_GFX_WIDTH 64
#define OCTEMU_GFX_HEIGHT 32

extern const uint8_t OctEmu_Keypad[16];

typedef struct OctEmu {
    // registers
    uint8_t v[0x10], sp;
    uint16_t pc, i;
    // timers
    uint8_t delay, sound;
    // states
    uint16_t keypad;
    // memory
    uint16_t stack[OCTEMU_STACK_SIZE];
    uint8_t mem[OCTEMU_MEM_SIZE];
    uint8_t gfx[OCTEMU_GFX_HEIGHT][OCTEMU_GFX_WIDTH / 8];
    // ROM
    uint8_t *rom;
    uint16_t rom_size;
} OctEmu;

OctEmu *octemu_new();
void octemu_free(OctEmu *);

/* Reset emulator states and reload ROM (or empty the memory if no ROM loaded). */
void octemu_reset(OctEmu *);

/**
 * Execute one instruction cycle.
 * @param keystroke Current keypad state bitmask
 * @return 0 on success, 1 if any error occurs
 */
int octemu_eval(OctEmu *, const uint16_t keystroke);

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

/* Get the next instruction without increasing the PC. Return 0 if failed. */
uint16_t octemu_peek_ins(const OctEmu *);

#endif // _OCTEMU_CORE_H_
