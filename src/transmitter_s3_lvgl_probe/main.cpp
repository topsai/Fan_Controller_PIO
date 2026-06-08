#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Wire.h>
#include <math.h>

namespace {

constexpr int LCD_POWER_PIN = 41;
constexpr int LCD_POWER_ACTIVE_LEVEL = LOW;
constexpr int LCD_TE_PIN = 47;
constexpr int LCD_RST_PIN = 8;
constexpr int AUX_I2C_SDA_PIN = 18;
constexpr int AUX_I2C_SCL_PIN = 19;
constexpr uint32_t AUX_I2C_FREQ_HZ = 300000;

constexpr uint8_t CW2015_ADDR = 0x62;
constexpr uint8_t BMP280_ADDR_0 = 0x76;
constexpr uint8_t BMP280_ADDR_1 = 0x77;
constexpr uint8_t LSM6DSLTR_ADDR_0 = 0x6A;
constexpr uint8_t LSM6DSLTR_ADDR_1 = 0x6B;
constexpr uint8_t QMC5883L_ADDR = 0x0D;

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

S3RoundDisplay display;
uint32_t lastUiMs = 0;
uint32_t lastTouchPrintMs = 0;
uint32_t lastSensorPrintMs = 0;
uint32_t invalidTouchCount = 0;
int lastTouchX = -1;
int lastTouchY = -1;
bool auxI2cFound[128] = {};

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
  float altitudeM = NAN;
  Bmp280Cal cal;
};

struct Lsm6dsltrData {
  bool present = false;
  bool valid = false;
  uint8_t addr = 0;
  float ax = NAN;
  float ay = NAN;
  float az = NAN;
  float gx = NAN;
  float gy = NAN;
  float gz = NAN;
};

struct Qmc5883lData {
  bool present = false;
  bool valid = false;
  int16_t mx = 0;
  int16_t my = 0;
  int16_t mz = 0;
  float headingDeg = NAN;
};

Cw2015Data cw2015;
Bmp280Data bmp280;
Lsm6dsltrData lsm6;
Qmc5883lData qmc;

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
  const size_t readLen = Wire1.requestFrom((int)addr, (int)len);
  if (readLen != len) {
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

void scanAuxI2c() {
  memset(auxI2cFound, 0, sizeof(auxI2cFound));
  Serial.println("AUX I2C scan on SDA=18 SCL=19");
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
  if (!cw2015.present) {
    return;
  }
  i2cWriteReg(CW2015_ADDR, 0x0A, 0x00);
  Serial.println("CW2015 present");
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
    if (!auxI2cFound[addr]) {
      continue;
    }
    uint8_t id = 0;
    if (i2cReadU8(addr, 0xD0, id) && id == 0x58 && loadBmp280Cal(addr, bmp280.cal)) {
      bmp280.present = true;
      bmp280.addr = addr;
      i2cWriteReg(addr, 0xF4, 0x27);
      i2cWriteReg(addr, 0xF5, 0xA0);
      Serial.printf("BMP280 present at 0x%02X\n", addr);
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
  bmp280.altitudeM = 44330.0f * (1.0f - powf(bmp280.pressureHpa / 1013.25f, 0.1903f));
  bmp280.valid = !isnan(bmp280.pressureHpa);
}

void initLsm6dsltr() {
  const uint8_t candidates[] = {LSM6DSLTR_ADDR_0, LSM6DSLTR_ADDR_1};
  for (uint8_t addr : candidates) {
    if (!auxI2cFound[addr]) {
      continue;
    }
    uint8_t id = 0;
    if (i2cReadU8(addr, 0x0F, id) && id == 0x6A) {
      lsm6.present = true;
      lsm6.addr = addr;
      i2cWriteReg(addr, 0x12, 0x44);
      i2cWriteReg(addr, 0x10, 0x40);
      i2cWriteReg(addr, 0x11, 0x40);
      Serial.printf("LSM6DSLTR present at 0x%02X\n", addr);
      return;
    }
  }
}

void readLsm6dsltr() {
  lsm6.valid = false;
  if (!lsm6.present) {
    return;
  }
  uint8_t data[12] = {};
  if (!i2cReadBytes(lsm6.addr, 0x22, data, sizeof(data))) {
    return;
  }
  const int16_t gxRaw = s16le(&data[0]);
  const int16_t gyRaw = s16le(&data[2]);
  const int16_t gzRaw = s16le(&data[4]);
  const int16_t axRaw = s16le(&data[6]);
  const int16_t ayRaw = s16le(&data[8]);
  const int16_t azRaw = s16le(&data[10]);
  lsm6.gx = gxRaw * 0.00875f;
  lsm6.gy = gyRaw * 0.00875f;
  lsm6.gz = gzRaw * 0.00875f;
  lsm6.ax = axRaw * 0.000061f;
  lsm6.ay = ayRaw * 0.000061f;
  lsm6.az = azRaw * 0.000061f;
  lsm6.valid = true;
}

void initQmc5883l() {
  qmc.present = auxI2cFound[QMC5883L_ADDR];
  if (!qmc.present) {
    return;
  }
  i2cWriteReg(QMC5883L_ADDR, 0x0B, 0x01);
  i2cWriteReg(QMC5883L_ADDR, 0x09, 0x1D);
  Serial.println("QMC5883L present");
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

void initAuxSensors() {
  Wire1.begin(AUX_I2C_SDA_PIN, AUX_I2C_SCL_PIN, AUX_I2C_FREQ_HZ);
  scanAuxI2c();
  initCw2015();
  initBmp280();
  initLsm6dsltr();
  initQmc5883l();
}

void readAuxSensors() {
  readCw2015();
  readBmp280();
  readLsm6dsltr();
  readQmc5883l();
  if (millis() - lastSensorPrintMs > 2000) {
    lastSensorPrintMs = millis();
    Serial.printf("SENS CW:%d", cw2015.valid);
    if (cw2015.valid) {
      Serial.printf(" %.3fV %.1f%%", cw2015.voltage, cw2015.soc);
    }
    Serial.printf(" | BMP:%d", bmp280.valid);
    if (bmp280.valid) {
      Serial.printf(" %.2fC %.2fhPa %.0fm", bmp280.temperatureC, bmp280.pressureHpa, bmp280.altitudeM);
    }
    Serial.printf(" | LSM:%d", lsm6.valid);
    if (lsm6.valid) {
      Serial.printf(" A %.2f %.2f %.2fg G %.1f %.1f %.1fdps", lsm6.ax, lsm6.ay, lsm6.az, lsm6.gx, lsm6.gy, lsm6.gz);
    }
    Serial.printf(" | QMC:%d", qmc.valid);
    if (qmc.valid) {
      Serial.printf(" %d %d %d %.0fdeg", qmc.mx, qmc.my, qmc.mz, qmc.headingDeg);
    }
    Serial.println();
  }
}

void printValueOrNa(float value, const char *unit, uint8_t decimals = 1) {
  if (isnan(value)) {
    display.print("N/A");
    return;
  }
  display.printf("%.*f%s", decimals, value, unit);
}

void drawSensorDashboard() {
  display.fillScreen(TFT_BLACK);
  display.drawCircle(120, 120, 118, TFT_DARKGREEN);
  display.setTextDatum(top_left);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(1);

  display.setCursor(52, 8);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.print("S3 SENSOR DASH");

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(18, 28);
  display.print("CW ");
  if (cw2015.valid) {
    display.printf("%.2fV %.1f%%", cw2015.voltage, cw2015.soc);
  } else {
    display.print(cw2015.present ? "read N/A" : "not found");
  }

  display.setCursor(18, 47);
  display.print("BMP ");
  if (bmp280.valid) {
    display.printf("%.1fC %.1fhPa", bmp280.temperatureC, bmp280.pressureHpa);
  } else {
    display.print(bmp280.present ? "read N/A" : "not found");
  }

  display.setCursor(18, 66);
  display.print("ALT ");
  printValueOrNa(bmp280.valid ? bmp280.altitudeM : NAN, "m", 0);

  display.setCursor(18, 88);
  display.print("ACC ");
  if (lsm6.valid) {
    display.printf("%.2f %.2f %.2fg", lsm6.ax, lsm6.ay, lsm6.az);
  } else {
    display.print(lsm6.present ? "read N/A" : "not found");
  }

  display.setCursor(18, 107);
  display.print("GYR ");
  if (lsm6.valid) {
    display.printf("%.0f %.0f %.0fd", lsm6.gx, lsm6.gy, lsm6.gz);
  } else {
    display.print(lsm6.present ? "read N/A" : "not found");
  }

  display.setCursor(18, 129);
  display.print("MAG ");
  if (qmc.valid) {
    display.printf("%d %d %d", qmc.mx, qmc.my, qmc.mz);
  } else {
    display.print(qmc.present ? "read N/A" : "not found");
  }

  display.setCursor(18, 148);
  display.print("HDG ");
  printValueOrNa(qmc.valid ? qmc.headingDeg : NAN, "deg", 0);

  display.setCursor(18, 171);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.printf("I2C %02X %02X %02X/%02X %02X/%02X %02X",
                 CW2015_ADDR,
                 bmp280.addr ? bmp280.addr : BMP280_ADDR_0,
                 LSM6DSLTR_ADDR_0,
                 LSM6DSLTR_ADDR_1,
                 BMP280_ADDR_0,
                 BMP280_ADDR_1,
                 QMC5883L_ADDR);

  display.setCursor(70, 213);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.printf("ms %lu", (unsigned long)millis());
}

void updateTouchUi(int x, int y) {
  display.fillRect(50, 194, 140, 14, TFT_BLACK);
  display.setTextDatum(top_left);
  display.setTextColor(TFT_MAGENTA, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(70, 196);
  display.printf("TOUCH %d,%d", x, y);

  lastTouchX = x;
  lastTouchY = y;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=================================");
  Serial.println("ESP32-S3 LVGL hardware probe");
  Serial.println("Display: GC9A01 240x240 8080 8-bit");
  Serial.println("Touch: CST816 I2C");
  Serial.println("AUX I2C: CW2015 BMP280 LSM6DSLTR QMC5883L");
  Serial.println("=================================");

  pinMode(LCD_POWER_PIN, OUTPUT);
  digitalWrite(LCD_POWER_PIN, LCD_POWER_ACTIVE_LEVEL);
  pinMode(LCD_RST_PIN, INPUT_PULLUP);
  pinMode(LCD_TE_PIN, INPUT);

  display.init();
  display.setRotation(0);
  display.setBrightness(255);
  initAuxSensors();
  readAuxSensors();
  drawSensorDashboard();

  Serial.println("Display init done. Dashboard is running.");
}

void loop() {
  uint16_t rawX = 0;
  uint16_t y = 0;
  const bool touched = display.getTouch(&rawX, &y);

  if (touched) {
    if (rawX >= 240 || y >= 240) {
      invalidTouchCount++;
      if (millis() - lastTouchPrintMs > 250) {
        lastTouchPrintMs = millis();
        Serial.printf("TOUCH invalid raw x=%u y=%u invalid_count=%lu\n", rawX, y, (unsigned long)invalidTouchCount);
      }
      delay(5);
      return;
    }

    const uint16_t x = mapTouchX(rawX);
    if (millis() - lastTouchPrintMs > 80) {
      lastTouchPrintMs = millis();
      Serial.printf("TOUCH raw_x=%u x=%u y=%u\n", rawX, x, y);
    }
    if ((int)x != lastTouchX || (int)y != lastTouchY) {
      updateTouchUi(x, y);
    }
  }

  if (millis() - lastUiMs > 500) {
    lastUiMs = millis();
    readAuxSensors();
    drawSensorDashboard();
  }

  delay(5);
}
