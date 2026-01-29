/**
 * Minimal SH1106 128x64 SPI display "driver" for rpi pico
 * based on SH1106 datasheet
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "sh1106.h"

// commands
#define SET_SEG_REMAP_REV 0xA1
#define SET_COM_SCAN_DEC 0xC8
#define DISPLAY_OFF 0xAE
#define DISPLAY_ON 0xAF

static inline void set_data_mode(const sh1106 *display, const bool data_mode) {
    gpio_put(display->dc, data_mode);
}

static inline void write_cmd(const sh1106 *display, const uint8_t cmd) {
    spi_write_blocking(display->spi, &cmd, 1);
}

void sh1106_init(sh1106 *display) {
    spi_init(display->spi, 8 * 1000 * 1000); // 8MHz, 1MB/s, 1B/us

    gpio_set_function(display->sck, GPIO_FUNC_SPI); // SCK
    gpio_set_function(display->tx, GPIO_FUNC_SPI); // TX

    gpio_init(display->res); // RES
    gpio_set_dir(display->res, GPIO_OUT);

    gpio_init(display->dc); // DC, default cmd mode
    gpio_set_dir(display->dc, GPIO_OUT);

    sleep_us(10);
    gpio_put(display->res, true);

#ifdef SH1106_ROTATE_SCREEN
    write_cmd(display, SET_COM_SCAN_DEC);
    write_cmd(display, SET_SEG_REMAP_REV);
#endif

    write_cmd(display, DISPLAY_ON);
    sleep_ms(100);
    sh1106_clear(display);
}

void sh1106_clear(sh1106 *display) {
    memset(display->vram, 0, sizeof(display->vram));
    display->page_dirty = 0xFF;
    sh1106_write(display);
}

int sh1106_write(sh1106 *display) {
    int pages_written = 0;
    for (uint i = 0; i < 8; i++) {
        if (!(display->page_dirty & (1 << i)))
            continue;
        set_data_mode(display, false);
        spi_write_blocking(display->spi, (const uint8_t[]){0xB0 | i, 0x02, 0x10}, 3);
        set_data_mode(display, true);
        spi_write_blocking(display->spi, display->vram[i], sizeof(display->vram[i]));
        ++pages_written;
    }
    display->page_dirty = 0;
    return pages_written;
}

void sh1106_shutdown(sh1106 *display) {
    set_data_mode(display, false);
    write_cmd(display, DISPLAY_OFF);
}
