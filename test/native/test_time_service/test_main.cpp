#include <ctime>

#include <unity.h>

#include "time_service.h"

namespace {

struct Fixture {
  bool wifiConnected = true;
  bool ntpSynced = true;
  bool rtcAvailable = true;
  bool rtcVoltLow = false;
  time_t ntpUnix = 1779573600;
  bool connectCalled = false;
  bool ntpCalled = false;
  bool writeRtcCalled = false;
  bool restoreCalled = false;
  time_t writtenUnix = 0;

  homedeck::TimeServiceDeps deps() {
    homedeck::TimeServiceDeps deps{};
    deps.connectWifi = [this](const std::string&, const std::string&) {
      connectCalled = true;
      return wifiConnected;
    };
    deps.syncNtp = [this](const std::string&, const std::string&, time_t* syncedUnix) {
      ntpCalled = true;
      if (!ntpSynced || syncedUnix == nullptr) {
        return false;
      }
      *syncedUnix = ntpUnix;
      return true;
    };
    deps.writeRtcUtc = [this](time_t unixTime) {
      writeRtcCalled = true;
      writtenUnix = unixTime;
      return true;
    };
    deps.rtcAvailable = [this]() { return rtcAvailable; };
    deps.rtcVoltLow = [this]() { return rtcVoltLow; };
    deps.restoreSystemTimeFromRtc = [this]() { restoreCalled = true; };
    return deps;
  }
};

homedeck::SetupConfig wifiAutoConfig() {
  homedeck::SetupConfig config{};
  config.wifiSsid = "Home";
  config.wifiPassword = "";
  config.timezoneIana = "Asia/Shanghai";
  config.autoRtcCorrection = true;
  config.ntpServer = "pool.ntp.org";
  return config;
}

}  // namespace

void test_ntp_success_writes_ntp_time_to_rtc() {
  Fixture f{};
  homedeck::TimeService service{f.deps()};

  const auto result = service.calibrateOnSave(wifiAutoConfig(), homedeck::ManualDateTime{});

  TEST_ASSERT_EQUAL(homedeck::TimeCalibrationStatus::SuccessNtp, result.status);
  TEST_ASSERT_TRUE(f.connectCalled);
  TEST_ASSERT_TRUE(f.ntpCalled);
  TEST_ASSERT_TRUE(f.writeRtcCalled);
  TEST_ASSERT_EQUAL_INT64(f.ntpUnix, f.writtenUnix);
}

void test_ntp_failure_uses_manual_fallback() {
  Fixture f{};
  f.ntpSynced = false;
  homedeck::TimeService service{f.deps()};
  homedeck::ManualDateTime manual{true, 2026, 5, 24, 12, 0, 0};

  const auto result = service.calibrateOnSave(wifiAutoConfig(), manual);

  TEST_ASSERT_EQUAL(homedeck::TimeCalibrationStatus::SuccessManualFallback, result.status);
  TEST_ASSERT_TRUE(f.writeRtcCalled);
}

void test_ntp_failure_without_manual_time_returns_error() {
  Fixture f{};
  f.ntpSynced = false;
  homedeck::TimeService service{f.deps()};

  const auto result = service.calibrateOnSave(wifiAutoConfig(), homedeck::ManualDateTime{});

  TEST_ASSERT_EQUAL(homedeck::TimeCalibrationStatus::FailedNeedsManualTime, result.status);
  TEST_ASSERT_FALSE(f.writeRtcCalled);
}

void test_offline_config_writes_manual_time() {
  Fixture f{};
  homedeck::TimeService service{f.deps()};
  homedeck::SetupConfig config{};
  config.timezoneIana = "Asia/Shanghai";
  homedeck::ManualDateTime manual{true, 2026, 5, 24, 12, 0, 0};

  const auto result = service.calibrateOnSave(config, manual);

  TEST_ASSERT_EQUAL(homedeck::TimeCalibrationStatus::SuccessManual, result.status);
  TEST_ASSERT_FALSE(f.connectCalled);
  TEST_ASSERT_TRUE(f.writeRtcCalled);
}

void test_restore_uses_rtc_when_available_and_not_low_voltage() {
  Fixture f{};
  homedeck::TimeService service{f.deps()};

  service.restoreSystemTimeFromRtc();

  TEST_ASSERT_TRUE(f.restoreCalled);
}

void test_restore_skips_low_voltage_rtc() {
  Fixture f{};
  f.rtcVoltLow = true;
  homedeck::TimeService service{f.deps()};

  service.restoreSystemTimeFromRtc();

  TEST_ASSERT_FALSE(f.restoreCalled);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ntp_success_writes_ntp_time_to_rtc);
  RUN_TEST(test_ntp_failure_uses_manual_fallback);
  RUN_TEST(test_ntp_failure_without_manual_time_returns_error);
  RUN_TEST(test_offline_config_writes_manual_time);
  RUN_TEST(test_restore_uses_rtc_when_available_and_not_low_voltage);
  RUN_TEST(test_restore_skips_low_voltage_rtc);
  return UNITY_END();
}
