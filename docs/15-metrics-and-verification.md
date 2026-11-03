# Metrics and verification

## Why two builds

The interrupts-masked instrument sits on `irq_save` and `irq_restore`. The scheduler's
critical section is on the context switch path. That puts the masked-window instrument
inside the switch measurement.

I measured what happens if you ignore that. With both instruments live, the switch
minimum moved from 509 to 689 cycles, a 35 % error produced entirely by the other
metric's apparatus. So each metric is taken from a build where the other instrument is
compiled out.

| Build | Measures | Instrument live |
|---|---|---|
| `BENCH_IRQ=0` | context switch, preemption latency, syscall | `CYCCNT` hooks in `pend_sv_handler.s` |
| `BENCH_IRQ=1` | interrupts-masked window | `CYCCNT` and a histogram update in `irq_save`/`irq_restore` |

Both builds use `BENCH=1`, which adds the workload (three priority-3 tasks handing off
through semaphores) and the `bench` command.

## Protocol

```
reset counters, start workload, discard 1,000 switches (warm-up),
reset again, measure until 20,000 switches, snapshot the counters,
stop the workload, print
```

The snapshot happens before the workload is stopped and before the syscall probe runs, so exit switches and probe overhead cannot leak into the reported statistics. 

## Results

84 MHz, `-O2` or `-O0 -g3` as stated, about 20,300 context switches and 41,000 critical
sections per run, 1,000 syscall round trips. Raw output is in `testing/benchmarks/`.

| Metric | `-O0 -g3` | `-O2` | factor |
|---|---|---|---|
| Context switch body, p50 | 856 cyc / 10.2 µs | 272 cyc / 3.2 µs | 3.1x |
| Context switch body, p99 | 1,176 | 297 | 4.0x |
| Preemption latency, p50 | 936 / 11.1 µs | 328 / 3.9 µs | 2.9x |
| Preemption latency, p99 | 1,187 | 377 | 3.1x |
| Syscall round trip, p50 | 405 / 4.8 µs | 111 / 1.3 µs | 3.6x |
| Interrupts-masked, p50 | 191 / 2.3 µs | 79 / 0.9 µs | 2.4x |
| Interrupts-masked, p99 | 527 | 216 | 2.4x |
| Interrupts-masked, max | **532** / 6.3 µs | **216** / 2.6 µs | 2.5x |
| Worst masked site | `scheduler_select_next` | same | n/a |

## What the numbers mean

The context switch at 856 cycles (`-O0`) or 272 (`-O2`) covers saving the outgoing
context, popping the ready queue, re-pointing the MPU stack region and restoring the
incoming context.  It excludes the hardware's own exception entry and exit,
which is 12 cycles of stacking and 10 of unstacking.

The residual `preemption - switch` is 56 to 80 cycles at both levels. It is the hardware
frame, tail-chaining and the `ICSR` write. 

The syscall costs 405 cycles at `-O0` and 111 at `-O2`, a factor of 3.6, the largest of
the four. At `-O0` most of that figure is the call into `syscall()`, its prologue and the
176-byte frame in `svc_dispatch`. At `-O2` the wrappers inline and what is left is close
to the architectural trap of about 25 cycles plus a real dispatcher. This is the price of
running tasks unprivileged.

## Behavioural verification

| What | How | Status |
|---|---|---|
| Boot to shell | power-on, boot lines and prompt | verified |
| Task states, priorities, masks | `ps` and `mutex` while contended | verified |
| Memory accounting | `mem` and `heap` before and after allocations | verified |
| Fault reporting | deliberate out-of-region write, MemManage decode | needs `mpu_init()`, currently off |
| UDP send and receive | host exchange, 1 KB datagrams | verified |
| DHCP handshake | `testing/dhcp_client.py`, 11 predicted-value checks, non-zero exit on failure | verified; independent-client test planned |
| Deterministic rebuild | `-O2` runs the same workload and reports the same statistics | verified |

## Reproducing

```sh
make clean && make OPT=-O0 BENCH=1 BENCH_IRQ=0     # switch and syscall
make clean && make OPT=-O0 BENCH=1 BENCH_IRQ=1     # masked window
make clean && make OPT=-O2 BENCH=1 BENCH_IRQ=0
make clean && make OPT=-O2 BENCH=1 BENCH_IRQ=1
# open the port at 115200 8N1 and type: bench
```

A `BENCH_TAG` is automatically compiled into the image and is printed above the table.
