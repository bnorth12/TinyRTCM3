#!/usr/bin/env python3
"""Factory-restore LC29H: PQTMRESTOREPAR then PAIR023."""
from __future__ import annotations

import argparse
import time

import serial


def nmea_checksum(payload: str) -> str:
    c = 0
    for ch in payload:
        c ^= ord(ch)
    return f"{c:02X}"


def make_sentence(payload: str) -> str:
    return f"${payload}*{nmea_checksum(payload)}\r\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    cmds = ["PQTMRESTOREPAR", "PAIR023"]
    ser = serial.Serial(port=args.port, baudrate=args.baud, timeout=0.3)
    print(f"Opened {args.port} @ {args.baud}")
    try:
        time.sleep(0.2)
        ser.reset_input_buffer()
        for p in cmds:
            line = make_sentence(p)
            print("TX", repr(line))
            ser.write(line.encode("ascii"))
            ser.flush()
            time.sleep(0.6 if p != "PAIR023" else 0.2)
        print("Waiting for reboot (~3s)...")
        time.sleep(3.0)
        leftover = ser.read(16384)
        print(f"RX after reset: {len(leftover)} bytes")
        text = leftover.decode("ascii", errors="replace")
        for marker in ("PQTMRESTOREPAR", "PAIR001", "PAIR023"):
            if marker in text:
                print(f"saw {marker} in RX")
        for line in text.splitlines()[:6]:
            if line.startswith("$"):
                print(" ", line[:120])
    finally:
        ser.close()
    print("Factory restore complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
