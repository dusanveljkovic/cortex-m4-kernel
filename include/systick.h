#ifndef _SYSTICK_H
#define _SYSTICK_H
#include <stdint.h>

typedef struct {
  uint32_t CTRL;
  uint32_t LOAD;
  uint32_t VAL;
  uint32_t CALIB;
} STK_RegisterMapType;

#define STK ((STK_RegisterMapType *)0xE000E010)

#define STK_CTRL_ENABLE (1 << 0)
#define STK_CTRL_TICKINT (1 << 1)
#define STK_CTRL_CLKSOURCE (1 << 2)
#define STK_CTRL_COUNTFLAG (1 << 16)

void systick_init();

#endif // !_SYSTICK_H
