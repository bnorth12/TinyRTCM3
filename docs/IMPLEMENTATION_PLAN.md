# TinyRTCM3 — Implementation Plan

**Status:** Active plan (2026-09-18)  
**Normative inputs:** [ARCHITECTURE.md](ARCHITECTURE.md), [INTEGRATION.md](INTEGRATION.md),
[ICD.md](ICD.md), [REQUIREMENTS.md](REQUIREMENTS.md) / `src/TinyRtcmRequirements.h`,
[RTCM_CAPTURE_EVAL.md](RTCM_CAPTURE_EVAL.md), [PRIVACY.md](PRIVACY.md)

This plan turns locked contracts into sequenced library work. It does **not** make
TinyRTCM3 required by LC29H_GNSS (optional peer). GNSS app cutover stays Phase B+
in INTEGRATION and is out of scope for v0.1–v0.3 library milestones unless noted.

---

## 1. Goals and non-goals

### Goals
- Reliable RTCM3 **framing + CRC** pipe (already largely done).
- **Selective codecs** starting with 1005; synthetic 1006/1033 for DePIN/NTRIP completeness.
- **Host/CI goldens** from sanitized field captures + synthetics.
- Modular growth per ARCHITECTURE §5 (passthrough first, decode optional).

### Non-goals (v1)
- Full MSM observation cell decode/encode (`REQ-COD-MSM-X` met by refusing encode).
- NTRIP/TCP/TLS client (app-owned; `INtripSink` boundary only).
- UART / Quectel PAIR/PQTM (LC29H_GNSS).
- Mandatory dependency from LC29H_GNSS.
- Ephemeris UART goldens as a release gate (optional CAP after failed EA captures).
- Table-accelerated CRC as a release gate (`REQ-CRC-04` deferred).

---

## 2. Current baseline (done)

| CAP / piece | State |
|-------------|--------|
| CAP-CRC-24Q | Implemented (bit-at-a-time) |
| CAP-FRAME-ASM | Implemented |
| CAP-BIT-WRITE | Implemented |
| CAP-HUB | Implemented |
| CAP-GOLDENS (process) | Synthetic corpus + privacy scripts; `raw/` gitignored |
| CAP-SELFTEST | Host smoke + `runSelfTests` |
| CAP-NTRIP-BOUNDARY | Documented |
| Field captures (local) | iso-1005, msm4-bundle, msm7-bundle CRC-validated |
| Contracts | Ownership, optional peer, phased integration, message-extension model |

**Immediate gap blocking codecs:** `CAP-BIT-READ` / `REQ-BIT-02` (`getBits` stub).

---

## 3. Milestone map

### v0.1 — Decode foundation (library-critical path)

**Intent:** First real decode path + CI that proves it against goldens.

| Work item | REQs / CAPs | Exit criteria |
|-----------|-------------|----------------|
| Implement `BitBuffer::getBits` | REQ-BIT-02, CAP-BIT-READ | Unit tests: round-trip putBits↔getBits; mark `met` |
| `decode1005` | REQ-COD-1005-D | Parses station id + ECEF from golden 1005 frames |
| Host tests: assemble+CRC on sanitized msm4/1005 slices | REQ-GOLD-01/02, ASM/CRC | CI runs without `raw/` |
| Promote sanitized fixtures into `test/golden/field/` | PRIVACY, REQ-SAN-01 | No real ARP in git; scripted sanitize from local raw |
| Extend `runSelfTests` for getBits + decode1005 | REQ-VER-01 | Self-test fails if decode regresses |
| Update ICD/REQUIREMENTS `implemented`/`met` flags | docs sync | Header and prose agree |

**Explicitly not in v0.1:** encode1005 rewrite, Policy/Stats polish, 1006/1033, MSM summary, LC29H adapters.

### v0.2 — Pipe quality + privacy rewrite

| Work item | REQs / CAPs | Exit criteria |
|-----------|-------------|----------------|
| Stock Policy predicates usable with Hub | REQ-POL-01/02, CAP-POLICY | ISO/station+MSM helpers tested |
| Stream Stats | REQ-STAT-01/02, CAP-STATS | Counters + reset without clearing assembler |
| Registry table dispatch | REQ-REG-01/02, CAP-REGISTRY | Unknown type → Unsupported; stream continues |
| `encode1005` + rewrite-to-publish-identity | REQ-COD-1005-E/R | Dummy ARP constants match Types.h |
| C++ Sanitize API (drop or rewrite 1005/1006/1033) | REQ-SAN-02, CAP-SANITIZE | Public export path covered by test |
| Hub example optional filter/stats wiring | docs/example | PassThroughHub still builds minimal |

### v0.3 — DePIN/NTRIP message completeness (synthetic-first)

| Work item | REQs / CAPs | Exit criteria |
|-----------|-------------|----------------|
| `encode1006` (+ decode1006 if cheap) | REQ-COD-1006-* | Synthetic golden round-trip |
| `encode1033` (+ decode1033 if cheap) | REQ-COD-1033-* | Synthetic golden; strings sanitized in public fixtures |
| Generator script updates | REQ-GOLD-01 | `gen_synthetic_goldens.py` emits 1006/1033 |
| ICD notes LC29H cannot TX 1006/1033 | capture eval | Apps know to synthesize for caster profiles |

### v0.4 — Optional diagnostics (schedule after v0.3 or in parallel if staffing allows)

| Work item | REQs / CAPs | Exit criteria |
|-----------|-------------|----------------|
| MSM4/7 header + mean CNR summary | REQ-COD-MSM-S, CAP-CODEC-MSM | Host test on sanitized msm4/msm7 bundles |
| Table CRC (optional accel) | REQ-CRC-04 | Bit-identical to bit engine; feature flag |
| Ephemeris codec CAP (only if goldens appear) | new CAP or defer | Not a gate; document Unsupported if no UART TX |

### Integration track (not a TinyRTCM3 version bump)

See [INTEGRATION.md](INTEGRATION.md):

| Phase | When relative to library | Notes |
|-------|--------------------------|-------|
| A | Now / through v0.3 | Library-only |
| B | After v0.1 usable | Optional side-by-side consume example; **no** LC29H required dep |
| C–D | Product decision | App pump cutover / live sanitize — out of library-only plan |

---

## 4. Suggested implementation order (v0.1 detail)

1. **getBits** + bit-buffer host tests (blocked decode otherwise).
2. **Sanitize/copy pipeline**: scripted extract of 1005 frames from local `raw/iso-1005` and `raw/msm4-bundle` → rewrite ARP → `test/golden/field/`.
3. **decode1005** against those fixtures + synthetic `1005_dummy_arp.bin`.
4. **Assembler soak test**: feed msm4-bundle bytes, expect CRC-OK type histogram (1005+MSM4), no decode required.
5. Flip REQ/CAP flags; extend Verify; CI green on host target.
6. Tag `v0.1.0`.

Then v0.2 items in the table order (Policy → Stats → Registry → encode1005/sanitize).

---

## 5. Test strategy

| Layer | What | Where |
|-------|------|--------|
| Host unit | CRC, assembler, bit buffer, codecs | `test/host/` |
| Synthetic golden | Merge gate | `test/golden/synthetic/` |
| Field golden | Optional soak; sanitizer-gated | `test/golden/field/` |
| Self-test | On-device / host smoke | `runSelfTests` |
| Capture tooling | COM scripts | `scripts/`; output under gitignored `raw/` |

**CI rule:** never require `raw/` or unsanitized ECEF. Fail PR if field goldens contain real ARP (sanitizer check).

---

## 6. Documentation updates per milestone

Each milestone PR should touch as needed:

- `TinyRtcmRequirements.h` (`met` / `implemented`)
- [ICD.md](ICD.md), [REQUIREMENTS.md](REQUIREMENTS.md)
- [ARCHITECTURE.md](ARCHITECTURE.md) only if contracts change
- This plan: check off exit criteria / date the milestone

---

## 7. Risk register

| Risk | Mitigation |
|------|------------|
| getBits bit-order bugs poison all codecs | Round-trip tests before decode1005 |
| Real ARP leaks into git | Sanitize script + CI grep / PRIVACY checklist |
| Scope creep into full MSM decode | REQ-COD-MSM-X; summary-only CAP |
| Premature LC29H coupling | Optional peer rule; Phase B example only |
| Eph never appears on EA UART | Keep optional; synthetic or skip |
| Hub reentrancy / ISR misuse | ARCHITECTURE contracts; docs warn ISR must not `feed` |

---

## 8. Definition of done (library v1 slice)

Call the **v1 library slice** complete when:

- [ ] v0.1 + v0.2 + v0.3 exit criteria met
- [ ] CAP-BIT-READ, CAP-CODEC-1005 (D/E/R), CAP-POLICY, CAP-STATS, CAP-REGISTRY, CAP-SANITIZE marked implemented for claimed REQs
- [ ] CAP-CODEC-1006/1033 encode path green on synthetics
- [ ] CAP-CODEC-MSM summary either implemented (v0.4) or explicitly deferred in ICD with Unsupported
- [ ] Host CI green; no required LC29H dependency
- [ ] PassThroughHub (+ optional filtered example) builds

MSM summary and table CRC may lag as v0.4 without blocking the v1 slice if ICD says so.

---

## 9. Decision log

| Date | Decision |
|------|----------|
| 2026-09-18 | Enough architecture/REQ/golden basis to execute this plan |
| 2026-09-18 | v0.1 = getBits + decode1005 + sanitized host goldens |
| 2026-09-18 | 1006/1033 = synthetic-first (LC29H no TX) |
| 2026-09-18 | Eph not a release gate |
| 2026-09-18 | TinyRTCM3 remains optional peer to LC29H_GNSS |
