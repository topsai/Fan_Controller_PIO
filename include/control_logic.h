#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

inline int16_t safeThrottleForArming(int16_t throttle, bool armed) {
  return armed ? throttle : 0;
}

inline bool shouldArmByBrakeHold(
  int16_t throttle,
  uint32_t nowMs,
  uint32_t &holdStartMs,
  bool &holding,
  int16_t brakeThreshold,
  uint32_t holdDurationMs
) {
  if (throttle > brakeThreshold) {
    holding = false;
    return false;
  }
  if (!holding) {
    holding = true;
    holdStartMs = nowMs;
    return false;
  }
  return nowMs - holdStartMs >= holdDurationMs;
}

inline int16_t slewLimitedThrottle(int16_t currentThrottle, int16_t targetThrottle, int16_t maxStep) {
  const int safeMaxStep = maxStep < 0 ? -maxStep : maxStep;
  if (safeMaxStep == 0) {
    return currentThrottle;
  }
  const int delta = (int)targetThrottle - (int)currentThrottle;
  if (delta > safeMaxStep) {
    return (int16_t)clampInt((int)currentThrottle + safeMaxStep, -1000, 1000);
  }
  if (delta < -safeMaxStep) {
    return (int16_t)clampInt((int)currentThrottle - safeMaxStep, -1000, 1000);
  }
  return (int16_t)clampInt(targetThrottle, -1000, 1000);
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

inline bool shouldEmitReceiverLinkAlert(
  bool connected,
  bool failsafeActive,
  uint32_t nowMs,
  uint32_t &lastAlertMs,
  bool &hasAlerted,
  uint32_t intervalMs
) {
  if (connected && !failsafeActive) {
    hasAlerted = false;
    return false;
  }

  if (!hasAlerted || nowMs - lastAlertMs >= intervalMs) {
    lastAlertMs = nowMs;
    hasAlerted = true;
    return true;
  }

  return false;
}

inline bool shouldAllowRemoteHorn(
  bool buttonPressed,
  uint32_t nowMs,
  uint32_t &hornStartMs,
  bool &hornActive,
  uint32_t maxDurationMs
) {
  if (!buttonPressed) {
    hornActive = false;
    return false;
  }

  if (!hornActive) {
    hornActive = true;
    hornStartMs = nowMs;
    return true;
  }

  if (nowMs - hornStartMs >= maxDurationMs) {
    hornActive = false;
    return false;
  }

  return true;
}

inline bool shouldSignalReceiverConnectionSuccess(bool wasConnected, bool wasFailsafeActive) {
  return !wasConnected || wasFailsafeActive;
}

inline void rememberStatusTarget(const uint8_t *sourceMac, uint8_t *targetMac, bool &hasTarget) {
  if (sourceMac == nullptr || targetMac == nullptr) {
    return;
  }
  memcpy(targetMac, sourceMac, 6);
  hasTarget = true;
}
