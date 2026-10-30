#ifndef _SLEEP_H
#define _SLEEP_H

#include "stdint.h"
#include "tcb.h"

void sleep_tick(void);
void sleep_for_ticks(uint32_t ticks);

#endif
