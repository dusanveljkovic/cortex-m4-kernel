# Cortex-M4 Kernel

**A preemptive real-time operating system for the STM32F446RE, written from the reset
vector up, and built to run a DHCP server on a real network.** There is no existing RTOS underneath
or any library code except my own.  The vector table, the boot code, the context switch, 
the syscall boundary, the memory protection setup, the allocator, the `printf` engine 
and the network stack are all in this repository

|  |  |
|---|---|
| **Target** | STM32F446RE, ARM Cortex-M4F, 84 MHz, 512 KB flash, 128 KB SRAM |
| **Third-party code** | No HAL, no libc, no CMSIS, no RTOS |
| **Build** | `arm-none-eabi-gcc` and `make`; no IDE, no code generator, no vendor project files |
| **Kernel features** | fixed-priority preemption, 1 ms tick, PendSV switching, SVC syscalls, priority-inheriting mutexes, first-fit heap, MPU regions, driver and console layer |
| **Docs** | 16 subsystem pages, starting at [`docs/`](docs/) |

## Why this exists

The thing I wanted was a DHCP server on the network: one running on the board, handing
out addresses to real machines, keeping leases across a power cycle and staying correct
when a client misbehaves.

There is no operating system underneath that, so the operating system had to be built
first. Building the server is what produced the kernel's requirements:

| The server needs | So the kernel has |
|---|---|
| to wait for a reply without wedging the CPU | a preemptive scheduler whose worst case is a computable number |
| two clients that both want one SPI bus | a single privileged owner of the chip, and requests through syscalls instead of shared memory |
| leases that survive a reset | flash storage with a log-structured write path |
| a lease to expire | a 1 ms tick, and a sleep that queues rather than spins |
| to run without being trusted | unprivileged tasks, and a syscall boundary instead of direct hardware access |

The kernel came first as a strong base with not many features. The DHCP server 
kept turning up the requirements which had not been accounted for. The loop of writing 
an abstraction, using it, watching it break and then fixing is the part of this project 
I would most like a reader to look at.

Real-time correctness is about being able to calculate worst-case response time. The response time 
calculation consists of $C_i$ (execution time), $I_i$ (interference from higher priority tasks) and 
$B_i$ (blocking). Preemption and priorities bound $I_i$ and priority inheritance bounds $B_i$.
Each of those bounds is measured on the board and the [metrics table](#measured-behaviour) can be 
reproduced on hardware

The W5500 performs ARP, IP, TCP and UDP in hardware, so there is no TCP/IP stack written
from nothing. Instead this project includes the SPI driver, the socket state machine, the 
service with interacting with the W5500 and the DHCP protocol above it.

## What is implemented

| Layer | Subsystem | What is notable about it | Verified |
|---|---|---|---|
| Boot | vector table, `.data`/`.bss` init, clock tree | 84 MHz from the internal HSI via PLL; flash wait states raised before the clock switch, bus prescalers set before the PLL is enabled | on hardware |
| Scheduling | fixed-priority preemptive scheduler | priority-ordered ready queue, FIFO within a priority, no allocation on the scheduling path | on hardware |
| Switching | PendSV context switch | save and restore in assembly; the FPU frame is saved only when the task actually used the FPU, via lazy stacking and `EXC_RETURN[4]` | on hardware |
| Privilege | unprivileged tasks by default | tasks reach kernel data structures or hardware only through `svc`; privilege is opt-in per task | on hardware |
| Isolation | 4 MPU regions | per-task stack region re-pointed on every context switch; the fault handler decodes `CFSR` and `MMFAR` and dumps the stacked frame | implemented, **disabled at boot** (see [Limitations](#limitations)) |
| Syscalls | 25 syscall numbers | every pointer and handle argument validated at the boundary before it is dereferenced | on hardware |
| Synchronisation | counting semaphores, priority-inheriting mutexes | transitive priority raise on lock, owner-scanning priority recalculation on unlock, own-lock detection | on hardware |
| Memory | static pools and a first-fit heap | 8 task slots of 2 KB stack each, 8 sockets, 60 KB heap with immediate coalescing | on hardware |
| Console | driver registry into console fan-out | separate polled and interrupt-driven paths | on hardware |
| Networking | socket layer over SPI1 at 10.5 MHz | the W5500 does IP, TCP and UDP in hardware; this is the SPI driver, the socket state machine and the single-owner service, so clients get a blocking RPC syscall | on hardware |
| DHCP | BOOTP/DHCP server | wire-format parser, lease table, flash append journal that survives reboot | handshake verified against the host harness; real-client test planned |
| CLI | debug shell over USART2 | `ps`, `mem`, `heap`, `mutex`, `leasetable`, `link`, `uptime`, `reboot` | on hardware |

## Measured behaviour

Measured on the board with the harness in `testing/benchmarks/`, at 84 MHz, at two
optimisation levels, with about 20,300 context switches and 41,000 critical sections per
run. Method and failure modes are in
[`docs/15`](docs/15-metrics-and-verification.md).

| Metric | `-O0 -g3` (debug) | `-O2` (release) |
|---|---|---|
| Context switch body, PendSV save + select + restore, p50 / p99 | **856 / 1,176 cycles** (10.2 / 14.0 µs) | **272 / 297 cycles** (3.2 / 3.5 µs) |
| Preemption latency, switch decision to incoming task's first instruction, p50 / p99 | 936 / 1,187 | 328 / 377 |
| Syscall round trip, non-rescheduling call, p50 | 405 (4.8 µs) | 111 (1.3 µs) |
| Interrupts-masked window, every `irq_save`/`irq_restore` pair, p50 / p99 / max | 191 / 527 / **532** (6.3 µs) | 79 / 216 / **216** (2.6 µs) |
| Worst masked site | `scheduler_select_next` | same |

What the metrics show:
- The masked window is the system's interrupt-latency floor being 532 cycles at `-O0` 
and 216 at `-O2`. No interrupt is serviced for longed than that
- Half of all the crictical sections are the scheduler's, and they run abour 3x the median.
That is because of `scheduler_select_next` that does two O(n) list walks under one mask. 
- `-O0` costs about 3x across all four independet metrics. The factor is uniform meaning it 
correctly shows the debug build's call overhead.
- The `preemption - switch` cycles number is about 56 to 80 cycles at both optimisation levels. 
That is hardware framing, tail-chaining and the `ICSR` write all which come from the hardware architecture.

## Architecture

```
  unprivileged     +---------------+   +---------------+
  Thread mode      |   user task   |   |  DHCP task    |
  (MPU-gated)      |   prio 1      |   |  prio 1       |
                   +-------+-------+   +-------+-------+
                           |        svc        |
  privileged     +---------v-------------------v---------+
  Handler mode   |          syscall dispatcher           |
                 |  validate arguments, dispatch, yield  |
                 +---+--------------+--------------+-----+
                     |              |              |
             +-------v------+ +-----v------+ +-----v--------+
             | ready queue  | |  kernel    | |  net task    |
             | + scheduler  | |  objects   | |  prio 0      |
             +-------+------+ +------------+ +-----+--------+
                     |                             |
             +-------v--------+            +-------v--------+
             | PendSV switch  |            | SPI1 to W5500  |
             | (assembly)     |            | 10.5 MHz       |
             +----------------+            +----------------+

  SysTick, 1 ms: sleep queue and preemption      USART2 IRQ: 16 B ring buffers
```

## Design decisions

Short versions here. The reasoning, the alternatives and their costs are in
[`docs/14-design-decisions.md`](docs/14-design-decisions.md).

- **No HAL, no libc, no CMSIS.** Every register access is a struct over a fixed address,
  and the runtime routines the compiler would normally call (`memcpy`, `strlen`,
  `printf`) are written in this repository. The build needs a toolchain and `make`.
- **Static task pool, 8 slots.** The scheduler never allocates, so scheduling cannot fail.
  The 2 KB stacks are a power of two because the MPU requires a region to be naturally
  aligned to its own size.
- **Unprivileged by default.** A user task must not access the kernel memory directly.
- **Priority inheritance, raised transitively.** The holder of a contended mutex is
  boosted to the highest waiter and the boost propagates along a chain. On unlock the
  owner's priority is recomputed from the mutexes it still holds.
- **One privileged task owns the Ethernet chip.** The W5500 has one SPI port and one set
  of socket registers, so funnelling every operation through one owner turns a
  concurrency problem into a queue, and makes an unprivileged client's I/O a syscall.

## Getting started

```sh
make                                  # -> build/program.elf, build/program.hex
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/program.elf verify reset exit"
picocom -b 115200 /dev/ttyACM0
```

You need `arm-none-eabi-gcc`, OpenOCD (or the ST-Link Utility) and a serial terminal.
The Nucleo's ST-Link exposes USART2 as a virtual COM port at 115200 8N1. On boot:

```
[boot] ok, sysclk 84MHz, console=usart2

CLI
>
```

Wiring for the optional Ethernet part: SPI1 on the Arduino header, with SCK on PA5, MISO
on PA6, MOSI on PA7, software chip-select on PA4 and the W5500 reset line on PA1. See
[`docs/10-spi-and-w5500.md`](docs/10-spi-and-w5500.md).

| CLI command | What it shows |
|---|---|
| `ps` | each task's state, base and effective priority, saved stack pointer |
| `mem` | static RAM layout: `.data`, `.bss`, heap start and end |
| `heap` | heap bytes in use and free |
| `mutex` | mutex ownership and waiters, including inherited priorities |
| `leasetable` | DHCP leases: address, state, key, expiry |
| `bench` | Run the benchmarks |
| `link` | W5500 PHY link status |
| `uptime` | milliseconds since boot |
| `spioff` | disables SPI1, for fault injection |
| `reboot` | software reset via `SCB->AIRCR` |

## Documentation

| Doc | Question it answers |
|---|---|
| [00, Architecture overview](docs/00-architecture-overview.md) | how the pieces fit together, and the boot sequence |
| [01, Boot and exceptions](docs/01-boot-and-exceptions.md) | vector table, `.data`/`.bss`, exception priorities, fault handling |
| [02, Clock and power](docs/02-clock-and-power.md) | the 84 MHz PLL setup and why each step is ordered that way |
| [03, Scheduler and tasks](docs/03-scheduler-and-tasks.md) | task states, ready queue, context switch, sleeping |
| [04, Syscalls](docs/04-syscalls.md) | the SVC ABI, the dispatch table, argument validation |
| [05, Synchronisation](docs/05-synchronization.md) | semaphores and priority-inheriting mutexes |
| [06, Memory management](docs/06-memory-management.md) | static pools and the first-fit coalescing heap |
| [07, Memory protection](docs/07-memory-protection-mpu.md) | the four MPU regions and per-task stack isolation |
| [08, Driver framework and console](docs/08-driver-framework-and-console.md) | driver registry, console fan-out, the three `printf` paths |
| [09, USART2](docs/09-usart-driver.md) | interrupt-driven, ring-buffered serial |
| [10, SPI1 and W5500](docs/10-spi-and-w5500.md) | the bus layer and the Ethernet controller's register model |
| [11, CLI](docs/11-cli.md) | the debug shell |
| [12, Network stack](docs/12-network-stack.md) | socket layer, the `net` RPC task, UDP paths, error recovery |
| [13, DHCP server](docs/13-dhcp-server.md) | wire format, lease table, flash journal, reply addressing |
| [14, Design decisions](docs/14-design-decisions.md) | why each mechanism looks the way it does |
| [15, Metrics and verification](docs/15-metrics-and-verification.md) | how every measurement was produced, and the self-tests |

## Limitations

These are the current edge of the project, written down instead of left out.

- **The MPU is not enabled at boot.** Region setup, per-switch stack region and the
  `MemManage` handler all exist and are correct, but `mpu_init()` is commented out: the
  DHCP task writes the flash journal directly, which unprivileged code cannot do.
  Enabling isolation needs a flash syscall, or a privileged storage service, first.
- **One network operation is serviced at a time.** The `net` task serialises all chip
  access. Per-request workers, or a non-blocking state machine, would fix it.
- **No TCP.** 
- **Fixed resource limits** 8 tasks, 8 sockets, 60 KB heap, 4 MPU regions.
- **DHCP covers DISCOVER, OFFER, REQUEST, ACK, RELEASE and DECLINE**, not `NAK` or
  `INFORM`, and not relay operation.

## Roadmap

- [ ] A flash syscall, so the MPU can be enabled. 
- [ ] A non-blocking network state machine. 
- [ ] Response-time analysis over measured WCETs.
- [ ] An O(1) priority queue.
- [ ] TCP support
- [ ] DHCP NAK and INFORM

## Author

**Dušan Veljković**, fourth-year student at the School of Electrical Engineering,
University of Belgrade. Built as an independent project to learn embedded systems from the
silicon up.

Questions and corrections are welcome:
[open an issue](https://github.com/dusanveljkovic/cortex-m4-kernel/issues).
