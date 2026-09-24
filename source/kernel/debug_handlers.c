#include "../../include/scb.h"
#include "../../include/usart.h"

void debug_puts(const char *name, uint32_t num) {
  usart2_fault_puts(name);
  usart2_fault_puts(" =  0x");
  usart2_fault_puthex(num);
  usart2_fault_puts("\r\n");
}
void mem_fault_callback(uint32_t *stack) {
  uint32_t cfsr = SCB->CFSR;
  uint32_t mmfar = SCB->MMFAR;

  usart2_fault_puts("\r\n");
  usart2_fault_puts("========== MEMMANAGE FAULT ==========\r\n");
  debug_puts("CFSR", cfsr);
  debug_puts("MMFAR", mmfar);

  usart2_fault_puts("MMFSR: \r\n");
  if (cfsr & SCB_CFSR_IACCVIOL_Msk)
    usart2_fault_puts("  IACCVIOL  - instruction access violation\r\n");

  if (cfsr & SCB_CFSR_DACCVIOL_Msk)
    usart2_fault_puts("  DACCVIOL  - data access violation\r\n");

  if (cfsr & SCB_CFSR_MUNSTKERR_Msk)
    usart2_fault_puts("  MUNSTKERR - MPU fault during exception return\r\n");

  if (cfsr & SCB_CFSR_MSTKERR_Msk)
    usart2_fault_puts("  MSTKERR   - MPU fault while stacking\r\n");

  if (cfsr & SCB_CFSR_MMARVALID_Msk)
    usart2_fault_puts("  MMARVALID - MMFAR contains valid address\r\n");

  usart2_fault_puts("\r\nStacked registers:\r\n");
  debug_puts("R0", stack[0]);
  debug_puts("R1", stack[1]);
  debug_puts("R2", stack[2]);
  debug_puts("R3", stack[3]);
  debug_puts("R12", stack[4]);
  debug_puts("LR", stack[5]);
  debug_puts("PC", stack[6]);
  debug_puts("xPSR", stack[7]);

  usart2_fault_puts("=====================================\r\n");

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
