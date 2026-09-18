# Base capture plan (intentional goldens)

Goal: collect **known message sets** for soak tests. **CI contract remains synthetic.**
Field captures are optional and must be **location-sanitized** before any public commit
(see [PRIVACY.md](PRIVACY.md)).

## Preferred automation (no QGNSS)

Drive the LC29H over the **COM port** with the same payloads wrapped by
`LC29H_GNSS` (`makeSentence` / `sendPayload`):

| Library API | Payload |
|-------------|---------|
| `setReceiverModeBase()` | `PQTMCFGRCVRMODE,W,2` |
| `enableRTCM(true)` | `PAIR432,1` then `PAIR434,1` (MSM7 + 1005) |
| `enableRTCM(false)` | `PAIR432,-1` then `PAIR434,0` |
| MSM4 + 1005 | `PAIR432,0` + `PAIR434,1` |
| Persist (DA/EA) | `PQTMSAVEPAR` then `PAIR023` |

Script:

```bash
pip install pyserial
python scripts/capture_base_com.py --list-ports
python scripts/capture_base_com.py --dry-run --run all --configure-base
python scripts/capture_base_com.py --port COM7 --run iso-1005 --duration 30 --configure-base
python scripts/capture_base_com.py --port COM7 --run msm7-bundle --duration 30
```

Outputs under local `raw/<run-id>/` (gitignored): `<timestamp>.bin` + `run.json`
with `unsanitized: true`. Close QGNSS first so the port is free.

**Limitations:** `iso-1006` / `iso-1033` have no first-class PAIR wrappers in
`LC29H_GNSS` yet — the script turns MSM/1005 off and warns; extend when Quectel
message-rate docs for those types are wired into the library.

## Runs

| Run id | Enable (via script) | Notes |
|--------|---------------------|--------|
| `iso-1005` | `PAIR432,-1` + `PAIR434,1` | Station ARP only |
| `iso-1006` | limited stub | Needs library/docs for exclusive 1006 |
| `iso-1033` | limited stub | Needs library/docs for exclusive 1033 |
| `msm4-bundle` | `PAIR432,0` + `PAIR434,1` | Typical NTRIP lite set |
| `msm7-bundle` | `PAIR432,1` + `PAIR434,1` | Matches `enableRTCM(true)` |

For each run:

1. Capture ≥30 s after first valid CRC frames.
2. Do **not** put real ECEF or street address in `run.json`.
3. Sanitize → `test/golden/field/<run-id>/` before any git add.

## Manual QGNSS (optional)

QGNSS remains fine for interactive bring-up. It has **no CLI** for automation;
prefer `capture_base_com.py` for repeatable goldens.

## Related

- `scripts/capture_base_com.py` — COM automation
- `scripts/gen_synthetic_goldens.py` — CI contract
- `scripts/sanitize_rtcm_location.py` — privacy gate
- `LC29H_GNSS` — `enableRTCM`, `setReceiverModeBase`, CommandReference.md
