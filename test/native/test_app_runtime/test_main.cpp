#include <unity.h>

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <M5Unified.h>
#include <M5PM1.h>
#include <Preferences.h>
#include <driver/rtc_io.h>
#include <esp_sntp.h>
#include <esp_sleep.h>

#include <array>
#include <cstdlib>
#include <cstdint>

#include "app/app_runtime.h"
#include "app/boot_controller.h"

namespace homedeck {
extern SystemView gRtcSavedView;
void prepareEpdAfterWakeupForTest();
void initRgbLedForTest();
void shutdownRgbLedForSleepForTest();
}

namespace {

std::uint8_t sht40Crc(std::uint8_t msb, std::uint8_t lsb) {
  std::uint8_t crc = 0xFF;
  const std::uint8_t bytes[] = {msb, lsb};
  for (const std::uint8_t byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<std::uint8_t>((crc << 1) ^ 0x31) : static_cast<std::uint8_t>(crc << 1);
    }
  }
  return crc;
}

std::array<std::uint8_t, 6> makeSht40Frame(std::uint16_t tempRaw, std::uint16_t humidityRaw) {
  const auto tMsb = static_cast<std::uint8_t>(tempRaw >> 8);
  const auto tLsb = static_cast<std::uint8_t>(tempRaw & 0xFF);
  const auto hMsb = static_cast<std::uint8_t>(humidityRaw >> 8);
  const auto hLsb = static_cast<std::uint8_t>(humidityRaw & 0xFF);
  return {tMsb, tLsb, sht40Crc(tMsb, tLsb), hMsb, hLsb, sht40Crc(hMsb, hLsb)};
}

void loadEnvironmentFrame(std::uint16_t tempRaw, std::uint16_t humidityRaw) {
  const auto frame = makeSht40Frame(tempRaw, humidityRaw);
  std::copy(frame.begin(), frame.end(), M5.In_I2C.nextReadBuffer.begin());
}

bool hasDeepSleepPrint() {
  for (const auto& print : M5.Display.prints) {
    if (print.text == "DEEP SLEEP") {
      return true;
    }
  }
  return false;
}

bool hasNonBlockingEpdWakeupSequence() {
  using Event = FakeDisplay::FakeEpdEvent;
  const auto& events = M5.Display.epdEvents;
  for (std::size_t i = 0; i + 2 < events.size(); ++i) {
    if (events[i].type != Event::Type::SetMode || events[i].mode != epd_mode_t::epd_quality) {
      continue;
    }
    if (events[i + 1].type != Event::Type::Wakeup) {
      continue;
    }
    if (events[i + 2].type != Event::Type::SetMode || events[i + 2].mode != epd_mode_t::epd_fast) {
      continue;
    }
    return true;
  }
  return false;
}

}  // namespace

void setUp() {
  M5 = FakeM5Global{};
  fakeArduinoResetClock();
  fakeSntpReset();
  fakePreferencesReset();
  fakeEspSleepReset();
  fakeEspSleepResetExt0();
  fakeRtcIoReset();
  fakeM5Pm1Reset();
  fakeNeoPixelReset();
  homedeck::gRtcSavedView = homedeck::SystemView::Almanac;
}

void tearDown() {
}

void test_enter_home_deep_sleep_configures_timer_button_c_gpio_and_display_sleep() {
  M5.In_I2C.enabled = true;
  loadEnvironmentFrame(28086, 29360);
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

void test_enter_home_deep_sleep_does_not_touch_i2c() {
  M5.In_I2C.enabled = true;
  loadEnvironmentFrame(28086, 29360);
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_EQUAL(0, M5.In_I2C.startCalls);
  TEST_ASSERT_EQUAL(0, M5.In_I2C.stopCalls);
  TEST_ASSERT_EQUAL(0, static_cast<int>(M5.In_I2C.writtenBytes.size()));
}

void test_shutdown_rgb_led_for_sleep_turns_off_rgb_pixels_and_ldo() {
  homedeck::initRgbLedForTest();

  const int clearCountBeforeSleep = gFakeNeoPixelClearCount;
  const int showCountBeforeSleep = gFakeNeoPixelShowCount;
  gFakePm1LdoEnableCalls.clear();

  homedeck::shutdownRgbLedForSleepForTest();

  TEST_ASSERT_EQUAL(clearCountBeforeSleep + 1, gFakeNeoPixelClearCount);
  TEST_ASSERT_EQUAL(showCountBeforeSleep + 1, gFakeNeoPixelShowCount);
  TEST_ASSERT_EQUAL(1, static_cast<int>(gFakePm1LdoEnableCalls.size()));
  TEST_ASSERT_FALSE(gFakePm1LdoEnableCalls[0]);
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

void test_prepare_epd_after_wakeup_uses_non_blocking_wakeup_sequence() {
  homedeck::prepareEpdAfterWakeupForTest();

  TEST_ASSERT_TRUE(hasNonBlockingEpdWakeupSequence());
  TEST_ASSERT_EQUAL(0, M5.Display.waitDisplayCount);
}

void test_init_rgb_led_enables_power_and_keeps_pixels_off() {
  homedeck::initRgbLedForTest();

  TEST_ASSERT_EQUAL(1, gFakePm1BeginCount);
  TEST_ASSERT_EQUAL(1, static_cast<int>(gFakePm1LdoEnableCalls.size()));
  TEST_ASSERT_TRUE(gFakePm1LdoEnableCalls[0]);
  TEST_ASSERT_EQUAL(1, gFakeNeoPixelBeginCount);
  TEST_ASSERT_EQUAL(1, gFakeNeoPixelClearCount);
  TEST_ASSERT_EQUAL(1, gFakeNeoPixelShowCount);
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

void test_write_rtc_utc_accepts_one_second_readback_drift() {
  M5.Rtc.enabled = true;
  M5.Rtc.getDateTimeOk = true;
  M5.Rtc.readBackSecondsOffset = -1;

  const bool ok = homedeck::writeRtcUtcForTest(1779573600);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(M5.Rtc.setDateTimeCalled);
}

void test_enter_home_deep_sleep_does_not_sleep_when_timer_wakeup_fails() {
  M5.In_I2C.enabled = true;
  loadEnvironmentFrame(28086, 29360);
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;
  gFakeTimerWakeupError = ESP_ERR_INVALID_ARG;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_FALSE(gFakeTimerWakeupConfigured);
  TEST_ASSERT_FALSE(gFakeGpioWakeupConfigured);
  TEST_ASSERT_FALSE(hasDeepSleepPrint());
  TEST_ASSERT_EQUAL(0, M5.Display.sleepCount);
  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

void test_enter_home_deep_sleep_does_not_sleep_when_rtc_gpio_setup_fails() {
  M5.In_I2C.enabled = true;
  loadEnvironmentFrame(28086, 29360);
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;
  gFakeRtcGpioPullupError = ESP_ERR_INVALID_ARG;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_TRUE(gFakeTimerWakeupConfigured);
  TEST_ASSERT_EQUAL(-1, gFakeExt0Gpio);
  TEST_ASSERT_FALSE(gFakeSleepPdConfigured);
  TEST_ASSERT_FALSE(hasDeepSleepPrint());
  TEST_ASSERT_EQUAL(0, M5.Display.sleepCount);
  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

void test_enter_home_deep_sleep_does_not_sleep_when_rtc_gpio_pulldown_disable_fails() {
  M5.In_I2C.enabled = true;
  loadEnvironmentFrame(28086, 29360);
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;
  gFakeRtcGpioPulldownDisableError = ESP_ERR_INVALID_ARG;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_TRUE(gFakeTimerWakeupConfigured);
  TEST_ASSERT_EQUAL(1, gFakeRtcGpioPullupPin);
  TEST_ASSERT_EQUAL(-1, gFakeRtcGpioPulldownDisabledPin);
  TEST_ASSERT_EQUAL(-1, gFakeRtcGpioSleepDirectionPin);
  TEST_ASSERT_FALSE(gFakeSleepPdConfigured);
  TEST_ASSERT_EQUAL(-1, gFakeExt0Gpio);
  TEST_ASSERT_FALSE(hasDeepSleepPrint());
  TEST_ASSERT_EQUAL(0, M5.Display.sleepCount);
  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

void test_enter_home_deep_sleep_does_not_sleep_when_rtc_gpio_sleep_direction_fails() {
  M5.In_I2C.enabled = true;
  loadEnvironmentFrame(28086, 29360);
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;
  gFakeRtcGpioSleepDirectionError = ESP_ERR_INVALID_ARG;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_TRUE(gFakeTimerWakeupConfigured);
  TEST_ASSERT_EQUAL(1, gFakeRtcGpioPullupPin);
  TEST_ASSERT_EQUAL(1, gFakeRtcGpioPulldownDisabledPin);
  TEST_ASSERT_EQUAL(-1, gFakeRtcGpioSleepDirectionPin);
  TEST_ASSERT_FALSE(gFakeSleepPdConfigured);
  TEST_ASSERT_EQUAL(-1, gFakeExt0Gpio);
  TEST_ASSERT_FALSE(hasDeepSleepPrint());
  TEST_ASSERT_EQUAL(0, M5.Display.sleepCount);
  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

void test_enter_home_deep_sleep_does_not_sleep_when_sleep_pd_config_fails() {
  M5.In_I2C.enabled = true;
  loadEnvironmentFrame(28086, 29360);
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;
  gFakeSleepPdConfigError = ESP_ERR_INVALID_ARG;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_TRUE(gFakeTimerWakeupConfigured);
  TEST_ASSERT_EQUAL(1, gFakeRtcGpioPullupPin);
  TEST_ASSERT_EQUAL(1, gFakeRtcGpioPulldownDisabledPin);
  TEST_ASSERT_EQUAL(1, gFakeRtcGpioSleepDirectionPin);
  TEST_ASSERT_EQUAL(RTC_GPIO_MODE_INPUT_ONLY, gFakeRtcGpioSleepDirectionMode);
  TEST_ASSERT_FALSE(gFakeSleepPdConfigured);
  TEST_ASSERT_EQUAL(-1, gFakeExt0Gpio);
  TEST_ASSERT_FALSE(hasDeepSleepPrint());
  TEST_ASSERT_EQUAL(0, M5.Display.sleepCount);
  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

void test_enter_home_deep_sleep_does_not_sleep_when_ext0_wakeup_fails() {
  M5.In_I2C.enabled = true;
  loadEnvironmentFrame(28086, 29360);
  homedeck::HomeSleepRequest request{};
  request.timerWakeupUs = 43200000000ULL;
  request.wakeupGpio = 1;
  request.wakeOnLow = true;
  gFakeExt0WakeupError = ESP_ERR_INVALID_ARG;

  homedeck::enterHomeDeepSleep(request);

  TEST_ASSERT_TRUE(gFakeSleepPdConfigured);
  TEST_ASSERT_EQUAL(-1, gFakeExt0Gpio);
  TEST_ASSERT_FALSE(hasDeepSleepPrint());
  TEST_ASSERT_EQUAL(0, M5.Display.sleepCount);
  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_enter_home_deep_sleep_configures_timer_button_c_gpio_and_display_sleep);
  RUN_TEST(test_enter_home_deep_sleep_does_not_touch_i2c);
  RUN_TEST(test_shutdown_rgb_led_for_sleep_turns_off_rgb_pixels_and_ldo);
  RUN_TEST(test_app_setup_reapplies_timezone_after_rtc_restore);
  RUN_TEST(test_prepare_epd_after_wakeup_uses_non_blocking_wakeup_sequence);
  RUN_TEST(test_init_rgb_led_enables_power_and_keeps_pixels_off);
  RUN_TEST(test_sync_ntp_waits_for_sntp_completion_even_when_clock_is_already_modern);
  RUN_TEST(test_sync_ntp_returns_time_after_sntp_completion);
  RUN_TEST(test_write_rtc_utc_accepts_one_second_readback_drift);
  RUN_TEST(test_enter_home_deep_sleep_does_not_sleep_when_timer_wakeup_fails);
  RUN_TEST(test_enter_home_deep_sleep_does_not_sleep_when_rtc_gpio_setup_fails);
  RUN_TEST(test_enter_home_deep_sleep_does_not_sleep_when_rtc_gpio_pulldown_disable_fails);
  RUN_TEST(test_enter_home_deep_sleep_does_not_sleep_when_rtc_gpio_sleep_direction_fails);
  RUN_TEST(test_enter_home_deep_sleep_does_not_sleep_when_sleep_pd_config_fails);
  RUN_TEST(test_enter_home_deep_sleep_does_not_sleep_when_ext0_wakeup_fails);
  return UNITY_END();
}
