#ifndef TRANSMITTER_S3_UI_H
#define TRANSMITTER_S3_UI_H

#include <Arduino.h>

struct S3UiState {
  bool connected = false;
  uint16_t receiverVoltageX100 = 0;
  uint8_t speedLevel = 1;
  int16_t joystickValue = 0;
  uint16_t receiverSpeed = 0;
  bool cw2015Valid = false;
  float cw2015Soc = NAN;
  bool bmp280Valid = false;
  float bmp280PressureHpa = NAN;
  float bmp280AltitudeM = NAN;
  bool qmcValid = false;
  float qmcHeadingDeg = NAN;
  float mcuTemperatureC = NAN;
  bool mcuTemperatureWarning = false;
  int16_t rssiValue = -100;
  uint8_t receiverStatusFlags = 0;
  uint16_t statusPacketRateHz = 0;
  uint16_t statusLostPackets = 0;
  uint16_t controlSequence = 0;
  uint16_t lastStatusSequence = 0;
  uint8_t displayBrightness = 0;
  bool displayDimmed = false;
  bool standbyMode = false;
  bool takeoverActive = false;
  bool armed = false;
  bool settingsMode = false;
  int joystickCenter = 2048;
  int joystickRawAdc = 2048;
  int joystickMinRaw = 0;
  int joystickMaxRaw = 4095;
  int joystickDeadzone = 50;
  uint16_t speedAdcRaw = 0;
  const char *firmwareVersion = "";
  const char *buildDate = "";
};

enum class S3UiTouchAction : uint8_t {
  None,
  CalibrateCenter,
  CenterMinus,
  CenterPlus,
  ResetCalibration,
  CloseSettings,
  PageChanged,
};

void s3_ui_init();
void s3_ui_update(const S3UiState &state);
void s3_ui_set_settings_visible(bool visible);
S3UiTouchAction s3_ui_set_touch(bool pressed, int16_t x, int16_t y);
bool s3_ui_consume_brightness_request(uint8_t &brightness);

#endif
