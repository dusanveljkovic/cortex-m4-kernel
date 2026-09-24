#ifndef _MPU_H
#define _MPU_H

#include "stdbool.h"
#include "stdint.h"

typedef struct {
  uint32_t TYPER;
  uint32_t CTRL;
  uint32_t RNR;
  uint32_t RBAR;
  uint32_t RASR;
} MPU_RegisterMapType;

#define MPU ((MPU_RegisterMapType *)0xE000ED90)

#define MPU_RASR_XN_Pos 28
#define MPU_RASR_AP_Pos 24
#define MPU_RASR_TEX_Pos 19
#define MPU_RASR_S_Pos 18
#define MPU_RASR_C_Pos 17
#define MPU_RASR_B_Pos 16
#define MPU_RASR_SIZE_Pos 1
#define MPU_RASR_ENABLE_Pos 0

#define MPU_AP_RW_RW 0b011
#define MPU_AP_RO_RO 0b110
#define MPU_AP_RW___ 0b001

#define MPU_CTRL_ENABLE_Pos 0
#define MPU_CTRL_PRIVDEFENA_Pos 2

#define MPU_REGION_FLASH 0
#define MPU_REGION_TASK_STACK 1
#define MPU_REGION_HEAP 2
#define MPU_REGION_KERNEL_STACK 7

void mpu_init(void);
void mpu_set_region(uint32_t region, uintptr_t base, uint32_t size, uint32_t ap,
                    bool exectute_never);
void mpu_disable_region(uint32_t region);
void mpu_configure_task_stack(uintptr_t base, uint32_t size);

#endif // !_MPU_H
