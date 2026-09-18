#include "TinyRtcmSanitize.h"
#include "TinyRtcmCodec.h"
#include "TinyRtcmCrc24q.h"

namespace tinyrtcm3 {

Status sanitizeRewriteLocationFrame(uint8_t* frame, size_t len, size_t cap, size_t* outLen) {
  if (frame == nullptr || outLen == nullptr) return Status::InvalidArg;
  if (len < kRtcmMinFrameLen || len > cap) return Status::InvalidArg;
  if (!frameCrcOk(frame, len)) return Status::BadCrc;
  const uint16_t t = messageType(frame, len);
  if (t == 1005) {
    return rewrite1005ToPublishIdentity(frame, len, frame, cap, outLen);
  }
  // 1006/1033 rewrite not implemented yet — callers should Drop
  if (t == 1006 || t == 1033) return Status::Unsupported;
  *outLen = len;
  return Status::Ok;
}

}  // namespace tinyrtcm3
