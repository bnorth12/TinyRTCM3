# RTCM binary evaluation (offline) — 2026-09-18

Source: `raw/*/` captures on LC29H(EA). CRC-24Q scanned; report JSON: `raw/_rtcm_eval_report.json`.

## Matrix

| Run | Bytes | CRC-OK frames | Types | Library use |
|-----|------:|--------------:|-------|-------------|
| iso-1005 | 13872 | 30 | 1005×30 | Station ARP golden — CRC/assembler + decode1005 |
| msm4-bundle | 27938 | 186 | 1005 + 1074/1084/1094/1114/1124 (×31) | **Rover-lite diet PASS** (MSM4+1005) |
| msm7-bundle | 25100 | 180 | 1005 + 1077/1087/1097/1117/1127 (×30) | MSM7 soak / enableRTCM(true) twin |
| eph-bundle | 81920 | **0** | none | Retry needed (bytes, no framed RTCM) |
| iso-1006 | 13318 | 0 | none | Expected — LC29H cannot TX 1006 |

No bad-CRC frames in any OK run (crc_ok path only advanced on good frames).

## Gaps vs DePIN / NTRIP caster completeness

- **1006** / **1033**: not in any TX capture — stay **synthetic** (encode in TinyRTCM3).
- **1104/1107** (NavIC MSM): absent — constellation not in this EA stream.
- **Ephemeris** (1019/1020/1042/1044/1046…): unverified on UART TX; eph-bundle is noise/non-framed today.

## Library implications

1. **Framing/CRC**: field goldens for 1005 + MSM4/MSM7 are solid — use for assembler + CRC-24Q host tests.
2. **decode1005 / MSM header+CNR**: prefer `msm4-bundle` (richest multi-GNSS MSM4 set).
3. **1006/1033 codecs**: no module TX goldens — keep synthetic-first; do not block CI on LC29H capture.
4. **eph**: do not mark unsupported until PAIR436 + SAVE + PAIR023 retry with CRC type counts.

## NMEA gated (same session, finished)

PASS: PQTMGEOFENCESTATUS (364), PQTMSVINSTATUS (2).  
EMPTY: ZDA, GNS, GRS, GST, PQTMJAMMINGSTATUS, PQTMLS, PQTMSTD.  
Factory reset completed after gated series.
