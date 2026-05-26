#include "time_service.h"

#include <cstdlib>
#include <cstring>
#include <utility>

#include "timezone_catalog.h"

namespace homedeck {
namespace {

bool isBlank(const std::string& value) {
  for (const unsigned char ch : value) {
    if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
      return false;
    }
  }
  return true;
}

TimeCalibrationResult result(TimeCalibrationStatus status, const char* message) {
  return TimeCalibrationResult{status, message};
}

}  // namespace

TimeService::TimeService(TimeServiceDeps deps) : deps_(std::move(deps)) {
}

TimeCalibrationResult TimeService::calibrateOnSave(
    const SetupConfig& config,
    const ManualDateTime& manualDateTime) {
  const TimezoneInfo* timezone = findTimezoneByIana(config.timezoneIana);
  if (timezone == nullptr) {
    return result(TimeCalibrationStatus::FailedTimezone, "时区不受支持。");
  }

  const bool hasWifi = !isBlank(config.wifiSsid);
  if (hasWifi && config.autoRtcCorrection) {
    if (deps_.connectWifi && !deps_.connectWifi(config.wifiSsid, config.wifiPassword)) {
      if (manualDateTime.present) {
        return writeManual(manualDateTime, config.timezoneIana, TimeCalibrationStatus::SuccessManualFallback);
      }
      return result(TimeCalibrationStatus::FailedWifi, "无法连接 Wi-Fi，请填写手动时间。");
    }

    time_t syncedUnix = 0;
    if (deps_.syncNtp && deps_.syncNtp(timezone->posix, config.ntpServer, &syncedUnix)) {
      if (deps_.writeRtcUtc && deps_.writeRtcUtc(syncedUnix)) {
        if (!applyTimezone(config.timezoneIana)) {
          return result(TimeCalibrationStatus::FailedTimezone, "时区不受支持。");
        }
        return result(TimeCalibrationStatus::SuccessNtp, "");
      }
      return result(TimeCalibrationStatus::FailedRtcWrite, "RTC 写入失败。");
    }

    if (manualDateTime.present) {
      return writeManual(manualDateTime, config.timezoneIana, TimeCalibrationStatus::SuccessManualFallback);
    }
    return result(TimeCalibrationStatus::FailedNeedsManualTime, "无法自动校准，请填写手动时间。");
  }

  if (manualDateTime.present) {
    return writeManual(manualDateTime, config.timezoneIana, TimeCalibrationStatus::SuccessManual);
  }

  return result(TimeCalibrationStatus::FailedNeedsManualTime, "请填写手动时间。");
}

bool TimeService::applyTimezone(const std::string& timezoneIana) const {
  const TimezoneInfo* timezone = findTimezoneByIana(timezoneIana);
  if (timezone == nullptr) {
    return false;
  }
  setenv("TZ", timezone->posix, 1);
  tzset();
  return true;
}

void TimeService::restoreSystemTimeFromRtc() {
  if (deps_.rtcAvailable && !deps_.rtcAvailable()) {
    return;
  }
  if (deps_.rtcVoltLow && deps_.rtcVoltLow()) {
    return;
  }
  if (deps_.restoreSystemTimeFromRtc) {
    deps_.restoreSystemTimeFromRtc();
  }
}

bool TimeService::manualDateTimeToUnix(
    const ManualDateTime& manualDateTime,
    const std::string& timezoneIana,
    time_t* unixTime) const {
  if (unixTime == nullptr || !manualDateTime.present) {
    return false;
  }

  const TimezoneInfo* timezone = findTimezoneByIana(timezoneIana);
  if (timezone == nullptr) {
    return false;
  }

  char* oldTimezone = std::getenv("TZ");
  const std::string savedTimezone = oldTimezone != nullptr ? oldTimezone : "";
  setenv("TZ", timezone->posix, 1);
  tzset();

  std::tm local{};
  local.tm_year = manualDateTime.year - 1900;
  local.tm_mon = manualDateTime.month - 1;
  local.tm_mday = manualDateTime.day;
  local.tm_hour = manualDateTime.hour;
  local.tm_min = manualDateTime.minute;
  local.tm_sec = manualDateTime.second;
  local.tm_isdst = -1;
  const time_t converted = std::mktime(&local);

  if (oldTimezone != nullptr) {
    setenv("TZ", savedTimezone.c_str(), 1);
  } else {
    unsetenv("TZ");
  }
  tzset();

  if (converted < 0) {
    return false;
  }

  *unixTime = converted;
  return true;
}

TimeCalibrationResult TimeService::writeManual(
    const ManualDateTime& manualDateTime,
    const std::string& timezoneIana,
    TimeCalibrationStatus successStatus) {
  time_t unixTime = 0;
  if (!manualDateTimeToUnix(manualDateTime, timezoneIana, &unixTime)) {
    return result(TimeCalibrationStatus::FailedTimezone, "手动时间无法按时区转换。");
  }
  if (deps_.writeRtcUtc && deps_.writeRtcUtc(unixTime)) {
    if (!applyTimezone(timezoneIana)) {
      return result(TimeCalibrationStatus::FailedTimezone, "时区不受支持。");
    }
    return result(successStatus, "");
  }
  return result(TimeCalibrationStatus::FailedRtcWrite, "RTC 写入失败。");
}

}  // namespace homedeck
