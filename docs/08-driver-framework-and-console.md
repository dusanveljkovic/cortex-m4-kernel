# Driver Framework, Console & printf

**Files:** `source/kernel/driver.c`, `include/driver.h`,
`source/kernel/console.c`, `include/console.h`,
`source/kernel/kprintf.c`, `include/kprintf.h`,
`source/user/uprintf.c`, `source/user/uprintf.h`,
`source/kernel/ring_buffer.c`, `include/ring_buffer.h`

## The point of this layer

`kprintf` (and everything above it) knows nothing about USART2 or any other
specific piece of hardware. It only knows about the **console**, which fans
output out to whatever **drivers** are attached to it. Swapping or adding an
output - say, logging over a W5500 socket as well as USART2 - is a matter of
writing one more `driver_t` and attaching it; nothing above the driver layer
changes.

```
kprintf / uprintf      formats into a local buffer
        |
     console            fans out to every attached driver
        |
     driver_t            name + init / write / write_sync / read function pointers
        |
   usart2_driver          adapter over source/drivers/usart.c
```

## `driver_t` and the registry

```c
typedef struct driver {
  const char *name;
  int (*init)(void);
  int (*write)(const uint8_t *buf, uint32_t len);
  int (*write_sync)(const uint8_t *buf, uint32_t len);
  int (*read)(uint8_t *buf, uint32_t len);
} driver_t;
```

Any op a device can't support is left `NULL`; callers check before calling.
`driver_register(drv)` (`driver.c`) runs `drv->init` (if present) and then
records the driver in an 8-slot static registry, so it can later be found by
name with `driver_find`.

## The console

`console_attach`/`console_detach` (`console.c`) add or remove a driver from a
4-slot list of "sinks." `console_write` calls every attached sink's `write`;
`console_write_sync` does the same for `write_sync` only, and is what fault
handlers use (see below); `console_read` returns the first successful read
from any attached sink that supports reading.

`console_write` is deliberately **not** wrapped in `irq_save`/`irq_restore`:
a sink's `write` may itself block waiting for its own interrupt to drain a
buffer (USART2's does - see [USART2 Driver](09-usart-driver.md)), and
blocking with interrupts masked would deadlock. Each call hands a sink one
whole formatted string, so output from different tasks rarely interleaves
mid-message in practice; there's no hard guarantee of that without adding a
console-level mutex on top.

## `kprintf` / `kpanicf` / `uprintf`: three ways to print

All three share the same formatting engine (`kvsnprintf`), and differ only in
who's allowed to call them and which output path they use:

| Function | Who can call it | Output path |
|---|---|---|
| `kprintf` | Privileged code only (touches console state) | `console_write` (normal, interrupt-driven) |
| `kpanicf` | Fault handlers | `console_write_sync` (polled — doesn't need interrupts to work) |
| `uprintf` | Unprivileged tasks | Formats locally, then calls the `SYS_CONSOLE_WRITE` syscall |

Separating formatting from output matters here specifically because
`kvsnprintf` only touches its own arguments and the caller-supplied buffer,
it never touches console or driver state making it safe to run from
unprivileged code, unlike `kprintf` itself.

### The printf subset

`%c %s %d %i %u %x %X %p %%`, plus a `0` padding flag and a numeric width
(e.g. `%08x`, `%5d`). No floating point, no 64-bit values, no `%l` length
modifiers.

`kprintf`/`kpanicf` use a 128-byte stack buffer; `uprintf` uses 96 bytes
(smaller, since it runs on a task's own, much smaller stack). Longer output
is truncated, matching `snprintf` semantics. The return value reports what
the *full* length would have been.

## The ring buffer

`ring_buffer.c` is a small, fixed-capacity byte buffer
(`ring_buffer_put`/`get`/`full`/`empty`) used by `usart.c` for both its TX and
RX paths. It's intentionally **not** protected by `irq_save`/`irq_restore` because
each instance is used as a classic single-producer/single-consumer queue
(one side only ever writes `head`, the other only ever writes `tail`), which
is safe without locking as long as that discipline holds. See
[USART2 Driver](09-usart-driver.md) for which side is which.
