# Privacy: location & identity in RTCM goldens

## Why this matters

RTCM **1005** and **1006** carry the base Antenna Reference Point (ARP) in **ECEF**. Publishing an unsanitized capture from your farm or shop **publishes your coordinates**. Message **1033** can leak antenna/receiver descriptor strings.

Raw QGNSS / COM logs are **not anonymous**. Treat them like site survey data.

## Policy (locked)

1. **`raw/` is local-only** — gitignored. Never push unsanitized QGNSS, COM, or `historyLogFile` dumps.
2. **Public `test/golden/field/`** may only contain frames that have been through `scripts/sanitize_rtcm_location.py` (or equivalent). That tool **rewrites 1005/1006 ARP** (and station id) to the published dummy ECEF in `TinyRtcmTypes.h`, and **strips or replaces 1033** descriptors.
3. **CI uses synthetic goldens only** (`test/golden/synthetic/`). Field goldens are optional soak / HIL, not the merge gate.
4. Do not commit screenshots or QGNSS UI that show Lat/Lon/height of a real site alongside RTCM dumps.

## Published dummy ARP

Used after sanitization (see `kPublishArpEcef01mm*` in `TinyRtcmTypes.h`):

- Approx WGS84: lat 0°, lon 0°, h ≈ 0
- Station id: `0`

Any PR that adds `field/` frames without a sanitizer note in the commit message should be rejected.

## Capture workflow (short)

1. Capture into a local folder under `raw/` (never the repo tree that will be pushed without review).
2. Run sanitizer → write into `test/golden/field/<run-id>/`.
3. Commit only the sanitized tree + `run.json` metadata (no real ECEF in JSON either).
