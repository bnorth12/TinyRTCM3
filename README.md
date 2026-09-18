# TinyRTCM3

**Solid library gate:** [docs/SOLID_LIBRARY.md](docs/SOLID_LIBRARY.md) (v0.5). CRC evaluate+generate is foundational (table-accelerated by default).

Compact **RTCM 3** helper for Arduino / PlatformIO (ESP32 RTK pipelines).

Stack with Quectel LC29H: `LC29H UART → TinyRTCM3 → app policy → NTRIP`.  
This library is **not** a Quectel config driver — keep that in [LC29H_GNSS-Library](https://github.com/bnorth12/LC29H_GNSS-Library).

**Optional peer:** usable with [LC29H_GNSS](https://github.com/bnorth12/LC29H_GNSS-Library), never required by it.

**Plan:** [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md).

**Contracts:** [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) (ownership) · [docs/INTEGRATION.md](docs/INTEGRATION.md) (phased app/GNSS impact) · [docs/ICD.md](docs/ICD.md).

## Status (v0.5.0 scaffold)

| Piece | Status |
|-------|--------|
| CRC-24Q + frame assembler | Implemented |
| Passthrough hub | Implemented |
| Bit buffer (write) | Stub |
| 1005 decode / 1033 encode / MSM CNR | **implemented (see ICD)** — next milestone |
| Synthetic goldens | CI contract (`test/golden/synthetic/`) |
| Field goldens | Optional; **must be sanitized** — see [docs/PRIVACY.md](docs/PRIVACY.md) |

## Layout

```
src/                 headers + codec stubs
examples/PassThroughHub/
test/golden/synthetic/   CI fixtures (safe to publish)
test/golden/field/       sanitized soak only (empty until you add)
test/host/               native CRC/assembler smoke
scripts/                 gen_synthetic_goldens.py, sanitize_rtcm_location.py
docs/                    ARCHITECTURE, INTEGRATION, ICD, CAPTURE, PRIVACY
raw/                     local captures only (gitignored)
```

## Privacy (do not skip)

Raw base captures are **not anonymous**: **1005/1006 contain ECEF ARP**. Never commit `raw/` or unsanitized QGNSS logs. Public field goldens only after `scripts/sanitize_rtcm_location.py`. Details: [docs/PRIVACY.md](docs/PRIVACY.md), capture plan: [docs/CAPTURE.md](docs/CAPTURE.md).

## Quick use

```cpp
#include <TinyRTCM3.h>
using namespace tinyrtcm3;
Hub hub;
// hub.setEmit(...); then hub.feed(byte);
```

## Generate synthetic goldens

```bash
python scripts/gen_synthetic_goldens.py
```

## License

MIT — Copyright 2026 Brian

## Requirements & self-test

Capability matrix: src/TinyRtcmRequirements.h (see [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md)).

`cpp
#include <TinyRTCM3.h>
tinyrtcm3::VerifyReport report;
int failed = tinyrtcm3::runSelfTests(&report);  // 0 = ok
`

Host: 	est/host/test_self_tests.cpp (GitHub Actions host-verify). Device: examples/SelfTest.

## ICD / requirements

- [docs/ICD.md](docs/ICD.md) — proposed functional scope and interfaces
- [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md) — capability → requirement map
- `src/TinyRtcmRequirements.h` — normative CAP/REQ catalogs for CI and self-test

