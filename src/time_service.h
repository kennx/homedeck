#pragma once

#include <ctime>
#include <functional>
#include <string>

#include "config_types.h"

namespace homedeck {

enum class TimeCalibrationStatus {
  SuccessNtp,
  SuccessManual,
  SuccessManualFallback,
  FailedWifi,
  FailedNeedsManualTime,
  FailedRtcWrite,
  FailedTimezone,
};

struct TimeCalibrationResult {
  TimeCalibrationStatus status = TimeCalibrationStatus::SuccessManual;
  const char* message = "";
  bool ok() const {
    return status == TimeCalibrationStatus::SuccessNtp ||
           status == TimeCalibrationStatus::SuccessManual ||
           status == TimeCalibrationStatus::SuccessManualFallback;
  }
};

struct TimeServiceDeps {
  std::function<bool(const std::string& ssid, const std::string& password)> connectWifi;
  std::function<bool(const std::string& posixTimezone, const std::string& ntpServer, time_t* syncedUnix)> syncNtp;
  std::function<bool(time_t utcUnix)> writeRtcUtc;
  std::function<bool()> rtcAvailable;
  std::function<bool()> rtcVoltLow;
  std::function<void()> restoreSystemTimeFromRtc;
};

class TimeService {
 public:
  explicit TimeService(TimeServiceDeps deps);

  TimeCalibrationResult calibrateOnSave(
      const SetupConfig& config,
      const ManualDateTime& manualDateTime);
  void restoreSystemTimeFromRtc();

 private:
  bool manualDateTimeToUnix(
      const ManualDateTime& manualDateTime,
      const std::string& timezoneIana,
      time_t* unixTime) const;
  TimeCalibrationResult writeManual(
      const ManualDateTime& manualDateTime,
      const std::string& timezoneIana,
      TimeCalibrationStatus successStatus);

  TimeServiceDeps deps_;
};

}  // namespace homedeck
