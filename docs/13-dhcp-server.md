# DHCP server

**Files:** `source/user/dhcp.c`, `include/user/dhcp.h`,
`source/kernel/flash.c` and `include/kernel/flash.h` (journal storage),
`testing/dhcp_client.py` (host-side test client)

## Where this sits

The DHCP server is an application and not a kernel subsystem. It runs as an ordinary task and uses syscalls (`sys_net_*`, `sys_malloc`, ...) to communicate with the outside world. It does not know about SPI or W5500, it only knows how to send and receive network packets which is exactly what it needs to know.

The server binds one UDP socket at MAC `EE:FF:EE:FF:EE:FF` and IP `192.168.0.105`, and
answers BOOTP/DHCP clients on port 6767. The client port is 6868. Both are
non-standard on purpose, so the host-side harness can run as an unprivileged process. A
standards-compliant deployment uses 67 and 68.

## The wire format

A BOOTP/DHCP message is a 236-byte positional header, then a magic cookie, then a TLV
option list.

| Offset | Field | Parsed as |
|---|---|---|
| 0 | `op` | `BOOTREQUEST` (1) or `BOOTREPLY` (2) |
| 1 to 3 | `htype`, `hlen`, `hops` | Ethernet is 1 and 6 |
| 4 to 7 | `xid` | transaction ID, echoed verbatim |
| 8 to 11 | `secs`, `flags` | bit 15 of `flags` is the broadcast request |
| 12 to 27 | `ciaddr`, `yiaddr`, `siaddr`, `giaddr` | client, "your", next server, relay |
| 28 to 43 | `chaddr[16]` | MAC in the first 6 bytes |
| 44 to 235 | `sname[64]`, `file[128]` | not parsed |
| 236 to 239 | magic cookie | `99.130.83.99` |
| 240 onward | options | TLV list, `PAD` is 0, `END` is 255 |

### Option parsing

```c
while (pos + 1 < len) {
  code = buf[pos];
  if (code == PAD)  { pos += 1;  continue; }   // 0: advances one byte, no length
  if (code == END)  break;                     // 255: stop
  pos++;  data_len = buf[pos];  pos++;
  if (pos + data_len > len) return DHCP_ERR_PARSING;
  ...
}
```

The tag byte decides whether the next byte is a length, which is why `PAD` is the only
option that is one byte long. `len` is the length of data received from socket so if the parser need to parse data outside it, it results in an parsing error.

Options understood: 
1. 50 - requested IP
2. 51 - lease time
3. 53 - message type
4. 54 - server identifier
5. 55 - parameter request list (stored as a 256 length bitmap)
6. 61 - client identifier

### Failure codes, partitioned by recovery

| Code | Meaning | What the caller does |
|---|---|---|
| `DHCP_ERR_SHORT` | datagram shorter than the fixed header | ignore the datagram |
| `DHCP_ERR_NOT_DHCP` | no magic cookie | ignore the datagram |
| `DHCP_ERR_PARSING` | an option's declared length overruns the datagram | ignore the datagram |
| `DHCP_ERR_MALFORMED` | a known option has the wrong value length | ignore the datagram |
| `DHCP_ERR_ARG` | programmer error | returns to the caller |

## Client identity

A lease is keyed by option 61 when present, and by `chaddr` otherwise. That is 16 bytes
with the length stored. Two clients can share a MAC in spoofed or virtualised setups, so the client identifier wins when one is offered.

## Building a reply

`op` becomes `BOOTREPLY`, `yiaddr` becomes the address being assigned, `secs` and `hops` are zeroed,
and the options are rebuilt from the configuration and the client's PRL. Everything else
(`xid`, `flags`, `giaddr`, `chaddr`, the cookie) is copied verbatim.

Options written: 53, 54, 51, 1, 3, 6 and 28. Which of those appear is decided by the PRL.

## Addressing the reply

There are three mechanisms for getting a reply to a client that has no address yet, so the
destination is chosen by an ordered ladder.

| Rung | Condition | Destination | `use_mac` |
|---|---|---|---|
| 1 | `giaddr` is not zero | the relay agent | no |
| 2 | `ciaddr` is not zero | the client's own address | no |
| 3 | broadcast flag set | `255.255.255.255` | no |
| 4 | otherwise | `yiaddr` | yes, `SEND_MAC` |

## Lease table

One slot per address in `[start_addr, end_addr]`, in a heap allocation of
`sizeof(lease_slot_t)` times the pool size.

| State | Meaning | Lifetime |
|---|---|---|
| `LEASE_ST_FREE` | available | none |
| `LEASE_ST_OFFERED` | held for a client that received an OFFER | 60 s |
| `LEASE_ST_BOUND` | lease granted and acknowledged | the configured `lease_time` |
| `LEASE_ST_DECLINED` | the client found the address in use | quarantine, 300 s |

Expiry is lazy, meaning there is no reaper task and no timer callback. A slot is alive if `slot_alive()` says so at the moment it is used:

```c
return slot->state != LEASE_ST_FREE &&
       (int32_t)(now_seconds - slot->expires_at) < 0;
```

The signed comparison is the wraparound-safe idiom.

## The flash journal

Leases have to survive a reboot, which can be achivied by saving them on the FLASH. Using the FLASH imposes two constraints: a bit can only go from 1 to 0 without an erase, and an erase is per sector. This means a record cannot be updated in place, so storage is designed as a append-only log.

Sector 3 (`0x0800C000`, 16 KB) holds the journal. The header record is 16 bytes: the magic
`LHDR`, a version, the record size (32), the pool base address, the pool size and a CRC.
Each lease record is 32 bytes: the magic `LSLT`, the state, the key length, the address,
`expires_at` little-endian, the 16-byte key, and a CRC of the preceding 31 bytes in the
last byte.

Three consequences:
1. A later record is further into the sector, so the last valid record for an address wind on replay.
2. The CRC is a commit marks and protects against a torn write. Observing a bad CRC produces a compaction request.
3. Compaction rewrites the header and every live slot into a freshly erased sector.

## Invariants

1. A datagram is accepted only if every declared option length fits inside the reported
   datagram length.
2. One slot per address.
3. A reply echoes the request: `xid`, `flags`, `giaddr`, `chaddr` and option 61 are
   byte-identical to what arrived.
4. A reply is never sent to an address that cannot be resolved without `SEND_MAC`.
5. The last valid journal record for an address wins, and no record is interpreted without passing CRC.

## Verification

`testing/dhcp_client.py` is a stateful client used for verification. It carries
context across messages, so a failure tells you which rung of the exchange broke. Its
happy path is `DISCOVER -> OFFER -> DECLINE -> OFFER -> REQUEST -> ACK -> RELEASE`

By supplying the script with command argument `--negative` it runs the tests for malformed inputs and by giving it `--twice` it simulates two clients and two leases.

The next step in testing is testing this against a real DHCP client implementation like `udhcpc`.

## Known gaps

1. `NAK` and `INFORM` are not implemented
2. Expiry is stored as uptime coordinates, but uptime restarts at zero on reset so a lease read on reboot gets a fresh lifetime.
3. CRC is only a 1-byte value, meaning a wrong one has a 1-256 change to be right.
4. The task writes `FLASH` directly
5. Not tested against a real DHCP client implementation
