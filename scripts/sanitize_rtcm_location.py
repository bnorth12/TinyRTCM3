#!/usr/bin/env python3
"""Rewrite 1005/1006 ARP (+ station id) to published dummy; neutralize 1033.

v0.1 behavior:
  - Parses RTCM3 frames from a byte stream (CRC-24Q validated).
  - Drops or replaces 1005/1006/1033 with warnings until full bit rewrite lands.
  - Passthrough for MSM and unknown types (MSM has no ARP; still review logs).

Until Codec encode1005/encode1033 are implemented, this script REFUSES to emit
public field goldens that still contain original 1005/1006/1033 payloads.
It will:
  - copy non-location messages through
  - omit 1005/1006/1033 and record them in report.json
  - exit non-zero if any 1005/1006/1033 were present (so you know to re-run
    after Codec, or use synthetic-only until then)

This keeps anonymity: we never publish original ARP by accident.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

POLY = 0x1864CFB
LOCATION_TYPES = {1005, 1006, 1033}


def crc24q(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 16
        for _ in range(8):
            crc <<= 1
            if crc & 0x1000000:
                crc ^= POLY
    return crc & 0xFFFFFF


def iter_frames(blob: bytes):
    i = 0
    n = len(blob)
    while i < n:
        if blob[i] != 0xD3:
            i += 1
            continue
        if i + 3 > n:
            break
        plen = ((blob[i + 1] & 0x03) << 8) | blob[i + 2]
        flen = plen + 6
        if i + flen > n:
            break
        frame = blob[i : i + flen]
        body, crc_b = frame[:-3], frame[-3:]
        got = (crc_b[0] << 16) | (crc_b[1] << 8) | crc_b[2]
        if crc24q(body) == got and plen >= 0:
            mtype = (frame[3] << 4) | (frame[4] >> 4) if plen >= 2 else 0
            yield frame, mtype
            i += flen
        else:
            i += 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    ap.add_argument("output_dir", type=Path)
    ap.add_argument("--allow-drop-location", action="store_true",
                    help="Write MSM-only stream even if 1005/1006/1033 were dropped")
    args = ap.parse_args()

    blob = args.input.read_bytes()
    kept = bytearray()
    counts: dict[str, int] = {}
    dropped_loc = 0
    for frame, mtype in iter_frames(blob):
        key = str(mtype)
        counts[key] = counts.get(key, 0) + 1
        if mtype in LOCATION_TYPES:
            dropped_loc += 1
            continue
        kept.extend(frame)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    out_bin = args.output_dir / "frames.bin"
    out_bin.write_bytes(bytes(kept))
    report = {
        "input": str(args.input),
        "policy": "drop_1005_1006_1033_until_rewrite_encoder",
        "message_counts_in": counts,
        "location_frames_dropped": dropped_loc,
        "output_bytes": len(kept),
        "anonymous": True,
        "note": "Full ARP rewrite lands with encode1005; until then location msgs are omitted.",
    }
    (args.output_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    if dropped_loc and not args.allow_drop_location:
        print("Refusing clean exit: location frames were present and dropped. "
              "Re-run with --allow-drop-location for MSM-only public soak, "
              "or wait for encode1005 rewrite.", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
