# Architecture & contracts (v0.2)

Normative API shalls still live in `src/TinyRtcmRequirements.h` and [ICD.md](ICD.md).
This document locks **layer ownership** and **integration contracts** so firmware,
LC29H_GNSS, and applications stay aligned while TinyRTCM3 evolves.

## 1. Stack picture

```
Application (base / rover / DePIN caster firmware)
  policy, NTRIP session, radio TX, logging, product mode
        | Emit(frame) / decode                 | NMEA lines
        v                                      v
TinyRTCM3                                 NMEA / PQTM path
  FrameAssembler + Hub                      (LC29H_GNSS / app parser)
  CRC-24Q, type helpers
  optional Codec/Sanitize
        ^
        | feed(byte) RTCM only
        v
Transport pump (app or LC29H_GNSS UartPump)
  drain HW FIFO/DMA -> software ring -> demux $ vs 0xD3
        ^
        |
      LC29H UART (hardware)
```

Quectel **PAIR/PQTM config** stays in LC29H_GNSS. TinyRTCM3 never opens a `HardwareSerial`.

**Relationship:** TinyRTCM3 is an **optional peer** to LC29H_GNSS. Applications may use one, the other, or both. LC29H_GNSS must not require TinyRTCM3 to compile or run (see [INTEGRATION.md](INTEGRATION.md) §1a).

## 2. Ownership contracts (locked)

| Concern | Owner | TinyRTCM3 |
|---------|-------|-----------|
| UART ISR / DMA / HW FIFO / baud | App or LC29H_GNSS pump | **No** |
| Software ring / FIFO storage & overwrite policy | App or shared util | **No** (app passes bytes) |
| Demux NMEA (`$`) vs RTCM (`0xD3`) | Transport pump | Prefers RTCM-only bytes; demux is not its job |
| Frame assemble, CRC-24Q, message type (DF002) | **TinyRTCM3** | Yes |
| Filter / allowlist / drop-MSM predicates | TinyRTCM3 Policy + app | Library supplies; app chooses |
| Decode/encode 1005/1006/1033/MSM summary | TinyRTCM3 Codec | Staged; stubs until goldens wired |
| Privacy rewrite of ARP / descriptors | TinyRTCM3 Sanitize | Before any public emit/golden |
| NTRIP TCP/TLS session | App | Boundary only (`NtripSink` stub) |
| Module survey-in / RTCM enable PAIR432/434/436 | LC29H_GNSS | **No** |

**Rule of thumb:** if it needs a pin, IRQ, or Quectel sentence, it is not TinyRTCM3.

## 3. Byte & frame contracts

### 3.1 Inbound (`Hub::feed` / `FrameAssembler::feed`)

- **Input:** opaque `uint8_t` stream, ideally already demuxed RTCM.
- **Output statuses:** `NeedMore` | `Ok` (frame completed; may have been filter-dropped) | `BadCrc` | `Overflow`
- **Completed frame:** `[0xD3 | len | payload | crc24]` with valid CRC-24Q.
- **Message identity:** first 12 bits of payload = DF002 type (`messageType()`). No external framing between messages.
- **Threading:** single consumer unless the app serializes `feed()`.

### 3.2 Emit (`Hub::Emit`)

- Called only with **CRC-valid** complete frames.
- Callback must not re-enter `feed()` on the same Hub (v0.2: **not** reentrant).
- Lifetime of `frame` pointer: only during the callback unless the sink copies.

### 3.3 Filter (`Hub::Filter`)

- Pure predicate on `(messageType, frame, len)`.
- Return `false` => drop; Hub still returns `Ok` (drop != error). See REQ-HUB-02.

### 3.4 Ring-buffer (app / pump side)

TinyRTCM3 does **not** allocate or own the ring. Recommended loop:

1. ISR/DMA -> ring (lock-free or critical section per platform).
2. Task pops bytes -> optional demux.
3. RTCM bytes -> `hub.feed(b)` until ring empty or budget exhausted.
4. Never block ISR on CRC or decode.

## 4. How RTCM messages are differentiated

RTCM 3 is **self-describing**:

1. Preamble `0xD3`
2. 10-bit payload length
3. Payload; bits 0-11 = message number (1005, 1074, 1077, ...)
4. CRC-24Q over preamble+length+payload

Helpers: `FrameAssembler`, `frameCrcOk`, `messageType`, Policy MSM/station predicates, Registry (stub).

NMEA remains a **separate** identification path (`$` + checksum). Do not overload TinyRTCM3 for NMEA.

## 5. Related docs

- [ICD.md](ICD.md) — CAP/REQ catalog and API surface
- [INTEGRATION.md](INTEGRATION.md) — **scope of change** vs LC29H_GNSS / apps (phased)
- [RTCM_CAPTURE_EVAL.md](RTCM_CAPTURE_EVAL.md) — which field goldens back the contracts
- [PRIVACY.md](PRIVACY.md) — 1005/1006 ARP handling
