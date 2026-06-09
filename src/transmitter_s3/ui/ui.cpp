#include "ui.h"

#include "generated/ui.h"
#include "s3_ui_bindings.h"
#include <lvgl.h>

namespace {

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
    lv_label_set_text(controlValueLabel(), "1");
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

  if (state.connected) {
    snprintf(buffer, sizeof(buffer), "OK");
  } else {
    snprintf(buffer, sizeof(buffer), "LOST");
  }
  if (statusValueLabel() != nullptr) {
    lv_label_set_text(statusValueLabel(), buffer);
    lv_obj_set_style_text_color(statusValueLabel(),
                                state.connected ? lv_color_hex(0x00D86A) : lv_color_hex(0xFF4040),
                                LV_PART_MAIN);
  }

  if (controlValueLabel() != nullptr) {
    snprintf(buffer, sizeof(buffer), "%u", state.speedLevel);
    lv_label_set_text(controlValueLabel(), buffer);
  }

  if (speedValueLabel() != nullptr) {
    snprintf(buffer, sizeof(buffer), "%u", state.receiverSpeed);
    lv_label_set_text(speedValueLabel(), buffer);
    lv_obj_set_style_text_color(speedValueLabel(), lv_color_hex(s3SpeedColorHex(state.receiverSpeed)), LV_PART_MAIN);
  }

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

  if (bmp280ValueLabel() != nullptr) {
    if (state.bmp280Valid) {
      snprintf(buffer, sizeof(buffer), "%.0fhPa %.0fm", state.bmp280PressureHpa, state.bmp280AltitudeM);
    } else {
      snprintf(buffer, sizeof(buffer), "BMP N/A");
    }
    lv_label_set_text(bmp280ValueLabel(), buffer);
  }

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
}

void s3_ui_set_touch(bool pressed, int16_t x, int16_t y) {
  (void)pressed;
  (void)x;
  (void)y;
}
