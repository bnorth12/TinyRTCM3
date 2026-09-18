#!/usr/bin/env python3
"""Verify local raw/ RTCM capture completeness vs CAPTURE.md expectations.

Scans CRC-valid frames under raw/*/ and prints type histograms.
Exit 0 always for informational runs; exit 1 if --require-msm4 and no MSM4.
Does not read or print ECEF payloads.
"""
from __future__ import annotations
import argparse
from collections import Counter
from pathlib import Path

POLY = 0x1864CFB

def crc24q(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 16
        for _ in range(8):
            crc <<= 1
            if crc & 0x1000000:
                crc ^= POLY
    return crc & 0xFFFFFF

def frames(blob: bytes):
    i = 0
    while i + 3 < len(blob):
        if blob[i] != 0xD3:
            i += 1
            continue
        plen = ((blob[i + 1] & 3) << 8) | blob[i + 2]
        flen = plen + 6
        if i + flen > len(blob):
            break
        fr = blob[i : i + flen]
        c = (fr[-3] << 16) | (fr[-2] << 8) | fr[-1]
        if crc24q(fr[:-3]) == c and plen >= 2:
            mt = (fr[3] << 4) | (fr[4] >> 4)
            yield mt
        i += 1

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--raw", type=Path, default=Path("raw"))
    ap.add_argument("--require-msm4", action="store_true")
    args = ap.parse_args()
    total = Counter()
    if not args.raw.exists():
        print("no raw/ directory")
        return 0
    for bin_path in sorted(args.raw.glob("*/*.bin")):
        c = Counter(frames(bin_path.read_bytes()))
        print(f"{bin_path.parent.name}/{bin_path.name}: {dict(sorted(c.items()))}")
        total.update(c)
    print("TOTAL", dict(sorted(total.items())))
    msm4 = any((t % 10) == 4 and 1071 <= t <= 1127 for t in total)
    if args.require_msm4 and not msm4:
        print("FAIL: no MSM4 types found")
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())