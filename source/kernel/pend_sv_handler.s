.cpu cortex-m4
.syntax unified
.thumb

.extern scheduler_select_next
.extern mpu_switch_to_task
.extern task_set_privilege
.extern current_task
.extern next_task

.section .text.pend_sv_handler
.type pend_sv_handler, %function
.global pend_sv_handler
.align 2
pend_sv_handler:
  cpsid i
  isb 

  ldr r1, =current_task
  ldr r1, [r1]

  cmp r1, #0
  beq first_task

  mrs r0, psp
  stmdb r0!, {r4-r11}
  str r0, [r1]

  bl scheduler_select_next

  b restore_task
first_task:
  bl scheduler_select_next

restore_task:
  ldr r0, =current_task
  ldr r1, =next_task
  ldr r2, [r1]
  str r2, [r0]

  mov r0, r2
  push {r2}
  bl mpu_switch_to_task
  pop {r2}

  mov r0, r2
  push {r2}
  bl task_set_privilege
  pop {r2}

  ldr r0, [r2]
  ldmia r0!, {r4-r11}

  msr psp, r0
  isb
  cpsie i
  ldr lr, =0xFFFFFFFD
  bx lr
