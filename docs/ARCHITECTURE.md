# Architecture (v0.1)

```
UART bytes → FrameAssembler (CRC-24Q) → Hub (filter) → app / NTRIP
                              ↓
                         Codec (1005 / 1033 / MSM CNR summary)
```

| Piece | Role | v0.1 status |
|-------|------|-------------|
| `TinyRtcmCrc24q` | Transport CRC-24Q compute/verify/**append**/finalizeFrame | Implemented (bit-at-a-time; table accel later) |
| `FrameAssembler` | Stream → frames | Implemented |
| `BitBuffer` | MSB bit pack | Stub writer |
| `Hub` | Filter + emit | Implemented |
| `Codec` | 1005/1033/MSM | **Unsupported stubs** — next milestone |
| Synthetic goldens | CI contract | Generator script |
| Field goldens | Soak only | Sanitized or absent |

Stack with Quectel: **LC29H UART → TinyRTCM3 → app policy → NTRIP**. LC29H library stays config/UART only.

## ICD

Proposed scope, capability↔requirement linkage, and API contracts: [ICD.md](ICD.md). Checklist: [REQUIREMENTS.md](REQUIREMENTS.md) / `src/TinyRtcmRequirements.h`.

