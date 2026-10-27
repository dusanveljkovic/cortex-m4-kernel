#!/usr/bin/env python3
"""UDP listener for testing the W5500 send path.

Usage:
    ./udp_listen.py                        # listen on 6767 until Ctrl-C
    ./udp_listen.py -p 68                  # (needs root) DHCP client port
    ./udp_listen.py --count 2              # exit after 2 datagrams  (P2)
    ./udp_listen.py --expect-len 300       # flag any other length
    ./udp_listen.py --quiet                # only print anomalies + summary

Checks performed on every datagram (after the first):
  DUP      identical bytes to the previous datagram
           -> the board re-sent a staged buffer: Sn_TX_WR never advanced,
              or the same datagram was SENT twice.
  PREFIX   this datagram starts with the whole previous payload
           -> stale bytes left inside [Sn_TX_RD, Sn_TX_WR).
  SHORT/  length differs from --expect-len
  LONG

The board-side failures this cannot see: a datagram that never left the chip
(stale SENDOK -> ghost success) shows up only as a *missing* datagram, and
ARPTO happens entirely inside the chip. Both are judged by the count and by
the board's own status output.
"""

import argparse
import socket
import sys
import time

RESET = "\033[0m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
DIM = "\033[2m"


def hexdump(data, limit=48):
    head = data[:limit]
    text = " ".join(f"{b:02x}" for b in head)
    if len(data) > limit:
        text += f" ... (+{len(data) - limit} bytes)"
    ascii_ = "".join(chr(b) if 32 <= b < 127 else "." for b in head)
    return text, ascii_


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", type=int, default=6767)
    ap.add_argument("--count", type=int, default=0,
                    help="exit after N datagrams (0 = run forever)")
    ap.add_argument("--expect-len", type=int, default=None)
    ap.add_argument("--timeout", type=float, default=None,
                    help="give up after N seconds of silence")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", args.port))
    if args.timeout:
        sock.settimeout(args.timeout)

    print(f"listening on 0.0.0.0:{args.port}"
          f"{' (expecting ' + str(args.expect_len) + ' bytes)' if args.expect_len else ''}")
    print(f"{DIM}^C to stop{RESET}\n")

    prev = None
    n = 0
    t0 = None
    problems = []

    try:
        while True:
            try:
                data, src = sock.recvfrom(65535)
            except socket.timeout:
                print(f"\n{YELLOW}timeout: no datagram for {args.timeout}s{RESET}")
                problems.append("timeout")
                break

            now = time.monotonic()
            if t0 is None:
                t0 = now
            n += 1
            dt = (now - t0) * 1000.0

            verdict = []
            if args.expect_len is not None and len(data) != args.expect_len:
                tag = "SHORT" if len(data) < args.expect_len else "LONG"
                verdict.append(f"{RED}{tag}{RESET} (expected {args.expect_len})")
                problems.append(tag)
            if prev is not None:
                if data == prev:
                    verdict.append(f"{RED}DUP{RESET} identical to previous")
                    problems.append("DUP")
                elif len(data) > len(prev) and data.startswith(prev):
                    verdict.append(f"{RED}PREFIX{RESET} previous payload found at "
                                   f"offset 0, +{len(data) - len(prev)} bytes after it")
                    problems.append("PREFIX")

            line = (f"#{n:<3} t+{dt:8.1f}ms  {src[0]}:{src[1]:<6} "
                    f"{len(data):>4}B  ")
            if verdict:
                print(line + "  ".join(verdict))
            elif not args.quiet:
                print(line + f"{GREEN}ok{RESET}")

            if not args.quiet or verdict:
                hx, asc = hexdump(data)
                print(f"{DIM}     {hx}")
                print(f"     {asc}{RESET}")

            prev = data
            if args.count and n >= args.count:
                break
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()

    print(f"\n--- {n} datagram(s) ---")
    if n == 0:
        print(f"{RED}nothing arrived{RESET}: the send never left the chip, "
              f"wrong destination IP/port, or the board is not on the wire.")
    elif problems:
        seen = sorted(set(problems))
        print(f"{RED}problems: {', '.join(seen)}{RESET}")
        if "DUP" in seen:
            print("  DUP   -> Sn_TX_WR not advancing, or SEND issued twice for one buffer")
        if "PREFIX" in seen:
            print("  PREFIX-> stale bytes inside [Sn_TX_RD, Sn_TX_WR): wrong write pointer,")
            print("           Sn_TX_WR advanced by more than was written, or a failed")
            print("           send's content left in the window")
        if "SHORT" in seen or "LONG" in seen:
            print("  len   -> Sn_TX_WR advanced by something other than what you wrote")
    else:
        print(f"{GREEN}all datagrams consistent{RESET}")
    if args.count and n < args.count:
        print(f"{RED}expected {args.count}, got {n}{RESET} -> a send reported success "
              f"without transmitting (stale SENDOK).")
    return 0 if (not problems and not (args.count and n < args.count)) else 1


if __name__ == "__main__":
    sys.exit(main())
