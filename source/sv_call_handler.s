.cpu cortex-m4
.syntax unified
.thumb

.extern svc_dispatch

.section .text.sv_call_handler
.type sv_call_handler, %function
.global sv_call_handler
.align 2
sv_call_handler:
  tst lr, #4
  ite eq
  mrseq r0, msp
  mrsne r0, psp

  push {lr}
  bl svc_dispatch
  pop {lr}

  bx lr
