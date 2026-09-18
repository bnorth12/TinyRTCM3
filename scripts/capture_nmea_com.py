#!/usr/bin/env python3
"""Discrete NMEA/PQTM capture for LC29H (EA focus). One message type per file."""
from __future__ import annotations

import argparse
import json
import re
import time
from datetime import datetime, timezone
from pathlib import Path

import serial

# Protocol Table 6 — CFGMSGRATE supported names
NMEA_STANDARD = ["RMC", "GGA", "GSV", "GSA", "VTG", "GLL", "ZDA", "GRS", "GST", "GNS"]
PQTM_OUTPUT = [
    "PQTMEPE",
    "PQTMGEOFENCESTATUS",
    "PQTMSVINSTATUS",
    "PQTMPVT",
    "PQTMPL",
    "PQTMDOP",
    "PQTMVEL",
    "PQTMODO",
    "PQTMJAMMINGSTATUS",
    "PQTMLS",
    "PQTMSTD",
]


def nmea_checksum(payload: str) -> str:
    c = 0
    for ch in payload:
        c ^= ord(ch)
    return f"{c:02X}"


def make_sentence(payload: str) -> str:
    return f"${payload}*{nmea_checksum(payload)}\r\n"


def sentence_ok(line: str) -> bool:
    if not line.startswith("$") or "*" not in line:
        return False
    body, _, hx = line[1:].partition("*")
    if len(hx) < 2:
        return False
    try:
        got = int(hx[:2], 16)
    except ValueError:
        return False
    c = 0
    for ch in body:
        c ^= ord(ch)
    return c == got


def talker_key(line: str) -> str | None:
    """Return RMC/GGA/... or PQTMxxx from a sentence."""
    if not line.startswith("$") or len(line) < 6:
        return None
    body = line[1:].split("*", 1)[0]
    if body.startswith("PQTM"):
        return body.split(",", 1)[0]
    # $GNGGA / $GPRMC -> GGA / RMC
    if len(body) >= 5:
        return body[2:5]
    return None


class Module:
    def __init__(self, port: str, baud: int):
        self.ser = serial.Serial(port=port, baudrate=baud, timeout=0.2)

    def close(self) -> None:
        self.ser.close()

    def send(self, payload: str) -> None:
        self.ser.write(make_sentence(payload).encode("ascii"))
        self.ser.flush()
        time.sleep(0.08)

    def drain(self, seconds: float) -> bytes:
        end = time.time() + seconds
        buf = bytearray()
        while time.time() < end:
            chunk = self.ser.read(8192)
            if chunk:
                buf.extend(chunk)
            else:
                time.sleep(0.005)
        return bytes(buf)

    def cfg_rate(self, name: str, rate: int) -> str:
        if name.startswith("PQTM"):
            ver = 2 if name == "PQTMEPE" else 1
            payload = f"PQTMCFGMSGRATE,W,{name},{rate},{ver}"
        else:
            payload = f"PQTMCFGMSGRATE,W,{name},{rate}"
        self.send(payload)
        return payload

    def query_identity(self) -> dict:
        self.drain(0.2)
        self.ser.reset_input_buffer()
        for p in ("PQTMVERNO", "PQTMSN", "PQTMUNIQID"):
            self.send(p)
            time.sleep(0.2)
        text = self.drain(1.5).decode("ascii", errors="replace")
        lines = [ln.strip() for ln in text.splitlines() if ln.startswith("$PQTM")]
        verno = next((ln for ln in lines if ln.startswith("$PQTMVERNO")), None)
        sn = next((ln for ln in lines if ln.startswith("$PQTMSN")), None)
        uid = next((ln for ln in lines if ln.startswith("$PQTMUNIQID")), None)
        module_type = firmware = variant = None
        if verno:
            parts = verno.split("*", 1)[0].split(",")
            if len(parts) >= 2:
                module_type = parts[1].strip()
                m = re.search(r"LC29H(EA|DA|BA|BS|CA)", module_type, re.I)
                if m:
                    variant = m.group(1).upper()
            if len(parts) >= 3:
                firmware = parts[2].strip()
        def clean(line: str | None) -> str | None:
            if not line:
                return None
            body = line.split("*", 1)[0]
            parts = body.split(",")
            if len(parts) >= 2 and parts[1].upper() == "ERROR":
                return None
            return ",".join(parts[1:]).strip() if len(parts) >= 2 else None
        return {
            "module_type": module_type,
            "variant": variant,
            "firmware": firmware,
            "serial_number": clean(sn),
            "unique_id": clean(uid),
            "raw_replies": {"PQTMVERNO": verno, "PQTMSN": sn, "PQTMUNIQID": uid},
        }


def count_sentences(text: str, want: str) -> dict:
    total = 0
    ok = 0
    matched = 0
    matched_ok = 0
    by: dict[str, int] = {}
    for ln in text.splitlines():
        ln = ln.strip()
        if not ln.startswith("$"):
            continue
        total += 1
        good = sentence_ok(ln)
        if good:
            ok += 1
        key = talker_key(ln)
        if key:
            by[key] = by.get(key, 0) + 1
            if key == want:
                matched += 1
                if good:
                    matched_ok += 1
    return {
        "lines_total": total,
        "checksum_ok": ok,
        "target_lines": matched,
        "target_checksum_ok": matched_ok,
        "by_sentence": by,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", default="COM8")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--duration", type=float, default=12.0, help="Seconds per message")
    ap.add_argument("--settle", type=float, default=1.5)
    ap.add_argument("--out-root", type=Path, default=Path("raw/nmea"))
    ap.add_argument("--set", choices=["nmea", "pqtm", "all"], default="all")
    ap.add_argument("--only", nargs="*", help="Optional subset of message names")
    ap.add_argument("--no-factory-reset", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    names: list[str] = []
    if args.set in ("nmea", "all"):
        names += NMEA_STANDARD
    if args.set in ("pqtm", "all"):
        names += PQTM_OUTPUT
    if args.only:
        names = [n for n in names if n in set(args.only)]

    if args.dry_run:
        print("Would capture:", ", ".join(names))
        return 0

    mod = Module(args.port, args.baud)
    try:
        identity = mod.query_identity()
        print("Identity:", json.dumps({k: identity[k] for k in (
            "module_type", "variant", "firmware", "serial_number")}, indent=2))

        print("Muting all documented NMEA/PQTM outputs...")
        mute_cmds = []
        for n in NMEA_STANDARD + PQTM_OUTPUT:
            mute_cmds.append(mod.cfg_rate(n, 0))
        time.sleep(0.5)
        mod.drain(0.5)

        results = []
        for name in names:
            print(f"=== {name} ===")
            cmds = list(mute_cmds)
            # ensure muted then enable target
            for n in NMEA_STANDARD + PQTM_OUTPUT:
                mod.cfg_rate(n, 0)
            cmds.append(mod.cfg_rate(name, 1))
            time.sleep(args.settle)
            mod.drain(0.2)
            blob = mod.drain(args.duration)
            text = blob.decode("ascii", errors="replace")
            stats = count_sentences(text, name)
            ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
            out_dir = args.out_root / name
            out_dir.mkdir(parents=True, exist_ok=True)
            nmea_path = out_dir / f"{ts}.nmea"
            json_path = out_dir / "run.json"
            nmea_path.write_bytes(blob)
            meta = {
                "run_id": f"nmea-{name}",
                "capture_kind": "nmea_discrete",
                "message": name,
                "port": args.port,
                "baud": args.baud,
                "duration_s": args.duration,
                "module_type": identity.get("module_type"),
                "variant": identity.get("variant"),
                "firmware": identity.get("firmware"),
                "serial_number": identity.get("serial_number"),
                "unique_id": identity.get("unique_id"),
                "captured_at_utc": ts,
                "captured_at_iso": f"{ts[0:4]}-{ts[4:6]}-{ts[6:8]}T{ts[9:11]}:{ts[11:13]}:{ts[13:15]}Z",
                "bytes": len(blob),
                "stats": stats,
                "commands_sent_tail": cmds[-5:],
                "unsanitized": True,
                "privacy": "May contain lat/lon; do not publish raw/",
                "pass": stats["target_checksum_ok"] > 0,
            }
            json_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
            status = "PASS" if meta["pass"] else "EMPTY/FAIL"
            print(f"  {status} bytes={len(blob)} target_ok={stats['target_checksum_ok']} by={stats['by_sentence']}")
            results.append({"message": name, "pass": meta["pass"], "target_ok": stats["target_checksum_ok"]})

        summary = {
            "queried_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "identity": identity,
            "results": results,
            "passed": sum(1 for r in results if r["pass"]),
            "failed": sum(1 for r in results if not r["pass"]),
        }
        args.out_root.mkdir(parents=True, exist_ok=True)
        (args.out_root / "session_summary.json").write_text(
            json.dumps(summary, indent=2) + "\n", encoding="utf-8"
        )
        print(f"Summary: passed={summary['passed']} failed={summary['failed']}")

        if not args.no_factory_reset:
            print("Factory reset PQTMRESTOREPAR + PAIR023...")
            mod.send("PQTMRESTOREPAR")
            time.sleep(0.5)
            mod.send("PAIR023")
            time.sleep(3.0)
            print("Factory reset done.")
    finally:
        mod.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())