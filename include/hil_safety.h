#pragma once

#ifdef FAN_CONTROLLER_HIL

#include <stdint.h>

struct HilOutputGate {
  bool unlocked;
  int16_t safePwm;
  int16_t expectedPwm;
  bool expectedBuzzer;
  uint32_t lastCommandAt;
};

inline HilOutputGate hilInitialOutputGate(int16_t safePwm) {
  return {false, safePwm, safePwm, false, 0};
}

inline void hilSetExpectedPwm(HilOutputGate &gate, int16_t pwm) {
  gate.expectedPwm = pwm;
}

inline int16_t hilActualPwm(const HilOutputGate &gate) {
  return gate.unlocked ? gate.expectedPwm : gate.safePwm;
}

inline bool hilActualBuzzer(const HilOutputGate &gate) {
  return gate.unlocked && gate.expectedBuzzer;
}

inline void hilSetOutputsUnlocked(HilOutputGate &gate, bool unlocked, uint32_t nowMs) {
  gate.unlocked = unlocked;
  gate.lastCommandAt = nowMs;
}

inline bool hilApplyOutputWatchdog(HilOutputGate &gate, uint32_t nowMs, uint32_t timeoutMs) {
  if (!gate.unlocked || static_cast<uint32_t>(nowMs - gate.lastCommandAt) < timeoutMs) return false;
  gate.unlocked = false;
  return true;
}

#endif
