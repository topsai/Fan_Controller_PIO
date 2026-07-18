#include <unity.h>
#include <receiver_frame.h>
#ifdef FAN_CONTROLLER_HIL
#include <hil_protocol.h>
#include <hil_safety.h>
#endif
#include "control_logic.h"
#include "beep_profiles.h"
#include "protocol.h"
#include "diagnostic_protocol.h"
#include "oled_chinese_font.h"
#include "s3_runtime_config.h"
#include "s3_ui_bindings.h"

void setUp() {}
void tearDown() {}

void test_pwm_mapping_uses_vesc_servo_range() {
  TEST_ASSERT_EQUAL_INT(51, pwmDutyForThrottle(-1000, 3, 51, 102));
  TEST_ASSERT_EQUAL_INT(102, pwmDutyForThrottle(1000, 3, 51, 102));
  TEST_ASSERT_EQUAL_INT(76, pwmDutyForThrottle(0, 3, 51, 102));
}

void test_pwm_mapping_applies_speed_level_limits() {
  TEST_ASSERT_EQUAL_INT(89, pwmDutyForThrottle(1000, 1, 51, 102));
  TEST_ASSERT_EQUAL_INT(95, pwmDutyForThrottle(1000, 2, 51, 102));
  TEST_ASSERT_EQUAL_INT(63, pwmDutyForThrottle(-1000, 1, 51, 102));
}

void test_pwm_mapping_defaults_invalid_speed_level_to_level_1() {
  TEST_ASSERT_EQUAL_INT(89, pwmDutyForThrottle(1000, 0, 51, 102));
  TEST_ASSERT_EQUAL_INT(89, pwmDutyForThrottle(1000, 4, 51, 102));
}

void test_joystick_mapping_uses_calibrated_center_and_deadzone() {
  TEST_ASSERT_EQUAL_INT(0, joystickToThrottle(2100, 2100, 50));
  TEST_ASSERT_EQUAL_INT(0, joystickToThrottle(2149, 2100, 50));
  TEST_ASSERT_EQUAL_INT(0, joystickToThrottle(2051, 2100, 50));
  TEST_ASSERT_TRUE(joystickToThrottle(4095, 2100, 50) > 950);
  TEST_ASSERT_TRUE(joystickToThrottle(0, 2100, 50) < -950);
}

void test_joystick_center_calibration_averages_samples() {
  const int samples[] = {2098, 2100, 2102, 2100};
  TEST_ASSERT_EQUAL_INT(2100, calibratedJoystickCenter(samples, 4, 2048));
}

void test_joystick_center_rejects_invalid_persisted_values() {
  TEST_ASSERT_TRUE(joystickCenterIsValid(1));
  TEST_ASSERT_TRUE(joystickCenterIsValid(2048));
  TEST_ASSERT_TRUE(joystickCenterIsValid(4094));
  TEST_ASSERT_FALSE(joystickCenterIsValid(0));
  TEST_ASSERT_FALSE(joystickCenterIsValid(4095));
  TEST_ASSERT_FALSE(joystickCenterIsValid(-1));
}

void test_joystick_center_adjustment_clamps_to_valid_range() {
  TEST_ASSERT_EQUAL_INT(2058, adjustedJoystickCenter(2048, 10));
  TEST_ASSERT_EQUAL_INT(2038, adjustedJoystickCenter(2048, -10));
  TEST_ASSERT_EQUAL_INT(1, adjustedJoystickCenter(4, -10));
  TEST_ASSERT_EQUAL_INT(4094, adjustedJoystickCenter(4090, 10));
}

void test_joystick_neutral_range_uses_center_and_deadzone() {
  TEST_ASSERT_EQUAL_INT(1998, joystickNeutralRangeMin(2048, 50));
  TEST_ASSERT_EQUAL_INT(2098, joystickNeutralRangeMax(2048, 50));
  TEST_ASSERT_EQUAL_INT(1, joystickNeutralRangeMin(20, 50));
  TEST_ASSERT_EQUAL_INT(4094, joystickNeutralRangeMax(4080, 50));
}

void test_joystick_neutral_range_calibration_uses_two_round_return_samples() {
  const int samples[] = {2042, 2060, 2038, 2055};
  const JoystickNeutralRange range = calibratedJoystickNeutralRange(samples, 4, 2048, 10);
  TEST_ASSERT_EQUAL_INT(2038, range.minRaw);
  TEST_ASSERT_EQUAL_INT(2060, range.maxRaw);
  TEST_ASSERT_EQUAL_INT(2049, range.center);

  const int identicalSamples[] = {2048, 2048, 2048, 2048};
  const JoystickNeutralRange widened = calibratedJoystickNeutralRange(identicalSamples, 4, 2048, 10);
  TEST_ASSERT_EQUAL_INT(2038, widened.minRaw);
  TEST_ASSERT_EQUAL_INT(2058, widened.maxRaw);
  TEST_ASSERT_EQUAL_INT(2048, widened.center);
}

void test_joystick_mapping_uses_neutral_range_instead_of_single_center() {
  TEST_ASSERT_EQUAL_INT(0, joystickToThrottleNeutralRange(2038, 2038, 2060));
  TEST_ASSERT_EQUAL_INT(0, joystickToThrottleNeutralRange(2060, 2038, 2060));
  TEST_ASSERT_TRUE(joystickToThrottleNeutralRange(4095, 2038, 2060) > 980);
  TEST_ASSERT_TRUE(joystickToThrottleNeutralRange(0, 2038, 2060) < -980);
}

void test_c3_chinese_font_contains_required_ui_glyphs() {
  const char *required = "设置中心当前保存锁定退出连接断线电量速度油门刹车校准取位已解档公里压";
  for (const char *cursor = required; *cursor != '\0';) {
    const C3ChineseGlyph *glyph = c3FindChineseGlyph(cursor);
    TEST_ASSERT_NOT_NULL(glyph);
    cursor += c3Utf8CharLength((uint8_t)*cursor);
  }
}

void test_c3_chinese_font_uses_smaller_render_size() {
  TEST_ASSERT_EQUAL_UINT8(13, C3_CHINESE_RENDER_FONT_SIZE);
}

void test_c3_home_layout_uses_original_english_labels() {
  TEST_ASSERT_EQUAL_STRING("[OK]", c3HomeLinkLabel(true));
  TEST_ASSERT_EQUAL_STRING("[LOST]", c3HomeLinkLabel(false));
  TEST_ASSERT_EQUAL_STRING("ARM", c3HomeArmLabel(true));
  TEST_ASSERT_EQUAL_STRING("LOCK", c3HomeArmLabel(false));
  TEST_ASSERT_EQUAL_STRING("THR", c3HomeDirectionLabel(1));
  TEST_ASSERT_EQUAL_STRING("BRK", c3HomeDirectionLabel(0));
  TEST_ASSERT_EQUAL_STRING("BRK", c3HomeDirectionLabel(-1));
  TEST_ASSERT_EQUAL_STRING("SOC", c3HomeBatteryLabel(true));
  TEST_ASSERT_EQUAL_STRING("N/A", c3HomeBatteryLabel(false));
}

void test_c3_home_layout_uses_large_speed_and_framed_throttle_bar() {
  TEST_ASSERT_EQUAL_UINT8(4, c3HomeSpeedTextSize());
  TEST_ASSERT_FALSE(c3HomeUsesReadableChineseLabels());
  TEST_ASSERT_EQUAL_INT(0, c3HomeThrottleBarFillWidth(0, 40));
  TEST_ASSERT_EQUAL_INT(20, c3HomeThrottleBarFillWidth(500, 40));
  TEST_ASSERT_EQUAL_INT(40, c3HomeThrottleBarFillWidth(1000, 40));
  TEST_ASSERT_EQUAL_INT(40, c3HomeThrottleBarFillWidth(-1200, 40));
}

void test_joystick_calibration_rejects_invalid_persisted_values() {
  JoystickCalibration calibration = {2048, 0, 4095, 50};
  TEST_ASSERT_TRUE(joystickCalibrationIsValid(calibration));

  calibration.center = 0;
  TEST_ASSERT_FALSE(joystickCalibrationIsValid(calibration));

  calibration.center = 2048;
  calibration.minRaw = 2100;
  calibration.maxRaw = 2000;
  TEST_ASSERT_FALSE(joystickCalibrationIsValid(calibration));

  calibration.minRaw = 0;
  calibration.maxRaw = 4095;
  calibration.deadzone = 900;
  TEST_ASSERT_FALSE(joystickCalibrationIsValid(calibration));
}

void test_joystick_calibrated_mapping_uses_persisted_range() {
  const JoystickCalibration calibration = {2100, 100, 3900, 60};
  TEST_ASSERT_EQUAL_INT(0, joystickToThrottleCalibrated(2130, calibration));
  TEST_ASSERT_TRUE(joystickToThrottleCalibrated(3900, calibration) > 980);
  TEST_ASSERT_TRUE(joystickToThrottleCalibrated(100, calibration) < -980);
  TEST_ASSERT_EQUAL_INT(1000, joystickToThrottleCalibrated(4095, calibration));
  TEST_ASSERT_EQUAL_INT(-1000, joystickToThrottleCalibrated(0, calibration));
}

void test_transmitter_safety_forces_zero_until_armed() {
  TEST_ASSERT_EQUAL_INT16(0, safeThrottleForArming(700, false));
  TEST_ASSERT_EQUAL_INT16(-300, safeThrottleForArming(-300, true));
}

void test_transmitter_arming_requires_full_brake_hold_for_three_seconds() {
  uint32_t holdStartMs = 0;
  bool holding = false;

  TEST_ASSERT_FALSE(shouldArmByBrakeHold(-899, 1000, holdStartMs, holding, -900, 3000));
  TEST_ASSERT_FALSE(holding);

  TEST_ASSERT_FALSE(shouldArmByBrakeHold(-950, 2000, holdStartMs, holding, -900, 3000));
  TEST_ASSERT_TRUE(holding);
  TEST_ASSERT_EQUAL_UINT32(2000, holdStartMs);

  TEST_ASSERT_FALSE(shouldArmByBrakeHold(-950, 4999, holdStartMs, holding, -900, 3000));
  TEST_ASSERT_TRUE(shouldArmByBrakeHold(-950, 5000, holdStartMs, holding, -900, 3000));
}

void test_transmitter_brake_hold_resets_when_brake_released() {
  uint32_t holdStartMs = 0;
  bool holding = false;

  TEST_ASSERT_FALSE(shouldArmByBrakeHold(-950, 1000, holdStartMs, holding, -900, 3000));
  TEST_ASSERT_FALSE(shouldArmByBrakeHold(-200, 2500, holdStartMs, holding, -900, 3000));
  TEST_ASSERT_FALSE(holding);
  TEST_ASSERT_FALSE(shouldArmByBrakeHold(-950, 3000, holdStartMs, holding, -900, 3000));
  TEST_ASSERT_EQUAL_UINT32(3000, holdStartMs);
}

void test_throttle_slew_rate_limits_step_changes() {
  TEST_ASSERT_EQUAL_INT16(60, slewLimitedThrottle(0, 800, 60));
  TEST_ASSERT_EQUAL_INT16(-60, slewLimitedThrottle(0, -800, 60));
  TEST_ASSERT_EQUAL_INT16(95, slewLimitedThrottle(100, 95, 60));
  TEST_ASSERT_EQUAL_INT16(-1000, slewLimitedThrottle(-980, -1200, 60));
}

void test_receiver_failsafe_clears_all_stale_control_inputs() {
  ReceiverControlState state = {
    800,
    3,
    0x03,
    true,
    false,
  };

  applyReceiverFailsafe(state);

  TEST_ASSERT_EQUAL_INT(0, state.throttle);
  TEST_ASSERT_EQUAL_UINT8(1, state.speedLevel);
  TEST_ASSERT_EQUAL_UINT8(0, state.buttons);
  TEST_ASSERT_FALSE(state.connected);
  TEST_ASSERT_TRUE(state.failsafeActive);
}

void test_receiver_link_alert_beeps_immediately_then_every_two_seconds() {
  uint32_t lastAlertMs = 0;
  bool hasAlerted = false;

  TEST_ASSERT_TRUE(shouldEmitReceiverLinkAlert(false, true, 0, lastAlertMs, hasAlerted, 2000));
  TEST_ASSERT_EQUAL_UINT32(0, lastAlertMs);
  TEST_ASSERT_TRUE(hasAlerted);
  TEST_ASSERT_FALSE(shouldEmitReceiverLinkAlert(false, true, 1999, lastAlertMs, hasAlerted, 2000));
  TEST_ASSERT_TRUE(shouldEmitReceiverLinkAlert(false, true, 2000, lastAlertMs, hasAlerted, 2000));
  TEST_ASSERT_EQUAL_UINT32(2000, lastAlertMs);
}

void test_receiver_link_alert_is_silent_when_connected_and_not_failsafe() {
  uint32_t lastAlertMs = 0;
  bool hasAlerted = true;

  TEST_ASSERT_FALSE(shouldEmitReceiverLinkAlert(true, false, 5000, lastAlertMs, hasAlerted, 2000));
  TEST_ASSERT_EQUAL_UINT32(0, lastAlertMs);
  TEST_ASSERT_FALSE(hasAlerted);
}

void test_receiver_packet_gap_diagnostics_track_max_gap() {
  uint32_t lastPacketMs = 0;
  uint32_t maxGapMs = 0;

  updateReceiverPacketGapDiagnostics(1000, lastPacketMs, maxGapMs);
  TEST_ASSERT_EQUAL_UINT32(1000, lastPacketMs);
  TEST_ASSERT_EQUAL_UINT32(0, maxGapMs);

  updateReceiverPacketGapDiagnostics(1080, lastPacketMs, maxGapMs);
  TEST_ASSERT_EQUAL_UINT32(1080, lastPacketMs);
  TEST_ASSERT_EQUAL_UINT32(80, maxGapMs);

  updateReceiverPacketGapDiagnostics(2600, lastPacketMs, maxGapMs);
  TEST_ASSERT_EQUAL_UINT32(2600, lastPacketMs);
  TEST_ASSERT_EQUAL_UINT32(1520, maxGapMs);
}

void test_remote_horn_has_maximum_continuous_duration() {
  uint32_t hornStartMs = 0;
  bool hornActive = false;

  TEST_ASSERT_TRUE(shouldAllowRemoteHorn(true, 1000, hornStartMs, hornActive, 3000));
  TEST_ASSERT_TRUE(hornActive);
  TEST_ASSERT_EQUAL_UINT32(1000, hornStartMs);
  TEST_ASSERT_TRUE(shouldAllowRemoteHorn(true, 3999, hornStartMs, hornActive, 3000));
  TEST_ASSERT_FALSE(shouldAllowRemoteHorn(true, 4000, hornStartMs, hornActive, 3000));
  TEST_ASSERT_FALSE(hornActive);
}

void test_remote_horn_resets_after_button_release() {
  uint32_t hornStartMs = 0;
  bool hornActive = false;

  TEST_ASSERT_TRUE(shouldAllowRemoteHorn(true, 1000, hornStartMs, hornActive, 3000));
  TEST_ASSERT_FALSE(shouldAllowRemoteHorn(false, 1200, hornStartMs, hornActive, 3000));
  TEST_ASSERT_FALSE(hornActive);
  TEST_ASSERT_TRUE(shouldAllowRemoteHorn(true, 5000, hornStartMs, hornActive, 3000));
  TEST_ASSERT_TRUE(hornActive);
  TEST_ASSERT_EQUAL_UINT32(5000, hornStartMs);
}

void test_beep_profiles_use_distinct_frequencies() {
  TEST_ASSERT_EQUAL_UINT16(1800, BEEP_FREQ_BUTTON);
  TEST_ASSERT_EQUAL_UINT16(2200, BEEP_FREQ_STARTUP);
  TEST_ASSERT_EQUAL_UINT16(2400, BEEP_FREQ_CONNECTED);
  TEST_ASSERT_EQUAL_UINT16(1200, BEEP_FREQ_LINK_ALERT);
  TEST_ASSERT_EQUAL_UINT16(900, BEEP_FREQ_FAILSAFE);
  TEST_ASSERT_EQUAL_UINT16(2600, BEEP_FREQ_REMOTE_HORN);
  TEST_ASSERT_EQUAL_UINT16(700, BEEP_FREQ_LOW_BATTERY);

  TEST_ASSERT_NOT_EQUAL(BEEP_FREQ_BUTTON, BEEP_FREQ_STARTUP);
  TEST_ASSERT_NOT_EQUAL(BEEP_FREQ_CONNECTED, BEEP_FREQ_STARTUP);
  TEST_ASSERT_NOT_EQUAL(BEEP_FREQ_CONNECTED, BEEP_FREQ_REMOTE_HORN);
  TEST_ASSERT_NOT_EQUAL(BEEP_FREQ_LINK_ALERT, BEEP_FREQ_FAILSAFE);
  TEST_ASSERT_NOT_EQUAL(BEEP_FREQ_REMOTE_HORN, BEEP_FREQ_LINK_ALERT);
  TEST_ASSERT_NOT_EQUAL(BEEP_FREQ_LOW_BATTERY, BEEP_FREQ_BUTTON);
}

void test_receiver_connection_success_only_on_reconnect_edge() {
  TEST_ASSERT_TRUE(shouldSignalReceiverConnectionSuccess(false, false));
  TEST_ASSERT_TRUE(shouldSignalReceiverConnectionSuccess(false, true));
  TEST_ASSERT_TRUE(shouldSignalReceiverConnectionSuccess(true, true));
  TEST_ASSERT_FALSE(shouldSignalReceiverConnectionSuccess(true, false));
}

void test_s3_lvgl_timing_uses_low_latency_profile() {
  TEST_ASSERT_EQUAL_UINT16(16, S3_LVGL_DISPLAY_REFRESH_PERIOD_MS);
  TEST_ASSERT_EQUAL_UINT16(5, S3_LVGL_INPUT_READ_PERIOD_MS);
  TEST_ASSERT_EQUAL_UINT8(2, S3_LVGL_HANDLER_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(2, S3_MAIN_LOOP_DELAY_MS);
}

void test_s3_lvgl_display_dma_is_enabled() {
  TEST_ASSERT_TRUE(S3_LVGL_DISPLAY_USE_DMA);
}

void test_s3_espnow_link_uses_stable_radio_profile() {
  TEST_ASSERT_TRUE(S3_ESPNOW_STATUS_TIMEOUT_MS >= 1000);
  TEST_ASSERT_EQUAL_UINT16(60, S3_ESPNOW_TX_POWER_DBM_X4);
  TEST_ASSERT_TRUE(S3_ESPNOW_CONTROL_TASK_ENABLED);
}

void test_receiver_status_target_tracks_last_valid_transmitter() {
  const uint8_t s3Mac[] = {0x48, 0xCA, 0x43, 0x9A, 0xA9, 0xB0};
  uint8_t statusTarget[] = {0xAC, 0xEB, 0xE6, 0x44, 0xD5, 0x54};
  bool hasTarget = false;

  rememberStatusTarget(s3Mac, statusTarget, hasTarget);

  TEST_ASSERT_TRUE(hasTarget);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(s3Mac, statusTarget, 6);
}

void test_controller_source_rejects_non_active_remote_without_takeover() {
  const uint8_t activeMac[] = {0xAC, 0xEB, 0xE6, 0x44, 0xD5, 0x54};
  const uint8_t otherMac[] = {0x48, 0xCA, 0x43, 0x9A, 0xA9, 0xB0};
  ControllerSourceState state = {};
  rememberActiveController(activeMac, state, false, 1000);

  TEST_ASSERT_TRUE(controllerSourceIsActive(activeMac, state));
  TEST_ASSERT_FALSE(controllerSourceAllowsPacket(otherMac, state, true, false));
}

void test_controller_source_allows_takeover_and_resets_stability() {
  const uint8_t activeMac[] = {0xAC, 0xEB, 0xE6, 0x44, 0xD5, 0x54};
  const uint8_t otherMac[] = {0x48, 0xCA, 0x43, 0x9A, 0xA9, 0xB0};
  ControllerSourceState state = {};
  rememberActiveController(activeMac, state, false, 1000);

  TEST_ASSERT_TRUE(controllerSourceAllowsPacket(otherMac, state, true, true));
  TEST_ASSERT_TRUE(controllerSourceShouldResetForTakeover(otherMac, state, true));
  rememberActiveController(otherMac, state, false, 1500);
  TEST_ASSERT_TRUE(controllerSourceIsActive(otherMac, state));
}

void test_controller_source_releases_after_timeout() {
  const uint8_t activeMac[] = {0xAC, 0xEB, 0xE6, 0x44, 0xD5, 0x54};
  const uint8_t otherMac[] = {0x48, 0xCA, 0x43, 0x9A, 0xA9, 0xB0};
  ControllerSourceState state = {};
  rememberActiveController(activeMac, state, false, 1000);

  releaseActiveControllerIfTimedOut(state, 3001, 2000);
  TEST_ASSERT_FALSE(state.hasActiveController);
  TEST_ASSERT_TRUE(controllerSourceAllowsPacket(otherMac, state, false, false));
}

void test_button_short_press_and_long_press_are_distinct() {
  ButtonLongPressState state = {};

  TEST_ASSERT_EQUAL_INT(BUTTON_PRESS_NONE, buttonLongPressUpdate(true, 1000, 3000, state));
  TEST_ASSERT_EQUAL_INT(BUTTON_PRESS_SHORT, buttonLongPressUpdate(false, 1800, 3000, state));

  TEST_ASSERT_EQUAL_INT(BUTTON_PRESS_NONE, buttonLongPressUpdate(true, 5000, 3000, state));
  TEST_ASSERT_EQUAL_INT(BUTTON_PRESS_NONE, buttonLongPressUpdate(true, 7999, 3000, state));
  TEST_ASSERT_EQUAL_INT(BUTTON_PRESS_LONG, buttonLongPressUpdate(true, 8000, 3000, state));
  TEST_ASSERT_EQUAL_INT(BUTTON_PRESS_NONE, buttonLongPressUpdate(false, 8200, 3000, state));
}

void test_protocol_crc8_detects_packet_changes() {
  const uint8_t payload[] = {0xA5, 0x01, CONTROL_PROTOCOL_VERSION, 0x34, 0x12, 0x00};
  const uint8_t crc = protocolCrc8(payload, sizeof(payload));
  TEST_ASSERT_NOT_EQUAL_UINT8(0, crc);

  uint8_t changed[sizeof(payload)] = {};
  memcpy(changed, payload, sizeof(payload));
  changed[4] ^= 0x01;
  TEST_ASSERT_NOT_EQUAL_UINT8(crc, protocolCrc8(changed, sizeof(changed)));
}

void test_protocol_sequence_accepts_fresh_values_and_rejects_stale_replays() {
  uint16_t lastSeq = 0;
  bool hasSeq = false;

  TEST_ASSERT_TRUE(protocolSequenceIsFresh(10, lastSeq, hasSeq));
  protocolRememberSequence(10, lastSeq, hasSeq);
  TEST_ASSERT_TRUE(hasSeq);
  TEST_ASSERT_EQUAL_UINT16(10, lastSeq);
  TEST_ASSERT_FALSE(protocolSequenceIsFresh(10, lastSeq, hasSeq));
  TEST_ASSERT_FALSE(protocolSequenceIsFresh(9, lastSeq, hasSeq));
  TEST_ASSERT_TRUE(protocolSequenceIsFresh(11, lastSeq, hasSeq));
  TEST_ASSERT_TRUE(protocolSequenceIsFresh(0, 65535, true));
}

void test_status_sequence_resets_after_connection_timeout() {
  uint16_t lastSeq = 4200;
  bool hasSeq = true;

  resetSequenceAfterConnectionTimeout(false, hasSeq, lastSeq);

  TEST_ASSERT_FALSE(hasSeq);
  TEST_ASSERT_TRUE(protocolSequenceIsFresh(0, lastSeq, hasSeq));
}

void test_protocol_status_flags_are_composable() {
  uint8_t flags = 0;
  flags |= STATUS_FLAG_FAILSAFE;
  flags |= STATUS_FLAG_VESC_VALID;
  TEST_ASSERT_TRUE((flags & STATUS_FLAG_FAILSAFE) != 0);
  TEST_ASSERT_TRUE((flags & STATUS_FLAG_VESC_VALID) != 0);
  TEST_ASSERT_FALSE((flags & STATUS_FLAG_PROTOCOL_FAULT) != 0);
}

void test_protocol_control_takeover_flag_is_separate_from_status_lock_flag() {
  TEST_ASSERT_EQUAL_UINT8(0x01, CONTROL_FLAG_TAKEOVER_REQUEST);
  TEST_ASSERT_NOT_EQUAL(CONTROL_FLAG_TAKEOVER_REQUEST, STATUS_FLAG_OUTPUT_LOCKED);
}

void test_protocol_legacy_checksum_keeps_v1_packets_migratable() {
  const uint8_t legacyControl[] = {0xA5, 0x01, 0x00, 0x00, 0x01, 0x00, 0xA7};
  TEST_ASSERT_EQUAL_UINT8(7, LEGACY_CONTROL_PACKET_SIZE);
  TEST_ASSERT_EQUAL_UINT8(14, LEGACY_STATUS_PACKET_SIZE);
  TEST_ASSERT_TRUE(protocolLegacyChecksumIsValid(legacyControl, sizeof(legacyControl)));

  uint8_t changed[sizeof(legacyControl)] = {};
  memcpy(changed, legacyControl, sizeof(legacyControl));
  changed[4] = 0x02;
  TEST_ASSERT_FALSE(protocolLegacyChecksumIsValid(changed, sizeof(changed)));
}

void test_diagnostic_duration_requires_explicit_long_flag_for_30_minutes() {
  TEST_ASSERT_TRUE(diagnosticDurationIsAllowed(10000, false));
  TEST_ASSERT_FALSE(diagnosticDurationIsAllowed(DIAGNOSTIC_LONG_TEST_MS, false));
  TEST_ASSERT_TRUE(diagnosticDurationIsAllowed(DIAGNOSTIC_LONG_TEST_MS, true));
}

void test_diagnostic_status_line_includes_role_and_counters() {
  char buffer[96] = {};
  diagnosticFormatStatusLine(buffer, sizeof(buffer), "receiver", true, 125, 0, 3);
  TEST_ASSERT_EQUAL_STRING("DIAG STATUS role=receiver connected=1 rx=125 lost=0 faults=3", buffer);
}

void test_s3_battery_arc_value_clamps_and_rounds_soc() {
  TEST_ASSERT_EQUAL_INT16(0, s3BatteryPercentForArc(false, 80.0f));
  TEST_ASSERT_EQUAL_INT16(0, s3BatteryPercentForArc(true, -4.0f));
  TEST_ASSERT_EQUAL_INT16(0, s3BatteryPercentForArc(true, NAN));
  TEST_ASSERT_EQUAL_INT16(81, s3BatteryPercentForArc(true, 80.6f));
  TEST_ASSERT_EQUAL_INT16(100, s3BatteryPercentForArc(true, 120.0f));
}

void test_s3_speed_label_color_uses_speed_bands() {
  TEST_ASSERT_EQUAL_UINT32(0x00D86A, s3SpeedColorHex(0));
  TEST_ASSERT_EQUAL_UINT32(0x00D86A, s3SpeedColorHex(14));
  TEST_ASSERT_EQUAL_UINT32(0xFFD23F, s3SpeedColorHex(15));
  TEST_ASSERT_EQUAL_UINT32(0xFFD23F, s3SpeedColorHex(29));
  TEST_ASSERT_EQUAL_UINT32(0xFF4040, s3SpeedColorHex(30));
}

void test_s3_status_text_appends_lock_without_hiding_link_status() {
  char buffer[32] = {};

  s3FormatStatusText(buffer, sizeof(buffer), true, false);
  TEST_ASSERT_EQUAL_STRING("已连接 锁定", buffer);

  s3FormatStatusText(buffer, sizeof(buffer), false, false);
  TEST_ASSERT_EQUAL_STRING("未连接 锁定", buffer);

  s3FormatStatusText(buffer, sizeof(buffer), true, true);
  TEST_ASSERT_EQUAL_STRING("已连接", buffer);
}

void test_s3_main_status_text_prioritizes_actionable_states() {
  char buffer[32] = {};

  s3FormatMainStatusText(buffer, sizeof(buffer), false, true, false, false, 0);
  TEST_ASSERT_EQUAL_STRING("待机", buffer);

  s3FormatMainStatusText(buffer, sizeof(buffer), true, false, true, true, 0);
  TEST_ASSERT_EQUAL_STRING("接管中", buffer);

  s3FormatMainStatusText(buffer, sizeof(buffer), true, false, false, true, STATUS_FLAG_FAILSAFE);
  TEST_ASSERT_EQUAL_STRING("接收失效", buffer);

  s3FormatMainStatusText(buffer, sizeof(buffer), true, false, false, false, 0);
  TEST_ASSERT_EQUAL_STRING("已连接 锁定", buffer);
}

void test_s3_voltage_arc_value_clamps_to_vesc_display_range() {
  TEST_ASSERT_EQUAL_INT16(6, s3StatusVoltageForArc(false, 4800));
  TEST_ASSERT_EQUAL_INT16(6, s3StatusVoltageForArc(true, 0));
  TEST_ASSERT_EQUAL_INT16(6, s3StatusVoltageForArc(true, 599));
  TEST_ASSERT_EQUAL_INT16(36, s3StatusVoltageForArc(true, 3560));
  TEST_ASSERT_EQUAL_INT16(48, s3StatusVoltageForArc(true, 5200));
}

void test_s3_throttle_bar_keeps_center_and_uses_direction_colors() {
  TEST_ASSERT_EQUAL_INT16(-1000, s3ThrottleBarValue(-1500));
  TEST_ASSERT_EQUAL_INT16(0, s3ThrottleBarValue(0));
  TEST_ASSERT_EQUAL_INT16(1000, s3ThrottleBarValue(1500));
  TEST_ASSERT_EQUAL_UINT32(0x00C853, s3ThrottleColorHex(500));
  TEST_ASSERT_EQUAL_UINT32(0xFF9800, s3ThrottleColorHex(-500));
  TEST_ASSERT_EQUAL_UINT32(0x606060, s3ThrottleColorHex(0));
}

void test_s3_compass_angle_uses_qmc_heading_in_lvgl_tenths() {
  TEST_ASSERT_EQUAL_INT16(0, s3CompassAngleForHeading(false, 180.0f));
  TEST_ASSERT_EQUAL_INT16(0, s3CompassAngleForHeading(true, NAN));
  TEST_ASSERT_EQUAL_INT16(0, s3CompassAngleForHeading(true, 0.0f));
  TEST_ASSERT_EQUAL_INT16(905, s3CompassAngleForHeading(true, 90.49f));
  TEST_ASSERT_EQUAL_INT16(10, s3CompassAngleForHeading(true, 361.0f));
  TEST_ASSERT_EQUAL_INT16(3590, s3CompassAngleForHeading(true, -1.0f));
}

void test_s3_mcu_temperature_formats_one_decimal_or_placeholder() {
  char buffer[16] = {};
  s3FormatMcuTemperatureText(buffer, sizeof(buffer), 43.24f);
  TEST_ASSERT_EQUAL_STRING("43.2C", buffer);

  s3FormatMcuTemperatureText(buffer, sizeof(buffer), 43.25f);
  TEST_ASSERT_EQUAL_STRING("43.3C", buffer);

  s3FormatMcuTemperatureText(buffer, sizeof(buffer), NAN);
  TEST_ASSERT_EQUAL_STRING("--.-C", buffer);
}

void test_s3_diagnostic_text_formats_link_quality() {
  char buffer[48] = {};
  s3FormatLinkDiagnosticText(buffer, sizeof(buffer), -62, 48, 3);
  TEST_ASSERT_EQUAL_STRING("信号 -62 包 48 丢 3", buffer);
}

void test_s3_ui_detail_text_helpers_format_product_pages() {
  char buffer[96] = {};

  s3FormatProtocolText(buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("协议 控制v2 状态v2", buffer);

  s3FormatSequenceText(buffer, sizeof(buffer), 12, 34);
  TEST_ASSERT_EQUAL_STRING("发送 12 接收 34", buffer);

  s3FormatVescText(buffer, sizeof(buffer), STATUS_FLAG_VESC_VALID);
  TEST_ASSERT_EQUAL_STRING("电调 正常", buffer);

  s3FormatSensorsText(buffer, sizeof(buffer), true, false, true);
  TEST_ASSERT_EQUAL_STRING("传感器 电池正常 气压无 指南正常", buffer);

  s3FormatJoystickText(buffer, sizeof(buffer), "摇杆原始", 2048);
  TEST_ASSERT_EQUAL_STRING("摇杆原始 2048", buffer);

  s3FormatFirmwareText(buffer, sizeof(buffer), "s3-remote", "Jun 15 2026");
  TEST_ASSERT_EQUAL_STRING("固件 s3-remote Jun 15 2026", buffer);

  s3FormatBrightnessText(buffer, sizeof(buffer), 140);
  TEST_ASSERT_EQUAL_STRING("亮度 140", buffer);

  TEST_ASSERT_EQUAL_UINT8(20, s3ClampUserBrightness(0));
  TEST_ASSERT_EQUAL_UINT8(140, s3ClampUserBrightness(140));
  TEST_ASSERT_EQUAL_UINT8(255, s3ClampUserBrightness(300));

  s3FormatPowerModeText(buffer, sizeof(buffer), false, true);
  TEST_ASSERT_EQUAL_STRING("变暗", buffer);

  s3FormatUpgradeText(buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("升级 USB烧录", buffer);
}

void test_s3_receiver_status_text_decodes_flags() {
  char buffer[48] = {};
  s3FormatReceiverStatusFlags(buffer, sizeof(buffer), STATUS_FLAG_FAILSAFE | STATUS_FLAG_PROTOCOL_FAULT);
  TEST_ASSERT_EQUAL_STRING("失效 协议", buffer);

  s3FormatReceiverStatusFlags(buffer, sizeof(buffer), STATUS_FLAG_VESC_VALID | STATUS_FLAG_OUTPUT_LOCKED);
  TEST_ASSERT_EQUAL_STRING("电调 锁定", buffer);
}

void test_s3_persisted_brightness_uses_fallback_for_missing_value() {
  TEST_ASSERT_EQUAL_UINT8(140, s3ResolveStoredBrightness(-1, 140));
  TEST_ASSERT_EQUAL_UINT8(20, s3ResolveStoredBrightness(0, 140));
  TEST_ASSERT_EQUAL_UINT8(180, s3ResolveStoredBrightness(180, 140));
  TEST_ASSERT_EQUAL_UINT8(255, s3ResolveStoredBrightness(300, 140));
}

void test_s3_mcu_temperature_warning_threshold() {
  TEST_ASSERT_FALSE(s3McuTemperatureWarns(false, 90.0f, 75.0f));
  TEST_ASSERT_FALSE(s3McuTemperatureWarns(true, NAN, 75.0f));
  TEST_ASSERT_FALSE(s3McuTemperatureWarns(true, 74.9f, 75.0f));
  TEST_ASSERT_TRUE(s3McuTemperatureWarns(true, 75.0f, 75.0f));
}

void test_s3_swipe_direction_requires_horizontal_drag() {
  TEST_ASSERT_EQUAL_INT8(1, s3SwipeDirectionForDrag(190, 120, 80, 124));
  TEST_ASSERT_EQUAL_INT8(-1, s3SwipeDirectionForDrag(50, 120, 160, 118));
  TEST_ASSERT_EQUAL_INT8(0, s3SwipeDirectionForDrag(100, 120, 130, 118));
  TEST_ASSERT_EQUAL_INT8(0, s3SwipeDirectionForDrag(100, 40, 160, 130));
}

void test_s3_bmp280_altitude_uses_standard_sea_level_pressure() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, s3Bmp280AltitudeMeters(1013.25f));
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 110.9f, s3Bmp280AltitudeMeters(1000.0f));
  TEST_ASSERT_TRUE(isnan(s3Bmp280AltitudeMeters(NAN)));
}

void test_s3_cw2015_reading_rejects_transient_zero_or_nan_values() {
  TEST_ASSERT_TRUE(s3Cw2015ReadingIsPlausible(4.1f, 80.0f));
  TEST_ASSERT_FALSE(s3Cw2015ReadingIsPlausible(0.0f, 0.0f));
  TEST_ASSERT_FALSE(s3Cw2015ReadingIsPlausible(NAN, 80.0f));
  TEST_ASSERT_FALSE(s3Cw2015ReadingIsPlausible(4.1f, NAN));
  TEST_ASSERT_FALSE(s3Cw2015ReadingIsPlausible(4.1f, 120.0f));
}

void test_s3_bmp280_reading_rejects_transient_zero_or_nan_values() {
  TEST_ASSERT_TRUE(s3Bmp280ReadingIsPlausible(802.0f, 1930.0f));
  TEST_ASSERT_FALSE(s3Bmp280ReadingIsPlausible(0.0f, 0.0f));
  TEST_ASSERT_FALSE(s3Bmp280ReadingIsPlausible(NAN, 100.0f));
  TEST_ASSERT_FALSE(s3Bmp280ReadingIsPlausible(1000.0f, NAN));
}

void test_s3_speed_level_from_adc_matches_ladder_ranges() {
  TEST_ASSERT_EQUAL_UINT8(1, s3SpeedLevelFromAdc(0));
  TEST_ASSERT_EQUAL_UINT8(1, s3SpeedLevelFromAdc(1364));
  TEST_ASSERT_EQUAL_UINT8(2, s3SpeedLevelFromAdc(1365));
  TEST_ASSERT_EQUAL_UINT8(2, s3SpeedLevelFromAdc(2729));
  TEST_ASSERT_EQUAL_UINT8(3, s3SpeedLevelFromAdc(2730));
  TEST_ASSERT_EQUAL_UINT8(3, s3SpeedLevelFromAdc(4095));
}

#ifdef FAN_CONTROLLER_HIL
void test_hil_protocol_parses_sequence_and_receiver_control() {
  HilCommand command = {};
  TEST_ASSERT_EQUAL(HIL_PARSE_OK,
                    hilParseCommand("HIL 42 REMOTE CONTROL c3 -250 2 2 1", command));
  TEST_ASSERT_EQUAL_UINT32(42, command.sequence);
  TEST_ASSERT_EQUAL(HIL_COMMAND_REMOTE_CONTROL, command.type);
  TEST_ASSERT_EQUAL_STRING("c3", command.text);
  TEST_ASSERT_EQUAL_INT32(-250, command.values[0]);
  TEST_ASSERT_EQUAL_INT32(2, command.values[1]);
  TEST_ASSERT_EQUAL_INT32(2, command.values[2]);
  TEST_ASSERT_EQUAL_INT32(1, command.values[3]);
}

void test_hil_protocol_rejects_missing_extra_and_invalid_arguments() {
  HilCommand command = {};
  TEST_ASSERT_EQUAL(HIL_PARSE_INVALID_ARGUMENT, hilParseCommand("HIL 1 OUTPUTS", command));
  TEST_ASSERT_EQUAL(HIL_PARSE_INVALID_ARGUMENT, hilParseCommand("HIL 2 PING extra", command));
  TEST_ASSERT_EQUAL(HIL_PARSE_INVALID_ARGUMENT,
                    hilParseCommand("HIL 3 REMOTE CONTROL c3 nope 1 0 0", command));
  TEST_ASSERT_EQUAL(HIL_PARSE_INVALID_SEQUENCE, hilParseCommand("HIL nope PING", command));
  TEST_ASSERT_EQUAL(HIL_PARSE_UNKNOWN_COMMAND, hilParseCommand("HIL 4 FAN POWER ON", command));
}

void test_hil_protocol_has_stable_error_names_and_line_limit() {
  char line[HIL_MAX_LINE_LENGTH + 8] = {};
  memset(line, 'A', sizeof(line) - 1);
  HilCommand command = {};
  TEST_ASSERT_EQUAL(HIL_PARSE_LINE_TOO_LONG, hilParseCommand(line, command));
  TEST_ASSERT_EQUAL_STRING("line_too_long", hilParseError(HIL_PARSE_LINE_TOO_LONG));
  TEST_ASSERT_EQUAL_STRING("invalid_prefix", hilParseError(HIL_PARSE_INVALID_PREFIX));
  TEST_ASSERT_EQUAL_STRING("invalid_sequence", hilParseError(HIL_PARSE_INVALID_SEQUENCE));
  TEST_ASSERT_EQUAL_STRING("unknown_command", hilParseError(HIL_PARSE_UNKNOWN_COMMAND));
  TEST_ASSERT_EQUAL_STRING("invalid_argument", hilParseError(HIL_PARSE_INVALID_ARGUMENT));
}

void test_hil_protocol_parses_status_frame_and_decimal_sensor_value() {
  HilCommand command = {};
  TEST_ASSERT_EQUAL(HIL_PARSE_OK, hilParseCommand("HIL 8 STATUS FRAME 5A020100", command));
  TEST_ASSERT_EQUAL(HIL_COMMAND_STATUS_FRAME, command.type);
  TEST_ASSERT_EQUAL_STRING("5A020100", command.data);

  TEST_ASSERT_EQUAL(HIL_PARSE_OK, hilParseCommand("HIL 9 SENSOR MCU VALUE 43.25", command));
  TEST_ASSERT_EQUAL(HIL_COMMAND_SENSOR_VALUE, command.type);
  TEST_ASSERT_EQUAL_STRING("MCU", command.text);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 43.25f, command.realValues[0]);
  TEST_ASSERT_EQUAL(HIL_PARSE_OK, hilParseCommand("HIL 10 SENSOR BMP280 FAULT", command));
  TEST_ASSERT_EQUAL(HIL_COMMAND_SENSOR_FAULT, command.type);
}

void test_hil_safety_uses_neutral_pwm_while_locked() {
  HilOutputGate gate = hilInitialOutputGate(76);
  TEST_ASSERT_FALSE(gate.unlocked);
  hilSetExpectedPwm(gate, 102);
  TEST_ASSERT_EQUAL_INT16(102, gate.expectedPwm);
  TEST_ASSERT_EQUAL_INT16(76, hilActualPwm(gate));
  hilSetOutputsUnlocked(gate, true, 1000);
  TEST_ASSERT_EQUAL_INT16(102, hilActualPwm(gate));
  TEST_ASSERT_FALSE(hilApplyOutputWatchdog(gate, 10999, 10000));
  TEST_ASSERT_TRUE(hilApplyOutputWatchdog(gate, 11000, 10000));
  TEST_ASSERT_FALSE(gate.unlocked);
  TEST_ASSERT_EQUAL_INT16(76, hilActualPwm(gate));
}

void test_receiver_frame_decoder_accepts_v2_and_exposes_wire_fields() {
  ReceiverControlFrameV2 frame = {};
  frame.head = CONTROL_PACKET_HEAD;
  frame.type = CONTROL_PACKET_TYPE;
  frame.version = CONTROL_PROTOCOL_VERSION;
  frame.sequence = 321;
  frame.throttle = -456;
  frame.speedLevel = 2;
  frame.buttons = 0x02;
  frame.flags = CONTROL_FLAG_TAKEOVER_REQUEST;
  frame.crc = protocolCrc8(reinterpret_cast<const uint8_t *>(&frame), sizeof(frame) - 1);

  ReceiverDecodedControl decoded = {};
  TEST_ASSERT_EQUAL(RECEIVER_FRAME_OK,
                    receiverDecodeControlFrame(reinterpret_cast<const uint8_t *>(&frame), sizeof(frame), decoded));
  TEST_ASSERT_FALSE(decoded.legacy);
  TEST_ASSERT_TRUE(decoded.hasSequence);
  TEST_ASSERT_EQUAL_UINT16(321, decoded.sequence);
  TEST_ASSERT_EQUAL_INT16(-456, decoded.throttle);
  TEST_ASSERT_EQUAL_UINT8(2, decoded.speedLevel);
  TEST_ASSERT_EQUAL_UINT8(0x02, decoded.buttons);
  TEST_ASSERT_EQUAL_UINT8(CONTROL_FLAG_TAKEOVER_REQUEST, decoded.flags);
}

void test_receiver_frame_decoder_rejects_crc_and_truncated_frames() {
  ReceiverControlFrameV2 frame = {};
  frame.head = CONTROL_PACKET_HEAD;
  frame.type = CONTROL_PACKET_TYPE;
  frame.version = CONTROL_PROTOCOL_VERSION;
  frame.crc = 0x55;
  ReceiverDecodedControl decoded = {};
  TEST_ASSERT_EQUAL(RECEIVER_FRAME_BAD_CRC,
                    receiverDecodeControlFrame(reinterpret_cast<const uint8_t *>(&frame), sizeof(frame), decoded));
  TEST_ASSERT_EQUAL(RECEIVER_FRAME_BAD_LENGTH,
                    receiverDecodeControlFrame(reinterpret_cast<const uint8_t *>(&frame), sizeof(frame) - 2, decoded));
}

void test_receiver_frame_decoder_keeps_legacy_packets_migratable() {
  ReceiverControlFrameV1 frame = {};
  frame.head = CONTROL_PACKET_HEAD;
  frame.type = CONTROL_PACKET_TYPE;
  frame.throttle = 250;
  frame.speedLevel = 3;
  frame.buttons = 0;
  frame.checksum = protocolLegacyChecksum(reinterpret_cast<const uint8_t *>(&frame), sizeof(frame));
  ReceiverDecodedControl decoded = {};
  TEST_ASSERT_EQUAL(RECEIVER_FRAME_OK,
                    receiverDecodeControlFrame(reinterpret_cast<const uint8_t *>(&frame), sizeof(frame), decoded));
  TEST_ASSERT_TRUE(decoded.legacy);
  TEST_ASSERT_FALSE(decoded.hasSequence);
  TEST_ASSERT_EQUAL_INT16(250, decoded.throttle);
}
#endif

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_pwm_mapping_uses_vesc_servo_range);
  RUN_TEST(test_pwm_mapping_applies_speed_level_limits);
  RUN_TEST(test_pwm_mapping_defaults_invalid_speed_level_to_level_1);
  RUN_TEST(test_joystick_mapping_uses_calibrated_center_and_deadzone);
  RUN_TEST(test_joystick_center_calibration_averages_samples);
  RUN_TEST(test_joystick_center_rejects_invalid_persisted_values);
  RUN_TEST(test_joystick_center_adjustment_clamps_to_valid_range);
  RUN_TEST(test_joystick_neutral_range_uses_center_and_deadzone);
  RUN_TEST(test_joystick_neutral_range_calibration_uses_two_round_return_samples);
  RUN_TEST(test_joystick_mapping_uses_neutral_range_instead_of_single_center);
  RUN_TEST(test_c3_chinese_font_contains_required_ui_glyphs);
  RUN_TEST(test_c3_chinese_font_uses_smaller_render_size);
  RUN_TEST(test_c3_home_layout_uses_original_english_labels);
  RUN_TEST(test_c3_home_layout_uses_large_speed_and_framed_throttle_bar);
  RUN_TEST(test_joystick_calibration_rejects_invalid_persisted_values);
  RUN_TEST(test_joystick_calibrated_mapping_uses_persisted_range);
  RUN_TEST(test_transmitter_safety_forces_zero_until_armed);
  RUN_TEST(test_transmitter_arming_requires_full_brake_hold_for_three_seconds);
  RUN_TEST(test_transmitter_brake_hold_resets_when_brake_released);
  RUN_TEST(test_throttle_slew_rate_limits_step_changes);
  RUN_TEST(test_receiver_failsafe_clears_all_stale_control_inputs);
  RUN_TEST(test_receiver_link_alert_beeps_immediately_then_every_two_seconds);
  RUN_TEST(test_receiver_link_alert_is_silent_when_connected_and_not_failsafe);
  RUN_TEST(test_receiver_packet_gap_diagnostics_track_max_gap);
  RUN_TEST(test_remote_horn_has_maximum_continuous_duration);
  RUN_TEST(test_remote_horn_resets_after_button_release);
  RUN_TEST(test_beep_profiles_use_distinct_frequencies);
  RUN_TEST(test_receiver_connection_success_only_on_reconnect_edge);
  RUN_TEST(test_s3_lvgl_timing_uses_low_latency_profile);
  RUN_TEST(test_s3_lvgl_display_dma_is_enabled);
  RUN_TEST(test_s3_espnow_link_uses_stable_radio_profile);
  RUN_TEST(test_receiver_status_target_tracks_last_valid_transmitter);
  RUN_TEST(test_controller_source_rejects_non_active_remote_without_takeover);
  RUN_TEST(test_controller_source_allows_takeover_and_resets_stability);
  RUN_TEST(test_controller_source_releases_after_timeout);
  RUN_TEST(test_button_short_press_and_long_press_are_distinct);
  RUN_TEST(test_protocol_crc8_detects_packet_changes);
  RUN_TEST(test_protocol_sequence_accepts_fresh_values_and_rejects_stale_replays);
  RUN_TEST(test_status_sequence_resets_after_connection_timeout);
  RUN_TEST(test_protocol_status_flags_are_composable);
  RUN_TEST(test_protocol_control_takeover_flag_is_separate_from_status_lock_flag);
  RUN_TEST(test_protocol_legacy_checksum_keeps_v1_packets_migratable);
  RUN_TEST(test_diagnostic_duration_requires_explicit_long_flag_for_30_minutes);
  RUN_TEST(test_diagnostic_status_line_includes_role_and_counters);
  RUN_TEST(test_s3_battery_arc_value_clamps_and_rounds_soc);
  RUN_TEST(test_s3_speed_label_color_uses_speed_bands);
  RUN_TEST(test_s3_status_text_appends_lock_without_hiding_link_status);
  RUN_TEST(test_s3_main_status_text_prioritizes_actionable_states);
  RUN_TEST(test_s3_voltage_arc_value_clamps_to_vesc_display_range);
  RUN_TEST(test_s3_throttle_bar_keeps_center_and_uses_direction_colors);
  RUN_TEST(test_s3_compass_angle_uses_qmc_heading_in_lvgl_tenths);
  RUN_TEST(test_s3_mcu_temperature_formats_one_decimal_or_placeholder);
  RUN_TEST(test_s3_diagnostic_text_formats_link_quality);
  RUN_TEST(test_s3_ui_detail_text_helpers_format_product_pages);
  RUN_TEST(test_s3_receiver_status_text_decodes_flags);
  RUN_TEST(test_s3_persisted_brightness_uses_fallback_for_missing_value);
  RUN_TEST(test_s3_mcu_temperature_warning_threshold);
  RUN_TEST(test_s3_swipe_direction_requires_horizontal_drag);
  RUN_TEST(test_s3_bmp280_altitude_uses_standard_sea_level_pressure);
  RUN_TEST(test_s3_cw2015_reading_rejects_transient_zero_or_nan_values);
  RUN_TEST(test_s3_bmp280_reading_rejects_transient_zero_or_nan_values);
  RUN_TEST(test_s3_speed_level_from_adc_matches_ladder_ranges);
#ifdef FAN_CONTROLLER_HIL
  RUN_TEST(test_hil_protocol_parses_sequence_and_receiver_control);
  RUN_TEST(test_hil_protocol_rejects_missing_extra_and_invalid_arguments);
  RUN_TEST(test_hil_protocol_has_stable_error_names_and_line_limit);
  RUN_TEST(test_hil_protocol_parses_status_frame_and_decimal_sensor_value);
  RUN_TEST(test_hil_safety_uses_neutral_pwm_while_locked);
  RUN_TEST(test_receiver_frame_decoder_accepts_v2_and_exposes_wire_fields);
  RUN_TEST(test_receiver_frame_decoder_rejects_crc_and_truncated_frames);
  RUN_TEST(test_receiver_frame_decoder_keeps_legacy_packets_migratable);
#endif
  UNITY_END();
}

void loop() {}

int main(int, char**) {
  setup();
  return 0;
}
