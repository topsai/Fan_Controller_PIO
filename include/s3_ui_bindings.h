#pragma once

#include <math.h>
#include <stdint.h>

#include "control_logic.h"

inline int16_t s3BatteryPercentForArc(bool valid, float socPercent) {
  if (!valid || isnan(socPercent)) {
    return 0;
  }
  return (int16_t)clampInt((int)(socPercent + 0.5f), 0, 100);
}

inline uint32_t s3SpeedColorHex(uint16_t speedKmh) {
  if (speedKmh >= 30) {
    return 0xFF4040;
  }
  if (speedKmh >= 15) {
    return 0xFFD23F;
  }
  return 0x00D86A;
}
