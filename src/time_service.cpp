#include "time_service.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_sntp.h>
#include <time.h>

#include <cstdio>
#include <cstdlib>

namespace {

constexpr time_t kValidTimestampThreshold = 1704067200;  // 2024-01-01 00:00:00 UTC
bool gTimeSyncNotified = false;

void handleTimeSync(struct timeval*) {
  gTimeSyncNotified = true;
}

void applyTimezone(const std::string& timezonePosix) {
  if (timezonePosix.empty()) {
    return;
  }

  setenv("TZ", timezonePosix.c_str(), 1);
  tzset();
}

bool isSystemTimeValid(time_t now) {
  return now >= kValidTimestampThreshold;
}

bool hasRestorableSyncState(time_t now, time_t lastSuccessfulSyncUnix) {
  return isSystemTimeValid(now) && lastSuccessfulSyncUnix > 0 &&
         lastSuccessfulSyncUnix <= now;
}

bool convertUtcRtcToLocalTm(const m5::rtc_datetime_t& rtcDateTime, tm* localTime) {
  if (localTime == nullptr) {
    return false;
  }

  tm utcTime = {};
  utcTime.tm_year = rtcDateTime.date.year - 1900;
  utcTime.tm_mon = rtcDateTime.date.month - 1;
  utcTime.tm_mday = rtcDateTime.date.date;
  utcTime.tm_hour = rtcDateTime.time.hours;
  utcTime.tm_min = rtcDateTime.time.minutes;
  utcTime.tm_sec = rtcDateTime.time.seconds;
  utcTime.tm_isdst = -1;

  char* oldTimezone = getenv("TZ");
  std::string savedTimezone = oldTimezone != nullptr ? oldTimezone : "";
  setenv("TZ", "UTC0", 1);
  tzset();
  const time_t utcStamp = mktime(&utcTime);
  if (oldTimezone != nullptr) {
    setenv("TZ", savedTimezone.c_str(), 1);
  } else {
    unsetenv("TZ");
  }
  tzset();

  if (utcStamp < 0) {
    return false;
  }

  return localtime_r(&utcStamp, localTime) != nullptr;
}

TimeSnapshot makeSnapshotFromTm(const tm& localTime, bool timeSynced) {
  char timeBuffer[6] = {};
  char dateBuffer[32] = {};
  std::snprintf(
      timeBuffer,
      sizeof(timeBuffer),
      "%02d:%02d",
      localTime.tm_hour,
      localTime.tm_min);
  std::snprintf(
      dateBuffer,
      sizeof(dateBuffer),
      "%d年%d月%d日",
      localTime.tm_year + 1900,
      localTime.tm_mon + 1,
      localTime.tm_mday);

  return TimeSnapshot{
      timeBuffer,
      dateBuffer,
      true,
      timeSynced,
  };
}

}  // namespace

bool TimeService::begin(const char* timezonePosix, const char* ntpServer) {
  timezonePosix_ = timezonePosix != nullptr ? timezonePosix : "";
  ntpServer_ = ntpServer != nullptr ? ntpServer : "";
  timeSynced_ = false;
  lastSyncedAt_ = 0;

  applyTimezone(timezonePosix_);

  rtcAvailable_ = M5.Rtc.isEnabled();
  if (rtcAvailable_ && !M5.Rtc.getVoltLow()) {
    M5.Rtc.setSystemTimeFromRtc();
  }

  return rtcAvailable_ || (!timezonePosix_.empty() && !ntpServer_.empty());
}

void TimeService::restoreSyncState(time_t lastSuccessfulSyncUnix) {
  lastSyncedAt_ = lastSuccessfulSyncUnix;
  timeSynced_ = hasRestorableSyncState(time(nullptr), lastSuccessfulSyncUnix);
}

time_t TimeService::lastSuccessfulSyncUnix() const {
  return lastSyncedAt_;
}

TimeSnapshot TimeService::snapshot() const {
  const time_t now = time(nullptr);
  if (isSystemTimeValid(now)) {
    tm localTime = {};
    if (localtime_r(&now, &localTime) != nullptr) {
      return makeSnapshotFromTm(localTime, timeSynced_);
    }
  }

  if (!rtcAvailable_) {
    return TimeSnapshot{};
  }

  if (M5.Rtc.getVoltLow()) {
    return TimeSnapshot{};
  }

  m5::rtc_datetime_t rtcDateTime;
  if (!M5.Rtc.getDateTime(&rtcDateTime)) {
    return TimeSnapshot{};
  }

  tm localTime = {};
  if (!convertUtcRtcToLocalTm(rtcDateTime, &localTime)) {
    return TimeSnapshot{};
  }

  const time_t rtcLocalStamp = mktime(&localTime);
  if (!isSystemTimeValid(rtcLocalStamp)) {
    return TimeSnapshot{};
  }

  return makeSnapshotFromTm(localTime, timeSynced_);
}

bool TimeService::syncFromNtp() {
  if (timezonePosix_.empty() || ntpServer_.empty()) {
    return false;
  }

  const time_t now = time(nullptr);
  if (timeSynced_ && rtcAvailable_ && !M5.Rtc.getVoltLow() &&
      hasRestorableSyncState(now, lastSyncedAt_) &&
      (now - lastSyncedAt_) < (24 * 3600)) {
    return false;
  }

  gTimeSyncNotified = false;
  sntp_set_time_sync_notification_cb(handleTimeSync);
  applyTimezone(timezonePosix_);
  configTzTime(timezonePosix_.c_str(), ntpServer_.c_str());

  for (int attempt = 0; attempt < 20; ++attempt) {
    const time_t now = time(nullptr);
    if (!gTimeSyncNotified || !isSystemTimeValid(now)) {
      delay(250);
      continue;
    }

    if (rtcAvailable_) {
      const tm* utcTime = gmtime(&now);
      if (utcTime != nullptr) {
        M5.Rtc.setDateTime(utcTime);
      }
    }

    timeSynced_ = true;
    lastSyncedAt_ = time(nullptr);
    sntp_set_time_sync_notification_cb(nullptr);
    return true;
  }

  sntp_set_time_sync_notification_cb(nullptr);
  return false;
}
