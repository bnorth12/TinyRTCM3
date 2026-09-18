# TinyRTCM3 — Solid Library Gate (v0.5)

**Status:** Active execution (2026-09-18)  
**Scope:** TinyRTCM3 in-repo only. No LC29H_GNSS / Base Station edits.  
**Re-evaluate** after this gate before any peer/product CRC or Hub integration.

## Decisions locked

| Topic | Decision |
|-------|----------|
| Peer CRC duplicates | **Keep** for now; TinyRTCM3 remains reference. Update peers later when touching those projects. |
| Table CRC | **Implement** in TinyRTCM3 (bit-identical to bit engine). |
| Hub ↔ Registry | **Skip** until a real need appears. |
| Ephemeris | **Deferred** / optional; not a gate (LC29H TX unproven). |
| NTRIP sockets | Stay app-owned (`INtripSink` boundary only). |

## Work packs (order)

### P0 — Truth pack
- Bump `kVersion*` + `library.properties` / `library.json` to **0.5.0**
- Sync README, ICD, REQUIREMENTS.md, keywords, REQ notes (remove false "Unsupported stub")
- Mark CAP-CRC-24Q as foundational evaluate+generate in ICD

### P1 — Privacy pack
- `policyFilterIsoOnly` (1005/1006/1033 only)
- Public `rewrite1006ToPublishIdentity`
- Sanitize-before-emit helper usable with Hub Emit
- Example: `FilteredSanitizeHub` (Policy + Sanitize + Stats; no Registry)

### P2 — Pump pack
- `Hub::feed(const uint8_t* data, size_t len, size_t* consumed)`
- StreamStats type histogram (1005/1006/1033/MSM4/MSM7/other) updated on OK emit path
- Self-tests + host coverage

### P3 — Table CRC
- 256-byte (or 256×uint32) table path; default on; bit path retained for identity test
- Flip REQ-CRC-04 / CAP-CRC-TABLE to met
- Self-test: random + golden payloads bit-identical

### P4 — Process pack
- Commit sanitized `docs/RTCM_CAPTURE_EVAL.md` (no secrets)
- Add `scripts/verify_capture_completeness.py`
- Align `scripts/sanitize_rtcm_location.py` notes with C++ rewrite
- GitHub Actions: Arduino-cli compile of examples (plus existing host-verify)

### Explicit non-goals (frozen)
- Hub↔Registry wiring, full MSM encode, NTRIP/TLS, forced LC29H dependency, eph as gate

## Exit criteria
- Host self-tests green; table vs bit CRC identical
- Docs/package version 0.5.0 agree with `TinyRtcmRequirements.h`
- FilteredSanitizeHub example builds (Arduino-cli CI)
- Capture eval + completeness script present
- User re-eval before peer/product work

## Suggested commits
1. `docs(v0.5): solid-library plan + REQ catalog`
2. `feat(v0.5): privacy IsoOnly, rewrite1006, sanitize-emit + example`
3. `feat(v0.5): Hub batch feed + Stats type histogram`
4. `feat(v0.5): table-accelerated CRC-24Q bit-identical`
5. `chore(v0.5): process pack + Arduino CI + truth sync`
## Execution log
- 2026-09-18: P0�P3 implemented; host self-tests 22/0/0; package 0.5.0.

