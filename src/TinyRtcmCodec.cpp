#include "TinyRtcmCodec.h"
#include "TinyRtcmBitBuffer.h"
#include "TinyRtcmCrc24q.h"

namespace tinyrtcm3 {
namespace {

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

Status putBits38Signed(BitBuffer& bb, int64_t v) {
  const uint64_t u = static_cast<uint64_t>(v) & ((1ULL << 38) - 1ULL);
  Status st = bb.putBits(static_cast<uint32_t>(u >> 32), 6);
  if (st != Status::Ok) return st;
  return bb.putBits(static_cast<uint32_t>(u), 32);
}

Status put1005StyleBody(BitBuffer& bb, uint16_t msgType, uint16_t stationId, int64_t x, int64_t y,
                        int64_t z) {
  Status st = bb.putBits(msgType, 12);
  if (st != Status::Ok) return st;
  st = bb.putBits(stationId, 12);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 6);
  if (st != Status::Ok) return st;
  st = bb.putBits(1, 1);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);
  if (st != Status::Ok) return st;
  st = putBits38Signed(bb, x);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);
  if (st != Status::Ok) return st;
  st = putBits38Signed(bb, y);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 2);
  if (st != Status::Ok) return st;
  return putBits38Signed(bb, z);
}

Status get1005StyleBody(BitBuffer& bb, uint16_t expectType, uint16_t* station, int64_t* x, int64_t* y,
                        int64_t* z) {
  uint32_t msg = 0, stn = 0, skip = 0;
  Status st = bb.getBits(12, &msg);
  if (st != Status::Ok) return st;
  if (msg != expectType) return Status::InvalidArg;
  st = bb.getBits(12, &stn);
  if (st != Status::Ok) return st;
  st = bb.getBits(6, &skip);
  if (st != Status::Ok) return st;
  st = bb.getBits(4, &skip);
  if (st != Status::Ok) return st;
  st = getBits38Signed(bb, x);
  if (st != Status::Ok) return st;
  st = bb.getBits(1, &skip);
  if (st != Status::Ok) return st;
  st = bb.getBits(1, &skip);
  if (st != Status::Ok) return st;
  st = getBits38Signed(bb, y);
  if (st != Status::Ok) return st;
  st = bb.getBits(2, &skip);
  if (st != Status::Ok) return st;
  st = getBits38Signed(bb, z);
  if (st != Status::Ok) return st;
  *station = static_cast<uint16_t>(stn);
  return Status::Ok;
}

Status putCountedString(BitBuffer& bb, const char* s, size_t maxLen) {
  size_t n = 0;
  if (s) {
    while (n < maxLen && s[n] != '\0') ++n;
  }
  if (n > 31) n = 31;
  Status st = bb.putBits(static_cast<uint32_t>(n), 8);
  if (st != Status::Ok) return st;
  for (size_t i = 0; i < n; ++i) {
    st = bb.putBits(static_cast<uint8_t>(s[i]), 8);
    if (st != Status::Ok) return st;
  }
  return Status::Ok;
}

Status getCountedString(BitBuffer& bb, char* dest, size_t destCap) {
  uint32_t n = 0;
  Status st = bb.getBits(8, &n);
  if (st != Status::Ok) return st;
  if (n >= destCap) return Status::Overflow;
  for (uint32_t i = 0; i < n; ++i) {
    uint32_t ch = 0;
    st = bb.getBits(8, &ch);
    if (st != Status::Ok) return st;
    dest[i] = static_cast<char>(ch);
  }
  dest[n] = '\0';
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
  st = bb.getBits(6, &skip);
  if (st != Status::Ok) return st;
  st = bb.getBits(4, &skip);
  if (st != Status::Ok) return st;

  int64_t x = 0;
  int64_t y = 0;
  int64_t z = 0;
  st = getBits38Signed(bb, &x);
  if (st != Status::Ok) return st;
  st = bb.getBits(1, &skip);
  if (st != Status::Ok) return st;
  st = bb.getBits(1, &skip);
  if (st != Status::Ok) return st;
  st = getBits38Signed(bb, &y);
  if (st != Status::Ok) return st;
  st = bb.getBits(2, &skip);
  if (st != Status::Ok) return st;
  st = getBits38Signed(bb, &z);
  if (st != Status::Ok) return st;

  out->stationId = static_cast<uint16_t>(station);
  out->ecefX01mm = x;
  out->ecefY01mm = y;
  out->ecefZ01mm = z;
  return Status::Ok;
}

Status encode1005(const Msg1005& msg, uint8_t* out, size_t cap, size_t* outLen) {
  if (out == nullptr || outLen == nullptr) return Status::InvalidArg;
  uint8_t payload[kMsg1005PayloadBytes] = {};
  BitBuffer bb(payload, sizeof(payload));
  bb.resetWrite();
  Status st = bb.putBits(kMsg1005Type, 12);
  if (st != Status::Ok) return st;
  st = bb.putBits(msg.stationId, 12);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 6);  // DF021 ITRF year
  if (st != Status::Ok) return st;
  st = bb.putBits(1, 1);  // DF022 GPS
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);  // GLONASS
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);  // Galileo
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);  // ref-station
  if (st != Status::Ok) return st;
  st = putBits38Signed(bb, msg.ecefX01mm);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);  // oscillator
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 1);  // reserved
  if (st != Status::Ok) return st;
  st = putBits38Signed(bb, msg.ecefY01mm);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 2);  // DF364
  if (st != Status::Ok) return st;
  st = putBits38Signed(bb, msg.ecefZ01mm);
  if (st != Status::Ok) return st;
  if (bb.bitLength() != 152) return Status::Overflow;
  return finalizeFrame(payload, sizeof(payload), out, cap, outLen);
}

Status rewrite1005ToPublishIdentity(const uint8_t* in, size_t inLen, uint8_t* out, size_t cap,
                                    size_t* outLen) {
  Msg1005 msg;
  const Status st = decode1005(in, inLen, &msg);
  if (st != Status::Ok) return st;
  msg.stationId = kPublishStationId;
  msg.ecefX01mm = kPublishArpEcef01mmX;
  msg.ecefY01mm = kPublishArpEcef01mmY;
  msg.ecefZ01mm = kPublishArpEcef01mmZ;
  return encode1005(msg, out, cap, outLen);
}

Status rewrite1006ToPublishIdentity(const uint8_t* in, size_t inLen, uint8_t* out, size_t cap,
                                    size_t* outLen) {
  Msg1006 msg;
  const Status st = decode1006(in, inLen, &msg);
  if (st != Status::Ok) return st;
  msg.stationId = kPublishStationId;
  msg.ecefX01mm = kPublishArpEcef01mmX;
  msg.ecefY01mm = kPublishArpEcef01mmY;
  msg.ecefZ01mm = kPublishArpEcef01mmZ;
  return encode1006(msg, out, cap, outLen);
}


Status decode1006(const uint8_t* frame, size_t len, Msg1006* out) {
  if (frame == nullptr || out == nullptr) return Status::InvalidArg;
  if (len < kRtcmMinFrameLen || frame[0] != 0xD3) return Status::InvalidArg;
  const size_t payloadLen =
      (static_cast<size_t>(frame[1] & 0x03u) << 8) | static_cast<size_t>(frame[2]);
  if (len != payloadLen + 6u) return Status::InvalidArg;
  if (!frameCrcOk(frame, len)) return Status::BadCrc;
  if (messageType(frame, len) != 1006) return Status::InvalidArg;
  if (payloadLen < 21) return Status::Overflow;
  uint8_t payload[21];
  for (size_t i = 0; i < 21; ++i) payload[i] = frame[3 + i];
  BitBuffer bb(payload, 21);
  bb.resetRead();
  uint16_t station = 0;
  int64_t x = 0, y = 0, z = 0;
  Status st = get1005StyleBody(bb, 1006, &station, &x, &y, &z);
  if (st != Status::Ok) return st;
  uint32_t h = 0;
  st = bb.getBits(16, &h);
  if (st != Status::Ok) return st;
  out->stationId = station;
  out->ecefX01mm = x;
  out->ecefY01mm = y;
  out->ecefZ01mm = z;
  out->antennaHeight01mm = static_cast<int32_t>(h);
  return Status::Ok;
}

Status encode1006(const Msg1006& msg, uint8_t* out, size_t cap, size_t* outLen) {
  if (out == nullptr || outLen == nullptr) return Status::InvalidArg;
  if (msg.antennaHeight01mm < 0 || msg.antennaHeight01mm > 65535) return Status::InvalidArg;
  uint8_t payload[21] = {};
  BitBuffer bb(payload, sizeof(payload));
  bb.resetWrite();
  Status st = put1005StyleBody(bb, 1006, msg.stationId, msg.ecefX01mm, msg.ecefY01mm, msg.ecefZ01mm);
  if (st != Status::Ok) return st;
  st = bb.putBits(static_cast<uint32_t>(msg.antennaHeight01mm), 16);
  if (st != Status::Ok) return st;
  if (bb.bitLength() != 168) return Status::Overflow;
  return finalizeFrame(payload, sizeof(payload), out, cap, outLen);
}

Status encode1033(const Msg1033& msg, uint8_t* out, size_t cap, size_t* outLen) {
  if (out == nullptr || outLen == nullptr) return Status::InvalidArg;
  uint8_t payload[128] = {};
  BitBuffer bb(payload, sizeof(payload));
  bb.resetWrite();
  Status st = bb.putBits(1033, 12);
  if (st != Status::Ok) return st;
  st = bb.putBits(msg.stationId, 12);
  if (st != Status::Ok) return st;
  st = putCountedString(bb, msg.antennaDescriptor, 31);
  if (st != Status::Ok) return st;
  st = bb.putBits(0, 8);
  if (st != Status::Ok) return st;
  st = putCountedString(bb, "", 0);
  if (st != Status::Ok) return st;
  st = putCountedString(bb, msg.receiverDescriptor, 31);
  if (st != Status::Ok) return st;
  st = putCountedString(bb, "", 0);
  if (st != Status::Ok) return st;
  st = putCountedString(bb, "", 0);
  if (st != Status::Ok) return st;
  return finalizeFrame(payload, bb.byteLength(), out, cap, outLen);
}

Status decode1033(const uint8_t* frame, size_t len, Msg1033* out) {
  if (frame == nullptr || out == nullptr) return Status::InvalidArg;
  if (len < kRtcmMinFrameLen || frame[0] != 0xD3) return Status::InvalidArg;
  const size_t payloadLen =
      (static_cast<size_t>(frame[1] & 0x03u) << 8) | static_cast<size_t>(frame[2]);
  if (len != payloadLen + 6u) return Status::InvalidArg;
  if (!frameCrcOk(frame, len)) return Status::BadCrc;
  if (messageType(frame, len) != 1033) return Status::InvalidArg;
  if (payloadLen == 0 || payloadLen > 128) return Status::Overflow;
  uint8_t payload[128];
  for (size_t i = 0; i < payloadLen; ++i) payload[i] = frame[3 + i];
  BitBuffer bb(payload, payloadLen);
  bb.resetRead();
  uint32_t msg = 0, stn = 0;
  Status st = bb.getBits(12, &msg);
  if (st != Status::Ok) return st;
  if (msg != 1033) return Status::InvalidArg;
  st = bb.getBits(12, &stn);
  if (st != Status::Ok) return st;
  char ant[32] = {};
  char rx[32] = {};
  char tmp[32] = {};
  st = getCountedString(bb, ant, sizeof(ant));
  if (st != Status::Ok) return st;
  uint32_t setup = 0;
  st = bb.getBits(8, &setup);
  if (st != Status::Ok) return st;
  st = getCountedString(bb, tmp, sizeof(tmp));
  if (st != Status::Ok) return st;
  st = getCountedString(bb, rx, sizeof(rx));
  if (st != Status::Ok) return st;
  st = getCountedString(bb, tmp, sizeof(tmp));
  if (st != Status::Ok) return st;
  st = getCountedString(bb, tmp, sizeof(tmp));
  if (st != Status::Ok) return st;
  out->stationId = static_cast<uint16_t>(stn);
  for (size_t i = 0; i < sizeof(out->antennaDescriptor); ++i) out->antennaDescriptor[i] = ant[i];
  for (size_t i = 0; i < sizeof(out->receiverDescriptor); ++i) out->receiverDescriptor[i] = rx[i];
  return Status::Ok;
}
Status summarizeMsmCnr(const uint8_t* frame, size_t len, MsmHeaderCnrSummary* out) {
  if (frame == nullptr || out == nullptr) return Status::InvalidArg;
  if (len < kRtcmMinFrameLen || frame[0] != 0xD3) return Status::InvalidArg;
  const size_t payloadLen =
      (static_cast<size_t>(frame[1] & 0x03u) << 8) | static_cast<size_t>(frame[2]);
  if (len != payloadLen + 6u) return Status::InvalidArg;
  if (!frameCrcOk(frame, len)) return Status::BadCrc;
  const uint16_t mt = messageType(frame, len);
  // MSM4 / MSM7 only (x74 / x77) across GNSS bands 1071..1127
  if (mt < 1071 || mt > 1127) return Status::InvalidArg;
  const uint8_t msm = static_cast<uint8_t>(mt % 10);
  if (msm != 4 && msm != 7) return Status::Unsupported;

  if (payloadLen == 0 || payloadLen > 1023) return Status::Overflow;
  // Payload lives in frame[3..]; BitBuffer needs mutable pointer â€” copy.
  uint8_t payload[1023];
  for (size_t i = 0; i < payloadLen; ++i) payload[i] = frame[3 + i];
  BitBuffer bb(payload, payloadLen);
  bb.resetRead();

  uint32_t msg = 0, stn = 0, skip = 0;
  Status st = bb.getBits(12, &msg);
  if (st != Status::Ok) return st;
  if (msg != mt) return Status::InvalidArg;
  st = bb.getBits(12, &stn);
  if (st != Status::Ok) return st;
  // GNSS epoch is 30 bits for all MSM (GLONASS packs DOW+TOD into 30).
  st = bb.getBits(30, &skip);
  if (st != Status::Ok) return st;
  // multiple(1)+IODS(3)+reserved(7)+clock(2)+extClock(2)+divFree(1)+smooth(3) = 19
  st = bb.getBits(19, &skip);
  if (st != Status::Ok) return st;

  uint32_t satHi = 0, satLo = 0, sigMask = 0;
  st = bb.getBits(32, &satHi);
  if (st != Status::Ok) return st;
  st = bb.getBits(32, &satLo);
  if (st != Status::Ok) return st;
  st = bb.getBits(32, &sigMask);
  if (st != Status::Ok) return st;

  auto pop32 = [](uint32_t x) -> uint8_t {
    uint8_t n = 0;
    while (x) {
      n = static_cast<uint8_t>(n + (x & 1u));
      x >>= 1;
    }
    return n;
  };
  const uint8_t nsat = static_cast<uint8_t>(pop32(satHi) + pop32(satLo));
  const uint8_t nsig = pop32(sigMask);
  const uint16_t ncellMax = static_cast<uint16_t>(nsat) * static_cast<uint16_t>(nsig);
  if (ncellMax > 64 * 32) return Status::Overflow;

  uint16_t ncell = 0;
  for (uint16_t i = 0; i < ncellMax; ++i) {
    uint32_t bit = 0;
    st = bb.getBits(1, &bit);
    if (st != Status::Ok) return st;
    if (bit) ++ncell;
  }

  const uint8_t satBits = (msm == 4) ? 18 : 36;
  const uint8_t cellBits = (msm == 4) ? 48 : 80;
  const uint8_t cnrBits = (msm == 4) ? 6 : 10;

  for (uint8_t i = 0; i < nsat; ++i) {
    // Skip satellite block in chunks of <=32 bits.
    uint8_t left = satBits;
    while (left > 0) {
      const uint8_t n = left > 32 ? 32 : left;
      st = bb.getBits(n, &skip);
      if (st != Status::Ok) return st;
      left = static_cast<uint8_t>(left - n);
    }
  }

  uint32_t sum01 = 0;
  uint16_t counted = 0;
  for (uint16_t i = 0; i < ncell; ++i) {
    const uint8_t skipBits = static_cast<uint8_t>(cellBits - cnrBits);
    uint8_t left = skipBits;
    while (left > 0) {
      const uint8_t n = left > 32 ? 32 : left;
      st = bb.getBits(n, &skip);
      if (st != Status::Ok) return st;
      left = static_cast<uint8_t>(left - n);
    }
    uint32_t cnr = 0;
    st = bb.getBits(cnrBits, &cnr);
    if (st != Status::Ok) return st;
    if (msm == 4) {
      sum01 += cnr * 10u;  // 1 dB-Hz -> 0.1 dB-Hz
    } else {
      // 0.0625 dB-Hz -> 0.1 dB-Hz: raw * 5 / 8
      sum01 += (cnr * 5u) / 8u;
    }
    ++counted;
  }

  out->messageType = mt;
  out->stationId = static_cast<uint16_t>(stn);
  out->satCount = nsat;
  out->sigCount = nsig;
  if (counted == 0) {
    out->meanCnr01dBHz = 0xFFFF;
  } else {
    out->meanCnr01dBHz = static_cast<uint16_t>(sum01 / counted);
  }
  return Status::Ok;
}

}  // namespace tinyrtcm3
