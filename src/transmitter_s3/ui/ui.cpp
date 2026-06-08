#include "ui.h"

#include <lvgl.h>

namespace {

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
lv_obj_t *touchLabel = nullptr;
lv_obj_t *fpsLabel = nullptr;
lv_obj_t *touchDot = nullptr;
lv_obj_t *placeholderLabel = nullptr;

lv_obj_t *createDashboardLabel(int16_t x, int16_t y, lv_color_t color) {
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
  return label;
}

}  // namespace

void ui_init() {
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

  touchLabel = lv_label_create(screen);
  lv_obj_set_pos(touchLabel, 24, 198);
  lv_obj_set_style_text_color(touchLabel, lv_color_hex(0xC0C0C0), 0);
  lv_obj_set_style_text_font(touchLabel, &lv_font_montserrat_10, 0);
  lv_label_set_text(touchLabel, "TOUCH --");

  fpsLabel = lv_label_create(screen);
  lv_obj_set_pos(fpsLabel, 150, 198);
  lv_obj_set_style_text_color(fpsLabel, lv_color_hex(0xC0C0C0), 0);
  lv_obj_set_style_text_font(fpsLabel, &lv_font_montserrat_10, 0);
  lv_label_set_text(fpsLabel, "FPS --");

  touchDot = lv_obj_create(screen);
  lv_obj_set_size(touchDot, 8, 8);
  lv_obj_set_style_radius(touchDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(touchDot, lv_color_hex(0xFF2ED1), 0);
  lv_obj_set_style_bg_opa(touchDot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(touchDot, 0, 0);
  lv_obj_add_flag(touchDot, LV_OBJ_FLAG_HIDDEN);

  placeholderLabel = lv_label_create(screen);
  lv_obj_set_style_text_color(placeholderLabel, lv_color_hex(0xA0A0A0), 0);
  lv_obj_set_style_text_font(placeholderLabel, &lv_font_montserrat_10, 0);
  lv_label_set_text(placeholderLabel, "pins are placeholders");
  lv_obj_align(placeholderLabel, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void ui_update(const S3UiState &state) {
  char buffer[48] = {};

  lv_obj_set_style_text_color(statusLabel, state.connected ? lv_color_hex(0x00FF66) : lv_color_hex(0xFF4040), 0);
  if (state.connected) {
    snprintf(buffer, sizeof(buffer), "OK  VESC %.2fV", state.receiverVoltageX100 / 100.0f);
  } else {
    snprintf(buffer, sizeof(buffer), "LOST");
  }
  lv_label_set_text(statusLabel, buffer);

  snprintf(buffer, sizeof(buffer), "SPD %u  THR %d", state.speedLevel, state.joystickValue);
  lv_label_set_text(controlLabel, buffer);

  snprintf(buffer, sizeof(buffer), "SPEED %u KM", state.receiverSpeed);
  lv_label_set_text(speedLabel, buffer);

  if (state.cw2015Valid) {
    snprintf(buffer, sizeof(buffer), "BAT %.2fV %.1f%%", state.cw2015Voltage, state.cw2015Soc);
  } else {
    snprintf(buffer, sizeof(buffer), "BAT N/A");
  }
  lv_label_set_text(batteryLabel, buffer);

  if (state.bmp280Valid) {
    snprintf(buffer, sizeof(buffer), "BMP %.1fC %.0fhPa", state.bmp280TemperatureC, state.bmp280PressureHpa);
  } else {
    snprintf(buffer, sizeof(buffer), "BMP N/A");
  }
  lv_label_set_text(bmpLabel, buffer);

  if (state.qmcValid) {
    snprintf(buffer, sizeof(buffer), "HDG %.0fdeg", state.qmcHeadingDeg);
  } else {
    snprintf(buffer, sizeof(buffer), "HDG N/A");
  }
  lv_label_set_text(headingLabel, buffer);

  snprintf(buffer, sizeof(buffer), "BTN %02X RSSI %d", state.buttonState, state.rssiValue);
  lv_label_set_text(buttonLabel, buffer);

  lv_label_set_text(barLabel, state.joystickValue >= 0 ? "THR" : "BRK");
  lv_obj_set_style_bg_color(throttleBar,
                            state.joystickValue >= 0 ? lv_color_hex(0x00C853) : lv_color_hex(0xFF9800),
                            LV_PART_INDICATOR);
  lv_bar_set_value(throttleBar, state.joystickValue, LV_ANIM_OFF);

  snprintf(buffer, sizeof(buffer), "FPS %u", state.displayFps);
  lv_label_set_text(fpsLabel, buffer);
}

void ui_set_touch(bool pressed, int16_t x, int16_t y) {
  char buffer[24] = {};
  if (!pressed) {
    lv_label_set_text(touchLabel, "TOUCH --");
    lv_obj_add_flag(touchDot, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  snprintf(buffer, sizeof(buffer), "TOUCH %d,%d", x, y);
  lv_label_set_text(touchLabel, buffer);
  lv_obj_clear_flag(touchDot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(touchDot, x - 4, y - 4);
}
