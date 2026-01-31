#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/rand.h"

#include "sh1106.h"
#include "fonts.h"
#include "rom_config.h"
#include "../core.h"

#ifdef OCTEMU_DEBUG 
#include <stdio.h>
#define UART_BAUDRATE 115200
#endif

#define OCTEMU_PICO_PAUSE_GPIO 14
#define OCTEMU_PICO_RESET_GPIO 15
#define OCTEMU_PICO_QUIT_GPIO 16

#define RUNNING 1
#define PAUSED 2
#define HALTED 3

extern const OctEmuRom emu_roms[];
extern const uint emu_roms_count; 

static sh1106 *display = NULL;

static inline void start_sound() {
    pwm_set_enabled(pwm_gpio_to_slice_num(20), true);
}

static inline void stop_sound() {
    pwm_set_enabled(pwm_gpio_to_slice_num(20), false);
}

static inline void gpio_init_pulldown(const uint gpio) {
    gpio_init(gpio);
    gpio_pull_down(gpio);
}

static bool wait_key(const uint gpio) {
    if (!gpio_get(gpio))
        return false;
    while (gpio_get(gpio))
        sleep_ms(1);
    return true;
}

static uint16_t read_keypad() {
    uint16_t ret = 0;
    for (uint row = 0; row < 4; row++) {
        gpio_put(6 + row, true);
        sleep_us(1);
        for (uint col = 0; col < 4; col++) {
            if (gpio_get(10 + col)) {
                ret |= (1 << OctEmu_Keypad[row * 4 + col]);
                break;
            }
        }
        gpio_put(6 + row, false);
    }
    return ret;
}

// :(
static void convert_vram(const OctEmu *emu, sh1106 *display) {
    for (uint page = 0; page < 8; page++) {
        uint8_t dirty = 0;
        for (uint col = 0; col < 128; col++) {
            uint8_t byte = 0;
            for (uint bit = 0; bit < 8; bit++) {
                const uint8_t pixel = (emu->gfx[page * 8 + bit][col / 8] >> (7 - (col % 8))) & 1;
                byte |= pixel << bit;
            }
            if (display->vram[page][col] != byte) {
                dirty = 1;
                display->vram[page][col] = byte;
            }
        }
        display->page_dirty |= dirty << page;
    }
}

static inline OctEmuMode str2mode(const char *mode_str) {
    if (strcmp(mode_str, "chip8") == 0)
        return OCTEMU_MODE_CHIP8;
    else if (strcmp(mode_str, "schip") == 0)
        return OCTEMU_MODE_SCHIP;
    else if (strcmp(mode_str, "octo") == 0)
        return OCTEMU_MODE_OCTO;
    else
        return OCTEMU_MODE_OCTO; // default
}

static const OctEmuRom *menu(uint *pos) {
    // assert(display);

    memcpy(&display->vram[7][4], left_arrow, sizeof(left_arrow));
    memcpy(&display->vram[7][60], point, sizeof(point));
    memcpy(&display->vram[7][112], right_arrow, sizeof(right_arrow));
    display->page_dirty |= 1 << 7;
    sh1106_write(display);

    while (1) {
        const char *title = emu_roms[*pos].title;
        const uint width = strlen(title) * 8;
        uint8_t col = width < 128 ? (128 - width) / 2 : 0;
        memset(&display->vram[2], 0, sizeof(display->vram[2]));
        memset(&display->vram[3], 0, sizeof(display->vram[3]));
        for (const char *c = title; *c && col <= 120; c++, col += 8) {
            if (*c < ' ' || *c > '~')
                continue;
            const uint8_t *font_char = Font1608[*c - ' '];
            memcpy(&display->vram[2][col], font_char, 8);
            memcpy(&display->vram[3][col], font_char + 8, 8);
        }
        display->page_dirty |= 1 << 2 | 1 << 3;
        sh1106_write(display);
        uint16_t prev_keypad = 0;
        while (1) {
            const uint16_t keypad = read_keypad();
            if (prev_keypad & 1 << 1 && !(keypad & 1 << 1)) {
                *pos = *pos ? *pos - 1 : emu_roms_count - 1;
                break;
            } else if (prev_keypad & 1 << 3 && !(keypad & 1 << 3)) {
                *pos = *pos < emu_roms_count - 1 ? *pos + 1 : 0;
                break;
            } else if (prev_keypad & 1 << 2 && !(keypad & 1 << 2))
                return &emu_roms[*pos];
            prev_keypad = keypad;
            sleep_ms(40);
        }
    }
}

// emulator main loop
static void emu_loop(OctEmu *emu, const unsigned int tickrate) {
    // assert(display);
    uint s = RUNNING;
    bool sound = false;

    while (!wait_key(OCTEMU_PICO_QUIT_GPIO)) {
        if (wait_key(OCTEMU_PICO_PAUSE_GPIO)) {
            if (s == RUNNING)
                s = PAUSED;
            else if (s == PAUSED)
                s = RUNNING;
        } else if (wait_key(OCTEMU_PICO_RESET_GPIO)) {
            stop_sound();
            octemu_reset(emu);
            sh1106_clear(display);
            s = RUNNING;
            sound = false;
        }

        if (s == PAUSED || s == HALTED) {
            sleep_ms(100);
            continue;
        }

        int err = 0;
        const uint16_t keypad = read_keypad();
        for (uint i = 0; i < tickrate; i++) {
            err = octemu_eval(emu, keypad);
            if (err)
                break;
        }

        if (err) {
            if (sound){
                stop_sound();
                sound = false;
            }
            s = HALTED;
#ifdef OCTEMU_DEBUG
            puts("Emulator halted...");
#endif
            continue;
        }

        if (emu->sound && !sound) {
            start_sound();
            sound = true;
        } else if (!emu->sound && sound) {
            stop_sound();
            sound = false;
        }

        if (emu->gfx_dirty) {
            convert_vram(emu, display);
            const int pages_written = sh1106_write(display);
            sleep_us(16660 - pages_written * 132); // -SPI write time
            emu->gfx_dirty = false;
        } else
            sleep_us(16660);
        octemu_tick(emu);
    }
    if (sound)
        stop_sound();
}

int main() {
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

#ifdef OCTEMU_DEBUG
    gpio_set_function(0, UART_FUNCSEL_NUM(uart0, 0));
    gpio_set_function(1, UART_FUNCSEL_NUM(uart0, 1));
    uart_init(uart0, UART_BAUDRATE);
    stdio_init_all();
#endif

    if (!emu_roms_count) {
#ifdef OCTEMU_DEBUG
        puts("No ROMs available");
#endif
        goto err;
    }

    // spi display
    display = malloc(sizeof(sh1106));
    *display = (sh1106){.spi = spi0, .sck = 2, .tx = 3, .res = 4, .dc = 5};
    sh1106_init(display);
#ifdef OCTEMU_DEBUG
    printf("SPI: %d Hz\n", spi_get_baudrate(display->spi));
#endif

    // keypad
    for (uint i = 6; i < 10; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
    }
    for (uint i = 10; i < 14; i++)
        gpio_init_pulldown(i);

    // buttons
    gpio_init_pulldown(OCTEMU_PICO_PAUSE_GPIO);
    gpio_init_pulldown(OCTEMU_PICO_RESET_GPIO);
    gpio_init_pulldown(OCTEMU_PICO_QUIT_GPIO);

    // pwm buzzer 500Hz
    gpio_set_function(20, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(20);
    pwm_set_clkdiv(slice_num, SYS_CLK_KHZ / 1250);
    pwm_set_wrap(slice_num, 2500);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(20), 1250);

    // emulator core
    OctEmu *emu = octemu_new(OCTEMU_MODE_OCTO);
    srand(get_rand_32());

    // ready
    gpio_put(25, true);

    uint menu_pos = 0;
    while (1) {
        // select rom
        const OctEmuRom *emu_rom = emu_roms_count > 1 ? menu(&menu_pos) : &emu_roms[0];
        // load rom
        if (octemu_set_rom(emu, emu_rom->data, emu_rom->length))
            break;
        emu->mode = str2mode(emu_rom->mode);
        sh1106_clear(display);
#ifdef OCTEMU_DEBUG
        printf("Loaded ROM \"%s\": %d bytes, mode %d\n", emu_rom->title, emu->rom_size, emu->mode);
#endif
        // run
        emu_loop(emu, emu_rom->tickrate);
        octemu_clear_rom(emu);
        sh1106_clear(display);
    }

    // cleanup
    octemu_free(emu);
    sh1106_shutdown(display);
    free(display);
err:
    gpio_put(25, false);
    sleep_ms(UINT32_MAX);
}
