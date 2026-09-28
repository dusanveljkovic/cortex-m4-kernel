#include "../../include/kprintf.h"
#include "../../include/scb.h"

void mem_fault_callback(uint32_t *stack) {
  uint32_t cfsr = SCB->CFSR;
  uint32_t mmfar = SCB->MMFAR;

  kpanicf("\r\n");
  kpanicf("========== MEMMANAGE FAULT ==========\r\n");
  kpanicf("CFSR: %x\r\n", cfsr);
  kpanicf("MMFAR: %x\r\n", mmfar);

  kpanicf("MMFSR: \r\n");
  if (cfsr & SCB_CFSR_IACCVIOL_Msk)
    kpanicf("  IACCVIOL  - instruction access violation\r\n");

  if (cfsr & SCB_CFSR_DACCVIOL_Msk)
    kpanicf("  DACCVIOL  - data access violation\r\n");

  if (cfsr & SCB_CFSR_MUNSTKERR_Msk)
    kpanicf("  MUNSTKERR - MPU fault during exception return\r\n");

  if (cfsr & SCB_CFSR_MSTKERR_Msk)
    kpanicf("  MSTKERR   - MPU fault while stacking\r\n");

  if (cfsr & SCB_CFSR_MMARVALID_Msk)
    kpanicf("  MMARVALID - MMFAR contains valid address\r\n");

  kpanicf("\r\nStacked registers:\r\n");
  kpanicf("R0: %x\r\n", stack[0]);
  kpanicf("R1: %x\r\n", stack[1]);
  kpanicf("R2: %x\r\n", stack[2]);
  kpanicf("R3: %x\r\n", stack[3]);
  kpanicf("R12: %x\r\n", stack[4]);
  kpanicf("LR: %x\r\n", stack[5]);
  kpanicf("PC: %x\r\n", stack[6]);
  kpanicf("xPSR: %x\r\n", stack[7]);

  kpanicf("=====================================\r\n");

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
