#ifndef _FPU_H
#define _FPU_H

#include "arm.h"
#include "stdint.h"

#define CPACR *((volatile uint32_t *)(0xE000ED88))
#define CPACR_CP_FULL_ACCESS (0b11)
#define CPACR_CP_Pos(num) (2 * num)

#define FPCCR *((volatile uint32_t *)(0xE000EF34))
#define FPCCR_LSPEN_Pos (30)
#define FPCCR_ASPEN_Pos (31)

#endif
