#pragma once

#include <ctime>
#include <functional>
#include <string>

#include "calendar_service.h"
#include "homedeck/boot_mode.h"
#include "homedeck/config_types.h"
#include "homedeck/home_screen.h"
#include "lunar_calendar_service.h"
#include "sensor_service.h"
#include "time_service.h"

struct BootCalendarCacheRecord {
  std::string payload;
  std::string updatedAt;
};

using BootControllerSaveCallback = std::function<int(const homedeck::SetupConfig& config)>;

struct RtcMemoryState {
  uint32_t magic;                       // kRtcMemoryMagic，用于校验数据有效性
  uint32_t wakeupCount;                 // 唤醒次数计数
  time_t lastNtpSyncAt;                 // 上次 NTP 同步时间戳
  uint32_t lastSensorSampleWakeupCount; // 上次传感器采样所在 boot 序号
  uint32_t lastNetworkAttemptWakeupCount;  // 上次网络周期尝试所在 boot 序号
  char lastSensorTemp[16];              // 上次温度读数
  char lastSensorHumidity[16];          // 上次湿度读数
  bool lastSensorAvailable;             // 上次传感器是否可用
  uint8_t consecutiveNetworkFailures;   // 连续网络失败次数
  bool isConfigured;                    // 是否已完成初始配置
};

struct BootControllerDeps {
  std::function<void()> m5Begin;
  std::function<void()> m5Update;
  std::function<bool()> areSetupButtonsPressed;
  std::function<unsigned long()> millis;
  std::function<std::string()> makeAccessPointSuffix;
  std::function<bool(const std::string& ssid, const std::string& password)> connectWifi;

  std::function<homedeck::SetupConfig()> loadSetupConfig;
  std::function<bool(const homedeck::SetupConfig& config)> saveSetupConfig;
  std::function<BootCalendarCacheRecord()> loadPersonalCalendarCache;
  std::function<BootCalendarCacheRecord()> loadHolidayCalendarCache;
  std::function<bool(const BootCalendarCacheRecord& record)> savePersonalCalendarCache;
  std::function<bool(const BootCalendarCacheRecord& record)> saveHolidayCalendarCache;

  std::function<void(
      const std::string& apSsid,
      const homedeck::SetupConfig& defaults,
      const BootControllerSaveCallback& onSave)> beginSetupPortal;
  std::function<void()> handleSetupPortalClient;
  std::function<void(const std::string& apSsid, const std::string& ipText)> renderSetupScreen;

  std::function<bool(const char* timezonePosix, const char* ntpServer)> timeBegin;
  std::function<TimeSnapshot()> timeSnapshot;
  std::function<void(time_t lastSuccessfulSyncUnix)> timeRestoreSyncState;
  std::function<time_t()> timeLastSuccessfulSyncUnix;
  std::function<bool()> syncTimeFromNtp;
  std::function<bool()> sensorBegin;
  std::function<SensorSnapshot()> sensorSample;

  std::function<CalendarFetchResult(const std::string& url)> fetchCalendarText;
  std::function<ParsedPersonalCalendar(const char* ics, int year, int month, int day)>
      parsePersonalCalendar;
  std::function<ParsedHolidayCalendar(const char* ics, int year, int month, int day)>
      parseHolidayCalendar;
  std::function<std::string(const ParsedPersonalCalendar& calendar)> encodePersonalCalendarCache;
  std::function<ParsedPersonalCalendar(const std::string& payload)> decodePersonalCalendarCache;
  std::function<std::string(const ParsedHolidayCalendar& calendar)> encodeHolidayCalendarCache;
  std::function<ParsedHolidayCalendar(const std::string& payload)> decodeHolidayCalendarCache;

  std::function<LunarDayInfo(int year, int month, int day)> describeLunarDate;
  std::function<void(const homedeck::HomeViewModel& model)> renderHomeScreen;
};

class BootController {
 public:
  BootController();
  explicit BootController(BootControllerDeps deps);

  void begin();
  void update();
  void enterDeepSleep();

 private:
  void enterAccessPointMode(const homedeck::SetupConfig& config);
  void enterHomeModeFast(const homedeck::SetupConfig& config);
  void enterHomeMode(const homedeck::SetupConfig& config);
  void refreshHomeScreen();
  bool runBackgroundTasks(unsigned long nowMs, TimeSnapshot* snapshot);
  bool updateSensorState(unsigned long nowMs);
  void syncCalendarStateForSnapshot(const TimeSnapshot& snapshot);
  bool fetchPersonalCalendar(const TimeSnapshot& snapshot, int year, int month, int day);
  bool fetchHolidayCalendar(const TimeSnapshot& snapshot, int year, int month, int day);
  std::string resolveTimezonePosix(const std::string& timezoneIana) const;
  bool isCacheCurrentForDate(
      const BootCalendarCacheRecord& record,
      const std::string& dateText) const;

  homedeck::HomeViewModel buildHomeViewModel(
      const homedeck::SetupConfig& config,
      const TimeSnapshot& timeSnapshot,
      bool wifiConnected);

  bool hasSavedConfig(const homedeck::SetupConfig& config) const;
  bool parseSnapshotDate(const TimeSnapshot& snapshot, int* year, int* month, int* day) const;
  bool shouldRefreshHome(const TimeSnapshot& snapshot) const;
  void saveStateToRtcMemory();
  void restoreStateFromRtcMemory();
  uint32_t hashHomeViewModel(const homedeck::HomeViewModel& model) const;
  bool shouldRunNetworkCycle(unsigned long nowMs) const;
  bool hasElapsedSleepSlots(uint32_t currentWakeupCount, uint32_t lastWakeupCount, unsigned long intervalMs) const;
  void recordNetworkFailure();
  void resetNetworkFailureCount();

  BootControllerDeps deps_;
  homedeck::SetupConfig config_{};
  bool started_ = false;
  bool accessPointMode_ = false;
  bool wifiConnected_ = false;
  unsigned long lastRefreshAtMs_ = 0;
  std::string lastRenderedDate_;
  unsigned long lastSensorSampleAtMs_ = 0;
  bool sensorSampled_ = false;
  SensorSnapshot sensorSnapshot_{};
  unsigned long lastNetworkCycleAtMs_ = 0;
  bool networkCycleStarted_ = false;
  ParsedPersonalCalendar personalCalendar_{};
  bool personalCalendarAvailable_ = false;
  bool personalCalendarFresh_ = false;
  ParsedHolidayCalendar holidayCalendar_{};
  bool holidayCalendarAvailable_ = false;
  bool holidayCalendarFresh_ = false;
  std::string activeCalendarDate_;
  unsigned long lastNetworkAttemptAtMs_ = 0;
  uint32_t currentWakeupCount_ = 0;
  uint32_t lastSensorSampleWakeupCount_ = 0;
  uint32_t lastNetworkAttemptWakeupCount_ = 0;
  uint32_t lastModelHash_ = 0;
  unsigned long apModeStartAtMs_ = 0;
};
