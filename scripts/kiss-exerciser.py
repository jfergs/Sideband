#!/usr/bin/env python3
"""Send simple KISS or raw test payloads to a Sideband TCP bridge."""

from __future__ import annotations

import argparse
import socket
import sys


FEND = 0xC0
FESC = 0xDB
TFEND = 0xDC
TFESC = 0xDD


def parse_hex(value: str) -> bytes:
    cleaned = value.replace(" ", "").replace(":", "")
    if len(cleaned) % 2 != 0:
        raise argparse.ArgumentTypeError("hex payload must contain an even number of digits")
    try:
        return bytes.fromhex(cleaned)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(str(exc)) from exc


def escape_kiss(payload: bytes) -> bytes:
    encoded = bytearray()
    for byte in payload:
        if byte == FEND:
            encoded.extend((FESC, TFEND))
        elif byte == FESC:
            encoded.extend((FESC, TFESC))
        else:
            encoded.append(byte)
    return bytes(encoded)


def kiss_frame(payload: bytes) -> bytes:
    return bytes((FEND,)) + escape_kiss(payload) + bytes((FEND,))


def hexdump(data: bytes) -> str:
    lines: list[str] = []
    for offset in range(0, len(data), 16):
        chunk = data[offset : offset + 16]
        hex_bytes = " ".join(f"{byte:02x}" for byte in chunk)
        ascii_bytes = "".join(chr(byte) if 32 <= byte <= 126 else "." for byte in chunk)
        lines.append(f"{offset:04x}: {hex_bytes:<47} {ascii_bytes}")
    return "\n".join(lines)


def build_payload(args: argparse.Namespace) -> bytes:
    if args.raw_text is not None:
        raw = args.raw_text.encode("ascii")
        if args.cr:
            raw += b"\r"
        return raw

    payload = args.payload_hex if args.payload_hex is not None else b"\x00"
    if args.malformed:
        return bytes((FEND, FESC, 0x01, FEND))
    return kiss_frame(payload)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.4.1", help="Sideband bridge host")
    parser.add_argument("--port", type=int, default=8001, help="Sideband bridge TCP port")
    parser.add_argument("--timeout", type=float, default=1.0, help="socket timeout seconds")
    parser.add_argument("--payload-hex", type=parse_hex, help="KISS payload bytes before framing")
    parser.add_argument("--malformed", action="store_true", help="send a malformed escaped KISS frame")
    parser.add_argument("--raw-text", help="send ASCII text without KISS framing")
    parser.add_argument("--cr", action="store_true", help="append carriage return to --raw-text")
    parser.add_argument("--read", action="store_true", help="read and print response bytes until timeout")
    args = parser.parse_args()

    if args.malformed and args.raw_text is not None:
        parser.error("--malformed cannot be combined with --raw-text")

    outbound = build_payload(args)
    with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
        sock.settimeout(args.timeout)
        sock.sendall(outbound)
        print(f"sent {len(outbound)} bytes")
        print(hexdump(outbound))

        if not args.read:
            return 0

        chunks: list[bytes] = []
        while True:
            try:
                chunk = sock.recv(4096)
            except TimeoutError:
                break
            if not chunk:
                break
            chunks.append(chunk)

    inbound = b"".join(chunks)
    print(f"received {len(inbound)} bytes")
    if inbound:
        print(hexdump(inbound))
    return 0


if __name__ == "__main__":
    sys.exit(main())
