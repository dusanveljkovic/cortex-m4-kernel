# USART2 Driver

**Files:** `source/drivers/usart.c`, `source/drivers/usart.h`,
`source/drivers/usart_driver.c`, `include/gpio.h`, `include/nvic.h`

## Pins and role

USART2 on PA2 (TX) / PA3 (RX), AF7. On a Nucleo-F446RE these two pins are
wired to the onboard ST-Link, which exposes them as a USB virtual COM port.
This is the console you see when you open a serial terminal, and the only
console driver currently in the project.

## Setup

`usart2_init()`:
1. Enables the `GPIOA` and `USART2` peripheral clocks.
2. Configures PA2/PA3 as alternate function, AF7, push-pull, no pull-up/down,
   fast speed.
3. Sets `USART2->BRR = 0x016D` for the baud rate (see below), enables TX, RX,
   and the RX-not-empty interrupt (`RXNEIE`), then enables the peripheral.
4. Enables the USART2 IRQ at the NVIC.
5. Zeroes both ring buffers and initializes `rx_sem` (count 0) — the
   semaphore a blocking read waits on.

**Baud rate:** `0x016D` is 365 decimal. With `OVER8=0` (the reset default),
the `BRR` register directly holds `16 × USARTDIV`, which works out to
`f_PCLK1 / baud`. At the project's current 42 MHz APB1 clock (see
[Clock & Power](02-clock-and-power.md)), `42,000,000 / 365 ≈ 115,068` - close
enough to standard **115200 baud** to work correctly on real hardware. This
value is a hardcoded magic number, though, not computed from the actual
clock - if the clock configuration ever changes, this needs to be
recalculated (or replaced with a real `(APB1_CLK + baud/2) / baud`
expression) or the baud rate will silently drift.

## TX and RX: interrupt-driven, ring-buffered

Both directions go through a 16-byte `ring_buffer_t` (see
[Driver Framework & Console](08-driver-framework-and-console.md)):

- **TX**: `usart2_putc(c)` blocks (busy-waits) only if the TX ring buffer is
  full; otherwise it just enqueues the byte and sets `TXEIE`. The actual byte
  transmission happens in `usart2_handler` (the USART2 IRQ), which while
  `TXE` is set and `TXEIE` is enabled pulls one byte from the ring buffer
  and writes it to `USART2->DR`, or clears `TXEIE` once the buffer is empty
  . TX is a single producer (whatever task calls `usart2_putc`) and single consumer
  (the ISR), so the ring buffer needs no locking.
- **RX**: the same IRQ, on `RXNE`, pushes the received byte into the RX ring
  buffer (if there's room) and posts `rx_sem`. `usart2_getc(&c)` calls
  `sys_sem_wait(&rx_sem)` which blocks the calling task
  until a byte has arrived, then pulls it out of the ring buffer. Single
  producer (the ISR), single consumer (whoever's reading), same reasoning.

## The fault-safe path

`usart2_fault_putc(c)` is a separate, purely polled write: spin until `TXE`
is set, then write directly to `DR`. This is what backs `console_write_sync`/`kpanicf`
(see [Driver Framework & Console](08-driver-framework-and-console.md)). By
the time a fault handler runs, you can't assume the USART2 interrupt is still
going to fire, or that the scheduler is in a state where blocking on a
semaphore would ever return.

## The driver adapter

`usart_driver.c` is the thin `driver_t` wrapper (`usart2_driver`) that maps
the generic interface onto the four functions above:
`write`→`usart2_putc` (looped), `write_sync`→`usart2_fault_putc` (looped),
`read`→`usart2_getc` (looped). `usart.c` itself has no idea the driver
framework exists.
