#pragma once

#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "control_logic.h"
#include "protocol.h"

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

inline void s3FormatMainStatusText(
  char *buffer,
  size_t bufferSize,
  bool connected,
  bool standby,
  bool takeoverActive,
  bool armed,
  uint8_t receiverStatusFlags
) {
  if (bufferSize == 0) {
    return;
  }
  if (takeoverActive) {
    snprintf(buffer, bufferSize, "TAKEOVER");
    return;
  }
  if (connected && (receiverStatusFlags & STATUS_FLAG_FAILSAFE) != 0) {
    snprintf(buffer, bufferSize, "RX FS");
    return;
  }
  if (connected && (receiverStatusFlags & STATUS_FLAG_PROTOCOL_FAULT) != 0) {
    snprintf(buffer, bufferSize, "PROTO");
    return;
  }
  if (standby && !connected) {
    snprintf(buffer, bufferSize, "STBY");
    return;
  }
  s3FormatStatusText(buffer, bufferSize, connected, armed);
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

inline bool s3McuTemperatureWarns(bool valid, float temperatureC, float warnThresholdC) {
  return valid && isfinite(temperatureC) && temperatureC >= warnThresholdC;
}

inline void s3FormatLinkDiagnosticText(char *buffer, size_t bufferSize, int16_t rssi, uint16_t packetRateHz, uint16_t lostPackets) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, "RSSI %d PKT %u LOSS %u", rssi, packetRateHz, lostPackets);
}

inline void s3FormatProtocolText(char *buffer, size_t bufferSize) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, "CTRL v%u STAT v%u", CONTROL_PROTOCOL_VERSION, STATUS_PROTOCOL_VERSION);
}

inline void s3FormatSequenceText(char *buffer, size_t bufferSize, uint16_t controlSequence, uint16_t statusSequence) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, "TX %u RX %u", controlSequence, statusSequence);
}

inline void s3FormatVescText(char *buffer, size_t bufferSize, uint8_t receiverStatusFlags) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, (receiverStatusFlags & STATUS_FLAG_VESC_VALID) != 0 ? "VESC OK" : "VESC N/A");
}

inline void s3FormatSensorsText(char *buffer, size_t bufferSize, bool cwValid, bool bmpValid, bool qmcValid) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer,
           bufferSize,
           "CW %s BMP %s QMC %s",
           cwValid ? "OK" : "N/A",
           bmpValid ? "OK" : "N/A",
           qmcValid ? "OK" : "N/A");
}

inline void s3FormatJoystickText(char *buffer, size_t bufferSize, const char *label, int value) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, "%s %d", label, value);
}

inline void s3FormatFirmwareText(char *buffer, size_t bufferSize, const char *version, const char *buildDate) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, "FW %s %s", version, buildDate);
}

inline void s3FormatBrightnessText(char *buffer, size_t bufferSize, uint8_t brightness) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, "BRI %u", brightness);
}

inline uint8_t s3ClampUserBrightness(int value) {
  return (uint8_t)clampInt(value, 20, 255);
}

inline void s3FormatPowerModeText(char *buffer, size_t bufferSize, bool standby, bool dimmed) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, "%s", standby ? "STANDBY" : (dimmed ? "DIM" : "ACTIVE"));
}

inline void s3FormatUpgradeText(char *buffer, size_t bufferSize) {
  if (bufferSize == 0) {
    return;
  }
  snprintf(buffer, bufferSize, "USB: pio upload");
}

inline void s3FormatReceiverStatusFlags(char *buffer, size_t bufferSize, uint8_t flags) {
  if (bufferSize == 0) {
    return;
  }
  if (flags == 0) {
    snprintf(buffer, bufferSize, "OK");
    return;
  }
  char text[48] = {};
  bool wrote = false;
  if ((flags & STATUS_FLAG_FAILSAFE) != 0) {
    snprintf(text + strlen(text), sizeof(text) - strlen(text), "%sFS", wrote ? " " : "");
    wrote = true;
  }
  if ((flags & STATUS_FLAG_PROTOCOL_FAULT) != 0) {
    snprintf(text + strlen(text), sizeof(text) - strlen(text), "%sPROTO", wrote ? " " : "");
    wrote = true;
  }
  if ((flags & STATUS_FLAG_VESC_VALID) != 0) {
    snprintf(text + strlen(text), sizeof(text) - strlen(text), "%sVESC", wrote ? " " : "");
    wrote = true;
  }
  if ((flags & STATUS_FLAG_OUTPUT_LOCKED) != 0) {
    snprintf(text + strlen(text), sizeof(text) - strlen(text), "%sLOCK", wrote ? " " : "");
  }
  snprintf(buffer, bufferSize, "%s", text);
}

inline float s3Bmp280AltitudeMeters(float pressureHpa) {
  if (isnan(pressureHpa) || pressureHpa <= 0.0f) {
    return NAN;
  }
  return 44330.0f * (1.0f - powf(pressureHpa / 1013.25f, 0.1903f));
}

inline uint8_t s3SpeedLevelFromAdc(int raw) {
  const int safeRaw = clampInt(raw, 0, 4095);
  if (safeRaw < 1365) {
    return 1;
  }
  if (safeRaw < 2730) {
    return 2;
  }
  return 3;
}
