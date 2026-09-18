#include "TinyRtcmSanitize.h"

namespace tinyrtcm3 {

Status sanitizeRewriteLocationFrame(uint8_t*, size_t, size_t, size_t*) {
  // Intent: eventually decode1005 → overwrite ARP/station → encode1005 → CRC.
  // Blocked on CAP-CODEC-1005. Callers must Drop (see sanitizeClassify) for now.
  return Status::Unsupported;
}

}  // namespace tinyrtcm3

