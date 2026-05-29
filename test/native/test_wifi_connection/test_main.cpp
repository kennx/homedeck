#include <unity.h>

#include <Arduino.h>
#include <WiFi.h>

#include "system/wifi_connection.h"

void setUp() {
  fakeArduinoResetClock();
  WiFi = FakeWiFiClass{};
}

void tearDown() {
}

void test_connect_wifi_keeps_config_portal_ap_enabled() {
  WiFi.mode(WIFI_AP);

  const bool ok = homedeck::connectWifiPreservingAccessPoint("Home", "secret", 1000);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(WIFI_AP_STA, WiFi.modeValue);
  TEST_ASSERT_EQUAL_STRING("Home", WiFi.staSsid.c_str());
  TEST_ASSERT_EQUAL_STRING("secret", WiFi.staPassword.c_str());
}

void test_connect_wifi_uses_station_mode_without_existing_ap() {
  const bool ok = homedeck::connectWifiPreservingAccessPoint("Home", "", 1000);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(WIFI_STA, WiFi.modeValue);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_connect_wifi_keeps_config_portal_ap_enabled);
  RUN_TEST(test_connect_wifi_uses_station_mode_without_existing_ap);
  return UNITY_END();
}
