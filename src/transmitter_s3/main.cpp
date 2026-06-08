#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <math.h>
#include "beep_profiles.h"
#include "control_logic.h"

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
constexpr uint16_t CONNECTION_TIMEOUT_MS = 500;
constexpr uint8_t LCD_BRIGHTNESS = 140;
constexpr uint8_t MAIN_LOOP_DELAY_MS = 5;
constexpr uint8_t LVGL_HANDLER_INTERVAL_MS = 5;
constexpr int JOYSTICK_DEADZONE = 50;
constexpr int ADC_CENTER = 2048;
constexpr uint8_t LOW_BATTERY_THRESHOLD = 20;
constexpr uint8_t CRITICAL_BATTERY_THRESHOLD = 10;

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
  uint8_t addr = 0;
  float temperatureC = NAN;
  float pressureHpa = NAN;
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

lv_obj_t *titleLabel = nullptr;
lv_obj_t *statusLabel = nullptr;
lv_obj_t *controlLabel = nullptr;
lv_obj_t *speedLabel = nullptr;
lv_obj_t *batteryLabel = nullptr;
lv_obj_t *bmpLabel = nullptr;
lv_obj_t *headingLabel = nullptr;
lv_obj_t *buttonLabel = nullptr;
lv_obj_t *barLabel = nullptr;
lv_obj_t *throttleBar = nullptr;
lv_obj_t *placeholderLabel = nullptr;

Cw2015Data cw2015;
Bmp280Data bmp280;
Qmc5883lData qmc;
bool auxI2cFound[128] = {};
uint8_t receiverMac[] = {0xAC, 0xEB, 0xE6, 0x44, 0xC5, 0x90};

int joystickCenter = ADC_CENTER;
int16_t joystickValue = 0;
uint8_t speedLevel = 1;
uint8_t buttonState = 0;
bool connected = false;
uint32_t lastRecvTime = 0;
int16_t rssiValue = -100;
uint16_t receiverVoltageX100 = 0;
uint16_t receiverSpeed = 0;
bool lowBatteryWarned = false;
uint32_t lastControlMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastLocalSensorMs = 0;
uint32_t lastDebugMs = 0;
uint32_t lastLinkAlertMs = 0;
uint32_t lastLvglHandlerMs = 0;

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
  cw2015.valid = false;
  if (!cw2015.present) {
    return;
  }
  uint8_t vcell[2] = {};
  uint8_t soc[2] = {};
  if (!i2cReadBytes(CW2015_ADDR, 0x02, vcell, sizeof(vcell))) {
    return;
  }
  if (!i2cReadBytes(CW2015_ADDR, 0x04, soc, sizeof(soc))) {
    return;
  }
  const uint16_t rawVoltage = ((uint16_t)vcell[0] << 8) | vcell[1];
  cw2015.voltage = rawVoltage * 0.000305f;
  cw2015.soc = soc[0] + soc[1] / 256.0f;
  cw2015.valid = true;
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
  bmp280.valid = false;
  if (!bmp280.present) {
    return;
  }
  uint8_t data[6] = {};
  if (!i2cReadBytes(bmp280.addr, 0xF7, data, sizeof(data))) {
    return;
  }
  const int32_t adcP = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
  const int32_t adcT = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);
  bmp280.temperatureC = compensateBmp280Temperature(bmp280.cal, adcT);
  bmp280.pressureHpa = compensateBmp280Pressure(bmp280.cal, adcP);
  bmp280.valid = !isnan(bmp280.pressureHpa);
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
  joystickValue = joystickToThrottle(analogRead(JOYSTICK_PIN), joystickCenter, JOYSTICK_DEADZONE);

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
  if (!digitalRead(BUTTON_1_PIN)) {
    nextButtons |= 0x01;
  }
  if (!digitalRead(BUTTON_2_PIN)) {
    nextButtons |= 0x02;
  }
  static uint8_t lastButtons = 0;
  if ((nextButtons & ~lastButtons) != 0) {
    beep(BEEP_FREQ_BUTTON, 50);
  }
  lastButtons = nextButtons;
  buttonState = nextButtons;
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
  display.startWrite();
  display.pushImage(area->x1, area->y1, width, height, reinterpret_cast<const uint16_t *>(colorP));
  display.endWrite();
  lv_disp_flush_ready(disp);
}

void lvTouchReadCallback(lv_indev_drv_t *, lv_indev_data_t *data) {
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  if (display.getTouch(&rawX, &rawY) && rawX < LCD_WIDTH && rawY < LCD_HEIGHT) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = clampTouchCoord(mapTouchX(rawX));
    data->point.y = clampTouchCoord(rawY);
    return;
  }
  data->state = LV_INDEV_STATE_RELEASED;
}

lv_obj_t *createDashboardLabel(int16_t x, int16_t y, lv_color_t color) {
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
  return label;
}

void createLvglDashboard() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  titleLabel = lv_label_create(screen);
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(0x00D8FF), 0);
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_14, 0);
  lv_label_set_text(titleLabel, "S3 REMOTE");
  lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 8);

  statusLabel = createDashboardLabel(24, 30, lv_color_hex(0x00FF66));
  controlLabel = createDashboardLabel(24, 52, lv_color_white());
  speedLabel = createDashboardLabel(24, 72, lv_color_white());
  batteryLabel = createDashboardLabel(24, 94, lv_color_white());
  bmpLabel = createDashboardLabel(24, 114, lv_color_white());
  headingLabel = createDashboardLabel(24, 134, lv_color_white());
  buttonLabel = createDashboardLabel(24, 156, lv_color_white());
  barLabel = createDashboardLabel(24, 178, lv_color_hex(0xC0C0C0));

  throttleBar = lv_bar_create(screen);
  lv_obj_set_size(throttleBar, 96, 10);
  lv_obj_set_pos(throttleBar, 56, 180);
  lv_bar_set_range(throttleBar, -1000, 1000);
  lv_bar_set_mode(throttleBar, LV_BAR_MODE_SYMMETRICAL);
  lv_obj_set_style_bg_color(throttleBar, lv_color_hex(0x303030), LV_PART_MAIN);
  lv_obj_set_style_bg_color(throttleBar, lv_color_hex(0x00C853), LV_PART_INDICATOR);

  placeholderLabel = lv_label_create(screen);
  lv_obj_set_style_text_color(placeholderLabel, lv_color_hex(0xA0A0A0), 0);
  lv_obj_set_style_text_font(placeholderLabel, &lv_font_montserrat_10, 0);
  lv_label_set_text(placeholderLabel, "pins are placeholders");
  lv_obj_align(placeholderLabel, LV_ALIGN_BOTTOM_MID, 0, -18);
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

  createLvglDashboard();
  lastLvTickMs = millis();
  lastLvglHandlerMs = lastLvTickMs;
}

void setupEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.channel(1);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMac, sizeof(receiverMac));
  peer.channel = 0;
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
}

void sendControlPacket() {
  ControlPacket pkt = {};
  pkt.head = 0xA5;
  pkt.type = 0x01;
  pkt.throttle = joystickValue;
  pkt.speedLevel = speedLevel;
  pkt.buttons = buttonState;
  pkt.checksum = calcChecksum((const uint8_t *)&pkt, sizeof(pkt));
  esp_now_send(receiverMac, (const uint8_t *)&pkt, sizeof(pkt));
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
  char buffer[48] = {};

  lv_obj_set_style_text_color(statusLabel, connected ? lv_color_hex(0x00FF66) : lv_color_hex(0xFF4040), 0);
  if (connected) {
    snprintf(buffer, sizeof(buffer), "OK  VESC %.2fV", receiverVoltageX100 / 100.0f);
  } else {
    snprintf(buffer, sizeof(buffer), "LOST");
  }
  lv_label_set_text(statusLabel, buffer);

  snprintf(buffer, sizeof(buffer), "SPD %u  THR %d", speedLevel, joystickValue);
  lv_label_set_text(controlLabel, buffer);

  snprintf(buffer, sizeof(buffer), "SPEED %u KM", receiverSpeed);
  lv_label_set_text(speedLabel, buffer);

  if (cw2015.valid) {
    snprintf(buffer, sizeof(buffer), "BAT %.2fV %.1f%%", cw2015.voltage, cw2015.soc);
  } else {
    snprintf(buffer, sizeof(buffer), "BAT N/A");
  }
  lv_label_set_text(batteryLabel, buffer);

  if (bmp280.valid) {
    snprintf(buffer, sizeof(buffer), "BMP %.1fC %.0fhPa", bmp280.temperatureC, bmp280.pressureHpa);
  } else {
    snprintf(buffer, sizeof(buffer), "BMP N/A");
  }
  lv_label_set_text(bmpLabel, buffer);

  if (qmc.valid) {
    snprintf(buffer, sizeof(buffer), "HDG %.0fdeg", qmc.headingDeg);
  } else {
    snprintf(buffer, sizeof(buffer), "HDG N/A");
  }
  lv_label_set_text(headingLabel, buffer);

  snprintf(buffer, sizeof(buffer), "BTN %02X RSSI %d", buttonState, rssiValue);
  lv_label_set_text(buttonLabel, buffer);

  lv_label_set_text(barLabel, joystickValue >= 0 ? "THR" : "BRK");
  lv_obj_set_style_bg_color(throttleBar,
                            joystickValue >= 0 ? lv_color_hex(0x00C853) : lv_color_hex(0xFF9800),
                            LV_PART_INDICATOR);
  lv_bar_set_value(throttleBar, joystickValue, LV_ANIM_OFF);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(160);
  delay(1000);
  Serial.println();
  Serial.println("ESP32-S3 formal transmitter");
  Serial.printf("S3 power profile: CPU %uMHz, LCD brightness %u, WiFi TX 8.5dBm\n",
                getCpuFrequencyMhz(),
                LCD_BRIGHTNESS);

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

  const uint32_t now = millis();
  const uint32_t lvElapsedMs = now - lastLvTickMs;
  if (lvElapsedMs > 0) {
    lv_tick_inc(lvElapsedMs);
    lastLvTickMs = now;
  }

  const uint16_t controlIntervalMs = connected ? CONTROL_CONNECTED_INTERVAL_MS : CONTROL_SEARCH_INTERVAL_MS;
  if (now - lastControlMs >= controlIntervalMs) {
    lastControlMs = now;
    sendControlPacket();
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

  if (now - lastLvglHandlerMs >= LVGL_HANDLER_INTERVAL_MS) {
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

  delay(MAIN_LOOP_DELAY_MS);
}
