# TinyRTCM3

Small standalone **Arduino / PlatformIO** helper for RTCM 3.x framing and selective encode/decode.

**Not** a full RTCM 3.2 stack. Kept separate from [LC29H_GNSS-Library](https://github.com/bnorth12/LC29H_GNSS-Library) (Quectel config / UART).

## Role in the stack

`LC29H UART → TinyRTCM3 (assemble / CRC / selective codec) → app policy → NTRIP`

- Library owns: framing, CRC-24Q, selective decode/encode, passthrough hub
- Apps own: RTK vs other policy (what to forward, rewrite, or drop)

## v1 scope (planned)

| Direction | Messages |
|-----------|----------|
| Decode | 1005, optional 1006; MSM4/7 **headers + CNR/quality summary** (listed MSM types) — no full observation cells |
| Encode | 1033; optional 1006 from ECEF; optional 1005 station-id rewrite — **never** MSM / 1230 |

## Status

Scaffold only (README + MIT). Implementation and synthetic goldens come next. Field QGNSS captures are optional soak tests, not the CI contract.

## License

MIT — see [LICENSE](LICENSE).
