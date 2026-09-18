# TinyRTCM3 — Interface Control Document (ICD) v0.1

**Status:** Initial proposed scope. Normative shall-statements also live in
`src/TinyRtcmRequirements.h` (`kRequirements[]`, `kCapabilities[]`). If prose and
header disagree, **fix the header and open a PR to sync this doc**.

**Stack boundary:** `LC29H UART -> (pump/ring/demux) -> TinyRTCM3 -> app policy -> NTRIP/radio`. Quectel config + HW UART/NMEA remain in LC29H_GNSS. TinyRTCM3 does **not** own HW FIFO or the software ring — see [ARCHITECTURE.md](ARCHITECTURE.md) and [INTEGRATION.md](INTEGRATION.md) (optional peer — not required by LC29H_GNSS). This ICD covers TinyRTCM3 API/CAP/REQ only.
Quectel config/UART remains in LC29H_GNSS-Library. This ICD covers TinyRTCM3 only.

## 1. Purpose

TinyRTCM3 is a small Arduino/PlatformIO RTCM 3.x helper: transport framing/CRC,
selective decode/encode, passthrough hub, and privacy-aware golden handling.
It is **not** a full RTCM stack and **not** an NTRIP client.

## 2. How capabilities and requirements relate

| Layer | Artifact | Role |
|-------|----------|------|
| Capability | `Capability` / `CAP-*` | Product feature the lib claims or plans |
| Requirement | `Requirement` / `REQ-*` | Normative shall for that feature |
| Link | `Capability.reqIds` | Every capability **must** list its REQ-IDs |
| Verify | `runSelfTests` | Checks implemented CAPs / expected Unsupported stubs |

**Rule:** No capability without documented requirements. Adding a CAP without REQ
rows is a documentation defect.

## 3. Capability catalog (proposed scope)

### CAP-CRC-24Q — CRC-24Q transport *(implemented)*
**Intent:** Seal and check every RTCM3 frame.
- **REQ-CRC-01** Compute CRC-24Q over preamble+length+payload
- **REQ-CRC-02** Verify CRC on complete frames; report failure
- **REQ-CRC-03** Append CRC when finalizing outbound frames  
**API:** `crc24q`, `frameCrcOk`, `appendCrc24q`, `finalizeFrame`  
**Limit:** bit-at-a-time (see CAP-CRC-TABLE).

### CAP-CRC-TABLE — table-accelerated CRC *(stub / deferred)*
- **REQ-CRC-04** Optional table path, bit-identical to CAP-CRC-24Q

### CAP-FRAME-ASM — frame assembler *(implemented)*
**Intent:** Byte stream → discrete CRC-valid frames.
- **REQ-ASM-01** Assemble frames from stream
- **REQ-ASM-02** BadCrc + resync on CRC fail
- **REQ-ASM-03** Overflow when frame exceeds buffer  
**API:** `FrameAssembler::feed` / `reset`

### CAP-BIT-WRITE / CAP-BIT-READ — bit buffer
**Intent:** MSB-first DF packing for codecs.
- **REQ-BIT-01** `putBits` *(implemented)*
- **REQ-BIT-02** `getBits` *(stub)*

### CAP-HUB — passthrough hub *(implemented)*
- **REQ-HUB-01** Assemble → optional filter → emit
- **REQ-HUB-02** Filter false drops frame without error status  
**API:** `Hub::setFilter`, `setEmit`, `feed`

### CAP-POLICY — stock filters *(stub)*
- **REQ-POL-01** Helpers: ISO-only, drop-MSM, allowlist
- **REQ-POL-02** Pure predicates for `Hub::setFilter`  
**API:** `TinyRtcmPolicy.h` stubs

### CAP-REGISTRY — message registry *(stub)*
- **REQ-REG-01** Map DF002 type → handler
- **REQ-REG-02** Unknown type → Unsupported, stream continues  
**API:** `TinyRtcmRegistry.h` stubs

### CAP-STATS — stream statistics *(stub)*
- **REQ-STAT-01** Counters: OK frames, BadCrc, drops, bytes
- **REQ-STAT-02** Reset stats without resetting assembler  
**API:** `TinyRtcmStats.h` stubs

### CAP-CODEC-1005 / 1006 / 1033 / MSM *(stubs)*
| CAP | Requirements | Notes |
|-----|----------------|-------|
| CAP-CODEC-1005 | REQ-COD-1005-D/E/R | Decode/encode/privacy rewrite |
| CAP-CODEC-1006 | REQ-COD-1006-D/E | ARP + antenna height |
| CAP-CODEC-1033 | REQ-COD-1033-D/E | Descriptors |
| CAP-CODEC-MSM | REQ-COD-MSM-S, REQ-COD-MSM-X | Header+CNR summary; **no MSM encode in v1** |

**API:** `TinyRtcmCodec.h` — all bodies return `Status::Unsupported` until implemented.

### CAP-SANITIZE — location sanitization *(partial)*
- **REQ-SAN-01** No unsanitized ARP/1033 in public tree *(process — met)*
- **REQ-SAN-02** C++ drop/rewrite API *(stub; rewrite needs encode1005)*

### CAP-GOLDENS — golden corpora *(implemented process)*
- **REQ-GOLD-01** Synthetic = CI contract
- **REQ-GOLD-02** Field = optional, sanitizer-gated

### CAP-SELFTEST — in-library tests *(implemented)*
- **REQ-VER-01** `runSelfTests` covers implemented CAPs
- **REQ-VER-02** Unimplemented codec APIs expected as Unsupported skips

### CAP-NTRIP-BOUNDARY — NTRIP out of scope *(documented)*
- **REQ-NTRIP-01** Sockets/TLS/NTRIP are app-owned; `INtripSink` is a boundary stub only

## 4. External interfaces (byte-level)

### 4.1 Inbound stream
Caller feeds bytes (UART/TCP). Library returns `NeedMore` until a frame completes,
then `Ok` + `FrameView`, or `BadCrc` / `Overflow` / `InvalidArg`.

### 4.2 Outbound frame
Encoder fills payload bits, then **must** call `finalizeFrame` or `appendCrc24q`
before emit. Incomplete CRC is a contract violation.

### 4.3 Hub callbacks
```text
Filter(type, frame, len, ctx) -> bool   // false = drop
Emit(frame, len, ctx)                   // full frame incl. CRC
```

### 4.4 Status codes
`Ok`, `NeedMore`, `BadCrc`, `Overflow`, `Unsupported`, `InvalidArg` — see
`TinyRtcmTypes.h`.

## 5. Privacy interface

Public artifacts may only contain ARP/station identity equal to published dummy
constants (`kPublishArpEcef01mm*`, `kPublishStationId`) or omit location messages.
See `docs/PRIVACY.md`.

## 6. Non-goals (v1)

- Full MSM observation encode/decode cells
- RTCM 1230 encode
- NTRIP client/server, TLS, caster protocol
- Quectel `$PQTM*` / UART configuration

## 7. Document control

| Version | Date | Notes |
|---------|------|-------|
| 0.1 | 2026-09-18 | Initial ICD; stubs for unimplemented CAPs; REQ linked per CAP |

When implementing a stub: update `kRequirements[].met`, `kCapabilities[].implemented`,
`runSelfTests`, synthetic goldens, and this ICD in one change set.
