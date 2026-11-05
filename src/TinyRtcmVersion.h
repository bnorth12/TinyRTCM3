#pragma once
// Compile-time version for consumer static_assert pins (Mozihao / Base Station).
// Keep in sync with library.properties / library.json.

#ifndef TINYRTCM3_VERSION_MAJOR
#define TINYRTCM3_VERSION_MAJOR 0
#endif
#ifndef TINYRTCM3_VERSION_MINOR
#define TINYRTCM3_VERSION_MINOR 6
#endif
#ifndef TINYRTCM3_VERSION_PATCH
#define TINYRTCM3_VERSION_PATCH 2
#endif

#ifndef TINYRTCM3_VERSION_NUMBER
#define TINYRTCM3_VERSION_NUMBER \
  ((TINYRTCM3_VERSION_MAJOR * 10000) + (TINYRTCM3_VERSION_MINOR * 100) + TINYRTCM3_VERSION_PATCH)
#endif

#ifndef TINYRTCM3_VERSION_STRING
#define TINYRTCM3_VERSION_STRING "0.6.2"
#endif
