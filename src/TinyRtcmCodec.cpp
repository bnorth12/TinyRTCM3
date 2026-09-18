#include "TinyRtcmCodec.h"
#include "TinyRtcmBitBuffer.h"
#include "TinyRtcmCrc24q.h"

namespace tinyrtcm3 {
namespace {

// RTCM 10403.x 1005 body is 152 bits / 19 bytes:
// DF002 12, DF003 12, DF021 6, DF022/023/024/141 (1 each), DF025 38,
// DF142 1, DF001 1, DF026 38, DF364 2, DF027 38.
static constexpr size_t kMsg1005PayloadBytes = 19;
static constexpr uint16_t kMsg1005Type = 1005;

Status getBits38Signed(BitBuffer& bb, int64_t* out) {
  uint32_t hi = 0;
  uint32_t lo = 0;
  Status st = bb.getBits(6, &hi);
  if (st != Status::Ok) return st;
  st = bb.getBits(32, &lo);
  if (st != Status::Ok) return st;
  const uint64_t raw = (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
  if (raw & (1ULL << 37)) {
    *out = static_cast<int64_t>(raw) - (1LL << 38);
  } else {
    *out = static_cast<int64_t>(raw);
  }
  return Status::Ok;
}

}  // namespace

Status decode1005(const uint8_t* frame, size_t len, Msg1005* out) {
  if (frame == nullptr || out == nullptr) return Status::InvalidArg;
  if (len < kRtcmMinFrameLen) return Status::InvalidArg;
  if (frame[0] != 0xD3) return Status::InvalidArg;
  const size_t payloadLen =
      (static_cast<size_t>(frame[1] & 0x03u) << 8) | static_cast<size_t>(frame[2]);
  if (len != payloadLen + 6u) return Status::InvalidArg;
  if (!frameCrcOk(frame, len)) return Status::BadCrc;
  if (messageType(frame, len) != kMsg1005Type) return Status::InvalidArg;
  if (payloadLen < kMsg1005PayloadBytes) return Status::Overflow;

  uint8_t payload[kMsg1005PayloadBytes];
  for (size_t i = 0; i < kMsg1005PayloadBytes; ++i) payload[i] = frame[3 + i];
  BitBuffer bb(payload, kMsg1005PayloadBytes);
  bb.resetRead();

  uint32_t msg = 0;
  uint32_t station = 0;
  uint32_t skip = 0;
  Status st = bb.getBits(12, &msg);
  if (st != Status::Ok) return st;
  if (msg != kMsg1005Type) return Status::InvalidArg;
  st = bb.getBits(12, &station);
  if (st != Status::Ok) return st;
  st = bb.getBits(6, &skip);  // DF021 ITRF year
  if (st != Status::Ok) return st;
  st = bb.getBits(4, &skip);  // GPS / GLO / GAL / ref-station
  if (st != Status::Ok) return st;

  int64_t x = 0;
  int64_t y = 0;
  int64_t z = 0;
  st = getBits38Signed(bb, &x);
  if (st != Status::Ok) return st;
  st = bb.getBits(1, &skip);  // DF142 oscillator
  if (st != Status::Ok) return st;
  st = bb.getBits(1, &skip);  // DF001 reserved
  if (st != Status::Ok) return st;
  st = getBits38Signed(bb, &y);
  if (st != Status::Ok) return st;
  st = bb.getBits(2, &skip);  // DF364 quarter-cycle / reserved
  if (st != Status::Ok) return st;
  st = getBits38Signed(bb, &z);
  if (st != Status::Ok) return st;

  out->stationId = static_cast<uint16_t>(station);
  out->ecefX01mm = x;
  out->ecefY01mm = y;
  out->ecefZ01mm = z;
  return Status::Ok;
}

Status encode1005(const Msg1005&, uint8_t*, size_t, size_t*) { return Status::Unsupported; }
Status rewrite1005ToPublishIdentity(const uint8_t*, size_t, uint8_t*, size_t, size_t*) {
  return Status::Unsupported;
}
Status decode1006(const uint8_t*, size_t, Msg1006*) { return Status::Unsupported; }
Status encode1006(const Msg1006&, uint8_t*, size_t, size_t*) { return Status::Unsupported; }
Status decode1033(const uint8_t*, size_t, Msg1033*) { return Status::Unsupported; }
Status encode1033(const Msg1033&, uint8_t*, size_t, size_t*) { return Status::Unsupported; }
Status summarizeMsmCnr(const uint8_t*, size_t, MsmHeaderCnrSummary*) {
  return Status::Unsupported;
}

}  // namespace tinyrtcm3
