#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// CAP-CRC-24Q / CAP-CRC-TABLE â€” CRC-24Q transport (poly 0x1864CFB, init 0).
// REQ-CRC-01..04: evaluate ingress (frameCrcOk) and seal egress (finalizeFrame).
// Default path is table-accelerated; bit path retained for identity self-tests.
// Define TINYRTCM3_CRC_BIT_ONLY to force bit-at-a-time.

static constexpr uint32_t kCrc24qPoly = 0x1864CFBu;
static constexpr size_t kRtcmMinFrameLen = 6;  // preamble+len+CRC, zero payload
static constexpr size_t kRtcmCrcLen = 3;

inline uint32_t crc24qBit(const uint8_t* data, size_t len) {
  uint32_t crc = 0;
  if (data == nullptr && len != 0) return 0;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint32_t>(data[i]) << 16;
    for (int b = 0; b < 8; ++b) {
      crc <<= 1;
      if (crc & 0x1000000u) crc ^= kCrc24qPoly;
    }
  }
  return crc & 0xFFFFFFu;
}

inline const uint32_t* crc24qTableEntries() {
  static const uint32_t kTable[256] = {
      0x000000u, 0x864CFBu, 0x8AD50Du, 0x0C99F6u, 0x93E6E1u, 0x15AA1Au, 0x1933ECu, 0x9F7F17u,
      0xA18139u, 0x27CDC2u, 0x2B5434u, 0xAD18CFu, 0x3267D8u, 0xB42B23u, 0xB8B2D5u, 0x3EFE2Eu,
      0xC54E89u, 0x430272u, 0x4F9B84u, 0xC9D77Fu, 0x56A868u, 0xD0E493u, 0xDC7D65u, 0x5A319Eu,
      0x64CFB0u, 0xE2834Bu, 0xEE1ABDu, 0x685646u, 0xF72951u, 0x7165AAu, 0x7DFC5Cu, 0xFBB0A7u,
      0x0CD1E9u, 0x8A9D12u, 0x8604E4u, 0x00481Fu, 0x9F3708u, 0x197BF3u, 0x15E205u, 0x93AEFEu,
      0xAD50D0u, 0x2B1C2Bu, 0x2785DDu, 0xA1C926u, 0x3EB631u, 0xB8FACAu, 0xB4633Cu, 0x322FC7u,
      0xC99F60u, 0x4FD39Bu, 0x434A6Du, 0xC50696u, 0x5A7981u, 0xDC357Au, 0xD0AC8Cu, 0x56E077u,
      0x681E59u, 0xEE52A2u, 0xE2CB54u, 0x6487AFu, 0xFBF8B8u, 0x7DB443u, 0x712DB5u, 0xF7614Eu,
      0x19A3D2u, 0x9FEF29u, 0x9376DFu, 0x153A24u, 0x8A4533u, 0x0C09C8u, 0x00903Eu, 0x86DCC5u,
      0xB822EBu, 0x3E6E10u, 0x32F7E6u, 0xB4BB1Du, 0x2BC40Au, 0xAD88F1u, 0xA11107u, 0x275DFCu,
      0xDCED5Bu, 0x5AA1A0u, 0x563856u, 0xD074ADu, 0x4F0BBAu, 0xC94741u, 0xC5DEB7u, 0x43924Cu,
      0x7D6C62u, 0xFB2099u, 0xF7B96Fu, 0x71F594u, 0xEE8A83u, 0x68C678u, 0x645F8Eu, 0xE21375u,
      0x15723Bu, 0x933EC0u, 0x9FA736u, 0x19EBCDu, 0x8694DAu, 0x00D821u, 0x0C41D7u, 0x8A0D2Cu,
      0xB4F302u, 0x32BFF9u, 0x3E260Fu, 0xB86AF4u, 0x2715E3u, 0xA15918u, 0xADC0EEu, 0x2B8C15u,
      0xD03CB2u, 0x567049u, 0x5AE9BFu, 0xDCA544u, 0x43DA53u, 0xC596A8u, 0xC90F5Eu, 0x4F43A5u,
      0x71BD8Bu, 0xF7F170u, 0xFB6886u, 0x7D247Du, 0xE25B6Au, 0x641791u, 0x688E67u, 0xEEC29Cu,
      0x3347A4u, 0xB50B5Fu, 0xB992A9u, 0x3FDE52u, 0xA0A145u, 0x26EDBEu, 0x2A7448u, 0xAC38B3u,
      0x92C69Du, 0x148A66u, 0x181390u, 0x9E5F6Bu, 0x01207Cu, 0x876C87u, 0x8BF571u, 0x0DB98Au,
      0xF6092Du, 0x7045D6u, 0x7CDC20u, 0xFA90DBu, 0x65EFCCu, 0xE3A337u, 0xEF3AC1u, 0x69763Au,
      0x578814u, 0xD1C4EFu, 0xDD5D19u, 0x5B11E2u, 0xC46EF5u, 0x42220Eu, 0x4EBBF8u, 0xC8F703u,
      0x3F964Du, 0xB9DAB6u, 0xB54340u, 0x330FBBu, 0xAC70ACu, 0x2A3C57u, 0x26A5A1u, 0xA0E95Au,
      0x9E1774u, 0x185B8Fu, 0x14C279u, 0x928E82u, 0x0DF195u, 0x8BBD6Eu, 0x872498u, 0x016863u,
      0xFAD8C4u, 0x7C943Fu, 0x700DC9u, 0xF64132u, 0x693E25u, 0xEF72DEu, 0xE3EB28u, 0x65A7D3u,
      0x5B59FDu, 0xDD1506u, 0xD18CF0u, 0x57C00Bu, 0xC8BF1Cu, 0x4EF3E7u, 0x426A11u, 0xC426EAu,
      0x2AE476u, 0xACA88Du, 0xA0317Bu, 0x267D80u, 0xB90297u, 0x3F4E6Cu, 0x33D79Au, 0xB59B61u,
      0x8B654Fu, 0x0D29B4u, 0x01B042u, 0x87FCB9u, 0x1883AEu, 0x9ECF55u, 0x9256A3u, 0x141A58u,
      0xEFAAFFu, 0x69E604u, 0x657FF2u, 0xE33309u, 0x7C4C1Eu, 0xFA00E5u, 0xF69913u, 0x70D5E8u,
      0x4E2BC6u, 0xC8673Du, 0xC4FECBu, 0x42B230u, 0xDDCD27u, 0x5B81DCu, 0x57182Au, 0xD154D1u,
      0x26359Fu, 0xA07964u, 0xACE092u, 0x2AAC69u, 0xB5D37Eu, 0x339F85u, 0x3F0673u, 0xB94A88u,
      0x87B4A6u, 0x01F85Du, 0x0D61ABu, 0x8B2D50u, 0x145247u, 0x921EBCu, 0x9E874Au, 0x18CBB1u,
      0xE37B16u, 0x6537EDu, 0x69AE1Bu, 0xEFE2E0u, 0x709DF7u, 0xF6D10Cu, 0xFA48FAu, 0x7C0401u,
      0x42FA2Fu, 0xC4B6D4u, 0xC82F22u, 0x4E63D9u, 0xD11CCEu, 0x575035u, 0x5BC9C3u, 0xDD8538u,
  };
  return kTable;
}

inline uint32_t crc24qTable(const uint8_t* data, size_t len) {
  uint32_t crc = 0;
  if (data == nullptr && len != 0) return 0;
  const uint32_t* t = crc24qTableEntries();
  for (size_t i = 0; i < len; ++i) {
    const uint8_t idx = static_cast<uint8_t>((crc >> 16) ^ data[i]);
    crc = ((crc << 8) & 0xFFFFFFu) ^ t[idx];
  }
  return crc & 0xFFFFFFu;
}

inline uint32_t crc24q(const uint8_t* data, size_t len) {
#if defined(TINYRTCM3_CRC_BIT_ONLY)
  return crc24qBit(data, len);
#else
  return crc24qTable(data, len);
#endif
}

inline void storeCrc24q(uint8_t out[3], uint32_t crc) {
  out[0] = static_cast<uint8_t>((crc >> 16) & 0xFF);
  out[1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
  out[2] = static_cast<uint8_t>(crc & 0xFF);
}

inline uint32_t loadCrc24q(const uint8_t in[3]) {
  return (static_cast<uint32_t>(in[0]) << 16) | (static_cast<uint32_t>(in[1]) << 8) |
         static_cast<uint32_t>(in[2]);
}

inline bool frameCrcOk(const uint8_t* frame, size_t len) {
  if (frame == nullptr || len < kRtcmMinFrameLen) return false;
  const uint32_t got = loadCrc24q(frame + len - kRtcmCrcLen);
  return crc24q(frame, len - kRtcmCrcLen) == got;
}

inline Status appendCrc24q(uint8_t* frame, size_t bodyLen, size_t cap, size_t* outLen) {
  if (frame == nullptr || outLen == nullptr) return Status::InvalidArg;
  if (bodyLen < 3) return Status::InvalidArg;
  if (cap < bodyLen + kRtcmCrcLen) return Status::Overflow;
  if (frame[0] != 0xD3) return Status::InvalidArg;
  const size_t payload =
      (static_cast<size_t>(frame[1] & 0x03u) << 8) | static_cast<size_t>(frame[2]);
  if (bodyLen != payload + 3u) return Status::InvalidArg;
  const uint32_t c = crc24q(frame, bodyLen);
  storeCrc24q(frame + bodyLen, c);
  *outLen = bodyLen + kRtcmCrcLen;
  return Status::Ok;
}

inline Status finalizeFrame(const uint8_t* payload, size_t payloadLen, uint8_t* out, size_t cap,
                            size_t* outLen) {
  if (out == nullptr || outLen == nullptr) return Status::InvalidArg;
  if (payloadLen > 1023u) return Status::Overflow;
  if (payload == nullptr && payloadLen != 0) return Status::InvalidArg;
  const size_t need = payloadLen + 6u;
  if (cap < need) return Status::Overflow;
  out[0] = 0xD3;
  out[1] = static_cast<uint8_t>((payloadLen >> 8) & 0x03u);
  out[2] = static_cast<uint8_t>(payloadLen & 0xFFu);
  if (payloadLen) {
    for (size_t i = 0; i < payloadLen; ++i) out[3 + i] = payload[i];
  }
  return appendCrc24q(out, payloadLen + 3u, cap, outLen);
}

inline uint16_t messageType(const uint8_t* frame, size_t len) {
  if (frame == nullptr || len < 5) return 0;
  return static_cast<uint16_t>((static_cast<uint16_t>(frame[3]) << 4) |
                               (static_cast<uint16_t>(frame[4]) >> 4));
}

}  // namespace tinyrtcm3
