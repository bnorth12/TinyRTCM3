# Gated capture modes — plan (LC29H family)

This document plans **mode-specific** captures for messages that returned EMPTY
when only `PQTMCFGMSGRATE` was toggled. Same idea applies to some **binary RTCM**
outputs (e.g. ephemeris). Cross-links:

- TinyRTCM3 capture tooling: `docs/CAPTURE_NMEA.md`, `docs/CAPTURE.md`
- LC29H library config recipes: `LC29H_GNSS/docs/GatedOutputs.md`

**Rule:** EMPTY after mute→enable alone ⇒ `mode_or_feature_gated_candidate` until
re-run under the prerequisite mode. Do not mark unsupported on EA without that.

---

## 1. ASCII / PQTM gated set (from 2026-09-18 EA session)

| ID | Message(s) | Prerequisite mode | Setup sketch (library / PQTM·PAIR) | Capture | Exit |
|----|------------|-------------------|--------------------------------------|---------|------|
| **M-SVIN** | `PQTMSVINSTATUS` | Base + survey-in **in progress** | `setReceiverModeBase()` → `configureBaseSurveyIn(minDur, accLimit)` → `PQTMSAVEPAR` → `PAIR023` → enable `PQTMSVINSTATUS` @1 → wait until status Valid=1 | Discrete `raw/nmea/PQTMSVINSTATUS/` while SVIN active | Complete SVIN or `restoreDefaults` + `PAIR023` |
| **M-GEOFENCE** | `PQTMGEOFENCESTATUS` | ≥1 geofence configured + feature on | `PQTMCFGGEOFENCE` (or library geofence helpers when exposed) → save/reboot if required → enable status @1 | Discrete geofence status file | Clear fences / restore |
| **M-JAMMING** | `PQTMJAMMINGSTATUS` | Jamming/AIC detect enabled | `PQTMCFGAIC` / `PAIR074` AIC enable (see library `queryJammingStatus`) → enable status @1 | Discrete jamming status | Disable AIC / restore |
| **M-ZDA** | `ZDA` | UTC/time available; **not** via PAIR062 types 0–5 on DA/EA | Use **`PQTMCFGMSGRATE,W,ZDA,1`** only (PAIR062 cannot select ZDA on DA/EA per library notes) after fix/time | Discrete ZDA | Rate 0 |
| **M-GRS** | `GRS` | Residuals / related nav path; CFGMSGRATE | `PQTMCFGMSGRATE,W,GRS,1` after 3D fix; confirm any residual-related PAIR if docs require | Discrete GRS | Rate 0 |
| **M-GST** | `GST` | Error-model / quality output; CFGMSGRATE | Prefer `setMessageRate("GST",…)` (library already routes GST via CFGMSGRATE, not PAIR062) after fix | Discrete GST | Rate 0 |
| **M-GNS** | `GNS` | Multi-GNSS NMEA sentence enable | `PQTMCFGMSGRATE,W,GNS,1`; confirm constellation enable (`PQTMCFGCNST`) | Discrete GNS | Rate 0 |
| **M-LS / M-STD** | `PQTMLS`, `PQTMSTD` | FW feature mode | Enable with MsgVer; if still empty, check EA FW notes / Quectel for feature flags | Discrete files | Rate 0 / restore |

### Baseline (no special mode) — already PASS on EA
RMC, GGA, GSV, GSA, VTG, GLL, PQTMEPE, PQTMGEOFENCESTATUS*, PQTMPVT, PQTMPL, PQTMDOP, PQTMVEL, PQTMODO  

\*Geofence **status** passed once on EA; still document M-GEOFENCE for intentional fence configs.

---

## 2. Binary RTCM gated set (same pattern)

| ID | Message / stream | Prerequisite | Setup | Capture | Notes |
|----|------------------|--------------|-------|---------|-------|
| **B-MSM4** | MSM4 + 1005 | Base mode | `CFGRCVRMODE,W,2` + `PAIR432,0` + `PAIR434,1` | Done — PASS | — |
| **B-MSM7** | MSM7 + 1005 | Base mode | `PAIR432,1` + `PAIR434,1` | Done — PASS | Library `enableRTCM(true)` |
| **B-1005** | 1005 only | Base mode | `PAIR432,-1` + `PAIR434,1` | Done — PASS | — |
| **B-EPH** | RTCM ephemeris (1019/1020/1042/1044/1046…) | Base + **PAIR436,1**; often needs SAVE+`PAIR023`; MSM may need off to see cleanly | `PAIR432,-1`, `PAIR434,0`, `PAIR436,1`, save/reboot, settle longer, capture ≥60s | **Failed once** (bytes but 0 CRC types) — retry with reboot + longer settle; verify with `PAIR437` query | Do not mark unsupported until retry |
| **B-1006 / B-1033** | 1006, 1033 | **Not LC29H TX** | N/A on module UART TX | Synthetic / encode in TinyRTCM3 | Protocol: 1006 input-only; 1033 not listed |

---

## 3. Execution order (EA on COM)

1. Keep existing PASS RTCM + baseline NMEA as-is.  
2. **M-ZDA → M-GNS → M-GRS → M-GST** (nav fix, CFGMSGRATE only).  
3. **M-JAMMING** (AIC on).  
4. **M-GEOFENCE** (configure fence).  
5. **M-SVIN** (base+SVIN; longest).  
6. **B-EPH** retry (save/reboot/settle).  
7. **M-LS / M-STD** last (feature probe).  
8. Always **factory-reset** (`PQTMRESTOREPAR` + `PAIR023`) after gated series.

Each run: `run.json` must include `required_mode`, `prerequisite`, `commands_sent`, identity, `captured_at_*`.

---

## 4. Library follow-through

Promote recipes into **LC29H_GNSS** `docs/GatedOutputs.md` (and later optional helpers):

- `applySurveyBaseProfile` + enable `PQTMSVINSTATUS` (already largely present)
- AIC / jamming enable + `PQTMJAMMINGSTATUS` rate
- Geofence config + status rate
- Explicit note: **PAIR062 types 0–5 only on DA/EA** — use CFGMSGRATE for ZDA/GRS/GST/GNS/GST/SVIN/EPE

TinyRTCM3 remains capture/golden owner; LC29H_GNSS owns durable config semantics.
