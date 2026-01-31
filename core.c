#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"

#define ins_x ((ins & 0xF00) >> 8)
#define ins_y ((ins & 0xF0) >> 4)
#define ins_n (ins & 0xF)
#define ins_nn (ins & 0xFF)
#define ins_nnn (ins & 0xFFF)
#define chip8_mode (emu->mode == OCTEMU_MODE_CHIP8)
#define schip_mode (emu->mode == OCTEMU_MODE_SCHIP)
#define octo_mode (emu->mode == OCTEMU_MODE_OCTO)

const uint8_t OctEmu_Keypad[16] = {
    0x1, 0x2, 0x3, 0xC,
    0x4, 0x5, 0x6, 0xD,
    0x7, 0x8, 0x9, 0xE,
    0xA, 0x0, 0xB, 0xF
};

static const uint8_t sprites[80] = {
    0x60, 0xA0, 0xA0, 0xA0, 0xC0,
    0x40, 0xC0, 0x40, 0x40, 0xE0,
    0xC0, 0x20, 0x40, 0x80, 0xE0,
    0xC0, 0x20, 0x40, 0x20, 0xC0,
    0x20, 0xA0, 0xE0, 0x20, 0x20,
    0xE0, 0x80, 0xC0, 0x20, 0xC0,
    0x40, 0x80, 0xC0, 0xA0, 0x40,
    0xE0, 0x20, 0x60, 0x40, 0x40,
    0x40, 0xA0, 0x40, 0xA0, 0x40,
    0x40, 0xA0, 0x60, 0x20, 0x40,
    0x40, 0xA0, 0xE0, 0xA0, 0xA0,
    0xC0, 0xA0, 0xC0, 0xA0, 0xC0,
    0x60, 0x80, 0x80, 0x80, 0x60,
    0xC0, 0xA0, 0xA0, 0xA0, 0xC0,
    0xE0, 0x80, 0xC0, 0x80, 0xE0,
    0xE0, 0x80, 0xC0, 0x80, 0x80,
};

static const uint8_t sprites_hr[160] = { // Octo's 0-F big fonts
    0x7C, 0xC6, 0xCE, 0xDE, 0xD6, 0xF6, 0xE6, 0xC6, 0x7C, 0x00,
    0x10, 0x30, 0xF0, 0x30, 0x30, 0x30, 0x30, 0x30, 0xFC, 0x00,
    0x78, 0xCC, 0xCC, 0x0C, 0x18, 0x30, 0x60, 0xCC, 0xFC, 0x00,
    0x78, 0xCC, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0xCC, 0x78, 0x00,
    0x0C, 0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x0C, 0x1E, 0x00,
    0xFC, 0xC0, 0xC0, 0xC0, 0xF8, 0x0C, 0x0C, 0xCC, 0x78, 0x00,
    0x38, 0x60, 0xC0, 0xC0, 0xF8, 0xCC, 0xCC, 0xCC, 0x78, 0x00,
    0xFE, 0xC6, 0xC6, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00,
    0x78, 0xCC, 0xCC, 0xEC, 0x78, 0xDC, 0xCC, 0xCC, 0x78, 0x00,
    0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x18, 0x18, 0x30, 0x70, 0x00,
    0x30, 0x78, 0xCC, 0xCC, 0xCC, 0xFC, 0xCC, 0xCC, 0xCC, 0x00,
    0xFC, 0x66, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x66, 0xFC, 0x00,
    0x3C, 0x66, 0xC6, 0xC0, 0xC0, 0xC0, 0xC6, 0x66, 0x3C, 0x00,
    0xF8, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00,
    0xFE, 0x62, 0x60, 0x64, 0x7C, 0x64, 0x60, 0x62, 0xFE, 0x00,
    0xFE, 0x66, 0x62, 0x64, 0x7C, 0x64, 0x60, 0x60, 0xF0, 0x00
};

OctEmu *octemu_new(OctEmuMode mode) {
    OctEmu *emu = calloc(1, sizeof(OctEmu));
    if (!emu)
        fputs("Failed to create OctEmu\n", stderr);
    else {
        memcpy(emu->mem, sprites, sizeof(sprites));
        memcpy(emu->mem + sizeof(sprites), sprites_hr, sizeof(sprites_hr));
        emu->mode = mode;
        emu->pc = 0x200;
    }
    return emu;
}

void octemu_reset(OctEmu *emu) {
    emu->i = emu->sp = emu->delay = emu->sound = emu->keypad = 0;
    emu->gfx_dirty = emu->hires = false;
    emu->pc = 0x200;
    memset(emu->v, 0, sizeof(emu->v));
    memset(emu->stack, 0, sizeof(emu->stack));

    memcpy(emu->mem, sprites, sizeof(sprites));
    memcpy(emu->mem + sizeof(sprites), sprites_hr, sizeof(sprites_hr));
    memset(emu->mem + sizeof(sprites) + sizeof(sprites_hr),
           0, 0x200 - sizeof(sprites) - sizeof(sprites_hr));
    if (emu->rom) {
        memcpy(emu->mem + 0x200, emu->rom, emu->rom_size);
        memset(emu->mem + 0x200 + emu->rom_size, 0, OCTEMU_MEM_SIZE - 0x200 - emu->rom_size);
    } else {
        memset(emu->mem + 0x200, 0, OCTEMU_MEM_SIZE - 0x200);
        memset(emu->rpl, 0, sizeof(emu->rpl));
    }
    memset(emu->gfx, 0, sizeof(emu->gfx));
}

void octemu_free(OctEmu *emu) {
    if (emu->rom && !emu->rom_external)
        free(emu->rom);
    free(emu);
}

void octemu_print_states(const OctEmu *emu) {
    fprintf(stderr, "\nPC: 0x%.4X I: 0x%.4X SP: %d\n", emu->pc, emu->i, emu->sp);
    for (int i = 0; i < 8; i++)
        fprintf(stderr, "V%.1X: %.3d ", i, emu->v[i]);
    fputc('\n', stderr);
    for (int i = 8; i < 16; i++)
        fprintf(stderr, "V%.1X: %.3d ", i, emu->v[i]);
    fputs("\nStack:", stderr);
    for (uint8_t i = 0; i < emu->sp; i++)
        fprintf(stderr, " 0x%.4X", emu->stack[i]);
    fprintf(stderr, "\nHires Mode: %d\nGFX_Dirty: %d", emu->hires, emu->gfx_dirty);
    fprintf(stderr, "\nDelay Timer: %d\nSound Timer: %d", emu->delay, emu->sound);
    fputs("\nKeypad State:", stderr);
    for (int b = 0; b < 16; b++) {
        if (emu->keypad & 1 << b)
            fprintf(stderr, " %.1X", b);
    }
    fputs("\n\n", stderr);
}

// 76543210 -> 77665544 33221100
static inline void expand_uint8(const uint8_t val, uint8_t *left, uint8_t *right) {
    for (uint8_t b = 0; b < 4; b++) {
        if (val >> b & 1)
            *right |= 3 << b * 2;
        if (val >> (b + 4) & 1)
            *left |= 3 << b * 2;
    }
}

static inline uint8_t wrap_col(const uint8_t col) { return col & (OCTEMU_GFX_WIDTH / 8 - 1); }
static inline uint8_t wrap_row(const uint8_t row) { return row & (OCTEMU_GFX_HEIGHT - 1); }

static void put_pixels_hr(OctEmu *emu, const uint8_t x_col, const uint8_t y, const uint8_t rows,
                          const uint8_t cols, const uint8_t pixels[][cols]) {
    emu->v[0xF] = 0;
    for (uint8_t r = 0; r < rows; r++) {
        uint8_t *row = emu->gfx[wrap_row(y + r)];
        for (uint8_t c = 0; c < cols; c++) {
            const uint8_t col = wrap_col(x_col + c);
            emu->v[0xF] |= (pixels[r][c] & row[col]) != 0;
            row[col] ^= pixels[r][c];
        }
    }
}

// lowres mode draws 2 * rows
static void put_pixels_lr(OctEmu *emu, const uint8_t x_col, const uint8_t y, const uint8_t rows,
                          const uint8_t cols, const uint8_t pixels[][cols]) {
    emu->v[0xF] = 0;
    for (uint8_t r = 0; r < rows; r++) {
        uint8_t *row1 = emu->gfx[wrap_row(y + r * 2)];
        uint8_t *row2 = emu->gfx[wrap_row(y + r * 2 + 1)];
        for (uint8_t c = 0; c < cols; c++) {
            const uint8_t col = wrap_col(x_col + c);
            emu->v[0xF] |= (pixels[r][c] & row1[col]) != 0;
            row1[col] ^= pixels[r][c];
            row2[col] = row1[col];
        }
    }
}

/**
 *   x
 * |r| mem[i] | 8-r  |
 * |  col   |  col+1 | <- row
 */

static int draw8hr(OctEmu *emu, const uint8_t vx, const uint8_t vy, const uint8_t n) {
    const uint8_t x = vx & (OCTEMU_GFX_WIDTH - 1), y = vy & (OCTEMU_GFX_HEIGHT - 1);
    uint8_t rows, cols = 1;
    if (octo_mode)
        rows = n;
    else { // clip y
        const uint8_t max_row = OCTEMU_GFX_HEIGHT - y;
        rows = n > max_row ? max_row : n;
    }
    if (emu->i > OCTEMU_MEM_SIZE - rows)
        return 1;
    const uint8_t x_col = x >> 3, r = x & 7;
    if ((octo_mode || x_col < OCTEMU_GFX_WIDTH / 8 - 1) && r != 0)
        ++cols;
    uint8_t pixels[rows][cols];
    for (uint8_t i = 0; i < rows; i++) {
        pixels[i][0] = emu->mem[emu->i + i] >> r;
        if (cols < 2) continue;
        pixels[i][1] = emu->mem[emu->i + i] << (8 - r);
    }
    put_pixels_hr(emu, x_col, y, rows, cols, pixels);
    return 0;
}

/**
 *   x
 * |r|  m[i]  | m[i+1] | 8-r  |
 * |  col   |  col+1 |  col+2 | <- row
 */

static int draw16hr(OctEmu *emu, const uint8_t vx, const uint8_t vy) {
    const uint8_t x = vx & (OCTEMU_GFX_WIDTH - 1), y = vy & (OCTEMU_GFX_HEIGHT - 1);
    uint8_t rows, cols;
    if (octo_mode)
        rows = 16;
    else { // clip y
        const uint8_t max_row = OCTEMU_GFX_HEIGHT - y;
        rows = 16 > max_row ? max_row : 16;
    }
    if (emu->i > OCTEMU_MEM_SIZE - rows * 2)
        return 1;
    const uint8_t x_col = x >> 3, r = x & 7;
    if (octo_mode || x_col < OCTEMU_GFX_WIDTH / 8 - 2)
        cols = 2 + (r != 0);
    else
        cols = OCTEMU_GFX_WIDTH / 8 - x_col;
    uint8_t pixels[rows][cols];
    for (uint8_t i = 0; i < rows; i++) {
        const uint8_t left = emu->mem[emu->i + i * 2], right = emu->mem[emu->i + i * 2 + 1];
        pixels[i][0] = left >> r;
        if (cols < 2) continue;
        pixels[i][1] = (left << (8 - r)) | (right >> r);
        if (cols < 3) continue;
        pixels[i][2] = right << (8 - r);
    }
    put_pixels_hr(emu, x_col, y, rows, cols, pixels);
    return 0;
}

/**
 *   x
 *   | 7 6 5 4 3 2 1 0 | <- mem[i]
 *   |77665544|33221100|
 * |r|  left  | right  | 8-r  |
 * |  col   |  col+1 |  col+2 | <- row1
 * |  col   |  col+1 |  col+2 | <- row2
 */

static int draw8lr(OctEmu *emu, const uint8_t vx, const uint8_t vy, const uint8_t n) {
    const uint8_t x = vx * 2 & (OCTEMU_GFX_WIDTH - 1), y = vy * 2 & (OCTEMU_GFX_HEIGHT - 1);
    uint8_t rows, cols;
    if (octo_mode)
        rows = n;
    else {
        const uint8_t max_row = (OCTEMU_GFX_HEIGHT - y) >> 1;
        rows = n > max_row ? max_row : n;
    }
    if (emu->i > OCTEMU_MEM_SIZE - rows)
        return 1;
    const uint8_t x_col = x >> 3, r = x & 7;
    if (octo_mode || x_col < OCTEMU_GFX_WIDTH / 8 - 2)
        cols = 2 + (r != 0);
    else
        cols = OCTEMU_GFX_WIDTH / 8 - x_col;
    uint8_t pixels[rows][cols];
    for (uint8_t i = 0; i < rows; i++) {
        uint8_t left = 0, right = 0;
        expand_uint8(emu->mem[emu->i + i], &left, &right);
        pixels[i][0] = left >> r;
        if (cols < 2) continue;
        pixels[i][1] = left << (8 - r) | right >> r;
        if (cols < 3) continue;
        pixels[i][2] = right << (8 - r);
    }
    put_pixels_lr(emu, x_col, y, rows, cols, pixels);
    return 0;
}

/**
 *   x
 *   |      mem[i]     |     mem[i+1]    |
 *   | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 |
 *   |77665544|33221100|77665544|33221100|
 * |r|  left1 | right1 |  left2 | right2 | 8-r  |
 * |  col   |  col+1 |  col+2 |  col+3 |  col+4 | <- row1
 * |  col   |  col+1 |  col+2 |  col+3 |  col+4 | <- row2
 */

// Why should I write this??? :(
static int draw16lr(OctEmu *emu, const uint8_t vx, const uint8_t vy) {
    const uint8_t x = vx * 2 & (OCTEMU_GFX_WIDTH - 1), y = vy * 2 & (OCTEMU_GFX_HEIGHT - 1);
    uint8_t rows, cols;
    if (octo_mode)
        rows = 16;
    else {
        const uint8_t max_row = (OCTEMU_GFX_HEIGHT - y) >> 1;
        rows = 16 > max_row ? max_row : 16;
    }
    if (emu->i > OCTEMU_MEM_SIZE - rows * 2)
        return 1;
    const uint8_t x_col = x >> 3, r = x & 7;
    if (octo_mode || x_col < OCTEMU_GFX_WIDTH / 8 - 4)
        cols = 4 + (r != 0);
    else
        cols = OCTEMU_GFX_WIDTH / 8 - x_col;
    uint8_t pixels[rows][cols]; // precomputed values
    for (uint8_t i = 0; i < rows; i++) {
        uint8_t left1 = 0, right1 = 0, left2 = 0, right2 = 0;
        expand_uint8(emu->mem[emu->i + i * 2], &left1, &right1);
        expand_uint8(emu->mem[emu->i + i * 2 + 1], &left2, &right2);
        pixels[i][0] = left1 >> r;
        if (cols < 2) continue;
        pixels[i][1] = left1 << (8 - r) | right1 >> r;
        if (cols < 3) continue;
        pixels[i][2] = right1 << (8 - r) | left2 >> r;
        if (cols < 4) continue;
        pixels[i][3] = left2 << (8 - r) | right2 >> r;
        if (cols < 5) continue;
        pixels[i][4] = right2 << (8 - r);
    }
    put_pixels_lr(emu, x_col, y, rows, cols, pixels);
    return 0;
}

static inline int arithmetic_eval(OctEmu *emu, const uint16_t ins) {
    // assert(ins >> 12 == 0x8);
    uint8_t *vx = &emu->v[ins_x], *vy = &emu->v[ins_y], flag;
    switch (ins & 0xF) {
    case 0: // mov vx, vy
        *vx = *vy;
        return 0;
    case 0x1: // or vx, vy
        *vx |= *vy;
        if (chip8_mode)
            emu->v[0xF] = 0;
        break;
    case 0x2: // and vx, vy
        *vx &= *vy;
        if (chip8_mode)
            emu->v[0xF] = 0;
        break;
    case 0x3: // xor vx, vy
        *vx ^= *vy;
        if (chip8_mode)
            emu->v[0xF] = 0;
        break;
    case 0x4: // add vx, vy
        flag = *vx > 0xFF - *vy;
        *vx += *vy;
        emu->v[0xF] = flag;
        break;
    case 0x5: // sub vx, vy
        flag = *vx >= *vy;
        *vx -= *vy;
        emu->v[0xF] = flag;
        break;
    case 0x6:
        if (schip_mode) { // shr vx
            flag = *vx & 1;
            *vx >>= 1;
        } else { // shr vx, vy
            flag = *vy & 1;
            *vx = *vy >> 1;
        }
        emu->v[0xF] = flag;
        break;
    case 0x7: // subn vx, vy
        flag = *vy >= *vx;
        *vx = *vy - *vx;
        emu->v[0xF] = flag;
        break;
    case 0xE:
        if (schip_mode) { // shl vx
            flag = *vx >> 7;
            *vx <<= 1;
        } else { // shl vx, vy
            flag = *vy >> 7;
            *vx = *vy << 1;
        }
        emu->v[0xF] = flag;
        break;
    default:
        return 1;
    }
    return 0;
}

static inline void clear_gfx(OctEmu *emu) { memset(emu->gfx, 0, sizeof(emu->gfx)); }

int octemu_eval(OctEmu *emu, const uint16_t keypad) {
    if (emu->pc > OCTEMU_MEM_SIZE - 2 || emu->pc < 0x200) {
        fprintf(stderr, "PC memory access out of bound: 0x%.4X\n", emu->pc);
        goto err;
    }
    const uint16_t ins = emu->mem[emu->pc] << 8 | emu->mem[emu->pc + 1];
    emu->pc += 2;
    switch (ins >> 12) {
    case 0:
        if (ins >> 8)
            goto err_invalid_ins;
        if (((ins & 0xF0) >> 4) == 0xC && ins_n) {
            const uint8_t n = emu->hires ? ins_n : ins_n * 2;
            for (int y = OCTEMU_GFX_HEIGHT - 1; y >= n; y--)
                memcpy(emu->gfx[y], emu->gfx[y - n], sizeof(emu->gfx[y]));
            memset(emu->gfx, 0, sizeof(emu->gfx[0]) * n);
            emu->gfx_dirty = true;
        } else switch (ins & 0xFF) {
        case 0x00:
            return 1;
        case 0xE0: // cls
            clear_gfx(emu);
            emu->gfx_dirty = true;
            break;
        case 0xEE: // ret
            if (!emu->sp) {
                fputs("Return from empty stack\n", stderr);
                goto err;
            }
            emu->pc = emu->stack[--emu->sp];
            break;
        case 0xFB:
            if (emu->hires) {
                for (int y = 0; y < OCTEMU_GFX_HEIGHT; y++) {
                    uint8_t *row = emu->gfx[y];
                    for (int x = OCTEMU_GFX_WIDTH / 8 - 1; x > 0; x--){
                        row[x] >>= 4;
                        row[x] |= row[x - 1] << 4;
                    }
                    row[0] >>= 4;
                }
            } else {
                for (int y = 0; y < OCTEMU_GFX_HEIGHT; y++) {
                    uint8_t *row = emu->gfx[y];
                    for (int x = OCTEMU_GFX_WIDTH / 8 - 1; x > 0; x--)
                        row[x] = row[x - 1];
                    row[0] = 0;
                }
            }
            emu->gfx_dirty = true;
            break;
        case 0xFC:
            if (emu->hires) {
                for (int y = 0; y < OCTEMU_GFX_HEIGHT; y++) {
                    uint8_t *row = emu->gfx[y];
                    for (int x = 0; x < OCTEMU_GFX_WIDTH / 8 - 1; x++) {
                        row[x] <<= 4;
                        row[x] |= row[x + 1] >> 4;
                    }
                    row[OCTEMU_GFX_WIDTH / 8 - 1] <<= 4;
                }
            } else {
                for (int y = 0; y < OCTEMU_GFX_HEIGHT; y++) {
                    uint8_t *row = emu->gfx[y];
                    for (int x = 0; x < OCTEMU_GFX_WIDTH / 8 - 1; x++)
                        row[x] = row[x + 1];
                    row[OCTEMU_GFX_WIDTH / 8 - 1] = 0;
                }
            }
            emu->gfx_dirty = true;
            break;
        case 0xFD: // exit
            return 1;
        case 0xFE:
            emu->hires = false;
            clear_gfx(emu);
            emu->gfx_dirty = true;
            break;
        case 0xFF:
            emu->hires = true;
            clear_gfx(emu);
            emu->gfx_dirty = true;
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
            goto err;
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
    case 0xB:
        if (schip_mode)
            emu->pc = ins_nnn + emu->v[ins_x]; // jmp vx+xnn
        else
            emu->pc = ins_nnn + emu->v[0]; // jmp v0+nnn
        break;
    case 0xC: // rnd vx, nn
        emu->v[ins_x] = ins_nn & rand();
        break;
    case 0xD: { // mov gfx(vx, vy..), [I]..[I+n-1]
        const uint8_t vx = emu->v[ins_x], vy = emu->v[ins_y], n = ins_n;
        if (!n) {
            if (emu->hires) {
                if (draw16hr(emu, vx, vy))
                    goto err_i_memory;
            } else {
                if (draw16lr(emu, vx, vy))
                    goto err_i_memory;
            }
        } else {
            if (emu->hires) {
                if (draw8hr(emu, vx, vy, n))
                    goto err_i_memory;
            } else {
                if (draw8lr(emu, vx, vy, n))
                    goto err_i_memory;
            }
        }
        emu->gfx_dirty = true;
        break;
    }
    case 0xE:
        switch (ins & 0xFF) {
        case 0x9E: // se vx, key
            if (keypad & 1 << (emu->v[ins_x] & 0xF))
                emu->pc += 2;
            break;
        case 0xA1: // sne vx, key
            if (!(keypad & 1 << (emu->v[ins_x] & 0xF)))
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
            if (emu->keypad & ~keypad) { // detect released
                for (int i = 0; i < 16; i++) {
                    if (emu->keypad & ~keypad & 1 << i) {
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
        case 0x30: // mov I, &sprites_hr(vx)
            emu->i = sizeof(sprites) + (emu->v[ins_x] & 0xF) * 10;
            break;
        case 0x33: // mov [I]..[I+2], bcd(vx)
            if (emu->i > OCTEMU_MEM_SIZE - 3)
                goto err_i_memory;
            uint8_t remain = emu->v[ins_x];
            emu->mem[emu->i] = remain / 100;
            remain -= emu->mem[emu->i] * 100;
            emu->mem[emu->i + 1] = remain / 10;
            remain -= emu->mem[emu->i + 1] * 10;
            emu->mem[emu->i + 2] = remain;
            break;
        case 0x55: // mov [I], v0..vx
            if (emu->i >= OCTEMU_MEM_SIZE - ins_x)
                goto err_i_memory;
            memcpy(emu->mem + emu->i, emu->v, (ins_x + 1) * sizeof(uint8_t));
            if (!schip_mode)
                emu->i += ins_x + 1;
            break;
        case 0x65: // mov v0..vx, [I]
            if (emu->i >= OCTEMU_MEM_SIZE - ins_x)
                goto err_i_memory;
            memcpy(emu->v, emu->mem + emu->i, (ins_x + 1) * sizeof(uint8_t));
            if (!schip_mode)
                emu->i += ins_x + 1;
            break;
        case 0x75: // mov rpl, v0..vx
            memcpy(emu->rpl, emu->v, (ins_x + 1) * sizeof(uint8_t));
            break;
        case 0x85: // mov v0..vx, rpl
            memcpy(emu->v, emu->rpl, (ins_x + 1) * sizeof(uint8_t));
            break;

#ifdef OCTEMU_HCF
        case 0xCF: // hcf
            emu->pc -= 2;
            emu->sound = 0xFF;
            break;
#endif // OCTEMU_HCF

        default:
            goto err_invalid_ins;
        }
        break;
    }
    emu->keypad = keypad;
    return 0;

err_invalid_ins:
    fprintf(stderr, "Invalid instruction %.4X at 0x%.4X\n", ins, emu->pc - 2);
    goto err;

err_i_memory:
    fprintf(stderr, "I memory access out of bound: 0x%.4X\n", emu->i);
    goto err;

err:
#ifdef OCTEMU_DEBUG
    octemu_print_states(emu);
#endif
    return 1;
}

void octemu_tick(OctEmu *emu) {
    if (emu->delay)
        --emu->delay;
    if (emu->sound)
        --emu->sound;
}

int octemu_load_rom_file(OctEmu *emu, const char *rom_path) {
    if (emu->rom) {
        fputs("ROM already loaded\n", stderr);
        return 1;
    }
    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open ROM file %s: %s\n", rom_path, strerror(errno));
        return 1;
    }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    if ((size < 2) || (size > OCTEMU_MEM_SIZE - 0x200)) {
        fclose(f);
        fputs("Invalid ROM size\n", stderr);
        return 1;
    }
    rewind(f);
    emu->rom = malloc(size);
    if (!emu->rom) {
        fclose(f);
        return 1;
    } else if (fread(emu->rom, sizeof(uint8_t), size, f) != size) {
        fputs("Failed to load ROM\n", stderr);
        free(emu->rom);
        emu->rom = NULL;
        fclose(f);
        return 1;
    }
    emu->rom_size = size;
    emu->rom_external = false;
    memcpy(emu->mem + 0x200, emu->rom, emu->rom_size);
    fclose(f);
    return 0;
}

int octemu_load_rom(OctEmu *emu, const uint8_t *rom_data, const size_t size) {
    if (emu->rom) {
        fputs("ROM already loaded\n", stderr);
        return 1;
    }
    if ((size < 2) || (size > OCTEMU_MEM_SIZE - 0x200)) {
        fputs("Invalid ROM size\n", stderr);
        return 1;
    }
    emu->rom = malloc(size);
    if (!emu->rom)
        return 1;
    memcpy(emu->rom, rom_data, size);
    emu->rom_size = size;
    emu->rom_external = false;
    memcpy(emu->mem + 0x200, emu->rom, emu->rom_size);
    return 0;
}

int octemu_set_rom(OctEmu *emu, const uint8_t *rom_data, const size_t size) {
    if (emu->rom) {
        fputs("ROM already loaded\n", stderr);
        return 1;
    }
    if ((size < 2) || (size > OCTEMU_MEM_SIZE - 0x200)) {
        fputs("Invalid ROM size\n", stderr);
        return 1;
    }
    emu->rom = (uint8_t *)rom_data;
    emu->rom_size = size;
    emu->rom_external = true;
    memcpy(emu->mem + 0x200, emu->rom, emu->rom_size);
    return 0;
}

void octemu_clear_rom(OctEmu *emu) {
    if (emu->rom) {
        if (!emu->rom_external)
            free(emu->rom);
        emu->rom = NULL;
        emu->rom_size = 0;
    }
    octemu_reset(emu);
}
