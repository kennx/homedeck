#include "system/wifi_connection.h"

#include <Arduino.h>
#include <WiFi.h>

namespace homedeck {

bool connectWifiPreservingAccessPoint(
    const std::string& ssid,
    const std::string& password,
    unsigned long timeoutMs) {
  const int currentMode = WiFi.getMode();
  const bool apEnabled = (currentMode & WIFI_AP) != 0;
  WiFi.mode(apEnabled ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < timeoutMs) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

}  // namespace homedeck
