#include "boot_controller.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>

#include "homedeck/home_screen.h"
#include "homedeck/timezone_catalog.h"

#if !defined(UNIT_TEST)
#include <Arduino.h>
#include <ESP.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "ap_config_portal.h"
#include "config_store.h"
#include "home_renderer.h"
#include "setup_renderer.h"
#endif

namespace {

constexpr unsigned long kRefreshIntervalMs = 60UL * 60UL * 1000UL;
constexpr unsigned long kSensorSampleIntervalMs = 60UL * 1000UL;
constexpr unsigned long kNetworkRetryIntervalMs = 60UL * 1000UL;

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

    WiFi.disconnect(false, true);
    delay(100);

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

  if (accessPointMode_) {
    if (deps_.handleSetupPortalClient) {
      deps_.handleSetupPortalClient();
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
  const std::string apSsid = buildAccessPointSsid(
      deps_.makeAccessPointSuffix ? deps_.makeAccessPointSuffix()
                                  : std::string("000000"));
  const BootControllerSaveCallback onSave = [this](const homedeck::SetupConfig& saved) {
    return deps_.saveSetupConfig ? deps_.saveSetupConfig(saved) : false;
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
  lastNetworkAttemptAtMs_ = 0;

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
  }

  const TimeSnapshot snapshot = deps_.timeSnapshot ? deps_.timeSnapshot() : TimeSnapshot{};
  if (snapshot.timeValid) {
    syncCalendarStateForSnapshot(snapshot);
  }

  refreshHomeScreen();
}

void BootController::refreshHomeScreen() {
  const TimeSnapshot snapshot = deps_.timeSnapshot ? deps_.timeSnapshot() : TimeSnapshot{};
  const homedeck::HomeViewModel model =
      buildHomeViewModel(config_, snapshot, wifiConnected_);

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
  const bool calendarReady = personalCalendarFresh_ && holidayCalendarFresh_;
  const unsigned long retryInterval = calendarReady ? kRefreshIntervalMs : kNetworkRetryIntervalMs;
  const bool shouldRunNetworkCycle = !networkCycleStarted_ || dateChanged ||
      (nowMs - lastNetworkAttemptAtMs_ >= retryInterval);
  if (!shouldRunNetworkCycle) {
    return shouldRefresh;
  }

  networkCycleStarted_ = true;
  lastNetworkAttemptAtMs_ = nowMs;
  wifiConnected_ = deps_.connectWifi
      ? deps_.connectWifi(config_.wifiSsid, config_.wifiPassword)
      : false;

  bool timeSyncedNow = false;
  if (wifiConnected_ && deps_.syncTimeFromNtp) {
    timeSyncedNow = deps_.syncTimeFromNtp();
  }
  if (snapshot != nullptr && deps_.timeSnapshot) {
    *snapshot = deps_.timeSnapshot();
  }

  if (snapshot == nullptr || !snapshot->timeValid) {
    return shouldRefresh || timeSyncedNow;
  }

  const bool personalWasFresh = personalCalendarFresh_;
  const bool holidayWasFresh = holidayCalendarFresh_;
  syncCalendarStateForSnapshot(*snapshot);
  if (personalCalendarFresh_ && holidayCalendarFresh_) {
    lastNetworkCycleAtMs_ = nowMs;
  }

  if (timeSyncedNow || dateChanged ||
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
    return false;
  }

  if (nowMs - lastSensorSampleAtMs_ < kSensorSampleIntervalMs) {
    return false;
  }

  const bool wasAvailable = sensorSnapshot_.available;
  sensorSnapshot_ = deps_.sensorSample();
  lastSensorSampleAtMs_ = nowMs;
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
