# Boot & Exceptions

**Files:** `source/kernel/startup_code.s`, `source/kernel/handlers.s`,
`source/kernel/debug_handlers.c`, `include/scb.h`, `include/nvic.h`,
`include/arm.h`, `linker_script.ld`

## The vector table and reset

`startup_code.s` places the vector table at the very start of flash
(`0x08000000`): word 0 is the initial main stack pointer
(`_main_stack_pointer_value = 0x20020000`, the top of SRAM, from
`linker_script.ld`), word 1 is the reset vector, and the rest are exception
and IRQ handlers.

`reset_handler` runs first, and does the two things every C runtime needs
before `main()` can safely execute:
1. Copies the initial values of all `.data` variables from their load address
   in flash (`_lma_data_start`) to their run address in SRAM
   (`_vma_data_start` .. `_vma_data_end`).
2. Zeroes `.bss` (`_bss_start` .. `_bss_end`).

Handlers not otherwise implemented are simple C stubs in `debug_handlers.c`
(`nmi_handler`, `hard_fault_handler`, `usage_fault_handler`, and the four
early IRQ stubs `irq0_WWDD`..`irq3_RTC`) - they exist so the vector table has
something to point at, and currently just return immediately.

## Exception priorities

Set once, in `main()`, via `NVIC_SET_PRIORITY`/`SCB_SET_PRIORITY`:

| Exception | Priority (lower number = higher priority) |
|-----------|-------------------------------|
| USART2 IRQ | `0xC0` |
| SVCall     | `0xD0` |
| SysTick    | `0xE0` |
| PendSV     | `0xF0` (lowest) |

PendSV is the lowest priority exception so a switch never happens in the 
middle of another exception. USART2 is the highest because a byte arriving 
is usually the most time-critical event. SVCall is above SysTick to prevent 
a syscall in progress being interrupted.

## MemManage fault handling

This is the one fault that gets real handling, since it's the one the MPU
raises when a task oversteps its permitted memory (see
[Memory Protection (MPU)](07-memory-protection-mpu.md)).

`handlers.s`'s `mem_fault_handler` is hand-written assembly, because we need 
to know what stack the exception used. This is decided by the second bit of the 
`LR` register.

```asm
tst lr, #4          ; bit 2 of the EXC_RETURN value in LR:
ite eq              ;   0 -> exception used the main stack (MSP)
mrseq r0, msp       ;   1 -> exception used the process stack (PSP)
mrsne r0, psp
b mem_fault_callback
```

It then jumps to `mem_fault_callback(uint32_t *stack)` in C
(`debug_handlers.c`), which:
- Reads `SCB->CFSR` (fault status) and `SCB->MMFAR` (faulting address).
- Decodes and prints each individual MemManage fault status bit that's set
  (`IACCVIOL`, `DACCVIOL`, `MUNSTKERR`, `MSTKERR`, `MMARVALID`).
- Prints the 8 registers the CPU auto-stacked on exception entry (`R0-R3`,
  `R12`, `LR`, `PC`, `xPSR`) - `stack[6]` (`PC`) is the instruction that
  actually faulted
- Halts in an infinite loop.

All of this goes through `kpanicf`, the polled/synchronous print path (see
[Driver Framework & Console](08-driver-framework-and-console.md)), because the 
fault may have occured in the scheduler, interrupts or anything the normal 
`kprintf` needs.
