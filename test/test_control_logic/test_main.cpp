#include <unity.h>
#include "control_logic.h"
#include "beep_profiles.h"
#include "protocol.h"
#include "diagnostic_protocol.h"
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

void test_protocol_status_flags_are_composable() {
  uint8_t flags = 0;
  flags |= STATUS_FLAG_FAILSAFE;
  flags |= STATUS_FLAG_VESC_VALID;
  TEST_ASSERT_TRUE((flags & STATUS_FLAG_FAILSAFE) != 0);
  TEST_ASSERT_TRUE((flags & STATUS_FLAG_VESC_VALID) != 0);
  TEST_ASSERT_FALSE((flags & STATUS_FLAG_PROTOCOL_FAULT) != 0);
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
  char buffer[16] = {};

  s3FormatStatusText(buffer, sizeof(buffer), true, false);
  TEST_ASSERT_EQUAL_STRING("OK LOCK", buffer);

  s3FormatStatusText(buffer, sizeof(buffer), false, false);
  TEST_ASSERT_EQUAL_STRING("LOST LOCK", buffer);

  s3FormatStatusText(buffer, sizeof(buffer), true, true);
  TEST_ASSERT_EQUAL_STRING("OK", buffer);
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
  char buffer[32] = {};
  s3FormatLinkDiagnosticText(buffer, sizeof(buffer), -62, 48, 3);
  TEST_ASSERT_EQUAL_STRING("RSSI -62 PKT 48 LOSS 3", buffer);
}

void test_s3_receiver_status_text_decodes_flags() {
  char buffer[48] = {};
  s3FormatReceiverStatusFlags(buffer, sizeof(buffer), STATUS_FLAG_FAILSAFE | STATUS_FLAG_PROTOCOL_FAULT);
  TEST_ASSERT_EQUAL_STRING("FS PROTO", buffer);

  s3FormatReceiverStatusFlags(buffer, sizeof(buffer), STATUS_FLAG_VESC_VALID | STATUS_FLAG_OUTPUT_LOCKED);
  TEST_ASSERT_EQUAL_STRING("VESC LOCK", buffer);
}

void test_s3_mcu_temperature_warning_threshold() {
  TEST_ASSERT_FALSE(s3McuTemperatureWarns(false, 90.0f, 75.0f));
  TEST_ASSERT_FALSE(s3McuTemperatureWarns(true, NAN, 75.0f));
  TEST_ASSERT_FALSE(s3McuTemperatureWarns(true, 74.9f, 75.0f));
  TEST_ASSERT_TRUE(s3McuTemperatureWarns(true, 75.0f, 75.0f));
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

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_pwm_mapping_uses_vesc_servo_range);
  RUN_TEST(test_pwm_mapping_applies_speed_level_limits);
  RUN_TEST(test_pwm_mapping_defaults_invalid_speed_level_to_level_1);
  RUN_TEST(test_joystick_mapping_uses_calibrated_center_and_deadzone);
  RUN_TEST(test_joystick_center_calibration_averages_samples);
  RUN_TEST(test_joystick_calibration_rejects_invalid_persisted_values);
  RUN_TEST(test_joystick_calibrated_mapping_uses_persisted_range);
  RUN_TEST(test_transmitter_safety_forces_zero_until_armed);
  RUN_TEST(test_transmitter_arming_requires_full_brake_hold_for_three_seconds);
  RUN_TEST(test_transmitter_brake_hold_resets_when_brake_released);
  RUN_TEST(test_throttle_slew_rate_limits_step_changes);
  RUN_TEST(test_receiver_failsafe_clears_all_stale_control_inputs);
  RUN_TEST(test_receiver_link_alert_beeps_immediately_then_every_two_seconds);
  RUN_TEST(test_receiver_link_alert_is_silent_when_connected_and_not_failsafe);
  RUN_TEST(test_remote_horn_has_maximum_continuous_duration);
  RUN_TEST(test_remote_horn_resets_after_button_release);
  RUN_TEST(test_beep_profiles_use_distinct_frequencies);
  RUN_TEST(test_receiver_connection_success_only_on_reconnect_edge);
  RUN_TEST(test_s3_lvgl_timing_uses_low_latency_profile);
  RUN_TEST(test_s3_lvgl_display_dma_is_enabled);
  RUN_TEST(test_s3_espnow_link_uses_stable_radio_profile);
  RUN_TEST(test_receiver_status_target_tracks_last_valid_transmitter);
  RUN_TEST(test_protocol_crc8_detects_packet_changes);
  RUN_TEST(test_protocol_sequence_accepts_fresh_values_and_rejects_stale_replays);
  RUN_TEST(test_protocol_status_flags_are_composable);
  RUN_TEST(test_protocol_legacy_checksum_keeps_v1_packets_migratable);
  RUN_TEST(test_diagnostic_duration_requires_explicit_long_flag_for_30_minutes);
  RUN_TEST(test_diagnostic_status_line_includes_role_and_counters);
  RUN_TEST(test_s3_battery_arc_value_clamps_and_rounds_soc);
  RUN_TEST(test_s3_speed_label_color_uses_speed_bands);
  RUN_TEST(test_s3_status_text_appends_lock_without_hiding_link_status);
  RUN_TEST(test_s3_voltage_arc_value_clamps_to_vesc_display_range);
  RUN_TEST(test_s3_throttle_bar_keeps_center_and_uses_direction_colors);
  RUN_TEST(test_s3_compass_angle_uses_qmc_heading_in_lvgl_tenths);
  RUN_TEST(test_s3_mcu_temperature_formats_one_decimal_or_placeholder);
  RUN_TEST(test_s3_diagnostic_text_formats_link_quality);
  RUN_TEST(test_s3_receiver_status_text_decodes_flags);
  RUN_TEST(test_s3_mcu_temperature_warning_threshold);
  RUN_TEST(test_s3_bmp280_altitude_uses_standard_sea_level_pressure);
  RUN_TEST(test_s3_cw2015_reading_rejects_transient_zero_or_nan_values);
  RUN_TEST(test_s3_bmp280_reading_rejects_transient_zero_or_nan_values);
  UNITY_END();
}

void loop() {}

int main(int, char**) {
  setup();
  return 0;
}
