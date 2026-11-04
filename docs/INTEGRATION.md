# Integration & scope of change (v0.2)

TinyRTCM3 is the **priority build artifact**. Integrating it into LC29H_GNSS and
product firmwares (base / rover / display) can be disruptive if done as a big bang.
This doc locks **what changes when**, so contracts stay stable while codecs mature.

Companion: [ARCHITECTURE.md](ARCHITECTURE.md) (ownership), [ICD.md](ICD.md) (API shalls).

## 1. Principles

1. **Library-first** — land framing/CRC/hub/codecs + host tests in TinyRTCM3 before rewriting apps.
2. **Consume-only integration first** — apps may call TinyRTCM3 without deleting existing RTCM paths until parity is proven.
3. **One owner per concern** — no second CRC assembler inside LC29H_GNSS once TinyRTCM3 Hub is adopted.
4. **Demux stays at the pump** — LC29H_GNSS (or app) keeps NMEA mailboxes; TinyRTCM3 never steals `$` lines.
5. **Disruptive cuts are explicit milestones** — versioned, with a short migration note in the app repo.


## 1a. Independent libraries (hard rule)

TinyRTCM3 and LC29H_GNSS are **fully independent**. Neither depends on the other in either direction.

| Rule | Meaning |
|------|---------|
| No dependency either way | Neither library's `library.properties` / `library.json` / PlatformIO manifest lists the other |
| No detection | Neither library uses `__has_include` or macros to detect the other; no class layout depends on the other's presence |
| No shared types | Neither public API exposes the other's types (LC29H `UartPump` / `LC29H_Rtcm::Assembler` have no `setStreamStats()`) |
| Single-library examples | Each library's examples use only that library. Sketches needing both live in the application repo |
| App wires them | Product firmware pops whole CRC-checked frames from LC29H and passes them to TinyRTCM3 (FrameQueue, StreamStats, decode) |
| Caller serializes | `FrameQueue` has no internal locking; push/pull/reads must be on one task or under one lock |

Phases B-D below never change this rule.

## 2. Phase plan

### Phase A — TinyRTCM3 standalone (current focus)

**In scope**
- Assembler, CRC-24Q, Hub, Policy stubs, Codec stubs -> real `getBits` + decode1005
- Host tests on iso-1005 + msm4-bundle goldens (sanitized)
- Synthetic 1006/1033 encode goldens for DePIN/NTRIP completeness
- Docs: ARCHITECTURE, ICD, this file

**Out of scope**
- Changing LC29H_GNSS public API
- Replacing LC29H UartPump RTCM FIFO
- Shipping NTRIP client logic inside TinyRTCM3

**Exit criteria:** host CI green on CRC/assemble + decode1005 against goldens; Hub example unchanged.

### Phase B — Side-by-side consume (low disruption)

**In scope**
- App or thin adapter feeds copies of RTCM bytes/frames into `tinyrtcm3::Hub` for metrics / filter / sanitize-before-publish
- LC29H_GNSS may gain an **optional** helper or example only — TinyRTCM3 remains **never** a required dependency of the LC29H package or its existing examples

**Out of scope**
- Removing existing writeRaw / BLE RTCM paths
- Making Arduino Library Manager dependency mandatory for LC29H_GNSS

**Exit criteria:** one product firmware (prefer base) can log type histogram via TinyRTCM3 without behavior change to rover link.

### Phase C — Pump contract alignment (moderate disruption)

**In scope**
- Stable pump interface: RTCM byte sink callback or `feedRtcmByte()` hook from UartPump after binary detection
- Single demux policy: `$` -> NMEA mailboxes; `0xD3` -> TinyRTCM3 Hub
- Deprecate duplicate CRC-24Q copies in apps once Hub is authoritative

**Risk:** timing/budget in drain/frame loops; Hub work must stay bounded (assemble only in Phase C; decode optional/off-loop).

**Exit criteria:** base firmware emits to NTRIP/radio **only** via Hub emit path in at least one example; CRC mismatch stats visible.

### Phase D — Codec & privacy in the live path (higher disruption)

**In scope**
- Optional decode1005 for station identity UI / survey checks
- Sanitize/rewrite ARP before any public network publish (DePIN)
- MSM header+CNR summary for diagnostics (not full observation decode)

**Out of scope (still)**
- Full MSM observation encode/decode
- Quectel PAIR/PQTM inside TinyRTCM3

**Exit criteria:** public publish path cannot leak field ECEF; ICD CAP-CODEC-* marked implemented for the subset used.

## 3. What we will not break casually

| Surface | Stability promise during Phase A-B |
|---------|-------------------------------------|
| `Hub::feed` / `setFilter` / `setEmit` | Stable |
| `FrameAssembler::feed` frame layout | Stable (RTCM3 transport) |
| Status enum NeedMore/Ok/BadCrc/Overflow | Stable; new values only appended |
| LC29H_GNSS UartPump mailbox priorities | Unchanged until Phase C milestone |
| App sketch enableRTCM / survey presets | Unchanged by TinyRTCM3 commits |

## 4. Suggested repo boundaries

| Repo | May depend on | Must not absorb |
|------|---------------|-----------------|
| **TinyRTCM3** | nothing Quectel-specific | PAIR/PQTM, UART drivers |
| **LC29H_GNSS** | TinyRTCM3 as **optional** later | Re-implement CRC/assembler long-term |
| **esp32s3-lc29h-base / rover / display** | both | Forked RTCM framing copies after Phase C |

## 5. Integration checklist (when an app adopts Hub)

- [ ] Software ring owned by pump; ISR does not call `Hub::feed`
- [ ] Demux `$` vs `0xD3` before or while feeding
- [ ] Emit callback copies or forwards under a known MTU/budget
- [ ] Filter policy explicit (rover-lite: 1005+MSM4/7)
- [ ] Privacy: no raw 1005/1006 to public sinks
- [ ] Factory/field config still via LC29H_GNSS (PAIR432/434, survey-in)

## 6. Decision log (2026-09-18)

- Hardware FIFO / ring ownership -> **app or LC29H pump**, not TinyRTCM3
- Message differentiation -> **RTCM self-describing framing**; library helpers identify types
- NMEA -> **separate path**
- Integration -> **phased**; TinyRTCM3 library work precedes disruptive GNSS app changes
- Message-type growth -> additive Codec/Registry/Policy only; see ARCHITECTURE.md §5 (passthrough first, decode optional)
