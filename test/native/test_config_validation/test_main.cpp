#include <unity.h>

#include "config_validator.h"
#include "timezone_catalog.h"

void test_empty_wifi_requires_manual_time() {
  homedeck::SetupConfig config{};
  config.timezoneIana = "Asia/Shanghai";
  config.ntpServer = "pool.ntp.org";
  config.autoRtcCorrection = false;
  homedeck::ManualDateTime manual{};

  const auto result = homedeck::validateSetupSubmission(config, manual);

  TEST_ASSERT_EQUAL(homedeck::ConfigValidationError::MissingManualDateTime, result.error);
}

void test_empty_wifi_accepts_manual_time_and_empty_password() {
  homedeck::SetupConfig config{};
  config.timezoneIana = "Asia/Shanghai";
  config.wifiPassword = "";
  homedeck::ManualDateTime manual{true, 2026, 5, 24, 12, 30, 0};

  const auto result = homedeck::validateSetupSubmission(config, manual);

  TEST_ASSERT_EQUAL(homedeck::ConfigValidationError::None, result.error);
}

void test_wifi_auto_correction_requires_ntp_server() {
  homedeck::SetupConfig config{};
  config.wifiSsid = "Home";
  config.timezoneIana = "Asia/Shanghai";
  config.autoRtcCorrection = true;
  config.ntpServer = "";
  homedeck::ManualDateTime manual{};

  const auto result = homedeck::validateSetupSubmission(config, manual);

  TEST_ASSERT_EQUAL(homedeck::ConfigValidationError::MissingNtpServer, result.error);
}

void test_wifi_without_auto_correction_requires_manual_time() {
  homedeck::SetupConfig config{};
  config.wifiSsid = "Home";
  config.timezoneIana = "Asia/Shanghai";
  config.autoRtcCorrection = false;
  config.ntpServer = "pool.ntp.org";
  homedeck::ManualDateTime manual{};

  const auto result = homedeck::validateSetupSubmission(config, manual);

  TEST_ASSERT_EQUAL(homedeck::ConfigValidationError::MissingManualDateTime, result.error);
}

void test_parse_datetime_local_value() {
  homedeck::ManualDateTime manual{};

  const bool ok = homedeck::parseManualDateTime("2026-05-24T09:08", &manual);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(manual.present);
  TEST_ASSERT_EQUAL(2026, manual.year);
  TEST_ASSERT_EQUAL(5, manual.month);
  TEST_ASSERT_EQUAL(24, manual.day);
  TEST_ASSERT_EQUAL(9, manual.hour);
  TEST_ASSERT_EQUAL(8, manual.minute);
  TEST_ASSERT_EQUAL(0, manual.second);
}

void test_invalid_timezone_is_rejected() {
  homedeck::SetupConfig config{};
  config.timezoneIana = "Mars/Base";
  homedeck::ManualDateTime manual{true, 2026, 5, 24, 12, 0, 0};

  const auto result = homedeck::validateSetupSubmission(config, manual);

  TEST_ASSERT_EQUAL(homedeck::ConfigValidationError::InvalidTimezone, result.error);
}

void test_timezone_catalog_maps_asia_shanghai() {
  const homedeck::TimezoneInfo* info = homedeck::findTimezoneByIana("Asia/Shanghai");

  TEST_ASSERT_NOT_NULL(info);
  TEST_ASSERT_EQUAL_STRING("CST-8", info->posix);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_wifi_requires_manual_time);
  RUN_TEST(test_empty_wifi_accepts_manual_time_and_empty_password);
  RUN_TEST(test_wifi_auto_correction_requires_ntp_server);
  RUN_TEST(test_wifi_without_auto_correction_requires_manual_time);
  RUN_TEST(test_parse_datetime_local_value);
  RUN_TEST(test_invalid_timezone_is_rejected);
  RUN_TEST(test_timezone_catalog_maps_asia_shanghai);
  return UNITY_END();
}
