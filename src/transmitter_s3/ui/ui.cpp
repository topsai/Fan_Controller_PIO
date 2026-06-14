#include "ui.h"

#include "generated/ui.h"
#include "s3_ui_bindings.h"
#include <lvgl.h>

namespace {

lv_obj_t *settingsPanel = nullptr;
lv_obj_t *settingsTitleLabel = nullptr;
lv_obj_t *settingsCenterLabel = nullptr;
lv_obj_t *settingsHintLabel = nullptr;
lv_obj_t *settingsMinusButton = nullptr;
lv_obj_t *settingsPlusButton = nullptr;
lv_obj_t *settingsCalButton = nullptr;
lv_obj_t *settingsCloseButton = nullptr;
bool settingsVisible = false;
bool touchWasPressed = false;

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

lv_obj_t *compassImage() {
  return ui_Compass;
}

lv_obj_t *mcuTemperatureLabel() {
  return ui_MCUTemp;
}

lv_obj_t *createSettingsButton(lv_obj_t *parent, const char *text, int16_t x, int16_t y, int16_t w, int16_t h) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_size(button, w, h);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_style_radius(button, 6, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x202833), LV_PART_MAIN);
  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
  lv_obj_center(label);
  return button;
}

void createSettingsPanel() {
  if (settingsPanel != nullptr) {
    return;
  }

  settingsPanel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(settingsPanel, 210, 170);
  lv_obj_align(settingsPanel, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(settingsPanel, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(settingsPanel, lv_color_hex(0x101820), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(settingsPanel, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_border_color(settingsPanel, lv_color_hex(0x00D8FF), LV_PART_MAIN);
  lv_obj_set_style_border_width(settingsPanel, 1, LV_PART_MAIN);
  lv_obj_clear_flag(settingsPanel, LV_OBJ_FLAG_SCROLLABLE);

  settingsTitleLabel = lv_label_create(settingsPanel);
  lv_label_set_text(settingsTitleLabel, "JOYSTICK SET");
  lv_obj_set_style_text_color(settingsTitleLabel, lv_color_hex(0x00D8FF), LV_PART_MAIN);
  lv_obj_align(settingsTitleLabel, LV_ALIGN_TOP_MID, 0, 8);

  settingsCenterLabel = lv_label_create(settingsPanel);
  lv_label_set_text(settingsCenterLabel, "CENTER 2048");
  lv_obj_set_style_text_color(settingsCenterLabel, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(settingsCenterLabel, LV_ALIGN_TOP_MID, 0, 36);

  settingsMinusButton = createSettingsButton(settingsPanel, "-10", 12, 68, 52, 36);
  settingsCalButton = createSettingsButton(settingsPanel, "CAL", 78, 68, 52, 36);
  settingsPlusButton = createSettingsButton(settingsPanel, "+10", 144, 68, 52, 36);
  settingsCloseButton = createSettingsButton(settingsPanel, "EXIT", 60, 116, 90, 34);

  settingsHintLabel = lv_label_create(settingsPanel);
  lv_label_set_text(settingsHintLabel, "Output locked");
  lv_obj_set_style_text_color(settingsHintLabel, lv_color_hex(0xFFD23F), LV_PART_MAIN);
  lv_obj_align(settingsHintLabel, LV_ALIGN_BOTTOM_MID, 0, -4);

  lv_obj_add_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
}

bool touchInside(lv_obj_t *obj, int16_t x, int16_t y) {
  if (obj == nullptr) {
    return false;
  }
  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  return x >= coords.x1 && x <= coords.x2 && y >= coords.y1 && y <= coords.y2;
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
    lv_obj_set_style_text_color(bmp280ValueLabel(), lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(bmp280ValueLabel(), &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_align(bmp280ValueLabel(), LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
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

  if (compassImage() != nullptr) {
    lv_obj_update_layout(compassImage());
    lv_img_set_pivot(compassImage(), lv_obj_get_width(compassImage()) / 2, lv_obj_get_height(compassImage()) / 2);
    lv_img_set_angle(compassImage(), 0);
    lv_obj_set_style_opa(compassImage(), LV_OPA_50, LV_PART_MAIN);
  }

  if (mcuTemperatureLabel() != nullptr) {
    lv_label_set_text(mcuTemperatureLabel(), "--.-C");
    lv_obj_set_style_text_color(mcuTemperatureLabel(), lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(mcuTemperatureLabel(), &lv_font_montserrat_12, LV_PART_MAIN);
  }

  createSettingsPanel();
}

void s3_ui_update(const S3UiState &state) {
  char buffer[64] = {};
  const int16_t batteryPercent = s3BatteryPercentForArc(state.cw2015Valid, state.cw2015Soc);
  const int16_t throttleBarValue = s3ThrottleBarValue(state.joystickValue);
  const int16_t statusVoltageValue = s3StatusVoltageForArc(state.connected, state.receiverVoltageX100);
  const int16_t compassAngle = s3CompassAngleForHeading(state.qmcValid, state.qmcHeadingDeg);

  s3FormatStatusText(buffer, sizeof(buffer), state.connected, state.armed);
  if (statusValueLabel() != nullptr) {
    lv_label_set_text(statusValueLabel(), buffer);
    const uint32_t color = !state.armed ? 0xFFD23F : (state.connected ? 0x00D86A : 0xFF4040);
    lv_obj_set_style_text_color(statusValueLabel(), lv_color_hex(color), LV_PART_MAIN);
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
      snprintf(buffer, sizeof(buffer), "%.0fhPa\n%.0fm", state.bmp280PressureHpa, state.bmp280AltitudeM);
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

  if (compassImage() != nullptr) {
    lv_img_set_angle(compassImage(), compassAngle);
    lv_obj_set_style_opa(compassImage(), state.qmcValid ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
  }

  if (mcuTemperatureLabel() != nullptr) {
    s3FormatMcuTemperatureText(buffer, sizeof(buffer), state.mcuTemperatureC);
    lv_label_set_text(mcuTemperatureLabel(), buffer);
  }

  s3_ui_set_settings_visible(state.settingsMode);
  if (settingsCenterLabel != nullptr) {
    snprintf(buffer, sizeof(buffer), "CENTER %d", state.joystickCenter);
    lv_label_set_text(settingsCenterLabel, buffer);
  }
  if (settingsHintLabel != nullptr) {
    lv_label_set_text(settingsHintLabel, state.armed ? "Output locked" : "Re-arm after exit");
  }
}

void s3_ui_set_settings_visible(bool visible) {
  settingsVisible = visible;
  if (settingsPanel == nullptr) {
    return;
  }
  if (visible) {
    lv_obj_clear_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settingsPanel);
  } else {
    lv_obj_add_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
  }
}

S3UiTouchAction s3_ui_set_touch(bool pressed, int16_t x, int16_t y) {
  if (!pressed) {
    touchWasPressed = false;
    return S3UiTouchAction::None;
  }
  if (touchWasPressed || !settingsVisible) {
    return S3UiTouchAction::None;
  }
  touchWasPressed = true;

  if (touchInside(settingsCalButton, x, y)) {
    return S3UiTouchAction::CalibrateCenter;
  }
  if (touchInside(settingsMinusButton, x, y)) {
    return S3UiTouchAction::CenterMinus;
  }
  if (touchInside(settingsPlusButton, x, y)) {
    return S3UiTouchAction::CenterPlus;
  }
  if (touchInside(settingsCloseButton, x, y)) {
    return S3UiTouchAction::CloseSettings;
  }
  return S3UiTouchAction::None;
}
