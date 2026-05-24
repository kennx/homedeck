#include "app_runtime.h"

#include <Arduino.h>
#include <ESP.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>

#include <cstdio>
#include <cstdint>
#include <memory>

#include "boot_controller.h"
#include "config_portal.h"
#include "config_store.h"
#include "home_renderer.h"
#include "time_service.h"
#include "timezone_catalog.h"

namespace homedeck {
namespace {

ConfigStore gConfigStore;
HomeRenderer gHomeRenderer;
ConfigPortal gConfigPortal;
std::unique_ptr<TimeService> gTimeService;
std::unique_ptr<BootController> gBootController;

std::string makeApSsid() {
  const std::uint64_t mac = ESP.getEfuseMac();
  char suffix[5] = {};
  std::snprintf(suffix, sizeof(suffix), "%04X", static_cast<unsigned int>(mac & 0xFFFF));
  return std::string("HomeDeck-") + suffix;
}

std::string softApIpAddress() {
  return WiFi.softAPIP().toString().c_str();
}

bool connectWifi(const std::string& ssid, const std::string& password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 10000) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool syncNtp(const std::string& posixTimezone, const std::string& ntpServer, time_t* syncedUnix) {
  if (syncedUnix == nullptr) {
    return false;
  }
  configTzTime(posixTimezone.c_str(), ntpServer.c_str());
  const unsigned long startedAt = millis();
  while (millis() - startedAt < 10000) {
    const time_t now = time(nullptr);
    if (now >= 1704067200) {
      *syncedUnix = now;
      return true;
    }
    delay(250);
  }
  return false;
}

bool writeRtcUtc(time_t unixTime) {
  if (!M5.Rtc.isEnabled()) {
    return false;
  }
  const std::tm* utc = gmtime(&unixTime);
  if (utc == nullptr) {
    return false;
  }
  M5.Rtc.setDateTime(utc);
  return true;
}

TimeServiceDeps makeTimeDeps() {
  TimeServiceDeps deps{};
  deps.connectWifi = connectWifi;
  deps.syncNtp = syncNtp;
  deps.writeRtcUtc = writeRtcUtc;
  deps.rtcAvailable = []() { return M5.Rtc.isEnabled(); };
  deps.rtcVoltLow = []() { return M5.Rtc.getVoltLow(); };
  deps.restoreSystemTimeFromRtc = []() { M5.Rtc.setSystemTimeFromRtc(); };
  return deps;
}

ConfigValidationResult saveSubmittedConfig(
    const SetupConfig& config,
    const ManualDateTime& manualDateTime) {
  const TimeCalibrationResult timeResult = gTimeService->calibrateOnSave(config, manualDateTime);
  if (!timeResult.ok()) {
    return ConfigValidationResult{ConfigValidationError::InvalidManualDateTime, timeResult.message};
  }
  if (!gConfigStore.saveSetupConfig(config) || !gConfigStore.saveConfigured(true)) {
    return ConfigValidationResult{ConfigValidationError::InvalidManualDateTime, "保存配置失败。"};
  }
  return ConfigValidationResult{};
}

BootControllerDeps makeBootDeps() {
  BootControllerDeps deps{};
  deps.loadFlags = []() { return gConfigStore.loadBootFlags(); };
  deps.clearForceConfigOnNextBoot = []() { return gConfigStore.clearForceConfigOnNextBoot(); };
  deps.setForceConfigOnNextBoot = []() { return gConfigStore.setForceConfigOnNextBoot(true); };
  deps.startConfigPortal = []() {
    const std::string apSsid = makeApSsid();
    gConfigPortal.begin(apSsid, gConfigStore.loadSetupConfig(), saveSubmittedConfig);
    gHomeRenderer.renderConfigPortal(apSsid, softApIpAddress());
  };
  deps.handleConfigPortalClient = []() { gConfigPortal.handleClient(); };
  deps.restoreSystemTimeFromRtc = []() { gTimeService->restoreSystemTimeFromRtc(); };
  deps.renderHome = []() { gHomeRenderer.render(); };
  deps.updateButtons = []() { M5.update(); };
  deps.areSetupButtonsPressed = []() { return M5.BtnA.isPressed() && M5.BtnB.isPressed(); };
  deps.millis = []() { return millis(); };
  deps.restart = []() { ESP.restart(); };
  return deps;
}

}  // namespace

void appSetup() {
  M5.begin();
  gConfigStore.begin();
  gTimeService = std::make_unique<TimeService>(makeTimeDeps());
  gBootController = std::make_unique<BootController>(makeBootDeps());
  gBootController->begin();
}

void appLoop() {
  if (gBootController) {
    gBootController->update();
  }
}

}  // namespace homedeck
