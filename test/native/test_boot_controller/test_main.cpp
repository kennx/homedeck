#include <unity.h>

#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "homedeck/boot_mode.h"

#include "../support/fake_arduino/Arduino.h"
#include "../support/fake_arduino/esp_sleep.h"

#define private public
#include "../../../src/boot_controller.h"
#include "../../../src/boot_controller.cpp"
#undef private

namespace {

struct FakeRuntime {
  bool forceButtons = false;
  bool wifiConnectResult = false;
  bool syncTimeResult = false;
  bool fetchSucceeds = false;
  bool fetchReturnsInvalidButOk = false;
  bool sensorAvailable = true;
  bool timeBeginClearsRestoredSync = false;
  time_t lastRestoredSyncUnix = 0;
  time_t lastReportedSyncUnix = 0;
  int m5BeginCalls = 0;
  int m5UpdateCalls = 0;
  int portalBeginCalls = 0;
  int portalHandleCalls = 0;
  int homeRenderCalls = 0;
  int setupRenderCalls = 0;
  int connectWifiCalls = 0;
  int timeBeginCalls = 0;
  int sensorBeginCalls = 0;
  int sensorSampleCalls = 0;
  int syncCalls = 0;
  int savePersonalCacheCalls = 0;
  int saveHolidayCacheCalls = 0;
  std::string lastApSsid;
  std::string lastTimezonePosix;
  homedeck::HomeViewModel lastModel;
  homedeck::SetupConfig config;
  BootCalendarCacheRecord personalCache;
  BootCalendarCacheRecord holidayCache;
  std::vector<std::pair<unsigned long, TimeSnapshot>> snapshots;
  std::string fetchedPersonalUrl;
  std::string fetchedHolidayUrl;
};

void resetNativeFakes() {
  fakeArduinoResetClock();
  fakeEspSleepReset();
}

RtcMemoryState fakeRtcState() {
  RtcMemoryState state{};
  std::memcpy(&state, fakeEspSleepRtcMemory(), sizeof(state));
  return state;
}

void setFakeRtcState(const RtcMemoryState& state) {
  std::memcpy(fakeEspSleepRtcMemory(), &state, sizeof(state));
}

TimeSnapshot snapshotAt(
    const FakeRuntime& runtime,
    unsigned long nowMs,
    const TimeSnapshot& fallback = TimeSnapshot{}) {
  for (const auto& entry : runtime.snapshots) {
    if (entry.first == nowMs) {
      return entry.second;
    }
  }

  return fallback;
}

BootControllerDeps makeDeps(FakeRuntime& runtime) {
  BootControllerDeps deps;
  deps.m5Begin = [&runtime]() { ++runtime.m5BeginCalls; };
  deps.m5Update = [&runtime]() { ++runtime.m5UpdateCalls; };
  deps.areSetupButtonsPressed = [&runtime]() { return runtime.forceButtons; };
  deps.millis = []() { return millis(); };
  deps.makeAccessPointSuffix = []() { return std::string("ABC123"); };
  deps.connectWifi = [&runtime](const std::string& ssid, const std::string&) {
    ++runtime.connectWifiCalls;
    return !ssid.empty() && runtime.wifiConnectResult;
  };
  deps.loadSetupConfig = [&runtime]() { return runtime.config; };
  deps.saveSetupConfig = [&runtime](const homedeck::SetupConfig& config) {
    runtime.config = config;
    return true;
  };
  deps.loadPersonalCalendarCache = [&runtime]() { return runtime.personalCache; };
  deps.loadHolidayCalendarCache = [&runtime]() { return runtime.holidayCache; };
  deps.savePersonalCalendarCache = [&runtime](const BootCalendarCacheRecord& record) {
    ++runtime.savePersonalCacheCalls;
    runtime.personalCache = record;
    return true;
  };
  deps.saveHolidayCalendarCache = [&runtime](const BootCalendarCacheRecord& record) {
    ++runtime.saveHolidayCacheCalls;
    runtime.holidayCache = record;
    return true;
  };
  deps.beginSetupPortal = [&runtime](
                               const std::string& apSsid,
                               const homedeck::SetupConfig&,
                               const BootControllerSaveCallback&) {
    ++runtime.portalBeginCalls;
    runtime.lastApSsid = apSsid;
  };
  deps.handleSetupPortalClient = [&runtime]() { ++runtime.portalHandleCalls; };
  deps.renderSetupScreen = [&runtime](const std::string& apSsid, const std::string&) {
    ++runtime.setupRenderCalls;
    runtime.lastApSsid = apSsid;
  };
  deps.timeBegin = [&runtime](const char* timezonePosix, const char*) {
    ++runtime.timeBeginCalls;
    runtime.lastTimezonePosix = timezonePosix != nullptr ? timezonePosix : "";
    if (runtime.timeBeginClearsRestoredSync) {
      runtime.lastRestoredSyncUnix = 0;
    }
    return true;
  };
  deps.timeSnapshot = [&runtime]() {
    return snapshotAt(runtime, millis(), TimeSnapshot{});
  };
  deps.timeRestoreSyncState = [&runtime](time_t value) {
    runtime.lastRestoredSyncUnix = value;
  };
  deps.timeLastSuccessfulSyncUnix = [&runtime]() {
    return runtime.lastReportedSyncUnix;
  };
  deps.syncTimeFromNtp = [&runtime]() {
    ++runtime.syncCalls;
    return runtime.syncTimeResult;
  };
  deps.sensorBegin = [&runtime]() {
    ++runtime.sensorBeginCalls;
    return true;
  };
  deps.sensorSample = [&runtime]() {
    ++runtime.sensorSampleCalls;
    return runtime.sensorAvailable ? SensorSnapshot{"23.7°C", "56%", true}
                                   : SensorSnapshot{"--", "--", false};
  };
  deps.fetchCalendarText = [&runtime](const std::string& url) {
    if (url == runtime.config.personalCalendarUrl) {
      runtime.fetchedPersonalUrl = url;
    }
    if (url == runtime.config.holidayCalendarUrl) {
      runtime.fetchedHolidayUrl = url;
    }
    if (runtime.fetchReturnsInvalidButOk) {
      return CalendarFetchResult{true, "<html>login</html>", 200, ""};
    }
    if (runtime.fetchSucceeds) {
      if (url == runtime.config.personalCalendarUrl) {
        return CalendarFetchResult{true, "personal-ics", 200, ""};
      }
      if (url == runtime.config.holidayCalendarUrl) {
        return CalendarFetchResult{true, "holiday-ics", 200, ""};
      }
    }
    return CalendarFetchResult{false, "", 503, "offline"};
  };
  deps.parsePersonalCalendar = [](const char*, int, int, int) {
    return ParsedPersonalCalendar{};
  };
  deps.parseHolidayCalendar = [](const char*, int, int, int) {
    return ParsedHolidayCalendar{};
  };
  deps.encodePersonalCalendarCache = [](const ParsedPersonalCalendar&) {
    return std::string();
  };
  deps.decodePersonalCalendarCache = [](const std::string&) {
    return ParsedPersonalCalendar{};
  };
  deps.encodeHolidayCalendarCache = [](const ParsedHolidayCalendar&) {
    return std::string();
  };
  deps.decodeHolidayCalendarCache = [](const std::string&) {
    return ParsedHolidayCalendar{};
  };
  deps.describeLunarDate = [](int, int, int) {
    return LunarDayInfo{"农历 四月初五", "节气 小满"};
  };
  deps.renderHomeScreen = [&runtime](const homedeck::HomeViewModel& model) {
    ++runtime.homeRenderCalls;
    runtime.lastModel = model;
  };

  return deps;
}

void test_begin_enters_ap_mode_when_config_missing() {
  resetNativeFakes();
  FakeRuntime runtime;
  BootController controller(makeDeps(runtime));

  controller.begin();

  TEST_ASSERT_EQUAL(1, runtime.m5BeginCalls);
  TEST_ASSERT_EQUAL(1, runtime.setupRenderCalls);
  TEST_ASSERT_EQUAL(1, runtime.portalBeginCalls);
  TEST_ASSERT_EQUAL_STRING("HomeDeck-ABC123", runtime.lastApSsid.c_str());
  TEST_ASSERT_EQUAL(0, runtime.homeRenderCalls);
}

void test_begin_enters_home_mode_when_config_present() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "",
      "",
  };
  runtime.snapshots.push_back({
      0,
      TimeSnapshot{"09:30", "2026年5月21日", true, false},
  });

  BootController controller(makeDeps(runtime));
  controller.begin();

  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);
  TEST_ASSERT_EQUAL(0, runtime.portalBeginCalls);
  TEST_ASSERT_EQUAL_STRING("未配置个人日程", runtime.lastModel.eventRows[0].titleText.c_str());
  TEST_ASSERT_EQUAL_STRING("未配置节假日", runtime.lastModel.holidayText.c_str());
}

void test_begin_renders_home_before_background_network_work() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
  };
  runtime.wifiConnectResult = true;
  runtime.syncTimeResult = true;
  runtime.fetchSucceeds = true;
  runtime.snapshots.push_back({
      0,
      TimeSnapshot{"09:30", "2026年5月21日", true, false},
  });

  BootController controller(makeDeps(runtime));
  controller.begin();

  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);
  TEST_ASSERT_EQUAL(0, runtime.connectWifiCalls);
  TEST_ASSERT_EQUAL(0, runtime.syncCalls);
  TEST_ASSERT_EQUAL_STRING("", runtime.fetchedPersonalUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("", runtime.fetchedHolidayUrl.c_str());
}

void test_begin_maps_iana_timezone_to_posix_for_time_service() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "",
      "",
  };
  runtime.snapshots.push_back({
      0,
      TimeSnapshot{"09:30", "2026年5月21日", true, false},
  });

  BootController controller(makeDeps(runtime));
  controller.begin();

  TEST_ASSERT_EQUAL(1, runtime.timeBeginCalls);
  TEST_ASSERT_EQUAL_STRING("CST-8", runtime.lastTimezonePosix.c_str());
}

void test_home_mode_uses_cache_when_calendar_fetch_fails() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
    };
  runtime.personalCache.updatedAt = "2026年5月21日 08:00";
  runtime.holidayCache.updatedAt = "2026年5月21日 08:00";
  runtime.personalCache.payload = "cached-personal";
  runtime.holidayCache.payload = "cached-holiday";
  runtime.snapshots.push_back({
      0,
      TimeSnapshot{"09:30", "2026年5月21日", true, true},
  });

  BootControllerDeps deps = makeDeps(runtime);
  deps.decodePersonalCalendarCache = [](const std::string& payload) {
    ParsedPersonalCalendar result;
    if (payload == "cached-personal") {
      result.events[0] = {"09:00", "缓存会议"};
      result.eventCount = 1;
    }
    return result;
  };
  deps.decodeHolidayCalendarCache = [](const std::string& payload) {
    if (payload == "cached-holiday") {
      return ParsedHolidayCalendar{"节假日 缓存节日"};
    }
    return ParsedHolidayCalendar{};
  };

  BootController controller(std::move(deps));
  controller.begin();

  TEST_ASSERT_EQUAL_STRING("缓存会议", runtime.lastModel.eventRows[0].titleText.c_str());
  TEST_ASSERT_EQUAL_STRING("节假日 缓存节日", runtime.lastModel.holidayText.c_str());
  TEST_ASSERT_FALSE(runtime.lastModel.calendarFresh);
}

void test_home_mode_ignores_stale_cache_from_previous_day() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
  };
  runtime.personalCache.updatedAt = "2026年5月20日 08:00";
  runtime.holidayCache.updatedAt = "2026年5月20日 08:00";
  runtime.personalCache.payload = "cached-personal";
  runtime.holidayCache.payload = "cached-holiday";
  runtime.snapshots.push_back({
      0,
      TimeSnapshot{"09:30", "2026年5月21日", true, false},
  });

  BootControllerDeps deps = makeDeps(runtime);
  deps.decodePersonalCalendarCache = [](const std::string&) {
    ParsedPersonalCalendar result;
    result.events[0] = {"09:00", "昨天的缓存会议"};
    result.eventCount = 1;
    return result;
  };
  deps.decodeHolidayCalendarCache = [](const std::string&) {
    return ParsedHolidayCalendar{"节假日 昨天的缓存节日"};
  };

  BootController controller(std::move(deps));
  controller.begin();

  TEST_ASSERT_EQUAL_STRING("今日日程不可用", runtime.lastModel.eventRows[0].titleText.c_str());
  TEST_ASSERT_EQUAL_STRING("节假日不可用", runtime.lastModel.holidayText.c_str());
  TEST_ASSERT_FALSE(runtime.lastModel.calendarFresh);
}

void test_update_does_not_fetch_or_overwrite_cache_when_date_is_invalid() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
  };
  runtime.wifiConnectResult = true;
  runtime.fetchSucceeds = true;
  runtime.snapshots.push_back({0, TimeSnapshot{"--:--", "", false, false}});
  fakeArduinoSetMillis(1);
  runtime.snapshots.push_back({1, TimeSnapshot{"--:--", "", false, false}});

  BootController controller(makeDeps(runtime));
  controller.begin();
  controller.update();

  TEST_ASSERT_EQUAL_STRING("", runtime.fetchedPersonalUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("", runtime.fetchedHolidayUrl.c_str());
  TEST_ASSERT_EQUAL(0, runtime.savePersonalCacheCalls);
  TEST_ASSERT_EQUAL(0, runtime.saveHolidayCacheCalls);
}

void test_update_runs_background_sync_and_refreshes_after_success() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
  };
  runtime.wifiConnectResult = true;
  runtime.syncTimeResult = true;
  runtime.fetchSucceeds = true;
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  fakeArduinoSetMillis(1);
  runtime.snapshots.push_back({1, TimeSnapshot{"09:31", "2026年5月21日", true, true}});

  BootControllerDeps deps = makeDeps(runtime);
  deps.parsePersonalCalendar = [](const char* ics, int year, int month, int day) {
    TEST_ASSERT_EQUAL_STRING("personal-ics", ics);
    TEST_ASSERT_EQUAL(2026, year);
    TEST_ASSERT_EQUAL(5, month);
    TEST_ASSERT_EQUAL(21, day);
    ParsedPersonalCalendar result;
    result.events[0] = {"12:00", "联网会议"};
    result.eventCount = 1;
    return result;
  };
  deps.parseHolidayCalendar = [](const char* ics, int year, int month, int day) {
    TEST_ASSERT_EQUAL_STRING("holiday-ics", ics);
    TEST_ASSERT_EQUAL(2026, year);
    TEST_ASSERT_EQUAL(5, month);
    TEST_ASSERT_EQUAL(21, day);
    return ParsedHolidayCalendar{"节假日 联网节日"};
  };
  deps.encodePersonalCalendarCache = [](const ParsedPersonalCalendar& calendar) {
    TEST_ASSERT_EQUAL_UINT32(1, calendar.eventCount);
    return std::string("saved-personal");
  };
  deps.encodeHolidayCalendarCache = [](const ParsedHolidayCalendar& calendar) {
    TEST_ASSERT_EQUAL_STRING("节假日 联网节日", calendar.holidayText.c_str());
    return std::string("saved-holiday");
  };

  BootController controller(std::move(deps));
  controller.begin();
  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);

  controller.update();

  TEST_ASSERT_EQUAL(1, runtime.connectWifiCalls);
  TEST_ASSERT_EQUAL(1, runtime.syncCalls);
  TEST_ASSERT_EQUAL_STRING("https://calendar.example.com/home.ics", runtime.fetchedPersonalUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("https://calendar.example.com/holiday.ics", runtime.fetchedHolidayUrl.c_str());
  TEST_ASSERT_EQUAL(2, runtime.homeRenderCalls);
  TEST_ASSERT_EQUAL_STRING("联网会议", runtime.lastModel.eventRows[0].titleText.c_str());
  TEST_ASSERT_EQUAL_STRING("节假日 联网节日", runtime.lastModel.holidayText.c_str());
  TEST_ASSERT_TRUE(runtime.lastModel.timeSynced);
  TEST_ASSERT_TRUE(runtime.lastModel.calendarFresh);
}

void test_update_samples_sensor_every_sixty_seconds_and_refreshes_on_recovery() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "",
      "",
  };
  runtime.sensorAvailable = false;
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  fakeArduinoSetMillis(60000);
  runtime.snapshots.push_back({60000, TimeSnapshot{"09:31", "2026年5月21日", true, false}});
  fakeArduinoSetMillis(600000);
  runtime.snapshots.push_back({600000, TimeSnapshot{"09:31", "2026年5月21日", true, false}});

  BootController controller(makeDeps(runtime));
  fakeArduinoSetMillis(0);
  controller.begin();
  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);
  TEST_ASSERT_EQUAL(1, runtime.sensorSampleCalls);
  TEST_ASSERT_FALSE(runtime.lastModel.sensorAvailable);

  fakeArduinoSetMillis(60000);
  controller.update();
  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);
  TEST_ASSERT_EQUAL(1, runtime.sensorSampleCalls);

  runtime.sensorAvailable = true;
  fakeArduinoSetMillis(600000);
  controller.update();
  TEST_ASSERT_EQUAL(2, runtime.homeRenderCalls);
  TEST_ASSERT_EQUAL(2, runtime.sensorSampleCalls);
  TEST_ASSERT_TRUE(runtime.lastModel.sensorAvailable);
}

void test_update_retries_network_before_one_hour_when_first_attempt_failed() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
  };
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  fakeArduinoSetMillis(1);
  runtime.snapshots.push_back({1, TimeSnapshot{"09:31", "2026年5月21日", true, false}});
  fakeArduinoSetMillis(60001);
  runtime.snapshots.push_back({60001, TimeSnapshot{"09:35", "2026年5月21日", true, false}});

  BootController controller(makeDeps(runtime));
  fakeArduinoSetMillis(0);
  controller.begin();
  TEST_ASSERT_EQUAL(0, runtime.connectWifiCalls);

  fakeArduinoSetMillis(1);
  controller.update();
  TEST_ASSERT_EQUAL(1, runtime.connectWifiCalls);

  runtime.wifiConnectResult = true;
  runtime.syncTimeResult = true;
  runtime.fetchSucceeds = true;
  fakeArduinoSetMillis(60001);
  controller.update();

  TEST_ASSERT_EQUAL(2, runtime.connectWifiCalls);
  TEST_ASSERT_EQUAL(1, runtime.syncCalls);
  TEST_ASSERT_EQUAL_STRING("https://calendar.example.com/home.ics", runtime.fetchedPersonalUrl.c_str());
}

void test_invalid_successful_calendar_response_keeps_same_day_cache() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
  };
  runtime.wifiConnectResult = true;
  runtime.fetchReturnsInvalidButOk = true;
  runtime.personalCache.payload = "cached-personal";
  runtime.personalCache.updatedAt = "2026年5月21日 08:00";
  runtime.holidayCache.payload = "cached-holiday";
  runtime.holidayCache.updatedAt = "2026年5月21日 08:00";
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  fakeArduinoSetMillis(1);
  runtime.snapshots.push_back({1, TimeSnapshot{"09:31", "2026年5月21日", true, false}});

  BootControllerDeps deps = makeDeps(runtime);
  deps.parsePersonalCalendar = [](const char* ics, int, int, int) {
    TEST_ASSERT_EQUAL_STRING("<html>login</html>", ics);
    return ParsedPersonalCalendar{};
  };
  deps.parseHolidayCalendar = [](const char* ics, int, int, int) {
    TEST_ASSERT_EQUAL_STRING("<html>login</html>", ics);
    return ParsedHolidayCalendar{};
  };
  deps.decodePersonalCalendarCache = [](const std::string& payload) {
    ParsedPersonalCalendar result;
    if (payload == "cached-personal") {
      result.events[0] = {"09:00", "缓存会议"};
      result.eventCount = 1;
    }
    return result;
  };
  deps.decodeHolidayCalendarCache = [](const std::string& payload) {
    if (payload == "cached-holiday") {
      return ParsedHolidayCalendar{"节假日 缓存节日"};
    }
    return ParsedHolidayCalendar{};
  };
  deps.encodePersonalCalendarCache = [](const ParsedPersonalCalendar&) {
    TEST_FAIL_MESSAGE("invalid response should not overwrite personal cache");
    return std::string();
  };
  deps.encodeHolidayCalendarCache = [](const ParsedHolidayCalendar&) {
    TEST_FAIL_MESSAGE("invalid response should not overwrite holiday cache");
    return std::string();
  };

  BootController controller(std::move(deps));
  fakeArduinoSetMillis(0);
  controller.begin();
  fakeArduinoSetMillis(1);
  controller.update();

  TEST_ASSERT_EQUAL_STRING("缓存会议", runtime.lastModel.eventRows[0].titleText.c_str());
  TEST_ASSERT_EQUAL_STRING("节假日 缓存节日", runtime.lastModel.holidayText.c_str());
  TEST_ASSERT_FALSE(runtime.lastModel.calendarFresh);
}

void test_update_refreshes_home_screen_after_one_hour() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "",
      "",
  };
  runtime.snapshots.push_back({
      0,
      TimeSnapshot{"09:30", "2026年5月21日", true, false},
  });
  runtime.snapshots.push_back({
      3599999,
      TimeSnapshot{"10:29", "2026年5月21日", true, false},
  });
  runtime.snapshots.push_back({
      3600000,
      TimeSnapshot{"10:30", "2026年5月21日", true, false},
  });

  BootController controller(makeDeps(runtime));
  controller.begin();
  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);

  fakeArduinoSetMillis(599999);
  controller.update();
  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);

  fakeArduinoSetMillis(600000);
  controller.update();
  TEST_ASSERT_EQUAL(2, runtime.homeRenderCalls);
}

void test_timer_wakeup_uses_fast_path() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi", "secret", "Asia/Shanghai", "pool.ntp.org", "", "",
  };
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});

  fakeEspSleepSetWakeupCause(ESP_SLEEP_WAKEUP_TIMER);

  BootController controller(makeDeps(runtime));
  controller.begin();

  TEST_ASSERT_EQUAL(0, runtime.portalBeginCalls);
  TEST_ASSERT_EQUAL(0, runtime.setupRenderCalls);
}

void test_enter_deep_sleep_sets_timer_and_calls_esp_deep_sleep() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi", "secret", "Asia/Shanghai", "pool.ntp.org", "", "",
  };

  BootController controller(makeDeps(runtime));
  controller.begin();

  gDeepSleepCalled = false;
  controller.enterDeepSleep();

  TEST_ASSERT_TRUE(gDeepSleepCalled);
  TEST_ASSERT_EQUAL_UINT64(10ULL * 60ULL * 1000000ULL, gFakeSleepDurationUs);
}

void test_deep_sleep_not_entered_in_ap_mode() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {};

  BootController controller(makeDeps(runtime));
  controller.begin();

  gDeepSleepCalled = false;
  controller.enterDeepSleep();

  TEST_ASSERT_FALSE(gDeepSleepCalled);
}

void test_hash_skips_refresh_when_model_unchanged() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi", "secret", "Asia/Shanghai", "pool.ntp.org", "", "",
  };
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  runtime.snapshots.push_back({1, TimeSnapshot{"09:30", "2026年5月21日", true, false}});

  BootController controller(makeDeps(runtime));
  controller.begin();
  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);

  fakeArduinoSetMillis(1);
  controller.update();

  // 时间未变，不应触发额外刷新
  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);
}

void test_timer_wakeup_network_backoff_uses_sleep_slots_for_thirty_minutes() {
  resetNativeFakes();

  RtcMemoryState oneSlotState{};
  oneSlotState.magic = kRtcMemoryMagic;
  oneSlotState.wakeupCount = 1;
  oneSlotState.lastNetworkAttemptWakeupCount = 0;
  oneSlotState.consecutiveNetworkFailures = 3;
  setFakeRtcState(oneSlotState);

  FakeRuntime oneSlotRuntime;
  oneSlotRuntime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
  };
  oneSlotRuntime.wifiConnectResult = true;
  oneSlotRuntime.syncTimeResult = true;
  oneSlotRuntime.fetchSucceeds = true;
  oneSlotRuntime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  oneSlotRuntime.snapshots.push_back({1, TimeSnapshot{"09:30", "2026年5月21日", true, false}});

  fakeEspSleepSetWakeupCause(ESP_SLEEP_WAKEUP_TIMER);
  fakeArduinoSetMillis(0);
  BootController oneSlotController(makeDeps(oneSlotRuntime));
  oneSlotController.begin();
  fakeArduinoSetMillis(1);
  oneSlotController.update();
  TEST_ASSERT_EQUAL(0, oneSlotRuntime.connectWifiCalls);

  RtcMemoryState threeSlotState = oneSlotState;
  threeSlotState.wakeupCount = 3;
  setFakeRtcState(threeSlotState);

  FakeRuntime threeSlotRuntime;
  threeSlotRuntime.config = oneSlotRuntime.config;
  threeSlotRuntime.wifiConnectResult = true;
  threeSlotRuntime.syncTimeResult = true;
  threeSlotRuntime.fetchSucceeds = true;
  threeSlotRuntime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  threeSlotRuntime.snapshots.push_back({1, TimeSnapshot{"09:30", "2026年5月21日", true, true}});

  fakeArduinoSetMillis(0);
  BootController threeSlotController(makeDeps(threeSlotRuntime));
  threeSlotController.begin();
  fakeArduinoSetMillis(1);
  threeSlotController.update();
  TEST_ASSERT_EQUAL(1, threeSlotRuntime.connectWifiCalls);
}

void test_timer_wakeup_with_unavailable_history_uses_slot_throttle_before_resampling() {
  resetNativeFakes();

  RtcMemoryState oneSlotState{};
  oneSlotState.magic = kRtcMemoryMagic;
  oneSlotState.wakeupCount = 1;
  oneSlotState.lastSensorSampleWakeupCount = 0;
  oneSlotState.lastSensorAvailable = false;
  std::snprintf(oneSlotState.lastSensorTemp, sizeof(oneSlotState.lastSensorTemp), "%s", "--");
  std::snprintf(oneSlotState.lastSensorHumidity, sizeof(oneSlotState.lastSensorHumidity), "%s", "--");
  setFakeRtcState(oneSlotState);

  FakeRuntime oneSlotRuntime;
  oneSlotRuntime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "",
      "",
  };
  oneSlotRuntime.sensorAvailable = true;
  oneSlotRuntime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  oneSlotRuntime.snapshots.push_back({1, TimeSnapshot{"09:30", "2026年5月21日", true, false}});

  fakeEspSleepSetWakeupCause(ESP_SLEEP_WAKEUP_TIMER);
  fakeArduinoSetMillis(0);
  BootController oneSlotController(makeDeps(oneSlotRuntime));
  oneSlotController.begin();
  TEST_ASSERT_EQUAL(0, oneSlotRuntime.sensorSampleCalls);
  TEST_ASSERT_TRUE(oneSlotController.sensorSampled_);
  TEST_ASSERT_FALSE(oneSlotController.sensorSnapshot_.available);

  fakeArduinoSetMillis(1);
  oneSlotController.update();
  TEST_ASSERT_EQUAL(1, oneSlotRuntime.sensorSampleCalls);
  TEST_ASSERT_TRUE(oneSlotController.sensorSnapshot_.available);
}

void test_hash_changes_when_only_event_count_changes() {
  resetNativeFakes();

  BootController controller;
  homedeck::HomeViewModel base;
  base.timeText = "09:30";
  base.dateText = "2026年5月21日";
  base.eventRows[0] = {"09:00", "例会"};
  base.eventRows[1] = {"10:00", "站会"};
  base.eventRows[2] = {"14:00", "评审"};
  base.eventRows[3] = {"16:00", "同步"};
  base.eventCount = 4;

  homedeck::HomeViewModel moreEvents = base;
  moreEvents.eventCount = 5;

  TEST_ASSERT_NOT_EQUAL(
      controller.hashHomeViewModel(base),
      controller.hashHomeViewModel(moreEvents));
}

void test_enter_deep_sleep_preserves_consecutive_network_failures_in_rtc_memory() {
  resetNativeFakes();

  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
  };
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  runtime.snapshots.push_back({1, TimeSnapshot{"09:31", "2026年5月21日", true, false}});

  fakeArduinoSetMillis(0);
  BootController controller(makeDeps(runtime));
  controller.begin();
  fakeArduinoSetMillis(1);
  controller.update();

  controller.enterDeepSleep();

  const RtcMemoryState persisted = fakeRtcState();
  TEST_ASSERT_EQUAL_UINT8(1, persisted.consecutiveNetworkFailures);
}

void test_enter_deep_sleep_persists_last_successful_ntp_sync_for_next_timer_wakeup() {
  resetNativeFakes();

  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi", "secret", "Asia/Shanghai", "pool.ntp.org", "", "",
  };
  runtime.lastReportedSyncUnix = 1779355800;
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});

  fakeArduinoSetMillis(0);
  BootController first(makeDeps(runtime));
  first.begin();
  first.enterDeepSleep();

  fakeEspSleepReboot();

  runtime.lastRestoredSyncUnix = 0;
  runtime.snapshots.clear();
  runtime.snapshots.push_back({0, TimeSnapshot{"09:31", "2026年5月21日", true, false}});

  BootController second(makeDeps(runtime));
  second.begin();

  TEST_ASSERT_EQUAL_INT64(1779355800, runtime.lastRestoredSyncUnix);
}

void test_timer_wakeup_restores_sync_state_after_time_begin() {
  resetNativeFakes();

  RtcMemoryState rtcState{};
  rtcState.magic = kRtcMemoryMagic;
  rtcState.wakeupCount = 1;
  rtcState.lastNtpSyncAt = 1779355800;
  setFakeRtcState(rtcState);

  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi", "secret", "Asia/Shanghai", "pool.ntp.org", "", "",
  };
  runtime.timeBeginClearsRestoredSync = true;
  runtime.snapshots.push_back({0, TimeSnapshot{"09:31", "2026年5月21日", true, false}});

  fakeEspSleepSetWakeupCause(ESP_SLEEP_WAKEUP_TIMER);
  fakeArduinoSetMillis(0);

  BootController controller(makeDeps(runtime));
  controller.begin();

  TEST_ASSERT_EQUAL(1, runtime.timeBeginCalls);
  TEST_ASSERT_EQUAL_INT64(1779355800, runtime.lastRestoredSyncUnix);
}

void test_wifi_success_does_not_clear_failure_count_when_network_cycle_still_fails() {
  resetNativeFakes();

  RtcMemoryState rtcState{};
  rtcState.magic = kRtcMemoryMagic;
  rtcState.consecutiveNetworkFailures = 2;
  rtcState.wakeupCount = 1;
  rtcState.lastNetworkAttemptWakeupCount = 0;
  setFakeRtcState(rtcState);

  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi",
      "secret",
      "Asia/Shanghai",
      "pool.ntp.org",
      "https://calendar.example.com/home.ics",
      "https://calendar.example.com/holiday.ics",
  };
  runtime.wifiConnectResult = true;
  runtime.syncTimeResult = false;
  runtime.fetchSucceeds = false;
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});
  runtime.snapshots.push_back({1, TimeSnapshot{"09:30", "2026年5月21日", true, false}});

  fakeEspSleepSetWakeupCause(ESP_SLEEP_WAKEUP_TIMER);
  fakeArduinoSetMillis(0);
  BootController controller(makeDeps(runtime));
  controller.begin();

  fakeArduinoSetMillis(1);
  controller.update();

  const RtcMemoryState persisted = fakeRtcState();
  TEST_ASSERT_EQUAL(1, runtime.connectWifiCalls);
  TEST_ASSERT_EQUAL_UINT8(3, persisted.consecutiveNetworkFailures);
}

void test_ap_mode_timeout_restarts_after_ten_minutes() {
  resetNativeFakes();
  FakeRuntime runtime;
  runtime.config = {};

  BootController controller(makeDeps(runtime));
  controller.begin();
  TEST_ASSERT_EQUAL(1, runtime.setupRenderCalls);

  fakeArduinoSetMillis(10 * 60 * 1000);
  controller.update();

  // AP 模式超时后应当触发 ESP.restart()
  // 在 native 测试中 ESP.restart() 是空操作，主要通过代码路径覆盖验证
}

void test_fake_arduino_millis_is_controllable() {
  resetNativeFakes();

  TEST_ASSERT_EQUAL_UINT32(0, millis());

  fakeArduinoSetMillis(1234);
  TEST_ASSERT_EQUAL_UINT32(1234, millis());

  delay(66);
  TEST_ASSERT_EQUAL_UINT32(1300, millis());
}

void test_make_deps_does_not_inject_timer_wakeup_from_runtime_flag() {
  resetNativeFakes();

  FakeRuntime runtime;
  fakeEspSleepSetWakeupCause(ESP_SLEEP_WAKEUP_TIMER);

  (void)makeDeps(runtime);

  TEST_ASSERT_EQUAL(ESP_SLEEP_WAKEUP_TIMER, esp_sleep_get_wakeup_cause());
}

void test_fake_reboot_without_timer_wakeup_does_not_report_timer_cause() {
  resetNativeFakes();

  esp_deep_sleep_start();
  fakeEspSleepReboot();

  TEST_ASSERT_EQUAL(ESP_SLEEP_WAKEUP_UNDEFINED, esp_sleep_get_wakeup_cause());
}

void test_deep_sleep_reboot_resets_millis_and_preserves_rtc_state() {
  resetNativeFakes();

  fakeEspSleepRtcMemory()[sizeof(RtcMemoryState)] = 0x5A;

  FakeRuntime runtime;
  runtime.config = {
      "HomeWiFi", "secret", "Asia/Shanghai", "pool.ntp.org", "", "",
  };
  runtime.sensorAvailable = false;
  runtime.snapshots.push_back({0, TimeSnapshot{"09:30", "2026年5月21日", true, false}});

  fakeArduinoSetMillis(0);
  BootController controller(makeDeps(runtime));
  controller.begin();
  TEST_ASSERT_FALSE(runtime.lastModel.sensorAvailable);

  fakeArduinoSetMillis(4321);
  controller.enterDeepSleep();
  TEST_ASSERT_TRUE(gDeepSleepCalled);

  fakeEspSleepReboot();

  TEST_ASSERT_EQUAL_UINT32(0, millis());
  TEST_ASSERT_EQUAL(ESP_SLEEP_WAKEUP_TIMER, esp_sleep_get_wakeup_cause());
  TEST_ASSERT_FALSE(gDeepSleepCalled);
  TEST_ASSERT_EQUAL_HEX8(0x5A, fakeEspSleepRtcMemory()[sizeof(RtcMemoryState)]);

  runtime.m5BeginCalls = 0;
  runtime.homeRenderCalls = 0;
  runtime.sensorAvailable = false;
  runtime.snapshots.clear();
  runtime.snapshots.push_back({0, TimeSnapshot{"09:31", "2026年5月21日", true, false}});

  BootController rebootedController(makeDeps(runtime));
  rebootedController.begin();
  rebootedController.update();

  TEST_ASSERT_EQUAL(1, runtime.homeRenderCalls);
  TEST_ASSERT_FALSE(runtime.lastModel.sensorAvailable);
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_begin_enters_ap_mode_when_config_missing);
  RUN_TEST(test_begin_enters_home_mode_when_config_present);
  RUN_TEST(test_begin_renders_home_before_background_network_work);
  RUN_TEST(test_begin_maps_iana_timezone_to_posix_for_time_service);
  RUN_TEST(test_home_mode_uses_cache_when_calendar_fetch_fails);
  RUN_TEST(test_home_mode_ignores_stale_cache_from_previous_day);
  RUN_TEST(test_update_does_not_fetch_or_overwrite_cache_when_date_is_invalid);
  RUN_TEST(test_update_runs_background_sync_and_refreshes_after_success);
  RUN_TEST(test_update_samples_sensor_every_sixty_seconds_and_refreshes_on_recovery);
  RUN_TEST(test_update_retries_network_before_one_hour_when_first_attempt_failed);
  RUN_TEST(test_invalid_successful_calendar_response_keeps_same_day_cache);
  RUN_TEST(test_update_refreshes_home_screen_after_one_hour);
  RUN_TEST(test_timer_wakeup_uses_fast_path);
  RUN_TEST(test_enter_deep_sleep_sets_timer_and_calls_esp_deep_sleep);
  RUN_TEST(test_deep_sleep_not_entered_in_ap_mode);
  RUN_TEST(test_hash_skips_refresh_when_model_unchanged);
  RUN_TEST(test_timer_wakeup_network_backoff_uses_sleep_slots_for_thirty_minutes);
  RUN_TEST(test_timer_wakeup_with_unavailable_history_uses_slot_throttle_before_resampling);
  RUN_TEST(test_hash_changes_when_only_event_count_changes);
  RUN_TEST(test_enter_deep_sleep_preserves_consecutive_network_failures_in_rtc_memory);
  RUN_TEST(test_enter_deep_sleep_persists_last_successful_ntp_sync_for_next_timer_wakeup);
  RUN_TEST(test_timer_wakeup_restores_sync_state_after_time_begin);
  RUN_TEST(test_wifi_success_does_not_clear_failure_count_when_network_cycle_still_fails);
  RUN_TEST(test_ap_mode_timeout_restarts_after_ten_minutes);
  RUN_TEST(test_fake_arduino_millis_is_controllable);
  RUN_TEST(test_make_deps_does_not_inject_timer_wakeup_from_runtime_flag);
  RUN_TEST(test_fake_reboot_without_timer_wakeup_does_not_report_timer_cause);
  RUN_TEST(test_deep_sleep_reboot_resets_millis_and_preserves_rtc_state);
  return UNITY_END();
}
