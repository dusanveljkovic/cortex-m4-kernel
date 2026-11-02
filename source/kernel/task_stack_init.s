.cpu cortex-m4
.syntax unified
.thumb

.extern task_wrapper
.section .text.task_stack_init
.type task_stack_init, %function
.global task_stack_init
.align 2
task_stack_init:
  mov r12, r0
  // mov sp, r0 // initialize stack at thread stack top
  mov r3, #0x0000
  movt r3, #0x0100
  str r3, [r12, #-4]! // xPSR: push xPSR with thumb bit 1
  ldr r3, =task_wrapper
  str r3, [r12, #-4]! // PC: go to task_wrapper
  mov r3, #0xFFFD
  movt r3, #0xFFFF
  str r3, [r12, #-4]! // LR: (thread mode, PSP, no FPU)
  mov r3, #0x12121212
  str r3, [r12, #-4]! // R12
  mov r3, #0x03030303
  str r3, [r12, #-4]! // R3
  mov r3, #0x02020202
  str r3, [r12, #-4]! // R2
  mov r3, r2
  str r3, [r12, #-4]! // R1: args of function to call
  mov r3, r1
  str r3, [r12, #-4]! // R0: function to call

  mov r3, 0xFFFD
  movt r3, 0xFFFF
  str r3, [r12, #-4]!

  mov r3, #0x11111111
  str r3, [r12, #-4]! // R11
  mov r3, #0x10101010
  str r3, [r12, #-4]! // R10
  mov r3, #0x09090909
  str r3, [r12, #-4]! // R9
  mov r3, #0x08080808
  str r3, [r12, #-4]! // R8
  mov r3, #0x07070707
  str r3, [r12, #-4]! // R7
  mov r3, #0x06060606
  str r3, [r12, #-4]! // R6
  mov r3, #0x05050505
  str r3, [r12, #-4]! // R5
  mov r3, #0x04040404
  str r3, [r12, #-4]! // R4


  // mov r0, sp
  mov r0, r12
  bx lr

.end
