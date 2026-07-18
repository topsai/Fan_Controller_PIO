#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <math.h>
#include "beep_profiles.h"
#include "control_logic.h"
#include "diagnostic_protocol.h"
#include "protocol.h"
#include "s3_runtime_config.h"
#include "s3_ui_bindings.h"
#include "ui/ui.h"
#ifdef FAN_CONTROLLER_HIL
#include "hil_protocol.h"
#include "hil_safety.h"
#endif

namespace {

constexpr const char *S3_FIRMWARE_VERSION = "s3-remote-ui";

#if defined(S3_NEW_PCB_PINOUT) && S3_NEW_PCB_PINOUT
constexpr int LCD_POWER_PIN = 47;
constexpr int LCD_POWER_ACTIVE_LEVEL = LOW;
constexpr int LCD_TE_PIN = 37;
constexpr int LCD_RST_PIN = 8;

constexpr int AUX_I2C_SDA_PIN = 39;
constexpr int AUX_I2C_SCL_PIN = 38;
constexpr uint32_t AUX_I2C_FREQ_HZ = 300000;

constexpr int JOYSTICK_PIN = 1;
constexpr int SPEED_LEVEL_ADC_PIN = 2;
constexpr int BUTTON_1_PIN = 0;
constexpr int BUTTON_2_PIN = 46;
constexpr int BUZZER_PIN = 40;
#else
constexpr int LCD_POWER_PIN = 41;
constexpr int LCD_POWER_ACTIVE_LEVEL = LOW;
constexpr int LCD_TE_PIN = 47;
constexpr int LCD_RST_PIN = 8;

constexpr int AUX_I2C_SDA_PIN = 18;
constexpr int AUX_I2C_SCL_PIN = 19;
constexpr uint32_t AUX_I2C_FREQ_HZ = 300000;

constexpr int JOYSTICK_PIN = 1;
constexpr int SWITCH_PIN_1 = 37;
constexpr int SWITCH_PIN_2 = 38;
constexpr int SWITCH_PIN_3 = 39;
constexpr int BUTTON_1_PIN = 35;
constexpr int BUTTON_2_PIN = 36;
constexpr int BUZZER_PIN = 42;
#endif

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
constexpr uint8_t LCD_DIM_BRIGHTNESS = 45;
constexpr uint32_t DISPLAY_DIM_AFTER_MS = 30000;
constexpr float MCU_TEMPERATURE_WARN_C = 75.0f;
constexpr int JOYSTICK_DEADZONE = 50;
constexpr int ADC_CENTER = 2048;
constexpr const char *CALIBRATION_NAMESPACE = "joy_cal";
constexpr const char *CALIBRATION_CENTER_KEY = "center";
constexpr const char *CALIBRATION_MIN_KEY = "min";
constexpr const char *CALIBRATION_MAX_KEY = "max";
constexpr const char *CALIBRATION_DEADZONE_KEY = "deadzone";
constexpr const char *DISPLAY_NAMESPACE = "display";
constexpr const char *DISPLAY_BRIGHTNESS_KEY = "brightness";
constexpr int ARM_BRAKE_THRESHOLD = -900;
constexpr uint32_t ARM_BRAKE_HOLD_MS = 3000;
constexpr int THROTTLE_SLEW_STEP = 40;
constexpr int JOYSTICK_CENTER_ADJUST_STEP = 10;
constexpr uint8_t LOW_BATTERY_THRESHOLD = 20;
constexpr uint8_t CRITICAL_BATTERY_THRESHOLD = 10;
constexpr uint32_t TAKEOVER_LONG_PRESS_MS = 3000;
constexpr uint32_t TAKEOVER_REQUEST_WINDOW_MS = 1000;

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
  uint8_t version;
  uint16_t sequence;
  int16_t throttle;
  uint8_t speedLevel;
  uint8_t buttons;
  uint8_t flags;
  uint8_t crc;
};

struct StatusPacket {
  uint8_t head;
  uint8_t type;
  uint8_t version;
  uint16_t sequence;
  int16_t rssi;
  uint16_t voltage;
  uint8_t motorPWM[4];
  uint16_t speed;
  uint8_t status;
  uint8_t crc;
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
Preferences preferences;
constexpr uint16_t LCD_WIDTH = 240;
constexpr uint16_t LCD_HEIGHT = 240;
constexpr uint16_t LVGL_BUFFER_LINES = 40;
lv_disp_draw_buf_t lvDrawBuffer;
lv_color_t lvBuffer[LCD_WIDTH * LVGL_BUFFER_LINES];
lv_disp_drv_t lvDisplayDriver;
lv_indev_drv_t lvTouchDriver;
uint32_t lastLvTickMs = 0;

Cw2015Data cw2015;
Bmp280Data bmp280;
Qmc5883lData qmc;
float mcuTemperatureC = NAN;
bool auxI2cFound[128] = {};
uint8_t receiverMac[] = {0xAC, 0xEB, 0xE6, 0x44, 0xC5, 0x90};

int joystickCenter = ADC_CENTER;
JoystickCalibration joystickCalibration = {ADC_CENTER, 0, 4095, JOYSTICK_DEADZONE};
int joystickAdcRaw = ADC_CENTER;
uint16_t speedAdcRaw = 0;
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
ButtonLongPressState button1LongPressState = {};
uint32_t takeoverRequestUntilMs = 0;
bool connected = false;
uint32_t lastRecvTime = 0;
int16_t rssiValue = -100;
uint16_t receiverVoltageX100 = 0;
uint16_t receiverSpeed = 0;
uint8_t receiverStatusFlags = 0;
uint16_t controlSequence = 0;
uint16_t lastStatusSequence = 0;
bool hasStatusSequence = false;
uint32_t statusPacketCounter = 0;
uint32_t lastStatusRateSampleMs = 0;
uint16_t statusPacketRateHz = 0;
uint16_t statusLostPackets = 0;
uint32_t diagnosticStatusPackets = 0;
uint32_t diagnosticFaultCount = 0;
bool lowBatteryWarned = false;
uint32_t lastDisplayMs = 0;
uint32_t lastLocalSensorMs = 0;
uint32_t lastDebugMs = 0;
uint32_t lastLinkAlertMs = 0;
bool everReceivedStatusPacket = false;
uint32_t lastLvglHandlerMs = 0;
uint32_t lastUserActivityMs = 0;
uint8_t userBrightness = LCD_BRIGHTNESS;
uint8_t currentBrightness = LCD_BRIGHTNESS;
TaskHandle_t controlTaskHandle = nullptr;

#ifdef FAN_CONTROLLER_HIL
static constexpr uint32_t HIL_OUTPUT_WATCHDOG_MS = 10000;
HilOutputGate hilOutputGate = hilInitialOutputGate(0);
bool hilJoystickPhysical = true;
int hilJoystickRaw = ADC_CENTER;
bool hilSpeedPhysical = true;
uint8_t hilSpeedLevel = 1;
bool hilButton1Down = false;
bool hilButton2Down = false;
uint32_t hilButton1ReleaseAt = 0;
uint32_t hilButton2ReleaseAt = 0;
uint32_t hilLastSequence = 0;
char hilLastError[24] = "ok";
enum HilSensorMode : uint8_t { HIL_SENSOR_PHYSICAL, HIL_SENSOR_VALUE, HIL_SENSOR_FAULT };
HilSensorMode hilCwMode = HIL_SENSOR_PHYSICAL;
HilSensorMode hilBmpMode = HIL_SENSOR_PHYSICAL;
HilSensorMode hilCompassMode = HIL_SENSOR_PHYSICAL;
HilSensorMode hilMcuMode = HIL_SENSOR_PHYSICAL;
float hilCwValue = 0.0f;
float hilBmpValue = 0.0f;
float hilCompassValue = 0.0f;
float hilMcuValue = 0.0f;
#endif

void startControlSendTask();
bool processStatusFrame(const uint8_t *data, int len);

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
#ifdef FAN_CONTROLLER_HIL
  hilOutputGate.expectedBuzzer = true;
  if (!hilActualBuzzer(hilOutputGate)) {
    noTone(BUZZER_PIN);
    return;
  }
#endif
  tone(BUZZER_PIN, frequencyHz, durationMs);
}

void setDisplayBrightness(uint8_t brightness) {
  currentBrightness = s3ClampUserBrightness(brightness);
  display.setBrightness(currentBrightness);
}

uint8_t loadUserBrightness() {
  preferences.begin(DISPLAY_NAMESPACE, true);
  const int storedBrightness = preferences.getInt(DISPLAY_BRIGHTNESS_KEY, -1);
  preferences.end();
  const uint8_t brightness = s3ResolveStoredBrightness(storedBrightness, LCD_BRIGHTNESS);
  Serial.printf("S3 display brightness loaded: %u\n", brightness);
  return brightness;
}

void saveUserBrightness() {
  preferences.begin(DISPLAY_NAMESPACE, false);
  preferences.putInt(DISPLAY_BRIGHTNESS_KEY, userBrightness);
  preferences.end();
  Serial.printf("S3 display brightness saved: %u\n", userBrightness);
}

void markUserActivity() {
  lastUserActivityMs = millis();
  if (currentBrightness != userBrightness) {
    setDisplayBrightness(userBrightness);
  }
}

void updateDisplayPower() {
  if (settingsMode || millis() - lastUserActivityMs < DISPLAY_DIM_AFTER_MS) {
    return;
  }
  const uint8_t targetBrightness = userBrightness < LCD_DIM_BRIGHTNESS ? userBrightness : LCD_DIM_BRIGHTNESS;
  if (currentBrightness != targetBrightness) {
    setDisplayBrightness(targetBrightness);
  }
}

void saveJoystickCalibration() {
  preferences.begin(CALIBRATION_NAMESPACE, false);
  preferences.putInt(CALIBRATION_CENTER_KEY, joystickCalibration.center);
  preferences.putInt(CALIBRATION_MIN_KEY, joystickCalibration.minRaw);
  preferences.putInt(CALIBRATION_MAX_KEY, joystickCalibration.maxRaw);
  preferences.putInt(CALIBRATION_DEADZONE_KEY, joystickCalibration.deadzone);
  preferences.end();
  Serial.printf("S3 joystick calibration saved: center=%d min=%d max=%d deadzone=%d\n",
                joystickCalibration.center,
                joystickCalibration.minRaw,
                joystickCalibration.maxRaw,
                joystickCalibration.deadzone);
}

bool loadJoystickCalibration() {
  preferences.begin(CALIBRATION_NAMESPACE, true);
  JoystickCalibration stored = {
    preferences.getInt(CALIBRATION_CENTER_KEY, ADC_CENTER),
    preferences.getInt(CALIBRATION_MIN_KEY, 0),
    preferences.getInt(CALIBRATION_MAX_KEY, 4095),
    preferences.getInt(CALIBRATION_DEADZONE_KEY, JOYSTICK_DEADZONE),
  };
  preferences.end();
  if (!joystickCalibrationIsValid(stored)) {
    return false;
  }
  joystickCalibration = stored;
  joystickCenter = stored.center;
  Serial.printf("S3 joystick calibration loaded: center=%d min=%d max=%d deadzone=%d\n",
                stored.center,
                stored.minRaw,
                stored.maxRaw,
                stored.deadzone);
  return true;
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
  mcuTemperatureC = temperatureRead();
  readCw2015();
  readBmp280();
  readQmc5883l();
#ifdef FAN_CONTROLLER_HIL
  if (hilCwMode == HIL_SENSOR_VALUE) { cw2015.present = true; cw2015.valid = true; cw2015.voltage = hilCwValue; cw2015.soc = constrain(hilCwValue * 20.0f, 0.0f, 100.0f); }
  else if (hilCwMode == HIL_SENSOR_FAULT) { cw2015.present = true; cw2015.valid = false; }
  if (hilBmpMode == HIL_SENSOR_VALUE) { bmp280.present = true; bmp280.valid = true; bmp280.temperatureC = hilBmpValue; bmp280.pressureHpa = 1013.25f; bmp280.altitudeM = 0.0f; }
  else if (hilBmpMode == HIL_SENSOR_FAULT) { bmp280.present = true; bmp280.valid = false; }
  if (hilCompassMode == HIL_SENSOR_VALUE) { qmc.present = true; qmc.valid = true; qmc.headingDeg = hilCompassValue; }
  else if (hilCompassMode == HIL_SENSOR_FAULT) { qmc.present = true; qmc.valid = false; }
  if (hilMcuMode == HIL_SENSOR_VALUE) mcuTemperatureC = hilMcuValue;
  else if (hilMcuMode == HIL_SENSOR_FAULT) mcuTemperatureC = NAN;
#endif
}

void setupPins() {
#if defined(S3_NEW_PCB_PINOUT) && S3_NEW_PCB_PINOUT
  pinMode(SPEED_LEVEL_ADC_PIN, INPUT);
#else
  pinMode(SWITCH_PIN_1, INPUT_PULLUP);
  pinMode(SWITCH_PIN_2, INPUT_PULLUP);
  pinMode(SWITCH_PIN_3, INPUT_PULLUP);
#endif
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
  joystickCalibration.center = joystickCenter;
  joystickCalibration.minRaw = 0;
  joystickCalibration.maxRaw = 4095;
  joystickCalibration.deadzone = JOYSTICK_DEADZONE;
  Serial.printf("S3 joystick center: %d\n", joystickCenter);
  saveJoystickCalibration();
}

void readInputs() {
  joystickAdcRaw =
#ifdef FAN_CONTROLLER_HIL
    hilJoystickPhysical ? analogRead(JOYSTICK_PIN) : hilJoystickRaw;
#else
    analogRead(JOYSTICK_PIN);
#endif
  const int16_t targetThrottle = joystickToThrottleCalibrated(joystickAdcRaw, joystickCalibration);
  if (abs(targetThrottle) > JOYSTICK_DEADZONE) {
    markUserActivity();
  }
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

#if defined(S3_NEW_PCB_PINOUT) && S3_NEW_PCB_PINOUT
  if (
#ifdef FAN_CONTROLLER_HIL
      !hilSpeedPhysical
#else
      false
#endif
  ) {
#ifdef FAN_CONTROLLER_HIL
    speedLevel = hilSpeedLevel;
    speedAdcRaw = speedLevel == 1 ? 0 : (speedLevel == 2 ? 2048 : 4095);
#endif
  } else {
    speedAdcRaw = (uint16_t)analogRead(SPEED_LEVEL_ADC_PIN);
    speedLevel = s3SpeedLevelFromAdc(speedAdcRaw);
  }
#else
  const bool sw1 = !digitalRead(SWITCH_PIN_1);
  const bool sw2 = !digitalRead(SWITCH_PIN_2);
  const bool sw3 = !digitalRead(SWITCH_PIN_3);
  speedAdcRaw = sw3 ? 4095 : (sw2 ? 2048 : (sw1 ? 0 : 0));
  if (sw3) {
    speedLevel = 3;
  } else if (sw2) {
    speedLevel = 2;
  } else {
    speedLevel = 1;
  }
#ifdef FAN_CONTROLLER_HIL
  if (!hilSpeedPhysical) speedLevel = hilSpeedLevel;
#endif
#endif

  uint8_t nextButtons = 0;
  const bool button1Down =
#ifdef FAN_CONTROLLER_HIL
    hilButton1Down || !digitalRead(BUTTON_1_PIN);
#else
    !digitalRead(BUTTON_1_PIN);
#endif
  const bool button2Down =
#ifdef FAN_CONTROLLER_HIL
    hilButton2Down || !digitalRead(BUTTON_2_PIN);
#else
    !digitalRead(BUTTON_2_PIN);
#endif
  if (button2Down) {
    nextButtons |= 0x02;
  }
  static uint8_t lastButtons = 0;
  const ButtonPressEvent button1Event = buttonLongPressUpdate(button1Down, millis(), TAKEOVER_LONG_PRESS_MS, button1LongPressState);
  if (button1Event == BUTTON_PRESS_SHORT) {
    settingsMode = !settingsMode;
    markUserActivity();
    transmitterArmed = false;
    armBrakeHolding = false;
    joystickValue = 0;
    beep(BEEP_FREQ_BUTTON, 50);
  } else if (button1Event == BUTTON_PRESS_LONG) {
    takeoverRequestUntilMs = millis() + TAKEOVER_REQUEST_WINDOW_MS;
    markUserActivity();
    transmitterArmed = false;
    armBrakeHolding = false;
    joystickValue = 0;
    beep(BEEP_FREQ_CONNECTED, 80);
  }

  if ((nextButtons & ~lastButtons) != 0 && !settingsMode) {
    markUserActivity();
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
  setDisplayBrightness(userBrightness);
  lastUserActivityMs = millis();
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
  lv_disp_flush_ready(disp);
}

void handleS3TouchAction(S3UiTouchAction action) {
  switch (action) {
    case S3UiTouchAction::CalibrateCenter:
      joystickCalibrateRequested = true;
      markUserActivity();
      beep(BEEP_FREQ_BUTTON, 50);
      break;
    case S3UiTouchAction::CenterMinus:
      joystickCenterAdjust -= JOYSTICK_CENTER_ADJUST_STEP;
      markUserActivity();
      beep(BEEP_FREQ_BUTTON, 30);
      break;
    case S3UiTouchAction::CenterPlus:
      joystickCenterAdjust += JOYSTICK_CENTER_ADJUST_STEP;
      markUserActivity();
      beep(BEEP_FREQ_BUTTON, 30);
      break;
    case S3UiTouchAction::ResetCalibration:
      joystickCalibration = {ADC_CENTER, 0, 4095, JOYSTICK_DEADZONE};
      joystickCenter = ADC_CENTER;
      saveJoystickCalibration();
      markUserActivity();
      transmitterArmed = false;
      armBrakeHolding = false;
      joystickValue = 0;
      beep(BEEP_FREQ_BUTTON, 50);
      break;
    case S3UiTouchAction::CloseSettings:
      settingsMode = false;
      markUserActivity();
      transmitterArmed = false;
      armBrakeHolding = false;
      joystickValue = 0;
      beep(BEEP_FREQ_BUTTON, 50);
      break;
    case S3UiTouchAction::PageChanged:
      markUserActivity();
      beep(BEEP_FREQ_BUTTON, 30);
      break;
    case S3UiTouchAction::None:
    default:
      break;
  }
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
    handleS3TouchAction(s3_ui_set_touch(true, x, y));
    return;
  }
  data->state = LV_INDEV_STATE_RELEASED;
  handleS3TouchAction(s3_ui_set_touch(false, 0, 0));
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
    processStatusFrame(data, len);
  });

  Serial.print("S3 transmitter MAC: ");
  Serial.println(WiFi.macAddress());
  startControlSendTask();
}

bool processStatusFrame(const uint8_t *data, int len) {
    if (len != sizeof(StatusPacket)) {
      diagnosticFaultCount++;
      return false;
    }
    const StatusPacket *pkt = (const StatusPacket *)data;
    if (pkt->head != STATUS_PACKET_HEAD || pkt->type != STATUS_PACKET_TYPE || pkt->version != STATUS_PROTOCOL_VERSION) {
      diagnosticFaultCount++;
      return false;
    }
    if (protocolCrc8((const uint8_t *)pkt, sizeof(StatusPacket) - 1) != pkt->crc) {
      diagnosticFaultCount++;
      return false;
    }
    if (!protocolSequenceIsFresh(pkt->sequence, lastStatusSequence, hasStatusSequence)) {
      diagnosticFaultCount++;
      return false;
    }
    if (hasStatusSequence) {
      const uint16_t delta = (uint16_t)(pkt->sequence - lastStatusSequence);
      if (delta > 1) {
        statusLostPackets = (uint16_t)clampInt((int)statusLostPackets + (int)delta - 1, 0, 65535);
      }
    }
    protocolRememberSequence(pkt->sequence, lastStatusSequence, hasStatusSequence);
    statusPacketCounter++;
    diagnosticStatusPackets++;
    rssiValue = pkt->rssi;
    receiverVoltageX100 = pkt->voltage;
    receiverSpeed = pkt->speed;
    receiverStatusFlags = pkt->status;
    lastRecvTime = millis();
    connected = true;
    everReceivedStatusPacket = true;
    return true;
}

void sendControlPacket() {
  ControlPacket pkt = {};
  pkt.head = CONTROL_PACKET_HEAD;
  pkt.type = CONTROL_PACKET_TYPE;
  pkt.version = CONTROL_PROTOCOL_VERSION;
  pkt.sequence = controlSequence++;
  pkt.throttle = settingsMode ? 0 : joystickValue;
  pkt.speedLevel = speedLevel;
  pkt.buttons = buttonState;
  pkt.flags = transmitterArmed ? 0 : STATUS_FLAG_OUTPUT_LOCKED;
  if (millis() < takeoverRequestUntilMs) {
    pkt.flags |= CONTROL_FLAG_TAKEOVER_REQUEST;
  }
  pkt.crc = protocolCrc8((const uint8_t *)&pkt, sizeof(pkt) - 1);
#ifdef FAN_CONTROLLER_HIL
  if (!hilOutputGate.unlocked) return;
#endif
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
    resetSequenceAfterConnectionTimeout(connected, hasStatusSequence, lastStatusSequence);
    statusLostPackets = 0;
  }
  const bool standbyMode = !connected && !transmitterArmed && !settingsMode && millis() >= takeoverRequestUntilMs;
  if (!connected && !standbyMode && everReceivedStatusPacket && millis() - lastLinkAlertMs > 1000) {
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
  const uint32_t rateElapsedMs = now - lastStatusRateSampleMs;
  if (rateElapsedMs >= 1000) {
    statusPacketRateHz = (uint16_t)((statusPacketCounter * 1000UL + rateElapsedMs / 2UL) / rateElapsedMs);
    statusPacketCounter = 0;
    lastStatusRateSampleMs = now;
  }

  S3UiState state = {};
  state.connected = connected;
  state.receiverVoltageX100 = receiverVoltageX100;
  state.speedLevel = speedLevel;
  state.joystickValue = joystickValue;
  state.receiverSpeed = receiverSpeed;
  state.cw2015Valid = cw2015.valid;
  state.cw2015Soc = cw2015.soc;
  state.bmp280Valid = bmp280.valid;
  state.bmp280PressureHpa = bmp280.pressureHpa;
  state.bmp280AltitudeM = bmp280.altitudeM;
  state.qmcValid = qmc.valid;
  state.qmcHeadingDeg = qmc.headingDeg;
  state.mcuTemperatureC = mcuTemperatureC;
  state.mcuTemperatureWarning = s3McuTemperatureWarns(isfinite(mcuTemperatureC), mcuTemperatureC, MCU_TEMPERATURE_WARN_C);
  state.rssiValue = rssiValue;
  state.receiverStatusFlags = receiverStatusFlags;
  state.statusPacketRateHz = statusPacketRateHz;
  state.statusLostPackets = statusLostPackets;
  state.controlSequence = controlSequence;
  state.lastStatusSequence = lastStatusSequence;
  state.displayBrightness = currentBrightness;
  state.displayDimmed = currentBrightness == LCD_DIM_BRIGHTNESS;
  state.takeoverActive = millis() < takeoverRequestUntilMs;
  state.standbyMode = !connected && !transmitterArmed && !settingsMode && !state.takeoverActive;
  state.armed = transmitterArmed;
  state.settingsMode = settingsMode;
  state.joystickCenter = joystickCenter;
  state.joystickRawAdc = joystickAdcRaw;
  state.joystickMinRaw = joystickCalibration.minRaw;
  state.joystickMaxRaw = joystickCalibration.maxRaw;
  state.joystickDeadzone = joystickCalibration.deadzone;
  state.speedAdcRaw = speedAdcRaw;
  state.firmwareVersion = S3_FIRMWARE_VERSION;
  state.buildDate = __DATE__;
  s3_ui_update(state);
}

void printDiagnosticStatus() {
  char line[96] = {};
  diagnosticFormatStatusLine(line,
                             sizeof(line),
                             "s3_transmitter",
                             connected,
                             diagnosticStatusPackets,
                             statusLostPackets,
                             diagnosticFaultCount);
  Serial.println(line);
}

void handleDiagnosticCommand(const String &line) {
  if (line == "DIAG PING") {
    Serial.println("DIAG PONG role=s3_transmitter protocol=2");
    return;
  }
  if (line == "DIAG STATUS") {
    printDiagnosticStatus();
    return;
  }
#ifdef FAN_CONTROLLER_HIL
  if (line.startsWith("DIAG SIMSTATUS ")) {
    int rssi = -60;
    int voltage = 4800;
    int speed = 0;
    int status = 0;
    if (sscanf(line.c_str(), "DIAG SIMSTATUS %d %d %d %d", &rssi, &voltage, &speed, &status) != 4) {
      Serial.println("DIAG ERR simstatus");
      diagnosticFaultCount++;
      return;
    }
    rssiValue = (int16_t)rssi;
    receiverVoltageX100 = (uint16_t)clampInt(voltage, 0, 65535);
    receiverSpeed = (uint16_t)clampInt(speed, 0, 65535);
    receiverStatusFlags = (uint8_t)clampInt(status, 0, 255);
    lastRecvTime = millis();
    connected = true;
    everReceivedStatusPacket = true;
    statusPacketCounter++;
    diagnosticStatusPackets++;
    Serial.println("DIAG OK simstatus");
    return;
  }
#endif
  if (line.length() > 0) {
    Serial.println("DIAG ERR unknown");
    diagnosticFaultCount++;
  }
}

void updateDiagnosticSerial() {
  static String line;
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      handleDiagnosticCommand(line);
      line = "";
    } else if (line.length() < 96) {
      line += c;
    } else {
      line = "";
      diagnosticFaultCount++;
      Serial.println("DIAG ERR overflow");
    }
  }
}

#ifdef FAN_CONTROLLER_HIL
void sendHilAck(uint32_t sequence, bool ok, const char *error = nullptr) {
  Serial.printf("{\"type\":\"ack\",\"sequence\":%lu,\"ok\":%s",
                static_cast<unsigned long>(sequence), ok ? "true" : "false");
  if (error != nullptr) Serial.printf(",\"error\":\"%s\"", error);
  Serial.println("}");
}

bool decodeHilHex(const char *text, uint8_t *output, size_t capacity, size_t &length) {
  length = 0;
  const size_t textLength = text == nullptr ? 0 : strlen(text);
  if (textLength == 0 || (textLength & 1u) != 0 || textLength / 2 > capacity) return false;
  for (size_t i = 0; i < textLength; i += 2) {
    char pair[3] = {text[i], text[i + 1], '\0'};
    char *end = nullptr;
    const unsigned long value = strtoul(pair, &end, 16);
    if (end != pair + 2 || value > 255) return false;
    output[length++] = static_cast<uint8_t>(value);
  }
  return true;
}

const char *hilSensorModeName(HilSensorMode mode) {
  return mode == HIL_SENSOR_PHYSICAL ? "physical" : (mode == HIL_SENSOR_VALUE ? "value" : "fault");
}

void sendHilStatus(uint32_t sequence) {
  Serial.printf("{\"type\":\"status\",\"sequence\":%lu,\"ok\":true,"
                "\"firmware\":\"%s\",\"protocol\":%u,\"role\":\"s3_transmitter\","
                "\"uptime_ms\":%lu,\"last_sequence\":%lu,\"last_result\":\"%s\",",
                static_cast<unsigned long>(sequence), S3_FIRMWARE_VERSION, HIL_PROTOCOL_VERSION,
                static_cast<unsigned long>(millis()), static_cast<unsigned long>(hilLastSequence), hilLastError);
  Serial.printf("\"outputs_unlocked\":%s,\"connected\":%s,\"armed\":%s,\"settings_mode\":%s,"
                "\"input_modes\":{\"joystick\":\"%s\",\"speed\":\"%s\",\"cw2015\":\"%s\","
                "\"bmp280\":\"%s\",\"compass\":\"%s\",\"mcu\":\"%s\"},",
                hilOutputGate.unlocked ? "true" : "false", connected ? "true" : "false",
                transmitterArmed ? "true" : "false", settingsMode ? "true" : "false",
                hilJoystickPhysical ? "physical" : "value", hilSpeedPhysical ? "physical" : "value",
                hilSensorModeName(hilCwMode), hilSensorModeName(hilBmpMode), hilSensorModeName(hilCompassMode),
                hilSensorModeName(hilMcuMode));
  Serial.printf("\"joystick\":{\"adc\":%d,\"raw\":%d,\"output\":%d,\"center\":%d,"
                "\"min\":%d,\"max\":%d,\"deadzone\":%d},\"speed_level\":%u,\"buttons\":%u,",
                joystickAdcRaw, joystickRawValue, joystickValue, joystickCalibration.center,
                joystickCalibration.minRaw, joystickCalibration.maxRaw, joystickCalibration.deadzone,
                speedLevel, buttonState);
  Serial.printf("\"receiver\":{\"rssi\":%d,\"voltage_x100\":%u,\"speed\":%u,\"status_flags\":%u,"
                "\"packets\":%lu,\"lost\":%u},",
                rssiValue, receiverVoltageX100, receiverSpeed, receiverStatusFlags,
                static_cast<unsigned long>(diagnosticStatusPackets), statusLostPackets);
  Serial.printf("\"sensors\":{\"battery_valid\":%s,\"battery_voltage\":%.3f,\"battery_percent\":%.1f,"
                "\"bmp_valid\":%s,\"bmp_temperature\":%.2f,\"pressure_hpa\":%.2f,"
                "\"compass_valid\":%s,\"heading\":%.2f,\"mcu_temperature\":%.2f},",
                cw2015.valid ? "true" : "false", cw2015.voltage, cw2015.soc,
                bmp280.valid ? "true" : "false", bmp280.temperatureC, bmp280.pressureHpa,
                qmc.valid ? "true" : "false", qmc.headingDeg, mcuTemperatureC);
  Serial.printf("\"display\":{\"brightness\":%u,\"dimmed\":%s},"
                "\"expected_outputs\":{\"radio_send\":true,\"buzzer\":%s},"
                "\"actual_outputs\":{\"radio_send\":%s,\"buzzer\":%s}}\n",
                currentBrightness, currentBrightness == LCD_DIM_BRIGHTNESS ? "true" : "false",
                hilOutputGate.expectedBuzzer ? "true" : "false", hilOutputGate.unlocked ? "true" : "false",
                hilActualBuzzer(hilOutputGate) ? "true" : "false");
}

HilSensorMode *sensorModeForName(const char *name, float *&value) {
  if (strcmp(name, "CW2015") == 0) { value = &hilCwValue; return &hilCwMode; }
  if (strcmp(name, "BMP280") == 0) { value = &hilBmpValue; return &hilBmpMode; }
  if (strcmp(name, "COMPASS") == 0 || strcmp(name, "QMC5883L") == 0) { value = &hilCompassValue; return &hilCompassMode; }
  if (strcmp(name, "MCU") == 0) { value = &hilMcuValue; return &hilMcuMode; }
  value = nullptr;
  return nullptr;
}

void handleHilCommand(const HilCommand &command) {
  hilLastSequence = command.sequence;
  strcpy(hilLastError, "ok");
  hilOutputGate.lastCommandAt = millis();
  switch (command.type) {
    case HIL_COMMAND_PING: sendHilAck(command.sequence, true); return;
    case HIL_COMMAND_STATUS: sendHilStatus(command.sequence); return;
    case HIL_COMMAND_OUTPUTS_LOCK:
      hilSetOutputsUnlocked(hilOutputGate, false, millis()); noTone(BUZZER_PIN); sendHilAck(command.sequence, true); return;
    case HIL_COMMAND_OUTPUTS_UNLOCK:
      hilSetOutputsUnlocked(hilOutputGate, true, millis()); sendHilAck(command.sequence, true); return;
    case HIL_COMMAND_INPUT_JOYSTICK:
      if (strcmp(command.text, "PHYSICAL") == 0 || strcmp(command.text, "physical") == 0) hilJoystickPhysical = true;
      else if (command.values[0] >= 0 && command.values[0] <= 4095) { hilJoystickPhysical = false; hilJoystickRaw = command.values[0]; }
      else { sendHilAck(command.sequence, false, "invalid_argument"); return; }
      sendHilAck(command.sequence, true); return;
    case HIL_COMMAND_INPUT_SPEED:
      if (strcmp(command.text, "PHYSICAL") == 0 || strcmp(command.text, "physical") == 0) hilSpeedPhysical = true;
      else if (command.values[0] >= 1 && command.values[0] <= 3) { hilSpeedPhysical = false; hilSpeedLevel = command.values[0]; }
      else { sendHilAck(command.sequence, false, "invalid_argument"); return; }
      sendHilAck(command.sequence, true); return;
    case HIL_COMMAND_BUTTON_CLICK:
    case HIL_COMMAND_BUTTON_HOLD: {
      const uint32_t duration = command.type == HIL_COMMAND_BUTTON_CLICK ? 80u : static_cast<uint32_t>(command.values[0]);
      if (duration < 40 || duration > 30000) { sendHilAck(command.sequence, false, "invalid_argument"); return; }
      if (strcmp(command.text, "1") == 0) { hilButton1Down = true; hilButton1ReleaseAt = millis() + duration; }
      else if (strcmp(command.text, "2") == 0) { hilButton2Down = true; hilButton2ReleaseAt = millis() + duration; }
      else { sendHilAck(command.sequence, false, "invalid_argument"); return; }
      sendHilAck(command.sequence, true); return;
    }
    case HIL_COMMAND_STATUS_FRAME: {
      uint8_t bytes[sizeof(StatusPacket)] = {};
      size_t length = 0;
      if (!decodeHilHex(command.data, bytes, sizeof(bytes), length) || !processStatusFrame(bytes, static_cast<int>(length))) {
        sendHilAck(command.sequence, false, "frame_rejected"); return;
      }
      sendHilAck(command.sequence, true); return;
    }
    case HIL_COMMAND_SENSOR_PHYSICAL:
    case HIL_COMMAND_SENSOR_VALUE:
    case HIL_COMMAND_SENSOR_FAULT: {
      float *value = nullptr;
      HilSensorMode *mode = sensorModeForName(command.text, value);
      if (mode == nullptr) { sendHilAck(command.sequence, false, "unsupported"); return; }
      if (command.type == HIL_COMMAND_SENSOR_PHYSICAL) *mode = HIL_SENSOR_PHYSICAL;
      else if (command.type == HIL_COMMAND_SENSOR_FAULT) *mode = HIL_SENSOR_FAULT;
      else { *mode = HIL_SENSOR_VALUE; *value = command.realValues[0]; }
      readLocalSensors();
      sendHilAck(command.sequence, true); return;
    }
    case HIL_COMMAND_NVS_CLEAR:
      preferences.begin(DISPLAY_NAMESPACE, false); preferences.clear(); preferences.end();
      preferences.begin(CALIBRATION_NAMESPACE, false); preferences.clear(); preferences.end();
      sendHilAck(command.sequence, true); return;
    case HIL_COMMAND_RESET:
      hilSetOutputsUnlocked(hilOutputGate, false, millis()); hilJoystickPhysical = true; hilSpeedPhysical = true;
      hilButton1Down = false; hilButton2Down = false; hilCwMode = hilBmpMode = hilCompassMode = hilMcuMode = HIL_SENSOR_PHYSICAL;
      transmitterArmed = false; joystickValue = 0; noTone(BUZZER_PIN); sendHilAck(command.sequence, true); return;
    case HIL_COMMAND_REBOOT:
      hilSetOutputsUnlocked(hilOutputGate, false, millis()); noTone(BUZZER_PIN); sendHilAck(command.sequence, true);
      Serial.flush(); delay(50); ESP.restart(); return;
    default: strcpy(hilLastError, "unsupported"); sendHilAck(command.sequence, false, "unsupported"); return;
  }
}

void pollHilSerial() {
  static char line[HIL_MAX_LINE_LENGTH] = {};
  static size_t length = 0;
  static bool discarding = false;
  while (Serial.available() > 0) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r') continue;
    if (value == '\n') {
      if (discarding) { discarding = false; length = 0; sendHilAck(0, false, "line_too_long"); continue; }
      if (length == 0) { sendHilAck(0, false, "empty"); continue; }
      line[length] = '\0';
      HilCommand command = {};
      const HilParseResult result = hilParseCommand(line, command);
      length = 0;
      if (result == HIL_PARSE_OK) handleHilCommand(command);
      else { strncpy(hilLastError, hilParseError(result), sizeof(hilLastError) - 1); sendHilAck(command.sequence, false, hilParseError(result)); }
      continue;
    }
    if (discarding) continue;
    if (length + 1 >= sizeof(line)) { discarding = true; continue; }
    line[length++] = value;
  }
  const uint32_t now = millis();
  if (hilButton1Down && static_cast<int32_t>(now - hilButton1ReleaseAt) >= 0) hilButton1Down = false;
  if (hilButton2Down && static_cast<int32_t>(now - hilButton2ReleaseAt) >= 0) hilButton2Down = false;
  if (hilApplyOutputWatchdog(hilOutputGate, now, HIL_OUTPUT_WATCHDOG_MS)) noTone(BUZZER_PIN);
}
#endif

}  // namespace

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(160);
  delay(1000);
  Serial.println();
  Serial.println("ESP32-S3 formal transmitter");
  userBrightness = loadUserBrightness();
  currentBrightness = userBrightness;
  Serial.printf("S3 power profile: CPU %uMHz, LCD brightness %u, WiFi TX %.1fdBm\n",
                getCpuFrequencyMhz(),
                userBrightness,
                S3_ESPNOW_TX_POWER_DBM_X4 / 4.0f);

  setupPins();
  if (!loadJoystickCalibration()) {
    calibrateJoystickCenter();
  }
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
#ifdef FAN_CONTROLLER_HIL
  pollHilSerial();
#else
  updateDiagnosticSerial();
#endif
  readInputs();
  if (joystickCenterAdjust != 0) {
    joystickCenter = adjustedJoystickCenter(joystickCenter, joystickCenterAdjust);
    joystickCalibration.center = joystickCenter;
    saveJoystickCalibration();
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

  uint8_t requestedBrightness = 0;
  if (s3_ui_consume_brightness_request(requestedBrightness)) {
    const uint8_t nextBrightness = s3ClampUserBrightness(requestedBrightness);
    const bool brightnessChanged = nextBrightness != userBrightness;
    userBrightness = nextBrightness;
    setDisplayBrightness(userBrightness);
    if (brightnessChanged) {
      saveUserBrightness();
    }
    markUserActivity();
  }

  updateDisplayPower();

  if (now - lastDebugMs >= 1000) {
    lastDebugMs = now;
    const bool standbyMode = !connected && !transmitterArmed && !settingsMode && millis() >= takeoverRequestUntilMs;
    Serial.printf("S3 THR:%d SPD:%u BTN:%02X %s BAT:%s\n",
                  joystickValue,
                  speedLevel,
                  buttonState,
                  connected ? "[OK]" : (standbyMode ? "[STBY]" : "[LOST]"),
                  cw2015.valid ? "OK" : "N/A");
  }

  delay(S3_MAIN_LOOP_DELAY_MS);
}
