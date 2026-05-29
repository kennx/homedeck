#pragma once

#include <cstdint>
#include <string>

namespace homedeck {

struct SetupConfig {
  std::string wifiSsid;
  std::string wifiPassword;
  std::string timezoneIana = "Asia/Shanghai";
  bool autoRtcCorrection = false;
  std::string ntpServer = "pool.ntp.org";
};

struct ManualDateTime {
  bool present = false;
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
};

struct WifiNetwork {
  std::string ssid;
  int32_t rssi = 0;
};

enum class ConfigValidationError {
  None,
  MissingManualDateTime,
  MissingNtpServer,
  InvalidManualDateTime,
  InvalidTimezone,
};

struct ConfigValidationResult {
  ConfigValidationError error = ConfigValidationError::None;
  const char* message = "";
  bool ok() const { return error == ConfigValidationError::None; }
};

}  // namespace homedeck
