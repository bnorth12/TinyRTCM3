#include "TinyRtcmCodec.h"

namespace tinyrtcm3 {

Status decode1005(const uint8_t*, size_t, Msg1005*) { return Status::Unsupported; }
Status encode1005(const Msg1005&, uint8_t*, size_t, size_t*) { return Status::Unsupported; }
Status encode1033(const Msg1033&, uint8_t*, size_t, size_t*) { return Status::Unsupported; }
Status summarizeMsmCnr(const uint8_t*, size_t, MsmHeaderCnrSummary*) {
  return Status::Unsupported;
}

}  // namespace tinyrtcm3
