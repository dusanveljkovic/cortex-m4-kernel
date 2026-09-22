#include "../../include/mpu.h"
#include "../../include/arm.h"
#include "../../include/scb.h"
#include "../../include/tcb.h"

void mpu_init(void) {
  MPU->CTRL = 0;

  MPU->RNR = MPU_REGION_FLASH;
  MPU->RBAR = 0x08000000;
  MPU->RASR = (MPU_AP_RO_RO << MPU_RASR_AP_Pos) | (18 << MPU_RASR_SIZE_Pos) |
              (1 << MPU_RASR_ENABLE_Pos);

  MPU->RNR = MPU_REGION_HEAP;
  MPU->RBAR = 0x20010000;
  MPU->RASR = (MPU_AP_RW_RW << MPU_RASR_AP_Pos) | (15 << MPU_RASR_SIZE_Pos) |
              (1 << MPU_RASR_ENABLE_Pos);

  SCB->SHCSR |= SCB_SHCSR_MEMFAULTENABLE;

  MPU->CTRL = (1 << MPU_CTRL_ENABLE_Pos) | (1 << MPU_CTRL_PRIVDEFENA_Pos);
}

void mpu_set_region(uint32_t region, uintptr_t base, uint32_t size, uint32_t ap,
                    bool exectute_never) {
  MPU->RNR = region;

  MPU->RBAR = base;

  uint32_t rasr = 0;
  if (exectute_never)
    rasr |= (1 << MPU_RASR_XN_Pos);

  rasr = (ap << MPU_RASR_AP_Pos);

  uint32_t size_encoding = 0;
  uint32_t region_size = size;
  while (region_size > 2) {
    region_size >>= 1;
    size_encoding++;
  }

  rasr |= size_encoding << MPU_RASR_SIZE_Pos;

  rasr |= (1 << MPU_RASR_ENABLE_Pos);

  MPU->RASR = rasr;
}

void mpu_disable_region(uint32_t region);
inline void mpu_configure_task_stack(uintptr_t base, uint32_t size) {
  mpu_set_region(MPU_REGION_TASK_STACK, base, size, MPU_AP_RW_RW, true);
}

void mpu_switch_to_task(tcb_t *task) {
  mpu_configure_task_stack((uintptr_t)task->stack_base, task->stack_size);

  __DSB();
  __ISB();
}
