#include "config_portal.h"

#include <Arduino.h>
#include <ESP.h>
#include <WiFi.h>

#include "config_validator.h"
#include "setup_page.h"

namespace homedeck {

namespace {

constexpr std::uint16_t kDnsPort = 53;
const IPAddress kSoftApIp{192, 168, 4, 1};
const IPAddress kSoftApSubnet{255, 255, 255, 0};

}  // namespace

void ConfigPortal::begin(
    const std::string& apSsid,
    const SetupConfig& defaults,
    ConfigPortalSaveCallback onSave) {
  apSsid_ = apSsid;
  defaults_ = defaults;
  onSave_ = std::move(onSave);
  restartScheduled_ = false;
  restartAtMs_ = 0;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(kSoftApIp, kSoftApIp, kSoftApSubnet);
  WiFi.softAP(apSsid_.c_str());
  delay(100);

  networks_ = selectTopWifiNetworks(scanWifiNetworks(), 5);

  dnsServer_.start(kDnsPort, "*", WiFi.softAPIP());

  server_.on("/", HTTP_GET, [this]() { handleIndex(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  registerCaptivePortalRoutes();
  server_.begin();
}

void ConfigPortal::handleClient() {
  dnsServer_.processNextRequest();
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

void ConfigPortal::redirectToIndex() {
  const std::string location = std::string("http://") + WiFi.softAPIP().toString().c_str() + "/";
  server_.sendHeader("Location", location.c_str(), true);
  server_.send(302, "text/plain", "");
}

void ConfigPortal::registerCaptivePortalRoutes() {
  server_.on("/generate_204", HTTP_GET, [this]() { redirectToIndex(); });
  server_.on("/gen_204", HTTP_GET, [this]() { redirectToIndex(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { redirectToIndex(); });
  server_.on("/library/test/success.html", HTTP_GET, [this]() { redirectToIndex(); });
  server_.on("/connecttest.txt", HTTP_GET, [this]() { redirectToIndex(); });
  server_.on("/ncsi.txt", HTTP_GET, [this]() { redirectToIndex(); });
  server_.onNotFound([this]() { redirectToIndex(); });
}

void ConfigPortal::sendPage(int status, const SetupConfig& values, const std::string& message) {
  const std::string html = buildSetupPageHtml(apSsid_, values, networks_, message);
  server_.send(status, "text/html; charset=utf-8", html.c_str());
}

}  // namespace homedeck
