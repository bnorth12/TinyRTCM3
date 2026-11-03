#include "TinyRtcmVerify.h"
#include "TinyRtcmRequirements.h"
#include "TinyRtcmCrc24q.h"
#include "TinyRtcmFrameAssembler.h"
#include "TinyRtcmBitBuffer.h"
#include "TinyRtcmCodec.h"
#include "TinyRtcmSanitize.h"
#include "TinyRtcmPolicy.h"
#include "TinyRtcmRegistry.h"
#include "TinyRtcmStats.h"
#include "TinyRtcmHub.h"
#include "TinyRtcmFrameQueue.h"
#include "TinyRtcmDecode.h"

namespace tinyrtcm3 {
namespace {

void emit(VerifyLogFn log, void* user, const char* line) {
  if (log) log(line, user);
}
void pass(VerifyReport* r, VerifyLogFn log, void* user, const char* msg) {
  ++r->passed;
  emit(log, user, msg);
}
void fail(VerifyReport* r, VerifyLogFn log, void* user, const char* msg) {
  ++r->failed;
  emit(log, user, msg);
}
void skip(VerifyReport* r, VerifyLogFn log, void* user, const char* msg) {
  ++r->skipped;
  emit(log, user, msg);
}

}  // namespace

int runSelfTests(VerifyReport* report, VerifyLogFn log, void* user) {
  VerifyReport local;
  VerifyReport* r = report ? report : &local;
  r->passed = r->failed = r->skipped = 0;
  emit(log, user, "TinyRTCM3 self-test begin");

  // CAP/REQ catalog sanity: each capability lists at least one known REQ
  {
    bool ok = kCapabilityCount > 0 && kRequirementCount > 0;
    for (size_t i = 0; ok && i < kCapabilityCount; ++i) {
      const Capability& c = kCapabilities[i];
      if (c.reqIds == nullptr || c.reqIds[0] == nullptr) ok = false;
      else if (findRequirement(c.reqIds[0]) == nullptr) ok = false;
    }
    const Capability* crc = findCapability("CAP-CRC-24Q");
    const Requirement* rcrc = findRequirement("REQ-CRC-01");
    const Capability* bitr = findCapability("CAP-BIT-READ");
    const Requirement* rbit = findRequirement("REQ-BIT-02");
    const Requirement* r1005d = findRequirement("REQ-COD-1005-D");
    ok = ok && crc && crc->implemented && rcrc && rcrc->met;
    ok = ok && bitr && bitr->implemented && rbit && rbit->met;
    ok = ok && r1005d && r1005d->met;
    if (ok) pass(r, log, user, "ok CAP/REQ catalog linkage");
    else fail(r, log, user, "FAIL CAP/REQ catalog linkage");
  }

  // CAP-CRC-24Q + CAP-FRAME-ASM
  {
    uint8_t frame[16] = {};
    size_t n = 0;
    if (finalizeFrame(nullptr, 0, frame, sizeof(frame), &n) == Status::Ok && n == 6 &&
        frameCrcOk(frame, 6)) {
      pass(r, log, user, "ok CAP-CRC-24Q finalize empty");
    } else {
      fail(r, log, user, "FAIL CAP-CRC-24Q finalize empty");
    }

    uint8_t bad[6];
    for (size_t i = 0; i < 6; ++i) bad[i] = frame[i];
    bad[5] ^= 0x01;
    if (!frameCrcOk(bad, 6)) pass(r, log, user, "ok CAP-CRC-24Q reject corrupt");
    else fail(r, log, user, "FAIL CAP-CRC-24Q reject corrupt");

    FrameAssembler asmblr;
    uint8_t out[32];
    size_t outLen = 0;
    FrameView view;
    Status st = Status::NeedMore;
    for (size_t i = 0; i < 6; ++i) st = asmblr.feed(frame[i], out, sizeof(out), &outLen, &view);
    if (st == Status::Ok) pass(r, log, user, "ok CAP-FRAME-ASM good frame");
    else fail(r, log, user, "FAIL CAP-FRAME-ASM good frame");

    asmblr.reset();
    st = Status::NeedMore;
    for (size_t i = 0; i < 6; ++i) st = asmblr.feed(bad[i], out, sizeof(out), &outLen, &view);
    if (st == Status::BadCrc) pass(r, log, user, "ok CAP-FRAME-ASM BadCrc");
    else fail(r, log, user, "FAIL CAP-FRAME-ASM BadCrc");
  }

  // CAP-BIT-READ: putBits ↔ getBits round-trip (REQ-BIT-02)
  {
    uint8_t buf[8] = {};
    BitBuffer bb(buf, sizeof(buf));
    bb.resetWrite();
    uint32_t got = 0xFFFFFFFFu;
    const bool ok = bb.putBits(0xABCu, 12) == Status::Ok &&
                    bb.putBits(0x5Au, 8) == Status::Ok &&
                    bb.putBits(1u, 1) == Status::Ok &&
                    (bb.resetRead(), true) && bb.getBits(12, &got) == Status::Ok &&
                    got == 0xABCu && bb.getBits(8, &got) == Status::Ok && got == 0x5Au &&
                    bb.getBits(1, &got) == Status::Ok && got == 1u;
    if (ok) pass(r, log, user, "ok CAP-BIT-READ put/get round-trip");
    else fail(r, log, user, "FAIL CAP-BIT-READ put/get round-trip");

    uint32_t unused = 0;
    const bool args = bb.getBits(0, &unused) == Status::InvalidArg &&
                      bb.getBits(33, &unused) == Status::InvalidArg &&
                      bb.getBits(8, nullptr) == Status::InvalidArg;
    BitBuffer empty(nullptr, 0);
    const bool ovf = empty.getBits(1, &unused) == Status::InvalidArg;
    uint8_t tiny[1] = {0xFF};
    BitBuffer shortBb(tiny, 1);
    shortBb.resetRead();
    const bool past = shortBb.getBits(8, &unused) == Status::Ok &&
                      shortBb.getBits(1, &unused) == Status::Overflow;
    if (args && ovf && past) pass(r, log, user, "ok CAP-BIT-READ InvalidArg/Overflow");
    else fail(r, log, user, "FAIL CAP-BIT-READ InvalidArg/Overflow");
  }

  // REQ-COD-1005-D: construct dummy 1005 via putBits and decode
  {
    uint8_t payload[19] = {};
    BitBuffer bb(payload, sizeof(payload));
    bb.resetWrite();
    auto put38 = [&bb](int64_t v) -> bool {
      const uint64_t u = static_cast<uint64_t>(v) & ((1ULL << 38) - 1ULL);
      return bb.putBits(static_cast<uint32_t>(u >> 32), 6) == Status::Ok &&
             bb.putBits(static_cast<uint32_t>(u), 32) == Status::Ok;
    };
    bool pok = bb.putBits(1005, 12) == Status::Ok &&
               bb.putBits(kPublishStationId, 12) == Status::Ok &&
               bb.putBits(0, 6) == Status::Ok && bb.putBits(1, 1) == Status::Ok &&
               bb.putBits(0, 1) == Status::Ok && bb.putBits(0, 1) == Status::Ok &&
               bb.putBits(0, 1) == Status::Ok && put38(kPublishArpEcef01mmX) &&
               bb.putBits(0, 1) == Status::Ok && bb.putBits(0, 1) == Status::Ok &&
               put38(kPublishArpEcef01mmY) && bb.putBits(0, 2) == Status::Ok &&
               put38(kPublishArpEcef01mmZ) && bb.bitLength() == 152;
    uint8_t frame[32] = {};
    size_t n = 0;
    Msg1005 m;
    pok = pok && finalizeFrame(payload, sizeof(payload), frame, sizeof(frame), &n) == Status::Ok &&
          decode1005(frame, n, &m) == Status::Ok && m.stationId == kPublishStationId &&
          m.ecefX01mm == kPublishArpEcef01mmX && m.ecefY01mm == kPublishArpEcef01mmY &&
          m.ecefZ01mm == kPublishArpEcef01mmZ;
    if (pok) pass(r, log, user, "ok REQ-COD-1005-D decode dummy ARP");
    else fail(r, log, user, "FAIL REQ-COD-1005-D decode dummy ARP");
  }

  // v0.2: encode1005 + rewrite + sanitize 1005
  {
    Msg1005 m;
    m.stationId = 42;
    m.ecefX01mm = kPublishArpEcef01mmX;
    m.ecefY01mm = -1;
    m.ecefZ01mm = kPublishArpEcef01mmZ;
    uint8_t frame[64];
    size_t n = 0;
    Msg1005 back;
    bool ok = encode1005(m, frame, sizeof(frame), &n) == Status::Ok && n >= 6 &&
              decode1005(frame, n, &back) == Status::Ok && back.stationId == 42 &&
              back.ecefX01mm == m.ecefX01mm && back.ecefY01mm == -1 &&
              back.ecefZ01mm == m.ecefZ01mm;
    if (ok) pass(r, log, user, "ok REQ-COD-1005-E encode/decode round-trip");
    else fail(r, log, user, "FAIL REQ-COD-1005-E encode/decode round-trip");

    Msg1005 dirty = m;
    dirty.stationId = 99;
    dirty.ecefX01mm = 123456789LL;
    ok = encode1005(dirty, frame, sizeof(frame), &n) == Status::Ok;
    uint8_t out[64];
    size_t on = 0;
    Msg1005 pub;
    ok = ok && rewrite1005ToPublishIdentity(frame, n, out, sizeof(out), &on) == Status::Ok &&
         decode1005(out, on, &pub) == Status::Ok && pub.stationId == kPublishStationId &&
         pub.ecefX01mm == kPublishArpEcef01mmX && pub.ecefY01mm == kPublishArpEcef01mmY &&
         pub.ecefZ01mm == kPublishArpEcef01mmZ;
    if (ok) pass(r, log, user, "ok REQ-COD-1005-R rewrite to publish identity");
    else fail(r, log, user, "FAIL REQ-COD-1005-R rewrite to publish identity");

    size_t sn = 0;
    ok = sanitizeRewriteLocationFrame(frame, n, sizeof(frame), &sn) == Status::Ok &&
         decode1005(frame, sn, &pub) == Status::Ok && pub.stationId == kPublishStationId;
    if (ok) pass(r, log, user, "ok REQ-SAN-02 sanitize rewrite 1005");
    else fail(r, log, user, "FAIL REQ-SAN-02 sanitize rewrite 1005");
  }

  // Policy + registry + stats smoke
  {
    const bool pol = policyIsStationIdentityType(1005) && policyIsMsmType(1077) &&
                     policyFilterKeepStationAndMsm(1005, nullptr, 0, nullptr) &&
                     !policyFilterDropMsm(1077, nullptr, 0, nullptr);
    if (pol) pass(r, log, user, "ok CAP-POLICY stock predicates");
    else fail(r, log, user, "FAIL CAP-POLICY stock predicates");

    MessageRegistry reg;
    const RegistryEntry table[] = {{1005, nullptr}};
    reg.setTable(table, 1);
    const bool regOk = reg.dispatch(9999, nullptr, 0, nullptr) == Status::Unsupported;
    if (regOk) pass(r, log, user, "ok CAP-REGISTRY unknown -> Unsupported");
    else fail(r, log, user, "FAIL CAP-REGISTRY unknown");

    StreamStats s;
    statsOnByte(&s);
    statsOnByte(&s);
    statsOnStatus(&s, Status::Ok, false);
    statsOnStatus(&s, Status::Ok, true);
    statsOnStatus(&s, Status::BadCrc, false);
    const bool stOk = s.bytesIn == 2 && s.framesOk == 1 && s.framesDroppedByFilter == 1 &&
                      s.framesBadCrc == 1;
    statsReset(&s);
    const bool resetOk = s.bytesIn == 0 && s.framesOk == 0;
    if (stOk && resetOk) pass(r, log, user, "ok CAP-STATS counters + reset");
    else fail(r, log, user, "FAIL CAP-STATS");
  }

  // v0.3: 1006 / 1033 encode-decode + MSM still Unsupported
  {
    Msg1006 m6;
    m6.stationId = 7;
    m6.ecefX01mm = kPublishArpEcef01mmX;
    m6.ecefY01mm = -2;
    m6.ecefZ01mm = kPublishArpEcef01mmZ;
    m6.antennaHeight01mm = 15000;
    uint8_t frame[64];
    size_t n = 0;
    Msg1006 back;
    bool ok = encode1006(m6, frame, sizeof(frame), &n) == Status::Ok && n == 27 &&
              decode1006(frame, n, &back) == Status::Ok && back.stationId == 7 &&
              back.ecefX01mm == m6.ecefX01mm && back.ecefY01mm == -2 &&
              back.ecefZ01mm == m6.ecefZ01mm && back.antennaHeight01mm == 15000;
    if (ok) pass(r, log, user, "ok REQ-COD-1006-E/D encode/decode round-trip");
    else fail(r, log, user, "FAIL REQ-COD-1006-E/D encode/decode round-trip");

    Msg1033 m3;
    m3.stationId = 3;
    m3.antennaDescriptor[0] = 'A';
    m3.antennaDescriptor[1] = 'N';
    m3.antennaDescriptor[2] = 'T';
    m3.receiverDescriptor[0] = 'R';
    m3.receiverDescriptor[1] = 'C';
    m3.receiverDescriptor[2] = 'V';
    Msg1033 b3;
    ok = encode1033(m3, frame, sizeof(frame), &n) == Status::Ok && n >= 6 &&
         decode1033(frame, n, &b3) == Status::Ok && b3.stationId == 3 &&
         b3.antennaDescriptor[0] == 'A' && b3.antennaDescriptor[1] == 'N' &&
         b3.antennaDescriptor[2] == 'T' && b3.receiverDescriptor[0] == 'R' &&
         b3.receiverDescriptor[1] == 'C' && b3.receiverDescriptor[2] == 'V';
    if (ok) pass(r, log, user, "ok REQ-COD-1033-E/D encode/decode round-trip");
    else fail(r, log, user, "FAIL REQ-COD-1033-E/D encode/decode round-trip");

    // Minimal GPS MSM4: 2 sats, 2 sigs, 3 cells, CNRs 40/50/45 -> mean 450
    uint8_t msmPayload[64] = {};
    BitBuffer mb(msmPayload, sizeof(msmPayload));
    mb.resetWrite();
    auto putN = [&mb](uint32_t v, uint8_t n) -> bool { return mb.putBits(v, n) == Status::Ok; };
    bool mok = putN(1074, 12) && putN(0, 12) && putN(0, 30) && putN(0, 19) &&
               putN(0xC0000000u, 32) && putN(0, 32) && putN(0xC0000000u, 32) &&
               putN(0xDu, 4) && putN(0, 18) && putN(0, 18);
    const uint32_t cnrs[3] = {40u, 50u, 45u};
    for (int ci = 0; ci < 3; ++ci) {
      mok = mok && putN(0, 32) && putN(0, 10) && putN(cnrs[ci], 6);
    }
    uint8_t msmFrame[96];
    size_t mn = 0;
    MsmHeaderCnrSummary msm;
    mok = mok && finalizeFrame(msmPayload, mb.byteLength(), msmFrame, sizeof(msmFrame), &mn) ==
                     Status::Ok &&
          summarizeMsmCnr(msmFrame, mn, &msm) == Status::Ok && msm.messageType == 1074 &&
          msm.satCount == 2 && msm.sigCount == 2 && msm.meanCnr01dBHz == 450;
    if (mok) pass(r, log, user, "ok REQ-COD-MSM-S MSM4 mean CNR");
    else fail(r, log, user, "FAIL REQ-COD-MSM-S MSM4 mean CNR");
  }


  // v0.5 solid: table CRC identity, IsoOnly, batch feed, histogram, rewrite1006
  {
    uint8_t buf[64];
    for (int i = 0; i < 64; ++i) buf[i] = static_cast<uint8_t>(i * 37u);
    bool ok = true;
    for (size_t n = 0; n <= 64; n += 3) {
      if (crc24qBit(buf, n) != crc24qTable(buf, n)) ok = false;
    }
    uint8_t frame[16];
    size_t fl = 0;
    ok = ok && finalizeFrame(nullptr, 0, frame, sizeof(frame), &fl) == Status::Ok &&
         frameCrcOk(frame, fl) && crc24qBit(frame, fl - 3) == crc24qTable(frame, fl - 3);
    if (ok) pass(r, log, user, "ok REQ-VER-03 / CAP-CRC-TABLE bit-identical");
    else fail(r, log, user, "FAIL REQ-VER-03 table vs bit CRC");

    ok = policyFilterIsoOnly(1005, nullptr, 0, nullptr) &&
         policyFilterIsoOnly(1006, nullptr, 0, nullptr) &&
         policyFilterIsoOnly(1033, nullptr, 0, nullptr) &&
         !policyFilterIsoOnly(1074, nullptr, 0, nullptr);
    if (ok) pass(r, log, user, "ok REQ-POL-03 ISO-only filter");
    else fail(r, log, user, "FAIL REQ-POL-03");

    Hub hub;
    StreamStats st{};
    hub.setStats(&st);
    size_t consumed = 0;
    Status last = hub.feed(frame, fl, &consumed);
    ok = consumed == fl && last == Status::Ok && st.bytesIn == fl && st.framesOk == 1;
    if (ok) pass(r, log, user, "ok REQ-HUB-03 batch feed + stats");
    else fail(r, log, user, "FAIL REQ-HUB-03 batch feed");

    Msg1006 m6;
    m6.stationId = 9;
    m6.ecefX01mm = 123;
    m6.ecefY01mm = 456;
    m6.ecefZ01mm = 789;
    m6.antennaHeight01mm = 100;
    uint8_t f6[64];
    size_t n6 = 0;
    Msg1006 back;
    ok = encode1006(m6, f6, sizeof(f6), &n6) == Status::Ok &&
         rewrite1006ToPublishIdentity(f6, n6, f6, sizeof(f6), &n6) == Status::Ok &&
         decode1006(f6, n6, &back) == Status::Ok && back.stationId == kPublishStationId &&
         back.ecefX01mm == kPublishArpEcef01mmX && back.antennaHeight01mm == 100;
    if (ok) pass(r, log, user, "ok REQ-COD-1006-R rewrite1006");
    else fail(r, log, user, "FAIL REQ-COD-1006-R");

    Msg1005 m5;
    m5.stationId = 1;
    m5.ecefX01mm = kPublishArpEcef01mmX;
    uint8_t f5[64];
    size_t n5 = 0;
    StreamStats hst{};
    Hub hub2;
    hub2.setStats(&hst);
    ok = encode1005(m5, f5, sizeof(f5), &n5) == Status::Ok &&
         hub2.feed(f5, n5, nullptr) == Status::Ok && hst.type1005 == 1;
    if (ok) pass(r, log, user, "ok REQ-STAT-03 type histogram");
    else fail(r, log, user, "FAIL REQ-STAT-03");
  }

  // v0.6 frame queue, type table, coverage, decode mask, 1230, MSM header
  {
    FrameQueue<3, 64> q;
    uint8_t a[16] = {};
    uint8_t b[16] = {};
    a[0] = 0xD3;
    a[3] = static_cast<uint8_t>(1033 >> 4);
    a[4] = static_cast<uint8_t>((1033 & 0x0F) << 4);
    for (int i = 0; i < 12; ++i) a[i + 0] = a[i];
    // length 12 frames
    bool ok = q.push(a, 12, 1033) && q.push(a, 12, 1008) && q.count() == 2;
    uint8_t dest[64];
    ok = ok && q.pull(dest, 4) == 0 && q.pull(dest, 12) == 12 && q.count() == 1;
    ok = ok && q.pull(dest, 12) == 12 && q.empty();
    if (ok) pass(r, log, user, "ok REQ-FQ-01 pull whole frame by writable");
    else fail(r, log, user, "FAIL REQ-FQ-01 pull");

    auto stamp = [](uint8_t* f, uint16_t type, uint32_t epoch) {
      for (int i = 0; i < 16; ++i) f[i] = 0;
      f[0] = 0xD3;
      f[3] = static_cast<uint8_t>(type >> 4);
      f[4] = static_cast<uint8_t>((type & 0x0F) << 4);
      f[6] = static_cast<uint8_t>((epoch >> 22) & 0xFF);
      f[7] = static_cast<uint8_t>((epoch >> 14) & 0xFF);
      f[8] = static_cast<uint8_t>((epoch >> 6) & 0xFF);
      f[9] = static_cast<uint8_t>((epoch & 0x3F) << 2);
    };
    FrameQueue<3, 64> qm;
    stamp(a, 1077, 100);
    stamp(b, 1087, 5);
    uint8_t c[16];
    stamp(c, 1097, 7);
    ok = qm.push(a, 12, 1077) && qm.push(b, 12, 1087) && qm.push(c, 12, 1097) && qm.count() == 3;
    ok = ok && !qm.push(a, 12, 1077) && qm.empty();
    ok = ok && qm.drops().byMsmEpoch >= 4;
    if (ok) pass(r, log, user, "ok REQ-FQ-02 drop whole MSM epoch");
    else fail(r, log, user, "FAIL REQ-FQ-02 epoch drop");

    FrameQueue<3, 64> qo;
    stamp(a, 1077, 1);
    stamp(b, 1087, 2);
    stamp(c, 1033, 0);
    ok = qo.push(a, 12, 1077) && qo.push(b, 12, 1087) && qo.push(c, 12, 1033);
    stamp(a, 1077, 99);
    ok = ok && qo.push(a, 12, 1077) && qo.count() == 2;
    ok = ok && qo.pull(dest, 12) == 12 && dest[3] == static_cast<uint8_t>(1033 >> 4);
    ok = ok && qo.pull(dest, 12) == 12 && messageType(dest, 12) == 1077;
    if (ok) pass(r, log, user, "ok REQ-FQ-02 keep closed epoch drop then new MSM");
    else fail(r, log, user, "FAIL REQ-FQ-02 older epoch");

    FrameQueue<2, 64> qp;
    stamp(a, 1077, 3);
    stamp(b, 1087, 4);
    ok = qp.push(a, 12, 1077) && qp.push(b, 12, 1087);
    Msg1005 m5;
    m5.stationId = 4;
    m5.ecefX01mm = kPublishArpEcef01mmX;
    uint8_t f5[64];
    size_t n5 = 0;
    ok = ok && encode1005(m5, f5, sizeof(f5), &n5) == Status::Ok && n5 <= 64;
    ok = ok && qp.push(f5, n5, 1005) && qp.count() == 1 && qp.pull(dest, 64) == n5;
    ok = ok && qp.push(f5, n5, 1005) && qp.push(f5, n5, 1005) && qp.count() == 1;
    ok = ok && qp.drops().byReplacedProtected == 1 && !qp.unhealthy();
    ok = ok && !qp.push(a, 80, 1077) && qp.drops().byOversize >= 1;
    qp.setConfig(FrameQueueConfig{4, 2});
    ok = ok && !qp.unhealthy();
    qp.push(a, 80, 1077);
    qp.push(a, 0, 1077);
    ok = ok && qp.unhealthy();
    qp.clearUnhealthy();
    ok = ok && !qp.unhealthy();
    if (ok) pass(r, log, user, "ok REQ-FQ-02/03 protected keep + unhealthy window");
    else fail(r, log, user, "FAIL REQ-FQ-02/03 protected");

    StreamStats ts;
    statsOnCrcPass(&ts, 1005, 20, 1000);
    statsOnCrcPass(&ts, 1005, 20, 2500);
    const TypeRow* row = statsFindTypeConst(&ts, 1005);
    ok = row != nullptr && row->count == 2 && row->maxGapMs == 1500 && row->bytesPerSec == 26;
    CoverageFlag flags[kOnocoyRequiredCount];
    coverageCheck(&ts, 2500, kDefaultLateAfterMs, flags, kOnocoyRequiredCount);
    ok = ok && flags[0].messageType == 1005 && !flags[0].missing && !flags[0].late;
    ok = ok && flags[1].messageType == 1033 && flags[1].missing;
    coverageCheck(&ts, 2500 + 20000, kDefaultLateAfterMs, flags, kOnocoyRequiredCount);
    ok = ok && !flags[0].missing && flags[0].late;
    coverageCheck(&ts, 0, kDefaultLateAfterMs, flags, kOnocoyRequiredCount);
    ok = ok && !flags[0].late;
    bool maskOff = true;
    for (size_t i = 0; i < sizeof(ts.decodeMask); ++i) {
      if (ts.decodeMask[i] != 0) maskOff = false;
    }
    decodeMaskSet(&ts, 1005, true);
    ok = ok && maskOff && decodeMaskGet(&ts, 1005) && !decodeMaskGet(&ts, 1006);
    DecodeView view;
    ok = ok && decodeIfEnabled(&ts, f5, n5, 1005, &view) == Status::Ok && view.has1005 &&
         view.arp1005.stationId == 4;
    decodeMaskSet(&ts, 1005, false);
    DecodeView view2;
    ok = ok && decodeIfEnabled(&ts, f5, n5, 1005, &view2) == Status::Ok && !view2.has1005;
    if (ok) pass(r, log, user, "ok REQ-STAT-04/05 REQ-DEC-01 type table coverage mask");
    else fail(r, log, user, "FAIL REQ-STAT-04/05 or REQ-DEC-01");

    uint8_t p1230[32] = {};
    BitBuffer b1230(p1230, sizeof(p1230));
    b1230.resetWrite();
    ok = b1230.putBits(1230, 12) == Status::Ok && b1230.putBits(9, 12) == Status::Ok &&
         b1230.putBits(1, 1) == Status::Ok && b1230.putBits(0, 3) == Status::Ok &&
         b1230.putBits(1, 1) == Status::Ok && b1230.putBits(0xFFFBu, 16) == Status::Ok &&
         b1230.putBits(0, 1) == Status::Ok && b1230.putBits(0, 1) == Status::Ok &&
         b1230.putBits(0, 1) == Status::Ok;
    uint8_t f1230[64];
    size_t n1230 = 0;
    Msg1230 g;
    ok = ok && finalizeFrame(p1230, b1230.byteLength(), f1230, sizeof(f1230), &n1230) == Status::Ok &&
         decode1230(f1230, n1230, &g) == Status::Ok && g.stationId == 9 && g.biasIndicator == 1 &&
         g.validMask == 0x1 && g.bias002m[0] == -5;
    if (ok) pass(r, log, user, "ok REQ-COD-1230-D decode");
    else fail(r, log, user, "FAIL REQ-COD-1230-D");

    uint8_t msmPayload[64] = {};
    BitBuffer mb(msmPayload, sizeof(msmPayload));
    mb.resetWrite();
    auto putN = [&mb](uint32_t v, uint8_t n) -> bool { return mb.putBits(v, n) == Status::Ok; };
    bool mok = putN(1137, 12) && putN(11, 12) && putN(0x15555u, 30) && putN(0, 19) &&
               putN(0x80000000u, 32) && putN(0, 32) && putN(0x80000000u, 32);
    uint8_t msmFrame[96];
    size_t mn = 0;
    MsmHeaderView mh;
    mok = mok && finalizeFrame(msmPayload, mb.byteLength(), msmFrame, sizeof(msmFrame), &mn) ==
                     Status::Ok &&
          decodeMsmHeader(msmFrame, mn, &mh) == Status::Ok && mh.messageType == 1137 &&
          mh.stationId == 11 && mh.epoch30 == 0x15555u && mh.satCount == 1 && mh.sigCount == 1;
    if (mok) pass(r, log, user, "ok REQ-COD-MSM-H header");
    else fail(r, log, user, "FAIL REQ-COD-MSM-H");
  }

  emit(log, user, "TinyRTCM3 self-test end");
  return r->failed;
}

}  // namespace tinyrtcm3
