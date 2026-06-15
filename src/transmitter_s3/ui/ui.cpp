#include "ui.h"

#include "generated/ui.h"
#include "s3_ui_bindings.h"
#include <lvgl.h>

extern "C" {
extern lv_obj_t *ui_LabelLinkDiag __attribute__((weak));
extern lv_obj_t *ui_LabelRxFlags __attribute__((weak));
extern lv_obj_t *ui_LabelProtocol __attribute__((weak));
extern lv_obj_t *ui_LabelSeq __attribute__((weak));
extern lv_obj_t *ui_LabelVesc __attribute__((weak));
extern lv_obj_t *ui_LabelSensors __attribute__((weak));
extern lv_obj_t *ui_LabelJoyCenter __attribute__((weak));
extern lv_obj_t *ui_LabelJoyRaw __attribute__((weak));
extern lv_obj_t *ui_LabelJoyOut __attribute__((weak));
extern lv_obj_t *ui_LabelJoyRange __attribute__((weak));
extern lv_obj_t *ui_LabelSpeedAdc __attribute__((weak));
extern lv_obj_t *ui_ButtonJoyCal __attribute__((weak));
extern lv_obj_t *ui_ButtonJoyMinus __attribute__((weak));
extern lv_obj_t *ui_ButtonJoyPlus __attribute__((weak));
extern lv_obj_t *ui_ButtonJoyReset __attribute__((weak));
extern lv_obj_t *ui_LabelFirmware __attribute__((weak));
extern lv_obj_t *ui_LabelBrightness __attribute__((weak));
extern lv_obj_t *ui_LabelPowerMode __attribute__((weak));
extern lv_obj_t *ui_LabelUpgrade __attribute__((weak));
extern lv_obj_t *ui_SliderBrightness __attribute__((weak));
extern const lv_font_t ui_font_ChineseSmall __attribute__((weak));
}

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
bool brightnessRequestPending = false;
uint8_t brightnessRequest = 0;

lv_obj_t *optionalUiObject(lv_obj_t **symbol) {
  return symbol == nullptr ? nullptr : *symbol;
}

void applyChineseFont(lv_obj_t *object) {
  if (object != nullptr && &ui_font_ChineseSmall != nullptr) {
    lv_obj_set_style_text_font(object, &ui_font_ChineseSmall, LV_PART_MAIN);
  }
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

lv_obj_t *compassImage() {
  return ui_Compass;
}

lv_obj_t *mcuTemperatureLabel() {
  return ui_MCUTemp;
}

lv_obj_t *linkDiagnosticLabel() {
  return optionalUiObject(&ui_LabelLinkDiag);
}

lv_obj_t *receiverFlagsLabel() {
  return optionalUiObject(&ui_LabelRxFlags);
}

lv_obj_t *protocolLabel() {
  return optionalUiObject(&ui_LabelProtocol);
}

lv_obj_t *sequenceLabel() {
  return optionalUiObject(&ui_LabelSeq);
}

lv_obj_t *vescLabel() {
  return optionalUiObject(&ui_LabelVesc);
}

lv_obj_t *sensorsLabel() {
  return optionalUiObject(&ui_LabelSensors);
}

lv_obj_t *joyCenterLabel() {
  return optionalUiObject(&ui_LabelJoyCenter);
}

lv_obj_t *joyRawLabel() {
  return optionalUiObject(&ui_LabelJoyRaw);
}

lv_obj_t *joyOutLabel() {
  return optionalUiObject(&ui_LabelJoyOut);
}

lv_obj_t *joyRangeLabel() {
  return optionalUiObject(&ui_LabelJoyRange);
}

lv_obj_t *speedAdcLabel() {
  return optionalUiObject(&ui_LabelSpeedAdc);
}

lv_obj_t *joyCalButton() {
  return optionalUiObject(&ui_ButtonJoyCal);
}

lv_obj_t *joyMinusButton() {
  return optionalUiObject(&ui_ButtonJoyMinus);
}

lv_obj_t *joyPlusButton() {
  return optionalUiObject(&ui_ButtonJoyPlus);
}

lv_obj_t *joyResetButton() {
  return optionalUiObject(&ui_ButtonJoyReset);
}

lv_obj_t *firmwareLabel() {
  return optionalUiObject(&ui_LabelFirmware);
}

lv_obj_t *brightnessLabel() {
  return optionalUiObject(&ui_LabelBrightness);
}

lv_obj_t *powerModeLabel() {
  return optionalUiObject(&ui_LabelPowerMode);
}

lv_obj_t *upgradeLabel() {
  return optionalUiObject(&ui_LabelUpgrade);
}

lv_obj_t *brightnessSlider() {
  return optionalUiObject(&ui_SliderBrightness);
}

void brightnessSliderEvent(lv_event_t *event) {
  lv_obj_t *target = lv_event_get_target(event);
  if (target == nullptr) {
    return;
  }
  brightnessRequest = s3ClampUserBrightness(lv_slider_get_value(target));
  brightnessRequestPending = true;
}

lv_obj_t *createSettingsButton(lv_obj_t *parent, const char *text, int16_t x, int16_t y, int16_t w, int16_t h) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_size(button, w, h);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_style_radius(button, 6, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x202833), LV_PART_MAIN);
  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  applyChineseFont(label);
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
  lv_label_set_text(settingsTitleLabel, "摇杆设置");
  applyChineseFont(settingsTitleLabel);
  lv_obj_set_style_text_color(settingsTitleLabel, lv_color_hex(0x00D8FF), LV_PART_MAIN);
  lv_obj_align(settingsTitleLabel, LV_ALIGN_TOP_MID, 0, 8);

  settingsCenterLabel = lv_label_create(settingsPanel);
  lv_label_set_text(settingsCenterLabel, "中心 2048");
  applyChineseFont(settingsCenterLabel);
  lv_obj_set_style_text_color(settingsCenterLabel, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(settingsCenterLabel, LV_ALIGN_TOP_MID, 0, 36);

  settingsMinusButton = createSettingsButton(settingsPanel, "-10", 12, 68, 52, 36);
  settingsCalButton = createSettingsButton(settingsPanel, "校准", 78, 68, 52, 36);
  settingsPlusButton = createSettingsButton(settingsPanel, "+10", 144, 68, 52, 36);
  settingsCloseButton = createSettingsButton(settingsPanel, "退出", 60, 116, 90, 34);

  settingsHintLabel = lv_label_create(settingsPanel);
  lv_label_set_text(settingsHintLabel, "输出锁定");
  applyChineseFont(settingsHintLabel);
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
    lv_label_set_text(statusValueLabel(), "未连接");
    applyChineseFont(statusValueLabel());
    lv_obj_set_style_text_color(statusValueLabel(), lv_color_hex(0xFF4040), LV_PART_MAIN);
  }

  if (controlValueLabel() != nullptr) {
    lv_label_set_text(controlValueLabel(), "1");
  }

  if (bmp280ValueLabel() != nullptr) {
    lv_label_set_text(bmp280ValueLabel(), "气压 --\n高度 --");
    lv_obj_set_style_text_color(bmp280ValueLabel(), lv_color_white(), LV_PART_MAIN);
    applyChineseFont(bmp280ValueLabel());
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
    applyChineseFont(mcuTemperatureLabel());
  }

  if (linkDiagnosticLabel() != nullptr) {
    lv_label_set_text(linkDiagnosticLabel(), "信号 -- 包 0 丢 0");
    applyChineseFont(linkDiagnosticLabel());
  }
  if (receiverFlagsLabel() != nullptr) {
    lv_label_set_text(receiverFlagsLabel(), "正常");
    applyChineseFont(receiverFlagsLabel());
  }
  if (protocolLabel() != nullptr) {
    char buffer[32] = {};
    s3FormatProtocolText(buffer, sizeof(buffer));
    lv_label_set_text(protocolLabel(), buffer);
    applyChineseFont(protocolLabel());
  }
  if (sequenceLabel() != nullptr) {
    lv_label_set_text(sequenceLabel(), "发送 0 接收 0");
    applyChineseFont(sequenceLabel());
  }
  if (vescLabel() != nullptr) {
    lv_label_set_text(vescLabel(), "电调 无数据");
    applyChineseFont(vescLabel());
  }
  if (sensorsLabel() != nullptr) {
    lv_label_set_text(sensorsLabel(), "传感器 电池无 气压无 指南无");
    applyChineseFont(sensorsLabel());
  }
  if (joyCenterLabel() != nullptr) {
    lv_label_set_text(joyCenterLabel(), "摇杆中心 2048");
    applyChineseFont(joyCenterLabel());
  }
  if (joyRawLabel() != nullptr) {
    lv_label_set_text(joyRawLabel(), "摇杆原始 2048");
    applyChineseFont(joyRawLabel());
  }
  if (joyOutLabel() != nullptr) {
    lv_label_set_text(joyOutLabel(), "摇杆输出 0");
    applyChineseFont(joyOutLabel());
  }
  if (joyRangeLabel() != nullptr) {
    lv_label_set_text(joyRangeLabel(), "最小 0 最大 4095 死区 50");
    applyChineseFont(joyRangeLabel());
  }
  if (speedAdcLabel() != nullptr) {
    lv_label_set_text(speedAdcLabel(), "速度旋钮 0");
    applyChineseFont(speedAdcLabel());
  }
  if (firmwareLabel() != nullptr) {
    lv_label_set_text(firmwareLabel(), "固件");
    applyChineseFont(firmwareLabel());
  }
  if (brightnessLabel() != nullptr) {
    lv_label_set_text(brightnessLabel(), "亮度 0");
    applyChineseFont(brightnessLabel());
  }
  if (powerModeLabel() != nullptr) {
    lv_label_set_text(powerModeLabel(), "活动");
    applyChineseFont(powerModeLabel());
  }
  if (upgradeLabel() != nullptr) {
    char buffer[32] = {};
    s3FormatUpgradeText(buffer, sizeof(buffer));
    lv_label_set_text(upgradeLabel(), buffer);
    applyChineseFont(upgradeLabel());
  }
  if (brightnessSlider() != nullptr) {
    lv_slider_set_range(brightnessSlider(), 20, 255);
    lv_slider_set_value(brightnessSlider(), 140, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightnessSlider(), brightnessSliderEvent, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  createSettingsPanel();
}

void s3_ui_update(const S3UiState &state) {
  char buffer[64] = {};
  const int16_t batteryPercent = s3BatteryPercentForArc(state.cw2015Valid, state.cw2015Soc);
  const int16_t throttleBarValue = s3ThrottleBarValue(state.joystickValue);
  const int16_t statusVoltageValue = s3StatusVoltageForArc(state.connected, state.receiverVoltageX100);
  const int16_t compassAngle = s3CompassAngleForHeading(state.qmcValid, state.qmcHeadingDeg);

  s3FormatMainStatusText(buffer,
                         sizeof(buffer),
                         state.connected,
                         state.standbyMode,
                         state.takeoverActive,
                         state.armed,
                         state.receiverStatusFlags);
  if (statusValueLabel() != nullptr) {
    lv_label_set_text(statusValueLabel(), buffer);
    const bool receiverFault = (state.receiverStatusFlags & (STATUS_FLAG_FAILSAFE | STATUS_FLAG_PROTOCOL_FAULT)) != 0;
    const uint32_t color = receiverFault ? 0xFF4040 :
                           (state.takeoverActive ? 0x00D8FF :
                            (state.standbyMode ? 0x8AA0B2 :
                             (!state.armed ? 0xFFD23F : (state.connected ? 0x00D86A : 0xFF4040))));
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
      snprintf(buffer, sizeof(buffer), "气压 --\n高度 --");
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
    lv_obj_set_style_text_color(mcuTemperatureLabel(),
                                state.mcuTemperatureWarning ? lv_color_hex(0xFF4040) : lv_color_white(),
                                LV_PART_MAIN);
  }

  if (linkDiagnosticLabel() != nullptr) {
    s3FormatLinkDiagnosticText(buffer, sizeof(buffer), state.rssiValue, state.statusPacketRateHz, state.statusLostPackets);
    lv_label_set_text(linkDiagnosticLabel(), buffer);
  }
  if (receiverFlagsLabel() != nullptr) {
    s3FormatReceiverStatusFlags(buffer, sizeof(buffer), state.receiverStatusFlags);
    lv_label_set_text(receiverFlagsLabel(), buffer);
  }
  if (protocolLabel() != nullptr) {
    s3FormatProtocolText(buffer, sizeof(buffer));
    lv_label_set_text(protocolLabel(), buffer);
  }
  if (sequenceLabel() != nullptr) {
    s3FormatSequenceText(buffer, sizeof(buffer), state.controlSequence, state.lastStatusSequence);
    lv_label_set_text(sequenceLabel(), buffer);
  }
  if (vescLabel() != nullptr) {
    s3FormatVescText(buffer, sizeof(buffer), state.receiverStatusFlags);
    lv_label_set_text(vescLabel(), buffer);
  }
  if (sensorsLabel() != nullptr) {
    s3FormatSensorsText(buffer, sizeof(buffer), state.cw2015Valid, state.bmp280Valid, state.qmcValid);
    lv_label_set_text(sensorsLabel(), buffer);
  }
  if (joyCenterLabel() != nullptr) {
    s3FormatJoystickText(buffer, sizeof(buffer), "摇杆中心", state.joystickCenter);
    lv_label_set_text(joyCenterLabel(), buffer);
  }
  if (joyRawLabel() != nullptr) {
    s3FormatJoystickText(buffer, sizeof(buffer), "摇杆原始", state.joystickRawAdc);
    lv_label_set_text(joyRawLabel(), buffer);
  }
  if (joyOutLabel() != nullptr) {
    s3FormatJoystickText(buffer, sizeof(buffer), "摇杆输出", state.joystickValue);
    lv_label_set_text(joyOutLabel(), buffer);
  }
  if (joyRangeLabel() != nullptr) {
    snprintf(buffer,
             sizeof(buffer),
             "最小 %d 最大 %d 死区 %d",
             state.joystickMinRaw,
             state.joystickMaxRaw,
             state.joystickDeadzone);
    lv_label_set_text(joyRangeLabel(), buffer);
  }
  if (speedAdcLabel() != nullptr) {
    snprintf(buffer, sizeof(buffer), "速度旋钮 %u", state.speedAdcRaw);
    lv_label_set_text(speedAdcLabel(), buffer);
  }
  if (firmwareLabel() != nullptr) {
    s3FormatFirmwareText(buffer, sizeof(buffer), state.firmwareVersion, state.buildDate);
    lv_label_set_text(firmwareLabel(), buffer);
  }
  if (brightnessLabel() != nullptr) {
    s3FormatBrightnessText(buffer, sizeof(buffer), state.displayBrightness);
    lv_label_set_text(brightnessLabel(), buffer);
  }
  if (powerModeLabel() != nullptr) {
    s3FormatPowerModeText(buffer, sizeof(buffer), state.standbyMode, state.displayDimmed);
    lv_label_set_text(powerModeLabel(), buffer);
  }
  if (upgradeLabel() != nullptr) {
    s3FormatUpgradeText(buffer, sizeof(buffer));
    lv_label_set_text(upgradeLabel(), buffer);
  }
  if (brightnessSlider() != nullptr && !lv_obj_has_state(brightnessSlider(), LV_STATE_PRESSED)) {
    lv_slider_set_value(brightnessSlider(), state.displayBrightness, LV_ANIM_OFF);
  }

  s3_ui_set_settings_visible(state.settingsMode);
  if (settingsCenterLabel != nullptr) {
    snprintf(buffer, sizeof(buffer), "中心 %d", state.joystickCenter);
    lv_label_set_text(settingsCenterLabel, buffer);
  }
  if (settingsHintLabel != nullptr) {
    lv_label_set_text(settingsHintLabel, state.armed ? "输出锁定" : "退出后重新解锁");
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

bool s3_ui_consume_brightness_request(uint8_t &brightness) {
  if (!brightnessRequestPending) {
    return false;
  }
  brightness = brightnessRequest;
  brightnessRequestPending = false;
  return true;
}

S3UiTouchAction s3_ui_set_touch(bool pressed, int16_t x, int16_t y) {
  if (!pressed) {
    touchWasPressed = false;
    return S3UiTouchAction::None;
  }
  if (touchWasPressed) {
    return S3UiTouchAction::None;
  }
  touchWasPressed = true;

  if (touchInside(joyCalButton(), x, y)) {
    return S3UiTouchAction::CalibrateCenter;
  }
  if (touchInside(joyMinusButton(), x, y)) {
    return S3UiTouchAction::CenterMinus;
  }
  if (touchInside(joyPlusButton(), x, y)) {
    return S3UiTouchAction::CenterPlus;
  }
  if (touchInside(joyResetButton(), x, y)) {
    return S3UiTouchAction::ResetCalibration;
  }
  if (!settingsVisible) {
    return S3UiTouchAction::None;
  }

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
