#pragma once

#include <cstdint>
#include <string>
#include <vector>

constexpr int WIFI_AP = 2;
constexpr int WIFI_STA = 1;
constexpr int WL_CONNECTED = 3;

struct IPAddress {
  int a = 192;
  int b = 168;
  int c = 4;
  int d = 1;
  std::string toString() const { return "192.168.4.1"; }
};

struct FakeWifiNetwork {
  std::string ssid;
  int32_t rssi = 0;
};

struct FakeWiFiClass {
  int modeValue = 0;
  bool softApStarted = false;
  std::string apSsid;
  std::string staSsid;
  std::string staPassword;
  int statusValue = WL_CONNECTED;
  std::vector<FakeWifiNetwork> scanResults;

  void mode(int value) { modeValue = value; }
  bool softAP(const char* ssid, const char* = nullptr) {
    softApStarted = true;
    apSsid = ssid != nullptr ? ssid : "";
    return true;
  }
  IPAddress softAPIP() const { return IPAddress{}; }
  int scanNetworks() { return static_cast<int>(scanResults.size()); }
  std::string SSID(int index) const { return scanResults[index].ssid; }
  int32_t RSSI(int index) const { return scanResults[index].rssi; }
  void scanDelete() { scanResults.clear(); }
  void begin(const char* ssid, const char* password) {
    staSsid = ssid != nullptr ? ssid : "";
    staPassword = password != nullptr ? password : "";
  }
  int status() const { return statusValue; }
};

inline FakeWiFiClass WiFi;
