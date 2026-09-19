# Arduino Library Manager publish (TinyRTCM3)

**Authorizer:** RTK GNSS. **Executor:** Grok Build (evidence + packaging); publish PR to registry is human/authorizer gated.

## Gate — do not publish until
1. Host CI / goldens green on the tagged SHA.
2. Base Station HIL evidence attached (see Base Station `docs/TESTING_AND_TELEMETRY.md`):
   - T0.1 `rtcm.engine=="tiny"`
   - T0.2 CRC soak (`ok` rising, `crcFail` flat)
   - T0.3 type mix (1005 + MSM)
   - T1.3 / T1.4 ARP + MSM CNR strip when fixed
3. Optional A/B: legacy vs Tiny same stream (T0.4) if still linked.
4. README / SOLID_LIBRARY / ICD match the tagged API (no “stub” claims for shipped codecs).
5. `library.properties` + `library.json` version == git tag (semver, no `v` prefix in properties).

## Baseline + check-in
1. Clean tree (no capture WIP, no `_grok_*.log` in commit).
2. Bump `library.properties` / `library.json` version if needed.
3. Commit with evidence note (SHA of Base Station soak or link to `docs/test-artifacts`).
4. Tag `X.Y.Z` on `bnorth12/TinyRTCM3` and push tag.
5. GitHub Release for that tag (notes: CRC/assembler/codec scope; privacy/sanitize reminder).

## Arduino Library Manager
1. Confirm packaging: root `library.properties`, `src/`, `examples/`, MIT `LICENSE`, `keywords.txt`.
2. First-time: open a PR against [arduino/library-registry](https://github.com/arduino/library-registry) adding:
   `https://github.com/bnorth12/TinyRTCM3.git`
3. Later versions: push a new git tag; Library Manager picks it up after registry indexing (hours).
4. Verify in Arduino IDE / `arduino-cli lib search TinyRTCM3`.

## Consumers
- Base Station should prefer Library Manager / PlatformIO registry once published; keep optional peer (never required by LC29H_GNSS).
- Document the minimum Tiny version in Base Station README when cutting 0.3.0.

## Related libs (same path)
- [LC29H_GNSS-Library](https://github.com/bnorth12/LC29H_GNSS-Library) — already public; keep Tiny optional peer only.
- Base Station app is not an Arduino Library Manager package; it *depends* on the published libs.
