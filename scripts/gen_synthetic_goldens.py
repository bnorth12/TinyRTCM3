#!/usr/bin/env python3
"""Generate synthetic RTCM3 golden frames for CI (no real site coordinates).

v0.1 emits:
  - empty payload frame (CRC smoke)
  - minimal 1005-like bit layout using published dummy ECEF (scaffold; full
    bit-accurate 1005 lands with Codec milestone)

Output: test/golden/synthetic/
"""
from __future__ import annotations

import json
import struct
from pathlib import Path

POLY = 0x1864CFB
PUBLISH_STATION_ID = 0
# 0.0001 m units — matches TinyRtcmTypes.h
PUBLISH_ECEF_X = 63781370000
PUBLISH_ECEF_Y = 0
PUBLISH_ECEF_Z = 0


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

    def to_bytes(self) -> bytes:
        out = bytearray((len(self._bits) + 7) // 8)
        for i, bit in enumerate(self._bits):
            if bit:
                out[i // 8] |= 1 << (7 - (i % 8))
        return bytes(out)


def make_1005_scaffold() -> bytes:
    """Minimal 1005 payload shape for golden CRC/type extraction.

    Not yet bit-identical to RTCM 1005; Codec milestone will replace this
    with a verified encoder and update goldens in lockstep.
    """
    w = BitWriter()
    w.put(1005, 12)  # DF002 message number
    w.put(PUBLISH_STATION_ID, 12)  # DF003
    w.put(0, 6)  # reserved / ITRF year stub
    w.put(1, 1)  # GPS indicator stub
    w.put(0, 1)
    w.put(0, 1)
    w.put(0, 1)
    w.put(0, 1)
    # ECEF as 38-bit signed-ish stubs (scaffold — not full DF025 layout)
    for v in (PUBLISH_ECEF_X, PUBLISH_ECEF_Y, PUBLISH_ECEF_Z):
        w.put(v & ((1 << 38) - 1), 38)
    w.put(0, 1)  # oscillator
    w.put(0, 1)  # reserved
    payload = w.to_bytes()
    # pad to even-ish length for transport
    if len(payload) < 19:
        payload = payload + bytes(19 - len(payload))
    return wrap_frame(payload)


def msg_type(frame: bytes) -> int:
    return (frame[3] << 4) | (frame[4] >> 4)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    out_dir = root / "test" / "golden" / "synthetic"
    out_dir.mkdir(parents=True, exist_ok=True)

    empty = wrap_frame(b"")
    f1005 = make_1005_scaffold()

    frames = {
        "empty.bin": empty,
        "1005_dummy_arp.bin": f1005,
    }
    manifest = {"version": 1, "note": "synthetic CI contract; 1005 scaffold until Codec lands", "frames": {}}
    for name, blob in frames.items():
        (out_dir / name).write_bytes(blob)
        manifest["frames"][name] = {
            "bytes": len(blob),
            "crc_ok": True,
            "message_type": msg_type(blob) if len(blob) >= 5 and len(blob) > 6 else None,
            "station_policy": "publish_dummy" if "1005" in name else None,
        }

    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(frames)} frames -> {out_dir}")


if __name__ == "__main__":
    main()
