#include <unity.h>

#include <array>
#include <cstdlib>
#include <ctime>

#include "../support/fake_arduino/M5Unified.h"
#include "../support/fake_arduino/Arduino.h"
#include "../support/fake_arduino/esp_sntp.h"

FakeM5Global M5;

namespace {

time_t gFakeNow = 0;
bool gConfigTzTimeCalled = false;

time_t fakeTimeNow(time_t* out) {
  if (out != nullptr) {
    *out = gFakeNow;
  }
  return gFakeNow;
}

extern "C" tm* gmtime(const time_t* timep) {
  static tm value{};
  std::tm* result = ::gmtime_r(timep, &value);
  return result;
}

int fakeConfigTzTime(const char* tz, const char* server) {
  (void)tz;
  (void)server;
  gConfigTzTimeCalled = true;
  return 0;
}

void resetFakes() {
  M5 = FakeM5Global{};
  gFakeNow = 0;
  gConfigTzTimeCalled = false;
  gFakeDelayCallback = nullptr;
  gFakeSntpCallback = nullptr;
  setenv("TZ", "UTC0", 1);
  tzset();
}

std::array<std::uint8_t, 6> makeValidSht40Sample() {
  return {
      0x64,
      0x6D,
      0xA0,
      0x7E,
      0xF5,
      0xA0,
  };
}

std::array<std::uint8_t, 6> makeSht40BytesWithInvalidCrc(
    std::uint16_t rawTemperature,
    std::uint16_t rawHumidity) {
  return {
      static_cast<std::uint8_t>(rawTemperature >> 8U),
      static_cast<std::uint8_t>(rawTemperature & 0xFFU),
      0xFF,
      static_cast<std::uint8_t>(rawHumidity >> 8U),
      static_cast<std::uint8_t>(rawHumidity & 0xFFU),
      0xFF,
  };
}

}  // namespace

#define private public
#define time(...) fakeTimeNow(__VA_ARGS__)
#define configTzTime(...) fakeConfigTzTime(__VA_ARGS__)
#include "../../../src/time_service.cpp"
#undef private
#include "../../../src/sensor_service.cpp"
#undef configTzTime
#undef time

namespace {

void test_sync_from_ntp_does_not_mark_synced_when_only_rtc_time_exists() {
  resetFakes();
  setenv("TZ", "CST-8", 1);
  tzset();

  gFakeDelayCallback = [](unsigned long) { ++gFakeNow; };

  M5.Rtc.enabled = true;
  M5.Rtc.getDateTimeOk = true;
  M5.Rtc.dateTime = {{2026, 5, 21, 4}, {9, 30, 0}};
  gFakeNow = 1779355800;  // 2026-05-21 09:30:00 UTC，模拟 RTC 已恢复旧系统时间

  TimeService service;
  TEST_ASSERT_TRUE(service.begin("CST-8", "pool.ntp.org"));
  TEST_ASSERT_FALSE(service.syncFromNtp());

  const TimeSnapshot snapshot = service.snapshot();
  TEST_ASSERT_TRUE(snapshot.timeValid);
  TEST_ASSERT_FALSE(snapshot.timeSynced);
  TEST_ASSERT_TRUE(gConfigTzTimeCalled);
}

void test_sync_from_ntp_marks_synced_after_sntp_notification() {
  resetFakes();
  setenv("TZ", "CST-8", 1);
  tzset();

  M5.Rtc.enabled = true;
  M5.Rtc.getDateTimeOk = true;
  M5.Rtc.dateTime = {{2026, 5, 21, 4}, {9, 30, 0}};
  gFakeNow = 1779355800;
  gFakeDelayCallback = [](unsigned long) {
    gFakeNow = 1779359400;
    fakeSntpNotifySync();
  };

  TimeService service;
  TEST_ASSERT_TRUE(service.begin("CST-8", "pool.ntp.org"));
  TEST_ASSERT_TRUE(service.syncFromNtp());

  const TimeSnapshot snapshot = service.snapshot();
  TEST_ASSERT_TRUE(snapshot.timeValid);
  TEST_ASSERT_TRUE(snapshot.timeSynced);
  TEST_ASSERT_TRUE(M5.Rtc.setDateTimeCalled);
}

void test_snapshot_converts_rtc_utc_time_into_local_timezone() {
  resetFakes();
  setenv("TZ", "CST-8", 1);
  tzset();

  M5.Rtc.enabled = true;
  M5.Rtc.getDateTimeOk = true;
  M5.Rtc.dateTime = {{2026, 5, 21, 4}, {1, 30, 0}};  // RTC 存 UTC

  TimeService service;
  TEST_ASSERT_TRUE(service.begin("CST-8", "pool.ntp.org"));

  const TimeSnapshot snapshot = service.snapshot();
  TEST_ASSERT_TRUE(snapshot.timeValid);
  TEST_ASSERT_EQUAL_STRING("09:30", snapshot.timeText.c_str());
  TEST_ASSERT_EQUAL_STRING("2026年5月21日", snapshot.dateText.c_str());
}

void test_snapshot_returns_invalid_when_rtc_voltage_is_low() {
  resetFakes();
  setenv("TZ", "CST-8", 1);
  tzset();

  M5.Rtc.enabled = true;
  M5.Rtc.getDateTimeOk = true;
  M5.Rtc.voltLow = true;
  M5.Rtc.dateTime = {{2026, 5, 21, 4}, {1, 30, 0}};

  TimeService service;
  TEST_ASSERT_TRUE(service.begin("CST-8", "pool.ntp.org"));

  const TimeSnapshot snapshot = service.snapshot();
  TEST_ASSERT_FALSE(snapshot.timeValid);
}

void test_begin_does_not_restore_system_time_from_low_voltage_rtc() {
  resetFakes();
  setenv("TZ", "CST-8", 1);
  tzset();

  M5.Rtc.enabled = true;
  M5.Rtc.getDateTimeOk = true;
  M5.Rtc.voltLow = true;
  M5.Rtc.dateTime = {{2026, 5, 21, 4}, {9, 30, 0}};
  gFakeNow = 0;

  TimeService service;
  TEST_ASSERT_TRUE(service.begin("CST-8", "pool.ntp.org"));
  TEST_ASSERT_FALSE(M5.Rtc.setSystemTimeFromRtcCalled);

  const TimeSnapshot snapshot = service.snapshot();
  TEST_ASSERT_FALSE(snapshot.timeValid);
}

void test_restore_sync_state_marks_snapshot_synced_when_system_time_is_valid() {
  resetFakes();
  setenv("TZ", "CST-8", 1);
  tzset();

  gFakeNow = 1779355800;

  TimeService service;
  TEST_ASSERT_TRUE(service.begin("CST-8", "pool.ntp.org"));

  service.restoreSyncState(1779352200);

  const TimeSnapshot snapshot = service.snapshot();
  TEST_ASSERT_TRUE(snapshot.timeValid);
  TEST_ASSERT_TRUE(snapshot.timeSynced);
  TEST_ASSERT_EQUAL_INT64(1779352200, service.lastSuccessfulSyncUnix());
}

void test_restore_sync_state_throttles_ntp_within_24_hours() {
  resetFakes();
  setenv("TZ", "CST-8", 1);
  tzset();

  M5.Rtc.enabled = true;
  M5.Rtc.getDateTimeOk = true;
  gFakeNow = 1779355800;

  TimeService service;
  TEST_ASSERT_TRUE(service.begin("CST-8", "pool.ntp.org"));

  service.restoreSyncState(gFakeNow - 600);

  TEST_ASSERT_FALSE(service.syncFromNtp());
  TEST_ASSERT_FALSE(gConfigTzTimeCalled);
}

void test_restore_sync_state_does_not_throttle_ntp_for_future_timestamp() {
  resetFakes();
  setenv("TZ", "CST-8", 1);
  tzset();

  M5.Rtc.enabled = true;
  M5.Rtc.getDateTimeOk = true;
  gFakeNow = 1779355800;
  gFakeDelayCallback = [](unsigned long) {
    gFakeNow = 1779359400;
    fakeSntpNotifySync();
  };

  TimeService service;
  TEST_ASSERT_TRUE(service.begin("CST-8", "pool.ntp.org"));

  service.restoreSyncState(gFakeNow + 600);

  TEST_ASSERT_TRUE(service.syncFromNtp());
  TEST_ASSERT_TRUE(gConfigTzTimeCalled);
}

void test_restore_sync_state_does_not_mark_synced_when_system_time_is_invalid() {
  resetFakes();
  setenv("TZ", "CST-8", 1);
  tzset();

  M5.Rtc.enabled = true;
  M5.Rtc.getDateTimeOk = true;
  M5.Rtc.dateTime = {{2026, 5, 21, 4}, {1, 30, 0}};
  gFakeNow = 0;

  TimeService service;
  TEST_ASSERT_TRUE(service.begin("CST-8", "pool.ntp.org"));

  service.restoreSyncState(1779352200);

  TEST_ASSERT_FALSE(service.timeSynced_);

  const TimeSnapshot snapshot = service.snapshot();
  TEST_ASSERT_TRUE(snapshot.timeValid);
  TEST_ASSERT_EQUAL_STRING("09:30", snapshot.timeText.c_str());
  TEST_ASSERT_FALSE(snapshot.timeSynced);
  TEST_ASSERT_EQUAL_INT64(1779352200, service.lastSuccessfulSyncUnix());
}

void test_sensor_service_retries_after_transient_read_failure() {
  resetFakes();
  M5.In_I2C.enabled = true;
  M5.In_I2C.scanFound = true;

  SensorService service;
  TEST_ASSERT_TRUE(service.begin());

  M5.In_I2C.failNextRead = true;
  const SensorSnapshot failed = service.sample();
  TEST_ASSERT_FALSE(failed.available);
  TEST_ASSERT_EQUAL_STRING("--", failed.temperatureText.c_str());
  TEST_ASSERT_EQUAL_STRING("--", failed.humidityText.c_str());

  M5.In_I2C.nextReadBuffer = makeValidSht40Sample();
  const SensorSnapshot recovered = service.sample();
  TEST_ASSERT_TRUE(recovered.available);
  TEST_ASSERT_EQUAL_STRING("23.7°C", recovered.temperatureText.c_str());
  TEST_ASSERT_EQUAL_STRING("56%", recovered.humidityText.c_str());
}

void test_sensor_service_rejects_invalid_crc_payload() {
  resetFakes();
  M5.In_I2C.enabled = true;
  M5.In_I2C.scanFound = true;
  M5.In_I2C.nextReadBuffer = makeSht40BytesWithInvalidCrc(25709, 32501);

  SensorService service;
  TEST_ASSERT_TRUE(service.begin());

  const SensorSnapshot snapshot = service.sample();
  TEST_ASSERT_FALSE(snapshot.available);
  TEST_ASSERT_EQUAL_STRING("--", snapshot.temperatureText.c_str());
  TEST_ASSERT_EQUAL_STRING("--", snapshot.humidityText.c_str());
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sync_from_ntp_does_not_mark_synced_when_only_rtc_time_exists);
  RUN_TEST(test_sync_from_ntp_marks_synced_after_sntp_notification);
  RUN_TEST(test_snapshot_converts_rtc_utc_time_into_local_timezone);
  RUN_TEST(test_snapshot_returns_invalid_when_rtc_voltage_is_low);
  RUN_TEST(test_begin_does_not_restore_system_time_from_low_voltage_rtc);
  RUN_TEST(test_restore_sync_state_marks_snapshot_synced_when_system_time_is_valid);
  RUN_TEST(test_restore_sync_state_throttles_ntp_within_24_hours);
  RUN_TEST(test_restore_sync_state_does_not_throttle_ntp_for_future_timestamp);
  RUN_TEST(test_restore_sync_state_does_not_mark_synced_when_system_time_is_invalid);
  RUN_TEST(test_sensor_service_retries_after_transient_read_failure);
  RUN_TEST(test_sensor_service_rejects_invalid_crc_payload);
  return UNITY_END();
}
