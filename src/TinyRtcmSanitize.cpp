#include "TinyRtcmSanitize.h"
#include "TinyRtcmCodec.h"
#include "TinyRtcmCrc24q.h"

namespace tinyrtcm3 {

namespace {
Status rewrite1006ToPublishIdentity(const uint8_t* in, size_t inLen, uint8_t* out, size_t cap,
                                    size_t* outLen) {
  Msg1006 msg;
  const Status st = decode1006(in, inLen, &msg);
  if (st != Status::Ok) return st;
  msg.stationId = kPublishStationId;
  msg.ecefX01mm = kPublishArpEcef01mmX;
  msg.ecefY01mm = kPublishArpEcef01mmY;
  msg.ecefZ01mm = kPublishArpEcef01mmZ;
  // Keep antenna height; location privacy is ECEF/station.
  return encode1006(msg, out, cap, outLen);
}

Status rewrite1033ToSanitized(const uint8_t* in, size_t inLen, uint8_t* out, size_t cap,
                              size_t* outLen) {
  Msg1033 msg;
  const Status st = decode1033(in, inLen, &msg);
  if (st != Status::Ok) return st;
  msg.stationId = kPublishStationId;
  // Fixed public descriptors — never real farm/shop strings.
  for (size_t i = 0; i < sizeof(msg.antennaDescriptor); ++i) msg.antennaDescriptor[i] = 0;
  for (size_t i = 0; i < sizeof(msg.receiverDescriptor); ++i) msg.receiverDescriptor[i] = 0;
  msg.antennaDescriptor[0] = 'A';
  msg.antennaDescriptor[1] = 'N';
  msg.antennaDescriptor[2] = 'T';
  msg.receiverDescriptor[0] = 'R';
  msg.receiverDescriptor[1] = 'C';
  msg.receiverDescriptor[2] = 'V';
  return encode1033(msg, out, cap, outLen);
}
}  // namespace

Status sanitizeRewriteLocationFrame(uint8_t* frame, size_t len, size_t cap, size_t* outLen) {
  if (frame == nullptr || outLen == nullptr) return Status::InvalidArg;
  if (len < kRtcmMinFrameLen || len > cap) return Status::InvalidArg;
  if (!frameCrcOk(frame, len)) return Status::BadCrc;
  const uint16_t t = messageType(frame, len);
  if (t == 1005) {
    return rewrite1005ToPublishIdentity(frame, len, frame, cap, outLen);
  }
  if (t == 1006) {
    return rewrite1006ToPublishIdentity(frame, len, frame, cap, outLen);
  }
  if (t == 1033) {
    return rewrite1033ToSanitized(frame, len, frame, cap, outLen);
  }
  *outLen = len;
  return Status::Ok;
}

}  // namespace tinyrtcm3
