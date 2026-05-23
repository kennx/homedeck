#include <unity.h>
#include <Arduino.h>
#include <M5Unified.h>

FakeM5Global M5;

#include "../../../src/led_service.h"
#include "../../../src/led_service.cpp"

void setUp() {
  M5.Led = FakeLed{};
  M5.Power = FakePower{};
  resetLedServicePixelStateForTest();
}

void tearDown() {}

void test_led_service_init() {
  LedService service;
  TEST_ASSERT_TRUE(service.begin());
  const LedServicePixelState& pixels = ledServicePixelStateForTest();
  TEST_ASSERT_TRUE(pixels.begun);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.brightness);
  TEST_ASSERT_EQUAL(1, pixels.showCalls);
}

void test_led_service_orange_when_charging_less_than_100() {
  M5.Power.vbus_voltage = 5000;
  M5.Power.battery_level = 80;

  LedService service;
  service.begin();
  service.update();

  const LedServicePixelState& pixels = ledServicePixelStateForTest();
  TEST_ASSERT_EQUAL_UINT8(60, pixels.brightness);
  TEST_ASSERT_EQUAL_UINT8(255, pixels.r[0]);
  TEST_ASSERT_EQUAL_UINT8(128, pixels.g[0]);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.b[0]);
  TEST_ASSERT_EQUAL_UINT8(255, pixels.r[1]);
  TEST_ASSERT_EQUAL_UINT8(128, pixels.g[1]);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.b[1]);
  TEST_ASSERT_TRUE(pixels.showCalls >= 2);
  TEST_ASSERT_FALSE(M5.Led.displayed);
}

void test_led_service_green_when_charging_at_100() {
  M5.Power.vbus_voltage = 5000;
  M5.Power.battery_level = 100;

  LedService service;
  service.begin();
  service.update();

  const LedServicePixelState& pixels = ledServicePixelStateForTest();
  TEST_ASSERT_EQUAL_UINT8(60, pixels.brightness);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.r[0]);
  TEST_ASSERT_EQUAL_UINT8(255, pixels.g[0]);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.b[0]);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.r[1]);
  TEST_ASSERT_EQUAL_UINT8(255, pixels.g[1]);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.b[1]);
}

void test_led_service_blinking_red_when_low_battery() {
  M5.Power.vbus_voltage = 0;
  M5.Power.battery_level = 3;

  LedService service;
  service.begin();
  service.update();

  const LedServicePixelState& pixels = ledServicePixelStateForTest();
  TEST_ASSERT_TRUE(pixels.brightness >= 10 && pixels.brightness <= 90);
  TEST_ASSERT_EQUAL_UINT8(255, pixels.r[0]);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.g[0]);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.b[0]);
  TEST_ASSERT_EQUAL_UINT8(255, pixels.r[1]);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.g[1]);
  TEST_ASSERT_EQUAL_UINT8(0, pixels.b[1]);
}

void test_led_service_treats_pm1_5vin_power_source_as_usb_when_vbus_voltage_unavailable() {
  M5.Power.vbus_voltage = -1;
  M5.In_I2C.enabled = true;
  M5.In_I2C.registers[0x04] = 0;

  LedService service;

  TEST_ASSERT_TRUE(service.isUsbConnected());
}

void test_led_service_does_not_treat_pm1_5vinout_power_source_as_usb() {
  M5.Power.vbus_voltage = -1;
  M5.In_I2C.enabled = true;
  M5.In_I2C.registers[0x04] = 1;

  LedService service;

  TEST_ASSERT_FALSE(service.isUsbConnected());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_led_service_init);
  RUN_TEST(test_led_service_orange_when_charging_less_than_100);
  RUN_TEST(test_led_service_green_when_charging_at_100);
  RUN_TEST(test_led_service_blinking_red_when_low_battery);
  RUN_TEST(test_led_service_treats_pm1_5vin_power_source_as_usb_when_vbus_voltage_unavailable);
  RUN_TEST(test_led_service_does_not_treat_pm1_5vinout_power_source_as_usb);
  return UNITY_END();
}
