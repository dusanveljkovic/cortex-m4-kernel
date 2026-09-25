#include "../../include/scb.h"
#include "../../include/utils.h"

void mem_fault_callback(uint32_t *stack) {
  uint32_t cfsr = SCB->CFSR;
  uint32_t mmfar = SCB->MMFAR;

  printf_fault("\r\n");
  printf_fault("========== MEMMANAGE FAULT ==========\r\n");
  printf_fault("CFSR: %x\r\n", cfsr);
  printf_fault("MMFAR: %x\r\n", mmfar);

  printf_fault("MMFSR: \r\n");
  if (cfsr & SCB_CFSR_IACCVIOL_Msk)
    printf_fault("  IACCVIOL  - instruction access violation\r\n");

  if (cfsr & SCB_CFSR_DACCVIOL_Msk)
    printf_fault("  DACCVIOL  - data access violation\r\n");

  if (cfsr & SCB_CFSR_MUNSTKERR_Msk)
    printf_fault("  MUNSTKERR - MPU fault during exception return\r\n");

  if (cfsr & SCB_CFSR_MSTKERR_Msk)
    printf_fault("  MSTKERR   - MPU fault while stacking\r\n");

  if (cfsr & SCB_CFSR_MMARVALID_Msk)
    printf_fault("  MMARVALID - MMFAR contains valid address\r\n");

  printf_fault("\r\nStacked registers:\r\n");
  printf_fault("R0: %x\r\n", stack[0]);
  printf_fault("R1: %x\r\n", stack[1]);
  printf_fault("R2: %x\r\n", stack[2]);
  printf_fault("R3: %x\r\n", stack[3]);
  printf_fault("R12: %x\r\n", stack[4]);
  printf_fault("LR: %x\r\n", stack[5]);
  printf_fault("PC: %x\r\n", stack[6]);
  printf_fault("xPSR: %x\r\n", stack[7]);

  printf_fault("=====================================\r\n");

  while (1)
    ;
}

void nmi_handler() {}

void hard_fault_handler() {}

void usage_fault_handler() {}

void irq0_WWDD() {}

void irq1_PVD() {}

void irq2_TAMPER() {}

void irq3_RTC() {}
