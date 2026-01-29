#ifndef _SH1106_H_
#define _SH1106_H_

#include <stdint.h>

#include "hardware/spi.h"

typedef struct sh1106 {
    spi_inst_t *spi;
    uint8_t sck, tx, res, dc;
    uint8_t vram[8][128];
    uint8_t page_dirty;
} sh1106;

void sh1106_init(sh1106 *display);
void sh1106_shutdown(sh1106 *display);
int sh1106_write(sh1106 *display);
void sh1106_clear(sh1106 *display);

#endif // _SH1106_H_
