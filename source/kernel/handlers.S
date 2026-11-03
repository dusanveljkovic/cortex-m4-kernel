.cpu cortex-m4
.syntax unified
.thumb

.extern mem_fault_callback

.section .text.mem_fault_handler
.type mem_fault_handler, %function
.global mem_fault_handler
.align 2
mem_fault_handler:
  tst lr, #4
  ite eq
  mrseq r0, msp
  mrsne r0, psp

  b mem_fault_callback
