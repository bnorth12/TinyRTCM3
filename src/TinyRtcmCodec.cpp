#include "TinyRtcmCodec.h"

namespace tinyrtcm3 {

// All Codec entry points intentionally return Unsupported until bit-accurate
// implementations and synthetic goldens land. Keeping stubs linked lets Hub,
// Sanitize, and apps compile against the ICD surface (REQ-VER-02).

Status decode1005(const uint8_t*, size_t, Msg1005*) { return Status::Unsupported; }
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
