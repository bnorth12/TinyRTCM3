# Requirements & capabilities

**Normative machine-readable source:** src/TinyRtcmRequirements.h  
**Interface prose:** [ICD.md](ICD.md)  
**Solid gate:** [SOLID_LIBRARY.md](SOLID_LIBRARY.md)

## Rule

Every **capability** (CAP-*) documents one or more **requirements** (REQ-*)
via Capability.reqIds. Do not add a capability without requirements.

Library version: **0.5.0** (kVersion* in the header).

| Capability | Implemented | Notes |
|------------|-------------|-------|
| CAP-CRC-24Q | yes | evaluate+generate foundational |
| CAP-CRC-TABLE | yes | default path; bit path for identity tests |
| CAP-FRAME-ASM | yes | |
| CAP-BIT-WRITE / CAP-BIT-READ | yes | |
| CAP-HUB | yes | + batch feed |
| CAP-POLICY | yes | + ISO-only |
| CAP-REGISTRY | yes | not wired into Hub (deferred) |
| CAP-STATS | yes | + type histogram |
| CAP-CODEC-1005/1006/1033 | yes | + rewrite1006 |
| CAP-CODEC-MSM | yes | MSM4/7 summary only |
| CAP-SANITIZE | yes | + sanitizeBeforeEmit |
| CAP-GOLDENS / CAP-SELFTEST | yes | |
| CAP-NTRIP-BOUNDARY | yes | sink stub only |

Shall-statements live in kRequirements[]. Flip met / implemented in the same commit as code.

Ephemeris and Hub↔Registry wiring are **deferred**. Peer CRC duplicates stay until product integration.
