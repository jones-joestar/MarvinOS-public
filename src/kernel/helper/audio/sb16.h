#pragma once
#include "../common.h"

int  sb16_init(uint32_t sample_rate);
void sb16_fill_half(const short *src);  /* non-blocking write to DMA buffer */
void sb16_stop(void);                   /* pause DMA; call before heavy disk I/O */
void sb16_start(void);                  /* resume DMA with zeroed buffer; call after disk I/O */
void sb16_shutdown(void);               /* halt DMA and mask IRQ5; call when audio owner exits */
void sb16_sync(void);                   /* poll I/O to yield QEMU event loop */
