#pragma once

#include <stddef.h>
#include <stdint.h>

inline int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

inline long mapLong(long value, long inMin, long inMax, long outMin, long outMax) {
  return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

inline int maxThrottleForSpeedLevel(uint8_t speedLevel) {
  switch (speedLevel) {
    case 2: return 750;
    case 3: return 1000;
    case 1:
    default: return 500;
  }
}

inline int constrainedThrottle(int throttle, uint8_t speedLevel) {
  const int maxThrottle = maxThrottleForSpeedLevel(speedLevel);
  return clampInt(throttle, -maxThrottle, maxThrottle);
}

inline int pwmDutyForThrottle(int throttle, uint8_t speedLevel, int pwmMin, int pwmMax) {
  const int limitedThrottle = constrainedThrottle(throttle, speedLevel);
  return (int)mapLong(limitedThrottle, -1000, 1000, pwmMin, pwmMax);
}

inline int joystickToThrottle(int raw, int center, int deadzone) {
  const int safeCenter = clampInt(center, 1, 4094);
  int centered = raw - safeCenter;

  if (centered > -deadzone && centered < deadzone) {
    return 0;
  }

  long mapped;
  if (centered > 0) {
    mapped = mapLong(raw, safeCenter, 4095, 0, 1000);
  } else {
    mapped = mapLong(raw, 0, safeCenter, -1000, 0);
  }
  return clampInt((int)mapped, -1000, 1000);
}

inline int calibratedJoystickCenter(const int *samples, size_t sampleCount, int fallbackCenter) {
  if (samples == nullptr || sampleCount == 0) {
    return fallbackCenter;
  }

  long sum = 0;
  for (size_t i = 0; i < sampleCount; i++) {
    sum += samples[i];
  }
  return (int)((sum + (long)sampleCount / 2) / (long)sampleCount);
}

struct ReceiverControlState {
  int16_t throttle;
  uint8_t speedLevel;
  uint8_t buttons;
  bool connected;
  bool failsafeActive;
};

inline void applyReceiverFailsafe(ReceiverControlState &state) {
  state.throttle = 0;
  state.speedLevel = 1;
  state.buttons = 0;
  state.connected = false;
  state.failsafeActive = true;
}
