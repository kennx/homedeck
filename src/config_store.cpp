#include "config_store.h"

namespace {

constexpr const char* kNamespace = "homedeck";

constexpr const char* kConfiguredKey = "configured";
constexpr const char* kWifiSsidKey = "wifi_ssid";
constexpr const char* kWifiPasswordKey = "wifi_password";
constexpr const char* kTimezoneKey = "timezone";
constexpr const char* kNtpServerKey = "ntp_server";
constexpr const char* kPersonalCalendarUrlKey = "pers_cal_url";
constexpr const char* kHolidayCalendarUrlKey = "holi_cal_url";
constexpr const char* kPersonalCachePayloadKey = "pers_cal_payld";
constexpr const char* kPersonalCacheUpdatedAtKey = "pers_cal_updt";
constexpr const char* kHolidayCachePayloadKey = "holi_cal_payld";
constexpr const char* kHolidayCacheUpdatedAtKey = "holi_cal_updt";

String toArduinoString(const std::string& value) {
  return String(value.c_str());
}

std::string toStdString(const String& value) {
  return std::string(value.c_str());
}

bool saveStringValue(Preferences& prefs, const char* key, const String& value) {
  if (value.isEmpty()) {
    prefs.remove(key);
    return true;
  }
  const size_t written = prefs.putString(key, value);
  return written == value.length();
}

}  // namespace

bool ConfigStore::begin() {
  if (started_) {
    return true;
  }

  started_ = prefs_.begin(kNamespace, false);
  return started_;
}

homedeck::SetupConfig ConfigStore::loadSetupConfig() const {
  if (!started_ || !prefs_.getBool(kConfiguredKey, false)) {
    return homedeck::SetupConfig{};
  }

  return homedeck::SetupConfig{
      toStdString(prefs_.getString(kWifiSsidKey, "")),
      toStdString(prefs_.getString(kWifiPasswordKey, "")),
      toStdString(prefs_.getString(kTimezoneKey, "")),
      toStdString(prefs_.getString(kNtpServerKey, "")),
      toStdString(prefs_.getString(kPersonalCalendarUrlKey, "")),
      toStdString(prefs_.getString(kHolidayCalendarUrlKey, "")),
  };
}

bool ConfigStore::saveSetupConfig(const homedeck::SetupConfig& config) {
  if (!started_) {
    return false;
  }

  return saveStringValue(prefs_, kWifiSsidKey, toArduinoString(config.wifiSsid)) &&
      saveStringValue(prefs_, kWifiPasswordKey, toArduinoString(config.wifiPassword)) &&
      saveStringValue(prefs_, kTimezoneKey, toArduinoString(config.timezoneIana)) &&
      saveStringValue(prefs_, kNtpServerKey, toArduinoString(config.ntpServer)) &&
      saveStringValue(
             prefs_, kPersonalCalendarUrlKey, toArduinoString(config.personalCalendarUrl)) &&
      saveStringValue(
             prefs_, kHolidayCalendarUrlKey, toArduinoString(config.holidayCalendarUrl)) &&
      prefs_.putBool(kConfiguredKey, true);
}

CalendarCacheRecord ConfigStore::loadPersonalCalendarCache() const {
  return loadCalendarCache(kPersonalCachePayloadKey, kPersonalCacheUpdatedAtKey);
}

CalendarCacheRecord ConfigStore::loadHolidayCalendarCache() const {
  return loadCalendarCache(kHolidayCachePayloadKey, kHolidayCacheUpdatedAtKey);
}

bool ConfigStore::savePersonalCalendarCache(const CalendarCacheRecord& record) {
  return saveCalendarCache(kPersonalCachePayloadKey, kPersonalCacheUpdatedAtKey, record);
}

bool ConfigStore::saveHolidayCalendarCache(const CalendarCacheRecord& record) {
  return saveCalendarCache(kHolidayCachePayloadKey, kHolidayCacheUpdatedAtKey, record);
}

CalendarCacheRecord ConfigStore::loadCalendarCache(
    const char* payloadKey,
    const char* updatedAtKey) const {
  if (!started_) {
    return CalendarCacheRecord{};
  }

  return CalendarCacheRecord{
      prefs_.getString(payloadKey, ""),
      prefs_.getString(updatedAtKey, ""),
  };
}

bool ConfigStore::saveCalendarCache(
    const char* payloadKey,
    const char* updatedAtKey,
    const CalendarCacheRecord& record) {
  if (!started_) {
    return false;
  }

  return saveStringValue(prefs_, payloadKey, record.payload) &&
      saveStringValue(prefs_, updatedAtKey, record.updatedAt);
}
