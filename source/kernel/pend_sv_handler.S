.cpu cortex-m4
.syntax unified
.thumb

.extern scheduler_select_next
.extern scheduler_put_task
.extern mpu_switch_to_task
.extern task_set_privilege
.extern current_task
.extern next_task

#if BENCH
.extern bench_pendsv_start
.extern bench_pendsv_end
.extern bench_pendsv_lr
.extern bench_record_switch
#endif

.section .text.pend_sv_handler
.type pend_sv_handler, %function
.global pend_sv_handler
.align 2
pend_sv_handler:
#if BENCH
  ldr r1, =0xE0001004
  ldr r1, [r1]
  ldr r2, =bench_pendsv_start
  str r1, [r2]
  ldr r2, =bench_pendsv_lr
  str lr, [r2]
#endif

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

#if BENCH
  ldr r1, =0xE0001004
  ldr r1, [r1]
  ldr r2, =bench_pendsv_end
  str r1, [r2]

  mov r0, lr
  bl bench_record_switch
  mov lr, r0
#endif

  bx lr
