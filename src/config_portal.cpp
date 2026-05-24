#include "config_portal.h"

#include <Arduino.h>
#include <ESP.h>
#include <WiFi.h>

#include "config_validator.h"
#include "setup_page.h"

namespace homedeck {

void ConfigPortal::begin(
    const std::string& apSsid,
    const SetupConfig& defaults,
    ConfigPortalSaveCallback onSave) {
  apSsid_ = apSsid;
  defaults_ = defaults;
  onSave_ = std::move(onSave);
  restartScheduled_ = false;
  restartAtMs_ = 0;

  networks_ = selectTopWifiNetworks(scanWifiNetworks(), 5);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid_.c_str());
  delay(100);

  server_.on("/", HTTP_GET, [this]() { handleIndex(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.begin();
}

void ConfigPortal::handleClient() {
  server_.handleClient();
  if (restartScheduled_ && static_cast<long>(millis() - restartAtMs_) >= 0) {
    ESP.restart();
  }
}

std::vector<WifiNetwork> ConfigPortal::scanWifiNetworks() {
  std::vector<WifiNetwork> networks;
  const int count = WiFi.scanNetworks();
  for (int i = 0; i < count; ++i) {
    networks.push_back(WifiNetwork{WiFi.SSID(i).c_str(), WiFi.RSSI(i)});
  }
  WiFi.scanDelete();
  return networks;
}

SetupConfig ConfigPortal::readConfigFromRequest() {
  SetupConfig config{};
  config.wifiSsid = server_.arg("wifi_ssid").c_str();
  config.wifiPassword = server_.arg("wifi_password").c_str();
  config.timezoneIana = server_.arg("timezone").c_str();
  config.autoRtcCorrection = server_.arg("auto_rtc") == "1" && !config.wifiSsid.empty();
  config.ntpServer = server_.arg("ntp_server").c_str();
  return config;
}

ManualDateTime ConfigPortal::readManualTimeFromRequest() {
  ManualDateTime manual{};
  parseManualDateTime(server_.arg("manual_datetime").c_str(), &manual);
  return manual;
}

void ConfigPortal::handleIndex() {
  sendPage(200, defaults_, "");
}

void ConfigPortal::handleSave() {
  const SetupConfig submitted = readConfigFromRequest();
  ManualDateTime manual{};
  if (!parseManualDateTime(server_.arg("manual_datetime").c_str(), &manual)) {
    sendPage(400, submitted, "手动时间格式无效。");
    return;
  }

  const ConfigValidationResult validation = validateSetupSubmission(submitted, manual);
  if (!validation.ok()) {
    sendPage(400, submitted, validation.message);
    return;
  }

  ConfigValidationResult saveResult{};
  if (onSave_) {
    saveResult = onSave_(submitted, manual);
  }
  if (!saveResult.ok()) {
    sendPage(500, submitted, saveResult.message);
    return;
  }

  defaults_ = submitted;
  restartScheduled_ = true;
  restartAtMs_ = millis() + 1000;
  sendPage(200, submitted, "设置已保存，设备将在 1 秒后重启。");
}

void ConfigPortal::sendPage(int status, const SetupConfig& values, const std::string& message) {
  const std::string html = buildSetupPageHtml(apSsid_, values, networks_, message);
  server_.send(status, "text/html; charset=utf-8", html.c_str());
}

}  // namespace homedeck
