import socket, struct, random, sys

dst = sys.argv[1] if len(sys.argv) > 1 else "192.168.0.105"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 67
count = int(sys.argv[3]) if len(sys.argv) > 3 else 1

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

for i in range(count):
    xid = random.randrange(1 << 32)
    pkt = struct.pack(
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
    s.sendto(pkt, (dst, port))
    print(f"sent xid={xid:#010x} ({len(pkt)} bytes) to {dst}:{port}")
