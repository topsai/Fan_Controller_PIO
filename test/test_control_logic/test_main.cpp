#include <unity.h>
#include "control_logic.h"
#include "beep_profiles.h"
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

void test_transmitter_arming_requires_centered_joystick() {
  TEST_ASSERT_FALSE(canArmTransmitter(80, 50));
  TEST_ASSERT_TRUE(canArmTransmitter(50, 50));
  TEST_ASSERT_TRUE(canArmTransmitter(-50, 50));
}

void test_transmitter_safety_forces_zero_until_armed() {
  TEST_ASSERT_EQUAL_INT16(0, safeThrottleForArming(700, false));
  TEST_ASSERT_EQUAL_INT16(-300, safeThrottleForArming(-300, true));
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
  TEST_ASSERT_EQUAL_UINT16(1000, S3_DISPLAY_FPS_SAMPLE_INTERVAL_MS);
}

void test_s3_lvgl_display_dma_is_enabled() {
  TEST_ASSERT_TRUE(S3_LVGL_DISPLAY_USE_DMA);
}

void test_display_fps_rounds_from_frame_count_and_elapsed_time() {
  TEST_ASSERT_EQUAL_UINT16(59, displayFpsForFrameCount(59, 1000));
  TEST_ASSERT_EQUAL_UINT16(60, displayFpsForFrameCount(30, 500));
  TEST_ASSERT_EQUAL_UINT16(0, displayFpsForFrameCount(10, 0));
}

void test_receiver_status_target_tracks_last_valid_transmitter() {
  const uint8_t s3Mac[] = {0x48, 0xCA, 0x43, 0x9A, 0xA9, 0xB0};
  uint8_t statusTarget[] = {0xAC, 0xEB, 0xE6, 0x44, 0xD5, 0x54};
  bool hasTarget = false;

  rememberStatusTarget(s3Mac, statusTarget, hasTarget);

  TEST_ASSERT_TRUE(hasTarget);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(s3Mac, statusTarget, 6);
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

void test_s3_bmp280_altitude_uses_standard_sea_level_pressure() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, s3Bmp280AltitudeMeters(1013.25f));
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 110.9f, s3Bmp280AltitudeMeters(1000.0f));
  TEST_ASSERT_TRUE(isnan(s3Bmp280AltitudeMeters(NAN)));
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_pwm_mapping_uses_vesc_servo_range);
  RUN_TEST(test_pwm_mapping_applies_speed_level_limits);
  RUN_TEST(test_pwm_mapping_defaults_invalid_speed_level_to_level_1);
  RUN_TEST(test_joystick_mapping_uses_calibrated_center_and_deadzone);
  RUN_TEST(test_joystick_center_calibration_averages_samples);
  RUN_TEST(test_transmitter_arming_requires_centered_joystick);
  RUN_TEST(test_transmitter_safety_forces_zero_until_armed);
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
  RUN_TEST(test_display_fps_rounds_from_frame_count_and_elapsed_time);
  RUN_TEST(test_receiver_status_target_tracks_last_valid_transmitter);
  RUN_TEST(test_s3_battery_arc_value_clamps_and_rounds_soc);
  RUN_TEST(test_s3_speed_label_color_uses_speed_bands);
  RUN_TEST(test_s3_voltage_arc_value_clamps_to_vesc_display_range);
  RUN_TEST(test_s3_throttle_bar_keeps_center_and_uses_direction_colors);
  RUN_TEST(test_s3_bmp280_altitude_uses_standard_sea_level_pressure);
  UNITY_END();
}

void loop() {}

int main(int, char**) {
  setup();
  return 0;
}
