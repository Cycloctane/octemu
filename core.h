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
} OctEmu;

OctEmu *octemu_new();
void octemu_reset(OctEmu *);
void octemu_free(OctEmu *);
int octemu_eval(OctEmu *, const uint16_t);
void octemu_tick(OctEmu *);
int octemu_load_rom(OctEmu *, const char *);
uint16_t octemu_peek_ins(const OctEmu *);

#endif // _OCTEMU_CORE_H_
