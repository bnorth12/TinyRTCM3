# TinyRTCM3 — library attributes / publish readiness audit

**Date:** 2026-09-18 CT (BNLaptop)  
**Verdict:** Packaging scaffold is mostly ready; **truth/docs/CI/registry** are behind before Arduino Library Manager submit. Base Station already consumes the solid core (assembler+CRC+1005+MSM CNR).

## What is OK (shipped code)
| Attribute | Status |
|-----------|--------|
| `library.properties` required LM fields | Present (name/version/sentence/paragraph/category/url/architectures/includes) |
| `library.json` (PlatformIO) | Present |
| MIT `LICENSE` | Present |
| Umbrella `src/TinyRTCM3.h` | Present |
| Examples (3) | Present |
| Host CI (`host-verify.yml`) | Present (g++ self-tests) |
| Table CRC + FrameAssembler | Implemented; Base Station default path |
| Codec 1005/1006/1033 + MSM CNR summary | Implemented |
| Sanitize / IsoOnly / Hub batch / Stats histogram | Implemented (v0.5 solid gate) |
| Version constants | `kVersion* = 0.5.0` matches properties |

## Gaps before public Library Manager
| Gap | Severity | Action |
|-----|----------|--------|
| Not in [arduino/library-registry](https://github.com/arduino/library-registry) | Blocker for IDE discoverability | PR after HIL evidence + clean tag |
| No Arduino-cli **example compile** CI (SOLID P4) | High | Add workflow job compiling examples for esp32 |
| README / keywords still had stub-era wording | Medium | Sync to v0.5 truth (in progress this audit) |
| Author/maintainer lacked email form used by LC29H | Low | Fixed to noreply GitHub email |
| Dirty tree (`_grok_v01_*`, uncommitted capture scripts) | Medium | Exclude from release commit |
| CAP-REGISTRY / INtripSink still stubs | OK / document | Keep as intentional non-goals; README must say so clearly |
| Full MSM obs encode/decode | Out of scope v1 | Document non-goal |
| GitHub Release + annotated tag for 0.5.0 | Needed for LM updates | After HIL evidence |
| Base Station HIL T0/T1 evidence archived | Blocker for “verified” claim | Run soaks; store under Base Station `docs/test-artifacts/` |
| README still says “Status (v0.5.0 scaffold)” | Medium | Rewrite status table to solid |

## Base Station dependency map (must keep working)
- `FrameAssembler::feed` + BadCrc/Ok
- `decode1005`
- `summarizeMsmCnr`
- Table CRC path (default)

Optional later (Onocoy / P3): `encode1033`, sanitize rewrite on export mirrors only.

## Suggested order
1. Finish Base Station 0.2.59 multi-slot + HIL T0/T1 evidence.
2. Truth-sync README + ICD leftover stub language; add Arduino example CI.
3. Clean commit + tag `0.5.0` (or `0.5.1` if docs/CI-only bump) + GitHub Release.
4. Registry PR → Library Manager.
5. Cut Base Station **0.3.0** pinning min Tiny version from Library Manager.
