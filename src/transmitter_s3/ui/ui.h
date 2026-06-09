#ifndef TRANSMITTER_S3_UI_H
#define TRANSMITTER_S3_UI_H

#include <Arduino.h>

struct S3UiState {
  bool connected = false;
  uint16_t receiverVoltageX100 = 0;
  uint8_t speedLevel = 1;
  int16_t joystickValue = 0;
  uint16_t receiverSpeed = 0;
  int16_t rssiValue = -100;
  uint8_t buttonState = 0;
  bool cw2015Valid = false;
  float cw2015Voltage = NAN;
  float cw2015Soc = NAN;
  bool bmp280Valid = false;
  float bmp280TemperatureC = NAN;
  float bmp280PressureHpa = NAN;
  bool qmcValid = false;
  float qmcHeadingDeg = NAN;
  uint16_t displayFps = 0;
};

void s3_ui_init();
void s3_ui_update(const S3UiState &state);
void s3_ui_set_touch(bool pressed, int16_t x, int16_t y);

#endif
