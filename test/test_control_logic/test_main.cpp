#include <unity.h>
#include "control_logic.h"
#include "beep_profiles.h"
#include "s3_runtime_config.h"

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

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_pwm_mapping_uses_vesc_servo_range);
  RUN_TEST(test_pwm_mapping_applies_speed_level_limits);
  RUN_TEST(test_pwm_mapping_defaults_invalid_speed_level_to_level_1);
  RUN_TEST(test_joystick_mapping_uses_calibrated_center_and_deadzone);
  RUN_TEST(test_joystick_center_calibration_averages_samples);
  RUN_TEST(test_receiver_failsafe_clears_all_stale_control_inputs);
  RUN_TEST(test_receiver_link_alert_beeps_immediately_then_every_two_seconds);
  RUN_TEST(test_receiver_link_alert_is_silent_when_connected_and_not_failsafe);
  RUN_TEST(test_remote_horn_has_maximum_continuous_duration);
  RUN_TEST(test_remote_horn_resets_after_button_release);
  RUN_TEST(test_beep_profiles_use_distinct_frequencies);
  RUN_TEST(test_receiver_connection_success_only_on_reconnect_edge);
  RUN_TEST(test_s3_lvgl_timing_uses_low_latency_profile);
  UNITY_END();
}

void loop() {}

int main(int, char**) {
  setup();
  return 0;
}
