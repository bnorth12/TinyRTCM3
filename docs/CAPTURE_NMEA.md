# NMEA / PQTM capture plan (discrete, LC29H family)

**Separate from RTCM goldens.** One talker (or one PQTM message) per file.
**Current hardware focus:** LC29H(**EA**) on the live COM port. DA/BS later.

## Documented output messages (protocol Table 6 / CFGMSGRATE)

### Standard NMEA (MsgVer omitted)
RMC, GGA, GSV, GSA, VTG, GLL, ZDA, GRS, GST, GNS

### PQTM (MsgVer required; EPE uses 2, others 1)
PQTMEPE, PQTMGEOFENCESTATUS, PQTMSVINSTATUS, PQTMPVT, PQTMPL, PQTMDOP,
PQTMVEL, PQTMODO, PQTMJAMMINGSTATUS, PQTMLS, PQTMSTD

## Rule: EMPTY ≠ unsupported

Several sentences only produce useful (or any) content when the module is in a
**specific mode / feature state**. Enabling `PQTMCFGMSGRATE` alone is not enough.

Until a message has been captured under its documented prerequisite mode, classify
EMPTY results as **`mode_or_feature_gated_candidate`**, not “unsupported on EA.”

### Prerequisite matrix (working list)

| Message | Mode / setup required for details | Capture notes |
|---------|-----------------------------------|---------------|
| **RMC, GGA, GSV, GSA, VTG, GLL** | Normal nav / fix (rover or after restore) | Baseline discrete PASS on EA 2026-09-18 |
| **ZDA** | Time/UTC available; sentence explicitly enabled | Often off by default — rate 1 after valid time |
| **GRS** | Residuals path enabled; typically needs fix + related config | Mode-gated; re-run under residual-capable setup |
| **GST** | Error/ellipse / quality output path enabled; needs fix | Mode-gated; not expected in bare mute→enable |
| **GNS** | Multi-GNSS NMEA mode / talker config as required by FW | Confirm CFGMSGRATE + any NMEA mode PAIR |
| **PQTMSVINSTATUS** | **Base + survey-in active** (`PQTMCFGSVIN` / SVIN running) | Expected empty in default rover |
| **PQTMGEOFENCESTATUS** | Geofence(s) defined and feature enabled | Enable alone may not emit status |
| **PQTMJAMMINGSTATUS** | Jamming / AIC / interference detect enabled | Feature mode required |
| **PQTMLS / PQTMSTD** | Matching FW feature / debug-stat mode if applicable | Verify against EA release notes |
| **PQTMPVT, PQTMVEL, PQTMDOP, PQTMPL, PQTMEPE, PQTMODO** | Normal nav (and odos if ODO) | PASS on EA 2026-09-18 without special modes |

## Process
1. Query identity (`PQTMVERNO`) into session / `run.json`.
2. Mute all listed messages.
3. **Enter prerequisite mode** for the target message (base+SVIN, geofence, jamming, etc.).
4. Enable **only** that message @ rate 1 → capture N seconds →
   `raw/nmea/<name>/<timestamp>.nmea` + `run.json` with `prerequisite` / `mode` fields.
5. Exit special mode (or factory-reset) before the next gated capture.
6. End of session: `PQTMRESTOREPAR` + `PAIR023`.

## Privacy
GGA/RMC/GLL/GNS (and some PQTM) contain lat/lon — keep under `raw/`, never publish unsanitized.

## Tool
`scripts/capture_nmea_com.py` — extend with prerequisite profiles, e.g.
`--mode default|svin|geofence|jamming` for the gated set.

## Full gated-mode plan

See [GATED_CAPTURE_MODES.md](GATED_CAPTURE_MODES.md). Library recipes: LC29H_GNSS `docs/GatedOutputs.md`. 

