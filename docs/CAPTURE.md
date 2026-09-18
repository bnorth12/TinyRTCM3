# QGNSS base capture plan (intentional goldens)

Goal: collect **known message sets** for soak tests. **CI contract remains synthetic.** Field captures are optional and must be **location-sanitized** before any public commit (see [PRIVACY.md](PRIVACY.md)).

## Preconditions

- LC29H (or other) in **base / RTCM out** mode on a known COM port.
- QGNSS (or equivalent) logging raw RTCM to a dedicated file under a local `raw/` directory.
- Message enable/disable plan below — captures only reflect **what you enabled that day**.

## Runs

| Run id | Enable | Notes |
|--------|--------|--------|
| `iso-1005` | 1005 only | Station ARP (no height antenna delta) |
| `iso-1006` | 1006 only | ARP + antenna height |
| `iso-1033` | 1033 only | Antenna/receiver descriptors |
| `msm4-bundle` | 1005 + MSM4 (1074/1084/1094/1124 as available) | Typical NTRIP lite set |
| `msm7-bundle` | 1005 + MSM7 (1077/…) | Higher bandwidth |

For each run:

1. Clear old log / start a new file: `raw/<run-id>/<timestamp>.bin` (or `.log`).
2. Record ≥30 s of steady output after first valid CRC frames.
3. Write `raw/<run-id>/run.json`:

```json
{
  "run_id": "iso-1005",
  "device": "LC29H",
  "firmware": "<version>",
  "qgnss": "2.5",
  "port": "COM7",
  "notes": "messages enabled: 1005 only",
  "unsanitized": true
}
```

4. Do **not** put real ECEF or street address in `run.json`.
5. Sanitize → `test/golden/field/<run-id>/` before any git add.

## Layout after sanitize

```
test/golden/field/
  iso-1005/
    run.json          # unsanitized=false, dummy ARP noted
    frames.bin        # CRC-valid RTCM stream
    manifest.json     # type counts
```

## What we already know from opportunistic logs

- Dedicated `RTCM_*.log` channels were often **0 bytes**; useful frames lived in mixed COM history logs.
- Opportunistic Jul 26 capture had CRC-OK **1005 + MSM4/7**; **1006/1033 absent** (not enabled that day).
- Do not treat opportunistic dumps as the contract — re-capture per table above when ready.

## Related

- Synthetic generator: `scripts/gen_synthetic_goldens.py`
- Sanitizer: `scripts/sanitize_rtcm_location.py`
