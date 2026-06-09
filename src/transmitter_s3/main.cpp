#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>
#include "beep_profiles.h"
#include "control_logic.h"
#include "s3_runtime_config.h"
#include "s3_ui_bindings.h"
#include "ui/ui.h"

namespace {

constexpr int LCD_POWER_PIN = 41;
constexpr int LCD_POWER_ACTIVE_LEVEL = LOW;
constexpr int LCD_TE_PIN = 47;
constexpr int LCD_RST_PIN = 8;

constexpr int AUX_I2C_SDA_PIN = 18;
constexpr int AUX_I2C_SCL_PIN = 19;
constexpr uint32_t AUX_I2C_FREQ_HZ = 300000;

// Temporary software placeholders. Replace after the S3 PCB pinout is finalized.
constexpr int JOYSTICK_PIN = 1;
constexpr int SWITCH_PIN_1 = 37;
constexpr int SWITCH_PIN_2 = 38;
constexpr int SWITCH_PIN_3 = 39;
constexpr int BUTTON_1_PIN = 35;
constexpr int BUTTON_2_PIN = 36;
constexpr int BUZZER_PIN = 42;

constexpr uint8_t RECEIVER_MAC[] = {0xAC, 0xEB, 0xE6, 0x44, 0xC5, 0x90};
constexpr uint8_t CW2015_ADDR = 0x62;
constexpr uint8_t BMP280_ADDR_0 = 0x76;
constexpr uint8_t BMP280_ADDR_1 = 0x77;
constexpr uint8_t QMC5883L_ADDR = 0x0D;

constexpr uint16_t CONTROL_CONNECTED_INTERVAL_MS = 10;
constexpr uint16_t CONTROL_SEARCH_INTERVAL_MS = 50;
constexpr uint16_t DISPLAY_INTERVAL_MS = 200;
constexpr uint16_t LOCAL_SENSOR_INTERVAL_MS = 500;
constexpr uint8_t LOCAL_SENSOR_INVALIDATE_AFTER_FAILURES = 6;
constexpr uint16_t CONNECTION_TIMEOUT_MS = S3_ESPNOW_STATUS_TIMEOUT_MS;
constexpr uint8_t ESPNOW_CHANNEL = 1;
constexpr uint8_t LCD_BRIGHTNESS = 140;
constexpr int JOYSTICK_DEADZONE = 50;
constexpr int ADC_CENTER = 2048;
constexpr int ARM_BRAKE_THRESHOLD = -900;
constexpr uint32_t ARM_BRAKE_HOLD_MS = 3000;
constexpr int THROTTLE_SLEW_STEP = 40;
constexpr int JOYSTICK_CENTER_ADJUST_STEP = 10;
constexpr uint8_t LOW_BATTERY_THRESHOLD = 20;
constexpr uint8_t CRITICAL_BATTERY_THRESHOLD = 10;

#if S3_ESPNOW_TX_POWER_DBM_X4 >= 78
constexpr wifi_power_t S3_ESPNOW_TX_POWER = WIFI_POWER_19_5dBm;
#elif S3_ESPNOW_TX_POWER_DBM_X4 >= 60
constexpr wifi_power_t S3_ESPNOW_TX_POWER = WIFI_POWER_15dBm;
#elif S3_ESPNOW_TX_POWER_DBM_X4 >= 34
constexpr wifi_power_t S3_ESPNOW_TX_POWER = WIFI_POWER_8_5dBm;
#else
constexpr wifi_power_t S3_ESPNOW_TX_POWER = WIFI_POWER_2dBm;
#endif

#pragma pack(push, 1)
struct ControlPacket {
  uint8_t head;
  uint8_t type;
  int16_t throttle;
  uint8_t speedLevel;
  uint8_t buttons;
  uint8_t checksum;
};

struct StatusPacket {
  uint8_t head;
  uint8_t type;
  int16_t rssi;
  uint16_t voltage;
  uint8_t motorPWM[4];
  uint16_t speed;
  uint8_t status;
  uint8_t checksum;
};
#pragma pack(pop)

class S3RoundDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 panel_;
  lgfx::Bus_Parallel8 bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_CST816S touch_;

public:
  S3RoundDisplay() {
    {
      auto cfg = bus_.config();
      cfg.port = 0;
      cfg.freq_write = 20000000;
      cfg.freq_read = 8000000;
      cfg.pin_rd = -1;
      cfg.pin_wr = 7;
      cfg.pin_rs = 13;
      cfg.pin_d0 = 6;
      cfg.pin_d1 = 12;
      cfg.pin_d2 = 5;
      cfg.pin_d3 = 11;
      cfg.pin_d4 = 4;
      cfg.pin_d5 = 10;
      cfg.pin_d6 = 3;
      cfg.pin_d7 = 9;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }

    {
      auto cfg = panel_.config();
      cfg.pin_cs = 14;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 240;
      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }

    {
      auto cfg = light_.config();
      cfg.pin_bl = 45;
      cfg.invert = false;
      cfg.freq = 1000;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }

    {
      auto cfg = touch_.config();
      cfg.i2c_port = 0;
      cfg.i2c_addr = 0x15;
      cfg.pin_sda = 15;
      cfg.pin_scl = 16;
      cfg.pin_int = 17;
      cfg.pin_rst = -1;
      cfg.freq = 400000;
      cfg.x_min = 0;
      cfg.x_max = 239;
      cfg.y_min = 0;
      cfg.y_max = 239;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      touch_.config(cfg);
      panel_.setTouch(&touch_);
    }

    setPanel(&panel_);
  }
};

struct Cw2015Data {
  bool present = false;
  bool valid = false;
  uint8_t failureCount = 0;
  float voltage = NAN;
  float soc = NAN;
};

struct Bmp280Cal {
  uint16_t dig_T1 = 0;
  int16_t dig_T2 = 0;
  int16_t dig_T3 = 0;
  uint16_t dig_P1 = 0;
  int16_t dig_P2 = 0;
  int16_t dig_P3 = 0;
  int16_t dig_P4 = 0;
  int16_t dig_P5 = 0;
  int16_t dig_P6 = 0;
  int16_t dig_P7 = 0;
  int16_t dig_P8 = 0;
  int16_t dig_P9 = 0;
  int32_t tFine = 0;
};

struct Bmp280Data {
  bool present = false;
  bool valid = false;
  uint8_t failureCount = 0;
  uint8_t addr = 0;
  float temperatureC = NAN;
  float pressureHpa = NAN;
  float altitudeM = NAN;
  Bmp280Cal cal;
};

struct Qmc5883lData {
  bool present = false;
  bool valid = false;
  int16_t mx = 0;
  int16_t my = 0;
  int16_t mz = 0;
  float headingDeg = NAN;
};

S3RoundDisplay display;
constexpr uint16_t LCD_WIDTH = 240;
constexpr uint16_t LCD_HEIGHT = 240;
constexpr uint16_t LVGL_BUFFER_LINES = 40;
lv_disp_draw_buf_t lvDrawBuffer;
lv_color_t lvBuffer[LCD_WIDTH * LVGL_BUFFER_LINES];
lv_disp_drv_t lvDisplayDriver;
lv_indev_drv_t lvTouchDriver;
uint32_t lastLvTickMs = 0;
uint32_t lastDisplayFpsSampleMs = 0;
uint16_t displayFrameCount = 0;
uint16_t displayFps = 0;

Cw2015Data cw2015;
Bmp280Data bmp280;
Qmc5883lData qmc;
bool auxI2cFound[128] = {};
uint8_t receiverMac[] = {0xAC, 0xEB, 0xE6, 0x44, 0xC5, 0x90};

int joystickCenter = ADC_CENTER;
int16_t joystickRawValue = 0;
int16_t joystickValue = 0;
uint8_t speedLevel = 1;
uint8_t buttonState = 0;
bool transmitterArmed = false;
bool settingsMode = false;
bool joystickCalibrateRequested = false;
int16_t joystickCenterAdjust = 0;
uint32_t armBrakeHoldStartMs = 0;
bool armBrakeHolding = false;
bool connected = false;
uint32_t lastRecvTime = 0;
int16_t rssiValue = -100;
uint16_t receiverVoltageX100 = 0;
uint16_t receiverSpeed = 0;
bool lowBatteryWarned = false;
uint32_t lastDisplayMs = 0;
uint32_t lastLocalSensorMs = 0;
uint32_t lastDebugMs = 0;
uint32_t lastLinkAlertMs = 0;
uint32_t lastLvglHandlerMs = 0;
TaskHandle_t controlTaskHandle = nullptr;

void startControlSendTask();

uint8_t calcChecksum(const uint8_t *data, uint8_t len) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len - 1; i++) {
    sum += data[i];
  }
  return sum;
}

uint16_t mapTouchX(uint16_t rawX) {
  return 239 - rawX;
}

int16_t clampTouchCoord(uint16_t value) {
  if (value >= LCD_WIDTH) {
    return LCD_WIDTH - 1;
  }
  return value;
}

bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(addr);
  Wire1.write(reg);
  Wire1.write(value);
  return Wire1.endTransmission() == 0;
}

bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t *buffer, size_t len) {
  Wire1.beginTransmission(addr);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) {
    return false;
  }
  if (Wire1.requestFrom((int)addr, (int)len) != len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    buffer[i] = Wire1.read();
  }
  return true;
}

bool i2cReadU8(uint8_t addr, uint8_t reg, uint8_t &value) {
  return i2cReadBytes(addr, reg, &value, 1);
}

uint16_t u16le(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

int16_t s16le(const uint8_t *p) {
  return (int16_t)u16le(p);
}

void beep(uint16_t frequencyHz, uint16_t durationMs) {
  tone(BUZZER_PIN, frequencyHz, durationMs);
}

void scanAuxI2c() {
  memset(auxI2cFound, 0, sizeof(auxI2cFound));
  Serial.println("S3 AUX I2C scan");
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire1.beginTransmission(addr);
    if (Wire1.endTransmission() == 0) {
      auxI2cFound[addr] = true;
      Serial.printf("  found 0x%02X\n", addr);
    }
  }
}

void initCw2015() {
  cw2015.present = auxI2cFound[CW2015_ADDR];
  if (cw2015.present) {
    i2cWriteReg(CW2015_ADDR, 0x0A, 0x00);
  }
}

void readCw2015() {
  if (!cw2015.present) {
    cw2015.valid = false;
    return;
  }
  uint8_t vcell[2] = {};
  uint8_t soc[2] = {};
  if (!i2cReadBytes(CW2015_ADDR, 0x02, vcell, sizeof(vcell))) {
    if (++cw2015.failureCount >= LOCAL_SENSOR_INVALIDATE_AFTER_FAILURES) {
      cw2015.valid = false;
    }
    return;
  }
  if (!i2cReadBytes(CW2015_ADDR, 0x04, soc, sizeof(soc))) {
    if (++cw2015.failureCount >= LOCAL_SENSOR_INVALIDATE_AFTER_FAILURES) {
      cw2015.valid = false;
    }
    return;
  }
  const uint16_t rawVoltage = ((uint16_t)vcell[0] << 8) | vcell[1];
  const float voltage = rawVoltage * 0.000305f;
  const float socPercent = soc[0] + soc[1] / 256.0f;
  if (!s3Cw2015ReadingIsPlausible(voltage, socPercent)) {
    if (++cw2015.failureCount >= LOCAL_SENSOR_INVALIDATE_AFTER_FAILURES) {
      cw2015.valid = false;
    }
    return;
  }
  cw2015.voltage = voltage;
  cw2015.soc = socPercent;
  cw2015.valid = true;
  cw2015.failureCount = 0;
}

bool loadBmp280Cal(uint8_t addr, Bmp280Cal &cal) {
  uint8_t data[24] = {};
  if (!i2cReadBytes(addr, 0x88, data, sizeof(data))) {
    return false;
  }
  cal.dig_T1 = u16le(&data[0]);
  cal.dig_T2 = s16le(&data[2]);
  cal.dig_T3 = s16le(&data[4]);
  cal.dig_P1 = u16le(&data[6]);
  cal.dig_P2 = s16le(&data[8]);
  cal.dig_P3 = s16le(&data[10]);
  cal.dig_P4 = s16le(&data[12]);
  cal.dig_P5 = s16le(&data[14]);
  cal.dig_P6 = s16le(&data[16]);
  cal.dig_P7 = s16le(&data[18]);
  cal.dig_P8 = s16le(&data[20]);
  cal.dig_P9 = s16le(&data[22]);
  return true;
}

float compensateBmp280Temperature(Bmp280Cal &cal, int32_t adcT) {
  const int32_t var1 = ((((adcT >> 3) - ((int32_t)cal.dig_T1 << 1))) * (int32_t)cal.dig_T2) >> 11;
  const int32_t var2 = (((((adcT >> 4) - (int32_t)cal.dig_T1) * ((adcT >> 4) - (int32_t)cal.dig_T1)) >> 12) *
                        (int32_t)cal.dig_T3) >>
                       14;
  cal.tFine = var1 + var2;
  return ((cal.tFine * 5 + 128) >> 8) / 100.0f;
}

float compensateBmp280Pressure(const Bmp280Cal &cal, int32_t adcP) {
  int64_t var1 = (int64_t)cal.tFine - 128000;
  int64_t var2 = var1 * var1 * (int64_t)cal.dig_P6;
  var2 += (var1 * (int64_t)cal.dig_P5) << 17;
  var2 += (int64_t)cal.dig_P4 << 35;
  var1 = ((var1 * var1 * (int64_t)cal.dig_P3) >> 8) + ((var1 * (int64_t)cal.dig_P2) << 12);
  var1 = (((int64_t)1 << 47) + var1) * (int64_t)cal.dig_P1 >> 33;
  if (var1 == 0) {
    return NAN;
  }
  int64_t p = 1048576 - adcP;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = ((int64_t)cal.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
  var2 = ((int64_t)cal.dig_P8 * p) >> 19;
  p = ((p + var1 + var2) >> 8) + ((int64_t)cal.dig_P7 << 4);
  return (p / 256.0f) / 100.0f;
}

void initBmp280() {
  const uint8_t candidates[] = {BMP280_ADDR_0, BMP280_ADDR_1};
  for (uint8_t addr : candidates) {
    uint8_t id = 0;
    if (auxI2cFound[addr] && i2cReadU8(addr, 0xD0, id) && id == 0x58 && loadBmp280Cal(addr, bmp280.cal)) {
      bmp280.present = true;
      bmp280.addr = addr;
      i2cWriteReg(addr, 0xF4, 0x27);
      i2cWriteReg(addr, 0xF5, 0xA0);
      return;
    }
  }
}

void readBmp280() {
  if (!bmp280.present) {
    bmp280.valid = false;
    return;
  }
  uint8_t data[6] = {};
  if (!i2cReadBytes(bmp280.addr, 0xF7, data, sizeof(data))) {
    if (++bmp280.failureCount >= LOCAL_SENSOR_INVALIDATE_AFTER_FAILURES) {
      bmp280.valid = false;
    }
    return;
  }
  const int32_t adcP = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
  const int32_t adcT = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);
  const float temperatureC = compensateBmp280Temperature(bmp280.cal, adcT);
  const float pressureHpa = compensateBmp280Pressure(bmp280.cal, adcP);
  const float altitudeM = s3Bmp280AltitudeMeters(pressureHpa);
  if (!s3Bmp280ReadingIsPlausible(pressureHpa, altitudeM)) {
    if (++bmp280.failureCount >= LOCAL_SENSOR_INVALIDATE_AFTER_FAILURES) {
      bmp280.valid = false;
    }
    return;
  }
  bmp280.temperatureC = temperatureC;
  bmp280.pressureHpa = pressureHpa;
  bmp280.altitudeM = altitudeM;
  bmp280.valid = true;
  bmp280.failureCount = 0;
}

void initQmc5883l() {
  qmc.present = auxI2cFound[QMC5883L_ADDR];
  if (qmc.present) {
    i2cWriteReg(QMC5883L_ADDR, 0x0B, 0x01);
    i2cWriteReg(QMC5883L_ADDR, 0x09, 0x1D);
  }
}

void readQmc5883l() {
  qmc.valid = false;
  if (!qmc.present) {
    return;
  }
  uint8_t data[6] = {};
  if (!i2cReadBytes(QMC5883L_ADDR, 0x00, data, sizeof(data))) {
    return;
  }
  qmc.mx = s16le(&data[0]);
  qmc.my = s16le(&data[2]);
  qmc.mz = s16le(&data[4]);
  qmc.headingDeg = atan2f((float)qmc.my, (float)qmc.mx) * 180.0f / PI;
  if (qmc.headingDeg < 0.0f) {
    qmc.headingDeg += 360.0f;
  }
  qmc.valid = true;
}

void initLocalSensors() {
  Wire1.begin(AUX_I2C_SDA_PIN, AUX_I2C_SCL_PIN, AUX_I2C_FREQ_HZ);
  scanAuxI2c();
  initCw2015();
  initBmp280();
  initQmc5883l();
}

void readLocalSensors() {
  readCw2015();
  readBmp280();
  readQmc5883l();
}

void setupPins() {
  pinMode(SWITCH_PIN_1, INPUT_PULLUP);
  pinMode(SWITCH_PIN_2, INPUT_PULLUP);
  pinMode(SWITCH_PIN_3, INPUT_PULLUP);
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

void calibrateJoystickCenter() {
  const size_t sampleCount = 64;
  int samples[sampleCount] = {};
  for (size_t i = 0; i < sampleCount; i++) {
    samples[i] = analogRead(JOYSTICK_PIN);
    delay(2);
  }
  joystickCenter = calibratedJoystickCenter(samples, sampleCount, ADC_CENTER);
  Serial.printf("S3 joystick center: %d\n", joystickCenter);
}

void readInputs() {
  const int16_t targetThrottle = joystickToThrottle(analogRead(JOYSTICK_PIN), joystickCenter, JOYSTICK_DEADZONE);
  joystickRawValue = targetThrottle;
  if (!settingsMode && !transmitterArmed &&
      shouldArmByBrakeHold(targetThrottle, millis(), armBrakeHoldStartMs, armBrakeHolding,
                           ARM_BRAKE_THRESHOLD, ARM_BRAKE_HOLD_MS)) {
    transmitterArmed = true;
    armBrakeHolding = false;
    beep(BEEP_FREQ_CONNECTED, 80);
  }
  const int16_t safeTarget = settingsMode ? 0 : safeThrottleForArming(targetThrottle, transmitterArmed);
  joystickValue = slewLimitedThrottle(joystickValue, safeTarget, THROTTLE_SLEW_STEP);

  const bool sw1 = !digitalRead(SWITCH_PIN_1);
  const bool sw2 = !digitalRead(SWITCH_PIN_2);
  const bool sw3 = !digitalRead(SWITCH_PIN_3);
  if (sw3) {
    speedLevel = 3;
  } else if (sw2) {
    speedLevel = 2;
  } else {
    speedLevel = 1;
  }

  uint8_t nextButtons = 0;
  const bool button1Down = !digitalRead(BUTTON_1_PIN);
  if (!digitalRead(BUTTON_2_PIN)) {
    nextButtons |= 0x02;
  }
  static uint8_t lastButtons = 0;
  static bool lastButton1Down = false;
  if (button1Down && !lastButton1Down) {
    settingsMode = !settingsMode;
    transmitterArmed = false;
    armBrakeHolding = false;
    joystickValue = 0;
    beep(BEEP_FREQ_BUTTON, 50);
  }
  lastButton1Down = button1Down;

  if ((nextButtons & ~lastButtons) != 0 && !settingsMode) {
    beep(BEEP_FREQ_BUTTON, 50);
  }
  lastButtons = nextButtons;
  buttonState = settingsMode ? 0 : nextButtons;
}

void setupDisplay() {
  pinMode(LCD_POWER_PIN, OUTPUT);
  digitalWrite(LCD_POWER_PIN, LCD_POWER_ACTIVE_LEVEL);
  pinMode(LCD_RST_PIN, INPUT_PULLUP);
  pinMode(LCD_TE_PIN, INPUT);
  display.init();
  display.setRotation(0);
  display.setBrightness(LCD_BRIGHTNESS);
}

void lvFlushCallback(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *colorP) {
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;
#if S3_LVGL_DISPLAY_USE_DMA
  display.pushImageDMA(area->x1, area->y1, width, height, reinterpret_cast<const uint16_t *>(colorP));
  display.waitDMA();
#else
  display.pushImage(area->x1, area->y1, width, height, reinterpret_cast<const uint16_t *>(colorP));
#endif
  displayFrameCount++;
  lv_disp_flush_ready(disp);
}

void lvTouchReadCallback(lv_indev_drv_t *, lv_indev_data_t *data) {
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  if (display.getTouch(&rawX, &rawY) && rawX < LCD_WIDTH && rawY < LCD_HEIGHT) {
    const int16_t x = clampTouchCoord(mapTouchX(rawX));
    const int16_t y = clampTouchCoord(rawY);
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
    const S3UiTouchAction action = s3_ui_set_touch(true, x, y);
    switch (action) {
      case S3UiTouchAction::CalibrateCenter:
        joystickCalibrateRequested = true;
        beep(BEEP_FREQ_BUTTON, 50);
        break;
      case S3UiTouchAction::CenterMinus:
        joystickCenterAdjust -= JOYSTICK_CENTER_ADJUST_STEP;
        beep(BEEP_FREQ_BUTTON, 30);
        break;
      case S3UiTouchAction::CenterPlus:
        joystickCenterAdjust += JOYSTICK_CENTER_ADJUST_STEP;
        beep(BEEP_FREQ_BUTTON, 30);
        break;
      case S3UiTouchAction::CloseSettings:
        settingsMode = false;
        transmitterArmed = false;
        armBrakeHolding = false;
        joystickValue = 0;
        beep(BEEP_FREQ_BUTTON, 50);
        break;
      case S3UiTouchAction::None:
      default:
        break;
    }
    return;
  }
  data->state = LV_INDEV_STATE_RELEASED;
  s3_ui_set_touch(false, 0, 0);
}

void setupLvgl() {
  lv_init();
  lv_disp_draw_buf_init(&lvDrawBuffer, lvBuffer, nullptr, LCD_WIDTH * LVGL_BUFFER_LINES);

  lv_disp_drv_init(&lvDisplayDriver);
  lvDisplayDriver.hor_res = LCD_WIDTH;
  lvDisplayDriver.ver_res = LCD_HEIGHT;
  lvDisplayDriver.flush_cb = lvFlushCallback;
  lvDisplayDriver.draw_buf = &lvDrawBuffer;
  lv_disp_drv_register(&lvDisplayDriver);

  lv_indev_drv_init(&lvTouchDriver);
  lvTouchDriver.type = LV_INDEV_TYPE_POINTER;
  lvTouchDriver.read_cb = lvTouchReadCallback;
  lv_indev_drv_register(&lvTouchDriver);

  s3_ui_init();
  lastLvTickMs = millis();
  lastLvglHandlerMs = lastLvTickMs;
}

void setupEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(S3_ESPNOW_TX_POWER);
  WiFi.channel(ESPNOW_CHANNEL);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMac, sizeof(receiverMac));
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  esp_now_del_peer(receiverMac);
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("ESP-NOW add peer failed");
  }

  esp_now_register_send_cb([](const uint8_t *, esp_now_send_status_t) {});
  esp_now_register_recv_cb([](const uint8_t *, const uint8_t *data, int len) {
    if (len != sizeof(StatusPacket)) {
      return;
    }
    const StatusPacket *pkt = (const StatusPacket *)data;
    if (pkt->head != 0x5A || pkt->type != 0x02) {
      return;
    }
    if (calcChecksum((const uint8_t *)pkt, sizeof(StatusPacket)) != pkt->checksum) {
      return;
    }
    rssiValue = pkt->rssi;
    receiverVoltageX100 = pkt->voltage;
    receiverSpeed = pkt->speed;
    lastRecvTime = millis();
    connected = true;
  });

  Serial.print("S3 transmitter MAC: ");
  Serial.println(WiFi.macAddress());
  startControlSendTask();
}

void sendControlPacket() {
  ControlPacket pkt = {};
  pkt.head = 0xA5;
  pkt.type = 0x01;
  pkt.throttle = settingsMode ? 0 : joystickValue;
  pkt.speedLevel = speedLevel;
  pkt.buttons = buttonState;
  pkt.checksum = calcChecksum((const uint8_t *)&pkt, sizeof(pkt));
  esp_now_send(receiverMac, (const uint8_t *)&pkt, sizeof(pkt));
}

void controlSendTask(void *) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    sendControlPacket();
    const uint16_t intervalMs = connected ? CONTROL_CONNECTED_INTERVAL_MS : CONTROL_SEARCH_INTERVAL_MS;
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(intervalMs));
  }
}

void startControlSendTask() {
#if S3_ESPNOW_CONTROL_TASK_ENABLED
  if (controlTaskHandle != nullptr) {
    return;
  }
  xTaskCreatePinnedToCore(controlSendTask,
                          "s3_ctrl_tx",
                          S3_ESPNOW_CONTROL_TASK_STACK_WORDS,
                          nullptr,
                          S3_ESPNOW_CONTROL_TASK_PRIORITY,
                          &controlTaskHandle,
                          0);
#endif
}

void updateConnectionState() {
  if (millis() - lastRecvTime > CONNECTION_TIMEOUT_MS) {
    connected = false;
  }
  if (!connected && millis() - lastLinkAlertMs > 1000) {
    lastLinkAlertMs = millis();
    beep(BEEP_FREQ_LINK_ALERT, 80);
  }
}

void updateBatteryAlert() {
  if (!cw2015.valid) {
    return;
  }
  if (cw2015.soc <= LOW_BATTERY_THRESHOLD && !lowBatteryWarned) {
    lowBatteryWarned = true;
    beep(BEEP_FREQ_LOW_BATTERY, 300);
  }
  if (cw2015.soc <= CRITICAL_BATTERY_THRESHOLD) {
    static uint32_t lastCriticalMs = 0;
    if (millis() - lastCriticalMs > 1000) {
      lastCriticalMs = millis();
      beep(BEEP_FREQ_LOW_BATTERY, 500);
    }
  }
  if (cw2015.soc > LOW_BATTERY_THRESHOLD + 5) {
    lowBatteryWarned = false;
  }
}

void updateDashboard() {
  const uint32_t now = millis();
  const uint32_t elapsedMs = now - lastDisplayFpsSampleMs;
  if (elapsedMs >= S3_DISPLAY_FPS_SAMPLE_INTERVAL_MS) {
    displayFps = displayFpsForFrameCount(displayFrameCount, elapsedMs);
    displayFrameCount = 0;
    lastDisplayFpsSampleMs = now;
  }

  S3UiState state = {};
  state.connected = connected;
  state.receiverVoltageX100 = receiverVoltageX100;
  state.speedLevel = speedLevel;
  state.joystickValue = joystickValue;
  state.receiverSpeed = receiverSpeed;
  state.rssiValue = rssiValue;
  state.buttonState = buttonState;
  state.cw2015Valid = cw2015.valid;
  state.cw2015Voltage = cw2015.voltage;
  state.cw2015Soc = cw2015.soc;
  state.bmp280Valid = bmp280.valid;
  state.bmp280TemperatureC = bmp280.temperatureC;
  state.bmp280PressureHpa = bmp280.pressureHpa;
  state.bmp280AltitudeM = bmp280.altitudeM;
  state.qmcValid = qmc.valid;
  state.qmcHeadingDeg = qmc.headingDeg;
  state.displayFps = displayFps;
  state.armed = transmitterArmed;
  state.settingsMode = settingsMode;
  state.joystickCenter = joystickCenter;
  s3_ui_update(state);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(160);
  delay(1000);
  Serial.println();
  Serial.println("ESP32-S3 formal transmitter");
  Serial.printf("S3 power profile: CPU %uMHz, LCD brightness %u, WiFi TX %.1fdBm\n",
                getCpuFrequencyMhz(),
                LCD_BRIGHTNESS,
                S3_ESPNOW_TX_POWER_DBM_X4 / 4.0f);

  setupPins();
  calibrateJoystickCenter();
  setupDisplay();
  setupLvgl();
  initLocalSensors();
  readLocalSensors();
  setupEspNow();

  beep(BEEP_FREQ_STARTUP, 100);
  delay(120);
  beep(BEEP_FREQ_STARTUP, 100);
  updateDashboard();
}

void loop() {
  readInputs();
  if (joystickCenterAdjust != 0) {
    joystickCenter = clampInt(joystickCenter + joystickCenterAdjust, 1, 4094);
    joystickCenterAdjust = 0;
    transmitterArmed = false;
    armBrakeHolding = false;
    joystickValue = 0;
  }
  if (joystickCalibrateRequested) {
    joystickCalibrateRequested = false;
    joystickValue = 0;
    calibrateJoystickCenter();
    transmitterArmed = false;
    armBrakeHolding = false;
  }

  const uint32_t now = millis();
  const uint32_t lvElapsedMs = now - lastLvTickMs;
  if (lvElapsedMs > 0) {
    lv_tick_inc(lvElapsedMs);
    lastLvTickMs = now;
  }

  if (now - lastLocalSensorMs >= LOCAL_SENSOR_INTERVAL_MS) {
    lastLocalSensorMs = now;
    readLocalSensors();
    updateBatteryAlert();
  }

  updateConnectionState();

  if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
    lastDisplayMs = now;
    updateDashboard();
  }

  if (now - lastLvglHandlerMs >= S3_LVGL_HANDLER_INTERVAL_MS) {
    lastLvglHandlerMs = now;
    lv_timer_handler();
  }

  if (now - lastDebugMs >= 1000) {
    lastDebugMs = now;
    Serial.printf("S3 THR:%d SPD:%u BTN:%02X %s BAT:%s\n",
                  joystickValue,
                  speedLevel,
                  buttonState,
                  connected ? "[OK]" : "[LOST]",
                  cw2015.valid ? "OK" : "N/A");
  }

  delay(S3_MAIN_LOOP_DELAY_MS);
}
