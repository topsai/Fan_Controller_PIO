#include <Arduino.h>
#include <LovyanGFX.hpp>

namespace {

constexpr int LCD_POWER_PIN = 41;
constexpr int LCD_TE_PIN = 47;
constexpr int LCD_RST_PIN = 8;

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
uint32_t invalidTouchCount = 0;
int lastTouchX = -1;
int lastTouchY = -1;

void drawStaticProbe() {
  display.fillScreen(TFT_BLACK);
  display.fillRect(0, 0, 240, 60, TFT_RED);
  display.fillRect(0, 60, 240, 60, TFT_GREEN);
  display.fillRect(0, 120, 240, 60, TFT_BLUE);
  display.fillRect(0, 180, 240, 60, TFT_WHITE);
  display.drawCircle(120, 120, 118, TFT_YELLOW);
  display.drawLine(120, 0, 120, 239, TFT_BLACK);
  display.drawLine(0, 120, 239, 120, TFT_BLACK);

  display.setTextDatum(middle_center);
  display.setTextColor(TFT_BLACK, TFT_WHITE);
  display.setTextSize(2);
  display.drawString("S3 DISPLAY", 120, 198);
  display.drawString("TOUCH TEST", 120, 220);
}

void updateTouchUi(int x, int y) {
  display.fillCircle(lastTouchX, lastTouchY, 8, TFT_BLACK);
  display.fillCircle(x, y, 8, TFT_MAGENTA);
  display.drawCircle(x, y, 12, TFT_BLACK);

  display.fillRect(45, 102, 150, 36, TFT_BLACK);
  display.setTextDatum(middle_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString(String(x) + "," + String(y), 120, 120);

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
  Serial.println("=================================");

  pinMode(LCD_POWER_PIN, OUTPUT);
  digitalWrite(LCD_POWER_PIN, HIGH);
  pinMode(LCD_RST_PIN, INPUT_PULLUP);
  pinMode(LCD_TE_PIN, INPUT);

  display.init();
  display.setRotation(0);
  display.setBrightness(255);
  drawStaticProbe();

  Serial.println("Display init done. Touch the screen to print coordinates.");
}

void loop() {
  uint16_t x = 0;
  uint16_t y = 0;
  const bool touched = display.getTouch(&x, &y);

  if (touched) {
    if (x >= 240 || y >= 240) {
      invalidTouchCount++;
      if (millis() - lastTouchPrintMs > 250) {
        lastTouchPrintMs = millis();
        Serial.printf("TOUCH invalid raw x=%u y=%u invalid_count=%lu\n", x, y, (unsigned long)invalidTouchCount);
      }
      delay(5);
      return;
    }

    if (millis() - lastTouchPrintMs > 80) {
      lastTouchPrintMs = millis();
      Serial.printf("TOUCH x=%u y=%u\n", x, y);
    }
    if ((int)x != lastTouchX || (int)y != lastTouchY) {
      updateTouchUi(x, y);
    }
  }

  if (millis() - lastUiMs > 1000) {
    lastUiMs = millis();
    display.setTextDatum(top_left);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(1);
    display.fillRect(70, 4, 100, 14, TFT_BLACK);
    display.drawString(String("ms ") + millis(), 72, 6);
  }

  delay(5);
}
