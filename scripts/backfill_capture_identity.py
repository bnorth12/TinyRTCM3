#!/usr/bin/env python3
"""Query LC29H identity and backfill raw/*/run.json metadata."""
from __future__ import annotations

import argparse
import json
import re
import time
from datetime import datetime, timezone
from pathlib import Path

import serial


def nmea_checksum(payload: str) -> str:
    c = 0
    for ch in payload:
        c ^= ord(ch)
    return f"{c:02X}"


def make_sentence(payload: str) -> str:
    return f"${payload}*{nmea_checksum(payload)}\r\n"


def read_lines(ser: serial.Serial, seconds: float) -> list[str]:
    end = time.time() + seconds
    buf = bytearray()
    while time.time() < end:
        chunk = ser.read(4096)
        if chunk:
            buf.extend(chunk)
        else:
            time.sleep(0.01)
    text = buf.decode("ascii", errors="replace")
    return [ln.strip() for ln in text.splitlines() if ln.strip()]


def first_match(lines: list[str], prefix: str) -> str | None:
    for ln in lines:
        if ln.startswith(prefix):
            return ln
    return None


def parse_verno(line: str | None) -> dict:
    """$PQTMVERNO,<model>,<fw>,... or similar — keep raw + best-effort fields."""
    out = {"raw": line, "module_type": None, "firmware": None, "variant": None}
    if not line:
        return out
    body = line.split("*", 1)[0]
    parts = body.split(",")
    # Typical: $PQTMVERNO,LC29HEA,.... or with more fields
    if len(parts) >= 2:
        model = parts[1].strip()
        out["module_type"] = model
        m = re.search(r"LC29H([A-Z]{2})", model, re.I)
        if m:
            out["variant"] = m.group(1).upper()
        elif "LC29H" in model.upper():
            # e.g. LC29HEA packed
            m2 = re.search(r"LC29H(EA|DA|BA|BS|CA)", model, re.I)
            if m2:
                out["variant"] = m2.group(1).upper()
    if len(parts) >= 3:
        out["firmware"] = parts[2].strip() or None
    return out


def parse_simple(line: str | None, key: str) -> dict:
    out = {"raw": line, key: None}
    if not line:
        return out
    body = line.split("*", 1)[0]
    parts = body.split(",")
    if len(parts) >= 2:
        out[key] = ",".join(parts[1:]).strip() or None
    return out


def query_identity(port: str, baud: int) -> dict:
    ser = serial.Serial(port=port, baudrate=baud, timeout=0.2)
    try:
        time.sleep(0.2)
        ser.reset_input_buffer()
        # Quiet period then queries
        for payload in ("PQTMVERNO", "PQTMUNIQID", "PQTMSN"):
            ser.write(make_sentence(payload).encode("ascii"))
            ser.flush()
            time.sleep(0.25)
        lines = read_lines(ser, 2.0)
        # Also catch delayed replies
        lines += read_lines(ser, 1.0)

        verno = parse_verno(first_match(lines, "$PQTMVERNO"))
        uniq = parse_simple(first_match(lines, "$PQTMUNIQID"), "unique_id")
        sn = parse_simple(first_match(lines, "$PQTMSN"), "serial_number")

        # Fallback: scan any line containing LC29H
        if not verno["module_type"]:
            for ln in lines:
                if "LC29H" in ln.upper() and ln.startswith("$"):
                    verno = parse_verno(ln)
                    break

        return {
            "queried_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "port": port,
            "baud": baud,
            "module_type": verno.get("module_type"),
            "variant": verno.get("variant"),
            "firmware": verno.get("firmware"),
            "serial_number": sn.get("serial_number"),
            "unique_id": uniq.get("unique_id"),
            "raw_replies": {
                "PQTMVERNO": verno.get("raw"),
                "PQTMSN": sn.get("raw"),
                "PQTMUNIQID": uniq.get("raw"),
            },
            "all_identity_lines": [ln for ln in lines if ln.startswith("$PQTM")],
        }
    finally:
        ser.close()


def backfill(raw_root: Path, identity: dict, note: str) -> None:
    for path in sorted(raw_root.glob("*/run.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        # Preserve original capture timestamp fields; add identity block
        data["module_type"] = identity.get("module_type")
        data["variant"] = identity.get("variant")
        data["firmware"] = identity.get("firmware") or data.get("firmware")
        data["serial_number"] = identity.get("serial_number")
        data["unique_id"] = identity.get("unique_id")
        # Normalize / keep capture date
        if "captured_at_utc" not in data and "captured_at" in data:
            data["captured_at_utc"] = data["captured_at"]
        data["identity_backfill"] = {
            "queried_at_utc": identity.get("queried_at_utc"),
            "note": note,
            "raw_replies": identity.get("raw_replies"),
        }
        path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        print(f"updated {path}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM8")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--raw-root", type=Path, default=Path("raw"))
    ap.add_argument(
        "--note",
        default="Identity queried after capture session; same physical module assumed for all runs that day.",
    )
    args = ap.parse_args()

    identity = query_identity(args.port, args.baud)
    print(json.dumps({k: identity[k] for k in (
        "module_type", "variant", "firmware", "serial_number", "unique_id",
        "queried_at_utc", "raw_replies", "all_identity_lines"
    )}, indent=2))

    if not identity.get("module_type") and not identity.get("serial_number"):
        print("WARNING: no identity replies parsed; not writing run.json")
        return 1

    backfill(args.raw_root, identity, args.note)
    # Also write a session-level identity file
    sess = args.raw_root / "module_identity.json"
    sess.write_text(json.dumps(identity, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {sess}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
