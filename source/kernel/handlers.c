#include "../../include/scb.h"
#include "../../include/usart.h"

void mem_fault_handler() {
  uint32_t cfsr = SCB->CFSR;
  uint32_t mmfar = SCB->MMFAR;

  usart2_fault_puts("\r\n");
  usart2_fault_puts("========== MEMMANAGE FAULT ==========\r\n");
  usart2_fault_puts("CFSR = 0x");
  usart2_fault_puthex(cfsr);
  usart2_fault_puts("\r\n");
  usart2_fault_puts("MMFAR = 0x");
  usart2_fault_puthex(mmfar);
  usart2_fault_puts("\r\n");
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
