#!/usr/bin/env python3
"""
LC29H base RTCM capture over COM — no QGNSS.

Mirrors configuration concepts from LC29H_GNSS (bnorth12):
  - Base mode:      PQTMCFGRCVRMODE,W,2
  - RTCM MSM:       PAIR432  (-1=off, 0=MSM4, 1=MSM7)   [enableRTCM]
  - RTCM 1005:      PAIR434  (0=off, 1=on)              [enableRTCM]
  - Persist:        PQTMSAVEPAR
  - Take effect:    PAIR023  (full reboot; not GNSS sleep)

Run matrix aligns with docs/CAPTURE.md. Writes raw/<run_id>/<timestamp>.bin
plus run.json (unsanitized=true). Never put real ECEF into run.json.

Limitations (v0.1):
  - iso-1006 / iso-1033: LC29H_GNSS has no first-class PAIR wrappers for
    exclusive 1006/1033; those runs configure base + disable MSM/1005 and
    log a warning — enable those messages manually or extend when docs land.
  - Assumes exclusive access to the COM port (close QGNSS first).
  - Does not change survey-in / fixed ECEF (use --configure-base only for
    receiver mode + RTCM enables, optional SAVE+PAIR023).

Requires: pyserial
  pip install pyserial
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pyserial required: pip install pyserial", file=sys.stderr)
    sys.exit(2)

# --- NMEA / Quectel sentence helpers (same contract as LC29H_GNSS::makeSentence) ---

def nmea_checksum(payload: str) -> str:
    c = 0
    for ch in payload:
        c ^= ord(ch)
    return f"{c:02X}"


def make_sentence(payload: str) -> str:
    """payload without leading '$' or '*hh'."""
    return f"${payload}*{nmea_checksum(payload)}\r\n"


# --- RTCM CRC-24Q (for live frame counting during capture) ---

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


def iter_rtcm_frames(blob: bytes):
    i, n = 0, len(blob)
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
        if crc24q(body) == got:
            mtype = (frame[3] << 4) | (frame[4] >> 4) if plen >= 2 else 0
            yield frame, mtype
            i += flen
        else:
            i += 1


def count_types(blob: bytes) -> dict[str, int]:
    counts: dict[str, int] = {}
    for _, mtype in iter_rtcm_frames(blob):
        key = str(mtype)
        counts[key] = counts.get(key, 0) + 1
    return counts


# --- Run profiles (CAPTURE.md) ---
# pair432: MSM mode; pair434: 1005 antenna point
# Library: enableRTCM(true) => PAIR432,1 + PAIR434,1
#          enableRTCM(false)=> PAIR432,-1 + PAIR434,0

PROFILES = {
    "iso-1005": {
        "pair432": "-1",
        "pair434": "1",
        "pair436": "0",
        "notes": "1005 only (MSM off). Required gate A.",
        "expect_hint": ["1005"],
        "required": True,
    },
    "msm4-bundle": {
        "pair432": "0",
        "pair434": "1",
        "pair436": "0",
        "notes": "MSM4 + 1005 — LC29H rover lite + DePIN MSM4/1005. Required gate A/B.",
        "expect_hint": ["1005", "1074", "1084", "1094", "1124"],
        "required": True,
    },
    "msm7-bundle": {
        "pair432": "1",
        "pair434": "1",
        "pair436": "0",
        "notes": "MSM7 + 1005 — LC29H_GNSS enableRTCM(true). Required gate A.",
        "expect_hint": ["1005", "1077", "1087", "1097", "1127"],
        "required": True,
    },
    "eph-bundle": {
        "pair432": "-1",
        "pair434": "0",
        "pair436": "1",
        "notes": "Optional: PAIR436 ephemeris RTCM (1019/…). Not required for rover/DePIN gate.",
        "expect_hint": ["1019", "1020", "1042", "1046"],
        "required": False,
    },
    "iso-1006": {
        "pair432": "-1",
        "pair434": "0",
        "pair436": "0",
        "notes": "IMPOSSIBLE on LC29H TX (1006 is input-only per protocol). Use synthetic encode1006.",
        "expect_hint": ["1006"],
        "required": False,
        "impossible_tx": True,
    },
    "iso-1033": {
        "pair432": "-1",
        "pair434": "0",
        "pair436": "0",
        "notes": "IMPOSSIBLE on LC29H TX (1033 not in output table). Use synthetic encode1033 for DePIN.",
        "expect_hint": ["1033"],
        "required": False,
        "impossible_tx": True,
    },
}


class Lc29hCom:
    def __init__(self, port: str, baud: int, timeout: float = 0.2):
        self.ser = serial.Serial(port=port, baudrate=baud, timeout=timeout)

    def close(self) -> None:
        self.ser.close()

    def send_payload(self, payload: str) -> None:
        line = make_sentence(payload)
        self.ser.write(line.encode("ascii"))
        self.ser.flush()

    def drain(self, seconds: float = 0.3) -> bytes:
        end = time.time() + seconds
        buf = bytearray()
        while time.time() < end:
            chunk = self.ser.read(4096)
            if chunk:
                buf.extend(chunk)
            else:
                time.sleep(0.01)
        return bytes(buf)

    def configure_rtcm_profile(self, profile: dict, set_base_mode: bool) -> list[str]:
        """Apply LC29H_GNSS-equivalent RTCM enables. Returns payloads sent."""
        sent: list[str] = []
        if set_base_mode:
            # LC29H_GNSS::setReceiverModeBase
            self.send_payload("PQTMCFGRCVRMODE,W,2")
            sent.append("PQTMCFGRCVRMODE,W,2")
            time.sleep(0.15)
        # Order matches enableRTCM: PAIR432 then PAIR434
        p432 = f"PAIR432,{profile['pair432']}"
        p434 = f"PAIR434,{profile['pair434']}"
        p436 = f"PAIR436,{profile.get('pair436', '0')}"
        self.send_payload(p432)
        sent.append(p432)
        time.sleep(0.1)
        self.send_payload(p434)
        sent.append(p434)
        time.sleep(0.1)
        self.send_payload(p436)
        sent.append(p436)
        time.sleep(0.1)
        return sent

    def save_and_reboot(self) -> None:
        # PQTMSAVEPAR then PAIR023 — required for DA/EA CFGRCVRMODE to stick
        self.send_payload("PQTMSAVEPAR")
        time.sleep(0.3)
        self.send_payload("PAIR023")
        # Module reboots; give UART time
        time.sleep(2.0)
        self.drain(0.5)

    def capture(self, duration_s: float) -> bytes:
        buf = bytearray()
        end = time.time() + duration_s
        while time.time() < end:
            chunk = self.ser.read(8192)
            if chunk:
                buf.extend(chunk)
            else:
                time.sleep(0.005)
        return bytes(buf)


def list_com_ports() -> None:
    ports = list(list_ports.comports())
    if not ports:
        print("No COM ports found")
        return
    for p in ports:
        print(f"{p.device}\t{p.description}\t{p.hwid}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--list-ports", action="store_true")
    ap.add_argument("--port", help="COMx")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument(
        "--run",
        choices=list(PROFILES.keys()) + ["all"],
        default="msm7-bundle",
        help="CAPTURE.md run id (default msm7-bundle = library enableRTCM true)",
    )
    ap.add_argument("--duration", type=float, default=30.0, help="Seconds to record after config")
    ap.add_argument(
        "--out-root",
        type=Path,
        default=Path("raw"),
        help="Local raw/ root (gitignored)",
    )
    ap.add_argument(
        "--configure-base",
        action="store_true",
        help="Send PQTMCFGRCVRMODE,W,2 before RTCM enables",
    )
    ap.add_argument(
        "--save-reboot",
        action="store_true",
        help="PQTMSAVEPAR + PAIR023 after config (DA/EA persistence)",
    )
    ap.add_argument(
        "--settle",
        type=float,
        default=2.0,
        help="Seconds to wait after config before recording",
    )
    ap.add_argument("--firmware", default="", help="Optional firmware string for run.json")
    ap.add_argument("--no-factory-reset", action="store_true",
                    help="Skip PQTMRESTOREPAR+PAIR023 after captures (default: factory-reset when done)")
    ap.add_argument("--dry-run", action="store_true", help="Print payloads only; no serial")
    args = ap.parse_args()

    if args.list_ports:
        list_com_ports()
        return 0

    runs = list(PROFILES.keys()) if args.run == "all" else [args.run]

    if args.dry_run:
        for rid in runs:
            prof = PROFILES[rid]
            print(f"=== {rid} ===")
            if args.configure_base:
                print("  ", make_sentence("PQTMCFGRCVRMODE,W,2").strip())
            print("  ", make_sentence(f"PAIR432,{prof['pair432']}").strip())
            print("  ", make_sentence(f"PAIR434,{prof['pair434']}").strip())
            if args.save_reboot:
                print("  ", make_sentence("PQTMSAVEPAR").strip())
                print("  ", make_sentence("PAIR023").strip())
            if prof.get("limited"):
                print("  WARNING:", prof["notes"])
        return 0

    if not args.port:
        print("--port required (or --list-ports / --dry-run)", file=sys.stderr)
        return 2

    device = Lc29hCom(args.port, args.baud)
    try:
        for rid in runs:
            prof = PROFILES[rid]
            if prof.get("limited"):
                print(f"WARNING {rid}: {prof['notes']}", file=sys.stderr)

            ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
            out_dir = args.out_root / rid
            out_dir.mkdir(parents=True, exist_ok=True)
            bin_path = out_dir / f"{ts}.bin"
            json_path = out_dir / "run.json"

            print(f"[{rid}] configuring on {args.port} @ {args.baud}")
            device.drain(0.2)
            sent = device.configure_rtcm_profile(prof, set_base_mode=args.configure_base)
            if args.save_reboot:
                device.save_and_reboot()
                # Re-apply RTCM after reboot if save was used (PAIR023 clears live enables
                # unless they were saved — SAVEPAR should keep them; re-apply for safety)
                sent += device.configure_rtcm_profile(prof, set_base_mode=False)

            print(f"[{rid}] settle {args.settle}s then capture {args.duration}s -> {bin_path}")
            time.sleep(args.settle)
            device.drain(0.1)
            blob = device.capture(args.duration)
            bin_path.write_bytes(blob)
            counts = count_types(blob)
            meta = {
                "run_id": rid,
                "device": "LC29H",
                "firmware": args.firmware or None,
                "tool": "scripts/capture_base_com.py",
                "qgnss": None,
                "port": args.port,
                "baud": args.baud,
                "duration_s": args.duration,
                "notes": prof["notes"],
                "commands_sent": sent,
                "unsanitized": True,
                "bytes": len(blob),
                "rtcm_type_counts": counts,
                "expect_hint": prof["expect_hint"],
                "captured_at_utc": ts,
                "privacy": "Do not commit raw/; sanitize before public field/",
            }
            json_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
            print(f"[{rid}] wrote {len(blob)} bytes; types={counts}")
            if not counts:
                print(f"[{rid}] WARNING: no CRC-valid RTCM frames — check base mode / antenna / baud",
                      file=sys.stderr)
    finally:
        device.close()


    if (not args.dry_run) and (not args.no_factory_reset) and args.port:
        print("Factory-resetting module (PQTMRESTOREPAR + PAIR023)...")
        import serial as _serial
        import time as _time

        def _ck(payload: str) -> str:
            c = 0
            for ch in payload:
                c ^= ord(ch)
            return f"{c:02X}"

        def _sent(payload: str) -> str:
            return f"${payload}*{_ck(payload)}\r\n"

        _ser = _serial.Serial(port=args.port, baudrate=args.baud, timeout=0.3)
        try:
            _ser.reset_input_buffer()
            for _payload in ("PQTMRESTOREPAR", "PAIR023"):
                _line = _sent(_payload)
                print("TX", repr(_line))
                _ser.write(_line.encode("ascii"))
                _ser.flush()
                _time.sleep(0.6 if _payload != "PAIR023" else 0.2)
            _time.sleep(3.0)
            print("Factory restore complete.")
        finally:
            _ser.close()

    print("Done. raw/ is gitignored. Sanitize before any public commit (docs/PRIVACY.md).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
