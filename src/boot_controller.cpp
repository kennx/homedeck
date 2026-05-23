#include "boot_controller.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <utility>

#include "homedeck/home_screen.h"
#include "homedeck/timezone_catalog.h"

#if !defined(UNIT_TEST)
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "ap_config_portal.h"
#include "config_store.h"
#include "home_renderer.h"
#include "setup_renderer.h"
#else
#include "support/fake_arduino/esp_sleep.h"
#endif

namespace {

constexpr unsigned long kRefreshIntervalMs = 10UL * 60UL * 1000UL;
constexpr unsigned long kSensorSampleIntervalMs = 10UL * 60UL * 1000UL;
constexpr unsigned long kNetworkRetryIntervalMs = 60UL * 1000UL;
constexpr unsigned long kApModeTimeoutMs = 10UL * 60UL * 1000UL;
constexpr uint64_t kDeepSleepIntervalUs = 10ULL * 60ULL * 1000000ULL;
constexpr uint32_t kRtcMemoryMagic = 0x48444544;  // "HDED"，Task 4 升级 RTC schema

std::string makeIsoTimestamp(const TimeSnapshot& snapshot) {
  return snapshot.dateText + " " + snapshot.timeText;
}

homedeck::HomeViewModel makeUnavailableHomeModel() {
  homedeck::HomeViewModel model;
  model.temperatureText = "--";
  model.humidityText = "--";
  model.sensorAvailable = false;
  model.eventRows[0] = {"", "今日日程不可用"};
  model.holidayText = "节假日不可用";
  return model;
}

std::string buildAccessPointSsid(const std::string& suffix) {
  return std::string("HomeDeck-") + suffix;
}

bool isSameDatePrefix(const std::string& cacheUpdatedAt, const std::string& dateText) {
  return !dateText.empty() && cacheUpdatedAt.rfind(dateText, 0) == 0;
}

#if !defined(UNIT_TEST)
RTC_DATA_ATTR RtcMemoryState gRtcState;
#endif

RtcMemoryState* rtcMemoryStatePtr() {
#if defined(UNIT_TEST)
  return reinterpret_cast<RtcMemoryState*>(fakeEspSleepRtcMemory());
#else
  return &gRtcState;
#endif
}

void rtcMemoryWrite(const RtcMemoryState& state) {
  std::memcpy(rtcMemoryStatePtr(), &state, sizeof(state));
}

bool rtcMemoryRead(RtcMemoryState* state) {
  if (state == nullptr) return false;
  std::memcpy(state, rtcMemoryStatePtr(), sizeof(*state));
  return state->magic == kRtcMemoryMagic;
}

uint32_t hashString(const std::string& s) {
  uint32_t hash = 5381;
  for (unsigned char c : s) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

}  // namespace

#if !defined(UNIT_TEST)
namespace {

BootControllerDeps makeDefaultBootControllerDeps() {
  static ConfigStore configStore;
  static ApConfigPortal portal;
  static SetupRenderer setupRenderer;
  static TimeService timeService;
  static SensorService sensorService;
  static CalendarService calendarService;
  static LunarCalendarService lunarCalendarService;
  static HomeRenderer homeRenderer;

  BootControllerDeps deps;
  deps.m5Begin = []() { M5.begin(); };
  deps.m5Update = []() { M5.update(); };
  deps.areSetupButtonsPressed = []() {
    return M5.BtnA.isPressed() && M5.BtnB.isPressed();
  };
  deps.millis = []() { return ::millis(); };
  deps.makeAccessPointSuffix = []() {
    const std::uint64_t mac = ESP.getEfuseMac();
    char suffix[7] = {};
    std::snprintf(
        suffix,
        sizeof(suffix),
        "%06llX",
        static_cast<unsigned long long>(mac & 0xFFFFFFULL));
    return std::string(suffix);
  };
  deps.connectWifi = [](const std::string& ssid, const std::string& password) {
    if (ssid.empty()) {
      return false;
    }

    wifi_mode_t currentMode = WiFi.getMode();
    if (currentMode == WIFI_AP || currentMode == WIFI_AP_STA) {
      WiFi.mode(WIFI_AP_STA);
    } else {
      WiFi.mode(WIFI_STA);
    }

    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    WiFi.begin(ssid.c_str(), password.c_str());
    for (int attempt = 0; attempt < 40; ++attempt) {
      if (WiFi.status() == WL_CONNECTED) {
        return true;
      }
      delay(200);
    }
    return WiFi.status() == WL_CONNECTED;
  };
  deps.loadSetupConfig = []() {
    configStore.begin();
    return configStore.loadSetupConfig();
  };
  deps.saveSetupConfig = [](const homedeck::SetupConfig& config) {
    configStore.begin();
    return configStore.saveSetupConfig(config);
  };
  deps.loadPersonalCalendarCache = []() {
    configStore.begin();
    const CalendarCacheRecord record = configStore.loadPersonalCalendarCache();
    return BootCalendarCacheRecord{record.payload.c_str(), record.updatedAt.c_str()};
  };
  deps.loadHolidayCalendarCache = []() {
    configStore.begin();
    const CalendarCacheRecord record = configStore.loadHolidayCalendarCache();
    return BootCalendarCacheRecord{record.payload.c_str(), record.updatedAt.c_str()};
  };
  deps.savePersonalCalendarCache = [](const BootCalendarCacheRecord& record) {
    configStore.begin();
    return configStore.savePersonalCalendarCache(
        CalendarCacheRecord{record.payload.c_str(), record.updatedAt.c_str()});
  };
  deps.saveHolidayCalendarCache = [](const BootCalendarCacheRecord& record) {
    configStore.begin();
    return configStore.saveHolidayCalendarCache(
        CalendarCacheRecord{record.payload.c_str(), record.updatedAt.c_str()});
  };
  deps.beginSetupPortal = [](const std::string& apSsid,
                             const homedeck::SetupConfig& defaults,
                             const BootControllerSaveCallback& onSave) {
    portal.begin(apSsid.c_str(), defaults, onSave);
  };
  deps.handleSetupPortalClient = []() { portal.handleClient(); };
  deps.renderSetupScreen =
      [](const std::string& apSsid, const std::string& ipText) {
        setupRenderer.render(apSsid.c_str(), ipText.c_str());
      };
  deps.timeBegin = [](const char* timezonePosix, const char* ntpServer) {
    return timeService.begin(timezonePosix, ntpServer);
  };
  deps.timeSnapshot = []() { return timeService.snapshot(); };
  deps.timeRestoreSyncState = [](time_t value) {
    timeService.restoreSyncState(value);
  };
  deps.timeLastSuccessfulSyncUnix = []() {
    return timeService.lastSuccessfulSyncUnix();
  };
  deps.syncTimeFromNtp = []() { return timeService.syncFromNtp(); };
  deps.sensorBegin = []() { return sensorService.begin(); };
  deps.sensorSample = []() { return sensorService.sample(); };
  deps.fetchCalendarText = [](const std::string& url) {
    return calendarService.fetchText(url);
  };
  deps.parsePersonalCalendar =
      [](const char* ics, int year, int month, int day) {
        return calendarService.parsePersonalCalendarForDay(ics, year, month, day);
      };
  deps.parseHolidayCalendar =
      [](const char* ics, int year, int month, int day) {
        return calendarService.parseHolidayCalendarForDay(ics, year, month, day);
      };
  deps.encodePersonalCalendarCache =
      [](const ParsedPersonalCalendar& calendar) {
        return calendarService.encodePersonalCalendarCache(calendar);
      };
  deps.decodePersonalCalendarCache = [](const std::string& payload) {
    return calendarService.decodePersonalCalendarCache(payload);
  };
  deps.encodeHolidayCalendarCache =
      [](const ParsedHolidayCalendar& calendar) {
        return calendarService.encodeHolidayCalendarCache(calendar);
      };
  deps.decodeHolidayCalendarCache = [](const std::string& payload) {
    return calendarService.decodeHolidayCalendarCache(payload);
  };
  deps.describeLunarDate =
      [](int year, int month, int day) {
        return lunarCalendarService.describeDate(year, month, day);
      };
  deps.renderHomeScreen = [](const homedeck::HomeViewModel& model) {
    homeRenderer.render(model);
  };
  return deps;
}

}  // namespace
#endif

BootController::BootController()
#if !defined(UNIT_TEST)
    : BootController(makeDefaultBootControllerDeps()) {
}
#else
    : BootController(BootControllerDeps{}) {
}
#endif

BootController::BootController(BootControllerDeps deps) : deps_(std::move(deps)) {
}

void BootController::begin() {
  if (started_) {
    return;
  }

  started_ = true;
  if (deps_.m5Begin) {
    deps_.m5Begin();
  }
  if (deps_.m5Update) {
    deps_.m5Update();
  }

  if (deps_.loadSetupConfig) {
    config_ = deps_.loadSetupConfig();
  }

  RtcMemoryState rtcState{};
  if (rtcMemoryRead(&rtcState)) {
    currentWakeupCount_ = rtcState.wakeupCount;
  }

  const esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
  const bool isTimerWakeup = (wakeupCause == ESP_SLEEP_WAKEUP_TIMER);

  if (isTimerWakeup) {
    // 快速唤醒路径
    enterHomeModeFast(config_);
    return;
  }

  // 冷启动路径（保持原有逻辑）
  const homedeck::BootTarget target = homedeck::decideBootTarget({
      hasSavedConfig(config_),
      deps_.areSetupButtonsPressed ? deps_.areSetupButtonsPressed() : false,
  });

  if (target == homedeck::BootTarget::AccessPointSetup) {
    enterAccessPointMode(config_);
    return;
  }

  enterHomeMode(config_);
}

void BootController::update() {
  if (!started_) {
    return;
  }

  if (deps_.m5Update) {
    deps_.m5Update();
  }

  if (!accessPointMode_ && deps_.areSetupButtonsPressed && deps_.areSetupButtonsPressed()) {
    if (deps_.saveSetupConfig) {
      deps_.saveSetupConfig(homedeck::SetupConfig{});
    }
#if !defined(UNIT_TEST)
    ESP.restart();
#endif
  }

  if (accessPointMode_) {
    if (deps_.handleSetupPortalClient) {
      deps_.handleSetupPortalClient();
    }

    const unsigned long now = deps_.millis ? deps_.millis() : 0;
    if (now - apModeStartAtMs_ >= kApModeTimeoutMs) {
#if !defined(UNIT_TEST)
      ESP.restart();
#endif
    }
    return;
  }

  TimeSnapshot snapshot = deps_.timeSnapshot ? deps_.timeSnapshot() : TimeSnapshot{};
  const unsigned long nowMs = deps_.millis ? deps_.millis() : 0;
  const bool backgroundRefresh = runBackgroundTasks(nowMs, &snapshot);
  if (backgroundRefresh || shouldRefreshHome(snapshot)) {
    refreshHomeScreen();
  }
}

void BootController::enterAccessPointMode(const homedeck::SetupConfig& config) {
  accessPointMode_ = true;
  apModeStartAtMs_ = deps_.millis ? deps_.millis() : 0;
  const std::string apSsid = buildAccessPointSsid(
      deps_.makeAccessPointSuffix ? deps_.makeAccessPointSuffix()
                                  : std::string("000000"));
  const BootControllerSaveCallback onSave = [this](const homedeck::SetupConfig& saved) {
    return deps_.saveSetupConfig && deps_.saveSetupConfig(saved) ? 0 : 2;
  };

  if (deps_.beginSetupPortal) {
    deps_.beginSetupPortal(apSsid, config, onSave);
  }
  if (deps_.renderSetupScreen) {
    deps_.renderSetupScreen(apSsid, "192.168.4.1");
  }
}

void BootController::enterHomeMode(const homedeck::SetupConfig& config) {
  accessPointMode_ = false;
  wifiConnected_ = false;
  networkCycleStarted_ = false;
  lastNetworkCycleAtMs_ = 0;
  activeCalendarDate_.clear();
  personalCalendar_ = ParsedPersonalCalendar{};
  personalCalendarAvailable_ = false;
  personalCalendarFresh_ = false;
  holidayCalendar_ = ParsedHolidayCalendar{};
  holidayCalendarAvailable_ = false;
  holidayCalendarFresh_ = false;
  sensorSnapshot_ = SensorSnapshot{};
  sensorSampled_ = false;
  lastSensorSampleAtMs_ = 0;
  lastSensorSampleWakeupCount_ = 0;
  lastNetworkAttemptAtMs_ = 0;
  lastNetworkAttemptWakeupCount_ = 0;

  if (deps_.timeBegin) {
    const std::string timezonePosix = resolveTimezonePosix(config.timezoneIana);
    deps_.timeBegin(timezonePosix.c_str(), config.ntpServer.c_str());
  }
  if (deps_.sensorBegin) {
    deps_.sensorBegin();
  }
  if (deps_.sensorSample) {
    sensorSnapshot_ = deps_.sensorSample();
    sensorSampled_ = true;
    lastSensorSampleAtMs_ = deps_.millis ? deps_.millis() : 0;
    lastSensorSampleWakeupCount_ = currentWakeupCount_;
  }

  const TimeSnapshot snapshot = deps_.timeSnapshot ? deps_.timeSnapshot() : TimeSnapshot{};
  if (snapshot.timeValid) {
    syncCalendarStateForSnapshot(snapshot);
  }

  refreshHomeScreen();
}

void BootController::enterHomeModeFast(const homedeck::SetupConfig& config) {
  accessPointMode_ = false;
  wifiConnected_ = false;
  networkCycleStarted_ = true;
  lastNetworkCycleAtMs_ = 0;
  activeCalendarDate_.clear();
  personalCalendar_ = ParsedPersonalCalendar{};
  personalCalendarAvailable_ = false;
  personalCalendarFresh_ = false;
  holidayCalendar_ = ParsedHolidayCalendar{};
  holidayCalendarAvailable_ = false;
  holidayCalendarFresh_ = false;
  sensorSnapshot_ = SensorSnapshot{};
  sensorSampled_ = false;
  lastSensorSampleAtMs_ = 0;
  lastSensorSampleWakeupCount_ = 0;
  lastNetworkAttemptAtMs_ = 0;
  lastNetworkAttemptWakeupCount_ = 0;
  lastModelHash_ = 0;

  // 从 RTC 内存恢复 Task 3 的传感器/slot 状态。
  restoreStateFromRtcMemory();
  if (sensorSnapshot_.available || !sensorSnapshot_.temperatureText.empty() ||
      !sensorSnapshot_.humidityText.empty()) {
    sensorSampled_ = true;
  }

  // 仍然调用 timeBegin（耗时很小，确保时区正确）
  if (deps_.timeBegin) {
    const std::string timezonePosix = resolveTimezonePosix(config.timezoneIana);
    deps_.timeBegin(timezonePosix.c_str(), config.ntpServer.c_str());
  }

  RtcMemoryState rtcState{};
  if (rtcMemoryRead(&rtcState) && deps_.timeRestoreSyncState) {
    deps_.timeRestoreSyncState(rtcState.lastNtpSyncAt);
  }

  // 传感器初始化仍然需要（I2C 状态在 deep sleep 后丢失）
  if (deps_.sensorBegin) {
    deps_.sensorBegin();
  }

  const TimeSnapshot snapshot = deps_.timeSnapshot ? deps_.timeSnapshot() : TimeSnapshot{};
  if (snapshot.timeValid) {
    syncCalendarStateForSnapshot(snapshot);
  }
}

uint32_t BootController::hashHomeViewModel(const homedeck::HomeViewModel& model) const {
  uint32_t hash = 5381;
  auto mix = [&hash](const std::string& s) {
    for (unsigned char c : s) {
      hash = ((hash << 5) + hash) + c;
    }
  };
  mix(model.timeText);
  mix(model.dateText);
  mix(model.lunarText);
  mix(model.solarTermText);
  mix(model.holidayText);
  mix(model.temperatureText);
  mix(model.humidityText);
  hash = ((hash << 5) + hash) + model.eventCount;
  for (const auto& row : model.eventRows) {
    mix(row.timeText);
    mix(row.titleText);
  }
  hash = (hash << 1) | (model.wifiConnected ? 1 : 0);
  hash = (hash << 1) | (model.timeSynced ? 1 : 0);
  hash = (hash << 1) | (model.sensorAvailable ? 1 : 0);
  hash = (hash << 1) | (model.calendarFresh ? 1 : 0);
  return hash;
}

void BootController::refreshHomeScreen() {
  const TimeSnapshot snapshot = deps_.timeSnapshot ? deps_.timeSnapshot() : TimeSnapshot{};
  const homedeck::HomeViewModel model =
      buildHomeViewModel(config_, snapshot, wifiConnected_);

  const uint32_t newHash = hashHomeViewModel(model);
  if (newHash == lastModelHash_ && lastModelHash_ != 0) {
    // 内容未变化，跳过刷新
    lastRefreshAtMs_ = deps_.millis ? deps_.millis() : 0;
    return;
  }

  lastModelHash_ = newHash;

  if (deps_.renderHomeScreen) {
    deps_.renderHomeScreen(model);
  }

  lastRefreshAtMs_ = deps_.millis ? deps_.millis() : 0;
  lastRenderedDate_ = snapshot.dateText;
}

bool BootController::runBackgroundTasks(unsigned long nowMs, TimeSnapshot* snapshot) {
  bool shouldRefresh = updateSensorState(nowMs);

  const bool dateChanged = snapshot != nullptr && snapshot->timeValid &&
      snapshot->dateText != activeCalendarDate_;
  if (!networkCycleStarted_ || dateChanged) {
    // 日期变化或首次启动，立即执行网络周期
  } else if (!shouldRunNetworkCycle(nowMs)) {
    return shouldRefresh;
  }

  networkCycleStarted_ = true;
  lastNetworkAttemptAtMs_ = nowMs;
  lastNetworkAttemptWakeupCount_ = currentWakeupCount_;
  wifiConnected_ = deps_.connectWifi
      ? deps_.connectWifi(config_.wifiSsid, config_.wifiPassword)
      : false;

  if (!wifiConnected_) {
    recordNetworkFailure();
  }

  const bool wasTimeSynced = snapshot != nullptr ? snapshot->timeSynced : false;
  bool timeSyncedNow = false;
  if (wifiConnected_ && deps_.syncTimeFromNtp) {
    timeSyncedNow = deps_.syncTimeFromNtp();
  }
  if (snapshot != nullptr && deps_.timeSnapshot) {
    *snapshot = deps_.timeSnapshot();
  }

  if (snapshot == nullptr || !snapshot->timeValid) {
    return shouldRefresh || (timeSyncedNow && !wasTimeSynced);
  }

  const bool personalWasFresh = personalCalendarFresh_;
  const bool holidayWasFresh = holidayCalendarFresh_;
  syncCalendarStateForSnapshot(*snapshot);
  const bool networkCycleSucceeded = wifiConnected_ && timeSyncedNow &&
      personalCalendarFresh_ && holidayCalendarFresh_;
  if (networkCycleSucceeded) {
    resetNetworkFailureCount();
  } else if (wifiConnected_) {
    recordNetworkFailure();
  }
  if (personalCalendarFresh_ && holidayCalendarFresh_) {
    lastNetworkCycleAtMs_ = nowMs;
  }

  if ((timeSyncedNow && !wasTimeSynced) || dateChanged ||
      (!personalWasFresh && personalCalendarFresh_) ||
      (!holidayWasFresh && holidayCalendarFresh_)) {
    shouldRefresh = true;
  }

  return shouldRefresh;
}

bool BootController::updateSensorState(unsigned long nowMs) {
  if (!deps_.sensorSample) {
    return false;
  }

  if (!sensorSampled_) {
    sensorSnapshot_ = deps_.sensorSample();
    sensorSampled_ = true;
    lastSensorSampleAtMs_ = nowMs;
    lastSensorSampleWakeupCount_ = currentWakeupCount_;
    return false;
  }

  const bool millisElapsed = nowMs - lastSensorSampleAtMs_ >= kSensorSampleIntervalMs;
  const bool slotElapsed = hasElapsedSleepSlots(
      currentWakeupCount_, lastSensorSampleWakeupCount_, kSensorSampleIntervalMs);
  if (!millisElapsed && !slotElapsed) {
    return false;
  }

  const bool wasAvailable = sensorSnapshot_.available;
  sensorSnapshot_ = deps_.sensorSample();
  lastSensorSampleAtMs_ = nowMs;
  lastSensorSampleWakeupCount_ = currentWakeupCount_;
  return !wasAvailable && sensorSnapshot_.available;
}

void BootController::syncCalendarStateForSnapshot(const TimeSnapshot& snapshot) {
  int year = 0;
  int month = 0;
  int day = 0;
  if (!parseSnapshotDate(snapshot, &year, &month, &day)) {
    return;
  }

  activeCalendarDate_ = snapshot.dateText;
  personalCalendarFresh_ = fetchPersonalCalendar(snapshot, year, month, day);
  holidayCalendarFresh_ = fetchHolidayCalendar(snapshot, year, month, day);
}

bool BootController::fetchPersonalCalendar(
    const TimeSnapshot& snapshot,
    int year,
    int month,
    int day) {
  personalCalendar_ = ParsedPersonalCalendar{};
  personalCalendarAvailable_ = false;

  if (config_.personalCalendarUrl.empty()) {
    return true;
  }

  if (wifiConnected_ && deps_.fetchCalendarText) {
    const CalendarFetchResult fetched = deps_.fetchCalendarText(config_.personalCalendarUrl);
    if (fetched.ok && deps_.parsePersonalCalendar) {
      const ParsedPersonalCalendar parsed =
          deps_.parsePersonalCalendar(fetched.body.c_str(), year, month, day);
      if (parsed.eventCount > 0 ||
          fetched.body.find("BEGIN:VCALENDAR") != std::string::npos) {
        personalCalendar_ = parsed;
        personalCalendarAvailable_ = true;
        if (deps_.savePersonalCalendarCache && deps_.encodePersonalCalendarCache) {
          deps_.savePersonalCalendarCache({
              deps_.encodePersonalCalendarCache(personalCalendar_),
              makeIsoTimestamp(snapshot),
          });
        }
        return true;
      }
    }
  }

  const BootCalendarCacheRecord cache = deps_.loadPersonalCalendarCache
      ? deps_.loadPersonalCalendarCache()
      : BootCalendarCacheRecord{};
  if (isCacheCurrentForDate(cache, snapshot.dateText) &&
      deps_.decodePersonalCalendarCache) {
    personalCalendar_ = deps_.decodePersonalCalendarCache(cache.payload);
    personalCalendarAvailable_ = true;
  }

  return false;
}

bool BootController::fetchHolidayCalendar(
    const TimeSnapshot& snapshot,
    int year,
    int month,
    int day) {
  holidayCalendar_ = ParsedHolidayCalendar{};
  holidayCalendarAvailable_ = false;

  if (config_.holidayCalendarUrl.empty()) {
    return true;
  }

  if (wifiConnected_ && deps_.fetchCalendarText) {
    const CalendarFetchResult fetched = deps_.fetchCalendarText(config_.holidayCalendarUrl);
    if (fetched.ok && deps_.parseHolidayCalendar) {
      const ParsedHolidayCalendar parsed =
          deps_.parseHolidayCalendar(fetched.body.c_str(), year, month, day);
      if (!parsed.holidayText.empty() ||
          fetched.body.find("BEGIN:VCALENDAR") != std::string::npos) {
        holidayCalendar_ = parsed;
        holidayCalendarAvailable_ = true;
        if (deps_.saveHolidayCalendarCache && deps_.encodeHolidayCalendarCache) {
          deps_.saveHolidayCalendarCache({
              deps_.encodeHolidayCalendarCache(holidayCalendar_),
              makeIsoTimestamp(snapshot),
          });
        }
        return true;
      }
    }
  }

  const BootCalendarCacheRecord cache = deps_.loadHolidayCalendarCache
      ? deps_.loadHolidayCalendarCache()
      : BootCalendarCacheRecord{};
  if (isCacheCurrentForDate(cache, snapshot.dateText) &&
      deps_.decodeHolidayCalendarCache) {
    holidayCalendar_ = deps_.decodeHolidayCalendarCache(cache.payload);
    holidayCalendarAvailable_ = true;
  }

  return false;
}

std::string BootController::resolveTimezonePosix(const std::string& timezoneIana) const {
  const auto* timezone = homedeck::findTimezoneByIana(timezoneIana);
  if (timezone == nullptr) {
    return timezoneIana;
  }

  return std::string(timezone->posix);
}

bool BootController::isCacheCurrentForDate(
    const BootCalendarCacheRecord& record,
    const std::string& dateText) const {
  return !record.payload.empty() && isSameDatePrefix(record.updatedAt, dateText);
}

homedeck::HomeViewModel BootController::buildHomeViewModel(
    const homedeck::SetupConfig& config,
    const TimeSnapshot& timeSnapshot,
    bool wifiConnected) {
  homedeck::HomeViewModel model = makeUnavailableHomeModel();
  model.timeText = timeSnapshot.timeText;
  model.dateText = timeSnapshot.dateText;
  model.wifiConnected = wifiConnected;
  model.timeSynced = timeSnapshot.timeSynced;
  model.temperatureText = sensorSnapshot_.temperatureText;
  model.humidityText = sensorSnapshot_.humidityText;
  model.sensorAvailable = sensorSnapshot_.available;

  int year = 0;
  int month = 0;
  int day = 0;
  if (timeSnapshot.timeValid && parseSnapshotDate(timeSnapshot, &year, &month, &day) &&
      deps_.describeLunarDate) {
    const LunarDayInfo lunar = deps_.describeLunarDate(year, month, day);
    model.lunarText = lunar.lunarText;
    model.solarTermText = lunar.solarTermText;
  }

  bool calendarFresh = true;

  if (config.personalCalendarUrl.empty()) {
    model.eventRows[0] = {"", "未配置个人日程"};
    model.eventCount = 0;
  } else {
    if (!personalCalendarFresh_) {
      calendarFresh = false;
    }

    if (!timeSnapshot.timeValid || !personalCalendarAvailable_) {
      model.eventRows[0] = {"", "今日日程不可用"};
      model.eventCount = 0;
    } else if (personalCalendar_.eventCount == 0) {
      model.eventRows[0] = {"", homedeck::makeEmptyEventMessage(true, true)};
      model.eventCount = 0;
    } else {
      std::array<homedeck::EventRow, 5> source{};
      const std::uint32_t copyCount = std::min<std::uint32_t>(
          static_cast<std::uint32_t>(source.size()), personalCalendar_.eventCount);
      for (std::uint32_t index = 0; index < copyCount; ++index) {
        source[index] = personalCalendar_.events[index];
      }

      const homedeck::VisibleEvents visible =
          homedeck::clampTodayEvents(source, personalCalendar_.eventCount, 4);
      for (std::uint32_t index = 0; index < visible.visibleCount; ++index) {
        model.eventRows[index] = visible.visible[index];
      }
      model.eventCount = personalCalendar_.eventCount;
    }
  }

  if (config.holidayCalendarUrl.empty()) {
    model.holidayText = "未配置节假日";
  } else {
    if (!holidayCalendarFresh_) {
      calendarFresh = false;
    }

    if (!timeSnapshot.timeValid || !holidayCalendarAvailable_) {
      model.holidayText = "节假日不可用";
    } else {
      model.holidayText =
          homedeck::makeHolidayText(true, holidayCalendar_.holidayText);
    }
  }

  model.calendarFresh = calendarFresh;
  return model;
}

bool BootController::hasSavedConfig(const homedeck::SetupConfig& config) const {
  return !config.wifiSsid.empty() && !config.timezoneIana.empty() &&
      !config.ntpServer.empty();
}

bool BootController::parseSnapshotDate(
    const TimeSnapshot& snapshot,
    int* year,
    int* month,
    int* day) const {
  if (year == nullptr || month == nullptr || day == nullptr) {
    return false;
  }

  return std::sscanf(snapshot.dateText.c_str(), "%d年%d月%d日", year, month, day) == 3;
}

bool BootController::shouldRefreshHome(const TimeSnapshot& snapshot) const {
  if (accessPointMode_) {
    return false;
  }

  if (snapshot.dateText != lastRenderedDate_ && !snapshot.dateText.empty()) {
    return true;
  }

  const unsigned long now = deps_.millis ? deps_.millis() : 0;
  return now - lastRefreshAtMs_ >= kRefreshIntervalMs;
}

bool BootController::shouldRunNetworkCycle(unsigned long nowMs) const {
  const bool dateChanged = false;  // 由调用方处理
  if (dateChanged) {
    return true;
  }

  // 指数退避：根据连续失败次数决定间隔
  unsigned long interval = kRefreshIntervalMs;
  if (!personalCalendarFresh_ || !holidayCalendarFresh_) {
    RtcMemoryState rtcState{};
    const uint8_t failures = rtcMemoryRead(&rtcState) ? rtcState.consecutiveNetworkFailures : 0;
    if (failures >= 5) {
      interval = 60UL * 60UL * 1000UL;  // 60 分钟
    } else if (failures >= 3) {
      interval = 30UL * 60UL * 1000UL;  // 30 分钟
    } else {
      interval = kNetworkRetryIntervalMs;  // 60 秒（保持原有）
    }
  }

  if (!networkCycleStarted_) {
    return true;
  }

  const bool millisElapsed = nowMs - lastNetworkAttemptAtMs_ >= interval;
  const bool slotElapsed = hasElapsedSleepSlots(
      currentWakeupCount_, lastNetworkAttemptWakeupCount_, interval);
  return millisElapsed || slotElapsed;
}

bool BootController::hasElapsedSleepSlots(
    uint32_t currentWakeupCount,
    uint32_t lastWakeupCount,
    unsigned long intervalMs) const {
  if (currentWakeupCount <= lastWakeupCount) {
    return false;
  }

  const uint64_t elapsedSleepMs =
      static_cast<uint64_t>(currentWakeupCount - lastWakeupCount) *
      (kDeepSleepIntervalUs / 1000ULL);
  return elapsedSleepMs >= intervalMs;
}

void BootController::recordNetworkFailure() {
  RtcMemoryState state{};
  if (!rtcMemoryRead(&state)) {
    state = RtcMemoryState{};
    state.magic = kRtcMemoryMagic;
  }
  if (state.consecutiveNetworkFailures < 255) {
    state.consecutiveNetworkFailures++;
  }
  rtcMemoryWrite(state);
}

void BootController::resetNetworkFailureCount() {
  RtcMemoryState state{};
  if (!rtcMemoryRead(&state)) {
    state = RtcMemoryState{};
  }
  state.magic = kRtcMemoryMagic;
  state.consecutiveNetworkFailures = 0;
  rtcMemoryWrite(state);
}

void BootController::saveStateToRtcMemory() {
  RtcMemoryState previous{};
  const bool hasPrevious = rtcMemoryRead(&previous);

  RtcMemoryState state{};
  state.magic = kRtcMemoryMagic;
  state.wakeupCount = currentWakeupCount_ + 1;
  state.lastNtpSyncAt = deps_.timeLastSuccessfulSyncUnix
      ? deps_.timeLastSuccessfulSyncUnix()
      : (hasPrevious ? previous.lastNtpSyncAt : 0);
  state.lastSensorSampleWakeupCount = lastSensorSampleWakeupCount_;
  state.lastNetworkAttemptWakeupCount = lastNetworkAttemptWakeupCount_;
  state.lastSensorAvailable = sensorSnapshot_.available;

  std::snprintf(state.lastSensorTemp, sizeof(state.lastSensorTemp), "%s", sensorSnapshot_.temperatureText.c_str());
  std::snprintf(state.lastSensorHumidity, sizeof(state.lastSensorHumidity), "%s", sensorSnapshot_.humidityText.c_str());

  state.consecutiveNetworkFailures = hasPrevious ? previous.consecutiveNetworkFailures : 0;
  state.isConfigured = hasSavedConfig(config_);

  rtcMemoryWrite(state);
}

void BootController::restoreStateFromRtcMemory() {
  RtcMemoryState state{};
  if (!rtcMemoryRead(&state)) {
    return;  // RTC 内存无效，使用默认值
  }

  sensorSnapshot_.temperatureText = state.lastSensorTemp;
  sensorSnapshot_.humidityText = state.lastSensorHumidity;
  sensorSnapshot_.available = state.lastSensorAvailable;
  currentWakeupCount_ = state.wakeupCount;
  lastSensorSampleWakeupCount_ = state.lastSensorSampleWakeupCount;
  lastNetworkAttemptWakeupCount_ = state.lastNetworkAttemptWakeupCount;
}

void BootController::enterDeepSleep() {
  if (accessPointMode_) {
    return;
  }

  saveStateToRtcMemory();

  esp_sleep_enable_timer_wakeup(kDeepSleepIntervalUs);
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_start();
}
