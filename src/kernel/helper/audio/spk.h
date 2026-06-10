#pragma once
#include "../common.h"


void spk_init(void);

/* Silence speaker and restore the scheduler timer to 100 Hz. */
void spk_shutdown(void);


void spk_fill(const short *samples, unsigned count);


void spk_irq_tick(void);
