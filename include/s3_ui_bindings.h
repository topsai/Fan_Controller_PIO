#pragma once

#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "control_logic.h"

inline int16_t s3BatteryPercentForArc(bool valid, float socPercent) {
  if (!valid || isnan(socPercent)) {
    return 0;
  }
  return (int16_t)clampInt((int)(socPercent + 0.5f), 0, 100);
}

inline bool s3Cw2015ReadingIsPlausible(float voltage, float socPercent) {
  return isfinite(voltage) && isfinite(socPercent) &&
         voltage >= 2.5f && voltage <= 5.5f &&
         socPercent >= 0.0f && socPercent <= 100.0f;
}

inline bool s3Bmp280ReadingIsPlausible(float pressureHpa, float altitudeM) {
  return isfinite(pressureHpa) && isfinite(altitudeM) &&
         pressureHpa >= 300.0f && pressureHpa <= 1100.0f &&
         altitudeM >= -1000.0f && altitudeM <= 10000.0f;
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

inline void s3FormatStatusText(char *buffer, size_t bufferSize, bool connected, bool armed) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, "%s%s", connected ? "OK" : "LOST", armed ? "" : " LOCK");
}

inline int16_t s3StatusVoltageForArc(bool connected, uint16_t voltageX100) {
  if (!connected) {
    return 6;
  }
  const int voltage = (voltageX100 + 50) / 100;
  return (int16_t)clampInt(voltage, 6, 48);
}

inline int16_t s3ThrottleBarValue(int16_t joystickValue) {
  return (int16_t)clampInt(joystickValue, -1000, 1000);
}

inline uint32_t s3ThrottleColorHex(int16_t joystickValue) {
  if (joystickValue > 0) {
    return 0x00C853;
  }
  if (joystickValue < 0) {
    return 0xFF9800;
  }
  return 0x606060;
}

inline int16_t s3CompassAngleForHeading(bool valid, float headingDeg) {
  if (!valid || !isfinite(headingDeg)) {
    return 0;
  }
  float normalized = fmodf(headingDeg, 360.0f);
  if (normalized < 0.0f) {
    normalized += 360.0f;
  }
  return (int16_t)(normalized * 10.0f + 0.5f);
}

inline void s3FormatMcuTemperatureText(char *buffer, size_t bufferSize, float temperatureC) {
  if (bufferSize == 0) {
    return;
  }
  if (!isfinite(temperatureC)) {
    snprintf(buffer, bufferSize, "--.-C");
    return;
  }
  snprintf(buffer, bufferSize, "%.1fC", temperatureC);
}

inline float s3Bmp280AltitudeMeters(float pressureHpa) {
  if (isnan(pressureHpa) || pressureHpa <= 0.0f) {
    return NAN;
  }
  return 44330.0f * (1.0f - powf(pressureHpa / 1013.25f, 0.1903f));
}
