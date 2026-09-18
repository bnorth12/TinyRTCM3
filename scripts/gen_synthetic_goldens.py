#!/usr/bin/env python3
"""Generate synthetic RTCM3 golden frames for CI (no real site coordinates).

Emits:
  - empty payload frame (CRC smoke)
  - bit-accurate RTCM 1005 using published dummy ECEF (152-bit payload)
  - RTCM 1006 = 1005 body + 16-bit antenna height (168-bit / 21-byte payload)
  - RTCM 1033 with sanitized short descriptors (counted strings)

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
# Synthetic antenna height above marker (0.0001 m) — 1.5000 m
PUBLISH_ANT_HEIGHT = 15000
# Sanitized public descriptors (never real farm/shop strings)
PUBLISH_ANT_DESC = "ANT"
PUBLISH_RX_DESC = "RCV"

MSG1005_PAYLOAD_BITS = 152
MSG1006_PAYLOAD_BITS = 168


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

    def put_counted_string(self, s: str) -> None:
        n = min(len(s), 31)
        self.put(n, 8)
        for ch in s[:n]:
            self.put(ord(ch), 8)

    def to_bytes(self) -> bytes:
        out = bytearray((len(self._bits) + 7) // 8)
        for i, bit in enumerate(self._bits):
            if bit:
                out[i // 8] |= 1 << (7 - (i % 8))
        return bytes(out)


def put_1005_style_body(w: BitWriter, msg_type: int) -> None:
    w.put(msg_type, 12)
    w.put(PUBLISH_STATION_ID, 12)
    w.put(0, 6)  # ITRF
    w.put(1, 1)  # GPS
    w.put(0, 1)
    w.put(0, 1)
    w.put(0, 1)
    w.put_signed(PUBLISH_ECEF_X, 38)
    w.put(0, 1)  # osc
    w.put(0, 1)  # res
    w.put_signed(PUBLISH_ECEF_Y, 38)
    w.put(0, 2)  # QCI
    w.put_signed(PUBLISH_ECEF_Z, 38)


def make_1005() -> bytes:
    w = BitWriter()
    put_1005_style_body(w, 1005)
    if len(w._bits) != MSG1005_PAYLOAD_BITS:
        raise RuntimeError(f"1005 payload is {len(w._bits)} bits, expected {MSG1005_PAYLOAD_BITS}")
    payload = w.to_bytes()
    if len(payload) != 19:
        raise RuntimeError(f"1005 payload is {len(payload)} bytes, expected 19")
    return wrap_frame(payload)


def make_1006() -> bytes:
    w = BitWriter()
    put_1005_style_body(w, 1006)
    w.put(PUBLISH_ANT_HEIGHT, 16)
    if len(w._bits) != MSG1006_PAYLOAD_BITS:
        raise RuntimeError(f"1006 payload is {len(w._bits)} bits, expected {MSG1006_PAYLOAD_BITS}")
    payload = w.to_bytes()
    if len(payload) != 21:
        raise RuntimeError(f"1006 payload is {len(payload)} bytes, expected 21")
    return wrap_frame(payload)


def make_1033() -> bytes:
    w = BitWriter()
    w.put(1033, 12)
    w.put(PUBLISH_STATION_ID, 12)
    w.put_counted_string(PUBLISH_ANT_DESC)  # antenna descriptor
    w.put(0, 8)  # antenna setup id
    w.put_counted_string("")  # antenna serial
    w.put_counted_string(PUBLISH_RX_DESC)  # receiver type
    w.put_counted_string("")  # firmware
    w.put_counted_string("")  # receiver serial
    payload = w.to_bytes()
    return wrap_frame(payload)


def msg_type(frame: bytes) -> int:
    return (frame[3] << 4) | (frame[4] >> 4)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    out_dir = root / "test" / "golden" / "synthetic"
    out_dir.mkdir(parents=True, exist_ok=True)

    empty = wrap_frame(b"")
    f1005 = make_1005()
    f1006 = make_1006()
    f1033 = make_1033()

    frames = {
        "empty.bin": empty,
        "1005_dummy_arp.bin": f1005,
        "1006_dummy_arp.bin": f1006,
        "1033_sanitized.bin": f1033,
    }
    manifest = {
        "version": 2,
        "note": "synthetic CI contract; 1005/1006 dummy ARP; 1033 sanitized descriptors",
        "frames": {},
    }
    for name, blob in frames.items():
        (out_dir / name).write_bytes(blob)
        entry = {
            "bytes": len(blob),
            "crc_ok": True,
            "message_type": msg_type(blob) if len(blob) > 6 else None,
            "station_policy": "publish_dummy" if name != "empty.bin" else None,
        }
        if "1005" in name or "1006" in name:
            entry["station_id"] = PUBLISH_STATION_ID
            entry["ecef_01mm"] = [PUBLISH_ECEF_X, PUBLISH_ECEF_Y, PUBLISH_ECEF_Z]
        if "1005" in name:
            entry["payload_bits"] = MSG1005_PAYLOAD_BITS
        if "1006" in name:
            entry["payload_bits"] = MSG1006_PAYLOAD_BITS
            entry["antenna_height_01mm"] = PUBLISH_ANT_HEIGHT
        if "1033" in name:
            entry["antenna_descriptor"] = PUBLISH_ANT_DESC
            entry["receiver_descriptor"] = PUBLISH_RX_DESC
        manifest["frames"][name] = entry

    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(frames)} frames -> {out_dir}")


if __name__ == "__main__":
    main()
