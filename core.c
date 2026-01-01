#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"

#define ins_x ((ins & 0xF00) >> 8)
#define ins_y ((ins & 0xF0) >> 4)
#define ins_nn (ins & 0xFF)
#define ins_nnn (ins & 0xFFF)

const uint8_t OctEmu_Keypad[16] = {
    0x1, 0x2, 0x3, 0xC,
    0x4, 0x5, 0x6, 0xD,
    0x7, 0x8, 0x9, 0xE,
    0xA, 0x0, 0xB, 0xF
};

static const uint8_t sprites[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,
    0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0,
    0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10,
    0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0,
    0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0,
    0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x90, 0xF0, 0x90, 0x90,
    0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0,
    0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0,
    0xF0, 0x80, 0xF0, 0x80, 0x80
};

OctEmu *octemu_new() {
    OctEmu *emu = malloc(sizeof(OctEmu));
    if (!emu)
        fputs("Failed to create OctEmu\n", stderr);
    else
        octemu_reset(emu);
    return emu;
}

void octemu_reset(OctEmu *emu) {
    memset(emu, 0, sizeof(OctEmu));
    memcpy(emu->mem, sprites, sizeof(sprites));
    emu->pc = 0x200;
}

void octemu_free(OctEmu *emu) { free(emu); }

static inline int arithmetic_eval(OctEmu *emu, const uint16_t ins) {
    // assert(ins >> 12 == 0x8);
    uint8_t *vx = &emu->v[ins_x], *vy = &emu->v[ins_y], flag = 0;
    switch (ins & 0xF) {
    case 0: // mov vx, vy
        *vx = *vy;
        return 0;
    case 0x1: // or vx, vy
        *vx |= *vy;
        break;
    case 0x2: // and vx, vy
        *vx &= *vy;
        break;
    case 0x3: // xor vx, vy
        *vx ^= *vy;
        break;
    case 0x4: // add vx, vy
        flag = (int)*vx + (int)*vy > UINT8_MAX;
        *vx += *vy;
        break;
    case 0x5: // sub vx, vy
        flag = *vx >= *vy;
        *vx -= *vy;
        break;
    case 0x6: // shr vx
        flag = *vy & 1;
        *vx = *vy >> 1;
        break;
    case 0x7: // subn vx, vy
        flag = *vy >= *vx;
        *vx = *vy - *vx;
        break;
    case 0xE: // shl vx, vy
        flag = *vy >> 7;
        *vx = *vy << 1;
        break;
    default:
        return 1;
    }
    emu->v[0xF] = flag;
    return 0;
}

int octemu_eval(OctEmu *emu, const uint16_t keystroke) {
    if (emu->pc > OCTEMU_MEM_SIZE - 2) {
        fprintf(stderr, "PC memory access out of bound: 0x%.4X\n", emu->pc);
        return 1;
    }
    const uint16_t ins = emu->mem[emu->pc] << 8 | emu->mem[emu->pc + 1];
    emu->pc += 2;
    switch (ins >> 12) {
    case 0:
        if (ins >> 8)
            goto err_invalid_ins;
        switch (ins & 0xFF) {
        case 0xE0: // cls
            memset(emu->gfx, 0, sizeof(emu->gfx));
            break;
        case 0xEE: // ret
            if (!emu->sp) {
                fputs("Return from empty stack\n", stderr);
                return 1;
            }
            emu->pc = emu->stack[--emu->sp];
            break;
        default:
            goto err_invalid_ins;
        }
        break;
    case 0x1: // jmp nnn
        emu->pc = ins_nnn;
        break;
    case 0x2: // call nnn
        if (emu->sp >= OCTEMU_STACK_SIZE) {
            fputs("Stack Overflow\n", stderr);
            return 1;
        }
        emu->stack[emu->sp++] = emu->pc;
        emu->pc = ins_nnn;
        break;
    case 0x3: // se vx, nn
        if (emu->v[ins_x] == ins_nn)
            emu->pc += 2;
        break;
    case 0x4: // sne vx, nn
        if (emu->v[ins_x] != ins_nn)
            emu->pc += 2;
        break;
    case 0x5: // se vx, vy
        if (ins & 0xF)
            goto err_invalid_ins;
        if (emu->v[ins_x] == emu->v[ins_y])
            emu->pc += 2;
        break;
    case 0x6: // mov vx, nn
        emu->v[ins_x] = ins_nn;
        break;
    case 0x7: // add vx, nn
        emu->v[ins_x] += ins_nn;
        break;
    case 0x8:
        if (arithmetic_eval(emu, ins))
            goto err_invalid_ins;
        break;
    case 0x9: // sne vx, vy
        if (ins & 0xF)
            goto err_invalid_ins;
        if (emu->v[ins_x] != emu->v[ins_y])
            emu->pc += 2;
        break;
    case 0xA: // mov I, nnn
        emu->i = ins_nnn;
        break;
    case 0xB: // jmp v0+nnn
        emu->pc = ins_nnn + emu->v[0];
        break;
    case 0xC: // rnd vx, nn
        emu->v[ins_x] = ins_nn & rand() & 0xFF;
        break;
    case 0xD: { // mov gfx(vx, vy..), [I]..[I+n-1]
        const uint8_t vx = emu->v[ins_x], vy = emu->v[ins_y], n = ins & 0xF;
        if (emu->i > OCTEMU_MEM_SIZE - n)
            goto err_i_memory;
        uint8_t col = (vx & (OCTEMU_GFX_WIDTH - 1)) >> 3, col2, r = vx & 7; // vx%WIDTH/8, vx%8
        if (r)
            col2 = (col + 1) & ((OCTEMU_GFX_WIDTH >> 3) - 1); // (col+1)%(WIDTH/8)
        emu->v[0xF] = 0;
        for (uint8_t i = 0; i < n; i++) {
            uint8_t *row = emu->gfx[(vy + i) & (OCTEMU_GFX_HEIGHT - 1)]; // (vy+i)%HEIGHT
            uint8_t val = emu->mem[emu->i + i] >> r;
            emu->v[0xF] |= (val & row[col]) != 0;
            row[col] ^= val;
            if (r) {
                val = emu->mem[emu->i + i] << (8 - r);
                emu->v[0xF] |= (val & row[col2]) != 0;
                row[col2] ^= val;
            }
        }
        break;
    }
    case 0xE:
        switch (ins & 0xFF) {
        case 0x9E: // se vx, key
            if ((emu->v[ins_x] < 16) && (keystroke & 1 << emu->v[ins_x]))
                emu->pc += 2;
            break;
        case 0xA1: // sne vx, key
            if ((emu->v[ins_x] < 16) && !(keystroke & 1 << emu->v[ins_x]))
                emu->pc += 2;
            break;
        default:
            goto err_invalid_ins;
        }
        break;
    case 0xF:
        switch (ins & 0xFF) {
        case 0x07: // mov vx, delay
            emu->v[ins_x] = emu->delay;
            break;
        case 0x0A: // mov vx, key
            if (emu->keypad & ~keystroke) { // detect released
                for (int i = 0; i < 16; i++) {
                    if (emu->keypad & ~keystroke & 1 << i) {
                        emu->v[ins_x] = i;
                        break;
                    }
                }
            } else
                emu->pc -= 2;
            break;
        case 0x15: // mov delay, vx
            emu->delay = emu->v[ins_x];
            break;
        case 0x18: // mov sound, vx
            emu->sound = emu->v[ins_x];
            break;
        case 0x1E: // mov I, I+vx
            emu->i += emu->v[ins_x];
            break;
        case 0x29: // mov I, &sprite(vx)
            emu->i = 0 + (emu->v[ins_x] & 0xF) * 5;
            break;
        case 0x33: // mov [I]..[I+2], bcd(vx)
            if (emu->i >= OCTEMU_MEM_SIZE - 2 || emu->i < 0x200)
                goto err_i_memory;
            uint8_t remain = emu->v[ins_x];
            emu->mem[emu->i] = remain / 100;
            remain -= emu->mem[emu->i] * 100;
            emu->mem[emu->i + 1] = remain / 10;
            remain -= emu->mem[emu->i + 1] * 10;
            emu->mem[emu->i + 2] = remain;
            break;
        case 0x55: // mov [I], v0..vx
            if (emu->i >= OCTEMU_MEM_SIZE - ins_x || emu->i < 0x200)
                goto err_i_memory;
            memcpy(emu->mem + emu->i, emu->v, (ins_x + 1) * sizeof(uint8_t));
            emu->i += ins_x + 1;
            break;
        case 0x65: // mov v0..vx, [I]
            if (emu->i >= OCTEMU_MEM_SIZE - ins_x)
                goto err_i_memory;
            memcpy(emu->v, emu->mem + emu->i, (ins_x + 1) * sizeof(uint8_t));
            emu->i += ins_x + 1;
            break;
        default:
            goto err_invalid_ins;
        }
        break;
    }
    emu->keypad = keystroke;
    return 0;

err_invalid_ins:
    fprintf(stderr, "Invalid instruction %.4X at 0x%.3X\n", ins, emu->pc - 2);
    return 1;

err_i_memory:
    fprintf(stderr, "I memory access out of bound: 0x%.4X\n", emu->i);
    return 1;
}

uint16_t octemu_peek_ins(const OctEmu *emu) {
    if (emu->pc > OCTEMU_MEM_SIZE - 2)
        return 0;
    return emu->mem[emu->pc] << 8 | emu->mem[emu->pc + 1];
}

void octemu_tick(OctEmu *emu) {
    if (emu->delay)
        --emu->delay;
    if (emu->sound)
        --emu->sound;
}

int octemu_load_rom(OctEmu *emu, const char *rom_path) {
    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open ROM file %s: %s\n", rom_path, strerror(errno));
        return 1;
    }
    fseek(f, 0, SEEK_END);
    if (ftell(f) > OCTEMU_MEM_SIZE - 0x200) {
        fclose(f);
        fputs("ROM too large\n", stderr);
        return 1;
    }
    rewind(f);
    fread(emu->mem + 0x200, sizeof(uint8_t), OCTEMU_MEM_SIZE - 0x200, f);
    fclose(f);
    return 0;
}
