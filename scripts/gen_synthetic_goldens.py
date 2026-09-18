#!/usr/bin/env python3
"""Generate synthetic RTCM3 golden frames for CI (no real site coordinates).

v0.1 emits:
  - empty payload frame (CRC smoke)
  - bit-accurate RTCM 1005 using published dummy ECEF (152-bit payload)

Output: test/golden/synthetic/
"""
from __future__ import annotations

import json
from pathlib import Path

POLY = 0x1864CFB
PUBLISH_STATION_ID = 0
# 0.0001 m units — matches TinyRtcmTypes.h
PUBLISH_ECEF_X = 63781370000
PUBLISH_ECEF_Y = 0
PUBLISH_ECEF_Z = 0

# RTCM 10403.x 1005: 12+12+6+4+38+1+1+38+2+38 = 152 bits (19 bytes)
MSG1005_PAYLOAD_BITS = 152


def crc24q(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 16
        for _ in range(8):
            crc <<= 1
            if crc & 0x1000000:
                crc ^= POLY
    return crc & 0xFFFFFF


def wrap_frame(payload: bytes) -> bytes:
    if len(payload) > 1023:
        raise ValueError("payload too long")
    hdr = bytes([0xD3, (len(payload) >> 8) & 0x03, len(payload) & 0xFF])
    body = hdr + payload
    c = crc24q(body)
    return body + bytes([(c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF])


class BitWriter:
    def __init__(self) -> None:
        self._bits: list[int] = []

    def put(self, value: int, n: int) -> None:
        for i in range(n - 1, -1, -1):
            self._bits.append((value >> i) & 1)

    def put_signed(self, value: int, n: int) -> None:
        self.put(value & ((1 << n) - 1), n)

    def to_bytes(self) -> bytes:
        out = bytearray((len(self._bits) + 7) // 8)
        for i, bit in enumerate(self._bits):
            if bit:
                out[i // 8] |= 1 << (7 - (i % 8))
        return bytes(out)


def make_1005() -> bytes:
    """RTCM 1005 payload: dummy publish ARP (REQ-COD-1005-D golden).

    Layout (152 bits): msg12, station12, ITRF6, GPS/GLO/GAL/ref (1 each),
    X38, osc1, res1, Y38, QCI/res2, Z38.
    """
    w = BitWriter()
    w.put(1005, 12)  # DF002 message number
    w.put(PUBLISH_STATION_ID, 12)  # DF003
    w.put(0, 6)  # DF021 ITRF realization year
    w.put(1, 1)  # DF022 GPS indicator
    w.put(0, 1)  # DF023 GLONASS
    w.put(0, 1)  # DF024 Galileo
    w.put(0, 1)  # DF141 reference-station indicator
    w.put_signed(PUBLISH_ECEF_X, 38)  # DF025 X
    w.put(0, 1)  # DF142 oscillator
    w.put(0, 1)  # DF001 reserved
    w.put_signed(PUBLISH_ECEF_Y, 38)  # DF026 Y
    w.put(0, 2)  # DF364 quarter-cycle indicator (reserved in 10403.1)
    w.put_signed(PUBLISH_ECEF_Z, 38)  # DF027 Z
    if len(w._bits) != MSG1005_PAYLOAD_BITS:
        raise RuntimeError(f"1005 payload is {len(w._bits)} bits, expected {MSG1005_PAYLOAD_BITS}")
    payload = w.to_bytes()
    if len(payload) != 19:
        raise RuntimeError(f"1005 payload is {len(payload)} bytes, expected 19")
    return wrap_frame(payload)


def msg_type(frame: bytes) -> int:
    return (frame[3] << 4) | (frame[4] >> 4)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    out_dir = root / "test" / "golden" / "synthetic"
    out_dir.mkdir(parents=True, exist_ok=True)

    empty = wrap_frame(b"")
    f1005 = make_1005()

    frames = {
        "empty.bin": empty,
        "1005_dummy_arp.bin": f1005,
    }
    manifest = {
        "version": 1,
        "note": "synthetic CI contract; 1005 is bit-accurate dummy ARP",
        "frames": {},
    }
    for name, blob in frames.items():
        (out_dir / name).write_bytes(blob)
        entry = {
            "bytes": len(blob),
            "crc_ok": True,
            "message_type": msg_type(blob) if len(blob) > 6 else None,
            "station_policy": "publish_dummy" if "1005" in name else None,
        }
        if "1005" in name:
            entry["payload_bits"] = MSG1005_PAYLOAD_BITS
            entry["station_id"] = PUBLISH_STATION_ID
            entry["ecef_01mm"] = [PUBLISH_ECEF_X, PUBLISH_ECEF_Y, PUBLISH_ECEF_Z]
        manifest["frames"][name] = entry

    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(frames)} frames -> {out_dir}")


if __name__ == "__main__":
    main()
