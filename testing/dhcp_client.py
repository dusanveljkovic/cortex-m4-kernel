#!/usr/bin/env python3
"""DHCP client harness for testing the STM32 DHCP server.

This is a *client*, not a packet injector (that's dhcp_send.py): it carries
state across messages, so it can tell you *which* part of DORA broke.

Usage:
    sudo ./dhcp_client.py --server 192.168.0.105
    sudo ./dhcp_client.py --server 192.168.0.105 --negative
    sudo ./dhcp_client.py --server 192.168.0.105 --twice
    sudo ./dhcp_client.py --server 192.168.0.105 -i eth0 --timeout 3

Exit code: 0 = every check passed, 1 = at least one failed.

Ports: binds 0.0.0.0:68 (privileged -> sudo). Free it first:
    ss -ulpn 'sport = :68'
Multi-homed host: pass -i/--iface, otherwise the kernel picks the egress.

TODO(marker): every function containing TODO is a stub. Fill it in; the
docstrings and the check lists are the spec, not a suggestion.
"""

import argparse
import random
import socket
import struct
import sys
import time

# --- protocol constants -----------------------------------------------------

SERVER_PORT = 6767
CLIENT_PORT = 6868
MAGIC_COOKIE = bytes([99, 130, 83, 99])
BOOTREQUEST, BOOTREPLY = 1, 2
ETH_HTYPE, ETH_HLEN = 1, 6
BROADCAST_FLAG = 0x8000

# option codes
OPT_SUBNET_MASK = 1
OPT_ROUTER = 3
OPT_DNS = 6
OPT_HOSTNAME = 12
OPT_DOMAIN = 15
OPT_BROADCAST_ADDR = 28
OPT_NTP = 42
OPT_REQUESTED_IP = 50
OPT_LEASE_TIME = 51
OPT_MESSAGE_TYPE = 53
OPT_SERVER_ID = 54
OPT_PARAM_LIST = 55
OPT_MAX_MSG_SIZE = 57
OPT_VENDOR_CLASS = 60
OPT_CLIENT_ID = 61

# message types
DISCOVER, OFFER, REQUEST, DECLINE, ACK, NAK, RELEASE, INFORM = range(1, 9)
TYPE_NAME = {
    DISCOVER: "DISCOVER",
    OFFER: "OFFER",
    REQUEST: "REQUEST",
    DECLINE: "DECLINE",
    ACK: "ACK",
    NAK: "NAK",
    RELEASE: "RELEASE",
    INFORM: "INFORM",
}

# what this client asks for in option 55
PARAM_LIST = [
    OPT_SUBNET_MASK,
    OPT_ROUTER,
    OPT_DNS,
    OPT_HOSTNAME,
    OPT_DOMAIN,
    OPT_BROADCAST_ADDR,
    OPT_NTP,
]
# asked for, but a minimal server cannot provide -> must be omitted, not empty
CANNOT_PROVIDE = [OPT_HOSTNAME, OPT_DOMAIN, OPT_NTP]

RED, GREEN, YELLOW, DIM, RESET = (
    "\033[31m",
    "\033[32m",
    "\033[33m",
    "\033[2m",
    "\033[0m",
)


# --- packet construction ----------------------------------------------------


def build_header(
    op,
    xid,
    chaddr,
    flags=0,
    ciaddr=b"\0" * 4,
    yiaddr=b"\0" * 4,
    siaddr=b"\0" * 4,
    giaddr=b"\0" * 4,
    secs=0,
    hops=0,
):
    """236 bytes, no cookie, no options."""
    pkt = struct.pack(
        "!BBBB4sHH4s4s4s4s16s64s128s",
        op,
        1,
        8,
        0,
        xid,
        0,
        flags,
        ciaddr,
        yiaddr,
        siaddr,
        giaddr,
        chaddr.ljust(16),
        b"\0" * 64,
        b"\0" * 128,
    )
    return pkt


def build_options(options):
    """(tag, value-bytes) pairs -> TLV bytes, END appended.

    Serve RESPONSES only through this function; option 55 is a *list of tags*,
    not a normal value, so it needs its own path.
    """
    ret = b""
    for opt_number, opt_val in options.items():
        val = opt_val
        if opt_number == OPT_PARAM_LIST:
            val = b""
            for param in opt_val:
                val = val + param.to_bytes(1)
        ret = ret + b"".join(
            [
                opt_number.to_bytes(1),
                len(opt_val).to_bytes(1),
                val,
            ]
        )
    ret = ret + bytes.fromhex("ff")

    return ret


def build(kind, xid, chaddr, client_id, **fields):
    """Full datagram: header + cookie + options, for the message `kind`.

    TODO: dispatch on kind.
      DISCOVER : 53, 61=client_id, 57=576, 55=bytes(PARAM_LIST), 60=b"udhcp 1.19.3"
                 ciaddr=0, yiaddr=0, options MUST NOT contain 50 or 54
      REQUEST  : 53=REQUEST, 61, 57, 55, 60, 54=server_id, 50=requested_ip,
                 ciaddr=0, yiaddr=0     (this is the SELECTING form)
      plus the four negative cases used by --negative, which are easier to
      build by hand than through this dispatcher.
    """
    if kind == DISCOVER:
        options = {
            OPT_MESSAGE_TYPE: DISCOVER.to_bytes(1, byteorder="big"),
            OPT_MAX_MSG_SIZE: (576).to_bytes(2, byteorder="big"),
            OPT_VENDOR_CLASS: b"udhcp 1.19.3",
            OPT_CLIENT_ID: client_id,
            OPT_PARAM_LIST: PARAM_LIST,
        }
    if kind == REQUEST:
        options = {
            OPT_MESSAGE_TYPE: REQUEST.to_bytes(1, byteorder="big"),
            OPT_MAX_MSG_SIZE: (576).to_bytes(2, byteorder="big"),
            OPT_VENDOR_CLASS: b"udhcp 1.19.3",
            OPT_CLIENT_ID: client_id,
            OPT_PARAM_LIST: PARAM_LIST,
        }
    option_bytes = build_options(options)
    header_bytes = build_header(BOOTREQUEST, xid, chaddr)
    full_bytes = header_bytes + MAGIC_COOKIE + option_bytes
    return full_bytes


# --- packet parsing (deliberately independent of the C parser) -------------


def parse(data):
    """Datagram -> dict. Mirror of dhcp_parse(), written without looking at it.

    TODO: return at least:
      op, htype, hlen, hops, xid, secs, flags, ciaddr/yiaddr/siaddr/giaddr
      (as dotted strings), chaddr (bytes), opts (dict tag -> value bytes)
    Rules: require len >= 240 and the cookie; walk from 240; PAD advances 1;
    END stops; every step bounds-checked *before* the read; a tag whose length
    overruns the datagram is an error, not a truncation.
    Raise ValueError on a malformed datagram so the caller can print it.
    """
    raise NotImplementedError


def hexdump(data, limit=64):
    head = data[:limit]
    hexb = " ".join(f"{b:02x}" for b in head)
    if len(data) > limit:
        hexb += f" ... (+{len(data) - limit})"
    asc = "".join(chr(b) if 32 <= b < 127 else "." for b in head)
    return f"{hexb}\n{DIM}     {asc}{RESET}"


# --- the checks: each returns a list of failure strings --------------------


def check_offer(msg, expect):
    """expect: {'xid', 'chaddr', 'server_id', 'client_id'}.

    Every item below is a predicted value, not the absence of an error.
    TODO: implement each bullet; return one string per violation.
      - op == BOOTREPLY
      - xid == expect['xid']
      - chaddr[:6] == expect['chaddr']
      - yiaddr != 0.0.0.0
      - opts[54] present and == expect['server_id']       (another server = fail)
      - opts[51] present, len 4, value > 0
      - opts[61] present and byte-identical to expect['client_id']   (RFC 6842)
      - for t in PARAM_LIST not in CANNOT_PROVIDE: opts[t] present
      - for t in CANNOT_PROVIDE: t not in opts             (must omit, not empty)
      - len(data) >= 300
      - END (0xff) found at or before the end
    Print nothing here; just return the list.
    """
    raise NotImplementedError


def check_ack(msg, expect):
    """Same set as check_offer, plus: yiaddr == expect['requested_ip'].

    TODO: reuse check_offer and extend.
    """
    raise NotImplementedError


# --- socket plumbing -------------------------------------------------------


def open_socket(iface=None):
    """Bind 0.0.0.0:68, enable broadcast, optionally pin the interface."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    if iface:
        # SO_BINDTODEVICE, needs root -- keeps a multi-homed host deterministic
        sock.setsockopt(socket.SOL_SOCKET, 25, iface.encode() + b"\0")
    sock.bind(("0.0.0.0", CLIENT_PORT))
    return sock


def send(sock, data, dst="255.255.255.255"):
    """DISCOVER/REQUEST go to the broadcast address, from port 68."""
    sock.sendto(data, (dst, SERVER_PORT))
    print(
        f"{DIM}  -> {TYPE_NAME.get(parse_type(data), '?'):8} "
        f"{len(data)}B to {dst}:{SERVER_PORT}{RESET}"
    )


def parse_type(data):
    """Cheap peek at option 53 without a full parse.

    TODO: walk options, return the message type or None (BOOTP).
    """
    raise NotImplementedError


def recv_matching(sock, xid, expect_server_id, timeout):
    """Wait for a datagram whose xid matches; ignore and report other servers.

    TODO: loop until deadline.
      - parse; on ValueError print and continue
      - if xid mismatch: print (noisy client traffic uses other xids)
      - if opts[54] != expect_server_id: print
        "offer from <ip> -- not us (router DHCP still on?)" and continue
      - on timeout: return None (the caller decides if that is a failure)
    """
    raise NotImplementedError


# --- the two runs ----------------------------------------------------------


def run_happy_path(sock, args):
    """DISCOVER -> OFFER -> REQUEST -> ACK. Returns list of failures.

    TODO: the state machine.
      xid = random 32-bit, chaddr = args.mac, client_id = b"\\x01" + chaddr
      1. send DISCOVER (broadcast)
      2. recv_matching -> None means "no OFFER": FAIL and stop, printing that
         this also happens when the router wins the race
      3. check_offer; only continue to REQUEST if the offer named us
      4. send REQUEST with 54=server_id, 50=yiaddr
      5. recv_matching -> check_ack
    A server that never answers and a server that answers wrongly are different
    bugs; keep their failure strings distinguishable.
    """
    raise NotImplementedError


def run_negative(sock, args):
    """Garbage first, then a valid DISCOVER. The second half is the point.

    TODO: for each case, send it, then assert *silence* for ~1s:
      - dhcp_send.py-style datagram (cookie, no option 53)   -> BOOTP, not ours
      - cookie deadbeef                                      -> not DHCP
      - 200 bytes                                            -> cannot be DHCP
      - 240 bytes ending exactly on a tag byte               -> no OOB read
      - option 53 with len 2                                 -> never half-read
    Then send a real DISCOVER and require an OFFER. A server that dies on
    garbage is indistinguishable from one that ignored it until you ask it a
    question it must answer.
    """
    raise NotImplementedError


def run_twice(sock, args):
    """Two DISCOVERs, same client-id -> the same yiaddr.

    TODO: the I3 check. Requires the server to remember, so it will fail
    until the lease table exists. Failure here is informative, not fatal.
    """
    raise NotImplementedError


# --- entry point -----------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument(
        "--server",
        required=True,
        help="server identifier we are testing, e.g. 192.168.0.105",
    )
    ap.add_argument(
        "-m",
        "--mac",
        default="02:00:00:00:00:01",
        help="fake chaddr (locally administered)",
    )
    ap.add_argument(
        "-i", "--iface", default=None, help="pin the egress interface (needs root)"
    )
    ap.add_argument(
        "--timeout", type=float, default=2.0, help="seconds to wait for each reply"
    )
    ap.add_argument("--negative", action="store_true", help="run the garbage cases")
    ap.add_argument("--twice", action="store_true", help="run the same-client-id case")
    args = ap.parse_args()

    mac = bytes(int(x, 16) for x in args.mac.split(":"))
    sock = open_socket(args.iface)
    print(
        f"client chaddr {args.mac}  client-id 01{mac.hex()}  "
        f"testing server {args.server}\n"
    )

    # print(options)
    xid = bytes.fromhex("deadbeef")
    print(hexdump(build(REQUEST, xid, mac, mac), limit=300))
    # failures = []
    # try:
    #     failures += run_happy_path(sock, args)
    #     if args.negative:
    #         failures += run_negative(sock, args)
    #     if args.twice:
    #         failures += run_twice(sock, args)
    # except KeyboardInterrupt:
    #     print("\ninterrupted")
    # finally:
    #     sock.close()
    #
    # print()
    # if not failures:
    #     print(f"{GREEN}ALL CHECKS PASSED{RESET}")
    #     return 0
    # for f in failures:
    #     print(f"{RED}FAIL{RESET} {f}")
    # print(f"\n{RED}{len(failures)} failure(s){RESET}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
