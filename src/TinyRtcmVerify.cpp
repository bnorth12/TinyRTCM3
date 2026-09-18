#include "TinyRtcmVerify.h"
#include "TinyRtcmCrc24q.h"
#include "TinyRtcmFrameAssembler.h"
#include "TinyRtcmCodec.h"

namespace tinyrtcm3 {
namespace {

void emit(VerifyLogFn log, void* user, const char* line) {
  if (log != nullptr) {
    log(line, user);
  }
}

void pass(VerifyReport* r, VerifyLogFn log, void* user, const char* msg) {
  ++r->passed;
  emit(log, user, msg);
}

void fail(VerifyReport* r, VerifyLogFn log, void* user, const char* msg) {
  ++r->failed;
  emit(log, user, msg);
}

}  // namespace

int runSelfTests(VerifyReport* report, VerifyLogFn log, void* user) {
  VerifyReport local;
  VerifyReport* r = report != nullptr ? report : &local;
  r->passed = 0;
  r->failed = 0;
  r->skipped = 0;

  emit(log, user, "TinyRTCM3 self-test begin");

  // REQ matrix vs kCap*
  {
    const Requirement* crc01 = findRequirement("REQ-CRC-01");
    const Requirement* crc03 = findRequirement("REQ-CRC-03");
    const Requirement* cod01 = findRequirement("REQ-COD-01");
    const bool ok = crc01 != nullptr && crc01->implemented == kCapCrc24qCompute &&
                    crc03 != nullptr && crc03->implemented == kCapCrc24qAppend &&
                    cod01 != nullptr && cod01->implemented == kCapDecode1005 &&
                    kCapCrc24qCompute && kCapCrc24qAppend && !kCapDecode1005;
    if (ok) {
      pass(r, log, user, "ok REQ matrix vs kCap*");
    } else {
      fail(r, log, user, "FAIL REQ matrix vs kCap*");
    }
  }

  // CRC + assembler
  {
    uint8_t frame[16] = {};
    size_t n = 0;
    const Status fin = finalizeFrame(nullptr, 0, frame, sizeof(frame), &n);
    if (fin == Status::Ok && n == 6 && frameCrcOk(frame, 6) && frame[0] == 0xD3) {
      pass(r, log, user, "ok REQ-CRC-03 finalize empty");
    } else {
      fail(r, log, user, "FAIL REQ-CRC-03 finalize empty");
    }

    const uint8_t payload[3] = {0x40, 0x00, 0x00};
    uint8_t framed[32] = {};
    size_t n2 = 0;
    if (finalizeFrame(payload, sizeof(payload), framed, sizeof(framed), &n2) == Status::Ok &&
        n2 == 9 && frameCrcOk(framed, n2)) {
      pass(r, log, user, "ok REQ-CRC-03 finalize payload");
    } else {
      fail(r, log, user, "FAIL REQ-CRC-03 finalize payload");
    }

    uint8_t bad[6];
    for (size_t i = 0; i < 6; ++i) {
      bad[i] = frame[i];
    }
    bad[5] = static_cast<uint8_t>(bad[5] ^ 0x01u);
    if (!frameCrcOk(bad, 6)) {
      pass(r, log, user, "ok REQ-CRC-02 reject corrupt");
    } else {
      fail(r, log, user, "FAIL REQ-CRC-02 reject corrupt");
    }

    FrameAssembler asmblr;
    uint8_t out[32];
    size_t outLen = 0;
    FrameView view;
    Status st = Status::NeedMore;
    for (size_t i = 0; i < 6; ++i) {
      st = asmblr.feed(frame[i], out, sizeof(out), &outLen, &view);
    }
    if (st == Status::Ok && outLen == 6) {
      pass(r, log, user, "ok REQ-ASM-01 assemble good");
    } else {
      fail(r, log, user, "FAIL REQ-ASM-01 assemble good");
    }

    asmblr.reset();
    st = Status::NeedMore;
    for (size_t i = 0; i < 6; ++i) {
      st = asmblr.feed(bad[i], out, sizeof(out), &outLen, &view);
    }
    if (st == Status::BadCrc) {
      pass(r, log, user, "ok REQ-ASM-01 BadCrc");
    } else {
      fail(r, log, user, "FAIL REQ-ASM-01 BadCrc");
    }
  }

  // Codec stubs
  {
    Msg1005 m1005;
    Msg1033 m1033;
    MsmHeaderCnrSummary msm;
    uint8_t buf[64];
    size_t n = 0;
    const bool stubOk =
        decode1005(buf, 0, &m1005) == Status::Unsupported &&
        encode1005(m1005, buf, sizeof(buf), &n) == Status::Unsupported &&
        encode1033(m1033, buf, sizeof(buf), &n) == Status::Unsupported &&
        summarizeMsmCnr(buf, 0, &msm) == Status::Unsupported;
    if (stubOk) {
      ++r->skipped;
      emit(log, user, "skip REQ-COD-* still Unsupported (expected @ v0.1)");
    } else {
      fail(r, log, user, "FAIL REQ-COD-* unexpected non-stub behavior");
    }
  }

  emit(log, user, "TinyRTCM3 self-test end");
  return r->failed;
}

}  // namespace tinyrtcm3
