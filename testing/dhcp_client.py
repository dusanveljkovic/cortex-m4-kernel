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
        6,
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
                len(val).to_bytes(1),
                val,
            ]
        )
    ret = ret + bytes.fromhex("ff")

    return ret


def build(kind, xid, chaddr, client_id, broadcast_flag, **fields):
    """Full datagram: header + cookie + options, for the message `kind`."""
    options = {
        OPT_MESSAGE_TYPE: kind.to_bytes(1, byteorder="big"),
        OPT_MAX_MSG_SIZE: (576).to_bytes(2, byteorder="big"),
        OPT_VENDOR_CLASS: b"udhcp 1.19.3",
        OPT_CLIENT_ID: client_id,
        OPT_PARAM_LIST: PARAM_LIST,
    }
    if kind == REQUEST or kind == DECLINE:
        options[OPT_REQUESTED_IP] = fields["opts"][OPT_REQUESTED_IP]
        options[OPT_SERVER_ID] = fields["opts"][OPT_SERVER_ID]
    option_bytes = build_options(options)
    header_bytes = build_header(
        BOOTREQUEST,
        xid,
        chaddr,
        0x8000 if broadcast_flag else 0,
        ciaddr=fields.get("ciaddr", b"\0" * 4),
    )
    full_bytes = header_bytes + MAGIC_COOKIE + option_bytes
    return full_bytes


# --- packet parsing (deliberately independent of the C parser) -------------


def parse(data):
    """Datagram -> dict. Mirror of dhcp_parse(), written without looking at it."""
    data_dict = {}
    if len(data) < 240 or (data[236:240] != MAGIC_COOKIE):
        return None

    data_dict["raw_length"] = len(data)
    data_dict["raw"] = data

    data_dict["op"] = data[0]
    data_dict["htype"] = data[1]
    data_dict["hlen"] = data[2]
    data_dict["hops"] = data[3]

    data_dict["xid"] = data[4:8]
    data_dict["secs"] = int.from_bytes(data[8:10], byteorder="big")
    data_dict["flags"] = int.from_bytes(data[10:12], byteorder="big")

    data_dict["ciaddr"] = f"{data[12]}.{data[13]}.{data[14]}.{data[15]}"
    data_dict["yiaddr"] = f"{data[16]}.{data[17]}.{data[18]}.{data[19]}"
    data_dict["siaddr"] = f"{data[20]}.{data[21]}.{data[22]}.{data[23]}"
    data_dict["giaddr"] = f"{data[24]}.{data[25]}.{data[26]}.{data[27]}"

    data_dict["chaddr"] = data[28:34]

    options_pos = 240
    options = {}
    while options_pos + 1 < len(data):
        code = data[options_pos]
        if code == 0:
            options_pos += 1
            continue
        if code == 255:
            break
        data_len = data[options_pos + 1]
        options_pos += 2
        if options_pos + data_len > len(data):
            return None

        opt_data = data[options_pos : (options_pos + data_len)]
        options[code] = opt_data

        options_pos += data_len

    data_dict["opts"] = options

    return data_dict


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
    """
    failure_strings = []
    if msg["op"] != BOOTREPLY:
        failure_strings.append(f"msg op {msg['op']} != BOOTREPLY")
    if msg["xid"] != expect["xid"]:
        failure_strings.append(f"xid mismatch {msg['xid']} != {expect['xid']}")
    if msg["chaddr"][:6] != expect["chaddr"]:
        failure_strings.append(
            f"chaddr mismatch {msg['chaddr'][:6]} != {expect['chaddr']}"
        )
    if msg["yiaddr"] == "0.0.0.0":
        failure_strings.append("yiaddr 0.0.0.0")
    if OPT_SERVER_ID not in msg["opts"]:
        failure_strings.append("OPT_SERVER_ID[54] not present")
    if (
        OPT_SERVER_ID in msg["opts"]
        and msg["opts"][OPT_SERVER_ID] != expect["server_id"]
    ):
        failure_strings.append(
            f"server id mismatch {msg['opts'][OPT_SERVER_ID]} != {expect['server_id']}"
        )

    if OPT_LEASE_TIME not in msg["opts"]:
        failure_strings.append("OPT_LEASE_TIME[51] not present")
    if OPT_CLIENT_ID not in msg["opts"]:
        failure_strings.append("OPT_CLIENT_ID[61] not present")
    if (
        OPT_CLIENT_ID in msg["opts"]
        and msg["opts"][OPT_CLIENT_ID] != expect["client_id"]
    ):
        failure_strings.append(
            f"client id mismatch {msg['opts'][OPT_CLIENT_ID]} != {expect['client_id']}"
        )
    for t in sorted(set(PARAM_LIST) - set(CANNOT_PROVIDE)):
        if t not in msg["opts"]:
            failure_strings.append(f"param {t} not in {msg['opts']}")
    for t in CANNOT_PROVIDE:
        if t in msg["opts"]:
            failure_strings.append(f"param {t} in {msg['opts']}")
    if msg["raw_length"] < 300:
        failure_strings.append(f"length {msg['raw_length']} < 300")

    return failure_strings


def check_ack(msg, expect):
    """Same set as check_offer, plus: yiaddr == expect['requested_ip']."""
    failure_strings = check_offer(msg, expect)
    if socket.inet_aton(msg["yiaddr"]) != expect["requested_ip"]:
        failure_strings.append(
            f"requested ip mismatch {msg['yiaddr']} != {expect['requested_ip']}"
        )
    return failure_strings


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
    """Cheap peek at option 53 without a full parse."""
    data_dict = parse(data)
    if data_dict is None or OPT_MESSAGE_TYPE not in data_dict["opts"]:
        return None
    return data_dict["opts"][OPT_MESSAGE_TYPE][0]


def recv_matching(sock, xid, expect_server_id, timeout):
    """Wait for a datagram whose xid matches; ignore and report other servers."""
    sock.settimeout(timeout)
    while True:
        try:
            data, address = sock.recvfrom(1500)
            data_dict = parse(data)
            if data_dict["xid"] != xid:
                print(f"recv xid mismatch {data_dict['xid']} != {xid}")
                continue
            if data_dict["opts"][OPT_SERVER_ID] != expect_server_id:
                print(f"offer from another server {data_dict['opts'][OPT_SERVER_ID]}")
                continue
            return data_dict
        except TimeoutError:
            return None
        except ValueError as e:
            print(e)
            return None


# --- the two runs ----------------------------------------------------------


def run_happy_path(sock, args):
    """DISCOVER -> OFFER -> DECLINE -> OFFER -> REQUEST -> ACK -> RELEASE. Returns list of failures."""
    xid = random.randrange(1 << 32)
    xid = xid.to_bytes(4, byteorder="big")
    chaddr = bytes.fromhex("".join(args.mac.split(":")))
    client_id = b"\x01" + chaddr

    server_id = socket.inet_aton(args.server)

    # 1. send DISCOVER
    send(sock, build(DISCOVER, xid, chaddr, client_id, args.bcast))

    # 2. receive OFFER
    data_dict = recv_matching(sock, xid, server_id, args.timeout)
    if data_dict is None:
        return [f"{RED}no OFFER{RESET}"]

    # 3. check OFFER
    failure_strings = check_offer(
        data_dict,
        {
            "xid": xid,
            "chaddr": chaddr,
            "client_id": client_id,
            "server_id": server_id,
        },
    )

    requested_ip = data_dict["yiaddr"]
    requested_ip = socket.inet_aton(requested_ip)

    # 4. send DECLINE with offered ip
    send(
        sock,
        build(
            DECLINE,
            xid,
            chaddr,
            client_id,
            args.bcast,
            opts={OPT_REQUESTED_IP: requested_ip, OPT_SERVER_ID: server_id},
        ),
    )

    # 5. receive new OFFER
    data_dict = recv_matching(sock, xid, server_id, args.timeout)
    if data_dict is None:
        return [f"{RED} no back from DECLINE{RESET}"]

    # 6. check new OFFER
    failure_strings += check_offer(
        data_dict,
        {
            "xid": xid,
            "chaddr": chaddr,
            "client_id": client_id,
            "server_id": server_id,
        },
    )

    requested_ip = data_dict["yiaddr"]
    requested_ip = socket.inet_aton(requested_ip)

    # 7. send REQUEST with new ip
    send(
        sock,
        build(
            REQUEST,
            xid,
            chaddr,
            client_id,
            args.bcast,
            opts={OPT_REQUESTED_IP: requested_ip, OPT_SERVER_ID: server_id},
        ),
    )

    # 8. receive ACK
    data_dict = recv_matching(sock, xid, server_id, args.timeout)
    if data_dict is None:
        return [f"{RED} no back from REQUEST{RESET}"]

    # 9. check ACK
    failure_strings += check_ack(
        data_dict,
        {
            "xid": xid,
            "chaddr": chaddr,
            "client_id": client_id,
            "server_id": server_id,
            "requested_ip": requested_ip,
        },
    )

    # 10. send RELEASE
    send(
        sock,
        build(
            RELEASE,
            xid,
            chaddr,
            client_id,
            args.bcast,
            opts={OPT_REQUESTED_IP: requested_ip, OPT_SERVER_ID: server_id},
            ciaddr=requested_ip,
        ),
    )
    return failure_strings


def run_negative(sock, args):
    """Garbage first, then a valid DISCOVER. The second half is the point."""

    server_id = socket.inet_aton(args.server)
    xid = random.randrange(1 << 32)
    pkt_without_options = struct.pack(
        "!BBBBIHH4s4s4s4s16s64s128s",
        1,
        1,
        6,
        0,  # op=BOOTREQUEST, htype, hlen, hops
        xid,
        0,
        0x8000,  # xid -> buffer[4:8], secs, broadcast flag
        b"\0" * 4,
        b"\0" * 4,
        b"\0" * 4,
        b"\0" * 4,  # ciaddr yiaddr siaddr giaddr
        bytes.fromhex("eedfeeffeeff") + b"\0" * 10,  # chaddr -> buffer[28:34]
        b"\0" * 64,
        b"\0" * 128,  # sname, file
    ) + bytes(
        [99, 130, 83, 99]
    )  # magic cookie at 236
    send(sock, pkt_without_options)
    failure_strings = []
    if recv_matching(sock, xid, server_id, 1) is not None:
        failure_strings.append("Server replied to a message with no option 53")

    pkt_wrong_magic = struct.pack(
        "!BBBBIHH4s4s4s4s16s64s128s",
        1,
        1,
        6,
        0,  # op=BOOTREQUEST, htype, hlen, hops
        xid,
        0,
        0x8000,  # xid -> buffer[4:8], secs, broadcast flag
        b"\0" * 4,
        b"\0" * 4,
        b"\0" * 4,
        b"\0" * 4,  # ciaddr yiaddr siaddr giaddr
        bytes.fromhex("eedfeeffeeff") + b"\0" * 10,  # chaddr -> buffer[28:34]
        b"\0" * 64,
        b"\0" * 128,  # sname, file
    ) + bytes(
        [00, 11, 22, 33]
    )  # magic cookie at 236
    send(sock, pkt_wrong_magic)
    if recv_matching(sock, xid, server_id, 1) is not None:
        failure_strings.append("Server replied to a message with wrong magic cookie")

    small_msg = b"\xef" * 200
    send(sock, small_msg)
    if recv_matching(sock, xid, server_id, 1) is not None:
        failure_strings.append("Server replied to a message with size 200")

    malformed_msg = pkt_without_options + b"\53"
    send(sock, malformed_msg)
    if recv_matching(sock, xid, server_id, 1) is not None:
        failure_strings.append("Server replied to a malformed message")

    xid = random.randrange(1 << 32)
    xid = xid.to_bytes(4, byteorder="big")
    chaddr = bytes.fromhex("".join(args.mac.split(":")))
    client_id = b"\x01" + chaddr

    send(sock, build(DISCOVER, xid, chaddr, client_id, args.bcast))

    data_dict = recv_matching(sock, xid, server_id, args.timeout)
    if data_dict is None:
        failure_strings.append(f"{RED}no OFFER{RESET}")

    failure_strings_2 = check_offer(
        data_dict,
        {
            "xid": xid,
            "chaddr": chaddr,
            "client_id": client_id,
            "server_id": server_id,
        },
    )
    failure_strings = failure_strings + failure_strings_2
    return failure_strings


def run_twice(sock, args):
    """Two DISCOVERs, same client-id -> the same yiaddr."""
    xid = random.randrange(1 << 32)
    xid = xid.to_bytes(4, byteorder="big")
    chaddr = bytes.fromhex("".join(args.mac.split(":")))
    client_id = b"\x01" + chaddr
    server_id = socket.inet_aton(args.server)
    failure_strings = []

    send(sock, build(DISCOVER, xid, chaddr, client_id, args.bcast))

    data_dict = recv_matching(sock, xid, server_id, args.timeout)
    if data_dict is None:
        return [f"{RED}no OFFER{RESET}"]

    first_yiaddr = data_dict["yiaddr"]

    send(sock, build(DISCOVER, xid, chaddr, client_id, args.bcast))

    data_dict = recv_matching(sock, xid, server_id, args.timeout)
    if data_dict is None:
        return [f"{RED}no OFFER{RESET}"]

    second_yiaddr = data_dict["yiaddr"]
    if first_yiaddr != second_yiaddr:
        failure_strings.append(
            f"Yiaddr mismatch on same client-id dicsover {first_yiaddr} != {second_yiaddr}"
        )

    send(sock, build(DISCOVER, xid, chaddr, b"\x02" + chaddr, args.bcast))

    data_dict = recv_matching(sock, xid, server_id, args.timeout)
    if data_dict is None:
        return [f"{RED}no OFFER{RESET}"]
    third_yiaddr = data_dict["yiaddr"]

    if first_yiaddr == third_yiaddr:
        failure_strings.append(
            f"Yiaddr match on different client-id dicsover {first_yiaddr} == {third_yiaddr}"
        )

    return failure_strings


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
    ap.add_argument("--bcast", action="store_true", help="set the B flag")
    args = ap.parse_args()

    mac = bytes(int(x, 16) for x in args.mac.split(":"))
    sock = open_socket(args.iface)
    print(
        f"client chaddr {args.mac}  client-id 01{mac.hex()}  "
        f"testing server {args.server}\n"
    )

    failures = []
    try:
        failures += run_happy_path(sock, args)
        if args.negative:
            failures += run_negative(sock, args)
        if args.twice:
            failures += run_twice(sock, args)
    except KeyboardInterrupt:
        print("\ninterrupted")
    finally:
        sock.close()

    print()
    if not failures:
        print(f"{GREEN}ALL CHECKS PASSED{RESET}")
        return 0
    for f in failures:
        print(f"{RED}FAIL{RESET} {f}")
    print(f"\n{RED}{len(failures)} failure(s){RESET}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
