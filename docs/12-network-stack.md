# Network stack: the W5500 socket layer

**Files:** `source/kernel/net.c`, `include/kernel/net.h`, `source/drivers/w5500.c`,
`include/drivers/w5500.h`, `source/kernel/static_memory.c` (socket pool),
`source/kernel/syscall.c` (`SYS_SOCKET_*`, `SYS_NET_*`)

## The problem this layer solves

The W5500 is shared, stateful and blocking. Its operations end when the chip says so and not when the CPU does. An example of that is waiting on a receive, or waiting on ARP resolution which can take about 1.6s. If these operations were implemented as a syscall it would require blocking inside a syscall which is forbidden. 

Because of this limitations the chip gets one owner, a privileged task `net_task` which is the only code in the system allowed to touch the chip. The layer is an RPC server insted of a library.

## Two files, two responsibilities

| File | Knows about | Does not know about |
|---|---|---|
| `w5500.c` | SPI framing, register blocks, socket commands, `Sn_SR` and `Sn_IR` state machines | tasks, schedulers, syscalls |
| `net.c` | the request mailbox, the two semaphores, status to error mapping, recovery | SPI, register addresses |

`net.c` contains no register writes, and `w5500.c` contains no task or kernel calls. That
split lets the driver be reasoned about as a device and the service as a concurrency
problem.

## The request mailbox

```c
typedef struct {
  net_op_t op;            // LINK | SOCKET_OPEN | SOCKET_CLOSE | RECV | SEND
  volatile tcb_t *caller;
  socket_t *socket;
  net_reply_t *reply_slot;// out-param for everything: status, got, src, truncated
  uint8_t *buffer;        // in/out data
  uint16_t buf_len;
  uint8_t dst_ip[4];
  uint16_t dst_port;
  uint8_t dst_mac[6];
  bool use_mac;
} net_request_t;
```

One static `current_req` holds the request in flight. A client fills it, posts
`net_posted` and waits on `net_done`:

```c
void net_send_request(net_request_t *req) { current_req = *req; semaphore_post(net_posted); }
bool net_will_wait(void) { return (semaphore_wait(net_done) == SEM_OK_YIELD); }
```

The name `net_will_wait` is misleading: it does the wait and returns true when the wait
actually blocked. The syscall dispatcher uses the return value to decide whether it has
to yield:

```c
net_send_request(&req);
if (net_will_wait()) should_yield = 1;
```

This matters because blocking is only legal at the top level of a syscall. A nested `svc`
at the same exception priority is pended rather than executed, so a dispatcher that
blocked halfway through a case would deadlock against itself.

Only one request may be in flight at a time, and the copy into and out of `current_req`
happens inside `irq_save`/`irq_restore` so a tick cannot observe a half-written request.

## The service loop

```c
while (1) {
  sys_sem_wait(net_posted);            // sleep until someone asks
  take a private copy of current_req;  // under irq_save
  dispatch on my_req.op;               // call the w5500_* primitive
  map w5500_status_t -> net_status_t;  // OK | RETRY | UNREACHABLE | BAD_REQUEST | FAULT | DEAD
  sys_sem_post(net_done);              // wake the client
}
```

### Status mapping

| Driver status | Client sees | Recovery performed by the service |
|---|---|---|
| `W5500_OK` | `NET_STATUS_OK` | none |
| `W5500_ERR_TRANSIENT` or `ERR_UNSTABLE` | `NET_STATUS_RETRY` | none, the caller retries |
| `W5500_ERR_UNREACHABLE` | `NET_STATUS_UNREACHABLE` | none, because this status is observed rather than inferred |
| `W5500_ERR_ARG` | `NET_STATUS_BAD_REQUEST` | none |
| `W5500_ERR_STATE` | `NET_STATUS_FAULT` | full re-init: `w5500_init()` and `socket_memory_init()` |
| `W5500_ERR_FAULT` | `NET_STATUS_FAULT` | `spi1_recover()` |
| init failed at boot | `NET_STATUS_DEAD` | none, every later request fails fast |

## Socket model

Eight sockets live in static memory (`get_socket(0..7)`, with `socket_memory_init()`
resetting them all). A `socket_t` is 8 bytes: number, port, protocol, status and a
`has_unreaped_send` flag.

```c
socket_t *sys_socket_get(uint8_t n);       // fetch a pool entry
void sys_socket_open(socket_t *s, net_reply_t *r);
void sys_socket_close(socket_t *s, net_reply_t *r);
void sys_net_link(int32_t *up, net_reply_t *r);
void sys_net_recv(socket_t *s, uint8_t *buf, uint16_t len, net_reply_t *r);
void sys_net_send(socket_t *s, net_send_args_t *args, net_reply_t *r);
```

`net_reply_t` carries the outcome and the data metadata (`got`, `src_ip`, `port`, `truncated`).

### Receive

`w5500_socket_recv()` reads the chip's 8-byte PACKET-INFO prefix (source IP, source port,
payload length), then the payload, then retires the buffer by advancing the read pointer
by `8 + len` regardless of how much was actually copied. A short buffer truncates the copy
but must not leave the chip's pointer behind, or every later read re-delivers the same
bytes. `truncated` tells the caller the datagram was larger than the buffer supplied.

### Send

`w5500_socket_send()` writes the payload into the transmit buffer, advances `Sn_TX_WR` and
issues `SEND`. Three properties matter.

1. **A datagram is atomic.** `SEND` transmits exactly `[Sn_TX_RD, Sn_TX_WR)` as one unit, so the free-space wait is for the whole length and a partial write is never a legal state.

2.  **The commit point is `Sn_CR == 0`.** When the command register clears, the buffer belongs to the chip, so `OK` means committed rather than delivered. `SENDOK` and
`TIMEOUT` arrive later and are reaped at the top of the next call, through
`has_unreaped_send`.

3. **Payload can be sent to a MAC.** `use_mac` selects `SEND_MAC`, which transmits to a MAC address instead of ARPing for an IP. It is required when sending to an address nobody owns yet.

## Invariants

1. Only `net_task` calls `w5500_*` or `spi1_*`.
2. At most one request is in flight, and the mailbox is read and written under `irq_save`.
3. A client blocks only inside the syscall that asked for the work.
4. Every blocking operation returns a status. No operation succeeds silently.
5. The service never assumes the chip is healthy. `ERR_STATE` triggers a full
   re-initialisation.

## Known gaps

1. Client pointers are not validated on the send path. The receive path checks its buffer
with `ptr_on_heap()`, but `tx_data`, `net_send_args_t` and `reply_slot` are dereferenced as given.
2. I/O is serialised.
3. There is no TCP.

