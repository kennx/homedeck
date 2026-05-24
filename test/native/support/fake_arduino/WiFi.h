#pragma once

#include <cstdint>
#include <string>
#include <vector>

constexpr int WIFI_AP = 2;
constexpr int WIFI_STA = 1;
constexpr int WIFI_AP_STA = WIFI_AP | WIFI_STA;
constexpr int WL_CONNECTED = 3;

struct IPAddress {
  int a = 192;
  int b = 168;
  int c = 4;
  int d = 1;
  IPAddress() = default;
  IPAddress(int first, int second, int third, int fourth)
      : a(first), b(second), c(third), d(fourth) {}
  std::string toString() const {
    return std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c) + "." +
        std::to_string(d);
  }
};

struct FakeWifiNetwork {
  std::string ssid;
  int32_t rssi = 0;
};

struct FakeWiFiClass {
  int modeValue = 0;
  bool softApStarted = false;
  bool softApConfigCalled = false;
  std::string apSsid;
  std::string staSsid;
  std::string staPassword;
  IPAddress apLocalIp{};
  IPAddress apGateway{};
  IPAddress apSubnet{255, 255, 255, 0};
  int statusValue = WL_CONNECTED;
  std::vector<FakeWifiNetwork> scanResults;

  void mode(int value) { modeValue = value; }
  int getMode() const { return modeValue; }
  bool softAP(const char* ssid, const char* = nullptr) {
    softApStarted = true;
    apSsid = ssid != nullptr ? ssid : "";
    return true;
  }
  bool softAPConfig(IPAddress localIp, IPAddress gateway, IPAddress subnet, IPAddress = IPAddress{}) {
    softApConfigCalled = true;
    apLocalIp = localIp;
    apGateway = gateway;
    apSubnet = subnet;
    return true;
  }
  IPAddress softAPIP() const { return apLocalIp; }
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
