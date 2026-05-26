#include <unity.h>

#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <driver/rtc_io.h>
#include <esp_sntp.h>
#include <esp_sleep.h>

#include <cstdlib>

#include "app_runtime.h"
#include "boot_controller.h"

void setUp() {
  M5 = FakeM5Global{};
  fakeArduinoResetClock();
  fakeSntpReset();
  fakePreferencesReset();
  fakeEspSleepReset();
  fakeEspSleepResetExt0();
  fakeRtcIoReset();
}

void tearDown() {
}

void test_enter_home_deep_sleep_configures_timer_button_c_gpio_and_display_sleep() {
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_EQUAL(1, gFakeLastPinModePin);
  TEST_ASSERT_EQUAL(INPUT_PULLUP, gFakeLastPinModeMode);
  TEST_ASSERT_TRUE(gFakeTimerWakeupConfigured);
  TEST_ASSERT_EQUAL_UINT64(43200000000ULL, gFakeSleepDurationUs);
  TEST_ASSERT_EQUAL(1, gFakeRtcGpioPullupPin);
  TEST_ASSERT_EQUAL(1, gFakeRtcGpioPulldownDisabledPin);
  TEST_ASSERT_EQUAL(1, gFakeRtcGpioSleepDirectionPin);
  TEST_ASSERT_EQUAL(RTC_GPIO_MODE_INPUT_ONLY, gFakeRtcGpioSleepDirectionMode);
  TEST_ASSERT_TRUE(gFakeSleepPdConfigured);
  TEST_ASSERT_EQUAL(ESP_PD_DOMAIN_RTC_PERIPH, gFakeSleepPdDomain);
  TEST_ASSERT_EQUAL(ESP_PD_OPTION_ON, gFakeSleepPdOption);
  TEST_ASSERT_EQUAL(1, gFakeExt0Gpio);
  TEST_ASSERT_EQUAL(0, gFakeExt0Level);
  TEST_ASSERT_EQUAL(1, M5.Display.sleepCount);
  TEST_ASSERT_EQUAL(1, M5.Display.waitDisplayCount);
  TEST_ASSERT_TRUE(gDeepSleepCalled);
}

void test_app_setup_reapplies_timezone_after_rtc_restore() {
  setenv("TZ", "UTC", 1);
  tzset();
  gFakePreferenceBools["configured"] = true;
  gFakePreferenceStrings["tz"] = "Asia/Shanghai";
  M5.Rtc.enabled = true;
  M5.Rtc.corruptTimezoneOnRestore = true;

  homedeck::appSetup();

  TEST_ASSERT_TRUE(M5.Rtc.setSystemTimeFromRtcCalled);
  TEST_ASSERT_EQUAL_STRING("CST-8", std::getenv("TZ"));
}

void test_sync_ntp_waits_for_sntp_completion_even_when_clock_is_already_modern() {
  time_t syncedUnix = 0;

  const bool ok = homedeck::syncNtpForTest("CST-8", "pool.ntp.org", &syncedUnix);

  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_INT64(0, syncedUnix);
  TEST_ASSERT_EQUAL_STRING("CST-8", gFakeTimezone.c_str());
  TEST_ASSERT_EQUAL_STRING("pool.ntp.org", gFakeNtpServer.c_str());
}

void test_sync_ntp_returns_time_after_sntp_completion() {
  gFakeDelayCallback = [](unsigned long ms) {
    fakeArduinoSetMillis(millis() + ms);
    sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
  };
  time_t syncedUnix = 0;

  const bool ok = homedeck::syncNtpForTest("CST-8", "pool.ntp.org", &syncedUnix);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_GREATER_OR_EQUAL(250, static_cast<int>(millis()));
  TEST_ASSERT_GREATER_OR_EQUAL_INT64(1704067200, syncedUnix);
}

void test_enter_home_deep_sleep_does_not_sleep_when_timer_wakeup_fails() {
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;
  gFakeTimerWakeupError = ESP_ERR_INVALID_ARG;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_FALSE(gFakeTimerWakeupConfigured);
  TEST_ASSERT_FALSE(gFakeGpioWakeupConfigured);
  TEST_ASSERT_EQUAL(0, M5.Display.sleepCount);
  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

void test_enter_home_deep_sleep_does_not_sleep_when_rtc_gpio_setup_fails() {
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;
  gFakeRtcGpioPullupError = ESP_ERR_INVALID_ARG;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_TRUE(gFakeTimerWakeupConfigured);
  TEST_ASSERT_EQUAL(-1, gFakeExt0Gpio);
  TEST_ASSERT_FALSE(gFakeSleepPdConfigured);
  TEST_ASSERT_EQUAL(0, M5.Display.sleepCount);
  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

void test_enter_home_deep_sleep_does_not_sleep_when_ext0_wakeup_fails() {
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;
  gFakeExt0WakeupError = ESP_ERR_INVALID_ARG;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_TRUE(gFakeSleepPdConfigured);
  TEST_ASSERT_EQUAL(-1, gFakeExt0Gpio);
  TEST_ASSERT_EQUAL(0, M5.Display.sleepCount);
  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_enter_home_deep_sleep_configures_timer_button_c_gpio_and_display_sleep);
  RUN_TEST(test_app_setup_reapplies_timezone_after_rtc_restore);
  RUN_TEST(test_sync_ntp_waits_for_sntp_completion_even_when_clock_is_already_modern);
  RUN_TEST(test_sync_ntp_returns_time_after_sntp_completion);
  RUN_TEST(test_enter_home_deep_sleep_does_not_sleep_when_timer_wakeup_fails);
  RUN_TEST(test_enter_home_deep_sleep_does_not_sleep_when_rtc_gpio_setup_fails);
  RUN_TEST(test_enter_home_deep_sleep_does_not_sleep_when_ext0_wakeup_fails);
  return UNITY_END();
}
