#ifndef _MPU_H
#define _MPU_H

#include "stdint.h"

typedef struct {
  uint32_t TYPE;
  uint32_t CTRL;
  uint32_t RNR;
  uint32_t RBAR;
  uint32_t RASR;
} MPU_RegisterMapType;

#define MPU ((MPU_RegisterMapType *)0xE000ED90)

#endif // !_MPU_H
