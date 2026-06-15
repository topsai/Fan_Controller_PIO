#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static constexpr uint32_t DIAGNOSTIC_SHORT_TEST_MS = 10000;
static constexpr uint32_t DIAGNOSTIC_LONG_TEST_MS = 30UL * 60UL * 1000UL;

inline bool diagnosticDurationIsAllowed(uint32_t durationMs, bool allowLongTest) {
  return allowLongTest || durationMs <= DIAGNOSTIC_SHORT_TEST_MS;
}

inline void diagnosticFormatStatusLine(
  char *buffer,
  size_t bufferSize,
  const char *role,
  bool connected,
  uint32_t receivedPackets,
  uint32_t lostPackets,
  uint32_t faultCount
) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer,
           bufferSize,
           "DIAG STATUS role=%s connected=%u rx=%lu lost=%lu faults=%lu",
           role,
           connected ? 1 : 0,
           (unsigned long)receivedPackets,
           (unsigned long)lostPackets,
           (unsigned long)faultCount);
}
