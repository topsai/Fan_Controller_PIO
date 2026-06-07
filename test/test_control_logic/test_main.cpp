#include <unity.h>
#include "control_logic.h"

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

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_pwm_mapping_uses_vesc_servo_range);
  RUN_TEST(test_pwm_mapping_applies_speed_level_limits);
  RUN_TEST(test_pwm_mapping_defaults_invalid_speed_level_to_level_1);
  RUN_TEST(test_joystick_mapping_uses_calibrated_center_and_deadzone);
  RUN_TEST(test_joystick_center_calibration_averages_samples);
  RUN_TEST(test_receiver_failsafe_clears_all_stale_control_inputs);
  UNITY_END();
}

void loop() {}

int main(int, char**) {
  setup();
  return 0;
}
