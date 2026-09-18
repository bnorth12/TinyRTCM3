# NMEA / PQTM capture plan (discrete, LC29H family)

**Separate from RTCM goldens.** One talker (or one PQTM message) per file.
**Current hardware focus:** LC29H(**EA**) on the live COM port. DA/BS later.

## Documented output messages (protocol Table 6 / CFGMSGRATE)

### Standard NMEA (MsgVer omitted)
RMC, GGA, GSV, GSA, VTG, GLL, ZDA, GRS, GST, GNS

### PQTM (MsgVer required; EPE uses 2, others 1)
PQTMEPE, PQTMGEOFENCESTATUS, PQTMSVINSTATUS, PQTMPVT, PQTMPL, PQTMDOP,
PQTMVEL, PQTMODO, PQTMJAMMINGSTATUS, PQTMLS, PQTMSTD

## Mode prerequisites (do not treat EMPTY as “unsupported” without this)

Some messages only emit when the receiver is in a matching mode / feature state.
Mute→enable alone is not enough.

| Message | Prerequisite (typical) | Notes |
|---------|------------------------|--------|
| `PQTMSVINSTATUS` | Base + **survey-in active** (`PQTMCFGSVIN` / SVIN running) | Expected empty in plain rover/default |
| `PQTMGEOFENCESTATUS` | Geofence configured/enabled | May still ACK CFGMSGRATE; body needs fence |
| `PQTMJAMMINGSTATUS` | Jamming/interference detect enabled (PAIR/AIC path) | Feature-dependent |
| `PQTMLS` / `PQTMSTD` | Firmware / feature set that outputs those PQTM types | Verify against EA FW notes |
| `ZDA` / `GRS` / `GST` / `GNS` | Often off by default; some need nav solution quality or specific CFG | Re-try after known-good fix + explicit rate 1; confirm EA support for each |

**2026-09-18 EA session:** PASS on RMC/GGA/GSV/GSA/VTG/GLL + several PQTM; EMPTY on ZDA/GRS/GST/GNS + PQTMSVINSTATUS/JAMMING/LS/STD — re-run EMPTY set under the prerequisites above before marking “unsupported on EA.”

## Process
1. Query identity (`PQTMVERNO`) into session metadata.
2. Mute all listed messages (`PQTMCFGMSGRATE,W,<name>,0` [`,ver` for PQTM]).
3. For each message: enter **required mode** (if any) → enable only that name @ rate 1 →
   capture N seconds → `raw/nmea/<name>/<timestamp>.nmea` + `run.json`.
4. Record `prerequisite` + `mode` in `run.json`.
5. Factory-reset: `PQTMRESTOREPAR` + `PAIR023` when the exercise ends.

## Privacy
GGA/RMC/GLL/GNS (and some PQTM) contain lat/lon — `unsanitized: true`, do not publish.

## Tool
`scripts/capture_nmea_com.py` (extend with `--prerequisite-mode svin|…` for gated messages).
