# Requirements & capabilities

**Normative machine-readable source:** `src/TinyRtcmRequirements.h`  
**Interface prose:** [ICD.md](ICD.md)

## Rule

Every **capability** (`CAP-*`) documents one or more **requirements** (`REQ-*`)
via `Capability.reqIds`. Do not add a capability without requirements.

| Capability | Implemented (v0.1) | Requirements |
|------------|--------------------|--------------|
| CAP-CRC-24Q | yes | REQ-CRC-01..03 |
| CAP-CRC-TABLE | no | REQ-CRC-04 |
| CAP-FRAME-ASM | yes | REQ-ASM-01..03 |
| CAP-BIT-WRITE | yes | REQ-BIT-01 |
| CAP-BIT-READ | no (stub) | REQ-BIT-02 |
| CAP-HUB | yes | REQ-HUB-01..02 |
| CAP-POLICY | stub helpers | REQ-POL-01..02 |
| CAP-REGISTRY | stub | REQ-REG-01..02 |
| CAP-STATS | stub | REQ-STAT-01..02 |
| CAP-CODEC-1005 | stub | REQ-COD-1005-D/E/R |
| CAP-CODEC-1006 | stub | REQ-COD-1006-D/E |
| CAP-CODEC-1033 | stub | REQ-COD-1033-D/E |
| CAP-CODEC-MSM | stub | REQ-COD-MSM-S/X |
| CAP-SANITIZE | partial | REQ-SAN-01..02 |
| CAP-GOLDENS | yes (process) | REQ-GOLD-01..02 |
| CAP-SELFTEST | yes | REQ-VER-01..02 |
| CAP-NTRIP-BOUNDARY | documented | REQ-NTRIP-01 |

Shall-statements live in `kRequirements[]` (not duplicated here). When implementing:
update `met` / `implemented`, extend `runSelfTests`, goldens, and ICD together.

## Execution

Milestone sequencing and exit criteria: [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).
