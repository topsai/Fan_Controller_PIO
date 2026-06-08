#include <Arduino.h>
#include <LovyanGFX.hpp>
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

constexpr uint16_t CONTROL_INTERVAL_MS = 10;
constexpr uint16_t DISPLAY_INTERVAL_MS = 100;
constexpr uint16_t LOCAL_SENSOR_INTERVAL_MS = 500;
constexpr uint16_t CONNECTION_TIMEOUT_MS = 500;
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
  display.setBrightness(255);
}

void setupEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_15dBm);
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

void drawDashboard() {
  display.fillScreen(TFT_BLACK);
  display.drawCircle(120, 120, 118, connected ? TFT_GREEN : TFT_RED);
  display.setTextDatum(top_left);
  display.setTextSize(1);

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(58, 8);
  display.print("S3 REMOTE");

  display.setTextColor(connected ? TFT_GREEN : TFT_RED, TFT_BLACK);
  display.setCursor(24, 28);
  if (connected) {
    display.printf("OK  VESC %.2fV", receiverVoltageX100 / 100.0f);
  } else {
    display.print("LOST");
  }

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(24, 50);
  display.printf("SPD %u  THR %d", speedLevel, joystickValue);

  display.setCursor(24, 70);
  display.printf("SPEED %u KM", receiverSpeed);

  display.setCursor(24, 92);
  if (cw2015.valid) {
    display.printf("BAT %.2fV %.1f%%", cw2015.voltage, cw2015.soc);
  } else {
    display.print("BAT N/A");
  }

  display.setCursor(24, 112);
  if (bmp280.valid) {
    display.printf("BMP %.1fC %.0fhPa", bmp280.temperatureC, bmp280.pressureHpa);
  } else {
    display.print("BMP N/A");
  }

  display.setCursor(24, 132);
  if (qmc.valid) {
    display.printf("HDG %.0fdeg", qmc.headingDeg);
  } else {
    display.print("HDG N/A");
  }

  display.setCursor(24, 154);
  display.printf("BTN %02X RSSI %d", buttonState, rssiValue);

  const char *direction = joystickValue >= 0 ? "THR" : "BRK";
  const int barWidth = map(abs(joystickValue), 0, 1000, 0, 92);
  display.setCursor(24, 176);
  display.print(direction);
  display.drawRect(52, 174, 96, 10, TFT_DARKGREY);
  display.fillRect(54, 176, barWidth, 6, joystickValue >= 0 ? TFT_GREEN : TFT_ORANGE);

  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setCursor(46, 208);
  display.print("pins are placeholders");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("ESP32-S3 formal transmitter");

  setupPins();
  calibrateJoystickCenter();
  setupDisplay();
  initLocalSensors();
  readLocalSensors();
  setupEspNow();

  beep(BEEP_FREQ_STARTUP, 100);
  delay(120);
  beep(BEEP_FREQ_STARTUP, 100);
  drawDashboard();
}

void loop() {
  readInputs();

  const uint32_t now = millis();
  if (now - lastControlMs >= CONTROL_INTERVAL_MS) {
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
    drawDashboard();
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

  delay(1);
}
