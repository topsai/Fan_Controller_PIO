#include "ui.h"

#include "generated/ui.h"
#include "s3_ui_bindings.h"
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

lv_obj_t *batteryArc() {
  return ui_ArcBattery;
}

lv_obj_t *batteryValueLabel() {
  return ui_LabelBattery;
}

lv_obj_t *speedValueLabel() {
  return ui_LabelSpeed;
}

lv_obj_t *statusValueLabel() {
  return ui_LabelStatus;
}

lv_obj_t *controlValueLabel() {
  return ui_LabelControl;
}

lv_obj_t *bmp280ValueLabel() {
  return ui_LabelBmp280;
}

lv_obj_t *throttleValueBar() {
  return ui_BarThrottle;
}

lv_obj_t *statusVoltageArc() {
  return ui_ArcStatusVoltage;
}

lv_obj_t *statusVoltageLabel() {
  return ui_LabelStatusVoltage;
}

}  // namespace

void s3_ui_init() {
  ui_init();

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

  if (batteryArc() != nullptr) {
    lv_arc_set_range(batteryArc(), 0, 100);
    lv_arc_set_value(batteryArc(), 0);
    lv_obj_clear_flag(batteryArc(), LV_OBJ_FLAG_CLICKABLE);
  }

  if (batteryValueLabel() != nullptr) {
    lv_label_set_text(batteryValueLabel(), "--%");
  }

  if (speedValueLabel() != nullptr) {
    lv_label_set_text(speedValueLabel(), "0");
    lv_obj_set_style_text_color(speedValueLabel(), lv_color_hex(s3SpeedColorHex(0)), LV_PART_MAIN);
  }

  if (statusValueLabel() != nullptr) {
    lv_label_set_text(statusValueLabel(), "LOST");
    lv_obj_set_style_text_color(statusValueLabel(), lv_color_hex(0xFF4040), LV_PART_MAIN);
  }

  if (controlValueLabel() != nullptr) {
    lv_label_set_text(controlValueLabel(), "SPD 1");
  }

  if (bmp280ValueLabel() != nullptr) {
    lv_label_set_text(bmp280ValueLabel(), "BMP N/A");
  }

  if (throttleValueBar() != nullptr) {
    lv_bar_set_range(throttleValueBar(), -1000, 1000);
    lv_bar_set_mode(throttleValueBar(), LV_BAR_MODE_SYMMETRICAL);
    lv_bar_set_value(throttleValueBar(), 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(throttleValueBar(), lv_color_hex(0x303030), LV_PART_MAIN);
    lv_obj_set_style_bg_color(throttleValueBar(), lv_color_hex(s3ThrottleColorHex(0)), LV_PART_INDICATOR);
  }

  if (statusVoltageArc() != nullptr) {
    lv_arc_set_range(statusVoltageArc(), 6, 48);
    lv_arc_set_value(statusVoltageArc(), 6);
    lv_obj_clear_flag(statusVoltageArc(), LV_OBJ_FLAG_CLICKABLE);
  }

  if (statusVoltageLabel() != nullptr) {
    lv_label_set_text(statusVoltageLabel(), "--.-V");
  }
}

void s3_ui_update(const S3UiState &state) {
  char buffer[64] = {};
  const int16_t batteryPercent = s3BatteryPercentForArc(state.cw2015Valid, state.cw2015Soc);
  const int16_t throttleBarValue = s3ThrottleBarValue(state.joystickValue);
  const int16_t statusVoltageValue = s3StatusVoltageForArc(state.connected, state.receiverVoltageX100);

  lv_obj_set_style_text_color(statusLabel, state.connected ? lv_color_hex(0x00FF66) : lv_color_hex(0xFF4040), 0);
  if (state.connected) {
    snprintf(buffer, sizeof(buffer), "OK");
  } else {
    snprintf(buffer, sizeof(buffer), "LOST");
  }
  lv_label_set_text(statusLabel, buffer);
  if (statusValueLabel() != nullptr) {
    lv_label_set_text(statusValueLabel(), buffer);
    lv_obj_set_style_text_color(statusValueLabel(),
                                state.connected ? lv_color_hex(0x00D86A) : lv_color_hex(0xFF4040),
                                LV_PART_MAIN);
  }

  snprintf(buffer, sizeof(buffer), "SPD %u  THR %d", state.speedLevel, state.joystickValue);
  lv_label_set_text(controlLabel, buffer);
  if (controlValueLabel() != nullptr) {
    snprintf(buffer, sizeof(buffer), "SPD %u", state.speedLevel);
    lv_label_set_text(controlValueLabel(), buffer);
  }

  snprintf(buffer, sizeof(buffer), "SPEED %u KM", state.receiverSpeed);
  lv_label_set_text(speedLabel, buffer);
  if (speedValueLabel() != nullptr) {
    snprintf(buffer, sizeof(buffer), "%u", state.receiverSpeed);
    lv_label_set_text(speedValueLabel(), buffer);
    lv_obj_set_style_text_color(speedValueLabel(), lv_color_hex(s3SpeedColorHex(state.receiverSpeed)), LV_PART_MAIN);
  }

  if (state.cw2015Valid) {
    snprintf(buffer, sizeof(buffer), "BAT %.2fV %.1f%%", state.cw2015Voltage, state.cw2015Soc);
  } else {
    snprintf(buffer, sizeof(buffer), "BAT N/A");
  }
  lv_label_set_text(batteryLabel, buffer);

  if (batteryArc() != nullptr) {
    lv_arc_set_value(batteryArc(), batteryPercent);
    lv_obj_set_style_arc_color(batteryArc(),
                               batteryPercent <= 20 ? lv_color_hex(0xFF4040) : lv_color_hex(0x00D86A),
                               LV_PART_INDICATOR);
  }
  if (batteryValueLabel() != nullptr) {
    snprintf(buffer, sizeof(buffer), state.cw2015Valid ? "%d%%" : "--%%", batteryPercent);
    lv_label_set_text(batteryValueLabel(), buffer);
  }

  if (state.bmp280Valid) {
    snprintf(buffer, sizeof(buffer), "BMP %.1fC %.0fhPa", state.bmp280TemperatureC, state.bmp280PressureHpa);
  } else {
    snprintf(buffer, sizeof(buffer), "BMP N/A");
  }
  lv_label_set_text(bmpLabel, buffer);
  if (bmp280ValueLabel() != nullptr) {
    if (state.bmp280Valid) {
      snprintf(buffer, sizeof(buffer), "%.0fhPa %.0fm", state.bmp280PressureHpa, state.bmp280AltitudeM);
    } else {
      snprintf(buffer, sizeof(buffer), "BMP N/A");
    }
    lv_label_set_text(bmp280ValueLabel(), buffer);
  }

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
                            lv_color_hex(s3ThrottleColorHex(state.joystickValue)),
                            LV_PART_INDICATOR);
  lv_bar_set_value(throttleBar, throttleBarValue, LV_ANIM_OFF);
  if (throttleValueBar() != nullptr) {
    lv_obj_set_style_bg_color(throttleValueBar(), lv_color_hex(s3ThrottleColorHex(state.joystickValue)),
                              LV_PART_INDICATOR);
    lv_bar_set_value(throttleValueBar(), throttleBarValue, LV_ANIM_OFF);
  }

  if (statusVoltageArc() != nullptr) {
    lv_arc_set_value(statusVoltageArc(), statusVoltageValue);
    lv_obj_set_style_arc_color(statusVoltageArc(),
                               state.connected ? lv_color_hex(0x00D86A) : lv_color_hex(0x606060),
                               LV_PART_INDICATOR);
  }
  if (statusVoltageLabel() != nullptr) {
    if (state.connected) {
      snprintf(buffer, sizeof(buffer), "%.2fV", state.receiverVoltageX100 / 100.0f);
    } else {
      snprintf(buffer, sizeof(buffer), "--.-V");
    }
    lv_label_set_text(statusVoltageLabel(), buffer);
  }

  snprintf(buffer, sizeof(buffer), "FPS %u", state.displayFps);
  lv_label_set_text(fpsLabel, buffer);
}

void s3_ui_set_touch(bool pressed, int16_t x, int16_t y) {
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
