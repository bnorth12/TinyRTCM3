# Capture plan — completeness gate (required before “full capture” done)

**CI contract remains synthetic.** Field/COM captures are soak + codec fixtures.
**Do not mark this plan complete until the Required gate below passes.**

## Hardware capability (Quectel LC29H protocol v1.5)

| RTCM type | LC29H as **base TX** | Notes |
|-----------|----------------------|--------|
| 1005 | **Output** (PAIR434,1) | Station ARP — library `enableRTCM` |
| 1006 | **Input only** | Cannot capture from LC29H base TX |
| 1033 | **Not in LC29H RTCM output table** | DePIN casters often want it; produce via TinyRTCM3 `encode1033` (synthetic) or ingest from another source |
| MSM4 | **Output** (PAIR432,0) | 1074/1084/1094/1114/1124 (+ regional) |
| MSM7 | **Output** (PAIR432,1) | 1077/1087/1097/1117/1127 |
| Ephemeris | **Output** (PAIR436,1) | 1019/1020/1042/… — optional, usually off for NTRIP |

Commands (same as `LC29H_GNSS`):
- Base mode: `PQTMCFGRCVRMODE,W,2`
- MSM: `PAIR432` (`-1` off, `0` MSM4, `1` MSM7)
- 1005: `PAIR434` (`0`/`1`)
- Ephemeris: `PAIR436` (`0`/`1`)
- Persist (DA/EA): `PQTMSAVEPAR` then `PAIR023`

Automation: `scripts/capture_base_com.py` (no QGNSS). Close QGNSS before opening the COM port.

## Required gate (must all pass)

### A. LC29H base → rover diet (what an LC29H rover consumes from this base)

| Run id | Must contain (CRC-OK) | Status source |
|--------|------------------------|---------------|
| `iso-1005` | 1005 only (no MSM) | COM capture |
| `msm4-bundle` | 1005 + MSM4 family | COM capture |
| `msm7-bundle` | 1005 + MSM7 family | COM capture |

### B. DePIN / NTRIP caster profile (GEODNET-style)

| Element | How satisfied on this stack |
|---------|-----------------------------|
| MSM4 + 1005 | **Required COM run** `msm4-bundle` (same as A) |
| 1033 receiver/antenna descriptor | **Not LC29H-TX** — Required **synthetic** golden via Codec `encode1033` (track REQ-COD-1033-E). Optional later: ingest 1033 from an external caster into `raw/external/` for soak only |
| 1006 | **Not LC29H-TX** — Required **synthetic** golden via Codec `encode1006` when implemented; rover-ingest optional |

### C. Privacy

- Every COM capture under `raw/` with `unsanitized: true`
- No public commit of ARP/1033 until sanitizer / dummy rewrite

## Optional (not blocking gate)

| Run id | Purpose |
|--------|---------|
| `eph-bundle` | PAIR436,1 + MSM off or with MSM — ephemeris RTCM soak |
| `iso-1006` / `iso-1033` | **Retired as LC29H-TX targets**; kept as labels for synthetic / external ingest only |

## Completeness checker

```bash
python scripts/verify_capture_completeness.py --raw-root raw
```

Exit 0 only when Required gate A files exist with expected type counts.
Gate B 1033/1006 synthetic rows are reported as `pending_codec` until those goldens exist.

## Execution (COM)

```bash
pip install pyserial
python scripts/capture_base_com.py --list-ports
python scripts/capture_base_com.py --port COM8 --run iso-1005 --duration 30 --configure-base
python scripts/capture_base_com.py --port COM8 --run msm4-bundle --duration 30 --configure-base
python scripts/capture_base_com.py --port COM8 --run msm7-bundle --duration 30 --configure-base
# optional:
python scripts/capture_base_com.py --port COM8 --run eph-bundle --duration 30 --configure-base
python scripts/verify_capture_completeness.py --raw-root raw
```

## Related

- `scripts/capture_base_com.py`
- `scripts/verify_capture_completeness.py`
- `scripts/gen_synthetic_goldens.py` / Codec encode for 1006/1033 gaps
- `docs/PRIVACY.md`, `docs/ICD.md`
## After capture (required)

When the capture exercise finishes, **factory-reset the module**:

```bash
python scripts/factory_reset_lc29h.py --port COM8
```

capture_base_com.py does this automatically at the end unless --no-factory-reset.
Sequence (from LC29H_GNSS): `PQTMRESTOREPAR` then `PAIR023`.
