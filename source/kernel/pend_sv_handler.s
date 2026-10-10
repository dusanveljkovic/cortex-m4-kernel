.cpu cortex-m4
.syntax unified
.thumb

.extern scheduler_select_next
.extern scheduler_put_task
.extern mpu_switch_to_task
.extern task_set_privilege
.extern current_task
.extern next_task

.section .text.pend_sv_handler
.type pend_sv_handler, %function
.global pend_sv_handler
.align 2
pend_sv_handler:
  isb 

  ldr r1, =current_task
  ldr r1, [r1]

  cmp r1, #0
  beq first_task

  mrs r0, psp

  tst lr, #0x10
  bne skip_vstmdb
  vstmdb r0!, {s16-s31}

skip_vstmdb:
  stmdb r0!, {r4-r11, lr}

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
  mov r4, r2

  mov r0, r4
  bl mpu_switch_to_task

  mov r0, r4
  bl task_set_privilege

  ldr r0, [r4]
  ldmia r0!, {r4-r11, lr}
  tst lr, #0x10
  bne skip_vldmia
  vldmia r0!, {s16-s31} 

skip_vldmia:
  msr psp, r0
  isb
  bx lr
