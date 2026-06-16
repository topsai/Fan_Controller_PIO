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

struct JoystickCalibration {
  int center;
  int minRaw;
  int maxRaw;
  int deadzone;
};

inline bool joystickCalibrationIsValid(const JoystickCalibration &calibration) {
  return calibration.minRaw >= 0 &&
         calibration.maxRaw <= 4095 &&
         calibration.minRaw < calibration.center &&
         calibration.center < calibration.maxRaw &&
         calibration.deadzone >= 0 &&
         calibration.deadzone <= 500;
}

inline int joystickToThrottleCalibrated(int raw, const JoystickCalibration &calibration) {
  if (!joystickCalibrationIsValid(calibration)) {
    return joystickToThrottle(raw, calibration.center, calibration.deadzone);
  }
  const int centered = raw - calibration.center;
  if (centered > -calibration.deadzone && centered < calibration.deadzone) {
    return 0;
  }
  long mapped;
  if (centered > 0) {
    mapped = mapLong(raw, calibration.center, calibration.maxRaw, 0, 1000);
  } else {
    mapped = mapLong(raw, calibration.minRaw, calibration.center, -1000, 0);
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

inline bool joystickCenterIsValid(int center) {
  return center >= 1 && center <= 4094;
}

inline int adjustedJoystickCenter(int center, int delta) {
  return clampInt(center + delta, 1, 4094);
}

inline const char *c3HomeLinkLabel(bool connected) {
  return connected ? u8"\u8FDE\u63A5" : u8"\u65AD\u7EBF";
}

inline const char *c3HomeArmLabel(bool armed) {
  return armed ? u8"\u89E3" : u8"\u9501";
}

inline const char *c3HomeDirectionLabel(int16_t throttle) {
  return throttle > 0 ? u8"\u6CB9" : u8"\u5239";
}

inline const char *c3HomeBatteryLabel(bool batteryAvailable) {
  return u8"\u91CF";
}

inline uint8_t c3HomeSpeedTextSize() {
  return 3;
}

inline bool c3HomeUsesReadableChineseLabels() {
  return true;
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

inline void updateReceiverPacketGapDiagnostics(uint32_t nowMs, uint32_t &lastPacketMs, uint32_t &maxGapMs) {
  if (lastPacketMs != 0) {
    const uint32_t gapMs = nowMs - lastPacketMs;
    if (gapMs > maxGapMs) {
      maxGapMs = gapMs;
    }
  }
  lastPacketMs = nowMs;
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

inline void resetSequenceAfterConnectionTimeout(bool connected, bool &hasSequence, uint16_t &lastSequence) {
  if (connected) {
    return;
  }
  hasSequence = false;
  lastSequence = 0;
}

struct ControllerSourceState {
  bool hasActiveController;
  bool activeUsesLegacyProtocol;
  uint32_t lastSeenMs;
  uint8_t activeMac[6];
};

inline bool controllerMacEquals(const uint8_t *left, const uint8_t *right) {
  return left != nullptr && right != nullptr && memcmp(left, right, 6) == 0;
}

inline void rememberActiveController(const uint8_t *sourceMac, ControllerSourceState &state, bool legacyProtocol, uint32_t nowMs) {
  if (sourceMac == nullptr) {
    return;
  }
  memcpy(state.activeMac, sourceMac, 6);
  state.hasActiveController = true;
  state.activeUsesLegacyProtocol = legacyProtocol;
  state.lastSeenMs = nowMs;
}

inline bool controllerSourceIsActive(const uint8_t *sourceMac, const ControllerSourceState &state) {
  return state.hasActiveController && controllerMacEquals(sourceMac, state.activeMac);
}

inline bool controllerSourceAllowsPacket(
  const uint8_t *sourceMac,
  const ControllerSourceState &state,
  bool activeOnline,
  bool takeoverRequested
) {
  if (!state.hasActiveController || !activeOnline) {
    return true;
  }
  if (controllerSourceIsActive(sourceMac, state)) {
    return true;
  }
  return takeoverRequested;
}

inline bool controllerSourceShouldResetForTakeover(
  const uint8_t *sourceMac,
  const ControllerSourceState &state,
  bool takeoverRequested
) {
  return state.hasActiveController &&
         takeoverRequested &&
         !controllerSourceIsActive(sourceMac, state);
}

inline void releaseActiveControllerIfTimedOut(ControllerSourceState &state, uint32_t nowMs, uint32_t timeoutMs) {
  if (state.hasActiveController && nowMs - state.lastSeenMs > timeoutMs) {
    state.hasActiveController = false;
    state.activeUsesLegacyProtocol = false;
  }
}

enum ButtonPressEvent {
  BUTTON_PRESS_NONE = 0,
  BUTTON_PRESS_SHORT = 1,
  BUTTON_PRESS_LONG = 2,
};

struct ButtonLongPressState {
  bool wasDown;
  bool longPressFired;
  uint32_t pressedAtMs;
};

inline ButtonPressEvent buttonLongPressUpdate(bool isDown, uint32_t nowMs, uint32_t longPressMs, ButtonLongPressState &state) {
  if (isDown && !state.wasDown) {
    state.wasDown = true;
    state.longPressFired = false;
    state.pressedAtMs = nowMs;
    return BUTTON_PRESS_NONE;
  }
  if (isDown && state.wasDown && !state.longPressFired && nowMs - state.pressedAtMs >= longPressMs) {
    state.longPressFired = true;
    return BUTTON_PRESS_LONG;
  }
  if (!isDown && state.wasDown) {
    const bool wasLong = state.longPressFired;
    state.wasDown = false;
    state.longPressFired = false;
    return wasLong ? BUTTON_PRESS_NONE : BUTTON_PRESS_SHORT;
  }
  return BUTTON_PRESS_NONE;
}
