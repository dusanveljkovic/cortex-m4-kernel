# STM32F446RE Bare-Metal RTOS

A small preemptive, priority-based RTOS for the STM32F446RE (Nucleo-64), written in
C and ARM assembly with no vendor HAL and no libc everything from the vector
table to `printf` is implemented in this repository.

Highlights: priority-preemptive scheduling with a 1 ms tick, priority-inheriting
mutexes, a heap allocator, per-task MPU stack isolation with unprivileged task
execution, a pluggable driver/console layer, a debug CLI over USART2, and
SPI/W5500 (Ethernet) drivers.

For what each piece does and how it works internally, see the per-component
docs in [`docs/`](docs/). This file only covers getting a build onto the board.

## 1. Hardware

- **Board:** ST Nucleo-F446RE (or bare STM32F446RE with a way to flash/debug it).
- **Connection:** USB cable into the board's ST-Link connector. This both powers
  the board and exposes a virtual COM port for USART2.
- **Optional:** an SPI peripheral on the Arduino header (SCK=PA5, MISO=PA6,
  MOSI=PA7, CS=PA4) if you're using the SPI/W5500 drivers. Note PA5 is shared
  with the onboard LED (LD2), which will flicker with SCK traffic.

## 2. Prerequisites

Install the GNU Arm Embedded Toolchain (provides `arm-none-eabi-gcc`,
`arm-none-eabi-ld`, `arm-none-eabi-objcopy`) and make sure it's on your `PATH`:

```sh
arm-none-eabi-gcc --version
```

You'll also need a way to get the built firmware onto the chip and to talk to
it afterward. You can use:
- **OpenOCD** + `arm-none-eabi-gdb` for flashing and interactive debugging, or
- ST-Link Utility (Windows).

Any serial terminal works for talking to the board once it's running:
PuTTY, `picocom`, `minicom`, etc.

## 3. Build

```sh
make
```

This produces `build/program.elf` (for debugging) and `build/program.hex`
(Intel HEX, for flashing) using the settings in `makefile`:

- Target: `-mcpu=cortex-m4 -mthumb`
- No standard library linked (`-fno-builtin`)
- Debug info included by default (`DEBUG_ENABLED = 1` → `-g3 -O0`)

```sh
make clean   # remove build/
```

## 4. Flash it

**Using OpenOCD:**
```sh
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/program.elf verify reset exit"
```

The board resets and starts running immediately after flashing.

## 5. Run it

1. Open a serial terminal on the Nucleo's virtual COM port at **115200 baud,
   8N1, no flow control**. (On Linux/macOS it typically shows up as
   `/dev/ttyACM0`; on Windows, check Device Manager for the assigned COM port.)
2. Reset the board (or re-flash it). You should see:
   ```
   [boot] ok, sysclk 84MHz, console=usart2
   [boot] Initializing tasks: CLI, IDLE, user

   CLI
   >
   ```
3. Type a command and press Enter. Available commands:

   | Command  | What it shows |
   |----------|----------------|
   | `help`   | List all commands |
   | `ps`     | Every task's state, priorities, and stack pointer |
   | `uptime` | Milliseconds since boot |
   | `mem`    | Static RAM layout (`.data`/`.bss`/heap sizes) |
   | `heap`   | Heap used/free bytes |
   | `mutex`  | Which tasks currently own which mutexes, and who's waiting |
   | `reboot` | Triggers a software reset (`SCB->AIRCR`) |

Everything else the "user" task does lives in `source/user/user_main.c`.
Write user code in this file to build projects on top of this kernel.

## 6. Project layout

```
include/            Shared headers: peripheral register maps, kernel APIs
source/kernel/       Kernel: boot, scheduler, syscalls, sync, memory, MPU, CLI
source/drivers/       Peripheral drivers: USART2, SPI1, W5500
source/user/          Your application code (user_main.c) + uprintf helper
source/utils.c        Freestanding strcmp/strlen (no libc)
linker_script.ld       Memory map (512K flash / 128K SRAM) and section layout
makefile               Build rules
```

See [`docs/00-architecture-overview.md`](docs/00-architecture-overview.md) for
how these pieces fit together, and the rest of `docs/` for one file per
subsystem.
