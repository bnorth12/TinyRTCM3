# Requirements & verification (v0.1)

Development process for TinyRTCM3:

1. **Requirements** live in `src/TinyRtcmRequirements.h` (`kRequirements[]`, `kCap*`).
2. **Verification** lives in `src/TinyRtcmVerify.{h,cpp}` (`runSelfTests`).
3. Host CI / `examples/SelfTest` call `runSelfTests` so capability checks stay in-tree.

## Status legend

| `implemented` | Meaning |
|---------------|---------|
| `true` | Self-test must pass for this REQ-ID |
| `false` | Codec (or other) stub; self-test expects `Status::Unsupported` (counted as skip) |

## Current matrix

See `kRequirements` in code — do not duplicate long tables here. Flip `kCap*` and the matching row when a feature lands, then extend `runSelfTests`.

## Limitations (v0.1)

- Hub / BitBuffer are not deeply exercised by `runSelfTests` yet (CRC + assembler + stub Codec only).
- No PlatformIO Unity suite beyond host `test/host`; Actions compiles `runSelfTests` on Ubuntu.
- Field goldens are out of CI (`kCapFieldGoldenCi = false`).
