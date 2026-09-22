.cpu cortex-m4
.syntax unified
.thumb

.extern task_wrapper
.section .text.task_stack_init
.type task_stack_init, %function
.global task_stack_init
.align 2
task_stack_init:
  mov sp, r0 // initialize stack at thread stack top
  mov r5, #0x0000
  movt r5, #0x0100
  push {r5} // xPSR: push xPSR with thumb bit 1
  ldr r5, =task_wrapper
  push {r5} // PC: go to task_wrapper
  mov r5, #0xFFFD
  movt r5, #0xFFFF
  push {r5} // LR: (thread mode, PSP, no FPU)
  mov r5, #0x12121212
  push {r5} // R12
  mov r5, #0x03030303
  push {r5} // R3
  mov r5, #0x02020202
  push {r5} // R2
  mov r5, r2
  push {r5} // R1: args of function to call
  mov r5, r1
  push {r5} // R0: function to call

  mov r5, #0x11111111
  push {r5} // R11
  mov r5, #0x10101010
  push {r5} // R10
  mov r5, #0x09090909
  push {r5} // R9
  mov r5, #0x08080808
  push {r5} // R8
  mov r5, #0x07070707
  push {r5} // R7
  mov r5, #0x06060606
  push {r5} // R6
  mov r5, #0x05050505
  push {r5} // R5
  mov r5, #0x04040404
  push {r5} // R4

  mov r0, sp
  bx lr

.end
